
#pragma once
// #pragma GCC optimize ("O3")
// #pragma GCC diagnostic ignored "-Wnarrowing"

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
 *  Updated on: 19.06.2025
 */

#include "../Audio.h"
#include "../psram_unique_ptr.hpp"

class VorbisDecoder : public Decoder {

  public:
    VorbisDecoder(Audio& audioRef) : Decoder(audioRef), audio(audioRef) {}
    ~VorbisDecoder() { reset(); }
    bool                  init() override;
    void                  clear() override;
    void                  reset() override;
    bool                  isValid() override;
    int32_t               findSyncWord(uint8_t* buf, int32_t nBytes) override;
    uint8_t               getChannels() override;
    uint32_t              getSampleRate() override;
    uint32_t              getOutputSamples();
    uint8_t               getBitsPerSample() override;
    uint32_t              getBitRate() override;
    uint32_t              getAudioDataStart() override;
    uint32_t              getAudioFileDuration() override;
    const char*           getStreamTitle() override;
    const char*           whoIsIt() override;
    int32_t               decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf) override;
    void                  setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength) override;
    std::vector<uint32_t> getMetadataBlockPicture() override;
    const char*           arg1() override;
    const char*           arg2() override;
    virtual int32_t       val1() override;
    virtual int32_t       val2() override;

    enum : int8_t { VORBIS_CONTINUE = 110, VORBIS_PARSE_OGG_DONE = 100, VORBIS_NONE = 0, VORBIS_ERR = -1 };
    enum ParseResult { VORBIS_COMMENT_INVALID = -1, VORBIS_COMMENT_NEED_MORE = 100, VORBIS_COMMENT_DONE = 110 };

  private:
    Audio& audio;

#define VI_FLOORB 2
#define VIF_POSIT 63

#define LSP_FRACBITS 14

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

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    const uint32_t mask[33] = {0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff, 0x000001ff, 0x000003ff,
                               0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff,
                               0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    const uint16_t barklook[54] = {0,    51,   102,  154,  206,  258,  311,  365,  420,  477,   535,   594,   656,   719,   785,   854,   926,   1002,
                                   1082, 1166, 1256, 1352, 1454, 1564, 1683, 1812, 1953, 2107,  2276,  2463,  2670,  2900,  3155,  3440,  3756,  4106,
                                   4493, 4919, 5387, 5901, 6466, 7094, 7798, 8599, 9528, 10623, 11935, 13524, 15453, 17775, 20517, 23667, 27183, 31004};
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    const uint8_t MLOOP_1[64] = {
        0,  10, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    };
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    const uint8_t MLOOP_2[64] = {
        0, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    };
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    const uint8_t MLOOP_3[8] = {0, 1, 2, 2, 3, 3, 3, 3};
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    /* interpolated 1./sqrt(p) where .5 <= a < 1. (.100000... to .111111...) in 16.16 format returns in m.8 format */
    int32_t ADJUST_SQRT2[2] = {8192, 5792};
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

    typedef struct {
        char    class_dim;        /* 1 to 8 */
        char    class_subs;       /* 0,1,2,3 (bits: 1<<n poss) */
        uint8_t class_book;       /* subs ^ dim entries */
        uint8_t class_subbook[8]; /* [VIF_CLASS][subs] */
    } floor1class_t;

    typedef struct _submap {
        char floor;
        char residue;
    } submap_t;

    typedef struct _coupling_step { // Mapping backend generic
        uint8_t mag;
        uint8_t ang;
    } coupling_step_t;

    typedef struct { // mode
        uint8_t blockflag;
        uint8_t mapping;
    } vorbis_info_mode_t;

    typedef struct _bitreader {
        uint8_t* data{};    // Anfang des Puffers
        uint8_t* headptr{}; // Aktuelle Leseposition (Byte)
        uint32_t length{};  // Gesamtlänge in Bytes
        uint32_t headend{}; // Verbleibende Bytes ab headptr
        uint8_t  headbit{}; // Aktuelle Bitposition im Byte (0–7)

        void reset() {
            *this = _bitreader{}; // reinitialize cleanly
        }
    } bitReader_t;
    bitReader_t m_bitReader;

    union magic {
        struct {
            int32_t lo;
            int32_t hi;
        } halves;
        int64_t whole;
    };
    struct vorbis_dsp_state { // vorbis_dsp_state buffers the current vorbis audio analysis/synthesis state.
        ps_ptr<ps_ptr<int32_t>> work;
        ps_ptr<ps_ptr<int32_t>> mdctright;
        int32_t                 lW = 0; // last window
        int32_t                 W = 0;  // window
        int32_t                 out_begin = -1;
        int32_t                 out_end = -1;
    };

    struct vorbis_info_mapping {
        int32_t                 submaps{};
        ps_ptr<uint8_t>         chmuxlist{};
        ps_ptr<submap_t>        submaplist{};
        int32_t                 coupling_steps{};
        ps_ptr<coupling_step_t> coupling{};

        void reset() {
            *this = vorbis_info_mapping{}; // sauber neu initialisieren
        }
    };

    struct vorbis_info_residue {
        int32_t         type{};
        ps_ptr<uint8_t> stagemasks{};
        ps_ptr<uint8_t> stagebooks{};
        /* block-partitioned VQ coded straight residue */
        uint32_t begin{};
        uint32_t end{};
        /* first stage (lossless partitioning) */
        uint32_t grouping{};   /* group n vectors per partition */
        char     partitions{}; /* possible codebooks for a partition */
        uint8_t  groupbook{};  /* huffbook for partitioning */
        char     stages{};

        void reset() {
            *this = vorbis_info_residue{}; // reinitialize cleanly
        }
    };

    struct vorbis_info_floor {
        int32_t               order{};
        int32_t               rate{};
        int32_t               barkmap{};
        int32_t               ampbits{};
        int32_t               ampdB{};
        int32_t               numbooks{}; /* <= 16 */
        char                  books[16]{};
        ps_ptr<floor1class_t> _class{};         /* [VIF_CLASS] */
        ps_ptr<uint8_t>       partitionclass{}; /* [VIF_PARTS]; 0 to 15 */
        ps_ptr<uint16_t>      postlist{};       /* [VIF_POSIT+2]; first two implicit */
        ps_ptr<uint8_t>       forward_index{};  /* [VIF_POSIT+2]; */
        ps_ptr<uint8_t>       hineighbor{};     /* [VIF_POSIT]; */
        ps_ptr<uint8_t>       loneighbor{};     /* [VIF_POSIT]; */
        int32_t               partitions{};     /* 0 to 31 */
        int32_t               posts{};
        int32_t               mult{}; /* 1 2 3 or 4 */
    };

    typedef struct _codebook {
        uint8_t          dim{};          /* codebook dimensions (elements per vector) */
        int16_t          entries{};      /* codebook entries */
        uint16_t         used_entries{}; /* populated codebook entries */
        uint32_t         dec_maxlength{};
        ps_ptr<uint16_t> dec_table{};
        uint32_t         dec_nodeb{};
        uint32_t         dec_leafw{};
        uint32_t         dec_type{}; /* 0 = entry number
                                      1 = packed vector of values
                                      2 = packed vector of column offsets, maptype 1
                                      3 = scalar offset into value array,  maptype 2  */
        int32_t          q_min{};
        int32_t          q_minp{};
        int32_t          q_del{};
        int32_t          q_delp{};
        int32_t          q_seq{};
        int32_t          q_bits{};
        uint8_t          q_pack{};
        ps_ptr<uint16_t> q_val{};
    } codebook_t;

    typedef struct _comment {
        uint32_t pointer{};
        uint32_t list_length{};
        bool     oob{}; // out of bounds (block overflow)
        uint32_t save_len{};
        uint32_t comment_size{};
        uint32_t start_pos{};       // comment start file position
        uint32_t end_pos{};         // comment end file position
        uint8_t  length_bytes[4]{}; // 🆕 Addition for split 4-byte length fields
        uint8_t  partial_length{};  // how many of the 4 bytes have already been read
        uint32_t bytes_available{};

        ps_ptr<char>          stream_title{};
        ps_ptr<char>          comment_content{};
        std::vector<uint32_t> item_vec;
        std::vector<uint32_t> pic_vec;

        void reset() { *this = _comment{}; }
    } comment_t;
    comment_t m_comment;

    typedef struct _ogg_items {
        std::deque<uint32_t> segment_table{};
        uint32_t             bytes_consumed_from_other{};
        uint8_t*             data_ptr{};
        uint32_t             lastSegmentTableLen{};
        ps_ptr<uint8_t>      lastSegmentTable{};
        void                 reset() { *this = _ogg_items{}; }
    } ogg_items_t;
    ogg_items_t m_ogg_items;

    // global vars
    bool     m_f_newSteamTitle = false; // streamTitle
    bool     m_f_newMetadataBlockPicture = false;
    bool     m_f_oggFirstPage = false;
    bool     m_f_oggContinuedPage = false;
    bool     m_f_oggLastPage = false;
    bool     m_f_parseOggDone = true;
    bool     m_f_isValid = false;
    bool     m_f_comment_done = false;
    uint16_t m_identificatonHeaderLength = 0;
    uint16_t m_vorbisCommentHeaderLength = 0;
    uint8_t  m_pageNr = 0;
    uint16_t m_oggHeaderSize = 0;
    uint8_t  m_vorbisChannels = 0;
    uint16_t m_vorbisSamplerate = 0;
    uint32_t m_vorbisBitRate = 0;
    uint32_t m_vorbis_segment_length = 0;
    uint32_t m_vorbisCurrentFilePos = 0;
    uint32_t m_vorbisAudioDataStart = 0;

    int32_t  m_vorbisValidSamples = 0;
    int32_t  m_commentBlockSegmentSize = 0;
    uint8_t  m_vorbisOldMode = 0;
    uint32_t m_blocksizes[2];
    uint32_t m_vorbisBlockPicPos = 0;
    uint32_t m_vorbisBlockPicLen = 0;
    int32_t  m_vorbisRemainBlockPicLen = 0;
    int32_t  m_commentLength = 0;

    uint8_t m_nrOfCodebooks = 0;
    uint8_t m_nrOfFloors = 0;
    uint8_t m_nrOfResidues = 0;
    uint8_t m_nrOfMaps = 0;
    uint8_t m_nrOfModes = 0;

    uint16_t m_oggPage3Len = 0; // length of the current audio segment
    int8_t   m_vorbisError = 0;
    float    m_vorbisCompressionRatio = 0;

    ps_ptr<codebook_t>                m_codebooks;
    ps_ptr<ps_ptr<vorbis_info_floor>> m_floor_param{};
    ps_ptr<int8_t>                    m_floor_type;
    ps_ptr<vorbis_info_residue>       m_residue_param;
    ps_ptr<vorbis_info_mapping>       m_map_param;
    ps_ptr<vorbis_info_mode_t>        m_mode_param;
    ps_ptr<vorbis_dsp_state>          m_dsp_state;
    ps_ptr<int16_t>                   m_out16;

    std::vector<uint32_t> m_vorbisBlockPicItem;

    //----------------------------------------------------------------------------------------------------------------------

    // ogg impl

    ps_ptr<vorbis_info_floor> floor0_info_unpack();
    void                      setDefaults();
    void                      clearGlobalConfigurations();
    int32_t                   parse_OGG(uint8_t* inbuf, int32_t* bytesLeft);
    int32_t                   vorbisDecodePage1(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
    int32_t                   vorbisDecodePage2(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, uint32_t current_file_pos);
    int32_t                   vorbisDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
    int32_t                   vorbisDecodePage4(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t* outbuf);
    int32_t                   parseVorbisComment(uint8_t* inbuf, int16_t nBytes, uint32_t current_file_pos);
    int32_t                   parseVorbisCodebook();
    int32_t                   parseVorbisFirstPacket(uint8_t* inbuf, int16_t nBytes);
    int32_t                   vorbis_book_unpack(codebook_t* s);
    uint32_t                  decpack(int32_t entry, int32_t used_entry, uint8_t quantvals, codebook_t* b, int32_t maptype);
    int32_t                   oggpack_eop();
    ps_ptr<vorbis_info_floor> floor1_info_unpack();
    void                      vorbis_mergesort(uint8_t* index, uint16_t* vals, uint16_t n);
    int32_t                   res_unpack(vorbis_info_residue* info);
    int32_t                   mapping_info_unpack(vorbis_info_mapping* info);

    // vorbis decoder impl
    int32_t                  vorbis_dsp_synthesis(uint8_t* inbuf, uint16_t len, int16_t* outbuf);
    ps_ptr<vorbis_dsp_state> vorbis_dsp_create();
    void                     vorbis_dsp_destroy(ps_ptr<vorbis_dsp_state>& v);
    void                     vorbis_book_clear(ps_ptr<codebook_t>& v);
    void                     mdct_shift_right(int32_t n, int32_t* in, int32_t* right);
    int32_t                  mapping_inverse(vorbis_info_mapping* info);
    int32_t                  floor0_memosize(ps_ptr<vorbis_info_floor>& i);
    int32_t                  floor1_memosize(ps_ptr<vorbis_info_floor>& i);
    int32_t*                 floor0_inverse1(ps_ptr<vorbis_info_floor>& i, int32_t* lsp);
    int32_t*                 floor1_inverse1(ps_ptr<vorbis_info_floor>& in, int32_t* fit_value);
    int32_t                  vorbis_book_decode(codebook_t* book);
    int32_t                  decode_packed_entry_number(codebook_t* book);
    int32_t                  render_point(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t x);
    int32_t                  vorbis_book_decodev_set(codebook_t* book, int32_t* a, int32_t n, int32_t point);
    int32_t                  decode_map(codebook_t* s, int32_t* v, int32_t point);
    int32_t                  res_inverse(vorbis_info_residue* info, int32_t** in, int32_t* nonzero, uint8_t ch);
    int32_t                  vorbis_book_decodev_add(codebook_t* book, int32_t* a, int32_t n, int32_t point);
    int32_t                  vorbis_book_decodevs_add(codebook_t* book, int32_t* a, int32_t n, int32_t point);
    int32_t                  floor0_inverse2(ps_ptr<vorbis_info_floor>& i, int32_t* lsp, int32_t* out);
    int32_t                  floor1_inverse2(ps_ptr<vorbis_info_floor>& in, int32_t* fit_value, int32_t* out);
    void                     render_line(int32_t n, int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t* d);
    void                     vorbis_lsp_to_curve(int32_t* curve, int32_t n, int32_t ln, int32_t* lsp, int32_t m, int32_t amp, int32_t ampoffset, int32_t nyq);
    int32_t                  toBARK(int32_t n);
    int32_t                  vorbis_coslook_i(int32_t a);
    int32_t                  vorbis_coslook2_i(int32_t a);
    int32_t                  vorbis_fromdBlook_i(int32_t a);
    int32_t                  vorbis_invsqlook_i(int32_t a, int32_t e);
    void                     mdct_backward(int32_t n, int32_t* in);
    void                     presymmetry(int32_t* in, int32_t n2, int32_t step);
    void                     mdct_butterflies(int32_t* x, int32_t points, int32_t shift);
    void                     mdct_butterfly_generic(int32_t* x, int32_t points, int32_t step);
    void                     mdct_butterfly_32(int32_t* x);
    void                     mdct_butterfly_16(int32_t* x);
    void                     mdct_butterfly_8(int32_t* x);
    void                     mdct_bitreverse(int32_t* x, int32_t n, int32_t shift);
    int32_t                  bitrev12(int32_t x);
    void                     mdct_step7(int32_t* x, int32_t n, int32_t step);
    void                     mdct_step8(int32_t* x, int32_t n, int32_t step);
    int32_t                  vorbis_book_decodevv_add(codebook_t* book, int32_t** a, int32_t offset, uint8_t ch, int32_t n, int32_t point);
    int32_t                  vorbis_dsp_pcmout(int16_t* outBuff, int32_t outBuffSize);
    void                     mdct_unroll_lap(int32_t n0, int32_t n1, int32_t lW, int32_t W, int32_t* in, int32_t* right, const int32_t* w0, const int32_t* w1, int16_t* out, int32_t step,
                                             int32_t start, /* samples, this frame */
                                             int32_t end /* samples, this frame */);

    // some helper functions
    int32_t  special_index_of(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
    void     bitReader_setData(uint8_t* buff, uint32_t buffSize);
    int32_t  bitReader(uint16_t bits);
    int32_t  bitReader_look(uint16_t nBits);
    int8_t   bitReader_adv(uint16_t bits);
    uint8_t  _ilog(uint32_t v);
    int32_t  ilog(uint32_t v);
    int32_t  _float32_unpack(int32_t val, int32_t* point);
    int32_t  _determine_node_bytes(uint32_t used, uint8_t leafwidth);
    int32_t  _determine_leaf_words(int32_t nodeb, int32_t leafwidth);
    int32_t  _make_decode_table(codebook_t* s, int32_t* lengthlist, uint8_t quantvals, int32_t maptype);
    int32_t  _make_words(int32_t* l, uint16_t n, uint32_t* r, uint8_t quantvals, codebook_t* b, int32_t maptype);
    uint8_t  _book_maptype1_quantvals(codebook_t* b);
    int32_t* _vorbis_window(int32_t left);

    int32_t  MULT32(int32_t x, int32_t y);
    int32_t  MULT31_SHIFT15(int32_t x, int32_t y);
    int32_t  MULT31(int32_t x, int32_t y);
    void     XPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t* x, int32_t* y);
    void     XNPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t* x, int32_t* y);
    int32_t  CLIP_TO_15(int32_t x);
    int32_t  specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
    int32_t  specialIndexOf_icase(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
    uint32_t little_endian(uint8_t* data);

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// Macro for comfortable calls
#define VORBIS_LOG_ERROR(fmt, ...)   Audio::AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VORBIS_LOG_WARN(fmt, ...)    Audio::AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VORBIS_LOG_INFO(fmt, ...)    Audio::AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VORBIS_LOG_DEBUG(fmt, ...)   Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define VORBIS_LOG_VERBOSE(fmt, ...) Audio::AUDIO_LOG_IMPL(5, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
