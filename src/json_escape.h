#pragma once

#include <Arduino.h>

// Escapes text for use inside a JSON string literal, without the surrounding
// quotes. An alert message is rendered twice — into the Gotify request body
// and into the WebSocket event frame — so the escaping lives here rather than
// once in each renderer.
String jsonEscape(const String &text);
