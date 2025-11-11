/*
 * Copyright (c) 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <ti/driverlib/driverlib.h>

#define EXPECTED_HW_CS DL_SPI_CHIP_SELECT_1
static const struct device *const spi_device_uc2 = DEVICE_DT_GET(DT_NODELABEL(unicomm2_spi));

#define EXPECTED_GPIO_CS DL_GPIO_PIN_0
#define EXPECTED_GPIO_CS_PINCM 11
static const struct device *const spi_device_uc3 = DEVICE_DT_GET(DT_NODELABEL(unicomm3_spi));

struct spi_config cfg;
struct spi_config cfg_alt;

#define SPI_BUF_SIZE 18

static const char spi_tx_data[SPI_BUF_SIZE] = "0123456789abcdef-\0";
static __aligned(32) char spi_buffer_tx[SPI_BUF_SIZE] __used;
static __aligned(32) char spi_buffer_rx[SPI_BUF_SIZE] __used;

bool spi_do_transfer(const struct device *dev, const struct spi_config *spi_cfg)
{
	int ret;
	memset(spi_buffer_tx, 0, sizeof(spi_buffer_tx));
	memcpy(spi_buffer_tx, spi_tx_data, sizeof(spi_tx_data));

	const struct spi_buf tx_bufs[] = {
		{
			.buf = spi_buffer_tx,
			.len = SPI_BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = spi_buffer_rx,
			.len = SPI_BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};

	ret = spi_transceive(dev, spi_cfg, &tx, &rx);
	if(ret < 0) return false;

	for(int i = 0; i < SPI_BUF_SIZE; i++) {
		if(spi_buffer_rx[i] != spi_buffer_tx[i-1]) return false;
	}

	return true;
}

ZTEST(unicomm_spi, test_context_lock_on)
{
	cfg.frequency = 100000;
	cfg.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_MODE_CPOL | SPI_WORD_SET(8) | SPI_LOCK_ON;
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg));
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg)); //Second transfer should still succeed

	zassert_equal(spi_release(spi_device_uc2, &cfg), 0); // Should only return 0 if lock still on
}

ZTEST(unicomm_spi, test_frame_formats)
{
	cfg_alt.frequency = 100000;
	cfg_alt.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_MODE_CPOL | SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg_alt));
	zassert_equal(DL_SPI_getFrameFormat(UC2), DL_SPI_FRAME_FORMAT_MOTO4_POL1_PHA0);

	cfg.frequency = 100000;
	cfg.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_MODE_CPHA |SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg));
	zassert_equal(DL_SPI_getFrameFormat(UC2), DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA1);

	cfg_alt.frequency = 100000;
	cfg_alt.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg_alt));
	zassert_equal(DL_SPI_getFrameFormat(UC2), DL_SPI_FRAME_FORMAT_MOTO4_POL1_PHA1);

	cfg.frequency = 100000;
	cfg.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_FRAME_FORMAT_TI | SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg));
	zassert_equal(DL_SPI_getFrameFormat(UC2), DL_SPI_FRAME_FORMAT_TI_SYNC);
}

ZTEST(unicomm_spi, test_hold_on_cs)
{
	cfg_alt.frequency = 100000;
	cfg_alt.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_MODE_CPOL | SPI_WORD_SET(8) | SPI_HOLD_ON_CS;
	struct spi_cs_control cs_ctrl = (struct spi_cs_control){
		.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(unicomm3_spi), cs_gpios),
		.delay = 0u,
	};
	cfg_alt.cs = cs_ctrl;


	zassert_true(DL_GPIO_readPins(GPIOB, EXPECTED_GPIO_CS) > 0);
	zassert_true(spi_do_transfer(spi_device_uc3, &cfg_alt));
	/* CS should still be active */
	zassert_true(DL_GPIO_readPins(GPIOB, EXPECTED_GPIO_CS) == 0);

	cfg_alt.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_MODE_CPOL | SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc3, &cfg_alt));
	/* CS should still not be active */
	zassert_true(DL_GPIO_readPins(GPIOB, EXPECTED_GPIO_CS) > 0);
}


/* Test requires that the overlay file be updated and re-compiled. There
 * is no way to change the device tree properties on the fly.
 */
ZTEST(unicomm_spi, test_hw_chip_select)
{
	cfg.frequency = 100000;
	cfg.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg));
	zassert_equal(DL_SPI_getChipSelect(UC2), EXPECTED_HW_CS);
}

ZTEST(unicomm_spi, test_transceive_lsb)
{
	cfg_alt.frequency = 100000;
	cfg_alt.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_TRANSFER_LSB | SPI_WORD_SET(8);

	zassert_true(spi_do_transfer(spi_device_uc2, &cfg_alt));
	zassert_equal(DL_SPI_getBitOrder(UC2), DL_SPI_BIT_ORDER_LSB_FIRST);
}

ZTEST(unicomm_spi, test_transceive_msb)
{
	cfg.frequency = 100000;
	cfg.operation = SPI_OP_MODE_MASTER | SPI_MODE_LOOP | SPI_TRANSFER_MSB |SPI_WORD_SET(8);

	zassert_true(spi_do_transfer(spi_device_uc2, &cfg));
	zassert_equal(DL_SPI_getBitOrder(UC2), DL_SPI_BIT_ORDER_MSB_FIRST);
}

/* Require manual connection */
ZTEST(unicomm_spi, test_transceive_no_loopback)
{
	cfg_alt.frequency = 100000;
	cfg_alt.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_LSB | SPI_WORD_SET(8);
	zassert_true(spi_do_transfer(spi_device_uc2, &cfg_alt));
}

/*
 * Test setup
 */
void *test_setup(void)
{
	zassert_true(device_is_ready(spi_device_uc2), "spi device uc2 is not ready");
	zassert_true(device_is_ready(spi_device_uc3), "spi device uc3 is not ready");
	IOMUX->SECCFG.PINCM[EXPECTED_GPIO_CS_PINCM] |= IOMUX_PINCM_INENA_ENABLE;

	return NULL;
}

ZTEST_SUITE(unicomm_spi, NULL, test_setup, NULL, NULL, NULL);
