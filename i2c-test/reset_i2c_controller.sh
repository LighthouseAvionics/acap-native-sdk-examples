#!/bin/sh
# Script to reset the I2C controller (bus 0) without rebooting
# Usage: ./reset_i2c_controller.sh [unbind_devices]
#   unbind_devices: Optional, if "yes" will unbind kernel drivers from I2C devices

I2C_CONTROLLER="f801b800.i2c"
DRIVER_PATH="/sys/bus/platform/drivers/nec-i2c"
UNBIND_DEVICES="$1"

echo "Resetting I2C controller ${I2C_CONTROLLER}..."

# Kill any processes using I2C
killall -9 i2c_test 2>/dev/null || true
killall -9 lrf_controller 2>/dev/null || true
sleep 1

# Unbind the I2C controller
echo "Unbinding controller..."
echo "${I2C_CONTROLLER}" > "${DRIVER_PATH}/unbind" 2>/dev/null || true
sleep 2

# Rebind the I2C controller
echo "Rebinding controller..."
echo "${I2C_CONTROLLER}" > "${DRIVER_PATH}/bind" 2>/dev/null &
BIND_PID=$!

# Wait up to 30 seconds for bind to complete
echo "Waiting for device to come back..."
i=0
while [ $i -lt 30 ]; do
    i=$((i + 1))
    if [ -e /dev/i2c-0 ]; then
        echo "I2C controller reset successfully!"
        echo "Device /dev/i2c-0 is available"

        # Give kernel driver time to probe
        sleep 2

        # Unbind kernel drivers if requested
        if [ "$UNBIND_DEVICES" = "yes" ]; then
            echo "Unbinding kernel drivers from I2C devices..."
            if [ -L /sys/bus/i2c/devices/0-0054/driver ]; then
                if command -v /usr/local/packages/i2c_unbind/i2c_unbind >/dev/null 2>&1; then
                    /usr/local/packages/i2c_unbind/i2c_unbind unbind 0 0x54
                else
                    echo "Warning: i2c_unbind not found, manually unbind with:"
                    echo "  echo '0-0054' > /sys/bus/i2c/drivers/q62-irmcu/unbind"
                fi
            fi
        else
            # Just warn if driver is bound
            if [ -L /sys/bus/i2c/devices/0-0054/driver ]; then
                echo "Note: q62-irmcu driver is bound to device 0x54"
                echo "Run with 'yes' argument to auto-unbind:"
                echo "  $0 yes"
            fi
        fi

        exit 0
    fi
    sleep 1
done

echo "Error: I2C controller did not come back up within 30 seconds"
exit 1
