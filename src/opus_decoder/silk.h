/***********************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#pragma once
#include <Arduino.h>
#include "../psram_unique_ptr.hpp"
#include "opus_decoder.h"
#include "silk_defines.h"
#include "silk_tables.h"
#include "silk_structs.h"
#include "range_decoder.h"

extern const int16_t silk_LSFCosTab_FIX_Q12[LSF_COS_TAB_SZ_FIX + 1];
extern const int16_t silk_stereo_pred_quant_Q13[STEREO_QUANT_TAB_SIZE];
extern const uint8_t silk_stereo_pred_joint_iCDF[25];
extern const uint8_t silk_stereo_only_code_mid_iCDF[2];
extern const uint8_t silk_LBRR_flags_2_iCDF[3];
extern const uint8_t silk_LBRR_flags_3_iCDF[7];
extern const uint8_t* const silk_LBRR_flags_iCDF_ptr[2];
extern const uint8_t silk_lsb_iCDF[2];
extern const uint8_t silk_LTPscale_iCDF[3];
extern const uint8_t silk_type_offset_VAD_iCDF[4];
extern const uint8_t silk_type_offset_no_VAD_iCDF[2];
extern const uint8_t silk_NLSF_interpolation_factor_iCDF[5];
extern const int16_t silk_Quantization_Offsets_Q10[2][2];
extern const int16_t silk_LTPScales_table_Q14[3];
extern const uint8_t silk_uniform3_iCDF[3];
extern const uint8_t silk_uniform4_iCDF[4];
extern const uint8_t silk_uniform5_iCDF[5];
extern const uint8_t silk_uniform6_iCDF[6];
extern const uint8_t silk_uniform8_iCDF[8];
extern const uint8_t silk_NLSF_EXT_iCDF[7];
extern const int32_t silk_Transition_LP_B_Q28[TRANSITION_INT_NUM][TRANSITION_NB];
extern const int32_t silk_Transition_LP_A_Q28[TRANSITION_INT_NUM][TRANSITION_NA];
extern const uint8_t silk_max_pulses_table[4];
extern const uint8_t silk_pulses_per_block_iCDF[10][18];
extern const uint8_t silk_rate_levels_iCDF[2][9];
extern const uint8_t silk_rate_levels_BITS_Q5[2][9];
extern const uint8_t silk_shell_code_table0[152];
extern const uint8_t silk_shell_code_table1[152];
extern const uint8_t silk_shell_code_table2[152];
extern const uint8_t silk_shell_code_table3[152];
extern const uint8_t silk_shell_code_table_offsets[17];
extern const uint8_t silk_sign_iCDF[42];
extern const uint8_t silk_NLSF_CB1_NB_MB_Q8[320];
extern const int16_t silk_NLSF_CB1_Wght_Q9[320];
extern const uint8_t silk_NLSF_CB1_iCDF_NB_MB[64];
extern const uint8_t silk_NLSF_CB2_SELECT_NB_MB[160];
extern const uint8_t silk_NLSF_CB2_iCDF_NB_MB[72];
extern const uint8_t silk_NLSF_CB2_BITS_NB_MB_Q5[72];
extern const uint8_t silk_NLSF_PRED_NB_MB_Q8[18];
extern const int16_t silk_NLSF_DELTA_MIN_NB_MB_Q15[11];
extern const uint8_t silk_gain_iCDF[3][N_LEVELS_QGAIN / 8];
extern const uint8_t silk_delta_gain_iCDF[MAX_DELTA_GAIN_QUANT - MIN_DELTA_GAIN_QUANT + 1];
extern const uint8_t silk_pitch_lag_iCDF[2 * (PITCH_EST_MAX_LAG_MS - PITCH_EST_MIN_LAG_MS)];
extern const uint8_t silk_pitch_delta_iCDF[21];
extern const uint8_t silk_pitch_contour_iCDF[34];
extern const uint8_t silk_pitch_contour_NB_iCDF[11];
extern const uint8_t silk_pitch_contour_10_ms_iCDF[12];
extern const uint8_t silk_pitch_contour_10_ms_NB_iCDF[3];
extern const uint8_t silk_LTP_per_index_iCDF[3];
extern const uint8_t silk_LTP_gain_iCDF_0[8];
extern const uint8_t silk_LTP_gain_iCDF_1[16];
extern const uint8_t silk_LTP_gain_iCDF_2[32];
extern const uint8_t silk_LTP_gain_BITS_Q5_0[8];
extern const uint8_t silk_LTP_gain_BITS_Q5_1[16];
extern const uint8_t silk_LTP_gain_BITS_Q5_2[32];
extern const uint8_t* const silk_LTP_gain_iCDF_ptrs[NB_LTP_CBKS];
extern const uint8_t* const silk_LTP_gain_BITS_Q5_ptrs[NB_LTP_CBKS];
extern const int8_t silk_LTP_gain_vq_0[8][5];
extern const int8_t silk_LTP_gain_vq_1[16][5];
extern const int8_t silk_LTP_gain_vq_2[32][5];
extern const uint8_t silk_NLSF_CB1_WB_Q8[512];
extern const int16_t silk_NLSF_CB1_WB_Wght_Q9[512];
extern const uint8_t silk_NLSF_CB1_iCDF_WB[64];
extern const uint8_t silk_NLSF_CB2_SELECT_WB[256];
extern const uint8_t silk_NLSF_CB2_iCDF_WB[72];
extern const uint8_t silk_NLSF_CB2_BITS_WB_Q5[72];
extern const uint8_t silk_NLSF_PRED_WB_Q8[30];
extern const int16_t silk_NLSF_DELTA_MIN_WB_Q15[17];
extern const int8_t silk_CB_lags_stage2_10_ms[PE_MAX_NB_SUBFR >> 1][PE_NB_CBKS_STAGE2_10MS];
extern const int8_t silk_CB_lags_stage3_10_ms[PE_MAX_NB_SUBFR >> 1][PE_NB_CBKS_STAGE3_10MS];
extern const int8_t silk_CB_lags_stage3_10_ms[PE_MAX_NB_SUBFR >> 1][PE_NB_CBKS_STAGE3_10MS];
extern const int8_t silk_Lag_range_stage3_10_ms[PE_MAX_NB_SUBFR >> 1][2];
extern const int8_t silk_CB_lags_stage2[PE_MAX_NB_SUBFR][PE_NB_CBKS_STAGE2_EXT];
extern const int8_t silk_CB_lags_stage3[PE_MAX_NB_SUBFR][PE_NB_CBKS_STAGE3_MAX];
extern const int8_t silk_Lag_range_stage3[SILK_PE_MAX_COMPLEX + 1][PE_MAX_NB_SUBFR][2];
extern const int8_t delay_matrix_enc[5][3];
extern const int8_t delay_matrix_dec[3][5];
extern const int16_t silk_Resampler_3_4_COEFS[2 + 3 * RESAMPLER_DOWN_ORDER_FIR0 / 2];
extern const int16_t silk_Resampler_2_3_COEFS[2 + 2 * RESAMPLER_DOWN_ORDER_FIR0 / 2];
extern const int16_t silk_Resampler_1_2_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR1 / 2];
extern const int16_t silk_Resampler_1_3_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR2 / 2];
extern const int16_t silk_Resampler_1_4_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR2 / 2];
extern const int16_t silk_Resampler_1_6_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR2 / 2];
extern const int16_t silk_Resampler_2_3_COEFS_LQ[2 + 2 * 2];
extern const int16_t silk_resampler_frac_FIR_12[12][RESAMPLER_ORDER_FIR_12 / 2];
extern const int16_t HARM_ATT_Q15[NB_ATT];
extern const int16_t PLC_RAND_ATTENUATE_V_Q15[NB_ATT];
extern const int16_t PLC_RAND_ATTENUATE_UV_Q15[NB_ATT];
extern const int16_t silk_resampler_down2_0;
extern const int16_t silk_resampler_down2_1;
extern const int16_t silk_resampler_up2_hq_0[3];
extern const int16_t silk_resampler_up2_hq_1[3];
extern const int32_t sigm_LUT_slope_Q10[6];
extern const int32_t sigm_LUT_pos_Q15[6];
extern const int32_t sigm_LUT_neg_Q15[6];
extern const int8_t silk_nb_cbk_searchs_stage3[SILK_PE_MAX_COMPLEX + 1];


class SilkDecoder{
public:
    SilkDecoder(RangeDecoder& rangeDecoder) : rd(rangeDecoder) {}
    ~SilkDecoder() {reset();}
    bool init();
    void clear();
    void reset();
    int32_t silk_InitDecoder();
    void setChannelsAPI(uint8_t nChannelsAPI);
    void setChannelsInternal(uint8_t nChannelsInternal);
    void setAPIsampleRate(uint32_t API_sampleRate);
    void silk_setRawParams(uint8_t channels, uint8_t API_channels, uint8_t payloadSize_ms, uint32_t internalSampleRate, uint32_t API_samleRate);
    int32_t silk_Decode(int32_t lostFlag, int32_t newPacketFlag, int16_t *samplesOut, int32_t *nSamplesOut);

private:
    RangeDecoder& rd;  // Referenz auf RangeDecoder

    ps_ptr<silk_resampler_state_struct_t> m_resampler_state;
    ps_ptr<silk_decoder_state_t>          m_channel_state;
    ps_ptr<silk_decoder_t>                m_silk_decoder;
    ps_ptr<silk_decoder_control_t>        m_silk_decoder_control;
    ps_ptr<silk_DecControlStruct_t>       m_silk_DecControlStruct;

    uint8_t            m_channelsInternal = 0;
    uint8_t            m_payloadSize_ms = 0;
    uint8_t            m_API_channels = 0;
    uint32_t           m_silk_internalSampleRate = 0;
    uint32_t           m_API_sampleRate = 0;
    uint32_t           m_prevPitchLag = 0;

    /* Coefficients for 2-band filter bank based on first-order allpass filters */
    int16_t A_fb1_20 = 5394 << 1;
    int16_t A_fb1_21 = -24290; /* (int16_t)(20623 << 1) */

    const silk_NLSF_CB_struct_t silk_NLSF_CB_WB = {
        32,
        16,
        SILK_FIX_CONST(0.15, 16),
        SILK_FIX_CONST(1.0 / 0.15, 6),
        silk_NLSF_CB1_WB_Q8,
        silk_NLSF_CB1_WB_Wght_Q9,
        silk_NLSF_CB1_iCDF_WB,
        silk_NLSF_PRED_WB_Q8,
        silk_NLSF_CB2_SELECT_WB,
        silk_NLSF_CB2_iCDF_WB,
        silk_NLSF_CB2_BITS_WB_Q5,
        silk_NLSF_DELTA_MIN_WB_Q15,
    };

    const int8_t* const silk_LTP_vq_ptrs_Q7[NB_LTP_CBKS] = {(int8_t*)&silk_LTP_gain_vq_0[0][0], (int8_t*)&silk_LTP_gain_vq_1[0][0], (int8_t*)&silk_LTP_gain_vq_2[0][0]};

    /* Maximum frequency-dependent response of the pitch taps above,
       computed as max(abs(freqz(taps))) */
    const uint8_t silk_LTP_gain_vq_0_gain[8] = {46, 2, 90, 87, 93, 91, 82, 98};

    const uint8_t silk_LTP_gain_vq_1_gain[16] = {109, 120, 118, 12, 113, 115, 117, 119, 99, 59, 87, 111, 63, 111, 112, 80};

    const uint8_t silk_LTP_gain_vq_2_gain[32] = {126, 124, 125, 124, 129, 121, 126, 23, 132, 127, 127, 127, 126, 127, 122, 133,
                                                        130, 134, 101, 118, 119, 145, 126, 86, 124, 120, 123, 119, 170, 173, 107, 109};

    const uint8_t* const silk_LTP_vq_gain_ptrs_Q7[NB_LTP_CBKS] = {&silk_LTP_gain_vq_0_gain[0], &silk_LTP_gain_vq_1_gain[0], &silk_LTP_gain_vq_2_gain[0]};

    const int8_t silk_LTP_vq_sizes[NB_LTP_CBKS] = {8, 16, 32};

    const silk_NLSF_CB_struct_t silk_NLSF_CB_NB_MB = {
        32,
        10,
        SILK_FIX_CONST(0.18, 16),
        SILK_FIX_CONST(1.0 / 0.18, 6),
        silk_NLSF_CB1_NB_MB_Q8,
        silk_NLSF_CB1_Wght_Q9,
        silk_NLSF_CB1_iCDF_NB_MB,
        silk_NLSF_PRED_NB_MB_Q8,
        silk_NLSF_CB2_SELECT_NB_MB,
        silk_NLSF_CB2_iCDF_NB_MB,
        silk_NLSF_CB2_BITS_NB_MB_Q5,
        silk_NLSF_DELTA_MIN_NB_MB_Q15,
    };
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

    void silk_ana_filt_bank_1(const int16_t *in, int32_t *S, int16_t *outL, int16_t *outH, const int32_t N);
    void silk_biquad_alt_stride1(const int16_t *in, const int32_t *B_Q28, const int32_t *A_Q28, int32_t *S,int16_t *out, const int32_t len);
    void silk_biquad_alt_stride2_c(const int16_t *in, const int32_t *B_Q28, const int32_t *A_Q28, int32_t *S, int16_t *out, const int32_t len);
    void silk_bwexpander_32(int32_t *ar, const int32_t d, int32_t chirp_Q16);
    void silk_bwexpander(int16_t *ar, const int32_t d, int32_t chirp_Q16);
    void silk_stereo_decode_pred(int32_t pred_Q13[]);
    void silk_stereo_decode_mid_only(int32_t *decode_only_mid);
    void silk_PLC_Reset(uint8_t n);
    void silk_PLC(uint8_t n, int16_t frame[], int32_t lost);
    void silk_PLC_glue_frames(uint8_t, int16_t frame[], int32_t length);
    void silk_LP_interpolate_filter_taps(int32_t B_Q28[TRANSITION_NB], int32_t A_Q28[TRANSITION_NA], const int32_t ind, const int32_t fac_Q16);
    void silk_LP_variable_cutoff(silk_LP_state_t *psLP, int16_t *frame, const int32_t frame_length);
    void silk_NLSF_unpack(int16_t ec_ix[], uint8_t pred_Q8[], const silk_NLSF_CB_struct_t *psNLSF_CB, const int32_t CB1_index);
    void silk_NLSF_decode(int16_t *pNLSF_Q15, int8_t *NLSFIndices, const silk_NLSF_CB_struct_t   *psNLSF_CB);
    int32_t silk_decoder_set_fs(uint8_t n, int32_t fs_kHz, int32_t fs_API_Hz);
    int32_t combine_and_check(int32_t* pulses_comb, const int32_t* pulses_in, int32_t max_pulses, int32_t len);
    void silk_decode_indices(uint8_t n, int32_t FrameIndex, int32_t decode_LBRR, int32_t condCoding);
    void silk_decode_parameters(uint8_t n, int32_t condCoding);
    void silk_decode_core(uint8_t n, int16_t xq[], const int16_t pulses[MAX_FRAME_LENGTH]);
    void silk_decode_pulses(int16_t pulses[], const int32_t signalType, const int32_t quantOffsetType, const int32_t frame_length);
    int32_t silk_init_decoder(uint8_t n);
    int32_t silk_NLSF_del_dec_quant(int8_t indices[], const int16_t x_Q10[], const int16_t w_Q5[], const uint8_t pred_coef_Q8[], const int16_t ec_ix[], const uint8_t ec_rates_Q5[],
                                    const int32_t quant_step_size_Q16, const int16_t inv_quant_step_size_Q6, const int32_t mu_Q20, const int16_t order);
    void silk_NLSF_VQ(int32_t err_Q26[], const int16_t in_Q15[], const uint8_t pCB_Q8[], const int16_t pWght_Q9[], const int32_t K, const int32_t LPC_order);
    int32_t silk_VAD_Init(silk_VAD_state_t *psSilk_VAD);
    void silk_stereo_MS_to_LR(stereo_dec_state_t *state, int16_t x1[], int16_t x2[], const int32_t pred_Q13[], int32_t fs_kHz, int32_t frame_length);
    int32_t silk_stereo_find_predictor(int32_t *ratio_Q14, const int16_t x[], const int16_t y[], int32_t mid_res_amp_Q0[], int32_t length, int32_t smooth_coef_Q16);
    void silk_stereo_quant_pred(int32_t pred_Q13[], int8_t ix[2][3]);
    void silk_decode_signs(int16_t pulses[], int32_t length, const int32_t signalType, const int32_t quantOffsetType, const int32_t sum_pulses[MAX_NB_SHELL_BLOCKS]);
    void silk_shell_decoder(int16_t *pulses0, const int32_t pulses4);
    void silk_gains_quant(int8_t ind[MAX_NB_SUBFR], int32_t gain_Q16[MAX_NB_SUBFR], int8_t *prev_ind, const int32_t conditional, const int32_t nb_subfr);
    void silk_gains_dequant(int32_t gain_Q16[MAX_NB_SUBFR], const int8_t ind[MAX_NB_SUBFR], int8_t *prev_ind, const int32_t conditional, const int32_t nb_subfr);
    int32_t silk_gains_ID(const int8_t ind[MAX_NB_SUBFR], const int32_t nb_subfr);
    void silk_interpolate(int16_t xi[MAX_LPC_ORDER], const int16_t x0[MAX_LPC_ORDER], const int16_t x1[MAX_LPC_ORDER], const int32_t ifact_Q2, const int32_t d);
    void silk_quant_LTP_gains(int16_t B_Q14[MAX_NB_SUBFR * LTP_ORDER], int8_t cbk_index[MAX_NB_SUBFR], int8_t *periodicity_index, int32_t *sum_gain_dB_Q7, int32_t *pred_gain_dB_Q7,
                              const int32_t XX_Q17[MAX_NB_SUBFR * LTP_ORDER * LTP_ORDER], const int32_t xX_Q17[MAX_NB_SUBFR * LTP_ORDER], const int32_t subfr_len, const int32_t nb_subfr);
    void decode_split(int16_t *p_child1, int16_t *p_child2, const int32_t p, const uint8_t *shell_table);
    void silk_VQ_WMat_EC_c(int8_t *ind, int32_t *res_nrg_Q15, int32_t *rate_dist_Q8, int32_t *gain_Q7, const int32_t *XX_Q17, const int32_t *xX_Q17, const int8_t *cb_Q7, const uint8_t *cb_gain_Q7,
                           const uint8_t *cl_Q5, const int32_t subfr_len, const int32_t max_gain_Q7, const int32_t L);
    void silk_CNG_Reset(uint8_t n);
    void silk_CNG(uint8_t n, int16_t frame[], int32_t length);
    int32_t silk_Get_Decoder_Size(int32_t *decSizeBytes);
    void silk_NLSF2A_find_poly(int32_t *out, const int32_t *cLSF, int32_t dd);
    void silk_NLSF2A(int16_t *a_Q12, const int16_t *NLSF, const int32_t d);
    void silk_CNG_exc(int32_t exc_Q14[], int32_t exc_buf_Q14[], int32_t length, int32_t *rand_seed);
    int32_t silk_decode_frame(uint8_t n, int16_t pOut[], int32_t *pN, int32_t lostFlag, int32_t condCoding);
    void silk_decode_pitch(int16_t lagIndex, int8_t contourIndex, int32_t pitch_lags[], const int32_t Fs_kHz, const int32_t nb_subfr);
    int32_t silk_inner_prod_aligned_scale(const int16_t *const inVec1, const int16_t *const inVec2, const int32_t scale, const int32_t len);
    int32_t silk_lin2log(const int32_t inLin);
    int32_t silk_log2lin(const int32_t inLog_Q7);
    void silk_LPC_analysis_filter(int16_t *out, const int16_t *in, const int16_t *B, const int32_t len, const int32_t d);
    void silk_LPC_fit(int16_t *a_QOUT, int32_t *a_QIN, const int32_t QOUT, const int32_t QIN, const int32_t d);
    int32_t LPC_inverse_pred_gain_QA_c(int32_t A_QA[SILK_MAX_ORDER_LPC], const int32_t order);
    int32_t silk_LPC_inverse_pred_gain_c(const int16_t *A_Q12, const int32_t order);
    void silk_NLSF_residual_dequant(int16_t x_Q10[], const int8_t indices[], const uint8_t pred_coef_Q8[], const int32_t quant_step_size_Q16, const int16_t order);
    void silk_NLSF_stabilize(int16_t *NLSF_Q15, const int16_t *NDeltaMin_Q15, const int32_t L);
    void silk_NLSF_VQ_weights_laroia(int16_t *pNLSFW_Q_OUT, const int16_t *pNLSF_Q15, const int32_t D);
    void silk_PLC_update(uint8_t n);
    void silk_PLC_energy(int32_t *energy1, int32_t *shift1, int32_t *energy2, int32_t *shift2, const int32_t *exc_Q14, const int32_t *prevGain_Q10, int subfr_length, int nb_subfr);
    void silk_PLC_conceal(uint8_t n, int16_t frame[]);
    void silk_resampler_down2(int32_t *S, int16_t *out, const int16_t *in, int32_t inLen);
    void silk_resampler_private_AR2(int32_t S[], int32_t out_Q8[], const int16_t in[], const int16_t A_Q14[], int32_t len);
    int16_t *silk_resampler_private_down_FIR_INTERPOL(int16_t *out, int32_t *buf, const int16_t *FIR_Coefs, int32_t FIR_Order, int32_t FIR_Fracs, int32_t max_index_Q16, int32_t index_increment_Q16);
    void silk_resampler_private_down_FIR(void *SS, int16_t out[], const int16_t in[], int32_t inLen);
    int16_t *silk_resampler_private_IIR_FIR_INTERPOL(int16_t *out, int16_t *buf, int32_t max_index_Q16, int32_t index_increment_Q16);
    void silk_resampler_private_IIR_FIR(void *SS, int16_t out[], const int16_t in[], int32_t inLen);
    void silk_resampler_private_up2_HQ(int32_t *S, int16_t *out, const int16_t *in, int32_t len);
    void silk_resampler_private_up2_HQ_wrapper(void *SS, int16_t *out, const int16_t *in, int32_t len);
    int32_t silk_resampler_init(uint8_t n, int32_t Fs_Hz_in, int32_t Fs_Hz_out, int32_t forEnc);
    int32_t silk_resampler(uint8_t n, int16_t out[], const int16_t in[], int32_t inLen);
    int32_t silk_sigm_Q15(int32_t in_Q5);
    void silk_insertion_sort_increasing(int32_t *a, int32_t *idx, const int32_t L, const int32_t K);
    void silk_insertion_sort_decreasing_int16(int16_t *a, int32_t *idx, const int32_t L, const int32_t K);
    void silk_insertion_sort_increasing_all_values_int16(int16_t *a, const int32_t L);
    void silk_sum_sqr_shift(int32_t *energy, int32_t *shift, const int16_t *x, int32_t len);
    void combine_pulses(int32_t *out, const int32_t *in, const int32_t len);
    int32_t silk_INVERSE32_varQ(const int32_t b32, const int32_t Qres);
    int32_t silk_DIV32_varQ(const int32_t a32, const int32_t b32, const int32_t Qres);
    int32_t silk_SQRT_APPROX(int32_t x);
    void silk_CLZ_FRAC(int32_t in, int32_t *lz, int32_t *frac_Q7);
    uint32_t silk_getPrevPitchLag();
    int32_t silk_ROR32(int32_t a32, int32_t rot);
    int32_t silk_CLZ64(int64_t in);
    int32_t silk_min_int(int32_t a, int32_t b);
    int16_t silk_min_16(int16_t a, int16_t b);
    int32_t silk_min_32(int32_t a, int32_t b);
    int64_t silk_min_64(int64_t a, int64_t b);

    /* silk_min() versions with typecast in the function call */
    int32_t silk_max_int(int32_t a, int32_t b);
    int16_t silk_max_16(int16_t a, int16_t b);
    int32_t silk_max_32(int32_t a, int32_t b);
    int64_t silk_max_64(int64_t a, int64_t b);
    int32_t silk_noise_shape_quantizer_short_prediction_c(const int32_t *buf32, const int16_t *coef16, int32_t order);
    int32_t silk_NSQ_noise_shape_feedback_loop_c(const int32_t *data0, int32_t *data1, const int16_t *coef, int32_t order);
    int32_t silk_CLZ16(int16_t in16);
    int32_t silk_CLZ32(int32_t in32);
};