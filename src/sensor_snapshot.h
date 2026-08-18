#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <LevelWindow.h>

// One coherent read of everything `GET /status` and the WebSocket telemetry
// frame report, so the two cannot drift apart. They did once: `pushLost`
// existed on the REST endpoint and not on the socket.
//
// Packed because ws.cpp hashes this struct for change detection and padding
// bytes would hash as noise. uptimeMs is deliberately absent — a ticking clock
// must not count as a change.
struct __attribute__((packed)) SensorSnapshot
{
    uint8_t presence;
    uint8_t targetState;
    uint16_t movingDistanceCm;
    uint8_t movingEnergy;
    uint16_t stationaryDistanceCm;
    uint8_t stationaryEnergy;
    int16_t micPeakToPeak;
    int16_t micMin;
    int16_t micMax;
    uint32_t audioDroppedSamples;
    uint32_t pushLost;
    uint8_t audioStreaming;
    uint8_t pushBackingOff;
};

// Safe to call from any task: every accessor it reads is synchronised.
SensorSnapshot takeSensorSnapshot(bool presenceNow, const LevelWindow &mic);

// Appends the shared fields to an open JSON object, with no leading comma and
// no braces, so each caller can add its own keys around them.
void appendSensorFields(String &json, const SensorSnapshot &snapshot, uint32_t uptimeMs);
