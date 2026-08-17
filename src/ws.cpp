#include "ws.h"

#include <atomic>

#include <TelemetryGate.h>

#include "config.h"
#include "mic.h"
#include "net.h"
#include "notifications.h"
#include "radar.h"

namespace
{
    AsyncWebSocket socket("/ws");

    // Only ever touched from the sensor loop.
    TelemetryGate gate(WS_MIN_PUSH_INTERVAL_MS, WS_HEARTBEAT_MS);

    // Zero means nobody is connected. Written from AsyncTCP callbacks and read
    // from the sensor loop, so it has to be atomic. Every use of it goes
    // through the socket's id-based API, which holds the library's client lock
    // for the whole lookup-and-send — a raw AsyncWebSocketClient* would dangle
    // if the client disconnected between the lookup and the send.
    std::atomic<uint32_t> activeClientId{0};

    // Everything the client is told except uptimeMs, so change detection covers
    // the whole frame and a ticking clock alone never counts as a change.
    // Packed because padding bytes would hash as noise.
    struct __attribute__((packed)) Snapshot
    {
        uint8_t presence;
        uint8_t targetState;
        uint16_t movingDistanceCm;
        uint8_t movingEnergy;
        uint16_t stationaryDistanceCm;
        uint8_t stationaryEnergy;
        int16_t micPeakToPeak;
        int16_t micMin;
        int16_t micMax;
        uint32_t audioDroppedSamples;
        uint8_t audioStreaming;
        uint8_t pushBackingOff;
    };

    Snapshot takeSnapshot(bool presenceNow, const LevelWindow &mic)
    {
        const Ld2412Parser::Report radar = radarReport();
        Snapshot snapshot = {};
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
        snapshot.audioStreaming = micPcmStreaming() ? 1 : 0;
        snapshot.pushBackingOff = pushBackingOff() ? 1 : 0;
        return snapshot;
    }

    String telemetryJson(const Snapshot &snapshot, uint32_t nowMs)
    {
        String json;
        json.reserve(320); // one allocation per frame instead of a dozen
        json = "{\"type\":\"telemetry\"";
        json += ",\"presence\":" + String(snapshot.presence ? "true" : "false");
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
        if (len > WS_COMMAND_MAX)
        {
            // Refuse before allocating: the longest command is 9 bytes, and a
            // client should not be able to size a heap buffer on this device.
            client->text("{\"type\":\"error\",\"message\":\"command too long\"}");
            return;
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
            const uint32_t previous = activeClientId.exchange(client->id());
            if (previous != 0 && previous != client->id())
            {
                // A killed app leaves a socket that TCP only reaps minutes
                // later; the live client wins so the node cannot lock us out.
                socket.close(previous, 1001, "replaced by a newer client");
            }
            Serial.printf("ws: client %s connected\n",
                          client->remoteIP().toString().c_str());
            sendHello(client);
            break;
        }
        case WS_EVT_DISCONNECT:
        case WS_EVT_ERROR:
        {
            uint32_t expected = client->id();
            if (activeClientId.compare_exchange_strong(expected, 0))
            {
                Serial.println("ws: client disconnected");
            }
            break;
        }
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
    const uint32_t id = activeClientId.load();
    if (id == 0)
    {
        return;
    }

    // Serving a different client than last pass: start it with a full frame
    // instead of making it wait for the room to change. Both variables are
    // loop-task local, so the gate is never touched from a network callback.
    static uint32_t servedClientId = 0;
    if (id != servedClientId)
    {
        gate.reset();
        servedClientId = id;
    }

    const Snapshot snapshot = takeSnapshot(presenceNow, mic);
    const uint32_t fingerprint = telemetryFingerprint(&snapshot, sizeof(snapshot));
    const uint32_t now = millis();
    if (!gate.shouldSend(fingerprint, now) || !socket.availableForWrite(id))
    {
        return; // telemetry is disposable; the gate retries next pass
    }
    if (socket.text(id, telemetryJson(snapshot, now)))
    {
        gate.sent(fingerprint, now);
    }
}

void wsPublishEvent(const char *type, const String &message)
{
    const uint32_t id = activeClientId.load();
    if (id == 0 || !socket.availableForWrite(id))
    {
        return;
    }
    String json = "{\"type\":\"event\",\"event\":\"";
    json += type;
    json += "\",\"message\":\"" + jsonEscape(message) + "\"";
    json += ",\"uptimeMs\":" + String(millis());
    json += "}";
    socket.text(id, json);
}

bool wsClientConnected()
{
    return activeClientId.load() != 0;
}
