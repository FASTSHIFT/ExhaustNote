#!/usr/bin/env bash
# Auto-format project C/C++ source files using clang-format.
# Uses the .clang-format config at the project root.
#
# Usage:
#   ./tools/format.sh          # Format in-place
#   ./tools/format.sh --check  # Dry-run check (for CI, exits non-zero on diff)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Determine mode
if [[ "${1:-}" == "--check" ]]; then
    FORMAT_FLAGS="--dry-run --Werror"
    echo "Checking format..."
else
    FORMAT_FLAGS="-i"
fi

# Collect files (exclude vendor SDK, BSP, third_party)
FILES=$(find core app tests platform/include platform/sim \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) \
    ! -path '*/platform/at32/Artery/*' \
    ! -path '*/platform/at32/bsp/*' \
    ! -path '*/third_party/*' \
    ! -name 'system_call.c' \
    2>/dev/null | sort)

if [[ -z "$FILES" ]]; then
    echo "No files to format."
    exit 0
fi

# shellcheck disable=SC2086
echo "$FILES" | xargs clang-format $FORMAT_FLAGS

if [[ "${1:-}" != "--check" ]]; then
    echo "Done. All files formatted."
fi
