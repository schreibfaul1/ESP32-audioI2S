#pragma once
#include <Arduino.h>
#include <Wire.h>


#define ES8311_ADDR              0x18
#define ES8311_SAMPLE_RATE48     48000
#define ES8311_BITS_PER_SAMPLE16 16

struct _coeff_div {       /* Clock coefficient structure */
    uint32_t mclk;        /* mclk frequency */
    uint32_t rate;        /* sample rate */
    uint8_t pre_div;      /* the pre divider with range from 1 to 8 */
    uint8_t pre_multi;    /* the pre multiplier with 0: 1x, 1: 2x, 2: 4x, 3: 8x selection */
    uint8_t adc_div;      /* adcclk divider */
    uint8_t dac_div;      /* dacclk divider */
    uint8_t fs_mode;      /* double speed or single speed, =0, ss, =1, ds */
    uint8_t lrck_h;       /* adclrck divider and daclrck divider */
    uint8_t lrck_l;
    uint8_t bclk_div;     /* sclk divider */
    uint8_t adc_osr;      /* adc osr */
    uint8_t dac_osr;      /* dac osr */
};

class ES8311{

private:
    TwoWire *_TwoWireInstance = NULL;	// TwoWire Instance
    uint32_t _mclk_hz = 48000 * 256; // default MCLK frequency
public:
	// Constructor.
    ES8311(TwoWire  *TwoWireInstance = &Wire);
    ~ES8311();
    bool begin(int32_t sda, int32_t scl, uint32_t frequency);
    bool setVolume(uint8_t volume);
    uint8_t getVolume();
    bool setSampleRate(uint32_t sample_rate);
    bool setBitsPerSample(uint8_t bps);
    bool enableMicrophone(bool enable);
    bool setMicrophoneGain(uint8_t gain);
    uint8_t getMicrophoneGain();
    void read_all();
protected:
    int get_coeff(uint32_t mclk, uint32_t rate);
	bool WriteReg(uint8_t reg, uint8_t val);
	uint8_t ReadReg(uint8_t reg);
};

