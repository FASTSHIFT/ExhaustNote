#!/bin/bash
# Flash ExhaustNote firmware to AT32F437VMT7 via ATLink
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ATLINK_DIR="$SCRIPT_DIR/atlink"
FIRMWARE="$PROJECT_DIR/build/mcu/exhaust_mcu.bin"

# Check firmware exists
if [ ! -f "$FIRMWARE" ]; then
    echo "ERROR: Firmware not found at $FIRMWARE"
    echo "Build first: cmake --build build/mcu -j\$(nproc)"
    exit 1
fi

echo "=== ExhaustNote Flash Tool ==="
echo "Firmware: $FIRMWARE"
echo "Size: $(stat -c%s "$FIRMWARE") bytes"
echo "Target: AT32F437VMT7"
echo ""

cd "$ATLINK_DIR"
export LD_LIBRARY_PATH="$ATLINK_DIR"

# Flash command:
#   -device AT32F437VMT7    : Target MCU
#   -connect                : Connect to target
#   -p --dfap --depp        : Disable flash/SRAM protection
#   -e --all                : Erase all flash
#   -d --a 08000000 --fn    : Download binary to 0x08000000
#   --v                     : Verify after download
./ATLink_Console \
    -device AT32F437VMT7 \
    -connect \
    -p --dfap --depp \
    -e --all \
    -d --a 08000000 --fn "$FIRMWARE" --v

echo ""
echo "=== Flash complete! ==="
