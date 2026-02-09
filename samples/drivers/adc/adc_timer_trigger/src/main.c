/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Timer-triggered ADC sample using MSPM33 event fabric
 *
 * Demonstrates hardware-triggered ADC conversions where a timer event
 * automatically triggers ADC sampling without CPU intervention.
 * Timer publishes events on channel 1, ADC subscribes to channel 1.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include "timer_event_config.h"

/* ADC device from devicetree */
#define ADC_NODE DT_NODELABEL(adc0)
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_VDD_1
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 200)
#define ADC_CHANNEL_ID 0

/* ADC channel configuration */
static const struct adc_channel_cfg adc_channel_cfg = {
	.gain = ADC_GAIN,
	.reference = ADC_REFERENCE,
	.acquisition_time = ADC_ACQUISITION_TIME,
	.channel_id = ADC_CHANNEL_ID,
};

/* ADC sequence buffer */
#define BUFFER_SIZE 1
static int16_t adc_sample_buffer[BUFFER_SIZE];

/* ADC sequence configuration */
static struct adc_sequence adc_sequence = {
	.buffer = adc_sample_buffer,
	.buffer_size = sizeof(adc_sample_buffer),
	.resolution = ADC_RESOLUTION,
};

/* Counter device from devicetree - configured to trigger ADC */
#define COUNTER_NODE DT_NODELABEL(counterg8_0)

/* Timer node for reading event type configuration */
#define TIMER_NODE DT_NODELABEL(timg8_0)

/* Timer event type from device tree (default to ZERO_EVENT if not specified) */
#define TIMER_EVENT_TYPE DT_PROP_OR(TIMER_NODE, ti_timer_event_type, 0)

/* Convert ADC raw value to millivolts (custom converter for this sample) */
static int32_t adc_convert_raw_to_millivolts(int16_t raw_value)
{
	/* For MSPM33 with 12-bit ADC and 3.3V reference:
	 * Voltage = (raw_value / 4095) * 3300 mV
	 */
	return (int32_t)((int64_t)raw_value * 3300 / 4095);
}

/* Get event type name for display */
static const char *get_event_name(enum timer_event_type event_type)
{
	switch (event_type) {
	case TIMER_EVENT_ZERO:
		return "ZERO_EVENT";
	case TIMER_EVENT_LOAD:
		return "LOAD_EVENT";
	case TIMER_EVENT_CC0_DN:
		return "CC0_DN";
	case TIMER_EVENT_CC0_UP:
		return "CC0_UP";
	case TIMER_EVENT_CC1_DN:
		return "CC1_DN";
	case TIMER_EVENT_CC1_UP:
		return "CC1_UP";
	default:
		return "UNKNOWN";
	}
}

int main(void)
{
	int err;
	uint32_t sample_count = 0;

	printk("\n=== MSPM33 Timer-Triggered ADC Sample ===\n");
	printk("This demonstrates hardware-triggered ADC conversions\n");
	printk("using the MSPM33 event fabric.\n\n");

	/* Get ADC device */
	const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		printk("ERROR: ADC device not ready\n");
		return -ENODEV;
	}
	printk("ADC device ready: %s\n", adc_dev->name);

	/* Get counter/timer device */
	const struct device *counter_dev = DEVICE_DT_GET(COUNTER_NODE);
	if (!device_is_ready(counter_dev)) {
		printk("ERROR: Counter device not ready\n");
		return -ENODEV;
	}
	printk("Counter device ready: %s\n", counter_dev->name);

	/* Configure ADC channel */
	err = adc_channel_setup(adc_dev, &adc_channel_cfg);
	if (err < 0) {
		printk("ERROR: Failed to configure ADC channel (err %d)\n", err);
		return err;
	}
	printk("ADC channel %d configured\n", ADC_CHANNEL_ID);

	/* Set up ADC sequence for this channel */
	adc_sequence.channels = BIT(ADC_CHANNEL_ID);

	/* Configure timer for event publishing with user-specified event type */
	enum timer_event_type event_type = (enum timer_event_type)TIMER_EVENT_TYPE;
	printk("Configuring timer for event publishing (event type: %d)...\n", event_type);
	err = configure_timer_event_publishing(counter_dev, event_type);
	if (err < 0) {
		printk("ERROR: Failed to configure timer event publishing (err %d)\n", err);
		return err;
	}

	/* Start timer for periodic ADC triggering at 1 kHz */
	printk("Starting timer at 1 kHz for ADC triggering...\n");
	err = start_timer_for_adc_trigger(counter_dev, 1000);
	if (err < 0) {
		printk("ERROR: Failed to start timer (err %d)\n", err);
		return err;
	}
	printk("Timer started\n");

	printk("\nConfiguration:\n");
	printk("  - Timer: %s\n", counter_dev->name);
	printk("  - Timer frequency: 1 kHz (1 ms period)\n");
	printk("  - Timer event type: %s\n", get_event_name(event_type));
	printk("  - Event channel: 1\n");
	printk("  - ADC channel: %d (PA27)\n", ADC_CHANNEL_ID);
	printk("  - ADC resolution: %d bits\n", ADC_RESOLUTION);
	printk("  - Reference voltage: VDDA (3.3V)\n");
	printk("  - Hardware trigger: Enabled (GEN_SUB_0)\n");
	printk("\nHardware trigger path:\n");
	printk("  Timer %s -> Event Channel 1 -> ADC GEN_SUB_0 -> Conversion\n",
	       get_event_name(event_type));

	printk("\nStarting periodic ADC sampling (timer-triggered)...\n");
	printk("Timer triggers ADC every 1 ms. Reading samples every 1 second.\n\n");

	while (1) {
		/* Start ADC read - waits for next hardware trigger */
		err = adc_read(adc_dev, &adc_sequence);
		if (err < 0) {
			printk("ERROR: ADC read failed (err %d)\n", err);
			k_sleep(K_MSEC(1000));
			continue;
		}

		/* Get the raw ADC value */
		int16_t raw_value = adc_sample_buffer[0];

		/* Convert to millivolts */
		int32_t millivolts = adc_convert_raw_to_millivolts(raw_value);

		/* Print result */
		sample_count++;
		printk("[%6u] ADC CH%d: Raw=%4d (0x%03X), Voltage=%4d mV\n",
		       sample_count, ADC_CHANNEL_ID, raw_value, raw_value & 0xFFF, millivolts);

		/* Wait 1 second before next reading (timer continues triggering at 1 kHz) */
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
