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
** $Id: ps_tables.h,v 1.8 2007/11/01 12:33:33 menno Exp $
**/
#ifndef __PS_TABLES_H__
#define __PS_TABLES_H__
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _MSC_VER
#pragma warning(disable:4305)
#pragma warning(disable:4244)
#endif
#if 0
#if 0
float f_center_20[12] = {
    0.5/4,  1.5/4,  2.5/4,  3.5/4,
    4.5/4*0,  5.5/4*0, -1.5/4, -0.5/4,
    3.5/2,  2.5/2,  4.5/2,  5.5/2
};
#else
float f_center_20[12] = {
    0.5/8,  1.5/8,  2.5/8,  3.5/8,
    4.5/8*0,  5.5/8*0, -1.5/8, -0.5/8,
    3.5/4,  2.5/4,  4.5/4,  5.5/4
};
#endif
float f_center_34[32] = {
    1/12,   3/12,   5/12,   7/12,
    9/12,  11/12,  13/12,  15/12,
    17/12, -5/12,  -3/12,  -1/12,
    17/8,   19/8,    5/8,    7/8,
    9/8,    11/8,   13/8,   15/8,
    9/4,    11/4,   13/4,    7/4,
    17/4,   11/4,   13/4,   15/4,
    17/4,   19/4,   21/4,   15/4
};
static const real_t frac_delay_q[] = {
    FRAC_CONST(0.43),
    FRAC_CONST(0.75),
    FRAC_CONST(0.347)
};
#endif


#ifdef __cplusplus
}
#endif
#endif
