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
** $Id: bits.c,v 1.44 2007/11/01 12:33:29 menno Exp $
**/
#include "Arduino.h"
#include <stdlib.h>
#include <stdint-gcc.h>
#include "common.h"
#include "structs.h"
#include "tables.h"
#include "sbr_dec.h"
#include "mp4.h"
#include "syntax.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char *err_msg[] = {
    "No error",
    "Gain control not yet implemented",
    "Pulse coding not allowed in short blocks",
    "Invalid huffman codebook",
    "Scalefactor out of range",
    "Unable to find ADTS syncword",
    "Channel coupling not yet implemented",
    "Channel configuration not allowed in error resilient frame",
    "Bit error in error resilient scalefactor decoding",
    "Error decoding huffman scalefactor (bitstream error)",
    "Error decoding huffman codeword (bitstream error)",
    "Non existent huffman codebook number found",
    "Invalid number of channels",
    "Maximum number of bitstream elements exceeded",
    "Input data buffer too small",
    "Array index out of range",
    "Maximum number of scalefactor bands exceeded",
    "Quantised value out of range",
    "LTP lag out of range",
    "Invalid SBR parameter decoded",
    "SBR called without being initialised",
    "Unexpected channel configuration change",
    "Error in program_config_element",
    "First SBR frame is not the same as first AAC frame",
    "Unexpected fill element with SBR data",
    "Not all elements were provided with SBR data",
    "LTP decoding not available",
    "Output data buffer too small",
    "CRC error in DRM data",
    "PNS not allowed in DRM data stream",
    "No standard extension payload allowed in DRM",
    "PCE shall be the first element in a frame",
    "Bitstream value not allowed by specification",
	"MAIN prediction not initialised"
};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int NeAACDecGetVersion(const char** faad_id_string, const char** faad_copyright_string) {
    const char* libfaadName = "2.20.1";
    const char* libCopyright = " Copyright 2002-2004: Ahead Software AG\n"
                                      " http://www.audiocoding.com\n"
                                      " bug tracking: https://sourceforge.net/p/faac/bugs/\n";
    if(faad_id_string) *faad_id_string = libfaadName;
    if(faad_copyright_string) *faad_copyright_string = libCopyright;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* NeAACDecGetErrorMessage(unsigned const char errcode) {
    if(errcode >= NUM_ERROR_MESSAGES) return NULL;
    return err_msg[errcode];
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
unsigned long NeAACDecGetCapabilities(void) {
    uint32_t cap = 0;
    /* can't do without it */
    cap += LC_DEC_CAP;
#ifdef MAIN_DEC
    cap += MAIN_DEC_CAP;
#endif
#ifdef LTP_DEC
    cap += LTP_DEC_CAP;
#endif
#ifdef LD_DEC
    cap += LD_DEC_CAP;
#endif
#ifdef ERROR_RESILIENCE
    cap += ERROR_RESILIENCE_CAP;
#endif
#ifdef FIXED_POINT
    cap += FIXED_POINT_CAP;
#endif
    return cap;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const unsigned char mes[] = {0x67, 0x20, 0x61, 0x20, 0x20, 0x20, 0x6f, 0x20, 0x72, 0x20, 0x65, 0x20, 0x6e, 0x20, 0x20, 0x20, 0x74,
                             0x20, 0x68, 0x20, 0x67, 0x20, 0x69, 0x20, 0x72, 0x20, 0x79, 0x20, 0x70, 0x20, 0x6f, 0x20, 0x63};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
NeAACDecHandle      NeAACDecOpen(void) {
    uint8_t         i;
    NeAACDecStruct* hDecoder = NULL;
    if((hDecoder = (NeAACDecStruct*)ps_calloc(1, sizeof(NeAACDecStruct))) == NULL) return NULL;
    memset(hDecoder, 0, sizeof(NeAACDecStruct));
    hDecoder->cmes = mes;
    hDecoder->config.outputFormat = FAAD_FMT_16BIT;
    hDecoder->config.defObjectType = MAIN;
    hDecoder->config.defSampleRate = 44100; /* Default: 44.1kHz */
    hDecoder->config.downMatrix = 0;
    hDecoder->adts_header_present = 0;
    hDecoder->adif_header_present = 0;
    hDecoder->latm_header_present = 0;
#ifdef ERROR_RESILIENCE
    hDecoder->aacSectionDataResilienceFlag = 0;
    hDecoder->aacScalefactorDataResilienceFlag = 0;
    hDecoder->aacSpectralDataResilienceFlag = 0;
#endif
    hDecoder->frameLength = 1024;
    hDecoder->frame = 0;
    hDecoder->sample_buffer = NULL;
    hDecoder->__r1 = 1;
    hDecoder->__r2 = 1;
    for(i = 0; i < MAX_CHANNELS; i++) {
        hDecoder->element_id[i] = INVALID_ELEMENT_ID;
        hDecoder->window_shape_prev[i] = 0;
        hDecoder->time_out[i] = NULL;
        hDecoder->fb_intermed[i] = NULL;
#ifdef SSR_DEC
        hDecoder->ssr_overlap[i] = NULL;
        hDecoder->prev_fmd[i] = NULL;
#endif
#ifdef MAIN_DEC
        hDecoder->pred_stat[i] = NULL;
#endif
#ifdef LTP_DEC
        hDecoder->ltp_lag[i] = 0;
        hDecoder->lt_pred_stat[i] = NULL;
#endif
    }
#ifdef SBR_DEC
    for(i = 0; i < MAX_SYNTAX_ELEMENTS; i++) { hDecoder->sbr[i] = NULL; }
#endif
    hDecoder->drc = drc_init(REAL_CONST(1.0), REAL_CONST(1.0));
    return hDecoder;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
NeAACDecConfigurationPtr NeAACDecGetCurrentConfiguration(NeAACDecHandle hpDecoder) {
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if(hDecoder) {
        NeAACDecConfigurationPtr config = &(hDecoder->config);
        return config;
    }
    return NULL;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
unsigned char NeAACDecSetConfiguration(NeAACDecHandle hpDecoder, NeAACDecConfigurationPtr config) {
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if(hDecoder && config) {
        /* check if we can decode this object type */
        if(can_decode_ot(config->defObjectType) < 0) return 0;
        hDecoder->config.defObjectType = config->defObjectType;
        /* samplerate: anything but 0 should be possible */
        if(config->defSampleRate == 0) return 0;
        hDecoder->config.defSampleRate = config->defSampleRate;
        /* check output format */
#ifdef FIXED_POINT
        if((config->outputFormat < 1) || (config->outputFormat > 4)) return 0;
#else
        if((config->outputFormat < 1) || (config->outputFormat > 5)) return 0;
#endif
        hDecoder->config.outputFormat = config->outputFormat;
        if(config->downMatrix > 1) return 0;
        hDecoder->config.downMatrix = config->downMatrix;
        /* OK */
        return 1;
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
__unused int latmCheck(latm_header* latm, bitfile* ld) {
    uint32_t good = 0, bad = 0, bits, m;
    while(ld->bytes_left) {
        bits = faad_latm_frame(latm, ld);
        if(bits == 0xFFFFFFFF) bad++;
        else {
            good++;
            while(bits > 0) {
                m = min(bits, 8);
                faad_getbits(ld, m);
                bits -= m;
            }
        }
    }
    return (good > 0);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
long NeAACDecInit(NeAACDecHandle hpDecoder, unsigned char* buffer, unsigned long buffer_size, unsigned long* samplerate, unsigned char* channels) {
    uint32_t        bits = 0;
    int32_t         ret = 0;
    // bitfile         ld;
    // adif_header     adif;
    // adts_header     adts;
    adif_header* adif = (adif_header*)faad_malloc(1 * sizeof(adif_header));
    adts_header* adts = (adts_header*)faad_malloc(1 * sizeof(adts_header));
    bitfile*       ld = (bitfile*)faad_malloc(1 * sizeof(bitfile));
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if((hDecoder == NULL) || (samplerate == NULL) || (channels == NULL) || (buffer_size == 0)){
        ret = -1;
        goto exit;
    }
    hDecoder->sf_index = get_sr_index(hDecoder->config.defSampleRate);
    hDecoder->object_type = hDecoder->config.defObjectType;
    *samplerate = get_sample_rate(hDecoder->sf_index);
    *channels = 1;
    if(buffer != NULL) {
#if 0
        int is_latm;
        latm_header *l = &hDecoder->latm_config;
#endif
        faad_initbits(ld, buffer, buffer_size);
#if 0
        memset(l, 0, sizeof(latm_header));
        is_latm = latmCheck(l, &ld);
        l->inited = 0;
        l->frameLength = 0;
        faad_rewindbits(&ld);
        if(is_latm && l->ASCbits>0)
        {
            int32_t x;
            hDecoder->latm_header_present = 1;
            x = NeAACDecInit2(hDecoder, l->ASC, (l->ASCbits+7)/8, samplerate, channels);
            if(x!=0)
                hDecoder->latm_header_present = 0;
            return x;
        } else
#endif
        /* Check if an ADIF header is present */
        if((buffer[0] == 'A') && (buffer[1] == 'D') && (buffer[2] == 'I') && (buffer[3] == 'F')) {
            hDecoder->adif_header_present = 1;
            get_adif_header(adif, ld);
            faad_byte_align(ld);
            hDecoder->sf_index = adif->pce[0].sf_index;
            hDecoder->object_type = adif->pce[0].object_type + 1;
            *samplerate = get_sample_rate(hDecoder->sf_index);
            *channels = adif->pce[0].channels;
            memcpy(&(hDecoder->pce), &(adif->pce[0]), sizeof(program_config));
            hDecoder->pce_set = 1;
            bits = bit2byte(faad_get_processed_bits(ld));
            /* Check if an ADTS header is present */
        }
        else if(faad_showbits(ld, 12) == 0xfff) {
            hDecoder->adts_header_present = 1;
            adts->old_format = hDecoder->config.useOldADTSFormat;
            adts_frame(adts, ld);
            hDecoder->sf_index = adts->sf_index;
            hDecoder->object_type = adts->profile + 1;
            *samplerate = get_sample_rate(hDecoder->sf_index);
            *channels = (adts->channel_configuration > 6) ? 2 : adts->channel_configuration;
        }
        if(ld->error) {
            faad_endbits(ld);
            ret = -1;
            goto exit;
        }
        faad_endbits(ld);
    }
    if(!*samplerate) {
        ret = -1;
        goto exit;
    }
#if(defined(PS_DEC) || defined(DRM_PS))
    /* check if we have a mono file */
    if(*channels == 1) {
        /* upMatrix to 2 channels for implicit signalling of PS */
        *channels = 2;
    }
#endif
    hDecoder->channelConfiguration = *channels;
#ifdef SBR_DEC
    /* implicit signalling */
    if(*samplerate <= 24000 && (hDecoder->config.dontUpSampleImplicitSBR == 0)) {
        *samplerate *= 2;
        hDecoder->forceUpSampling = 1;
    }
    else if(*samplerate > 24000 && (hDecoder->config.dontUpSampleImplicitSBR == 0)) { hDecoder->downSampledSBR = 1; }
#endif
    /* must be done before frameLength is divided by 2 for LD */
#ifdef SSR_DEC
    if(hDecoder->object_type == SSR) hDecoder->fb = ssr_filter_bank_init(hDecoder->frameLength / SSR_BANDS);
    else
#endif
        hDecoder->fb = filter_bank_init(hDecoder->frameLength);
#ifdef LD_DEC
    if(hDecoder->object_type == LD) hDecoder->frameLength >>= 1;
#endif
    if(can_decode_ot(hDecoder->object_type) < 0) {ret = -1; goto exit;}
    ret = bits;
    goto exit;
exit:
    if(ld) {
        free(ld);
        ld = NULL;
    }
    if(adts) {
        free(adts);
        adts = NULL;
    }
    if(adif) {
        free(adif);
        adif = NULL;
    }
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Init the library using a DecoderSpecificInfo */
char NeAACDecInit2(NeAACDecHandle hpDecoder, unsigned char* pBuffer, unsigned long SizeOfDecoderSpecificInfo, unsigned long* samplerate, unsigned char* channels) {
    NeAACDecStruct*        hDecoder = (NeAACDecStruct*)hpDecoder;
    int8_t                 rc;
    mp4AudioSpecificConfig mp4ASC;
    if((hDecoder == NULL) || (pBuffer == NULL) || (SizeOfDecoderSpecificInfo < 2) || (samplerate == NULL) || (channels == NULL)) { return -1; }
    hDecoder->adif_header_present = 0;
    hDecoder->adts_header_present = 0;
    /* decode the audio specific config */
    rc = AudioSpecificConfig2(pBuffer, SizeOfDecoderSpecificInfo, &mp4ASC, &(hDecoder->pce), hDecoder->latm_header_present);
    /* copy the relevant info to the decoder handle */
    *samplerate = mp4ASC.samplingFrequency;
    if(mp4ASC.channelsConfiguration) { *channels = mp4ASC.channelsConfiguration; }
    else {
        *channels = hDecoder->pce.channels;
        hDecoder->pce_set = 1;
    }
#if(defined(PS_DEC) || defined(DRM_PS))
    /* check if we have a mono file */
    if(*channels == 1) {
        /* upMatrix to 2 channels for implicit signalling of PS */
        *channels = 2;
    }
#endif
    hDecoder->sf_index = mp4ASC.samplingFrequencyIndex;
    hDecoder->object_type = mp4ASC.objectTypeIndex;
#ifdef ERROR_RESILIENCE
    hDecoder->aacSectionDataResilienceFlag = mp4ASC.aacSectionDataResilienceFlag;
    hDecoder->aacScalefactorDataResilienceFlag = mp4ASC.aacScalefactorDataResilienceFlag;
    hDecoder->aacSpectralDataResilienceFlag = mp4ASC.aacSpectralDataResilienceFlag;
#endif
#ifdef SBR_DEC
    hDecoder->sbr_present_flag = mp4ASC.sbr_present_flag;
    hDecoder->downSampledSBR = mp4ASC.downSampledSBR;
    if(hDecoder->config.dontUpSampleImplicitSBR == 0) hDecoder->forceUpSampling = mp4ASC.forceUpSampling;
    else hDecoder->forceUpSampling = 0;
    /* AAC core decoder samplerate is 2 times as low */
    if(((hDecoder->sbr_present_flag == 1) && (!hDecoder->downSampledSBR)) || hDecoder->forceUpSampling == 1) { hDecoder->sf_index = get_sr_index(mp4ASC.samplingFrequency / 2); }
#endif
    if(rc != 0) { return rc; }
    hDecoder->channelConfiguration = mp4ASC.channelsConfiguration;
    if(mp4ASC.frameLengthFlag)
#ifdef ALLOW_SMALL_FRAMELENGTH
        hDecoder->frameLength = 960;
#else
        return -1;
#endif
        /* must be done before frameLength is divided by 2 for LD */
#ifdef SSR_DEC
    if(hDecoder->object_type == SSR) hDecoder->fb = ssr_filter_bank_init(hDecoder->frameLength / SSR_BANDS);
    else
#endif
        hDecoder->fb = filter_bank_init(hDecoder->frameLength);
#ifdef LD_DEC
    if(hDecoder->object_type == LD) hDecoder->frameLength >>= 1;
#endif
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
char NeAACDecInitDRM(NeAACDecHandle* hpDecoder, unsigned long samplerate, unsigned char channels) {
    NeAACDecStruct** hDecoder = (NeAACDecStruct**)hpDecoder;
    if(hDecoder == NULL) return 1; /* error */
    NeAACDecClose(*hDecoder);
    *hDecoder = NeAACDecOpen();
    /* Special object type defined for DRM */
    (*hDecoder)->config.defObjectType = DRM_ER_LC;
    (*hDecoder)->config.defSampleRate = samplerate;
    #ifdef ERROR_RESILIENCE                            // This shoudl always be defined for DRM
    (*hDecoder)->aacSectionDataResilienceFlag = 1;     /* VCB11 */
    (*hDecoder)->aacScalefactorDataResilienceFlag = 0; /* no RVLC */
    (*hDecoder)->aacSpectralDataResilienceFlag = 1;    /* HCR */
    #endif
    (*hDecoder)->frameLength = 960;
    (*hDecoder)->sf_index = get_sr_index((*hDecoder)->config.defSampleRate);
    (*hDecoder)->object_type = (*hDecoder)->config.defObjectType;
    if((channels == DRMCH_STEREO) || (channels == DRMCH_SBR_STEREO)) (*hDecoder)->channelConfiguration = 2;
    else (*hDecoder)->channelConfiguration = 1;
    #ifdef SBR_DEC
    if((channels == DRMCH_MONO) || (channels == DRMCH_STEREO)) (*hDecoder)->sbr_present_flag = 0;
    else (*hDecoder)->sbr_present_flag = 1;
    #endif
    (*hDecoder)->fb = filter_bank_init((*hDecoder)->frameLength);
    return 0;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void NeAACDecClose(NeAACDecHandle hpDecoder) {
    uint8_t         i;
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if (hDecoder == NULL) return;
#ifdef PROFILE
    printf("AAC decoder total:  %I64d cycles\n", hDecoder->cycles);
    printf("requant:            %I64d cycles\n", hDecoder->requant_cycles);
    printf("spectral_data:      %I64d cycles\n", hDecoder->spectral_cycles);
    printf("scalefactors:       %I64d cycles\n", hDecoder->scalefac_cycles);
    printf("output:             %I64d cycles\n", hDecoder->output_cycles);
#endif
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (hDecoder->time_out[i]) faad_free(hDecoder->time_out[i]);
        if (hDecoder->fb_intermed[i]) faad_free(hDecoder->fb_intermed[i]);
#ifdef SSR_DEC
        if (hDecoder->ssr_overlap[i]) faad_free(hDecoder->ssr_overlap[i]);
        if (hDecoder->prev_fmd[i]) faad_free(hDecoder->prev_fmd[i]);
#endif
#ifdef MAIN_DEC
        if (hDecoder->pred_stat[i]) faad_free(hDecoder->pred_stat[i]);
#endif
#ifdef LTP_DEC
        if (hDecoder->lt_pred_stat[i]) faad_free(hDecoder->lt_pred_stat[i]);
#endif
    }
#ifdef SSR_DEC
    if (hDecoder->object_type == SSR)
        ssr_filter_bank_end(hDecoder->fb);
    else
#endif
        filter_bank_end(hDecoder->fb);
    drc_end(hDecoder->drc);
    if (hDecoder->sample_buffer) faad_free(hDecoder->sample_buffer);
#ifdef SBR_DEC
    for (i = 0; i < MAX_SYNTAX_ELEMENTS; i++) {
        if (hDecoder->sbr[i]) sbrDecodeEnd(hDecoder->sbr[i]);
    }
#endif
    if (hDecoder) faad_free(hDecoder);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void NeAACDecPostSeekReset(NeAACDecHandle hpDecoder, long frame) {
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if(hDecoder) {
        hDecoder->postSeekResetFlag = 1;
        if(frame != -1) hDecoder->frame = frame;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void create_channel_config(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo) {
    hInfo->num_front_channels = 0;
    hInfo->num_side_channels = 0;
    hInfo->num_back_channels = 0;
    hInfo->num_lfe_channels = 0;
    memset(hInfo->channel_position, 0, MAX_CHANNELS * sizeof(uint8_t));
    if(hDecoder->downMatrix) {
        hInfo->num_front_channels = 2;
        hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
        hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
        return;
    }
    /* check if there is a PCE */
    if(hDecoder->pce_set) {
        uint8_t i, chpos = 0;
        uint8_t chdir, back_center = 0, total = 0;
        hInfo->num_front_channels = hDecoder->pce.num_front_channels;
        total += hInfo->num_front_channels;
        hInfo->num_side_channels = hDecoder->pce.num_side_channels;
        total += hInfo->num_side_channels;
        hInfo->num_back_channels = hDecoder->pce.num_back_channels;
        total += hInfo->num_back_channels;
        hInfo->num_lfe_channels = hDecoder->pce.num_lfe_channels;
        total += hInfo->num_lfe_channels;
        chdir = hInfo->num_front_channels;
        if(chdir & 1) {
#if(defined(PS_DEC) || defined(DRM_PS))
            if(total == 1) {
                /* When PS is enabled output is always stereo */
                hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
            }
            else
#endif
                hInfo->channel_position[chpos++] = FRONT_CHANNEL_CENTER;
            chdir--;
        }
        for(i = 0; i < chdir; i += 2) {
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
        }
        for(i = 0; i < hInfo->num_side_channels; i += 2) {
            hInfo->channel_position[chpos++] = SIDE_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = SIDE_CHANNEL_RIGHT;
        }
        chdir = hInfo->num_back_channels;
        if(chdir & 1) {
            back_center = 1;
            chdir--;
        }
        for(i = 0; i < chdir; i += 2) {
            hInfo->channel_position[chpos++] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = BACK_CHANNEL_RIGHT;
        }
        if(back_center) { hInfo->channel_position[chpos++] = BACK_CHANNEL_CENTER; }
        for(i = 0; i < hInfo->num_lfe_channels; i++) { hInfo->channel_position[chpos++] = LFE_CHANNEL; }
    }
    else {
        switch(hDecoder->channelConfiguration) {
            case 1:
#if(defined(PS_DEC) || defined(DRM_PS))
                /* When PS is enabled output is always stereo */
                hInfo->num_front_channels = 2;
                hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
#else
                hInfo->num_front_channels = 1;
                hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
#endif
                break;
            case 2:
                hInfo->num_front_channels = 2;
                hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
                break;
            case 3:
                hInfo->num_front_channels = 3;
                hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
                break;
            case 4:
                hInfo->num_front_channels = 3;
                hInfo->num_back_channels = 1;
                hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
                hInfo->channel_position[3] = BACK_CHANNEL_CENTER;
                break;
            case 5:
                hInfo->num_front_channels = 3;
                hInfo->num_back_channels = 2;
                hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
                hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
                hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
                break;
            case 6:
                hInfo->num_front_channels = 3;
                hInfo->num_back_channels = 2;
                hInfo->num_lfe_channels = 1;
                hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
                hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
                hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
                hInfo->channel_position[5] = LFE_CHANNEL;
                break;
            case 7:
                hInfo->num_front_channels = 3;
                hInfo->num_side_channels = 2;
                hInfo->num_back_channels = 2;
                hInfo->num_lfe_channels = 1;
                hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
                hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
                hInfo->channel_position[3] = SIDE_CHANNEL_LEFT;
                hInfo->channel_position[4] = SIDE_CHANNEL_RIGHT;
                hInfo->channel_position[5] = BACK_CHANNEL_LEFT;
                hInfo->channel_position[6] = BACK_CHANNEL_RIGHT;
                hInfo->channel_position[7] = LFE_CHANNEL;
                break;
            default: /* channelConfiguration == 0 || channelConfiguration > 7 */
            {
                uint8_t i;
                uint8_t ch = hDecoder->fr_channels - hDecoder->has_lfe;
                if(ch & 1) /* there's either a center front or a center back channel */
                {
                    uint8_t ch1 = (ch - 1) / 2;
                    if(hDecoder->first_syn_ele == ID_SCE) {
                        hInfo->num_front_channels = ch1 + 1;
                        hInfo->num_back_channels = ch1;
                        hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                        for(i = 1; i <= ch1; i += 2) {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                        }
                        for(i = ch1 + 1; i < ch; i += 2) {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                        }
                    }
                    else {
                        hInfo->num_front_channels = ch1;
                        hInfo->num_back_channels = ch1 + 1;
                        for(i = 0; i < ch1; i += 2) {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                        }
                        for(i = ch1; i < ch - 1; i += 2) {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                        }
                        hInfo->channel_position[ch - 1] = BACK_CHANNEL_CENTER;
                    }
                }
                else {
                    uint8_t ch1 = (ch) / 2;
                    hInfo->num_front_channels = ch1;
                    hInfo->num_back_channels = ch1;
                    if(ch1 & 1) {
                        hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                        for(i = 1; i <= ch1; i += 2) {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                        }
                        for(i = ch1 + 1; i < ch - 1; i += 2) {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                        }
                        hInfo->channel_position[ch - 1] = BACK_CHANNEL_CENTER;
                    }
                    else {
                        for(i = 0; i < ch1; i += 2) {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = FRONT_CHANNEL_RIGHT;
                        }
                        for(i = ch1; i < ch; i += 2) {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i + 1] = BACK_CHANNEL_RIGHT;
                        }
                    }
                }
                hInfo->num_lfe_channels = hDecoder->has_lfe;
                for(i = ch; i < hDecoder->fr_channels; i++) { hInfo->channel_position[i] = LFE_CHANNEL; }
            } break;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void* NeAACDecDecode(NeAACDecHandle hpDecoder, NeAACDecFrameInfo* hInfo, unsigned char* buffer, unsigned long buffer_size) {
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    return aac_frame_decode(hDecoder, hInfo, buffer, buffer_size, NULL, 0);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void* NeAACDecDecode2(NeAACDecHandle hpDecoder, NeAACDecFrameInfo* hInfo, unsigned char* buffer, unsigned long buffer_size, void** sample_buffer, unsigned long sample_buffer_size) {
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if((sample_buffer == NULL) || (sample_buffer_size == 0)) {
        hInfo->error = 27;
        return NULL;
    }
    return aac_frame_decode(hDecoder, hInfo, buffer, buffer_size, sample_buffer, sample_buffer_size);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
    #define ERROR_STATE_INIT 6
void conceal_output(NeAACDecStruct* hDecoder, uint16_t frame_len, uint8_t out_ch, void* sample_buffer) { return; }
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void* aac_frame_decode(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, unsigned char* buffer, unsigned long buffer_size, void** sample_buffer2, unsigned long sample_buffer_size) {
    uint16_t i;
    uint8_t  channels = 0;
    uint8_t  output_channels = 0;
    bitfile  ld = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t bitsconsumed;
    uint16_t frame_len;
    void*    sample_buffer;
    uint32_t startbit = 0, endbit = 0, payload_bits = 0;
    (void)endbit;
    (void)startbit;
    (void)payload_bits;
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
    /* safety checks */
    if((hDecoder == NULL) || (hInfo == NULL) || (buffer == NULL)) { return NULL; }
#if 0
    printf("%d\n", buffer_size*8);
#endif
    frame_len = hDecoder->frameLength;
    memset(hInfo, 0, sizeof(NeAACDecFrameInfo));
    memset(hDecoder->internal_channel, 0, MAX_CHANNELS * sizeof(hDecoder->internal_channel[0]));
#ifdef USE_TIME_LIMIT
    if((TIME_LIMIT * get_sample_rate(hDecoder->sf_index)) > hDecoder->TL_count) { hDecoder->TL_count += 1024; }
    else {
        hInfo->error = (NUM_ERROR_MESSAGES - 1);
        goto error;
    }
#endif
    /* check for some common metadata tag types in the bitstream
     * No need to return an error
     */
    /* ID3 */
    if(buffer_size >= 128) {
        if(memcmp(buffer, "TAG", 3) == 0) {
            /* found it */
            hInfo->bytesconsumed = 128; /* 128 bytes fixed size */
            /* no error, but no output either */
            return NULL;
        }
    }
    /* initialize the bitstream */
    faad_initbits(&ld, buffer, buffer_size);
#if 0
    {
        int i;
        for (i = 0; i < ((buffer_size+3)>>2); i++)
        {
            uint8_t *buf;
            uint32_t temp = 0;
            buf = faad_getbitbuffer(&ld, 32);
            //temp = getdword((void*)buf);
            temp = *((uint32_t*)buf);
            printf("0x%.8X\n", temp);
            free(buf);
        }
        faad_endbits(&ld);
        faad_initbits(&ld, buffer, buffer_size);
    }
#endif
#if 0
    if(hDecoder->latm_header_present)
    {
        payload_bits = faad_latm_frame(&hDecoder->latm_config, &ld);
        startbit = faad_get_processed_bits(&ld);
        if(payload_bits == -1U)
        {
            hInfo->error = 1;
            goto error;
        }
    }
#endif
#ifdef DRM
    if(hDecoder->object_type == DRM_ER_LC) {
        /* We do not support stereo right now */
        if(0) //(hDecoder->channelConfiguration == 2)
        {
            hInfo->error = 28; // Throw CRC error
            goto error;
        }
        faad_getbits(&ld, 8);
    }
#endif
    if(hDecoder->adts_header_present) {
        adts_header adts;
        adts.old_format = hDecoder->config.useOldADTSFormat;
        if((hInfo->error = adts_frame(&adts, &ld)) > 0) goto error;
        /* MPEG2 does byte_alignment() here,
         * but ADTS header is always multiple of 8 bits in MPEG2
         * so not needed to actually do it.
         */
    }
#ifdef ANALYSIS
    dbg_count = 0;
#endif
    /* decode the complete bitstream */
#ifdef DRM
    if(/*(hDecoder->object_type == 6) ||*/ (hDecoder->object_type == DRM_ER_LC)) { DRM_aac_scalable_main_element(hDecoder, hInfo, &ld, &hDecoder->pce, hDecoder->drc); }
    else {
#endif
        raw_data_block(hDecoder, hInfo, &ld, &hDecoder->pce, hDecoder->drc);
#ifdef DRM
    }
#endif
#if 0
    if(hDecoder->latm_header_present)
    {
        endbit = faad_get_processed_bits(&ld);
        if(endbit-startbit > payload_bits)
            fprintf(stderr, "\r\nERROR, too many payload bits read: %u > %d. Please. report with a link to a sample\n",
                endbit-startbit, payload_bits);
        if(hDecoder->latm_config.otherDataLenBits > 0)
            faad_getbits(&ld, hDecoder->latm_config.otherDataLenBits);
        faad_byte_align(&ld);
    }
#endif
    channels = hDecoder->fr_channels;
    if(hInfo->error > 0) goto error;
    /* safety check */
    if(channels == 0 || channels > MAX_CHANNELS) {
        /* invalid number of channels */
        hInfo->error = 12;
        goto error;
    }
    /* no more bit reading after this */
    bitsconsumed = faad_get_processed_bits(&ld);
    hInfo->bytesconsumed = bit2byte(bitsconsumed);
    if(ld.error) {
        hInfo->error = 14;
        goto error;
    }
    faad_endbits(&ld);
    if(!hDecoder->adts_header_present && !hDecoder->adif_header_present
#if 0
        && !hDecoder->latm_header_present
#endif
    ) {
        if(hDecoder->channelConfiguration == 0) hDecoder->channelConfiguration = channels;
        if(channels == 8) /* 7.1 */
            hDecoder->channelConfiguration = 7;
        if(channels == 7) /* not a standard channelConfiguration */
            hDecoder->channelConfiguration = 0;
    }
    if((channels == 5 || channels == 6) && hDecoder->config.downMatrix) {
        hDecoder->downMatrix = 1;
        output_channels = 2;
    }
    else { output_channels = channels; }
#if(defined(PS_DEC) || defined(DRM_PS))
    hDecoder->upMatrix = 0;
    /* check if we have a mono file */
    if(output_channels == 1) {
        /* upMatrix to 2 channels for implicit signalling of PS */
        hDecoder->upMatrix = 1;
        output_channels = 2;
    }
#endif
    /* Make a channel configuration based on either a PCE or a channelConfiguration */
    create_channel_config(hDecoder, hInfo);
    /* number of samples in this frame */
    hInfo->samples = frame_len * output_channels;
    /* number of channels in this frame */
    hInfo->channels = output_channels;
    /* samplerate */
    hInfo->samplerate = get_sample_rate(hDecoder->sf_index);
    /* object type */
    hInfo->object_type = hDecoder->object_type;
    /* sbr */
    hInfo->sbr = NO_SBR;
    /* header type */
    hInfo->header_type = RAW;
    if(hDecoder->adif_header_present) hInfo->header_type = ADIF;
    if(hDecoder->adts_header_present) hInfo->header_type = ADTS;
#if 0
    if (hDecoder->latm_header_present)
        hInfo->header_type = LATM;
#endif
#if(defined(PS_DEC) || defined(DRM_PS))
    hInfo->ps = hDecoder->ps_used_global;
#endif
    /* check if frame has channel elements */
    if(channels == 0) {
        hDecoder->frame++;
        return NULL;
    }
    /* allocate the buffer for the final samples */
    if((hDecoder->sample_buffer == NULL) || (hDecoder->alloced_channels != output_channels)) {
        const uint8_t str[] = {
            sizeof(int16_t), sizeof(int32_t), sizeof(int32_t), sizeof(float32_t), sizeof(double), sizeof(int16_t), sizeof(int16_t), sizeof(int16_t), sizeof(int16_t), 0, 0, 0};
        uint8_t stride = str[hDecoder->config.outputFormat - 1];
#ifdef SBR_DEC
        if(((hDecoder->sbr_present_flag == 1) && (!hDecoder->downSampledSBR)) || (hDecoder->forceUpSampling == 1)) { stride = 2 * stride; }
#endif
        /* check if we want to use internal sample_buffer */
        if(sample_buffer_size == 0) {
            if(hDecoder->sample_buffer) faad_free(hDecoder->sample_buffer);
            hDecoder->sample_buffer = NULL;
            hDecoder->sample_buffer = faad_malloc(frame_len * output_channels * stride);
        }
        else if(sample_buffer_size < frame_len * output_channels * stride) {
            /* provided sample buffer is not big enough */
            hInfo->error = 27;
            return NULL;
        }
        hDecoder->alloced_channels = output_channels;
    }
    if(sample_buffer_size == 0) { sample_buffer = hDecoder->sample_buffer; }
    else { sample_buffer = *sample_buffer2; }
#ifdef SBR_DEC
    if((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) {
        uint8_t ele;
        /* this data is different when SBR is used or when the data is upsampled */
        if(!hDecoder->downSampledSBR) {
            frame_len *= 2;
            hInfo->samples *= 2;
            hInfo->samplerate *= 2;
        }
        /* check if every element was provided with SBR data */
        for(ele = 0; ele < hDecoder->fr_ch_ele; ele++) {
            if(hDecoder->sbr[ele] == NULL) {
                hInfo->error = 25;
                goto error;
            }
        }
        /* sbr */
        if(hDecoder->sbr_present_flag == 1) {
            hInfo->object_type = HE_AAC;
            hInfo->sbr = SBR_UPSAMPLED;
        }
        else { hInfo->sbr = NO_SBR_UPSAMPLED; }
        if(hDecoder->downSampledSBR) { hInfo->sbr = SBR_DOWNSAMPLED; }
    }
#endif
    sample_buffer = output_to_PCM(hDecoder, hDecoder->time_out, sample_buffer, output_channels, frame_len, hDecoder->config.outputFormat);
#ifdef DRM
    // conceal_output(hDecoder, frame_len, output_channels, sample_buffer);
#endif
    hDecoder->postSeekResetFlag = 0;
    hDecoder->frame++;
#ifdef LD_DEC
    if(hDecoder->object_type != LD) {
#endif
        if(hDecoder->frame <= 1) hInfo->samples = 0;
#ifdef LD_DEC
    }
    else {
        /* LD encoders will give lower delay */
        if(hDecoder->frame <= 0) hInfo->samples = 0;
    }
#endif
    /* cleanup */
#ifdef ANALYSIS
    fflush(stdout);
#endif
#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->cycles += count;
#endif
    return sample_buffer;
error:
#ifdef DRM
    hDecoder->error_state = ERROR_STATE_INIT;
#endif
    /* reset filterbank state */
    for(i = 0; i < MAX_CHANNELS; i++) {
        if(hDecoder->fb_intermed[i] != NULL) { memset(hDecoder->fb_intermed[i], 0, hDecoder->frameLength * sizeof(real_t)); }
    }
#ifdef SBR_DEC
    for(i = 0; i < MAX_SYNTAX_ELEMENTS; i++) {
        if(hDecoder->sbr[i] != NULL) { sbrReset(hDecoder->sbr[i]); }
    }
#endif
    faad_endbits(&ld);
    /* cleanup */
#ifdef ANALYSIS
    fflush(stdout);
#endif
    return NULL;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const uint8_t tabFlipbits[256] = {0, 128, 64, 192, 32, 160, 96,  224, 16, 144, 80, 208, 48, 176, 112, 240, 8,  136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
                                     4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244, 12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
                                     2, 130, 66, 194, 34, 162, 98,  226, 18, 146, 82, 210, 50, 178, 114, 242, 10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
                                     6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246, 14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
                                     1, 129, 65, 193, 33, 161, 97,  225, 17, 145, 81, 209, 49, 177, 113, 241, 9,  137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
                                     5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245, 13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
                                     3, 131, 67, 195, 35, 163, 99,  227, 19, 147, 83, 211, 51, 179, 115, 243, 11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
                                     7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247, 15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* CRC lookup table for G8 polynome in DRM standard */
const uint8_t crc_table_G8[256] = {
    0x0,  0x1d, 0x3a, 0x27, 0x74, 0x69, 0x4e, 0x53, 0xe8, 0xf5, 0xd2, 0xcf, 0x9c, 0x81, 0xa6, 0xbb, 0xcd, 0xd0, 0xf7, 0xea, 0xb9, 0xa4, 0x83, 0x9e, 0x25, 0x38, 0x1f, 0x2,  0x51, 0x4c, 0x6b, 0x76,
    0x87, 0x9a, 0xbd, 0xa0, 0xf3, 0xee, 0xc9, 0xd4, 0x6f, 0x72, 0x55, 0x48, 0x1b, 0x6,  0x21, 0x3c, 0x4a, 0x57, 0x70, 0x6d, 0x3e, 0x23, 0x4,  0x19, 0xa2, 0xbf, 0x98, 0x85, 0xd6, 0xcb, 0xec, 0xf1,
    0x13, 0xe,  0x29, 0x34, 0x67, 0x7a, 0x5d, 0x40, 0xfb, 0xe6, 0xc1, 0xdc, 0x8f, 0x92, 0xb5, 0xa8, 0xde, 0xc3, 0xe4, 0xf9, 0xaa, 0xb7, 0x90, 0x8d, 0x36, 0x2b, 0xc,  0x11, 0x42, 0x5f, 0x78, 0x65,
    0x94, 0x89, 0xae, 0xb3, 0xe0, 0xfd, 0xda, 0xc7, 0x7c, 0x61, 0x46, 0x5b, 0x8,  0x15, 0x32, 0x2f, 0x59, 0x44, 0x63, 0x7e, 0x2d, 0x30, 0x17, 0xa,  0xb1, 0xac, 0x8b, 0x96, 0xc5, 0xd8, 0xff, 0xe2,
    0x26, 0x3b, 0x1c, 0x1,  0x52, 0x4f, 0x68, 0x75, 0xce, 0xd3, 0xf4, 0xe9, 0xba, 0xa7, 0x80, 0x9d, 0xeb, 0xf6, 0xd1, 0xcc, 0x9f, 0x82, 0xa5, 0xb8, 0x3,  0x1e, 0x39, 0x24, 0x77, 0x6a, 0x4d, 0x50,
    0xa1, 0xbc, 0x9b, 0x86, 0xd5, 0xc8, 0xef, 0xf2, 0x49, 0x54, 0x73, 0x6e, 0x3d, 0x20, 0x7,  0x1a, 0x6c, 0x71, 0x56, 0x4b, 0x18, 0x5,  0x22, 0x3f, 0x84, 0x99, 0xbe, 0xa3, 0xf0, 0xed, 0xca, 0xd7,
    0x35, 0x28, 0xf,  0x12, 0x41, 0x5c, 0x7b, 0x66, 0xdd, 0xc0, 0xe7, 0xfa, 0xa9, 0xb4, 0x93, 0x8e, 0xf8, 0xe5, 0xc2, 0xdf, 0x8c, 0x91, 0xb6, 0xab, 0x10, 0xd,  0x2a, 0x37, 0x64, 0x79, 0x5e, 0x43,
    0xb2, 0xaf, 0x88, 0x95, 0xc6, 0xdb, 0xfc, 0xe1, 0x5a, 0x47, 0x60, 0x7d, 0x2e, 0x33, 0x14, 0x9,  0x7f, 0x62, 0x45, 0x58, 0xb,  0x16, 0x31, 0x2c, 0x97, 0x8a, 0xad, 0xb0, 0xe3, 0xfe, 0xd9, 0xc4,
};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t faad_check_CRC(bitfile* ld, uint16_t len) {
    int          bytes, rem;
    unsigned int CRC;
    unsigned int r = 255; /* Initialize to all ones */
    /* CRC polynome used x^8 + x^4 + x^3 + x^2 +1 */
#define GPOLY 0435
    faad_rewindbits(ld);
    CRC = (unsigned int)~faad_getbits(ld, 8) & 0xFF; /* CRC is stored inverted */
    bytes = len >> 3;
    rem = len & 0x7;
    for (; bytes > 0; bytes--) { r = crc_table_G8[(r ^ faad_getbits(ld, 8)) & 0xFF]; }
    for (; rem > 0; rem--) { r = ((r << 1) ^ (((faad_get1bit(ld) & 1) ^ ((r >> 7) & 1)) * GPOLY)) & 0xFF; }
    if (r != CRC)
    //  if (0)
    {
        return 28;
    } else {
        return 0;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* initialize buffer, call once before first getbits or showbits */
void faad_initbits(bitfile* ld, const void* _buffer, const uint32_t buffer_size) {
    uint32_t tmp;
    if(ld == NULL) return;
    // useless
    // memset(ld, 0, sizeof(bitfile));
    if(buffer_size == 0 || _buffer == NULL) {
        ld->error = 1;
        return;
    }
    ld->buffer = _buffer;
    ld->buffer_size = buffer_size;
    ld->bytes_left = buffer_size;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)ld->buffer);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)ld->buffer, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)ld->buffer + 1);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)ld->buffer + 1, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->start = (uint32_t*)ld->buffer;
    ld->tail = ((uint32_t*)ld->buffer + 2);
    ld->bits_left = 32;
    ld->error = 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void faad_endbits(bitfile* ld) {
    // void
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t faad_get_processed_bits(bitfile* ld) { return (uint32_t)(8 * (4 * (ld->tail - ld->start) - 4) - (ld->bits_left)); }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t faad_byte_align(bitfile* ld) {
    int remainder = (32 - ld->bits_left) & 0x7;
    if(remainder) {
        faad_flushbits(ld, 8 - remainder);
        return (uint8_t)(8 - remainder);
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void faad_flushbits_ex(bitfile* ld, uint32_t bits) {
    uint32_t tmp;
    ld->bufa = ld->bufb;
    if(ld->bytes_left >= 4) {
        tmp = getdword(ld->tail);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(ld->tail, ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->tail++;
    ld->bits_left += (32 - bits);
    // ld->bytes_left -= 4;
    //    if (ld->bytes_left == 0)
    //        ld->no_more_reading = 1;
    //    if (ld->bytes_left < 0)
    //        ld->error = 1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* rewind to beginning */
void faad_rewindbits(bitfile* ld) {
    uint32_t tmp;
    ld->bytes_left = ld->buffer_size;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)&ld->start[0]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)&ld->start[0], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;
    if(ld->bytes_left >= 4) {
        tmp = getdword((uint32_t*)&ld->start[1]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n((uint32_t*)&ld->start[1], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->bits_left = 32;
    ld->tail = &ld->start[2];
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* reset to a certain point */
void faad_resetbits(bitfile* ld, int bits) {
    uint32_t tmp;
    int      words = bits >> 5;
    int      remainder = bits & 0x1F;
    if(ld->buffer_size < words * 4) ld->bytes_left = 0;
    else ld->bytes_left = ld->buffer_size - words * 4;
    if(ld->bytes_left >= 4) {
        tmp = getdword(&ld->start[words]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(&ld->start[words], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufa = tmp;
    if(ld->bytes_left >= 4) {
        tmp = getdword(&ld->start[words + 1]);
        ld->bytes_left -= 4;
    }
    else {
        tmp = getdword_n(&ld->start[words + 1], ld->bytes_left);
        ld->bytes_left = 0;
    }
    ld->bufb = tmp;
    ld->bits_left = 32 - remainder;
    ld->tail = &ld->start[words + 2];
    /* recheck for reading too many bytes */
    ld->error = 0;
    //    if (ld->bytes_left == 0)
    //        ld->no_more_reading = 1;
    //    if (ld->bytes_left < 0)
    //        ld->error = 1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t* faad_getbitbuffer(bitfile* ld, uint32_t bits) {
    int          i;
    unsigned int temp;
    int          bytes = bits >> 3;
    int          remainder = bits & 0x7;
    uint8_t* buffer = (uint8_t*)faad_malloc((bytes + 1) * sizeof(uint8_t));
    for(i = 0; i < bytes; i++) { buffer[i] = (uint8_t)faad_getbits(ld, 8); }
    if(remainder) {
        temp = faad_getbits(ld, remainder) << (8 - remainder);
        buffer[bytes] = (uint8_t)temp;
    }
    return buffer;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
/* return the original data buffer */
void* faad_origbitbuffer(bitfile* ld) { return (void*)ld->start; }
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
/* return the original data buffer size */
uint32_t faad_origbitbuffer_size(bitfile* ld) { return ld->buffer_size; }
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* reversed bit reading routines, used for RVLC and HCR */
void faad_initbits_rev(bitfile* ld, void* buffer, uint32_t bits_in_buffer) {
    uint32_t tmp;
    int32_t  index;
    ld->buffer_size = bit2byte(bits_in_buffer);
    index = (bits_in_buffer + 31) / 32 - 1;
    ld->start = (uint32_t*)buffer + index - 2;
    tmp = getdword((uint32_t*)buffer + index);
    ld->bufa = tmp;
    tmp = getdword((uint32_t*)buffer + index - 1);
    ld->bufb = tmp;
    ld->tail = (uint32_t*)buffer + index;
    ld->bits_left = bits_in_buffer % 32;
    if(ld->bits_left == 0) ld->bits_left = 32;
    ld->bytes_left = ld->buffer_size;
    ld->error = 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t faad_showbits(bitfile* ld, uint32_t bits) {
    if (bits <= ld->bits_left) {
        // return (ld->bufa >> (ld->bits_left - bits)) & bitmask[bits];
        return (ld->bufa << (32 - ld->bits_left)) >> (32 - bits);
    }
    bits -= ld->bits_left;
    // return ((ld->bufa & bitmask[ld->bits_left]) << bits) | (ld->bufb >> (32 - bits));
    return ((ld->bufa & ((1 << ld->bits_left) - 1)) << bits) | (ld->bufb >> (32 - bits));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void faad_flushbits(bitfile* ld, uint32_t bits) {
    /* do nothing if error */
    if (ld->error != 0) return;
    if (bits < ld->bits_left) {
        ld->bits_left -= bits;
    } else {
        faad_flushbits_ex(ld, bits);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* reversed bitreading routines */
uint32_t faad_showbits_rev(bitfile* ld, uint32_t bits) {
    uint8_t  i;
    uint32_t B = 0;
    if (bits <= ld->bits_left) {
        for (i = 0; i < bits; i++) {
            if (ld->bufa & (1 << (i + (32 - ld->bits_left)))) B |= (1 << (bits - i - 1));
        }
        return B;
    } else {
        for (i = 0; i < ld->bits_left; i++) {
            if (ld->bufa & (1 << (i + (32 - ld->bits_left)))) B |= (1 << (bits - i - 1));
        }
        for (i = 0; i < bits - ld->bits_left; i++) {
            if (ld->bufb & (1 << (i + (32 - ld->bits_left)))) B |= (1 << (bits - ld->bits_left - i - 1));
        }
        return B;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void faad_flushbits_rev(bitfile* ld, uint32_t bits) {
    /* do nothing if error */
    if (ld->error != 0) return;
    if (bits < ld->bits_left) {
        ld->bits_left -= bits;
    } else {
        uint32_t tmp;
        ld->bufa = ld->bufb;
        tmp = getdword(ld->start);
        ld->bufb = tmp;
        ld->start--;
        ld->bits_left += (32 - bits);
        if (ld->bytes_left < 4) {
            ld->error = 1;
            ld->bytes_left = 0;
        } else {
            ld->bytes_left -= 4;
        }
        //        if (ld->bytes_left == 0)
        //            ld->no_more_reading = 1;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t faad_getbits_rev(bitfile* ld, uint32_t n) {
    uint32_t ret;
    if (n == 0) return 0;
    ret = faad_showbits_rev(ld, n);
    faad_flushbits_rev(ld, n);
#ifdef ANALYSIS
    if (print) fprintf(stdout, "%4d %2d bits, val: %4d, variable: %d %s\n", dbg_count++, n, ret, var, dbg);
#endif
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
uint32_t showbits_hcr(bits_t* ld, uint8_t bits) {
    if (bits == 0) return 0;
    if (ld->len <= 32) {
        /* huffman_spectral_data_2 needs to read more than may be available, bits maybe
           > ld->len, deliver 0 than */
        if (ld->len >= bits)
            return ((ld->bufa >> (ld->len - bits)) & (0xFFFFFFFF >> (32 - bits)));
        else
            return ((ld->bufa << (bits - ld->len)) & (0xFFFFFFFF >> (32 - bits)));
    } else {
        if ((ld->len - bits) < 32) {
            return ((ld->bufb & (0xFFFFFFFF >> (64 - ld->len))) << (bits - ld->len + 32)) | (ld->bufa >> (ld->len - bits));
        } else {
            return ((ld->bufb >> (ld->len - bits - 32)) & (0xFFFFFFFF >> (32 - bits)));
        }
    }
}
#endif /*ERROR_RESILIENCE*/
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
/* return 1 if position is outside of buffer, 0 otherwise */
int8_t flushbits_hcr(bits_t* ld, uint8_t bits) {
    ld->len -= bits;
    if (ld->len < 0) {
        ld->len = 0;
        return 1;
    } else {
        return 0;
    }
}
#endif /*ERROR_RESILIENCE*/
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
int8_t getbits_hcr(bits_t* ld, uint8_t n, uint32_t* result) {
    *result = showbits_hcr(ld, n);
    return flushbits_hcr(ld, n);
}
#endif /*ERROR_RESILIENCE*/
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
int8_t get1bit_hcr(bits_t* ld, uint8_t* result) {
    uint32_t res;
    int8_t   ret;
    ret = getbits_hcr(ld, 1, &res);
    *result = (int8_t)(res & 1);
    return ret;
}
#endif /*ERROR_RESILIENCE*/
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t faad_get1bit(bitfile* ld) {
    uint8_t r;
    if (ld->bits_left > 0) {
        ld->bits_left--;
        r = (uint8_t)((ld->bufa >> ld->bits_left) & 1);
        return r;
    }
    /* bits_left == 0 */
#if 0
    r = (uint8_t)(ld->bufb >> 31);
    faad_flushbits_ex(ld, 1);
#else
    r = (uint8_t)faad_getbits(ld, 1);
#endif
    return r;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t faad_getbits(bitfile *ld, uint32_t n){
    uint32_t ret;
    if (n == 0)
        return 0;
    ret = faad_showbits(ld, n);
    faad_flushbits(ld, n);
#ifdef ANALYSIS
    if (print)
        fprintf(stdout, "%4d %2d bits, val: %4d, variable: %d %s\n", dbg_count++, n, ret, var, dbg);
#endif
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t getdword(void *mem)
{
    uint32_t tmp;
#ifndef ARCH_IS_BIG_ENDIAN
    ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[3];
    ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[2];
    ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[1];
    ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[0];
#else
    ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[0];
    ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[1];
    ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[2];
    ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[3];
#endif
    return tmp;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* reads only n bytes from the stream instead of the standard 4 */
uint32_t getdword_n(void *mem, int n)
{
    uint32_t tmp = 0;
#ifndef ARCH_IS_BIG_ENDIAN
    switch (n)
    {
    case 3:
        ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[2]; [[fallthrough]];
    case 2:
        ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[1]; [[fallthrough]];
    case 1:
        ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[0];
    default:
        break;
    }
#else
    switch (n)
    {
    case 3:
        ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[2];
    case 2:
        ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[1];
    case 1:
        ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[0];
    default:
        break;
    }
#endif
    return tmp;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*----------------------------------------------------------------------
   passf2, passf3, passf4, passf5. Complex FFT passes fwd and bwd.
  ----------------------------------------------------------------------*/
//————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void passf2pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa) {
    uint16_t i, k, ah, ac;
    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            ah = 2 * k;
            ac = 4 * k;
            RE(ch[ah]) = RE(cc[ac]) + RE(cc[ac + 1]);
            RE(ch[ah + l1]) = RE(cc[ac]) - RE(cc[ac + 1]);
            IM(ch[ah]) = IM(cc[ac]) + IM(cc[ac + 1]);
            IM(ch[ah + l1]) = IM(cc[ac]) - IM(cc[ac + 1]);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ah = k * ido;
            ac = 2 * k * ido;
            for(i = 0; i < ido; i++) {
                complex_t t2;
                RE(ch[ah + i]) = RE(cc[ac + i]) + RE(cc[ac + i + ido]);
                RE(t2) = RE(cc[ac + i]) - RE(cc[ac + i + ido]);
                IM(ch[ah + i]) = IM(cc[ac + i]) + IM(cc[ac + i + ido]);
                IM(t2) = IM(cc[ac + i]) - IM(cc[ac + i + ido]);
#if 1
                ComplexMult(&IM(ch[ah + i + l1 * ido]), &RE(ch[ah + i + l1 * ido]), IM(t2), RE(t2), RE(wa[i]), IM(wa[i]));
#else
                ComplexMult(&RE(ch[ah + i + l1 * ido]), &IM(ch[ah + i + l1 * ido]), RE(t2), IM(t2), RE(wa[i]), IM(wa[i]));
#endif
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void passf2neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa) {
    uint16_t i, k, ah, ac;
    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            ah = 2 * k;
            ac = 4 * k;
            RE(ch[ah]) = RE(cc[ac]) + RE(cc[ac + 1]);
            RE(ch[ah + l1]) = RE(cc[ac]) - RE(cc[ac + 1]);
            IM(ch[ah]) = IM(cc[ac]) + IM(cc[ac + 1]);
            IM(ch[ah + l1]) = IM(cc[ac]) - IM(cc[ac + 1]);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ah = k * ido;
            ac = 2 * k * ido;
            for(i = 0; i < ido; i++) {
                complex_t t2;
                RE(ch[ah + i]) = RE(cc[ac + i]) + RE(cc[ac + i + ido]);
                RE(t2) = RE(cc[ac + i]) - RE(cc[ac + i + ido]);
                IM(ch[ah + i]) = IM(cc[ac + i]) + IM(cc[ac + i + ido]);
                IM(t2) = IM(cc[ac + i]) - IM(cc[ac + i + ido]);
#if 1
                ComplexMult(&RE(ch[ah + i + l1 * ido]), &IM(ch[ah + i + l1 * ido]), RE(t2), IM(t2), RE(wa[i]), IM(wa[i]));
#else
                ComplexMult(&IM(ch[ah + i + l1 * ido]), &RE(ch[ah + i + l1 * ido]), IM(t2), RE(t2), RE(wa[i]), IM(wa[i]));
#endif
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void passf3(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const int8_t isign) {
    static real_t taur = FRAC_CONST(-0.5);
    static real_t taui = FRAC_CONST(0.866025403784439);
    uint16_t      i, k, ac, ah;
    complex_t     c2, c3, d2, d3, t2;
    if(ido == 1) {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                ac = 3 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 1]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 1]);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), taur);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), taur);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2);
                RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + 1])), taui);
                IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + 1])), taui);
                RE(ch[ah + l1]) = RE(c2) - IM(c3);
                IM(ch[ah + l1]) = IM(c2) + RE(c3);
                RE(ch[ah + 2 * l1]) = RE(c2) + IM(c3);
                IM(ch[ah + 2 * l1]) = IM(c2) - RE(c3);
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                ac = 3 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 1]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 1]);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), taur);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), taur);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2);
                RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + 1])), taui);
                IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + 1])), taui);
                RE(ch[ah + l1]) = RE(c2) + IM(c3);
                IM(ch[ah + l1]) = IM(c2) - RE(c3);
                RE(ch[ah + 2 * l1]) = RE(c2) - IM(c3);
                IM(ch[ah + 2 * l1]) = IM(c2) + RE(c3);
            }
        }
    }
    else {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (3 * k + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + ido]);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), taur);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + ido]);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), taur);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2);
                    RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + ido])), taui);
                    IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + ido])), taui);
                    RE(d2) = RE(c2) - IM(c3);
                    IM(d3) = IM(c2) - RE(c3);
                    RE(d3) = RE(c2) + IM(c3);
                    IM(d2) = IM(c2) + RE(c3);
#if 1
                    ComplexMult(&IM(ch[ah + l1 * ido]), &RE(ch[ah + l1 * ido]), IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah + 2 * l1 * ido]), &RE(ch[ah + 2 * l1 * ido]), IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
#else
                    ComplexMult(&RE(ch[ah + l1 * ido]), &IM(ch[ah + l1 * ido]), RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah + 2 * l1 * ido]), &IM(ch[ah + 2 * l1 * ido]), RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
#endif
                }
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (3 * k + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + ido]);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), taur);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + ido]);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), taur);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2);
                    RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac + ido])), taui);
                    IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac + ido])), taui);
                    RE(d2) = RE(c2) + IM(c3);
                    IM(d3) = IM(c2) + RE(c3);
                    RE(d3) = RE(c2) - IM(c3);
                    IM(d2) = IM(c2) - RE(c3);
#if 1
                    ComplexMult(&RE(ch[ah + l1 * ido]), &IM(ch[ah + l1 * ido]), RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah + 2 * l1 * ido]), &IM(ch[ah + 2 * l1 * ido]), RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
#else
                    ComplexMult(&IM(ch[ah + l1 * ido]), &RE(ch[ah + l1 * ido]), IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah + 2 * l1 * ido]), &RE(ch[ah + 2 * l1 * ido]), IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
#endif
                }
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void passf4pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3) {
    uint16_t i, k, ac, ah;
    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            complex_t t1, t2, t3, t4;
            ac = 4 * k;
            ah = k;
            RE(t2) = RE(cc[ac]) + RE(cc[ac + 2]);
            RE(t1) = RE(cc[ac]) - RE(cc[ac + 2]);
            IM(t2) = IM(cc[ac]) + IM(cc[ac + 2]);
            IM(t1) = IM(cc[ac]) - IM(cc[ac + 2]);
            RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 3]);
            IM(t4) = RE(cc[ac + 1]) - RE(cc[ac + 3]);
            IM(t3) = IM(cc[ac + 3]) + IM(cc[ac + 1]);
            RE(t4) = IM(cc[ac + 3]) - IM(cc[ac + 1]);
            RE(ch[ah]) = RE(t2) + RE(t3);
            RE(ch[ah + 2 * l1]) = RE(t2) - RE(t3);
            IM(ch[ah]) = IM(t2) + IM(t3);
            IM(ch[ah + 2 * l1]) = IM(t2) - IM(t3);
            RE(ch[ah + l1]) = RE(t1) + RE(t4);
            RE(ch[ah + 3 * l1]) = RE(t1) - RE(t4);
            IM(ch[ah + l1]) = IM(t1) + IM(t4);
            IM(ch[ah + 3 * l1]) = IM(t1) - IM(t4);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ac = 4 * k * ido;
            ah = k * ido;
            for(i = 0; i < ido; i++) {
                complex_t c2, c3, c4, t1, t2, t3, t4;
                RE(t2) = RE(cc[ac + i]) + RE(cc[ac + i + 2 * ido]);
                RE(t1) = RE(cc[ac + i]) - RE(cc[ac + i + 2 * ido]);
                IM(t2) = IM(cc[ac + i]) + IM(cc[ac + i + 2 * ido]);
                IM(t1) = IM(cc[ac + i]) - IM(cc[ac + i + 2 * ido]);
                RE(t3) = RE(cc[ac + i + ido]) + RE(cc[ac + i + 3 * ido]);
                IM(t4) = RE(cc[ac + i + ido]) - RE(cc[ac + i + 3 * ido]);
                IM(t3) = IM(cc[ac + i + 3 * ido]) + IM(cc[ac + i + ido]);
                RE(t4) = IM(cc[ac + i + 3 * ido]) - IM(cc[ac + i + ido]);
                RE(c2) = RE(t1) + RE(t4);
                RE(c4) = RE(t1) - RE(t4);
                IM(c2) = IM(t1) + IM(t4);
                IM(c4) = IM(t1) - IM(t4);
                RE(ch[ah + i]) = RE(t2) + RE(t3);
                RE(c3) = RE(t2) - RE(t3);
                IM(ch[ah + i]) = IM(t2) + IM(t3);
                IM(c3) = IM(t2) - IM(t3);
#if 1
                ComplexMult(&IM(ch[ah + i + l1 * ido]), &RE(ch[ah + i + l1 * ido]), IM(c2), RE(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&IM(ch[ah + i + 2 * l1 * ido]), &RE(ch[ah + i + 2 * l1 * ido]), IM(c3), RE(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&IM(ch[ah + i + 3 * l1 * ido]), &RE(ch[ah + i + 3 * l1 * ido]), IM(c4), RE(c4), RE(wa3[i]), IM(wa3[i]));
#else
                ComplexMult(&RE(ch[ah + i + l1 * ido]), &IM(ch[ah + i + l1 * ido]), RE(c2), IM(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&RE(ch[ah + i + 2 * l1 * ido]), &IM(ch[ah + i + 2 * l1 * ido]), RE(c3), IM(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&RE(ch[ah + i + 3 * l1 * ido]), &IM(ch[ah + i + 3 * l1 * ido]), RE(c4), IM(c4), RE(wa3[i]), IM(wa3[i]));
#endif
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void passf4neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3) {
    uint16_t i, k, ac, ah;
    if(ido == 1) {
        for(k = 0; k < l1; k++) {
            complex_t t1, t2, t3, t4;
            ac = 4 * k;
            ah = k;
            RE(t2) = RE(cc[ac]) + RE(cc[ac + 2]);
            RE(t1) = RE(cc[ac]) - RE(cc[ac + 2]);
            IM(t2) = IM(cc[ac]) + IM(cc[ac + 2]);
            IM(t1) = IM(cc[ac]) - IM(cc[ac + 2]);
            RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 3]);
            IM(t4) = RE(cc[ac + 1]) - RE(cc[ac + 3]);
            IM(t3) = IM(cc[ac + 3]) + IM(cc[ac + 1]);
            RE(t4) = IM(cc[ac + 3]) - IM(cc[ac + 1]);
            RE(ch[ah]) = RE(t2) + RE(t3);
            RE(ch[ah + 2 * l1]) = RE(t2) - RE(t3);
            IM(ch[ah]) = IM(t2) + IM(t3);
            IM(ch[ah + 2 * l1]) = IM(t2) - IM(t3);
            RE(ch[ah + l1]) = RE(t1) - RE(t4);
            RE(ch[ah + 3 * l1]) = RE(t1) + RE(t4);
            IM(ch[ah + l1]) = IM(t1) - IM(t4);
            IM(ch[ah + 3 * l1]) = IM(t1) + IM(t4);
        }
    }
    else {
        for(k = 0; k < l1; k++) {
            ac = 4 * k * ido;
            ah = k * ido;
            for(i = 0; i < ido; i++) {
                complex_t c2, c3, c4, t1, t2, t3, t4;
                RE(t2) = RE(cc[ac + i]) + RE(cc[ac + i + 2 * ido]);
                RE(t1) = RE(cc[ac + i]) - RE(cc[ac + i + 2 * ido]);
                IM(t2) = IM(cc[ac + i]) + IM(cc[ac + i + 2 * ido]);
                IM(t1) = IM(cc[ac + i]) - IM(cc[ac + i + 2 * ido]);
                RE(t3) = RE(cc[ac + i + ido]) + RE(cc[ac + i + 3 * ido]);
                IM(t4) = RE(cc[ac + i + ido]) - RE(cc[ac + i + 3 * ido]);
                IM(t3) = IM(cc[ac + i + 3 * ido]) + IM(cc[ac + i + ido]);
                RE(t4) = IM(cc[ac + i + 3 * ido]) - IM(cc[ac + i + ido]);
                RE(c2) = RE(t1) - RE(t4);
                RE(c4) = RE(t1) + RE(t4);
                IM(c2) = IM(t1) - IM(t4);
                IM(c4) = IM(t1) + IM(t4);
                RE(ch[ah + i]) = RE(t2) + RE(t3);
                RE(c3) = RE(t2) - RE(t3);
                IM(ch[ah + i]) = IM(t2) + IM(t3);
                IM(c3) = IM(t2) - IM(t3);
#if 1
                ComplexMult(&RE(ch[ah + i + l1 * ido]), &IM(ch[ah + i + l1 * ido]), RE(c2), IM(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&RE(ch[ah + i + 2 * l1 * ido]), &IM(ch[ah + i + 2 * l1 * ido]), RE(c3), IM(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&RE(ch[ah + i + 3 * l1 * ido]), &IM(ch[ah + i + 3 * l1 * ido]), RE(c4), IM(c4), RE(wa3[i]), IM(wa3[i]));
#else
                ComplexMult(&IM(ch[ah + i + l1 * ido]), &RE(ch[ah + i + l1 * ido]), IM(c2), RE(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&IM(ch[ah + i + 2 * l1 * ido]), &RE(ch[ah + i + 2 * l1 * ido]), IM(c3), RE(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&IM(ch[ah + i + 3 * l1 * ido]), &RE(ch[ah + i + 3 * l1 * ido]), IM(c4), RE(c4), RE(wa3[i]), IM(wa3[i]));
#endif
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void passf5(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3, const complex_t* wa4,
                   const int8_t isign) {
    real_t tr11 = FRAC_CONST(0.309016994374947);
    real_t ti11 = FRAC_CONST(0.951056516295154);
    real_t tr12 = FRAC_CONST(-0.809016994374947);
    real_t ti12 = FRAC_CONST(0.587785252292473);
    uint16_t      i, k, ac, ah;
    complex_t     c2, c3, c4, c5, d3, d4, d5, d2, t2, t3, t4, t5;
    if(ido == 1) {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                ac = 5 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 3]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 3]);
                RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 2]);
                IM(t3) = IM(cc[ac + 1]) + IM(cc[ac + 2]);
                RE(t4) = RE(cc[ac + 1]) - RE(cc[ac + 2]);
                IM(t4) = IM(cc[ac + 1]) - IM(cc[ac + 2]);
                RE(t5) = RE(cc[ac]) - RE(cc[ac + 3]);
                IM(t5) = IM(cc[ac]) - IM(cc[ac + 3]);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2) + RE(t3);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2) + IM(t3);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                RE(c3) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                IM(c3) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                ComplexMult(&RE(c5), &RE(c4), ti11, ti12, RE(t5), RE(t4));
                ComplexMult(&IM(c5), &IM(c4), ti11, ti12, IM(t5), IM(t4));
                RE(ch[ah + l1]) = RE(c2) - IM(c5);
                IM(ch[ah + l1]) = IM(c2) + RE(c5);
                RE(ch[ah + 2 * l1]) = RE(c3) - IM(c4);
                IM(ch[ah + 2 * l1]) = IM(c3) + RE(c4);
                RE(ch[ah + 3 * l1]) = RE(c3) + IM(c4);
                IM(ch[ah + 3 * l1]) = IM(c3) - RE(c4);
                RE(ch[ah + 4 * l1]) = RE(c2) + IM(c5);
                IM(ch[ah + 4 * l1]) = IM(c2) - RE(c5);
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                ac = 5 * k + 1;
                ah = k;
                RE(t2) = RE(cc[ac]) + RE(cc[ac + 3]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac + 3]);
                RE(t3) = RE(cc[ac + 1]) + RE(cc[ac + 2]);
                IM(t3) = IM(cc[ac + 1]) + IM(cc[ac + 2]);
                RE(t4) = RE(cc[ac + 1]) - RE(cc[ac + 2]);
                IM(t4) = IM(cc[ac + 1]) - IM(cc[ac + 2]);
                RE(t5) = RE(cc[ac]) - RE(cc[ac + 3]);
                IM(t5) = IM(cc[ac]) - IM(cc[ac + 3]);
                RE(ch[ah]) = RE(cc[ac - 1]) + RE(t2) + RE(t3);
                IM(ch[ah]) = IM(cc[ac - 1]) + IM(t2) + IM(t3);
                RE(c2) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                IM(c2) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                RE(c3) = RE(cc[ac - 1]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                IM(c3) = IM(cc[ac - 1]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                ComplexMult(&RE(c4), &RE(c5), ti12, ti11, RE(t5), RE(t4));
                ComplexMult(&IM(c4), &IM(c5), ti12, ti11, IM(t5), IM(t4));
                RE(ch[ah + l1]) = RE(c2) + IM(c5);
                IM(ch[ah + l1]) = IM(c2) - RE(c5);
                RE(ch[ah + 2 * l1]) = RE(c3) + IM(c4);
                IM(ch[ah + 2 * l1]) = IM(c3) - RE(c4);
                RE(ch[ah + 3 * l1]) = RE(c3) - IM(c4);
                IM(ch[ah + 3 * l1]) = IM(c3) + RE(c4);
                RE(ch[ah + 4 * l1]) = RE(c2) - IM(c5);
                IM(ch[ah + 4 * l1]) = IM(c2) + RE(c5);
            }
        }
    }
    else {
        if(isign == 1) {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (k * 5 + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + 3 * ido]);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + 3 * ido]);
                    RE(t3) = RE(cc[ac + ido]) + RE(cc[ac + 2 * ido]);
                    IM(t3) = IM(cc[ac + ido]) + IM(cc[ac + 2 * ido]);
                    RE(t4) = RE(cc[ac + ido]) - RE(cc[ac + 2 * ido]);
                    IM(t4) = IM(cc[ac + ido]) - IM(cc[ac + 2 * ido]);
                    RE(t5) = RE(cc[ac]) - RE(cc[ac + 3 * ido]);
                    IM(t5) = IM(cc[ac]) - IM(cc[ac + 3 * ido]);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2) + RE(t3);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2) + IM(t3);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                    RE(c3) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                    IM(c3) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                    ComplexMult(&RE(c5), &RE(c4), ti11, ti12, RE(t5), RE(t4));
                    ComplexMult(&IM(c5), &IM(c4), ti11, ti12, IM(t5), IM(t4));
                    IM(d2) = IM(c2) + RE(c5);
                    IM(d3) = IM(c3) + RE(c4);
                    RE(d4) = RE(c3) + IM(c4);
                    RE(d5) = RE(c2) + IM(c5);
                    RE(d2) = RE(c2) - IM(c5);
                    IM(d5) = IM(c2) - RE(c5);
                    RE(d3) = RE(c3) - IM(c4);
                    IM(d4) = IM(c3) - RE(c4);
#if 1
                    ComplexMult(&IM(ch[ah + l1 * ido]), &RE(ch[ah + l1 * ido]), IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah + 2 * l1 * ido]), &RE(ch[ah + 2 * l1 * ido]), IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&IM(ch[ah + 3 * l1 * ido]), &RE(ch[ah + 3 * l1 * ido]), IM(d4), RE(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&IM(ch[ah + 4 * l1 * ido]), &RE(ch[ah + 4 * l1 * ido]), IM(d5), RE(d5), RE(wa4[i]), IM(wa4[i]));
#else
                    ComplexMult(&RE(ch[ah + l1 * ido]), &IM(ch[ah + l1 * ido]), RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah + 2 * l1 * ido]), &IM(ch[ah + 2 * l1 * ido]), RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&RE(ch[ah + 3 * l1 * ido]), &IM(ch[ah + 3 * l1 * ido]), RE(d4), IM(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&RE(ch[ah + 4 * l1 * ido]), &IM(ch[ah + 4 * l1 * ido]), RE(d5), IM(d5), RE(wa4[i]), IM(wa4[i]));
#endif
                }
            }
        }
        else {
            for(k = 0; k < l1; k++) {
                for(i = 0; i < ido; i++) {
                    ac = i + (k * 5 + 1) * ido;
                    ah = i + k * ido;
                    RE(t2) = RE(cc[ac]) + RE(cc[ac + 3 * ido]);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac + 3 * ido]);
                    RE(t3) = RE(cc[ac + ido]) + RE(cc[ac + 2 * ido]);
                    IM(t3) = IM(cc[ac + ido]) + IM(cc[ac + 2 * ido]);
                    RE(t4) = RE(cc[ac + ido]) - RE(cc[ac + 2 * ido]);
                    IM(t4) = IM(cc[ac + ido]) - IM(cc[ac + 2 * ido]);
                    RE(t5) = RE(cc[ac]) - RE(cc[ac + 3 * ido]);
                    IM(t5) = IM(cc[ac]) - IM(cc[ac + 3 * ido]);
                    RE(ch[ah]) = RE(cc[ac - ido]) + RE(t2) + RE(t3);
                    IM(ch[ah]) = IM(cc[ac - ido]) + IM(t2) + IM(t3);
                    RE(c2) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr11) + MUL_F(RE(t3), tr12);
                    IM(c2) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr11) + MUL_F(IM(t3), tr12);
                    RE(c3) = RE(cc[ac - ido]) + MUL_F(RE(t2), tr12) + MUL_F(RE(t3), tr11);
                    IM(c3) = IM(cc[ac - ido]) + MUL_F(IM(t2), tr12) + MUL_F(IM(t3), tr11);
                    ComplexMult(&RE(c4), &RE(c5), ti12, ti11, RE(t5), RE(t4));
                    ComplexMult(&IM(c4), &IM(c5), ti12, ti11, IM(t5), IM(t4));
                    IM(d2) = IM(c2) - RE(c5);
                    IM(d3) = IM(c3) - RE(c4);
                    RE(d4) = RE(c3) - IM(c4);
                    RE(d5) = RE(c2) - IM(c5);
                    RE(d2) = RE(c2) + IM(c5);
                    IM(d5) = IM(c2) + RE(c5);
                    RE(d3) = RE(c3) + IM(c4);
                    IM(d4) = IM(c3) + RE(c4);
#if 1
                    ComplexMult(&RE(ch[ah + l1 * ido]), &IM(ch[ah + l1 * ido]), RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah + 2 * l1 * ido]), &IM(ch[ah + 2 * l1 * ido]), RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&RE(ch[ah + 3 * l1 * ido]), &IM(ch[ah + 3 * l1 * ido]), RE(d4), IM(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&RE(ch[ah + 4 * l1 * ido]), &IM(ch[ah + 4 * l1 * ido]), RE(d5), IM(d5), RE(wa4[i]), IM(wa4[i]));
#else
                    ComplexMult(&IM(ch[ah + l1 * ido]), &RE(ch[ah + l1 * ido]), IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah + 2 * l1 * ido]), &RE(ch[ah + 2 * l1 * ido]), IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&IM(ch[ah + 3 * l1 * ido]), &RE(ch[ah + 3 * l1 * ido]), IM(d4), RE(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&IM(ch[ah + 4 * l1 * ido]), &RE(ch[ah + 4 * l1 * ido]), IM(d5), RE(d5), RE(wa4[i]), IM(wa4[i]));
#endif
                }
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*----------------------------------------------------------------------
   cfftf1, cfftf, cfftb, cffti1, cffti. Complex FFTs.
  ----------------------------------------------------------------------*/
void cfftf1pos(uint16_t n, complex_t* c, complex_t* ch, const uint16_t* ifac, const complex_t* wa, const int8_t isign) {
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1; (void)idl1;
    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;
    for(k1 = 2; k1 <= nf + 1; k1++) {
        ip = ifac[k1];
        l2 = ip * l1;
        ido = n / l2;
        idl1 = ido * l1;
        switch(ip) {
            case 4:
                ix2 = iw + ido;
                ix3 = ix2 + ido;
                if(na == 0) passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
                else passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
                na = 1 - na;
                break;
            case 2:
                if(na == 0) passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
                else passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
                na = 1 - na;
                break;
            case 3:
                ix2 = iw + ido;
                if(na == 0) passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
                else passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);
                na = 1 - na;
                break;
            case 5:
                ix2 = iw + ido;
                ix3 = ix2 + ido;
                ix4 = ix3 + ido;
                if(na == 0) passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
                else passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
                na = 1 - na;
                break;
        }
        l1 = l2;
        iw += (ip - 1) * ido;
    }
    if(na == 0) return;
    for(i = 0; i < n; i++) {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void cfftf1neg(uint16_t n, complex_t* c, complex_t* ch, const uint16_t* ifac, const complex_t* wa, const int8_t isign) {
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1; (void)idl1;
    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;
    for(k1 = 2; k1 <= nf + 1; k1++) {
        ip = ifac[k1];
        l2 = ip * l1;
        ido = n / l2;
        idl1 = ido * l1;
        switch(ip) {
            case 4:
                ix2 = iw + ido;
                ix3 = ix2 + ido;
                if(na == 0) passf4neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
                else passf4neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
                na = 1 - na;
                break;
            case 2:
                if(na == 0) passf2neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
                else passf2neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
                na = 1 - na;
                break;
            case 3:
                ix2 = iw + ido;
                if(na == 0) passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
                else passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);
                na = 1 - na;
                break;
            case 5:
                ix2 = iw + ido;
                ix3 = ix2 + ido;
                ix4 = ix3 + ido;
                if(na == 0) passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
                else passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
                na = 1 - na;
                break;
        }
        l1 = l2;
        iw += (ip - 1) * ido;
    }
    if(na == 0) return;
    for(i = 0; i < n; i++) {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void cfftf(cfft_info* cfft, complex_t* c) { cfftf1neg(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, -1); }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void cfftb(cfft_info* cfft, complex_t* c) { cfftf1pos(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, +1); }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void cffti1(uint16_t n, complex_t* wa, uint16_t* ifac) {
    uint16_t ntryh[4] = {3, 4, 2, 5};
#ifndef FIXED_POINT
    real_t   arg, argh, argld, fi;
    uint16_t ido, ipm;
    uint16_t i1, k1, l1, l2;
    uint16_t ld, ii, ip;
#endif
    uint16_t ntry = 0, i, j;
    uint16_t ib;
    uint16_t nf, nl, nq, nr;
    nl = n;
    nf = 0;
    j = 0;
startloop:
    j++;
    if(j <= 4) ntry = ntryh[j - 1];
    else ntry += 2;
    do {
        nq = nl / ntry;
        nr = nl - ntry * nq;
        if(nr != 0) goto startloop;
        nf++;
        ifac[nf + 1] = ntry;
        nl = nq;
        if(ntry == 2 && nf != 1) {
            for(i = 2; i <= nf; i++) {
                ib = nf - i + 2;
                ifac[ib + 1] = ifac[ib];
            }
            ifac[2] = 2;
        }
    } while(nl != 1);
    ifac[0] = n;
    ifac[1] = nf;
#ifndef FIXED_POINT
    argh = (real_t)2.0 * (real_t)M_PI / (real_t)n;
    i = 0;
    l1 = 1;
    for(k1 = 1; k1 <= nf; k1++) {
        ip = ifac[k1 + 1];
        ld = 0;
        l2 = l1 * ip;
        ido = n / l2;
        ipm = ip - 1;
        for(j = 0; j < ipm; j++) {
            i1 = i;
            RE(wa[i]) = 1.0;
            IM(wa[i]) = 0.0;
            ld += l1;
            fi = 0;
            argld = ld * argh;
            for(ii = 0; ii < ido; ii++) {
                i++;
                fi++;
                arg = fi * argld;
                RE(wa[i]) = (real_t)cos(arg);
    #if 1
                IM(wa[i]) = (real_t)sin(arg);
    #else
                IM(wa[i]) = (real_t)-sin(arg);
    #endif
            }
            if(ip > 5) {
                RE(wa[i1]) = RE(wa[i]);
                IM(wa[i1]) = IM(wa[i]);
            }
        }
        l1 = l2;
    }
#endif
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
cfft_info* cffti(uint16_t n) {
    cfft_info* cfft = (cfft_info*)faad_malloc(sizeof(cfft_info));
    cfft->n = n;
    cfft->work = (complex_t*)faad_malloc(n * sizeof(complex_t));
#ifndef FIXED_POINT
    cfft->tab = (complex_t*)faad_malloc(n * sizeof(complex_t));
    cffti1(n, cfft->tab, cfft->ifac);
#else
    cffti1(n, NULL, cfft->ifac);
    switch(n) {
        case 64: cfft->tab = (complex_t*)cfft_tab_64; break;
        case 512: cfft->tab = (complex_t*)cfft_tab_512; break;
    #ifdef LD_DEC
        case 256: cfft->tab = (complex_t*)cfft_tab_256; break;
    #endif
    #ifdef ALLOW_SMALL_FRAMELENGTH
        case 60: cfft->tab = (complex_t*)cfft_tab_60; break;
        case 480: cfft->tab = (complex_t*)cfft_tab_480; break;
        #ifdef LD_DEC
        case 240: cfft->tab = (complex_t*)cfft_tab_240; break;
        #endif
    #endif
        case 128: cfft->tab = (complex_t*)cfft_tab_128; break;
    }
#endif
    return cfft;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void cfftu(cfft_info* cfft) {
    if(cfft->work) faad_free(cfft->work);
#ifndef FIXED_POINT
    if(cfft->tab) faad_free(cfft->tab);
#endif
    if(cfft) faad_free(cfft);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
drc_info* drc_init(real_t cut, real_t boost) {
    drc_info* drc = (drc_info*)faad_malloc(sizeof(drc_info));
    memset(drc, 0, sizeof(drc_info));
    drc->ctrl1 = cut;
    drc->ctrl2 = boost;
    drc->num_bands = 1;
    drc->band_top[0] = 1024 / 4 - 1;
    drc->dyn_rng_sgn[0] = 1;
    drc->dyn_rng_ctl[0] = 0;
    return drc;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void drc_end(drc_info* drc) {
    if(drc) faad_free(drc);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void drc_decode(drc_info* drc, real_t* spec) {
    uint16_t i, bd, top;
#ifdef FIXED_POINT
    int32_t exp, frac;
#else
    real_t factor, exp;
#endif
    uint16_t bottom = 0;
    if(drc->num_bands == 1) drc->band_top[0] = 1024 / 4 - 1;
    for(bd = 0; bd < drc->num_bands; bd++) {
        top = 4 * (drc->band_top[bd] + 1);
#ifndef FIXED_POINT
        /* Decode DRC gain factor */
        if(drc->dyn_rng_sgn[bd]) /* compress */
            exp = ((-drc->ctrl1 * drc->dyn_rng_ctl[bd]) - (DRC_REF_LEVEL - drc->prog_ref_level)) / REAL_CONST(24.0);
        else /* boost */ exp = ((drc->ctrl2 * drc->dyn_rng_ctl[bd]) - (DRC_REF_LEVEL - drc->prog_ref_level)) / REAL_CONST(24.0);
        factor = (real_t)pow(2.0, exp);
        /* Apply gain factor */
        for(i = bottom; i < top; i++) spec[i] *= factor;
#else
        /* Decode DRC gain factor */
        if(drc->dyn_rng_sgn[bd]) /* compress */
        {
            exp = -1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) / 24;
            frac = -1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) % 24;
        }
        else { /* boost */ exp = (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) / 24;
            frac = (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) % 24;
        }
        /* Apply gain factor */
        if(exp < 0) {
            for(i = bottom; i < top; i++) {
                spec[i] >>= -exp;
                if(frac) spec[i] = MUL_R(spec[i], drc_pow2_table[frac + 23]);
            }
        }
        else {
            for(i = bottom; i < top; i++) {
                spec[i] <<= exp;
                if(frac) spec[i] = MUL_R(spec[i], drc_pow2_table[frac + 23]);
            }
        }
#endif
        bottom = top;
    }
}
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
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void ms_decode(ic_stream* ics, ic_stream* icsr, real_t* l_spec, real_t* r_spec, uint16_t frame_len) {
    uint8_t  g, b, sfb;
    uint8_t  group = 0;
    uint16_t nshort = frame_len / 8;
    uint16_t i, k;
    real_t   tmp;
    if(ics->ms_mask_present >= 1) {
        for(g = 0; g < ics->num_window_groups; g++) {
            for(b = 0; b < ics->window_group_length[g]; b++) {
                for(sfb = 0; sfb < ics->max_sfb; sfb++) {
                    /* If intensity stereo coding or noise substitution is on
                       for a particular scalefactor band, no M/S stereo decoding
                       is carried out.
                     */
                    if((ics->ms_used[g][sfb] || ics->ms_mask_present == 2) && !is_intensity(icsr, g, sfb) && !is_noise(ics, g, sfb)) {
                        for(i = ics->swb_offset[sfb]; i < min(ics->swb_offset[sfb + 1], ics->swb_offset_max); i++) {
                            k = (group * nshort) + i;
                            tmp = l_spec[k] - r_spec[k];
                            l_spec[k] = l_spec[k] + r_spec[k];
                            r_spec[k] = tmp;
                        }
                    }
                }
                group++;
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t is_intensity(ic_stream* ics, uint8_t group, uint8_t sfb) {
    switch (ics->sfb_cb[group][sfb]) {
        case INTENSITY_HCB: return 1;
        case INTENSITY_HCB2: return -1;
        default: return 0;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t invert_intensity(ic_stream* ics, uint8_t group, uint8_t sfb) {
    if (ics->ms_mask_present == 1) return (1 - 2 * ics->ms_used[group][sfb]);
    return 1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t is_noise(ic_stream* ics, uint8_t group, uint8_t sfb) {
    if (ics->sfb_cb[group][sfb] == NOISE_HCB) return 1;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
static inline real_t get_sample(real_t** input, uint8_t channel, uint16_t sample, uint8_t down_matrix, uint8_t* internal_channel) {
    if(!down_matrix) return input[internal_channel[channel]][sample];
    if(channel == 0) { return DM_MUL * (input[internal_channel[1]][sample] + input[internal_channel[0]][sample] * RSQRT2 + input[internal_channel[3]][sample] * RSQRT2); }
    else { return DM_MUL * (input[internal_channel[2]][sample] + input[internal_channel[0]][sample] * RSQRT2 + input[internal_channel[4]][sample] * RSQRT2); }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
    #ifndef HAS_LRINTF
        #define CLIP(sample, max, min)          \
            if(sample >= 0.0f) {                \
                sample += 0.5f;                 \
                if(sample >= max) sample = max; \
            }                                   \
            else {                              \
                sample += -0.5f;                \
                if(sample <= min) sample = min; \
            }
    #else
        #define CLIP(sample, max, min)          \
            if(sample >= 0.0f) {                \
                if(sample >= max) sample = max; \
            }                                   \
            else {                              \
                if(sample <= min) sample = min; \
            }
    #endif
    #define CONV(a, b) ((a << 1) | (b & 0x1))
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
static void to_PCM_16bit(NeAACDecStruct* hDecoder, real_t** input, uint8_t channels, uint16_t frame_len, int16_t** sample_buffer) {
    uint8_t  ch, ch1;
    uint16_t i;

    switch (CONV(channels, hDecoder->downMatrix)) {
        case CONV(1, 0):
        case CONV(1, 1):
            for (i = 0; i < frame_len; i++) {
                real_t inp = input[hDecoder->internal_channel[0]][i];

                CLIP(inp, 32767.0f, -32768.0f);

                (*sample_buffer)[i] = (int16_t)lrintf(inp);
            }
            break;
        case CONV(2, 0):
            if (hDecoder->upMatrix) {
                ch = hDecoder->internal_channel[0];
                for (i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];

                    CLIP(inp0, 32767.0f, -32768.0f);

                    (*sample_buffer)[(i * 2) + 0] = (int16_t)lrintf(inp0);
                    (*sample_buffer)[(i * 2) + 1] = (int16_t)lrintf(inp0);
                }
            } else {
                ch = hDecoder->internal_channel[0];
                ch1 = hDecoder->internal_channel[1];
                for (i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];
                    real_t inp1 = input[ch1][i];

                    CLIP(inp0, 32767.0f, -32768.0f);
                    CLIP(inp1, 32767.0f, -32768.0f);

                    (*sample_buffer)[(i * 2) + 0] = (int16_t)lrintf(inp0);
                    (*sample_buffer)[(i * 2) + 1] = (int16_t)lrintf(inp1);
                }
            }
            break;
        default:
            for (ch = 0; ch < channels; ch++) {
                for (i = 0; i < frame_len; i++) {
                    real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                    CLIP(inp, 32767.0f, -32768.0f);

                    (*sample_buffer)[(i * channels) + ch] = (int16_t)lrintf(inp);
                }
            }
            break;
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
void to_PCM_24bit(NeAACDecStruct* hDecoder, real_t** input, uint8_t channels, uint16_t frame_len, int32_t **sample_buffer) {
    uint8_t  ch, ch1;
    uint16_t i;

    switch (CONV(channels, hDecoder->downMatrix)) {
        case CONV(1, 0):
        case CONV(1, 1):
            for (i = 0; i < frame_len; i++) {
                real_t inp = input[hDecoder->internal_channel[0]][i];

                inp *= 256.0f;
                CLIP(inp, 8388607.0f, -8388608.0f);

                (*sample_buffer)[i] = (int32_t)lrintf(inp);
            }
            break;
        case CONV(2, 0):
            if (hDecoder->upMatrix) {
                ch = hDecoder->internal_channel[0];
                for (i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];

                    inp0 *= 256.0f;
                    CLIP(inp0, 8388607.0f, -8388608.0f);

                    (*sample_buffer)[(i * 2) + 0] = (int32_t)lrintf(inp0);
                    (*sample_buffer)[(i * 2) + 1] = (int32_t)lrintf(inp0);
                }
            } else {
                ch = hDecoder->internal_channel[0];
                ch1 = hDecoder->internal_channel[1];
                for (i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];
                    real_t inp1 = input[ch1][i];

                    inp0 *= 256.0f;
                    inp1 *= 256.0f;
                    CLIP(inp0, 8388607.0f, -8388608.0f);
                    CLIP(inp1, 8388607.0f, -8388608.0f);

                    (*sample_buffer)[(i * 2) + 0] = (int32_t)lrintf(inp0);
                    (*sample_buffer)[(i * 2) + 1] = (int32_t)lrintf(inp1);
                }
            }
            break;
        default:
            for (ch = 0; ch < channels; ch++) {
                for (i = 0; i < frame_len; i++) {
                    real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                    inp *= 256.0f;
                    CLIP(inp, 8388607.0f, -8388608.0f);

                    (*sample_buffer)[(i * channels) + ch] = (int32_t)lrintf(inp);
                }
            }
            break;
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
static void to_PCM_32bit(NeAACDecStruct *hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            inp *= 65536.0f;
            CLIP(inp, 2147483647.0f, -2147483648.0f);

            (*sample_buffer)[i] = (int32_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        if (hDecoder->upMatrix)
        {
            ch = hDecoder->internal_channel[0];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch][i];

                inp0 *= 65536.0f;
                CLIP(inp0, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp0);
            }
        } else {
            ch  = hDecoder->internal_channel[0];
            ch1 = hDecoder->internal_channel[1];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch ][i];
                real_t inp1 = input[ch1][i];

                inp0 *= 65536.0f;
                inp1 *= 65536.0f;
                CLIP(inp0, 2147483647.0f, -2147483648.0f);
                CLIP(inp1, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp1);
            }
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                inp *= 65536.0f;
                CLIP(inp, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*channels)+ch] = (int32_t)lrintf(inp);
            }
        }
        break;
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
static void to_PCM_float(NeAACDecStruct* hDecoder, real_t** input, uint8_t channels, uint16_t frame_len, float32_t** sample_buffer) {
    uint8_t  ch, ch1;
    uint16_t i;
    switch(CONV(channels, hDecoder->downMatrix)) {
        case CONV(1, 0):
        case CONV(1, 1):
            for(i = 0; i < frame_len; i++) {
                real_t inp = input[hDecoder->internal_channel[0]][i];
                (*sample_buffer)[i] = inp * FLOAT_SCALE;
            }
            break;
        case CONV(2, 0):
            if(hDecoder->upMatrix) {
                ch = hDecoder->internal_channel[0];
                for(i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];
                    (*sample_buffer)[(i * 2) + 0] = inp0 * FLOAT_SCALE;
                    (*sample_buffer)[(i * 2) + 1] = inp0 * FLOAT_SCALE;
                }
            }
            else {
                ch = hDecoder->internal_channel[0];
                ch1 = hDecoder->internal_channel[1];
                for(i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];
                    real_t inp1 = input[ch1][i];
                    (*sample_buffer)[(i * 2) + 0] = inp0 * FLOAT_SCALE;
                    (*sample_buffer)[(i * 2) + 1] = inp1 * FLOAT_SCALE;
                }
            }
            break;
        default:
            for(ch = 0; ch < channels; ch++) {
                for(i = 0; i < frame_len; i++) {
                    real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                    (*sample_buffer)[(i * channels) + ch] = inp * FLOAT_SCALE;
                }
            }
            break;
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
static void to_PCM_double(NeAACDecStruct* hDecoder, real_t** input, uint8_t channels, uint16_t frame_len, double** sample_buffer) {
    uint8_t  ch, ch1;
    uint16_t i;
    switch(CONV(channels, hDecoder->downMatrix)) {
        case CONV(1, 0):
        case CONV(1, 1):
            for(i = 0; i < frame_len; i++) {
                real_t inp = input[hDecoder->internal_channel[0]][i];
                (*sample_buffer)[i] = (double)inp * FLOAT_SCALE;
            }
            break;
        case CONV(2, 0):
            if(hDecoder->upMatrix) {
                ch = hDecoder->internal_channel[0];
                for(i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];
                    (*sample_buffer)[(i * 2) + 0] = (double)inp0 * FLOAT_SCALE;
                    (*sample_buffer)[(i * 2) + 1] = (double)inp0 * FLOAT_SCALE;
                }
            }
            else {
                ch = hDecoder->internal_channel[0];
                ch1 = hDecoder->internal_channel[1];
                for(i = 0; i < frame_len; i++) {
                    real_t inp0 = input[ch][i];
                    real_t inp1 = input[ch1][i];
                    (*sample_buffer)[(i * 2) + 0] = (double)inp0 * FLOAT_SCALE;
                    (*sample_buffer)[(i * 2) + 1] = (double)inp1 * FLOAT_SCALE;
                }
            }
            break;
        default:
            for(ch = 0; ch < channels; ch++) {
                for(i = 0; i < frame_len; i++) {
                    real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                    (*sample_buffer)[(i * channels) + ch] = (double)inp * FLOAT_SCALE;
                }
            }
            break;
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifndef FIXED_POINT
void* output_to_PCM(NeAACDecStruct* hDecoder, real_t** input, void* sample_buffer, uint8_t channels, uint16_t frame_len, uint8_t format) {
    int16_t*   short_sample_buffer = (int16_t*)sample_buffer;
    int32_t*   int_sample_buffer = (int32_t*)sample_buffer;
    float32_t* float_sample_buffer = (float32_t*)sample_buffer;
    double*    double_sample_buffer = (double*)sample_buffer;
    #ifdef PROFILE
    int64_t count = faad_get_ts();
    #endif
    /* Copy output to a standard PCM buffer */
    switch(format) {
        case FAAD_FMT_16BIT: to_PCM_16bit(hDecoder, input, channels, frame_len, &short_sample_buffer); break;
        case FAAD_FMT_24BIT: to_PCM_24bit(hDecoder, input, channels, frame_len, &int_sample_buffer); break;
        case FAAD_FMT_32BIT: to_PCM_32bit(hDecoder, input, channels, frame_len, &int_sample_buffer); break;
        case FAAD_FMT_FLOAT: to_PCM_float(hDecoder, input, channels, frame_len, &float_sample_buffer); break;
        case FAAD_FMT_DOUBLE: to_PCM_double(hDecoder, input, channels, frame_len, &double_sample_buffer); break;
    }
    #ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->output_cycles += count;
    #endif
    return sample_buffer;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//#ifdef FIXED_POINT
//    #define DM_MUL FRAC_CONST(0.3203772410170407)    // 1/(1+sqrt(2) + 1/sqrt(2))
//    #define RSQRT2 FRAC_CONST(0.7071067811865475244) // 1/sqrt(2)
//#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef FIXED_POINT
    #define DM_MUL FRAC_CONST(0.3203772410170407)    // 1/(1+sqrt(2) + 1/sqrt(2))
    #define RSQRT2 FRAC_CONST(0.7071067811865475244) // 1/sqrt(2)
static inline real_t get_sample(real_t** input, uint8_t channel, uint16_t sample, uint8_t down_matrix, uint8_t up_matrix, uint8_t* internal_channel) {
    if(up_matrix == 1) return input[internal_channel[0]][sample];
    if(!down_matrix) return input[internal_channel[channel]][sample];
    if(channel == 0) {
        real_t C = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        real_t L_S = MUL_F(input[internal_channel[3]][sample], RSQRT2);
        real_t cum = input[internal_channel[1]][sample] + C + L_S;
        return MUL_F(cum, DM_MUL);
    }
    else {
        real_t C = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        real_t R_S = MUL_F(input[internal_channel[4]][sample], RSQRT2);
        real_t cum = input[internal_channel[2]][sample] + C + R_S;
        return MUL_F(cum, DM_MUL);
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef FIXED_POINT
void* output_to_PCM(NeAACDecStruct* hDecoder, real_t** input, void* sample_buffer, uint8_t channels, uint16_t frame_len, uint8_t format) {
    uint8_t  ch;
    uint16_t i;
    int16_t* short_sample_buffer = (int16_t*)sample_buffer;
    int32_t* int_sample_buffer = (int32_t*)sample_buffer;
    /* Copy output to a standard PCM buffer */
    for(ch = 0; ch < channels; ch++) {
        switch(format) {
            case FAAD_FMT_16BIT:
                for(i = 0; i < frame_len; i++) {
                    int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                    if(tmp >= 0) {
                        tmp += (1 << (REAL_BITS - 1));
                        if(tmp >= REAL_CONST(32767)) { tmp = REAL_CONST(32767); }
                    }
                    else {
                        tmp += -(1 << (REAL_BITS - 1));
                        if(tmp <= REAL_CONST(-32768)) { tmp = REAL_CONST(-32768); }
                    }
                    tmp >>= REAL_BITS;
                    short_sample_buffer[(i * channels) + ch] = (int16_t)tmp;
                }
                break;
            case FAAD_FMT_24BIT:
                for(i = 0; i < frame_len; i++) {
                    int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                    if(tmp >= 0) {
                        tmp += (1 << (REAL_BITS - 9));
                        tmp >>= (REAL_BITS - 8);
                        if(tmp >= 8388607) { tmp = 8388607; }
                    }
                    else {
                        tmp += -(1 << (REAL_BITS - 9));
                        tmp >>= (REAL_BITS - 8);
                        if(tmp <= -8388608) { tmp = -8388608; }
                    }
                    int_sample_buffer[(i * channels) + ch] = (int32_t)tmp;
                }
                break;
            case FAAD_FMT_32BIT:
                for(i = 0; i < frame_len; i++) {
                    int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                    if(tmp >= 0) {
                        tmp += (1 << (16 - REAL_BITS - 1));
                        tmp <<= (16 - REAL_BITS);
                    }
                    else {
                        tmp += -(1 << (16 - REAL_BITS - 1));
                        tmp <<= (16 - REAL_BITS);
                    }
                    int_sample_buffer[(i * channels) + ch] = (int32_t)tmp;
                }
                break;
            case FAAD_FMT_FIXED:
                for(i = 0; i < frame_len; i++) {
                    real_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix, hDecoder->internal_channel);
                    int_sample_buffer[(i * channels) + ch] = (int32_t)tmp;
                }
                break;
        }
    }
    return sample_buffer;
}
#endif //FIXED_POINT
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* The function gen_rand_vector(addr, size) generates a vector of length <size> with signed random values of average energy MEAN_NRG per random
   value. A suitable random number generator can be realized using one multiplication/accumulation per random value.*/
void gen_rand_vector(real_t* spec, int16_t scale_factor, uint16_t size, uint8_t sub, uint32_t* __r1, uint32_t* __r2) {
#ifndef FIXED_POINT
    uint16_t i;
    real_t   energy = 0.0;
    real_t   scale = (real_t)1.0 / (real_t)size;
    for (i = 0; i < size; i++) {
        real_t tmp = scale * (real_t)(int32_t)ne_rng(__r1, __r2);
        spec[i] = tmp;
        energy += tmp * tmp;
    }
    scale = (real_t)1.0 / (real_t)sqrt(energy);
    scale *= (real_t)pow(2.0, 0.25 * scale_factor);
    for (i = 0; i < size; i++) { spec[i] *= scale; }
#else
    uint16_t i;
    real_t   energy = 0, scale;
    int32_t  exp, frac;
    for (i = 0; i < size; i++) {
        /* this can be replaced by a 16 bit random generator!!!! */
        real_t tmp = (int32_t)ne_rng(__r1, __r2);
        if (tmp < 0)
            tmp = -(tmp & ((1 << (REAL_BITS - 1)) - 1));
        else
            tmp = (tmp & ((1 << (REAL_BITS - 1)) - 1));
        energy += MUL_R(tmp, tmp);
        spec[i] = tmp;
    }
    energy = fp_sqrt(energy);
    if (energy > 0) {
        scale = DIV(REAL_CONST(1), energy);
        exp = scale_factor >> 2;
        frac = scale_factor & 3;
        /* IMDCT pre-scaling */
        exp -= sub;
        if (exp < 0)
            scale >>= -exp;
        else
            scale <<= exp;
        if (frac) scale = MUL_C(scale, pow2_table[frac]);
        for (i = 0; i < size; i++) { spec[i] = MUL_R(spec[i], scale); }
    }
#endif
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void pns_decode(ic_stream* ics_left, ic_stream* ics_right, real_t* spec_left, real_t* spec_right, uint16_t frame_len, uint8_t channel_pair, uint8_t object_type,
                /* RNG states */ uint32_t* __r1, uint32_t* __r2) {
    uint8_t  g, sfb, b;
    uint16_t size, offs;
    uint8_t  group = 0;
    uint16_t nshort = frame_len >> 3;
    uint8_t sub = 0;
#ifdef FIXED_POINT
    /* IMDCT scaling */
    if(object_type == LD) { sub = 9 /*9*/; }
    else {
        if(ics_left->window_sequence == EIGHT_SHORT_SEQUENCE) sub = 7 /*7*/;
        else sub = 10 /*10*/;
    }
#endif
    for(g = 0; g < ics_left->num_window_groups; g++) {
        /* Do perceptual noise substitution decoding */
        for(b = 0; b < ics_left->window_group_length[g]; b++) {
            for(sfb = 0; sfb < ics_left->max_sfb; sfb++) {
                uint32_t r1_dep = 0, r2_dep = 0;
                if(is_noise(ics_left, g, sfb)) {
#ifdef LTP_DEC
                    /* Simultaneous use of LTP and PNS is not prevented in the
                       syntax. If both LTP, and PNS are enabled on the same
                       scalefactor band, PNS takes precedence, and no prediction
                       is applied to this band.
                    */
                    ics_left->ltp.long_used[sfb] = 0;
                    ics_left->ltp2.long_used[sfb] = 0;
#endif
#ifdef MAIN_DEC
                    /* For scalefactor bands coded using PNS the corresponding
                       predictors are switched to "off".
                    */
                    ics_left->pred.prediction_used[sfb] = 0;
#endif
                    offs = ics_left->swb_offset[sfb];
                    size = min(ics_left->swb_offset[sfb + 1], ics_left->swb_offset_max) - offs;
                    r1_dep = *__r1;
                    r2_dep = *__r2;
                    /* Generate random vector */
                    gen_rand_vector(&spec_left[(group * nshort) + offs], ics_left->scale_factors[g][sfb], size, sub, __r1, __r2);
                }
                /* From the spec:
                   If the same scalefactor band and group is coded by perceptual noise
                   substitution in both channels of a channel pair, the correlation of
                   the noise signal can be controlled by means of the ms_used field: While
                   the default noise generation process works independently for each channel
                   (separate generation of random vectors), the same random vector is used
                   for both channels if ms_used[] is set for a particular scalefactor band
                   and group. In this case, no M/S stereo coding is carried out (because M/S
                   stereo coding and noise substitution coding are mutually exclusive).
                   If the same scalefactor band and group is coded by perceptual noise
                   substitution in only one channel of a channel pair the setting of ms_used[]
                   is not evaluated.
                */
                if((ics_right != NULL) && is_noise(ics_right, g, sfb)) {
#ifdef LTP_DEC
                    /* See comment above. */
                    ics_right->ltp.long_used[sfb] = 0;
                    ics_right->ltp2.long_used[sfb] = 0;
#endif
#ifdef MAIN_DEC
                    /* See comment above. */
                    ics_right->pred.prediction_used[sfb] = 0;
#endif
                    if(channel_pair && is_noise(ics_left, g, sfb) && (((ics_left->ms_mask_present == 1) && (ics_left->ms_used[g][sfb])) || (ics_left->ms_mask_present == 2))) {
                        /*uint16_t c;*/
                        offs = ics_right->swb_offset[sfb];
                        size = min(ics_right->swb_offset[sfb + 1], ics_right->swb_offset_max) - offs;
                        /* Generate random vector dependent on left channel*/
                        gen_rand_vector(&spec_right[(group * nshort) + offs], ics_right->scale_factors[g][sfb], size, sub, &r1_dep, &r2_dep);
                    }
                    else /*if (ics_left->ms_mask_present == 0)*/ {
                        offs = ics_right->swb_offset[sfb];
                        size = min(ics_right->swb_offset[sfb + 1], ics_right->swb_offset_max) - offs;
                        /* Generate random vector */
                        gen_rand_vector(&spec_right[(group * nshort) + offs], ics_right->scale_factors[g][sfb], size, sub, __r1, __r2);
                    }
                }
            } /* sfb */
            group++;
        } /* b */
    } /* g */
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t huffman_scale_factor(bitfile* ld) {
    uint16_t offset = 0;
    while(hcb_sf[offset][1]) {
        uint8_t b = faad_get1bit(ld);
        offset += hcb_sf[offset][b];
        if(offset > 240) {
            /* printf("ERROR: offset into hcb_sf = %d >240!\n", offset); */
            return -1;
        }
    }
    return hcb_sf[offset][0];
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const hcb* hcb_table[] = {0, hcb1_1, hcb2_1, 0, hcb4_1, 0, hcb6_1, 0, hcb8_1, 0, hcb10_1, hcb11_1};
const hcb_2_quad* hcb_2_quad_table[] = {0, hcb1_2, hcb2_2, 0, hcb4_2, 0, 0, 0, 0, 0, 0, 0};
const hcb_2_pair* hcb_2_pair_table[] = {0, 0, 0, 0, 0, 0, hcb6_2, 0, hcb8_2, 0, hcb10_2, hcb11_2};
const hcb_bin_pair* hcb_bin_table[] = {0, 0, 0, 0, 0, hcb5, 0, hcb7, 0, hcb9, 0, 0};
uint8_t hcbN[] = {0, 5, 5, 0, 5, 0, 5, 0, 5, 0, 6, 5};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* defines whether a huffman codebook is unsigned or not */
/* Table 4.6.2 */
uint8_t unsigned_cb[] = {
    0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
int hcb_2_quad_table_size[] = {0, 114, 86, 0, 185, 0, 0, 0, 0, 0, 0, 0};
int hcb_2_pair_table_size[] = {0, 0, 0, 0, 0, 0, 126, 0, 83, 0, 210, 373};
int hcb_bin_table_size[] = {0, 0, 0, 161, 0, 161, 0, 127, 0, 337, 0, 0};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void huffman_sign_bits(bitfile* ld, int16_t* sp, uint8_t len) {
    uint8_t i;
    for(i = 0; i < len; i++) {
        if(sp[i]) {
            if(faad_get1bit(ld) & 1) { sp[i] = -sp[i]; }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_getescape(bitfile* ld, int16_t* sp) {
    uint8_t neg, i;
    int16_t j;
    int16_t off;
    int16_t x = *sp;
    if(x < 0) {
        if(x != -16) return 0;
        neg = 1;
    }
    else {
        if(x != 16) return 0;
        neg = 0;
    }
    for(i = 4; i < 16; i++) {
        if(faad_get1bit(ld) == 0) { break; }
    }
    if(i >= 16) return 10;
    off = (int16_t)faad_getbits(ld, i);
    j = off | (1 << i);
    if(neg) j = -j;
    *sp = j;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_2step_quad(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t  extra_bits;
    cw = faad_showbits(ld, hcbN[cb]);
    offset = hcb_table[cb][cw].offset;
    extra_bits = hcb_table[cb][cw].extra_bits;
    if(extra_bits) {
        /* we know for sure it's more than hcbN[cb] bits long */
        faad_flushbits(ld, hcbN[cb]);
        offset += (uint16_t)faad_showbits(ld, extra_bits);
        faad_flushbits(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb]);
    }
    else { faad_flushbits(ld, hcb_2_quad_table[cb][offset].bits); }
    if(offset > hcb_2_quad_table_size[cb]) {
        /* printf("ERROR: offset into hcb_2_quad_table = %d >%d!\n", offset,
           hcb_2_quad_table_size[cb]); */
        return 10;
    }
    sp[0] = hcb_2_quad_table[cb][offset].x;
    sp[1] = hcb_2_quad_table[cb][offset].y;
    sp[2] = hcb_2_quad_table[cb][offset].v;
    sp[3] = hcb_2_quad_table[cb][offset].w;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_2step_quad_sign(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint8_t err = huffman_2step_quad(cb, ld, sp);
    huffman_sign_bits(ld, sp, QUAD_LEN);
    return err;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_2step_pair(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t  extra_bits;
    cw = faad_showbits(ld, hcbN[cb]);
    offset = hcb_table[cb][cw].offset;
    extra_bits = hcb_table[cb][cw].extra_bits;
    if(extra_bits) {
        /* we know for sure it's more than hcbN[cb] bits long */
        faad_flushbits(ld, hcbN[cb]);
        offset += (uint16_t)faad_showbits(ld, extra_bits);
        faad_flushbits(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb]);
    }
    else { faad_flushbits(ld, hcb_2_pair_table[cb][offset].bits); }
    if(offset > hcb_2_pair_table_size[cb]) {
        /* printf("ERROR: offset into hcb_2_pair_table = %d >%d!\n", offset,
           hcb_2_pair_table_size[cb]); */
        return 10;
    }
    sp[0] = hcb_2_pair_table[cb][offset].x;
    sp[1] = hcb_2_pair_table[cb][offset].y;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_2step_pair_sign(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint8_t err = huffman_2step_pair(cb, ld, sp);
    huffman_sign_bits(ld, sp, PAIR_LEN);
    return err;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_binary_quad(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint16_t offset = 0;
    while(!hcb3[offset].is_leaf) {
        uint8_t b = faad_get1bit(ld);
        offset += hcb3[offset].data[b];
    }
    if(offset > hcb_bin_table_size[cb]) {
        /* printf("ERROR: offset into hcb_bin_table = %d >%d!\n", offset,
           hcb_bin_table_size[cb]); */
        return 10;
    }
    sp[0] = hcb3[offset].data[0];
    sp[1] = hcb3[offset].data[1];
    sp[2] = hcb3[offset].data[2];
    sp[3] = hcb3[offset].data[3];
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_binary_quad_sign(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint8_t err = huffman_binary_quad(cb, ld, sp);
    huffman_sign_bits(ld, sp, QUAD_LEN);
    return err;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_binary_pair(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint16_t offset = 0;
    while(!hcb_bin_table[cb][offset].is_leaf) {
        uint8_t b = faad_get1bit(ld);
        offset += hcb_bin_table[cb][offset].data[b];
    }
    if(offset > hcb_bin_table_size[cb]) {
        /* printf("ERROR: offset into hcb_bin_table = %d >%d!\n", offset,
           hcb_bin_table_size[cb]); */
        return 10;
    }
    sp[0] = hcb_bin_table[cb][offset].data[0];
    sp[1] = hcb_bin_table[cb][offset].data[1];
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_binary_pair_sign(uint8_t cb, bitfile* ld, int16_t* sp) {
    uint8_t err = huffman_binary_pair(cb, ld, sp);
    huffman_sign_bits(ld, sp, PAIR_LEN);
    return err;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int16_t huffman_codebook(uint8_t i) {
    static const uint32_t data = 16428320;
    if(i == 0) return (int16_t)(data >> 16) & 0xFFFF;
    else return (int16_t)data & 0xFFFF;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void vcb11_check_LAV(uint8_t cb, int16_t* sp) {
    static const uint16_t vcb11_LAV_tab[] = {16, 31, 47, 63, 95, 127, 159, 191, 223, 255, 319, 383, 511, 767, 1023, 2047};
    uint16_t              max = 0;
    if(cb < 16 || cb > 31) return;
    max = vcb11_LAV_tab[cb - 16];
    if((abs(sp[0]) > max) || (abs(sp[1]) > max)) {
        sp[0] = 0;
        sp[1] = 0;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t huffman_spectral_data(uint8_t cb, bitfile* ld, int16_t* sp) {
    switch(cb) {
        case 1: /* 2-step method for data quadruples */
        case 2: return huffman_2step_quad(cb, ld, sp);
        case 3: /* binary search for data quadruples */ return huffman_binary_quad_sign(cb, ld, sp);
        case 4: /* 2-step method for data quadruples */ return huffman_2step_quad_sign(cb, ld, sp);
        case 5: /* binary search for data pairs */ return huffman_binary_pair(cb, ld, sp);
        case 6: /* 2-step method for data pairs */ return huffman_2step_pair(cb, ld, sp);
        case 7: /* binary search for data pairs */
        case 9: return huffman_binary_pair_sign(cb, ld, sp);
        case 8: /* 2-step method for data pairs */
        case 10: return huffman_2step_pair_sign(cb, ld, sp);
        case 12: {
            uint8_t err = huffman_2step_pair(11, ld, sp);
            sp[0] = huffman_codebook(0);
            sp[1] = huffman_codebook(1);
            return err;
        }
        case 11: {
            uint8_t err = huffman_2step_pair_sign(11, ld, sp);
            if(!err) err = huffman_getescape(ld, &sp[0]);
            if(!err) err = huffman_getescape(ld, &sp[1]);
            return err;
        }
#ifdef ERROR_RESILIENCE
        /* VCB11 uses codebook 11 */
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31: {
            uint8_t err = huffman_2step_pair_sign(11, ld, sp);
            if(!err) err = huffman_getescape(ld, &sp[0]);
            if(!err) err = huffman_getescape(ld, &sp[1]);
            /* check LAV (Largest Absolute Value) */
            /* this finds errors in the ESCAPE signal */
            vcb11_check_LAV(cb, sp);
            return err;
        }
#endif
        default:
            /* Non existent codebook number, something went wrong */
            return 11;
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
/* Special version of huffman_spectral_data
Will not read from a bitfile but a bits_t structure.
Will keep track of the bits decoded and return the number of bits remaining.
Do not read more than ld->len, return -1 if codeword would be longer */
int8_t huffman_spectral_data_2(uint8_t cb, bits_t* ld, int16_t* sp) {
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t  extra_bits;
    uint8_t  i, vcb11 = 0;
    switch(cb) {
        case 1: /* 2-step method for data quadruples */
        case 2:
        case 4:
            cw = showbits_hcr(ld, hcbN[cb]);
            offset = hcb_table[cb][cw].offset;
            extra_bits = hcb_table[cb][cw].extra_bits;
            if(extra_bits) {
                /* we know for sure it's more than hcbN[cb] bits long */
                if(flushbits_hcr(ld, hcbN[cb])) return -1;
                offset += (uint16_t)showbits_hcr(ld, extra_bits);
                if(flushbits_hcr(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb])) return -1;
            }
            else {
                if(flushbits_hcr(ld, hcb_2_quad_table[cb][offset].bits)) return -1;
            }
            sp[0] = hcb_2_quad_table[cb][offset].x;
            sp[1] = hcb_2_quad_table[cb][offset].y;
            sp[2] = hcb_2_quad_table[cb][offset].v;
            sp[3] = hcb_2_quad_table[cb][offset].w;
            break;
        case 6: /* 2-step method for data pairs */
        case 8:
        case 10:
        case 11:
        /* VCB11 uses codebook 11 */
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
            if(cb >= 16) {
                /* store the virtual codebook */
                vcb11 = cb;
                cb = 11;
            }
            cw = showbits_hcr(ld, hcbN[cb]);
            offset = hcb_table[cb][cw].offset;
            extra_bits = hcb_table[cb][cw].extra_bits;
            if(extra_bits) {
                /* we know for sure it's more than hcbN[cb] bits long */
                if(flushbits_hcr(ld, hcbN[cb])) return -1;
                offset += (uint16_t)showbits_hcr(ld, extra_bits);
                if(flushbits_hcr(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb])) return -1;
            }
            else {
                if(flushbits_hcr(ld, hcb_2_pair_table[cb][offset].bits)) return -1;
            }
            sp[0] = hcb_2_pair_table[cb][offset].x;
            sp[1] = hcb_2_pair_table[cb][offset].y;
            break;
        case 3: /* binary search for data quadruples */
            while(!hcb3[offset].is_leaf) {
                uint8_t b;
                if(get1bit_hcr(ld, &b)) return -1;
                offset += hcb3[offset].data[b];
            }
            sp[0] = hcb3[offset].data[0];
            sp[1] = hcb3[offset].data[1];
            sp[2] = hcb3[offset].data[2];
            sp[3] = hcb3[offset].data[3];
            break;
        case 5: /* binary search for data pairs */
        case 7:
        case 9:
            while(!hcb_bin_table[cb][offset].is_leaf) {
                uint8_t b;
                if(get1bit_hcr(ld, &b)) return -1;
                offset += hcb_bin_table[cb][offset].data[b];
            }
            sp[0] = hcb_bin_table[cb][offset].data[0];
            sp[1] = hcb_bin_table[cb][offset].data[1];
            break;
    }
    /* decode sign bits */
    if(unsigned_cb[cb]) {
        for(i = 0; i < ((cb < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN); i++) {
            if(sp[i]) {
                uint8_t b;
                if(get1bit_hcr(ld, &b)) return -1;
                if(b != 0) { sp[i] = -sp[i]; }
            }
        }
    }
    /* decode huffman escape bits */
    if((cb == ESC_HCB) || (cb >= 16)) {
        uint8_t k;
        for(k = 0; k < 2; k++) {
            if((sp[k] == 16) || (sp[k] == -16)) {
                uint8_t  neg, i;
                int32_t  j;
                uint32_t off;
                neg = (sp[k] < 0) ? 1 : 0;
                for(i = 4;; i++) {
                    uint8_t b;
                    if(get1bit_hcr(ld, &b)) return -1;
                    if(b == 0) break;
                }
                if(getbits_hcr(ld, i, &off)) return -1;
                j = off + (1 << i);
                sp[k] = (int16_t)((neg) ? -j : j);
            }
        }
        if(vcb11 != 0) {
            /* check LAV (Largest Absolute Value) */
            /* this finds errors in the ESCAPE signal */
            vcb11_check_LAV(vcb11, sp);
        }
    }
    return ld->len;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void is_decode(ic_stream* ics, ic_stream* icsr, real_t* l_spec, real_t* r_spec, uint16_t frame_len) {
    uint8_t  g, sfb, b;
    uint16_t i;
#ifndef FIXED_POINT
    real_t scale;
#else
    int32_t exp, frac;
#endif
    uint16_t nshort = frame_len / 8;
    uint8_t  group = 0;
    for(g = 0; g < icsr->num_window_groups; g++) {
        /* Do intensity stereo decoding */
        for(b = 0; b < icsr->window_group_length[g]; b++) {
            for(sfb = 0; sfb < icsr->max_sfb; sfb++) {
                if(is_intensity(icsr, g, sfb)) {
#ifdef MAIN_DEC
                    /* For scalefactor bands coded in intensity stereo the
                       corresponding predictors in the right channel are
                       switched to "off".
                     */
                    ics->pred.prediction_used[sfb] = 0;
                    icsr->pred.prediction_used[sfb] = 0;
#endif
#ifndef FIXED_POINT
                    scale = (real_t)pow(0.5, (0.25 * icsr->scale_factors[g][sfb]));
#else
                    exp = icsr->scale_factors[g][sfb] >> 2;
                    frac = icsr->scale_factors[g][sfb] & 3;
#endif
                    /* Scale from left to right channel,
                       do not touch left channel */
                    for(i = icsr->swb_offset[sfb]; i < min(icsr->swb_offset[sfb + 1], ics->swb_offset_max); i++) {
#ifndef FIXED_POINT
                        r_spec[(group * nshort) + i] = MUL_R(l_spec[(group * nshort) + i], scale);
#else
                        if(exp < 0) r_spec[(group * nshort) + i] = l_spec[(group * nshort) + i] << -exp;
                        else r_spec[(group * nshort) + i] = l_spec[(group * nshort) + i] >> exp;
                        r_spec[(group * nshort) + i] = MUL_C(r_spec[(group * nshort) + i], pow05_table[frac + 3]);
#endif
                        if(is_intensity(icsr, g, sfb) != invert_intensity(ics, g, sfb)) r_spec[(group * nshort) + i] = -r_spec[(group * nshort) + i];
                    }
                }
            }
            group++;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
mdct_info* faad_mdct_init(uint16_t N) {
    mdct_info* mdct = (mdct_info*)faad_malloc(sizeof(mdct_info));
    assert(N % 8 == 0);
    mdct->N = N;
    /* NOTE: For "small framelengths" in FIXED_POINT the coefficients need to be
     * scaled by sqrt("(nearest power of 2) > N" / N) */
    /* RE(mdct->sincos[k]) = scale*(real_t)(cos(2.0*M_PI*(k+1./8.) / (real_t)N));
     * IM(mdct->sincos[k]) = scale*(real_t)(sin(2.0*M_PI*(k+1./8.) / (real_t)N)); */
    /* scale is 1 for fixed point, sqrt(N) for floating point */
    switch(N) {
        case 2048: mdct->sincos = (complex_t*)mdct_tab_2048; break;
        case 256: mdct->sincos = (complex_t*)mdct_tab_256; break;
#ifdef LD_DEC
        case 1024: mdct->sincos = (complex_t*)mdct_tab_1024; break;
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
        case 1920: mdct->sincos = (complex_t*)mdct_tab_1920; break;
        case 240: mdct->sincos = (complex_t*)mdct_tab_240; break;
    #ifdef LD_DEC
        case 960: mdct->sincos = (complex_t*)mdct_tab_960; break;
    #endif
#endif
#ifdef SSR_DEC
        case 512: mdct->sincos = (complex_t*)mdct_tab_512; break;
        case 64: mdct->sincos = (complex_t*)mdct_tab_64; break;
#endif
    }
    /* initialise fft */
    mdct->cfft = cffti(N / 4);
#ifdef PROFILE
    mdct->cycles = 0;
    mdct->fft_cycles = 0;
#endif
    return mdct;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void faad_mdct_end(mdct_info* mdct) {
    if(mdct != NULL) {
#ifdef PROFILE
        printf("MDCT[%.4d]:         %I64d cycles\n", mdct->N, mdct->cycles);
        printf("CFFT[%.4d]:         %I64d cycles\n", mdct->N / 4, mdct->fft_cycles);
#endif
        cfftu(mdct->cfft);
        faad_free(mdct);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void faad_imdct(mdct_info* mdct, real_t* X_in, real_t* X_out) {
    uint16_t k;
    complex_t x;
#ifdef ALLOW_SMALL_FRAMELENGTH
    #ifdef FIXED_POINT
    real_t scale, b_scale = 0;
    #endif
#endif
    // ALIGN complex_t Z1[512];
    complex_t* Z1 = ps_malloc(512 * sizeof(complex_t));
    complex_t*      sincos = mdct->sincos;
    uint16_t N = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;
#ifdef PROFILE
    int64_t count1, count2 = faad_get_ts();
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    #ifdef FIXED_POINT
    /* detect non-power of 2 */
    if(N & (N - 1)) {
        /* adjust scale for non-power of 2 MDCT */
        /* 2048/1920 */
        b_scale = 1;
        scale = COEF_CONST(1.0666666666666667);
    }
    #endif
#endif
    /* pre-IFFT complex multiplication */
    for(k = 0; k < N4; k++) { ComplexMult(&IM(Z1[k]), &RE(Z1[k]), X_in[2 * k], X_in[N2 - 1 - 2 * k], RE(sincos[k]), IM(sincos[k])); }
#ifdef PROFILE
    count1 = faad_get_ts();
#endif
    /* complex IFFT, any non-scaling FFT can be used here */
    cfftb(mdct->cfft, Z1);
#ifdef PROFILE
    count1 = faad_get_ts() - count1;
#endif
    /* post-IFFT complex multiplication */
    for(k = 0; k < N4; k++) {
        RE(x) = RE(Z1[k]);
        IM(x) = IM(Z1[k]);
        ComplexMult(&IM(Z1[k]), &RE(Z1[k]), IM(x), RE(x), RE(sincos[k]), IM(sincos[k]));
#ifdef ALLOW_SMALL_FRAMELENGTH
    #ifdef FIXED_POINT
        /* non-power of 2 MDCT scaling */
        if(b_scale) {
            RE(Z1[k]) = MUL_C(RE(Z1[k]), scale);
            IM(Z1[k]) = MUL_C(IM(Z1[k]), scale);
        }
    #endif
#endif
    }
    /* reordering */
    for(k = 0; k < N8; k += 2) {
        X_out[2 * k] = IM(Z1[N8 + k]);
        X_out[2 + 2 * k] = IM(Z1[N8 + 1 + k]);
        X_out[1 + 2 * k] = -RE(Z1[N8 - 1 - k]);
        X_out[3 + 2 * k] = -RE(Z1[N8 - 2 - k]);
        X_out[N4 + 2 * k] = RE(Z1[k]);
        X_out[N4 + +2 + 2 * k] = RE(Z1[1 + k]);
        X_out[N4 + 1 + 2 * k] = -IM(Z1[N4 - 1 - k]);
        X_out[N4 + 3 + 2 * k] = -IM(Z1[N4 - 2 - k]);
        X_out[N2 + 2 * k] = RE(Z1[N8 + k]);
        X_out[N2 + +2 + 2 * k] = RE(Z1[N8 + 1 + k]);
        X_out[N2 + 1 + 2 * k] = -IM(Z1[N8 - 1 - k]);
        X_out[N2 + 3 + 2 * k] = -IM(Z1[N8 - 2 - k]);
        X_out[N2 + N4 + 2 * k] = -IM(Z1[k]);
        X_out[N2 + N4 + 2 + 2 * k] = -IM(Z1[1 + k]);
        X_out[N2 + N4 + 1 + 2 * k] = RE(Z1[N4 - 1 - k]);
        X_out[N2 + N4 + 3 + 2 * k] = RE(Z1[N4 - 2 - k]);
    }
#ifdef PROFILE
    count2 = faad_get_ts() - count2;
    mdct->fft_cycles += count1;
    mdct->cycles += (count2 - count1);
#endif
    if(Z1) free(Z1);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef LTP_DEC
void faad_mdct(mdct_info* mdct, real_t* X_in, real_t* X_out) {
    uint16_t k;
    complex_t       x;
    // ALIGN complex_t Z1[512];
    complex_t* Z1 = ps_malloc(512 * sizeof(complex_t));
    complex_t*      sincos = mdct->sincos;
    uint16_t N = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;
    #ifndef FIXED_POINT
    real_t scale = REAL_CONST(N);
    #else
    real_t scale = REAL_CONST(4.0 / N);
    #endif
    #ifdef ALLOW_SMALL_FRAMELENGTH
        #ifdef FIXED_POINT
    /* detect non-power of 2 */
    if(N & (N - 1)) {
        /* adjust scale for non-power of 2 MDCT */
        /* *= sqrt(2048/1920) */
        scale = MUL_C(scale, COEF_CONST(1.0327955589886444));
    }
        #endif
    #endif
    /* pre-FFT complex multiplication */
    for(k = 0; k < N8; k++) {
        uint16_t n = k << 1;
        RE(x) = X_in[N - N4 - 1 - n] + X_in[N - N4 + n];
        IM(x) = X_in[N4 + n] - X_in[N4 - 1 - n];
        ComplexMult(&RE(Z1[k]), &IM(Z1[k]), RE(x), IM(x), RE(sincos[k]), IM(sincos[k]));
        RE(Z1[k]) = MUL_R(RE(Z1[k]), scale);
        IM(Z1[k]) = MUL_R(IM(Z1[k]), scale);
        RE(x) = X_in[N2 - 1 - n] - X_in[n];
        IM(x) = X_in[N2 + n] + X_in[N - 1 - n];
        ComplexMult(&RE(Z1[k + N8]), &IM(Z1[k + N8]), RE(x), IM(x), RE(sincos[k + N8]), IM(sincos[k + N8]));
        RE(Z1[k + N8]) = MUL_R(RE(Z1[k + N8]), scale);
        IM(Z1[k + N8]) = MUL_R(IM(Z1[k + N8]), scale);
    }
    /* complex FFT, any non-scaling FFT can be used here  */
    cfftf(mdct->cfft, Z1);
    /* post-FFT complex multiplication */
    for(k = 0; k < N4; k++) {
        uint16_t n = k << 1;
        ComplexMult(&RE(x), &IM(x), RE(Z1[k]), IM(Z1[k]), RE(sincos[k]), IM(sincos[k]));
        X_out[n] = -RE(x);
        X_out[N2 - 1 - n] = IM(x);
        X_out[N2 + n] = -IM(x);
        X_out[N - 1 - n] = RE(x);
    }
    if(Z1)free(Z1);
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
