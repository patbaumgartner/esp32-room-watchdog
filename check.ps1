# Local "check" alias: run unit tests, then build the firmware.
# Usage: ./check.ps1
$ErrorActionPreference = 'Stop'

$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'
if (-not (Test-Path $pio)) { $pio = 'pio' }

& $pio test -e native
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $pio run -e esp32-c3-supermini
exit $LASTEXITCODE
