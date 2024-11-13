#include "Arduino.h"
#include "Audio.h"
#include "SD_MMC.h"
#include "FS.h"
#include <vector>

#define I2S_LRC     26
#define I2S_DOUT    25
#define I2S_BCLK    27

#define SD_MMC_D0   2
#define SD_MMC_CLK  14
#define SD_MMC_CMD  15

void listDir(fs::FS &fs, const char * dirname, uint8_t levels); //proto
std::vector<char*>  v_audioContent;
int pirPin = 4;
Audio audio;

File dir;
const char audioDir[] = "/mp3";

void setup() {
    Serial.begin(115200);
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(SD_MMC_CLK,SD_MMC_CMD, SD_MMC_D0);
    if(!SD_MMC.begin( "/sdmmc", true, false, 20000)){
        Serial.println("Card Mount Failed");
        return;
    }
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(17); // 0...21 Will need to add a volume setting in the app
    dir = SD_MMC.open(audioDir);
    listDir(SD_MMC, audioDir, 1);
    if(v_audioContent.size() > 0){
        const char* s = (const char*)v_audioContent[v_audioContent.size() -1];
        Serial.printf("playing %s\n", s);
        audio.connecttoFS(SD_MMC, s);
        v_audioContent.pop_back();
    }
}

void loop(){
    audio.loop();
    vTaskDelay(1);    // Audio is distoreted without this
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
            v_audioContent.insert(v_audioContent.begin(), strdup(file.path()));
        }
        file = root.openNextFile();
    }
    Serial.printf("num files %i", v_audioContent.size());
    root.close();
    file.close();
}

void vector_clear_and_shrink(vector<char*>&vec){
    uint size = vec.size();
    for (int i = 0; i < size; i++) {
        if(vec[i]){
            free(vec[i]);
            vec[i] = NULL;
        }
    }
    vec.clear();
    vec.shrink_to_fit();
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
    if(v_audioContent.size() == 0){
        vector_clear_and_shrink(v_audioContent); // free memory
        return;
    }
    const char* s = (const char*)v_audioContent[v_audioContent.size() - 1];
    Serial.printf("playing %s\n", s);
    audio.connecttoFS(SD_MMC, s);
    v_audioContent.pop_back();
}
