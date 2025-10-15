/*
 * Copyright (c) 2025 Texas Instruments Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MSP_CLOCK_CONTROL
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MSP_CLOCK_CONTROL

#include <stdint.h>
#include <zephyr/dt-bindings/clock/msp_clock.h>

struct msp_sys_clock {
	uint32_t clk;
};

#define MSP_CLOCK_SUBSYS_FN(index) {.clk = DT_INST_CLOCKS_CELL(index, clk)}

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MSP_CLOCK_CONTROL */
