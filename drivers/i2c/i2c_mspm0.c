/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_mspm0_i2c

/* Zephyr includes */
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/mspm0_clock_control.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/dt-bindings/i2c/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <soc.h>

#if (CONFIG_USE_MSPM0_DL_UNICOMM_I2CC == 1) || (CONFIG_USE_MSPM0_DL_UNICOMM_I2CT == 1)
#define MSPM0_UNICOMM 1
#else
#define MSPM0_UNICOMM 0
#endif

LOG_MODULE_REGISTER(i2c_mspm0, CONFIG_I2C_LOG_LEVEL);
#include "i2c-priv.h"


/* TI DriverLib includes */
#ifdef CONFIG_USE_MSPM0_DL_UNICOMM_I2CC
#include <driverlib/dl_unicommi2cc.h>
#define I2C_API(func) DL_I2CC_##func
#define I2C_CONST(name) DL_I2CC_##name

#define I2C_CONTROLLER(name) DL_I2CC_##name
#define I2C_CONTROLLER_FXN(verb, noun) DL_I2CC_##verb##noun

#define I2C_CONTROLLER_INT(name) DL_I2CC_INTERRUPT_##name
#define I2C_TARGET_INT(name) DL_I2CC_INTERRUPT_##name
#define I2C_INT(name) DL_I2CC_INTERRUPT_##name
#define I2C_CONTROLLER_IIDX(name) DL_I2CC_IIDX_##name
#define I2C_TARGET_IIDX(name) DL_I2CC_IIDX_##name
#define I2C_IIDX(name) DL_I2CC_IIDX_##name

#define I2C_TX_FIFO_LEVEL DL_I2CC_TX_FIFO_LEVEL_1_4_EMPTY
#define I2C_RX_FIFO_LEVEL DL_I2CC_RX_FIFO_LEVEL_1_4_FULL

/* patch while driverlib does not have the followign function */
void DL_I2CC_blockAsync(UNICOMM_Inst_Regs *unicomm){
	unicomm->inst->GPRCM.CLKCFG = (1 << 8) | (0xA9 << 24);
}

#elif CONFIG_USE_MSPM0_DL_UNICOMM_I2CT
#include <driverlib/dl_unicommi2ct.h>
#define I2C_API(func) DL_I2CT_##func
#define I2C_CONST(name) DL_I2CT_##name
#define I2C_CONTROLLER_INT(name) DL_I2CT_INTERRUPT_##name

#define I2C_TARGET_INT(name) DL_I2CT_INTERRUPT_##name
#define I2C_TARGET_FXN(verb,noun) DL_I2CT_##verb##noun

#define I2C_INT(name) DL_I2CT_INTERRUPT_##name
#define I2C_CONTROLLER_IIDX(name) DL_I2CT_IIDX_##name
#define I2C_TARGET_IIDX(name) DL_I2CT_IIDX_##name
#define I2C_IIDX(name) DL_I2CT_IIDX_##name

/* FIFOS */
#define I2C_TX_FIFO_LEVEL DL_I2CC_TX_FIFO_LEVEL_1_4_EMPTY
#define I2C_RX_FIFO_LEVEL DL_I2CC_RX_FIFO_LEVEL_1_4_FULL

/* patch while driverlib does not have the followign function */
void DL_I2CT_blockAsync(UNICOMM_Inst_Regs *unicomm){
	unicomm->inst->GPRCM.CLKCFG = (1 << 8) | (0xA9 << 24);
}

#else
#include <ti/driverlib/dl_i2c.h>
#define I2C_API(func) DL_I2C_##func
#define I2C_CONST(name) DL_I2C_##name
#define I2C_CONTROLLER(name) DL_I2C_CONTROLLER_##name
#define I2C_CONTROLLER_FXN(verb, noun) DL_I2C_##verb##Controller##noun
#define I2C_TARGET_FXN(verb, noun) DL_I2C_##verb##Target##noun

#define I2C_CONTROLLER_INT(name) DL_I2C_INTERRUPT_CONTROLLER_##name
#define I2C_TARGET_INT(name) DL_I2C_INTERRUPT_TARGET_##name
#define I2C_INT(name) DL_I2C_INTERRUPT_##name
#define I2C_CONTROLLER_IIDX(name) DL_I2C_IIDX_CONTROLLER_##name
#define I2C_TARGET_IIDX(name) DL_I2C_IIDX_TARGET_##name
#define I2C_IIDX(name) DL_I2C_IIDX_##name

/* FIFOS */
#define I2C_TX_FIFO_LEVEL DL_I2CC_TX_FIFO_LEVEL_1_4_EMPTY
#define I2C_RX_FIFO_LEVEL DL_I2CC_RX_FIFO_LEVEL_1_4_FULL


/* patch while driverlib does not have the followign function */
void DL_I2C_blockAsync(I2C_Regs * i2c){
	i2c->GPRCM.CLKCFG = (1 << 8) | (0xA9 << 24);
}

#endif /* CONFIG_HAS_MSP_UNICOMM*/


#define TI_MSPM0_CONTROLLER_INTERRUPTS                                                             \
	(I2C_CONTROLLER_INT(ARBITRATION_LOST) | I2C_CONTROLLER_INT(NACK) | \
	I2C_CONTROLLER_INT(RXFIFO_TRIGGER) | I2C_CONTROLLER_INT(RX_DONE) | \
	I2C_CONTROLLER_INT(TX_DONE) | I2C_INT(TIMEOUT_A)) | \
	I2C_CONTROLLER_INT(STOP)


#define TI_MSPM0_TARGET_INTERRUPTS                                                                 \
	(I2C_TARGET_INT(RX_DONE) | I2C_TARGET_INT(TXFIFO_EMPTY) |          \
	I2C_TARGET_INT(START) | I2C_TARGET_INT(STOP) |                     \
	I2C_INT(TIMEOUT_A))

enum i2c_mspm0_state {
	I2C_MSPM0_IDLE,
	I2C_MSPM0_TX_STARTED,
	I2C_MSPM0_TX_INPROGRESS,
	I2C_MSPM0_TX_COMPLETE,
	I2C_MSPM0_RX_STARTED,
	I2C_MSPM0_RX_INPROGRESS,
	I2C_MSPM0_RX_COMPLETE,
	I2C_MSPM0_TARGET_STARTED,
	I2C_MSPM0_TARGET_TX_INPROGRESS,
	I2C_MSPM0_TARGET_RX_INPROGRESS,
	I2C_MSPM0_TARGET_PREEMPTED,
	I2C_MSPM0_TIMEOUT,
	I2C_MSPM0_ERROR,
};

struct i2c_mspm0_config {
#ifdef CONFIG_HAS_MSP_UNICOMM
	UNICOMM_Inst_Regs *base;
#else
	I2C_Regs *base;
#endif
	uint32_t bitrate;
	uint32_t merge_buf_size;
	uint8_t *merge_buf;
	I2C_CONST(ClockConfig) clockConfig;
	const struct mspm0_sys_clock *clock_subsys;
	const struct pinctrl_dev_config *pinctrl;
	void (*irq_config_func)(const struct device *dev);
};

struct i2c_mspm0_data {
	uint32_t dev_config;
	volatile enum i2c_mspm0_state state;
	struct k_sem i2c_lock;
	struct k_sem device_sync_sem;
	uint32_t transfer_count;
	uint32_t transfer_len;
	uint8_t *msg_buf;
#if defined(CONFIG_I2C_TARGET) || defined(CONFIG_UNICOMM_I2CT)
	struct i2c_target_config *target_config;
	const struct i2c_target_callbacks *target_callbacks;
#endif /* CONFIG_I2C_TARGET || CONFIG_UNICOMM_I2CT */
	bool is_target;
};

#if CONFIG_I2C_SCL_LOW_TIMEOUT_MS != 0
static int i2c_mspm0_configure_timeout(const struct device *dev, uint32_t period,
				       uint32_t timeout_ms)
{
	const struct i2c_mspm0_config *config = dev->config;
	const struct device *clk_dev = DEVICE_DT_GET(DT_NODELABEL(ckm));
	uint32_t clock_rate;
	uint32_t tick_cycles;
	uint64_t timeout_cycles;
	uint32_t ticks_needed;
	uint16_t counter_value;
	int ret;

	ret = clock_control_get_rate(clk_dev, (struct mspm0_sys_clock *)config->clock_subsys,
				     &clock_rate);
	if (ret < 0) {
		return ret;
	}

	/* Each count is equal to (1 + TPR) * 12 functional clocks */
	tick_cycles = (period + 1) * 12;
	timeout_cycles = (uint64_t)timeout_ms * (clock_rate / 1000);
	ticks_needed = (timeout_cycles + tick_cycles - 1) / tick_cycles;
	/* Lower 4-bits of counter are automatically set to 0x0 */
	counter_value = ticks_needed >> 4;

	if (counter_value > 0xFF) {
		return -EINVAL;
	}

	I2C_API(enableTimeoutA)(config->base);
	I2C_API(setTimeoutACount)(config->base, counter_value);
	return 0;
}
#endif /* CONFIG_I2C_SCL_LOW_TIMEOUT_MS != 0 */

static int i2c_mspm0_configure(const struct device *dev, uint32_t dev_config)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;
	const struct device *const clk_dev = DEVICE_DT_GET(DT_NODELABEL(ckm));
	uint32_t clock_rate;
	uint32_t desired_speed;
	int32_t tpr_val;
	int ret;

	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t)config->clock_subsys,
				     &clock_rate);
	if (ret < 0) {
		return -ENODEV;
	}

	if (dev_config & I2C_MSG_ADDR_10_BITS) {
		return -EINVAL;
	}

	k_sem_take(&data->i2c_lock, K_FOREVER);

	/* Config I2C speed */
	switch (I2C_SPEED_GET(dev_config)) {
	case I2C_SPEED_STANDARD:
		desired_speed = 100000;
		break;
	case I2C_SPEED_FAST:
		desired_speed = 400000;
		break;
	case I2C_SPEED_FAST_PLUS:
		desired_speed = 1000000;
		break;
	default:
		ret = -EINVAL;
		goto sem_give;
	}

	/* Calculate the timer period based on the desired period and clock rate.
	 *
	 * Per TRM, the value stored in the Timer Period (TPR_Val) creates a
	 * period from the following equation:
	 * Desired Period = (1 + TPR_Val) X (10) X clock_rate
	 *
	 * Solving the above equation for TPR_VAL given a desired period yields
	 * the following:
	 *
	 * TPR_val = ceiling(clock_rate/(desiredSpeed*10)) - 1
	 *
	 * The ceiling is chosen such that a non-clean divide gives a period
	 * greater than the desired rather than lower. This means that the
	 * frequency will thus be lower, and the frequency bounds are considered
	 * preferred.
	 */
	tpr_val = DIV_ROUND_UP(clock_rate, (desired_speed * 10)) - 1;

	/* Set the I2C speed */
	if (tpr_val <= 0) {
		ret = -EINVAL;
		goto sem_give;
	}

	I2C_API(setTimerPeriod)(config->base, tpr_val);
	data->dev_config = dev_config;

#if CONFIG_I2C_SCL_LOW_TIMEOUT_MS != 0
	ret = i2c_mspm0_configure_timeout(dev, tpr_val, CONFIG_I2C_SCL_LOW_TIMEOUT_MS);
#endif /* CONFIG_I2C_SCL_LOW_TIMEOUT_MS */

	/* Config other settings */


	I2C_CONTROLLER_FXN(set,TXFIFOThreshold)(config->base, I2C_TX_FIFO_LEVEL);
	I2C_CONTROLLER_FXN(set,RXFIFOThreshold)(config->base, I2C_RX_FIFO_LEVEL);
	I2C_CONTROLLER_FXN(enable,ClockStretching)(config->base);

	/* Configure Interrupts */
	I2C_API(enableInterrupt)(config->base, TI_MSPM0_CONTROLLER_INTERRUPTS);

	/* Enable module */
	I2C_CONTROLLER_FXN(enable,)(config->base);

	/* disable async fast clock requests (not needed)
	 * this also allows I2C to enter low power modes.
	 */
	I2C_API(blockAsync)(config->base);

sem_give:
	k_sem_give(&data->i2c_lock);
	return ret;
}

static int i2c_mspm0_init(const struct device *dev)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;
	int ret;
	uint32_t speed_config;

	/* initialize semaphores */
	k_sem_init(&data->i2c_lock, 1, 1);
	k_sem_init(&data->device_sync_sem, 0, 1);

	/* Init power */
	I2C_API(reset)(config->base);
	I2C_API(enablePower)(config->base);
	delay_cycles(CONFIG_MSPM0_PERIPH_STARTUP_DELAY);
	I2C_CONTROLLER_FXN(reset,Transfer)(config->base);

#if CONFIG_I2C_TARGET && !MSPM0_UNICOMM
	/* Workaround for errata I2C_ERR_04 (non-unicomm only)*/
	I2C_TARGET_FXN(disable,Wakeup)(config->base);
#endif
	/* Init GPIO */
	ret = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	/* Config clocks and analog filter */
	I2C_API(setClockConfig)(config->base, &config->clockConfig);
	I2C_API(disableAnalogGlitchFilter)(config->base);

	/* Set frequency */
	speed_config = i2c_map_dt_bitrate(config->bitrate);
	i2c_mspm0_configure(dev, speed_config);

	/* Enable interrupts */
	config->irq_config_func(dev);

	return 0;
}

static int i2c_mspm0_get_config(const struct device *dev, uint32_t *dev_config)
{
	struct i2c_mspm0_data *data = dev->data;
	*dev_config = data->dev_config;

	return 0;
}

static int i2c_mspm0_reset_controller(const struct device *dev)
{
	const struct i2c_mspm0_config *config = dev->config;

	I2C_API(reset)(config->base);
	I2C_API(disablePower)(config->base);

	I2C_API(enablePower)(config->base);
	delay_cycles(CONFIG_MSPM0_PERIPH_STARTUP_DELAY);\

#if CONFIG_I2C_TARGET && !MSPM0_UNICOMM
	I2C_TARGET_FXN(disable,Wakeup)(config->base);
#endif
	/* Config clocks and analog filter */
	I2C_API(setClockConfig)(config->base, &config->clockConfig);
	I2C_API(disableAnalogGlitchFilter)(config->base);

	/* Configure Controller Mode */
	I2C_CONTROLLER_FXN(reset,Transfer)(config->base);

	/* Config other settings */

	I2C_CONTROLLER_FXN(set,TXFIFOThreshold)(config->base, I2C_TX_FIFO_LEVEL);
	I2C_CONTROLLER_FXN(set,RXFIFOThreshold)(config->base, I2C_RX_FIFO_LEVEL);
	I2C_CONTROLLER_FXN(enable,ClockStretching)(config->base);

	/* Configure Interrupts */
	I2C_API(clearInterruptStatus)(config->base, TI_MSPM0_CONTROLLER_INTERRUPTS);
	I2C_API(enableInterrupt)(config->base, TI_MSPM0_CONTROLLER_INTERRUPTS);

	/* Enable module */
	I2C_CONTROLLER_FXN(enable,)(config->base);

	/* disable async fast clock requests (not needed)
	 * this also allows I2C to enter low power modes.
	 */
	I2C_API(blockAsync)(config->base);

	return 0;
}

static int i2c_mspm0_receive(const struct device *dev, struct i2c_msg msg, uint16_t addr)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;
	I2C_CONTROLLER(STOP) stop;

	/* Send a read request to Target */
	data->msg_buf = msg.buf;
	data->transfer_count = 0;
	data->transfer_len = msg.len;
	data->state = I2C_MSPM0_RX_STARTED;

	if(msg.flags & I2C_MSG_STOP) {
		stop = I2C_CONTROLLER(STOP_ENABLE);
	}
	else {
		stop = I2C_CONTROLLER(STOP_DISABLE);
	}

	I2C_CONTROLLER_FXN(start,TransferAdvanced)(config->base, addr, I2C_CONTROLLER(DIRECTION_RX),
				data->transfer_len, I2C_CONTROLLER(START_ENABLE), stop,
				I2C_CONTROLLER(ACK_DISABLE));

	/* Wait for the read to complete */
	if (k_sem_take(&data->device_sync_sem, K_MSEC(CONFIG_I2C_TRANSFER_TIMEOUT_MS))) {
		return -ETIMEDOUT;
	}

	if (data->state == I2C_MSPM0_TIMEOUT) {
		return -ETIMEDOUT;
	}

	/* If error, return error */
	if ((((I2C_CONTROLLER_FXN(get,Status)(config->base)) & (I2C_CONTROLLER(STATUS_ERROR))) != 0x00) ||
	    (data->state == I2C_MSPM0_ERROR)) {
		return -EIO;
	}

	return 0;
}

static int i2c_mspm0_transmit(const struct device *dev, struct i2c_msg msg, uint16_t addr)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;
	I2C_CONTROLLER(STOP) stop;

	data->msg_buf = msg.buf;
	data->transfer_count = 0;
	data->transfer_len = msg.len;
	data->state = I2C_MSPM0_IDLE;

	/* Flush anything that is left in the stale FIFO */
	I2C_CONTROLLER_FXN(flush,TXFIFO)(config->base);

	/*  Fill the FIFO
	 *  The FIFO is 8-bytes deep, and this function will return number
	 *  of bytes written to FIFO
	 */
	data->transfer_count = I2C_CONTROLLER_FXN(fill,TXFIFO)(config->base, data->msg_buf, msg.len);

	/* Enable TXFIFO trigger interrupt if there are more bytes to send */
	if (data->transfer_count < data->transfer_len) {
		I2C_API(enableInterrupt)(config->base, I2C_CONTROLLER_INT(TXFIFO_TRIGGER));
	} else {
		I2C_API(disableInterrupt)(config->base, I2C_CONTROLLER_INT(TXFIFO_TRIGGER));
	}

	if(msg.flags & I2C_MSG_STOP) {
		stop = I2C_CONTROLLER(STOP_ENABLE);
	}
	else {
		stop = I2C_CONTROLLER(STOP_DISABLE);
	}

	data->state = I2C_MSPM0_TX_STARTED;
	I2C_CONTROLLER_FXN(start,TransferAdvanced)(config->base, addr, I2C_CONTROLLER(DIRECTION_TX),
				data->transfer_len, I2C_CONTROLLER(START_ENABLE), stop,
				I2C_CONTROLLER(ACK_ENABLE));

	/* Wait for the transmit to complete */
	if (k_sem_take(&data->device_sync_sem, K_MSEC(CONFIG_I2C_TRANSFER_TIMEOUT_MS))) {
		return -ETIMEDOUT;
	}

	if (data->state == I2C_MSPM0_TIMEOUT) {
		return -ETIMEDOUT;
	}

	if (((I2C_CONTROLLER_FXN(get,Status)(config->base) & I2C_CONTROLLER(STATUS_ERROR)) != 0x00) ||
	    (data->state == I2C_MSPM0_ERROR)) {
		return -EIO;
	}

	return 0;
}

static bool i2c_mspm0_is_merge_next(const struct i2c_msg *msgs, const uint8_t num_msgs, int i)
{
	bool additional_messages = (i + 1 < num_msgs);
	bool flag_allow = !(msgs[i].flags & I2C_MSG_STOP) && !(msgs[i + 1].flags & I2C_MSG_RESTART);
	bool same_dir = ((msgs[i].flags & I2C_MSG_READ) == (msgs[i + 1].flags & I2C_MSG_READ));

	return additional_messages && flag_allow && same_dir;
}

static int i2c_mspm0_transfer(const struct device *dev, struct i2c_msg *msgs, uint8_t num_msgs,
			      uint16_t addr)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;
	uint32_t dev_config;
	uint8_t *internal_buf = config->merge_buf;
	uint16_t merge_buf_size = config->merge_buf_size;
	uint16_t current_internal_buf_size = 0;
	uint8_t *transaction_buf;
	uint16_t transaction_len;
	struct i2c_msg transaction_msg;
	int ret = 0;

#ifdef CONFIG_PM_DEVICE
#ifdef CONFIG_PM_DEVICE_RUNTIME
	/* Prevent device from entering sleep state until transaction is complete */
	pm_device_runtime_get(dev);
#endif
	pm_policy_state_all_lock_get();
#endif
	k_sem_take(&data->i2c_lock, K_FOREVER);

	if (data->is_target) {
		/* Currently target is registered, initiating transfer is not allowed */
		k_sem_give(&data->i2c_lock);
#ifdef CONFIG_PM_DEVICE
#ifdef CONFIG_PM_DEVICE_RUNTIME
		pm_device_runtime_put(dev);
#endif
		/* Allow device to enter sleep state */
		pm_policy_state_all_lock_put();
#endif
		return -EBUSY;
	}

	/* Transmit each message */
	for (int i = 0; i < num_msgs; i++) {
		bool merge_next_msg = i2c_mspm0_is_merge_next(msgs, num_msgs, i);

		if (merge_next_msg || current_internal_buf_size != 0) {
			if ((current_internal_buf_size + msgs[i].len) > merge_buf_size) {
				LOG_ERR("Need to use the internal driver "
					"buffer but its size is insufficient "
					"(%u + %u > %u). ",
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
		if (merge_next_msg) {
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
			ret = i2c_mspm0_receive(dev, transaction_msg, addr);
		} else {
			ret = i2c_mspm0_transmit(dev, transaction_msg, addr);
		}

		if (ret != 0) {
			break;
		}

		/* For merged reads, populate data in original user buffers */
		if ((transaction_msg.flags & I2C_MSG_READ) && (transaction_buf == internal_buf)) {
			int j = i;

			while (current_internal_buf_size >= msgs[j].len) {
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
		i2c_mspm0_get_config(dev, &dev_config);
		i2c_mspm0_reset_controller(dev);
		i2c_mspm0_configure(dev, dev_config);
	}

	return ret;
}

#ifdef CONFIG_I2C_TARGET
static int i2c_mspm0_target_register(const struct device *dev, struct i2c_target_config *cfg)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;

	/* Device is already registered as target */
	if (data->is_target || data->target_config == cfg) {
		return -EBUSY;
	}

	data->target_config = cfg;
	data->target_callbacks = cfg->callbacks;
	k_sem_take(&data->i2c_lock, K_FOREVER);

	if (data->state == I2C_MSPM0_TARGET_PREEMPTED) {
		I2C_API(clearInterruptStatus)(config->base, TI_MSPM0_TARGET_INTERRUPTS);
	}

	I2C_CONTROLLER_FXN(disable,)(config->base);
	I2C_API(disableInterrupt)(config->base, TI_MSPM0_CONTROLLER_INTERRUPTS);

	I2C_TARGET_FXN(set,TXFIFOThreshold)(config->base, I2C_TX_FIFO_LEVEL);
	I2C_TARGET_FXN(set,RXFIFOThreshold)(config->base, I2C_RX_FIFO_LEVEL);
	I2C_TARGET_FXN(enable,TXTriggerInTXMode)(config->base);
	I2C_TARGET_FXN(enable,TXEmptyOnTXRequest)(config->base);
	I2C_TARGET_FXN(enable,ClockStretching)(config->base);
	I2C_TARGET_FXN(set,OwnAddress)(config->base, data->target_config->address);

	I2C_API(clearInterruptStatus)(config->base, I2C_TARGET_INT(TXFIFO_EMPTY));
	I2C_API(enableInterrupt)(config->base, TI_MSPM0_TARGET_INTERRUPTS);
	I2C_API(enableTarget)(config->base);

	data->dev_config &= ~I2C_MODE_CONTROLLER;
	data->is_target = true;
	data->state = I2C_MSPM0_IDLE;

#ifdef CONFIG_PM_DEVICE
	/* Allow operation only down to STOP0 for I2C targets */

	/* Lock STANDBY Operation */
	pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);

	/* Lock STOP1 and STOP2 Operation */
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, DL_SYSCTL_POWER_POLICY_STOP1);
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, DL_SYSCTL_POWER_POLICY_STOP2);
#endif

	k_sem_give(&data->i2c_lock);
	return 0;
}

static int i2c_mspm0_target_unregister(const struct device *dev, struct i2c_target_config *cfg)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;

	if (data->is_target == false) {
		return 0;
	}

	data->target_config = NULL;
	data->is_target = false;
	k_sem_take(&data->i2c_lock, K_FOREVER);

	I2C_API(disableTarget)(config->base);
	I2C_API(disableInterrupt)(config->base, TI_MSPM0_TARGET_INTERRUPTS);

#ifdef CONFIG_PM_DEVICE
	/* Allow STANDBY Operation */
	pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);

	/* Allow STOP1 and STOP2 Operation */
	pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, DL_SYSCTL_POWER_POLICY_STOP1);
	pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, DL_SYSCTL_POWER_POLICY_STOP2);
#endif

	k_sem_give(&data->i2c_lock);
	return 0;
}

static int i2c_mspm0_reset_target(const struct device *dev)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;

	I2C_API(reset)(config->base);
	I2C_API(disablePower)(config->base);

	I2C_API(enablePower)(config->base);
	delay_cycles(CONFIG_MSPM0_PERIPH_STARTUP_DELAY);

	I2C_TARGET_FXN(disable,Wakeup)(config->base);

	/* Config clocks and analog filter */
	I2C_API(setClockConfig)(config->base, (I2C_CONST(ClockConfig) *)&config->clockConfig);
	I2C_API(disableAnalogGlitchFilter)(config->base);

	I2C_TARGET_FXN(set,OwnAddress)(config->base, data->target_config->address);
#ifdef MSPM0_UNICOMM /* thresholds are not matches */
	I2C_API(setTXFIFOThreshold)(config->base, I2C_CONST(TX_FIFO_LEVEL_1_4_EMPTY));
	I2C_API(setRXFIFOThreshold)(config->base, I2C_CONST(RX_FIFO_LEVEL_1_4_FULL));
#else
	I2C_TARGET_FXN(set,TXFIFOThreshold)(config->base, I2C_CONST(TX_FIFO_LEVEL_BYTES_1));
	I2C_TARGET_FXN(set,RXFIFOThreshold)(config->base, I2C_CONST(RX_FIFO_LEVEL_BYTES_1));
#endif
	I2C_TARGET_FXN(enable,TXTriggerInTXMode)(config->base);
	I2C_TARGET_FXN(enable,TXEmptyOnTXRequest)(config->base);

	I2C_API(clearInterruptStatus)(config->base, I2C_TARGET_INT(TXFIFO_EMPTY));

	I2C_API(enableInterrupt)(config->base, TI_MSPM0_TARGET_INTERRUPTS);

	data->state = I2C_MSPM0_IDLE;

	/* Enable module */
	I2C_API(enableTarget)(config->base);

	return 0;
}

static void i2c_mspm0_isr_target(const struct device *dev)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;
	int ret;
	uint8_t txByte;
	uint8_t rxByte;

	switch (I2C_API(getPendingInterrupt)(config->base)) {
	case I2C_TARGET_IIDX(START):
		data->state = I2C_MSPM0_TARGET_STARTED;
		/* Flush TX FIFO to clear out any stale data */
		I2C_TARGET_FXN(flush,TXFIFO)(config->base);
		break;
	case I2C_TARGET_IIDX(RX_DONE):
		if (data->state == I2C_MSPM0_TARGET_STARTED) {
			data->state = I2C_MSPM0_TARGET_RX_INPROGRESS;
			if (data->target_callbacks->write_requested != NULL) {
				ret = data->target_callbacks->write_requested(data->target_config);
				if (ret == 0) {
					I2C_TARGET_FXN(set,ACKOverrideValue)(
						config->base,
						I2C_CONST(TARGET_RESPONSE_OVERRIDE_VALUE_ACK));
				} else {
					I2C_TARGET_FXN(set,ACKOverrideValue)(
						config->base,
						I2C_CONST(TARGET_RESPONSE_OVERRIDE_VALUE_NACK));
				}
			}
		}
		/* Store received data in buffer */
		if (data->target_callbacks->write_received != NULL) {
			while (I2C_TARGET_FXN(is,RXFIFOEmpty)(config->base) != true) {
				rxByte = I2C_TARGET_FXN(receive,Data)(config->base);
				ret = data->target_callbacks->write_received(data->target_config,
									     rxByte);
				if (ret == 0) {
					I2C_TARGET_FXN(set,ACKOverrideValue)(
						config->base,
						I2C_CONST(TARGET_RESPONSE_OVERRIDE_VALUE_ACK));
				} else {
					I2C_TARGET_FXN(set,ACKOverrideValue)(
						config->base,
						I2C_CONST(TARGET_RESPONSE_OVERRIDE_VALUE_NACK));
				}
			}
		} else {
			I2C_TARGET_FXN(receive,Data)(config->base);
			I2C_TARGET_FXN(set,ACKOverrideValue)(
				config->base, I2C_CONST(TARGET_RESPONSE_OVERRIDE_VALUE_NACK));
		}
		break;
	case I2C_TARGET_IIDX(TXFIFO_EMPTY):
		if (data->state == I2C_MSPM0_TARGET_STARTED) {
			/* First byte detected from a read request */
			data->state = I2C_MSPM0_TARGET_TX_INPROGRESS;
			if (data->target_callbacks->read_requested != NULL) {
				ret = data->target_callbacks->read_requested(data->target_config,
									     &txByte);
				if (ret == 0) {
					I2C_TARGET_FXN(transmit,Data)(config->base, txByte);
				} else {
					/* In this case, no new data is desired to be filled, thus
					 * 0's are transmitted
					 */
					I2C_TARGET_FXN(transmit,Data)(config->base, 0x00);
				}
			} else {
				/* read_requested function is not found. The target data will
				 * continue to transmit to fulfill the error and not hang
				 * the controller by stretching indefinitely
				 */
				I2C_TARGET_FXN(transmit,DataCheck)(config->base, 0xFF);
			}
		} else {
			/* still using the FIFO, we call read_processed in order to add
			 * additional data rather than from a buffer. If the write-received
			 * function chooses to return 0 (no more data present), then 0's will
			 * be filled in
			 */
			if (data->target_callbacks->read_processed != NULL) {
				ret = data->target_callbacks->read_processed(data->target_config,
									     &txByte);

				if (ret == 0) {
					I2C_TARGET_FXN(transmit,Data)(config->base, txByte);
				} else {
					/* In this case, no new data is desired to be filled, thus
					 * 0's are transmitted
					 */
					I2C_TARGET_FXN(transmit,Data)(config->base, 0x00);
				}
			} else {
				/* Read_processed function is not found. The target data will
				 * continue to transmit to fulfill the error and not hang
				 * the controller by stretching indefinitely
				 */
				I2C_TARGET_FXN(transmit,DataCheck)(config->base, 0xFF);
			}
		}
		break;
	case I2C_TARGET_IIDX(STOP):
		data->state = I2C_MSPM0_IDLE;
		if (data->target_callbacks->stop) {
			data->target_callbacks->stop(data->target_config);
		}
		break;
	case I2C_IIDX(TIMEOUT_A):
		I2C_API(disableInterrupt)(config->base, TI_MSPM0_TARGET_INTERRUPTS);
		I2C_API(clearInterruptStatus)(config->base, TI_MSPM0_TARGET_INTERRUPTS);
		if (data->target_callbacks->stop) {
			data->target_callbacks->stop(data->target_config);
		}
		i2c_mspm0_reset_target(dev);
		k_sem_give(&data->i2c_lock);
		break;
	default:
		break;
	}
}
#endif /* CONFIG_I2C_TARGET */

static inline void i2c_mspm0_isr_controller(const struct device *dev)
{
	const struct i2c_mspm0_config *config = dev->config;
	struct i2c_mspm0_data *data = dev->data;

	switch (I2C_API(getPendingInterrupt)(config->base)) {
	case I2C_CONTROLLER_IIDX(RX_DONE):
		data->state = I2C_MSPM0_RX_COMPLETE;
		k_sem_give(&data->device_sync_sem);
		break;
	case I2C_CONTROLLER_IIDX(TX_DONE):
		I2C_API(disableInterrupt)(config->base, I2C_CONTROLLER_INT(TXFIFO_TRIGGER));
		data->state = I2C_MSPM0_TX_COMPLETE;
		k_sem_give(&data->device_sync_sem);
		break;
	case I2C_CONTROLLER_IIDX(RXFIFO_TRIGGER):
		/* Receive all bytes from target */
		if (data->state != I2C_MSPM0_RX_COMPLETE) {
			data->state = I2C_MSPM0_RX_INPROGRESS;
		}
		while (I2C_CONTROLLER_FXN(is,RXFIFOEmpty)(config->base) != true) {
			if (data->transfer_count < data->transfer_len) {
				data->msg_buf[data->transfer_count++] =
					I2C_CONTROLLER_FXN(receive,Data)(config->base);
			} else {
				/* Ignore if transaction length exceeded */
				I2C_CONTROLLER_FXN(receive,Data)(config->base);
			}
		}
		break;
	case I2C_CONTROLLER_IIDX(TXFIFO_TRIGGER):
		data->state = I2C_MSPM0_TX_INPROGRESS;
		/* Fill TX FIFO with next bytes to send */
		if (data->transfer_count < data->transfer_len) {
			data->transfer_count += I2C_CONTROLLER_FXN(fill,TXFIFO)(
				config->base, &data->msg_buf[data->transfer_count],
				data->transfer_len - data->transfer_count);
		}
		break;
	case I2C_CONTROLLER_IIDX(NACK):
		if ((data->state == I2C_MSPM0_RX_STARTED) ||
		    (data->state == I2C_MSPM0_TX_STARTED)) {
			/* NACK interrupt if I2C Target is disconnected */
			data->state = I2C_MSPM0_ERROR;
			k_sem_give(&data->device_sync_sem);
		}
		break;
	case I2C_IIDX(TIMEOUT_A):
		data->state = I2C_MSPM0_TIMEOUT;
		k_sem_give(&data->device_sync_sem);
		I2C_API(disableInterrupt)(config->base, TI_MSPM0_CONTROLLER_INTERRUPTS);
		I2C_API(clearInterruptStatus)(config->base, TI_MSPM0_CONTROLLER_INTERRUPTS);
		I2C_CONTROLLER_FXN(flush,TXFIFO)(config->base);
	case I2C_CONTROLLER_IIDX(STOP):
		k_sem_give(&data->i2c_lock);
#ifdef CONFIG_PM_DEVICE
#ifdef CONFIG_PM_DEVICE_RUNTIME
		pm_device_runtime_put(dev);
#endif
		/* Allow device to enter sleep state once transaction is complete */
		pm_policy_state_all_lock_put();
#endif
		break;
	default:
		break;
	}
}

static inline void i2c_mspm0_isr(const struct device *dev)
{
	struct i2c_mspm0_data *data = dev->data;

	if (data->is_target == false) {
		i2c_mspm0_isr_controller(dev);
	}
#if defined(CONFIG_I2C_TARGET)
	if (data->is_target) {
		i2c_mspm0_isr_target(dev);
	}
#endif
}

static DEVICE_API(i2c, i2c_mspm0_driver_api) = {
	.configure = i2c_mspm0_configure,
	.get_config = i2c_mspm0_get_config,
	.transfer = i2c_mspm0_transfer,
#ifdef CONFIG_I2C_RTIO
	.iodev_submit = i2c_iodev_submit_fallback,
#endif
#ifdef CONFIG_I2C_TARGET
	.target_register = i2c_mspm0_target_register,
	.target_unregister = i2c_mspm0_target_unregister,
#endif
};

/* Macros to assist with the device-specific initialization */
#define MERGE_BUF_SIZE(index)                                                                      \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(index, merge_buf_size),                                  \
			(DT_INST_PROP(index, merge_buf_size)), (0))

#define USES_MERGE_BUF(index) COND_CODE_0(MERGE_BUF_SIZE(index), (0), (1))

#define I2C_MSPM0_CONFIG_IRQ_FUNC(index)                                                           \
	static void i2c_mspm0_irq_config_func_##index(const struct device *dev)                    \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), i2c_mspm0_isr,      \
			    DEVICE_DT_INST_GET(index), 0);                                         \
		irq_enable(DT_INST_IRQN(index));                                                   \
	}

#define MSP_I2C_INIT_FN(index)                                                                     \
	PINCTRL_DT_INST_DEFINE(index);                                                             \
	static const struct mspm0_sys_clock mspm0_i2c_clockSys##index =                            \
	MSPM0_CLOCK_SUBSYS_FN(index);                                                              \
	I2C_MSPM0_CONFIG_IRQ_FUNC(index)                                                           \
                                                                                                   \
	IF_ENABLED(CONFIG_USE_MSPM0_DL_UNICOMM_I2CC,                                               \
	(static UNICOMM_Inst_Regs i2c_mspm0_uc_regs_##index = {                                    \
		.inst =  (UNICOMM_Regs *)DT_INST_REG_ADDR(index),                                  \
		.i2cc = (UNICOMMI2CC_Regs *)UC_I2CC_BASE(DT_INST_REG_ADDR(index)),                 \
		.fixedMode = DT_CHILD_NUM(DT_PARENT(DT_DRV_INST(index))) == 1,                     \
	};)                                                                                        \
	)                                                                                          \
	IF_ENABLED(CONFIG_USE_MSPM0_DL_UNICOMM_I2CT,                                                            \
	(static UNICOMM_Inst_Regs i2c_mspm0_uc_regs_##index = {                                    \
		.inst =  (UNICOMM_Regs *)DT_INST_REG_ADDR(index),                                  \
		.i2ct = (UNICOMMI2CT_Regs *)UC_I2CT_BASE(DT_INST_REG_ADDR(index)),                 \
		.fixedMode = DT_CHILD_NUM(DT_PARENT(DT_DRV_INST(index))) == 1,                     \
	};)                                                                                        \
	)                                                                                          \
                                                                                                   \
	IF_ENABLED(USES_MERGE_BUF(index),                                                          \
		(static uint8_t mspm0_i2c_msg_buf_##index[MERGE_BUF_SIZE(index)];));               \
	static const struct i2c_mspm0_config i2c_mspm0_cfg_##index = {                             \
		.base = COND_CODE_1(CONFIG_HAS_MSP_UNICOMM,                                        \
			(&i2c_mspm0_uc_regs_##index), ((I2C_Regs *)DT_INST_REG_ADDR(index))),      \
		.clock_subsys = &mspm0_i2c_clockSys##index,                                        \
		.bitrate = DT_INST_PROP(index, clock_frequency),                                   \
		.merge_buf_size = MERGE_BUF_SIZE(index),                                           \
		IF_ENABLED(USES_MERGE_BUF(index), (                                                \
		.merge_buf = mspm0_i2c_msg_buf_##index,                                            \
		))                                                                                 \
		.pinctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(index),                                  \
		.irq_config_func = i2c_mspm0_irq_config_func_##index,                              \
		.clockConfig = {                                                                   \
			.clockSel = MSPM0_CLOCK_PERIPH_REG_MASK(DT_INST_CLOCKS_CELL(index, clk)),  \
			.divideRatio = I2C_CONST(CLOCK_DIVIDE_1),                                  \
		}                                                                                  \
	};                                                                                         \
	static struct i2c_mspm0_data i2c_mspm0_data_##index;                                       \
	I2C_DEVICE_DT_INST_DEFINE(index, i2c_mspm0_init, NULL, &i2c_mspm0_data_##index,            \
		&i2c_mspm0_cfg_##index, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,                     \
		&i2c_mspm0_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MSP_I2C_INIT_FN)
