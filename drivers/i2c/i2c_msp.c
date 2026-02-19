/*
 * Copyright (c) 2024 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_msp_i2c

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/dt-bindings/i2c/i2c.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/msp_clock_control.h>
#include <soc.h>

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_msp);

#include "i2c-priv.h"
#include "i2c_msp_compat.h"

#define I2C_TI_MSP_CONTROLLER_INTERRUPTS                                                           \
	(DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST | DL_I2C_INTERRUPT_CONTROLLER_NACK |         \
	 DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER | DL_I2C_INTERRUPT_CONTROLLER_STOP |           \
	 DL_I2C_INTERRUPT_CONTROLLER_TX_DONE | DL_I2C_INTERRUPT_CONTROLLER_TIMEOUT_A)

#define I2C_TI_MSP_TARGET_INTERRUPTS                                                               \
	(DL_I2C_INTERRUPT_TARGET_RX_DONE | DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY |                  \
	 DL_I2C_INTERRUPT_TARGET_START | DL_I2C_INTERRUPT_TARGET_STOP |                            \
	 DL_I2C_INTERRUPT_TARGET_TIMEOUT_A)

enum i2c_msp_state {
	I2C_MSP_IDLE,
	I2C_MSP_TX_STARTED,
	I2C_MSP_TX_INPROGRESS,
	I2C_MSP_TX_COMPLETE,
	I2C_MSP_RX_STARTED,
	I2C_MSP_RX_INPROGRESS,
	I2C_MSP_RX_COMPLETE,
	I2C_MSP_TARGET_STARTED,
	I2C_MSP_TARGET_TX_INPROGRESS,
	I2C_MSP_TARGET_RX_INPROGRESS,
	I2C_MSP_TARGET_PREEMPTED,
	I2C_MSP_TIMEOUT,
	I2C_MSP_ERROR,
};

struct i2c_msp_config {
	i2c_msp_base_t base;
	uint32_t bitrate;
	uint32_t merge_buf_size;
	uint8_t *merge_buf;
	i2c_msp_clock_config_t clock_config;
	const struct msp_sys_clock *clock_subsys;
	const struct pinctrl_dev_config *pinctrl;
	void (*irq_config_func)(const struct device *dev);
};

struct i2c_msp_data {
	struct k_sem lock;
	struct k_sem sync;
	uint32_t dev_config;
	volatile enum i2c_msp_state state;
	uint32_t transfer_count;
	uint32_t transfer_len;
	uint8_t *msg_buf;
#if defined(CONFIG_I2C_TARGET)
	struct i2c_target_config *target_config;
	const struct i2c_target_callbacks *target_callbacks;
#endif
	bool is_target;
};

#if (CONFIG_I2C_SCL_LOW_TIMEOUT != 0)
static int i2c_msp_configure_timeout(const struct device *dev, uint32_t period, uint32_t timeout_ms)
{
	const struct i2c_msp_config *config = dev->config;
	const struct device *clk_dev = DEVICE_DT_GET(DT_NODELABEL(ckm));
	uint32_t clock_rate;
	uint32_t tick_cycles;
	uint64_t timeout_cycles;
	uint32_t ticks_needed;
	uint16_t counter_value;
	int ret;

	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t)config->clock_subsys,
				     &clock_rate);
	if (ret < 0) {
		return ret;
	}

	/* Each count is equal to (1 + TPR) * 12 functional clocks */
	tick_cycles = (period + 1) * 12;
	timeout_cycles = (uint64_t)timeout_ms * (clock_rate / 1000);
	ticks_needed = (timeout_cycles + tick_cycles - 1) / tick_cycles;
	/* Lower 4 bits of counter are automatically set to 0x0 */
	counter_value = ticks_needed >> 4;

	if (counter_value > 0xFF) {
		/* Clamp to hardware maximum - longest possible timeout */
		counter_value = 0xFF;
	}

	DL_I2C_enableTimeoutA(config->base);
	DL_I2C_setTimeoutACount(config->base, counter_value);
	return 0;
}
#endif /* CONFIG_I2C_SCL_LOW_TIMEOUT != 0 */

static int i2c_msp_configure(const struct device *dev, uint32_t dev_config)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;
	const struct device *const clk_dev = DEVICE_DT_GET(DT_NODELABEL(ckm));
	uint32_t clock_rate;
	uint32_t desired_speed;
	int32_t period;
	int ret;

	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t)config->clock_subsys,
				     &clock_rate);
	if (ret < 0) {
		return -ENODEV;
	}

	if (dev_config & I2C_MSG_ADDR_10_BITS) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	switch (I2C_SPEED_GET(dev_config)) {
	case I2C_SPEED_STANDARD:
		desired_speed = 100000;
		break;
	case I2C_SPEED_FAST:
		desired_speed = 400000;
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Calculate the timer period based on the desired speed and clock rate.
	 * This works out to be ceil(clock_rate / (desired_speed * 10)) - 1
	 */
	period = clock_rate / (desired_speed * 10);
	period -= (clock_rate % (desired_speed * 10) == 0) ? 1 : 0;

	if (period <= 0) {
		ret = -EINVAL;
		goto out;
	}

	DL_I2C_setTimerPeriod(config->base, period);
	data->dev_config = dev_config;

#if (CONFIG_I2C_SCL_LOW_TIMEOUT != 0)
	ret = i2c_msp_configure_timeout(dev, period, CONFIG_I2C_SCL_LOW_TIMEOUT);
	if (ret < 0) {
		goto out;
	}
#endif

	DL_I2C_setControllerTXFIFOThreshold(config->base, DL_I2C_TX_FIFO_LEVEL_BYTES_1);
	DL_I2C_setControllerRXFIFOThreshold(config->base, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
	DL_I2C_enableControllerClockStretching(config->base);

	DL_I2C_enableControllerInterrupt(config->base, I2C_TI_MSP_CONTROLLER_INTERRUPTS);
	DL_I2C_enableController(config->base);
	ret = 0;

out:
	k_sem_give(&data->lock);
	return ret;
}

static int i2c_msp_init(const struct device *dev)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;
	uint32_t speed_config;
	int ret;

	k_sem_init(&data->lock, 1, 1);
	k_sem_init(&data->sync, 0, 1);

	DL_I2C_reset(config->base);
	DL_I2C_enablePower(config->base);
	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);
	DL_I2C_resetControllerTransfer(config->base);

#ifdef CONFIG_I2C_TARGET
	/* Workaround for errata I2C_ERR_04 */
	DL_I2C_disableTargetWakeup(config->base);
#endif

	ret = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	DL_I2C_setClockConfig(config->base, (i2c_msp_clock_config_t *)&config->clock_config);
	DL_I2C_disableAnalogGlitchFilter(config->base);

	speed_config = i2c_map_dt_bitrate(config->bitrate);
	ret = i2c_msp_configure(dev, speed_config);
	if (ret < 0) {
		return ret;
	}

	config->irq_config_func(dev);

	return 0;
}

static int i2c_msp_get_config(const struct device *dev, uint32_t *dev_config)
{
	struct i2c_msp_data *data = dev->data;

	*dev_config = data->dev_config;
	return 0;
}

static void i2c_msp_reset_peripheral_controller(const struct device *dev)
{
	const struct i2c_msp_config *config = dev->config;

	DL_I2C_reset(config->base);
	DL_I2C_disablePower(config->base);
	DL_I2C_enablePower(config->base);
	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);

#ifdef CONFIG_I2C_TARGET
	DL_I2C_disableTargetWakeup(config->base);
#endif

	DL_I2C_setClockConfig(config->base, (i2c_msp_clock_config_t *)&config->clock_config);
	DL_I2C_disableAnalogGlitchFilter(config->base);

	DL_I2C_resetControllerTransfer(config->base);

	DL_I2C_setControllerTXFIFOThreshold(config->base, DL_I2C_TX_FIFO_LEVEL_BYTES_1);
	DL_I2C_setControllerRXFIFOThreshold(config->base, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
	DL_I2C_enableControllerClockStretching(config->base);

	DL_I2C_clearControllerInterruptStatus(config->base, I2C_TI_MSP_CONTROLLER_INTERRUPTS);
	DL_I2C_enableControllerInterrupt(config->base, I2C_TI_MSP_CONTROLLER_INTERRUPTS);
	DL_I2C_enableController(config->base);
}

/*
 * Wait for a controller transfer to complete.  Returns 0 on success,
 * -ETIMEDOUT on hardware or software timeout, -EIO on bus error.
 */
static int i2c_msp_wait_completion(const struct device *dev)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;

#if (CONFIG_I2C_SCL_LOW_TIMEOUT != 0)
	int ret = k_sem_take(&data->sync, K_MSEC(CONFIG_I2C_SCL_LOW_TIMEOUT + 100));

	if (ret != 0) {
		DL_I2C_disableControllerInterrupt(config->base, I2C_TI_MSP_CONTROLLER_INTERRUPTS);
		DL_I2C_clearControllerInterruptStatus(config->base,
						      I2C_TI_MSP_CONTROLLER_INTERRUPTS);
		data->state = I2C_MSP_IDLE;
		return -ETIMEDOUT;
	}
#else
	k_sem_take(&data->sync, K_FOREVER);
#endif

	if (data->state == I2C_MSP_TIMEOUT) {
		return -ETIMEDOUT;
	}

	if (((DL_I2C_getControllerStatus(config->base) & DL_I2C_CONTROLLER_STATUS_ERROR) != 0) ||
	    (data->state == I2C_MSP_ERROR)) {
		return -EIO;
	}

	return 0;
}

static int i2c_msp_receive(const struct device *dev, struct i2c_msg *msg, uint16_t addr)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;
	DL_I2C_CONTROLLER_STOP stop;

	data->msg_buf = msg->buf;
	data->transfer_count = 0;
	data->transfer_len = msg->len;
	data->state = I2C_MSP_RX_STARTED;

	if (msg->flags & I2C_MSG_STOP) {
		stop = DL_I2C_CONTROLLER_STOP_ENABLE;
	} else {
		stop = DL_I2C_CONTROLLER_STOP_DISABLE;
	}

	DL_I2C_startControllerTransferAdvanced(config->base, addr, DL_I2C_CONTROLLER_DIRECTION_RX,
					       data->transfer_len, DL_I2C_CONTROLLER_START_ENABLE,
					       stop, DL_I2C_CONTROLLER_ACK_DISABLE);

	return i2c_msp_wait_completion(dev);
}

static int i2c_msp_transmit(const struct device *dev, struct i2c_msg *msg, uint16_t addr)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;
	DL_I2C_CONTROLLER_STOP stop;

	data->msg_buf = msg->buf;
	data->transfer_count = 0;
	data->transfer_len = msg->len;
	data->state = I2C_MSP_IDLE;

	DL_I2C_flushControllerTXFIFO(config->base);

	/* Fill the 8-byte deep FIFO; returns number of bytes written */
	data->transfer_count = DL_I2C_fillControllerTXFIFO(config->base, data->msg_buf, msg->len);

	if (data->transfer_count < data->transfer_len) {
		DL_I2C_enableControllerInterrupt(config->base,
						 DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_TRIGGER);
	} else {
		DL_I2C_disableControllerInterrupt(config->base,
						  DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_TRIGGER);
	}

	if (msg->flags & I2C_MSG_STOP) {
		stop = DL_I2C_CONTROLLER_STOP_ENABLE;
	} else {
		stop = DL_I2C_CONTROLLER_STOP_DISABLE;
	}

	data->state = I2C_MSP_TX_STARTED;
	DL_I2C_startControllerTransferAdvanced(config->base, addr, DL_I2C_CONTROLLER_DIRECTION_TX,
					       data->transfer_len, DL_I2C_CONTROLLER_START_ENABLE,
					       stop, DL_I2C_CONTROLLER_ACK_ENABLE);

	return i2c_msp_wait_completion(dev);
}

static int i2c_msp_transfer(const struct device *dev, struct i2c_msg *msgs, uint8_t num_msgs,
			    uint16_t addr)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;
	uint32_t saved_config;
	uint8_t *internal_buf = config->merge_buf;
	uint32_t merge_buf_size = config->merge_buf_size;
	uint32_t current_internal_buf_size = 0;
	uint8_t *transaction_buf;
	uint32_t transaction_len;
	struct i2c_msg transaction_msg;
	int ret = 0;

	k_sem_take(&data->lock, K_FOREVER);

	if (data->is_target) {
		k_sem_give(&data->lock);
		return -EBUSY;
	}

	/* Transmit each message */
	for (int i = 0; i < num_msgs; i++) {
		bool merge_next =
			(i + 1 < num_msgs) && !(msgs[i].flags & I2C_MSG_STOP) &&
			!(msgs[i + 1].flags & I2C_MSG_RESTART) &&
			((msgs[i].flags & I2C_MSG_WRITE) == (msgs[i + 1].flags & I2C_MSG_WRITE));

		if (merge_next || current_internal_buf_size != 0) {
			if ((current_internal_buf_size + msgs[i].len) > merge_buf_size) {
				LOG_ERR("Merge buffer too small (%u + %u > %u)",
					current_internal_buf_size, msgs[i].len, merge_buf_size);
				ret = -ENOSPC;
				break;
			}
			if (!(msgs[i].flags & I2C_MSG_READ)) {
				memcpy(internal_buf + current_internal_buf_size, msgs[i].buf,
				       msgs[i].len);
			}
			current_internal_buf_size += msgs[i].len;
		}

		/* Continue merging messages */
		if (merge_next) {
			continue;
		}

		/* No merge was performed, send standard frame */
		if (current_internal_buf_size == 0) {
			transaction_buf = msgs[i].buf;
			transaction_len = msgs[i].len;
		} else {
			transaction_buf = internal_buf;
			transaction_len = current_internal_buf_size;
		}
		/* Fill transaction_msg */
		transaction_msg.flags = msgs[i].flags;
		transaction_msg.buf = transaction_buf;
		transaction_msg.len = transaction_len;

		if ((msgs[i].flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) {
			ret = i2c_msp_receive(dev, &transaction_msg, addr);
		} else {
			ret = i2c_msp_transmit(dev, &transaction_msg, addr);
		}

		if (ret != 0) {
			break;
		}

		/* For merged reads, copy data back to original user buffers */
		if ((transaction_msg.flags & I2C_MSG_READ) && (transaction_buf == internal_buf)) {
			int j = i;

			while (j >= 0 && current_internal_buf_size >= msgs[j].len) {
				current_internal_buf_size -= msgs[j].len;
				memcpy(msgs[j].buf, transaction_buf + current_internal_buf_size,
				       msgs[j].len);
				j--;
			}
		}
		/* Safe to reset internal buffer size back to 0 at this point */
		current_internal_buf_size = 0;
	}

	if (ret == -ETIMEDOUT) {
		i2c_msp_get_config(dev, &saved_config);
		i2c_msp_reset_peripheral_controller(dev);
	}

	k_sem_give(&data->lock);

	if (ret == -ETIMEDOUT) {
		i2c_msp_configure(dev, saved_config);
	}

	return ret;
}

#ifdef CONFIG_I2C_TARGET
static int i2c_msp_target_register(const struct device *dev, struct i2c_target_config *cfg)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;

	/* Device is already registered as target */
	if (data->is_target || data->target_config == cfg) {
		return -EINVAL;
	}

	data->target_config = cfg;
	data->target_callbacks = cfg->callbacks;

	k_sem_take(&data->lock, K_FOREVER);

	if (data->state == I2C_MSP_TARGET_PREEMPTED) {
		DL_I2C_clearTargetInterruptStatus(config->base, I2C_TI_MSP_TARGET_INTERRUPTS);
	}

	DL_I2C_disableController(config->base);
	DL_I2C_disableControllerInterrupt(config->base, I2C_TI_MSP_CONTROLLER_INTERRUPTS);

	DL_I2C_setTargetTXFIFOThreshold(config->base, DL_I2C_TX_FIFO_LEVEL_BYTES_1);
	DL_I2C_setTargetRXFIFOThreshold(config->base, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
	DL_I2C_enableTargetTXTriggerInTXMode(config->base);
	DL_I2C_enableTargetTXEmptyOnTXRequest(config->base);
	DL_I2C_enableTargetClockStretching(config->base);
	DL_I2C_setTargetOwnAddress(config->base, cfg->address);

	DL_I2C_clearTargetInterruptStatus(config->base, DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY);
	DL_I2C_enableTargetInterrupt(config->base, I2C_TI_MSP_TARGET_INTERRUPTS);

	/* Set target flag before enabling hardware to avoid ISR race */
	data->is_target = true;
	data->dev_config &= ~I2C_MODE_CONTROLLER;
	data->state = I2C_MSP_IDLE;

	DL_I2C_enableTarget(config->base);

	k_sem_give(&data->lock);
	return 0;
}

static int i2c_msp_target_unregister(const struct device *dev, struct i2c_target_config *cfg)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;

	if (!data->is_target) {
		return 0;
	}

	k_sem_take(&data->lock, K_FOREVER);

	DL_I2C_disableTarget(config->base);
	DL_I2C_disableTargetInterrupt(config->base, I2C_TI_MSP_TARGET_INTERRUPTS);

	data->target_config = NULL;
	data->target_callbacks = NULL;
	data->is_target = false;

	k_sem_give(&data->lock);
	return 0;
}

static void i2c_msp_reset_peripheral_target(const struct device *dev)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;

	DL_I2C_reset(config->base);
	DL_I2C_disablePower(config->base);
	DL_I2C_enablePower(config->base);
	delay_cycles(CONFIG_MSP_PERIPH_STARTUP_DELAY);

	DL_I2C_disableTargetWakeup(config->base);

	DL_I2C_setClockConfig(config->base, (i2c_msp_clock_config_t *)&config->clock_config);
	DL_I2C_disableAnalogGlitchFilter(config->base);

	DL_I2C_setTargetOwnAddress(config->base, data->target_config->address);
	DL_I2C_setTargetTXFIFOThreshold(config->base, DL_I2C_TX_FIFO_LEVEL_BYTES_1);
	DL_I2C_setTargetRXFIFOThreshold(config->base, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
	DL_I2C_enableTargetTXTriggerInTXMode(config->base);
	DL_I2C_enableTargetTXEmptyOnTXRequest(config->base);

	DL_I2C_clearTargetInterruptStatus(config->base, DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY);
	DL_I2C_enableTargetInterrupt(config->base, I2C_TI_MSP_TARGET_INTERRUPTS);

	data->state = I2C_MSP_IDLE;
	DL_I2C_enableTarget(config->base);
}

static void i2c_msp_isr_target(const struct device *dev)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;
	uint8_t tx_byte;
	uint8_t rx_byte;
	int ret;

	switch (DL_I2C_getTargetPendingInterrupt(config->base)) {
	case DL_I2C_IIDX_TARGET_START:
		data->state = I2C_MSP_TARGET_STARTED;
		DL_I2C_flushTargetTXFIFO(config->base);
		break;

	case DL_I2C_IIDX_TARGET_RX_DONE:
		if (data->state == I2C_MSP_TARGET_STARTED) {
			data->state = I2C_MSP_TARGET_RX_INPROGRESS;
			if (data->target_callbacks->write_requested != NULL) {
				ret = data->target_callbacks->write_requested(data->target_config);
				DL_I2C_setTargetACKOverrideValue(
					config->base,
					ret == 0 ? DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_ACK
						 : DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_NACK);
			}
		}
	}
	/* Store received data in buffer */
	if (data->target_callbacks->write_received != NULL) {
		while (!DL_I2C_isTargetRXFIFOEmpty(config->base)) {
			rx_byte = DL_I2C_receiveTargetData(config->base);
			ret = data->target_callbacks->write_received(data->target_config, rx_byte);
			DL_I2C_setTargetACKOverrideValue(
				config->base, ret == 0
						      ? DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_ACK
						      : DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_NACK);
		}
	} else {
		DL_I2C_receiveTargetData(config->base);
		DL_I2C_setTargetACKOverrideValue(config->base,
						 DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_NACK);
	}
	break;

case DL_I2C_IIDX_TARGET_TXFIFO_EMPTY:
	if (data->state == I2C_MSP_TARGET_STARTED) {
		data->state = I2C_MSP_TARGET_TX_INPROGRESS;
		if (data->target_callbacks->read_requested != NULL) {
			ret = data->target_callbacks->read_requested(data->target_config, &tx_byte);
			DL_I2C_transmitTargetData(config->base, ret == 0 ? tx_byte : 0x00);
		} else {
			/* read_requested function is not found. The target data will
			 * continue to transmit to fulfill the error and not hang
			 * the controller by stretching indefinitely
			 */
			DL_I2C_transmitTargetDataCheck(config->base, 0xFF);
		}
	} else {
		/* still using the FIFO, we call read_processed in order to add
		 * additional data rather than from a buffer. If the write-received
		 * function chooses to return 0 (no more data present), then 0's will
		 * be filled in
		 */
		if (data->target_callbacks->read_processed != NULL) {
			ret = data->target_callbacks->read_processed(data->target_config, &tx_byte);
			DL_I2C_transmitTargetData(config->base, ret == 0 ? tx_byte : 0x00);
		} else {
			DL_I2C_transmitTargetDataCheck(config->base, 0xFF);
		}
	}
	break;

case DL_I2C_IIDX_TARGET_STOP:
	data->state = I2C_MSP_IDLE;
	if (data->target_callbacks->stop) {
		data->target_callbacks->stop(data->target_config);
	}
	break;

case DL_I2C_IIDX_TARGET_TIMEOUT_A:
	DL_I2C_disableTargetInterrupt(config->base, I2C_TI_MSP_TARGET_INTERRUPTS);
	DL_I2C_clearTargetInterruptStatus(config->base, I2C_TI_MSP_TARGET_INTERRUPTS);
	if (data->target_callbacks->stop) {
		data->target_callbacks->stop(data->target_config);
	}
	i2c_msp_reset_peripheral_target(dev);
	k_sem_give(&data->lock);
	break;

default:
	break;
}
}
#endif /* CONFIG_I2C_TARGET */

static void i2c_msp_isr_controller(const struct device *dev)
{
	const struct i2c_msp_config *config = dev->config;
	struct i2c_msp_data *data = dev->data;

	switch (DL_I2C_getControllerPendingInterrupt(config->base)) {
	case DL_I2C_IIDX_CONTROLLER_STOP:
		if (data->state == I2C_MSP_RX_INPROGRESS) {
			data->state = I2C_MSP_RX_COMPLETE;
			k_sem_give(&data->sync);
		}
		break;

	case DL_I2C_IIDX_CONTROLLER_TX_DONE:
		DL_I2C_disableControllerInterrupt(config->base,
						  DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_TRIGGER);
		data->state = I2C_MSP_TX_COMPLETE;
		k_sem_give(&data->sync);
		break;

	case DL_I2C_IIDX_CONTROLLER_RXFIFO_TRIGGER:
		/* Receive all bytes from target */
		data->state = I2C_MSP_RX_INPROGRESS;
		while (!DL_I2C_isControllerRXFIFOEmpty(config->base)) {
			if (data->transfer_count < data->transfer_len) {
				data->msg_buf[data->transfer_count++] =
					DL_I2C_receiveControllerData(config->base);
			} else {
				/* Ignore if transaction length exceeded */
				DL_I2C_receiveControllerData(config->base);
			}
		}
		break;

	case DL_I2C_IIDX_CONTROLLER_TXFIFO_TRIGGER:
		data->state = I2C_MSP_TX_INPROGRESS;
		/* Fill TX FIFO with next bytes to send */
		if (data->transfer_count < data->transfer_len) {
			data->transfer_count += DL_I2C_fillControllerTXFIFO(
				config->base, &data->msg_buf[data->transfer_count],
				data->transfer_len - data->transfer_count);
		}
		break;

	case DL_I2C_IIDX_CONTROLLER_NACK:
		if ((data->state == I2C_MSP_RX_STARTED) || (data->state == I2C_MSP_TX_STARTED)) {
			/* NACK interrupt if I2C Target is disconnected */
			data->state = I2C_MSP_ERROR;
			k_sem_give(&data->sync);
		}
		break;

	case DL_I2C_IIDX_CONTROLLER_TIMEOUT_A:
		data->state = I2C_MSP_TIMEOUT;
		k_sem_give(&data->sync);
		DL_I2C_disableControllerInterrupt(config->base, I2C_TI_MSP_CONTROLLER_INTERRUPTS);
		DL_I2C_clearControllerInterruptStatus(config->base,
						      I2C_TI_MSP_CONTROLLER_INTERRUPTS);
		DL_I2C_flushControllerTXFIFO(config->base);
		break;

	default:
		break;
	}
}

static void i2c_msp_isr(const struct device *dev)
{
#if defined(CONFIG_I2C_TARGET)
	struct i2c_msp_data *data = dev->data;

	if (data->is_target) {
		i2c_msp_isr_target(dev);
		return;
	}
#endif
	i2c_msp_isr_controller(dev);
}

static DEVICE_API(i2c, i2c_msp_driver_api) = {
	.configure = i2c_msp_configure,
	.get_config = i2c_msp_get_config,
	.transfer = i2c_msp_transfer,
#ifdef CONFIG_I2C_RTIO
	.iodev_submit = i2c_iodev_submit_fallback,
#endif
#ifdef CONFIG_I2C_TARGET
	.target_register = i2c_msp_target_register,
	.target_unregister = i2c_msp_target_unregister,
#endif
};

/* Instance-level helper macros */
#define MERGE_BUF_SIZE(index)                                                                      \
	COND_CODE_1(DT_NODE_HAS_PROP(DT_NODELABEL(i2c##index), merge_buf_size),                    \
		    (DT_PROP(DT_NODELABEL(i2c##index), merge_buf_size)), (0))
#define USES_MERGE_BUF(index) COND_CODE_0(MERGE_BUF_SIZE(index), (0), (1))

#define I2C_MSP_IRQ_FUNC_DECLARE(index)                                                            \
	static void i2c_msp_irq_config_func_##index(const struct device *dev)

#define I2C_MSP_IRQ_FUNC(index)                                                                    \
	static void i2c_msp_irq_config_func_##index(const struct device *dev)                      \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), i2c_msp_isr,        \
			    DEVICE_DT_INST_GET(index), 0);                                         \
		irq_enable(DT_INST_IRQN(index));                                                   \
	}

#define I2C_MSP_INIT(index)                                                                        \
                                                                                                   \
	PINCTRL_DT_INST_DEFINE(index);                                                             \
                                                                                                   \
	static const struct msp_sys_clock msp_i2c_clock_sys_##index =                              \
		MSP_CLOCK_SUBSYS_FN(index);                                                        \
                                                                                                   \
	I2C_MSP_IRQ_FUNC_DECLARE(index);                                                           \
                                                                                                   \
	IF_ENABLED(CONFIG_HAS_MSP_UNICOMM,                                                        \
	(static UNICOMM_Inst_Regs i2c_msp_uc_regs_##index = {                                     \
		.inst = (UNICOMM_Regs *)DT_INST_REG_ADDR(index),                                  \
		.i2cc = (UNICOMMI2CC_Regs *)                                                       \
			UC_I2CC_BASE(DT_INST_REG_ADDR(index)),                                    \
		.i2ct = (UNICOMMI2CT_Regs *)                                                       \
			UC_I2CT_BASE(DT_INST_REG_ADDR(index)),                                    \
		.fixedMode = DT_CHILD_NUM(                                                         \
			DT_PARENT(DT_DRV_INST(index))) == 1,                                      \
	};))                                                                                       \
                                                                                                   \
	IF_ENABLED(USES_MERGE_BUF(index),                                                         \
		(static uint8_t                                                                    \
		 msp_i2c_merge_buf_##index[MERGE_BUF_SIZE(index)];))                               \
                                                                                                   \
	static const struct i2c_msp_config i2c_msp_cfg_##index = {                                 \
		.base = COND_CODE_1(CONFIG_HAS_MSP_UNICOMM,                                       \
			(&i2c_msp_uc_regs_##index),                                                \
			((I2C_Regs *)DT_INST_REG_ADDR(index))),                                   \
		.clock_subsys = &msp_i2c_clock_sys_##index,                                        \
		.bitrate = DT_INST_PROP(index, clock_frequency),                                   \
		.merge_buf_size = MERGE_BUF_SIZE(index),                                           \
		IF_ENABLED(USES_MERGE_BUF(index),                                                  \
			(.merge_buf = msp_i2c_merge_buf_##index,))                                 \
		.pinctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(index),                                  \
		.irq_config_func = i2c_msp_irq_config_func_##index,                                \
		.clock_config = {                                                                  \
			.clockSel = MSP_CLOCK_PERIPH_REG_MASK(                                     \
				DT_INST_CLOCKS_CELL(index, clk)),                                  \
			.divideRatio = DL_I2C_CLOCK_DIVIDE_1,                                      \
		},                                                                                 \
	};                                                                                         \
                                                                                                   \
	static struct i2c_msp_data i2c_msp_data_##index;                                           \
                                                                                                   \
	I2C_DEVICE_DT_INST_DEFINE(index, i2c_msp_init, NULL,                                       \
				  &i2c_msp_data_##index,                                           \
				  &i2c_msp_cfg_##index,                                            \
				  POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,                           \
				  &i2c_msp_driver_api);                                            \
                                                                                                   \
	I2C_MSP_IRQ_FUNC(index)

DT_INST_FOREACH_STATUS_OKAY(I2C_MSP_INIT)
