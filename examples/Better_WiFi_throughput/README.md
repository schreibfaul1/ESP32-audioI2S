# ESP32-audioI2S, better WiFi speed
with a high bit rate or low compression rate, the data throughput via WiFi may not be sufficient to fill the audio buffer sufficiently. In this case, the message 'slow stream, dropouts are possible' appears periodically. Better TCP settings can help. This can be achieved via menuconfig followed by Arduino compilation.
Here is a complete example that can be easily copy that into PlatformIO.


````c++
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
    audio.connecttohost("http://us3.internet-radio.com:8342/stream");
}

void loop(){
    audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}


````



These are the main settings:
![menuconfig](https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/examples/Better_WiFi_throughput/better_WiFi_throughput.jpeg)

