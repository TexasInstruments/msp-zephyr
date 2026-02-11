/*
 * Copyright (c) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @addtogroup t_i2c_controller
 * @{
 * @defgroup t_i2c_controller_operations test_i2c_controller_operations
 * @brief TestPurpose: verify I2C controller read/write operations
 * @}
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#if DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(i2c_0))
#define I2C_DEV_NODE DT_ALIAS(i2c_0)
#else
#error "Please set the correct I2C device using i2c-0 alias"
#endif

#if DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(dev_1))
#define I2C_TARGET_NODE DT_ALIAS(dev_1)
#else
#error "Please set the I2C target device using dev-1 alias"
#endif

/* Test data buffers */
static uint8_t txPacket[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB};
static uint8_t rxPacket[6];

/* I2C configuration */
static uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

/* Invalid I2C address for negative testing */
#define INVALID_I2C_ADDR 0x77

/**
 * @brief Test I2C write to invalid address
 *
 * Verify that writing to an invalid I2C address returns an error.
 */
ZTEST(i2c_controller, test_i2c_write_invalid_address)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Attempt to write to invalid address - should fail */
	ret = i2c_write(i2c_dev, &txPacket[0], 1, INVALID_I2C_ADDR);
	zassert_equal(ret, -EIO, "Expected -EIO when writing to invalid address, got %d", ret);
}

/**
 * @brief Test I2C single byte write
 *
 * Verify that a single byte can be written to the I2C target device.
 */
ZTEST(i2c_controller, test_i2c_single_write)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Single byte write */
	ret = i2c_write_dt(&dev_1, &txPacket[0], 1);
	zassert_equal(ret, 0, "Single byte write failed with error %d", ret);
}

/**
 * @brief Test I2C single byte read
 *
 * Verify that a single byte can be read from the I2C target device.
 */
ZTEST(i2c_controller, test_i2c_single_read)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Clear receive buffer */
	memset(rxPacket, 0, sizeof(rxPacket));

	/* Single byte read */
	ret = i2c_read_dt(&dev_1, &rxPacket[0], 1);
	zassert_equal(ret, 0, "Single byte read failed with error %d", ret);

	/* Verify received data matches expected pattern */
	zassert_equal(rxPacket[0], 0x51, "Expected 0x51, got 0x%02X", rxPacket[0]);
}

/**
 * @brief Test I2C burst read
 *
 * Verify that multiple bytes can be read from the I2C target device.
 */
ZTEST(i2c_controller, test_i2c_burst_read)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Clear receive buffer */
	memset(rxPacket, 0, sizeof(rxPacket));

	/* Burst read (5 bytes) */
	ret = i2c_read_dt(&dev_1, &rxPacket[0], 5);
	zassert_equal(ret, 0, "Burst read failed with error %d", ret);

	/* Verify received data matches expected pattern */
	zassert_equal(rxPacket[0], 0x51, "Expected 0x51, got 0x%02X", rxPacket[0]);
	zassert_equal(rxPacket[1], 0x52, "Expected 0x52, got 0x%02X", rxPacket[1]);
	zassert_equal(rxPacket[2], 0x53, "Expected 0x53, got 0x%02X", rxPacket[2]);
	zassert_equal(rxPacket[3], 0x54, "Expected 0x54, got 0x%02X", rxPacket[3]);
	zassert_equal(rxPacket[4], 0x55, "Expected 0x55, got 0x%02X", rxPacket[4]);
}

/**
 * @brief Test I2C burst write
 *
 * Verify that multiple bytes can be written to the I2C target device.
 */
ZTEST(i2c_controller, test_i2c_burst_write)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Burst write (5 bytes) */
	ret = i2c_write_dt(&dev_1, &txPacket[1], 5);
	zassert_equal(ret, 0, "Burst write failed with error %d", ret);
}

/**
 * @brief Test I2C repeated-start write/read
 *
 * Verify that a write followed by a read with repeated-start works correctly.
 */
ZTEST(i2c_controller, test_i2c_write_read)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Clear receive buffer */
	memset(rxPacket, 0, sizeof(rxPacket));

	/* Write/Read with repeated-start (write 1 byte, read 5 bytes) */
	ret = i2c_write_read_dt(&dev_1, &txPacket[0], 1, &rxPacket[0], 5);
	zassert_equal(ret, 0, "Write/Read with repeated-start failed with error %d", ret);

	/* Verify received data matches expected pattern */
	zassert_equal(rxPacket[0], 0x51, "Expected 0x51, got 0x%02X", rxPacket[0]);
	zassert_equal(rxPacket[1], 0x52, "Expected 0x52, got 0x%02X", rxPacket[1]);
	zassert_equal(rxPacket[2], 0x53, "Expected 0x53, got 0x%02X", rxPacket[2]);
	zassert_equal(rxPacket[3], 0x54, "Expected 0x54, got 0x%02X", rxPacket[3]);
	zassert_equal(rxPacket[4], 0x55, "Expected 0x55, got 0x%02X", rxPacket[4]);
}

/**
 * @brief Test I2C get configuration
 *
 * Verify that i2c_get_config returns the correct configuration.
 */
ZTEST(i2c_controller, test_i2c_get_config)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	uint32_t i2c_cfg_tmp;
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* Get configuration */
	ret = i2c_get_config(i2c_dev, &i2c_cfg_tmp);
	zassert_equal(ret, 0, "I2C get_config failed");

	/* Verify configuration matches */
	zassert_equal(i2c_cfg, i2c_cfg_tmp, "I2C get_config returned invalid config");
}

/**
 * @brief Test I2C fast speed (400kHz) configuration and operation
 *
 * Verify that the I2C controller can be configured to fast speed (400kHz),
 * and that read/write operations work correctly at this speed.
 */
ZTEST(i2c_controller, test_i2c_fast_speed)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	uint32_t fast_cfg = I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER;
	uint32_t i2c_cfg_tmp;
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C to fast speed (400kHz) */
	ret = i2c_configure(i2c_dev, fast_cfg);
	zassert_equal(ret, 0, "Failed to configure I2C to fast speed (400kHz)");

	/* Verify configuration was updated */
	ret = i2c_get_config(i2c_dev, &i2c_cfg_tmp);
	zassert_equal(ret, 0, "I2C get_config failed");
	zassert_equal(fast_cfg, i2c_cfg_tmp, "I2C config mismatch after setting fast speed");

	/* Perform write operation at fast speed */
	ret = i2c_write_dt(&dev_1, &txPacket[0], 3);
	zassert_equal(ret, 0, "Write at fast speed failed with error %d", ret);

	/* Clear receive buffer */
	memset(rxPacket, 0, sizeof(rxPacket));

	/* Perform read operation at fast speed */
	ret = i2c_read_dt(&dev_1, &rxPacket[0], 3);
	zassert_equal(ret, 0, "Read at fast speed failed with error %d", ret);

	/* Verify received data matches expected pattern */
	zassert_equal(rxPacket[0], 0x51, "Expected 0x51, got 0x%02X", rxPacket[0]);
	zassert_equal(rxPacket[1], 0x52, "Expected 0x52, got 0x%02X", rxPacket[1]);
	zassert_equal(rxPacket[2], 0x53, "Expected 0x53, got 0x%02X", rxPacket[2]);

	/* Restore to standard speed for other tests */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "Failed to restore standard speed");
}

/**
 * @brief Test I2C complex message merging with mixed read/write operations
 *
 * Tests the driver's ability to intelligently merge messages based on direction
 * changes, RESTART flags, and STOP flags. This comprehensive test exercises:
 * - Merging consecutive writes
 * - RESTART flag preventing merge
 * - Direction changes (WRITE→READ, READ→WRITE) preventing merge
 * - Merging consecutive reads
 * - RESTART flag splitting read sequences
 * - STOP flag ending a merge sequence
 * - Multiple independent merge sequences in one transfer
 *
 * Expected bus transactions:
 * 1. Merged write: msgs[0-2] (3 bytes) - no STOP
 * 2. Merged write: msgs[3-4] (2 bytes with RESTART) - no STOP
 * 3. Merged read: msgs[5-6] (3 bytes: 2+1) - no STOP
 * 4. Standalone read: msg[7] (1 byte with RESTART) - no STOP
 * 5. Merged write: msgs[8-9] (2 bytes) - STOP after msg[9]
 * 6. Standalone write: msg[10] (1 byte) - STOP
 */
ZTEST(i2c_controller, test_i2c_complex_merging)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(I2C_TARGET_NODE);
	struct i2c_msg msgs[11];
	uint8_t write_buf0[1] = {0xA0};
	uint8_t write_buf1[1] = {0xA1};
	uint8_t write_buf2[1] = {0xA2};
	uint8_t write_buf3[1] = {0xB0};
	uint8_t write_buf4[1] = {0xB1};
	uint8_t read_buf5[2] = {0};
	uint8_t read_buf6[1] = {0};
	uint8_t read_buf7[1] = {0};
	uint8_t write_buf8[1] = {0xC0};
	uint8_t write_buf9[1] = {0xC1};
	uint8_t write_buf10[1] = {0xD0};
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device is not ready");

	/* Configure I2C */
	ret = i2c_configure(i2c_dev, i2c_cfg);
	zassert_equal(ret, 0, "I2C configure failed");

	/* --- Transaction 1: Merged writes [0-2] --- */
	/* msg[0]: Write 1 byte, no flags (will merge with msg[1]) */
	msgs[0].buf = write_buf0;
	msgs[0].len = 1;
	msgs[0].flags = I2C_MSG_WRITE;

	/* msg[1]: Write 1 byte, no flags (will merge with msg[0] and msg[2]) */
	msgs[1].buf = write_buf1;
	msgs[1].len = 1;
	msgs[1].flags = I2C_MSG_WRITE;

	/* msg[2]: Write 1 byte, no flags (merged, but msg[3] has RESTART) */
	msgs[2].buf = write_buf2;
	msgs[2].len = 1;
	msgs[2].flags = I2C_MSG_WRITE;

	/* --- Transaction 2: Merged writes [3-4] with RESTART --- */
	/* msg[3]: Write 1 byte, RESTART (prevents merge with msg[2]) */
	msgs[3].buf = write_buf3;
	msgs[3].len = 1;
	msgs[3].flags = I2C_MSG_WRITE | I2C_MSG_RESTART;

	/* msg[4]: Write 1 byte, no flags (will merge with msg[3]) */
	msgs[4].buf = write_buf4;
	msgs[4].len = 1;
	msgs[4].flags = I2C_MSG_WRITE;

	/* --- Transaction 3: Merged reads [5-6] --- */
	/* msg[5]: Read 2 bytes, no flags (direction change stops write merge) */
	msgs[5].buf = read_buf5;
	msgs[5].len = 2;
	msgs[5].flags = I2C_MSG_READ;

	/* msg[6]: Read 1 byte, no flags (will merge with msg[5], but msg[7] has RESTART) */
	msgs[6].buf = read_buf6;
	msgs[6].len = 1;
	msgs[6].flags = I2C_MSG_READ;

	/* --- Transaction 4: Standalone read [7] with RESTART --- */
	/* msg[7]: Read 1 byte, RESTART (prevents merge with msg[6]) */
	msgs[7].buf = read_buf7;
	msgs[7].len = 1;
	msgs[7].flags = I2C_MSG_READ | I2C_MSG_RESTART;

	/* --- Transaction 5: Merged writes [8-9] --- */
	/* msg[8]: Write 1 byte, no flags (direction change stops read merge) */
	msgs[8].buf = write_buf8;
	msgs[8].len = 1;
	msgs[8].flags = I2C_MSG_WRITE;

	/* msg[9]: Write 1 byte, STOP (will merge with msg[8], then STOP) */
	msgs[9].buf = write_buf9;
	msgs[9].len = 1;
	msgs[9].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	/* --- Transaction 6: Standalone write [10] --- */
	/* msg[10]: Write 1 byte (standalone, msg[9] had STOP) */
	msgs[10].buf = write_buf10;
	msgs[10].len = 1;
	msgs[10].flags = I2C_MSG_WRITE;

	/* Execute complex transfer with multiple merge points */
	ret = i2c_transfer_dt(&dev_1, msgs, 11);
	zassert_equal(ret, 0, "Complex merged transaction failed with error %d", ret);

	/* Verify read data was correctly distributed across different transactions */
	/* Transaction 3: msgs[5-6] merged read gets first 3 bytes (2+1) */
	zassert_equal(read_buf5[0], 0x51, "Expected 0x51, got 0x%02X", read_buf5[0]);
	zassert_equal(read_buf5[1], 0x52, "Expected 0x52, got 0x%02X", read_buf5[1]);
	zassert_equal(read_buf6[0], 0x53, "Expected 0x53, got 0x%02X", read_buf6[0]);
	/* Transaction 4: msg[7] standalone read with RESTART gets 1 byte */
	zassert_equal(read_buf7[0], 0x51, "Expected 0x51, got 0x%02X", read_buf7[0]);
}

ZTEST_SUITE(i2c_controller, NULL, NULL, NULL, NULL, NULL);
