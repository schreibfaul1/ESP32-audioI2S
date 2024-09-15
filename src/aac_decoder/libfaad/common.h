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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* COMPILE TIME DEFINITIONS */
#define PREFER_POINTERS // Use if target platform has address generators with autoincrement
// #define BIG_IQ_TABLE
// #define USE_DOUBLE_PRECISION // use double precision
#define FIXED_POINT          // use fixed point reals
#define ERROR_RESILIENCE 2
#define MAIN_DEC // Allow decoding of MAIN profile AAC
// #define SSR_DEC // Allow decoding of SSR profile AAC
#define LTP_DEC // Allow decoding of LTP (Long Term Prediction) profile AAC
#define LD_DEC  // Allow decoding of LD (Low Delay) profile AAC
// #define DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
#define SBR_DEC // Allow decoding of SBR (Spectral Band Replication) profile AAC
// #define SBR_LOW_POWER
#define PS_DEC // Allow decoding of PS (Parametric Stereo) profile AAC
#define ALLOW_SMALL_FRAMELENGTH
// #define LC_ONLY_DECODER // if you want a pure AAC LC decoder (independant of SBR_DEC and PS_DEC)
#ifdef DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
    #define DRM
    #define DRM_PS
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



//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* defines if an object type can be decoded by this library or not */
__unused static uint8_t ObjectTypesTable[32] = {
    0, /*  0 NULL */
#ifdef MAIN_DEC
    1, /*  1 AAC Main */
#else
    0, /*  1 AAC Main */
#endif
    1, /*  2 AAC LC */
#ifdef SSR_DEC
    1, /*  3 AAC SSR */
#else
    0, /*  3 AAC SSR */
#endif
#ifdef LTP_DEC
    1, /*  4 AAC LTP */
#else
    0, /*  4 AAC LTP */
#endif
#ifdef SBR_DEC
    1, /*  5 SBR */
#else
    0, /*  5 SBR */
#endif
    0, /*  6 AAC Scalable */
    0, /*  7 TwinVQ */
    0, /*  8 CELP */
    0, /*  9 HVXC */
    0, /* 10 Reserved */
    0, /* 11 Reserved */
    0, /* 12 TTSI */
    0, /* 13 Main synthetic */
    0, /* 14 Wavetable synthesis */
    0, /* 15 General MIDI */
    0, /* 16 Algorithmic Synthesis and Audio FX */
/* MPEG-4 Version 2 */
#ifdef ERROR_RESILIENCE
    1, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    #ifdef LTP_DEC
    1, /* 19 ER AAC LTP */
    #else
    0, /* 19 ER AAC LTP */
    #endif
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    #ifdef LD_DEC
    1, /* 23 ER AAC LD */
    #else
    0, /* 23 ER AAC LD */
    #endif
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#else  /* No ER defined */
    0, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    0, /* 19 ER AAC LTP */
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    0, /* 23 ER AAC LD */
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#endif
    0, /* 28 (Reserved) */
#ifdef PS_DEC
    1, /* 29 AAC LC + SBR + PS */
#else
    0, /* 29 AAC LC + SBR + PS */
#endif
    0, /* 30 (Reserved) */
    0  /* 31 (Reserved) */
};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
