.. _i2c_mspm0_target_test:

I2C MSPM0 Target Test
#####################

Overview
********

This is a Ztest-based hardware test for the TI MSPM0 I2C controller driver.
It validates proper I2C controller operation by communicating with an I2C target
device at address 0x32 that implements a specific test fixture protocol.

The test suite includes four test cases:

1. **Write Mode Test** - Verifies controller can send data to target
2. **Read Mode Test** - Verifies controller can receive incrementing data pattern
3. **Repeated Start Mode Test** - Tests write-then-read with repeated start condition
4. **Clock Stretching Test** - Verifies proper handling of SCL clock stretching by target

Hardware Requirements
*********************

This test requires the following hardware setup:

- **Controller Board**: TI LaunchPad (lp_mspm0g3507 or lp_mspm0l1117) running this test
- **Target Board**: Separate board running I2C target firmware at address 0x32
- **Physical Connections**:
  - SDA to SDA
  - SCL to SCL
  - GND to GND

Target Device Protocol
**********************

The I2C target device must be configured at 7-bit address **0x32** and implement
the following command protocol:

Command Bytes (Single-byte writes):
  - ``0x01`` - Write Mode: Accept and discard write data
  - ``0x02`` - Read Mode: Return bytes 0x00-0x07 incrementing
  - ``0x03`` - Repeated Start Mode: Add 1 to received byte, return incremented values
  - ``0x04`` - Clock Stretch Mode: Stretch SCL during data reception

See the ``samples/i2c_target_tester/TESTING.md`` file for detailed protocol specifications.

Building and Running
********************

To build the test:

.. code-block:: console

   west build -b lp_mspm0g3507 tests/drivers/i2c/i2c_mspm0

To flash and run:

.. code-block:: console

   west flash
   west attach

Running with Twister
********************

To run this test using Twister with connected hardware:

.. code-block:: console

   # Flash target device firmware first on second board

   # Run test on controller board
   west twister -p lp_mspm0g3507 -T tests/drivers/i2c/i2c_mspm0 \\
     --device-testing --device-serial /dev/ttyACM0

Test Output
***********

Expected output when all tests pass:

.. code-block:: console

   *** Booting Zephyr OS build v4.3.0 ***
   Running TESTSUITE i2c_mspm0_target
   ===================================================================
   START - test_write_mode

   === TEST 1: WRITE MODE ===
   Setting write mode (command 0x01)...
   Write mode command sent successfully
   Writing 1 byte of test data...
   1 byte write successful
   ...
   TEST 1 PASSED: All write operations completed
    PASS - test_write_mode in 0.532 seconds
   ===================================================================
   START - test_read_mode
   ...
    PASS - test_read_mode in 0.423 seconds
   ===================================================================
   START - test_repeated_start_mode
   ...
    PASS - test_repeated_start_mode in 0.687 seconds
   ===================================================================
   START - test_clock_stretch_mode
   ...
    PASS - test_clock_stretch_mode in 0.845 seconds
   ===================================================================
   TESTSUITE i2c_mspm0_target succeeded

   ------ TESTSUITE SUMMARY START ------
   SUITE PASS - 100.00% [i2c_mspm0_target]: pass = 4, fail = 0, skip = 0, total = 4 duration = 2.487 seconds
   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   RunID: xxx
   PROJECT EXECUTION SUCCESSFUL

Troubleshooting
***************

Test Failures
=============

**"I2C device not ready"**
  Check that the I2C controller is enabled in devicetree and properly configured

**"Failed to send write mode command"**
  - Verify target device is powered and running
  - Check physical I2C connections (SDA, SCL, GND)
  - Verify target is at address 0x32
  - Check for proper pull-up resistors on I2C lines

**"Byte mismatch" in read tests**
  - Target may not be implementing protocol correctly
  - Verify target firmware matches expected behavior

**"Read operation timed out"**
  - Target may be holding SCL low indefinitely
  - Check target's clock stretching implementation
  - Verify CONFIG_I2C_SCL_LOW_TIMEOUT is appropriate

Logic Analyzer Debug
====================

For detailed I2C transaction analysis:

1. Connect logic analyzer to SDA and SCL lines
2. Configure for I2C protocol decode
3. Trigger on address 0x32
4. Verify:
   - Proper START/STOP conditions
   - ACK/NACK responses
   - Data byte values match expected patterns
   - Clock stretching behavior in Test 4

Configuration Options
*********************

The following Kconfig options affect test behavior:

- ``CONFIG_I2C_SCL_LOW_TIMEOUT`` - Timeout for cumulative SCL low period (default: 100ms)
- ``CONFIG_DYNAMIC_THREAD`` - Required for application-level timeout in Test 4
- ``CONFIG_ZTEST_ASSERT_VERBOSE`` - Verbose assertion output (default: 1)

Related Documentation
*********************

- :ref:`i2c_target_tester` - Interactive sample version of this test
- :ref:`i2c_api` - Zephyr I2C API documentation
- TI MSPM0 I2C Controller Technical Reference Manual
