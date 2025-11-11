/* --COPYRIGHT--,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//TODO
//*****************************************************************************
//
// HAL_MSP_EXP430F5529LP_KITRONIX320X240_SSD2119_SPI.h - Prototypes for the
//           KITRONIX320X240_SSD2119 LCD display driver.
//
//
//                 MSP430F5529                 BOOSTXL-K350QVG-S1
//                -----------------              ------------
//               |     P1.6/UCB0SIMO|---------> |LCD_SDI     |
//            /|\|                  |           |            |
//             | |      P1.5/UCB0CLK|---------> |LCD_SCL     |
//             --|RST               |           |            |
//               |              P5.0|---------> |LCD_SCS     |
//               |              P4.6|---------> |LCD_SDC     |
//               |              P3.5|---------> |LCD_RESET   |
//               |        P2.7/TA2.2|---------> |LCD_PWM     |
//               |                  |           |            |
//               |                  |            ------------
//                ------------------
//****************************************************************************

#ifndef __HAL_MSP_EXP430F5529_DOGS102X64_UC1701__
#define __HAL_MSP_EXP430F5529_DOGS102X64_UC1701__

//*****************************************************************************
//
// User Configuration for the LCD Driver
//
//*****************************************************************************
// MCLK FREQUENCY (in Hz)
#define HAL_LCD_MCLK_FREQUENCY      25000000

#define GRLIB_MSP432_MODE                       1

//// Ports from MSP430 connected to LCD
//#define LCD_SDI_PORT       GPIO_PORT_P1
//#define LCD_SCL_PORT       GPIO_PORT_P1
//#define LCD_SCS_PORT       GPIO_PORT_P5
//#define LCD_SDC_PORT       GPIO_PORT_P4
//#define LCD_RESET_PORT     GPIO_PORT_P3
//#define LCD_PWM_PORT       GPIO_PORT_P2
//
//// Pins from MSP430 connected to LCD
//#define LCD_SDI_PIN                      GPIO_PIN6
//#define LCD_SDI_PIN_FUNCTION             GPIO_PRIMARY_MODULE_FUNCTION
//#define LCD_SCL_PIN                      GPIO_PIN5
//#define LCD_SCL_PIN_FUNCTION             GPIO_PRIMARY_MODULE_FUNCTION
//#define LCD_SCS_PIN                      GPIO_PIN0
//#define LCD_SDC_PIN                      GPIO_PIN6
//#define LCD_RESET_PIN                    GPIO_PIN5
//#define LCD_PWM_PIN                          GPIO_PIN7

// Pins from MSP430 mode selection

//// Definition of USCI base address to be used for SPI communication
//#define LCD_EUSCI_MODULE                      EUSCI_B0_BASE
//
//// Definition of TIMER_A base address to be used for backlight control
//#define LCD_TIMER_BASE_BKLT       TIMER_B0_BASE

//*****************************************************************************
//
// Deprecated names.  These definitions ensure backwards compatibility
// but new code should avoid using deprecated struct names since these will
// be removed at some point in the future.
//
//*****************************************************************************

//*****************************************************************************
//
// Prototypes for the globals exported by this driver.
//
//*****************************************************************************
extern void HAL_LCD_initLCD(void);
extern void HAL_LCD_writeCommand(uint8_t command);
extern void HAL_LCD_writeData(uint16_t data);
extern void HAL_LCD_delay(uint16_t msec);
extern void HAL_LCD_selectLCD(void);
extern void HAL_LCD_deselectLCD(void);
extern void HAL_LCD_initTimer(uint16_t captureCompareVal);
extern uint16_t HAL_LCD_getTimerCaptureCompare();
extern void HAL_LCD_setTimerDutyCycle(uint16_t dutyCycle);
extern void HAL_LCD_startTimerCounter(void);
extern void HAL_LCD_stopTimerCounter(void);

#endif // __HAL_MSP_EXP430F5529_DOGS102X64_UC1701__
