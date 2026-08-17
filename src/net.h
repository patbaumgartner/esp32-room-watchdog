#pragma once

#include <Arduino.h>

// Connects to the WiFi in secrets.h; blocks up to WIFI_CONNECT_TIMEOUT_MS.
void connectWifi();

// Advertises the node as <MDNS_HOSTNAME>.local with its HTTP/WS services.
void mdnsBegin();

// Starts the worker task that drains the push queue. Call once, after
// connectWifi() and before the first queueGotify().
void gotifyBegin();

// Hands a message to the delivery queue and returns immediately. The oldest
// pending message is discarded when the queue is full, so this never blocks
// and never fails from the caller's point of view.
void queueGotify(const String &message);

// True while the worker is holding off after a failed push. Reported to
// clients as a health signal; it does not stop anything from being queued.
bool pushBackingOff();

// Messages that never reached the server: discarded from a full queue, or
// given up on after GOTIFY_MAX_ATTEMPTS.
uint32_t gotifyLostCount();
