#pragma once

/* ----------------------COMPILE TIME DEFINITIONS ---------------- */
#define PREFER_POINTERS // Use if target platform has address generators with autoincrement
// #define BIG_IQ_TABLE
// #define USE_DOUBLE_PRECISION // use double precision
#define FIXED_POINT          // use fixed point reals, undefs MAIN_DEC and SSR_DEC
#define ERROR_RESILIENCE 2
#define MAIN_DEC // Allow decoding of MAIN profile AAC
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

