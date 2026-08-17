[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DeviceIp,

    [string]$Output = "room-watchdog-$(Get-Date -Format 'yyyyMMdd-HHmmss').wav"
)

$ErrorActionPreference = 'Stop'

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw 'ffmpeg is required and was not found on PATH.'
}

# Kept out of the parameter list so it does not land in the shell history.
$token = $env:WATCHDOG_API_TOKEN
if (-not $token) {
    $secure = Read-Host -Prompt 'API token' -AsSecureString
    $token = [System.Net.NetworkCredential]::new('', $secure).Password
}

& ffmpeg -hide_banner `
    -headers "Authorization: Bearer $token`r`n" `
    -f s16le -ar 48000 -ac 1 `
    -i "http://${DeviceIp}/audio.pcm" `
    -c:a pcm_s16le $Output
exit $LASTEXITCODE
exit $LASTEXITCODE