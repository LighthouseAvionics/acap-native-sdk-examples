# I2C Test Utility for Axis Cameras

ACAP application for testing I2C communication on Axis cameras, with support for both SMBUS and raw I2C protocols.

## Features

- SMBUS single-byte read/write operations
- Raw I2C multi-byte transactions with repeated start
- Register dump with timeout protection
- Programmatic I2C controller reset (no reboot required)

## Installation

```bash
# Build the package
docker build --tag <APP_IMAGE> .
docker cp $(docker create <APP_IMAGE>):/opt/app ./build
scp i2c_test_*_armv7hf.eap root@<camera-ip>:/tmp/

# Install on camera
ssh root@<camera-ip>
eap-install.sh install /tmp/i2c_test_*_armv7hf.eap
```

## Usage

### SMBUS Commands

```bash
# Read single byte from register
/usr/local/packages/i2c_test/i2c_test read <bus> <addr> <reg>
# Example: /usr/local/packages/i2c_test/i2c_test read 0 0x54 0x01

# Write single byte to register
/usr/local/packages/i2c_test/i2c_test write <bus> <addr> <reg> <value>
# Example: /usr/local/packages/i2c_test/i2c_test write 0 0x54 0x10 0xAB

# Dump register range (with 3-second timeout per register)
/usr/local/packages/i2c_test/i2c_test dump <bus> <addr> <start_reg> <end_reg>
# Example: /usr/local/packages/i2c_test/i2c_test dump 0 0x54 0x00 0x1F
```

### Raw I2C Commands

For devices requiring multi-byte transactions with repeated start:

```bash
# Write register address, then read N bytes (with repeated start)
/usr/local/packages/i2c_test/i2c_test raw-wr <bus> <addr> <reg> <count>
# Example: /usr/local/packages/i2c_test/i2c_test raw-wr 0 0x54 0x13 4

# Write multiple bytes
/usr/local/packages/i2c_test/i2c_test raw-write <bus> <addr> <byte1> [byte2...]
# Example: /usr/local/packages/i2c_test/i2c_test raw-write 0 0x54 0x28 0x00 0x00 0x00

# Read N bytes directly (no register address)
/usr/local/packages/i2c_test/i2c_test raw-read <bus> <addr> <count>
# Example: /usr/local/packages/i2c_test/i2c_test raw-read 0 0x54 4
```

## Troubleshooting

### I2C Controller Stuck in Recovery Loop

If you see repeated "controller recovery failed" errors in dmesg, the I2C controller is stuck. Use the reset script:

```bash
# Copy script to camera
scp reset_i2c_controller.sh root@<camera-ip>:/tmp/

# Run on camera
ssh root@<camera-ip>
chmod +x /tmp/reset_i2c_controller.sh

# Reset controller only
/tmp/reset_i2c_controller.sh

# Reset controller and unbind kernel drivers
/tmp/reset_i2c_controller.sh yes
```

The script will:
1. Kill any processes using I2C
2. Unbind the I2C controller driver (nec-i2c)
3. Rebind the controller (resets hardware state)
4. Optionally unbind kernel drivers from I2C devices

### Device Busy Error

If you get "Device or resource busy", a kernel driver is bound to the device. Unbind it:

```bash
# Using i2c_unbind utility
/usr/local/packages/i2c_unbind/i2c_unbind unbind 0 0x54

# Or manually
echo '0-0054' > /sys/bus/i2c/drivers/q62-irmcu/unbind
```

### Remote I/O Error

"Remote I/O error" means the device is NACK'ing the transaction. This can happen if:
- Device needs initialization by its kernel driver
- Device is in a bad state and needs reset
- Wrong I2C address or protocol

For the IR MCU (0x54), try the GPIO reset:

```bash
scp reset_ir_mcu.sh root@<camera-ip>:/tmp/
ssh root@<camera-ip> 'chmod +x /tmp/reset_ir_mcu.sh && /tmp/reset_ir_mcu.sh'
```

Note: The IR MCU may require specific initialization sequences that only the q62-irmcu kernel driver knows.

## Technical Details

### Timeout Protection

The `dump` command uses a 3-second alarm timeout per register to prevent infinite hangs when accessing problematic I2C devices. If a register times out, it's marked as "TO" in the output.

### Raw I2C Implementation

Raw I2C commands use the `I2C_RDWR` ioctl with `i2c_msg` structures, allowing:
- Multi-byte read/write transactions
- Repeated start sequences (no STOP between messages)
- Direct control over I2C protocol

This is necessary for devices that don't support SMBUS-style combined write+read transactions.

### I2C_TIMEOUT and I2C_RETRIES

The tool sets:
- `I2C_TIMEOUT`: 100ms (10 ticks × 10ms)
- `I2C_RETRIES`: 1 retry

These are kernel-level settings that control hardware timeout behavior.

## Files

- `i2c_test.c` - Main application
- `reset_i2c_controller.sh` - Reset I2C controller without reboot
- `reset_ir_mcu.sh` - Reset IR MCU via GPIO
- `manifest.json` - ACAP manifest

## Related Tools

- `i2c-unbind` - Utility to unbind/bind kernel I2C drivers
- `lrf-controller` - HTTP API for laser rangefinder control

## Version History

- **1.0.3** - Fixed alarm interference with raw I2C ioctls
- **1.0.2** - Added alarm timeout protection for dump command
- **1.0.1** - Initial raw I2C support
- **1.0.0** - Initial SMBUS implementation
