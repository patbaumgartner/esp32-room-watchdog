#include "mic.h"

#include <Arduino.h>
#include <PcmEncoder.h>

#include <esp_adc/adc_continuous.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <soc/soc_caps.h>

#include <string.h>

#include "config.h"

namespace
{
    constexpr size_t DMA_READ_BYTES = 2048;
    constexpr size_t DMA_STORE_BYTES = DMA_READ_BYTES * 4;
    constexpr size_t DMA_SAMPLES = DMA_READ_BYTES / SOC_ADC_DIGI_RESULT_BYTES;
    constexpr uint32_t WINDOW_SAMPLES =
        AUDIO_SAMPLE_RATE_HZ * SOUND_SAMPLE_WINDOW_MS / 1000;

    static_assert(AUDIO_SAMPLE_RATE_HZ * SOUND_SAMPLE_WINDOW_MS % 1000 == 0,
                  "Sound window must contain a whole number of samples");

    LevelWindow lastWindow;
    QueueHandle_t windowQueue = nullptr;
    SemaphoreHandle_t pcmAvailable = nullptr;
    portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
    adc_continuous_handle_t adcHandle = nullptr;
    adc_channel_t micAdcChannel = ADC_CHANNEL_0;

    alignas(4) uint8_t dmaBuffer[DMA_READ_BYTES];
    int16_t pcmBuffer[DMA_SAMPLES];

    int16_t pcmRing[AUDIO_STREAM_BUFFER_SAMPLES];
    size_t pcmReadIndex = 0;
    size_t pcmWriteIndex = 0;
    size_t pcmCount = 0;
    bool pcmStreaming = false;
    uint32_t droppedSamples = 0;

    void publishWindow(const LevelWindow &window)
    {
        portENTER_CRITICAL(&stateMux);
        lastWindow = window;
        portEXIT_CRITICAL(&stateMux);
        xQueueOverwrite(windowQueue, &window);
    }

    void appendPcm(const int16_t *samples, size_t sampleCount)
    {
        bool added = false;
        portENTER_CRITICAL(&stateMux);
        if (pcmStreaming)
        {
            const size_t freeSamples = AUDIO_STREAM_BUFFER_SAMPLES - pcmCount;
            const size_t accepted = sampleCount < freeSamples ? sampleCount : freeSamples;
            const size_t firstPart = accepted < AUDIO_STREAM_BUFFER_SAMPLES - pcmWriteIndex
                                         ? accepted
                                         : AUDIO_STREAM_BUFFER_SAMPLES - pcmWriteIndex;
            memcpy(&pcmRing[pcmWriteIndex], samples, firstPart * sizeof(int16_t));
            memcpy(pcmRing, samples + firstPart, (accepted - firstPart) * sizeof(int16_t));
            pcmWriteIndex = (pcmWriteIndex + accepted) % AUDIO_STREAM_BUFFER_SAMPLES;
            pcmCount += accepted;
            droppedSamples += sampleCount - accepted;
            added = accepted > 0;
        }
        portEXIT_CRITICAL(&stateMux);

        if (added)
        {
            // cppcheck-suppress cstyleCast
            xSemaphoreGive(pcmAvailable);
        }
    }

    void micAdcTask(void *)
    {
        PcmEncoder encoder;
        LevelWindow window;
        uint32_t samplesInWindow = 0;
        bool overflowReported = false;

        while (true)
        {
            uint32_t bytesRead = 0;
            const esp_err_t result = adc_continuous_read(
                adcHandle, dmaBuffer, sizeof(dmaBuffer), &bytesRead, 1000);
            if (result == ESP_ERR_TIMEOUT)
            {
                continue;
            }
            if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
            {
                Serial.printf("mic: ADC read failed: %s\n", esp_err_to_name(result));
                continue;
            }
            if (result == ESP_ERR_INVALID_STATE && !overflowReported)
            {
                Serial.println("mic: ADC DMA overflow; samples were lost");
                overflowReported = true;
            }

            size_t pcmSamples = 0;
            for (size_t offset = 0;
                 offset + SOC_ADC_DIGI_RESULT_BYTES <= bytesRead;
                 offset += SOC_ADC_DIGI_RESULT_BYTES)
            {
                const auto *sample = reinterpret_cast<const adc_digi_output_data_t *>(
                    dmaBuffer + offset);
                if (sample->type2.unit != ADC_UNIT_1 ||
                    sample->type2.channel != micAdcChannel)
                {
                    continue;
                }

                const uint16_t raw = sample->type2.data;
                window.add(raw);
                pcmBuffer[pcmSamples++] = encoder.encode(raw);

                if (++samplesInWindow == WINDOW_SAMPLES)
                {
                    publishWindow(window);
                    window.reset();
                    samplesInWindow = 0;
                }
            }
            appendPcm(pcmBuffer, pcmSamples);
        }
    }

    bool configureAdc()
    {
        adc_unit_t adcUnit = ADC_UNIT_1;
        esp_err_t result = adc_continuous_io_to_channel(
            PIN_MIC_OUT, &adcUnit, &micAdcChannel);
        if (result != ESP_OK || adcUnit != ADC_UNIT_1)
        {
            Serial.printf("mic: GPIO%d is not an ADC1 channel\n", PIN_MIC_OUT);
            return false;
        }

        adc_continuous_handle_cfg_t handleConfig = {};
        handleConfig.max_store_buf_size = DMA_STORE_BYTES;
        handleConfig.conv_frame_size = DMA_READ_BYTES;
        result = adc_continuous_new_handle(&handleConfig, &adcHandle);
        if (result != ESP_OK)
        {
            Serial.printf("mic: ADC init failed: %s\n", esp_err_to_name(result));
            return false;
        }

        adc_digi_pattern_config_t pattern = {};
        pattern.atten = ADC_ATTEN_DB_12;
        pattern.channel = micAdcChannel;
        pattern.unit = adcUnit;
        pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        adc_continuous_config_t adcConfig = {};
        adcConfig.pattern_num = 1;
        adcConfig.adc_pattern = &pattern;
        adcConfig.sample_freq_hz = AUDIO_SAMPLE_RATE_HZ;
        adcConfig.conv_mode = ADC_CONV_SINGLE_UNIT_1;
        adcConfig.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
        result = adc_continuous_config(adcHandle, &adcConfig);
        if (result == ESP_OK)
        {
            result = adc_continuous_start(adcHandle);
        }
        if (result != ESP_OK)
        {
            Serial.printf("mic: ADC start failed: %s\n", esp_err_to_name(result));
            adc_continuous_deinit(adcHandle);
            adcHandle = nullptr;
            return false;
        }
        return true;
    }
}

bool micBegin()
{
    windowQueue = xQueueCreate(1, sizeof(LevelWindow));
    pcmAvailable = xSemaphoreCreateBinary();
    if (windowQueue == nullptr || pcmAvailable == nullptr)
    {
        Serial.println("mic: buffer allocation failed");
        return false;
    }
    if (!configureAdc())
    {
        return false;
    }
    if (xTaskCreate(micAdcTask, "mic-adc", 4096, nullptr, 3, nullptr) != pdPASS)
    {
        Serial.println("mic: task allocation failed");
        adc_continuous_stop(adcHandle);
        adc_continuous_deinit(adcHandle);
        adcHandle = nullptr;
        return false;
    }

    Serial.printf("mic: sampling %luHz mono PCM\n", AUDIO_SAMPLE_RATE_HZ);
    return true;
}

LevelWindow micSampleWindow()
{
    LevelWindow window;
    if (windowQueue != nullptr)
    {
        xQueueReceive(windowQueue, &window, portMAX_DELAY);
    }
    return window;
}

LevelWindow micLastWindow()
{
    portENTER_CRITICAL(&stateMux);
    const LevelWindow window = lastWindow;
    portEXIT_CRITICAL(&stateMux);
    return window;
}

bool micStartPcmStream()
{
    portENTER_CRITICAL(&stateMux);
    if (pcmStreaming)
    {
        portEXIT_CRITICAL(&stateMux);
        return false;
    }
    pcmReadIndex = 0;
    pcmWriteIndex = 0;
    pcmCount = 0;
    droppedSamples = 0;
    pcmStreaming = true;
    portEXIT_CRITICAL(&stateMux);
    xSemaphoreTake(pcmAvailable, 0);
    return true;
}

void micStopPcmStream()
{
    portENTER_CRITICAL(&stateMux);
    pcmStreaming = false;
    pcmCount = 0;
    portEXIT_CRITICAL(&stateMux);
    // cppcheck-suppress cstyleCast
    xSemaphoreGive(pcmAvailable);
}

size_t micReadPcm(int16_t *samples, size_t maxSamples)
{
    while (samples != nullptr && maxSamples > 0)
    {
        portENTER_CRITICAL(&stateMux);
        const size_t available = pcmCount < maxSamples ? pcmCount : maxSamples;
        const size_t firstPart = available < AUDIO_STREAM_BUFFER_SAMPLES - pcmReadIndex
                                     ? available
                                     : AUDIO_STREAM_BUFFER_SAMPLES - pcmReadIndex;
        memcpy(samples, &pcmRing[pcmReadIndex], firstPart * sizeof(int16_t));
        memcpy(samples + firstPart, pcmRing, (available - firstPart) * sizeof(int16_t));
        pcmReadIndex = (pcmReadIndex + available) % AUDIO_STREAM_BUFFER_SAMPLES;
        pcmCount -= available;
        const bool active = pcmStreaming;
        portEXIT_CRITICAL(&stateMux);

        if (available > 0 || !active)
        {
            return available;
        }
        xSemaphoreTake(pcmAvailable, pdMS_TO_TICKS(1000));
    }
    return 0;
}

bool micPcmStreaming()
{
    portENTER_CRITICAL(&stateMux);
    const bool active = pcmStreaming;
    portEXIT_CRITICAL(&stateMux);
    return active;
}

uint32_t micDroppedSamples()
{
    portENTER_CRITICAL(&stateMux);
    const uint32_t dropped = droppedSamples;
    portEXIT_CRITICAL(&stateMux);
    return dropped;
}
