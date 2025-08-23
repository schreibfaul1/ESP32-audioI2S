#include "Arduino.h"
#include "Audio.h"
#include "WiFi.h"

#define I2S_DOUT            9
#define I2S_BCLK            3
#define I2S_LRC             1

Audio audio;

String ssid =     "*****";
String password = "*****";

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Audio::audio_info_callback = my_audio_info;
    Serial.begin(115200);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(12); // default 0...21
    audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de"); // Google TTS
}

void loop() {
    audio.loop();
    vTaskDelay(1);
}
