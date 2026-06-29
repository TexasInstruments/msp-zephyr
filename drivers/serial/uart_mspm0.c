/*
 * Copyright (c) 2025-2026 Texas Instruments
 * Copyright (c) 2025 Linumiz
 * Copyright (c) 2025 Bang & Olufsen A/S, Denmark
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_mspm0_uart

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/mspm0_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/device.h>

/* Driverlib includes */
#ifdef CONFIG_HAS_MSP_UNICOMM
#include <ti/driverlib/dl_unicommuart.h>
#else
#include <ti/driverlib/dl_uart_main.h>
#endif /* CONFIG_HAS_MSP_UNICOMM */

/* Flags used to indicate active interrupt driven UART usage */
#define UART_PM_IRQ_TX		BIT(0)
#define UART_PM_IRQ_RX		BIT(1)

struct uart_mspm0_config {
#ifdef CONFIG_HAS_MSP_UNICOMM
	UNICOMM_Inst_Regs *regs;
#else
	UART_Regs *regs;
#endif /* CONFIG_HAS_MSP_UNICOM */
	const struct mspm0_sys_clock *clock_subsys;
	const struct pinctrl_dev_config *pinctrl;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	void (*irq_config_func)(const struct device *dev);
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

struct uart_mspm0_data {
	/* UART clock structure */
	DL_UART_Main_ClockConfig uart_clockconfig;
	/* Baud Rate */
	uint32_t current_speed;
	/* UART config structure */
	DL_UART_Main_Config uart_config;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	/* Callback function pointer */
	uart_irq_callback_user_data_t cb;
	/* Callback function arg */
	void *cb_data;
	/* Pending interrupt backup */
	DL_UART_IIDX pending_interrupt;
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
#ifdef CONFIG_PM
	/* Used to determine if peripheral should prevent MCU from entering LPM */
	uint8_t pm_activity;
	struct k_spinlock pm_lock;
#endif
};

#if defined(CONFIG_PM)

static int __maybe_unused uart_mspm_pm_activity_start(const struct device *dev, uint8_t activity)
{
	struct uart_mspm0_data *data = dev->data;
	k_spinlock_key_t key;

	/* Check if this is the first time we are setting a MCU PM lock*/
	bool first_activity = false;
	/* Current activity that is being tracked. */
	uint8_t old_activity;

	key = k_spin_lock(&data->pm_lock);

	old_activity = data->pm_activity;

	/* New activity is already active and tracked. Nothing to do */
	if (((old_activity & activity) != 0U)) {
		k_spin_unlock(&data->pm_lock, key);
		return 0;
	}

	/* No activity was previously active so this will be the first. */
	if(old_activity == 0U) {
		first_activity = true;
	}

	data->pm_activity = old_activity | activity;

	k_spin_unlock(&data->pm_lock, key);

	if(first_activity) {
		pm_policy_state_all_lock_get();
	}

	return 0;
}

static void __maybe_unused uart_mspm_pm_activity_stop(const struct device *dev, uint8_t activity)
{
	struct uart_mspm0_data *data = dev->data;
	k_spinlock_key_t key;

	/* Check if this is the first time we are setting a MCU PM lock*/
	uint8_t new_activity;
	/* Current activity that is being tracked. */
	uint8_t old_activity;

	key = k_spin_lock(&data->pm_lock);

	old_activity = data->pm_activity;

	/* New activity is not active or tracked. Nothing to do */
	if ((old_activity & activity) == 0U) {
		k_spin_unlock(&data->pm_lock, key);
		return;
	}

	new_activity = old_activity & ~activity;
	data->pm_activity = new_activity;

	k_spin_unlock(&data->pm_lock, key);

	if(new_activity == 0U) {
		pm_policy_state_all_lock_put();
	}

	return;
}
#endif

#if defined(CONFIG_PM_DEVICE)

static int uart_mspm_wait_until_idle(UART_Regs *regs, uint32_t baud_rate)
{
	/* Wait for a margin of 2 characters (20 bits total) */
	uint32_t bits_to_wait = 20;

	/* Formula: (Bits * 1,000,000) / Baud Rate = Microseconds */
	uint32_t timeout_us = (bits_to_wait * USEC_PER_SEC) / baud_rate;

	/* Enforce a minimum floor window of 1 millisecond */
	if (timeout_us < USEC_PER_MSEC) {
		timeout_us = USEC_PER_MSEC;
	}

	/* Dynamically wait for the calculated microsecond window */
	if (!WAIT_FOR(!DL_UART_isBusy(regs), timeout_us, k_busy_wait(1))) {
		return -EBUSY;
	}

	return 0;
}

static int uart_mspm_pm_action(const struct device *dev, enum pm_device_action action)
{
	int err;
	const struct uart_mspm0_config *config = dev->config;
	struct uart_mspm0_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		/* Set pins to active state */
		err = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_DEFAULT);
		if (err < 0) {
			return err;
		}

		/* Restore selection of parent clock that may been previously gated */
		config->regs->CLKSEL = data->uart_clockconfig.clockSel;

		break;
	case PM_DEVICE_ACTION_SUSPEND:
		/* Check if the line clears using our dynamic timeout helper */
		err = uart_mspm_wait_until_idle(config->regs, data->current_speed);
		if (err < 0) {
			return err; /* Automatically bubbles up -EBUSY and aborts suspend */
		}

		/* Clear the RX FIFO in case something was received while transitioning
		 * to suspend.
		 */
		while(!DL_UART_isRXFIFOEmpty(config->regs)) {
			DL_UART_receiveData(config->regs);
		}

		/* Unselect all parent clocks effectively gating the entire peripheral */
		config->regs->CLKSEL = 0;

		/* Move pins to sleep state */
		err = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_SLEEP);
		if ((err < 0) && (err != -ENOENT)) {
			/*
			 * If returning -ENOENT, no pins where defined for sleep mode :
			 * Do not output on console (might sleep already) when going to sleep,
			 * "UART pinctrl sleep state not available"
			 * and don't block PM suspend.
			 * Else return the error.
			 */
			return err;
		}

		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}
#endif

static int uart_mspm0_poll_in(const struct device *dev, unsigned char *c)
{
	const struct uart_mspm0_config *config = dev->config;

	if (DL_UART_Main_receiveDataCheck(config->regs, c) == false) {
		return -1;
	}

	return 0;
}

static void uart_mspm0_poll_out(const struct device *dev, unsigned char c)
{
	const struct uart_mspm0_config *config = dev->config;

	while (DL_UART_Main_isTXFIFOFull(config->regs)) {
	}

	DL_UART_Main_transmitData(config->regs, c);
}

static int uart_mspm0_install_configuration(const struct device *dev)
{
	const struct device *clk_dev = DEVICE_DT_GET(DT_NODELABEL(ckm));
	const struct uart_mspm0_config *config = dev->config;
	struct uart_mspm0_data *data = dev->data;
	uint32_t clock_rate;
	int ret;

	/* Set UART configs */
	DL_UART_Main_setClockConfig(config->regs, &data->uart_clockconfig);
	DL_UART_Main_init(config->regs, &data->uart_config);

	/*
	 * Configure baud rate by setting oversampling and baud rate divisor
	 * from the selected current-speed
	 */
	ret = clock_control_get_rate(clk_dev, (struct mspm0_sys_clock *)config->clock_subsys,
				     &clock_rate);
	if (ret < 0) {
		return ret;
	}

	DL_UART_Main_configBaudRate(config->regs, clock_rate, data->current_speed);

	return 0;
}

#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE

static const uint32_t uart_parity_to_mspm0[5] = {
	DL_UART_MAIN_PARITY_NONE,      DL_UART_MAIN_PARITY_ODD,        DL_UART_MAIN_PARITY_EVEN,
	DL_UART_MAIN_PARITY_STICK_ONE, DL_UART_MAIN_PARITY_STICK_ZERO,
};

static const uint32_t uart_stop_bits_to_mspm0[4] = {
	UINT32_MAX,
	DL_UART_MAIN_STOP_BITS_ONE,
	UINT32_MAX,
	DL_UART_MAIN_STOP_BITS_TWO,
};

static const uint32_t uart_data_bits_to_mspm0[4] = {
	DL_UART_MAIN_WORD_LENGTH_5_BITS,
	DL_UART_MAIN_WORD_LENGTH_6_BITS,
	DL_UART_MAIN_WORD_LENGTH_7_BITS,
	DL_UART_MAIN_WORD_LENGTH_8_BITS,
};

static const uint32_t uart_flow_control_to_mspm0[2] = {
	DL_UART_MAIN_FLOW_CONTROL_NONE,
	DL_UART_MAIN_FLOW_CONTROL_RTS_CTS,
};

static int uart_mspm0_translate_in(const uint32_t value_array[], int value_array_length,
				   uint8_t uart_cfg_value, uint32_t *mspm0_cfg_value)
{
	if (uart_cfg_value >= value_array_length) {
		return -EINVAL;
	}

	if (value_array[uart_cfg_value] == UINT32_MAX) {
		return -ENOSYS;
	}

	*mspm0_cfg_value = value_array[uart_cfg_value];

	return 0;
}

static int uart_mspm0_translate_out(const uint32_t value_array[], int value_array_length,
				    uint32_t mspm0_cfg_value, uint8_t *uart_cfg_value)
{
	int idx;

	for (idx = 0; idx < value_array_length; idx++) {
		if (value_array[idx] == mspm0_cfg_value) {
			break;
		}
	}

	if (idx == value_array_length || value_array[idx] == UINT32_MAX) {
		return -EINVAL;
	}

	*uart_cfg_value = (uint8_t)idx;

	return 0;
}

static int uart_mspm0_configure(const struct device *dev, const struct uart_config *cfg)
{
	const struct uart_mspm0_config *config = dev->config;
	struct uart_mspm0_data *data = dev->data;
	uint32_t value;
	int ret;

	DL_UART_Main_disable(config->regs);

	data->current_speed = cfg->baudrate;

	ret = uart_mspm0_translate_in(uart_parity_to_mspm0, ARRAY_SIZE(uart_parity_to_mspm0),
				      cfg->parity, &value);
	if (ret != 0) {
		return ret;
	}

	data->uart_config.parity = value;

	ret = uart_mspm0_translate_in(uart_stop_bits_to_mspm0, ARRAY_SIZE(uart_stop_bits_to_mspm0),
				      cfg->stop_bits, &value);
	if (ret != 0) {
		return ret;
	}

	data->uart_config.stopBits = value;

	ret = uart_mspm0_translate_in(uart_data_bits_to_mspm0, ARRAY_SIZE(uart_data_bits_to_mspm0),
				      cfg->data_bits, &value);
	if (ret != 0) {
		return ret;
	}

	data->uart_config.wordLength = value;

	ret = uart_mspm0_translate_in(uart_flow_control_to_mspm0,
				      ARRAY_SIZE(uart_flow_control_to_mspm0), cfg->flow_ctrl,
				      &value);
	if (ret != 0) {
		return ret;
	}

	data->uart_config.flowControl = value;

	ret = uart_mspm0_install_configuration(dev);
	if (ret != 0) {
		return ret;
	}

	DL_UART_Main_enable(config->regs);

	return 0;
}

static int uart_mspm0_config_get(const struct device *dev, struct uart_config *cfg)
{
	struct uart_mspm0_data *data = dev->data;
	int ret;

	cfg->baudrate = data->current_speed;

	ret = uart_mspm0_translate_out(uart_parity_to_mspm0, ARRAY_SIZE(uart_parity_to_mspm0),
				       data->uart_config.parity, &cfg->parity);
	if (ret != 0) {
		return ret;
	}

	ret = uart_mspm0_translate_out(uart_stop_bits_to_mspm0, ARRAY_SIZE(uart_stop_bits_to_mspm0),
				       data->uart_config.stopBits, &cfg->stop_bits);
	if (ret != 0) {
		return ret;
	}

	ret = uart_mspm0_translate_out(uart_data_bits_to_mspm0, ARRAY_SIZE(uart_data_bits_to_mspm0),
				       data->uart_config.wordLength, &cfg->data_bits);
	if (ret != 0) {
		return ret;
	}

	ret = uart_mspm0_translate_out(uart_flow_control_to_mspm0,
				       ARRAY_SIZE(uart_flow_control_to_mspm0),
				       data->uart_config.flowControl, &cfg->flow_ctrl);
	if (ret != 0) {
		return ret;
	}

	return 0;
}
#endif /* CONFIG_UART_USE_RUNTIME_CONFIGURE */

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int uart_mspm0_err_check(const struct device *dev)
{
	struct uart_mspm0_data *data = dev->data;

	switch (data->pending_interrupt) {
	case DL_UART_MAIN_IIDX_BREAK_ERROR:
		return UART_BREAK;
	case DL_UART_MAIN_IIDX_FRAMING_ERROR:
		return UART_ERROR_FRAMING;
	default:
		return 0;
	}
}

#define UART_MSPM0_TX_INTERRUPTS (DL_UART_MAIN_INTERRUPT_TX | DL_UART_MAIN_INTERRUPT_EOT_DONE)

#define UART_MSPM0_RX_INTERRUPTS                                                                   \
	(DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR)

static int uart_mspm0_fifo_fill(const struct device *dev, const uint8_t *tx_data, int size)
{
	const struct uart_mspm0_config *config = dev->config;

	return (int)DL_UART_Main_fillTXFIFO(config->regs, (uint8_t *)tx_data, size);
}

static int uart_mspm0_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	const struct uart_mspm0_config *config = dev->config;

	return (int)DL_UART_Main_drainRXFIFO(config->regs, rx_data, size);
}

static void uart_mspm0_irq_tx_enable(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;

#ifdef CONFIG_PM
	uart_mspm_pm_activity_start(dev,UART_PM_IRQ_TX);
#endif

	DL_UART_Main_enableInterrupt(config->regs, UART_MSPM0_TX_INTERRUPTS);
}

static void uart_mspm0_irq_tx_disable(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;
	DL_UART_Main_disableInterrupt(config->regs, UART_MSPM0_TX_INTERRUPTS);

#ifdef CONFIG_PM
	uart_mspm_pm_activity_stop(dev,UART_PM_IRQ_TX);
#endif
}

static int uart_mspm0_irq_tx_ready(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;
	struct uart_mspm0_data *data = dev->data;

	return ((data->pending_interrupt == DL_UART_MAIN_IIDX_TX) ||
		(data->pending_interrupt == DL_UART_MAIN_IIDX_EOT_DONE)) &&
			       !DL_UART_Main_isTXFIFOFull(config->regs)
		       ? 1
		       : 0;
}

static void uart_mspm0_irq_rx_enable(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;

#ifdef CONFIG_PM
	uart_mspm_pm_activity_start(dev,UART_PM_IRQ_RX);
#endif

	DL_UART_Main_enableInterrupt(config->regs, UART_MSPM0_RX_INTERRUPTS);
}

static void uart_mspm0_irq_rx_disable(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;

	DL_UART_Main_disableInterrupt(config->regs, UART_MSPM0_RX_INTERRUPTS);

#ifdef CONFIG_PM
	uart_mspm_pm_activity_stop(dev,UART_PM_IRQ_RX);
#endif
}

static int uart_mspm0_irq_tx_complete(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;

	return (DL_UART_Main_isTXFIFOEmpty(config->regs)) ? 1 : 0;
}

static int uart_mspm0_irq_rx_ready(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;
	struct uart_mspm0_data *data = dev->data;

	return ((data->pending_interrupt == DL_UART_MAIN_IIDX_RX) ||
		(data->pending_interrupt == DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR)) &&
			       !DL_UART_Main_isRXFIFOEmpty(config->regs)
		       ? 1
		       : 0;
}

static int uart_mspm0_irq_is_pending(const struct device *dev)
{
	struct uart_mspm0_data *data = dev->data;

	return data->pending_interrupt != DL_UART_MAIN_IIDX_NO_INTERRUPT;
}

static int uart_mspm0_irq_update(const struct device *dev)
{
	struct uart_mspm0_data *data = dev->data;
	const struct uart_mspm0_config *config = dev->config;

	data->pending_interrupt = DL_UART_Main_getPendingInterrupt(config->regs);

	return 1;
}

static void uart_mspm0_irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb,
					void *cb_data)
{
	struct uart_mspm0_data *const dev_data = dev->data;

	/* Set callback function and data */
	dev_data->cb = cb;
	dev_data->cb_data = cb_data;
}

#define UART_MSPM0_ERROR_INTERRUPTS                                                                \
	(DL_UART_MAIN_INTERRUPT_BREAK_ERROR | DL_UART_MAIN_INTERRUPT_FRAMING_ERROR)

static void uart_mspm0_irq_error_enable(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;

	DL_UART_Main_enableInterrupt(config->regs, UART_MSPM0_ERROR_INTERRUPTS);
}

static void uart_mspm0_irq_error_disable(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;

	DL_UART_Main_disableInterrupt(config->regs, UART_MSPM0_ERROR_INTERRUPTS);
}

static void uart_mspm0_isr(const struct device *dev)
{
	struct uart_mspm0_data *const dev_data = dev->data;

	/* Perform callback if defined */
	if (dev_data->cb) {
		dev_data->cb(dev, dev_data->cb_data);
	}
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int uart_mspm0_init(const struct device *dev)
{
	const struct uart_mspm0_config *config = dev->config;
	int ret;

	/* Reset power */
	DL_UART_Main_reset(config->regs);
	DL_UART_Main_enablePower(config->regs);
	delay_cycles(CONFIG_MSPM0_PERIPH_STARTUP_DELAY);

	/* Init UART pins */
	ret = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	ret = uart_mspm0_install_configuration(dev);
	if (ret != 0) {
		return ret;
	}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	DL_UART_Main_enableFIFOs(config->regs);
	DL_UART_Main_setRXFIFOThreshold(config->regs, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
	DL_UART_Main_setTXFIFOThreshold(config->regs, DL_UART_TX_FIFO_LEVEL_EMPTY);
	DL_UART_Main_setRXInterruptTimeout(config->regs, 15U);
	config->irq_config_func(dev);
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

	/* Enable UART */
	DL_UART_Main_enable(config->regs);

	return 0;
}

static DEVICE_API(uart, uart_mspm0_driver_api) = {
	.poll_in = uart_mspm0_poll_in,
	.poll_out = uart_mspm0_poll_out,
#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	.configure = uart_mspm0_configure,
	.config_get = uart_mspm0_config_get,
#endif /* CONFIG_UART_USE_RUNTIME_CONFIGURE */
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.err_check = uart_mspm0_err_check,
	.fifo_fill = uart_mspm0_fifo_fill,
	.fifo_read = uart_mspm0_fifo_read,
	.irq_tx_enable = uart_mspm0_irq_tx_enable,
	.irq_tx_disable = uart_mspm0_irq_tx_disable,
	.irq_tx_ready = uart_mspm0_irq_tx_ready,
	.irq_rx_enable = uart_mspm0_irq_rx_enable,
	.irq_rx_disable = uart_mspm0_irq_rx_disable,
	.irq_tx_complete = uart_mspm0_irq_tx_complete,
	.irq_rx_ready = uart_mspm0_irq_rx_ready,
	.irq_is_pending = uart_mspm0_irq_is_pending,
	.irq_update = uart_mspm0_irq_update,
	.irq_callback_set = uart_mspm0_irq_callback_set,
	.irq_err_enable = uart_mspm0_irq_error_enable,
	.irq_err_disable = uart_mspm0_irq_error_disable,
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
#define MSP_UART_IRQ_DEFINE(inst)                                                                  \
	static void uart_mspm0_##inst##_irq_register(const struct device *dev)                     \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), uart_mspm0_isr,       \
			    DEVICE_DT_INST_GET(inst), 0);                                          \
		irq_enable(DT_INST_IRQN(inst));                                                    \
	}
#else
#define MSP_UART_IRQ_DEFINE(inst)
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

#define MSPM0_MAIN_CLK_DIV(n) CONCAT(DL_UART_MAIN_CLOCK_DIVIDE_RATIO_, DT_INST_PROP(n, clk_div))

#define MSPM0_UART_INIT_FN(index)                                                                  \
                                                                                                   \
	PINCTRL_DT_INST_DEFINE(index);                                                             \
                                                                                                   \
	static const struct mspm0_sys_clock mspm0_uart_sys_clock##index =                          \
		MSPM0_CLOCK_SUBSYS_FN(index);		                                           \
                                                                                                   \
	MSP_UART_IRQ_DEFINE(index);                                                                \
                                                                                                   \
	IF_ENABLED(CONFIG_HAS_MSP_UNICOMM,                                                         \
	(static UNICOMM_Inst_Regs uart_mspm0_uc_regs_##index = {                                   \
		.inst =  (UNICOMM_Regs *)DT_INST_REG_ADDR(index),                                  \
		.uart = (UNICOMMUART_Regs *)UC_UART_BASE(DT_INST_REG_ADDR(index)),                 \
		.fixedMode = DT_CHILD_NUM(DT_PARENT(DT_DRV_INST(index))) == 1,                     \
	};))                                                                                       \
                                                                                                   \
	static const struct uart_mspm0_config uart_mspm0_cfg_##index = {                           \
		.regs = COND_CODE_1(CONFIG_HAS_MSP_UNICOMM,                                        \
			(&uart_mspm0_uc_regs_##index), ((UART_Regs *)DT_INST_REG_ADDR(index))),    \
			 .pinctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(index),                         \
			 .clock_subsys = &mspm0_uart_sys_clock##index,                             \
			 IF_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN,                                  \
			   (.irq_config_func = uart_mspm0_##index##_irq_register,)) };             \
                                                                                                   \
	static struct uart_mspm0_data uart_mspm0_data_##index = {                                  \
		.uart_clockconfig =                                                                \
			{                                                                          \
				.clockSel = MSPM0_CLOCK_PERIPH_REG_MASK(                           \
					DT_INST_CLOCKS_CELL(index, clk)),                          \
				.divideRatio = MSPM0_MAIN_CLK_DIV(index),                          \
			},                                                                         \
		.current_speed = DT_INST_PROP(index, current_speed),                               \
		.uart_config =                                                                     \
			{                                                                          \
				.mode = DL_UART_MAIN_MODE_NORMAL,                                  \
				.direction = DL_UART_MAIN_DIRECTION_TX_RX,                         \
				.flowControl = (DT_INST_PROP(index, hw_flow_control)               \
							? DL_UART_MAIN_FLOW_CONTROL_RTS_CTS        \
							: DL_UART_MAIN_FLOW_CONTROL_NONE),         \
				.parity = DL_UART_MAIN_PARITY_NONE,                                \
				.wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,                     \
				.stopBits = DL_UART_MAIN_STOP_BITS_ONE,                            \
			},                                                                         \
	};                                                                                         \
	PM_DEVICE_DT_INST_DEFINE(index, uart_mspm_pm_action);			\
                                                                                                   \
	DEVICE_DT_INST_DEFINE(index, &uart_mspm0_init, PM_DEVICE_DT_INST_GET(index), &uart_mspm0_data_##index,             \
			      &uart_mspm0_cfg_##index, PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY,  \
			      &uart_mspm0_driver_api)

DT_INST_FOREACH_STATUS_OKAY(MSPM0_UART_INIT_FN)
