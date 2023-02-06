// based on Xiph.Org Foundation celt decoder
#pragma once
//#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

#include "Arduino.h"

enum : int8_t  {OPUS_PARSE_OGG_DONE = 100,
                ERR_OPUS_NONE = 0,
                ERR_OPUS_NR_OF_CHANNELS_UNSUPPORTED = -1,
                ERR_OPUS_INVALID_SAMPLERATE = -2,
                ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED = -3,
                ERR_OPUS_DECODER_ASYNC = -4,
                ERR_OPUS_SILK_MODE_UNSUPPORTED = -5,
                ERR_OPUS_HYBRID_MODE_UNSUPPORTED = -6,
                ERR_OPUS_OGG_SYNC_NOT_FOUND = - 7,
                ERR_OPUS_CELT_BAD_ARG = -18,
                ERR_OPUS_CELT_INTERNAL_ERROR = -19,
                ERR_OPUS_CELT_UNIMPLEMENTED = -20,
                ERR_OPUS_CELT_ALLOC_FAIL = -21};

bool     OPUSDecoder_AllocateBuffers();
void     OPUSDecoder_FreeBuffers();
void     OPUSDecoder_ClearBuffers();
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
int      parseOpusTOC(uint8_t TOC_Byte);
int32_t  opus_packet_get_samples_per_frame(const uint8_t *data, int32_t Fs);

// some helper functions
int OPUS_specialIndexOf(uint8_t* base, const char* str, int baselen, bool exact = false);