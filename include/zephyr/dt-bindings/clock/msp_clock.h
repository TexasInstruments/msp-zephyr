/*
 * Copyright 2025 Texas Instruments Inc.
 * Copyright 2025 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_MSP_CLOCK_H
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_MSP_CLOCK_H

#define MSP_CLOCK(clk, bit) ((clk << 8) | bit)

/* Peripheral clock source selection register mask */
#define MSP_CLOCK_PERIPH_REG_MASK(X) (X & 0xFF)

/* Clock references - common for both M0 and M33 */
#define MSP_CLOCK_SYSOSC  MSP_CLOCK(0x0, 0x0)
#define MSP_CLOCK_LFCLK   MSP_CLOCK(0x1, 0x2)
#define MSP_CLOCK_MFCLK   MSP_CLOCK(0x2, 0x4)
#define MSP_CLOCK_BUSCLK  MSP_CLOCK(0x3, 0x8)
#define MSP_CLOCK_ULPCLK  MSP_CLOCK(0x4, 0x8)
#define MSP_CLOCK_MCLK    MSP_CLOCK(0x5, 0x8)
#define MSP_CLOCK_MFPCLK  MSP_CLOCK(0x6, 0x0)
#define MSP_CLOCK_CANCLK  MSP_CLOCK(0x7, 0x0)
#define MSP_CLOCK_CLK_OUT MSP_CLOCK(0x8, 0x0)
#define MSP_CLOCK_MCLKBY2 MSP_CLOCK(0x9, 0x8)

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_MSP_CLOCK_H */
