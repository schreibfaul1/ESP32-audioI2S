

#ifndef __SSR_H__
#define __SSR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SSR_BANDS 4
#define PQFTAPS 96
#include "syntax.h"
#include "ssr.h"
#include "structs.h"


#ifdef SSR_DEC
void ssr_decode(ssr_info *ssr, fb_info *fb, uint8_t window_sequence,
                uint8_t window_shape, uint8_t window_shape_prev,
                real_t *freq_in, real_t *time_out, real_t *overlap,
                real_t ipqf_buffer[SSR_BANDS][96/4],
                real_t *prev_fmd, uint16_t frame_len);

__unused void ssr_gain_control(ssr_info* ssr, real_t* data, real_t* output, real_t* overlap, real_t* prev_fmd, uint8_t band, uint8_t window_sequence, uint16_t frame_len);
__unused void ssr_gc_function(ssr_info* ssr, real_t* prev_fmd, real_t* gc_function, uint8_t window_sequence, uint16_t frame_len);

#endif
#ifdef __cplusplus
}
#endif
#endif
