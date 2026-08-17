#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <LevelWindow.h>

// Registers the /ws telemetry socket on the API server. The handshake goes
// through the same bearer-token middleware as the REST endpoints, so an
// unauthenticated client is rejected with a real 401 before the upgrade.
void wsAttach(AsyncWebServer &server, AsyncMiddleware &authentication);

// Publishes the current sensor snapshot when it changed or the heartbeat is
// due. Called from the sensor loop so the JSON is built on that task and the
// AsyncTCP callbacks never touch sensor state.
void wsPublishTelemetry(bool presenceNow, const LevelWindow &mic);

// Publishes a discrete event: "boot", "presence", "cleared", "moved", "sound"
// or "calibration". Dropped silently when no client is listening — anything
// that must survive that goes through queueGotify() instead.
void wsPublishEvent(const char *type, const String &message);

// True while a telemetry client is connected.
bool wsClientConnected();
