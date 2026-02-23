# Wiring Diagram for I2C Target Test

## Connection Table

| Signal | Target Board | Controller Board | Notes |
|--------|--------------|------------------|-------|
| I2C SDA | I2C0_SDA | I2C0_SDA | Requires 4.7kΩ pull-up to VCC |
| I2C SCL | I2C0_SCL | I2C0_SCL | Requires 4.7kΩ pull-up to VCC |
| READY | PA18 (output) | PA18 (input) | Target → Controller handshake |
| TRIGGER | PA27 (input) | PA27 (output) | Controller → Target handshake |
| Ground | GND | GND | Common ground required |
| Power | 3.3V | 3.3V | Each board powered separately |

## Visual Diagram

```
    TARGET BOARD (DUT)              CONTROLLER BOARD
   ┌─────────────────┐             ┌─────────────────┐
   │                 │             │                 │
   │  PA18 (OUT) ────┼─────────────┼──── PA18 (IN)  │  READY signal
   │                 │             │                 │
   │  PA27 (IN)  ────┼─────────────┼──── PA27 (OUT) │  TRIGGER signal
   │                 │             │                 │
   │  SDA (I2C0) ────┼─────────────┼──── SDA (I2C0) │
   │         │       │             │       │         │
   │        4.7kΩ    │             │      4.7kΩ      │
   │         │       │             │       │         │
   │  SCL (I2C0) ────┼─────────────┼──── SCL (I2C0) │
   │         │       │             │       │         │
   │        4.7kΩ    │             │      4.7kΩ      │
   │         │       │             │       │         │
   │  GND ───────────┼─────────────┼──── GND        │
   │                 │             │                 │
   │  3.3V           │             │           3.3V  │
   │   │             │             │             │   │
   └───┼─────────────┘             └─────────────┼───┘
       │                                         │
       │                                         │
     Power                                    Power
     Source                                   Source
```

## LP-MSPM0G3507 Specific Pins

For the LP-MSPM0G3507 LaunchPad, use these header locations:

### I2C0 Pins (Default Configuration)
- **SDA**: Check your board's schematic for I2C0_SDA pin location
- **SCL**: Check your board's schematic for I2C0_SCL pin location

### GPIO Pins (Configurable)
- **PA18**: User configurable GPIO
- **PA27**: User configurable GPIO

**Note**: Verify the exact pin locations on your specific board's pinout diagram. The overlay files specify the logical GPIO names; physical header locations depend on your board layout.

## Pull-up Resistors

The I2C bus requires pull-up resistors on both SDA and SCL:
- **Recommended value**: 4.7kΩ
- **Voltage**: Pull up to 3.3V
- **Location**: Can be on either board, or external

Some development boards have built-in I2C pull-ups. Check your board's schematic.

## Signal Levels

All signals operate at **3.3V logic levels**:
- Logic HIGH: ~3.3V
- Logic LOW: 0V (GND)

## Testing the Wiring

### Before Running Tests

1. **Power off both boards**
2. **Measure continuity** between connected pins with a multimeter
3. **Check for shorts** between VCC and GND
4. **Verify pull-up resistors** are present on SDA/SCL

### With Power On (Before Running Firmware)

1. **Measure voltages**:
   - I2C SDA should read ~3.3V (pulled high)
   - I2C SCL should read ~3.3V (pulled high)
   - GPIO pins should read 0V (default state)

2. **Use a logic analyzer** (optional but recommended):
   - Connect to SDA, SCL, READY, TRIGGER
   - Verify signal integrity during test runs
   - Check I2C timing parameters

## Common Wiring Issues

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| I2C transaction fails immediately | Missing pull-ups | Add 4.7kΩ resistors |
| Controller never starts test | READY signal not connected | Check PA18 wiring |
| Target never receives trigger | TRIGGER signal not connected | Check PA27 wiring |
| Intermittent failures | Loose connection | Check all connections |
| Both boards hang | Possible short circuit | Power off, check for shorts |

## Using a Logic Analyzer

For debugging, connect a logic analyzer to monitor:
- **Channel 0**: SDA
- **Channel 1**: SCL
- **Channel 2**: READY (optional)
- **Channel 3**: TRIGGER (optional)

Configure the I2C decoder:
- **Mode**: I2C
- **Speed**: 100 kHz (Standard)
- **Address**: 0x54 (7-bit)

This allows you to verify:
- Correct address transmission
- Data integrity
- ACK/NACK responses
- Timing violations
