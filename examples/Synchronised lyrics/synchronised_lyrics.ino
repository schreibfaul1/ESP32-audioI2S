#include "Arduino.h"
#include "Audio.h"
#include "SD_MMC.h"
#include "Ticker.h"

Audio audio;

#define I2S_LRC     26
#define I2S_DOUT    25
#define I2S_BCLK    27
#define I2S_MCLK     0

#define SD_MMC_D0    2
#define SD_MMC_CLK  14
#define SD_MMC_CMD  15


void setup() {
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    Serial.begin(115200);
    if(!SD_MMC.begin( "/sdcard", true, false, 20000)){
        Serial.println("Card Mount Failed");
        return;
    }
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(10); // 0...21
    audio.connecttoFS(SD_MMC, "/Little London Girl(lyrics).mp3");
}

void loop(){
    vTaskDelay(1);
    audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
//    if(strncmp(info, "Year: ", 6) == 0) Serial.println(info + 6);
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}

void audio_id3lyrics(const char* text) {
    Serial.printf("%s\n", text);

}

