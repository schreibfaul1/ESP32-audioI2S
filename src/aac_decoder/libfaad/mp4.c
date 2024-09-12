#include "common.h"
#include "structs.h"
#include <stdlib.h>
#include "mp4.h"
#include "syntax.h"
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* defines if an object type can be decoded by this library or not */
static uint8_t ObjectTypesTable[32] = {
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
/* Table 1.6.1 */
char NeAACDecAudioSpecificConfig(unsigned char* pBuffer, unsigned long buffer_size, mp4AudioSpecificConfig* mp4ASC) { return AudioSpecificConfig2(pBuffer, buffer_size, mp4ASC, NULL, 0); }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t AudioSpecificConfigFromBitfile(bitfile* ld, mp4AudioSpecificConfig* mp4ASC, program_config* pce, uint32_t buffer_size, uint8_t short_form) {
    int8_t   result = 0;
    uint32_t startpos = faad_get_processed_bits(ld);
#ifdef SBR_DEC
    int8_t bits_to_decode = 0;
#endif
    if(mp4ASC == NULL) return -8;
    memset(mp4ASC, 0, sizeof(mp4AudioSpecificConfig));
    mp4ASC->objectTypeIndex = (uint8_t)faad_getbits(ld, 5);
    mp4ASC->samplingFrequencyIndex = (uint8_t)faad_getbits(ld, 4);
    if(mp4ASC->samplingFrequencyIndex == 0x0f) faad_getbits(ld, 24);
    mp4ASC->channelsConfiguration = (uint8_t)faad_getbits(ld, 4);
    mp4ASC->samplingFrequency = get_sample_rate(mp4ASC->samplingFrequencyIndex);
    if(ObjectTypesTable[mp4ASC->objectTypeIndex] != 1) { return -1; }
    if(mp4ASC->samplingFrequency == 0) { return -2; }
    if(mp4ASC->channelsConfiguration > 7) { return -3; }
#if(defined(PS_DEC) || defined(DRM_PS))
    /* check if we have a mono file */
    if(mp4ASC->channelsConfiguration == 1) {
        /* upMatrix to 2 channels for implicit signalling of PS */
        mp4ASC->channelsConfiguration = 2;
    }
#endif
#ifdef SBR_DEC
    mp4ASC->sbr_present_flag = -1;
    if(mp4ASC->objectTypeIndex == 5 || mp4ASC->objectTypeIndex == 29) {
        uint8_t tmp;
        mp4ASC->sbr_present_flag = 1;
        tmp = (uint8_t)faad_getbits(ld, 4);
        /* check for downsampled SBR */
        if(tmp == mp4ASC->samplingFrequencyIndex) mp4ASC->downSampledSBR = 1;
        mp4ASC->samplingFrequencyIndex = tmp;
        if(mp4ASC->samplingFrequencyIndex == 15) { mp4ASC->samplingFrequency = (uint32_t)faad_getbits(ld, 24); }
        else { mp4ASC->samplingFrequency = get_sample_rate(mp4ASC->samplingFrequencyIndex); }
        mp4ASC->objectTypeIndex = (uint8_t)faad_getbits(ld, 5);
    }
#endif
    /* get GASpecificConfig */
    if(mp4ASC->objectTypeIndex == 1 || mp4ASC->objectTypeIndex == 2 || mp4ASC->objectTypeIndex == 3 || mp4ASC->objectTypeIndex == 4 || mp4ASC->objectTypeIndex == 6 || mp4ASC->objectTypeIndex == 7) {
        result = GASpecificConfig(ld, mp4ASC, pce);
#ifdef ERROR_RESILIENCE
    }
    else if(mp4ASC->objectTypeIndex >= ER_OBJECT_START) { /* ER */ result = GASpecificConfig(ld, mp4ASC, pce);
        mp4ASC->epConfig = (uint8_t)faad_getbits(ld, 2);
        if(mp4ASC->epConfig != 0) result = -5;
#endif
    }
    else { result = -4; }
#ifdef SSR_DEC
    /* shorter frames not allowed for SSR */
    if((mp4ASC->objectTypeIndex == 4) && mp4ASC->frameLengthFlag) return -6;
#endif
#ifdef SBR_DEC
    if(short_form) bits_to_decode = 0;
    else bits_to_decode = (int8_t)(buffer_size * 8 - (startpos - faad_get_processed_bits(ld)));
    if((mp4ASC->objectTypeIndex != 5 && mp4ASC->objectTypeIndex != 29) && (bits_to_decode >= 16)) {
        int16_t syncExtensionType = (int16_t)faad_getbits(ld, 11);
        if(syncExtensionType == 0x2b7) {
            uint8_t tmp_OTi = (uint8_t)faad_getbits(ld, 5);
            if(tmp_OTi == 5) {
                mp4ASC->sbr_present_flag = (uint8_t)faad_get1bit(ld);
                if(mp4ASC->sbr_present_flag) {
                    uint8_t tmp;
                    /* Don't set OT to SBR until checked that it is actually there */
                    mp4ASC->objectTypeIndex = tmp_OTi;
                    tmp = (uint8_t)faad_getbits(ld, 4);
                    /* check for downsampled SBR */
                    if(tmp == mp4ASC->samplingFrequencyIndex) mp4ASC->downSampledSBR = 1;
                    mp4ASC->samplingFrequencyIndex = tmp;
                    if(mp4ASC->samplingFrequencyIndex == 15) {
                        mp4ASC->samplingFrequency = (uint32_t)faad_getbits(ld, 24);
                    }
                    else { mp4ASC->samplingFrequency = get_sample_rate(mp4ASC->samplingFrequencyIndex); }
                }
            }
        }
    }
    /* no SBR signalled, this could mean either implicit signalling or no SBR in this file */
    /* MPEG specification states: assume SBR on files with samplerate <= 24000 Hz */
    if(mp4ASC->sbr_present_flag == (char)-1) /* cannot be -1 on systems with unsigned char */
    {
        if(mp4ASC->samplingFrequency <= 24000) {
            mp4ASC->samplingFrequency *= 2;
            mp4ASC->forceUpSampling = 1;
        }
        else /* > 24000*/ { mp4ASC->downSampledSBR = 1; }
    }
#endif
    faad_endbits(ld);
    return result;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t AudioSpecificConfig2(uint8_t* pBuffer, uint32_t buffer_size, mp4AudioSpecificConfig* mp4ASC, program_config* pce, uint8_t short_form) {
    uint8_t ret = 0;
    bitfile ld;
    faad_initbits(&ld, pBuffer, buffer_size);
    faad_byte_align(&ld);
    ret = AudioSpecificConfigFromBitfile(&ld, mp4ASC, pce, buffer_size, short_form);
    faad_endbits(&ld);
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————