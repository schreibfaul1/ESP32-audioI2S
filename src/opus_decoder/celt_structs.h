#pragma once

#include <stdint-gcc.h>
#include "celt_defines.h"

typedef struct {
    int32_t r;
    int32_t i;
} kiss_fft_cpx;

typedef struct {
    int16_t r;
    int16_t i;
} kiss_twiddle_cpx;

typedef struct kiss_fft_state {
    int32_t                 nfft;
    int16_t                 scale;
    int32_t                 scale_shift;
    int32_t                 shift;
    int16_t                 factors[2 * MAXFACTORS];
    const int16_t*          bitrev;
    const kiss_twiddle_cpx* twiddles;
} kiss_fft_state;

typedef struct {
    int32_t               n;
    int32_t               maxshift;
    const kiss_fft_state* kfft[4];
    const int16_t*        trig;
} mdct_lookup_t;

typedef struct {
    int32_t        size;
    const int16_t* index;
    const uint8_t* bits;
    const uint8_t* caps;
} PulseCache;

typedef struct _CELTMode {
    int32_t Fs;
    int32_t overlap;
    int32_t nbEBands;
    int32_t effEBands;
    int16_t preemph[4];
    int32_t maxLM;
    int32_t nbShortMdcts;
    int32_t shortMdctSize;
    int32_t nbAllocVectors; /**< Number of lines in the matrix below */
} CELTMode_t;

typedef struct _band_ctx {
    int32_t           resynth;
    const CELTMode_t* m;
    int32_t           i;
    int32_t           intensity;
    int32_t           spread;
    int32_t           tf_change;
    int32_t           remaining_bits;
    const int32_t*    bandE;
    uint32_t          seed;
    int32_t           theta_round;
    int32_t           disable_inv;
    int32_t           avoid_split_noise;
} band_ctx_t;

struct split_ctx {
    int32_t inv;
    int32_t imid;
    int32_t iside;
    int32_t delta;
    int32_t itheta;
    int32_t qalloc;
};

typedef struct _CELTDecoder {
    int32_t  overlap;
    int32_t  channels;
    int32_t  stream_channels;
    int32_t  downsample;
    int32_t  start, end;
    int32_t  signalling;
    int32_t  disable_inv;
    uint32_t rng;
    int32_t  error;
    int32_t  last_pitch_index;
    int32_t  loss_count;
    int32_t  skip_plc;
    int32_t  postfilter_period;
    int32_t  postfilter_period_old;
    int16_t  postfilter_gain;
    int16_t  postfilter_gain_old;
    int32_t  postfilter_tapset;
    int32_t  postfilter_tapset_old;
    int32_t  preemph_memD[2];
} CELTDecoder_t;


