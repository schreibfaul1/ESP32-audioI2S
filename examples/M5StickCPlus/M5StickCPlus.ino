//**********************************************************************************************************
//*    audioI2S-- I2S audiodecoder for M5StickC Plus and SPK HAT                                           *
//**********************************************************************************************************
//
// first release on May.12/2021
//
//
// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
// OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
//

#include <M5StickCPlus.h>
#include "Audio.h"

Audio audio = Audio(true);

String ssid =     "xxxxxxxx";
String password = "xxxxxxxx";

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Audio::audio_info_callback = my_audio_info;
    M5.begin(false);        // Lcd disabled to reduce noise
    M5.Axp.ScreenBreath(1); // Lower Lcd backlight
    pinMode(36, INPUT);
    gpio_pulldown_dis(GPIO_NUM_25);
    gpio_pullup_dis(GPIO_NUM_25);
    M5.Beep.tone(44100); // Built-in buzzer tone
    M5.Beep.end();       // disabled

    audio.setVolume(15); // 0...21

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (!WiFi.isConnected()) { delay(10); }
    ESP_LOGI(TAG, "Connected");
    ESP_LOGI(TAG, "Starting MP3...\n");

    audio.connecttohost("http://air.ofr.fm:8008/jazz/mp3/128");
    //  audio.connecttospeech("Миска вареників з картоплею та шкварками, змащених салом!", "uk-UA");
}

void loop() {
    vTaskDelay(1);
    audio.loop();
    if(Serial.available()){ // put streamURL in serial monitor
        audio.stopSong();
        String r=Serial.readString();
        r.trim();
        if(r.length()>5) audio.connecttohost(r.c_str());
        log_i("free heap=%i", ESP.getFreeHeap());
    }
}
