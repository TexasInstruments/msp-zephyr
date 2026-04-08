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

LOG_MODULE_REGISTER(test_bq27z8xx_df_write, LOG_LEVEL_DBG);

/* Maximum payload bytes in one data flash MAC write/read */
#define DF_MAX_PAYLOAD 32

struct bq27z8xx_df_write_fixture {
	const struct device *dev;
};

/* ---------------------------------------------------------------------------
 * Data flash write test cases
 *
 * Each entry has:
 *   .addr     — 16-bit DF MAC address (must be in BQ27Z8XX_DF_ADDR_MIN..MAX)
 *   .data     — bytes to write
 *   .data_len — number of bytes to write
 * ---------------------------------------------------------------------------
 */
struct df_write_tc {
	uint16_t addr;
	uint8_t  data[DF_MAX_PAYLOAD];
	uint8_t  data_len;
};

static const struct df_write_tc df_write_tcs[] = {
	{ .addr = 0x4000, .data = {0xAA, 0xBB, 0xCC, 0xDD}, .data_len = 4 },
	{ .addr = 0x4020, .data = {0x01, 0x02, 0x03},        .data_len = 3 },
	{ .addr = 0x5000, .data = {0xFF, 0xFE, 0xFD},        .data_len = 3 },
};

static void *bq27z8xx_df_write_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_df_write_fixture fixture;

	fixture.dev = DEVICE_DT_GET_ANY(ti_bq27z855);

	zassert_true(device_is_ready(fixture.dev), "BQ27Z8XX device not ready");

	return &fixture;
}

/*
 * Addresses outside BQ27Z8XX_DF_ADDR_MIN..BQ27Z8XX_DF_ADDR_MAX must be
 * rejected with -EINVAL before any I2C transaction is attempted.
 */
ZTEST_F(bq27z8xx_df_write, test_df_write_invalid_address_rejected)
{
	static const uint8_t dummy[4] = {0x01, 0x02, 0x03, 0x04};

	zassert_equal(bq27z8xx_df_write(fixture->dev, 0x0001, dummy, sizeof(dummy)),
		      -EINVAL, "df_write(0x0001) should be -EINVAL");
	zassert_equal(bq27z8xx_df_write(fixture->dev, 0x3FFF, dummy, sizeof(dummy)),
		      -EINVAL, "df_write(0x3FFF) should be -EINVAL");
	zassert_equal(bq27z8xx_df_write(fixture->dev, 0x6000, dummy, sizeof(dummy)),
		      -EINVAL, "df_write(0x6000) should be -EINVAL");
}

/*
 * Iterates over df_write_tcs.  Every write must return 0.  Under CONFIG_EMUL
 * a read-back is performed to verify the written data is echoed correctly.
 */
ZTEST_F(bq27z8xx_df_write, test_df_write_valid_addresses)
{
	for (int i = 0; i < ARRAY_SIZE(df_write_tcs); i++) {
		const struct df_write_tc *tc = &df_write_tcs[i];
		int ret;

		ret = bq27z8xx_df_write(fixture->dev, tc->addr, tc->data, tc->data_len);
		zassert_ok(ret, "df_write(0x%04x) failed: %d", tc->addr, ret);

		/* buf[0] = reported payload length; buf[1..DF_MAX_PAYLOAD] = payload */
		uint8_t buf[1 + DF_MAX_PAYLOAD];

		ret = bq27z8xx_df_read(fixture->dev, tc->addr, buf, DF_MAX_PAYLOAD);
		zassert_ok(ret, "df_read(0x%04x) after write failed: %d", tc->addr, ret);
		zassert_mem_equal(&buf[1], tc->data, tc->data_len,
				  "df_write/read(0x%04x): payload mismatch", tc->addr);
	}
}

/*
 * Write 0 bytes to a valid address.  Under CONFIG_EMUL the read-back must
 * return all-zero payload bytes (no data was committed to the write-back cache).
 */
ZTEST_F(bq27z8xx_df_write, test_df_write_zero_length)
{
	int ret;

	ret = bq27z8xx_df_write(fixture->dev, 0x4000, NULL, 0);
	zassert_ok(ret, "df_write(0x4000, len=0) failed: %d", ret);

#if defined(CONFIG_EMUL)
	uint8_t buf[1 + DF_MAX_PAYLOAD];
	static const uint8_t zeros[DF_MAX_PAYLOAD];

	ret = bq27z8xx_df_read(fixture->dev, 0x4000, buf, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_read(0x4000) after zero-length write failed: %d", ret);
	zassert_mem_equal(&buf[1], zeros, DF_MAX_PAYLOAD,
			  "df_read(0x4000) after zero-length write: expected all zeros");
#endif
}

/*
 * Write exactly DF_MAX_PAYLOAD bytes (32) to a valid address and verify the
 * full payload is echoed back by a subsequent read.
 */
ZTEST_F(bq27z8xx_df_write, test_df_write_max_length)
{
	uint8_t pattern[DF_MAX_PAYLOAD];
	int ret;

	for (int i = 0; i < DF_MAX_PAYLOAD; i++) {
		pattern[i] = (uint8_t)i; /* 0x00..0x1F */
	}

	ret = bq27z8xx_df_write(fixture->dev, 0x4100, pattern, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_write(0x4100, len=32) failed: %d", ret);

	uint8_t buf[1 + DF_MAX_PAYLOAD];

	ret = bq27z8xx_df_read(fixture->dev, 0x4100, buf, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_read(0x4100) after max-length write failed: %d", ret);
	zassert_mem_equal(&buf[1], pattern, DF_MAX_PAYLOAD,
			  "df_write/read(0x4100): max-length payload mismatch");
}

/*
 * Write data only to the SECOND 32-byte block (0x4220).
 * df_read(0x4200) → first block is unwritten → zeros.
 * df_read_next()  → auto-increments to 0x4200+32 = 0x4220 → written data.
 *
 * This exercises the auto-increment addressing without requiring a multi-slot
 * write-back cache in the emulator.
 */
ZTEST_F(bq27z8xx_df_write, test_df_read_next_sequential)
{
	static const uint8_t block1[4] = {0xAA, 0xBB, 0xCC, 0xDD};
	int ret;

	/* Write only to the second block; first block (0x4200) is unwritten */
	ret = bq27z8xx_df_write(fixture->dev, 0x4220, block1, sizeof(block1));
	zassert_ok(ret, "df_write(0x4220) failed: %d", ret);

#if defined(CONFIG_EMUL)
	uint8_t buf[1 + DF_MAX_PAYLOAD];
	static const uint8_t zeros[DF_MAX_PAYLOAD];

	/* First block read — cache holds 0x4220, so 0x4200 returns zeros */
	ret = bq27z8xx_df_read(fixture->dev, 0x4200, buf, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_read(0x4200) failed: %d", ret);
	zassert_mem_equal(&buf[1], zeros, DF_MAX_PAYLOAD,
			  "df_read(0x4200): expected zeros");

	/* Read next (auto-increment to 0x4220) — should return written data */
	ret = bq27z8xx_df_read_next(fixture->dev, buf, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_read_next() after 0x4200 failed: %d", ret);
	zassert_mem_equal(&buf[1], block1, sizeof(block1),
			  "df_read_next() after 0x4200: payload mismatch");
#endif
}

/*
 * Writing more than DF_MAX_PAYLOAD bytes must return -EINVAL before any I2C
 * transaction is attempted.  Verifies the 32-byte hard limit in bq27z8xx_df_write().
 */
ZTEST_F(bq27z8xx_df_write, test_df_write_overlength_data_rejected)
{
	static const uint8_t data[DF_MAX_PAYLOAD + 1]; /* 33 bytes — one over the limit */

	zassert_equal(bq27z8xx_df_write(fixture->dev, 0x4000, data, sizeof(data)),
		      -EINVAL,
		      "df_write(len=33) should be -EINVAL");
}

/*
 * The emulator maintains a single-slot DF write-back cache: writing to a
 * second address evicts the first.  After eviction, reading the first address
 * must return all-zero payload bytes (not the previously written data).
 */
ZTEST_F(bq27z8xx_df_write, test_df_write_cache_eviction)
{
	static const uint8_t aa = 0xAA;
	static const uint8_t bb = 0xBB;
	int ret;

	/* Populate 0x4000 in the write-back cache */
	ret = bq27z8xx_df_write(fixture->dev, 0x4000, &aa, 1);
	zassert_ok(ret, "df_write(0x4000) failed: %d", ret);

	/* Write to a different address — evicts 0x4000 from the single-slot cache */
	ret = bq27z8xx_df_write(fixture->dev, 0x4020, &bb, 1);
	zassert_ok(ret, "df_write(0x4020) failed: %d", ret);

#if defined(CONFIG_EMUL)
	/* Read 0x4000 — cache was evicted; emulator must return zeros */
	uint8_t buf[1 + DF_MAX_PAYLOAD];
	static const uint8_t zeros[DF_MAX_PAYLOAD];

	ret = bq27z8xx_df_read(fixture->dev, 0x4000, buf, DF_MAX_PAYLOAD);
	zassert_ok(ret, "df_read(0x4000) after cache eviction failed: %d", ret);
	zassert_mem_equal(&buf[1], zeros, DF_MAX_PAYLOAD,
			  "df_read(0x4000): expected zeros after cache eviction");
#endif
}

ZTEST_SUITE(bq27z8xx_df_write, NULL, bq27z8xx_df_write_setup, NULL, NULL, NULL);
