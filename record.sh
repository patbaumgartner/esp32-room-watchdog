#!/usr/bin/env bash
# Record the device's live PCM stream to a WAV file until you press Ctrl+C.
#
# Usage:
#   export WATCHDOG_API_TOKEN='<API_TOKEN>'   # or let the script prompt
#   ./record.sh 192.168.1.42 [room.wav]
set -euo pipefail

device_ip=${1:-}
output=${2:-room-watchdog-$(date +%Y%m%d-%H%M%S).wav}

if [ -z "$device_ip" ]; then
    echo "usage: $0 <device-ip> [output.wav]" >&2
    exit 2
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg is required and was not found on PATH." >&2
    exit 1
fi

token=${WATCHDOG_API_TOKEN:-}
if [ -z "$token" ]; then
    read -rsp 'API token: ' token
    echo
fi

exec ffmpeg -hide_banner \
    -headers "Authorization: Bearer ${token}"$'\r\n' \
    -f s16le -ar 48000 -ac 1 \
    -i "http://${device_ip}/audio.pcm" \
    -c:a pcm_s16le "$output"
