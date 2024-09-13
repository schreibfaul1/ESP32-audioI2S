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
** $Id: sbr_dec.h,v 1.39 2007/11/01 12:33:34 menno Exp $
**/
#ifndef __SBR_DEC_H__
#define __SBR_DEC_H__
#include "common.h"
#include "fixed.h"



#ifdef __cplusplus
extern "C" {
#endif
#ifdef PS_DEC
#include "ps_dec.h"
#endif
#ifdef DRM_PS
#include "drm_dec.h"
#endif
/* MAX_NTSRHFG: maximum of number_time_slots * rate + HFGen. 16*2+8 */
#define MAX_NTSRHFG 40
#define MAX_NTSR    32 /* max number_time_slots * rate, ok for DRM and not DRM mode */
/* MAX_M: maximum value for M */
#define MAX_M       49
/* MAX_L_E: maximum value for L_E */
#define MAX_L_E      5
typedef struct {
    real_t *x;
    int16_t x_index;
    uint8_t channels;
} qmfa_info;
typedef struct {
    real_t *v;
    int16_t v_index;
    uint8_t channels;
} qmfs_info;



#ifdef __cplusplus
}
#endif
#endif
