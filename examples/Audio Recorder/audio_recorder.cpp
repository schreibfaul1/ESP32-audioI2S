
// example AUDIO RECORDER
// starts an audio stream and records 10 seconds of it on the SD card.


#include "Arduino.h"
#include "Audio.h"
#include "WiFiMulti.h"
#include <atomic>
#include <algorithm>

Audio     audio;
WiFiMulti wifiMulti;

//______________________________________________________________________________________________________________________________________________________________________________________________________
//   A U D I O R E C O R D E R
//______________________________________________________________________________________________________________________________________________________________________________________________________

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
    uint16_t bits;
    char     data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};

constexpr size_t REC_BUFFER_SIZE = 512 * 1024; // 512KB für 2-3 Sekunden Puffer
constexpr size_t WRITE_CHUNK_SIZE = 10242;      // not too big!
constexpr size_t SD_FLUSH_INTERVAL = 65536;    // Alle 64KB flush
ps_ptr<uint8_t>  rec_buffer;
ps_ptr<uint8_t>  writeBuffer;

class AudioRecorder {
  public:
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};
    uint32_t            totalBytes = 0;
    uint16_t            sampleRate = 44100;
    uint32_t            overflowCount = 0;

    volatile bool startRequested = false;
    volatile bool stopRequested = false;
    volatile bool running = false;

    bool push16(const int32_t* data, size_t frames) {
        // frames = Stereo-Frames
        size_t bytes16 = frames * 2 * sizeof(int16_t);

        size_t currentWrite = writePos.load(std::memory_order_relaxed);
        size_t currentRead = readPos.load(std::memory_order_acquire);

        size_t free = (currentRead + REC_BUFFER_SIZE - currentWrite - 1) % REC_BUFFER_SIZE;

        if (bytes16 > free) {
            overflowCount++;
            return false;
        }

        for (size_t i = 0; i < frames * 2; i++) {
            // 32 → 16 Bit (High word)
            int32_t v = data[i] >> 16;

            // Optional Clipping (save)
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;

            int16_t s = (int16_t)v;

            // Write byte by byte (LE)
            rec_buffer[currentWrite] = s & 0xFF;
            currentWrite = (currentWrite + 1) % REC_BUFFER_SIZE;
            rec_buffer[currentWrite] = (s >> 8) & 0xFF;
            currentWrite = (currentWrite + 1) % REC_BUFFER_SIZE;
        }
        writePos.store(currentWrite, std::memory_order_release);
        return true;
    }

    // Copies data to dest, returns bytes actually read
    size_t pop(uint8_t* dest, size_t maxLen) {
        size_t currentRead = readPos.load(std::memory_order_relaxed);
        size_t currentWrite = writePos.load(std::memory_order_acquire);

        if (currentRead == currentWrite) return 0;

        size_t avail = (currentWrite > currentRead) ? (currentWrite - currentRead) : (REC_BUFFER_SIZE - currentRead);

        size_t toRead = std::min(avail, maxLen);

        // Wrap-around handling
        size_t firstChunk = std::min(toRead, REC_BUFFER_SIZE - currentRead);
        memcpy(dest, &rec_buffer[currentRead], firstChunk);
        if (toRead > firstChunk) { memcpy(dest + firstChunk, &rec_buffer[0], toRead - firstChunk); }

        readPos.store((currentRead + toRead) % REC_BUFFER_SIZE, std::memory_order_release);
        return toRead;
    }

    // For external access to buffers (e.g. for pop with pointer math, but not recommended)
    size_t available() {
        size_t w = writePos.load(std::memory_order_acquire);
        size_t r = readPos.load(std::memory_order_acquire);
        return (w >= r) ? (w - r) : (REC_BUFFER_SIZE - r + w);
    }
};

AudioRecorder recorder;

void wavWriterTask(void*) {
    File      file;
    WAVHeader hdr;
    bool      fileOpen = false;

    size_t   writeBufferFill = 0;
    uint32_t bytesSinceFlush = 0;

    while (true) {
        // --- START REQUEST ---
        if (recorder.startRequested && !fileOpen) {
            recorder.startRequested = false;

            // Datei mit Zeitstempel erstellen
            char filename[64];
            snprintf(filename, sizeof(filename), "/recording.wav");

            file = SD_MMC.open(filename, FILE_WRITE);
            if (!file) {
                Serial.println("Failed to open file! \"/recording.wav\"");
                continue;
            }
            // prepeare header
            hdr.sampleRate = recorder.sampleRate;
            hdr.byteRate = recorder.sampleRate * 2 * 2; // Stereo, 16-bit
            hdr.blockAlign = 2 * 2;                     // 8 bytes per frame
            hdr.bits = 16;
            hdr.dataSize = 0;
            hdr.size = 36; // 44 - 8 (RIFF header)

            file.write((uint8_t*)&hdr, sizeof(hdr));
            recorder.totalBytes = 0;
            writeBufferFill = 0;
            bytesSinceFlush = 0;
            fileOpen = true;
            recorder.running = true;

            Serial.printf("Recording started: %s\n", filename);
        }

        // --- WRITE DATA ---
        if (fileOpen) {
            // fill local buffer
            while (writeBufferFill < WRITE_CHUNK_SIZE) {
                size_t spaceInLocalBuffer = WRITE_CHUNK_SIZE - writeBufferFill;
                size_t bytesRead = recorder.pop(writeBuffer + writeBufferFill, spaceInLocalBuffer);

                if (bytesRead == 0) break; // ringbuffer is empty

                writeBufferFill += bytesRead;
            }

            // write full block to SD
            if (writeBufferFill >= WRITE_CHUNK_SIZE) {
                size_t written = file.write(writeBuffer.get(), WRITE_CHUNK_SIZE);
                if (written != WRITE_CHUNK_SIZE) {
                    Serial.println("SD write error!");
                    // Optional: error handling, hold buffer?
                }

                recorder.totalBytes += written;
                bytesSinceFlush += written;
                writeBufferFill = 0; // buffer is empty (or move remaining data)

                // Periodic flush for data integrity
                if (bytesSinceFlush >= SD_FLUSH_INTERVAL) {
                    file.flush();
                    bytesSinceFlush = 0;
                }
            }
        }

        // --- STOP REQUEST ---
        if (recorder.stopRequested && fileOpen) {
            recorder.stopRequested = false;

            // Write remaining data to local buffer
            if (writeBufferFill > 0) {
                file.write(writeBuffer.get(), writeBufferFill);
                recorder.totalBytes += writeBufferFill;
            }

            // Update header
            hdr.dataSize = recorder.totalBytes;
            hdr.size = recorder.totalBytes + 36;

            file.seek(0);
            file.write((uint8_t*)&hdr, sizeof(hdr));
            file.flush();
            file.close();

            fileOpen = false;
            writeBufferFill = 0;
            recorder.running = false;
            Serial.printf("Recording stopped. Total bytes: %u, Overflows: %u\n", recorder.totalBytes, recorder.overflowCount);
        }

        // Small delay to feed watchdog and release CPU
        // But not too long, so that the ring buffer does not overflow!
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms = ~176 Bytes bei 44.1kHz Stereo 32-bit
    }
}

//______________________________________________________________________________________________________________________________________________________________________________________________________

#define I2S_DOUT   25
#define I2S_BCLK   27
#define I2S_LRC    26
#define SD_MMC_D0  2
#define SD_MMC_CLK 14
#define SD_MMC_CMD 15

String ssid =     "*****";
String password = "*****";

uint32_t t;
enum {IDLE, RECORDING, FINISH, PLAYBACK};
uint8_t state = IDLE;

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Serial.begin(115200);
    Audio::audio_info_callback = my_audio_info;
    Serial.print("\n\n");

    wifiMulti.addAP(ssid.c_str(), password.c_str());
    wifiMulti.run(); // if there are multiple access points, use the strongest one
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    SD_MMC.begin("/sdcard", true);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(20);                                                                                // default 0...21
    audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac

    rec_buffer.alloc_array(REC_BUFFER_SIZE, "rec_buffer");                             // allocate in PSRAM
    writeBuffer.alloc_array(WRITE_CHUNK_SIZE, "writeBuffer");                          // allocate in PSRAM
    xTaskCreatePinnedToCore(wavWriterTask, "wavWriter", 4096, nullptr, 1, nullptr, 0); // start recorder task
    Serial.printf("recorder task started, Free heap: %u\n", ESP.getFreeHeap());
    t = millis();
}

void loop() {
    audio.loop();
    vTaskDelay(1);
    if (t + 5000 < millis() && state == IDLE && audio.isRunning()) {
        Serial.println("start recording");
        state = RECORDING;
        recorder.sampleRate = audio.getSampleRate();
        recorder.startRequested = true;
    }
    if (t + 15000 < millis() && state == RECORDING) {
        Serial.println("stop recording");
        state = FINISH;
        recorder.stopRequested = true;
        audio.stopSong();
    }
    if (t + 17000 < millis() && state == FINISH) {
        state = PLAYBACK;
        audio.connecttoFS(SD_MMC, "/recording.wav");
    }
    if (t + 29000 < millis() && state == PLAYBACK && !audio.isRunning()) {
        state = IDLE;
        audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/");
        t = millis();
    }
}
//______________________________________________________________________________________________________________________________________________________________________________________________________
void audio_process_raw_samples(int32_t* outBuff, int16_t validSamples) { // samples are available, write in wav file
    if (recorder.running == true) {
        recorder.push16(outBuff, validSamples);
    }
}
