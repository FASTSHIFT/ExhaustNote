#!/usr/bin/env bash
# Build the MCU firmware for AT32F437.
# Requires arm-none-eabi-gcc in PATH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Check toolchain
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "ERROR: arm-none-eabi-gcc not found in PATH"
    echo "Install with: sudo apt install gcc-arm-none-eabi"
    exit 1
fi

echo "ARM GCC version: $(arm-none-eabi-gcc --version | head -1)"

cmake -B build/mcu -S app/mcu \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build/mcu -j"$(nproc)"

echo ""
echo "Build complete!"
echo "  Firmware: build/mcu/exhaust_mcu.bin"
echo "  Flash with: scripts/flash.sh"
