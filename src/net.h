#pragma once

#include <Arduino.h>

// Connects to the WiFi in secrets.h; blocks up to WIFI_CONNECT_TIMEOUT_MS.
void connectWifi();

// POSTs message to the Gotify server in secrets.h. Returns true on HTTP 2xx;
// returns false without a network attempt when WiFi is down.
bool pushGotify(const String &message);

// True while pushGotify is refusing attempts after a failure
// (GOTIFY_RETRY_BACKOFF_MS).
bool pushBackingOff();
