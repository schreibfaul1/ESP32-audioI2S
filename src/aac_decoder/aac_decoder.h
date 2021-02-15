// based om helix aac decoder
#pragma once
#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

#include "Arduino.h"

#define AAC_ENABLE_MPEG4
//#define AAC_ENABLE_SBR  // needs additional 60KB Heap,

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
    ERR_AAC_SBR_INIT                      = -16,
    ERR_AAC_SBR_BITSTREAM                 = -17,
    ERR_AAC_SBR_DATA                      = -18,
    ERR_AAC_SBR_PCM_FORMAT                = -19,
    ERR_AAC_SBR_NCHANS_TOO_HIGH           = -20,
    ERR_AAC_SBR_SINGLERATE_UNSUPPORTED    = -21,
    ERR_AAC_RAWBLOCK_PARAMS               = -22,
    ERR_AAC_UNKNOWN                       = -9999
};

enum {
    SBR_GRID_FIXFIX = 0,
    SBR_GRID_FIXVAR = 1,
    SBR_GRID_VARFIX = 2,
    SBR_GRID_VARVAR = 3
};

enum {
    HuffTabSBR_tEnv15 =    0,
    HuffTabSBR_fEnv15 =    1,
    HuffTabSBR_tEnv15b =   2,
    HuffTabSBR_fEnv15b =   3,
    HuffTabSBR_tEnv30 =    4,
    HuffTabSBR_fEnv30 =    5,
    HuffTabSBR_tEnv30b =   6,
    HuffTabSBR_fEnv30b =   7,
    HuffTabSBR_tNoise30 =  8,
    HuffTabSBR_fNoise30 =  5,
    HuffTabSBR_tNoise30b = 9,
    HuffTabSBR_fNoise30b = 7
};

typedef struct _AACDecInfo_t {
    /* raw decoded data, before rounding to 16-bit PCM (for postprocessing such as SBR) */
    void *rawSampleBuf[2];
    int rawSampleBytes;
    int rawSampleFBits;
    /* fill data (can be used for processing SBR or other extensions) */
    unsigned char *fillBuf;
    int fillCount;
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
    unsigned char *bytePtr;
    unsigned int iCache;
    int cachedBits;
    int nBytes;
} aac_BitStreamInfo_t;

typedef union _U64 {
    int64_t w64;
    struct {
        unsigned int lo32;
        signed int  hi32;
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
    unsigned char count[20];        /*  count[MAX_HUFF_BITS] = number of codes with length i+1 bits */
    int offset;               /* offset into symbol table */
} HuffInfo_t;

typedef struct _PulseInfo_t {
    unsigned char pulseDataPresent;
    unsigned char numPulse;
    unsigned char startSFB;
    unsigned char offset[4]; // [MAX_PULSES]
    unsigned char amp[4];    // [MAX_PULSES]
} PulseInfo_t;

typedef struct _TNSInfo_t {
    unsigned char tnsDataPresent;
    unsigned char numFilt[8]; // [MAX_TNS_FILTERS] max 1 filter each for 8 short windows, or 3 filters for 1 long window
    unsigned char coefRes[8]; // [MAX_TNS_FILTERS]
    unsigned char length[8];  // [MAX_TNS_FILTERS]
    unsigned char order[8];   // [MAX_TNS_FILTERS]
    unsigned char dir[8];     // [MAX_TNS_FILTERS]
    int8_t   coef[60];        // [MAX_TNS_COEFS] max 3 filters * 20 coefs for 1 long window,
                              //  or 1 filter * 7 coefs for each of 8 short windows
} TNSInfo_t;

typedef struct _GainControlInfo_t {
    unsigned char gainControlDataPresent;
    unsigned char maxBand;
    unsigned char adjNum[3][8];      // [MAX_GAIN_BANDS][MAX_GAIN_WIN]
    unsigned char alevCode[3][8][7]; // [MAX_GAIN_BANDS][MAX_GAIN_WIN][MAX_GAIN_ADJUST]
    unsigned char alocCode[3][8][7]; // [MAX_GAIN_BANDS][MAX_GAIN_WIN][MAX_GAIN_ADJUST]
} GainControlInfo_t;

typedef struct _ICSInfo_t {
    unsigned char icsResBit;
    unsigned char winSequence;
    unsigned char winShape;
    unsigned char maxSFB;
    unsigned char sfGroup;
    unsigned char predictorDataPresent;
    unsigned char predictorReset;
    unsigned char predictorResetGroupNum;
    unsigned char predictionUsed[41]; // [MAX_PRED_SFB]
    unsigned char numWinGroup;
    unsigned char winGroupLen[8];     // [MAX_WIN_GROUPS]
} ICSInfo_t;

typedef struct _ADTSHeader_t {
    /* fixed */
    unsigned char id;                         /* MPEG bit - should be 1 */
    unsigned char layer;                      /* MPEG layer - should be 0 */
    unsigned char protectBit;                 /* 0 = CRC word follows, 1 = no CRC word */
    unsigned char profile;                    /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    unsigned char sampRateIdx;                /* sample rate index range = [0, 11] */
    unsigned char privateBit;                 /* ignore */
    unsigned char channelConfig;              /* 0 = implicit, >0 = use default table */
    unsigned char origCopy;                   /* 0 = copy, 1 = original */
    unsigned char home;                       /* ignore */
    /* variable */
    unsigned char copyBit;                    /* 1 bit of the 72-bit copyright ID (transmitted as 1 bit per frame) */
    unsigned char copyStart;                  /* 1 = this bit starts the 72-bit ID, 0 = it does not */
    int           frameLength;                /* length of frame */
    int           bufferFull;                 /* number of 32-bit words left in enc buffer, 0x7FF = VBR */
    unsigned char numRawDataBlocks;           /* number of raw data blocks in frame */
    /* CRC */
    int     crcCheckWord;                     /* 16-bit CRC check word (present if protectBit == 0) */
} ADTSHeader_t;

typedef struct _ADIFHeader_t {
    unsigned char copyBit;                    /* 0 = no copyright ID, 1 = 72-bit copyright ID follows immediately */
    unsigned char origCopy;                   /* 0 = copy, 1 = original */
    unsigned char home;                       /* ignore */
    unsigned char bsType;                     /* bitstream type: 0 = CBR, 1 = VBR */
    int           bitRate;                    /* bitRate: CBR = bits/sec, VBR = peak bits/frame, 0 = unknown */
    unsigned char numPCE;                     /* number of program config elements (max = 16) */
    int           bufferFull;                 /* bits left in bit reservoir */
    unsigned char copyID[9];                  /* [ADIF_COPYID_SIZE] optional 72-bit copyright ID */
} ADIFHeader_t;

/* sizeof(ProgConfigElement_t) = 82 bytes (if KEEP_PCE_COMMENTS not defined) */
typedef struct _ProgConfigElement_t {
    unsigned char elemInstTag;   /* element instance tag */
    unsigned char profile;       /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    unsigned char sampRateIdx;   /* sample rate index range = [0, 11] */
    unsigned char numFCE;        /* number of front channel elements (max = 15) */
    unsigned char numSCE;        /* number of side channel elements (max = 15) */
    unsigned char numBCE;        /* number of back channel elements (max = 15) */
    unsigned char numLCE;        /* number of LFE channel elements (max = 3) */
    unsigned char numADE;        /* number of associated data elements (max = 7) */
    unsigned char numCCE;        /* number of valid channel coupling elements (max = 15) */
    unsigned char monoMixdown;   /* mono mixdown: bit 4 = present flag, bits 3-0 = element number */
    unsigned char stereoMixdown; /* stereo mixdown: bit 4 = present flag, bits 3-0 = element number */
    unsigned char matrixMixdown; /* bit 4 = present flag, bit 3 = unused,bits 2-1 = index, bit 0 = pseudo-surround enable */
    unsigned char fce[15];       /* [MAX_NUM_FCE] front element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    unsigned char sce[15];       /* [MAX_NUM_SCE] side element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    unsigned char bce[15];       /* [MAX_NUM_BCE] back element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    unsigned char lce[3];        /* [MAX_NUM_LCE] instance tag for LFE elements */
    unsigned char ade[7];        /* [MAX_NUM_ADE] instance tag for ADE elements */
    unsigned char cce[15];       /* [MAX_NUM_BCE] channel coupling elements: bit 4 = switching flag, bits 3-0 = inst tag */
} ProgConfigElement_t;

typedef struct _SBRHeader {
    int                   count;

    unsigned char         ampRes;
    unsigned char         startFreq;
    unsigned char         stopFreq;
    unsigned char         crossOverBand;
    unsigned char         resBitsHdr;
    unsigned char         hdrExtra1;
    unsigned char         hdrExtra2;

    unsigned char         freqScale;
    unsigned char         alterScale;
    unsigned char         noiseBands;

    unsigned char         limiterBands;
    unsigned char         limiterGains;
    unsigned char         interpFreq;
    unsigned char         smoothMode;
} SBRHeader;

/* need one SBRGrid per channel, updated every frame */
typedef struct _SBRGrid {
    unsigned char         frameClass;
    unsigned char         ampResFrame;
    unsigned char         pointer;

    unsigned char         numEnv;                       /* L_E */
    unsigned char         envTimeBorder[5 + 1];   // [MAX_NUM_ENV+1] /* t_E */
    unsigned char         freqRes[5];             // [MAX_NUM_ENV]/* r */

    unsigned char         numNoiseFloors;                           /* L_Q */
    unsigned char         noiseTimeBorder[2 + 1]; // [MAX_NUM_NOISE_FLOORS+1] /* t_Q */

    unsigned char         numEnvPrev;
    unsigned char         numNoiseFloorsPrev;
    unsigned char         freqResPrev;
} SBRGrid;

/* need one SBRFreq per element (SCE/CPE/LFE), updated only on header reset */
typedef struct _SBRFreq {
    int                   kStart;               /* k_x */
    int                   nMaster;
    int                   nHigh;
    int                   nLow;
    int                   nLimiter;             /* N_l */
    int                   numQMFBands;          /* M */
    int                   numNoiseFloorBands;   /* Nq */

    int                   kStartPrev;
    int                   numQMFBandsPrev;

    unsigned char         freqMaster[48 + 1];     // [MAX_QMF_BANDS + 1]      /* not necessary to save this  after derived tables are generated */
    unsigned char         freqHigh[48 + 1];       // [MAX_QMF_BANDS + 1]
    unsigned char         freqLow[48 / 2 + 1];    // [MAX_QMF_BANDS / 2 + 1]  /* nLow = nHigh - (nHigh >> 1) */
    unsigned char         freqNoise[5 + 1];       // [MAX_NUM_NOISE_FLOOR_BANDS+1]
    unsigned char         freqLimiter[48 / 2 + 5];// [MAX_QMF_BANDS / 2 + MAX_NUM_PATCHES]    /* max (intermediate) size = nLow + numPatches - 1 */

    unsigned char         numPatches;
    unsigned char         patchNumSubbands[5 + 1];  // [MAX_NUM_PATCHES + 1]
    unsigned char         patchStartSubband[5 + 1]; // [MAX_NUM_PATCHES + 1]
} SBRFreq;

typedef struct _SBRChan {
    int                   reset;
    unsigned char         deltaFlagEnv[5];          // [MAX_NUM_ENV]
    unsigned char         deltaFlagNoise[2];        // [MAX_NUM_NOISE_FLOORS]

    signed char           envDataQuant[5][48];      // [MAX_NUM_ENV][MAX_QMF_BANDS] /* range = [0, 127] */
    signed char           noiseDataQuant[2][5];     // [MAX_NUM_NOISE_FLOORS][MAX_NUM_NOISE_FLOOR_BANDS]

    unsigned char         invfMode[2][5];           // [2][MAX_NUM_NOISE_FLOOR_BANDS] /* invfMode[0/1][band] = prev/curr */
    int                   chirpFact[5];             // [MAX_NUM_NOISE_FLOOR_BANDS]  /* bwArray */
    unsigned char         addHarmonicFlag[2];       /* addHarmonicFlag[0/1] = prev/curr */
    unsigned char         addHarmonic[2][64];       /* addHarmonic[0/1][band] = prev/curr */

    int                   gbMask[2];    /* gbMask[0/1] = XBuf[0-31]/XBuf[32-39] */
    signed char           laPrev;

    int                   noiseTabIndex;
    int                   sinIndex;
    int                   gainNoiseIndex;
    int                   gTemp[5][48];  // [MAX_NUM_SMOOTH_COEFS][MAX_QMF_BANDS]
    int                   qTemp[5][48];  // [MAX_NUM_SMOOTH_COEFS][MAX_QMF_BANDS]

} SBRChan;


/* state info struct for baseline (MPEG-4 LC) decoding */
typedef struct _PSInfoBase_t {
    int                   dataCount;
    unsigned char               dataBuf[510]; // [DATA_BUF_SIZE]
    int                   fillCount;
    unsigned char         fillBuf[269]; //[FILL_BUF_SIZE]
    /* state information which is the same throughout whole frame */
    int                   nChans;
    int                   useImpChanMap;
    int                   sampRateIdx;
    /* state information which can be overwritten by subsequent elements within frame */
    ICSInfo_t             icsInfo[2]; // [MAX_NCHANS_ELEM]
    int                   commonWin;
    short                 scaleFactors[2][15*8]; // [MAX_NCHANS_ELEM][MAX_SF_BANDS]
    unsigned char               sfbCodeBook[2][15*8]; // [MAX_NCHANS_ELEM][MAX_SF_BANDS]
    int                   msMaskPresent;
    unsigned char               msMaskBits[(15 * 8 + 7) >> 3]; // [MAX_MS_MASK_BYTES]
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
#ifdef AAC_ENABLE_SBR
    int                   sbrWorkBuf[2][1024]; // [MAX_NCHANS_ELEM][AAC_MAX_NSAMPS];
#endif
    /* state information which must be saved for each element and used in next frame */
    int                   overlap[2][1024];  // [AAC_MAX_NCHANS][AAC_MAX_NSAMPS]
    int                   prevWinShape[2]; // [AAC_MAX_NCHANS]
} PSInfoBase_t;

typedef struct _PSInfoSBR {
    /* save for entire file */
    int                   frameCount;
    int                   sampRateIdx;

    /* state info that must be saved for each channel */
    SBRHeader             sbrHdr[2];
    SBRGrid               sbrGrid[2];
    SBRFreq               sbrFreq[2];
    SBRChan               sbrChan[2];

    /* temp variables, no need to save between blocks */
    unsigned char         dataExtra;
    unsigned char         resBitsData;
    unsigned char         extendedDataPresent;
    int                   extendedDataSize;

    signed char           envDataDequantScale[2][5];  // [MAX_NCHANS_ELEM][MAX_NUM_ENV
    int                   envDataDequant[2][5][48];   // [MAX_NCHANS_ELEM][MAX_NUM_ENV][MAX_QMF_BANDS
    int                   noiseDataDequant[2][2][5];  // [MAX_NCHANS_ELEM][MAX_NUM_NOISE_FLOORS][MAX_NUM_NOISE_FLOOR_BANDS]

    int                   eCurr[48];    // [MAX_QMF_BANDS]
    unsigned char         eCurrExp[48]; // [MAX_QMF_BANDS]
    unsigned char         eCurrExpMax;
    signed char           la;

    int                   crcCheckWord;
    int                   couplingFlag;
    int                   envBand;
    int                   eOMGainMax;
    int                   gainMax;
    int                   gainMaxFBits;
    int                   noiseFloorBand;
    int                   qp1Inv;
    int                   qqp1Inv;
    int                   sMapped;
    int                   sBand;
    int                   highBand;

    int                   sumEOrigMapped;
    int                   sumECurrGLim;
    int                   sumSM;
    int                   sumQM;
    int                   gLimBoost[48];
    int                   qmLimBoost[48];
    int                   smBoost[48];

    int                   smBuf[48];
    int                   qmLimBuf[48];
    int                   gLimBuf[48];
    int                   gLimFbits[48];

    int                   gFiltLast[48];
    int                   qFiltLast[48];

    /* large buffers */
    int                   delayIdxQMFA[2];        // [AAC_MAX_NCHANS]
    int                   delayQMFA[2][10 * 32];  // [AAC_MAX_NCHANS][DELAY_SAMPS_QMFA]
    int                   delayIdxQMFS[2];        // [AAC_MAX_NCHANS]
    int                   delayQMFS[2][10 * 128]; // [AAC_MAX_NCHANS][DELAY_SAMPS_QMFS]
    int                   XBufDelay[2][8][64][2]; // [AAC_MAX_NCHANS][HF_GEN][64][2]
    int                   XBuf[32+8][64][2];

} PSInfoSBR_t;

#define CLIP_2N(y, n) { \
    int sign = (y) >> 31;  \
    if (sign != (y) >> (n))  { \
        (y) = sign ^ ((1 << (n)) - 1); \
    } \
}

/* do y <<= n, clipping to range [-2^30, 2^30 - 1] (i.e. output has one guard bit) */
#define CLIP_2N_SHIFT30(y, n) { \
    int sign = (y) >> 31;  \
    if (sign != (y) >> (30 - (n)))  { \
        (y) = sign ^ (0x3fffffff); \
    } else { \
        (y) = (y) << (n); \
    } \
}

bool AACDecoder_AllocateBuffers(void);
int AACFlushCodec();
void AACDecoder_FreeBuffers(void);
int AACFindSyncWord(unsigned char *buf, int nBytes);
int AACSetRawBlockParams(int copyLast, int nChans, int sampRateCore, int profile);
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
int DecodeHuffmanScalar(const signed short *huffTab, const HuffInfo_t *huffTabInfo, unsigned int bitBuf, int32_t *val);
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
unsigned int Get32BitVal(unsigned int *last);
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
unsigned int GetBits(int nBits);
unsigned int GetBitsNoAdvance(int nBits);
void AdvanceBitstream(int nBits);
int CalcBitsUsed(uint8_t *startBuf, int startOffset);
void ByteAlignBitstream();
// SBR
void InitSBRState();
int DecodeSBRBitstream(int chBase);
int DecodeSBRData(int chBase, short *outbuf);
int FlushCodecSBR();
void BubbleSort(unsigned char *v, int nItems);
unsigned char VMin(unsigned char *v, int nItems);
unsigned char VMax(unsigned char *v, int nItems);
int CalcFreqMasterScaleZero(unsigned char *freqMaster, int alterScale, int k0, int k2);
int CalcFreqMaster(unsigned char *freqMaster, int freqScale, int alterScale, int k0, int k2);
int CalcFreqHigh(unsigned char *freqHigh, unsigned char *freqMaster, int nMaster, int crossOverBand);
int CalcFreqLow(unsigned char *freqLow, unsigned char *freqHigh, int nHigh);
int CalcFreqNoise(unsigned char *freqNoise, unsigned char *freqLow, int nLow, int kStart, int k2, int noiseBands);
int BuildPatches(unsigned char *patchNumSubbands, unsigned char *patchStartSubband, unsigned char *freqMaster,
                        int nMaster, int k0, int kStart, int numQMFBands, int sampRateIdx);
int FindFreq(unsigned char *freq, int nFreq, unsigned char val);
void RemoveFreq(unsigned char *freq, int nFreq, int removeIdx);
int CalcFreqLimiter(unsigned char *freqLimiter, unsigned char *patchNumSubbands, unsigned char *freqLow,
                           int nLow, int kStart, int limiterBands, int numPatches);
int CalcFreqTables(SBRHeader *sbrHdr, SBRFreq *sbrFreq, int sampRateIdx);
void EstimateEnvelope(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, int env);
int GetSMapped(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int env, int band, int la);
void CalcMaxGain(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, int ch, int env, int lim, int fbitsDQ);
void CalcNoiseDivFactors(int q, int *qp1Inv, int *qqp1Inv);
void CalcComponentGains(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int ch, int env, int lim, int fbitsDQ);
void ApplyBoost(SBRFreq *sbrFreq, int lim, int fbitsDQ);
void CalcGain(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int ch, int env);
void MapHF(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int env, int hfReset);
void AdjustHighFreq(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int ch);
int CalcCovariance1(int *XBuf, int *p01reN, int *p01imN, int *p12reN, int *p12imN, int *p11reN, int *p22reN);
int CalcCovariance2(int *XBuf, int *p02reN, int *p02imN);
void CalcLPCoefs(int *XBuf, int *a0re, int *a0im, int *a1re, int *a1im, int gb);
void GenerateHighFreq(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int ch);
int DecodeHuffmanScalar(const signed /*short*/ int *huffTab, const HuffInfo_t *huffTabInfo, unsigned int bitBuf,
                        signed int *val);
int DecodeOneSymbol(int huffTabIndex);
int DequantizeEnvelope(int nBands, int ampRes, signed char *envQuant, int *envDequant);
void DequantizeNoise(int nBands, signed char *noiseQuant, int *noiseDequant);
void DecodeSBREnvelope(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int ch);
void DecodeSBRNoise(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int ch);
void UncoupleSBREnvelope(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChanR);
void UncoupleSBRNoise(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChanR);
void DecWindowOverlapNoClip(int *buf0, int *over0, int *out0, int winTypeCurr, int winTypePrev);
void DecWindowOverlapLongStartNoClip(int *buf0, int *over0, int *out0, int winTypeCurr, int winTypePrev);
void DecWindowOverlapLongStopNoClip(int *buf0, int *over0, int *out0, int winTypeCurr, int winTypePrev);
void DecWindowOverlapShortNoClip(int *buf0, int *over0, int *out0, int winTypeCurr, int winTypePrev);
void PreMultiply64(int *zbuf1);
void PostMultiply64(int *fft1, int nSampsOut);
void QMFAnalysisConv(int *cTab, int *delay, int dIdx, int *uBuf);
int QMFAnalysis(int *inbuf, int *delay, int *XBuf, int fBitsIn, int *delayIdx, int qmfaBands);
void QMFSynthesisConv(int *cPtr, int *delay, int dIdx, short *outbuf, int nChans);
void QMFSynthesis(int *inbuf, int *delay, int *delayIdx, int qmfsBands, short *outbuf, int nChans);
int UnpackSBRHeader(SBRHeader *sbrHdr);
void UnpackSBRGrid(SBRHeader *sbrHdr, SBRGrid *sbrGrid);
void UnpackDeltaTimeFreq(int numEnv, unsigned char *deltaFlagEnv,
                                int numNoiseFloors, unsigned char *deltaFlagNoise);
void UnpackInverseFilterMode(int numNoiseFloorBands, unsigned char *mode);
void UnpackSinusoids(int nHigh, int addHarmonicFlag, unsigned char *addHarmonic);
void CopyCouplingGrid(SBRGrid *sbrGridLeft, SBRGrid *sbrGridRight);
void CopyCouplingInverseFilterMode(int numNoiseFloorBands, unsigned char *modeLeft, unsigned char *modeRight);
void UnpackSBRSingleChannel(int chBase);
void UnpackSBRChannelPair(int chBase);











