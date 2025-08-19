#include "Arduino.h"
/*
    I2S Bluetooth Transmitter
    Can be connected to an I2S Master.Sampling rate must be 48KHz
    Include PSchatzmann /ESP32-A2DP     https://github.com/pschatzmann/ESP32-A2DP.git
*/

#include "Arduino.h"
#include "esp_bt.h"
#include "BluetoothA2DPCommon.h"
#include "BluetoothA2DPSource.h"
#include <driver/i2s_std.h>
#include "AudioResampler.hpp"

#define RX_I2S_DIN    25    // connect with I2S Master (signal dout)
#define RX_I2S_BCLK   27    // connect with I2S Master (bit clock)
#define RX_I2S_LRC    26    // connect with I2S Master (word select)

#define MEASUREMENT_INTERVAL_MS 100 // Messintervall in ms
#define TOLERANCE_PERCENT 5         // Toleranz für Schwankungen in %
static volatile uint32_t lrck_count = 0;
static uint32_t current_samplerate = 0;

BluetoothA2DPSource a2dp_source;
AudioResampleBuffer resampler;

char BT_SINK_NAME[]   = " Pebble V3\r\n"; // set your sink devicename here
//char BT_SINK_NAME[]   = "Manhattan-165327";

i2s_chan_handle_t     i2s_rx_handle = {};
i2s_chan_config_t     i2s_chan_cfg = {}; // stores I2S channel values
i2s_std_config_t      i2s_std_cfg = {};  // stores I2S driver values

const i2s_port_t i2s_num     = I2S_NUM_0;

//--------------------------------Recognise Host Samplerate-------------------------------------------------------------------------------------
// Interrupt-Handler for LRCK-Pulse
void IRAM_ATTR lrck_interrupt_handler(void *arg) {
    lrck_count+=1;
}

// Initialize GPIO interrupt for LRCK
void init_lrck_monitor() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RX_I2S_LRC),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE // Interrupt bei steigender Flanke
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)RX_I2S_LRC, lrck_interrupt_handler, NULL);
}

// calculate samplerates based on LRCK counter
uint32_t measure_samplerate() {
    lrck_count = 0;
    vTaskDelay(MEASUREMENT_INTERVAL_MS / portTICK_PERIOD_MS);
    uint32_t samplerate = (lrck_count * 1000) / MEASUREMENT_INTERVAL_MS; // Hz
    return samplerate;
}

// check whether samplerates are one of the expected values
uint32_t map_to_valid_samplerate(uint32_t measured) {
    const uint32_t valid_rates[] = {8000, 22050, 44100, 48000};
    const uint32_t num_rates = sizeof(valid_rates) / sizeof(valid_rates[0]);
    uint32_t closest_rate = valid_rates[0];
    int min_diff = abs((int)measured - (int)valid_rates[0]);

    for (int i = 1; i < num_rates; i++) {
        int diff = abs((int)measured - (int)valid_rates[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest_rate = valid_rates[i];
        }
    }

    // check tolerance
    if (min_diff <= (closest_rate * TOLERANCE_PERCENT / 100)) {
        return closest_rate;
    }
    return 0; // Invalid samplerates
}

// task to monitor the samplerates
void samplerate_monitor_task(void *pvParameters) {
    init_lrck_monitor();

    while (1) {
        uint32_t new_samplerate = map_to_valid_samplerate(measure_samplerate());

        if (new_samplerate != 0 && new_samplerate != current_samplerate) {
            log_w("Samplerate changed to %u Hz", new_samplerate);
            resampler.setInputSamplerate(new_samplerate);
            // response to samplerates change
            if (new_samplerate == 48000) {
                log_w("Activating resampling to 44100 Hz");
                // activate resampling logic here
            } else if (new_samplerate == 44100) {
                log_w("No resampling needed");
                // deactivate Resampling, direct transmission
            } else {
                log_w("Unsupported samplerate: %u Hz", new_samplerate);
                // treat 8000 or 22050 Hz, if necessary
            }
            current_samplerate = new_samplerate;
        }

        vTaskDelay(500 / portTICK_PERIOD_MS); // Prüfe alle 500 ms
    }
}

//---------------------------------------------------------------------------------------------------------------------
void i2s_install(){

    i2s_chan_cfg.id            = (i2s_port_t)i2s_num;   // I2S_NUM_AUTO, I2S_NUM_0, I2S_NUM_1
    i2s_chan_cfg.role          = I2S_ROLE_SLAVE;        // I2S controller slave role, bclk and lrc signal will be set to input
    i2s_chan_cfg.dma_desc_num  = 8;                     // number of DMA buffer
    i2s_chan_cfg.dma_frame_num = 512;                   // I2S frame number in one DMA buffer.
    i2s_chan_cfg.auto_clear    = true;                  // i2s will always send zero automatically if no data to send
    i2s_new_channel(&i2s_chan_cfg, NULL, &i2s_rx_handle);

    i2s_std_cfg.slot_cfg                = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO); // Set to enable bit shift in Philips mode
    i2s_std_cfg.gpio_cfg.bclk           = (gpio_num_t)RX_I2S_BCLK;          // BCLK Assignment
    i2s_std_cfg.gpio_cfg.din            = (gpio_num_t)RX_I2S_DIN;           // DIN  Assignment
    i2s_std_cfg.gpio_cfg.dout           = I2S_GPIO_UNUSED;                  //
    i2s_std_cfg.gpio_cfg.mclk           = I2S_GPIO_UNUSED;                  //
    i2s_std_cfg.gpio_cfg.ws             = (gpio_num_t)RX_I2S_LRC;           // LRC  Assignment
    i2s_std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    i2s_std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    i2s_std_cfg.gpio_cfg.invert_flags.ws_inv   = false;
    i2s_std_cfg.clk_cfg.sample_rate_hz = 44800;
    i2s_std_cfg.clk_cfg.clk_src        = I2S_CLK_SRC_DEFAULT;        // Select PLL_F160M as the default source clock
    i2s_std_cfg.clk_cfg.mclk_multiple  = I2S_MCLK_MULTIPLE_128;      // 
    i2s_channel_init_std_mode(i2s_rx_handle, &i2s_std_cfg);
    i2s_channel_enable(i2s_rx_handle);
}

//---------------------------------------------CallBacks--------------------------------------------------------------------

int32_t get_data(uint8_t *data, int32_t bytes) {
      return resampler.getData(data, bytes); // Holt exakt die benötigten Daten
}

// gets called when button on bluetooth speaker is pressed
void button_handler(uint8_t id, bool isReleased){
  if (isReleased) {
    Serial.print("button id ");
    Serial.print(id);
    Serial.println(" released");
  }
}
//---------------------------------------------SETUP--------------------------------------------------------------------
void setup(){
    Serial.begin(115200);
    i2s_install();
    a2dp_source.set_data_callback(get_data);
    a2dp_source.set_avrc_passthru_command_callback(button_handler);
    a2dp_source.start(BT_SINK_NAME);
    resampler.setChannelHandle(i2s_rx_handle);
    xTaskCreate(samplerate_monitor_task, "samplerate_monitor", 2048, NULL, 5, NULL);
}
//----------------------------------------------LOOP--------------------------------------------------------------------
void loop() {
    vTaskDelay(1);
    resampler.loopResample();
}