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
#include <zephyr/pm/device.h>
#include <soc.h>

/* Driverlib includes */
#include <ti/driverlib/dl_hsadc.h>
#include <ti/driverlib/dl_vref.h>

/* VREF module instance */
#define VREF ((VREF_Regs *)VREF_BASE)

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#define ADC_MSP_HSADC_SOC_MAX (16)

/* Helper macros to reduce repeated type casts */
#define ADC_REGS(cfg)    ((hsadc_ADC_LITE_REGS_Regs *)(cfg)->config_base)
#define RESULT_REGS(cfg) ((hsadc_ADC_LITE_RESULT_REGS_Regs *)(cfg)->result_base)

/* VREF source options */
#define ADC_MSP_HSADC_VREF_VDDA          0
#define ADC_MSP_HSADC_VREF_INTERNAL_2_5V 1
#define ADC_MSP_HSADC_VREF_INTERNAL_1_4V 2

/* VDDA reference voltage in mV */
#define ADC_MSP_HSADC_VDDA_MV 3300

struct hsadc_channel_config {
	uint8_t channel_id;
	uint8_t soc;
	uint8_t sequencer;
	bool configured;
};

struct adc_msp_hsadc_data {
	struct adc_context ctx;
	const struct device *dev;
	uint16_t *buffer;
	uint16_t *repeat_buffer;

	struct hsadc_channel_config channels[ADC_MSP_HSADC_SOC_MAX];
	uint8_t num_configured;
	uint32_t active_channels;
	uint32_t active_sequencers;
	uint8_t oversampling; /* Current oversampling ratio for all channels */
};

struct adc_msp_hsadc_cfg {
	uint32_t config_base;
	uint32_t result_base;
	uint32_t clockDivider;
	uint32_t sampleWindow;
	void (*irq_cfg_func)(void);
	uint8_t vref_source;
};

static void adc_msp_hsadc_isr(const struct device *dev);

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct adc_msp_hsadc_data *data = CONTAINER_OF(ctx, struct adc_msp_hsadc_data, ctx);
	const struct device *dev = data->dev;
	const struct adc_msp_hsadc_cfg *config = dev->config;

	data->repeat_buffer = data->buffer;

	/* Clear status, enable interrupt, and trigger each active sequencer */
	for (int seq = 0; seq < 4; seq++) {
		if (data->active_sequencers & BIT(seq)) {
			DL_HSADC_InterruptStatusClear(ADC_REGS(config), (DL_HSADC_INT)seq);
			DL_HSADC_enableInterrupt(ADC_REGS(config), (DL_HSADC_INT)seq);
			DL_HSADC_triggerSequencerSoftwareForce(ADC_REGS(config),
							       (DL_HSADC_SEQ_NUMBER)seq);
		}
	}
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx, bool repeat)
{
	struct adc_msp_hsadc_data *data = CONTAINER_OF(ctx, struct adc_msp_hsadc_data, ctx);

	if (repeat) {
		data->buffer = data->repeat_buffer;
	}
}

static int adc_msp_hsadc_config_vref(int ref_mv)
{
	if (ref_mv == ADC_MSP_HSADC_VDDA_MV) {
		return 0; /* Using external VDDA, no VREF needed */
	}

	/* Validate and determine which internal reference voltage */
	bool use_2_5v;
	if (ref_mv == 2500) {
		use_2_5v = true;
	} else if (ref_mv == 1400) {
		use_2_5v = false;
	} else {
		LOG_ERR("Invalid VREF value: %d mV (expected 2500 or 1400)", ref_mv);
		return -EINVAL;
	}

	/* Check if VREF already enabled with correct voltage */
	if (DL_VREF_isEnabled(VREF)) {
		uint32_t current_config = VREF->CTL0 & VREF_CTL0_BUFCONFIG_MASK;
		uint32_t expected_config =
			use_2_5v ? VREF_CTL0_BUFCONFIG_OUTPUT2P5V : VREF_CTL0_BUFCONFIG_OUTPUT1P4V;
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
				      .bufConfig = use_2_5v ? DL_VREF_BUFCONFIG_OUTPUT_2_5V
							    : DL_VREF_BUFCONFIG_OUTPUT_1_4V,
				      .shModeEnable = DL_VREF_SHMODE_DISABLE,
				      .holdCycleCount = DL_VREF_HOLD_MIN,
				      .shCycleCount = DL_VREF_SH_MIN};
	DL_VREF_configReference(VREF, &vref_config);

	/* Wait for VREF to settle */
	int timeout = 1000;
	while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
		if (--timeout == 0) {
			LOG_ERR("VREF failed to settle within timeout");
			return -ETIMEDOUT;
		}
		k_busy_wait(10);
	}

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

	DL_HSADC_setClockDivideRatio(ADC_REGS(config), config->clockDivider);

	if (config->vref_source != ADC_MSP_HSADC_VREF_VDDA) {
		int ref_internal =
			(config->vref_source == ADC_MSP_HSADC_VREF_INTERNAL_2_5V) ? 2500 : 1400;
		LOG_INF("Configuring VREF to %d mV (vref_source=%d, VDDA=%d, 2.5V=%d, 1.4V=%d)",
			ref_internal, config->vref_source, ADC_MSP_HSADC_VREF_VDDA,
			ADC_MSP_HSADC_VREF_INTERNAL_2_5V, ADC_MSP_HSADC_VREF_INTERNAL_1_4V);
		int ret = adc_msp_hsadc_config_vref(ref_internal);
		if (ret < 0) {
			LOG_ERR("Failed to configure VREF: %d", ret);
			return ret;
		}
		LOG_INF("VREF configured successfully");
	}

	DL_HSADC_PowerUp(ADC_REGS(config));

	if (config->vref_source != ADC_MSP_HSADC_VREF_VDDA) {
		int timeout = 1000;
		while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
			k_busy_wait(10);
			if (--timeout == 0) {
				LOG_ERR("VREF not ready after ADC power-up");
				return -ETIMEDOUT;
			}
		}
	}

	/* Initialize all driver data to zero */
	data->active_sequencers = 0;
	data->active_channels = 0;
	data->num_configured = 0;
	data->oversampling = 0;
	memset(data->channels, 0, sizeof(data->channels));

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

	if (ch >= 32) {
		LOG_ERR("Channel 0x%X is not supported, max 31", ch);
		return -EINVAL;
	}

	if (channel_cfg->differential) {
		LOG_ERR("Differential channels are not supported");
		return -EINVAL;
	}

	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("Gain is not valid");
		return -EINVAL;
	}

	if (channel_cfg->reference == ADC_REF_VDD_1) {
		if (config->vref_source != ADC_MSP_HSADC_VREF_VDDA) {
			LOG_WRN("Using VDDA reference despite internal reference being configured");
		}
	} else if (channel_cfg->reference == ADC_REF_INTERNAL) {
		if (config->vref_source == ADC_MSP_HSADC_VREF_VDDA) {
			LOG_ERR("Internal reference requested but not configured");
			return -EINVAL;
		}
	} else {
		LOG_ERR("Unsupported reference voltage");
		return -EINVAL;
	}

	int slot = -1;
	for (int i = 0; i < ADC_MSP_HSADC_SOC_MAX; i++) {
		if (data->channels[i].configured && data->channels[i].channel_id == ch) {
			slot = i;
			break;
		}
		if (!data->channels[i].configured && slot == -1) {
			slot = i;
		}
	}

	if (slot == -1) {
		LOG_ERR("No free slots for channel configuration");
		return -ENOMEM;
	}

	data->channels[slot].channel_id = ch;
	data->channels[slot].configured = true;

	if (slot == data->num_configured) {
		data->num_configured++;
	}

	return 0;
}

static int adc_msp_hsadc_configure_sequence(const struct device *dev)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	uint32_t active_channels = data->active_channels;
	uint8_t ch_id;
	uint8_t next_soc = 0;
	uint8_t current_seq = 0;
	uint8_t socs_in_current_seq = 0;

	/* Reset active sequencers */
	data->active_sequencers = 0;

	/* Track start and end SOC for each sequencer */
	uint8_t seq_start_soc[4];
	uint8_t seq_end_soc[4];

	/* Allocate sequencers and SOCs dynamically based on enabled channels */
	uint32_t temp_channels = active_channels;
	while (temp_channels) {
		ch_id = find_lsb_set(temp_channels) - 1;

		/* Find this channel in our configuration */
		struct hsadc_channel_config *ch_cfg = NULL;
		for (int i = 0; i < data->num_configured; i++) {
			if (data->channels[i].configured && data->channels[i].channel_id == ch_id) {
				ch_cfg = &data->channels[i];
				break;
			}
		}

		if (!ch_cfg) {
			LOG_ERR("Channel %d requested but not configured", ch_id);
			return -EINVAL;
		}

		/* Check if we've reached the SOC limit */
		if (next_soc >= ADC_MSP_HSADC_SOC_MAX) {
			LOG_ERR("Too many channels, max SOCs: %d", ADC_MSP_HSADC_SOC_MAX);
			return -EINVAL;
		}

		/* Allocate new sequencer if current is full (4 SOCs per sequencer) */
		if (socs_in_current_seq >= 4) {
			current_seq++;
			socs_in_current_seq = 0;

			if (current_seq >= 4) {
				LOG_ERR("Too many channels (max 16 SOCs across 4 sequencers)");
				return -EINVAL;
			}
		}

		/* Track sequencer start/end SOCs */
		if (socs_in_current_seq == 0) {
			seq_start_soc[current_seq] = next_soc;
		}
		seq_end_soc[current_seq] = next_soc;

		ch_cfg->soc = next_soc;
		ch_cfg->sequencer = current_seq;
		data->active_sequencers |= BIT(current_seq);

		DL_HSADC_SOCChannelSelect(ADC_REGS(config), (DL_HSADC_SOC_NUMBER)next_soc,
					  (DL_HSADC_ADCIN)ch_id);

		socs_in_current_seq++;
		next_soc++;
		temp_channels &= ~BIT(ch_id);
	}

	uint8_t last_seq_end_soc = 0;
	for (int seq = 0; seq < 4; seq++) {
		if (data->active_sequencers & BIT(seq)) {
			DL_HSADC_disableSequencer(ADC_REGS(config), (DL_HSADC_SEQ_NUMBER)seq);

			uint8_t seq_start = seq_start_soc[seq];
			uint8_t seq_end = seq_end_soc[seq];
			last_seq_end_soc = seq_end;

			DL_HSADC_setupSequencer(ADC_REGS(config), (DL_HSADC_SEQ_NUMBER)seq,
						config->sampleWindow, DL_HSADC_TRIGGER_TIELOW_SW,
						(DL_HSADC_SOC_NUMBER)seq_start);

			DL_HSADC_InterruptSourceSelect(ADC_REGS(config), (DL_HSADC_INT)seq,
						       (DL_HSADC_SOC_NUMBER)seq_end);

			DL_HSADC_setSampleCapReset(ADC_REGS(config), (DL_HSADC_SEQ_NUMBER)seq,
						   DL_HSADC_SAMPCAPRESET_HALF_VREFHI);

			/* Configure oversampling if enabled (same for all sequencers) */
			if (data->oversampling > 0) {
				DL_HSADC_setPPBOversamplingLimit(
					ADC_REGS(config), (DL_HSADC_SEQ_NUMBER)seq,
					(DL_HSADC_OVERSAMPLING_LIMIT)data->oversampling);

				DL_HSADC_setPPBRightShift(
					ADC_REGS(config), (DL_HSADC_SEQ_NUMBER)seq,
					(DL_HSADC_PPB_RIGHTSHIFT)data->oversampling);
			}

			DL_HSADC_enableSequencer(ADC_REGS(config), (DL_HSADC_SEQ_NUMBER)seq);
		}
	}

	DL_HSADC_setEndOfSequencer(ADC_REGS(config), (DL_HSADC_SOC_NUMBER)last_seq_end_soc);

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

	if (DL_HSADC_isBusy(ADC_REGS(config))) {
		LOG_ERR("ADC is busy with another conversion");
		return -EBUSY;
	}

	data->active_channels = sequence->channels;
	ch_count = POPCOUNT(data->active_channels);
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
		LOG_ERR("Required buffer size is %u, but %u got", exp_size, sequence->buffer_size);
		return -ENOMEM;
	}

	if (sequence->resolution != 12) {
		LOG_ERR("ADC resolution %d not supported. Only 12 bits.", sequence->resolution);
		return -EINVAL;
	}

	data->buffer = sequence->buffer;

	/* Validate and store oversampling ratio (same for all channels in sequence) */
	if (sequence->oversampling > 3) {
		LOG_ERR("Oversampling value %d not supported. Max is 3 (8x oversampling)",
			sequence->oversampling);
		return -EINVAL;
	}
	data->oversampling = sequence->oversampling;

	if (sequence->calibrate) {
		LOG_ERR("Calibration not supported");
		return -ENOTSUP;
	}

	/* Configure the ADC sequencers and SOCs */
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

#ifdef CONFIG_PM_DEVICE
	error = pm_device_runtime_get(dev);
	if (error < 0) {
		return error;
	}
#endif

	adc_context_lock(&data->ctx, false, NULL);
	error = adc_msp_hsadc_read_internal(dev, sequence);
	adc_context_release(&data->ctx, error);

#ifdef CONFIG_PM_DEVICE
	pm_device_runtime_put(dev);
#endif

	return error;
}

#ifdef CONFIG_ADC_ASYNC
static int adc_msp_hsadc_read_async(const struct device *dev, const struct adc_sequence *sequence,
				    struct k_poll_signal *async)
{
	struct adc_msp_hsadc_data *data = dev->data;
	int error;

#ifdef CONFIG_PM_DEVICE
	error = pm_device_runtime_get(dev);
	if (error < 0) {
		return error;
	}
#endif

	adc_context_lock(&data->ctx, true, async);
	error = adc_msp_hsadc_read_internal(dev, sequence);
	adc_context_release(&data->ctx, error);

#ifdef CONFIG_PM_DEVICE
	pm_device_runtime_put(dev);
#endif

	return error;
}
#endif /* CONFIG_ADC_ASYNC */

static void adc_msp_hsadc_isr(const struct device *dev)
{
	struct adc_msp_hsadc_data *data = dev->data;
	const struct adc_msp_hsadc_cfg *config = dev->config;
	uint32_t active_channels = data->active_channels;
	uint16_t result;
	bool interrupt_triggered = false;
	uint32_t processed_channels = 0;
	bool error_detected = false;

	/* Check for overflow errors on active sequencers */
	for (int seq = 0; seq < 4; seq++) {
		if (!(data->active_sequencers & BIT(seq))) {
			continue;
		}

		/* Check sequencer interrupt overflow */
		if (DL_HSADC_InterruptOverflowStatus(ADC_REGS(config), (DL_HSADC_INT)seq)) {
			DL_HSADC_InterruptOverflowStatusClear(ADC_REGS(config), (DL_HSADC_INT)seq);
			LOG_ERR("Sequencer %d overflow - data loss", seq);
			error_detected = true;
			break;
		}
	}

	/* Check SOC overflows only if no sequencer overflow detected */
	if (!error_detected) {
		for (uint8_t soc = 0; soc < ADC_MSP_HSADC_SOC_MAX; soc++) {
			if (DL_HSADC_getStartOfConversationOverflowStatus(
				    ADC_REGS(config), (DL_HSADC_SOC_NUMBER)soc)) {
				DL_HSADC_clearStartOfConversationOverflowStatus(
					ADC_REGS(config), (DL_HSADC_SOC_NUMBER)soc);
				LOG_ERR("SOC %d overflow - data loss", soc);
				error_detected = true;
				break;
			}
		}
	}

	/* If overflow detected, abort sequence and report error */
	if (error_detected) {
		for (int seq = 0; seq < 4; seq++) {
			if (data->active_sequencers & BIT(seq)) {
				DL_HSADC_disableInterrupt(ADC_REGS(config), (DL_HSADC_INT)seq);
				DL_HSADC_InterruptStatusClear(ADC_REGS(config), (DL_HSADC_INT)seq);
			}
		}
		adc_context_complete(&data->ctx, -EIO);
		return;
	}

	for (int seq = 0; seq < 4; seq++) {
		if ((data->active_sequencers & BIT(seq)) &&
		    DL_HSADC_getInterruptStatus(ADC_REGS(config), (DL_HSADC_INT)seq)) {
			interrupt_triggered = true;

			for (int i = 0; i < data->num_configured; i++) {
				struct hsadc_channel_config *ch_cfg = &data->channels[i];

				if (!ch_cfg->configured ||
				    !(active_channels & BIT(ch_cfg->channel_id))) {
					continue;
				}

				if (ch_cfg->sequencer != seq) {
					continue;
				}

				if (data->oversampling > 0) {
					/* Oversampling enabled: read sum of all samples
					 * Oversampling values: 0=1x (none), 1=2x, 2=4x, 3=8x (max)
					 * Formula: num_samples = 2^oversampling (for values 1-3)
					 */
					result = DL_HSADC_getFinalSumResult(
						RESULT_REGS(config), (DL_HSADC_SEQ_NUMBER)seq);
					uint8_t num_samples = 1 << (data->oversampling + 1);
					result = result / num_samples;
				} else {
					/* No oversampling: read single conversion result */
					result = DL_HSADC_getResult(
						RESULT_REGS(config),
						(DL_HSADC_SOC_NUMBER)ch_cfg->soc);
				}

				*data->buffer++ = result;
				processed_channels |= BIT(ch_cfg->channel_id);
			}

			DL_HSADC_InterruptStatusClear(ADC_REGS(config), (DL_HSADC_INT)seq);
			DL_HSADC_disableInterrupt(ADC_REGS(config), (DL_HSADC_INT)seq);
		}
	}

	if (interrupt_triggered && processed_channels == active_channels) {
		adc_context_on_sampling_done(&data->ctx, dev);
	}
}

#ifdef CONFIG_PM_DEVICE
static int adc_msp_hsadc_pm_action(const struct device *dev, enum pm_device_action action)
{
	const struct adc_msp_hsadc_cfg *config = dev->config;
	struct adc_msp_hsadc_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
	case PM_DEVICE_ACTION_TURN_OFF:
		/* Check if ADC is busy before suspending */
		if (DL_HSADC_isBusy(ADC_REGS(config))) {
			LOG_WRN("Cannot suspend ADC while conversion in progress");
			return -EBUSY;
		}
		DL_HSADC_PowerDown(ADC_REGS(config));
		DL_HSADC_disablePower(ADC_REGS(config));
		break;

	case PM_DEVICE_ACTION_RESUME:
	case PM_DEVICE_ACTION_TURN_ON:
		DL_HSADC_enablePower(ADC_REGS(config));
		delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);
		DL_HSADC_PowerUp(ADC_REGS(config));

		if (config->vref_source != ADC_MSP_HSADC_VREF_VDDA) {
			int timeout = 1000;
			while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
				k_busy_wait(10);
				if (--timeout == 0) {
					LOG_ERR("VREF failed to settle on resume");
					return -ETIMEDOUT;
				}
			}
		}
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}
#endif

static DEVICE_API(adc, msp_hsadc_driver_api) = {
	.channel_setup = adc_msp_hsadc_channel_setup,
	.read = adc_msp_hsadc_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = adc_msp_hsadc_read_async,
#endif /* CONFIG_ADC_ASYNC */
};

#define ADC_DT_CLOCK_DIVIDER(x) DT_INST_PROP(x, ti_clk_divider)
#define ADC_DT_SAMPLE_WINDOW(x) DT_INST_PROP(x, ti_sample_window)

/* Helper macro to get DT VREF source value directly as integer */
#define ADC_MSP_HSADC_VREF_SOURCE(index) DT_INST_PROP(index, ti_vref_source)

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
	};                                                                                         \
	static struct adc_msp_hsadc_data adc_msp_hsadc_data_##index = {                            \
		ADC_CONTEXT_INIT_TIMER(adc_msp_hsadc_data_##index, ctx),                           \
		ADC_CONTEXT_INIT_LOCK(adc_msp_hsadc_data_##index, ctx),                            \
		ADC_CONTEXT_INIT_SYNC(adc_msp_hsadc_data_##index, ctx),                            \
	};                                                                                         \
	IF_ENABLED(CONFIG_PM_DEVICE, (PM_DEVICE_DT_INST_DEFINE(index, adc_msp_hsadc_pm_action);))                                                                                 \
	DEVICE_DT_INST_DEFINE(index, &adc_msp_hsadc_init, PM_DEVICE_DT_INST_GET_OR_NULL(index),    \
			      &adc_msp_hsadc_data_##index, &adc_msp_hsadc_cfg_##index,             \
			      POST_KERNEL, CONFIG_ADC_INIT_PRIORITY, &msp_hsadc_driver_api);       \
                                                                                                   \
	static void adc_msp_hsadc_cfg_func_##index(void)                                           \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), adc_msp_hsadc_isr,  \
			    DEVICE_DT_INST_GET(index), 0);                                         \
		irq_enable(DT_INST_IRQN(index));                                                   \
	}

DT_INST_FOREACH_STATUS_OKAY(MSP_HSADC_ADC_INIT)
