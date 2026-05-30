#!/usr/bin/env bash
# Build the desktop simulator target.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

cmake -B build/sim -DTARGET_PLATFORM=sim
cmake --build build/sim -j"$(nproc)"

echo "Build complete: build/sim/app/sim/exhaust_sim"
