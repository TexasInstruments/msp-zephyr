/*
 * Copyright (c) 2024 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * I2C driver test using TLV320AIC3254 audio codec on CC3200AUDBOOST.
 * Codec I2C address: 0x18
 * Exercises: init, configure, get_config, transfer (TX/RX), error paths.
 *
 * Tests that require the codec to respond will be skipped if the
 * codec is not detected during suite setup.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/i2c.h>

#define I2C_DEV_NODE DT_ALIAS(i2c_0)

/* TLV320AIC3254 register definitions */
#define CODEC_ADDR          0x18
#define CODEC_PAGE_SEL_REG  0x00
#define CODEC_SW_RESET_REG  0x01
#define CODEC_CLK_MUX_REG   0x04

/* Non-existent I2C address for NACK testing */
#define INVALID_ADDR        0x7E

static const struct device *dev_i2c;
static bool codec_present;

/* ------------------------------------------------------------------ */
/* Helper: probe codec by attempting a single-byte read               */
/* ------------------------------------------------------------------ */
static bool probe_codec(const struct device *dev)
{
	uint8_t dummy;
	int ret;

	ret = i2c_read(dev, &dummy, 1, CODEC_ADDR);
	return (ret == 0);
}

/* ------------------------------------------------------------------ */
/* Suite setup / before                                               */
/* ------------------------------------------------------------------ */
static void *suite_setup(void)
{
	dev_i2c = DEVICE_DT_GET(I2C_DEV_NODE);

	TC_PRINT("Test executed on %s\n", CONFIG_BOARD_TARGET);
	TC_PRINT("I2C codec test: TLV320AIC3254 @ 0x%02x\n", CODEC_ADDR);
	TC_PRINT("============================================\n");

	/* Allow codec time to power up before probing */
	k_msleep(100);

	/* Detect whether the codec is present on the bus */
	codec_present = probe_codec(dev_i2c);
	TC_PRINT("Codec detected: %s\n", codec_present ? "yes" : "no");

	return NULL;
}

/* ------------------------------------------------------------------ */
/* Tests that do NOT require codec hardware                           */
/* ------------------------------------------------------------------ */

ZTEST(i2c_codec, test_i2c_device_ready)
{
	zassert_true(device_is_ready(dev_i2c), "I2C device is not ready");
}

ZTEST(i2c_codec, test_i2c_configure_standard)
{
	int ret;
	uint32_t cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

	ret = i2c_configure(dev_i2c, cfg);
	zassert_equal(ret, 0, "i2c_configure(STANDARD) failed: %d", ret);
}

ZTEST(i2c_codec, test_i2c_configure_fast)
{
	int ret;
	uint32_t cfg = I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER;

	ret = i2c_configure(dev_i2c, cfg);
	zassert_equal(ret, 0, "i2c_configure(FAST) failed: %d", ret);
}

ZTEST(i2c_codec, test_i2c_get_config)
{
	int ret;
	uint32_t cfg_set = I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER;
	uint32_t cfg_get;

	ret = i2c_configure(dev_i2c, cfg_set);
	zassert_equal(ret, 0, "i2c_configure() failed: %d", ret);

	ret = i2c_get_config(dev_i2c, &cfg_get);
	zassert_equal(ret, 0, "i2c_get_config() failed: %d", ret);
	zassert_equal(cfg_set, cfg_get,
		      "get_config mismatch: set=0x%x got=0x%x", cfg_set, cfg_get);
}

ZTEST(i2c_codec, test_i2c_configure_10bit_rejected)
{
	int ret;
	uint32_t cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER |
		       I2C_MSG_ADDR_10_BITS;

	ret = i2c_configure(dev_i2c, cfg);
	zassert_equal(ret, -EINVAL,
		      "i2c_configure(10-bit) should return -EINVAL, got %d", ret);
}

ZTEST(i2c_codec, test_i2c_configure_high_speed_rejected)
{
	int ret;
	uint32_t cfg = I2C_SPEED_SET(I2C_SPEED_HIGH) | I2C_MODE_CONTROLLER;

	ret = i2c_configure(dev_i2c, cfg);
	zassert_equal(ret, -EINVAL,
		      "i2c_configure(HIGH) should return -EINVAL, got %d", ret);
}

ZTEST(i2c_codec, test_i2c_nack_write_invalid_address)
{
	int ret;
	uint8_t buf[1] = { 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, buf, sizeof(buf), INVALID_ADDR);
	zassert_not_equal(ret, 0,
			  "i2c_write to invalid addr should fail, got %d", ret);

	TC_PRINT("NACK on write to 0x%02x returned: %d\n", INVALID_ADDR, ret);
}

ZTEST(i2c_codec, test_i2c_nack_read_invalid_address)
{
	int ret;
	uint8_t buf[1];

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_read(dev_i2c, buf, sizeof(buf), INVALID_ADDR);
	zassert_not_equal(ret, 0,
			  "i2c_read from invalid addr should fail, got %d", ret);

	TC_PRINT("NACK on read from 0x%02x returned: %d\n", INVALID_ADDR, ret);
}

ZTEST(i2c_codec, test_i2c_nack_write_read_invalid_address)
{
	int ret;
	uint8_t reg = 0x00;
	uint8_t val;

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write_read(dev_i2c, INVALID_ADDR, &reg, 1, &val, 1);
	zassert_not_equal(ret, 0,
			  "i2c_write_read to invalid addr should fail, got %d", ret);

	TC_PRINT("NACK on write_read to 0x%02x returned: %d\n", INVALID_ADDR, ret);
}

ZTEST(i2c_codec, test_i2c_reconfigure_after_nack)
{
	int ret;
	uint8_t buf[1] = { 0x00 };
	uint32_t cfg;

	/* Configure and trigger a NACK */
	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, buf, sizeof(buf), INVALID_ADDR);
	zassert_not_equal(ret, 0, "expected NACK");

	/* Verify the driver is still usable after NACK */
	ret = i2c_get_config(dev_i2c, &cfg);
	zassert_equal(ret, 0, "get_config after NACK failed: %d", ret);

	ret = i2c_configure(dev_i2c,
			     I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER);
	zassert_equal(ret, 0, "reconfigure after NACK failed: %d", ret);

	/* Trigger another NACK to confirm recovery */
	ret = i2c_read(dev_i2c, buf, sizeof(buf), INVALID_ADDR);
	zassert_not_equal(ret, 0, "expected NACK on second attempt");

	TC_PRINT("Driver recovered after NACK\n");
}

ZTEST(i2c_codec, test_i2c_configure_speed_switch)
{
	int ret;
	uint32_t cfg_get;

	/* Standard → Fast → Standard */
	ret = i2c_configure(dev_i2c,
			     I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER);
	zassert_equal(ret, 0, "STANDARD failed: %d", ret);

	ret = i2c_get_config(dev_i2c, &cfg_get);
	zassert_equal(ret, 0, "get_config failed: %d", ret);
	zassert_equal(I2C_SPEED_GET(cfg_get), I2C_SPEED_STANDARD,
		      "expected STANDARD speed");

	ret = i2c_configure(dev_i2c,
			     I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);
	zassert_equal(ret, 0, "FAST failed: %d", ret);

	ret = i2c_get_config(dev_i2c, &cfg_get);
	zassert_equal(ret, 0, "get_config failed: %d", ret);
	zassert_equal(I2C_SPEED_GET(cfg_get), I2C_SPEED_FAST,
		      "expected FAST speed");

	ret = i2c_configure(dev_i2c,
			     I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER);
	zassert_equal(ret, 0, "back to STANDARD failed: %d", ret);
}

/* ------------------------------------------------------------------ */
/* Tests that REQUIRE the codec to respond (skipped if absent)        */
/* ------------------------------------------------------------------ */

ZTEST(i2c_codec, test_i2c_write_page_select)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	uint8_t buf[2] = { CODEC_PAGE_SEL_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, buf, sizeof(buf), CODEC_ADDR);
	zassert_equal(ret, 0, "i2c_write(page select) failed: %d", ret);
}

ZTEST(i2c_codec, test_i2c_write_read_register)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	uint8_t reg = CODEC_CLK_MUX_REG;
	uint8_t val = 0xFF;
	uint8_t page_sel[2] = { CODEC_PAGE_SEL_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, page_sel, sizeof(page_sel), CODEC_ADDR);
	zassert_equal(ret, 0, "page select failed: %d", ret);

	ret = i2c_write_read(dev_i2c, CODEC_ADDR, &reg, 1, &val, 1);
	zassert_equal(ret, 0, "i2c_write_read() failed: %d", ret);

	TC_PRINT("CLK_MUX_REG (0x%02x) = 0x%02x\n", reg, val);
}

ZTEST(i2c_codec, test_i2c_burst_read)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	uint8_t start_reg = 0x00;
	uint8_t data[4];
	uint8_t page_sel[2] = { CODEC_PAGE_SEL_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, page_sel, sizeof(page_sel), CODEC_ADDR);
	zassert_equal(ret, 0, "page select failed: %d", ret);

	ret = i2c_burst_read(dev_i2c, CODEC_ADDR, start_reg, data, sizeof(data));
	zassert_equal(ret, 0, "i2c_burst_read() failed: %d", ret);

	TC_PRINT("Burst read [0x00..0x03]: 0x%02x 0x%02x 0x%02x 0x%02x\n",
		 data[0], data[1], data[2], data[3]);
}

ZTEST(i2c_codec, test_i2c_codec_sw_reset)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	uint8_t val;
	uint8_t page_sel[2] = { CODEC_PAGE_SEL_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, page_sel, sizeof(page_sel), CODEC_ADDR);
	zassert_equal(ret, 0, "page select failed: %d", ret);

	uint8_t reset_cmd[2] = { CODEC_SW_RESET_REG, 0x01 };

	ret = i2c_write(dev_i2c, reset_cmd, sizeof(reset_cmd), CODEC_ADDR);
	zassert_equal(ret, 0, "sw reset write failed: %d", ret);

	k_msleep(10);

	uint8_t reg = CODEC_PAGE_SEL_REG;

	ret = i2c_write_read(dev_i2c, CODEC_ADDR, &reg, 1, &val, 1);
	zassert_equal(ret, 0, "read after reset failed: %d", ret);
	zassert_equal(val, 0x00,
		      "page reg should be 0x00 after reset, got 0x%02x", val);
}

ZTEST(i2c_codec, test_i2c_page_switch)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	uint8_t val;
	uint8_t reg = CODEC_PAGE_SEL_REG;

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	uint8_t page1[2] = { CODEC_PAGE_SEL_REG, 0x01 };

	ret = i2c_write(dev_i2c, page1, sizeof(page1), CODEC_ADDR);
	zassert_equal(ret, 0, "switch to page 1 failed: %d", ret);

	ret = i2c_write_read(dev_i2c, CODEC_ADDR, &reg, 1, &val, 1);
	zassert_equal(ret, 0, "read page reg failed: %d", ret);
	zassert_equal(val, 0x01, "page should be 1, got 0x%02x", val);

	uint8_t page0[2] = { CODEC_PAGE_SEL_REG, 0x00 };

	ret = i2c_write(dev_i2c, page0, sizeof(page0), CODEC_ADDR);
	zassert_equal(ret, 0, "switch to page 0 failed: %d", ret);

	ret = i2c_write_read(dev_i2c, CODEC_ADDR, &reg, 1, &val, 1);
	zassert_equal(ret, 0, "read page reg failed: %d", ret);
	zassert_equal(val, 0x00, "page should be 0, got 0x%02x", val);
}

ZTEST(i2c_codec, test_i2c_speed_reconfigure_with_codec)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	uint8_t reg = CODEC_PAGE_SEL_REG;
	uint8_t val;

	ret = i2c_configure(dev_i2c,
			     I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER);
	zassert_equal(ret, 0, "configure STANDARD failed: %d", ret);

	ret = i2c_write_read(dev_i2c, CODEC_ADDR, &reg, 1, &val, 1);
	zassert_equal(ret, 0, "read at STANDARD speed failed: %d", ret);

	ret = i2c_configure(dev_i2c,
			     I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);
	zassert_equal(ret, 0, "configure FAST failed: %d", ret);

	ret = i2c_write_read(dev_i2c, CODEC_ADDR, &reg, 1, &val, 1);
	zassert_equal(ret, 0, "read at FAST speed failed: %d", ret);
}

/* ------------------------------------------------------------------ */
/* Tests for TXFIFO ISR and merge-buffer paths                        */
/* ------------------------------------------------------------------ */

ZTEST(i2c_codec, test_i2c_long_write_fifo_trigger)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;

	/*
	 * Write 32 bytes: first byte is register address (page-select = 0),
	 * remaining bytes are data for regs 0x01..0x1E.
	 * The TX FIFO will be filled initially; the remaining bytes
	 * trigger the TXFIFO_TRIGGER ISR path in the driver.
	 */
	uint8_t buf[32] = { CODEC_PAGE_SEL_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	ret = i2c_write(dev_i2c, buf, sizeof(buf), CODEC_ADDR);
	zassert_equal(ret, 0, "long write (32 bytes) failed: %d", ret);

	TC_PRINT("Long write of %zu bytes succeeded (TXFIFO ISR exercised)\n",
		 sizeof(buf));

	/* Software-reset the codec to restore default register state */
	uint8_t reset_cmd[2] = { CODEC_SW_RESET_REG, 0x01 };

	ret = i2c_write(dev_i2c, reset_cmd, sizeof(reset_cmd), CODEC_ADDR);
	zassert_equal(ret, 0, "sw reset after long write failed: %d", ret);
	k_msleep(10);
}

ZTEST(i2c_codec, test_i2c_transfer_merge_write)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	struct i2c_msg msgs[2];

	/*
	 * Two consecutive WRITE messages without STOP/RESTART between them.
	 * The driver should merge them into a single I2C bus transfer using
	 * the internal merge buffer.
	 *
	 * msg[0]: page-select = 0  (WRITE, no STOP)  → merged
	 * msg[1]: clk-mux reg = 0  (WRITE, STOP)     → flush merged buffer
	 */
	uint8_t buf1[2] = { CODEC_PAGE_SEL_REG, 0x00 };
	uint8_t buf2[2] = { CODEC_CLK_MUX_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	msgs[0].buf = buf1;
	msgs[0].len = sizeof(buf1);
	msgs[0].flags = I2C_MSG_WRITE; /* no STOP → driver will merge */

	msgs[1].buf = buf2;
	msgs[1].len = sizeof(buf2);
	msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	ret = i2c_transfer(dev_i2c, msgs, 2, CODEC_ADDR);
	zassert_equal(ret, 0, "merge-write transfer failed: %d", ret);

	TC_PRINT("Merge-write transfer (2 msgs → 1 bus xfer) succeeded\n");

	/* Reset codec to clean state */
	uint8_t reset_cmd[2] = { CODEC_SW_RESET_REG, 0x01 };

	i2c_write(dev_i2c, reset_cmd, sizeof(reset_cmd), CODEC_ADDR);
	k_msleep(10);
}

ZTEST(i2c_codec, test_i2c_transfer_merge_read)
{
	if (!codec_present) {
		ztest_test_skip();
	}

	int ret;
	struct i2c_msg msgs[3];

	/*
	 * Exercise the merge-read and no-STOP transmit paths:
	 *
	 * msg[0]: WRITE register address (no STOP) → exercises no-STOP TX path
	 * msg[1]: READ 2 bytes (no STOP)           → merged with msg[2]
	 * msg[2]: READ 2 bytes (STOP)              → flush merged read + copy-back
	 *
	 * After the merged receive, the driver copies data from the internal
	 * merge buffer back into the individual msg buffers.
	 */
	uint8_t reg = CODEC_PAGE_SEL_REG;
	uint8_t read1[2];
	uint8_t read2[2];
	uint8_t page_sel[2] = { CODEC_PAGE_SEL_REG, 0x00 };

	i2c_configure(dev_i2c,
		       I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER);

	/* Ensure page 0 is selected first */
	ret = i2c_write(dev_i2c, page_sel, sizeof(page_sel), CODEC_ADDR);
	zassert_equal(ret, 0, "page select failed: %d", ret);

	/* msg[0]: WRITE reg address, no STOP */
	msgs[0].buf = &reg;
	msgs[0].len = 1;
	msgs[0].flags = I2C_MSG_WRITE;

	/* msg[1]: READ 2 bytes, no STOP → merged with msg[2] */
	msgs[1].buf = read1;
	msgs[1].len = sizeof(read1);
	msgs[1].flags = I2C_MSG_READ;

	/* msg[2]: READ 2 bytes, with STOP → end of merged read */
	msgs[2].buf = read2;
	msgs[2].len = sizeof(read2);
	msgs[2].flags = I2C_MSG_READ | I2C_MSG_STOP;

	ret = i2c_transfer(dev_i2c, msgs, 3, CODEC_ADDR);
	zassert_equal(ret, 0, "merge-read transfer failed: %d", ret);

	TC_PRINT("Merge-read: [0x%02x 0x%02x] [0x%02x 0x%02x]\n",
		 read1[0], read1[1], read2[0], read2[1]);
}

ZTEST_SUITE(i2c_codec, NULL, suite_setup, NULL, NULL, NULL);
