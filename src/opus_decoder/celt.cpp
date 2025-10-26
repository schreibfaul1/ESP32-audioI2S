/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2010 Xiph.Org Foundation
   Copyright (c) 2008 Gregory Maxwell
   Written by Jean-Marc Valin and Gregory Maxwell */
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

#include <Arduino.h>
#include "range_decoder.h"
#include "opus_decoder.h"
#include "celt.h"


//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t CeltDecoder::celt_pvq_u_row(uint32_t row, uint32_t data){
    uint32_t  ret = CELT_PVQ_U_DATA[row_idx[row] + data];
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool CeltDecoder::init() {
    size_t omd = celt_decoder_get_size(2);
    m_decode_mem.alloc(omd, "decode_mem");
    if (m_decode_mem.valid()) {
        OPUS_LOG_DEBUG("Celt decoder, allocated bytes: %u", omd);
        m_decode_mem.clear();  // mem zero
        return true;
    }
    OPUS_LOG_ERROR("oom for %i bytes", omd);
    return false;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::clear(){
    m_decode_mem.clear();  // mem zero
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::reset(){
    m_decode_mem.reset();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::exp_rotation1(int16_t *X, int32_t len, int32_t stride, int16_t c, int16_t s) {
    int32_t i;
    int16_t ms;
    int16_t *Xptr;
    Xptr = X;
    ms = NEG16(s);
    for (i = 0; i < len - stride; i++) {
        int16_t x1, x2;
        x1 = Xptr[0];
        x2 = Xptr[stride];
        Xptr[stride] = EXTRACT16(PSHR32(MAC16_16(MULT16_16(c, x2), s, x1), 15));
        *Xptr++ = EXTRACT16(PSHR32(MAC16_16(MULT16_16(c, x1), ms, x2), 15));
    }
    Xptr = &X[len - 2 * stride - 1];
    for (i = len - 2 * stride - 1; i >= 0; i--) {
        int16_t x1, x2;
        x1 = Xptr[0];
        x2 = Xptr[stride];
        Xptr[stride] = EXTRACT16(PSHR32(MAC16_16(MULT16_16(c, x2), s, x1), 15));
        *Xptr-- = EXTRACT16(PSHR32(MAC16_16(MULT16_16(c, x1), ms, x2), 15));
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::exp_rotation(int16_t *X, int32_t len, int32_t dir, int32_t stride, int32_t K, int32_t spread) {
    const int32_t SPREAD_FACTOR[3] = {15, 10, 5};
    int32_t i;
    int16_t c, s;
    int16_t gain, theta;
    int32_t stride2 = 0;
    int32_t factor;

    if (2 * K >= len || spread == SPREAD_NONE) return;
    factor = SPREAD_FACTOR[spread - 1];

    gain = celt_div((int32_t)MULT16_16(Q15_ONE, len), (int32_t)(len + factor * K));
    theta = HALF16(MULT16_16_Q15(gain, gain));

    c = celt_cos_norm(EXTEND32(theta));
    s = celt_cos_norm(EXTEND32(SUB16(32767, theta))); /*  sin(theta) */

    if (len >= 8 * stride) {
        stride2 = 1;
        /* This is just a simple (equivalent) way of computing sqrt(len/stride) with rounding.
           It's basically incrementing long as (stride2+0.5)^2 < len/stride. */
        while ((stride2 * stride2 + stride2) * stride + (stride >> 2) < len) stride2++;
    }
    /*NOTE: As a minor optimization, we could be passing around log2(B), not B, for both this and for
       extract_collapse_mask().*/
    len = celt_udiv(len, stride);
    for (i = 0; i < stride; i++) {
        if (dir < 0) {
            if (stride2) exp_rotation1(X + i * len, len, stride2, s, c);
            exp_rotation1(X + i * len, len, 1, c, s);
        } else {
            exp_rotation1(X + i * len, len, 1, c, -s);
            if (stride2) exp_rotation1(X + i * len, len, stride2, s, -c);
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Takes the pitch vector and the decoded residual vector, computes the gain that will give ||p+g*y||=1 and mixes the residual with the pitch. */
void CeltDecoder::normalise_residual(int32_t * iy, int16_t * X, int32_t N, int32_t Ryy, int16_t gain) {
    int32_t i;
    int32_t k;
    int32_t t;
    int16_t g;

    k = celt_ilog2(Ryy) >> 1;
    t = VSHR32(Ryy, 2 * (k - 7));
    g = MULT16_16_P15(celt_rsqrt_norm(t), gain);

    i = 0;
    do X[i] = EXTRACT16(PSHR32(MULT16_16(g, iy[i]), k + 1));
    while (++i < N);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t CeltDecoder::extract_collapse_mask(int32_t *iy, int32_t N, int32_t B) {
    uint32_t collapse_mask;
    int32_t N0;
    int32_t i;
    if (B <= 1) return 1;
    /*NOTE: As a minor optimization, we could be passing around log2(B), not B, for both this and for exp_rotation().*/
    N0 = celt_udiv(N, B);
    collapse_mask = 0;
    i = 0;
    do {
        int32_t j;
        uint32_t tmp = 0;
        j = 0;
        do {
            tmp |= iy[i * N0 + j];
        } while (++j < N0);
        collapse_mask |= (tmp != 0) << i;
    } while (++i < B);
    return collapse_mask;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Decode pulse vector and combine the result with the pitch vector to produce the final normalised signal in the current band. */
uint32_t CeltDecoder::alg_unquant(int16_t *X, int32_t N, int32_t K, int32_t spread, int32_t B, int16_t gain) {
    int32_t Ryy;
    uint32_t collapse_mask;

    assert2(K > 0, "alg_unquant() needs at least one pulse");
    assert2(N > 1, "alg_unquant() needs at least two dimensions");
    ps_ptr<int32_t>iy; iy.alloc_array(N + 3);

    Ryy = decode_pulses(iy.get(), N, K);
    normalise_residual(iy.get(), X, N, Ryy, gain);
    exp_rotation(X, N, -1, B, K, spread);
    collapse_mask = extract_collapse_mask(iy.get(), N, B);
    return collapse_mask;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::renormalise_vector(int16_t *X, int32_t N, int16_t gain) {
    int32_t i;
    int32_t k;
    int32_t E;
    int16_t g;
    int32_t t;
    int16_t *xptr;
    E = EPSILON + celt_inner_prod(X, X, N);
    k = celt_ilog2(E) >> 1;
    t = VSHR32(E, 2 * (k - 7));
    g = MULT16_16_P15(celt_rsqrt_norm(t), gain);

    xptr = X;
    for (i = 0; i < N; i++) {
        *xptr = EXTRACT16(PSHR32(MULT16_16(g, *xptr), k + 1));
        xptr++;
    }
    /*return celt_sqrt(E);*/
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::resampling_factor(int32_t rate){
    int32_t ret;
    switch (rate){
        case 48000: ret = 1;  break;
        case 24000: ret = 2;  break;
        case 16000: ret = 3;  break;
        case 12000: ret = 4;  break;
        case 8000:  ret = 6;  break;
        default:    ret = 0;  break;
    }
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::comb_filter_const_c(int32_t *y, int32_t *x, int32_t T, int32_t N, int16_t g10, int16_t g11, int16_t g12) {
    int32_t x0, x1, x2, x3, x4;
    int32_t i;
    x4 = x[-T - 2];
    x3 = x[-T - 1];
    x2 = x[-T];
    x1 = x[-T + 1];
    for (i = 0; i < N; i++) {
        x0 = x[i - T + 2];
        y[i] = x[i] + MULT16_32_Q15(g10, x2) + MULT16_32_Q15(g11, ADD32(x1, x3)) + MULT16_32_Q15(g12, ADD32(x0, x4));
        y[i] = SATURATE(y[i], SIG_SAT);
        x4 = x3;
        x3 = x2;
        x2 = x1;
        x1 = x0;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::comb_filter(int32_t *y, int32_t *x, int32_t T0, int32_t T1, int32_t N, int16_t g0, int16_t g1, int32_t tapset0, int32_t tapset1){
    int32_t i;
    /* printf ("%d %d %f %f\n", T0, T1, g0, g1); */
    uint8_t  overlap = m_CELTMode.overlap; // =120
    int16_t g00, g01, g02, g10, g11, g12;
    int32_t x0, x1, x2, x3, x4;
    const int16_t gains[3][3] = {
        {QCONST16(0.3066406250f, 15), QCONST16(0.2170410156f, 15), QCONST16(0.1296386719f, 15)},
        {QCONST16(0.4638671875f, 15), QCONST16(0.2680664062f, 15), QCONST16(0.f, 15)},
        {QCONST16(0.7998046875f, 15), QCONST16(0.1000976562f, 15), QCONST16(0.f, 15)}};

    if (g0 == 0 && g1 == 0) {
        if (x != y)
            OPUS_MOVE(y, x, N);
        return;
    }
    /* When the gain is zero, T0 and/or T1 is set to zero. We need
       to have then be at least 2 to avoid processing garbage data. */
    T0 = max(T0, (int32_t)COMBFILTER_MINPERIOD);
    T1 = max(T1, (int32_t)COMBFILTER_MINPERIOD);
    g00 = MULT16_16_P15(g0, gains[tapset0][0]);
    g01 = MULT16_16_P15(g0, gains[tapset0][1]);
    g02 = MULT16_16_P15(g0, gains[tapset0][2]);
    g10 = MULT16_16_P15(g1, gains[tapset1][0]);
    g11 = MULT16_16_P15(g1, gains[tapset1][1]);
    g12 = MULT16_16_P15(g1, gains[tapset1][2]);
    x1 = x[-T1 + 1];
    x2 = x[-T1];
    x3 = x[-T1 - 1];
    x4 = x[-T1 - 2];
    /* If the filter didn't change, we don't need the overlap */
    if (g0 == g1 && T0 == T1 && tapset0 == tapset1) overlap = 0;
    for (i = 0; i < overlap; i++) {
        int16_t f;
        x0 = x[i - T1 + 2];
        f = MULT16_16_Q15(window120[i], window120[i]);
        y[i] = x[i] + MULT16_32_Q15(MULT16_16_Q15((32767 - f), g00), x[i - T0]) + MULT16_32_Q15(MULT16_16_Q15((32767 - f), g01), ADD32(x[i - T0 + 1], x[i - T0 - 1])) + MULT16_32_Q15(MULT16_16_Q15((32767 - f), g02), ADD32(x[i - T0 + 2], x[i - T0 - 2])) + MULT16_32_Q15(MULT16_16_Q15(f, g10), x2) + MULT16_32_Q15(MULT16_16_Q15(f, g11), ADD32(x1, x3)) + MULT16_32_Q15(MULT16_16_Q15(f, g12), ADD32(x0, x4));
        y[i] = SATURATE(y[i], SIG_SAT);
        x4 = x3;
        x3 = x2;
        x2 = x1;
        x1 = x0;
    }
    if (g1 == 0) {
        if (x != y)
            OPUS_MOVE(y + overlap, x + overlap, N - overlap);
        return;
    }

    /* Compute the part with the constant filter. */
    comb_filter_const(y + i, x + i, T1, N - i, g10, g11, g12);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::init_caps(int32_t *cap, int32_t LM, int32_t C) {
    int32_t i;
    for (i = 0; i < m_CELTMode.nbEBands; i++)
    {
        int32_t N;
        N = (eband5ms[i + 1] - eband5ms[i]) << LM;
        cap[i] = (cache_caps50[m_CELTMode.nbEBands * (2 * LM + C - 1) + i] + 64) * C * N >> 2;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t CeltDecoder::celt_lcg_rand(uint32_t seed) {
    return 1664525 * seed + 1013904223;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This is a cos() approximation designed to be bit-exact on any platform. Bit exactness with this approximation is important because it has an impact on the bit allocation */
int16_t CeltDecoder::bitexact_cos(int16_t x) {
    int32_t tmp;
    int16_t x2;
    tmp = (4096 + ((int32_t)(x) * (x))) >> 13;
    assert(tmp <= 32767);
    x2 = tmp;
    x2 = (32767 - x2) + FRAC_MUL16(x2, (-7651 + FRAC_MUL16(x2, (8277 + FRAC_MUL16(-626, x2)))));
    assert(x2 <= 32766);
    return 1 + x2;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::bitexact_log2tan(int32_t isin, int32_t icos) {
    int32_t lc;
    int32_t ls;
    lc = CELT_ILOG(icos);
    ls = CELT_ILOG(isin);
    icos <<= 15 - lc;
    isin <<= 15 - ls;
    return (ls - lc) * (1 << 11) + FRAC_MUL16(isin, FRAC_MUL16(isin, -2597) + 7932) - FRAC_MUL16(icos, FRAC_MUL16(icos, -2597) + 7932);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* De-normalise the energy to produce the synthesis from the unit-energy bands */
void CeltDecoder::denormalise_bands(const int16_t * X, int32_t * freq,
                       const int16_t *bandLogE, int32_t start, int32_t end, int32_t M, int32_t downsample, int32_t silence) {
    int32_t i, N;
    int32_t bound;
    int32_t * f;
    const int16_t * x;
    const int16_t *eBands = eband5ms;
    N = M * m_CELTMode.shortMdctSize;
    bound = M * eBands[end];
    if (downsample != 1) bound = min(bound, N / downsample);
    if (silence) {
        bound = 0;
        start = end = 0;
    }
    f = freq;
    x = X + M * eBands[start];
    for (i = 0; i < M * eBands[start]; i++)
        *f++ = 0;
    for (i = start; i < end; i++) {
        int32_t j, band_end;
        int16_t g;
        int16_t lg;

        int32_t shift;

        j = M * eBands[i];
        band_end = M * eBands[i + 1];
        lg = SATURATE16(ADD32(bandLogE[i], SHL32((int32_t)eMeans[i], 6)));

        /* Handle the integer part of the log energy */
        shift = 16 - (lg >> DB_SHIFT);
        if (shift > 31) {
            shift = 0;
            g = 0;
        }
        else {
            /* Handle the fractional part. */
            g = celt_exp2_frac(lg & ((1 << DB_SHIFT) - 1));
        }
        /* Handle extreme gains with negative shift. */
        if (shift < 0) {
            /* For shift <= -2 and g > 16384 we'd be likely to overflow, so we're capping the gain here, which is equivalent to a cap of 18 on lg.
               This shouldn't trigger unless the bitstream is already corrupted. */
            if (shift <= -2) {
                g = 16384;
                shift = -2;
            }
            do {
                *f++ = SHL32(MULT16_16(*x++, g), -shift);
            } while (++j < band_end);
        }
        else
            /* Be careful of the fixed-point "else" just above when changing this code */
            do {
                *f++ = SHR32(MULT16_16(*x++, g), shift);
            } while (++j < band_end);
    }
    assert(start <= end);
    OPUS_CLEAR(&freq[bound], N - bound);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This prevents energy collapse for transients with multiple short MDCTs */
void CeltDecoder::anti_collapse(int16_t *X_, uint8_t *collapse_masks, int32_t LM, int32_t C, int32_t size, int32_t start,
                   int32_t end, const int16_t *logE, const int16_t *prev1logE, const int16_t *prev2logE, const int32_t *pulses,
                   uint32_t seed){
    int32_t c, i, j, k;
    for (i = start; i < end; i++) {
        int32_t N0;
        int16_t thresh, sqrt_1;
        int32_t depth;

        int32_t shift;
        int32_t thresh32;

        N0 = eband5ms[i + 1] - eband5ms[i];
        /* depth in 1/8 bits */
        assert(pulses[i] >= 0);
        depth = celt_udiv(1 + pulses[i], (eband5ms[i + 1] - eband5ms[i])) >> LM;

        thresh32 = SHR32(celt_exp2(-SHL16(depth, 10 - BITRES)), 1);
        thresh = MULT16_32_Q15(QCONST16(0.5f, 15), min((int32_t)32767, thresh32)); {
            int32_t t;
            t = N0 << LM;
            shift = celt_ilog2(t) >> 1;
            t = SHL32(t, (7 - shift) << 1);
            sqrt_1 = celt_rsqrt_norm(t);
        }

        c = 0;
        do {
            int16_t *X;
            int16_t prev1;
            int16_t prev2;
            int32_t Ediff;
            int16_t r;
            int32_t renormalize = 0;
            prev1 = prev1logE[c * m_CELTMode.nbEBands + i];
            prev2 = prev2logE[c * m_CELTMode.nbEBands + i];
            if (C == 1) {
                prev1 = max(prev1, prev1logE[m_CELTMode.nbEBands + i]);
                prev2 = max(prev2, prev2logE[m_CELTMode.nbEBands + i]);
            }
            Ediff = EXTEND32(logE[c * m_CELTMode.nbEBands + i]) - EXTEND32(min(prev1, prev2));
            Ediff = max((int32_t)0, Ediff);

            if (Ediff < 16384) {
                int32_t r32 = SHR32(celt_exp2(-EXTRACT16(Ediff)), 1);
                r = 2 * min((int32_t)16383, r32);
            }
            else {
                r = 0;
            }
            if (LM == 3)
                r = MULT16_16_Q14(23170, min((int16_t)23169, r));
            r = SHR16(min(thresh, r), 1);
            r = SHR32(MULT16_16_Q15(sqrt_1, r), shift);

            X = X_ + c * size + (eband5ms[i] << LM);
            for (k = 0; k < 1 << LM; k++) {
                /* Detect collapse */
                if (!(collapse_masks[i * C + c] & 1 << k)) {
                    /* Fill with noise */
                    for (j = 0; j < N0; j++) {
                        seed = celt_lcg_rand(seed);
                        X[(j << LM) + k] = (seed & 0x8000 ? r : -r);
                    }
                    renormalize = 1;
                }
            }
            /* We just added some energy, so we need to renormalise */
            if (renormalize)
                renormalise_vector(X, N0 << LM, 32767);
        } while (++c < C);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Compute the weights to use for optimizing normalized distortion across channels. We use the amplitude to weight   square distortion, which means that we use the square root of the value we would
   have been using if we wanted to minimize the MSE in the non-normalized domain. This roughly corresponds to some quick-and-dirty perceptual experiments I ran to measure inter-aural masking
   (there doesn't seem to be any published data on the topic). */
void CeltDecoder::compute_channel_weights(int32_t Ex, int32_t Ey, int16_t w[2]) {
    int32_t minE;
    int32_t shift;

    minE = min(Ex, Ey);
    /* Adjustment to make the weights a bit more conservative. */
    Ex = ADD32(Ex, minE / 3);
    Ey = ADD32(Ey, minE / 3);

    shift = celt_ilog2(EPSILON + max(Ex, Ey)) - 14;

    w[0] = VSHR32(Ex, shift);
    w[1] = VSHR32(Ey, shift);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::stereo_split(int16_t * X, int16_t * Y, int32_t N) {
    int32_t j;
    for (j = 0; j < N; j++) {
        int32_t r, l;
        l = MULT16_16(QCONST16(.70710678f, 15), X[j]);
        r = MULT16_16(QCONST16(.70710678f, 15), Y[j]);
        X[j] = EXTRACT16(SHR32(ADD32(l, r), 15));
        Y[j] = EXTRACT16(SHR32(SUB32(r, l), 15));
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::stereo_merge(int16_t * X, int16_t * Y, int16_t mid, int32_t N){
    int32_t j;
    int32_t xp = 0, side = 0;
    int32_t El, Er;
    int16_t mid2;

    int32_t kl, kr;

    int32_t t, lgain, rgain;

    /* Compute the norm of X+Y and X-Y as |X|^2 + |Y|^2 +/- sum(xy) */
    dual_inner_prod(Y, X, Y, N, &xp, &side);
    /* Compensating for the mid normalization */
    xp = MULT16_32_Q15(mid, xp);
    /* mid and side are in Q15, not Q14 like X and Y */
    mid2 = SHR16(mid, 1);
    El = MULT16_16(mid2, mid2) + side - 2 * xp;
    Er = MULT16_16(mid2, mid2) + side + 2 * xp;
    if (Er < QCONST32(6e-4f, 28) || El < QCONST32(6e-4f, 28)) {
        memcpy(Y, X, N * sizeof(*Y));
        return;
    }

    kl = celt_ilog2(El) >> 1;
    kr = celt_ilog2(Er) >> 1;

    t = VSHR32(El, (kl - 7) << 1);
    lgain = celt_rsqrt_norm(t);
    t = VSHR32(Er, (kr - 7) << 1);
    rgain = celt_rsqrt_norm(t);

    if (kl < 7)  kl = 7;
    if (kr < 7)  kr = 7;

    for (j = 0; j < N; j++) {
        int16_t r, l;
        /* Apply mid scaling (side is already scaled) */
        l = MULT16_16_P15(mid, X[j]);
        r = Y[j];
        X[j] = EXTRACT16(PSHR32(MULT16_16(lgain, SUB16(l, r)), kl + 1));
        Y[j] = EXTRACT16(PSHR32(MULT16_16(rgain, ADD16(l, r)), kr + 1));
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::deinterleave_hadamard(int16_t *X, int32_t N0, int32_t stride, int32_t hadamard){
    int32_t i, j;
    int32_t N;
    N = N0 * stride;
    ps_ptr<int16_t>tmp; tmp.alloc_array(N);
    assert(stride > 0);
    if (hadamard) {
        const int32_t *ordery = ordery_table + stride - 2;
        for (i = 0; i < stride; i++) {
            for (j = 0; j < N0; j++)
                tmp[ordery[i] * N0 + j] = X[j * stride + i];
        }
    }
    else {
        for (i = 0; i < stride; i++)
            for (j = 0; j < N0; j++)
                tmp[i * N0 + j] = X[j * stride + i];
    }
    memcpy(X, tmp.get(), N * sizeof(*X));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::interleave_hadamard(int16_t *X, int32_t N0, int32_t stride, int32_t hadamard){
    int32_t i, j;
    int32_t N;
    N = N0 * stride;
    ps_ptr<int16_t>tmp; tmp.alloc_array(N);
    if (hadamard) {
        const int32_t *ordery = ordery_table + stride - 2;
        for (i = 0; i < stride; i++)
            for (j = 0; j < N0; j++)
                tmp[j * stride + i] = X[ordery[i] * N0 + j];
    }
    else {
        for (i = 0; i < stride; i++)
            for (j = 0; j < N0; j++)
                tmp[j * stride + i] = X[i * N0 + j];
    }
    memcpy(X, tmp.get(), N * sizeof(*X));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::haar1(int16_t *X, int32_t N0, int32_t stride) {
    int32_t i, j;
    N0 >>= 1;
    for (i = 0; i < stride; i++)
        for (j = 0; j < N0; j++) {
            int32_t tmp1, tmp2;
            tmp1 = MULT16_16(QCONST16(.70710678f, 15), X[stride * 2 * j + i]);
            tmp2 = MULT16_16(QCONST16(.70710678f, 15), X[stride * (2 * j + 1) + i]);
            X[stride * 2 * j + i] = EXTRACT16(PSHR32(ADD32(tmp1, tmp2), 15));
            X[stride * (2 * j + 1) + i] = EXTRACT16(PSHR32(SUB32(tmp1, tmp2), 15));
        }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::compute_qn(int32_t N, int32_t b, int32_t offset, int32_t pulse_cap, int32_t stereo) {
    const int16_t exp2_table8[8] =
        {16384, 17866, 19483, 21247, 23170, 25267, 27554, 30048};
    int32_t qn, qb;
    int32_t N2 = 2 * N - 1;
    if (stereo && N == 2)
        N2--;
    /* The upper limit ensures that in a stereo split with itheta==16384, we'll
        always have enough bits left over to code at least one pulse in the
        side; otherwise it would collapse, since it doesn't get folded. */
    qb = celt_sudiv(b + N2 * offset, N2);
    qb = min(b - pulse_cap - (4 << BITRES), qb);

    qb = min((int32_t)(8 << BITRES), qb);

    if (qb < (1 << BITRES >> 1)) {
        qn = 1;
    }
    else {
        qn = exp2_table8[qb & 0x7] >> (14 - (qb >> BITRES));
        qn = (qn + 1) >> 1 << 1;
    }
    assert(qn <= 256);
    return qn;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::compute_theta(struct split_ctx *sctx, int16_t *X, int16_t *Y, int32_t N, int32_t *b, int32_t B,
                          int32_t __B0, int32_t LM, int32_t stereo, int32_t *fill) {
    int32_t qn;
    int32_t itheta = 0;
    int32_t delta;
    int32_t imid, iside;
    int32_t qalloc;
    int32_t pulse_cap;
    int32_t offset;
    int32_t tell;
    int32_t inv = 0;
    int32_t i;
    int32_t intensity;

    i = m_band_ctx.i;
    intensity = m_band_ctx.intensity;

    /* Decide on the resolution to give to the split parameter theta */
    pulse_cap = logN400[i] + LM * (1 << BITRES);
    offset = (pulse_cap >> 1) - (stereo && N == 2 ? QTHETA_OFFSET_TWOPHASE : QTHETA_OFFSET);
    qn = compute_qn(N, *b, offset, pulse_cap, stereo);
    if (stereo && i >= intensity)
        qn = 1;

    tell = rd.tell_frac(); // ->tell_frac();
    if (qn != 1) {
        /* Entropy coding of the angle. We use a uniform pdf for the time split, a step for stereo, and a triangular one for the rest. */
        if (stereo && N > 2) {
            int32_t p0 = 3;
            int32_t x = itheta;
            int32_t x0 = qn / 2;
            int32_t ft = p0 * (x0 + 1) + x0;
            /* Use a probability of p0 up to itheta=8192 and then use 1 after */
            int32_t fs;
            fs = rd.decode(ft);
            if (fs < (x0 + 1) * p0)
                x = fs / p0;
            else
                x = x0 + 1 + (fs - (x0 + 1) * p0);
            rd.dec_update( x <= x0 ? p0 * x : (x - 1 - x0) + (x0 + 1) * p0, x <= x0 ? p0 * (x + 1) : (x - x0) + (x0 + 1) * p0, ft);
            itheta = x;
        }
        else if (__B0 > 1 || stereo) {
            /* Uniform pdf */
            itheta = rd.dec_uint(qn + 1);
        }
        else {
            int32_t fs = 1, ft;
            ft = ((qn >> 1) + 1) * ((qn >> 1) + 1);
            /* Triangular pdf */
            int32_t fl = 0;
            int32_t fm;
            fm = rd.decode(ft);
            if (fm < ((qn >> 1) * ((qn >> 1) + 1) >> 1))
            {
                itheta = (isqrt32(8 * (uint32_t)fm + 1) - 1) >> 1;
                fs = itheta + 1;
                fl = itheta * (itheta + 1) >> 1;
            }
            else
            {
                itheta = (2 * (qn + 1) - isqrt32(8 * (uint32_t)(ft - fm - 1) + 1)) >> 1;
                fs = qn + 1 - itheta;
                fl = ft - ((qn + 1 - itheta) * (qn + 2 - itheta) >> 1);
            }
            rd.dec_update(fl, fl + fs, ft);
        }
        assert(itheta >= 0);
        itheta = celt_udiv((int32_t)itheta * 16384, qn);

        /* NOTE: Renormalising X and Y *may* help fixed-point a bit at very high rate.
                 Let's do that at higher complexity */
    }
    else if (stereo) {

        if (*b > 2 << BITRES && m_band_ctx.remaining_bits > 2 << BITRES) {
            inv = rd.dec_bit_logp(2);
        }
        else
            inv = 0;
        /* inv flag override to avoid problems with downmixing. */
        if (m_band_ctx.disable_inv)
            inv = 0;
        itheta = 0;
    }
    qalloc = rd.tell_frac() - tell;
    *b -= qalloc;

    if (itheta == 0) {
        imid = 32767;
        iside = 0;
        *fill &= (1 << B) - 1;
        delta = -16384;
    }
    else if (itheta == 16384){
        imid = 0;
        iside = 32767;
        *fill &= ((1 << B) - 1) << B;
        delta = 16384;
    }
    else {
        imid = bitexact_cos((int16_t)itheta);
        iside = bitexact_cos((int16_t)(16384 - itheta));
        /* This is the mid vs side allocation that minimizes squared error
           in that band. */
        delta = FRAC_MUL16((N - 1) << 7, bitexact_log2tan(iside, imid));
    }

    sctx->inv = inv;
    sctx->imid = imid;
    sctx->iside = iside;
    sctx->delta = delta;
    sctx->itheta = itheta;
    sctx->qalloc = qalloc;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t CeltDecoder::quant_band_n1(int16_t *X, int16_t *Y, int32_t b,  int16_t *lowband_out) {
    int32_t c;
    int32_t stereo;
    int16_t *x = X;

    stereo = Y != NULL;
    c = 0;
    do {
        int32_t sign = 0;
        if (m_band_ctx.remaining_bits >= 1 << BITRES) {
            sign = rd.dec_bits(1);
            m_band_ctx.remaining_bits -= 1 << BITRES;
            b -= 1 << BITRES;
        }
        if (m_band_ctx.resynth)
            x[0] = sign ? -NORM_SCALING : NORM_SCALING;
        x = Y;
    } while (++c < 1 + stereo);
    if (lowband_out)
        lowband_out[0] = SHR16(X[0], 4);
    return 1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This function is responsible for encoding and decoding a mono partition. It can split the band in two and transmit  the energy difference with the two half-bands.
   It can be called recursively so bands can end up being split in 8 parts. */
uint32_t CeltDecoder::quant_partition(int16_t *X, int32_t N, int32_t b, int32_t B, int16_t *lowband, int32_t LM,
                                int16_t gain, int32_t fill){
    const uint8_t *cache;
    int32_t q;
    int32_t curr_bits;
    int32_t imid = 0, iside = 0;
    int32_t _B0 = B;
    int16_t mid = 0, side = 0;
    uint32_t cm = 0;
    int16_t *Y = NULL;
    int32_t i;
    int32_t spread;

    i = m_band_ctx.i;
    spread = m_band_ctx.spread;

    /* If we need 1.5 more bit than we can produce, split the band in two. */
    cache = cache_bits50 + cache_index50[(LM + 1) * m_CELTMode.nbEBands + i];
    if (LM != -1 && b > cache[cache[0]] + 12 && N > 2) {
        int32_t mbits, sbits, delta;
        int32_t itheta;
        int32_t qalloc;
        struct split_ctx sctx;
        int16_t *next_lowband2 = NULL;
        int32_t rebalance;

        N >>= 1;
        Y = X + N;
        LM -= 1;
        if (B == 1)
            fill = (fill & 1) | (fill << 1);
        B = (B + 1) >> 1;

        compute_theta(&sctx, X, Y, N, &b, B, _B0, LM, 0, &fill);
        imid = sctx.imid;
        iside = sctx.iside;
        delta = sctx.delta;
        itheta = sctx.itheta;
        qalloc = sctx.qalloc;

        mid = imid;
        side = iside;

        /* Give more bits to low-energy MDCTs than they would otherwise deserve */
        if (_B0 > 1 && (itheta & 0x3fff)) {
            if (itheta > 8192)
                /* Rough approximation for pre-echo masking */
                delta -= delta >> (4 - LM);
            else
                /* Corresponds to a forward-masking slope of 1.5 dB per 10 ms */
                delta = min((int32_t)0, delta + (N << BITRES >> (5 - LM)));
        }
        mbits = max((int32_t)0, min(b, (b - delta) / 2));
        sbits = b - mbits;
        m_band_ctx.remaining_bits -= qalloc;

        if (lowband)
            next_lowband2 = lowband + N; /* >32-bit split case */

        rebalance = m_band_ctx.remaining_bits;
        if (mbits >= sbits)  {
            cm = quant_partition(X, N, mbits, B, lowband, LM,
                                 MULT16_16_P15(gain, mid), fill);
            rebalance = mbits - (rebalance - m_band_ctx.remaining_bits);
            if (rebalance > 3 << BITRES && itheta != 0)
                sbits += rebalance - (3 << BITRES);
            cm |= quant_partition(Y, N, sbits, B, next_lowband2, LM,
                                  MULT16_16_P15(gain, side), fill >> B)
                  << (_B0 >> 1);
        }
        else {
            cm = quant_partition(Y, N, sbits, B, next_lowband2, LM,
                                 MULT16_16_P15(gain, side), fill >> B)
                 << (_B0 >> 1);
            rebalance = sbits - (rebalance - m_band_ctx.remaining_bits);
            if (rebalance > 3 << BITRES && itheta != 16384)
                mbits += rebalance - (3 << BITRES);
            cm |= quant_partition(X, N, mbits, B, lowband, LM,
                                  MULT16_16_P15(gain, mid), fill);
        }
    }
    else {
        /* This is the basic no-split case */
        q = bits2pulses(i, LM, b);
        curr_bits = pulses2bits(i, LM, q);
        m_band_ctx.remaining_bits -= curr_bits;

        /* Ensures we can never bust the budget */
        while (m_band_ctx.remaining_bits < 0 && q > 0) {
            m_band_ctx.remaining_bits += curr_bits;
            q--;
            curr_bits = pulses2bits(i, LM, q);
            m_band_ctx.remaining_bits -= curr_bits;
        }

        if (q != 0) {
            int32_t K = get_pulses(q);

            /* Finally do the actual quantization */
            cm = alg_unquant(X, N, K, spread, B, gain);
        }
        else {
            /* If there's no pulse, fill the band anyway */
            int32_t j;
            if (m_band_ctx.resynth)
            {
                uint32_t cm_mask;
                /* B can be as large as 16, so this shift might overflow an int32_t on a 16-bit platform; use a long to get defined behavior.*/
                cm_mask = (uint32_t)(1UL << B) - 1;
                fill &= cm_mask;
                if (!fill) {
                    OPUS_CLEAR(X, N);
                }
                else  {
                    if (lowband == NULL) {
                        /* Noise */
                        for (j = 0; j < N; j++) {
                            m_band_ctx.seed = celt_lcg_rand(m_band_ctx.seed);
                            X[j] = (int16_t)((int32_t)m_band_ctx.seed >> 20);
                        }
                        cm = cm_mask;
                    }
                    else {
                        /* Folded spectrum */
                        for (j = 0; j < N; j++) {
                            int16_t tmp;
                            m_band_ctx.seed = celt_lcg_rand(m_band_ctx.seed);
                            /* About 48 dB below the "normal" folding level */
                            tmp = QCONST16(1.0f / 256, 10);
                            tmp = (m_band_ctx.seed) & 0x8000 ? tmp : -tmp;
                            X[j] = lowband[j] + tmp;
                        }
                        cm = fill;
                    }
                    renormalise_vector(X, N, gain);
                }
            }
        }
    }

    return cm;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This function is responsible for encoding and decoding a band for the mono case. */
uint32_t CeltDecoder::quant_band(int16_t *X, int32_t N, int32_t b, int32_t B, int16_t *lowband, int32_t LM,
                           int16_t *lowband_out, int16_t gain, int16_t *lowband_scratch, int32_t fill) {
    int32_t N0 = N;
    int32_t N_B = N;
    int32_t N__B0;
    int32_t _B0 = B;
    int32_t time_divide = 0;
    int32_t recombine = 0;
    int32_t longBlocks;
    uint32_t cm = 0;
    int32_t k;
    int32_t tf_change;

    tf_change = m_band_ctx.tf_change;

    longBlocks = _B0 == 1;

    N_B = celt_udiv(N_B, B);

    /* Special case for one sample */
    if (N == 1) {
        return quant_band_n1(X, NULL, b, lowband_out);
    }

    if (tf_change > 0)
        recombine = tf_change;
    /* Band recombining to increase frequency resolution */

    if (lowband_scratch && lowband && (recombine || ((N_B & 1) == 0 && tf_change < 0) || _B0 > 1)) {
        memcpy(lowband_scratch, lowband, N * sizeof(*lowband_scratch));
        lowband = lowband_scratch;
    }

    for (k = 0; k < recombine; k++) {
        const uint8_t bit_interleave_table[16] = {
            0, 1, 1, 1, 2, 3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3};
        if (lowband)
            haar1(lowband, N >> k, 1 << k);
        fill = bit_interleave_table[fill & 0xF] | bit_interleave_table[fill >> 4] << 2;
    }
    B >>= recombine;
    N_B <<= recombine;

    /* Increasing the time resolution */
    while ((N_B & 1) == 0 && tf_change < 0) {
        if (lowband)
            haar1(lowband, N_B, B);
        fill |= fill << B;
        B <<= 1;
        N_B >>= 1;
        time_divide++;
        tf_change++;
    }
    _B0 = B;
    N__B0 = N_B;

    /* Reorganize the samples in time order instead of frequency order */
    if (_B0 > 1) {
        if (lowband)
            deinterleave_hadamard(lowband, N_B >> recombine, _B0 << recombine, longBlocks);
    }

    cm = quant_partition(X, N, b, B, lowband, LM, gain, fill);

    if (m_band_ctx.resynth) {
        /* Undo the sample reorganization going from time order to frequency order */
        if (_B0 > 1)
            interleave_hadamard(X, N_B >> recombine, _B0 << recombine, longBlocks);

        /* Undo time-freq changes that we did earlier */
        N_B = N__B0;
        B = _B0;
        for (k = 0; k < time_divide; k++) {
            B >>= 1;
            N_B <<= 1;
            cm |= cm >> B;
            haar1(X, N_B, B);
        }

        for (k = 0; k < recombine; k++) {
            const uint8_t bit_deinterleave_table[16] = {
                0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
                0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF};
            cm = bit_deinterleave_table[cm];
            haar1(X, N0 >> k, 1 << k);
        }
        B <<= recombine;

        /* Scale output for later folding */
        if (lowband_out) {
            int32_t j;
            int16_t n;
            n = celt_sqrt(SHL32(EXTEND32(N0), 22));
            for (j = 0; j < N0; j++)
                lowband_out[j] = MULT16_16_Q15(n, X[j]);
        }
        cm &= (1 << B) - 1;
    }
    return cm;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This function is responsible for encoding and decoding a band for the stereo case. */
uint32_t CeltDecoder::quant_band_stereo(int16_t *X, int16_t *Y, int32_t N, int32_t b, int32_t B, int16_t *lowband,
                                  int32_t LM, int16_t *lowband_out, int16_t *lowband_scratch, int32_t fill) {
    int32_t imid = 0, iside = 0;
    int32_t inv = 0;
    int16_t mid = 0, side = 0;
    uint32_t cm = 0;
    int32_t mbits, sbits, delta;
    int32_t itheta;
    int32_t qalloc;
    struct split_ctx sctx;
    int32_t orig_fill;

    /* Special case for one sample */
    if (N == 1){
        return quant_band_n1(X, Y, b, lowband_out);
    }

    orig_fill = fill;

    compute_theta(&sctx, X, Y, N, &b, B, B, LM, 1, &fill);
    inv = sctx.inv;
    imid = sctx.imid;
    iside = sctx.iside;
    delta = sctx.delta;
    itheta = sctx.itheta;
    qalloc = sctx.qalloc;

    mid = imid;
    side = iside;

    /* This is a special case for N=2 that only works for stereo and takes advantage of the fact that mid and side are orthogonal to encode the side with just one bit. */
    if (N == 2) {
        int32_t c;
        int32_t sign = 0;
        int16_t *x2, *y2;
        mbits = b;
        sbits = 0;
        /* Only need one bit for the side. */
        if (itheta != 0 && itheta != 16384)
            sbits = 1 << BITRES;
        mbits -= sbits;
        c = itheta > 8192;
        m_band_ctx.remaining_bits -= qalloc + sbits;

        x2 = c ? Y : X;
        y2 = c ? X : Y;
        if (sbits) {
            sign = rd.dec_bits(1);
        }
        sign = 1 - 2 * sign;
        /* We use orig_fill here because we want to fold the side, but if itheta==16384, we'll have cleared the low bits of fill. */
        cm = quant_band(x2, N, mbits, B, lowband, LM, lowband_out, 32767,
                        lowband_scratch, orig_fill);
        /* We don't split N=2 bands, so cm is either 1 or 0 (for a fold-collapse), and there's no need to worry about mixing with the other channel. */
        y2[0] = -sign * x2[1];
        y2[1] = sign * x2[0];
        if (m_band_ctx.resynth) {
            int16_t tmp;
            X[0] = MULT16_16_Q15(mid, X[0]);
            X[1] = MULT16_16_Q15(mid, X[1]);
            Y[0] = MULT16_16_Q15(side, Y[0]);
            Y[1] = MULT16_16_Q15(side, Y[1]);
            tmp = X[0];
            X[0] = SUB16(tmp, Y[0]);
            Y[0] = ADD16(tmp, Y[0]);
            tmp = X[1];
            X[1] = SUB16(tmp, Y[1]);
            Y[1] = ADD16(tmp, Y[1]);
        }
    }
    else {
        /* "Normal" split code */
        int32_t rebalance;

        mbits = max((int32_t)0, min(b, (b - delta) / 2));
        sbits = b - mbits;
        m_band_ctx.remaining_bits -= qalloc;

        rebalance = m_band_ctx.remaining_bits;
        if (mbits >= sbits) {
            /* In stereo mode, we do not apply a scaling to the mid because we need the normalized mid for folding later. */
            cm = quant_band(X, N, mbits, B, lowband, LM, lowband_out, 32767,
                            lowband_scratch, fill);
            rebalance = mbits - (rebalance - m_band_ctx.remaining_bits);
            if (rebalance > 3 << BITRES && itheta != 0)
                sbits += rebalance - (3 << BITRES);

            /* For a stereo split, the high bits of fill are always zero, so no
               folding will be done to the side. */
            cm |= quant_band(Y, N, sbits, B, NULL, LM, NULL, side, NULL, fill >> B);
        }
        else {
            /* For a stereo split, the high bits of fill are always zero, so no folding will be done to the side. */
            cm = quant_band(Y, N, sbits, B, NULL, LM, NULL, side, NULL, fill >> B);
            rebalance = sbits - (rebalance - m_band_ctx.remaining_bits);
            if (rebalance > 3 << BITRES && itheta != 16384)
                mbits += rebalance - (3 << BITRES);
            /* In stereo mode, we do not apply a scaling to the mid because we need the normalized mid for folding later. */
            cm |= quant_band(X, N, mbits, B, lowband, LM, lowband_out, 32767,
                             lowband_scratch, fill);
        }
    }
    if (m_band_ctx.resynth) {
        if (N != 2)
            stereo_merge(X, Y, mid, N);
        if (inv)
        {
            int32_t j;
            for (j = 0; j < N; j++)
                Y[j] = -Y[j];
        }
    }
    return cm;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::special_hybrid_folding(int16_t *norm, int16_t *norm2, int32_t start, int32_t M, int32_t dual_stereo){
    int32_t n1, n2;
    const int16_t * eBands = eband5ms;
    n1 = M * (eBands[start + 1] - eBands[start]);
    n2 = M * (eBands[start + 2] - eBands[start + 1]);
    /* Duplicate enough of the first band folding data to be able to fold the second band. Copies no data for CELT-only mode. */
    memcpy(&norm[n1], &norm[2 * n1 - n2], (n2 - n1) * sizeof(*norm));
    if (dual_stereo)
        memcpy(&norm2[n1], &norm2[2 * n1 - n2], (n2 - n1) * sizeof(*norm2));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::quant_all_bands(int32_t start, int32_t end, int16_t *X_, int16_t *Y_, uint8_t *collapse_masks, const int32_t *bandE, int32_t *pulses, int32_t shortBlocks, int32_t spread, int32_t dual_stereo,
                     int32_t intensity, int32_t *tf_res, int32_t total_bits, int32_t balance,  int32_t LM, int32_t codedBands, uint32_t *seed, int32_t complexity, int32_t disable_inv){
    int32_t i;
    int32_t remaining_bits;
    const int16_t * eBands = eband5ms;
    int16_t * norm, * norm2;

    int32_t resynth_alloc;
    int16_t *lowband_scratch;
    int32_t B;
    int32_t M;
    int32_t lowband_offset;
    int32_t update_lowband = 1;
    int32_t C = Y_ != NULL ? 2 : 1;
    int32_t norm_offset;
    int32_t resynth = 1;

    M = 1 << LM;
    B = shortBlocks ? M : 1;
    norm_offset = M * eBands[start];
    /* No need to allocate norm for the last band because we don't need an
       output in that band. */
    ps_ptr<int16_t> _norm; _norm.alloc_array(C * (M * eBands[m_CELTMode.nbEBands - 1] - norm_offset));
    norm = _norm.get();
    norm2 = norm + M * eBands[m_CELTMode.nbEBands - 1] - norm_offset;

    /* For decoding, we can use the last band as scratch space because we don't need that scratch space for the last band and we don't care about the data there until we're
       decoding the last band. */
    resynth_alloc = ALLOC_NONE;

    ps_ptr<int16_t>_lowband_scratch;  _lowband_scratch.alloc_array(resynth_alloc);
    lowband_scratch = X_ + M * eBands[m_CELTMode.nbEBands - 1];

    ps_ptr<int16_t>X_save;      X_save.alloc_array(resynth_alloc);
    ps_ptr<int16_t>Y_save;      Y_save.alloc_array(resynth_alloc);
    ps_ptr<int16_t>X_save2;     X_save2.alloc_array(resynth_alloc);
    ps_ptr<int16_t>Y_save2;     Y_save2.alloc_array(resynth_alloc);
    ps_ptr<int16_t>norm_save2;  norm_save2.alloc_array(resynth_alloc);

    lowband_offset = 0;
    m_band_ctx.bandE = bandE;
    m_band_ctx.intensity = intensity;
    m_band_ctx.seed = *seed;
    m_band_ctx.spread = spread;
    m_band_ctx.disable_inv = disable_inv;
    m_band_ctx.resynth = resynth;
    m_band_ctx.theta_round = 0;
    /* Avoid injecting noise in the first band on transients. */
    m_band_ctx.avoid_split_noise = B > 1;
    for (i = start; i < end; i++){
        int32_t tell;
        int32_t b;
        int32_t N;
        int32_t curr_balance;
        int32_t effective_lowband = -1;
        int16_t * X, * Y;
        int32_t tf_change = 0;
        uint32_t x_cm;
        uint32_t y_cm;
        int32_t last;

        m_band_ctx.i = i;
        last = (i == end - 1);

        X = X_ + M * eBands[i];
        if (Y_ != NULL)
            Y = Y_ + M * eBands[i];
        else
            Y = NULL;
        N = M * eBands[i + 1] - M * eBands[i];
        assert(N > 0);
        tell = rd.tell_frac();

        /* Compute how many bits we want to allocate to this band */
        if (i != start)
            balance -= tell;
        remaining_bits = total_bits - tell - 1;
        m_band_ctx.remaining_bits = remaining_bits;
        if (i <= codedBands - 1){
            curr_balance = celt_sudiv(balance, min((int32_t)3, codedBands - i));
            b = max((int32_t)0, min((int32_t)16383, min(remaining_bits + (int32_t)1, (int32_t)pulses[i] + curr_balance)));
        }
        else {
            b = 0;
        }

        if (resynth && (M * eBands[i] - N >= M * eBands[start] || i == start + 1) && (update_lowband || lowband_offset == 0))
            lowband_offset = i;
        if (i == start + 1)
            special_hybrid_folding(norm, norm2, start, M, dual_stereo);

        tf_change = tf_res[i];
        m_band_ctx.tf_change = tf_change;
        if (i >= m_CELTMode.effEBands) {
            X = norm;
            if (Y_ != NULL)
                Y = norm;
            lowband_scratch = NULL;
        }
        if (last)
            lowband_scratch = NULL;

        /* Get a conservative estimate of the collapse_mask's for the bands we're going to be folding from. */
        if (lowband_offset != 0 && (spread != SPREAD_AGGRESSIVE || B > 1 || tf_change < 0)) {
            int32_t fold_start;
            int32_t fold_end;
            int32_t fold_i;
            /* This ensures we never repeat spectral content within one band */
            effective_lowband = max((int32_t)0, M * eBands[lowband_offset] - norm_offset - N);
            fold_start = lowband_offset;
            while (M * eBands[--fold_start] > effective_lowband + norm_offset)
                ;
            fold_end = lowband_offset - 1;

            while (++fold_end < i && M * eBands[fold_end] < effective_lowband + norm_offset + N)
                ;

            x_cm = y_cm = 0;
            fold_i = fold_start;
            do {
                x_cm |= collapse_masks[fold_i * C + 0];
                y_cm |= collapse_masks[fold_i * C + C - 1];
            } while (++fold_i < fold_end);
        }
        /* Otherwise, we'll be using the LCG to fold, so all blocks will (almost always) be non-zero. */
        else
            x_cm = y_cm = (1 << B) - 1;

        if (dual_stereo && i == intensity) {
            int32_t j;

            /* Switch off dual stereo to do intensity. */
            dual_stereo = 0;
            if (resynth)
                for (j = 0; j < M * eBands[i] - norm_offset; j++)
                    norm[j] = HALF32(norm[j] + norm2[j]);
        }
        if (dual_stereo) {
            x_cm = quant_band(X, N, b / 2, B,
                              effective_lowband != -1 ? norm + effective_lowband : NULL, LM,
                              last ? NULL : norm + M * eBands[i] - norm_offset, 32767, lowband_scratch, x_cm);
            y_cm = quant_band(Y, N, b / 2, B,
                              effective_lowband != -1 ? norm2 + effective_lowband : NULL, LM,
                              last ? NULL : norm2 + M * eBands[i] - norm_offset, 32767, lowband_scratch, y_cm);
        }
        else {
            if (Y != NULL) {
                m_band_ctx.theta_round = 0;
                x_cm = quant_band_stereo(X, Y, N, b, B,
                    effective_lowband != -1 ? norm + effective_lowband : NULL, LM,
                    last ? NULL : norm + M * eBands[i] - norm_offset, lowband_scratch, x_cm | y_cm);
            }
            else {
                x_cm = quant_band(X, N, b, B,
                                  effective_lowband != -1 ? norm + effective_lowband : NULL, LM,
                                  last ? NULL : norm + M * eBands[i] - norm_offset, 32767, lowband_scratch, x_cm | y_cm);
            }
            y_cm = x_cm;
        }
        collapse_masks[i * C + 0] = (uint8_t)x_cm;
        collapse_masks[i * C + C - 1] = (uint8_t)y_cm;
        balance += pulses[i] + tell;

        /* Update the folding position only as long as we have 1 bit/sample depth. */
        update_lowband = b > (N << BITRES);
        /* We only need to avoid noise on a split for the first band. After that, we
           have folding. */
        m_band_ctx.avoid_split_noise = 0;
    }
    *seed = m_band_ctx.seed;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_decoder_get_size(int32_t channels){
    int32_t size = (channels * (DECODE_BUFFER_SIZE + m_CELTMode.overlap) - 1) * sizeof(int32_t)
           + channels * 24 * sizeof(int16_t) + 4 * 2 * m_CELTMode.nbEBands * sizeof(int16_t);
    return size;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_decoder_init(int32_t channels){

    m_celtDec.downsample = 1; //resampling_factor(Fs);
    m_celtDec.channels = channels;
    if(channels == 1) m_celtDec.disable_inv = 1; else m_celtDec.disable_inv = 0; // 1 mono ,  0 stereo
    m_celtDec.end = m_CELTMode.nbEBands; // 21
    m_celtDec.error = 0;
    m_celtDec.overlap = m_CELTMode.overlap;
    m_celtDec.postfilter_gain = 0;
    m_celtDec.postfilter_gain_old = 0;
    m_celtDec.postfilter_period = 0;
    m_celtDec.postfilter_tapset = 0;
    m_celtDec.postfilter_tapset_old = 0;
    m_celtDec.preemph_memD[0] = 0;
    m_celtDec.preemph_memD[1] = 0;
    m_celtDec.rng = 0;
    m_celtDec.signalling = 1;
    m_celtDec.start = 0;
    m_celtDec.end = m_CELTMode.effEBands;
    m_celtDec.stream_channels = channels;
    int32_t ret = celt_decoder_ctl(OPUS_RESET_STATE);
    if(ret < 0) return ret;

    return OPUS_OK;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Special case for stereo with no downsampling and no accumulation. This is quite common and we can make it faster by  processing both channels in the same loop, reducing overhead due to the
   dependency loop in the IIR filter. */
void CeltDecoder::deemphasis_stereo_simple(int32_t *in[], int16_t *pcm, int32_t N, const int16_t coef0, int32_t *mem) {
    int32_t * x0;
    int32_t * x1;
    int32_t m0, m1;
    int32_t j;
    x0 = in[0];
    x1 = in[1];
    m0 = mem[0];
    m1 = mem[1];
    for (j = 0; j < N; j++) {
        int32_t tmp0, tmp1;
        /* Add VERY_SMALL to x[] first to reduce dependency chain. */
        tmp0 = x0[j] + VERY_SMALL + m0;
        tmp1 = x1[j] + VERY_SMALL + m1;
        m0 = MULT16_32_Q15(coef0, tmp0);
        m1 = MULT16_32_Q15(coef0, tmp1);
        pcm[2 * j] = SCALEOUT(sig2word16(tmp0));
        pcm[2 * j + 1] = SCALEOUT(sig2word16(tmp1));
    }
    mem[0] = m0;
    mem[1] = m1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::deemphasis(int32_t *in[], int16_t *pcm, int32_t N, int32_t C, int32_t downsample, const int16_t *coef,
               int32_t *mem, int32_t accum) {
    int32_t c;
    int32_t Nd;
    int32_t apply_downsampling = 0;
    int16_t coef0;

    /* Short version for common case. */
    if (downsample == 1 && C == 2 && !accum) {
        deemphasis_stereo_simple(in, pcm, N, coef[0], mem);
        return;
    }

    ps_ptr<int32_t>scratch; scratch.alloc_array(N);
    coef0 = coef[0];
    Nd = N / downsample;
    c = 0;
    do  {
        int32_t j;
        int32_t * x;
        int16_t * y;
        int32_t m = mem[c];
        x = in[c];
        y = pcm + c;

        if (downsample > 1) {
            /* Shortcut for the standard (non-custom modes) case */
            for (j = 0; j < N; j++) {
                int32_t tmp = x[j] + VERY_SMALL + m;
                m = MULT16_32_Q15(coef0, tmp);
                scratch[j] = tmp;
            }
            apply_downsampling = 1;
        }
        else {
            /* Shortcut for the standard (non-custom modes) case */

            if (accum) {
                for (j = 0; j < N; j++) {
                    int32_t tmp = x[j] + m + VERY_SMALL;
                    m = MULT16_32_Q15(coef0, tmp);
                    y[j * C] = SAT16(ADD32(y[j * C], SCALEOUT(sig2word16(tmp))));
                }
            }
            else {
                for (j = 0; j < N; j++) {
                    int32_t tmp = x[j] + VERY_SMALL + m;
                    m = MULT16_32_Q15(coef0, tmp);
                    y[j * C] = SCALEOUT(sig2word16(tmp));
                }
            }
        }
        mem[c] = m;

        if (apply_downsampling) {
            /* Perform down-sampling */

            if (accum)  {
                for (j = 0; j < Nd; j++)
                    y[j * C] = SAT16(ADD32(y[j * C], SCALEOUT(sig2word16(scratch[j * downsample]))));
            }
            else {
                for (j = 0; j < Nd; j++)
                    y[j * C] = SCALEOUT(sig2word16(scratch[j * downsample]));
            }
        }
    } while (++c < C);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::celt_synthesis(int16_t *X, int32_t *out_syn[], int16_t *oldBandE, int32_t start,
                           int32_t effEnd, int32_t C, int32_t CC, int32_t isTransient, int32_t LM, int32_t downsample, int32_t silence){
    int32_t c, i;
    int32_t M;
    int32_t b;
    int32_t B;
    int32_t N, NB;
    int32_t shift;
    int32_t nbEBands;
    int32_t overlap;

    overlap =  m_CELTMode.overlap;
    nbEBands = m_CELTMode.nbEBands;
    N = m_CELTMode.shortMdctSize << LM;
    ps_ptr<int32_t>freq; freq.alloc_array(N); /**< Interleaved signal MDCTs */
    M = 1 << LM;

    if (isTransient) {
        B = M;
        NB = m_CELTMode.shortMdctSize;
        shift = m_CELTMode.maxLM;
    }
    else {
        B = 1;
        NB = m_CELTMode.shortMdctSize << LM;
        shift = m_CELTMode.maxLM - LM;
    }

    if (CC == 2 && C == 1) {
        /* Copying a mono streams to two channels */
        int32_t *freq2;
        denormalise_bands(X, freq.get(), oldBandE, start, effEnd, M,
                          downsample, silence);
        /* Store a temporary copy in the output buffer because the IMDCT destroys its input. */
        freq2 = out_syn[1] + overlap / 2;
        memcpy(freq2, freq.get(), N * sizeof(*freq2));
        for(b = 0; b < B; b++) clt_mdct_backward(&freq2[b], out_syn[0] + NB * b, overlap, shift, B);
        for(b = 0; b < B; b++) clt_mdct_backward(&freq[b], out_syn[1] + NB * b, overlap, shift, B);
    }
    else if (CC == 1 && C == 2) {
        /* Downmixing a stereo stream to mono */
        int32_t *freq2;
        freq2 = out_syn[0] + overlap / 2;
        denormalise_bands(X, freq.get(), oldBandE, start, effEnd, M,
                          downsample, silence);
        /* Use the output buffer as temp array before downmixing. */
        denormalise_bands(X + N, freq2, oldBandE + nbEBands, start, effEnd, M,
                          downsample, silence);
        for (i = 0; i < N; i++)
            freq[i] = ADD32(HALF32(freq[i]), HALF32(freq2[i]));
        for (b = 0; b < B; b++)
            for(b = 0; b < B; b++) clt_mdct_backward(&freq[b], out_syn[0] + NB * b, overlap, shift, B);
    }
    else {
        /* Normal case (mono or stereo) */
        c = 0;
        do {
            denormalise_bands(X + c * N, freq.get(), oldBandE + c * nbEBands, start, effEnd, M,
                              downsample, silence);
            for (b = 0; b < B; b++)
                for(b = 0; b < B; b++) clt_mdct_backward(&freq[b], out_syn[c] + NB * b, overlap, shift, B);
        } while (++c < CC);
    }
    /* Saturate IMDCT output so that we can't overflow in the pitch postfilter or in the */
    c = 0;
    do {
        for (i = 0; i < N; i++)
            out_syn[c][i] = SATURATE(out_syn[c][i], SIG_SAT);
    } while (++c < CC);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::tf_decode(int32_t start, int32_t end, int32_t isTransient, int32_t *tf_res, int32_t LM){
    int32_t i, curr, tf_select;
    int32_t tf_select_rsv;
    int32_t tf_changed;
    int32_t logp;
    uint32_t budget;
    uint32_t tell;

    budget = rd.get_storage() * 8;
    tell = rd.tell();
    logp = isTransient ? 2 : 4;
    tf_select_rsv = LM > 0 && tell + logp + 1 <= budget;
    budget -= tf_select_rsv;
    tf_changed = curr = 0;
    for (i = start; i < end; i++) {
        if (tell + logp <= budget) {
            curr ^= rd.dec_bit_logp(logp);
            tell = rd.tell();
            tf_changed |= curr;
        }
        tf_res[i] = curr;
        logp = isTransient ? 4 : 5;
    }
    tf_select = 0;
    if (tf_select_rsv &&
        tf_select_table[LM][4 * isTransient + 0 + tf_changed] !=
            tf_select_table[LM][4 * isTransient + 2 + tf_changed]) {
        tf_select = rd.dec_bit_logp(1);
    }
    for (i = start; i < end; i++) {
        tf_res[i] = tf_select_table[LM][4 * isTransient + 2 * tf_select + tf_res[i]];
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_decode_with_ec(int16_t * outbuf, int32_t frame_size) {
    int32_t c, i, N;
    int32_t spread_decision;
    int32_t bits;
    int32_t *decode_mem[2];
    int32_t *out_syn[2];
    int16_t *lpc;
    int16_t *oldBandE, *oldLogE, *oldLogE2, *backgroundLogE;

    int32_t shortBlocks;
    int32_t isTransient;
    int32_t intra_ener;
    const int32_t CC = m_celtDec.channels;
    int32_t LM, M;
    int32_t start;
    int32_t end;
    int32_t effEnd;
    int32_t codedBands;
    int32_t alloc_trim;
    int32_t postfilter_pitch;
    int16_t postfilter_gain;
    int32_t intensity = 0;
    int32_t dual_stereo = 0;
    int32_t total_bits;
    int32_t balance;
    int32_t tell;
    int32_t dynalloc_logp;
    int32_t postfilter_tapset;
    int32_t anti_collapse_rsv;
    int32_t anti_collapse_on = 0;
    int32_t silence;
    const uint8_t  C = m_celtDec.stream_channels; // =channels=2
    const uint8_t  nbEBands = m_CELTMode.nbEBands; // =21
    const uint8_t  overlap = m_CELTMode.overlap; // =120
    const int16_t *eBands = eband5ms;

    start = m_celtDec.start;
    end = m_CELTMode.effEBands;
    frame_size *= m_celtDec.downsample;

    lpc = (int16_t *)(m_decode_mem.get() + (DECODE_BUFFER_SIZE + overlap) * CC);
    oldBandE = lpc + CC * LPC_ORDER;
    oldLogE = oldBandE + 2 * nbEBands;
    oldLogE2 = oldLogE + 2 * nbEBands;
    backgroundLogE = oldLogE2 + 2 * nbEBands;

    {
        for (LM = 0; LM <= m_CELTMode.maxLM; LM++)
            if (m_CELTMode.shortMdctSize << LM == frame_size) break;
        if (LM > m_CELTMode.maxLM) {OPUS_LOG_ERROR("Opus Celt bas arg"); return OPUS_BAD_ARG;}
    }

    M = 1 << LM;

    if(rd.get_storage() > 1275 || outbuf == NULL) {OPUS_LOG_ERROR("Opus Celt bad arg"); return OPUS_BAD_ARG;}

    N = M * m_CELTMode.shortMdctSize;
    c = 0;
    do {
        decode_mem[c] = (int32_t*)(m_decode_mem.get() + c * (DECODE_BUFFER_SIZE + overlap));  // todo
        out_syn[c] = decode_mem[c] + DECODE_BUFFER_SIZE - N;
    } while (++c < CC);

    if(rd.get_storage() <= 1) {OPUS_LOG_ERROR("Opus Celt bas arg"); return OPUS_BAD_ARG;}

    effEnd = end;
    if (effEnd > m_CELTMode.effEBands)
        effEnd = m_CELTMode.effEBands;

    /* Check if there are at least two packets received consecutively before turning on the pitch-based PLC */
    m_celtDec.skip_plc = m_celtDec.loss_count != 0;

    if (C == 1) {
        for (i = 0; i < nbEBands; i++)
            oldBandE[i] = max(oldBandE[i], oldBandE[nbEBands + i]);
    }

    total_bits = rd.get_storage() * 8;
    tell = rd.tell();

    if (tell >= total_bits)
        silence = 1;
    else if (tell == 1)
        silence = rd.dec_bit_logp(15);
    else
        silence = 0;
    if (silence)  {
        /* Pretend we've read all the remaining bits */
        tell = rd.get_storage() * 8;
        rd.add_nbits_total(tell - rd.tell());
    }

    postfilter_gain = 0;
    postfilter_pitch = 0;
    postfilter_tapset = 0;
    if (start == 0 && tell + 16 <= total_bits) {
        if (rd.dec_bit_logp(1))
        {
            int32_t qg, octave;
            octave = rd.dec_uint(6);
            postfilter_pitch = (16 << octave) + rd.dec_bits(4 + octave) - 1;
            qg = rd.dec_bits(3);
            if (rd.tell() + 2 <= total_bits)
                postfilter_tapset = rd.dec_icdf(tapset_icdf, 2);
            postfilter_gain = QCONST16(.09375f, 15) * (qg + 1);
        }
        tell = rd.tell();
    }

    if (LM > 0 && tell + 3 <= total_bits) {
        isTransient = rd.dec_bit_logp( 3);
        tell = rd.tell();
    }
    else
        isTransient = 0;

    if (isTransient)
        shortBlocks = M;
    else
        shortBlocks = 0;

    /* Decode the global flags (first symbols in the stream) */
    intra_ener = tell + 3 <= total_bits ? rd.dec_bit_logp(3) : 0;
    /* Get band energies */
    unquant_coarse_energy(start, end, oldBandE, intra_ener, C, LM);

    ps_ptr<int32_t>tf_res; tf_res.alloc_array(nbEBands);
    tf_decode(start, end, isTransient, tf_res.get(), LM);

    tell = rd.tell();
    spread_decision = SPREAD_NORMAL;
    if (tell + 4 <= total_bits) spread_decision = rd.dec_icdf(spread_icdf, 5);

    ps_ptr<int32_t>cap; cap.alloc_array(nbEBands);

    init_caps(cap.get(), LM, C);

    ps_ptr<int32_t>offsets; offsets.alloc_array(nbEBands);

    dynalloc_logp = 6;
    total_bits <<= BITRES;
    tell = rd.tell_frac();
    for (i = start; i < end; i++) {
        int32_t width, quanta;
        int32_t dynalloc_loop_logp;
        int32_t boost;
        width = C * (eBands[i + 1] - eBands[i]) << LM;
        /* quanta is 6 bits, but no more than 1 bit/sample and no less than 1/8 bit/sample */
        quanta = min(width << BITRES, max((int32_t)(6 << BITRES), width));
        dynalloc_loop_logp = dynalloc_logp;
        boost = 0;
        while (tell + (dynalloc_loop_logp << BITRES) < total_bits && boost < cap[i])
        {
            int32_t flag;
            flag = rd.dec_bit_logp(dynalloc_loop_logp);
            tell = rd.tell_frac();
            if (!flag)
                break;
            boost += quanta;
            total_bits -= quanta;
            dynalloc_loop_logp = 1;
        }
        offsets[i] = boost;
        /* Making dynalloc more likely */
        if (boost > 0)
            dynalloc_logp = max((int32_t)2, dynalloc_logp - 1);
    }

    ps_ptr<int32_t>fine_quant; fine_quant.alloc_array(nbEBands);

    alloc_trim = tell + (6 << BITRES) <= total_bits ? rd.dec_icdf(trim_icdf, 7) : 5;

    bits = (((int32_t)rd.get_storage() * 8) << BITRES) - rd.tell_frac() - 1;
    anti_collapse_rsv = isTransient && LM >= 2 && bits >= ((LM + 2) << BITRES) ? (1 << BITRES) : 0;
    bits -= anti_collapse_rsv;

    ps_ptr<int32_t>pulses; pulses.alloc_array(nbEBands);
    ps_ptr<int32_t>fine_priority; fine_priority.alloc_array(nbEBands);

    codedBands = clt_compute_allocation(start, end, offsets.get(), cap.get(),
                                        alloc_trim, &intensity, &dual_stereo, bits, &balance, pulses.get(),
                                        fine_quant.get(), fine_priority.get(), C, LM, 0, 0, 0);

    unquant_fine_energy(start, end, oldBandE, fine_quant.get(), C);

    c = 0;
    do {
        OPUS_MOVE(decode_mem[c], decode_mem[c] + N, DECODE_BUFFER_SIZE - N + overlap / 2);
    } while (++c < CC);

    /* Decode fixed codebook */
    ps_ptr<uint8_t>collapse_masks; collapse_masks.alloc_array(C * nbEBands);

    ps_ptr<int16_t>X; X.alloc_array(C * N); /**< Interleaved normalised MDCTs */

    quant_all_bands(start, end, X.get(), C == 2 ? X.get() + N : NULL, collapse_masks.get(),
                    NULL, pulses.get(), shortBlocks, spread_decision, dual_stereo, intensity, tf_res.get(),
                    rd.get_storage() * (8 << BITRES) - anti_collapse_rsv, balance, LM, codedBands, &m_celtDec.rng, 0,
                    m_celtDec.disable_inv);

    if (anti_collapse_rsv > 0) {
        anti_collapse_on = rd.dec_bits(1);
    }

    unquant_energy_finalise(start, end, oldBandE,
                            fine_quant.get(), fine_priority.get(), rd.get_storage() * 8 - rd.tell(), C);

    if (anti_collapse_on)
        anti_collapse(X.get(), collapse_masks.get(), LM, C, N,
                      start, end, oldBandE, oldLogE, oldLogE2, pulses.get(), m_celtDec.rng);

    if (silence) {
        for (i = 0; i < C * nbEBands; i++)
            oldBandE[i] = -QCONST16(28.f, DB_SHIFT);
    }

    celt_synthesis(X.get(), out_syn, oldBandE, start, effEnd,
                   C, CC, isTransient, LM, m_celtDec.downsample, silence);

    c = 0;
    do  {
        m_celtDec.postfilter_period = max(m_celtDec.postfilter_period, (int32_t)COMBFILTER_MINPERIOD);
        m_celtDec.postfilter_period_old = max(m_celtDec.postfilter_period_old, (int32_t)COMBFILTER_MINPERIOD);
        comb_filter(out_syn[c], out_syn[c], m_celtDec.postfilter_period_old, m_celtDec.postfilter_period,m_CELTMode.shortMdctSize,
                    m_celtDec.postfilter_gain_old, m_celtDec.postfilter_gain, m_celtDec.postfilter_tapset_old, m_celtDec.postfilter_tapset);
        if (LM != 0)
            comb_filter(out_syn[c] + m_CELTMode.shortMdctSize, out_syn[c] + m_CELTMode.shortMdctSize, m_celtDec.postfilter_period, postfilter_pitch, N - m_CELTMode.shortMdctSize,
                        m_celtDec.postfilter_gain, postfilter_gain, m_celtDec.postfilter_tapset, postfilter_tapset);

    } while (++c < CC);
    m_celtDec.postfilter_period_old = m_celtDec.postfilter_period;
    m_celtDec.postfilter_gain_old = m_celtDec.postfilter_gain;
    m_celtDec.postfilter_tapset_old = m_celtDec.postfilter_tapset;
    m_celtDec.postfilter_period = postfilter_pitch;
    m_celtDec.postfilter_gain = postfilter_gain;
    m_celtDec.postfilter_tapset = postfilter_tapset;
    if (LM != 0) {
        m_celtDec.postfilter_period_old = m_celtDec.postfilter_period;
        m_celtDec.postfilter_gain_old = m_celtDec.postfilter_gain;
        m_celtDec.postfilter_tapset_old = m_celtDec.postfilter_tapset;
    }

    if (C == 1)
        memcpy(&oldBandE[nbEBands], oldBandE, nbEBands * sizeof(*oldBandE));

    /* In case start or end were to change */
    if (!isTransient) {
        int16_t max_background_increase;
        memcpy(oldLogE2, oldLogE, 2 * nbEBands * sizeof(*oldLogE2));
        memcpy(oldLogE, oldBandE, 2 * nbEBands * sizeof(*oldLogE));
        /* In normal circumstances, we only allow the noise floor to increase by up to 2.4 dB/second, but when we're in DTX, we allow up to 6 dB increase for each update.*/
        if (m_celtDec.loss_count < 10)
            max_background_increase = M * QCONST16(0.001f, DB_SHIFT);
        else
            max_background_increase = QCONST16(1.f, DB_SHIFT);
        for (i = 0; i < 2 * nbEBands; i++)
            backgroundLogE[i] = min((int16_t)(backgroundLogE[i] + max_background_increase), (int16_t)(oldBandE[i]));
    }
    else {
        for (i = 0; i < 2 * nbEBands; i++)
            oldLogE[i] = min(oldLogE[i], oldBandE[i]);
    }
    c = 0;
    do {
        for (i = 0; i < start; i++)
        {
            oldBandE[c * nbEBands + i] = 0;
            oldLogE[c * nbEBands + i] = oldLogE2[c * nbEBands + i] = -QCONST16(28.f, DB_SHIFT);
        }
        for (i = end; i < nbEBands; i++)
        {
            oldBandE[c * nbEBands + i] = 0;
            oldLogE[c * nbEBands + i] = oldLogE2[c * nbEBands + i] = -QCONST16(28.f, DB_SHIFT);
        }
    } while (++c < 2);
    m_celtDec.rng = rd.get_rng();

    deemphasis(out_syn, outbuf, N, CC, m_celtDec.downsample, m_CELTMode.preemph, m_celtDec.preemph_memD, 0);
    m_celtDec.loss_count = 0;
    if (rd.tell() > 8 * rd.get_storage())
        return OPUS_INTERNAL_ERROR;
    if (rd.get_error())
        m_celtDec.error = 1;
    return frame_size / m_celtDec.downsample;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_decoder_ctl(int32_t request, ...) {
    va_list ap;

    va_start(ap, request);
    switch (request) {
        case CELT_SET_START_BAND_REQUEST: {
            int32_t value = va_arg(ap, int32_t);
            if (value < 0 || value >= m_CELTMode.nbEBands) goto bad_arg;
            m_celtDec.start = value;
        } break;
        case CELT_SET_END_BAND_REQUEST: {
            int32_t value = va_arg(ap, int32_t);
            if (value < 1 || value > m_CELTMode.nbEBands) goto bad_arg;
            m_celtDec.end = value;
        } break;
        case CELT_SET_CHANNELS_REQUEST: {
            int32_t value = va_arg(ap, int32_t);
            if (value < 1 || value > 2) goto bad_arg;
            m_celtDec.stream_channels = value;
        } break;
        case CELT_GET_AND_CLEAR_ERROR_REQUEST: {
            int32_t *value = va_arg(ap, int32_t *);
            if (value == NULL) goto bad_arg;
            *value = m_celtDec.error;
            m_celtDec.error = 0;
        } break;
        case OPUS_GET_LOOKAHEAD_REQUEST: {
            int32_t *value = va_arg(ap, int32_t *);
            if (value == NULL) goto bad_arg;
            *value = m_celtDec.overlap / m_celtDec.downsample;
        } break;
        case OPUS_RESET_STATE: {
            int32_t i;
            int16_t *lpc, *oldBandE, *oldLogE, *oldLogE2;
            lpc = (int16_t *)(m_decode_mem.get() + (DECODE_BUFFER_SIZE + m_celtDec.overlap) * m_celtDec.channels);
            oldBandE = lpc + m_celtDec.channels * LPC_ORDER;
            oldLogE = oldBandE + 2 * m_CELTMode.nbEBands;
            oldLogE2 = oldLogE + 2 * m_CELTMode.nbEBands;

            m_celtDec.rng = 0;
            m_celtDec.error = 0;
            m_celtDec.postfilter_period = 0;
            m_celtDec.postfilter_period_old = 0;
            m_celtDec.postfilter_gain = 0;
            m_celtDec.postfilter_gain_old = 0;
            m_celtDec.postfilter_tapset = 0;
            m_celtDec.postfilter_tapset_old = 0;

            for (i = 0; i < 2 * m_CELTMode.nbEBands; i++) oldLogE[i] = oldLogE2[i] = -QCONST16(28.f, DB_SHIFT);
            m_celtDec.skip_plc = 1;
        } break;
        case OPUS_GET_PITCH_REQUEST: {
            int32_t *value = va_arg(ap, int32_t *);
            if (value == NULL) goto bad_arg;
            *value = m_celtDec.postfilter_period;
        } break;
        case CELT_GET_MODE_REQUEST: {
            const CELTMode_t **value = va_arg(ap, const CELTMode_t **);
            if (value == 0) goto bad_arg;
            *value = &m_CELTMode;
        } break;
        case CELT_SET_SIGNALLING_REQUEST: {
            int32_t value = va_arg(ap, int32_t);
            m_celtDec.signalling = value;
        } break;
        case OPUS_GET_FINAL_RANGE_REQUEST: {
            uint32_t *value = va_arg(ap, uint32_t *);
            if (value == 0) goto bad_arg;
            *value = m_celtDec.rng;
        } break;
        case OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST: {
            int32_t value = va_arg(ap, int32_t);
            if (value < 0 || value > 1) {
                goto bad_arg;
            }
            m_celtDec.disable_inv = value;
        } break;
        case OPUS_GET_PHASE_INVERSION_DISABLED_REQUEST: {
            int32_t *value = va_arg(ap, int32_t *);
            if (!value) {
                goto bad_arg;
            }
            *value = m_celtDec.disable_inv;
        } break;
        default:
            goto bad_request;
    }
    va_end(ap);
    return OPUS_OK;
bad_arg:
    va_end(ap);
    return OPUS_BAD_ARG;
bad_request:
    va_end(ap);
    return OPUS_UNIMPLEMENTED;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::cwrsi(int32_t _n, int32_t _k, uint32_t _i, int32_t *_y) {
    uint32_t p;
    int32_t s;
    int32_t k0;
    int16_t val;
    int32_t yy = 0;
    assert(_k > 0);
    assert(_n > 1);
    while (_n > 2) {
        uint32_t q;
        /*Lots of pulses case:*/
        if (_k >= _n) {
            const uint32_t *row;
            //row = celt_pvq_u_row[_n];
            row = &CELT_PVQ_U_DATA[row_idx[_n]];

            /*Are the pulses in this dimension negative?*/
            p = row[_k + 1];
            s = -(_i >= p);
            _i -= p & s;
            /*Count how many pulses were placed in this dimension.*/
            k0 = _k;
            q = row[_n];
            if (q > _i) {
                assert(p > q);
                _k = _n;
                do p = celt_pvq_u_row(--_k, _n);
                while (p > _i);
            } else
                for (p = row[_k]; p > _i; p = row[_k]) _k--;
            _i -= p;
            val = (k0 - _k + s) ^ s;
            *_y++ = val;
            yy = MAC16_16(yy, val, val);
        }
        /*Lots of dimensions case:*/
        else {
            /*Are there any pulses in this dimension at all?*/
            p = celt_pvq_u_row(_k, _n);
            q = celt_pvq_u_row(_k + 1, _n);
            if (p <= _i && _i < q) {
                _i -= p;
                *_y++ = 0;
            } else {
                /*Are the pulses in this dimension negative?*/
                s = -(_i >= q);
                _i -= q & s;
                /*Count how many pulses were placed in this dimension.*/
                k0 = _k;
                do p = celt_pvq_u_row(--_k, _n);
                while (p > _i);
                _i -= p;
                val = (k0 - _k + s) ^ s;
                *_y++ = val;
                yy = MAC16_16(yy, val, val);
            }
        }
        _n--;
    }
    /*_n==2*/
    p = 2 * _k + 1;
    s = -(_i >= p);
    _i -= p & s;
    k0 = _k;
    _k = (_i + 1) >> 1;
    if (_k) _i -= 2 * _k - 1;
    val = (k0 - _k + s) ^ s;
    *_y++ = val;
    yy = MAC16_16(yy, val, val);
    /*_n==1*/
    s = -(int32_t)_i;
    val = (_k + s) ^ s;
    *_y = val;
    yy = MAC16_16(yy, val, val);
    return yy;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::decode_pulses(int32_t *_y, int32_t _n, int32_t _k) {
    return cwrsi(_n, _k, rd.dec_uint(CELT_PVQ_V(_n, _k)), _y);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::kf_bfly2(kiss_fft_cpx *Fout, int32_t m, int32_t N) {
    kiss_fft_cpx *Fout2;
    int32_t i;
    (void)m;

    {
        int16_t tw;
        tw = QCONST16(0.7071067812f, 15);
        /* We know that m==4 here because the radix-2 is just after a radix-4 */
        assert(m == 4);
        for (i = 0; i < N; i++) {
            kiss_fft_cpx t;
            Fout2 = Fout + 4;
            t = Fout2[0];
            C_SUB(Fout2[0], Fout[0], t);
            C_ADDTO(Fout[0], t);

            t.r = S_MUL(ADD32_ovflw(Fout2[1].r, Fout2[1].i), tw);
            t.i = S_MUL(SUB32_ovflw(Fout2[1].i, Fout2[1].r), tw);
            C_SUB(Fout2[1], Fout[1], t);
            C_ADDTO(Fout[1], t);

            t.r = Fout2[2].i;
            t.i = -Fout2[2].r;
            C_SUB(Fout2[2], Fout[2], t);
            C_ADDTO(Fout[2], t);

            t.r = S_MUL(SUB32_ovflw(Fout2[3].i, Fout2[3].r), tw);
            t.i = S_MUL(NEG32_ovflw(ADD32_ovflw(Fout2[3].i, Fout2[3].r)), tw);
            C_SUB(Fout2[3], Fout[3], t);
            C_ADDTO(Fout[3], t);
            Fout += 8;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::kf_bfly4(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_state *tw, int32_t m, int32_t N, int32_t mm) {
    int32_t i;

    if (m == 1) {
        /* Degenerate case where all the twiddles are 1. */
        for (i = 0; i < N; i++) {
            kiss_fft_cpx scratch0, scratch1;

            C_SUB(scratch0, *Fout, Fout[2]);
            C_ADDTO(*Fout, Fout[2]);
            C_ADD(scratch1, Fout[1], Fout[3]);
            C_SUB(Fout[2], *Fout, scratch1);
            C_ADDTO(*Fout, scratch1);
            C_SUB(scratch1, Fout[1], Fout[3]);

            Fout[1].r = ADD32_ovflw(scratch0.r, scratch1.i);
            Fout[1].i = SUB32_ovflw(scratch0.i, scratch1.r);
            Fout[3].r = SUB32_ovflw(scratch0.r, scratch1.i);
            Fout[3].i = ADD32_ovflw(scratch0.i, scratch1.r);
            Fout += 4;
        }
    } else {
        int32_t j;
        kiss_fft_cpx scratch[6];
        const kiss_twiddle_cpx *tw1, *tw2, *tw3;
        const int32_t m2 = 2 * m;
        const int32_t m3 = 3 * m;
        kiss_fft_cpx *Fout_beg = Fout;
        for (i = 0; i < N; i++) {
            Fout = Fout_beg + i * mm;
            tw3 = tw2 = tw1 = tw->twiddles;
            /* m is guaranteed to be a multiple of 4. */
            for (j = 0; j < m; j++) {
                C_MUL(scratch[0], Fout[m], *tw1);
                C_MUL(scratch[1], Fout[m2], *tw2);
                C_MUL(scratch[2], Fout[m3], *tw3);

                C_SUB(scratch[5], *Fout, scratch[1]);
                C_ADDTO(*Fout, scratch[1]);
                C_ADD(scratch[3], scratch[0], scratch[2]);
                C_SUB(scratch[4], scratch[0], scratch[2]);
                C_SUB(Fout[m2], *Fout, scratch[3]);
                tw1 += fstride;
                tw2 += fstride * 2;
                tw3 += fstride * 3;
                C_ADDTO(*Fout, scratch[3]);

                Fout[m].r = ADD32_ovflw(scratch[5].r, scratch[4].i);
                Fout[m].i = SUB32_ovflw(scratch[5].i, scratch[4].r);
                Fout[m3].r = SUB32_ovflw(scratch[5].r, scratch[4].i);
                Fout[m3].i = ADD32_ovflw(scratch[5].i, scratch[4].r);
                ++Fout;
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::kf_bfly3(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_state *tw, int32_t m, int32_t N, int32_t mm) {
    int32_t i;
    size_t k;
    const size_t m2 = 2 * m;
    const kiss_twiddle_cpx *tw1, *tw2;
    kiss_fft_cpx scratch[5];
    kiss_twiddle_cpx epi3;

    kiss_fft_cpx *Fout_beg = Fout;
    /*epi3.r = -16384;*/ /* Unused */
    epi3.i = -28378;
    for (i = 0; i < N; i++) {
        Fout = Fout_beg + i * mm;
        tw1 = tw2 = tw->twiddles;
        /* For non-custom modes, m is guaranteed to be a multiple of 4. */
        k = m;
        do {
            C_MUL(scratch[1], Fout[m], *tw1);
            C_MUL(scratch[2], Fout[m2], *tw2);

            C_ADD(scratch[3], scratch[1], scratch[2]);
            C_SUB(scratch[0], scratch[1], scratch[2]);
            tw1 += fstride;
            tw2 += fstride * 2;

            Fout[m].r = SUB32_ovflw(Fout->r, HALF_OF(scratch[3].r));
            Fout[m].i = SUB32_ovflw(Fout->i, HALF_OF(scratch[3].i));

            C_MULBYSCALAR(scratch[0], epi3.i);

            C_ADDTO(*Fout, scratch[3]);

            Fout[m2].r = ADD32_ovflw(Fout[m].r, scratch[0].i);
            Fout[m2].i = SUB32_ovflw(Fout[m].i, scratch[0].r);

            Fout[m].r = SUB32_ovflw(Fout[m].r, scratch[0].i);
            Fout[m].i = ADD32_ovflw(Fout[m].i, scratch[0].r);

            ++Fout;
        } while (--k);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::kf_bfly5(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_state *tff, int32_t m, int32_t N, int32_t mm) {
    kiss_fft_cpx *Fout0, *Fout1, *Fout2, *Fout3, *Fout4;
    int32_t i, u;
    kiss_fft_cpx scratch[13];
    const kiss_twiddle_cpx *tw;
    kiss_twiddle_cpx ya, yb;
    kiss_fft_cpx *Fout_beg = Fout;

    ya.r = 10126;
    ya.i = -31164;
    yb.r = -26510;
    yb.i = -19261;
    tw = tff->twiddles;

    for (i = 0; i < N; i++) {
        Fout = Fout_beg + i * mm;
        Fout0 = Fout;
        Fout1 = Fout0 + m;
        Fout2 = Fout0 + 2 * m;
        Fout3 = Fout0 + 3 * m;
        Fout4 = Fout0 + 4 * m;

        /* For non-custom modes, m is guaranteed to be a multiple of 4. */
        for (u = 0; u < m; ++u) {
            scratch[0] = *Fout0;

            C_MUL(scratch[1], *Fout1, tw[u * fstride]);
            C_MUL(scratch[2], *Fout2, tw[2 * u * fstride]);
            C_MUL(scratch[3], *Fout3, tw[3 * u * fstride]);
            C_MUL(scratch[4], *Fout4, tw[4 * u * fstride]);

            C_ADD(scratch[7], scratch[1], scratch[4]);
            C_SUB(scratch[10], scratch[1], scratch[4]);
            C_ADD(scratch[8], scratch[2], scratch[3]);
            C_SUB(scratch[9], scratch[2], scratch[3]);

            Fout0->r = ADD32_ovflw(Fout0->r, ADD32_ovflw(scratch[7].r, scratch[8].r));
            Fout0->i = ADD32_ovflw(Fout0->i, ADD32_ovflw(scratch[7].i, scratch[8].i));

            scratch[5].r = ADD32_ovflw(scratch[0].r, ADD32_ovflw(S_MUL(scratch[7].r, ya.r), S_MUL(scratch[8].r, yb.r)));
            scratch[5].i = ADD32_ovflw(scratch[0].i, ADD32_ovflw(S_MUL(scratch[7].i, ya.r), S_MUL(scratch[8].i, yb.r)));

            scratch[6].r = ADD32_ovflw(S_MUL(scratch[10].i, ya.i), S_MUL(scratch[9].i, yb.i));
            scratch[6].i = NEG32_ovflw(ADD32_ovflw(S_MUL(scratch[10].r, ya.i), S_MUL(scratch[9].r, yb.i)));

            C_SUB(*Fout1, scratch[5], scratch[6]);
            C_ADD(*Fout4, scratch[5], scratch[6]);

            scratch[11].r =
                ADD32_ovflw(scratch[0].r, ADD32_ovflw(S_MUL(scratch[7].r, yb.r), S_MUL(scratch[8].r, ya.r)));
            scratch[11].i =
                ADD32_ovflw(scratch[0].i, ADD32_ovflw(S_MUL(scratch[7].i, yb.r), S_MUL(scratch[8].i, ya.r)));
            scratch[12].r = SUB32_ovflw(S_MUL(scratch[9].i, ya.i), S_MUL(scratch[10].i, yb.i));
            scratch[12].i = SUB32_ovflw(S_MUL(scratch[10].r, yb.i), S_MUL(scratch[9].r, ya.i));

            C_ADD(*Fout2, scratch[11], scratch[12]);
            C_SUB(*Fout3, scratch[11], scratch[12]);

            ++Fout0;
            ++Fout1;
            ++Fout2;
            ++Fout3;
            ++Fout4;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::opus_fft_impl(const kiss_fft_state *tff, kiss_fft_cpx *fout) {
    int32_t m2, m;
    int32_t p;
    int32_t L;
    int32_t fstride[MAXFACTORS];
    int32_t i;
    int32_t shift;

    /* m_celtDec.shift can be -1 */
    shift = tff->shift > 0 ? tff->shift : 0;

    fstride[0] = 1;
    L = 0;
    do {
        p = tff->factors[2 * L];
        m = tff->factors[2 * L + 1];
        fstride[L + 1] = fstride[L] * p;
        L++;
    } while (m != 1);
    m = tff->factors[2 * L - 1];
    for (i = L - 1; i >= 0; i--) {
        if (i != 0)
            m2 = tff->factors[2 * i - 1];
        else
            m2 = 1;
        switch (tff->factors[2 * i]) {
            case 2:
                kf_bfly2(fout, m, fstride[i]);
                break;
            case 4:
                kf_bfly4(fout, fstride[i] << shift, tff, m, fstride[i], m2);
                break;
            case 3:
                kf_bfly3(fout, fstride[i] << shift, tff, m, fstride[i], m2);
                break;
            case 5:
                kf_bfly5(fout, fstride[i] << shift, tff, m, fstride[i], m2);
                break;
        }
        m = m2;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*Compute floor(sqrt(_val)) with exact arithmetic. _val must be greater than 0. This has been tested on all possible 32-bit inputs greater than 0.*/
uint32_t CeltDecoder::isqrt32(uint32_t _val) {
    uint32_t b;
    uint32_t g;
    int32_t bshift;
    /*Uses the second method from  http://www.azillionmonkeys.com/qed/sqroot.html The main idea is to search for the
     largest binary digit b such that (g+b)*(g+b) <= _val, and add it to the solution g.*/
    g = 0;
    bshift = (CELT_ILOG(_val) - 1) >> 1;
    b = 1U << bshift;
    do {
        uint32_t t;
        t = (((uint32_t)g << 1) + b) << bshift;
        if (t <= _val) {
            g += b;
            _val -= t;
        }
        b >>= 1;
        bshift--;
    } while (bshift >= 0);
    return g;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Reciprocal sqrt approximation in the range [0.25,1) (Q16 in, Q14 out) */
int16_t CeltDecoder::celt_rsqrt_norm(int32_t x) {
    int16_t n;
    int16_t r;
    int16_t r2;
    int16_t y;
    /* Range of n is [-16384,32767] ([-0.5,1) in Q15). */
    n = x - 32768;
    /* Get a rough initial guess for the root. The optimal minmax quadratic approximation (using relative error) is
       r = 1.437799046117536+n*(-0.823394375837328+n*0.4096419668459485).  Coefficients here, and the final result r,
       are Q14.*/
    r = ADD16(23557, MULT16_16_Q15(n, ADD16(-13490, MULT16_16_Q15(n, 6713))));
    /* We want y = x*r*r-1 in Q15, but x is 32-bit Q16 and r is Q14. We can compute the result from n and r using Q15
       multiplies with some adjustment, carefully done to avoid overflow. Range of y is [-1564,1594]. */
    r2 = MULT16_16_Q15(r, r);
    y = SHL16(SUB16(ADD16(MULT16_16_Q15(r2, n), r2), 16384), 1);
    /* Apply a 2nd-order Householder iteration: r += r*y*(y*0.375-0.5). This yields the Q14 reciprocal square root of
       the Q16 x, with a maximum relative error of 1.04956E-4, a (relative) RMSE of 2.80979E-5, and a peak absolute
       error of 2.26591/16384. */
    return ADD16(r, MULT16_16_Q15(r, MULT16_16_Q15(y, SUB16(MULT16_16_Q15(y, 12288), 16384))));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Sqrt approximation (QX input, QX/2 output) */
int32_t CeltDecoder::celt_sqrt(int32_t x) {
    int32_t k;
    int16_t n;
    int32_t rt;
    const int16_t C[5] = {23175, 11561, -3011, 1699, -664};
    if (x == 0)
        return 0;
    else if (x >= 1073741824)
        return 32767;
    k = (celt_ilog2(x) >> 1) - 7;
    x = VSHR32(x, 2 * k);
    n = x - 32768;
    rt = ADD16(
        C[0],
        MULT16_16_Q15(
            n, ADD16(C[1], MULT16_16_Q15(n, ADD16(C[2], MULT16_16_Q15(n, ADD16(C[3], MULT16_16_Q15(n, (C[4])))))))));
    rt = VSHR32(rt, 7 - k);
    return rt;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int16_t CeltDecoder::_celt_cos_pi_2(int16_t x) {
    int16_t x2;
    x2 = MULT16_16_P15(x, x);
    return ADD16(1, min((int32_t)32766, (int32_t)(ADD32(SUB16(32767, x2), MULT16_16_P15(x2, ADD32(-7651, MULT16_16_P15(x2, ADD32(8277, MULT16_16_P15(-626, x2)))))))));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int16_t CeltDecoder::celt_cos_norm(int32_t x) {
    x = x & 0x0001ffff;
    if (x > SHL32(EXTEND32(1), 16)) x = SUB32(SHL32(EXTEND32(1), 17), x);
    if (x & 0x00007fff) {
        if (x < SHL32(EXTEND32(1), 15)) {
            return _celt_cos_pi_2(EXTRACT16(x));
        } else {
            return NEG16(_celt_cos_pi_2(EXTRACT16(65536 - x)));
        }
    } else {
        if (x & 0x0000ffff)
            return 0;
        else if (x & 0x0001ffff)
            return -32767;
        else
            return 32767;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Reciprocal approximation (Q15 input, Q16 output) */
int32_t CeltDecoder::celt_rcp(int32_t x) {
    int32_t i;
    int16_t n;
    int16_t r;
    assert(x > 0);
    i = celt_ilog2(x);
    /* n is Q15 with range [0,1). */
    n = VSHR32(x, i - 15) - 32768;
    /* Start with a linear approximation:
       r = 1.8823529411764706-0.9411764705882353*n.
       The coefficients and the result are Q14 in the range [15420,30840].*/
    r = ADD16(30840, MULT16_16_Q15(-15420, n));
    /* Perform two Newton iterations:
       r -= r*((r*n)-1.Q15) = r*((r*n)+(r-1.Q15)). */
    r = SUB16(r, MULT16_16_Q15(r, ADD16(MULT16_16_Q15(r, n), ADD16(r, -32768))));
    /* We subtract an extra 1 in the second iteration to avoid overflow; it also
        neatly compensates for truncation error in the rest of the process. */
    r = SUB16(r, ADD16(1, MULT16_16_Q15(r, ADD16(MULT16_16_Q15(r, n), ADD16(r, -32768)))));
    /* r is now the Q15 solution to 2/(n+1), with a maximum relative error
        of 7.05346E-5, a (relative) RMSE of 2.14418E-5, and a peak absolute error of 1.24665/32768. */
    return VSHR32(EXTEND32(r), i - 16);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::clt_mdct_backward(int32_t *in, int32_t * out, int32_t overlap, int32_t shift, int32_t stride) {
    int32_t i;
    int32_t N, N2, N4;
    const int16_t *trig;

    N = m_mdct_lookup.n;
    trig = m_mdct_lookup.trig;
    for (i = 0; i < shift; i++) {
        N >>= 1;
        trig += N;
    }
    N2 = N >> 1;
    N4 = N >> 2;

    /* Pre-rotate */
    {
        /* Temp pointers to make it really clear to the compiler what we're doing */
        const int32_t * xp1 = in;
        const int32_t * xp2 = in + stride * (N2 - 1);
        int32_t * yp = out + (overlap >> 1);
        const int16_t * t = &trig[0];
        const int16_t * bitrev = m_mdct_lookup.kfft[shift]->bitrev;
        for (i = 0; i < N4; i++) {
            int32_t rev;
            int32_t yr, yi;
            rev = *bitrev++;
            yr = ADD32_ovflw(S_MUL(*xp2, t[i]), S_MUL(*xp1, t[N4 + i]));
            yi = SUB32_ovflw(S_MUL(*xp1, t[i]), S_MUL(*xp2, t[N4 + i]));
            /* We swap real and imag because we use an FFT instead of an IFFT. */
            yp[2 * rev + 1] = yr;
            yp[2 * rev] = yi;
            /* Storing the pre-rotation directly in the bitrev order. */
            xp1 += 2 * stride;
            xp2 -= 2 * stride;
        }
    }

    opus_fft_impl(m_mdct_lookup.kfft[shift], (kiss_fft_cpx *)(out + (overlap >> 1)));

    /* Post-rotate and de-shuffle from both ends of the buffer at once to make it in-place. */
    {
        int32_t *yp0 = out + (overlap >> 1);
        int32_t *yp1 = out + (overlap >> 1) + N2 - 2;
        const int16_t *t = &trig[0];
        /* Loop to (N4+1)>>1 to handle odd N4. When N4 is odd, the
           middle pair will be computed twice. */
        for (i = 0; i < (N4 + 1) >> 1; i++) {
            int32_t re, im, yr, yi;
            int16_t t0, t1;
            /* We swap real and imag because we're using an FFT instead of an IFFT. */
            re = yp0[1];
            im = yp0[0];
            t0 = t[i];
            t1 = t[N4 + i];
            /* We'd scale up by 2 here, but instead it's done when mixing the windows */
            yr = ADD32_ovflw(S_MUL(re, t0), S_MUL(im, t1));
            yi = SUB32_ovflw(S_MUL(re, t1), S_MUL(im, t0));
            /* We swap real and imag because we're using an FFT instead of an IFFT. */
            re = yp1[1];
            im = yp1[0];
            yp0[0] = yr;
            yp1[1] = yi;

            t0 = t[(N4 - i - 1)];
            t1 = t[(N2 - i - 1)];
            /* We'd scale up by 2 here, but instead it's done when mixing the windows */
            yr = ADD32_ovflw(S_MUL(re, t0), S_MUL(im, t1));
            yi = SUB32_ovflw(S_MUL(re, t1), S_MUL(im, t0));
            yp1[0] = yr;
            yp0[1] = yi;
            yp0 += 2;
            yp1 -= 2;
        }
    }

    /* Mirror on both sides for TDAC */
    {
        int32_t * xp1 = out + overlap - 1;
        int32_t * yp1 = out;
        const int16_t * wp1 = window120;
        const int16_t * wp2 = window120 + overlap - 1;

        for (i = 0; i < overlap / 2; i++) {
            int32_t x1, x2;
            x1 = *xp1;
            x2 = *yp1;
            *yp1++ = SUB32_ovflw(MULT16_32_Q15(*wp2, x2), MULT16_32_Q15(*wp1, x1));
            *xp1-- = ADD32_ovflw(MULT16_32_Q15(*wp1, x2), MULT16_32_Q15(*wp2, x1));
            wp1++;
            wp2--;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::interp_bits2pulses( int32_t start, int32_t end, int32_t skip_start, const int32_t *bits1, const int32_t *bits2, const int32_t *thresh, const int32_t *cap, int32_t total, int32_t *_balance,
                            int32_t skip_rsv, int32_t *intensity, int32_t intensity_rsv, int32_t *dual_stereo, int32_t dual_stereo_rsv, int32_t *bits, int32_t *ebits, int32_t *fine_priority,
                            int32_t C, int32_t LM, int32_t encode, int32_t prev, int32_t signalBandwidth) {
    int32_t psum;
    int32_t lo, hi;
    int32_t i, j;
    int32_t logM;
    int32_t stereo;
    int32_t codedBands = -1;
    int32_t alloc_floor;
    int32_t left, percoeff;
    int32_t done;
    int32_t balance;

    alloc_floor = C << BITRES;
    stereo = C > 1;

    logM = LM << BITRES;
    lo = 0;
    hi = 1 << ALLOC_STEPS;
    for (i = 0; i < ALLOC_STEPS; i++) {
        int32_t mid = (lo + hi) >> 1;
        psum = 0;
        done = 0;
        for (j = end; j-- > start;) {
            int32_t tmp = bits1[j] + (mid * (int32_t)bits2[j] >> ALLOC_STEPS);
            if (tmp >= thresh[j] || done) {
                done = 1;
                /* Don't allocate more than we can actually use */
                psum += min(tmp, cap[j]);
            } else {
                if (tmp >= alloc_floor) psum += alloc_floor;
            }
        }
        if (psum > total)
            hi = mid;
        else
            lo = mid;
    }
    psum = 0;
    /*printf ("interp bisection gave %d\n", lo);*/
    done = 0;
    for (j = end; j-- > start;) {
        int32_t tmp = bits1[j] + ((int32_t)lo * bits2[j] >> ALLOC_STEPS);
        if (tmp < thresh[j] && !done) {
            if (tmp >= alloc_floor)
                tmp = alloc_floor;
            else
                tmp = 0;
        } else
            done = 1;
        /* Don't allocate more than we can actually use */
        tmp = min(tmp, cap[j]);
        bits[j] = tmp;
        psum += tmp;
    }

    /* Decide which bands to skip, working backwards from the end. */
    for (codedBands = end;; codedBands--) {
        int32_t band_width;
        int32_t band_bits;
        int32_t rem;
        j = codedBands - 1;
        /* Never skip the first band, nor a band that has been boosted by dynalloc.
           In the first case, we'd be coding a bit to signal we're going to waste all the other bits.
           In the second case, we'd be coding a bit to redistribute all the bits we just signaled should be cocentrated in this band. */
        if (j <= skip_start) {
            /* Give the bit we reserved to end skipping back. */
            total += skip_rsv;
            break;
        }
        /*Figure out how many left-over bits we would be adding to this band. This can include bits we've stolen back from higher, skipped bands.*/
        left = total - psum;
        percoeff = celt_udiv(left, eband5ms[codedBands] - eband5ms[start]);
        left -= (eband5ms[codedBands] - eband5ms[start]) * percoeff;
        rem = max(left - (eband5ms[j] - eband5ms[start]), (int32_t)0);
        band_width = eband5ms[codedBands] - eband5ms[j];
        band_bits = (int32_t)(bits[j] + percoeff * band_width + rem);
        /*Only code a skip decision if we're above the threshold for this band. Otherwise it is force-skipped. This ensures that we have enough bits to code the skip flag.*/
        if (band_bits >= max(thresh[j], alloc_floor + (1 << BITRES))) {
            if (encode) {
                ;
            } else if (rd.dec_bit_logp(1)) {
                break;
            }
            /*We used a bit to skip this band.*/
            psum += 1 << BITRES;
            band_bits -= 1 << BITRES;
        }
        /*Reclaim the bits originally allocated to this band.*/
        psum -= bits[j] + intensity_rsv;
        if (intensity_rsv > 0) intensity_rsv = LOG2_FRAC_TABLE[j - start];
        psum += intensity_rsv;
        if (band_bits >= alloc_floor) {
            /*If we have enough for a fine energy bit per channel, use it.*/
            psum += alloc_floor;
            bits[j] = alloc_floor;
        } else {
            /*Otherwise this band gets nothing at all.*/
            bits[j] = 0;
        }
    }

    assert(codedBands > start);
    /* Code the intensity and dual stereo parameters. */
    if (intensity_rsv > 0) {
        if (encode) {
            ;
        } else
            *intensity = start + rd.dec_uint(codedBands + 1 - start);
    } else
        *intensity = 0;
    if (*intensity <= start) {
        total += dual_stereo_rsv;
        dual_stereo_rsv = 0;
    }
    if (dual_stereo_rsv > 0) {
        if (encode)
            ;
        else
            *dual_stereo = rd.dec_bit_logp(1);
    } else
        *dual_stereo = 0;

    /* Allocate the remaining bits */
    left = total - psum;
    percoeff = celt_udiv(left, eband5ms[codedBands] - eband5ms[start]);
    left -= (eband5ms[codedBands] - eband5ms[start]) * percoeff;
    for (j = start; j < codedBands; j++) bits[j] += ((int32_t)percoeff * (eband5ms[j + 1] - eband5ms[j]));
    for (j = start; j < codedBands; j++) {
        int32_t tmp = (int32_t)min(left, (int32_t)(eband5ms[j + 1] - eband5ms[j]));
        bits[j] += tmp;
        left -= tmp;
    }
    /*for (j=0;j<end;j++)printf("%d ", bits[j]);printf("\n");*/

    balance = 0;
    for (j = start; j < codedBands; j++) {
        int32_t N0, N, den;
        int32_t offset;
        int32_t NClogN;
        int32_t excess, bit;

        assert(bits[j] >= 0);
        N0 = eband5ms[j + 1] - eband5ms[j];
        N = N0 << LM;
        bit = (int32_t)bits[j] + balance;

        if (N > 1) {
            excess = max(bit - cap[j], (int32_t)0);
            bits[j] = bit - excess;

            /* Compensate for the extra DoF in stereo */
            den = (C * N + ((C == 2 && N > 2 && !*dual_stereo && j < *intensity) ? 1 : 0));

            NClogN = den * (logN400[j] + logM);

            /* Offset for the number of fine bits by log2(N)/2 + FINE_OFFSET compared to their "fair share" of total/N */
            offset = (NClogN >> 1) - den * FINE_OFFSET;

            /* N=2 is the only point that doesn't match the curve */
            if (N == 2) offset += den << BITRES >> 2;

            /* Changing the offset for allocating the second and third
                fine energy bit */
            if (bits[j] + offset < den * 2 << BITRES)
                offset += NClogN >> 2;
            else if (bits[j] + offset < den * 3 << BITRES)
                offset += NClogN >> 3;

            /* Divide with rounding */
            ebits[j] = max((int32_t)0, (bits[j] + offset + (den << (BITRES - 1))));
            ebits[j] = celt_udiv(ebits[j], den) >> BITRES;

            /* Make sure not to bust */
            if (C * ebits[j] > (bits[j] >> BITRES)) ebits[j] = bits[j] >> stereo >> BITRES;

            /* More than that is useless because that's about as far as PVQ can go */
            ebits[j] = min(ebits[j], (int32_t)MAX_FINE_BITS);

            /* If we rounded down or capped this band, make it a candidate for the
                final fine energy pass */
            fine_priority[j] = ebits[j] * (den << BITRES) >= bits[j] + offset;

            /* Remove the allocated fine bits; the rest are assigned to PVQ */
            bits[j] -= C * ebits[j] << BITRES;

        } else {
            /* For N=1, all bits go to fine energy except for a single sign bit */
            excess = max((int32_t)0, bit - (C << BITRES));
            bits[j] = bit - excess;
            ebits[j] = 0;
            fine_priority[j] = 1;
        }

        /* Fine energy can't take advantage of the re-balancing in quant_all_bands().
           Instead, do the re-balancing here.*/
        if (excess > 0) {
            int32_t extra_fine;
            int32_t extra_bits;
            extra_fine = min(excess >> (stereo + BITRES), (int32_t)(MAX_FINE_BITS - ebits[j]));
            ebits[j] += extra_fine;
            extra_bits = extra_fine * C << BITRES;
            fine_priority[j] = extra_bits >= excess - balance;
            excess -= extra_bits;
        }
        balance = excess;

        assert(bits[j] >= 0);
        assert(ebits[j] >= 0);
    }
    /* Save any remaining bits over the cap for the rebalancing in quant_all_bands(). */
    *_balance = balance;

    /* The skipped bands use all their bits for fine energy. */
    for (; j < end; j++) {
        ebits[j] = bits[j] >> stereo >> BITRES;
        assert(C * ebits[j] << BITRES == bits[j]);
        bits[j] = 0;
        fine_priority[j] = ebits[j] < 1;
    }

    return codedBands;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::clt_compute_allocation(int32_t start, int32_t end, const int32_t *offsets, const int32_t *cap, int32_t alloc_trim, int32_t *intensity, int32_t *dual_stereo, int32_t total, int32_t *balance,
                               int32_t *pulses, int32_t *ebits, int32_t *fine_priority, int32_t C, int32_t LM, int32_t encode, int32_t prev, int32_t signalBandwidth) {
    int32_t lo, hi, len, j;
    int32_t codedBands;
    int32_t skip_start;
    int32_t skip_rsv;
    int32_t intensity_rsv;
    int32_t dual_stereo_rsv;

    total = max(total, (int32_t)0);
    len = m_CELTMode.nbEBands;
    skip_start = start;
    /* Reserve a bit to signal the end of manually skipped bands. */
    skip_rsv = total >= 1 << BITRES ? 1 << BITRES : 0;
    total -= skip_rsv;
    /* Reserve bits for the intensity and dual stereo parameters. */
    intensity_rsv = dual_stereo_rsv = 0;
    if (C == 2) {
        intensity_rsv = LOG2_FRAC_TABLE[end - start];
        if (intensity_rsv > total)
            intensity_rsv = 0;
        else {
            total -= intensity_rsv;
            dual_stereo_rsv = total >= 1 << BITRES ? 1 << BITRES : 0;
            total -= dual_stereo_rsv;
        }
    }
    ps_ptr<int32_t>bits1;       bits1.alloc_array(len);
    ps_ptr<int32_t>bits2;       bits2.alloc_array(len);
    ps_ptr<int32_t>thresh;      thresh.alloc_array(len);
    ps_ptr<int32_t>trim_offset; trim_offset.alloc_array(len);

    for (j = start; j < end; j++) {
        /* Below this threshold, we're sure not to allocate any PVQ bits */
        thresh[j] = max((int32_t)((C) << BITRES), (int32_t)((3 * (eband5ms[j + 1] - eband5ms[j]) << LM << BITRES) >> 4));
        /* Tilt of the allocation curve */
        trim_offset[j] =
            C * (eband5ms[j + 1] - eband5ms[j]) * (alloc_trim - 5 - LM) * (end - j - 1) * (1 << (LM + BITRES)) >> 6;
        /* Giving less resolution to single-coefficient bands because they get  more benefit from having one coarse value per coefficient*/
        if ((eband5ms[j + 1] - eband5ms[j]) << LM == 1) trim_offset[j] -= C << BITRES;
    }
    lo = 1;
    hi = m_CELTMode.nbAllocVectors - 1;
    do {
        int32_t done = 0;
        int32_t psum = 0;
        int32_t mid = (lo + hi) >> 1;
        for (j = end; j-- > start;) {
            int32_t bitsj;
            int32_t N = eband5ms[j + 1] - eband5ms[j];
            bitsj = C * N * band_allocation[mid * len + j] << LM >> 2;
            if (bitsj > 0) bitsj = max((int32_t)0, bitsj + trim_offset[j]);
            bitsj += offsets[j];
            if (bitsj >= thresh[j] || done) {
                done = 1;
                /* Don't allocate more than we can actually use */
                psum += min(bitsj, cap[j]);
            } else {
                if (bitsj >= C << BITRES) psum += C << BITRES;
            }
        }
        if (psum > total)
            hi = mid - 1;
        else
            lo = mid + 1;
        /*printf ("lo = %d, hi = %d\n", lo, hi);*/
    } while (lo <= hi);
    hi = lo--;
    /*printf ("interp between %d and %d\n", lo, hi);*/
    for (j = start; j < end; j++) {
        int32_t bits1j, bits2j;
        int32_t N = eband5ms[j + 1] - eband5ms[j];
        bits1j = C * N * band_allocation[lo * len + j] << LM >> 2;
        bits2j = hi >= m_CELTMode.nbAllocVectors ? cap[j] : C * N * band_allocation[hi * len + j] << LM >> 2;
        if (bits1j > 0) bits1j = max((int32_t)0, bits1j + trim_offset[j]);
        if (bits2j > 0) bits2j = max((int32_t)0, bits2j + trim_offset[j]);
        if (lo > 0) bits1j += offsets[j];
        bits2j += offsets[j];
        if (offsets[j] > 0) skip_start = j;
        bits2j = max((int32_t)0, bits2j - bits1j);
        bits1[j] = bits1j;
        bits2[j] = bits2j;
    }
    codedBands = interp_bits2pulses(start, end, skip_start, bits1.get(), bits2.get(), thresh.get(), cap, total, balance, skip_rsv,
                                    intensity, intensity_rsv, dual_stereo, dual_stereo_rsv, pulses, ebits,
                                    fine_priority, C, LM, encode, prev, signalBandwidth);

    return codedBands;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::unquant_coarse_energy(int32_t start, int32_t end, int16_t *oldEBands, int32_t intra, int32_t C, int32_t LM) {
    const uint8_t *prob_model = e_prob_model[LM][intra];
    int32_t i, c;
    int32_t prev[2] = {0, 0};
    int16_t coef;
    int16_t beta;
    int32_t budget;
    int32_t tell;

    if (intra) {
        coef = 0;
        beta = beta_intra;
    } else {
        beta = beta_coef[LM];
        coef = pred_coef[LM];
    }

    budget = rd.get_storage() * 8;

    /* Decode at a fixed coarse resolution */
    for (i = start; i < end; i++) {
        c = 0;
        do {
            int32_t qi;
            int32_t q;
            int32_t tmp;
            /* It would be better to express this invariant as a test on C at function entry, but that isn't enough to make the static analyzer happy. */
            assert(c < 2);
            tell = rd.tell();
            if (budget - tell >= 15) {
                int32_t pi;
                pi = 2 * min(i, (int32_t)20);
                qi = rd.laplace_decode(prob_model[pi] << 7, prob_model[pi + 1] << 6);
            } else if (budget - tell >= 2) {
                qi = rd.dec_icdf(small_energy_icdf, 2);
                qi = (qi >> 1) ^ -(qi & 1);
            } else if (budget - tell >= 1) {
                qi = -rd.dec_bit_logp(1);
            } else
                qi = -1;
            q = (int32_t)SHL32(EXTEND32(qi), DB_SHIFT);

            oldEBands[i + c * m_CELTMode.nbEBands] = max((int32_t)(-QCONST16(9.f, DB_SHIFT)), (int32_t)(oldEBands[i + c * m_CELTMode.nbEBands]));
            tmp = PSHR32(MULT16_16(coef, oldEBands[i + c * m_CELTMode.nbEBands]), 8) + prev[c] + SHL32(q, 7);
            tmp = max(-QCONST32(28.f, DB_SHIFT + 7), tmp);
            oldEBands[i + c * m_CELTMode.nbEBands] = PSHR32(tmp, 7);
            prev[c] = prev[c] + SHL32(q, 7) - MULT16_16(beta, PSHR32(q, 8));
        } while (++c < C);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::unquant_fine_energy(int32_t start, int32_t end, int16_t *oldEBands, int32_t *fine_quant, int32_t C) {
    int32_t i, c;
    /* Decode finer resolution */
    for (i = start; i < end; i++) {
        if (fine_quant[i] <= 0) continue;
        c = 0;
        do {
            int32_t q2;
            int16_t offset;
            q2 = rd.dec_bits(fine_quant[i]);
            offset = SUB16(SHR32(SHL32(EXTEND32(q2), DB_SHIFT) + QCONST16(.5f, DB_SHIFT), fine_quant[i]),
                           QCONST16(.5f, DB_SHIFT));
            oldEBands[i + c * m_CELTMode.nbEBands] += offset;
        } while (++c < C);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::unquant_energy_finalise(int32_t start, int32_t end, int16_t *oldEBands, int32_t *fine_quant,
                             int32_t *fine_priority, int32_t bits_left, int32_t C) {
    int32_t i, prio, c;

    /* Use up the remaining bits */
    for (prio = 0; prio < 2; prio++) {
        for (i = start; i < end && bits_left >= C; i++) {
            if (fine_quant[i] >= MAX_FINE_BITS || fine_priority[i] != prio) continue;
            c = 0;
            do {
                int32_t q2;
                int16_t offset;
                q2 = rd.dec_bits(1);
                offset = SHR16(SHL16(q2, DB_SHIFT) - QCONST16(.5f, DB_SHIFT), fine_quant[i] + 1);
                oldEBands[i + c * m_CELTMode.nbEBands] += offset;
                bits_left--;
            } while (++c < C);
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int16_t CeltDecoder::SAT16(int32_t x) {
   return x > 32767 ? 32767 : x < -32768 ? -32768 : (int16_t)x;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t CeltDecoder::celt_udiv(uint32_t n, uint32_t d) {
   assert(d>0); return n/d;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_sudiv(int32_t n, int32_t d) {
   assert(d>0); return n/d;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int16_t CeltDecoder::sig2word16(int32_t x){
   x = PSHR32(x, 12);
   x = max(x, (int32_t)-32768);
   x = min(x, (int32_t)32767);
   return EXTRACT16(x);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Atan approximation using a 4th order polynomial. Input is in Q15 format and normalized by pi/4. Output is in Q15 format */
int16_t CeltDecoder::celt_atan01(int16_t x) {
    return MULT16_16_P15(
        x, ADD32(32767, MULT16_16_P15(x, ADD32(-21, MULT16_16_P15(x, ADD32(-11943, MULT16_16_P15(4936, x)))))));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* atan2() approximation valid for positive input values */
int16_t CeltDecoder::celt_atan2p(int16_t y, int16_t x) {
    if (y < x) {
        int32_t arg;
        arg = celt_div(SHL32(EXTEND32(y), 15), x);
        if (arg >= 32767) arg = 32767;
        return SHR16(celt_atan01(EXTRACT16(arg)), 1);
    } else {
        int32_t arg;
        arg = celt_div(SHL32(EXTEND32(x), 15), y);
        if (arg >= 32767) arg = 32767;
        return 25736 - SHR16(celt_atan01(EXTRACT16(arg)), 1);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_maxabs16(const int16_t *x, int32_t len) {
    int32_t i;
    int16_t maxval = 0;
    int16_t minval = 0;
    for (i = 0; i < len; i++) {
        maxval = max(maxval, x[i]);
        minval = min(minval, x[i]);
    }
    return max(EXTEND32(maxval), -EXTEND32(minval));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_maxabs32(const int32_t *x, int32_t len) {
    int32_t i;
    int32_t maxval = 0;
    int32_t minval = 0;
    for (i = 0; i < len; i++) {
        maxval = max(maxval, x[i]);
        minval = min(minval, x[i]);
    }
    return max(maxval, -minval);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Integer log in base2. Undefined for zero and negative numbers */
int16_t CeltDecoder::celt_ilog2(int32_t x) {
    assert(x > 0);
    return CELT_ILOG(x) - 1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Integer log in base2. Defined for zero, but not for negative numbers */
int16_t CeltDecoder::celt_zlog2(int32_t x) { return x <= 0 ? 0 : celt_ilog2(x); }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Base-2 logarithm approximation (log2(x)). (Q14 input, Q10 output) */
int16_t CeltDecoder::celt_log2(int32_t x) {
    int32_t i;
    int16_t n, frac;
    /* -0.41509302963303146, 0.9609890551383969, -0.31836011537636605,
        0.15530808010959576, -0.08556153059057618 */
    const int16_t C[5] = {-6801 + (1 << (13 - DB_SHIFT)), 15746, -5217, 2545, -1401};
    if (x == 0) return -32767;
    i = celt_ilog2(x);
    n = VSHR32(x, i - 15) - 32768 - 16384;
    frac = ADD16(
        C[0],
        MULT16_16_Q15(
            n, ADD16(C[1], MULT16_16_Q15(n, ADD16(C[2], MULT16_16_Q15(n, ADD16(C[3], MULT16_16_Q15(n, C[4]))))))));
    return SHL16(i - 13, DB_SHIFT) + SHR16(frac, 14 - DB_SHIFT);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_exp2_frac(int16_t x) {
    int16_t frac;
    frac = SHL16(x, 4);
    return ADD16(16383,
                 MULT16_16_Q15(frac, ADD16(22804, MULT16_16_Q15(frac, ADD16(14819, MULT16_16_Q15(10204, frac))))));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/** Base-2 exponential approximation (2^x). (Q10 input, Q16 output) */
int32_t CeltDecoder::celt_exp2(int16_t x) {
    int32_t integer;
    int16_t frac;
    integer = SHR16(x, 10);
    if (integer > 14)
        return 0x7f000000;
    else if (integer < -15)
        return 0;
    frac = celt_exp2_frac(x - SHL16(integer, 10));
    return VSHR32(EXTEND32(frac), -integer - 2);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void CeltDecoder::dual_inner_prod_c(const int16_t *x, const int16_t *y01, const int16_t *y02, int32_t N, int32_t *xy1, int32_t *xy2) {
    int32_t i;
    int32_t xy01 = 0;
    int32_t xy02 = 0;
    for (i = 0; i < N; i++) {
        xy01 = MAC16_16(xy01, x[i], y01[i]);
        xy02 = MAC16_16(xy02, x[i], y02[i]);
    }
    *xy1 = xy01;
    *xy2 = xy02;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::celt_inner_prod_c(const int16_t *x, const int16_t *y, int32_t N) {
    int32_t i;
    int32_t xy = 0;
    for (i = 0; i < N; i++) xy = MAC16_16(xy, x[i], y[i]);
    return xy;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::get_pulses(int32_t i){
   return i<8 ? i : (8 + (i&7)) << ((i>>3)-1);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::bits2pulses(int32_t band, int32_t LM, int32_t bits){
   int32_t i;
   int32_t lo, hi;
   const uint8_t *cache;

   LM++;
   cache = cache_bits50 + cache_index50[LM * m_CELTMode.nbEBands+band];

   lo = 0;
   hi = cache[0];
   bits--;
   for (i=0;i<LOG_MAX_PSEUDO;i++)
   {
      int32_t mid = (lo+hi+1)>>1;
      /* OPT: Make sure this is implemented with a conditional move */
      if ((int32_t)cache[mid] >= bits)
         hi = mid;
      else
         lo = mid;
   }
   if (bits- (lo == 0 ? -1 : (int32_t)cache[lo]) <= (int32_t)cache[hi]-bits)
      return lo;
   else
      return hi;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t CeltDecoder::pulses2bits(int32_t band, int32_t LM, int32_t pulses){
   const uint8_t *cache;

   LM++;
   cache = cache_bits50 + cache_index50[LM * m_CELTMode.nbEBands+band];
   return pulses == 0 ? 0 : cache[pulses]+1;
}
