#pragma once

#include <WiFi.h>

enum class AudioStreamStartResult
{
    Started,
    Busy,
    ResourceFailure,
};

// Transfers an already authenticated client to a dedicated streaming task.
// This lets WebServer return to accept() and reject concurrent recordings.
AudioStreamStartResult startAudioPcmStream(const WiFiClient &client);
