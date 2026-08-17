#!/usr/bin/env bash
# Local quality gate: unit tests, static analysis, firmware build.
# Usage: ./check.sh
set -euo pipefail

pio=pio
if ! command -v "$pio" >/dev/null 2>&1; then
    pio="$HOME/.platformio/penv/bin/pio"
fi

"$pio" test -e native
"$pio" check -e esp32-c3-supermini --fail-on-defect=medium --fail-on-defect=high
"$pio" run -e esp32-c3-supermini
