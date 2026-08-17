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
- The `espressif32` platform is pinned to `7.0.1`. It used to float, so a
  future major could have swapped in an Arduino core that does not have the ADC
  API `mic.cpp` is built on.

### Added

- `check.sh` and `record.sh` for Linux and macOS.
- `pio check` (cppcheck) in the local quality gate and in CI.
- `SECURITY.md` with the threat model, `CODE_OF_CONDUCT.md`, issue and pull
  request templates.
- CI builds weekly, uploads `firmware.bin` as an artifact, and runs with
  least-privilege permissions, a concurrency group, and a job timeout.

### Fixed

- API tokens are compared over their full length instead of stopping at the
  first differing byte, and a rejection no longer logs the expected length.
- A Gotify push now has explicit connect and read timeouts, so an unreachable
  server cannot stall presence and sound detection.
- `-Wall -Wextra` are on for both environments.
