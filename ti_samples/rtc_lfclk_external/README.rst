# RTC with External Low-Frequency Clock

## Overview

This sample demonstrates how to configure and use the Real-Time Clock (RTC) with an external low-frequency clock source on TI microcontrollers. The application sets up the RTC with a calendar time and continuously reads the current time.

The sample uses an external 32.768 kHz crystal connected to the device to provide the low-frequency clock source for the RTC peripheral, which allows for accurate timekeeping even in low-power modes.

## Requirements

* A board compatible with TI MSPM0 microcontrollers
* An external 32.768 kHz crystal connected to the appropriate pins
* A serial terminal (for viewing output)

## Building and Running

The application can be built and executed for MSPM0 boards as follows:

```bash
west build -b lp_mspm0g3507 -d ti_samples/rtc_lfclk_external -d rtc_lfclk_external_out
west flash -d rtc_lfclk_external_out
```

## Features Demonstrated

* Configuring the RTC peripheral with an external low-frequency clock source
* Setting the calendar time in the RTC
* Reading the current time from the RTC

## Implementation Details

The sample initializes the RTC with a specific date and time:
- Hour: 2
- Minute: 0
- Second: 0
- Weekday: Wednesday (3)
- Day: 23
- Month: October (9)
- Year: 2025

It then enters a loop where it reads the current time from the RTC every second.

The external crystal provides a stable and accurate clock source for the RTC, which is especially important for applications that require precise timekeeping or that operate in low-power modes where the main system clock may be disabled.

## References

* [TI MSPM0 Technical Reference Manual](https://www.ti.com/lit/pdf/sprui74)
* [Zephyr RTC API Documentation](https://docs.zephyrproject.org/latest/hardware/peripherals/rtc.html)