// This arduino sketch for ESP32 announces the time every hour.
// A connection to the Internet is required
// 1) to synchronize with the internal RTC on startup
// 2) so that GoogleTTS can be reached



#include <Arduino.h>
#include "WiFiMulti.h"
#include "Audio.h"
#include "time.h"
#include "esp_sntp.h"

//------------------------USER SETTINGS / GPIOs-------------------------------------------------------------------------
String ssid =     "xxxx";
String password = "xxxx";
uint8_t I2S_BCLK = 27;
uint8_t I2S_LRC  = 26;
uint8_t I2S_DOUT = 25;
//------------------------OBJECTS /GLOBAL VARS--------------------------------------------------------------------------
Audio audio(false, 3, 0);
WiFiMulti wifiMulti;
uint32_t sec1 = millis();
String time_s = "";
char chbuf[200];
int timeIdx = 0;
//------------------------TIME / SNTP STUFF-----------------------------------------------------------------------------
#define TZName    "CET-1CEST,M3.5.0,M10.5.0/3"    // https://remotemonitoringsystems.ca/time-zone-abbreviations.php
char strftime_buf[64];
struct tm timeinfo;
time_t now;
boolean obtain_time(){
    time_t now = 0;
    int retry = 0;
    Serial.println("Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        Serial.printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
        vTaskDelay(uint16_t(2000 / portTICK_PERIOD_MS));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    setenv("TZ", TZName, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    if(retry < retry_count) return true;
    else return false;
}

const char* gettime_s(){  // hh:mm:ss
	time(&now);
	localtime_r(&now, &timeinfo);
	sprintf(strftime_buf,"%02d:%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	return strftime_buf;
}
//-----------------------SETUP------------------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    wifiMulti.addAP(ssid.c_str(), password.c_str());
    wifiMulti.run();
    obtain_time();
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(15);
}
//-----------------------LOOP-------------------------------------------------------------------------------------------
void loop() {
    audio.loop();
    if(sec1 < millis()){    // every second
        sec1 = millis() + 1000;
        time_s = gettime_s();
        Serial.println(time_s);
        if(time_s.endsWith("00:00")){ // time announcement every full hour
            char am_pm[5] = "am.";
            int h = time_s.substring(0,2).toInt();
            if(h > 12){h -= 12; strcpy(am_pm,"pm.");}
            sprintf(chbuf, "It is now %i%s and %i minutes", h, am_pm, time_s.substring(3,5).toInt());
            Serial.println(chbuf);
            audio.connecttospeech(chbuf, "en");
        }
    }
}
//------------------EVENTS----------------------------------------------------------------------------------------------
void audio_info(const char *info){
    Serial.printf("info: %s\n", info);
}