#!/usr/bin/env bash
# Record the device's live PCM stream to a WAV file until you press Ctrl+C.
#
# Usage:
#   export WATCHDOG_API_TOKEN='<API_TOKEN>'   # or let the script prompt
#   ./record.sh [host[:port]] [room.wav]
#
# host defaults to the node's mDNS name. Pass host:port, or a full URL base,
# when going through a reverse proxy.
set -euo pipefail

device=${1:-watchdog.local}
output=${2:-room-watchdog-$(date +%Y%m%d-%H%M%S).wav}

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
    -i "http://${device}/audio.pcm" \
    -c:a pcm_s16le "$output"
