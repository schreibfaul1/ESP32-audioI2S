#include "Arduino.h"
#include "Audio.h"
#include "SD_MMC.h"
#include "Ticker.h"

Audio audio;
Ticker   ticker;
char     *lyricsText;
size_t   lyricsTextSize = 0;
uint16_t lyricsPtr = 0;
uint32_t timeStamp = 0;
uint32_t ms = 0;
char     chbuf[512];

#define I2S_LRC     26
#define I2S_DOUT    25
#define I2S_BCLK    27
#define I2S_MCLK     0

#define SD_MMC_D0    2
#define SD_MMC_CLK  14
#define SD_MMC_CMD  15


size_t bigEndian(char* base, uint8_t numBytes, uint8_t shiftLeft = 8){
    uint64_t result = 0;
    if(numBytes < 1 || numBytes > 8) return 0;
    for (int i = 0; i < numBytes; i++) {
            result += *(base + i) << (numBytes -i - 1) * shiftLeft;
    }
    if(result > SIZE_MAX) {log_e("range overflow"); result = 0;} // overflow
    return (size_t)result;
}

void tckr(){ // caller every 100ms
    if(audio.isRunning()){
        ms += 100;
        if(ms >= timeStamp){
            Serial.print(chbuf);
            strcpy(chbuf, lyricsText + lyricsPtr); lyricsPtr += strlen(chbuf) + 1; // strlen + '\0'
            timeStamp = bigEndian(lyricsText + lyricsPtr, 4); lyricsPtr += 4;
        }
    }
    else{
        if(lyricsText) {free(lyricsText); lyricsText = NULL; lyricsTextSize = 0;}

        ticker.detach();
    }
}

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

void audio_id3lyrics(File &file, const size_t pos, const size_t size) {
    Serial.printf("\naudio_id3lyrics, pos: %d, size: %d\n", pos, size);
    lyricsText = (char *)malloc(size);
    lyricsTextSize = size;
    file.seek(pos);
    file.read((uint8_t *)lyricsText, size);
    Serial.printf("text encoding: %i\n", lyricsText[0]); // 0: ASCII, 3: UTF-8
    char lang[14]; memcpy(lang, (const char*)lyricsText + 1, 3); lang[3] = '\0'; Serial.printf("language: %s\n", lang);
    Serial.printf("time stamp format: %i\n", lyricsText[4]);
    Serial.printf("content type: %i\n", lyricsText[5]);
    Serial.printf("content descriptor: %i\n\n", lyricsText[6]);
    lyricsPtr = 7;
    strcpy(chbuf, lyricsText + lyricsPtr); lyricsPtr += strlen(chbuf) + 1; // strlen + '\0'
    timeStamp = bigEndian(lyricsText + lyricsPtr, 4);
    ticker.attach(0.1, tckr); lyricsPtr += 4;
}

