/*
 * Copyright (c) 2025 Texas Instruments Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MSPM0_CLOCK_CONTROL
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MSPM0_CLOCK_CONTROL

#include <zephyr/dt-bindings/clock/mspm0_clock.h>

struct mspm0_sys_clock {
	uint32_t clk;
};

#define MSPM0_CLOCK_SUBSYS_FN(index) {.clk = DT_INST_CLOCKS_CELL(index, clk)}

/* function disabling syspll and returning MCLK to SYSOSC (if so configured) */
void mspm0_switch_and_disable_syspll(void);

void mspm0_enable_syspll(void);

/* function implementing the workaround to add SYSPLL and verify that the
 * frequency lock has been correctly achieved.
 * Also switches MCLK to SYSPLL if configured that way in the devicetree
 */
void mspm0_verify_syspll(void);

void mspm0_switch_to_syspll(void);

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MSPM0_CLOCK_CONTROL */
