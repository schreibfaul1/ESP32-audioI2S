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
fb_info* filter_bank_init(uint16_t frame_len) {
    uint16_t nshort = frame_len / 8;
#ifdef LD_DEC
    uint16_t frame_len_ld = frame_len / 2;
#endif
    fb_info* fb = (fb_info*)faad_malloc(sizeof(fb_info));
    memset(fb, 0, sizeof(fb_info));
    /* normal */
    fb->mdct256 = faad_mdct_init(2 * nshort);
    fb->mdct2048 = faad_mdct_init(2 * frame_len);
#ifdef LD_DEC
    /* LD */
    fb->mdct1024 = faad_mdct_init(2 * frame_len_ld);
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    if(frame_len == 1024) {
#endif
        fb->long_window[0] = sine_long_1024;
        fb->short_window[0] = sine_short_128;
        fb->long_window[1] = kbd_long_1024;
        fb->short_window[1] = kbd_short_128;
#ifdef LD_DEC
        fb->ld_window[0] = sine_mid_512;
        fb->ld_window[1] = ld_mid_512;
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    }
    else /* (frame_len == 960) */ {
        fb->long_window[0] = sine_long_960;
        fb->short_window[0] = sine_short_120;
        fb->long_window[1] = kbd_long_960;
        fb->short_window[1] = kbd_short_120;
    #ifdef LD_DEC
        fb->ld_window[0] = sine_mid_480;
        fb->ld_window[1] = ld_mid_480;
    #endif
    }
#endif
    return fb;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void filter_bank_end(fb_info* fb) {
    if(fb != NULL) {
#ifdef PROFILE
        printf("FB:                 %I64d cycles\n", fb->cycles);
#endif
        faad_mdct_end(fb->mdct256);
        faad_mdct_end(fb->mdct2048);
#ifdef LD_DEC
        faad_mdct_end(fb->mdct1024);
#endif
        faad_free(fb);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void imdct_long(fb_info* fb, real_t* in_data, real_t* out_data, uint16_t len) {
#ifdef LD_DEC
    mdct_info* mdct = NULL;
    switch(len) {
        case 2048:
        case 1920: mdct = fb->mdct2048; break;
        case 1024:
        case 960: mdct = fb->mdct1024; break;
    }
    faad_imdct(mdct, in_data, out_data);
#else
    faad_imdct(fb->mdct2048, in_data, out_data);
#endif
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef LTP_DEC
void mdct(fb_info* fb, real_t* in_data, real_t* out_data, uint16_t len) {
    mdct_info* mdct = NULL;
    switch(len) {
        case 2048:
        case 1920: mdct = fb->mdct2048; break;
        case 256:
        case 240: mdct = fb->mdct256; break;
    #ifdef LD_DEC
        case 1024:
        case 960: mdct = fb->mdct1024; break;
    #endif
    }
    faad_mdct(mdct, in_data, out_data);
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* freq_in, real_t* time_out, real_t* overlap, uint8_t object_type, uint16_t frame_len) {
    int16_t i;
    //    ALIGN real_t transf_buf[2*1024] = {0};
    real_t* transf_buf = (real_t*)ps_calloc(2 * 1024, sizeof(real_t));
    const real_t* window_long = NULL;
    const real_t* window_long_prev = NULL;
    const real_t* window_short = NULL;
    const real_t* window_short_prev = NULL;
    uint16_t nlong = frame_len;
    uint16_t nshort = frame_len / 8;
    uint16_t trans = nshort / 2;
    uint16_t nflat_ls = (nlong - nshort) / 2;
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
    /* select windows of current frame and previous frame (Sine or KBD) */
#ifdef LD_DEC
    if(object_type == LD) {
        window_long = fb->ld_window[window_shape];
        window_long_prev = fb->ld_window[window_shape_prev];
    }
    else {
#endif
        window_long = fb->long_window[window_shape];
        window_long_prev = fb->long_window[window_shape_prev];
        window_short = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
#ifdef LD_DEC
    }
#endif
#if 0
    for (i = 0; i < 1024; i++)
    {
        printf("%d\n", freq_in[i]);
    }
#endif
#if 0
    printf("%d %d\n", window_sequence, window_shape);
#endif
    switch(window_sequence) {
        case ONLY_LONG_SEQUENCE:
            /* perform iMDCT */
            imdct_long(fb, freq_in, transf_buf, 2 * nlong);
            /* add second half output of previous frame to windowed output of current frame */
            for(i = 0; i < nlong; i += 4) {
                time_out[i] = overlap[i] + MUL_F(transf_buf[i], window_long_prev[i]);
                time_out[i + 1] = overlap[i + 1] + MUL_F(transf_buf[i + 1], window_long_prev[i + 1]);
                time_out[i + 2] = overlap[i + 2] + MUL_F(transf_buf[i + 2], window_long_prev[i + 2]);
                time_out[i + 3] = overlap[i + 3] + MUL_F(transf_buf[i + 3], window_long_prev[i + 3]);
            }
            /* window the second half and save as overlap for next frame */
            for(i = 0; i < nlong; i += 4) {
                overlap[i] = MUL_F(transf_buf[nlong + i], window_long[nlong - 1 - i]);
                overlap[i + 1] = MUL_F(transf_buf[nlong + i + 1], window_long[nlong - 2 - i]);
                overlap[i + 2] = MUL_F(transf_buf[nlong + i + 2], window_long[nlong - 3 - i]);
                overlap[i + 3] = MUL_F(transf_buf[nlong + i + 3], window_long[nlong - 4 - i]);
            }
            break;
        case LONG_START_SEQUENCE:
            /* perform iMDCT */
            imdct_long(fb, freq_in, transf_buf, 2 * nlong);
            /* add second half output of previous frame to windowed output of current frame */
            for(i = 0; i < nlong; i += 4) {
                time_out[i] = overlap[i] + MUL_F(transf_buf[i], window_long_prev[i]);
                time_out[i + 1] = overlap[i + 1] + MUL_F(transf_buf[i + 1], window_long_prev[i + 1]);
                time_out[i + 2] = overlap[i + 2] + MUL_F(transf_buf[i + 2], window_long_prev[i + 2]);
                time_out[i + 3] = overlap[i + 3] + MUL_F(transf_buf[i + 3], window_long_prev[i + 3]);
            }
            /* window the second half and save as overlap for next frame */
            /* construct second half window using padding with 1's and 0's */
            for(i = 0; i < nflat_ls; i++) overlap[i] = transf_buf[nlong + i];
            for(i = 0; i < nshort; i++) overlap[nflat_ls + i] = MUL_F(transf_buf[nlong + nflat_ls + i], window_short[nshort - i - 1]);
            for(i = 0; i < nflat_ls; i++) overlap[nflat_ls + nshort + i] = 0;
            break;
        case EIGHT_SHORT_SEQUENCE:
            /* perform iMDCT for each short block */
            faad_imdct(fb->mdct256, freq_in + 0 * nshort, transf_buf + 2 * nshort * 0);
            faad_imdct(fb->mdct256, freq_in + 1 * nshort, transf_buf + 2 * nshort * 1);
            faad_imdct(fb->mdct256, freq_in + 2 * nshort, transf_buf + 2 * nshort * 2);
            faad_imdct(fb->mdct256, freq_in + 3 * nshort, transf_buf + 2 * nshort * 3);
            faad_imdct(fb->mdct256, freq_in + 4 * nshort, transf_buf + 2 * nshort * 4);
            faad_imdct(fb->mdct256, freq_in + 5 * nshort, transf_buf + 2 * nshort * 5);
            faad_imdct(fb->mdct256, freq_in + 6 * nshort, transf_buf + 2 * nshort * 6);
            faad_imdct(fb->mdct256, freq_in + 7 * nshort, transf_buf + 2 * nshort * 7);
            /* add second half output of previous frame to windowed output of current frame */
            for(i = 0; i < nflat_ls; i++) time_out[i] = overlap[i];
            for(i = 0; i < nshort; i++) {
                time_out[nflat_ls + i] = overlap[nflat_ls + i] + MUL_F(transf_buf[nshort * 0 + i], window_short_prev[i]);
                time_out[nflat_ls + 1 * nshort + i] =
                    overlap[nflat_ls + nshort * 1 + i] + MUL_F(transf_buf[nshort * 1 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 2 + i], window_short[i]);
                time_out[nflat_ls + 2 * nshort + i] =
                    overlap[nflat_ls + nshort * 2 + i] + MUL_F(transf_buf[nshort * 3 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 4 + i], window_short[i]);
                time_out[nflat_ls + 3 * nshort + i] =
                    overlap[nflat_ls + nshort * 3 + i] + MUL_F(transf_buf[nshort * 5 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 6 + i], window_short[i]);
                if(i < trans)
                    time_out[nflat_ls + 4 * nshort + i] =
                        overlap[nflat_ls + nshort * 4 + i] + MUL_F(transf_buf[nshort * 7 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 8 + i], window_short[i]);
            }
            /* window the second half and save as overlap for next frame */
            for(i = 0; i < nshort; i++) {
                if(i >= trans) overlap[nflat_ls + 4 * nshort + i - nlong] = MUL_F(transf_buf[nshort * 7 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 8 + i], window_short[i]);
                overlap[nflat_ls + 5 * nshort + i - nlong] = MUL_F(transf_buf[nshort * 9 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 10 + i], window_short[i]);
                overlap[nflat_ls + 6 * nshort + i - nlong] = MUL_F(transf_buf[nshort * 11 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 12 + i], window_short[i]);
                overlap[nflat_ls + 7 * nshort + i - nlong] = MUL_F(transf_buf[nshort * 13 + i], window_short[nshort - 1 - i]) + MUL_F(transf_buf[nshort * 14 + i], window_short[i]);
                overlap[nflat_ls + 8 * nshort + i - nlong] = MUL_F(transf_buf[nshort * 15 + i], window_short[nshort - 1 - i]);
            }
            for(i = 0; i < nflat_ls; i++) overlap[nflat_ls + nshort + i] = 0;
            break;
        case LONG_STOP_SEQUENCE:
            /* perform iMDCT */
            imdct_long(fb, freq_in, transf_buf, 2 * nlong);
            /* add second half output of previous frame to windowed output of current frame */
            /* construct first half window using padding with 1's and 0's */
            for(i = 0; i < nflat_ls; i++) time_out[i] = overlap[i];
            for(i = 0; i < nshort; i++) time_out[nflat_ls + i] = overlap[nflat_ls + i] + MUL_F(transf_buf[nflat_ls + i], window_short_prev[i]);
            for(i = 0; i < nflat_ls; i++) time_out[nflat_ls + nshort + i] = overlap[nflat_ls + nshort + i] + transf_buf[nflat_ls + nshort + i];
            /* window the second half and save as overlap for next frame */
            for(i = 0; i < nlong; i++) overlap[i] = MUL_F(transf_buf[nlong + i], window_long[nlong - 1 - i]);
            break;
    }
#if 0
    for (i = 0; i < 1024; i++)
    {
        printf("%d\n", time_out[i]);
        //printf("0x%.8X\n", time_out[i]);
    }
#endif
#ifdef PROFILE
    count = faad_get_ts() - count;
    fb->cycles += count;
#endif
    if(transf_buf) free(transf_buf);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef LTP_DEC
/* only works for LTP -> no overlapping, no short blocks */
void filter_bank_ltp(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* in_data, real_t* out_mdct, uint8_t object_type, uint16_t frame_len) {
    int16_t i;
    // ALIGN real_t windowed_buf[2*1024] = {0};
    real_t* windowed_buf = ps_calloc(2 * 1024, sizeof(real_t));
    const real_t* window_long = NULL;
    const real_t* window_long_prev = NULL;
    const real_t* window_short = NULL;
    const real_t* window_short_prev = NULL;
    uint16_t nlong = frame_len;
    uint16_t nshort = frame_len / 8;
    uint16_t nflat_ls = (nlong - nshort) / 2;
    assert(window_sequence != EIGHT_SHORT_SEQUENCE);
    #ifdef LD_DEC
    if(object_type == LD) {
        window_long = fb->ld_window[window_shape];
        window_long_prev = fb->ld_window[window_shape_prev];
    }
    else {
    #endif
        window_long = fb->long_window[window_shape];
        window_long_prev = fb->long_window[window_shape_prev];
        window_short = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
    #ifdef LD_DEC
    }
    #endif
    switch(window_sequence) {
        case ONLY_LONG_SEQUENCE:
            for(i = nlong - 1; i >= 0; i--) {
                windowed_buf[i] = MUL_F(in_data[i], window_long_prev[i]);
                windowed_buf[i + nlong] = MUL_F(in_data[i + nlong], window_long[nlong - 1 - i]);
            }
            mdct(fb, windowed_buf, out_mdct, 2 * nlong);
            break;
        case LONG_START_SEQUENCE:
            for(i = 0; i < nlong; i++) windowed_buf[i] = MUL_F(in_data[i], window_long_prev[i]);
            for(i = 0; i < nflat_ls; i++) windowed_buf[i + nlong] = in_data[i + nlong];
            for(i = 0; i < nshort; i++) windowed_buf[i + nlong + nflat_ls] = MUL_F(in_data[i + nlong + nflat_ls], window_short[nshort - 1 - i]);
            for(i = 0; i < nflat_ls; i++) windowed_buf[i + nlong + nflat_ls + nshort] = 0;
            mdct(fb, windowed_buf, out_mdct, 2 * nlong);
            break;
        case LONG_STOP_SEQUENCE:
            for(i = 0; i < nflat_ls; i++) windowed_buf[i] = 0;
            for(i = 0; i < nshort; i++) windowed_buf[i + nflat_ls] = MUL_F(in_data[i + nflat_ls], window_short_prev[i]);
            for(i = 0; i < nflat_ls; i++) windowed_buf[i + nflat_ls + nshort] = in_data[i + nflat_ls + nshort];
            for(i = 0; i < nlong; i++) windowed_buf[i + nlong] = MUL_F(in_data[i + nlong], window_long[nlong - 1 - i]);
            mdct(fb, windowed_buf, out_mdct, 2 * nlong);
            break;
    }
    if(windowed_buf) free(windowed_buf);
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const uint8_t PreSortCB_STD[NUM_CB] = {11, 9, 7, 5, 3, 1};
const uint8_t PreSortCB_ER[NUM_CB_ER] = {11, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 9, 7, 5, 3, 1};
/* 8.5.3.3.2 Derivation of segment width */
const uint8_t maxCwLen[MAX_CB] = {0, 11, 9, 20, 16, 13, 11, 14, 12, 17, 14, 49, 0, 0, 0, 0, 14, 17, 21, 21, 25, 25, 29, 29, 29, 29, 33, 33, 33, 37, 37, 41};
#define segmentWidth(cb) min(maxCwLen[cb], ics->length_of_longest_codeword)
/* bit-twiddling helpers */
const uint8_t  S[] = {1, 2, 4, 8, 16};
const uint32_t B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* rewind and reverse */
/* 32 bit version */
uint32_t rewrev_word(uint32_t v, const uint8_t len) {
    /* 32 bit reverse */
    v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]);
    v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]);
    v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]);
    v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
    v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);
    /* shift off low bits */
    v >>= (32 - len);
    return v;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* 64 bit version */
void rewrev_lword(uint32_t* hi, uint32_t* lo, const uint8_t len) {
    if(len <= 32) {
        *hi = 0;
        *lo = rewrev_word(*lo, len);
    }
    else {
        uint32_t t = *hi, v = *lo;
        /* double 32 bit reverse */
        v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]);
        t = ((t >> S[0]) & B[0]) | ((t << S[0]) & ~B[0]);
        v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]);
        t = ((t >> S[1]) & B[1]) | ((t << S[1]) & ~B[1]);
        v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]);
        t = ((t >> S[2]) & B[2]) | ((t << S[2]) & ~B[2]);
        v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
        t = ((t >> S[3]) & B[3]) | ((t << S[3]) & ~B[3]);
        v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);
        t = ((t >> S[4]) & B[4]) | ((t << S[4]) & ~B[4]);
        /* last 32<>32 bit swap is implicit below */
        /* shift off low bits (this is really only one 64 bit shift) */
        *lo = (t >> (64 - len)) | (v << (len - 32));
        *hi = v >> (64 - len);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* bits_t version */
void rewrev_bits(bits_t* bits) {
    if(bits->len == 0) return;
    rewrev_lword(&bits->bufb, &bits->bufa, bits->len);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* merge bits of a to b */
void concat_bits(bits_t* b, bits_t* a) {
    uint32_t bl, bh, al, ah;
    if(a->len == 0) return;
    al = a->bufa;
    ah = a->bufb;
    if(b->len > 32) {
        /* maskoff superfluous high b bits */
        bl = b->bufa;
        bh = b->bufb & ((1 << (b->len - 32)) - 1);
        /* left shift a b->len bits */
        ah = al << (b->len - 32);
        al = 0;
    }
    else {
        bl = b->bufa & ((1 << (b->len)) - 1);
        bh = 0;
        ah = (ah << (b->len)) | (al >> (32 - b->len));
        al = al << b->len;
    }
    /* merge */
    b->bufa = bl | al;
    b->bufb = bh | ah;
    b->len += a->len;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t is_good_cb(uint8_t this_CB, uint8_t this_sec_CB) {
    /* only want spectral data CB's */
    if((this_sec_CB > ZERO_HCB && this_sec_CB <= ESC_HCB) || (this_sec_CB >= VCB11_FIRST && this_sec_CB <= VCB11_LAST)) {
        if(this_CB < ESC_HCB) {
            /* normal codebook pairs */
            return ((this_sec_CB == this_CB) || (this_sec_CB == this_CB + 1));
        }
        else {
            /* escape codebook */
            return (this_sec_CB == this_CB);
        }
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void read_segment(bits_t* segment, uint8_t segwidth, bitfile* ld) {
    segment->len = segwidth;
    if(segwidth > 32) {
        segment->bufb = faad_getbits(ld, segwidth - 32);
        segment->bufa = faad_getbits(ld, 32);
    }
    else {
        segment->bufa = faad_getbits(ld, segwidth);
        segment->bufb = 0;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void fill_in_codeword(codeword_t* codeword, uint16_t index, uint16_t sp, uint8_t cb) {
    codeword[index].sp_offset = sp;
    codeword[index].cb = cb;
    codeword[index].decoded = 0;
    codeword[index].bits.len = 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t reordered_spectral_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, int16_t* spectral_data) {
    uint8_t ret = 0;
    uint16_t PCWs_done;
    uint16_t numberOfSegments, numberOfSets, numberOfCodewords;
    // codeword_t codeword[512];
    // bits_t     segment[512];
    codeword_t* codeword = (codeword_t*)ps_malloc(sizeof(codeword_t) * 512);
    bits_t* segment = (bits_t*)ps_malloc(sizeof(bits_t) * 512);
    uint16_t sp_offset[8];
    uint16_t g, i, sortloop, set, bitsread;
    /*uint16_t bitsleft, codewordsleft*/;
    uint8_t w_idx, sfb, this_CB, last_CB, this_sec_CB;
    const uint16_t nshort = hDecoder->frameLength / 8;
    const uint16_t sp_data_len = ics->length_of_reordered_spectral_data;
    const uint8_t* PreSortCb;
    /* no data (e.g. silence) */
    if(sp_data_len == 0) {ret = 0; goto exit;}
    /* since there is spectral data, at least one codeword has nonzero length */
    if(ics->length_of_longest_codeword == 0) {ret = 10; goto exit;}
    if(sp_data_len < ics->length_of_longest_codeword) {ret = 10; goto exit;}
    sp_offset[0] = 0;
    for(g = 1; g < ics->num_window_groups; g++) { sp_offset[g] = sp_offset[g - 1] + nshort * ics->window_group_length[g - 1]; }
    PCWs_done = 0;
    numberOfSegments = 0;
    numberOfCodewords = 0;
    bitsread = 0;
    /* VCB11 code books in use */
    if(hDecoder->aacSectionDataResilienceFlag) {
        PreSortCb = PreSortCB_ER;
        last_CB = NUM_CB_ER;
    }
    else {
        PreSortCb = PreSortCB_STD;
        last_CB = NUM_CB;
    }
    /* step 1: decode PCW's (set 0), and stuff data in easier-to-use format */
    for(sortloop = 0; sortloop < last_CB; sortloop++) {
        /* select codebook to process this pass */
        this_CB = PreSortCb[sortloop];
        /* loop over sfbs */
        for(sfb = 0; sfb < ics->max_sfb; sfb++) {
            /* loop over all in this sfb, 4 lines per loop */
            for(w_idx = 0; 4 * w_idx < (min(ics->swb_offset[sfb + 1], ics->swb_offset_max) - ics->swb_offset[sfb]); w_idx++) {
                for(g = 0; g < ics->num_window_groups; g++) {
                    for(i = 0; i < ics->num_sec[g]; i++) {
                        /* check whether sfb used here is the one we want to process */
                        if((ics->sect_start[g][i] <= sfb) && (ics->sect_end[g][i] > sfb)) {
                            /* check whether codebook used here is the one we want to process */
                            this_sec_CB = ics->sect_cb[g][i];
                            if(is_good_cb(this_CB, this_sec_CB)) {
                                /* precalculate some stuff */
                                uint16_t sect_sfb_size = ics->sect_sfb_offset[g][sfb + 1] - ics->sect_sfb_offset[g][sfb];
                                uint8_t  inc = (this_sec_CB < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN;
                                uint16_t group_cws_count = (4 * ics->window_group_length[g]) / inc;
                                uint8_t  segwidth = segmentWidth(this_sec_CB);
                                uint16_t cws;
                                /* read codewords until end of sfb or end of window group (shouldn't only 1 trigger?) */
                                for(cws = 0; (cws < group_cws_count) && ((cws + w_idx * group_cws_count) < sect_sfb_size); cws++) {
                                    uint16_t sp = sp_offset[g] + ics->sect_sfb_offset[g][sfb] + inc * (cws + w_idx * group_cws_count);
                                    /* read and decode PCW */
                                    if(!PCWs_done) {
                                        /* read in normal segments */
                                        if(bitsread + segwidth <= sp_data_len) {
                                            read_segment(&segment[numberOfSegments], segwidth, ld);
                                            bitsread += segwidth;
                                            huffman_spectral_data_2(this_sec_CB, &segment[numberOfSegments], &spectral_data[sp]);
                                            /* keep leftover bits */
                                            rewrev_bits(&segment[numberOfSegments]);
                                            numberOfSegments++;
                                        }
                                        else {
                                            /* remaining stuff after last segment, we unfortunately couldn't read
                                               this in earlier because it might not fit in 64 bits. since we already
                                               decoded (and removed) the PCW it is now guaranteed to fit */
                                            if(bitsread < sp_data_len) {
                                                const uint8_t additional_bits = sp_data_len - bitsread;
                                                read_segment(&segment[numberOfSegments], additional_bits, ld);
                                                segment[numberOfSegments].len += segment[numberOfSegments - 1].len;
                                                rewrev_bits(&segment[numberOfSegments]);
                                                if(segment[numberOfSegments - 1].len > 32) {
                                                    segment[numberOfSegments - 1].bufb =
                                                        segment[numberOfSegments].bufb + showbits_hcr(&segment[numberOfSegments - 1], segment[numberOfSegments - 1].len - 32);
                                                    segment[numberOfSegments - 1].bufa = segment[numberOfSegments].bufa + showbits_hcr(&segment[numberOfSegments - 1], 32);
                                                }
                                                else {
                                                    segment[numberOfSegments - 1].bufa =
                                                        segment[numberOfSegments].bufa + showbits_hcr(&segment[numberOfSegments - 1], segment[numberOfSegments - 1].len);
                                                    segment[numberOfSegments - 1].bufb = segment[numberOfSegments].bufb;
                                                }
                                                segment[numberOfSegments - 1].len += additional_bits;
                                            }
                                            bitsread = sp_data_len;
                                            PCWs_done = 1;
                                            fill_in_codeword(codeword, 0, sp, this_sec_CB);
                                        }
                                    }
                                    else { fill_in_codeword(codeword, numberOfCodewords - numberOfSegments, sp, this_sec_CB); }
                                    numberOfCodewords++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if(numberOfSegments == 0) {ret = 10; goto exit;}
    numberOfSets = numberOfCodewords / numberOfSegments;
    /* step 2: decode nonPCWs */
    for(set = 1; set <= numberOfSets; set++) {
        uint16_t trial;
        for(trial = 0; trial < numberOfSegments; trial++) {
            uint16_t codewordBase;
            for(codewordBase = 0; codewordBase < numberOfSegments; codewordBase++) {
                const uint16_t segment_idx = (trial + codewordBase) % numberOfSegments;
                const uint16_t codeword_idx = codewordBase + set * numberOfSegments - numberOfSegments;
                /* data up */
                if(codeword_idx >= numberOfCodewords - numberOfSegments) break;
                if(!codeword[codeword_idx].decoded && segment[segment_idx].len > 0) {
                    uint8_t tmplen;
                    if(codeword[codeword_idx].bits.len != 0) concat_bits(&segment[segment_idx], &codeword[codeword_idx].bits);
                    tmplen = segment[segment_idx].len;
                    if(huffman_spectral_data_2(codeword[codeword_idx].cb, &segment[segment_idx], &spectral_data[codeword[codeword_idx].sp_offset]) >= 0) { codeword[codeword_idx].decoded = 1; }
                    else {
                        codeword[codeword_idx].bits = segment[segment_idx];
                        codeword[codeword_idx].bits.len = tmplen;
                    }
                }
            }
        }
        for(i = 0; i < numberOfSegments; i++) rewrev_bits(&segment[i]);
    }
    #if 0 // Seems to give false errors
    bitsleft = 0;
    for (i = 0; i < numberOfSegments && !bitsleft; i++)
        bitsleft += segment[i].len;
    if (bitsleft) {ret = 10; goto exit;}
    codewordsleft = 0;
    for (i = 0; (i < numberOfCodewords - numberOfSegments) && (!codewordsleft); i++)
        if (!codeword[i].decoded)
                codewordsleft++;
    if (codewordsleft) {ret = 10; goto exit;}
    #endif
    ret = 0;
exit:
    if(codeword) free(codeword);
    if(segment) free(segment);
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
void DCT4_32(real_t* y, real_t* x) {
    // printf(ANSI_ESC_YELLOW "dct4_32" ANSI_ESC_WHITE "\n");
    int32_t* f = (int32_t*)faad_malloc(397 * sizeof(int32_t));
    f[0] = x[15] - x[16];
    f[1] = x[15] + x[16];
    f[2] = MUL_F(FRAC_CONST(0.7071067811865476), f[1]);
    f[3] = MUL_F(FRAC_CONST(0.7071067811865476), f[0]);
    f[4] = x[8] - x[23];
    f[5] = x[8] + x[23];
    f[6] = MUL_F(FRAC_CONST(0.7071067811865476), f[5]);
    f[7] = MUL_F(FRAC_CONST(0.7071067811865476), f[4]);
    f[8] = x[12] - x[19];
    f[9] = x[12] + x[19];
    f[10] = MUL_F(FRAC_CONST(0.7071067811865476), f[9]);
    f[11] = MUL_F(FRAC_CONST(0.7071067811865476), f[8]);
    f[12] = x[11] - x[20];
    f[13] = x[11] + x[20];
    f[14] = MUL_F(FRAC_CONST(0.7071067811865476), f[13]);
    f[15] = MUL_F(FRAC_CONST(0.7071067811865476), f[12]);
    f[16] = x[14] - x[17];
    f[17] = x[14] + x[17];
    f[18] = MUL_F(FRAC_CONST(0.7071067811865476), f[17]);
    f[19] = MUL_F(FRAC_CONST(0.7071067811865476), f[16]);
    f[20] = x[9] - x[22];
    f[21] = x[9] + x[22];
    f[22] = MUL_F(FRAC_CONST(0.7071067811865476), f[21]);
    f[23] = MUL_F(FRAC_CONST(0.7071067811865476), f[20]);
    f[24] = x[13] - x[18];
    f[25] = x[13] + x[18];
    f[26] = MUL_F(FRAC_CONST(0.7071067811865476), f[25]);
    f[27] = MUL_F(FRAC_CONST(0.7071067811865476), f[24]);
    f[28] = x[10] - x[21];
    f[29] = x[10] + x[21];
    f[30] = MUL_F(FRAC_CONST(0.7071067811865476), f[29]);
    f[31] = MUL_F(FRAC_CONST(0.7071067811865476), f[28]);
    f[32] = x[0] - f[2];
    f[33] = x[0] + f[2];
    f[34] = x[31] - f[3];
    f[35] = x[31] + f[3];
    f[36] = x[7] - f[6];
    f[37] = x[7] + f[6];
    f[38] = x[24] - f[7];
    f[39] = x[24] + f[7];
    f[40] = x[3] - f[10];
    f[41] = x[3] + f[10];
    f[42] = x[28] - f[11];
    f[43] = x[28] + f[11];
    f[44] = x[4] - f[14];
    f[45] = x[4] + f[14];
    f[46] = x[27] - f[15];
    f[47] = x[27] + f[15];
    f[48] = x[1] - f[18];
    f[49] = x[1] + f[18];
    f[50] = x[30] - f[19];
    f[51] = x[30] + f[19];
    f[52] = x[6] - f[22];
    f[53] = x[6] + f[22];
    f[54] = x[25] - f[23];
    f[55] = x[25] + f[23];
    f[56] = x[2] - f[26];
    f[57] = x[2] + f[26];
    f[58] = x[29] - f[27];
    f[59] = x[29] + f[27];
    f[60] = x[5] - f[30];
    f[61] = x[5] + f[30];
    f[62] = x[26] - f[31];
    f[63] = x[26] + f[31];
    f[64] = f[39] + f[37];
    f[65] = MUL_F(FRAC_CONST(-0.5411961001461969), f[39]);
    f[66] = MUL_F(FRAC_CONST(0.9238795325112867), f[64]);
    f[67] = MUL_C(COEF_CONST(1.3065629648763766), f[37]);
    f[68] = f[65] + f[66];
    f[69] = f[67] - f[66];
    f[70] = f[38] + f[36];
    f[71] = MUL_C(COEF_CONST(1.3065629648763770), f[38]);
    f[72] = MUL_F(FRAC_CONST(-0.3826834323650904), f[70]);
    f[73] = MUL_F(FRAC_CONST(0.5411961001461961), f[36]);
    f[74] = f[71] + f[72];
    f[75] = f[73] - f[72];
    f[76] = f[47] + f[45];
    f[77] = MUL_F(FRAC_CONST(-0.5411961001461969), f[47]);
    f[78] = MUL_F(FRAC_CONST(0.9238795325112867), f[76]);
    f[79] = MUL_C(COEF_CONST(1.3065629648763766), f[45]);
    f[80] = f[77] + f[78];
    f[81] = f[79] - f[78];
    f[82] = f[46] + f[44];
    f[83] = MUL_C(COEF_CONST(1.3065629648763770), f[46]);
    f[84] = MUL_F(FRAC_CONST(-0.3826834323650904), f[82]);
    f[85] = MUL_F(FRAC_CONST(0.5411961001461961), f[44]);
    f[86] = f[83] + f[84];
    f[87] = f[85] - f[84];
    f[88] = f[55] + f[53];
    f[89] = MUL_F(FRAC_CONST(-0.5411961001461969), f[55]);
    f[90] = MUL_F(FRAC_CONST(0.9238795325112867), f[88]);
    f[91] = MUL_C(COEF_CONST(1.3065629648763766), f[53]);
    f[92] = f[89] + f[90];
    f[93] = f[91] - f[90];
    f[94] = f[54] + f[52];
    f[95] = MUL_C(COEF_CONST(1.3065629648763770), f[54]);
    f[96] = MUL_F(FRAC_CONST(-0.3826834323650904), f[94]);
    f[97] = MUL_F(FRAC_CONST(0.5411961001461961), f[52]);
    f[98] = f[95] + f[96];
    f[99] = f[97] - f[96];
    f[100] = f[63] + f[61];
    f[101] = MUL_F(FRAC_CONST(-0.5411961001461969), f[63]);
    f[102] = MUL_F(FRAC_CONST(0.9238795325112867), f[100]);
    f[103] = MUL_C(COEF_CONST(1.3065629648763766), f[61]);
    f[104] = f[101] + f[102];
    f[105] = f[103] - f[102];
    f[106] = f[62] + f[60];
    f[107] = MUL_C(COEF_CONST(1.3065629648763770), f[62]);
    f[108] = MUL_F(FRAC_CONST(-0.3826834323650904), f[106]);
    f[109] = MUL_F(FRAC_CONST(0.5411961001461961), f[60]);
    f[110] = f[107] + f[108];
    f[111] = f[109] - f[108];
    f[112] = f[33] - f[68];
    f[113] = f[33] + f[68];
    f[114] = f[35] - f[69];
    f[115] = f[35] + f[69];
    f[116] = f[32] - f[74];
    f[117] = f[32] + f[74];
    f[118] = f[34] - f[75];
    f[119] = f[34] + f[75];
    f[120] = f[41] - f[80];
    f[121] = f[41] + f[80];
    f[122] = f[43] - f[81];
    f[123] = f[43] + f[81];
    f[124] = f[40] - f[86];
    f[125] = f[40] + f[86];
    f[126] = f[42] - f[87];
    f[127] = f[42] + f[87];
    f[128] = f[49] - f[92];
    f[129] = f[49] + f[92];
    f[130] = f[51] - f[93];
    f[131] = f[51] + f[93];
    f[132] = f[48] - f[98];
    f[133] = f[48] + f[98];
    f[134] = f[50] - f[99];
    f[135] = f[50] + f[99];
    f[136] = f[57] - f[104];
    f[137] = f[57] + f[104];
    f[138] = f[59] - f[105];
    f[139] = f[59] + f[105];
    f[140] = f[56] - f[110];
    f[141] = f[56] + f[110];
    f[142] = f[58] - f[111];
    f[143] = f[58] + f[111];
    f[144] = f[123] + f[121];
    f[145] = MUL_F(FRAC_CONST(-0.7856949583871021), f[123]);
    f[146] = MUL_F(FRAC_CONST(0.9807852804032304), f[144]);
    f[147] = MUL_C(COEF_CONST(1.1758756024193588), f[121]);
    f[148] = f[145] + f[146];
    f[149] = f[147] - f[146];
    f[150] = f[127] + f[125];
    f[151] = MUL_F(FRAC_CONST(0.2758993792829431), f[127]);
    f[152] = MUL_F(FRAC_CONST(0.5555702330196022), f[150]);
    f[153] = MUL_C(COEF_CONST(1.3870398453221475), f[125]);
    f[154] = f[151] + f[152];
    f[155] = f[153] - f[152];
    f[156] = f[122] + f[120];
    f[157] = MUL_C(COEF_CONST(1.1758756024193591), f[122]);
    f[158] = MUL_F(FRAC_CONST(-0.1950903220161287), f[156]);
    f[159] = MUL_F(FRAC_CONST(0.7856949583871016), f[120]);
    f[160] = f[157] + f[158];
    f[161] = f[159] - f[158];
    f[162] = f[126] + f[124];
    f[163] = MUL_C(COEF_CONST(1.3870398453221473), f[126]);
    f[164] = MUL_F(FRAC_CONST(-0.8314696123025455), f[162]);
    f[165] = MUL_F(FRAC_CONST(-0.2758993792829436), f[124]);
    f[166] = f[163] + f[164];
    f[167] = f[165] - f[164];
    f[168] = f[139] + f[137];
    f[169] = MUL_F(FRAC_CONST(-0.7856949583871021), f[139]);
    f[170] = MUL_F(FRAC_CONST(0.9807852804032304), f[168]);
    f[171] = MUL_C(COEF_CONST(1.1758756024193588), f[137]);
    f[172] = f[169] + f[170];
    f[173] = f[171] - f[170];
    f[174] = f[143] + f[141];
    f[175] = MUL_F(FRAC_CONST(0.2758993792829431), f[143]);
    f[176] = MUL_F(FRAC_CONST(0.5555702330196022), f[174]);
    f[177] = MUL_C(COEF_CONST(1.3870398453221475), f[141]);
    f[178] = f[175] + f[176];
    f[179] = f[177] - f[176];
    f[180] = f[138] + f[136];
    f[181] = MUL_C(COEF_CONST(1.1758756024193591), f[138]);
    f[182] = MUL_F(FRAC_CONST(-0.1950903220161287), f[180]);
    f[183] = MUL_F(FRAC_CONST(0.7856949583871016), f[136]);
    f[184] = f[181] + f[182];
    f[185] = f[183] - f[182];
    f[186] = f[142] + f[140];
    f[187] = MUL_C(COEF_CONST(1.3870398453221473), f[142]);
    f[188] = MUL_F(FRAC_CONST(-0.8314696123025455), f[186]);
    f[189] = MUL_F(FRAC_CONST(-0.2758993792829436), f[140]);
    f[190] = f[187] + f[188];
    f[191] = f[189] - f[188];
    f[192] = f[113] - f[148];
    f[193] = f[113] + f[148];
    f[194] = f[115] - f[149];
    f[195] = f[115] + f[149];
    f[196] = f[117] - f[154];
    f[197] = f[117] + f[154];
    f[198] = f[119] - f[155];
    f[199] = f[119] + f[155];
    f[200] = f[112] - f[160];
    f[201] = f[112] + f[160];
    f[202] = f[114] - f[161];
    f[203] = f[114] + f[161];
    f[204] = f[116] - f[166];
    f[205] = f[116] + f[166];
    f[206] = f[118] - f[167];
    f[207] = f[118] + f[167];
    f[208] = f[129] - f[172];
    f[209] = f[129] + f[172];
    f[210] = f[131] - f[173];
    f[211] = f[131] + f[173];
    f[212] = f[133] - f[178];
    f[213] = f[133] + f[178];
    f[214] = f[135] - f[179];
    f[215] = f[135] + f[179];
    f[216] = f[128] - f[184];
    f[217] = f[128] + f[184];
    f[218] = f[130] - f[185];
    f[219] = f[130] + f[185];
    f[220] = f[132] - f[190];
    f[221] = f[132] + f[190];
    f[222] = f[134] - f[191];
    f[223] = f[134] + f[191];
    f[224] = f[211] + f[209];
    f[225] = MUL_F(FRAC_CONST(-0.8971675863426361), f[211]);
    f[226] = MUL_F(FRAC_CONST(0.9951847266721968), f[224]);
    f[227] = MUL_C(COEF_CONST(1.0932018670017576), f[209]);
    f[228] = f[225] + f[226];
    f[229] = f[227] - f[226];
    f[230] = f[215] + f[213];
    f[231] = MUL_F(FRAC_CONST(-0.4105245275223571), f[215]);
    f[232] = MUL_F(FRAC_CONST(0.8819212643483549), f[230]);
    f[233] = MUL_C(COEF_CONST(1.3533180011743529), f[213]);
    f[234] = f[231] + f[232];
    f[235] = f[233] - f[232];
    f[236] = f[219] + f[217];
    f[237] = MUL_F(FRAC_CONST(0.1386171691990915), f[219]);
    f[238] = MUL_F(FRAC_CONST(0.6343932841636455), f[236]);
    f[239] = MUL_C(COEF_CONST(1.4074037375263826), f[217]);
    f[240] = f[237] + f[238];
    f[241] = f[239] - f[238];
    f[242] = f[223] + f[221];
    f[243] = MUL_F(FRAC_CONST(0.6666556584777466), f[223]);
    f[244] = MUL_F(FRAC_CONST(0.2902846772544623), f[242]);
    f[245] = MUL_C(COEF_CONST(1.2472250129866711), f[221]);
    f[246] = f[243] + f[244];
    f[247] = f[245] - f[244];
    f[248] = f[210] + f[208];
    f[249] = MUL_C(COEF_CONST(1.0932018670017574), f[210]);
    f[250] = MUL_F(FRAC_CONST(-0.0980171403295605), f[248]);
    f[251] = MUL_F(FRAC_CONST(0.8971675863426364), f[208]);
    f[252] = f[249] + f[250];
    f[253] = f[251] - f[250];
    f[254] = f[214] + f[212];
    f[255] = MUL_C(COEF_CONST(1.3533180011743529), f[214]);
    f[256] = MUL_F(FRAC_CONST(-0.4713967368259979), f[254]);
    f[257] = MUL_F(FRAC_CONST(0.4105245275223569), f[212]);
    f[258] = f[255] + f[256];
    f[259] = f[257] - f[256];
    f[260] = f[218] + f[216];
    f[261] = MUL_C(COEF_CONST(1.4074037375263826), f[218]);
    f[262] = MUL_F(FRAC_CONST(-0.7730104533627369), f[260]);
    f[263] = MUL_F(FRAC_CONST(-0.1386171691990913), f[216]);
    f[264] = f[261] + f[262];
    f[265] = f[263] - f[262];
    f[266] = f[222] + f[220];
    f[267] = MUL_C(COEF_CONST(1.2472250129866711), f[222]);
    f[268] = MUL_F(FRAC_CONST(-0.9569403357322089), f[266]);
    f[269] = MUL_F(FRAC_CONST(-0.6666556584777469), f[220]);
    f[270] = f[267] + f[268];
    f[271] = f[269] - f[268];
    f[272] = f[193] - f[228];
    f[273] = f[193] + f[228];
    f[274] = f[195] - f[229];
    f[275] = f[195] + f[229];
    f[276] = f[197] - f[234];
    f[277] = f[197] + f[234];
    f[278] = f[199] - f[235];
    f[279] = f[199] + f[235];
    f[280] = f[201] - f[240];
    f[281] = f[201] + f[240];
    f[282] = f[203] - f[241];
    f[283] = f[203] + f[241];
    f[284] = f[205] - f[246];
    f[285] = f[205] + f[246];
    f[286] = f[207] - f[247];
    f[287] = f[207] + f[247];
    f[288] = f[192] - f[252];
    f[289] = f[192] + f[252];
    f[290] = f[194] - f[253];
    f[291] = f[194] + f[253];
    f[292] = f[196] - f[258];
    f[293] = f[196] + f[258];
    f[294] = f[198] - f[259];
    f[295] = f[198] + f[259];
    f[296] = f[200] - f[264];
    f[297] = f[200] + f[264];
    f[298] = f[202] - f[265];
    f[299] = f[202] + f[265];
    f[300] = f[204] - f[270];
    f[301] = f[204] + f[270];
    f[302] = f[206] - f[271];
    f[303] = f[206] + f[271];
    f[304] = f[275] + f[273];
    f[305] = MUL_F(FRAC_CONST(-0.9751575901732920), f[275]);
    f[306] = MUL_F(FRAC_CONST(0.9996988186962043), f[304]);
    f[307] = MUL_C(COEF_CONST(1.0242400472191164), f[273]);
    y[0] = f[305] + f[306];
    y[31] = f[307] - f[306];
    f[310] = f[279] + f[277];
    f[311] = MUL_F(FRAC_CONST(-0.8700688593994936), f[279]);
    f[312] = MUL_F(FRAC_CONST(0.9924795345987100), f[310]);
    f[313] = MUL_C(COEF_CONST(1.1148902097979263), f[277]);
    y[2] = f[311] + f[312];
    y[29] = f[313] - f[312];
    f[316] = f[283] + f[281];
    f[317] = MUL_F(FRAC_CONST(-0.7566008898816587), f[283]);
    f[318] = MUL_F(FRAC_CONST(0.9757021300385286), f[316]);
    f[319] = MUL_C(COEF_CONST(1.1948033701953984), f[281]);
    y[4] = f[317] + f[318];
    y[27] = f[319] - f[318];
    f[322] = f[287] + f[285];
    f[323] = MUL_F(FRAC_CONST(-0.6358464401941451), f[287]);
    f[324] = MUL_F(FRAC_CONST(0.9495281805930367), f[322]);
    f[325] = MUL_C(COEF_CONST(1.2632099209919283), f[285]);
    y[6] = f[323] + f[324];
    y[25] = f[325] - f[324];
    f[328] = f[291] + f[289];
    f[329] = MUL_F(FRAC_CONST(-0.5089684416985408), f[291]);
    f[330] = MUL_F(FRAC_CONST(0.9142097557035307), f[328]);
    f[331] = MUL_C(COEF_CONST(1.3194510697085207), f[289]);
    y[8] = f[329] + f[330];
    y[23] = f[331] - f[330];
    f[334] = f[295] + f[293];
    f[335] = MUL_F(FRAC_CONST(-0.3771887988789273), f[295]);
    f[336] = MUL_F(FRAC_CONST(0.8700869911087114), f[334]);
    f[337] = MUL_C(COEF_CONST(1.3629851833384954), f[293]);
    y[10] = f[335] + f[336];
    y[21] = f[337] - f[336];
    f[340] = f[299] + f[297];
    f[341] = MUL_F(FRAC_CONST(-0.2417766217337384), f[299]);
    f[342] = MUL_F(FRAC_CONST(0.8175848131515837), f[340]);
    f[343] = MUL_C(COEF_CONST(1.3933930045694289), f[297]);
    y[12] = f[341] + f[342];
    y[19] = f[343] - f[342];
    f[346] = f[303] + f[301];
    f[347] = MUL_F(FRAC_CONST(-0.1040360035527077), f[303]);
    f[348] = MUL_F(FRAC_CONST(0.7572088465064845), f[346]);
    f[349] = MUL_C(COEF_CONST(1.4103816894602612), f[301]);
    y[14] = f[347] + f[348];
    y[17] = f[349] - f[348];
    f[352] = f[274] + f[272];
    f[353] = MUL_F(FRAC_CONST(0.0347065382144002), f[274]);
    f[354] = MUL_F(FRAC_CONST(0.6895405447370668), f[352]);
    f[355] = MUL_C(COEF_CONST(1.4137876276885337), f[272]);
    y[16] = f[353] + f[354];
    y[15] = f[355] - f[354];
    f[358] = f[278] + f[276];
    f[359] = MUL_F(FRAC_CONST(0.1731148370459795), f[278]);
    f[360] = MUL_F(FRAC_CONST(0.6152315905806268), f[358]);
    f[361] = MUL_C(COEF_CONST(1.4035780182072330), f[276]);
    y[18] = f[359] + f[360];
    y[13] = f[361] - f[360];
    f[364] = f[282] + f[280];
    f[365] = MUL_F(FRAC_CONST(0.3098559453626100), f[282]);
    f[366] = MUL_F(FRAC_CONST(0.5349976198870972), f[364]);
    f[367] = MUL_C(COEF_CONST(1.3798511851368043), f[280]);
    y[20] = f[365] + f[366];
    y[11] = f[367] - f[366];
    f[370] = f[286] + f[284];
    f[371] = MUL_F(FRAC_CONST(0.4436129715409088), f[286]);
    f[372] = MUL_F(FRAC_CONST(0.4496113296546065), f[370]);
    f[373] = MUL_C(COEF_CONST(1.3428356308501219), f[284]);
    y[22] = f[371] + f[372];
    y[9] = f[373] - f[372];
    f[376] = f[290] + f[288];
    f[377] = MUL_F(FRAC_CONST(0.5730977622997509), f[290]);
    f[378] = MUL_F(FRAC_CONST(0.3598950365349881), f[376]);
    f[379] = MUL_C(COEF_CONST(1.2928878353697271), f[288]);
    y[24] = f[377] + f[378];
    y[7] = f[379] - f[378];
    f[382] = f[294] + f[292];
    f[383] = MUL_F(FRAC_CONST(0.6970633083205415), f[294]);
    f[384] = MUL_F(FRAC_CONST(0.2667127574748984), f[382]);
    f[385] = MUL_C(COEF_CONST(1.2304888232703382), f[292]);
    y[26] = f[383] + f[384];
    y[5] = f[385] - f[384];
    f[388] = f[298] + f[296];
    f[389] = MUL_F(FRAC_CONST(0.8143157536286401), f[298]);
    f[390] = MUL_F(FRAC_CONST(0.1709618887603012), f[388]);
    f[391] = MUL_C(COEF_CONST(1.1562395311492424), f[296]);
    y[28] = f[389] + f[390];
    y[3] = f[391] - f[390];
    f[394] = f[302] + f[300];
    f[395] = MUL_F(FRAC_CONST(0.9237258930790228), f[302]);
    f[396] = MUL_F(FRAC_CONST(0.0735645635996674), f[394]);
    f[397] = MUL_C(COEF_CONST(1.0708550202783576), f[300]);
    y[30] = f[395] + f[396];
    y[1] = f[397] - f[396];
    if(f) {
        free(f);
        f = NULL;
    }
}
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
void DST4_32(real_t* y, real_t* x) {
    // printf(ANSI_ESC_YELLOW "DST4_32" ANSI_ESC_WHITE "\n");
    int32_t* f = (int32_t*)faad_malloc(336 * sizeof(int32_t));
    f[0] = x[0] - x[1];
    f[1] = x[2] - x[1];
    f[2] = x[2] - x[3];
    f[3] = x[4] - x[3];
    f[4] = x[4] - x[5];
    f[5] = x[6] - x[5];
    f[6] = x[6] - x[7];
    f[7] = x[8] - x[7];
    f[8] = x[8] - x[9];
    f[9] = x[10] - x[9];
    f[10] = x[10] - x[11];
    f[11] = x[12] - x[11];
    f[12] = x[12] - x[13];
    f[13] = x[14] - x[13];
    f[14] = x[14] - x[15];
    f[15] = x[16] - x[15];
    f[16] = x[16] - x[17];
    f[17] = x[18] - x[17];
    f[18] = x[18] - x[19];
    f[19] = x[20] - x[19];
    f[20] = x[20] - x[21];
    f[21] = x[22] - x[21];
    f[22] = x[22] - x[23];
    f[23] = x[24] - x[23];
    f[24] = x[24] - x[25];
    f[25] = x[26] - x[25];
    f[26] = x[26] - x[27];
    f[27] = x[28] - x[27];
    f[28] = x[28] - x[29];
    f[29] = x[30] - x[29];
    f[30] = x[30] - x[31];
    f[31] = MUL_F(FRAC_CONST(0.7071067811865476), f[15]);
    f[32] = x[0] - f[31];
    f[33] = x[0] + f[31];
    f[34] = f[7] + f[23];
    f[35] = MUL_C(COEF_CONST(1.3065629648763766), f[7]);
    f[36] = MUL_F(FRAC_CONST(-0.9238795325112866), f[34]);
    f[37] = MUL_F(FRAC_CONST(-0.5411961001461967), f[23]);
    f[38] = f[35] + f[36];
    f[39] = f[37] - f[36];
    f[40] = f[33] - f[39];
    f[41] = f[33] + f[39];
    f[42] = f[32] - f[38];
    f[43] = f[32] + f[38];
    f[44] = f[11] - f[19];
    f[45] = f[11] + f[19];
    f[46] = MUL_F(FRAC_CONST(0.7071067811865476), f[45]);
    f[47] = f[3] - f[46];
    f[48] = f[3] + f[46];
    f[49] = MUL_F(FRAC_CONST(0.7071067811865476), f[44]);
    f[50] = f[49] - f[27];
    f[51] = f[49] + f[27];
    f[52] = f[51] + f[48];
    f[53] = MUL_F(FRAC_CONST(-0.7856949583871021), f[51]);
    f[54] = MUL_F(FRAC_CONST(0.9807852804032304), f[52]);
    f[55] = MUL_C(COEF_CONST(1.1758756024193588), f[48]);
    f[56] = f[53] + f[54];
    f[57] = f[55] - f[54];
    f[58] = f[50] + f[47];
    f[59] = MUL_F(FRAC_CONST(-0.2758993792829430), f[50]);
    f[60] = MUL_F(FRAC_CONST(0.8314696123025452), f[58]);
    f[61] = MUL_C(COEF_CONST(1.3870398453221475), f[47]);
    f[62] = f[59] + f[60];
    f[63] = f[61] - f[60];
    f[64] = f[41] - f[56];
    f[65] = f[41] + f[56];
    f[66] = f[43] - f[62];
    f[67] = f[43] + f[62];
    f[68] = f[42] - f[63];
    f[69] = f[42] + f[63];
    f[70] = f[40] - f[57];
    f[71] = f[40] + f[57];
    f[72] = f[5] - f[9];
    f[73] = f[5] + f[9];
    f[74] = f[13] - f[17];
    f[75] = f[13] + f[17];
    f[76] = f[21] - f[25];
    f[77] = f[21] + f[25];
    f[78] = MUL_F(FRAC_CONST(0.7071067811865476), f[75]);
    f[79] = f[1] - f[78];
    f[80] = f[1] + f[78];
    f[81] = f[73] + f[77];
    f[82] = MUL_C(COEF_CONST(1.3065629648763766), f[73]);
    f[83] = MUL_F(FRAC_CONST(-0.9238795325112866), f[81]);
    f[84] = MUL_F(FRAC_CONST(-0.5411961001461967), f[77]);
    f[85] = f[82] + f[83];
    f[86] = f[84] - f[83];
    f[87] = f[80] - f[86];
    f[88] = f[80] + f[86];
    f[89] = f[79] - f[85];
    f[90] = f[79] + f[85];
    f[91] = MUL_F(FRAC_CONST(0.7071067811865476), f[74]);
    f[92] = f[29] - f[91];
    f[93] = f[29] + f[91];
    f[94] = f[76] + f[72];
    f[95] = MUL_C(COEF_CONST(1.3065629648763766), f[76]);
    f[96] = MUL_F(FRAC_CONST(-0.9238795325112866), f[94]);
    f[97] = MUL_F(FRAC_CONST(-0.5411961001461967), f[72]);
    f[98] = f[95] + f[96];
    f[99] = f[97] - f[96];
    f[100] = f[93] - f[99];
    f[101] = f[93] + f[99];
    f[102] = f[92] - f[98];
    f[103] = f[92] + f[98];
    f[104] = f[101] + f[88];
    f[105] = MUL_F(FRAC_CONST(-0.8971675863426361), f[101]);
    f[106] = MUL_F(FRAC_CONST(0.9951847266721968), f[104]);
    f[107] = MUL_C(COEF_CONST(1.0932018670017576), f[88]);
    f[108] = f[105] + f[106];
    f[109] = f[107] - f[106];
    f[110] = f[90] - f[103];
    f[111] = MUL_F(FRAC_CONST(-0.6666556584777466), f[103]);
    f[112] = MUL_F(FRAC_CONST(0.9569403357322089), f[110]);
    f[113] = MUL_C(COEF_CONST(1.2472250129866713), f[90]);
    f[114] = f[112] - f[111];
    f[115] = f[113] - f[112];
    f[116] = f[102] + f[89];
    f[117] = MUL_F(FRAC_CONST(-0.4105245275223571), f[102]);
    f[118] = MUL_F(FRAC_CONST(0.8819212643483549), f[116]);
    f[119] = MUL_C(COEF_CONST(1.3533180011743529), f[89]);
    f[120] = f[117] + f[118];
    f[121] = f[119] - f[118];
    f[122] = f[87] - f[100];
    f[123] = MUL_F(FRAC_CONST(-0.1386171691990915), f[100]);
    f[124] = MUL_F(FRAC_CONST(0.7730104533627370), f[122]);
    f[125] = MUL_C(COEF_CONST(1.4074037375263826), f[87]);
    f[126] = f[124] - f[123];
    f[127] = f[125] - f[124];
    f[128] = f[65] - f[108];
    f[129] = f[65] + f[108];
    f[130] = f[67] - f[114];
    f[131] = f[67] + f[114];
    f[132] = f[69] - f[120];
    f[133] = f[69] + f[120];
    f[134] = f[71] - f[126];
    f[135] = f[71] + f[126];
    f[136] = f[70] - f[127];
    f[137] = f[70] + f[127];
    f[138] = f[68] - f[121];
    f[139] = f[68] + f[121];
    f[140] = f[66] - f[115];
    f[141] = f[66] + f[115];
    f[142] = f[64] - f[109];
    f[143] = f[64] + f[109];
    f[144] = f[0] + f[30];
    f[145] = MUL_C(COEF_CONST(1.0478631305325901), f[0]);
    f[146] = MUL_F(FRAC_CONST(-0.9987954562051724), f[144]);
    f[147] = MUL_F(FRAC_CONST(-0.9497277818777548), f[30]);
    f[148] = f[145] + f[146];
    f[149] = f[147] - f[146];
    f[150] = f[4] + f[26];
    f[151] = MUL_F(FRAC_CONST(1.2130114330978077), f[4]);
    f[152] = MUL_F(FRAC_CONST(-0.9700312531945440), f[150]);
    f[153] = MUL_F(FRAC_CONST(-0.7270510732912803), f[26]);
    f[154] = f[151] + f[152];
    f[155] = f[153] - f[152];
    f[156] = f[8] + f[22];
    f[157] = MUL_C(COEF_CONST(1.3315443865537255), f[8]);
    f[158] = MUL_F(FRAC_CONST(-0.9039892931234433), f[156]);
    f[159] = MUL_F(FRAC_CONST(-0.4764341996931612), f[22]);
    f[160] = f[157] + f[158];
    f[161] = f[159] - f[158];
    f[162] = f[12] + f[18];
    f[163] = MUL_C(COEF_CONST(1.3989068359730781), f[12]);
    f[164] = MUL_F(FRAC_CONST(-0.8032075314806453), f[162]);
    f[165] = MUL_F(FRAC_CONST(-0.2075082269882124), f[18]);
    f[166] = f[163] + f[164];
    f[167] = f[165] - f[164];
    f[168] = f[16] + f[14];
    f[169] = MUL_C(COEF_CONST(1.4125100802019777), f[16]);
    f[170] = MUL_F(FRAC_CONST(-0.6715589548470187), f[168]);
    f[171] = MUL_F(FRAC_CONST(0.0693921705079402), f[14]);
    f[172] = f[169] + f[170];
    f[173] = f[171] - f[170];
    f[174] = f[20] + f[10];
    f[175] = MUL_C(COEF_CONST(1.3718313541934939), f[20]);
    f[176] = MUL_F(FRAC_CONST(-0.5141027441932219), f[174]);
    f[177] = MUL_F(FRAC_CONST(0.3436258658070501), f[10]);
    f[178] = f[175] + f[176];
    f[179] = f[177] - f[176];
    f[180] = f[24] + f[6];
    f[181] = MUL_C(COEF_CONST(1.2784339185752409), f[24]);
    f[182] = MUL_F(FRAC_CONST(-0.3368898533922200), f[180]);
    f[183] = MUL_F(FRAC_CONST(0.6046542117908008), f[6]);
    f[184] = f[181] + f[182];
    f[185] = f[183] - f[182];
    f[186] = f[28] + f[2];
    f[187] = MUL_C(COEF_CONST(1.1359069844201433), f[28]);
    f[188] = MUL_F(FRAC_CONST(-0.1467304744553624), f[186]);
    f[189] = MUL_F(FRAC_CONST(0.8424460355094185), f[2]);
    f[190] = f[187] + f[188];
    f[191] = f[189] - f[188];
    f[192] = f[149] - f[173];
    f[193] = f[149] + f[173];
    f[194] = f[148] - f[172];
    f[195] = f[148] + f[172];
    f[196] = f[155] - f[179];
    f[197] = f[155] + f[179];
    f[198] = f[154] - f[178];
    f[199] = f[154] + f[178];
    f[200] = f[161] - f[185];
    f[201] = f[161] + f[185];
    f[202] = f[160] - f[184];
    f[203] = f[160] + f[184];
    f[204] = f[167] - f[191];
    f[205] = f[167] + f[191];
    f[206] = f[166] - f[190];
    f[207] = f[166] + f[190];
    f[208] = f[192] + f[194];
    f[209] = MUL_C(COEF_CONST(1.1758756024193588), f[192]);
    f[210] = MUL_F(FRAC_CONST(-0.9807852804032304), f[208]);
    f[211] = MUL_F(FRAC_CONST(-0.7856949583871021), f[194]);
    f[212] = f[209] + f[210];
    f[213] = f[211] - f[210];
    f[214] = f[196] + f[198];
    f[215] = MUL_C(COEF_CONST(1.3870398453221475), f[196]);
    f[216] = MUL_F(FRAC_CONST(-0.5555702330196022), f[214]);
    f[217] = MUL_F(FRAC_CONST(0.2758993792829431), f[198]);
    f[218] = f[215] + f[216];
    f[219] = f[217] - f[216];
    f[220] = f[200] + f[202];
    f[221] = MUL_F(FRAC_CONST(0.7856949583871022), f[200]);
    f[222] = MUL_F(FRAC_CONST(0.1950903220161283), f[220]);
    f[223] = MUL_C(COEF_CONST(1.1758756024193586), f[202]);
    f[224] = f[221] + f[222];
    f[225] = f[223] - f[222];
    f[226] = f[204] + f[206];
    f[227] = MUL_F(FRAC_CONST(-0.2758993792829430), f[204]);
    f[228] = MUL_F(FRAC_CONST(0.8314696123025452), f[226]);
    f[229] = MUL_C(COEF_CONST(1.3870398453221475), f[206]);
    f[230] = f[227] + f[228];
    f[231] = f[229] - f[228];
    f[232] = f[193] - f[201];
    f[233] = f[193] + f[201];
    f[234] = f[195] - f[203];
    f[235] = f[195] + f[203];
    f[236] = f[197] - f[205];
    f[237] = f[197] + f[205];
    f[238] = f[199] - f[207];
    f[239] = f[199] + f[207];
    f[240] = f[213] - f[225];
    f[241] = f[213] + f[225];
    f[242] = f[212] - f[224];
    f[243] = f[212] + f[224];
    f[244] = f[219] - f[231];
    f[245] = f[219] + f[231];
    f[246] = f[218] - f[230];
    f[247] = f[218] + f[230];
    f[248] = f[232] + f[234];
    f[249] = MUL_C(COEF_CONST(1.3065629648763766), f[232]);
    f[250] = MUL_F(FRAC_CONST(-0.9238795325112866), f[248]);
    f[251] = MUL_F(FRAC_CONST(-0.5411961001461967), f[234]);
    f[252] = f[249] + f[250];
    f[253] = f[251] - f[250];
    f[254] = f[236] + f[238];
    f[255] = MUL_F(FRAC_CONST(0.5411961001461969), f[236]);
    f[256] = MUL_F(FRAC_CONST(0.3826834323650898), f[254]);
    f[257] = MUL_C(COEF_CONST(1.3065629648763766), f[238]);
    f[258] = f[255] + f[256];
    f[259] = f[257] - f[256];
    f[260] = f[240] + f[242];
    f[261] = MUL_C(COEF_CONST(1.3065629648763766), f[240]);
    f[262] = MUL_F(FRAC_CONST(-0.9238795325112866), f[260]);
    f[263] = MUL_F(FRAC_CONST(-0.5411961001461967), f[242]);
    f[264] = f[261] + f[262];
    f[265] = f[263] - f[262];
    f[266] = f[244] + f[246];
    f[267] = MUL_F(FRAC_CONST(0.5411961001461969), f[244]);
    f[268] = MUL_F(FRAC_CONST(0.3826834323650898), f[266]);
    f[269] = MUL_C(COEF_CONST(1.3065629648763766), f[246]);
    f[270] = f[267] + f[268];
    f[271] = f[269] - f[268];
    f[272] = f[233] - f[237];
    f[273] = f[233] + f[237];
    f[274] = f[235] - f[239];
    f[275] = f[235] + f[239];
    f[276] = f[253] - f[259];
    f[277] = f[253] + f[259];
    f[278] = f[252] - f[258];
    f[279] = f[252] + f[258];
    f[280] = f[241] - f[245];
    f[281] = f[241] + f[245];
    f[282] = f[243] - f[247];
    f[283] = f[243] + f[247];
    f[284] = f[265] - f[271];
    f[285] = f[265] + f[271];
    f[286] = f[264] - f[270];
    f[287] = f[264] + f[270];
    f[288] = f[272] - f[274];
    f[289] = f[272] + f[274];
    f[290] = MUL_F(FRAC_CONST(0.7071067811865474), f[288]);
    f[291] = MUL_F(FRAC_CONST(0.7071067811865474), f[289]);
    f[292] = f[276] - f[278];
    f[293] = f[276] + f[278];
    f[294] = MUL_F(FRAC_CONST(0.7071067811865474), f[292]);
    f[295] = MUL_F(FRAC_CONST(0.7071067811865474), f[293]);
    f[296] = f[280] - f[282];
    f[297] = f[280] + f[282];
    f[298] = MUL_F(FRAC_CONST(0.7071067811865474), f[296]);
    f[299] = MUL_F(FRAC_CONST(0.7071067811865474), f[297]);
    f[300] = f[284] - f[286];
    f[301] = f[284] + f[286];
    f[302] = MUL_F(FRAC_CONST(0.7071067811865474), f[300]);
    f[303] = MUL_F(FRAC_CONST(0.7071067811865474), f[301]);
    f[304] = f[129] - f[273];
    f[305] = f[129] + f[273];
    f[306] = f[131] - f[281];
    f[307] = f[131] + f[281];
    f[308] = f[133] - f[285];
    f[309] = f[133] + f[285];
    f[310] = f[135] - f[277];
    f[311] = f[135] + f[277];
    f[312] = f[137] - f[295];
    f[313] = f[137] + f[295];
    f[314] = f[139] - f[303];
    f[315] = f[139] + f[303];
    f[316] = f[141] - f[299];
    f[317] = f[141] + f[299];
    f[318] = f[143] - f[291];
    f[319] = f[143] + f[291];
    f[320] = f[142] - f[290];
    f[321] = f[142] + f[290];
    f[322] = f[140] - f[298];
    f[323] = f[140] + f[298];
    f[324] = f[138] - f[302];
    f[325] = f[138] + f[302];
    f[326] = f[136] - f[294];
    f[327] = f[136] + f[294];
    f[328] = f[134] - f[279];
    f[329] = f[134] + f[279];
    f[330] = f[132] - f[287];
    f[331] = f[132] + f[287];
    f[332] = f[130] - f[283];
    f[333] = f[130] + f[283];
    f[334] = f[128] - f[275];
    f[335] = f[128] + f[275];
    y[31] = MUL_F(FRAC_CONST(0.5001506360206510), f[305]);
    y[30] = MUL_F(FRAC_CONST(0.5013584524464084), f[307]);
    y[29] = MUL_F(FRAC_CONST(0.5037887256810443), f[309]);
    y[28] = MUL_F(FRAC_CONST(0.5074711720725553), f[311]);
    y[27] = MUL_F(FRAC_CONST(0.5124514794082247), f[313]);
    y[26] = MUL_F(FRAC_CONST(0.5187927131053328), f[315]);
    y[25] = MUL_F(FRAC_CONST(0.5265773151542700), f[317]);
    y[24] = MUL_F(FRAC_CONST(0.5359098169079920), f[319]);
    y[23] = MUL_F(FRAC_CONST(0.5469204379855088), f[321]);
    y[22] = MUL_F(FRAC_CONST(0.5597698129470802), f[323]);
    y[21] = MUL_F(FRAC_CONST(0.5746551840326600), f[325]);
    y[20] = MUL_F(FRAC_CONST(0.5918185358574165), f[327]);
    y[19] = MUL_F(FRAC_CONST(0.6115573478825099), f[329]);
    y[18] = MUL_F(FRAC_CONST(0.6342389366884031), f[331]);
    y[17] = MUL_F(FRAC_CONST(0.6603198078137061), f[333]);
    y[16] = MUL_F(FRAC_CONST(0.6903721282002123), f[335]);
    y[15] = MUL_F(FRAC_CONST(0.7251205223771985), f[334]);
    y[14] = MUL_F(FRAC_CONST(0.7654941649730891), f[332]);
    y[13] = MUL_F(FRAC_CONST(0.8127020908144905), f[330]);
    y[12] = MUL_F(FRAC_CONST(0.8683447152233481), f[328]);
    y[11] = MUL_F(FRAC_CONST(0.9345835970364075), f[326]);
    y[10] = MUL_C(COEF_CONST(1.0144082649970547), f[324]);
    y[9] = MUL_C(COEF_CONST(1.1120716205797176), f[322]);
    y[8] = MUL_C(COEF_CONST(1.2338327379765710), f[320]);
    y[7] = MUL_C(COEF_CONST(1.3892939586328277), f[318]);
    y[6] = MUL_C(COEF_CONST(1.5939722833856311), f[316]);
    y[5] = MUL_C(COEF_CONST(1.8746759800084078), f[314]);
    y[4] = MUL_C(COEF_CONST(2.2820500680051619), f[312]);
    y[3] = MUL_C(COEF_CONST(2.9246284281582162), f[310]);
    y[2] = MUL_C(COEF_CONST(4.0846110781292477), f[308]);
    y[1] = MUL_C(COEF_CONST(6.7967507116736332), f[306]);
    y[0] = MUL_R(REAL_CONST(20.3738781672314530), f[304]);
    if(f) {
        free(f);
        f = NULL;
    }
}
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
    #ifdef SBR_LOW_POWER
void DCT2_16_unscaled(real_t* y, real_t* x) {
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f31, f32;
    real_t f33, f34, f37, f38, f39, f40, f41, f42, f43, f44;
    real_t f45, f46, f47, f48, f49, f51, f53, f54, f57, f58;
    real_t f59, f60, f61, f62, f63, f64, f65, f66, f67, f68;
    real_t f69, f70, f71, f72, f73, f74, f75, f76, f77, f78;
    real_t f79, f80, f81, f82, f83, f84, f85, f86, f87, f88;
    real_t f89, f90, f91, f92, f95, f96, f97, f98, f101, f102;
    real_t f103, f104, f107, f108, f109, f110;
    f0 = x[0] - x[15];
    f1 = x[0] + x[15];
    f2 = x[1] - x[14];
    f3 = x[1] + x[14];
    f4 = x[2] - x[13];
    f5 = x[2] + x[13];
    f6 = x[3] - x[12];
    f7 = x[3] + x[12];
    f8 = x[4] - x[11];
    f9 = x[4] + x[11];
    f10 = x[5] - x[10];
    f11 = x[5] + x[10];
    f12 = x[6] - x[9];
    f13 = x[6] + x[9];
    f14 = x[7] - x[8];
    f15 = x[7] + x[8];
    f16 = f1 - f15;
    f17 = f1 + f15;
    f18 = f3 - f13;
    f19 = f3 + f13;
    f20 = f5 - f11;
    f21 = f5 + f11;
    f22 = f7 - f9;
    f23 = f7 + f9;
    f24 = f17 - f23;
    f25 = f17 + f23;
    f26 = f19 - f21;
    f27 = f19 + f21;
    f28 = f25 - f27;
    y[0] = f25 + f27;
    y[8] = MUL_F(f28, FRAC_CONST(0.7071067811865476));
    f31 = f24 + f26;
    f32 = MUL_C(f24, COEF_CONST(1.3065629648763766));
    f33 = MUL_F(f31, FRAC_CONST(-0.9238795325112866));
    f34 = MUL_F(f26, FRAC_CONST(-0.5411961001461967));
    y[12] = f32 + f33;
    y[4] = f34 - f33;
    f37 = f16 + f22;
    f38 = MUL_C(f16, COEF_CONST(1.1758756024193588));
    f39 = MUL_F(f37, FRAC_CONST(-0.9807852804032304));
    f40 = MUL_F(f22, FRAC_CONST(-0.7856949583871021));
    f41 = f38 + f39;
    f42 = f40 - f39;
    f43 = f18 + f20;
    f44 = MUL_C(f18, COEF_CONST(1.3870398453221473));
    f45 = MUL_F(f43, FRAC_CONST(-0.8314696123025455));
    f46 = MUL_F(f20, FRAC_CONST(-0.2758993792829436));
    f47 = f44 + f45;
    f48 = f46 - f45;
    f49 = f42 - f48;
    y[2] = f42 + f48;
    f51 = MUL_F(f49, FRAC_CONST(0.7071067811865476));
    y[14] = f41 - f47;
    f53 = f41 + f47;
    f54 = MUL_F(f53, FRAC_CONST(0.7071067811865476));
    y[10] = f51 - f54;
    y[6] = f51 + f54;
    f57 = f2 - f4;
    f58 = f2 + f4;
    f59 = f6 - f8;
    f60 = f6 + f8;
    f61 = f10 - f12;
    f62 = f10 + f12;
    f63 = MUL_F(f60, FRAC_CONST(0.7071067811865476));
    f64 = f0 - f63;
    f65 = f0 + f63;
    f66 = f58 + f62;
    f67 = MUL_C(f58, COEF_CONST(1.3065629648763766));
    f68 = MUL_F(f66, FRAC_CONST(-0.9238795325112866));
    f69 = MUL_F(f62, FRAC_CONST(-0.5411961001461967));
    f70 = f67 + f68;
    f71 = f69 - f68;
    f72 = f65 - f71;
    f73 = f65 + f71;
    f74 = f64 - f70;
    f75 = f64 + f70;
    f76 = MUL_F(f59, FRAC_CONST(0.7071067811865476));
    f77 = f14 - f76;
    f78 = f14 + f76;
    f79 = f61 + f57;
    f80 = MUL_C(f61, COEF_CONST(1.3065629648763766));
    f81 = MUL_F(f79, FRAC_CONST(-0.9238795325112866));
    f82 = MUL_F(f57, FRAC_CONST(-0.5411961001461967));
    f83 = f80 + f81;
    f84 = f82 - f81;
    f85 = f78 - f84;
    f86 = f78 + f84;
    f87 = f77 - f83;
    f88 = f77 + f83;
    f89 = f86 + f73;
    f90 = MUL_F(f86, FRAC_CONST(-0.8971675863426361));
    f91 = MUL_F(f89, FRAC_CONST(0.9951847266721968));
    f92 = MUL_C(f73, COEF_CONST(1.0932018670017576));
    y[1] = f90 + f91;
    y[15] = f92 - f91;
    f95 = f75 - f88;
    f96 = MUL_F(f88, FRAC_CONST(-0.6666556584777466));
    f97 = MUL_F(f95, FRAC_CONST(0.9569403357322089));
    f98 = MUL_C(f75, COEF_CONST(1.2472250129866713));
    y[3] = f97 - f96;
    y[13] = f98 - f97;
    f101 = f87 + f74;
    f102 = MUL_F(f87, FRAC_CONST(-0.4105245275223571));
    f103 = MUL_F(f101, FRAC_CONST(0.8819212643483549));
    f104 = MUL_C(f74, COEF_CONST(1.3533180011743529));
    y[5] = f102 + f103;
    y[11] = f104 - f103;
    f107 = f72 - f85;
    f108 = MUL_F(f85, FRAC_CONST(-0.1386171691990915));
    f109 = MUL_F(f107, FRAC_CONST(0.7730104533627370));
    f110 = MUL_C(f72, COEF_CONST(1.4074037375263826));
    y[7] = f109 - f108;
    y[9] = f110 - f109;
}
    #endif
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
    #ifdef SBR_LOW_POWER
void DCT4_16(real_t* y, real_t* x) {
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f61, f62, f63, f64, f65, f66, f67, f68, f69, f70;
    real_t f71, f72, f73, f74, f75, f76, f77, f78, f79, f80;
    real_t f81, f82, f83, f84, f85, f86, f87, f88, f89, f90;
    real_t f91, f92, f93, f94, f95, f96, f97, f98, f99, f100;
    real_t f101, f102, f103, f104, f105, f106, f107, f108, f109, f110;
    real_t f111, f112, f113, f114, f115, f116, f117, f118, f119, f120;
    real_t f121, f122, f123, f124, f125, f126, f127, f128, f130, f132;
    real_t f134, f136, f138, f140, f142, f144, f145, f148, f149, f152;
    real_t f153, f156, f157;
    f0 = x[0] + x[15];
    f1 = MUL_C(COEF_CONST(1.0478631305325901), x[0]);
    f2 = MUL_F(FRAC_CONST(-0.9987954562051724), f0);
    f3 = MUL_F(FRAC_CONST(-0.9497277818777548), x[15]);
    f4 = f1 + f2;
    f5 = f3 - f2;
    f6 = x[2] + x[13];
    f7 = MUL_C(COEF_CONST(1.2130114330978077), x[2]);
    f8 = MUL_F(FRAC_CONST(-0.9700312531945440), f6);
    f9 = MUL_F(FRAC_CONST(-0.7270510732912803), x[13]);
    f10 = f7 + f8;
    f11 = f9 - f8;
    f12 = x[4] + x[11];
    f13 = MUL_C(COEF_CONST(1.3315443865537255), x[4]);
    f14 = MUL_F(FRAC_CONST(-0.9039892931234433), f12);
    f15 = MUL_F(FRAC_CONST(-0.4764341996931612), x[11]);
    f16 = f13 + f14;
    f17 = f15 - f14;
    f18 = x[6] + x[9];
    f19 = MUL_C(COEF_CONST(1.3989068359730781), x[6]);
    f20 = MUL_F(FRAC_CONST(-0.8032075314806453), f18);
    f21 = MUL_F(FRAC_CONST(-0.2075082269882124), x[9]);
    f22 = f19 + f20;
    f23 = f21 - f20;
    f24 = x[8] + x[7];
    f25 = MUL_C(COEF_CONST(1.4125100802019777), x[8]);
    f26 = MUL_F(FRAC_CONST(-0.6715589548470187), f24);
    f27 = MUL_F(FRAC_CONST(0.0693921705079402), x[7]);
    f28 = f25 + f26;
    f29 = f27 - f26;
    f30 = x[10] + x[5];
    f31 = MUL_C(COEF_CONST(1.3718313541934939), x[10]);
    f32 = MUL_F(FRAC_CONST(-0.5141027441932219), f30);
    f33 = MUL_F(FRAC_CONST(0.3436258658070501), x[5]);
    f34 = f31 + f32;
    f35 = f33 - f32;
    f36 = x[12] + x[3];
    f37 = MUL_C(COEF_CONST(1.2784339185752409), x[12]);
    f38 = MUL_F(FRAC_CONST(-0.3368898533922200), f36);
    f39 = MUL_F(FRAC_CONST(0.6046542117908008), x[3]);
    f40 = f37 + f38;
    f41 = f39 - f38;
    f42 = x[14] + x[1];
    f43 = MUL_C(COEF_CONST(1.1359069844201433), x[14]);
    f44 = MUL_F(FRAC_CONST(-0.1467304744553624), f42);
    f45 = MUL_F(FRAC_CONST(0.8424460355094185), x[1]);
    f46 = f43 + f44;
    f47 = f45 - f44;
    f48 = f5 - f29;
    f49 = f5 + f29;
    f50 = f4 - f28;
    f51 = f4 + f28;
    f52 = f11 - f35;
    f53 = f11 + f35;
    f54 = f10 - f34;
    f55 = f10 + f34;
    f56 = f17 - f41;
    f57 = f17 + f41;
    f58 = f16 - f40;
    f59 = f16 + f40;
    f60 = f23 - f47;
    f61 = f23 + f47;
    f62 = f22 - f46;
    f63 = f22 + f46;
    f64 = f48 + f50;
    f65 = MUL_C(COEF_CONST(1.1758756024193588), f48);
    f66 = MUL_F(FRAC_CONST(-0.9807852804032304), f64);
    f67 = MUL_F(FRAC_CONST(-0.7856949583871021), f50);
    f68 = f65 + f66;
    f69 = f67 - f66;
    f70 = f52 + f54;
    f71 = MUL_C(COEF_CONST(1.3870398453221475), f52);
    f72 = MUL_F(FRAC_CONST(-0.5555702330196022), f70);
    f73 = MUL_F(FRAC_CONST(0.2758993792829431), f54);
    f74 = f71 + f72;
    f75 = f73 - f72;
    f76 = f56 + f58;
    f77 = MUL_F(FRAC_CONST(0.7856949583871022), f56);
    f78 = MUL_F(FRAC_CONST(0.1950903220161283), f76);
    f79 = MUL_C(COEF_CONST(1.1758756024193586), f58);
    f80 = f77 + f78;
    f81 = f79 - f78;
    f82 = f60 + f62;
    f83 = MUL_F(FRAC_CONST(-0.2758993792829430), f60);
    f84 = MUL_F(FRAC_CONST(0.8314696123025452), f82);
    f85 = MUL_C(COEF_CONST(1.3870398453221475), f62);
    f86 = f83 + f84;
    f87 = f85 - f84;
    f88 = f49 - f57;
    f89 = f49 + f57;
    f90 = f51 - f59;
    f91 = f51 + f59;
    f92 = f53 - f61;
    f93 = f53 + f61;
    f94 = f55 - f63;
    f95 = f55 + f63;
    f96 = f69 - f81;
    f97 = f69 + f81;
    f98 = f68 - f80;
    f99 = f68 + f80;
    f100 = f75 - f87;
    f101 = f75 + f87;
    f102 = f74 - f86;
    f103 = f74 + f86;
    f104 = f88 + f90;
    f105 = MUL_C(COEF_CONST(1.3065629648763766), f88);
    f106 = MUL_F(FRAC_CONST(-0.9238795325112866), f104);
    f107 = MUL_F(FRAC_CONST(-0.5411961001461967), f90);
    f108 = f105 + f106;
    f109 = f107 - f106;
    f110 = f92 + f94;
    f111 = MUL_F(FRAC_CONST(0.5411961001461969), f92);
    f112 = MUL_F(FRAC_CONST(0.3826834323650898), f110);
    f113 = MUL_C(COEF_CONST(1.3065629648763766), f94);
    f114 = f111 + f112;
    f115 = f113 - f112;
    f116 = f96 + f98;
    f117 = MUL_C(COEF_CONST(1.3065629648763766), f96);
    f118 = MUL_F(FRAC_CONST(-0.9238795325112866), f116);
    f119 = MUL_F(FRAC_CONST(-0.5411961001461967), f98);
    f120 = f117 + f118;
    f121 = f119 - f118;
    f122 = f100 + f102;
    f123 = MUL_F(FRAC_CONST(0.5411961001461969), f100);
    f124 = MUL_F(FRAC_CONST(0.3826834323650898), f122);
    f125 = MUL_C(COEF_CONST(1.3065629648763766), f102);
    f126 = f123 + f124;
    f127 = f125 - f124;
    f128 = f89 - f93;
    y[0] = f89 + f93;
    f130 = f91 - f95;
    y[15] = f91 + f95;
    f132 = f109 - f115;
    y[3] = f109 + f115;
    f134 = f108 - f114;
    y[12] = f108 + f114;
    f136 = f97 - f101;
    y[1] = f97 + f101;
    f138 = f99 - f103;
    y[14] = f99 + f103;
    f140 = f121 - f127;
    y[2] = f121 + f127;
    f142 = f120 - f126;
    y[13] = f120 + f126;
    f144 = f128 - f130;
    f145 = f128 + f130;
    y[8] = MUL_F(FRAC_CONST(0.7071067811865474), f144);
    y[7] = MUL_F(FRAC_CONST(0.7071067811865474), f145);
    f148 = f132 - f134;
    f149 = f132 + f134;
    y[11] = MUL_F(FRAC_CONST(0.7071067811865474), f148);
    y[4] = MUL_F(FRAC_CONST(0.7071067811865474), f149);
    f152 = f136 - f138;
    f153 = f136 + f138;
    y[9] = MUL_F(FRAC_CONST(0.7071067811865474), f152);
    y[6] = MUL_F(FRAC_CONST(0.7071067811865474), f153);
    f156 = f140 - f142;
    f157 = f140 + f142;
    y[10] = MUL_F(FRAC_CONST(0.7071067811865474), f156);
    y[5] = MUL_F(FRAC_CONST(0.7071067811865474), f157);
}
    #endif
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
    #ifdef SBR_LOW_POWER
void DCT3_32_unscaled(real_t* y, real_t* x) {
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f61, f62, f63, f64, f65, f66, f67, f68, f69, f70;
    real_t f71, f72, f73, f74, f75, f76, f77, f78, f79, f80;
    real_t f81, f82, f83, f84, f85, f86, f87, f88, f89, f90;
    real_t f91, f92, f93, f94, f95, f96, f97, f98, f99, f100;
    real_t f101, f102, f103, f104, f105, f106, f107, f108, f109, f110;
    real_t f111, f112, f113, f114, f115, f116, f117, f118, f119, f120;
    real_t f121, f122, f123, f124, f125, f126, f127, f128, f129, f130;
    real_t f131, f132, f133, f134, f135, f136, f137, f138, f139, f140;
    real_t f141, f142, f143, f144, f145, f146, f147, f148, f149, f150;
    real_t f151, f152, f153, f154, f155, f156, f157, f158, f159, f160;
    real_t f161, f162, f163, f164, f165, f166, f167, f168, f169, f170;
    real_t f171, f172, f173, f174, f175, f176, f177, f178, f179, f180;
    real_t f181, f182, f183, f184, f185, f186, f187, f188, f189, f190;
    real_t f191, f192, f193, f194, f195, f196, f197, f198, f199, f200;
    real_t f201, f202, f203, f204, f205, f206, f207, f208, f209, f210;
    real_t f211, f212, f213, f214, f215, f216, f217, f218, f219, f220;
    real_t f221, f222, f223, f224, f225, f226, f227, f228, f229, f230;
    real_t f231, f232, f233, f234, f235, f236, f237, f238, f239, f240;
    real_t f241, f242, f243, f244, f245, f246, f247, f248, f249, f250;
    real_t f251, f252, f253, f254, f255, f256, f257, f258, f259, f260;
    real_t f261, f262, f263, f264, f265, f266, f267, f268, f269, f270;
    real_t f271, f272;
    f0 = MUL_F(x[16], FRAC_CONST(0.7071067811865476));
    f1 = x[0] - f0;
    f2 = x[0] + f0;
    f3 = x[8] + x[24];
    f4 = MUL_C(x[8], COEF_CONST(1.3065629648763766));
    f5 = MUL_F(f3, FRAC_CONST((-0.9238795325112866)));
    f6 = MUL_F(x[24], FRAC_CONST((-0.5411961001461967)));
    f7 = f4 + f5;
    f8 = f6 - f5;
    f9 = f2 - f8;
    f10 = f2 + f8;
    f11 = f1 - f7;
    f12 = f1 + f7;
    f13 = x[4] + x[28];
    f14 = MUL_C(x[4], COEF_CONST(1.1758756024193588));
    f15 = MUL_F(f13, FRAC_CONST((-0.9807852804032304)));
    f16 = MUL_F(x[28], FRAC_CONST((-0.7856949583871021)));
    f17 = f14 + f15;
    f18 = f16 - f15;
    f19 = x[12] + x[20];
    f20 = MUL_C(x[12], COEF_CONST(1.3870398453221473));
    f21 = MUL_F(f19, FRAC_CONST((-0.8314696123025455)));
    f22 = MUL_F(x[20], FRAC_CONST((-0.2758993792829436)));
    f23 = f20 + f21;
    f24 = f22 - f21;
    f25 = f18 - f24;
    f26 = f18 + f24;
    f27 = MUL_F(f25, FRAC_CONST(0.7071067811865476));
    f28 = f17 - f23;
    f29 = f17 + f23;
    f30 = MUL_F(f29, FRAC_CONST(0.7071067811865476));
    f31 = f27 - f30;
    f32 = f27 + f30;
    f33 = f10 - f26;
    f34 = f10 + f26;
    f35 = f12 - f32;
    f36 = f12 + f32;
    f37 = f11 - f31;
    f38 = f11 + f31;
    f39 = f9 - f28;
    f40 = f9 + f28;
    f41 = x[2] + x[30];
    f42 = MUL_C(x[2], COEF_CONST(1.0932018670017569));
    f43 = MUL_F(f41, FRAC_CONST((-0.9951847266721969)));
    f44 = MUL_F(x[30], FRAC_CONST((-0.8971675863426368)));
    f45 = f42 + f43;
    f46 = f44 - f43;
    f47 = x[6] + x[26];
    f48 = MUL_C(x[6], COEF_CONST(1.2472250129866711));
    f49 = MUL_F(f47, FRAC_CONST((-0.9569403357322089)));
    f50 = MUL_F(x[26], FRAC_CONST((-0.6666556584777469)));
    f51 = f48 + f49;
    f52 = f50 - f49;
    f53 = x[10] + x[22];
    f54 = MUL_C(x[10], COEF_CONST(1.3533180011743526));
    f55 = MUL_F(f53, FRAC_CONST((-0.8819212643483551)));
    f56 = MUL_F(x[22], FRAC_CONST((-0.4105245275223575)));
    f57 = f54 + f55;
    f58 = f56 - f55;
    f59 = x[14] + x[18];
    f60 = MUL_C(x[14], COEF_CONST(1.4074037375263826));
    f61 = MUL_F(f59, FRAC_CONST((-0.7730104533627369)));
    f62 = MUL_F(x[18], FRAC_CONST((-0.1386171691990913)));
    f63 = f60 + f61;
    f64 = f62 - f61;
    f65 = f46 - f64;
    f66 = f46 + f64;
    f67 = f52 - f58;
    f68 = f52 + f58;
    f69 = f66 - f68;
    f70 = f66 + f68;
    f71 = MUL_F(f69, FRAC_CONST(0.7071067811865476));
    f72 = f65 + f67;
    f73 = MUL_C(f65, COEF_CONST(1.3065629648763766));
    f74 = MUL_F(f72, FRAC_CONST((-0.9238795325112866)));
    f75 = MUL_F(f67, FRAC_CONST((-0.5411961001461967)));
    f76 = f73 + f74;
    f77 = f75 - f74;
    f78 = f45 - f63;
    f79 = f45 + f63;
    f80 = f51 - f57;
    f81 = f51 + f57;
    f82 = f79 + f81;
    f83 = MUL_C(f79, COEF_CONST(1.3065629648763770));
    f84 = MUL_F(f82, FRAC_CONST((-0.3826834323650904)));
    f85 = MUL_F(f81, FRAC_CONST(0.5411961001461961));
    f86 = f83 + f84;
    f87 = f85 - f84;
    f88 = f78 - f80;
    f89 = f78 + f80;
    f90 = MUL_F(f89, FRAC_CONST(0.7071067811865476));
    f91 = f77 - f87;
    f92 = f77 + f87;
    f93 = f71 - f90;
    f94 = f71 + f90;
    f95 = f76 - f86;
    f96 = f76 + f86;
    f97 = f34 - f70;
    f98 = f34 + f70;
    f99 = f36 - f92;
    f100 = f36 + f92;
    f101 = f38 - f91;
    f102 = f38 + f91;
    f103 = f40 - f94;
    f104 = f40 + f94;
    f105 = f39 - f93;
    f106 = f39 + f93;
    f107 = f37 - f96;
    f108 = f37 + f96;
    f109 = f35 - f95;
    f110 = f35 + f95;
    f111 = f33 - f88;
    f112 = f33 + f88;
    f113 = x[1] + x[31];
    f114 = MUL_C(x[1], COEF_CONST(1.0478631305325901));
    f115 = MUL_F(f113, FRAC_CONST((-0.9987954562051724)));
    f116 = MUL_F(x[31], FRAC_CONST((-0.9497277818777548)));
    f117 = f114 + f115;
    f118 = f116 - f115;
    f119 = x[5] + x[27];
    f120 = MUL_C(x[5], COEF_CONST(1.2130114330978077));
    f121 = MUL_F(f119, FRAC_CONST((-0.9700312531945440)));
    f122 = MUL_F(x[27], FRAC_CONST((-0.7270510732912803)));
    f123 = f120 + f121;
    f124 = f122 - f121;
    f125 = x[9] + x[23];
    f126 = MUL_C(x[9], COEF_CONST(1.3315443865537255));
    f127 = MUL_F(f125, FRAC_CONST((-0.9039892931234433)));
    f128 = MUL_F(x[23], FRAC_CONST((-0.4764341996931612)));
    f129 = f126 + f127;
    f130 = f128 - f127;
    f131 = x[13] + x[19];
    f132 = MUL_C(x[13], COEF_CONST(1.3989068359730781));
    f133 = MUL_F(f131, FRAC_CONST((-0.8032075314806453)));
    f134 = MUL_F(x[19], FRAC_CONST((-0.2075082269882124)));
    f135 = f132 + f133;
    f136 = f134 - f133;
    f137 = x[17] + x[15];
    f138 = MUL_C(x[17], COEF_CONST(1.4125100802019777));
    f139 = MUL_F(f137, FRAC_CONST((-0.6715589548470187)));
    f140 = MUL_F(x[15], FRAC_CONST(0.0693921705079402));
    f141 = f138 + f139;
    f142 = f140 - f139;
    f143 = x[21] + x[11];
    f144 = MUL_C(x[21], COEF_CONST(1.3718313541934939));
    f145 = MUL_F(f143, FRAC_CONST((-0.5141027441932219)));
    f146 = MUL_F(x[11], FRAC_CONST(0.3436258658070501));
    f147 = f144 + f145;
    f148 = f146 - f145;
    f149 = x[25] + x[7];
    f150 = MUL_C(x[25], COEF_CONST(1.2784339185752409));
    f151 = MUL_F(f149, FRAC_CONST((-0.3368898533922200)));
    f152 = MUL_F(x[7], FRAC_CONST(0.6046542117908008));
    f153 = f150 + f151;
    f154 = f152 - f151;
    f155 = x[29] + x[3];
    f156 = MUL_C(x[29], COEF_CONST(1.1359069844201433));
    f157 = MUL_F(f155, FRAC_CONST((-0.1467304744553624)));
    f158 = MUL_F(x[3], FRAC_CONST(0.8424460355094185));
    f159 = f156 + f157;
    f160 = f158 - f157;
    f161 = f118 - f142;
    f162 = f118 + f142;
    f163 = f117 - f141;
    f164 = f117 + f141;
    f165 = f124 - f148;
    f166 = f124 + f148;
    f167 = f123 - f147;
    f168 = f123 + f147;
    f169 = f130 - f154;
    f170 = f130 + f154;
    f171 = f129 - f153;
    f172 = f129 + f153;
    f173 = f136 - f160;
    f174 = f136 + f160;
    f175 = f135 - f159;
    f176 = f135 + f159;
    f177 = f161 + f163;
    f178 = MUL_C(f161, COEF_CONST(1.1758756024193588));
    f179 = MUL_F(f177, FRAC_CONST((-0.9807852804032304)));
    f180 = MUL_F(f163, FRAC_CONST((-0.7856949583871021)));
    f181 = f178 + f179;
    f182 = f180 - f179;
    f183 = f165 + f167;
    f184 = MUL_C(f165, COEF_CONST(1.3870398453221475));
    f185 = MUL_F(f183, FRAC_CONST((-0.5555702330196022)));
    f186 = MUL_F(f167, FRAC_CONST(0.2758993792829431));
    f187 = f184 + f185;
    f188 = f186 - f185;
    f189 = f169 + f171;
    f190 = MUL_F(f169, FRAC_CONST(0.7856949583871022));
    f191 = MUL_F(f189, FRAC_CONST(0.1950903220161283));
    f192 = MUL_C(f171, COEF_CONST(1.1758756024193586));
    f193 = f190 + f191;
    f194 = f192 - f191;
    f195 = f173 + f175;
    f196 = MUL_F(f173, FRAC_CONST((-0.2758993792829430)));
    f197 = MUL_F(f195, FRAC_CONST(0.8314696123025452));
    f198 = MUL_C(f175, COEF_CONST(1.3870398453221475));
    f199 = f196 + f197;
    f200 = f198 - f197;
    f201 = f162 - f170;
    f202 = f162 + f170;
    f203 = f164 - f172;
    f204 = f164 + f172;
    f205 = f166 - f174;
    f206 = f166 + f174;
    f207 = f168 - f176;
    f208 = f168 + f176;
    f209 = f182 - f194;
    f210 = f182 + f194;
    f211 = f181 - f193;
    f212 = f181 + f193;
    f213 = f188 - f200;
    f214 = f188 + f200;
    f215 = f187 - f199;
    f216 = f187 + f199;
    f217 = f201 + f203;
    f218 = MUL_C(f201, COEF_CONST(1.3065629648763766));
    f219 = MUL_F(f217, FRAC_CONST((-0.9238795325112866)));
    f220 = MUL_F(f203, FRAC_CONST((-0.5411961001461967)));
    f221 = f218 + f219;
    f222 = f220 - f219;
    f223 = f205 + f207;
    f224 = MUL_F(f205, FRAC_CONST(0.5411961001461969));
    f225 = MUL_F(f223, FRAC_CONST(0.3826834323650898));
    f226 = MUL_C(f207, COEF_CONST(1.3065629648763766));
    f227 = f224 + f225;
    f228 = f226 - f225;
    f229 = f209 + f211;
    f230 = MUL_C(f209, COEF_CONST(1.3065629648763766));
    f231 = MUL_F(f229, FRAC_CONST((-0.9238795325112866)));
    f232 = MUL_F(f211, FRAC_CONST((-0.5411961001461967)));
    f233 = f230 + f231;
    f234 = f232 - f231;
    f235 = f213 + f215;
    f236 = MUL_F(f213, FRAC_CONST(0.5411961001461969));
    f237 = MUL_F(f235, FRAC_CONST(0.3826834323650898));
    f238 = MUL_C(f215, COEF_CONST(1.3065629648763766));
    f239 = f236 + f237;
    f240 = f238 - f237;
    f241 = f202 - f206;
    f242 = f202 + f206;
    f243 = f204 - f208;
    f244 = f204 + f208;
    f245 = f222 - f228;
    f246 = f222 + f228;
    f247 = f221 - f227;
    f248 = f221 + f227;
    f249 = f210 - f214;
    f250 = f210 + f214;
    f251 = f212 - f216;
    f252 = f212 + f216;
    f253 = f234 - f240;
    f254 = f234 + f240;
    f255 = f233 - f239;
    f256 = f233 + f239;
    f257 = f241 - f243;
    f258 = f241 + f243;
    f259 = MUL_F(f257, FRAC_CONST(0.7071067811865474));
    f260 = MUL_F(f258, FRAC_CONST(0.7071067811865474));
    f261 = f245 - f247;
    f262 = f245 + f247;
    f263 = MUL_F(f261, FRAC_CONST(0.7071067811865474));
    f264 = MUL_F(f262, FRAC_CONST(0.7071067811865474));
    f265 = f249 - f251;
    f266 = f249 + f251;
    f267 = MUL_F(f265, FRAC_CONST(0.7071067811865474));
    f268 = MUL_F(f266, FRAC_CONST(0.7071067811865474));
    f269 = f253 - f255;
    f270 = f253 + f255;
    f271 = MUL_F(f269, FRAC_CONST(0.7071067811865474));
    f272 = MUL_F(f270, FRAC_CONST(0.7071067811865474));
    y[31] = f98 - f242;
    y[0] = f98 + f242;
    y[30] = f100 - f250;
    y[1] = f100 + f250;
    y[29] = f102 - f254;
    y[2] = f102 + f254;
    y[28] = f104 - f246;
    y[3] = f104 + f246;
    y[27] = f106 - f264;
    y[4] = f106 + f264;
    y[26] = f108 - f272;
    y[5] = f108 + f272;
    y[25] = f110 - f268;
    y[6] = f110 + f268;
    y[24] = f112 - f260;
    y[7] = f112 + f260;
    y[23] = f111 - f259;
    y[8] = f111 + f259;
    y[22] = f109 - f267;
    y[9] = f109 + f267;
    y[21] = f107 - f271;
    y[10] = f107 + f271;
    y[20] = f105 - f263;
    y[11] = f105 + f263;
    y[19] = f103 - f248;
    y[12] = f103 + f248;
    y[18] = f101 - f256;
    y[13] = f101 + f256;
    y[17] = f99 - f252;
    y[14] = f99 + f252;
    y[16] = f97 - f244;
    y[15] = f97 + f244;
}
    #endif
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
    #ifdef SBR_LOW_POWER
void DCT2_32_unscaled(real_t* y, real_t* x) {
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f63, f64, f65, f66, f69, f70, f71, f72, f73, f74;
    real_t f75, f76, f77, f78, f79, f80, f81, f83, f85, f86;
    real_t f89, f90, f91, f92, f93, f94, f95, f96, f97, f98;
    real_t f99, f100, f101, f102, f103, f104, f105, f106, f107, f108;
    real_t f109, f110, f111, f112, f113, f114, f115, f116, f117, f118;
    real_t f119, f120, f121, f122, f123, f124, f127, f128, f129, f130;
    real_t f133, f134, f135, f136, f139, f140, f141, f142, f145, f146;
    real_t f147, f148, f149, f150, f151, f152, f153, f154, f155, f156;
    real_t f157, f158, f159, f160, f161, f162, f163, f164, f165, f166;
    real_t f167, f168, f169, f170, f171, f172, f173, f174, f175, f176;
    real_t f177, f178, f179, f180, f181, f182, f183, f184, f185, f186;
    real_t f187, f188, f189, f190, f191, f192, f193, f194, f195, f196;
    real_t f197, f198, f199, f200, f201, f202, f203, f204, f205, f206;
    real_t f207, f208, f209, f210, f211, f212, f213, f214, f215, f216;
    real_t f217, f218, f219, f220, f221, f222, f223, f224, f225, f226;
    real_t f227, f228, f229, f230, f231, f232, f233, f234, f235, f236;
    real_t f237, f238, f239, f240, f241, f242, f243, f244, f247, f248;
    real_t f249, f250, f253, f254, f255, f256, f259, f260, f261, f262;
    real_t f265, f266, f267, f268, f271, f272, f273, f274, f277, f278;
    real_t f279, f280, f283, f284, f285, f286;
    f0 = x[0] - x[31];
    f1 = x[0] + x[31];
    f2 = x[1] - x[30];
    f3 = x[1] + x[30];
    f4 = x[2] - x[29];
    f5 = x[2] + x[29];
    f6 = x[3] - x[28];
    f7 = x[3] + x[28];
    f8 = x[4] - x[27];
    f9 = x[4] + x[27];
    f10 = x[5] - x[26];
    f11 = x[5] + x[26];
    f12 = x[6] - x[25];
    f13 = x[6] + x[25];
    f14 = x[7] - x[24];
    f15 = x[7] + x[24];
    f16 = x[8] - x[23];
    f17 = x[8] + x[23];
    f18 = x[9] - x[22];
    f19 = x[9] + x[22];
    f20 = x[10] - x[21];
    f21 = x[10] + x[21];
    f22 = x[11] - x[20];
    f23 = x[11] + x[20];
    f24 = x[12] - x[19];
    f25 = x[12] + x[19];
    f26 = x[13] - x[18];
    f27 = x[13] + x[18];
    f28 = x[14] - x[17];
    f29 = x[14] + x[17];
    f30 = x[15] - x[16];
    f31 = x[15] + x[16];
    f32 = f1 - f31;
    f33 = f1 + f31;
    f34 = f3 - f29;
    f35 = f3 + f29;
    f36 = f5 - f27;
    f37 = f5 + f27;
    f38 = f7 - f25;
    f39 = f7 + f25;
    f40 = f9 - f23;
    f41 = f9 + f23;
    f42 = f11 - f21;
    f43 = f11 + f21;
    f44 = f13 - f19;
    f45 = f13 + f19;
    f46 = f15 - f17;
    f47 = f15 + f17;
    f48 = f33 - f47;
    f49 = f33 + f47;
    f50 = f35 - f45;
    f51 = f35 + f45;
    f52 = f37 - f43;
    f53 = f37 + f43;
    f54 = f39 - f41;
    f55 = f39 + f41;
    f56 = f49 - f55;
    f57 = f49 + f55;
    f58 = f51 - f53;
    f59 = f51 + f53;
    f60 = f57 - f59;
    y[0] = f57 + f59;
    y[16] = MUL_F(FRAC_CONST(0.7071067811865476), f60);
    f63 = f56 + f58;
    f64 = MUL_C(COEF_CONST(1.3065629648763766), f56);
    f65 = MUL_F(FRAC_CONST(-0.9238795325112866), f63);
    f66 = MUL_F(FRAC_CONST(-0.5411961001461967), f58);
    y[24] = f64 + f65;
    y[8] = f66 - f65;
    f69 = f48 + f54;
    f70 = MUL_C(COEF_CONST(1.1758756024193588), f48);
    f71 = MUL_F(FRAC_CONST(-0.9807852804032304), f69);
    f72 = MUL_F(FRAC_CONST(-0.7856949583871021), f54);
    f73 = f70 + f71;
    f74 = f72 - f71;
    f75 = f50 + f52;
    f76 = MUL_C(COEF_CONST(1.3870398453221473), f50);
    f77 = MUL_F(FRAC_CONST(-0.8314696123025455), f75);
    f78 = MUL_F(FRAC_CONST(-0.2758993792829436), f52);
    f79 = f76 + f77;
    f80 = f78 - f77;
    f81 = f74 - f80;
    y[4] = f74 + f80;
    f83 = MUL_F(FRAC_CONST(0.7071067811865476), f81);
    y[28] = f73 - f79;
    f85 = f73 + f79;
    f86 = MUL_F(FRAC_CONST(0.7071067811865476), f85);
    y[20] = f83 - f86;
    y[12] = f83 + f86;
    f89 = f34 - f36;
    f90 = f34 + f36;
    f91 = f38 - f40;
    f92 = f38 + f40;
    f93 = f42 - f44;
    f94 = f42 + f44;
    f95 = MUL_F(FRAC_CONST(0.7071067811865476), f92);
    f96 = f32 - f95;
    f97 = f32 + f95;
    f98 = f90 + f94;
    f99 = MUL_C(COEF_CONST(1.3065629648763766), f90);
    f100 = MUL_F(FRAC_CONST(-0.9238795325112866), f98);
    f101 = MUL_F(FRAC_CONST(-0.5411961001461967), f94);
    f102 = f99 + f100;
    f103 = f101 - f100;
    f104 = f97 - f103;
    f105 = f97 + f103;
    f106 = f96 - f102;
    f107 = f96 + f102;
    f108 = MUL_F(FRAC_CONST(0.7071067811865476), f91);
    f109 = f46 - f108;
    f110 = f46 + f108;
    f111 = f93 + f89;
    f112 = MUL_C(COEF_CONST(1.3065629648763766), f93);
    f113 = MUL_F(FRAC_CONST(-0.9238795325112866), f111);
    f114 = MUL_F(FRAC_CONST(-0.5411961001461967), f89);
    f115 = f112 + f113;
    f116 = f114 - f113;
    f117 = f110 - f116;
    f118 = f110 + f116;
    f119 = f109 - f115;
    f120 = f109 + f115;
    f121 = f118 + f105;
    f122 = MUL_F(FRAC_CONST(-0.8971675863426361), f118);
    f123 = MUL_F(FRAC_CONST(0.9951847266721968), f121);
    f124 = MUL_C(COEF_CONST(1.0932018670017576), f105);
    y[2] = f122 + f123;
    y[30] = f124 - f123;
    f127 = f107 - f120;
    f128 = MUL_F(FRAC_CONST(-0.6666556584777466), f120);
    f129 = MUL_F(FRAC_CONST(0.9569403357322089), f127);
    f130 = MUL_C(COEF_CONST(1.2472250129866713), f107);
    y[6] = f129 - f128;
    y[26] = f130 - f129;
    f133 = f119 + f106;
    f134 = MUL_F(FRAC_CONST(-0.4105245275223571), f119);
    f135 = MUL_F(FRAC_CONST(0.8819212643483549), f133);
    f136 = MUL_C(COEF_CONST(1.3533180011743529), f106);
    y[10] = f134 + f135;
    y[22] = f136 - f135;
    f139 = f104 - f117;
    f140 = MUL_F(FRAC_CONST(-0.1386171691990915), f117);
    f141 = MUL_F(FRAC_CONST(0.7730104533627370), f139);
    f142 = MUL_C(COEF_CONST(1.4074037375263826), f104);
    y[14] = f141 - f140;
    y[18] = f142 - f141;
    f145 = f2 - f4;
    f146 = f2 + f4;
    f147 = f6 - f8;
    f148 = f6 + f8;
    f149 = f10 - f12;
    f150 = f10 + f12;
    f151 = f14 - f16;
    f152 = f14 + f16;
    f153 = f18 - f20;
    f154 = f18 + f20;
    f155 = f22 - f24;
    f156 = f22 + f24;
    f157 = f26 - f28;
    f158 = f26 + f28;
    f159 = MUL_F(FRAC_CONST(0.7071067811865476), f152);
    f160 = f0 - f159;
    f161 = f0 + f159;
    f162 = f148 + f156;
    f163 = MUL_C(COEF_CONST(1.3065629648763766), f148);
    f164 = MUL_F(FRAC_CONST(-0.9238795325112866), f162);
    f165 = MUL_F(FRAC_CONST(-0.5411961001461967), f156);
    f166 = f163 + f164;
    f167 = f165 - f164;
    f168 = f161 - f167;
    f169 = f161 + f167;
    f170 = f160 - f166;
    f171 = f160 + f166;
    f172 = f146 + f158;
    f173 = MUL_C(COEF_CONST(1.1758756024193588), f146);
    f174 = MUL_F(FRAC_CONST(-0.9807852804032304), f172);
    f175 = MUL_F(FRAC_CONST(-0.7856949583871021), f158);
    f176 = f173 + f174;
    f177 = f175 - f174;
    f178 = f150 + f154;
    f179 = MUL_C(COEF_CONST(1.3870398453221473), f150);
    f180 = MUL_F(FRAC_CONST(-0.8314696123025455), f178);
    f181 = MUL_F(FRAC_CONST(-0.2758993792829436), f154);
    f182 = f179 + f180;
    f183 = f181 - f180;
    f184 = f177 - f183;
    f185 = f177 + f183;
    f186 = MUL_F(FRAC_CONST(0.7071067811865476), f184);
    f187 = f176 - f182;
    f188 = f176 + f182;
    f189 = MUL_F(FRAC_CONST(0.7071067811865476), f188);
    f190 = f186 - f189;
    f191 = f186 + f189;
    f192 = f169 - f185;
    f193 = f169 + f185;
    f194 = f171 - f191;
    f195 = f171 + f191;
    f196 = f170 - f190;
    f197 = f170 + f190;
    f198 = f168 - f187;
    f199 = f168 + f187;
    f200 = MUL_F(FRAC_CONST(0.7071067811865476), f151);
    f201 = f30 - f200;
    f202 = f30 + f200;
    f203 = f155 + f147;
    f204 = MUL_C(COEF_CONST(1.3065629648763766), f155);
    f205 = MUL_F(FRAC_CONST(-0.9238795325112866), f203);
    f206 = MUL_F(FRAC_CONST(-0.5411961001461967), f147);
    f207 = f204 + f205;
    f208 = f206 - f205;
    f209 = f202 - f208;
    f210 = f202 + f208;
    f211 = f201 - f207;
    f212 = f201 + f207;
    f213 = f157 + f145;
    f214 = MUL_C(COEF_CONST(1.1758756024193588), f157);
    f215 = MUL_F(FRAC_CONST(-0.9807852804032304), f213);
    f216 = MUL_F(FRAC_CONST(-0.7856949583871021), f145);
    f217 = f214 + f215;
    f218 = f216 - f215;
    f219 = f153 + f149;
    f220 = MUL_C(COEF_CONST(1.3870398453221473), f153);
    f221 = MUL_F(FRAC_CONST(-0.8314696123025455), f219);
    f222 = MUL_F(FRAC_CONST(-0.2758993792829436), f149);
    f223 = f220 + f221;
    f224 = f222 - f221;
    f225 = f218 - f224;
    f226 = f218 + f224;
    f227 = MUL_F(FRAC_CONST(0.7071067811865476), f225);
    f228 = f217 - f223;
    f229 = f217 + f223;
    f230 = MUL_F(FRAC_CONST(0.7071067811865476), f229);
    f231 = f227 - f230;
    f232 = f227 + f230;
    f233 = f210 - f226;
    f234 = f210 + f226;
    f235 = f212 - f232;
    f236 = f212 + f232;
    f237 = f211 - f231;
    f238 = f211 + f231;
    f239 = f209 - f228;
    f240 = f209 + f228;
    f241 = f234 + f193;
    f242 = MUL_F(FRAC_CONST(-0.9497277818777543), f234);
    f243 = MUL_F(FRAC_CONST(0.9987954562051724), f241);
    f244 = MUL_C(COEF_CONST(1.0478631305325905), f193);
    y[1] = f242 + f243;
    y[31] = f244 - f243;
    f247 = f195 - f236;
    f248 = MUL_F(FRAC_CONST(-0.8424460355094192), f236);
    f249 = MUL_F(FRAC_CONST(0.9891765099647810), f247);
    f250 = MUL_C(COEF_CONST(1.1359069844201428), f195);
    y[3] = f249 - f248;
    y[29] = f250 - f249;
    f253 = f238 + f197;
    f254 = MUL_F(FRAC_CONST(-0.7270510732912801), f238);
    f255 = MUL_F(FRAC_CONST(0.9700312531945440), f253);
    f256 = MUL_C(COEF_CONST(1.2130114330978079), f197);
    y[5] = f254 + f255;
    y[27] = f256 - f255;
    f259 = f199 - f240;
    f260 = MUL_F(FRAC_CONST(-0.6046542117908007), f240);
    f261 = MUL_F(FRAC_CONST(0.9415440651830208), f259);
    f262 = MUL_C(COEF_CONST(1.2784339185752409), f199);
    y[7] = f261 - f260;
    y[25] = f262 - f261;
    f265 = f239 + f198;
    f266 = MUL_F(FRAC_CONST(-0.4764341996931611), f239);
    f267 = MUL_F(FRAC_CONST(0.9039892931234433), f265);
    f268 = MUL_C(COEF_CONST(1.3315443865537255), f198);
    y[9] = f266 + f267;
    y[23] = f268 - f267;
    f271 = f196 - f237;
    f272 = MUL_F(FRAC_CONST(-0.3436258658070505), f237);
    f273 = MUL_F(FRAC_CONST(0.8577286100002721), f271);
    f274 = MUL_C(COEF_CONST(1.3718313541934939), f196);
    y[11] = f273 - f272;
    y[21] = f274 - f273;
    f277 = f235 + f194;
    f278 = MUL_F(FRAC_CONST(-0.2075082269882114), f235);
    f279 = MUL_F(FRAC_CONST(0.8032075314806448), f277);
    f280 = MUL_C(COEF_CONST(1.3989068359730783), f194);
    y[13] = f278 + f279;
    y[19] = f280 - f279;
    f283 = f192 - f233;
    f284 = MUL_F(FRAC_CONST(-0.0693921705079408), f233);
    f285 = MUL_F(FRAC_CONST(0.7409511253549591), f283);
    f286 = MUL_C(COEF_CONST(1.4125100802019774), f192);
    y[15] = f285 - f284;
    y[17] = f286 - f285;
}
    #endif // SBR_LOW_POWER
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
    #ifndef SBR_LOW_POWER
        #define n     32
        #define log2n 5
// w_array_real[i] = cos(2*M_PI*i/32)
static const real_t w_array_real[] = {FRAC_CONST(1.000000000000000),  FRAC_CONST(0.980785279337272),  FRAC_CONST(0.923879528329380),  FRAC_CONST(0.831469603195765),
                                      FRAC_CONST(0.707106765732237),  FRAC_CONST(0.555570210304169),  FRAC_CONST(0.382683402077046),  FRAC_CONST(0.195090284503576),
                                      FRAC_CONST(0.000000000000000),  FRAC_CONST(-0.195090370246552), FRAC_CONST(-0.382683482845162), FRAC_CONST(-0.555570282993553),
                                      FRAC_CONST(-0.707106827549476), FRAC_CONST(-0.831469651765257), FRAC_CONST(-0.923879561784627), FRAC_CONST(-0.980785296392607)};
// w_array_imag[i] = sin(-2*M_PI*i/32)
static const real_t w_array_imag[] = {FRAC_CONST(0.000000000000000),  FRAC_CONST(-0.195090327375064), FRAC_CONST(-0.382683442461104), FRAC_CONST(-0.555570246648862),
                                      FRAC_CONST(-0.707106796640858), FRAC_CONST(-0.831469627480512), FRAC_CONST(-0.923879545057005), FRAC_CONST(-0.980785287864940),
                                      FRAC_CONST(-1.000000000000000), FRAC_CONST(-0.980785270809601), FRAC_CONST(-0.923879511601754), FRAC_CONST(-0.831469578911016),
                                      FRAC_CONST(-0.707106734823616), FRAC_CONST(-0.555570173959476), FRAC_CONST(-0.382683361692986), FRAC_CONST(-0.195090241632088)};
// FFT decimation in frequency
// 4*16*2+16=128+16=144 multiplications
// 6*16*2+10*8+4*16*2=192+80+128=400 additions
    #endif // SBR_LOW_POWER
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
    #ifndef SBR_LOW_POWER
static void fft_dif(real_t* Real, real_t* Imag) {
    real_t   w_real, w_imag;                                     // For faster access
    real_t   point1_real, point1_imag, point2_real, point2_imag; // For faster access
    uint32_t j, i, i2, w_index;                                  // Counters
    // First 2 stages of 32 point FFT decimation in frequency
    // 4*16*2=64*2=128 multiplications
    // 6*16*2=96*2=192 additions
    // Stage 1 of 32 point FFT decimation in frequency
    for (i = 0; i < 16; i++) {
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i + 16;
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        w_real = w_array_real[i];
        w_imag = w_array_imag[i];
        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;
        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = (MUL_F(point1_real, w_real) - MUL_F(point1_imag, w_imag));
        Imag[i2] = (MUL_F(point1_real, w_imag) + MUL_F(point1_imag, w_real));
    }
    // Stage 2 of 32 point FFT decimation in frequency
    for (j = 0, w_index = 0; j < 8; j++, w_index += 2) {
        w_real = w_array_real[w_index];
        w_imag = w_array_imag[w_index];
        i = j;
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i + 8;
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;
        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = (MUL_F(point1_real, w_real) - MUL_F(point1_imag, w_imag));
        Imag[i2] = (MUL_F(point1_real, w_imag) + MUL_F(point1_imag, w_real));
        i = j + 16;
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i + 8;
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;
        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = (MUL_F(point1_real, w_real) - MUL_F(point1_imag, w_imag));
        Imag[i2] = (MUL_F(point1_real, w_imag) + MUL_F(point1_imag, w_real));
    }
    // Stage 3 of 32 point FFT decimation in frequency
    // 2*4*2=16 multiplications
    // 4*4*2+6*4*2=10*8=80 additions
    for (i = 0; i < n; i += 8) {
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // out[i1] = point1 + point2
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // out[i2] = point1 - point2
        Real[i2] = point1_real - point2_real;
        Imag[i2] = point1_imag - point2_imag;
    }
    w_real = w_array_real[4]; // = sqrt(2)/2
    // w_imag = -w_real; // = w_array_imag[4]; // = -sqrt(2)/2
    for (i = 1; i < n; i += 8) {
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;
        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = MUL_F(point1_real + point1_imag, w_real);
        Imag[i2] = MUL_F(point1_imag - point1_real, w_real);
    }
    for (i = 2; i < n; i += 8) {
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // x[i] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * (-i)
        Real[i2] = point1_imag - point2_imag;
        Imag[i2] = point2_real - point1_real;
    }
    w_real = w_array_real[12]; // = -sqrt(2)/2
    // w_imag = w_real; // = w_array_imag[12]; // = -sqrt(2)/2
    for (i = 3; i < n; i += 8) {
        i2 = i + 4;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;
        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = MUL_F(point1_real - point1_imag, w_real);
        Imag[i2] = MUL_F(point1_real + point1_imag, w_real);
    }
    // Stage 4 of 32 point FFT decimation in frequency (no multiplications)
    // 16*4=64 additions
    for (i = 0; i < n; i += 4) {
        i2 = i + 2;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = x[i] - x[i2]
        Real[i2] = point1_real - point2_real;
        Imag[i2] = point1_imag - point2_imag;
    }
    for (i = 1; i < n; i += 4) {
        i2 = i + 2;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // x[i] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // x[i2] = (x[i] - x[i2]) * (-i)
        Real[i2] = point1_imag - point2_imag;
        Imag[i2] = point2_real - point1_real;
    }
    // Stage 5 of 32 point FFT decimation in frequency (no multiplications)
    // 16*4=64 additions
    for (i = 0; i < n; i += 2) {
        i2 = i + 1;
        point1_real = Real[i];
        point1_imag = Imag[i];
        point2_real = Real[i2];
        point2_imag = Imag[i2];
        // out[i1] = point1 + point2
        Real[i] += point2_real;
        Imag[i] += point2_imag;
        // out[i2] = point1 - point2
        Real[i2] = point1_real - point2_real;
        Imag[i2] = point1_imag - point2_imag;
    }
        #ifdef REORDER_IN_FFT
    FFTReorder(Real, Imag);
        #endif // #ifdef REORDER_IN_FFT
}
    #endif // SBR_LOW_POWER
#endif // SBR_DEC
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
static void flt_round(float32_t* pf) {
    int32_t  flg;
    uint32_t tmp, tmp1, tmp2;
    tmp = *(uint32_t*)pf;
    flg = tmp & (uint32_t)0x00008000;
    tmp &= (uint32_t)0xffff0000;
    tmp1 = tmp;
    /* round 1/2 lsb toward infinity */
    if(flg) {
        tmp &= (uint32_t)0xff800000; /* extract exponent and sign */
        tmp |= (uint32_t)0x00010000; /* insert 1 lsb */
        tmp2 = tmp;                  /* add 1 lsb and elided one */
        tmp &= (uint32_t)0xff800000; /* extract exponent and sign */
    //  *pf = *(float32_t*)&tmp1 + *(float32_t*)&tmp2 - *(float32_t*)&tmp;  // [-Wstrict-aliasing]
        float32_t f1, f2, f3;
        memcpy(&f1, &tmp1, sizeof(float32_t));
        memcpy(&f2, &tmp2, sizeof(float32_t));
        memcpy(&f3, &tmp, sizeof(float32_t));
        *pf = f1 + f2 - f3;
    }
    else {
    //  *pf = *(float32_t*)&tmp;  // [-Wstrict-aliasing]
        memcpy(pf, &tmp, sizeof(float32_t));
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
static int16_t quant_pred(float32_t x) {
    int16_t   q;
    uint32_t* tmp = (uint32_t*)&x;
    q = (int16_t)(*tmp >> 16);
    return q;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
static float32_t inv_quant_pred(int16_t q) {
    float32_t x = 0.0f;
    uint32_t* tmp = (uint32_t*)&x;
    *tmp = ((uint32_t)q) << 16;
    return x;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
static void ic_predict(pred_state* state, real_t input, real_t* output, uint8_t pred) {
    uint16_t  tmp;
    int16_t   i, j;
    real_t    dr1;
    float32_t predictedvalue;
    real_t    e0, e1;
    real_t    k1, k2;
    real_t r[2];
    real_t COR[2];
    real_t VAR[2];
    r[0] = inv_quant_pred(state->r[0]);
    r[1] = inv_quant_pred(state->r[1]);
    COR[0] = inv_quant_pred(state->COR[0]);
    COR[1] = inv_quant_pred(state->COR[1]);
    VAR[0] = inv_quant_pred(state->VAR[0]);
    VAR[1] = inv_quant_pred(state->VAR[1]);
    #if 1
    tmp = state->VAR[0];
    j = (tmp >> 7);
    i = tmp & 0x7f;
    if(j >= 128) {
        j -= 128;
        k1 = COR[0] * exp_table[j] * mnt_table[i];
    }
    else { k1 = REAL_CONST(0); }
    #else
    {
        #define B 0.953125
        real_t    c = COR[0];
        real_t    v = VAR[0];
        float32_t tmp;
        if(c == 0 || v <= 1) { k1 = 0; }
        else {
            tmp = B / v;
            flt_round(&tmp);
            k1 = c * tmp;
        }
    }
    #endif
    if(pred) {
    #if 1
        tmp = state->VAR[1];
        j = (tmp >> 7);
        i = tmp & 0x7f;
        if(j >= 128) {
            j -= 128;
            k2 = COR[1] * exp_table[j] * mnt_table[i];
        }
        else { k2 = REAL_CONST(0); }
    #else
        #define B 0.953125
        real_t    c = COR[1];
        real_t    v = VAR[1];
        float32_t tmp;
        if(c == 0 || v <= 1) { k2 = 0; }
        else {
            tmp = B / v;
            flt_round(&tmp);
            k2 = c * tmp;
        }
    #endif
        predictedvalue = k1 * r[0] + k2 * r[1];
        flt_round(&predictedvalue);
        *output = input + predictedvalue;
    }
    /* calculate new state data */
    e0 = *output;
    e1 = e0 - k1 * r[0];
    dr1 = k1 * e0;
    VAR[0] = ALPHA * VAR[0] + 0.5f * (r[0] * r[0] + e0 * e0);
    COR[0] = ALPHA * COR[0] + r[0] * e0;
    VAR[1] = ALPHA * VAR[1] + 0.5f * (r[1] * r[1] + e1 * e1);
    COR[1] = ALPHA * COR[1] + r[1] * e1;
    r[1] = A * (r[0] - dr1);
    r[0] = A * e0;
    state->r[0] = quant_pred(r[0]);
    state->r[1] = quant_pred(r[1]);
    state->COR[0] = quant_pred(COR[0]);
    state->COR[1] = quant_pred(COR[1]);
    state->VAR[0] = quant_pred(VAR[0]);
    state->VAR[1] = quant_pred(VAR[1]);
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
static void reset_pred_state(pred_state* state) {
    state->r[0] = 0;
    state->r[1] = 0;
    state->COR[0] = 0;
    state->COR[1] = 0;
    state->VAR[0] = 0x3F80;
    state->VAR[1] = 0x3F80;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
void pns_reset_pred_state(ic_stream* ics, pred_state* state) {
    uint8_t  sfb, g, b;
    uint16_t i, offs, offs2;
    /* prediction only for long blocks */
    if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) return;
    for(g = 0; g < ics->num_window_groups; g++) {
        for(b = 0; b < ics->window_group_length[g]; b++) {
            for(sfb = 0; sfb < ics->max_sfb; sfb++) {
                if(is_noise(ics, g, sfb)) {
                    offs = ics->swb_offset[sfb];
                    offs2 = min(ics->swb_offset[sfb + 1], ics->swb_offset_max);
                    for(i = offs; i < offs2; i++) reset_pred_state(&state[i]);
                }
            }
        }
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
void reset_all_predictors(pred_state* state, uint16_t frame_len) {
    uint16_t i;
    for(i = 0; i < frame_len; i++) reset_pred_state(&state[i]);
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
/* intra channel prediction */
void ic_prediction(ic_stream* ics, real_t* spec, pred_state* state, uint16_t frame_len, uint8_t sf_index) {
    uint8_t  sfb;
    uint16_t bin;
    if(ics->window_sequence == EIGHT_SHORT_SEQUENCE) { reset_all_predictors(state, frame_len); }
    else {
        for(sfb = 0; sfb < max_pred_sfb(sf_index); sfb++) {
            uint16_t low = ics->swb_offset[sfb];
            uint16_t high = min(ics->swb_offset[sfb + 1], ics->swb_offset_max);
            for(bin = low; bin < high; bin++) { ic_predict(&state[bin], spec[bin], &spec[bin], (ics->predictor_data_present && ics->pred.prediction_used[sfb])); }
        }
        if(ics->predictor_data_present) {
            if(ics->pred.predictor_reset) {
                for(bin = ics->pred.predictor_reset_group_number - 1; bin < frame_len; bin += 30) { reset_pred_state(&state[bin]); }
            }
        }
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef LD_DEC
ALIGN static const uint16_t* swb_offset_512_window[] = {
    0,                 /* 96000 */
    0,                 /* 88200 */
    0,                 /* 64000 */
    swb_offset_512_48, /* 48000 */
    swb_offset_512_48, /* 44100 */
    swb_offset_512_32, /* 32000 */
    swb_offset_512_24, /* 24000 */
    swb_offset_512_24, /* 22050 */
    0,                 /* 16000 */
    0,                 /* 12000 */
    0,                 /* 11025 */
    0                  /* 8000  */
};
#endif // LD_DEC
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef LD_DEC
ALIGN static const uint16_t* swb_offset_480_window[] = {
    0,                 /* 96000 */
    0,                 /* 88200 */
    0,                 /* 64000 */
    swb_offset_480_48, /* 48000 */
    swb_offset_480_48, /* 44100 */
    swb_offset_480_32, /* 32000 */
    swb_offset_480_24, /* 24000 */
    swb_offset_480_24, /* 22050 */
    0,                 /* 16000 */
    0,                 /* 12000 */
    0,                 /* 11025 */
    0                  /* 8000  */
};
#endif // LD_DEC
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
ALIGN static const uint16_t* swb_offset_128_window[] = {
    swb_offset_128_96, /* 96000 */
    swb_offset_128_96, /* 88200 */
    swb_offset_128_64, /* 64000 */
    swb_offset_128_48, /* 48000 */
    swb_offset_128_48, /* 44100 */
    swb_offset_128_48, /* 32000 */
    swb_offset_128_24, /* 24000 */
    swb_offset_128_24, /* 22050 */
    swb_offset_128_16, /* 16000 */
    swb_offset_128_16, /* 12000 */
    swb_offset_128_16, /* 11025 */
    swb_offset_128_8   /* 8000  */
};
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#define bit_set(A, B) ((A) & (1 << (B)))
/* 4.5.2.3.4 */
/*
  - determine the number of windows in a window_sequence named num_windows
  - determine the number of window_groups named num_window_groups
  - determine the number of windows in each group named window_group_length[g]
  - determine the total number of scalefactor window bands named num_swb for
    the actual window type
  - determine swb_offset[swb], the offset of the first coefficient in
    scalefactor window band named swb of the window actually used
  - determine sect_sfb_offset[g][section],the offset of the first coefficient
    in section named section. This offset depends on window_sequence and
    scale_factor_grouping and is needed to decode the spectral_data().
*/
uint8_t window_grouping_info(NeAACDecStruct* hDecoder, ic_stream* ics) {
    uint8_t i, g;
    uint8_t sf_index = hDecoder->sf_index;
    switch (ics->window_sequence) {
        case ONLY_LONG_SEQUENCE:
        case LONG_START_SEQUENCE:
        case LONG_STOP_SEQUENCE:
            ics->num_windows = 1;
            ics->num_window_groups = 1;
            ics->window_group_length[ics->num_window_groups - 1] = 1;
#ifdef LD_DEC
            if (hDecoder->object_type == LD) {
                if (hDecoder->frameLength == 512)
                    ics->num_swb = num_swb_512_window[sf_index];
                else /* if (hDecoder->frameLength == 480) */
                    ics->num_swb = num_swb_480_window[sf_index];
            } else {
#endif
                if (hDecoder->frameLength == 1024)
                    ics->num_swb = num_swb_1024_window[sf_index];
                else /* if (hDecoder->frameLength == 960) */
                    ics->num_swb = num_swb_960_window[sf_index];
#ifdef LD_DEC
            }
#endif
            if (ics->max_sfb > ics->num_swb) { return 32; }
            /* preparation of sect_sfb_offset for long blocks */
            /* also copy the last value! */
#ifdef LD_DEC
            if (hDecoder->object_type == LD) {
                if (hDecoder->frameLength == 512) {
                    for (i = 0; i < ics->num_swb; i++) {
                        ics->sect_sfb_offset[0][i] = swb_offset_512_window[sf_index][i];
                        ics->swb_offset[i] = swb_offset_512_window[sf_index][i];
                    }
                } else /* if (hDecoder->frameLength == 480) */ {
                    for (i = 0; i < ics->num_swb; i++) {
                        ics->sect_sfb_offset[0][i] = swb_offset_480_window[sf_index][i];
                        ics->swb_offset[i] = swb_offset_480_window[sf_index][i];
                    }
                }
                ics->sect_sfb_offset[0][ics->num_swb] = hDecoder->frameLength;
                ics->swb_offset[ics->num_swb] = hDecoder->frameLength;
                ics->swb_offset_max = hDecoder->frameLength;
            } else {
#endif
                for (i = 0; i < ics->num_swb; i++) {
                    ics->sect_sfb_offset[0][i] = swb_offset_1024_window[sf_index][i];
                    ics->swb_offset[i] = swb_offset_1024_window[sf_index][i];
                }
                ics->sect_sfb_offset[0][ics->num_swb] = hDecoder->frameLength;
                ics->swb_offset[ics->num_swb] = hDecoder->frameLength;
                ics->swb_offset_max = hDecoder->frameLength;
#ifdef LD_DEC
            }
#endif
            return 0;
        case EIGHT_SHORT_SEQUENCE:
            ics->num_windows = 8;
            ics->num_window_groups = 1;
            ics->window_group_length[ics->num_window_groups - 1] = 1;
            ics->num_swb = num_swb_128_window[sf_index];
            if (ics->max_sfb > ics->num_swb) { return 32; }
            for (i = 0; i < ics->num_swb; i++) ics->swb_offset[i] = swb_offset_128_window[sf_index][i];
            ics->swb_offset[ics->num_swb] = hDecoder->frameLength / 8;
            ics->swb_offset_max = hDecoder->frameLength / 8;
            for (i = 0; i < ics->num_windows - 1; i++) {
                if (bit_set(ics->scale_factor_grouping, 6 - i) == 0) {
                    ics->num_window_groups += 1;
                    ics->window_group_length[ics->num_window_groups - 1] = 1;
                } else {
                    ics->window_group_length[ics->num_window_groups - 1] += 1;
                }
            }
            /* preparation of sect_sfb_offset for short blocks */
            for (g = 0; g < ics->num_window_groups; g++) {
                uint16_t width;
                uint8_t  sect_sfb = 0;
                uint16_t offset = 0;
                for (i = 0; i < ics->num_swb; i++) {
                    if (i + 1 == ics->num_swb) {
                        width = (hDecoder->frameLength / 8) - swb_offset_128_window[sf_index][i];
                    } else {
                        width = swb_offset_128_window[sf_index][i + 1] - swb_offset_128_window[sf_index][i];
                    }
                    width *= ics->window_group_length[g];
                    ics->sect_sfb_offset[g][sect_sfb++] = offset;
                    offset += width;
                }
                ics->sect_sfb_offset[g][sect_sfb] = offset;
            }
            return 0;
        default: return 32;
    }
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* iquant() */
/* output = sign(input)*abs(input)^(4/3) */
static inline real_t iquant(int16_t q, const real_t* tab, uint8_t* error) {
#ifdef FIXED_POINT
    /* For FIXED_POINT the iq_table is prescaled by 3 bits (iq_table[]/8) */
    /* BIG_IQ_TABLE allows you to use the full 8192 value table, if this is not
     * defined a 1026 value table and interpolation will be used
     */
    #ifndef BIG_IQ_TABLE
    static const real_t errcorr[] = {REAL_CONST(0),         REAL_CONST(1.0 / 8.0), REAL_CONST(2.0 / 8.0), REAL_CONST(3.0 / 8.0), REAL_CONST(4.0 / 8.0),
                                     REAL_CONST(5.0 / 8.0), REAL_CONST(6.0 / 8.0), REAL_CONST(7.0 / 8.0), REAL_CONST(0)};
    real_t              x1, x2;
    #endif
    int16_t sgn = 1;
    if (q < 0) {
        q = -q;
        sgn = -1;
    }
    if (q < IQ_TABLE_SIZE) {
    // #define IQUANT_PRINT
    #ifdef IQUANT_PRINT
        // printf("0x%.8X\n", sgn * tab[q]);
        printf("%d\n", sgn * tab[q]);
    #endif
        return sgn * tab[q];
    }
    #ifndef BIG_IQ_TABLE
    if (q >= 8192) {
        *error = 17;
        return 0;
    }
    /* linear interpolation */
    x1 = tab[q >> 3];
    x2 = tab[(q >> 3) + 1];
    return sgn * 16 * (MUL_R(errcorr[q & 7], (x2 - x1)) + x1);
    #else
    *error = 17;
    return 0;
    #endif
#else
    if (q < 0) {
        /* tab contains a value for all possible q [0,8192] */
        if (-q < IQ_TABLE_SIZE) return -tab[-q];
        *error = 17;
        return 0;
    } else {
        /* tab contains a value for all possible q [0,8192] */
        if (q < IQ_TABLE_SIZE) return tab[q];
        *error = 17;
        return 0;
    }
#endif
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* quant_to_spec: perform dequantisation and scaling and in case of short block it also does the deinterleaving */
/*
  For ONLY_LONG_SEQUENCE windows (num_window_groups = 1,  window_group_length[0] = 1) the spectral data is in ascending spectral order.
  For the EIGHT_SHORT_SEQUENCE window, the spectral order depends on the grouping in the following manner:
  - Groups are ordered sequentially
  - Within a group, a scalefactor band consists of the spectral data of all grouped SHORT_WINDOWs for the associated scalefactor window band. To
    clarify via example, the length of a group is in the range of one to eight SHORT_WINDOWs.
  - If there are eight groups each with length one (num_window_groups = 8, window_group_length[0..7] = 1), the result is a sequence of eight spectra,
    each in ascending spectral order.
  - If there is only one group with length eight (num_window_groups = 1, window_group_length[0] = 8), the result is that spectral data of all eight
    SHORT_WINDOWs is interleaved by scalefactor window bands.
  - Within a scalefactor window band, the coefficients are in ascending spectral order.
*/
uint8_t quant_to_spec(NeAACDecStruct* hDecoder, ic_stream* ics, int16_t* quant_data, real_t* spec_data, uint16_t frame_len) {
    ALIGN static const real_t pow2_table[] = {
        COEF_CONST(1.0), COEF_CONST(1.1892071150027210667174999705605), /* 2^0.25 */
        COEF_CONST(1.4142135623730950488016887242097),                  /* 2^0.5 */
        COEF_CONST(1.6817928305074290860622509524664)                   /* 2^0.75 */
    };
    const real_t* tab = iq_table;
    uint8_t       g, sfb, win;
    uint16_t      width, bin, k, gindex, wa, wb;
    uint8_t       error = 0; /* Init error flag */
#ifndef FIXED_POINT
    real_t scf;
#endif
    k = 0;
    gindex = 0;
    for (g = 0; g < ics->num_window_groups; g++) {
        uint16_t j = 0;
        uint16_t gincrease = 0;
        uint16_t win_inc = ics->swb_offset[ics->num_swb];
        for (sfb = 0; sfb < ics->num_swb; sfb++) {
            int32_t exp, frac;
            width = ics->swb_offset[sfb + 1] - ics->swb_offset[sfb];
            /* this could be scalefactor for IS or PNS, those can be negative or bigger then 255 */
            /* just ignore them */
            if (ics->scale_factors[g][sfb] < 0 || ics->scale_factors[g][sfb] > 255) {
                exp = 0;
                frac = 0;
            } else {
                /* ics->scale_factors[g][sfb] must be between 0 and 255 */
                exp = (ics->scale_factors[g][sfb] /* - 100 */) >> 2;
                /* frac must always be > 0 */
                frac = (ics->scale_factors[g][sfb] /* - 100 */) & 3;
            }
#ifdef FIXED_POINT
            exp -= 25;
            /* IMDCT pre-scaling */
            if (hDecoder->object_type == LD) {
                exp -= 6 /*9*/;
            } else {
                if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
                    exp -= 4 /*7*/;
                else
                    exp -= 7 /*10*/;
            }
#endif
            wa = gindex + j;
#ifndef FIXED_POINT
            scf = pow2sf_tab[exp /*+25*/] * pow2_table[frac];
#endif
            for (win = 0; win < ics->window_group_length[g]; win++) {
                for (bin = 0; bin < width; bin += 4) {
#ifndef FIXED_POINT
                    wb = wa + bin;
                    spec_data[wb + 0] = iquant(quant_data[k + 0], tab, &error) * scf;
                    spec_data[wb + 1] = iquant(quant_data[k + 1], tab, &error) * scf;
                    spec_data[wb + 2] = iquant(quant_data[k + 2], tab, &error) * scf;
                    spec_data[wb + 3] = iquant(quant_data[k + 3], tab, &error) * scf;
#else
                    real_t iq0 = iquant(quant_data[k + 0], tab, &error);
                    real_t iq1 = iquant(quant_data[k + 1], tab, &error);
                    real_t iq2 = iquant(quant_data[k + 2], tab, &error);
                    real_t iq3 = iquant(quant_data[k + 3], tab, &error);
                    wb = wa + bin;
                    if (exp < 0) {
                        spec_data[wb + 0] = iq0 >>= -exp;
                        spec_data[wb + 1] = iq1 >>= -exp;
                        spec_data[wb + 2] = iq2 >>= -exp;
                        spec_data[wb + 3] = iq3 >>= -exp;
                    } else {
                        spec_data[wb + 0] = iq0 <<= exp;
                        spec_data[wb + 1] = iq1 <<= exp;
                        spec_data[wb + 2] = iq2 <<= exp;
                        spec_data[wb + 3] = iq3 <<= exp;
                    }
                    if (frac != 0) {
                        spec_data[wb + 0] = MUL_C(spec_data[wb + 0], pow2_table[frac]);
                        spec_data[wb + 1] = MUL_C(spec_data[wb + 1], pow2_table[frac]);
                        spec_data[wb + 2] = MUL_C(spec_data[wb + 2], pow2_table[frac]);
                        spec_data[wb + 3] = MUL_C(spec_data[wb + 3], pow2_table[frac]);
                    }
    // #define SCFS_PRINT
    #ifdef SCFS_PRINT
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 0]);
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 1]);
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 2]);
                    printf("%d\n", spec_data[gindex + (win * win_inc) + j + bin + 3]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+0]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+1]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+2]);
                        // printf("0x%.8X\n", spec_data[gindex+(win*win_inc)+j+bin+3]);
    #endif
#endif
                    gincrease += 4;
                    k += 4;
                }
                wa += win_inc;
            }
            j += width;
        }
        gindex += gincrease;
    }
    return error;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static uint8_t allocate_single_channel(NeAACDecStruct* hDecoder, uint8_t channel, uint8_t output_channels) {
    int mul = 1;
#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        /* allocate the state only when needed */
        if (hDecoder->pred_stat[channel] != NULL) {
            faad_free(hDecoder->pred_stat[channel]);
            hDecoder->pred_stat[channel] = NULL;
        }
        hDecoder->pred_stat[channel] = (pred_state*)faad_malloc(hDecoder->frameLength * sizeof(pred_state));
        reset_all_predictors(hDecoder->pred_stat[channel], hDecoder->frameLength);
    }
#endif
#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type)) {
        /* allocate the state only when needed */
        if (hDecoder->lt_pred_stat[channel] != NULL) {
            faad_free(hDecoder->lt_pred_stat[channel]);
            hDecoder->lt_pred_stat[channel] = NULL;
        }
        hDecoder->lt_pred_stat[channel] = (int16_t*)faad_malloc(hDecoder->frameLength * 4 * sizeof(int16_t));
        memset(hDecoder->lt_pred_stat[channel], 0, hDecoder->frameLength * 4 * sizeof(int16_t));
    }
#endif
    if (hDecoder->time_out[channel] != NULL) {
        faad_free(hDecoder->time_out[channel]);
        hDecoder->time_out[channel] = NULL;
    }
    {
        mul = 1;
#ifdef SBR_DEC
        hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 0;
        if ((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) {
            /* SBR requires 2 times as much output data */
            mul = 2;
            hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 1;
        }
#endif
        hDecoder->time_out[channel] = (real_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(real_t));
        memset(hDecoder->time_out[channel], 0, mul * hDecoder->frameLength * sizeof(real_t));
    }
#if (defined(PS_DEC) || defined(DRM_PS))
    if (output_channels == 2) {
        if (hDecoder->time_out[channel + 1] != NULL) {
            faad_free(hDecoder->time_out[channel + 1]);
            hDecoder->time_out[channel + 1] = NULL;
        }
        hDecoder->time_out[channel + 1] = (real_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(real_t));
        memset(hDecoder->time_out[channel + 1], 0, mul * hDecoder->frameLength * sizeof(real_t));
    }
#endif
    if (hDecoder->fb_intermed[channel] != NULL) {
        faad_free(hDecoder->fb_intermed[channel]);
        hDecoder->fb_intermed[channel] = NULL;
    }
    hDecoder->fb_intermed[channel] = (real_t*)faad_malloc(hDecoder->frameLength * sizeof(real_t));
    memset(hDecoder->fb_intermed[channel], 0, hDecoder->frameLength * sizeof(real_t));
#ifdef SSR_DEC
    if (hDecoder->object_type == SSR) {
        if (hDecoder->ssr_overlap[channel] == NULL) {
            hDecoder->ssr_overlap[channel] = (real_t*)faad_malloc(2 * hDecoder->frameLength * sizeof(real_t));
            memset(hDecoder->ssr_overlap[channel], 0, 2 * hDecoder->frameLength * sizeof(real_t));
        }
        if (hDecoder->prev_fmd[channel] == NULL) {
            uint16_t k;
            hDecoder->prev_fmd[channel] = (real_t*)faad_malloc(2 * hDecoder->frameLength * sizeof(real_t));
            for (k = 0; k < 2 * hDecoder->frameLength; k++) hDecoder->prev_fmd[channel][k] = REAL_CONST(-1);
        }
    }
#endif
    return 0;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static uint8_t allocate_channel_pair(NeAACDecStruct* hDecoder, uint8_t channel, uint8_t paired_channel) {
    int mul = 1;
#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        /* allocate the state only when needed */
        if (hDecoder->pred_stat[channel] == NULL) {
            hDecoder->pred_stat[channel] = (pred_state*)faad_malloc(hDecoder->frameLength * sizeof(pred_state));
            reset_all_predictors(hDecoder->pred_stat[channel], hDecoder->frameLength);
        }
        if (hDecoder->pred_stat[paired_channel] == NULL) {
            hDecoder->pred_stat[paired_channel] = (pred_state*)faad_malloc(hDecoder->frameLength * sizeof(pred_state));
            reset_all_predictors(hDecoder->pred_stat[paired_channel], hDecoder->frameLength);
        }
    }
#endif
#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type)) {
        /* allocate the state only when needed */
        if (hDecoder->lt_pred_stat[channel] == NULL) {
            hDecoder->lt_pred_stat[channel] = (int16_t*)faad_malloc(hDecoder->frameLength * 4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[channel], 0, hDecoder->frameLength * 4 * sizeof(int16_t));
        }
        if (hDecoder->lt_pred_stat[paired_channel] == NULL) {
            hDecoder->lt_pred_stat[paired_channel] = (int16_t*)faad_malloc(hDecoder->frameLength * 4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[paired_channel], 0, hDecoder->frameLength * 4 * sizeof(int16_t));
        }
    }
#endif
    if (hDecoder->time_out[channel] == NULL) {
        mul = 1;
#ifdef SBR_DEC
        hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 0;
        if ((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) {
            /* SBR requires 2 times as much output data */
            mul = 2;
            hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 1;
        }
#endif
        hDecoder->time_out[channel] = (real_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(real_t));
        memset(hDecoder->time_out[channel], 0, mul * hDecoder->frameLength * sizeof(real_t));
    }
    if (hDecoder->time_out[paired_channel] == NULL) {
        hDecoder->time_out[paired_channel] = (real_t*)faad_malloc(mul * hDecoder->frameLength * sizeof(real_t));
        memset(hDecoder->time_out[paired_channel], 0, mul * hDecoder->frameLength * sizeof(real_t));
    }
    if (hDecoder->fb_intermed[channel] == NULL) {
        hDecoder->fb_intermed[channel] = (real_t*)faad_malloc(hDecoder->frameLength * sizeof(real_t));
        memset(hDecoder->fb_intermed[channel], 0, hDecoder->frameLength * sizeof(real_t));
    }
    if (hDecoder->fb_intermed[paired_channel] == NULL) {
        hDecoder->fb_intermed[paired_channel] = (real_t*)faad_malloc(hDecoder->frameLength * sizeof(real_t));
        memset(hDecoder->fb_intermed[paired_channel], 0, hDecoder->frameLength * sizeof(real_t));
    }
#ifdef SSR_DEC
    if (hDecoder->object_type == SSR) {
        if (hDecoder->ssr_overlap[channel] == NULL) {
            hDecoder->ssr_overlap[channel] = (real_t*)faad_malloc(2 * hDecoder->frameLength * sizeof(real_t));
            memset(hDecoder->ssr_overlap[channel], 0, 2 * hDecoder->frameLength * sizeof(real_t));
        }
        if (hDecoder->ssr_overlap[paired_channel] == NULL) {
            hDecoder->ssr_overlap[paired_channel] = (real_t*)faad_malloc(2 * hDecoder->frameLength * sizeof(real_t));
            memset(hDecoder->ssr_overlap[paired_channel], 0, 2 * hDecoder->frameLength * sizeof(real_t));
        }
        if (hDecoder->prev_fmd[channel] == NULL) {
            uint16_t k;
            hDecoder->prev_fmd[channel] = (real_t*)faad_malloc(2 * hDecoder->frameLength * sizeof(real_t));
            for (k = 0; k < 2 * hDecoder->frameLength; k++) hDecoder->prev_fmd[channel][k] = REAL_CONST(-1);
        }
        if (hDecoder->prev_fmd[paired_channel] == NULL) {
            uint16_t k;
            hDecoder->prev_fmd[paired_channel] = (real_t*)faad_malloc(2 * hDecoder->frameLength * sizeof(real_t));
            for (k = 0; k < 2 * hDecoder->frameLength; k++) hDecoder->prev_fmd[paired_channel][k] = REAL_CONST(-1);
        }
    }
#endif
    return 0;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t reconstruct_single_channel(NeAACDecStruct* hDecoder, ic_stream* ics, element* sce, int16_t* spec_data) {
    uint8_t retval;
    int     output_channels;
    // ALIGN real_t spec_coef[1024];
    real_t* spec_coef = ps_malloc(1024 * sizeof(real_t));
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
    /* always allocate 2 channels, PS can always "suddenly" turn up */
#if ((defined(DRM) && defined(DRM_PS)))
    output_channels = 2;
#elif defined(PS_DEC)
    if (hDecoder->ps_used[hDecoder->fr_ch_ele])
        output_channels = 2;
    else
        output_channels = 1;
#else
    output_channels = 1;
#endif
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0) {
        /* element_output_channels not set yet */
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = output_channels;
    } else if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] != output_channels) {
        /* element inconsistency */
        /* this only happens if PS is actually found but not in the first frame
         * this means that there is only 1 bitstream element!
         */
        /* reset the allocation */
        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 0;
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = output_channels;
        // return 21;
    }
    if (hDecoder->element_alloced[hDecoder->fr_ch_ele] == 0) {
        retval = allocate_single_channel(hDecoder, sce->channel, output_channels);
        if (retval > 0) goto exit;
        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 1;
    }
    /* sanity check, CVE-2018-20199, CVE-2018-20360 */
    if (!hDecoder->time_out[sce->channel]) {
        retval = 15;
        goto exit;
    }
    if (output_channels > 1 && !hDecoder->time_out[sce->channel + 1]) {
        retval = 15;
        goto exit;
    }
    if (!hDecoder->fb_intermed[sce->channel]) {
        retval = 15;
        goto exit;
    }
    /* dequantisation and scaling */
    retval = quant_to_spec(hDecoder, ics, spec_data, spec_coef, hDecoder->frameLength);
    if (retval > 0) goto exit;
#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->requant_cycles += count;
#endif
    /* pns decoding */
    pns_decode(ics, NULL, spec_coef, NULL, hDecoder->frameLength, 0, hDecoder->object_type, &(hDecoder->__r1), &(hDecoder->__r2));
#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        if (!hDecoder->pred_stat[sce->channel]) {
            retval = 33;
            goto exit;
        } // return 33;
        /* intra channel prediction */
        ic_prediction(ics, spec_coef, hDecoder->pred_stat[sce->channel], hDecoder->frameLength, hDecoder->sf_index);
        /* In addition, for scalefactor bands coded by perceptual
           noise substitution the predictors belonging to the
           corresponding spectral coefficients are reset.
        */
        pns_reset_pred_state(ics, hDecoder->pred_stat[sce->channel]);
    }
#endif
#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type)) {
    #ifdef LD_DEC
        if (hDecoder->object_type == LD) {
            if (ics->ltp.data_present) {
                if (ics->ltp.lag_update) hDecoder->ltp_lag[sce->channel] = ics->ltp.lag;
            }
            ics->ltp.lag = hDecoder->ltp_lag[sce->channel];
        }
    #endif
        /* long term prediction */
        lt_prediction(ics, &(ics->ltp), spec_coef, hDecoder->lt_pred_stat[sce->channel], hDecoder->fb, ics->window_shape, hDecoder->window_shape_prev[sce->channel], hDecoder->sf_index,
                      hDecoder->object_type, hDecoder->frameLength);
    }
#endif
    /* tns decoding */
    tns_decode_frame(ics, &(ics->tns), hDecoder->sf_index, hDecoder->object_type, spec_coef, hDecoder->frameLength);
    /* drc decoding */
#ifdef APPLY_DRC
    if (hDecoder->drc->present) {
        if (!hDecoder->drc->exclude_mask[sce->channel] || !hDecoder->drc->excluded_chns_present) drc_decode(hDecoder->drc, spec_coef);
    }
#endif
    /* filter bank */
#ifdef SSR_DEC
    if (hDecoder->object_type != SSR) {
#endif
        ifilter_bank(hDecoder->fb, ics->window_sequence, ics->window_shape, hDecoder->window_shape_prev[sce->channel], spec_coef, hDecoder->time_out[sce->channel], hDecoder->fb_intermed[sce->channel],
                     hDecoder->object_type, hDecoder->frameLength);
#ifdef SSR_DEC
    } else {
        ssr_decode(&(ics->ssr), hDecoder->fb, ics->window_sequence, ics->window_shape, hDecoder->window_shape_prev[sce->channel], spec_coef, hDecoder->time_out[sce->channel],
                   hDecoder->ssr_overlap[sce->channel], hDecoder->ipqf_buffer[sce->channel], hDecoder->prev_fmd[sce->channel], hDecoder->frameLength);
    }
#endif
    /* save window shape for next frame */
    hDecoder->window_shape_prev[sce->channel] = ics->window_shape;
#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type)) {
        lt_update_state(hDecoder->lt_pred_stat[sce->channel], hDecoder->time_out[sce->channel], hDecoder->fb_intermed[sce->channel], hDecoder->frameLength, hDecoder->object_type);
    }
#endif
#ifdef SBR_DEC
    if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        int ele = hDecoder->fr_ch_ele;
        int ch = sce->channel;
        /* following case can happen when forceUpSampling == 1 */
        if (hDecoder->sbr[ele] == NULL) { hDecoder->sbr[ele] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[ele], 2 * get_sample_rate(hDecoder->sf_index), hDecoder->downSampledSBR, 0); }
        if (!hDecoder->sbr[ele]) {
            retval = 19;
            goto exit;
        }
        if (sce->ics1.window_sequence == EIGHT_SHORT_SEQUENCE)
            hDecoder->sbr[ele]->maxAACLine = 8 * min(sce->ics1.swb_offset[max(sce->ics1.max_sfb - 1, 0)], sce->ics1.swb_offset_max);
        else
            hDecoder->sbr[ele]->maxAACLine = min(sce->ics1.swb_offset[max(sce->ics1.max_sfb - 1, 0)], sce->ics1.swb_offset_max);
            /* check if any of the PS tools is used */
    #if (defined(PS_DEC) || defined(DRM_PS))
        if (hDecoder->ps_used[ele] == 0) {
    #endif
            retval = sbrDecodeSingleFrame(hDecoder->sbr[ele], hDecoder->time_out[ch], hDecoder->postSeekResetFlag, hDecoder->downSampledSBR); hDecoder->isPS = 0;
    #if (defined(PS_DEC) || defined(DRM_PS))
        } else {
            retval = sbrDecodeSingleFramePS(hDecoder->sbr[ele], hDecoder->time_out[ch], hDecoder->time_out[ch + 1], hDecoder->postSeekResetFlag, hDecoder->downSampledSBR); hDecoder->isPS = 1;
        }
    #endif
        if (retval > 0) goto exit;
    } else if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && !hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        {
            retval = 23;
            goto exit;
        }
    }
#endif
    /* copy L to R when no PS is used */
#if (defined(PS_DEC) || defined(DRM_PS))
    if ((hDecoder->ps_used[hDecoder->fr_ch_ele] == 0) && (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2)) {
        int ele = hDecoder->fr_ch_ele;
        int ch = sce->channel;
        int frame_size = (hDecoder->sbr_alloced[ele]) ? 2 : 1;
        frame_size *= hDecoder->frameLength * sizeof(real_t);
        memcpy(hDecoder->time_out[ch + 1], hDecoder->time_out[ch], frame_size);
    }
#endif
    retval = 0;
exit:
    if (spec_coef) { free(spec_coef); }
    return retval;
}
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t reconstruct_channel_pair(NeAACDecStruct* hDecoder, ic_stream* ics1, ic_stream* ics2, element* cpe, int16_t* spec_data1, int16_t* spec_data2) {
    uint8_t retval;
    // ALIGN real_t spec_coef1[1024];
    // ALIGN real_t spec_coef2[1024];
    real_t* spec_coef1 = ps_malloc(1024 * sizeof(real_t));
    real_t* spec_coef2 = ps_malloc(1024 * sizeof(real_t));
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
    if (hDecoder->element_alloced[hDecoder->fr_ch_ele] != 2) {
        retval = allocate_channel_pair(hDecoder, cpe->channel, (uint8_t)cpe->paired_channel);
        if (retval > 0) goto exit;
        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 2;
    }
    /* sanity check, CVE-2018-20199, CVE-2018-20360 */
    if (!hDecoder->time_out[cpe->channel] || !hDecoder->time_out[cpe->paired_channel]) {
        retval = 15;
        goto exit;
    }
    if (!hDecoder->fb_intermed[cpe->channel] || !hDecoder->fb_intermed[cpe->paired_channel]) {
        retval = 15;
        goto exit;
    }
    /* dequantisation and scaling */
    retval = quant_to_spec(hDecoder, ics1, spec_data1, spec_coef1, hDecoder->frameLength);
    if (retval > 0) goto exit;
    retval = quant_to_spec(hDecoder, ics2, spec_data2, spec_coef2, hDecoder->frameLength);
    if (retval > 0) goto exit;
#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->requant_cycles += count;
#endif
    /* pns decoding */
    if (ics1->ms_mask_present) {
        pns_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength, 1, hDecoder->object_type, &(hDecoder->__r1), &(hDecoder->__r2));
    } else {
        pns_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength, 0, hDecoder->object_type, &(hDecoder->__r1), &(hDecoder->__r2));
    }
    /* mid/side decoding */
    ms_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength);
#if 0
    {
        int i;
        for (i = 0; i < 1024; i++)
        {
            //printf("%d\n", spec_coef1[i]);
            printf("0x%.8X\n", spec_coef1[i]);
        }
        for (i = 0; i < 1024; i++)
        {
            //printf("%d\n", spec_coef2[i]);
            printf("0x%.8X\n", spec_coef2[i]);
        }
    }
#endif
    /* intensity stereo decoding */
    is_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength);
#if 0
    {
        int i;
        for (i = 0; i < 1024; i++)
        {
            printf("%d\n", spec_coef1[i]);
            //printf("0x%.8X\n", spec_coef1[i]);
        }
        for (i = 0; i < 1024; i++)
        {
            printf("%d\n", spec_coef2[i]);
            //printf("0x%.8X\n", spec_coef2[i]);
        }
    }
#endif // 0
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN) {
        /* intra channel prediction */
        ic_prediction(ics1, spec_coef1, hDecoder->pred_stat[cpe->channel], hDecoder->frameLength, hDecoder->sf_index);
        ic_prediction(ics2, spec_coef2, hDecoder->pred_stat[cpe->paired_channel], hDecoder->frameLength, hDecoder->sf_index);
        /* In addition, for scalefactor bands coded by perceptual
           noise substitution the predictors belonging to the
           corresponding spectral coefficients are reset.
        */
        pns_reset_pred_state(ics1, hDecoder->pred_stat[cpe->channel]);
        pns_reset_pred_state(ics2, hDecoder->pred_stat[cpe->paired_channel]);
    }
#endif
#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type)) {
        ltp_info* ltp1 = &(ics1->ltp);
        ltp_info* ltp2 = (cpe->common_window) ? &(ics2->ltp2) : &(ics2->ltp);
    #ifdef LD_DEC
        if (hDecoder->object_type == LD) {
            if (ltp1->data_present) {
                if (ltp1->lag_update) hDecoder->ltp_lag[cpe->channel] = ltp1->lag;
            }
            ltp1->lag = hDecoder->ltp_lag[cpe->channel];
            if (ltp2->data_present) {
                if (ltp2->lag_update) hDecoder->ltp_lag[cpe->paired_channel] = ltp2->lag;
            }
            ltp2->lag = hDecoder->ltp_lag[cpe->paired_channel];
        }
    #endif
        /* long term prediction */
        lt_prediction(ics1, ltp1, spec_coef1, hDecoder->lt_pred_stat[cpe->channel], hDecoder->fb, ics1->window_shape, hDecoder->window_shape_prev[cpe->channel], hDecoder->sf_index,
                      hDecoder->object_type, hDecoder->frameLength);
        lt_prediction(ics2, ltp2, spec_coef2, hDecoder->lt_pred_stat[cpe->paired_channel], hDecoder->fb, ics2->window_shape, hDecoder->window_shape_prev[cpe->paired_channel], hDecoder->sf_index,
                      hDecoder->object_type, hDecoder->frameLength);
    }
#endif
    /* tns decoding */
    tns_decode_frame(ics1, &(ics1->tns), hDecoder->sf_index, hDecoder->object_type, spec_coef1, hDecoder->frameLength);
    tns_decode_frame(ics2, &(ics2->tns), hDecoder->sf_index, hDecoder->object_type, spec_coef2, hDecoder->frameLength);
    /* drc decoding */
#if APPLY_DRC
    if (hDecoder->drc->present) {
        if (!hDecoder->drc->exclude_mask[cpe->channel] || !hDecoder->drc->excluded_chns_present) drc_decode(hDecoder->drc, spec_coef1);
        if (!hDecoder->drc->exclude_mask[cpe->paired_channel] || !hDecoder->drc->excluded_chns_present) drc_decode(hDecoder->drc, spec_coef2);
    }
#endif
    /* filter bank */
#ifdef SSR_DEC
    if (hDecoder->object_type != SSR) {
#endif
        ifilter_bank(hDecoder->fb, ics1->window_sequence, ics1->window_shape, hDecoder->window_shape_prev[cpe->channel], spec_coef1, hDecoder->time_out[cpe->channel],
                     hDecoder->fb_intermed[cpe->channel], hDecoder->object_type, hDecoder->frameLength);
        ifilter_bank(hDecoder->fb, ics2->window_sequence, ics2->window_shape, hDecoder->window_shape_prev[cpe->paired_channel], spec_coef2, hDecoder->time_out[cpe->paired_channel],
                     hDecoder->fb_intermed[cpe->paired_channel], hDecoder->object_type, hDecoder->frameLength);
#ifdef SSR_DEC
    } else {
        ssr_decode(&(ics1->ssr), hDecoder->fb, ics1->window_sequence, ics1->window_shape, hDecoder->window_shape_prev[cpe->channel], spec_coef1, hDecoder->time_out[cpe->channel],
                   hDecoder->ssr_overlap[cpe->channel], hDecoder->ipqf_buffer[cpe->channel], hDecoder->prev_fmd[cpe->channel], hDecoder->frameLength);
        ssr_decode(&(ics2->ssr), hDecoder->fb, ics2->window_sequence, ics2->window_shape, hDecoder->window_shape_prev[cpe->paired_channel], spec_coef2, hDecoder->time_out[cpe->paired_channel],
                   hDecoder->ssr_overlap[cpe->paired_channel], hDecoder->ipqf_buffer[cpe->paired_channel], hDecoder->prev_fmd[cpe->paired_channel], hDecoder->frameLength);
    }
#endif
    /* save window shape for next frame */
    hDecoder->window_shape_prev[cpe->channel] = ics1->window_shape;
    hDecoder->window_shape_prev[cpe->paired_channel] = ics2->window_shape;
#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type)) {
        lt_update_state(hDecoder->lt_pred_stat[cpe->channel], hDecoder->time_out[cpe->channel], hDecoder->fb_intermed[cpe->channel], hDecoder->frameLength, hDecoder->object_type);
        lt_update_state(hDecoder->lt_pred_stat[cpe->paired_channel], hDecoder->time_out[cpe->paired_channel], hDecoder->fb_intermed[cpe->paired_channel], hDecoder->frameLength, hDecoder->object_type);
    }
#endif
#ifdef SBR_DEC
    if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        int ele = hDecoder->fr_ch_ele;
        int ch0 = cpe->channel;
        int ch1 = cpe->paired_channel;
        /* following case can happen when forceUpSampling == 1 */
        if (hDecoder->sbr[ele] == NULL) { hDecoder->sbr[ele] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[ele], 2 * get_sample_rate(hDecoder->sf_index), hDecoder->downSampledSBR, 0); }
        if (!hDecoder->sbr[ele]) {
            retval = 19;
            goto exit;
        }
        if (cpe->ics1.window_sequence == EIGHT_SHORT_SEQUENCE)
            hDecoder->sbr[ele]->maxAACLine = 8 * min(cpe->ics1.swb_offset[max(cpe->ics1.max_sfb - 1, 0)], cpe->ics1.swb_offset_max);
        else
            hDecoder->sbr[ele]->maxAACLine = min(cpe->ics1.swb_offset[max(cpe->ics1.max_sfb - 1, 0)], cpe->ics1.swb_offset_max);
        retval = sbrDecodeCoupleFrame(hDecoder->sbr[ele], hDecoder->time_out[ch0], hDecoder->time_out[ch1], hDecoder->postSeekResetFlag, hDecoder->downSampledSBR);
        if (retval > 0) { goto exit; }
    } else if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1)) && !hDecoder->sbr_alloced[hDecoder->fr_ch_ele]) {
        retval = 23;
        goto exit;
    }
#endif
    retval = 0;
exit:
    if (spec_coef1) free(spec_coef1);
    if (spec_coef2) free(spec_coef2);
    return retval;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* TNS decoding for one channel and frame */
void tns_decode_frame(ic_stream* ics, tns_info* tns, uint8_t sr_index, uint8_t object_type, real_t* spec, uint16_t frame_len) {
    uint8_t  w, f, tns_order;
    int8_t   inc;
    int16_t  size;
    uint16_t bottom, top, start, end;
    uint16_t nshort = frame_len / 8;
    real_t   lpc[TNS_MAX_ORDER + 1];

    if (!ics->tns_data_present) return;

    for (w = 0; w < ics->num_windows; w++) {
        bottom = ics->num_swb;

        for (f = 0; f < tns->n_filt[w]; f++) {
            top = bottom;
            bottom = max(top - tns->length[w][f], 0);
            tns_order = min(tns->order[w][f], TNS_MAX_ORDER);
            if (!tns_order) continue;
            tns_decode_coef(tns_order, tns->coef_res[w] + 3, tns->coef_compress[w][f], tns->coef[w][f], lpc);
            start = min(bottom, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            start = min(start, ics->max_sfb);
            start = min(ics->swb_offset[start], ics->swb_offset_max);
            end = min(top, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            end = min(end, ics->max_sfb);
            end = min(ics->swb_offset[end], ics->swb_offset_max);
            size = end - start;
            if (size <= 0) continue;
            if (tns->direction[w][f]) {
                inc = -1;
                start = end - 1;
            } else {
                inc = 1;
            }
            tns_ar_filter(&spec[(w * nshort) + start], size, inc, lpc, tns_order);
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* TNS encoding for one channel and frame */
void tns_encode_frame(ic_stream* ics, tns_info* tns, uint8_t sr_index, uint8_t object_type, real_t* spec, uint16_t frame_len) {
    uint8_t  w, f, tns_order;
    int8_t   inc;
    int16_t  size;
    uint16_t bottom, top, start, end;
    uint16_t nshort = frame_len / 8;
    real_t   lpc[TNS_MAX_ORDER + 1];

    if (!ics->tns_data_present) return;
    for (w = 0; w < ics->num_windows; w++) {
        bottom = ics->num_swb;
        for (f = 0; f < tns->n_filt[w]; f++) {
            top = bottom;
            bottom = max(top - tns->length[w][f], 0);
            tns_order = min(tns->order[w][f], TNS_MAX_ORDER);
            if (!tns_order) continue;
            tns_decode_coef(tns_order, tns->coef_res[w] + 3, tns->coef_compress[w][f], tns->coef[w][f], lpc);
            start = min(bottom, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            start = min(start, ics->max_sfb);
            start = min(ics->swb_offset[start], ics->swb_offset_max);
            end = min(top, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            end = min(end, ics->max_sfb);
            end = min(ics->swb_offset[end], ics->swb_offset_max);
            size = end - start;
            if (size <= 0) continue;
            if (tns->direction[w][f]) {
                inc = -1;
                start = end - 1;
            } else {
                inc = 1;
            }
            tns_ma_filter(&spec[(w * nshort) + start], size, inc, lpc, tns_order);
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Decoder transmitted coefficients for one TNS filter */
void tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress, uint8_t* coef, real_t* a) {
    uint8_t i, m;
    real_t  tmp2[TNS_MAX_ORDER + 1], b[TNS_MAX_ORDER + 1];

    /* Conversion to signed integer */
    for (i = 0; i < order; i++) {
        if (coef_compress == 0) {
            if (coef_res_bits == 3) {
                tmp2[i] = tns_coef_0_3[coef[i]];
            } else {
                tmp2[i] = tns_coef_0_4[coef[i]];
            }
        } else {
            if (coef_res_bits == 3) {
                tmp2[i] = tns_coef_1_3[coef[i]];
            } else {
                tmp2[i] = tns_coef_1_4[coef[i]];
            }
        }
    }
    /* Conversion to LPC coefficients */
    a[0] = COEF_CONST(1.0);
    for (m = 1; m <= order; m++) {
        for (i = 1; i < m; i++) /* loop only while i<m */
            b[i] = a[i] + MUL_C(tmp2[m - 1], a[m - i]);

        for (i = 1; i < m; i++) /* loop only while i<m */
            a[i] = b[i];

        a[m] = tmp2[m - 1]; /* changed */
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void tns_ar_filter(real_t* spectrum, uint16_t size, int8_t inc, real_t* lpc, uint8_t order) {
    /*
     - Simple all-pole filter of order "order" defined by y(n) = x(n) - lpc[1]*y(n-1) - ... - lpc[order]*y(n-order)
     - The state variables of the filter are initialized to zero every time
     - The output data is written over the input data ("in-place operation")
     - An input vector of "size" samples is processed and the index increment to the next data sample is given by "inc"
    */

    uint8_t  j;
    uint16_t i;
    real_t   y;
    /* state is stored as a double ringbuffer */
    real_t state[2 * TNS_MAX_ORDER] = {0};
    int8_t state_index = 0;

    for (i = 0; i < size; i++) {
        y = *spectrum;
        for (j = 0; j < order; j++) y -= MUL_C(state[state_index + j], lpc[j + 1]);
        /* double ringbuffer state */
        state_index--;
        if (state_index < 0) state_index = order - 1;
        state[state_index] = state[state_index + order] = y;
        *spectrum = y;
        spectrum += inc;
// #define TNS_PRINT
#ifdef TNS_PRINT
        // printf("%d\n", y);
        printf("0x%.8X\n", y);
#endif
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void tns_ma_filter(real_t* spectrum, uint16_t size, int8_t inc, real_t* lpc, uint8_t order) {
    /*
     - Simple all-zero filter of order "order" defined by y(n) =  x(n) + a(2)*x(n-1) + ... + a(order+1)*x(n-order)
     - The state variables of the filter are initialized to zero every time
     - The output data is written over the input data ("in-place operation")
     - An input vector of "size" samples is processed and the index increment to the next data sample is given by "inc"
    */

    uint8_t  j;
    uint16_t i;
    real_t   y;
    /* state is stored as a double ringbuffer */
    real_t state[2 * TNS_MAX_ORDER] = {0};
    int8_t state_index = 0;

    for (i = 0; i < size; i++) {
        y = *spectrum;
        for (j = 0; j < order; j++) y += MUL_C(state[state_index + j], lpc[j + 1]);
        /* double ringbuffer state */
        state_index--;
        if (state_index < 0) state_index = order - 1;
        state[state_index] = state[state_index + order] = *spectrum;
        *spectrum = y;
        spectrum += inc;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.1 */
int8_t GASpecificConfig(bitfile* ld, mp4AudioSpecificConfig* mp4ASC, program_config* pce_out) {
    program_config pce;
    /* 1024 or 960 */
    mp4ASC->frameLengthFlag = faad_get1bit(ld);
#ifndef ALLOW_SMALL_FRAMELENGTH
    if (mp4ASC->frameLengthFlag == 1) return -3;
#endif
    mp4ASC->dependsOnCoreCoder = faad_get1bit(ld);
    if (mp4ASC->dependsOnCoreCoder == 1) { mp4ASC->coreCoderDelay = (uint16_t)faad_getbits(ld, 14); }
    mp4ASC->extensionFlag = faad_get1bit(ld);
    if (mp4ASC->channelsConfiguration == 0) {
        if (program_config_element(&pce, ld)) return -3;
        // mp4ASC->channelsConfiguration = pce.channels;
        if (pce_out != NULL) memcpy(pce_out, &pce, sizeof(program_config));
        /*
        if (pce.num_valid_cc_elements)
            return -3;
        */
    }
#ifdef ERROR_RESILIENCE
    if (mp4ASC->extensionFlag == 1) {
        /* Error resilience not supported yet */
        if (mp4ASC->objectTypeIndex >= ER_OBJECT_START) {
            mp4ASC->aacSectionDataResilienceFlag = faad_get1bit(ld);
            mp4ASC->aacScalefactorDataResilienceFlag = faad_get1bit(ld);
            mp4ASC->aacSpectralDataResilienceFlag = faad_get1bit(ld);
        }
        /* 1 bit: extensionFlag3 */
        faad_getbits(ld, 1);
    }
#endif
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.2 */
/* An MPEG-4 Audio decoder is only required to follow the Program Configuration Element in GASpecificConfig(). The decoder shall ignore
   any Program Configuration Elements that may occur in raw data blocks. PCEs transmitted in raw data blocks cannot be used to convey decoder
   configuration information.
*/
uint8_t program_config_element(program_config* pce, bitfile* ld) {
    uint8_t i;
    memset(pce, 0, sizeof(program_config));
    pce->channels = 0;
    pce->element_instance_tag = (uint8_t)faad_getbits(ld, 4);
    pce->object_type = (uint8_t)faad_getbits(ld, 2);
    pce->sf_index = (uint8_t)faad_getbits(ld, 4);
    pce->num_front_channel_elements = (uint8_t)faad_getbits(ld, 4);
    pce->num_side_channel_elements = (uint8_t)faad_getbits(ld, 4);
    pce->num_back_channel_elements = (uint8_t)faad_getbits(ld, 4);
    pce->num_lfe_channel_elements = (uint8_t)faad_getbits(ld, 2);
    pce->num_assoc_data_elements = (uint8_t)faad_getbits(ld, 3);
    pce->num_valid_cc_elements = (uint8_t)faad_getbits(ld, 4);
    pce->mono_mixdown_present = faad_get1bit(ld);
    if (pce->mono_mixdown_present == 1) { pce->mono_mixdown_element_number = (uint8_t)faad_getbits(ld, 4); }
    pce->stereo_mixdown_present = faad_get1bit(ld);
    if (pce->stereo_mixdown_present == 1) { pce->stereo_mixdown_element_number = (uint8_t)faad_getbits(ld, 4); }
    pce->matrix_mixdown_idx_present = faad_get1bit(ld);
    if (pce->matrix_mixdown_idx_present == 1) {
        pce->matrix_mixdown_idx = (uint8_t)faad_getbits(ld, 2);
        pce->pseudo_surround_enable = faad_get1bit(ld);
    }
    for (i = 0; i < pce->num_front_channel_elements; i++) {
        pce->front_element_is_cpe[i] = faad_get1bit(ld);
        pce->front_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        if (pce->front_element_is_cpe[i] & 1) {
            pce->cpe_channel[pce->front_element_tag_select[i]] = pce->channels;
            pce->num_front_channels += 2;
            pce->channels += 2;
        } else {
            pce->sce_channel[pce->front_element_tag_select[i]] = pce->channels;
            pce->num_front_channels++;
            pce->channels++;
        }
    }
    for (i = 0; i < pce->num_side_channel_elements; i++) {
        pce->side_element_is_cpe[i] = faad_get1bit(ld);
        pce->side_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        if (pce->side_element_is_cpe[i] & 1) {
            pce->cpe_channel[pce->side_element_tag_select[i]] = pce->channels;
            pce->num_side_channels += 2;
            pce->channels += 2;
        } else {
            pce->sce_channel[pce->side_element_tag_select[i]] = pce->channels;
            pce->num_side_channels++;
            pce->channels++;
        }
    }
    for (i = 0; i < pce->num_back_channel_elements; i++) {
        pce->back_element_is_cpe[i] = faad_get1bit(ld);
        pce->back_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        if (pce->back_element_is_cpe[i] & 1) {
            pce->cpe_channel[pce->back_element_tag_select[i]] = pce->channels;
            pce->channels += 2;
            pce->num_back_channels += 2;
        } else {
            pce->sce_channel[pce->back_element_tag_select[i]] = pce->channels;
            pce->num_back_channels++;
            pce->channels++;
        }
    }
    for (i = 0; i < pce->num_lfe_channel_elements; i++) {
        pce->lfe_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
        pce->sce_channel[pce->lfe_element_tag_select[i]] = pce->channels;
        pce->num_lfe_channels++;
        pce->channels++;
    }
    for (i = 0; i < pce->num_assoc_data_elements; i++) pce->assoc_data_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
    for (i = 0; i < pce->num_valid_cc_elements; i++) {
        pce->cc_element_is_ind_sw[i] = faad_get1bit(ld);
        pce->valid_cc_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4);
    }
    faad_byte_align(ld);
    pce->comment_field_bytes = (uint8_t)faad_getbits(ld, 8);
    for (i = 0; i < pce->comment_field_bytes; i++) { pce->comment_field_data[i] = (uint8_t)faad_getbits(ld, 8); }
    pce->comment_field_data[i] = 0;
    if (pce->channels > MAX_CHANNELS) return 22;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void decode_sce_lfe(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, uint8_t id_syn_ele) {
    uint8_t channels = hDecoder->fr_channels;
    uint8_t tag = 0;
    if (channels + 1 > MAX_CHANNELS) {
        hInfo->error = 12;
        return;
    }
    if (hDecoder->fr_ch_ele + 1 > MAX_SYNTAX_ELEMENTS) {
        hInfo->error = 13;
        return;
    }
    /* for SCE hDecoder->element_output_channels[] is not set here because this
       can become 2 when some form of Parametric Stereo coding is used
    */
    if (hDecoder->element_id[hDecoder->fr_ch_ele] != INVALID_ELEMENT_ID && hDecoder->element_id[hDecoder->fr_ch_ele] != id_syn_ele) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }
    /* save the syntax element id */
    hDecoder->element_id[hDecoder->fr_ch_ele] = id_syn_ele;
    /* decode the element */
    hInfo->error = single_lfe_channel_element(hDecoder, ld, channels, &tag);
    /* map output channels position to internal data channels */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2) {
        /* this might be faulty when pce_set is true */
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels + 1] = channels + 1;
    } else {
        if (hDecoder->pce_set)
            hDecoder->internal_channel[hDecoder->pce.sce_channel[tag]] = channels;
        else
            hDecoder->internal_channel[channels] = channels;
    }
    hDecoder->fr_channels += hDecoder->element_output_channels[hDecoder->fr_ch_ele];
    hDecoder->fr_ch_ele++;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void decode_cpe(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, uint8_t id_syn_ele) {
    uint8_t channels = hDecoder->fr_channels;
    uint8_t tag = 0;
    if (channels + 2 > MAX_CHANNELS) {
        hInfo->error = 12;
        return;
    }
    if (hDecoder->fr_ch_ele + 1 > MAX_SYNTAX_ELEMENTS) {
        hInfo->error = 13;
        return;
    }
    /* for CPE the number of output channels is always 2 */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0) {
        /* element_output_channels not set yet */
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = 2;
    } else if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] != 2) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }
    if (hDecoder->element_id[hDecoder->fr_ch_ele] != INVALID_ELEMENT_ID && hDecoder->element_id[hDecoder->fr_ch_ele] != id_syn_ele) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }
    /* save the syntax element id */
    hDecoder->element_id[hDecoder->fr_ch_ele] = id_syn_ele;
    /* decode the element */
    hInfo->error = channel_pair_element(hDecoder, ld, channels, &tag);
    /* map output channel position to internal data channels */
    if (hDecoder->pce_set) {
        hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag]] = channels;
        hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag] + 1] = channels + 1;
    } else {
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels + 1] = channels + 1;
    }
    hDecoder->fr_channels += 2;
    hDecoder->fr_ch_ele++;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void raw_data_block(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, program_config* pce, drc_info* drc) {
    uint8_t id_syn_ele;
    uint8_t ele_this_frame = 0;
    hDecoder->fr_channels = 0;
    hDecoder->fr_ch_ele = 0;
    hDecoder->first_syn_ele = 25;
    hDecoder->has_lfe = 0;
#ifdef ERROR_RESILIENCE
    if (hDecoder->object_type < ER_OBJECT_START) {
#endif
        /* Table 4.4.3: raw_data_block() */
        while ((id_syn_ele = (uint8_t)faad_getbits(ld, LEN_SE_ID)) != ID_END) {
            switch (id_syn_ele) {
                case ID_SCE:
                    ele_this_frame++;
                    if (hDecoder->first_syn_ele == 25) hDecoder->first_syn_ele = id_syn_ele;
                    decode_sce_lfe(hDecoder, hInfo, ld, id_syn_ele);
                    if (hInfo->error > 0) return;
                    break;
                case ID_CPE:
                    ele_this_frame++;
                    if (hDecoder->first_syn_ele == 25) hDecoder->first_syn_ele = id_syn_ele;
                    decode_cpe(hDecoder, hInfo, ld, id_syn_ele);
                    if (hInfo->error > 0) return;
                    break;
                case ID_LFE:
#ifdef DRM
                    hInfo->error = 32;
#else
                ele_this_frame++;
                hDecoder->has_lfe++;
                decode_sce_lfe(hDecoder, hInfo, ld, id_syn_ele);
#endif
                    if (hInfo->error > 0) return;
                    break;
                case ID_CCE: /* not implemented yet, but skip the bits */
#ifdef DRM
                    hInfo->error = 32;
#else
                ele_this_frame++;
    #ifdef COUPLING_DEC
                hInfo->error = coupling_channel_element(hDecoder, ld);
    #else
                hInfo->error = 6;
    #endif
#endif
                    if (hInfo->error > 0) return;
                    break;
                case ID_DSE:
                    ele_this_frame++;
                    data_stream_element(hDecoder, ld);
                    break;
                case ID_PCE:
                    if (ele_this_frame != 0) {
                        hInfo->error = 31;
                        return;
                    }
                    ele_this_frame++;
                    /* 14496-4: 5.6.4.1.2.1.3: */
                    /* program_configuration_element()'s in access units shall be ignored */
                    program_config_element(pce, ld);
                    // if ((hInfo->error = program_config_element(pce, ld)) > 0)
                    //     return;
                    // hDecoder->pce_set = 1;
                    break;
                case ID_FIL:
                    ele_this_frame++;
                    /* one sbr_info describes a channel_element not a channel! */
                    /* if we encounter SBR data here: error */
                    /* SBR data will be read directly in the SCE/LFE/CPE element */
                    if ((hInfo->error = fill_element(hDecoder, ld, drc
#ifdef SBR_DEC
                                                     ,
                                                     INVALID_SBR_ELEMENT
#endif
                                                     )) > 0)
                        return;
                    break;
            }
        }
#ifdef ERROR_RESILIENCE
    } else {
        /* Table 262: er_raw_data_block() */
        switch (hDecoder->channelConfiguration) {
            case 1:
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                break;
            case 2:
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                break;
            case 3:
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                break;
            case 4:
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                break;
            case 5:
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                break;
            case 6:
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_sce_lfe(hDecoder, hInfo, ld, ID_LFE);
                if (hInfo->error > 0) return;
                break;
            case 7: /* 8 channels */
                decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_cpe(hDecoder, hInfo, ld, ID_CPE);
                if (hInfo->error > 0) return;
                decode_sce_lfe(hDecoder, hInfo, ld, ID_LFE);
                if (hInfo->error > 0) return;
                break;
            default: hInfo->error = 7; return;
        }
    #if 0
        cnt = bits_to_decode() / 8;
        while (cnt >= 1)
        {
            cnt -= extension_payload(cnt);
        }
    #endif
    }
#endif
    /* new in corrigendum 14496-3:2002 */
#ifdef DRM
    if (hDecoder->object_type != DRM_ER_LC
    #if 0
        && !hDecoder->latm_header_present
    #endif
    )
#endif
    {
        faad_byte_align(ld);
    }
    return;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.4 and */
/* Table 4.4.9 */
uint8_t single_lfe_channel_element(NeAACDecStruct* hDecoder, bitfile* ld, uint8_t channel, uint8_t* tag) {
    uint8_t retval = 0;
    //  element       sce = {0};
    element*   sce = (element*)ps_calloc(1, sizeof(element));
    ic_stream* ics = &(sce->ics1);
    // ALIGN int16_t spec_data[1024] = {0};
    int16_t* spec_data = ps_calloc(1024, sizeof(int16_t));
    sce->element_instance_tag = (uint8_t)faad_getbits(ld, LEN_TAG);
    *tag = sce->element_instance_tag;
    sce->channel = channel;
    sce->paired_channel = -1;
    retval = individual_channel_stream(hDecoder, sce, ld, ics, 0, spec_data);
    if (retval > 0) goto exit;
    /* IS not allowed in single channel */
    if (ics->is_used) {
        retval = 32;
        goto exit;
    }
#ifdef SBR_DEC
    /* check if next bitstream element is a fill element */
    /* if so, read it now so SBR decoding can be done in case of a file with SBR */
    if (faad_showbits(ld, LEN_SE_ID) == ID_FIL) {
        faad_flushbits(ld, LEN_SE_ID);
        /* one sbr_info describes a channel_element not a channel! */
        if ((retval = fill_element(hDecoder, ld, hDecoder->drc, hDecoder->fr_ch_ele)) > 0) { goto exit; }
    }
#endif
    /* noiseless coding is done, spectral reconstruction is done now */
    retval = reconstruct_single_channel(hDecoder, ics, sce, spec_data);
    if (retval > 0) goto exit;
    retval = 0;
exit:
    if (spec_data) free(spec_data);
    if (sce) free(sce);
    return retval;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.5 */
uint8_t channel_pair_element(NeAACDecStruct* hDecoder, bitfile* ld, uint8_t channels, uint8_t* tag) {
    // ALIGN int16_t spec_data1[1024] = {0};
    // ALIGN int16_t spec_data2[1024] = {0};
    int16_t* spec_data1 = (int16_t*)ps_calloc(1024, sizeof(int16_t));
    int16_t* spec_data2 = (int16_t*)ps_calloc(1024, sizeof(int16_t));
    // element    cpe = {0};
    element*   cpe = (element*)ps_calloc(1, sizeof(element));
    ic_stream* ics1 = &(cpe->ics1);
    ic_stream* ics2 = &(cpe->ics2);
    uint8_t    result;
    cpe->channel = channels;
    cpe->paired_channel = channels + 1;
    cpe->element_instance_tag = (uint8_t)faad_getbits(ld, LEN_TAG);
    *tag = cpe->element_instance_tag;
    if ((cpe->common_window = faad_get1bit(ld) & 1)) {
        /* both channels have common ics information */
        if ((result = ics_info(hDecoder, ics1, ld, cpe->common_window)) > 0) goto exit;
        ics1->ms_mask_present = (uint8_t)faad_getbits(ld, 2);
        if (ics1->ms_mask_present == 3) {
            /* bitstream error */
            result = 32;
            goto exit;
        }
        if (ics1->ms_mask_present == 1) {
            uint8_t g, sfb;
            for (g = 0; g < ics1->num_window_groups; g++) {
                for (sfb = 0; sfb < ics1->max_sfb; sfb++) { ics1->ms_used[g][sfb] = faad_get1bit(ld); }
            }
        }
#ifdef ERROR_RESILIENCE
        if ((hDecoder->object_type >= ER_OBJECT_START) && (ics1->predictor_data_present)) {
            if ((
    #ifdef LTP_DEC
                    ics1->ltp.data_present =
    #endif
                        faad_get1bit(ld)) &
                1) {
    #ifdef LTP_DEC
                if ((result = ltp_data(hDecoder, ics1, &(ics1->ltp), ld)) > 0) { goto exit; }
    #else
                result = 26;
                goto exit; // return 26;
    #endif
            }
        }
#endif
        memcpy(ics2, ics1, sizeof(ic_stream));
    } else {
        ics1->ms_mask_present = 0;
    }
    if ((result = individual_channel_stream(hDecoder, cpe, ld, ics1, 0, spec_data1)) > 0) { goto exit; }
#ifdef ERROR_RESILIENCE
    if (cpe->common_window && (hDecoder->object_type >= ER_OBJECT_START) && (ics1->predictor_data_present)) {
        if ((
    #ifdef LTP_DEC
                ics1->ltp2.data_present =
    #endif
                    faad_get1bit(ld)) &
            1) {
    #ifdef LTP_DEC
            if ((result = ltp_data(hDecoder, ics1, &(ics1->ltp2), ld)) > 0) { goto exit; }
    #else
            result = 26;
            goto exit; // return 26;
    #endif
        }
    }
#endif
    if ((result = individual_channel_stream(hDecoder, cpe, ld, ics2, 0, spec_data2)) > 0) { goto exit; }
#ifdef SBR_DEC
    /* check if next bitstream element is a fill element */
    /* if so, read it now so SBR decoding can be done in case of a file with SBR */
    if (faad_showbits(ld, LEN_SE_ID) == ID_FIL) {
        faad_flushbits(ld, LEN_SE_ID);
        /* one sbr_info describes a channel_element not a channel! */
        if ((result = fill_element(hDecoder, ld, hDecoder->drc, hDecoder->fr_ch_ele)) > 0) { goto exit; }
    }
#endif
    /* noiseless coding is done, spectral reconstruction is done now */
    if ((result = reconstruct_channel_pair(hDecoder, ics1, ics2, cpe, spec_data1, spec_data2)) > 0) { goto exit; }
    result = 0;
exit:
    if (spec_data1) {
        free(spec_data1);
        spec_data1 = NULL;
    }
    if (spec_data2) {
        free(spec_data2);
        spec_data2 = NULL;
    }
    if (cpe) free(cpe);
    return result;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.6 */
uint8_t ics_info(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, uint8_t common_window) {
    uint8_t retval = 0;
    uint8_t ics_reserved_bit;
    ics_reserved_bit = faad_get1bit(ld);
    if (ics_reserved_bit != 0) return 32;
    ics->window_sequence = (uint8_t)faad_getbits(ld, 2);
    ics->window_shape = faad_get1bit(ld);
#ifdef LD_DEC
    /* No block switching in LD */
    if ((hDecoder->object_type == LD) && (ics->window_sequence != ONLY_LONG_SEQUENCE)) return 32;
#endif
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        ics->max_sfb = (uint8_t)faad_getbits(ld, 4);
        ics->scale_factor_grouping = (uint8_t)faad_getbits(ld, 7);
    } else {
        ics->max_sfb = (uint8_t)faad_getbits(ld, 6);
    }
    /* get the grouping information */
    if ((retval = window_grouping_info(hDecoder, ics)) > 0) return retval;
    /* should be an error */
    /* check the range of max_sfb */
    if (ics->max_sfb > ics->num_swb) return 16;
    if (ics->window_sequence != EIGHT_SHORT_SEQUENCE) {
        if ((ics->predictor_data_present = faad_get1bit(ld)) & 1) {
            if (hDecoder->object_type == MAIN) /* MPEG2 style AAC predictor */
            {
                uint8_t sfb;
                uint8_t limit = min(ics->max_sfb, max_pred_sfb(hDecoder->sf_index));
#ifdef MAIN_DEC
                ics->pred.limit = limit;
#endif
                if ((
#ifdef MAIN_DEC
                        ics->pred.predictor_reset =
#endif
                            faad_get1bit(ld)) &
                    1) {
#ifdef MAIN_DEC
                    ics->pred.predictor_reset_group_number =
#endif
                        faad_getbits(ld, 5);
                }
                for (sfb = 0; sfb < limit; sfb++) {
#ifdef MAIN_DEC
                    ics->pred.prediction_used[sfb] =
#endif
                        faad_get1bit(ld);
                }
            }
#ifdef LTP_DEC
            else { /* Long Term Prediction */
                if (hDecoder->object_type < ER_OBJECT_START) {
                    if ((ics->ltp.data_present = faad_get1bit(ld)) & 1) {
                        if ((retval = ltp_data(hDecoder, ics, &(ics->ltp), ld)) > 0) { return retval; }
                    }
                    if (common_window) {
                        if ((ics->ltp2.data_present = faad_get1bit(ld)) & 1) {
                            if ((retval = ltp_data(hDecoder, ics, &(ics->ltp2), ld)) > 0) { return retval; }
                        }
                    }
                }
    #ifdef ERROR_RESILIENCE
                if (!common_window && (hDecoder->object_type >= ER_OBJECT_START)) {
                    if ((ics->ltp.data_present = faad_get1bit(ld)) & 1) {
                        if ((retval = ltp_data(hDecoder, ics, &(ics->ltp), ld)) > 0) { return retval; }
                    }
                }
    #endif
            }
#endif
        }
    }
    return retval;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.7 */
uint8_t pulse_data(ic_stream* ics, pulse_info* pul, bitfile* ld) {
    uint8_t i;
    pul->number_pulse = (uint8_t)faad_getbits(ld, 2);
    pul->pulse_start_sfb = (uint8_t)faad_getbits(ld, 6);
    /* check the range of pulse_start_sfb */
    if (pul->pulse_start_sfb > ics->num_swb) return 16;
    for (i = 0; i < pul->number_pulse + 1; i++) {
        pul->pulse_offset[i] = (uint8_t)faad_getbits(ld, 5);
#if 0
        printf("%d\n", pul->pulse_offset[i]);
#endif
        pul->pulse_amp[i] = (uint8_t)faad_getbits(ld, 4);
#if 0
        printf("%d\n", pul->pulse_amp[i]);
#endif
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef COUPLING_DEC
/* Table 4.4.8: Currently just for skipping the bits... */
uint8_t coupling_channel_element(NeAACDecStruct* hDecoder, bitfile* ld) {
    uint8_t   c, result = 0;
    uint8_t   ind_sw_cce_flag = 0;
    uint8_t   num_gain_element_lists = 0;
    uint8_t   num_coupled_elements = 0;
    element   el_empty = {0};
    ic_stream ics_empty = {0};
    int16_t   sh_data[1024];
    c = faad_getbits(ld, LEN_TAG);
    ind_sw_cce_flag = faad_get1bit(ld);
    num_coupled_elements = faad_getbits(ld, 3);
    for (c = 0; c < num_coupled_elements + 1; c++) {
        uint8_t cc_target_is_cpe, cc_target_tag_select;
        num_gain_element_lists++;
        cc_target_is_cpe = faad_get1bit(ld);
        cc_target_tag_select = faad_getbits(ld, 4);
        if (cc_target_is_cpe) {
            uint8_t cc_l = faad_get1bit(ld);
            uint8_t cc_r = faad_get1bit(ld);
            if (cc_l && cc_r) num_gain_element_lists++;
        }
    }
    faad_get1bit(ld);
    faad_get1bit(ld);
    faad_getbits(ld, 2);
    if ((result = individual_channel_stream(hDecoder, &el_empty, ld, &ics_empty, 0, sh_data)) > 0) { return result; }
    /* IS not allowed in single channel */
    if (ics->is_used) return 32;
    for (c = 1; c < num_gain_element_lists; c++) {
        uint8_t cge;
        if (ind_sw_cce_flag) {
            cge = 1;
        } else {
            cge = faad_get1bit(ld);
        }
        if (cge) {
            huffman_scale_factor(ld);
        } else {
            uint8_t g, sfb;
            for (g = 0; g < ics_empty.num_window_groups; g++) {
                for (sfb = 0; sfb < ics_empty.max_sfb; sfb++) {
                    if (ics_empty.sfb_cb[g][sfb] != ZERO_HCB) huffman_scale_factor(ld);
                }
            }
        }
    }
    return 0;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.10 */
uint16_t data_stream_element(NeAACDecStruct* hDecoder, bitfile* ld) {
    uint8_t  byte_aligned;
    uint16_t i, count;
    /* element_instance_tag = */ faad_getbits(ld, LEN_TAG);
    byte_aligned = faad_get1bit(ld);
    count = (uint16_t)faad_getbits(ld, 8);
    if (count == 255) { count += (uint16_t)faad_getbits(ld, 8); }
    if (byte_aligned) faad_byte_align(ld);
    for (i = 0; i < count; i++) { faad_getbits(ld, LEN_BYTE); }
    return count;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.11 */
uint8_t fill_element(NeAACDecStruct* hDecoder, bitfile* ld, drc_info* drc
#ifdef SBR_DEC
                            ,
                            uint8_t sbr_ele
#endif
) {
    uint16_t count;
#ifdef SBR_DEC
    uint8_t bs_extension_type;
#endif
    count = (uint16_t)faad_getbits(ld, 4);
    if (count == 15) { count += (uint16_t)faad_getbits(ld, 8) - 1; }
    if (count > 0) {
#ifdef SBR_DEC
        bs_extension_type = (uint8_t)faad_showbits(ld, 4);
        if ((bs_extension_type == EXT_SBR_DATA) || (bs_extension_type == EXT_SBR_DATA_CRC)) {
            if (sbr_ele == INVALID_SBR_ELEMENT) return 24;
            if (!hDecoder->sbr[sbr_ele]) {
                hDecoder->sbr[sbr_ele] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[sbr_ele], 2 * get_sample_rate(hDecoder->sf_index), hDecoder->downSampledSBR, 0);
            }
            if (!hDecoder->sbr[sbr_ele]) return 19;
            hDecoder->sbr_present_flag = 1;
            /* parse the SBR data */
            hDecoder->sbr[sbr_ele]->ret = sbr_extension_data(ld, hDecoder->sbr[sbr_ele], count, hDecoder->postSeekResetFlag);
    #if 0
            if (hDecoder->sbr[sbr_ele]->ret > 0)
            {
                printf("%s\n", NeAACDecGetErrorMessage(hDecoder->sbr[sbr_ele]->ret));
            }
    #endif
    #if (defined(PS_DEC) || defined(DRM_PS))
            if (hDecoder->sbr[sbr_ele]->ps_used) {
                hDecoder->ps_used[sbr_ele] = 1;
                /* set element independent flag to 1 as well */
                hDecoder->ps_used_global = 1;
            }
    #endif
        } else {
#endif
#ifndef DRM
            while (count > 0) { count -= extension_payload(ld, drc, count); }
#else
        return 30;
#endif
#ifdef SBR_DEC
        }
#endif
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.12 */
#ifdef SSR_DEC
static void gain_control_data(bitfile* ld, ic_stream* ics) {
    uint8_t   bd, wd, ad;
    ssr_info* ssr = &(ics->ssr);
    ssr->max_band = (uint8_t)faad_getbits(ld, 2);
    if (ics->window_sequence == ONLY_LONG_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++) {
            for (wd = 0; wd < 1; wd++) {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3);
                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++) {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4);
                    ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 5);
                }
            }
        }
    } else if (ics->window_sequence == LONG_START_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++) {
            for (wd = 0; wd < 2; wd++) {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3);
                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++) {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4);
                    if (wd == 0) {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4);
                    } else {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 2);
                    }
                }
            }
        }
    } else if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++) {
            for (wd = 0; wd < 8; wd++) {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3);
                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++) {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4);
                    ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 2);
                }
            }
        }
    } else if (ics->window_sequence == LONG_STOP_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++) {
            for (wd = 0; wd < 2; wd++) {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3);
                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++) {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4);
                    if (wd == 0) {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4);
                    } else {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 5);
                    }
                }
            }
        }
    }
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM
/* Table 4.4.13 ASME */
void DRM_aac_scalable_main_element(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, program_config* pce, drc_info* drc) {
    uint8_t retval = 0;
    (void)retval;
    uint8_t channels = hDecoder->fr_channels = 0;
    uint8_t ch;
    (void)ch;
    uint8_t    this_layer_stereo = (hDecoder->channelConfiguration > 1) ? 1 : 0;
    element    cpe = {0};
    ic_stream* ics1 = &(cpe.ics1);
    ic_stream* ics2 = &(cpe.ics2);
    int16_t*   spec_data;
    (void)spec_data;
    ALIGN int16_t spec_data1[1024] = {0};
    ALIGN int16_t spec_data2[1024] = {0};
    hDecoder->fr_ch_ele = 0;
    hInfo->error = DRM_aac_scalable_main_header(hDecoder, ics1, ics2, ld, this_layer_stereo);
    if (hInfo->error > 0) return;
    cpe.common_window = 1;
    if (this_layer_stereo) {
        hDecoder->element_id[0] = ID_CPE;
        if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0) hDecoder->element_output_channels[hDecoder->fr_ch_ele] = 2;
    } else {
        hDecoder->element_id[0] = ID_SCE;
    }
    if (this_layer_stereo) {
        cpe.channel = 0;
        cpe.paired_channel = 1;
    }
    /* Stereo2 / Mono1 */
    ics1->tns_data_present = faad_get1bit(ld);
    #if defined(LTP_DEC)
    ics1->ltp.data_present = faad_get1bit(ld);
    #elif defined(DRM)
    if (faad_get1bit(ld)) {
        hInfo->error = 26;
        return;
    }
    #else
    faad_get1bit(ld);
    #endif
    hInfo->error = side_info(hDecoder, &cpe, ld, ics1, 1);
    if (hInfo->error > 0) return;
    if (this_layer_stereo) {
        /* Stereo3 */
        ics2->tns_data_present = faad_get1bit(ld);
    #ifdef LTP_DEC
        ics1->ltp.data_present =
    #endif
            faad_get1bit(ld);
        hInfo->error = side_info(hDecoder, &cpe, ld, ics2, 1);
        if (hInfo->error > 0) return;
    }
    /* Stereo4 / Mono2 */
    if (ics1->tns_data_present) tns_data(ics1, &(ics1->tns), ld);
    if (this_layer_stereo) {
        /* Stereo5 */
        if (ics2->tns_data_present) tns_data(ics2, &(ics2->tns), ld);
    }
    #ifdef DRM
    /* CRC check */
    if (hDecoder->object_type == DRM_ER_LC) {
        if ((hInfo->error = (uint8_t)faad_check_CRC(ld, (uint16_t)faad_get_processed_bits(ld) - 8)) > 0) return;
    }
    #endif
    /* Stereo6 / Mono3 */
    /* error resilient spectral data decoding */
    if ((hInfo->error = reordered_spectral_data(hDecoder, ics1, ld, spec_data1)) > 0) { return; }
    if (this_layer_stereo) {
        /* Stereo7 */
        /* error resilient spectral data decoding */
        if ((hInfo->error = reordered_spectral_data(hDecoder, ics2, ld, spec_data2)) > 0) { return; }
    }
    #ifdef DRM
        #ifdef SBR_DEC
    /* In case of DRM we need to read the SBR info before channel reconstruction */
    if ((hDecoder->sbr_present_flag == 1) && (hDecoder->object_type == DRM_ER_LC)) {
        bitfile  ld_sbr = {0};
        uint32_t i;
        uint16_t count = 0;
        uint8_t* revbuffer;
        uint8_t* prevbufstart;
        uint8_t* pbufend;
        /* all forward bitreading should be finished at this point */
        uint32_t bitsconsumed = faad_get_processed_bits(ld);
        uint32_t buffer_size = faad_origbitbuffer_size(ld);
        uint8_t* buffer = (uint8_t*)faad_origbitbuffer(ld);
        if (bitsconsumed + 8 > buffer_size * 8) {
            hInfo->error = 14;
            return;
        }
        if (!hDecoder->sbr[0]) { hDecoder->sbr[0] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[0], 2 * get_sample_rate(hDecoder->sf_index), 0 /* ds SBR */, 1); }
        if (!hDecoder->sbr[0]) {
            hInfo->error = 19;
            return;
        }
        /* Reverse bit reading of SBR data in DRM audio frame */
        revbuffer = (uint8_t*)faad_malloc(buffer_size * sizeof(uint8_t));
        prevbufstart = revbuffer;
        pbufend = &buffer[buffer_size - 1];
        for (i = 0; i < buffer_size; i++) *prevbufstart++ = tabFlipbits[*pbufend--];
        /* Set SBR data */
        /* consider 8 bits from AAC-CRC */
        /* SBR buffer size is original buffer size minus AAC buffer size */
        count = (uint16_t)bit2byte(buffer_size * 8 - bitsconsumed);
        faad_initbits(&ld_sbr, revbuffer, count);
        hDecoder->sbr[0]->sample_rate = get_sample_rate(hDecoder->sf_index);
        hDecoder->sbr[0]->sample_rate *= 2;
        faad_getbits(&ld_sbr, 8); /* Skip 8-bit CRC */
        hDecoder->sbr[0]->ret = sbr_extension_data(&ld_sbr, hDecoder->sbr[0], count, hDecoder->postSeekResetFlag);
            #if (defined(PS_DEC) || defined(DRM_PS))
        if (hDecoder->sbr[0]->ps_used) {
            hDecoder->ps_used[0] = 1;
            hDecoder->ps_used_global = 1;
        }
            #endif
        if (ld_sbr.error) { hDecoder->sbr[0]->ret = 1; }
        /* check CRC */
        /* no need to check it if there was already an error */
        if (hDecoder->sbr[0]->ret == 0) hDecoder->sbr[0]->ret = (uint8_t)faad_check_CRC(&ld_sbr, (uint16_t)faad_get_processed_bits(&ld_sbr) - 8);
        /* SBR data was corrupted, disable it until the next header */
        if (hDecoder->sbr[0]->ret != 0) { hDecoder->sbr[0]->header_count = 0; }
        faad_endbits(&ld_sbr);
        if (revbuffer) faad_free(revbuffer);
    }
        #endif
    #endif
    if (this_layer_stereo) {
        hInfo->error = reconstruct_channel_pair(hDecoder, ics1, ics2, &cpe, spec_data1, spec_data2);
        if (hInfo->error > 0) return;
    } else {
        hInfo->error = reconstruct_single_channel(hDecoder, ics1, &cpe, spec_data1);
        if (hInfo->error > 0) return;
    }
    /* map output channels position to internal data channels */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2) {
        /* this might be faulty when pce_set is true */
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels + 1] = channels + 1;
    } else {
        hDecoder->internal_channel[channels] = channels;
    }
    hDecoder->fr_channels += hDecoder->element_output_channels[hDecoder->fr_ch_ele];
    hDecoder->fr_ch_ele++;
    return;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.15 */
int8_t DRM_aac_scalable_main_header(NeAACDecStruct* hDecoder, ic_stream* ics1, ic_stream* ics2, bitfile* ld, uint8_t this_layer_stereo) {
    uint8_t retval = 0;
    uint8_t ch;
    (void)ch;
    ic_stream* ics;
    (void)ics;
    uint8_t ics_reserved_bit;
    ics_reserved_bit = faad_get1bit(ld);
    if (ics_reserved_bit != 0) return 32;
    ics1->window_sequence = (uint8_t)faad_getbits(ld, 2);
    ics1->window_shape = faad_get1bit(ld);
    if (ics1->window_sequence == EIGHT_SHORT_SEQUENCE) {
        ics1->max_sfb = (uint8_t)faad_getbits(ld, 4);
        ics1->scale_factor_grouping = (uint8_t)faad_getbits(ld, 7);
    } else {
        ics1->max_sfb = (uint8_t)faad_getbits(ld, 6);
    }
    /* get the grouping information */
    if ((retval = window_grouping_info(hDecoder, ics1)) > 0) return retval;
    /* should be an error */
    /* check the range of max_sfb */
    if (ics1->max_sfb > ics1->num_swb) return 16;
    if (this_layer_stereo) {
        ics1->ms_mask_present = (uint8_t)faad_getbits(ld, 2);
        if (ics1->ms_mask_present == 3) {
            /* bitstream error */
            return 32;
        }
        if (ics1->ms_mask_present == 1) {
            uint8_t g, sfb;
            for (g = 0; g < ics1->num_window_groups; g++) {
                for (sfb = 0; sfb < ics1->max_sfb; sfb++) { ics1->ms_used[g][sfb] = faad_get1bit(ld); }
            }
        }
        memcpy(ics2, ics1, sizeof(ic_stream));
    } else {
        ics1->ms_mask_present = 0;
    }
    return 0;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t side_info(NeAACDecStruct* hDecoder, element* ele, bitfile* ld, ic_stream* ics, uint8_t scal_flag) {
    uint8_t result;
    ics->global_gain = (uint8_t)faad_getbits(ld, 8);
    if (!ele->common_window && !scal_flag) {
        if ((result = ics_info(hDecoder, ics, ld, ele->common_window)) > 0) return result;
    }
    if ((result = section_data(hDecoder, ics, ld)) > 0) return result;
    if ((result = scale_factor_data(hDecoder, ics, ld)) > 0) return result;
    if (!scal_flag) {
        /**
         **  NOTE: It could be that pulse data is available in scalable AAC too,
         **        as said in Amendment 1, this could be only the case for ER AAC,
         **        though. (have to check this out later)
         **/
        /* get pulse data */
        if ((ics->pulse_data_present = faad_get1bit(ld)) & 1) {
            if ((result = pulse_data(ics, &(ics->pul), ld)) > 0) return result;
        }
        /* get tns data */
        if ((ics->tns_data_present = faad_get1bit(ld)) & 1) {
#ifdef ERROR_RESILIENCE
            if (hDecoder->object_type < ER_OBJECT_START)
#endif
                tns_data(ics, &(ics->tns), ld);
        }
        /* get gain control data */
        if ((ics->gain_control_data_present = faad_get1bit(ld)) & 1) {
#ifdef SSR_DEC
            if (hDecoder->object_type != SSR)
                return 1;
            else
                gain_control_data(ld, ics);
#else
            return 1;
#endif
        }
    }
#ifdef ERROR_RESILIENCE
    if (hDecoder->aacSpectralDataResilienceFlag) {
        ics->length_of_reordered_spectral_data = (uint16_t)faad_getbits(ld, 14);
        if (hDecoder->channelConfiguration == 2) {
            if (ics->length_of_reordered_spectral_data > 6144) ics->length_of_reordered_spectral_data = 6144;
        } else {
            if (ics->length_of_reordered_spectral_data > 12288) ics->length_of_reordered_spectral_data = 12288;
        }
        ics->length_of_longest_codeword = (uint8_t)faad_getbits(ld, 6);
        if (ics->length_of_longest_codeword >= 49) ics->length_of_longest_codeword = 49;
    }
    /* RVLC spectral data is put here */
    if (hDecoder->aacScalefactorDataResilienceFlag) {
        if ((result = rvlc_decode_scale_factors(ics, ld)) > 0) return result;
    }
#endif
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.24 */
uint8_t individual_channel_stream(NeAACDecStruct* hDecoder, element* ele, bitfile* ld, ic_stream* ics, uint8_t scal_flag, int16_t* spec_data) {
    uint8_t result;
    result = side_info(hDecoder, ele, ld, ics, scal_flag);
    if (result > 0) return result;
    if (hDecoder->object_type >= ER_OBJECT_START) {
        if (ics->tns_data_present) tns_data(ics, &(ics->tns), ld);
    }
#ifdef DRM
    /* CRC check */
    if (hDecoder->object_type == DRM_ER_LC) {
        if ((result = (uint8_t)faad_check_CRC(ld, (uint16_t)faad_get_processed_bits(ld) - 8)) > 0) return result;
    }
#endif
#ifdef ERROR_RESILIENCE
    if (hDecoder->aacSpectralDataResilienceFlag) {
        /* error resilient spectral data decoding */
        if ((result = reordered_spectral_data(hDecoder, ics, ld, spec_data)) > 0) { return result; }
    } else {
#endif
        /* decode the spectral data */
        if ((result = spectral_data(hDecoder, ics, ld, spec_data)) > 0) { return result; }
#ifdef ERROR_RESILIENCE
    }
#endif
    /* pulse coding reconstruction */
    if (ics->pulse_data_present) {
        if (ics->window_sequence != EIGHT_SHORT_SEQUENCE) {
            if ((result = pulse_decode(ics, spec_data, hDecoder->frameLength)) > 0) return result;
        } else {
            return 2; /* pulse coding not allowed for short blocks */
        }
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.25 */
uint8_t section_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld) {
    uint8_t g;
    uint8_t sect_esc_val, sect_bits;
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
        sect_bits = 3;
    else
        sect_bits = 5;
    sect_esc_val = (1 << sect_bits) - 1;
#if 0
    printf("\ntotal sfb %d\n", ics->max_sfb);
    printf("   sect    top     cb\n");
#endif
    for (g = 0; g < ics->num_window_groups; g++) {
        uint8_t k = 0;
        uint8_t i = 0;
        while (k < ics->max_sfb) {
#ifdef ERROR_RESILIENCE
            uint8_t vcb11 = 0;
#endif
            uint8_t  sfb;
            uint8_t  sect_len_incr;
            uint16_t sect_len = 0;
            uint8_t  sect_cb_bits = 4;
            /* if "faad_getbits" detects error and returns "0", "k" is never
               incremented and we cannot leave the while loop */
            if (ld->error != 0) return 14;
#ifdef ERROR_RESILIENCE
            if (hDecoder->aacSectionDataResilienceFlag) sect_cb_bits = 5;
#endif
            ics->sect_cb[g][i] = (uint8_t)faad_getbits(ld, sect_cb_bits);
            if (ics->sect_cb[g][i] == 12) return 32;
#if 0
            printf("%d\n", ics->sect_cb[g][i]);
#endif
#ifndef DRM
            if (ics->sect_cb[g][i] == NOISE_HCB) ics->noise_used = 1;
#else
            /* PNS not allowed in DRM */
            if (ics->sect_cb[g][i] == NOISE_HCB) return 29;
#endif
            if (ics->sect_cb[g][i] == INTENSITY_HCB2 || ics->sect_cb[g][i] == INTENSITY_HCB) ics->is_used = 1;
#ifdef ERROR_RESILIENCE
            if (hDecoder->aacSectionDataResilienceFlag) {
                if ((ics->sect_cb[g][i] == 11) || ((ics->sect_cb[g][i] >= 16) && (ics->sect_cb[g][i] <= 32))) { vcb11 = 1; }
            }
            if (vcb11) {
                sect_len_incr = 1;
            } else {
#endif
                sect_len_incr = (uint8_t)faad_getbits(ld, sect_bits);
#ifdef ERROR_RESILIENCE
            }
#endif
            while ((sect_len_incr == sect_esc_val) /* &&
                (k+sect_len < ics->max_sfb)*/)
            {
                sect_len += sect_len_incr;
                sect_len_incr = (uint8_t)faad_getbits(ld, sect_bits);
            }
            sect_len += sect_len_incr;
            ics->sect_start[g][i] = k;
            ics->sect_end[g][i] = k + sect_len;
#if 0
            printf("%d\n", ics->sect_start[g][i]);
#endif
#if 0
            printf("%d\n", ics->sect_end[g][i]);
#endif
            if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
                if (k + sect_len > 8 * 15) return 15;
                if (i >= 8 * 15) return 15;
            } else {
                if (k + sect_len > MAX_SFB) return 15;
                if (i >= MAX_SFB) return 15;
            }
            for (sfb = k; sfb < k + sect_len; sfb++) {
                ics->sfb_cb[g][sfb] = ics->sect_cb[g][i];
#if 0
                printf("%d\n", ics->sfb_cb[g][sfb]);
#endif
            }
#if 0
            printf(" %6d %6d %6d\n",
                i,
                ics->sect_end[g][i],
                ics->sect_cb[g][i]);
#endif
            k += sect_len;
            i++;
        }
        ics->num_sec[g] = i;
        /* the sum of all sect_len_incr elements for a given window
         * group shall equal max_sfb */
        if (k != ics->max_sfb) { return 32; }
#if 0
        printf("%d\n", ics->num_sec[g]);
#endif
    }
#if 0
    printf("\n");
#endif
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*  decode_scale_factors() decodes the scalefactors from the bitstream
 * All scalefactors (and also the stereo positions and pns energies) are transmitted using Huffman coded DPCM relative to the previous active
 * scalefactor (respectively previous stereo position or previous pns energy, see subclause 4.6.2 and 4.6.3). The first active scalefactor is
 * differentially coded relative to the global gain.
 */
uint8_t decode_scale_factors(ic_stream* ics, bitfile* ld) {
    uint8_t g, sfb;
    int16_t t;
    int8_t  noise_pcm_flag = 1;
    (void)noise_pcm_flag;
    int16_t scale_factor = ics->global_gain;
    int16_t is_position = 0;
    int16_t noise_energy = ics->global_gain - 90;
    (void)noise_energy;
    for (g = 0; g < ics->num_window_groups; g++) {
        for (sfb = 0; sfb < ics->max_sfb; sfb++) {
            switch (ics->sfb_cb[g][sfb]) {
                case ZERO_HCB: /* zero book */
                    ics->scale_factors[g][sfb] = 0;
// #define SF_PRINT
#ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
#endif
                    break;
                case INTENSITY_HCB: /* intensity books */
                case INTENSITY_HCB2:
                    /* decode intensity position */
                    t = huffman_scale_factor(ld);
                    is_position += (t - 60);
                    ics->scale_factors[g][sfb] = is_position;
#ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
#endif
                    break;
                case NOISE_HCB: /* noise books */
#ifndef DRM
                    /* decode noise energy */
                    if (noise_pcm_flag) {
                        noise_pcm_flag = 0;
                        t = (int16_t)faad_getbits(ld, 9) - 256;
                    } else {
                        t = huffman_scale_factor(ld);
                        t -= 60;
                    }
                    noise_energy += t;
                    ics->scale_factors[g][sfb] = noise_energy;
    #ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
    #endif
#else
                    /* PNS not allowed in DRM */
                    return 29;
#endif
                    break;
                default: /* spectral books */
                    /* ics->scale_factors[g][sfb] must be between 0 and 255 */
                    ics->scale_factors[g][sfb] = 0;
                    /* decode scale factor */
                    t = huffman_scale_factor(ld);
                    scale_factor += (t - 60);
                    if (scale_factor < 0 || scale_factor > 255) return 4;
                    ics->scale_factors[g][sfb] = scale_factor;
#ifdef SF_PRINT
                    printf("%d\n", ics->scale_factors[g][sfb]);
#endif
                    break;
            }
        }
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.26 */
uint8_t scale_factor_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld) {
    uint8_t ret = 0;
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
#ifdef ERROR_RESILIENCE
    if (!hDecoder->aacScalefactorDataResilienceFlag) {
#endif
        ret = decode_scale_factors(ics, ld);
#ifdef ERROR_RESILIENCE
    } else {
        /* In ER AAC the parameters for RVLC are seperated from the actual
           data that holds the scale_factors.
           Strangely enough, 2 parameters for HCR are put inbetween them.
        */
        ret = rvlc_scale_factor_data(ics, ld);
    }
#endif
#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->scalefac_cycles += count;
#endif
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.27 */
void tns_data(ic_stream* ics, tns_info* tns, bitfile* ld) {
    uint8_t w, filt, i, start_coef_bits = 0, coef_bits;
    uint8_t n_filt_bits = 2;
    uint8_t length_bits = 6;
    uint8_t order_bits = 5;
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        n_filt_bits = 1;
        length_bits = 4;
        order_bits = 3;
    }
    for (w = 0; w < ics->num_windows; w++) {
        tns->n_filt[w] = (uint8_t)faad_getbits(ld, n_filt_bits);
#if 0
        printf("%d\n", tns->n_filt[w]);
#endif
        if (tns->n_filt[w]) {
            if ((tns->coef_res[w] = faad_get1bit(ld)) & 1) {
                start_coef_bits = 4;
            } else {
                start_coef_bits = 3;
            }
#if 0
            printf("%d\n", tns->coef_res[w]);
#endif
        }
        for (filt = 0; filt < tns->n_filt[w]; filt++) {
            tns->length[w][filt] = (uint8_t)faad_getbits(ld, length_bits);
#if 0
            printf("%d\n", tns->length[w][filt]);
#endif
            tns->order[w][filt] = (uint8_t)faad_getbits(ld, order_bits);
#if 0
            printf("%d\n", tns->order[w][filt]);
#endif
            if (tns->order[w][filt]) {
                tns->direction[w][filt] = faad_get1bit(ld);
#if 0
                printf("%d\n", tns->direction[w][filt]);
#endif
                tns->coef_compress[w][filt] = faad_get1bit(ld);
#if 0
                printf("%d\n", tns->coef_compress[w][filt]);
#endif
                coef_bits = start_coef_bits - tns->coef_compress[w][filt];
                for (i = 0; i < tns->order[w][filt]; i++) {
                    tns->coef[w][filt][i] = (uint8_t)faad_getbits(ld, coef_bits);
#if 0
                    printf("%d\n", tns->coef[w][filt][i]);
#endif
                }
            }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef LTP_DEC
/* Table 4.4.28 */
uint8_t ltp_data(NeAACDecStruct* hDecoder, ic_stream* ics, ltp_info* ltp, bitfile* ld) {
    uint8_t sfb, w;
    ltp->lag = 0;
    #ifdef LD_DEC
    if (hDecoder->object_type == LD) {
        ltp->lag_update = (uint8_t)faad_getbits(ld, 1);
        if (ltp->lag_update) { ltp->lag = (uint16_t)faad_getbits(ld, 10); }
    } else {
    #endif
        ltp->lag = (uint16_t)faad_getbits(ld, 11);
    #ifdef LD_DEC
    }
    #endif
    /* Check length of lag */
    if (ltp->lag > (hDecoder->frameLength << 1)) return 18;
    ltp->coef = (uint8_t)faad_getbits(ld, 3);
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        for (w = 0; w < ics->num_windows; w++) {
            if ((ltp->short_used[w] = faad_get1bit(ld)) & 1) {
                ltp->short_lag_present[w] = faad_get1bit(ld);
                if (ltp->short_lag_present[w]) { ltp->short_lag[w] = (uint8_t)faad_getbits(ld, 4); }
            }
        }
    } else {
        ltp->last_band = (ics->max_sfb < MAX_LTP_SFB ? ics->max_sfb : MAX_LTP_SFB);
        for (sfb = 0; sfb < ltp->last_band; sfb++) { ltp->long_used[sfb] = faad_get1bit(ld); }
    }
    return 0;
}
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.29 */
uint8_t spectral_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, int16_t* spectral_data) {
    int8_t   i;
    uint8_t  g;
    uint16_t inc, k, p = 0;
    uint8_t  groups = 0;
    uint8_t  sect_cb;
    uint8_t  result;
    uint16_t nshort = hDecoder->frameLength / 8;
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
    for (g = 0; g < ics->num_window_groups; g++) {
        p = groups * nshort;
        for (i = 0; i < ics->num_sec[g]; i++) {
            sect_cb = ics->sect_cb[g][i];
            inc = (sect_cb >= FIRST_PAIR_HCB) ? 2 : 4;
            switch (sect_cb) {
                case ZERO_HCB:
                case NOISE_HCB:
                case INTENSITY_HCB:
                case INTENSITY_HCB2:
// #define SD_PRINT
#ifdef SD_PRINT
                {
                    int j;
                    for (j = ics->sect_sfb_offset[g][ics->sect_start[g][i]]; j < ics->sect_sfb_offset[g][ics->sect_end[g][i]]; j++) { printf("%d\n", 0); }
                }
#endif
// #define SFBO_PRINT
#ifdef SFBO_PRINT
                    printf("%d\n", ics->sect_sfb_offset[g][ics->sect_start[g][i]]);
#endif
                    p += (ics->sect_sfb_offset[g][ics->sect_end[g][i]] - ics->sect_sfb_offset[g][ics->sect_start[g][i]]);
                    break;
                default:
#ifdef SFBO_PRINT
                    printf("%d\n", ics->sect_sfb_offset[g][ics->sect_start[g][i]]);
#endif
                    for (k = ics->sect_sfb_offset[g][ics->sect_start[g][i]]; k < ics->sect_sfb_offset[g][ics->sect_end[g][i]]; k += inc) {
                        if ((result = huffman_spectral_data(sect_cb, ld, &spectral_data[p])) > 0) return result;
#ifdef SD_PRINT
                        {
                            int j;
                            for (j = p; j < p + inc; j++) { printf("%d\n", spectral_data[j]); }
                        }
#endif
                        p += inc;
                    }
                    break;
            }
        }
        groups += ics->window_group_length[g];
    }
#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->spectral_cycles += count;
#endif
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.30 */
uint16_t extension_payload(bitfile* ld, drc_info* drc, uint16_t count) {
    uint16_t i, n, dataElementLength;
    uint8_t  dataElementLengthPart;
    uint8_t  align = 4, data_element_version, loopCounter;
    uint8_t  extension_type = (uint8_t)faad_getbits(ld, 4);
    switch (extension_type) {
        case EXT_DYNAMIC_RANGE:
            drc->present = 1;
            n = dynamic_range_info(ld, drc);
            return n;
        case EXT_FILL_DATA:
            /* fill_nibble = */ faad_getbits(ld, 4); /* must be '0000' */
            for (i = 0; i < count - 1; i++) {        /* fill_byte[i] = */
                faad_getbits(ld, 8);                 /* must be '10100101' */
            }
            return count;
            break;
        case EXT_DATA_ELEMENT:
            data_element_version = (uint8_t)faad_getbits(ld, 4);
            switch (data_element_version) {
                case ANC_DATA:
                    loopCounter = 0;
                    dataElementLength = 0;
                    do {
                        dataElementLengthPart = (uint8_t)faad_getbits(ld, 8);
                        dataElementLength += dataElementLengthPart;
                        loopCounter++;
                    } while (dataElementLengthPart == 255);
                    for (i = 0; i < dataElementLength; i++) {
                        /* data_element_byte[i] = */ faad_getbits(ld, 8);
                        return (dataElementLength + loopCounter + 1);
                    }
                    /* fallthrough */
                default: align = 0; break;
            }
            /* fallthrough */
        case EXT_FIL:
            /* fallthrough */
        default:
            faad_getbits(ld, align);
            for (i = 0; i < count - 1; i++) { /* other_bits[i] = */
                faad_getbits(ld, 8);
            }
            return count;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.31 */
uint8_t dynamic_range_info(bitfile* ld, drc_info* drc) {
    uint8_t i, n = 1;
    uint8_t band_incr;
    drc->num_bands = 1;
    if (faad_get1bit(ld) & 1) {
        drc->pce_instance_tag = (uint8_t)faad_getbits(ld, 4);
        /* drc->drc_tag_reserved_bits = */ faad_getbits(ld, 4);
        n++;
    }
    drc->excluded_chns_present = faad_get1bit(ld);
    if (drc->excluded_chns_present == 1) { n += excluded_channels(ld, drc); }
    if (faad_get1bit(ld)) {
        band_incr = (uint8_t)faad_getbits(ld, 4);
        /* drc->drc_bands_reserved_bits = */ faad_getbits(ld, 4);
        n++;
        drc->num_bands += band_incr;
        for (i = 0; i < drc->num_bands; i++) {
            drc->band_top[i] = (uint8_t)faad_getbits(ld, 8);
            n++;
        }
    }
    if (faad_get1bit(ld) & 1) {
        drc->prog_ref_level = (uint8_t)faad_getbits(ld, 7);
        /* drc->prog_ref_level_reserved_bits = */ faad_get1bit(ld);
        n++;
    }
    for (i = 0; i < drc->num_bands; i++) {
        drc->dyn_rng_sgn[i] = faad_get1bit(ld);
        drc->dyn_rng_ctl[i] = (uint8_t)faad_getbits(ld, 7);
        n++;
    }
    return n;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 4.4.32 */
uint8_t excluded_channels(bitfile* ld, drc_info* drc) {
    uint8_t i, n = 0;
    uint8_t num_excl_chan = 7;
    for (i = 0; i < 7; i++) { drc->exclude_mask[i] = faad_get1bit(ld); }
    n++;
    while ((drc->additional_excluded_chns[n - 1] = faad_get1bit(ld)) == 1) {
        if (i >= MAX_CHANNELS - num_excl_chan - 7) return n;
        for (i = num_excl_chan; i < num_excl_chan + 7; i++) { drc->exclude_mask[i] = faad_get1bit(ld); }
        n++;
        num_excl_chan += 7;
    }
    return n;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Annex A: Audio Interchange Formats */
/* Table 1.A.2 */
void get_adif_header(adif_header* adif, bitfile* ld) {
    uint8_t i;
    /* adif_id[0] = */ faad_getbits(ld, 8);
    /* adif_id[1] = */ faad_getbits(ld, 8);
    /* adif_id[2] = */ faad_getbits(ld, 8);
    /* adif_id[3] = */ faad_getbits(ld, 8);
    adif->copyright_id_present = faad_get1bit(ld);
    if (adif->copyright_id_present) {
        for (i = 0; i < 72 / 8; i++) { adif->copyright_id[i] = (int8_t)faad_getbits(ld, 8); }
        adif->copyright_id[i] = 0;
    }
    adif->original_copy = faad_get1bit(ld);
    adif->home = faad_get1bit(ld);
    adif->bitstream_type = faad_get1bit(ld);
    adif->bitrate = faad_getbits(ld, 23);
    adif->num_program_config_elements = (uint8_t)faad_getbits(ld, 4);
    for (i = 0; i < adif->num_program_config_elements + 1; i++) {
        if (adif->bitstream_type == 0) {
            adif->adif_buffer_fullness = faad_getbits(ld, 20);
        } else {
            adif->adif_buffer_fullness = 0;
        }
        program_config_element(&adif->pce[i], ld);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 1.A.5 */
uint8_t adts_frame(adts_header* adts, bitfile* ld) {
    /* faad_byte_align(ld); */
    if (adts_fixed_header(adts, ld)) return 5;
    adts_variable_header(adts, ld);
    adts_error_check(adts, ld);
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 1.A.6 */
uint8_t adts_fixed_header(adts_header* adts, bitfile* ld) {
    uint16_t i;
    uint8_t  sync_err = 1;
    /* try to recover from sync errors */
    for (i = 0; i < 768; i++) {
        adts->syncword = (uint16_t)faad_showbits(ld, 12);
        if (adts->syncword != 0xFFF) {
            faad_getbits(ld, 8);
        } else {
            sync_err = 0;
            faad_getbits(ld, 12);
            break;
        }
    }
    if (sync_err) return 5;
    adts->id = faad_get1bit(ld);
    adts->layer = (uint8_t)faad_getbits(ld, 2);
    adts->protection_absent = faad_get1bit(ld);
    adts->profile = (uint8_t)faad_getbits(ld, 2);
    adts->sf_index = (uint8_t)faad_getbits(ld, 4);
    adts->private_bit = faad_get1bit(ld);
    adts->channel_configuration = (uint8_t)faad_getbits(ld, 3);
    adts->original = faad_get1bit(ld);
    adts->home = faad_get1bit(ld);
    if (adts->old_format == 1) {
        /* Removed in corrigendum 14496-3:2002 */
        if (adts->id == 0) { adts->emphasis = (uint8_t)faad_getbits(ld, 2); }
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 1.A.7 */
void adts_variable_header(adts_header* adts, bitfile* ld) {
    adts->copyright_identification_bit = faad_get1bit(ld);
    adts->copyright_identification_start = faad_get1bit(ld);
    adts->aac_frame_length = (uint16_t)faad_getbits(ld, 13);
    adts->adts_buffer_fullness = (uint16_t)faad_getbits(ld, 11);
    adts->no_raw_data_blocks_in_frame = (uint8_t)faad_getbits(ld, 2);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Table 1.A.8 */
void adts_error_check(adts_header* adts, bitfile* ld) {
    if (adts->protection_absent == 0) { adts->crc_check = (uint16_t)faad_getbits(ld, 16); }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* LATM parsing functions */
uint32_t latm_get_value(bitfile* ld) {
    uint32_t l, value;
    uint8_t  bytesForValue;
    bytesForValue = (uint8_t)faad_getbits(ld, 2);
    value = 0;
    for (l = 0; l < bytesForValue; l++) value = (value << 8) | (uint8_t)faad_getbits(ld, 8);
    return value;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t latmParsePayload(latm_header* latm, bitfile* ld) {
    // assuming there's only one program with a single layer and 1 subFrame,
    // allStreamsSametimeframing is set,
    uint32_t framelen;
    uint8_t  tmp;
    // this should be the payload length field for the current configuration
    framelen = 0;
    if (latm->framelen_type == 0) {
        do {
            tmp = (uint8_t)faad_getbits(ld, 8);
            framelen += tmp;
        } while (tmp == 0xff);
    } else if (latm->framelen_type == 1)
        framelen = latm->frameLength;
    return framelen;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t latmAudioMuxElement(latm_header* latm, bitfile* ld) {
    uint32_t               ascLen, asc_bits = 0;
    uint32_t               x1, y1, m, n, i;
    program_config         pce;
    mp4AudioSpecificConfig mp4ASC;
    latm->useSameStreamMux = (uint8_t)faad_getbits(ld, 1);
    if (!latm->useSameStreamMux) {
        // parseSameStreamMuxConfig
        latm->version = (uint8_t)faad_getbits(ld, 1);
        if (latm->version) latm->versionA = (uint8_t)faad_getbits(ld, 1);
        if (latm->versionA) {
            // dunno the payload format for versionA
            fprintf(stderr, "versionA not supported\n");
            return 0;
        }
        if (latm->version) // read taraBufferFullness
            latm_get_value(ld);
        latm->allStreamsSameTimeFraming = (uint8_t)faad_getbits(ld, 1);
        latm->numSubFrames = (uint8_t)faad_getbits(ld, 6) + 1;
        latm->numPrograms = (uint8_t)faad_getbits(ld, 4) + 1;
        latm->numLayers = faad_getbits(ld, 3) + 1;
        if (latm->numPrograms > 1 || !latm->allStreamsSameTimeFraming || latm->numSubFrames > 1 || latm->numLayers > 1) {
            fprintf(stderr, "\r\nUnsupported LATM configuration: %d programs/ %d subframes, %d layers, allstreams: %d\n", latm->numPrograms, latm->numSubFrames, latm->numLayers,
                    latm->allStreamsSameTimeFraming);
            return 0;
        }
        ascLen = 0;
        if (latm->version) ascLen = latm_get_value(ld);
        x1 = faad_get_processed_bits(ld);
        if (AudioSpecificConfigFromBitfile(ld, &mp4ASC, &pce, 0, 1) < 0) return 0;
        // horrid hack to unread the ASC bits and store them in latm->ASC
        // the correct code would rely on an ideal faad_ungetbits()
        y1 = faad_get_processed_bits(ld);
        if ((y1 - x1) <= MAX_ASC_BYTES * 8) {
            faad_rewindbits(ld);
            m = x1;
            while (m > 0) {
                n = min(m, 32);
                faad_getbits(ld, n);
                m -= n;
            }
            i = 0;
            m = latm->ASCbits = y1 - x1;
            while (m > 0) {
                n = min(m, 8);
                latm->ASC[i++] = (uint8_t)faad_getbits(ld, n);
                m -= n;
            }
        }
        asc_bits = y1 - x1;
        if (ascLen > asc_bits) faad_getbits(ld, ascLen - asc_bits);
        latm->framelen_type = (uint8_t)faad_getbits(ld, 3);
        if (latm->framelen_type == 0) {
            latm->frameLength = 0;
            faad_getbits(ld, 8); // buffer fullness for frame_len_type==0, useless
        } else if (latm->framelen_type == 1) {
            latm->frameLength = faad_getbits(ld, 9);
            if (latm->frameLength == 0) {
                fprintf(stderr, "Invalid frameLength: 0\r\n");
                return 0;
            }
            latm->frameLength = (latm->frameLength + 20) * 8;
        } else { // hellish CELP or HCVX stuff, discard
            fprintf(stderr, "Unsupported CELP/HCVX framelentype: %d\n", latm->framelen_type);
            return 0;
        }
        latm->otherDataLenBits = 0;
        if (faad_getbits(ld, 1)) { // other data present
            int esc, tmp;
            if (latm->version)
                latm->otherDataLenBits = latm_get_value(ld);
            else
                do {
                    esc = faad_getbits(ld, 1);
                    tmp = faad_getbits(ld, 8);
                    latm->otherDataLenBits = (latm->otherDataLenBits << 8) + tmp;
                } while (esc);
        }
        if (faad_getbits(ld, 1)) // crc
            faad_getbits(ld, 8);
        latm->inited = 1;
    }
    // read payload
    if (latm->inited)
        return latmParsePayload(latm, ld);
    else
        return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t faad_latm_frame(latm_header* latm, bitfile* ld) {
    uint16_t len;
    uint32_t initpos, endpos, firstpos, ret;
    (void)firstpos;
    firstpos = faad_get_processed_bits(ld);
    while (ld->bytes_left) {
        faad_byte_align(ld);
        if (faad_showbits(ld, 11) != 0x2B7) {
            faad_getbits(ld, 8);
            continue;
        }
        faad_getbits(ld, 11);
        len = faad_getbits(ld, 13);
        if (!len) continue;
        initpos = faad_get_processed_bits(ld);
        ret = latmAudioMuxElement(latm, ld);
        endpos = faad_get_processed_bits(ld);
        if (ret > 0) return (len * 8) - (endpos - initpos);
        // faad_getbits(ld, initpos-endpos); //go back to initpos, but is valid a getbits(-N) ?
    }
    return 0xFFFFFFFF;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
uint8_t rvlc_scale_factor_data(ic_stream* ics, bitfile* ld) {
    uint8_t bits = 9;
    ics->sf_concealment = faad_get1bit(ld);
    ics->rev_global_gain = (uint8_t)faad_getbits(ld, 8);
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) bits = 11;
    /* the number of bits used for the huffman codewords */
    ics->length_of_rvlc_sf = (uint16_t)faad_getbits(ld, bits);
    if (ics->noise_used) {
        ics->dpcm_noise_nrg = (uint16_t)faad_getbits(ld, 9);
        ics->length_of_rvlc_sf -= 9;
    }
    ics->sf_escapes_present = faad_get1bit(ld);
    if (ics->sf_escapes_present) { ics->length_of_rvlc_escapes = (uint8_t)faad_getbits(ld, 8); }
    if (ics->noise_used) { ics->dpcm_noise_last_position = (uint16_t)faad_getbits(ld, 9); }
    return 0;
}
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
uint8_t rvlc_decode_scale_factors(ic_stream* ics, bitfile* ld) {
    uint8_t  result;
    uint8_t  intensity_used = 0;
    uint8_t* rvlc_sf_buffer = NULL;
    uint8_t* rvlc_esc_buffer = NULL;
    bitfile  ld_rvlc_sf, ld_rvlc_esc;
    //    bitfile ld_rvlc_sf_rev, ld_rvlc_esc_rev;
    if (ics->length_of_rvlc_sf > 0) {
        /* We read length_of_rvlc_sf bits here to put it in a seperate bitfile. */
        rvlc_sf_buffer = faad_getbitbuffer(ld, ics->length_of_rvlc_sf);
        faad_initbits(&ld_rvlc_sf, (void*)rvlc_sf_buffer, bit2byte(ics->length_of_rvlc_sf));
        //        faad_initbits_rev(&ld_rvlc_sf_rev, (void*)rvlc_sf_buffer,
        //            ics->length_of_rvlc_sf);
    }
    if (ics->sf_escapes_present) {
        /* We read length_of_rvlc_escapes bits here to put it in a seperate bitfile. */
        rvlc_esc_buffer = faad_getbitbuffer(ld, ics->length_of_rvlc_escapes);
        faad_initbits(&ld_rvlc_esc, (void*)rvlc_esc_buffer, bit2byte(ics->length_of_rvlc_escapes));
        //        faad_initbits_rev(&ld_rvlc_esc_rev, (void*)rvlc_esc_buffer,
        //            ics->length_of_rvlc_escapes);
    }
    /* decode the rvlc scale factors and escapes */
    result = rvlc_decode_sf_forward(ics, &ld_rvlc_sf, &ld_rvlc_esc, &intensity_used);
    //    result = rvlc_decode_sf_reverse(ics, &ld_rvlc_sf_rev,
    //        &ld_rvlc_esc_rev, intensity_used);
    if (rvlc_esc_buffer) faad_free(rvlc_esc_buffer);
    if (rvlc_sf_buffer) faad_free(rvlc_sf_buffer);
    if (ics->length_of_rvlc_sf > 0) faad_endbits(&ld_rvlc_sf);
    if (ics->sf_escapes_present) faad_endbits(&ld_rvlc_esc);
    return result;
}
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
uint8_t rvlc_decode_sf_forward(ic_stream* ics, bitfile* ld_sf, bitfile* ld_esc, uint8_t* intensity_used) {
    int8_t g, sfb;
    int8_t t = 0;
    int8_t error = 0;
    int8_t noise_pcm_flag = 1;
    int16_t scale_factor = ics->global_gain;
    int16_t is_position = 0;
    int16_t noise_energy = ics->global_gain - 90 - 256;
    #ifdef PRINT_RVLC
    printf("\nglobal_gain: %d\n", ics->global_gain);
    #endif
    for (g = 0; g < ics->num_window_groups; g++) {
        for (sfb = 0; sfb < ics->max_sfb; sfb++) {
            if (error) {
                ics->scale_factors[g][sfb] = 0;
            } else {
                switch (ics->sfb_cb[g][sfb]) {
                    case ZERO_HCB: /* zero book */ ics->scale_factors[g][sfb] = 0; break;
                    case INTENSITY_HCB: /* intensity books */
                    case INTENSITY_HCB2:
                        *intensity_used = 1;
                        /* decode intensity position */
                        t = rvlc_huffman_sf(ld_sf, ld_esc, +1);
                        is_position += t;
                        ics->scale_factors[g][sfb] = is_position;
                        break;
                    case NOISE_HCB: /* noise books */
                        /* decode noise energy */
                        if (noise_pcm_flag) {
                            int16_t n = ics->dpcm_noise_nrg;
                            noise_pcm_flag = 0;
                            noise_energy += n;
                        } else {
                            t = rvlc_huffman_sf(ld_sf, ld_esc, +1);
                            noise_energy += t;
                        }
                        ics->scale_factors[g][sfb] = noise_energy;
                        break;
                    default: /* spectral books */
                        /* decode scale factor */
                        t = rvlc_huffman_sf(ld_sf, ld_esc, +1);
                        scale_factor += t;
                        if (scale_factor < 0) return 4;
                        ics->scale_factors[g][sfb] = scale_factor;
                        break;
                }
    #ifdef PRINT_RVLC
                printf("%3d:%4d%4d\n", sfb, ics->sfb_cb[g][sfb], ics->scale_factors[g][sfb]);
    #endif
                if (t == 99) { error = 1; }
            }
        }
    }
    #ifdef PRINT_RVLC
    printf("\n\n");
    #endif
    return 0;
}
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
    #if 0 // not used right now, doesn't work correctly yet
static uint8_t rvlc_decode_sf_reverse(ic_stream *ics, bitfile *ld_sf, bitfile *ld_esc,
                                      uint8_t intensity_used)
{
    int8_t g, sfb;
    int8_t t = 0;
    int8_t error = 0;
    int8_t noise_pcm_flag = 1, is_pcm_flag = 1, sf_pcm_flag = 1;
    int16_t scale_factor = ics->rev_global_gain;
    int16_t is_position = 0;
    int16_t noise_energy = ics->rev_global_gain;
        #ifdef PRINT_RVLC
    printf("\nrev_global_gain: %d\n", ics->rev_global_gain);
        #endif
    if (intensity_used)
    {
        is_position = rvlc_huffman_sf(ld_sf, ld_esc, -1);
        #ifdef PRINT_RVLC
        printf("is_position: %d\n", is_position);
        #endif
    }
    for (g = ics->num_window_groups-1; g >= 0; g--)
    {
        for (sfb = ics->max_sfb-1; sfb >= 0; sfb--)
        {
            if (error)
            {
                ics->scale_factors[g][sfb] = 0;
            } else {
                switch (ics->sfb_cb[g][sfb])
                {
                case ZERO_HCB: /* zero book */
                    ics->scale_factors[g][sfb] = 0;
                    break;
                case INTENSITY_HCB: /* intensity books */
                case INTENSITY_HCB2:
                    if (is_pcm_flag)
                    {
                        is_pcm_flag = 0;
                        ics->scale_factors[g][sfb] = is_position;
                    } else {
                        t = rvlc_huffman_sf(ld_sf, ld_esc, -1);
                        is_position -= t;
                        ics->scale_factors[g][sfb] = (uint8_t)is_position;
                    }
                    break;
                case NOISE_HCB: /* noise books */
                    /* decode noise energy */
                    if (noise_pcm_flag)
                    {
                        noise_pcm_flag = 0;
                        noise_energy = ics->dpcm_noise_last_position;
                    } else {
                        t = rvlc_huffman_sf(ld_sf, ld_esc, -1);
                        noise_energy -= t;
                    }
                    ics->scale_factors[g][sfb] = (uint8_t)noise_energy;
                    break;
                default: /* spectral books */
                    if (sf_pcm_flag || (sfb == 0))
                    {
                        sf_pcm_flag = 0;
                        if (sfb == 0)
                            scale_factor = ics->global_gain;
                    } else {
                        /* decode scale factor */
                        t = rvlc_huffman_sf(ld_sf, ld_esc, -1);
                        scale_factor -= t;
                    }
                    if (scale_factor < 0)
                        return 4;
                    ics->scale_factors[g][sfb] = (uint8_t)scale_factor;
                    break;
                }
        #ifdef PRINT_RVLC
                printf("%3d:%4d%4d\n", sfb, ics->sfb_cb[g][sfb],
                    ics->scale_factors[g][sfb]);
        #endif
                if (t == 99)
                {
                    error = 1;
                }
            }
        }
    }
        #ifdef PRINT_RVLC
    printf("\n\n");
        #endif
    return 0;
}
    #endif // 0
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
/* index == 99 means not allowed codeword */
static rvlc_huff_table book_rvlc[] = {
    /*index  length  codeword */
    {0, 1, 0},    /*         0 */
    {-1, 3, 5},   /*       101 */
    {1, 3, 7},    /*       111 */
    {-2, 4, 9},   /*      1001 */
    {-3, 5, 17},  /*     10001 */
    {2, 5, 27},   /*     11011 */
    {-4, 6, 33},  /*    100001 */
    {99, 6, 50},  /*    110010 */
    {3, 6, 51},   /*    110011 */
    {99, 6, 52},  /*    110100 */
    {-7, 7, 65},  /*   1000001 */
    {99, 7, 96},  /*   1100000 */
    {99, 7, 98},  /*   1100010 */
    {7, 7, 99},   /*   1100011 */
    {4, 7, 107},  /*   1101011 */
    {-5, 8, 129}, /*  10000001 */
    {99, 8, 194}, /*  11000010 */
    {5, 8, 195},  /*  11000011 */
    {99, 8, 212}, /*  11010100 */
    {99, 9, 256}, /* 100000000 */
    {-6, 9, 257}, /* 100000001 */
    {99, 9, 426}, /* 110101010 */
    {6, 9, 427},  /* 110101011 */
    {99, 10, 0}   /* Shouldn't come this far */
};
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
static rvlc_huff_table book_escape[] = {
    /*index  length  codeword */
    {1, 2, 0},        {0, 2, 2},        {3, 3, 2},        {2, 3, 6},        {4, 4, 14},       {7, 5, 13},       {6, 5, 15},       {5, 5, 31},       {11, 6, 24},      {10, 6, 25},
    {9, 6, 29},       {8, 6, 61},       {13, 7, 56},      {12, 7, 120},     {15, 8, 114},     {14, 8, 242},     {17, 9, 230},     {16, 9, 486},     {19, 10, 463},    {18, 10, 974},
    {22, 11, 925},    {20, 11, 1950},   {21, 11, 1951},   {23, 12, 1848},   {25, 13, 3698},   {24, 14, 7399},   {26, 15, 14797},  {49, 19, 236736}, {50, 19, 236737}, {51, 19, 236738},
    {52, 19, 236739}, {53, 19, 236740}, {27, 20, 473482}, {28, 20, 473483}, {29, 20, 473484}, {30, 20, 473485}, {31, 20, 473486}, {32, 20, 473487}, {33, 20, 473488}, {34, 20, 473489},
    {35, 20, 473490}, {36, 20, 473491}, {37, 20, 473492}, {38, 20, 473493}, {39, 20, 473494}, {40, 20, 473495}, {41, 20, 473496}, {42, 20, 473497}, {43, 20, 473498}, {44, 20, 473499},
    {45, 20, 473500}, {46, 20, 473501}, {47, 20, 473502}, {48, 20, 473503}, {99, 21, 0} /* Shouldn't come this far */
};
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
int8_t rvlc_huffman_sf(bitfile* ld_sf, bitfile* ld_esc, int8_t direction) {
    uint8_t          i, j;
    int8_t           index;
    uint32_t         cw;
    rvlc_huff_table* h = book_rvlc;
    i = h->len;
    if (direction > 0)
        cw = faad_getbits(ld_sf, i);
    else
        cw = faad_getbits_rev(ld_sf, i);
    while ((cw != h->cw) && (i < 10)) {
        h++;
        j = h->len - i;
        i += j;
        cw <<= j;
        if (direction > 0)
            cw |= faad_getbits(ld_sf, j);
        else
            cw |= faad_getbits_rev(ld_sf, j);
    }
    index = h->index;
    if (index == +ESC_VAL) {
        int8_t esc = rvlc_huffman_esc(ld_esc, direction);
        if (esc == 99) return 99;
        index += esc;
    #ifdef PRINT_RVLC
        printf("esc: %d - ", esc);
    #endif
    }
    if (index == -ESC_VAL) {
        int8_t esc = rvlc_huffman_esc(ld_esc, direction);
        if (esc == 99) return 99;
        index -= esc;
    #ifdef PRINT_RVLC
        printf("esc: %d - ", esc);
    #endif
    }
    return index;
}
#endif // ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
int8_t rvlc_huffman_esc(bitfile* ld, int8_t direction) {
    uint8_t          i, j;
    uint32_t         cw;
    rvlc_huff_table* h = book_escape;
    i = h->len;
    if (direction > 0)
        cw = faad_getbits(ld, i);
    else
        cw = faad_getbits_rev(ld, i);
    while ((cw != h->cw) && (i < 21)) {
        h++;
        j = h->len - i;
        i += j;
        cw <<= j;
        if (direction > 0)
            cw |= faad_getbits(ld, j);
        else
            cw |= faad_getbits_rev(ld, j);
    }
    return h->index;
}
#endif //ERROR_RESILIENCE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

