[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DeviceIp,

    [Parameter(Mandatory = $true)]
    [string]$ApiToken,

    [string]$Output = "room-watchdog-$(Get-Date -Format 'yyyyMMdd-HHmmss').wav"
)

$ErrorActionPreference = 'Stop'

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw 'ffmpeg is required and was not found on PATH.'
}

$postData = -join ([System.Text.Encoding]::ASCII.GetBytes($ApiToken) | ForEach-Object { $_.ToString('x2') })
& ffmpeg -hide_banner -f s16le -ar 48000 -ac 1 -method POST `
    -post_data $postData -i "http://${DeviceIp}/audio.pcm" -c:a pcm_s16le $Output
exit $LASTEXITCODE