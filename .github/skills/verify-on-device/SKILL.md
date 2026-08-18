---
name: verify-on-device
description: "Flash the firmware to the ESP32-C3 and verify it on real hardware. Use after changing anything that touches the network stack, the audio stream, the WebSocket, push delivery, or the HTTP API — and whenever a claim about throughput, TCP behaviour, or socket timing needs proving rather than reasoning. Covers flashing from the Windows host over WSL2, the API smoke test, the authentication matrix, and audio throughput measurement."
argument-hint: 'Optional: what changed, e.g. "websocket telemetry" or "audio stream"'
---

# Verify on Device

Analytical arguments about TCP, AsyncTCP scheduling, and throughput have
produced confidently wrong conclusions in this repo — including a shipped fix
for a problem that did not exist. Anything touching the network path gets
flashed and measured.

## When to use

- A change touches `src/ws.cpp`, `src/audio_*.cpp`, `src/net.cpp`, or `src/api.cpp`
- You are about to state a throughput, latency, or socket-timing number
- Before closing an issue whose claim rests on runtime behaviour

Pure `lib/` logic changes do not need this — `./check.sh` covers them.

## Procedure

### 1. Gate and flash

```bash
./check.sh
```

Then flash from the Windows host — WSL2 cannot see the serial port:

```bash
cd /mnt/c && powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
  "& '\\\\wsl.localhost\\Ubuntu\\<path-to-repo>\\deploy.ps1' -Port COM3"
```

`deploy.ps1` reruns the gate inside WSL, then flashes. To skip the rebuild use
`-SkipChecks`. After a partition-layout change, flash
`.pio/build/esp32-c3-supermini/firmware.factory.bin` at `0x0` instead.

Wait ~12s for WiFi and mDNS to come up before probing.

### 2. Probe

Run [scripts/probe.sh](./scripts/probe.sh). It covers the API smoke test, the
full authentication matrix, and a 10-second throughput measurement.

```bash
bash .github/skills/verify-on-device/scripts/probe.sh
```

Write multi-step device checks into a script file rather than chaining them
inline — this shell mangles `; echo` after a quoted URL into a literal `\;`,
which produces phantom `404`s that look like firmware bugs.

### 3. Read the results

| Signal                                       | Expected                |
| -------------------------------------------- | ----------------------- |
| `audioDroppedSamples` during a recording     | `0`                     |
| Audio throughput                             | ~94 kB/s                |
| Recording slot freed after client disconnect | < ~50 ms                |
| Every endpoint, no/wrong token               | `401`                   |
| `/ws` handshake, no token                    | `401`; with token `101` |

A dropped-sample count above zero on a LAN recording means the change
regressed the stream — investigate before committing.

## Cautions

- **Never call `POST /calibrate` with a valid token to test auth.** It starts a
  real ~2 minute radar calibration and rewrites the background baseline, which
  degrades detection if the room is not empty. Use an invalid token.
- **Never echo the API token.** Read it from the gitignored `src/secrets.h`
  with `grep` inside the script; do not print it.
- A stray background `curl` still streaming will make the next request return
  `409` and look like a leaked slot. `pkill -f audio.pcm` before measuring.

## WebSocket checks

`curl` cannot frame WebSocket traffic. Node is available and needs no
dependencies — `net` plus `crypto` is enough for a handshake and a text-frame
read. Browsers cannot set headers on a WS handshake, so this endpoint is for
native and Node clients only.
