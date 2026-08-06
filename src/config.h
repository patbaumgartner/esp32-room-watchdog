#pragma once

#include <stdint.h>

// Pin map — see docs/hardware.md for full wiring reference.
constexpr int PIN_MIC_OUT = 0;     // MAX9814 analog audio (ADC1)
constexpr int PIN_LD2412_OUT = 5;  // LD2412 digital presence output
constexpr int PIN_LD2412_RX = 6;   // ESP32 RX <- LD2412 TX
constexpr int PIN_LD2412_TX = 7;   // ESP32 TX -> LD2412 RX
constexpr int PIN_BOOT_BUTTON = 9; // onboard BOOT button, usable after boot

// Datasheet default (protocol PDF §1.2): 115200 8N1. Not 256000 — that's the
// older LD2410's default.
constexpr uint32_t LD2412_BAUD = 115200;

// Radar tuning (applied at every boot; stored in the module anyway).
// One gate = 75cm at default resolution; gate index = distance / 0.75m.
// Sensitivity is the energy threshold per gate (0-100): HIGHER = LESS
// sensitive. Factory defaults are permissive; these are calmer values —
// near gates need high thresholds (strong reflections), far gates moderate.
constexpr uint8_t RADAR_MIN_GATE = 1;
constexpr uint8_t RADAR_MAX_GATE = 8; // 8 * 0.75m = 6m; ignore beyond
constexpr uint16_t RADAR_UNMANNED_SECONDS = 5;
constexpr uint8_t RADAR_MOTION_SENSITIVITY[14] = {
    60, 50, 40, 35, 35, 35, 35, 35, 40, 40, 40, 40, 40, 40};
constexpr uint8_t RADAR_STATIC_SENSITIVITY[14] = {
    40, 40, 40, 40, 40, 40, 40, 40, 45, 45, 45, 45, 45, 45};

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 10000; // needed for TLS cert validation

// Backoff after a failed push (detectors repeat events until delivered, so
// without this a failing push is retried every ~50ms loop pass).
constexpr uint32_t GOTIFY_RETRY_BACKOFF_MS = 30000;

// Sound detection: peak-to-peak ADC swing within one sample window.
constexpr uint32_t SOUND_SAMPLE_WINDOW_MS = 50;
constexpr int SOUND_PP_THRESHOLD = 1600; // ADC counts, tune to taste. Initial it was 800, but I changed it to 1600 to reduce false positives.
constexpr uint32_t SOUND_NOTIFY_COOLDOWN_MS = 15000;

// Presence: LD2412 OUT must hold a new state this long before we notify.
constexpr uint32_t PRESENCE_DEBOUNCE_MS = 2000;

// Movement updates: notify when a present person moves this far from the
// last reported position, at most once per interval.
constexpr uint16_t DISTANCE_DELTA_CM = 100;
constexpr uint32_t DISTANCE_UPDATE_MIN_INTERVAL_MS = 10000;
