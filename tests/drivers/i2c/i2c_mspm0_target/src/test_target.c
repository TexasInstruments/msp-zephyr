/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C Target API Test - Target Side
 *
 * This test validates the I2C target (slave) functionality on MSPM0.
 * It requires a controller board to initiate I2C transactions.
 * The two boards communicate via I2C (data) and GPIO (synchronization).
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <string.h>

/* Target address for testing */
#define TARGET_ADDR 0x54

/* GPIO for synchronization with controller */
#define READY_GPIO_NODE    DT_ALIAS(ready_gpio)
#define TRIGGER_GPIO_NODE  DT_ALIAS(trigger_gpio)

/* I2C bus node */
#define I2C_TARGET_NODE    DT_ALIAS(i2c_target)

/* Test buffer sizes */
#define TEST_BUFFER_SIZE 64

/* GPIO specs */
static const struct gpio_dt_spec ready_gpio = GPIO_DT_SPEC_GET(READY_GPIO_NODE, gpios);
static const struct gpio_dt_spec trigger_gpio = GPIO_DT_SPEC_GET(TRIGGER_GPIO_NODE, gpios);

/* I2C device */
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_TARGET_NODE);

/* Test buffers */
static uint8_t write_buffer[TEST_BUFFER_SIZE];
static uint8_t read_buffer[TEST_BUFFER_SIZE];
static volatile size_t write_idx;
static volatile size_t read_idx;
static volatile bool stop_received;
static volatile bool write_requested_called;
static volatile bool read_requested_called;

/* Synchronization */
static K_SEM_DEFINE(transaction_complete, 0, 1);
static struct gpio_callback trigger_cb_data;

/* Test statistics */
static volatile uint32_t total_writes;
static volatile uint32_t total_reads;
static volatile uint32_t total_stops;

/*
 * GPIO interrupt handler - triggered by controller when transaction is complete
 */
void trigger_gpio_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	k_sem_give(&transaction_complete);
}

/*
 * I2C Target Callback: Write requested
 * Called when controller initiates a write to this target
 */
static int target_write_requested_cb(struct i2c_target_config *config)
{
	write_idx = 0;
	write_requested_called = true;
	stop_received = false;
	TC_PRINT("Write requested\n");
	return 0;
}

/*
 * I2C Target Callback: Write received
 * Called for each byte received from controller
 */
static int target_write_received_cb(struct i2c_target_config *config, uint8_t val)
{
	if (write_idx >= TEST_BUFFER_SIZE) {
		TC_PRINT("Write buffer overflow!\n");
		return -ENOMEM;
	}

	write_buffer[write_idx++] = val;
	TC_PRINT("Received byte[%zu]: 0x%02X\n", write_idx - 1, val);
	return 0;
}

/*
 * I2C Target Callback: Read requested
 * Called when controller initiates a read from this target
 */
static int target_read_requested_cb(struct i2c_target_config *config, uint8_t *val)
{
	read_idx = 0;
	read_requested_called = true;
	stop_received = false;

	if (read_idx >= TEST_BUFFER_SIZE) {
		TC_PRINT("Read buffer underflow!\n");
		return -ENODATA;
	}

	*val = read_buffer[read_idx++];
	TC_PRINT("Read requested, sending byte[0]: 0x%02X\n", *val);
	total_reads++;
	return 0;
}

/*
 * I2C Target Callback: Read processed
 * Called to provide subsequent bytes during a read transaction
 */
static int target_read_processed_cb(struct i2c_target_config *config, uint8_t *val)
{
	if (read_idx >= TEST_BUFFER_SIZE) {
		TC_PRINT("Read buffer underflow!\n");
		return -ENODATA;
	}

	*val = read_buffer[read_idx++];
	TC_PRINT("Sending byte[%zu]: 0x%02X\n", read_idx - 1, *val);
	total_reads++;
	return 0;
}

/*
 * I2C Target Callback: Stop condition
 * Called when a STOP condition is detected on the bus
 */
static int target_stop_cb(struct i2c_target_config *config)
{
	stop_received = true;
	total_stops++;
	TC_PRINT("Stop condition received\n");

	if (write_requested_called) {
		total_writes += write_idx;
		TC_PRINT("Write complete: %zu bytes\n", write_idx);
	}

	if (read_requested_called) {
		TC_PRINT("Read complete: %zu bytes\n", read_idx);
	}

	return 0;
}

/* I2C target callbacks structure */
static const struct i2c_target_callbacks target_callbacks = {
	.write_requested = target_write_requested_cb,
	.write_received = target_write_received_cb,
	.read_requested = target_read_requested_cb,
	.read_processed = target_read_processed_cb,
	.stop = target_stop_cb,
};

/* I2C target configuration */
static struct i2c_target_config target_config = {
	.address = TARGET_ADDR,
	.callbacks = &target_callbacks,
};

/*
 * Signal controller that target is ready for next test
 */
static void signal_ready(void)
{
	gpio_pin_set_dt(&ready_gpio, 1);
	k_msleep(10);
	gpio_pin_set_dt(&ready_gpio, 0);
	TC_PRINT("Signaled READY to controller\n");
}

/*
 * Wait for controller to complete transaction
 */
static int wait_for_trigger(k_timeout_t timeout)
{
	int ret = k_sem_take(&transaction_complete, timeout);
	if (ret == 0) {
		TC_PRINT("Received TRIGGER from controller\n");
	}
	return ret;
}

/*
 * Reset test state between tests
 */
static void reset_test_state(void)
{
	write_idx = 0;
	read_idx = 0;
	stop_received = false;
	write_requested_called = false;
	read_requested_called = false;
	memset(write_buffer, 0, sizeof(write_buffer));
}

/*
 * Test setup - runs once before all tests
 */
static void *target_test_setup(void)
{
	int ret;

	TC_PRINT("\n=== I2C Target Test Setup ===\n");

	/* Check I2C device */
	zassert_true(device_is_ready(i2c_dev), "I2C device not ready");
	TC_PRINT("I2C device ready\n");

	/* Configure READY GPIO (output) */
	zassert_true(gpio_is_ready_dt(&ready_gpio), "READY GPIO not ready");
	ret = gpio_pin_configure_dt(&ready_gpio, GPIO_OUTPUT_INACTIVE);
	zassert_equal(ret, 0, "Failed to configure READY GPIO: %d", ret);
	TC_PRINT("READY GPIO configured\n");

	/* Configure TRIGGER GPIO (input with interrupt) */
	zassert_true(gpio_is_ready_dt(&trigger_gpio), "TRIGGER GPIO not ready");
	ret = gpio_pin_configure_dt(&trigger_gpio, GPIO_INPUT);
	zassert_equal(ret, 0, "Failed to configure TRIGGER GPIO: %d", ret);

	ret = gpio_pin_interrupt_configure_dt(&trigger_gpio, GPIO_INT_EDGE_RISING);
	zassert_equal(ret, 0, "Failed to configure TRIGGER GPIO interrupt: %d", ret);

	gpio_init_callback(&trigger_cb_data, trigger_gpio_handler, BIT(trigger_gpio.pin));
	ret = gpio_add_callback(trigger_gpio.port, &trigger_cb_data);
	zassert_equal(ret, 0, "Failed to add GPIO callback: %d", ret);
	TC_PRINT("TRIGGER GPIO configured with interrupt\n");

	/* Register as I2C target */
	ret = i2c_target_register(i2c_dev, &target_config);
	zassert_equal(ret, 0, "Failed to register I2C target: %d", ret);
	TC_PRINT("Registered as I2C target at address 0x%02X\n", TARGET_ADDR);

	/* Initialize statistics */
	total_writes = 0;
	total_reads = 0;
	total_stops = 0;

	TC_PRINT("=== Setup Complete ===\n\n");

	return NULL;
}

/*
 * Test cleanup - runs once after all tests
 */
static void target_test_teardown(void *fixture)
{
	int ret;

	TC_PRINT("\n=== Test Statistics ===\n");
	TC_PRINT("Total bytes written: %u\n", total_writes);
	TC_PRINT("Total bytes read: %u\n", total_reads);
	TC_PRINT("Total stop conditions: %u\n", total_stops);

	ret = i2c_target_unregister(i2c_dev, &target_config);
	zassert_equal(ret, 0, "Failed to unregister I2C target: %d", ret);
	TC_PRINT("I2C target unregistered\n");
}

/*
 * TEST 1: Single byte write
 */
ZTEST(i2c_mspm0_target_test, test_01_single_byte_write)
{
	TC_PRINT("\n=== TEST 1: Single Byte Write ===\n");

	reset_test_state();
	signal_ready();

	/* Wait for controller to complete transaction (timeout 5s) */
	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");

	/* Wait a bit for stop callback to be processed */
	k_msleep(100);

	/* Verify */
	zassert_true(write_requested_called, "Write not requested");
	zassert_true(stop_received, "Stop not received");
	zassert_equal(write_idx, 1, "Expected 1 byte, got %zu", write_idx);
	zassert_equal(write_buffer[0], 0xAA, "Expected 0xAA, got 0x%02X", write_buffer[0]);

	TC_PRINT("TEST 1 PASSED\n");
}

/*
 * TEST 2: Multi-byte write
 */
ZTEST(i2c_mspm0_target_test, test_02_multi_byte_write)
{
	TC_PRINT("\n=== TEST 2: Multi-Byte Write (8 bytes) ===\n");

	reset_test_state();
	signal_ready();

	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");
	k_msleep(100);

	/* Verify */
	zassert_true(write_requested_called, "Write not requested");
	zassert_true(stop_received, "Stop not received");
	zassert_equal(write_idx, 8, "Expected 8 bytes, got %zu", write_idx);

	/* Verify sequential data 0x00-0x07 */
	for (size_t i = 0; i < 8; i++) {
		zassert_equal(write_buffer[i], i,
		              "Byte %zu: expected 0x%02X, got 0x%02X",
		              i, i, write_buffer[i]);
	}

	TC_PRINT("TEST 2 PASSED\n");
}

/*
 * TEST 3: Single byte read
 */
ZTEST(i2c_mspm0_target_test, test_03_single_byte_read)
{
	TC_PRINT("\n=== TEST 3: Single Byte Read ===\n");

	reset_test_state();

	/* Prepare data to be read by controller */
	read_buffer[0] = 0x42;

	signal_ready();
	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");
	k_msleep(100);

	/* Verify */
	zassert_true(read_requested_called, "Read not requested");
	zassert_true(stop_received, "Stop not received");
	zassert_equal(read_idx, 1, "Expected 1 byte read, got %zu", read_idx);

	TC_PRINT("TEST 3 PASSED\n");
}

/*
 * TEST 4: Multi-byte read
 */
ZTEST(i2c_mspm0_target_test, test_04_multi_byte_read)
{
	TC_PRINT("\n=== TEST 4: Multi-Byte Read (8 bytes) ===\n");

	reset_test_state();

	/* Prepare sequential data 0x10-0x17 */
	for (size_t i = 0; i < 8; i++) {
		read_buffer[i] = 0x10 + i;
	}

	signal_ready();
	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");
	k_msleep(100);

	/* Verify */
	zassert_true(read_requested_called, "Read not requested");
	zassert_true(stop_received, "Stop not received");
	zassert_equal(read_idx, 8, "Expected 8 bytes read, got %zu", read_idx);

	TC_PRINT("TEST 4 PASSED\n");
}

/*
 * TEST 5: Repeated start (write then read)
 */
ZTEST(i2c_mspm0_target_test, test_05_repeated_start)
{
	TC_PRINT("\n=== TEST 5: Repeated Start (Write-Read) ===\n");

	reset_test_state();

	/* Prepare read data */
	for (size_t i = 0; i < 4; i++) {
		read_buffer[i] = 0x50 + i;
	}

	signal_ready();
	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");
	k_msleep(100);

	/* Verify both write and read occurred */
	zassert_true(write_requested_called, "Write not requested");
	zassert_true(read_requested_called, "Read not requested");
	zassert_true(stop_received, "Stop not received");

	/* Verify write data (2 bytes) */
	zassert_equal(write_idx, 2, "Expected 2 bytes written, got %zu", write_idx);

	/* Verify read data (4 bytes) */
	zassert_equal(read_idx, 4, "Expected 4 bytes read, got %zu", read_idx);

	TC_PRINT("TEST 5 PASSED\n");
}

/*
 * TEST 6: Write large buffer (32 bytes)
 */
ZTEST(i2c_mspm0_target_test, test_06_large_write)
{
	TC_PRINT("\n=== TEST 6: Large Write (32 bytes) ===\n");

	reset_test_state();
	signal_ready();

	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");
	k_msleep(100);

	/* Verify */
	zassert_true(write_requested_called, "Write not requested");
	zassert_true(stop_received, "Stop not received");
	zassert_equal(write_idx, 32, "Expected 32 bytes, got %zu", write_idx);

	/* Verify pattern 0x00-0x1F */
	for (size_t i = 0; i < 32; i++) {
		zassert_equal(write_buffer[i], i,
		              "Byte %zu: expected 0x%02X, got 0x%02X",
		              i, i, write_buffer[i]);
	}

	TC_PRINT("TEST 6 PASSED\n");
}

/*
 * TEST 7: Read large buffer (32 bytes)
 */
ZTEST(i2c_mspm0_target_test, test_07_large_read)
{
	TC_PRINT("\n=== TEST 7: Large Read (32 bytes) ===\n");

	reset_test_state();

	/* Prepare data pattern 0xA0-0xBF */
	for (size_t i = 0; i < 32; i++) {
		read_buffer[i] = 0xA0 + i;
	}

	signal_ready();
	zassert_equal(wait_for_trigger(K_SECONDS(5)), 0, "Timeout waiting for controller");
	k_msleep(100);

	/* Verify */
	zassert_true(read_requested_called, "Read not requested");
	zassert_true(stop_received, "Stop not received");
	zassert_equal(read_idx, 32, "Expected 32 bytes read, got %zu", read_idx);

	TC_PRINT("TEST 7 PASSED\n");
}

/* Define test suite */
ZTEST_SUITE(i2c_mspm0_target_test, NULL, target_test_setup, NULL, NULL, target_test_teardown);
