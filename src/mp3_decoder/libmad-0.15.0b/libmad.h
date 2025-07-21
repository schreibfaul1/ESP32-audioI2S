// libmad.h

#pragma once

#include "Arduino.h"
#include "../mp3_decoder.h"
#include <vector>

static const char* mpeg_version_table[] = {
    "MPEG-1",    // MAD_MPEG_1 = 0
    "MPEG-2",    // MAD_MPEG_2 = 1
    "MPEG-2.5"   // MAD_MPEG_25 = 2
};

static const char* layer_table[] = {
    "Unknown",   // 0 (reserviert)
    "Layer I",   // MAD_LAYER_I = 1
    "Layer II",  // MAD_LAYER_II = 2
    "Layer III"  // MAD_LAYER_III = 3
};


bool allocateBuffers();
void clearBuffers();
void freeBuffers();
void mad_bit_init(struct mad_bitptr* bitptr, uint8_t const* byte);
uint32_t mad_bit_length(struct mad_bitptr const* begin, struct mad_bitptr const* end);
uint8_t const* mad_bit_nextbyte(struct mad_bitptr const* bitptr);
void mad_bit_skip(struct mad_bitptr* bitptr, uint32_t len);
uint32_t mad_bit_read(struct mad_bitptr* bitptr, uint32_t len);
uint16_t mad_bit_crc(struct mad_bitptr bitptr, uint32_t len, uint16_t init);
int32_t I_sample(struct mad_bitptr* ptr, uint32_t nb);
int32_t mad_layer_I(mad_stream_t* stream, mad_frame_t* frame);
void II_samples(struct mad_bitptr* ptr, struct quantclass const* quantclass, int32_t output[3]);
int32_t mad_layer_II(mad_stream_t* stream, mad_frame_t* frame);
char const* mad_stream_errorstr(mad_stream_t const* stream);
int32_t mad_stream_sync(mad_stream_t* stream);
void mad_stream_skip(mad_stream_t* stream, uint32_t length);
void mad_stream_buffer(mad_stream_t* stream, uint8_t const* buffer, uint32_t length);
void mad_stream_finish(mad_stream_t* stream);
void mad_stream_init(mad_stream_t* stream);
void mad_timer_string(mad_timer_t timer, char* dest, char const* format, enum mad_units units, enum mad_units fracunits, uint32_t subparts);
uint32_t mad_timer_fraction(mad_timer_t timer, uint32_t denom);
int32_t mad_timer_count(mad_timer_t timer, mad_units units);
void mad_timer_multiply(mad_timer_t* timer, int32_t scalar);
void mad_timer_add(mad_timer_t* timer, mad_timer_t incr);
void mad_timer_set(mad_timer_t* timer, uint32_t seconds, uint32_t numer, uint32_t denom);
uint32_t scale_rational(uint32_t numer, uint32_t denom, uint32_t scale);
void reduce_rational(uint32_t* numer, uint32_t* denom);
uint32_t gcd(uint32_t num1, uint32_t num2);
void reduce_timer(mad_timer_t* timer);
mad_timer_t mad_timer_abs(mad_timer_t timer);
void mad_timer_negate(mad_timer_t* timer);
int32_t mad_timer_compare(mad_timer_t timer1, mad_timer_t timer2);
void dct32(int32_t const in[32], uint32_t slot, int32_t lo[16][8], int32_t hi[16][8]);
void synth_full(mad_synth_t* synth, mad_frame_t const* frame, uint32_t nch, uint32_t ns);
void synth_half(mad_synth_t* synth, mad_frame_t const* frame, uint32_t nch, uint32_t ns);
void mad_synth_frame(mad_synth_t* synth, mad_frame_t const* frame);
void mad_synth_mute(mad_synth_t* synth);
void mad_synth_init(mad_synth_t* synth);
enum mad_error III_sideinfo(struct mad_bitptr* ptr, uint32_t nch, int32_t lsf, struct sideinfo* si, uint32_t* data_bitlen, uint32_t* priv_bitlen);
uint32_t III_scalefactors_lsf(struct mad_bitptr* ptr, struct channel* channel, struct channel* gr1ch, int32_t mode_extension);
uint32_t III_scalefactors(struct mad_bitptr* ptr, struct channel* channel, struct channel const* gr0ch, uint32_t scfsi);
void III_exponents(struct channel const* channel, uint8_t const* sfbwidth, int32_t exponents[39]);
int32_t III_requantize(uint32_t value, int32_t exp);
enum mad_error III_huffdecode(struct mad_bitptr* ptr, int32_t xr[576], struct channel* channel, uint8_t const* sfbwidth, uint32_t part2_length);
void III_reorder(int32_t xr[576], struct channel const* channel, uint8_t const sfbwidth[39]);
enum mad_error III_stereo(int32_t xr[2][576], struct granule const* granule, struct mad_header* header, uint8_t const* sfbwidth);
void III_aliasreduce(int32_t xr[576], int32_t lines);
void imdct36(int32_t const X[18], int32_t x[36]);
void III_imdct_l(int32_t const X[18], int32_t z[36], uint32_t block_type);
void III_imdct_s(int32_t const X[18], int32_t z[36]);
void III_overlap(int32_t const output[36], int32_t overlap[18], int32_t sample[18][32], uint32_t sb);
void III_overlap_z(int32_t overlap[18], int32_t sample[18][32], uint32_t sb);
void III_freqinver(int32_t sample[18][32], uint32_t sb);
enum mad_error III_decode(struct mad_bitptr* ptr, mad_frame_t* frame, struct sideinfo* si, uint32_t nch);
int32_t mad_layer_III(mad_stream_t* stream, mad_frame_t* frame);
signed short mad_fixed_to_short(int32_t sample);
void mad_header_init(struct mad_header* header);
void mad_frame_init(mad_frame_t* frame);
void mad_frame_finish(mad_frame_t* frame);
int32_t decode_header(struct mad_header* header, mad_stream_t* stream);
int32_t free_bitrate(mad_stream_t* stream, struct mad_header const* header);
int32_t mad_header_decode(struct mad_header* header, mad_stream_t* stream);
int32_t mad_frame_decode(mad_frame_t* frame, mad_stream_t* stream);
void mad_frame_mute(mad_frame_t* frame);
int32_t mad_fill_outbuff(int16_t *outSamples);
uint8_t mad_get_channels();
uint32_t mad_get_sample_rate();
uint32_t mad_get_bitrate();
int32_t mad_decode(uint8_t *data, int32_t *bytesLeft, int16_t *outSamples);
uint16_t mad_get_output_samps();
int32_t mad_find_syncword(uint8_t *buf, int32_t nBytes);
bool mad_parse_xing_header(uint8_t *buf, int32_t nBytes);
uint32_t mad_xing_bitrate();
uint32_t mad_xing_tota_bytes();
uint32_t mad_xing_duration_seconds();
uint32_t mad_xing_total_frames();
const char* mad_get_mpeg_version();
const char* mad_get_layer();

int32_t (*const decoder_table[3])(mad_stream_t*, mad_frame_t*) = {mad_layer_I, mad_layer_II, mad_layer_III};