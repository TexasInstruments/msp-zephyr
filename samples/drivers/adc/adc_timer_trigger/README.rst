.. _adc_timer_trigger:

ADC Timer-Triggered Sampling (MSPM33)
######################################

Overview
********

This sample demonstrates hardware-triggered ADC conversions on the MSPM33
platform using the event fabric architecture. A timer is configured to
automatically trigger ADC conversions without CPU intervention, showcasing:

* **Zero CPU overhead** - Hardware-to-hardware triggering via event fabric
* **Precise timing** - No software jitter, hardware-level synchronization
* **Power efficiency** - CPU can sleep while conversions occur
* **Event fabric** - Publisher-subscriber model connecting peripherals

Architecture
************

The MSPM33 event fabric connects peripherals through event channels:

.. code-block:: none

   ┌──────────┐          Event Fabric          ┌──────────┐
   │  Timer   │────► [Event Channel 1] ────►   │  HSADC   │
   │(Publisher)│                                │(Subscriber)│
   └──────────┘                                └──────────┘
       │                                             │
       └──► ZERO_EVENT (1 kHz)                      └──► GEN_SUB_0
                                                          triggers
                                                          conversion

Event Flow:
1. Timer counts down and reaches zero (1 ms period = 1 kHz)
2. Timer publishes ZERO_EVENT on event channel 1
3. HSADC subscribed to channel 1 receives event
4. HSADC automatically starts conversion (no CPU involved)
5. Conversion complete interrupt wakes application

Building and Running
********************

This sample is specifically for the MSPM33C321A LaunchPad (LP_MSPM33C321A).

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/adc/adc_timer_trigger
   :board: lp_mspm33c321a
   :goals: build
   :compact:

Hardware Setup
**************

Connect an analog voltage source to ADC channel 0:

* **ADC Channel 0**: PA27 (pin 30 on LaunchPad)
* **Reference**: VDDA (3.3V) - no external reference needed
* **Input range**: 0V to 3.3V

For testing:
* Connect a potentiometer between GND and 3.3V, wiper to PA27
* Or connect a known voltage source (0-3.3V) to PA27
* **Important**: Never exceed 3.3V on ADC inputs!

Device Tree Configuration
*************************

The sample requires board-specific device tree configuration in
``boards/lp_mspm33c321a.overlay``:

Timer Configuration (Event Publisher):

.. code-block:: devicetree

   &timer0 {
       status = "okay";
       ti,timer-mode = <0>;         /* Periodic mode */
       ti,timer-period = <80000>;   /* 80000 cycles at 80 MHz = 1 ms */
       ti,event-publisher-channel = <1>;  /* Publish on event channel 1 */
   };

ADC Configuration (Event Subscriber):

.. code-block:: devicetree

   &adc0 {
       status = "okay";
       ti,hw-trigger-enable;              /* Enable hardware triggering */
       ti,hw-trigger-source = <0>;        /* Use GEN_SUB_0 */
       ti,hw-trigger-event-channel = <1>; /* Subscribe to event channel 1 */

       /* ADC clock and reference configuration */
       ti,clk-divider = <2>;
       ti,vref-source = <0>;  /* VDDA (3.3V) */
       vref-mv = <3300>;

       /* Pin configuration */
       pinctrl-0 = <&analog_pa27>;
       pinctrl-names = "default";

       /* Channel configuration */
       #address-cells = <1>;
       #size-cells = <0>;

       channel@0 {
           reg = <0>;
           zephyr,gain = "ADC_GAIN_1";
           zephyr,reference = "ADC_REF_VDD_1";
           zephyr,acquisition-time = <ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 200)>;
           zephyr,resolution = <12>;
           zephyr,vref-mv = <3300>;
       };
   };

Key Device Tree Properties:

* ``ti,hw-trigger-enable``: Enables hardware triggering (boolean)
* ``ti,hw-trigger-source``: Event subscriber index (0-3 for GEN_SUB_0 to GEN_SUB_3)
* ``ti,hw-trigger-event-channel``: Event fabric channel number (1-15)

**Important**: The timer's publisher channel and ADC's subscriber channel must match!

Expected Output
***************

The console output shows:

.. code-block:: console

   === MSPM33 Timer-Triggered ADC Sample ===
   This demonstrates hardware-triggered ADC conversions
   using the MSPM33 event fabric.

   ADC device ready: adc@40004000
   Timer device ready: timer@40860000
   ADC channel 0 configured

   Configuration:
     - Timer: timer@40860000
     - Timer frequency: 1 kHz (1 ms period)
     - Event channel: 1
     - ADC channel: 0 (PA27)
     - ADC resolution: 12 bits
     - Reference voltage: VDDA (3.3V)
     - Hardware trigger: Enabled (GEN_SUB_0)

   Hardware trigger path:
     Timer ZERO_EVENT -> Event Channel 1 -> ADC GEN_SUB_0 -> Conversion

   Starting periodic ADC sampling (timer-triggered)...
   Timer triggers ADC every 1 ms. Reading samples every 1 second.

   [     1] ADC CH0: Raw=2048 (0x800), Voltage=1650 mV
   [     2] ADC CH0: Raw=2050 (0x802), Voltage=1651 mV
   [     3] ADC CH0: Raw=2047 (0x7FF), Voltage=1649 mV
   ...

Notes:
* Raw ADC values range from 0 to 4095 (12-bit resolution)
* 0 corresponds to 0V, 4095 corresponds to 3.3V
* Voltage calculation: ``mV = (raw * 3300) / 4095``

How It Works
************

Software Flow:
1. Application configures ADC channel (one-time setup)
2. Application calls ``adc_read()`` which prepares for conversion
3. Application waits for completion (sleeps)

Hardware Flow (happens automatically):
1. Timer counts down and reaches zero
2. Timer hardware publishes event on channel 1
3. Event fabric routes event to HSADC GEN_SUB_0
4. HSADC hardware starts conversion
5. Conversion complete → interrupt fires
6. ISR stores result, wakes application
7. Application reads result from buffer

The key insight: **Steps 1-5 happen in hardware without CPU**!

Benefits of Hardware Triggering
********************************

Compared to software triggering (``adc_read()`` in a loop):

+---------------------------+---------------------+----------------------+
| Aspect                    | Software Trigger    | Hardware Trigger     |
+===========================+=====================+======================+
| **CPU Overhead**          | High (ISR every 1ms)| Low (interrupt only) |
+---------------------------+---------------------+----------------------+
| **Timing Precision**      | ±1-10 µs jitter     | <100 ns jitter       |
+---------------------------+---------------------+----------------------+
| **Power Consumption**     | CPU always active   | CPU sleeps           |
+---------------------------+---------------------+----------------------+
| **Interrupt Rate**        | Every sample        | Only when needed     |
+---------------------------+---------------------+----------------------+
| **Maximum Sample Rate**   | Limited by software | Hardware limited     |
+---------------------------+---------------------+----------------------+

Use Cases
*********

Hardware-triggered ADC is ideal for:

* **High-speed sampling**: Data acquisition at precise intervals
* **Low-power applications**: Minimize CPU wake-ups
* **Synchronization**: Sample synchronized with PWM, timers, or external events
* **Control loops**: ADC sampling synchronized with PWM period
* **Signal processing**: Consistent sampling rates for DSP algorithms

Example applications:
* Motor control (sample current/voltage synchronized with PWM)
* Power monitoring (periodic voltage/current measurements)
* Audio sampling (precise timing for DSP)
* Sensor data acquisition (periodic environmental monitoring)

References
**********

* MSPM33 Technical Reference Manual - Event Manager (EVTMGR) chapter
* HSADC Hardware Triggering Analysis Document
* Vendor ADC Hardware Trigger Analysis Document
* ``drivers/adc/adc_msp_hsadc.c`` - ADC driver implementation
* ``dts/bindings/adc/ti,msp-hsadc.yaml`` - Device tree binding documentation
