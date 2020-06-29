// based om helix aac decoder
#pragma once
#pragma GCC optimize ("O3")

#include "Arduino.h"

#define ASSERT(x) /* do nothing */

#ifndef MAX
#define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#endif


/* AAC file format */
enum {
    AAC_FF_Unknown = 0,        /* should be 0 on init */
    AAC_FF_ADTS    = 1,
    AAC_FF_ADIF    = 2,
    AAC_FF_RAW     =  3
};

/* syntactic element type */
enum {
    AAC_ID_INVALID = -1,
    AAC_ID_SCE     =  0,
    AAC_ID_CPE     =  1,
    AAC_ID_CCE     =  2,
    AAC_ID_LFE     =  3,
    AAC_ID_DSE     =  4,
    AAC_ID_PCE     =  5,
    AAC_ID_FIL     =  6,
    AAC_ID_END     =  7
};

enum {
    ERR_AAC_NONE                          =   0,
    ERR_AAC_INDATA_UNDERFLOW              =  -1,
    ERR_AAC_NULL_POINTER                  =  -2,
    ERR_AAC_INVALID_ADTS_HEADER           =  -3,
    ERR_AAC_INVALID_ADIF_HEADER           =  -4,
    ERR_AAC_INVALID_FRAME                 =  -5,
    ERR_AAC_MPEG4_UNSUPPORTED             =  -6,
    ERR_AAC_CHANNEL_MAP                   =  -7,
    ERR_AAC_SYNTAX_ELEMENT                =  -8,
    ERR_AAC_DEQUANT                       =  -9,
    ERR_AAC_STEREO_PROCESS                = -10,
    ERR_AAC_PNS                           = -11,
    ERR_AAC_SHORT_BLOCK_DEINT             = -12,
    ERR_AAC_TNS                           = -13,
    ERR_AAC_IMDCT                         = -14,
    ERR_AAC_NCHANS_TOO_HIGH               = -15,
    ERR_AAC_RAWBLOCK_PARAMS               = -22,
    ERR_AAC_UNKNOWN                        = -9999
};

typedef struct _AACDecInfo_t {
    int fillExtType;
    int prevBlockID;    /* block information */
    int currBlockID;
    int currInstTag;
    int sbDeinterleaveReqd[2]; // [MAX_NCHANS_ELEM]
    int adtsBlocksLeft;
    int bitRate;    /* user-accessible info */
    int nChans;
    int sampRate;
    int profile;
    int format;
    int sbrEnabled;
    int tnsUsed;
    int pnsUsed;
    int frameCount;
} AACDecInfo_t;


typedef struct _aac_BitStreamInfo_t {
    uint8_t *bytePtr;
    uint32_t iCache;
    int cachedBits;
    int nBytes;
} aac_BitStreamInfo_t;

typedef union _U64 {
    int64_t w64;
    struct {
        uint32_t lo32;
        int32_t  hi32;
    } r;
} U64;

typedef struct _AACFrameInfo_t {
    int bitRate;
    int nChans;
    int sampRateCore;
    int sampRateOut;
    int bitsPerSample;
    int outputSamps;
    int profile;
    int tnsUsed;
    int pnsUsed;
} AACFrameInfo_t;

typedef struct _HuffInfo_t {
    int maxBits;              /* number of bits in longest codeword */
    uint8_t count[20];        /*  count[MAX_HUFF_BITS] = number of codes with length i+1 bits */
    int offset;               /* offset into symbol table */
} HuffInfo_t;

typedef struct _PulseInfo_t {
    uint8_t pulseDataPresent;
    uint8_t numPulse;
    uint8_t startSFB;
    uint8_t offset[4]; // [MAX_PULSES]
    uint8_t amp[4];    // [MAX_PULSES]
} PulseInfo_t;

typedef struct _TNSInfo_t {
    uint8_t tnsDataPresent;
    uint8_t numFilt[8]; // [MAX_TNS_FILTERS] max 1 filter each for 8 short windows, or 3 filters for 1 long window
    uint8_t coefRes[8]; // [MAX_TNS_FILTERS]
    uint8_t length[8];  // [MAX_TNS_FILTERS]
    uint8_t order[8];   // [MAX_TNS_FILTERS]
    uint8_t dir[8];     // [MAX_TNS_FILTERS]
    int8_t   coef[60];  // [MAX_TNS_COEFS] max 3 filters * 20 coefs for 1 long window, or 1 filter * 7 coefs for each of 8 short windows
} TNSInfo_t;

typedef struct _GainControlInfo_t {
    uint8_t gainControlDataPresent;
    uint8_t maxBand;
    uint8_t adjNum[3][8];      // [MAX_GAIN_BANDS][MAX_GAIN_WIN]
    uint8_t alevCode[3][8][7]; // [MAX_GAIN_BANDS][MAX_GAIN_WIN][MAX_GAIN_ADJUST]
    uint8_t alocCode[3][8][7]; // [MAX_GAIN_BANDS][MAX_GAIN_WIN][MAX_GAIN_ADJUST]
} GainControlInfo_t;

typedef struct _ICSInfo_t {
    uint8_t icsResBit;
    uint8_t winSequence;
    uint8_t winShape;
    uint8_t maxSFB;
    uint8_t sfGroup;
    uint8_t predictorDataPresent;
    uint8_t predictorReset;
    uint8_t predictorResetGroupNum;
    uint8_t predictionUsed[41]; // [MAX_PRED_SFB]
    uint8_t numWinGroup;
    uint8_t winGroupLen[8];     // [MAX_WIN_GROUPS]
} ICSInfo_t;

typedef struct _ADTSHeader_t {
    /* fixed */
    uint8_t id;                             /* MPEG bit - should be 1 */
    uint8_t layer;                          /* MPEG layer - should be 0 */
    uint8_t protectBit;                     /* 0 = CRC word follows, 1 = no CRC word */
    uint8_t profile;                        /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    uint8_t sampRateIdx;                    /* sample rate index range = [0, 11] */
    uint8_t privateBit;                     /* ignore */
    uint8_t channelConfig;                  /* 0 = implicit, >0 = use default table */
    uint8_t origCopy;                       /* 0 = copy, 1 = original */
    uint8_t home;                           /* ignore */
    /* variable */
    uint8_t copyBit;                        /* 1 bit of the 72-bit copyright ID (transmitted as 1 bit per frame) */
    uint8_t copyStart;                      /* 1 = this bit starts the 72-bit ID, 0 = it does not */
    int     frameLength;                    /* length of frame */
    int     bufferFull;                     /* number of 32-bit words left in enc buffer, 0x7FF = VBR */
    uint8_t numRawDataBlocks;               /* number of raw data blocks in frame */
    /* CRC */
    int     crcCheckWord;                   /* 16-bit CRC check word (present if protectBit == 0) */
} ADTSHeader_t;

typedef struct _ADIFHeader_t {
    uint8_t copyBit;                        /* 0 = no copyright ID, 1 = 72-bit copyright ID follows immediately */
    uint8_t origCopy;                       /* 0 = copy, 1 = original */
    uint8_t home;                           /* ignore */
    uint8_t bsType;                         /* bitstream type: 0 = CBR, 1 = VBR */
    int     bitRate;                        /* bitRate: CBR = bits/sec, VBR = peak bits/frame, 0 = unknown */
    uint8_t numPCE;                         /* number of program config elements (max = 16) */
    int     bufferFull;                     /* bits left in bit reservoir */
    uint8_t copyID[9];                      /* [ADIF_COPYID_SIZE] optional 72-bit copyright ID */
} ADIFHeader_t;

/* sizeof(ProgConfigElement_t) = 82 bytes (if KEEP_PCE_COMMENTS not defined) */
typedef struct _ProgConfigElement_t {
    uint8_t elemInstTag;   /* element instance tag */
    uint8_t profile;       /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    uint8_t sampRateIdx;   /* sample rate index range = [0, 11] */
    uint8_t numFCE;        /* number of front channel elements (max = 15) */
    uint8_t numSCE;        /* number of side channel elements (max = 15) */
    uint8_t numBCE;        /* number of back channel elements (max = 15) */
    uint8_t numLCE;        /* number of LFE channel elements (max = 3) */
    uint8_t numADE;        /* number of associated data elements (max = 7) */
    uint8_t numCCE;        /* number of valid channel coupling elements (max = 15) */
    uint8_t monoMixdown;   /* mono mixdown: bit 4 = present flag, bits 3-0 = element number */
    uint8_t stereoMixdown; /* stereo mixdown: bit 4 = present flag, bits 3-0 = element number */
    uint8_t matrixMixdown; /* bit 4 = present flag, bit 3 = unused,bits 2-1 = index, bit 0 = pseudo-surround enable */
    uint8_t fce[15];       /* [MAX_NUM_FCE] front element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    uint8_t sce[15];       /* [MAX_NUM_SCE] side element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    uint8_t bce[15];       /* [MAX_NUM_BCE] back element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    uint8_t lce[3];        /* [MAX_NUM_LCE] instance tag for LFE elements */
    uint8_t ade[7];        /* [MAX_NUM_ADE] instance tag for ADE elements */
    uint8_t cce[15];       /* [MAX_NUM_BCE] channel coupling elements: bit 4 = switching flag, bits 3-0 = inst tag */
} ProgConfigElement_t;

/* state info struct for baseline (MPEG-4 LC) decoding */
typedef struct _PSInfoBase_t {
    int                   dataCount;
    uint8_t               dataBuf[510]; // [DATA_BUF_SIZE]
    /* state information which is the same throughout whole frame */
    int                   nChans;
    int                   useImpChanMap;
    int                   sampRateIdx;
    /* state information which can be overwritten by subsequent elements within frame */
    ICSInfo_t             icsInfo[2]; // [MAX_NCHANS_ELEM]
    int                   commonWin;
    short                 scaleFactors[2][15*8]; // [MAX_NCHANS_ELEM][MAX_SF_BANDS]
    uint8_t               sfbCodeBook[2][15*8]; // [MAX_NCHANS_ELEM][MAX_SF_BANDS]
    int                   msMaskPresent;
    uint8_t               msMaskBits[(15 * 8 + 7) >> 3]; // [MAX_MS_MASK_BYTES]
    int                   pnsUsed[2]; // [MAX_NCHANS_ELEM]
    int                   pnsLastVal;
    int                   intensityUsed[2]; // [MAX_NCHANS_ELEM]
//    PulseInfo_t           pulseInfo[2]; // [MAX_NCHANS_ELEM]
    TNSInfo_t             tnsInfo[2]; // [MAX_NCHANS_ELEM]
    int                   tnsLPCBuf[20]; // [MAX_TNS_ORDER]
    int                   tnsWorkBuf[20]; //[MAX_TNS_ORDER]
    GainControlInfo_t     gainControlInfo[2]; // [MAX_NCHANS_ELEM]
    int                   gbCurrent[2];  // [MAX_NCHANS_ELEM]
    int                   coef[2][1024]; // [MAX_NCHANS_ELEM][AAC_MAX_NSAMPS]
    /* state information which must be saved for each element and used in next frame */
    int                   overlap[2][1024];  // [AAC_MAX_NCHANS][AAC_MAX_NSAMPS]
    int                   prevWinShape[2]; // [AAC_MAX_NCHANS]
} PSInfoBase_t;

bool AACDecoder_AllocateBuffers(void);
void AACDecoder_FreeBuffers(void);
int AACFindSyncWord(uint8_t *buf, int nBytes);
void AACGetLastFrameInfo(AACFrameInfo_t *aacFrameInfo);
int AACDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf);
int AACGetSampRate();
int AACGetChannels();
int AACGetBitsPerSample();
int AACGetBitrate();
int AACGetOutputSamps();
int AACGetBitrate();
void DecodeLPCCoefs(int order, int res, int8_t *filtCoef, int *a, int *b);
int FilterRegion(int size, int dir, int order, int *audioCoef, int *a, int *hist);
int TNSFilter(int ch);
int DecodeSingleChannelElement();
int DecodeChannelPairElement();
int DecodeLFEChannelElement();
int DecodeDataStreamElement();
int DecodeProgramConfigElement(uint8_t idx);
int DecodeFillElement();
int DecodeNextElement(uint8_t **buf, int *bitOffset, int *bitsAvail);
void PreMultiply(int tabidx, int *zbuf1);
void PostMultiply(int tabidx, int *fft1);
void PreMultiplyRescale(int tabidx, int *zbuf1, int es);
void PostMultiplyRescale(int tabidx, int *fft1, int es);
void DCT4(int tabidx, int *coef, int gb);
void BitReverse(int *inout, int tabidx);
void R4FirstPass(int *x, int bg);
void R8FirstPass(int *x, int bg);
void R4Core(int *x, int bg, int gp, int *wtab);
void R4FFT(int tabidx, int *x);
void UnpackZeros(int nVals, int *coef);
void UnpackQuads(int cb, int nVals, int *coef);
void UnpackPairsNoEsc(int cb, int nVals, int *coef);
void UnpackPairsEsc(int cb, int nVals, int *coef);
void DecodeSpectrumLong(int ch);
void DecodeSpectrumShort(int ch);
void DecWindowOverlap(int *buf0, int *over0, short *pcm0, int nChans, int winTypeCurr, int winTypePrev);
void DecWindowOverlapLongStart(int *buf0, int *over0, short *pcm0, int nChans, int winTypeCurr, int winTypePrev);
void DecWindowOverlapLongStop(int *buf0, int *over0, short *pcm0, int nChans, int winTypeCurr, int winTypePrev);
void DecWindowOverlapShort(int *buf0, int *over0, short *pcm0, int nChans, int winTypeCurr, int winTypePrev);
int IMDCT(int ch, int chOut, short *outbuf);
void DecodeICSInfo(ICSInfo_t *icsInfo, int sampRateIdx);
void DecodeSectionData(int winSequence, int numWinGrp, int maxSFB, uint8_t *sfbCodeBook);
int DecodeOneScaleFactor();
void DecodeScaleFactors(int numWinGrp, int maxSFB, int globalGain, uint8_t *sfbCodeBook, short *scaleFactors);
void DecodePulseInfo(uint8_t ch);
void DecodeTNSInfo(int winSequence, TNSInfo_t *ti, int8_t *tnsCoef);
void DecodeGainControlInfo(int winSequence, GainControlInfo_t *gi);
void DecodeICS(int ch);
int DecodeNoiselessData(uint8_t **buf, int *bitOffset, int *bitsAvail, int ch);
int DecodeHuffmanScalar(const signed short *huffTab, const HuffInfo_t *huffTabInfo, uint32_t bitBuf, int32_t *val);
int UnpackADTSHeader(uint8_t **buf, int *bitOffset, int *bitsAvail);
int GetADTSChannelMapping(uint8_t *buf, int bitOffset, int bitsAvail);
int GetNumChannelsADIF(int nPCE);
int GetSampleRateIdxADIF(int nPCE);
int UnpackADIFHeader(uint8_t **buf, int *bitOffset, int *bitsAvail);
int SetRawBlockParams(int copyLast, int nChans, int sampRate, int profile);
int PrepareRawBlock();
int DequantBlock(int *inbuf, int nSamps, int scale);
int AACDequantize(int ch);
int DeinterleaveShortBlocks(int ch);
uint32_t Get32BitVal(uint32_t *last);
int InvRootR(int r);
int ScaleNoiseVector(int *coef, int nVals, int sf);
void GenerateNoiseVector(int *coef, int *last, int nVals);
void CopyNoiseVector(int *coefL, int *coefR, int nVals);
int PNS(int ch);
int GetSampRateIdx(int sampRate);
void StereoProcessGroup(int *coefL, int *coefR, const uint16_t *sfbTab, int msMaskPres, uint8_t *msMaskPtr,
int msMaskOffset, int maxSFB, uint8_t *cbRight, short *sfRight, int *gbCurrent);
int StereoProcess();
int RatioPowInv(int a, int b, int c);
int SqrtFix(int q, int fBitsIn, int *fBitsOut);
int InvRNormalized(int r);
void BitReverse32(int *inout);
void R8FirstPass32(int *r0);
void R4Core32(int *r0);
void FFT32C(int *x);
void CVKernel1(int *XBuf, int *accBuf);
void CVKernel2(int *XBuf, int *accBuf);
void SetBitstreamPointer(int nBytes, uint8_t *buf);
inline void RefillBitstreamCache();
uint32_t GetBits(int nBits);
uint32_t GetBitsNoAdvance(int nBits);
void AdvanceBitstream(int nBits);
int CalcBitsUsed(uint8_t *startBuf, int startOffset);
void ByteAlignBitstream();

