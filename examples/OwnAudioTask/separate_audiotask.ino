#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"

// Digital I/O used
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

/* We create 10 structures and 10 memory areas for the queues.
 * Only the pointer to a structure is transferred to the queue.
 * The queue therefore only takes up a small amount of memory.
 * Since the command is executed with a time delay, the structure cannot be overwritten prematurely.
 * However, you can work cyclically. Then a check for a full queue is not necessary. */


struct audioMessage{
    uint8_t  cmd;
    char*    txt;
    uint32_t value;
} audioTxMessage[10], audioRxMessage;


enum : uint8_t { SET_VOLUME, GET_VOLUME, CONNECTTOHOST, CONNECTTOFS };


QueueHandle_t audioSetQueue = NULL;
QueueHandle_t audioGetQueue = NULL;


Audio audio;

String ssid =     "*****";
String password = "*****";

void AudioPlayer_Task(void *parameter) {
    struct audioMessage audioRxTaskMessage;
    struct audioMessage audioTxTaskMessage[10];
    while(true){
        if(audioSetQueue != NULL ){
            if(xQueueReceive(audioSetQueue, &audioRxTaskMessage, 0) == pdPASS) {
                if(audioRxTaskMessage.cmd == SET_VOLUME){
                    log_i("set volume to %d", audioRxTaskMessage.value);
                    audio.setVolume(audioRxTaskMessage.value);
                }
                if(audioRxTaskMessage.cmd == CONNECTTOHOST){
                    log_i("new url is %s", audioRxTaskMessage.txt);
                    audio.connecttohost(audioRxTaskMessage.txt);
                }
                if(audioRxTaskMessage.cmd == GET_VOLUME){
                    audioTxTaskMessage[0].cmd = GET_VOLUME;
                    audioTxTaskMessage[0].value = audio.getVolume();
                    xQueueSend(audioGetQueue, &audioTxTaskMessage[0], 0);
                }
            }
        }
        audio.loop();
        if(!audio.isRunning()) vTaskDelay(10); // yield
    }
}

void AudioPlayer_Init(void) {
    xTaskCreatePinnedToCore(
        AudioPlayer_Task,      /* Function to implement the task */
        "audioplay",           /* Name of the task */
        5000,                  /* Stack size in words */
        NULL,                  /* Task input parameter */
        2 | portPRIVILEGE_BIT, /* Priority of the task */
        NULL,                  /* Task handle. */
        1                      /* Core where the task should run */
    );
}

void CreateQueues(){
    audioSetQueue = xQueueCreate(10, sizeof(struct audioMessage));
    audioGetQueue = xQueueCreate(10, sizeof(struct audioMessage));
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    CreateQueues();
    AudioPlayer_Init();

    int i = 0;
    audioTxMessage[i].cmd = CONNECTTOHOST;
    audioTxMessage[i].txt = "http://mp3.ffh.de/radioffh/hqlivestream.mp3";
    audioTxMessage[i].value = 0;
    xQueueSend(audioSetQueue, &audioTxMessage[i], 0);
    i++;
    audioTxMessage[i].cmd = SET_VOLUME;
    audioTxMessage[i].txt = NULL;
    audioTxMessage[i].value = 10;
    xQueueSend(audioSetQueue, &audioTxMessage[i], 0);
    i++;
    audioTxMessage[i].cmd = GET_VOLUME;
    audioTxMessage[i].txt = NULL;
    audioTxMessage[i].value = 0;
    xQueueSend(audioSetQueue, &audioTxMessage[i], 0);
}

void loop(){
    if(audioGetQueue != NULL ){
        if(xQueueReceive(audioGetQueue, &audioRxMessage, (TickType_t)0) == pdPASS) {
            if(audioRxMessage.cmd == GET_VOLUME){
                log_i("current volume is %d", audioRxMessage.value);
            }
        }
    }

    // your own code here
}

void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}


