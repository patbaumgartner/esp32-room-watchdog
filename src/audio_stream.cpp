#include "audio_stream.h"

#include <Arduino.h>

#include "config.h"
#include "mic.h"

namespace
{
    // ffmpeg needs the format up front; the body is headerless raw PCM.
    const char *const PCM_CONTENT_TYPE = "audio/x-raw";

    size_t fillPcm(uint8_t *buffer, size_t maxLen, size_t)
    {
        if (!micPcmStreaming())
        {
            return 0; // terminating chunk: the recording was stopped
        }
        const size_t written = micTryReadPcm(buffer, maxLen);

        // 0 would end the stream, so distinguish "nothing yet" from "done".
        return written > 0 ? written : RESPONSE_TRY_AGAIN;
    }
}

void audioStreamAttach(AsyncWebServer &server, AsyncMiddleware &authentication)
{
    server
        .on("/audio.pcm", HTTP_GET,
            [](AsyncWebServerRequest *request)
            {
                if (!micStartPcmStream())
                {
                    request->send(409, "application/json",
                                  "{\"error\":\"an audio stream is already active\"}");
                    return;
                }

                AsyncWebServerResponse *response =
                    request->beginChunkedResponse(PCM_CONTENT_TYPE, fillPcm);
                if (response == nullptr)
                {
                    micStopPcmStream();
                    request->send(503, "application/json",
                                  "{\"error\":\"audio stream could not be started\"}");
                    return;
                }

                response->addHeader("Cache-Control", "no-store");
                response->addHeader("X-Audio-Format", "s16le");
                response->addHeader("X-Audio-Sample-Rate", String(AUDIO_SAMPLE_RATE_HZ));
                response->addHeader("X-Audio-Channels", "1");

                // Fires on a client drop and on normal completion alike, so the
                // single-recording slot cannot leak.
                request->onDisconnect(
                    []()
                    {
                        Serial.printf("audio: client disconnected, dropped %lu samples\n",
                                      micDroppedSamples());
                        micStopPcmStream();
                    });

                Serial.printf("audio: client %s connected\n",
                              request->client()->remoteIP().toString().c_str());
                request->send(response);
            })
        .addMiddleware(&authentication);
}
