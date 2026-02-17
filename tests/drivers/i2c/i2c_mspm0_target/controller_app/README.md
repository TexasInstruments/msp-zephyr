# I2C Target Test - Controller Application

This is the controller-side application for testing I2C target functionality. It must be used in conjunction with the target test application.

## Purpose

This application:
- Acts as I2C controller (master)
- Waits for target board to signal READY via GPIO
- Initiates I2C transactions (write, read, write-read)
- Signals target when transaction completes via GPIO
- Runs through a predefined test sequence

## Building

```bash
cd $ZEPHYR_BASE
west build -p -b lp_mspm0g3507 tests/drivers/i2c/i2c_mspm0_target/controller_app -d build-controller
west flash -d build-controller
```

## Configuration

- **Target I2C Address**: 0x54 (configurable in `src/controller.c`)
- **READY GPIO**: PA18 (input, receives signal from target)
- **TRIGGER GPIO**: PA27 (output, sends signal to target)
- **I2C Bus**: I2C0 at 100kHz (standard mode)

## Usage

1. Build and flash the target test application first
2. Build and flash this controller application
3. Wire the two boards (I2C + GPIO + GND)
4. Reset controller board first, then target board
5. Watch serial output on both boards

For complete setup instructions, see the main README in the parent directory.

## Test Sequence

The controller runs these tests in order:
1. Single byte write (0xAA)
2. Multi-byte write (8 bytes: 0x00-0x07)
3. Single byte read (expects 0x42)
4. Multi-byte read (8 bytes: expects 0x10-0x17)
5. Repeated start write-read (write 2, read 4)
6. Large write (32 bytes: 0x00-0x1F)
7. Large read (32 bytes: expects 0xA0-0xBF)

## Timeout Configuration

- `READY_TIMEOUT_MS`: 30 seconds (time to wait for target to be ready)
- `INTER_TEST_DELAY_MS`: 500 ms (delay between tests)

Increase these if needed for slower setups or debugging.
