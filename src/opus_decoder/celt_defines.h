#pragma once

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
