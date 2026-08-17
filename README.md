# ESP32 Room Watchdog

[![CI](https://github.com/patbaumgartner/esp32-room-watchdog/actions/workflows/ci.yml/badge.svg)](https://github.com/patbaumgartner/esp32-room-watchdog/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange)](https://platformio.org/)

A room watchdog built around an ESP32-C3 SuperMini: it combines a 24GHz mmWave
radar and a microphone to detect people and sounds nearby, reports how far away
the person is, and pushes notifications to your phone via a
[Gotify](https://gotify.net) server — fully self-hosted, no cloud account, no
third-party service in the loop.

## Features

- **Person detection with distance** — HLK-LD2412 mmWave radar, debounced edge
  detection ("Person detected at 1.5m (moving)" / "Presence cleared")
- **Movement tracking** — "Person moved to 2.7m" when the person shifts more
  than a configurable distance
- **Sound detection** — MAX9814 mic, peak-to-peak loudness with threshold +
  cooldown so a noisy room doesn't flood your phone
- **Lossless audio streaming** — authenticated 48kHz mono PCM over WiFi,
  recorded as WAV on another computer without device-side storage
- **Radar tuning + calibration** — per-gate sensitivity applied at boot via
  the LD2412 command protocol; background calibration on demand (BOOT button
  or HTTP API) to cancel out static room clutter
- **HTTP API** — token-authenticated `GET /status` live sensor JSON and
  `POST /calibrate` to recalibrate remotely
- **Push notifications** — JSON POST to a self-hosted [Gotify](https://gotify.net)
  server over HTTPS (TLS validated against the Let's Encrypt root); failed
  pushes back off and retry so events aren't lost
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

WiFi credentials, the Gotify server URL + app token (`GOTIFY_URL` /
`GOTIFY_TOKEN`), and the HTTP API key (`API_TOKEN`) live in the gitignored
`src/secrets.h`. `API_TOKEN` guards live occupancy and the microphone stream,
so generate it randomly — the build rejects anything shorter than 16
characters.

## HTTP API

The node serves a small LAN API on port 80. Every endpoint requires the token,
supplied as `Authorization: Bearer <API_TOKEN>` or `X-Api-Key: <API_TOKEN>`;
anything else gets a `401`.

| Endpoint | Response |
|---|---|
| `GET /status` | Live JSON: sensor values, audio streaming/drop state, uptime |
| `POST /calibrate` | `202` — starts radar background calibration in 10s (~2 min; leave the room) |
| `GET /audio.pcm` | Continuous 48kHz signed 16-bit little-endian mono PCM stream |

```bash
curl -H "Authorization: Bearer $WATCHDOG_API_TOKEN" http://<device-ip>/status
curl -X POST -H "Authorization: Bearer $WATCHDOG_API_TOKEN" http://<device-ip>/calibrate
```

The traffic is plain HTTP: keep the device on a trusted LAN and never
port-forward it. See [SECURITY.md](SECURITY.md) for the full threat model.

Calibration can also be triggered on the device by holding the BOOT button
for ~1s. The result is persisted in the radar module's flash.

## Audio recording

The node serves one lossless raw PCM stream at `GET /audio.pcm`, signed 16-bit
little-endian mono at 48kHz. Install [FFmpeg](https://ffmpeg.org/), then
record until you press Ctrl+C:

```bash
export WATCHDOG_API_TOKEN='<API_TOKEN>'   # omit to be prompted
./record.sh 192.168.1.42
./record.sh 192.168.1.42 room.wav
```

```powershell
$env:WATCHDOG_API_TOKEN = '<API_TOKEN>'
./record.ps1 -DeviceIp 192.168.1.42
./record.ps1 -DeviceIp 192.168.1.42 -Output room.wav
```

The ESP32 stores no recording. `GET /status` exposes `audioStreaming` and
`audioDroppedSamples`; the latter should remain zero. A nonzero value means
the receiving computer or WiFi could not drain the 170ms buffer in time.
Only one recording client is supported.

## Development

```bash
./check.sh                       # unit tests + cppcheck + firmware build
pio test -e native               # unit tests only
pio check -e esp32-c3-supermini  # static analysis only
pio run -e esp32-c3-supermini    # build only
pio device monitor               # serial log @ 115200 baud
```

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
│   └── ld2412/               # LD2412 UART protocol (unit-tested)
│       ├── Ld2412Parser.h    #   data frame decoding (distance/energy)
│       └── Ld2412Commands.h  #   command frame builders (tuning, calibration)
├── src/                      # hardware/network glue (runs on the ESP32)
│   ├── main.cpp              #   setup/loop composition only
│   ├── config.h              #   pins + tuning constants
│   ├── mic.cpp / .h          #   ADC DMA, level windows + PCM ring buffer
│   ├── audio_stream.cpp / .h #   PCM response writer for GET /audio.pcm
│   ├── radar.cpp / .h        #   UART parsing, tuning, calibration
│   ├── notifications.cpp / .h#   detector events → push messages
│   ├── api.cpp / .h          #   HTTP API (authentication + handlers)
│   ├── calibration_button.cpp#   BOOT button → calibration
│   ├── status_log.cpp / .h   #   1Hz serial diagnostics
│   ├── net.cpp / net.h       #   WiFi connect + Gotify push (TLS)
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
- [ ] Enclosure / final assembly

## Contributing

Issues and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).
User-visible changes are recorded in [CHANGELOG.md](CHANGELOG.md); security
reports go through [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE)
