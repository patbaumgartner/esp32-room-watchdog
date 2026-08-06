#include "notifications.h"

#include <Arduino.h>
#include <WiFi.h>

#include <DistanceTracker.h>
#include <PresenceMonitor.h>
#include <SoundDetector.h>

#include "config.h"
#include "net.h"
#include "radar.h"

namespace
{
    SoundDetector soundDetector(SOUND_PP_THRESHOLD, SOUND_NOTIFY_COOLDOWN_MS);
    PresenceMonitor presenceMonitor(PRESENCE_DEBOUNCE_MS);
    DistanceTracker distanceTracker(DISTANCE_DELTA_CM, DISTANCE_UPDATE_MIN_INTERVAL_MS);

    // Logs and pushes; true when delivered (detectors confirm on success only).
    bool notify(const String &message)
    {
        if (pushBackingOff())
        {
            return false; // don't spam the serial log with doomed attempts
        }
        Serial.println("notify: " + message);
        return pushGotify(message);
    }

    String personDetectedMessage()
    {
        const String where = radarDescribeTarget();
        return where.isEmpty() ? "Person detected" : "Person detected at " + where;
    }
}

void notifyBootOnline()
{
    notify("Sensor node online: " + WiFi.localIP().toString());
}

void startCalibration()
{
    radarCalibrateBackground();
    notify("Radar calibration starts in 10s - leave the room! (takes ~2 min)");
}

void notifyPresenceChanges(bool presentNow)
{
    const PresenceMonitor::Event event = presenceMonitor.onSample(presentNow, millis());
    if (event == PresenceMonitor::Event::None)
    {
        return;
    }

    const bool detected = event == PresenceMonitor::Event::Detected;
    if (!notify(detected ? personDetectedMessage() : "Presence cleared"))
    {
        return;
    }

    presenceMonitor.notificationSent();
    if (detected)
    {
        distanceTracker.notificationSent(radarReport().primaryDistanceCm(), millis());
    }
    else
    {
        distanceTracker.reset();
    }
}

void notifyMovement()
{
    // Movement is only meaningful while in the confirmed "present" state.
    if (!presenceMonitor.notifiedState())
    {
        return;
    }

    const uint16_t distanceCm = radarReport().primaryDistanceCm();
    if (!distanceTracker.onDistance(distanceCm, millis()))
    {
        return;
    }

    if (notify("Person moved to " + radarDescribeTarget()))
    {
        distanceTracker.notificationSent(distanceCm, millis());
    }
}

void notifyLoudSounds(const LevelWindow &mic)
{
    if (!soundDetector.shouldNotify(mic.peakToPeak(), millis()))
    {
        return;
    }

    if (notify("Sound detected (level " + String(mic.peakToPeak()) + ")"))
    {
        soundDetector.notificationSent(millis());
    }
}
