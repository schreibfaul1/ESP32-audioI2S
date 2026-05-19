#pragma once

/* ----------------------COMPILE TIME DEFINITIONS ---------------- */
#define PREFER_POINTERS // Use if target platform has address generators with autoincrement
// #define BIG_IQ_TABLE
// #define USE_DOUBLE_PRECISION // use double precision
// #define FIXED_POINT          // use fixed point reals, undefs MAIN_DEC and SSR_DEC
// #define ERROR_RESILIENCE 2
// #define MAIN_DEC // Allow decoding of MAIN profile AAC
// #define SSR_DEC // Allow decoding of SSR profile AAC
#define LTP_DEC // Allow decoding of LTP (Long Term Prediction) profile AAC
#define LD_DEC  // Allow decoding of LD (Low Delay) profile AAC
// #define DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
#if (defined CONFIG_IDF_TARGET_ESP32S3 || defined CONFIG_IDF_TARGET_ESP32P4)
    #define SBR_DEC // Allow decoding of SBR (Spectral Band Replication) profile AAC
    #define PS_DEC // Allow decoding of PS (Parametric Stereo) profile AAC
#endif
// #define SBR_LOW_POWER
#define ALLOW_SMALL_FRAMELENGTH
// #define LC_ONLY_DECODER // if you want a pure AAC LC decoder (independant of SBR_DEC and PS_DEC)
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifdef DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
    #define DRM
    #define DRM_PS
    #undef  PS_DEC
#endif
#ifdef LD_DEC /* LD can't do without LTP */
    #ifndef ERROR_RESILIENCE
        #define ERROR_RESILIENCE
    #endif
    #ifndef LTP_DEC
        #define LTP_DEC
    #endif
#endif
#ifdef LC_ONLY_DECODER
    #undef LD_DEC
    #undef LTP_DEC
    #undef MAIN_DEC
    #undef SSR_DEC
    #undef DRM
    #undef DRM_PS
    #undef ALLOW_SMALL_FRAMELENGTH
    #undef ERROR_RESILIENCE
#endif
#ifdef SBR_LOW_POWER
    #undef PS_DEC
#endif
#ifdef FIXED_POINT /*  No MAIN decoding */
    #ifdef MAIN_DEC
        #undef MAIN_DEC
    #endif
#endif // FIXED_POINT
#ifdef DRM
    #ifndef ALLOW_SMALL_FRAMELENGTH
        #define ALLOW_SMALL_FRAMELENGTH
    #endif
    #undef LD_DEC
    #undef LTP_DEC
    #undef MAIN_DEC
    #undef SSR_DEC
#endif
/* END COMPILE TIME DEFINITIONS */


#ifdef WORDS_BIGENDIAN
    #define ARCH_IS_BIG_ENDIAN
#endif
/* FIXED_POINT doesn't work with MAIN and SSR yet */
#ifdef FIXED_POINT
    #undef MAIN_DEC
    #undef SSR_DEC
#endif
#if defined(FIXED_POINT)
#elif defined(USE_DOUBLE_PRECISION)
#else                                   /* Normal floating point operation */
    #ifdef HAVE_LRINTF
        #define HAS_LRINTF
        #define _ISOC9X_SOURCE 1
        #define _ISOC99_SOURCE 1
        #define __USE_ISOC9X   1
        #define __USE_ISOC99   1
    #endif


#endif
#ifndef HAS_LRINTF
/* standard cast */
// #define int32_t(f) ((int32_t)(f))
#endif