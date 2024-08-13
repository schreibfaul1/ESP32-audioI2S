/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: neaacdec.h,v 1.14 2012/03/02 15:29:47 knik Exp $
**/

#pragma once

#ifdef ESP32
#include "Arduino.h"
#endif
#pragma GCC optimize ("Ofast")

#include <inttypes.h>
#include <math.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

//------------------------------------------------------
//  MEMORY USAGE without SBR ~19KB
//                  with SBR and PS ~59KB
//                  with SBR no PS  ~35KB
//------------------------------------------------------

/* COMPILE TIME DEFINITIONS */
#define MAIN_DEC /* Allow decoding of MAIN profile AAC */
#define PREFER_POINTERS
#define ERROR_RESILIENCE
#define LTP_DEC /* Allow decoding of LTP (long term prediction) profile AAC */
#define LD_DEC  /* Allow decoding of LD (low delay) profile AAC */
#define ALLOW_SMALL_FRAMELENGTH
#if (defined CONFIG_IDF_TARGET_ESP32S3 && defined BOARD_HAS_PSRAM)
    #define SBR_DEC /* Allow decoding of SBR (spectral band replication) */
    #define PS_DEC /* Allow decoding of PS (parametric stereo) */
#endif
#define FIXED_POINT  // must be defined!!
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* LD can't do without LTP */
#ifdef LD_DEC
    #ifndef ERROR_RESILIENCE
        #define ERROR_RESILIENCE
    #endif
    #ifndef LTP_DEC
        #define LTP_DEC
    #endif
#endif

typedef int32_t      complex_t[2];
typedef void*        NeAACDecHandle;
typedef const int8_t (*ps_huff_tab)[2];
typedef const int8_t (*sbr_huff_tab)[2];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define MAIN                 1 /* object types for AAC */
#define LC                   2
#define SSR                  3
#define LTP                  4
#define HE_AAC               5
#define ER_LC                17
#define ER_LTP               19
#define LD                   23
#define RAW                  0 /* header types */
#define ADIF                 1
#define ADTS                 2
#define LATM                 3
#define NO_SBR               0 /* SBR signalling */
#define SBR_UPSAMPLED        1
#define SBR_DOWNSAMPLED      2
#define NO_SBR_UPSAMPLED     3
#define FAAD_FMT_16BIT       1 /* library output formats */
#define FAAD_FMT_24BIT       2
#define FAAD_FMT_32BIT       3
#define FAAD_FMT_FLOAT       4
#define FAAD_FMT_FIXED       FAAD_FMT_FLOAT
#define FAAD_FMT_DOUBLE      5
#define LC_DEC_CAP           (1 << 0) /* Can decode LC */
#define MAIN_DEC_CAP         (1 << 1) /* Can decode MAIN */
#define LTP_DEC_CAP          (1 << 2) /* Can decode LTP */
#define LD_DEC_CAP           (1 << 3) /* Can decode LD */
#define ERROR_RESILIENCE_CAP (1 << 4) /* Can decode ER */
#define FIXED_POINT_CAP      (1 << 5) /* Fixed point */
#define FRONT_CHANNEL_CENTER (1)      /* Channel definitions */
#define FRONT_CHANNEL_LEFT   (2)
#define FRONT_CHANNEL_RIGHT  (3)
#define SIDE_CHANNEL_LEFT    (4)
#define SIDE_CHANNEL_RIGHT   (5)
#define BACK_CHANNEL_LEFT    (6)
#define BACK_CHANNEL_RIGHT   (7)
#define BACK_CHANNEL_CENTER  (8)
#define LFE_CHANNEL          (9)
#define UNKNOWN_CHANNEL      (0)
#define ER_OBJECT_START      17 /* First object type that has ER */
#define LEN_SE_ID            3  /* Bitstream */
#define LEN_TAG              4
#define LEN_BYTE             8
#define EXT_FIL              0
#define EXT_FILL_DATA        1
#define EXT_DATA_ELEMENT     2
#define EXT_DYNAMIC_RANGE    11
#define ANC_DATA             0
#define ID_SCE               0x0 /* Syntax elements */
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
#define MAX_CHANNELS         64
#define MAX_SYNTAX_ELEMENTS  48
#define MAX_WINDOW_GROUPS    8
#define MAX_SFB              51
#define MAX_LTP_SFB          40
#define MAX_LTP_SFB_S        8
#define BYTE_NUMBIT          8
#define BYTE_NUMBIT_LD       3
#define TNS_MAX_ORDER        20
#define EXTENSION_ID_PS      2
#define MAX_PS_ENVELOPES     5
#define NO_ALLPASS_LINKS     3
#define MAX_NTSRHFG          40     /* MAX_NTSRHFG: maximum of number_time_slots * rate + HFGen. 16*2+8 */
#define MAX_NTSR             32     /* max number_time_slots * rate */
#define MAX_M                49     /* MAX_M: maximum value for M */
#define MAX_L_E              5      /* MAX_L_E: maximum value for L_E */
#define DRC_REF_LEVEL        20 * 4 /* -20 dB */
#define NUM_ERROR_MESSAGES   34
#define ZERO_HCB             0
#define FIRST_PAIR_HCB       5
#define ESC_HCB              11
#define QUAD_LEN             4
#define PAIR_LEN             2
#define NOISE_HCB            13
#define INTENSITY_HCB2       14
#define INTENSITY_HCB        15
#define IQ_TABLE_SIZE        1026
#define NUM_CB               6
#define NUM_CB_ER            22
#define MAX_CB               32
#define VCB11_FIRST          16
#define VCB11_LAST           31
#define NOISE_OFFSET         90
#define NEGATE_IPD_MASK      (0x1000)
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
#define ESC_VAL              7
#define MAX_ASC_BYTES        64
#define NOISE_FLOOR_OFFSET   6
#define TABLE_BITS           6  /* just take the maximum number of bits for interpolation */

#define INTERP_BITS    (REAL_BITS - TABLE_BITS)
#define bit2byte(a)    ((a + 7) >> BYTE_NUMBIT_LD)
#define COEF_BITS      28
#define COEF_PRECISION (1 << COEF_BITS)
#define REAL_BITS      14 // MAXIMUM OF 14 FOR FIXED POINT SBR
#define REAL_PRECISION (1 << REAL_BITS)
#define FRAC_SIZE      32 /* frac is a 32 bit integer */
#define FRAC_BITS      31
#define FRAC_PRECISION ((uint32_t)(1 << FRAC_BITS))
#define FRAC_MAX       0x7FFFFFFF
#define REAL_CONST(A)  (((A) >= 0) ? ((int32_t)((A) * (REAL_PRECISION) + 0.5)) : ((int32_t)((A) * (REAL_PRECISION)-0.5)))
#define LOG2_MIN_INF   REAL_CONST(-10000)
#define COEF_CONST(A)  (((A) >= 0) ? ((int32_t)((A) * (COEF_PRECISION) + 0.5)) : ((int32_t)((A) * (COEF_PRECISION)-0.5)))
#define FRAC_CONST(A) \
    (((A) == 1.00) ? ((int32_t)FRAC_MAX) : (((A) >= 0) ? ((int32_t)((A) * (FRAC_PRECISION) + 0.5)) : ((int32_t)((A) * (FRAC_PRECISION)-0.5))))
#define DECAY_SLOPE       FRAC_CONST(0.05)
#define COEF_SQRT2        COEF_CONST(1.4142135623731)
#define Q2_BITS           22
#define Q2_PRECISION      (1 << Q2_BITS)
#define Q2_CONST(A)       (((A) >= 0) ? ((int32_t)((A) * (Q2_PRECISION) + 0.5)) : ((int32_t)((A) * (Q2_PRECISION)-0.5)))
#define MUL_R(A, B)       (int32_t)(((int64_t)(A) * (int64_t)(B) + (1  << (REAL_BITS - 1))) >> REAL_BITS) /* multiply with real shift */
#define MUL_C(A, B)       (int32_t)(((int64_t)(A) * (int64_t)(B) + (1  << (COEF_BITS - 1))) >> COEF_BITS) /* multiply with coef shift */
#define _MulHigh(A, B)    (int32_t)(((int64_t)(A) * (int64_t)(B) + (1u << (FRAC_SIZE - 1))) >> FRAC_SIZE) /* multiply with fractional shift */
#define MUL_F(A, B)       (int32_t)(((int64_t)(A) * (int64_t)(B) + (1  << (FRAC_BITS - 1))) >> FRAC_BITS)
#define MUL_Q2(A, B)      (int32_t)(((int64_t)(A) * (int64_t)(B) + (1  << (Q2_BITS - 1))) >> Q2_BITS)
#define MUL_SHIFT6(A, B)  (int32_t)(((int64_t)(A) * (int64_t)(B) + (1  << (6 - 1))) >> 6)
#define MUL_SHIFT23(A, B) (int32_t)(((int64_t)(A) * (int64_t)(B) + (1  << (23 - 1))) >> 23)
#define RE(A)             A[0]
#define IM(A)             A[1]

#ifdef FIXED_POINT
#define DIV_R(A, B) (((int64_t)A * REAL_PRECISION)/B)
#define DIV_C(A, B) (((int64_t)A * COEF_PRECISION)/B)
#define DIV_F(A, B) (((int64_t)A * FRAC_PRECISION)/B)
#else
#define DIV_R(A, B) ((A)/(B))
#define DIV_C(A, B) ((A)/(B))
#define DIV_F(A, B) ((A)/(B))
#endif

#define QMF_RE(A)         RE(A)
#define QMF_IM(A)         IM(A)
#define DM_MUL            FRAC_CONST(0.3203772410170407)    // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2            FRAC_CONST(0.7071067811865475244) // 1/sqrt(2)
#define segmentWidth(cb)  min(maxCwLen[cb], ics->length_of_longest_codeword)
#define DIV(A, B)         (((int64_t)A << REAL_BITS) / B)
#define bit_set(A, B)     ((A) & (1 << (B)))
#define SAT_SHIFT_MASK(E) (~0u << (31u - (E)))
#define SAT_SHIFT(V,E,M) (((((V) >> ((E) + 1)) ^ (V)) & (M)) ? (((V) < 0) ? (int32_t)0x80000000 : 0x7FFFFFFF) : ((int32_t)((uint32_t)(V) << (E))))

#define step(shift)                                  \
    if((0x40000000l >> shift) + root <= value) {     \
        value -= (0x40000000l >> shift) + root;      \
        root = (root >> 1) | (0x40000000l >> shift); \
    }                                                \
    else { root = root >> 1; }

#ifndef max
    #define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
    #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2 /* PI/2 */
    #define M_PI_2 1.57079632679489661923
#endif

#define ANSI_ESC_BLACK      "\033[30m"
#define ANSI_ESC_RED        "\033[31m"
#define ANSI_ESC_GREEN      "\033[32m"
#define ANSI_ESC_YELLOW     "\033[33m"
#define ANSI_ESC_BLUE       "\033[34m"
#define ANSI_ESC_MAGENTA    "\033[35m"
#define ANSI_ESC_CYAN       "\033[36m"
#define ANSI_ESC_WHITE      "\033[37m"
#define ANSI_ESC_RESET      "\033[0m"
#define ANSI_ESC_BROWN      "\033[38;5;130m"
#define ANSI_ESC_ORANGE     "\033[38;5;214m"


#include "structs.h"
#include "tables.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//                                              P R O T O T Y P E S
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// clang-format off
static void*               aac_frame_decode(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, uint8_t*buffer, uint32_t buffer_size, void** sample_buffer2, uint32_t sample_buffer_size);
static void                adts_error_check(adts_header_t* adts, bitfile_t* ld);
static uint8_t             adts_fixed_header(adts_header_t* adts, bitfile_t* ld);
uint8_t                    adts_frame(adts_header_t* adts, bitfile_t* ld);
static void                adts_variable_header(adts_header_t* adts, bitfile_t* ld);
uint8_t                    allocate_single_channel(NeAACDecStruct_t* hDecoder, uint8_t channel, uint8_t output_channels);
int8_t                     AudioSpecificConfig2(uint8_t* pBuffer, uint32_t buffer_size, program_config_t* pce, uint8_t short_form);
int8_t                     AudioSpecificConfigFrombitfile(bitfile_t* ld, program_config_t* pce, uint32_t bsize, uint8_t short_form);
static void                calc_chirp_factors(sbr_info_t* sbr, uint8_t ch);
static void                calc_prediction_coef(sbr_info_t* sbr, complex_t Xlow[MAX_NTSRHFG][64], complex_t* alpha_0, complex_t* alpha_1, uint8_t k);
static uint8_t             calc_sbr_tables(sbr_info_t* sbr, uint8_t start_freq, uint8_t stop_freq, uint8_t samplerate_mode, uint8_t freq_scale, uint8_t alter_scale, uint8_t xover_band);
static void                calculate_gain(sbr_info_t* sbr, sbr_hfadj_info_t* adj, uint8_t ch);
int8_t                     can_decode_ot(const uint8_t object_type);
void                       cfftb(cfft_info_t* cfft, complex_t* c);
void                       cfftf(cfft_info_t* cfft, complex_t* c);
cfft_info_t*                 cffti(uint16_t n);
static void                cffti1(uint16_t n, complex_t* wa, uint16_t* ifac);
void                       cfftu(cfft_info_t* cfft);
static void                channel_filter2(hyb_info_t* hyb, uint8_t frame_len, const int32_t* filter, complex_t* buffer, complex_t** X_hybrid);
static void                channel_filter8(hyb_info_t* hyb, uint8_t frame_len, const int32_t* filter, complex_t* buffer, complex_t** X_hybrid);
static uint8_t             channel_pair_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld, uint8_t channel, uint8_t* tag);
static void                create_channel_config(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo);
static uint16_t            data_stream_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld);
static void                DCT3_4_unscaled(int32_t* y, int32_t* x);
static void                DCT4_32(int32_t* y, int32_t* x);
void                       dct4_kernel(int32_t* in_real, int32_t* in_imag, int32_t* out_real, int32_t* out_imag);
static void                decode_cpe(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, bitfile_t* ld, uint8_t id_syn_ele);
static void                decode_sce_lfe(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, bitfile_t* ld, uint8_t id_syn_ele);
static int8_t              delta_clip(int8_t i, int8_t min, int8_t max);
static void                delta_decode(uint8_t enable, int8_t* index, int8_t* index_prev, uint8_t dt_flag, uint8_t nr_par, uint8_t stride, int8_t min_index, int8_t max_index);
static void                delta_modulo_decode(uint8_t enable, int8_t* index, int8_t* index_prev, uint8_t dt_flag, uint8_t nr_par, uint8_t stride, int8_t and_modulo);
uint8_t                    derived_frequency_table(sbr_info_t* sbr, uint8_t bs_xover_band, uint8_t k2);
void                       drc_decode(drc_info_t* drc, int32_t* spec);
void                       drc_end(drc_info_t* drc);
drc_info_t*                drc_init(int32_t cut, int32_t boost);
static void                DST4_32(int32_t* y, int32_t* x);
static uint8_t             dynamic_range_info(bitfile_t* ld, drc_info_t* drc);
uint8_t                    envelope_time_border_vector(sbr_info_t* sbr, uint8_t ch);
static uint8_t             estimate_current_envelope(sbr_info_t* sbr, sbr_hfadj_info_t* adj, complex_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
static uint8_t             excluded_channels(bitfile_t* ld, drc_info_t* drc);
static uint16_t            extension_payload(bitfile_t* ld, drc_info_t* drc, uint16_t count);
void                       extract_envelope_data(sbr_info_t* sbr, uint8_t ch);
void                       extract_noise_floor_data(sbr_info_t* sbr, uint8_t ch);
static uint8_t             faad_byte_align(bitfile_t* ld);
void*                      faad_calloc(size_t a, size_t s);
static void                faad_flushbits_ex(bitfile_t* ld, uint32_t bits);
static void                faad_flushbits_rev(bitfile_t* ld, uint32_t bits);
static void                faad_free(void* b);
static uint32_t            faad_get_processed_bits(bitfile_t* ld);
static uint8_t*            faad_getbitbuffer(bitfile_t* ld, uint32_t bits);
static uint32_t            faad_getbits(bitfile_t* ld, uint32_t n);
static uint32_t            faad_getbits_rev(bitfile_t* ld, uint32_t n) __attribute__((unused));
static void                faad_imdct(mdct_info_t* mdct, int32_t* X_in, int32_t* X_out);
static void                faad_initbits_rev(bitfile_t* ld, void* buffer, uint32_t bits_in_buffer);
static void                faad_initbits(bitfile_t* ld, const void* buffer, const uint32_t buffer_size);
uint32_t                   faad_latm_frame(latm_header_t* latm, bitfile_t* ld);
void*                      faad_malloc(size_t size);
void                       faad_mdct_end(mdct_info_t* mdct);
mdct_info_t*               faad_mdct_init(uint16_t N);
void                       faad_mdct(mdct_info_t* mdct, int32_t* X_in, int32_t* X_out);
static void                faad_resetbits(bitfile_t* ld, int32_t bits);
static void                faad_rewindbits(bitfile_t* ld);
static uint32_t            faad_showbits(bitfile_t* ld, uint32_t bits);
static uint8_t             fill_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld, drc_info_t* drc, uint8_t sbr_ele);
void                       filter_bank_end(fb_info_t* fb);
fb_info_t*                 filter_bank_init(uint16_t frame_len);
void                       filter_bank_ltp(fb_info_t* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* in_data, int32_t* out_mdct, uint8_t object_type, uint16_t frame_len);
static int32_t             find_bands(uint8_t warp, uint8_t bands, uint8_t a0, uint8_t a1);
int8_t                     GASpecificConfig(bitfile_t* ld, program_config_t* pce);
static void                gen_rand_vector(int32_t* spec, int16_t scale_factor, uint16_t size, uint8_t sub, uint32_t* __r1, uint32_t* __r2);
void                       get_adif_header_t(adif_header_t* adif, bitfile_t* ld);
uint32_t                   get_sample_rate(const uint8_t sr_index);
uint8_t                    get_sr_index(const uint32_t samplerate);
static uint32_t            getdword_n(void* mem, int32_t n);
static uint32_t            getdword(void* mem);
uint8_t                    hf_adjustment(sbr_info_t* sbr, complex_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
static void                hf_assembly(sbr_info_t* sbr, sbr_hfadj_info_t* adj, complex_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
void                       hf_generation(sbr_info_t* sbr, complex_t Xlow[MAX_NTSRHFG][64], complex_t Xhigh[MAX_NTSRHFG][64], uint8_t ch);
static void                huff_data(bitfile_t* ld, const uint8_t dt, const uint8_t nr_par, ps_huff_tab t_huff, ps_huff_tab f_huff, int8_t* par);
static uint8_t             huffman_2step_pair_sign(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_2step_pair(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_2step_quad_sign(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_2step_quad(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_binary_pair_sign(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_binary_pair(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_binary_quad_sign(uint8_t cb, bitfile_t* ld, int16_t* sp);
static uint8_t             huffman_binary_quad(uint8_t cb, bitfile_t* ld, int16_t* sp);
static int16_t             huffman_codebook(uint8_t i);
static uint8_t             huffman_getescape(bitfile_t* ld, int16_t* sp);
int8_t                     huffman_scale_factor(bitfile_t* ld);
static void                huffman_sign_bits(bitfile_t* ld, int16_t* sp, uint8_t len);
int8_t                     huffman_spectral_data_2(uint8_t cb, bits_t_t* ld, int16_t* sp);
uint8_t                    huffman_spectral_data(uint8_t cb, bitfile_t* ld, int16_t* sp);
static void                hybrid_analysis(hyb_info_t* hyb, complex_t* X[64], complex_t* X_hybrid[32], uint8_t use34, uint8_t numTimeSlotsRate);
static hyb_info_t*         hybrid_init(uint8_t numTimeSlotsRate);
static void                hybrid_synthesis(hyb_info_t* hyb, complex_t* X[64], complex_t* X_hybrid[32], uint8_t use34, uint8_t numTimeSlotsRate);
static uint8_t             ics_info(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld, uint8_t common_window);
void                       ifilter_bank(fb_info_t* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* freq_in, int32_t* time_out, int32_t* overlap, uint8_t object_type, uint16_t frame_len);
static uint8_t             individual_channel_stream(NeAACDecStruct_t* hDecoder, element_t* ele, bitfile_t* ld, ic_stream_t* ics, uint8_t scal_flag, int16_t* spec_data);
static void                invf_mode(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch);
void                       is_decode(ic_stream_t* ics, ic_stream_t* icsr, int32_t* l_spec, int32_t* r_spec, uint16_t frame_len);
uint8_t                    is_ltp_ot(uint8_t object_type);
void                       limiter_frequency_table(sbr_info_t* sbr);
int32_t                    log2_fix(uint32_t val);
int32_t                    log2_int(uint32_t val);
void                       lt_prediction(ic_stream_t* ics, ltp_info_t* ltp, int32_t* spec, int16_t* lt_pred_stat, fb_info_t* fb, uint8_t win_shape, uint8_t win_shape_prev, uint8_t sr_index, uint8_t object_type, uint16_t frame_len);
void                       lt_update_state(int16_t* lt_pred_stat, int32_t* time, int32_t* overlap, uint16_t frame_len, uint8_t object_type);
static uint8_t             ltp_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, ltp_info_t* ltp, bitfile_t* ld);
static void                map20indexto34(int8_t* index, uint8_t bins);
uint8_t                    master_frequency_table_fs0(sbr_info_t* sbr, uint8_t k0, uint8_t k2, uint8_t bs_alter_scale);
static int                 int32cmp(const void *a, const void *b);
uint8_t                    master_frequency_table(sbr_info_t* sbr, uint8_t k0, uint8_t k2, uint8_t bs_freq_scale, uint8_t bs_alter_scale);
uint8_t                    max_pred_sfb(const uint8_t sr_index);
uint8_t                    max_tns_sfb(const uint8_t sr_index, const uint8_t object_type, const uint8_t is_short);
static uint8_t             middleBorder(sbr_info_t* sbr, uint8_t ch);
void                       ms_decode(ic_stream_t* ics, ic_stream_t* icsr, int32_t* l_spec, int32_t* r_spec, uint16_t frame_len);
uint32_t                   ne_rng(uint32_t* __r1, uint32_t* __r2);
int8_t                     NeAACDecAudioSpecificConfig(uint8_t*pBuffer, uint32_t buffer_size);
void                       NeAACDecClose(NeAACDecHandle hDecoder);
void*                      NeAACDecDecode(NeAACDecHandle hDecoder, NeAACDecFrameInfo_t* hInfo, uint8_t*buffer, uint32_t buffer_size);
void*                      NeAACDecDecode2(NeAACDecHandle hDecoder, NeAACDecFrameInfo_t* hInfo, uint8_t*buffer, uint32_t buffer_size, void** sample_buffer, uint32_t sample_buffer_size);
uint32_t                   NeAACDecGetCapabilities(void);
NeAACDecConfigurationPtr_t NeAACDecGetCurrentConfiguration(NeAACDecHandle hDecoder);
const char*                NeAACDecGetErrorMessage(uint8_t errcode);
int32_t                    NeAACDecGetVersion(const char** faad_id_string, const char** faad_copyright_string);
long                       NeAACDecInit(NeAACDecHandle hDecoder, uint8_t*buffer, uint32_t buffer_size, uint32_t* samplerate, uint8_t*channels);
int8_t                     NeAACDecInit2(NeAACDecHandle hDecoder, uint8_t*pBuffer, uint32_t SizeOfDecoderSpecificInfo, uint32_t* samplerate, uint8_t*channels);
NeAACDecHandle             NeAACDecOpen(void);
void                       NeAACDecPostSeekReset(NeAACDecHandle hDecoder, long frame);
uint8_t                    NeAACDecSetConfiguration(NeAACDecHandle hDecoder, NeAACDecConfigurationPtr_t config);
void                       noise_floor_time_border_vector(sbr_info_t* sbr, uint8_t ch);
void*                      output_to_PCM(NeAACDecStruct_t* hDecoder, int32_t** input, void* samplebuffer, uint8_t channels, uint16_t frame_len, uint8_t format);
static void                passf2neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa);
static void                passf2pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa);
static void                passf3(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const int8_t isign);
static void                passf4neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3);
static void                passf4pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3);
static void                passf5(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3, const complex_t* wa4, const int8_t isign);
static void                patch_construction(sbr_info_t* sbr);
void                       pns_decode(ic_stream_t* ics_left, ic_stream_t* ics_right, int32_t* spec_left, int32_t* spec_right, uint16_t frame_len, uint8_t channel_pair, uint8_t object_type, uint32_t* __r1, uint32_t* __r2);
int32_t                    pow2_fix(int32_t val);
int32_t                    pow2_int(int32_t val);
static uint8_t             program_config_t_element(program_config_t* pce, bitfile_t* ld);
static void                ps_data_decode(ps_info_t* ps);
uint16_t                   ps_data(ps_info_t* ps, bitfile_t* ld, uint8_t* header); /* ps_syntax.c */
uint8_t                    ps_decode(ps_info_t* ps, complex_t* X_left[64], complex_t* X_right[64]);
static void                ps_decorrelate(ps_info_t* ps, complex_t* X_left[64], complex_t* X_right[64], complex_t* X_hybrid_left[32], complex_t* X_hybrid_right[32]);
static uint16_t            ps_extension(ps_info_t* ps, bitfile_t* ld, const uint8_t ps_extension_id, const uint16_t num_bits_left);
void                       ps_free(ps_info_t* ps);
ps_info_t*                 ps_init(uint8_t sr_index, uint8_t numTimeSlotsRate); /* ps_dec.c */
static void                ps_mix_phase(ps_info_t* ps, complex_t* X_left[64], complex_t* X_right[64], complex_t* X_hybrid_left[32], complex_t* X_hybrid_right[32]);
static uint8_t             pulse_data(ic_stream_t* ics, pulse_info_t* pul, bitfile_t* ld);
uint8_t                    pulse_decode(ic_stream_t* ics, int16_t* spec_coef, uint16_t framelen);
uint8_t                    qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode, uint32_t sample_rate);
uint8_t                    qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate, uint8_t k0);
void                       qmfa_end(qmfa_info_t* qmfa);
qmfa_info_t*               qmfa_init(uint8_t channels);
void                       qmfs_end(qmfs_info_t* qmfs);
qmfs_info_t*               qmfs_init(uint8_t channels);
static uint8_t             quant_to_spec(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, int16_t* quant_data, int32_t* spec_data, uint16_t frame_len);
void                       raw_data_block(NeAACDecStruct_t* hDecoder, NeAACDecFrameInfo_t* hInfo, bitfile_t* ld, program_config_t* pce, drc_info_t* drc);
static int16_t             real_to_int16(int32_t sig_in);
uint8_t                    reconstruct_channel_pair(NeAACDecStruct_t* hDecoder, ic_stream_t* ics1, ic_stream_t* ics2, element_t* cpe, int16_t* spec_data1, int16_t* spec_data2);
uint8_t                    reconstruct_single_channel(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, element_t* sce, int16_t* spec_data);
uint8_t                    reordered_spectral_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld, int16_t* spectral_data);
uint8_t                    rvlc_decode_scale_factors(ic_stream_t* ics, bitfile_t* ld);
static uint8_t             rvlc_decode_sf_forward(ic_stream_t* ics, bitfile_t* ld_sf, bitfile_t* ld_esc, uint8_t* is_used);
static int8_t              rvlc_huffman_esc(bitfile_t* ld /*, int8_t direction*/);
static int8_t              rvlc_huffman_sf(bitfile_t *ld_sf, bitfile_t *ld_esc /*, int8_t direction*/);
uint8_t                    rvlc_scale_factor_data(ic_stream_t* ics, bitfile_t* ld);
static uint8_t             sbr_channel_pair_element(bitfile_t* ld, sbr_info_t* sbr);
static uint8_t             sbr_data(bitfile_t* ld, sbr_info_t* sbr);
static void                sbr_dtdf(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch);
void                       sbr_envelope(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch);
static uint8_t             sbr_extension_data(bitfile_t* ld, sbr_info_t* sbr, uint16_t cnt, uint8_t resetFlag);
static uint16_t            sbr_extension(bitfile_t* ld, sbr_info_t* sbr, uint8_t bs_extension_id, uint16_t num_bits_left);
static uint8_t             sbr_grid(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch);
static void                sbr_header(bitfile_t* ld, sbr_info_t* sbr);
void                       sbr_noise(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch);
void                       sbr_qmf_analysis_32(sbr_info_t* sbr, qmfa_info_t* qmfa, const int32_t* input, complex_t X[MAX_NTSRHFG][64], uint8_t offset, uint8_t kx);
void                       sbr_qmf_synthesis_32(sbr_info_t* sbr, qmfs_info_t* qmfs, complex_t* X[64], int32_t* output);
void                       sbr_qmf_synthesis_64(sbr_info_t* sbr, qmfs_info_t* qmfs, complex_t* X[64], int32_t* output);
static void                sbr_save_matrix(sbr_info_t* sbr, uint8_t ch);
static uint8_t             sbr_save_prev_data(sbr_info_t* sbr, uint8_t ch);
static uint8_t             sbr_single_channel_element(bitfile_t* ld, sbr_info_t* sbr);
uint8_t                    sbrDecodeCoupleFrame(sbr_info_t* sbr, int32_t* left_chan, int32_t* right_chan, const uint8_t just_seeked, const uint8_t downSampledSBR);
void                       sbrDecodeEnd(sbr_info_t* sbr);
sbr_info_t*                sbrDecodeInit(uint16_t framelength, uint8_t id_aac, uint32_t sample_rate, uint8_t downSampledSBR);
uint8_t                    sbrDecodeSingleFrame(sbr_info_t* sbr, int32_t* channel, const uint8_t just_seeked, const uint8_t downSampledSBR);
uint8_t                    sbrDecodeSingleFramePS(sbr_info_t* sbr, int32_t* left_channel, int32_t* right_channel, const uint8_t just_seeked, const uint8_t downSampledSBR);
void                       sbrReset(sbr_info_t* sbr);
static uint8_t             scale_factor_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld);
static uint8_t             section_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld);
static uint32_t            showbits_hcr(bits_t_t* ld, uint8_t bits);
static uint8_t             side_info(NeAACDecStruct_t* hDecoder, element_t* ele, bitfile_t* ld, ic_stream_t* ics, uint8_t scal_flag);
static uint8_t             single_lfe_channel_element(NeAACDecStruct_t* hDecoder, bitfile_t* ld, uint8_t channel, uint8_t* tag);
static void                sinusoidal_coding(bitfile_t* ld, sbr_info_t* sbr, uint8_t ch);
static uint8_t             spectral_data(NeAACDecStruct_t* hDecoder, ic_stream_t* ics, bitfile_t* ld, int16_t* spectral_data);
static void                tns_ar_filter(int32_t* spectrum, uint16_t size, int8_t inc, int32_t* lpc, uint8_t order, uint8_t exp);
static void                tns_data(ic_stream_t* ics, tns_info_t* tns, bitfile_t* ld);
static uint8_t             tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress, uint8_t* coef, int32_t* a);
void                       tns_decode_frame(ic_stream_t* ics, tns_info_t* tns, uint8_t sr_index, uint8_t object_type, int32_t* spec, uint16_t frame_len);
void                       tns_encode_frame(ic_stream_t* ics, tns_info_t* tns, uint8_t sr_index, uint8_t object_type, int32_t* spec, uint16_t frame_len);
static void                tns_ma_filter(int32_t* spectrum, uint16_t size, int8_t inc, int32_t* lpc, uint8_t order, uint8_t exp);
static void                vcb11_check_LAV(uint8_t cb, int16_t* sp);
uint8_t                    window_grouping_info(NeAACDecStruct_t* hDecoder, ic_stream_t* ics);
uint32_t                   wl_min_lzc(uint32_t x);
static void                reset_all_predictors(pred_state_t *state, uint16_t frame_len);
static void                ic_prediction(ic_stream_t *ics, int32_t *spec, pred_state_t *state, uint16_t frame_len, uint8_t sf_index);
static void                ic_predict(pred_state_t *state, int32_t input, int32_t *output, uint8_t pred);
static void                pns_reset_pred_state(ic_stream_t *ics, pred_state_t *state);
static float               inv_quant_pred(int16_t q) __attribute__((unused));
static float               flt_round(float_t pf) __attribute__((unused));
static int16_t             quant_pred(float x) __attribute__((unused));
// clang-format on
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//                                              I N L I N E S
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline void faad_flushbits(bitfile_t* ld, uint32_t bits) {
    /* do nothing if error */
    if(ld->error != 0) return;
    if(bits < ld->bits_left) { ld->bits_left -= bits; }
    else { faad_flushbits_ex(ld, bits); }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline uint8_t faad_get1bit(bitfile_t* ld) {
    uint8_t r;
    if(ld->bits_left > 0) {
        ld->bits_left--;
        r = (uint8_t)((ld->bufa >> ld->bits_left) & 1);
        return r;
    }
    /* bits_left == 0 */
    r = (uint8_t)faad_getbits(ld, 1);
    return r;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* reversed bitreading routines */
static inline uint32_t faad_showbits_rev(bitfile_t* ld, uint32_t bits) {
    uint8_t  i;
    uint32_t B = 0;
    if(bits <= ld->bits_left) {
        for(i = 0; i < bits; i++) {
            if(ld->bufa & (1u << (i + (32 - ld->bits_left)))) B |= (1u << (bits - i - 1));
        }
        return B;
    }
    else {
        for(i = 0; i < ld->bits_left; i++) {
            if(ld->bufa & (1u << (i + (32 - ld->bits_left)))) B |= (1u << (bits - i - 1));
        }
        for(i = 0; i < bits - ld->bits_left; i++) {
            if(ld->bufb & (1u << (i + (32 - ld->bits_left)))) B |= (1u << (bits - ld->bits_left - i - 1));
        }
        return B;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* return 1 if position is outside of buffer, 0 otherwise */
static inline int8_t flushbits_hcr(bits_t_t* ld, uint8_t bits) {
    ld->len -= bits;

    if(ld->len < 0) {
        ld->len = 0;
        return 1;
    }
    else { return 0; }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int8_t getbits_hcr(bits_t_t* ld, uint8_t n, uint32_t* result) {
    *result = showbits_hcr(ld, n);
    return flushbits_hcr(ld, n);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int8_t get1bit_hcr(bits_t_t* ld, uint8_t* result) {
    uint32_t res;
    int8_t   ret;

    ret = getbits_hcr(ld, 1, &res);
    *result = (int8_t)(res & 1);
    return ret;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int8_t is_intensity(ic_stream_t* ics, uint8_t group, uint8_t sfb) {
    switch(ics->sfb_cb[group][sfb]) {
    case INTENSITY_HCB: return 1;
    case INTENSITY_HCB2: return -1;
    default: return 0;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int8_t invert_intensity(ic_stream_t* ics, uint8_t group, uint8_t sfb) {
    if(ics->ms_mask_present == 1) return (1 - 2 * ics->ms_used[group][sfb]);
    return 1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Complex multiplication */
inline void ComplexMult(int32_t* y1, int32_t* y2, int32_t x1, int32_t x2, int32_t c1, int32_t c2) {
    *y1 = (_MulHigh(x1, c1) + _MulHigh(x2, c2)) << (FRAC_SIZE - FRAC_BITS);
    *y2 = (_MulHigh(x2, c1) - _MulHigh(x1, c2)) << (FRAC_SIZE - FRAC_BITS);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline uint8_t is_noise(ic_stream_t* ics, uint8_t group, uint8_t sfb) {
    if(ics->sfb_cb[group][sfb] == NOISE_HCB) return 1;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static inline int8_t ps_huff_dec(bitfile_t* ld, ps_huff_tab t_huff) { /* binary search huffman decoding */
    uint8_t bit;
    int16_t index = 0;

    while(index >= 0) {
        bit = (uint8_t)faad_get1bit(ld);
        index = t_huff[index][bit];
    }
    return index + 31;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
