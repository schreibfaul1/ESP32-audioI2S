#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s_std.h>
#include "esp_log.h"


class AudioResampleBuffer {
public:
    static constexpr size_t FIFO_SIZE_BYTES = 16384; // Muss Vielfaches von 4 sein (Stereo, 16 Bit)
    static constexpr size_t I2S_BLOCK_SIZE = 512;    // DMA Blockgröße


    AudioResampleBuffer()
        :fifoWrite(0), fifoRead(0), m_resampleCursor(0.0f) {
        memset(fifo, 0, sizeof(fifo));
        memset(m_inputHistory, 0, sizeof(m_inputHistory));
    }
    void setChannelHandle(i2s_chan_handle_t i2sHandle){
        m_i2s = i2sHandle;
    }

    // Set the input samplerates (updated by the LRCK monitoring)
    void setInputSamplerate(uint32_t samplerate) {
        if (samplerate == 8000 || samplerate == 22050 || samplerate == 44100 || samplerate == 48000) {
            m_sampleRate = samplerate;
            ESP_LOGI("ResampleBuffer", "Input samplerate set to %u Hz", samplerate);
        } else {
            ESP_LOGW("ResampleBuffer", "Invalid samplerate %u Hz, defaulting to 44100 Hz", samplerate);
            m_sampleRate = 44100;
        }
    }

    // Muss zyklisch aufgerufen werden (z. B. aus Task)
    void loopResample() {
        alignas(4) uint8_t i2sBuf[I2S_BLOCK_SIZE];
        size_t bytesRead = 0;
        if (i2s_channel_read(m_i2s, i2sBuf, I2S_BLOCK_SIZE, &bytesRead, 50) != ESP_OK || bytesRead == 0)
            return;

        size_t inSamples = bytesRead / 4; // Stereo, 16 Bit
        int16_t* inData = reinterpret_cast<int16_t*>(i2sBuf);

        int16_t resampled[1024];
        size_t outSamples = resampleTo441Stereo(inData, inSamples, resampled);
        size_t outBytes = outSamples * 4;

        if (fifoFree() >= outBytes) {
            fifoWriteBytes(reinterpret_cast<uint8_t*>(resampled), outBytes);
        } else {
            vTaskDelay(100);
            // ESP_LOGW("ResampleBuffer", "FIFO voll, Daten verworfen %i Bytes",  outBytes - fifoFree());
        }
    }

    // Bluetooth Callback: muss exakt "bytes" liefern
    int32_t getData(uint8_t* data, int32_t bytes) {
        while (fifoAvailable() < static_cast<size_t>(bytes)) {
            vTaskDelay(1);
        }
        fifoReadBytes(data, bytes);
        return bytes;
    }

private:
private:
    i2s_chan_handle_t m_i2s;
    uint8_t fifo[FIFO_SIZE_BYTES];
    size_t fifoWrite;
    size_t fifoRead;
    float m_sampleRate = 44100.0f;

    float m_resampleCursor;
    int16_t m_inputHistory[6]; // 3 Stereo-Samples

    size_t fifoAvailable() const {
        return (fifoWrite + FIFO_SIZE_BYTES - fifoRead) % FIFO_SIZE_BYTES;
    }

    size_t fifoFree() const {
        return FIFO_SIZE_BYTES - fifoAvailable() - 1;
    }

    void fifoWriteBytes(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            fifo[fifoWrite] = data[i];
            fifoWrite = (fifoWrite + 1) % FIFO_SIZE_BYTES;
        }
    }

    void fifoReadBytes(uint8_t* out, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            out[i] = fifo[fifoRead];
            fifoRead = (fifoRead + 1) % FIFO_SIZE_BYTES;
        }
    }

    // Catmull-Rom Spline Resampling von 48 kHz auf 44,1 kHz
    size_t resampleTo441Stereo(const int16_t* input, size_t inputSamples, int16_t* output) {
        float ratio = m_sampleRate / 44100.0f;
        float cursor = m_resampleCursor;

        size_t extendedSamples = inputSamples + 3;
        std::vector<int16_t> extendedInput(extendedSamples * 2);

        memcpy(&extendedInput[0], m_inputHistory, 6 * sizeof(int16_t));
        memcpy(&extendedInput[6], input, inputSamples * 2 * sizeof(int16_t));

        size_t outputIndex = 0;

        auto catmullRom = [](float t, float xm1, float x0, float x1, float x2) {
            return 0.5f * (
                (2.0f * x0) +
                (-xm1 + x1) * t +
                (2.0f * xm1 - 5.0f * x0 + 4.0f * x1 - x2) * t * t +
                (-xm1 + 3.0f * x0 - 3.0f * x1 + x2) * t * t * t
            );
        };

        auto clip = [](float v) -> int16_t {
            return v > 32767.0f ? 32767 : (v < -32768.0f ? -32768 : static_cast<int16_t>(v));
        };

        for (size_t inIdx = 1; inIdx < extendedSamples - 2; ++inIdx) {
            int16_t xm1_l = extendedInput[(inIdx - 1) * 2];
            int16_t x0_l  = extendedInput[(inIdx + 0) * 2];
            int16_t x1_l  = extendedInput[(inIdx + 1) * 2];
            int16_t x2_l  = extendedInput[(inIdx + 2) * 2];

            int16_t xm1_r = extendedInput[(inIdx - 1) * 2 + 1];
            int16_t x0_r  = extendedInput[(inIdx + 0) * 2 + 1];
            int16_t x1_r  = extendedInput[(inIdx + 1) * 2 + 1];
            int16_t x2_r  = extendedInput[(inIdx + 2) * 2 + 1];

            while (cursor < 1.0f) {
                float t = cursor;
                output[outputIndex * 2]     = clip(catmullRom(t, xm1_l, x0_l, x1_l, x2_l));
                output[outputIndex * 2 + 1] = clip(catmullRom(t, xm1_r, x0_r, x1_r, x2_r));
                ++outputIndex;
                cursor += ratio;
            }
            cursor -= 1.0f;
        }

        // Historie sichern
        for (int i = 0; i < 3; ++i) {
            size_t idx = inputSamples - 3 + i;
            m_inputHistory[i * 2]     = input[idx * 2];
            m_inputHistory[i * 2 + 1] = input[idx * 2 + 1];
        }

        m_resampleCursor = cursor;
        return outputIndex;
    }
};
