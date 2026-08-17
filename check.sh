#!/usr/bin/env bash
# Local quality gate: unit tests, static analysis, firmware build.
# Usage: ./check.sh
set -euo pipefail

pio=pio
if ! command -v "$pio" >/dev/null 2>&1; then
    pio="$HOME/.platformio/penv/bin/pio"
fi

"$pio" test -e native
"$pio" run -e esp32-c3-supermini -t compiledb
"$pio" pkg exec --package platformio/tool-cppcheck@1.21100.230717 -- \
    cppcheck --project=compile_commands.json '--file-filter=src/*' \
    --enable=warning,style,performance,portability \
    --inline-suppr '--suppress=*:*platformio*packages*' \
    '--suppress=*:*libdeps*' \
    --suppress=missingIncludeSystem --error-exitcode=1
"$pio" run -e esp32-c3-supermini
