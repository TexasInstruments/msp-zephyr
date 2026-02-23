/*
 * Copyright (c) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C Loopback Test - Controller Side
 *
 * Drives loopback tests against a target board running test_i2c_target.c.
 *
 * Synchronization:
 *   - PA8  (TRIGGER, active-low output) is wired to the target's nRST pin.
 *     The controller asserts this once during suite_setup to reset the target
 *     and synchronize the start of the test run.
 *   - PA13 (READY, input) receives a pulse from the target to signal that it
 *     has finished booting / is ready for the next I2C transaction.
 *
 * Each test uses a two-phase handshake driven entirely by READY pulses:
 *
 *   Phase 1 — data exchange:
 *     1. Target signals READY.
 *     2. Controller performs the I2C transaction (write / write_read).
 *     3. Target's stop_cb fires, target prepares echo + STATUS byte.
 *
 *   Phase 2 — echo / status read:
 *     4. Target signals READY again.
 *     5. Controller reads echo bytes + STATUS byte (0x00 = pass, 0xFF = fail).
 *     6. Target's stop_cb fires; both sides run their zasserts.
 *
 * Tests 05 and 06 (NAK) are controller-only and require no target coordination.
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <string.h>

/* ---- Target address ---- */
#define TARGET_ADDR         0x54

/*
 * An address with no device on the bus.  0x77 is chosen because it does not
 * conflict with the loopback target (0x54) and is unlikely to be occupied by
 * any other peripheral present during testing.
 */
#define INVALID_ADDR        0x77

/* ---- Status byte values ---- */
#define TEST_STATUS_PASS    0x00
#define TEST_STATUS_FAIL    0xFF

/* ---- Burst lengths ---- */
#define BURST_8_LEN         8
#define BURST_32_LEN        32

/* ---- Test data (must match target side) ---- */
#define SINGLE_BYTE_VAL     0xAB

static const uint8_t burst_8_data[BURST_8_LEN] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

static const uint8_t burst_32_data[BURST_32_LEN] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

/* Repeated-start: controller writes these two bytes, then reads rs_read_expected */
static const uint8_t rs_write_data[2]    = {0xDE, 0xAD};
static const uint8_t rs_read_expected[4] = {0x11, 0x22, 0x33, 0x44};

/* Merged transaction A: write1 + RS + write2 + RS + read */
static const uint8_t merged_write1[2]        = {0xA1, 0xA2};
static const uint8_t merged_write2[2]        = {0xB1, 0xB2};
static const uint8_t merged_read_expected[4] = {0xC1, 0xC2, 0xC3, 0xC4};

/*
 * Merged transaction B: read1 + RS + read2 + RS + write
 *
 * The target's read_requested_cb resets read_idx to 0 on every new read
 * address, so both read segments are served from read_buffer[0].  The two
 * segments are distinguished by length: read1 is 2 bytes, read2 is 3 bytes.
 */
static const uint8_t rr_w_read1_expected[2] = {0xD1, 0xD2};
static const uint8_t rr_w_read2_expected[3] = {0xD1, 0xD2, 0xD3};
static const uint8_t rr_w_write_data[2]     = {0xF1, 0xF2};

/* ---- Device / GPIO bindings ---- */
#define READY_GPIO_NODE     DT_ALIAS(ready_gpio)
#define TRIGGER_GPIO_NODE   DT_ALIAS(trigger_gpio)
#define I2C_CONTROLLER_NODE DT_ALIAS(i2c_controller)

static const struct gpio_dt_spec ready_gpio =
	GPIO_DT_SPEC_GET(READY_GPIO_NODE, gpios);
static const struct gpio_dt_spec trigger_gpio =
	GPIO_DT_SPEC_GET(TRIGGER_GPIO_NODE, gpios);
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_CONTROLLER_NODE);

/* ---- Synchronization ---- */
static K_SEM_DEFINE(ready_sem, 0, 1);
static struct gpio_callback ready_cb_data;

static void ready_gpio_handler(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	k_sem_give(&ready_sem);
}

static int wait_for_ready(k_timeout_t timeout)
{
	return k_sem_take(&ready_sem, timeout);
}

/*
 * Assert nRST on the target for 10 ms then release.
 * PA8 is configured active-low so gpio_pin_set_dt(1) drives the pin low.
 */
static void reset_target(void)
{
	gpio_pin_set_dt(&trigger_gpio, 1); /* assert reset (PA8 low) */
	k_msleep(10);
	gpio_pin_set_dt(&trigger_gpio, 0); /* release reset (PA8 high) */
}

/* ---- Suite setup ---- */
static void *suite_setup(void)
{
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device not ready");

	/*
	 * Configure READY GPIO first so the interrupt is armed before we
	 * release the target from reset — we must not miss the first pulse.
	 */
	zassert_true(gpio_is_ready_dt(&ready_gpio), "READY GPIO not ready");
	ret = gpio_pin_configure_dt(&ready_gpio, GPIO_INPUT);
	zassert_equal(ret, 0, "READY GPIO configure failed: %d", ret);

	ret = gpio_pin_interrupt_configure_dt(&ready_gpio, GPIO_INT_EDGE_RISING);
	zassert_equal(ret, 0, "READY interrupt configure failed: %d", ret);

	gpio_init_callback(&ready_cb_data, ready_gpio_handler,
			   BIT(ready_gpio.pin));
	ret = gpio_add_callback(ready_gpio.port, &ready_cb_data);
	zassert_equal(ret, 0, "GPIO add callback failed: %d", ret);

	/* Configure TRIGGER (nRST) as output, inactive = PA8 high = not in reset */
	zassert_true(gpio_is_ready_dt(&trigger_gpio), "TRIGGER GPIO not ready");
	ret = gpio_pin_configure_dt(&trigger_gpio, GPIO_OUTPUT_INACTIVE);
	zassert_equal(ret, 0, "TRIGGER GPIO configure failed: %d", ret);

	/* Reset the target to synchronize the start of the test run */
	reset_target();

	return NULL;
}

/* ---- TEST 1: Single byte echo ------------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_01_single_byte_echo)
{
	uint8_t tx = SINGLE_BYTE_VAL;
	uint8_t rx[2] = {0}; /* [0] = echo byte, [1] = status */

	/* Phase 1: write one byte */
	zassert_equal(wait_for_ready(K_SECONDS(30)), 0,
		      "Timeout waiting for target READY (phase 1)");
	zassert_equal(i2c_write(i2c_dev, &tx, 1, TARGET_ADDR), 0,
		      "i2c_write failed");

	/* Phase 2: read echo + status */
	zassert_equal(wait_for_ready(K_SECONDS(5)), 0,
		      "Timeout waiting for target READY (phase 2)");
	zassert_equal(i2c_read(i2c_dev, rx, 2, TARGET_ADDR), 0,
		      "i2c_read (echo+status) failed");

	zassert_equal(rx[0], SINGLE_BYTE_VAL,
		      "Echo mismatch: expected 0x%02X, got 0x%02X",
		      SINGLE_BYTE_VAL, rx[0]);
	zassert_equal(rx[1], TEST_STATUS_PASS,
		      "Target reported test failure (status 0x%02X)", rx[1]);
}

/* ---- TEST 2: Burst echo - 8 bytes --------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_02_burst_echo_8)
{
	uint8_t rx[BURST_8_LEN + 1] = {0}; /* [0..7] = echo, [8] = status */

	zassert_equal(wait_for_ready(K_SECONDS(30)), 0,
		      "Timeout waiting for target READY (phase 1)");
	zassert_equal(i2c_write(i2c_dev, burst_8_data, BURST_8_LEN, TARGET_ADDR),
		      0, "i2c_write failed");

	zassert_equal(wait_for_ready(K_SECONDS(5)), 0,
		      "Timeout waiting for target READY (phase 2)");
	zassert_equal(i2c_read(i2c_dev, rx, BURST_8_LEN + 1, TARGET_ADDR), 0,
		      "i2c_read (echo+status) failed");

	zassert_equal(rx[BURST_8_LEN], TEST_STATUS_PASS,
		      "Target reported test failure (status 0x%02X)",
		      rx[BURST_8_LEN]);
	for (size_t i = 0; i < BURST_8_LEN; i++) {
		zassert_equal(rx[i], burst_8_data[i],
			      "Echo byte %zu: expected 0x%02X, got 0x%02X",
			      i, burst_8_data[i], rx[i]);
	}
}

/* ---- TEST 3: Burst echo - 32 bytes -------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_03_burst_echo_32)
{
	uint8_t rx[BURST_32_LEN + 1] = {0}; /* [0..31] = echo, [32] = status */

	zassert_equal(wait_for_ready(K_SECONDS(30)), 0,
		      "Timeout waiting for target READY (phase 1)");
	zassert_equal(i2c_write(i2c_dev, burst_32_data, BURST_32_LEN, TARGET_ADDR),
		      0, "i2c_write failed");

	zassert_equal(wait_for_ready(K_SECONDS(5)), 0,
		      "Timeout waiting for target READY (phase 2)");
	zassert_equal(i2c_read(i2c_dev, rx, BURST_32_LEN + 1, TARGET_ADDR), 0,
		      "i2c_read (echo+status) failed");

	zassert_equal(rx[BURST_32_LEN], TEST_STATUS_PASS,
		      "Target reported test failure (status 0x%02X)",
		      rx[BURST_32_LEN]);
	for (size_t i = 0; i < BURST_32_LEN; i++) {
		zassert_equal(rx[i], burst_32_data[i],
			      "Echo byte %zu: expected 0x%02X, got 0x%02X",
			      i, burst_32_data[i], rx[i]);
	}
}

/* ---- TEST 4: Repeated start (write_read) --------------------------------- */
ZTEST(i2c_mspm0_loopback, test_04_repeated_start)
{
	uint8_t rx[sizeof(rs_read_expected)] = {0};
	uint8_t status;

	zassert_equal(wait_for_ready(K_SECONDS(30)), 0,
		      "Timeout waiting for target READY (phase 1)");
	zassert_equal(
		i2c_write_read(i2c_dev, TARGET_ADDR,
			       rs_write_data, sizeof(rs_write_data),
			       rx, sizeof(rx)),
		0, "i2c_write_read failed");

	zassert_equal(wait_for_ready(K_SECONDS(5)), 0,
		      "Timeout waiting for target READY (phase 2)");
	zassert_equal(i2c_read(i2c_dev, &status, 1, TARGET_ADDR), 0,
		      "i2c_read (status) failed");

	zassert_equal(status, TEST_STATUS_PASS,
		      "Target reported test failure (status 0x%02X)", status);
	for (size_t i = 0; i < sizeof(rs_read_expected); i++) {
		zassert_equal(rx[i], rs_read_expected[i],
			      "Read byte %zu: expected 0x%02X, got 0x%02X",
			      i, rs_read_expected[i], rx[i]);
	}
}

/* ---- TEST 5: Merged transactions ----------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_05_merged_transactions)
{
	uint8_t rx_a[sizeof(merged_read_expected)] = {0};
	uint8_t rx1_b[sizeof(rr_w_read1_expected)] = {0};
	uint8_t rx2_b[sizeof(rr_w_read2_expected)] = {0};
	uint8_t status;
	struct i2c_msg msgs[3];

	/* ---- Sequence A: WRITE + RS + WRITE + RS + READ ---- */
	zassert_equal(wait_for_ready(K_SECONDS(30)), 0,
		      "A: Timeout waiting for target READY (phase 1)");

	msgs[0].buf   = (uint8_t *)merged_write1;
	msgs[0].len   = sizeof(merged_write1);
	msgs[0].flags = I2C_MSG_WRITE;

	msgs[1].buf   = (uint8_t *)merged_write2;
	msgs[1].len   = sizeof(merged_write2);
	msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_RESTART;

	msgs[2].buf   = rx_a;
	msgs[2].len   = sizeof(rx_a);
	msgs[2].flags = I2C_MSG_READ | I2C_MSG_RESTART | I2C_MSG_STOP;

	zassert_equal(i2c_transfer(i2c_dev, msgs, 3, TARGET_ADDR), 0,
		      "A: i2c_transfer (write+RS+write+RS+read) failed");

	zassert_equal(wait_for_ready(K_SECONDS(5)), 0,
		      "A: Timeout waiting for target READY (phase 2)");
	zassert_equal(i2c_read(i2c_dev, &status, 1, TARGET_ADDR), 0,
		      "A: i2c_read (status) failed");

	zassert_equal(status, TEST_STATUS_PASS,
		      "A: Target reported failure (status 0x%02X)", status);
	for (size_t i = 0; i < sizeof(merged_read_expected); i++) {
		zassert_equal(rx_a[i], merged_read_expected[i],
			      "A: Read byte %zu: expected 0x%02X, got 0x%02X",
			      i, merged_read_expected[i], rx_a[i]);
	}

	/* ---- Sequence B: READ + RS + READ + RS + WRITE ---- */
	zassert_equal(wait_for_ready(K_SECONDS(30)), 0,
		      "B: Timeout waiting for target READY (phase 1)");

	msgs[0].buf   = rx1_b;
	msgs[0].len   = sizeof(rx1_b);
	msgs[0].flags = I2C_MSG_READ;

	msgs[1].buf   = rx2_b;
	msgs[1].len   = sizeof(rx2_b);
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_RESTART;

	msgs[2].buf   = (uint8_t *)rr_w_write_data;
	msgs[2].len   = sizeof(rr_w_write_data);
	msgs[2].flags = I2C_MSG_WRITE | I2C_MSG_RESTART | I2C_MSG_STOP;

	zassert_equal(i2c_transfer(i2c_dev, msgs, 3, TARGET_ADDR), 0,
		      "B: i2c_transfer (read+RS+read+RS+write) failed");

	zassert_equal(wait_for_ready(K_SECONDS(5)), 0,
		      "B: Timeout waiting for target READY (phase 2)");
	zassert_equal(i2c_read(i2c_dev, &status, 1, TARGET_ADDR), 0,
		      "B: i2c_read (status) failed");

	zassert_equal(status, TEST_STATUS_PASS,
		      "B: Target reported failure (status 0x%02X)", status);
	for (size_t i = 0; i < sizeof(rr_w_read1_expected); i++) {
		zassert_equal(rx1_b[i], rr_w_read1_expected[i],
			      "B: Read1 byte %zu: expected 0x%02X, got 0x%02X",
			      i, rr_w_read1_expected[i], rx1_b[i]);
	}
	for (size_t i = 0; i < sizeof(rr_w_read2_expected); i++) {
		zassert_equal(rx2_b[i], rr_w_read2_expected[i],
			      "B: Read2 byte %zu: expected 0x%02X, got 0x%02X",
			      i, rr_w_read2_expected[i], rx2_b[i]);
	}
}

/* ---- TEST 6: NAK on write to invalid address ----------------------------- */
ZTEST(i2c_mspm0_loopback, test_06_nak_invalid_write)
{
	uint8_t tx = 0xAB;
	int ret;

	ret = i2c_write(i2c_dev, &tx, 1, INVALID_ADDR);
	zassert_equal(ret, -EIO,
		      "Expected -EIO from write to invalid address, got %d", ret);
}

/* ---- TEST 7: NAK on read from invalid address ---------------------------- */
ZTEST(i2c_mspm0_loopback, test_07_nak_invalid_read)
{
	uint8_t rx;
	int ret;

	ret = i2c_read(i2c_dev, &rx, 1, INVALID_ADDR);
	zassert_equal(ret, -EIO,
		      "Expected -EIO from read from invalid address, got %d", ret);
}

ZTEST_SUITE(i2c_mspm0_loopback, NULL, suite_setup, NULL, NULL, NULL);
