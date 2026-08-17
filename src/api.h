#pragma once

#include <Arduino.h>

// Starts the LAN API: async REST + WebSocket on API_PORT, and the
// synchronous PCM stream server on AUDIO_PORT.
void apiBegin();

// Constant-time check of one Authorization or X-Api-Key header value, with an
// optional "Bearer " prefix. Shared so both servers authenticate identically.
bool apiTokenAccepted(const String &headerValue);
