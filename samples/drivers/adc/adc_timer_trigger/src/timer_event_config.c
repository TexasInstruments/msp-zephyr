/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Timer event publishing configuration for ADC hardware triggering
 *
 * Configures MSPM33 timer to publish events on the event fabric for
 * triggering ADC conversions. This is a sample-specific configuration;
 * future work may integrate this into the timer driver.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/sys/printk.h>
#include "timer_event_config.h"

/* Driverlib includes for direct hardware configuration */
#include <ti/driverlib/dl_timera.h>
#include <soc.h>  /* For TIMG8_0 base address */

/* TIMG8_0 is already defined in device header */
#define TIMG8_0 ((GPTIMER_Regs *)TIMG8_0_BASE)

/* Event channel configuration */
#define ADC_TRIGGER_EVENT_CHANNEL 1  /* Must match ADC overlay config */

/* Map timer event type enum to driverlib event mask */
static uint32_t map_event_type(enum timer_event_type event_type)
{
	switch (event_type) {
	case TIMER_EVENT_ZERO:
		return DL_TIMERA_EVENT_ZERO_EVENT;
	case TIMER_EVENT_LOAD:
		return DL_TIMERA_EVENT_LOAD_EVENT;
	case TIMER_EVENT_CC0_DN:
		return DL_TIMERA_EVENT_CC0_DN_EVENT;
	case TIMER_EVENT_CC0_UP:
		return DL_TIMERA_EVENT_CC0_UP_EVENT;
	case TIMER_EVENT_CC1_DN:
		return DL_TIMERA_EVENT_CC1_DN_EVENT;
	case TIMER_EVENT_CC1_UP:
		return DL_TIMERA_EVENT_CC1_UP_EVENT;
	default:
		return DL_TIMERA_EVENT_ZERO_EVENT; /* Default to ZERO */
	}
}

/* Get event type name string for logging */
static const char *get_event_type_name(enum timer_event_type event_type)
{
	switch (event_type) {
	case TIMER_EVENT_ZERO:
		return "ZERO_EVENT";
	case TIMER_EVENT_LOAD:
		return "LOAD_EVENT";
	case TIMER_EVENT_CC0_DN:
		return "CC0_DN_EVENT";
	case TIMER_EVENT_CC0_UP:
		return "CC0_UP_EVENT";
	case TIMER_EVENT_CC1_DN:
		return "CC1_DN_EVENT";
	case TIMER_EVENT_CC1_UP:
		return "CC1_UP_EVENT";
	default:
		return "UNKNOWN";
	}
}

/**
 * @brief Configure timer to publish events for ADC triggering
 *
 * Enables the specified timer event type and configures it to publish
 * on event channel 1 for ADC hardware triggering.
 *
 * @param counter_dev Counter device pointer
 * @param event_type Timer event type to publish
 * @return 0 on success, negative error code on failure
 */
int configure_timer_event_publishing(const struct device *counter_dev,
				     enum timer_event_type event_type)
{
	if (!device_is_ready(counter_dev)) {
		return -ENODEV;
	}

	/* Map enum to driverlib event mask */
	uint32_t event_mask = map_event_type(event_type);

	/* Configure timer to publish specified event on event route 1 */
	DL_TimerA_enableEvent(TIMG8_0, DL_TIMERA_EVENT_ROUTE_1, event_mask);

	/* Set publisher channel ID to 1 (must match ADC subscriber channel) */
	DL_TimerA_setPublisherChanID(TIMG8_0, DL_TIMERA_PUBLISHER_INDEX_0,
				     ADC_TRIGGER_EVENT_CHANNEL);

	printk("Timer event publishing configured: %s\n", get_event_type_name(event_type));

	return 0;
}

/**
 * @brief Start timer for periodic ADC triggering
 *
 * Starts the timer in periodic mode at the specified frequency.
 * Timer event publishing must be configured first.
 *
 * @param counter_dev Counter device pointer
 * @param frequency_hz Trigger frequency in Hz
 * @return 0 on success, negative error code on failure
 */
int start_timer_for_adc_trigger(const struct device *counter_dev, uint32_t frequency_hz)
{
	if (!device_is_ready(counter_dev)) {
		return -ENODEV;
	}

	/* Get timer frequency from counter API */
	uint32_t timer_freq = counter_get_frequency(counter_dev);

	/* Calculate period for desired frequency */
	uint32_t period_ticks = timer_freq / frequency_hz;

	/* Start counter first */
	int err = counter_start(counter_dev);
	if (err < 0) {
		return err;
	}

	/* Get current counter value */
	uint32_t current_ticks = 0;
	err = counter_get_value(counter_dev, &current_ticks);
	if (err < 0) {
		return err;
	}

	/* Calculate alarm tick (current + period) for immediate first trigger */
	uint32_t alarm_ticks = current_ticks + period_ticks;

	/* Configure counter alarm for periodic triggering */
	struct counter_alarm_cfg alarm_cfg = {
		.flags = COUNTER_ALARM_CFG_ABSOLUTE,
		.ticks = alarm_ticks,
		.callback = NULL,  /* No callback needed, hardware trigger only */
		.user_data = NULL,
	};

	/* Set alarm to generate events */
	err = counter_set_channel_alarm(counter_dev, 0, &alarm_cfg);
	if (err < 0) {
		return err;
	}

	return 0;
}
