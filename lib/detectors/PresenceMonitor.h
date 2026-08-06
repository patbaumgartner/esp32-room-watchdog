#pragma once

#include <stdint.h>

// Turns the raw radar output pin into debounced Detected/Cleared events.
//
// Contract:
//   - onSample() must be called with every reading. A state change only
//     produces an event after it has held steady for debounceMs (radar
//     flicker resets the timer).
//   - The event repeats on every sample until notificationSent() confirms
//     delivery, so a failed push is retried automatically.
//
// Pure logic: no hardware or clock access — callers pass the current time.
class PresenceMonitor
{
public:
    enum class Event
    {
        None,
        Detected,
        Cleared
    };

    explicit PresenceMonitor(uint32_t debounceMs) : debounceMs_(debounceMs) {}

    // Feed one raw reading; returns the event to deliver, if any.
    Event onSample(bool present, uint32_t nowMs)
    {
        if (present != candidate_)
        {
            candidate_ = present;
            candidateSinceMs_ = nowMs;
            return Event::None;
        }

        if (candidate_ != notified_ && nowMs - candidateSinceMs_ >= debounceMs_)
        {
            return candidate_ ? Event::Detected : Event::Cleared;
        }
        return Event::None;
    }

    // Record a successfully delivered notification for the pending state.
    void notificationSent() { notified_ = candidate_; }

    bool notifiedState() const { return notified_; }

private:
    uint32_t debounceMs_;
    bool candidate_ = false;
    bool notified_ = false;
    uint32_t candidateSinceMs_ = 0;
};
