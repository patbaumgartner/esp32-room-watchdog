# Local "deploy" alias: run checks, then flash the firmware to the board.
# Usage: ./deploy.ps1 [-Port COM3] [-SkipChecks]
param(
    [string]$Port = 'COM3',
    [switch]$SkipChecks
)
$ErrorActionPreference = 'Stop'

$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'
if (-not (Test-Path $pio)) { $pio = 'pio' }

if (-not $SkipChecks) {
    & $pio test -e native
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $pio run -e esp32-c3-supermini -t upload --upload-port $Port
exit $LASTEXITCODE
