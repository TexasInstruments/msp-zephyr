/*
 * Copyright (c) 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/fuel_gauge.h>

#define STACKSIZE 2048
#define PRIORITY 7

const struct device *const fuel_gauge = DEVICE_DT_GET_ONE(ti_bq27z746);
K_MUTEX_DEFINE(i2c_mutex);
K_MUTEX_DEFINE(serial_mutex);

void read_bq27Z746_current()
{
	union fuel_gauge_prop_val prop;
	
	while(1) {
		k_mutex_lock(&i2c_mutex, K_FOREVER);
		fuel_gauge_get_prop(fuel_gauge, FUEL_GAUGE_AVG_CURRENT, &prop);
		k_mutex_unlock(&i2c_mutex);

		k_mutex_lock(&serial_mutex, K_FOREVER);
		printk("Current reading: %d mA\n", prop.avg_current);
		k_mutex_unlock(&serial_mutex);

		k_msleep(500);
	}
}

void read_bq27Z746_voltage()
{
	union fuel_gauge_prop_val prop;
	while(1) {
		k_mutex_lock(&i2c_mutex, K_FOREVER);
		fuel_gauge_get_prop(fuel_gauge, FUEL_GAUGE_VOLTAGE, &prop);
		k_mutex_unlock(&i2c_mutex);

		k_mutex_lock(&serial_mutex, K_FOREVER);
		printk("Voltage reading: %d mV\n", prop.voltage);
		k_mutex_unlock(&serial_mutex);

		k_msleep(500);
	}
}

K_THREAD_DEFINE(read_bq27Z746_current_id, STACKSIZE, read_bq27Z746_current, NULL, NULL, NULL,
		PRIORITY, 0, 0);
K_THREAD_DEFINE(read_bq27Z746_voltage_id, STACKSIZE, read_bq27Z746_voltage, NULL, NULL, NULL,
		PRIORITY, 0, 0);