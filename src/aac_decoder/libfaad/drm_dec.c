#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "fixed.h"
#ifdef DRM
#include "sbr_dec.h"
#include "drm_dec.h"

/* constants */
#define DECAY_CUTOFF         3
#define DECAY_SLOPE          0.05f
/* type definitaions */
typedef const int8_t (*drm_ps_huff_tab)[2];
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
/* function declarations */
void   drm_ps_sa_element(drm_ps_info* ps, bitfile* ld);
void   drm_ps_pan_element(drm_ps_info* ps, bitfile* ld);
int8_t huff_dec(bitfile* ld, drm_ps_huff_tab huff);
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
uint16_t drm_ps_data(drm_ps_info* ps, bitfile* ld) {
    uint16_t bits = (uint16_t)faad_get_processed_bits(ld);
    ps->drm_ps_data_available = 1;
    ps->bs_enable_sa = faad_get1bit(ld);
    ps->bs_enable_pan = faad_get1bit(ld);
    if(ps->bs_enable_sa) { drm_ps_sa_element(ps, ld); }
    if(ps->bs_enable_pan) { drm_ps_pan_element(ps, ld); }
    bits = (uint16_t)faad_get_processed_bits(ld) - bits;
    return bits;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_ps_sa_element(drm_ps_info* ps, bitfile* ld) {
    drm_ps_huff_tab huff;
    uint8_t         band;
    ps->bs_sa_dt_flag = faad_get1bit(ld);
    if(ps->bs_sa_dt_flag) { huff = t_huffman_sa; }
    else { huff = f_huffman_sa; }
    for(band = 0; band < DRM_NUM_SA_BANDS; band++) { ps->bs_sa_data[band] = huff_dec(ld, huff); }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_ps_pan_element(drm_ps_info* ps, bitfile* ld) {
    drm_ps_huff_tab huff;
    uint8_t         band;
    ps->bs_pan_dt_flag = faad_get1bit(ld);
    if(ps->bs_pan_dt_flag) { huff = t_huffman_pan; }
    else { huff = f_huffman_pan; }
    for(band = 0; band < DRM_NUM_PAN_BANDS; band++) { ps->bs_pan_data[band] = huff_dec(ld, huff); }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
/* binary search huffman decoding */
int8_t huff_dec(bitfile* ld, drm_ps_huff_tab huff) {
    uint8_t bit;
    int16_t index = 0;
    while(index >= 0) {
        bit = (uint8_t)faad_get1bit(ld);
        index = huff[index][bit];
    }
    return index + 15;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
int8_t sa_delta_clip(drm_ps_info* ps, int8_t i) {
    if(i < 0) {
        /*  printf(" SAminclip %d", i); */
        ps->sa_decode_error = 1;
        return 0;
    }
    else if(i > 7) {
        /*   printf(" SAmaxclip %d", i); */
        ps->sa_decode_error = 1;
        return 7;
    }
    else return i;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
int8_t pan_delta_clip(drm_ps_info* ps, int8_t i) {
    if(i < -7) {
        /* printf(" PANminclip %d", i); */
        ps->pan_decode_error = 1;
        return -7;
    }
    else if(i > 7) {
        /* printf(" PANmaxclip %d", i);  */
        ps->pan_decode_error = 1;
        return 7;
    }
    else return i;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_ps_delta_decode(drm_ps_info* ps) {
    uint8_t band;
    if(ps->bs_enable_sa) {
        if(ps->bs_sa_dt_flag && !ps->g_last_had_sa) {
            /* wait until we get a DT frame */
            ps->bs_enable_sa = 0;
        }
        else if(ps->bs_sa_dt_flag) {
            /* DT frame, we have a last frame, so we can decode */
            ps->g_sa_index[0] = sa_delta_clip(ps, ps->g_prev_sa_index[0] + ps->bs_sa_data[0]);
        }
        else {
            /* DF always decodable */
            ps->g_sa_index[0] = sa_delta_clip(ps, ps->bs_sa_data[0]);
        }
        for(band = 1; band < DRM_NUM_SA_BANDS; band++) {
            if(ps->bs_sa_dt_flag && ps->g_last_had_sa) { ps->g_sa_index[band] = sa_delta_clip(ps, ps->g_prev_sa_index[band] + ps->bs_sa_data[band]); }
            else if(!ps->bs_sa_dt_flag) { ps->g_sa_index[band] = sa_delta_clip(ps, ps->g_sa_index[band - 1] + ps->bs_sa_data[band]); }
        }
    }
    /* An error during SA decoding implies PAN data will be undecodable, too */
    /* Also, we don't like on/off switching in PS, so we force to last settings */
    if(ps->sa_decode_error) {
        ps->pan_decode_error = 1;
        ps->bs_enable_pan = ps->g_last_had_pan;
        ps->bs_enable_sa = ps->g_last_had_sa;
    }
    if(ps->bs_enable_sa) {
        if(ps->sa_decode_error) {
            for(band = 0; band < DRM_NUM_SA_BANDS; band++) { ps->g_sa_index[band] = ps->g_last_good_sa_index[band]; }
        }
        else {
            for(band = 0; band < DRM_NUM_SA_BANDS; band++) { ps->g_last_good_sa_index[band] = ps->g_sa_index[band]; }
        }
    }
    if(ps->bs_enable_pan) {
        if(ps->bs_pan_dt_flag && !ps->g_last_had_pan) { ps->bs_enable_pan = 0; }
        else if(ps->bs_pan_dt_flag) { ps->g_pan_index[0] = pan_delta_clip(ps, ps->g_prev_pan_index[0] + ps->bs_pan_data[0]); }
        else { ps->g_pan_index[0] = pan_delta_clip(ps, ps->bs_pan_data[0]); }
        for(band = 1; band < DRM_NUM_PAN_BANDS; band++) {
            if(ps->bs_pan_dt_flag && ps->g_last_had_pan) { ps->g_pan_index[band] = pan_delta_clip(ps, ps->g_prev_pan_index[band] + ps->bs_pan_data[band]); }
            else if(!ps->bs_pan_dt_flag) { ps->g_pan_index[band] = pan_delta_clip(ps, ps->g_pan_index[band - 1] + ps->bs_pan_data[band]); }
        }
        if(ps->pan_decode_error) {
            for(band = 0; band < DRM_NUM_PAN_BANDS; band++) { ps->g_pan_index[band] = ps->g_last_good_pan_index[band]; }
        }
        else {
            for(band = 0; band < DRM_NUM_PAN_BANDS; band++) { ps->g_last_good_pan_index[band] = ps->g_pan_index[band]; }
        }
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_calc_sa_side_signal(drm_ps_info* ps, qmf_t X[38][64]) {
    uint8_t   s, b, k;
    complex_t qfrac, tmp0, tmp, in, R0;
    real_t    peakdiff;
    real_t    nrg;
    real_t    power;
    real_t    transratio;
    real_t    new_delay_slopes[NUM_OF_LINKS];
    uint8_t   temp_delay_ser[NUM_OF_LINKS];
    complex_t Phi_Fract;
    #ifdef FIXED_POINT
    uint32_t in_re, in_im;
    #endif
    for(b = 0; b < sa_freq_scale[DRM_NUM_SA_BANDS]; b++) {
        /* set delay indices */
        for(k = 0; k < NUM_OF_LINKS; k++) temp_delay_ser[k] = ps->delay_buf_index_ser[k];
        RE(Phi_Fract) = RE(Phi_Fract_Qmf[b]);
        IM(Phi_Fract) = IM(Phi_Fract_Qmf[b]);
        for(s = 0; s < NUM_OF_SUBSAMPLES; s++) {
            const real_t gamma = REAL_CONST(1.5);
            const real_t sigma = REAL_CONST(1.5625);
            RE(in) = QMF_RE(X[s][b]);
            IM(in) = QMF_IM(X[s][b]);
    #ifdef FIXED_POINT
            /* NOTE: all input is scaled by 2^(-5) because of fixed point QMF
             * meaning that P will be scaled by 2^(-10) compared to floating point version
             */
            in_re = ((abs(RE(in)) + (1 << (REAL_BITS - 1))) >> REAL_BITS);
            in_im = ((abs(IM(in)) + (1 << (REAL_BITS - 1))) >> REAL_BITS);
            power = in_re * in_re + in_im * in_im;
    #else
            power = MUL_R(RE(in), RE(in)) + MUL_R(IM(in), IM(in));
    #endif
            ps->peakdecay_fast[b] = MUL_F(ps->peakdecay_fast[b], peak_decay);
            if(ps->peakdecay_fast[b] < power) ps->peakdecay_fast[b] = power;
            peakdiff = ps->prev_peakdiff[b];
            peakdiff += MUL_F((ps->peakdecay_fast[b] - power - ps->prev_peakdiff[b]), smooth_coeff);
            ps->prev_peakdiff[b] = peakdiff;
            nrg = ps->prev_nrg[b];
            nrg += MUL_F((power - ps->prev_nrg[b]), smooth_coeff);
            ps->prev_nrg[b] = nrg;
            if(MUL_R(peakdiff, gamma) <= nrg) { transratio = sigma; }
            else { transratio = MUL_R(DIV_R(nrg, MUL_R(peakdiff, gamma)), sigma); }
            for(k = 0; k < NUM_OF_LINKS; k++) { new_delay_slopes[k] = MUL_F(g_decayslope[b], filter_coeff[k]); }
            RE(tmp0) = RE(ps->d_buff[0][b]);
            IM(tmp0) = IM(ps->d_buff[0][b]);
            RE(ps->d_buff[0][b]) = RE(ps->d_buff[1][b]);
            IM(ps->d_buff[0][b]) = IM(ps->d_buff[1][b]);
            RE(ps->d_buff[1][b]) = RE(in);
            IM(ps->d_buff[1][b]) = IM(in);
            ComplexMult(&RE(tmp), &IM(tmp), RE(tmp0), IM(tmp0), RE(Phi_Fract), IM(Phi_Fract));
            RE(R0) = RE(tmp);
            IM(R0) = IM(tmp);
            for(k = 0; k < NUM_OF_LINKS; k++) {
                RE(qfrac) = RE(Q_Fract_allpass_Qmf[b][k]);
                IM(qfrac) = IM(Q_Fract_allpass_Qmf[b][k]);
                RE(tmp0) = RE(ps->d2_buff[k][temp_delay_ser[k]][b]);
                IM(tmp0) = IM(ps->d2_buff[k][temp_delay_ser[k]][b]);
                ComplexMult(&RE(tmp), &IM(tmp), RE(tmp0), IM(tmp0), RE(qfrac), IM(qfrac));
                RE(tmp) += -MUL_F(new_delay_slopes[k], RE(R0));
                IM(tmp) += -MUL_F(new_delay_slopes[k], IM(R0));
                RE(ps->d2_buff[k][temp_delay_ser[k]][b]) = RE(R0) + MUL_F(new_delay_slopes[k], RE(tmp));
                IM(ps->d2_buff[k][temp_delay_ser[k]][b]) = IM(R0) + MUL_F(new_delay_slopes[k], IM(tmp));
                RE(R0) = RE(tmp);
                IM(R0) = IM(tmp);
            }
            QMF_RE(ps->SA[s][b]) = MUL_R(RE(R0), transratio);
            QMF_IM(ps->SA[s][b]) = MUL_R(IM(R0), transratio);
            for(k = 0; k < NUM_OF_LINKS; k++) {
                if(++temp_delay_ser[k] >= delay_length[k]) temp_delay_ser[k] = 0;
            }
        }
    }
    for(k = 0; k < NUM_OF_LINKS; k++) ps->delay_buf_index_ser[k] = temp_delay_ser[k];
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_add_ambiance(drm_ps_info* ps, qmf_t X_left[38][64], qmf_t X_right[38][64]) {
    uint8_t s, b, ifreq, qclass;
    real_t  sa_map[MAX_SA_BAND], sa_dir_map[MAX_SA_BAND], k_sa_map[MAX_SA_BAND], k_sa_dir_map[MAX_SA_BAND];
    real_t  new_dir_map, new_sa_map;
    if(ps->bs_enable_sa) {
        /* Instead of dequantization and mapping, we use an inverse mapping
           to look up all the values we need */
        for(b = 0; b < sa_freq_scale[DRM_NUM_SA_BANDS]; b++) {
            const real_t inv_f_num_of_subsamples = FRAC_CONST(0.03333333333);
            ifreq = sa_inv_freq[b];
            qclass = (b != 0);
            sa_map[b] = sa_quant[ps->g_prev_sa_index[ifreq]][qclass];
            new_sa_map = sa_quant[ps->g_sa_index[ifreq]][qclass];
            k_sa_map[b] = MUL_F(inv_f_num_of_subsamples, (new_sa_map - sa_map[b]));
            sa_dir_map[b] = sa_sqrt_1_minus[ps->g_prev_sa_index[ifreq]][qclass];
            new_dir_map = sa_sqrt_1_minus[ps->g_sa_index[ifreq]][qclass];
            k_sa_dir_map[b] = MUL_F(inv_f_num_of_subsamples, (new_dir_map - sa_dir_map[b]));
        }
        for(s = 0; s < NUM_OF_SUBSAMPLES; s++) {
            for(b = 0; b < sa_freq_scale[DRM_NUM_SA_BANDS]; b++) {
                QMF_RE(X_right[s][b]) = MUL_F(QMF_RE(X_left[s][b]), sa_dir_map[b]) - MUL_F(QMF_RE(ps->SA[s][b]), sa_map[b]);
                QMF_IM(X_right[s][b]) = MUL_F(QMF_IM(X_left[s][b]), sa_dir_map[b]) - MUL_F(QMF_IM(ps->SA[s][b]), sa_map[b]);
                QMF_RE(X_left[s][b]) = MUL_F(QMF_RE(X_left[s][b]), sa_dir_map[b]) + MUL_F(QMF_RE(ps->SA[s][b]), sa_map[b]);
                QMF_IM(X_left[s][b]) = MUL_F(QMF_IM(X_left[s][b]), sa_dir_map[b]) + MUL_F(QMF_IM(ps->SA[s][b]), sa_map[b]);
                sa_map[b] += k_sa_map[b];
                sa_dir_map[b] += k_sa_dir_map[b];
            }
            for(b = sa_freq_scale[DRM_NUM_SA_BANDS]; b < NUM_OF_QMF_CHANNELS; b++) {
                QMF_RE(X_right[s][b]) = QMF_RE(X_left[s][b]);
                QMF_IM(X_right[s][b]) = QMF_IM(X_left[s][b]);
            }
        }
    }
    else {
        for(s = 0; s < NUM_OF_SUBSAMPLES; s++) {
            for(b = 0; b < NUM_OF_QMF_CHANNELS; b++) {
                QMF_RE(X_right[s][b]) = QMF_RE(X_left[s][b]);
                QMF_IM(X_right[s][b]) = QMF_IM(X_left[s][b]);
            }
        }
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_add_pan(drm_ps_info* ps, qmf_t X_left[38][64], qmf_t X_right[38][64]) {
    uint8_t s, b, qclass, ifreq;
    real_t  tmp, coeff1, coeff2;
    real_t  pan_base[MAX_PAN_BAND];
    real_t  pan_delta[MAX_PAN_BAND];
    qmf_t   temp_l, temp_r;
    if(ps->bs_enable_pan) {
        for(b = 0; b < NUM_OF_QMF_CHANNELS; b++) {
            /* Instead of dequantization, 20->64 mapping and 2^G(x,y) we do an
               inverse mapping 64->20 and look up the 2^G(x,y) values directly */
            ifreq = pan_inv_freq[b];
            qclass = pan_quant_class[ifreq];
            if(ps->g_prev_pan_index[ifreq] >= 0) { pan_base[b] = pan_pow_2_pos[ps->g_prev_pan_index[ifreq]][qclass]; }
            else { pan_base[b] = pan_pow_2_neg[-ps->g_prev_pan_index[ifreq]][qclass]; }
            /* 2^((a-b)/30) = 2^(a/30) * 1/(2^(b/30)) */
            /* a en b can be negative so we may need to inverse parts */
            if(ps->g_pan_index[ifreq] >= 0) {
                if(ps->g_prev_pan_index[ifreq] >= 0) { pan_delta[b] = MUL_C(pan_pow_2_30_pos[ps->g_pan_index[ifreq]][qclass], pan_pow_2_30_neg[ps->g_prev_pan_index[ifreq]][qclass]); }
                else { pan_delta[b] = MUL_C(pan_pow_2_30_pos[ps->g_pan_index[ifreq]][qclass], pan_pow_2_30_pos[-ps->g_prev_pan_index[ifreq]][qclass]); }
            }
            else {
                if(ps->g_prev_pan_index[ifreq] >= 0) { pan_delta[b] = MUL_C(pan_pow_2_30_neg[-ps->g_pan_index[ifreq]][qclass], pan_pow_2_30_neg[ps->g_prev_pan_index[ifreq]][qclass]); }
                else { pan_delta[b] = MUL_C(pan_pow_2_30_neg[-ps->g_pan_index[ifreq]][qclass], pan_pow_2_30_pos[-ps->g_prev_pan_index[ifreq]][qclass]); }
            }
        }
        for(s = 0; s < NUM_OF_SUBSAMPLES; s++) {
            /* PAN always uses all 64 channels */
            for(b = 0; b < NUM_OF_QMF_CHANNELS; b++) {
                tmp = pan_base[b];
                coeff2 = DIV_R(REAL_CONST(2.0), (REAL_CONST(1.0) + tmp));
                coeff1 = MUL_R(coeff2, tmp);
                QMF_RE(temp_l) = QMF_RE(X_left[s][b]);
                QMF_IM(temp_l) = QMF_IM(X_left[s][b]);
                QMF_RE(temp_r) = QMF_RE(X_right[s][b]);
                QMF_IM(temp_r) = QMF_IM(X_right[s][b]);
                QMF_RE(X_left[s][b]) = MUL_R(QMF_RE(temp_l), coeff1);
                QMF_IM(X_left[s][b]) = MUL_R(QMF_IM(temp_l), coeff1);
                QMF_RE(X_right[s][b]) = MUL_R(QMF_RE(temp_r), coeff2);
                QMF_IM(X_right[s][b]) = MUL_R(QMF_IM(temp_r), coeff2);
                /* 2^(a+k*b) = 2^a * 2^b * ... * 2^b */
                /*                   ^^^^^^^^^^^^^^^ k times */
                pan_base[b] = MUL_C(pan_base[b], pan_delta[b]);
            }
        }
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
drm_ps_info* drm_ps_init(void) {
    drm_ps_info* ps = (drm_ps_info*)faad_malloc(sizeof(drm_ps_info));
    memset(ps, 0, sizeof(drm_ps_info));
    return ps;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
void drm_ps_free(drm_ps_info* ps) { faad_free(ps); }
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
/* main DRM PS decoding function */
uint8_t drm_ps_decode(drm_ps_info* ps, uint8_t guess, qmf_t X_left[38][64], qmf_t X_right[38][64]) {
    if(ps == NULL) {
        memcpy(X_right, X_left, sizeof(qmf_t) * 30 * 64);
        return 0;
    }
    if(!ps->drm_ps_data_available && !guess) {
        memcpy(X_right, X_left, sizeof(qmf_t) * 30 * 64);
        memset(ps->g_prev_sa_index, 0, sizeof(ps->g_prev_sa_index));
        memset(ps->g_prev_pan_index, 0, sizeof(ps->g_prev_pan_index));
        return 0;
    }
    /* if SBR CRC doesn't match out, we can assume decode errors to start with,
       and we'll guess what the parameters should be */
    if(!guess) {
        ps->sa_decode_error = 0;
        ps->pan_decode_error = 0;
        drm_ps_delta_decode(ps);
    }
    else {
        ps->sa_decode_error = 1;
        ps->pan_decode_error = 1;
        /* don't even bother decoding */
    }
    ps->drm_ps_data_available = 0;
    drm_calc_sa_side_signal(ps, X_left);
    drm_add_ambiance(ps, X_left, X_right);
    if(ps->bs_enable_sa) {
        ps->g_last_had_sa = 1;
        memcpy(ps->g_prev_sa_index, ps->g_sa_index, sizeof(int8_t) * DRM_NUM_SA_BANDS);
    }
    else { ps->g_last_had_sa = 0; }
    if(ps->bs_enable_pan) {
        drm_add_pan(ps, X_left, X_right);
        ps->g_last_had_pan = 1;
        memcpy(ps->g_prev_pan_index, ps->g_pan_index, sizeof(int8_t) * DRM_NUM_PAN_BANDS);
    }
    else { ps->g_last_had_pan = 0; }
    return 0;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————