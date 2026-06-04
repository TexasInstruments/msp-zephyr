.. zephyr:code-sample:: full_pm
   :name: Full Power Management with I2C and RTC
   :relevant-api: gpio_interface pm_policy_api i2c_interface rtc_interface

   Comprehensive power management example with I2C communication, RTC timekeeping, and low-power mode demonstration.

Overview
********

This example demonstrates system-level idle power management with multiple peripheral features:

- **System Clock Output**: ULPCLK output on PA22 for real-time frequency observation
- **I2C Communication**: Periodic I2C transactions to a target slave device
- **RTC Timekeeping**: Real-time clock with automatic date/time management
- **Power State Debugging**: GPIO-based debug signals to observe active/idle states

The system enters low-power mode during 2-second sleep periods, where the clock frequency drops significantly. The debug GPIO pins allow observation of:

- **PB20**: Power state indicator
- **PB22**: Main loop activity
- **PB18**: Idle state indicator

PA22 Clock Output
*****************

- ULPCLK is output on PA22 with a 16x divider
- For example, 32MHz system clock ÷ 16 = 2MHz
- **Active mode**: ~2MHz (32MHz ÷ 16)
- **Low-power mode**: ~2kHz (32kHz ÷ 16)

Use a logic analyzer to observe the frequency change during the 2-second sleep period.

I2C Operation
*************

The system performs periodic I2C write transactions during the active phase:

- Target slave address: 0x32
- Writes 10 bytes of data with an incrementing counter
- Device must be ready before operation begins

RTC Operation
*************

The real-time clock is initialized to June 4, 2026 at 00:00:00 and provides:

- Date and time tracking across sleep cycles
- Timestamp output to console each cycle
- Maintains time accuracy during low-power states

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: ti_samples/full_pm
   :board: lp_mspm0g3519
   :goals: build flash
   :compact:

After flashing:

1. Open a serial terminal to view UART0 output (460800 baud)
2. Observe console output showing system uptime and RTC time
3. Connect a logic analyzer to:
   - PA22 to observe clock frequency changes during sleep
   - PB22 to see main loop activity
   - PB18 to monitor idle state transitions

Prerequisites
*************

- I2C slave device responding at address 0x32
- Logic analyzer (optional, for clock and debug signal observation)
- Serial terminal for console output (460800 baud)

Expected Output
***************

- **Console**: System uptime and RTC date/time printed each cycle
- **PA22**: Frequency drops from ~2MHz to 0kHz during 2-second low power mode (when using STANDBY1)
- **PB22**: Pulses indicating main loop execution
- **PB18**: High during sleep states, low during active states
