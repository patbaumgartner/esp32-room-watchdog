# Contributing

Thanks for your interest in improving this project! Issues and pull requests
are welcome.

## Getting started

1. Fork and clone the repository.
2. Follow [docs/setup.md](docs/setup.md) to install PlatformIO.
3. Create `src/secrets.h` from the template (required to compile):

   ```bash
   cp src/secrets.h.example src/secrets.h
   ```

## Development workflow

- **Run the quality gate before pushing:**

  ```bash
  ./check.sh         # Linux/macOS
  ```

  ```powershell
  ./check.ps1        # Windows
  ```

  Both run native unit tests, cppcheck, and the firmware build. The scripts
  also pin cppcheck through PlatformIO's package runner, so contributors and
  CI use the same analyzer on Linux and Windows. Always pass `-e` to direct
  PlatformIO commands — the two environments target incompatible platforms.

- **Flash to hardware** (optional, needs a board):

  ```powershell
  ./deploy.ps1 [-Port COM3]
  ```

  ```bash
  pio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0
  ```

CI runs the same checks on every pull request — a green local gate means a
green pipeline.

## Design rules

- **Decision logic goes in [lib/detectors](lib/detectors)** — header-only,
  pure C++, no `Arduino.h`, no hardware calls. Time is passed in as a
  parameter (`nowMs`), never read via `millis()` inside the class.
- **Hardware and network I/O stays in src/** — thin glue only.
- **Every class in lib/ gets a test** in [test/](test/) (Unity, runs natively).
- **Tuning constants live in [src/config.h](src/config.h)**, credentials in
  the gitignored `src/secrets.h`. Never commit real credentials.

## Pull requests

- Keep PRs focused — one topic per PR.
- Add or update unit tests for behavior changes in `lib/`.
- Update the affected docs (`README.md`, `docs/`) when behavior or wiring
  changes, and add a `CHANGELOG.md` entry for anything a user would notice —
  especially changes to the HTTP API, `src/config.h` defaults, or wiring.
- Describe how you tested (unit tests only, or verified on hardware).

## Reporting issues

Please include:

- What you expected vs. what happened
- Serial monitor output (`pio device monitor`, 115200 baud) if relevant
- Your hardware variant (board, sensor modules) and wiring deviations, if any

Never paste `API_TOKEN`, your Gotify token, or your WiFi password into an
issue. Security problems go to [SECURITY.md](SECURITY.md) instead of a public
issue.
