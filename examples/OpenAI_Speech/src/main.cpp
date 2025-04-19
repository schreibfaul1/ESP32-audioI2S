#include <Arduino.h>
#include "SPI.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include "Audio.h"

// WiFi credentials
#define WIFI_SSID "<YOUR_WIFI_SSID>"
#define PASSWORD "<YOUR_WIFI_PASSWORD>"
#define OPENAI_API_KEY "<YOUR_OPENAI_API_KEY>" // https://platform.openai.com/api-keys

// Configure I2S pins
#define I2S_LRC D1
#define I2S_DOUT D2
#define I2S_BCLK D3
#define I2S_MCLK 0

// Inits
WiFiMulti wifiMulti;
Audio audio;


String input = "Added OpenAI Text to speech API support";
String instructions = "Voice: Gruff, fast-talking, and a little worn-out, like a New York cabbie who's seen it all but still keeps things moving.\n\n"
                      "Tone: Slightly exasperated but still functional, with a mix of sarcasm and no-nonsense efficiency.\n\n"
                      "Dialect: Strong New York accent, with dropped \"r\"s, sharp consonants, and classic phrases like whaddaya and lemme guess.\n\n"
                      "Pronunciation: Quick and clipped, with a rhythm that mimics the natural hustle of a busy city conversation.\n\n"
                      "Features: Uses informal, straight-to-the-point language, throws in some dry humor, and keeps the energy just on the edge of impatience but still";

// Declaration
void audio_info(const char *info);

// Default
void setup() {
    Serial.begin(115200);
    // Wifi
    wifiMulti.addAP(WIFI_SSID, PASSWORD);
    Serial.println("Connecting to WiFi...");
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
    }
    Serial.print("Connected to WiFi\nIP: ");
    Serial.println(WiFi.localIP());

    delay(500);

    // Speech
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, -1);
    audio.setVolume(15); // 0...21
    audio.openai_speech(OPENAI_API_KEY, "tts-1", input, instructions, "shimmer", "mp3", "1");
}

void loop(void) {
    vTaskDelay(1);
    audio.loop();
}

void audio_info(const char *info) {
    Serial.print("audio_info: ");
    Serial.println(info);
}