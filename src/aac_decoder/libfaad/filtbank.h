#ifndef __FILTBANK_H__
#define __FILTBANK_H__
#include "fixed.h"
#ifdef __cplusplus
extern "C" {
#endif
fb_info *filter_bank_init(uint16_t frame_len);
void filter_bank_end(fb_info *fb);
#ifdef LTP_DEC
void filter_bank_ltp(fb_info *fb,
                     uint8_t window_sequence,
                     uint8_t window_shape,
                     uint8_t window_shape_prev,
                     real_t *in_data,
                     real_t *out_mdct,
                     uint8_t object_type,
                     uint16_t frame_len);
#endif
void ifilter_bank(fb_info *fb, uint8_t window_sequence, uint8_t window_shape,
                  uint8_t window_shape_prev, real_t *freq_in,
                  real_t *time_out, real_t *overlap,
                  uint8_t object_type, uint16_t frame_len);
#ifdef __cplusplus
}
#endif
#endif
