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
#include "range_decoder.h"


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

    #define SILK_MAX_FRAMES_PER_PACKET 3
    /* Decoder API flags */
    #define FLAG_DECODE_NORMAL                      0
    #define FLAG_PACKET_LOST                        1
    #define FLAG_DECODE_LBRR                        2
    #define SILK_ALLOC_NONE                         1
    /* Number of binary divisions, when not in low complexity mode */
    #define BIN_DIV_STEPS_A2NLSF_FIX 3 /* must be no higher than 16 - log2( LSF_COS_TAB_SZ_FIX ) */
    #define MAX_ITERATIONS_A2NLSF_FIX 16

    /* Fixed point macros */
    #define silk_MUL(a32, b32)                  ((a32) * (b32)) /* (a32 * b32) output have to be 32bit int */
    #define silk_MUL_uint(a32, b32)             silk_MUL(a32, b32) /* (a32 * b32) output have to be 32bit uint */
    #define silk_MLA(a32, b32, c32)             silk_ADD32((a32),((b32) * (c32))) /* a32 + (b32 * c32) output have to be 32bit int */
    #define silk_MLA_uint(a32, b32, c32)        silk_MLA(a32, b32, c32) /* a32 + (b32 * c32) output have to be 32bit uint */
    #define silk_SMULTT(a32, b32)               (((a32) >> 16) * ((b32) >> 16)) /* ((a32 >> 16)  * (b32 >> 16)) output have to be 32bit int */
    #define silk_SMLATT(a32, b32, c32)          silk_ADD32((a32),((b32) >> 16) * ((c32) >> 16)) /* a32 + ((a32 >> 16)  * (b32 >> 16)) output have to be 32bit int */
    #define silk_SMLALBB(a64, b16, c16)         silk_ADD64((a64),(int64_t)((int32_t)(b16) * (int32_t)(c16)))
    #define silk_SMULL(a32, b32)                ((int64_t)(a32) * /*(int64_t)*/(b32)) /* (a32 * b32) */
    #define silk_ADD32_ovflw(a, b)              ((int32_t)((uint32_t)(a) + (uint32_t)(b))) /* Adds two signed 32-bit values in a way that can overflow, while not relying on undefined behaviour (just standard two's complement implementation-specific behaviour) */
    #define silk_SUB32_ovflw(a, b)              ((int32_t)((uint32_t)(a) - (uint32_t)(b))) /* Subtractss two signed 32-bit values in a way that can overflow, while not relying on undefined behaviour (just standard two's complement implementation-specific behaviour) */
    #define silk_MLA_ovflw(a32, b32, c32)       silk_ADD32_ovflw((a32), (uint32_t)(b32) * (uint32_t)(c32)) /* Multiply-accumulate macros that allow overflow in the addition (ie, no asserts in debug mode) */
    #define silk_SMLABB_ovflw(a32, b32, c32)    (silk_ADD32_ovflw((a32) , ((int32_t)((int16_t)(b32))) * (int32_t)((int16_t)(c32))))
    #define silk_DIV32_16(a32, b16)             ((int32_t)((a32) / (b16)))
    #define silk_DIV32(a32, b32)                ((int32_t)((a32) / (b32)))
    #define silk_ADD16(a, b)                    ((a) + (b)) /* These macros enables checking for overflow in silk_API_Debug.h*/
    #define silk_ADD32(a, b)                    ((a) + (b))
    #define silk_ADD64(a, b)                    ((a) + (b))
    #define silk_SUB16(a, b)                    ((a) - (b))
    #define silk_SUB32(a, b)                    ((a) - (b))
    #define silk_SUB64(a, b)                    ((a) - (b))
    #define silk_SAT8(a)                        ((a) > silk_int8_MAX ? silk_int8_MAX  : ((a) < silk_int8_MIN ? silk_int8_MIN  : (a)))
    #define silk_SAT16(a)                       ((a) > silk_int16_MAX ? silk_int16_MAX : ((a) < silk_int16_MIN ? silk_int16_MIN : (a)))
    #define silk_SAT32(a)                       ((a) > silk_int32_MAX ? silk_int32_MAX : ((a) < silk_int32_MIN ? silk_int32_MIN : (a)))
    #define silk_CHECK_FIT8(a)                  (a)
    #define silk_CHECK_FIT16(a)                 (a)
    #define silk_CHECK_FIT32(a)                 (a)
    #define silk_ADD_SAT16(a, b)                (int16_t)silk_SAT16( silk_ADD32( (int32_t)(a), (b) ) )
    #define silk_ADD_SAT64(a, b)                ((((a) + (b)) & 0x8000000000000000LL) == 0 ?                              \
                                                ((((a) & (b)) & 0x8000000000000000LL) != 0 ? silk_int64_MIN : (a)+(b)) : ((((a) | (b)) & 0x8000000000000000LL) == 0 ? silk_int64_MAX : (a)+(b)) )
    #define silk_SUB_SAT16(a, b) (int16_t)silk_SAT16( silk_SUB32( (int32_t)(a), (b) ) )
    #define silk_SUB_SAT64(a, b) ((((a)-(b)) & 0x8000000000000000LL) == 0 ?                                               \
                                 (( (a) & ((b)^0x8000000000000000LL) & 0x8000000000000000LL) ? silk_int64_MIN : (a)-(b)) : ((((a)^0x8000000000000000LL) & (b)  & 0x8000000000000000LL) ? silk_int64_MAX : (a)-(b)) )
    #define silk_POS_SAT32(a)                   ((a) > silk_int32_MAX ? silk_int32_MAX : (a)) /* Saturation for positive input values */
    #define silk_ADD_POS_SAT8(a, b)             ((((a)+(b)) & 0x80)                 ? silk_int8_MAX  : ((a)+(b))) /* Add with saturation for positive input values */
    #define silk_ADD_POS_SAT16(a, b)            ((((a)+(b)) & 0x8000)               ? silk_int16_MAX : ((a)+(b)))
    #define silk_ADD_POS_SAT32(a, b)            ((((uint32_t)(a)+(uint32_t)(b)) & 0x80000000) ? silk_int32_MAX : ((a)+(b)))
    #define silk_LSHIFT8(a, shift)              ((int32_t8)((uint8_t)(a)<<(shift)))         /* shift >= 0, shift < 8  */
    #define silk_LSHIFT16(a, shift)             ((int16_t)((uint16_t)(a)<<(shift)))         /* shift >= 0, shift < 16 */
    #define silk_LSHIFT32(a, shift)             ((int32_t)((uint32_t)(a)<<(shift)))         /* shift >= 0, shift < 32 */
    #define silk_LSHIFT64(a, shift)             ((int64_t)((uint64_t)(a)<<(shift)))         /* shift >= 0, shift < 64 */
    #define silk_LSHIFT(a, shift)               silk_LSHIFT32(a, shift)                     /* shift >= 0, shift < 32 */
    #define silk_RSHIFT8(a, shift)              ((a)>>(shift))                              /* shift >= 0, shift < 8  */
    #define silk_RSHIFT16(a, shift)             ((a)>>(shift))                              /* shift >= 0, shift < 16 */
    #define silk_RSHIFT32(a, shift)             ((a)>>(shift))                              /* shift >= 0, shift < 32 */
    #define silk_RSHIFT64(a, shift)             ((a)>>(shift))                              /* shift >= 0, shift < 64 */
    #define silk_RSHIFT(a, shift)               silk_RSHIFT32(a, shift)                     /* shift >= 0, shift < 32 */
    #define silk_LSHIFT_SAT32(a, shift)         (silk_LSHIFT32( silk_LIMIT( (a), silk_RSHIFT32( silk_int32_MIN, (shift) ), silk_RSHIFT32( silk_int32_MAX, (shift) ) ), (shift) ))
    #define silk_LSHIFT_ovflw(a, shift)         ((int32_t)((uint32_t)(a) << (shift)))     /* shift >= 0, allowed to overflow */
    #define silk_LSHIFT_uint(a, shift)          ((a) << (shift))                                /* shift >= 0 */
    #define silk_RSHIFT_uint(a, shift)          ((a) >> (shift))                                /* shift >= 0 */
    #define silk_ADD_LSHIFT(a, b, shift)        ((a) + silk_LSHIFT((b), (shift)))               /* shift >= 0 */
    #define silk_ADD_LSHIFT32(a, b, shift)      silk_ADD32((a), silk_LSHIFT32((b), (shift)))    /* shift >= 0 */
    #define silk_ADD_LSHIFT_uint(a, b, shift)   ((a) + silk_LSHIFT_uint((b), (shift)))          /* shift >= 0 */
    #define silk_ADD_RSHIFT(a, b, shift)        ((a) + silk_RSHIFT((b), (shift)))               /* shift >= 0 */
    #define silk_ADD_RSHIFT32(a, b, shift)      silk_ADD32((a), silk_RSHIFT32((b), (shift)))    /* shift >= 0 */
    #define silk_ADD_RSHIFT_uint(a, b, shift)   ((a) + silk_RSHIFT_uint((b), (shift)))          /* shift >= 0 */
    #define silk_SUB_LSHIFT32(a, b, shift)      silk_SUB32((a), silk_LSHIFT32((b), (shift)))    /* shift >= 0 */
    #define silk_SUB_RSHIFT32(a, b, shift)      silk_SUB32((a), silk_RSHIFT32((b), (shift)))    /* shift >= 0 */
    #define silk_RSHIFT_ROUND(a, shift)         ((shift) == 1 ? ((a) >> 1) + ((a) & 1) : (((a) >> ((shift) - 1)) + 1) >> 1)  /* Requires that shift > 0 */
    #define silk_RSHIFT_ROUND64(a, shift)       ((shift) == 1 ? ((a) >> 1) + ((a) & 1) : (((a) >> ((shift) - 1)) + 1) >> 1)
    #define silk_NSHIFT_MUL_32_32(a, b)         ( -(31- (32-silk_CLZ32(silk_abs(a)) + (32-silk_CLZ32(silk_abs(b))))) ) /* Number of rightshift required to fit the multiplication */
    #define silk_NSHIFT_MUL_16_16(a, b)         ( -(15- (16-silk_CLZ16(silk_abs(a)) + (16-silk_CLZ16(silk_abs(b))))) )
    #define silk_min(a, b)                      (((a) < (b)) ? (a) : (b))
    #define silk_max(a, b)                      (((a) > (b)) ? (a) : (b))
    #define MIN_QGAIN_DB                            2   /* dB level of lowest gain quantization level */
    #define MAX_QGAIN_DB                            88  /* dB level of highest gain quantization level */
    #define N_LEVELS_QGAIN                          64  /* Number of gain quantization levels */
    #define MAX_DELTA_GAIN_QUANT                    36  /* Max increase in gain quantization index */
    #define MIN_DELTA_GAIN_QUANT                    -4  /* Max decrease in gain quantization index */
    #define OFFSET_VL_Q10                           32  /* Quantization offsets (multiples of 4) */
    #define OFFSET_VH_Q10                           100
    #define OFFSET_UVL_Q10                          100
    #define OFFSET_UVH_Q10                          240
    #define QUANT_LEVEL_ADJUST_Q10                  80
    #define MAX_LPC_STABILIZE_ITERATIONS            16  /* Maximum numbers of iterations used to stabilize an LPC vector */
    #define MAX_PREDICTION_POWER_GAIN               1e4f
    #define MAX_PREDICTION_POWER_GAIN_AFTER_RESET   1e2f
    #define MAX_LPC_ORDER                           16
    #define MIN_LPC_ORDER                           10
    #define LTP_ORDER                               5   /* Find Pred Coef defines */
    #define NB_LTP_CBKS                             3   /* LTP quantization settings */
    #define USE_HARM_SHAPING                        1   /* Flag to use harmonic noise shaping */
    #define MAX_SHAPE_LPC_ORDER                     24  /* Max LPC order of noise shaping filters */
    #define HARM_SHAPE_FIR_TAPS                     3
    #define MAX_DEL_DEC_STATES                      4   /* Maximum number of delayed decision states */
    #define LTP_BUF_LENGTH                          512
    #define LTP_MASK                                ( LTP_BUF_LENGTH - 1 )
    #define DECISION_DELAY                          40
    #define MAX_NB_SUBFR                            4   /* Maximum number of subframes */
    #define ENCODER_NUM_CHANNELS                    2   /* Max number of encoder channels (1/2) */
    #define DECODER_NUM_CHANNELS                    2   /* Number of decoder channels (1/2) */
    #define MAX_FRAMES_PER_PACKET                   3
    #define MIN_TARGET_RATE_BPS                     5000    /* Limits on bitrate */
    #define MAX_TARGET_RATE_BPS                     80000
    #define LBRR_NB_MIN_RATE_BPS                    12000   /* LBRR thresholds */
    #define LBRR_MB_MIN_RATE_BPS                    14000
    #define LBRR_WB_MIN_RATE_BPS                    16000
    #define NB_SPEECH_FRAMES_BEFORE_DTX             10      /* eq 200 ms */
    #define MAX_CONSECUTIVE_DTX                     20      /* eq 400 ms */
    #define DTX_ACTIVITY_THRESHOLD                  0.1f
    #define VAD_NO_DECISION                         -1  /* VAD decision */
    #define VAD_NO_ACTIVITY                         0
    #define VAD_ACTIVITY                            1
    #define MAX_FS_KHZ                              16  /* Maximum sampling frequency */
    #define MAX_API_FS_KHZ                          48
    #define TYPE_NO_VOICE_ACTIVITY                  0   /* Signal types */
    #define TYPE_UNVOICED                           1
    #define TYPE_VOICED                             2
    #define CODE_INDEPENDENTLY                      0       /* Conditional coding types */
    #define CODE_INDEPENDENTLY_NO_LTP_SCALING       1
    #define CODE_CONDITIONALLY                      2
    #define STEREO_QUANT_TAB_SIZE                   16      /* Settings for stereo processing */
    #define STEREO_QUANT_SUB_STEPS                  5
    #define STEREO_INTERP_LEN_MS                    8       /* must be even */
    #define STEREO_RATIO_SMOOTH_COEF                0.01    /* smoothing coef for signal norms and stereo width */
    #define PITCH_EST_MIN_LAG_MS                    2       /* 2 ms -> 500 Hz */
    #define PITCH_EST_MAX_LAG_MS                    18      /* 18 ms -> 56 Hz */
    #define LTP_MEM_LENGTH_MS                       20  /* Number of samples per frame */
    #define SUB_FRAME_LENGTH_MS                     5
    #define MAX_SUB_FRAME_LENGTH                    ( SUB_FRAME_LENGTH_MS * MAX_FS_KHZ )
    #define MAX_FRAME_LENGTH_MS                     ( SUB_FRAME_LENGTH_MS * MAX_NB_SUBFR )
    #define MAX_FRAME_LENGTH                        ( MAX_FRAME_LENGTH_MS * MAX_FS_KHZ )
    #define LA_PITCH_MS                             2   /* Milliseconds of lookahead for pitch analysis */
    #define LA_PITCH_MAX                            ( LA_PITCH_MS * MAX_FS_KHZ )
    #define MAX_FIND_PITCH_LPC_ORDER                16  /* Order of LPC used in find pitch */
    #define FIND_PITCH_LPC_WIN_MS                   ( 20 + (LA_PITCH_MS << 1) )/* Length of LPC window used in find pitch */
    #define FIND_PITCH_LPC_WIN_MS_2_SF              ( 10 + (LA_PITCH_MS << 1) )
    #define FIND_PITCH_LPC_WIN_MAX                  ( FIND_PITCH_LPC_WIN_MS * MAX_FS_KHZ )
    #define LA_SHAPE_MS                             5   /* Milliseconds of lookahead for noise shape analysis */
    #define LA_SHAPE_MAX                            ( LA_SHAPE_MS * MAX_FS_KHZ )
    #define SHAPE_LPC_WIN_MAX                       ( 15 * MAX_FS_KHZ )/* Max lenof LPCwindow in noise shape analysis */
    #define SHELL_CODEC_FRAME_LENGTH                16 /* Number of subframes for excitation entropy coding */
    #define LOG2_SHELL_CODEC_FRAME_LENGTH           4
    #define MAX_NB_SHELL_BLOCKS                     ( MAX_FRAME_LENGTH / SHELL_CODEC_FRAME_LENGTH )
    #define N_RATE_LEVELS                           10 /* Number of rate levels, for entropy coding of excitation */
    #define SILK_MAX_PULSES                         16 /* Maximum sum of pulses per shell coding frame */
    #define MAX_MATRIX_SIZE                         MAX_LPC_ORDER /* Max of LPC Order and LTP order */
    #define NSQ_LPC_BUF_LENGTH                      MAX_LPC_ORDER
    #define VAD_N_BANDS                             4
    #define VAD_INTERNAL_SUBFRAMES_LOG2             2
    #define VAD_INTERNAL_SUBFRAMES                  ( 1 << VAD_INTERNAL_SUBFRAMES_LOG2 )
    #define VAD_NOISE_LEVEL_SMOOTH_COEF_Q16         1024    /* Must be <  4096 */
    #define VAD_NOISE_LEVELS_BIAS                   50
    #define VAD_NEGATIVE_OFFSET_Q5                  128     /* sigmoid is 0 at -128 */
    #define VAD_SNR_FACTOR_Q16                      45000
    #define VAD_SNR_SMOOTH_COEF_Q18                 4096 /* smoothing for SNR measurement */
    #define LSF_COS_TAB_SZ_FIX                      128/* Sizeof piecewise linear cos approximation table for the LSFs */
    #define BWE_COEF                                0.99
    #define V_PITCH_GAIN_START_MIN_Q14              11469
    #define V_PITCH_GAIN_START_MAX_Q14              15565
    #define MAX_PITCH_LAG_MS                        18
    #define RAND_BUF_SIZE                           128
    #define RAND_BUF_MASK                           ( RAND_BUF_SIZE - 1)
    #define LOG2_INV_LPC_GAIN_HIGH_THRES            3
    #define LOG2_INV_LPC_GAIN_LOW_THRES             8
    #define PITCH_DRIFT_FAC_Q16                     655
    #define BITRESERVOIR_DECAY_TIME_MS              500 /* Decay time for bitreservoir */
    #define FIND_PITCH_WHITE_NOISE_FRACTION         1e-3f /* Level of noise floor for whitening filter LPC analysis */
    #define FIND_PITCH_BANDWIDTH_EXPANSION          0.99f /* Bandwidth expansion for whitening filter in pitch analysis */
    #define FIND_LPC_COND_FAC                       1e-5f /* LPC analysis regularization */
    #define MAX_SUM_LOG_GAIN_DB                     250.0f /* Max cumulative LTP gain */
    #define LTP_CORR_INV_MAX                        0.03f /* LTP analysis defines */
    #define VARIABLE_HP_SMTH_COEF1                  0.1f
    #define VARIABLE_HP_SMTH_COEF2                  0.015f
    #define VARIABLE_HP_MAX_DELTA_FREQ              0.4f
    #define VARIABLE_HP_MIN_CUTOFF_HZ               60 /* Min and max cut-off frequency values (-3 dB points) */
    #define VARIABLE_HP_MAX_CUTOFF_HZ               100
    #define SPEECH_ACTIVITY_DTX_THRES               0.05f /* VAD threshold */
    #define LBRR_SPEECH_ACTIVITY_THRES              0.3f /* Speech Activity LBRR enable threshold */
    #define BG_SNR_DECR_dB                          2.0f /* reduction in coding SNR during low speech activity */
    #define HARM_SNR_INCR_dB                        2.0f /* factor for reducing quantization noise during voiced speech */
    #define SPARSE_SNR_INCR_dB                      2.0f /* factor for reducing quant. noise for unvoiced sparse signals */
    #define ENERGY_VARIATION_THRESHOLD_QNT_OFFSET   0.6f
    #define WARPING_MULTIPLIER                      0.015f /* warping control */
    #define SHAPE_WHITE_NOISE_FRACTION              3e-5f /* fraction added to first autocorrelation value */
    #define BANDWIDTH_EXPANSION                     0.94f /* noise shaping filter chirp factor */
    #define HARMONIC_SHAPING                        0.3f /* harmonic noise shaping */
    #define HIGH_RATE_OR_LOW_QUALITY_HARMONIC_SHAPING  0.2f /* extra harmonic noise shaping for high bitr. or noisy input */
    #define HP_NOISE_COEF                           0.25f /* parameter for shaping noise towards higher frequencies */
    #define HARM_HP_NOISE_COEF                      0.35f
    #define INPUT_TILT                              0.05f
    #define HIGH_RATE_INPUT_TILT                    0.1f /* for extra high-pass tilt to the input signal at high rates */
    #define LOW_FREQ_SHAPING                        4.0f /* parameter for reducing noise at the very low frequencies */
    #define LOW_QUALITY_LOW_FREQ_SHAPING_DECR       0.5f
    #define SUBFR_SMTH_COEF                         0.4f
    #define LAMBDA_OFFSET                           1.2f /* param. defining the R/D tradeoff in the residual quantizer */
    #define LAMBDA_SPEECH_ACT                       -0.2f
    #define LAMBDA_DELAYED_DECISIONS                -0.05f
    #define LAMBDA_INPUT_QUALITY                    -0.1f
    #define LAMBDA_CODING_QUALITY                   -0.2f
    #define LAMBDA_QUANT_OFFSET                     0.8f
    #define REDUCE_BITRATE_10_MS_BPS                2200 /* Compensation in bitrate calculations for 10 ms modes */
    #define MAX_BANDWIDTH_SWITCH_DELAY_MS           5000 /* Maximum time before allowing a bandwidth transition */
    #define silk_int64_MAX   ((int64_t)0x7FFFFFFFFFFFFFFFLL)   /*  2^63 - 1 */
    #define silk_int64_MIN   ((int64_t)0x8000000000000000LL)   /* -2^63 */
    #define silk_int32_MAX   0x7FFFFFFF                        /*  2^31 - 1 =  2147483647 */
    #define silk_int32_MIN   ((int32_t)0x80000000)             /* -2^31     = -2147483648 */
    #define silk_int16_MAX   0x7FFF                            /*  2^15 - 1 =  32767 */
    #define silk_int16_MIN   ((int16_t)0x8000)                 /* -2^15     = -32768 */
    #define silk_int8_MAX    0x7F                              /*  2^7 - 1  =  127 */
    #define silk_int8_MIN    ((int8_t)0x80)                    /* -2^7      = -128 */
    #define silk_uint8_MAX   0xFF                              /*  2^8 - 1 = 255 */
    #define silk_TRUE        1
    #define silk_FALSE       0
    #define silk_enc_map(a)                  ( silk_RSHIFT( (a), 15 ) + 1 )
    #define silk_dec_map(a)                  ( silk_LSHIFT( (a),  1 ) - 1 )
    #define SILK_FIX_CONST(C, Q) ((int32_t)((C) * ((int64_t)1 << (Q)) + 0.5L)) /* Macro to convert floating-point constants to fixed-point */
    #define TIC(TAG_NAME) /* define macros as empty strings */
    #define TOC(TAG_NAME)
    #define silk_TimerSave(FILE_NAME)
    #define NLSF_W_Q                                2 /* NLSF quantizer */
    #define NLSF_VQ_MAX_VECTORS                     32
    #define NLSF_QUANT_MAX_AMPLITUDE                4
    #define NLSF_QUANT_MAX_AMPLITUDE_EXT            10
    #define NLSF_QUANT_LEVEL_ADJ                    0.1
    #define NLSF_QUANT_DEL_DEC_STATES_LOG2          2
    #define NLSF_QUANT_DEL_DEC_STATES               ( 1 << NLSF_QUANT_DEL_DEC_STATES_LOG2 )
    #define TRANSITION_TIME_MS                      5120    /* 5120 = 64 * FRAME_LENGTH_MS * ( TRANSITION_INT_NUM - 1 ) = 64*(20*4)*/
    #define TRANSITION_NB                           3       /* Hardcoded in tables */
    #define TRANSITION_NA                           2       /* Hardcoded in tables */
    #define TRANSITION_INT_NUM                      5       /* Hardcoded in tables */
    #define TRANSITION_FRAMES                       ( TRANSITION_TIME_MS / MAX_FRAME_LENGTH_MS )
    #define TRANSITION_INT_STEPS                    ( TRANSITION_FRAMES  / ( TRANSITION_INT_NUM - 1 ) )
    #define BWE_AFTER_LOSS_Q16                      63570 /* BWE factors to apply after packet loss */
    #define CNG_BUF_MASK_MAX                        255     /* 2^floor(log2(MAX_FRAME_LENGTH))-1    */
    #define CNG_GAIN_SMTH_Q16                       4634    /* 0.25^(1/4)                           */
    #define CNG_NLSF_SMTH_Q16                       16348   /* 0.25                                 */
    #define PE_MAX_FS_KHZ               16 /* Maximum sampling frequency used */
    #define PE_MAX_NB_SUBFR             4
    #define PE_SUBFR_LENGTH_MS          5   /* 5 ms */
    #define PE_LTP_MEM_LENGTH_MS        ( 4 * PE_SUBFR_LENGTH_MS )
    #define PE_MAX_FRAME_LENGTH_MS      ( PE_LTP_MEM_LENGTH_MS + PE_MAX_NB_SUBFR * PE_SUBFR_LENGTH_MS )
    #define PE_MAX_FRAME_LENGTH         ( PE_MAX_FRAME_LENGTH_MS * PE_MAX_FS_KHZ )
    #define PE_MAX_FRAME_LENGTH_ST_1    ( PE_MAX_FRAME_LENGTH >> 2 )
    #define PE_MAX_FRAME_LENGTH_ST_2    ( PE_MAX_FRAME_LENGTH >> 1 )
    #define PE_MAX_LAG_MS               18           /* 18 ms -> 56 Hz */
    #define PE_MIN_LAG_MS               2            /* 2 ms -> 500 Hz */
    #define PE_MAX_LAG                  ( PE_MAX_LAG_MS * PE_MAX_FS_KHZ )
    #define PE_MIN_LAG                  ( PE_MIN_LAG_MS * PE_MAX_FS_KHZ )
    #define PE_D_SRCH_LENGTH            24
    #define PE_NB_STAGE3_LAGS           5
    #define PE_NB_CBKS_STAGE2           3
    #define PE_NB_CBKS_STAGE2_EXT       11
    #define PE_NB_CBKS_STAGE3_MAX       34
    #define PE_NB_CBKS_STAGE3_MID       24
    #define PE_NB_CBKS_STAGE3_MIN       16
    #define PE_NB_CBKS_STAGE3_10MS      12
    #define PE_NB_CBKS_STAGE2_10MS      3
    #define PE_SHORTLAG_BIAS            0.2f    /* for logarithmic weighting    */
    #define PE_PREVLAG_BIAS             0.2f    /* for logarithmic weighting    */
    #define PE_FLATCONTOUR_BIAS         0.05f
    #define SILK_PE_MIN_COMPLEX         0
    #define SILK_PE_MID_COMPLEX         1
    #define SILK_PE_MAX_COMPLEX         2
    #define USE_CELT_FIR                0
    #define MAX_LOOPS                   20
    #define NB_ATT                      2
    #define ORDER_FIR                   4
    #define RESAMPLER_DOWN_ORDER_FIR0   18
    #define RESAMPLER_DOWN_ORDER_FIR1   24
    #define RESAMPLER_DOWN_ORDER_FIR2   36
    #define RESAMPLER_ORDER_FIR_12      8
    #define SILK_MAX_ORDER_LPC          24     /* max order of the LPC analysis in schur() and k2a() */
    #define SILK_RESAMPLER_MAX_FIR_ORDER                 36
    #define SILK_RESAMPLER_MAX_IIR_ORDER                 6
    #define A_LIMIT                     SILK_FIX_CONST( 0.99975, 24 )
    #define MUL32_FRAC_Q(a32, b32, Q)   ((int32_t)(silk_RSHIFT_ROUND64(silk_SMULL(a32, b32), Q)))
    #define RESAMPLER_MAX_BATCH_SIZE_MS                  10 /* Number of input samples to process in the inner loop */
    #define RESAMPLER_MAX_FS_KHZ                         48
    #define RESAMPLER_MAX_BATCH_SIZE_IN                  ( RESAMPLER_MAX_BATCH_SIZE_MS * RESAMPLER_MAX_FS_KHZ )
    #define rateID(R) ( ( ( ((R)>>12) - ((R)>16000) ) >> ((R)>24000) ) - 1 ) /* Simple way to make [8000, 12000, 16000, 24000, 48000] to [0, 1, 2, 3, 4] */
    #define USE_silk_resampler_copy                     (0)
    #define USE_silk_resampler_private_up2_HQ_wrapper   (1)
    #define USE_silk_resampler_private_IIR_FIR          (2)
    #define USE_silk_resampler_private_down_FIR         (3)
    #define SILK_NO_ERROR                                0
    #define silk_encoder_state_Fxx      silk_encoder_state_FIX
    #define silk_encode_do_VAD_Fxx      silk_encode_do_VAD_FIX
    #define silk_encode_frame_Fxx       silk_encode_frame_FIX
    #define silk_LIMIT(a, limit1, limit2) ((limit1) > (limit2) ? ((a) > (limit1) ? (limit1) : ((a) < (limit2) ? (limit2) : (a))) : ((a) > (limit2) ? (limit2) : ((a) < (limit1) ? (limit1) : (a))))
    #define silk_sign(a)                        ((a) > 0 ? 1 : ( (a) < 0 ? -1 : 0 ))
    #define silk_LIMIT_int                      silk_LIMIT
    #define silk_LIMIT_16                       silk_LIMIT
    #define silk_LIMIT_32                       silk_LIMIT
    #define silk_abs(a)                         (((a) >  0)  ? (a) : -(a))
    #define silk_abs_int(a)                     (((a) ^ ((a) >> (8 * sizeof(a) - 1))) - ((a) >> (8 * sizeof(a) - 1)))
    #define silk_abs_int32(a)                   (((a) ^ ((a) >> 31)) - ((a) >> 31))
    #define silk_abs_int64(a)                   (((a) >  0)  ? (a) : -(a))
    #define OFFSET ((MIN_QGAIN_DB * 128) / 6 + 16 * 128)
    #define SCALE_Q16 ((65536 * (N_LEVELS_QGAIN - 1)) / (((MAX_QGAIN_DB - MIN_QGAIN_DB) * 128) / 6))
    #define INV_SCALE_Q16 ((65536 * (((MAX_QGAIN_DB - MIN_QGAIN_DB) * 128) / 6)) / (N_LEVELS_QGAIN - 1))
    #define silk_SMULWB(a32, b32)            ((int32_t)(((a32) * (int64_t)((int16_t)(b32))) >> 16)) /* (a32 * (int32_t)((int16_t)(b32))) >> 16 output have to be 32bit int */
    #define silk_SMLAWB(a32, b32, c32)       ((int32_t)((a32) + (((b32) * (int64_t)((int16_t)(c32))) >> 16))) /* a32 + (b32 * (int32_t)((int16_t)(c32))) >> 16 output have to be 32bit int */
    #define silk_SMULWT(a32, b32)            ((int32_t)(((a32) * (int64_t)((b32) >> 16)) >> 16)) /* (a32 * (b32 >> 16)) >> 16 */
    #define silk_SMLAWT(a32, b32, c32)       ((int32_t)((a32) + (((b32) * ((int64_t)(c32) >> 16)) >> 16)))/* a32 + (b32 * (c32 >> 16)) >> 16 */
    #define silk_SMULBB(a32, b32)            ((int32_t)((int16_t)(a32)) * (int32_t)((int16_t)(b32))) /* (int32_t)((int16_t)(a3))) * (int32_t)((int16_t)(b32)) output have to be 32bit int */
    #define silk_SMLABB(a32, b32, c32)       ((a32) + ((int32_t)((int16_t)(b32))) * (int32_t)((int16_t)(c32))) /* a32 + (int32_t)((int16_t)(b32)) * (int32_t)((int16_t)(c32)) output have to be 32bit int */
    #define silk_SMULBT(a32, b32)            ((int32_t)((int16_t)(a32)) * ((b32) >> 16)) /* (int32_t)((int16_t)(a32)) * (b32 >> 16) */
    #define silk_SMLABT(a32, b32, c32)       ((a32) + ((int32_t)((int16_t)(b32))) * ((c32) >> 16)) /* a32 + (int32_t)((int16_t)(b32)) * (c32 >> 16) */
    #define silk_SMLAL(a64, b32, c32)        (silk_ADD64((a64), ((int64_t)(b32) * (int64_t)(c32)))) /* a64 + (b32 * c32) */
    #define silk_SMULWW(a32, b32)            ((int32_t)(((int64_t)(a32) * (b32)) >> 16))  /* (a32 * b32) >> 16 */
    #define silk_SMLAWW(a32, b32, c32)       ((int32_t)((a32) + (((int64_t)(b32) * (c32)) >> 16))) /* a32 + ((b32 * c32) >> 16) */
    #define silk_ADD_SAT32(a, b)             ((((uint32_t)(a) + (uint32_t)(b)) & 0x80000000) == 0 ?                 \
                                             ((((a) & (b)) & 0x80000000) != 0 ? silk_int32_MIN : (a)+(b)) : ((((a) | (b)) & 0x80000000) == 0 ? silk_int32_MAX : (a)+(b)) )
    #define silk_SUB_SAT32(a, b)             ((((uint32_t)(a)-(uint32_t)(b)) & 0x80000000) == 0 ?                    \
                                             (( (a) & ((b)^0x80000000) & 0x80000000) ? silk_int32_MIN : (a)-(b)) : ((((a)^0x80000000) & (b)  & 0x80000000) ? silk_int32_MAX : (a)-(b)) )
    #define EC_CLZ0    ((int)sizeof(unsigned)*CHAR_BIT)
    #define EC_CLZ(_x) (__builtin_clz(_x))
    #define EC_ILOGs(_x) (EC_CLZ0-EC_CLZ(_x))
    #define matrix_ptr(Matrix_base_adr, row, column, N) (*((Matrix_base_adr) + ((row) * (N) + (column)))) /* Row based */
    #define matrix_adr(Matrix_base_adr, row, column, N) ((Matrix_base_adr) + ((row) * (N) + (column)))
    #define silk_VQ_WMat_EC(ind, res_nrg_Q15, rate_dist_Q8, gain_Q7, XX_Q17, xX_Q17, cb_Q7, cb_gain_Q7, cl_Q5, subfr_len, max_gain_Q7, L) (silk_VQ_WMat_EC_c(ind, res_nrg_Q15, rate_dist_Q8, gain_Q7, XX_Q17, xX_Q17, cb_Q7, cb_gain_Q7, cl_Q5, subfr_len, max_gain_Q7, L))
    #define silk_noise_shape_quantizer_short_prediction(in, coef, coefRev, order) (silk_noise_shape_quantizer_short_prediction_c(in, coef, order))
    #define silk_SMMUL(a32, b32) (int32_t) silk_RSHIFT64(silk_SMULL((a32), (b32)), 32)
    #define silk_burg_modified(res_nrg, res_nrg_Q, A_Q16, x, minInvGain_Q30, subfr_length, nb_subfr, D) (silk_burg_modified_c(res_nrg, res_nrg_Q, A_Q16, x, minInvGain_Q30, subfr_length, nb_subfr, D))
    #define silk_inner_prod16_aligned_64(inVec1, inVec2, len) (silk_inner_prod16_aligned_64_c(inVec1, inVec2, len))
    #define silk_biquad_alt_stride2(in, B_Q28, A_Q28, S, out, len) (silk_biquad_alt_stride2_c(in, B_Q28, A_Q28, S, out, len))
    #define silk_LPC_inverse_pred_gain(A_Q12, order) (silk_LPC_inverse_pred_gain_c(A_Q12, order))
    #define silk_sign(a)                        ((a) > 0 ? 1 : ( (a) < 0 ? -1 : 0 ))
    #define RAND_MULTIPLIER                     196314165
    #define RAND_INCREMENT                      907633515
    #define silk_RAND(seed)                     (silk_MLA_ovflw((RAND_INCREMENT), (seed), (RAND_MULTIPLIER)))
    #define silk_NSQ_noise_shape_feedback_loop(data0, data1, coef, order) (silk_NSQ_noise_shape_feedback_loop_c(data0, data1, coef, order))

    typedef struct {
        int8_t                    GainsIndices[ MAX_NB_SUBFR ];
        int8_t                    LTPIndex[ MAX_NB_SUBFR ];
        int8_t                    NLSFIndices[ MAX_LPC_ORDER + 1 ];
        int16_t                   lagIndex;
        int8_t                    contourIndex;
        int8_t                    signalType;
        int8_t                    quantOffsetType;
        int8_t                    NLSFInterpCoef_Q2;
        int8_t                    PERIndex;
        int8_t                    LTP_scaleIndex;
        int8_t                    Seed;
    } sideInfoIndices_t;

    typedef struct {
        int32_t                  AnaState[ 2 ];                  /* Analysis filterbank state: 0-8 kHz                                   */
        int32_t                  AnaState1[ 2 ];                 /* Analysis filterbank state: 0-4 kHz                                   */
        int32_t                  AnaState2[ 2 ];                 /* Analysis filterbank state: 0-2 kHz                                   */
        int32_t                  XnrgSubfr[ VAD_N_BANDS ];       /* Subframe energies                                                    */
        int32_t                  NrgRatioSmth_Q8[ VAD_N_BANDS ]; /* Smoothed energy level in each band                                   */
        int16_t                  HPstate;                        /* State of differentiator in the lowest band                           */
        int32_t                  NL[ VAD_N_BANDS ];              /* Noise energy level in each band                                      */
        int32_t                  inv_NL[ VAD_N_BANDS ];          /* Inverse noise energy level in each band                              */
        int32_t                  NoiseLevelBias[ VAD_N_BANDS ];  /* Noise level estimator bias/offset                                    */
        int32_t                  counter;                        /* Frame counter used in the initial phase                              */
    } silk_VAD_state_t;


    typedef struct {                                          /* Variable cut-off low-pass filter state */
        int32_t                   In_LP_State[ 2 ];           /* Low pass filter state */
        int32_t                   transition_frame_no;        /* Counter which is mapped to a cut-off frequency */
        int32_t                   mode;                       /* Operating mode, <0: switch down, >0: switch up; 0: do nothing           */
        int32_t                   saved_fs_kHz;               /* If non-zero, holds the last sampling rate before a bandwidth switching reset. */
    } silk_LP_state_t;


    typedef struct {   /* Structure containing NLSF codebook */
        const int16_t             nVectors;
        const int16_t             order;
        const int16_t             quantStepSize_Q16;
        const int16_t             invQuantStepSize_Q6;
        const uint8_t             *CB1_NLSF_Q8;
        const int16_t             *CB1_Wght_Q9;
        const uint8_t             *CB1_iCDF;
        const uint8_t             *pred_Q8;
        const uint8_t             *ec_sel;
        const uint8_t             *ec_iCDF;
        const uint8_t             *ec_Rates_Q5;
        const int16_t             *deltaMin_Q15;
    } silk_NLSF_CB_struct_t;

    typedef struct _silk_resampler_state_struct{
        int32_t       sIIR[ SILK_RESAMPLER_MAX_IIR_ORDER ]; /* this must be the first element of this struct */
        union{
            int32_t   i32[ SILK_RESAMPLER_MAX_FIR_ORDER ];
            int16_t   i16[ SILK_RESAMPLER_MAX_FIR_ORDER ];
        }                sFIR;
        int16_t       delayBuf[ 48 ];
        int32_t         resampler_function;
        int32_t         batchSize;
        int32_t       invRatio_Q16;
        int32_t         FIR_Order;
        int32_t         FIR_Fracs;
        int32_t         Fs_in_kHz;
        int32_t         Fs_out_kHz;
        int32_t         inputDelay;
        const int16_t *Coefs;
    } silk_resampler_state_struct_t;

    typedef struct {
        int16_t                   pred_prev_Q13[ 2 ];
        int16_t                   sMid[ 2 ];
        int16_t                   sSide[ 2 ];
    } stereo_dec_state_t;

/* Struct for Packet Loss Concealment */
    typedef struct {
        int32_t                  pitchL_Q8;                          /* Pitch lag to use for voiced concealment                          */
        int16_t                  LTPCoef_Q14[ LTP_ORDER ];           /* LTP coeficients to use for voiced concealment                    */
        int16_t                  prevLPC_Q12[ MAX_LPC_ORDER ];
        int32_t                  last_frame_lost;                    /* Was previous frame lost                                          */
        int32_t                  rand_seed;                          /* Seed for unvoiced signal generation                              */
        int16_t                  randScale_Q14;                      /* Scaling of unvoiced random signal                                */
        int32_t                  conc_energy;
        int32_t                  conc_energy_shift;
        int16_t                  prevLTP_scale_Q14;
        int32_t                  prevGain_Q16[ 2 ];
        int32_t                  fs_kHz;
        int32_t                  nb_subfr;
        int32_t                  subfr_length;
    } silk_PLC_struct_t;

/* Struct for CNG */
    typedef struct {
        int32_t                  CNG_exc_buf_Q14[ MAX_FRAME_LENGTH ];
        int16_t                  CNG_smth_NLSF_Q15[ MAX_LPC_ORDER ];
        int32_t                  CNG_synth_state[ MAX_LPC_ORDER ];
        int32_t                  CNG_smth_Gain_Q16;
        int32_t                  rand_seed;
        int32_t                  fs_kHz;
    } silk_CNG_struct_t;

    typedef struct {
        int32_t prev_gain_Q16;
        int32_t exc_Q14[MAX_FRAME_LENGTH];
        int32_t sLPC_Q14_buf[MAX_LPC_ORDER];
        int16_t outBuf[MAX_FRAME_LENGTH + 2 * MAX_SUB_FRAME_LENGTH]; /* Buffer for output signal                    */
        int32_t lagPrev;                        /* Previous Lag                                                     */
        int8_t LastGainIndex;                   /* Previous gain index                                              */
        int32_t fs_kHz;                         /* Sampling frequency in kHz                                        */
        int32_t fs_API_hz;                      /* API sample frequency (Hz)                                        */
        int32_t nb_subfr;                       /* Number of 5 ms subframes in a frame                              */
        int32_t frame_length;                   /* Frame length (samples)                                           */
        int32_t subfr_length;                   /* Subframe length (samples)                                        */
        int32_t ltp_mem_length;                 /* Length of LTP memory                                             */
        int32_t LPC_order;                      /* LPC order                                                        */
        int16_t prevNLSF_Q15[MAX_LPC_ORDER];    /* Used to interpolate LSFs                                         */
        int32_t first_frame_after_reset;        /* Flag for deactivating NLSF interpolation                         */
        const uint8_t *pitch_lag_low_bits_iCDF; /* Pointer to iCDF table for low bits of pitch lag index            */
        const uint8_t *pitch_contour_iCDF;      /* Pointer to iCDF table for pitch contour index                    */
        /* For buffering payload in case of more frames per packet */
        int32_t nFramesDecoded;
        int32_t nFramesPerPacket;
        /* Specifically for entropy coding */
        int32_t ec_prevSignalType;
        int16_t ec_prevLagIndex;
        int32_t VAD_flags[MAX_FRAMES_PER_PACKET];
        int32_t LBRR_flag;
        int32_t LBRR_flags[MAX_FRAMES_PER_PACKET];
        const silk_NLSF_CB_struct_t *psNLSF_CB; /* Pointer to NLSF codebook                                         */
        sideInfoIndices_t indices; /* Quantization indices */
        silk_CNG_struct_t sCNG; /* CNG state */
        int32_t lossCnt; /* Stuff used for PLC */
        int32_t prevSignalType;
        silk_PLC_struct_t sPLC;
    } silk_decoder_state_t;

    typedef struct {
        int32_t pitchL[MAX_NB_SUBFR]; /* Prediction and coding parameters */
        int32_t Gains_Q16[MAX_NB_SUBFR];
        int16_t PredCoef_Q12[2][MAX_LPC_ORDER]; /* Holds interpolated and final coefficients, 4-byte aligned */
        int16_t LTPCoef_Q14[LTP_ORDER * MAX_NB_SUBFR];
        int32_t LTP_scale_Q14;
    } silk_decoder_control_t;

/* Decoder Super Struct */
    typedef struct {
        stereo_dec_state_t              sStereo;
        int32_t                         nChannelsAPI;
        int32_t                         nChannelsInternal;
        int32_t                         prev_decode_only_middle;
    } silk_decoder_t;

    typedef struct {
        int32_t sLPC_Q14[MAX_SUB_FRAME_LENGTH + NSQ_LPC_BUF_LENGTH];
        int32_t RandState[DECISION_DELAY];
        int32_t Q_Q10[DECISION_DELAY];
        int32_t Xq_Q14[DECISION_DELAY];
        int32_t Pred_Q15[DECISION_DELAY];
        int32_t Shape_Q14[DECISION_DELAY];
        int32_t sAR2_Q14[MAX_SHAPE_LPC_ORDER];
        int32_t LF_AR_Q14;
        int32_t Diff_Q14;
        int32_t Seed;
        int32_t SeedInit;
        int32_t RD_Q10;
    } NSQ_del_dec_struct;

    typedef struct {
        int32_t Q_Q10;
        int32_t RD_Q10;
        int32_t xq_Q14;
        int32_t LF_AR_Q14;
        int32_t Diff_Q14;
        int32_t sLTP_shp_Q14;
        int32_t LPC_exc_Q14;
    } NSQ_sample_struct;

    typedef NSQ_sample_struct NSQ_sample_pair[2];

    typedef struct {
        int32_t nChannelsAPI; /* I:   Number of channels; 1/2 */
        int32_t nChannelsInternal; /* I:   Number of channels; 1/2 */
        int32_t API_sampleRate; /* I:   Output signal sampling rate in Hertz; 8000/12000/16000/24000/32000/44100/48000  */
        int32_t internalSampleRate; /* I:   Internal sampling rate used, in Hertz; 8000/12000/16000                         */
        int32_t payloadSize_ms; /* I:   Number of samples per packet in milliseconds; 10/20/40/60 */
        int32_t prevPitchLag; /* O:   Pitch lag of previous frame (0 if unvoiced), measured in samples at 48 kHz */
    } silk_DecControlStruct_t;

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

    const int16_t HARM_ATT_Q15[NB_ATT] = {32440, 31130};              /* 0.99, 0.95 */
    const int16_t PLC_RAND_ATTENUATE_V_Q15[NB_ATT] = {31130, 26214};  /* 0.95, 0.8 */
    const int16_t PLC_RAND_ATTENUATE_UV_Q15[NB_ATT] = {32440, 29491}; /* 0.99, 0.9 */


    /* Cosine approximation table for LSF conversion */
    /* Q12 values (even) */
    const int16_t silk_LSFCosTab_FIX_Q12[LSF_COS_TAB_SZ_FIX + 1] = {
        8192,  8190,  8182,  8170,  8152,  8130,  8104,  8072,  8034,  7994,  7946,  7896,  7840,  7778,  7714,  7644,  7568,  7490,  7406,  7318,  7226,  7128,  7026,  6922,  6812,  6698,
        6580,  6458,  6332,  6204,  6070,  5934,  5792,  5648,  5502,  5352,  5198,  5040,  4880,  4718,  4552,  4382,  4212,  4038,  3862,  3684,  3502,  3320,  3136,  2948,  2760,  2570,
        2378,  2186,  1990,  1794,  1598,  1400,  1202,  1002,  802,   602,   402,   202,   0,     -202,  -402,  -602,  -802,  -1002, -1202, -1400, -1598, -1794, -1990, -2186, -2378, -2570,
        -2760, -2948, -3136, -3320, -3502, -3684, -3862, -4038, -4212, -4382, -4552, -4718, -4880, -5040, -5198, -5352, -5502, -5648, -5792, -5934, -6070, -6204, -6332, -6458, -6580, -6698,
        -6812, -6922, -7026, -7128, -7226, -7318, -7406, -7490, -7568, -7644, -7714, -7778, -7840, -7896, -7946, -7994, -8034, -8072, -8104, -8130, -8152, -8170, -8182, -8190, -8192};

    /* Tables for stereo predictor coding */
    const int16_t silk_stereo_pred_quant_Q13[STEREO_QUANT_TAB_SIZE] = {-13732, -10050, -8266, -7526, -6500, -5000, -2950, -820, 820, 2950, 5000, 6500, 7526, 8266, 10050, 13732};
    const uint8_t silk_stereo_pred_joint_iCDF[25] = {249, 247, 246, 245, 244, 234, 210, 202, 201, 200, 197, 174, 82, 59, 56, 55, 54, 46, 22, 12, 11, 10, 9, 7, 0};
    const uint8_t silk_stereo_only_code_mid_iCDF[2] = {64, 0};

    /* Tables for LBRR flags */
    const uint8_t silk_LBRR_flags_2_iCDF[3] = {203, 150, 0};
    const uint8_t silk_LBRR_flags_3_iCDF[7] = {215, 195, 166, 125, 110, 82, 0};
    const uint8_t* const silk_LBRR_flags_iCDF_ptr[2] = {silk_LBRR_flags_2_iCDF, silk_LBRR_flags_3_iCDF};

    /* Table for LSB coding */
    const uint8_t silk_lsb_iCDF[2] = {120, 0};

    /* Tables for LTPScale */
    const uint8_t silk_LTPscale_iCDF[3] = {128, 64, 0};

    /* Tables for signal type and offset coding */
    const uint8_t silk_type_offset_VAD_iCDF[4] = {232, 158, 10, 0};
    const uint8_t silk_type_offset_no_VAD_iCDF[2] = {230, 0};

    /* Tables for NLSF interpolation factor */
    const uint8_t silk_NLSF_interpolation_factor_iCDF[5] = {243, 221, 192, 181, 0};

    /* Quantization offsets */
    const int16_t silk_Quantization_Offsets_Q10[2][2] = {{OFFSET_UVL_Q10, OFFSET_UVH_Q10}, {OFFSET_VL_Q10, OFFSET_VH_Q10}};

    /* Table for LTPScale */
    const int16_t silk_LTPScales_table_Q14[3] = {15565, 12288, 8192};

    /* Uniform entropy tables */
    const uint8_t silk_uniform3_iCDF[3] = {171, 85, 0};
    const uint8_t silk_uniform4_iCDF[4] = {192, 128, 64, 0};
    const uint8_t silk_uniform5_iCDF[5] = {205, 154, 102, 51, 0};
    const uint8_t silk_uniform6_iCDF[6] = {213, 171, 128, 85, 43, 0};
    const uint8_t silk_uniform8_iCDF[8] = {224, 192, 160, 128, 96, 64, 32, 0};

    const uint8_t silk_NLSF_EXT_iCDF[7] = {100, 40, 16, 7, 3, 1, 0};

    /*  Elliptic/Cauer filters designed with 0.1 dB passband ripple,
            80 dB minimum stopband attenuation, and
            [0.95 : 0.15 : 0.35] normalized cut off frequencies. */

    /* Interpolation points for filter coefficients used in the bandwidth transition smoother */
    const int32_t silk_Transition_LP_B_Q28[TRANSITION_INT_NUM][TRANSITION_NB] = {
        {250767114, 501534038, 250767114}, {209867381, 419732057, 209867381}, {170987846, 341967853, 170987846}, {131531482, 263046905, 131531482}, {89306658, 178584282, 89306658}};

    /* Interpolation points for filter coefficients used in the bandwidth transition smoother */
    const int32_t silk_Transition_LP_A_Q28[TRANSITION_INT_NUM][TRANSITION_NA] = {{506393414, 239854379}, {411067935, 169683996}, {306733530, 116694253}, {185807084, 77959395}, {35497197, 57401098}};

    const uint8_t silk_max_pulses_table[4] = {8, 10, 12, 16};

    const uint8_t silk_pulses_per_block_iCDF[10][18] = {{125, 51, 26, 18, 15, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
                                                        {198, 105, 45, 22, 15, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
                                                        {213, 162, 116, 83, 59, 43, 32, 24, 18, 15, 12, 9, 7, 6, 5, 3, 2, 0},
                                                        {239, 187, 116, 59, 28, 16, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
                                                        {250, 229, 188, 135, 86, 51, 30, 19, 13, 10, 8, 6, 5, 4, 3, 2, 1, 0},
                                                        {249, 235, 213, 185, 156, 128, 103, 83, 66, 53, 42, 33, 26, 21, 17, 13, 10, 0},
                                                        {254, 249, 235, 206, 164, 118, 77, 46, 27, 16, 10, 7, 5, 4, 3, 2, 1, 0},
                                                        {255, 253, 249, 239, 220, 191, 156, 119, 85, 57, 37, 23, 15, 10, 6, 4, 2, 0},
                                                        {255, 253, 251, 246, 237, 223, 203, 179, 152, 124, 98, 75, 55, 40, 29, 21, 15, 0},
                                                        {255, 254, 253, 247, 220, 162, 106, 67, 42, 28, 18, 12, 9, 6, 4, 3, 2, 0}};

    const uint8_t silk_rate_levels_iCDF[2][9] = {{241, 190, 178, 132, 87, 74, 41, 14, 0}, {223, 193, 157, 140, 106, 57, 39, 18, 0}};

    const uint8_t silk_rate_levels_BITS_Q5[2][9] = {{131, 74, 141, 79, 80, 138, 95, 104, 134}, {95, 99, 91, 125, 93, 76, 123, 115, 123}};

    const uint8_t silk_shell_code_table0[152] = {128, 0,   214, 42,  0,   235, 128, 21,  0,   244, 184, 72,  11,  0,   248, 214, 128, 42,  7,   0,   248, 225, 170, 80,  25,  5,   0,   251, 236, 198, 126,
                                                 54,  18,  3,   0,   250, 238, 211, 159, 82,  35,  15,  5,   0,   250, 231, 203, 168, 128, 88,  53,  25,  6,   0,   252, 238, 216, 185, 148, 108, 71,  40,
                                                 18,  4,   0,   253, 243, 225, 199, 166, 128, 90,  57,  31,  13,  3,   0,   254, 246, 233, 212, 183, 147, 109, 73,  44,  23,  10,  2,   0,   255, 250, 240,
                                                 223, 198, 166, 128, 90,  58,  33,  16,  6,   1,   0,   255, 251, 244, 231, 210, 181, 146, 110, 75,  46,  25,  12,  5,   1,   0,   255, 253, 248, 238, 221,
                                                 196, 164, 128, 92,  60,  35,  18,  8,   3,   1,   0,   255, 253, 249, 242, 229, 208, 180, 146, 110, 76,  48,  27,  14,  7,   3,   1,   0};

    const uint8_t silk_shell_code_table1[152] = {129, 0,   207, 50,  0,   236, 129, 20,  0,   245, 185, 72,  10,  0,   249, 213, 129, 42,  6,   0,   250, 226, 169, 87,  27,  4,   0,   251, 233, 194, 130,
                                                 62,  20,  4,   0,   250, 236, 207, 160, 99,  47,  17,  3,   0,   255, 240, 217, 182, 131, 81,  41,  11,  1,   0,   255, 254, 233, 201, 159, 107, 61,  20,
                                                 2,   1,   0,   255, 249, 233, 206, 170, 128, 86,  50,  23,  7,   1,   0,   255, 250, 238, 217, 186, 148, 108, 70,  39,  18,  6,   1,   0,   255, 252, 243,
                                                 226, 200, 166, 128, 90,  56,  30,  13,  4,   1,   0,   255, 252, 245, 231, 209, 180, 146, 110, 76,  47,  25,  11,  4,   1,   0,   255, 253, 248, 237, 219,
                                                 194, 163, 128, 93,  62,  37,  19,  8,   3,   1,   0,   255, 254, 250, 241, 226, 205, 177, 145, 111, 79,  51,  30,  15,  6,   2,   1,   0};

    const uint8_t silk_shell_code_table2[152] = {129, 0,   203, 54,  0,   234, 129, 23,  0,   245, 184, 73,  10,  0,   250, 215, 129, 41,  5,   0,   252, 232, 173, 86,  24,  3,   0,   253, 240, 200, 129,
                                                 56,  15,  2,   0,   253, 244, 217, 164, 94,  38,  10,  1,   0,   253, 245, 226, 189, 132, 71,  27,  7,   1,   0,   253, 246, 231, 203, 159, 105, 56,  23,
                                                 6,   1,   0,   255, 248, 235, 213, 179, 133, 85,  47,  19,  5,   1,   0,   255, 254, 243, 221, 194, 159, 117, 70,  37,  12,  2,   1,   0,   255, 254, 248,
                                                 234, 208, 171, 128, 85,  48,  22,  8,   2,   1,   0,   255, 254, 250, 240, 220, 189, 149, 107, 67,  36,  16,  6,   2,   1,   0,   255, 254, 251, 243, 227,
                                                 201, 166, 128, 90,  55,  29,  13,  5,   2,   1,   0,   255, 254, 252, 246, 234, 213, 183, 147, 109, 73,  43,  22,  10,  4,   2,   1,   0};

    const uint8_t silk_shell_code_table3[152] = {130, 0,   200, 58,  0,   231, 130, 26,  0,   244, 184, 76,  12,  0,   249, 214, 130, 43,  6,   0,   252, 232, 173, 87,  24,  3,   0,   253, 241, 203, 131,
                                                 56,  14,  2,   0,   254, 246, 221, 167, 94,  35,  8,   1,   0,   254, 249, 232, 193, 130, 65,  23,  5,   1,   0,   255, 251, 239, 211, 162, 99,  45,  15,
                                                 4,   1,   0,   255, 251, 243, 223, 186, 131, 74,  33,  11,  3,   1,   0,   255, 252, 245, 230, 202, 158, 105, 57,  24,  8,   2,   1,   0,   255, 253, 247,
                                                 235, 214, 179, 132, 84,  44,  19,  7,   2,   1,   0,   255, 254, 250, 240, 223, 196, 159, 112, 69,  36,  15,  6,   2,   1,   0,   255, 254, 253, 245, 231,
                                                 209, 176, 136, 93,  55,  27,  11,  3,   2,   1,   0,   255, 254, 253, 252, 239, 221, 194, 158, 117, 76,  42,  18,  4,   3,   2,   1,   0};

    const uint8_t silk_shell_code_table_offsets[17] = {0, 0, 2, 5, 9, 14, 20, 27, 35, 44, 54, 65, 77, 90, 104, 119, 135};

    const uint8_t silk_sign_iCDF[42] = {254, 49, 67, 77, 82, 93, 99, 198, 11, 18,  24,  31,  36,  45,  255, 46, 66, 78, 87, 94, 104,
                                        208, 14, 21, 32, 42, 51, 66, 255, 94, 104, 109, 112, 115, 118, 248, 53, 69, 80, 88, 95, 102};

    const uint8_t silk_NLSF_CB1_NB_MB_Q8[320] = {
        12, 35, 60, 83, 108, 132, 157, 180, 206, 228, 15, 32, 55, 77,  101, 125, 151, 175, 201, 225, 19, 42, 66, 89,  114, 137, 162, 184, 209, 230, 12, 25, 50, 72, 97,  120, 147, 172, 200, 223,
        26, 44, 69, 90, 114, 135, 159, 180, 205, 225, 13, 22, 53, 80,  106, 130, 156, 180, 205, 228, 15, 25, 44, 64,  90,  115, 142, 168, 196, 222, 19, 24, 62, 82, 100, 120, 145, 168, 190, 214,
        22, 31, 50, 79, 103, 120, 151, 170, 203, 227, 21, 29, 45, 65,  106, 124, 150, 171, 196, 224, 30, 49, 75, 97,  121, 142, 165, 186, 209, 229, 19, 25, 52, 70, 93,  116, 143, 166, 192, 219,
        26, 34, 62, 75, 97,  118, 145, 167, 194, 217, 25, 33, 56, 70,  91,  113, 143, 165, 196, 223, 21, 34, 51, 72,  97,  117, 145, 171, 196, 222, 20, 29, 50, 67, 90,  117, 144, 168, 197, 221,
        22, 31, 48, 66, 95,  117, 146, 168, 196, 222, 24, 33, 51, 77,  116, 134, 158, 180, 200, 224, 21, 28, 70, 87,  106, 124, 149, 170, 194, 217, 26, 33, 53, 64, 83,  117, 152, 173, 204, 225,
        27, 34, 65, 95, 108, 129, 155, 174, 210, 225, 20, 26, 72, 99,  113, 131, 154, 176, 200, 219, 34, 43, 61, 78,  93,  114, 155, 177, 205, 229, 23, 29, 54, 97, 124, 138, 163, 179, 209, 229,
        30, 38, 56, 89, 118, 129, 158, 178, 200, 231, 21, 29, 49, 63,  85,  111, 142, 163, 193, 222, 27, 48, 77, 103, 133, 158, 179, 196, 215, 232, 29, 47, 74, 99, 124, 151, 176, 198, 220, 237,
        33, 42, 61, 76, 93,  121, 155, 174, 207, 225, 29, 53, 87, 112, 136, 154, 170, 188, 208, 227, 24, 30, 52, 84,  131, 150, 166, 186, 203, 229, 37, 48, 64, 84, 104, 118, 156, 177, 201, 230};

    const int16_t silk_NLSF_CB1_Wght_Q9[320] = {
        2897, 2314, 2314, 2314, 2287, 2287, 2314, 2300, 2327, 2287, 2888, 2580, 2394, 2367, 2314, 2274, 2274, 2274, 2274, 2194, 2487, 2340, 2340, 2314, 2314, 2314, 2340, 2340, 2367, 2354, 3216, 2766,
        2340, 2340, 2314, 2274, 2221, 2207, 2261, 2194, 2460, 2474, 2367, 2394, 2394, 2394, 2394, 2367, 2407, 2314, 3479, 3056, 2127, 2207, 2274, 2274, 2274, 2287, 2314, 2261, 3282, 3141, 2580, 2394,
        2247, 2221, 2207, 2194, 2194, 2114, 4096, 3845, 2221, 2620, 2620, 2407, 2314, 2394, 2367, 2074, 3178, 3244, 2367, 2221, 2553, 2434, 2340, 2314, 2167, 2221, 3338, 3488, 2726, 2194, 2261, 2460,
        2354, 2367, 2207, 2101, 2354, 2420, 2327, 2367, 2394, 2420, 2420, 2420, 2460, 2367, 3779, 3629, 2434, 2527, 2367, 2274, 2274, 2300, 2207, 2048, 3254, 3225, 2713, 2846, 2447, 2327, 2300, 2300,
        2274, 2127, 3263, 3300, 2753, 2806, 2447, 2261, 2261, 2247, 2127, 2101, 2873, 2981, 2633, 2367, 2407, 2354, 2194, 2247, 2247, 2114, 3225, 3197, 2633, 2580, 2274, 2181, 2247, 2221, 2221, 2141,
        3178, 3310, 2740, 2407, 2274, 2274, 2274, 2287, 2194, 2114, 3141, 3272, 2460, 2061, 2287, 2500, 2367, 2487, 2434, 2181, 3507, 3282, 2314, 2700, 2647, 2474, 2367, 2394, 2340, 2127, 3423, 3535,
        3038, 3056, 2300, 1950, 2221, 2274, 2274, 2274, 3404, 3366, 2087, 2687, 2873, 2354, 2420, 2274, 2474, 2540, 3760, 3488, 1950, 2660, 2897, 2527, 2394, 2367, 2460, 2261, 3028, 3272, 2740, 2888,
        2740, 2154, 2127, 2287, 2234, 2247, 3695, 3657, 2025, 1969, 2660, 2700, 2580, 2500, 2327, 2367, 3207, 3413, 2354, 2074, 2888, 2888, 2340, 2487, 2247, 2167, 3338, 3366, 2846, 2780, 2327, 2154,
        2274, 2287, 2114, 2061, 2327, 2300, 2181, 2167, 2181, 2367, 2633, 2700, 2700, 2553, 2407, 2434, 2221, 2261, 2221, 2221, 2340, 2420, 2607, 2700, 3038, 3244, 2806, 2888, 2474, 2074, 2300, 2314,
        2354, 2380, 2221, 2154, 2127, 2287, 2500, 2793, 2793, 2620, 2580, 2367, 3676, 3713, 2234, 1838, 2181, 2753, 2726, 2673, 2513, 2207, 2793, 3160, 2726, 2553, 2846, 2513, 2181, 2394, 2221, 2181};

    const uint8_t silk_NLSF_CB1_iCDF_NB_MB[64] = {212, 178, 148, 129, 108, 96,  85,  82,  79,  77,  61,  59,  57,  56,  51,  49,  48,  45, 42, 41, 40, 38, 36, 34, 31, 30, 21, 12, 10, 3,  1, 0,
                                                         255, 245, 244, 236, 233, 225, 217, 203, 190, 176, 175, 161, 149, 136, 125, 114, 102, 91, 81, 71, 60, 52, 43, 35, 28, 20, 19, 18, 12, 11, 5, 0};

    const uint8_t silk_NLSF_CB2_SELECT_NB_MB[160] = {16,  0,   0,   0,   0,   99,  66,  36,  36,  34,  36,  34,  34,  34,  34,  83,  69,  36,  52,  34,  116, 102, 70,  68,  68,  176, 102,
                                                            68,  68,  34,  65,  85,  68,  84,  36,  116, 141, 152, 139, 170, 132, 187, 184, 216, 137, 132, 249, 168, 185, 139, 104, 102, 100, 68,
                                                            68,  178, 218, 185, 185, 170, 244, 216, 187, 187, 170, 244, 187, 187, 219, 138, 103, 155, 184, 185, 137, 116, 183, 155, 152, 136, 132,
                                                            217, 184, 184, 170, 164, 217, 171, 155, 139, 244, 169, 184, 185, 170, 164, 216, 223, 218, 138, 214, 143, 188, 218, 168, 244, 141, 136,
                                                            155, 170, 168, 138, 220, 219, 139, 164, 219, 202, 216, 137, 168, 186, 246, 185, 139, 116, 185, 219, 185, 138, 100, 100, 134, 100, 102,
                                                            34,  68,  68,  100, 68,  168, 203, 221, 218, 168, 167, 154, 136, 104, 70,  164, 246, 171, 137, 139, 137, 155, 218, 219, 139};

    const uint8_t silk_NLSF_CB2_iCDF_NB_MB[72] = {255, 254, 253, 238, 14, 3, 2, 1, 0, 255, 254, 252, 218, 35, 3,  2, 1, 0, 255, 254, 250, 208, 59, 4,  2, 1, 0, 255, 254, 246, 194, 71, 10, 2, 1, 0,
                                                         255, 252, 236, 183, 82, 8, 2, 1, 0, 255, 252, 235, 180, 90, 17, 2, 1, 0, 255, 248, 224, 171, 97, 30, 4, 1, 0, 255, 254, 236, 173, 95, 37, 7, 1, 0};

    const uint8_t silk_NLSF_CB2_BITS_NB_MB_Q5[72] = {255, 255, 255, 131, 6,   145, 255, 255, 255, 255, 255, 236, 93,  15,  96,  255, 255, 255, 255, 255, 194, 83,  25,  71,
                                                            221, 255, 255, 255, 255, 162, 73,  34,  66,  162, 255, 255, 255, 210, 126, 73,  43,  57,  173, 255, 255, 255, 201, 125,
                                                            71,  48,  58,  130, 255, 255, 255, 166, 110, 73,  57,  62,  104, 210, 255, 255, 251, 123, 65,  55,  68,  100, 171, 255};

    const uint8_t silk_NLSF_PRED_NB_MB_Q8[18] = {179, 138, 140, 148, 151, 149, 153, 151, 163, 116, 67, 82, 59, 92, 72, 100, 89, 92};

    const int16_t silk_NLSF_DELTA_MIN_NB_MB_Q15[11] = {250, 3, 6, 3, 3, 3, 4, 3, 3, 3, 461};

    const uint8_t silk_gain_iCDF[3][N_LEVELS_QGAIN / 8] = {{224, 112, 44, 15, 3, 2, 1, 0}, {254, 237, 192, 132, 70, 23, 4, 0}, {255, 252, 226, 155, 61, 11, 2, 0}};

    const uint8_t silk_delta_gain_iCDF[MAX_DELTA_GAIN_QUANT - MIN_DELTA_GAIN_QUANT + 1] = {250, 245, 234, 203, 71, 50, 42, 38, 35, 33, 31, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20,
                                                                                           19,  18,  17,  16,  15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};

    const uint8_t silk_pitch_lag_iCDF[2 * (PITCH_EST_MAX_LAG_MS - PITCH_EST_MIN_LAG_MS)] = {253, 250, 244, 233, 212, 182, 150, 131, 120, 110, 98, 85, 72, 60, 49, 40,
                                                                                            32,  25,  19,  15,  13,  11,  9,   8,   7,   6,   5,  4,  3,  2,  1,  0};

    const uint8_t silk_pitch_delta_iCDF[21] = {210, 208, 206, 203, 199, 193, 183, 168, 142, 104, 74, 52, 37, 27, 20, 14, 10, 6, 4, 2, 0};

    const uint8_t silk_pitch_contour_iCDF[34] = {223, 201, 183, 167, 152, 138, 124, 111, 98, 88, 79, 70, 62, 56, 50, 44, 39, 35, 31, 27, 24, 21, 18, 16, 14, 12, 10, 8, 6, 4, 3, 2, 1, 0};

    const uint8_t silk_pitch_contour_NB_iCDF[11] = {188, 176, 155, 138, 119, 97, 67, 43, 26, 10, 0};

    const uint8_t silk_pitch_contour_10_ms_iCDF[12] = {165, 119, 80, 61, 47, 35, 27, 20, 14, 9, 4, 0};

    const uint8_t silk_pitch_contour_10_ms_NB_iCDF[3] = {113, 63, 0};

    const uint8_t silk_LTP_per_index_iCDF[3] = {179, 99, 0};

    const uint8_t silk_LTP_gain_iCDF_0[8] = {71, 56, 43, 30, 21, 12, 6, 0};

    const uint8_t silk_LTP_gain_iCDF_1[16] = {199, 165, 144, 124, 109, 96, 84, 71, 61, 51, 42, 32, 23, 15, 8, 0};

    const uint8_t silk_LTP_gain_iCDF_2[32] = {241, 225, 211, 199, 187, 175, 164, 153, 142, 132, 123, 114, 105, 96, 88, 80, 72, 64, 57, 50, 44, 38, 33, 29, 24, 20, 16, 12, 9, 5, 2, 0};

    const uint8_t silk_LTP_gain_BITS_Q5_0[8] = {15, 131, 138, 138, 155, 155, 173, 173};

    const uint8_t silk_LTP_gain_BITS_Q5_1[16] = {69, 93, 115, 118, 131, 138, 141, 138, 150, 150, 155, 150, 155, 160, 166, 160};

    const uint8_t silk_LTP_gain_BITS_Q5_2[32] = {131, 128, 134, 141, 141, 141, 145, 145, 145, 150, 155, 155, 155, 155, 160, 160,
                                                        160, 160, 166, 166, 173, 173, 182, 192, 182, 192, 192, 192, 205, 192, 205, 224};

    const uint8_t* const silk_LTP_gain_iCDF_ptrs[NB_LTP_CBKS] = {silk_LTP_gain_iCDF_0, silk_LTP_gain_iCDF_1, silk_LTP_gain_iCDF_2};

    const uint8_t* const silk_LTP_gain_BITS_Q5_ptrs[NB_LTP_CBKS] = {silk_LTP_gain_BITS_Q5_0, silk_LTP_gain_BITS_Q5_1, silk_LTP_gain_BITS_Q5_2};

    const int8_t silk_LTP_gain_vq_0[8][5] = {{4, 6, 24, 7, 5},    {0, 0, 2, 0, 0},      {12, 28, 41, 13, -4}, {-9, 15, 42, 25, 14},
                                                    {1, -2, 62, 41, -9}, {-10, 37, 65, -4, 3}, {-6, 4, 66, 7, -8},   {16, 14, 38, -3, 33}};

    const int8_t silk_LTP_gain_vq_1[16][5] = {{13, 22, 39, 23, 12}, {-1, 36, 64, 27, -6}, {-7, 10, 55, 43, 17}, {1, 1, 8, 1, 1},     {6, -11, 74, 53, -9}, {-12, 55, 76, -12, 8},
                                                     {-3, 3, 93, 27, -4},  {26, 39, 59, 3, -8},  {2, 0, 77, 11, 9},    {-8, 22, 44, -6, 7}, {40, 9, 26, 3, 9},    {-7, 20, 101, -7, 4},
                                                     {3, -8, 42, 26, 0},   {-15, 33, 68, 2, 23}, {-2, 55, 46, -2, 15}, {3, -1, 21, 16, 41}};

    const int8_t silk_LTP_gain_vq_2[32][5] = {
        {-6, 27, 61, 39, 5},   {-11, 42, 88, 4, 1},    {-2, 60, 65, 6, -4},  {-1, -5, 73, 56, 1},    {-9, 19, 94, 29, -9},  {0, 12, 99, 6, 4},      {8, -19, 102, 46, -13}, {3, 2, 13, 3, 2},
        {9, -21, 84, 72, -18}, {-11, 46, 104, -22, 8}, {18, 38, 48, 23, 0},  {-16, 70, 83, -21, 11}, {5, -11, 117, 22, -8}, {-6, 23, 117, -12, 3},  {3, -8, 95, 28, 4},     {-10, 15, 77, 60, -15},
        {-1, 4, 124, 2, -4},   {3, 38, 84, 24, -25},   {2, 13, 42, 13, 31},  {21, -4, 56, 46, -1},   {-1, 35, 79, -13, 19}, {-7, 65, 88, -9, -14},  {20, 4, 81, 49, -29},   {20, 0, 75, 3, -17},
        {5, -9, 44, 92, -8},   {1, -3, 22, 69, 31},    {-6, 95, 41, -12, 5}, {39, 67, 16, -4, 1},    {0, -6, 120, 55, -36}, {-13, 44, 122, 4, -24}, {81, 5, 11, 3, 7},      {2, 0, 9, 10, 88}};

    const uint8_t silk_NLSF_CB1_WB_Q8[512] = {
        7,   23,  38,  54,  69,  85,  100, 116, 131, 147, 162, 178, 193, 208, 223, 239, 13,  25,  41,  55,  69,  83,  98,  112, 127, 142, 157, 171, 187, 203, 220, 236, 15,  21,  34,  51,  61,
        78,  92,  106, 126, 136, 152, 167, 185, 205, 225, 240, 10,  21,  36,  50,  63,  79,  95,  110, 126, 141, 157, 173, 189, 205, 221, 237, 17,  20,  37,  51,  59,  78,  89,  107, 123, 134,
        150, 164, 184, 205, 224, 240, 10,  15,  32,  51,  67,  81,  96,  112, 129, 142, 158, 173, 189, 204, 220, 236, 8,   21,  37,  51,  65,  79,  98,  113, 126, 138, 155, 168, 179, 192, 209,
        218, 12,  15,  34,  55,  63,  78,  87,  108, 118, 131, 148, 167, 185, 203, 219, 236, 16,  19,  32,  36,  56,  79,  91,  108, 118, 136, 154, 171, 186, 204, 220, 237, 11,  28,  43,  58,
        74,  89,  105, 120, 135, 150, 165, 180, 196, 211, 226, 241, 6,   16,  33,  46,  60,  75,  92,  107, 123, 137, 156, 169, 185, 199, 214, 225, 11,  19,  30,  44,  57,  74,  89,  105, 121,
        135, 152, 169, 186, 202, 218, 234, 12,  19,  29,  46,  57,  71,  88,  100, 120, 132, 148, 165, 182, 199, 216, 233, 17,  23,  35,  46,  56,  77,  92,  106, 123, 134, 152, 167, 185, 204,
        222, 237, 14,  17,  45,  53,  63,  75,  89,  107, 115, 132, 151, 171, 188, 206, 221, 240, 9,   16,  29,  40,  56,  71,  88,  103, 119, 137, 154, 171, 189, 205, 222, 237, 16,  19,  36,
        48,  57,  76,  87,  105, 118, 132, 150, 167, 185, 202, 218, 236, 12,  17,  29,  54,  71,  81,  94,  104, 126, 136, 149, 164, 182, 201, 221, 237, 15,  28,  47,  62,  79,  97,  115, 129,
        142, 155, 168, 180, 194, 208, 223, 238, 8,   14,  30,  45,  62,  78,  94,  111, 127, 143, 159, 175, 192, 207, 223, 239, 17,  30,  49,  62,  79,  92,  107, 119, 132, 145, 160, 174, 190,
        204, 220, 235, 14,  19,  36,  45,  61,  76,  91,  108, 121, 138, 154, 172, 189, 205, 222, 238, 12,  18,  31,  45,  60,  76,  91,  107, 123, 138, 154, 171, 187, 204, 221, 236, 13,  17,
        31,  43,  53,  70,  83,  103, 114, 131, 149, 167, 185, 203, 220, 237, 17,  22,  35,  42,  58,  78,  93,  110, 125, 139, 155, 170, 188, 206, 224, 240, 8,   15,  34,  50,  67,  83,  99,
        115, 131, 146, 162, 178, 193, 209, 224, 239, 13,  16,  41,  66,  73,  86,  95,  111, 128, 137, 150, 163, 183, 206, 225, 241, 17,  25,  37,  52,  63,  75,  92,  102, 119, 132, 144, 160,
        175, 191, 212, 231, 19,  31,  49,  65,  83,  100, 117, 133, 147, 161, 174, 187, 200, 213, 227, 242, 18,  31,  52,  68,  88,  103, 117, 126, 138, 149, 163, 177, 192, 207, 223, 239, 16,
        29,  47,  61,  76,  90,  106, 119, 133, 147, 161, 176, 193, 209, 224, 240, 15,  21,  35,  50,  61,  73,  86,  97,  110, 119, 129, 141, 175, 198, 218, 237};

    const int16_t silk_NLSF_CB1_WB_Wght_Q9[512] = {
        3657, 2925, 2925, 2925, 2925, 2925, 2925, 2925, 2925, 2925, 2925, 2925, 2963, 2963, 2925, 2846, 3216, 3085, 2972, 3056, 3056, 3010, 3010, 3010, 2963, 2963, 3010, 2972, 2888, 2846, 2846, 2726,
        3920, 4014, 2981, 3207, 3207, 2934, 3056, 2846, 3122, 3244, 2925, 2846, 2620, 2553, 2780, 2925, 3516, 3197, 3010, 3103, 3019, 2888, 2925, 2925, 2925, 2925, 2888, 2888, 2888, 2888, 2888, 2753,
        5054, 5054, 2934, 3573, 3385, 3056, 3085, 2793, 3160, 3160, 2972, 2846, 2513, 2540, 2753, 2888, 4428, 4149, 2700, 2753, 2972, 3010, 2925, 2846, 2981, 3019, 2925, 2925, 2925, 2925, 2888, 2726,
        3620, 3019, 2972, 3056, 3056, 2873, 2806, 3056, 3216, 3047, 2981, 3291, 3291, 2981, 3310, 2991, 5227, 5014, 2540, 3338, 3526, 3385, 3197, 3094, 3376, 2981, 2700, 2647, 2687, 2793, 2846, 2673,
        5081, 5174, 4615, 4428, 2460, 2897, 3047, 3207, 3169, 2687, 2740, 2888, 2846, 2793, 2846, 2700, 3122, 2888, 2963, 2925, 2925, 2925, 2925, 2963, 2963, 2963, 2963, 2925, 2925, 2963, 2963, 2963,
        4202, 3207, 2981, 3103, 3010, 2888, 2888, 2925, 2972, 2873, 2916, 3019, 2972, 3010, 3197, 2873, 3760, 3760, 3244, 3103, 2981, 2888, 2925, 2888, 2972, 2934, 2793, 2793, 2846, 2888, 2888, 2660,
        3854, 4014, 3207, 3122, 3244, 2934, 3047, 2963, 2963, 3085, 2846, 2793, 2793, 2793, 2793, 2580, 3845, 4080, 3357, 3516, 3094, 2740, 3010, 2934, 3122, 3085, 2846, 2846, 2647, 2647, 2846, 2806,
        5147, 4894, 3225, 3845, 3441, 3169, 2897, 3413, 3451, 2700, 2580, 2673, 2740, 2846, 2806, 2753, 4109, 3789, 3291, 3160, 2925, 2888, 2888, 2925, 2793, 2740, 2793, 2740, 2793, 2846, 2888, 2806,
        5081, 5054, 3047, 3545, 3244, 3056, 3085, 2944, 3103, 2897, 2740, 2740, 2740, 2846, 2793, 2620, 4309, 4309, 2860, 2527, 3207, 3376, 3376, 3075, 3075, 3376, 3056, 2846, 2647, 2580, 2726, 2753,
        3056, 2916, 2806, 2888, 2740, 2687, 2897, 3103, 3150, 3150, 3216, 3169, 3056, 3010, 2963, 2846, 4375, 3882, 2925, 2888, 2846, 2888, 2846, 2846, 2888, 2888, 2888, 2846, 2888, 2925, 2888, 2846,
        2981, 2916, 2916, 2981, 2981, 3056, 3122, 3216, 3150, 3056, 3010, 2972, 2972, 2972, 2925, 2740, 4229, 4149, 3310, 3347, 2925, 2963, 2888, 2981, 2981, 2846, 2793, 2740, 2846, 2846, 2846, 2793,
        4080, 4014, 3103, 3010, 2925, 2925, 2925, 2888, 2925, 2925, 2846, 2846, 2846, 2793, 2888, 2780, 4615, 4575, 3169, 3441, 3207, 2981, 2897, 3038, 3122, 2740, 2687, 2687, 2687, 2740, 2793, 2700,
        4149, 4269, 3789, 3657, 2726, 2780, 2888, 2888, 3010, 2972, 2925, 2846, 2687, 2687, 2793, 2888, 4215, 3554, 2753, 2846, 2846, 2888, 2888, 2888, 2925, 2925, 2888, 2925, 2925, 2925, 2963, 2888,
        5174, 4921, 2261, 3432, 3789, 3479, 3347, 2846, 3310, 3479, 3150, 2897, 2460, 2487, 2753, 2925, 3451, 3685, 3122, 3197, 3357, 3047, 3207, 3207, 2981, 3216, 3085, 2925, 2925, 2687, 2540, 2434,
        2981, 3010, 2793, 2793, 2740, 2793, 2846, 2972, 3056, 3103, 3150, 3150, 3150, 3103, 3010, 3010, 2944, 2873, 2687, 2726, 2780, 3010, 3432, 3545, 3357, 3244, 3056, 3010, 2963, 2925, 2888, 2846,
        3019, 2944, 2897, 3010, 3010, 2972, 3019, 3103, 3056, 3056, 3010, 2888, 2846, 2925, 2925, 2888, 3920, 3967, 3010, 3197, 3357, 3216, 3291, 3291, 3479, 3704, 3441, 2726, 2181, 2460, 2580, 2607};

    const uint8_t silk_NLSF_CB1_iCDF_WB[64] = {225, 204, 201, 184, 183, 175, 158, 154, 153, 135, 119, 115, 113, 110, 109, 99,  98, 95, 79, 68, 52, 50, 48, 45, 43, 32, 31, 27, 18, 10, 3, 0,
                                                      255, 251, 235, 230, 212, 201, 196, 182, 167, 166, 163, 151, 138, 124, 110, 104, 90, 78, 76, 70, 69, 57, 45, 34, 24, 21, 11, 6,  5,  4,  3, 0};

    const uint8_t silk_NLSF_CB2_SELECT_WB[256] = {
        0,   0,   0,   0,   0,   0,   0,   1,   100, 102, 102, 68,  68,  36,  34,  96,  164, 107, 158, 185, 180, 185, 139, 102, 64,  66,  36,  34,  34,  0,   1,   32,  208, 139, 141, 191, 152,
        185, 155, 104, 96,  171, 104, 166, 102, 102, 102, 132, 1,   0,   0,   0,   0,   16,  16,  0,   80,  109, 78,  107, 185, 139, 103, 101, 208, 212, 141, 139, 173, 153, 123, 103, 36,  0,
        0,   0,   0,   0,   0,   1,   48,  0,   0,   0,   0,   0,   0,   32,  68,  135, 123, 119, 119, 103, 69,  98,  68,  103, 120, 118, 118, 102, 71,  98,  134, 136, 157, 184, 182, 153, 139,
        134, 208, 168, 248, 75,  189, 143, 121, 107, 32,  49,  34,  34,  34,  0,   17,  2,   210, 235, 139, 123, 185, 137, 105, 134, 98,  135, 104, 182, 100, 183, 171, 134, 100, 70,  68,  70,
        66,  66,  34,  131, 64,  166, 102, 68,  36,  2,   1,   0,   134, 166, 102, 68,  34,  34,  66,  132, 212, 246, 158, 139, 107, 107, 87,  102, 100, 219, 125, 122, 137, 118, 103, 132, 114,
        135, 137, 105, 171, 106, 50,  34,  164, 214, 141, 143, 185, 151, 121, 103, 192, 34,  0,   0,   0,   0,   0,   1,   208, 109, 74,  187, 134, 249, 159, 137, 102, 110, 154, 118, 87,  101,
        119, 101, 0,   2,   0,   36,  36,  66,  68,  35,  96,  164, 102, 100, 36,  0,   2,   33,  167, 138, 174, 102, 100, 84,  2,   2,   100, 107, 120, 119, 36,  197, 24,  0};

    const uint8_t silk_NLSF_CB2_iCDF_WB[72] = {255, 254, 253, 244, 12, 3, 2, 1, 0, 255, 254, 252, 224, 38, 3,  2, 1, 0, 255, 254, 251, 209, 57, 4,  2, 1, 0, 255, 254, 244, 195, 69,  4,  2, 1, 0,
                                                      255, 251, 232, 184, 84, 7, 2, 1, 0, 255, 254, 240, 186, 86, 14, 2, 1, 0, 255, 254, 239, 178, 91, 30, 5, 1, 0, 255, 248, 227, 177, 100, 19, 2, 1, 0};

    const uint8_t silk_NLSF_CB2_BITS_WB_Q5[72] = {255, 255, 255, 156, 4,   154, 255, 255, 255, 255, 255, 227, 102, 15,  92,  255, 255, 255, 255, 255, 213, 83,  24,  72,
                                                         236, 255, 255, 255, 255, 150, 76,  33,  63,  214, 255, 255, 255, 190, 121, 77,  43,  55,  185, 255, 255, 255, 245, 137,
                                                         71,  43,  59,  139, 255, 255, 255, 255, 131, 66,  50,  66,  107, 194, 255, 255, 166, 116, 76,  55,  53,  125, 255, 255};

    const uint8_t silk_NLSF_PRED_WB_Q8[30] = {175, 148, 160, 176, 178, 173, 174, 164, 177, 174, 196, 182, 198, 192, 182, 68, 62, 66, 60, 72, 117, 85, 90, 118, 136, 151, 142, 160, 142, 155};

    const int16_t silk_NLSF_DELTA_MIN_WB_Q15[17] = {100, 3, 40, 3, 3, 3, 5, 14, 14, 10, 11, 3, 8, 9, 7, 3, 347};

    const int8_t silk_CB_lags_stage2_10_ms[PE_MAX_NB_SUBFR >> 1][PE_NB_CBKS_STAGE2_10MS] = {{0, 1, 0}, {0, 0, 1}};

    const int8_t silk_CB_lags_stage3_10_ms[PE_MAX_NB_SUBFR >> 1][PE_NB_CBKS_STAGE3_10MS] = {{0, 0, 1, -1, 1, -1, 2, -2, 2, -2, 3, -3}, {0, 1, 0, 1, -1, 2, -1, 2, -2, 3, -2, 3}};

    const int8_t silk_Lag_range_stage3_10_ms[PE_MAX_NB_SUBFR >> 1][2] = {{-3, 7}, {-2, 7}};

    const int8_t silk_CB_lags_stage2[PE_MAX_NB_SUBFR][PE_NB_CBKS_STAGE2_EXT] = {
        {0, 2, -1, -1, -1, 0, 0, 1, 1, 0, 1}, {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0}, {0, -1, 2, 1, 0, 1, 1, 0, 0, -1, -1}};

    const int8_t silk_CB_lags_stage3[PE_MAX_NB_SUBFR][PE_NB_CBKS_STAGE3_MAX] = {{0, 0, 1, -1, 0, 1, -1, 0, -1, 1, -2, 2, -2, -2, 2, -3, 2, 3, -3, -4, 3, -4, 4, 4, -5, 5, -6, -5, 6, -7, 6, 5, 8, -9},
                                                                                {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 0, 1, -1, 0, 1, -1, -1, 1, -1, 2, 1, -1, 2, -2, -2, 2, -2, 2, 2, 3, -3},
                                                                                {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, -1, 1, 0, 0, 2, 1, -1, 2, -1, -1, 2, -1, 2, 2, -1, 3, -2, -2, -2, 3},
                                                                                {0, 1, 0, 0, 1, 0, 1, -1, 2, -1, 2, -1, 2, 3, -2, 3, -2, -2, 4, 4, -3, 5, -3, -4, 6, -4, 6, 5, -5, 8, -6, -5, -7, 9}};

    const int8_t silk_Lag_range_stage3[SILK_PE_MAX_COMPLEX + 1][PE_MAX_NB_SUBFR][2] = {
        /* Lags to search for low number of stage3 cbks */
        {{-5, 8}, {-1, 6}, {-1, 6}, {-4, 10}},
        /* Lags to search for middle number of stage3 cbks */
        {{-6, 10}, {-2, 6}, {-1, 6}, {-5, 10}},
        /* Lags to search for max number of stage3 cbks */
        {{-9, 12}, {-3, 7}, {-2, 7}, {-7, 13}}};

    /* Tables with delay compensation values to equalize total delay for different modes */
    const int8_t delay_matrix_enc[5][3] = {
        /* in  \ out  8  12  16 */
        /*  8 */ {6, 0, 3},
        /* 12 */ {0, 7, 3},
        /* 16 */ {0, 1, 10},
        /* 24 */ {0, 2, 6},
        /* 48 */ {18, 10, 12}};

    const int8_t delay_matrix_dec[3][5] = {
        /* in  \ out  8  12  16  24  48 */
        /*  8 */ {4, 0, 2, 0, 0},
        /* 12 */ {0, 9, 4, 7, 4},
        /* 16 */ {0, 3, 12, 7, 7}};

    /* Tables with IIR and FIR coefficients for fractional downsamplers (123 Words) */
    const int16_t silk_Resampler_3_4_COEFS[2 + 3 * RESAMPLER_DOWN_ORDER_FIR0 / 2] = {
        -20694, -13867, -49, 64, 17, -157, 353, -496, 163, 11047, 22205, -39, 6, 91, -170, 186, 23, -896, 6336, 19928, -19, -36, 102, -89, -24, 328, -951, 2568, 15909,
    };

    const int16_t silk_Resampler_2_3_COEFS[2 + 2 * RESAMPLER_DOWN_ORDER_FIR0 / 2] = {
        -14457, -14019, 64, 128, -122, 36, 310, -768, 584, 9267, 17733, 12, 128, 18, -142, 288, -117, -865, 4123, 14459,
    };

    const int16_t silk_Resampler_1_2_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR1 / 2] = {
        616, -14323, -10, 39, 58, -46, -84, 120, 184, -315, -541, 1284, 5380, 9024,
    };

    const int16_t silk_Resampler_1_3_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR2 / 2] = {
        16102, -15162, -13, 0, 20, 26, 5, -31, -43, -4, 65, 90, 7, -157, -248, -44, 593, 1583, 2612, 3271,
    };

    const int16_t silk_Resampler_1_4_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR2 / 2] = {
        22500, -15099, 3, -14, -20, -15, 2, 25, 37, 25, -16, -71, -107, -79, 50, 292, 623, 982, 1288, 1464,
    };

    const int16_t silk_Resampler_1_6_COEFS[2 + RESAMPLER_DOWN_ORDER_FIR2 / 2] = {
        27540, -15257, 17, 12, 8, 1, -10, -22, -30, -32, -22, 3, 44, 100, 168, 243, 317, 381, 429, 455,
    };

    const int16_t silk_Resampler_2_3_COEFS_LQ[2 + 2 * 2] = {
        -2797, -6507, 4697, 10739, 1567, 8276,
    };

    /* Table with interplation fractions of 1/24, 3/24, 5/24, ... , 23/24 : 23/24 (46 Words) */
    const int16_t silk_resampler_frac_FIR_12[12][RESAMPLER_ORDER_FIR_12 / 2] = {
        {189, -600, 617, 30567},  {117, -159, -1070, 29704}, {52, 221, -2392, 28276},   {-4, 529, -3350, 26341}, {-48, 758, -3956, 23973}, {-80, 905, -4235, 21254},
        {-99, 972, -4222, 18278}, {-107, 967, -3957, 15143}, {-103, 896, -3487, 11950}, {-91, 773, -2865, 8798}, {-71, 611, -2143, 5784},  {-46, 425, -1375, 2996},
    };

    /* Tables for 2x downsampler */
    const int16_t silk_resampler_down2_0 = 9872;
    const int16_t silk_resampler_down2_1 = 39809 - 65536;

    /* Tables for 2x upsampler, high quality */
    const int16_t silk_resampler_up2_hq_0[3] = {1746, 14986, 39083 - 65536};
    const int16_t silk_resampler_up2_hq_1[3] = {6854, 25769, 55542 - 65536};

    /* fprintf(1, '%d, ', round(1024 * ([1 ./ (1 + exp(-(1:5))), 1] - 1 ./ (1 + exp(-(0:5)))))); */
    const int32_t sigm_LUT_slope_Q10[6] = {237, 153, 73, 30, 12, 7};
    /* fprintf(1, '%d, ', round(32767 * 1 ./ (1 + exp(-(0:5))))); */
    const int32_t sigm_LUT_pos_Q15[6] = {16384, 23955, 28861, 31213, 32178, 32548};
    /* fprintf(1, '%d, ', round(32767 * 1 ./ (1 + exp((0:5))))); */
    const int32_t sigm_LUT_neg_Q15[6] = {16384, 8812, 3906, 1554, 589, 219};

    const int8_t silk_nb_cbk_searchs_stage3[SILK_PE_MAX_COMPLEX + 1] = {PE_NB_CBKS_STAGE3_MIN, PE_NB_CBKS_STAGE3_MID, PE_NB_CBKS_STAGE3_MAX};

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
    void silk_resampler_down2_3(int32_t *S, int16_t *out, const int16_t *in, int32_t inLen);
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