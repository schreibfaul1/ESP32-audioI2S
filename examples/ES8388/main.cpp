#include "Arduino.h"
#include "WiFi.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "Wire.h"
#include "ES8388.h"
#include "Audio.h"


// SPI GPIOs
#define SD_CS         13
#define SPI_MOSI      15
#define SPI_MISO       2
#define SPI_SCK       14

// I2S GPIOs, the names refer on ES8388, AS1 Audio Kit V2.2 3378
#define I2S_DSIN    35 // pin not used
#define I2S_BCLK    27
#define I2S_LRC     25
#define I2S_MCLK     0
#define I2S_DOUT    26

// I2C GPIOs
#define IIC_CLK       32
#define IIC_DATA      33

// buttons
// #define BUTTON_2_PIN 13             // shared mit SPI_CS
#define BUTTON_3_PIN  19
#define BUTTON_4_PIN  23
#define BUTTON_5_PIN  18               // Stop
#define BUTTON_6_PIN   5               // Play

// amplifier enable
#define GPIO_PA_EN    21

//Switch S1: 1-OFF, 2-ON, 3-ON, 4-OFF, 5-OFF

String ssid =     "*****";
String password = "*****";

ES8388 dac;                                 // ES8388 (new board)
int volume = 40;                            // 0...100

Audio audio;

//#####################################################################

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Audio::audio_info_callback = my_audio_info;
    Serial.begin(115200);
    Serial.println("\r\nReset");
    Serial.printf_P(PSTR("Free mem=%l\n"), ESP.getFreeHeap());

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);

    SD.begin(SD_CS);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(100);
    }

    Serial.printf_P(PSTR("Connected\r\nRSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.print(" IP: ");
    Serial.println(WiFi.localIP());

    Serial.printf("Connect to DAC codec... ");
    while (not dac.begin(IIC_DATA, IIC_CLK)){
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
