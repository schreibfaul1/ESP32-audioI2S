# ESP32-audioI2S

:warning: **This library only works on multi-core chips like ESP32, ESP32-S3 and ESP32-P4. Your board must have PSRAM! It does not work on the ESP32-S2, ESP32-C3 etc** :warning:

Plays mp3, m4a and wav files from SD card via I2S with external hardware.
HELIX-mp3 and faad2-aac decoder is included. There is also an OPUS decoder for Fullband, an VORBIS decoder and a FLAC decoder.
Works with MAX98357A (3 Watt amplifier with DAC), connected three lines (DOUT, BLCK, LRC) to I2S. The I2S output frequency is always 48kHz, regardless of the input source, so Bluetooth devices can also be connected without any problems.
For stereo are two MAX98357A necessary. AudioI2S works with UDA1334A (Adafruit I2S Stereo Decoder Breakout Board), PCM5102A and CS4344.
Other HW may work but not tested. Plays also icy-streams, GoogleTTS and OpenAIspeech. Can be compiled with Arduino IDE. [WIKI](https://github.com/schreibfaul1/ESP32-audioI2S/wiki)

```` c++
#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
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

String ssid =     "*******";
String password = "*******";

Audio audio;

// callbacks
void my_audio_info(Audio::msg_t m) {
    switch(m.e){
        case Audio::evt_info:           Serial.printf("info: ....... %s\n", m.msg); break;
        case Audio::evt_eof:            Serial.printf("end of file:  %s\n", m.msg); break;
        case Audio::evt_bitrate:        Serial.printf("bitrate: .... %s\n", m.msg); break; // icy-bitrate or bitrate from metadata
        case Audio::evt_icyurl:         Serial.printf("icy URL: .... %s\n", m.msg); break;
        case Audio::evt_id3data:        Serial.printf("ID3 data: ... %s\n", m.msg); break; // id3-data or metadata
        case Audio::evt_lasthost:       Serial.printf("last URL: ... %s\n", m.msg); break;
        case Audio::evt_name:           Serial.printf("station name: %s\n", m.msg); break; // station name or icy-name
        case Audio::evt_streamtitle:    Serial.printf("stream title: %s\n", m.msg); break;
        case Audio::evt_icylogo:        Serial.printf("icy logo: ... %s\n", m.msg); break;
        case Audio::evt_icydescription: Serial.printf("icy descr: .. %s\n", m.msg); break;
        case Audio::evt_image: for(int i = 0; i < m.vec.size(); i += 2){
                                        Serial.printf("cover image:  segment %02i, pos %07i, len %05i\n", i / 2, m.vec[i], m.vec[i + 1]);} break; // APIC
        default:                        Serial.printf("message:..... %s\n", m.msg); break;
    }
}

void setup() {
    Audio::audio_info_callback = my_audio_info; // optional
    pinMode(SD_CS, OUTPUT);      digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.begin(115200);
    SD.begin(SD_CS);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // default 0...21
//  or alternative
//  audio.setVolumeSteps(64); // max 255
//  audio.setVolume(63);
//
//  *** radio streams ***
    audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
//  audio.connecttohost("http://mcrscast.mcr.iol.pt/cidadefm");                                         // mp3
//  audio.connecttohost("http://www.wdr.de/wdrlive/media/einslive.m3u");                                // m3u
//  audio.connecttohost("https://stream.srg-ssr.ch/rsp/aacp_48.asx");                                   // asx
//  audio.connecttohost("http://tuner.classical102.com/listen.pls");                                    // pls
//  audio.connecttohost("http://stream.radioparadise.com/flac");                                        // flac
//  audio.connecttohost("http://stream.sing-sing-bis.org:8000/singsingFlac");                           // flac (ogg)
//  audio.connecttohost("http://s1.knixx.fm:5347/dein_webradio_vbr.opus");                              // opus (ogg)
//  audio.connecttohost("http://stream2.dancewave.online:8080/dance.ogg");                              // vorbis (ogg)
//  audio.connecttohost("http://26373.live.streamtheworld.com:3690/XHQQ_FMAAC/HLSTS/playlist.m3u8");    // HLS
//  audio.connecttohost("http://eldoradolive02.akamaized.net/hls/live/2043453/eldorado/master.m3u8");   // HLS (ts)
//  *** web files ***
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Pink-Panther.wav");        // wav
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Santiano-Wellerman.flac"); // flac
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Olsen-Banden.mp3");        // mp3
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Miss-Marple.m4a");         // m4a (aac)
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Collide.ogg");             // vorbis
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/sample.opus");             // opus
//  *** local files ***
//  audio.connecttoFS(SD, "/test.wav");     // SD
//  audio.connecttoFS(SD_MMC, "/test.wav"); // SD_MMC
//  audio.connecttoFS(SPIFFS, "/test.wav"); // SPIFFS

//  audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de"); // Google TTS
}

void loop(){
    audio.loop();
    vTaskDelay(1);
}

````

````c++
/* ESP32-S3, ESP32-P4 EXAMPLE */

#include "Arduino.h"
#include "Audio.h"
#include "WiFi.h"
#include "SD_MMC.h"

#define I2S_DOUT            9
#define I2S_BCLK            3
#define I2S_LRC             1
#define SD_MMC_D0          11
#define SD_MMC_CLK         13
#define SD_MMC_CMD         14

Audio audio;

String ssid =     "*****";
String password = "*****";


void my_audio_info(Audio::msg_t m) {
    if(m.e == Audio::evt_info) Serial.printf("info: %s\n", m.msg);
}

void setup() {
    Audio::audio_info_callback = my_audio_info; // optional
    Serial.begin(115200);
//    WiFi.begin(ssid.c_str(), password.c_str());
//    while (WiFi.status() != WL_CONNECTED) delay(1500);

    pinMode(SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    SD_MMC.begin("/sdcard", true);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(12); // default 0...21
//   audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
    audio.connecttoFS(SD_MMC, "/test.wav");
}

void loop() {
    audio.loop();
    vTaskDelay(1);
}
````

<br>

|Codec       | ESP32       |ESP32-S3 or ESP32-P4         |                          |
|------------|-------------|-----------------------------|--------------------------|
| mp3        | y           | y                           |                          |
| aac        | y           | y                           |                          |
| aacp       | y (mono)    | y (+SBR, +Parametric Stereo)|                          |
| wav        | y           | y                           |                          |
| flac       | y           | y                           |blocksize max 24576 bytes |
| vorbis     | y           | y                           | <=196Kbit/s              |
| m4a        | y           | y                           |                          |
| opus       | y           | y                           |hybrid mode not impl yet  |

<br>

***
Wiring
![Wiring ESP32-S3](https://github.com/user-attachments/assets/15dd1766-0fc1-4079-b378-bc566583e80d)
***
Impulse diagram
![Impulse diagram](https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/additional_info/Impulsdiagramm.jpg)
***
Yellobyte has developed an all-in-one board. It includes an ESP32-S3 N8R2, 2x MAX98357 and an SD card adapter.
Documentation, circuit diagrams and examples can be found here: https://github.com/yellobyte/ESP32-DevBoards-Getting-Started
![image](https://github.com/user-attachments/assets/4002d09e-8e76-4e08-9265-188fed7628d3)

