// Firmware entry point — pure composition. Sensor glue lives in mic/radar,
// event decisions in lib/, delivery in notifications. See docs/architecture.md
#include <Arduino.h>

#include <LevelWindow.h>

#include "api.h"
#include "calibration_button.h"
#include "mic.h"
#include "net.h"
#include "notifications.h"
#include "radar.h"
#include "status_log.h"
#include "ws.h"

void setup()
{
  Serial.begin(115200);
  // USB CDC writes block for ~2s when the host is not draining the port, which
  // stalled the whole sensor loop whenever no monitor was attached. Diagnostics
  // are not worth a stalled control loop, so drop them instead.
  Serial.setTxTimeoutMs(0);
  if (!micBegin())
  {
    Serial.println("Fatal: microphone initialization failed");
    while (true)
    {
      delay(1000);
    }
  }
  radarBegin();
  calibrationButtonBegin();

  Serial.println("Sensor node booting...");
  radarApplyTuning();
  connectWifi();
  gotifyBegin();
  netPoll();
  apiBegin();
  notifyBootOnline();
}

void loop()
{
  const LevelWindow mic = micSampleWindow();
  const bool presentNow = radarPresenceDetected();
  radarPoll();
  netPoll();

  calibrationButtonPoll();
  pollCalibrationRequest();
  notifyPresenceChanges(presentNow);
  notifyMovement();
  notifyLoudSounds(mic);
  wsPublishTelemetry(presentNow, mic);
  logStatusEverySecond(presentNow, mic);
}
