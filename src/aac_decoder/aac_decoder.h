// based on helix aac decoder
#pragma once
//#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

#include "Arduino.h"

#define AAC_ENABLE_MPEG4

#if (defined CONFIG_IDF_TARGET_ESP32S3 && defined BOARD_HAS_PSRAM)
    #define AAC_ENABLE_SBR  // needs additional 60KB DRAM,
#endif

#define ASSERT(x) /* do nothing */

#ifndef MAX
#define MAX(a,b)    std::max(a,b)
#endif

#ifndef MIN
#define MIN(a,b)    std::min(a,b)
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
    void  *rawSampleBuf[2];
    int32_t   rawSampleBytes;
    int32_t   rawSampleFBits;
    /* fill data (can be used for processing SBR or other extensions) */
    uint8_t *fillBuf;
    int32_t   fillCount;
    int32_t   fillExtType;
    int32_t   prevBlockID;    /* block information */
    int32_t   currBlockID;
    int32_t   currInstTag;
    int32_t   sbDeinterleaveReqd[2]; // [MAX_NCHANS_ELEM]
    int32_t   adtsBlocksLeft;
    int32_t   bitRate;    /* user-accessible info */
    int32_t   nChans;
    int32_t   sampRate;
    float compressionRatio;
    int32_t   id;         /* 0: MPEG-4, 1: MPEG2 */
    int32_t   profile;    /* 0: Main profile, 1: LowComplexity (LC), 2: ScalableSamplingRate (SSR), 3: reserved */
    int32_t   format;
    int32_t   sbrEnabled;
    int32_t   tnsUsed;
    int32_t   pnsUsed;
    int32_t   frameCount;
} AACDecInfo_t;


typedef struct _aac_BitStreamInfo_t {
    uint8_t *bytePtr;
    uint32_t iCache;
    int32_t cachedBits;
    int32_t nBytes;
} aac_BitStreamInfo_t;

typedef union _U64 {
    int64_t w64;
    struct {
        uint32_t lo32;
        int32_t  hi32;
    } r;
} U64;

typedef struct _AACFrameInfo_t {
    int32_t bitRate;
    int32_t nChans;
    int32_t sampRateCore;
    int32_t sampRateOut;
    int32_t bitsPerSample;
    int32_t outputSamps;
    int32_t profile;
    int32_t tnsUsed;
    int32_t pnsUsed;
} AACFrameInfo_t;

typedef struct _HuffInfo_t {
    int32_t maxBits;              /* number of bits in longest codeword */
    uint8_t count[20];        /*  count[MAX_HUFF_BITS] = number of codes with length i+1 bits */
    int32_t offset;               /* offset into symbol table */
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
    int8_t   coef[60];        // [MAX_TNS_COEFS] max 3 filters * 20 coefs for 1 long window,
                              //  or 1 filter * 7 coefs for each of 8 short windows
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
    uint8_t id;                         /* MPEG bit - should be 1 */
    uint8_t layer;                      /* MPEG layer - should be 0 */
    uint8_t protectBit;                 /* 0 = CRC word follows, 1 = no CRC word */
    uint8_t profile;                    /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    uint8_t sampRateIdx;                /* sample rate index range = [0, 11] */
    uint8_t privateBit;                 /* ignore */
    uint8_t channelConfig;              /* 0 = implicit, >0 = use default table */
    uint8_t origCopy;                   /* 0 = copy, 1 = original */
    uint8_t home;                       /* ignore */
    /* variable */
    uint8_t copyBit;                    /* 1 bit of the 72-bit copyright ID (transmitted as 1 bit per frame) */
    uint8_t copyStart;                  /* 1 = this bit starts the 72-bit ID, 0 = it does not */
    int32_t     frameLength;                /* length of frame */
    int32_t     bufferFull;                 /* number of 32-bit words left in enc buffer, 0x7FF = VBR */
    uint8_t numRawDataBlocks;           /* number of raw data blocks in frame */
    /* CRC */
    int32_t     crcCheckWord;                     /* 16-bit CRC check word (present if protectBit == 0) */
} ADTSHeader_t;

typedef struct _ADIFHeader_t {
    uint8_t  copyBit;                    /* 0 = no copyright ID, 1 = 72-bit copyright ID follows immediately */
    uint8_t  origCopy;                   /* 0 = copy, 1 = original */
    uint8_t  home;                       /* ignore */
    uint8_t  bsType;                     /* bitstream type: 0 = CBR, 1 = VBR */
    int32_t      bitRate;                    /* bitRate: CBR = bits/sec, VBR = peak bits/frame, 0 = unknown */
    uint8_t  numPCE;                     /* number of program config elements (max = 16) */
    int32_t      bufferFull;                 /* bits left in bit reservoir */
    uint8_t  copyID[9];                  /* [ADIF_COPYID_SIZE] optional 72-bit copyright ID */
} ADIFHeader_t;

/* sizeof(ProgConfigElement_t) = 82 bytes (if KEEP_PCE_COMMENTS not defined) */
typedef struct _ProgConfigElement_t {
    uint8_t  elemInstTag;   /* element instance tag */
    uint8_t  profile;       /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    uint8_t  sampRateIdx;   /* sample rate index range = [0, 11] */
    uint8_t  numFCE;        /* number of front channel elements (max = 15) */
    uint8_t  numSCE;        /* number of side channel elements (max = 15) */
    uint8_t  numBCE;        /* number of back channel elements (max = 15) */
    uint8_t  numLCE;        /* number of LFE channel elements (max = 3) */
    uint8_t  numADE;        /* number of associated data elements (max = 7) */
    uint8_t  numCCE;        /* number of valid channel coupling elements (max = 15) */
    uint8_t  monoMixdown;   /* mono mixdown: bit 4 = present flag, bits 3-0 = element number */
    uint8_t  stereoMixdown; /* stereo mixdown: bit 4 = present flag, bits 3-0 = element number */
    uint8_t  matrixMixdown; /* bit 4 = present flag, bit 3 = unused,bits 2-1 = index, bit 0 = pseudo-surround enable */
    uint8_t  fce[15];       /* [MAX_NUM_FCE] front element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    uint8_t  sce[15];       /* [MAX_NUM_SCE] side element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    uint8_t  bce[15];       /* [MAX_NUM_BCE] back element channel pair: bit 4 = SCE/CPE flag, bits 3-0 = inst tag */
    uint8_t  lce[3];        /* [MAX_NUM_LCE] instance tag for LFE elements */
    uint8_t  ade[7];        /* [MAX_NUM_ADE] instance tag for ADE elements */
    uint8_t  cce[15];       /* [MAX_NUM_BCE] channel coupling elements: bit 4 = switching flag, bits 3-0 = inst tag */
} ProgConfigElement_t;

typedef struct _SBRHeader {
    int32_t      count;

    uint8_t  ampRes;
    uint8_t  startFreq;
    uint8_t  stopFreq;
    uint8_t  crossOverBand;
    uint8_t  resBitsHdr;
    uint8_t  hdrExtra1;
    uint8_t  hdrExtra2;

    uint8_t  freqScale;
    uint8_t  alterScale;
    uint8_t  noiseBands;

    uint8_t  limiterBands;
    uint8_t  limiterGains;
    uint8_t  interpFreq;
    uint8_t  smoothMode;
} SBRHeader;

/* need one SBRGrid per channel, updated every frame */
typedef struct _SBRGrid {
    uint8_t  frameClass;
    uint8_t  ampResFrame;
    uint8_t  pointer;

    uint8_t  numEnv;                       /* L_E */
    uint8_t  envTimeBorder[5 + 1];   // [MAX_NUM_ENV+1] /* t_E */
    uint8_t  freqRes[5];             // [MAX_NUM_ENV]/* r */
    uint8_t  numNoiseFloors;                           /* L_Q */
    uint8_t  noiseTimeBorder[2 + 1]; // [MAX_NUM_NOISE_FLOORS+1] /* t_Q */

    uint8_t  numEnvPrev;
    uint8_t  numNoiseFloorsPrev;
    uint8_t  freqResPrev;
} SBRGrid;

/* need one SBRFreq per element (SCE/CPE/LFE), updated only on header reset */
typedef struct _SBRFreq {
    int32_t      kStart;               /* k_x */
    int32_t      nMaster;
    int32_t      nHigh;
    int32_t      nLow;
    int32_t      nLimiter;             /* N_l */
    int32_t      numQMFBands;          /* M */
    int32_t      numNoiseFloorBands;   /* Nq */
    int32_t      kStartPrev;
    int32_t      numQMFBandsPrev;
    uint8_t  freqMaster[48 + 1];     // [MAX_QMF_BANDS + 1]      /* not necessary to save this  after derived tables are generated */
    uint8_t  freqHigh[48 + 1];       // [MAX_QMF_BANDS + 1]
    uint8_t  freqLow[48 / 2 + 1];    // [MAX_QMF_BANDS / 2 + 1]  /* nLow = nHigh - (nHigh >> 1) */
    uint8_t  freqNoise[5 + 1];       // [MAX_NUM_NOISE_FLOOR_BANDS+1]
    uint8_t  freqLimiter[48 / 2 + 5];// [MAX_QMF_BANDS / 2 + MAX_NUM_PATCHES]    /* max (intermediate) size = nLow + numPatches - 1 */

    uint8_t  numPatches;
    uint8_t  patchNumSubbands[5 + 1];  // [MAX_NUM_PATCHES + 1]
    uint8_t  patchStartSubband[5 + 1]; // [MAX_NUM_PATCHES + 1]
} SBRFreq;

typedef struct _SBRChan {
    int32_t      reset;
    uint8_t  deltaFlagEnv[5];          // [MAX_NUM_ENV]
    uint8_t  deltaFlagNoise[2];        // [MAX_NUM_NOISE_FLOORS]
    int8_t   envDataQuant[5][48];      // [MAX_NUM_ENV][MAX_QMF_BANDS] /* range = [0, 127] */
    int8_t   noiseDataQuant[2][5];     // [MAX_NUM_NOISE_FLOORS][MAX_NUM_NOISE_FLOOR_BANDS]

    uint8_t  invfMode[2][5];           // [2][MAX_NUM_NOISE_FLOOR_BANDS] /* invfMode[0/1][band] = prev/curr */
    int32_t      chirpFact[5];             // [MAX_NUM_NOISE_FLOOR_BANDS]  /* bwArray */
    uint8_t  addHarmonicFlag[2];       /* addHarmonicFlag[0/1] = prev/curr */
    uint8_t  addHarmonic[2][64];       /* addHarmonic[0/1][band] = prev/curr */

    int32_t      gbMask[2];    /* gbMask[0/1] = XBuf[0-31]/XBuf[32-39] */
    int8_t   laPrev;

    int32_t      noiseTabIndex;
    int32_t      sinIndex;
    int32_t      gainNoiseIndex;
    int32_t      gTemp[5][48];  // [MAX_NUM_SMOOTH_COEFS][MAX_QMF_BANDS]
    int32_t      qTemp[5][48];  // [MAX_NUM_SMOOTH_COEFS][MAX_QMF_BANDS]

} SBRChan;


/* state info struct for baseline (MPEG-4 LC) decoding */
typedef struct _PSInfoBase_t {
    int32_t      dataCount;
    uint8_t  dataBuf[510]; // [DATA_BUF_SIZE]
    int32_t      fillCount;
    uint8_t  fillBuf[269]; //[FILL_BUF_SIZE]
    /* state information which is the same throughout whole frame */
    int32_t      nChans;
    int32_t      useImpChanMap;
    int32_t      sampRateIdx;
    /* state information which can be overwritten by subsequent elements within frame */
    ICSInfo_t  icsInfo[2]; // [MAX_NCHANS_ELEM]
    int32_t      commonWin;
    int16_t    scaleFactors[2][15*8]; // [MAX_NCHANS_ELEM][MAX_SF_BANDS]
    uint8_t  sfbCodeBook[2][15*8]; // [MAX_NCHANS_ELEM][MAX_SF_BANDS]
    int32_t      msMaskPresent;
    uint8_t  msMaskBits[(15 * 8 + 7) >> 3]; // [MAX_MS_MASK_BYTES]
    int32_t      pnsUsed[2]; // [MAX_NCHANS_ELEM]
    int32_t      pnsLastVal;
    int32_t      intensityUsed[2]; // [MAX_NCHANS_ELEM]
//    PulseInfo_t           pulseInfo[2]; // [MAX_NCHANS_ELEM]
    TNSInfo_t   tnsInfo[2]; // [MAX_NCHANS_ELEM]
    int32_t      tnsLPCBuf[20]; // [MAX_TNS_ORDER]
    int32_t      tnsWorkBuf[20]; //[MAX_TNS_ORDER]
    GainControlInfo_t     gainControlInfo[2]; // [MAX_NCHANS_ELEM]
    int32_t      gbCurrent[2];  // [MAX_NCHANS_ELEM]
    int32_t      coef[2][1024]; // [MAX_NCHANS_ELEM][AAC_MAX_NSAMPS]
#ifdef AAC_ENABLE_SBR
    int32_t      sbrWorkBuf[2][1024]; // [MAX_NCHANS_ELEM][AAC_MAX_NSAMPS];
#endif
    /* state information which must be saved for each element and used in next frame */
    int32_t      overlap[2][1024];  // [AAC_MAX_NCHANS][AAC_MAX_NSAMPS]
    int32_t      prevWinShape[2]; // [AAC_MAX_NCHANS]
} PSInfoBase_t;

typedef struct _PSInfoSBR {
    /* save for entire file */
    int32_t      frameCount;
    int32_t      sampRateIdx;

    /* state info that must be saved for each channel */
    SBRHeader   sbrHdr[2];
    SBRGrid     sbrGrid[2];
    SBRFreq     sbrFreq[2];
    SBRChan     sbrChan[2];

    /* temp variables, no need to save between blocks */
    uint8_t  dataExtra;
    uint8_t  resBitsData;
    uint8_t  extendedDataPresent;
    int32_t      extendedDataSize;

    int8_t   envDataDequantScale[2][5];  // [MAX_NCHANS_ELEM][MAX_NUM_ENV
    int32_t      envDataDequant[2][5][48];   // [MAX_NCHANS_ELEM][MAX_NUM_ENV][MAX_QMF_BANDS
    int32_t      noiseDataDequant[2][2][5];  // [MAX_NCHANS_ELEM][MAX_NUM_NOISE_FLOORS][MAX_NUM_NOISE_FLOOR_BANDS]

    int32_t      eCurr[48];    // [MAX_QMF_BANDS]
    uint8_t  eCurrExp[48]; // [MAX_QMF_BANDS]
    uint8_t  eCurrExpMax;
    int8_t   la;

    int32_t      crcCheckWord;
    int32_t      couplingFlag;
    int32_t      envBand;
    int32_t      eOMGainMax;
    int32_t      gainMax;
    int32_t      gainMaxFBits;
    int32_t      noiseFloorBand;
    int32_t      qp1Inv;
    int32_t      qqp1Inv;
    int32_t      sMapped;
    int32_t      sBand;
    int32_t      highBand;

    int32_t      sumEOrigMapped;
    int32_t      sumECurrGLim;
    int32_t      sumSM;
    int32_t      sumQM;
    int32_t      gLimBoost[48];
    int32_t      qmLimBoost[48];
    int32_t      smBoost[48];

    int32_t      smBuf[48];
    int32_t      qmLimBuf[48];
    int32_t      gLimBuf[48];
    int32_t      gLimFbits[48];

    int32_t      gFiltLast[48];
    int32_t      qFiltLast[48];

    /* large buffers */
    int32_t      delayIdxQMFA[2];        // [AAC_MAX_NCHANS]
    int32_t      delayQMFA[2][10 * 32];  // [AAC_MAX_NCHANS][DELAY_SAMPS_QMFA]
    int32_t      delayIdxQMFS[2];        // [AAC_MAX_NCHANS]
    int32_t      delayQMFS[2][10 * 128]; // [AAC_MAX_NCHANS][DELAY_SAMPS_QMFS]
    int32_t      XBufDelay[2][8][64][2]; // [AAC_MAX_NCHANS][HF_GEN][64][2]
    int32_t      XBuf[32+8][64][2];
} PSInfoSBR_t;

bool AACDecoder_AllocateBuffers(void);
int32_t AACFlushCodec();
void AACDecoder_FreeBuffers(void);
bool AACDecoder_IsInit(void);
int32_t AACFindSyncWord(uint8_t *buf, int32_t nBytes);
int32_t AACSetRawBlockParams(int32_t copyLast, int32_t nChans, int32_t sampRateCore, int32_t profile);
int32_t AACDecode(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf);
int32_t AACGetSampRate();
int32_t AACGetChannels();
int32_t AACGetID(); // 0-MPEG4, 1-MPEG2
uint8_t AACGetProfile(); // 0-Main, 1-LC, 2-SSR, 3-reserved
uint8_t AACGetFormat(); // 0-unknown 1-ADTS 2-ADIF, 3-RAW
int32_t AACGetBitsPerSample();
int32_t AACGetBitrate();
int32_t AACGetOutputSamps();
int32_t AACGetBitrate();
void DecodeLPCCoefs(int32_t order, int32_t res, int8_t *filtCoef, int32_t *a, int32_t *b);
int32_t FilterRegion(int32_t size, int32_t dir, int32_t order, int32_t *audioCoef, int32_t *a, int32_t *hist);
int32_t TNSFilter(int32_t ch);
int32_t DecodeSingleChannelElement();
int32_t DecodeChannelPairElement();
int32_t DecodeLFEChannelElement();
int32_t DecodeDataStreamElement();
int32_t DecodeProgramConfigElement(uint8_t idx);
int32_t DecodeFillElement();
int32_t DecodeNextElement(uint8_t **buf, int32_t *bitOffset, int32_t *bitsAvail);
void PreMultiply(int32_t tabidx, int32_t *zbuf1);
void PostMultiply(int32_t tabidx, int32_t *fft1);
void PreMultiplyRescale(int32_t tabidx, int32_t *zbuf1, int32_t es);
void PostMultiplyRescale(int32_t tabidx, int32_t *fft1, int32_t es);
void DCT4(int32_t tabidx, int32_t *coef, int32_t gb);
void BitReverse(int32_t *inout, int32_t tabidx);
void R4FirstPass(int32_t *x, int32_t bg);
void R8FirstPass(int32_t *x, int32_t bg);
void R4Core(int32_t *x, int32_t bg, int32_t gp, int32_t *wtab);
void R4FFT(int32_t tabidx, int32_t *x);
void UnpackZeros(int32_t nVals, int32_t *coef);
void UnpackQuads(int32_t cb, int32_t nVals, int32_t *coef);
void UnpackPairsNoEsc(int32_t cb, int32_t nVals, int32_t *coef);
void UnpackPairsEsc(int32_t cb, int32_t nVals, int32_t *coef);
void DecodeSpectrumLong(int32_t ch);
void DecodeSpectrumShort(int32_t ch);
void DecWindowOverlap(int32_t *buf0, int32_t *over0, int16_t *pcm0, int32_t nChans, int32_t winTypeCurr, int32_t winTypePrev);
void DecWindowOverlapLongStart(int32_t *buf0, int32_t *over0, int16_t *pcm0, int32_t nChans, int32_t winTypeCurr, int32_t winTypePrev);
void DecWindowOverlapLongStop(int32_t *buf0, int32_t *over0, int16_t *pcm0, int32_t nChans, int32_t winTypeCurr, int32_t winTypePrev);
void DecWindowOverlapShort(int32_t *buf0, int32_t *over0, int16_t *pcm0, int32_t nChans, int32_t winTypeCurr, int32_t winTypePrev);
int32_t IMDCT(int32_t ch, int32_t chOut, int16_t *outbuf);
void DecodeICSInfo(ICSInfo_t *icsInfo, int32_t sampRateIdx);
void DecodeSectionData(int32_t winSequence, int32_t numWinGrp, int32_t maxSFB, uint8_t *sfbCodeBook);
int32_t DecodeOneScaleFactor();
void DecodeScaleFactors(int32_t numWinGrp, int32_t maxSFB, int32_t globalGain, uint8_t *sfbCodeBook, int16_t *scaleFactors);
void DecodePulseInfo(uint8_t ch);
void DecodeTNSInfo(int32_t winSequence, TNSInfo_t *ti, int8_t *tnsCoef);
void DecodeGainControlInfo(int32_t winSequence, GainControlInfo_t *gi);
void DecodeICS(int32_t ch);
int32_t DecodeNoiselessData(uint8_t **buf, int32_t *bitOffset, int32_t *bitsAvail, int32_t ch);
int32_t UnpackADTSHeader(uint8_t **buf, int32_t *bitOffset, int32_t *bitsAvail);
int32_t GetADTSChannelMapping(uint8_t *buf, int32_t bitOffset, int32_t bitsAvail);
int32_t GetNumChannelsADIF(int32_t nPCE);
int32_t GetSampleRateIdxADIF(int32_t nPCE);
int32_t UnpackADIFHeader(uint8_t **buf, int32_t *bitOffset, int32_t *bitsAvail);
int32_t SetRawBlockParams(int32_t copyLast, int32_t nChans, int32_t sampRate, int32_t profile);
int32_t PrepareRawBlock();
int32_t DequantBlock(int32_t *inbuf, int32_t nSamps, int32_t scale);
int32_t AACDequantize(int32_t ch);
int32_t DeinterleaveShortBlocks(int32_t ch);
uint32_t Get32BitVal(uint32_t *last);
int32_t InvRootR(int32_t r);
int32_t ScaleNoiseVector(int32_t *coef, int32_t nVals, int32_t sf);
void GenerateNoiseVector(int32_t *coef, int32_t *last, int32_t nVals);
void CopyNoiseVector(int32_t *coefL, int32_t *coefR, int32_t nVals);
int32_t PNS(int32_t ch);
int32_t GetSampRateIdx(int32_t sampRate);
void StereoProcessGroup(int32_t *coefL, int32_t *coefR, const uint16_t *sfbTab, int32_t msMaskPres, uint8_t *msMaskPtr,
        int32_t msMaskOffset, int32_t maxSFB, uint8_t *cbRight, int16_t *sfRight, int32_t *gbCurrent);
int32_t StereoProcess();
int32_t RatioPowInv(int32_t a, int32_t b, int32_t c);
int32_t SqrtFix(int32_t q, int32_t fBitsIn, int32_t *fBitsOut);
int32_t InvRNormalized(int32_t r);
void BitReverse32(int32_t *inout);
void R8FirstPass32(int32_t *r0);
void R4Core32(int32_t *r0);
void FFT32C(int32_t *x);
void CVKernel1(int32_t *XBuf, int32_t *accBuf);
void CVKernel2(int32_t *XBuf, int32_t *accBuf);
void SetBitstreamPointer(int32_t nBytes, uint8_t *buf);
inline void RefillBitstreamCache();
uint32_t GetBits(int32_t nBits);
uint32_t GetBitsNoAdvance(int32_t nBits);
void AdvanceBitstream(int32_t nBits);
int32_t CalcBitsUsed(uint8_t *startBuf, int32_t startOffset);
void ByteAlignBitstream();
// SBR
void InitSBRState();
int32_t DecodeSBRBitstream(int32_t chBase);
int32_t DecodeSBRData(int32_t chBase, int16_t *outbuf);
int32_t FlushCodecSBR();
void BubbleSort(uint8_t *v, int32_t nItems);
uint8_t VMin(uint8_t *v, int32_t nItems);
uint8_t VMax(uint8_t *v, int32_t nItems);
int32_t CalcFreqMasterScaleZero(uint8_t *freqMaster, int32_t alterScale, int32_t k0, int32_t k2);
int32_t CalcFreqMaster(uint8_t *freqMaster, int32_t freqScale, int32_t alterScale, int32_t k0, int32_t k2);
int32_t CalcFreqHigh(uint8_t *freqHigh, uint8_t *freqMaster, int32_t nMaster, int32_t crossOverBand);
int32_t CalcFreqLow(uint8_t *freqLow, uint8_t *freqHigh, int32_t nHigh);
int32_t CalcFreqNoise(uint8_t *freqNoise, uint8_t *freqLow, int32_t nLow, int32_t kStart, int32_t k2, int32_t noiseBands);
int32_t BuildPatches(uint8_t *patchNumSubbands, uint8_t *patchStartSubband, uint8_t *freqMaster, int32_t nMaster, int32_t k0,
        int32_t kStart, int32_t numQMFBands, int32_t sampRateIdx);
int32_t FindFreq(uint8_t *freq, int32_t nFreq, uint8_t val);
void RemoveFreq(uint8_t *freq, int32_t nFreq, int32_t removeIdx);
int32_t CalcFreqLimiter(uint8_t *freqLimiter, uint8_t *patchNumSubbands, uint8_t *freqLow, int32_t nLow, int32_t kStart,
        int32_t limiterBands, int32_t numPatches);
int32_t CalcFreqTables(SBRHeader *sbrHdr, SBRFreq *sbrFreq, int32_t sampRateIdx);
void EstimateEnvelope(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, int32_t env);
int32_t GetSMapped(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t env, int32_t band, int32_t la);
void CalcMaxGain(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, int32_t ch, int32_t env, int32_t lim, int32_t fbitsDQ);
void CalcNoiseDivFactors(int32_t q, int32_t *qp1Inv, int32_t *qqp1Inv);
void CalcComponentGains(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t ch, int32_t env, int32_t lim, int32_t fbitsDQ);
void ApplyBoost(SBRFreq *sbrFreq, int32_t lim, int32_t fbitsDQ);
void CalcGain(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t ch, int32_t env);
void MapHF(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t env, int32_t hfReset);
void AdjustHighFreq(SBRHeader *sbrHdr, SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t ch);
int32_t CalcCovariance1(int32_t *XBuf, int32_t *p01reN, int32_t *p01imN, int32_t *p12reN, int32_t *p12imN, int32_t *p11reN, int32_t *p22reN);
int32_t CalcCovariance2(int32_t *XBuf, int32_t *p02reN, int32_t *p02imN);
void CalcLPCoefs(int32_t *XBuf, int32_t *a0re, int32_t *a0im, int32_t *a1re, int32_t *a1im, int32_t gb);
void GenerateHighFreq(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t ch);
int32_t DecodeHuffmanScalar(const int16_t *huffTab, const HuffInfo_t *huffTabInfo, uint32_t bitBuf, int32_t *val);
int32_t DecodeOneSymbol(int32_t huffTabIndex);
int32_t DequantizeEnvelope(int32_t nBands, int32_t ampRes, int8_t *envQuant, int32_t *envDequant);
void DequantizeNoise(int32_t nBands, int8_t *noiseQuant, int32_t *noiseDequant);
void DecodeSBREnvelope(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t ch);
void DecodeSBRNoise(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChan, int32_t ch);
void UncoupleSBREnvelope(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChanR);
void UncoupleSBRNoise(SBRGrid *sbrGrid, SBRFreq *sbrFreq, SBRChan *sbrChanR);
void DecWindowOverlapNoClip(int32_t *buf0, int32_t *over0, int32_t *out0, int32_t winTypeCurr, int32_t winTypePrev);
void DecWindowOverlapLongStartNoClip(int32_t *buf0, int32_t *over0, int32_t *out0, int32_t winTypeCurr, int32_t winTypePrev);
void DecWindowOverlapLongStopNoClip(int32_t *buf0, int32_t *over0, int32_t *out0, int32_t winTypeCurr, int32_t winTypePrev);
void DecWindowOverlapShortNoClip(int32_t *buf0, int32_t *over0, int32_t *out0, int32_t winTypeCurr, int32_t winTypePrev);
void PreMultiply64(int32_t *zbuf1);
void PostMultiply64(int32_t *fft1, int32_t nSampsOut);
void QMFAnalysisConv(int32_t *cTab, int32_t *delay, int32_t dIdx, int32_t *uBuf);
int32_t QMFAnalysis(int32_t *inbuf, int32_t *delay, int32_t *XBuf, int32_t fBitsIn, int32_t *delayIdx, int32_t qmfaBands);
void QMFSynthesisConv(int32_t *cPtr, int32_t *delay, int32_t dIdx, int16_t *outbuf, int32_t nChans);
void QMFSynthesis(int32_t *inbuf, int32_t *delay, int32_t *delayIdx, int32_t qmfsBands, int16_t *outbuf, int32_t nChans);
int32_t UnpackSBRHeader(SBRHeader *sbrHdr);
void UnpackSBRGrid(SBRHeader *sbrHdr, SBRGrid *sbrGrid);
void UnpackDeltaTimeFreq(int32_t numEnv, uint8_t *deltaFlagEnv, int32_t numNoiseFloors, uint8_t *deltaFlagNoise);
void UnpackInverseFilterMode(int32_t numNoiseFloorBands, uint8_t *mode);
void UnpackSinusoids(int32_t nHigh, int32_t addHarmonicFlag, uint8_t *addHarmonic);
void CopyCouplingGrid(SBRGrid *sbrGridLeft, SBRGrid *sbrGridRight);
void CopyCouplingInverseFilterMode(int32_t numNoiseFloorBands, uint8_t *modeLeft, uint8_t *modeRight);
void UnpackSBRSingleChannel(int32_t chBase);
void UnpackSBRChannelPair(int32_t chBase);
