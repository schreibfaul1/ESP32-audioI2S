
// M5Stack Node support
// thanks to Cellie - issue #35     25.Apr.2020 
// M5Stck boards also need a MCLK signal on GPIO0.

// M5Stack Node pins https://github.com/m5stack/Bases-Node

#define I2C_SDA  21
#define I2C_SCL  22

/* M5Stack Node WM8978 pins */ 
//      codec pin    esp32 pin
#define I2S_BCK      5
#define I2S_WS      13
#define I2S_DOUT     2  
#define I2S_DIN     34   
#define I2S_MCLK     0

#include "Audio.h"
#include "WM8978.h"
#include "I2S.h"

Audio audio;

void setup() {
  WiFi.begin("xxx", "xxx");
  while (!WiFi.isConnected()) {
    delay(10);
  }
  ESP_LOGI(TAG, "Connected");

  //init wm8978 I2C interface
  wm8978Setup(I2C_SDA, I2C_SCL);

  //set clock on pin 0 - https://www.esp32.com/viewtopic.php?t=3060
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
  //REG_WRITE(PIN_CTRL, 0xFFFFFFF0);
  REG_WRITE(PIN_CTRL, 0);
  
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DIN);

  ESP_LOGI(TAG, "Starting MP3...\n");
  audio.connecttohost("http://icecast.omroep.nl/3fm-bb-mp3");
  
  WM8978_SPKvol_Set(40); /* max 63? */
  WM8978_HPvol_Set(32,32);  
}

void loop() {
  audio.loop();
  //delay(2);
}