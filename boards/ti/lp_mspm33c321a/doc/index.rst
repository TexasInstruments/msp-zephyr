.. zephyr:board:: lp_mspm33c321a

Overview
********

MSPM33C321A microcontrollers (MCUs) are part of the MSP highly integrated, ultra-low-power 32-bit MCU
family based on the Arm® Cortex®-M33 32-bit core platform operating at up to 160-MHz frequency.
These MCUs offer high-performance analog peripheral integration, support extended temperature
ranges, and operate with supply voltages ranging from 1.62 V to 3.6 V.

The MSPM33C321A devices provide 1MB embedded flash program memory with built-in error correction
code (ECC) and 256KB SRAM with hardware parity option. These MCUs also incorporate a
memory protection unit, DMA, and a variety of peripherals including GPIO, ADC, timers, and
communication interfaces.

Hardware
********

The LP_MSPM33C321A LaunchPad is a development platform for the MSPM33C321A microcontroller.
Zephyr uses the ``lp_mspm33c321a`` board configuration for building applications for this platform.

Board Features
==============

* MSPM33C321A microcontroller with Arm® Cortex®-M33 core running at up to 160 MHz
* 1MB Flash memory with ECC
* 256KB SRAM
* On-board XDS110 debugger
* User LEDs and buttons
* Multiple expansion headers for BoosterPack ecosystem compatibility


Development Environment
=======================

The following development environment was used while developing and testing:

* TI Code Composer Studio (CCS) version 20.3.0 (`Download Link <https://www.ti.com/tool/download/CCSTUDIO/20.3.0>`_)
* Zephyr SDK version 0.17.4
* Zephyr version v4.2.0

Building and Flashing
*********************

Building
========

Follow the :ref:`getting_started` instructions for Zephyr application development.

For example, to build the basic/blinky application for the MSPM33C321A LaunchPad:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: lp_mspm33c321a
   :goals: build

The resulting ``zephyr.elf`` binary in the build directory can be loaded onto
MSPM33C321A LaunchPad using the steps mentioned below.

Flashing
========

Currently, the MSPM33C321A board does not support the west flashing tool or OpenOCD support.
Instead, we use SRAM-based loading where we build the samples and load the zephyr.elf file
from the build directory to the TI Code Composer Studio (CCS) IDE in debugger mode.

To flash the board:

1. Build your Zephyr application as described above
2. Open TI Code Composer Studio IDE
3. Create a new project or import an existing CCS project for the MSPM33C321A
4. Start project less debug using MSPM33 target configuration(CCXML) file
5. Connect to the MSPM33 core
6. In the debugger, select "Run" > "Load" > "Load Program..."
7. Browse to your Zephyr build directory and select the ``zephyr.elf`` file
8. The program will be loaded into the SRAM of the device
9. Click "Resume" to start the program execution

Future updates to the Zephyr support for this board will include flash-based programming
and support for the west flash command.

Serial Console
==============

The MSPM33C321A LaunchPad includes an on-board XDS110 debugger that also provides a
virtual COM port over USB. This can be used for serial console output.

To connect to the serial console, use a terminal emulator such as PuTTY, minicom, or screen
with the following settings:

* Baud rate: 115200
* Data bits: 8
* Parity: None
* Stop bits: 1
* Flow control: None

Debugging
=========

You can debug an application using TI Code Composer Studio IDE as described in the flashing
section above. The debugger provides full visibility into the device state, registers,
memory, and supports common debugging features like breakpoints, watchpoints, and step-by-step
execution.

References
**********
