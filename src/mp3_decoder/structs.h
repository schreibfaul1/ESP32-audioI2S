#pragma once

#include "stdint-gcc.h"

#define SYNCWORDH            0xFF
#define SYNCWORDL            0xE0
#define DQ_FRACBITS_OUT      25 // number of fraction bits in output of dequant
#define CSHIFT               12 // coefficients have 12 leading sign bits for early-terminating mulitplies
#define SIBYTES_MPEG1_MONO   17
#define SIBYTES_MPEG1_STEREO 32
#define SIBYTES_MPEG2_MONO   9
#define SIBYTES_MPEG2_STEREO 17
#define IMDCT_SCALE          2 // additional scaling (by sqrt(2)) for fast IMDCT36
#define NGRANS_MPEG1         2
#define NGRANS_MPEG2         1
#define SQRTHALF             0x5a82799a // sqrt(0.5) in Q31 format
#define MAX_NGRAN            2          /* max granules */
#define MAX_NCHAN            2          /* max channels */
#define MAX_NSAMP            576        /* max samples per channel, per granule */
#define MAX_SCFBD            4          /* max scalefactor bands per channel */
#define NGRANS_MPEG1         2
#define NGRANS_MPEG2         1
#define HUFF_PAIRTABS        32
#define BLOCK_SIZE           18
#define NBANDS               32
#define MAX_REORDER_SAMPS    (192 - 126) * 3 // largest critical band for short blocks (see sfBandTable)
#define VBUF_LENGTH          17 * 2 * NBANDS // for double-sized vbuf FIFO
#define MAX_SCFBD            4               // max scalefactor bands per channel
#define MAINBUF_SIZE         1940
#define MAX_NGRAN            2                // max granules
#define MAX_NCHAN            2                // max channels
#define MAX_NSAMP            576              // max samples per channel, per granule
#define CLZ(x)               __builtin_clz(x) // fb

#define CLIP_2N(y, n)       \
    {                       \
        int32_t x = 1 << n; \
        if (y < -x) y = -x; \
        x--;                \
        if (y > x) y = x;   \
    }

#define D32FP(i, s1, s2)                                    \
    {                                                       \
        a0 = buf[i];                                        \
        a3 = buf[31 - i];                                   \
        a1 = buf[15 - i];                                   \
        a2 = buf[16 + i];                                   \
        b0 = a0 + a3;                                       \
        b3 = MULSHIFT32(*cptr++, a0 - a3) << 1;             \
        b1 = a1 + a2;                                       \
        b2 = MULSHIFT32(*cptr++, a1 - a2) << (s1);          \
        buf[i] = b0 + b1;                                   \
        buf[15 - i] = MULSHIFT32(*cptr, b0 - b1) << (s2);   \
        buf[16 + i] = b2 + b3;                              \
        buf[31 - i] = MULSHIFT32(*cptr++, b3 - b2) << (s2); \
    }


typedef struct MP3FrameInfo {
    int32_t bitrate;
    int32_t nChans;
    int32_t samprate;
    int32_t bitsPerSample;
    int32_t outputSamps;
    int32_t layer;
    int32_t version;
} MP3FrameInfo_t;

typedef struct SFBandTable {
    int32_t l[23];
    int32_t s[14];
} SFBandTable_t;

typedef struct BitStreamInfo {
    uint8_t* bytePtr;
    uint32_t iCache;
    int32_t  cachedBits;
    int32_t  nBytes;
} BitStreamInfo_t;

typedef enum {                /* map these to the corresponding 2-bit values in the frame header */
               Stereo = 0x00, /* two independent channels, but L and R frames might have different # of bits */
               Joint = 0x01,  /* coupled channels - layer III: mix of M-S and intensity, Layers I/II: intensity and direct coding only */
               Dual = 0x02,   /* two independent channels, L and R always have exactly 1/2 the total bitrate */
               Mono = 0x03    /* one channel */
} StereoMode_t;

typedef enum { /* map to 0,1,2 to make table indexing easier */
               MPEG1 = 0,
               MPEG2 = 1,
               MPEG25 = 2
} MPEGVersion_t;

typedef struct FrameHeader {
    int32_t layer;      /* layer index (1, 2, or 3) */
    int32_t crc;        /* CRC flag: 0 = disabled, 1 = enabled */
    int32_t brIdx;      /* bitrate index (0 - 15) */
    int32_t srIdx;      /* sample rate index (0 - 2) */
    int32_t paddingBit; /* padding flag: 0 = no padding, 1 = single pad byte */
    int32_t privateBit; /* unused */
    int32_t modeExt;    /* used to decipher joint stereo mode */
    int32_t copyFlag;   /* copyright flag: 0 = no, 1 = yes */
    int32_t origFlag;   /* original flag: 0 = copy, 1 = original */
    int32_t emphasis;   /* deemphasis mode */
    int32_t CRCWord;    /* CRC word (16 bits, 0 if crc not enabled) */
} FrameHeader_t;

typedef struct SideInfoSub {
    int32_t part23Length;      /* number of bits in main data */
    int32_t nBigvals;          /* 2x this = first set of Huffman cw's (maximum amplitude can be > 1) */
    int32_t globalGain;        /* overall gain for dequantizer */
    int32_t sfCompress;        /* unpacked to figure out number of bits in scale factors */
    int32_t winSwitchFlag;     /* window switching flag */
    int32_t blockType;         /* block type */
    int32_t mixedBlock;        /* 0 = regular block (all short or long), 1 = mixed block */
    int32_t tableSelect[3];    /* index of Huffman tables for the big values regions */
    int32_t subBlockGain[3];   /* subblock gain offset, relative to global gain */
    int32_t region0Count;      /* 1+region0Count = num scale factor bands in first region of bigvals */
    int32_t region1Count;      /* 1+region1Count = num scale factor bands in second region of bigvals */
    int32_t preFlag;           /* for optional high frequency boost */
    int32_t sfactScale;        /* scaling of the scalefactors */
    int32_t count1TableSelect; /* index of Huffman table for quad codewords */
} SideInfoSub_t;

typedef struct SideInfo {
    int32_t mainDataBegin;
    int32_t privateBits;
    int32_t scfsi[MAX_NCHAN][MAX_SCFBD]; /* 4 scalefactor bands per channel */
} SideInfo_t;

typedef struct {
    int32_t cbType;    /* pure long = 0, pure short = 1, mixed = 2 */
    int32_t cbEndS[3]; /* number nonzero short cb's, per subbblock */
    int32_t cbEndSMax; /* max of cbEndS[] */
    int32_t cbEndL;    /* number nonzero long cb's  */
} CriticalBandInfo_t;

typedef struct DequantInfo {
    int32_t workBuf[MAX_REORDER_SAMPS]; /* workbuf for reordering short blocks */
} DequantInfo_t;

typedef struct HuffmanInfo {
    int32_t huffDecBuf[MAX_NCHAN][MAX_NSAMP]; /* used both for decoded Huffman values and dequantized coefficients */
    int32_t nonZeroBound[MAX_NCHAN];          /* number of coeffs in huffDecBuf[ch] which can be > 0 */
    int32_t gb[MAX_NCHAN];                    /* minimum number of guard bits in huffDecBuf[ch] */
} HuffmanInfo_t;

typedef enum HuffTabType { noBits, oneShot, loopNoLinbits, loopLinbits, quadA, quadB, invalidTab } HuffTabType_t;

typedef struct HuffTabLookup {
    int32_t linBits;
    int32_t tabType; /*HuffTabType*/
} HuffTabLookup_t;

typedef struct IMDCTInfo {
    int32_t outBuf[MAX_NCHAN][BLOCK_SIZE][NBANDS]; /* output of IMDCT */
    int32_t overBuf[MAX_NCHAN][MAX_NSAMP / 2];     /* overlap-add buffer (by symmetry, only need 1/2 size) */
    int32_t numPrevIMDCT[MAX_NCHAN];               /* how many IMDCT's calculated in this channel on prev. granule */
    int32_t prevType[MAX_NCHAN];
    int32_t prevWinSwitch[MAX_NCHAN];
    int32_t gb[MAX_NCHAN];
} IMDCTInfo_t;

typedef struct BlockCount {
    int32_t nBlocksLong;
    int32_t nBlocksTotal;
    int32_t nBlocksPrev;
    int32_t prevType;
    int32_t prevWinSwitch;
    int32_t currWinSwitch;
    int32_t gbIn;
    int32_t gbOut;
} BlockCount_t;

typedef struct ScaleFactorInfoSub { /* max bits in scalefactors = 5, so use char's to save space */
    char l[23];                     /* [band] */
    char s[13][3];                  /* [band][window] */
} ScaleFactorInfoSub_t;

typedef struct ScaleFactorJS { /* used in MPEG 2, 2.5 intensity (joint) stereo only */
    int32_t intensityScale;
    int32_t slen[4];
    int32_t nr[4];
} ScaleFactorJS_t;

/* NOTE - could get by with smaller vbuf if memory is more important than speed
 *  (in Subband, instead of replicating each block in FDCT32 you would do a memmove on the
 *   last 15 blocks to shift them down one, a hardware style FIFO)
 */
typedef struct SubbandInfo {
    int32_t vbuf[MAX_NCHAN * VBUF_LENGTH]; /* vbuf for fast DCT-based synthesis PQMF - double size for speed (no modulo indexing) */
    int32_t vindex;                        /* internal index for tracking position in vbuf */
} SubbandInfo_t;

typedef struct MP3DecInfo {
    /* buffer which must be large enough to hold largest possible main_data section */
    uint8_t mainBuf[MAINBUF_SIZE];
    /* special info for "free" bitrate files */
    int32_t freeBitrateFlag;
    int32_t freeBitrateSlots;
    /* user-accessible info */
    int32_t bitrate;
    int32_t nChans;
    int32_t samprate;
    int32_t nGrans;     /* granules per frame */
    int32_t nGranSamps; /* samples per granule */
    int32_t nSlots;
    int32_t layer;

    int32_t mainDataBegin;
    int32_t mainDataBytes;
    int32_t part23Length[MAX_NGRAN][MAX_NCHAN];
} MP3DecInfo_t;

typedef struct {
    uint8_t  mpeg_version; // 0=MPEG2.5, 1=reserved, 2=MPEG2, 3=MPEG1
    uint8_t  layer;        // 0=reserved, 1=Layer III, 2=Layer II, 3=Layer I
    bool     crc_protected;
    uint8_t  bitrate_idx;
    uint8_t  sample_rate_idx;
    bool     padding;
    uint8_t  channel_mode;
    uint32_t frame_length; // In Bytes
} Mp3FrameHeader;

struct invalid_frame {
    uint32_t timer = 0;
    bool start = true;
    uint32_t count1 = 0;
    uint32_t count2 = 0;

};