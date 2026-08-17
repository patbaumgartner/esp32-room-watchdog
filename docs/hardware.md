# Hardware & Wiring

## Parts list (BOM)

| Component       | Model                                            | Purpose                                | Where we got it                                                   |
| --------------- | ------------------------------------------------ | -------------------------------------- | ----------------------------------------------------------------- |
| MCU             | EstarDyn ESP32-C3 MINI (SuperMini, native USB-C) | WiFi/BT controller                     | [AliExpress](https://de.aliexpress.com/item/1005010679041264.html) |
| Presence sensor | Hi-Link HLK-LD2412                               | 24GHz mmWave presence/motion detection | [AliExpress](https://de.aliexpress.com/item/1005007989577185.html) |
| Microphone      | MAX9814                                          | AGC mic amplifier (analog audio out)   | [AliExpress](https://de.aliexpress.com/item/1005008009711790.html) |
| Jumper wires    | 2.54mm Dupont kit, 10/20cm, M-M/M-F/F-F, 40-pin  | Sensor-to-board wiring                 | [AliExpress](https://de.aliexpress.com/item/1005006085062548.html) |

## Datasheets

Local copies kept in [`docs/datasheets/`](datasheets/) for offline reference:

| Component                    | Local copy                                                                                                                                                                     | Source                                                                                                               |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------- |
| ESP32-C3                     | [`esp32-c3_datasheet_en.pdf`](datasheets/esp32-c3_datasheet_en.pdf)                                                                                                           | [documentation.espressif.com](https://documentation.espressif.com/esp32-c3_datasheet_en.pdf)                          |
| MAX9814                      | [`MAX9814.pdf`](datasheets/MAX9814.pdf)                                                                                                                                       | [analog.com](https://www.analog.com/en/products/max9814.html) — saved by hand, the vendor blocks automated downloads |
| HLK-LD2412 user manual V1.01 | [`HLK-2412/HLK-LD2412 Human presence sensor module user manual V1.01.pdf`](<datasheets/HLK-2412/HLK-LD2412%20Human%20presence%20sensor%20module%20user%20manual%20V1.01.pdf>) | vendor kit (below)                                                                                                   |
| HLK-LD2412 serial protocol   | [`HLK-2412/HLK-LD2412 Serial Communication Protocol.pdf`](<datasheets/HLK-2412/HLK-LD2412%20Serial%20Communication%20Protocol.pdf>)                                           | vendor kit (below)                                                                                                   |

### HLK-LD2412 vendor material

- Product page: [HLK-LD2412 wide-angle sensing radar module](https://www.hlktech.net/index.php?id=1076) — Shenzhen Hi-Link Electronic. 24GHz FMCW, up to 9m sensing distance, 0.75m distance resolution, ±60° coverage.
- Official download folder: [Google Drive](https://drive.google.com/drive/folders/17TAVgH5YI_6naA24dpjnJjm2v28alRPl?usp=sharing) (linked from the product page).

The full kit was downloaded by hand into [`docs/datasheets/HLK-2412/`](datasheets/HLK-2412/) — note the folder is named after the vendor's download bundle (`HLK-2412`), while the part itself is the **HLK-LD2412**:

| File                                                                                                                                                                  | What it is                                            | Needed here?                                    |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------- | ----------------------------------------------- |
| [`HLK-LD2412 Human presence sensor module user manual V1.01.pdf`](<datasheets/HLK-2412/HLK-LD2412%20Human%20presence%20sensor%20module%20user%20manual%20V1.01.pdf>) | Pinout, electrical specs, mounting, config parameters | Yes — the reference for this doc               |
| [`HLK-LD2412 Serial Communication Protocol.pdf`](<datasheets/HLK-2412/HLK-LD2412%20Serial%20Communication%20Protocol.pdf>)                                           | UART frame format and command set                     | Yes — needed for the UART1 driver              |
| [`HLK-2412_Tool_install.exe`](datasheets/HLK-2412/HLK-2412_Tool_install.exe)                                                                                         | Windows installer for the visual config tool (~57MB)  | No                                              |
| [`HLK-2412-uppper computer tool V1.04 (1).zip`](<datasheets/HLK-2412/HLK-2412-uppper%20computer%20tool%20V1.04%20%281%29.zip>)                                       | Same config tool, portable build (~57MB)              | No                                              |
| [`serial port driver_ch341ser (2).zip`](<datasheets/HLK-2412/serial%20port%20driver_ch341ser%20%282%29.zip>)                                                         | CH341 USB-serial driver for the vendor's USB adapter  | No — we drive the module from the ESP32's UART |
| [`App download link.doc`](<datasheets/HLK-2412/App%20download%20link.doc>)                                                                                           | Link sheet for the vendor's phone app                 | No                                              |

> The two config-tool archives are ~57MB each (~112MB for the folder). They are
> Windows-only and irrelevant to building or flashing this project — worth
> excluding from version control if this tree ever gets committed.

## ESP32-C3 SuperMini pinout reference

With USB-C at the top, chip facing up:

- Left row: `5V, GND, 4, 3, 2, 1, 0`
- Right row: `21, 20, 10, 8, 9, 7, 6`

Strapping pins to avoid using as digital I/O during boot: `GPIO2`, `GPIO8`, `GPIO9`.

## HLK-LD2412 pinout

Two headers on the module:

**Main header (power + digital output):**

- `5V` — power input, connected to the expansion board's 5V rail (one of the two documented supply inputs; this is the one we use)
- `GND`
- `OUT` — digital presence output, active HIGH

**UART/debug header (3.3V logic):**

- `3V3` — 3.3V **power input**, an alternative to the main header's `5V` (manual Table 1: "either 5V or 3.3V power supply can be selected"). Left unconnected here because we power from 5V.
- `RX` — sensor listens (connect to ESP32 TX)
- `TX` — sensor talks (connect to ESP32 RX)
- `GND`

> **Update:** we initially powered the LD2412 from the ESP32's 3V3 rail via the
> `3V3` pin, and have since switched to the main header's `5V` fed from the
> expansion board's 5V rail. Both are legitimate — the manual lists `5V` and
> `3V3` as alternative power inputs, exactly one of which should be used. An
> earlier version of this note called `3V3` an LDO output that must not be
> driven; that was wrong, and the vendor manual contradicts it.

Default UART: **115200 baud, 8N1** (per the serial protocol PDF §1.2 — the
256000 default belongs to the older LD2410; an earlier version of this note
had that wrong).

## MAX9814 pinout

- `GND`
- `VDD` — power in, 2.7–5V (we use 3.3V for cleaner audio)
- `GAIN` — gain select: floating = 50dB, tied to GND = 60dB, tied to VDD = 40dB
- `OUT` — analog audio output
- `A/R` — attack/release ratio select: floating = default (~1:4000), tied to GND = 1:2000, tied to VDD = 1:500

For audio recording, the current 60dB gain gives the strongest signal but also
the most noise and the least clipping headroom. Tie `GAIN` to VDD for 40dB and
best fidelity when voices and sound sources are reasonably close, or leave it
floating for a 50dB compromise. Changing gain also changes peak-to-peak values,
so retune `SOUND_PP_THRESHOLD` afterward. The MAX9814 always applies automatic
gain control, so recordings are useful for speech and room events but do not
preserve absolute loudness dynamics.

## Cable color legend (as wired)

| Color        | Signal                        | From                       | To                      |
| ------------ | ----------------------------- | -------------------------- | ----------------------- |
| Black        | GND                           | LD2412 main header GND     | ESP32 GND (common)      |
| Black        | GND                           | LD2412 UART header GND     | ESP32 GND (common)      |
| Black        | GAIN (tied low → 60dB)       | MAX9814 GAIN               | ESP32 GND (common)      |
| Red          | 5V power                      | LD2412`5V` (main header) | Expansion board 5V rail |
| Red          | VDD power                     | MAX9814 VDD                | ESP32 3V3               |
| Yellow       | LD2412`OUT` (presence)      | LD2412 main header         | ESP32 GPIO5             |
| Green        | LD2412`TX`                  | LD2412 UART header         | ESP32 GPIO6 (ESP32 RX)  |
| White        | LD2412`RX`                  | LD2412 UART header         | ESP32 GPIO7 (ESP32 TX)  |
| Orange       | MAX9814`OUT` (analog audio) | MAX9814                    | ESP32 GPIO0 (ADC1)      |
| — (unwired) | LD2412`3V3` (debug header)  | left floating, not used    | —                      |
| — (unwired) | MAX9814`A/R`                | left floating              | —                      |

## Final pin map (ESP32-C3 GPIO → function)

```
GPIO0  ── MAX9814 OUT   (ADC1, analog audio level)
GPIO5  ── LD2412 OUT    (digital, presence detected = HIGH)
GPIO6  ── LD2412 TX     (ESP32 RX, UART1 @ 115200 8N1)
GPIO7  ── LD2412 RX     (ESP32 TX, UART1 @ 115200 8N1)
GPIO9  ── onboard BOOT button (hold ~1s after boot → radar calibration)
5V     ── LD2412 5V (main header), from expansion board 5V rail
3V3    ── MAX9814 VDD
GND    ── LD2412 GND ×2 + MAX9814 GND + MAX9814 GAIN (shared rail)
```

## Safety notes for future changes

- Feed the LD2412 from either `5V` or `3V3`, never both at once — the manual
  treats them as alternative supply inputs.
- Keep black wires exclusively as ground references to avoid confusing future
  rewiring.
- Avoid `GPIO2`, `GPIO8`, `GPIO9` for anything that must stay logic-low/high
  during boot — they're strapping pins on the ESP32-C3.
