/*
 * Copyright 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @ingroup charger_interface
 * @brief BQ25628 Charger Includes
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CHARGER_BQ25628_H_
#define ZEPHYR_INCLUDE_DRIVERS_CHARGER_BQ25628_H_

/**
 * @brief Interfaces for battery chargers.
 * @defgroup charger_interface Battery Charger
 * @ingroup io_interfaces
 * @{
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/charger.h>

/**
 * @brief Runtime Dynamic Battery Parameters
 */
enum bq25628_custom_charger_property {
	CHARGER_PROP_WATCHDOG_TIMER = CHARGER_PROP_CUSTOM_BEGIN,
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#endif