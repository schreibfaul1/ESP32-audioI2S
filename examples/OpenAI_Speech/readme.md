# OpenAI Speech

### platformio.ini - example for: [XIAO ESP32S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)

```ShellCheck Config
[env:seeed_xiao_esp32s3]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200
build_flags = 
	-Wall
	-Wextra
	-DCORE_DEBUG_LEVEL=3
	-DBOARD_HAS_PSRAM
    -DAUDIO_LOG
	-DARDUINO_RUNNING_CORE=1       ; Arduino Runs On Core (setup, loop)
	-DARDUINO_EVENT_RUNNING_CORE=1 ; Events Run On Core
lib_deps = 
	https://github.com/schreibfaul1/ESP32-audioI2S.git
```

### main.cpp - using xTask example:

```cpp
#include <Arduino.h>
#include "SPI.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include "Audio.h"

// WiFi credentials
#define WIFI_SSID "<YOUR_WIFI_SSID>"
#define PASSWORD "<YOUR_WIFI_PASSWORD>"
#define OPENAI_API_KEY "<YOUR_OPENAI_API_KEY>"

// Configure I2S pins
#define I2S_LRC D1
#define I2S_DOUT D2
#define I2S_BCLK D3
#define I2S_MCLK 0

// Vars
bool isWIFIConnected;

String result = "Added OpenAI Text to speech API support";

// Inits
WiFiMulti wifiMulti;
TaskHandle_t playaudio_handle;
QueueHandle_t audioQueue;
Audio audio;

// Declaration
void audio_info(const char *info);
void wifiConnect(void *pvParameters);
void playaudio(void *pvParameters);

// Default
void setup() {
    Serial.begin(115200);
    isWIFIConnected = false;

    // Create queue
    audioQueue = xQueueCreate(1, sizeof(int));
    if (audioQueue == NULL) {
        Serial.println("Failed to create audioQueue");
        while(1);
    }

    // Create tasks
    xTaskCreate(wifiConnect, "wifi_Connect", 4096, NULL, 0, NULL);
    delay(500);
    xTaskCreate(playaudio, "playaudio", 1024 * 8, NULL, 3, &playaudio_handle);
}

void loop(void) {
    audio.loop();
}

void audio_info(const char *info) {
    Serial.print("audio_info: ");
    Serial.println(info);
}

void wifiConnect(void *pvParameters) {
    while(1) {
        if (!isWIFIConnected) {
            wifiMulti.addAP(WIFI_SSID, PASSWORD);
            Serial.println("Connecting to WiFi...");
            while (wifiMulti.run() != WL_CONNECTED) {
                vTaskDelay(500);
            }
            Serial.print("Connected to WiFi\nIP: ");
            Serial.println(WiFi.localIP());
            isWIFIConnected = true;

            Serial.println("Sending result...");
            int eventMessage;
            if (xQueueSend(audioQueue, &eventMessage, 0) != pdPASS) {
                Serial.println("Failed to send result to queue");
            }
        } else {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

void playaudio(void *pvParameters) {
    while(1) {
        if (isWIFIConnected && audioQueue != 0) {
            int eventMessage;
            Serial.println("Waiting for result...");
            if (xQueueReceive(audioQueue, &eventMessage, portMAX_DELAY) == pdPASS) {
                Serial.print("Received result: ");
                Serial.println(result);

                // Speech
                audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, -1);
                audio.setVolume(15); // 0...21
                audio.openai_speech(OPENAI_API_KEY, "tts-1", result, "shimmer", "mp3", "1");
            }
        } else {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}
```
---

### console output example:

```ShellSession
--- Terminal on /dev/ttyACM0 | 115200 8-N-1
--- Available filters and text transformations: colorize, debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
[  3911][I][WiFiMulti.cpp:114] run(): [WIFI] scan done
[  3911][I][WiFiMulti.cpp:119] run(): [WIFI] 15 networks found
[  3911][I][WiFiMulti.cpp:160] run(): [WIFI] Connecting BSSID: 26:AD:69:C2:AB:E8 SSID: OpwnSS Channel: 11 (-38)
[  4000][I][WiFiMulti.cpp:174] run(): [WIFI] Connecting done.
Connected to WiFi
IP: 192.168.86.23
audio_info: Connect to new host: "api.openai.com"
audio_info: PSRAM found, inputBufferSize: 638965 bytes
[  4698][I][Audio.cpp:5331] ts_parsePacket(): parseTS reset
audio_info: buffers freed, free Heap: 255068 bytes
audio_info: connect to api.openai.com on port 443 path /v1/audio/speech
audio_info: SSL has been established in 925 ms, free Heap: 213908 bytes
[  6921][I][Audio.cpp:4000] parseContentType(): ContentType audio/mpeg, format is mp3
audio_info: MP3Decoder has been initialized, free Heap: 214564 bytes , free stack 3760 DWORDs
[  6924][I][Audio.cpp:3846] parseHttpResponseHeader(): Switch to DATA, metaint is 0
audio_info: stream ready
audio_info: syncword found at pos 0
audio_info: Channels: 1
audio_info: SampleRate: 24000
audio_info: BitsPerSample: 16
audio_info: BitRate: 160000
audio_info: slow stream, dropouts are possible
audio_info: End of Stream.
```
