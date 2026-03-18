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

LOG_MODULE_REGISTER(test_bq27z8xx_mac_api, LOG_LEVEL_DBG);

struct bq27z8xx_mac_api_fixture {
	const struct device *dev;
};

static void *bq27z8xx_mac_api_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_mac_api_fixture fixture;

	fixture.dev = DEVICE_DT_GET_ANY(ti_bq27z855);

	zassert_true(device_is_ready(fixture.dev), "BQ27Z8XX device not ready");

	return &fixture;
}

/*
 * Passing more than 32 payload bytes to bq27z8xx_write_mac() must return
 * -EINVAL immediately, before any I2C transaction is attempted.
 */
ZTEST_F(bq27z8xx_mac_api, test_write_mac_overlength_data_rejected)
{
	static const uint8_t data[33]; /* 33 bytes — one over the 32-byte hard limit */

	zassert_equal(bq27z8xx_write_mac(fixture->dev, 0x0001, data, sizeof(data)),
		      -EINVAL,
		      "write_mac(len=33) should be -EINVAL");
}

/*
 * bq27z8xx_read_mac() with a known MAC command must complete successfully and
 * return the expected payload.  Uses BQ27Z8XX_MAC_CMD_DEVICETYPE (0x0001),
 * which the bq27z855 emulator responds to with 0x1855 in little-endian order.
 *
 * After the call:
 *   buf[0] = payload length (2)
 *   buf[1] = 0x55  (low byte of 0x1855)
 *   buf[2] = 0x18  (high byte of 0x1855)
 */
ZTEST_F(bq27z8xx_mac_api, test_read_mac_known_command_completes)
{
	/* [0] = payload length, [1..2] = device type in LE */
	uint8_t buf[3] = {0};
	int ret;

	ret = bq27z8xx_read_mac(fixture->dev, BQ27Z8XX_MAC_CMD_DEVICETYPE, buf, 2, K_NO_WAIT);
	zassert_ok(ret, "read_mac(DEVICETYPE) failed: %d", ret);

	/* Emulator returns BQ27Z8XX_DEVICE_TYPE_BQ27Z855 = 0x1855 in LE */
	zassert_equal(buf[1], 0x55,
		      "DEVICETYPE byte 0: expected 0x55, got 0x%02x", buf[1]);
	zassert_equal(buf[2], 0x18,
		      "DEVICETYPE byte 1: expected 0x18, got 0x%02x", buf[2]);
}


ZTEST_SUITE(bq27z8xx_mac_api, NULL, bq27z8xx_mac_api_setup, NULL, NULL, NULL);
