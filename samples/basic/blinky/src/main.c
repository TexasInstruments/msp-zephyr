/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "ti_msp_dl_config.h"

/* GrLib Includes */
#include "grlib.h"
#include "button.h"
#include "imageButton.h"
#include "radioButton.h"
#include "checkbox.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "images.h"
#include "core_cm33.h"
//#include "touch_P401R.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

char *boot_str = "M33 Zephyr is Alive";
char *t1_str = "Zephyr Thread 1";
char *t2_str = "Zephyrblink0_id Thread 2";
//void drawTiLogo(char *);
void drawTiLogo(Graphics_Context *ctx, char *str);

bool led_state = true;
// Graphic library context
Graphics_Context g_sContext;
Graphics_Context g_sContext1;
Graphics_Context g_sContext2;
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

K_SEM_DEFINE(my_sem, 0, 1);

void blink0(void)
{
  uint8_t ret = 0;

  k_sched_lock();
  Kitronix320x240x16_SSD2119Init();
  Graphics_initContext(&g_sContext1, &g_sKitronix320x240x16_SSD2119);
  Graphics_setBackgroundColor(&g_sContext1, GRAPHICS_COLOR_BLACK);
  Graphics_setFont(&g_sContext1, &g_sFontCmss20b);
  Graphics_clearDisplay(&g_sContext1);
  k_sched_unlock();

  while(1) {

    k_sched_lock();
    drawTiLogo(&g_sContext1, t1_str);
    k_sched_unlock();

    gpio_pin_toggle_dt(&led);
    k_msleep(1000);
    gpio_pin_toggle_dt(&led);

    led_state = !led_state;
    printf("LED state: %s\n", led_state ? "ON" : "OFF");
    k_msleep(1000);
    //k_sem_give(&my_sem);
  }
}

void blink1(void)
{

  uint8_t ret = 0;

  k_sched_lock();
  Kitronix320x240x16_SSD2119Init();
  Graphics_initContext(&g_sContext2, &g_sKitronix320x240x16_SSD2119);
  Graphics_setBackgroundColor(&g_sContext2, GRAPHICS_COLOR_BLACK);
  Graphics_setFont(&g_sContext2, &g_sFontCmss20b);
  Graphics_clearDisplay(&g_sContext2);
  k_sched_unlock();
  while(1) {
    //while(k_sem_take(&mDL_UNICOMM_SPIy_sem, K_MSEC(50)) != 0);

    k_sched_lock();
    drawTiLogo(&g_sContext2, t2_str);
    k_sched_unlock();

    DL_GPIO_initDigitalOutput(GPIO_LEDS_USER_LED_1_IOMUX);
    DL_GPIO_togglePins(GPIOC, GPIO_LEDS_USER_LED_1_PIN);
    k_msleep(1000);
    DL_GPIO_togglePins(GPIOC, GPIO_LEDS_USER_LED_1_PIN);
#if 0
    gpio_pin_toggle_dt(&led);
    k_msleep(1000);
    gpio_pin_toggle_dt(&led);
#endif

    led_state = !led_state;
    printf("LED state: %s\n", led_state ? "ON" : "OFF");
    k_msleep(1000);
  }
}

K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,
    PRIORITY, 0, 0);
K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL,
    PRIORITY, 0, 0);



/*
 * Number of bytes for SPI packet size
 * The packet will be transmitted by the SPI controller.
 * This example uses FIFOs with polling, and the maximum FIFO size is 4.
 * Refer to interrupt examples to handle larger packets.
 */
#define SPI_PACKET_SIZE (4)

/* Data for SPI to transmit */
uint8_t gTxPacket[SPI_PACKET_SIZE] = {'M', 'S', 'P', '!'};

/* Data received from SPI Peripheral */
volatile uint8_t gRxPacket[SPI_PACKET_SIZE];



void drawTiLogo( Graphics_Context *ctx, char *str)
{
  Graphics_setForegroundColor(ctx, GRAPHICS_COLOR_RED);
  Graphics_setBackgroundColor(ctx, GRAPHICS_COLOR_BLACK);
  Graphics_clearDisplay(ctx);
  //Graphics_drawStringCentered(&g_sContext, "MSP M33 Graphics Library Demo",
  //Graphics_drawStringCentered(&g_sContext, "MSP M33C321A Graphics Library Demo",
    Graphics_drawStringCentered(ctx, (char*)str,
        //AUTO_STRING_LENGTH,
        strlen(str),
        159,
        15,
        TRANSPARENT_TEXT);

  // Draw TI banner at the bottom of screnn
  Graphics_drawImage(ctx,
      &TI_platform_bar_red4BPP_UNCOMP,
      0,
      Graphics_getDisplayHeight(
        ctx) - TI_platform_bar_red4BPP_UNCOMP.ySize);

  //Draw TI Logo
  Graphics_drawImage(ctx,
      &TI_logo_150x1501BPP_COMP_RLE4,
      85,
      45);

  //    // Draw Primitives image button
  //    Graphics_drawImageButton(&g_sContext, &primitiveButton);
  //
  //    // Draw Images image button
  //    Graphics_drawImageButton(&g_sContext, &imageButton);
}

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TZM_IS_NONSECURE_CALLED __attribute__((cmse_nonsecure_call))
#define TZM_IS_NOSECURE_ENTRY   __attribute__((cmse_nonsecure_entry))
/* typedef for non-secure callback functions */
typedef void (*funcptr_ns)(void) TZM_IS_NONSECURE_CALLED;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief This function jumps to normal world.
 */
void TZM_JumpToNormalWorld(uint32_t nonsecVtorAddress)
{
    funcptr_ns ResetHandler_ns;

    /* Set non-secure main stack (MSP_NS) */
    __TZ_set_MSP_NS(*((uint32_t *)(nonsecVtorAddress)));

    /* Set non-secure vector table */
    SCB_NS->VTOR = nonsecVtorAddress;

    /* Get non-secure reset handler */
    ResetHandler_ns = (funcptr_ns)(*((uint32_t *)((nonsecVtorAddress) + 4U)));

#if defined(IAR_FP_VLSTM_ALIGNED_ISSUE) && (IAR_FP_VLSTM_ALIGNED_ISSUE == 1U) && defined(__ICCARM__)
#define COMPILER_VERSION_MAJOR ((__VER__) / 1000000)
#define COMPILER_VERSION_MINOR (((__VER__) / 1000) % 1000)
#if (COMPILER_VERSION_MAJOR == 9) && (COMPILER_VERSION_MINOR < 32)
    {
        /*
         * IAR issue: in ResetHandler_ns asm code, only push 7 register into stack
         * for Armv8-M, Stack pointer need aligned with 8bytes, otherwise will triger
         * a exception.
         * After IAR (9.32) fixed this issue ,this code must be deleted.
         */

        register unsigned int tmpSp;
        int byteAlign;

        asm volatile("MOV %0, SP\n" : "=r"(tmpSp));

        /* One register size is 4byte */
        tmpSp = (tmpSp % 32) / 4;
        for (byteAlign = 0; (byteAlign + tmpSp + 7) % 8 != 0; byteAlign++)
        {
            asm volatile("PUSH {r12}");
        }
    }
#endif
#endif
    /* Call non-secure application - jump to normal world */
    ResetHandler_ns();
#if defined(IAR_FP_VLSTM_ALIGNED_ISSUE) && (IAR_FP_VLSTM_ALIGNED_ISSUE == 1U) && defined(__ICCARM__)
    {
        /* Never return here, so do not need 'POP' */
    }
#endif
}
#define SAU_REGION_0_BASE 0x10080000U
#define SAU_REGION_0_END  0x10100000U
void BOARD_InitTrustZone()
{
    /* SAU configuration */

    /* Set SAU Control register: Disable SAU and All Secure */
    SAU->CTRL = 0;


    /* Set SAU region number */
    SAU->RNR = 0;
    /* Region base address */
    SAU->RBAR = SAU_REGION_0_BASE & SAU_RBAR_BADDR_Msk;
    /* Region end address */
    SAU->RLAR = (SAU_REGION_0_END & SAU_RLAR_LADDR_Msk) | ((0U << SAU_RLAR_NSC_Pos) & SAU_RLAR_NSC_Msk) |
                ((1U << SAU_RLAR_ENABLE_Pos) & SAU_RLAR_ENABLE_Msk);
    /* Force memory writes before continuing */
    __DSB();
    /* Flush and refill pipeline with updated permissions */
    __ISB();

    /* Set SAU Control register: Enable SAU and All Secure (applied only if disabled) */
    SAU->CTRL = ((0U << SAU_CTRL_ALLNS_Pos) & SAU_CTRL_ALLNS_Msk) | ((1U << SAU_CTRL_ENABLE_Pos) & SAU_CTRL_ENABLE_Msk);
}

#define NON_SECURE_START 0x10080000

int main(void)
{
	int ret;

  BOARD_InitTrustZone();

  SYSCFG_DL_init();

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

  // LCD setup using Graphics Library API calls
  Kitronix320x240x16_SSD2119Init();
  Graphics_initContext(&g_sContext, &g_sKitronix320x240x16_SSD2119);
  Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
  Graphics_setFont(&g_sContext, &g_sFontCmss20b);
  Graphics_clearDisplay(&g_sContext);

  //Draw TI Logo
  drawTiLogo(&g_sContext, boot_str);
  k_msleep(3000);
  while(1) {
    k_msleep(SLEEP_TIME_MS);
  }
  #if 0
  while(1) {
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0) {
      return 0;
    }

    led_state = !led_state;
    printf("LED state: %s\n", led_state ? "ON" : "OFF");
    k_msleep(SLEEP_TIME_MS);
  }
  #endif

  //TZM_JumpToNormalWorld(NON_SECURE_START);

	//return 0;
}
