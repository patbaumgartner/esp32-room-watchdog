#include "api.h"

#include <WebServer.h>

#include "mic.h"
#include "notifications.h"
#include "radar.h"
#include "secrets.h"

namespace
{
    WebServer server(80);

    bool authorized()
    {
        return server.header("X-Api-Key") == API_TOKEN;
    }

    void handleStatus()
    {
        const Ld2412Parser::Report &radar = radarReport();
        const LevelWindow &mic = micLastWindow();
        String json = "{";
        json += "\"presence\":" + String(radarPresenceDetected() ? "true" : "false");
        json += ",\"targetState\":" + String(radar.targetState);
        json += ",\"movingDistanceCm\":" + String(radar.movingDistanceCm);
        json += ",\"movingEnergy\":" + String(radar.movingEnergy);
        json += ",\"stationaryDistanceCm\":" + String(radar.stationaryDistanceCm);
        json += ",\"stationaryEnergy\":" + String(radar.stationaryEnergy);
        json += ",\"micPeakToPeak\":" + String(mic.peakToPeak());
        json += ",\"micMin\":" + String(mic.minLevel());
        json += ",\"micMax\":" + String(mic.maxLevel());
        json += ",\"uptimeMs\":" + String(millis());
        json += "}";
        server.send(200, "application/json", json);
    }

    void handleCalibrate()
    {
        if (!authorized())
        {
            server.send(401, "application/json", "{\"error\":\"missing or wrong X-Api-Key\"}");
            return;
        }
        startCalibration();
        server.send(202, "application/json", "{\"status\":\"calibration scheduled in 10s\"}");
    }
}

void apiBegin()
{
    // WebServer only exposes headers listed here.
    static const char *headerKeys[] = {"X-Api-Key"};
    server.collectHeaders(headerKeys, 1);

    server.on("/status", HTTP_GET, handleStatus);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.onNotFound([]
                      { server.send(404, "application/json", "{\"error\":\"not found\"}"); });
    server.begin();
    Serial.println("api: listening on port 80 (/status, /calibrate)");
}

void apiPoll()
{
    server.handleClient();
}
