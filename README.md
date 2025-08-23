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

// Digital I/O used
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

String ssid =     "*******";
String password = "*******";

Audio audio;

// callbacks
void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Audio::audio_info_callback = my_audio_info; // optional
    Serial.begin(115200);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // default 0...21
    audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/");
}

void loop(){
    audio.loop();
    vTaskDelay(1);
}

````
You can find more examples here: https://github.com/schreibfaul1/ESP32-audioI2S/tree/master/examples

````c++
// detailed cb output
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
                                        Serial.printf("cover image:  segment %02i, pos %07lu, len %05lu\n", i / 2, m.vec[i], m.vec[i + 1]);} break; // APIC
        case Audio::evt_lyrics:         Serial.printf("sync lyrics:  %s\n", m.msg); break;
        case Audio::evt_log   :         Serial.printf("audio_logs:   %s\n", m.msg); break;
        default:                        Serial.printf("message:..... %s\n", m.msg); break;
    }
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

