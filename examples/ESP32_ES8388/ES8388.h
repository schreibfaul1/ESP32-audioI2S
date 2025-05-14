#pragma once
#include <stdint.h>

class ES8388
{

    bool write_reg(uint8_t slave_add, uint8_t reg_add, uint8_t data);
    bool read_reg(uint8_t slave_add, uint8_t reg_add, uint8_t &data);
    bool identify(int32_t sda, int32_t scl, uint32_t frequency);

public:
    bool begin(int32_t sda = -1, int32_t scl = -1, uint32_t frequency = 400000U);

    enum ES8388_OUT
    {
        ES_MAIN, // this is the DAC output volume (both outputs)
        ES_OUT1, // this is the additional gain for OUT1
        ES_OUT2  // this is the additional gain for OUT2
    };

    void SetVolumeSpeaker(uint8_t vol);
    void SetVolumeHeadphone(uint8_t vol);

    void mute(const ES8388_OUT out, const bool muted);
    void volume(const ES8388_OUT out, const uint8_t vol);
};

