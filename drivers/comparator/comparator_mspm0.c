/*
 * Copyright (c) 2026 Texas Instruments Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_mspm0_comp_io

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/comparator.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* TI Driverlib includes */
#include <ti/driverlib/driverlib.h>

#define COMPARATOR_MSPM0_OUTPUT_READY_TIMEOUT_US 10000
#define COMPARATOR_MSPM0_OUTPUT_READY_POLL_US    10

#define PINCTRL_STATE_INPUT  PINCTRL_STATE_PRIV_START
#define PINCTRL_STATE_OUTPUT (PINCTRL_STATE_PRIV_START + 1U)

enum comparator_mspm0_mode {
	COMPARATOR_MSPM0_MODE_FAST = 0,
	COMPARATOR_MSPM0_MODE_ULP,
};

enum comparator_mspm0_reference_mode {
	COMPARATOR_MSPM0_REFERENCE_MODE_STATIC = 0,
	COMPARATOR_MSPM0_REFERENCE_MODE_SAMPLED,
};

enum comparator_mspm0_reference_source {
	COMPARATOR_MSPM0_REFERENCE_SOURCE_NONE = 0,
	COMPARATOR_MSPM0_REFERENCE_SOURCE_VDDA_DAC,
	COMPARATOR_MSPM0_REFERENCE_SOURCE_VREF_DAC,
	COMPARATOR_MSPM0_REFERENCE_SOURCE_VREF,
	COMPARATOR_MSPM0_REFERENCE_SOURCE_VDDA,
	COMPARATOR_MSPM0_REFERENCE_SOURCE_INTERNAL_VREF_DAC,
};

enum comparator_mspm0_reference_terminal {
	COMPARATOR_MSPM0_REFERENCE_TERMINAL_POSITIVE = 0,
	COMPARATOR_MSPM0_REFERENCE_TERMINAL_NEGATIVE,
};

struct comparator_mspm0_config {
	COMP_Regs *regs;
	const struct pinctrl_dev_config *pincfg;
	const struct device *vref;
	uint8_t positive_input_channel;
	uint8_t negative_input_channel;
	uint8_t dac_code0;
	uint8_t hysteresis_mv;
	uint8_t mode;
	uint8_t reference_mode;
	uint8_t reference_source;
	uint8_t reference_terminal;
	bool invert_output;
};

static DL_COMP_IPSEL_CHANNEL comparator_mspm0_positive_channel(uint8_t channel)
{
	switch (channel) {
	case 0:
		return DL_COMP_IPSEL_CHANNEL_0;
	case 1:
		return DL_COMP_IPSEL_CHANNEL_1;
	case 2:
		return DL_COMP_IPSEL_CHANNEL_2;
	case 3:
		return DL_COMP_IPSEL_CHANNEL_3;
	case 4:
		return DL_COMP_IPSEL_CHANNEL_4;
	case 5:
		return DL_COMP_IPSEL_CHANNEL_5;
	case 6:
		return DL_COMP_IPSEL_CHANNEL_6;
	default:
		return DL_COMP_IPSEL_CHANNEL_7;
	}
}

static DL_COMP_IMSEL_CHANNEL comparator_mspm0_negative_channel(uint8_t channel)
{
	switch (channel) {
	case 0:
		return DL_COMP_IMSEL_CHANNEL_0;
	case 1:
		return DL_COMP_IMSEL_CHANNEL_1;
	case 2:
		return DL_COMP_IMSEL_CHANNEL_2;
	case 3:
		return DL_COMP_IMSEL_CHANNEL_3;
	case 4:
		return DL_COMP_IMSEL_CHANNEL_4;
	case 5:
		return DL_COMP_IMSEL_CHANNEL_5;
	case 6:
		return DL_COMP_IMSEL_CHANNEL_6;
	default:
		return DL_COMP_IMSEL_CHANNEL_7;
	}
}

static DL_COMP_ENABLE_CHANNEL
comparator_mspm0_enabled_channels(const struct comparator_mspm0_config *config)
{
	if (config->reference_source == COMPARATOR_MSPM0_REFERENCE_SOURCE_NONE) {
		return DL_COMP_ENABLE_CHANNEL_POS_NEG;
	}

	if (config->reference_terminal == COMPARATOR_MSPM0_REFERENCE_TERMINAL_NEGATIVE) {
		return DL_COMP_ENABLE_CHANNEL_POS;
	}

	return DL_COMP_ENABLE_CHANNEL_NEG;
}

static DL_COMP_MODE comparator_mspm0_mode(uint8_t mode)
{
	if (mode == COMPARATOR_MSPM0_MODE_FAST) {
		return DL_COMP_MODE_FAST;
	}

	return DL_COMP_MODE_ULP;
}

static DL_COMP_HYSTERESIS comparator_mspm0_hysteresis(uint8_t hysteresis_mv)
{
	switch (hysteresis_mv) {
	case 10:
		return DL_COMP_HYSTERESIS_10;
	case 20:
		return DL_COMP_HYSTERESIS_20;
	case 30:
		return DL_COMP_HYSTERESIS_30;
	default:
		return DL_COMP_HYSTERESIS_NONE;
	}
}

static DL_COMP_REF_MODE comparator_mspm0_reference_mode(uint8_t mode)
{
	if (mode == COMPARATOR_MSPM0_REFERENCE_MODE_SAMPLED) {
		return DL_COMP_REF_MODE_SAMPLED;
	}

	return DL_COMP_REF_MODE_STATIC;
}

static DL_COMP_REF_SOURCE comparator_mspm0_reference_source(uint8_t source)
{
	switch (source) {
	case COMPARATOR_MSPM0_REFERENCE_SOURCE_VDDA_DAC:
		return DL_COMP_REF_SOURCE_VDDA_DAC;
	case COMPARATOR_MSPM0_REFERENCE_SOURCE_VREF_DAC:
		return DL_COMP_REF_SOURCE_VREF_DAC;
	case COMPARATOR_MSPM0_REFERENCE_SOURCE_VREF:
		return DL_COMP_REF_SOURCE_VREF;
	case COMPARATOR_MSPM0_REFERENCE_SOURCE_VDDA:
		return DL_COMP_REF_SOURCE_VDDA;
	case COMPARATOR_MSPM0_REFERENCE_SOURCE_INTERNAL_VREF_DAC:
		return DL_COMP_REF_SOURCE_INT_VREF_DAC;
	default:
		return DL_COMP_REF_SOURCE_NONE;
	}
}

static DL_COMP_REF_TERMINAL_SELECT comparator_mspm0_reference_terminal(uint8_t terminal)
{
	if (terminal == COMPARATOR_MSPM0_REFERENCE_TERMINAL_NEGATIVE) {
		return DL_COMP_REF_TERMINAL_SELECT_NEG;
	}

	return DL_COMP_REF_TERMINAL_SELECT_POS;
}

static bool comparator_mspm0_uses_vref(const struct comparator_mspm0_config *config)
{
	return config->reference_source == COMPARATOR_MSPM0_REFERENCE_SOURCE_VREF_DAC ||
	       config->reference_source == COMPARATOR_MSPM0_REFERENCE_SOURCE_VREF;
}

static int comparator_mspm0_wait_output_ready(COMP_Regs *regs)
{
	uint32_t elapsed_us = 0;

	while ((DL_COMP_getRawInterruptStatus(regs, DL_COMP_INTERRUPT_OUTPUT_READY) &
		DL_COMP_INTERRUPT_OUTPUT_READY) == 0U) {
		if (elapsed_us >= COMPARATOR_MSPM0_OUTPUT_READY_TIMEOUT_US) {
			return -ETIMEDOUT;
		}

		k_busy_wait(COMPARATOR_MSPM0_OUTPUT_READY_POLL_US);
		elapsed_us += COMPARATOR_MSPM0_OUTPUT_READY_POLL_US;
	}

	return 0;
}

static int comparator_mspm0_apply_pinctrl_state(const struct comparator_mspm0_config *config,
						uint8_t state)
{
	int ret;

	ret = pinctrl_apply_state(config->pincfg, state);
	if (ret == -ENOENT) {
		return 0;
	}

	return ret;
}

static int comparator_mspm0_get_output(const struct device *dev)
{
	const struct comparator_mspm0_config *config = dev->config;

	return DL_COMP_getComparatorOutput(config->regs) == DL_COMP_OUTPUT_HIGH ? 1 : 0;
}

static int comparator_mspm0_init(const struct device *dev)
{
	const struct comparator_mspm0_config *config = dev->config;
	DL_COMP_Config comp_config = {
		.mode = comparator_mspm0_mode(config->mode),
		.channelEnable = comparator_mspm0_enabled_channels(config),
		.posChannel = comparator_mspm0_positive_channel(config->positive_input_channel),
		.negChannel = comparator_mspm0_negative_channel(config->negative_input_channel),
		.polarity = config->invert_output ? DL_COMP_POLARITY_INV : DL_COMP_POLARITY_NON_INV,
		.hysteresis = comparator_mspm0_hysteresis(config->hysteresis_mv),
	};
	DL_COMP_RefVoltageConfig ref_config = {
		.mode = comparator_mspm0_reference_mode(config->reference_mode),
		.source = comparator_mspm0_reference_source(config->reference_source),
		.terminalSelect = comparator_mspm0_reference_terminal(config->reference_terminal),
		.controlSelect = DL_COMP_DAC_CONTROL_SW,
		.inputSelect = DL_COMP_DAC_INPUT_DACCODE0,
	};
	int ret;

	if (comparator_mspm0_uses_vref(config)) {
		if (config->vref == NULL || !device_is_ready(config->vref)) {
			return -ENODEV;
		}

		ret = regulator_is_enabled(config->vref);
		if (ret < 0) {
			return ret;
		}

		if (ret == 0) {
			return -ENODEV;
		}
	}

	ret = comparator_mspm0_apply_pinctrl_state(config, PINCTRL_STATE_INPUT);
	if (ret < 0) {
		return ret;
	}

	DL_COMP_reset(config->regs);
	DL_COMP_enablePower(config->regs);
	delay_cycles(CONFIG_MSPM0_PERIPH_STARTUP_DELAY);

	DL_COMP_init(config->regs, &comp_config);
	DL_COMP_refVoltageInit(config->regs, &ref_config);
	DL_COMP_setDACCode0(config->regs, config->dac_code0);
	DL_COMP_enable(config->regs);

	ret = comparator_mspm0_wait_output_ready(config->regs);
	if (ret < 0) {
		goto disable;
	}

	ret = comparator_mspm0_apply_pinctrl_state(config, PINCTRL_STATE_OUTPUT);
	if (ret < 0) {
		goto disable;
	}

	return 0;

disable:
	DL_COMP_disable(config->regs);
	DL_COMP_disablePower(config->regs);
	return ret;
}

static DEVICE_API(comparator, comparator_mspm0_api) = {
	.get_output = comparator_mspm0_get_output,
};

#define COMPARATOR_MSPM0_VREF_DEVICE(inst)                                                         \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, vref),				\
		    (DEVICE_DT_GET(DT_INST_PHANDLE(inst, vref))), (NULL))

#define COMPARATOR_MSPM0_INIT(inst)                                                                \
	BUILD_ASSERT(DT_INST_PROP(inst, ti_dac_code0) <= UINT8_MAX,                                \
		     "ti,dac-code0 must fit in 8 bits");                                           \
                                                                                                   \
	PINCTRL_DT_INST_DEFINE(inst);                                                              \
                                                                                                   \
	static const struct comparator_mspm0_config comparator_mspm0_config_##inst = {             \
		.regs = (COMP_Regs *)DT_INST_REG_ADDR(inst),                                       \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                                    \
		.vref = COMPARATOR_MSPM0_VREF_DEVICE(inst),                                        \
		.positive_input_channel = DT_INST_PROP(inst, ti_positive_input_channel),           \
		.negative_input_channel = DT_INST_PROP(inst, ti_negative_input_channel),           \
		.dac_code0 = DT_INST_PROP(inst, ti_dac_code0),                                     \
		.hysteresis_mv = DT_INST_PROP(inst, ti_hysteresis_mv),                             \
		.mode = DT_INST_ENUM_IDX(inst, ti_mode),                                           \
		.reference_mode = DT_INST_ENUM_IDX(inst, ti_reference_mode),                       \
		.reference_source = DT_INST_ENUM_IDX(inst, ti_reference_source),                   \
		.reference_terminal = DT_INST_ENUM_IDX(inst, ti_reference_terminal),               \
		.invert_output = DT_INST_PROP(inst, invert_output),                                \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, comparator_mspm0_init, NULL, NULL,                             \
			      &comparator_mspm0_config_##inst, POST_KERNEL,                        \
			      CONFIG_COMPARATOR_INIT_PRIORITY, &comparator_mspm0_api);

DT_INST_FOREACH_STATUS_OKAY(COMPARATOR_MSPM0_INIT)
