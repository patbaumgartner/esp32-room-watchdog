# Changelog

Notable changes to the firmware and its HTTP API. This project has no tagged
releases yet — flash `main`. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed

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

### Added

- `check.sh` and `record.sh` for Linux and macOS.
- `lib/auth/ApiToken.h` — the token comparison moved out of `api.cpp` so it is
  covered by native tests like the rest of the decision logic.
- `pio check` (cppcheck) in the local quality gate and in CI.
- `SECURITY.md` with the threat model, `CODE_OF_CONDUCT.md`, issue and pull
  request templates.
- CI builds weekly, uploads `firmware.bin` as an artifact, and runs with
  least-privilege permissions, a concurrency group, and a job timeout.

### Fixed

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
