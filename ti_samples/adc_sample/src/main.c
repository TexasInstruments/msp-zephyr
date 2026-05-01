/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <ti/driverlib/driverlib.h>


/* PA22 for CLK_OUT */
#define GPIO_CLKOUT_PIN                                           DL_GPIO_PIN_22
#define GPIO_CLKOUT_IOMUX                                        (IOMUX_PINCM47)
#define GPIO_CLKOUT_IOMUX_FUNC                   IOMUX_PINCM47_PF_SYSCTL_CLK_OUT
#define GPIO_CLKOUT_PORT                                                   GPIOA


/*
 * ADC sample application for MSPM0 (lp_mspm0g3507).
 *
 * This application demonstrates periodic ADC sampling with automatic
 * low-power mode enabled. It reads one ADC channel every second and prints
 * both the raw ADC value and the converted millivolt reading.
 *
 * The ADC channel is configured via device tree overlay (boards/lp_mspm0g3507.overlay).
 * During k_msleep(), the PM policy manager automatically selects an appropriate
 * SoC sleep state and the system enters low-power mode.
 */

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA_FOR_INPUTS(node_id, prop, idx) \
	COND_CODE_1(DT_PHA_HAS_CELL_AT_IDX(node_id, prop, idx, input), \
		    (ADC_DT_SPEC_GET_BY_IDX(node_id, idx),), ())

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA_FOR_INPUTS)
};

#define SLEEP_TIME_MS   1000

int main(void)
{

	DL_GPIO_enablePower(GPIOA);
	delay_cycles(16);
	DL_GPIO_initPeripheralOutputFunction(
        GPIO_CLKOUT_IOMUX, GPIO_CLKOUT_IOMUX_FUNC);
    	DL_GPIO_enableOutput(GPIO_CLKOUT_PORT, GPIO_CLKOUT_PIN);
	DL_SYSCTL_enableExternalClock(DL_SYSCTL_CLK_OUT_SOURCE_ULPCLK, DL_SYSCTL_CLK_OUT_DIVIDE_16);

	int err;
	uint32_t count = 0;
	uint32_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			printk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}

		err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0) {
			printk("Could not setup channel #%zu (%d)\n", i, err);
			return 0;
		}
	}

	printk("ADC sample application started\n");

	while (1) {
		printk("Sample [%u]:\n", count++);
		for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
			int32_t val_mv;

			(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

			err = adc_read_dt(&adc_channels[i], &sequence);
			if (err < 0) {
				printk("  Could not read channel %d (%d)\n",
					adc_channels[i].channel_id, err);
				continue;
			}

			/*
			 * If using differential mode, the 16 bit value
			 * in the ADC sample buffer should be a signed 2's
			 * complement value.
			 */
			if (adc_channels[i].channel_cfg.differential) {
				val_mv = (int32_t)((int16_t)buf);
			} else {
				val_mv = (int32_t)buf;
			}

			printk("  Channel %d: raw=%u", adc_channels[i].channel_id, val_mv);
			err = adc_raw_to_millivolts_dt(&adc_channels[i], &val_mv);
			/* conversion to mV may not be supported, skip if not */
			if (err < 0) {
				printk(" (mV conversion not available)\n");
			} else {
				printk(", mv=%"PRId32"\n", val_mv);
			}
		}

		/* During this k_msleep(), the PM policy manager automatically
		 * selects an appropriate SoC sleep state and the system enters
		 * low-power mode. Timer interrupt wakes the system after 1000ms.
		 */
		delay_cycles(10000);
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
