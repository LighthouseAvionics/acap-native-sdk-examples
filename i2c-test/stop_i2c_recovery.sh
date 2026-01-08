#!/bin/sh
# Script to stop I2C controller recovery loop by unbinding the controller
# This leaves the I2C bus inaccessible but stops the recovery spam
# Usage: ./stop_i2c_recovery.sh

I2C_CONTROLLER="f801b800.i2c"
DRIVER_PATH="/sys/bus/platform/drivers/nec-i2c"

echo "Stopping I2C controller recovery loop..."

# Kill any processes using I2C
killall -9 i2c_test 2>/dev/null || true
killall -9 lrf_controller 2>/dev/null || true
sleep 1

# Check if controller is bound
if [ ! -L "${DRIVER_PATH}/${I2C_CONTROLLER}" ]; then
    echo "Controller is not bound, nothing to do"
    exit 0
fi

# Unbind the I2C controller
echo "Unbinding controller..."
echo "${I2C_CONTROLLER}" > "${DRIVER_PATH}/unbind" 2>/dev/null || true
sleep 2

# Verify it's unbound
if [ ! -L "${DRIVER_PATH}/${I2C_CONTROLLER}" ]; then
    echo "I2C controller unbound successfully"
    echo "/dev/i2c-0 is no longer available"
    echo ""
    echo "To restore I2C access, reboot the camera"
    exit 0
else
    echo "Error: Failed to unbind controller"
    exit 1
fi
