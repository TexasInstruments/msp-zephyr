.. _i2c_controller_test:

I2C Controller Test
###################

Overview
********

This test suite validates I2C controller functionality for TI MSPM0 devices.
It exercises various I2C operations including single and burst read/write
operations, repeated-start transactions, and error handling.

Test Coverage
*************

The test suite includes the following test cases:

1. **test_i2c_write_invalid_address**

   Verifies that writing to an invalid I2C address returns the expected error (-EIO).

2. **test_i2c_single_write**

   Tests writing a single byte to the I2C target device.

3. **test_i2c_single_read**

   Tests reading a single byte from the I2C target device.

4. **test_i2c_burst_read**

   Tests reading multiple bytes (5 bytes) from the I2C target device.

5. **test_i2c_burst_write**

   Tests writing multiple bytes (5 bytes) to the I2C target device.

6. **test_i2c_write_read**

   Tests a combined write/read operation with repeated-start condition.

7. **test_i2c_get_config**

   Verifies that the I2C configuration can be read back correctly.

Hardware Setup
**************

The test requires:

* An I2C target device connected at address 0x32
* I2C0 pins configured as follows:

  - SCL: PA1
  - SDA: PA0

Supported Boards
****************

* lp_mspm0g3507
* lp_mspm0g3519
* lp_mspm0l1117
* lp_mspm0l2228

Building and Running
********************

To build and run the test for a specific board:

.. code-block:: bash

   west build -p auto -b lp_mspm0g3507 ti_tests/drivers/i2c/i2c_controller
   west flash

To run with Twister:

.. code-block:: bash

   ./scripts/twister -T ti_tests/drivers/i2c/i2c_controller -p lp_mspm0g3507
