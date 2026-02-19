/*
 * Copyright (c) 2026 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * I2C MSP Compatibility Header
 *
 * Provides a unified HAL interface for the I2C driver across
 * MSPM0 (legacy I2C) and MSPM33 (UNICOMM I2C) platforms.
 *
 * The driver uses MSPM0-style naming (DL_I2C_*), which provides
 * descriptive Controller/Target infix naming for all functions
 * and constants.
 *
 * On MSPM0, most names map directly to the native HAL. A small set
 * of defines adds Controller/Target variants for the interrupt
 * functions and timeout constants that MSPM0 shares across both modes.
 *
 * On UNICOMM (MSPM33), this header maps all DL_I2C_* names to the
 * native DL_I2CC_* / DL_I2CT_* HAL equivalents.
 */

#ifndef ZEPHYR_DRIVERS_I2C_I2C_MSP_COMPAT_H_
#define ZEPHYR_DRIVERS_I2C_I2C_MSP_COMPAT_H_

#ifdef CONFIG_HAS_MSP_UNICOMM

/* MSPM33 UNICOMM I2C - map DL_I2C_* to native DL_I2CC_ / DL_I2CT_ */
#include <driverlib/dl_unicommi2cc.h>
#include <driverlib/dl_unicommi2ct.h>

typedef UNICOMM_Inst_Regs * i2c_msp_base_t;
typedef DL_I2CC_ClockConfig i2c_msp_clock_config_t;

/*
 * Shared / power functions -> controller sub-peripheral
 */
#define DL_I2C_reset                         DL_I2CC_reset
#define DL_I2C_enablePower                   DL_I2CC_enablePower
#define DL_I2C_disablePower                  DL_I2CC_disablePower
#define DL_I2C_setClockConfig                DL_I2CC_setClockConfig
#define DL_I2C_disableAnalogGlitchFilter     DL_I2CC_disableAnalogGlitchFilter
#define DL_I2C_setTimerPeriod                DL_I2CC_setTimerPeriod
#define DL_I2C_enableTimeoutA                DL_I2CC_enableTimeoutA
#define DL_I2C_setTimeoutACount              DL_I2CC_setTimeoutACount

/*
 * Controller functions
 */
#define DL_I2C_enableController              DL_I2CC_enable
#define DL_I2C_disableController             DL_I2CC_disable
#define DL_I2C_resetControllerTransfer       DL_I2CC_resetTransfer
#define DL_I2C_startControllerTransferAdvanced DL_I2CC_startTransferAdvanced
#define DL_I2C_fillControllerTXFIFO          DL_I2CC_fillTXFIFO
#define DL_I2C_flushControllerTXFIFO         DL_I2CC_flushTXFIFO
#define DL_I2C_isControllerRXFIFOEmpty       DL_I2CC_isRXFIFOEmpty
#define DL_I2C_receiveControllerData         DL_I2CC_receiveData
#define DL_I2C_getControllerStatus           DL_I2CC_getStatus
#define DL_I2C_setControllerTXFIFOThreshold  DL_I2CC_setTXFIFOThreshold
#define DL_I2C_setControllerRXFIFOThreshold  DL_I2CC_setRXFIFOThreshold
#define DL_I2C_enableControllerClockStretching DL_I2CC_enableClockStretching

/*
 * Target functions
 */
#define DL_I2C_enableTarget                  DL_I2CT_enable
#define DL_I2C_disableTarget                 DL_I2CT_disable
#define DL_I2C_setTargetOwnAddress           DL_I2CT_setOwnAddress
#define DL_I2C_setTargetTXFIFOThreshold      DL_I2CT_setTXFIFOThreshold
#define DL_I2C_setTargetRXFIFOThreshold      DL_I2CT_setRXFIFOThreshold
#define DL_I2C_enableTargetTXTriggerInTXMode DL_I2CT_enableTXTriggerInTXMode
#define DL_I2C_enableTargetTXEmptyOnTXRequest DL_I2CT_enableTXEmptyOnTXRequest
#define DL_I2C_enableTargetClockStretching   DL_I2CT_enableClockStretching
#define DL_I2C_flushTargetTXFIFO             DL_I2CT_flushTXFIFO
#define DL_I2C_setTargetACKOverrideValue     DL_I2CT_setACKOverrideValue
#define DL_I2C_isTargetRXFIFOEmpty           DL_I2CT_isRXFIFOEmpty
#define DL_I2C_receiveTargetData             DL_I2CT_receiveData
#define DL_I2C_transmitTargetData            DL_I2CT_transmitData
#define DL_I2C_transmitTargetDataCheck       DL_I2CT_transmitDataCheck
#define DL_I2C_disableTargetWakeup           DL_I2CT_disableWakeup

/*
 * Interrupt functions - UNICOMM has separate controller/target registers
 */
#define DL_I2C_enableControllerInterrupt      DL_I2CC_enableInterrupt
#define DL_I2C_disableControllerInterrupt     DL_I2CC_disableInterrupt
#define DL_I2C_clearControllerInterruptStatus DL_I2CC_clearInterruptStatus
#define DL_I2C_getControllerPendingInterrupt  DL_I2CC_getPendingInterrupt

#define DL_I2C_enableTargetInterrupt          DL_I2CT_enableInterrupt
#define DL_I2C_disableTargetInterrupt         DL_I2CT_disableInterrupt
#define DL_I2C_clearTargetInterruptStatus     DL_I2CT_clearInterruptStatus
#define DL_I2C_getTargetPendingInterrupt      DL_I2CT_getPendingInterrupt

/*
 * Controller IIDX constants
 */
#define DL_I2C_IIDX_CONTROLLER_STOP           DL_I2CC_IIDX_STOP
#define DL_I2C_IIDX_CONTROLLER_TX_DONE        DL_I2CC_IIDX_TX_DONE
#define DL_I2C_IIDX_CONTROLLER_RXFIFO_TRIGGER DL_I2CC_IIDX_RXFIFO_TRIGGER
#define DL_I2C_IIDX_CONTROLLER_TXFIFO_TRIGGER DL_I2CC_IIDX_TXFIFO_TRIGGER
#define DL_I2C_IIDX_CONTROLLER_NACK           DL_I2CC_IIDX_NACK
#define DL_I2C_IIDX_CONTROLLER_TIMEOUT_A      DL_I2CC_IIDX_TIMEOUT_A

/*
 * Target IIDX constants
 */
#define DL_I2C_IIDX_TARGET_START              DL_I2CT_IIDX_START
#define DL_I2C_IIDX_TARGET_RX_DONE            DL_I2CT_IIDX_RX_DONE
#define DL_I2C_IIDX_TARGET_TXFIFO_EMPTY       DL_I2CT_IIDX_TXFIFO_EMPTY
#define DL_I2C_IIDX_TARGET_STOP               DL_I2CT_IIDX_STOP
#define DL_I2C_IIDX_TARGET_TIMEOUT_A          DL_I2CT_IIDX_TIMEOUT_A

/*
 * Controller interrupt mask constants
 */
#define DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST DL_I2CC_INTERRUPT_ARBITRATION_LOST
#define DL_I2C_INTERRUPT_CONTROLLER_NACK             DL_I2CC_INTERRUPT_NACK
#define DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER   DL_I2CC_INTERRUPT_RXFIFO_TRIGGER
#define DL_I2C_INTERRUPT_CONTROLLER_STOP             DL_I2CC_INTERRUPT_STOP
#define DL_I2C_INTERRUPT_CONTROLLER_TX_DONE          DL_I2CC_INTERRUPT_TX_DONE
#define DL_I2C_INTERRUPT_CONTROLLER_TIMEOUT_A        DL_I2CC_INTERRUPT_TIMEOUT_A
#define DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_TRIGGER   DL_I2CC_INTERRUPT_TXFIFO_TRIGGER

/*
 * Target interrupt mask constants
 */
#define DL_I2C_INTERRUPT_TARGET_RX_DONE       DL_I2CT_INTERRUPT_RX_DONE
#define DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY  DL_I2CT_INTERRUPT_TXFIFO_EMPTY
#define DL_I2C_INTERRUPT_TARGET_START         DL_I2CT_INTERRUPT_START
#define DL_I2C_INTERRUPT_TARGET_STOP          DL_I2CT_INTERRUPT_STOP
#define DL_I2C_INTERRUPT_TARGET_TIMEOUT_A     DL_I2CT_INTERRUPT_TIMEOUT_A

/*
 * FIFO level constants
 * UNICOMM uses fraction-based naming, MSPM0 uses byte-count naming.
 * Register values are identical across controller and target.
 */
#define DL_I2C_TX_FIFO_LEVEL_BYTES_1          DL_I2CC_TX_FIFO_LEVEL_ONE_ENTRY
#define DL_I2C_RX_FIFO_LEVEL_BYTES_1          DL_I2CC_RX_FIFO_LEVEL_ONE_ENTRY

/*
 * Enum type aliases - MSPM0 uses DL_I2C_CONTROLLER_STOP as enum type name,
 * UNICOMM uses DL_I2CC_STOP.
 */
typedef DL_I2CC_STOP DL_I2C_CONTROLLER_STOP;

/*
 * Direction / start / stop / ACK enum constants
 */
#define DL_I2C_CONTROLLER_DIRECTION_TX        DL_I2CC_DIRECTION_TX
#define DL_I2C_CONTROLLER_DIRECTION_RX        DL_I2CC_DIRECTION_RX
#define DL_I2C_CONTROLLER_START_ENABLE        DL_I2CC_START_ENABLE
#define DL_I2C_CONTROLLER_START_DISABLE       DL_I2CC_START_DISABLE
#define DL_I2C_CONTROLLER_STOP_ENABLE         DL_I2CC_STOP_ENABLE
#define DL_I2C_CONTROLLER_STOP_DISABLE        DL_I2CC_STOP_DISABLE
#define DL_I2C_CONTROLLER_ACK_ENABLE          DL_I2CC_ACK_ENABLE
#define DL_I2C_CONTROLLER_ACK_DISABLE         DL_I2CC_ACK_DISABLE

/*
 * Status / clock constants
 */
#define DL_I2C_CONTROLLER_STATUS_ERROR        DL_I2CC_STATUS_ERROR
#define DL_I2C_CLOCK_DIVIDE_1                 DL_I2CC_CLOCK_DIVIDE_1

/*
 * Target response override constants
 */
#define DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_ACK  DL_I2CT_RESPONSE_OVERRIDE_VALUE_ACK
#define DL_I2C_TARGET_RESPONSE_OVERRIDE_VALUE_NACK DL_I2CT_RESPONSE_OVERRIDE_VALUE_NACK

#else /* !CONFIG_HAS_MSP_UNICOMM - MSPM0 legacy I2C */

#include <ti/driverlib/dl_i2c.h>

typedef I2C_Regs *i2c_msp_base_t;
typedef DL_I2C_ClockConfig i2c_msp_clock_config_t;

/*
 * MSPM0 uses a single shared interrupt register for both controller and
 * target modes.  The driver needs distinct names so that on UNICOMM each
 * call reaches the correct sub-peripheral register.  On MSPM0 they all
 * resolve to the same shared function.
 */
#define DL_I2C_enableControllerInterrupt       DL_I2C_enableInterrupt
#define DL_I2C_disableControllerInterrupt      DL_I2C_disableInterrupt
#define DL_I2C_clearControllerInterruptStatus  DL_I2C_clearInterruptStatus
#define DL_I2C_getControllerPendingInterrupt   DL_I2C_getPendingInterrupt

#define DL_I2C_enableTargetInterrupt           DL_I2C_enableInterrupt
#define DL_I2C_disableTargetInterrupt          DL_I2C_disableInterrupt
#define DL_I2C_clearTargetInterruptStatus      DL_I2C_clearInterruptStatus
#define DL_I2C_getTargetPendingInterrupt       DL_I2C_getPendingInterrupt

/*
 * MSPM0 shares the timeout IIDX and interrupt mask across both modes.
 * Create controller/target-specific aliases so the driver can use
 * distinct names in each ISR path.
 */
#define DL_I2C_IIDX_CONTROLLER_TIMEOUT_A       DL_I2C_IIDX_TIMEOUT_A
#define DL_I2C_IIDX_TARGET_TIMEOUT_A           DL_I2C_IIDX_TIMEOUT_A

#define DL_I2C_INTERRUPT_CONTROLLER_TIMEOUT_A  DL_I2C_INTERRUPT_TIMEOUT_A
#define DL_I2C_INTERRUPT_TARGET_TIMEOUT_A      DL_I2C_INTERRUPT_TIMEOUT_A

#endif /* CONFIG_HAS_MSP_UNICOMM */

#endif /* ZEPHYR_DRIVERS_I2C_I2C_MSP_COMPAT_H_ */
