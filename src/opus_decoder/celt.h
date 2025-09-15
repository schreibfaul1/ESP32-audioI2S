/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2008 Gregory Maxwell
   Written by Jean-Marc Valin and Gregory Maxwell */
/**
  @file celt.h
  @brief Contains all the functions for encoding and decoding audio
 */

/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "../psram_unique_ptr.hpp"
#include "Arduino.h"
#include "celt_structs.h"
#include "celt_tables.h"
#include "range_decoder.h"

extern const int16_t          eband5ms[22];
extern const uint8_t          band_allocation[231];
extern const uint32_t         CELT_PVQ_U_DATA[];
extern const int16_t          mdct_twiddles960[];
extern const int16_t          window120[];
extern const int16_t          logN400[];
extern const int16_t          cache_index50[];
extern const uint8_t          cache_bits50[];
extern const uint8_t          cache_caps50[];
extern const kiss_twiddle_cpx fft_twiddles48000_960[];
extern const int16_t          fft_bitrev480[];
extern const int16_t          fft_bitrev240[];
extern const int16_t          fft_bitrev12[];
extern const int16_t          fft_bitrev60[];
extern const uint8_t          LOG2_FRAC_TABLE[];
extern const uint8_t          e_prob_mode[];
extern const uint8_t          small_energy_icdf[];
extern const int8_t           tf_select_table[4][8];
extern const int32_t          ordery_table[];
extern const int32_t          second_check[];
extern const uint8_t          trim_icd[];
extern const uint8_t          spread_icd[];
extern const uint8_t          tapset_icdf[];
extern const uint32_t         row_idx[];

class CeltDecoder {
  public:
    CeltDecoder(RangeDecoder& rangeDecoder) : rd(rangeDecoder) {}
    ~CeltDecoder() { reset(); }
    bool    init();
    void    clear();
    void    reset();
    int32_t celt_decoder_init(int32_t channels);
    int32_t celt_decoder_ctl(int32_t request, ...);
    int32_t celt_decode_with_ec(int16_t* pcm, int32_t frame_size);
    int16_t SAT16(int32_t x);

  private:
    RangeDecoder& rd; // Referenz auf RangeDecoder

#define OPUS_OK                                   0
#define OPUS_BAD_ARG                              -1
#define OPUS_BUFFER_TOO_SMALL                     -2
#define OPUS_INTERNAL_ERROR                       -3
#define OPUS_INVALID_PACKET                       -4
#define OPUS_UNIMPLEMENTED                        -5
#define OPUS_INVALID_STATE                        -6
#define OPUS_ALLOC_FAIL                           -7
#define OPUS_GET_LOOKAHEAD_REQUEST                4027
#define OPUS_RESET_STATE                          4028
#define OPUS_GET_PITCH_REQUEST                    4033
#define OPUS_GET_FINAL_RANGE_REQUEST              4031
#define OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST 4046
#define OPUS_GET_PHASE_INVERSION_DISABLED_REQUEST 4047
#define LEAK_BANDS                                19
#define MAXFACTORS                                8
#define CELT_CLZ0s                                ((int32_t)sizeof(uint32_t) * CHAR_BIT)
#define CELT_CLZ(_x)                              (__builtin_clz(_x))
#define CELT_ILOG(_x)                             (CELT_CLZ0s - CELT_CLZ(_x))
#define DECODER_RESET_START                       rng
#define TOTAL_MODES                               1
#define BITRES                                    3
#define SPREAD_NONE                               (0)
#define SPREAD_LIGHT                              (1)
#define SPREAD_NORMAL                             (2)
#define SPREAD_AGGRESSIVE                         (3)
#define opus_likely(x)                            (__builtin_expect(!!(x), 1))
#define opus_unlikely(x)                          (__builtin_expect(!!(x), 0))
#define assert2(cond, message)
#define TWID_MAX     32767
#define TRIG_UPSCALE 1
#define LPC_ORDER    24
#define S_MUL(a, b)  MULT16_32_Q15(b, a)
#define C_MUL(m, a, b)                                                 \
    do {                                                               \
        (m).r = SUB32_ovflw(S_MUL((a).r, (b).r), S_MUL((a).i, (b).i)); \
        (m).i = ADD32_ovflw(S_MUL((a).r, (b).i), S_MUL((a).i, (b).r)); \
    } while (0)
#define C_MULBYSCALAR(c, s)      \
    do {                         \
        (c).r = S_MUL((c).r, s); \
        (c).i = S_MUL((c).i, s); \
    } while (0)
#define DIVSCALAR(x, k) (x) = S_MUL(x, (TWID_MAX - ((k) >> 1)) / (k) + 1)
#define C_ADD(res, a, b)                     \
    do {                                     \
        (res).r = ADD32_ovflw((a).r, (b).r); \
        (res).i = ADD32_ovflw((a).i, (b).i); \
    } while (0)
#define C_SUB(res, a, b)                     \
    do {                                     \
        (res).r = SUB32_ovflw((a).r, (b).r); \
        (res).i = SUB32_ovflw((a).i, (b).i); \
    } while (0)
#define C_ADDTO(res, a)                        \
    do {                                       \
        (res).r = ADD32_ovflw((res).r, (a).r); \
        (res).i = ADD32_ovflw((res).i, (a).i); \
    } while (0)
#define HALF_OF(x)                  ((x) >> 1)
#define COMBFILTER_MINPERIOD 15
#define comb_filter_const(y, x, T, N, g10, g11, g12) (comb_filter_const_c(y, x, T, N, g10, g11, g12))
#define SIG_SAT                                      (300000000)
#define NORM_SCALING                                 16384
#define DB_SHIFT                                     10
#define EPSILON                                      1
#define VERY_SMALL                                   0
#define VERY_LARGE16                                 ((int16_t)32767)
#define Q15_ONE                                      ((int16_t)32767)
#define SCALEIN(a)                                   (a)
#define SCALEOUT(a)                                  (a)
#define MULT16_16SU(a, b)                            ((int32_t)(int16_t)(a) * (int32_t)(uint16_t)(b)) /** Multiply a 16-bit signed value by a 16-bit uint32_t value. The result is a 32-bit signed value */
#define MULT16_32_P16(a, b)                          ((int32_t)PSHR((int64_t)((int16_t)(a)) * (b), 16)) /** 16x32 multiplication, followed by a 16-bit shift right (round-to-nearest). Results fits in 32 bits */
#define MULT16_32_Q15(a, b)                          ((int32_t)SHR((int64_t)((int16_t)(a)) * (b), 15))  /** 16x32 multiplication, followed by a 15-bit shift right. Results fits in 32 bits */
#define MULT32_32_Q31(a, b)                          ((int32_t)SHR((int64_t)(a) * (int64_t)(b), 31))    /** 32x32 multiplication, followed by a 31-bit shift right. Results fits in 32 bits */
#define QCONST16(x, bits)                            ((int16_t)(0.5L + (x) * (((int32_t)1) << (bits)))) /** Compile-time conversion of float constant to 16-bit value */
#define QCONST32(x, bits)                            ((int32_t)(0.5L + (x) * (((int32_t)1) << (bits)))) /** Compile-time conversion of float constant to 32-bit value */
#define NEG16(x)                                     (-(x))                                             /** Negate a 16-bit value */
#define NEG32(x)                                     (-(x))                                             /** Negate a 32-bit value */
#define EXTRACT16(x)                                 ((int16_t)(x))   /** Change a 32-bit value into a 16-bit value. The value is assumed to fit in 16-bit, otherwise the result is undefined */
#define EXTEND32(x)                                  ((int32_t)(x))   /** Change a 16-bit value into a 32-bit value */
#define SHR16(a, shift)                              ((a) >> (shift)) /** Arithmetic shift-right of a 16-bit value */
#define SHL16(a, shift)                              ((int16_t)((uint16_t)(a) << (shift)))                   /** Arithmetic shift-left of a 16-bit value */
#define SHR32(a, shift)                              ((a) >> (shift))                                        /** Arithmetic shift-right of a 32-bit value */
#define SHL32(a, shift)                              ((int32_t)((uint32_t)(a) << (shift)))                   /** Arithmetic shift-left of a 32-bit value */
#define PSHR32(a, shift)                             (SHR32((a) + ((EXTEND32(1) << ((shift)) >> 1)), shift)) /** 32-bit arithmetic shift right with rounding-to-nearest instead of rounding down */
#define VSHR32(a, shift)                             (((shift) > 0) ? SHR32(a, shift) : SHL32(a, -(shift)))  /** 32-bit arithmetic shift right where the argument can be negative */
#define SHR(a, shift)                                ((a) >> (shift))                                        /** "RAW" macros, should not be used outside of this header file */
#define SHL(a, shift)                                SHL32(a, shift)
#define PSHR(a, shift)                               (SHR((a) + ((EXTEND32(1) << ((shift)) >> 1)), shift))
#define SATURATE(x, a)                               (((x) > (a) ? (a) : (x) < -(a) ? -(a) : (x)))
#define SATURATE16(x)                                (EXTRACT16((x) > 32767 ? 32767 : (x) < -32768 ? -32768 : (x)))
#define ROUND16(x, a)                                (EXTRACT16(PSHR32((x), (a))))             /** Shift by a and round-to-neareast 32-bit value. Result is a 16-bit value */
#define SROUND16(x, a)                               EXTRACT16(SATURATE(PSHR32(x, a), 32767)); /** Shift by a and round-to-neareast 32-bit value. Result is a saturated 16-bit value */
#define HALF16(x)                                    (SHR16(x, 1))                             /** Divide by two */
#define HALF32(x)                                    (SHR32(x, 1))
#define ADD16(a, b)                                  ((int16_t)((int16_t)(a) + (int16_t)(b)))   /** Add two 16-bit values */
#define SUB16(a, b)                                  ((int16_t)(a) - (int16_t)(b))              /** Subtract two 16-bit values */
#define ADD32(a, b)                                  ((int32_t)(a) + (int32_t)(b))              /** Add two 32-bit values */
#define SUB32(a, b)                                  ((int32_t)(a) - (int32_t)(b))              /** Subtract two 32-bit values */
#define ADD32_ovflw(a, b)                            ((int32_t)((uint32_t)(a) + (uint32_t)(b))) /** Add two 32-bit values, ignore any overflows */
#define SUB32_ovflw(a, b)                            ((int32_t)((uint32_t)(a) - (uint32_t)(b))) /** Subtract two 32-bit values, ignore any overflows */
#define NEG32_ovflw(a)                               ((int32_t)(0 - (uint32_t)(a))) /* Avoid MSVC warning C4146: unary minus operator applied to uint32_t type, Negate 32-bit value, ignore any overflows */
#define MULT16_16_16(a, b)                           ((((int16_t)(a)) * ((int16_t)(b))))
#define MULT16_16(a, b)                              (((int32_t)(int16_t)(a)) * ((int32_t)(int16_t)(b))) /** 16x16 multiplication where the result fits in 32 bits */
#define MAC16_16(c, a, b)                            (ADD32((c), MULT16_16((a), (b))))                   /** 16x16 multiply-add where the result fits in 32 bits */
#define MULT16_16_Q11_32(a, b)                    (SHR(MULT16_16((a), (b)), 11))
#define MULT16_16_Q11(a, b)                       (SHR(MULT16_16((a), (b)), 11))
#define MULT16_16_Q13(a, b)                       (SHR(MULT16_16((a), (b)), 13))
#define MULT16_16_Q14(a, b)                       (SHR(MULT16_16((a), (b)), 14))
#define MULT16_16_Q15(a, b)                       (SHR(MULT16_16((a), (b)), 15))
#define MULT16_16_P13(a, b)                       (SHR(ADD32(4096, MULT16_16((a), (b))), 13))
#define MULT16_16_P14(a, b)                       (SHR(ADD32(8192, MULT16_16((a), (b))), 14))
#define MULT16_16_P15(a, b)                       (SHR(ADD32(16384, MULT16_16((a), (b))), 15))
#define DIV32_16(a, b)                            ((int16_t)(((int32_t)(a)) / ((int16_t)(b)))) /** Divide a 32-bit value by a 16-bit value. Result fits in 16 bits */
#define DIV32(a, b)                               (((int32_t)(a)) / ((int32_t)(b)))            /** Divide a 32-bit value by a 32-bit value. Result fits in 32 bits */
#define celt_div(a, b)                            MULT32_32_Q31((int32_t)(a), celt_rcp(b))
#define MAX_PERIOD                                1024
#define OPUS_MOVE(dst, src, n)                    (memmove((dst), (src), (n) * sizeof(*(dst)) + 0 * ((dst) - (src))))
#define OPUS_CLEAR(dst, n)                        (memset((dst), 0, (n) * sizeof(*(dst))))
#define ALLOC_STEPS                               6
#define celt_inner_prod(x, y, N)                  (celt_inner_prod_c(x, y, N))
#define dual_inner_prod(x, y01, y02, N, xy1, xy2) (dual_inner_prod_c(x, y01, y02, N, xy1, xy2))
#define FRAC_MUL16(a, b)                          ((16384 + ((int32_t)(int16_t)(a) * (int16_t)(b))) >> 15) /* Multiplies two 16-bit fractional values. Bit-exactness of this macro is important */
#define VARDECL(type, var)
#define ALLOC(var, size, type)           type var[size]
#define FINE_OFFSET                      21
#define QTHETA_OFFSET                    4
#define QTHETA_OFFSET_TWOPHASE           16
#define MAX_FINE_BITS                    8
#define MAX_PSEUDO                       40
#define LOG_MAX_PSEUDO                   6
#define ALLOC_NONE                       1
#define OPUS_FPRINTF                     (void)
#define DECODE_BUFFER_SIZE               2048
#define CELT_PVQ_U(_n, _k)               (celt_pvq_u_row(min(_n, _k), max(_n, _k)))
#define CELT_PVQ_V(_n, _k)               (CELT_PVQ_U(_n, _k) + CELT_PVQ_U(_n, (_k) + 1))
#define CELT_GET_AND_CLEAR_ERROR_REQUEST 10007
#define CELT_SET_CHANNELS_REQUEST        10008
#define CELT_SET_START_BAND_REQUEST      10010
#define CELT_SET_END_BAND_REQUEST        10012
#define CELT_GET_MODE_REQUEST            10015
#define CELT_SET_SIGNALLING_REQUEST      10016
#define CELT_SET_TONALITY_REQUEST        10018
#define CELT_SET_TONALITY_SLOPE_REQUEST  10020
#define CELT_SET_ANALYSIS_REQUEST        10022
#define OPUS_SET_LFE_REQUEST             10024
#define OPUS_SET_ENERGY_MASK_REQUEST     10026
#define CELT_SET_SILK_INFO_REQUEST       10028
#define PLC_PITCH_LAG_MAX                720
#define PLC_PITCH_LAG_MIN                100

    const kiss_fft_state fft_state48000_960_0 = {
        480,                                                /* nfft */
        17476,                                              /* scale */
        8,                                                  /* scale_shift */
        -1,                                                 /* shift */
        {5, 96, 3, 32, 4, 8, 2, 4, 4, 1, 0, 0, 0, 0, 0, 0}, /* factors */
        fft_bitrev480,                                      /* bitrev */
        fft_twiddles48000_960,                              /* bitrev */
    };

    const kiss_fft_state fft_state48000_960_1 = {
        240,                                                /* nfft */
        17476,                                              /* scale */
        7,                                                  /* scale_shift */
        1,                                                  /* shift */
        {5, 48, 3, 16, 4, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0}, /* factors */
        fft_bitrev240,                                      /* bitrev */
        fft_twiddles48000_960,                              /* bitrev */
    };

    const kiss_fft_state fft_state48000_960_2 = {
        120,                                               /* nfft */
        17476,                                             /* scale */
        6,                                                 /* scale_shift */
        2,                                                 /* shift */
        {5, 24, 3, 8, 2, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0}, /* factors */
        fft_bitrev120,                                     /* bitrev */
        fft_twiddles48000_960,                             /* bitrev */
    };
    const kiss_fft_state fft_state48000_960_3 = {
        60,                                                /* nfft */
        17476,                                             /* scale */
        5,                                                 /* scale_shift */
        3,                                                 /* shift */
        {5, 12, 3, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* factors */
        fft_bitrev60,                                      /* bitrev */
        fft_twiddles48000_960,                             /* bitrev */
    };

    const CELTMode_t m_CELTMode = {
        48000,                  /* Fs */
        120,                    /* overlap */
        21,                     /* nbEBands */
        21,                     /* effEBands */
        {27853, 0, 4096, 8192}, /* preemph */
        3,                      /* maxLM */
        8,                      /* nbShortMdcts */
        120,                    /* shortMdctSize */
        11,                     /* nbAllocVectors */
    };

    const mdct_lookup_t m_mdct_lookup = {
        1920,
        3,
        {
            &fft_state48000_960_0,
            &fft_state48000_960_1,
            &fft_state48000_960_2,
            &fft_state48000_960_3,
        },
        mdct_twiddles960, /* mdct */
    };

    // ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    CELTDecoder_t   m_celtDec; // unique pointer
    ps_ptr<int32_t> m_decode_mem;
    band_ctx_t      m_band_ctx;
    // ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

    int32_t  celt_inner_prod_c(const int16_t* x, const int16_t* y, int32_t N);
    int32_t  celt_rcp(int32_t x);
    uint32_t celt_pvq_u_row(uint32_t row, uint32_t data);
    void     exp_rotation1(int16_t* X, int32_t len, int32_t stride, int16_t c, int16_t s);
    void     exp_rotation(int16_t* X, int32_t len, int32_t dir, int32_t stride, int32_t K, int32_t spread);
    void     normalise_residual(int32_t* iy, int16_t* X, int32_t N, int32_t Ryy, int16_t gain);
    uint32_t extract_collapse_mask(int32_t* iy, int32_t N, int32_t B);
    uint32_t alg_unquant(int16_t* X, int32_t N, int32_t K, int32_t spread, int32_t B, int16_t gain);
    void     renormalise_vector(int16_t* X, int32_t N, int16_t gain);
    int32_t  resampling_factor(int32_t rate);
    void     comb_filter_const_c(int32_t* y, int32_t* x, int32_t T, int32_t N, int16_t g10, int16_t g11, int16_t g12);
    void     comb_filter(int32_t* y, int32_t* x, int32_t T0, int32_t T1, int32_t N, int16_t g0, int16_t g1, int32_t tapset0, int32_t tapset1);
    void     init_caps(int32_t* cap, int32_t LM, int32_t C);
    uint32_t celt_lcg_rand(uint32_t seed);
    int16_t  bitexact_cos(int16_t x);
    int32_t  bitexact_log2tan(int32_t isin, int32_t icos);
    void     denormalise_bands(const int16_t* X, int32_t* freq, const int16_t* bandLogE, int32_t start, int32_t end, int32_t M, int32_t downsample, int32_t silence);
    void anti_collapse(int16_t* X_, uint8_t* collapse_masks, int32_t LM, int32_t C, int32_t size, int32_t start, int32_t end, const int16_t* logE, const int16_t* prev1logE, const int16_t* prev2logE,
                       const int32_t* pulses, uint32_t seed);
    void compute_channel_weights(int32_t Ex, int32_t Ey, int16_t w[2]);
    void stereo_split(int16_t* X, int16_t* Y, int32_t N);
    void stereo_merge(int16_t* X, int16_t* Y, int16_t mid, int32_t N);
    void deinterleave_hadamard(int16_t* X, int32_t N0, int32_t stride, int32_t hadamard);
    void interleave_hadamard(int16_t* X, int32_t N0, int32_t stride, int32_t hadamard);
    void haar1(int16_t* X, int32_t N0, int32_t stride);
    int32_t  compute_qn(int32_t N, int32_t b, int32_t offset, int32_t pulse_cap, int32_t stereo);
    void     compute_theta(struct split_ctx* sctx, int16_t* X, int16_t* Y, int32_t N, int32_t* b, int32_t B, int32_t __B0, int32_t LM, int32_t stereo, int32_t* fill);
    uint32_t quant_band_n1(int16_t* X, int16_t* Y, int32_t b, int16_t* lowband_out);
    uint32_t quant_partition(int16_t* X, int32_t N, int32_t b, int32_t B, int16_t* lowband, int32_t LM, int16_t gain, int32_t fill);
    uint32_t quant_band(int16_t* X, int32_t N, int32_t b, int32_t B, int16_t* lowband, int32_t LM, int16_t* lowband_out, int16_t gain, int16_t* lowband_scratch, int32_t fill);
    uint32_t quant_band_stereo(int16_t* X, int16_t* Y, int32_t N, int32_t b, int32_t B, int16_t* lowband, int32_t LM, int16_t* lowband_out, int16_t* lowband_scratch, int32_t fill);
    void     special_hybrid_folding(int16_t* norm, int16_t* norm2, int32_t start, int32_t M, int32_t dual_stereo);
    void quant_all_bands(int32_t start, int32_t end, int16_t* X_, int16_t* Y_, uint8_t* collapse_masks, const int32_t* bandE, int32_t* pulses, int32_t shortBlocks, int32_t spread, int32_t dual_stereo,
                         int32_t intensity, int32_t* tf_res, int32_t total_bits, int32_t balance, int32_t LM, int32_t codedBands, uint32_t* seed, int32_t complexity, int32_t disable_inv);
    int32_t celt_decoder_get_size(int32_t channels);
    void    deemphasis_stereo_simple(int32_t* in[], int16_t* pcm, int32_t N, const int16_t coef0, int32_t* mem);
    void    deemphasis(int32_t* in[], int16_t* pcm, int32_t N, int32_t C, int32_t downsample, const int16_t* coef, int32_t* mem, int32_t accum);
    void celt_synthesis(int16_t* X, int32_t* out_syn[], int16_t* oldBandE, int32_t start, int32_t effEnd, int32_t C, int32_t CC, int32_t isTransient, int32_t LM, int32_t downsample, int32_t silence);
    void tf_decode(int32_t start, int32_t end, int32_t isTransient, int32_t* tf_res, int32_t LM);
    int32_t  cwrsi(int32_t _n, int32_t _k, uint32_t _i, int32_t* _y);
    int32_t  decode_pulses(int32_t* _y, int32_t _n, int32_t _k);
    void     kf_bfly2(kiss_fft_cpx* Fout, int32_t m, int32_t N);
    void     kf_bfly4(kiss_fft_cpx* Fout, const size_t fstride, const kiss_fft_state* st, int32_t m, int32_t N, int32_t mm);
    void     kf_bfly3(kiss_fft_cpx* Fout, const size_t fstride, const kiss_fft_state* st, int32_t m, int32_t N, int32_t mm);
    void     kf_bfly5(kiss_fft_cpx* Fout, const size_t fstride, const kiss_fft_state* st, int32_t m, int32_t N, int32_t mm);
    void     opus_fft_impl(const kiss_fft_state* st, kiss_fft_cpx* fout);
    uint32_t isqrt32(uint32_t _val);
    int16_t  celt_rsqrt_norm(int32_t x);
    int32_t  celt_sqrt(int32_t x);
    int16_t  celt_cos_norm(int32_t x);
    void     clt_mdct_backward(int32_t* in, int32_t* out, int32_t overlap, int32_t shift, int32_t stride);
    int32_t  interp_bits2pulses(int32_t start, int32_t end, int32_t skip_start, const int32_t* bits1, const int32_t* bits2, const int32_t* thresh, const int32_t* cap, int32_t total, int32_t* _balance,
                                int32_t skip_rsv, int32_t* intensity, int32_t intensity_rsv, int32_t* dual_stereo, int32_t dual_stereo_rsv, int32_t* bits, int32_t* ebits, int32_t* fine_priority,
                                int32_t C, int32_t LM, int32_t encode, int32_t prev, int32_t signalBandwidth);
    int32_t  clt_compute_allocation(int32_t start, int32_t end, const int32_t* offsets, const int32_t* cap, int32_t alloc_trim, int32_t* intensity, int32_t* dual_stereo, int32_t total,
                                    int32_t* balance, int32_t* pulses, int32_t* ebits, int32_t* fine_priority, int32_t C, int32_t LM, int32_t encode, int32_t prev, int32_t signalBandwidth);
    void     unquant_coarse_energy(int32_t start, int32_t end, int16_t* oldEBands, int32_t intra, int32_t C, int32_t LM);
    void     unquant_fine_energy(int32_t start, int32_t end, int16_t* oldEBands, int32_t* fine_quant, int32_t C);
    void     unquant_energy_finalise(int32_t start, int32_t end, int16_t* oldEBands, int32_t* fine_quant, int32_t* fine_priority, int32_t bits_left, int32_t C);
    uint32_t celt_udiv(uint32_t n, uint32_t d);
    int32_t  celt_sudiv(int32_t n, int32_t d);
    int16_t  sig2word16(int32_t x);
    int16_t  celt_atan01(int16_t x);
    int16_t  celt_atan2p(int16_t y, int16_t x);
    int32_t  celt_maxabs16(const int16_t* x, int32_t len);
    int32_t  celt_maxabs32(const int32_t* x, int32_t len);
    int16_t  celt_ilog2(int32_t x);
    int16_t  celt_zlog2(int32_t x);
    int32_t  celt_exp2_frac(int16_t x);
    int32_t  celt_exp2(int16_t x);
    void     dual_inner_prod_c(const int16_t* x, const int16_t* y01, const int16_t* y02, int32_t N, int32_t* xy1, int32_t* xy2);
    int32_t  get_pulses(int32_t i);
    int32_t  bits2pulses(int32_t band, int32_t LM, int32_t bits);
    int32_t  pulses2bits(int32_t band, int32_t LM, int32_t pulses);
    int16_t  celt_log2(int32_t x);
    int16_t  _celt_cos_pi_2(int16_t x);
};
