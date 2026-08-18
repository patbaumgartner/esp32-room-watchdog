# ESP32 Room Watchdog

[![CI](https://github.com/patbaumgartner/esp32-room-watchdog/actions/workflows/ci.yml/badge.svg)](https://github.com/patbaumgartner/esp32-room-watchdog/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange)](https://platformio.org/)

A room watchdog built around an ESP32-C3 SuperMini: it combines a 24GHz mmWave
radar and a microphone to detect people and sounds nearby, reports how far away
the person is, streams live telemetry over a WebSocket, and pushes alerts to
your phone via a [Gotify](https://gotify.net) server — fully self-hosted, no
cloud account, no third-party service in the loop.

## Features

- **Person detection with distance** — HLK-LD2412 mmWave radar, debounced edge
  detection ("Person detected at 1.5m (moving)" / "Presence cleared")
- **Movement tracking** — "Person moved to 2.7m" when the person shifts more
  than a configurable distance
- **Sound detection** — MAX9814 mic, peak-to-peak loudness with threshold +
  cooldown so a noisy room doesn't flood your phone
- **Live telemetry over WebSocket** — the full sensor state at up to 10Hz for a
  dashboard or native app, on the same authenticated port as the REST API
- **Alerts that survive the network** — presence, sound and boot events are
  queued on the device and retried by a worker task, so detection never waits
  for a slow or unreachable push server
- **Lossless audio streaming** — authenticated 48kHz mono PCM over WiFi,
  recorded as WAV on another computer without device-side storage
- **Radar tuning + calibration** — per-gate sensitivity applied at boot via
  the LD2412 command protocol; background calibration on demand (BOOT button
  or HTTP API) to cancel out static room clutter
- **Zero-config discovery** — the node announces itself as `watchdog.local`
  over mDNS, so no client needs a hardcoded IP
- **Testable core** — all decision logic is hardware-independent and covered by
  native unit tests; CI runs them on every push

## Hardware

| Component | Model | Role |
|---|---|---|
| MCU | ESP32-C3 SuperMini (native USB-C) | WiFi + control |
| Presence sensor | Hi-Link HLK-LD2412 | 24GHz mmWave radar, ±60°, up to 9m |
| Microphone | MAX9814 | AGC mic amplifier (analog out) |

Full wiring, pin map, cable color legend, and datasheets: [docs/hardware.md](docs/hardware.md)

## Quick start

1. **Wire the sensors** — see [docs/hardware.md](docs/hardware.md).
2. **Install the toolchain** — see [docs/setup.md](docs/setup.md)
   (PlatformIO; no Arduino IDE needed).
3. **Configure credentials, your Gotify server, and an API key:**

   ```bash
   cp src/secrets.h.example src/secrets.h
   # edit src/secrets.h: WiFi SSID/password, Gotify URL + app token, API token
   ```

4. **Create a Gotify application** (Gotify UI → Apps → Create application) and
   put its token into `GOTIFY_TOKEN`; install the
   [Gotify app](https://gotify.net/docs/install) on your phone.
5. **Flash:**

   ```powershell
   ./deploy.ps1              # tests + build + upload (default port COM3)
   ./deploy.ps1 -Port COM5   # different port
   ```

   On Linux or macOS:

   ```bash
   ./check.sh                                                  # tests + analysis + build
   pio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0
   ```

   Or use the PlatformIO sidebar. Details and troubleshooting:
   [docs/flashing.md](docs/flashing.md)

## Configuration

All tuning knobs live in [src/config.h](src/config.h):

| Constant | Default | Meaning |
|---|---|---|
| `SOUND_PP_THRESHOLD` | `1600` | Min peak-to-peak ADC swing to count as "sound" (idle ≈ 300) |
| `SOUND_NOTIFY_COOLDOWN_MS` | `15000` | Quiet period after a sound notification |
| `SOUND_SAMPLE_WINDOW_MS` | `50` | Mic sampling window length |
| `AUDIO_SAMPLE_RATE_HZ` | `48000` | Hardware-timed ADC sample rate for detection and streaming |
| `AUDIO_STREAM_BUFFER_SAMPLES` | `8192` | Network backpressure buffer (~170ms at 48kHz) |
| `PRESENCE_DEBOUNCE_MS` | `2000` | Radar state must hold this long before notifying |
| `DISTANCE_DELTA_CM` | `100` | Movement needed before a "Person moved to X" update |
| `DISTANCE_UPDATE_MIN_INTERVAL_MS` | `10000` | Min gap between movement updates |
| `RADAR_MIN_GATE` / `RADAR_MAX_GATE` | `1` / `8` | Detection range in 0.75m gates (8 = 6m) |
| `RADAR_UNMANNED_SECONDS` | `5` | Radar-side hold time before reporting "unmanned" |
| `RADAR_MOTION_SENSITIVITY` / `RADAR_STATIC_SENSITIVITY` | per-gate arrays | Energy thresholds per gate (0–100, higher = less sensitive) |
| `WS_MIN_PUSH_INTERVAL_MS` | `50` | Fastest telemetry frame rate on the socket |
| `WS_HEARTBEAT_MS` | `2000` | Frame sent even when nothing changed |
| `WS_COMMAND_MAX` | `32` | Longest client command accepted, refused before it is copied |
| `GOTIFY_QUEUE_DEPTH` | `8` | Pending pushes held on the device; the oldest is discarded when full |
| `GOTIFY_MAX_ATTEMPTS` | `3` | Delivery attempts before a message is given up on |
| `MDNS_HOSTNAME` | `watchdog` | Announced as `<name>.local` |
| `TRUST_PROXY_HEADERS` | `false` | Log `X-Forwarded-For` instead of the peer address (reverse proxy only) |

WiFi credentials, the Gotify server URL + app token (`GOTIFY_URL` /
`GOTIFY_TOKEN`), and the HTTP API key (`API_TOKEN`) live in the gitignored
`src/secrets.h`. `API_TOKEN` guards live occupancy, the telemetry socket and
the microphone stream, so generate it randomly — the build rejects anything
shorter than 16 characters.

## HTTP API

The node answers at `watchdog.local` (mDNS) as well as at its IP. Every
endpoint requires the token, supplied as `Authorization: Bearer <API_TOKEN>`
or `X-Api-Key: <API_TOKEN>`; anything else gets a `401`.

| Endpoint | Port | Response |
|---|---|---|
| `GET /status` | 80 | Live JSON: sensor values, audio and push health, uptime |
| `POST /calibrate` | 80 | `202` — starts radar background calibration in 10s (~2 min; leave the room) |
| `GET /ws` | 80 | WebSocket: live telemetry and events (see below) |
| `GET /audio.pcm` | 81 | Continuous 48kHz signed 16-bit little-endian mono PCM stream |

```bash
curl -H "Authorization: Bearer $WATCHDOG_API_TOKEN" http://watchdog.local/status
curl -X POST -H "Authorization: Bearer $WATCHDOG_API_TOKEN" http://watchdog.local/calibrate
```

The PCM stream keeps its own port because it holds its socket for the whole
recording — the one thing an async handler must never do.

The traffic is plain HTTP: keep the device on a trusted LAN and never
port-forward it. To reach it from outside, put a TLS-terminating reverse proxy
in front (`https://` / `wss://`) rather than exposing the node — see
[SECURITY.md](SECURITY.md) for the full threat model and a proxy example.

Calibration can also be triggered on the device by holding the BOOT button
for ~1s. The result is persisted in the radar module's flash.

## Live telemetry (WebSocket)

Notifications and live data are deliberately split:

- **Gotify** answers *"should I care right now?"* — presence, sound, boot. It
  is queued on the device and retried, so an event survives a slow server, a
  WiFi blip, or a phone that is asleep or away from home.
- **The WebSocket** answers *"what is happening this second?"* — the full
  sensor state at up to 10Hz. Frames are disposable: if nobody is listening or
  the socket is backed up, they are dropped rather than buffered.

That is why movement updates ("Person moved to 2.7m") no longer reach your
phone. They stream continuously over the socket instead, where they are useful
rather than noisy.

Connect to `ws://watchdog.local/ws` with the same token in a header. Browsers
cannot set headers on a WebSocket handshake — this endpoint is meant for native
apps and Node clients:

```javascript
import WebSocket from 'ws';

const socket = new WebSocket('ws://watchdog.local/ws', {
  headers: { Authorization: `Bearer ${process.env.WATCHDOG_API_TOKEN}` },
});
socket.on('message', (data) => console.log(JSON.parse(data)));
socket.send('calibrate'); // the only command the node accepts
```

Server messages are JSON with a `type` field:

| `type` | When | Payload |
|---|---|---|
| `hello` | Once, on connect | Host name, audio sample rate and port, heartbeat interval, radar gate range |
| `telemetry` | On change, and at least every `WS_HEARTBEAT_MS` | Presence, target state, distances, energies, mic levels, audio and push health, uptime |
| `event` | On a detector edge | `event` (`boot`, `presence`, `cleared`, `moved`, `sound`, `calibration`) and the same human-readable `message` Gotify would show |

Telemetry is sent only when something actually changed, and never faster than
`WS_MIN_PUSH_INTERVAL_MS`. The heartbeat guarantees a frame even in a still
room, so a client can tell "quiet" from "dead link".

**One client at a time.** A second authenticated connection replaces the first
rather than being refused — an app killed on a phone leaves a socket that TCP
only reaps minutes later, and being locked out of your own sensor until then
would be worse.

## Audio recording

The node serves one lossless raw PCM stream at `GET /audio.pcm` on port 81,
signed 16-bit little-endian mono at 48kHz. Install
[FFmpeg](https://ffmpeg.org/), then record until you press Ctrl+C:

```bash
export WATCHDOG_API_TOKEN='<API_TOKEN>'   # omit to be prompted
./record.sh                          # defaults to watchdog.local:81
./record.sh 192.168.1.42 room.wav
```

```powershell
$env:WATCHDOG_API_TOKEN = '<API_TOKEN>'
./record.ps1
./record.ps1 -Device 192.168.1.42 -Output room.wav
```

The ESP32 stores no recording. `GET /status` exposes `audioStreaming` and
`audioDroppedSamples`; the latter should remain zero. A nonzero value means
the receiving computer or WiFi could not drain the 170ms buffer in time.
Only one recording client is supported; stopping one and starting another
works immediately, because the device releases the slot within ~50ms of the
client going away.

## Development

```bash
./check.sh                                # unit tests + cppcheck + firmware build
pio test -e native                        # unit tests only
pio run -e esp32-c3-supermini             # build only
pio device monitor -e esp32-c3-supermini  # serial log @ 115200 baud
```

`pio check` is deliberately not used: pioarduino's bundled Linux analyzer
needs the obsolete `libpcre3` runtime and fails. `check.sh` and `check.ps1`
run a pinned, cross-platform cppcheck package against PlatformIO's compilation
database instead, which is also what CI runs.

Windows equivalents: `./check.ps1` and `./deploy.ps1` (check + flash). The two
PlatformIO environments target incompatible platforms, so always pass `-e`.

### Architecture

Decision logic is separated from hardware I/O so it can be unit-tested on the
host — no board needed. Full design rationale, data-flow diagram, and
notification semantics: [docs/architecture.md](docs/architecture.md)

```
esp32-room-watchdog/
├── lib/
│   ├── audio/                # ADC bias removal + PCM conversion (unit-tested)
│   ├── auth/                 # API token comparison (unit-tested)
│   ├── detectors/            # pure logic, no Arduino deps (unit-tested)
│   │   ├── LevelWindow.h     #   min/max accumulator for mic sampling windows
│   │   ├── SoundDetector.h   #   threshold + cooldown decisions
│   │   ├── PresenceMonitor.h #   debounced presence edge detection
│   │   └── DistanceTracker.h #   movement-delta decisions
│   ├── ld2412/               # LD2412 UART protocol (unit-tested)
│   │   ├── Ld2412Parser.h    #   data frame decoding (distance/energy)
│   │   └── Ld2412Commands.h  #   command frame builders (tuning, calibration)
│   ├── notify/               # bounded push queue, drop-oldest (unit-tested)
│   └── telemetry/            # socket send-on-change + heartbeat (unit-tested)
├── src/                      # hardware/network glue (runs on the ESP32)
│   ├── main.cpp              #   setup/loop composition only
│   ├── config.h              #   pins + tuning constants
│   ├── mic.cpp / .h          #   ADC DMA, level windows + PCM ring buffer
│   ├── audio_stream.cpp / .h #   PCM response writer
│   ├── audio_api.cpp / .h    #   synchronous server for GET /audio.pcm
│   ├── radar.cpp / .h        #   UART parsing, tuning, calibration
│   ├── notifications.cpp / .h#   detector events → alert vs live routing
│   ├── api.cpp / .h          #   async REST API (authentication + handlers)
│   ├── ws.cpp / .h           #   WebSocket telemetry + events
│   ├── calibration_button.cpp#   BOOT button → calibration
│   ├── status_log.cpp / .h   #   1Hz serial diagnostics
│   ├── net.cpp / net.h       #   WiFi, mDNS, queued Gotify delivery (TLS)
│   ├── json_escape.cpp / .h  #   one JSON string escaper for both renderers
│   ├── certs.h               #   Let's Encrypt root CA for TLS validation
│   └── secrets.h.example     #   template for credentials (gitignored copy)
├── test/                     # native Unity tests for lib/
├── docs/                     # hardware, setup, flashing, architecture
├── .github/workflows/ci.yml  # tests + cppcheck + firmware build
├── check.sh / check.ps1      # local quality gate
├── record.sh / record.ps1    # record the live audio stream to WAV
└── deploy.ps1                # check + flash
```

CI runs on pushes to `main`, on pull requests, and weekly: native unit tests,
cppcheck, and a full firmware build, with `firmware.bin` kept as an artifact.
Actions are pinned to commit SHAs and kept current by Dependabot.

## Status

- [x] Hardware wired (LD2412 presence/UART + MAX9814 mic)
- [x] WiFi connectivity + boot notification
- [x] Sound detection with threshold/cooldown → push notification
- [x] Lossless 48kHz PCM audio streaming over WiFi
- [x] Person detection with debounce → push notification
- [x] Unit tests + CI pipeline
- [x] LD2412 UART frame parser — presence notifications include distance
- [x] LD2412 command protocol — per-gate sensitivity tuning + background calibration
- [x] HTTP API — live status + remote calibration
- [x] WebSocket telemetry + mDNS discovery — alerts and live data split
- [ ] Enclosure / final assembly

## Contributing

Issues and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).
User-visible changes are recorded in [CHANGELOG.md](CHANGELOG.md); security
reports go through [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE)

The firmware links two third-party libraries under the **LGPL-3.0**:
[ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) and
[AsyncTCP](https://github.com/ESP32Async/AsyncTCP). Building from source keeps
you clear of their relinking requirement; if you redistribute a compiled
`firmware.bin`, that obligation is yours to honour. Everything in this
repository is MIT.
