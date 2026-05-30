#!/usr/bin/env bash
# Auto-format all C++ source files using clang-format.
# Uses the .clang-format config at the project root (WebKit style).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Format project code only (exclude third-party: AT32 SDK, BSP, Artery, cJSON)
find core app tests tools platform/include platform/sim \
    -type f \( -name '*.cpp' -o -name '*.h' \) \
    -exec clang-format -i {} +

# Format MCU app code (but not BSP/Artery)
find app/mcu -maxdepth 1 -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) \
    ! -name 'system_call.c' ! -name 'cxx_stubs.cpp' \
    -exec clang-format -i {} +

echo "Done. All files formatted."
