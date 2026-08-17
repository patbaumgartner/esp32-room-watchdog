#include "status_log.h"

#include <Arduino.h>

#include "radar.h"

void logStatusEverySecond(bool presentNow, const LevelWindow &mic)
{
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs < 1000)
    {
        return;
    }
    lastPrintMs = millis();

    const Ld2412Parser::Report radar = radarReport();
    Serial.printf("presence=%d dist=%dcm(m:%dcm/%d%% s:%dcm/%d%%) micPP=%d\n",
                  presentNow, radar.primaryDistanceCm(),
                  radar.movingDistanceCm, radar.movingEnergy,
                  radar.stationaryDistanceCm, radar.stationaryEnergy,
                  mic.peakToPeak());
}
