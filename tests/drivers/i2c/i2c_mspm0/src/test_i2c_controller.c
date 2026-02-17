/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C Target Tester - Ztest Version
 *
 * This test validates an I2C target device at address 0x32
 * that implements a specific test fixture protocol.
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>

#define TARGET_ADDR 0x32

/* Test mode commands */
#define CMD_WRITE_MODE          0x01
#define CMD_READ_MODE           0x02
#define CMD_REPEATED_START_MODE 0x03
#define CMD_CLOCK_STRETCH_MODE  0x04

/* Device tree nodes */
#define I2C_NODE	DT_ALIAS(i2c_0)
#define DEV_1_NODE	DT_NODELABEL(dev1)
#define LED0_NODE	DT_ALIAS(led0)

#define SLEEP_INTERVAL_MS 100
#define I2C_READ_TIMEOUT_MS 100

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct i2c_dt_spec target_dev = I2C_DT_SPEC_GET(DEV_1_NODE);

static uint8_t tx_buffer[32];
static uint8_t rx_buffer[32];

/* Global flag to track critical test failures */
static bool critical_test_failed = false;

/* Thread resources for timeout-based I2C read */
K_THREAD_STACK_DEFINE(i2c_read_stack, 1024);
static struct k_thread i2c_read_thread_data;
static struct k_mutex i2c_read_mutex;

/* Context for I2C read with timeout */
struct i2c_read_ctx {
	const struct i2c_dt_spec *spec;
	uint8_t *buf;
	uint32_t num_bytes;
	int result;
	volatile bool completed;
};

/* Thread entry point for I2C read */
void i2c_read_thread_entry(void *p1, void *p2, void *p3)
{
	struct i2c_read_ctx *ctx = (struct i2c_read_ctx *)p1;

	ctx->result = i2c_read_dt(ctx->spec, ctx->buf, ctx->num_bytes);
	ctx->completed = true;
}

/* Perform I2C read with application-level timeout */
int i2c_read_with_timeout(const struct i2c_dt_spec *spec, uint8_t *buf,
                          uint32_t num_bytes, uint32_t timeout_ms)
{
	int ret;

	/* Lock mutex to prevent concurrent timeout operations */
	k_mutex_lock(&i2c_read_mutex, K_FOREVER);

	struct i2c_read_ctx ctx = {
		.spec = spec,
		.buf = buf,
		.num_bytes = num_bytes,
		.result = 0,
		.completed = false,
	};

	/* Create and start the I2C read thread */
	k_tid_t tid = k_thread_create(&i2c_read_thread_data, i2c_read_stack,
	                               K_THREAD_STACK_SIZEOF(i2c_read_stack),
	                               i2c_read_thread_entry,
	                               &ctx, NULL, NULL,
	                               K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

	/* Wait for completion with timeout */
	uint32_t start_time = k_uptime_get_32();
	while (!ctx.completed) {
		k_msleep(1);
		if ((k_uptime_get_32() - start_time) > timeout_ms) {
			/* Timeout occurred */
			TC_PRINT("Application timeout after %u ms\n", timeout_ms);
			k_thread_abort(tid);
			k_mutex_unlock(&i2c_read_mutex);
			return -ETIMEDOUT;
		}
	}

	ret = ctx.result;
	k_mutex_unlock(&i2c_read_mutex);

	return ret;
}

/* Predicate to check if tests should continue */
static bool should_run_test(const void *state)
{
	if (critical_test_failed) {
		TC_PRINT("Skipping test due to previous critical failure\n");
		return false;
	}
	return true;
}

/* Test setup - runs before each test */
static void *i2c_target_setup(void)
{
	int ret;

	/* Initialize I2C read timeout mutex */
	k_mutex_init(&i2c_read_mutex);

	/* Configure LED if available */
	if (gpio_is_ready_dt(&led)) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
		zassert_equal(ret, 0, "Failed to configure LED: %d", ret);
	}

	/* Check I2C device */
	zassert_true(device_is_ready(target_dev.bus), "I2C device not ready");

	TC_PRINT("I2C Target Test Setup Complete\n");
	TC_PRINT("Target Address: 0x%02X\n", TARGET_ADDR);

	return NULL;
}

/* TEST 1: Write Mode */
ZTEST(i2c_mspm0_target, test_1_write_mode)
{
	int ret;

	TC_PRINT("\n=== TEST 1: WRITE MODE ===\n");
	TC_PRINT("Setting write mode (command 0x01)...\n");

	/* Step 1: Send command to set write mode */
	tx_buffer[0] = CMD_WRITE_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send write mode command: %d", ret);
	TC_PRINT("Write mode command sent successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 2: Write 1 byte of data */
	TC_PRINT("Writing 1 byte of test data...\n");
	tx_buffer[0] = 0xAA;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to write 1 byte: %d", ret);
	TC_PRINT("1 byte write successful\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 3: Set write mode again before next test */
	TC_PRINT("Setting write mode again (command 0x01)...\n");
	tx_buffer[0] = CMD_WRITE_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send write mode command: %d", ret);

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 4: Write 10 bytes of data */
	TC_PRINT("Writing 10 bytes of test data...\n");
	for (int i = 0; i < 10; i++) {
		tx_buffer[i] = 0x10 + i;
	}
	ret = i2c_write_dt(&target_dev, tx_buffer, 10);
	zassert_equal(ret, 0, "Failed to write 10 bytes: %d", ret);
	TC_PRINT("10 byte write successful\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 5: Set write mode again before next test */
	TC_PRINT("Setting write mode again (command 0x01)...\n");
	tx_buffer[0] = CMD_WRITE_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send write mode command: %d", ret);

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 6: Write 8 bytes of data */
	TC_PRINT("Writing 8 bytes of test data...\n");
	for (int i = 0; i < 8; i++) {
		tx_buffer[i] = 0x20 + i;
	}
	ret = i2c_write_dt(&target_dev, tx_buffer, 8);
	zassert_equal(ret, 0, "Failed to write 8 bytes: %d", ret);
	TC_PRINT("8 byte write successful\n");

	TC_PRINT("TEST 1 PASSED: All write operations completed\n");
}

/* TEST 2: Read Mode */
ZTEST(i2c_mspm0_target, test_2_read_mode)
{
	int ret;

	TC_PRINT("\n=== TEST 2: READ MODE ===\n");
	TC_PRINT("Setting read mode (command 0x02)...\n");

	/* Step 1: Send command to set read mode */
	tx_buffer[0] = CMD_READ_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send read mode command: %d", ret);
	TC_PRINT("Read mode command sent successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 2: Read 4 bytes */
	TC_PRINT("Reading 4 bytes (expected: 0x00, 0x01, 0x02, 0x03)...\n");
	memset(rx_buffer, 0xFF, sizeof(rx_buffer));
	ret = i2c_read_dt(&target_dev, rx_buffer, 4);
	zassert_equal(ret, 0, "Failed to read 4 bytes: %d", ret);

	/* Verify data */
	for (int i = 0; i < 4; i++) {
		zassert_equal(rx_buffer[i], i,
		              "Byte %d mismatch - expected 0x%02X, got 0x%02X",
		              i, i, rx_buffer[i]);
	}
	TC_PRINT("4 byte read verified successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 3: Set read mode again before next test */
	TC_PRINT("Setting read mode again (command 0x02)...\n");
	tx_buffer[0] = CMD_READ_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send read mode command: %d", ret);

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 4: Read 8 bytes */
	TC_PRINT("Reading 8 bytes (expected: 0x00-0x07)...\n");
	memset(rx_buffer, 0xFF, sizeof(rx_buffer));
	ret = i2c_read_dt(&target_dev, rx_buffer, 8);
	zassert_equal(ret, 0, "Failed to read 8 bytes: %d", ret);

	/* Verify data */
	for (int i = 0; i < 8; i++) {
		zassert_equal(rx_buffer[i], i,
		              "Byte %d mismatch - expected 0x%02X, got 0x%02X",
		              i, i, rx_buffer[i]);
	}
	TC_PRINT("8 byte read verified successfully\n");

	TC_PRINT("TEST 2 PASSED: Read mode verification complete\n");
}

/* TEST 3: Repeated Start Mode */
ZTEST(i2c_mspm0_target, test_3_repeated_start_mode)
{
	int ret;

	TC_PRINT("\n=== TEST 3: REPEATED START MODE ===\n");
	TC_PRINT("Setting repeated start mode (command 0x03)...\n");

	/* Step 1: Send command to set repeated start mode */
	tx_buffer[0] = CMD_REPEATED_START_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send repeated start mode command: %d", ret);
	TC_PRINT("Repeated start mode command sent successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 2: Write base value 0x50, then read 4 bytes */
	TC_PRINT("Test 1: Write 0x50, read 4 bytes (expected: 0x51-0x54)...\n");
	tx_buffer[0] = 0x50;
	memset(rx_buffer, 0xFF, sizeof(rx_buffer));
	ret = i2c_write_read_dt(&target_dev, tx_buffer, 1, rx_buffer, 4);
	zassert_equal(ret, 0, "Failed write-read operation: %d", ret);

	/* Verify data */
	for (int i = 0; i < 4; i++) {
		uint8_t expected = 0x50 + i + 1;
		zassert_equal(rx_buffer[i], expected,
		              "Byte %d mismatch - expected 0x%02X, got 0x%02X",
		              i, expected, rx_buffer[i]);
	}
	TC_PRINT("Repeated start test 1 verified successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 3: Set repeated start mode again before next test */
	TC_PRINT("Setting repeated start mode again (command 0x03)...\n");
	tx_buffer[0] = CMD_REPEATED_START_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send repeated start mode command: %d", ret);

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 4: Write base value 0x10, then read 3 bytes */
	TC_PRINT("Test 2: Write 0x10, read 3 bytes (expected: 0x11-0x13)...\n");
	tx_buffer[0] = 0x10;
	memset(rx_buffer, 0xFF, sizeof(rx_buffer));
	ret = i2c_write_read_dt(&target_dev, tx_buffer, 1, rx_buffer, 3);
	zassert_equal(ret, 0, "Failed write-read operation: %d", ret);

	/* Verify data */
	for (int i = 0; i < 3; i++) {
		uint8_t expected = 0x10 + i + 1;
		zassert_equal(rx_buffer[i], expected,
		              "Byte %d mismatch - expected 0x%02X, got 0x%02X",
		              i, expected, rx_buffer[i]);
	}
	TC_PRINT("Repeated start test 2 verified successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 5: Set repeated start mode again before next test */
	TC_PRINT("Setting repeated start mode again (command 0x03)...\n");
	tx_buffer[0] = CMD_REPEATED_START_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send repeated start mode command: %d", ret);

	k_msleep(SLEEP_INTERVAL_MS);

	/* Step 6: Write base value 0x00, then read 5 bytes */
	TC_PRINT("Test 3: Write 0x00, read 5 bytes (expected: 0x01-0x05)...\n");
	tx_buffer[0] = 0x00;
	memset(rx_buffer, 0xFF, sizeof(rx_buffer));
	ret = i2c_write_read_dt(&target_dev, tx_buffer, 1, rx_buffer, 5);
	zassert_equal(ret, 0, "Failed write-read operation: %d", ret);

	/* Verify data */
	for (int i = 0; i < 5; i++) {
		uint8_t expected = 0x00 + i + 1;
		zassert_equal(rx_buffer[i], expected,
		              "Byte %d mismatch - expected 0x%02X, got 0x%02X",
		              i, expected, rx_buffer[i]);
	}
	TC_PRINT("Repeated start test 3 verified successfully\n");

	TC_PRINT("TEST 3 PASSED: Repeated start mode verification complete\n");
}

/* TEST 4: Clock Stretching Mode */
ZTEST(i2c_mspm0_target, test_4_clock_stretch_mode)
{
	int ret;

	TC_PRINT("\n=== TEST 4: CLOCK STRETCHING MODE ===\n");
	TC_PRINT("Setting clock stretching mode (command 0x04)...\n");

	// Step 1: Send command to set clock stretching mode
	tx_buffer[0] = CMD_CLOCK_STRETCH_MODE;
	ret = i2c_write_dt(&target_dev, tx_buffer, 1);
	zassert_equal(ret, 0, "Failed to send clock stretch mode command: %d", ret);
	TC_PRINT("Clock stretch mode command sent successfully\n");

	k_msleep(SLEEP_INTERVAL_MS);

	// Step 2: Write 8 bytes to trigger clock stretching
	TC_PRINT("Writing 8 bytes to trigger clock stretching...\n");
	TC_PRINT("(Target will stretch clock after 4 bytes + add 500us delay)\n");
	for (int i = 0; i < 8; i++) {
		tx_buffer[i] = 0x30 + i;
	}

	uint32_t start_time = k_cycle_get_32();
	ret = i2c_write_dt(&target_dev, tx_buffer, 8);
	uint32_t end_time = k_cycle_get_32();

	zassert_equal(ret, 0, "Failed to write 8 bytes: %d", ret);

	// Step 3: Immediately perform a read to verify bus still works
	TC_PRINT("Performing immediate 4-byte read with %d ms timeout...\n",
	         I2C_READ_TIMEOUT_MS);
	memset(rx_buffer, 0xFF, sizeof(rx_buffer));
	ret = i2c_read_with_timeout(&target_dev, rx_buffer, 4, I2C_READ_TIMEOUT_MS);
	if (ret == -ETIMEDOUT) {
		TC_PRINT("CRITICAL: Read operation timed out after %d ms\n", I2C_READ_TIMEOUT_MS);
		TC_PRINT("I2C bus may be stuck - marking as critical failure\n");
		critical_test_failed = true;
		zassert_not_equal(ret, -ETIMEDOUT,
		                  "Read operation timed out after %d ms", I2C_READ_TIMEOUT_MS);
	}
	zassert_equal(ret, 0, "Failed to read 4 bytes after clock stretch: %d", ret);

	uint32_t cycles = end_time - start_time;
	uint32_t us = k_cyc_to_us_floor32(cycles);
	TC_PRINT("Write completed successfully\n");
	TC_PRINT("Transaction took approximately %u microseconds\n", us);
	TC_PRINT("(Should be longer than normal due to clock stretching)\n");
	TC_PRINT("Read operation successful - bus operational after clock stretch\n");

	TC_PRINT("TEST 4 PASSED: Clock stretching transaction completed\n");
	TC_PRINT("Note: Use logic analyzer/oscilloscope to verify SCL stretching\n");
}

/* Define the test suite */
ZTEST_SUITE(i2c_mspm0_target, should_run_test, i2c_target_setup, NULL, NULL, NULL);
