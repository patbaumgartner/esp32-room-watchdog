#include "ws.h"

#include <TelemetryGate.h>

#include "config.h"
#include "mic.h"
#include "net.h"
#include "notifications.h"
#include "radar.h"

namespace
{
    AsyncWebSocket socket("/ws");
    TelemetryGate gate(WS_MIN_PUSH_INTERVAL_MS, WS_HEARTBEAT_MS);

    // Zero means nobody is connected. Only ever written from AsyncTCP
    // callbacks, only ever read as a hint — ws.client() is the real check.
    uint32_t activeClientId = 0;

    // Change detection operates on the raw values rather than the rendered
    // JSON, so uptime alone never counts as a change. Packed because padding
    // bytes would hash as noise.
    struct __attribute__((packed)) Snapshot
    {
        uint8_t presence;
        uint8_t targetState;
        uint16_t movingDistanceCm;
        uint8_t movingEnergy;
        uint16_t stationaryDistanceCm;
        uint8_t stationaryEnergy;
        int16_t micPeakToPeak;
        uint8_t audioStreaming;
    };

    Snapshot takeSnapshot(bool presenceNow, const LevelWindow &mic)
    {
        const Ld2412Parser::Report &radar = radarReport();
        Snapshot snapshot = {};
        snapshot.presence = presenceNow ? 1 : 0;
        snapshot.targetState = radar.targetState;
        snapshot.movingDistanceCm = radar.movingDistanceCm;
        snapshot.movingEnergy = radar.movingEnergy;
        snapshot.stationaryDistanceCm = radar.stationaryDistanceCm;
        snapshot.stationaryEnergy = radar.stationaryEnergy;
        snapshot.micPeakToPeak = static_cast<int16_t>(mic.peakToPeak());
        snapshot.audioStreaming = micPcmStreaming() ? 1 : 0;
        return snapshot;
    }

    String telemetryJson(const Snapshot &snapshot, const LevelWindow &mic, uint32_t nowMs)
    {
        String json = "{\"type\":\"telemetry\"";
        json += ",\"presence\":" + String(snapshot.presence ? "true" : "false");
        json += ",\"targetState\":" + String(snapshot.targetState);
        json += ",\"movingDistanceCm\":" + String(snapshot.movingDistanceCm);
        json += ",\"movingEnergy\":" + String(snapshot.movingEnergy);
        json += ",\"stationaryDistanceCm\":" + String(snapshot.stationaryDistanceCm);
        json += ",\"stationaryEnergy\":" + String(snapshot.stationaryEnergy);
        json += ",\"micPeakToPeak\":" + String(mic.peakToPeak());
        json += ",\"micMin\":" + String(mic.minLevel());
        json += ",\"micMax\":" + String(mic.maxLevel());
        json += ",\"audioStreaming\":" + String(snapshot.audioStreaming ? "true" : "false");
        json += ",\"audioDroppedSamples\":" + String(micDroppedSamples());
        json += ",\"pushBackingOff\":" + String(pushBackingOff() ? "true" : "false");
        json += ",\"uptimeMs\":" + String(nowMs);
        json += "}";
        return json;
    }

    String jsonEscape(const String &s)
    {
        String out = s;
        out.replace("\\", "\\\\");
        out.replace("\"", "\\\"");
        return out;
    }

    AsyncWebSocketClient *listener()
    {
        return activeClientId == 0 ? nullptr : socket.client(activeClientId);
    }

    void sendHello(AsyncWebSocketClient *client)
    {
        String json = "{\"type\":\"hello\"";
        json += ",\"host\":\"" + String(MDNS_HOSTNAME) + ".local\"";
        json += ",\"audioSampleRate\":" + String(AUDIO_SAMPLE_RATE_HZ);
        json += ",\"audioPort\":" + String(AUDIO_PORT);
        json += ",\"heartbeatMs\":" + String(WS_HEARTBEAT_MS);
        json += ",\"minGate\":" + String(RADAR_MIN_GATE);
        json += ",\"maxGate\":" + String(RADAR_MAX_GATE);
        json += ",\"uptimeMs\":" + String(millis());
        json += "}";
        client->text(json);
    }

    void handleCommand(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len)
    {
        const AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
        if (info == nullptr || !info->final || info->index != 0 || info->len != len ||
            info->opcode != WS_TEXT)
        {
            return; // commands are single short text frames; ignore anything else
        }

        String command;
        command.reserve(len);
        for (size_t i = 0; i < len; ++i)
        {
            command += static_cast<char>(data[i]);
        }
        command.trim();

        if (command == "calibrate")
        {
            requestCalibration();
            return;
        }
        client->text("{\"type\":\"error\",\"message\":\"unknown command\"}");
    }

    void onSocketEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type,
                       void *arg, uint8_t *data, size_t len)
    {
        switch (type)
        {
        case WS_EVT_CONNECT:
        {
            client->setCloseClientOnQueueFull(false);
            AsyncWebSocketClient *previous = listener();
            if (previous != nullptr && previous->id() != client->id())
            {
                // A killed app leaves a socket that TCP only reaps minutes
                // later; the live client wins so the node cannot lock us out.
                previous->close(1001, "replaced by a newer client");
            }
            activeClientId = client->id();
            gate.reset();
            Serial.printf("ws: client %s connected\n",
                          client->remoteIP().toString().c_str());
            sendHello(client);
            break;
        }
        case WS_EVT_DISCONNECT:
        case WS_EVT_ERROR:
            if (client->id() == activeClientId)
            {
                activeClientId = 0;
                Serial.println("ws: client disconnected");
            }
            break;
        case WS_EVT_DATA:
            handleCommand(client, arg, data, len);
            break;
        default:
            break;
        }
    }
}

void wsAttach(AsyncWebServer &server, AsyncMiddleware &authentication)
{
    socket.addMiddleware(&authentication);
    socket.onEvent(onSocketEvent);
    server.addHandler(&socket);
}

void wsPublishTelemetry(bool presenceNow, const LevelWindow &mic)
{
    socket.cleanupClients(1);
    AsyncWebSocketClient *client = listener();
    if (client == nullptr)
    {
        return;
    }

    const Snapshot snapshot = takeSnapshot(presenceNow, mic);
    const uint32_t fingerprint = telemetryFingerprint(&snapshot, sizeof(snapshot));
    const uint32_t now = millis();
    if (!gate.shouldSend(fingerprint, now))
    {
        return;
    }
    if (client->queueIsFull())
    {
        return; // telemetry is disposable; the gate retries next pass
    }
    client->text(telemetryJson(snapshot, mic, now));
    gate.sent(fingerprint, now);
}

void wsPublishEvent(const char *type, const String &message)
{
    AsyncWebSocketClient *client = listener();
    if (client == nullptr || client->queueIsFull())
    {
        return;
    }
    String json = "{\"type\":\"event\",\"event\":\"";
    json += type;
    json += "\",\"message\":\"" + jsonEscape(message) + "\"";
    json += ",\"uptimeMs\":" + String(millis());
    json += "}";
    client->text(json);
}

bool wsClientConnected()
{
    return listener() != nullptr;
}
