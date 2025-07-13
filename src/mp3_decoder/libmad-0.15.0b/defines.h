

#pragma once

#define MAD_VERSION_MAJOR 0
#define MAD_VERSION_MINOR 15
#define MAD_VERSION_PATCH 0
#define MAD_VERSION_EXTRA " (beta)"

#define MAD_F_FRACBITS  28
#define MAD_F_SCALEBITS         MAD_F_FRACBITS
#define FPM_64BIT
#define MAD_F(x)                ((int64_t)(x##LL))
#define MAD_F_MIN               ((int32_t)-0x80000000L)
#define MAD_F_MAX               ((int32_t)+0x7fffffffL)
#define MAD_F_MLN(hi, lo)       ((lo) = -(lo))
#define MAD_F_MLZ(hi, lo)       ((void)(hi), (int32_t)(lo))
#define mad_f_mul(x, y)         ((int32_t)(((int64_t)(x) * (y)) >> MAD_F_SCALEBITS))
#define MAD_F_ML0(hi, lo, x, y) ((lo) = mad_f_mul((x), (y)))
#define MAD_F_MLA(hi, lo, x, y) ((lo) += mad_f_mul((x), (y)))
#define MUL(x, y) mad_f_mul((x), (y))
#define CRC_POLY  0x8005

#define SHIFT(x)          ((x))
#define ML0(hi, lo, x, y) MAD_F_ML0((hi), (lo), (x), (y))
#define MLA(hi, lo, x, y) MAD_F_MLA((hi), (lo), (x), (y))
#define MLN(hi, lo)       MAD_F_MLN((hi), (lo))
#define MLZ(hi, lo)       MAD_F_MLZ((hi), (lo))
#define PRESHIFT(x)       (MAD_F(x) >> 12)

#define MASK(cache, sz, bits) (((cache) >> ((sz) - (bits))) & ((1 << (bits)) - 1))
#define MASK1BIT(cache, sz)   ((cache) & (1 << ((sz) - 1)))

#define mad_f_mul_16(x, y)         ((int32_t)(((int64_t)(x) * (y)) >> (MAD_F_FRACBITS - 12)))
#define MAD_F_ML0_16(hi, lo, x, y) ((lo) = mad_f_mul_16((x), (y)))
#define MAD_F_MLA_16(hi, lo, x, y) ((lo) += mad_f_mul_16((x), (y)))
#define ML0_16(hi, lo, x, y)       MAD_F_ML0_16((hi), (lo), (x), (y))
#define MLA_16(hi, lo, x, y)       MAD_F_MLA_16((hi), (lo), (x), (y))


#define mad_bit_finish(bitptr)		/* nothing */
#define sfb_16000_long sfb_22050_long
#define sfb_12000_long sfb_16000_long
#define sfb_11025_long sfb_12000_long
#define sfb_12000_short sfb_16000_short
#define sfb_11025_short sfb_12000_short
#define sfb_12000_mixed sfb_16000_mixed
#define sfb_11025_mixed sfb_12000_mixed

# if defined(OPT_SPEED) && defined(OPT_ACCURACY)
#  error "cannot optimize for both speed and accuracy"
# endif

#define MAD_BUFFER_GUARD	8
#define MAD_BUFFER_MDLEN	(511 + 2048 + MAD_BUFFER_GUARD)
#define mad_bit_bitsleft(bitptr)  ((bitptr)->left)
#define MAD_TIMER_RESOLUTION	352800000UL
#define MAD_RECOVERABLE(error) ((error) & 0xff00)
#define MAD_NCHANNELS(header)  ((header)->mode ? 2 : 1)
#define MAD_NSBSAMPLES(header) ((header)->layer == MAD_LAYER_I ? 12 : (((header)->layer == MAD_LAYER_III && ((header)->flags & MAD_FLAG_LSF_EXT)) ? 18 : 36))
#define mad_header_finish(header)  /* nothing */

#define MAD_F_BITS  28  /* number of fraction bits */
#define MAD_F_SCALE (1L << MAD_F_BITS)
#define MAD_F_ONE   (1LL << MAD_F_BITS) // Value 1.0 in fixed-point format
