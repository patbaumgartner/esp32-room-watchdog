#include "api.h"

#include <WebServer.h>

#include "audio_stream.h"
#include "mic.h"
#include "notifications.h"
#include "radar.h"
#include "secrets.h"

namespace
{
    WebServer server(80);

    void apiServerTask(void *)
    {
        while (true)
        {
            server.handleClient();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    bool authorized()
    {
        String supplied = server.header("Authorization");
        if (supplied.startsWith("Bearer "))
        {
            supplied.remove(0, 7);
        }
        else
        {
            supplied = server.header("X-Api-Key");
        }
        if (supplied.isEmpty() && server.hasArg("plain"))
        {
            supplied = server.arg("plain");
        }
        const bool matches = supplied == API_TOKEN;
        if (!matches)
        {
            Serial.printf("api: auth rejected (header=%d, received=%u, expected=%u)\n",
                          server.hasHeader("Authorization") || server.hasHeader("X-Api-Key") ||
                              server.hasArg("plain"),
                          supplied.length(),
                          strlen(API_TOKEN));
        }
        return matches;
    }

    void handleStatus()
    {
        const Ld2412Parser::Report &radar = radarReport();
        const LevelWindow mic = micLastWindow();
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
        json += ",\"audioStreaming\":" + String(micPcmStreaming() ? "true" : "false");
        json += ",\"audioDroppedSamples\":" + String(micDroppedSamples());
        json += ",\"uptimeMs\":" + String(millis());
        json += "}";
        server.send(200, "application/json", json);
    }

    void handleCalibrate()
    {
        if (!authorized())
        {
            server.send(401, "application/json", "{\"error\":\"missing or wrong API token\"}");
            return;
        }
        startCalibration();
        server.send(202, "application/json", "{\"status\":\"calibration scheduled in 10s\"}");
    }

    void handleAudio()
    {
        if (!authorized())
        {
            server.send(401, "application/json", "{\"error\":\"missing or wrong API token\"}");
            return;
        }
        streamAudioPcm(server.client());
    }
}

void apiBegin()
{
    // WebServer only exposes headers listed here.
    static const char *headerKeys[] = {"X-Api-Key"};
    server.collectHeaders(headerKeys, 1);

    server.on("/status", HTTP_GET, handleStatus);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.on("/audio.pcm", HTTP_POST, handleAudio);
    server.onNotFound([]
                      { server.send(404, "application/json", "{\"error\":\"not found\"}"); });
    server.begin();
    if (xTaskCreate(apiServerTask, "http-api", 8192, nullptr, 1, nullptr) != pdPASS)
    {
        Serial.println("api: server task allocation failed");
        return;
    }
    Serial.println("api: listening on port 80 (/status, /calibrate, /audio.pcm)");
}
