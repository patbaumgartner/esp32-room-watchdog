#pragma once

#include <stddef.h>
#include <stdint.h>

// Decides when the live telemetry socket should receive a new frame.
//
// Contract:
//   - shouldSend() is true when the payload changed since the last delivered
//     frame and minIntervalMs has passed, or when heartbeatMs elapsed with no
//     traffic at all — a client must be able to tell a quiet room from a dead
//     link.
//   - It stays true until sent() confirms delivery, so a frame the socket
//     refused under backpressure is retried on the next pass.
//   - reset() forgets the history so a freshly connected client is served
//     immediately instead of waiting for the room to change.
//
// Pure logic: no hardware or clock access — callers pass the current time.
class TelemetryGate
{
public:
    TelemetryGate(uint32_t minIntervalMs, uint32_t heartbeatMs)
        : minIntervalMs_(minIntervalMs), heartbeatMs_(heartbeatMs) {}

    bool shouldSend(uint32_t fingerprint, uint32_t nowMs) const
    {
        if (!hasSent_)
        {
            return true;
        }
        const uint32_t sinceMs = nowMs - lastSentMs_;
        if (sinceMs >= heartbeatMs_)
        {
            return true;
        }
        return fingerprint != lastFingerprint_ && sinceMs >= minIntervalMs_;
    }

    void sent(uint32_t fingerprint, uint32_t nowMs)
    {
        lastFingerprint_ = fingerprint;
        lastSentMs_ = nowMs;
        hasSent_ = true;
    }

    void reset() { hasSent_ = false; }

private:
    uint32_t minIntervalMs_;
    uint32_t heartbeatMs_;
    uint32_t lastFingerprint_ = 0;
    uint32_t lastSentMs_ = 0;
    bool hasSent_ = false;
};

// FNV-1a over the payload bytes: change detection without keeping a copy of
// the previous frame. Callers must zero-initialize padded structs, or padding
// bytes will read as spurious changes.
inline uint32_t telemetryFingerprint(const void *data, size_t length)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}
