/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIMER_EVENT_CONFIG_H_
#define TIMER_EVENT_CONFIG_H_

#include <zephyr/device.h>
#include <stdint.h>

/* Timer event types for ADC triggering via event fabric */
enum timer_event_type {
	TIMER_EVENT_ZERO = 0,   /**< Counter reaches zero (most common) */
	TIMER_EVENT_LOAD = 1,   /**< Counter loaded with initial value */
	TIMER_EVENT_CC0_DN = 2, /**< Capture/Compare 0 match (down counting) */
	TIMER_EVENT_CC0_UP = 3, /**< Capture/Compare 0 match (up counting) */
	TIMER_EVENT_CC1_DN = 4, /**< Capture/Compare 1 match (down counting) */
	TIMER_EVENT_CC1_UP = 5, /**< Capture/Compare 1 match (up counting) */
};

/** Configure timer to publish events for ADC triggering */
int configure_timer_event_publishing(const struct device *counter_dev,
				     enum timer_event_type event_type);

/** Start timer for periodic ADC triggering */
int start_timer_for_adc_trigger(const struct device *counter_dev, uint32_t frequency_hz);

#endif /* TIMER_EVENT_CONFIG_H_ */
