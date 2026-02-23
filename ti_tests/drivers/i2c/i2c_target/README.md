# I2C Loopback Test — Target Side

Runs alongside the `i2c_controller` test image on a second board.  The target
registers itself as an I2C device at address `0x54` and responds to each test
transaction driven by the controller.

See `i2c_controller/README.md` for wiring and build instructions.

## Synchronization

Each test uses a two-phase handshake:

1. Target pulses **READY** (PA13) high to tell the controller it is ready for
   the next transaction.
2. The controller performs the I2C transaction.
3. The target's `stop` callback fires and releases an internal semaphore,
   waking the test thread.
4. The target validates received data, loads a **status byte** into its read
   buffer (`0x00` = pass, `0xFF` = fail), then pulses READY again.
5. The controller reads the echo payload and status byte.
6. Both sides run their `zassert` checks.

The status byte is written before any `zassert` runs, so the controller can
always retrieve it regardless of whether the target passes or fails.

The controller resets this board via nRST (PA8 on the controller, connected to
the hardware nRST pin here) once at suite start.  No nRST handling is needed
in this firmware.

## Tests

| Test | Description |
|------|-------------|
| `test_01_single_byte_echo`    | Receives 1 byte; echoes it back with a status byte |
| `test_02_burst_echo_8`        | Receives 8 bytes; echoes them back with a status byte |
| `test_03_burst_echo_32`       | Receives 32 bytes; echoes them back with a status byte |
| `test_04_repeated_start`      | Receives a `write_read`; serves fixed read data, then a status byte |
| `test_05_merged_transactions` | Two merged transaction sequences (see below) |

### test_05 sequences

**Sequence A** — `WRITE + RS + WRITE + RS + READ`: receives two write segments
(accumulated contiguously in the write buffer) and serves a fixed read
response.  Both write segments are validated after the STOP.

**Sequence B** — `READ + RS + READ + RS + WRITE`: serves read data from the
start of the read buffer for each read segment (the `read_requested` callback
resets the read index on every new read address), then receives a write
segment.  The write bytes are validated after the STOP.
