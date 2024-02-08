#include <Arduino.h>
#include "WiFiMulti.h"
#include "Audio.h"

Audio audio;
WiFiMulti wifiMulti;

String ssid =     "xxxx";
String password = "xxxx";

#define I2S_LRC     26
#define I2S_DOUT    25
#define I2S_BCLK    27
#define I2S_MCLK     0

void setup() {
    Serial.begin(115200);
    wifiMulti.addAP(ssid.c_str(), password.c_str());
    wifiMulti.run();
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(20); // 0...21
    audio.setConnectionTimeout(500, 2700);
    audio.connecttohost("http://us3.internet-radio.com:8342/stream");
}

void loop(){
    audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
