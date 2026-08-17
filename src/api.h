#pragma once

#include <Arduino.h>

// Starts the LAN API: REST, the telemetry WebSocket and the PCM stream, all
// served asynchronously on API_PORT.
void apiBegin();

// Constant-time check of one Authorization or X-Api-Key header value, with an
// optional "Bearer " prefix. Shared so both servers authenticate identically.
bool apiTokenAccepted(const String &headerValue);
