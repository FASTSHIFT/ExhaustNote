#!/usr/bin/env bash
# Build with coverage and run tests, then generate HTML report.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Build with coverage
cmake -B build/test -DTARGET_PLATFORM=sim -DENABLE_COVERAGE=ON
cmake --build build/test -j"$(nproc)"

# Run tests
cd build/test
ctest --output-on-failure

# Generate coverage report
cd "$PROJECT_ROOT"
lcov --capture --directory build/test --output-file coverage.info \
     --ignore-errors mismatch
lcov --remove coverage.info '/usr/*' '*/third_party/*' '*/tests/*' '*/_deps/*' \
     --output-file coverage.info --ignore-errors unused

# Summary
lcov --list coverage.info

# Generate HTML report (optional, requires genhtml)
if command -v genhtml &> /dev/null; then
    genhtml coverage.info --output-directory coverage/
    echo "HTML coverage report: coverage/index.html"
fi
