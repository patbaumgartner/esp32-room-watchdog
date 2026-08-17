#include "net.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <time.h>
#include <string.h>

#include "certs.h"
#include "config.h"
#include "secrets.h"

namespace
{
    uint32_t lastFailureMs = 0;
    bool hadFailure = false;

    void logWifiDisconnect(arduino_event_id_t, arduino_event_info_t info)
    {
        Serial.printf("WiFi: disconnected (reason %u)\n",
                      info.wifi_sta_disconnected.reason);
    }

    bool gotifyUsesTls()
    {
        return strncmp(GOTIFY_URL, "https:", 6) == 0;
    }

    String jsonEscape(const String &s)
    {
        String out = s;
        out.replace("\\", "\\\\");
        out.replace("\"", "\\\"");
        return out;
    }

    // TLS cert validation compares against the clock; without NTP the ESP32
    // thinks it's 1970 and rejects every certificate.
    void syncClock()
    {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        Serial.print("NTP: syncing");
        const uint32_t start = millis();
        while (time(nullptr) < 8 * 3600 * 2 && millis() - start < NTP_SYNC_TIMEOUT_MS)
        {
            delay(250);
            Serial.print('.');
        }
        Serial.println(time(nullptr) > 8 * 3600 * 2 ? " ok" : " timeout (HTTPS pushes will fail)");
    }
}

void connectWifi()
{
    Serial.printf("WiFi: connecting to %s", WIFI_SSID);
    WiFi.onEvent(logWifiDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS)
    {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("WiFi: connected, IP %s\n", WiFi.localIP().toString().c_str());
        if (gotifyUsesTls())
        {
            syncClock();
        }
    }
    else
    {
        Serial.println("WiFi: connection failed (timeout)");
    }
}

bool pushBackingOff()
{
    return hadFailure && millis() - lastFailureMs < GOTIFY_RETRY_BACKOFF_MS;
}

bool pushGotify(const String &message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("gotify: skipped, WiFi not connected");
        return false;
    }
    if (pushBackingOff())
    {
        return false; // detectors will retry after the backoff
    }

    const String url = String(GOTIFY_URL) + "/message";
    HTTPClient http;
    bool began;
    WiFiClientSecure secureClient;
    if (gotifyUsesTls())
    {
        secureClient.setCACert(ISRG_ROOT_X1_PEM);
        secureClient.setTimeout(GOTIFY_TIMEOUT_MS / 1000); // this overload takes seconds
        began = http.begin(secureClient, url);
    }
    else
    {
        began = http.begin(url);
    }
    if (!began)
    {
        Serial.println("gotify: http.begin failed");
        lastFailureMs = millis();
        hadFailure = true;
        return false;
    }

    // The push runs on the sensor loop; an unreachable server must not stall it
    // for longer than this.
    http.setConnectTimeout(GOTIFY_TIMEOUT_MS);
    http.setTimeout(GOTIFY_TIMEOUT_MS);
    http.addHeader("X-Gotify-Key", GOTIFY_TOKEN);
    http.addHeader("Content-Type", "application/json");
    const String body = "{\"title\":\"ESP32 Room Watchdog\",\"message\":\"" +
                        jsonEscape(message) + "\",\"priority\":5}";
    const int code = http.POST(body);
    http.end();

    Serial.printf("gotify: POST -> %d\n", code);
    const bool ok = code >= 200 && code < 300;
    if (!ok)
    {
        lastFailureMs = millis();
        hadFailure = true;
    }
    return ok;
}
