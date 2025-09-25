#pragma once

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
