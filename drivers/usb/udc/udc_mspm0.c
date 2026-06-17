/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * USB Device Controller (UDC) driver for Texas Instruments MSPM0 series.
 *
 * The MSPM0 USB peripheral is based on the Mentor Graphics MUSB IP core.
 * This driver implements the Zephyr UDC API for full-speed device operation.
 */

#include "udc_common.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/udc.h>

#include <zephyr/logging/log.h>

#include <ti/devices/msp/peripherals/hw_usb.h>
#include <ti/driverlib/m0p/sysctl/dl_sysctl_mspm0g518x.h>

LOG_MODULE_REGISTER(udc_mspm0, CONFIG_UDC_DRIVER_LOG_LEVEL);

/*
 * Max packet size and max FIFO size specifications 
 */
#define UDC_MSPM0_CONTROL_MAX_PACKET_SIZE	64U
#define UDC_MSPM0_BULK_MAX_PACKET_SIZE	64U
#define UDC_MSPM0_INTR_MAX_PACKET_SIZE	64U
#define UDC_MSPM0_ISO_MAX_PACKET_SIZE	1024U
#define UDC_MSPM0_EP_MAX_PACKET_SIZE	1024U
#define UDC_MSPM0_CONTROL_MAX_FIFO_SIZE	64U
#define UDC_MSPM0_EP_FIFO_SIZE	256U

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*
 * Structure for holding controller configuration items that can remain in
 * non-volatile memory. This is usually accessed as
 * const struct udc_mspm0_config *config = dev->config;
 */
struct udc_mspm0_config {
	size_t num_of_eps;
	size_t num_in_eps;
	size_t num_out_eps;
	struct udc_ep_config *ep_cfg_in;
	struct udc_ep_config *ep_cfg_out;
	k_thread_stack_t *thread_stk;
	size_t thread_stk_sz;
	int speed_idx;
	void (*irq_config_func)(const struct device *dev);
};
typedef enum {
	EP0_STATE_IDLE,        /* Waiting for a brand-new SETUP packet */
	EP0_STATE_DATA_IN,     /* We are actively transmitting data to the host (Control Read) */
	EP0_STATE_DATA_OUT,    /* We are actively receiving data from the host (Control Write) */
	EP0_STATE_STATUS_IN,   /* We are sending a ZLP to finish a transaction (like SetAddress) */
	EP0_STATE_STATUS_OUT   /* We are waiting to receive a ZLP from the host to finish */
} mspm0_ep0_state_t;

/*
 * Structure to hold driver private data.
 * Note that this is not accessible via dev->data, but as
 *   struct udc_mspm0_data *priv = udc_get_private(dev);
 */
struct udc_mspm0_data {
	struct k_thread thread_data;
	struct k_event events;
	struct usb_setup_packet setup_data;    /* last received SETUP packet */
	volatile mspm0_ep0_state_t ep0_state; /* EP0 control transfer state machine */
	const struct device *dev;
	volatile bool txrdy_set[8];           /* tracks whether TXRDY was set per EP, used to detect TX complete */
	uint16_t fifo_ram_addr_offset;        /* next available offset in USB FIFO RAM, grows as EPs are enabled */
	volatile bool ep0_stalled;            /* set when a protocol STALL is issued on EP0 */
 };

/* Thread event bits. BIT(n) for EP n TX load, BIT(8) for new SETUP packet. */
#define UDC_MSPM0_EVT_EP0_LOAD	BIT(0)
#define UDC_MSPM0_EVT_EP1_LOAD BIT(1)
#define UDC_MSPM0_EVT_EP2_LOAD BIT(2)
#define UDC_MSPM0_EVT_EP3_LOAD BIT(3)
#define UDC_MSPM0_EVT_EP4_LOAD BIT(4)
#define UDC_MSPM0_EVT_EP5_LOAD BIT(5)
#define UDC_MSPM0_EVT_EP6_LOAD BIT(6)
#define UDC_MSPM0_EVT_EP7_LOAD BIT(7)
#define UDC_MSPM0_EVT_SETUP	BIT(8)

/*
 * Driver thread handler.
 *
 * Handles two types of events:
 *   UDC_MSPM0_EVT_SETUP   - A new SETUP packet was read from the FIFO in the
 *                            ISR. Forwards it to the stack via udc_setup_received()
 *                            and advances the EP0 state machine.
 *   UDC_MSPM0_EVT_EPn_LOAD - Loads the next chunk of a pending TX transfer into
 *                            the FIFO for EP n. Only used for EP0 and for EP1-7
 *                            multi-packet continuations; first packets on idle
 *                            EP1-7 endpoints are loaded inline in udc_ep_enqueue().
 */
static void mspm0_thread_handler(void *arg1, void *arg2, void *arg3)
{
	const struct device *dev = (const struct device *)arg1;
	struct udc_mspm0_data *priv = udc_get_private(dev);
	const struct udc_mspm0_config *config = dev->config;
	mspm0_ep0_state_t prev_state;

	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		uint32_t evt = k_event_wait(&priv->events,
						 UDC_MSPM0_EVT_SETUP | 
						 UDC_MSPM0_EVT_EP0_LOAD |
						 UDC_MSPM0_EVT_EP1_LOAD |
						 UDC_MSPM0_EVT_EP2_LOAD |
						 UDC_MSPM0_EVT_EP3_LOAD |
						 UDC_MSPM0_EVT_EP4_LOAD |
						 UDC_MSPM0_EVT_EP5_LOAD |
						 UDC_MSPM0_EVT_EP6_LOAD |
						 UDC_MSPM0_EVT_EP7_LOAD,
						 true,
						 K_FOREVER);
		
		struct net_buf *buf;

		if (evt & UDC_MSPM0_EVT_SETUP) {

			/* Hand the SETUP packet to the stack. This is async — the stack
			 * processes it in its own thread and may call ep_set_halt() or
			 * ep_enqueue() later. We must advance the EP0 state now so the
			 * hardware is ready before the stack's response arrives. */
			udc_setup_received(dev, &priv->setup_data);

			if(priv->ep0_stalled) {
				USBFS0->REGISTERS.CSR0L |= (USB_CSR0L_RXRDYC_STATUS_MASK);
			}

			if(!priv->ep0_stalled) {
				/* Determine control state based on wlength and data transaction type*/
				if (priv->setup_data.wLength == 0) {
					udc_ep_set_busy(&config->ep_cfg_out[0], true);
					priv->txrdy_set[0] = true;
					priv->ep0_state = EP0_STATE_STATUS_IN;
					USBFS0->REGISTERS.CSR0L |= (USB_CSR0L_RXRDYC_STATUS_MASK | USB_CSR0L_DATAEND_SETUP_MASK);
				}
				else if(priv->setup_data.wLength > 0) {
					if (USB_EP_DIR_IS_IN(priv->setup_data.bmRequestType)) {
						priv->ep0_state = EP0_STATE_DATA_IN;
						USBFS0->REGISTERS.CSR0L |= USB_CSR0L_RXRDYC_STATUS_MASK;
					}
					if (USB_EP_DIR_IS_OUT(priv->setup_data.bmRequestType)) {
						priv->ep0_state = EP0_STATE_DATA_OUT;
						USBFS0->REGISTERS.CSR0L |= USB_CSR0L_RXRDYC_STATUS_MASK;
					}
				}
			} 


		}

		/* EP0 IN data load — only valid during DATA_IN phase. The stack
		 * enqueues the response buffer after processing the SETUP packet,
		 * which triggers this event via udc_ep_enqueue(). */
		if (evt & UDC_MSPM0_EVT_EP0_LOAD) {

			struct udc_ep_config *ep_cfg = &config->ep_cfg_in[0];

			/* Skip if endpoint is halted or already busy with current transfer */
			if (ep_cfg->stat.halted || udc_ep_is_busy(ep_cfg)) {
				LOG_INF("Endpoint is halted or busy for EP0");
				continue;
			}
			
			/* Load queued buffer into hw FIFO */
			buf = udc_buf_peek(ep_cfg);
			
			if(buf) {
				if(priv->ep0_state == EP0_STATE_DATA_IN) {

					udc_ep_set_busy(ep_cfg, true);

					uint16_t len = buf->len > ep_cfg->caps.mps ? ep_cfg->caps.mps : buf->len;
					uint8_t *data = buf->data;

					/* Load data into TX FIFO */
					for (uint16_t j = 0; j < len; j++) {
						USBFS0->REGISTERS.FIFO_BYTE[0] = data[j];
					}

					net_buf_pull(buf, len);

					if(buf->len == 0) {
						USBFS0->REGISTERS.CSR0L |= (USB_CSR0L_TXRDY_MASK | USB_CSR0L_DATAEND_SETUP_MASK);
						priv->txrdy_set[0] = true;
					}
					else {
						USBFS0->REGISTERS.CSR0L |= USB_CSR0L_TXRDY_MASK;
						priv->txrdy_set[0] = true;
					}

				}
			}
			else {
				LOG_INF("Buffer is NULL for EP0");
			}

		}

		/* EP1-7 TX continuation — reached only for multi-packet transfers
		 * where more data remains after the first chunk was sent. */
		for(int i = 1; i < config->num_in_eps; i++) {

			if(evt & BIT(i)) {

				struct udc_ep_config *ep_cfg = &config->ep_cfg_in[i];

				/* Skip if endpoint is halted or already busy with current transfer */
				if (ep_cfg->stat.halted || udc_ep_is_busy(ep_cfg)) {
					LOG_INF("Endpoint is halted or busy FOR EP%", i);
					continue;
				}
				
				/* Load queued buffer into hw FIFO */
				buf = udc_buf_peek(ep_cfg);
				
				if(buf) {
					
					udc_ep_set_busy(ep_cfg, true);

					uint16_t len = buf->len > ep_cfg->mps ? ep_cfg->mps : buf->len;
					uint8_t *data = buf->data;

					/* Load data into TX FIFO */
					for (uint16_t j = 0; j < len; j++) {
						*((__IO uint8_t *)&USBFS0->REGISTERS.FIFO[i]) = data[j];  // correct
					}

					net_buf_pull(buf, len);

					unsigned int key = irq_lock();

					USBFS0->REGISTERS.EPINDEX = i;

					USBFS0->REGISTERS.IDXTXCSRL |= USB_TXCSRL_TXRDY_MASK;

					priv->txrdy_set[i] = true;

					irq_unlock(key);

				}
				else {
					// LOG_INF("EP%d: Buffer is NULL", i);
				}

			}
		}
		
	}
}

/*
 * This is called in the context of udc_ep_enqueue() and must
 * not block. The driver can immediately claim the buffer if the queue is empty,
 * but usually it is offloaded to a thread or workqueue to handle transfers
 * in a single location. Please refer to existing driver implementations
 * for examples.
 */
static int udc_mspm0_ep_enqueue(const struct device *dev,
				   struct udc_ep_config *const cfg,
				   struct net_buf *buf)
{
	struct udc_mspm0_data *priv = udc_get_private(dev);

	if (!cfg || !buf) {
		return -ENODEV;
	}

	if (!cfg->stat.enabled) {
		return -EACCES;
	}

	uint8_t ep_num = USB_EP_GET_IDX(cfg->addr);

	LOG_DBG("%p enqueue %p for EP%d", dev, buf, ep_num);
	udc_buf_put(cfg, buf);
  
	if (cfg->stat.halted) {
		/*
		 * It is fine to enqueue a transfer for a halted endpoint,
		 * you need to make sure that transfers are retriggered when
		 * the halt is cleared.
		 *
		 * Always use the abbreviation 'ep' for the endpoint address
		 * and 'ep_idx' or 'ep_num' for the endpoint number identifiers.
		 * Although struct udc_ep_config uses address to be unambiguous
		 * in its context.
		 */
		LOG_DBG("ep 0x%02x halted", cfg->addr);
		return 0;
	}

	if (USB_EP_DIR_IS_IN(cfg->addr)) {
		/* For EP0 IN the thread handles loading (ep0_state must be DATA_IN
		 * first, which the thread sets after udc_setup_received() returns).
		 * For EP1-7 IN signal the thread to load if the endpoint is idle. */
		if (!udc_ep_is_busy(cfg)) {
			k_event_set(&priv->events, BIT(ep_num));
			LOG_DBG("ep 0x%02x TX enqueued", cfg->addr);
		}
	} else {

		LOG_DBG("ep 0x%02x RX enqueued", cfg->addr);

		if (ep_num == 0 && priv->ep0_state == EP0_STATE_DATA_OUT) {

			/* ACK the DATA_OUT phase now that the stack has a buffer. */
			USBFS0->REGISTERS.CSR0L |= USB_CSR0L_RXRDYC_STATUS_MASK;

		} else if (ep_num > 0) {

			/* Race: RXRDY may have fired before buf was enqueued.
			 * ISR skipped (no buf), so handle it here. */

			unsigned int key = irq_lock();
			USBFS0->REGISTERS.EPINDEX = ep_num;

			if (USBFS0->REGISTERS.IDXRXCSRL & USB_RXCSRL_RXRDY_MASK) {

				uint16_t count = USBFS0->REGISTERS.IDXRXCOUNT;
				struct net_buf *rx_buf = udc_buf_get(cfg);

				if (rx_buf) {

					udc_ep_set_busy(cfg, true);

					for (uint16_t j = 0; j < count && j < rx_buf->size; j++) {
						rx_buf->data[j] = *((__IO uint8_t *)&USBFS0->REGISTERS.FIFO[ep_num]);
					}

					net_buf_add(rx_buf, count);
					irq_unlock(key);
					udc_submit_ep_event(dev, rx_buf, 0);
					udc_ep_set_busy(cfg, false);
					USBFS0->REGISTERS.IDXRXCSRL &= ~USB_RXCSRL_RXRDY_MASK;
				}
			}

			irq_unlock(key);
		}
	}

	return 0;
}

/*
 * This is called in the context of udc_ep_dequeue()
 * and must remove all requests from an endpoint queue
 * Successful removal should be reported to the higher level with
 * ECONNABORTED as the request result.
 * It is up to the request owner to clean up or reuse the buffer.
 */
static int udc_mspm0_ep_dequeue(const struct device *dev,
				   struct udc_ep_config *const cfg)
{
	unsigned int lock_key;

	if (!cfg) {
		return -ENODEV;
	}

	if (cfg->stat.enabled) {
		return -EACCES;
	}

	lock_key = irq_lock();

	uint8_t ep_num = USB_EP_GET_IDX(cfg->addr);
	USBFS0->REGISTERS.EPINDEX = ep_num;

	if(ep_num == 0) {
		USBFS0->REGISTERS.CSR0H |= USB_CSR0H_FLUSH_MASK;
	}

	/* Flush out latest packet from endpoint TX FIFO */
	if (USB_EP_DIR_IS_IN(cfg->addr)) {
		USBFS0->REGISTERS.IDXTXCSRL |= USB_TXCSRL_FLUSH_MASK;
	}

	/* Flush out latest packet from endpoint RX FIFO */
	if (USB_EP_DIR_IS_OUT(cfg->addr)) {
		USBFS0->REGISTERS.IDXRXCSRL |= USB_RXCSRL_FLUSH_MASK;
	}

	udc_ep_cancel_queued(dev, cfg);

	udc_ep_set_busy(cfg, false);

	irq_unlock(lock_key);

	return 0;
}


/*
 * Configure and make an endpoint ready for use.
 * This is called in the context of udc_ep_enable() or udc_ep_enable_internal(),
 * the latter of which may be used by the driver to enable control endpoints.
 */
/*
 * Configure and enable an endpoint.
 *
 * Allocates a slice of the USB FIFO RAM, sets the FIFO size and address
 * registers, and enables the corresponding TX/RX interrupt. EP0 is special:
 * it uses a fixed 64-byte FIFO at offset 0 and shares TX/RX on a single FIFO.
 * The fifo_ram_addr_offset in priv advances linearly as endpoints are enabled.
 */
static int udc_mspm0_ep_enable(const struct device *dev,
				  struct udc_ep_config *const cfg)
{
	struct udc_mspm0_data *priv = udc_get_private(dev);

	if (!cfg) {
		return -ENODEV;
	}

	if (cfg->stat.enabled) {
		return -EALREADY;
	}

	bool bulk = false;
	bool intr = false;
	bool iso = false;

	/* Set EP transfer type and max packet size supported by MSPM0 */
	switch (cfg->attributes & USB_EP_TRANSFER_TYPE_MASK) {
	case USB_EP_TYPE_BULK:
		cfg->caps.mps = UDC_MSPM0_BULK_MAX_PACKET_SIZE;
		bulk = true;
		break;
	case USB_EP_TYPE_INTERRUPT:
		cfg->caps.mps = UDC_MSPM0_INTR_MAX_PACKET_SIZE;
		intr = true;
		break;
	case USB_EP_TYPE_ISO:
		cfg->caps.mps = UDC_MSPM0_ISO_MAX_PACKET_SIZE;
		iso = true;
		break;
	case USB_EP_TYPE_CONTROL:
		cfg->caps.mps = UDC_MSPM0_CONTROL_MAX_PACKET_SIZE;
		break;
	default:
		return -EINVAL;
	}
	
	uint8_t ep_idx = USB_EP_GET_IDX(cfg->addr);

	USBFS0->REGISTERS.EPINDEX = ep_idx;

	bool is_in = (cfg->addr & USB_EP_DIR_MASK) == USB_EP_DIR_IN;

	if(ep_idx == 0) {
		/* EP0 FIFO is always at offset 0. Reserve 64 bytes and enable
		 * the combined EP0 TX/RX interrupt (bit 0 of TXIE). */
		priv->fifo_ram_addr_offset = UDC_MSPM0_CONTROL_MAX_FIFO_SIZE;
		USBFS0->REGISTERS.TXIE |= 1;
	}
	else {
		/* FIFO size index: bulk/intr always use 64 bytes (index 0x03).
		 * ISO uses the actual MPS rounded up to the next power of two. */
		uint8_t fifo_size = (bulk || intr) ? 64 : MIN(cfg->mps, 1024);
		uint8_t fifo_size_idx = (bulk || intr) ? 0x03 : MAX(ceil(log2(fifo_size)) - 3, 0);

		if (is_in) {
			USBFS0->REGISTERS.TXIE |= 1 << ep_idx;
			if(bulk || intr) {
				/* Disable DMA/autoset/ISO modes; clear data toggle. */
				USBFS0->REGISTERS.IDXTXCSRH &= ~ (USB_TXCSRH_AUTOSET_MASK | USB_TXCSRH_ISO_MASK | USB_TXCSRH_DMAEN_MASK | USB_TXCSRH_FDT_MASK);
				USBFS0->REGISTERS.IDXTXCSRL |= USB_TXCSRL_CLRDT_MASK;
				/* Flush any stale data left from a previous session. */
				if(USBFS0->REGISTERS.IDXTXCSRL & USB_TXCSRL_FIFONE_MASK) USBFS0->REGISTERS.IDXTXCSRL |= USB_TXCSRL_FLUSH_MASK;
			}
			USBFS0->REGISTERS.IDXTXFIFOSZ = fifo_size_idx;
			USBFS0->REGISTERS.IDXTXMAXP = cfg->mps;
			USBFS0->REGISTERS.IDXTXFIFOADD = (priv->fifo_ram_addr_offset >> 3);;
			priv->fifo_ram_addr_offset += fifo_size;
		}
		else {
			USBFS0->REGISTERS.RXIE |= 1 << ep_idx;
			USBFS0->REGISTERS.IDXRXFIFOSZ = fifo_size_idx;
			USBFS0->REGISTERS.IDXRXMAXP = cfg->mps;
			USBFS0->REGISTERS.IDXRXFIFOADD = (priv->fifo_ram_addr_offset >> 3);
			priv->fifo_ram_addr_offset += fifo_size;
		}
		
	}
	
	LOG_ERR("ENABLE EP addr=0x%02x type=%d, caps.mps=%d, mps=%d",
		cfg ? cfg->addr : 0xFF,
		cfg ? (cfg->attributes & USB_EP_TRANSFER_TYPE_MASK) : -1,
		cfg ? cfg->caps.mps : -1,
		cfg ? cfg->mps : -1);

	LOG_DBG("Enable ep 0x%02x", cfg->addr);

	return 0;
}

/*
 * Opposite function to udc_mspm0_ep_enable(). udc_ep_disable_internal()
 * may be used by the driver to disable control endpoints.
 */
static int udc_mspm0_ep_disable(const struct device *dev,
				   struct udc_ep_config *const cfg)
{

	if (!cfg) {
		return -ENODEV;
	}
		
	if (!cfg->stat.enabled) {
		return -EALREADY;
	}
	
	uint8_t ep_idx = USB_EP_GET_IDX(cfg->addr);
	bool is_in = (cfg->addr & USB_EP_DIR_MASK) == USB_EP_DIR_IN;

	/* Clear TX or RX interrupt on Endpoint to disable */
	if (is_in || ep_idx == 0) {
		USBFS0->REGISTERS.TXIE &= ~(1 << ep_idx);
	}
	else {
		USBFS0->REGISTERS.RXIE &= ~(1 << ep_idx);
	}

	LOG_DBG("Disable ep 0x%02x", cfg->addr);

	return 0;
}


/* Halt endpoint. Halted endpoint should respond with a STALL handshake. */
static int udc_mspm0_ep_set_halt(const struct device *dev,
					struct udc_ep_config *const cfg)
{

	if (!cfg) {
		return -ENODEV;
	}

	if ((cfg->attributes & USB_EP_TRANSFER_TYPE_MASK) == USB_EP_TYPE_ISO) {
		return -ENOTSUP;
	}

	struct udc_mspm0_data *priv = udc_get_private(dev);

	LOG_DBG("Set halt ep 0x%02x", cfg->addr);

	/*
	 * NOTE: udc_ep_clear_halt() is not called for control endpoints.
	 *
	 * When an endpoint is halted or a control pipe request is not
	 * supported, endpoint responds with a STALL handshake packet. The
	 * specification distinguishes between a functional stall and a
	 * protocol stall. The stack calls udc_ep_set_halt() to set a
	 * functional or protocol stall. A protocol stall is unique to control
	 * pipes and terminates at the beginning of the next control transfer.
	 * Although a control pipe may support functional stall, it is not
	 * recommended by the specification. The stack does not call
	 * udc_ep_clear_halt() for control endpoints.
	 *
	 * How a driver clears a protocol stall depends on the implementation.
	 * Some controllers automatically clear the protocol stall condition
	 * when the next setup packet arrives, while others require software
	 * intervention.
	 */
	uint8_t ep_idx = USB_EP_GET_IDX(cfg->addr);
	bool is_in = (cfg->addr & USB_EP_DIR_MASK) == USB_EP_DIR_IN;

	if(ep_idx == 0) {
		priv->ep0_stalled = true;
		USBFS0->REGISTERS.CSR0L |= USB_CSR0L_STALL_RQPKT_MASK;
		USBFS0->REGISTERS.CSR0H |= USB_CSR0H_FLUSH_MASK;
	}
	else {
		USBFS0->REGISTERS.EPINDEX = ep_idx;
		if(is_in) {
			USBFS0->REGISTERS.IDXTXCSRL |= USB_TXCSRL_STALLSETUP_MASK;
		}
		else {
			USBFS0->REGISTERS.IDXRXCSRL |= USB_RXCSRL_STALLREQPKT_MASK;
		}
	}
	cfg->stat.halted = true;

	return 0;
}

/*
 * Opposite to halt endpoint. If there are requests in the endpoint queue,
 * the next transfer should be prepared.
 */
static int udc_mspm0_ep_clear_halt(const struct device *dev,
					  struct udc_ep_config *const cfg)
{

	
	if (!cfg) {
		return -ENODEV;
	}
	
	if ((cfg->attributes & USB_EP_TRANSFER_TYPE_MASK) == USB_EP_TYPE_ISO) {
		return -ENOTSUP;
	}
	
	LOG_DBG("Clear halt ep 0x%02x", cfg->addr);
	uint8_t ep_idx = USB_EP_GET_IDX(cfg->addr);
	bool is_in = (cfg->addr & USB_EP_DIR_MASK) == USB_EP_DIR_IN;

	USBFS0->REGISTERS.EPINDEX = ep_idx;
	if(is_in) {
		USBFS0->REGISTERS.IDXTXCSRL &= ~USB_TXCSRL_STALLSETUP_MASK;
	}
	else {
		USBFS0->REGISTERS.IDXRXCSRL &= ~USB_RXCSRL_STALLREQPKT_MASK;
	}

	cfg->stat.halted = false;

	return 0;
}

static int udc_mspm0_set_address(const struct device *dev, const uint8_t addr)
{
	struct udc_mspm0_data *priv = udc_get_private(dev);

	LOG_DBG("Set new address %u for %p", addr, dev);
	USBFS0->REGISTERS.FADDR = addr;
	return 0;
}

static int udc_mspm0_host_wakeup(const struct device *dev)
{
	LOG_DBG("Remote wakeup from %p", dev);

	return 0;
}

/* Return actual USB device speed */
static enum udc_bus_speed udc_mspm0_device_speed(const struct device *dev)
{
	return UDC_BUS_SPEED_FS;
}

static int udc_mspm0_enable(const struct device *dev)
{
	struct udc_data *data = (struct udc_data *)dev->data;

	if (atomic_test_bit(&data->status, UDC_STATUS_ENABLED)) {
		return -EALREADY;
	}
	
	/*
	* USB peripheral configured as a device - set device mode enable bit alongside the
	* PHY bit as well to configure the PHY. By configuring the PHY, IOMUX for the DP and DM
	* pins are automatically configured and no further configuration is required.
	*/
	USBFS0->USBMODE = (USB_USBMODE_DEVICEONLY_ENABLE | USB_USBMODE_PHYMODE_USB);

	/* Clear any pending USB interrupt */
	NVIC_ClearPendingIRQ(USBFS0_INT_IRQn);

	/* Clearing USB interrupts on CPU_INT */
	USBFS0->CPU_INT.ICLR = (USB_ICLR_INTRUSB_CLR | USB_ICLR_VUSBPWRDN_CLR | USB_ICLR_INTRTX_CLR
							| USB_ICLR_INTRRX_CLR);

	/* Clearing out USB interrupts on USBIS, TXIS and RXIS */
	USBFS0->REGISTERS.USBIS;
	USBFS0->REGISTERS.TXIS;
	USBFS0->REGISTERS.RXIS;
	
	/* Setting USB interrupts on CPU_INT */
	USBFS0->CPU_INT.IMASK  = (USB_IMASK_INTRTX_SET | USB_IMASK_INTRRX_SET | 
								USB_IMASK_INTRUSB_SET);
	
	/* Setting USB interrupts on USBIE */
	USBFS0->REGISTERS.USBIE = (USB_USBIE_SUSPEND_ENABLE | USB_USBIE_RESUME_ENABLE | 
								USB_USBIE_RESETBABBLE_ENABLE);
	

	/* Soft connect to enable USB D+/D- lines*/
	USBFS0->REGISTERS.POWER = USB_POWER_SOFT_CONN_ENABLE;
	
	/* Enable USB interrupts */
	NVIC_EnableIRQ(USBFS0_INT_IRQn);

	LOG_DBG("Enable device %p", dev);
	
	return 0;
}

void udc_mspm0_isr(void)
{

	/* Get device instance */
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(usbfs0));
	if (!dev) {
		LOG_ERR("ISR: device is NULL!");
		return;
	}
	const struct udc_mspm0_config *config = dev->config;
	struct udc_mspm0_data *priv = udc_get_private(dev);
	struct net_buf *buf;
	struct udc_ep_config *ep_cfg;
	mspm0_ep0_state_t prev_state;

	/* Reading IIDX returns the highest-priority pending interrupt and
	 * clears its pending bit. Only one source is handled per ISR entry;
	 * if multiple are pending the CPU will re-enter immediately after. */
	uint32_t pendingInterrupt = USBFS0->CPU_INT.IIDX;

	switch (pendingInterrupt) {

	/* ------------------------------------------------------------------ */
	/* Bus-level events: Reset, Resume, Suspend                            */
	/* ------------------------------------------------------------------ */
	case USB_IIDX_STAT_INTRUSB: {
		uint8_t usb_status = USBFS0->REGISTERS.USBIS;

		if (usb_status & USB_USBIS_RESETBABBLE_MASK) {
			priv->ep0_state = EP0_STATE_IDLE;
			udc_submit_event(dev, UDC_EVT_RESET, 0);
		}
		if (usb_status & USB_USBIS_RESUME_MASK) {
			udc_submit_event(dev, UDC_EVT_RESUME, 0);
		}
		if (usb_status & USB_USBIS_SUSPEND_MASK) {
			udc_submit_event(dev, UDC_EVT_SUSPEND, 0);
		}

		break;
	}

	/* ------------------------------------------------------------------ */
	/* TX complete / EP0 events (TXIS covers EP0 + EP1-7 IN)              */
	/* ------------------------------------------------------------------ */
	case USB_IIDX_STAT_INTRTX: {
		uint16_t tx_status = USBFS0->REGISTERS.TXIS;

		/* --- EP0 handling ------------------------------------------- */
		if (tx_status & USB_TXIS_EP0_MASK) {

			/* Snapshot CSR0L once — it is read-sensitive on some bits. */
			uint8_t csr0l = USBFS0->REGISTERS.CSR0L;

			/* SETEND: host aborted the current control transfer by sending
			 * a new SETUP before we finished. Flush queued buffers and
			 * return to IDLE so we are ready for the incoming SETUP. */
			if(csr0l & USB_CSR0L_SETEND_ERROR_MASK) {

				USBFS0->REGISTERS.CSR0L |= USB_CSR0L_SETENDC_NAKTO_MASK;

				// Clean up EP0 IN
				ep_cfg = &config->ep_cfg_in[0];
				buf = udc_buf_get(ep_cfg);
				if (buf) { udc_submit_ep_event(dev, buf, -ECONNABORTED); }
				udc_ep_set_busy(ep_cfg, false);

				// Clean up EP0 OUT
				ep_cfg = &config->ep_cfg_out[0];
				buf = udc_buf_get(ep_cfg);
				if (buf) { udc_submit_ep_event(dev, buf, -ECONNABORTED); }
				udc_ep_set_busy(ep_cfg, false);
				
				priv->ep0_state = EP0_STATE_IDLE;
			}

			/* STALLED: the STALL handshake was actually sent to the host.
			 * Per USB spec a SETUP packet always clears a protocol stall,
			 * so reset all EP0 software state here and wait for the next
			 * SETUP. The hardware already cleared the STALL bit. */
			if(csr0l & USB_CSR0L_STALLED_MASK) {
				priv->ep0_stalled = false;
				config->ep_cfg_in[0].stat.halted = false;
				config->ep_cfg_out[0].stat.halted = false;
				priv->ep0_state = EP0_STATE_IDLE;
				USBFS0->REGISTERS.CSR0L &= ~USB_CSR0L_STALLED_MASK;
			}

			/* STATUS_IN ZLP was consumed by the host — transaction done. */
			if(!(csr0l & USB_CSR0L_RXRDYC_STATUS_MASK) && (priv->ep0_state == EP0_STATE_STATUS_IN)) {
				priv->ep0_state = EP0_STATE_IDLE;
				LOG_DBG("Hardware has handled the Status IN Phase.");
			}

			/* RXRDY: data arrived in the EP0 FIFO (SETUP, OUT data, or ZLP) */
			if (csr0l & USB_CSR0L_RXRDY_MASK) {

				uint8_t count0 = USBFS0->REGISTERS.COUNT0;
				uint8_t *dest;
				ep_cfg = &config->ep_cfg_out[0];
				buf = udc_buf_peek(ep_cfg);

				/* IDLE + RXRDY = new SETUP packet. Read it out and
				 * signal the thread; the thread calls udc_setup_received()
				 * and advances the state machine. Also clear any lingering
				 * protocol stall — hardware does this automatically but
				 * software flags need to follow. */
				if(priv->ep0_state == EP0_STATE_IDLE) {

					dest = (uint8_t *)&priv->setup_data;
					for (uint8_t i = 0; i < count0; i++) {
						dest[i] = USBFS0->REGISTERS.FIFO_BYTE[0];
					}

					k_event_set(&priv->events, UDC_MSPM0_EVT_SETUP);

				}

				else if(priv->ep0_state == EP0_STATE_DATA_OUT && buf) {

					dest = net_buf_tail(buf);

					/* Read OUT transfer from FIFO */
					for (uint8_t i = 0; i < count0; i++) {
						dest[i] = USBFS0->REGISTERS.FIFO_BYTE[0];
					}

					net_buf_add(buf, count0);

					/* Issue ACK by clearing RXRDY */
					USBFS0->REGISTERS.CSR0L |= USB_CSR0L_RXRDYC_STATUS_MASK;

					if(buf->len == priv->setup_data.wLength) {

						buf = udc_buf_get(ep_cfg);

						udc_ep_set_busy(ep_cfg, false);

						udc_submit_ep_event(dev, buf, 0);

						USBFS0->REGISTERS.CSR0L |= USB_CSR0L_DATAEND_SETUP_MASK;

						// prev_state = priv->ep0_state;
						priv->ep0_state = EP0_STATE_STATUS_IN;
					}

				}
				else if(priv->ep0_state == EP0_STATE_STATUS_OUT && count0 == 0) {

					USBFS0->REGISTERS.CSR0L |= (USB_CSR0L_RXRDYC_STATUS_MASK | USB_CSR0L_DATAEND_SETUP_MASK);
					
					buf = udc_buf_get(ep_cfg);
					udc_ep_set_busy(ep_cfg, false);
					udc_submit_ep_event(dev, buf, 0);

					priv->ep0_state = EP0_STATE_IDLE;

				}
			}

				/* TXRDY cleared by hardware = host consumed our IN packet.
			 * Advance to the next chunk or complete the transfer. */
			if (priv->txrdy_set[0] && (csr0l & USB_CSR0L_TXRDY_MASK) == 0) {

				/* Data phase or status phase completion */
				ep_cfg = &config->ep_cfg_in[0];
				buf = udc_buf_peek(ep_cfg);

				priv->txrdy_set[0] = false;

				if(buf) {
					if (buf->len == 0) {
						if(priv->ep0_state == EP0_STATE_STATUS_IN) {
							priv->ep0_state = EP0_STATE_IDLE;
							
						}
						else if(priv->ep0_state == EP0_STATE_DATA_IN) {
							priv->ep0_state = EP0_STATE_IDLE;
						}
						
						buf = udc_buf_get(ep_cfg);
						udc_ep_set_busy(ep_cfg, false);
						udc_submit_ep_event(dev, buf, 0);
					}
					else {
						udc_ep_set_busy(ep_cfg, false);
						k_event_set(&priv->events, UDC_MSPM0_EVT_EP0_LOAD);
					}
					
				}
				else {
					udc_ep_set_busy(ep_cfg, false);
					priv->ep0_state = EP0_STATE_IDLE;
				}

			}
		}

		/* --- EP1-7 IN handling -------------------------------------- */
		for (int i = 1; i < config->num_in_eps; i++) {
			if (tx_status & (1 << i)) {

				USBFS0->REGISTERS.EPINDEX = i;

				/* Clear the STALLED status bit after the host received it. */
				if(USBFS0->REGISTERS.IDXTXCSRL & USB_TXCSRL_STALLED_MASK) {
					LOG_DBG("Host has received STALL for EP%d", i);
					USBFS0->REGISTERS.IDXTXCSRL &= ~USB_TXCSRL_STALLED_MASK;
				}

				/* TXRDY cleared = host consumed our packet. */
				if(priv->txrdy_set[i] && ((USBFS0->REGISTERS.IDXTXCSRL & USB_TXCSRL_TXRDY_MASK) == 0)) {

					priv->txrdy_set[i] = false;
					ep_cfg = &config->ep_cfg_in[i];
	
					/* Get completed buffer and submit event */
					buf = udc_buf_peek(ep_cfg);
					if (buf) {
						if(buf->len == 0) {
							buf = udc_buf_get(ep_cfg);
							udc_ep_set_busy(ep_cfg, false);
							udc_submit_ep_event(dev, buf, 0);
						}
						else {
							udc_ep_set_busy(ep_cfg, false);
							k_event_set(&priv->events, BIT(i));
						}
					}
	
					/* Clear endpoint busy and signal thread to load next data */
				}

			}
		}

		break;
	}

	/* ------------------------------------------------------------------ */
	/* RX complete (EP1-7 OUT)                                             */
	/* ------------------------------------------------------------------ */
	case USB_IIDX_STAT_INTRRX: {
		uint16_t rx_status = USBFS0->REGISTERS.RXIS;

		for (int i = 1; i < config->num_out_eps; i++) {
			if (rx_status & (1 << i)) {

				USBFS0->REGISTERS.EPINDEX = i;

				if(USBFS0->REGISTERS.IDXRXCSRL & USB_RXCSRL_RXRDY_MASK) {

					ep_cfg = &config->ep_cfg_out[i];
					/* Dequeue the pre-allocated RX buffer. If no buffer is
					 * available the packet is lost; the upper layer must
					 * always have a buffer enqueued before data arrives. */
					buf = udc_buf_get(ep_cfg);
					
					if (buf) {
						/* Select endpoint index to read FIFO */
						/* Get RX FIFO count */
						uint16_t count = USBFS0->REGISTERS.IDXRXCOUNT;
	
						/* Read data from RX FIFO into buffer */
						for (uint16_t j = 0; j < count && j < buf->size; j++) {
							buf->data[j] = *((__IO uint8_t *)&USBFS0->REGISTERS.FIFO[i]);  // byte access to EP i's FIFO
						}

						net_buf_add(buf, count);

						USBFS0->REGISTERS.IDXRXCSRL &= ~USB_RXCSRL_RXRDY_MASK;  // allow next packet
						udc_ep_set_busy(ep_cfg, false);
						udc_submit_ep_event(dev, buf, 0);
					}

				}
			}
		}

		break;
	}

	default:
		break;
	}

}

static int udc_mspm0_disable(const struct device *dev)
{
	struct udc_data *data = (struct udc_data *)dev->data;
	if (!atomic_test_bit(&data->status, UDC_STATUS_ENABLED)) {
		return -EALREADY;
	}

	/* Clear any pending USB interrupt */
	NVIC_ClearPendingIRQ(USBFS0_INT_IRQn);
	NVIC_DisableIRQ(USBFS0_INT_IRQn);

	/* Clearing USB interrupts on CPU_INT */
	USBFS0->CPU_INT.ICLR = (USB_ICLR_INTRUSB_CLR | USB_ICLR_VUSBPWRDN_CLR | USB_ICLR_INTRTX_CLR | USB_ICLR_INTRRX_CLR);

	USBFS0->CPU_INT.IMASK = 0;

	USBFS0->REGISTERS.TXIE = 0;
	USBFS0->REGISTERS.RXIE = 0;
	USBFS0->REGISTERS.USBIE = 0;
	
	/* Clearing out USB interrupts on USBIS, TXIS and RXIS */
	USBFS0->REGISTERS.USBIS;
	USBFS0->REGISTERS.TXIS;
	USBFS0->REGISTERS.RXIS;

	/* Clear soft connect to disable USB D+/D- lines*/
	USBFS0->REGISTERS.POWER &= ~USB_POWER_SOFT_CONN_ENABLE;

	LOG_DBG("Disable device %p", dev);

	return 0;
}

/*
 * Prepare and configure most of the parts, if the controller has a way
 * of detecting VBUS activity it should be enabled here.
 * Only udc_mspm0_enable() makes device visible to the host.
 */
/*
 * Power on and configure the USB peripheral.
 *
 * Sequence mirrors the MSPM0 TRM:
 *   1. Configure USBFLL to generate the 48 MHz USB reference from SOF.
 *   2. Assert peripheral reset, then enable power and wait for ready.
 *   3. Set clock divider to 1 (48 MHz USBCLK).
 *   4. Register the IRQ and configure debug GPIO pins.
 *   5. Enable EP0 IN/OUT via the UDC common layer.
 *
 * Does NOT soft-connect — that happens in udc_mspm0_enable().
 */
static int udc_mspm0_init(const struct device *dev)
{
	if (!dev) {
		return -EINVAL;
	}

	struct udc_data *data = (struct udc_data *)dev->data;
	struct udc_mspm0_data *priv = udc_get_private(dev);

	priv->ep0_state = EP0_STATE_IDLE;

	if (atomic_test_bit(&data->status, UDC_STATUS_INITIALIZED)) {
		return -EALREADY;
	}

	const struct udc_mspm0_config *config = dev->config;
	int ret;

	/* Configure USBFLL */
	DL_SYSCTL_configUSBFLL(DL_SYSCTL_USBFLL_REFERENCE_SOF);
	
	/* Set USBCLK source to USBFLL */
	DL_SYSCTL_setUSBCLKSource(DL_SYSCTL_USBCLK_SOURCE_USBFLL);
	
	/* Reset the USB peripheral */
	USBFS0->GPRCM.RSTCTL = (USB_RSTCTL_KEY_UNLOCK_W | USB_RSTCTL_RESETSTKYCLR_CLR |
		USB_RSTCTL_RESETASSERT_ASSERT);
		
	/* Enable power to the USB peripheral */
	USBFS0->GPRCM.PWREN = (USB_PWREN_ENABLE_ENABLE |  USB_PWREN_KEY_UNLOCK_W);
	
	/* Polling until USB peripheral has been powered on with timeout */
	uint32_t timeout = 1000000;
	while (( USBFS0->GPRCM.PWREN & USB_PWREN_ENABLE_ENABLE ) == 0 && timeout-- > 0);
	if (timeout == 0) {
		return -ETIMEDOUT;
	}
	
	/* Polling for status of USB peripheral from SYSCTL module with timeout */
	timeout = 1000000;
	while (((SYSCTL->SOCLOCK.SYSSTATUS) & SYSCTL_SYSSTATUS_USBFS0READY_MASK) !=
	SYSCTL_SYSSTATUS_USBFS0READY_TRUE && timeout-- > 0);
	if (timeout == 0) {
		return -ETIMEDOUT;
	}
	
	/* Set clock divider for USBCLK */
	USBFS0->GPRCM.CLKCTL = USB_CLKCTL_CLKDIV_DIV_BY_1;
	
	/* Initialize IRQ */
	config->irq_config_func(dev);
	
	/* Setting USB interrupts for EP0 */
	USBFS0->REGISTERS.TXIE = 0;
	USBFS0->REGISTERS.RXIE = 0;
	
	LOG_DBG("Initialize Device");
	
	ret = udc_ep_enable_internal(dev, USB_CONTROL_EP_OUT,
					USB_EP_TYPE_CONTROL, UDC_MSPM0_CONTROL_MAX_PACKET_SIZE, 0);
	if (ret) {
		LOG_ERR("Failed to enable control endpoint OUT: %d", ret);
		return ret;
	}

	ret = udc_ep_enable_internal(dev, USB_CONTROL_EP_IN,
					USB_EP_TYPE_CONTROL, UDC_MSPM0_CONTROL_MAX_PACKET_SIZE, 0);
	if (ret) {
		LOG_ERR("Failed to enable control endpoint IN: %d", ret);
		return ret;
	}

	if (IS_ENABLED(CONFIG_UDC_ENABLE_SOF)) {
		LOG_INF("Enable SOF interrupt");
	}

	return 0;
}

/* Shut down the controller completely */
static int udc_mspm0_shutdown(const struct device *dev)
{
	if (!dev) {
		return -EINVAL;
	}

	struct udc_data *data = (struct udc_data *)dev->data;
	if (!atomic_test_bit(&data->status, UDC_STATUS_INITIALIZED)) {
		return -EALREADY;
	}

	int ret;

	ret = udc_ep_disable_internal(dev, USB_CONTROL_EP_OUT);
	if (ret) {
		LOG_ERR("Failed to disable control endpoint OUT: %d", ret);
		return ret;
	}

	ret = udc_ep_disable_internal(dev, USB_CONTROL_EP_IN);
	if (ret) {
		LOG_ERR("Failed to disable control endpoint IN: %d", ret);
		return ret;
	}

	/* Disable power to the USB peripheral */
	USBFS0->GPRCM.PWREN = (USB_PWREN_ENABLE_DISABLE |  USB_PWREN_KEY_UNLOCK_W);

	return 0;
}

/*
 * This is called once to initialize the controller and endpoints
 * capabilities, and register endpoint structures.
 */
static int udc_mspm0_driver_preinit(const struct device *dev)
{
	const struct udc_mspm0_config *config = dev->config;
	struct udc_mspm0_data *priv = udc_get_private(dev);
	struct udc_data *data = dev->data;
	int err;

	/*
	 * You do not need to initialize it if your driver does not use
	 * udc_lock_internal() / udc_unlock_internal(), but implements its
	 * own mechanism.
	 */
	k_mutex_init(&data->mutex);

	data->caps.rwup = true;
	data->caps.mps0 = UDC_MPS0_64;
	// if (config->speed_idx == 2) {
	// 	data->caps.hs = true;
	// 	mps = 1024;
	// }
	data->caps.addr_before_status = true;
	data->caps.out_ack = true;

	for (int i = 0; i < config->num_in_eps; i++) {
		priv->txrdy_set[i] = false;
	}

	priv->ep0_stalled = false;

	for (int i = 0; i < config->num_out_eps; i++) {

		config->ep_cfg_out[i].caps.out = 1;

		if (i == 0) {
			config->ep_cfg_out[i].caps.control = 1;
			config->ep_cfg_out[i].caps.mps = UDC_MSPM0_CONTROL_MAX_PACKET_SIZE;
		} else {
			if(config->ep_cfg_out)
			config->ep_cfg_out[i].caps.bulk = 1;
			config->ep_cfg_out[i].caps.interrupt = 1;
			config->ep_cfg_out[i].caps.iso = 1;
			config->ep_cfg_out[i].caps.mps = UDC_MSPM0_EP_MAX_PACKET_SIZE;

		}

		config->ep_cfg_out[i].addr = USB_EP_DIR_OUT | i;
		err = udc_register_ep(dev, &config->ep_cfg_out[i]);

		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	for (int i = 0; i < config->num_in_eps; i++) {

		config->ep_cfg_in[i].caps.in = 1;
		if (i == 0) {
			config->ep_cfg_in[i].caps.control = 1;
			config->ep_cfg_in[i].caps.mps = UDC_MSPM0_CONTROL_MAX_PACKET_SIZE;
		} else {
			config->ep_cfg_in[i].caps.bulk = 1;
			config->ep_cfg_in[i].caps.interrupt = 1;
			config->ep_cfg_in[i].caps.iso = 1;
			config->ep_cfg_in[i].caps.mps = UDC_MSPM0_EP_MAX_PACKET_SIZE;
						
		}

		config->ep_cfg_in[i].addr = USB_EP_DIR_IN | i;
		err = udc_register_ep(dev, &config->ep_cfg_in[i]);

		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}


	k_event_init(&priv->events);
	priv->dev = dev;

	k_thread_create(&priv->thread_data,
			config->thread_stk,
			config->thread_stk_sz,
			mspm0_thread_handler,
			(void *)dev, NULL, NULL,
			K_PRIO_COOP(8),
			K_ESSENTIAL,
			K_NO_WAIT);
	k_thread_name_set(&priv->thread_data, dev->name);

	LOG_INF("Device %p (max. speed %d)", dev, config->speed_idx);

	return 0;
}

static void udc_mspm0_lock(const struct device *dev)
{
	udc_lock_internal(dev, K_FOREVER);
}

static void udc_mspm0_unlock(const struct device *dev)
{
	udc_unlock_internal(dev);
}

/*
 * UDC API structure.
 * Note, you do not need to implement basic checks, these are done by
 * the UDC common layer udc_common.c
 */
static const struct udc_api udc_mspm0_api = {
	.lock = udc_mspm0_lock,
	.unlock = udc_mspm0_unlock,
	.device_speed = udc_mspm0_device_speed,
	.init = udc_mspm0_init,
	.enable = udc_mspm0_enable,
	.disable = udc_mspm0_disable,
	.shutdown = udc_mspm0_shutdown,
	.set_address = udc_mspm0_set_address,
	.host_wakeup = udc_mspm0_host_wakeup,
	.ep_try_config = NULL,
	.ep_enable = udc_mspm0_ep_enable,
	.ep_disable = udc_mspm0_ep_disable,
	.ep_set_halt = udc_mspm0_ep_set_halt,
	.ep_clear_halt = udc_mspm0_ep_clear_halt,
	.ep_enqueue = udc_mspm0_ep_enqueue,
	.ep_dequeue = udc_mspm0_ep_dequeue,
};

#define DT_DRV_COMPAT ti_mspm0_usb

#define MSP_UDC_IRQ_DEFINE(inst)                                                                  \
	static void udc_mspm0_##inst##_irq_register(const struct device *dev)                     \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), udc_mspm0_isr,       \
				DEVICE_DT_INST_GET(inst), 0);                                          \
		irq_enable(DT_INST_IRQN(inst));                                                    \
	}

/*
 * A UDC driver should always be implemented as a multi-instance
 * driver, even if your platform does not require it.
 */
#define UDC_MSPM0_DEVICE_DEFINE(n)						\
	K_THREAD_STACK_DEFINE(udc_mspm0_stack_##n,				\
				  CONFIG_UDC_MSPM0_STACK_SIZE);			\
										\
	static struct udc_ep_config						\
		ep_cfg_out[DT_INST_PROP(n, num_bidir_endpoints)];		\
	static struct udc_ep_config						\
		ep_cfg_in[DT_INST_PROP(n, num_bidir_endpoints)];		\
										\
	MSP_UDC_IRQ_DEFINE(n);				\
											\
	static const struct udc_mspm0_config udc_mspm0_config_##n = {	\
		.num_of_eps = DT_INST_PROP(n, num_bidir_endpoints),		\
		.num_in_eps = DT_INST_PROP(n, num_in_endpoints), \
		.num_out_eps = DT_INST_PROP(n, num_out_endpoints), \
		.ep_cfg_in = ep_cfg_in,						\
		.ep_cfg_out = ep_cfg_out,					\
		.thread_stk = udc_mspm0_stack_##n,				\
		.thread_stk_sz = K_THREAD_STACK_SIZEOF(udc_mspm0_stack_##n),	\
		.speed_idx = DT_ENUM_IDX(DT_DRV_INST(n), maximum_speed),	\
		.irq_config_func = udc_mspm0_##n##_irq_register,						\
	};									\
										\
	static struct udc_mspm0_data udc_priv_##n = {			\
	};									\
										\
	static struct udc_data udc_data_##n = {					\
		.mutex = Z_MUTEX_INITIALIZER(udc_data_##n.mutex),		\
		.priv = &udc_priv_##n,						\
	};									\
										\
	DEVICE_DT_INST_DEFINE(n, udc_mspm0_driver_preinit, NULL,		\
				  &udc_data_##n, &udc_mspm0_config_##n,		\
				  POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,	\
				  &udc_mspm0_api);

DT_INST_FOREACH_STATUS_OKAY(UDC_MSPM0_DEVICE_DEFINE)
