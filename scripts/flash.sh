#!/usr/bin/env bash
# Flash the MCU firmware using OpenOCD + AT-Link (CMSIS-DAP/SWD)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOLS_DIR="$PROJECT_ROOT/platform/at32/Artery/Tools"

BIN_FILE="$PROJECT_ROOT/build/mcu/exhaust_mcu.bin"

if [ ! -f "$BIN_FILE" ]; then
    echo "ERROR: Firmware not found: $BIN_FILE"
    echo "Run scripts/build_mcu.sh first"
    exit 1
fi

echo "Flashing: $BIN_FILE"
echo "Using OpenOCD config: $TOOLS_DIR/openocd_at32f437.cfg"

openocd -f "$TOOLS_DIR/openocd_at32f437.cfg" \
    -c "program $BIN_FILE 0x08000000 verify reset exit"
