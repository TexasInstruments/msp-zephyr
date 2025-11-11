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
//*****************************************************************************
//
// kitronix320x240x16_ssd2119_16bit.c - Display driver for the Kitronix
//                                     K350QVG-V1-F TFT display with an SSD2119
//                                     controller.
//
// Copyright (c) 2008-2011 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
//
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
//
//*****************************************************************************
//
//! \addtogroup display_api
//! @{
//
//*****************************************************************************

#include "ti_msp_dl_config.h"
#include "grlib.h"
#include "HAL_MSP_M0G3519_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"

//*****************************************************************************
//
//! Initializes the display driver.
//!
//! This function initializes the SSD2119 display controller on the panel,
//! preparing it to display data.
//!
//! \return None.
//
//*****************************************************************************
void
Kitronix320x240x16_SSD2119Init(void)
{
    uint32_t ulCount;
    volatile uint32_t i;

    HAL_LCD_initLCD();

    //
    // Select the LCD for SPI comunication.
    //
    HAL_LCD_selectLCD();

    //
    // Enter sleep mode (if we are not already there).
    //
    HAL_LCD_writeCommand(SSD2119_SLEEP_MODE_1_REG);
    HAL_LCD_writeData(0x0001);

    //
    // Set initial power parameters.
    //
    HAL_LCD_writeCommand(SSD2119_PWR_CTRL_5_REG);
    HAL_LCD_writeData(0x00B2);
    HAL_LCD_writeCommand(SSD2119_VCOM_OTP_1_REG);
    HAL_LCD_writeData(0x0006);

    //
    // Start the oscillator.
    //
    HAL_LCD_writeCommand(SSD2119_OSC_START_REG);
    HAL_LCD_writeData(0x0001);

    //
    // Set pixel format and basic display orientation (scanning direction).
    //
    HAL_LCD_writeCommand(SSD2119_OUTPUT_CTRL_REG);
    HAL_LCD_writeData(0x30EF);
    HAL_LCD_writeCommand(SSD2119_LCD_DRIVE_AC_CTRL_REG);
    HAL_LCD_writeData(0x0600);

    //
    // Exit sleep mode.
    //
    HAL_LCD_writeCommand(SSD2119_SLEEP_MODE_1_REG);
    HAL_LCD_writeData(0x0000);

    //
    // Delay 30mS
    //
    HAL_LCD_delay(30);

    //
    // Configure pixel color format and MCU interface parameters.
    //
    HAL_LCD_writeCommand(SSD2119_ENTRY_MODE_REG);
    HAL_LCD_writeData(ENTRY_MODE_DEFAULT);

    //
    // Set analog parameters.
    //
    HAL_LCD_writeCommand(SSD2119_SLEEP_MODE_2_REG);
    HAL_LCD_writeData(0x0999);
    HAL_LCD_writeCommand(SSD2119_ANALOG_SET_REG);
    HAL_LCD_writeData(0x3800);

    //
    // Enable the display.
    //
    HAL_LCD_writeCommand(SSD2119_DISPLAY_CTRL_REG);
    HAL_LCD_writeData(0x0033);

    //
    // Set VCIX2 voltage to 6.1V.
    //
    HAL_LCD_writeCommand(SSD2119_PWR_CTRL_2_REG);
    HAL_LCD_writeData(0x0005);

    //
    // Configure gamma correction.
    //
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_1_REG);
    HAL_LCD_writeData(0x0000);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_2_REG);
    HAL_LCD_writeData(0x0303);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_3_REG);
    HAL_LCD_writeData(0x0407);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_4_REG);
    HAL_LCD_writeData(0x0301);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_5_REG);
    HAL_LCD_writeData(0x0301);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_6_REG);
    HAL_LCD_writeData(0x0403);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_7_REG);
    HAL_LCD_writeData(0x0707);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_8_REG);
    HAL_LCD_writeData(0x0400);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_9_REG);
    HAL_LCD_writeData(0x0a00);
    HAL_LCD_writeCommand(SSD2119_GAMMA_CTRL_10_REG);
    HAL_LCD_writeData(0x1000);

    //
    // Configure Vlcd63 and VCOMl.
    //
    HAL_LCD_writeCommand(SSD2119_PWR_CTRL_3_REG);
    HAL_LCD_writeData(0x000A);
    HAL_LCD_writeCommand(SSD2119_PWR_CTRL_4_REG);
    HAL_LCD_writeData(0x2E00);

    //
    // Set the display size and ensure that the GRAM window is set to allow
    // access to the full display buffer.
    //
    HAL_LCD_writeCommand(SSD2119_V_RAM_POS_REG);
    HAL_LCD_writeData((uint16_t)(LCD_VERTICAL_MAX - 1) << 8);
    HAL_LCD_writeCommand(SSD2119_H_RAM_START_REG);
    HAL_LCD_writeData(0x0000);
    HAL_LCD_writeCommand(SSD2119_H_RAM_END_REG);
    HAL_LCD_writeData(LCD_HORIZONTAL_MAX - 1);
    HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
    HAL_LCD_writeData(0x00);
    HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
    HAL_LCD_writeData(0x00);

    //
    // Clear the contents of the display buffer.
    //
    HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
    for(ulCount = 0; ulCount < 76800; ulCount++)
    {
        HAL_LCD_writeData(0x0000);    // Black
        //HAL_LCD_writeData(0xF800);    // Red
        //HAL_LCD_writeData(0x07E0);    // Green
        //HAL_LCD_writeData(0x001F);    // Blue
    }

    //
    // Deselect the LCD for SPI comunication.
    //
    HAL_LCD_deselectLCD();
}

//
// Sets the cursor to coordinates X, Y. Increment from Left to Right
//
// \param X and Y are the LCD pixel coordinates to place the cursor
//
// This macro sets the cursor location, and sets auto incrementing
// of the X value from left to right on the LCD screen. This is used
// when drawing rows of pixels in images or lines. Upon exiting this
// macro, the LCD should be ready to accept a stream of data
//
// Note that left to right is relative to the screen orientation, but
// HORIZ_DIRECTION is defined depending on screen orientation to always
// be left to right. (See Coordinate Space and Mapping above)
//
// \return None

void Kitronix320x240x16_SSD2119_setCursorLtoR(uint16_t X,
                                              uint16_t Y)
{
    HAL_LCD_writeCommand(SSD2119_ENTRY_MODE_REG);
    HAL_LCD_writeData(MAKE_ENTRY_MODE(HORIZ_DIRECTION));
    HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
    HAL_LCD_writeData(MAPPED_X(X, Y));
    HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
    HAL_LCD_writeData(MAPPED_Y(X, Y));
    HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
}

//
// Sets the cursor to coordinates X, Y. Increment from Top to Bottom
//
// \param X and Y are the LCD pixel coordinates to place the cursor
//
// This macro sets the cursor location, and sets auto incrementing
// of the Y value from top to bottom on the LCD screen. This is used
// when drawing rows of pixels in images or lines. Upon exiting this
// macro, the LCD should be ready to accept a stream of data
//
// Note that top to bottom is relative to the screen orientation, but
// VERT_DIRECTION is defined depending on screen orientation to always
// be top to bottom. (See Coordinate Space and Mapping above)
//
// \return None

void Kitronix320x240x16_SSD2119_setCursorTtoB(uint16_t X,
                                              uint16_t Y)
{
    HAL_LCD_writeCommand(SSD2119_ENTRY_MODE_REG);
    HAL_LCD_writeData(MAKE_ENTRY_MODE(VERT_DIRECTION));
    HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
    HAL_LCD_writeData(MAPPED_X(X, Y));
    HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
    HAL_LCD_writeData(MAPPED_Y(X, Y));
    HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
}

//*****************************************************************************
//
//! Draws a pixel on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param X is the X coordinate of the pixel.
//! \param Y is the Y coordinate of the pixel.
//! \param Value is the color of the pixel.
//!
//! This function sets the given pixel to a particular color.  The coordinates
//! of the pixel are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119PixelDraw(void *pvDisplayData,
                                    int16_t X,
                                    int16_t Y,
                                    uint16_t Value)
{
    //
    // Select the LCD for SPI comunication.
    //
    HAL_LCD_selectLCD();

    //
    // Set the X address of the display cursor.
    //
    HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
    HAL_LCD_writeData(MAPPED_X(X, Y));

    //
    // Set the Y address of the display cursor.
    //
    HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
    HAL_LCD_writeData(MAPPED_Y(X, Y));

    //
    // Write the pixel value.
    //
    HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
    HAL_LCD_writeData(Value);

    //
    // Deselect the LCD for SPI comunication.
    //
    HAL_LCD_deselectLCD();
}

//*****************************************************************************
//
//! Draws a horizontal sequence of pixels on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param X is the X coordinate of the first pixel.
//! \param Y is the Y coordinate of the first pixel.
//! \param X0 is sub-pixel offset within the pixel data, which is valid for 1
//! or 4 bit per pixel formats.
//! \param Count is the number of pixels to draw.
//! \param BPP is the number of bits per pixel; must be 1, 4, or 8.
//! \param puint8Data is a pointer to the pixel data.  For 1 and 4 bit per pixel
//! formats, the most significant bit(s) represent the left-most pixel.
//! \param pucPalette is a pointer to the palette used to draw the pixels.
//!
//! This function draws a horizontal sequence of pixels on the screen, using
//! the supplied palette.  For 1 bit per pixel format, the palette contains
//! pre-translated colors; for 4 and 8 bit per pixel formats, the palette
//! contains 24-bit RGB values that must be translated before being written to
//! the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119PixelDrawMultiple(void *pvDisplayData,
                                            int16_t X,
                                            int16_t Y,
                                            int16_t X0,
                                            int16_t Count,
                                            int16_t BPP,
                                            const uint8_t *puint8Data,
                                            const uint32_t *pucPalette)
{
    uint16_t Data;

    //
    // Select the LCD for SPI comunication.
    //
    HAL_LCD_selectLCD();

    //
    // Set the cursor increment to left to right, followed by top to bottom.
    //
    Kitronix320x240x16_SSD2119_setCursorLtoR(X, Y);

    //
    // Determine how to interpret the pixel data based on the number of bits
    // per pixel.
    //
    switch(BPP)
    {
    // The pixel data is in 1 bit per pixel format
    case 1:
    {
        // Loop while there are more pixels to draw
        while(Count > 0)
        {
            // Get the next byte of image data
            Data = *puint8Data++;

            // Loop through the pixels in this byte of image data
            for(; (X0 < 8) && Count; X0++, Count--)
            {
                // Draw this pixel in the appropriate color
                HAL_LCD_writeData(((uint32_t *)pucPalette)[(Data >>
                                                            (7 - X0)) & 1]);
            }

            // Start at the beginning of the next byte of image data
            X0 = 0;
        }
        // The image data has been drawn

        break;
    }

    // The pixel data is in 4 bit per pixel format
    case 4:
    {
        // Loop while there are more pixels to draw.  "Duff's device" is
        // used to jump into the middle of the loop if the first nibble of
        // the pixel data should not be used.  Duff's device makes use of
        // the fact that a case statement is legal anywhere within a
        // sub-block of a switch statement.  See
        // http://en.wikipedia.org/wiki/Duff's_device for detailed
        // information about Duff's device.
        switch(X0 & 1)
        {
        case 0:

            while(Count)
            {
                // Get the upper nibble of the next byte of pixel data
                // and extract the corresponding entry from the palette
                Data = (*puint8Data >> 4);
                Data = (*(uint16_t *)(pucPalette + Data));
                // Write to LCD screen
                HAL_LCD_writeData(Data);

                // Decrement the count of pixels to draw
                Count--;

                // See if there is another pixel to draw
                if(Count)
                {
                case 1:
                    // Get the lower nibble of the next byte of pixel
                    // data and extract the corresponding entry from
                    // the palette
                    Data = (*puint8Data++ & 15);
                    Data = (*(uint16_t *)(pucPalette + Data));
                    // Write to LCD screen
                    HAL_LCD_writeData(Data);

                    // Decrement the count of pixels to draw
                    Count--;
                }
            }
        }
        // The image data has been drawn.

        break;
    }

    // The pixel data is in 8 bit per pixel format
    case 8:
    {
        // Loop while there are more pixels to draw
        while(Count--)
        {
            // Get the next byte of pixel data and extract the
            // corresponding entry from the palette
            Data = *puint8Data++;
            Data = (*(uint16_t *)(pucPalette + Data));
            // Write to LCD screen
            HAL_LCD_writeData(Data);
        }
        // The image data has been drawn
        break;
    }

    //
    // We are being passed data in the display's native format.  Merely
    // write it directly to the display.  This is a special case which is
    // not used by the graphics library but which is helpful to
    // applications which may want to handle, for example, JPEG images.
    //
    case 16:
    {
        uint16_t usData;

        // Loop while there are more pixels to draw.

        while(Count--)
        {
            // Get the next byte of pixel data and extract the
            // corresponding entry from the palette
            usData = *((uint16_t *)puint8Data);
            puint8Data += 2;

            // Translate this palette entry and write it to the screen
            HAL_LCD_writeData(usData);
        }
    }
    }

    //
    // Deselect the LCD for SPI comunication.
    //
    HAL_LCD_deselectLCD();
}

//*****************************************************************************
//
//! Draws a horizontal line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param X1 is the X coordinate of the start of the line.
//! \param X2 is the X coordinate of the end of the line.
//! \param Y is the Y coordinate of the line.
//! \param Value is the color of the line.
//!
//! This function draws a horizontal line on the display.  The coordinates of
//! the line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119LineDrawH(void *pvDisplayData,
                                    int16_t X1,
                                    int16_t X2,
                                    int16_t Y,
                                    uint16_t Value)
{
    //
    // Select the LCD for SPI comunication.
    //
    HAL_LCD_selectLCD();

    //
    // Set the cursor increment to left to right, followed by top to bottom.
    //
    Kitronix320x240x16_SSD2119_setCursorLtoR(X1, Y);

    //
    // Loop through the pixels of this horizontal line.
    //
    while(X1++ <= X2)
    {
        //
        // Write the pixel value.
        //
        HAL_LCD_writeData(Value);
    }

    //
    // Deselect the LCD for SPI comunication.
    //
    HAL_LCD_deselectLCD();
}

//*****************************************************************************
//
//! Draws a vertical line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param X is the X coordinate of the line.
//! \param Y1 is the Y coordinate of the start of the line.
//! \param Y2 is the Y coordinate of the end of the line.
//! \param Value is the color of the line.
//!
//! This function draws a vertical line on the display.  The coordinates of the
//! line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119LineDrawV(void *pvDisplayData,
                                    int16_t X,
                                    int16_t Y1,
                                    int16_t Y2,
                                    uint16_t Value)
{
    //
    // Select the LCD for SPI comunication.
    //
    HAL_LCD_selectLCD();

    //
    // Set the cursor increment top to bottom, followed by left to right
    //
    Kitronix320x240x16_SSD2119_setCursorTtoB(X, Y1);

    //
    // Loop through the pixels of this vertical line.
    //
    while(Y1++ <= Y2)
    {
        //
        // Write the pixel value.
        //
        HAL_LCD_writeData(Value);
    }

    //
    // Deselect the LCD for SPI comunication.
    //
    HAL_LCD_deselectLCD();
}

//*****************************************************************************
//
//! Draws a line.
//!
//! \param pContext is a pointer to the drawing context to use.
//! \param lX1 is the X coordinate of the start of the line.
//! \param lY1 is the Y coordinate of the start of the line.
//! \param lX2 is the X coordinate of the end of the line.
//! \param lY2 is the Y coordinate of the end of the line.
//!
//! This function uses Bresenham's line drawing algorithm.
//!
//! \return None.
//
//*****************************************************************************
void
Kitronix320x240x16_SSD2119LineDraw(void *pvDisplayData,
                                   int16_t lX1,
                                   int16_t lY1,
                                   int16_t lX2,
                                   int16_t lY2,
                                   uint16_t Value)
{
    int16_t lError, lDeltaX, lDeltaY, lYStep, bSteep;

    //
    // Determine if the line is steep.  A steep line has more motion in the Y
    // direction than the X direction.
    //
    if(((lY2 > lY1) ? (lY2 - lY1) : (lY1 - lY2)) >
       ((lX2 > lX1) ? (lX2 - lX1) : (lX1 - lX2)))
    {
        bSteep = 1;
    }
    else
    {
        bSteep = 0;
    }

    //
    // If the line is steep, then swap the X and Y coordinates.
    //
    if(bSteep)
    {
        lError = lX1;
        lX1 = lY1;
        lY1 = lError;
        lError = lX2;
        lX2 = lY2;
        lY2 = lError;
    }

    //
    // If the starting X coordinate is larger than the ending X coordinate,
    // then swap the start and end coordinates.
    //
    if(lX1 > lX2)
    {
        lError = lX1;
        lX1 = lX2;
        lX2 = lError;
        lError = lY1;
        lY1 = lY2;
        lY2 = lError;
    }

    //
    // Compute the difference between the start and end coordinates in each
    // axis.
    //
    lDeltaX = lX2 - lX1;
    lDeltaY = (lY2 > lY1) ? (lY2 - lY1) : (lY1 - lY2);

    //
    // Initialize the error term to negative half the X delta.
    //
    lError = -lDeltaX / 2;

    //
    // Determine the direction to step in the Y axis when required.
    //
    if(lY1 < lY2)
    {
        lYStep = 1;
    }
    else
    {
        lYStep = -1;
    }

    //
    // Select the LCD for SPI comunication.
    //
    HAL_LCD_selectLCD();

    //
    // Loop through all the points along the X axis of the line.
    //
    for(; lX1 <= lX2; lX1++)
    {
        //
        // See if this is a steep line.
        //
        if(bSteep)
        {
            //
            // Plot this point of the line, swapping the X and Y coordinates.
            //

            //
            // Set the X address of the display cursor.
            //
            HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
            HAL_LCD_writeData(MAPPED_X(lY1, lX1));

            //
            // Set the Y address of the display cursor.
            //
            HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
            HAL_LCD_writeData(MAPPED_Y(lY1, lX1));

            //
            // Write the pixel value.
            //
            HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
            HAL_LCD_writeData(Value);
        }
        else
        {
            //
            // Plot this point of the line, using the coordinates as is.
            //

            //
            // Set the X address of the display cursor.
            //
            HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
            HAL_LCD_writeData(MAPPED_X(lX1, lY1));

            //
            // Set the Y address of the display cursor.
            //
            HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
            HAL_LCD_writeData(MAPPED_Y(lX1, lY1));

            //
            // Write the pixel value.
            //
            HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
            HAL_LCD_writeData(Value);
        }

        //
        // Increment the error term by the Y delta.
        //
        lError += lDeltaY;

        //
        // See if the error term is now greater than zero.
        //
        if(lError > 0)
        {
            //
            // Take a step in the Y axis.
            //
            lY1 += lYStep;

            //
            // Decrement the error term by the X delta.
            //
            lError -= lDeltaX;
        }
    }

    //
    // Deselect the LCD for SPI comunication.
    //
    HAL_LCD_deselectLCD();
}

//*****************************************************************************
//
//! Fills a rectangle.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param pRect is a pointer to the structure describing the rectangle.
//! \param ulValue is the color of the rectangle.
//!
//! This function fills a rectangle on the display.  The coordinates of the
//! rectangle are assumed to be within the extents of the display, and the
//! rectangle specification is fully inclusive (in other words, both sXMin and
//! sXMax are drawn, along with sYMin and sYMax).
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119RectFill(void *pvDisplayData,
                                   const Graphics_Rectangle *pRect,
                                   uint16_t ulValue)
{
    int16_t x0 = pRect->sXMin;
    int16_t x1 = pRect->sXMax;
    int16_t y0 = pRect->sYMin;
    int16_t y1 = pRect->sYMax;

    // Less horizontal lines
    if((x1 - x0) > (y1 - y0))     // Less horizontal lines
    {
        for(; y0 <= y1; y0++)
        {
            Kitronix320x240x16_SSD2119LineDrawH(pvDisplayData, x0, x1, y0,
                                                ulValue);
        }
    }
    else                          // Less vertical lines
    {
        for(; x0 <= x1; x0++)
        {
            Kitronix320x240x16_SSD2119LineDrawV(pvDisplayData, x0, y0, y1,
                                                ulValue);
        }
    }
}

//*****************************************************************************
//
//! Translates a 24-bit RGB color to a display driver-specific color.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param ulValue is the 24-bit RGB color.  The least-significant byte is the
//! blue channel, the next byte is the green channel, and the third byte is the
//! red channel.
//!
//! This function translates a 24-bit RGB color into a value that can be
//! written into the display's frame buffer in order to reproduce that color,
//! or the closest possible approximation of that color.
//!
//! \return Returns the display-driver specific color.
//
//*****************************************************************************
static uint32_t
Kitronix320x240x16_SSD2119ColorTranslate(void *pvDisplayData,
                                         uint32_t ulValue)
{
    //
    // Translate from a 24-bit RGB color to a 5-6-5 RGB color.
    //
    return(((((ulValue) & 0x00f80000) >> 8) |
            (((ulValue) & 0x0000fc00) >> 5) |
            (((ulValue) & 0x000000f8) >> 3)));
}

//*****************************************************************************
//
//! Flushes any cached drawing operations.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//! This functions flushes any cached drawing operations to the display.  This
//! is useful when a local frame buffer is used for drawing operations, and the
//! flush would copy the local frame buffer to the display.  For the SSD2119
//! driver, the flush is a no operation.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119Flush(void *pvDisplayData)
{
    //
    // There is nothing to be done.
    //
}

//*****************************************************************************
//
//! Send command to clear screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//! This function does a clear screen and the Display Buffer contents
//! are initialized to the current background color.
//!
//! \return None.
//
//*****************************************************************************
static void
Kitronix320x240x16_SSD2119ClearScreen(void *pvDisplayData,
                                      uint16_t ulValue)
{
    uint16_t y0;

    for(y0 = 0; y0 < LCD_VERTICAL_MAX; y0++)
    {
        Kitronix320x240x16_SSD2119LineDrawH(pvDisplayData, 0,
                                            LCD_HORIZONTAL_MAX - 1, y0,
                                            ulValue);
    }
}

//*****************************************************************************
//
//! The display structure that describes the driver for the Kitronix
//! K350QVG-V1-F TFT panel with an SSD2119 controller.
//
//*****************************************************************************
const Graphics_Display g_sKitronix320x240x16_SSD2119 =
{
    sizeof(tDisplay),
    0,
#if defined(PORTRAIT) || defined(PORTRAIT_FLIP)
    240,
    320,
#else
    320,
    240,
#endif
    Kitronix320x240x16_SSD2119PixelDraw,
    Kitronix320x240x16_SSD2119PixelDrawMultiple,
    Kitronix320x240x16_SSD2119LineDrawH,
    Kitronix320x240x16_SSD2119LineDrawV,
    Kitronix320x240x16_SSD2119RectFill,
    Kitronix320x240x16_SSD2119ColorTranslate,
    Kitronix320x240x16_SSD2119Flush,
    Kitronix320x240x16_SSD2119ClearScreen
};

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
