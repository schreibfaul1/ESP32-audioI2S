// based on Xiph.Org Foundation celt decoder
#pragma once
//#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

#include <stdint.h>
#include <string.h>
#include <vector>
#include "../psram_unique_ptr.hpp"
using namespace std;

enum : int8_t { OPUS_END = 120,
                OPUS_CONTINUE = 110,
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
                ERR_OPUS_BUFFER_TOO_SMALL = -11,
                ERR_OPUS_COMMENT_PAGE_NOT_FOUND = -12,
                ERR_OPUS_HEAD_NOT_FOUND = -13,
                ERR_OPUS_WRONG_CONFIG_NUMBER = -14,
                ERR_OPUS_UNKNOWN_BANDWIDTH = -15,
                EER_OPUS_UNKNOWN_COUNT_CODE = -16,
                ERR_OPUS_NOT_ENOUGH_BYTES = -17,
                ERR_OPUS_WRONG_VBR_FRAME_ENGTH = -18,
                ERR_OPUS_INVALID_VFS_INDEX_ACCESS = -19,
                ERR_OPUS_CELT_UNIMPLEMENTED = -20,
                ERR_OPUS_CELT_ALLOC_FAIL = -21,
                ERR_OPUS_CELT_UNKNOWN_REQUEST = -22,
                ERR_OPUS_CELT_GET_MODE_REQUEST = - 23,
                ERR_OPUS_CELT_CLEAR_REQUEST = -24,
                ERR_OPUS_CELT_SET_CHANNELS = -25,
                ERR_OPUS_CELT_END_BAND = -26,
                ERR_OPUS_CELT_START_BAND = -27,
                ERR_OPUS_CELT_INTERNAL_ERROR = -28,
                ERR_OPUS_CELT_BAD_ARG = -29,
                ERR_OPUS_CELT_NOT_INIT = -30,
                ERR_OPUS_CBR_WITH_0_FRAMES = -35,
                ERR_OPUS_CODE_3_PACKET_WITH_NO_FRAMES = -36,
                ERR_OPUS_PACKET_TRUNCATED_DURING_PADDING_LENGTH_PARSING = -37,
                ERR_OPUS_PACKET_TRUNCATED_DURING_VBR = -38,
                ERR_OPUS_TOO_MANY_PADDING_BYTES = -39,
                ERR_OPUS_SILK_DEC_INVALID_SAMPLING_FREQUENCY = -40,  /* Output samplfreq lower than intern. decoded sampling freq */
                ERR_OPUS_SILK_DEC_PAYLOAD_TOO_LARGE          = -41,  /* Payload size exceeded the maximum allowed 1024 bytes */
                ERR_OPUS_SILK_DEC_PAYLOAD_ERROR              = -42,  /* Payload has bit errors */
                ERR_OPUS_SILK_DEC_INVALID_FRAME_SIZE         = -43,  /* Payload has bit errors */
                ERR_OPUS_SILK_DEC_NOT_INIT                   = -44};

enum {MODE_NONE = 0, MODE_SILK_ONLY = 1000, MODE_HYBRID = 1001,  MODE_CELT_ONLY = 1002};

inline const char* getOpusErrMsg(int8_t err){
    const char *p = nullptr;
    switch(err){
        case   0: p = "No error";                                                                   break;      //  0
        case  -1: p = "OPUS_CHANNELS_OUT_OF_RANGE";                                                 break;      //  1
        case  -2: p = "ERR_OPUS_INVALID_SAMPLERATE";                                                break;      //  2
        case  -3: p = "OPUS_EXTRA_CHANNELS_UNSUPPORTED";                                            break;      //  3
        case  -4: p = "ERR_OPUS_DECODER_ASYNC";                                                     break;      //  4
        case  -5: p = "OPUS_SILK_MODE_UNSUPPORTED";                                                 break;      //  5
        case  -6: p = "OPUS_HYBRID_MODE_UNSUPPORTED";                                               break;      //  6
        case  -7: p = "OPUS_NARROW_BAND_UNSUPPORTED";                                               break;      //  7
        case  -8: p = "OPUS_WIDE_BAND_UNSUPPORTED";                                                 break;      //  8
        case  -9: p = "OPUS_SUPER_WIDE_BAND_UNSUPPORTED";                                           break;      //  9
        case -10: p = "OPUS_OGG_SYNC_NOT_FOUND";                                                    break;      // 10
        case -11: p = "OPUS_BUFFER_TOO_SMALL";                                                      break;      // 11
        case -12: p = "OpusCommemtPage not found";                                                  break;      // 12
        case -13: p = "OpusHead not found";                                                         break;      // 13
        case -14: p = "Opus wrong config number";                                                   break;      // 14
        case -15: p = "Opus wron bandwidth";                                                        break;      // 15
        case -16: p = "Opus unknown count code";                                                    break;      // 16
        case -17: p = "Not enough bytes for current frame";                                         break;      // 17
        case -18: p = "Sum of signaled VBR frame lengths exceeds available compressed data bytes";  break;      // 18
        case -19: p = "Invalid VFS index access";                                                   break;      // 19
        case -20: p = "OPUS_CELT_UNIMPLEMENTED";                                                    break;      // 20
        case -21: p = "OPUS_CELT_ALLOC_FAIL";                                                       break;      // 21
        case -22: p = "OPUS_CELT_UNKNOWN_REQUEST";                                                  break;      // 22
        case -23: p = "OPUS_CELT_GET_MODE_REQUEST";                                                 break;      // 23
        case -24: p = "OPUS_CELT_CLEAR_REQUEST";                                                    break;      // 24
        case -25: p = "OPUS_CELT_SET_CHANNELS";                                                     break;      // 25
        case -26: p = "OPUS_CELT_END_BAND";                                                         break;      // 26
        case -27: p = "OPUS_CELT_START_BAND";                                                       break;      // 27
        case -28: p = "CELT_OPUS_INTERNAL_ERROR";                                                   break;      // 28
        case -29: p = "OPUS_CELT_BAD_ARG";                                                          break;      // 29
        case -30: p = "Opus CELT decoder could not be initialized";                                 break;      // 30
        case -31: p = "OPUS_UNKNOWN_ERROR";                                                         break;      // 31
        case -32: p = "OPUS_UNKNOWN_ERROR";                                                         break;      // 32
        case -33: p = "OPUS_UNKNOWN_ERROR";                                                         break;      // 33
        case -34: p = "OPUS_UNKNOWN_ERROR";                                                         break;      // 34
        case -35: p = "OPUS CBR with 0 frames";                                                     break;      // 35
        case -36: p = "OPUS Code 3 packet with no frames";                                          break;      // 36
        case -37: p = "OPUS_Packet truncated during padding length parsing";                        break;      // 37
        case -38: p = "OPUS_Packet truncated during VBR frame length parsing";                      break;      // 38
        case -39: p = "OPUS_Padding length exceeds remaining packet bytes";                         break;      // 39
        case -40: p = "SILK_DEC_INVALID_SAMPLING_FREQUENCY";                                        break;      // 40
        case -41: p = "SILK_DEC_PAYLOAD_TOO_LARGE";                                                 break;      // 41
        case -42: p = "SILK_DEC_PAYLOAD_ERROR";                                                     break;      // 42
        case -43: p = "SILK_DEC_INVALID_FRAME_SIZE";                                                break;      // 43
        case -44: p = "OPUS SILK Decoder could not be initialized";                                 break;      // 44
        default:  p = "OPUS unknown err";                                                           break;
    }
    return p;
}

typedef struct _ofp2 {
    uint16_t firstFrameLength;
    uint16_t secondFrameLength;
} ofp2;

typedef struct _ofp3 { // opus_FramePacking_Code
    bool        firstCall = true;
    bool        v = false; // VBR indicator
    bool        p = false; // padding exists
    int16_t     fs = 0;    // frame size
    uint8_t     M = 0;     // nr of frames
    int32_t     spf = 0;   // samples per frame
    int32_t     paddingLength = 0;
    uint16_t    c1fs = 0;
    uint16_t    vfs[48];   // variable frame size
    uint32_t    idx;
} ofp3;

typedef struct _odp3 {
    int8_t configNr;
    uint16_t samplesPerFrame;
} odp3;

bool             OPUSDecoder_AllocateBuffers();
void             OPUSDecoder_FreeBuffers();
void             OPUSDecoder_ClearBuffers();
void             OPUSsetDefaults();
int32_t          OPUSDecode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf);
int32_t          opusDecodePage0(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
int32_t          opusDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t *outbuf);
int8_t           opus_FramePacking_Code0(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame);
int8_t           opus_FramePacking_Code1(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
int8_t           opus_FramePacking_Code2(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
int8_t           opus_FramePacking_Code3(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
uint8_t          OPUSGetChannels();
uint32_t         OPUSGetSampRate();
uint8_t          OPUSGetBitsPerSample();
uint32_t         OPUSGetBitRate();
uint16_t         OPUSGetOutputSamps();
uint32_t         OPUSGetAudioDataStart();
const char*      OPUSgetStreamTitle();
uint16_t         OPUSgetMode();
vector<uint32_t> OPUSgetMetadataBlockPicture();
int32_t          OPUSFindSyncWord(unsigned char* buf, int32_t nBytes);
int32_t          OPUSparseOGG(uint8_t* inbuf, int32_t* bytesLeft);
int32_t          parseOpusHead(uint8_t* inbuf, int32_t nBytes);
int32_t          parseOpusComment(uint8_t* inbuf, int32_t nBytes);
int8_t           parseOpusTOC(uint8_t TOC_Byte);
int32_t          opus_packet_get_samples_per_frame(const uint8_t* data, int32_t Fs);
const char*      OPUSGetErrorMessage(int8_t err);

// some helper functions
int32_t OPUS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
