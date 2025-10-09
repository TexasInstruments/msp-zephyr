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

#define STACKSIZE 2048
#define PRIORITY 7
#define LP5810_LP5811_NUM_LEDS 4

const struct device *const dev = DEVICE_DT_GET_ANY(ti_lp5810);

void turn_on_all_leds() {
	for(uint8_t i = 0; i < LP5810_LP5811_NUM_LEDS; i++) {
		lp5810_lp5811_set_analog_dimming(dev, i, 0xFF);
		lp5810_lp5811_set_pwm_dimming(dev, i, 0xFF);
	}
}

void turn_off_all_leds() {
	for(uint8_t i = 0; i < LP5810_LP5811_NUM_LEDS; i++) {
		lp5810_lp5811_set_analog_dimming(dev, i, 0x00);
		lp5810_lp5811_set_pwm_dimming(dev, i, 0x00);
	}
}

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
	if (!dev) {
		return 0;
	} else if (!device_is_ready(dev)) {
		return 0;
	}

	turn_on_all_leds();

	while(1) {
		led_marquee();
	}

	return 0;
}