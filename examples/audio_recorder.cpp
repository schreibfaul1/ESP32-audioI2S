
// example AUDIO RECORDER
// starts an audio stream and records 10 seconds of it on the SD card.


#include "Arduino.h"
#include "Audio.h"
#include "WiFiMulti.h"

//--------------- recorder --------------------------------------------------------------------------------------------------------
struct WAVHeader {
    char     riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t size;
    char     wave[4] = {'W', 'A', 'V', 'E'};
    char     fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t format = 1;
    uint16_t channels = 2;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bits = 32;
    char     data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};

constexpr size_t REC_BUFFER_SIZE = 64 * 1024; // 64 KB
ps_ptr<uint8_t>  rec_buffer;

class AudioRecorder {

  public:
    volatile size_t writePos = 0;
    volatile size_t readPos = 0;
    uint32_t        totalBytes = 0;
    uint16_t        sampleRate = 44100;

    volatile bool startRequested = false;
    volatile bool stopRequested = false;
    volatile bool running = false;

    bool push(const void* data, size_t len) {
        size_t free = (readPos + REC_BUFFER_SIZE - writePos - 1) % REC_BUFFER_SIZE;

        if (len > free) return false; // overflow → verwerfen

        const uint8_t* src = (const uint8_t*)data;

        for (size_t i = 0; i < len; i++) {
            rec_buffer[writePos] = src[i];
            writePos = (writePos + 1) % REC_BUFFER_SIZE;
        }
        return true;
    }

    size_t pop(uint8_t*& ptr) {
        if (readPos == writePos) return 0;

        ptr = &rec_buffer[readPos];

        if (writePos > readPos) return writePos - readPos;

        return REC_BUFFER_SIZE - readPos;
    }
};
//--------------------------------------------------------------------------------------------------------------------------------

AudioRecorder recorder;
Audio         audio;
WiFiMulti     wifiMulti;

void wavWriterTask(void*) {
    File      file;
    WAVHeader hdr;
    bool      open = false;

    while (true) {

        if (recorder.startRequested && !open) {
            recorder.startRequested = false;

            file = SD_MMC.open("/recording.wav", FILE_WRITE);
            if (!file) continue;

            hdr.sampleRate = recorder.sampleRate;
            hdr.byteRate = hdr.sampleRate * 2 * 4;
            hdr.blockAlign = 2 * 4;
            hdr.dataSize = 0;
            hdr.size = 36;

            file.write((uint8_t*)&hdr, sizeof(hdr));
            recorder.totalBytes = 0;
            open = true;
        }

        uint8_t* ptr;
        size_t   avail = recorder.pop(ptr);

        if (avail && open) {
            file.write(ptr, avail);
            recorder.totalBytes += avail;
            recorder.readPos = (recorder.readPos + avail) % REC_BUFFER_SIZE;
        }

        if (recorder.stopRequested && open) {
            recorder.stopRequested = false;

            hdr.dataSize = recorder.totalBytes;
            hdr.sampleRate = recorder.sampleRate;
            hdr.byteRate = hdr.sampleRate * 2 * 4;
            hdr.size = recorder.totalBytes + 36;

            file.seek(0);
            file.write((uint8_t*)&hdr, sizeof(hdr));
            file.close();
            open = false;
        }

        vTaskDelay(1);
    }
}
//--------------------------------------------------------------------------------------------------------------------------------

#define I2S_DOUT   25
#define I2S_BCLK   27
#define I2S_LRC    26
#define SD_MMC_D0  2
#define SD_MMC_CLK 14
#define SD_MMC_CMD 15

String ssid = "****";
String password = "****";

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Serial.begin(115200);

    rec_buffer.alloc_array(REC_BUFFER_SIZE, "rec_buffer");                             // allocate in PSRAM
    xTaskCreatePinnedToCore(wavWriterTask, "wavWriter", 4096, nullptr, 1, nullptr, 0); // start recorder task
    Serial.printf("recorder task started, Free heap: %u\n", ESP.getFreeHeap());

    Audio::audio_info_callback = my_audio_info;

    wifiMulti.addAP(ssid.c_str(), password.c_str());
    wifiMulti.run(); // if there are multiple access points, use the strongest one
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    SD_MMC.begin("/sdcard", true);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(20);                                                                                // default 0...21
    audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
}

uint32_t t = 0;
uint32_t recording_time = 10;
void     loop() {
    audio.loop();
    vTaskDelay(1);
    if (audio.isRunning()) {
        if (!recorder.running) {
            t = millis();
            recorder.startRequested = true;
            recorder.running = true;
            Serial.println("Start recording");
        }
    }

    if (t + 10000 < millis()) {
        if (audio.isRunning()) {
            recorder.sampleRate = audio.getSampleRate();
            audio.stopSong();
            Serial.println("Stop recording");
            recorder.stopRequested = true;
            recorder.running = false;
        }
    }
}

void audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S) {
    if (recorder.running == true) {
        size_t bytes = validSamples * 2 * sizeof(int32_t);
        recorder.push(outBuff, bytes); // memcpy only
    }
}