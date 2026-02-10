/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

uint8_t txPacket[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
uint8_t rxPacket[6];
struct k_sem i2c_controller_sem;


#define SW0_NODE	DT_ALIAS(sw0)
#define I2C_NODE	DT_ALIAS(i2c_0)
#define DEV_1_NODE	DT_NODELABEL(dev1)
#define SLEEP_INTERVAL_MS 1


#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	k_sem_give(&i2c_controller_sem);
}


struct i2c_msg msg_0 = {
	.buf = txPacket,
	.len = 1,
	.flags = I2C_MSG_WRITE
};

struct i2c_msg msg_1 = {
	.buf = &txPacket[1],
	.len = 3,
	.flags = I2C_MSG_WRITE
};

struct i2c_msg msg_2 = {
	.buf = rxPacket,
	.len = 1,
	.flags = I2C_MSG_READ
};

struct i2c_msg msg_3 = {
	.buf = &rxPacket[1],
	.len = 4,
	.flags = I2C_MSG_READ
};

#define LED0_NODE DT_ALIAS(led0)

static const struct i2c_dt_spec dev_1 = I2C_DT_SPEC_GET(DEV_1_NODE);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);


int main(void)
{
	int ret;
	printf("Hello TI! %s\n", CONFIG_BOARD_TARGET);

	k_sem_init(&i2c_controller_sem, 1, 1);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	while(true){
		k_sem_take(&i2c_controller_sem, K_FOREVER);
		k_msleep(SLEEP_INTERVAL_MS);

		printf("transmission of single write...");
		gpio_pin_toggle_dt(&led);
		ret = i2c_write_dt(&dev_1, &txPacket[0], 1);
		if(ret == 0){
			printf("success!!\n");
		} else {
			printf("failed with %d\n", ret);
		}

		k_msleep(SLEEP_INTERVAL_MS);

		printf("transmission of single read... ");
		gpio_pin_toggle_dt(&led);
		ret = i2c_read_dt(&dev_1, &rxPacket[0], 1);
		if(ret == 0){
			printf("success!!\n");
		} else {
			printf("failed with %d\n", ret);
		}
		k_msleep(SLEEP_INTERVAL_MS);

		printf("transmission of burst read...");
		gpio_pin_toggle_dt(&led);
		ret = i2c_read_dt(&dev_1, &rxPacket[1], 5);
		if(ret == 0){
			printf("success!!\n");
		} else {
			printf("failed with %d\n", ret);
		}

		k_msleep(SLEEP_INTERVAL_MS);

		printf("transmission of burst write...");
		gpio_pin_toggle_dt(&led);
		ret = i2c_write_dt(&dev_1, &txPacket[1], 5);
		if(ret == 0){
			printf("success!!\n");
		} else {
			printf("failed with %d\n", ret);
		}
	}


	while(1)

	return 0;
}
