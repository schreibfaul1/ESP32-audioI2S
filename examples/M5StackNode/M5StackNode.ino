
// M5Stack Node support
// thanks to Cellie - issue #35     25.Apr.2020
// M5Stack board with Node base also need a MCLK signal on GPIO0.

#include <WM8978.h> /* https://github.com/CelliesProjects/wm8978-esp32 */
#include <Audio.h> /* https://github.com/schreibfaul1/ESP32-audioI2S */

/* M5Stack Node WM8978 I2C pins */
#define I2C_SDA     21
#define I2C_SCL     22

/* M5Stack Node I2S pins */
#define I2S_BCK      5
#define I2S_WS      13
#define I2S_DOUT     2
#define I2S_DIN     34

/* M5Stack WM8978 MCLK gpio number */
#define I2S_MCLKPIN  0

WM8978 dac;
Audio audio;

void setup() {
  /* Setup wm8978 I2C interface */
  if (!dac.begin(I2C_SDA, I2C_SCL)) {
    log_e("Error setting up dac. System halted");
    while (1) delay(100);
  }

  dac.setSPKvol(40); /* max 63 */
  dac.setHPvol(32, 32);

  /* Setup wm8978 I2S interface */
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCLKPIN);

  WiFi.begin("xxx", "xxx");
  while (!WiFi.isConnected()) {
    delay(10);
  }

  log_i("Connected\nStarting MP3...\n");
  audio.connecttohost("http://icecast.omroep.nl/3fm-bb-mp3");
}

void loop() {
  audio.loop();
}
