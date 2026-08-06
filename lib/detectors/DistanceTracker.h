#pragma once

#include <stdint.h>

// Decides when a present target's distance change warrants an update
// notification ("person moved").
//
// Contract:
//   - The first non-zero distance becomes the silent baseline (the initial
//     "Person detected at X" notification covers it).
//   - onDistance() returns true when the distance differs from the last
//     notified one by at least deltaCm and minIntervalMs has passed since the
//     last delivered update. It stays true until notificationSent() confirms
//     delivery, so failed pushes retry.
//   - reset() clears the baseline; call it when presence clears.
//
// Pure logic: no hardware or clock access — callers pass the current time.
class DistanceTracker
{
public:
    DistanceTracker(uint16_t deltaCm, uint32_t minIntervalMs)
        : deltaCm_(deltaCm), minIntervalMs_(minIntervalMs) {}

    // Feed the current target distance (0 = no target). True when an update
    // notification should be sent.
    bool onDistance(uint16_t distanceCm, uint32_t nowMs)
    {
        if (distanceCm == 0)
        {
            return false;
        }
        if (!hasBaseline_)
        {
            baselineCm_ = distanceCm;
            hasBaseline_ = true;
            lastEventMs_ = nowMs;
            return false;
        }

        const uint16_t delta = distanceCm > baselineCm_ ? distanceCm - baselineCm_
                                                        : baselineCm_ - distanceCm;
        return delta >= deltaCm_ && nowMs - lastEventMs_ >= minIntervalMs_;
    }

    // Record a successfully delivered update; its distance becomes the new baseline.
    void notificationSent(uint16_t distanceCm, uint32_t nowMs)
    {
        baselineCm_ = distanceCm;
        lastEventMs_ = nowMs;
    }

    void reset() { hasBaseline_ = false; }

private:
    uint16_t deltaCm_;
    uint32_t minIntervalMs_;
    uint16_t baselineCm_ = 0;
    bool hasBaseline_ = false;
    uint32_t lastEventMs_ = 0;
};
