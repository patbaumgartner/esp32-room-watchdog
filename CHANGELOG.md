# Changelog

Notable changes to the firmware and its HTTP API. This project has no tagged
releases yet — flash `main`. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- **Live telemetry over WebSocket at `GET /ws`.** The full sensor state streams
  at up to 10Hz to one authenticated client, with `hello`, `telemetry` and
  `event` JSON messages, and `calibrate` accepted from the client. Frames are
  sent on change (never faster than `WS_MIN_PUSH_INTERVAL_MS`) with a forced
  heartbeat every `WS_HEARTBEAT_MS`, so a client can tell a quiet room from a
  dead link.
- **mDNS.** The node announces itself as `watchdog.local` with its HTTP and
  WebSocket services, so no client needs a hardcoded IP. Configurable via
  `MDNS_HOSTNAME`.
- **Queued push delivery.** Alerts go into a bounded `NotificationQueue`
  drained by a worker task, which retries up to `GOTIFY_MAX_ATTEMPTS` times.
  Detection no longer waits for the network at all.
- `lib/telemetry/TelemetryGate.h` and `lib/notify/NotificationQueue.h` — the
  socket's send policy and the push queue are pure logic with native tests,
  like the rest of the decision layer. 17 new test cases.
- `GET /status` reports `telemetryClient`, `pushBackingOff` and `pushLost`.
- `TRUST_PROXY_HEADERS` logs the `X-Forwarded-For` client when the node sits
  behind a reverse proxy. Logging only; never used for authentication.
- `SECURITY.md` documents the reverse-proxy setup for reaching the node over
  `https://` / `wss://`, since the device itself terminates no TLS.
- `check.sh` and `record.sh` for Linux and macOS.
- `lib/auth/ApiToken.h` — the token comparison moved out of `api.cpp` so it is
  covered by native tests like the rest of the decision logic.
- `pio check` (cppcheck) in the local quality gate and in CI.
- `SECURITY.md` with the threat model, `CODE_OF_CONDUCT.md`, issue and pull
  request templates.
- CI builds weekly, uploads `firmware.bin` as an artifact, and runs with
  least-privilege permissions, a concurrency group, and a job timeout.

### Changed

- **Breaking — `GET /audio.pcm` moved to port 81.** The REST API and WebSocket
  are now served by `ESPAsyncWebServer` on port 80, but a recording holds its
  socket for minutes, which an async handler must never do. It keeps the
  synchronous server on its own port. `record.sh` and `record.ps1` follow
  automatically; a reverse proxy needs a second route.
- **Breaking — `record.ps1 -DeviceIp` is now `-Device`,** and both recorders
  default to `watchdog.local:81` instead of requiring an IP.
- **Breaking — movement updates no longer reach Gotify.** "Person moved to X"
  is a live event on the WebSocket only. The socket streams position
  continuously, so pushing the same thing to a phone was noise. Presence,
  sound, boot and calibration alerts are unchanged.
- **The firmware uses the `huge_app` partition layout.** The async stack pushed
  the default 1.3MB app partition to ~97% full; there is no OTA here, so the
  second app slot is traded for headroom (now ~40%). Reflashing rewrites the
  partition table.
- Detectors now confirm delivery as soon as an event is queued rather than
  when an HTTP POST returns 2xx. Retries, timeouts and backoff moved to the
  delivery worker, so a failing Gotify server can no longer stall the sensor
  loop or make a detector repeat its event every pass.
- The API token check moved to a shared `apiTokenAccepted()` used by both
  servers, enforced on the async side by middleware that also covers the
  WebSocket handshake — a bad token gets a real `401` instead of an upgrade
  followed by a close.
- Calibration requests from the API, the WebSocket and the BOOT button all run
  on the sensor loop via `pollCalibrationRequest()`. The command blocks ~300ms
  waiting for UART acknowledgements, which must not happen inside an AsyncTCP
  callback.
- cppcheck suppresses findings from vendored sources under `.pio/libdeps`, the
  way it already did for the platform packages.
- **Breaking — `GET /status` now requires the API token.** It used to return
  live occupancy (presence, distance, mic level, uptime) to anything that could
  reach port 80. Send `Authorization: Bearer <API_TOKEN>` or
  `X-Api-Key: <API_TOKEN>`.
- **Breaking — the audio stream moved from `POST /audio.pcm` to
  `GET /audio.pcm`.** It is a read, and the POST body only existed to smuggle
  the token past `ffmpeg`.
- **Breaking — no endpoint accepts the token in the request body any more.**
  Use one of the two headers above. `record.ps1` and `record.sh` already do.
- **Breaking — `record.ps1` no longer takes `-ApiToken`.** Set
  `WATCHDOG_API_TOKEN` or let the script prompt, so the token stays out of the
  shell history.
- The build fails if `API_TOKEN` is shorter than 16 characters.
- The firmware now builds with the pinned pioarduino `55.03.311` platform
  (Arduino-ESP32 3.3.11 and ESP-IDF 5.5.5) instead of Arduino-ESP32 2.0.17.
- Microphone DMA sampling now uses ESP-IDF's supported `adc_continuous_*`
  driver instead of the deprecated legacy ADC driver.
- The quality gate runs a pinned, cross-platform cppcheck package against
  PlatformIO's compilation database; pioarduino's bundled Linux analyzer
  depends on the obsolete `libpcre3` runtime.

### Fixed

- The telemetry socket no longer holds an `AsyncWebSocketClient*` across a
  send. `AsyncWebSocket::client()` releases the library's client lock before
  returning, and disconnects are processed on the AsyncTCP task, so the pointer
  could be freed mid-frame. The loop now keeps only an atomic client id and
  uses the id-based API, which locks for the whole lookup-and-send.
- `radarReport()` returns a snapshot instead of a reference into the live
  parser. `Ld2412Parser` fills its report field by field on the sensor loop, so
  `GET /status` could mix a distance from one frame with an energy from the
  next.
- `POST /calibrate` and the socket's `calibrate` command no longer run the
  radar command sequence inside a network callback, where its ~300ms of UART
  waits would stall every other connection. Both set a flag that the sensor
  loop acts on, which is the path the BOOT button already used.
- API tokens are compared over their full length instead of stopping at the
  first differing byte, and a rejection no longer logs the expected length.
- `deploy.ps1` runs the same gate as CI. It only ran the unit tests, so it
  could flash a build that had never been through cppcheck.
- `409 Conflict` from `GET /audio.pcm` is now JSON like every other error.
- Audio streaming runs in its own task, so a second recording request receives
  the documented `409 Conflict` immediately instead of waiting for the first
  client to disconnect.
- Removed a duplicated exit statement from `record.ps1`.
- A Gotify push now has explicit connect and read timeouts, so an unreachable
  server cannot stall presence and sound detection.
- The Gotify retry backoff no longer compares against an absolute deadline, so
  it survives the `millis()` rollover every ~49.7 days like the detectors do.
- `-Wall -Wextra` are on for both environments.
- Documented commands that never worked: `pio run` and `pio run -t upload` fail
  without `-e esp32-c3-supermini`, `docs/flashing.md` linked to a nonexistent
  anchor, and `docs/hardware.md` linked to four gitignored vendor archives.
