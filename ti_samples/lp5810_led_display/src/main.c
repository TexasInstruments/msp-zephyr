/*
 * Copyright (c) 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led/lp5810_lp5811.h>

#define LP5810_LP5811_NUM_LEDS 4

const struct device *const dev = DEVICE_DT_GET_ANY(ti_lp5810);

void led_marquee() {
	int i;
	int loop_length = 256 + 128*3;
	
	for(uint8_t i = 0; i < LP5810_LP5811_NUM_LEDS; i++) {
		lp5810_lp5811_set_analog_dimming(dev, i, 0x10);
		lp5810_lp5811_set_pwm_dimming(dev, i, 0x00);
	}

	for(i = 0; i < loop_length; i++) {
		if(i < 128) {
			lp5810_lp5811_set_pwm_dimming(dev, 0, i);
			k_msleep(1);
		}
		else if(i < 256) {
			lp5810_lp5811_set_pwm_dimming(dev, 0, i);
			lp5810_lp5811_set_pwm_dimming(dev, 1, i - 256);
			k_msleep(1);
		}
		else if(i < 384) {
			lp5810_lp5811_set_pwm_dimming(dev, 0, 255);
			lp5810_lp5811_set_pwm_dimming(dev, 1, i - 128);
			lp5810_lp5811_set_pwm_dimming(dev, 1, i - 256);
			k_msleep(1);
		}
		else if(i < 512) {
			lp5810_lp5811_set_pwm_dimming(dev, 0, 255);
			lp5810_lp5811_set_pwm_dimming(dev, 1, 255);
			lp5810_lp5811_set_pwm_dimming(dev, 2, i - 256);
			lp5810_lp5811_set_pwm_dimming(dev, 3, i - 384);
			k_msleep(1);
		}
		else {
			lp5810_lp5811_set_pwm_dimming(dev, 0, 255);
			lp5810_lp5811_set_pwm_dimming(dev, 1, 255);
			lp5810_lp5811_set_pwm_dimming(dev, 2, 255);
			lp5810_lp5811_set_pwm_dimming(dev, 3, i - 384);
			k_msleep(1);
		}
	}
	k_msleep(250);

	for(i = 128; i >= 0; i--) {
		lp5810_lp5811_set_pwm_dimming(dev, 0, i*2);
		lp5810_lp5811_set_pwm_dimming(dev, 1, i*2);
		lp5810_lp5811_set_pwm_dimming(dev, 2, i*2);
		lp5810_lp5811_set_pwm_dimming(dev, 3, i*2);
		k_msleep(1);
	}

	k_msleep(250);
}


int main() {
	uint8_t i;

	if (!dev) {
		return 0;
	} else if (!device_is_ready(dev)) {
		return 0;
	}

	for(i = 0; i < LP5810_LP5811_NUM_LEDS; i++) {
		led_on(dev, i);
		k_msleep(500);
	}

	/* Turn all LEDs off slowly to demonstrate set_brightness */
	for (i = 0; i <= 100; i++) {
		for (int j = 0; j < LP5810_LP5811_NUM_LEDS; j++) {
			led_set_brightness(dev, j, 100 - i);
		}
		k_msleep(100);
	}

	while(1) {
		led_marquee();
	}

	return 0;
}