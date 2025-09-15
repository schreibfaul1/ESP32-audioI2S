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
#include "celt_defines.h"
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
