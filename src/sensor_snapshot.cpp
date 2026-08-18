#include "sensor_snapshot.h"

#include <Ld2412Parser.h>

#include "mic.h"
#include "net.h"
#include "radar.h"

namespace
{
    // Enough for the rendered fields at their widest, so a frame costs one
    // allocation rather than a dozen reallocations.
    constexpr size_t SENSOR_JSON_BYTES = 320;
}

SensorSnapshot takeSensorSnapshot(bool presenceNow, const LevelWindow &mic)
{
    const Ld2412Parser::Report radar = radarReport();
    SensorSnapshot snapshot = {};
    snapshot.presence = presenceNow ? 1 : 0;
    snapshot.targetState = radar.targetState;
    snapshot.movingDistanceCm = radar.movingDistanceCm;
    snapshot.movingEnergy = radar.movingEnergy;
    snapshot.stationaryDistanceCm = radar.stationaryDistanceCm;
    snapshot.stationaryEnergy = radar.stationaryEnergy;
    snapshot.micPeakToPeak = static_cast<int16_t>(mic.peakToPeak());
    snapshot.micMin = static_cast<int16_t>(mic.minLevel());
    snapshot.micMax = static_cast<int16_t>(mic.maxLevel());
    snapshot.audioDroppedSamples = micDroppedSamples();
    snapshot.pushLost = pushLostCount();
    snapshot.audioStreaming = micPcmStreaming() ? 1 : 0;
    snapshot.pushBackingOff = pushBackingOff() ? 1 : 0;
    return snapshot;
}

void appendSensorFields(String &json, const SensorSnapshot &snapshot, uint32_t uptimeMs)
{
    json.reserve(json.length() + SENSOR_JSON_BYTES);
    json += "\"presence\":" + String(snapshot.presence ? "true" : "false");
    json += ",\"targetState\":" + String(snapshot.targetState);
    json += ",\"movingDistanceCm\":" + String(snapshot.movingDistanceCm);
    json += ",\"movingEnergy\":" + String(snapshot.movingEnergy);
    json += ",\"stationaryDistanceCm\":" + String(snapshot.stationaryDistanceCm);
    json += ",\"stationaryEnergy\":" + String(snapshot.stationaryEnergy);
    json += ",\"micPeakToPeak\":" + String(snapshot.micPeakToPeak);
    json += ",\"micMin\":" + String(snapshot.micMin);
    json += ",\"micMax\":" + String(snapshot.micMax);
    json += ",\"audioStreaming\":" + String(snapshot.audioStreaming ? "true" : "false");
    json += ",\"audioDroppedSamples\":" + String(snapshot.audioDroppedSamples);
    json += ",\"pushBackingOff\":" + String(snapshot.pushBackingOff ? "true" : "false");
    json += ",\"pushLost\":" + String(snapshot.pushLost);
    json += ",\"uptimeMs\":" + String(uptimeMs);
}
