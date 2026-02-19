# I2C Target API Test for MSPM0

This test validates the I2C target (slave) functionality on MSPM0 devices. It requires two boards: one acting as the target (device under test) and another as the controller (test driver).

## Overview

The test validates the following I2C target capabilities:
- Single byte write/read operations
- Multi-byte write/read operations (8 and 32 bytes)
- Repeated start (write-then-read) transactions
- Proper callback invocation (write_requested, write_received, read_requested, read_processed, stop)
- Data integrity verification

## Hardware Requirements

### Two MSPM0 Boards Required

1. **Target Board (DUT)**: Runs the test suite with assertions
2. **Controller Board**: Drives I2C transactions based on GPIO synchronization

### Wiring Configuration

Connect the two boards as follows:

```
Target Board          Controller Board
------------          ----------------
SDA (I2C0)    <----->  SDA (I2C0)
SCL (I2C0)    <----->  SCL (I2C0)
GND           <----->  GND

PA18 (READY)  ------->  PA18 (READY)     [Target signals ready]
PA27 (TRIGGER) <------  PA27 (TRIGGER)   [Controller signals complete]
```

**Important Notes:**
- Add pull-up resistors (4.7kΩ) on SDA and SCL lines if not already present on your boards
- Ensure both boards share a common ground
- GPIO pins PA18 and PA27 are used for handshaking (configurable in overlay files)

## Test Protocol

The two applications synchronize using GPIO signals:

1. **Target** signals READY (PA18 pulse high) when prepared for a test
2. **Controller** waits for READY, then initiates the I2C transaction
3. **Controller** signals TRIGGER (PA27 pulse high) when transaction completes
4. **Target** waits for TRIGGER, then validates the transaction and runs assertions
5. Repeat for next test

## Building and Running

### Step 1: Build and Flash the Target Test

```bash
cd $ZEPHYR_BASE
west build -p -b lp_mspm0g3507 tests/drivers/i2c/i2c_mspm0_target
west flash
```

Connect to the target board's serial console:
```bash
minicom -D /dev/ttyACM0 -b 115200
```

### Step 2: Build and Flash the Controller App

On a separate terminal (or after flashing the target):

```bash
cd $ZEPHYR_BASE
west build -p -b lp_mspm0g3507 tests/drivers/i2c/i2c_mspm0_target/controller_app -d build-controller
west flash -d build-controller
```

Connect to the controller board's serial console:
```bash
minicom -D /dev/ttyACM1 -b 115200
```

### Step 3: Reset Both Boards

Reset the **controller board first**, then the **target board**. The controller will wait for the target to be ready.

## Expected Output

### Target Board Console

```
*** Booting Zephyr OS build v4.0.0 ***

=== I2C Target Test Setup ===
I2C device ready
READY GPIO configured
TRIGGER GPIO configured with interrupt
Registered as I2C target at address 0x54
=== Setup Complete ===

Running TESTSUITE i2c_mspm0_target_test
===================================================================

=== TEST 1: Single Byte Write ===
Signaled READY to controller
Received TRIGGER from controller
Write requested
Received byte[0]: 0xAA
Stop condition received
Write complete: 1 bytes
TEST 1 PASSED
PASS - test_01_single_byte_write in 0.501 seconds
...
[All tests run]
===================================================================
TESTSUITE i2c_mspm0_target_test succeeded

=== Test Statistics ===
Total bytes written: 43
Total bytes read: 45
Total stop conditions: 7
I2C target unregistered
```

### Controller Board Console

```
========================================
  I2C Target Test - Controller Board
========================================

I2C controller ready
READY GPIO configured
TRIGGER GPIO configured

Starting test sequence...
Target address: 0x54

=== TEST 1: Single Byte Write ===
Waiting for target READY signal...
Target is READY
Wrote 1 byte: 0xAA
Sent TRIGGER to target

=== TEST 2: Multi-Byte Write ===
...

========================================
  ALL TESTS COMPLETED SUCCESSFULLY!
========================================
```

## Test Details

| Test # | Description | Controller Action | Target Validation |
|--------|-------------|-------------------|-------------------|
| 1 | Single byte write | Write 0xAA | Verify 1 byte received = 0xAA |
| 2 | Multi-byte write | Write 8 bytes (0x00-0x07) | Verify 8 bytes, check pattern |
| 3 | Single byte read | Read 1 byte | Target provides 0x42 |
| 4 | Multi-byte read | Read 8 bytes | Target provides 0x10-0x17 |
| 5 | Repeated start | Write 2, read 4 bytes | Verify both operations |
| 6 | Large write | Write 32 bytes (0x00-0x1F) | Verify 32 bytes, check pattern |
| 7 | Large read | Read 32 bytes | Target provides 0xA0-0xBF |

## Customization

### Changing GPIO Pins

Edit the board overlay files:
- Target: `boards/lp_mspm0g3507.overlay`
- Controller: `controller_app/boards/lp_mspm0g3507.overlay`

Modify the GPIO pin assignments in the `gpio_ready` and `gpio_trigger` nodes.

### Changing I2C Target Address

Modify `TARGET_ADDR` in both:
- `src/test_target.c`
- `controller_app/src/controller.c`

### Adding More Tests

1. Add a new test function in `src/test_target.c` using `ZTEST()` macro
2. Add corresponding test function in `controller_app/src/controller.c`
3. Call the new test in the controller's `main()` function
4. Follow the same GPIO synchronization pattern

## Troubleshooting

### Target Never Receives READY Signal
- Check GPIO wiring (PA18)
- Verify GPIO interrupt is configured correctly
- Use logic analyzer to confirm signal levels

### I2C Transactions Fail
- Check SDA/SCL wiring
- Verify pull-up resistors are present (4.7kΩ typical)
- Ensure both boards share common ground
- Use logic analyzer to inspect I2C bus signals

### Timeout Waiting for Target
- Ensure target board is running and not stuck
- Increase `READY_TIMEOUT_MS` in controller application
- Check serial console of target for error messages

### Tests Pass on Controller but Fail on Target
- Timing issue: increase `INTER_TEST_DELAY_MS` in controller
- Data mismatch: check with logic analyzer
- May indicate driver bug in target mode

## Driver Bug Fixes

### MSPM0 I2C Target Callback Order Bug (FIXED)

**Bug Description:**
The MSPM0 I2C target driver had a critical bug where `read_requested_cb` was called multiple times or `read_processed_cb` was called before `read_requested_cb` during repeated start operations (write-then-read). This occurred due to two issues:

1. The driver only checked for `I2C_MSPM0_TARGET_STARTED` state, missing repeated starts where state is `I2C_MSPM0_TARGET_RX_INPROGRESS`
2. Interrupts could fire in unexpected order, causing `read_requested` to be called multiple times

**Root Cause:**
During a repeated start, the hardware may generate both TXFIFO_EMPTY and START interrupts. Without proper tracking, this could cause:
- `read_requested_cb()` to be called twice (duplicating the first byte)
- Or `read_processed_cb()` to be called before `read_requested_cb()` (wrong callback order)

**The Fix:**
Two changes were made to `i2c_mspm0.c`:

1. **Added state check for repeated start** (line 607):
```c
if ((data->state == I2C_MSPM0_TARGET_STARTED ||
     data->state == I2C_MSPM0_TARGET_RX_INPROGRESS) &&
    !data->target_read_started) {
```

2. **Added transaction tracking flag** (line 73):
```c
bool target_read_started; /* Tracks if read_requested was called for current transaction */
```

The flag is:
- Cleared on START interrupt (new transaction)
- Checked and set when calling `read_requested_cb()`
- Cleared on STOP interrupt (transaction complete)

This ensures `read_requested_cb()` is called exactly once per read transaction, regardless of interrupt timing.

**Files Modified:**
- `/home/a0232293/zephyrproject/zephyr/drivers/i2c/i2c_mspm0.c` (lines 73, 565, 607-615, 669, 495)

## Architecture Notes

This implementation uses:
- **Zephyr ztest framework** for target-side testing
- **GPIO-based synchronization** for deterministic test sequencing
- **I2C target callbacks** (write_requested, write_received, read_requested, read_processed, stop)
- **Separate applications** to avoid interference between controller and target roles

The separation allows the target test to use full assertion capabilities while the controller focuses solely on driving transactions according to the test protocol.
