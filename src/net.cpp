#include "net.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <time.h>
#include <string.h>

#include <NotificationQueue.h>

#include "certs.h"
#include "config.h"
#include "json_escape.h"
#include "secrets.h"

namespace
{
    // Written by the delivery worker, read from the API and sensor-loop tasks.
    // undeliverable is also incremented by queueGotify() on a third task, so a
    // plain ++ could lose a count.
    std::atomic<uint32_t> lastFailureMs{0};
    std::atomic<bool> hadFailure{false};
    std::atomic<uint32_t> undeliverable{0};

    NotificationQueue<GOTIFY_QUEUE_DEPTH, GOTIFY_MESSAGE_MAX> pending;
    SemaphoreHandle_t queueLock = nullptr;
    SemaphoreHandle_t queueSignal = nullptr;

    void logWifiDisconnect(arduino_event_id_t, arduino_event_info_t info)
    {
        Serial.printf("WiFi: disconnected (reason %u)\n",
                      info.wifi_sta_disconnected.reason);
    }

    bool gotifyUsesTls()
    {
        return strncmp(GOTIFY_URL, "https:", 6) == 0;
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

    // Runs on the delivery worker only. Blocking here is fine; blocking on the
    // sensor loop was not, which is why the queue exists.
    bool pushGotify(const char *message)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("gotify: skipped, WiFi not connected");
            lastFailureMs = millis();
            hadFailure = true;
            return false;
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
        if (ok)
        {
            hadFailure = false;
        }
        else
        {
            lastFailureMs = millis();
            hadFailure = true;
        }
        return ok;
    }

    bool takeNextMessage(char *message, size_t size)
    {
        if (xSemaphoreTake(queueLock, portMAX_DELAY) != pdTRUE)
        {
            return false;
        }
        const bool taken = pending.pop(message, size);
        xSemaphoreGive(queueLock);
        return taken;
    }

    void gotifyWorkerTask(void *)
    {
        char message[GOTIFY_MESSAGE_MAX];
        while (true)
        {
            if (!takeNextMessage(message, sizeof(message)))
            {
                xSemaphoreTake(queueSignal, pdMS_TO_TICKS(1000));
                continue;
            }

            for (uint8_t attempt = 1; !pushGotify(message); ++attempt)
            {
                if (attempt >= GOTIFY_MAX_ATTEMPTS)
                {
                    Serial.printf("gotify: gave up on \"%s\"\n", message);
                    ++undeliverable;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(GOTIFY_RETRY_BACKOFF_MS));
            }
        }
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

void mdnsBegin()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("mDNS: skipped, WiFi not connected");
        return;
    }
    if (!MDNS.begin(MDNS_HOSTNAME))
    {
        Serial.println("mDNS: responder failed to start");
        return;
    }
    MDNS.addService("http", "tcp", API_PORT);
    MDNS.addService("ws", "tcp", API_PORT);
    Serial.printf("mDNS: http://%s.local and ws://%s.local/ws\n",
                  MDNS_HOSTNAME, MDNS_HOSTNAME);
}

void gotifyBegin()
{
    queueLock = xSemaphoreCreateMutex();
    queueSignal = xSemaphoreCreateBinary();
    if (queueLock == nullptr || queueSignal == nullptr ||
        xTaskCreate(gotifyWorkerTask, "gotify", 8192, nullptr, 1, nullptr) != pdPASS)
    {
        Serial.println("gotify: delivery worker allocation failed; pushes are disabled");
        queueLock = nullptr;
        return;
    }
    Serial.printf("gotify: delivery worker ready (queue depth %u)\n",
                  static_cast<unsigned>(GOTIFY_QUEUE_DEPTH));
}

void queueGotify(const String &message)
{
    if (queueLock == nullptr)
    {
        return; // no worker; the WebSocket still carries the event
    }
    if (xSemaphoreTake(queueLock, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ++undeliverable;
        return;
    }
    const bool keptEverything = pending.push(message.c_str());
    xSemaphoreGive(queueLock);
    if (!keptEverything)
    {
        Serial.println("gotify: queue full, discarded the oldest pending message");
    }
    // cppcheck-suppress cstyleCast
    xSemaphoreGive(queueSignal);
}

bool pushBackingOff()
{
    return hadFailure && millis() - lastFailureMs < GOTIFY_RETRY_BACKOFF_MS;
}

uint32_t pushLostCount()
{
    uint32_t discarded = 0;
    if (queueLock != nullptr && xSemaphoreTake(queueLock, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        discarded = pending.dropped();
        xSemaphoreGive(queueLock);
    }
    return discarded + undeliverable;
}
