#include "common.h"
#ifdef SBR_DEC
#ifdef _MSC_VER
#pragma warning(disable:4305)
#pragma warning(disable:4244)
#endif

#endif // SBR_DEC
#ifdef SBR_DEC
    #ifndef SBR_LOW_POWER
/* size 64 only! */
void dct4_kernel(real_t* in_real, real_t* in_imag, real_t* out_real, real_t* out_imag) {
    // Tables with bit reverse values for 5 bits, bit reverse of i at i-th position
    const uint8_t bit_rev_tab[32] = {0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30, 1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31};
    uint32_t      i, i_rev;
    /* Step 2: modulate */
    // 3*32=96 multiplications
    // 3*32=96 additions
    for (i = 0; i < 32; i++) {
        real_t x_re, x_im, tmp;
        x_re = in_real[i];
        x_im = in_imag[i];
        tmp = MUL_C(x_re + x_im, dct4_64_tab[i]);
        in_real[i] = MUL_C(x_im, dct4_64_tab[i + 64]) + tmp;
        in_imag[i] = MUL_C(x_re, dct4_64_tab[i + 32]) + tmp;
    }
    /* Step 3: FFT, but with output in bit reverse order */
    fft_dif(in_real, in_imag);
    /* Step 4: modulate + bitreverse reordering */
    // 3*31+2=95 multiplications
    // 3*31+2=95 additions
    for (i = 0; i < 16; i++) {
        real_t x_re, x_im, tmp;
        i_rev = bit_rev_tab[i];
        x_re = in_real[i_rev];
        x_im = in_imag[i_rev];
        tmp = MUL_C(x_re + x_im, dct4_64_tab[i + 3 * 32]);
        out_real[i] = MUL_C(x_im, dct4_64_tab[i + 5 * 32]) + tmp;
        out_imag[i] = MUL_C(x_re, dct4_64_tab[i + 4 * 32]) + tmp;
    }
    // i = 16, i_rev = 1 = rev(16);
    out_imag[16] = MUL_C(in_imag[1] - in_real[1], dct4_64_tab[16 + 3 * 32]);
    out_real[16] = MUL_C(in_real[1] + in_imag[1], dct4_64_tab[16 + 3 * 32]);
    for (i = 17; i < 32; i++) {
        real_t x_re, x_im, tmp;
        i_rev = bit_rev_tab[i];
        x_re = in_real[i_rev];
        x_im = in_imag[i_rev];
        tmp = MUL_C(x_re + x_im, dct4_64_tab[i + 3 * 32]);
        out_real[i] = MUL_C(x_im, dct4_64_tab[i + 5 * 32]) + tmp;
        out_imag[i] = MUL_C(x_re, dct4_64_tab[i + 4 * 32]) + tmp;
    }
}
    #endif // SBR_LOW_POWER
#endif //  SBR_DEC
