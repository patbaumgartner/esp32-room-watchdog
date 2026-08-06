#pragma once

#include <stdint.h>

// Decides when a mic loudness reading warrants a notification.
//
// Contract:
//   - shouldNotify() is true when peakToPeak >= threshold and no cooldown is
//     active. It stays true (retry) until notificationSent() confirms
//     delivery, so a failed push is retried on the next loud window.
//   - notificationSent(nowMs) starts the cooldown.
//
// Pure logic: no hardware or clock access — callers pass the current time.
class SoundDetector
{
public:
    // ppThreshold: minimum peak-to-peak ADC swing; cooldownMs: quiet period
    // after a delivered notification.
    SoundDetector(int ppThreshold, uint32_t cooldownMs)
        : ppThreshold_(ppThreshold), cooldownMs_(cooldownMs) {}

    // True when this window's peak-to-peak level should raise a notification.
    bool shouldNotify(int peakToPeak, uint32_t nowMs) const
    {
        if (peakToPeak < ppThreshold_)
        {
            return false;
        }
        return !notified_ || nowMs - lastNotifyMs_ >= cooldownMs_;
    }

    // Record a successfully delivered notification (starts the cooldown).
    void notificationSent(uint32_t nowMs)
    {
        notified_ = true;
        lastNotifyMs_ = nowMs;
    }

private:
    int ppThreshold_;
    uint32_t cooldownMs_;
    uint32_t lastNotifyMs_ = 0;
    bool notified_ = false;
};
