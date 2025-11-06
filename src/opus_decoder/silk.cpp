/*******************************************************************************************************************************************************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved. Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the
  distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the names of specific contributors, may be used to endorse or promote products derived from this software without specific prior written
  permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************************************************************************************************************************************/

#include "Arduino.h"
#include "opus_decoder.h"
#include "range_decoder.h"
#include "silk.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool SilkDecoder::init() {
    m_resampler_state.alloc_array(DECODER_NUM_CHANNELS);
    m_channel_state.alloc_array(DECODER_NUM_CHANNELS);
    m_silk_decoder.alloc();
    m_silk_decoder_control.alloc();
    m_silk_DecControlStruct.alloc();
    return true;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::clear() {
    m_resampler_state.clear();
    m_channel_state.clear();
    m_silk_decoder.clear();
    m_silk_decoder_control.clear();
    m_silk_DecControlStruct.clear();
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::reset() {
    m_resampler_state.reset();
    m_channel_state.reset();
    m_silk_decoder.reset();
    m_silk_decoder_control.reset();
    m_silk_DecControlStruct.reset();
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Split signal into two decimated bands using first-order allpass filters */
void SilkDecoder::silk_ana_filt_bank_1(const int16_t* in,   /* I    Input signal [N]        */
                                       int32_t*       S,    /* I/O  State vector [2]        */
                                       int16_t*       outL, /* O    Low band [N/2]          */
                                       int16_t*       outH, /* O    High band [N/2]         */
                                       const int32_t  N     /* I    Number of input samples */
) {
    int32_t k, N2 = silk_RSHIFT(N, 1);
    int32_t in32, X, Y, out_1, out_2;

    /* Internal variables and state are in Q10 format */
    for (k = 0; k < N2; k++) {
        /* Convert to Q10 */
        in32 = silk_LSHIFT((int32_t)in[2 * k], 10);

        /* All-pass section for even input sample */
        Y = silk_SUB32(in32, S[0]);
        X = silk_SMLAWB(Y, Y, A_fb1_21);
        out_1 = silk_ADD32(S[0], X);
        S[0] = silk_ADD32(in32, X);

        /* Convert to Q10 */
        in32 = silk_LSHIFT((int32_t)in[2 * k + 1], 10);

        /* All-pass section for odd input sample, and add to output of previous section */
        Y = silk_SUB32(in32, S[1]);
        X = silk_SMULWB(Y, A_fb1_20);
        out_2 = silk_ADD32(S[1], X);
        S[1] = silk_ADD32(in32, X);

        /* Add/subtract, convert back to int16 and store to output */
        outL[k] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(silk_ADD32(out_2, out_1), 11));
        outH[k] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(silk_SUB32(out_2, out_1), 11));
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Second order ARMA filter, alternative implementation */
void SilkDecoder::silk_biquad_alt_stride1(const int16_t* in,    /* I     input signal                 */
                                          const int32_t* B_Q28, /* I     MA coefficients [3]          */
                                          const int32_t* A_Q28, /* I     AR coefficients [2]          */
                                          int32_t*       S,     /* I/O   State vector [2]             */
                                          int16_t*       out,   /* O     output signal                */
                                          const int32_t  len    /* I     signal length (must be even) */
) {
    /* DIRECT FORM II TRANSPOSED (uses 2 element state vector) */
    int32_t k;
    int32_t inval, A0_U_Q28, A0_L_Q28, A1_U_Q28, A1_L_Q28, out32_Q14;

    /* Negate A_Q28 values and split in two parts */
    A0_L_Q28 = (-A_Q28[0]) & 0x00003FFF;   /* lower part */
    A0_U_Q28 = silk_RSHIFT(-A_Q28[0], 14); /* upper part */
    A1_L_Q28 = (-A_Q28[1]) & 0x00003FFF;   /* lower part */
    A1_U_Q28 = silk_RSHIFT(-A_Q28[1], 14); /* upper part */

    for (k = 0; k < len; k++) {
        /* S[ 0 ], S[ 1 ]: Q12 */
        inval = in[k];
        out32_Q14 = silk_LSHIFT(silk_SMLAWB(S[0], B_Q28[0], inval), 2);

        S[0] = S[1] + silk_RSHIFT_ROUND(silk_SMULWB(out32_Q14, A0_L_Q28), 14);
        S[0] = silk_SMLAWB(S[0], out32_Q14, A0_U_Q28);
        S[0] = silk_SMLAWB(S[0], B_Q28[1], inval);

        S[1] = silk_RSHIFT_ROUND(silk_SMULWB(out32_Q14, A1_L_Q28), 14);
        S[1] = silk_SMLAWB(S[1], out32_Q14, A1_U_Q28);
        S[1] = silk_SMLAWB(S[1], B_Q28[2], inval);

        /* Scale back to Q0 and saturate */
        out[k] = (int16_t)silk_SAT16(silk_RSHIFT(out32_Q14 + (1 << 14) - 1, 14));
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_biquad_alt_stride2_c(const int16_t* in,    /* I     input signal                 */
                                            const int32_t* B_Q28, /* I     MA coefficients [3]          */
                                            const int32_t* A_Q28, /* I     AR coefficients [2]          */
                                            int32_t*       S,     /* I/O   State vector [4]             */
                                            int16_t*       out,   /* O     output signal                */
                                            const int32_t  len    /* I     signal length (must be even) */
) {
    /* DIRECT FORM II TRANSPOSED (uses 2 element state vector) */
    int32_t k;
    int32_t A0_U_Q28, A0_L_Q28, A1_U_Q28, A1_L_Q28, out32_Q14[2];

    /* Negate A_Q28 values and split in two parts */
    A0_L_Q28 = (-A_Q28[0]) & 0x00003FFF;   /* lower part */
    A0_U_Q28 = silk_RSHIFT(-A_Q28[0], 14); /* upper part */
    A1_L_Q28 = (-A_Q28[1]) & 0x00003FFF;   /* lower part */
    A1_U_Q28 = silk_RSHIFT(-A_Q28[1], 14); /* upper part */

    for (k = 0; k < len; k++) {
        /* S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ]: Q12 */
        out32_Q14[0] = silk_LSHIFT(silk_SMLAWB(S[0], B_Q28[0], in[2 * k + 0]), 2);
        out32_Q14[1] = silk_LSHIFT(silk_SMLAWB(S[2], B_Q28[0], in[2 * k + 1]), 2);

        S[0] = S[1] + silk_RSHIFT_ROUND(silk_SMULWB(out32_Q14[0], A0_L_Q28), 14);
        S[2] = S[3] + silk_RSHIFT_ROUND(silk_SMULWB(out32_Q14[1], A0_L_Q28), 14);
        S[0] = silk_SMLAWB(S[0], out32_Q14[0], A0_U_Q28);
        S[2] = silk_SMLAWB(S[2], out32_Q14[1], A0_U_Q28);
        S[0] = silk_SMLAWB(S[0], B_Q28[1], in[2 * k + 0]);
        S[2] = silk_SMLAWB(S[2], B_Q28[1], in[2 * k + 1]);

        S[1] = silk_RSHIFT_ROUND(silk_SMULWB(out32_Q14[0], A1_L_Q28), 14);
        S[3] = silk_RSHIFT_ROUND(silk_SMULWB(out32_Q14[1], A1_L_Q28), 14);
        S[1] = silk_SMLAWB(S[1], out32_Q14[0], A1_U_Q28);
        S[3] = silk_SMLAWB(S[3], out32_Q14[1], A1_U_Q28);
        S[1] = silk_SMLAWB(S[1], B_Q28[2], in[2 * k + 0]);
        S[3] = silk_SMLAWB(S[3], B_Q28[2], in[2 * k + 1]);

        /* Scale back to Q0 and saturate */
        out[2 * k + 0] = (int16_t)silk_SAT16(silk_RSHIFT(out32_Q14[0] + (1 << 14) - 1, 14));
        out[2 * k + 1] = (int16_t)silk_SAT16(silk_RSHIFT(out32_Q14[1] + (1 << 14) - 1, 14));
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Chirp (bandwidth expand) LP AR filter */
void SilkDecoder::silk_bwexpander_32(int32_t*      ar,       /* I/O  AR filter to be expanded (without leading 1)                */
                                     const int32_t d,        /* I    Length of ar                                                */
                                     int32_t       chirp_Q16 /* I    Chirp factor in Q16                                         */
) {
    int32_t i;
    int32_t chirp_minus_one_Q16 = chirp_Q16 - 65536;

    for (i = 0; i < d - 1; i++) {
        ar[i] = silk_SMULWW(chirp_Q16, ar[i]);
        chirp_Q16 += silk_RSHIFT_ROUND(silk_MUL(chirp_Q16, chirp_minus_one_Q16), 16);
    }
    ar[d - 1] = silk_SMULWW(chirp_Q16, ar[d - 1]);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Chirp (bandwidth expand) LP AR filter */
void SilkDecoder::silk_bwexpander(int16_t*      ar,       /* I/O  AR filter to be expanded (without leading 1)                */
                                  const int32_t d,        /* I    Length of ar                                                */
                                  int32_t       chirp_Q16 /* I    Chirp factor (typically in the range 0 to 1)                */
) {
    int32_t i;
    int32_t chirp_minus_one_Q16 = chirp_Q16 - 65536;

    /* NB: Dont use silk_SMULWB, instead of silk_RSHIFT_ROUND( silk_MUL(), 16 ), below. Bias in silk_SMULWB can lead to unstable filters                                */
    for (i = 0; i < d - 1; i++) {
        ar[i] = (int16_t)silk_RSHIFT_ROUND(silk_MUL(chirp_Q16, ar[i]), 16);
        chirp_Q16 += silk_RSHIFT_ROUND(silk_MUL(chirp_Q16, chirp_minus_one_Q16), 16);
    }
    ar[d - 1] = (int16_t)silk_RSHIFT_ROUND(silk_MUL(chirp_Q16, ar[d - 1]), 16);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Decode mid/side predictors */
void SilkDecoder::silk_stereo_decode_pred(int32_t pred_Q13[] /* O    Predictors                                  */
) {
    int32_t n, ix[2][3];
    int32_t low_Q13, step_Q13;

    /* Entropy decoding */
    n = rd.dec_icdf(silk_stereo_pred_joint_iCDF, 8);
    ix[0][2] = silk_DIV32_16(n, 5);
    ix[1][2] = n - 5 * ix[0][2];
    for (n = 0; n < 2; n++) {
        ix[n][0] = rd.dec_icdf(silk_uniform3_iCDF, 8);
        ix[n][1] = rd.dec_icdf(silk_uniform5_iCDF, 8);
    }

    /* Dequantize */
    for (n = 0; n < 2; n++) {
        ix[n][0] += 3 * ix[n][2];
        low_Q13 = silk_stereo_pred_quant_Q13[ix[n][0]];
        step_Q13 = silk_SMULWB(silk_stereo_pred_quant_Q13[ix[n][0] + 1] - low_Q13, SILK_FIX_CONST(0.5 / STEREO_QUANT_SUB_STEPS, 16));
        pred_Q13[n] = silk_SMLABB(low_Q13, step_Q13, 2 * ix[n][1] + 1);
    }

    /* Subtract second from first predictor (helps when actually applying these) */
    pred_Q13[0] -= pred_Q13[1];
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Decode mid-only flag */
void SilkDecoder::silk_stereo_decode_mid_only(int32_t* decode_only_mid /* O    Flag that only mid channel has been coded   */
) {
    /* Decode flag that only mid channel is coded */
    *decode_only_mid = rd.dec_icdf(silk_stereo_only_code_mid_iCDF, 8);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* helper function for NLSF2A(intermediate polynomial, Q[dd+1], vector of interleaved 2*cos(LSFs), QA_[d], polynomial order (= 1/2 * filter order)  ) */
void SilkDecoder::silk_NLSF2A_find_poly(int32_t* out, const int32_t* cLSF, int32_t dd) {
    int32_t k, n;
    int32_t ftmp;
    uint8_t QA16 = 16;

    out[0] = silk_LSHIFT(1, QA16);
    out[1] = -cLSF[0];
    for (k = 1; k < dd; k++) {
        ftmp = cLSF[2 * k]; /* QA16*/
        out[k + 1] = silk_LSHIFT(out[k - 1], 1) - (int32_t)silk_RSHIFT_ROUND64(silk_SMULL(ftmp, out[k]), QA16);
        for (n = k; n > 1; n--) { out[n] += out[n - 2] - (int32_t)silk_RSHIFT_ROUND64(silk_SMULL(ftmp, out[n - 1]), QA16); }
        out[1] -= ftmp;
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* compute whitening filter coefficients from normalized line spectral frequencies(monic whitening filter coefficients in Q12, normalized line spectral frequencies in Q15, filter order) */
void SilkDecoder::silk_NLSF2A(int16_t* a_Q12, const int16_t* NLSF, const int32_t d) {
    /* This ordering was found to maximize quality. It improves numerical accuracy of silk_NLSF2A_find_poly() compared to "standard" ordering. */
    const unsigned char  ordering16[16] = {0, 15, 8, 7, 4, 11, 12, 3, 2, 13, 10, 5, 6, 9, 14, 1};
    const unsigned char  ordering10[10] = {0, 9, 6, 3, 4, 5, 8, 1, 2, 7};
    const unsigned char* ordering;
    uint8_t              QA16 = 16;
    int32_t              k, i, dd;
    ps_ptr<int32_t>      cos_LSF_QA;
    cos_LSF_QA.alloc_array(SILK_MAX_ORDER_LPC);
    ps_ptr<int32_t> P;
    P.alloc_array(SILK_MAX_ORDER_LPC / 2 + 1);
    ps_ptr<int32_t> Q;
    Q.alloc_array(SILK_MAX_ORDER_LPC / 2 + 1);
    int32_t         Ptmp, Qtmp, f_int, f_frac, cos_val, delta;
    ps_ptr<int32_t> a32_QA1;
    a32_QA1.alloc_array(SILK_MAX_ORDER_LPC);

    assert(LSF_COS_TAB_SZ_FIX == 128);
    assert(d == 10 || d == 16);

    /* convert LSFs to 2*cos(LSF), using piecewise linear curve from table */
    ordering = d == 16 ? ordering16 : ordering10;
    for (k = 0; k < d; k++) {
        assert(NLSF[k] >= 0);

        /* f_int on a scale 0-127 (rounded down) */
        f_int = silk_RSHIFT(NLSF[k], 15 - 7);

        /* f_frac, range: 0..255 */
        f_frac = NLSF[k] - silk_LSHIFT(f_int, 15 - 7);

        assert(f_int >= 0);
        assert(f_int < LSF_COS_TAB_SZ_FIX);

        /* Read start and end value from table */
        cos_val = silk_LSFCosTab_FIX_Q12[f_int];             /* Q12 */
        delta = silk_LSFCosTab_FIX_Q12[f_int + 1] - cos_val; /* Q12, with a range of 0..200 */

        /* Linear interpolation */
        cos_LSF_QA[ordering[k]] = silk_RSHIFT_ROUND(silk_LSHIFT(cos_val, 8) + silk_MUL(delta, f_frac), 20 - QA16); /* QA16 */
    }

    dd = silk_RSHIFT(d, 1);

    /* generate even and odd polynomials using convolution */
    silk_NLSF2A_find_poly(P.get(), &cos_LSF_QA[0], dd);
    silk_NLSF2A_find_poly(Q.get(), &cos_LSF_QA[1], dd);

    /* convert even and odd polynomials to int32_t Q12 filter coefs */
    for (k = 0; k < dd; k++) {
        Ptmp = P[k + 1] + P[k];
        Qtmp = Q[k + 1] - Q[k];

        /* the Ptmp and Qtmp values at this stage need to fit in int32 */
        a32_QA1[k] = -Qtmp - Ptmp;
        a32_QA1[d - k - 1] = Qtmp - Ptmp;
    }

    /* Convert int32 coefficients to Q12 int16 coefs */
    silk_LPC_fit(a_Q12, a32_QA1.get(), 12, QA16 + 1, d);

    for (i = 0; silk_LPC_inverse_pred_gain(a_Q12, d) == 0 && i < MAX_LPC_STABILIZE_ITERATIONS; i++) {
        /* Prediction coefficients are (too close to) unstable; apply bandwidth expansion   */
        /* on the unscaled coefficients, convert to Q12 and measure again                   */
        silk_bwexpander_32(a32_QA1.get(), d, 65536 - silk_LSHIFT(2, i));
        for (k = 0; k < d; k++) { a_Q12[k] = (int16_t)silk_RSHIFT_ROUND(a32_QA1[k], QA16 + 1 - 12); /* QA16+1 -> Q12 */ }
    }
}
//----------------------------------------------------------------------------------------------------------------------
/* Decode side-information parameters from payload */
void SilkDecoder::silk_decode_indices(uint8_t n, int32_t FrameIndex, /* I    Frame number                                */
                                      int32_t decode_LBRR,           /* I    Flag indicating LBRR data is being decoded  */
                                      int32_t condCoding             /* I    The type of conditional coding to use       */
) {
    int32_t i, k, Ix;
    int32_t decode_absolute_lagIndex, delta_lagIndex;
    int16_t ec_ix[MAX_LPC_ORDER];
    uint8_t pred_Q8[MAX_LPC_ORDER];

    /*******************************************/
    /* Decode signal type and quantizer offset */
    /*******************************************/
    if (decode_LBRR || m_channel_state[n].VAD_flags[FrameIndex]) {
        Ix = rd.dec_icdf(silk_type_offset_VAD_iCDF, 8) + 2;
    } else {
        Ix = rd.dec_icdf(silk_type_offset_no_VAD_iCDF, 8);
    }
    m_channel_state[n].indices.signalType = (int8_t)silk_RSHIFT(Ix, 1);
    m_channel_state[n].indices.quantOffsetType = (int8_t)(Ix & 1);

    /****************/
    /* Decode gains */
    /****************/
    /* First subframe */
    if (condCoding == CODE_CONDITIONALLY) {
        /* Conditional coding */
        m_channel_state[n].indices.GainsIndices[0] = (int8_t)rd.dec_icdf(silk_delta_gain_iCDF, 8);
    } else {
        /* Independent coding, in two stages: MSB bits followed by 3 LSBs */
        m_channel_state[n].indices.GainsIndices[0] = (int8_t)silk_LSHIFT(rd.dec_icdf(silk_gain_iCDF[m_channel_state[n].indices.signalType], 8), 3);
        m_channel_state[n].indices.GainsIndices[0] += (int8_t)rd.dec_icdf(silk_uniform8_iCDF, 8);
    }

    /* Remaining subframes */
    for (i = 1; i < m_channel_state[n].nb_subfr; i++) {
        if (i < MAX_NB_SUBFR) { m_channel_state[n].indices.GainsIndices[i] = (int8_t)rd.dec_icdf(silk_delta_gain_iCDF, 8); }
    }
    /**********************/
    /* Decode LSF Indices */
    /**********************/
    m_channel_state[n].indices.NLSFIndices[0] = (int8_t)rd.dec_icdf(&m_channel_state[n].psNLSF_CB->CB1_iCDF[(m_channel_state[n].indices.signalType >> 1) * m_channel_state[n].psNLSF_CB->nVectors], 8);
    silk_NLSF_unpack(ec_ix, pred_Q8, m_channel_state[n].psNLSF_CB, m_channel_state[n].indices.NLSFIndices[0]);
    assert(m_channel_state[n].psNLSF_CB->order == m_channel_state[n].LPC_order);
    for (i = 0; i < m_channel_state[n].psNLSF_CB->order; i++) {
        Ix = rd.dec_icdf(&m_channel_state[n].psNLSF_CB->ec_iCDF[ec_ix[i]], 8);
        if (Ix == 0) {
            Ix -= rd.dec_icdf(silk_NLSF_EXT_iCDF, 8);
        } else if (Ix == 2 * NLSF_QUANT_MAX_AMPLITUDE) {
            Ix += rd.dec_icdf(silk_NLSF_EXT_iCDF, 8);
        }
        m_channel_state[n].indices.NLSFIndices[i + 1] = (int8_t)(Ix - NLSF_QUANT_MAX_AMPLITUDE);
    }

    /* Decode LSF interpolation factor */
    if (m_channel_state[n].nb_subfr == MAX_NB_SUBFR) {
        m_channel_state[n].indices.NLSFInterpCoef_Q2 = (int8_t)rd.dec_icdf(silk_NLSF_interpolation_factor_iCDF, 8);
    } else {
        m_channel_state[n].indices.NLSFInterpCoef_Q2 = 4;
    }

    if (m_channel_state[n].indices.signalType == TYPE_VOICED) {
        /*********************/
        /* Decode pitch lags */
        /*********************/
        /* Get lag index */
        decode_absolute_lagIndex = 1;
        if (condCoding == CODE_CONDITIONALLY && m_channel_state[n].ec_prevSignalType == TYPE_VOICED) {
            /* Decode Delta index */
            delta_lagIndex = (int16_t)rd.dec_icdf(silk_pitch_delta_iCDF, 8);
            if (delta_lagIndex > 0) {
                delta_lagIndex = delta_lagIndex - 9;
                m_channel_state[n].indices.lagIndex = (int16_t)(m_channel_state[n].ec_prevLagIndex + delta_lagIndex);
                decode_absolute_lagIndex = 0;
            }
        }
        if (decode_absolute_lagIndex) {
            /* Absolute decoding */
            m_channel_state[n].indices.lagIndex = (int16_t)rd.dec_icdf(silk_pitch_lag_iCDF, 8) * silk_RSHIFT(m_channel_state[n].fs_kHz, 1);
            m_channel_state[n].indices.lagIndex += (int16_t)rd.dec_icdf(m_channel_state[n].pitch_lag_low_bits_iCDF, 8);
        }
        m_channel_state[n].ec_prevLagIndex = m_channel_state[n].indices.lagIndex;

        /* Get countour index */
        m_channel_state[n].indices.contourIndex = (int8_t)rd.dec_icdf(m_channel_state[n].pitch_contour_iCDF, 8);

        /********************/
        /* Decode LTP gains */
        /********************/
        /* Decode PERIndex value */
        m_channel_state[n].indices.PERIndex = (int8_t)rd.dec_icdf(silk_LTP_per_index_iCDF, 8);

        for (k = 0; k < m_channel_state[n].nb_subfr; k++) {
            if (k < MAX_NB_SUBFR) { m_channel_state[n].indices.LTPIndex[k] = (int8_t)rd.dec_icdf(silk_LTP_gain_iCDF_ptrs[m_channel_state[n].indices.PERIndex], 8); }
        }

        /**********************/
        /* Decode LTP scaling */
        /**********************/
        if (condCoding == CODE_INDEPENDENTLY) {
            m_channel_state[n].indices.LTP_scaleIndex = (int8_t)rd.dec_icdf(silk_LTPscale_iCDF, 8);
        } else {
            m_channel_state[n].indices.LTP_scaleIndex = 0;
        }
    }
    m_channel_state[n].ec_prevSignalType = m_channel_state[n].indices.signalType;

    /***************/
    /* Decode seed */
    /***************/
    m_channel_state[n].indices.Seed = (int8_t)rd.dec_icdf(silk_uniform4_iCDF, 8);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_decode_parameters(uint8_t n, int32_t condCoding) {
    int32_t       i, k, Ix;
    int16_t       pNLSF_Q15[MAX_LPC_ORDER], pNLSF0_Q15[MAX_LPC_ORDER];
    const int8_t* cbk_ptr_Q7;

    /* Dequant Gains */
    silk_gains_dequant(m_silk_decoder_control->Gains_Q16, m_channel_state[n].indices.GainsIndices, &m_channel_state[n].LastGainIndex, condCoding == CODE_CONDITIONALLY, m_channel_state[n].nb_subfr);

    /****************/
    /* Decode NLSFs */
    /****************/
    silk_NLSF_decode(pNLSF_Q15, m_channel_state[n].indices.NLSFIndices, m_channel_state[n].psNLSF_CB);

    /* Convert NLSF parameters to AR prediction filter coefficients */
    silk_NLSF2A(m_silk_decoder_control->PredCoef_Q12[1], pNLSF_Q15, m_channel_state[n].LPC_order);

    /* If just reset, e.g., because internal Fs changed, do not allow interpolation */
    /* improves the case of packet loss in the first frame after a switch           */
    if (m_channel_state[n].first_frame_after_reset == 1) { m_channel_state[n].indices.NLSFInterpCoef_Q2 = 4; }

    if (m_channel_state[n].indices.NLSFInterpCoef_Q2 < 4) {
        /* Calculation of the interpolated NLSF0 vector from the interpolation factor, the previous NLSF1, and the current NLSF1 */
        for (i = 0; i < m_channel_state[n].LPC_order; i++) {
            pNLSF0_Q15[i] = m_channel_state[n].prevNLSF_Q15[i] + silk_RSHIFT(silk_MUL(m_channel_state[n].indices.NLSFInterpCoef_Q2, pNLSF_Q15[i] - m_channel_state[n].prevNLSF_Q15[i]), 2);
        }

        /* Convert NLSF parameters to AR prediction filter coefficients */
        silk_NLSF2A(m_silk_decoder_control->PredCoef_Q12[0], pNLSF0_Q15, m_channel_state[n].LPC_order);
    } else {
        /* Copy LPC coefficients for first half from second half */
        memcpy(m_silk_decoder_control->PredCoef_Q12[0], m_silk_decoder_control->PredCoef_Q12[1], m_channel_state[n].LPC_order * sizeof(int16_t));
    }
    memcpy(m_channel_state[n].prevNLSF_Q15, pNLSF_Q15, m_channel_state[n].LPC_order * sizeof(int16_t));

    /* After a packet loss do BWE of LPC coefs */
    if (m_channel_state[n].lossCnt) {
        silk_bwexpander(m_silk_decoder_control->PredCoef_Q12[0], m_channel_state[n].LPC_order, BWE_AFTER_LOSS_Q16);
        silk_bwexpander(m_silk_decoder_control->PredCoef_Q12[1], m_channel_state[n].LPC_order, BWE_AFTER_LOSS_Q16);
    }

    if (m_channel_state[n].indices.signalType == TYPE_VOICED) {
        /*********************/
        /* Decode pitch lags */
        /*********************/

        /* Decode pitch values */
        silk_decode_pitch(m_channel_state[n].indices.lagIndex, m_channel_state[n].indices.contourIndex, m_silk_decoder_control->pitchL, m_channel_state[n].fs_kHz, m_channel_state[n].nb_subfr);

        /* Decode Codebook Index */
        cbk_ptr_Q7 = silk_LTP_vq_ptrs_Q7[m_channel_state[n].indices.PERIndex]; /* set pointer to start of codebook */

        for (k = 0; k < m_channel_state[n].nb_subfr; k++) {
            Ix = m_channel_state[n].indices.LTPIndex[k];
            for (i = 0; i < LTP_ORDER; i++) { m_silk_decoder_control->LTPCoef_Q14[k * LTP_ORDER + i] = silk_LSHIFT(cbk_ptr_Q7[Ix * LTP_ORDER + i], 7); }
        }

        /**********************/
        /* Decode LTP scaling */
        /**********************/
        Ix = m_channel_state[n].indices.LTP_scaleIndex;
        m_silk_decoder_control->LTP_scale_Q14 = silk_LTPScales_table_Q14[Ix];
    } else {
        memset(m_silk_decoder_control->pitchL, 0, m_channel_state[n].nb_subfr * sizeof(int32_t));
        memset(m_silk_decoder_control->LTPCoef_Q14, 0, LTP_ORDER * m_channel_state[n].nb_subfr * sizeof(int16_t));
        m_channel_state[n].indices.PERIndex = 0;
        m_silk_decoder_control->LTP_scale_Q14 = 0;
    }
}
//----------------------------------------------------------------------------------------------------------------------
/* Decode quantization indices of excitation */

void SilkDecoder::silk_decode_pulses(int16_t       pulses[],        /* O    Excitation signal                           */
                                     const int32_t signalType,      /* I    Sigtype                                     */
                                     const int32_t quantOffsetType, /* I    quantOffsetType                             */
                                     const int32_t frame_length     /* I    Frame length                                */
) {
    int32_t         i, j, k, iter, abs_q, nLS, RateLevelIndex;
    ps_ptr<int32_t> sum_pulses;
    sum_pulses.alloc_array(MAX_NB_SHELL_BLOCKS);
    ps_ptr<int32_t> nLshifts;
    nLshifts.alloc_array(MAX_NB_SHELL_BLOCKS);
    int16_t*       pulses_ptr;
    const uint8_t* cdf_ptr;

    /*********************/
    /* Decode rate level */
    /*********************/
    RateLevelIndex = rd.dec_icdf(silk_rate_levels_iCDF[signalType >> 1], 8);

    /* Calculate number of shell blocks */
    assert(1 << LOG2_SHELL_CODEC_FRAME_LENGTH == SHELL_CODEC_FRAME_LENGTH);
    iter = silk_RSHIFT(frame_length, LOG2_SHELL_CODEC_FRAME_LENGTH);
    if (iter * SHELL_CODEC_FRAME_LENGTH < frame_length) {
        assert(frame_length == 12 * 10); /* Make sure only happens for 10 ms @ 12 kHz */
        iter++;
    }

    /***************************************************/
    /* Sum-Weighted-Pulses Decoding                    */
    /***************************************************/
    cdf_ptr = silk_pulses_per_block_iCDF[RateLevelIndex];
    for (i = 0; i < iter; i++) {
        nLshifts[i] = 0;
        sum_pulses[i] = rd.dec_icdf(cdf_ptr, 8);

        /* LSB indication */
        while (sum_pulses[i] == SILK_MAX_PULSES + 1) {
            nLshifts[i]++;
            /* When we've already got 10 LSBs, we shift the table to not allow (SILK_MAX_PULSES + 1) */
            sum_pulses[i] = rd.dec_icdf(silk_pulses_per_block_iCDF[N_RATE_LEVELS - 1] + (nLshifts[i] == 10), 8);
        }
    }

    /***************************************************/
    /* Shell decoding                                  */
    /***************************************************/
    for (i = 0; i < iter; i++) {
        if (sum_pulses[i] > 0) {
            silk_shell_decoder(&pulses[silk_SMULBB(i, SHELL_CODEC_FRAME_LENGTH)], sum_pulses[i]);
        } else {
            memset(&pulses[silk_SMULBB(i, SHELL_CODEC_FRAME_LENGTH)], 0, SHELL_CODEC_FRAME_LENGTH * sizeof(pulses[0]));
        }
    }

    /***************************************************/
    /* LSB Decoding                                    */
    /***************************************************/
    for (i = 0; i < iter; i++) {
        if (nLshifts[i] > 0) {
            nLS = nLshifts[i];
            pulses_ptr = &pulses[silk_SMULBB(i, SHELL_CODEC_FRAME_LENGTH)];
            for (k = 0; k < SHELL_CODEC_FRAME_LENGTH; k++) {
                abs_q = pulses_ptr[k];
                for (j = 0; j < nLS; j++) {
                    abs_q = silk_LSHIFT(abs_q, 1);
                    abs_q += rd.dec_icdf(silk_lsb_iCDF, 8);
                }
                pulses_ptr[k] = abs_q;
            }
            /* Mark the number of pulses non-zero for sign decoding. */
            sum_pulses[i] |= nLS << 5;
        }
    }

    /****************************************/
    /* Decode and add signs to pulse signal */
    /****************************************/
    silk_decode_signs(pulses, frame_length, signalType, quantOffsetType, sum_pulses.get());
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Set decoder sampling rate (Decoder state pointer , Sampling frequency (kHz) , API Sampling frequency (Hz))*/
int32_t SilkDecoder::silk_decoder_set_fs(uint8_t n, int32_t fs_kHz, int32_t fs_API_Hz) {
    int32_t frame_length, ret = 0;

    assert(fs_kHz == 8 || fs_kHz == 12 || fs_kHz == 16);
    assert(m_channel_state[n].nb_subfr == MAX_NB_SUBFR || m_channel_state[n].nb_subfr == MAX_NB_SUBFR / 2);

    /* New (sub)frame length */
    m_channel_state[n].subfr_length = silk_SMULBB(SUB_FRAME_LENGTH_MS, fs_kHz);
    frame_length = silk_SMULBB(m_channel_state[n].nb_subfr, m_channel_state[n].subfr_length);

    /* Initialize resampler when switching internal or external sampling frequency */
    if (m_channel_state[n].fs_kHz != fs_kHz || m_channel_state[n].fs_API_hz != fs_API_Hz) {
        /* Initialize the resampler for dec_API.c preparing resampling from fs_kHz to API_fs_Hz */

        ret += silk_resampler_init(n, silk_SMULBB(fs_kHz, 1000), fs_API_Hz, 0);

        m_channel_state[n].fs_API_hz = fs_API_Hz;
    }

    if (m_channel_state[n].fs_kHz != fs_kHz || frame_length != m_channel_state[n].frame_length) {
        if (fs_kHz == 8) {
            if (m_channel_state[n].nb_subfr == MAX_NB_SUBFR) {
                m_channel_state[n].pitch_contour_iCDF = silk_pitch_contour_NB_iCDF;
            } else {
                m_channel_state[n].pitch_contour_iCDF = silk_pitch_contour_10_ms_NB_iCDF;
            }
        } else {
            if (m_channel_state[n].nb_subfr == MAX_NB_SUBFR) {
                m_channel_state[n].pitch_contour_iCDF = silk_pitch_contour_iCDF;
            } else {
                m_channel_state[n].pitch_contour_iCDF = silk_pitch_contour_10_ms_iCDF;
            }
        }
        if (m_channel_state[n].fs_kHz != fs_kHz) {
            m_channel_state[n].ltp_mem_length = silk_SMULBB(LTP_MEM_LENGTH_MS, fs_kHz);
            if (fs_kHz == 8 || fs_kHz == 12) {
                m_channel_state[n].LPC_order = MIN_LPC_ORDER;
                m_channel_state[n].psNLSF_CB = &silk_NLSF_CB_NB_MB;
            } else {
                m_channel_state[n].LPC_order = MAX_LPC_ORDER;
                m_channel_state[n].psNLSF_CB = &silk_NLSF_CB_WB;
            }
            if (fs_kHz == 16) {
                m_channel_state[n].pitch_lag_low_bits_iCDF = silk_uniform8_iCDF;
            } else if (fs_kHz == 12) {
                m_channel_state[n].pitch_lag_low_bits_iCDF = silk_uniform6_iCDF;
            } else if (fs_kHz == 8) {
                m_channel_state[n].pitch_lag_low_bits_iCDF = silk_uniform4_iCDF;
            } else {
                /* unsupported sampling rate */
                assert(0);
            }
            m_channel_state[n].first_frame_after_reset = 1;
            m_channel_state[n].lagPrev = 100;
            m_channel_state[n].LastGainIndex = 10;
            m_channel_state[n].prevSignalType = TYPE_NO_VOICE_ACTIVITY;
            memset(m_channel_state[n].outBuf, 0, sizeof(m_channel_state[n].outBuf));
            memset(m_channel_state[n].sLPC_Q14_buf, 0, sizeof(m_channel_state[n].sLPC_Q14_buf));
        }

        m_channel_state[n].fs_kHz = fs_kHz;
        m_channel_state[n].frame_length = frame_length;
    }

    /* Check that settings are valid */
    assert(m_channel_state[n].frame_length > 0 && m_channel_state[n].frame_length <= MAX_FRAME_LENGTH);

    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Encode quantization indices of excitation */
int32_t SilkDecoder::combine_and_check(                            /* return ok                           */
                                       int32_t*       pulses_comb, /* O                                   */
                                       const int32_t* pulses_in,   /* I                                   */
                                       int32_t        max_pulses,  /* I    max value for sum of pulses    */
                                       int32_t        len          /* I    number of output values        */
) {
    int32_t k, sum;

    for (k = 0; k < len; k++) {
        sum = pulses_in[2 * k] + pulses_in[2 * k + 1];
        if (sum > max_pulses) { return 1; }
        pulses_comb[k] = sum;
    }

    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_quant_LTP_gains(int16_t       B_Q14[MAX_NB_SUBFR * LTP_ORDER],              /* O    Quantized LTP gains             */
                                       int8_t        cbk_index[MAX_NB_SUBFR],                      /* O    Codebook Index                  */
                                       int8_t*       periodicity_index,                            /* O    Periodicity Index               */
                                       int32_t*      sum_log_gain_Q7,                              /* I/O  Cumulative max prediction gain  */
                                       int32_t*      pred_gain_dB_Q7,                              /* O    LTP prediction gain             */
                                       const int32_t XX_Q17[MAX_NB_SUBFR * LTP_ORDER * LTP_ORDER], /* I    Correlation matrix in Q18       */
                                       const int32_t xX_Q17[MAX_NB_SUBFR * LTP_ORDER],             /* I    Correlation vector in Q18       */
                                       const int32_t subfr_len,                                    /* I    Number of samples per subframe  */
                                       const int32_t nb_subfr                                      /* I    Number of subframes             */
) {
    int32_t        j, k, cbk_size;
    int8_t         temp_idx[MAX_NB_SUBFR];
    const uint8_t* cl_ptr_Q5;
    const int8_t*  cbk_ptr_Q7;
    const uint8_t* cbk_gain_ptr_Q7;
    const int32_t *XX_Q17_ptr, *xX_Q17_ptr;
    int32_t        res_nrg_Q15_subfr, res_nrg_Q15, rate_dist_Q7_subfr, rate_dist_Q7, min_rate_dist_Q7;
    int32_t        sum_log_gain_tmp_Q7, best_sum_log_gain_Q7, max_gain_Q7;
    int32_t        gain_Q7;

    /***************************************************/
    /* iterate over different codebooks with different */
    /* rates/distortions, and choose best */
    /***************************************************/
    min_rate_dist_Q7 = silk_int32_MAX;
    best_sum_log_gain_Q7 = 0;
    for (k = 0; k < 3; k++) {
        /* Safety margin for pitch gain control, to take into account factors
           such as state rescaling/rewhitening. */
        int32_t gain_safety = SILK_FIX_CONST(0.4, 7);

        cl_ptr_Q5 = silk_LTP_gain_BITS_Q5_ptrs[k];
        cbk_ptr_Q7 = silk_LTP_vq_ptrs_Q7[k];
        cbk_gain_ptr_Q7 = silk_LTP_vq_gain_ptrs_Q7[k];
        cbk_size = silk_LTP_vq_sizes[k];

        /* Set up pointers to first subframe */
        XX_Q17_ptr = XX_Q17;
        xX_Q17_ptr = xX_Q17;

        res_nrg_Q15 = 0;
        rate_dist_Q7 = 0;
        sum_log_gain_tmp_Q7 = *sum_log_gain_Q7;
        for (j = 0; j < nb_subfr; j++) {
            max_gain_Q7 = silk_log2lin((SILK_FIX_CONST(MAX_SUM_LOG_GAIN_DB / 6.0L, 7) - sum_log_gain_tmp_Q7) + SILK_FIX_CONST(7, 7)) - gain_safety;
            silk_VQ_WMat_EC(&temp_idx[j],        /* O    index of best codebook vector                           */
                            &res_nrg_Q15_subfr,  /* O    residual energy                                         */
                            &rate_dist_Q7_subfr, /* O    best weighted quantization error + mu * rate            */
                            &gain_Q7,            /* O    sum of absolute LTP coefficients                        */
                            XX_Q17_ptr,          /* I    correlation matrix                                      */
                            xX_Q17_ptr,          /* I    correlation vector                                      */
                            cbk_ptr_Q7,          /* I    codebook                                                */
                            cbk_gain_ptr_Q7,     /* I    codebook effective gains                                */
                            cl_ptr_Q5,           /* I    code length for each codebook vector                    */
                            subfr_len,           /* I    number of samples per subframe                          */
                            max_gain_Q7,         /* I    maximum sum of absolute LTP coefficients                */
                            cbk_size             /* I    number of vectors in codebook                           */
            );

            res_nrg_Q15 = silk_ADD_POS_SAT32(res_nrg_Q15, res_nrg_Q15_subfr);
            rate_dist_Q7 = silk_ADD_POS_SAT32(rate_dist_Q7, rate_dist_Q7_subfr);
            sum_log_gain_tmp_Q7 = silk_max(0, sum_log_gain_tmp_Q7 + silk_lin2log(gain_safety + gain_Q7) - SILK_FIX_CONST(7, 7));

            XX_Q17_ptr += LTP_ORDER * LTP_ORDER;
            xX_Q17_ptr += LTP_ORDER;
        }

        if (rate_dist_Q7 <= min_rate_dist_Q7) {
            min_rate_dist_Q7 = rate_dist_Q7;
            *periodicity_index = (int8_t)k;
            memcpy(cbk_index, temp_idx, nb_subfr * sizeof(int8_t));
            best_sum_log_gain_Q7 = sum_log_gain_tmp_Q7;
        }
    }

    cbk_ptr_Q7 = silk_LTP_vq_ptrs_Q7[*periodicity_index];
    for (j = 0; j < nb_subfr; j++) {
        for (k = 0; k < LTP_ORDER; k++) { B_Q14[j * LTP_ORDER + k] = silk_LSHIFT(cbk_ptr_Q7[cbk_index[j] * LTP_ORDER + k], 7); }
    }

    if (nb_subfr == 2) {
        res_nrg_Q15 = silk_RSHIFT32(res_nrg_Q15, 1);
    } else {
        res_nrg_Q15 = silk_RSHIFT32(res_nrg_Q15, 2);
    }

    *sum_log_gain_Q7 = best_sum_log_gain_Q7;
    *pred_gain_dB_Q7 = (int32_t)silk_SMULBB(-3, silk_lin2log(res_nrg_Q15) - (15 << 7));
}
//----------------------------------------------------------------------------------------------------------------------
void SilkDecoder::decode_split(int16_t*       p_child1,   /* O    pulse amplitude of first child subframe     */
                               int16_t*       p_child2,   /* O    pulse amplitude of second child subframe    */
                               const int32_t  p,          /* I    pulse amplitude of current subframe         */
                               const uint8_t* shell_table /* I    table of shell cdfs                         */
) {
    if (p > 0) {
        p_child1[0] = rd.dec_icdf(&shell_table[silk_shell_code_table_offsets[p]], 8);
        p_child2[0] = p - p_child1[0];
    } else {
        p_child1[0] = 0;
        p_child2[0] = 0;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Shell decoder, operates on one shell code frame of 16 pulses */
void SilkDecoder::silk_shell_decoder(int16_t*      pulses0, /* O    data: nonnegative pulse amplitudes          */
                                     const int32_t pulses4  /* I    number of pulses per pulse-subframe         */
) {
    int16_t pulses3[2], pulses2[4], pulses1[8];

    /* this function operates on one shell code frame of 16 pulses */
    assert(SHELL_CODEC_FRAME_LENGTH == 16);
    decode_split(&pulses3[0], &pulses3[1], pulses4, silk_shell_code_table3);
    decode_split(&pulses2[0], &pulses2[1], pulses3[0], silk_shell_code_table2);
    decode_split(&pulses1[0], &pulses1[1], pulses2[0], silk_shell_code_table1);
    decode_split(&pulses0[0], &pulses0[1], pulses1[0], silk_shell_code_table0);
    decode_split(&pulses0[2], &pulses0[3], pulses1[1], silk_shell_code_table0);
    decode_split(&pulses1[2], &pulses1[3], pulses2[1], silk_shell_code_table1);
    decode_split(&pulses0[4], &pulses0[5], pulses1[2], silk_shell_code_table0);
    decode_split(&pulses0[6], &pulses0[7], pulses1[3], silk_shell_code_table0);
    decode_split(&pulses2[2], &pulses2[3], pulses3[1], silk_shell_code_table2);
    decode_split(&pulses1[4], &pulses1[5], pulses2[2], silk_shell_code_table1);
    decode_split(&pulses0[8], &pulses0[9], pulses1[4], silk_shell_code_table0);
    decode_split(&pulses0[10], &pulses0[11], pulses1[5], silk_shell_code_table0);
    decode_split(&pulses1[6], &pulses1[7], pulses2[3], silk_shell_code_table1);
    decode_split(&pulses0[12], &pulses0[13], pulses1[6], silk_shell_code_table0);
    decode_split(&pulses0[14], &pulses0[15], pulses1[7], silk_shell_code_table0);
}
//----------------------------------------------------------------------------------------------------------------------
/* Quantize mid/side predictors */
void SilkDecoder::silk_stereo_quant_pred(int32_t pred_Q13[], /* I/O  Predictors (out: quantized)                 */
                                         int8_t  ix[2][3]    /* O    Quantization indices                        */
) {
    int32_t i, j, n;
    int32_t low_Q13, step_Q13, lvl_Q13, err_min_Q13, err_Q13, quant_pred_Q13 = 0;

    /* Quantize */
    for (n = 0; n < 2; n++) {
        /* Brute-force search over quantization levels */
        err_min_Q13 = silk_int32_MAX;
        for (i = 0; i < STEREO_QUANT_TAB_SIZE - 1; i++) {
            low_Q13 = silk_stereo_pred_quant_Q13[i];
            step_Q13 = silk_SMULWB(silk_stereo_pred_quant_Q13[i + 1] - low_Q13, SILK_FIX_CONST(0.5 / STEREO_QUANT_SUB_STEPS, 16));
            for (j = 0; j < STEREO_QUANT_SUB_STEPS; j++) {
                lvl_Q13 = silk_SMLABB(low_Q13, step_Q13, 2 * j + 1);
                err_Q13 = silk_abs(pred_Q13[n] - lvl_Q13);
                if (err_Q13 < err_min_Q13) {
                    err_min_Q13 = err_Q13;
                    quant_pred_Q13 = lvl_Q13;
                    ix[n][0] = i;
                    ix[n][1] = j;
                } else {
                    /* Error increasing, so we're past the optimum */
                    goto done;
                }
            }
        }
    done:
        ix[n][2] = silk_DIV32_16(ix[n][0], 3);
        ix[n][0] -= ix[n][2] * 3;
        pred_Q13[n] = quant_pred_Q13;
    }

    /* Subtract second from first predictor (helps when actually applying these) */
    pred_Q13[0] -= pred_Q13[1];
}
//----------------------------------------------------------------------------------------------------------------------
/* Helper function, interpolates the filter taps */
void SilkDecoder::silk_LP_interpolate_filter_taps(int32_t B_Q28[TRANSITION_NB], int32_t A_Q28[TRANSITION_NA], const int32_t ind, const int32_t fac_Q16) {
    int32_t nb, na;

    if (ind < TRANSITION_INT_NUM - 1) {
        if (fac_Q16 > 0) {
            if (fac_Q16 < 32768) { /* fac_Q16 is in range of a 16-bit int */
                /* Piece-wise linear interpolation of B and A */
                for (nb = 0; nb < TRANSITION_NB; nb++) {
                    B_Q28[nb] = silk_SMLAWB(silk_Transition_LP_B_Q28[ind][nb], silk_Transition_LP_B_Q28[ind + 1][nb] - silk_Transition_LP_B_Q28[ind][nb], fac_Q16);
                }
                for (na = 0; na < TRANSITION_NA; na++) {
                    A_Q28[na] = silk_SMLAWB(silk_Transition_LP_A_Q28[ind][na], silk_Transition_LP_A_Q28[ind + 1][na] - silk_Transition_LP_A_Q28[ind][na], fac_Q16);
                }
            } else { /* ( fac_Q16 - ( 1 << 16 ) ) is in range of a 16-bit int */
                assert(fac_Q16 - (1 << 16) == silk_SAT16(fac_Q16 - (1 << 16)));
                /* Piece-wise linear interpolation of B and A */
                for (nb = 0; nb < TRANSITION_NB; nb++) {
                    B_Q28[nb] = silk_SMLAWB(silk_Transition_LP_B_Q28[ind + 1][nb], silk_Transition_LP_B_Q28[ind + 1][nb] - silk_Transition_LP_B_Q28[ind][nb], fac_Q16 - ((int32_t)1 << 16));
                }
                for (na = 0; na < TRANSITION_NA; na++) {
                    A_Q28[na] = silk_SMLAWB(silk_Transition_LP_A_Q28[ind + 1][na], silk_Transition_LP_A_Q28[ind + 1][na] - silk_Transition_LP_A_Q28[ind][na], fac_Q16 - ((int32_t)1 << 16));
                }
            }
        } else {
            memcpy(B_Q28, silk_Transition_LP_B_Q28[ind], TRANSITION_NB * sizeof(int32_t));
            memcpy(A_Q28, silk_Transition_LP_A_Q28[ind], TRANSITION_NA * sizeof(int32_t));
        }
    } else {
        memcpy(B_Q28, silk_Transition_LP_B_Q28[TRANSITION_INT_NUM - 1], TRANSITION_NB * sizeof(int32_t));
        memcpy(A_Q28, silk_Transition_LP_A_Q28[TRANSITION_INT_NUM - 1], TRANSITION_NA * sizeof(int32_t));
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Low-pass filter with variable cutoff frequency based on  */
/* piece-wise linear interpolation between elliptic filters */
/* Start by setting psEncC->mode <> 0;                      */
/* Deactivate by setting psEncC->mode = 0;                  */
void SilkDecoder::silk_LP_variable_cutoff(silk_LP_state_t* psLP,        /* I/O  LP filter state                             */
                                          int16_t*         frame,       /* I/O  Low-pass filtered output signal             */
                                          const int32_t    frame_length /* I    Frame length                                */
) {
    int32_t B_Q28[TRANSITION_NB], A_Q28[TRANSITION_NA], fac_Q16 = 0;
    int32_t ind = 0;

    assert(psLP->transition_frame_no >= 0 && psLP->transition_frame_no <= TRANSITION_FRAMES);

    /* Run filter if needed */
    if (psLP->mode != 0) {
        /* Calculate index and interpolation factor for interpolation */
#if (TRANSITION_INT_STEPS == 64)
        fac_Q16 = silk_LSHIFT(TRANSITION_FRAMES - psLP->transition_frame_no, 16 - 6);
#else
        fac_Q16 = silk_DIV32_16(silk_LSHIFT(TRANSITION_FRAMES - psLP->transition_frame_no, 16), TRANSITION_FRAMES);
#endif
        ind = silk_RSHIFT(fac_Q16, 16);
        fac_Q16 -= silk_LSHIFT(ind, 16);

        assert(ind >= 0);
        assert(ind < TRANSITION_INT_NUM);

        /* Interpolate filter coefficients */
        silk_LP_interpolate_filter_taps(B_Q28, A_Q28, ind, fac_Q16);

        /* Update transition frame number for next frame */
        psLP->transition_frame_no = silk_LIMIT(psLP->transition_frame_no + psLP->mode, 0, TRANSITION_FRAMES);

        /* ARMA low-pass filtering */
        assert(TRANSITION_NB == 3 && TRANSITION_NA == 2);
        silk_biquad_alt_stride1(frame, B_Q28, A_Q28, psLP->In_LP_State, frame, frame_length);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Generates excitation for CNG LPC synthesis */
void SilkDecoder::silk_CNG_exc(int32_t  exc_Q14[],     /* O    CNG excitation signal Q10                   */
                               int32_t  exc_buf_Q14[], /* I    Random samples buffer Q10                   */
                               int32_t  length,        /* I    Length                                      */
                               int32_t* rand_seed      /* I/O  Seed to random index generator              */
) {
    int32_t seed;
    int32_t i, idx, exc_mask;

    exc_mask = CNG_BUF_MASK_MAX;
    while (exc_mask > length) { exc_mask = silk_RSHIFT(exc_mask, 1); }

    seed = *rand_seed;
    for (i = 0; i < length; i++) {
        seed = silk_RAND(seed);
        idx = (int32_t)(silk_RSHIFT(seed, 24) & exc_mask);
        assert(idx >= 0);
        assert(idx <= CNG_BUF_MASK_MAX);
        exc_Q14[i] = exc_buf_Q14[idx];
    }
    *rand_seed = seed;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_CNG_Reset(uint8_t n) {
    int32_t i, NLSF_step_Q15, NLSF_acc_Q15;

    NLSF_step_Q15 = silk_DIV32_16(silk_int16_MAX, m_channel_state[n].LPC_order + 1);
    NLSF_acc_Q15 = 0;
    for (i = 0; i < m_channel_state[n].LPC_order; i++) {
        NLSF_acc_Q15 += NLSF_step_Q15;
        m_channel_state[n].sCNG.CNG_smth_NLSF_Q15[i] = NLSF_acc_Q15;
    }
    m_channel_state[n].sCNG.CNG_smth_Gain_Q16 = 0;
    m_channel_state[n].sCNG.rand_seed = 3176576;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Updates CNG estimate, and applies the CNG when packet was lost   */
void SilkDecoder::silk_CNG(uint8_t n, int16_t frame[], int32_t length) {
    int32_t            i, subfr;
    int32_t            LPC_pred_Q10, max_Gain_Q16, gain_Q16, gain_Q10;
    int16_t            A_Q12[MAX_LPC_ORDER];
    silk_CNG_struct_t* psCNG = &m_channel_state[n].sCNG;

    if (m_channel_state[n].fs_kHz != psCNG->fs_kHz) {
        /* Reset state */
        silk_CNG_Reset(n);

        psCNG->fs_kHz = m_channel_state[n].fs_kHz;
    }
    if (m_channel_state[n].lossCnt == 0 && m_channel_state[n].prevSignalType == TYPE_NO_VOICE_ACTIVITY) {
        /* Update CNG parameters */

        /* Smoothing of LSF's  */
        for (i = 0; i < m_channel_state[n].LPC_order; i++) {
            psCNG->CNG_smth_NLSF_Q15[i] += silk_SMULWB((int32_t)m_channel_state[n].prevNLSF_Q15[i] - (int32_t)psCNG->CNG_smth_NLSF_Q15[i], CNG_NLSF_SMTH_Q16);
        }
        /* Find the subframe with the highest gain */
        max_Gain_Q16 = 0;
        subfr = 0;
        for (i = 0; i < m_channel_state[n].nb_subfr; i++) {
            if (m_silk_decoder_control->Gains_Q16[i] > max_Gain_Q16) {
                max_Gain_Q16 = m_silk_decoder_control->Gains_Q16[i];
                subfr = i;
            }
        }
        /* Update CNG excitation buffer with excitation from this subframe */
        memmove(&psCNG->CNG_exc_buf_Q14[m_channel_state[n].subfr_length], psCNG->CNG_exc_buf_Q14, (m_channel_state[n].nb_subfr - 1) * m_channel_state[n].subfr_length * sizeof(int32_t));
        memcpy(psCNG->CNG_exc_buf_Q14, &m_channel_state[n].exc_Q14[subfr * m_channel_state[n].subfr_length], m_channel_state[n].subfr_length * sizeof(int32_t));

        /* Smooth gains */
        for (i = 0; i < m_channel_state[n].nb_subfr; i++) { psCNG->CNG_smth_Gain_Q16 += silk_SMULWB(m_silk_decoder_control->Gains_Q16[i] - psCNG->CNG_smth_Gain_Q16, CNG_GAIN_SMTH_Q16); }
    }
    /* Add CNG when packet is lost or during DTX */
    if (m_channel_state[n].lossCnt) {
        ps_ptr<int32_t> CNG_sig_Q14;
        CNG_sig_Q14.alloc_array(length + MAX_LPC_ORDER);

        /* Generate CNG excitation */
        gain_Q16 = silk_SMULWW(m_channel_state[n].sPLC.randScale_Q14, m_channel_state[n].sPLC.prevGain_Q16[1]);
        if (gain_Q16 >= (1 << 21) || psCNG->CNG_smth_Gain_Q16 > (1 << 23)) {
            gain_Q16 = silk_SMULTT(gain_Q16, gain_Q16);
            gain_Q16 = silk_SUB_LSHIFT32(silk_SMULTT(psCNG->CNG_smth_Gain_Q16, psCNG->CNG_smth_Gain_Q16), gain_Q16, 5);
            gain_Q16 = silk_LSHIFT32(silk_SQRT_APPROX(gain_Q16), 16);
        } else {
            gain_Q16 = silk_SMULWW(gain_Q16, gain_Q16);
            gain_Q16 = silk_SUB_LSHIFT32(silk_SMULWW(psCNG->CNG_smth_Gain_Q16, psCNG->CNG_smth_Gain_Q16), gain_Q16, 5);
            gain_Q16 = silk_LSHIFT32(silk_SQRT_APPROX(gain_Q16), 8);
        }
        gain_Q10 = silk_RSHIFT(gain_Q16, 6);

        silk_CNG_exc(CNG_sig_Q14.get() + MAX_LPC_ORDER, psCNG->CNG_exc_buf_Q14, length, &psCNG->rand_seed);

        /* Convert CNG NLSF to filter representation */
        silk_NLSF2A(A_Q12, psCNG->CNG_smth_NLSF_Q15, m_channel_state[n].LPC_order);

        /* Generate CNG signal, by synthesis filtering */
        memcpy(CNG_sig_Q14.get(), psCNG->CNG_synth_state, MAX_LPC_ORDER * sizeof(int32_t));
        assert(m_channel_state[n].LPC_order == 10 || m_channel_state[n].LPC_order == 16);
        for (i = 0; i < length; i++) {
            /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
            LPC_pred_Q10 = silk_RSHIFT(m_channel_state[n].LPC_order, 1);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 1], A_Q12[0]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 2], A_Q12[1]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 3], A_Q12[2]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 4], A_Q12[3]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 5], A_Q12[4]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 6], A_Q12[5]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 7], A_Q12[6]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 8], A_Q12[7]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 9], A_Q12[8]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 10], A_Q12[9]);
            if (m_channel_state[n].LPC_order == 16) {
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 11], A_Q12[10]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 12], A_Q12[11]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 13], A_Q12[12]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 14], A_Q12[13]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 15], A_Q12[14]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, CNG_sig_Q14[MAX_LPC_ORDER + i - 16], A_Q12[15]);
            }

            /* Update states */
            CNG_sig_Q14[MAX_LPC_ORDER + i] = silk_ADD_SAT32(CNG_sig_Q14[MAX_LPC_ORDER + i], silk_LSHIFT_SAT32(LPC_pred_Q10, 4));

            /* Scale with Gain and add to input signal */
            frame[i] = (int16_t)silk_ADD_SAT16(frame[i], silk_SAT16(silk_RSHIFT_ROUND(silk_SMULWW(CNG_sig_Q14[MAX_LPC_ORDER + i], gain_Q10), 8)));
        }
        std::ranges::copy(CNG_sig_Q14.span().subspan(length, MAX_LPC_ORDER), psCNG->CNG_synth_state);
        // memcpy(psCNG->CNG_synth_state, &CNG_sig_Q14[length], MAX_LPC_ORDER * sizeof(int32_t));
    } else {
        memset(psCNG->CNG_synth_state, 0, m_channel_state[n].LPC_order * sizeof(int32_t));
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Decodes signs of excitation */
void SilkDecoder::silk_decode_signs(int16_t       pulses[],                       /* I/O  pulse signal                                */
                                    int32_t       length,                         /* I    length of input                             */
                                    const int32_t signalType,                     /* I    Signal type                                 */
                                    const int32_t quantOffsetType,                /* I    Quantization offset type                    */
                                    const int32_t sum_pulses[MAX_NB_SHELL_BLOCKS] /* I    Sum of absolute pulses per block */
) {
    int32_t        i, j, p;
    uint8_t        icdf[2];
    int16_t*       q_ptr;
    const uint8_t* icdf_ptr;

    icdf[1] = 0;
    q_ptr = pulses;
    i = silk_SMULBB(7, silk_ADD_LSHIFT(quantOffsetType, signalType, 1));
    icdf_ptr = &silk_sign_iCDF[i];
    length = silk_RSHIFT(length + SHELL_CODEC_FRAME_LENGTH / 2, LOG2_SHELL_CODEC_FRAME_LENGTH);
    for (i = 0; i < length; i++) {
        p = sum_pulses[i];
        if (p > 0) {
            icdf[0] = icdf_ptr[silk_min(p & 0x1F, 6)];
            for (j = 0; j < SHELL_CODEC_FRAME_LENGTH; j++) {
                if (q_ptr[j] > 0) {
                    /* attach sign */
                    /* implementation with shift, subtraction, multiplication */
                    q_ptr[j] *= silk_dec_map(rd.dec_icdf(icdf, 8));
                }
            }
        }
        q_ptr += SHELL_CODEC_FRAME_LENGTH;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_setRawParams(uint8_t channels, uint8_t API_channels, uint8_t payloadSize_ms, uint32_t internalSampleRate, uint32_t API_samleRate) {
    m_channelsInternal = channels;
    m_API_channels = API_channels;
    m_payloadSize_ms = payloadSize_ms;
    m_silk_internalSampleRate = internalSampleRate;
    m_API_sampleRate = API_samleRate;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t SilkDecoder::silk_getPrevPitchLag() {
    return m_prevPitchLag;
}
//----------------------------------------------------------------------------------------------------------------------
/* Decode a frame */
int32_t SilkDecoder::silk_Decode(/* O    Returns error code                              */

                                 int32_t  lostFlag,      /* I    0: no loss, 1 loss, 2 decode fec                */
                                 int32_t  newPacketFlag, /* I    Indicates first decoder call for this packet    */
                                 int16_t* samplesOut,    /* O    Decoded output speech vector                    */
                                 int32_t* nSamplesOut    /* O    Number of samples decoded                       */
) {

    uint8_t  n = 0;
    int32_t  i, decode_only_middle = 0, ret = SILK_NO_ERROR;
    int32_t  nSamplesOutDec = 0, LBRR_symbol;
    int16_t* samplesOut1_tmp[2];
    int32_t  MS_pred_Q13[2] = {0};
    int16_t* resample_out_ptr;
    // silk_decoder_state_t *m_channel_state = m_channel_state;
    int32_t has_side;
    int32_t stereo_to_mono;
    int     delay_stack_alloc;

    assert(m_silk_DecControlStruct->nChannelsInternal == 1 || m_silk_DecControlStruct->nChannelsInternal == 2);

    /**********************************/
    /* Test if first frame in payload */
    /**********************************/
    if (newPacketFlag) {
        for (n = 0; n < m_silk_DecControlStruct->nChannelsInternal; n++) { m_channel_state[n].nFramesDecoded = 0; /* Used to count frames in packet */ }
    }

    /* If Mono -> Stereo transition in bitstream: init state of second channel */
    if (m_silk_DecControlStruct->nChannelsInternal > m_silk_decoder.get()->nChannelsInternal) { ret += silk_init_decoder(1); }

    stereo_to_mono = m_silk_DecControlStruct->nChannelsInternal == 1 && m_silk_decoder.get()->nChannelsInternal == 2 && (m_silk_internalSampleRate == 1000 * m_channel_state[0].fs_kHz);

    if (m_channel_state[0].nFramesDecoded == 0) {
        for (n = 0; n < m_silk_DecControlStruct->nChannelsInternal; n++) {
            int32_t fs_kHz_dec;
            if (m_payloadSize_ms == 0) {
                /* Assuming packet loss, use 10 ms */
                m_channel_state[n].nFramesPerPacket = 1;
                m_channel_state[n].nb_subfr = 2;
            } else if (m_payloadSize_ms == 10) {
                m_channel_state[n].nFramesPerPacket = 1;
                m_channel_state[n].nb_subfr = 2;
            } else if (m_payloadSize_ms == 20) {
                m_channel_state[n].nFramesPerPacket = 1;
                m_channel_state[n].nb_subfr = 4;
            } else if (m_payloadSize_ms == 40) {
                m_channel_state[n].nFramesPerPacket = 2;
                m_channel_state[n].nb_subfr = 4;
            } else if (m_payloadSize_ms == 60) {
                m_channel_state[n].nFramesPerPacket = 3;
                m_channel_state[n].nb_subfr = 4;
            } else {
                OPUS_LOG_ERROR("Opus SILK: invalid frame size");
                return OPUS_BAD_ARG;
            }
            fs_kHz_dec = (m_silk_internalSampleRate >> 10) + 1;
            if (fs_kHz_dec != 8 && fs_kHz_dec != 12 && fs_kHz_dec != 16) {
                OPUS_LOG_ERROR("Opus SILK, invalid sampling frequency: %i", fs_kHz_dec);
                return OPUS_BAD_ARG;
            }
            ret += silk_decoder_set_fs(n, fs_kHz_dec, m_silk_DecControlStruct->API_sampleRate);
        }
    }

    if (m_silk_DecControlStruct->nChannelsAPI == 2 && m_silk_DecControlStruct->nChannelsInternal == 2 && (m_silk_decoder->nChannelsAPI == 1 || m_silk_decoder->nChannelsInternal == 1)) {
        memset(m_silk_decoder->sStereo.pred_prev_Q13, 0, sizeof(m_silk_decoder->sStereo.pred_prev_Q13));
        memset(m_silk_decoder->sStereo.sSide, 0, sizeof(m_silk_decoder->sStereo.sSide));
        memcpy(&m_resampler_state[n], &m_resampler_state[n], sizeof(silk_resampler_state_struct_t));
    }
    m_silk_decoder->nChannelsAPI = m_silk_DecControlStruct->nChannelsAPI;
    m_silk_decoder->nChannelsInternal = m_silk_DecControlStruct->nChannelsInternal;

    if (m_silk_DecControlStruct->API_sampleRate > (int32_t)MAX_API_FS_KHZ * 1000 || m_silk_DecControlStruct->API_sampleRate < 8000) {
        OPUS_LOG_ERROR("Opus SILK, invalid sampling rate: %i", m_silk_DecControlStruct->API_sampleRate);
        ret = OPUS_BAD_ARG;

        return (ret);
    }

    if (lostFlag != FLAG_PACKET_LOST && m_channel_state[0].nFramesDecoded == 0) {
        /* First decoder call for this payload */
        /* Decode VAD flags and LBRR flag */
        for (n = 0; n < m_silk_DecControlStruct->nChannelsInternal; n++) {
            for (i = 0; i < m_channel_state[n].nFramesPerPacket; i++) { m_channel_state[n].VAD_flags[i] = rd.dec_bit_logp(1); }
            m_channel_state[n].LBRR_flag = rd.dec_bit_logp(1);
        }
        /* Decode LBRR flags */
        for (n = 0; n < m_silk_DecControlStruct->nChannelsInternal; n++) {
            memset(m_channel_state[n].LBRR_flags, 0, sizeof(m_channel_state[n].LBRR_flags));
            if (m_channel_state[n].LBRR_flag) {
                if (m_channel_state[n].nFramesPerPacket == 1) {
                    m_channel_state[n].LBRR_flags[0] = 1;
                } else {
                    LBRR_symbol = rd.dec_icdf(silk_LBRR_flags_iCDF_ptr[m_channel_state[n].nFramesPerPacket - 2], 8) + 1;
                    for (i = 0; i < m_channel_state[n].nFramesPerPacket; i++) { m_channel_state[n].LBRR_flags[i] = silk_RSHIFT(LBRR_symbol, i) & 1; }
                }
            }
        }

        if (lostFlag == FLAG_DECODE_NORMAL) {
            /* Regular decoding: skip all LBRR data */
            for (i = 0; i < m_channel_state[0].nFramesPerPacket; i++) {
                for (n = 0; n < m_silk_DecControlStruct->nChannelsInternal; n++) {
                    if (m_channel_state[n].LBRR_flags[i]) {
                        int16_t pulses[MAX_FRAME_LENGTH];
                        int32_t condCoding;

                        if (m_silk_DecControlStruct->nChannelsInternal == 2 && n == 0) {
                            silk_stereo_decode_pred(MS_pred_Q13);
                            if (m_channel_state[1].LBRR_flags[i] == 0) { silk_stereo_decode_mid_only(&decode_only_middle); }
                        }
                        /* Use conditional coding if previous frame available */
                        if (i > 0 && m_channel_state[n].LBRR_flags[i - 1]) {
                            condCoding = CODE_CONDITIONALLY;
                        } else {
                            condCoding = CODE_INDEPENDENTLY;
                        }
                        silk_decode_indices(n, i, 1, condCoding);
                        silk_decode_pulses(pulses, m_channel_state[n].indices.signalType, m_channel_state[n].indices.quantOffsetType, m_channel_state[n].frame_length);
                    }
                }
            }
        }
    }

    /* Get MS predictor index */
    if (m_silk_DecControlStruct->nChannelsInternal == 2) {
        if (lostFlag == FLAG_DECODE_NORMAL || (lostFlag == FLAG_DECODE_LBRR && m_channel_state[0].LBRR_flags[m_channel_state[0].nFramesDecoded] == 1)) {
            silk_stereo_decode_pred(MS_pred_Q13);
            /* For LBRR data, decode mid-only flag only if side-channel's LBRR flag is false */
            if ((lostFlag == FLAG_DECODE_NORMAL && m_channel_state[1].VAD_flags[m_channel_state[0].nFramesDecoded] == 0) ||
                (lostFlag == FLAG_DECODE_LBRR && m_channel_state[1].LBRR_flags[m_channel_state[0].nFramesDecoded] == 0)) {
                silk_stereo_decode_mid_only(&decode_only_middle);
            } else {
                decode_only_middle = 0;
            }
        } else {
            for (n = 0; n < 2; n++) { MS_pred_Q13[n] = m_silk_decoder->sStereo.pred_prev_Q13[n]; }
        }
    }

    /* Reset side channel decoder prediction memory for first frame with side coding */
    if (m_silk_DecControlStruct->nChannelsInternal == 2 && decode_only_middle == 0 && m_silk_decoder->prev_decode_only_middle == 1) {
        memset(m_channel_state[1].outBuf, 0, sizeof(m_channel_state[1].outBuf));
        memset(m_channel_state[1].sLPC_Q14_buf, 0, sizeof(m_channel_state[1].sLPC_Q14_buf));
        m_channel_state[1].lagPrev = 100;
        m_channel_state[1].LastGainIndex = 10;
        m_channel_state[1].prevSignalType = TYPE_NO_VOICE_ACTIVITY;
        m_channel_state[1].first_frame_after_reset = 1;
    }

    /* Check if the temp buffer fits into the output PCM buffer. If it fits,
       we can delay allocating the temp buffer until after the SILK peak stack
       usage. We need to use a < and not a <= because of the two extra samples. */
    delay_stack_alloc = m_silk_DecControlStruct->internalSampleRate * m_silk_DecControlStruct->nChannelsInternal < m_silk_DecControlStruct->API_sampleRate * m_silk_DecControlStruct->nChannelsAPI;

    size_t          samplesOut1_tmp_storage1_len = delay_stack_alloc ? SILK_ALLOC_NONE : m_silk_DecControlStruct->nChannelsInternal * (m_channel_state[0].frame_length + 2);
    ps_ptr<int16_t> samplesOut1_tmp_storage1;
    samplesOut1_tmp_storage1.alloc_array(samplesOut1_tmp_storage1_len);

    if (delay_stack_alloc) {
        samplesOut1_tmp[0] = samplesOut;
        samplesOut1_tmp[1] = samplesOut + m_channel_state[0].frame_length + 2;
    } else {
        samplesOut1_tmp[0] = samplesOut1_tmp_storage1.get();
        samplesOut1_tmp[1] = samplesOut1_tmp_storage1.get() + m_channel_state[0].frame_length + 2;
    }

    if (lostFlag == FLAG_DECODE_NORMAL) {
        has_side = !decode_only_middle;
    } else {
        has_side = !m_silk_decoder->prev_decode_only_middle ||
                   (m_silk_DecControlStruct->nChannelsInternal == 2 && lostFlag == FLAG_DECODE_LBRR && m_channel_state[1].LBRR_flags[m_channel_state[1].nFramesDecoded] == 1);
    }
    /* Call decoder for one frame */
    for (n = 0; n < m_silk_DecControlStruct->nChannelsInternal; n++) {
        if (n == 0 || has_side) {
            int32_t FrameIndex;
            int32_t condCoding;

            FrameIndex = m_channel_state[0].nFramesDecoded - n;
            /* Use independent coding if no previous frame available */
            if (FrameIndex <= 0) {
                condCoding = CODE_INDEPENDENTLY;
            } else if (lostFlag == FLAG_DECODE_LBRR) {
                condCoding = m_channel_state[n].LBRR_flags[FrameIndex - 1] ? CODE_CONDITIONALLY : CODE_INDEPENDENTLY;
            } else if (n > 0 && m_silk_decoder->prev_decode_only_middle) {
                /* If we skipped a side frame in this packet, we don't
                   need LTP scaling; the LTP state is well-defined. */
                condCoding = CODE_INDEPENDENTLY_NO_LTP_SCALING;
            } else {
                condCoding = CODE_CONDITIONALLY;
            }
            ret += silk_decode_frame(n, &samplesOut1_tmp[n][2], &nSamplesOutDec, lostFlag, condCoding);
        } else {
            memset(&samplesOut1_tmp[n][2], 0, nSamplesOutDec * sizeof(int16_t));
        }
        m_channel_state[n].nFramesDecoded++;
    }

    if (m_silk_DecControlStruct->nChannelsAPI == 2 && m_silk_DecControlStruct->nChannelsInternal == 2) {
        /* Convert Mid/Side to Left/Right */
        silk_stereo_MS_to_LR(&m_silk_decoder->sStereo, samplesOut1_tmp[0], samplesOut1_tmp[1], MS_pred_Q13, m_channel_state[0].fs_kHz, nSamplesOutDec);
    } else {
        /* Buffering */
        memcpy(samplesOut1_tmp[0], m_silk_decoder->sStereo.sMid, 2 * sizeof(int16_t));
        memcpy(m_silk_decoder->sStereo.sMid, &samplesOut1_tmp[0][nSamplesOutDec], 2 * sizeof(int16_t));
    }

    /* Number of output samples */
    *nSamplesOut = silk_DIV32(nSamplesOutDec * m_silk_DecControlStruct->API_sampleRate, silk_SMULBB(m_channel_state[0].fs_kHz, 1000));

    /* Set up pointers to temp buffers */
    size_t          samplesOut2_tmp_len = m_silk_DecControlStruct->nChannelsAPI == 2 ? *nSamplesOut : SILK_ALLOC_NONE;
    ps_ptr<int16_t> samplesOut2_tmp;
    samplesOut2_tmp.alloc_array(samplesOut2_tmp_len);

    if (m_silk_DecControlStruct->nChannelsAPI == 2) {
        resample_out_ptr = samplesOut2_tmp.get();
    } else {
        resample_out_ptr = samplesOut;
    }

    size_t          samplesOut1_tmp_storage2_len = delay_stack_alloc ? m_silk_DecControlStruct->nChannelsInternal * (m_channel_state[0].frame_length + 2) : SILK_ALLOC_NONE;
    ps_ptr<int16_t> samplesOut1_tmp_storage2;
    samplesOut1_tmp_storage2.alloc_array(samplesOut1_tmp_storage2_len);

    if (delay_stack_alloc) {
        //    size_t val1 = m_silk_DecControlStruct->nChannelsInternal * (m_channel_state[0].frame_length + 2);
        size_t val2 = samplesOut1_tmp_storage2_len * sizeof(int16_t);
        // size_t n = val1 * val2;
        memcpy(samplesOut1_tmp_storage2.get(), samplesOut, val2);
        samplesOut1_tmp[0] = samplesOut1_tmp_storage2.get();
        samplesOut1_tmp[1] = samplesOut1_tmp_storage2.get() + m_channel_state[0].frame_length + 2;
    }
    for (n = 0; n < silk_min(m_silk_DecControlStruct->nChannelsAPI, m_silk_DecControlStruct->nChannelsInternal); n++) {

        /* Resample decoded signal to API_sampleRate */
        ret += silk_resampler(n, resample_out_ptr, &samplesOut1_tmp[n][1], nSamplesOutDec);

        /* Interleave if stereo output and stereo stream */
        if (m_silk_DecControlStruct->nChannelsAPI == 2) {
            for (i = 0; i < *nSamplesOut; i++) { samplesOut[n + 2 * i] = resample_out_ptr[i]; }
        }
    }
    /* Create two channel output from mono stream */
    if (m_silk_DecControlStruct->nChannelsAPI == 2 && m_silk_DecControlStruct->nChannelsInternal == 1) {
        if (stereo_to_mono) {
            /* Resample right channel for newly collapsed stereo just in case
               we weren't doing collapsing when switching to mono */
            ret += silk_resampler(n, resample_out_ptr, &samplesOut1_tmp[0][1], nSamplesOutDec);

            for (i = 0; i < *nSamplesOut; i++) { samplesOut[1 + 2 * i] = resample_out_ptr[i]; }
        } else {
            for (i = 0; i < *nSamplesOut; i++) { samplesOut[1 + 2 * i] = samplesOut[0 + 2 * i]; }
        }
    }
    /* Export pitch lag, measured at 48 kHz sampling rate */
    if (m_channel_state[0].prevSignalType == TYPE_VOICED) {
        int mult_tab[3] = {6, 4, 3};
        m_silk_DecControlStruct->prevPitchLag = m_channel_state[0].lagPrev * mult_tab[(m_channel_state[0].fs_kHz - 8) >> 2];
    } else {
        m_silk_DecControlStruct->prevPitchLag = 0;
    }

    if (lostFlag == FLAG_PACKET_LOST) {
        /* On packet loss, remove the gain clamping to prevent having the energy "bounce back"
           if we lose packets when the energy is going down */
        for (i = 0; i < m_silk_decoder->nChannelsInternal; i++) m_channel_state[i].LastGainIndex = 10;
    } else {
        m_silk_decoder->prev_decode_only_middle = decode_only_middle;
    }
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Decoder functions */
int32_t SilkDecoder::silk_Get_Decoder_Size(int32_t* decSizeBytes) {
    int32_t ret = SILK_NO_ERROR;

    *decSizeBytes = sizeof(silk_decoder_t) + sizeof(silk_decoder_state_t);

    return ret;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Reset decoder state */
int32_t SilkDecoder::silk_InitDecoder() {
    int32_t n, ret = SILK_NO_ERROR;
    // m_channel_state = (silk_decoder_state_t*) &decState->m_channel_state;

    for (n = 0; n < DECODER_NUM_CHANNELS; n++) { ret = silk_init_decoder(n); }
    memset(&m_silk_decoder->sStereo, 0, sizeof(m_silk_decoder->sStereo));
    m_silk_decoder->prev_decode_only_middle = 0;
    return ret;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Core decoder. Performs inverse NSQ operation LTP + LPC */
void SilkDecoder::silk_decode_core(uint8_t n, int16_t xq[], const int16_t pulses[MAX_FRAME_LENGTH]) {
    int32_t  i, k, lag = 0, start_idx, sLTP_buf_idx, NLSF_interpolation_flag, signalType;
    int16_t *A_Q12, *B_Q14, *pxq, A_Q12_tmp[MAX_LPC_ORDER];
    int32_t  LTP_pred_Q13, LPC_pred_Q10, Gain_Q10, inv_gain_Q31, gain_adj_Q16, rand_seed, offset_Q10;
    int32_t *pred_lag_ptr, *pexc_Q14, *pres_Q14;

    assert(m_channel_state[n].prev_gain_Q16 != 0);

    ps_ptr<int16_t> sLTP;
    sLTP.alloc_array(m_channel_state[n].ltp_mem_length);
    ps_ptr<int32_t> sLTP_Q15;
    sLTP_Q15.alloc_array(m_channel_state[n].ltp_mem_length + m_channel_state[n].frame_length);
    ps_ptr<int32_t> res_Q14;
    res_Q14.alloc_array(m_channel_state[n].subfr_length);
    ps_ptr<int32_t> sLPC_Q14;
    sLPC_Q14.alloc_array(m_channel_state[n].subfr_length + MAX_LPC_ORDER);

    offset_Q10 = silk_Quantization_Offsets_Q10[m_channel_state[n].indices.signalType >> 1][m_channel_state[n].indices.quantOffsetType];

    if (m_channel_state[n].indices.NLSFInterpCoef_Q2 < 1 << 2) {
        NLSF_interpolation_flag = 1;
    } else {
        NLSF_interpolation_flag = 0;
    }

    /* Decode excitation */
    rand_seed = m_channel_state[n].indices.Seed;
    for (i = 0; i < m_channel_state[n].frame_length; i++) {
        rand_seed = silk_RAND(rand_seed);
        m_channel_state[n].exc_Q14[i] = silk_LSHIFT((int32_t)pulses[i], 14);
        if (m_channel_state[n].exc_Q14[i] > 0) {
            m_channel_state[n].exc_Q14[i] -= QUANT_LEVEL_ADJUST_Q10 << 4;
        } else if (m_channel_state[n].exc_Q14[i] < 0) {
            m_channel_state[n].exc_Q14[i] += QUANT_LEVEL_ADJUST_Q10 << 4;
        }
        m_channel_state[n].exc_Q14[i] += offset_Q10 << 4;
        if (rand_seed < 0) { m_channel_state[n].exc_Q14[i] = -m_channel_state[n].exc_Q14[i]; }

        rand_seed = silk_ADD32_ovflw(rand_seed, pulses[i]);
    }

    /* Copy LPC state */
    memcpy(sLPC_Q14.get(), m_channel_state[n].sLPC_Q14_buf, MAX_LPC_ORDER * sizeof(int32_t));

    pexc_Q14 = m_channel_state[n].exc_Q14;
    pxq = xq;
    sLTP_buf_idx = m_channel_state[n].ltp_mem_length;
    /* Loop over subframes */
    for (k = 0; k < m_channel_state[n].nb_subfr; k++) {
        pres_Q14 = res_Q14.get();
        A_Q12 = m_silk_decoder_control->PredCoef_Q12[k >> 1];

        /* Preload LPC coeficients to array on stack. Gives small performance gain */
        memcpy(A_Q12_tmp, A_Q12, m_channel_state[n].LPC_order * sizeof(int16_t));
        B_Q14 = &m_silk_decoder_control->LTPCoef_Q14[k * LTP_ORDER];
        signalType = m_channel_state[n].indices.signalType;

        Gain_Q10 = silk_RSHIFT(m_silk_decoder_control->Gains_Q16[k], 6);
        inv_gain_Q31 = silk_INVERSE32_varQ(m_silk_decoder_control->Gains_Q16[k], 47);

        /* Calculate gain adjustment factor */
        if (m_silk_decoder_control->Gains_Q16[k] != m_channel_state[n].prev_gain_Q16) {
            gain_adj_Q16 = silk_DIV32_varQ(m_channel_state[n].prev_gain_Q16, m_silk_decoder_control->Gains_Q16[k], 16);

            /* Scale short term state */
            for (i = 0; i < MAX_LPC_ORDER; i++) { sLPC_Q14[i] = silk_SMULWW(gain_adj_Q16, sLPC_Q14[i]); }
        } else {
            gain_adj_Q16 = (int32_t)1 << 16;
        }

        /* Save inv_gain */
        assert(inv_gain_Q31 != 0);
        m_channel_state[n].prev_gain_Q16 = m_silk_decoder_control->Gains_Q16[k];

        /* Avoid abrupt transition from voiced PLC to unvoiced normal decoding */
        if (m_channel_state[n].lossCnt && m_channel_state[n].prevSignalType == TYPE_VOICED && m_channel_state[n].indices.signalType != TYPE_VOICED && k < MAX_NB_SUBFR / 2) {
            memset(B_Q14, 0, LTP_ORDER * sizeof(int16_t));
            B_Q14[LTP_ORDER / 2] = SILK_FIX_CONST(0.25, 14);

            signalType = TYPE_VOICED;
            m_silk_decoder_control->pitchL[k] = m_channel_state[n].lagPrev;
        }

        if (signalType == TYPE_VOICED) {
            /* Voiced */
            lag = m_silk_decoder_control->pitchL[k];

            /* Re-whitening */
            if (k == 0 || (k == 2 && NLSF_interpolation_flag)) {
                /* Rewhiten with new A coefs */
                start_idx = m_channel_state[n].ltp_mem_length - lag - m_channel_state[n].LPC_order - LTP_ORDER / 2;
                assert(start_idx > 0);

                if (k == 2) { memcpy(&m_channel_state[n].outBuf[m_channel_state[n].ltp_mem_length], xq, 2 * m_channel_state[n].subfr_length * sizeof(int16_t)); }

                silk_LPC_analysis_filter(&sLTP[start_idx], &m_channel_state[n].outBuf[start_idx + k * m_channel_state[n].subfr_length], A_Q12, m_channel_state[n].ltp_mem_length - start_idx,
                                         m_channel_state[n].LPC_order);

                /* After rewhitening the LTP state is unscaled */
                if (k == 0) {
                    /* Do LTP downscaling to reduce inter-packet dependency */
                    inv_gain_Q31 = silk_LSHIFT(silk_SMULWB(inv_gain_Q31, m_silk_decoder_control->LTP_scale_Q14), 2);
                }
                for (i = 0; i < lag + LTP_ORDER / 2; i++) { sLTP_Q15[sLTP_buf_idx - i - 1] = silk_SMULWB(inv_gain_Q31, sLTP[m_channel_state[n].ltp_mem_length - i - 1]); }
            } else {
                /* Update LTP state when Gain changes */
                if (gain_adj_Q16 != (int32_t)1 << 16) {
                    for (i = 0; i < lag + LTP_ORDER / 2; i++) { sLTP_Q15[sLTP_buf_idx - i - 1] = silk_SMULWW(gain_adj_Q16, sLTP_Q15[sLTP_buf_idx - i - 1]); }
                }
            }
        }

        /* Long-term prediction */
        if (signalType == TYPE_VOICED) {
            /* Set up pointer */
            pred_lag_ptr = &sLTP_Q15[sLTP_buf_idx - lag + LTP_ORDER / 2];
            for (i = 0; i < m_channel_state[n].subfr_length; i++) {
                /* Unrolled loop */
                /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
                LTP_pred_Q13 = 2;
                LTP_pred_Q13 = silk_SMLAWB(LTP_pred_Q13, pred_lag_ptr[0], B_Q14[0]);
                LTP_pred_Q13 = silk_SMLAWB(LTP_pred_Q13, pred_lag_ptr[-1], B_Q14[1]);
                LTP_pred_Q13 = silk_SMLAWB(LTP_pred_Q13, pred_lag_ptr[-2], B_Q14[2]);
                LTP_pred_Q13 = silk_SMLAWB(LTP_pred_Q13, pred_lag_ptr[-3], B_Q14[3]);
                LTP_pred_Q13 = silk_SMLAWB(LTP_pred_Q13, pred_lag_ptr[-4], B_Q14[4]);
                pred_lag_ptr++;

                /* Generate LPC excitation */
                pres_Q14[i] = silk_ADD_LSHIFT32(pexc_Q14[i], LTP_pred_Q13, 1);

                /* Update states */
                sLTP_Q15[sLTP_buf_idx] = silk_LSHIFT(pres_Q14[i], 1);
                sLTP_buf_idx++;
            }
        } else {
            pres_Q14 = pexc_Q14;
        }

        for (i = 0; i < m_channel_state[n].subfr_length; i++) {
            /* Short-term prediction */
            assert(m_channel_state[n].LPC_order == 10 || m_channel_state[n].LPC_order == 16);
            /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
            LPC_pred_Q10 = silk_RSHIFT(m_channel_state[n].LPC_order, 1);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 1], A_Q12_tmp[0]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 2], A_Q12_tmp[1]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 3], A_Q12_tmp[2]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 4], A_Q12_tmp[3]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 5], A_Q12_tmp[4]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 6], A_Q12_tmp[5]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 7], A_Q12_tmp[6]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 8], A_Q12_tmp[7]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 9], A_Q12_tmp[8]);
            LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 10], A_Q12_tmp[9]);
            if (m_channel_state[n].LPC_order == 16) {
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 11], A_Q12_tmp[10]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 12], A_Q12_tmp[11]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 13], A_Q12_tmp[12]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 14], A_Q12_tmp[13]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 15], A_Q12_tmp[14]);
                LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14[MAX_LPC_ORDER + i - 16], A_Q12_tmp[15]);
            }

            /* Add prediction to LPC excitation */
            sLPC_Q14[MAX_LPC_ORDER + i] = silk_ADD_SAT32(pres_Q14[i], silk_LSHIFT_SAT32(LPC_pred_Q10, 4));

            /* Scale with gain */
            pxq[i] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(silk_SMULWW(sLPC_Q14[MAX_LPC_ORDER + i], Gain_Q10), 8));
        }

        /* Update LPC filter state */
        memcpy(sLPC_Q14.get(), &sLPC_Q14[m_channel_state[n].subfr_length], MAX_LPC_ORDER * sizeof(int32_t));
        pexc_Q14 += m_channel_state[n].subfr_length;
        pxq += m_channel_state[n].subfr_length;
    }

    /* Save LPC state */
    memcpy(m_channel_state[n].sLPC_Q14_buf, sLPC_Q14.get(), MAX_LPC_ORDER * sizeof(int32_t));
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Decode frame */
int32_t SilkDecoder::silk_decode_frame(uint8_t n, int16_t pOut[], /* O    Pointer to output speech frame              */
                                       int32_t* pN,               /* O    Pointer to size of output frame             */
                                       int32_t  lostFlag,         /* I    0: no loss, 1 loss, 2 decode fec            */
                                       int32_t  condCoding        /* I    The type of conditional coding to use       */
) {
    int32_t L, mv_len, ret = 0;
    L = m_channel_state[n].frame_length;
    m_silk_decoder_control->LTP_scale_Q14 = 0;

    /* Safety checks */
    assert(L > 0 && L <= MAX_FRAME_LENGTH);

    if (lostFlag == FLAG_DECODE_NORMAL || (lostFlag == FLAG_DECODE_LBRR && m_channel_state[n].LBRR_flags[m_channel_state[n].nFramesDecoded] == 1)) {
        int16_t pulses[(L + SHELL_CODEC_FRAME_LENGTH - 1) & ~(SHELL_CODEC_FRAME_LENGTH - 1)];
        /*********************************************/
        /* Decode quantization indices of side info  */
        /*********************************************/
        silk_decode_indices(n, m_channel_state[n].nFramesDecoded, lostFlag, condCoding);

        /*********************************************/
        /* Decode quantization indices of excitation */
        /*********************************************/
        silk_decode_pulses(pulses, m_channel_state[n].indices.signalType, m_channel_state[n].indices.quantOffsetType, m_channel_state[n].frame_length);

        /********************************************/
        /* Decode parameters and pulse signal       */
        /********************************************/
        silk_decode_parameters(n, condCoding);

        /********************************************************/
        /* Run inverse NSQ                                      */
        /********************************************************/
        silk_decode_core(n, pOut, pulses);

        /********************************************************/
        /* Update PLC state                                     */
        /********************************************************/
        silk_PLC(n, pOut, 0);

        m_channel_state[n].lossCnt = 0;
        m_channel_state[n].prevSignalType = m_channel_state[n].indices.signalType;
        assert(m_channel_state[n].prevSignalType >= 0 && m_channel_state[n].prevSignalType <= 2);

        /* A frame has been decoded without errors */
        m_channel_state[n].first_frame_after_reset = 0;
    } else {
        /* Handle packet loss by extrapolation */
        m_channel_state[n].indices.signalType = m_channel_state[n].prevSignalType;
        silk_PLC(n, pOut, 1);
    }

    /*************************/
    /* Update output buffer. */
    /*************************/
    assert(m_channel_state[n].ltp_mem_length >= m_channel_state[n].frame_length);
    mv_len = m_channel_state[n].ltp_mem_length - m_channel_state[n].frame_length;
    memmove(m_channel_state[n].outBuf, &m_channel_state[n].outBuf[m_channel_state[n].frame_length], mv_len * sizeof(int16_t));
    memcpy(&m_channel_state[n].outBuf[mv_len], pOut, m_channel_state[n].frame_length * sizeof(int16_t));

    /************************************************/
    /* Comfort noise generation / estimation        */
    /************************************************/
    silk_CNG(n, pOut, L);

    /****************************************************************/
    /* Ensure smooth connection of extrapolated and good frames     */
    /****************************************************************/
    silk_PLC_glue_frames(n, pOut, L);

    /* Update some decoder state variables */
    m_channel_state[n].lagPrev = m_silk_decoder_control->pitchL[m_channel_state[n].nb_subfr - 1];

    /* Set output frame length */
    *pN = L;

    return ret;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_decode_pitch(int16_t       lagIndex,     /* I                                                                */
                                    int8_t        contourIndex, /* O                                                                */
                                    int32_t       pitch_lags[], /* O    4 pitch values                                              */
                                    const int32_t Fs_kHz,       /* I    sampling frequency (kHz)                                    */
                                    const int32_t nb_subfr      /* I    number of sub frames                                        */
) {
    int32_t       lag, k, min_lag, max_lag, cbk_size;
    const int8_t* Lag_CB_ptr;

    if (Fs_kHz == 8) {
        if (nb_subfr == PE_MAX_NB_SUBFR) {
            Lag_CB_ptr = &silk_CB_lags_stage2[0][0];
            cbk_size = PE_NB_CBKS_STAGE2_EXT;
        } else {
            assert(nb_subfr == PE_MAX_NB_SUBFR >> 1);
            Lag_CB_ptr = &silk_CB_lags_stage2_10_ms[0][0];
            cbk_size = PE_NB_CBKS_STAGE2_10MS;
        }
    } else {
        if (nb_subfr == PE_MAX_NB_SUBFR) {
            Lag_CB_ptr = &silk_CB_lags_stage3[0][0];
            cbk_size = PE_NB_CBKS_STAGE3_MAX;
        } else {
            assert(nb_subfr == PE_MAX_NB_SUBFR >> 1);
            Lag_CB_ptr = &silk_CB_lags_stage3_10_ms[0][0];
            cbk_size = PE_NB_CBKS_STAGE3_10MS;
        }
    }

    min_lag = silk_SMULBB(PE_MIN_LAG_MS, Fs_kHz);
    max_lag = silk_SMULBB(PE_MAX_LAG_MS, Fs_kHz);
    lag = min_lag + lagIndex;

    for (k = 0; k < nb_subfr; k++) {
        pitch_lags[k] = lag + matrix_ptr(Lag_CB_ptr, k, contourIndex, cbk_size);
        pitch_lags[k] = silk_LIMIT(pitch_lags[k], min_lag, max_lag);
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Gain scalar quantization with hysteresis, uniform on log scale */
void SilkDecoder::silk_gains_quant(int8_t        ind[MAX_NB_SUBFR],      /* O    gain indices                                */
                                   int32_t       gain_Q16[MAX_NB_SUBFR], /* I/O  gains (quantized out)                       */
                                   int8_t*       prev_ind,               /* I/O  last index in previous frame                */
                                   const int32_t conditional,            /* I    first gain is delta coded if 1              */
                                   const int32_t nb_subfr                /* I    number of subframes                         */
) {
    int32_t k, double_step_size_threshold;

    for (k = 0; k < nb_subfr; k++) {
        /* Convert to log scale, scale, floor() */
        ind[k] = silk_SMULWB(SCALE_Q16, silk_lin2log(gain_Q16[k]) - OFFSET);

        /* Round towards previous quantized gain (hysteresis) */
        if (ind[k] < *prev_ind) { ind[k]++; }
        ind[k] = silk_LIMIT_int(ind[k], 0, N_LEVELS_QGAIN - 1);

        /* Compute delta indices and limit */
        if (k == 0 && conditional == 0) {
            /* Full index */
            ind[k] = silk_LIMIT_int(ind[k], *prev_ind + MIN_DELTA_GAIN_QUANT, N_LEVELS_QGAIN - 1);
            *prev_ind = ind[k];
        } else {
            /* Delta index */
            ind[k] = ind[k] - *prev_ind;

            /* Double the quantization step size for large gain increases, so that the max gain level can be reached */
            double_step_size_threshold = 2 * MAX_DELTA_GAIN_QUANT - N_LEVELS_QGAIN + *prev_ind;
            if (ind[k] > double_step_size_threshold) { ind[k] = double_step_size_threshold + silk_RSHIFT(ind[k] - double_step_size_threshold + 1, 1); }

            ind[k] = silk_LIMIT_int(ind[k], MIN_DELTA_GAIN_QUANT, MAX_DELTA_GAIN_QUANT);

            /* Accumulate deltas */
            if (ind[k] > double_step_size_threshold) {
                *prev_ind += silk_LSHIFT(ind[k], 1) - double_step_size_threshold;
                *prev_ind = silk_min_int(*prev_ind, N_LEVELS_QGAIN - 1);
            } else {
                *prev_ind += ind[k];
            }

            /* Shift to make non-negative */
            ind[k] -= MIN_DELTA_GAIN_QUANT;
        }

        /* Scale and convert to linear scale */
        gain_Q16[k] = silk_log2lin(silk_min_32(silk_SMULWB(INV_SCALE_Q16, *prev_ind) + OFFSET, 3967)); /* 3967 = 31 in Q7 */
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Gains scalar dequantization, uniform on log scale */
void SilkDecoder::silk_gains_dequant(int32_t       gain_Q16[MAX_NB_SUBFR], /* O    quantized gains                             */
                                     const int8_t  ind[MAX_NB_SUBFR],      /* I    gain indices                                */
                                     int8_t*       prev_ind,               /* I/O  last index in previous frame                */
                                     const int32_t conditional,            /* I    first gain is delta coded if 1              */
                                     const int32_t nb_subfr                /* I    number of subframes                          */
) {
    int32_t k, ind_tmp, double_step_size_threshold;

    for (k = 0; k < nb_subfr; k++) {
        if (k == 0 && conditional == 0) {
            /* Gain index is not allowed to go down more than 16 steps (~21.8 dB) */
            *prev_ind = silk_max_int(ind[k], *prev_ind - 16);
        } else {
            /* Delta index */
            ind_tmp = ind[k] + MIN_DELTA_GAIN_QUANT;

            /* Accumulate deltas */
            double_step_size_threshold = 2 * MAX_DELTA_GAIN_QUANT - N_LEVELS_QGAIN + *prev_ind;
            if (ind_tmp > double_step_size_threshold) {
                *prev_ind += silk_LSHIFT(ind_tmp, 1) - double_step_size_threshold;
            } else {
                *prev_ind += ind_tmp;
            }
        }
        *prev_ind = silk_LIMIT_int(*prev_ind, 0, N_LEVELS_QGAIN - 1);

        /* Scale and convert to linear scale */
        gain_Q16[k] = silk_log2lin(silk_min_32(silk_SMULWB(INV_SCALE_Q16, *prev_ind) + OFFSET, 3967)); /* 3967 = 31 in Q7 */
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Compute unique identifier of gain indices vector */
int32_t SilkDecoder::silk_gains_ID(                                 /* O    returns unique identifier of gains          */
                                   const int8_t  ind[MAX_NB_SUBFR], /* I    gain indices                                */
                                   const int32_t nb_subfr           /* I    number of subframes                         */
) {
    int32_t k;
    int32_t gainsID;

    gainsID = 0;
    for (k = 0; k < nb_subfr; k++) { gainsID = silk_ADD_LSHIFT32(ind[k], gainsID, 8); }

    return gainsID;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t SilkDecoder::silk_init_decoder(uint8_t n) {
    /* Clear the entire encoder state, except anything copied */
    memset(&m_channel_state[n], 0, sizeof(silk_decoder_state_t));

    /* Used to deactivate LSF interpolation */
    m_channel_state[n].first_frame_after_reset = 1;
    m_channel_state[n].prev_gain_Q16 = 65536;

    /* Reset CNG state */
    silk_CNG_Reset(n);

    /* Reset PLC state */
    silk_PLC_Reset(n);

    return (0);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t SilkDecoder::silk_inner_prod_aligned_scale(const int16_t* const inVec1, /*    I input vector 1 */
                                                   const int16_t* const inVec2, /*    I input vector 2 */
                                                   const int32_t        scale,  /*    I number of bits to shift         */
                                                   const int32_t        len     /*    I vector lengths            */
) {
    int32_t i;
    int32_t sum = 0;
    for (i = 0; i < len; i++) { sum = silk_ADD_RSHIFT32(sum, silk_SMULBB(inVec1[i], inVec2[i]), scale); }
    return sum;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Interpolate two vectors */
void SilkDecoder::silk_interpolate(int16_t       xi[MAX_LPC_ORDER], /* O    interpolated vector                         */
                                   const int16_t x0[MAX_LPC_ORDER], /* I    first vector                                */
                                   const int16_t x1[MAX_LPC_ORDER], /* I    second vector                               */
                                   const int32_t ifact_Q2,          /* I    interp. factor, weight on 2nd vector        */
                                   const int32_t d                  /* I    number of parameters                        */
) {
    int32_t i;

    assert(ifact_Q2 >= 0);
    assert(ifact_Q2 <= 4);

    for (i = 0; i < d; i++) { xi[i] = (int16_t)silk_ADD_RSHIFT(x0[i], silk_SMULBB(x1[i] - x0[i], ifact_Q2), 2); }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int32_t SilkDecoder::silk_lin2log(const int32_t inLin /* I  input in linear scale                                         */
) {
    int32_t lz, frac_Q7;

    silk_CLZ_FRAC(inLin, &lz, &frac_Q7);

    /* Piece-wise parabolic approximation */
    return silk_ADD_LSHIFT32(silk_SMLAWB(frac_Q7, silk_MUL(frac_Q7, 128 - frac_Q7), 179), 31 - lz, 7);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Approximation of 2^() (very close inverse of silk_lin2log()) Convert input to a linear scale    */
int32_t SilkDecoder::silk_log2lin(const int32_t inLog_Q7) {
    int32_t out, frac_Q7;

    if (inLog_Q7 < 0) {
        return 0;
    } else if (inLog_Q7 >= 3967) {
        return silk_int32_MAX;
    }

    out = silk_LSHIFT(1, silk_RSHIFT(inLog_Q7, 7));
    frac_Q7 = inLog_Q7 & 0x7F;
    if (inLog_Q7 < 2048) {
        /* Piece-wise parabolic approximation */
        out = silk_ADD_RSHIFT32(out, silk_MUL(out, silk_SMLAWB(frac_Q7, silk_SMULBB(frac_Q7, 128 - frac_Q7), -174)), 7);
    } else {
        /* Piece-wise parabolic approximation */
        out = silk_MLA(out, silk_RSHIFT(out, 7), silk_SMLAWB(frac_Q7, silk_SMULBB(frac_Q7, 128 - frac_Q7), -174));
    }
    return out;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SilkDecoder::silk_LPC_analysis_filter(int16_t*       out, /* O    Output signal                                               */
                                           const int16_t* in,  /* I    Input signal                                                */
                                           const int16_t* B,   /* I    MA prediction coefficients, Q12 [order]                     */
                                           const int32_t  len, /* I    Signal length                                               */
                                           const int32_t  d    /* I    Filter order                                                */
) {
    int32_t        j;
    int            ix;
    int32_t        out32_Q12, out32;
    const int16_t* in_ptr;
    assert(d >= 6);
    assert((d & 1) == 0);
    assert(d <= len);

    for (ix = d; ix < len; ix++) {
        in_ptr = &in[ix - 1];

        out32_Q12 = silk_SMULBB(in_ptr[0], B[0]);
        /* Allowing wrap around so that two wraps can cancel each other. The rare
           cases where the result wraps around can only be triggered by invalid streams*/
        out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-1], B[1]);
        out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-2], B[2]);
        out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-3], B[3]);
        out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-4], B[4]);
        out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-5], B[5]);
        for (j = 6; j < d; j += 2) {
            out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-j], B[j]);
            out32_Q12 = silk_SMLABB_ovflw(out32_Q12, in_ptr[-j - 1], B[j + 1]);
        }

        /* Subtract prediction */
        out32_Q12 = silk_SUB32_ovflw(silk_LSHIFT((int32_t)in_ptr[1], 12), out32_Q12);

        /* Scale to Q0 */
        out32 = silk_RSHIFT_ROUND(out32_Q12, 12);

        /* Saturate output */
        out[ix] = (int16_t)silk_SAT16(out32);
    }

    /* Set first d output samples to zero */
    memset(out, 0, d * sizeof(int16_t));
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/* Convert int32 coefficients to int16 coefs and make sure there's no wrap-around */
void SilkDecoder::silk_LPC_fit(int16_t*      a_QOUT, /* O    Output signal                                               */
                               int32_t*      a_QIN,  /* I/O  Input signal                                                */
                               const int32_t QOUT,   /* I    Input Q domain                                              */
                               const int32_t QIN,    /* I    Input Q domain                                              */
                               const int32_t d       /* I    Filter order                                                */
) {
    int32_t i, k, idx = 0;
    int32_t maxabs, absval, chirp_Q16;

    /* Limit the maximum absolute value of the prediction coefficients, so that they'll fit in int16 */
    for (i = 0; i < 10; i++) {
        /* Find maximum absolute value and its index */
        maxabs = 0;
        for (k = 0; k < d; k++) {
            absval = silk_abs(a_QIN[k]);
            if (absval > maxabs) {
                maxabs = absval;
                idx = k;
            }
        }
        maxabs = silk_RSHIFT_ROUND(maxabs, QIN - QOUT);

        if (maxabs > silk_int16_MAX) {
            /* Reduce magnitude of prediction coefficients */
            maxabs = silk_min(maxabs, 163838); /* ( silk_int32_MAX >> 14 ) + silk_int16_MAX = 163838 */
            chirp_Q16 = SILK_FIX_CONST(0.999, 16) - silk_DIV32(silk_LSHIFT(maxabs - silk_int16_MAX, 14), silk_RSHIFT32(silk_MUL(maxabs, idx + 1), 2));
            silk_bwexpander_32(a_QIN, d, chirp_Q16);
        } else {
            break;
        }
    }

    if (i == 10) {
        /* Reached the last iteration, clip the coefficients */
        for (k = 0; k < d; k++) {
            a_QOUT[k] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(a_QIN[k], QIN - QOUT));
            a_QIN[k] = silk_LSHIFT((int32_t)a_QOUT[k], QIN - QOUT);
        }
    } else {
        for (k = 0; k < d; k++) { a_QOUT[k] = (int16_t)silk_RSHIFT_ROUND(a_QIN[k], QIN - QOUT); }
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Compute inverse of LPC prediction gain, and                          */
/* test if LPC coefficients are stable (all poles within unit circle)   */
int32_t SilkDecoder::LPC_inverse_pred_gain_QA_c(                                        /* O   Returns inverse prediction gain in energy domain, Q30    */
                                                int32_t       A_QA[SILK_MAX_ORDER_LPC], /* I   Prediction coefficients */
                                                const int32_t order                     /* I   Prediction order               */
) {
    const uint8_t QA24 = 24;
    int32_t       k, n, mult2Q;
    int32_t       invGain_Q30, rc_Q31, rc_mult1_Q30, rc_mult2, tmp1, tmp2;

    invGain_Q30 = SILK_FIX_CONST(1, 30);
    for (k = order - 1; k > 0; k--) {
        /* Check for stability */
        if ((A_QA[k] > A_LIMIT) || (A_QA[k] < -A_LIMIT)) { return 0; }

        /* Set RC equal to negated AR coef */
        rc_Q31 = -silk_LSHIFT(A_QA[k], 31 - QA24);

        /* rc_mult1_Q30 range: [ 1 : 2^30 ] */
        rc_mult1_Q30 = silk_SUB32(SILK_FIX_CONST(1, 30), silk_SMMUL(rc_Q31, rc_Q31));
        assert(rc_mult1_Q30 > (1 << 15)); /* reduce A_LIMIT if fails */
        assert(rc_mult1_Q30 <= (1 << 30));

        /* Update inverse gain */
        /* invGain_Q30 range: [ 0 : 2^30 ] */
        invGain_Q30 = silk_LSHIFT(silk_SMMUL(invGain_Q30, rc_mult1_Q30), 2);
        assert(invGain_Q30 >= 0);
        assert(invGain_Q30 <= (1 << 30));
        if (invGain_Q30 < SILK_FIX_CONST(1.0f / MAX_PREDICTION_POWER_GAIN, 30)) { return 0; }

        /* rc_mult2 range: [ 2^30 : silk_int32_MAX ] */
        mult2Q = 32 - silk_CLZ32(silk_abs(rc_mult1_Q30));
        rc_mult2 = silk_INVERSE32_varQ(rc_mult1_Q30, mult2Q + 30);

        /* Update AR coefficient */
        for (n = 0; n < (k + 1) >> 1; n++) {
            int64_t tmp64;
            tmp1 = A_QA[n];
            tmp2 = A_QA[k - n - 1];
            tmp64 = silk_RSHIFT_ROUND64(silk_SMULL(silk_SUB_SAT32(tmp1, MUL32_FRAC_Q(tmp2, rc_Q31, 31)), rc_mult2), mult2Q);
            if (tmp64 > silk_int32_MAX || tmp64 < silk_int32_MIN) { return 0; }
            A_QA[n] = (int32_t)tmp64;
            tmp64 = silk_RSHIFT_ROUND64(silk_SMULL(silk_SUB_SAT32(tmp2, MUL32_FRAC_Q(tmp1, rc_Q31, 31)), rc_mult2), mult2Q);
            if (tmp64 > silk_int32_MAX || tmp64 < silk_int32_MIN) { return 0; }
            A_QA[k - n - 1] = (int32_t)tmp64;
        }
    }

    /* Check for stability */
    if ((A_QA[k] > A_LIMIT) || (A_QA[k] < -A_LIMIT)) { return 0; }

    /* Set RC equal to negated AR coef */
    rc_Q31 = -silk_LSHIFT(A_QA[0], 31 - QA24);

    /* Range: [ 1 : 2^30 ] */
    rc_mult1_Q30 = silk_SUB32(SILK_FIX_CONST(1, 30), silk_SMMUL(rc_Q31, rc_Q31));

    /* Update inverse gain */
    /* Range: [ 0 : 2^30 ] */
    invGain_Q30 = silk_LSHIFT(silk_SMMUL(invGain_Q30, rc_mult1_Q30), 2);
    assert(invGain_Q30 >= 0);
    assert(invGain_Q30 <= (1 << 30));
    if (invGain_Q30 < SILK_FIX_CONST(1.0f / MAX_PREDICTION_POWER_GAIN, 30)) { return 0; }

    return invGain_Q30;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* For input in Q12 domain */
int32_t SilkDecoder::silk_LPC_inverse_pred_gain_c(                      /* O   Returns inverse prediction gain in energy domain, Q30        */
                                                  const int16_t* A_Q12, /* I   Prediction coefficients, Q12 [order] */
                                                  const int32_t  order  /* I   Prediction order   */
) {
    int32_t        k;
    int32_t        Atmp_QA[SILK_MAX_ORDER_LPC];
    int32_t        DC_resp = 0;
    const uint32_t QA24 = 24;

    /* Increase Q domain of the AR coefficients */
    for (k = 0; k < order; k++) {
        DC_resp += (int32_t)A_Q12[k];
        Atmp_QA[k] = silk_LSHIFT32((int32_t)A_Q12[k], QA24 - 12);
    }
    /* If the DC is unstable, we don't even need to do the full calculations */
    if (DC_resp >= 4096) { return 0; }
    return LPC_inverse_pred_gain_QA_c(Atmp_QA, order);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Predictive dequantizer for NLSF residuals */
void SilkDecoder::silk_NLSF_residual_dequant(                                   /* O    Returns RD value in Q30                     */
                                             int16_t       x_Q10[],             /* O    Output [ order ]                            */
                                             const int8_t  indices[],           /* I    Quantization indices [ order ]              */
                                             const uint8_t pred_coef_Q8[],      /* I    Backward predictor coefs [ order ]          */
                                             const int32_t quant_step_size_Q16, /* I    Quantization step size                      */
                                             const int16_t order                /* I    Number of input values                      */
) {
    int32_t i, out_Q10, pred_Q10;

    out_Q10 = 0;
    for (i = order - 1; i >= 0; i--) {
        pred_Q10 = silk_RSHIFT(silk_SMULBB(out_Q10, (int16_t)pred_coef_Q8[i]), 8);
        out_Q10 = silk_LSHIFT(indices[i], 10);
        if (out_Q10 > 0) {
            out_Q10 = silk_SUB16(out_Q10, SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10));
        } else if (out_Q10 < 0) {
            out_Q10 = out_Q10 + SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10);
        }
        out_Q10 = silk_SMLAWB(pred_Q10, (int32_t)out_Q10, quant_step_size_Q16);
        x_Q10[i] = out_Q10;
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* NLSF vector decoder */
void SilkDecoder::silk_NLSF_decode(int16_t*                     pNLSF_Q15,   /* O    Quantized NLSF vector [ LPC_ORDER ]         */
                                   int8_t*                      NLSFIndices, /* I    Codebook path vector [ LPC_ORDER + 1 ]      */
                                   const silk_NLSF_CB_struct_t* psNLSF_CB    /* I    Codebook object                             */
) {
    int32_t        i;
    uint8_t        pred_Q8[MAX_LPC_ORDER];
    int16_t        ec_ix[MAX_LPC_ORDER];
    int16_t        res_Q10[MAX_LPC_ORDER];
    int32_t        NLSF_Q15_tmp;
    const uint8_t* pCB_element;
    const int16_t* pCB_Wght_Q9;

    /* Unpack entropy table indices and predictor for current CB1 index */
    silk_NLSF_unpack(ec_ix, pred_Q8, psNLSF_CB, NLSFIndices[0]);

    /* Predictive residual dequantizer */
    silk_NLSF_residual_dequant(res_Q10, &NLSFIndices[1], pred_Q8, psNLSF_CB->quantStepSize_Q16, psNLSF_CB->order);

    /* Apply inverse square-rooted weights to first stage and add to output */
    pCB_element = &psNLSF_CB->CB1_NLSF_Q8[NLSFIndices[0] * psNLSF_CB->order];
    pCB_Wght_Q9 = &psNLSF_CB->CB1_Wght_Q9[NLSFIndices[0] * psNLSF_CB->order];
    for (i = 0; i < psNLSF_CB->order; i++) {
        NLSF_Q15_tmp = silk_ADD_LSHIFT32(silk_DIV32_16(silk_LSHIFT((int32_t)res_Q10[i], 14), pCB_Wght_Q9[i]), (int16_t)pCB_element[i], 7);
        pNLSF_Q15[i] = (int16_t)silk_LIMIT(NLSF_Q15_tmp, 0, 32767);
    }

    /* NLSF stabilization */
    silk_NLSF_stabilize(pNLSF_Q15, psNLSF_CB->deltaMin_Q15, psNLSF_CB->order);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Delayed-decision quantizer for NLSF residuals */
int32_t SilkDecoder::silk_NLSF_del_dec_quant(                                      /* O    Returns RD value in Q25                     */
                                             int8_t        indices[],              /* O    Quantization indices [ order ]              */
                                             const int16_t x_Q10[],                /* I    Input [ order ]                             */
                                             const int16_t w_Q5[],                 /* I    Weights [ order ]                           */
                                             const uint8_t pred_coef_Q8[],         /* I    Backward predictor coefs [ order ]          */
                                             const int16_t ec_ix[],                /* I    Indices to entropy coding tables [ order ]  */
                                             const uint8_t ec_rates_Q5[],          /* I    Rates []                                    */
                                             const int32_t quant_step_size_Q16,    /* I    Quantization step size    */
                                             const int16_t inv_quant_step_size_Q6, /* I    Inverse quantization step size */
                                             const int32_t mu_Q20,                 /* I    R/D tradeoff                                */
                                             const int16_t order                   /* I    Number of input values                      */
) {
    int32_t        i, j, nStates, ind_tmp, ind_min_max, ind_max_min, in_Q10, res_Q10;
    int32_t        pred_Q10, diff_Q10, rate0_Q5, rate1_Q5;
    int16_t        out0_Q10, out1_Q10;
    int32_t        RD_tmp_Q25, min_Q25, min_max_Q25, max_min_Q25;
    int32_t        ind_sort[NLSF_QUANT_DEL_DEC_STATES];
    int8_t         ind[NLSF_QUANT_DEL_DEC_STATES][MAX_LPC_ORDER];
    int16_t        prev_out_Q10[2 * NLSF_QUANT_DEL_DEC_STATES];
    int32_t        RD_Q25[2 * NLSF_QUANT_DEL_DEC_STATES];
    int32_t        RD_min_Q25[NLSF_QUANT_DEL_DEC_STATES];
    int32_t        RD_max_Q25[NLSF_QUANT_DEL_DEC_STATES];
    const uint8_t* rates_Q5;

    int32_t out0_Q10_table[2 * NLSF_QUANT_MAX_AMPLITUDE_EXT];
    int32_t out1_Q10_table[2 * NLSF_QUANT_MAX_AMPLITUDE_EXT];

    for (i = -NLSF_QUANT_MAX_AMPLITUDE_EXT; i <= NLSF_QUANT_MAX_AMPLITUDE_EXT - 1; i++) {
        out0_Q10 = silk_LSHIFT(i, 10);
        out1_Q10 = out0_Q10 + 1024;
        if (i > 0) {
            out0_Q10 = silk_SUB16(out0_Q10, SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10));
            out1_Q10 = silk_SUB16(out1_Q10, SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10));
        } else if (i == 0) {
            out1_Q10 = silk_SUB16(out1_Q10, SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10));
        } else if (i == -1) {
            out0_Q10 = out0_Q10 + SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10);
        } else {
            out0_Q10 = out0_Q10 + SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10);
            out1_Q10 = out1_Q10 + SILK_FIX_CONST(NLSF_QUANT_LEVEL_ADJ, 10);
        }
        out0_Q10_table[i + NLSF_QUANT_MAX_AMPLITUDE_EXT] = silk_RSHIFT(silk_SMULBB(out0_Q10, quant_step_size_Q16), 16);
        out1_Q10_table[i + NLSF_QUANT_MAX_AMPLITUDE_EXT] = silk_RSHIFT(silk_SMULBB(out1_Q10, quant_step_size_Q16), 16);
    }

    assert((NLSF_QUANT_DEL_DEC_STATES & (NLSF_QUANT_DEL_DEC_STATES - 1)) == 0); /* must be power of two */

    nStates = 1;
    RD_Q25[0] = 0;
    prev_out_Q10[0] = 0;
    for (i = order - 1; i >= 0; i--) {
        rates_Q5 = &ec_rates_Q5[ec_ix[i]];
        in_Q10 = x_Q10[i];
        for (j = 0; j < nStates; j++) {
            pred_Q10 = silk_RSHIFT(silk_SMULBB((int16_t)pred_coef_Q8[i], prev_out_Q10[j]), 8);
            res_Q10 = silk_SUB16(in_Q10, pred_Q10);
            ind_tmp = silk_RSHIFT(silk_SMULBB(inv_quant_step_size_Q6, res_Q10), 16);
            ind_tmp = silk_LIMIT(ind_tmp, -NLSF_QUANT_MAX_AMPLITUDE_EXT, NLSF_QUANT_MAX_AMPLITUDE_EXT - 1);
            ind[j][i] = (int8_t)ind_tmp;

            /* compute outputs for ind_tmp and ind_tmp + 1 */
            out0_Q10 = out0_Q10_table[ind_tmp + NLSF_QUANT_MAX_AMPLITUDE_EXT];
            out1_Q10 = out1_Q10_table[ind_tmp + NLSF_QUANT_MAX_AMPLITUDE_EXT];

            out0_Q10 = out0_Q10 + pred_Q10;
            out1_Q10 = out1_Q10 + pred_Q10;
            prev_out_Q10[j] = out0_Q10;
            prev_out_Q10[j + nStates] = out1_Q10;

            /* compute RD for ind_tmp and ind_tmp + 1 */
            if (ind_tmp + 1 >= NLSF_QUANT_MAX_AMPLITUDE) {
                if (ind_tmp + 1 == NLSF_QUANT_MAX_AMPLITUDE) {
                    rate0_Q5 = rates_Q5[ind_tmp + NLSF_QUANT_MAX_AMPLITUDE];
                    rate1_Q5 = 280;
                } else {
                    rate0_Q5 = silk_SMLABB(280 - 43 * NLSF_QUANT_MAX_AMPLITUDE, 43, ind_tmp);
                    rate1_Q5 = rate0_Q5 + 43;
                }
            } else if (ind_tmp <= -NLSF_QUANT_MAX_AMPLITUDE) {
                if (ind_tmp == -NLSF_QUANT_MAX_AMPLITUDE) {
                    rate0_Q5 = 280;
                    rate1_Q5 = rates_Q5[ind_tmp + 1 + NLSF_QUANT_MAX_AMPLITUDE];
                } else {
                    rate0_Q5 = silk_SMLABB(280 - 43 * NLSF_QUANT_MAX_AMPLITUDE, -43, ind_tmp);
                    rate1_Q5 = silk_SUB16(rate0_Q5, 43);
                }
            } else {
                rate0_Q5 = rates_Q5[ind_tmp + NLSF_QUANT_MAX_AMPLITUDE];
                rate1_Q5 = rates_Q5[ind_tmp + 1 + NLSF_QUANT_MAX_AMPLITUDE];
            }
            RD_tmp_Q25 = RD_Q25[j];
            diff_Q10 = silk_SUB16(in_Q10, out0_Q10);
            RD_Q25[j] = silk_SMLABB(silk_MLA(RD_tmp_Q25, silk_SMULBB(diff_Q10, diff_Q10), w_Q5[i]), mu_Q20, rate0_Q5);
            diff_Q10 = silk_SUB16(in_Q10, out1_Q10);
            RD_Q25[j + nStates] = silk_SMLABB(silk_MLA(RD_tmp_Q25, silk_SMULBB(diff_Q10, diff_Q10), w_Q5[i]), mu_Q20, rate1_Q5);
        }

        if (nStates <= NLSF_QUANT_DEL_DEC_STATES / 2) {
            /* double number of states and copy */
            for (j = 0; j < nStates; j++) { ind[j + nStates][i] = ind[j][i] + 1; }
            nStates = silk_LSHIFT(nStates, 1);
            for (j = nStates; j < NLSF_QUANT_DEL_DEC_STATES; j++) { ind[j][i] = ind[j - nStates][i]; }
        } else {
            /* sort lower and upper half of RD_Q25, pairwise */
            for (j = 0; j < NLSF_QUANT_DEL_DEC_STATES; j++) {
                if (RD_Q25[j] > RD_Q25[j + NLSF_QUANT_DEL_DEC_STATES]) {
                    RD_max_Q25[j] = RD_Q25[j];
                    RD_min_Q25[j] = RD_Q25[j + NLSF_QUANT_DEL_DEC_STATES];
                    RD_Q25[j] = RD_min_Q25[j];
                    RD_Q25[j + NLSF_QUANT_DEL_DEC_STATES] = RD_max_Q25[j];
                    /* swap prev_out values */
                    out0_Q10 = prev_out_Q10[j];
                    prev_out_Q10[j] = prev_out_Q10[j + NLSF_QUANT_DEL_DEC_STATES];
                    prev_out_Q10[j + NLSF_QUANT_DEL_DEC_STATES] = out0_Q10;
                    ind_sort[j] = j + NLSF_QUANT_DEL_DEC_STATES;
                } else {
                    RD_min_Q25[j] = RD_Q25[j];
                    RD_max_Q25[j] = RD_Q25[j + NLSF_QUANT_DEL_DEC_STATES];
                    ind_sort[j] = j;
                }
            }
            /* compare the highest RD values of the winning half with the lowest one in the losing half, and copy if
             * necessary */
            /* afterwards ind_sort[] will contain the indices of the NLSF_QUANT_DEL_DEC_STATES winning RD values */
            while (1) {
                min_max_Q25 = silk_int32_MAX;
                max_min_Q25 = 0;
                ind_min_max = 0;
                ind_max_min = 0;
                for (j = 0; j < NLSF_QUANT_DEL_DEC_STATES; j++) {
                    if (min_max_Q25 > RD_max_Q25[j]) {
                        min_max_Q25 = RD_max_Q25[j];
                        ind_min_max = j;
                    }
                    if (max_min_Q25 < RD_min_Q25[j]) {
                        max_min_Q25 = RD_min_Q25[j];
                        ind_max_min = j;
                    }
                }
                if (min_max_Q25 >= max_min_Q25) { break; }
                /* copy ind_min_max to ind_max_min */
                ind_sort[ind_max_min] = ind_sort[ind_min_max] ^ NLSF_QUANT_DEL_DEC_STATES;
                RD_Q25[ind_max_min] = RD_Q25[ind_min_max + NLSF_QUANT_DEL_DEC_STATES];
                prev_out_Q10[ind_max_min] = prev_out_Q10[ind_min_max + NLSF_QUANT_DEL_DEC_STATES];
                RD_min_Q25[ind_max_min] = 0;
                RD_max_Q25[ind_min_max] = silk_int32_MAX;
                memcpy(ind[ind_max_min], ind[ind_min_max], MAX_LPC_ORDER * sizeof(int8_t));
            }
            /* increment index if it comes from the upper half */
            for (j = 0; j < NLSF_QUANT_DEL_DEC_STATES; j++) { ind[j][i] += silk_RSHIFT(ind_sort[j], NLSF_QUANT_DEL_DEC_STATES_LOG2); }
        }
    }

    /* last sample: find winner, copy indices and return RD value */
    ind_tmp = 0;
    min_Q25 = silk_int32_MAX;
    for (j = 0; j < 2 * NLSF_QUANT_DEL_DEC_STATES; j++) {
        if (min_Q25 > RD_Q25[j]) {
            min_Q25 = RD_Q25[j];
            ind_tmp = j;
        }
    }
    for (j = 0; j < order; j++) {
        indices[j] = ind[ind_tmp & (NLSF_QUANT_DEL_DEC_STATES - 1)][j];
        assert(indices[j] >= -NLSF_QUANT_MAX_AMPLITUDE_EXT);
        assert(indices[j] <= NLSF_QUANT_MAX_AMPLITUDE_EXT);
    }
    indices[0] += silk_RSHIFT(ind_tmp, NLSF_QUANT_DEL_DEC_STATES_LOG2);
    assert(indices[0] <= NLSF_QUANT_MAX_AMPLITUDE_EXT);
    assert(min_Q25 >= 0);
    return min_Q25;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* NLSF stabilizer, for a single input data vector */
void SilkDecoder::silk_NLSF_stabilize(int16_t*       NLSF_Q15,      /* I/O   Unstable/stabilized normalized LSF vector in Q15 [L]       */
                                      const int16_t* NDeltaMin_Q15, /* I     Min distance vector, NDeltaMin_Q15[L] must be >= 1 [L+1]   */
                                      const int32_t  L              /* I     Number of NLSF parameters in the input vector              */
) {
    int32_t i, I = 0, k, loops;
    int16_t center_freq_Q15;
    int32_t diff_Q15, min_diff_Q15, min_center_Q15, max_center_Q15;

    /* This is necessary to ensure an output within range of a int16_t */
    assert(NDeltaMin_Q15[L] >= 1);

    for (loops = 0; loops < MAX_LOOPS; loops++) {
        /**************************/
        /* Find smallest distance */
        /**************************/
        /* First element */
        min_diff_Q15 = NLSF_Q15[0] - NDeltaMin_Q15[0];
        I = 0;
        /* Middle elements */
        for (i = 1; i <= L - 1; i++) {
            diff_Q15 = NLSF_Q15[i] - (NLSF_Q15[i - 1] + NDeltaMin_Q15[i]);
            if (diff_Q15 < min_diff_Q15) {
                min_diff_Q15 = diff_Q15;
                I = i;
            }
        }
        /* Last element */
        diff_Q15 = (1 << 15) - (NLSF_Q15[L - 1] + NDeltaMin_Q15[L]);
        if (diff_Q15 < min_diff_Q15) {
            min_diff_Q15 = diff_Q15;
            I = L;
        }

        /***************************************************/
        /* Now check if the smallest distance non-negative */
        /***************************************************/
        if (min_diff_Q15 >= 0) { return; }

        if (I == 0) {
            /* Move away from lower limit */
            NLSF_Q15[0] = NDeltaMin_Q15[0];
        } else if (I == L) {
            /* Move away from higher limit */
            NLSF_Q15[L - 1] = (1 << 15) - NDeltaMin_Q15[L];
        } else {
            /* Find the lower extreme for the location of the current center frequency */
            min_center_Q15 = 0;
            for (k = 0; k < I; k++) { min_center_Q15 += NDeltaMin_Q15[k]; }
            min_center_Q15 += silk_RSHIFT(NDeltaMin_Q15[I], 1);

            /* Find the upper extreme for the location of the current center frequency */
            max_center_Q15 = 1 << 15;
            for (k = L; k > I; k--) { max_center_Q15 -= NDeltaMin_Q15[k]; }
            max_center_Q15 -= silk_RSHIFT(NDeltaMin_Q15[I], 1);

            /* Move apart, sorted by value, keeping the same center frequency */
            center_freq_Q15 = (int16_t)silk_LIMIT_32(silk_RSHIFT_ROUND((int32_t)NLSF_Q15[I - 1] + (int32_t)NLSF_Q15[I], 1), min_center_Q15, max_center_Q15);
            NLSF_Q15[I - 1] = center_freq_Q15 - silk_RSHIFT(NDeltaMin_Q15[I], 1);
            NLSF_Q15[I] = NLSF_Q15[I - 1] + NDeltaMin_Q15[I];
        }
    }

    /* Safe and simple fall back method, which is less ideal than the above */
    if (loops == MAX_LOOPS) {
        /* Insertion sort (fast for already almost sorted arrays):   */
        /* Best case:  O(n)   for an already sorted array            */
        /* Worst case: O(n^2) for an inversely sorted array          */
        silk_insertion_sort_increasing_all_values_int16(&NLSF_Q15[0], L);

        /* First NLSF should be no less than NDeltaMin[0] */
        NLSF_Q15[0] = silk_max_int(NLSF_Q15[0], NDeltaMin_Q15[0]);

        /* Keep delta_min distance between the NLSFs */
        for (i = 1; i < L; i++) NLSF_Q15[i] = silk_max_int(NLSF_Q15[i], silk_ADD_SAT16(NLSF_Q15[i - 1], NDeltaMin_Q15[i]));

        /* Last NLSF should be no higher than 1 - NDeltaMin[L] */
        NLSF_Q15[L - 1] = silk_min_int(NLSF_Q15[L - 1], (1 << 15) - NDeltaMin_Q15[L]);

        /* Keep NDeltaMin distance between the NLSFs */
        for (i = L - 2; i >= 0; i--) NLSF_Q15[i] = silk_min_int(NLSF_Q15[i], NLSF_Q15[i + 1] - NDeltaMin_Q15[i + 1]);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Unpack predictor values and indices for entropy coding tables */
void SilkDecoder::silk_NLSF_unpack(int16_t                      ec_ix[],   /* O    Indices to entropy tables [ LPC_ORDER ]     */
                                   uint8_t                      pred_Q8[], /* O    LSF predictor [ LPC_ORDER ]                 */
                                   const silk_NLSF_CB_struct_t* psNLSF_CB, /* I    Codebook object                             */
                                   const int32_t                CB1_index  /* I    Index of vector in first LSF codebook       */
) {
    int32_t        i;
    uint8_t        entry;
    const uint8_t* ec_sel_ptr;

    ec_sel_ptr = &psNLSF_CB->ec_sel[CB1_index * psNLSF_CB->order / 2];
    for (i = 0; i < psNLSF_CB->order; i += 2) {
        entry = *ec_sel_ptr++;
        ec_ix[i] = silk_SMULBB(silk_RSHIFT(entry, 1) & 7, 2 * NLSF_QUANT_MAX_AMPLITUDE + 1);
        pred_Q8[i] = psNLSF_CB->pred_Q8[i + (entry & 1) * (psNLSF_CB->order - 1)];
        ec_ix[i + 1] = silk_SMULBB(silk_RSHIFT(entry, 5) & 7, 2 * NLSF_QUANT_MAX_AMPLITUDE + 1);
        pred_Q8[i + 1] = psNLSF_CB->pred_Q8[i + (silk_RSHIFT(entry, 4) & 1) * (psNLSF_CB->order - 1) + 1];
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Laroia low complexity NLSF weights */
void SilkDecoder::silk_NLSF_VQ_weights_laroia(int16_t*       pNLSFW_Q_OUT, /* O     Pointer to input vector weights [D]    */
                                              const int16_t* pNLSF_Q15,    /* I     Pointer to input vector         [D] */
                                              const int32_t  D             /* I     Input vector dimension (even)                              */
) {
    int32_t k;
    int32_t tmp1_int, tmp2_int;

    assert(D > 0);
    assert((D & 1) == 0);

    /* First value */
    tmp1_int = silk_max_int(pNLSF_Q15[0], 1);
    tmp1_int = silk_DIV32_16((int32_t)1 << (15 + NLSF_W_Q), tmp1_int);
    tmp2_int = silk_max_int(pNLSF_Q15[1] - pNLSF_Q15[0], 1);
    tmp2_int = silk_DIV32_16((int32_t)1 << (15 + NLSF_W_Q), tmp2_int);
    pNLSFW_Q_OUT[0] = (int16_t)silk_min_int(tmp1_int + tmp2_int, silk_int16_MAX);
    assert(pNLSFW_Q_OUT[0] > 0);

    /* Main loop */
    for (k = 1; k < D - 1; k += 2) {
        tmp1_int = silk_max_int(pNLSF_Q15[k + 1] - pNLSF_Q15[k], 1);
        tmp1_int = silk_DIV32_16((int32_t)1 << (15 + NLSF_W_Q), tmp1_int);
        pNLSFW_Q_OUT[k] = (int16_t)silk_min_int(tmp1_int + tmp2_int, silk_int16_MAX);
        assert(pNLSFW_Q_OUT[k] > 0);

        tmp2_int = silk_max_int(pNLSF_Q15[k + 2] - pNLSF_Q15[k + 1], 1);
        tmp2_int = silk_DIV32_16((int32_t)1 << (15 + NLSF_W_Q), tmp2_int);
        pNLSFW_Q_OUT[k + 1] = (int16_t)silk_min_int(tmp1_int + tmp2_int, silk_int16_MAX);
        assert(pNLSFW_Q_OUT[k + 1] > 0);
    }

    /* Last value */
    tmp1_int = silk_max_int((1 << 15) - pNLSF_Q15[D - 1], 1);
    tmp1_int = silk_DIV32_16((int32_t)1 << (15 + NLSF_W_Q), tmp1_int);
    pNLSFW_Q_OUT[D - 1] = (int16_t)silk_min_int(tmp1_int + tmp2_int, silk_int16_MAX);
    assert(pNLSFW_Q_OUT[D - 1] > 0);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Compute quantization errors for an LPC_order element input vector for a VQ codebook */
void SilkDecoder::silk_NLSF_VQ(int32_t       err_Q24[],  /* O    Quantization errors [K]                     */
                               const int16_t in_Q15[],   /* I    Input vectors to be quantized [LPC_order]   */
                               const uint8_t pCB_Q8[],   /* I    Codebook vectors [K*LPC_order]              */
                               const int16_t pWght_Q9[], /* I    Codebook weights [K*LPC_order]              */
                               const int32_t K,          /* I    Number of codebook vectors                  */
                               const int32_t LPC_order   /* I    Number of LPCs                              */
) {
    int32_t        i, m;
    int32_t        diff_Q15, diffw_Q24, sum_error_Q24, pred_Q24;
    const int16_t* w_Q9_ptr;
    const uint8_t* cb_Q8_ptr;

    assert((LPC_order & 1) == 0);

    /* Loop over codebook */
    cb_Q8_ptr = pCB_Q8;
    w_Q9_ptr = pWght_Q9;
    for (i = 0; i < K; i++) {
        sum_error_Q24 = 0;
        pred_Q24 = 0;
        for (m = LPC_order - 2; m >= 0; m -= 2) {
            /* Compute weighted absolute predictive quantization error for index m + 1 */
            diff_Q15 = silk_SUB_LSHIFT32(in_Q15[m + 1], (int32_t)cb_Q8_ptr[m + 1], 7); /* range: [ -32767 : 32767 ]*/
            diffw_Q24 = silk_SMULBB(diff_Q15, w_Q9_ptr[m + 1]);
            sum_error_Q24 = sum_error_Q24 + silk_abs(silk_SUB_RSHIFT32(diffw_Q24, pred_Q24, 1));
            pred_Q24 = diffw_Q24;

            /* Compute weighted absolute predictive quantization error for index m */
            diff_Q15 = silk_SUB_LSHIFT32(in_Q15[m], (int32_t)cb_Q8_ptr[m], 7); /* range: [ -32767 : 32767 ]*/
            diffw_Q24 = silk_SMULBB(diff_Q15, w_Q9_ptr[m]);
            sum_error_Q24 = sum_error_Q24 + silk_abs(silk_SUB_RSHIFT32(diffw_Q24, pred_Q24, 1));
            pred_Q24 = diffw_Q24;

            assert(sum_error_Q24 >= 0);
        }
        err_Q24[i] = sum_error_Q24;
        cb_Q8_ptr += LPC_order;
        w_Q9_ptr += LPC_order;
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::silk_PLC_Reset(uint8_t n) { /* I/O Decoder state        */
    m_channel_state[n].sPLC.pitchL_Q8 = silk_LSHIFT(m_channel_state[n].frame_length, 8 - 1);
    m_channel_state[n].sPLC.prevGain_Q16[0] = SILK_FIX_CONST(1, 16);
    m_channel_state[n].sPLC.prevGain_Q16[1] = SILK_FIX_CONST(1, 16);
    m_channel_state[n].sPLC.subfr_length = 20;
    m_channel_state[n].sPLC.nb_subfr = 2;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SilkDecoder::silk_PLC(uint8_t n, int16_t frame[], int32_t lost) {
    /* PLC control function */
    if (m_channel_state[n].fs_kHz != m_channel_state[n].sPLC.fs_kHz) {
        silk_PLC_Reset(n);
        m_channel_state[n].sPLC.fs_kHz = m_channel_state[n].fs_kHz;
    }

    if (lost) {
        /****************************/
        /* Generate Signal          */
        /****************************/
        silk_PLC_conceal(n, frame);

        m_channel_state[n].lossCnt++;
    } else {
        /****************************/
        /* Update state             */
        /****************************/
        silk_PLC_update(n);
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Update state of PLC                            */
void SilkDecoder::silk_PLC_update(uint8_t n) {
    int32_t            LTP_Gain_Q14, temp_LTP_Gain_Q14;
    int32_t            i, j;
    silk_PLC_struct_t* psPLC;

    psPLC = &m_channel_state[n].sPLC;

    /* Update parameters used in case of packet loss */
    m_channel_state[n].prevSignalType = m_channel_state[n].indices.signalType;
    LTP_Gain_Q14 = 0;
    if (m_channel_state[n].indices.signalType == TYPE_VOICED) {
        /* Find the parameters for the last subframe which contains a pitch pulse */
        for (j = 0; j * m_channel_state[n].subfr_length < m_silk_decoder_control->pitchL[m_channel_state[n].nb_subfr - 1]; j++) {
            if (j == m_channel_state[n].nb_subfr) { break; }
            temp_LTP_Gain_Q14 = 0;
            for (i = 0; i < LTP_ORDER; i++) { temp_LTP_Gain_Q14 += m_silk_decoder_control->LTPCoef_Q14[(m_channel_state[n].nb_subfr - 1 - j) * LTP_ORDER + i]; }
            if (temp_LTP_Gain_Q14 > LTP_Gain_Q14) {
                LTP_Gain_Q14 = temp_LTP_Gain_Q14;
                memcpy(psPLC->LTPCoef_Q14, &m_silk_decoder_control->LTPCoef_Q14[silk_SMULBB(m_channel_state[n].nb_subfr - 1 - j, LTP_ORDER)], LTP_ORDER * sizeof(int16_t));
                psPLC->pitchL_Q8 = silk_LSHIFT(m_silk_decoder_control->pitchL[m_channel_state[n].nb_subfr - 1 - j], 8);
            }
        }

        memset(psPLC->LTPCoef_Q14, 0, LTP_ORDER * sizeof(int16_t));
        psPLC->LTPCoef_Q14[LTP_ORDER / 2] = LTP_Gain_Q14;

        /* Limit LT coefs */
        if (LTP_Gain_Q14 < V_PITCH_GAIN_START_MIN_Q14) {
            int32_t scale_Q10;
            int32_t tmp;

            tmp = silk_LSHIFT(V_PITCH_GAIN_START_MIN_Q14, 10);
            scale_Q10 = silk_DIV32(tmp, silk_max(LTP_Gain_Q14, 1));
            for (i = 0; i < LTP_ORDER; i++) { psPLC->LTPCoef_Q14[i] = silk_RSHIFT(silk_SMULBB(psPLC->LTPCoef_Q14[i], scale_Q10), 10); }
        } else if (LTP_Gain_Q14 > V_PITCH_GAIN_START_MAX_Q14) {
            int32_t scale_Q14;
            int32_t tmp;

            tmp = silk_LSHIFT(V_PITCH_GAIN_START_MAX_Q14, 14);
            scale_Q14 = silk_DIV32(tmp, silk_max(LTP_Gain_Q14, 1));
            for (i = 0; i < LTP_ORDER; i++) { psPLC->LTPCoef_Q14[i] = silk_RSHIFT(silk_SMULBB(psPLC->LTPCoef_Q14[i], scale_Q14), 14); }
        }
    } else {
        psPLC->pitchL_Q8 = silk_LSHIFT(silk_SMULBB(m_channel_state[n].fs_kHz, 18), 8);
        memset(psPLC->LTPCoef_Q14, 0, LTP_ORDER * sizeof(int16_t));
    }

    /* Save LPC coeficients */
    memcpy(psPLC->prevLPC_Q12, m_silk_decoder_control->PredCoef_Q12[1], m_channel_state[n].LPC_order * sizeof(int16_t));
    psPLC->prevLTP_scale_Q14 = m_silk_decoder_control->LTP_scale_Q14;

    /* Save last two gains */
    memcpy(psPLC->prevGain_Q16, &m_silk_decoder_control->Gains_Q16[m_channel_state[n].nb_subfr - 2], 2 * sizeof(int32_t));

    psPLC->subfr_length = m_channel_state[n].subfr_length;
    psPLC->nb_subfr = m_channel_state[n].nb_subfr;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SilkDecoder::silk_PLC_energy(int32_t* energy1, int32_t* shift1, int32_t* energy2, int32_t* shift2, const int32_t* exc_Q14, const int32_t* prevGain_Q10, int subfr_length, int nb_subfr) {
    int             i, k;
    int16_t*        exc_buf_ptr;
    ps_ptr<int16_t> exc_buf;
    exc_buf.alloc_array(2 * subfr_length);
    /* Find random noise component */
    /* Scale previous excitation signal */
    exc_buf_ptr = exc_buf.get();
    for (k = 0; k < 2; k++) {
        for (i = 0; i < subfr_length; i++) { exc_buf_ptr[i] = (int16_t)silk_SAT16(silk_RSHIFT(silk_SMULWW(exc_Q14[i + (k + nb_subfr - 2) * subfr_length], prevGain_Q10[k]), 8)); }
        exc_buf_ptr += subfr_length;
    }
    /* Find the subframe with lowest energy of the last two and use that as random noise generator */
    silk_sum_sqr_shift(energy1, shift1, exc_buf.get(), subfr_length);
    silk_sum_sqr_shift(energy2, shift2, &exc_buf[subfr_length], subfr_length);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SilkDecoder::silk_PLC_conceal(uint8_t n, int16_t frame[]) {
    int32_t            i, j, k;
    int32_t            lag, idx, sLTP_buf_idx, shift1, shift2;
    int32_t            rand_seed, harm_Gain_Q15, rand_Gain_Q15, inv_gain_Q30;
    int32_t            energy1, energy2, *rand_ptr, *pred_lag_ptr;
    int32_t            LPC_pred_Q10, LTP_pred_Q12;
    int16_t            rand_scale_Q14;
    int16_t*           B_Q14;
    int32_t*           sLPC_Q14_ptr;
    int16_t            A_Q12[MAX_LPC_ORDER];
    silk_PLC_struct_t* psPLC = &m_channel_state[n].sPLC;
    int32_t            prevGain_Q10[2];

    ps_ptr<int32_t> sLTP_Q14;
    sLTP_Q14.alloc_array(m_channel_state[n].ltp_mem_length + m_channel_state[n].frame_length);
    ps_ptr<int16_t> sLTP;
    sLTP.alloc_array(m_channel_state[n].ltp_mem_length);

    prevGain_Q10[0] = silk_RSHIFT(psPLC->prevGain_Q16[0], 6);
    prevGain_Q10[1] = silk_RSHIFT(psPLC->prevGain_Q16[1], 6);

    if (m_channel_state[n].first_frame_after_reset) { memset(psPLC->prevLPC_Q12, 0, sizeof(psPLC->prevLPC_Q12)); }

    silk_PLC_energy(&energy1, &shift1, &energy2, &shift2, m_channel_state[n].exc_Q14, prevGain_Q10, m_channel_state[n].subfr_length, m_channel_state[n].nb_subfr);

    if (silk_RSHIFT(energy1, shift2) < silk_RSHIFT(energy2, shift1)) {
        /* First sub-frame has lowest energy */
        rand_ptr = &m_channel_state[n].exc_Q14[silk_max_int(0, (psPLC->nb_subfr - 1) * psPLC->subfr_length - RAND_BUF_SIZE)];
    } else {
        /* Second sub-frame has lowest energy */
        rand_ptr = &m_channel_state[n].exc_Q14[silk_max_int(0, psPLC->nb_subfr * psPLC->subfr_length - RAND_BUF_SIZE)];
    }

    /* Set up Gain to random noise component */
    B_Q14 = psPLC->LTPCoef_Q14;
    rand_scale_Q14 = psPLC->randScale_Q14;

    /* Set up attenuation gains */
    harm_Gain_Q15 = HARM_ATT_Q15[silk_min_int(NB_ATT - 1, m_channel_state[n].lossCnt)];
    if (m_channel_state[n].prevSignalType == TYPE_VOICED) {
        rand_Gain_Q15 = PLC_RAND_ATTENUATE_V_Q15[silk_min_int(NB_ATT - 1, m_channel_state[n].lossCnt)];
    } else {
        rand_Gain_Q15 = PLC_RAND_ATTENUATE_UV_Q15[silk_min_int(NB_ATT - 1, m_channel_state[n].lossCnt)];
    }

    /* LPC concealment. Apply BWE to previous LPC */
    silk_bwexpander(psPLC->prevLPC_Q12, m_channel_state[n].LPC_order, SILK_FIX_CONST(BWE_COEF, 16));

    /* Preload LPC coeficients to array on stack. Gives small performance gain */
    memcpy(A_Q12, psPLC->prevLPC_Q12, m_channel_state[n].LPC_order * sizeof(int16_t));

    /* First Lost frame */
    if (m_channel_state[n].lossCnt == 0) {
        rand_scale_Q14 = 1 << 14;

        /* Reduce random noise Gain for voiced frames */
        if (m_channel_state[n].prevSignalType == TYPE_VOICED) {
            for (i = 0; i < LTP_ORDER; i++) { rand_scale_Q14 -= B_Q14[i]; }
            rand_scale_Q14 = silk_max_16(3277, rand_scale_Q14); /* 0.2 */
            rand_scale_Q14 = (int16_t)silk_RSHIFT(silk_SMULBB(rand_scale_Q14, psPLC->prevLTP_scale_Q14), 14);
        } else {
            /* Reduce random noise for unvoiced frames with high LPC gain */
            int32_t invGain_Q30, down_scale_Q30;

            invGain_Q30 = silk_LPC_inverse_pred_gain(psPLC->prevLPC_Q12, m_channel_state[n].LPC_order);

            down_scale_Q30 = silk_min_32(silk_RSHIFT((int32_t)1 << 30, LOG2_INV_LPC_GAIN_HIGH_THRES), invGain_Q30);
            down_scale_Q30 = silk_max_32(silk_RSHIFT((int32_t)1 << 30, LOG2_INV_LPC_GAIN_LOW_THRES), down_scale_Q30);
            down_scale_Q30 = silk_LSHIFT(down_scale_Q30, LOG2_INV_LPC_GAIN_HIGH_THRES);

            rand_Gain_Q15 = silk_RSHIFT(silk_SMULWB(down_scale_Q30, rand_Gain_Q15), 14);
        }
    }

    rand_seed = psPLC->rand_seed;
    lag = silk_RSHIFT_ROUND(psPLC->pitchL_Q8, 8);
    sLTP_buf_idx = m_channel_state[n].ltp_mem_length;

    /* Rewhiten LTP state */
    idx = m_channel_state[n].ltp_mem_length - lag - m_channel_state[n].LPC_order - LTP_ORDER / 2;
    assert(idx > 0);
    silk_LPC_analysis_filter(&sLTP[idx], &m_channel_state[n].outBuf[idx], A_Q12, m_channel_state[n].ltp_mem_length - idx, m_channel_state[n].LPC_order);
    /* Scale LTP state */
    inv_gain_Q30 = silk_INVERSE32_varQ(psPLC->prevGain_Q16[1], 46);
    inv_gain_Q30 = silk_min(inv_gain_Q30, silk_int32_MAX >> 1);
    for (i = idx + m_channel_state[n].LPC_order; i < m_channel_state[n].ltp_mem_length; i++) { sLTP_Q14[i] = silk_SMULWB(inv_gain_Q30, sLTP[i]); }

    /***************************/
    /* LTP synthesis filtering */
    /***************************/
    for (k = 0; k < m_channel_state[n].nb_subfr; k++) {
        /* Set up pointer */
        pred_lag_ptr = &sLTP_Q14[sLTP_buf_idx - lag + LTP_ORDER / 2];
        for (i = 0; i < m_channel_state[n].subfr_length; i++) {
            /* Unrolled loop */
            /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
            LTP_pred_Q12 = 2;
            LTP_pred_Q12 = silk_SMLAWB(LTP_pred_Q12, pred_lag_ptr[0], B_Q14[0]);
            LTP_pred_Q12 = silk_SMLAWB(LTP_pred_Q12, pred_lag_ptr[-1], B_Q14[1]);
            LTP_pred_Q12 = silk_SMLAWB(LTP_pred_Q12, pred_lag_ptr[-2], B_Q14[2]);
            LTP_pred_Q12 = silk_SMLAWB(LTP_pred_Q12, pred_lag_ptr[-3], B_Q14[3]);
            LTP_pred_Q12 = silk_SMLAWB(LTP_pred_Q12, pred_lag_ptr[-4], B_Q14[4]);
            pred_lag_ptr++;

            /* Generate LPC excitation */
            rand_seed = silk_RAND(rand_seed);
            idx = silk_RSHIFT(rand_seed, 25) & RAND_BUF_MASK;
            sLTP_Q14[sLTP_buf_idx] = silk_LSHIFT32(silk_SMLAWB(LTP_pred_Q12, rand_ptr[idx], rand_scale_Q14), 2);
            sLTP_buf_idx++;
        }

        /* Gradually reduce LTP gain */
        for (j = 0; j < LTP_ORDER; j++) { B_Q14[j] = silk_RSHIFT(silk_SMULBB(harm_Gain_Q15, B_Q14[j]), 15); }
        if (m_channel_state[n].indices.signalType != TYPE_NO_VOICE_ACTIVITY) {
            /* Gradually reduce excitation gain */
            rand_scale_Q14 = silk_RSHIFT(silk_SMULBB(rand_scale_Q14, rand_Gain_Q15), 15);
        }

        /* Slowly increase pitch lag */
        psPLC->pitchL_Q8 = silk_SMLAWB(psPLC->pitchL_Q8, psPLC->pitchL_Q8, PITCH_DRIFT_FAC_Q16);
        psPLC->pitchL_Q8 = silk_min_32(psPLC->pitchL_Q8, silk_LSHIFT(silk_SMULBB(MAX_PITCH_LAG_MS, m_channel_state[n].fs_kHz), 8));
        lag = silk_RSHIFT_ROUND(psPLC->pitchL_Q8, 8);
    }

    /***************************/
    /* LPC synthesis filtering */
    /***************************/
    sLPC_Q14_ptr = &sLTP_Q14[m_channel_state[n].ltp_mem_length - MAX_LPC_ORDER];

    /* Copy LPC state */
    memcpy(sLPC_Q14_ptr, m_channel_state[n].sLPC_Q14_buf, MAX_LPC_ORDER * sizeof(int32_t));

    assert(m_channel_state[n].LPC_order >= 10); /* check that unrolling works */
    for (i = 0; i < m_channel_state[n].frame_length; i++) {
        /* partly unrolled */
        /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
        LPC_pred_Q10 = silk_RSHIFT(m_channel_state[n].LPC_order, 1);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 1], A_Q12[0]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 2], A_Q12[1]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 3], A_Q12[2]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 4], A_Q12[3]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 5], A_Q12[4]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 6], A_Q12[5]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 7], A_Q12[6]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 8], A_Q12[7]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 9], A_Q12[8]);
        LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - 10], A_Q12[9]);
        for (j = 10; j < m_channel_state[n].LPC_order; j++) { LPC_pred_Q10 = silk_SMLAWB(LPC_pred_Q10, sLPC_Q14_ptr[MAX_LPC_ORDER + i - j - 1], A_Q12[j]); }

        /* Add prediction to LPC excitation */
        sLPC_Q14_ptr[MAX_LPC_ORDER + i] = silk_ADD_SAT32(sLPC_Q14_ptr[MAX_LPC_ORDER + i], silk_LSHIFT_SAT32(LPC_pred_Q10, 4));

        /* Scale with Gain */
        frame[i] = (int16_t)silk_SAT16(silk_SAT16(silk_RSHIFT_ROUND(silk_SMULWW(sLPC_Q14_ptr[MAX_LPC_ORDER + i], prevGain_Q10[1]), 8)));
    }

    /* Save LPC state */
    memcpy(m_channel_state[n].sLPC_Q14_buf, &sLPC_Q14_ptr[m_channel_state[n].frame_length], MAX_LPC_ORDER * sizeof(int32_t));

    /**************************************/
    /* Update states                      */
    /**************************************/
    psPLC->rand_seed = rand_seed;
    psPLC->randScale_Q14 = rand_scale_Q14;
    for (i = 0; i < MAX_NB_SUBFR; i++) { m_silk_decoder_control->pitchL[i] = lag; }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Glues concealed frames with new good received frames */
void SilkDecoder::silk_PLC_glue_frames(uint8_t n, int16_t frame[], int32_t length) {
    int32_t            i, energy_shift;
    int32_t            energy;
    silk_PLC_struct_t* psPLC;
    psPLC = &m_channel_state[n].sPLC;

    if (m_channel_state[n].lossCnt) {
        /* Calculate energy in concealed residual */
        silk_sum_sqr_shift(&psPLC->conc_energy, &psPLC->conc_energy_shift, frame, length);

        psPLC->last_frame_lost = 1;
    } else {
        if (m_channel_state[n].sPLC.last_frame_lost) {
            /* Calculate residual in decoded signal if last frame was lost */
            silk_sum_sqr_shift(&energy, &energy_shift, frame, length);

            /* Normalize energies */
            if (energy_shift > psPLC->conc_energy_shift) {
                psPLC->conc_energy = silk_RSHIFT(psPLC->conc_energy, energy_shift - psPLC->conc_energy_shift);
            } else if (energy_shift < psPLC->conc_energy_shift) {
                energy = silk_RSHIFT(energy, psPLC->conc_energy_shift - energy_shift);
            }

            /* Fade in the energy difference */
            if (energy > psPLC->conc_energy) {
                int32_t frac_Q24, LZ;
                int32_t gain_Q16, slope_Q16;

                LZ = silk_CLZ32(psPLC->conc_energy);
                LZ = LZ - 1;
                psPLC->conc_energy = silk_LSHIFT(psPLC->conc_energy, LZ);
                energy = silk_RSHIFT(energy, silk_max_32(24 - LZ, 0));

                frac_Q24 = silk_DIV32(psPLC->conc_energy, silk_max(energy, 1));

                gain_Q16 = silk_LSHIFT(silk_SQRT_APPROX(frac_Q24), 4);
                slope_Q16 = silk_DIV32_16(((int32_t)1 << 16) - gain_Q16, length);
                /* Make slope 4x steeper to avoid missing onsets after DTX */
                slope_Q16 = silk_LSHIFT(slope_Q16, 2);

                for (i = 0; i < length; i++) {
                    frame[i] = silk_SMULWB(gain_Q16, frame[i]);
                    gain_Q16 += slope_Q16;
                    if (gain_Q16 > (int32_t)1 << 16) { break; }
                }
            }
        }
        psPLC->last_frame_lost = 0;
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Downsample by a factor 2 */
void SilkDecoder::silk_resampler_down2(int32_t*       S,    /* I/O  State vector [ 2 ]                                          */
                                       int16_t*       out,  /* O    Output signal [ floor(len/2) ]                              */
                                       const int16_t* in,   /* I    Input signal [ len ]                                        */
                                       int32_t        inLen /* I    Number of input samples                                     */
) {
    int32_t k, len2 = silk_RSHIFT32(inLen, 1);
    int32_t in32, out32, Y, X;

    assert(silk_resampler_down2_0 > 0);
    assert(silk_resampler_down2_1 < 0);

    /* Internal variables and state are in Q10 format */
    for (k = 0; k < len2; k++) {
        /* Convert to Q10 */
        in32 = silk_LSHIFT((int32_t)in[2 * k], 10);

        /* All-pass section for even input sample */
        Y = silk_SUB32(in32, S[0]);
        X = silk_SMLAWB(Y, Y, silk_resampler_down2_1);
        out32 = S[0] + X;
        S[0] = in32 + X;

        /* Convert to Q10 */
        in32 = silk_LSHIFT((int32_t)in[2 * k + 1], 10);

        /* All-pass section for odd input sample, and add to output of previous section */
        Y = silk_SUB32(in32, S[1]);
        X = silk_SMULWB(Y, silk_resampler_down2_0);
        out32 = out32 + S[1];
        out32 = out32 + X;
        S[1] = in32 + X;

        /* Add, convert back to int16 and store to output */
        out[k] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(out32, 11));
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Second order AR filter with single delay elements */
void SilkDecoder::silk_resampler_private_AR2(int32_t       S[],      /* I/O  State vector [ 2 ]          */
                                             int32_t       out_Q8[], /* O    Output signal               */
                                             const int16_t in[],     /* I    Input signal                */
                                             const int16_t A_Q14[],  /* I    AR coefficients, Q14        */
                                             int32_t       len       /* I    Signal length               */
) {
    int32_t k;
    int32_t out32;

    for (k = 0; k < len; k++) {
        out32 = silk_ADD_LSHIFT32(S[0], (int32_t)in[k], 8);
        out_Q8[k] = out32;
        out32 = silk_LSHIFT(out32, 2);
        S[0] = silk_SMLAWB(S[1], out32, A_Q14[0]);
        S[1] = silk_SMULWB(out32, A_Q14[1]);
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int16_t* SilkDecoder::silk_resampler_private_down_FIR_INTERPOL(int16_t* out, int32_t* buf, const int16_t* FIR_Coefs, int32_t FIR_Order, int32_t FIR_Fracs, int32_t max_index_Q16,
                                                               int32_t index_increment_Q16) {
    int32_t        index_Q16, res_Q6;
    int32_t*       buf_ptr;
    int32_t        interpol_ind;
    const int16_t* interpol_ptr;

    switch (FIR_Order) {
        case RESAMPLER_DOWN_ORDER_FIR0:
            for (index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16) {
                /* Integer part gives pointer to buffered input */
                buf_ptr = buf + silk_RSHIFT(index_Q16, 16);

                /* Fractional part gives interpolation coefficients */
                interpol_ind = silk_SMULWB(index_Q16 & 0xFFFF, FIR_Fracs);

                /* Inner product */
                interpol_ptr = &FIR_Coefs[RESAMPLER_DOWN_ORDER_FIR0 / 2 * interpol_ind];
                res_Q6 = silk_SMULWB(buf_ptr[0], interpol_ptr[0]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[1], interpol_ptr[1]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[2], interpol_ptr[2]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[3], interpol_ptr[3]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[4], interpol_ptr[4]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[5], interpol_ptr[5]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[6], interpol_ptr[6]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[7], interpol_ptr[7]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[8], interpol_ptr[8]);
                interpol_ptr = &FIR_Coefs[RESAMPLER_DOWN_ORDER_FIR0 / 2 * (FIR_Fracs - 1 - interpol_ind)];
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[17], interpol_ptr[0]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[16], interpol_ptr[1]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[15], interpol_ptr[2]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[14], interpol_ptr[3]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[13], interpol_ptr[4]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[12], interpol_ptr[5]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[11], interpol_ptr[6]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[10], interpol_ptr[7]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[9], interpol_ptr[8]);

                /* Scale down, saturate and store in output array */
                *out++ = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(res_Q6, 6));
            }
            break;
        case RESAMPLER_DOWN_ORDER_FIR1:
            for (index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16) {
                /* Integer part gives pointer to buffered input */
                buf_ptr = buf + silk_RSHIFT(index_Q16, 16);

                /* Inner product */
                res_Q6 = silk_SMULWB(buf_ptr[0] + buf_ptr[23], FIR_Coefs[0]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[1] + buf_ptr[22], FIR_Coefs[1]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[2] + buf_ptr[21], FIR_Coefs[2]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[3] + buf_ptr[20], FIR_Coefs[3]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[4] + buf_ptr[19], FIR_Coefs[4]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[5] + buf_ptr[18], FIR_Coefs[5]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[6] + buf_ptr[17], FIR_Coefs[6]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[7] + buf_ptr[16], FIR_Coefs[7]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[8] + buf_ptr[15], FIR_Coefs[8]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[9] + buf_ptr[14], FIR_Coefs[9]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[10] + buf_ptr[13], FIR_Coefs[10]);
                res_Q6 = silk_SMLAWB(res_Q6, buf_ptr[11] + buf_ptr[12], FIR_Coefs[11]);

                /* Scale down, saturate and store in output array */
                *out++ = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(res_Q6, 6));
            }
            break;
        case RESAMPLER_DOWN_ORDER_FIR2:
            for (index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16) {
                /* Integer part gives pointer to buffered input */
                buf_ptr = buf + silk_RSHIFT(index_Q16, 16);

                /* Inner product */
                res_Q6 = silk_SMULWB(silk_ADD32(buf_ptr[0], buf_ptr[35]), FIR_Coefs[0]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[1], buf_ptr[34]), FIR_Coefs[1]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[2], buf_ptr[33]), FIR_Coefs[2]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[3], buf_ptr[32]), FIR_Coefs[3]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[4], buf_ptr[31]), FIR_Coefs[4]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[5], buf_ptr[30]), FIR_Coefs[5]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[6], buf_ptr[29]), FIR_Coefs[6]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[7], buf_ptr[28]), FIR_Coefs[7]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[8], buf_ptr[27]), FIR_Coefs[8]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[9], buf_ptr[26]), FIR_Coefs[9]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[10], buf_ptr[25]), FIR_Coefs[10]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[11], buf_ptr[24]), FIR_Coefs[11]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[12], buf_ptr[23]), FIR_Coefs[12]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[13], buf_ptr[22]), FIR_Coefs[13]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[14], buf_ptr[21]), FIR_Coefs[14]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[15], buf_ptr[20]), FIR_Coefs[15]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[16], buf_ptr[19]), FIR_Coefs[16]);
                res_Q6 = silk_SMLAWB(res_Q6, silk_ADD32(buf_ptr[17], buf_ptr[18]), FIR_Coefs[17]);

                /* Scale down, saturate and store in output array */
                *out++ = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(res_Q6, 6));
            }
            break;
        default: {
            ;
        }
    }
    return out;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Resample with a 2nd order AR filter followed by FIR interpolation */
void SilkDecoder::silk_resampler_private_down_FIR(void*         SS,    /* I/O  Resampler state             */
                                                  int16_t       out[], /* O    Output signal               */
                                                  const int16_t in[],  /* I    Input signal                */
                                                  int32_t       inLen  /* I    Number of input samples     */
) {
    silk_resampler_state_struct_t* S = (silk_resampler_state_struct_t*)SS;
    int32_t                        nSamplesIn;
    int32_t                        max_index_Q16, index_increment_Q16;
    const int16_t*                 FIR_Coefs;

    ps_ptr<int32_t> buf;
    buf.alloc_array(S->batchSize + S->FIR_Order);

    /* Copy buffered samples to start of buffer */
    memcpy(buf.get(), S->sFIR.i32, S->FIR_Order * sizeof(int32_t));

    FIR_Coefs = &S->Coefs[2];

    /* Iterate over blocks of frameSizeIn input samples */
    index_increment_Q16 = S->invRatio_Q16;
    while (1) {
        nSamplesIn = silk_min(inLen, S->batchSize);

        /* Second-order AR filter (output in Q8) */
        silk_resampler_private_AR2(S->sIIR, &buf[S->FIR_Order], in, S->Coefs, nSamplesIn);

        max_index_Q16 = silk_LSHIFT32(nSamplesIn, 16);

        /* Interpolate filtered signal */
        out = silk_resampler_private_down_FIR_INTERPOL(out, buf.get(), FIR_Coefs, S->FIR_Order, S->FIR_Fracs, max_index_Q16, index_increment_Q16);

        in += nSamplesIn;
        inLen -= nSamplesIn;

        if (inLen > 1) {
            /* More iterations to do; copy last part of filtered signal to beginning of buffer */
            memcpy(buf.get(), &buf[nSamplesIn], S->FIR_Order * sizeof(int32_t));
        } else {
            break;
        }
    }

    /* Copy last part of filtered signal to the state for the next call */
    memcpy(S->sFIR.i32, &buf[nSamplesIn], S->FIR_Order * sizeof(int32_t));
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int16_t* SilkDecoder::silk_resampler_private_IIR_FIR_INTERPOL(int16_t* out, int16_t* buf, int32_t max_index_Q16, int32_t index_increment_Q16) {
    int32_t  index_Q16, res_Q15;
    int16_t* buf_ptr;
    int32_t  table_index;

    /* Interpolate upsampled signal and store in output array */
    for (index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16) {
        table_index = silk_SMULWB(index_Q16 & 0xFFFF, 12);
        buf_ptr = &buf[index_Q16 >> 16];

        res_Q15 = silk_SMULBB(buf_ptr[0], silk_resampler_frac_FIR_12[table_index][0]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[1], silk_resampler_frac_FIR_12[table_index][1]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[2], silk_resampler_frac_FIR_12[table_index][2]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[3], silk_resampler_frac_FIR_12[table_index][3]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[4], silk_resampler_frac_FIR_12[11 - table_index][3]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[5], silk_resampler_frac_FIR_12[11 - table_index][2]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[6], silk_resampler_frac_FIR_12[11 - table_index][1]);
        res_Q15 = silk_SMLABB(res_Q15, buf_ptr[7], silk_resampler_frac_FIR_12[11 - table_index][0]);
        *out++ = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(res_Q15, 15));
    }
    return out;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Upsample using a combination of allpass-based 2x upsampling and FIR interpolation */
void SilkDecoder::silk_resampler_private_IIR_FIR(void*         SS,    /* I/O  Resampler state             */
                                                 int16_t       out[], /* O    Output signal               */
                                                 const int16_t in[],  /* I    Input signal                */
                                                 int32_t       inLen  /* I    Number of input samples     */
) {
    silk_resampler_state_struct_t* S = (silk_resampler_state_struct_t*)SS;
    int32_t                        nSamplesIn;
    int32_t                        max_index_Q16, index_increment_Q16;

    ps_ptr<int16_t> buf;
    int32_t         buf_size = 2 * S->batchSize + RESAMPLER_ORDER_FIR_12;
    if (!buf.alloc_array(buf_size)) {
        return; // Allocation failed
    }

    /* Copy buffered samples to start of buffer */
    memcpy(buf.get(), S->sFIR.i16, RESAMPLER_ORDER_FIR_12 * sizeof(int16_t));

    /* Iterate over blocks of frameSizeIn input samples */
    index_increment_Q16 = S->invRatio_Q16;
    while (1) {
        nSamplesIn = silk_min(inLen, S->batchSize);

        /* Upsample 2x */
        silk_resampler_private_up2_HQ(S->sIIR, buf.get() + RESAMPLER_ORDER_FIR_12, in, nSamplesIn);

        max_index_Q16 = silk_LSHIFT32(nSamplesIn, 16 + 1); /* + 1 because 2x upsampling */
        out = silk_resampler_private_IIR_FIR_INTERPOL(out, buf.get(), max_index_Q16, index_increment_Q16);
        in += nSamplesIn;
        inLen -= nSamplesIn;

        if (inLen > 0) {
            /* More iterations to do; copy last part of filtered signal to beginning of buffer */
            int32_t src_offset = nSamplesIn << 1;
            if (src_offset + RESAMPLER_ORDER_FIR_12 <= buf_size) { memcpy(buf.get(), buf.get() + src_offset, RESAMPLER_ORDER_FIR_12 * sizeof(int16_t)); }
        } else {
            break;
        }
    }

    /* Copy last part of filtered signal to the state for the next call */
    int32_t src_offset = nSamplesIn << 1;
    if (src_offset + RESAMPLER_ORDER_FIR_12 <= buf_size) { memcpy(S->sFIR.i16, buf.get() + src_offset, RESAMPLER_ORDER_FIR_12 * sizeof(int16_t)); }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Upsample by a factor 2, high quality. Uses 2nd order allpass filters for the 2x upsampling, followed by a      */
/* notch filter just above Nyquist.                                         */
void SilkDecoder::silk_resampler_private_up2_HQ(int32_t*       S,   /* I/O  Resampler state [ 6 ]       */
                                                int16_t*       out, /* O    Output signal [ 2 * len ]   */
                                                const int16_t* in,  /* I    Input signal [ len ]        */
                                                int32_t        len  /* I    Number of input samples     */
) {
    int32_t k;
    int32_t in32, out32_1, out32_2, Y, X;

    assert(silk_resampler_up2_hq_0[0] > 0);
    assert(silk_resampler_up2_hq_0[1] > 0);
    assert(silk_resampler_up2_hq_0[2] < 0);
    assert(silk_resampler_up2_hq_1[0] > 0);
    assert(silk_resampler_up2_hq_1[1] > 0);
    assert(silk_resampler_up2_hq_1[2] < 0);

    /* Internal variables and state are in Q10 format */
    for (k = 0; k < len; k++) {
        /* Convert to Q10 */
        in32 = silk_LSHIFT((int32_t)in[k], 10);

        /* First all-pass section for even output sample */
        Y = silk_SUB32(in32, S[0]);
        X = silk_SMULWB(Y, silk_resampler_up2_hq_0[0]);
        out32_1 = silk_ADD32(S[0], X);
        S[0] = silk_ADD32(in32, X);

        /* Second all-pass section for even output sample */
        Y = silk_SUB32(out32_1, S[1]);
        X = silk_SMULWB(Y, silk_resampler_up2_hq_0[1]);
        out32_2 = silk_ADD32(S[1], X);
        S[1] = silk_ADD32(out32_1, X);

        /* Third all-pass section for even output sample */
        Y = silk_SUB32(out32_2, S[2]);
        X = silk_SMLAWB(Y, Y, silk_resampler_up2_hq_0[2]);
        out32_1 = silk_ADD32(S[2], X);
        S[2] = silk_ADD32(out32_2, X);

        /* Apply gain in Q15, convert back to int16 and store to output */
        out[2 * k] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(out32_1, 10));

        /* First all-pass section for odd output sample */
        Y = silk_SUB32(in32, S[3]);
        X = silk_SMULWB(Y, silk_resampler_up2_hq_1[0]);
        out32_1 = silk_ADD32(S[3], X);
        S[3] = silk_ADD32(in32, X);

        /* Second all-pass section for odd output sample */
        Y = silk_SUB32(out32_1, S[4]);
        X = silk_SMULWB(Y, silk_resampler_up2_hq_1[1]);
        out32_2 = silk_ADD32(S[4], X);
        S[4] = silk_ADD32(out32_1, X);

        /* Third all-pass section for odd output sample */
        Y = silk_SUB32(out32_2, S[5]);
        X = silk_SMLAWB(Y, Y, silk_resampler_up2_hq_1[2]);
        out32_1 = silk_ADD32(S[5], X);
        S[5] = silk_ADD32(out32_2, X);

        /* Apply gain in Q15, convert back to int16 and store to output */
        out[2 * k + 1] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(out32_1, 10));
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SilkDecoder::silk_resampler_private_up2_HQ_wrapper(void*          SS,  /* I/O  Resampler state (unused)    */
                                                        int16_t*       out, /* O    Output signal [ 2 * len ]   */
                                                        const int16_t* in,  /* I    Input signal [ len ]        */
                                                        int32_t        len  /* I    Number of input samples     */
) {
    silk_resampler_state_struct_t* S = (silk_resampler_state_struct_t*)SS;
    silk_resampler_private_up2_HQ(S->sIIR, out, in, len);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Initialize/reset the resampler state for a given pair of input/output sampling rates */
int32_t SilkDecoder::silk_resampler_init(uint8_t n, int32_t Fs_Hz_in, /* I    Input sampling rate (Hz)                                    */
                                         int32_t Fs_Hz_out,           /* I    Output sampling rate (Hz)                                   */
                                         int32_t forEnc               /* I    If 1: encoder; if 0: decoder                                */
) {
    int32_t up2x;
    /* Clear state */
    memset(&m_resampler_state[n], 0, sizeof(silk_resampler_state_struct_t));

    /* Input checking */
    if (forEnc) {
        if ((Fs_Hz_in != 8000 && Fs_Hz_in != 12000 && Fs_Hz_in != 16000 && Fs_Hz_in != 24000 && Fs_Hz_in != 48000) || (Fs_Hz_out != 8000 && Fs_Hz_out != 12000 && Fs_Hz_out != 16000)) { return -1; }
        m_resampler_state[n].inputDelay = delay_matrix_enc[rateID(Fs_Hz_in)][rateID(Fs_Hz_out)];
    } else {
        if ((Fs_Hz_in != 8000 && Fs_Hz_in != 12000 && Fs_Hz_in != 16000) || (Fs_Hz_out != 8000 && Fs_Hz_out != 12000 && Fs_Hz_out != 16000 && Fs_Hz_out != 24000 && Fs_Hz_out != 48000)) { return -1; }
        m_resampler_state[n].inputDelay = delay_matrix_dec[rateID(Fs_Hz_in)][rateID(Fs_Hz_out)];
    }

    m_resampler_state[n].Fs_in_kHz = silk_DIV32_16(Fs_Hz_in, 1000);
    m_resampler_state[n].Fs_out_kHz = silk_DIV32_16(Fs_Hz_out, 1000);

    /* Number of samples processed per batch */
    m_resampler_state[n].batchSize = m_resampler_state[n].Fs_in_kHz * RESAMPLER_MAX_BATCH_SIZE_MS;

    /* Find resampler with the right sampling ratio */
    up2x = 0;
    if (Fs_Hz_out > Fs_Hz_in) {
        /* Upsample */
        if (Fs_Hz_out == silk_MUL(Fs_Hz_in, 2)) { /* Fs_out : Fs_in = 2 : 1 */
            /* Special case: directly use 2x upsampler */
            m_resampler_state[n].resampler_function = USE_silk_resampler_private_up2_HQ_wrapper;
        } else {
            /* Default resampler */
            m_resampler_state[n].resampler_function = USE_silk_resampler_private_IIR_FIR;
            up2x = 1;
        }
    } else if (Fs_Hz_out < Fs_Hz_in) {
        /* Downsample */
        m_resampler_state[n].resampler_function = USE_silk_resampler_private_down_FIR;
        if (silk_MUL(Fs_Hz_out, 4) == silk_MUL(Fs_Hz_in, 3)) { /* Fs_out : Fs_in = 3 : 4 */
            m_resampler_state[n].FIR_Fracs = 3;
            m_resampler_state[n].FIR_Order = RESAMPLER_DOWN_ORDER_FIR0;
            m_resampler_state[n].Coefs = silk_Resampler_3_4_COEFS;
        } else if (silk_MUL(Fs_Hz_out, 3) == silk_MUL(Fs_Hz_in, 2)) { /* Fs_out : Fs_in = 2 : 3 */
            m_resampler_state[n].FIR_Fracs = 2;
            m_resampler_state[n].FIR_Order = RESAMPLER_DOWN_ORDER_FIR0;
            m_resampler_state[n].Coefs = silk_Resampler_2_3_COEFS;
        } else if (silk_MUL(Fs_Hz_out, 2) == Fs_Hz_in) { /* Fs_out : Fs_in = 1 : 2 */
            m_resampler_state[n].FIR_Fracs = 1;
            m_resampler_state[n].FIR_Order = RESAMPLER_DOWN_ORDER_FIR1;
            m_resampler_state[n].Coefs = silk_Resampler_1_2_COEFS;
        } else if (silk_MUL(Fs_Hz_out, 3) == Fs_Hz_in) { /* Fs_out : Fs_in = 1 : 3 */
            m_resampler_state[n].FIR_Fracs = 1;
            m_resampler_state[n].FIR_Order = RESAMPLER_DOWN_ORDER_FIR2;
            m_resampler_state[n].Coefs = silk_Resampler_1_3_COEFS;
        } else if (silk_MUL(Fs_Hz_out, 4) == Fs_Hz_in) { /* Fs_out : Fs_in = 1 : 4 */
            m_resampler_state[n].FIR_Fracs = 1;
            m_resampler_state[n].FIR_Order = RESAMPLER_DOWN_ORDER_FIR2;
            m_resampler_state[n].Coefs = silk_Resampler_1_4_COEFS;
        } else if (silk_MUL(Fs_Hz_out, 6) == Fs_Hz_in) { /* Fs_out : Fs_in = 1 : 6 */
            m_resampler_state[n].FIR_Fracs = 1;
            m_resampler_state[n].FIR_Order = RESAMPLER_DOWN_ORDER_FIR2;
            m_resampler_state[n].Coefs = silk_Resampler_1_6_COEFS;
        } else {
            /* None available */
            return -1;
        }
    } else {
        /* Input and output sampling rates are equal: copy */
        m_resampler_state[n].resampler_function = USE_silk_resampler_copy;
    }

    /* Ratio of input/output samples */
    m_resampler_state[n].invRatio_Q16 = silk_LSHIFT32(silk_DIV32(silk_LSHIFT32(Fs_Hz_in, 14 + up2x), Fs_Hz_out), 2);
    /* Make sure the ratio is rounded up */
    while (silk_SMULWW(m_resampler_state[n].invRatio_Q16, Fs_Hz_out) < silk_LSHIFT32(Fs_Hz_in, up2x)) { m_resampler_state[n].invRatio_Q16++; }

    return 0;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Resampler: convert from one sampling rate to another Input and output sampling rate are at most 48000 Hz  */
int32_t SilkDecoder::silk_resampler(uint8_t n, int16_t out[], /* O    Output signal                                               */
                                    const int16_t in[],       /* I    Input signal                                                */
                                    int32_t       inLen       /* I    Number of input samples                                     */
) {
    int32_t nSamples;

    /* Need at least 1 ms of input data */
    assert(inLen >= m_resampler_state[n].Fs_in_kHz);
    /* Delay can't exceed the 1 ms of buffering */
    assert(m_resampler_state[n].inputDelay <= m_resampler_state[n].Fs_in_kHz);

    nSamples = m_resampler_state[n].Fs_in_kHz - m_resampler_state[n].inputDelay;

    /* Copy to delay buffer */
    memcpy(&m_resampler_state[n].delayBuf[m_resampler_state[n].inputDelay], in, nSamples * sizeof(int16_t));

    switch (m_resampler_state[n].resampler_function) {
        case USE_silk_resampler_private_up2_HQ_wrapper:
            silk_resampler_private_up2_HQ_wrapper(&m_resampler_state[n], out, m_resampler_state[n].delayBuf, m_resampler_state[n].Fs_in_kHz);
            silk_resampler_private_up2_HQ_wrapper(&m_resampler_state[n], &out[m_resampler_state[n].Fs_out_kHz], &in[nSamples], inLen - m_resampler_state[n].Fs_in_kHz);
            break;
        case USE_silk_resampler_private_IIR_FIR:
            silk_resampler_private_IIR_FIR(&m_resampler_state[n], out, m_resampler_state[n].delayBuf, m_resampler_state[n].Fs_in_kHz);
            silk_resampler_private_IIR_FIR(&m_resampler_state[n], &out[m_resampler_state[n].Fs_out_kHz], &in[nSamples], inLen - m_resampler_state[n].Fs_in_kHz);
            break;
        case USE_silk_resampler_private_down_FIR:
            silk_resampler_private_down_FIR(&m_resampler_state[n], out, m_resampler_state[n].delayBuf, m_resampler_state[n].Fs_in_kHz);
            silk_resampler_private_down_FIR(&m_resampler_state[n], &out[m_resampler_state[n].Fs_out_kHz], &in[nSamples], inLen - m_resampler_state[n].Fs_in_kHz);
            break;
        default:
            memcpy(out, m_resampler_state[n].delayBuf, m_resampler_state[n].Fs_in_kHz * sizeof(int16_t));
            memcpy(&out[m_resampler_state[n].Fs_out_kHz], &in[nSamples], (inLen - m_resampler_state[n].Fs_in_kHz) * sizeof(int16_t));
    }

    /* Copy to delay buffer */
    memcpy(m_resampler_state[n].delayBuf, &in[inLen - m_resampler_state[n].inputDelay], m_resampler_state[n].inputDelay * sizeof(int16_t));

    return 0;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t SilkDecoder::silk_sigm_Q15(int32_t in_Q5) {
    int32_t ind;
    if (in_Q5 < 0) {
        /* Negative input */
        in_Q5 = -in_Q5;
        if (in_Q5 >= 6 * 32) {
            return 0; /* Clip */
        } else {
            /* Linear interpolation of look up table */
            ind = silk_RSHIFT(in_Q5, 5);
            return (sigm_LUT_neg_Q15[ind] - silk_SMULBB(sigm_LUT_slope_Q10[ind], in_Q5 & 0x1F));
        }
    } else {
        /* Positive input */
        if (in_Q5 >= 6 * 32) {
            return 32767; /* clip */
        } else {
            /* Linear interpolation of look up table */
            ind = silk_RSHIFT(in_Q5, 5);
            return (sigm_LUT_pos_Q15[ind] + silk_SMULBB(sigm_LUT_slope_Q10[ind], in_Q5 & 0x1F));
        }
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//  silk_insertion_sort_increasing(Unsorted / Sorted vector, Index vector for the sorted elements, Vector length, Number of correctly sorted positions )
void SilkDecoder::silk_insertion_sort_increasing(int32_t* a, int32_t* idx, const int32_t L, const int32_t K) {
    int32_t value;
    int32_t i, j;

    /* Safety checks */
    assert(K > 0);
    assert(L > 0);
    assert(L >= K);

    /* Write start indices in index vector */
    for (i = 0; i < K; i++) { idx[i] = i; }

    /* Sort vector elements by value, increasing order */
    for (i = 1; i < K; i++) {
        value = a[i];
        for (j = i - 1; (j >= 0) && (value < a[j]); j--) {
            a[j + 1] = a[j];     /* Shift value */
            idx[j + 1] = idx[j]; /* Shift index */
        }
        a[j + 1] = value; /* Write value */
        idx[j + 1] = i;   /* Write index */
    }

    /* If less than L values are asked for, check the remaining values, but only spend CPU to ensure that the K first values are correct */
    for (i = K; i < L; i++) {
        value = a[i];
        if (value < a[K - 1]) {
            for (j = K - 2; (j >= 0) && (value < a[j]); j--) {
                a[j + 1] = a[j];     /* Shift value */
                idx[j + 1] = idx[j]; /* Shift index */
            }
            a[j + 1] = value; /* Write value */
            idx[j + 1] = i;   /* Write index */
        }
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This function is only used by the fixed-point build */
void SilkDecoder::silk_insertion_sort_decreasing_int16(int16_t*      a,   /* I/O   Unsorted / Sorted vector      */
                                                       int32_t*      idx, /* O     Index vector for the sorted elements    */
                                                       const int32_t L,   /* I     Vector length */
                                                       const int32_t K    /* I     Number of correctly sorted positions  */
) {
    int32_t i, j;
    int32_t value;

    /* Safety checks */
    assert(K > 0);
    assert(L > 0);
    assert(L >= K);

    /* Write start indices in index vector */
    for (i = 0; i < K; i++) { idx[i] = i; }

    /* Sort vector elements by value, decreasing order */
    for (i = 1; i < K; i++) {
        value = a[i];
        for (j = i - 1; (j >= 0) && (value > a[j]); j--) {
            a[j + 1] = a[j];     /* Shift value */
            idx[j + 1] = idx[j]; /* Shift index */
        }
        a[j + 1] = value; /* Write value */
        idx[j + 1] = i;   /* Write index */
    }

    /* If less than L values are asked for, check the remaining values, */
    /* but only spend CPU to ensure that the K first values are correct */
    for (i = K; i < L; i++) {
        value = a[i];
        if (value > a[K - 1]) {
            for (j = K - 2; (j >= 0) && (value > a[j]); j--) {
                a[j + 1] = a[j];     /* Shift value */
                idx[j + 1] = idx[j]; /* Shift index */
            }
            a[j + 1] = value; /* Write value */
            idx[j + 1] = i;   /* Write index */
        }
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SilkDecoder::silk_insertion_sort_increasing_all_values_int16(int16_t*      a, /* I/O   Unsorted / Sorted vector     */
                                                                  const int32_t L  /* I     Vector length */
) {
    int32_t value;
    int32_t i, j;

    /* Safety checks */
    assert(L > 0);

    /* Sort vector elements by value, increasing order */
    for (i = 1; i < L; i++) {
        value = a[i];
        for (j = i - 1; (j >= 0) && (value < a[j]); j--) { a[j + 1] = a[j]; /* Shift value */ }
        a[j + 1] = value; /* Write value */
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Compute number of bits to right shift the sum of squares of a vector */
/* of int16s to make it fit in an int32                                 */
void SilkDecoder::silk_sum_sqr_shift(int32_t*       energy, /* O   Energy of x, after shifting to the right                     */
                                     int32_t*       shift,  /* O   Number of bits right shift applied to energy                 */
                                     const int16_t* x,      /* I   Input vector                                                 */
                                     int32_t        len     /* I   Length of input vector                                       */
) {
    int32_t  i, shft;
    uint32_t nrg_tmp;
    int32_t  nrg;

    /* Do a first run with the maximum shift we could have. */
    shft = 31 - silk_CLZ32(len);
    /* Let's be conservative with rounding and start with nrg=len. */
    nrg = len;
    for (i = 0; i < len - 1; i += 2) {
        nrg_tmp = silk_SMULBB(x[i], x[i]);
        nrg_tmp = silk_SMLABB_ovflw(nrg_tmp, x[i + 1], x[i + 1]);
        nrg = (int32_t)silk_ADD_RSHIFT_uint(nrg, nrg_tmp, shft);
    }
    if (i < len) {
        /* One sample left to process */
        nrg_tmp = silk_SMULBB(x[i], x[i]);
        nrg = (int32_t)silk_ADD_RSHIFT_uint(nrg, nrg_tmp, shft);
    }
    assert(nrg >= 0);
    /* Make sure the result will fit in a 32-bit signed integer with two bits
       of headroom. */
    shft = silk_max_32(0, shft + 3 - silk_CLZ32(nrg));
    nrg = 0;
    for (i = 0; i < len - 1; i += 2) {
        nrg_tmp = silk_SMULBB(x[i], x[i]);
        nrg_tmp = silk_SMLABB_ovflw(nrg_tmp, x[i + 1], x[i + 1]);
        nrg = (int32_t)silk_ADD_RSHIFT_uint(nrg, nrg_tmp, shft);
    }
    if (i < len) {
        /* One sample left to process */
        nrg_tmp = silk_SMULBB(x[i], x[i]);
        nrg = (int32_t)silk_ADD_RSHIFT_uint(nrg, nrg_tmp, shft);
    }

    assert(nrg >= 0);

    /* Output arguments */
    *shift = shft;
    *energy = nrg;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Entropy constrained matrix-weighted VQ, hard-coded to 5-element vectors, for a single input data vector */
void SilkDecoder::silk_VQ_WMat_EC_c(int8_t*        ind,          /* O    index of best codebook vector               */
                                    int32_t*       res_nrg_Q15,  /* O    best residual energy                        */
                                    int32_t*       rate_dist_Q8, /* O    best total bitrate                          */
                                    int32_t*       gain_Q7,      /* O    sum of absolute LTP coefficients            */
                                    const int32_t* XX_Q17,       /* I    correlation matrix                          */
                                    const int32_t* xX_Q17,       /* I    correlation vector                          */
                                    const int8_t*  cb_Q7,        /* I    codebook                                    */
                                    const uint8_t* cb_gain_Q7,   /* I    codebook effective gain                     */
                                    const uint8_t* cl_Q5,        /* I    code length for each codebook vector        */
                                    const int32_t  subfr_len,    /* I    number of samples per subframe              */
                                    const int32_t  max_gain_Q7,  /* I    maximum sum of absolute LTP coefficients    */
                                    const int32_t  L             /* I    number of vectors in codebook               */
) {
    int32_t       k, gain_tmp_Q7;
    const int8_t* cb_row_Q7;
    int32_t       neg_xX_Q24[5];
    int32_t       sum1_Q15, sum2_Q24;
    int32_t       bits_res_Q8, bits_tot_Q8;

    /* Negate and convert to new Q domain */
    neg_xX_Q24[0] = -silk_LSHIFT32(xX_Q17[0], 7);
    neg_xX_Q24[1] = -silk_LSHIFT32(xX_Q17[1], 7);
    neg_xX_Q24[2] = -silk_LSHIFT32(xX_Q17[2], 7);
    neg_xX_Q24[3] = -silk_LSHIFT32(xX_Q17[3], 7);
    neg_xX_Q24[4] = -silk_LSHIFT32(xX_Q17[4], 7);

    /* Loop over codebook */
    *rate_dist_Q8 = silk_int32_MAX;
    *res_nrg_Q15 = silk_int32_MAX;
    cb_row_Q7 = cb_Q7;
    /* In things go really bad, at least *ind is set to something safe. */
    *ind = 0;
    for (k = 0; k < L; k++) {
        int32_t penalty;
        gain_tmp_Q7 = cb_gain_Q7[k];
        /* Weighted rate */
        /* Quantization error: 1 - 2 * xX * cb + cb' * XX * cb */
        sum1_Q15 = SILK_FIX_CONST(1.001, 15);

        /* Penalty for too large gain */
        penalty = silk_LSHIFT32(silk_max(silk_SUB32(gain_tmp_Q7, max_gain_Q7), 0), 11);

        /* first row of XX_Q17 */
        sum2_Q24 = silk_MLA(neg_xX_Q24[0], XX_Q17[1], cb_row_Q7[1]);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[2], cb_row_Q7[2]);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[3], cb_row_Q7[3]);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[4], cb_row_Q7[4]);
        sum2_Q24 = silk_LSHIFT32(sum2_Q24, 1);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[0], cb_row_Q7[0]);
        sum1_Q15 = silk_SMLAWB(sum1_Q15, sum2_Q24, cb_row_Q7[0]);

        /* second row of XX_Q17 */
        sum2_Q24 = silk_MLA(neg_xX_Q24[1], XX_Q17[7], cb_row_Q7[2]);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[8], cb_row_Q7[3]);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[9], cb_row_Q7[4]);
        sum2_Q24 = silk_LSHIFT32(sum2_Q24, 1);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[6], cb_row_Q7[1]);
        sum1_Q15 = silk_SMLAWB(sum1_Q15, sum2_Q24, cb_row_Q7[1]);

        /* third row of XX_Q17 */
        sum2_Q24 = silk_MLA(neg_xX_Q24[2], XX_Q17[13], cb_row_Q7[3]);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[14], cb_row_Q7[4]);
        sum2_Q24 = silk_LSHIFT32(sum2_Q24, 1);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[12], cb_row_Q7[2]);
        sum1_Q15 = silk_SMLAWB(sum1_Q15, sum2_Q24, cb_row_Q7[2]);

        /* fourth row of XX_Q17 */
        sum2_Q24 = silk_MLA(neg_xX_Q24[3], XX_Q17[19], cb_row_Q7[4]);
        sum2_Q24 = silk_LSHIFT32(sum2_Q24, 1);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[18], cb_row_Q7[3]);
        sum1_Q15 = silk_SMLAWB(sum1_Q15, sum2_Q24, cb_row_Q7[3]);

        /* last row of XX_Q17 */
        sum2_Q24 = silk_LSHIFT32(neg_xX_Q24[4], 1);
        sum2_Q24 = silk_MLA(sum2_Q24, XX_Q17[24], cb_row_Q7[4]);
        sum1_Q15 = silk_SMLAWB(sum1_Q15, sum2_Q24, cb_row_Q7[4]);

        /* find best */
        if (sum1_Q15 >= 0) {
            /* Translate residual energy to bits using high-rate assumption (6 dB ==> 1 bit/sample) */
            bits_res_Q8 = silk_SMULBB(subfr_len, silk_lin2log(sum1_Q15 + penalty) - (15 << 7));
            /* In the following line we reduce the codelength component by half ("-1"); seems to slghtly improve quality
             */
            bits_tot_Q8 = silk_ADD_LSHIFT32(bits_res_Q8, cl_Q5[k], 3 - 1);
            if (bits_tot_Q8 <= *rate_dist_Q8) {
                *rate_dist_Q8 = bits_tot_Q8;
                *res_nrg_Q15 = sum1_Q15 + penalty;
                *ind = (int8_t)k;
                *gain_Q7 = gain_tmp_Q7;
            }
        }

        /* Go to next cbk vector */
        cb_row_Q7 += LTP_ORDER;
    }
}
//-----------------------------------------------------------------------------------------------------------
/* Find least-squares prediction gain for one signal based on another and quantize it */
int32_t SilkDecoder::silk_stereo_find_predictor(                                /* O    Returns predictor in Q13                    */
                                                int32_t*      ratio_Q14,        /* O    Ratio of residual and mid energies          */
                                                const int16_t x[],              /* I    Basis signal                                */
                                                const int16_t y[],              /* I    Target signal                               */
                                                int32_t       mid_res_amp_Q0[], /* I/O  Smoothed mid, residual norms                */
                                                int32_t       length,           /* I    Number of samples                           */
                                                int32_t       smooth_coef_Q16   /* I    Smoothing coefficient                       */
) {
    int32_t scale, scale1, scale2;
    int32_t nrgx, nrgy, corr, pred_Q13, pred2_Q10;

    /* Find  predictor */
    silk_sum_sqr_shift(&nrgx, &scale1, x, length);
    silk_sum_sqr_shift(&nrgy, &scale2, y, length);
    scale = silk_max_int(scale1, scale2);
    scale = scale + (scale & 1); /* make even */
    nrgy = silk_RSHIFT32(nrgy, scale - scale2);
    nrgx = silk_RSHIFT32(nrgx, scale - scale1);
    nrgx = silk_max_int(nrgx, 1);
    corr = silk_inner_prod_aligned_scale(x, y, scale, length);
    pred_Q13 = silk_DIV32_varQ(corr, nrgx, 13);
    pred_Q13 = silk_LIMIT(pred_Q13, -(1 << 14), 1 << 14);
    pred2_Q10 = silk_SMULWB(pred_Q13, pred_Q13);

    /* Faster update for signals with large prediction parameters */
    smooth_coef_Q16 = (int32_t)silk_max_int(smooth_coef_Q16, silk_abs(pred2_Q10));

    /* Smoothed mid and residual norms */
    assert(smooth_coef_Q16 < 32768);
    scale = silk_RSHIFT(scale, 1);
    mid_res_amp_Q0[0] = silk_SMLAWB(mid_res_amp_Q0[0], silk_LSHIFT(silk_SQRT_APPROX(nrgx), scale) - mid_res_amp_Q0[0], smooth_coef_Q16);
    /* Residual energy = nrgy - 2 * pred * corr + pred^2 * nrgx */
    nrgy = silk_SUB_LSHIFT32(nrgy, silk_SMULWB(corr, pred_Q13), 3 + 1);
    nrgy = silk_ADD_LSHIFT32(nrgy, silk_SMULWB(nrgx, pred2_Q10), 6);
    mid_res_amp_Q0[1] = silk_SMLAWB(mid_res_amp_Q0[1], silk_LSHIFT(silk_SQRT_APPROX(nrgy), scale) - mid_res_amp_Q0[1], smooth_coef_Q16);

    /* Ratio of smoothed residual and mid norms */
    *ratio_Q14 = silk_DIV32_varQ(mid_res_amp_Q0[1], silk_max(mid_res_amp_Q0[0], 1), 14);
    *ratio_Q14 = silk_LIMIT(*ratio_Q14, 0, 32767);

    return pred_Q13;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Convert adaptive Mid/Side representation to Left/Right stereo signal */
void SilkDecoder::silk_stereo_MS_to_LR(stereo_dec_state_t* state,       /* I/O  State                                       */
                                       int16_t             x1[],        /* I/O  Left input signal, becomes mid signal       */
                                       int16_t             x2[],        /* I/O  Right input signal, becomes side signal     */
                                       const int32_t       pred_Q13[],  /* I    Predictors                                  */
                                       int32_t             fs_kHz,      /* I    Samples rate (kHz)                          */
                                       int32_t             frame_length /* I    Number of samples                           */
) {
    int32_t n, denom_Q16, delta0_Q13, delta1_Q13;
    int32_t sum, diff, pred0_Q13, pred1_Q13;

    /* Buffering */
    memcpy(x1, state->sMid, 2 * sizeof(int16_t));
    memcpy(x2, state->sSide, 2 * sizeof(int16_t));
    memcpy(state->sMid, &x1[frame_length], 2 * sizeof(int16_t));
    memcpy(state->sSide, &x2[frame_length], 2 * sizeof(int16_t));

    /* Interpolate predictors and add prediction to side channel */
    pred0_Q13 = state->pred_prev_Q13[0];
    pred1_Q13 = state->pred_prev_Q13[1];
    denom_Q16 = silk_DIV32_16((int32_t)1 << 16, STEREO_INTERP_LEN_MS * fs_kHz);
    delta0_Q13 = silk_RSHIFT_ROUND(silk_SMULBB(pred_Q13[0] - state->pred_prev_Q13[0], denom_Q16), 16);
    delta1_Q13 = silk_RSHIFT_ROUND(silk_SMULBB(pred_Q13[1] - state->pred_prev_Q13[1], denom_Q16), 16);
    for (n = 0; n < STEREO_INTERP_LEN_MS * fs_kHz; n++) {
        pred0_Q13 += delta0_Q13;
        pred1_Q13 += delta1_Q13;
        sum = silk_LSHIFT(silk_ADD_LSHIFT(x1[n] + x1[n + 2], x1[n + 1], 1), 9); /* Q11 */
        sum = silk_SMLAWB(silk_LSHIFT((int32_t)x2[n + 1], 8), sum, pred0_Q13);  /* Q8  */
        sum = silk_SMLAWB(sum, silk_LSHIFT((int32_t)x1[n + 1], 11), pred1_Q13); /* Q8  */
        x2[n + 1] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(sum, 8));
    }
    pred0_Q13 = pred_Q13[0];
    pred1_Q13 = pred_Q13[1];
    for (n = STEREO_INTERP_LEN_MS * fs_kHz; n < frame_length; n++) {
        sum = silk_LSHIFT(silk_ADD_LSHIFT(x1[n] + x1[n + 2], x1[n + 1], 1), 9); /* Q11 */
        sum = silk_SMLAWB(silk_LSHIFT((int32_t)x2[n + 1], 8), sum, pred0_Q13);  /* Q8  */
        sum = silk_SMLAWB(sum, silk_LSHIFT((int32_t)x1[n + 1], 11), pred1_Q13); /* Q8  */
        x2[n + 1] = (int16_t)silk_SAT16(silk_RSHIFT_ROUND(sum, 8));
    }
    state->pred_prev_Q13[0] = pred_Q13[0];
    state->pred_prev_Q13[1] = pred_Q13[1];

    /* Convert to left/right signals */
    for (n = 0; n < frame_length; n++) {
        sum = x1[n + 1] + (int32_t)x2[n + 1];
        diff = x1[n + 1] - (int32_t)x2[n + 1];
        x1[n + 1] = (int16_t)silk_SAT16(sum);
        x2[n + 1] = (int16_t)silk_SAT16(diff);
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Initialization of the Silk VAD */
int32_t SilkDecoder::silk_VAD_Init(                             /* O    Return value, 0 if success                  */
                                   silk_VAD_state_t* psSilk_VAD /* I/O  Pointer to Silk VAD state                   */
) {
    int32_t b, ret = 0;

    /* reset state memory */
    memset(psSilk_VAD, 0, sizeof(silk_VAD_state_t));

    /* init noise levels */
    /* Initialize array with approx pink noise levels (psd proportional to inverse of frequency) */
    for (b = 0; b < VAD_N_BANDS; b++) { psSilk_VAD->NoiseLevelBias[b] = silk_max_32(silk_DIV32_16(VAD_NOISE_LEVELS_BIAS, b + 1), 1); }

    /* Initialize state */
    for (b = 0; b < VAD_N_BANDS; b++) {
        psSilk_VAD->NL[b] = silk_MUL(100, psSilk_VAD->NoiseLevelBias[b]);
        psSilk_VAD->inv_NL[b] = silk_DIV32(silk_int32_MAX, psSilk_VAD->NL[b]);
    }
    psSilk_VAD->counter = 15;

    /* init smoothed energy-to-noise ratio*/
    for (b = 0; b < VAD_N_BANDS; b++) { psSilk_VAD->NrgRatioSmth_Q8[b] = 100 * 256; /* 100 * 256 --> 20 dB SNR */ }

    return (ret);
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::setChannelsAPI(uint8_t nChannelsAPI) {
    m_silk_DecControlStruct->nChannelsAPI = nChannelsAPI;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::setChannelsInternal(uint8_t nChannelsInternal) {
    m_silk_DecControlStruct->nChannelsInternal = nChannelsInternal;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::setAPIsampleRate(uint32_t API_sampleRate) {
    m_silk_DecControlStruct->API_sampleRate = API_sampleRate;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void SilkDecoder::combine_pulses(int32_t* out, const int32_t* in, const int32_t len) {
    int32_t k;
    for (k = 0; k < len; k++) { out[k] = in[2 * k] + in[2 * k + 1]; }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Invert int32 value and return result as int32 in a given Q-domain */
int32_t SilkDecoder::silk_INVERSE32_varQ(const int32_t b32, const int32_t Qres) {
    int32_t b_headrm, lshift;
    int32_t b32_inv, b32_nrm, err_Q32, result;

    assert(b32 != 0);
    assert(Qres > 0);

    /* Compute number of bits head room and normalize input */
    b_headrm = silk_CLZ32(silk_abs(b32)) - 1;
    b32_nrm = silk_LSHIFT(b32, b_headrm); /* Q: b_headrm                */

    /* Inverse of b32, with 14 bits of precision */
    b32_inv = silk_DIV32_16(silk_int32_MAX >> 2, silk_RSHIFT(b32_nrm, 16)); /* Q: 29 + 16 - b_headrm    */

    /* First approximation */
    result = silk_LSHIFT(b32_inv, 16); /* Q: 61 - b_headrm            */

    /* Compute residual by subtracting product of denominator and first approximation from one */
    err_Q32 = silk_LSHIFT(((int32_t)1 << 29) - silk_SMULWB(b32_nrm, b32_inv), 3); /* Q32                        */

    /* Refinement */
    result = silk_SMLAWW(result, err_Q32, b32_inv); /* Q: 61 - b_headrm            */

    /* Convert to Qres domain */
    lshift = 61 - b_headrm - Qres;
    if (lshift <= 0) {
        return silk_LSHIFT_SAT32(result, -lshift);
    } else {
        if (lshift < 32) {
            return silk_RSHIFT(result, lshift);
        } else {
            /* Avoid undefined result */
            return 0;
        }
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Divide two int32 values and return result as int32 in a given Q-domain */
int32_t SilkDecoder::silk_DIV32_varQ(const int32_t a32, const int32_t b32, const int32_t Qres) {
    int32_t a_headrm, b_headrm, lshift;
    int32_t b32_inv, a32_nrm, b32_nrm, result;

    assert(b32 != 0);
    assert(Qres >= 0);

    /* Compute number of bits head room and normalize inputs */
    a_headrm = silk_CLZ32(silk_abs(a32)) - 1;
    a32_nrm = silk_LSHIFT(a32, a_headrm); /* Q: a_headrm                  */
    b_headrm = silk_CLZ32(silk_abs(b32)) - 1;
    b32_nrm = silk_LSHIFT(b32, b_headrm); /* Q: b_headrm                  */

    /* Inverse of b32, with 14 bits of precision */
    b32_inv = silk_DIV32_16(silk_int32_MAX >> 2, silk_RSHIFT(b32_nrm, 16)); /* Q: 29 + 16 - b_headrm        */

    /* First approximation */
    result = silk_SMULWB(a32_nrm, b32_inv); /* Q: 29 + a_headrm - b_headrm  */

    /* Compute residual by subtracting product of denominator and first approximation */
    /* It's OK to overflow because the final value of a32_nrm should always be small */
    a32_nrm = silk_SUB32_ovflw(a32_nrm, silk_LSHIFT_ovflw(silk_SMMUL(b32_nrm, result), 3)); /* Q: a_headrm   */

    /* Refinement */
    result = silk_SMLAWB(result, a32_nrm, b32_inv); /* Q: 29 + a_headrm - b_headrm  */

    /* Convert to Qres domain */
    lshift = 29 + a_headrm - b_headrm - Qres;
    if (lshift < 0) {
        return silk_LSHIFT_SAT32(result, -lshift);
    } else {
        if (lshift < 32) {
            return silk_RSHIFT(result, lshift);
        } else {
            /* Avoid undefined result */
            return 0;
        }
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Approximation of square root, Accuracy: < +/- 10%  for output values > 15, < +/- 2.5% for output values > 120 */
int32_t SilkDecoder::silk_SQRT_APPROX(int32_t x) {
    int32_t y, lz, frac_Q7;
    if (x <= 0) { return 0; }
    silk_CLZ_FRAC(x, &lz, &frac_Q7);

    if (lz & 1) {
        y = 32768;
    } else {
        y = 46214; /* 46214 = sqrt(2) * 32768 */
    }

    y >>= silk_RSHIFT(lz, 1);                         /* get scaling right */
    y = silk_SMLAWB(y, y, silk_SMULBB(213, frac_Q7)); /* increment using fractional part of input */
    return y;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* get number of leading zeros and fractional part (the bits right after the leading one */
void SilkDecoder::silk_CLZ_FRAC(int32_t in, int32_t* lz, int32_t* frac_Q7) {
    int32_t lzeros = silk_CLZ32(in);

    *lz = lzeros;
    *frac_Q7 = silk_ROR32(in, 24 - lzeros) & 0x7f;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Rotate a32 right by 'rot' bits. Negative rot values result in rotating left. Output is 32bit int.
   Note: contemporary compilers recognize the C expression below and compile it into a 'ror' instruction if available. No need for inline ASM! */
int32_t SilkDecoder::silk_ROR32(int32_t a32, int32_t rot) {
    uint32_t x = (uint32_t)a32;
    uint32_t r = (uint32_t)rot;
    uint32_t m = (uint32_t)-rot;
    if (rot == 0) {
        return a32;
    } else if (rot < 0) {
        return (int32_t)((x << m) | (x >> (32 - m)));
    } else {
        return (int32_t)((x << (32 - r)) | (x >> r));
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* count leading zeros of int32_t64 */
int32_t SilkDecoder::silk_CLZ64(int64_t in) {
    int32_t in_upper;

    in_upper = (int32_t)silk_RSHIFT64(in, 32);
    if (in_upper == 0) {
        /* Search in the lower 32 bits */
        return 32 + silk_CLZ32((int32_t)in);
    } else {
        /* Search in the upper 32 bits */
        return silk_CLZ32(in_upper);
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* silk_min() versions with typecast in the function call */
int32_t SilkDecoder::silk_min_int(int32_t a, int32_t b) {
    return (((a) < (b)) ? (a) : (b));
}
int16_t SilkDecoder::silk_min_16(int16_t a, int16_t b) {
    return (((a) < (b)) ? (a) : (b));
}
int32_t SilkDecoder::silk_min_32(int32_t a, int32_t b) {
    return (((a) < (b)) ? (a) : (b));
}
int64_t SilkDecoder::silk_min_64(int64_t a, int64_t b) {
    return (((a) < (b)) ? (a) : (b));
}

/* silk_min() versions with typecast in the function call */
int32_t SilkDecoder::silk_max_int(int32_t a, int32_t b) {
    return (((a) > (b)) ? (a) : (b));
}
int16_t SilkDecoder::silk_max_16(int16_t a, int16_t b) {
    return (((a) > (b)) ? (a) : (b));
}
int32_t SilkDecoder::silk_max_32(int32_t a, int32_t b) {
    return (((a) > (b)) ? (a) : (b));
}
int64_t SilkDecoder::silk_max_64(int64_t a, int64_t b) {
    return (((a) > (b)) ? (a) : (b));
}

int32_t SilkDecoder::silk_CLZ16(int16_t in16) {
    return 32 - EC_ILOGs(in16 << 16 | 0x8000);
}
int32_t SilkDecoder::silk_CLZ32(int32_t in32) {
    return in32 ? 32 - EC_ILOGs(in32) : 32;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t SilkDecoder::silk_noise_shape_quantizer_short_prediction_c(const int32_t* buf32, const int16_t* coef16, int32_t order) {
    int32_t out;
    assert(order == 10 || order == 16);

    /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
    out = silk_RSHIFT(order, 1);
    out = silk_SMLAWB(out, buf32[0], coef16[0]);
    out = silk_SMLAWB(out, buf32[-1], coef16[1]);
    out = silk_SMLAWB(out, buf32[-2], coef16[2]);
    out = silk_SMLAWB(out, buf32[-3], coef16[3]);
    out = silk_SMLAWB(out, buf32[-4], coef16[4]);
    out = silk_SMLAWB(out, buf32[-5], coef16[5]);
    out = silk_SMLAWB(out, buf32[-6], coef16[6]);
    out = silk_SMLAWB(out, buf32[-7], coef16[7]);
    out = silk_SMLAWB(out, buf32[-8], coef16[8]);
    out = silk_SMLAWB(out, buf32[-9], coef16[9]);

    if (order == 16) {
        out = silk_SMLAWB(out, buf32[-10], coef16[10]);
        out = silk_SMLAWB(out, buf32[-11], coef16[11]);
        out = silk_SMLAWB(out, buf32[-12], coef16[12]);
        out = silk_SMLAWB(out, buf32[-13], coef16[13]);
        out = silk_SMLAWB(out, buf32[-14], coef16[14]);
        out = silk_SMLAWB(out, buf32[-15], coef16[15]);
    }
    return out;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t SilkDecoder::silk_NSQ_noise_shape_feedback_loop_c(const int32_t* data0, int32_t* data1, const int16_t* coef, int32_t order) {
    int32_t out;
    int32_t tmp1, tmp2;
    int32_t j;

    tmp2 = data0[0];
    tmp1 = data1[0];
    data1[0] = tmp2;

    out = silk_RSHIFT(order, 1);
    out = silk_SMLAWB(out, tmp2, coef[0]);

    for (j = 2; j < order; j += 2) {
        tmp2 = data1[j - 1];
        data1[j - 1] = tmp1;
        out = silk_SMLAWB(out, tmp1, coef[j - 1]);
        tmp1 = data1[j + 0];
        data1[j + 0] = tmp2;
        out = silk_SMLAWB(out, tmp2, coef[j]);
    }
    data1[order - 1] = tmp1;
    out = silk_SMLAWB(out, tmp1, coef[order - 1]);
    /* Q11 -> Q12 */
    out = silk_LSHIFT32(out, 1);
    return out;
}