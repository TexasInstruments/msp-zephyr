/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/clock_control/mspm0_clock_control.h>
#include <ti/driverlib/driverlib.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   2000

/* PA22 for CLK_OUT */
#define GPIO_CLKOUT_PIN                                           DL_GPIO_PIN_22
#define GPIO_CLKOUT_IOMUX                                        (IOMUX_PINCM47)
#define GPIO_CLKOUT_IOMUX_FUNC                   IOMUX_PINCM47_PF_SYSCTL_CLK_OUT
#define GPIO_CLKOUT_PORT                                                   GPIOA

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;
	bool led_state = true;

	DL_GPIO_enablePower(GPIOA);
	delay_cycles(16);
	DL_GPIO_initPeripheralOutputFunction(GPIO_CLKOUT_IOMUX, GPIO_CLKOUT_IOMUX_FUNC);
	DL_GPIO_enableOutput(GPIO_CLKOUT_PORT, GPIO_CLKOUT_PIN);
	DL_SYSCTL_enableExternalClock(DL_SYSCTL_CLK_OUT_SOURCE_ULPCLK, DL_SYSCTL_CLK_OUT_DIVIDE_16);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	printf("Hello from TI\n");

	while (1) {

		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		/* During this k_msleep(), the PM policy manager automatically
		 * selects an appropriate SoC sleep state and the system enters
		 * low-power mode. Timer interrupt wakes the system after 1000ms.
		 */
		k_msleep(SLEEP_TIME_MS);

	}
	return 0;
}
