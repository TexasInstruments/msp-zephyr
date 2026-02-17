/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C Target Test - Controller Side
 *
 * This application runs on the controller board and initiates I2C transactions
 * to test the target board. It synchronizes with the target via GPIO signals.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* Target address */
#define TARGET_ADDR 0x54

/* GPIO for synchronization */
#define READY_GPIO_NODE    DT_ALIAS(ready_gpio)
#define TRIGGER_GPIO_NODE  DT_ALIAS(trigger_gpio)

/* I2C controller node */
#define I2C_CONTROLLER_NODE DT_ALIAS(i2c_controller)

/* Test parameters */
#define READY_TIMEOUT_MS 30000  /* 30 seconds timeout for target to be ready */
#define INTER_TEST_DELAY_MS 500 /* Delay between tests */

/* GPIO specs */
static const struct gpio_dt_spec ready_gpio = GPIO_DT_SPEC_GET(READY_GPIO_NODE, gpios);
static const struct gpio_dt_spec trigger_gpio = GPIO_DT_SPEC_GET(TRIGGER_GPIO_NODE, gpios);

/* I2C device */
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_CONTROLLER_NODE);

/* Buffers */
static uint8_t tx_buffer[64];
static uint8_t rx_buffer[64];

/* Synchronization */
static K_SEM_DEFINE(ready_signal, 0, 1);
static struct gpio_callback ready_cb_data;

/*
 * GPIO interrupt handler - triggered when target signals READY
 */
void ready_gpio_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	k_sem_give(&ready_signal);
}

/*
 * Wait for target to signal READY
 */
static int wait_for_ready(void)
{
	printk("Waiting for target READY signal...\n");

	int ret = k_sem_take(&ready_signal, K_MSEC(READY_TIMEOUT_MS));
	if (ret != 0) {
		printk("ERROR: Timeout waiting for target READY\n");
		return ret;
	}

	printk("Target is READY\n");
	return 0;
}

/*
 * Signal target that transaction is complete
 */
static void signal_trigger(void)
{
	gpio_pin_set_dt(&trigger_gpio, 1);
	k_msleep(10);
	gpio_pin_set_dt(&trigger_gpio, 0);
	printk("Sent TRIGGER to target\n");
}

/*
 * Initialize GPIO pins
 */
static int init_gpio(void)
{
	int ret;

	/* Configure READY GPIO (input with interrupt) */
	if (!gpio_is_ready_dt(&ready_gpio)) {
		printk("ERROR: READY GPIO not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&ready_gpio, GPIO_INPUT);
	if (ret != 0) {
		printk("ERROR: Failed to configure READY GPIO: %d\n", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&ready_gpio, GPIO_INT_EDGE_RISING);
	if (ret != 0) {
		printk("ERROR: Failed to configure READY GPIO interrupt: %d\n", ret);
		return ret;
	}

	gpio_init_callback(&ready_cb_data, ready_gpio_handler, BIT(ready_gpio.pin));
	ret = gpio_add_callback(ready_gpio.port, &ready_cb_data);
	if (ret != 0) {
		printk("ERROR: Failed to add GPIO callback: %d\n", ret);
		return ret;
	}

	printk("READY GPIO configured\n");

	/* Configure TRIGGER GPIO (output) */
	if (!gpio_is_ready_dt(&trigger_gpio)) {
		printk("ERROR: TRIGGER GPIO not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&trigger_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		printk("ERROR: Failed to configure TRIGGER GPIO: %d\n", ret);
		return ret;
	}

	printk("TRIGGER GPIO configured\n");

	return 0;
}

/*
 * TEST 1: Single byte write
 */
static int test_01_single_byte_write(void)
{
	printk("\n=== TEST 1: Single Byte Write ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	tx_buffer[0] = 0xAA;

	int ret = i2c_write(i2c_dev, tx_buffer, 1, TARGET_ADDR);
	if (ret != 0) {
		printk("ERROR: I2C write failed: %d\n", ret);
		return ret;
	}

	printk("Wrote 1 byte: 0xAA\n");
	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * TEST 2: Multi-byte write (8 bytes)
 */
static int test_02_multi_byte_write(void)
{
	printk("\n=== TEST 2: Multi-Byte Write ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	/* Prepare sequential data 0x00-0x07 */
	for (int i = 0; i < 8; i++) {
		tx_buffer[i] = i;
	}

	int ret = i2c_write(i2c_dev, tx_buffer, 8, TARGET_ADDR);
	if (ret != 0) {
		printk("ERROR: I2C write failed: %d\n", ret);
		return ret;
	}

	printk("Wrote 8 bytes: 0x00-0x07\n");
	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * TEST 3: Single byte read
 */
static int test_03_single_byte_read(void)
{
	printk("\n=== TEST 3: Single Byte Read ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	int ret = i2c_read(i2c_dev, rx_buffer, 1, TARGET_ADDR);
	if (ret != 0) {
		printk("ERROR: I2C read failed: %d\n", ret);
		return ret;
	}

	printk("Read 1 byte: 0x%02X (expected 0x42)\n", rx_buffer[0]);
	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * TEST 4: Multi-byte read (8 bytes)
 */
static int test_04_multi_byte_read(void)
{
	printk("\n=== TEST 4: Multi-Byte Read ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	int ret = i2c_read(i2c_dev, rx_buffer, 8, TARGET_ADDR);
	if (ret != 0) {
		printk("ERROR: I2C read failed: %d\n", ret);
		return ret;
	}

	printk("Read 8 bytes: ");
	for (int i = 0; i < 8; i++) {
		printk("0x%02X ", rx_buffer[i]);
	}
	printk("(expected 0x10-0x17)\n");

	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * TEST 5: Repeated start (write then read)
 */
static int test_05_repeated_start(void)
{
	printk("\n=== TEST 5: Repeated Start ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	/* Write 2 bytes */
	tx_buffer[0] = 0xDE;
	tx_buffer[1] = 0xAD;

	/* Request 4 bytes */
	int ret = i2c_write_read(i2c_dev, TARGET_ADDR, tx_buffer, 2, rx_buffer, 4);
	if (ret != 0) {
		printk("ERROR: I2C write-read failed: %d\n", ret);
		return ret;
	}

	printk("Write-Read: wrote 2 bytes, read 4 bytes: ");
	for (int i = 0; i < 4; i++) {
		printk("0x%02X ", rx_buffer[i]);
	}
	printk("(expected 0x50-0x53)\n");

	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * TEST 6: Large write (32 bytes)
 */
static int test_06_large_write(void)
{
	printk("\n=== TEST 6: Large Write ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	/* Prepare pattern 0x00-0x1F */
	for (int i = 0; i < 32; i++) {
		tx_buffer[i] = i;
	}

	int ret = i2c_write(i2c_dev, tx_buffer, 32, TARGET_ADDR);
	if (ret != 0) {
		printk("ERROR: I2C write failed: %d\n", ret);
		return ret;
	}

	printk("Wrote 32 bytes: 0x00-0x1F\n");
	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * TEST 7: Large read (32 bytes)
 */
static int test_07_large_read(void)
{
	printk("\n=== TEST 7: Large Read ===\n");

	if (wait_for_ready() != 0) {
		return -ETIMEDOUT;
	}

	int ret = i2c_read(i2c_dev, rx_buffer, 32, TARGET_ADDR);
	if (ret != 0) {
		printk("ERROR: I2C read failed: %d\n", ret);
		return ret;
	}

	printk("Read 32 bytes: 0x%02X...0x%02X (expected 0xA0-0xBF)\n",
	       rx_buffer[0], rx_buffer[31]);

	signal_trigger();
	k_msleep(INTER_TEST_DELAY_MS);

	return 0;
}

/*
 * Main function
 */
int main(void)
{
	int ret;

	printk("\n");
	printk("========================================\n");
	printk("  I2C Target Test - Controller Board\n");
	printk("========================================\n\n");

	/* Check I2C device */
	if (!device_is_ready(i2c_dev)) {
		printk("ERROR: I2C device not ready\n");
		return -ENODEV;
	}
	printk("I2C controller ready\n");

	/* Initialize GPIO */
	ret = init_gpio();
	if (ret != 0) {
		printk("ERROR: GPIO initialization failed\n");
		return ret;
	}

	printk("\nStarting test sequence...\n");
	printk("Target address: 0x%02X\n\n", TARGET_ADDR);

	/* Run all tests */
	ret = test_01_single_byte_write();
	if (ret != 0) {
		printk("TEST 1 FAILED\n");
		goto error;
	}

	ret = test_02_multi_byte_write();
	if (ret != 0) {
		printk("TEST 2 FAILED\n");
		goto error;
	}

	ret = test_03_single_byte_read();
	if (ret != 0) {
		printk("TEST 3 FAILED\n");
		goto error;
	}

	ret = test_04_multi_byte_read();
	if (ret != 0) {
		printk("TEST 4 FAILED\n");
		goto error;
	}

	ret = test_05_repeated_start();
	if (ret != 0) {
		printk("TEST 5 FAILED\n");
		goto error;
	}

	ret = test_06_large_write();
	if (ret != 0) {
		printk("TEST 6 FAILED\n");
		goto error;
	}

	ret = test_07_large_read();
	if (ret != 0) {
		printk("TEST 7 FAILED\n");
		goto error;
	}

	printk("\n========================================\n");
	printk("  ALL TESTS COMPLETED SUCCESSFULLY!\n");
	printk("========================================\n\n");

	return 0;

error:
	printk("\n========================================\n");
	printk("  TEST SEQUENCE FAILED\n");
	printk("========================================\n\n");
	return ret;
}
