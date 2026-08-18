#!/usr/bin/env bash
# Smoke-tests a flashed ESP32-C3 room watchdog over the LAN: API responses,
# the authentication matrix, and audio throughput.
#
# Usage: probe.sh [host]        host defaults to watchdog.local
#
# Reads API_TOKEN from the gitignored src/secrets.h and never prints it.
set -uo pipefail

HOST=${1:-watchdog.local}
REPO=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel 2>/dev/null || pwd)
SECRETS="$REPO/src/secrets.h"

[ -f "$SECRETS" ] || { echo "src/secrets.h not found — copy it from src/secrets.h.example" >&2; exit 1; }
TOKEN=$(grep -oP '#define\s+API_TOKEN\s+"\K[^"]+' "$SECRETS")
[ -n "$TOKEN" ] || { echo "API_TOKEN not found in src/secrets.h" >&2; exit 1; }

AUTH=(-H "Authorization: Bearer $TOKEN")
WRONG=(-H "Authorization: Bearer definitely-not-the-right-token")
fail=0

# A leftover stream would make everything below return 409.
pkill -f "audio.pcm" 2>/dev/null || true
sleep 1

echo "== status =="
STATUS=$(curl -s -m 8 "${AUTH[@]}" "http://$HOST/status")
if [ -z "$STATUS" ]; then
  echo "  no response from $HOST" >&2
  exit 1
fi
echo "  $STATUS"

echo "== authentication matrix =="
for spec in "GET|http://$HOST/status|200" "POST|http://$HOST/calibrate|401" "GET|http://$HOST:81/audio.pcm|200"; do
  method=${spec%%|*}; rest=${spec#*|}; url=${rest%|*}
  none=$(curl -s -o /dev/null -w '%{http_code}' -m 6 -X "$method" "$url")
  wrong=$(curl -s -o /dev/null -w '%{http_code}' -m 6 -X "$method" "${WRONG[@]}" "$url")
  printf '  %-32s none=%s wrong=%s\n' "$method ${url#http://}" "$none" "$wrong"
  [ "$none" = "401" ] && [ "$wrong" = "401" ] || { echo "    ^ EXPECTED 401 FOR BOTH" >&2; fail=1; }
done
# /calibrate is only probed unauthenticated on purpose: a valid call starts a
# real ~2 minute radar calibration and rewrites the background baseline.

echo "== websocket handshake =="
ws() { curl -s -o /dev/null -w '%{http_code}' -m 6 "$@" \
  -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Sec-WebSocket-Version: 13" \
  -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" "http://$HOST/ws"; }
no_tok=$(ws); with_tok=$(ws "${AUTH[@]}")
echo "  none=$no_tok  valid=$with_tok"
[ "$no_tok" = "401" ] && [ "$with_tok" = "101" ] || { echo "    ^ EXPECTED 401 then 101" >&2; fail=1; }

echo "== audio throughput and drops (8s, sampled mid-stream) =="
PCM=$(mktemp); trap 'rm -f "$PCM"' EXIT
curl -s "${AUTH[@]}" "http://$HOST:81/audio.pcm" -o "$PCM" &
STREAM=$!
sleep 8

# Read the counter while the stream is still up: tearing a recording down
# always drops the DMA batch in flight, which says nothing about steady state.
DROPPED=$(curl -s -m 8 "${AUTH[@]}" "http://$HOST/status" | grep -oE '"audioDroppedSamples":[0-9]+' | cut -d: -f2)
BYTES=$(stat -c %s "$PCM")
kill "$STREAM" 2>/dev/null || true
wait "$STREAM" 2>/dev/null || true

RATE=$((BYTES / 8 / 1024))
echo "  $BYTES bytes => ${RATE} kB/s (expect ~94)"
[ "$RATE" -ge 85 ] || { echo "    ^ THROUGHPUT REGRESSED" >&2; fail=1; }
echo "  audioDroppedSamples=$DROPPED (expect 0 mid-stream)"
[ "${DROPPED:-1}" = "0" ] || { echo "    ^ SAMPLES DROPPED WHILE STREAMING" >&2; fail=1; }

echo "== recording slot released =="
sleep 2
SLOT=$(curl -s -m 8 "${AUTH[@]}" "http://$HOST/status" | grep -oE '"audioStreaming":(true|false)' | cut -d: -f2)
echo "  audioStreaming=$SLOT (expect false)"
[ "$SLOT" = "false" ] || { echo "    ^ SLOT STILL HELD" >&2; fail=1; }

echo "== telemetry rate =="
if command -v node >/dev/null 2>&1; then
  node "$(dirname "${BASH_SOURCE[0]}")/telemetry-rate.js" "$HOST" 10 || fail=1
else
  echo "  skipped (node not on PATH)"
fi

[ "$fail" -eq 0 ] && echo "ALL CHECKS PASSED" || echo "SOME CHECKS FAILED" >&2
exit "$fail"
