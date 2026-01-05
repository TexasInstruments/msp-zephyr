/*
 * Copyright (c) 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_msp_hsadc

#include <errno.h>

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc_msp_hsadc);

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/adc.h>
#include <soc.h>

/* Driverlib includes */
#include <ti/driverlib/dl_hsadc.h>
#include <ti/driverlib/dl_vref.h>

/* VREF module instance */
#define VREF ((VREF_Regs *)VREF_BASE)

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

/* This ADC implementation supports up to 16 SOCs for sequence conversion */
#define ADC_MSP_HSADC_SOC_MAX (16)

/* VREF source options */
#define ADC_MSP_HSADC_VREF_VDDA          0
#define ADC_MSP_HSADC_VREF_INTERNAL_2_5V 1
#define ADC_MSP_HSADC_VREF_INTERNAL_1_4V 2

/* VDDA reference voltage in mV */
#define ADC_MSP_HSADC_VDDA_MV 3300

struct adc_msp_hsadc_data {
	struct adc_context ctx;
	const struct device *dev;
	uint16_t *buffer;
	uint16_t *repeat_buffer;

	uint8_t sequencer_mapping[ADC_MSP_HSADC_SOC_MAX]; /* Maps channels to sequencers */
	uint8_t soc_mapping[ADC_MSP_HSADC_SOC_MAX];       /* Maps SOCs to channels */
	uint8_t channel_to_soc[ADC_MSP_HSADC_SOC_MAX];    /* Maps channels to SOCs (reverse of
							     soc_mapping) */

	uint32_t channels;
	uint16_t oversampling;
	uint32_t active_sequencers; /* Bitmap of active sequencers */
};

struct adc_msp_hsadc_cfg {
	uint32_t config_base; /* Base address for ADC_LITE_REGS */
	uint32_t result_base; /* Base address for ADC_LITE_RESULT_REGS */
	uint32_t clockDivider;
	uint32_t sampleWindow; /* Sample window in ADC clock cycles */
	void (*irq_cfg_func)(void);
	uint8_t vref_source;
	uint16_t ref_internal; /* mV */
};

static void adc_msp_hsadc_isr(const struct device *dev);

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct adc_msp_hsadc_data *data = CONTAINER_OF(ctx, struct adc_msp_hsadc_data, ctx);
	const struct device *dev = data->dev;
	const struct adc_msp_hsadc_cfg *config = dev->config;

	data->repeat_buffer = data->buffer;

	/* Clear and enable interrupts for all active sequencers */
	for (int seq = 0; seq < 4; seq++) {
		if (data->active_sequencers & BIT(seq)) {
			/* Clear any pending interrupts */
			DL_HSADC_InterruptStatusClear(
				(hsadc_ADC_LITE_REGS_Regs *)config->config_base,
				(DL_HSADC_INT)seq);

			/* Enable the interrupt */
			DL_HSADC_enableInterrupt((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						 (DL_HSADC_INT)seq);
		}
	}

	/* Trigger all active sequencers */
	for (int seq = 0; seq < 4; seq++) {
		if (data->active_sequencers & BIT(seq)) {
			DL_HSADC_triggerSequencerSoftwareForce(
				(hsadc_ADC_LITE_REGS_Regs *)config->config_base,
				(DL_HSADC_SEQ_NUMBER)seq);
		}
	}
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx, bool repeat)
{
	struct adc_msp_hsadc_data *data = CONTAINER_OF(ctx, struct adc_msp_hsadc_data, ctx);

	if (repeat) {
		data->buffer = data->repeat_buffer;
	} else {
		data->buffer++;
	}
}

/**
 * @brief Configure the VREF module for internal reference
 *
 * This function configures the VREF module when an internal reference
 * voltage is selected. It checks if VREF is already configured correctly,
 * and if not, initializes it with the appropriate voltage level (2.5V or 1.4V).
 *
 * @param ref_internal Reference voltage value in mV (2500 or 1400)
 * @return 0 on success, negative error code on failure
 */
static int adc_msp_hsadc_config_vref(int ref_internal)
{
	int error = 0;
	bool init_vref = false;
	bool use_2_5v = false;

	/* No need to initialize VREF for VDDA */
	if (ref_internal == ADC_MSP_HSADC_VDDA_MV) {
		return 0;
	}

	/* Determine which internal reference to use */
	if (ref_internal == 2500) {
		use_2_5v = true;
	} else if (ref_internal == 1400) {
		use_2_5v = false;
	} else {
		LOG_ERR("Invalid VREF value: %d mV", ref_internal);
		return -EINVAL;
	}

	/* Check if VREF is already powered */
	if ((VREF->GPRCM.PWREN & VREF_PWREN_ENABLE_MASK) == VREF_PWREN_ENABLE_DISABLE) {
		/* VREF not powered, need to initialize */
		init_vref = true;
	} else {
		/* VREF is already powered, check if it's configured correctly */
		if (DL_VREF_isEnabled(VREF)) {
			/* VREF is enabled, check configuration */
			if (use_2_5v && ((VREF->CTL0 & VREF_CTL0_BUFCONFIG_MASK) ==
					 VREF_CTL0_BUFCONFIG_OUTPUT2P5V)) {
				/* Already configured for 2.5V */
				LOG_DBG("VREF already configured for 2.5V");
				return 0;
			} else if (!use_2_5v && ((VREF->CTL0 & VREF_CTL0_BUFCONFIG_MASK) ==
						 VREF_CTL0_BUFCONFIG_OUTPUT1P4V)) {
				/* Already configured for 1.4V */
				LOG_DBG("VREF already configured for 1.4V");
				return 0;
			} else {
				/* VREF is configured but doesn't match requested configuration */
				LOG_ERR("VREF already configured with different voltage");
				return -EINVAL;
			}
		} else {
			/* VREF is powered but not enabled, need to initialize */
			init_vref = true;
		}
	}

	if (init_vref) {
		/* Initialize VREF module */
		DL_VREF_reset(VREF);
	DL_VREF_enablePower(VREF);
	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);

		/* Configure VREF clock */
		DL_VREF_ClockConfig vref_clk_config = {.clockSel = DL_VREF_CLOCK_BUSCLK,
						       .divideRatio = DL_VREF_CLOCK_DIVIDE_1};
		DL_VREF_setClockConfig(VREF, &vref_clk_config);

		/* Configure VREF */
		DL_VREF_Config vref_config = {.vrefEnable = DL_VREF_ENABLE_ENABLE,
					      .bufConfig = use_2_5v ? DL_VREF_BUFCONFIG_OUTPUT_2_5V
								    : DL_VREF_BUFCONFIG_OUTPUT_1_4V,
					      .shModeEnable = DL_VREF_SHMODE_DISABLE,
					      .holdCycleCount = DL_VREF_HOLD_MIN,
					      .shCycleCount = DL_VREF_SH_MIN};
		DL_VREF_configReference(VREF, &vref_config);

		/* Wait for VREF to be ready */
		while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
			/* Wait for VREF to be ready */
		}

		LOG_DBG("VREF initialized with %s reference", use_2_5v ? "2.5V" : "1.4V");
	}

	return error;
}

static int adc_msp_hsadc_init(const struct device *dev)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;

	LOG_DBG("Initializing %s", dev->name);

	data->dev = dev;

	/* Init power */
	DL_HSADC_reset((hsadc_ADC_LITE_REGS_Regs *)config->config_base);
	DL_HSADC_enablePower((hsadc_ADC_LITE_REGS_Regs *)config->config_base);

	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY); // wait for power to stabilize

	/* Configure clock */
	DL_HSADC_setClockDivideRatio((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
				     config->clockDivider);

	/* Initialize VREF if using internal reference voltage value */
	if (config->vref_source != ADC_MSP_HSADC_VREF_VDDA) {
		/* Initialize VREF with the configured reference voltage */
		int ref_internal =
			(config->vref_source == ADC_MSP_HSADC_VREF_INTERNAL_2_5V) ? 2500 : 1400;
		int ret = adc_msp_hsadc_config_vref(ref_internal);
		if (ret < 0) {
			LOG_ERR("Failed to configure VREF: %d", ret);
			return ret;
		}
	}

	/* Power up the ADC */
	DL_HSADC_PowerUp((hsadc_ADC_LITE_REGS_Regs *)config->config_base);

	/* Reset active sequencers */
	data->active_sequencers = 0;

	/* Initialize sequencer mapping */
	for (int i = 0; i < ADC_MSP_HSADC_SOC_MAX; i++) {
		data->sequencer_mapping[i] = 0xFF; /* Invalid sequencer */
		data->soc_mapping[i] = 0xFF;       /* Invalid channel */
		data->channel_to_soc[i] = 0xFF;    /* Invalid SOC */
	}

	config->irq_cfg_func();

	adc_context_unlock_unconditionally(&data->ctx);
	return 0;
}

static int adc_msp_hsadc_channel_setup(const struct device *dev,
				       const struct adc_channel_cfg *channel_cfg)
{
	const struct adc_msp_hsadc_cfg *config = dev->config;
	const uint8_t ch = channel_cfg->channel_id;

	if (ch >= 32) { /* HSADC supports up to 32 channels */
		LOG_ERR("Channel 0x%X is not supported, max 31", ch);
		return -EINVAL;
	}

	/* Validate channel configuration */
	if (channel_cfg->differential) {
		LOG_ERR("Differential channels are not supported");
		return -EINVAL;
	}

	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("Gain is not valid");
		return -EINVAL;
	}

	/* Check reference voltage configuration */
	if (channel_cfg->reference == ADC_REF_VDD_1) {
		/* Using VDD as reference is always supported */
		if (config->vref_source != ADC_MSP_HSADC_VREF_VDDA) {
			LOG_WRN("Using VDDA reference despite internal reference being configured");
		}
		LOG_DBG("Using VDDA as reference voltage");
	} else if (channel_cfg->reference == ADC_REF_INTERNAL) {
		/* Internal reference is only valid if configured */
		if (config->vref_source == ADC_MSP_HSADC_VREF_VDDA) {
			LOG_ERR("Internal reference requested but not configured");
			return -EINVAL;
		}
		LOG_DBG("Using internal reference: %d mV", config->ref_internal);
	} else {
		LOG_ERR("Unsupported reference voltage");
		return -EINVAL;
	}

	/* We don't configure the SOC here - that's done during sequence configuration */
	LOG_DBG("ADC Channel setup successful!");
	return 0;
}

static int adc_msp_hsadc_configure_sequence(const struct device *dev)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	uint32_t channels;
	uint8_t ch;

	/* Configure all enabled channels for the sequence */
	channels = data->channels;

	/* Reset active sequencers, channel mappings, and SOC assignments */
	data->active_sequencers = 0;
	uint8_t next_soc = 0;

	for (int i = 0; i < ADC_MSP_HSADC_SOC_MAX; i++) {
		data->sequencer_mapping[i] = 0xFF; /* Invalid sequencer */
		data->soc_mapping[i] = 0xFF;       /* Invalid channel */
		data->channel_to_soc[i] = 0xFF;    /* Invalid SOC */
	}

	/* Group channels by sequencer based on hardware requirements */
	uint8_t seq_positions[4] = {0, 0, 0, 0}; /* Track position in each sequencer */

	/* Map channels to appropriate sequencers and SOCs */
	uint32_t temp_channels = channels;
	while (temp_channels) {
		ch = find_lsb_set(temp_channels) - 1;

		/* Check if we've reached the SOC limit */
		if (next_soc >= ADC_MSP_HSADC_SOC_MAX) {
			LOG_ERR("Too many channels requested, max SOCs: %d", ADC_MSP_HSADC_SOC_MAX);
			return -EINVAL;
		}

		/* Determine which sequencer this SOC belongs to */
		uint8_t soc = next_soc;
		uint8_t seq = soc / 4; /* Group SOCs in blocks of 4 */

		/* Mark this sequencer as active */
		data->active_sequencers |= BIT(seq);

		/* Map this channel to this sequencer */
		data->sequencer_mapping[ch] = seq;
		seq_positions[seq]++; /* Increment position counter for this sequencer */

		/* Map this SOC to this channel and vice versa */
		data->soc_mapping[soc] = ch;
		data->channel_to_soc[ch] = soc;

		/* Configure the SOC to use the specified channel */
		DL_HSADC_SOCChannelSelect((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
					  (DL_HSADC_SOC_NUMBER)soc, (DL_HSADC_ADCIN)ch);

		/* Move to next SOC */
		next_soc++;

		temp_channels &= ~BIT(ch);
	}

	/* Configure each active sequencer */
	uint8_t last_seq_end_soc = 0;
	for (int seq = 0; seq < 4; seq++) {
		if (data->active_sequencers & BIT(seq)) {
			/* Disable the sequencer first for configuration */
			DL_HSADC_disableSequencer((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						  (DL_HSADC_SEQ_NUMBER)seq);

			/* Calculate start and end SOCs for this sequencer */
			uint8_t seq_start_soc = seq * 4;
			uint8_t seq_end_soc = seq_start_soc + seq_positions[seq] - 1;
			last_seq_end_soc = seq_end_soc;

			/* Set up sequencer with proper trigger mode */
			DL_HSADC_setupSequencer((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						(DL_HSADC_SEQ_NUMBER)seq, config->sampleWindow,
						DL_HSADC_TRIGGER_TIELOW_SW,
						(DL_HSADC_SOC_NUMBER)seq_start_soc);

	/* Configure interrupt to trigger on the last channel */
	DL_HSADC_InterruptSourceSelect(
		(hsadc_ADC_LITE_REGS_Regs *)config->config_base,
		(DL_HSADC_INT)seq, (DL_HSADC_SOC_NUMBER)seq_end_soc);

			/* Configure sample cap reset to half VREF for better accuracy */
			DL_HSADC_setSampleCapReset((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						   (DL_HSADC_SEQ_NUMBER)seq,
						   DL_HSADC_SAMPCAPRESET_HALF_VREFHI);

			/* Configure oversampling if requested */
			if (data->oversampling > 0) {
				/* Configure oversampling for this sequencer */
				DL_HSADC_setPPBOversamplingLimit(
					(hsadc_ADC_LITE_REGS_Regs *)config->config_base,
					(DL_HSADC_SEQ_NUMBER)seq,
					(DL_HSADC_OVERSAMPLING_LIMIT)data->oversampling);

				/* Set right shift based on oversampling to maintain proper scaling
				 */
				DL_HSADC_setPPBRightShift(
					(hsadc_ADC_LITE_REGS_Regs *)config->config_base,
					(DL_HSADC_SEQ_NUMBER)seq,
					(DL_HSADC_PPB_RIGHTSHIFT)data->oversampling);
			}

			/* Enable the sequencer */
			DL_HSADC_enableSequencer((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						 (DL_HSADC_SEQ_NUMBER)seq);
		}
	}

	/* Set the end SOC for the sequencer using the last active sequencer's end SOC */
	DL_HSADC_setEndOfSequencer((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
				   (DL_HSADC_SOC_NUMBER)last_seq_end_soc);

	return 0;
}

static int adc_msp_hsadc_read_internal(const struct device *dev,
				       const struct adc_sequence *sequence)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	size_t exp_size;
	int sequence_ret;
	int ch_count;

	/* Check if ADC is busy */
	if (DL_HSADC_isBusy((hsadc_ADC_LITE_REGS_Regs *)config->config_base)) {
		LOG_ERR("ADC is busy with another conversion");
		return -EBUSY;
	}

	/* Validate resolution - HSADC supports 12-bit */
	if (sequence->resolution != 12) {
		LOG_ERR("ADC resolution %d not supported. Only 12 bits.", sequence->resolution);
		return -EINVAL;
	}

	/* Validate channel count */
	data->channels = sequence->channels;
	ch_count = POPCOUNT(data->channels);
	if (ch_count == 0) {
		LOG_ERR("No ADC channels selected");
		return -EINVAL;
	} else if (ch_count > ADC_MSP_HSADC_SOC_MAX) {
		LOG_ERR("ADC implementation supports up to %d channels per sequence",
			ADC_MSP_HSADC_SOC_MAX);
		return -EINVAL;
	}

	/* Validate buffer size */
	exp_size = ch_count * sizeof(uint16_t);
	if (sequence->options) {
		exp_size *= (1 + sequence->options->extra_samplings);
	}

	if (sequence->buffer_size < exp_size) {
		LOG_ERR("Required buffer size is %u, but %u got", exp_size, sequence->buffer_size);
		return -ENOMEM;
	}

	data->buffer = sequence->buffer;

	/* Configure oversampling if requested */
	data->oversampling = sequence->oversampling;

	/* Validate oversampling value - hardware supports 2x, 4x, and 8x */
	if (data->oversampling > 0) {
		/* Check if oversampling value is valid (0, 1, 2, or 3) */
		if (data->oversampling > 3) {
			LOG_ERR("Oversampling value %d not supported. Max is 3 (8x oversampling)",
				data->oversampling);
			return -EINVAL;
		}
	}

	if (sequence->calibrate) {
		LOG_ERR("Calibration not supported");
		return -ENOTSUP;
	}

	/* Configure the ADC sequence */
	sequence_ret = adc_msp_hsadc_configure_sequence(dev);
	if (sequence_ret < 0) {
		LOG_ERR("Error in ADC sequence configuration");
		return sequence_ret;
	}

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
	uint32_t channels = data->channels;
	uint8_t ch;
	uint16_t result;
	bool interrupt_triggered = false;
	uint32_t processed_channels = 0;

	/* Check if any of our interrupts are triggered */
	for (int seq = 0; seq < 4; seq++) {
		if ((data->active_sequencers & BIT(seq)) &&
		    DL_HSADC_getInterruptStatus((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						(DL_HSADC_INT)seq)) {
			interrupt_triggered = true;

			/* Process all channels for this sequencer */
			uint32_t seq_channels = channels;
			while (seq_channels) {
				ch = find_lsb_set(seq_channels) - 1;

				/* Only process channels mapped to this sequencer */
				if (data->sequencer_mapping[ch] == seq) {
					/* Get the SOC for this channel using direct lookup */
					uint8_t soc = data->channel_to_soc[ch];

					if (soc != 0xFF) {
						if (data->oversampling > 0) {
							/* When oversampling is enabled, read the
							 * final sum result */
							result = DL_HSADC_getFinalSumResult(
								(hsadc_ADC_LITE_RESULT_REGS_Regs *)
									config->result_base,
								(DL_HSADC_SEQ_NUMBER)seq);
						} else {
							/* Normal mode - read the direct ADC result
							 */
							result = DL_HSADC_getResult(
								(hsadc_ADC_LITE_RESULT_REGS_Regs *)
									config->result_base,
								(DL_HSADC_SOC_NUMBER)soc);
						}

						*data->buffer++ = result;
						processed_channels |= BIT(ch);
					}
				}

				seq_channels &= ~BIT(ch);
			}

			/* Clear the interrupt */
			DL_HSADC_InterruptStatusClear(
				(hsadc_ADC_LITE_REGS_Regs *)config->config_base,
				(DL_HSADC_INT)seq);

			/* Disable interrupt */
			DL_HSADC_disableInterrupt((hsadc_ADC_LITE_REGS_Regs *)config->config_base,
						  (DL_HSADC_INT)seq);
		}
	}

	/* Signal completion when all channels have been processed */
	if (interrupt_triggered && processed_channels == channels) {
		adc_context_on_sampling_done(&data->ctx, dev);
	}
}

static DEVICE_API(adc, msp_hsadc_driver_api) = {
	.channel_setup = adc_msp_hsadc_channel_setup,
	.read = adc_msp_hsadc_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = adc_msp_hsadc_read_async,
#endif /* CONFIG_ADC_ASYNC */
};

#define ADC_DT_CLOCK_DIVIDER(x) DT_INST_PROP(x, ti_clk_divider)
#define ADC_DT_SAMPLE_WINDOW(x) DT_INST_PROP(x, ti_sample_window)

/* Helper macro to convert DT VREF source string to enum value */
#define ADC_MSP_HSADC_VREF_SOURCE(index)                                                           \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(index, ti_vref_source),                             \
		(COND_CODE_1(DT_INST_STRING_TOKEN_EQUAL(index, ti_vref_source, vdda),         \
			(ADC_MSP_HSADC_VREF_VDDA),                                               \
			(COND_CODE_1(DT_INST_STRING_TOKEN_EQUAL(index, ti_vref_source,       \
							       internal_2_5v),             \
				(ADC_MSP_HSADC_VREF_INTERNAL_2_5V),                             \
				(ADC_MSP_HSADC_VREF_INTERNAL_1_4V))))),                         \
		(ADC_MSP_HSADC_VREF_VDDA))

#define MSP_HSADC_ADC_INIT(index)                                                                  \
                                                                                                   \
	static void adc_msp_hsadc_cfg_func_##index(void);                                          \
                                                                                                   \
	static const struct adc_msp_hsadc_cfg adc_msp_hsadc_cfg_##index = {                        \
		.config_base = DT_INST_REG_ADDR_BY_NAME(index, config),                            \
		.result_base = DT_INST_REG_ADDR_BY_NAME(index, result),                            \
		.irq_cfg_func = adc_msp_hsadc_cfg_func_##index,                                    \
		.clockDivider = ADC_DT_CLOCK_DIVIDER(index),                                       \
		.sampleWindow = ADC_DT_SAMPLE_WINDOW(index),                                       \
		.vref_source = ADC_MSP_HSADC_VREF_SOURCE(index),                                   \
		.ref_internal = DT_INST_PROP(index, vref_mv),                                      \
	};                                                                                         \
	static struct adc_msp_hsadc_data adc_msp_hsadc_data_##index = {                            \
		ADC_CONTEXT_INIT_TIMER(adc_msp_hsadc_data_##index, ctx),                           \
		ADC_CONTEXT_INIT_LOCK(adc_msp_hsadc_data_##index, ctx),                            \
		ADC_CONTEXT_INIT_SYNC(adc_msp_hsadc_data_##index, ctx),                            \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(index, &adc_msp_hsadc_init, NULL, &adc_msp_hsadc_data_##index,       \
			      &adc_msp_hsadc_cfg_##index, POST_KERNEL, CONFIG_ADC_INIT_PRIORITY,   \
			      &msp_hsadc_driver_api);                                              \
                                                                                                   \
	static void adc_msp_hsadc_cfg_func_##index(void)                                           \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), adc_msp_hsadc_isr,  \
			    DEVICE_DT_INST_GET(index), 0);                                         \
		irq_enable(DT_INST_IRQN(index));                                                   \
	}

DT_INST_FOREACH_STATUS_OKAY(MSP_HSADC_ADC_INIT)
