/*
 * mp3_decoder.cpp
 * libhelix_HMP3DECODER
 *
 *  Created on: 26.10.2018
 *  Updated on: 14.02.2026
 */
#include "mp3_decoder.h"

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool MP3Decoder::init() {
    m_MP3DecInfo.alloc("m_MP3DecInfo");
    m_FrameHeader.alloc("m_FrameHeader");
    m_SideInfo.alloc("m_SideInfo");
    m_ScaleFactorJS.alloc("m_ScaleFactorJS");
    m_HuffmanInfo.alloc("m_HuffmanInfo");
    m_DequantInfo.alloc("m_DequantInfo");
    m_IMDCTInfo.alloc("m_IMDCTInfo");
    m_SubbandInfo.alloc("m_SubbandInfo");
    m_MP3FrameInfo.alloc("m_MP3FrameInfo");
    m_out16.alloc_array(4608 * 2, "m_out16");

    if (!m_MP3DecInfo.valid() || !m_FrameHeader.valid() || !m_SideInfo.valid() || !m_ScaleFactorJS.valid() || !m_HuffmanInfo.valid() || !m_DequantInfo.valid() || !m_IMDCTInfo.valid() ||
        !m_SubbandInfo.valid() || !m_MP3FrameInfo.valid() || !m_out16.valid()) {
        reset();
        MP3_LOG_ERROR("not enough memory to allocate mp3decoder buffers");
        return false;
    }
    clear();
    return true;
}

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void MP3Decoder::reset() {
    m_MP3DecInfo.reset();
    m_FrameHeader.reset();
    m_SideInfo.reset();
    m_ScaleFactorJS.reset();
    m_HuffmanInfo.reset();
    m_DequantInfo.reset();
    m_IMDCTInfo.reset();
    m_SubbandInfo.reset();
    m_MP3FrameInfo.reset();
    m_mpeg_version_str.reset();
    m_out16.reset();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void MP3Decoder::clear() {
    /* important to do this - DSP primitives assume a bunch of state variables are 0 on first use */
    m_MP3DecInfo.clear();
    m_FrameHeader.clear();
    m_SideInfo.clear();
    m_ScaleFactorJS.clear();
    m_HuffmanInfo.clear();
    m_DequantInfo.clear();
    m_IMDCTInfo.clear();
    m_SubbandInfo.clear();
    m_MP3FrameInfo.clear();
    m_mpeg_version_str.clear();
    m_out16.clear();
    memset(&m_SFBandTable, 0, sizeof(SFBandTable_t));                                         // Clear SFBandTable
    memset(&m_ScaleFactorInfoSub, 0, sizeof(ScaleFactorInfoSub_t) * (MAX_NGRAN * MAX_NCHAN)); // Clear ScaleFactorInfo
    memset(&m_CriticalBandInfo, 0, sizeof(CriticalBandInfo_t) * MAX_NCHAN);                   // Clear CriticalBandInfo
    memset(&m_SideInfoSub, 0, sizeof(SideInfoSub_t) * (MAX_NGRAN * MAX_NCHAN));               // Clear SideInfoSub
    return;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool MP3Decoder::isValid() {
    if (!m_MP3DecInfo.valid() || !m_FrameHeader.valid() || !m_SideInfo.valid() || !m_ScaleFactorJS.valid() || !m_HuffmanInfo.valid() || !m_DequantInfo.valid() || !m_IMDCTInfo.valid() ||
        !m_SubbandInfo.valid() || !m_MP3FrameInfo.valid() || !m_out16.valid()) {
        return false;
    }
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* B I T S T R E A M */
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void MP3Decoder::SetBitstreamPointer(BitStreamInfo_t* bsi, int32_t nBytes, uint8_t* buf) {
    /* init bitstream */
    bsi->bytePtr = buf;
    bsi->iCache = 0;     /* 4-byte uint32_t */
    bsi->cachedBits = 0; /* i.e. zero bits in cache */
    bsi->nBytes = nBytes;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void MP3Decoder::RefillBitstreamCache(BitStreamInfo_t* bsi) {
    int32_t nBytes = bsi->nBytes;
    /* optimize for common case, independent of machine endian-ness */
    if (nBytes >= 4) {
        bsi->iCache = (*bsi->bytePtr++) << 24;
        bsi->iCache |= (*bsi->bytePtr++) << 16;
        bsi->iCache |= (*bsi->bytePtr++) << 8;
        bsi->iCache |= (*bsi->bytePtr++);
        bsi->cachedBits = 32;
        bsi->nBytes -= 4;
    } else {
        bsi->iCache = 0;
        while (nBytes--) {
            bsi->iCache |= (*bsi->bytePtr++);
            bsi->iCache <<= 8;
        }
        bsi->iCache <<= ((3 - bsi->nBytes) * 8);
        bsi->cachedBits = 8 * bsi->nBytes;
        bsi->nBytes = 0;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t MP3Decoder::GetBits(BitStreamInfo_t* bsi, int32_t nBits) {
    uint32_t data, lowBits;

    nBits &= 0x1f;                      /* nBits mod 32 to avoid MP3Decoder::  unpredictable results like >> by negative amount */
    data = bsi->iCache >> (31 - nBits); /* unsigned >> so zero-extend */
    data >>= 1;                         /* do as >> 31, >> 1 so that nBits = 0 works okay (returns 0) */
    bsi->iCache <<= nBits;              /* left-justify cache */
    bsi->cachedBits -= nBits;           /* how many bits have we drawn from the cache so far */
    if (bsi->cachedBits < 0) {          /* if we cross anint32_t boundary, refill the cache */
        lowBits = -bsi->cachedBits;
        RefillBitstreamCache(bsi);
        data |= bsi->iCache >> (32 - lowBits); /* get the low-order bits */
        bsi->cachedBits -= lowBits;            /* how many bits have we drawn from the cache so far */
        bsi->iCache <<= lowBits;               /* left-justify cache */
    }
    return data;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::CalcBitsUsed(BitStreamInfo_t* bsi, uint8_t* startBuf, int32_t startOffset) {
    int32_t bitsUsed;
    bitsUsed = (bsi->bytePtr - startBuf) * 8;
    bitsUsed -= bsi->cachedBits;
    bitsUsed -= startOffset;
    return bitsUsed;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::CheckPadBit() {
    return (m_FrameHeader->paddingBit ? 1 : 0);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::UnpackFrameHeader(uint8_t* buf) {
    int32_t verIdx;
    /* validate pointers and sync word */
    if ((buf[0] & SYNCWORDH) != SYNCWORDH || (buf[1] & SYNCWORDL) != SYNCWORDL) { return -1; }
    /* read header fields - use bitmasks instead of GetBits() for speed, since format never varies */
    verIdx = (buf[1] >> 3) & 0x03;
    m_MPEGVersion = (MPEGVersion_t)(verIdx == 0 ? MPEG25 : ((verIdx & 0x01) ? MPEG1 : MPEG2));
    m_FrameHeader->layer = 4 - ((buf[1] >> 1) & 0x03); /* easy mapping of index to layer number, 4 = error */
    m_FrameHeader->crc = 1 - ((buf[1] >> 0) & 0x01);
    m_FrameHeader->brIdx = (buf[2] >> 4) & 0x0f;
    m_FrameHeader->srIdx = (buf[2] >> 2) & 0x03;
    m_FrameHeader->paddingBit = (buf[2] >> 1) & 0x01;
    m_FrameHeader->privateBit = (buf[2] >> 0) & 0x01;
    m_sMode = (StereoMode_t)((buf[3] >> 6) & 0x03); /* maps to correct enum (see definition) */
    m_FrameHeader->modeExt = (buf[3] >> 4) & 0x03;
    m_FrameHeader->copyFlag = (buf[3] >> 3) & 0x01;
    m_FrameHeader->origFlag = (buf[3] >> 2) & 0x01;
    m_FrameHeader->emphasis = (buf[3] >> 0) & 0x03;
    /* check parameters to avoid MP3Decoder::  indexing tables with bad values */
    if (m_FrameHeader->srIdx == 3 || m_FrameHeader->layer == 4 || m_FrameHeader->brIdx == 15) { return -1; }
    /* for readability (we reference sfBandTable many times in decoder) */
    m_SFBandTable = sfBandTable[m_MPEGVersion][m_FrameHeader->srIdx];
    if (m_sMode != Joint) /* just to be safe (dequant, stproc check fh->modeExt) */
        m_FrameHeader->modeExt = 0;
    /* init user-accessible data */
    m_MP3DecInfo->nChans = (m_sMode == Mono ? 1 : 2);
    m_MP3DecInfo->samprate = samplerateTab[m_MPEGVersion][m_FrameHeader->srIdx];
    m_MP3DecInfo->nGrans = (m_MPEGVersion == MPEG1 ? NGRANS_MPEG1 : NGRANS_MPEG2);
    m_MP3DecInfo->nGranSamps = ((int32_t)samplesPerFrameTab[m_MPEGVersion][m_FrameHeader->layer - 1]) / m_MP3DecInfo->nGrans;
    m_MP3DecInfo->layer = m_FrameHeader->layer;

    /* get bitrate and nSlots from table, unless brIdx == 0 (free mode) in which case caller must figure it out himself
     * question - do we want to overwrite mp3DecInfo->bitrate with 0 each time if it's free mode, and
     *  copy the pre-calculated actual free bitrate into it in mp3dec.c (according to the spec,
     *  this shouldn't be necessary, since it should be either all frames free or none free)
     */
    if (m_FrameHeader->brIdx) {
        m_MP3DecInfo->bitrate = ((int32_t)bitrateTab[m_MPEGVersion][m_FrameHeader->layer - 1][m_FrameHeader->brIdx]) * 1000;
        /* nSlots = total frame bytes (from table) - sideInfo bytes - header - CRC (if present) + pad (if present) */
        m_MP3DecInfo->nSlots = (int32_t)slotTab[m_MPEGVersion][m_FrameHeader->srIdx][m_FrameHeader->brIdx] - (int32_t)sideBytesTab[m_MPEGVersion][(m_sMode == Mono ? 0 : 1)] - 4 -
                               (m_FrameHeader->crc ? 2 : 0) + (m_FrameHeader->paddingBit ? 1 : 0);
    }
    /* load crc word, if enabled, and return length of frame header (in bytes) */
    if (m_FrameHeader->crc) {
        m_FrameHeader->CRCWord = ((int32_t)buf[4] << 8 | (int32_t)buf[5] << 0);
        return 6;
    } else {
        m_FrameHeader->CRCWord = 0;
        return 4;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::UnpackSideInfo(uint8_t* buf) {
    int32_t         gr, ch, bd, nBytes;
    BitStreamInfo_t bitStreamInfo, *bsi;

    SideInfoSub_t* sis;
    /* validate pointers and sync word */
    bsi = &bitStreamInfo;
    if (m_MPEGVersion == MPEG1) {
        /* MPEG 1 */
        nBytes = (m_sMode == Mono ? SIBYTES_MPEG1_MONO : SIBYTES_MPEG1_STEREO);
        SetBitstreamPointer(bsi, nBytes, buf);
        m_SideInfo->mainDataBegin = GetBits(bsi, 9);
        m_SideInfo->privateBits = GetBits(bsi, (m_sMode == Mono ? 5 : 3));
        for (ch = 0; ch < m_MP3DecInfo->nChans; ch++)
            for (bd = 0; bd < MAX_SCFBD; bd++) m_SideInfo->scfsi[ch][bd] = GetBits(bsi, 1);
    } else {
        /* MPEG 2, MPEG 2.5 */
        nBytes = (m_sMode == Mono ? SIBYTES_MPEG2_MONO : SIBYTES_MPEG2_STEREO);
        SetBitstreamPointer(bsi, nBytes, buf);
        m_SideInfo->mainDataBegin = GetBits(bsi, 8);
        m_SideInfo->privateBits = GetBits(bsi, (m_sMode == Mono ? 1 : 2));
    }
    for (gr = 0; gr < m_MP3DecInfo->nGrans; gr++) {
        for (ch = 0; ch < m_MP3DecInfo->nChans; ch++) {
            sis = &m_SideInfoSub[gr][ch]; /* side info subblock for this granule, channel */
            sis->part23Length = GetBits(bsi, 12);
            sis->nBigvals = GetBits(bsi, 9);
            sis->globalGain = GetBits(bsi, 8);
            sis->sfCompress = GetBits(bsi, (m_MPEGVersion == MPEG1 ? 4 : 9));
            sis->winSwitchFlag = GetBits(bsi, 1);
            if (sis->winSwitchFlag) {
                /* this is a start, stop, short, or mixed block */
                sis->blockType = GetBits(bsi, 2);  /* 0 = normal, 1 = start, 2 = short, 3 = stop */
                sis->mixedBlock = GetBits(bsi, 1); /* 0 = not mixed, 1 = mixed */
                sis->tableSelect[0] = GetBits(bsi, 5);
                sis->tableSelect[1] = GetBits(bsi, 5);
                sis->tableSelect[2] = 0; /* unused */
                sis->subBlockGain[0] = GetBits(bsi, 3);
                sis->subBlockGain[1] = GetBits(bsi, 3);
                sis->subBlockGain[2] = GetBits(bsi, 3);
                if (sis->blockType == 0) {
                    /* this should not be allowed, according to spec */
                    sis->nBigvals = 0;
                    sis->part23Length = 0;
                    sis->sfCompress = 0;
                } else if (sis->blockType == 2 && sis->mixedBlock == 0) {
                    /* short block, not mixed */
                    sis->region0Count = 8;
                } else {
                    /* start, stop, or short-mixed */
                    sis->region0Count = 7;
                }
                sis->region1Count = 20 - sis->region0Count;
            } else {
                /* this is a normal block */
                sis->blockType = 0;
                sis->mixedBlock = 0;
                sis->tableSelect[0] = GetBits(bsi, 5);
                sis->tableSelect[1] = GetBits(bsi, 5);
                sis->tableSelect[2] = GetBits(bsi, 5);
                sis->region0Count = GetBits(bsi, 4);
                sis->region1Count = GetBits(bsi, 3);
            }
            sis->preFlag = (m_MPEGVersion == MPEG1 ? GetBits(bsi, 1) : 0);
            sis->sfactScale = GetBits(bsi, 1);
            sis->count1TableSelect = GetBits(bsi, 1);
        }
    }
    m_MP3DecInfo->mainDataBegin = m_SideInfo->mainDataBegin; /* needed by main decode loop */
    assert(nBytes == CalcBitsUsed(bsi, buf, 0) >> 3);
    return nBytes;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    UnpackSFMPEG1
 *
 * Description: unpack MPEG 1 scalefactors from bitstream
 *
 * Inputs:      BitStreamInfo, SideInfoSub, ScaleFactorInfoSub structs for this
 *                granule/channel
 *              vector of scfsi flags from side info, length = 4 (MAX_SCFBD)
 *              index of current granule
 *              ScaleFactorInfoSub from granule 0 (for granule 1, if scfsi[i] is set,
 *                then we just replicate the scale factors from granule 0 in the
 *                i'th set of scalefactor bands)
 *
 * Outputs:     updated BitStreamInfo struct
 *              scalefactors in sfis (short and/or long arrays, as appropriate)
 *
 * Return:      none
 *
 * Notes:       set order of short blocks to s[band][window] instead of s[window][band]
 *                so that we index through consectutive memory locations when unpacking
 *                (make sure dequantizer follows same convention)
 *              Illegal Intensity Position = 7 (always) for MPEG1 scale factors
 */
void MP3Decoder::UnpackSFMPEG1(BitStreamInfo_t* bsi, SideInfoSub_t* sis, ScaleFactorInfoSub_t* sfis, int32_t* scfsi, int32_t gr, ScaleFactorInfoSub_t* sfisGr0) {
    int32_t sfb;
    int32_t slen0, slen1;
    /* these can be 0, so make sure GetBits(bsi, 0) returns 0 (no >> 32 or anything) */
    slen0 = (int32_t)m_SFLenTab[sis->sfCompress][0];
    slen1 = (int32_t)m_SFLenTab[sis->sfCompress][1];
    if (sis->blockType == 2) {
        /* short block, type 2 (implies winSwitchFlag == 1) */
        if (sis->mixedBlock) {
            /* do long block portion */
            for (sfb = 0; sfb < 8; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen0);
            sfb = 3;
        } else {
            /* all short blocks */
            sfb = 0;
        }
        for (; sfb < 6; sfb++) {
            sfis->s[sfb][0] = (char)GetBits(bsi, slen0);
            sfis->s[sfb][1] = (char)GetBits(bsi, slen0);
            sfis->s[sfb][2] = (char)GetBits(bsi, slen0);
        }
        for (; sfb < 12; sfb++) {
            sfis->s[sfb][0] = (char)GetBits(bsi, slen1);
            sfis->s[sfb][1] = (char)GetBits(bsi, slen1);
            sfis->s[sfb][2] = (char)GetBits(bsi, slen1);
        }
        /* last sf band not transmitted */
        sfis->s[12][0] = sfis->s[12][1] = sfis->s[12][2] = 0;
    } else {
        /* long blocks, type 0, 1, or 3 */
        if (gr == 0) {
            /* first granule */
            for (sfb = 0; sfb < 11; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen0);
            for (sfb = 11; sfb < 21; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen1);
            return;
        } else {
            /* second granule
             * scfsi: 0 = different scalefactors for each granule,
             *        1 = copy sf's from granule 0 into granule 1
             * for block type == 2, scfsi is always 0
             */
            sfb = 0;
            if (scfsi[0])
                for (; sfb < 6; sfb++) sfis->l[sfb] = sfisGr0->l[sfb];
            else
                for (; sfb < 6; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen0);
            if (scfsi[1])
                for (; sfb < 11; sfb++) sfis->l[sfb] = sfisGr0->l[sfb];
            else
                for (; sfb < 11; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen0);
            if (scfsi[2])
                for (; sfb < 16; sfb++) sfis->l[sfb] = sfisGr0->l[sfb];
            else
                for (; sfb < 16; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen1);
            if (scfsi[3])
                for (; sfb < 21; sfb++) sfis->l[sfb] = sfisGr0->l[sfb];
            else
                for (; sfb < 21; sfb++) sfis->l[sfb] = (char)GetBits(bsi, slen1);
        }
        /* last sf band not transmitted */
        sfis->l[21] = 0;
        sfis->l[22] = 0;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    UnpackSFMPEG2
 *
 * Description: unpack MPEG 2 scalefactors from bitstream
 *
 * Inputs:      BitStreamInfo, SideInfoSub, ScaleFactorInfoSub structs for this
 *                granule/channel
 *              index of current granule and channel
 *              ScaleFactorInfoSub from this granule
 *              modeExt field from frame header, to tell whether intensity stereo is on
 *              ScaleFactorJS struct for storing IIP info used in Dequant()
 *
 * Outputs:     updated BitStreamInfo struct
 *              scalefactors in sfis (short and/or long arrays, as appropriate)
 *              updated intensityScale and preFlag flags
 *
 * Return:      none
 *
 * Notes:       Illegal Intensity Position = (2^slen) - 1 for MPEG2 scale factors
 */
void MP3Decoder::UnpackSFMPEG2(BitStreamInfo_t* bsi, SideInfoSub_t* sis, ScaleFactorInfoSub_t* sfis, int32_t gr, int32_t ch, int32_t modeExt, ScaleFactorJS_t* sfjs) {

    int32_t i, sfb, sfcIdx, btIdx, nrIdx; // iipTest;
    int32_t slen[4], nr[4];
    int32_t sfCompress, preFlag, intensityScale;
    (void)gr;
    sfCompress = sis->sfCompress;
    preFlag = 0;
    intensityScale = 0;

    /* stereo mode bits (1 = on): bit 1 = mid-side on/off, bit 0 = intensity on/off */
    if (!((modeExt & 0x01) && (ch == 1))) {
        /* in other words: if ((modeExt & 0x01) == 0 || ch == 0) */
        if (sfCompress < 400) {
            /* max slen = floor[(399/16) / 5] = 4 */
            slen[0] = (sfCompress >> 4) / 5;
            slen[1] = (sfCompress >> 4) % 5;
            slen[2] = (sfCompress & 0x0f) >> 2;
            slen[3] = (sfCompress & 0x03);
            sfcIdx = 0;
        } else if (sfCompress < 500) {
            /* max slen = floor[(99/4) / 5] = 4 */
            sfCompress -= 400;
            slen[0] = (sfCompress >> 2) / 5;
            slen[1] = (sfCompress >> 2) % 5;
            slen[2] = (sfCompress & 0x03);
            slen[3] = 0;
            sfcIdx = 1;
        } else {
            /* max slen = floor[11/3] = 3 (sfCompress = 9 bits in MPEG2) */
            sfCompress -= 500;
            slen[0] = sfCompress / 3;
            slen[1] = sfCompress % 3;
            slen[2] = slen[3] = 0;
            if (sis->mixedBlock) {
                /* adjust for long/short mix logic (see comment above in NRTab[] definition) */
                slen[2] = slen[1];
                slen[1] = slen[0];
            }
            preFlag = 1;
            sfcIdx = 2;
        }
    } else {
        /* intensity stereo ch = 1 (right) */
        intensityScale = sfCompress & 0x01;
        sfCompress >>= 1;
        if (sfCompress < 180) {
            /* max slen = floor[35/6] = 5 (from mod 36) */
            slen[0] = (sfCompress / 36);
            slen[1] = (sfCompress % 36) / 6;
            slen[2] = (sfCompress % 36) % 6;
            slen[3] = 0;
            sfcIdx = 3;
        } else if (sfCompress < 244) {
            /* max slen = floor[63/16] = 3 */
            sfCompress -= 180;
            slen[0] = (sfCompress & 0x3f) >> 4;
            slen[1] = (sfCompress & 0x0f) >> 2;
            slen[2] = (sfCompress & 0x03);
            slen[3] = 0;
            sfcIdx = 4;
        } else {
            /* max slen = floor[11/3] = 3 (max sfCompress >> 1 = 511/2 = 255) */
            sfCompress -= 244;
            slen[0] = (sfCompress / 3);
            slen[1] = (sfCompress % 3);
            slen[2] = slen[3] = 0;
            sfcIdx = 5;
        }
    }
    /* set index based on block type: (0,1,3) --> 0, (2 non-mixed) --> 1, (2 mixed) ---> 2 */
    btIdx = 0;
    if (sis->blockType == 2) btIdx = (sis->mixedBlock ? 2 : 1);
    for (i = 0; i < 4; i++) nr[i] = (int32_t)NRTab[sfcIdx][btIdx][i];

    /* save intensity stereo scale factor info */
    if ((modeExt & 0x01) && (ch == 1)) {
        for (i = 0; i < 4; i++) {
            sfjs->slen[i] = slen[i];
            sfjs->nr[i] = nr[i];
        }
        sfjs->intensityScale = intensityScale;
    }
    sis->preFlag = preFlag;

    /* short blocks */
    if (sis->blockType == 2) {
        if (sis->mixedBlock) {
            /* do long block portion */
            // iipTest = (1 << slen[0]) - 1;
            for (sfb = 0; sfb < 6; sfb++) { sfis->l[sfb] = (char)GetBits(bsi, slen[0]); }
            sfb = 3; /* start sfb for short */
            nrIdx = 1;
        } else {
            /* all short blocks, so start nr, sfb at 0 */
            sfb = 0;
            nrIdx = 0;
        }

        /* remaining short blocks, sfb just keeps incrementing */
        for (; nrIdx <= 3; nrIdx++) {
            // iipTest = (1 << slen[nrIdx]) - 1;
            for (i = 0; i < nr[nrIdx]; i++, sfb++) {
                sfis->s[sfb][0] = (char)GetBits(bsi, slen[nrIdx]);
                sfis->s[sfb][1] = (char)GetBits(bsi, slen[nrIdx]);
                sfis->s[sfb][2] = (char)GetBits(bsi, slen[nrIdx]);
            }
        }
        /* last sf band not transmitted */
        sfis->s[12][0] = sfis->s[12][1] = sfis->s[12][2] = 0;
    } else {
        /* long blocks */
        sfb = 0;
        for (nrIdx = 0; nrIdx <= 3; nrIdx++) {
            // iipTest = (1 << slen[nrIdx]) - 1;
            for (i = 0; i < nr[nrIdx]; i++, sfb++) { sfis->l[sfb] = (char)GetBits(bsi, slen[nrIdx]); }
        }
        /* last sf band not transmitted */
        sfis->l[21] = sfis->l[22] = 0;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    UnpackScaleFactors
 *
 * Description: parse the fields of the MP3 scale factor data section
 *
 * Inputs:      MP3DecInfo structure filled by UnpackFrameHeader() and UnpackSideInfo()
 *              buffer pointing to the MP3 scale factor data
 *              pointer to bit offset (0-7) indicating starting bit in buf[0]
 *              number of bits available in data buffer
 *              index of current granule and channel
 *
 * Outputs:     updated platform-specific ScaleFactorInfo struct
 *              updated bitOffset
 *
 * Return:      length (in bytes) of scale factor data, -1 if null input pointers
 */
int32_t MP3Decoder::UnpackScaleFactors(uint8_t* buf, int32_t* bitOffset, int32_t bitsAvail, int32_t gr, int32_t ch) {
    int32_t         bitsUsed;
    uint8_t*        startBuf;
    BitStreamInfo_t bitStreamInfo, *bsi;

    /* init GetBits reader */
    startBuf = buf;
    bsi = &bitStreamInfo;
    SetBitstreamPointer(bsi, (bitsAvail + *bitOffset + 7) / 8, buf);
    if (*bitOffset) GetBits(bsi, *bitOffset);

    if (m_MPEGVersion == MPEG1)
        UnpackSFMPEG1(bsi, &m_SideInfoSub[gr][ch], &m_ScaleFactorInfoSub[gr][ch], m_SideInfo->scfsi[ch], gr, &m_ScaleFactorInfoSub[0][ch]);
    else
        UnpackSFMPEG2(bsi, &m_SideInfoSub[gr][ch], &m_ScaleFactorInfoSub[gr][ch], gr, ch, m_FrameHeader->modeExt, m_ScaleFactorJS.get());

    m_MP3DecInfo->part23Length[gr][ch] = m_SideInfoSub[gr][ch].part23Length;

    bitsUsed = CalcBitsUsed(bsi, buf, *bitOffset);
    buf += (bitsUsed + *bitOffset) >> 3;
    *bitOffset = (bitsUsed + *bitOffset) & 0x07;

    return (buf - startBuf);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* M P 3 D E C */
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    MP3FindSyncWord
 *
 * Description: locate the next byte-alinged sync word in the raw mp3 stream
 *
 * Inputs:      buffer to search for sync word
 *              max number of bytes to search in buffer
 *
 * Outputs:     none
 *
 * Return:      offset to first sync word (bytes from start of buf)
 *              -1 if sync not found after searching nBytes
 */
int32_t MP3Decoder::findSyncWord(uint8_t* buf, int32_t nBytes) {

    // Auxiliary function for extracting bits, byte 'value', 'start_bit' is that bit from the left (0-7), 'num_bits' is the number of bits
    // auto extract_bits = [&](uint8_t byte, uint8_t start_bit, uint8_t num_bits) {
    //   return (byte >> (8 - start_bit - num_bits)) & ((1 << num_bits) - 1);
    // };

    typedef struct {
        uint8_t  mpeg_version = 0; // 0=MPEG2.5, 1=reserved, 2=MPEG2, 3=MPEG1
        uint8_t  layer = 0;        // 0=reserved, 1=Layer III, 2=Layer II, 3=Layer I
        bool     crc_protected = 0;
        uint8_t  bitrate_idx = 0;
        uint8_t  sample_rate_idx = 0;
        bool     padding = 0;
        uint8_t  channel_mode = 0;
        uint32_t frame_length = 0;   // cytes
        uint16_t sample_rate_hz = 0; // the actual sampling rate in Hz
        uint16_t bitrate_kbps = 0;   // the actual bit rate in Kbps
        uint16_t samples_per_frame = 0;
    } Mp3FrameHeader_sync_t;

    // SamplingFrequenz-Lookup tables(Beispiel für MPEG1, MPEG2, MPEG2.5)
    const uint16_t sampling_rates[3][4] = {
        {44100, 48000, 32000, 0}, // MPEG1
        {22050, 24000, 16000, 0}, // MPEG2
        {11025, 12000, 8000, 0}   // MPEG2.5
    };

    typedef enum { /* map to 0,1,2 to make table indexing easier */
                   MPEG1 = 0,
                   MPEG2 = 1,
                   MPEG25 = 2
    } MPEGVersion_t;

    const uint16_t mpeg1_layer1_bitrates[16] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0};

    const uint16_t mpeg1_layer3_bitrates[16] = {
        // Bitraten-Lookup tables (example for MPEG1 Layer III)
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 // Attention: These tables must be complete and correct!
    };
    // Define bitrate tables for MPEG1 Layer II and MPEG2/2.5 Layer II
    // These tables are examples and need to be complete based on the MPEG standard
    const uint16_t mpeg1_layer2_bitrates[] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0};
    const uint16_t mpeg2_layer2_bitrates[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    const uint16_t mpeg2_layer3_bitrates[] = {
        0, // "Free format" oder ungültig
        8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,
        0 // Ungültig
    };

    // Funktion zum Parsen des Headers und Überprüfen der Gültigkeit
    auto parseMp3Header = [&](const uint8_t* header_data, Mp3FrameHeader_sync_t* header_info) {
        // Byte 0: Syncword H (bereits geprüft)
        // Byte 1: Syncword L, MPEG Version, Layer
        // Byte 2: Bitrate, Sampling Frequency, Padding, Private
        // Byte 3: Channel Mode, Mode Extension, Copyright, Original, Emphasis

        // Syncword has already been checked, here we start with the other bits
        header_info->mpeg_version = (header_data[1] >> 3) & 0b11;    // Bits 12, 13 (A, B)
        header_info->layer = (header_data[1] >> 1) & 0b11;           // Bits 14, 15 (C, D)
        header_info->crc_protected = !((header_data[1] >> 0) & 0b1); // Bit 16 (Schutzbit)

        header_info->bitrate_idx = (header_data[2] >> 4) & 0b1111;   // Bits 17-20
        header_info->sample_rate_idx = (header_data[2] >> 2) & 0b11; // Bits 21-22
        header_info->padding = (header_data[2] >> 1) & 0b1;          // Bit 23

        header_info->channel_mode = (header_data[3] >> 6) & 0b11; // Bits 24-25

        // Gültigkeitsprüfungen
        if (header_info->mpeg_version == 1) { // Reserved
            MP3_LOG_DEBUG("Reserved MPEG version\n");
            return false;
        }
        if (header_info->layer == 0) { // Reserved
            MP3_LOG_DEBUG("Reserved Layer\n");
            return false;
        }
        // Modified part: Support for Layer II and Layer III
        if (header_info->layer != 1 && header_info->layer != 2 && header_info->layer != 3) { // Allow Layer I (3) Layer II (2) and Layer III (1)
            printf("\n");
            for (int i = 0; i < 10; i++) printf("0x%02x ", header_data[i]);
            printf("\n"); // Use header_data instead of buf
            MP3_LOG_DEBUG("Not Layer I or II or III\n");
            return false;
        }

        if (header_info->bitrate_idx == 0 || header_info->bitrate_idx == 15) { // Invalid bit rate
            MP3_LOG_DEBUG("Invalid bitrate index\n");
            return false;
        }
        if (header_info->sample_rate_idx == 3) { // Invalid sampling frequency
            MP3_LOG_DEBUG("Invalid sampling rate index\n");
            return false;
        }

        // Determine the actual bit rate and sampling frequency
        uint16_t bitrate_kbps = 0;
        uint16_t sample_rate_hz = 0;

        // Mapping from MPEG version to sampling rate table
        uint8_t sr_table_idx = 0;
        if (header_info->mpeg_version == 3)
            sr_table_idx = 0; // MPEG 1 (0b11)
        else if (header_info->mpeg_version == 2)
            sr_table_idx = 1; // MPEG 2 (0b10)
        else
            sr_table_idx = 2; // MPEG 2.5 (da mpeg_version == 0) - Although Google TTS is likely MPEG 2.0

        sample_rate_hz = sampling_rates[sr_table_idx][header_info->sample_rate_idx];

        // Bitraten-Mapping für verschiedene MPEG-Versionen und Layer
        if (header_info->mpeg_version == 3) { // MPEG 1
            if (header_info->layer == 1) {    // Layer III
                bitrate_kbps = mpeg1_layer3_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 2) { // Layer II
                bitrate_kbps = mpeg1_layer2_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 3) { // Layer I
                bitrate_kbps = mpeg1_layer1_bitrates[header_info->bitrate_idx];
            }
        } else if (header_info->mpeg_version == 2 || header_info->mpeg_version == 0) { // MPEG 2 or MPEG 2.5
            if (header_info->layer == 1) {                                             // Layer III
                bitrate_kbps = mpeg2_layer3_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 2) { // Layer II
                bitrate_kbps = mpeg2_layer2_bitrates[header_info->bitrate_idx];
            }
            // If you also want to support MPEG 2/2.5 Layer I, you'd add another else if here
            // e.g., else if (header_info->layer == 3) { bitrate_kbps = mpeg2_layer1_bitrates[header_info->bitrate_idx]; }
        }

        if (bitrate_kbps == 0 || sample_rate_hz == 0) {
            MP3_LOG_DEBUG("Could not determine valid bitrate or sample rate\n");
            return false;
        }

        // Calculate frame length based on layer
        // FrameSize = (1152 * BitRate / SampleRate) + Padding (for Layer III)
        // FrameSize = (144 * BitRate / SampleRate) + Padding (for Layer I)
        // FrameSize = (576 * BitRate / SampleRate) + Padding (for Layer II, MPEG 2.0/2.5)
        // Note: For MPEG 1 Layer II, it's (1152 * BitRate / SampleRate) + Padding
        // Need to be careful with the constant depending on MPEG version and layer
        if (header_info->layer == 1) {                                                   // Layer III
            header_info->frame_length = (144 * bitrate_kbps * 1000) / sample_rate_hz;    // Assuming MPEG1 Layer III
            if (header_info->mpeg_version == 2 || header_info->mpeg_version == 0) {      // MPEG2/2.5 Layer III
                header_info->frame_length = (72 * bitrate_kbps * 1000) / sample_rate_hz; // Correct constant for MPEG2/2.5 Layer III
            }
        } else if (header_info->layer == 2) {     // Layer II
            if (header_info->mpeg_version == 3) { // MPEG 1 Layer II
                header_info->frame_length = (144 * bitrate_kbps * 1000) / sample_rate_hz;
            } else if (header_info->mpeg_version == 2 || header_info->mpeg_version == 0) { // MPEG 2/2.5 Layer II
                header_info->frame_length = (144 * bitrate_kbps * 1000) / sample_rate_hz;
            }
        } else if (header_info->layer == 3) { // Layer I
            // For Layer I, the formula is (Bitrate * 12 / SampleRate) + Padding (in Bytes)
            // Note: Bitrate is in kbps, so multiply by 1000 to get bps
            if (header_info->mpeg_version == 3) {                                            // MPEG 1 Layer I
                header_info->frame_length = (bitrate_kbps * 1000 / 8 * 12) / sample_rate_hz; // Correct
            } else if (header_info->mpeg_version == 2 || header_info->mpeg_version == 0) {   // MPEG 2/2.5 Layer I (if supported)
                // You'd add the specific calculation for MPEG2/2.5 Layer I here
                // For MPEG 2/2.5 Layer I, samples per frame is 576, so the constant is 6
                header_info->frame_length = (bitrate_kbps * 1000 / 8 * 6) / sample_rate_hz; // Hypothetical, verify constant
            }
        }

        if (header_info->padding) {
            header_info->frame_length += 1; // Füge 1 Byte für Padding hinzu
        }

        if (header_info->frame_length == 0) {
            MP3_LOG_DEBUG("Calculated frame length is zero\n");
            return false;
        }
        header_info->sample_rate_hz = sample_rate_hz;
        header_info->bitrate_kbps = bitrate_kbps;

        // Determine samples_per_frame based on the version and layer
        if (header_info->mpeg_version == 3) {                         // MPEG-1
            if (header_info->layer == 1 || header_info->layer == 2) { // Layer III oder Layer II
                header_info->samples_per_frame = 1152;
            } else if (header_info->layer == 3) { // Layer I
                header_info->samples_per_frame = 384;
            } else {
                header_info->samples_per_frame = 0; // Should be caught by previous checks
                return false;
            }
        } else if (header_info->mpeg_version == 2 || header_info->mpeg_version == 0) { // MPEG-2 oder MPEG-2.5
            if (header_info->layer == 1) {                                             // Layer III
                header_info->samples_per_frame = 576;
            } else if (header_info->layer == 2) { // Layer II
                header_info->samples_per_frame = 1152;
            } else if (header_info->layer == 3) {     // Layer I
                header_info->samples_per_frame = 576; // Correct for MPEG-2/2.5 Layer I
            } else {
                header_info->samples_per_frame = 0; // Should be caught by previous checks
                return false;
            }
        } else { // header_info->mpeg_version == 1 (Reserved)
            header_info->samples_per_frame = 0;
            return false;
        }
        return true; // Header ist gültig
    };

    const uint8_t mp3FHsize = 4; // frame header size

    // Lambda for the fast syncword search
    auto findSync = [&](uint8_t* search_buf, uint16_t offset, uint16_t len) {
        for (int32_t i = 0; i < len - 1; i++) {
            // Prüfe auf die 11 oder 12 Sync-Bits
            if ((search_buf[i + offset] == SYNCWORDH) && ((search_buf[i + offset + 1] & SYNCWORDL) == SYNCWORDL)) { return i; }
        }
        return (int32_t)-1;
    };

    int32_t current_pos = 0;

    while (nBytes >= mp3FHsize) { // Make sure that there are enough bytes for a header
        int32_t sync_offset = findSync(buf, current_pos, nBytes);

        if (sync_offset == -1) {
            MP3_LOG_DEBUG("No syncword found in remaining buffer\n");
            return -1; // No more syncword found
        }

        current_pos += sync_offset;
        nBytes -= sync_offset;

        if (nBytes < mp3FHsize) {
            MP3_LOG_DEBUG("Not enough bytes for a full header after syncword\n");
            return -1; // Not enough data for a full header
        }

        Mp3FrameHeader_sync_t header;
        if (parseMp3Header(&buf[current_pos], &header)) {
            // This is where the crucial step comes: Check the next frame
            if (current_pos + header.frame_length + mp3FHsize <= current_pos + nBytes) {
                // Check whether there is a syncword at the expected next frame start and a valid header is (optional but very robust)
                Mp3FrameHeader_sync_t next_header;
                if (((buf[current_pos + header.frame_length] == SYNCWORDH) && ((buf[current_pos + header.frame_length + 1] & SYNCWORDL) == SYNCWORDL)) &&
                    parseMp3Header(&buf[current_pos + header.frame_length], &next_header)) {
                    // MP3_LOG_DEBUG("Found reliable MP3 frame at pos: %d, length: %lu\n", current_pos, header.frame_length);

                    // s_samplerate   = header.sample_rate_hz; // (suppose in the structure available)
                    // s_bitRate      = header.bitrate_kbps;   // (suppose in the structure available)
                    // s_mpeg_version = header.mpeg_version;
                    // s_layer        = header.layer;
                    // s_channel_mode = header.channel_mode;
                    // s_samples_per_frame = header.samples_per_frame;

                    // // s_channels (1 Mono, 2 Stereo)
                    // if (header.channel_mode == 0b11) { // 0b11 ist Mono
                    //     s_channels = 1;
                    // } else { // 2 channels for all other (Stereo, Joint Stereo, Dual Channel)
                    //     s_channels = 2;
                    // }
                    return current_pos;
                } else {
                    MP3_LOG_DEBUG("Header valid, but next frame does not validate. False positive. Moving on.");
                }
            } else {
                MP3_LOG_DEBUG("Header valid, but not enough data for next frame check. Possibly end of stream or false positive.");
                // If not enough data for the next frame, it could still be the right one.
                // This is a compromise.If in doubt, continue to search or return the current one.
                // For robustness: search.
            }
        } else {
            MP3_LOG_DEBUG("Found syncword but header is invalid. Moving to next possible syncword.");
        }

        // If the current header was invalid or the next frame did not validate the current "SyncWord" and continue to search
        current_pos += 1; // go a byte on and look for the Syncword again
        nBytes -= 1;
    }

    return -1; // no valid MP3 frame found
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    MP3FindFreeSync
 *
 * Description: figure out number of bytes between adjacent sync words in "free" mode
 *
 * Inputs:      buffer to search for next sync word
 *              the 4-byte frame header starting at the current sync word
 *              max number of bytes to search in buffer
 *
 * Outputs:     none
 *
 * Return:      offset to next sync word, minus any pad byte (i.e. nSlots)
 *              -1 if sync not found after searching nBytes
 *
 * Notes:       this checks that the first 22 bits of the next frame header are the
 *                same as the current frame header, but it's still not foolproof
 *                (could accidentally find a sequence in the bitstream which
 *                 appears to match but is not actually the next frame header)
 *              this could be made more error-resilient by checking several frames
 *                in a row and verifying that nSlots is the same in each case
 *              since free mode requires CBR (see spec) we generally only call
 *                this function once (first frame) then store the result (nSlots)
 *                and just use it from then on
 */
int32_t MP3Decoder::MP3FindFreeSync(uint8_t* buf, uint8_t firstFH[4], int32_t nBytes) {
    int32_t  offset = 0;
    uint8_t* bufPtr = buf;

    /* loop until we either:
     *  - run out of nBytes (FindMP3SyncWord() returns -1)
     *  - find the next valid frame header (sync word, version, layer, CRC flag, bitrate, and sample rate
     *      in next header must match current header)
     */
    while (1) {
        offset = findSyncWord(bufPtr, nBytes);
        bufPtr += offset;
        if (offset < 0) {
            return -1;
        } else if ((bufPtr[0] == firstFH[0]) && (bufPtr[1] == firstFH[1]) && ((bufPtr[2] & 0xfc) == (firstFH[2] & 0xfc))) {
            /* want to return number of bytes per frame,
             * NOT counting the padding byte, so subtract one if padFlag == 1 */
            if ((firstFH[2] >> 1) & 0x01) bufPtr--;
            return bufPtr - buf;
        }
        bufPtr += 3;
        nBytes -= (offset + 3);
    };

    return -1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    MP3GetLastFrameInfo
 *
 * Description: get info about last MP3 frame decoded (number of sampled decoded,
 *                sample rate, bitrate, etc.)
 *
 * Inputs:
 *
 * Outputs:     filled-in MP3FrameInfo struct
 *
 * Return:      none
 *
 * Notes:       call this right after calling MP3Decode
 */
void MP3Decoder::MP3GetLastFrameInfo() {
    if (m_MP3DecInfo->layer != 3) {
        m_MP3FrameInfo->bitrate = 0;
        m_MP3FrameInfo->nChans = 0;
        m_MP3FrameInfo->samprate = 0;
        m_MP3FrameInfo->bitsPerSample = 0;
        m_MP3FrameInfo->outputSamps = 0;
        m_MP3FrameInfo->layer = 0;
        m_MP3FrameInfo->version = 0;
    } else {
        m_MP3FrameInfo->bitrate = m_MP3DecInfo->bitrate;
        m_MP3FrameInfo->nChans = m_MP3DecInfo->nChans;
        m_MP3FrameInfo->samprate = m_MP3DecInfo->samprate;
        m_MP3FrameInfo->bitsPerSample = 16;
        m_MP3FrameInfo->outputSamps = (int32_t)samplesPerFrameTab[m_MPEGVersion][m_MP3DecInfo->layer - 1];
        m_MP3FrameInfo->layer = m_MP3DecInfo->layer;
        m_MP3FrameInfo->version = m_MPEGVersion;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t MP3Decoder::getSampleRate() {
    return m_MP3FrameInfo->samprate;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t MP3Decoder::getChannels() {
    return m_MP3FrameInfo->nChans;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t MP3Decoder::getBitsPerSample() {
    return m_MP3FrameInfo->bitsPerSample;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t MP3Decoder::getBitRate() {
    return m_MP3FrameInfo->bitrate;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t MP3Decoder::getOutputSamples() {
    return m_MP3FrameInfo->outputSamps;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* MP3Decoder::arg1() {
    m_mpeg_version_str.assign(mpeg_version_table[m_MP3FrameInfo->version]); // 0: MPEG-2.5, 1: Reserviert, 2: MPEG-2 (ISO/IEC 13818-3), 3: MPEG-1 (ISO/IEC 11172-3)
    m_mpeg_version_str.appendf(" %s", layer_table[m_MP3FrameInfo->layer]);
    return m_mpeg_version_str.get();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t MP3Decoder::getAudioDataStart() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t MP3Decoder::getAudioFileDuration() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* MP3Decoder::getStreamTitle() {
    return nullptr; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* MP3Decoder::whoIsIt() {
    return "MP3";
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void MP3Decoder::setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength) {
    return; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
std::vector<uint32_t> MP3Decoder::getMetadataBlockPicture() {
    return {};
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* MP3Decoder::arg2() {
    return ""; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::val1() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::val2() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    MP3GetNextFrameInfo
 *
 * Description: parse MP3 frame header
 *
 * Inputs:        pointer to buffer containing valid MP3 frame header (located using
 *                MP3FindSyncWord(), above)
 *
 * Outputs:     filled-in MP3FrameInfo struct
 *
 * Return:      error code, defined in mp3dec.h (0 means no error, < 0 means error)
 */
int32_t MP3Decoder::MP3GetNextFrameInfo(uint8_t* buf) {

    if (UnpackFrameHeader(buf) == -1 || m_MP3DecInfo->layer != 3) {
        MP3_LOG_ERROR("MP3 invalid frameheader");
        return MP3_ERR;
    }
    MP3GetLastFrameInfo();

    return MP3_NONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    MP3ClearBadFrame
 *
 * Description: zero out pcm buffer if error decoding MP3 frame
 *
 * Inputs:      mp3DecInfo struct with correct frame size parameters filled in
 *              pointer pcm output buffer
 *
 * Outputs:     zeroed out pcm buffer
 *
 * Return:      none
 */
void MP3Decoder::MP3ClearBadFrame(int16_t* outbuf) {
    int32_t i;
    for (i = 0; i < m_MP3DecInfo->nGrans * m_MP3DecInfo->nGranSamps * m_MP3DecInfo->nChans; i++) outbuf[i] = 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    IsLikelyRealFrame
 *
 * Description: Detection of valid MP3 frames
 *
 * Return:      true, if valid
 *              false if ID3 padding fragments, LAME Info, Xing Header, VBRI Header, Repeater-Frames, Encoder Delay Blocks
 * LAME Info
 */
int32_t MP3Decoder::IsLikelyRealFrame(const uint8_t* p, int32_t bytesLeft) {

    auto CalcFrameLength = [](const uint8_t* h) -> int {
        static const int bitrateTable[2][16] = {
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},    // MPEG2/2.5
            {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0} // MPEG1
        };

        static const int samplerateTable[3][3] = {
            {11025, 12000, 8000},  // MPEG 2.5
            {22050, 24000, 16000}, // MPEG 2
            {44100, 48000, 32000}  // MPEG 1
        };

        uint8_t verID = (h[1] >> 3) & 0x03;
        uint8_t layer = (h[1] >> 1) & 0x03;
        uint8_t brIdx = (h[2] >> 4) & 0x0F;
        uint8_t srIdx = (h[2] >> 2) & 0x03;
        uint8_t padding = (h[2] >> 1) & 0x01;

        if (layer != 1) return -1;
        if (srIdx == 3) return -1;
        if (brIdx == 0 || brIdx == 15) return -1;

        int verGroup = (verID == 3) ? 2 : (verID == 2 ? 1 : 0);
        int samplerate = samplerateTable[verGroup][srIdx];
        if (samplerate == 0) return -1;

        int br = bitrateTable[(verID == 3)][brIdx] * 1000;

        int frameLength = (verID == 3) ? (144 * br / samplerate + padding) : (72 * br / samplerate + padding);

        return frameLength;
    };

    // 1) Sync?
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return 0; // no header

    int frameLen = CalcFrameLength(p);
    if (frameLen <= 0 || frameLen > bytesLeft) return -frameLen; // Fake frame

    // 2) Hard limits
    if (frameLen < 96 || frameLen > 2880) return -frameLen; // is fake

    // 3) Check next header
    const uint8_t* next = p + frameLen;
    if (bytesLeft >= frameLen + 4 && next[0] == 0xFF && (next[1] & 0xE0) == 0xE0) {
        return frameLen; // echtes Frame
    }

    return -frameLen; // Fakeframe
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    MP3Decode
 *
 * Description: decode one frame of MP3 data
 *
 * Inputs:      number of valid bytes remaining in inbuf
 *              pointer to outbuf, big enough to hold one frame of decoded PCM samples
 *              flag indicating whether MP3 data is normal MPEG format (useSize = 0)
 *              or reformatted as "self-contained" frames (useSize = 1)
 *
 * Outputs:     PCM data in outbuf, interleaved LRLRLR... if stereo
 *              number of output samples = nGrans * nGranSamps * nChans
 *              updated inbuf pointer, updated bytesLeft
 *
 * Return:      error code, defined in mp3dec.h (0 means no error, < 0 means error)
 *
 * Notes:       switching useSize on and off between frames in the same stream
 *                is not supported (bit reservoir is not maintained if useSize on)
 */

int32_t MP3Decoder::decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf) {

    // Skip fake frames
    int frameLen = IsLikelyRealFrame(inbuf, *bytesLeft);
    if (frameLen == 0) {
        if (memcmp(inbuf, "APETAGEX", 8) == 0) {
            MP3_LOG_DEBUG("APETAGEX gefunden");
            uint32_t version = inbuf[8] | (inbuf[9] << 8) | (inbuf[10] << 16) | (inbuf[11] << 24);
            uint32_t size = inbuf[12] | (inbuf[13] << 8) | (inbuf[14] << 16) | (inbuf[15] << 24);
            MP3_LOG_DEBUG("version %i size %i", version, size);
            *bytesLeft -= min(*bytesLeft, (int32_t)size);
            return MP3_NEXT_FRAME;
        }
    }

    if (m_invalid_frame.start == true && m_invalid_frame.timer + 3000 > millis()) m_invalid_frame.start = false;

    if (frameLen <= 0) {
        int skip = abs(frameLen);
        if (skip > 0 && skip <= *bytesLeft) {
            *bytesLeft -= skip;
            MP3_LOG_DEBUG("Fakeframe, size %i", abs(frameLen));

            if (m_invalid_frame.start == false) { // fake frames control
                m_invalid_frame.start = true;
                m_invalid_frame.timer = millis();
                m_invalid_frame.count1 = 0;
                m_invalid_frame.count2 = 0;
            } else {
                m_invalid_frame.count1++;
                if (m_invalid_frame.start && m_invalid_frame.timer + 1000 < millis()) { m_invalid_frame.count2++; }
                if (m_invalid_frame.start && m_invalid_frame.timer + 2000 < millis()) {
                    if (m_invalid_frame.count1 > 5 && m_invalid_frame.count2 > 5) {
                        // network error
                        m_invalid_frame.start = false;
                        return MP3_NEED_RESTART;
                    }
                }
            }

            return MP3_NONE; // fakeframe
        }
        // inbuf empty or unusable
        return MP3_ERR;
    }

    int32_t        offset, bitOffset, mainBits, gr, ch, fhBytes, siBytes, freeFrameBytes;
    int32_t        prevBitOffset, sfBlockBits, huffBlockBits;
    uint8_t*       mainPtr;
    static uint8_t underflowCounter = 0; // http://macslons-irish-pub-radio.stream.laut.fm/macslons-irish-pub-radio
    /* unpack frame header */
    fhBytes = UnpackFrameHeader(inbuf);
    if (fhBytes < 0) {
        MP3_LOG_ERROR("MP3 invalid frameheader"); /* don't clear out16 since we don't know size (failed to parse header) */
        return MP3_ERR;
    }
    inbuf += fhBytes;
    /* unpack side info */
    siBytes = UnpackSideInfo(inbuf);
    if (siBytes < 0) {
        MP3ClearBadFrame(m_out16.get());
        MP3_LOG_ERROR("MP3 invalid sideinfo");
        return MP3_ERR;
    }
    inbuf += siBytes;
    *bytesLeft -= (fhBytes + siBytes);

    /* if free mode, need to calculate bitrate and nSlots manually, based on frame size */
    if (m_MP3DecInfo->bitrate == 0 || m_MP3DecInfo->freeBitrateFlag) {
        if (!m_MP3DecInfo->freeBitrateFlag) {
            /* first time through, need to scan for next sync word and figure out frame size */
            m_MP3DecInfo->freeBitrateFlag = 1;
            m_MP3DecInfo->freeBitrateSlots = MP3FindFreeSync(inbuf, inbuf - fhBytes - siBytes, *bytesLeft);
            if (m_MP3DecInfo->freeBitrateSlots < 0) {
                MP3ClearBadFrame(m_out16.get());
                m_MP3DecInfo->freeBitrateFlag = 0;
                MP3_LOG_ERROR("MP3, ca'nt find free bitrate slot");
                return MP3_ERR;
            }
            freeFrameBytes = m_MP3DecInfo->freeBitrateSlots + fhBytes + siBytes;
            m_MP3DecInfo->bitrate = (freeFrameBytes * m_MP3DecInfo->samprate * 8) / (m_MP3DecInfo->nGrans * m_MP3DecInfo->nGranSamps);
        }
        m_MP3DecInfo->nSlots = m_MP3DecInfo->freeBitrateSlots + CheckPadBit(); /* add pad byte, if required */
    }

    if (m_MP3DecInfo->nSlots > *bytesLeft) {
        MP3ClearBadFrame(m_out16.get());
        MP3_LOG_DEBUG("MP3, indata underflow");
        return MP3_MAIN_DATA_UNDERFLOW;
    }

    /* fill main data buffer with enough new data for this frame */
    if (m_MP3DecInfo->mainDataBytes >= m_MP3DecInfo->mainDataBegin) {
        /* adequate "old" main data available (i.e. bit reservoir) */
        underflowCounter = 0;
        memmove(m_MP3DecInfo->mainBuf, m_MP3DecInfo->mainBuf + m_MP3DecInfo->mainDataBytes - m_MP3DecInfo->mainDataBegin, m_MP3DecInfo->mainDataBegin);
        memcpy(m_MP3DecInfo->mainBuf + m_MP3DecInfo->mainDataBegin, inbuf, m_MP3DecInfo->nSlots);

        m_MP3DecInfo->mainDataBytes = m_MP3DecInfo->mainDataBegin + m_MP3DecInfo->nSlots;
        inbuf += m_MP3DecInfo->nSlots;
        *bytesLeft -= (m_MP3DecInfo->nSlots);
        mainPtr = m_MP3DecInfo->mainBuf;
    } else {
        /* not enough data in bit reservoir from previous frames (perhaps starting in middle of file) */
        underflowCounter++;
        memcpy(m_MP3DecInfo->mainBuf + m_MP3DecInfo->mainDataBytes, inbuf, m_MP3DecInfo->nSlots);
        m_MP3DecInfo->mainDataBytes += m_MP3DecInfo->nSlots;
        inbuf += m_MP3DecInfo->nSlots;
        *bytesLeft -= (m_MP3DecInfo->nSlots);
        if (underflowCounter < 4) { return MP3_NONE; }
        MP3ClearBadFrame(m_out16.get());
        MP3_LOG_DEBUG("MP3, maindata underflow");
        return MP3_NONE;
    }
    //    }
    bitOffset = 0;
    mainBits = m_MP3DecInfo->mainDataBytes * 8;

    /* decode one complete frame */
    for (gr = 0; gr < m_MP3DecInfo->nGrans; gr++) {
        for (ch = 0; ch < m_MP3DecInfo->nChans; ch++) {
            /* unpack scale factors and compute size of scale factor block */
            prevBitOffset = bitOffset;
            offset = UnpackScaleFactors(mainPtr, &bitOffset, mainBits, gr, ch);
            sfBlockBits = 8 * offset - prevBitOffset + bitOffset;
            huffBlockBits = m_MP3DecInfo->part23Length[gr][ch] - sfBlockBits;
            mainPtr += offset;
            mainBits -= sfBlockBits;

            if (offset < 0 || mainBits < huffBlockBits) {
                MP3ClearBadFrame(m_out16.get());
                MP3_LOG_ERROR("MP3, invalid scalefact");
                return MP3_ERR;
            }
            /* decode Huffman code words */
            prevBitOffset = bitOffset;
            offset = DecodeHuffman(mainPtr, &bitOffset, huffBlockBits, gr, ch);
            if (offset < 0) {
                MP3ClearBadFrame(m_out16.get());
                MP3_LOG_ERROR("MP3, invalid Huffman code words");
                return MP3_ERR;
            }
            mainPtr += offset;
            mainBits -= (8 * offset - prevBitOffset + bitOffset);
        }
        /* dequantize coefficients, decode stereo, reorder int16_t blocks */
        if (MP3Dequantize(gr) < 0) {
            MP3ClearBadFrame(m_out16.get());
            MP3_LOG_ERROR("MP3, invalid dequantize coefficients");
            return MP3_ERR;
        }

        /* alias reduction, inverse MDCT, overlap-add, frequency inversion */
        for (ch = 0; ch < m_MP3DecInfo->nChans; ch++) {
            if (IMDCT(gr, ch) < 0) {
                MP3ClearBadFrame(m_out16.get());
                MP3_LOG_ERROR("MP3, invalid inverse MDCT");
                return MP3_ERR;
            }
        }
        /* subband transform - if stereo, interleaves pcm LRLRLR */
        if (Subband(m_out16.get() + gr * m_MP3DecInfo->nGranSamps * m_MP3DecInfo->nChans) < 0) {
            MP3ClearBadFrame(m_out16.get());
            MP3_LOG_ERROR("MP3, invalid subband");
            return MP3_ERR;
        }
    }
    MP3GetLastFrameInfo();

    if (m_MP3FrameInfo->nChans == 1) {
        for (int i = 0; i < m_MP3FrameInfo->outputSamps; i++) {
            outbuf[i * 2] = m_out16[i] << 16;
            outbuf[i * 2 + 1] = m_out16[i] << 16;
        }
    }

    if (m_MP3FrameInfo->nChans == 2) {
        for (int i = 0; i < m_MP3FrameInfo->outputSamps * 2; i++) { outbuf[i] = m_out16[i] << 16; }
    }

    return MP3_NONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * H U F F M A N N
 */

/*
 * Function:    DecodeHuffmanPairs
 *
 * Description: decode 2-way vector Huffman codes in the "bigValues" region of spectrum
 *
 * Inputs:      valid BitStreamInfo struct, pointing to start of pair-wise codes
 *              pointer to xy buffer to received decoded values
 *              number of codewords to decode
 *              index of Huffman table to use
 *              number of bits remaining in bitstream
 *
 * Outputs:     pairs of decoded coefficients in vwxy
 *              updated BitStreamInfo struct
 *
 * Return:      number of bits used, or -1 if out of bits
 *
 * Notes:       assumes that nVals is an even number
 *              si_huff.bit tests every Huffman codeword in every table (though not
 *                necessarily all linBits outputs for x,y > 15)
 */
// no improvement with section=data

int32_t MP3Decoder::DecodeHuffmanPairs(int32_t* xy, int32_t nVals, int32_t tabIdx, int32_t bitsLeft, uint8_t* buf, int32_t bitOffset) {
    int32_t       i, x, y;
    int32_t       cachedBits, padBits, len, startBits, linBits, maxBits, minBits;
    HuffTabType_t tabType;
    uint16_t      cw, *tBase, *tCurr;
    uint32_t      cache;

    if (nVals <= 0) return 0;

    if (bitsLeft < 0) return -1;
    startBits = bitsLeft;

    tBase = (uint16_t*)(huffTable + huffTabOffset[tabIdx]);
    linBits = huffTabLookup[tabIdx].linBits;
    tabType = (HuffTabType_t)huffTabLookup[tabIdx].tabType;

    //    assert(!(nVals & 0x01));
    //    assert(tabIdx < m_HUFF_PAIRTABS);
    //    assert(tabIdx >= 0);
    //    assert(tabType != invalidTab);

    if ((nVals & 0x01)) {
        MP3_LOG_DEBUG("assert(!(nVals & 0x01))");
        return -1;
    }
    if (!(tabIdx < HUFF_PAIRTABS)) {
        MP3_LOG_DEBUG("assert(tabIdx < m_HUFF_PAIRTABS)");
        return -1;
    }
    if (!(tabIdx >= 0)) {
        MP3_LOG_DEBUG("(tabIdx >= 0)");
        return -1;
    }
    if (!(tabType != invalidTab)) {
        MP3_LOG_DEBUG("(tabType != invalidTab)");
        return -1;
    }

    /* initially fill cache with any partial byte */
    cache = 0;
    cachedBits = (8 - bitOffset) & 0x07;
    if (cachedBits) cache = (uint32_t)(*buf++) << (32 - cachedBits);
    bitsLeft -= cachedBits;

    if (tabType == noBits) {
        /* table 0, no data, x = y = 0 */
        for (i = 0; i < nVals; i += 2) {
            xy[i + 0] = 0;
            xy[i + 1] = 0;
        }
        return 0;
    } else if (tabType == oneShot) {
        /* single lookup, no escapes */

        maxBits = (int32_t)((((uint16_t)(pgm_read_word(&tBase[0])) >> 0) & 0x000f));
        tBase++;
        padBits = 0;
        while (nVals > 0) {
            /* refill cache - assumes cachedBits <= 16 */
            if (bitsLeft >= 16) {
                /* load 2 new bytes into left-justified cache */
                cache |= (uint32_t)(*buf++) << (24 - cachedBits);
                cache |= (uint32_t)(*buf++) << (16 - cachedBits);
                cachedBits += 16;
                bitsLeft -= 16;
            } else {
                /* last time through, pad cache with zeros and drain cache */
                if (cachedBits + bitsLeft <= 0) { return -1; }
                if (bitsLeft > 0) cache |= (uint32_t)(*buf++) << (24 - cachedBits);
                if (bitsLeft > 8) cache |= (uint32_t)(*buf++) << (16 - cachedBits);
                cachedBits += bitsLeft;
                bitsLeft = 0;

                cache &= (int32_t)0x80000000 >> (cachedBits - 1);
                padBits = 11;
                cachedBits += padBits; /* okay if this is > 32 (0's automatically shifted in from right) */
            }

            /* largest maxBits = 9, plus 2 for sign bits, so make sure cache has at least 11 bits */
            while (nVals > 0 && cachedBits >= 11) {
                cw = pgm_read_word(&tBase[cache >> (32 - maxBits)]);

                len = (int32_t)((((uint16_t)(cw)) >> 12) & 0x000f);
                cachedBits -= len;
                cache <<= len;

                x = (int32_t)((((uint16_t)(cw)) >> 4) & 0x000f);
                if (x) {
                    (x) |= ((cache) & 0x80000000);
                    cache <<= 1;
                    cachedBits--;
                }

                y = (int32_t)((((uint16_t)(cw)) >> 8) & 0x000f);
                if (y) {
                    (y) |= ((cache) & 0x80000000);
                    cache <<= 1;
                    cachedBits--;
                }

                /* ran out of bits - should never have consumed padBits */
                if (cachedBits < padBits) {
                    MP3_LOG_ERROR("MP3, error - overran end of bitstream"); // https://bestof80s.stream.laut.fm/best_of_80s (after advertising)
                    return MP3_ERR;
                }

                *xy++ = x;
                *xy++ = y;
                nVals -= 2;
            }
        }
        bitsLeft += (cachedBits - padBits);
        return (startBits - bitsLeft);
    } else if (tabType == loopLinbits || tabType == loopNoLinbits) {
        tCurr = tBase;
        padBits = 0;
        while (nVals > 0) {
            /* refill cache - assumes cachedBits <= 16 */
            if (bitsLeft >= 16) {
                /* load 2 new bytes into left-justified cache */
                cache |= (uint32_t)(*buf++) << (24 - cachedBits);
                cache |= (uint32_t)(*buf++) << (16 - cachedBits);
                cachedBits += 16;
                bitsLeft -= 16;
            } else {
                /* last time through, pad cache with zeros and drain cache */
                if (cachedBits + bitsLeft <= 0) { return -1; }
                if (bitsLeft > 0) cache |= (uint32_t)(*buf++) << (24 - cachedBits);
                if (bitsLeft > 8) cache |= (uint32_t)(*buf++) << (16 - cachedBits);
                cachedBits += bitsLeft;
                bitsLeft = 0;

                cache &= (int32_t)0x80000000 >> (cachedBits - 1);
                padBits = 11;
                cachedBits += padBits; /* okay if this is > 32 (0's automatically shifted in from right) */
            }

            /* largest maxBits = 9, plus 2 for sign bits, so make sure cache has at least 11 bits */
            while (nVals > 0 && cachedBits >= 11) {
                maxBits = (int32_t)((((uint16_t)(pgm_read_word(&tCurr[0]))) >> 0) & 0x000f);
                cw = pgm_read_word(&tCurr[(cache >> (32 - maxBits)) + 1]);
                len = (int32_t)((((uint16_t)(cw)) >> 12) & 0x000f);
                if (!len) {
                    cachedBits -= maxBits;
                    cache <<= maxBits;
                    tCurr += cw;
                    continue;
                }
                cachedBits -= len;
                cache <<= len;

                x = (int32_t)((((uint16_t)(cw)) >> 4) & 0x000f);
                y = (int32_t)((((uint16_t)(cw)) >> 8) & 0x000f);

                if (x == 15 && tabType == loopLinbits) {
                    minBits = linBits + 1 + (y ? 1 : 0);
                    if (cachedBits + bitsLeft < minBits) return -1;
                    while (cachedBits < minBits) {
                        cache |= (uint32_t)(*buf++) << (24 - cachedBits);
                        cachedBits += 8;
                        bitsLeft -= 8;
                    }
                    if (bitsLeft < 0) {
                        cachedBits += bitsLeft;
                        bitsLeft = 0;
                        cache &= (int32_t)0x80000000 >> (cachedBits - 1);
                    }
                    x += (int32_t)(cache >> (32 - linBits));
                    cachedBits -= linBits;
                    cache <<= linBits;
                }
                if (x) {
                    (x) |= ((cache) & 0x80000000);
                    cache <<= 1;
                    cachedBits--;
                }

                if (y == 15 && tabType == loopLinbits) {
                    minBits = linBits + 1;
                    if (cachedBits + bitsLeft < minBits) {
                        MP3_LOG_ERROR("MP3, error - overran end of bitstream"); // https://bestof80s.stream.laut.fm/best_of_80s (after advertising)
                        return MP3_ERR;
                    }
                    // return -1;
                    while (cachedBits < minBits) {
                        cache |= (uint32_t)(*buf++) << (24 - cachedBits);
                        cachedBits += 8;
                        bitsLeft -= 8;
                    }
                    if (bitsLeft < 0) {
                        cachedBits += bitsLeft;
                        bitsLeft = 0;
                        cache &= (int32_t)0x80000000 >> (cachedBits - 1);
                    }
                    y += (int32_t)(cache >> (32 - linBits));
                    cachedBits -= linBits;
                    cache <<= linBits;
                }
                if (y) {
                    (y) |= ((cache) & 0x80000000);
                    cache <<= 1;
                    cachedBits--;
                }

                /* ran out of bits - should never have consumed padBits */
                if (cachedBits < padBits) {
                    break; // https://bestof80s.stream.laut.fm/best_of_80s (after advertising)
                    // return -1;
                }

                *xy++ = x;
                *xy++ = y;
                nVals -= 2;
                tCurr = tBase;
            }
        }
        bitsLeft += (cachedBits - padBits);
        return (startBits - bitsLeft);
    }

    /* error in bitstream - trying to access unused Huffman table */
    return -1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    DecodeHuffmanQuads
 *
 * Description: decode 4-way vector Huffman codes in the "count1" region of spectrum
 *
 * Inputs:      valid BitStreamInfo struct, pointing to start of quadword codes
 *              pointer to vwxy buffer to received decoded values
 *              maximum number of codewords to decode
 *              index of quadword table (0 = table A, 1 = table B)
 *              number of bits remaining in bitstream
 *
 * Outputs:     quadruples of decoded coefficients in vwxy
 *              updated BitStreamInfo struct
 *
 * Return:      index of the first "zero_part" value (index of the first sample
 *                of the quad word after which all samples are 0)
 *
 * Notes:        si_huff.bit tests every vwxy output in both quad tables
 */
// no improvement with section=data
int32_t MP3Decoder::DecodeHuffmanQuads(int32_t* vwxy, int32_t nVals, int32_t tabIdx, int32_t bitsLeft, uint8_t* buf, int32_t bitOffset) {
    int32_t  i, v, w, x, y;
    int32_t  len, maxBits, cachedBits, padBits;
    uint32_t cache;
    uint8_t  cw, *tBase;

    if (bitsLeft <= 0) return 0;

    tBase = (uint8_t*)quadTable + quadTabOffset[tabIdx];
    maxBits = quadTabMaxBits[tabIdx];

    /* initially fill cache with any partial byte */
    cache = 0;
    cachedBits = (8 - bitOffset) & 0x07;
    if (cachedBits) cache = (uint32_t)(*buf++) << (32 - cachedBits);
    bitsLeft -= cachedBits;

    i = padBits = 0;
    while (i < (nVals - 3)) {
        /* refill cache - assumes cachedBits <= 16 */
        if (bitsLeft >= 16) {
            /* load 2 new bytes into left-justified cache */
            cache |= (uint32_t)(*buf++) << (24 - cachedBits);
            cache |= (uint32_t)(*buf++) << (16 - cachedBits);
            cachedBits += 16;
            bitsLeft -= 16;
        } else {
            /* last time through, pad cache with zeros and drain cache */
            if (cachedBits + bitsLeft <= 0) return i;
            if (bitsLeft > 0) cache |= (uint32_t)(*buf++) << (24 - cachedBits);
            if (bitsLeft > 8) cache |= (uint32_t)(*buf++) << (16 - cachedBits);
            cachedBits += bitsLeft;
            bitsLeft = 0;

            cache &= (int32_t)0x80000000 >> (cachedBits - 1);
            padBits = 10;
            cachedBits += padBits; /* okay if this is > 32 (0's automatically shifted in from right) */
        }

        /* largest maxBits = 6, plus 4 for sign bits, so make sure cache has at least 10 bits */
        while (i < (nVals - 3) && cachedBits >= 10) {
            cw = pgm_read_byte(&tBase[cache >> (32 - maxBits)]);
            len = (int32_t)((((uint8_t)(cw)) >> 4) & 0x0f);
            cachedBits -= len;
            cache <<= len;

            v = (int32_t)((((uint8_t)(cw)) >> 3) & 0x01);
            if (v) {
                (v) |= ((cache) & 0x80000000);
                cache <<= 1;
                cachedBits--;
            }
            w = (int32_t)((((uint8_t)(cw)) >> 2) & 0x01);
            if (w) {
                (w) |= ((cache) & 0x80000000);
                cache <<= 1;
                cachedBits--;
            }

            x = (int32_t)((((uint8_t)(cw)) >> 1) & 0x01);
            if (x) {
                (x) |= ((cache) & 0x80000000);
                cache <<= 1;
                cachedBits--;
            }

            y = (int32_t)((((uint8_t)(cw)) >> 0) & 0x01);
            if (y) {
                (y) |= ((cache) & 0x80000000);
                cache <<= 1;
                cachedBits--;
            }

            /* ran out of bits - okay (means we're done) */
            if (cachedBits < padBits) return i;

            *vwxy++ = v;
            *vwxy++ = w;
            *vwxy++ = x;
            *vwxy++ = y;
            i += 4;
        }
    }

    /* decoded max number of quad values */
    return i;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    DecodeHuffman
 *
 * Description: decode one granule, one channel worth of Huffman codes
 *
 * Inputs:      MP3DecInfo structure filled by UnpackFrameHeader(), UnpackSideInfo(),
 *                and UnpackScaleFactors() (for this granule)
 *              buffer pointing to start of Huffman data in MP3 frame
 *              pointer to bit offset (0-7) indicating starting bit in buf[0]
 *              number of bits in the Huffman data section of the frame
 *                (could include padding bits)
 *              index of current granule and channel
 *
 * Outputs:     decoded coefficients in hi->huffDecBuf[ch] (hi pointer in mp3DecInfo)
 *              updated bitOffset
 *
 * Return:      length (in bytes) of Huffman codes
 *              bitOffset also returned in parameter (0 = MSB, 7 = LSB of
 *                byte located at buf + offset)
 *              -1 if null input pointers, huffBlockBits < 0, or decoder runs
 *                out of bits prematurely (invalid bitstream)
 */
// .data about 1ms faster per frame
int32_t MP3Decoder::DecodeHuffman(uint8_t* buf, int32_t* bitOffset, int32_t huffBlockBits, int32_t gr, int32_t ch) {

    int32_t  r1Start, r2Start, rEnd[4]; /* region boundaries */
    int32_t  i, w, bitsUsed, bitsLeft;
    uint8_t* startBuf = buf;

    SideInfoSub_t* sis;
    sis = &m_SideInfoSub[gr][ch];
    // hi = (HuffmanInfo_t*) (m_MP3DecInfo->HuffmanInfoPS);

    if (huffBlockBits < 0) { return -1; }

    /* figure out region boundaries (the first 2*bigVals coefficients divided into 3 regions) */
    if (sis->winSwitchFlag && sis->blockType == 2) {
        if (sis->mixedBlock == 0) {
            r1Start = m_SFBandTable.s[(sis->region0Count + 1) / 3] * 3;
        } else {
            if (m_MPEGVersion == MPEG1) {
                r1Start = m_SFBandTable.l[sis->region0Count + 1];
            } else {
                /* see MPEG2 spec for explanation */
                w = m_SFBandTable.s[4] - m_SFBandTable.s[3];
                r1Start = m_SFBandTable.l[6] + 2 * w;
            }
        }
        r2Start = MAX_NSAMP; /* short blocks don't have region 2 */
    } else {
        r1Start = m_SFBandTable.l[sis->region0Count + 1];
        r2Start = m_SFBandTable.l[sis->region0Count + 1 + sis->region1Count + 1];
    }

    /* offset rEnd index by 1 so first region = rEnd[1] - rEnd[0], etc. */
    rEnd[3] = (MAX_NSAMP < (2 * sis->nBigvals) ? MAX_NSAMP : (2 * sis->nBigvals));
    rEnd[2] = (r2Start < rEnd[3] ? r2Start : rEnd[3]);
    rEnd[1] = (r1Start < rEnd[3] ? r1Start : rEnd[3]);
    rEnd[0] = 0;

    /* rounds up to first all-zero pair (we don't check last pair for (x,y) == (non-zero, zero)) */
    m_HuffmanInfo->nonZeroBound[ch] = rEnd[3];

    /* decode Huffman pairs (rEnd[i] are always even numbers) */
    bitsLeft = huffBlockBits;
    for (i = 0; i < 3; i++) {
        bitsUsed = DecodeHuffmanPairs(m_HuffmanInfo->huffDecBuf[ch] + rEnd[i], rEnd[i + 1] - rEnd[i], sis->tableSelect[i], bitsLeft, buf, *bitOffset);
        if (bitsUsed < 0 || bitsUsed > bitsLeft) { /* error - overran end of bitstream */
            return -1;
        }
        /* update bitstream position */
        buf += (bitsUsed + *bitOffset) >> 3;
        *bitOffset = (bitsUsed + *bitOffset) & 0x07;
        bitsLeft -= bitsUsed;
    }

    /* decode Huffman quads (if any) */
    m_HuffmanInfo->nonZeroBound[ch] += DecodeHuffmanQuads(m_HuffmanInfo->huffDecBuf[ch] + rEnd[3], MAX_NSAMP - rEnd[3], sis->count1TableSelect, bitsLeft, buf, *bitOffset);

    assert(m_HuffmanInfo->nonZeroBound[ch] <= MAX_NSAMP);
    for (i = m_HuffmanInfo->nonZeroBound[ch]; i < MAX_NSAMP; i++) m_HuffmanInfo->huffDecBuf[ch][i] = 0;

    /* If bits used for 576 samples < huffBlockBits, then the extras are considered
     *  to be stuffing bits (throw away, but need to return correct bitstream position)
     */
    buf += (bitsLeft + *bitOffset) >> 3;
    *bitOffset = (bitsLeft + *bitOffset) & 0x07;

    return (buf - startBuf);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * D E Q U A N T
 */

/*
 * Function:    MP3Dequantize
 *
 * Description: dequantize coefficients, decode stereo, reorder short blocks
 *                (one granule-worth)
 *
 * Inputs:      index of current granule
 *
 * Outputs:     dequantized and reordered coefficients in hi->huffDecBuf
 *                (one granule-worth, all channels), format = Q26
 *              operates in-place on huffDecBuf but also needs di->workBuf
 *              updated hi->nonZeroBound index for both channels
 *
 * Return:      0 on success, -1 if null input pointers
 *
 * Notes:       In calling output Q(DQ_FRACBITS_OUT), we assume an implicit bias
 *                of 2^15. Some (floating-point) reference implementations factor this
 *                into the 2^(0.25 * gain) scaling explicitly. But to avoid MP3Decoder::  precision
 *                loss, we don't do that. Instead take it into account in the final
 *                round to PCM (>> by 15 less than we otherwise would have).
 *              Equivalently, we can think of the dequantized coefficients as
 *                Q(DQ_FRACBITS_OUT - 15) with no implicit bias.
 */
int32_t MP3Decoder::MP3Dequantize(int32_t gr) {
    int32_t             i, ch, nSamps, mOut[2];
    CriticalBandInfo_t* cbi;
    cbi = &m_CriticalBandInfo[0];
    mOut[0] = mOut[1] = 0;

    /* dequantize all the samples in each channel */
    for (ch = 0; ch < m_MP3DecInfo->nChans; ch++) {
        m_HuffmanInfo->gb[ch] =
            DequantChannel(m_HuffmanInfo->huffDecBuf[ch], m_DequantInfo->workBuf, &m_HuffmanInfo->nonZeroBound[ch], &m_SideInfoSub[gr][ch], &m_ScaleFactorInfoSub[gr][ch], &cbi[ch]);
    }

    /* joint stereo processing assumes one guard bit in input samples
     * it's extremely rare not to have at least one gb, so if this is the case
     *   just make a pass over the data and clip to [-2^30+1, 2^30-1]
     * in practice this may never happen
     */
    if (m_FrameHeader->modeExt && (m_HuffmanInfo->gb[0] < 1 || m_HuffmanInfo->gb[1] < 1)) {
        for (i = 0; i < m_HuffmanInfo->nonZeroBound[0]; i++) {
            if (m_HuffmanInfo->huffDecBuf[0][i] < -0x3fffffff) m_HuffmanInfo->huffDecBuf[0][i] = -0x3fffffff;
            if (m_HuffmanInfo->huffDecBuf[0][i] > 0x3fffffff) m_HuffmanInfo->huffDecBuf[0][i] = 0x3fffffff;
        }
        for (i = 0; i < m_HuffmanInfo->nonZeroBound[1]; i++) {
            if (m_HuffmanInfo->huffDecBuf[1][i] < -0x3fffffff) m_HuffmanInfo->huffDecBuf[1][i] = -0x3fffffff;
            if (m_HuffmanInfo->huffDecBuf[1][i] > 0x3fffffff) m_HuffmanInfo->huffDecBuf[1][i] = 0x3fffffff;
        }
    }

    /* do mid-side stereo processing, if enabled */
    if (m_FrameHeader->modeExt >> 1) {
        if (m_FrameHeader->modeExt & 0x01) {
            /* intensity stereo enabled - run mid-side up to start of right zero region */
            if (cbi[1].cbType == 0)
                nSamps = m_SFBandTable.l[cbi[1].cbEndL + 1];
            else
                nSamps = 3 * m_SFBandTable.s[cbi[1].cbEndSMax + 1];
        } else {
            /* intensity stereo disabled - run mid-side on whole spectrum */
            nSamps = (m_HuffmanInfo->nonZeroBound[0] > m_HuffmanInfo->nonZeroBound[1] ? m_HuffmanInfo->nonZeroBound[0] : m_HuffmanInfo->nonZeroBound[1]);
        }
        MidSideProc(m_HuffmanInfo->huffDecBuf, nSamps, mOut);
    }

    /* do intensity stereo processing, if enabled */
    if (m_FrameHeader->modeExt & 0x01) {
        nSamps = m_HuffmanInfo->nonZeroBound[0];
        if (m_MPEGVersion == MPEG1) {
            IntensityProcMPEG1(m_HuffmanInfo->huffDecBuf, nSamps, &m_ScaleFactorInfoSub[gr][1], &m_CriticalBandInfo[0], m_FrameHeader->modeExt >> 1, m_SideInfoSub[gr][1].mixedBlock, mOut);
        } else {
            IntensityProcMPEG2(m_HuffmanInfo->huffDecBuf, nSamps, &m_ScaleFactorInfoSub[gr][1], &m_CriticalBandInfo[0], m_ScaleFactorJS.get(), m_FrameHeader->modeExt >> 1,
                               m_SideInfoSub[gr][1].mixedBlock, mOut);
        }
    }

    /* adjust guard bit count and nonZeroBound if we did any stereo processing */
    if (m_FrameHeader->modeExt) {
        m_HuffmanInfo->gb[0] = CLZ(mOut[0]) - 1;
        m_HuffmanInfo->gb[1] = CLZ(mOut[1]) - 1;
        nSamps = (m_HuffmanInfo->nonZeroBound[0] > m_HuffmanInfo->nonZeroBound[1] ? m_HuffmanInfo->nonZeroBound[0] : m_HuffmanInfo->nonZeroBound[1]);
        m_HuffmanInfo->nonZeroBound[0] = nSamps;
        m_HuffmanInfo->nonZeroBound[1] = nSamps;
    }

    /* output format Q(DQ_FRACBITS_OUT) */
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * D Q C H A N
 */

/*
 * Function:    DequantBlock
 *
 * Description: Ken's highly-optimized, low memory dequantizer performing the operation
 *              y = pow(x, 4.0/3.0) * pow(2, 25 - scale/4.0)
 *
 * Inputs:      input buffer of decode Huffman codewords (signed-magnitude)
 *              output buffer of same length (in-place (outbuf = inbuf) is allowed)
 *              number of samples
 *
 * Outputs:     dequantized samples in Q25 format
 *
 * Return:      bitwise-OR of the unsigned outputs (for guard bit calculations)
 */
int32_t MP3Decoder::DequantBlock(int32_t* inbuf, int32_t* outbuf, int32_t num, int32_t scale) {
    int32_t         tab4[4];
    int32_t         scalef, scalei, shift;
    int32_t         sx, x, y;
    int32_t         mask = 0;
    const int32_t*  tab16;
    const uint32_t* coef;

    tab16 = pow43_14[scale & 0x3];
    scalef = pow14[scale & 0x3];
    scalei = ((scale >> 2) < 31 ? (scale >> 2) : 31);
    // scalei = MIN(scale >> 2, 31);   /* smallest input scale = -47, so smallest scalei = -12 */

    /* cache first 4 values */
    shift = (scalei + 3 < 31 ? scalei + 3 : 31);
    shift = (shift > 0 ? shift : 0);

    tab4[0] = 0;
    tab4[1] = tab16[1] >> shift;
    tab4[2] = tab16[2] >> shift;
    tab4[3] = tab16[3] >> shift;

    do {
        sx = *inbuf++;
        x = sx & 0x7fffffff; /* sx = sign|mag */
        if (x < 4) {
            y = tab4[x];
        } else if (x < 16) {
            y = tab16[x];
            y = (scalei < 0) ? y << -scalei : y >> scalei;
        } else {
            if (x < 64) {
                y = pow43[x - 16];
                /* fractional scale */
                y = MULSHIFT32(y, scalef);
                shift = scalei - 3;
            } else {
                /* normalize to [0x40000000, 0x7fffffff] */
                x <<= 17;
                shift = 0;
                if (x < 0x08000000) x <<= 4, shift += 4;
                if (x < 0x20000000) x <<= 2, shift += 2;
                if (x < 0x40000000) x <<= 1, shift += 1;

                coef = (x < SQRTHALF) ? poly43lo : poly43hi;

                /* polynomial */
                y = coef[0];
                y = MULSHIFT32(y, x) + coef[1];
                y = MULSHIFT32(y, x) + coef[2];
                y = MULSHIFT32(y, x) + coef[3];
                y = MULSHIFT32(y, x) + coef[4];
                y = MULSHIFT32(y, pow2frac[shift]) << 3;

                /* fractional scale */
                y = MULSHIFT32(y, scalef);
                shift = scalei - pow2exp[shift];
            }

            /* integer scale */
            if (shift < 0) {
                shift = -shift;
                if (y > (0x7fffffff >> shift))
                    y = 0x7fffffff; /* clip */
                else
                    y <<= shift;
            } else {
                y >>= shift;
            }
        }

        /* sign and store */
        mask |= y;
        *outbuf++ = (sx < 0) ? -y : y;

    } while (--num);

    return mask;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    DequantChannel
 *
 * Description: dequantize one granule, one channel worth of decoded Huffman codewords
 *
 * Inputs:      sample buffer (decoded Huffman codewords), length = m_MAX_NSAMP samples
 *              work buffer for reordering short-block, length = m_MAX_REORDER_SAMPS
 *                samples (3 * width of largest short-block critical band)
 *              non-zero bound for this channel/granule
 *              valid FrameHeader, SideInfoSub, ScaleFactorInfoSub, and CriticalBandInfo
 *                structures for this channel/granule
 *
 * Outputs:     MAX_NSAMP dequantized samples in sampleBuf
 *              updated non-zero bound (indicating which samples are != 0 after DQ)
 *              filled-in cbi structure indicating start and end critical bands
 *
 * Return:      minimum number of guard bits in dequantized sampleBuf
 *
 * Notes:       dequantized samples in Q(DQ_FRACBITS_OUT) format
 */
int32_t MP3Decoder::DequantChannel(int32_t* sampleBuf, int32_t* workBuf, int32_t* nonZeroBound, SideInfoSub_t* sis, ScaleFactorInfoSub_t* sfis, CriticalBandInfo_t* cbi) {
    int32_t                 i, j, w, cb;
    int32_t /* cbStartL, */ cbEndL, cbStartS, cbEndS;
    int32_t                 nSamps, nonZero, sfactMultiplier, gbMask;
    int32_t                 globalGain, gainI;
    int32_t                 cbMax[3];
    typedef int32_t         ARRAY3[3]; /* for short-block reordering */
    ARRAY3*                 buf;       /* short block reorder */

    /* set default start/end points for short/long blocks - will update with non-zero cb info */
    if (sis->blockType == 2) {
        // cbStartL = 0;
        if (sis->mixedBlock) {
            cbEndL = (m_MPEGVersion == MPEG1 ? 8 : 6);
            cbStartS = 3;
        } else {
            cbEndL = 0;
            cbStartS = 0;
        }
        cbEndS = 13;
    } else {
        /* long block */
        // cbStartL = 0;
        cbEndL = 22;
        cbStartS = 13;
        cbEndS = 13;
    }
    cbMax[2] = cbMax[1] = cbMax[0] = 0;
    gbMask = 0;
    i = 0;

    /* sfactScale = 0 --> quantizer step size = 2
     * sfactScale = 1 --> quantizer step size = sqrt(2)
     *   so sfactMultiplier = 2 or 4 (jump through globalGain by powers of 2 or sqrt(2))
     */
    sfactMultiplier = 2 * (sis->sfactScale + 1);

    /* offset globalGain by -2 if midSide enabled, for 1/sqrt(2) used in MidSideProc()
     *  (DequantBlock() does 0.25 * gainI so knocking it down by two is the same as
     *   dividing every sample by sqrt(2) = multiplying by 2^-.5)
     */
    globalGain = sis->globalGain;
    if (m_FrameHeader->modeExt >> 1) globalGain -= 2;
    globalGain += IMDCT_SCALE; /* scale everything by sqrt(2), for fast IMDCT36 */

    /* long blocks */
    for (cb = 0; cb < cbEndL; cb++) {

        nonZero = 0;
        nSamps = m_SFBandTable.l[cb + 1] - m_SFBandTable.l[cb];
        gainI = 210 - globalGain + sfactMultiplier * (sfis->l[cb] + (sis->preFlag ? (int32_t)preTab[cb] : 0));

        nonZero |= DequantBlock(sampleBuf + i, sampleBuf + i, nSamps, gainI);
        i += nSamps;

        /* update highest non-zero critical band */
        if (nonZero) cbMax[0] = cb;
        gbMask |= nonZero;

        if (i >= *nonZeroBound) break;
    }

    /* set cbi (Type, EndS[], EndSMax will be overwritten if we proceed to do short blocks) */
    cbi->cbType = 0; /* long only */
    cbi->cbEndL = cbMax[0];
    cbi->cbEndS[0] = cbi->cbEndS[1] = cbi->cbEndS[2] = 0;
    cbi->cbEndSMax = 0;

    /* early exit if no short blocks */
    if (cbStartS >= 12) return CLZ(gbMask) - 1;

    /* short blocks */
    cbMax[2] = cbMax[1] = cbMax[0] = cbStartS;
    for (cb = cbStartS; cb < cbEndS; cb++) {

        nSamps = m_SFBandTable.s[cb + 1] - m_SFBandTable.s[cb];
        for (w = 0; w < 3; w++) {
            nonZero = 0;
            gainI = 210 - globalGain + 8 * sis->subBlockGain[w] + sfactMultiplier * (sfis->s[cb][w]);

            nonZero |= DequantBlock(sampleBuf + i + nSamps * w, workBuf + nSamps * w, nSamps, gainI);

            /* update highest non-zero critical band */
            if (nonZero) cbMax[w] = cb;
            gbMask |= nonZero;
        }

        /* reorder blocks */
        buf = (ARRAY3*)(sampleBuf + i);
        i += 3 * nSamps;
        for (j = 0; j < nSamps; j++) {
            buf[j][0] = workBuf[0 * nSamps + j];
            buf[j][1] = workBuf[1 * nSamps + j];
            buf[j][2] = workBuf[2 * nSamps + j];
        }

        assert(3 * nSamps <= MAX_REORDER_SAMPS);

        if (i >= *nonZeroBound) break;
    }

    /* i = last non-zero INPUT sample processed, which corresponds to highest possible non-zero
     *     OUTPUT sample (after reorder)
     * however, the original nzb is no longer necessarily true
     *   for each cb, buf[][] is updated with 3*nSamps samples (i increases 3*nSamps each time)
     *   (buf[j + 1][0] = 3 (input) samples ahead of buf[j][0])
     * so update nonZeroBound to i
     */
    *nonZeroBound = i;

    assert(*nonZeroBound <= MAX_NSAMP);

    cbi->cbType = (sis->mixedBlock ? 2 : 1); /* 2 = mixed short/long, 1 = short only */

    cbi->cbEndS[0] = cbMax[0];
    cbi->cbEndS[1] = cbMax[1];
    cbi->cbEndS[2] = cbMax[2];

    cbi->cbEndSMax = cbMax[0];
    cbi->cbEndSMax = (cbi->cbEndSMax > cbMax[1] ? cbi->cbEndSMax : cbMax[1]);
    cbi->cbEndSMax = (cbi->cbEndSMax > cbMax[2] ? cbi->cbEndSMax : cbMax[2]);

    return CLZ(gbMask) - 1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * S T P R O C
 */

/*
 * Function:    MidSideProc
 *
 * Description: sum-difference stereo reconstruction
 *
 * Inputs:      vector x with dequantized samples from left and right channels
 *              number of non-zero samples (MAX of left and right)
 *              assume 1 guard bit in input
 *              guard bit mask (left and right channels)
 *
 * Outputs:     updated sample vector x
 *              updated guard bit mask
 *
 * Return:      none
 *
 * Notes:       assume at least 1 GB in input
 */
void MP3Decoder::MidSideProc(int32_t x[MAX_NCHAN][MAX_NSAMP], int32_t nSamps, int32_t mOut[2]) {
    int32_t i, xr, xl, mOutL, mOutR;

    /* L = (M+S)/sqrt(2), R = (M-S)/sqrt(2)
     * NOTE: 1/sqrt(2) done in DequantChannel() - see comments there
     */
    mOutL = mOutR = 0;
    for (i = 0; i < nSamps; i++) {
        xl = x[0][i];
        xr = x[1][i];
        x[0][i] = xl + xr;
        x[1][i] = xl - xr;
        mOutL |= FASTABS(x[0][i]);
        mOutR |= FASTABS(x[1][i]);
    }
    mOut[0] |= mOutL;
    mOut[1] |= mOutR;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    IntensityProcMPEG1
 *
 * Description: intensity stereo processing for MPEG1
 *
 * Inputs:      vector x with dequantized samples from left and right channels
 *              number of non-zero samples in left channel
 *              valid FrameHeader struct
 *              two each of ScaleFactorInfoSub, CriticalBandInfo structs (both channels)
 *              flags indicating midSide on/off, mixedBlock on/off
 *              guard bit mask (left and right channels)
 *
 * Outputs:     updated sample vector x
 *              updated guard bit mask
 *
 * Return:      none
 *
 * Notes:       assume at least 1 GB in input
 *
 */
void MP3Decoder::IntensityProcMPEG1(int32_t x[MAX_NCHAN][MAX_NSAMP], int32_t nSamps, ScaleFactorInfoSub_t* sfis, CriticalBandInfo_t* cbi, int32_t midSideFlag, int32_t mixFlag, int32_t mOut[2]) {
    int32_t  i = 0, j = 0, n = 0, cb = 0, w = 0;
    int32_t  sampsLeft, isf, mOutL, mOutR, xl, xr;
    int32_t  fl, fr, fls[3], frs[3];
    int32_t  cbStartL = 0, cbStartS = 0, cbEndL = 0, cbEndS = 0;
    int32_t* isfTab;
    (void)mixFlag;

    /* NOTE - this works fine for mixed blocks, as long as the switch point starts in the
     *  short block section (i.e. on or after sample 36 = sfBand->l[8] = 3*sfBand->s[3]
     * is this a safe assumption?
     */
    if (cbi[1].cbType == 0) {
        /* long block */
        cbStartL = cbi[1].cbEndL + 1;
        cbEndL = cbi[0].cbEndL + 1;
        cbStartS = cbEndS = 0;
        i = m_SFBandTable.l[cbStartL];
    } else if (cbi[1].cbType == 1 || cbi[1].cbType == 2) {
        /* short or mixed block */
        cbStartS = cbi[1].cbEndSMax + 1;
        cbEndS = cbi[0].cbEndSMax + 1;
        cbStartL = cbEndL = 0;
        i = 3 * m_SFBandTable.s[cbStartS];
    }
    sampsLeft = nSamps - i; /* process to length of left */
    isfTab = (int32_t*)ISFMpeg1[midSideFlag];
    mOutL = mOutR = 0;

    /* long blocks */
    for (cb = cbStartL; cb < cbEndL && sampsLeft > 0; cb++) {
        isf = sfis->l[cb];
        if (isf == 7) {
            fl = ISFIIP[midSideFlag][0];
            fr = ISFIIP[midSideFlag][1];
        } else {
            fl = isfTab[isf];
            fr = isfTab[6] - isfTab[isf];
        }

        n = m_SFBandTable.l[cb + 1] - m_SFBandTable.l[cb];
        for (j = 0; j < n && sampsLeft > 0; j++, i++) {
            xr = MULSHIFT32(fr, x[0][i]) << 2;
            x[1][i] = xr;
            mOutR |= FASTABS(xr);
            xl = MULSHIFT32(fl, x[0][i]) << 2;
            x[0][i] = xl;
            mOutL |= FASTABS(xl);
            sampsLeft--;
        }
    }
    /* short blocks */
    for (cb = cbStartS; cb < cbEndS && sampsLeft >= 3; cb++) {
        for (w = 0; w < 3; w++) {
            isf = sfis->s[cb][w];
            if (isf == 7) {
                fls[w] = ISFIIP[midSideFlag][0];
                frs[w] = ISFIIP[midSideFlag][1];
            } else {
                fls[w] = isfTab[isf];
                frs[w] = isfTab[6] - isfTab[isf];
            }
        }
        n = m_SFBandTable.s[cb + 1] - m_SFBandTable.s[cb];
        for (j = 0; j < n && sampsLeft >= 3; j++, i += 3) {
            xr = MULSHIFT32(frs[0], x[0][i + 0]) << 2;
            x[1][i + 0] = xr;
            mOutR |= FASTABS(xr);
            xl = MULSHIFT32(fls[0], x[0][i + 0]) << 2;
            x[0][i + 0] = xl;
            mOutL |= FASTABS(xl);
            xr = MULSHIFT32(frs[1], x[0][i + 1]) << 2;
            x[1][i + 1] = xr;
            mOutR |= FASTABS(xr);
            xl = MULSHIFT32(fls[1], x[0][i + 1]) << 2;
            x[0][i + 1] = xl;
            mOutL |= FASTABS(xl);
            xr = MULSHIFT32(frs[2], x[0][i + 2]) << 2;
            x[1][i + 2] = xr;
            mOutR |= FASTABS(xr);
            xl = MULSHIFT32(fls[2], x[0][i + 2]) << 2;
            x[0][i + 2] = xl;
            mOutL |= FASTABS(xl);
            sampsLeft -= 3;
        }
    }
    mOut[0] = mOutL;
    mOut[1] = mOutR;
    return;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    IntensityProcMPEG2
 *
 * Description: intensity stereo processing for MPEG2
 *
 * Inputs:      vector x with dequantized samples from left and right channels
 *              number of non-zero samples in left channel
 *              valid FrameHeader struct
 *              two each of ScaleFactorInfoSub, CriticalBandInfo structs (both channels)
 *              ScaleFactorJS struct with joint stereo info from UnpackSFMPEG2()
 *              flags indicating midSide on/off, mixedBlock on/off
 *              guard bit mask (left and right channels)
 *
 * Outputs:     updated sample vector x
 *              updated guard bit mask
 *
 * Return:      none
 *
 * Notes:       assume at least 1 GB in input
 *
 */
void MP3Decoder::IntensityProcMPEG2(int32_t x[MAX_NCHAN][MAX_NSAMP], int32_t nSamps, ScaleFactorInfoSub_t* sfis, CriticalBandInfo_t* cbi, ScaleFactorJS_t* sfjs, int32_t midSideFlag, int32_t mixFlag,
                                    int32_t mOut[2]) {
    int32_t  i, j, k, n, r, cb, w;
    int32_t  fl, fr, mOutL, mOutR, xl, xr;
    int32_t  sampsLeft;
    int32_t  isf, sfIdx, tmp, il[23];
    int32_t* isfTab;
    int32_t  cbStartL, cbStartS, cbEndL, cbEndS;

    (void)mixFlag;

    isfTab = (int32_t*)ISFMpeg2[sfjs->intensityScale][midSideFlag];
    mOutL = mOutR = 0;

    /* fill buffer with illegal intensity positions (depending on slen) */
    for (k = r = 0; r < 4; r++) {
        tmp = (1 << sfjs->slen[r]) - 1;
        for (j = 0; j < sfjs->nr[r]; j++, k++) il[k] = tmp;
    }

    if (cbi[1].cbType == 0) {
        /* long blocks */
        il[21] = il[22] = 1;
        cbStartL = cbi[1].cbEndL + 1; /* start at end of right */
        cbEndL = cbi[0].cbEndL + 1;   /* process to end of left */
        i = m_SFBandTable.l[cbStartL];
        sampsLeft = nSamps - i;

        for (cb = cbStartL; cb < cbEndL; cb++) {
            sfIdx = sfis->l[cb];
            if (sfIdx == il[cb]) {
                fl = ISFIIP[midSideFlag][0];
                fr = ISFIIP[midSideFlag][1];
            } else {
                isf = (sfis->l[cb] + 1) >> 1;
                fl = isfTab[(sfIdx & 0x01 ? isf : 0)];
                fr = isfTab[(sfIdx & 0x01 ? 0 : isf)];
            }
            int32_t r = m_SFBandTable.l[cb + 1] - m_SFBandTable.l[cb];
            n = (r < sampsLeft ? r : sampsLeft);
            // n = MIN(fh->sfBand->l[cb + 1] - fh->sfBand->l[cb], sampsLeft);
            for (j = 0; j < n; j++, i++) {
                xr = MULSHIFT32(fr, x[0][i]) << 2;
                x[1][i] = xr;
                mOutR |= FASTABS(xr);
                xl = MULSHIFT32(fl, x[0][i]) << 2;
                x[0][i] = xl;
                mOutL |= FASTABS(xl);
            }
            /* early exit once we've used all the non-zero samples */
            sampsLeft -= n;
            if (sampsLeft == 0) break;
        }
    } else {
        /* short or mixed blocks */
        il[12] = 1;

        for (w = 0; w < 3; w++) {
            cbStartS = cbi[1].cbEndS[w] + 1; /* start at end of right */
            cbEndS = cbi[0].cbEndS[w] + 1;   /* process to end of left */
            i = 3 * m_SFBandTable.s[cbStartS] + w;

            /* skip through sample array by 3, so early-exit logic would be more tricky */
            for (cb = cbStartS; cb < cbEndS; cb++) {
                sfIdx = sfis->s[cb][w];
                if (sfIdx == il[cb]) {
                    fl = ISFIIP[midSideFlag][0];
                    fr = ISFIIP[midSideFlag][1];
                } else {
                    isf = (sfis->s[cb][w] + 1) >> 1;
                    fl = isfTab[(sfIdx & 0x01 ? isf : 0)];
                    fr = isfTab[(sfIdx & 0x01 ? 0 : isf)];
                }
                n = m_SFBandTable.s[cb + 1] - m_SFBandTable.s[cb];

                for (j = 0; j < n; j++, i += 3) {
                    xr = MULSHIFT32(fr, x[0][i]) << 2;
                    x[1][i] = xr;
                    mOutR |= FASTABS(xr);
                    xl = MULSHIFT32(fl, x[0][i]) << 2;
                    x[0][i] = xl;
                    mOutL |= FASTABS(xl);
                }
            }
        }
    }
    mOut[0] = mOutL;
    mOut[1] = mOutR;
    return;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * I M D C T
 */

/*
 * Function:    AntiAlias
 *
 * Description: smooth transition across DCT block boundaries (every 18 coefficients)
 *
 * Inputs:      vector of dequantized coefficients, length = (nBfly+1) * 18
 *              number of "butterflies" to perform (one butterfly means one
 *                inter-block smoothing operation)
 *
 * Outputs:     updated coefficient vector x
 *
 * Return:      none
 *
 * Notes:       weighted average of opposite bands (pairwise) from the 8 samples
 *                before and after each block boundary
 *              nBlocks = (nonZeroBound + 7) / 18, since nZB is the first ZERO sample
 *                above which all other samples are also zero
 *              max gain per sample = 1.372
 *                MAX(i) (abs(csa[i][0]) + abs(csa[i][1]))
 *              bits gained = 0
 *              assume at least 1 guard bit in x[] to avoid MP3Decoder::  overflow
 *                (should be guaranteed from dequant, and max gain from stproc * max
 *                 gain from AntiAlias < 2.0)
 */
// a little bit faster in RAM (< 1 ms per block)
/* __attribute__ ((section (".data"))) */
void MP3Decoder::AntiAlias(int32_t* x, int32_t nBfly) {
    int32_t         k, a0, b0, c0, c1;
    const uint32_t* c;

    /* csa = Q31 */
    for (k = nBfly; k > 0; k--) {
        c = csa[0];
        x += 18;
        a0 = x[-1];
        c0 = *c;
        c++;
        b0 = x[0];
        c1 = *c;
        c++;
        x[-1] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[0] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-2];
        c0 = *c;
        c++;
        b0 = x[1];
        c1 = *c;
        c++;
        x[-2] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[1] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-3];
        c0 = *c;
        c++;
        b0 = x[2];
        c1 = *c;
        c++;
        x[-3] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[2] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-4];
        c0 = *c;
        c++;
        b0 = x[3];
        c1 = *c;
        c++;
        x[-4] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[3] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-5];
        c0 = *c;
        c++;
        b0 = x[4];
        c1 = *c;
        c++;
        x[-5] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[4] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-6];
        c0 = *c;
        c++;
        b0 = x[5];
        c1 = *c;
        c++;
        x[-6] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[5] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-7];
        c0 = *c;
        c++;
        b0 = x[6];
        c1 = *c;
        c++;
        x[-7] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[6] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;

        a0 = x[-8];
        c0 = *c;
        c++;
        b0 = x[7];
        c1 = *c;
        c++;
        x[-8] = (MULSHIFT32(c0, a0) - MULSHIFT32(c1, b0)) << 1;
        x[7] = (MULSHIFT32(c0, b0) + MULSHIFT32(c1, a0)) << 1;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    WinPrevious
 *
 * Description: apply specified window to second half of previous IMDCT (overlap part)
 *
 * Inputs:      vector of 9 coefficients (xPrev)
 *
 * Outputs:     18 windowed output coefficients (gain 1 integer bit)
 *              window type (0, 1, 2, 3)
 *
 * Return:      none
 *
 * Notes:       produces 9 output samples from 18 input samples via symmetry
 *              all blocks gain at least 1 guard bit via window (long blocks get extra
 *                sign bit, short blocks can have one addition but max gain < 1.0)
 */

void MP3Decoder::WinPrevious(int32_t* xPrev, int32_t* xPrevWin, int32_t btPrev) {
    int32_t         i, x, *xp, *xpwLo, *xpwHi, wLo, wHi;
    const uint32_t *wpLo, *wpHi;

    xp = xPrev;
    /* mapping (see IMDCT12x3): xPrev[0-2] = sum[6-8], xPrev[3-8] = sum[12-17] */
    if (btPrev == 2) {
        /* this could be reordered for minimum loads/stores */
        wpLo = imdctWin[btPrev];
        xPrevWin[0] = MULSHIFT32(wpLo[6], xPrev[2]) + MULSHIFT32(wpLo[0], xPrev[6]);
        xPrevWin[1] = MULSHIFT32(wpLo[7], xPrev[1]) + MULSHIFT32(wpLo[1], xPrev[7]);
        xPrevWin[2] = MULSHIFT32(wpLo[8], xPrev[0]) + MULSHIFT32(wpLo[2], xPrev[8]);
        xPrevWin[3] = MULSHIFT32(wpLo[9], xPrev[0]) + MULSHIFT32(wpLo[3], xPrev[8]);
        xPrevWin[4] = MULSHIFT32(wpLo[10], xPrev[1]) + MULSHIFT32(wpLo[4], xPrev[7]);
        xPrevWin[5] = MULSHIFT32(wpLo[11], xPrev[2]) + MULSHIFT32(wpLo[5], xPrev[6]);
        xPrevWin[6] = MULSHIFT32(wpLo[6], xPrev[5]);
        xPrevWin[7] = MULSHIFT32(wpLo[7], xPrev[4]);
        xPrevWin[8] = MULSHIFT32(wpLo[8], xPrev[3]);
        xPrevWin[9] = MULSHIFT32(wpLo[9], xPrev[3]);
        xPrevWin[10] = MULSHIFT32(wpLo[10], xPrev[4]);
        xPrevWin[11] = MULSHIFT32(wpLo[11], xPrev[5]);
        xPrevWin[12] = xPrevWin[13] = xPrevWin[14] = xPrevWin[15] = xPrevWin[16] = xPrevWin[17] = 0;
    } else {
        /* use ARM-style pointers (*ptr++) so that ADS compiles well */
        wpLo = imdctWin[btPrev] + 18;
        wpHi = wpLo + 17;
        xpwLo = xPrevWin;
        xpwHi = xPrevWin + 17;
        for (i = 9; i > 0; i--) {
            x = *xp++;
            wLo = *wpLo++;
            wHi = *wpHi--;
            *xpwLo++ = MULSHIFT32(wLo, x);
            *xpwHi-- = MULSHIFT32(wHi, x);
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    FreqInvertRescale
 *
 * Description: do frequency inversion (odd samples of odd blocks) and rescale
 *                if necessary (extra guard bits added before IMDCT)
 *
 * Inputs:      output vector y (18 new samples, spaced NBANDS apart)
 *              previous sample vector xPrev (9 samples)
 *              index of current block
 *              number of extra shifts added before IMDCT (usually 0)
 *
 * Outputs:     inverted and rescaled (as necessary) outputs
 *              rescaled (as necessary) previous samples
 *
 * Return:      updated mOut (from new outputs y)
 */

int32_t MP3Decoder::FreqInvertRescale(int32_t* y, int32_t* xPrev, int32_t blockIdx, int32_t es) {

    if (es == 0) {
        /* fast case - frequency invert only (no rescaling) */
        if (blockIdx & 0x01) {
            y += NBANDS;
            for (int32_t i = 0; i < 9; i++) {
                *y = -*y;
                y += 2 * NBANDS;
            }
        }
        return 0;
    }

    int32_t d, mOut;
    /* undo pre-IMDCT scaling, clipping if necessary */
    mOut = 0;
    if (blockIdx & 0x01) {
        /* frequency invert */
        for (int32_t i = 0; i < 9; i++) {
            d = *y;
            CLIP_2N(d, (31 - es));
            *y = d << es;
            mOut |= FASTABS(*y);
            y += NBANDS;
            d = -*y;
            CLIP_2N(d, (31 - es));
            *y = d << es;
            mOut |= FASTABS(*y);
            y += NBANDS;
            d = *xPrev;
            CLIP_2N(d, (31 - es));
            *xPrev++ = d << es;
        }
    } else {
        for (int32_t i = 0; i < 9; i++) {
            d = *y;
            CLIP_2N(d, (31 - es));
            *y = d << es;
            mOut |= FASTABS(*y);
            y += NBANDS;
            d = *y;
            CLIP_2N(d, (31 - es));
            *y = d << es;
            mOut |= FASTABS(*y);
            y += NBANDS;
            d = *xPrev;
            CLIP_2N(d, (31 - es));
            *xPrev++ = d << es;
        }
    }
    return mOut;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* require at least 3 guard bits in x[] to ensure no overflow */
void MP3Decoder::idct9(int32_t* x) {
    int32_t a1, a2, a3, a4, a5, a6, a7, a8, a9;
    int32_t a10, a11, a12, a13, a14, a15, a16, a17, a18;
    int32_t a19, a20, a21, a22, a23, a24, a25, a26, a27;
    int32_t m1, m3, m5, m6, m7, m8, m9, m10, m11, m12;
    int32_t x0, x1, x2, x3, x4, x5, x6, x7, x8;

    x0 = x[0];
    x1 = x[1];
    x2 = x[2];
    x3 = x[3];
    x4 = x[4];
    x5 = x[5];
    x6 = x[6];
    x7 = x[7];
    x8 = x[8];

    a1 = x0 - x6;
    a2 = x1 - x5;
    a3 = x1 + x5;
    a4 = x2 - x4;
    a5 = x2 + x4;
    a6 = x2 + x8;
    a7 = x1 + x7;

    a8 = a6 - a5;  /* ie x[8] - x[4] */
    a9 = a3 - a7;  /* ie x[5] - x[7] */
    a10 = a2 - x7; /* ie x[1] - x[5] - x[7] */
    a11 = a4 - x8; /* ie x[2] - x[4] - x[8] */

    /* do the << 1 as constant shifts where mX is actually used (free, no stall or extra inst.) */
    m1 = MULSHIFT32(c9_0, x3);
    m3 = MULSHIFT32(c9_0, a10);
    m5 = MULSHIFT32(c9_1, a5);
    m6 = MULSHIFT32(c9_2, a6);
    m7 = MULSHIFT32(c9_1, a8);
    m8 = MULSHIFT32(c9_2, a5);
    m9 = MULSHIFT32(c9_3, a9);
    m10 = MULSHIFT32(c9_4, a7);
    m11 = MULSHIFT32(c9_3, a3);
    m12 = MULSHIFT32(c9_4, a9);

    a12 = x[0] + (x[6] >> 1);
    a13 = a12 + (m1 << 1);
    a14 = a12 - (m1 << 1);
    a15 = a1 + (a11 >> 1);
    a16 = (m5 << 1) + (m6 << 1);
    a17 = (m7 << 1) - (m8 << 1);
    a18 = a16 + a17;
    a19 = (m9 << 1) + (m10 << 1);
    a20 = (m11 << 1) - (m12 << 1);

    a21 = a20 - a19;
    a22 = a13 + a16;
    a23 = a14 + a16;
    a24 = a14 + a17;
    a25 = a13 + a17;
    a26 = a14 - a18;
    a27 = a13 - a18;

    x0 = a22 + a19;
    x[0] = x0;
    x1 = a15 + (m3 << 1);
    x[1] = x1;
    x2 = a24 + a20;
    x[2] = x2;
    x3 = a26 - a21;
    x[3] = x3;
    x4 = a1 - a11;
    x[4] = x4;
    x5 = a27 + a21;
    x[5] = x5;
    x6 = a25 - a20;
    x[6] = x6;
    x7 = a15 - (m3 << 1);
    x[7] = x7;
    x8 = a23 - a19;
    x[8] = x8;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    IMDCT36
 *
 * Description: 36-point modified DCT, with windowing and overlap-add (50% overlap)
 *
 * Inputs:      vector of 18 coefficients (N/2 inputs produces N outputs, by symmetry)
 *              overlap part of last IMDCT (9 samples - see output comments)
 *              window type (0,1,2,3) of current and previous block
 *              current block index (for deciding whether to do frequency inversion)
 *              number of guard bits in input vector
 *
 * Outputs:     18 output samples, after windowing and overlap-add with last frame
 *              second half of (unwindowed) 36-point IMDCT - save for next time
 *                only save 9 xPrev samples, using symmetry (see WinPrevious())
 *
 * Notes:       this is Ken's hyper-fast algorithm, including symmetric sin window
 *                optimization, if applicable
 *              total number of multiplies, general case:
 *                2*10 (idct9) + 9 (last stage imdct) + 36 (for windowing) = 65
 *              total number of multiplies, btCurr == 0 && btPrev == 0:
 *                2*10 (idct9) + 9 (last stage imdct) + 18 (for windowing) = 47
 *
 *              blockType == 0 is by far the most common case, so it should be
 *                possible to use the fast path most of the time
 *              this is the fastest known algorithm for performing
 *                long IMDCT + windowing + overlap-add in MP3
 *
 * Return:      mOut (OR of abs(y) for all y calculated here)
 */
// barely faster in RAM

int32_t MP3Decoder::IMDCT36(int32_t* xCurr, int32_t* xPrev, int32_t* y, int32_t btCurr, int32_t btPrev, int32_t blockIdx, int32_t gb) {
    int32_t         i, es, xBuf[18], xPrevWin[18];
    int32_t         acc1, acc2, s, d, t, mOut;
    int32_t         xo, xe, c, *xp, yLo, yHi;
    const uint32_t *cp, *wp;
    acc1 = acc2 = 0;
    xCurr += 17;
    /* 7 gb is always adequate for antialias + accumulator loop + idct9 */
    if (gb < 7) {
        /* rarely triggered - 5% to 10% of the time on normal clips (with Q25 input) */
        es = 7 - gb;
        for (i = 8; i >= 0; i--) {
            acc1 = ((*xCurr--) >> es) - acc1;
            acc2 = acc1 - acc2;
            acc1 = ((*xCurr--) >> es) - acc1;
            xBuf[i + 9] = acc2; /* odd */
            xBuf[i + 0] = acc1; /* even */
            xPrev[i] >>= es;
        }
    } else {
        es = 0;
        /* max gain = 18, assume adequate guard bits */
        for (i = 8; i >= 0; i--) {
            acc1 = (*xCurr--) - acc1;
            acc2 = acc1 - acc2;
            acc1 = (*xCurr--) - acc1;
            xBuf[i + 9] = acc2; /* odd */
            xBuf[i + 0] = acc1; /* even */
        }
    }
    /* xEven[0] and xOdd[0] scaled by 0.5 */
    xBuf[9] >>= 1;
    xBuf[0] >>= 1;

    /* do 9-point IDCT on even and odd */
    idct9(xBuf + 0); /* even */
    idct9(xBuf + 9); /* odd */

    xp = xBuf + 8;
    cp = c18 + 8;
    mOut = 0;
    if (btPrev == 0 && btCurr == 0) {
        /* fast path - use symmetry of sin window to reduce windowing multiplies to 18 (N/2) */
        wp = fastWin36;
        for (i = 0; i < 9; i++) {
            /* do ARM-style pointer arithmetic (i still needed for y[] indexing - compiler spills if 2 y pointers) */
            c = *cp--;
            xo = *(xp + 9);
            xe = *xp--;
            /* gain 2int32_t bits here */
            xo = MULSHIFT32(c, xo); /* 2*c18*xOdd (mul by 2 implicit in scaling)  */
            xe >>= 2;

            s = -(*xPrev);        /* sum from last block (always at least 2 guard bits) */
            d = -(xe - xo);       /* gain 2int32_t bits, don't shift xo (effective << 1 to eat sign bit, << 1 for mul by 2) */
            (*xPrev++) = xe + xo; /* symmetry - xPrev[i] = xPrev[17-i] for long blocks */
            t = s - d;

            yLo = (d + (MULSHIFT32(t, *wp++) << 2));
            yHi = (s + (MULSHIFT32(t, *wp++) << 2));
            y[(i)*NBANDS] = yLo;
            y[(17 - i) * NBANDS] = yHi;
            mOut |= FASTABS(yLo);
            mOut |= FASTABS(yHi);
        }
    } else {
        /* slower method - either prev or curr is using window type != 0 so do full 36-point window
         * output xPrevWin has at least 3 guard bits (xPrev has 2, gain 1 in WinPrevious)
         */
        WinPrevious(xPrev, xPrevWin, btPrev);

        wp = imdctWin[btCurr];
        for (i = 0; i < 9; i++) {
            c = *cp--;
            xo = *(xp + 9);
            xe = *xp--;
            /* gain 2int32_t bits here */
            xo = MULSHIFT32(c, xo); /* 2*c18*xOdd (mul by 2 implicit in scaling)  */
            xe >>= 2;

            d = xe - xo;
            (*xPrev++) = xe + xo; /* symmetry - xPrev[i] = xPrev[17-i] for long blocks */

            yLo = (xPrevWin[i] + MULSHIFT32(d, wp[i])) << 2;
            yHi = (xPrevWin[17 - i] + MULSHIFT32(d, wp[17 - i])) << 2;
            y[(i)*NBANDS] = yLo;
            y[(17 - i) * NBANDS] = yHi;
            mOut |= FASTABS(yLo);
            mOut |= FASTABS(yHi);
        }
    }

    xPrev -= 9;
    mOut |= FreqInvertRescale(y, xPrev, blockIdx, es);

    return mOut;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* 12-point inverse DCT, used in IMDCT12x3()
 * 4 input guard bits will ensure no overflow
 */
void MP3Decoder::imdct12(int32_t* x, int32_t* out) {
    int32_t a0, a1, a2;
    int32_t x0, x1, x2, x3, x4, x5;

    x0 = *x;
    x += 3;
    x1 = *x;
    x += 3;
    x2 = *x;
    x += 3;
    x3 = *x;
    x += 3;
    x4 = *x;
    x += 3;
    x5 = *x;
    x += 3;

    x4 -= x5;
    x3 -= x4;
    x2 -= x3;
    x3 -= x5;
    x1 -= x2;
    x0 -= x1;
    x1 -= x3;

    x0 >>= 1;
    x1 >>= 1;

    a0 = MULSHIFT32(c3_0, x2) << 1;
    a1 = x0 + (x4 >> 1);
    a2 = x0 - x4;
    x0 = a1 + a0;
    x2 = a2;
    x4 = a1 - a0;

    a0 = MULSHIFT32(c3_0, x3) << 1;
    a1 = x1 + (x5 >> 1);
    a2 = x1 - x5;

    /* cos window odd samples, mul by 2, eat sign bit */
    x1 = MULSHIFT32(c6[0], a1 + a0) << 2;
    x3 = MULSHIFT32(c6[1], a2) << 2;
    x5 = MULSHIFT32(c6[2], a1 - a0) << 2;

    *out = x0 + x1;
    out++;
    *out = x2 + x3;
    out++;
    *out = x4 + x5;
    out++;
    *out = x4 - x5;
    out++;
    *out = x2 - x3;
    out++;
    *out = x0 - x1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    IMDCT12x3
 *
 * Description: three 12-point modified DCT's for short blocks, with windowing,
 *                short block concatenation, and overlap-add
 *
 * Inputs:      3 interleaved vectors of 6 samples each
 *                (block0[0], block1[0], block2[0], block0[1], block1[1]....)
 *              overlap part of last IMDCT (9 samples - see output comments)
 *              window type (0,1,2,3) of previous block
 *              current block index (for deciding whether to do frequency inversion)
 *              number of guard bits in input vector
 *
 * Outputs:     updated sample vector x, net gain of 1 integer bit
 *              second half of (unwindowed) IMDCT's - save for next time
 *                only save 9 xPrev samples, using symmetry (see WinPrevious())
 *
 * Return:      mOut (OR of abs(y) for all y calculated here)
 */
// barely faster in RAM
int32_t MP3Decoder::IMDCT12x3(int32_t* xCurr, int32_t* xPrev, int32_t* y, int32_t btPrev, int32_t blockIdx, int32_t gb) {
    int32_t         i, es, mOut, yLo, xBuf[18], xPrevWin[18]; /* need temp buffer for reordering short blocks */
    const uint32_t* wp;
    es = 0;
    /* 7 gb is always adequate for accumulator loop + idct12 + window + overlap */
    if (gb < 7) {
        es = 7 - gb;
        for (i = 0; i < 18; i += 2) {
            xCurr[i + 0] >>= es;
            xCurr[i + 1] >>= es;
            *xPrev++ >>= es;
        }
        xPrev -= 9;
    }

    /* requires 4 input guard bits for each imdct12 */
    imdct12(xCurr + 0, xBuf + 0);
    imdct12(xCurr + 1, xBuf + 6);
    imdct12(xCurr + 2, xBuf + 12);

    /* window previous from last time */
    WinPrevious(xPrev, xPrevWin, btPrev);

    /* could unroll this for speed, minimum loads (short blocks usually rare, so doesn't make much overall difference)
     * xPrevWin[i] << 2 still has 1 gb always, max gain of windowed xBuf stuff also < 1.0 and gain the sign bit
     * so y calculations won't overflow
     */
    wp = imdctWin[2];
    mOut = 0;
    for (i = 0; i < 3; i++) {
        yLo = (xPrevWin[0 + i] << 2);
        mOut |= FASTABS(yLo);
        y[(0 + i) * NBANDS] = yLo;
        yLo = (xPrevWin[3 + i] << 2);
        mOut |= FASTABS(yLo);
        y[(3 + i) * NBANDS] = yLo;
        yLo = (xPrevWin[6 + i] << 2) + (MULSHIFT32(wp[0 + i], xBuf[3 + i]));
        mOut |= FASTABS(yLo);
        y[(6 + i) * NBANDS] = yLo;
        yLo = (xPrevWin[9 + i] << 2) + (MULSHIFT32(wp[3 + i], xBuf[5 - i]));
        mOut |= FASTABS(yLo);
        y[(9 + i) * NBANDS] = yLo;
        yLo = (xPrevWin[12 + i] << 2) + (MULSHIFT32(wp[6 + i], xBuf[2 - i]) + MULSHIFT32(wp[0 + i], xBuf[(6 + 3) + i]));
        mOut |= FASTABS(yLo);
        y[(12 + i) * NBANDS] = yLo;
        yLo = (xPrevWin[15 + i] << 2) + (MULSHIFT32(wp[9 + i], xBuf[0 + i]) + MULSHIFT32(wp[3 + i], xBuf[(6 + 5) - i]));
        mOut |= FASTABS(yLo);
        y[(15 + i) * NBANDS] = yLo;
    }

    /* save previous (unwindowed) for overlap - only need samples 6-8, 12-17 */
    for (i = 6; i < 9; i++) *xPrev++ = xBuf[i] >> 2;
    for (i = 12; i < 18; i++) *xPrev++ = xBuf[i] >> 2;

    xPrev -= 9;
    mOut |= FreqInvertRescale(y, xPrev, blockIdx, es);

    return mOut;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    HybridTransform
 *
 * Description: IMDCT's, windowing, and overlap-add on long/short/mixed blocks
 *
 * Inputs:      vector of input coefficients, length = nBlocksTotal * 18)
 *              vector of overlap samples from last time, length = nBlocksPrev * 9)
 *              buffer for output samples, length = MAXNSAMP
 *              SideInfoSub struct for this granule/channel
 *              BlockCount struct with necessary info
 *                number of non-zero input and overlap blocks
 *                number of long blocks in input vector (rest assumed to be short blocks)
 *                number of blocks which use long window (type) 0 in case of mixed block
 *                  (bc->currWinSwitch, 0 for non-mixed blocks)
 *
 * Outputs:     transformed, windowed, and overlapped sample buffer
 *              does frequency inversion on odd blocks
 *              updated buffer of samples for overlap
 *
 * Return:      number of non-zero IMDCT blocks calculated in this call
 *                (including overlap-add)
 */
int32_t MP3Decoder::HybridTransform(int32_t* xCurr, int32_t* xPrev, int32_t y[BLOCK_SIZE][NBANDS], SideInfoSub_t* sis, BlockCount_t* bc) {
    int32_t xPrevWin[18], currWinIdx, prevWinIdx;
    int32_t i, j, nBlocksOut, nonZero, mOut;
    int32_t fiBit, xp;

    assert(bc->nBlocksLong <= NBANDS);
    assert(bc->nBlocksTotal <= NBANDS);
    assert(bc->nBlocksPrev <= NBANDS);

    mOut = 0;

    /* do long blocks, if any */
    for (i = 0; i < bc->nBlocksLong; i++) {
        /* currWinIdx picks the right window for long blocks (if mixed, long blocks use window type 0) */
        currWinIdx = sis->blockType;
        if (sis->mixedBlock && i < bc->currWinSwitch) currWinIdx = 0;

        prevWinIdx = bc->prevType;
        if (i < bc->prevWinSwitch) prevWinIdx = 0;

        /* do 36-point IMDCT, including windowing and overlap-add */
        mOut |= IMDCT36(xCurr, xPrev, &(y[0][i]), currWinIdx, prevWinIdx, i, bc->gbIn);
        xCurr += 18;
        xPrev += 9;
    }

    /* do short blocks (if any) */
    for (; i < bc->nBlocksTotal; i++) {
        assert(sis->blockType == 2);

        prevWinIdx = bc->prevType;
        if (i < bc->prevWinSwitch) prevWinIdx = 0;

        mOut |= IMDCT12x3(xCurr, xPrev, &(y[0][i]), prevWinIdx, i, bc->gbIn);
        xCurr += 18;
        xPrev += 9;
    }
    nBlocksOut = i;

    /* window and overlap prev if prev longer that current */
    for (; i < bc->nBlocksPrev; i++) {
        prevWinIdx = bc->prevType;
        if (i < bc->prevWinSwitch) prevWinIdx = 0;
        WinPrevious(xPrev, xPrevWin, prevWinIdx);

        nonZero = 0;
        fiBit = i << 31;
        for (j = 0; j < 9; j++) {
            xp = xPrevWin[2 * j + 0] << 2; /* << 2 temp for scaling */
            nonZero |= xp;
            y[2 * j + 0][i] = xp;
            mOut |= FASTABS(xp);

            /* frequency inversion on odd blocks/odd samples (flip sign if i odd, j odd) */
            xp = xPrevWin[2 * j + 1] << 2;
            xp = (xp ^ (fiBit >> 31)) + (i & 0x01);
            nonZero |= xp;
            y[2 * j + 1][i] = xp;
            mOut |= FASTABS(xp);

            xPrev[j] = 0;
        }
        xPrev += 9;
        if (nonZero) nBlocksOut = i;
    }

    /* clear rest of blocks */
    for (; i < 32; i++) {
        for (j = 0; j < 18; j++) y[j][i] = 0;
    }

    bc->gbOut = CLZ(mOut) - 1;

    return nBlocksOut;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    IMDCT
 *
 * Description: do alias reduction, inverse MDCT, overlap-add, and frequency inversion
 *
 * Inputs:      MP3DecInfo structure filled by UnpackFrameHeader(), UnpackSideInfo(),
 *                UnpackScaleFactors(), and DecodeHuffman() (for this granule, channel)
 *                includes PCM samples in overBuf (from last call to IMDCT) for OLA
 *              index of current granule and channel
 *
 * Outputs:     PCM samples in outBuf, for input to subband transform
 *              PCM samples in overBuf, for OLA next time
 *              updated hi->nonZeroBound index for this channel
 *
 * Return:      0 on success,  -1 if null input pointers
 */
// a bit faster in RAM
/*__attribute__ ((section (".data")))*/
int32_t MP3Decoder::IMDCT(int32_t gr, int32_t ch) {
    int32_t      nBfly, blockCutoff;
    BlockCount_t bc;

    /* m_SideInfo is an array of up to 4 structs, stored as gr0ch0, gr0ch1, gr1ch0, gr1ch1 */
    /* anti-aliasing done on whole long blocks only
     * for mixed blocks, nBfly always 1, except 3 for 8 kHz MPEG 2.5 (see sfBandTab)
     *   nLongBlocks = number of blocks with (possibly) non-zero power
     *   nBfly = number of butterflies to do (nLongBlocks - 1, unless no long blocks)
     */
    blockCutoff = m_SFBandTable.l[(m_MPEGVersion == MPEG1 ? 8 : 6)] / 18; /* same as 3* num short sfb's in spec */
    if (m_SideInfoSub[gr][ch].blockType != 2) {
        /* all long transforms */
        int32_t x = (m_HuffmanInfo->nonZeroBound[ch] + 7) / 18 + 1;
        bc.nBlocksLong = (x < 32 ? x : 32);
        // bc.nBlocksLong = min((hi->nonZeroBound[ch] + 7) / 18 + 1, 32);
        nBfly = bc.nBlocksLong - 1;
    } else if (m_SideInfoSub[gr][ch].blockType == 2 && m_SideInfoSub[gr][ch].mixedBlock) {
        /* mixed block - long transforms until cutoff, then short transforms */
        bc.nBlocksLong = blockCutoff;
        nBfly = bc.nBlocksLong - 1;
    } else {
        /* all short transforms */
        bc.nBlocksLong = 0;
        nBfly = 0;
    }

    AntiAlias(m_HuffmanInfo->huffDecBuf[ch], nBfly);
    int32_t x = m_HuffmanInfo->nonZeroBound[ch];
    int32_t y = nBfly * 18 + 8;
    m_HuffmanInfo->nonZeroBound[ch] = (x > y ? x : y);

    assert(m_HuffmanInfo->nonZeroBound[ch] <= MAX_NSAMP);

    /* for readability, use a struct instead of passing a million parameters to HybridTransform() */
    bc.nBlocksTotal = (m_HuffmanInfo->nonZeroBound[ch] + 17) / 18;
    bc.nBlocksPrev = m_IMDCTInfo->numPrevIMDCT[ch];
    bc.prevType = m_IMDCTInfo->prevType[ch];
    bc.prevWinSwitch = m_IMDCTInfo->prevWinSwitch[ch];
    /* where WINDOW switches (not nec. transform) */
    bc.currWinSwitch = (m_SideInfoSub[gr][ch].mixedBlock ? blockCutoff : 0);
    bc.gbIn = m_HuffmanInfo->gb[ch];

    m_IMDCTInfo->numPrevIMDCT[ch] = HybridTransform(m_HuffmanInfo->huffDecBuf[ch], m_IMDCTInfo->overBuf[ch], m_IMDCTInfo->outBuf[ch], &m_SideInfoSub[gr][ch], &bc);
    m_IMDCTInfo->prevType[ch] = m_SideInfoSub[gr][ch].blockType;
    m_IMDCTInfo->prevWinSwitch[ch] = bc.currWinSwitch; /* 0 means not a mixed block (either all short or all long) */
    m_IMDCTInfo->gb[ch] = bc.gbOut;

    assert(m_IMDCTInfo->numPrevIMDCT[ch] <= NBANDS);

    /* output has gained 2int32_t bits */
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * S U B B A N D
 */

/*
 * Function:    Subband
 *
 * Description: do subband transform on all the blocks in one granule, all channels
 *
 * Inputs:      filled MP3DecInfo structure, after calling IMDCT for all channels
 *              vbuf[ch] and vindex[ch] must be preserved between calls
 *
 * Outputs:     decoded PCM data, interleaved LRLRLR... if stereo
 *
 * Return:      0 on success,  -1 if null input pointers
 */
int32_t MP3Decoder::Subband(int16_t* pcmBuf) {
    int32_t b;
    if (m_MP3DecInfo->nChans == 2) {
        /* stereo */
        for (b = 0; b < BLOCK_SIZE; b++) {
            FDCT32(m_IMDCTInfo->outBuf[0][b], m_SubbandInfo->vbuf + 0 * 32, m_SubbandInfo->vindex, (b & 0x01), m_IMDCTInfo->gb[0]);
            FDCT32(m_IMDCTInfo->outBuf[1][b], m_SubbandInfo->vbuf + 1 * 32, m_SubbandInfo->vindex, (b & 0x01), m_IMDCTInfo->gb[1]);
            PolyphaseStereo(pcmBuf, m_SubbandInfo->vbuf + m_SubbandInfo->vindex + VBUF_LENGTH * (b & 0x01), polyCoef);
            m_SubbandInfo->vindex = (m_SubbandInfo->vindex - (b & 0x01)) & 7;
            pcmBuf += (2 * NBANDS);
        }
    } else {
        /* mono */
        for (b = 0; b < BLOCK_SIZE; b++) {
            FDCT32(m_IMDCTInfo->outBuf[0][b], m_SubbandInfo->vbuf + 0 * 32, m_SubbandInfo->vindex, (b & 0x01), m_IMDCTInfo->gb[0]);
            PolyphaseMono(pcmBuf, m_SubbandInfo->vbuf + m_SubbandInfo->vindex + VBUF_LENGTH * (b & 0x01), polyCoef);
            m_SubbandInfo->vindex = (m_SubbandInfo->vindex - (b & 0x01)) & 7;
            pcmBuf += NBANDS;
        }
    }

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void MP3Decoder::FDCT32(int32_t* buf, int32_t* dest, int32_t offset, int32_t oddBlock, int32_t gb) {
    int32_t        i, s, tmp, es;
    const int32_t* cptr = (const int32_t*)m_dcttab;
    int32_t        a0, a1, a2, a3, a4, a5, a6, a7;
    int32_t        b0, b1, b2, b3, b4, b5, b6, b7;
    int32_t*       d;

    /* scaling - ensure at least 6 guard bits for DCT
     * (in practice this is already true 99% of time, so this code is
     *  almost never triggered)
     */
    es = 0;
    if (gb < 6) {
        es = 6 - gb;
        for (i = 0; i < 32; i++) buf[i] >>= es;
    }

    /* first pass */
    for (unsigned i = 0; i < 8; i++) { D32FP(i, FDCT32s1s2[0 + i], FDCT32s1s2[8 + i]); }

    /* second pass */
    for (i = 4; i > 0; i--) {
        a0 = buf[0];
        a7 = buf[7];
        a3 = buf[3];
        a4 = buf[4];
        b0 = a0 + a7;
        b7 = MULSHIFT32(*cptr++, a0 - a7) << 1;
        b3 = a3 + a4;
        b4 = MULSHIFT32(*cptr++, a3 - a4) << 3;
        a0 = b0 + b3;
        a3 = MULSHIFT32(*cptr, b0 - b3) << 1;
        a4 = b4 + b7;
        a7 = MULSHIFT32(*cptr++, b7 - b4) << 1;

        a1 = buf[1];
        a6 = buf[6];
        a2 = buf[2];
        a5 = buf[5];
        b1 = a1 + a6;
        b6 = MULSHIFT32(*cptr++, a1 - a6) << 1;
        b2 = a2 + a5;
        b5 = MULSHIFT32(*cptr++, a2 - a5) << 1;
        a1 = b1 + b2;
        a2 = MULSHIFT32(*cptr, b1 - b2) << 2;
        a5 = b5 + b6;
        a6 = MULSHIFT32(*cptr++, b6 - b5) << 2;

        b0 = a0 + a1;
        b1 = MULSHIFT32(m_COS4_0, a0 - a1) << 1;
        b2 = a2 + a3;
        b3 = MULSHIFT32(m_COS4_0, a3 - a2) << 1;
        buf[0] = b0;
        buf[1] = b1;
        buf[2] = b2 + b3;
        buf[3] = b3;

        b4 = a4 + a5;
        b5 = MULSHIFT32(m_COS4_0, a4 - a5) << 1;
        b6 = a6 + a7;
        b7 = MULSHIFT32(m_COS4_0, a7 - a6) << 1;
        b6 += b7;
        buf[4] = b4 + b6;
        buf[5] = b5 + b7;
        buf[6] = b5 + b6;
        buf[7] = b7;

        buf += 8;
    }
    buf -= 32; /* reset */

    /* sample 0 - always delayed one block */
    d = dest + 64 * 16 + ((offset - oddBlock) & 7) + (oddBlock ? 0 : VBUF_LENGTH);
    s = buf[0];
    d[0] = d[8] = s;

    /* samples 16 to 31 */
    d = dest + offset + (oddBlock ? VBUF_LENGTH : 0);

    s = buf[1];
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[25] + buf[29];
    s = buf[17] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[9] + buf[13];
    d[0] = d[8] = s;
    d += 64;
    s = buf[21] + tmp;
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[29] + buf[27];
    s = buf[5];
    d[0] = d[8] = s;
    d += 64;
    s = buf[21] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[13] + buf[11];
    d[0] = d[8] = s;
    d += 64;
    s = buf[19] + tmp;
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[27] + buf[31];
    s = buf[3];
    d[0] = d[8] = s;
    d += 64;
    s = buf[19] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[11] + buf[15];
    d[0] = d[8] = s;
    d += 64;
    s = buf[23] + tmp;
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[31];
    s = buf[7];
    d[0] = d[8] = s;
    d += 64;
    s = buf[23] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[15];
    d[0] = d[8] = s;
    d += 64;
    s = tmp;
    d[0] = d[8] = s;

    /* samples 16 to 1 (sample 16 used again) */
    d = dest + 16 + ((offset - oddBlock) & 7) + (oddBlock ? 0 : VBUF_LENGTH);

    s = buf[1];
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[30] + buf[25];
    s = buf[17] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[14] + buf[9];
    d[0] = d[8] = s;
    d += 64;
    s = buf[22] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[6];
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[26] + buf[30];
    s = buf[22] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[10] + buf[14];
    d[0] = d[8] = s;
    d += 64;
    s = buf[18] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[2];
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[28] + buf[26];
    s = buf[18] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[12] + buf[10];
    d[0] = d[8] = s;
    d += 64;
    s = buf[20] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[4];
    d[0] = d[8] = s;
    d += 64;

    tmp = buf[24] + buf[28];
    s = buf[20] + tmp;
    d[0] = d[8] = s;
    d += 64;
    s = buf[8] + buf[12];
    d[0] = d[8] = s;
    d += 64;
    s = buf[16] + tmp;
    d[0] = d[8] = s;

    /* this is so rarely invoked that it's not worth making two versions of the output
     *   shuffle code (one for no shift, one for clip + variable shift) like in IMDCT
     * here we just load, clip, shift, and store on the rare instances that es != 0
     */
    if (es) {
        d = dest + 64 * 16 + ((offset - oddBlock) & 7) + (oddBlock ? 0 : VBUF_LENGTH);
        s = d[0];
        CLIP_2N(s, (31 - es));
        d[0] = d[8] = (s << es);

        d = dest + offset + (oddBlock ? VBUF_LENGTH : 0);
        for (i = 16; i <= 31; i++) {
            s = d[0];
            CLIP_2N(s, (31 - es));
            d[0] = d[8] = (s << es);
            d += 64;
        }

        d = dest + 16 + ((offset - oddBlock) & 7) + (oddBlock ? 0 : VBUF_LENGTH);
        for (i = 15; i >= 0; i--) {
            s = d[0];
            CLIP_2N(s, (31 - es));
            d[0] = d[8] = (s << es);
            d += 64;
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * P O L Y P H A S E
 */
int16_t MP3Decoder::ClipToShort(int32_t x, int32_t fracBits) {
#if (defined CONFIG_IDF_TARGET_ESP32 || defined CONFIG_IDF_TARGET_ESP32S3)
    /* assumes you've already rounded (x += (1 << (fracBits-1))) */
    x >>= fracBits;
    // this is better on xtensa (fb)
    asm("clamps %0, %1, 15" : "=a"(x) : "a"(x) :);
    return x;
#endif

#if (defined CONFIG_IDF_TARGET_ESP32P4)
    int32_t sign;

    /* assumes you've already rounded (x += (1 << (fracBits-1))) */
    x >>= fracBits;

    /* Ken's trick: clips to [-32768, 32767] */
    sign = x >> 31;
    if (sign != (x >> 15)) { x = sign ^ ((1 << 15) - 1); }

    return (int16_t)x;
#endif
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    PolyphaseMono
 *
 * Description: filter one subband and produce 32 output PCM samples for one channel
 *
 * Inputs:      pointer to PCM output buffer
 *              number of "extra shifts" (vbuf format = Q(DQ_FRACBITS_OUT-2))
 *              pointer to start of vbuf (preserved from last call)
 *              start of filter coefficient table (in proper, shuffled order)
 *              no minimum number of guard bits is required for input vbuf
 *                (see additional scaling comments below)
 *
 * Outputs:     32 samples of one channel of decoded PCM data, (i.e. Q16.0)
 *
 * Return:      none
 */
void MP3Decoder::PolyphaseMono(int16_t* pcm, int32_t* vbuf, const uint32_t* coefBase) {
    int32_t         i;
    const uint32_t* coef;
    int32_t*        vb1;
    int32_t         vLo, vHi, c1, c2;
    uint64_t        sum1L, sum2L, rndVal;

    rndVal = (uint64_t)(1ULL << ((DQ_FRACBITS_OUT - 2 - 2 - 15) - 1 + (32 - CSHIFT)));

    /* special case, output sample 0 */
    coef = coefBase;
    vb1 = vbuf;
    sum1L = rndVal;
    for (int32_t j = 0; j < 8; j++) {
        c1 = *coef;
        coef++;
        c2 = *coef;
        coef++;
        vLo = *(vb1 + (j));
        vHi = *(vb1 + (23 - (j))); // 0...7
        sum1L = MADD64(sum1L, vLo, c1);
        sum1L = MADD64(sum1L, vHi, -c2);
    }
    *(pcm + 0) = ClipToShort((int32_t)SAR64(sum1L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);

    /* special case, output sample 16 */
    coef = coefBase + 256;
    vb1 = vbuf + 64 * 16;
    sum1L = rndVal;
    for (int32_t j = 0; j < 8; j++) {
        c1 = *coef;
        coef++;
        vLo = *(vb1 + (j));
        sum1L = MADD64(sum1L, vLo, c1); // 0...7
    }
    *(pcm + 16) = ClipToShort((int32_t)SAR64(sum1L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);

    /* main convolution loop: sum1L = samples 1, 2, 3, ... 15   sum2L = samples 31, 30, ... 17 */
    coef = coefBase + 16;
    vb1 = vbuf + 64;
    pcm++;

    /* right now, the compiler creates bad asm from this... */
    for (i = 15; i > 0; i--) {
        sum1L = sum2L = rndVal;
        for (int32_t j = 0; j < 8; j++) {
            c1 = *coef;
            coef++;
            c2 = *coef;
            coef++;
            vLo = *(vb1 + (j));
            vHi = *(vb1 + (23 - (j)));
            sum1L = MADD64(sum1L, vLo, c1);
            sum2L = MADD64(sum2L, vLo, c2);
            sum1L = MADD64(sum1L, vHi, -c2);
            sum2L = MADD64(sum2L, vHi, c1);
        }
        vb1 += 64;
        *(pcm) = ClipToShort((int32_t)SAR64(sum1L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
        *(pcm + 2 * i) = ClipToShort((int32_t)SAR64(sum2L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
        pcm++;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    PolyphaseStereo
 *
 * Description: filter one subband and produce 32 output PCM samples for each channel
 *
 * Inputs:      pointer to PCM output buffer
 *              number of "extra shifts" (vbuf format = Q(DQ_FRACBITS_OUT-2))
 *              pointer to start of vbuf (preserved from last call)
 *              start of filter coefficient table (in proper, shuffled order)
 *              no minimum number of guard bits is required for input vbuf
 *                (see additional scaling comments below)
 *
 * Outputs:     32 samples of two channels of decoded PCM data, (i.e. Q16.0)
 *
 * Return:      none
 *
 * Notes:       interleaves PCM samples LRLRLR...
 */
void MP3Decoder::PolyphaseStereo(int16_t* pcm, int32_t* vbuf, const uint32_t* coefBase) {
    int32_t         i;
    const uint32_t* coef;
    int32_t*        vb1;
    int32_t         vLo, vHi, c1, c2;
    uint64_t        sum1L, sum2L, sum1R, sum2R, rndVal;

    rndVal = (uint64_t)(1 << ((DQ_FRACBITS_OUT - 2 - 2 - 15) - 1 + (32 - CSHIFT)));

    /* special case, output sample 0 */
    coef = coefBase;
    vb1 = vbuf;
    sum1L = sum1R = rndVal;

    for (int32_t j = 0; j < 8; j++) {
        c1 = *coef;
        coef++;
        c2 = *coef;
        coef++;
        vLo = *(vb1 + (j));
        vHi = *(vb1 + (23 - (j)));
        sum1L = MADD64(sum1L, vLo, c1);
        sum1L = MADD64(sum1L, vHi, -c2);
        vLo = *(vb1 + 32 + (j));
        vHi = *(vb1 + 32 + (23 - (j)));
        sum1R = MADD64(sum1R, vLo, c1);
        sum1R = MADD64(sum1R, vHi, -c2);
    }
    *(pcm + 0) = ClipToShort((int32_t)SAR64(sum1L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
    *(pcm + 1) = ClipToShort((int32_t)SAR64(sum1R, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);

    /* special case, output sample 16 */
    coef = coefBase + 256;
    vb1 = vbuf + 64 * 16;
    sum1L = sum1R = rndVal;

    for (int32_t j = 0; j < 8; j++) {
        c1 = *coef;
        coef++;
        vLo = *(vb1 + (j));
        sum1L = MADD64(sum1L, vLo, c1);
        vLo = *(vb1 + 32 + (j));
        sum1R = MADD64(sum1R, vLo, c1);
    }
    *(pcm + 2 * 16 + 0) = ClipToShort((int32_t)SAR64(sum1L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
    *(pcm + 2 * 16 + 1) = ClipToShort((int32_t)SAR64(sum1R, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);

    /* main convolution loop: sum1L = samples 1, 2, 3, ... 15   sum2L = samples 31, 30, ... 17 */
    coef = coefBase + 16;
    vb1 = vbuf + 64;
    pcm += 2;

    /* right now, the compiler creates bad asm from this... */
    for (i = 15; i > 0; i--) {
        sum1L = sum2L = rndVal;
        sum1R = sum2R = rndVal;

        for (int32_t j = 0; j < 8; j++) {
            c1 = *coef;
            coef++;
            c2 = *coef;
            coef++;
            vLo = *(vb1 + (j));
            vHi = *(vb1 + (23 - (j)));
            sum1L = MADD64(sum1L, vLo, c1);
            sum2L = MADD64(sum2L, vLo, c2);
            sum1L = MADD64(sum1L, vHi, -c2);
            sum2L = MADD64(sum2L, vHi, c1);
            vLo = *(vb1 + 32 + (j));
            vHi = *(vb1 + 32 + (23 - (j)));
            sum1R = MADD64(sum1R, vLo, c1);
            sum2R = MADD64(sum2R, vLo, c2);
            sum1R = MADD64(sum1R, vHi, -c2);
            sum2R = MADD64(sum2R, vHi, c1);
        }
        vb1 += 64;
        *(pcm + 0) = ClipToShort((int32_t)SAR64(sum1L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
        *(pcm + 1) = ClipToShort((int32_t)SAR64(sum1R, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
        *(pcm + 2 * 2 * i + 0) = ClipToShort((int32_t)SAR64(sum2L, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
        *(pcm + 2 * 2 * i + 1) = ClipToShort((int32_t)SAR64(sum2R, (32 - CSHIFT)), DQ_FRACBITS_OUT - 2 - 2 - 15);
        pcm += 2;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*
 * Function:    AnalyzeFrame
 *
 * Description: filter one subband and produce 32 output PCM samples for each channel
 *
 * Inputs:      pointer to inpit buffer and length
 *
 * Outputs:     MPEG_VERSION
 *              LAYER
 *              CHANNEL_MODE
 *
 * Return:      main_data_begin
 *
 */
int MP3Decoder::MP3_AnalyzeFrame(const uint8_t* frame_data, size_t frame_len) {
    if (frame_len < 4) {
        MP3_LOG_ERROR("Error: Frame data too short for header (need 4 bytes, got %zu).\n", frame_len);
        return -3; // Frame too short for header
    }

    // Define constants for better readability
    const uint8_t MPEG_VERSION_2_5 = 0;      // 00 - unofficial, but often so coded
    const uint8_t MPEG_VERSION_RESERVED = 1; // 01
    const uint8_t MPEG_VERSION_2 = 2;        // 10
    const uint8_t MPEG_VERSION_1 = 3;        // 11

    const uint8_t LAYER_RESERVED = 0; // 00
    const uint8_t LAYER_III = 1;      // 01
    const uint8_t LAYER_II = 2;       // 10
    const uint8_t LAYER_I = 3;        // 11

    const uint8_t CHANNEL_MODE_STEREO = 0;       // 00
    const uint8_t CHANNEL_MODE_JOINT_STEREO = 1; // 01
    const uint8_t CHANNEL_MODE_DUAL_CHANNEL = 2; // 10
    const uint8_t CHANNEL_MODE_MONO = 3;         // 11

    (void)MPEG_VERSION_RESERVED;
    (void)LAYER_III;
    (void)LAYER_II;
    (void)LAYER_I;
    (void)CHANNEL_MODE_STEREO;
    (void)CHANNEL_MODE_JOINT_STEREO;
    (void)CHANNEL_MODE_DUAL_CHANNEL;
    (void)LAYER_RESERVED;

    // ---- 1. Analyze frame header (first 4 bytes) ---
    // combine the first 4 bytes into a 32-bit integer (Big Endian)
    uint32_t header = ((uint32_t)frame_data[0] << 24) | ((uint32_t)frame_data[1] << 16) | ((uint32_t)frame_data[2] << 8) | ((uint32_t)frame_data[3]);

    // check sync word (first 11 bits must be 1)
    // MPEG 2.5 Layer III often uses 12 bits (0xfff), other 11 bits (0xffe)
    // simple check: data [0] == 0xff and (data [1] & 0xe0) == 0xe0
    if (!(frame_data[0] == 0xFF && (frame_data[1] & 0xE0) == 0xE0)) {
        MP3_LOG_ERROR("Error: Invalid MP3 sync word.\n");
        return -4;
    }

    // MPEG version ID (Bits 11-12 of the header, or Bits 19-20 from right in the Uint32_t)
    // Header: SSSS SSSS SSSV Vllp PBBB BFFM MCCE (S = Sync, V = version, L = layer, p = Protection ...)
    // In our `Header` Uint32_t:
    // Bit 31..21: Sync word (11 bits)
    // Bit 20..19: MPEG Audio version ID
    // Bit 18..17: Layer description
    // Bit 16:     Protection bit
    // Bit 15..12: Bitrate index
    // Bit 11..10: Sampling rate frequency index
    // Bit 9:      Padding bit
    // Bit 8:      Private bit
    // Bit 7..6:   Channel mode
    // Bit 5..4:   Mode extension (for Joint Stereo)
    // Bit 3:      Copyright
    // Bit 2:      Original
    // Bit 1..0:   Emphasis

    uint8_t mpeg_version_id = (header >> 19) & 0x03;
    uint8_t layer_description = (header >> 17) & 0x03;
    uint8_t protection_bit = (header >> 16) & 0x01;
    uint8_t channel_mode = (header >> 6) & 0x03;

    // Debug output(optional)
    // MP3_LOG_DEBUG("MPEG Version ID raw: %u\n", mpeg_version_id);
    // MP3_LOG_DEBUG("Layer Description raw: %u\n", layer_description);
    // MP3_LOG_DEBUG("Protection Bit: %u\n", protection_bit);
    // MP3_LOG_DEBUG("Channel Mode raw: %u\n", channel_mode);

    // --- 2. Check whether it is Layer III ---
    if (layer_description != LAYER_III) {
        // fprintf(stderr, "Info: Not an MPEG Layer III frame (Layer: %u).\n", layer_description);
        return -1; // no Layer III
    }

    // --- 3. Side Information, Determine offset and size ---
    int side_info_offset = 4;  // after the 4-Byte Header
    if (protection_bit == 0) { // 0 means CRC is available
        side_info_offset += 2; // Skip 16-bit CRC
    }

    int side_info_size;
    (void)side_info_size;
    // Derive MPEG versions from the ID (according to ISO/IEC 13818-3 Table B.1)
    // ID '00' -> MPEG 2.5
    // ID '01' -> reserved
    // ID '10' -> MPEG 2
    // ID '11' -> MPEG 1
    if (mpeg_version_id == MPEG_VERSION_1) { // MPEG-1
        if (channel_mode == CHANNEL_MODE_MONO) {
            side_info_size = 17; // Mono
        } else {
            side_info_size = 32; // Stereo, Joint Stereo, Dual Channel
        }
    } else if (mpeg_version_id == MPEG_VERSION_2 || mpeg_version_id == MPEG_VERSION_2_5) { // MPEG-2 oder MPEG-2.5
        if (channel_mode == CHANNEL_MODE_MONO) {
            side_info_size = 9; // Mono
        } else {
            side_info_size = 17; // Stereo, Joint Stereo, Dual Channel
        }
    } else {
        fprintf(stderr, "Error: Reserved or unknown MPEG version ID: %u.\n", mpeg_version_id);
        return -2; // Unknown/reserved MPEG version
    }

    // ensure that the frame is long enough for the side information
    // We need at least 2 bytes of the side info for Main_data_begin
    if (frame_len < (size_t)(side_info_offset + 2)) {
        fprintf(stderr, "Error: Frame data too short for side information (need %d bytes, got %zu).\n", side_info_offset + 2, frame_len);
        return -3;
    }
    // (optional) Check whether the entire Side Information is available
    /*
    if (frame_len < (size_t)(side_info_offset + side_info_size)) {
        fprintf(stderr, "Warning: Frame data might be too short for full side information (expected %d, got %zu available after header/CRC).\n", side_info_size, frame_len - side_info_offset);
        // Fortfahren, da main_data_begin am Anfang ist, aber es ist ein Hinweis
    }
    */

    // --- 4. Main_data_begin extract from the Side Information ---
    // Main_data_begin are the first 9 bits of the Side Information
    // Side information begins with frame_data [side_info_offset]
    const uint8_t* side_info_ptr = frame_data + side_info_offset;

    // The 9 bits consist of:
    // - the complete 8 bits of the first bytes of the Side Information
    // - the MSB (highest quality bit) of the second bytes of the Side Information
    uint16_t main_data_begin_val = ((uint16_t)side_info_ptr[0] << 1) | (side_info_ptr[1] >> 7);

    return main_data_begin_val;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint64_t MP3Decoder::SAR64(uint64_t x, int32_t n) {
    return x >> n;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::MULSHIFT32(int32_t x, int32_t y) {
    int32_t z;
    z = (uint64_t)x * (uint64_t)y >> 32;
    return z;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint64_t MP3Decoder::MADD64(uint64_t sum64, int32_t x, int32_t y) {
    sum64 += (uint64_t)x * (uint64_t)y;
    return sum64;
} /* returns 64-bit value in [edx:eax] */
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint64_t MP3Decoder::xSAR64(uint64_t x, int32_t n) {
    return x >> n;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decoder::FASTABS(int32_t x) {
    return __builtin_abs(x);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
