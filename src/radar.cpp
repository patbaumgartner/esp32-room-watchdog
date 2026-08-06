#include "radar.h"

#include <Ld2412Commands.h>

#include "config.h"

namespace
{
    Ld2412Parser parser;

    void sendCommand(size_t (*build)(uint8_t *))
    {
        uint8_t frame[Ld2412Commands::MAX_FRAME];
        Serial1.write(frame, build(frame));
        Serial1.flush();
        delay(100); // module needs a moment to ACK before the next command
    }
}

void radarBegin()
{
    Serial1.begin(LD2412_BAUD, SERIAL_8N1, PIN_LD2412_RX, PIN_LD2412_TX);
    pinMode(PIN_LD2412_OUT, INPUT);
}

void radarApplyTuning()
{
    uint8_t frame[Ld2412Commands::MAX_FRAME];
    sendCommand(Ld2412Commands::enableConfig);
    Serial1.write(frame, Ld2412Commands::basicParams(frame, RADAR_MIN_GATE, RADAR_MAX_GATE,
                                                     RADAR_UNMANNED_SECONDS, 0));
    Serial1.flush();
    delay(100);
    Serial1.write(frame, Ld2412Commands::motionSensitivity(frame, RADAR_MOTION_SENSITIVITY));
    Serial1.flush();
    delay(100);
    Serial1.write(frame, Ld2412Commands::staticSensitivity(frame, RADAR_STATIC_SENSITIVITY));
    Serial1.flush();
    delay(100);
    sendCommand(Ld2412Commands::endConfig);
    Serial.println("radar: tuning applied");
}

void radarCalibrateBackground()
{
    sendCommand(Ld2412Commands::enableConfig);
    sendCommand(Ld2412Commands::backgroundCorrection);
    sendCommand(Ld2412Commands::endConfig);
    Serial.println("radar: background correction starts in 10s");
}

void radarPoll()
{
    while (Serial1.available())
    {
        parser.feed(Serial1.read());
    }
}

bool radarPresenceDetected()
{
    return digitalRead(PIN_LD2412_OUT) == HIGH;
}

const Ld2412Parser::Report &radarReport()
{
    return parser.report();
}

String radarDescribeTarget()
{
    const Ld2412Parser::Report &report = parser.report();
    if (!report.hasTarget())
    {
        return "";
    }
    const float meters = report.primaryDistanceCm() / 100.0f;
    return String(meters, 1) + "m" + (report.isMoving() ? " (moving)" : " (still)");
}
