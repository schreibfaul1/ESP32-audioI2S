// based om helix mp3 decoder
#pragma once

#include "Audio.h"
#include "tables.h"

enum {
    MP3_NONE = 0,
    MP3_ERR = -1,
    MP3_STOP = -100,
};

bool        MP3Decoder_AllocateBuffers(void);
bool        MP3Decoder_IsInit();
void        MP3Decoder_FreeBuffers();
int32_t     MP3Decode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf);
void        MP3GetLastFrameInfo();
int32_t     MP3GetNextFrameInfo(uint8_t* buf);
int32_t     MP3FindSyncWord(uint8_t* buf, int32_t nBytes);
int32_t     MP3GetSampRate();
int32_t     MP3GetChannels();
int32_t     MP3GetBitsPerSample();
int32_t     MP3GetBitrate();
int32_t     MP3GetOutputSamps();
const char* MP3GetLayer();
const char* MP3GetMPEGVersion();

// internally used
int      MP3_AnalyzeFrame(const uint8_t* frame_data, size_t frame_len);
void     MP3Decoder_ClearBuffer(void);
void     PolyphaseMono(int16_t* pcm, int32_t* vbuf, const uint32_t* coefBase);
void     PolyphaseStereo(int16_t* pcm, int32_t* vbuf, const uint32_t* coefBase);
void     SetBitstreamPointer(BitStreamInfo_t* bsi, int32_t nBytes, uint8_t* buf);
uint32_t GetBits(BitStreamInfo_t* bsi, int32_t nBits);
int32_t  CalcBitsUsed(BitStreamInfo_t* bsi, uint8_t* startBuf, int32_t startOffset);
int32_t  DequantChannel(int32_t* sampleBuf, int32_t* workBuf, int32_t* nonZeroBound, SideInfoSub_t* sis, ScaleFactorInfoSub_t* sfis, CriticalBandInfo_t* cbi);
void     MidSideProc(int32_t x[MAX_NCHAN][MAX_NSAMP], int32_t nSamps, int32_t mOut[2]);
void     IntensityProcMPEG1(int32_t x[MAX_NCHAN][MAX_NSAMP], int32_t nSamps, ScaleFactorInfoSub_t* sfis, CriticalBandInfo_t* cbi, int32_t midSideFlag, int32_t mixFlag, int32_t mOut[2]);
void     IntensityProcMPEG2(int32_t x[MAX_NCHAN][MAX_NSAMP], int32_t nSamps, ScaleFactorInfoSub_t* sfis, CriticalBandInfo_t* cbi, ScaleFactorJS_t* sfjs, int32_t midSideFlag, int32_t mixFlag, int32_t mOut[2]);
void     FDCT32(int32_t* x, int32_t* d, int32_t offset, int32_t oddBlock, int32_t gb); // __attribute__ ((section (".data")));
int32_t  CheckPadBit();
int32_t  UnpackFrameHeader(uint8_t* buf);
int32_t  UnpackSideInfo(uint8_t* buf);
int32_t  DecodeHuffman(uint8_t* buf, int32_t* bitOffset, int32_t huffBlockBits, int32_t gr, int32_t ch);
int32_t  MP3Dequantize(int32_t gr);
int32_t  IMDCT(int32_t gr, int32_t ch);
int32_t  UnpackScaleFactors(uint8_t* buf, int32_t* bitOffset, int32_t bitsAvail, int32_t gr, int32_t ch);
int32_t  Subband(int16_t* pcmBuf);
int16_t  ClipToShort(int32_t x, int32_t fracBits);
void     RefillBitstreamCache(BitStreamInfo_t* bsi);
void     UnpackSFMPEG1(BitStreamInfo_t* bsi, SideInfoSub_t* sis, ScaleFactorInfoSub_t* sfis, int32_t* scfsi, int32_t gr, ScaleFactorInfoSub_t* sfisGr0);
void     UnpackSFMPEG2(BitStreamInfo_t* bsi, SideInfoSub_t* sis, ScaleFactorInfoSub_t* sfis, int32_t gr, int32_t ch, int32_t modeExt, ScaleFactorJS_t* sfjs);
int32_t  MP3FindFreeSync(uint8_t* buf, uint8_t firstFH[4], int32_t nBytes);
void     MP3ClearBadFrame(int16_t* outbuf);
int32_t  DecodeHuffmanPairs(int32_t* xy, int32_t nVals, int32_t tabIdx, int32_t bitsLeft, uint8_t* buf, int32_t bitOffset);
int32_t  DecodeHuffmanQuads(int32_t* vwxy, int32_t nVals, int32_t tabIdx, int32_t bitsLeft, uint8_t* buf, int32_t bitOffset);
int32_t  DequantBlock(int32_t* inbuf, int32_t* outbuf, int32_t num, int32_t scale);
void     AntiAlias(int32_t* x, int32_t nBfly);
void     WinPrevious(int32_t* xPrev, int32_t* xPrevWin, int32_t btPrev);
int32_t  FreqInvertRescale(int32_t* y, int32_t* xPrev, int32_t blockIdx, int32_t es);
void     idct9(int32_t* x);
int32_t  IMDCT36(int32_t* xCurr, int32_t* xPrev, int32_t* y, int32_t btCurr, int32_t btPrev, int32_t blockIdx, int32_t gb);
void     imdct12(int32_t* x, int32_t* out);
int32_t  IMDCT12x3(int32_t* xCurr, int32_t* xPrev, int32_t* y, int32_t btPrev, int32_t blockIdx, int32_t gb);
int32_t  HybridTransform(int32_t* xCurr, int32_t* xPrev, int32_t y[BLOCK_SIZE][NBANDS], SideInfoSub_t* sis, BlockCount_t* bc);
uint64_t SAR64(uint64_t x, int32_t n);
int32_t  MULSHIFT32(int32_t x, int32_t y);
uint64_t MADD64(uint64_t sum64, int32_t x, int32_t y); /* returns 64-bit value in [edx:eax] */
uint64_t xSAR64(uint64_t x, int32_t n);
int32_t  FASTABS(int32_t x); // xtensa has a fast abs instruction //fb

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// Macro for comfortable calls
#define MP3_LOG_ERROR(fmt, ...)   Audio::AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MP3_LOG_WARN(fmt, ...)    Audio::AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MP3_LOG_INFO(fmt, ...)    Audio::AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MP3_LOG_DEBUG(fmt, ...)   Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MP3_LOG_VERBOSE(fmt, ...) Audio::AUDIO_LOG_IMPL(5, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
