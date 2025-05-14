#include "Arduino.h"
#include "WiFi.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "Wire.h"
#include "AC101.h"
#include "Audio.h"

// I2S GPIOs, the names refer on AC101, AS1 Audio Kit V2.2 2379
#define I2S_DSIN      35 // pin not used
#define I2S_BCLK      27
#define I2S_LRC       26
#define I2S_MCLK       0
#define I2S_DOUT      25

// I2C GPIOs
#define IIC_CLK       32
#define IIC_DATA      33

// amplifier enable
#define GPIO_PA_EN    21

//Switch S1: 1-OFF, 2-ON, 3-ON, 4-OFF, 5-OFF

String ssid =     "*****";
String password = "*****";

static AC101 dac;                                 // AC101
int volume = 40;                                  // 0...100

Audio audio;

//#####################################################################

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.printf_P(PSTR("Connected\r\nRSSI: "));
  Serial.print(WiFi.RSSI());
  Serial.print(" IP: ");
  Serial.println(WiFi.localIP());

    Serial.printf("Connect to DAC codec... ");
    while (not dac.begin(IIC_DATA, IIC_CLK))
    {
        Serial.printf("Failed!\n");
        delay(1000);
    }
    Serial.printf("OK\n");

    dac.SetVolumeSpeaker(volume);
    dac.SetVolumeHeadphone(volume);
//  ac.DumpRegisters();

    // Enable amplifier
    pinMode(GPIO_PA_EN, OUTPUT);
    digitalWrite(GPIO_PA_EN, HIGH);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
    audio.setVolume(10); // 0...21

    audio.connecttohost("http://mp3channels.webradio.antenne.de:80/oldies-but-goldies");
//  audio.connecttohost("http://dg-rbb-http-dus-dtag-cdn.cast.addradio.de/rbb/antennebrandenburg/live/mp3/128/stream.mp3");
//  audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");

}

//-----------------------------------------------------------------------

void loop(){
    vTaskDelay(1);
    audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info){
    Serial.print("eof_speech  ");Serial.println(info);
}