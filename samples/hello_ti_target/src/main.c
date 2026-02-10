/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>




struct k_sem i2c_target_sem;

#define DATA_BUF_SIZE 50
uint8_t i2c_data_buf[DATA_BUF_SIZE];
uint32_t gBufTxIdx = 0;
uint32_t gBufRxIdx = 0;
uint8_t timeoutDetected = 0;

struct k_sem target_sem;

#define SW0_NODE	DT_ALIAS(sw0)
#define I2C_NODE	DT_ALIAS(i2c_0)
#define SLEEP_INTERVAL_MS 1

static int i2c_read_requested(struct i2c_target_config* config, uint8_t *starting_value) {
	*starting_value = (config->address + 0x3);
	return 0;
}

static int i2c_read_processed(struct i2c_target_config* config, uint8_t *next_value) {
	if(gBufRxIdx < gBufTxIdx){
		*next_value = i2c_data_buf[gBufRxIdx++];
	} else {
		*next_value = 0xAA;
	}
	return 0;
}

static int i2c_write_received(struct i2c_target_config* config, uint8_t value) {
	if(gBufTxIdx < DATA_BUF_SIZE){
		i2c_data_buf[gBufTxIdx++] = value;
	}
	return 0;
}

static int i2c_write_requested(struct i2c_target_config *config) {
	/* always accept the write */
	return 0;
}

static int i2c_stop_callback(struct i2c_target_config * cfg){
	/* used to control test flow and notify thread that transaction has
	 * completed */
	int retVal = 0;
	if(cfg->flags & I2C_TARGET_FLAGS_ERROR_TIMEOUT){
		/* force the peripheral to reset */
		timeoutDetected = 1;
		retVal = -1;
	}
	k_sem_give(&i2c_target_sem);
	return retVal;
}

struct i2c_target_callbacks callbacks = {
	.write_requested = i2c_write_requested,
	.read_requested = i2c_read_requested,
	.write_received = i2c_write_received,
	.read_processed = i2c_read_processed,
	.stop = i2c_stop_callback,
};

#define LED0_NODE DT_ALIAS(led0)

static const struct device* const i2c_dev = DEVICE_DT_GET(I2C_NODE);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

struct i2c_target_config i2c_config = {
	.flags = 0,
	.address = 0x32,
	.callbacks = &callbacks,
};


int main(void)
{

	k_sem_init(&i2c_target_sem, 0, 1);

	printf("Hello TI! %s\n", CONFIG_BOARD_TARGET);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	i2c_target_register(i2c_dev, &i2c_config);

	printk("Target ready\n");

	while(true){
		k_sem_take(&i2c_target_sem, K_FOREVER);
		if(timeoutDetected == 0){
			printk("Target finished transaction\n");
		} else {
			printk("Timeout Detected Target Reset\n");
			timeoutDetected = 0;
		}
	}
	while(1)

	return 0;
}
