//**********************************************************************************************************
//*    audioI2S-- I2S audiodecoder for ESP32,                                                              *
//**********************************************************************************************************
//
// first release on 11/2018
// Version 3  , Jul.02/2020
//
//
// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
// OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
//

#include "Arduino.h"
#include "WiFiMulti.h"
#include "Audio.h"
#include "SPI.h"
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
WiFiMulti wifiMulti;
String ssid =     "xxxxx";
String password = "xxxxx";

// callback declaration
void meta_callback(const char *info, audio::callback_type_t type);

void setup() {
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);
    Serial.begin(115200);
    SD.begin(SD_CS);
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(ssid.c_str(), password.c_str());
    wifiMulti.run();
    if(WiFi.status() != WL_CONNECTED){
        WiFi.disconnect(true);
        wifiMulti.run();
    }
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(12); // 0...21


    // Optional - handling metadata and information logging

    // attach callback function (optional)
    audio.setLiteralCallback(meta_callback);
    // enable all info messages (optional)
    audio.enableCallbackType(audio::callback_type_t::all, true);
    /*
        you can also use functional callbacks and lamda's,
        i.e.
        audio.setLiteralCallback([](const char *info, audio::callback_type_t type){ Serial.printf("Message type:%u, msg:%s\n", static_cast<size_t>(type), info); });
        audio.enableCallbackType(audio::callback_type_t::all, true);

        selectively enabling/disabling message types as needed
        audio.enableCallbackType(audio::callback_type_t::streamtitle, true);    // enable stream title callbacks
        audio.enableCallbackType(audio::callback_type_t::info, false);          // disable info callbacks
        audio.setLiteralCallback(nullptr);                                      // detach callback at run-time
    */


//    audio.connecttoFS(SD, "test.wav");
//    audio.connecttohost("http://www.wdr.de/wdrlive/media/einslive.m3u");
//    audio.connecttohost("http://somafm.com/wma128/missioncontrol.asx"); //  asx
//    audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.aac"); //  128k aac
      audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.mp3"); //  128k mp3
}

void loop(){
    vTaskDelay(1);
    audio.loop();
    if(Serial.available()){ // put streamURL in serial monitor
        audio.stopSong();
        String r=Serial.readString(); r.trim();
        if(r.length()>5) audio.connecttohost(r.c_str());
        log_i("free heap=%i", ESP.getFreeHeap());
    }
}


// metadata and debugging handler
void meta_callback(const char *info, audio::callback_type_t type){
  switch (type){
    case audio::callback_type_t::id3data :
      Serial.print("id3data: ");
      Serial.println(info);
      break;

    // track title
    case audio::callback_type_t::streamtitle :
      Serial.print("Stream title: ");
      Serial.println(info);
        break;

    // station title
    case audio::callback_type_t::station :
      Serial.print("Station title: ");
      Serial.println(info);
        break;

    case audio::callback_type_t::bitrate :
        Serial.print("BitRate: ");
        Serial.println(info);
        break;
/*
    // tailor it to your needs
    case audio::callback_type_t::id3lyrics :
      break;
    case audio::callback_type_t::icyurl :
      break;
    case audio::callback_type_t::icylogo :
      break;
    case audio::callback_type_t::icydescr :
      break;
    case audio::callback_type_t::lasthost :
      // passes connection URL
      break;
    case audio::callback_type_t::eof :
      break;
*/

    default:
      // default is just print the message and info code
      Serial.printf("Audio info:%u, msg:", static_cast<size_t>(type));
      Serial.println(info);
  }
}
