/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/clock_control/mspm0_clock_control.h>
#include <ti/driverlib/driverlib.h>
#include <zephyr/drivers/uart.h>

/*
UART1 is Transmitter peripheral.
UART2 is Receiver peripheral.

Connect PA17 and PA16 on your board.
Please view prj.conf for more details on low power mode configuration.

*/

#define SLEEP_TIME_MS   1000

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define UART1_NODE DT_NODELABEL(uart1)
#define UART2_NODE DT_NODELABEL(uart2)
static const struct device *uart1_dev;
static const struct device *uart2_dev;

K_MSGQ_DEFINE(uart_msgq, sizeof(uint8_t), 32, 4);

static void serial_cb(const struct device *dev, void *user_data) {
	uint8_t c;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}
	
	while (uart_fifo_read(dev, &c, 1) == 1) {
        k_msgq_put(&uart_msgq, &c, K_NO_WAIT);
    }
}


int main(void)
{
	int ret;

	uart1_dev = DEVICE_DT_GET(UART1_NODE);
	uart2_dev = DEVICE_DT_GET(UART2_NODE);

	if (!device_is_ready(uart1_dev)) {
		printf("UART1 device not found!");
		return 0;
	}

	if (!device_is_ready(uart2_dev)) {
		printf("UART2 device not found!");
		return 0;
	}

	uart_irq_tx_enable (uart1_dev);
	uart_irq_rx_enable (uart2_dev);

	ret = uart_irq_callback_user_data_set(uart2_dev, serial_cb, NULL);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {

		const uint8_t data[] = "ABC\n";
		uart_fifo_fill(uart1_dev, data, sizeof(data));

		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		/* During this k_msleep(), the PM policy manager automatically
		 * selects an appropriate SoC sleep state and the system enters
		 * low-power mode. Timer interrupt wakes the system after 1000ms.
		 */

		uint8_t rx_char;
		while (k_msgq_get(&uart_msgq, &rx_char, K_MSEC(100)) == 0) {
			printf("%c", rx_char);
			if (rx_char == '\n') break;
		}

		k_msleep(SLEEP_TIME_MS);
		
	}
	return 0;
}
