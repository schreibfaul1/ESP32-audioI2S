#include <Arduino.h>
#include "ES8388.h"
#include <Wire.h>

#define ES8388_ADDR 0x10

/* ES8388 register */
#define ES8388_CONTROL1 0x00
#define ES8388_CONTROL2 0x01
#define ES8388_CHIPPOWER 0x02
#define ES8388_ADCPOWER 0x03
#define ES8388_DACPOWER 0x04
#define ES8388_CHIPLOPOW1 0x05
#define ES8388_CHIPLOPOW2 0x06
#define ES8388_ANAVOLMANAG 0x07
#define ES8388_MASTERMODE 0x08

/* ADC */
#define ES8388_ADCCONTROL1 0x09
#define ES8388_ADCCONTROL2 0x0a
#define ES8388_ADCCONTROL3 0x0b
#define ES8388_ADCCONTROL4 0x0c
#define ES8388_ADCCONTROL5 0x0d
#define ES8388_ADCCONTROL6 0x0e
#define ES8388_ADCCONTROL7 0x0f
#define ES8388_ADCCONTROL8 0x10
#define ES8388_ADCCONTROL9 0x11
#define ES8388_ADCCONTROL10 0x12
#define ES8388_ADCCONTROL11 0x13
#define ES8388_ADCCONTROL12 0x14
#define ES8388_ADCCONTROL13 0x15
#define ES8388_ADCCONTROL14 0x16

/* DAC */
#define ES8388_DACCONTROL1 0x17
#define ES8388_DACCONTROL2 0x18
#define ES8388_DACCONTROL3 0x19
#define ES8388_DACCONTROL4 0x1a
#define ES8388_DACCONTROL5 0x1b
#define ES8388_DACCONTROL6 0x1c
#define ES8388_DACCONTROL7 0x1d
#define ES8388_DACCONTROL8 0x1e
#define ES8388_DACCONTROL9 0x1f
#define ES8388_DACCONTROL10 0x20
#define ES8388_DACCONTROL11 0x21
#define ES8388_DACCONTROL12 0x22
#define ES8388_DACCONTROL13 0x23
#define ES8388_DACCONTROL14 0x24
#define ES8388_DACCONTROL15 0x25
#define ES8388_DACCONTROL16 0x26
#define ES8388_DACCONTROL17 0x27
#define ES8388_DACCONTROL18 0x28
#define ES8388_DACCONTROL19 0x29
#define ES8388_DACCONTROL20 0x2a
#define ES8388_DACCONTROL21 0x2b
#define ES8388_DACCONTROL22 0x2c
#define ES8388_DACCONTROL23 0x2d
#define ES8388_DACCONTROL24 0x2e
#define ES8388_DACCONTROL25 0x2f
#define ES8388_DACCONTROL26 0x30
#define ES8388_DACCONTROL27 0x31
#define ES8388_DACCONTROL28 0x32
#define ES8388_DACCONTROL29 0x33
#define ES8388_DACCONTROL30 0x34

bool ES8388::write_reg(uint8_t slave_add, uint8_t reg_add, uint8_t data)
{
    Wire.beginTransmission(slave_add);
    Wire.write(reg_add);
    Wire.write(data);
    return Wire.endTransmission() == 0;
}

bool ES8388::read_reg(uint8_t slave_add, uint8_t reg_add, uint8_t &data)
{
    bool retval = false;
    Wire.beginTransmission(slave_add);
    Wire.write(reg_add);
    Wire.endTransmission(false);
    Wire.requestFrom((uint16_t)slave_add, (uint8_t)1, true);
    if (Wire.available() >= 1)
    {
        data = Wire.read();
        retval = true;
    }
    return retval;
}

bool ES8388::begin(int32_t sda, int32_t scl, uint32_t frequency)
{
    bool res = identify(sda, scl, frequency);

    if (res == true)
    {

        /* mute DAC during setup, power up all systems, slave mode */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x04);
        res &= write_reg(ES8388_ADDR, ES8388_CONTROL2, 0x50);
        res &= write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0x00);
        res &= write_reg(ES8388_ADDR, ES8388_MASTERMODE, 0x00);

        /* power up DAC and enable LOUT1+2 / ROUT1+2, ADC sample rate = DAC sample rate */
        res &= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3e);
        res &= write_reg(ES8388_ADDR, ES8388_CONTROL1, 0x12);

        /* DAC I2S setup: 16 bit word length, I2S format; MCLK / Fs = 256*/
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL1, 0x18);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL2, 0x02);

        /* DAC to output route mixer configuration: ADC MIX TO OUTPUT */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL16, 0x1B);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL17, 0x90);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL20, 0x90);

        /* DAC and ADC use same LRCK, enable MCLK input; output resistance setup */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL21, 0x80);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL23, 0x00);

        /* DAC volume control: 0dB (maximum, unattenuated)  */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL5, 0x00);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL4, 0x00);

        /* power down ADC while configuring; volume: +9dB for both channels */
        res &= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0xff);
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL1, 0x88); // +24db

        /* select LINPUT2 / RINPUT2 as ADC input; stereo; 16 bit word length, format right-justified, MCLK / Fs = 256 */
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL2, 0xf0); // 50
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL3, 0x80); // 00
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL4, 0x0e);
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL5, 0x02);

        /* set ADC volume */
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL8, 0x20);
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL9, 0x20);

        /* set LOUT1 / ROUT1 volume: 0dB (unattenuated) */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL24, 0x1e);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL25, 0x1e);

        /* set LOUT2 / ROUT2 volume: 0dB (unattenuated) */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL26, 0x1e);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL27, 0x1e);

        /* power up and enable DAC; power up ADC (no MIC bias) */
        res &= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3c);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x00);
        res &= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0x00);

        /* set up MCLK) */
        #ifdef FUNC_GPIO0_CLK_OUT1
		    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
        #endif
        WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
    }
    return res;
}

/**
 * @brief (un)mute one of the two outputs or main dac output of the ES8388 by switching of the output register bits. Does not really mute the selected output, causes an attenuation. 
 * hence should be used in conjunction with appropriate volume setting. Main dac output mute does mute both outputs
 * 
 * @param out
 * @param muted
 */
void ES8388::mute(const ES8388_OUT out, const bool muted)
{
    uint8_t reg_addr;
    uint8_t mask_mute;
    uint8_t mask_val;

    switch (out)
    {
    case ES_OUT1:
        reg_addr = ES8388_DACPOWER;
        mask_mute = (3 << 4);
        mask_val = muted ? 0 : mask_mute;
        break;
    case ES_OUT2:
        reg_addr = ES8388_DACPOWER;
        mask_mute = (3 << 2);
        mask_val = muted ? 0 : mask_mute;
        break;
    case ES_MAIN:
    default:
        reg_addr = ES8388_DACCONTROL3;
        mask_mute = 1 << 2;
        mask_val = muted ? mask_mute : 0;
        break;
    }

    uint8_t reg;
    if (read_reg(ES8388_ADDR, reg_addr, reg))
    {
        reg = (reg & ~mask_mute) | (mask_val & mask_mute);
        write_reg(ES8388_ADDR, reg_addr, reg);
    }
}

/**
 * @brief Set volume gain for the main dac, or for one of the two output channels. Final gain = main gain + out channel gain 
 * 
 * @param out which gain setting to control
 * @param vol 0-100 (100 is max)
 */
void ES8388::volume(const ES8388_OUT out, const uint8_t vol)
{
    const uint32_t max_vol = 100; // max input volume value

    const int32_t max_vol_val = out == ES8388_OUT::ES_MAIN ? 96 : 0x21; // max register value for ES8388 out volume

    uint8_t lreg = 0, rreg = 0;

    switch (out)
    {
    case ES_MAIN:
        lreg = ES8388_DACCONTROL4;
        rreg = ES8388_DACCONTROL5;
        break;
    case ES_OUT1:
        lreg = ES8388_DACCONTROL24;
        rreg = ES8388_DACCONTROL25;
        break;
    case ES_OUT2:
        lreg = ES8388_DACCONTROL26;
        rreg = ES8388_DACCONTROL27;
        break;
    }

    uint8_t vol_val = vol > max_vol ? max_vol_val : (max_vol_val * vol) / max_vol;

    // main dac volume control is reverse scale (lowest value is loudest)
    // hence we reverse the calculated value
    if (out == ES_MAIN)
    {
        vol_val = max_vol_val - vol_val;
    }

    write_reg(ES8388_ADDR, lreg, vol_val);
    write_reg(ES8388_ADDR, rreg, vol_val);
}

void ES8388::SetVolumeSpeaker(uint8_t vol) {
    vol = vol * 1.6;
    volume(ES_OUT1, vol);
    volume(ES_MAIN, 100);
}

void ES8388::SetVolumeHeadphone(uint8_t vol){
    vol = vol * 1.6;
    volume(ES_OUT2, vol);
    volume(ES_MAIN, 100);    
}

/**
 * @brief Test if device with I2C address for ES8388 is connected to the I2C bus 
 * 
 * @param sda which pin to use for I2C SDA
 * @param scl which pin to use for I2C SCL
 * @param frequency which frequency to use as I2C bus frequency
 * @return true device was found
 * @return false device was not found
 */
bool ES8388::identify(int32_t sda, int32_t scl, uint32_t frequency)
{
    Wire.begin(sda, scl, frequency);
    Wire.beginTransmission(ES8388_ADDR);
    return Wire.endTransmission() == 0;
}

