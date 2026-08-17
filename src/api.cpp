#include "api.h"

#include <WebServer.h>

#include <ApiToken.h>

#include "audio_stream.h"
#include "mic.h"
#include "notifications.h"
#include "radar.h"
#include "secrets.h"

// An empty or guessable token would leave the LAN API wide open.
static_assert(sizeof(API_TOKEN) - 1 >= 16,
              "API_TOKEN in secrets.h must be at least 16 characters");

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
        const size_t prefix = ApiToken::bearerPrefixLength(supplied.c_str());
        if (prefix > 0)
        {
            supplied.remove(0, prefix);
        }
        else
        {
            supplied = server.header("X-Api-Key");
        }
        return ApiToken::matches(supplied.c_str(), supplied.length(),
                                 API_TOKEN, sizeof(API_TOKEN) - 1);
    }

    // True when the request carried a valid token; sends the 401 otherwise.
    bool rejectUnauthorized()
    {
        if (authorized())
        {
            return false;
        }
        Serial.printf("api: %s %s rejected, no valid API token\n",
                      server.method() == HTTP_GET ? "GET" : "POST",
                      server.uri().c_str());
        server.sendHeader("WWW-Authenticate", "Bearer");
        server.send(401, "application/json", "{\"error\":\"missing or wrong API token\"}");
        return true;
    }

    void handleStatus()
    {
        if (rejectUnauthorized())
        {
            return;
        }
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
        if (rejectUnauthorized())
        {
            return;
        }
        startCalibration();
        server.send(202, "application/json", "{\"status\":\"calibration scheduled in 10s\"}");
    }

    void handleAudio()
    {
        if (rejectUnauthorized())
        {
            return;
        }
        streamAudioPcm(server.client());
    }
}

void apiBegin()
{
    // WebServer only exposes headers listed here (it always adds Authorization).
    static const char *headerKeys[] = {"X-Api-Key"};
    server.collectHeaders(headerKeys, 1);

    server.on("/status", HTTP_GET, handleStatus);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.on("/audio.pcm", HTTP_GET, handleAudio);
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
