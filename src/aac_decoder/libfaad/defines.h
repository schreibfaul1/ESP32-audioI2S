#pragma once
#include "Arduino.h"
#include "settings.h"

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* defines if an object type can be decoded by this library or not */
__unused static uint8_t ObjectTypesTable[32] = {
    0, /*  0 NULL */
#ifdef MAIN_DEC
    1, /*  1 AAC Main */
#else
    0, /*  1 AAC Main */
#endif
    1, /*  2 AAC LC */
#ifdef SSR_DEC
    1, /*  3 AAC SSR */
#else
    0, /*  3 AAC SSR */
#endif
#ifdef LTP_DEC
    1, /*  4 AAC LTP */
#else
    0, /*  4 AAC LTP */
#endif
#ifdef SBR_DEC
    1, /*  5 SBR */
#else
    0, /*  5 SBR */
#endif
    0, /*  6 AAC Scalable */
    0, /*  7 TwinVQ */
    0, /*  8 CELP */
    0, /*  9 HVXC */
    0, /* 10 Reserved */
    0, /* 11 Reserved */
    0, /* 12 TTSI */
    0, /* 13 Main synthetic */
    0, /* 14 Wavetable synthesis */
    0, /* 15 General MIDI */
    0, /* 16 Algorithmic Synthesis and Audio FX */
/* MPEG-4 Version 2 */
#ifdef ERROR_RESILIENCE
    1, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    #ifdef LTP_DEC
    1, /* 19 ER AAC LTP */
    #else
    0, /* 19 ER AAC LTP */
    #endif
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    #ifdef LD_DEC
    1, /* 23 ER AAC LD */
    #else
    0, /* 23 ER AAC LD */
    #endif
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#else  /* No ER defined */
    0, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    0, /* 19 ER AAC LTP */
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    0, /* 23 ER AAC LD */
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#endif
    0, /* 28 (Reserved) */
#ifdef PS_DEC
    1, /* 29 AAC LC + SBR + PS */
#else
    0, /* 29 AAC LC + SBR + PS */
#endif
    0, /* 30 (Reserved) */
    0  /* 31 (Reserved) */
};
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define ZERO_HCB              0
#define FIRST_PAIR_HCB        5
#define ESC_HCB               11
#define QUAD_LEN              4
#define PAIR_LEN              2
#define NOISE_HCB             13
#define INTENSITY_HCB2        14
#define INTENSITY_HCB         15
#define DRC_REF_LEVEL         20 * 4 /* -20 dB */
#define DRM_PARAMETRIC_STEREO 0
#define DRM_NUM_SA_BANDS      8
#define DRM_NUM_PAN_BANDS     20
#define NUM_OF_LINKS          3
#define NUM_OF_QMF_CHANNELS   64
#define NUM_OF_SUBSAMPLES     30
#define MAX_SA_BAND           46
#define MAX_PAN_BAND          64
#define MAX_DELAY             5
#define EXTENSION_ID_PS       2
#define MAX_PS_ENVELOPES      5
#define NO_ALLPASS_LINKS      3
#define BYTE_NUMBIT           8
#define BYTE_NUMBIT_LD        3
#define bit2byte(a)           ((a + 7) >> BYTE_NUMBIT_LD)
#define NUM_ERROR_MESSAGES    34
#define ESC_VAL               7
#define SSR_BANDS             4
#define PQFTAPS               96

#ifdef DRM
    #define DECAY_CUTOFF 3
    #define DECAY_SLOPE  0.05f
/* type definitaions */
typedef const int8_t (*drm_ps_huff_tab)[2];
#endif

#define FLOAT_SCALE   (1.0f / (1 << 15))
#define DM_MUL        REAL_CONST(0.3203772410170407)    // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2        REAL_CONST(0.7071067811865475244) // 1/sqrt(2)
#define NUM_CB        6
#define NUM_CB_ER     22
#define MAX_CB        32
#define VCB11_FIRST   16
#define VCB11_LAST    31
#define TNS_MAX_ORDER 20
#define MAIN          1
#define LC            2
#define SSR           3
#define LTP           4
#define HE_AAC        5
#define LD            23
#define ER_LC         17
#define ER_LTP        19
#define DRM_ER_LC     27 /* special object type for DRM */
/* header types */
#define RAW  0
#define ADIF 1
#define ADTS 2
#define LATM 3
/* SBR signalling */
#define NO_SBR           0
#define SBR_UPSAMPLED    1
#define SBR_DOWNSAMPLED  2
#define NO_SBR_UPSAMPLED 3
/* DRM channel definitions */
#define DRMCH_MONO          1
#define DRMCH_STEREO        2
#define DRMCH_SBR_MONO      3
#define DRMCH_SBR_STEREO    4
#define DRMCH_SBR_PS_STEREO 5
/* First object type that has ER */
#define ER_OBJECT_START 17
/* Bitstream */
#define LEN_SE_ID         3
#define LEN_TAG           4
#define LEN_BYTE          8
#define EXT_FIL           0
#define EXT_FILL_DATA     1
#define EXT_DATA_ELEMENT  2
#define EXT_DYNAMIC_RANGE 11
#define ANC_DATA          0
/* Syntax elements */
#define ID_SCE               0x0
#define ID_CPE               0x1
#define ID_CCE               0x2
#define ID_LFE               0x3
#define ID_DSE               0x4
#define ID_PCE               0x5
#define ID_FIL               0x6
#define ID_END               0x7
#define INVALID_ELEMENT_ID   255
#define ONLY_LONG_SEQUENCE   0x0
#define LONG_START_SEQUENCE  0x1
#define EIGHT_SHORT_SEQUENCE 0x2
#define LONG_STOP_SEQUENCE   0x3
#define ZERO_HCB             0
#define FIRST_PAIR_HCB       5
#define ESC_HCB              11
#define QUAD_LEN             4
#define PAIR_LEN             2
#define NOISE_HCB            13
#define INTENSITY_HCB2       14
#define INTENSITY_HCB        15
#define INVALID_SBR_ELEMENT  255
#define T_HFGEN              8
#define T_HFADJ              2
#define EXT_SBR_DATA         13
#define EXT_SBR_DATA_CRC     14
#define FIXFIX               0
#define FIXVAR               1
#define VARFIX               2
#define VARVAR               3
#define LO_RES               0
#define HI_RES               1
#define NO_TIME_SLOTS_960    15
#define NO_TIME_SLOTS        16
#define RATE                 2
#define NOISE_FLOOR_OFFSET   6

#ifdef PS_DEC
    #define NEGATE_IPD_MASK (0x1000)
    #define DECAY_SLOPE     FRAC_CONST(0.05)
    #define COEF_SQRT2      COEF_CONST(1.4142135623731)
#endif //  PS_DEC

#define MAX_NTSRHFG 40 /* MAX_NTSRHFG: maximum of number_time_slots * rate + HFGen. 16*2+8 */
#define MAX_NTSR    32 /* max number_time_slots * rate, ok for DRM and not DRM mode */
#define MAX_M       49 /* MAX_M: maximum value for M */
#define MAX_L_E     5  /* MAX_L_E: maximum value for L_E */

#ifdef SBR_DEC
    #ifdef FIXED_POINT
        #define _EPS (1) /* smallest number available in fixed point */
    #else
        #define _EPS (1e-12)
    #endif
#endif // SBR_DEC

#ifdef FIXED_POINT /* int32_t */
    #define LOG2_MIN_INF   REAL_CONST(-10000)
    #define COEF_BITS      28
    #define COEF_PRECISION (1 << COEF_BITS)
    #define REAL_BITS      14 // MAXIMUM OF 14 FOR FIXED POINT SBR
    #define REAL_PRECISION (1 << REAL_BITS)
    /* FRAC is the fractional only part of the fixed point number [0.0..1.0) */
    #define FRAC_SIZE      32 /* frac is a 32 bit integer */
    #define FRAC_BITS      31
    #define FRAC_PRECISION ((uint32_t)(1 << FRAC_BITS))
    #define FRAC_MAX       0x7FFFFFFF
typedef int32_t real_t;
    #define REAL_CONST(A) (((A) >= 0) ? ((real_t)((A) * (REAL_PRECISION) + 0.5)) : ((real_t)((A) * (REAL_PRECISION) - 0.5)))
    #define COEF_CONST(A) (((A) >= 0) ? ((real_t)((A) * (COEF_PRECISION) + 0.5)) : ((real_t)((A) * (COEF_PRECISION) - 0.5)))
    #define FRAC_CONST(A) (((A) == 1.00) ? ((real_t)FRAC_MAX) : (((A) >= 0) ? ((real_t)((A) * (FRAC_PRECISION) + 0.5)) : ((real_t)((A) * (FRAC_PRECISION) - 0.5))))
    // #define FRAC_CONST(A) (((A) >= 0) ? ((real_t)((A)*(FRAC_PRECISION)+0.5)) : ((real_t)((A)*(FRAC_PRECISION)-0.5)))
    #define Q2_BITS      22
    #define Q2_PRECISION (1 << Q2_BITS)
    #define Q2_CONST(A)  (((A) >= 0) ? ((real_t)((A) * (Q2_PRECISION) + 0.5)) : ((real_t)((A) * (Q2_PRECISION) - 0.5)))
    /* multiply with real shift */
    #define MUL_R(A, B) (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (REAL_BITS - 1))) >> REAL_BITS)
    /* multiply with coef shift */
    #define MUL_C(A, B) (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (COEF_BITS - 1))) >> COEF_BITS)
    /* multiply with fractional shift */
    #define _MulHigh(A, B)    (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (FRAC_SIZE - 1))) >> FRAC_SIZE)
    #define MUL_F(A, B)       (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (FRAC_BITS - 1))) >> FRAC_BITS)
    #define MUL_Q2(A, B)      (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (Q2_BITS - 1))) >> Q2_BITS)
    #define MUL_SHIFT6(A, B)  (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (6 - 1))) >> 6)
    #define MUL_SHIFT23(A, B) (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (23 - 1))) >> 23)
    #define DIV_R(A, B)       (((int64_t)A << REAL_BITS) / B)
    #define DIV_C(A, B)       (((int64_t)A << COEF_BITS) / B)
/*    Complex multiplication */
static inline void ComplexMult(real_t* y1, real_t* y2, real_t x1, real_t x2, real_t c1, real_t c2) { // FIXED POINT
    *y1 = (_MulHigh(x1, c1) + _MulHigh(x2, c2)) << (FRAC_SIZE - FRAC_BITS);
    *y2 = (_MulHigh(x2, c1) - _MulHigh(x1, c2)) << (FRAC_SIZE - FRAC_BITS);
}
// static inline void ComplexMult(int32_t* y1, int32_t* y2, int32_t x1, int32_t x2, int32_t c1, int32_t c2) { // only XTENSA chips
//     asm volatile (
//         //  y1 = (x1 * c1) + (x2 * c2)
//         "mulsh a2, %2, %4\n"        // a2 = x1 * c1 (Low 32 bits)
//         "mulsh a3, %3, %5\n"        // a3 = x2 * c2 (Low 32 bits)
//         "add   a2, a2, a3\n"        // a2 = (x1 * c1) + (x2 * c2)
//         "slli  a2, a2,  1\n"        // a2 = a2 >> 31 (Fixed-Point scaling)
//         "s32i  a2, %0   \n"         // Store result in *y1
//         // y2 = (x2 * c1) - (x1 * c2)
//         "mulsh a2, %3, %4\n"        // a2 = x2 * c1 (Low 32 bits)
//         "mulsh a3, %2, %5\n"        // a3 = x1 * c2 (Low 32 bits)
//         "sub   a2, a2, a3\n"        // a2 = (x2 * c1) - (x1 * c2)
//         "slli  a2, a2,  1\n"        // a2 = a2 >> 31 (Fixed-Point scaling)
//         "s32i  a2, %1    \n"        // Store result in *y2
//         : "=m" (*y1), "=m" (*y2)                  // Output
//         : "r" (x1), "r" (x2), "r" (c1), "r" (c2)  // Input
//         : "a2", "a3"                              // Clobbers
//     );
// }

    #define DIV(A, B) (((int64_t)A << REAL_BITS) / B)
    #define step(shift)                                  \
        if ((0x40000000l >> shift) + root <= value) {    \
            value -= (0x40000000l >> shift) + root;      \
            root = (root >> 1) | (0x40000000l >> shift); \
        } else {                                         \
            root = root >> 1;                            \
        }

real_t const pow2_table[] = {COEF_CONST(1.0), COEF_CONST(1.18920711500272), COEF_CONST(1.41421356237310), COEF_CONST(1.68179283050743)};
#endif // FIXED_POINT
#ifndef FIXED_POINT
    #ifdef MAIN_DEC
        #define ALPHA REAL_CONST(0.90625)
        #define A     REAL_CONST(0.953125)
    #endif
    #define IQ_TABLE_SIZE 8192
    #define DIV_R(A, B)   ((A) / (B))
    #define DIV_C(A, B)   ((A) / (B))
    #ifdef USE_DOUBLE_PRECISION /* double */
typedef double real_t;
        #include <math.h>
        #define MUL_R(A, B)   ((A) * (B))
        #define MUL_C(A, B)   ((A) * (B))
        #define MUL_F(A, B)   ((A) * (B))
        #define REAL_CONST(A) ((real_t)(A))
        #define COEF_CONST(A) ((real_t)(A))
        #define Q2_CONST(A)   ((real_t)(A))
        #define FRAC_CONST(A) ((real_t)(A)) /* pure fractional part */
/* Complex multiplication */
static void ComplexMult(real_t* y1, real_t* y2, real_t x1, real_t x2, real_t c1, real_t c2) {
    *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
    *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
}
    #else /* Normal floating point operation */
typedef float real_t;
        #define MUL_R(A, B)   ((A) * (B))
        #define MUL_C(A, B)   ((A) * (B))
        #define MUL_F(A, B)   ((A) * (B))
        #define REAL_CONST(A) ((real_t)(A))
        #define COEF_CONST(A) ((real_t)(A))
        #define Q2_CONST(A)   ((real_t)(A))
        #define FRAC_CONST(A) ((real_t)(A)) /* pure fractional part */
/* Complex multiplication */
__unused static void ComplexMult(real_t* y1, real_t* y2, real_t x1, real_t x2, real_t c1, real_t c2) {
    *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
    *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
}

    #endif /* USE_DOUBLE_PRECISION */
#endif     // FIXED_POINT

#ifdef SBR_LOW_POWER
    #define qmf_t     real_t
    #define QMF_RE(A) (A)
    #define QMF_IM(A)
#else
    #define qmf_t     complex_t
    #define QMF_RE(A) RE(A)
    #define QMF_IM(A) IM(A)
#endif
typedef real_t complex_t[2];
#define RE(A) A[0]
#define IM(A) A[1]

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#if !defined(max) && !defined(__cplusplus)
    #define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min) && !defined(__cplusplus)
    #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef FAAD2_VERSION
    #define FAAD2_VERSION "unknown"
#endif
/* object types for AAC */
#define MAIN      1
#define LC        2
#define SSR       3
#define LTP       4
#define HE_AAC    5
#define ER_LC     17
#define ER_LTP    19
#define LD        23
#define DRM_ER_LC 27 /* special object type for DRM */
/* header types */
#define RAW  0
#define ADIF 1
#define ADTS 2
#define LATM 3
/* SBR signalling */
#define NO_SBR           0
#define SBR_UPSAMPLED    1
#define SBR_DOWNSAMPLED  2
#define NO_SBR_UPSAMPLED 3
/* library output formats */
#define FAAD_FMT_16BIT  1
#define FAAD_FMT_24BIT  2
#define FAAD_FMT_32BIT  3
#define FAAD_FMT_FLOAT  4
#define FAAD_FMT_FIXED  FAAD_FMT_FLOAT
#define FAAD_FMT_DOUBLE 5
/* Capabilities */
#define LC_DEC_CAP           (1 << 0) /* Can decode LC */
#define MAIN_DEC_CAP         (1 << 1) /* Can decode MAIN */
#define LTP_DEC_CAP          (1 << 2) /* Can decode LTP */
#define LD_DEC_CAP           (1 << 3) /* Can decode LD */
#define ERROR_RESILIENCE_CAP (1 << 4) /* Can decode ER */
#define FIXED_POINT_CAP      (1 << 5) /* Fixed point */
/* Channel definitions */
#define FRONT_CHANNEL_CENTER (1)
#define FRONT_CHANNEL_LEFT   (2)
#define FRONT_CHANNEL_RIGHT  (3)
#define SIDE_CHANNEL_LEFT    (4)
#define SIDE_CHANNEL_RIGHT   (5)
#define BACK_CHANNEL_LEFT    (6)
#define BACK_CHANNEL_RIGHT   (7)
#define BACK_CHANNEL_CENTER  (8)
#define LFE_CHANNEL          (9)
#define UNKNOWN_CHANNEL      (0)
/* DRM channel definitions */
#define DRMCH_MONO          1
#define DRMCH_STEREO        2
#define DRMCH_SBR_MONO      3
#define DRMCH_SBR_STEREO    4
#define DRMCH_SBR_PS_STEREO 5
/* A decode call can eat up to FAAD_MIN_STREAMSIZE bytes per decoded channel,
   so at least so much bytes per channel should be available in this stream */
#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */

#define MAX_CHANNELS        64
#define MAX_SYNTAX_ELEMENTS 48
#define MAX_WINDOW_GROUPS   8
#define MAX_SFB             51
#define MAX_LTP_SFB         40
#define MAX_LTP_SFB_S       8
#define MAX_ASC_BYTES       64

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
    #ifndef HAS_LRINTF
        #define CLIP(sample, max, min)           \
            if (sample >= 0.0f) {                \
                sample += 0.5f;                  \
                if (sample >= max) sample = max; \
            } else {                             \
                sample += -0.5f;                 \
                if (sample <= min) sample = min; \
            }
    #else
        #define CLIP(sample, max, min)           \
            if (sample >= 0.0f) {                \
                if (sample >= max) sample = max; \
            } else {                             \
                if (sample <= min) sample = min; \
            }
    #endif
    #define CONV(a, b) ((a << 1) | (b & 0x1))
#endif
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
