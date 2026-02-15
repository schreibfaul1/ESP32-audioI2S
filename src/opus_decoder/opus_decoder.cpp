/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on: 14.02.2026
 */
//----------------------------------------------------------------------------------------------------------------------
//                                     O G G / O P U S     I M P L.
//----------------------------------------------------------------------------------------------------------------------
#include "opus_decoder.h"
#include "Arduino.h"

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool OpusDecoder::init() {

    if (!rangedec) {
        OPUS_LOG_ERROR("RangeDecoder is null");
        return false;
    }
    // }

    if (!silkdec) {
        OPUS_LOG_ERROR("Failed to allocate SilkDecoder");
        return false;
    }
    silkdec->init();

    if (!celtdec) {
        OPUS_LOG_ERROR("Failed to allocate CeltkDecoder");
        return false;
    }
    celtdec->init();

    if (!m_opusSegmentTable.alloc_array(256)) return false;
    ;
    celtdec->clear();

    m_out16.alloc_array(4608 * 2, "m_out16");
    if(!m_out16.valid()) return false;

    clear();
    // allocate CELT buffers after OPUS head (nr of channels is needed)
    m_opusError = celtdec->celt_decoder_init(2);
    if (m_opusError < 0) { return false; /*ERR_OPUS_CELT_NOT_INIT;*/ }
    m_opusError = celtdec->celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST, 0);
    if (m_opusError < 0) { return false; /*ERR_OPUS_CELT_NOT_INIT;*/ }
    m_opusError = celtdec->celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, 21);
    if (m_opusError < 0) { return false; /*ERR_OPUS_CELT_NOT_INIT;*/ }
    OPUSsetDefaults();

    int32_t ret = 0, silkDecSizeBytes = 0;
    (void)ret;
    (void)silkDecSizeBytes;
    silkdec->silk_InitDecoder();
    m_isValid = true;
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void OpusDecoder::reset() {
    rangedec.reset();
    silkdec.reset();
    celtdec.reset();
    m_opusSegmentTable.reset();
    m_frameCount = 0;
    m_opusSegmentLength = 0;
    m_opusValidSamples = 0;
    m_opusSegmentTableSize = 0;
    m_opusOggHeaderSize = 0;
    m_opusSegmentTableRdPtr = -1;
    m_opusCountCode = 0;
    m_isValid = false;
    m_out16.reset();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void OpusDecoder::clear() {
    m_opusSegmentTable.clear();
    m_frameCount = 0;
    m_opusSegmentLength = 0;
    m_opusValidSamples = 0;
    m_opusSegmentTableSize = 0;
    m_opusOggHeaderSize = 0;
    m_opusSegmentTableRdPtr = -1;
    m_opusCountCode = 0;
    m_opusCurrentFilePos = 0;
    m_comment.reset();
    m_out16.clear();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool OpusDecoder::isValid() {
    return m_isValid;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void OpusDecoder::OPUSsetDefaults() {
    m_ofp2.reset();
    m_ofp3.reset();
    m_odp3.reset();
    m_ofp3.firstCall = true;
    m_f_opusParseOgg = false;
    m_f_newSteamTitle = false; // streamTitle
    m_f_opusNewMetadataBlockPicture = false;
    m_f_opusStereoFlag = false;
    m_f_lastPage = false;
    m_opusChannels = 0;
    m_frameCount = 0;
    m_mode = 0;
    m_opusSamplerate = 0;
    m_internalSampleRate = 0;
    m_bandWidth = 0;
    m_opusSegmentLength = 0;
    m_opusValidSamples = 0;
    m_opusSegmentTableSize = 0;
    m_opusOggHeaderSize = 0;
    m_opusSegmentTableRdPtr = -1;
    m_opusCountCode = 0;
    m_opusBlockPicPos = 0;
    m_opusCurrentFilePos = 0;
    m_opusAudioDataStart = 0;
    m_opusBlockPicLen = 0;
    m_opusCommentBlockSize = 0;
    m_opusRemainBlockPicLen = 0;
    m_blockPicLenUntilFrameEnd = 0;
    m_opusBlockLen = 0;
    m_opusPageNr = 0;
    m_opusError = 0;
    m_endband = 0;
    m_prev_mode = MODE_NONE;
    m_opusBlockPicItem.clear();
    m_opusBlockPicItem.shrink_to_fit();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf) {

    int32_t ret = OPUS_NONE;
    int32_t segmLen = 0;
    int32_t bytesLeft_begin = *bytesLeft;
    int32_t bytes_consumed = 0;

    if (m_f_lastPage && m_opusSegmentTableSize == 0) {
        if (OPUS_specialIndexOf(inbuf, "OggS", 5) == 0) { // next round
            m_opusPageNr = 0;
        } else {
            return OPUS_END;
        }
    }

    if (m_frameCount > 0) { // decode audio, next part
        ret = opusDecodePage3(inbuf, bytesLeft, segmLen, m_out16.get());
        goto exit;
    }

    if (!m_opusSegmentTableSize) {
        m_f_opusParseOgg = false;
        m_opusCountCode = 0;
        ret = parseOGG(inbuf, bytesLeft);
        bytes_consumed = bytesLeft_begin - (*bytesLeft);
        if (ret != OPUS_NONE) goto exit; // error
        inbuf += m_opusOggHeaderSize;    // no return, fall through
    }

    if (m_opusSegmentTableSize > 0) {
        m_opusSegmentTableRdPtr++;
        m_opusSegmentTableSize--;
        segmLen = m_opusSegmentTable[m_opusSegmentTableRdPtr];
    }

    if (m_opusPageNr == 0) { // OpusHead
        ret = opusDecodePage0(inbuf, bytesLeft, segmLen);
        m_comment.reset();
        goto exit;
    }

    else if (m_opusPageNr == 1) { // OpusComment Subsequent Pages
        ret = parseOpusComment(inbuf, segmLen, m_opusCurrentFilePos + bytes_consumed);

        if (ret == OPUS_COMMENT_INVALID) {
            OPUS_LOG_ERROR("Error in Opus comment page");
            return OPUS_ERR;
        } else if (ret == OPUS_COMMENT_NEED_MORE) { // more comment pages follows
            *bytesLeft -= segmLen;
            ret = OPUS_PARSE_OGG_DONE;
        } else { // OPUS_COMMENT_DONE
            *bytesLeft -= segmLen;
            m_opusPageNr = 3; // all comments are consumed
            ret = OPUS_PARSE_OGG_DONE;
        }
        goto exit;
    }

    else if (m_opusPageNr == 3) {
        ret = opusDecodePage3(inbuf, bytesLeft, segmLen, m_out16.get()); // decode audio
        goto exit;
    }

    else {
    }

exit:
    if (m_opusSegmentTableSize == 0) {
        m_opusSegmentTableRdPtr = -1; // back to the parking position
    }

    if (ret >= 0) { m_opusCurrentFilePos += bytesLeft_begin - (*bytesLeft); }

    if(ret == 0){

        if (m_opusChannels == 1) {
            for (int i = 0; i < m_opusValidSamples; i++) {
                outbuf[i * 2] = m_out16[i] << 16;
                outbuf[i * 2 + 1] = m_out16[i] << 16;
            }
        }

        if (m_opusChannels == 2) {
            for (int i = 0; i < m_opusValidSamples * 2; i++) { outbuf[i] = m_out16[i] << 16; }
        }

    }

    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::opusDecodePage0(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength) {
    int32_t ret = 0;
    ret = parseOpusHead(inbuf, segmentLength);
    *bytesLeft -= segmentLength;
    //   m_opusCurrentFilePos += segmentLength;
    if (ret == 1) { m_opusPageNr++; }
    if (ret == 0) {
        OPUS_LOG_ERROR("Opus head not found");
        return OPUS_ERR;
    }
    if (ret < 0) return ret;
    return OPUS_PARSE_OGG_DONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::opusDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t* outbuf) {

    if (m_opusAudioDataStart == 0) { m_opusAudioDataStart = m_opusCurrentFilePos; }
    m_endband = 21;

    int32_t ret = 0;

    if (m_frameCount > 0) goto FramePacking; // more than one frame in the packet

    m_odp3.configNr = parseOpusTOC(inbuf[0]);
    if (m_odp3.configNr < 0) {
        OPUS_LOG_ERROR("wrong config number: %i", m_odp3.configNr);
        return OPUS_ERR;
    } // SILK or Hybrid mode

    switch (m_odp3.configNr) {
        case 0 ... 3:
            m_endband = 0; // OPUS_BANDWIDTH_SILK_NARROWBAND
            m_mode = MODE_SILK_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_NARROWBAND;
            m_internalSampleRate = 8000;
            break;
        case 4 ... 7:
            m_endband = 0; // OPUS_BANDWIDTH_SILK_MEDIUMBAND
            m_mode = MODE_SILK_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_MEDIUMBAND;
            m_internalSampleRate = 12000;
            break;
        case 8 ... 11:
            m_endband = 0; // OPUS_BANDWIDTH_SILK_WIDEBAND
            m_mode = MODE_SILK_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_WIDEBAND;
            m_internalSampleRate = 16000;
            break;
        case 12 ... 13:
            m_endband = 0; // OPUS_BANDWIDTH_HYBRID_SUPERWIDEBAND
            m_mode = MODE_HYBRID;
            m_bandWidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
            break;
        case 14 ... 15:
            m_endband = 0; // OPUS_BANDWIDTH_HYBRID_FULLBAND
            m_mode = MODE_HYBRID;
            m_bandWidth = OPUS_BANDWIDTH_FULLBAND;
            break;
        case 16 ... 19:
            m_endband = 13; // OPUS_BANDWIDTH_CELT_NARROWBAND
            m_mode = MODE_CELT_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_NARROWBAND;
            break;
        case 20 ... 23:
            m_endband = 17; // OPUS_BANDWIDTH_CELT_WIDEBAND
            m_mode = MODE_CELT_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_WIDEBAND;
            break;
        case 24 ... 27:
            m_endband = 19; // OPUS_BANDWIDTH_CELT_SUPERWIDEBAND
            m_mode = MODE_CELT_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
            break;
        case 28 ... 31:
            m_endband = 21; // OPUS_BANDWIDTH_CELT_FULLBAND
            m_mode = MODE_CELT_ONLY;
            m_bandWidth = OPUS_BANDWIDTH_FULLBAND;
            break;
        default:
            OPUS_LOG_WARN("unknown bandwifth %i, m_odp3.configNr");
            m_endband = 21; // assume OPUS_BANDWIDTH_FULLBAND
            break;
    }

    //    celt_decoder_ctl(CELT_SET_START_BAND_REQUEST, m_endband);
    if (m_mode == MODE_CELT_ONLY) {
        celtdec->celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, m_endband);
    } else if (m_mode == MODE_SILK_ONLY) {
        // silk_InitDecoder();
    }

    m_odp3.samplesPerFrame = opus_packet_get_samples_per_frame(inbuf, /*s_opusSamplerate*/ 48000);

FramePacking: // https://www.tech-invite.com/y65/tinv-ietf-rfc-6716-2.html   3.2. Frame Packing
              // OPUS_LOG_INFO("s_opusCountCode %i, configNr %i", m_opusCountCode, configNr);

    switch (m_opusCountCode) {
        case 0: // Code 0: One Frame in the Packet
            ret = opus_FramePacking_Code0(inbuf, bytesLeft, outbuf, segmentLength, m_odp3.samplesPerFrame);
            break;
        case 1: // Code 1: Two Frames in the Packet, Each with Equal Compressed Size
            ret = opus_FramePacking_Code1(inbuf, bytesLeft, outbuf, segmentLength, m_odp3.samplesPerFrame, &m_frameCount);
            break;
        case 2: // Code 2: Two Frames in the Packet, with Different Compressed Sizes
            ret = opus_FramePacking_Code2(inbuf, bytesLeft, outbuf, segmentLength, m_odp3.samplesPerFrame, &m_frameCount);
            break;
        case 3: // Code 3: A Signaled Number of Frames in the Packet
            ret = opus_FramePacking_Code3(inbuf, bytesLeft, outbuf, segmentLength, m_odp3.samplesPerFrame, &m_frameCount);
            break;
        default:
            OPUS_LOG_ERROR("Opus unknown count code %i", m_opusCountCode);
            return OPUS_ERR;
            break;
    }
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::opus_decode_frame(uint8_t* inbuf, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame) {
    if (!packetLen) {
        OPUS_LOG_WARN("Opus packetLen is 0");
        return 0;
    }
    int      i, silk_ret = 0, celt_ret = 0;
    uint16_t audiosize = 960;
    uint8_t  payloadSize_ms = max(10, 1000 * samplesPerFrame / 48000); /* The SILK PLC cannot produce frames of less than 10 ms */
    int      decoded_samples = 0;
    int32_t  silk_frame_size;
    uint8_t  start_band = 17;
    uint8_t  end_band = 21;

    silkdec->setChannelsAPI(m_opusChannels);
    silkdec->setChannelsInternal(m_opusChannels);
    silkdec->setAPIsampleRate(48000);

    if (m_bandWidth == OPUS_BANDWIDTH_NARROWBAND) {
        m_internalSampleRate = 8000;
    } else if (m_bandWidth == OPUS_BANDWIDTH_MEDIUMBAND) {
        m_internalSampleRate = 12000;
    } else if (m_bandWidth == OPUS_BANDWIDTH_WIDEBAND) {
        m_internalSampleRate = 16000;
    } else {
        m_internalSampleRate = 16000;
    }

    if (m_prev_mode == MODE_NONE) celtdec->celt_decoder_ctl((int32_t)OPUS_RESET_STATE);

    if (m_mode == MODE_CELT_ONLY) {
        if (m_prev_mode != m_mode) {
            celtdec->celt_decoder_ctl((int32_t)OPUS_RESET_STATE);
            rangedec->dec_init((uint8_t*)inbuf, packetLen);
            celtdec->celt_decoder_ctl((int32_t)CELT_SET_START_BAND_REQUEST, 0);
        }
        m_prev_mode = m_mode;
        rangedec->dec_init((uint8_t*)inbuf, packetLen);
        celtdec->celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, m_endband);
        return celtdec->celt_decode_with_ec((int16_t*)outbuf, samplesPerFrame);
    }

    if (m_mode == MODE_SILK_ONLY) {
        if (m_prev_mode == MODE_CELT_ONLY) silkdec->silk_InitDecoder();
        decoded_samples = 0;
        rangedec->dec_init((uint8_t*)inbuf, samplesPerFrame);
        silkdec->silk_setRawParams(m_opusChannels, 2, payloadSize_ms, m_internalSampleRate, 48000);
        do { /* Call SILK decoder */
            int first_frame = decoded_samples == 0;
            int silk_ret = silkdec->silk_Decode(0, first_frame, (int16_t*)outbuf + decoded_samples, &silk_frame_size);
            if (silk_ret < 0) return silk_ret;
            decoded_samples += silk_frame_size;
        } while (decoded_samples < samplesPerFrame);

        return decoded_samples;
    }

    if (m_mode == MODE_HYBRID) {
        rangedec->dec_init((uint8_t*)inbuf, packetLen);
        int             pcm_silk_size = samplesPerFrame * 4;
        ps_ptr<int16_t> pcm_silk;
        pcm_silk.alloc_array(pcm_silk_size);
        int16_t* pcm_ptr;
        pcm_ptr = pcm_silk.get();
        if (m_prev_mode == MODE_CELT_ONLY || m_prev_mode == MODE_NONE) silkdec->silk_InitDecoder();
        decoded_samples = 0;
        silkdec->silk_setRawParams(m_opusChannels, 2, payloadSize_ms, m_internalSampleRate, 48000);
        do { /* Call SILK decoder */
            int     first_frame = decoded_samples == 0;
            int32_t nSamplesOut;
            silk_ret = silkdec->silk_Decode(0, first_frame, pcm_ptr, &nSamplesOut);
            if (silk_ret < 0) return silk_ret;
            pcm_ptr += nSamplesOut * m_opusChannels;
            decoded_samples += nSamplesOut;
        } while (decoded_samples < audiosize);

        if (rangedec->tell() + 17 + 20 <= 8 * packetLen) {
            /* Check if we have a redundant 0-8 kHz band */
            rangedec->dec_bit_logp(12);
        }
        if (m_bandWidth) {
            switch (m_bandWidth) {
                case OPUS_BANDWIDTH_NARROWBAND: end_band = 13; break;
                case OPUS_BANDWIDTH_MEDIUMBAND:
                case OPUS_BANDWIDTH_WIDEBAND: end_band = 17; break;
                case OPUS_BANDWIDTH_SUPERWIDEBAND: end_band = 19; break;
                case OPUS_BANDWIDTH_FULLBAND: end_band = 21; break;
                default: break;
            }
            celtdec->celt_decoder_ctl((int32_t)CELT_SET_END_BAND_REQUEST, (end_band));
            celtdec->celt_decoder_ctl((int32_t)CELT_SET_CHANNELS_REQUEST, (m_opusChannels));
        }

        /* MUST be after PLC */
        celtdec->celt_decoder_ctl((int32_t)CELT_SET_START_BAND_REQUEST, start_band);

        /* Make sure to discard any previous CELT state */
        if (m_mode != m_prev_mode && m_prev_mode > 0) celtdec->celt_decoder_ctl((int32_t)OPUS_RESET_STATE);
        celt_ret = celtdec->celt_decode_with_ec(outbuf, audiosize);

        for (i = 0; i < audiosize * m_opusChannels; i++) outbuf[i] = celtdec->SAT16(ADD32(outbuf[i], pcm_silk[i]));

        m_prev_mode = MODE_HYBRID;
        return celt_ret < 0 ? celt_ret : audiosize;
    }
    m_prev_mode = MODE_NONE;
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t OpusDecoder::opus_FramePacking_Code0(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame) {

    /*  Code 0: One Frame in the Packet

        For code 0 packets, the TOC byte is immediately followed by N-1 bytes
        of compressed data for a single frame (where N is the size of the
        packet), as illustrated
           0                   1                   2                   3
           0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          | config  |s|0|0|                                               |
          +-+-+-+-+-+-+-+-+                                               |
          |                    Compressed frame 1 (N-1 bytes)...          :
          :                                                               |
          |                                                               |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    int32_t ret = 0;
    *bytesLeft -= packetLen;
    //   m_opusCurrentFilePos += packetLen;
    packetLen--;
    inbuf++;
    ret = opus_decode_frame(inbuf, outbuf, packetLen, samplesPerFrame);

    if (ret < 0) {
        return ret; // decode err
    }
    m_opusValidSamples = ret;
    return OPUS_NONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t OpusDecoder::opus_FramePacking_Code1(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount) {

    /*  Code 1: Two Frames in the Packet, Each with Equal Compressed Size

       For code 1 packets, the TOC byte is immediately followed by the (N-1)/2 bytes [where N is the size of the packet] of compressed
       data for the first frame, followed by (N-1)/2 bytes of compressed data for the second frame, as illustrated. The number of payload bytes
       available for compressed data, N-1, MUST be even for all code 1 packets.
          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | config  |s|0|1|                                               |
         +-+-+-+-+-+-+-+-+                                               :
         |             Compressed frame 1 ((N-1)/2 bytes)...             |
         :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                               |                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
         |             Compressed frame 2 ((N-1)/2 bytes)...             |
         :                                               +-+-+-+-+-+-+-+-+
         |                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    int32_t ret = 0;
    if (*frameCount == 0) {
        packetLen--;
        inbuf++;
        *bytesLeft -= 1;
        //    m_opusCurrentFilePos += 1;
        m_ofp3.c1fs = packetLen / 2;
        // OPUS_LOG_WARN("OPUS countCode 1 len %i, c1fs %i", len, c1fs);
        *frameCount = 2;
    }
    if (*frameCount > 0) {
        ret = opus_decode_frame(inbuf, outbuf, m_ofp3.c1fs, samplesPerFrame);
        // OPUS_LOG_WARN("code 1, ret %i", ret);
        if (ret < 0) {
            *frameCount = 0;
            return ret; // decode err
        }
        m_opusValidSamples = ret;
        *bytesLeft -= m_ofp3.c1fs;
        //    m_opusCurrentFilePos += m_ofp3.c1fs;
    }
    *frameCount -= 1;
    return OPUS_NONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t OpusDecoder::opus_FramePacking_Code2(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount) {

    /*  Code 2: Two Frames in the Packet, with Different Compressed Sizes

       For code 2 packets, the TOC byte is followed by a one- or two-byte sequence indicating the length of the first frame (marked N1 in the Figure),
       followed by N1 bytes of compressed data for the first frame.  The remaining N-N1-2 or N-N1-3 bytes are the compressed data for the second frame.
       This is illustrated in the Figure.  A code 2 packet MUST contain enough bytes to represent a valid length.  For example, a 1-byte code 2 packet
       is always invalid, and a 2-byte code 2 packet whose second byte is in the range 252...255 is also invalid. The length of the first frame, N1,
       MUST also be no larger than the size of the payload remaining after decoding that length for all code 2 packets. This makes, for example,
       a 2-byte code 2 packet with a second byte in the range 1...251 invalid as well (the only valid 2-byte code 2 packet is one where the length of
       both frames is zero).
       compute N1:  o  1...251: Length of the frame in bytes
                    o  252...255: A second byte is needed.  The total length is (second_byte*4)+first_byte

          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | config  |s|1|0| N1 (1-2 bytes):                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
         |               Compressed frame 1 (N1 bytes)...                |
         :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                               |                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
         |                     Compressed frame 2...                     :
         :                                                               |
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    int32_t ret = 0;

    if (*frameCount == 0) {
        uint8_t b1 = inbuf[1];
        uint8_t b2 = inbuf[2];
        if (b1 < 252) {
            m_ofp2.firstFrameLength = b1;
            packetLen -= 2;
            *bytesLeft -= 2;
            //     m_opusCurrentFilePos += 2;
            inbuf += 2;
        } else {
            m_ofp2.firstFrameLength = b1 + (b2 * 4);
            packetLen -= 3;
            *bytesLeft -= 3;
            //     m_opusCurrentFilePos += 3;
            inbuf += 3;
        }
        m_ofp2.secondFrameLength = packetLen - m_ofp2.firstFrameLength;
        *frameCount = 2;
    }
    if (*frameCount == 2) {
        ret = opus_decode_frame(inbuf, outbuf, m_ofp2.firstFrameLength, samplesPerFrame);
        // OPUS_LOG_WARN("code 2, ret %i", ret);
        if (ret < 0) {
            *frameCount = 0;
            return ret; // decode err
        }
        m_opusValidSamples = ret;
        *bytesLeft -= m_ofp2.firstFrameLength;
        //    m_opusCurrentFilePos += m_ofp2.firstFrameLength;
    }
    if (*frameCount == 1) {
        ret = opus_decode_frame(inbuf, outbuf, m_ofp2.secondFrameLength, samplesPerFrame);
        // OPUS_LOG_WARN("code 2, ret %i", ret);
        if (ret < 0) {
            *frameCount = 0;
            return ret; // decode err
        }
        m_opusValidSamples = ret;
        *bytesLeft -= m_ofp2.secondFrameLength;
        //    m_opusCurrentFilePos += m_ofp2.secondFrameLength;
    }
    *frameCount -= 1;
    return OPUS_NONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t OpusDecoder::opus_FramePacking_Code3(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount) {

    /*  Code 3: A Signaled Number of Frames in the Packet

       Code 3 packets signal the number of frames, as well as additional padding, called "Opus padding" to indicate that this padding is added
       at the Opus layer rather than at the transport layer.  Code 3 packets MUST have at least 2 bytes [R6,R7].  The TOC byte is followed by a
       byte encoding the number of frames in the packet in bits 2 to 7 (marked "M" in the Figure )           0
                                                                                                             0 1 2 3 4 5 6 7
                                                                                                            +-+-+-+-+-+-+-+-+
                                                                                                            |v|p|     M     |
                                                                                                            +-+-+-+-+-+-+-+-+
       with bit 1 indicating whether or not Opus
       padding is inserted (marked "p" in Figure 5), and bit 0 indicating VBR (marked "v" in Figure). M MUST NOT be zero, and the audio
       duration contained within a packet MUST NOT exceed 120 ms. This limits the maximum frame count for any frame size to 48 (for 2.5 ms
       frames), with lower limits for longer frame sizes. The Figure below illustrates the layout of the frame count byte.
       When Opus padding is used, the number of bytes of padding is encoded in the bytes following the frame count byte.  Values from 0...254
       indicate that 0...254 bytes of padding are included, in addition to the byte(s) used to indicate the size of the padding.  If the value
       is 255, then the size of the additional padding is 254 bytes, plus the padding value encoded in the next byte.

                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       Padding Length 254      | 253 |           253 x 0x00                                     :
                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       Padding Length 255      | 254 |           254 x 0x00                                     :
                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       Padding Length 256      | 255 |  0  |     254 x 0x00                                     :
                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

       There MUST be at least one more byte in the packet in this case [R6,R7]. The additional padding bytes appear at the end of the packet and MUST
       be set to zero by the encoder to avoid creating a covert channel. The decoder MUST accept any value for the padding bytes, however.
       Although this encoding provides multiple ways to indicate a given number of padding bytes, each uses a different number of bytes to
       indicate the padding size and thus will increase the total packet size by a different amount.  For example, to add 255 bytes to a
       packet, set the padding bit, p, to 1, insert a single byte after the frame count byte with a value of 254, and append 254 padding bytes
       with the value zero to the end of the packet.  To add 256 bytes to a packet, set the padding bit to 1, insert two bytes after the frame
       count byte with the values 255 and 0, respectively, and append 254 padding bytes with the value zero to the end of the packet.  By using
       the value 255 multiple times, it is possible to create a packet of any specific, desired size.  Let P be the number of header bytes used
       to indicate the padding size plus the number of padding bytes themselves (i.e., P is the total number of bytes added to the
       packet). Then, P MUST be no more than N-2. In the CBR case, let R=N-2-P be the number of bytes remaining in the packet after subtracting the
       (optional) padding. Then, the compressed length of each frame in bytes is equal to R/M.  The value R MUST be a non-negative integer multiple
       of M. The compressed data for all M frames follows, each of size R/M bytes, as illustrated in the Figure below.
          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | config  |s|1|1|0|p|     M     |  Padding length (Optional)    :
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :               Compressed frame 1 (R/M bytes)...               :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :               Compressed frame 2 (R/M bytes)...               :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :                              ...                              :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :               Compressed frame M (R/M bytes)...               :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         :                  Opus Padding (Optional)...                   |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

       In the VBR case, the (optional) padding length is followed by M-1 frame lengths (indicated by "N1" to "N[M-1]" in Figure 7), each encoded in a
       one- or two-byte sequence as described above. The packet MUST contain enough data for the M-1 lengths after removing the (optional) padding,
       and the sum of these lengths MUST be no larger than the number of bytes remaining in the packet after decoding them. The compressed data for
       all M frames follows, each frame consisting of the indicated number of bytes, with the final frame consuming any remaining bytes before the final
       padding, as illustrated in the Figure below. The number of header bytes (TOC byte, frame count byte, padding length bytes, and frame length bytes),
       plus the signaled length of the first M-1 frames themselves, plus the signaled length of the padding MUST be no larger than N, the total
       size of the packet.
          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | config  |s|1|1|1|p|     M     | Padding length (Optional)     :
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         : N1 (1-2 bytes): N2 (1-2 bytes):     ...       :     N[M-1]    |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :               Compressed frame 1 (N1 bytes)...                :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :               Compressed frame 2 (N2 bytes)...                :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :                              ...                              :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         :                     Compressed frame M...                     :
         |                                                               |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         :                  Opus Padding (Optional)...                   |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    */
    int32_t ret = 0;
    int32_t current_payload_offset = 0; // Offset from inbuf start where the current frame data begins
    m_ofp3.idx = 0;

    if (m_ofp3.firstCall) {
        //    OPUS_LOG_WARN("0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X ",
        //          inbuf[0], inbuf[1], inbuf[2], inbuf[3], inbuf[4], inbuf[5], inbuf[6], inbuf[7], inbuf[8], inbuf[9]);

        // Reset all relevant state for a new packet
        m_ofp3.firstCall = false;
        m_ofp3.paddingLength = 0;
        m_ofp3.v = false;
        m_ofp3.p = false;
        m_ofp3.M = 0;
        m_ofp3.fs = 0;
        // Ensure vfs is cleared or handled appropriately if it's a static member
        // memset(m_ofp3.vfs, 0, sizeof(m_ofp3.vfs)); // Only if m_ofp3.vfs is part of m_ofp3

        //   m_opusCurrentFilePos += packetLen;
        m_ofp3.idx = 1; // Start reading after TOC byte (inbuf[0])
        m_ofp3.spf = samplesPerFrame;

        // Parse Frame Count Byte (inbuf[1])
        if (m_ofp3.idx >= packetLen) { // Check bounds before accessing
            *bytesLeft -= packetLen;   // Consume this potentially malformed packet
            *frameCount = 0;
            m_ofp3.firstCall = true;
            return OPUS_NONE; // Packet too short
        }
        if (inbuf[m_ofp3.idx] & 0b10000000) m_ofp3.v = true; // VBR indicator
        if (inbuf[m_ofp3.idx] & 0b01000000) m_ofp3.p = true; // padding bit
        m_ofp3.M = inbuf[m_ofp3.idx] & 0b00111111;           // framecount
        *frameCount = m_ofp3.M;                              // Set the output frameCount for this packet
        m_ofp3.idx++;                                        // Move past the frame count byte

        // M MUST NOT be zero (from spec)
        if (m_ofp3.M == 0) {
            // OPUS_LOG_INFO("Error: Opus Code 3 packet with M = 0 (no frames)");
            *bytesLeft -= packetLen;
            *frameCount = 0;
            m_ofp3.firstCall = true;
            OPUS_LOG_ERROR("Opus code 3; packet with no frames");
            return OPUS_ERR;
        }

        // Parse Padding Length
        if (m_ofp3.p) {
            uint32_t current_padding_chunk_val;
            do {
                if (m_ofp3.idx >= packetLen) { // Check bounds
                    // OPUS_LOG_INFO("Error: Packet truncated during padding length parsing");
                    *bytesLeft -= packetLen;
                    *frameCount = 0;
                    m_ofp3.firstCall = true;
                    OPUS_LOG_ERROR("Opus packet is truncated during padding length parsing");
                    return OPUS_ERR;
                }
                current_padding_chunk_val = inbuf[m_ofp3.idx];
                m_ofp3.idx++;
                m_ofp3.paddingLength += current_padding_chunk_val;
            } while (current_padding_chunk_val == 255); // Continue if the last byte read was 255
            // OPUS_LOG_WARN("we have %i padding bytes", m_ofp3.paddingLength);
        }

        // Parse Variable Frame Sizes (N1 to N[M-1] for VBR)
        if (m_ofp3.v && m_ofp3.M > 1) { // Only M-1 lengths are signaled if M > 1
            for (int m = 0; m < (m_ofp3.M - 1); m++) {
                if (m_ofp3.idx >= packetLen) { // Check bounds
                    *bytesLeft -= packetLen;
                    *frameCount = 0;
                    m_ofp3.firstCall = true;
                    OPUS_LOG_ERROR("Opus packet has been truncated at VBR parsing");
                    return OPUS_ERR;
                }
                uint16_t current_frame_len_val = inbuf[m_ofp3.idx];
                m_ofp3.idx++;
                if (current_frame_len_val == 255) {
                    if (m_ofp3.idx >= packetLen) { // Check bounds for second byte
                        // OPUS_LOG_INFO("Error: Packet truncated during VBR frame length parsing (second byte)");
                        *bytesLeft -= packetLen;
                        *frameCount = 0;
                        m_ofp3.firstCall = true;
                        OPUS_LOG_ERROR("Opus packet has been truncated at VBR parsing");
                        return OPUS_ERR;
                    }
                    current_frame_len_val += inbuf[m_ofp3.idx]; // Add the next byte's value
                    m_ofp3.idx++;
                }
                m_ofp3.vfs[m] = current_frame_len_val;
                // OPUS_LOG_INFO("VFS[%i]: %i", m, m_ofp3.vfs[m]);
            }
        }

        // Calculate bytes available for compressed audio frames
        int32_t total_header_bytes = m_ofp3.idx; // idx now points to start of first compressed frame data
        int32_t remaining_bytes_for_data_and_padding = packetLen - total_header_bytes;

        // Verify enough data for padding
        if (remaining_bytes_for_data_and_padding < m_ofp3.paddingLength) {
            // OPUS_LOG_INFO("Error: Padding length %i exceeds remaining packet bytes %i", m_ofp3.paddingLength, remaining_bytes_for_data_and_padding);
            *bytesLeft -= packetLen;
            *frameCount = 0;
            m_ofp3.firstCall = true;
            OPUS_LOG_ERROR("Too many parsing bytes: %i, padding length; %i", remaining_bytes_for_data_and_padding, m_ofp3.paddingLength);
            return OPUS_ERR;
        }

        // Bytes containing actual compressed data (excluding padding at the end)
        int32_t compressed_data_bytes = remaining_bytes_for_data_and_padding - m_ofp3.paddingLength;

        // OPUS_LOG_INFO("packetLen %i, total_header_bytes %i, compressed_data_bytes %i, paddingLength %i, framecount %u",
        //      packetLen, total_header_bytes, compressed_data_bytes, m_ofp3.paddingLength, *frameCount);

        if (!m_ofp3.v) { // Constant Bitrates (CBR)
            // R = N - 2 - P
            // R = packetLen (N) - (TOC + FrameCountByte) - (Padding Header Bytes + Padding Data Bytes)
            // R = packetLen - m_ofp3.idx (which is total header bytes) - m_ofp3.paddingLength (which is actual padding data)
            // But simplified: R = compressed_data_bytes (calculated above)

            if (m_ofp3.M == 0) { // Already checked, but good for robustness
                // OPUS_LOG_INFO("Error: CBR with 0 frames (should not happen based on spec M>0)");
                *bytesLeft -= packetLen;
                *frameCount = 0;
                m_ofp3.firstCall = true;
                OPUS_LOG_ERROR("Opus CBR wihtout frames");
                return OPUS_ERR;
            }

            m_ofp3.fs = compressed_data_bytes / m_ofp3.M;
            int r = compressed_data_bytes % m_ofp3.M;
            if (r > 0) {
                OPUS_LOG_WARN("CBR data not perfectly divisible by frame count. remainingBytes %i, frames %i, remainder %i", compressed_data_bytes, m_ofp3.M, r);
                // This might indicate a malformed packet, or a small rounding difference for very short packets.
                // For strict compliance, R MUST be a non-negative integer multiple of M.
                *bytesLeft -= packetLen;
                *frameCount = 0;
                m_ofp3.firstCall = true;
                return OPUS_NONE;
            }

            // In CBR, all frames have size m_ofp3.fs. We don't use vfs here.
        } else { // Variable Bitrates (VBR)
            // Calculate the length of the last frame (M)
            uint32_t sum_of_signaled_lengths = 0;
            for (int m = 0; m < (m_ofp3.M - 1); m++) { sum_of_signaled_lengths += m_ofp3.vfs[m]; }

            if (sum_of_signaled_lengths > compressed_data_bytes) {
                OPUS_LOG_ERROR("Opus wrong VBR length, sum_of_signaled_lengths: %i, compressed_data_bytes: %i", sum_of_signaled_lengths, compressed_data_bytes);
                return OPUS_ERR;
                *bytesLeft -= packetLen;
                *frameCount = 0;
                m_ofp3.firstCall = true;
                return OPUS_NONE;
            }
            m_ofp3.vfs[m_ofp3.M - 1] = compressed_data_bytes - sum_of_signaled_lengths;
            // OPUS_LOG_INFO("Calculated VFS[%i] (last frame): %i", m_ofp3.M - 1, m_ofp3.vfs[s_ofp3.M - 1]);
        }
        current_payload_offset = total_header_bytes; // This is where the first frame data starts
        (void)current_payload_offset;
        (*bytesLeft) -= total_header_bytes; // Account for all header bytes consumed
    }

    // Decoding loop (outside the firstCall block)
    if (*frameCount > 0) {
        int32_t frame_len;
        if (m_ofp3.v) {
            // Get the length of the current frame to decode
            uint8_t current_frame_idx = m_ofp3.M - (*frameCount); // 0 for first, M-1 for last
            if (current_frame_idx >= m_ofp3.M) {                  // Safety check
                // OPUS_LOG_INFO("Error: Invalid VFS index access. current_frame_idx %i, M %i", current_frame_idx, m_ofp3.M);
                *bytesLeft -= (*bytesLeft > 0 ? *bytesLeft : 0); // Consume remaining bytes to reset
                *frameCount = 0;
                m_ofp3.firstCall = true;
                OPUS_LOG_ERROR("opus invalid VFS index access, current_frame_idx; %i, nr of frames: %i", current_frame_idx, m_ofp3.M);
                return OPUS_ERR;
            }
            frame_len = m_ofp3.vfs[current_frame_idx];
        } else {
            frame_len = m_ofp3.fs;
        }
        // Check if enough bytes are left for the current frame
        if (*bytesLeft < frame_len) {
            OPUS_LOG_ERROR("Opus not enough bytes: %i, required: %i", *bytesLeft, frame_len);
            return OPUS_ERR;
            *bytesLeft -= (*bytesLeft > 0 ? *bytesLeft : 0); // Consume remaining bytes to reset
            *frameCount = 0;
            m_ofp3.firstCall = true;
            return OPUS_NONE;
        }

        // Decode the frame
        // The inbuf + current_payload_offset points to the start of the current frame data

        ret = opus_decode_frame(inbuf + m_ofp3.idx, outbuf, frame_len, m_ofp3.spf);
        // OPUS_LOG_INFO("code 3, fs %i, spf %i, ret %i, offs %i", frame_len, m_ofp3.spf, ret, m_ofp3.idx);
        // Update bytesLeft and frameCount
        *bytesLeft -= frame_len;
        *frameCount -= 1;
        m_opusValidSamples = ret;

        if (*frameCount > 0) {
            return OPUS_CONTINUE; // More frames in this packet
        }
    }

    // After all frames are decoded, account for padding bytes if any remain.
    // The *bytesLeft at this point should ideally be equal to m_ofp3.paddingLength
    // because total_header_bytes and frame_len (for all frames) have been subtracted.
    // If there's a mismatch, it indicates an issue or just consume the rest.
    *bytesLeft -= m_ofp3.paddingLength; // Consume padding bytes from *bytesLeft for the packet
    if (*bytesLeft < 0) {
        OPUS_LOG_WARN("Warning: Negative bytesLeft after consuming padding. Remaining: %i", *bytesLeft);
        *bytesLeft = 0; // Prevent negative
    }

    *frameCount = 0;                      // All frames processed for this packet
    m_opusValidSamples = samplesPerFrame; // Reset for next packet's first frame
    m_ofp3.firstCall = true;              // Signal for next packet
    return OPUS_NONE;                     // Packet finished
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::opus_packet_get_samples_per_frame(const uint8_t* data, int32_t Fs) {
    int32_t audiosize;
    if ((data[0] & 0x80) == 0x080) {
        audiosize = ((data[0] >> 3) & 0x03);
        audiosize = (Fs << audiosize) / 400;
    } else if ((data[0] & 0x60) == 0x60) {
        audiosize = (data[0] & 0x08) ? Fs / 50 : Fs / 100;
    } else {
        audiosize = ((data[0] >> 3) & 0x3);
        if (audiosize == 3) {
            audiosize = Fs * 60 / 1000;
        } else {
            audiosize = (Fs << audiosize) / 100;
        }
    }
    return audiosize;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t OpusDecoder::getChannels() {
    return m_opusChannels;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t OpusDecoder::getSampleRate() {
    return 48000;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t OpusDecoder::getBitsPerSample() {
    return 16;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t OpusDecoder::getOutputSamples() {
    return m_opusValidSamples;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t OpusDecoder::getAudioDataStart() {
    return m_opusAudioDataStart;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t OpusDecoder::getAudioFileDuration() {
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void OpusDecoder::setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength) {
    return;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* OpusDecoder::arg2() {
    return nullptr;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::val1() {
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::val2() {
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t OpusDecoder::getBitRate() {
    if (m_opusCompressionRatio != 0) {
        return (16 * 2 * 48000) / m_opusCompressionRatio; // bitsPerSample * channel* SampleRate/CompressionRatio
    } else
        return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* OpusDecoder::getStreamTitle() {
    if (m_f_newSteamTitle) {
        m_f_newSteamTitle = false;
        return m_comment.stream_title.c_get();
    }
    return NULL;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* OpusDecoder::whoIsIt() {
    return "OPUS";
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* OpusDecoder::arg1() { // mode CELT, SILK or HYBRID
    const char* p = "unknown mode";
    if (m_mode == MODE_CELT_ONLY) p = "Opus Mode: CELT_ONLY";
    if (m_mode == MODE_HYBRID) p = "Opus Mode: HYBRID";
    if (m_mode == MODE_SILK_ONLY) p = "Opus Mode: SILK_ONLY";
    if (m_mode == MODE_NONE) p = "Opus Mode: NONE";
    return p;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
std::vector<uint32_t> OpusDecoder::getMetadataBlockPicture() {
    if (m_f_opusNewMetadataBlockPicture) {
        m_f_opusNewMetadataBlockPicture = false;
        return m_comment.pic_vec;
    }
    std::vector<uint32_t> v;
    return v;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t OpusDecoder::parseOpusTOC(uint8_t TOC_Byte) { // https://www.rfc-editor.org/rfc/rfc6716  page 16 ff

    uint8_t configNr = 0;
    uint8_t s = 0; // stereo flag
    uint8_t c = 0;
    (void)c; // count code

    configNr = (TOC_Byte & 0b11111000) >> 3;
    s = (TOC_Byte & 0b00000100) >> 2;
    c = (TOC_Byte & 0b00000011);

    /*  Configuration       Mode  Bandwidth            FrameSizes         Audio Bandwidth   Sample Rate (Effective)

        configNr  0 ...  3  SILK     NB (narrow band)      10, 20, 40, 60ms   4 kHz              8 kHz
        configNr  4 ...  7  SILK     MB (medium band)      10, 20, 40, 60ms   6 kHz             12 kHz
        configNr  8 ... 11  SILK     WB (wide band)        10, 20, 40, 60ms   8 kHz             16 kHz
        configNr 12 ... 13  HYBRID  SWB (super wideband)   10, 20ms          12 kHz (*)         24 kHz
        configNr 14 ... 15  HYBRID   FB (full band)        10, 20ms          20 kHz (*)         48 kHz
        configNr 16 ... 19  CELT     NB (narrow band)      2.5, 5, 10, 20ms   4 kHz              8 kHz
        configNr 20 ... 23  CELT     WB (wide band)        2.5, 5, 10, 20ms   8 kHz             16 kHz
        configNr 24 ... 27  CELT    SWB (super wideband)   2.5, 5, 10, 20ms   12 kHz            24 kHz
        configNr 28 ... 31  CELT     FB (full band)        2.5, 5, 10, 20ms   20 kHz (*)        48 kHz     <-------

        (*) Although the sampling theorem allows a bandwidth as large as half the sampling rate, Opus never codes
        audio above 20 kHz, as that is the generally accepted upper limit of human hearing.

        s = 0: mono 1: stereo

        c = 0: 1 frame in the packet
        c = 1: 2 frames in the packet, each with equal compressed size
        c = 2: 2 frames in the packet, with different compressed sizes
        c = 3: an arbitrary number of frames in the packet
    */
    m_opusCountCode = c;
    m_f_opusStereoFlag = s;

    // if(configNr < 12) return ERR_OPUS_SILK_MODE_UNSUPPORTED;
    // if(configNr < 16) return ERR_OPUS_HYBRID_MODE_UNSUPPORTED;
    // if(configNr < 20) return ERR_OPUS_NARROW_BAND_UNSUPPORTED;
    // if(configNr < 24) return ERR_OPUS_WIDE_BAND_UNSUPPORTED;
    // if(configNr < 28) return ERR_OPUS_SUPER_WIDE_BAND_UNSUPPORTED;

    return configNr;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::parseOpusComment(uint8_t* inbuf, int32_t nBytes, uint32_t current_file_pos) {

    /* reference https://www.rfc-editor.org/rfc/rfc7845#section-5
       returns:
           OPUS_COMMENT_INVALID  (-1) → "OpusTags" not found
           OPUS_COMMENT_NEED_MORE (1) → needs more data (comment continues)
           OPUS_COMMENT_DONE      (2) → all comments consumed
    */
    constexpr uint32_t MAX_COMMENT_SIZE = 1024;
    int32_t            available_bytes = nBytes;

    auto parse_comment = [&](ps_ptr<char> comment) -> void {
        int idx = comment.index_of("=");
        if (idx <= 0) return;
        ps_ptr<char> key = comment.substr(0, idx);
        ps_ptr<char> val = comment.substr(idx + 1);
        if (key.starts_with_icase("metadata_block_picture")) {
            if (m_comment.item_vec.size() % 2 != 0) { OPUS_LOG_ERROR("vec.size is odd: %i", m_comment.item_vec.size()); }
            m_comment.item_vec[0] += strlen("METADATA_BLOCK_PICTURE=");
            for (int i = 0; i < m_comment.item_vec.size(); i += 2) {
                m_comment.pic_vec.push_back(m_comment.item_vec[i]);                             // start pos
                m_comment.pic_vec.push_back(m_comment.item_vec[i + 1] - m_comment.item_vec[i]); // len = end pos - start pos
            }
            m_comment.item_vec.clear();
            m_f_opusNewMetadataBlockPicture = true;
            // for (int i = 0; i < m_comment.pic_vec.size(); i += 2) { OPUS_LOG_INFO("Segment %i   %i - %i", i / 2, m_comment.pic_vec[i], m_comment.pic_vec[i + 1]); }
            OPUS_LOG_DEBUG("Skipping embedded picture (%d bytes)", val.size());
            return;
        }
       if (key.starts_with_icase("artist")) {
            if (!m_comment.stream_title.valid()) {
                m_comment.stream_title.assign(val.c_get());
            } else {
                m_comment.stream_title.append(" - ");
                m_comment.stream_title.append(val.c_get());
            }
            audio.info(audio, Audio::evt_id3data, "Artist: %s", val.c_get());
        }
        if (key.starts_with_icase("title")) {
            if (!m_comment.stream_title.valid()) {
                m_comment.stream_title.assign(val.c_get());
            } else {
                m_comment.stream_title.append(" - ");
                m_comment.stream_title.append(val.c_get());
            }
            audio.info(audio, Audio::evt_id3data, "Title: %s", val.c_get());
        }
        if (key.starts_with_icase("work")) {
            audio.info(audio, Audio::evt_id3data, "Work: %s", val.c_get());
        }
        if (key.starts_with_icase("composer")) {
            audio.info(audio, Audio::evt_id3data, "Composer: %s", val.c_get());
        }
        if (key.starts_with_icase("genre")) {
            audio.info(audio, Audio::evt_id3data, "Genre: %s", val.c_get());
        }
        if (key.starts_with_icase("date")) {
            audio.info(audio, Audio::evt_id3data, "Date: %s", val.c_get());
        }
        if (key.starts_with_icase("album")) {
            audio.info(audio, Audio::evt_id3data, "Album: %s", val.c_get());
        }
        if (key.starts_with_icase("comment")) {
            audio.info(audio, Audio::evt_id3data, "Comments: %s", val.c_get());
        }
        if (key.starts_with_icase("tracknumber")) {
            audio.info(audio, Audio::evt_id3data, "Track number/Position in set: %s", val.c_get());
        }
        if (m_comment.stream_title.valid()) m_f_newSteamTitle = true;
        // comment.println(); // optional output
        m_comment.item_vec.clear();
    };

    auto fill_content = [&](uint8_t* buff, uint32_t len) -> void {
        // defensive guards (avoid signed/unsigned confusion)
        const uint32_t S_MAX = MAX_COMMENT_SIZE;
        uint32_t       s = m_comment.comment_content.strlen(); // vorhandene länge
        if (s >= S_MAX) {
            // already full — nothing more to add
            OPUS_LOG_DEBUG("comment_content already at or above MAX_COMMENT_SIZE (%u >= %u)", s, S_MAX);
            return;
        }

        // clamp len to something sensible (len can come from the caller, so check)
        uint32_t available_space = S_MAX - s;
        uint32_t to_fill = (len <= available_space) ? len : available_space;

        OPUS_LOG_DEBUG("strlen %u, incoming len %u, to_fill %u", s, len, to_fill);

        // defensive: wenn to_fill == 0, nichts tun
        if (to_fill == 0) return;

        // copy/append execute safely
        const char* src = reinterpret_cast<const char*>(buff);
        if (s == 0) {
            // initial copy
            m_comment.comment_content.copy_from(src, to_fill);
        } else {
            // append, ensure append argument limited to to_fill
            m_comment.comment_content.append(src, to_fill);
        }
    };

    // 🔹 1. If the previous comment block was incomplete → continue now
    if (m_comment.oob) {
        int64_t tmp_to_read = (int64_t)m_comment.comment_size - (int64_t)m_comment.save_len;
        if (tmp_to_read < 0) tmp_to_read = 0;
        uint32_t to_read = (uint32_t)tmp_to_read;
        if (available_bytes <= 0) {  // clamp to available_bytes (available_bytes ist signed int)
            // nothing to do
            if (m_comment.list_length == 0) return OPUS_COMMENT_DONE;
            return OPUS_COMMENT_NEED_MORE;
        }
        if ((uint32_t)available_bytes < to_read) to_read = (uint32_t)available_bytes;

        OPUS_LOG_DEBUG("to_read %i, available_bytes %i", to_read, available_bytes);
        m_comment.start_pos = current_file_pos;
        OPUS_LOG_DEBUG("partial start %i", m_comment.start_pos);
        m_comment.item_vec.push_back(m_comment.start_pos);
        fill_content(inbuf, to_read);
        m_comment.save_len += to_read;
        m_comment.pointer = to_read;
        available_bytes -= to_read;
        if (m_comment.save_len == m_comment.comment_size) {
            OPUS_LOG_DEBUG("end %i", m_comment.start_pos + to_read);
            m_comment.item_vec.push_back(m_comment.start_pos + to_read);
            // m_comment.comment_content.println();
            parse_comment(m_comment.comment_content);
            m_comment.comment_content.reset();
            m_comment.oob = false;
            m_comment.list_length--;
        } else {
            OPUS_LOG_DEBUG("partial end %i", m_comment.start_pos + nBytes);
            m_comment.item_vec.push_back(m_comment.start_pos + nBytes);
        }
        if (m_comment.list_length == 0) return OPUS_COMMENT_DONE;
        if (available_bytes == 0) return OPUS_COMMENT_NEED_MORE;
        // fall through
    }

    // 🔹 2. If this is the first page → read header
    bool first_call = (m_comment.pointer == 0 && m_comment.list_length == 0);
    if (first_call) {
        int32_t idx = OPUS_specialIndexOf(inbuf, "OpusTags", 10);
        if (idx != 0) return OPUS_COMMENT_INVALID;

        m_comment.pointer = 8; // skip "OpusTags"
        available_bytes -= 8;
        uint32_t vendorLength = little_endian(inbuf + m_comment.pointer);
        m_comment.pointer += 4 + vendorLength; // skip vendor string
        available_bytes -= 4 + vendorLength;
        m_comment.list_length = little_endian(inbuf + m_comment.pointer);
        m_comment.pointer += 4;
        available_bytes -= 4;
        OPUS_LOG_DEBUG("VendorLen=%u, CommentCount=%u", vendorLength, m_comment.list_length);
        if(m_comment.list_length == 0){
            return OPUS_COMMENT_DONE;
        }
    }

    // 🔹 3. read comments
    while (m_comment.list_length > 0) {

        // --- handle possible split 4-byte comment length ---
        if (m_comment.partial_length > 0 || available_bytes < 4) {
            uint8_t bytes_to_copy = std::min<uint8_t>(4 - m_comment.partial_length, available_bytes);
            memcpy(m_comment.length_bytes + m_comment.partial_length, inbuf + (nBytes - available_bytes), bytes_to_copy);

            m_comment.partial_length += bytes_to_copy;
            available_bytes -= bytes_to_copy;
            m_comment.pointer += bytes_to_copy;

            OPUS_LOG_DEBUG("Partial length bytes collected: %d/4", m_comment.partial_length);

            if (m_comment.partial_length < 4) {
                // still incomplete → need more data next call
                return OPUS_COMMENT_NEED_MORE;
            }

            // now we have all 4 bytes
            m_comment.comment_size = little_endian(m_comment.length_bytes);
            m_comment.partial_length = 0; // reset for next comment
            OPUS_LOG_DEBUG("m_comment.comment_size (assembled) %u", m_comment.comment_size);
        } else {
            memcpy(m_comment.length_bytes, inbuf + (nBytes - available_bytes), 4);
            m_comment.comment_size = little_endian(m_comment.length_bytes);
            m_comment.pointer += 4;
            available_bytes -= 4;
            OPUS_LOG_DEBUG("m_comment.comment_size %u", m_comment.comment_size);
        }

        if (m_comment.comment_size <= available_bytes) { // can completely read
            m_comment.start_pos = current_file_pos + m_comment.pointer;
            OPUS_LOG_DEBUG("start %i", m_comment.start_pos);
            m_comment.item_vec.push_back(m_comment.start_pos);
            fill_content(inbuf + (nBytes - available_bytes), m_comment.comment_size);
            m_comment.end_pos = m_comment.start_pos + m_comment.comment_size;
            OPUS_LOG_DEBUG("end %i", m_comment.end_pos);
            m_comment.item_vec.push_back(m_comment.end_pos);
            m_comment.pointer += m_comment.comment_size;
            available_bytes -= m_comment.comment_size;
            parse_comment(m_comment.comment_content);
            m_comment.comment_content.reset();
            m_comment.list_length--;
            if (m_comment.list_length == 0) return OPUS_COMMENT_DONE;
        }

        else { // out of bounds
            m_comment.start_pos = current_file_pos + nBytes - available_bytes;
            OPUS_LOG_DEBUG("start %i", m_comment.start_pos);
            m_comment.item_vec.push_back(m_comment.start_pos);
            fill_content(inbuf + (nBytes - available_bytes), available_bytes);
            m_comment.save_len = available_bytes;
            OPUS_LOG_DEBUG("partial_end %i", m_comment.start_pos + m_comment.save_len);
            m_comment.item_vec.push_back(m_comment.start_pos + m_comment.save_len);
            m_comment.pointer = 0;
            m_comment.oob = true;
            return OPUS_COMMENT_NEED_MORE;
        }
    }
    return OPUS_COMMENT_NEED_MORE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::parseOpusHead(uint8_t* inbuf, int32_t nBytes) { // reference https://wiki.xiph.org/OggOpus

    int32_t idx = OPUS_specialIndexOf(inbuf, "OpusHead", 10);
    if (idx != 0) {
        return 0; // is not OpusHead
    }
    uint8_t version = *(inbuf + 8);
    (void)version;
    uint8_t  channelCount = *(inbuf + 9); // nr of channels
    uint16_t preSkip = *(inbuf + 11) << 8;
    preSkip += *(inbuf + 10);
    uint32_t sampleRate = *(inbuf + 15) << 24; // informational only
    sampleRate += *(inbuf + 14) << 16;
    sampleRate += *(inbuf + 13) << 8;
    sampleRate += *(inbuf + 12);
    uint16_t outputGain = *(inbuf + 17) << 8; // Q7.8 in dB
    outputGain += *(inbuf + 16);
    uint8_t channelMap = *(inbuf + 18);

    if (channelCount == 0 || channelCount > 2) {
        OPUS_LOG_ERROR("Opus channels out of range, ch: %i", channelCount);
        return OPUS_ERR;
    }
    m_opusChannels = channelCount;
    //    OPUS_LOG_INFO("sampleRate %i", sampleRate);
    //    if(sampleRate != 48000 && sampleRate != 44100) return ERR_OPUS_INVALID_SAMPLERATE;
    m_opusSamplerate = sampleRate;
    if (channelMap > 1) {
        OPUS_LOG_ERROR("Opus extra channels not supported");
        return OPUS_ERR;
    }

    (void)outputGain;

    m_opusError = celtdec->celt_decoder_init(m_opusChannels);
    if (m_opusError < 0) {
        OPUS_LOG_ERROR("The CELT Decoder could not be initialized");
        return OPUS_ERR;
    }
    m_opusError = celtdec->celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST, 0);
    if (m_opusError < 0) {
        OPUS_LOG_ERROR("The CELT Decoder could not be initialized");
        return OPUS_ERR;
    }
    m_opusError = celtdec->celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, 21);
    if (m_opusError < 0) {
        OPUS_LOG_ERROR("The CELT Decoder could not be initialized");
        return OPUS_ERR;
    }

    return 1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::parseOGG(uint8_t* inbuf, int32_t* bytesLeft) { // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    int32_t idx = OPUS_specialIndexOf(inbuf, "OggS", 6);
    if (idx != 0) {
        OPUS_LOG_ERROR("Opus dec async, OGG capture pattern \"OggS\" not found");
        return OPUS_ERR;
    }

    int16_t segmentTableWrPtr = -1;

    uint8_t version = *(inbuf + 4);
    (void)version;
    uint8_t headerType = *(inbuf + 5);
    (void)headerType;
    uint64_t granulePosition = (uint64_t)*(inbuf + 13) << 56; // granule_position: an 8 Byte field containing -
    granulePosition += (uint64_t)*(inbuf + 12) << 48;         // position information. For an audio stream, it MAY
    granulePosition += (uint64_t)*(inbuf + 11) << 40;         // contain the total number of PCM samples encoded
    granulePosition += (uint64_t)*(inbuf + 10) << 32;         // after including all frames finished on this page.
    granulePosition += *(inbuf + 9) << 24;                    // This is a hint for the decoder and gives it some timing
    granulePosition += *(inbuf + 8) << 16;                    // and position information. A special value of -1 (in two's
    granulePosition += *(inbuf + 7) << 8;                     // complement) indicates that no packets finish on this page.
    granulePosition += *(inbuf + 6);
    (void)granulePosition;
    uint32_t bitstreamSerialNr = *(inbuf + 17) << 24; // bitstream_serial_number: a 4 Byte field containing the
    bitstreamSerialNr += *(inbuf + 16) << 16;         // unique serial number by which the logical bitstream
    bitstreamSerialNr += *(inbuf + 15) << 8;          // is identified.
    bitstreamSerialNr += *(inbuf + 14);
    (void)bitstreamSerialNr;
    uint32_t pageSequenceNr = *(inbuf + 21) << 24; // page_sequence_number: a 4 Byte field containing the sequence
    pageSequenceNr += *(inbuf + 20) << 16;         // number of the page so the decoder can identify page loss
    pageSequenceNr += *(inbuf + 19) << 8;          // This sequence number is increasing on each logical bitstream
    pageSequenceNr += *(inbuf + 18);
    (void)pageSequenceNr;
    uint32_t CRCchecksum = *(inbuf + 25) << 24;
    CRCchecksum += *(inbuf + 24) << 16;
    CRCchecksum += *(inbuf + 23) << 8;
    CRCchecksum += *(inbuf + 22);
    (void)CRCchecksum;
    uint8_t pageSegments = *(inbuf + 26); // giving the number of segment entries

    // read the segment table (contains pageSegments bytes),  1...251: Length of the frame in bytes,
    // 255: A second byte is needed.  The total length is first_byte + second byte
    m_opusSegmentLength = 0;
    segmentTableWrPtr = -1;

    for (int32_t i = 0; i < pageSegments; i++) {
        int32_t n = *(inbuf + 27 + i);
        while (*(inbuf + 27 + i) == 255) {
            i++;
            if (i == pageSegments) break;
            n += *(inbuf + 27 + i);
        }
        segmentTableWrPtr++;
        m_opusSegmentTable[segmentTableWrPtr] = n;
        m_opusSegmentLength += n;
    }
    m_opusSegmentTableSize = segmentTableWrPtr + 1;
    m_opusCompressionRatio = (float)(960 * 2 * pageSegments) / m_opusSegmentLength; // const 960 validBytes out

    m_f_continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    m_f_firstPage = headerType & 0x02;     // set: this is the first page of a logical bitstream (bos)
    m_f_lastPage = headerType & 0x04;      // set: this is the last page of a logical bitstream (eos)

    if (m_f_firstPage) { m_opusPageNr = 0; }

    OPUS_LOG_DEBUG("firstPage %i, continuedPage %i, lastPage %i", m_f_firstPage, m_f_continuedPage, m_f_lastPage);

    uint16_t headerSize = pageSegments + 27;
    *bytesLeft -= headerSize;
    //   m_opusCurrentFilePos += headerSize;
    m_opusOggHeaderSize = headerSize;

    int32_t pLen = _min((int32_t)m_opusSegmentLength, m_opusRemainBlockPicLen);
    //  OPUS_LOG_INFO("s_opusSegmentLength %i, m_opusRemainBlockPicLen %i", m_opusSegmentLength, m_opusRemainBlockPicLen);
    if (m_opusBlockPicLen && pLen > 0) {
        m_opusBlockPicItem.push_back(m_opusCurrentFilePos);
        m_opusBlockPicItem.push_back(pLen);
    }
    return OPUS_NONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::findSyncWord(uint8_t* buf, int32_t nBytes) {
    // assume we have a ogg wrapper
    int32_t idx = OPUS_specialIndexOf(buf, "OggS", nBytes);
    if (idx >= 0) { // Magic Word found
                    //    OPUS_LOG_INFO("OggS found at %i", idx);
        m_f_opusParseOgg = true;
        return idx;
    }
    m_f_opusParseOgg = false;
    OPUS_LOG_ERROR("Opus syncword not found");
    return OPUS_ERR;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::OPUS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact) {
    int32_t result = -1;                  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int32_t i = 0; i < baselen - strlen(str); i++) {
        result = i;
        for (int32_t j = 0; j < strlen(str) + exact; j++) {
            if (*(base + i + j) != *(str + j)) {
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t OpusDecoder::OPUS_specialIndexOf_icase(uint8_t* base, const char* str, int32_t baselen, bool exact) {
    int32_t result = -1;                  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int32_t i = 0; i < baselen - strlen(str); i++) {
        result = i;
        for (int32_t j = 0; j < strlen(str) + exact; j++) {
            if (tolower(*(base + i + j)) != tolower(*(str + j))) {
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t OpusDecoder::little_endian(uint8_t* data) {
    return (uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24));
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
