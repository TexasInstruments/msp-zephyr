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
// TODO update this diagram
//*****************************************************************************
//
// HAL_MSP-EXP430FR5969_Sharp96x96.c
//
//*****************************************************************************
//
//! \addtogroup display_api
//! @{
//
//*****************************************************************************

#include "grlib.h"
//#include <LcdDriver/HAL_MSP_M0G3519_KITRONIX320X240_SSD2119_SPI.h>
#include "HAL_MSP_M0G3519_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "ti_msp_dl_config.h"

//*****************************************************************************
//
//! Initializes the display driver.
//!
//! This function initializes the Sharp96x96 display. This function
//! configures the GPIO pins used to control the LCD display when the basic
//! GPIO interface is in use. On exit, the LCD has been reset and is ready to
//! receive command and data writes.
//!
//! \return None.
//
//*****************************************************************************
void HAL_LCD_initLCD(void)
{
    //
    // Configure the pins that connect to the LCD as GPIO outputs.
    //
    DL_GPIO_clearPins(LCD_RESET_PORT, LCD_RESET_PIN);

    DL_GPIO_clearPins(LCD_SDC_PORT, LCD_SDC_PIN);

    DL_GPIO_clearPins(LCD_SCS_PORT, LCD_SCS_PIN);

    //
    // Set the LCD Backlight high to enable
    //
    DL_GPIO_setPins(LCD_PWM_PORT, LCD_PWM_PIN);

    //
    // Set the LCD control pins to their default values.
    //
    DL_GPIO_setPins(LCD_SDC_PORT, LCD_SDC_PIN);

    DL_GPIO_clearPins(LCD_SCS_PORT, LCD_SCS_PIN);

    //
    // Delay for 1ms.
    //
    HAL_LCD_delay(1);

    //
    // Deassert the LCD reset signal.
    //
    DL_GPIO_setPins(LCD_RESET_PORT, LCD_RESET_PIN);

    //
    // Delay for 1ms while the LCD comes out of reset.
    //
    HAL_LCD_delay(1);
}

//*****************************************************************************
//
// Writes a command to the UC1701.  This function implements the basic SPI
// interface to the LCD display.
//
//*****************************************************************************
void HAL_LCD_writeCommand(uint8_t command)
{
    /* Wait until all bytes have been transmitted and the TX FIFO is empty */
    while (DL_SPI_isBusy(SPI_0_INST))
        ;

    //
    // Set the LCD_SDC signal low, indicating that following writes are commands.
    //
    DL_GPIO_clearPins(LCD_SDC_PORT, LCD_SDC_PIN);

    //
    // Transmit the command.
    //
    DL_SPI_transmitDataBlocking8(SPI_0_INST, command);

    /* Wait until all bytes have been transmitted and the TX FIFO is empty */
    while (DL_SPI_isBusy(SPI_0_INST))
        ;

    //
    // Set the LCD_SDC signal high, indicating that following writes are data.
    //
    DL_GPIO_setPins(LCD_SDC_PORT, LCD_SDC_PIN);

}

//*****************************************************************************
//
// Writes a data word to the UC1701.  This function implements the basic SPI
// interface to the LCD display.
//
//*****************************************************************************
void HAL_LCD_writeData(uint16_t data)
{
    uint8_t ui8Data;

    //
    // Calculate the high byte to transmit.
    //
    ui8Data = (uint8_t)(data >> 8);

    /* Wait until all bytes have been transmitted and the TX FIFO is empty */
    while (DL_SPI_isBusy(SPI_0_INST))
        ;

    //
    // Transmit the high byte.
    //
    DL_SPI_transmitDataBlocking8(SPI_0_INST,ui8Data);

    //
    // Calculate the low byte to transmit.
    //
    ui8Data = (uint8_t)(data & 0xff);

    /* Wait until all bytes have been transmitted and the TX FIFO is empty */
    while (DL_SPI_isBusy(SPI_0_INST))
        ;

    //
    // Transmit the high byte.
    //
    DL_SPI_transmitDataBlocking8(SPI_0_INST,ui8Data);
}

//*****************************************************************************
//
// Clears CS line
//
// This macro allows to clear the Chip Select (CS) line
//
// \return None
//
//*****************************************************************************

void HAL_LCD_selectLCD(){
    /* Wait until all bytes have been transmitted and the TX FIFO is empty */
    while (DL_SPI_isBusy(SPI_0_INST))
        ;

    DL_GPIO_clearPins(LCD_SCS_PORT, LCD_SCS_PIN);
}

//*****************************************************************************
//
// Set CS line
//
// This macro allows to set the Chip Select (CS) line
//
// \return None
//
//*****************************************************************************

void HAL_LCD_deselectLCD(){
    /* Wait until all bytes have been transmitted and the TX FIFO is empty */
    while (DL_SPI_isBusy(SPI_0_INST))
        ;

    DL_GPIO_setPins(LCD_SCS_PORT, LCD_SCS_PIN);
}

//*****************************************************************************
//
// Generates delay of
//
// \param cycles number of cycles to delay
//
// \return None
//
//*****************************************************************************
void HAL_LCD_delay(uint16_t msec)
{
    for(uint16_t n = 0; n < msec; n++)
    {
        delay_cycles(32000); //1ms
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
