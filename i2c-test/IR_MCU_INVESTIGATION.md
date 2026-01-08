# IR MCU Boot Loop Investigation

## Problem Summary

The IR MCU at I2C bus 0, address 0x54 is causing the I2C controller to enter recovery loops, which causes:
- I2C operations to hang or timeout
- Repeated "controller recovery failed" messages in dmesg
- Services (temperature-controller, light-controller) to fail reading hardware status

## Root Cause Analysis

### Boot Sequence

1. **q62-irmcu-firmware-loader.service** runs at boot (Type=oneshot)
   - Location: `/lib/systemd/system/q62-irmcu-firmware-loader.service`
   - Script: `/usr/libexec/q62-irmcu-firmware-loader`
   - Required by: `temperature-controller.service`, `light-controller.service`

2. **Firmware loader attempts to:**
   - Read MCU firmware version from `/sys/devices/platform/q62-irmcu/q62-ir/fw`
   - Compare with expected version (v8) from `/lib/firmware/q62-ir.cfg`
   - If version mismatch or unavailable, flash firmware using `stm32flash`
   - Trigger kernel driver probe: `echo 0-0054 > /sys/bus/i2c/drivers_probe`

3. **Kernel driver (q62-irmcu.ko) probe function:**
   - Tries to reset the I2C device via GPIO (pins 486=reset, 487=boot)
   - Attempts I2C communication to verify device
   - **FAILS** with "controller recovery failed" errors
   - **FAILS** with "Failed to communicate with i2c device"

4. **Device registration but no functionality:**
   - Driver eventually registers: "Registered q62-ir bus=0 addr=0x54 irq=-1 reset=-1(0)"
   - But device files (`/sys/.../fw`, hwmon, etc.) are **never created**
   - Device binding status: NO driver bound (unbind successful, but probe fails)

### Why Probe Keeps Failing

The IR MCU at bus 0, address 0x54 is not responding to I2C transactions:
- Device returns NACK (Remote I/O error)
- I2C controller tries to recover the bus
- Recovery fails because device is holding SDA/SCL in bad state
- After 10+ seconds, controller gives up

### Dependent Services

Services that depend on the IR MCU and are currently degraded:

**temperature-controller.service**
- Tries to read fan status/speed files
- Files don't exist because IR MCU driver failed to initialize
- Repeatedly logs errors every 5 seconds

**light-controller.service**
- Similar dependency on IR MCU hardware
- Running but may have degraded functionality

## Hardware Details

### I2C Configuration
- **Bus**: 0 (f801b800.i2c, nec-i2c driver)
- **Device Address**: 0x54 (firmware mode), 0x3B (bootloader mode)
- **Device Tree**: `/sys/firmware/devicetree/base/amba/i2c@f801b800/ir_board@54`
- **Compatible**: "axis,q62-ir"

### GPIO Pins
- **ir_mcu_reset**: GPIO 486 (controls reset line, active_low=1)
- **ir_mcu_boot**: GPIO 487 (controls boot mode)
- Available at: `/sys/class/gpio/ir_mcu_reset/`

### MCU Firmware
- **Expected version**: 8
- **Hex file**: `/lib/firmware/q62-ir.hex`
- **Config**: `/lib/firmware/q62-ir.cfg`
- **Programmer**: stm32flash (I2C-based STM32 flasher)

### Other IR MCUs in System
The system has another working IR MCU on bus 5:
- **Device**: 5-0054 (`/sys/bus/i2c/devices/5-0054/`)
- **hwmon**: hwmon2 (temperature/fan control working)
- This MCU is functioning correctly

## Solutions

### Option 1: Disable Automatic Probing (Recommended for Testing)

Prevent the q62-irmcu driver from automatically binding:

```bash
# Stop services that depend on IR MCU
systemctl stop temperature-controller.service light-controller.service

# Mask the firmware loader service
systemctl mask q62-irmcu-firmware-loader.service

# Blacklist the kernel module
echo "blacklist q62_irmcu" > /etc/modprobe.d/blacklist-irmcu.conf

# Reboot
reboot
```

This allows I2C testing without interference.

### Option 2: Fix Hardware/Firmware Issue

The device itself needs attention:

1. **Check hardware connections**
   - Verify I2C pullups (SDA/SCL)
   - Check power supply to IR MCU
   - Inspect physical connections

2. **Try firmware recovery**
   ```bash
   # Erase MCU flash
   /usr/libexec/q62-irmcu-firmware-loader erase

   # Force firmware upgrade
   /usr/libexec/q62-irmcu-firmware-loader forced
   ```

3. **Manual GPIO reset**
   ```bash
   # Use the reset script
   /root/reset_ir_mcu.sh
   ```

### Option 3: Unbind Driver Permanently

Keep the service enabled but prevent driver binding:

```bash
# Create script to unbind after boot
cat > /etc/systemd/system/q62-irmcu-unbind.service << 'EOF'
[Unit]
Description=Unbind q62-irmcu driver
After=q62-irmcu-firmware-loader.service

[Service]
Type=oneshot
ExecStart=/usr/local/packages/i2c_unbind/i2c_unbind unbind 0 0x54

[Install]
WantedBy=multi-user.target
EOF

systemctl enable q62-irmcu-unbind.service
```

## Current Workaround

To use I2C tools without hanging:

1. **Reset I2C controller** (clears recovery loops):
   ```bash
   /root/reset_i2c_controller.sh yes
   ```

2. **This will:**
   - Kill processes using I2C
   - Unbind nec-i2c controller driver
   - Rebind controller (resets hardware)
   - Unbind q62-irmcu driver from device
   - Allow raw I2C access

3. **I2C commands will complete** (but device still won't respond):
   ```bash
   /usr/local/packages/i2c_test/i2c_test raw-read 0 0x54 4
   # Returns: Error: Failed to read: Remote I/O error
   # But completes immediately instead of hanging
   ```

## Investigation Commands

```bash
# Check if driver is bound
ls -la /sys/bus/i2c/devices/0-0054/driver

# View driver probe failures
dmesg | grep q62-irmcu

# Monitor I2C recovery attempts
watch -n 1 'dmesg | tail -5'

# Check firmware loader status
systemctl status q62-irmcu-firmware-loader.service

# View dependent service errors
journalctl -u temperature-controller.service -n 20

# Manually trigger probe (will cause recovery loop)
echo 0-0054 > /sys/bus/i2c/drivers_probe

# Check GPIO state
cat /sys/class/gpio/ir_mcu_reset/value
cat /sys/class/gpio/ir_mcu_reset/active_low

# List all MCUs in system
ls -la /sys/bus/i2c/devices/ | grep '00[0-9][0-9]'
```

## Files Involved

### Systemd Services
- `/lib/systemd/system/q62-irmcu-firmware-loader.service`
- `/lib/systemd/system/temperature-controller.service`
- `/lib/systemd/system/light-controller.service`

### Scripts
- `/usr/libexec/q62-irmcu-firmware-loader` - Firmware upgrade script
- `/usr/bin/stm32flash` - STM32 I2C programmer

### Kernel Module
- `/lib/modules/5.15.13-axis9/extra/q62-irmcu.ko`
- Driver author: Johan Hellman <johanhn@axis.com>
- Module name: q62_irmcu
- Parameter: MODE (uint)

### Firmware
- `/lib/firmware/q62-ir.hex` - MCU firmware image
- `/lib/firmware/q62-ir.cfg` - MCU firmware config (version 8)

### Device Tree
- `/sys/firmware/devicetree/base/amba/i2c@f801b800/ir_board@54/`

## Next Steps

To fully resolve the issue, need to determine:

1. **Why is the MCU not responding?**
   - Hardware failure?
   - Wrong firmware?
   - Power supply issue?
   - I2C bus electrical problems?

2. **Is the MCU actually populated?**
   - This camera might not have the IR MCU installed
   - Device tree might be generic across product line
   - Check hardware documentation for Q6225-LE

3. **Can we disable gracefully?**
   - Remove device from device tree overlay
   - Modify firmware loader to skip if device not present
   - Update services to handle missing IR MCU

4. **Do we need this MCU?**
   - What functionality does it provide?
   - Temperature monitoring? (hwmon2 on bus 5 is working)
   - IR illumination control?
   - Can system operate without it?
