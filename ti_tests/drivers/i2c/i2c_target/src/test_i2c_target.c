/*
 * Copyright (c) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C Loopback Test - Target Side
 *
 * Responds to a controller board running test_i2c_controller.c.
 *
 * The controller resets this board via nRST (connected to the controller's
 * PA8) before the test suite starts.  No TRIGGER GPIO is handled in firmware.
 *
 * Synchronization within each test uses two signals:
 *   - READY (PA13, output): target pulses this to tell the controller it is
 *     ready for the next I2C transaction.
 *   - stop_sem (internal): the i2c_target stop_cb gives this semaphore when
 *     an I2C STOP condition is received, waking the test thread to process
 *     the completed transaction.
 *
 * Each test follows this two-phase sequence:
 *
 *   Phase 1 — data exchange:
 *     1. Target signals READY.
 *     2. Controller performs the I2C transaction (write / write_read).
 *     3. stop_cb fires → stop_sem given → test thread wakes.
 *     4. Target validates received data and populates read_buffer with the
 *        echo payload followed by a STATUS byte (0x00 = pass, 0xFF = fail).
 *
 *   Phase 2 — echo / status read:
 *     5. Target signals READY again.
 *     6. Controller reads echo bytes + STATUS byte.
 *     7. stop_cb fires → stop_sem given → test thread wakes.
 *     8. Both sides run their zasserts.
 *
 * The STATUS byte is always written to read_buffer *before* zasserts run,
 * so the controller can always retrieve it regardless of target pass/fail.
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <string.h>

/* ---- Target address ---- */
#define TARGET_ADDR         0x54

/* ---- Status byte values ---- */
#define TEST_STATUS_PASS    0x00
#define TEST_STATUS_FAIL    0xFF

/* ---- Burst lengths ---- */
#define BURST_8_LEN         8
#define BURST_32_LEN        32

/* ---- Test data (must match controller side) ---- */
#define SINGLE_BYTE_VAL     0xAB

static const uint8_t burst_8_expected[BURST_8_LEN] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

static const uint8_t burst_32_expected[BURST_32_LEN] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

/* Repeated-start: expected write bytes from controller, fixed read response */
static const uint8_t rs_write_expected[2]  = {0xDE, 0xAD};
static const uint8_t rs_read_response[4]   = {0x11, 0x22, 0x33, 0x44};

/* Merged transaction A: write1 + RS + write2 + RS + read */
static const uint8_t merged_write1_expected[2] = {0xA1, 0xA2};
static const uint8_t merged_write2_expected[2] = {0xB1, 0xB2};
static const uint8_t merged_read_response[4]   = {0xC1, 0xC2, 0xC3, 0xC4};

/*
 * Merged transaction B: read1 + RS + read2 + RS + write
 *
 * read_requested_cb resets read_idx to 0 on every new read address, so both
 * read segments are served from read_buffer[0].  Both segments therefore
 * return the same prefix of rr_w_read_response; the segments differ only in
 * length (2 vs. 3 bytes), which is what the controller validates.
 */
static const uint8_t rr_w_read_response[4]  = {0xD1, 0xD2, 0xD3, 0xD4};
static const uint8_t rr_w_write_expected[2] = {0xF1, 0xF2};

/*
 * read_buffer size must fit the largest echo payload + 1 status byte.
 * BURST_32_LEN + 1 = 33; 64 bytes is comfortably sufficient.
 */
#define TEST_BUFFER_SIZE    64

/* ---- Device / GPIO bindings ---- */
#define READY_GPIO_NODE     DT_ALIAS(ready_gpio)
#define I2C_TARGET_NODE     DT_ALIAS(i2c_target)

static const struct gpio_dt_spec ready_gpio =
	GPIO_DT_SPEC_GET(READY_GPIO_NODE, gpios);
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_TARGET_NODE);

/* ---- I2C target state ---- */
static uint8_t write_buffer[TEST_BUFFER_SIZE];
static uint8_t read_buffer[TEST_BUFFER_SIZE];
static volatile size_t write_idx;
static volatile size_t read_idx;

/*
 * Given by stop_cb each time an I2C STOP condition is received.
 * The test thread waits on this to know a transaction has completed.
 */
static K_SEM_DEFINE(stop_sem, 0, 1);

/* ---- I2C target callbacks ---- */
static int target_write_requested_cb(struct i2c_target_config *config)
{
	/*
	 * Do not reset write_idx here.  Each test calls reset_test_state()
	 * before signalling READY, which sets write_idx = 0.  Leaving it
	 * untouched here lets multi-segment transactions (e.g. write + RS +
	 * write) accumulate all bytes contiguously in write_buffer.
	 */
	return 0;
}

static int target_write_received_cb(struct i2c_target_config *config,
				    uint8_t val)
{
	if (write_idx < TEST_BUFFER_SIZE) {
		write_buffer[write_idx++] = val;
	}
	return 0;
}

static int target_read_requested_cb(struct i2c_target_config *config,
				    uint8_t *val)
{
	read_idx = 0;
	*val = read_buffer[read_idx++];
	return 0;
}

static int target_read_processed_cb(struct i2c_target_config *config,
				    uint8_t *val)
{
	if (read_idx < TEST_BUFFER_SIZE) {
		*val = read_buffer[read_idx++];
	}
	return 0;
}

static int target_stop_cb(struct i2c_target_config *config)
{
	k_sem_give(&stop_sem);
	return 0;
}

static const struct i2c_target_callbacks target_callbacks = {
	.write_requested = target_write_requested_cb,
	.write_received  = target_write_received_cb,
	.read_requested  = target_read_requested_cb,
	.read_processed  = target_read_processed_cb,
	.stop            = target_stop_cb,
};

static struct i2c_target_config target_config = {
	.address   = TARGET_ADDR,
	.callbacks = &target_callbacks,
};

/* ---- Synchronization helpers ---- */
static void signal_ready(void)
{
	gpio_pin_set_dt(&ready_gpio, 1);
	k_msleep(10);
	gpio_pin_set_dt(&ready_gpio, 0);
}

/*
 * Wait for the next I2C STOP condition.  Returns 0; K_FOREVER means this
 * never times out.
 */
static int wait_for_stop(void)
{
	return k_sem_take(&stop_sem, K_FOREVER);
}

/* ---- State reset between tests ---- */
static void reset_test_state(void)
{
	write_idx = 0;
	read_idx  = 0;
	memset(write_buffer, 0, sizeof(write_buffer));
	memset(read_buffer,  0, sizeof(read_buffer));
	k_sem_reset(&stop_sem);
}

/* ---- Suite setup / teardown ---- */
static void *suite_setup(void)
{
	int ret;

	zassert_true(device_is_ready(i2c_dev), "I2C device not ready");

	zassert_true(gpio_is_ready_dt(&ready_gpio), "READY GPIO not ready");
	ret = gpio_pin_configure_dt(&ready_gpio, GPIO_OUTPUT_INACTIVE);
	zassert_equal(ret, 0, "READY GPIO configure failed: %d", ret);

	ret = i2c_target_register(i2c_dev, &target_config);
	zassert_equal(ret, 0, "i2c_target_register failed: %d", ret);

	return NULL;
}

static void suite_teardown(void *fixture)
{
	i2c_target_unregister(i2c_dev, &target_config);
}

/* ---- TEST 1: Single byte echo ------------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_01_single_byte_echo)
{
	reset_test_state();
	signal_ready(); /* Phase 1: ready for controller write */

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 1)");

	bool data_ok = (write_idx == 1) && (write_buffer[0] == SINGLE_BYTE_VAL);

	read_buffer[0] = write_buffer[0];
	read_buffer[1] = data_ok ? TEST_STATUS_PASS : TEST_STATUS_FAIL;

	signal_ready(); /* Phase 2: ready for controller echo read */

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 2)");

	zassert_equal(write_idx, 1,
		      "Expected 1 byte, got %zu", write_idx);
	zassert_equal(write_buffer[0], SINGLE_BYTE_VAL,
		      "Expected 0x%02X, got 0x%02X",
		      SINGLE_BYTE_VAL, write_buffer[0]);
}

/* ---- TEST 2: Burst echo - 8 bytes --------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_02_burst_echo_8)
{
	reset_test_state();
	signal_ready();

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 1)");

	bool data_ok = (write_idx == BURST_8_LEN) &&
		       (memcmp(write_buffer, burst_8_expected, BURST_8_LEN) == 0);

	memcpy(read_buffer, write_buffer, BURST_8_LEN);
	read_buffer[BURST_8_LEN] = data_ok ? TEST_STATUS_PASS : TEST_STATUS_FAIL;

	signal_ready();

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 2)");

	zassert_equal(write_idx, BURST_8_LEN,
		      "Expected %d bytes, got %zu", BURST_8_LEN, write_idx);
	for (size_t i = 0; i < BURST_8_LEN; i++) {
		zassert_equal(write_buffer[i], burst_8_expected[i],
			      "Byte %zu: expected 0x%02X, got 0x%02X",
			      i, burst_8_expected[i], write_buffer[i]);
	}
}

/* ---- TEST 3: Burst echo - 32 bytes -------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_03_burst_echo_32)
{
	reset_test_state();
	signal_ready();

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 1)");

	bool data_ok = (write_idx == BURST_32_LEN) &&
		       (memcmp(write_buffer, burst_32_expected, BURST_32_LEN) == 0);

	memcpy(read_buffer, write_buffer, BURST_32_LEN);
	read_buffer[BURST_32_LEN] = data_ok ? TEST_STATUS_PASS : TEST_STATUS_FAIL;

	signal_ready();

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 2)");

	zassert_equal(write_idx, BURST_32_LEN,
		      "Expected %d bytes, got %zu", BURST_32_LEN, write_idx);
	for (size_t i = 0; i < BURST_32_LEN; i++) {
		zassert_equal(write_buffer[i], burst_32_expected[i],
			      "Byte %zu: expected 0x%02X, got 0x%02X",
			      i, burst_32_expected[i], write_buffer[i]);
	}
}

/* ---- TEST 4: Repeated start (write_read) --------------------------------- */
ZTEST(i2c_mspm0_loopback, test_04_repeated_start)
{
	reset_test_state();

	/*
	 * Pre-populate read_buffer with the fixed response data BEFORE
	 * signalling READY.  The controller will issue a write_read, so the
	 * read phase of that transaction must find the data already in place.
	 */
	memcpy(read_buffer, rs_read_response, sizeof(rs_read_response));

	signal_ready(); /* Phase 1: ready for write_read transaction */

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 1)");

	/*
	 * Validate the write bytes received, then repurpose read_buffer[0]
	 * as the STATUS byte for phase 2 (rs_read_response already consumed).
	 */
	bool data_ok = (write_idx == sizeof(rs_write_expected)) &&
		       (memcmp(write_buffer, rs_write_expected,
			       sizeof(rs_write_expected)) == 0);

	read_buffer[0] = data_ok ? TEST_STATUS_PASS : TEST_STATUS_FAIL;

	signal_ready(); /* Phase 2: ready for controller status read */

	zassert_equal(wait_for_stop(), 0, "wait_for_stop failed (phase 2)");

	zassert_equal(write_idx, sizeof(rs_write_expected),
		      "Expected %zu write bytes, got %zu",
		      sizeof(rs_write_expected), write_idx);
	for (size_t i = 0; i < sizeof(rs_write_expected); i++) {
		zassert_equal(write_buffer[i], rs_write_expected[i],
			      "Write byte %zu: expected 0x%02X, got 0x%02X",
			      i, rs_write_expected[i], write_buffer[i]);
	}
}

/* ---- TEST 5: Merged transactions ----------------------------------------- */
ZTEST(i2c_mspm0_loopback, test_05_merged_transactions)
{
	/* ---- Sequence A: WRITE + RS + WRITE + RS + READ ---- */
	reset_test_state();

	/*
	 * Pre-populate read_buffer before signalling READY.  The read phase
	 * is embedded in the same i2c_transfer as the two write segments, so
	 * the data must be in place before the transaction begins.
	 */
	memcpy(read_buffer, merged_read_response, sizeof(merged_read_response));

	signal_ready();
	zassert_equal(wait_for_stop(), 0, "A: wait_for_stop failed (phase 1)");

	size_t expected_write_len_a =
		sizeof(merged_write1_expected) + sizeof(merged_write2_expected);

	bool data_ok_a =
		(write_idx == expected_write_len_a) &&
		(memcmp(write_buffer, merged_write1_expected,
			sizeof(merged_write1_expected)) == 0) &&
		(memcmp(write_buffer + sizeof(merged_write1_expected),
			merged_write2_expected,
			sizeof(merged_write2_expected)) == 0);

	read_buffer[0] = data_ok_a ? TEST_STATUS_PASS : TEST_STATUS_FAIL;

	signal_ready();
	zassert_equal(wait_for_stop(), 0, "A: wait_for_stop failed (phase 2)");

	zassert_equal(write_idx, expected_write_len_a,
		      "A: Expected %zu write bytes, got %zu",
		      expected_write_len_a, write_idx);
	for (size_t i = 0; i < sizeof(merged_write1_expected); i++) {
		zassert_equal(write_buffer[i], merged_write1_expected[i],
			      "A: Write1 byte %zu: expected 0x%02X, got 0x%02X",
			      i, merged_write1_expected[i], write_buffer[i]);
	}
	for (size_t i = 0; i < sizeof(merged_write2_expected); i++) {
		size_t idx = sizeof(merged_write1_expected) + i;

		zassert_equal(write_buffer[idx], merged_write2_expected[i],
			      "A: Write2 byte %zu: expected 0x%02X, got 0x%02X",
			      i, merged_write2_expected[i], write_buffer[idx]);
	}

	/* ---- Sequence B: READ + RS + READ + RS + WRITE ---- */
	/*
	 * Reset write_idx so the write segment lands at write_buffer[0].
	 * read_requested_cb resets read_idx to 0 on every new read address,
	 * so both read segments are served from read_buffer[0]; they differ
	 * only in length (2 vs. 3 bytes).
	 */
	write_idx = 0;
	memcpy(read_buffer, rr_w_read_response, sizeof(rr_w_read_response));

	signal_ready();
	zassert_equal(wait_for_stop(), 0, "B: wait_for_stop failed (phase 1)");

	bool data_ok_b =
		(write_idx == sizeof(rr_w_write_expected)) &&
		(memcmp(write_buffer, rr_w_write_expected,
			sizeof(rr_w_write_expected)) == 0);

	read_buffer[0] = data_ok_b ? TEST_STATUS_PASS : TEST_STATUS_FAIL;

	signal_ready();
	zassert_equal(wait_for_stop(), 0, "B: wait_for_stop failed (phase 2)");

	zassert_equal(write_idx, sizeof(rr_w_write_expected),
		      "B: Expected %zu write bytes, got %zu",
		      sizeof(rr_w_write_expected), write_idx);
	for (size_t i = 0; i < sizeof(rr_w_write_expected); i++) {
		zassert_equal(write_buffer[i], rr_w_write_expected[i],
			      "B: Write byte %zu: expected 0x%02X, got 0x%02X",
			      i, rr_w_write_expected[i], write_buffer[i]);
	}
}

ZTEST_SUITE(i2c_mspm0_loopback, NULL, suite_setup, NULL, NULL,
	    suite_teardown);
