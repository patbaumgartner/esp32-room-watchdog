#pragma once

#include <WiFi.h>

// Writes the raw PCM response to an already authenticated HTTP client.
void streamAudioPcm(WiFiClient client);