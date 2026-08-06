#pragma once

#include <LevelWindow.h>

// Event detection + Gotify delivery. Owns the detectors (lib/detectors) and
// applies their shared contract: events repeat until delivery is confirmed.
void notifyBootOnline();
void notifyPresenceChanges(bool presentNow);
void notifyMovement();
void notifyLoudSounds(const LevelWindow &mic);

// Kicks off radar background calibration and warns via push to leave the room.
void startCalibration();
