/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2 must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**/

// ESP32 Version 29.07.2024
// updated:      10.05.2025

#pragma once
#include "Arduino.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#include "defines.h"
#include "settings.h"
#include "structs.h"
#include "tables.h"
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————





uint32_t ne_rng(uint32_t* __r1, uint32_t* __r2);
uint32_t wl_min_lzc(uint32_t x);
#ifdef FIXED_POINT
int32_t log2_int(uint32_t val);
int32_t log2_fix(uint32_t val);
int32_t pow2_int(int32_t val);
int32_t  pow2_fix(int32_t val);
#endif
uint8_t                           get_sr_index(const uint32_t samplerate);
uint8_t                           max_pred_sfb(const uint8_t sr_index);
uint8_t                           max_tns_sfb(const uint8_t sr_index, const uint8_t object_type, const uint8_t is_short);
uint32_t                          get_sample_rate(const uint8_t sr_index);
int8_t                            can_decode_ot(const uint8_t object_type);
void*                             faad_malloc(size_t size);
template <typename freeType> void faad_free(freeType** b);

drc_info*                drc_init(int32_t cut, int32_t boost);
void                     drc_end(drc_info* drc);
void                     drc_decode(drc_info* drc, int32_t* spec);
sbr_info*                sbrDecodeInit(uint16_t framelength, uint8_t id_aac, uint32_t sample_rate, uint8_t downSampledSBR, uint8_t IsDRM);
void                     sbrDecodeEnd(sbr_info* sbr);
void                     sbrReset(sbr_info* sbr);
uint8_t                  sbrDecodeCoupleFrame(sbr_info* sbr, int32_t* left_chan, int32_t* right_chan, const uint8_t just_seeked, const uint8_t downSampledSBR);
uint8_t                  sbrDecodeSingleFrame(sbr_info* sbr, int32_t* channel, const uint8_t just_seeked, const uint8_t downSampledSBR);
uint16_t                 ps_data(ps_info* ps, bitfile* ld, uint8_t* header);
ps_info*                 ps_init(uint8_t sr_index, uint8_t numTimeSlotsRate);
void                     ps_free(ps_info* ps);
uint8_t                  ps_decode(ps_info* ps, qmf_t X_left[38][64], qmf_t X_right[38][64]);
void                     faad_initbits(bitfile* ld, const void* buffer, const uint32_t buffer_size);
void                     faad_endbits(bitfile* ld);
void                     faad_initbits_rev(bitfile* ld, void* buffer, uint32_t bits_in_buffer);
uint8_t                  faad_byte_align(bitfile* ld);
uint32_t                 faad_get_processed_bits(bitfile* ld);
void                     faad_flushbits_ex(bitfile* ld, uint32_t bits);
void                     faad_rewindbits(bitfile* ld);
void                     faad_resetbits(bitfile* ld, int bits);
uint8_t*                 faad_getbitbuffer(bitfile* ld, uint32_t bits);
void*                    faad_origbitbuffer(bitfile* ld);
uint32_t                 faad_origbitbuffer_size(bitfile* ld);
uint8_t                  faad_get1bit(bitfile* ld);
uint32_t                 faad_getbits(bitfile* ld, uint32_t n);
uint32_t                 faad_showbits_rev(bitfile* ld, uint32_t bits);
void                     faad_flushbits_rev(bitfile* ld, uint32_t bits);
uint32_t                 getdword(void* mem);
uint32_t                 getdword_n(void* mem, int n);
void                     faad_flushbits(bitfile* ld, uint32_t bits);
uint32_t                 faad_showbits(bitfile* ld, uint32_t bits);
uint32_t                 showbits_hcr(bits_t* ld, uint8_t bits);
uint32_t                 faad_getbits_rev(bitfile* ld, uint32_t n);
int8_t                   get1bit_hcr(bits_t* ld, uint8_t* result);
int8_t                   flushbits_hcr(bits_t* ld, uint8_t bits);
int8_t                   getbits_hcr(bits_t* ld, uint8_t n, uint32_t* result);
void                     cfftf(cfft_info* cfft, complex_t* c);
void                     cfftb(cfft_info* cfft, complex_t* c);
cfft_info*               cffti(uint16_t n);
void                     cfftu(cfft_info* cfft);
NeAACDecHandle           NeAACDecOpen(void);
const char*              NeAACDecGetErrorMessage(unsigned const char errcode);
void*                    NeAACDecDecode2(NeAACDecHandle hpDecoder, NeAACDecFrameInfo* hInfo, uint8_t* buffer, uint32_t buffer_size, void** sample_buffer, uint32_t sample_buffer_size);
long                     NeAACDecInit(NeAACDecHandle hpDecoder, uint8_t* buffer, uint32_t buffer_size, uint32_t* samplerate, uint8_t* channels);
uint8_t            NeAACDecSetConfiguration(NeAACDecHandle hpDecoder, NeAACDecConfigurationPtr config);
char                     NeAACDecInit2(NeAACDecHandle hpDecoder, uint8_t* pBuffer, uint32_t SizeOfDecoderSpecificInfo, uint32_t* samplerate, uint8_t* channels);
uint8_t            NeAACDecSetConfiguration(NeAACDecHandle hpDecoder, NeAACDecConfigurationPtr config);
void                     NeAACDecClose(NeAACDecHandle hpDecoder);
NeAACDecConfigurationPtr NeAACDecGetCurrentConfiguration(NeAACDecHandle hpDecoder);
void*                    aac_frame_decode(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, uint8_t* buffer, uint32_t buffer_size, void** sample_buffer2, uint32_t sample_buffer_size);
void                     create_channel_config(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo);
void                     ssr_filter_bank_end(fb_info* fb);
void                     passf2pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa);
void                     passf2neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa);
void                     passf3(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const int8_t isign);
void                     passf4pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3);
void                     passf4neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3);
void      passf5(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3, const complex_t* wa4, const int8_t isign);
void      cffti1(uint16_t n, complex_t* wa, uint16_t* ifac);
drc_info* drc_init(int32_t cut, int32_t boost);
void      drc_end(drc_info* drc);
void      drc_decode(drc_info* drc, int32_t* spec);
fb_info*  filter_bank_init(uint16_t frame_len);
void      filter_bank_end(fb_info* fb);
void      filter_bank_ltp(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* in_data, int32_t* out_mdct, uint8_t object_type, uint16_t frame_len);
void   ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* freq_in, int32_t* time_out, int32_t* overlap, uint8_t object_type, uint16_t frame_len);
void   ms_decode(ic_stream* ics, ic_stream* icsr, int32_t* l_spec, int32_t* r_spec, uint16_t frame_len);
void   is_decode(ic_stream* ics, ic_stream* icsr, int32_t* l_spec, int32_t* r_spec, uint16_t frame_len);
int8_t is_intensity(ic_stream* ics, uint8_t group, uint8_t sfb);
uint8_t      is_noise(ic_stream* ics, uint8_t group, uint8_t sfb);
int32_t       fp_sqrt(int32_t value);
void         pns_decode(ic_stream* ics_left, ic_stream* ics_right, int32_t* spec_left, int32_t* spec_right, uint16_t frame_len, uint8_t channel_pair, uint8_t object_type,
                        /* RNG states */ uint32_t* __r1, uint32_t* __r2);
int8_t       invert_intensity(ic_stream* ics, uint8_t group, uint8_t sfb);
void*        output_to_PCM(NeAACDecStruct* hDecoder, int32_t** input, void* samplebuffer, uint8_t channels, uint16_t frame_len, uint8_t format);
uint8_t      pulse_decode(ic_stream* ics, int16_t* spec_coef, uint16_t framelen);
void         gen_rand_vector(int32_t* spec, int16_t scale_factor, uint16_t size, uint8_t sub, uint32_t* __r1, uint32_t* __r2);
void         huffman_sign_bits(bitfile* ld, int16_t* sp, uint8_t len);
uint8_t      huffman_getescape(bitfile* ld, int16_t* sp);
uint8_t      huffman_2step_quad(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_2step_quad_sign(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_2step_pair(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_2step_pair_sign(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_binary_quad(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_binary_quad_sign(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_binary_pair(uint8_t cb, bitfile* ld, int16_t* sp);
uint8_t      huffman_binary_pair_sign(uint8_t cb, bitfile* ld, int16_t* sp);
int16_t      huffman_codebook(uint8_t i);
void         vcb11_check_LAV(uint8_t cb, int16_t* sp);
uint16_t     drm_ps_data(drm_ps_info* ps, bitfile* ld);
drm_ps_info* drm_ps_init(void);
void         drm_ps_free(drm_ps_info* ps);
uint8_t      drm_ps_decode(drm_ps_info* ps, uint8_t guess, qmf_t X_left[38][64], qmf_t X_right[38][64]);
int8_t       huffman_scale_factor(bitfile* ld);
uint8_t      huffman_spectral_data(uint8_t cb, bitfile* ld, int16_t* sp);
int8_t       huffman_spectral_data_2(uint8_t cb, bits_t* ld, int16_t* sp);
fb_info*     ssr_filter_bank_init(uint16_t frame_len);
void         ssr_filter_bank_end(fb_info* fb);
void         ssr_ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* freq_in, int32_t* time_out, uint16_t frame_len);
int8_t       AudioSpecificConfig2(uint8_t* pBuffer, uint32_t buffer_size, mp4AudioSpecificConfig* mp4ASC, program_config* pce, uint8_t short_form);
int8_t       AudioSpecificConfigFromBitfile(bitfile* ld, mp4AudioSpecificConfig* mp4ASC, program_config* pce, uint32_t bsize, uint8_t short_form);
void         pns_reset_pred_state(ic_stream* ics, pred_state* state);
void         reset_all_predictors(pred_state* state, uint16_t frame_len);
void         ic_prediction(ic_stream* ics, int32_t* spec, pred_state* state, uint16_t frame_len, uint8_t sf_index);
uint8_t      quant_to_spec(NeAACDecStruct* hDecoder, ic_stream* ics, int16_t* quant_data, int32_t* spec_data, uint16_t frame_len);
uint8_t      window_grouping_info(NeAACDecStruct* hDecoder, ic_stream* ics);
uint8_t      reconstruct_channel_pair(NeAACDecStruct* hDecoder, ic_stream* ics1, ic_stream* ics2, element* cpe, int16_t* spec_data1, int16_t* spec_data2);
uint8_t      reconstruct_single_channel(NeAACDecStruct* hDecoder, ic_stream* ics, element* sce, int16_t* spec_data);
void         tns_decode_frame(ic_stream* ics, tns_info* tns, uint8_t sr_index, uint8_t object_type, int32_t* spec, uint16_t frame_len);
void         tns_encode_frame(ic_stream* ics, tns_info* tns, uint8_t sr_index, uint8_t object_type, int32_t* spec, uint16_t frame_len);
uint8_t      is_ltp_ot(uint8_t object_type);
void         lt_prediction(ic_stream* ics, ltp_info* ltp, int32_t* spec, int16_t* lt_pred_stat, fb_info* fb, uint8_t win_shape, uint8_t win_shape_prev, uint8_t sr_index, uint8_t object_type,
                           uint16_t frame_len);
void         lt_update_state(int16_t* lt_pred_stat, int32_t* time, int32_t* overlap, uint16_t frame_len, uint8_t object_type);
void         tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress, uint8_t* coef, int32_t* a);
void         tns_ar_filter(int32_t* spectrum, uint16_t size, int8_t inc, int32_t* lpc, uint8_t order);
void         tns_ma_filter(int32_t* spectrum, uint16_t size, int8_t inc, int32_t* lpc, uint8_t order);
uint8_t      faad_check_CRC(bitfile* ld, uint16_t len);
/* static function declarations */
void    decode_sce_lfe(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, uint8_t id_syn_ele);
void    decode_cpe(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, uint8_t id_syn_ele);
uint8_t single_lfe_channel_element(NeAACDecStruct* hDecoder, bitfile* ld, uint8_t channel, uint8_t* tag);
uint8_t channel_pair_element(NeAACDecStruct* hDecoder, bitfile* ld, uint8_t channel, uint8_t* tag);
#ifdef COUPLING_DEC
uint8_t coupling_channel_element(NeAACDecStruct* hDecoder, bitfile* ld);
#endif
uint16_t data_stream_element(NeAACDecStruct* hDecoder, bitfile* ld);
uint8_t  program_config_element(program_config* pce, bitfile* ld);
uint8_t  fill_element(NeAACDecStruct* hDecoder, bitfile* ld, drc_info* drc, uint8_t sbr_ele);
uint8_t  individual_channel_stream(NeAACDecStruct* hDecoder, element* ele, bitfile* ld, ic_stream* ics, uint8_t scal_flag, int16_t* spec_data);
uint8_t  ics_info(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, uint8_t common_window);
uint8_t  section_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld);
uint8_t  scale_factor_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld);
#ifdef SSR_DEC
void gain_control_data(bitfile* ld, ic_stream* ics);
#endif
uint8_t  spectral_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, int16_t* spectral_data);
uint16_t extension_payload(bitfile* ld, drc_info* drc, uint16_t count);
uint8_t  pulse_data(ic_stream* ics, pulse_info* pul, bitfile* ld);
void     tns_data(ic_stream* ics, tns_info* tns, bitfile* ld);
#ifdef LTP_DEC
uint8_t ltp_data(NeAACDecStruct* hDecoder, ic_stream* ics, ltp_info* ltp, bitfile* ld);
#endif
uint8_t adts_fixed_header(adts_header* adts, bitfile* ld);
void    adts_variable_header(adts_header* adts, bitfile* ld);
void    adts_error_check(adts_header* adts, bitfile* ld);
uint8_t dynamic_range_info(bitfile* ld, drc_info* drc);
uint8_t excluded_channels(bitfile* ld, drc_info* drc);
uint8_t side_info(NeAACDecStruct* hDecoder, element* ele, bitfile* ld, ic_stream* ics, uint8_t scal_flag);
int8_t  GASpecificConfig(bitfile* ld, mp4AudioSpecificConfig* mp4ASC, program_config* pce);
uint8_t adts_frame(adts_header* adts, bitfile* ld);
void    get_adif_header(adif_header* adif, bitfile* ld);
void    raw_data_block(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, program_config* pce, drc_info* drc);
uint8_t reordered_spectral_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, int16_t* spectral_data);
#ifdef DRM
int8_t DRM_aac_scalable_main_header(NeAACDecStruct* hDecoder, ic_stream* ics1, ic_stream* ics2, bitfile* ld, uint8_t this_layer_stereo);
#endif
void    dct4_kernel(int32_t* in_real, int32_t* in_imag, int32_t* out_real, int32_t* out_imag);
void    DCT3_32_unscaled(int32_t* y, int32_t* x);
void    DCT4_32(int32_t* y, int32_t* x);
void    DST4_32(int32_t* y, int32_t* x);
void    DCT2_32_unscaled(int32_t* y, int32_t* x);
void    DCT4_16(int32_t* y, int32_t* x);
void    DCT2_16_unscaled(int32_t* y, int32_t* x);
uint8_t rvlc_scale_factor_data(ic_stream* ics, bitfile* ld);
uint8_t rvlc_decode_scale_factors(ic_stream* ics, bitfile* ld);
uint8_t sbr_extension_data(bitfile* ld, sbr_info* sbr, uint16_t cnt, uint8_t resetFlag);
int8_t  rvlc_huffman_sf(bitfile* ld_sf, bitfile* ld_esc, int8_t direction);
int8_t  rvlc_huffman_esc(bitfile* ld_esc, int8_t direction);
uint8_t rvlc_decode_sf_forward(ic_stream* ics, bitfile* ld_sf, bitfile* ld_esc, uint8_t* intensity_used);
#ifdef DRM
void DRM_aac_scalable_main_element(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, program_config* pce, drc_info* drc);
#endif
uint32_t faad_latm_frame(latm_header* latm, bitfile* ld);
#ifdef SSR_DEC
void ssr_decode(ssr_info* ssr, fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* freq_in, int32_t* time_out, int32_t* overlap,
                int32_t ipqf_buffer[SSR_BANDS][96 / 4], int32_t* prev_fmd, uint16_t frame_len);
ssr_gain_control(ssr_info* ssr, int32_t* data, int32_t* output, int32_t* overlap, int32_t* prev_fmd, uint8_t band, uint8_t window_sequence, uint16_t frame_len);
ssr_gc_function(ssr_info* ssr, int32_t* prev_fmd, int32_t* gc_function, uint8_t window_sequence, uint16_t frame_len);
#endif
void extract_envelope_data(sbr_info* sbr, uint8_t ch);
void extract_noise_floor_data(sbr_info* sbr, uint8_t ch);
#ifndef FIXED_POINT
void envelope_noise_dequantisation(sbr_info* sbr, uint8_t ch);
void unmap_envelope_noise(sbr_info* sbr);
#endif
void       ssr_ipqf(ssr_info* ssr, int32_t* in_data, int32_t* out_data, int32_t buffer[SSR_BANDS][96 / 4], uint16_t frame_len, uint8_t bands);
void       faad_mdct_init(uint16_t mdct_len, uint16_t N);
void       faad_mdct_end(uint16_t mdct_len);
void       faad_imdct(uint16_t mdct_idx, int32_t* X_in, int32_t* X_out);
void       faad_mdct(uint16_t mdct_len, int32_t* X_in, int32_t* X_out);
#if (defined(PS_DEC) || defined(DRM_PS))
uint8_t sbrDecodeSingleFramePS(sbr_info* sbr, int32_t* left_channel, int32_t* right_channel, const uint8_t just_seeked, const uint8_t downSampledSBR);
#endif
void     unmap_envelope_noise(sbr_info* sbr);
int16_t  real_to_int16(int32_t sig_in);
uint8_t  sbr_save_prev_data(sbr_info* sbr, uint8_t ch);
void     sbr_save_matrix(sbr_info* sbr, uint8_t ch);
fb_info* ssr_filter_bank_init(uint16_t frame_len);
void     ssr_filter_bank_end(fb_info* fb);
void     ssr_ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, int32_t* freq_in, int32_t* time_out, uint16_t frame_len);
int32_t  find_bands(uint8_t warp, uint8_t bands, uint8_t a0, uint8_t a1);
void     sbr_header(bitfile* ld, sbr_info* sbr);
uint8_t  calc_sbr_tables(sbr_info* sbr, uint8_t start_freq, uint8_t stop_freq, uint8_t samplerate_mode, uint8_t freq_scale, uint8_t alter_scale, uint8_t xover_band);
uint8_t  sbr_data(bitfile* ld, sbr_info* sbr);
uint16_t sbr_extension(bitfile* ld, sbr_info* sbr, uint8_t bs_extension_id, uint16_t num_bits_left);
uint8_t  sbr_single_channel_element(bitfile* ld, sbr_info* sbr);
uint8_t  sbr_channel_pair_element(bitfile* ld, sbr_info* sbr);
uint8_t  sbr_grid(bitfile* ld, sbr_info* sbr, uint8_t ch);
void     sbr_dtdf(bitfile* ld, sbr_info* sbr, uint8_t ch);
void     invf_mode(bitfile* ld, sbr_info* sbr, uint8_t ch);
void     sinusoidal_coding(bitfile* ld, sbr_info* sbr, uint8_t ch);
uint8_t  hf_adjustment(sbr_info* sbr, qmf_t Xsbr[MAX_NTSRHFG][64], int32_t* deg, uint8_t ch);
uint8_t  qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode, uint32_t sample_rate);
uint8_t  qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate, uint8_t k0);
uint8_t  master_frequency_table_fs0(sbr_info* sbr, uint8_t k0, uint8_t k2, uint8_t bs_alter_scale);
uint8_t  master_frequency_table(sbr_info* sbr, uint8_t k0, uint8_t k2, uint8_t bs_freq_scale, uint8_t bs_alter_scale);
uint8_t  derived_frequency_table(sbr_info* sbr, uint8_t bs_xover_band, uint8_t k2);
void     limiter_frequency_table(sbr_info* sbr);
#ifdef SBR_DEC
    #ifdef SBR_LOW_POWER
void calc_prediction_coef_lp(sbr_info* sbr, qmf_t Xlow[MAX_NTSRHFG][64], complex_t* alpha_0, complex_t* alpha_1, int32_t* rxx);
void calc_aliasing_degree(sbr_info* sbr, int32_t* rxx, int32_t* deg);
    #else  // SBR_LOW_POWER
void calc_prediction_coef(sbr_info* sbr, qmf_t Xlow[MAX_NTSRHFG][64], complex_t* alpha_0, complex_t* alpha_1, uint8_t k);
    #endif // SBR_LOW_POWER
void calc_chirp_factors(sbr_info* sbr, uint8_t ch);
void patch_construction(sbr_info* sbr);
#endif // SBR_DEC
#ifdef SBR_DEC
uint8_t estimate_current_envelope(sbr_info* sbr, sbr_hfadj_info* adj, qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
void    calculate_gain(sbr_info* sbr, sbr_hfadj_info* adj, uint8_t ch);
    #ifdef SBR_LOW_POWER
void calc_gain_groups(sbr_info* sbr, sbr_hfadj_info* adj, int32_t* deg, uint8_t ch);
void aliasing_reduction(sbr_info* sbr, sbr_hfadj_info* adj, int32_t* deg, uint8_t ch);
    #endif // SBR_LOW_POWER
void hf_assembly(sbr_info* sbr, sbr_hfadj_info* adj, qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
#endif // SBR_DEC
uint8_t    get_S_mapped(sbr_info* sbr, uint8_t ch, uint8_t l, uint8_t current_band);
qmfa_info* qmfa_init(uint8_t channels);
void       qmfa_end(qmfa_info* qmfa);
qmfs_info* qmfs_init(uint8_t channels);
void       qmfs_end(qmfs_info* qmfs);
void       sbr_qmf_analysis_32(sbr_info* sbr, qmfa_info* qmfa, const int32_t* input, qmf_t X[MAX_NTSRHFG][64], uint8_t offset, uint8_t kx);
void       sbr_qmf_synthesis_32(sbr_info* sbr, qmfs_info* qmfs, qmf_t X[MAX_NTSRHFG][64], int32_t* output);
void       sbr_qmf_synthesis_64(sbr_info* sbr, qmfs_info* qmfs, qmf_t X[MAX_NTSRHFG][64], int32_t* output);
uint8_t    envelope_time_border_vector(sbr_info* sbr, uint8_t ch);
void       noise_floor_time_border_vector(sbr_info* sbr, uint8_t ch);
void       hf_generation(sbr_info* sbr, qmf_t Xlow[MAX_NTSRHFG][64], qmf_t Xhigh[MAX_NTSRHFG][64], int32_t* deg, uint8_t ch);
void       sbr_envelope(bitfile* ld, sbr_info* sbr, uint8_t ch);
void       sbr_noise(bitfile* ld, sbr_info* sbr, uint8_t ch);
uint8_t    middleBorder(sbr_info* sbr, uint8_t ch);
#ifdef SSR_DEC
int32_t **pp_q0, **pp_t0, **pp_t1;
void            ssr_ipqf(ssr_info* ssr, int32_t* in_data, int32_t* out_data, int32_t buffer[SSR_BANDS][96 / 4], uint16_t frame_len, uint8_t bands);
void  gc_set_protopqf(int32_t* p_proto)
#endif
#ifdef PS_DEC
void      ps_data_decode(ps_info* ps);
hyb_info* hybrid_init(uint8_t numTimeSlotsRate);
void      channel_filter2(hyb_info* hyb, uint8_t frame_len, const int32_t* filter, qmf_t* buffer, qmf_t** X_hybrid);
void inline DCT3_4_unscaled(int32_t* y, int32_t* x);
void   channel_filter8(hyb_info* hyb, uint8_t frame_len, const int32_t* filter, qmf_t* buffer, qmf_t** X_hybrid);
void   hybrid_analysis(hyb_info* hyb, qmf_t X[32][64], qmf_t X_hybrid[32][32], uint8_t use34, uint8_t numTimeSlotsRate);
void   hybrid_synthesis(hyb_info* hyb, qmf_t X[32][64], qmf_t X_hybrid[32][32], uint8_t use34, uint8_t numTimeSlotsRate);
int8_t delta_clip(int8_t i, int8_t min, int8_t max);
void   delta_decode(uint8_t enable, int8_t* index, int8_t* index_prev, uint8_t dt_flag, uint8_t nr_par, uint8_t stride, int8_t min_index, int8_t max_index);
void   delta_modulo_decode(uint8_t enable, int8_t* index, int8_t* index_prev, uint8_t dt_flag, uint8_t nr_par, uint8_t stride, int8_t and_modulo);
void   map20indexto34(int8_t* index, uint8_t bins);
    #ifdef PS_LOW_POWER
void map34indexto20(int8_t* index, uint8_t bins);
    #endif
void ps_data_decode(ps_info* ps);
void ps_decorrelate(ps_info* ps, qmf_t X_left[38][64], qmf_t X_right[38][64], qmf_t X_hybrid_left[32][32], qmf_t X_hybrid_right[32][32]);
void ps_mix_phase(ps_info* ps, qmf_t X_left[38][64], qmf_t X_right[38][64], qmf_t X_hybrid_left[32][32], qmf_t X_hybrid_right[32][32]);
#endif //  PS_DEC
#ifdef PS_DEC
uint16_t             ps_extension(ps_info* ps, bitfile* ld, const uint8_t ps_extension_id, const uint16_t num_bits_left);
void                 huff_data(bitfile* ld, const uint8_t dt, const uint8_t nr_par, ps_huff_tab t_huff, ps_huff_tab f_huff, int8_t* par);
int8_t ps_huff_dec(bitfile* ld, ps_huff_tab t_huff);
#endif //  PS_DEC