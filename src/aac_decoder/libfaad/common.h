/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: common.h,v 1.79 2015/01/26 17:48:53 knik Exp $
**/
#pragma once
#define STDC_HEADERS    1
#define HAVE_STDLIB_H   1
#define HAVE_STRING_H   1
#define HAVE_INTTYPES_H 1
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "neaacdec.h"



#if 0 // defined(_WIN32) && !defined(_WIN32_WCE)
    #define ALIGN __declspec(align(16))
#else
    #define ALIGN
#endif
/* COMPILE TIME DEFINITIONS */

// #define PREFER_POINTERS // Use if target platform has address generators with autoincrement
// #define BIG_IQ_TABLE
// #define USE_DOUBLE_PRECISION // use double precision
// #define FIXED_POINT          // use fixed point reals
#define ERROR_RESILIENCE 2
#define MAIN_DEC // Allow decoding of MAIN profile AAC
// #define SSR_DEC // Allow decoding of SSR profile AAC
#define LTP_DEC // Allow decoding of LTP (Long Term Prediction) profile AAC
#define LD_DEC  // Allow decoding of LD (Low Delay) profile AAC
// #define DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
#define SBR_DEC // Allow decoding of SBR (Spectral Band Replication) profile AAC
#define SBR_LOW_POWER
#define PS_DEC // Allow decoding of PS (Parametric Stereo) profile AAC
#define ALLOW_SMALL_FRAMELENGTH
// #define LC_ONLY_DECODER // if you want a pure AAC LC decoder (independant of SBR_DEC and PS_DEC)

#ifdef DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
    #define DRM
    #define DRM_PS
#endif

#ifdef LD_DEC // LD can't do without LTP
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

#include <stdio.h>
#if HAVE_SYS_TYPES_H
    #include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
    #include <sys/stat.h>
#endif
#if STDC_HEADERS

#else
    #if HAVE_STDLIB_H
        #include <stdlib.h>
    #endif
#endif
#if HAVE_STRING_H
    #if !STDC_HEADERS && HAVE_MEMORY_H
        #include <memory.h>
    #endif
    #include <string.h>
#endif
#if HAVE_STRINGS_H
    #include <strings.h>
#endif
#if HAVE_INTTYPES_H
    #include <inttypes.h>
#else
    #if HAVE_STDINT_H
        #include <stdint.h>
    #else
        /* we need these... */
        #ifndef __TCS__
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;
        #else
typedef unsigned long uint64_t;
typedef signed long   int64_t;
        #endif
typedef unsigned long  uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef signed long    int32_t;
typedef signed short   int16_t;
typedef signed char    int8_t;
    #endif
#endif
#if HAVE_UNISTD_H
// # include <unistd.h>
#endif
#ifndef HAVE_FLOAT32_T
typedef float float32_t;
#endif
#if STDC_HEADERS

#else
    #if !HAVE_STRCHR
        #define strchr  index
        #define strrchr rindex
    #endif
char *strchr(), *strrchr();
    #if !HAVE_MEMCPY
        #define memcpy(d, s, n)  bcopy((s), (d), (n))
        #define memmove(d, s, n) bcopy((s), (d), (n))
    #endif
#endif

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
    #include <math.h>
    #ifdef HAVE_SINF
        #define sin sinf
        #error
    #endif
    #ifdef HAVE_COSF
        #define cos cosf
    #endif
    #ifdef HAVE_LOGF
        #define log logf
    #endif
    #ifdef HAVE_EXPF
        #define exp expf
    #endif
    #ifdef HAVE_FLOORF
        #define floor floorf
    #endif
    #ifdef HAVE_CEILF
        #define ceil ceilf
    #endif
    #ifdef HAVE_SQRTF
        #define sqrt sqrtf
    #endif

#endif
#ifndef HAS_LRINTF
/* standard cast */
// #define int32_t(f) ((int32_t)(f))
#endif

/* common functions */
uint8_t  cpu_has_sse(void);
uint32_t ne_rng(uint32_t* __r1, uint32_t* __r2);
uint32_t wl_min_lzc(uint32_t x);
#ifdef FIXED_POINT
    #define LOG2_MIN_INF REAL_CONST(-10000)
int32_t log2_int(uint32_t val);
int32_t log2_fix(uint32_t val);
int32_t pow2_int(real_t val);
real_t  pow2_fix(real_t val);
#endif
uint8_t  get_sr_index(const uint32_t samplerate);
uint8_t  max_pred_sfb(const uint8_t sr_index);
uint8_t  max_tns_sfb(const uint8_t sr_index, const uint8_t object_type, const uint8_t is_short);
uint32_t get_sample_rate(const uint8_t sr_index);
int8_t   can_decode_ot(const uint8_t object_type);
void*    faad_malloc(size_t size);
void     faad_free(void* b);

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif


#ifndef max
    #define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
    #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef __cplusplus
}
#endif