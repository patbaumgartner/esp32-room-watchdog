#include "notifications.h"

#include <Arduino.h>
#include <WiFi.h>

#include <atomic>

#include <DistanceTracker.h>
#include <PresenceMonitor.h>
#include <SoundDetector.h>

#include "config.h"
#include "net.h"
#include "radar.h"
#include "ws.h"

namespace
{
    SoundDetector soundDetector(SOUND_PP_THRESHOLD, SOUND_NOTIFY_COOLDOWN_MS);
    PresenceMonitor presenceMonitor(PRESENCE_DEBOUNCE_MS);
    DistanceTracker distanceTracker(DISTANCE_DELTA_CM, DISTANCE_UPDATE_MIN_INTERVAL_MS);

    std::atomic<bool> calibrationRequested{false};

    // Worth waking a phone for: goes to the socket and to the push queue.
    // Queueing always succeeds, so detectors confirm immediately and delivery
    // (including retries) is the worker's problem.
    void alert(const char *type, const String &message)
    {
        Serial.println("alert: " + message);
        wsPublishEvent(type, message);
        queueGotify(message);
    }

    // Live-only: the socket already streams position continuously, so pushing
    // the same thing to a phone would be noise.
    void live(const char *type, const String &message)
    {
        Serial.println("live: " + message);
        wsPublishEvent(type, message);
    }

    String personDetectedMessage()
    {
        const String where = radarDescribeTarget();
        return where.isEmpty() ? "Person detected" : "Person detected at " + where;
    }
}

void notifyBootOnline()
{
    alert("boot", "Sensor node online: " + WiFi.localIP().toString());
}

void requestCalibration()
{
    calibrationRequested = true;
}

void pollCalibrationRequest()
{
    if (!calibrationRequested.exchange(false))
    {
        return;
    }
    radarCalibrateBackground();
    alert("calibration",
          "Radar calibration starts in 10s - leave the room! (takes ~2 min)");
}
void notifyPresenceChanges(bool presentNow)
{
    const PresenceMonitor::Event event = presenceMonitor.onSample(presentNow, millis());
    if (event == PresenceMonitor::Event::None)
    {
        return;
    }

    const bool detected = event == PresenceMonitor::Event::Detected;
    alert(detected ? "presence" : "cleared",
          detected ? personDetectedMessage() : "Presence cleared");
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

    live("moved", "Person moved to " + radarDescribeTarget());
    distanceTracker.notificationSent(distanceCm, millis());
}

void notifyLoudSounds(const LevelWindow &mic)
{
    if (!soundDetector.shouldNotify(mic.peakToPeak(), millis()))
    {
        return;
    }

    alert("sound", "Sound detected (level " + String(mic.peakToPeak()) + ")");
    soundDetector.notificationSent(millis());
}
