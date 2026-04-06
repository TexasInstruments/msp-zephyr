/*
 * Copyright (c) 2025 Texas Instruments
 * Copyright (c) 2025 Linumiz GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_msp_dma

#include <stdio.h>
#include <string.h>
#include <soc.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>

LOG_MODULE_REGISTER(ti_msp_dma, CONFIG_DMA_LOG_LEVEL);

#define DMA_TI_MSP_BASE_CHANNEL_NUM 1

/* Data Transfer Width */
enum dma_ti_msp_datawidth {
	DMA_TI_MSP_DATAWIDTH_BYTE = 1, /* 1 byte */
	DMA_TI_MSP_DATAWIDTH_HALF = 2, /* 2 bytes */
	DMA_TI_MSP_DATAWIDTH_WORD = 4, /* 4 bytes */
	DMA_TI_MSP_DATAWIDTH_LONG = 8, /* 8 bytes */
};

struct dma_ti_msp_config {
	DMA_Regs *regs;
	void (*irq_config_func)(void);
};

struct dma_ti_msp_channel_data {
	dma_callback_t dma_callback;
	void *user_data;
	uint8_t direction;
	bool busy;
	bool external_trigger;
};

struct dma_ti_msp_data {
	struct dma_context dma_ctx;
	struct k_sem lock;
	struct dma_ti_msp_channel_data *ch_data;
	atomic_t atomic_channels;
};

static inline int dma_ti_msp_get_memory_increment(uint8_t adj, uint32_t *increment)
{
	if (increment == NULL) {
		return -EINVAL;
	}

	switch (adj) {
	case DMA_ADDR_ADJ_INCREMENT:
		*increment = DL_DMA_ADDR_INCREMENT;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		*increment = DL_DMA_ADDR_UNCHANGED;
		break;
	case DMA_ADDR_ADJ_DECREMENT:
		*increment = DL_DMA_ADDR_DECREMENT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline int dma_ti_msp_get_dstdatawidth(enum dma_ti_msp_datawidth wd, uint32_t *dwidth)
{
	if (dwidth == NULL) {
		return -EINVAL;
	}

	switch (wd) {
	case DMA_TI_MSP_DATAWIDTH_BYTE:
		*dwidth = DL_DMA_WIDTH_BYTE;
		break;
	case DMA_TI_MSP_DATAWIDTH_HALF:
		*dwidth = DL_DMA_WIDTH_HALF_WORD;
		break;
	case DMA_TI_MSP_DATAWIDTH_WORD:
		*dwidth = DL_DMA_WIDTH_WORD;
		break;
	case DMA_TI_MSP_DATAWIDTH_LONG:
		*dwidth = DL_DMA_WIDTH_LONG;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline int dma_ti_msp_get_srcdatawidth(enum dma_ti_msp_datawidth wd, uint32_t *dwidth)
{
	if (dwidth == NULL) {
		return -EINVAL;
	}

	switch (wd) {
	case DMA_TI_MSP_DATAWIDTH_BYTE:
		*dwidth = DL_DMA_WIDTH_BYTE;
		break;
	case DMA_TI_MSP_DATAWIDTH_HALF:
		*dwidth = DL_DMA_WIDTH_HALF_WORD;
		break;
	case DMA_TI_MSP_DATAWIDTH_WORD:
		*dwidth = DL_DMA_WIDTH_WORD;
		break;
	case DMA_TI_MSP_DATAWIDTH_LONG:
		*dwidth = DL_DMA_WIDTH_LONG;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int dma_ti_msp_configure(const struct device *dev, uint32_t channel,
				struct dma_config *config)
{
	uint32_t temp;
	unsigned int key;
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *dma_data = dev->data;
	struct dma_ti_msp_channel_data *chan_data = NULL;
	struct dma_block_config *blk_cfg = config->head_block;
	DL_DMA_Config dma_cfg = {0};

	if ((config == NULL) || (channel >= dma_data->dma_ctx.dma_channels)) {
		return -EINVAL;
	}

	chan_data = &dma_data->ch_data[channel];

	if (chan_data->busy != false) {
		return -EBUSY;
	}

	if (config->dest_data_size != config->source_data_size) {
		LOG_ERR("Source and Destination data width is not same");
		return -EINVAL;
	}

	if (config->source_burst_length != config->dest_burst_length) {
		LOG_ERR("Source and Destination burst lengths must be the same");
		return -EINVAL;
	}

	/* Check that the channel direction is supported */
	if (config->channel_direction != MEMORY_TO_MEMORY &&
	    config->channel_direction != MEMORY_TO_PERIPHERAL &&
	    config->channel_direction != PERIPHERAL_TO_MEMORY) {
		LOG_ERR("Unsupported channel direction");
		return -ENOTSUP;
	}

	if (dma_ti_msp_get_memory_increment(blk_cfg->source_addr_adj, &temp)) {
		LOG_ERR("Invalid Source address increment");
		return -EINVAL;
	}

	dma_cfg.srcIncrement = temp;

	if (dma_ti_msp_get_memory_increment(blk_cfg->dest_addr_adj, &temp)) {
		LOG_ERR("Invalid Destination address increment");
		return -EINVAL;
	}

	dma_cfg.destIncrement = temp;

	if (dma_ti_msp_get_dstdatawidth((enum dma_ti_msp_datawidth)config->dest_data_size, &temp)) {
		LOG_ERR("Invalid Destination data width");
		return -EINVAL;
	}

	dma_cfg.destWidth = temp;

	if (dma_ti_msp_get_srcdatawidth((enum dma_ti_msp_datawidth)config->source_data_size,
					&temp)) {
		LOG_ERR("Invalid Source data width");
		return -EINVAL;
	}

	dma_cfg.srcWidth = temp;
	chan_data->direction = config->channel_direction;
	chan_data->dma_callback = config->dma_callback;
	chan_data->user_data = config->user_data;
	dma_cfg.extendedMode = DL_DMA_NORMAL_MODE;
	dma_cfg.trigger = config->dma_slot;
	/* All triggers (software and peripheral hardware) are routed
	 * through the external trigger mux. INTERNAL type is only for
	 * inter-channel DMA chaining.
	 */
	dma_cfg.triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL;

	switch (config->dma_slot) {
	case 0:
		/* Software trigger (DMAREQ): single block transfer */
		dma_cfg.transferMode = DL_DMA_SINGLE_BLOCK_TRANSFER_MODE;
		chan_data->external_trigger = false;
		break;
	default:
		/* Peripheral hardware trigger: use repeat-single mode where
		 * each trigger transfers one element. Channel stays armed
		 * until all transfers complete.
		 */
		dma_cfg.transferMode = DL_DMA_FULL_CH_REPEAT_SINGLE_TRANSFER_MODE;
		chan_data->external_trigger = true;
		break;
	}

	/* Configure burst size based on source_burst_length */
	DL_DMA_BURST_SIZE burstSize;

	switch (config->source_burst_length) {
	case 8:
		burstSize = DL_DMA_BURST_SIZE_8;
		break;
	case 16:
		burstSize = DL_DMA_BURST_SIZE_16;
		break;
	case 32:
		burstSize = DL_DMA_BURST_SIZE_32;
		break;
	default:
		burstSize = DL_DMA_BURST_SIZE_INFINITY;
		break;
	}

	key = irq_lock();
	DL_DMA_setBurstSize(cfg->regs, burstSize);
	DL_DMA_clearInterruptStatus(cfg->regs, BIT(channel));
	DL_DMA_setTransferSize(cfg->regs, channel, blk_cfg->block_size);
	DL_DMA_initChannel(cfg->regs, channel, &dma_cfg);
	DL_DMA_setSrcAddr(cfg->regs, channel, blk_cfg->source_address);
	DL_DMA_setDestAddr(cfg->regs, channel, blk_cfg->dest_address);
	DL_DMA_enableInterrupt(cfg->regs, BIT(channel));
	chan_data->busy = true;
	irq_unlock(key);

	LOG_DBG("DMA Channel %u configured with burst length %u", channel,
		config->source_burst_length);

	return 0;
}

static int dma_ti_msp_start(const struct device *dev, const uint32_t channel)
{
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *dma_data = dev->data;

	if (channel >= dma_data->dma_ctx.dma_channels) {
		return -EINVAL;
	}

	DL_DMA_enableChannel(cfg->regs, channel);

	if (!DL_DMA_isChannelEnabled(cfg->regs, channel)) {
		LOG_ERR("Failed to enable DMA channel %u", channel);
		return -EINVAL;
	}

	/* Only issue software trigger for non-external channels.
	 * External trigger channels wait for the peripheral hardware pulse.
	 */
	if (!dma_data->ch_data[channel].external_trigger) {
		DL_DMA_startTransfer(cfg->regs, channel);
	}

	return 0;
}

static int dma_ti_msp_stop(const struct device *dev, const uint32_t channel)
{
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *data = dev->data;

	if (channel >= data->dma_ctx.dma_channels) {
		return -EINVAL;
	}

	DL_DMA_disableChannel(cfg->regs, channel);
	data->ch_data[channel].busy = false;

	return 0;
}

static int dma_ti_msp_reload(const struct device *dev, uint32_t channel, uint32_t src_addr,
			     uint32_t dest_addr, size_t size)
{
	unsigned int key;
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_channel_data *chan_data = NULL;
	struct dma_ti_msp_data *dma_data = dev->data;

	if (channel >= dma_data->dma_ctx.dma_channels) {
		return -EINVAL;
	}

	chan_data = &dma_data->ch_data[channel];

	key = irq_lock();
	switch (chan_data->direction) {
	case PERIPHERAL_TO_MEMORY:
		DL_DMA_setDestAddr(cfg->regs, channel, dest_addr);
		break;
	case MEMORY_TO_PERIPHERAL:
		DL_DMA_setSrcAddr(cfg->regs, channel, src_addr);
		break;
	case MEMORY_TO_MEMORY:
		DL_DMA_setSrcAddr(cfg->regs, channel, src_addr);
		DL_DMA_setDestAddr(cfg->regs, channel, dest_addr);
		break;
	default:
		LOG_ERR("Unsupported data direction");
		return -ENOTSUP;
	}

	DL_DMA_setTransferSize(cfg->regs, channel, size);
	chan_data->busy = true;
	irq_unlock(key);

	return 0;
}

static int dma_ti_msp_get_status(const struct device *dev, uint32_t channel,
				 struct dma_status *stat)
{
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *dma_data = dev->data;
	struct dma_ti_msp_channel_data *chan_data;

	if (channel >= dma_data->dma_ctx.dma_channels) {
		return -EINVAL;
	}

	chan_data = &dma_data->ch_data[channel];
	stat->pending_length = DL_DMA_getTransferSize(cfg->regs, channel);
	stat->dir = chan_data->direction;
	stat->busy = chan_data->busy;

	return 0;
}

static inline void dma_ti_msp_isr(const struct device *dev)
{
	int channel;
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *dma_data = dev->data;
	struct dma_ti_msp_channel_data *chan_data;

	channel = DL_DMA_getPendingInterrupt(cfg->regs);
	channel -= DMA_TI_MSP_BASE_CHANNEL_NUM;

	if ((channel < 0) || (channel >= dma_data->dma_ctx.dma_channels)) {
		return;
	}

	chan_data = &dma_data->ch_data[channel];
	DL_DMA_disableChannel(cfg->regs, channel);

	/* Clear interrupt status */
	DL_DMA_clearInterruptStatus(cfg->regs, BIT(channel));

	/* Mark channel as not busy before callback, as callback might want to reuse it */
	chan_data->busy = false;

	/* Call the callback if provided */
	if (chan_data->dma_callback != NULL) {
		chan_data->dma_callback(dev, chan_data->user_data, channel, DMA_STATUS_COMPLETE);
	}
}

static int dma_ti_msp_init(const struct device *dev)
{
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *data = dev->data;

	/* Initialize the semaphore */
	k_sem_init(&data->lock, 1, 1);

	if (cfg->irq_config_func != NULL) {
		cfg->irq_config_func();
	}

	return 0;
};

static int dma_ti_msp_suspend(const struct device *dev, uint32_t channel)
{
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *dma_data = dev->data;

	if (channel >= dma_data->dma_ctx.dma_channels) {
		return -EINVAL;
	}

	/* Check if the channel is enabled first */
	if (!DL_DMA_isChannelEnabled(cfg->regs, channel)) {
		return -EBUSY; /* Channel is already stopped */
	}

	/* Suspend by disabling the channel but don't change the busy flag */
	DL_DMA_disableChannel(cfg->regs, channel);

	return 0;
}

static int dma_ti_msp_resume(const struct device *dev, uint32_t channel)
{
	const struct dma_ti_msp_config *cfg = dev->config;
	struct dma_ti_msp_data *dma_data = dev->data;

	if (channel >= dma_data->dma_ctx.dma_channels) {
		return -EINVAL;
	}

	/* Enable and restart the channel */
	DL_DMA_enableChannel(cfg->regs, channel);

	/* Verify channel was enabled successfully */
	if (!DL_DMA_isChannelEnabled(cfg->regs, channel)) {
		LOG_ERR("Failed to enable DMA channel %u", channel);
		return -EINVAL;
	}

	DL_DMA_startTransfer(cfg->regs, channel);

	return 0;
}

static const struct dma_driver_api dma_ti_msp_api = {
	.config = dma_ti_msp_configure,
	.start = dma_ti_msp_start,
	.stop = dma_ti_msp_stop,
	.reload = dma_ti_msp_reload,
	.get_status = dma_ti_msp_get_status,
	.suspend = dma_ti_msp_suspend,
	.resume = dma_ti_msp_resume,
};

#define MSP_DMA_INIT(inst)                                                                         \
	static inline void dma_ti_msp_irq_cfg_##inst(void)                                         \
	{                                                                                          \
		irq_disable(DT_INST_IRQN(inst));                                                   \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), dma_ti_msp_isr,       \
			    DEVICE_DT_INST_GET(inst), 0);                                          \
                                                                                                   \
		irq_enable(DT_INST_IRQN(inst));                                                    \
	}                                                                                          \
                                                                                                   \
	static struct dma_ti_msp_channel_data                                                      \
		dma_ti_msp_channels_##inst[DT_INST_PROP(inst, dma_channels)];                      \
                                                                                                   \
	static const struct dma_ti_msp_config dma_cfg_##inst = {                                   \
		.regs = (DMA_Regs *)DT_INST_REG_ADDR(inst),                                        \
		.irq_config_func = dma_ti_msp_irq_cfg_##inst,                                      \
	};                                                                                         \
	struct dma_ti_msp_data dma_data_##inst = {                                                 \
		.dma_ctx =                                                                         \
			{                                                                          \
				.magic = DMA_MAGIC,                                                \
				.dma_channels = DT_INST_PROP(inst, dma_channels),                  \
				.atomic = &dma_data_##inst.atomic_channels,                        \
			},                                                                         \
		.ch_data = dma_ti_msp_channels_##inst,                                             \
		.lock = Z_SEM_INITIALIZER(dma_data_##inst.lock, 1, 1),                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &dma_ti_msp_init, NULL, &dma_data_##inst, &dma_cfg_##inst,     \
			      PRE_KERNEL_1, CONFIG_DMA_INIT_PRIORITY, &dma_ti_msp_api);

DT_INST_FOREACH_STATUS_OKAY(MSP_DMA_INIT);
