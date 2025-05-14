/*
	AC101 - An AC101 Codec driver library for Arduino
	Copyright (C) 2019, Ivo Pullens, Emmission

	Inspired by:
	https://github.com/donny681/esp-adf/tree/master/components/audio_hal/driver/AC101

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Febr  2021 modified by schreibfaul1  - set correct pll values
    March 2021 modified by schreibfaul1  - can handle two i2c instances
    May   2021 modified by schreibfaul1  - constructor changed
    Oct   2021 modified by schreibfaul1  - I2C wrong ACK in ReadReg
    Jan   2022 modified by schreibfaul1  - left right channel swapped
	Jan   2022 modified by schreibfaul1  - suppress compiler warning: left shift of negative value

	examples:

	//one I2C bus: (default behaviour)
	AC101 ac;
	ac.begin(sda, scl);

	//two I2C busses:
	TwoWire i2cBusOne = TwoWire(0);
	TwoWire i2cBusTwo = TwoWire(1);
    AC101 ac(&i2cBusOne);

    i2cBusOne.begin(sda, scl, 400000);
*/

#include "AC101.h"

#define BCLK        // clock over BCLK comment out: clock over MCLK

#define AC101_ADDR			0x1A				// Device address

#define CHIP_AUDIO_RS		0x00
#define PLL_CTRL1			0x01
#define PLL_CTRL2			0x02
#define SYSCLK_CTRL			0x03
#define MOD_CLK_ENA			0x04
#define MOD_RST_CTRL		0x05
#define I2S_SR_CTRL			0x06
#define I2S1LCK_CTRL		0x10
#define I2S1_SDOUT_CTRL		0x11
#define I2S1_SDIN_CTRL		0x12
#define I2S1_MXR_SRC		0x13
#define I2S1_VOL_CTRL1		0x14
#define I2S1_VOL_CTRL2		0x15
#define I2S1_VOL_CTRL3		0x16
#define I2S1_VOL_CTRL4		0x17
#define I2S1_MXR_GAIN		0x18
#define ADC_DIG_CTRL		0x40
#define ADC_VOL_CTRL		0x41
#define HMIC_CTRL1			0x44
#define HMIC_CTRL2			0x45
#define HMIC_STATUS			0x46
#define DAC_DIG_CTRL		0x48
#define DAC_VOL_CTRL		0x49
#define DAC_MXR_SRC			0x4C
#define DAC_MXR_GAIN		0x4D
#define ADC_APC_CTRL		0x50
#define ADC_SRC				0x51
#define ADC_SRCBST_CTRL		0x52
#define OMIXER_DACA_CTRL	0x53
#define OMIXER_SR			0x54
#define OMIXER_BST1_CTRL	0x55
#define HPOUT_CTRL			0x56
#define SPKOUT_CTRL			0x58
#define AC_DAC_DAPCTRL		0xA0
#define AC_DAC_DAPHHPFC 	0xA1
#define AC_DAC_DAPLHPFC 	0xA2
#define AC_DAC_DAPLHAVC 	0xA3
#define AC_DAC_DAPLLAVC 	0xA4
#define AC_DAC_DAPRHAVC 	0xA5
#define AC_DAC_DAPRLAVC 	0xA6
#define AC_DAC_DAPHGDEC 	0xA7
#define AC_DAC_DAPLGDEC 	0xA8
#define AC_DAC_DAPHGATC 	0xA9
#define AC_DAC_DAPLGATC 	0xAA
#define AC_DAC_DAPHETHD 	0xAB
#define AC_DAC_DAPLETHD 	0xAC
#define AC_DAC_DAPHGKPA 	0xAD
#define AC_DAC_DAPLGKPA 	0xAE
#define AC_DAC_DAPHGOPA 	0xAF
#define AC_DAC_DAPLGOPA 	0xB0
#define AC_DAC_DAPOPT   	0xB1
#define DAC_DAP_ENA     	0xB5

#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))

const uint8_t regs[] = {
	 CHIP_AUDIO_RS		,
	 PLL_CTRL1			,
	 PLL_CTRL2			,
	 SYSCLK_CTRL		,
	 MOD_CLK_ENA		,
	 MOD_RST_CTRL		,
	 I2S_SR_CTRL		,
	 I2S1LCK_CTRL		,
	 I2S1_SDOUT_CTRL	,
	 I2S1_SDIN_CTRL		,
	 I2S1_MXR_SRC		,
	 I2S1_VOL_CTRL1		,
	 I2S1_VOL_CTRL2		,
	 I2S1_VOL_CTRL3		,
	 I2S1_VOL_CTRL4		,
	 I2S1_MXR_GAIN		,
	 ADC_DIG_CTRL		,
	 ADC_VOL_CTRL		,
	 HMIC_CTRL1			,
	 HMIC_CTRL2			,
	 HMIC_STATUS		,
	 DAC_DIG_CTRL		,
	 DAC_VOL_CTRL		,
	 DAC_MXR_SRC		,
	 DAC_MXR_GAIN		,
	 ADC_APC_CTRL		,
	 ADC_SRC			,
	 ADC_SRCBST_CTRL	,
	 OMIXER_DACA_CTRL	,
	 OMIXER_SR			,
	 OMIXER_BST1_CTRL	,
	 HPOUT_CTRL			,
	 SPKOUT_CTRL		,
	 AC_DAC_DAPCTRL		,
	 AC_DAC_DAPHHPFC 	,
	 AC_DAC_DAPLHPFC 	,
	 AC_DAC_DAPLHAVC 	,
	 AC_DAC_DAPLLAVC 	,
	 AC_DAC_DAPRHAVC 	,
	 AC_DAC_DAPRLAVC 	,
	 AC_DAC_DAPHGDEC 	,
	 AC_DAC_DAPLGDEC 	,
	 AC_DAC_DAPHGATC 	,
	 AC_DAC_DAPLGATC 	,
	 AC_DAC_DAPHETHD 	,
	 AC_DAC_DAPLETHD 	,
	 AC_DAC_DAPHGKPA 	,
	 AC_DAC_DAPLGKPA 	,
	 AC_DAC_DAPHGOPA 	,
	 AC_DAC_DAPLGOPA 	,
	 AC_DAC_DAPOPT   	,
	 DAC_DAP_ENA
};
//----------------------------------------------------------------------------------------------------------------------
bool AC101::WriteReg(uint8_t reg, uint16_t val)
{
	_TwoWireInstance->beginTransmission(AC101_ADDR);
	_TwoWireInstance->write(reg);
	_TwoWireInstance->write(uint8_t((val >> 8) & 0xff));
	_TwoWireInstance->write(uint8_t(val & 0xff));
	return 0 == _TwoWireInstance->endTransmission(true);
}

uint16_t AC101::ReadReg(uint8_t reg)
{
	_TwoWireInstance->beginTransmission(AC101_ADDR);
	_TwoWireInstance->write(reg);
	_TwoWireInstance->endTransmission(false);

	uint16_t val = 0u;
	if (2 == _TwoWireInstance->requestFrom(uint16_t(AC101_ADDR), uint8_t(2)))
	{
		val = uint16_t(_TwoWireInstance->read() << 8) + uint16_t(_TwoWireInstance->read());
	}
	_TwoWireInstance->endTransmission(true);
	return val;
}
//----------------------------------------------------------------------------------------------------------------------
AC101::AC101( TwoWire *TwoWireInstance ){
    _TwoWireInstance = TwoWireInstance;
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::begin(int32_t sda, int32_t scl, uint32_t frequency) {
    bool ok;
    if((sda >= 0) && (scl >= 0)){
	    ok = _TwoWireInstance->begin(sda, scl, frequency);
    }
    else {
        ok = true;
    }

	// Reset all registers, readback default as sanity check
	ok &= WriteReg(CHIP_AUDIO_RS, 0x123);
	delay(100);
	ok &= 0x0101 == ReadReg(CHIP_AUDIO_RS);
	ok &= WriteReg(SPKOUT_CTRL, 0xe880);
	// Enable the PLL from 256*44.1KHz MCLK source
	ok &= WriteReg(PLL_CTRL1, 0x0141);
	uint16_t N = 48 << 4;                   /* 512 / (M * (2*K+1)) / (CHANNELS * WORD_SIZE) -> 512 / 3 * (2 * 16) */
	uint16_t PLL_EN = 1 << 15;
	uint16_t N_f = 0<<0;                    /* 0.2 N */
	ok &= WriteReg(PLL_CTRL2, N | PLL_EN | N_f);

	// Clocking system
	uint16_t PLLCLK_ENA = 1<<15;            /* 0: Disable, 1: Enable */
#ifdef BCLK
	uint16_t PLL_CLK = 0x2 << 12;           /* bclk1 */
	uint16_t I2S1CLK_SRC = 0x3<<8;          /* PLL */
#else
	uint16_t PLL_CLK = 0x0 << 12;           /* MCLK1 */
	uint16_t I2S1CLK_SRC = 0x0<<8;          /* MLCK1 */
#endif
	uint16_t I2S1CLK_ENA = 1<<11;           /* 0: Disable, 1: Enable */

	uint16_t SYSCLK_ENA = 1<<3;
	ok &= WriteReg(SYSCLK_CTRL, PLLCLK_ENA|PLL_CLK| I2S1CLK_ENA|I2S1CLK_SRC|SYSCLK_ENA/*0x8b08*/);

	ok &= WriteReg(MOD_CLK_ENA, 0x800c);
	ok &= WriteReg(MOD_RST_CTRL, 0x800c);

	// Set default at I2S, 44.1KHz, 16bit
	ok &= SetI2sSampleRate(SAMPLE_RATE_44100);
	ok &= SetI2sClock(BCLK_DIV_8, false, LRCK_DIV_32, false);
	ok &= SetI2sMode(MODE_SLAVE);
	ok &= SetI2sWordSize(WORD_SIZE_16_BITS);
	ok &= SetI2sFormat(DATA_FORMAT_I2S);

	// AIF config
	ok &= WriteReg(I2S1_SDOUT_CTRL, 0xc000);
	ok &= WriteReg(I2S1_SDIN_CTRL, 0xc000);
	ok &= WriteReg(I2S1_MXR_SRC, 0x2200);

	ok &= WriteReg(ADC_SRCBST_CTRL, 0xccc4);
	ok &= WriteReg(ADC_SRC, 0x1040);
	ok &= WriteReg(ADC_DIG_CTRL, 0x8000);
	ok &= WriteReg(ADC_APC_CTRL, 0xbbc3);

	// Path Configuration
	ok &= WriteReg(DAC_MXR_SRC, 0xcc00);
	ok &= WriteReg(DAC_DIG_CTRL, 0x8000);
	ok &= WriteReg(OMIXER_SR, 0x0081);
	ok &= WriteReg(OMIXER_DACA_CTRL, 0xf080);

	ok &= SetMode( MODE_DAC );

	return ok;
}
//----------------------------------------------------------------------------------------------------------------------
void AC101::DumpRegisters() {
	for (size_t i = 0; i < ARRAY_SIZE(regs); ++i){
		Serial.print(regs[i], HEX);
		Serial.print(" = ");
		Serial.println(ReadReg(regs[i]), HEX);
	}
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t AC101::GetVolumeSpeaker() {
    // Times 2, to scale to same range as headphone volume
    return (ReadReg(SPKOUT_CTRL) & 31) * 2;
}

bool AC101::SetVolumeSpeaker(uint8_t volume) {
    // Divide by 2, as it is scaled to same range as headphone volume
    volume /= 2;
    if(volume > 31) volume = 31;

    uint16_t val = ReadReg(SPKOUT_CTRL);
    val &= ~31;
    val |= volume;
    return WriteReg(SPKOUT_CTRL, val);
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t AC101::GetVolumeHeadphone() {
    return (ReadReg(HPOUT_CTRL) >> 4) & 63;
}

bool AC101::SetVolumeHeadphone(uint8_t volume) {
    if(volume > 63) volume = 63;

    uint16_t val = ReadReg(HPOUT_CTRL);
    val &= ~63U << 4;
    val |= volume << 4;
    return WriteReg(HPOUT_CTRL, val);
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::SetI2sSampleRate(I2sSampleRate_t rate) {
    return WriteReg(I2S_SR_CTRL, rate);
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::SetI2sMode(I2sMode_t mode) {
    uint16_t val = ReadReg(I2S1LCK_CTRL);
    val &= ~0x8000;
    val |= uint16_t(mode) << 15;
    return WriteReg(I2S1LCK_CTRL, val);
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::SetI2sWordSize(I2sWordSize_t size) {
    uint16_t val = ReadReg(I2S1LCK_CTRL);
    val &= ~0x0030;
    val |= uint16_t(size) << 4;
    return WriteReg(I2S1LCK_CTRL, val);
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::SetI2sFormat(I2sFormat_t format) {
    uint16_t val = ReadReg(I2S1LCK_CTRL);
    val &= ~0x000C;
    val |= uint16_t(format) << 2;
    return WriteReg(I2S1LCK_CTRL, val);
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::SetI2sClock(I2sBitClockDiv_t bitClockDiv, bool bitClockInv, I2sLrClockDiv_t lrClockDiv, bool lrClockInv) {
    uint16_t val = ReadReg(I2S1LCK_CTRL);
    val &= ~0x7FC0;
    val |= uint16_t(bitClockInv ? 1 : 0) << 14;
    val |= uint16_t(bitClockDiv) << 9;
    val |= uint16_t(lrClockInv ? 1 : 0) << 13;
    val |= uint16_t(lrClockDiv) << 6;
    return WriteReg(I2S1LCK_CTRL, val);
}
//----------------------------------------------------------------------------------------------------------------------
bool AC101::SetMode(Mode_t mode) {
    bool ok = true;
    if(MODE_LINE == mode) {
        ok &= WriteReg(ADC_SRC, 0x0408);
        ok &= WriteReg(ADC_DIG_CTRL, 0x8000);
        ok &= WriteReg(ADC_APC_CTRL, 0x3bc0);
    }

    if((MODE_ADC == mode) or (MODE_ADC_DAC == mode) or (MODE_LINE == mode)) {
        ok &= WriteReg(MOD_CLK_ENA, 0x800c);
        ok &= WriteReg(MOD_RST_CTRL, 0x800c);
    }

    if((MODE_DAC == mode) or (MODE_ADC_DAC == mode) or (MODE_LINE == mode)) {
        // Enable Headphone output
        ok &= WriteReg(OMIXER_DACA_CTRL, 0xff80);
        ok &= WriteReg(HPOUT_CTRL, 0xc3c1);
        ok &= WriteReg(HPOUT_CTRL, 0xcb00);
        delay(100);
        ok &= WriteReg(HPOUT_CTRL, 0xfbc0);
        ok &= SetVolumeHeadphone(30);

        // Enable Speaker output
        ok &= WriteReg(SPKOUT_CTRL, 0xeabd);
        delay(10);
        ok &= SetVolumeSpeaker(30);
    }
    return ok;
}
