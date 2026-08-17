# Development Setup

**No, you don't need the Arduino IDE.** This is a [PlatformIO](https://platformio.org/)
project (`platformio.ini`), which pulls its own compiler, ESP32 board support and
upload tool into an isolated environment. It uses the *Arduino framework* as a
library layer — that's a different thing from the Arduino IDE application.

You also don't need a separate ESP-IDF install, and there is no global toolchain
to manage.

Once this page is done, go to [`flashing.md`](flashing.md) for the actual
build / upload / monitor workflow.

## 1. Install PlatformIO

**Option A — VS Code extension (recommended)**

1. Extensions view → install **PlatformIO IDE** (`platformio.platformio-ide`).
2. Reload when prompted. First launch installs its own Python venv and toolchain.

**Option B — CLI only**

```bash
pipx install platformio        # or: python3 -m pip install --user platformio
```

The first build downloads the pinned pioarduino ESP32 platform and the RISC-V
toolchain (several hundred MB). It needs internet once; after that it works
offline.

The two `lib_deps` in `platformio.ini` — `ESP32Async/AsyncTCP` and
`ESP32Async/ESPAsyncWebServer` — are fetched automatically on that first build
and pinned to exact versions, so no manual library install is needed.

## 2. Make the board visible

The ESP32-C3 SuperMini uses the chip's **native USB** (no CH340/CP210x bridge),
so no vendor driver is needed on any OS. Use a **data-capable USB-C cable** — a
charge-only cable is the single most common reason a board "doesn't show up".

How the port appears depends on where you run PlatformIO:

| Host | Device | Notes |
|---|---|---|
| Windows | `COM3`, `COM4`, … | Works out of the box |
| macOS | `/dev/cu.usbmodem*` | Works out of the box |
| Native Linux | `/dev/ttyACM0` | Needs `dialout` group (below) |
| **WSL2** | not visible by default | Needs USB forwarding (below) |

### Native Linux permissions

```bash
sudo usermod -aG dialout $USER   # then log out and back in
```

### WSL2

WSL2 does not see USB devices plugged into Windows. `/dev/ttyACM0` simply won't
exist, no matter how the cable is wired. Pick one of these:

**Option A — flash from Windows (simplest).** Install VS Code + PlatformIO on the
Windows side and open the project there via its UNC path
(`\\wsl$\<distro>\<path-to-clone>`), or keep a copy on the Windows filesystem.
The board shows up as a `COM` port and everything just works. Editing stays
possible from WSL.

**Option B — forward the USB device into WSL with [usbipd-win](https://github.com/dorssel/usbipd-win).**

On Windows, in an **Administrator** PowerShell:

```powershell
winget install usbipd                # once
usbipd list                          # find the ESP32's BUSID, e.g. 2-4
usbipd bind --busid 2-4              # once per device, needs admin
usbipd attach --wsl --busid 2-4      # each time you plug it in
```

Then back in WSL:

```bash
ls /dev/ttyACM*        # should now list the board
pio device list
```

To release it back to Windows:

```powershell
usbipd detach --busid 2-4
```

Caveats worth knowing before you pick Option B:

- The attach is **not persistent** — repeat `usbipd attach` after every replug,
  after a Windows reboot, and after WSL shuts down.
- The ESP32-C3 **re-enumerates its USB device when it resets**, which is exactly
  what happens at the start of an upload. The forwarded device can drop mid-flash
  and the upload fails. If you hit this repeatedly, use Option A.

## 3. Create your secrets file

WiFi credentials are kept out of git. `src/secrets.h` is gitignored:

```bash
cp src/secrets.h.example src/secrets.h
# edit src/secrets.h and fill in your SSID / password, the Gotify URL and
# token, and a random API_TOKEN of at least 16 characters
```

## 4. Verify the setup

```bash
cd esp32-room-watchdog
pio system info                  # PlatformIO + Python versions
pio device list                  # your board's port (see section 2 if empty)
pio run -e esp32-c3-supermini    # compile only, no board needed
pio test -e native               # host-side unit tests, no board needed
```

The `-e` is not optional: the firmware and the unit tests live in two
environments that target incompatible platforms, so a bare `pio run` tries to
compile the Arduino sources for your host and fails.

A successful `pio run -e esp32-c3-supermini` proves the toolchain is healthy
even with no board attached. That's the cleanest way to separate "toolchain
problem" from "USB problem".

## What's already configured

From [`platformio.ini`](../platformio.ini):

| Setting | Value | Meaning |
|---|---|---|
| `platform` | pioarduino `55.03.311` | Pinned Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5 platform for reproducible builds |
| `board` | `lolin_c3_mini` | Closest match for the ESP32-C3 SuperMini |
| `framework` | `arduino` | Arduino libraries, not the Arduino IDE |
| `board_build.partitions` | `huge_app.csv` | ~3MB app slot, no OTA — the async web server does not fit the default 1.3MB layout |
| `lib_deps` | `AsyncTCP` 3.5.0, `ESPAsyncWebServer` 3.12.0 | Pinned exactly; bumped by hand since PlatformIO has no Dependabot ecosystem |
| `monitor_speed` | `115200` | Must match `Serial.begin()` in `main.cpp` |
| `upload_speed` | `921600` | Drop to `115200` if uploads are flaky |
| `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1` | | Routes `Serial` over the chip's native USB, so no extra USB-serial adapter is needed |

## Next

[`flashing.md`](flashing.md) — build, upload, monitor, and troubleshooting.
