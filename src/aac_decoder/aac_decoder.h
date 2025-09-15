/*
 *  aac_decoder.h
 *  faad2 - ESP32 adaptation
 *  Created on: 12.09.2023
 *  Updated on: 13.08.2024
*/


#pragma once

#include "Audio.h"
#include "libfaad/neaacdec.h"

#pragma GCC diagnostic warning "-Wunused-function"

struct AudioSpecificConfig {
    uint8_t audioObjectType;
    uint8_t samplingFrequencyIndex;
    uint8_t channelConfiguration;
};

bool        AACDecoder_IsInit();
bool        AACDecoder_AllocateBuffers();
void        AACDecoder_FreeBuffers();
uint8_t     AACGetFormat();
uint8_t     AACGetParametricStereo();
uint8_t     AACGetSBR();
int         AACFindSyncWord(uint8_t *buf, int nBytes);
int         AACSetRawBlockParams(int nChans, int sampRateCore, int profile);
int16_t     AACGetOutputSamps();
int         AACGetBitrate();
int         AACGetChannels();
int         AACGetSampRate();
int         AACGetBitsPerSample();
int         AACDecode(uint8_t *inbuf, int32_t *bytesLeft, short *outbuf);
const char* AACGetErrorMessage(int8_t err);

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// Macro for comfortable calls
#define AAC_LOG_ERROR(fmt, ...)   Audio::AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_WARN(fmt, ...)    Audio::AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_INFO(fmt, ...)    Audio::AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_DEBUG(fmt, ...)   Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_VERBOSE(fmt, ...) Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)