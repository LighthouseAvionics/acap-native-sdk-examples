#!/bin/sh
# Script to reset the IR MCU via GPIO
# Usage: ./reset_ir_mcu.sh
#
# Note: active_low=1, so writing 0 asserts reset (high), writing 1 deasserts (low)

IR_RESET_GPIO="/sys/class/gpio/ir_mcu_reset"

if [ ! -d "${IR_RESET_GPIO}" ]; then
    echo "Error: IR MCU reset GPIO not found at ${IR_RESET_GPIO}"
    exit 1
fi

echo "Resetting IR MCU via GPIO..."

# Read current active_low setting
ACTIVE_LOW=$(cat "${IR_RESET_GPIO}/active_low")
echo "GPIO active_low setting: ${ACTIVE_LOW}"

if [ "$ACTIVE_LOW" = "1" ]; then
    # active_low=1: write 0 to assert reset, 1 to deassert
    echo "Asserting reset (active low)..."
    echo 0 > "${IR_RESET_GPIO}/value"
    sleep 1
    echo "Deasserting reset..."
    echo 1 > "${IR_RESET_GPIO}/value"
    sleep 1
else
    # active_low=0: write 1 to assert reset, 0 to deassert
    echo "Asserting reset (active high)..."
    echo 1 > "${IR_RESET_GPIO}/value"
    sleep 1
    echo "Deasserting reset..."
    echo 0 > "${IR_RESET_GPIO}/value"
    sleep 1
fi

echo "IR MCU GPIO reset complete"
echo ""
echo "Note: The IR MCU may require kernel driver initialization to respond."
echo "The device appears to need specific command sequences that only the"
echo "q62-irmcu driver knows how to send."
