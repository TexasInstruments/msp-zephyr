/*
 * Copyright (c) 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <ti/driverlib/driverlib.h>

static const struct device *const serial_device = DEVICE_DT_GET(DT_NODELABEL(unicomm1_uart)); 

uint8_t test_data[] = {'M', 'S', 'P', '!'};
uint8_t check_buf[4];
uint8_t txCount;
uint8_t rxCount;

ZTEST(unicomm_uart, test_poll_in_poll_out)
{
	int err;
	uint8_t recv_char;

	/* Ensure FIFO empty to start */
	while(!DL_UART_Main_isRXFIFOEmpty(UC1)) {
		DL_UART_receiveData(UC1);
	}

	for(uint8_t i = 0; i < 4; i ++) {
		uart_poll_out(serial_device, test_data[i]);
	}

	for(uint8_t i = 0; i < 4; i ++) {
		err = uart_poll_in(serial_device, &recv_char);
		zassert_equal(err, 0, "'uart_poll_in' failed with error: %d\n", err);
		zassert_equal(recv_char, test_data[i], "'uart_poll_in' did not received expected data");
	}
}

static void test_interrupt_driven_transfer_cb(const struct device *dev, void *user_data)
{
	uart_irq_update(serial_device);
	
	if(uart_irq_tx_ready(serial_device)) {
		txCount += uart_fifo_fill(serial_device, &test_data[txCount], 4);
	}

	if(uart_irq_rx_ready(serial_device)) {
		rxCount += uart_fifo_read(serial_device, &check_buf[rxCount], 4);
	}
}

ZTEST(unicomm_uart, test_interrupt_driven_transfer)
{
	txCount = 0;
	rxCount = 0;
	uart_irq_callback_set(serial_device, test_interrupt_driven_transfer_cb);
	uart_irq_rx_enable(serial_device);
	uart_irq_tx_enable(serial_device);

	k_busy_wait(1000);

	uart_irq_tx_disable(serial_device);
	uart_irq_rx_disable(serial_device);
	uart_irq_callback_set(serial_device, NULL);

	for(int i = 0; i < 4; i++) {
		zassert_equal(test_data[i], check_buf[i], "check buf did not match test data");
	}
}

ZTEST(unicomm_uart, test_error_interrupts)
{
	uint32_t stat;

	uart_irq_err_enable(serial_device);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_INTERRUPT_BREAK_ERROR);
	zassert_equal(stat, DL_UART_INTERRUPT_BREAK_ERROR);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_INTERRUPT_FRAMING_ERROR);
	zassert_equal(stat, DL_UART_INTERRUPT_FRAMING_ERROR);

	uart_irq_err_disable(serial_device);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_INTERRUPT_BREAK_ERROR);
	zassert_equal(stat, 0);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_INTERRUPT_FRAMING_ERROR);
	zassert_equal(stat, 0);
}

ZTEST(unicomm_uart, test_rx_interrupts)
{
	uint32_t stat;

	uart_irq_rx_enable(serial_device);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_MAIN_INTERRUPT_RX);
	zassert_equal(stat, DL_UART_MAIN_INTERRUPT_RX);
	uart_irq_rx_disable(serial_device);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_MAIN_INTERRUPT_RX);
	zassert_equal(stat, 0);
}

ZTEST(unicomm_uart, test_tx_interrupts)
{
	uint32_t stat;

	uart_irq_tx_enable(serial_device);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_MAIN_INTERRUPT_TX);
	zassert_equal(stat, DL_UART_MAIN_INTERRUPT_TX);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_MAIN_INTERRUPT_EOT_DONE);
	zassert_equal(stat, DL_UART_MAIN_INTERRUPT_EOT_DONE);
	uart_irq_tx_disable(serial_device);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_MAIN_INTERRUPT_TX);
	zassert_equal(stat, 0);
	stat = DL_UART_getEnabledInterrupts(UC1, DL_UART_MAIN_INTERRUPT_EOT_DONE);
	zassert_equal(stat, 0);
}

/*
 * Test setup
 */
void *test_setup(void)
{
	zassert_true(device_is_ready(serial_device), "serial device is not ready");
	return NULL;
}

ZTEST_SUITE(unicomm_uart, NULL, test_setup, NULL, NULL, NULL);
