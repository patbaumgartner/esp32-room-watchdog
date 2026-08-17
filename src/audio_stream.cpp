#include "audio_stream.h"

#include <Arduino.h>

#include "config.h"
#include "mic.h"

namespace
{
    constexpr size_t NETWORK_CHUNK_SAMPLES = 1024;

    void sendTextResponse(WiFiClient &client, const char *status, const char *body)
    {
        client.printf("HTTP/1.0 %s\r\n", status);
        client.println("Content-Type: text/plain; charset=utf-8");
        client.println("Cache-Control: no-store");
        client.println("Connection: close");
        client.printf("Content-Length: %u\r\n\r\n", strlen(body));
        client.print(body);
    }
}

void streamAudioPcm(WiFiClient client)
{
    if (!micStartPcmStream())
    {
        sendTextResponse(client, "409 Conflict", "an audio stream is already active\n");
        return;
    }

    client.setNoDelay(true);
    client.println("HTTP/1.0 200 OK");
    client.printf("Content-Type: audio/x-raw; format=S16LE; rate=%lu; channels=1\r\n",
                  AUDIO_SAMPLE_RATE_HZ);
    client.println("Cache-Control: no-store");
    client.println("Connection: close");
    client.println();

    Serial.printf("audio: client %s connected\n", client.remoteIP().toString().c_str());
    int16_t samples[NETWORK_CHUNK_SAMPLES];
    while (client.connected() && WiFi.status() == WL_CONNECTED)
    {
        const size_t sampleCount = micReadPcm(samples, NETWORK_CHUNK_SAMPLES);
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(samples);
        const size_t byteCount = sampleCount * sizeof(int16_t);
        size_t sent = 0;
        while (sent < byteCount && client.connected())
        {
            const size_t written = client.write(bytes + sent, byteCount - sent);
            if (written == 0)
            {
                break;
            }
            sent += written;
        }
        if (sent < byteCount)
        {
            break;
        }
    }

    const uint32_t dropped = micDroppedSamples();
    micStopPcmStream();
    client.stop();
    Serial.printf("audio: client disconnected, dropped %lu samples\n", dropped);
}