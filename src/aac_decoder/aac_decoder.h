
#pragma once

#include <stdint.h>
#pragma GCC diagnostic warning "-Wunused-function"
bool        AACDecoder_IsInit();
bool        AACDecoder_AllocateBuffers();
void        AACDecoder_FreeBuffers();
uint8_t     AACGetFormat();
uint8_t     AACGetParametricStereo();
uint8_t     AACGetSBR();
int         AACFindSyncWord(uint8_t *buf, int nBytes);
int         AACSetRawBlockParams(int copyLast, int nChans, int sampRateCore, int profile);
int16_t     AACGetOutputSamps();
int         AACGetBitrate();
int         AACGetChannels();
int         AACGetSampRate();
int         AACGetBitsPerSample();
int         AACDecode(uint8_t *inbuf, int32_t *bytesLeft, short *outbuf);
const char* AACGetErrorMessage(int8_t err);