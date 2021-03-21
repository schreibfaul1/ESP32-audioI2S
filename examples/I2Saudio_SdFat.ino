#include "Arduino.h"
#include "Audio.h"
#include "SdFat.h"  // activate SDFATFS_USED in "Audio.h"


// Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      26
#define I2S_BCLK      27
#define I2S_LRC       25

Audio audio;

void setup() {
    Serial.begin(115200);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SD.begin(SD_CS);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(12); // 0...21
    audio.connecttoFS(SD, "/Test.mp3"); // @suppress("Invalid arguments")
}

void loop() {
    audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
