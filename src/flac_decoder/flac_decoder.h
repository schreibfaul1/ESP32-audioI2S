/*
 * flac_decoder.h
 *
 * Created on: Jul 03,2020
 * Updated on: Mar 29,2025
 *
 *      Author: wolle
 *
 *  Restrictions:
 *  blocksize must not exceed 24576 bytes
 *  bits per sample must be 8 or 16
 *  num Channels must be 1 or 2
 *
 *
 */
#pragma once
#pragma GCC optimize ("Ofast")

#include "Arduino.h"
#include <vector>
#include "../psram_unique_ptr.hpp"
using namespace std;

#define FLAC_MAX_CHANNELS 2
#define FLAC_MAX_BLOCKSIZE 24576  // 24 * 1024
#define FLAC_MAX_OUTBUFFSIZE 4096 * 2

enum : uint8_t {FLACDECODER_INIT, FLACDECODER_READ_IN, FLACDECODER_WRITE_OUT};
enum : uint8_t {DECODE_FRAME, DECODE_SUBFRAMES, OUT_SAMPLES};
enum : int8_t  {FLAC_PARSE_OGG_DONE = 100,
                FLAC_DECODE_FRAMES_LOOP = 100,
                FLAC_OGG_SYNC_FOUND = +2,
                GIVE_NEXT_LOOP = +1,
                FLAC_NONE = 0,
                FLAC_ERR = -1,
                FLAC_STOP = -100,
};

typedef struct FLACMetadataBlock_t{
                              // METADATA_BLOCK_STREAMINFO
    uint16_t minblocksize;    // The minimum block size (in samples) used in the stream.
                              //----------------------------------------------------------------------------------------
                              // The maximum block size (in samples) used in the stream.
    uint16_t maxblocksize;    // (Minimum blocksize == maximum blocksize) implies a fixed-blocksize stream.
                              //----------------------------------------------------------------------------------------
                              // The minimum frame size (in bytes) used in the stream.
    uint32_t minframesize;    // May be 0 to imply the value is not known.
                              //----------------------------------------------------------------------------------------
                              // The maximum frame size (in bytes) used in the stream.
    uint32_t maxframesize;    // May be 0 to imply the value is not known.
                              //----------------------------------------------------------------------------------------
                              // Sample rate in Hz. Though 20 bits are available,
                              // the maximum sample rate is limited by the structure of frame headers to 655350Hz.
    uint32_t sampleRate;      // Also, a value of 0 is invalid.
                              //----------------------------------------------------------------------------------------
                              // Number of channels FLAC supports from 1 to 8 channels
    uint8_t  numChannels;     // 000 : 1 channel .... 111 : 8 channels
                              //----------------------------------------------------------------------------------------
                              // Sample size in bits:
                              // 000 : get from STREAMINFO metadata block
                              // 001 : 8 bits per sample
                              // 010 : 12 bits per sample
                              // 011 : reserved
                              // 100 : 16 bits per sample
                              // 101 : 20 bits per sample
                              // 110 : 24 bits per sample
     uint8_t  bitsPerSample;  // 111 : reserved
                              //----------------------------------------------------------------------------------------
                              // Total samples in stream. 'Samples' means inter-channel sample,
                              // i.e. one second of 44.1Khz audio will have 44100 samples regardless of the number
     uint64_t totalSamples;   // of channels. A value of zero here means the number of total samples is unknown.
                              //----------------------------------------------------------------------------------------
     uint32_t audioDataLength;// is not the filelength, is only the length of the audio datablock in bytes



}FLACMetadataBlock_t;


typedef struct FLACFrameHeader_t {
                              // 0 : fixed-blocksize stream; frame header encodes the frame number
    uint8_t blockingStrategy; // 1 : variable-blocksize stream; frame header encodes the sample number
                              //----------------------------------------------------------------------------------------
                              // Block size in inter-channel samples:
                              // 0000 : reserved
                              // 0001 : 192 samples
                              // 0010-0101 : 576 * (2^(n-2)) samples, i.e. 576/1152/2304/4608
                              // 0110 : get 8 bit (blocksize-1) from end of header
                              // 0111 : get 16 bit (blocksize-1) from end of header
    uint8_t blockSizeCode;    // 1000-1111 : 256 * (2^(n-8)) samples, i.e. 256/512/1024/2048/4096/8192/16384/32768
                              //----------------------------------------------------------------------------------------
                              // 0000 : get from STREAMINFO metadata block
                              // 0001 : 88.2kHz
                              // 0010 : 176.4kHz
                              // 0011 : 192kHz
                              // 0100 : 8kHz
                              // 0101 : 16kHz
                              // 0110 : 22.05kHz
                              // 0111 : 24kHz
                              // 1000 : 32kHz
                              // 1001 : 44.1kHz
                              // 1010 : 48kHz
                              // 1011 : 96kHz
                              // 1100 : get 8 bit sample rate (in kHz) from end of header
                              // 1101 : get 16 bit sample rate (in Hz) from end of header
                              // 1110 : get 16 bit sample rate (in tens of Hz) from end of header
    uint8_t sampleRateCode;   // 1111 : invalid, to prevent sync-fooling string of 1s
                              //----------------------------------------------------------------------------------------
                              // Channel assignment
                              // 0000 1 channel: mono
                              // 0001 2 channels: left, right
                              // 0010 3 channels
                              // 0011 4 channels
                              // 0100 5 channels
                              // 0101 6 channels
                              // 0110 7 channels
                              // 0111 8 channels
                              // 1000 : left/side stereo: channel 0 is the left channel, channel 1 is the side(difference) channel
                              // 1001 : right/side stereo: channel 0 is the side(difference) channel, channel 1 is the right channel
                              // 1010 : mid/side stereo: channel 0 is the mid(average) channel, channel 1 is the side(difference) channel
    uint8_t chanAsgn;         // 1011-1111 : reserved
                              //----------------------------------------------------------------------------------------
                              // Sample size in bits:
                              // 000 : get from STREAMINFO metadata block
                              // 001 : 8 bits per sample
                              // 010 : 12 bits per sample
                              // 011 : reserved
                              // 100 : 16 bits per sample
                              // 101 : 20 bits per sample
                              // 110 : 24 bits per sample
    uint8_t sampleSizeCode;   // 111 : reserved
                              //----------------------------------------------------------------------------------------
    uint32_t totalSamples;    // totalSamplesInStream
                              //----------------------------------------------------------------------------------------
    uint32_t bitrate;         // bitrate


}FLACFrameHeader_t;

int32_t          FLACFindSyncWord(unsigned char* buf, int32_t nBytes);
boolean          FLACFindMagicWord(unsigned char* buf, int32_t nBytes);
char*            FLACgetStreamTitle();
int32_t          FLACparseOGG(uint8_t* inbuf, int32_t* bytesLeft);
vector<uint32_t> FLACgetMetadataBlockPicture();
int32_t          parseFlacFirstPacket(uint8_t* inbuf, int16_t nBytes);
int32_t          parseMetaDataBlockHeader(uint8_t* inbuf, int16_t nBytes);
bool             FLACDecoder_AllocateBuffers(void);
void             FLACDecoder_setDefaults();
void             FLACDecoder_ClearBuffer();
void             FLACDecoder_FreeBuffers();
void             FLACSetRawBlockParams(uint8_t Chans, uint32_t SampRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength);
void             FLACDecoderReset();
int8_t           FLACDecode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf);
int8_t           FLACDecodeNative(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf);
int8_t           flacDecodeFrame(uint8_t* inbuf, int32_t* bytesLeft);
uint32_t         FLACGetOutputSamps();
uint64_t         FLACGetTotoalSamplesInStream();
uint8_t          FLACGetBitsPerSample();
uint8_t          FLACGetChannels();
uint32_t         FLACGetSampRate();
uint32_t         FLACGetBitRate();
uint32_t         FLACGetAudioDataStart();
uint32_t         FLACGetAudioFileDuration();
uint32_t         readUint(uint8_t nBits, int32_t* bytesLeft);
int32_t          readSignedInt(int32_t nBits, int32_t* bytesLeft);
int64_t          readRiceSignedInt(uint8_t param, int32_t* bytesLeft);
void             alignToByte();
int8_t           decodeSubframes(int32_t* bytesLeft);
int8_t           decodeSubframe(uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft);
int8_t           decodeFixedPredictionSubframe(uint8_t predOrder, uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft);
int8_t           decodeLinearPredictiveCodingSubframe(int32_t lpcOrder, int32_t sampleDepth, uint8_t ch, int32_t* bytesLeft);
int8_t           decodeResiduals(uint8_t warmup, uint8_t ch, int32_t* bytesLeft);
void             restoreLinearPrediction(uint8_t ch, uint8_t shift);
int32_t          FLAC_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  L O G G I N G   📌📌📌
extern __attribute__((weak)) void audio_info(const char*);

template <typename... Args>
void FLAC_LOG_IMPL(uint8_t level, const char* path, int line, const char* fmt, Args&&... args) {
    #define ANSI_ESC_RESET          "\033[0m"
    #define ANSI_ESC_BLACK          "\033[30m"
    #define ANSI_ESC_RED            "\033[31m"
    #define ANSI_ESC_GREEN          "\033[32m"
    #define ANSI_ESC_YELLOW         "\033[33m"
    #define ANSI_ESC_BLUE           "\033[34m"
    #define ANSI_ESC_MAGENTA        "\033[35m"
    #define ANSI_ESC_CYAN           "\033[36m"
    #define ANSI_ESC_WHITE          "\033[37m"

    ps_ptr<char> result;
    ps_ptr<char> file;

    file.copy_from(path);
    while(file.contains("/")){
        file.remove_before('/', false);
    }

    // First run: determine size
    int len = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
    if (len <= 0) return;

    result.alloc(len + 1, "result");
    char* dst = result.get();
    if (!dst) return;
    std::snprintf(dst, len + 1, fmt, std::forward<Args>(args)...);

    // build a final string with file/line prefix
    ps_ptr<char> final;
    int total_len = std::snprintf(nullptr, 0, "%s:%d:" ANSI_ESC_RED " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
    if (total_len <= 0) return;
    final.calloc(total_len + 1, "final");
    final.clear();
    char* dest = final.get();
    if (!dest) return;  // Or error treatment
    if(audio_info){
        if     (level == 1 && CORE_DEBUG_LEVEL >= 1) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_RED " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 2 && CORE_DEBUG_LEVEL >= 2) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_YELLOW " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 3 && CORE_DEBUG_LEVEL >= 3) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_GREEN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 4 && CORE_DEBUG_LEVEL >= 4) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_CYAN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else              if( CORE_DEBUG_LEVEL >= 5) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_WHITE" %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        if(final.strlen()) audio_info(final.get());
    }
    else{
        std::snprintf(dest, total_len + 1, "%s:%d: %s", file.c_get(), line, dst);
        if     (level == 1) log_e("%s", final.c_get());
        else if(level == 2) log_w("%s", final.c_get());
        else if(level == 3) log_i("%s", final.c_get());
        else if(level == 4) log_d("%s", final.c_get());
        else                log_v("%s", final.c_get());
    }
    final.reset();
    result.reset();
}

// Macro for comfortable calls
#define FLAC_LOG_ERROR(fmt, ...)   FLAC_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FLAC_LOG_WARN(fmt, ...)    FLAC_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FLAC_LOG_INFO(fmt, ...)    FLAC_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FLAC_LOG_DEBUG(fmt, ...)   FLAC_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FLAC_LOG_VERBOSE(fmt, ...) FLAC_LOG_IMPL(5, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

