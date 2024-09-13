#include "common.h"
#include "structs.h"
#ifdef SSR_DEC
#include "ssr.h"
#include "ssr_ipqf.h"
//static real_t** app_pqfbuf; 
static real_t **pp_q0, **pp_t0, **pp_t1;



void ssr_ipqf(ssr_info *ssr, real_t *in_data, real_t *out_data,
              real_t buffer[SSR_BANDS][96/4],
              uint16_t frame_len, uint8_t bands);



#endif

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SSR_DEC
void gc_setcoef_eff_pqfsyn(int mm, int kk, real_t* p_proto, real_t*** ppp_q0, real_t*** ppp_t0, real_t*** ppp_t1) {
    int    i, k, n;
    real_t w;
    /* Set 1st Mul&Acc Coef's */
    *ppp_q0 = (real_t**)calloc(mm, sizeof(real_t*));
    for (n = 0; n < mm; ++n) { (*ppp_q0)[n] = (real_t*)calloc(mm, sizeof(real_t)); }
    for (n = 0; n < mm / 2; ++n) {
        for (i = 0; i < mm; ++i) {
            w = (2 * i + 1) * (2 * n + 1 - mm) * M_PI / (4 * mm);
            (*ppp_q0)[n][i] = 2.0 * cos((real_t)w);
            w = (2 * i + 1) * (2 * (mm + n) + 1 - mm) * M_PI / (4 * mm);
            (*ppp_q0)[n + mm / 2][i] = 2.0 * cos((real_t)w);
        }
    }
    /* Set 2nd Mul&Acc Coef's */
    *ppp_t0 = (real_t**)calloc(mm, sizeof(real_t*));
    *ppp_t1 = (real_t**)calloc(mm, sizeof(real_t*));
    for (n = 0; n < mm; ++n) {
        (*ppp_t0)[n] = (real_t*)calloc(kk, sizeof(real_t));
        (*ppp_t1)[n] = (real_t*)calloc(kk, sizeof(real_t));
    }
    for (n = 0; n < mm; ++n) {
        for (k = 0; k < kk; ++k) {
            (*ppp_t0)[n][k] = mm * p_proto[2 * k * mm + n];
            (*ppp_t1)[n][k] = mm * p_proto[(2 * k + 1) * mm + n];
            if (k % 2 != 0) {
                (*ppp_t0)[n][k] = -(*ppp_t0)[n][k];
                (*ppp_t1)[n][k] = -(*ppp_t1)[n][k];
            }
        }
    }
}
#endif
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
