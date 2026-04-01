.. zephyr:code-sample:: blinky_idle_pm
   :name: Blinky with System-Level Idle Power Management
   :relevant-api: gpio_interface pm_policy_api

   Blink an LED with automatic system-wide idle power management.

Overview
********

The Blinky Idle PM sample demonstrates system-level power management controlled automatically
by the Zephyr PM policy manager. Unlike manual device-level PM, the entire SoC automatically
enters deep sleep states during idle periods, with no explicit PM API calls in application code.

The source code shows how to:

#. Get a pin specification from the :ref:`devicetree <dt-guide>` as a
   :c:struct:`gpio_dt_spec`
#. Configure the GPIO pin as an output
#. Toggle the pin in a simple loop
#. Let the PM system automatically manage power during idle periods (via :c:func:`k_msleep()`)

Key Concepts
*************

**Automatic Idle PM:**
- The PM policy manager automatically monitors the scheduler for idle periods
- When no threads are ready to run, the idle thread is scheduled
- During idle (e.g., waiting in :c:func:`k_msleep()`), the policy manager:
  1. Calculates time until next scheduled event
  2. Selects the best sleep state based on residency times
  3. Powers down the CPU and SoC peripherals
  4. Enters the selected sleep state

**Policy Selection:**
- :c:macro:`CONFIG_PM_POLICY_DEFAULT` enables the default policy
- Policy analyzes available sleep states and picks the most efficient one
- :c:macro:`CONFIG_PM_STATE_MEMORY_RESIDENCY_THRESHOLD` controls which states are considered

**Automatic Device Suspend:**
- Devices are auto-suspended via :c:macro:`CONFIG_PM_DEVICE_RUNTIME_DEFAULT_ENABLE`
- When the system enters a sleep state, suspended devices are also powered down
- Upon wakeup, devices are automatically resumed

**Wakeup Events:**
- Timer interrupt fires after the scheduled sleep duration
- Interrupt resumes the system from sleep state
- Application continues normally

Power Savings
**************

The application code is identical to basic Blinky, but power consumption is dramatically lower
during the 1000ms sleep period because:

- Entire SoC enters low-power state (not just individual devices)
- CPU clock can be gated or stopped
- Multiple peripherals power down together
- System is in sleep state for the full 1000ms between LED toggles

Requirements
************

Your board must:

#. Have an LED connected via a GPIO pin (these are called "User LEDs" on many of
   Zephyr's :ref:`boards`).
#. Have the LED configured using the ``led0`` devicetree alias.
#. Support the :ref:`PM subsystem <pm_api>` with at least one CPU sleep state.

Building and Running
********************

Build and flash Blinky Idle PM as follows, changing ``lp_mspm0g3507`` for your board:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky_idle_pm
   :board: lp_mspm0g3507
   :goals: build flash
   :compact:

After flashing, the LED starts to blink and messages with the current LED state
are printed on the console. The system automatically enters low-power sleep states
during idle periods, with NO explicit PM API calls in the application code.

Comparing with Other Samples
*****************************

- **Blinky**: Basic LED blinking with no power management
- **Blinky Device PM**: Manual device-level PM control via API calls
- **Blinky Idle PM** (this sample): Automatic system-wide PM via policy manager (completely transparent to application)

Key Difference
***************

The application code for this sample is **identical** to basic Blinky. All power management
is handled transparently by the kernel's idle thread and PM policy manager. This demonstrates
that on systems supporting idle PM, you can achieve significant power savings simply by
enabling the appropriate kernel configuration options.
