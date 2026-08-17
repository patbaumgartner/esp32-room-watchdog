#include "api.h"

#include <ESPAsyncWebServer.h>

#include <ApiToken.h>

#include "audio_api.h"
#include "config.h"
#include "mic.h"
#include "net.h"
#include "notifications.h"
#include "radar.h"
#include "secrets.h"
#include "ws.h"

// An empty or guessable token would leave the LAN API wide open.
static_assert(sizeof(API_TOKEN) - 1 >= 16,
              "API_TOKEN in secrets.h must be at least 16 characters");

namespace
{
    AsyncWebServer server(API_PORT);

    bool authorized(AsyncWebServerRequest *request)
    {
        const AsyncWebHeader *header = request->getHeader("Authorization");
        if (header != nullptr && apiTokenAccepted(header->value()))
        {
            return true;
        }
        header = request->getHeader("X-Api-Key");
        return header != nullptr && apiTokenAccepted(header->value());
    }

    // Behind a TLS-terminating proxy every peer address is the proxy's, which
    // makes the log useless. The forwarded address is never trusted for
    // anything but this line.
    String clientLabel(AsyncWebServerRequest *request)
    {
        if (TRUST_PROXY_HEADERS)
        {
            const AsyncWebHeader *forwarded = request->getHeader("X-Forwarded-For");
            if (forwarded != nullptr && !forwarded->value().isEmpty())
            {
                return forwarded->value();
            }
        }
        return request->client()->remoteIP().toString();
    }

    AsyncMiddlewareFunction requireToken([](AsyncWebServerRequest *request,
                                            ArMiddlewareNext next)
                                         {
        if (!authorized(request))
        {
            Serial.printf("api: %s from %s rejected, no valid API token\n",
                          request->url().c_str(), clientLabel(request).c_str());
            AsyncWebServerResponse *response = request->beginResponse(
                401, "application/json", "{\"error\":\"missing or wrong API token\"}");
            response->addHeader("WWW-Authenticate", "Bearer");
            request->send(response);
            return;
        }
        next(); });

    String statusJson()
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
        json += ",\"telemetryClient\":" + String(wsClientConnected() ? "true" : "false");
        json += ",\"pushBackingOff\":" + String(pushBackingOff() ? "true" : "false");
        json += ",\"pushLost\":" + String(gotifyLostCount());
        json += ",\"uptimeMs\":" + String(millis());
        json += "}";
        return json;
    }
}

bool apiTokenAccepted(const String &headerValue)
{
    String supplied = headerValue;
    const size_t prefix = ApiToken::bearerPrefixLength(supplied.c_str());
    if (prefix > 0)
    {
        supplied.remove(0, prefix);
    }
    return ApiToken::matches(supplied.c_str(), supplied.length(),
                             API_TOKEN, sizeof(API_TOKEN) - 1);
}

void apiBegin()
{
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", statusJson()); })
        .addMiddleware(&requireToken);

    server.on("/calibrate", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        requestCalibration();
        request->send(202, "application/json",
                      "{\"status\":\"calibration scheduled in 10s\"}"); })
        .addMiddleware(&requireToken);

    wsAttach(server, requireToken);

    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "application/json", "{\"error\":\"not found\"}"); });
    server.begin();
    audioApiBegin();

    Serial.printf("api: port %u (/status, /calibrate, /ws), port %u (/audio.pcm)\n",
                  API_PORT, AUDIO_PORT);
}
