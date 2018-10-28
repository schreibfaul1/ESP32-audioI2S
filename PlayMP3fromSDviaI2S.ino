#include "Arduino.h"
#include "audioI2S/Audio.h"
#include "SD.h"
#include "FS.h"

// Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;

void setup() {
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.begin(115200);
    SD.begin(SD_CS);
    audio.connecttoSD("320k_test.mp3");
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // 0...21
}

void loop()
{
    audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.println(info);
}
