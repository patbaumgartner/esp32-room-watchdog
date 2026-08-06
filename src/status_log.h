#pragma once

#include <LevelWindow.h>

// Serial diagnostics: one status line per second (presence, radar, mic).
void logStatusEverySecond(bool presentNow, const LevelWindow &mic);
