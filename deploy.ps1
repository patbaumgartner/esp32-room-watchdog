# Local "deploy" alias: run the quality gate, then flash the firmware.
# Usage: ./deploy.ps1 [-Port COM3] [-SkipChecks]
# The gate is check.sh / check.ps1 so this cannot drift from CI.
param(
    [string]$Port = 'COM3',
    [switch]$SkipChecks
)
$ErrorActionPreference = 'Stop'

$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'
if (-not (Test-Path $pio)) { $pio = 'pio' }

$wslPath = [regex]::Match($PSScriptRoot, '^\\\\wsl(?:\.localhost|\$)\\([^\\]+)\\(.+)$')
if ($wslPath.Success) {
    $distribution = $wslPath.Groups[1].Value
    $linuxProject = '/' + $wslPath.Groups[2].Value.Replace('\', '/')
    $linuxPio = '~/.platformio/penv/bin/platformio'

    if (-not $SkipChecks) {
        & wsl.exe --distribution $distribution --cd $linuxProject ./check.sh
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    else {
        & wsl.exe --distribution $distribution --cd $linuxProject $linuxPio run -e esp32-c3-supermini
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    $python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
    $esptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'
    $bootApp = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'
    $build = Join-Path $PSScriptRoot '.pio\build\esp32-c3-supermini'
    $temp = Join-Path $env:TEMP ('esp32-room-watchdog-flash-' + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $temp | Out-Null

    try {
        Copy-Item -LiteralPath `
            (Join-Path $build 'bootloader.bin'), `
            (Join-Path $build 'partitions.bin'), `
            (Join-Path $build 'firmware.bin') `
            -Destination $temp

        & $python $esptool --chip esp32c3 --port $Port --baud 921600 `
            --before default_reset --after hard_reset write_flash -z `
            --flash_mode dio --flash_freq 80m --flash_size 4MB `
            0x0 (Join-Path $temp 'bootloader.bin') `
            0x8000 (Join-Path $temp 'partitions.bin') `
            0xe000 $bootApp `
            0x10000 (Join-Path $temp 'firmware.bin')
        exit $LASTEXITCODE
    }
    finally {
        Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if (-not $SkipChecks) {
    & (Join-Path $PSScriptRoot 'check.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $pio run -e esp32-c3-supermini -t upload --upload-port $Port
exit $LASTEXITCODE
