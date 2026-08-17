#include "audio_api.h"

#include <Arduino.h>
#include <WebServer.h>

#include "api.h"
#include "audio_stream.h"
#include "config.h"

namespace
{
    // Matches the stack the combined API task used before the async split; the
    // work here (WebServer header parsing plus the stream handoff) is the same.
    constexpr uint32_t AUDIO_API_TASK_STACK_BYTES = 8192;

    WebServer audioServer(AUDIO_PORT);

    void audioServerTask(void *)
    {
        while (true)
        {
            audioServer.handleClient();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // True when the request carried a valid token; sends the 401 otherwise.
    bool rejectUnauthorized()
    {
        if (apiTokenAccepted(audioServer.header("Authorization")) ||
            apiTokenAccepted(audioServer.header("X-Api-Key")))
        {
            return false;
        }
        Serial.println("audio: /audio.pcm rejected, no valid API token");
        audioServer.sendHeader("WWW-Authenticate", "Bearer");
        audioServer.send(401, "application/json",
                         "{\"error\":\"missing or wrong API token\"}");
        return true;
    }

    void handleAudio()
    {
        if (rejectUnauthorized())
        {
            return;
        }
        switch (startAudioPcmStream(audioServer.client()))
        {
        case AudioStreamStartResult::Started:
            return;
        case AudioStreamStartResult::Busy:
            audioServer.send(409, "application/json",
                             "{\"error\":\"an audio stream is already active\"}");
            return;
        case AudioStreamStartResult::ResourceFailure:
            audioServer.send(503, "application/json",
                             "{\"error\":\"audio stream task could not be started\"}");
            return;
        }
    }
}

void audioApiBegin()
{
    // WebServer only exposes headers listed here (it always adds Authorization).
    static const char *headerKeys[] = {"X-Api-Key"};
    audioServer.collectHeaders(headerKeys, 1);

    audioServer.on("/audio.pcm", HTTP_GET, handleAudio);
    audioServer.onNotFound([]
                           { audioServer.send(404, "application/json",
                                              "{\"error\":\"not found\"}"); });
    audioServer.begin();
    if (xTaskCreate(audioServerTask, "audio-api", AUDIO_API_TASK_STACK_BYTES,
                    nullptr, 1, nullptr) != pdPASS)
    {
        Serial.println("audio: server task allocation failed");
    }
}
