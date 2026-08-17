#pragma once

#include <stddef.h>
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

// The node answers to <MDNS_HOSTNAME>.local so clients need no fixed IP.
constexpr char MDNS_HOSTNAME[] = "watchdog";

// One async server carries the REST API, the telemetry socket and the PCM
// stream.
constexpr uint16_t API_PORT = 80;

// Log the X-Forwarded-For client instead of the peer address. Only enable
// behind a reverse proxy you control — anyone can forge the header otherwise.
// It never affects authentication, only what the serial log prints.
constexpr bool TRUST_PROXY_HEADERS = false;

// Live telemetry socket: push on change, no faster than this, and always at
// least once per heartbeat so a client can distinguish "quiet" from "dead".
constexpr uint32_t WS_MIN_PUSH_INTERVAL_MS = 100;
constexpr uint32_t WS_HEARTBEAT_MS = 2000;

// Longest client command accepted on the socket. Anything larger is refused
// before it is copied, so a client cannot size a heap buffer on the device.
constexpr size_t WS_COMMAND_MAX = 32;

// Backoff after a failed push (the worker retries the same message, so
// without this a dead server would be hammered every loop pass).
constexpr uint32_t GOTIFY_RETRY_BACKOFF_MS = 30000;

// Upper bound on how long one push may block the delivery worker.
constexpr uint16_t GOTIFY_TIMEOUT_MS = 5000;

// Delivery queue drained by a worker task, so detection never waits for the
// network. Alerts are rare; a short queue that drops the oldest entry is
// better than unbounded buffering of stale news.
constexpr size_t GOTIFY_QUEUE_DEPTH = 8;
constexpr size_t GOTIFY_MESSAGE_MAX = 128;
constexpr uint8_t GOTIFY_MAX_ATTEMPTS = 3;

// Sound detection: peak-to-peak ADC swing within one sample window.
constexpr uint32_t SOUND_SAMPLE_WINDOW_MS = 50;
constexpr int SOUND_PP_THRESHOLD = 1600; // ADC counts; a quiet room idles ~300
constexpr uint32_t SOUND_NOTIFY_COOLDOWN_MS = 15000;

// Lossless mono PCM stream. The ESP32-C3 ADC produces 12 useful bits; samples
// use a 16-bit container so standard recording tools can consume them.
constexpr uint32_t AUDIO_SAMPLE_RATE_HZ = 48000;

// The stream is drained by an AsyncTCP callback rather than a task that can
// block, so a refill is normally driven by the next TCP ack (~ms). If the
// connection ever goes fully idle, lwIP's poll timer is the fallback and fires
// only every 500ms, so size the buffer to ride out one of those without
// dropping a sample.
constexpr size_t AUDIO_STREAM_BUFFER_SAMPLES = 32768; // ~680ms

// Presence: LD2412 OUT must hold a new state this long before we notify.
constexpr uint32_t PRESENCE_DEBOUNCE_MS = 2000;

// Movement updates: notify when a present person moves this far from the
// last reported position, at most once per interval.
constexpr uint16_t DISTANCE_DELTA_CM = 100;
constexpr uint32_t DISTANCE_UPDATE_MIN_INTERVAL_MS = 10000;
