/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/clock_control/mspm0_clock_control.h>
#include <ti/driverlib/driverlib.h>
#include <zephyr/pm/policy.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   2000

/* PA22 for CLK_OUT */
#define GPIO_CLKOUT_PIN                                           DL_GPIO_PIN_22
#define GPIO_CLKOUT_PORT                                                   GPIOA
#define GPIO_CLKOUT_IOMUX                                        (IOMUX_PINCM47)
#define GPIO_CLKOUT_IOMUX_FUNC                   IOMUX_PINCM47_PF_SYSCTL_CLK_OUT


/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define TARGET_ADDR 0x32

uint8_t i2c_data[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0x00};

#define I2C_CONTROLLER_NODE DT_ALIAS(i2c_controller)

static const struct device *i2c_dev = DEVICE_DT_GET(I2C_CONTROLLER_NODE);

static const struct device *const rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

static int set_date_time(const struct device *rtc)
{
	int ret = 0;
	struct rtc_time tm = {
		.tm_year = 2026 - 1900,
		.tm_mon = 6 - 1,
		.tm_mday = 4,
		.tm_hour = 0,
		.tm_min = 0,
		.tm_sec = 0,
	};

	ret = rtc_set_time(rtc, &tm);

	return ret;
}

static int get_date_time(const struct device *rtc)
{
	int ret = 0;
	struct rtc_time tm;

	ret = rtc_get_time(rtc, &tm);
	if (ret < 0) {
		return ret;
	}

	printk("RTC date and time: %04d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900,
	       tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return ret;
}

int main(void)
{
	int ret;
	int64_t uptime_ms;

	DL_GPIO_enablePower(GPIOA);
	delay_cycles(16);
	DL_GPIO_initPeripheralOutputFunction(GPIO_CLKOUT_IOMUX, GPIO_CLKOUT_IOMUX_FUNC);
	DL_GPIO_enableOutput(GPIO_CLKOUT_PORT, GPIO_CLKOUT_PIN);
	DL_SYSCTL_enableExternalClock(DL_SYSCTL_CLK_OUT_SOURCE_ULPCLK, DL_SYSCTL_CLK_OUT_DIVIDE_16);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	if (!device_is_ready(i2c_dev)){
		return 0;
	}

	if (!device_is_ready(rtc)) {
		return 0;
	}

	set_date_time(rtc);


	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	printf("Hello from TI\n");

	while (1) {
		ret = gpio_pin_set_dt(&led, 1);
		if (ret < 0) {
			return 0;
		}

		/* send an i2c transaction */
		i2c_write(i2c_dev, i2c_data, 10, TARGET_ADDR);

		i2c_data[15]++;

		uptime_ms = k_uptime_get();
		printk("System uptime: %lld ms\n", uptime_ms);

		ret = get_date_time(rtc);

		ret = gpio_pin_set_dt(&led, 0);

		/* During this k_msleep(), the PM policy manager automatically
		 * selects an appropriate SoC sleep state and the system enters
		 * low-power mode. Timer interrupt wakes the system after 1000ms.
		 */

		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
