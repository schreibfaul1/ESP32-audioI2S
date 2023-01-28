// based on Xiph.Org Foundation celt decoder
#pragma once
//#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

#include "Arduino.h"
#include <vector>



enum : int8_t  {ERR_OPUS_NONE = 0,
                ERR_OPUS_NR_OF_CHANNELS_UNSUPPORTED = -1,
                ERR_OPUS_INVALID_SAMPLERATE = -2,
                ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED = -3,
                ERR_OPUS_DECODER_ASYNC = -4,
                ERR_OPUS_SILK_MODE_UNSUPPORTED,
                ERR_OPUS_HYBRID_MODE_UNSUPPORTED};



bool     OPUSDecoder_AllocateBuffers();
void     OPUSDecoder_FreeBuffers();
int      OPUSDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf);
uint8_t  OPUSGetChannels();
uint32_t OPUSGetSampRate();
uint8_t  OPUSGetBitsPerSample();
uint32_t OPUSGetBitRate();
uint16_t OPUSGetOutputSamps();
char    *OPUSgetStreamTitle();
int      OPUSFindSyncWord(unsigned char *buf, int nBytes);
int      OPUSparseOGG(uint8_t *inbuf, int *bytesLeft);
int      parseOpusHead(uint8_t *inbuf, int nBytes);
int      parseOpusComment(uint8_t *inbuf, int nBytes);
int      parseOpusFramePacket();

// some helper functions
int OPUS_specialIndexOf(uint8_t* base, const char* str, int baselen, bool exact = false);