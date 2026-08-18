# AGENTS.md

ESP32-C3 room sensor firmware: mmWave radar (HLK-LD2412) plus microphone
(MAX9814), exposing a token-authenticated LAN API, a WebSocket telemetry
stream, lossless PCM audio, and Gotify push alerts. PlatformIO + Arduino.

Read [docs/architecture.md](docs/architecture.md) before changing anything
structural — it records *why* the boundaries are where they are.
[CONTRIBUTING.md](CONTRIBUTING.md) holds the design rules; follow them rather
than re-deriving them.

## Commands

Always pass `-e`. The two environments target incompatible platforms and
PlatformIO will not pick one for you.

| Task | Command |
|---|---|
| Full gate — tests, cppcheck, firmware build | `./check.sh` · `./check.ps1` |
| Unit tests only | `pio test -e native` |
| Firmware build only | `pio run -e esp32-c3-supermini` |
| Gate then flash (Windows host) | `./deploy.ps1 [-Port COM3]` |

CI runs exactly these two gate scripts, on Linux and Windows. A green local
gate means a green pipeline.

If `pio` is not on `PATH`, use `~/.platformio/penv/bin/pio`.

`pio check` does **not** work on Linux here — the bundled analyzer needs the
obsolete `libpcre3`. The gate scripts run a pinned cppcheck package instead,
suppressing findings from vendored code under `.pio/libdeps`.

## Conventions that are easy to violate

- **`lib/` is pure logic; `src/` is glue.** No `Arduino.h`, no hardware, no
  heap, and no `millis()` inside `lib/` — time arrives as a `nowMs` parameter.
  Every `lib/` class has a matching `test/test_<snake_case>/`; currently 10 of
  10. Adding one without a test breaks the project's core convention.
- **Nothing reached from an AsyncTCP callback may block.** Handlers set a flag
  and the sensor loop does the work — see `requestCalibration()` /
  `pollCalibrationRequest()`. The rationale is in the architecture doc under
  "Crossing the task boundary".
- **Never hold an `AsyncWebSocketClient*` outside the callback that gave it to
  you.** The library releases its client lock before returning the pointer and
  disconnects are processed on another task. Use the id-based API.
- **`GET /status` and the WebSocket `telemetry` frame share one payload.**
  Both go through `takeSensorSnapshot()` / `appendSensorFields()` in
  [src/sensor_snapshot.h](src/sensor_snapshot.h). Add a reported field there,
  not in `api.cpp` or `ws.cpp` — rendering them separately is how `pushLost`
  ended up on one and not the other. The struct is `packed` because `ws.cpp`
  hashes it for change detection, and `uptimeMs` is deliberately outside it so
  a ticking clock never counts as a change.
- **State touched by more than one task must be atomic or mutex-guarded.**
  Sensor accessors already return guarded copies, which is why a handler may
  call them. `net.cpp`'s push counters are `std::atomic` because the delivery
  worker, the API task and the sensor loop all touch them.
- **Serial output can stall the sensor loop.** USB CDC writes block for up to
  ~2s when the host is not draining the port, and that once starved the loop
  to 0.4Hz. `setup()` calls `Serial.setTxTimeoutMs(0)` so diagnostics drop
  instead of blocking — do not remove it, and do not add serial output to a
  hot path.
- **`GET /audio.pcm` lives on a synchronous server on port 81, deliberately.**
  `WebServer.h` and `ESPAsyncWebServer.h` both define `HTTP_GET`, so they must
  stay in separate translation units. Do not fold it into the async server:
  that was implemented, measured at 2.6 kB/s against 95.5 kB/s, and reverted
  ([#4](https://github.com/patbaumgartner/esp32-room-watchdog/issues/4)).
- **`src/secrets.h` is gitignored.** Copy `src/secrets.h.example` to compile.
  Never print, echo, or commit its contents.
- **The build uses the `huge_app.csv` partition layout** (no OTA). Changing the
  layout means a full flash, not an incremental one.
- **Tuning constants belong in [src/config.h](src/config.h)**, credentials in
  `src/secrets.h`.

## Measure network behaviour, do not reason about it

Analytical arguments about TCP, AsyncTCP scheduling, and throughput have
produced confidently wrong conclusions in this repo more than once — including
a shipped fix for a problem that did not exist. If a change touches the audio
stream, the WebSocket, or push delivery, flash it and measure. The
[verify-on-device](.github/skills/verify-on-device/SKILL.md) skill bundles the
probe:

- `audioDroppedSamples` in `GET /status` must stay `0` across a recording.
- Audio throughput should be ~94 kB/s (48 kHz, 16-bit, mono).
- The recording slot should free within ~50 ms of a client disconnecting.

Two traps that produced false results here:

- **Sample counters while the stream is up, not after tearing it down.**
  Aborting a recording always drops the DMA batch in flight (512 samples),
  which says nothing about steady-state quality.
- **Kill stray clients first** (`pkill -f audio.pcm`). A forgotten background
  `curl` still streaming makes the next request return `409`, which looks
  exactly like a leaked recording slot.

`POST /calibrate` starts a real ~2 minute radar background calibration that
must run with the room empty. Never call it with a valid token just to
exercise authentication — use an invalid one.

## Hardware access

The board is flashed from the Windows host; WSL2 cannot see the USB serial
port. `deploy.ps1` handles the `\\wsl.localhost\...` path. Setup and USB
forwarding options are in [docs/setup.md](docs/setup.md#wsl2), flashing in
[docs/flashing.md](docs/flashing.md).

A companion Android client lives in
[esp32-room-watchdog-android](https://github.com/patbaumgartner/esp32-room-watchdog-android);
changes to the API surface or the WebSocket message schema affect it.

## Changes worth recording

Add a `CHANGELOG.md` entry for anything a user would notice — HTTP API shape,
WebSocket message fields, `src/config.h` defaults, or wiring. The project has
no tagged release yet, so do not label changes as breaking.
