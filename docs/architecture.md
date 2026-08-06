# Architecture

How the firmware is structured and why. For wiring see
[hardware.md](hardware.md); for build/flash see [flashing.md](flashing.md).

## Design principle

**Decision logic is separated from I/O.** Everything that decides *whether* to
notify lives in [`lib/detectors/`](../lib/detectors/) as header-only, pure C++
classes with no Arduino dependency — time is injected as a parameter, never
read inside. That makes the interesting behavior unit-testable on the host
(`pio test -e native`) without a board. Everything that touches hardware or
the network stays in [`src/`](../src/) as thin glue.

```mermaid
flowchart LR
    subgraph hardware [Hardware]
        MIC[MAX9814 mic<br/>GPIO0 / ADC]
        RADAR[LD2412 radar<br/>GPIO5 OUT + UART]
        BTN[BOOT button<br/>GPIO9]
    end

    subgraph src [src/ — glue, runs on ESP32]
        LOOP[main.cpp<br/>composition only]
        MICG[mic.cpp<br/>ADC sampling]
        RADG[radar.cpp<br/>UART parsing + tuning]
        NOTIF[notifications.cpp<br/>events → push]
        API[api.cpp<br/>HTTP /status + /calibrate]
        CBTN[calibration_button.cpp<br/>hold → calibrate]
        SLOG[status_log.cpp<br/>serial diagnostics]
        NET[net.cpp<br/>WiFi + HTTP]
    end

    subgraph lib [lib/ — pure logic, unit-tested]
        LW[LevelWindow<br/>min/max per window]
        SD[SoundDetector<br/>threshold + cooldown]
        PM[PresenceMonitor<br/>debounce + edges]
        DT[DistanceTracker<br/>movement deltas]
        LP[Ld2412Parser<br/>data frame decoding]
        LC[Ld2412Commands<br/>command frame builders]
    end

    MIC -->|analogRead| MICG --> LW
    RADAR -->|digitalRead + UART| RADG --> LP
    RADG --> LC
    BTN --> CBTN
    LOOP --> MICG
    LOOP --> RADG
    LOOP --> NOTIF
    LOOP --> API
    LOOP --> CBTN
    LOOP --> SLOG
    API --> NOTIF
    CBTN --> NOTIF
    NOTIF --> SD
    NOTIF --> PM
    NOTIF --> DT
    NOTIF -->|pushGotify| NET -->|HTTPS POST| GOTIFY[Gotify server]
```

## Loop lifecycle

`setup()` runs once after boot/reset: serial, mic, radar and button init,
radar tuning (per-gate sensitivities via the LD2412 command protocol), WiFi
connect, HTTP API start, and a "Sensor node online" push. Then `loop()`
repeats forever:

1. **`micSampleWindow()`** — busy-samples the ADC for `SOUND_SAMPLE_WINDOW_MS`
   (50ms) into a `LevelWindow`. Windows run back-to-back, so a short clap
   can't fall into a gap. The last window is kept for the HTTP API.
2. **`radarPoll()`** — feeds pending LD2412 UART bytes into `Ld2412Parser`
   for distance/energy reports.
3. **`calibrationButtonPoll()`** — BOOT button held ~1s starts radar
   background calibration (also available via `POST /calibrate`).
4. **`apiPoll()`** — serves pending HTTP requests (`GET /status`,
   `POST /calibrate`).
5. **`notifyPresenceChanges()`** — feeds the radar pin into `PresenceMonitor`;
   pushes "Person detected at X" / "Presence cleared" on debounced edges.
6. **`notifyMovement()`** — while present, `DistanceTracker` pushes "Person
   moved to X" when the distance shifts beyond `DISTANCE_DELTA_CM`.
7. **`notifyLoudSounds()`** — feeds the window's peak-to-peak into
   `SoundDetector`; pushes "Sound detected" above threshold, then cools down.
8. **`logStatusEverySecond()`** — one diagnostic line per second on serial.

One pass takes ~50ms (the mic window); the only blocking extras are the rare
HTTP pushes.

## Notification semantics

Both detectors share the same delivery contract:

- **Events repeat until confirmed.** A detector keeps reporting its event
  until the caller confirms delivery via `notificationSent()`. A failed HTTP
  push (WiFi drop, server error) is therefore retried on the next loop pass,
  throttled by GOTIFY_RETRY_BACKOFF_MS so a failing server isn't hammered —
  no event is silently lost.
- **Debounce (presence):** the radar state must hold for
  `PRESENCE_DEBOUNCE_MS` before an edge counts; flicker resets the timer.
- **Cooldown (sound):** after a *delivered* sound notification, further sound
  events are suppressed for `SOUND_NOTIFY_COOLDOWN_MS`.

## Signal reference (mic)

12-bit ADC, MAX9814 biased at ~1.25V:

| Condition | Typical reading |
|---|---|
| Silent room | micMin ≈ 2600, micMax ≈ 2800, micPP ≈ 200–300 |
| Loud clap nearby | micPP ≈ 1500–4095 |
| Mic disconnected/floating | micMin ≈ 0, micMax ≈ 300 (WiFi noise pickup) |

The last row is the diagnostic signature for a wiring fault: a healthy mic
always shows the DC bias (~2600), a floating pin bounces near ground.

## Radar tuning and calibration

The LD2412 command protocol lives in `lib/ld2412/Ld2412Commands.h` (pure
frame builders, byte-for-byte unit-tested against the datasheet examples)
with the send/sequence logic in `radar.cpp`:

- **Boot tuning** — `radarApplyTuning()` writes gate range, unmanned duration
  and the per-gate motion/static sensitivity arrays from `config.h` on every
  boot (the module persists them in flash anyway; rewriting keeps the source
  of truth in git).
- **Background calibration** — `startCalibration()` (notifications module)
  triggers the module's background correction: starts 10s after the command,
  takes ~2 minutes, and must run with the room empty. Reachable from the
  BOOT button (hold ~1s) and `POST /calibrate`; both share the same entry
  point, which also pushes a "leave the room" warning via Gotify.

## HTTP API

`api.cpp` runs a `WebServer` on port 80, polled from the loop (no extra
task). `GET /status` is read-only and unauthenticated; `POST /calibrate`
requires the `X-Api-Key` header to match `API_TOKEN` from secrets.h. The API
reuses the modules' public accessors (`radarReport()`, `micLastWindow()`)
rather than owning any state.

## Extension points

- **More LD2412 commands** — add builders to `Ld2412Commands.h` (e.g.
  engineering mode for per-gate energy readouts) following the existing
  pattern: pure builder + byte-level Unity test against the protocol PDF in
  [datasheets/HLK-2412](datasheets/HLK-2412/).
- **New detectors** — follow the pattern: pure class in `lib/detectors/`,
  time injected, `notificationSent()` confirmation, Unity test in `test/`.
- **New API endpoints** — register handlers in `api.cpp`; keep them thin and
  read module state via accessors.
