// based on Xiph.Org Foundation celt decoder
#pragma once
//#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

#include <stdint.h>
#include <string.h>
#include <vector>
using namespace std;

enum : int8_t  {OPUS_CONTINUE = 110,
                OPUS_PARSE_OGG_DONE = 100,
                ERR_OPUS_NONE = 0,
                ERR_OPUS_CHANNELS_OUT_OF_RANGE = -1,
                ERR_OPUS_INVALID_SAMPLERATE = -2,
                ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED = -3,
                ERR_OPUS_DECODER_ASYNC = -4,
                ERR_OPUS_SILK_MODE_UNSUPPORTED = -5,
                ERR_OPUS_HYBRID_MODE_UNSUPPORTED = -6,
                ERR_OPUS_NARROW_BAND_UNSUPPORTED = -7,
                ERR_OPUS_WIDE_BAND_UNSUPPORTED = -8,
                ERR_OPUS_SUPER_WIDE_BAND_UNSUPPORTED = -9,
                ERR_OPUS_OGG_SYNC_NOT_FOUND = - 10,
                ERR_OPUS_CELT_BAD_ARG = -18,
                ERR_OPUS_CELT_INTERNAL_ERROR = -19,
                ERR_OPUS_CELT_UNIMPLEMENTED = -20,
                ERR_OPUS_CELT_ALLOC_FAIL = -21,
                ERR_OPUS_CELT_UNKNOWN_REQUEST = -22,
                ERR_OPUS_CELT_GET_MODE_REQUEST = - 23,
                ERR_OPUS_CELT_CLEAR_REQUEST = -24,
                ERR_OPUS_CELT_SET_CHANNELS = -25,
                ERR_OPUS_CELT_END_BAND = -26,
                ERR_CELT_OPUS_INTERNAL_ERROR = -27};

bool             OPUSDecoder_AllocateBuffers();
void             OPUSDecoder_FreeBuffers();
void             OPUSDecoder_ClearBuffers();
void             OPUSsetDefaults();
int              OPUSDecode(uint8_t* inbuf, int* bytesLeft, short* outbuf);
int              opusDecodePage0(uint8_t* inbuf, int* bytesLeft, uint32_t segmentLength);
int              opusDecodePage3(uint8_t* inbuf, int* bytesLeft, uint32_t segmentLength, short *outbuf);
int8_t           opus_FramePacking_Code0(uint8_t *inbuf, int *bytesLeft, short *outbuf, int packetLen, uint16_t samplesPerFrame);
int8_t           opus_FramePacking_Code1(uint8_t *inbuf, int *bytesLeft, short *outbuf, int packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
int8_t           opus_FramePacking_Code2(uint8_t *inbuf, int *bytesLeft, short *outbuf, int packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
int8_t           opus_FramePacking_Code3(uint8_t *inbuf, int *bytesLeft, short *outbuf, int packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
uint8_t          OPUSGetChannels();
uint32_t         OPUSGetSampRate();
uint8_t          OPUSGetBitsPerSample();
uint32_t         OPUSGetBitRate();
uint16_t         OPUSGetOutputSamps();
uint32_t         OPUSGetAudioDataStart();
char*            OPUSgetStreamTitle();
vector<uint32_t> OPUSgetMetadataBlockPicture();
int              OPUSFindSyncWord(unsigned char* buf, int nBytes);
int              OPUSparseOGG(uint8_t* inbuf, int* bytesLeft);
int              parseOpusHead(uint8_t* inbuf, int nBytes);
int              parseOpusComment(uint8_t* inbuf, int nBytes);
int8_t           parseOpusTOC(uint8_t TOC_Byte);
int32_t          opus_packet_get_samples_per_frame(const uint8_t* data, int32_t Fs);

// some helper functions
int OPUS_specialIndexOf(uint8_t* base, const char* str, int baselen, bool exact = false);
