# Local "check" alias: run unit tests, static analysis, then build the firmware.
# Usage: ./check.ps1
$ErrorActionPreference = 'Stop'

$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'
if (-not (Test-Path $pio)) { $pio = 'pio' }

& $pio test -e native
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $pio run -e esp32-c3-supermini -t compiledb
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $pio pkg exec --package platformio/tool-cppcheck@1.21100.230717 -- `
    cppcheck --project=compile_commands.json '--file-filter=src/*' `
    --enable=warning,style,performance,portability `
    --inline-suppr '--suppress=*:*platformio*packages*' `
    --suppress=missingIncludeSystem --error-exitcode=1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $pio run -e esp32-c3-supermini
exit $LASTEXITCODE
