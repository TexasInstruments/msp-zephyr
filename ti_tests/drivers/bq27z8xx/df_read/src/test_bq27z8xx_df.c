/*
 * Copyright (c) 2026, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include "bq27z8xx_common.h"

LOG_MODULE_REGISTER(test_bq27z8xx_df, LOG_LEVEL_DBG);

/* Maximum payload bytes returned by one data flash MAC read */
#define DF_MAX_PAYLOAD 32

struct bq27z8xx_df_fixture {
	const struct device *dev;
};

/* ---------------------------------------------------------------------------
 * Data flash read test cases
 *
 * To test a new address, append a row to df_read_tcs.  Each entry has:
 *   .addr     — 16-bit DF MAC address (must be in BQ27Z8XX_DF_ADDR_MIN..MAX)
 *   .exp      — expected payload bytes (verified under CONFIG_EMUL only)
 *   .exp_len  — expected reported payload length (verified under CONFIG_EMUL)
 *
 * The emulator returns all-zero payloads with length DF_MAX_PAYLOAD for every
 * valid DF address, so every emulated entry uses exp[] = {0} and
 * exp_len = DF_MAX_PAYLOAD.  On real hardware, only ret == 0 is checked.
 * ---------------------------------------------------------------------------
 */
struct df_read_tc {
	uint16_t addr;
	uint16_t  exp[DF_MAX_PAYLOAD];
	uint8_t  exp_len;
};

static const struct df_read_tc df_read_tcs[] = {
	{ .addr = 0x4000, .exp = {0}, .exp_len = DF_MAX_PAYLOAD },
	{ .addr = 0x4004, .exp = {0}, .exp_len = DF_MAX_PAYLOAD },
	{ .addr = 0x5282, .exp = {0}, .exp_len = DF_MAX_PAYLOAD },
	{ .addr = 0x4288, .exp = {0}, .exp_len = DF_MAX_PAYLOAD },
};

static void *bq27z8xx_df_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_df_fixture fixture;

	fixture.dev = DEVICE_DT_GET_ANY(ti_bq27z855);

	zassert_true(device_is_ready(fixture.dev), "BQ27Z8XX device not ready");

	return &fixture;
}

/*
 * Iterates over df_read_tcs.  Every read must return 0.  Under CONFIG_EMUL
 * the reported payload length and every payload byte are also checked against
 * the table entry.
 */
ZTEST_F(bq27z8xx_df, test_df_read_valid_addresses)
{
	for (int i = 0; i < ARRAY_SIZE(df_read_tcs); i++) {
		const struct df_read_tc *tc = &df_read_tcs[i];
		/* buf[0] = reported payload length; buf[1..DF_MAX_PAYLOAD] = payload */
		uint8_t buf[1 + DF_MAX_PAYLOAD];
		int ret;

		ret = bq27z8xx_df_read(fixture->dev, tc->addr, buf, DF_MAX_PAYLOAD);
		zassert_ok(ret, "df_read(0x%04x) failed: %d", tc->addr, ret);

#if defined(CONFIG_EMUL)
		zassert_equal(buf[0], tc->exp_len,
			      "df_read(0x%04x): reported length %u, expected %u",
			      tc->addr, buf[0], tc->exp_len);
		zassert_mem_equal(&buf[1], tc->exp, tc->exp_len,
				  "df_read(0x%04x): payload mismatch", tc->addr);
#endif
	}
}

/*
 * Walk every 32-byte block across the full data flash address range
 * (BQ27Z8XX_DF_ADDR_MIN..BQ27Z8XX_DF_ADDR_MAX).
 *
 * bq27z8xx_df_read() is called once to seek to the first block; all
 * subsequent blocks are fetched with bq27z8xx_df_read_next(), which uses
 * the device's auto-increment register (0x44) and requires no per-block
 * sleep.  This keeps the test well within the default twister timeout.
 *
 * Only the return code is checked; payload contents are not verified.
 */
ZTEST_F(bq27z8xx_df, test_df_read_full_sweep)
{
	uint8_t buf[1 + DF_MAX_PAYLOAD];
	int ret;

	ret = bq27z8xx_df_read(fixture->dev, BQ27Z8XX_DF_ADDR_MIN, buf, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_read(0x%04x) failed: %d", BQ27Z8XX_DF_ADDR_MIN, ret);

	for (uint16_t addr = BQ27Z8XX_DF_ADDR_MIN + DF_MAX_PAYLOAD;
	     addr <= BQ27Z8XX_DF_ADDR_MAX;
	     addr += DF_MAX_PAYLOAD) {
		ret = bq27z8xx_df_read_next(fixture->dev, buf, DF_MAX_PAYLOAD);
		zassert_ok(ret, "df_read_next(0x%04x) failed: %d", addr, ret);
	}
}

ZTEST_SUITE(bq27z8xx_df, NULL, bq27z8xx_df_setup, NULL, NULL, NULL);
