
#pragma once
//#pragma GCC optimize ("O3")
//#pragma GCC diagnostic ignored "-Wnarrowing"

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation https://xiph.org/                     *
 *                                                                  *
 ********************************************************************/
/*
 * vorbis_decoder.h
 * based on Xiph.Org Foundation vorbis decoder
 * adapted for the ESP32 by schreibfaul1
 *
 *  Created on: 13.02.2023
 *  Updated on: 03.04.2024
 */




#include "Arduino.h"
#include <vector>
using namespace std;
#define VI_FLOORB       2
#define VIF_POSIT      63

#define LSP_FRACBITS   14

#define OV_EREAD      -128
#define OV_EFAULT     -129
#define OV_EIMPL      -130
#define OV_EINVAL     -131
#define OV_ENOTVORBIS -132
#define OV_EBADHEADER -133
#define OV_EVERSION   -134
#define OV_ENOTAUDIO  -135
#define OV_EBADPACKET -136
#define OV_EBADLINK   -137
#define OV_ENOSEEK    -138

#define INVSQ_LOOKUP_I_SHIFT 10
#define INVSQ_LOOKUP_I_MASK  1023
#define COS_LOOKUP_I_SHIFT   9
#define COS_LOOKUP_I_MASK    511
#define COS_LOOKUP_I_SZ      128

#define cPI3_8 (0x30fbc54d)
#define cPI2_8 (0x5a82799a)
#define cPI1_8 (0x7641af3d)

enum : int8_t  {VORBIS_CONTINUE = 110,
                VORBIS_PARSE_OGG_DONE = 100,
                ERR_VORBIS_NONE = 0,
                ERR_VORBIS_CHANNELS_OUT_OF_RANGE = -1,
                ERR_VORBIS_INVALID_SAMPLERATE = -2,
                ERR_VORBIS_EXTRA_CHANNELS_UNSUPPORTED = -3,
                ERR_VORBIS_DECODER_ASYNC = -4,
                ERR_VORBIS_OGG_SYNC_NOT_FOUND = - 5,
                ERR_VORBIS_BAD_HEADER = -6,
                ERR_VORBIS_NOT_AUDIO = -7,
                ERR_VORBIS_BAD_PACKET = -8
            };

typedef struct _codebook{
    uint8_t  dim;          /* codebook dimensions (elements per vector) */
    int16_t  entries;      /* codebook entries */
    uint16_t used_entries; /* populated codebook entries */
    uint32_t dec_maxlength;
    void    *dec_table;
    uint32_t dec_nodeb;
    uint32_t dec_leafw;
    uint32_t dec_type; /* 0 = entry number
                          1 = packed vector of values
                          2 = packed vector of column offsets, maptype 1
                          3 = scalar offset into value array,  maptype 2  */
    int32_t q_min;
    int32_t     q_minp;
    int32_t q_del;
    int32_t     q_delp;
    int32_t     q_seq;
    int32_t     q_bits;
    uint8_t q_pack;
    void   *q_val;
} codebook_t;

typedef struct{
    char           class_dim;        /* 1 to 8 */
    char           class_subs;       /* 0,1,2,3 (bits: 1<<n poss) */
    uint8_t        class_book;       /* subs ^ dim entries */
    uint8_t        class_subbook[8]; /* [VIF_CLASS][subs] */
} floor1class_t;

typedef struct{
    int32_t            order;
    int32_t        rate;
    int32_t        barkmap;
    int32_t            ampbits;
    int32_t            ampdB;
    int32_t            numbooks; /* <= 16 */
    char           books[16];
    floor1class_t *_class;         /* [VIF_CLASS] */
    uint8_t       *partitionclass; /* [VIF_PARTS]; 0 to 15 */
    uint16_t      *postlist;       /* [VIF_POSIT+2]; first two implicit */
    uint8_t       *forward_index;  /* [VIF_POSIT+2]; */
    uint8_t       *hineighbor;     /* [VIF_POSIT]; */
    uint8_t       *loneighbor;     /* [VIF_POSIT]; */
    int32_t            partitions;     /* 0 to 31 */
    int32_t            posts;
    int32_t            mult;           /* 1 2 3 or 4 */
} vorbis_info_floor_t;

typedef struct _vorbis_info_residue {
    int32_t      type;
    uint8_t *stagemasks;
    uint8_t *stagebooks;
    /* block-partitioned VQ coded straight residue */
    uint32_t begin;
    uint32_t end;
    /* first stage (lossless partitioning) */
    uint32_t grouping;   /* group n vectors per partition */
    char     partitions; /* possible codebooks for a partition */
    uint8_t  groupbook;  /* huffbook for partitioning */
    char     stages;
} vorbis_info_residue_t;

typedef struct _submap{
    char floor;
    char residue;
} submap_t;

typedef struct _coupling_step{  // Mapping backend generic
    uint8_t mag;
    uint8_t ang;
} coupling_step_t;

typedef struct _vorbis_info_mapping{
    int32_t              submaps;
    uint8_t         *chmuxlist;
    submap_t        *submaplist;
    int32_t              coupling_steps;
    coupling_step_t *coupling;
} vorbis_info_mapping_t;

typedef struct {  // mode
    uint8_t blockflag;
    uint8_t mapping;
} vorbis_info_mode_t;

typedef struct _vorbis_dsp_state{  // vorbis_dsp_state buffers the current vorbis audio analysis/synthesis state.
//    vorbis_info     *vi;  // The DSP state be int32_ts to a specific logical bitstream
//    oggpack_buffer_t opb;
    int32_t        **work;
    int32_t        **mdctright;
    int32_t              out_begin;
    int32_t              out_end;
    int32_t          lW;        // last window
    uint32_t         W;         // Window
} vorbis_dsp_state_t;

typedef struct _bitreader{
    uint8_t   *data;
    uint8_t    length;
    uint16_t   headbit;
    uint8_t   *headptr;
    int32_t        headend;
} bitReader_t;

//----------------------------------------------------------------------------------------------------------------------

union magic{
    struct{
        int32_t lo;
        int32_t hi;
    } halves;
    int64_t whole;
};

inline int32_t MULT32(int32_t x, int32_t y) {
    union magic magic;
    magic.whole = (int64_t)x * y;
    return magic.halves.hi;
}

inline int32_t MULT31_SHIFT15(int32_t x, int32_t y) {
    union magic magic;
    magic.whole = (int64_t)x * y;
    return ((uint32_t)(magic.halves.lo) >> 15) | ((magic.halves.hi) << 17);
}

inline int32_t MULT31(int32_t x, int32_t y) { return MULT32(x, y) << 1; }

inline void XPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t *x, int32_t *y) {
    *x = MULT31(a, t) + MULT31(b, v);
    *y = MULT31(b, t) - MULT31(a, v);
}

inline void XNPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t *x, int32_t *y) {
    *x = MULT31(a, t) - MULT31(b, v);
    *y = MULT31(b, t) + MULT31(a, v);
}

inline int32_t CLIP_TO_15(int32_t x) {
    int32_t ret = x;
    ret -= ((x <= 32767) - 1) & (x - 32767);
    ret -= ((x >= -32768) - 1) & (x + 32768);
    return (ret);
}

//----------------------------------------------------------------------------------------------------------------------

// ogg impl
bool                  VORBISDecoder_AllocateBuffers();
void                  VORBISDecoder_FreeBuffers();
void                  VORBISDecoder_ClearBuffers();
void                  VORBISsetDefaults();
void                  clearGlobalConfigurations();
int32_t               VORBISDecode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf);
uint8_t               VORBISGetChannels();
uint32_t              VORBISGetSampRate();
uint32_t              VORBISGetAudioDataStart();
uint8_t               VORBISGetBitsPerSample();
uint32_t              VORBISGetBitRate();
uint16_t              VORBISGetOutputSamps();
char*                 VORBISgetStreamTitle();
vector<uint32_t>      VORBISgetMetadataBlockPicture();
int32_t               VORBISFindSyncWord(unsigned char* buf, int32_t nBytes);
int32_t               VORBISparseOGG(uint8_t* inbuf, int32_t* bytesLeft);
int32_t               vorbisDecodePage1(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
int32_t               vorbisDecodePage2(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
int32_t               vorbisDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
int32_t               vorbisDecodePage4(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t* outbuf);
int32_t               parseVorbisComment(uint8_t* inbuf, int16_t nBytes);
int32_t               parseVorbisCodebook();
int32_t               parseVorbisFirstPacket(uint8_t* inbuf, int16_t nBytes);
uint16_t              continuedOggPackets(uint8_t* inbuf);
int32_t               vorbis_book_unpack(codebook_t* s);
uint32_t              decpack(int32_t entry, int32_t used_entry, uint8_t quantvals, codebook_t* b, int32_t maptype);
int32_t               oggpack_eop();
vorbis_info_floor_t*  floor0_info_unpack();
vorbis_info_floor_t*  floor1_info_unpack();
int32_t               res_unpack(vorbis_info_residue_t* info);
int32_t               mapping_info_unpack(vorbis_info_mapping_t* info);
void                  vorbis_mergesort(uint8_t* index, uint16_t* vals, uint16_t n);
void                  floor_free_info(vorbis_info_floor_t* i);
void                  res_clear_info(vorbis_info_residue_t* info);
void                  mapping_clear_info(vorbis_info_mapping_t* info);
// vorbis decoder impl
int32_t               vorbis_dsp_synthesis(uint8_t* inbuf, uint16_t len, int16_t* outbuf);
vorbis_dsp_state_t*   vorbis_dsp_create();
void                  vorbis_dsp_destroy(vorbis_dsp_state_t* v);
void                  mdct_shift_right(int32_t n, int32_t* in, int32_t* right);
int32_t               mapping_inverse(vorbis_info_mapping_t* info);
int32_t               floor0_memosize(vorbis_info_floor_t* i);
int32_t               floor1_memosize(vorbis_info_floor_t* i);
int32_t*              floor0_inverse1(vorbis_info_floor_t* i, int32_t* lsp);
int32_t*              floor1_inverse1(vorbis_info_floor_t* in, int32_t* fit_value);
int32_t               vorbis_book_decode(codebook_t* book);
int32_t               decode_packed_entry_number(codebook_t* book);
int32_t               render_point(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t x);
int32_t               vorbis_book_decodev_set(codebook_t* book, int32_t* a, int32_t n, int32_t point);
int32_t               decode_map(codebook_t* s, int32_t* v, int32_t point);
int32_t               res_inverse(vorbis_info_residue_t* info, int32_t** in, int32_t* nonzero, uint8_t ch);
int32_t               vorbis_book_decodev_add(codebook_t* book, int32_t* a, int32_t n, int32_t point);
int32_t               vorbis_book_decodevs_add(codebook_t* book, int32_t* a, int32_t n, int32_t point);
int32_t               floor0_inverse2(vorbis_info_floor_t* i, int32_t* lsp, int32_t* out);
int32_t               floor1_inverse2(vorbis_info_floor_t* in, int32_t* fit_value, int32_t* out);
void                  render_line(int32_t n, int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t* d);
void                  vorbis_lsp_to_curve(int32_t* curve, int32_t n, int32_t ln, int32_t* lsp, int32_t m, int32_t amp, int32_t ampoffset, int32_t nyq);
int32_t               toBARK(int32_t n);
int32_t               vorbis_coslook_i(int32_t a);
int32_t               vorbis_coslook2_i(int32_t a);
int32_t               vorbis_fromdBlook_i(int32_t a);
int32_t               vorbis_invsqlook_i(int32_t a, int32_t e);
void                  mdct_backward(int32_t n, int32_t* in);
void                  presymmetry(int32_t* in, int32_t n2, int32_t step);
void                  mdct_butterflies(int32_t* x, int32_t points, int32_t shift);
void                  mdct_butterfly_generic(int32_t* x, int32_t points, int32_t step);
void                  mdct_butterfly_32(int32_t* x);
void                  mdct_butterfly_16(int32_t* x);
void                  mdct_butterfly_8(int32_t* x);
void                  mdct_bitreverse(int32_t* x, int32_t n, int32_t shift);
int32_t               bitrev12(int32_t x);
void                  mdct_step7(int32_t* x, int32_t n, int32_t step);
void                  mdct_step8(int32_t* x, int32_t n, int32_t step);
int32_t               vorbis_book_decodevv_add(codebook_t* book, int32_t** a, int32_t offset, uint8_t ch, int32_t n, int32_t point);
int32_t               vorbis_dsp_pcmout(int16_t* outBuff, int32_t outBuffSize);
void                  mdct_unroll_lap(int32_t n0, int32_t n1, int32_t lW, int32_t W, int32_t* in, int32_t* right, const int32_t* w0, const int32_t* w1, int16_t* out, int32_t step, int32_t start, /* samples, this frame */
                                int32_t end /* samples, this frame */);

// some helper functions
int32_t  VORBIS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
void     bitReader_clear();
void     bitReader_setData(uint8_t *buff, uint16_t buffSize);
int32_t  bitReader(uint16_t bits);
int32_t  bitReader_look(uint16_t nBits);
int8_t   bitReader_adv(uint16_t bits);
uint8_t  _ilog(uint32_t v);
int32_t  ilog(uint32_t v);
int32_t  _float32_unpack(int32_t val, int32_t *point);
int32_t  _determine_node_bytes(uint32_t used, uint8_t leafwidth);
int32_t  _determine_leaf_words(int32_t nodeb, int32_t leafwidth);
int32_t  _make_decode_table(codebook_t *s, char *lengthlist, uint8_t quantvals, int32_t maptype);
int32_t  _make_words(char *l, uint16_t n, uint32_t *r, uint8_t quantvals, codebook_t *b, int32_t maptype);
uint8_t  _book_maptype1_quantvals(codebook_t *b);
void     vorbis_book_clear(codebook_t *b);
int32_t *_vorbis_window(int32_t left);
