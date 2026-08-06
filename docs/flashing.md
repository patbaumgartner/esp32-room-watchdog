# Flashing the ESP32-C3 SuperMini

This project uses [PlatformIO](https://platformio.org/) with the Arduino
framework — it handles the toolchain, board definitions, and upload process
without needing the full Arduino IDE.

> **Shortcut:** on Windows, `./check.ps1` runs unit tests + build, and
> `./deploy.ps1 [-Port COM3]` runs the checks and flashes the board. The steps
> below are the manual equivalents.

> First time on this machine? Do [`setup.md`](setup.md) first — it covers the
> toolchain install, getting the serial port visible (including **WSL2 USB
> forwarding**, which this page's Linux instructions do not cover) and the
> `secrets.h` file.

## 0. Recommended VS Code extensions

| Extension | ID | Why |
|---|---|---|
| PlatformIO IDE | `platformio.platformio-ide` | Provides build/upload/monitor tasks, IntelliSense for the Arduino/ESP-IDF framework, and manages the toolchain — this is the only one that's actually required |
| C/C++ (optional) | `ms-vscode.cpptools` | PlatformIO installs its own IntelliSense config, but this can help if you prefer Microsoft's C/C++ engine |

You do **not** need the Arduino IDE, ESP-IDF installed separately, or any
global toolchain — PlatformIO's VS Code extension bundles its own isolated
Python environment, compilers, and board support packages the first time you
open this folder.

## 1. Install PlatformIO

**Option A — VS Code extension (recommended):**
1. Open the Extensions view in VS Code.
2. Install "PlatformIO IDE" (`platformio.platformio-ide`).
3. Reload VS Code when prompted. It installs its own isolated toolchain (Python venv, compilers) automatically — no manual setup needed.

**Option B — CLI only:**
```bash
pipx install platformio
# or: python3 -m pip install --user platformio
```

## 2. Open the project

Open the project folder in VS Code. PlatformIO auto-detects
`platformio.ini` and shows a PlatformIO sidebar icon with Build/Upload/Monitor
tasks.

## 3. Connect the board

- Use a **data-capable USB-C cable** (not a charge-only cable) — this is the
  single most common reason a board "isn't detected."
- Plug the ESP32-C3 SuperMini into your computer.

Check Linux sees it:
```bash
pio device list
```
It typically shows up as `/dev/ttyACM0` (native USB CDC) — SuperMini boards
use the chip's built-in USB, not a separate USB-serial chip.

> **On WSL2 this list will be empty.** WSL cannot see USB devices plugged into
> Windows until they are forwarded with `usbipd`, or you flash from the Windows
> side instead — see [`setup.md`](setup.md#wsl2--this-machine).

**Permission denied on the serial port?**
```bash
sudo usermod -aG dialout $USER
```
Then log out and back in (group membership needs a fresh session).

## 4. Build

```bash
cd esp32-room-watchdog
pio run
```

Or use the PlatformIO sidebar → **Build** (checkmark icon).

## 5. Upload (flash)

```bash
pio run -t upload
```

Or PlatformIO sidebar → **Upload** (right-arrow icon).

**If upload fails to auto-reset into bootloader mode** (occasionally happens
on SuperMini clones with older USB-CDC bootstrap):
1. Hold the **BOOT** button on the board.
2. While holding BOOT, briefly tap/press **RESET** (or unplug/replug USB).
3. Release BOOT once the upload starts (PlatformIO will show "Connecting...").

## 6. Monitor serial output

```bash
pio device monitor
```
Baud rate is set to `115200` in `platformio.ini` — matches the `Serial.begin()`
call in `main.cpp`.

## 7. Secrets (WiFi credentials)

Never hardcode credentials in `main.cpp`. Copy the template and fill it in —
`secrets.h` is gitignored:

```bash
cp src/secrets.h.example src/secrets.h
# edit src/secrets.h with your WiFi SSID/password
```

## Troubleshooting quick reference

| Symptom | Likely cause |
|---|---|
| Board not in `pio device list` | Charge-only cable, or bad USB port/hub |
| Permission denied opening port | Missing `dialout` group membership |
| Upload times out / fails to connect | Try the manual BOOT+RESET sequence above |
| Monitor shows garbage characters | Baud rate mismatch — confirm `monitor_speed` matches `Serial.begin()` |
| Board resets/browns out under load | USB port/hub can't supply enough current once both sensors are attached — try a powered USB hub or different port |
