#!/usr/bin/env bash
# Auto-format all C++ source files using clang-format.
# Uses the .clang-format config at the project root (WebKit style).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

find core platform app tests \
    -type f \( -name '*.cpp' -o -name '*.h' \) \
    -exec clang-format -i {} +

echo "Done. All files formatted."
