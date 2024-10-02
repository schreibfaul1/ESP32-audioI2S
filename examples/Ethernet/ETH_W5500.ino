#include "Arduino.h" // >= Arduino V3
#include <ETH.h>
#include <SPI.h>
#define ETHERNET_IF
#include "Audio.h"

Audio audio;

#define USE_TWO_ETH_PORTS 0
#define ETH_PHY_TYPE ETH_PHY_W5500

// GPIOs
#define ETH_PHY_ADDR   1
#define ETH_PHY_CS     3
#define ETH_PHY_IRQ    8
#define ETH_PHY_RST    4
#define ETH_SPI_SCK    7
#define ETH_SPI_MISO   6
#define ETH_SPI_MOSI   5
#define I2S_DOUT      12
#define I2S_BCLK      13
#define I2S_LRC       14

static bool eth_connected = false;

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("ETH Started");
            ETH.setHostname("esp32-eth0"); //set eth hostname here
            break;
        case ARDUINO_EVENT_ETH_CONNECTED: Serial.println("ETH Connected"); break;
        case ARDUINO_EVENT_ETH_GOT_IP:    Serial.printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif)); Serial.println(ETH);
            eth_connected = true;
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            Serial.println("ETH Lost IP");
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            eth_connected = false;
            break;
        default: break;
    }
}

void setup(){
    Serial.begin(115200);
    Serial.print("\n\n");

    Network.onEvent(onEvent);

    SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);
    while (!eth_connected) delay(100);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // default 0...21
    audio.connecttohost("https://wdr-wdr2-ruhrgebiet.icecastssl.wdr.de/wdr/wdr2/ruhrgebiet/mp3/128/stream.mp3"); // mp3
}

void loop(){
    audio.loop();
    vTaskDelay(5 /portTICK_PERIOD_MS);
}

void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}