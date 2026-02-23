# I2C Loopback Test — Controller Side

Tests the MSPM0 I2C driver in controller mode against a second board running
the `i2c_target` test image.  Both boards must be flashed and running before
the test suite starts; the controller resets the target via nRST at the
beginning of `suite_setup` to synchronize the run.

## Hardware

Two LP-MSPM0G3507 boards are required.  Make the following connections:

| Signal | Controller pin | Target pin | Notes |
|--------|---------------|------------|-------|
| SDA    | PA0           | PA0        | I2C data |
| SCL    | PA1           | PA1        | I2C clock |
| READY  | PA13 (input)  | PA13 (output) | Target pulses high to signal ready |
| nRST   | PA8 (output)  | nRST       | Controller drives low to reset target |
| GND    | GND           | GND        | Common ground |

`PA8` on the controller is configured active-low; asserting it drives the pin
low, which holds the target in reset.

## Tests

| Test | Description | Requires target |
|------|-------------|----------------|
| `test_01_single_byte_echo`      | Write 1 byte; read back echo + status byte        | Yes |
| `test_02_burst_echo_8`          | Write 8 bytes; read back echo + status byte       | Yes |
| `test_03_burst_echo_32`         | Write 32 bytes; read back echo + status byte      | Yes |
| `test_04_repeated_start`        | `i2c_write_read` with fixed payload; read status  | Yes |
| `test_05_merged_transactions`   | Two merged `i2c_transfer` sequences (see below)   | Yes |
| `test_06_nak_invalid_write`     | Write to unused address; expects `-EIO`           | No  |
| `test_07_nak_invalid_read`      | Read from unused address; expects `-EIO`          | No  |

### test_05 sequences

**Sequence A** — `WRITE + RS + WRITE + RS + READ`: two write segments followed
by a read, all in one `i2c_transfer` call.

**Sequence B** — `READ + RS + READ + RS + WRITE`: two read segments followed by
a write.  Because the target's `read_requested` callback resets its read index
on each new read address, both read segments return data from the start of the
target's read buffer; they are distinguished by length (2 bytes vs. 3 bytes).

## Build and flash

```sh
# Controller
west build -b lp_mspm0g3507 ti_tests/drivers/i2c/i2c_controller
west flash

# Target (second board)
west build -b lp_mspm0g3507 ti_tests/drivers/i2c/i2c_target
west flash
```

The controller waits up to 30 s for the target's first READY pulse, so boot
order does not matter as long as both boards are running before the timeout.
