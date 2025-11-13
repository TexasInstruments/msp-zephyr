/*
 * Copyright (c) 2024 Bang & Olufsen A/S, Denmark
 * Copyright (c) 2025 Linumiz GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_mspm0_spi

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/msp_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

/* TI DriverLib includes */
#ifdef CONFIG_HAS_MSP_UNICOMM
#include <driverlib/dl_unicommspi.h>
#else
#include <driverlib/dl_spi.h>
#endif

LOG_MODULE_REGISTER(spi_msp, CONFIG_SPI_LOG_LEVEL);

/* This must be included after log module registration */
#include "spi_context.h"

/* Data Frame Size (DFS) */
#define SPI_DFS_8BIT		1
#define SPI_DFS_16BIT		2

/* Range for SPI Serial Clock Rate (SCR) */
#define MSP_SPI_SCR_MIN	0
#define MSP_SPI_SCR_MAX	1023

/* Delay after enabling power for SPI module */
#define POWER_STARTUP_DELAY	16

#define SPI_DT_CLK_DIV(inst)									\
	DT_INST_PROP(inst, ti_clk_div)

#define SPI_DT_CLK_DIV_ENUM(inst)								\
	_CONCAT(DL_SPI_CLOCK_DIVIDE_RATIO_, SPI_DT_CLK_DIV(inst))

#define SPI_MODE(operation)									\
	(operation & BIT(0) ? DL_SPI_MODE_PERIPHERAL : DL_SPI_MODE_CONTROLLER)

#define BIT_ORDER_MODE(operation)								\
	(operation & BIT(4) ? DL_SPI_BIT_ORDER_LSB_FIRST : DL_SPI_BIT_ORDER_MSB_FIRST)

/* MSP DSS field expects word size - 1 */
#define DATA_SIZE_MODE(operation)								\
	(SPI_WORD_SIZE_GET(operation) - 1)

/* Computes the minimum number of bytes per frame using word_size. Utilizes ceil
 * division, to ensure sufficient byte allocation for non-multiples of 8 bits
 */
#define BYTES_PER_FRAME(word_size) (((word_size) + 7) / 8)

struct spi_msp_config {
#ifdef CONFIG_HAS_MSP_UNICOMM
	UNICOMM_Inst_Regs *spi_base;
#else
	SPI_Regs *spi_base;
#endif
	const struct pinctrl_dev_config *pinctrl;
	const DL_SPI_CHIP_SELECT hw_cs;
	const DL_SPI_ClockConfig clock_config;
	const struct msp_sys_clock *clock_subsys;
	void (*irq_config_func)(const struct device *dev);
};

struct spi_msp_data {
	struct spi_context spi_ctx;
	struct k_sem spi_idle;
};

static void spi_msp_isr(const struct device *dev)
{
	const struct spi_msp_config *config = dev->config;
	struct spi_msp_data *data = dev->data;

	if (DL_SPI_getPendingInterrupt(config->spi_base) == DL_SPI_IIDX_IDLE) {
		DL_SPI_disableInterrupt(config->spi_base, DL_SPI_INTERRUPT_IDLE);
		k_sem_give(&data->spi_idle);
	}
}

static int spi_msp_configure(const struct device *dev, const struct spi_config *spi_cfg,
			       uint8_t dfs)
{
	const struct spi_msp_config *const config = dev->config;
	struct spi_msp_data *const data = dev->data;
	const struct device *clk_dev = DEVICE_DT_GET(DT_NODELABEL(ckm));

	uint32_t clock_rate;
	uint16_t clock_scr;
	DL_SPI_FRAME_FORMAT ff;
	int ret;

	if (spi_context_configured(&data->spi_ctx, spi_cfg)) {
		/* This configuration is already in use */
		return 0;
	}

	/* Only master mode is supported */
	if (SPI_OP_MODE_GET(spi_cfg->operation) != SPI_OP_MODE_MASTER) {
		return -ENOTSUP;
	}

	/* Half duplex mode is not supported */
	if (spi_cfg->operation == SPI_HALF_DUPLEX) {
		return -ENOTSUP;
	}

	if (IS_ENABLED(CONFIG_SPI_EXTENDED_MODES) &&
		(spi_cfg->operation & SPI_LINES_MASK) != SPI_LINES_SINGLE) {
		LOG_ERR("Only single line mode is supported");
		return -EINVAL;
	}

	ret = clock_control_get_rate(clk_dev, (struct msp_sys_clock *)config->clock_subsys,
				     &clock_rate);
	if (ret < 0) {
		return ret;
	}

	if (spi_cfg->frequency > (clock_rate / 2)) {
		return -EINVAL;
	}

	/* See DL_SPI_setBitRateSerialClockDivider for details */
	clock_scr = (clock_rate / (2 * spi_cfg->frequency)) - 1;
	if (!IN_RANGE(clock_scr, MSP_SPI_SCR_MIN, MSP_SPI_SCR_MAX)) {
		return -EINVAL;
	}

	if(spi_cfg->operation & SPI_FRAME_FORMAT_TI) {
		ff = DL_SPI_FRAME_FORMAT_TI_SYNC;
	}
	else {
		if((spi_cfg->operation & SPI_MODE_CPOL) && (spi_cfg->operation & SPI_MODE_CPHA)) {
			ff = config->hw_cs == DL_SPI_CHIP_SELECT_NONE ? DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA1\
														  :	DL_SPI_FRAME_FORMAT_MOTO4_POL1_PHA1;
		}
		else if((spi_cfg->operation & SPI_MODE_CPOL)) {
			ff = config->hw_cs == DL_SPI_CHIP_SELECT_NONE ? DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA0\
														  :	DL_SPI_FRAME_FORMAT_MOTO4_POL1_PHA0;
		}
		else if((spi_cfg->operation & SPI_MODE_CPHA)) {
			ff = config->hw_cs == DL_SPI_CHIP_SELECT_NONE ? DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA1\
														  :	DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA1;
		}
		else {
			ff = config->hw_cs == DL_SPI_CHIP_SELECT_NONE ? DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0\
														  :	DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0;
		}
	}

	const DL_SPI_Config dl_spi_cfg = {
		.parity = DL_SPI_PARITY_NONE,				/* Currently unused in zephyr */
		.chipSelectPin = config->hw_cs,
		.mode = SPI_MODE(spi_cfg->operation),
		.bitOrder = BIT_ORDER_MODE(spi_cfg->operation),
		.dataSize = DATA_SIZE_MODE(spi_cfg->operation),
		.frameFormat = ff
	};

	/* Peripheral should be disabled before applying a new configuration */
	DL_SPI_disable(config->spi_base);

	DL_SPI_init(config->spi_base, (DL_SPI_Config *)&dl_spi_cfg);
	DL_SPI_setBitRateSerialClockDivider(config->spi_base, (uint32_t)clock_scr);

	if (dfs > SPI_DFS_16BIT) {
#ifdef CONFIG_HAS_MSP_UNICOMM
		return -ENOTSUP;
#else
		DL_SPI_enablePacking(config->spi_base);
#endif
	}

	if (SPI_MODE_GET(spi_cfg->operation) & SPI_MODE_LOOP) {
		DL_SPI_enableLoopbackMode(config->spi_base);
	} else {
		DL_SPI_disableLoopbackMode(config->spi_base);
	}

	DL_SPI_enable(config->spi_base);

	/* Cache SPI config for reuse, required by spi_context owner */
	data->spi_ctx.config = spi_cfg;

	return 0;
}

static void spi_msp_frame_tx(const struct device *dev, uint8_t dfs)
{
	const struct spi_msp_config *config = dev->config;
	struct spi_msp_data *data = dev->data;

	/* Transmit dummy frame when no TX data is provided */
	uint32_t tx_frame = 0;

	k_sem_reset(&data->spi_idle);

	if (spi_context_tx_buf_on(&data->spi_ctx)) {
		if (dfs == SPI_DFS_8BIT) {
			tx_frame = UNALIGNED_GET((uint8_t *)(data->spi_ctx.tx_buf));
		} else if (dfs == SPI_DFS_16BIT) {
			tx_frame = UNALIGNED_GET((uint16_t *)(data->spi_ctx.tx_buf));
		} else {
			tx_frame = UNALIGNED_GET((uint32_t *)(data->spi_ctx.tx_buf));
		}
	}
	DL_SPI_transmitDataCheck32(config->spi_base, tx_frame);

	DL_SPI_enableInterrupt(config->spi_base, DL_SPI_INTERRUPT_IDLE);
	k_sem_take(&data->spi_idle, K_FOREVER);

	spi_context_update_tx(&data->spi_ctx, dfs, 1);
}

static void spi_msp_frame_rx(const struct device *dev, uint8_t dfs)
{
	const struct spi_msp_config *config = dev->config;
	struct spi_msp_data *data = dev->data;
#ifdef CONFIG_HAS_MSP_UNICOMM
	uint16_t rx_val = 0;
#else
	uint32_t rx_val = 0;
#endif

#ifdef CONFIG_HAS_MSP_UNICOMM
	DL_SPI_receiveDataCheck16(config->spi_base, &rx_val);
#else
	DL_SPI_receiveDataCheck32(config->spi_base, &rx_val);
#endif

	if (!spi_context_rx_buf_on(&data->spi_ctx)) {
		return;
	}

	if (dfs == SPI_DFS_8BIT) {
		UNALIGNED_PUT((uint8_t)rx_val, (uint8_t *)data->spi_ctx.rx_buf);
	} else if (dfs == SPI_DFS_16BIT) {
		UNALIGNED_PUT((uint16_t)rx_val, (uint16_t *)data->spi_ctx.rx_buf);
	} else {
		UNALIGNED_PUT(rx_val, (uint32_t *)data->spi_ctx.rx_buf);
	}

	spi_context_update_rx(&data->spi_ctx, dfs, 1);
}

static void spi_msp_start_transfer(const struct device *dev, uint8_t dfs)
{
	struct spi_msp_data *data = dev->data;

	spi_context_cs_control(&data->spi_ctx, true);

	while (spi_context_tx_on(&data->spi_ctx) || spi_context_rx_on(&data->spi_ctx)) {
		spi_msp_frame_tx(dev, dfs);
		spi_msp_frame_rx(dev, dfs);
	}

	spi_context_cs_control(&data->spi_ctx, false);
	spi_context_complete(&data->spi_ctx, dev, 0);
}

static int spi_msp_transceive(const struct device *dev,
				const struct spi_config *spi_cfg,
				const struct spi_buf_set *tx_bufs,
				const struct spi_buf_set *rx_bufs)
{
	struct spi_msp_data *data = dev->data;
	int ret;
	uint8_t dfs;

	if (!tx_bufs && !rx_bufs) {
		return 0;
	}

	spi_context_lock(&data->spi_ctx, false, NULL, NULL, spi_cfg);

	dfs = BYTES_PER_FRAME(SPI_WORD_SIZE_GET(spi_cfg->operation));

	ret = spi_msp_configure(dev, spi_cfg, dfs);
	if (ret != 0) {
		spi_context_release(&data->spi_ctx, ret);
		return ret;
	}

	spi_context_buffers_setup(&data->spi_ctx, tx_bufs, rx_bufs, dfs);

	spi_msp_start_transfer(dev, dfs);

	ret = spi_context_wait_for_completion(&data->spi_ctx);
	spi_context_release(&data->spi_ctx, ret);

	return ret;
}

static int spi_msp_release(const struct device *dev, const struct spi_config *spi_cfg)
{
	const struct spi_msp_config *config = dev->config;
	struct spi_msp_data *data = dev->data;

	if (!spi_context_configured(&data->spi_ctx, spi_cfg)) {
		return -EINVAL;
	}

	if (DL_SPI_isBusy(config->spi_base)) {
		return -EBUSY;
	}

	spi_context_unlock_unconditionally(&data->spi_ctx);
	return 0;
}

static const struct spi_driver_api spi_msp_api = {
	.transceive = spi_msp_transceive,
	.release    = spi_msp_release,
};

static int spi_msp_init(const struct device *dev)
{
	const struct spi_msp_config *config = dev->config;
	struct spi_msp_data *data = dev->data;
	int32_t ret;

	DL_SPI_enablePower(config->spi_base);
	delay_cycles(POWER_STARTUP_DELAY);

	ret = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	ret = spi_context_cs_configure_all(&data->spi_ctx);
	if (ret < 0) {
		return ret;
	}
	DL_SPI_setClockConfig(config->spi_base, (DL_SPI_ClockConfig *)&config->clock_config);
	DL_SPI_enable(config->spi_base);

	spi_context_unlock_unconditionally(&data->spi_ctx);

	config->irq_config_func(dev);

	return ret;
}

#define MSP_SPI_INIT(index)														\
	PINCTRL_DT_INST_DEFINE(index);													\
																					\
	static void spi_msp_irq_config_##index(const struct device *dev)				\
	{																				\
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), spi_msp_isr,\
				DEVICE_DT_INST_GET(index), 0);										\
		irq_enable(DT_INST_IRQN(index));											\
	};																				\
																					\
	IF_ENABLED(CONFIG_HAS_MSP_UNICOMM, 												\
	(static UNICOMM_Inst_Regs spi_msp_uc_regs_##index = {							\
		.inst =  (UNICOMM_Regs *)DT_INST_REG_ADDR(index), 							\
		.spi = (UNICOMMSPI_Regs *)UC_SPI_BASE(DT_INST_REG_ADDR(index)),				\
		.fixedMode = DT_CHILD_NUM(DT_PARENT(DT_DRV_INST(index))) == 1,				\
	};) 																			\
	)																				\
																					\
	static const struct msp_sys_clock msp_spi_sys_clock##index =				\
		MSP_CLOCK_SUBSYS_FN(index);												\
																					\
	static struct spi_msp_config spi_msp_config_##index = {						\
		.spi_base = COND_CODE_1(CONFIG_HAS_MSP_UNICOMM, 							\
			(&spi_msp_uc_regs_##index), ((SPI_Regs *)DT_INST_REG_ADDR(index))),	\
		.pinctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(index),							\
		.hw_cs = DT_STRING_TOKEN(DT_DRV_INST(index), hw_chip_select),				\
		.clock_config = {.clockSel =												\
				  MSP_CLOCK_PERIPH_REG_MASK(DT_INST_CLOCKS_CELL(index, clk)),		\
				 .divideRatio = SPI_DT_CLK_DIV_ENUM(index)},						\
		.clock_subsys = &msp_spi_sys_clock##index,								\
		.irq_config_func = spi_msp_irq_config_##index,							\
	};																				\
																					\
	static struct spi_msp_data spi_msp_data_##index = {							\
		.spi_idle = Z_SEM_INITIALIZER(spi_msp_data_##index.spi_idle, 0, 1),		\
		SPI_CONTEXT_INIT_LOCK(spi_msp_data_##index, spi_ctx),						\
		SPI_CONTEXT_INIT_SYNC(spi_msp_data_##index, spi_ctx),						\
		SPI_CONTEXT_CS_GPIOS_INITIALIZE(DT_DRV_INST(index), spi_ctx)				\
	};																				\
																					\
	DEVICE_DT_INST_DEFINE(index, spi_msp_init, NULL, &spi_msp_data_##index,		\
			      &spi_msp_config_##index, POST_KERNEL, CONFIG_SPI_INIT_PRIORITY,	\
			      &spi_msp_api);

DT_INST_FOREACH_STATUS_OKAY(MSP_SPI_INIT)
