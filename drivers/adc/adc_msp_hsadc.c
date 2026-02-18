/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_msp_hsadc

#include <errno.h>
#include <string.h>

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc_msp_hsadc);

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/adc.h>
#include <soc.h>

#ifdef CONFIG_ADC_MSP_HSADC_DMA
#include <zephyr/drivers/dma.h>
#endif

/* Driverlib includes */
#include <ti/driverlib/dl_hsadc.h>
#include <ti/driverlib/dl_vref.h>

/* VREF module instance */
#define VREF ((VREF_Regs *)VREF_BASE)

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

/* HSADC hardware limits - configured via Kconfig for SoC-specific variants */
#define ADC_MSP_HSADC_SOC_MAX          CONFIG_ADC_MSP_HSADC_SOC_MAX
#define ADC_MSP_HSADC_CHANNEL_MAX      CONFIG_ADC_MSP_HSADC_CHANNEL_MAX
#define ADC_MSP_HSADC_ACQ_TIME_DEFAULT 64 /* Default acquisition time in SYSCLK cycles */
#define ADC_MSP_HSADC_ACQ_TIME_MIN     minSampleWindow /* From driverlib (1 SYSCLK) */
#define ADC_MSP_HSADC_ACQ_TIME_MAX     maxSampleWindow /* From driverlib (1472 SYSCLK) */

/* Helper macros to reduce repeated type casts */
#define ADC_REGS(cfg)    ((hsadc_ADC_LITE_REGS_Regs *)(cfg)->config_base)
#define RESULT_REGS(cfg) ((hsadc_ADC_LITE_RESULT_REGS_Regs *)(cfg)->result_base)

/* Internal reference voltage values (mV) - used to auto-detect VREF source */
#define ADC_MSP_HSADC_VREF_INTERNAL_2_5V_MV 2500
#define ADC_MSP_HSADC_VREF_INTERNAL_1_4V_MV 1400

/* Helper to check if vref_mv indicates internal reference */
static inline bool adc_msp_hsadc_uses_internal_vref(uint16_t vref_mv)
{
	return (vref_mv == ADC_MSP_HSADC_VREF_INTERNAL_2_5V_MV ||
		vref_mv == ADC_MSP_HSADC_VREF_INTERNAL_1_4V_MV);
}

struct adc_msp_hsadc_data {
	struct adc_context ctx;
	const struct device *dev;
	uint16_t *repeat_buffer;

	/* Per-channel acquisition time in SYSCLK cycles (indexed by channel_id) */
	uint16_t ch_acq_time[ADC_MSP_HSADC_CHANNEL_MAX];
	uint32_t ch_configured; /* Bitmask of configured channels */

#ifdef CONFIG_ADC_MSP_HSADC_DMA
	/* Temp buffer for 32-bit packed FIFO results (2 x 16-bit per word) */
	uint32_t dma_fifo_result[(ADC_MSP_HSADC_SOC_MAX + 1) / 2];
#endif
};

struct adc_msp_hsadc_cfg {
	uint32_t config_base;
	uint32_t result_base;
	uint32_t clock_divider;
	void (*irq_cfg_func)(void);
	uint16_t vref_mv;
#ifdef CONFIG_ADC_MSP_HSADC_DMA
	const struct device *dma_dev;
	uint8_t dma_channel;
	uint8_t dma_trigsrc;
#endif
};

static void adc_msp_hsadc_isr(const struct device *dev);

#ifdef CONFIG_ADC_MSP_HSADC_DMA
static void adc_msp_hsadc_dma_callback(const struct device *dma_dev, void *user_data,
				       uint32_t channel, int status)
{
	const struct device *dev = user_data;
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	uint16_t *buffer = (uint16_t *)data->ctx.sequence.buffer;
	int ch_count = POPCOUNT(data->ctx.sequence.channels);
	int fifo_reads = (ch_count + 1) / 2;

	DL_HSADC_DMAInterruptStatusClear(ADC_REGS(config), DL_HSADC_DMA_INT_1);
	DL_HSADC_disableDMAInterrupt(ADC_REGS(config), DL_HSADC_DMA_INT_1);

	if (status != DMA_STATUS_COMPLETE) {
		LOG_ERR("DMA transfer error: %d", status);
		adc_context_complete(&data->ctx, -EIO);
		return;
	}

	/* Unpack 32-bit FIFO words into uint16_t user buffer.
	 * FIFO packing: each 32-bit read returns 2 consecutive SOC results.
	 * LSB[15:0] = first result, MSB[31:16] = second result.
	 */
	for (int i = 0; i < fifo_reads; i++) {
		uint32_t packed = data->dma_fifo_result[i];

		buffer[2 * i] = (uint16_t)(packed & 0xFFFF);
		if ((2 * i + 1) < ch_count) {
			buffer[2 * i + 1] = (uint16_t)((packed >> 16) & 0xFFFF);
		}
	}

	adc_context_on_sampling_done(&data->ctx, dev);
}

static int adc_msp_hsadc_dma_start(const struct device *dev, int ch_count)
{
	const struct adc_msp_hsadc_cfg *config = dev->config;
	struct adc_msp_hsadc_data *data = dev->data;
	int fifo_reads = (ch_count + 1) / 2;
	int ret;

	struct dma_block_config blk_cfg = {
		.source_address = (uint32_t)&RESULT_REGS(config)->
			ADC_LITE_RESULT_REGS.ADCSEQ1FIFORESULT,
		.dest_address = (uint32_t)data->dma_fifo_result,
		.source_addr_adj = DMA_ADDR_ADJ_NO_CHANGE,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.block_size = fifo_reads,
	};
	struct dma_config dma_cfg = {
		.dma_slot = config->dma_trigsrc,
		.channel_direction = PERIPHERAL_TO_MEMORY,
		.block_count = 1,
		.head_block = &blk_cfg,
		.source_data_size = 4,
		.dest_data_size = 4,
		.dma_callback = adc_msp_hsadc_dma_callback,
		.user_data = (void *)dev,
	};

	ret = dma_config(config->dma_dev, config->dma_channel, &dma_cfg);
	if (ret < 0) {
		LOG_ERR("DMA config failed: %d", ret);
		return ret;
	}

	ret = dma_start(config->dma_dev, config->dma_channel);
	if (ret < 0) {
		LOG_ERR("DMA start failed: %d", ret);
		return ret;
	}

	return 0;
}
#endif /* CONFIG_ADC_MSP_HSADC_DMA */

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct adc_msp_hsadc_data *data = CONTAINER_OF(ctx, struct adc_msp_hsadc_data, ctx);
	const struct device *dev = data->dev;
	const struct adc_msp_hsadc_cfg *config = dev->config;

	data->repeat_buffer = (uint16_t *)ctx->sequence.buffer;

#ifdef CONFIG_ADC_MSP_HSADC_DMA
	if (config->dma_dev != NULL) {
		int ch_count = POPCOUNT(ctx->sequence.channels);

		/* Re-enable HSADC DMA trigger for this sampling round.
		 * The DMA callback disables DMA_INT_1 after each transfer.
		 * For repeated samplings (extra_samplings > 0), we must
		 * re-enable it before each round.
		 */
		DL_HSADC_DMAInterruptStatusClear(ADC_REGS(config), DL_HSADC_DMA_INT_1);
		DL_HSADC_enableDMAInterrupt(ADC_REGS(config), DL_HSADC_DMA_INT_1);

		/* Start DMA transfer before triggering ADC */
		int ret = adc_msp_hsadc_dma_start(dev, ch_count);

		if (ret < 0) {
			adc_context_complete(ctx, ret);
			return;
		}

		/* Software trigger: DMA handles result collection */
		DL_HSADC_triggerSequencerSoftwareForce(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1);
		return;
	}
#endif

	/* Non-DMA path: interrupt-driven result collection */
	DL_HSADC_InterruptStatusClear(ADC_REGS(config), DL_HSADC_INT_1);
	DL_HSADC_enableInterrupt(ADC_REGS(config), DL_HSADC_INT_1);

	/* Software trigger: start conversion now */
	DL_HSADC_triggerSequencerSoftwareForce(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1);
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx, bool repeat)
{
	struct adc_msp_hsadc_data *data = CONTAINER_OF(ctx, struct adc_msp_hsadc_data, ctx);

	if (repeat) {
		/* Reset to beginning for ADC_ACTION_REPEAT */
		ctx->sequence.buffer = data->repeat_buffer;
	} else {
		/* Advance buffer by number of active channels for next sampling */
		uint16_t *buffer = (uint16_t *)ctx->sequence.buffer;

		buffer += POPCOUNT(ctx->sequence.channels);
		ctx->sequence.buffer = buffer;
	}
}

static int adc_msp_hsadc_config_vref(uint16_t vref_mv)
{
	bool is_2p5v = (vref_mv == ADC_MSP_HSADC_VREF_INTERNAL_2_5V_MV);

	/* VREF is global (shared across all ADC instances).
	 * Only one internal VREF voltage active at a time.
	 */
	if (DL_VREF_isEnabled(VREF)) {
		uint32_t current_config = VREF->CTL0 & VREF_CTL0_BUFCONFIG_MASK;
		uint32_t expected_config =
			is_2p5v ? VREF_CTL0_BUFCONFIG_OUTPUT2P5V : VREF_CTL0_BUFCONFIG_OUTPUT1P4V;
		if (current_config == expected_config) {
			return 0; /* Already configured correctly */
		}
		LOG_ERR("VREF conflict: already configured to different voltage");
		return -EINVAL;
	}

	/* Initialize VREF */
	DL_VREF_reset(VREF);
	DL_VREF_enablePower(VREF);
	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);

	DL_VREF_ClockConfig vref_clk_config = {.clockSel = DL_VREF_CLOCK_BUSCLK,
					       .divideRatio = DL_VREF_CLOCK_DIVIDE_1};
	DL_VREF_setClockConfig(VREF, &vref_clk_config);

	DL_VREF_Config vref_config = {.vrefEnable = DL_VREF_ENABLE_ENABLE,
				      .bufConfig = is_2p5v ? DL_VREF_BUFCONFIG_OUTPUT_2_5V
							   : DL_VREF_BUFCONFIG_OUTPUT_1_4V,
				      .shModeEnable = DL_VREF_SHMODE_DISABLE,
				      .holdCycleCount = DL_VREF_HOLD_MIN,
				      .shCycleCount = DL_VREF_SH_MIN};
	DL_VREF_configReference(VREF, &vref_config);

	/* VREF settling is verified in init() after ADC power-up */
	return 0;
}

static int adc_msp_hsadc_init(const struct device *dev)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;

	data->dev = dev;

	DL_HSADC_reset(ADC_REGS(config));
	DL_HSADC_enablePower(ADC_REGS(config));
	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);

	DL_HSADC_setClockDivideRatio(ADC_REGS(config), config->clock_divider);

	/* Configure internal VREF if vref_mv matches a known internal reference */
	if (adc_msp_hsadc_uses_internal_vref(config->vref_mv)) {
		LOG_INF("Configuring internal VREF to %d mV", config->vref_mv);
		int ret = adc_msp_hsadc_config_vref(config->vref_mv);
		if (ret < 0) {
			LOG_ERR("Failed to configure VREF: %d", ret);
			return ret;
		}
	}

	DL_HSADC_PowerUp(ADC_REGS(config));

	if (adc_msp_hsadc_uses_internal_vref(config->vref_mv)) {
		int timeout = 1000;

		while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
			k_busy_wait(10);
			if (--timeout == 0) {
				LOG_ERR("VREF not ready after ADC power-up");
				return -ETIMEDOUT;
			}
		}
	}

	/* Initialize driver data */
	data->ch_configured = 0;
	memset(data->ch_acq_time, 0, sizeof(data->ch_acq_time));

#ifdef CONFIG_ADC_MSP_HSADC_DMA
	if (config->dma_dev != NULL && !device_is_ready(config->dma_dev)) {
		LOG_ERR("DMA device not ready");
		return -ENODEV;
	}
#endif

	config->irq_cfg_func();
	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}

static int adc_msp_hsadc_channel_setup(const struct device *dev,
				       const struct adc_channel_cfg *channel_cfg)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	const uint8_t ch = channel_cfg->channel_id;

	if (ch >= ADC_MSP_HSADC_CHANNEL_MAX) {
		LOG_ERR("Channel %d not supported, max %d", ch, ADC_MSP_HSADC_CHANNEL_MAX - 1);
		return -EINVAL;
	}

	if (channel_cfg->differential) {
		LOG_ERR("Differential channels not supported");
		return -EINVAL;
	}

	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("Only ADC_GAIN_1 supported");
		return -EINVAL;
	}

	if (channel_cfg->reference == ADC_REF_VDD_1) {
		if (adc_msp_hsadc_uses_internal_vref(config->vref_mv)) {
			LOG_WRN("Channel uses ADC_REF_VDD_1 but internal reference configured");
		}
	} else if (channel_cfg->reference == ADC_REF_INTERNAL) {
		if (!adc_msp_hsadc_uses_internal_vref(config->vref_mv)) {
			LOG_WRN("Channel uses ADC_REF_INTERNAL but VDDA configured");
		}
	} else {
		LOG_ERR("Unsupported reference voltage");
		return -EINVAL;
	}

	/* Decode acquisition time from zephyr,acquisition-time DT property.
	 * ACQPS valid range: 1-1472 SYSCLK cycles.
	 * Only ADC_ACQ_TIME_TICKS unit is supported.
	 */
	uint16_t acq_time = channel_cfg->acquisition_time;
	uint16_t acq_ticks;

	if (acq_time == ADC_ACQ_TIME_DEFAULT) {
		acq_ticks = ADC_MSP_HSADC_ACQ_TIME_DEFAULT;
	} else if (ADC_ACQ_TIME_UNIT(acq_time) == ADC_ACQ_TIME_TICKS) {
		acq_ticks = ADC_ACQ_TIME_VALUE(acq_time);
		if (acq_ticks < ADC_MSP_HSADC_ACQ_TIME_MIN ||
		    acq_ticks > ADC_MSP_HSADC_ACQ_TIME_MAX) {
			LOG_ERR("Channel %d: acq_time %d out of range (%d-%d)", ch, acq_ticks,
				ADC_MSP_HSADC_ACQ_TIME_MIN, ADC_MSP_HSADC_ACQ_TIME_MAX);
			return -EINVAL;
		}
	} else {
		LOG_ERR("Channel %d: only ADC_ACQ_TIME_TICKS supported", ch);
		return -ENOTSUP;
	}

	/* Store channel configuration */
	data->ch_acq_time[ch] = acq_ticks;
	data->ch_configured |= BIT(ch);

	LOG_DBG("Channel %d: acq_time=%d SYSCLK cycles", ch, acq_ticks);

	return 0;
}

static int adc_msp_hsadc_configure_sequence(const struct device *dev,
					    const struct adc_sequence *sequence)
{
	const struct adc_msp_hsadc_cfg *config = dev->config;
	struct adc_msp_hsadc_data *data = dev->data;
	uint32_t channels = sequence->channels;
	uint32_t temp_channels;
	uint16_t acq_time = 0;
	uint8_t soc = 0;
	uint8_t end_soc = 0;

	/* Design assumption: Single sequencer (SEQ_NUMBER1) used for all channels.
	 * All channels must have identical acquisition time (hardware constraint).
	 */
	temp_channels = channels;
	while (temp_channels != 0) {
		uint8_t ch = find_lsb_set(temp_channels) - 1;

		if (!(data->ch_configured & BIT(ch))) {
			LOG_ERR("Channel %d not configured", ch);
			return -EINVAL;
		}

		if (acq_time == 0) {
			acq_time = data->ch_acq_time[ch];
		} else if (data->ch_acq_time[ch] != acq_time) {
			LOG_ERR("Channel %d acq_time=%d differs from %d", ch, data->ch_acq_time[ch],
				acq_time);
			return -EINVAL;
		}
		temp_channels &= ~BIT(ch);
	}

	/* Assign SOCs to channels (lowest channel first) */
	temp_channels = channels;
	while (temp_channels != 0) {
		uint8_t ch = find_lsb_set(temp_channels) - 1;

		DL_HSADC_SOCChannelSelect(ADC_REGS(config), (DL_HSADC_SOC_NUMBER)soc,
					  (DL_HSADC_ADCIN)ch);
		end_soc = soc;
		soc++;
		temp_channels &= ~BIT(ch);
	}

	/* Configure sequencer 0 (DL_HSADC_SEQ_NUMBER1 = seq 0) */
	DL_HSADC_disableSequencer(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1);

	/* Only software triggers supported (no hardware triggers).
	 * All conversions initiated by DL_HSADC_triggerSequencerSoftwareForce().
	 */
	DL_HSADC_setupSequencer(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1, acq_time,
				DL_HSADC_TRIGGER_TIELOW_SW, DL_HSADC_SOC_NUMBER0);

	DL_HSADC_InterruptSourceSelect(ADC_REGS(config), DL_HSADC_INT_1,
				       (DL_HSADC_SOC_NUMBER)end_soc);

	DL_HSADC_setSampleCapReset(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1,
				   DL_HSADC_SAMPCAPRESET_HALF_VREFHI);

	/* Always configure PPB oversampling registers to ensure clean state.
	 * A previous read with oversampling leaves LIMIT set in hardware,
	 * causing SOCs to repeat even when oversampling is now disabled.
	 * When oversampling=0: LIMIT=NULL (no accumulation), SHIFT=0.
	 */
	DL_HSADC_setPPBOversamplingLimit(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1,
					 (DL_HSADC_OVERSAMPLING_LIMIT)sequence->oversampling);
	DL_HSADC_setPPBRightShift(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1,
				  (DL_HSADC_PPB_RIGHTSHIFT)sequence->oversampling);

	DL_HSADC_enableSequencer(ADC_REGS(config), DL_HSADC_SEQ_NUMBER1);
	DL_HSADC_setEndOfSequencer(ADC_REGS(config), (DL_HSADC_SOC_NUMBER)end_soc);

#ifdef CONFIG_ADC_MSP_HSADC_DMA
	if (config->dma_dev != NULL) {
		/* Configure HSADC to generate DMA trigger at end-of-sequence */
		DL_HSADC_DMAInterruptStatusClear(ADC_REGS(config), DL_HSADC_DMA_INT_1);
		DL_HSADC_DMAInterruptSourceSelect(ADC_REGS(config), DL_HSADC_DMA_INT_1,
						  (DL_HSADC_SOC_NUMBER)end_soc);
		DL_HSADC_enableDMAInterrupt(ADC_REGS(config), DL_HSADC_DMA_INT_1);
	}
#endif

	return 0;
}

static int adc_msp_hsadc_read_internal(const struct device *dev,
				       const struct adc_sequence *sequence)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	size_t exp_size;
	int ret;
	int ch_count;

	if (DL_HSADC_isBusy(ADC_REGS(config))) {
		LOG_ERR("ADC is busy with another conversion");
		return -EBUSY;
	}

	ch_count = POPCOUNT(sequence->channels);
	if (ch_count == 0) {
		LOG_ERR("No ADC channels selected");
		return -EINVAL;
	} else if (ch_count > ADC_MSP_HSADC_SOC_MAX) {
		LOG_ERR("ADC implementation supports up to %d channels per sequence",
			ADC_MSP_HSADC_SOC_MAX);
		return -EINVAL;
	}

	exp_size = ch_count * sizeof(uint16_t);
	if (sequence->options) {
		exp_size *= (1 + sequence->options->extra_samplings);
	}

	if (sequence->buffer_size < exp_size) {
		LOG_ERR("Buffer too small: need %u bytes, have %u", exp_size,
			sequence->buffer_size);
		return -ENOMEM;
	}

	if (sequence->resolution != 12) {
		LOG_ERR("ADC resolution %d not supported. Only 12 bits.", sequence->resolution);
		return -EINVAL;
	}

	/* Validate oversampling ratio */
	if (sequence->oversampling > 3) {
		LOG_ERR("Oversampling value %d not supported. Max is 3 (8x oversampling)",
			sequence->oversampling);
		return -EINVAL;
	}

	/* Hardware oversampling (PPB) incompatible with multi-channel.
	 * PPB has single SUM register per sequencer; multiple SOCs overwrite each other.
	 * Only single-channel oversampling is supported.
	 */
	if (sequence->oversampling > 0 && ch_count > 1) {
		LOG_ERR("Oversampling is supported for single channel only");
		return -EINVAL;
	}

#ifdef CONFIG_ADC_MSP_HSADC_DMA
	/* DMA and oversampling use different data paths.
	 * DMA reads FIFO (packed 2x16-bit), oversampling reads PPB FinalSumResult.
	 * Cannot mix both in single read.
	 */
	if (config->dma_dev != NULL && sequence->oversampling > 0) {
		LOG_ERR("DMA mode does not support oversampling");
		return -EINVAL;
	}
#endif

	if (sequence->calibrate) {
		LOG_ERR("Calibration not supported");
		return -ENOTSUP;
	}

	/* Configure the ADC sequencers and SOCs BEFORE starting sampling */
	ret = adc_msp_hsadc_configure_sequence(dev, sequence);
	if (ret < 0) {
		return ret;
	}

	/* Now start the read operation (this will call adc_context_start_sampling) */
	adc_context_start_read(&data->ctx, sequence);
	return adc_context_wait_for_completion(&data->ctx);
}

static int adc_msp_hsadc_read(const struct device *dev, const struct adc_sequence *sequence)
{
	struct adc_msp_hsadc_data *data = dev->data;
	int error;

	adc_context_lock(&data->ctx, false, NULL);
	error = adc_msp_hsadc_read_internal(dev, sequence);
	adc_context_release(&data->ctx, error);

	return error;
}

#ifdef CONFIG_ADC_ASYNC
static int adc_msp_hsadc_read_async(const struct device *dev, const struct adc_sequence *sequence,
				    struct k_poll_signal *async)
{
	struct adc_msp_hsadc_data *data = dev->data;
	int error;

	adc_context_lock(&data->ctx, true, async);
	error = adc_msp_hsadc_read_internal(dev, sequence);
	adc_context_release(&data->ctx, error);

	return error;
}
#endif /* CONFIG_ADC_ASYNC */

static void adc_msp_hsadc_isr(const struct device *dev)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	uint32_t channels = data->ctx.sequence.channels;
	uint16_t *buffer = (uint16_t *)data->ctx.sequence.buffer;
	uint8_t soc = 0;
	uint32_t temp_channels = channels;
	uint8_t ch;

	/* Check for sequencer 0 overflow (DL_HSADC_INT_1 = seq 0) */
	if (DL_HSADC_InterruptOverflowStatus(ADC_REGS(config), DL_HSADC_INT_1)) {
		DL_HSADC_InterruptOverflowStatusClear(ADC_REGS(config), DL_HSADC_INT_1);
		DL_HSADC_disableInterrupt(ADC_REGS(config), DL_HSADC_INT_1);
		DL_HSADC_InterruptStatusClear(ADC_REGS(config), DL_HSADC_INT_1);
		LOG_ERR("Sequencer overflow - data loss");
		adc_context_complete(&data->ctx, -EIO);
		return;
	}

	/* Read results from SOCs in channel order (lowest channel first).
	 * SOC N contains result for the Nth channel in the bitmask.
	 *
	 * When oversampling is active, read from the PPB FinalSumResult which
	 * contains the hardware-averaged value (PSUM >> SHIFT). Only single-channel
	 * sequences are allowed with oversampling (validated in read_internal),
	 * so the PPB SUM register holds the correct averaged result.
	 */
	while (temp_channels != 0) {
		ch = find_lsb_set(temp_channels) - 1;

		if (data->ctx.sequence.oversampling > 0) {
			*buffer++ = DL_HSADC_getFinalSumResult(RESULT_REGS(config),
								DL_HSADC_SEQ_NUMBER1);
		} else {
			*buffer++ = DL_HSADC_getResult(RESULT_REGS(config),
								(DL_HSADC_SOC_NUMBER)soc);
		}
		soc++;
		temp_channels &= ~BIT(ch);
	}

	DL_HSADC_InterruptStatusClear(ADC_REGS(config), DL_HSADC_INT_1);
	DL_HSADC_disableInterrupt(ADC_REGS(config), DL_HSADC_INT_1);

	adc_context_on_sampling_done(&data->ctx, dev);
}

#define ADC_DT_CLOCK_DIVIDER(x) DT_INST_PROP(x, ti_clk_divider)

#ifdef CONFIG_ADC_MSP_HSADC_DMA
#define ADC_MSP_HSADC_DMA_INIT(index)                                                              \
	.dma_dev = DEVICE_DT_GET_OR_NULL(DT_INST_DMAS_CTLR_BY_IDX(index, 0)),                      \
	.dma_channel = DT_INST_DMAS_CELL_BY_IDX(index, 0, channel),                                \
	.dma_trigsrc = DT_INST_DMAS_CELL_BY_IDX(index, 0, trigger),
#else
#define ADC_MSP_HSADC_DMA_INIT(index)
#endif

#define MSP_HSADC_ADC_INIT(index)                                                                  \
                                                                                                   \
	static void adc_msp_hsadc_cfg_func_##index(void);                                          \
                                                                                                   \
	static const struct adc_msp_hsadc_cfg adc_msp_hsadc_cfg_##index = {                        \
		.config_base = DT_INST_REG_ADDR_BY_NAME(index, config),                            \
		.result_base = DT_INST_REG_ADDR_BY_NAME(index, result),                            \
		.irq_cfg_func = adc_msp_hsadc_cfg_func_##index,                                    \
		.clock_divider = ADC_DT_CLOCK_DIVIDER(index),                                      \
		.vref_mv = DT_INST_PROP(index, vref_mv),                                           \
		ADC_MSP_HSADC_DMA_INIT(index)};                                                    \
	static struct adc_msp_hsadc_data adc_msp_hsadc_data_##index = {                            \
		ADC_CONTEXT_INIT_TIMER(adc_msp_hsadc_data_##index, ctx),                           \
		ADC_CONTEXT_INIT_LOCK(adc_msp_hsadc_data_##index, ctx),                            \
		ADC_CONTEXT_INIT_SYNC(adc_msp_hsadc_data_##index, ctx),                            \
	};                                                                                         \
	static DEVICE_API(adc, msp_hsadc_driver_api_##index) = {                                   \
		.channel_setup = adc_msp_hsadc_channel_setup,                                      \
		.read = adc_msp_hsadc_read,                                                        \
		.ref_internal = DT_INST_PROP(index, vref_mv),                                      \
		IF_ENABLED(CONFIG_ADC_ASYNC,                                                       \
			   (.read_async = adc_msp_hsadc_read_async,)) };            \
	DEVICE_DT_INST_DEFINE(index, &adc_msp_hsadc_init, NULL, &adc_msp_hsadc_data_##index,       \
			      &adc_msp_hsadc_cfg_##index, POST_KERNEL, CONFIG_ADC_INIT_PRIORITY,   \
			      &msp_hsadc_driver_api_##index);                                      \
                                                                                                   \
	static void adc_msp_hsadc_cfg_func_##index(void)                                           \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), adc_msp_hsadc_isr,  \
			    DEVICE_DT_INST_GET(index), 0);                                         \
		irq_enable(DT_INST_IRQN(index));                                                   \
	}

DT_INST_FOREACH_STATUS_OKAY(MSP_HSADC_ADC_INIT)
