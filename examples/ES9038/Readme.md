# 32 bit ES9038

For those who have a board with a 32bit DAC
[DJ202](https://github.com/dj202) sent us a solution:


There are some great sounding and cheap 32 bit decoders available like boards with the ES9038Q2M that don't work because the library outputs 16 bits data.
You only need to change 3 lines lines of code to change it to 32bit:
## Audio.cpp
in Audio::Audio
````c++
m_i2s_std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
in Audio::resampleTo48kStereo:

m_samplesBuff48K[outputIndex * 2] = clipToInt16(outLeft);
m_samplesBuff48K[outputIndex * 2 + 1] = 0x00;
m_samplesBuff48K[outputIndex * 2 + 2] = clipToInt16(outRight);
m_samplesBuff48K[outputIndex * 2 + 3] = 0x00;
````
