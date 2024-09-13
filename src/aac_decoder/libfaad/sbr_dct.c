#include "common.h"
#ifdef SBR_DEC
#ifdef _MSC_VER
#pragma warning(disable:4305)
#pragma warning(disable:4244)
#endif
#include "sbr_dct.h"
#endif // SBR_DEC
#ifdef SBR_DEC
    #ifndef SBR_LOW_POWER
        #undef n
        #undef log2n
static const real_t dct4_64_tab[] = {COEF_CONST(0.999924719333649),
                                     COEF_CONST(0.998118102550507),
                                     COEF_CONST(0.993906974792480),
                                     COEF_CONST(0.987301409244537),
                                     COEF_CONST(0.978317379951477),
                                     COEF_CONST(0.966976463794708),
                                     COEF_CONST(0.953306019306183),
                                     COEF_CONST(0.937339007854462),
                                     COEF_CONST(0.919113874435425),
                                     COEF_CONST(0.898674488067627),
                                     COEF_CONST(0.876070082187653),
                                     COEF_CONST(0.851355195045471),
                                     COEF_CONST(0.824589252471924),
                                     COEF_CONST(0.795836925506592),
                                     COEF_CONST(0.765167236328125),
                                     COEF_CONST(0.732654273509979),
                                     COEF_CONST(0.698376238346100),
                                     COEF_CONST(0.662415742874146),
                                     COEF_CONST(0.624859452247620),
                                     COEF_CONST(0.585797846317291),
                                     COEF_CONST(0.545324981212616),
                                     COEF_CONST(0.503538429737091),
                                     COEF_CONST(0.460538715124130),
                                     COEF_CONST(0.416429549455643),
                                     COEF_CONST(0.371317148208618),
                                     COEF_CONST(0.325310230255127),
                                     COEF_CONST(0.278519600629807),
                                     COEF_CONST(0.231058135628700),
                                     COEF_CONST(0.183039888739586),
                                     COEF_CONST(0.134580686688423),
                                     COEF_CONST(0.085797272622585),
                                     COEF_CONST(0.036807164549828),
                                     COEF_CONST(-1.012196302413940),
                                     COEF_CONST(-1.059438824653626),
                                     COEF_CONST(-1.104129195213318),
                                     COEF_CONST(-1.146159529685974),
                                     COEF_CONST(-1.185428738594055),
                                     COEF_CONST(-1.221842169761658),
                                     COEF_CONST(-1.255311965942383),
                                     COEF_CONST(-1.285757660865784),
                                     COEF_CONST(-1.313105940818787),
                                     COEF_CONST(-1.337290763854981),
                                     COEF_CONST(-1.358253836631775),
                                     COEF_CONST(-1.375944852828980),
                                     COEF_CONST(-1.390321016311646),
                                     COEF_CONST(-1.401347875595093),
                                     COEF_CONST(-1.408998727798462),
                                     COEF_CONST(-1.413255214691162),
                                     COEF_CONST(-1.414107084274292),
                                     COEF_CONST(-1.411552190780640),
                                     COEF_CONST(-1.405596733093262),
                                     COEF_CONST(-1.396255016326904),
                                     COEF_CONST(-1.383549690246582),
                                     COEF_CONST(-1.367511272430420),
                                     COEF_CONST(-1.348178386688232),
                                     COEF_CONST(-1.325597524642944),
                                     COEF_CONST(-1.299823284149170),
                                     COEF_CONST(-1.270917654037476),
                                     COEF_CONST(-1.238950133323669),
                                     COEF_CONST(-1.203998088836670),
                                     COEF_CONST(-1.166145324707031),
                                     COEF_CONST(-1.125483393669128),
                                     COEF_CONST(-1.082109928131104),
                                     COEF_CONST(-1.036129593849182),
                                     COEF_CONST(-0.987653195858002),
                                     COEF_CONST(-0.936797380447388),
                                     COEF_CONST(-0.883684754371643),
                                     COEF_CONST(-0.828443288803101),
                                     COEF_CONST(-0.771206021308899),
                                     COEF_CONST(-0.712110757827759),
                                     COEF_CONST(-0.651300072669983),
                                     COEF_CONST(-0.588920354843140),
                                     COEF_CONST(-0.525121808052063),
                                     COEF_CONST(-0.460058242082596),
                                     COEF_CONST(-0.393886327743530),
                                     COEF_CONST(-0.326765477657318),
                                     COEF_CONST(-0.258857429027557),
                                     COEF_CONST(-0.190325915813446),
                                     COEF_CONST(-0.121335685253143),
                                     COEF_CONST(-0.052053272724152),
                                     COEF_CONST(0.017354607582092),
                                     COEF_CONST(0.086720645427704),
                                     COEF_CONST(0.155877828598022),
                                     COEF_CONST(0.224659323692322),
                                     COEF_CONST(0.292899727821350),
                                     COEF_CONST(0.360434412956238),
                                     COEF_CONST(0.427100926637650),
                                     COEF_CONST(0.492738455533981),
                                     COEF_CONST(0.557188928127289),
                                     COEF_CONST(0.620297133922577),
                                     COEF_CONST(0.681910991668701),
                                     COEF_CONST(0.741881847381592),
                                     COEF_CONST(0.800065577030182),
                                     COEF_CONST(0.856321990489960),
                                     COEF_CONST(0.910515367984772),
                                     COEF_CONST(0.962515234947205),
                                     COEF_CONST(1.000000000000000),
                                     COEF_CONST(0.998795449733734),
                                     COEF_CONST(0.995184719562531),
                                     COEF_CONST(0.989176511764526),
                                     COEF_CONST(0.980785250663757),
                                     COEF_CONST(0.970031261444092),
                                     COEF_CONST(0.956940352916718),
                                     COEF_CONST(0.941544055938721),
                                     COEF_CONST(0.923879504203796),
                                     COEF_CONST(0.903989315032959),
                                     COEF_CONST(0.881921231746674),
                                     COEF_CONST(0.857728600502014),
                                     COEF_CONST(0.831469595432281),
                                     COEF_CONST(0.803207516670227),
                                     COEF_CONST(0.773010432720184),
                                     COEF_CONST(0.740951120853424),
                                     COEF_CONST(0.707106769084930),
                                     COEF_CONST(0.671558916568756),
                                     COEF_CONST(0.634393274784088),
                                     COEF_CONST(0.595699310302734),
                                     COEF_CONST(0.555570185184479),
                                     COEF_CONST(0.514102697372437),
                                     COEF_CONST(0.471396654844284),
                                     COEF_CONST(0.427555114030838),
                                     COEF_CONST(0.382683426141739),
                                     COEF_CONST(0.336889833211899),
                                     COEF_CONST(0.290284633636475),
                                     COEF_CONST(0.242980122566223),
                                     COEF_CONST(0.195090234279633),
                                     COEF_CONST(0.146730497479439),
                                     COEF_CONST(0.098017133772373),
                                     COEF_CONST(0.049067649990320),
                                     COEF_CONST(-1.000000000000000),
                                     COEF_CONST(-1.047863125801086),
                                     COEF_CONST(-1.093201875686646),
                                     COEF_CONST(-1.135906934738159),
                                     COEF_CONST(-1.175875544548035),
                                     COEF_CONST(-1.213011503219605),
                                     COEF_CONST(-1.247225046157837),
                                     COEF_CONST(-1.278433918952942),
                                     COEF_CONST(-1.306562900543213),
                                     COEF_CONST(-1.331544399261475),
                                     COEF_CONST(-1.353317975997925),
                                     COEF_CONST(-1.371831417083740),
                                     COEF_CONST(-1.387039899826050),
                                     COEF_CONST(-1.398906826972961),
                                     COEF_CONST(-1.407403707504273),
                                     COEF_CONST(-1.412510156631470),
                                     COEF_CONST(0),
                                     COEF_CONST(-1.412510156631470),
                                     COEF_CONST(-1.407403707504273),
                                     COEF_CONST(-1.398906826972961),
                                     COEF_CONST(-1.387039899826050),
                                     COEF_CONST(-1.371831417083740),
                                     COEF_CONST(-1.353317975997925),
                                     COEF_CONST(-1.331544399261475),
                                     COEF_CONST(-1.306562900543213),
                                     COEF_CONST(-1.278433918952942),
                                     COEF_CONST(-1.247225046157837),
                                     COEF_CONST(-1.213011384010315),
                                     COEF_CONST(-1.175875544548035),
                                     COEF_CONST(-1.135907053947449),
                                     COEF_CONST(-1.093201875686646),
                                     COEF_CONST(-1.047863125801086),
                                     COEF_CONST(-1.000000000000000),
                                     COEF_CONST(-0.949727773666382),
                                     COEF_CONST(-0.897167563438416),
                                     COEF_CONST(-0.842446029186249),
                                     COEF_CONST(-0.785694956779480),
                                     COEF_CONST(-0.727051079273224),
                                     COEF_CONST(-0.666655659675598),
                                     COEF_CONST(-0.604654192924500),
                                     COEF_CONST(-0.541196048259735),
                                     COEF_CONST(-0.476434230804443),
                                     COEF_CONST(-0.410524487495422),
                                     COEF_CONST(-0.343625843524933),
                                     COEF_CONST(-0.275899350643158),
                                     COEF_CONST(-0.207508206367493),
                                     COEF_CONST(-0.138617098331451),
                                     COEF_CONST(-0.069392144680023),
                                     COEF_CONST(0),
                                     COEF_CONST(0.069392263889313),
                                     COEF_CONST(0.138617157936096),
                                     COEF_CONST(0.207508206367493),
                                     COEF_CONST(0.275899469852448),
                                     COEF_CONST(0.343625962734222),
                                     COEF_CONST(0.410524636507034),
                                     COEF_CONST(0.476434201002121),
                                     COEF_CONST(0.541196107864380),
                                     COEF_CONST(0.604654192924500),
                                     COEF_CONST(0.666655719280243),
                                     COEF_CONST(0.727051138877869),
                                     COEF_CONST(0.785695075988770),
                                     COEF_CONST(0.842446029186249),
                                     COEF_CONST(0.897167563438416),
                                     COEF_CONST(0.949727773666382)};
    #endif // SBR_LOW_POWER
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
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
