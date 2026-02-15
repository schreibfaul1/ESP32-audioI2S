/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation https://xiph.org/                     *
 *                                                                  *
 ********************************************************************/
/*
 * vorbis decoder.cpp
 * based on Xiph.Org Foundation vorbis decoder
 * adapted for the ESP32 by schreibfaul1
 *
 *  Created on: 13.02.2023
 *  Updated on: 14.02.2026
 */
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//                                     O G G    I M P L.
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#include "vorbis_decoder.h"
#include "lookup.h"

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool VorbisDecoder::init() {
    setDefaults();
    m_ogg_items.lastSegmentTable.alloc(4096, "m_lastSegmentTable");
    m_out16.alloc_array(4608 * 2, "m_out16");
    if (m_ogg_items.lastSegmentTable.valid() && m_out16.valid()) m_f_isValid = true;
    return m_f_isValid;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::reset() {
    clearGlobalConfigurations();
    if (m_codebooks.valid()) m_codebooks.reset();
    if (m_floor_param.valid()) m_floor_param.reset();
    if (m_floor_type.valid()) m_floor_type.reset();
    if (m_residue_param.valid()) m_residue_param.reset();
    if (m_map_param.valid()) m_map_param.reset();
    if (m_mode_param.valid()) m_mode_param.reset();
    if (m_dsp_state.valid()) m_dsp_state.reset();
    m_ogg_items.segment_table.clear();
    m_out16.reset();
    m_f_isValid = false;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::clear() {
    m_bitReader.reset();
    m_ogg_items.reset();
    m_ogg_items.lastSegmentTable.alloc(4096, "m_lastSegmentTable");
    m_vorbisBlockPicItem.clear();
    m_out16.clear();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool VorbisDecoder::isValid() {
    return m_f_isValid;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::setDefaults() {
    m_pageNr = 0;
    m_f_newSteamTitle = false; // streamTitle
    m_f_newMetadataBlockPicture = false;
    m_f_parseOggDone = false;
    m_f_oggFirstPage = false;
    m_f_oggContinuedPage = false;
    m_f_oggLastPage = false;
    m_f_comment_done = false;
    if (m_dsp_state.valid()) { vorbis_dsp_destroy(m_dsp_state); }
    m_vorbisChannels = 0;
    m_vorbisSamplerate = 0;
    m_vorbisBitRate = 0;
    m_vorbisValidSamples = 0;
    m_vorbisCurrentFilePos = 0;
    m_vorbisAudioDataStart = 0;
    m_vorbisOldMode = 0xFF;
    m_vorbisError = 0;
    m_vorbisBlockPicPos = 0;
    m_vorbisBlockPicLen = 0;
    m_commentBlockSegmentSize = 0;
    m_vorbisBlockPicItem.shrink_to_fit();
    clear();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::clearGlobalConfigurations() { // mode, mapping, floor etc
    vorbis_book_clear(m_codebooks);
    vorbis_dsp_destroy(m_dsp_state);

    if (m_nrOfFloors) {
        for (int32_t i = 0; i < m_nrOfFloors; i++) {
            m_floor_param[i]->_class.reset();
            m_floor_param[i]->partitionclass.reset();
            m_floor_param[i]->postlist.reset();
            m_floor_param[i]->forward_index.reset();
            m_floor_param[i]->hineighbor.reset();
            m_floor_param[i]->loneighbor.reset();
            m_floor_param[i].reset();
        }
        m_floor_param.reset();
        m_nrOfFloors = 0;
    }

    if (m_nrOfResidues) {
        for (int32_t i = 0; i < m_nrOfResidues; i++) {
            m_residue_param[i].stagemasks.reset();
            m_residue_param[i].stagebooks.reset();
        }
        m_residue_param.reset();
        m_nrOfResidues = 0;
    }

    if (m_nrOfMaps) {
        for (int32_t i = 0; i < m_nrOfMaps; i++) {
            m_map_param[i].chmuxlist.reset();
            m_map_param[i].submaplist.reset();
            m_map_param[i].coupling.reset();
        }
        m_map_param.reset();
        m_nrOfMaps = 0;
    }

    if (m_floor_type.valid()) m_floor_type.reset();
    if (m_residue_param.valid()) m_residue_param.reset();
    if (m_map_param.valid()) m_map_param.reset();
    if (m_mode_param.valid()) m_mode_param.reset();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf) {

    int32_t ret = 0;
    int32_t bytesLeft_begin = *bytesLeft;
    m_ogg_items.data_ptr = inbuf; // save for bitReader underrun

    if (!m_ogg_items.segment_table.size()) {
        ret = parse_OGG(inbuf, bytesLeft);
        m_f_parseOggDone = true;
        if (!m_ogg_items.segment_table.size()) { /* VORBIS_LOG_WARN("OggS without segments?") */
            ;
        }
        goto exit;
    }
    // With the last segment of a table, we don't know whether it will be continued in the next Ogg page.
    // So the last segment is saved. m_lastSegmentTableLen specifies the size of the last saved segment.
    // If the next Ogg Page does not contain a 'continuedPage', the last segment is played first. However,
    // if 'continuedPage' is set, the first segment of the new page is added to the saved segment and played.

    if (m_ogg_items.segment_table.size()) {
        m_vorbis_segment_length = m_ogg_items.segment_table[0];
        m_ogg_items.segment_table.pop_front();
        VORBIS_LOG_DEBUG("continue %i", m_f_oggContinuedPage);
    }

    if (m_pageNr < 4)
        if (special_index_of(inbuf, "vorbis", 10) == 1) m_pageNr++;

    switch (m_pageNr) {
        case 0: ret = VORBIS_PARSE_OGG_DONE; break;
        case 1:
            m_comment.reset();
            ret = vorbisDecodePage1(inbuf, bytesLeft, m_vorbis_segment_length); // blocksize, channels, samplerates
            break;
        case 2:
            ret = vorbisDecodePage2(inbuf, bytesLeft, m_vorbis_segment_length, m_vorbisCurrentFilePos); // comments
            break;
        case 3:
            ret = vorbisDecodePage3(inbuf, bytesLeft, m_vorbis_segment_length); // codebooks
            break;
        case 4:
            ret = vorbisDecodePage4(inbuf, bytesLeft, m_vorbis_segment_length, m_out16.get()); // decode audio
            break;
        default:
            VORBIS_LOG_ERROR("unknown page %s", m_pageNr);
            ret = VORBIS_ERR;
            break;
    }
exit:
    if (m_ogg_items.bytes_consumed_from_other > 0) {
        *bytesLeft -= m_ogg_items.bytes_consumed_from_other;
        m_ogg_items.bytes_consumed_from_other = 0;
    }
    if (ret >= 0) { m_vorbisCurrentFilePos += bytesLeft_begin - (*bytesLeft); }
    if (ret < 0) {
        VORBIS_LOG_ERROR("ret %i", ret);
        m_ogg_items.segment_table.clear();
    }
    if (ret == 0) {
        if (m_vorbisChannels == 1) {
            for (int i = 0; i < m_vorbisValidSamples; i++) {
                outbuf[i * 2] = m_out16[i] << 16;
                outbuf[i * 2 + 1] = m_out16[i] << 16;
            }
        }

        if (m_vorbisChannels == 2) {
            for (int i = 0; i < m_vorbisValidSamples * 2; i++) { outbuf[i] = m_out16[i] << 16; }
        }
    }
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————--------------------------------------------------------------------------------
int32_t VorbisDecoder::vorbisDecodePage1(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength) {
    int32_t ret = VORBIS_PARSE_OGG_DONE;
    clearGlobalConfigurations(); // if a new codebook is required, delete the old one
    int32_t idx = special_index_of(inbuf, "vorbis", 10);
    if (idx == 1) {
        // VORBIS_LOG_INFO("first packet (identification segmentLength) %i", segmentLength);
        m_identificatonHeaderLength = segmentLength;
        ret = parseVorbisFirstPacket(inbuf, segmentLength);
    } else {
        VORBIS_LOG_ERROR("vorbis identificator not found");
        ret = VORBIS_ERR;
    }

    *bytesLeft -= segmentLength;
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————--------------------------------------------------------------------------------
int32_t VorbisDecoder::vorbisDecodePage2(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, uint32_t current_file_pos) {

    int32_t ret = 0;
    ret = parseVorbisComment(inbuf, segmentLength, current_file_pos);

    if (ret == VORBIS_COMMENT_INVALID) {
        VORBIS_LOG_DEBUG("VORBIS_COMMENT_INVALID");
    } else if (ret == VORBIS_COMMENT_NEED_MORE) {
        VORBIS_LOG_DEBUG("VORBIS_COMMENT_NEED_MORE");
        *bytesLeft -= segmentLength;
    } else if (ret == VORBIS_COMMENT_DONE) {
        VORBIS_LOG_DEBUG("VORBIS_COMMENT_DONE");
        *bytesLeft -= segmentLength;
        VORBIS_LOG_DEBUG("bytesLeft %i", *bytesLeft);
    } else {
        VORBIS_LOG_DEBUG("VORBIS_UNKNOWN");
    }
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::vorbisDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength) {
    int32_t ret = VORBIS_PARSE_OGG_DONE;
    int32_t idx = special_index_of(inbuf, "vorbis", 10);
    if (idx == 1) {
        m_oggPage3Len = segmentLength - 7;           // skip ".vorbis"
        bitReader_setData(inbuf + 7, m_oggPage3Len); // skip also ".vorbis"
        VORBIS_LOG_DEBUG(" segmentLength %i", segmentLength);
        ret = parseVorbisCodebook();
    } else {
        VORBIS_LOG_ERROR("no \"vorbis\" something went wrong, segmentLenght: %i", segmentLength);
        ret = VORBIS_ERR;
    }
    m_pageNr = 4;
    m_dsp_state = vorbis_dsp_create();

    *bytesLeft -= segmentLength;
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::vorbisDecodePage4(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t* outbuf) {

    if (m_vorbisAudioDataStart == 0) { m_vorbisAudioDataStart = m_vorbisCurrentFilePos; }

    int32_t  ret = 0;
    uint16_t outBuffSize = 4608 * 2;

    if (m_f_oggLastPage) {
        if (segmentLength > 1) {
            bitReader_setData(inbuf, segmentLength);
            ret = vorbis_dsp_synthesis(inbuf, segmentLength, outbuf);
            m_vorbisValidSamples = vorbis_dsp_pcmout(outbuf, outBuffSize);
        } else {
            m_ogg_items.lastSegmentTableLen = 0;
            m_vorbisValidSamples = 0;
            ret = VORBIS_PARSE_OGG_DONE;
        }
        VORBIS_LOG_DEBUG("last page, sl %i", segmentLength);
        goto exit;
    }

    if (m_ogg_items.segment_table.size()) {
        if (m_f_oggContinuedPage) {
            m_f_oggContinuedPage = false;
            memcpy(m_ogg_items.lastSegmentTable.get() + m_ogg_items.lastSegmentTableLen, inbuf, segmentLength);
            bitReader_setData(m_ogg_items.lastSegmentTable.get(), m_ogg_items.lastSegmentTableLen + segmentLength);
            ret = vorbis_dsp_synthesis(m_ogg_items.lastSegmentTable.get(), m_ogg_items.lastSegmentTableLen + segmentLength, outbuf);
            m_vorbisValidSamples = vorbis_dsp_pcmout(outbuf, outBuffSize);
            m_ogg_items.lastSegmentTableLen = 0;
            goto exit;
        } else if (m_ogg_items.lastSegmentTableLen) {
            bitReader_setData(m_ogg_items.lastSegmentTable.get(), m_ogg_items.lastSegmentTableLen);
            ret = vorbis_dsp_synthesis(m_ogg_items.lastSegmentTable.get(), m_ogg_items.lastSegmentTableLen, outbuf);
            m_vorbisValidSamples = vorbis_dsp_pcmout(outbuf, outBuffSize);

            bitReader_setData(inbuf, segmentLength);
            ret = vorbis_dsp_synthesis(inbuf, segmentLength, outbuf);
            m_vorbisValidSamples += vorbis_dsp_pcmout(outbuf + (m_vorbisValidSamples * m_vorbisChannels), outBuffSize);
            m_ogg_items.lastSegmentTableLen = false;
            goto exit;
        } else {
            bitReader_setData(inbuf, segmentLength);
            ret = vorbis_dsp_synthesis(inbuf, segmentLength, outbuf);
            if (ret < 0) {
                VORBIS_LOG_ERROR("segmentLength %i", segmentLength);
                ps_ptr<uint8_t> hd;
                hd.set_name("p4");
                hd.copy_from(inbuf, 10);
                hd.hex_dump(10);
            }
            m_vorbisValidSamples = vorbis_dsp_pcmout(outbuf, outBuffSize);
            goto exit;
        }
    } else {
        memcpy(m_ogg_items.lastSegmentTable.get(), inbuf, segmentLength);
        m_ogg_items.lastSegmentTableLen = segmentLength;
        m_vorbisValidSamples = 0;
        ret = 0;
    }
exit:
    if (ret == OV_ENOTAUDIO) {
        m_vorbisValidSamples = 0;
        VORBIS_LOG_INFO("non audio package at pos %i", m_vorbisCurrentFilePos);
        ret = 0;
    }
    *bytesLeft -= segmentLength;
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t VorbisDecoder::getChannels() {
    return m_vorbisChannels;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t VorbisDecoder::getSampleRate() {
    return m_vorbisSamplerate;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t VorbisDecoder::getBitsPerSample() {
    return 16;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t VorbisDecoder::getBitRate() {
    return m_vorbisBitRate;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t VorbisDecoder::getAudioDataStart() {
    return m_vorbisAudioDataStart;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t VorbisDecoder::getOutputSamples() {
    return m_vorbisValidSamples; // 1024
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t VorbisDecoder::getAudioFileDuration() {
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* VorbisDecoder::getStreamTitle() {
    if (m_f_newSteamTitle) {
        m_f_newSteamTitle = false;
        return m_comment.stream_title.c_get();
    }
    return NULL;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
std::vector<uint32_t> VorbisDecoder::getMetadataBlockPicture() {
    if (m_f_newMetadataBlockPicture) {
        m_f_newMetadataBlockPicture = false;
        return m_comment.pic_vec;
    }
    std::vector<uint32_t> v;
    return v;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* VorbisDecoder::arg1() {
    return ""; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* VorbisDecoder::arg2() {
    return ""; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::val1() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::val2() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::parseVorbisFirstPacket(uint8_t* inbuf, int16_t nBytes) { // 4.2.2. Identification header
                                                                                // https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-820005
    // first bytes are: '.vorbis'
    uint16_t pos = 7;
    uint32_t version = *(inbuf + pos);
    version += *(inbuf + pos + 1) << 8;
    version += *(inbuf + pos + 2) << 16;
    version += *(inbuf + pos + 3) << 24;
    (void)version;

    uint8_t channels = *(inbuf + pos + 4);

    uint32_t sampleRate = *(inbuf + pos + 5);
    sampleRate += *(inbuf + pos + 6) << 8;
    sampleRate += *(inbuf + pos + 7) << 16;
    sampleRate += *(inbuf + pos + 8) << 24;

    uint32_t br_max = *(inbuf + pos + 9);
    br_max += *(inbuf + pos + 10) << 8;
    br_max += *(inbuf + pos + 11) << 16;
    br_max += *(inbuf + pos + 12) << 24;

    uint32_t br_nominal = *(inbuf + pos + 13);
    br_nominal += *(inbuf + pos + 14) << 8;
    br_nominal += *(inbuf + pos + 15) << 16;
    br_nominal += *(inbuf + pos + 16) << 24;

    uint32_t br_min = *(inbuf + pos + 17);
    br_min += *(inbuf + pos + 18) << 8;
    br_min += *(inbuf + pos + 19) << 16;
    br_min += *(inbuf + pos + 20) << 24;

    uint8_t blocksize = *(inbuf + pos + 21);

    m_blocksizes[0] = 1 << (blocksize & 0x0F);
    m_blocksizes[1] = 1 << ((blocksize & 0xF0) >> 4);

    if (m_blocksizes[0] < 64) {
        VORBIS_LOG_ERROR("Vorbis, blocksize[0] too low %i", m_blocksizes[0]);
        return -1;
    }
    if (m_blocksizes[1] < m_blocksizes[0]) {
        VORBIS_LOG_ERROR("Vorbis, m_blocksizes[1] %i is smaller than m_blocksizes[0] %i", m_blocksizes[1], m_blocksizes[0]);
        return -1;
    }
    if (m_blocksizes[1] > 8192) {
        VORBIS_LOG_ERROR("Vorbis, m_blocksizes[1] is too big: %i", m_blocksizes[1]);
        return -1;
    }

    if (channels < 1 || channels > 2) {
        VORBIS_LOG_ERROR("Vorbis, nr of channels is not valid ch=%i", channels);
        return -1;
    }
    m_vorbisChannels = channels;

    if (sampleRate < 4096 || sampleRate > 64000) {
        VORBIS_LOG_ERROR("Vorbis, sampleRate is not valid sr=%i", sampleRate);
        return -1;
    }
    m_vorbisSamplerate = sampleRate;

    m_vorbisBitRate = br_nominal;

    return VORBIS_PARSE_OGG_DONE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::parseVorbisComment(uint8_t* inbuf, int16_t nBytes, uint32_t current_file_pos) {
    // reference https://xiph.org/vorbis/doc/v-comment.html

    constexpr uint32_t MAX_COMMENT_SIZE = 1024;
    int32_t            available_bytes = nBytes;

    auto parse_comment = [&](ps_ptr<char> comment) -> void {
        int idx = comment.index_of("=");
        if (idx <= 0) return;
        ps_ptr<char> key = comment.substr(0, idx);
        ps_ptr<char> val = comment.substr(idx + 1);
        if (key.starts_with_icase("metadata_block_picture")) {
            if (m_comment.item_vec.size() % 2 != 0) { VORBIS_LOG_ERROR("vec.size is odd: %i", m_comment.item_vec.size()); }
            m_comment.item_vec[0] += strlen("METADATA_BLOCK_PICTURE=");
            for (int i = 0; i < m_comment.item_vec.size(); i += 2) {
                m_comment.pic_vec.push_back(m_comment.item_vec[i]);                             // start pos
                m_comment.pic_vec.push_back(m_comment.item_vec[i + 1] - m_comment.item_vec[i]); // len = end pos - start pos
            }
            m_comment.item_vec.clear();
            m_f_newMetadataBlockPicture = true;
            // for (int i = 0; i < m_comment.pic_vec.size(); i += 2) { OPUS_LOG_INFO("Segment %i   %i - %i", i / 2, m_comment.pic_vec[i], m_comment.pic_vec[i + 1]); }
            VORBIS_LOG_DEBUG("Skipping embedded picture (%d bytes)", val.size());
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
        if (key.starts_with_icase("work")) { audio.info(audio, Audio::evt_id3data, "Work: %s", val.c_get()); }
        if (key.starts_with_icase("composer")) { audio.info(audio, Audio::evt_id3data, "Composer: %s", val.c_get()); }
        if (key.starts_with_icase("genre")) { audio.info(audio, Audio::evt_id3data, "Genre: %s", val.c_get()); }
        if (key.starts_with_icase("date")) { audio.info(audio, Audio::evt_id3data, "Date: %s", val.c_get()); }
        if (key.starts_with_icase("album")) { audio.info(audio, Audio::evt_id3data, "Album: %s", val.c_get()); }
        if (key.starts_with_icase("comment")) { audio.info(audio, Audio::evt_id3data, "Comments: %s", val.c_get()); }
        if (key.starts_with_icase("tracknumber")) { audio.info(audio, Audio::evt_id3data, "Track number/Position in set: %s", val.c_get()); }

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
            VORBIS_LOG_DEBUG("comment_content already at or above MAX_COMMENT_SIZE (%u >= %u)", s, S_MAX);
            return;
        }

        // clamp len to something sensible (len can come from the caller, so check)
        uint32_t available_space = S_MAX - s;
        uint32_t to_fill = (len <= available_space) ? len : available_space;

        VORBIS_LOG_DEBUG("strlen %u, incoming len %u, to_fill %u", s, len, to_fill);

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
        if (available_bytes <= 0) { // clamp to available_bytes (available_bytes ist signed int)
            // nothing to do
            if (m_comment.list_length == 0) return VORBIS_COMMENT_DONE;
            return VORBIS_COMMENT_NEED_MORE;
        }
        if ((uint32_t)available_bytes < to_read) to_read = (uint32_t)available_bytes;

        VORBIS_LOG_DEBUG("to_read %i, available_bytes %i", to_read, available_bytes);
        m_comment.start_pos = current_file_pos;
        VORBIS_LOG_DEBUG("partial start %i", m_comment.start_pos);
        m_comment.item_vec.push_back(m_comment.start_pos);
        fill_content(inbuf, to_read);
        m_comment.save_len += to_read;
        m_comment.pointer = to_read;
        available_bytes -= to_read;
        if (m_comment.save_len == m_comment.comment_size) {
            VORBIS_LOG_DEBUG("end %i", m_comment.start_pos + to_read);
            m_comment.item_vec.push_back(m_comment.start_pos + to_read);
            // m_comment.comment_content.println();
            parse_comment(m_comment.comment_content);
            m_comment.comment_content.reset();
            m_comment.oob = false;
            m_comment.list_length--;
        } else {
            VORBIS_LOG_DEBUG("partial end %i", m_comment.start_pos + nBytes);
            m_comment.item_vec.push_back(m_comment.start_pos + nBytes);
        }
        if (m_comment.list_length == 0) return VORBIS_COMMENT_DONE;
        if (available_bytes == 0) return VORBIS_COMMENT_NEED_MORE;
        // fall through
    }
    // 🔹 2. If this is the first page → read header
    bool first_call = (m_comment.pointer == 0 && m_comment.list_length == 0);
    if (first_call) {
        int32_t idx = specialIndexOf(inbuf, "vorbis", 10);
        if (idx != 1) return VORBIS_COMMENT_INVALID;

        m_comment.pointer = 7; // skip ".vorbis"
        available_bytes -= 7;
        uint32_t vendorLength = little_endian(inbuf + m_comment.pointer);
        m_comment.pointer += 4 + vendorLength; // skip vendor string
        available_bytes -= 4 + vendorLength;
        m_comment.list_length = little_endian(inbuf + m_comment.pointer);
        m_comment.pointer += 4;
        available_bytes -= 4;
        VORBIS_LOG_DEBUG("VendorLen=%u, CommentCount=%u", vendorLength, m_comment.list_length);
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

            VORBIS_LOG_DEBUG("Partial length bytes collected: %d/4", m_comment.partial_length);

            if (m_comment.partial_length < 4) {
                // still incomplete → need more data next call
                return VORBIS_COMMENT_NEED_MORE;
            }

            // now we have all 4 bytes
            m_comment.comment_size = little_endian(m_comment.length_bytes);
            m_comment.partial_length = 0; // reset for next comment
            VORBIS_LOG_DEBUG("m_comment.comment_size (assembled) %u", m_comment.comment_size);
        } else {
            memcpy(m_comment.length_bytes, inbuf + (nBytes - available_bytes), 4);
            m_comment.comment_size = little_endian(m_comment.length_bytes);
            m_comment.pointer += 4;
            available_bytes -= 4;
            VORBIS_LOG_DEBUG("m_comment.comment_size %u", m_comment.comment_size);
        }

        if (m_comment.comment_size <= available_bytes) { // can completely read
            m_comment.start_pos = current_file_pos + m_comment.pointer;
            VORBIS_LOG_DEBUG("start %i", m_comment.start_pos);
            m_comment.item_vec.push_back(m_comment.start_pos);
            fill_content(inbuf + (nBytes - available_bytes), m_comment.comment_size);
            m_comment.end_pos = m_comment.start_pos + m_comment.comment_size;
            VORBIS_LOG_DEBUG("end %i", m_comment.end_pos);
            m_comment.item_vec.push_back(m_comment.end_pos);
            m_comment.pointer += m_comment.comment_size;
            available_bytes -= m_comment.comment_size;
            parse_comment(m_comment.comment_content);
            m_comment.comment_content.reset();
            m_comment.list_length--;
            if (m_comment.list_length == 0) return VORBIS_COMMENT_DONE;
        }

        else { // out of bounds
            m_comment.start_pos = current_file_pos + m_comment.pointer;
            VORBIS_LOG_DEBUG("start %i", m_comment.start_pos);
            m_comment.item_vec.push_back(m_comment.start_pos);
            fill_content(inbuf + (nBytes - available_bytes), available_bytes);
            m_comment.save_len = available_bytes;
            VORBIS_LOG_DEBUG("partial_end %i", m_comment.start_pos + m_comment.save_len);
            m_comment.item_vec.push_back(m_comment.start_pos + m_comment.save_len);
            m_comment.pointer = 0;
            m_comment.oob = true;
            return VORBIS_COMMENT_NEED_MORE;
        }
    }
    return VORBIS_COMMENT_NEED_MORE;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::parseVorbisCodebook() {

    int32_t i;
    int32_t ret = 0;

    m_nrOfCodebooks = bitReader(8) + 1;
    m_codebooks.calloc_array(m_nrOfCodebooks, "m_codebooks");
    VORBIS_LOG_DEBUG("nr of codebooks %i", m_nrOfCodebooks);
    for (i = 0; i < m_nrOfCodebooks; i++) {
        VORBIS_LOG_DEBUG("codebooks %i", i);
        ret = vorbis_book_unpack(m_codebooks.get() + i);
        if (ret) VORBIS_LOG_ERROR("Vorbis codebook %i returned  a err", i);
        if (ret) goto err_out;
    }

    /* time backend settings, not actually used */
    i = bitReader(6);
    for (; i >= 0; i--) {
        ret = bitReader(16);
        if (ret != 0) {
            VORBIS_LOG_ERROR("err while reading backend settings");
            goto err_out;
        }
    }
    /* floor backend settings */
    m_nrOfFloors = bitReader(6) + 1;
    m_floor_param.calloc_array(m_nrOfFloors, "m_floor_param");
    m_floor_type.alloc(sizeof(int8_t) * m_nrOfFloors, "m_floor_type");
    VORBIS_LOG_DEBUG("nr of floors %i", m_nrOfFloors);
    for (i = 0; i < m_nrOfFloors; i++) {
        VORBIS_LOG_DEBUG("floors %i", i);
        m_floor_type[i] = bitReader(16);
        if (m_floor_type[i] < 0 || m_floor_type[i] >= VI_FLOORB) {
            VORBIS_LOG_ERROR("err while reading floors");
            goto err_out;
        }
        if (m_floor_type[i]) {
            m_floor_param[i] = floor1_info_unpack();
        } else {
            m_floor_param[i] = floor0_info_unpack();
        }
        if (!m_floor_param[i].valid()) {
            VORBIS_LOG_ERROR("floor parameter not found");
            goto err_out;
        }
    }

    /* residue backend settings */
    m_nrOfResidues = bitReader(6) + 1;
    m_residue_param.calloc_array(m_nrOfResidues, "m_residue_param");
    VORBIS_LOG_DEBUG("nr of residues %i", m_nrOfResidues);
    for (i = 0; i < m_nrOfResidues; i++) {
        VORBIS_LOG_DEBUG("residue %i", i);
        if (res_unpack(m_residue_param.get() + i)) {
            VORBIS_LOG_ERROR("err while unpacking residues");
            goto err_out;
        }
    }

    // /* map backend settings */
    m_nrOfMaps = bitReader(6) + 1;
    m_map_param.calloc_array(m_nrOfMaps, "m_map_param");
    for (i = 0; i < m_nrOfMaps; i++) {
        if (bitReader(16) != 0) goto err_out;
        if (mapping_info_unpack(m_map_param.get() + i)) {
            VORBIS_LOG_ERROR("err while unpacking mappings");
            goto err_out;
        }
    }

    /* mode settings */
    m_nrOfModes = bitReader(6) + 1;
    m_mode_param.alloc(sizeof(vorbis_info_mode_t) * m_nrOfModes, "m_mode_param");
    for (i = 0; i < m_nrOfModes; i++) {
        m_mode_param[i].blockflag = bitReader(1);
        if (bitReader(16)) goto err_out;
        if (bitReader(16)) goto err_out;
        m_mode_param[i].mapping = bitReader(8);
        if (m_mode_param[i].mapping >= m_nrOfMaps) {
            VORBIS_LOG_ERROR("too many modes");
            goto err_out;
        }
    }

    if (bitReader(1) != 1) {
        VORBIS_LOG_ERROR("codebooks, end bit not found");
        goto err_out;
    }
    // if(m_setupHeaderLength != m_bitReader.headptr - m_bitReader.data){
    //     VORBIS_LOG_ERROR("Error reading setup header, assumed %i bytes, read %i bytes", m_setupHeaderLength, m_bitReader.headptr - m_bitReader.data);
    //     goto err_out;
    // }
    /* top level EOP check */

    return VORBIS_PARSE_OGG_DONE;

err_out:
    //    vorbis_info_clear(vi);
    VORBIS_LOG_ERROR("err in codebook!  at pos %d", m_bitReader.headptr - m_bitReader.data);
    return (OV_EBADHEADER);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::parse_OGG(uint8_t* inbuf, int32_t* bytesLeft) {
    // reference https://www.xiph.org/ogg/doc/rfc3533.txt
    int32_t  ret = 0;
    uint32_t ogg_lengt_total = 0;
    (void)ret;
    int32_t idx = special_index_of(inbuf, "OggS", 8192);
    if (idx != 0) {
        if (m_f_oggContinuedPage) {
            VORBIS_LOG_ERROR("Vorbis decoder asynchron, 'OggS' not found");
            return VORBIS_ERR;
        }
        inbuf += idx;
        *bytesLeft -= idx;
    }
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

    ogg_lengt_total = 27 + pageSegments;
    for (int32_t i = 0; i < pageSegments; i++) {
        int32_t n = *(inbuf + 27 + i);
        while (*(inbuf + 27 + i) == 255) {
            i++;
            if (i == pageSegments) break;
            n += *(inbuf + 27 + i);
        }
        m_ogg_items.segment_table.push_back(n);
        ogg_lengt_total += n;
    }
    m_vorbisCompressionRatio = (float)(960 * 2 * pageSegments) / ogg_lengt_total; // const 960 validBytes out

    bool continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    bool firstPage = headerType & 0x02;     // set: this is the first page of a logical bitstream (bos)
    bool lastPage = headerType & 0x04;      // set: this is the last page of a logical bitstream (eos)

    uint16_t headerSize = pageSegments + 27;

    // VORBIS_LOG_INFO("headerSize %i, m_vorbisSegmentLength %i, m_vorbisSegmentTableSize %i", headerSize, m_vorbisSegmentLength, m_vorbisSegmentTableSize);
    if (firstPage || continuedPage || lastPage) {
        // VORBIS_LOG_INFO("firstPage %i  continuedPage %i  lastPage %i", firstPage, continuedPage, lastPage);
    }

    *bytesLeft -= headerSize;
    inbuf += headerSize;

    m_f_oggFirstPage = firstPage;
    m_f_oggContinuedPage = continuedPage;
    m_f_oggLastPage = lastPage;

    if (firstPage) { m_pageNr = 0; }

    return VORBIS_PARSE_OGG_DONE; // no error
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::findSyncWord(uint8_t* buf, int32_t nBytes) {
    // assume we have a ogg wrapper
    int32_t idx = special_index_of(buf, "OggS", nBytes);
    if (idx >= 0) { // Magic Word found
                    //    VORBIS_LOG_INFO("OggS found at %i", idx);
        return idx;
    }
    VORBIS_LOG_ERROR("Vorbis, sync 'OggS' not found");
    return VORBIS_ERR;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* VorbisDecoder::whoIsIt() {
    return "VORBIS";
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength) {
    return; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::vorbis_book_unpack(codebook_t* s) {
    ps_ptr<int32_t> lengthlist;
    uint8_t         quantvals = 0;
    int32_t         i, j;
    int32_t         maptype;
    int32_t         ret = 0;

    /* make sure alignment is correct */
    if (bitReader(24) != 0x564342) {
        VORBIS_LOG_ERROR("Vorbis, string \"VCB\" not found"); // VorbisCodeBook
        goto _eofout;                                         // "BCV"
    }

    /* first the basic parameters */
    ret = bitReader(16);
    if (ret < 0) VORBIS_LOG_ERROR("error in vorbis_book_unpack, ret =%li\n", (long int)ret);
    if (ret > 255) VORBIS_LOG_ERROR("error in vorbis_book_unpack, ret =%li\n", (long int)ret);
    s->dim = (uint8_t)ret;
    s->entries = bitReader(24);
    if (s->entries == -1) {
        VORBIS_LOG_ERROR("no entries in unpack codebooks ?");
        goto _eofout;
    }

    /* codeword ordering.... length ordered or unordered? */
    switch (bitReader(1)) {
        case 0:
            /* unordered */
            lengthlist.alloc_array(s->entries * 2, "lengthlist");

            /* allocated but unused entries? */
            if (bitReader(1)) {
                /* yes, unused entries */

                for (i = 0; i < s->entries; i++) {
                    if (bitReader(1)) {
                        int32_t num = bitReader(5);
                        if (num == -1) goto _eofout;
                        lengthlist[i] = num + 1;
                        s->used_entries++;
                        if (num + 1 > s->dec_maxlength) s->dec_maxlength = num + 1;
                    } else
                        lengthlist[i] = 0;
                }
            } else {
                /* all entries used; no tagging */
                s->used_entries = s->entries;
                for (i = 0; i < s->entries; i++) {
                    int32_t num = bitReader(5);
                    if (num == -1) goto _eofout;
                    lengthlist[i] = num + 1;

                    if (num + 1 > s->dec_maxlength) s->dec_maxlength = num + 1;
                }
            }
            break;
        case 1:
            /* ordered */
            {
                int32_t length = bitReader(5) + 1;

                s->used_entries = s->entries;
                lengthlist.alloc_array(s->entries * 2, "lengthlist");

                for (i = 0; i < s->entries;) {
                    int32_t num = bitReader(_ilog(s->entries - i));
                    if (num == -1) goto _eofout;
                    for (j = 0; j < num && i < s->entries; j++, i++) lengthlist[i] = length;
                    s->dec_maxlength = length;
                    length++;
                }
            }
            break;
        default:
            /* EOF */
            goto _eofout;
    }

    /* Do we have a mapping to unpack? */
    if ((maptype = bitReader(4)) > 0) {
        s->q_min = _float32_unpack(bitReader(32), &s->q_minp);
        s->q_del = _float32_unpack(bitReader(32), &s->q_delp);

        s->q_bits = bitReader(4) + 1;
        s->q_seq = bitReader(1);

        s->q_del >>= s->q_bits;
        s->q_delp += s->q_bits;
    }

    switch (maptype) {
        case 0:
            /* no mapping; decode type 0 */
            /* how many bytes for the indexing? */
            /* this is the correct boundary here; we lose one bit to node/leaf mark */
            s->dec_nodeb = _determine_node_bytes(s->used_entries, _ilog(s->entries) / 8 + 1);
            s->dec_leafw = _determine_leaf_words(s->dec_nodeb, _ilog(s->entries) / 8 + 1);
            s->dec_type = 0;
            ret = _make_decode_table(s, lengthlist.get(), quantvals, maptype);
            if (ret != 0) { goto _errout; }
            break;

        case 1:
            /* mapping type 1; implicit values by lattice  position */
            quantvals = _book_maptype1_quantvals(s);

            /* dec_type choices here are 1,2; 3 doesn't make sense */
            {
                /* packed values */
                int32_t total1 = (s->q_bits * s->dim + 8) / 8; /* remember flag bit */
                /* vector of column offsets; remember flag bit */
                int32_t total2 = (_ilog(quantvals - 1) * s->dim + 8) / 8 + (s->q_bits + 7) / 8;

                if (total1 <= 4 && total1 <= total2) {
                    /* use dec_type 1: vector of packed values */
                    /* need quantized values before  */
                    s->q_val.calloc_array(sizeof(uint16_t) * quantvals, "q_val");
                    for (i = 0; i < quantvals; i++) ((uint16_t*)s->q_val.get())[i] = bitReader(s->q_bits);

                    if (oggpack_eop()) { goto _eofout; }

                    s->dec_type = 1;
                    s->dec_nodeb = _determine_node_bytes(s->used_entries, (s->q_bits * s->dim + 8) / 8);
                    s->dec_leafw = _determine_leaf_words(s->dec_nodeb, (s->q_bits * s->dim + 8) / 8);
                    ret = _make_decode_table(s, lengthlist.get(), quantvals, maptype);
                    if (ret) { goto _errout; }
                } else {
                    /* use dec_type 2: packed vector of column offsets */
                    /* need quantized values before */
                    if (s->q_bits <= 8) {
                        s->q_val.alloc(quantvals, "q_val");
                        for (i = 0; i < quantvals; i++) ((uint8_t*)s->q_val.get())[i] = bitReader(s->q_bits);
                    } else {
                        s->q_val.alloc(quantvals * 2, "q_val");
                        for (i = 0; i < quantvals; i++) ((uint16_t*)s->q_val.get())[i] = bitReader(s->q_bits);
                    }

                    if (oggpack_eop()) goto _eofout;

                    s->q_pack = _ilog(quantvals - 1);
                    s->dec_type = 2;
                    s->dec_nodeb = _determine_node_bytes(s->used_entries, (_ilog(quantvals - 1) * s->dim + 8) / 8);
                    s->dec_leafw = _determine_leaf_words(s->dec_nodeb, (_ilog(quantvals - 1) * s->dim + 8) / 8);

                    ret = _make_decode_table(s, lengthlist.get(), quantvals, maptype);
                    if (ret) { goto _errout; }
                }
            }
            break;
        case 2:
            /* mapping type 2; explicit array of values */
            quantvals = s->entries * s->dim;
            /* dec_type choices here are 1,3; 2 is not possible */

            if ((s->q_bits * s->dim + 8) / 8 <= 4) { /* remember flag bit */
                /* use dec_type 1: vector of packed values */

                s->dec_type = 1;
                s->dec_nodeb = _determine_node_bytes(s->used_entries, (s->q_bits * s->dim + 8) / 8);
                s->dec_leafw = _determine_leaf_words(s->dec_nodeb, (s->q_bits * s->dim + 8) / 8);
                if (_make_decode_table(s, lengthlist.get(), quantvals, maptype)) goto _errout;
            } else {
                /* use dec_type 3: scalar offset into packed value array */

                s->dec_type = 3;
                s->dec_nodeb = _determine_node_bytes(s->used_entries, _ilog(s->used_entries - 1) / 8 + 1);
                s->dec_leafw = _determine_leaf_words(s->dec_nodeb, _ilog(s->used_entries - 1) / 8 + 1);
                if (_make_decode_table(s, lengthlist.get(), quantvals, maptype)) goto _errout;

                /* get the vals & pack them */
                s->q_pack = (s->q_bits + 7) / 8 * s->dim;
                s->q_val.alloc(s->q_pack * s->used_entries, "q_val");

                if (s->q_bits <= 8) {
                    for (i = 0; i < s->used_entries * s->dim; i++) ((uint8_t*)(s->q_val.get()))[i] = bitReader(s->q_bits);
                } else {
                    for (i = 0; i < s->used_entries * s->dim; i++) ((uint16_t*)(s->q_val.get()))[i] = bitReader(s->q_bits);
                }
            }
            break;
        default: VORBIS_LOG_ERROR("maptype %i schould be 0, 1 or 2", maptype); goto _errout;
    }
    if (oggpack_eop()) goto _eofout;
    return 0; // ok
_errout:
_eofout:
    vorbis_book_clear(m_codebooks);
    return VORBIS_ERR; // error
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::special_index_of(uint8_t* base, const char* str, int32_t baselen, bool exact) {
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
void VorbisDecoder::bitReader_setData(uint8_t* buff, uint32_t buffSize) {
    m_bitReader.data = buff;
    m_bitReader.headptr = buff;
    m_bitReader.length = buffSize;
    m_bitReader.headend = buffSize * 8;
    m_bitReader.headbit = 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* Read in bits without advancing the bitptr; bits <= 32 */
int32_t VorbisDecoder::bitReader_look(uint16_t nBits) {
    uint32_t m = mask[nBits];
    int32_t  ret = 0;

    nBits += m_bitReader.headbit;

    if (nBits >= m_bitReader.headend << 3) {
        uint8_t* ptr = m_bitReader.headptr;
        if (nBits) {
            ret = *ptr++ >> m_bitReader.headbit;
            if (nBits > 8) {
                ret |= *ptr++ << (8 - m_bitReader.headbit);
                if (nBits > 16) {
                    ret |= *ptr++ << (16 - m_bitReader.headbit);
                    if (nBits > 24) {
                        ret |= *ptr++ << (24 - m_bitReader.headbit);
                        if (nBits > 32 && m_bitReader.headbit) { ret |= *ptr << (32 - m_bitReader.headbit); }
                    }
                }
            }
        }
    } else {
        /* make this a switch jump-table */
        ret = m_bitReader.headptr[0] >> m_bitReader.headbit;
        if (nBits > 8) {
            ret |= m_bitReader.headptr[1] << (8 - m_bitReader.headbit);
            if (nBits > 16) {
                ret |= m_bitReader.headptr[2] << (16 - m_bitReader.headbit);
                if (nBits > 24) {
                    ret |= m_bitReader.headptr[3] << (24 - m_bitReader.headbit);
                    if (nBits > 32 && m_bitReader.headbit) ret |= m_bitReader.headptr[4] << (32 - m_bitReader.headbit);
                }
            }
        }
    }

    ret &= (int32_t)m;
    return ret;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::bitReader(uint16_t nBits) {
    if (!m_bitReader.headptr || nBits == 0) return 0;

    uint32_t val = 0;
    uint32_t bits_collected = 0;

    while (nBits > 0) {

        // Check whether the BitReader is empty (end of the current Ogg page)
        if (m_bitReader.headptr >= m_bitReader.data + m_bitReader.length) {

            // ------------------------------------------------------------
            // 🧩 Ogg limit exceeded → read next page
            // ------------------------------------------------------------
            uint8_t leftover = 0;
            uint8_t bits_left = 0;

            // If in the middle of the byte: save remaining bits
            if (m_bitReader.headbit > 0 && m_bitReader.headptr > m_bitReader.data) {
                leftover = *(m_bitReader.headptr - 1) >> (8 - m_bitReader.headbit);
                bits_left = m_bitReader.headbit;
            }

            // Load new Ogg page
            int32_t  oggLen = 256;
            int32_t  ret = parse_OGG(m_bitReader.headptr, &oggLen);
            uint32_t header_consumed = (256 - oggLen);
            VORBIS_LOG_DEBUG("header_consumed %i", header_consumed);
            if (ret != VORBIS_PARSE_OGG_DONE) {
                VORBIS_LOG_ERROR("bitReader: failed to continue across Ogg page");
                return -1;
            }

            // Apply first segment
            if (!m_ogg_items.segment_table.empty()) {
                VORBIS_LOG_DEBUG("old segment %i", m_vorbis_segment_length);
                m_vorbis_segment_length = m_ogg_items.segment_table.front();
                m_ogg_items.segment_table.pop_front();
                VORBIS_LOG_DEBUG("new segment %i", m_vorbis_segment_length);
                uint32_t header_consumed = (256 - oggLen);
                m_ogg_items.bytes_consumed_from_other = m_vorbis_segment_length + header_consumed;
            } else {
                VORBIS_LOG_ERROR("bitReader: empty Ogg segment table");
                return -1;
            }

            // New page data starts behind header+lacing
            uint8_t* newPage = m_bitReader.headptr + header_consumed;
            bitReader_setData(newPage, m_vorbis_segment_length);

            // If remaining bits exist → append to the beginning of the new page
            if (bits_left > 0) {
                m_bitReader.data[0] = (m_bitReader.data[0] << bits_left) | leftover;
                m_bitReader.headbit = bits_left;
            } else {
                m_bitReader.headbit = 0;
            }

            m_bitReader.headptr = m_bitReader.data;
            m_bitReader.headend = m_vorbis_segment_length;

            VORBIS_LOG_INFO("BitReader: crossed Ogg boundary (bits_left=%u, new_segment=%u)", bits_left, m_vorbis_segment_length);
            continue;
        }

        // ------------------------------------------------------------
        // 🧩 Normal reading of bits from the current byte
        // ------------------------------------------------------------
        uint8_t  current_byte = *m_bitReader.headptr;
        uint32_t bits_avail = 8 - m_bitReader.headbit;
        uint32_t take = std::min<uint32_t>(bits_avail, nBits);

        uint32_t mask = ((1u << take) - 1);
        val |= ((current_byte >> m_bitReader.headbit) & mask) << bits_collected;

        bits_collected += take;
        nBits -= take;
        m_bitReader.headbit += take;

        // Byte vollständig gelesen?
        if (m_bitReader.headbit >= 8) {
            m_bitReader.headbit = 0;
            m_bitReader.headptr++;
            m_bitReader.headend--;
        }
    }

    if (m_bitReader.headend <= 0) { VORBIS_LOG_WARN("bitReader nearly empty: %d bytes left", m_bitReader.headend); }

    return val;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int8_t VorbisDecoder::bitReader_adv(uint16_t nBits) {
    // new bit position in the current byte
    uint32_t newBit = m_bitReader.headbit + nBits;

    // Calculate how many whole bytes we skip
    uint32_t byteAdvance = newBit >> 3;

    // Remaining bits in the current byte
    m_bitReader.headbit = newBit & 7;

    // Check if we would go past the end
    if (byteAdvance > (uint32_t)m_bitReader.headend || (byteAdvance == (uint32_t)m_bitReader.headend && m_bitReader.headbit > 0)) {
        VORBIS_LOG_ERROR("bitReader_adv: ran out of data (advance=%u, left=%i)", byteAdvance, m_bitReader.headend);
        return -1;
    }

    // Continue byte pointer
    m_bitReader.headptr += byteAdvance;
    m_bitReader.headend -= byteAdvance;

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::ilog(uint32_t v) {
    int32_t ret = 0;
    if (v) --v;
    while (v) {
        ret++;
        v >>= 1;
    }
    return (ret);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t VorbisDecoder::_ilog(uint32_t v) {
    uint8_t ret = 0;
    while (v) {
        ret++;
        v >>= 1;
    }
    return (ret);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* 32 bit float (not IEEE; nonnormalized mantissa + biased exponent) : neeeeeee eeemmmmm mmmmmmmm mmmmmmmm
 Why not IEEE?  It's just not that important here. */

int32_t VorbisDecoder::_float32_unpack(int32_t val, int32_t* point) {
    int32_t mant = val & 0x1fffff;
    bool    sign = val < 0;

    *point = ((val & 0x7fe00000L) >> 21) - 788;

    if (mant) {
        while (!(mant & 0x40000000)) {
            mant <<= 1;
            *point -= 1;
        }
        if (sign) mant = -mant;
    } else {
        *point = -9999;
    }
    return mant;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* choose the smallest supported node size that fits our decode table. Legal bytewidths are 1/1 1/2 2/2 2/4 4/4 */
int32_t VorbisDecoder::_determine_node_bytes(uint32_t used, uint8_t leafwidth) {
    /* special case small books to size 4 to avoid multiple special cases in repack */
    if (used < 2) return 4;

    if (leafwidth == 3) leafwidth = 4;
    if (_ilog((3 * used - 6)) + 1 <= leafwidth * 4) return leafwidth / 2 ? leafwidth / 2 : 1;
    return leafwidth;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* convenience/clarity; leaves are specified as multiple of node word size (1 or 2) */
int32_t VorbisDecoder::_determine_leaf_words(int32_t nodeb, int32_t leafwidth) {
    if (leafwidth > nodeb) return 2;
    return 1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::_make_decode_table(codebook_t* s, int32_t* lengthlist, uint8_t quantvals, int32_t maptype) {
    ps_ptr<uint32_t> work;

    if (s->dec_nodeb == 4) {
        s->dec_table.alloc((s->used_entries + 1) * sizeof(*work), "dec_table");
        /* +1 (rather than -2) is to accommodate 0 and 1 sized books, which are specialcased to nodeb==4 */
        if (_make_words(lengthlist, s->entries, (uint32_t*)s->dec_table.get(), quantvals, s, maptype)) return 1;

        return 0;
    }

    work.alloc((s->used_entries * 2) * sizeof(int32_t), "work");
    work.clear();

    if (_make_words(lengthlist, s->entries, work.get(), quantvals, s, maptype)) { return 1; }
    s->dec_table.alloc((s->used_entries * (s->dec_leafw + 1) - 2) * s->dec_nodeb, "dec_table");
    if (s->dec_leafw == 1) {
        switch (s->dec_nodeb) {
            case 1:
                for (uint32_t i = 0; i < s->used_entries * 2 - 2; i++) ((uint8_t*)s->dec_table.get())[i] = (uint16_t)((work[i] & 0x80000000UL) >> 24) | work[i];
                break;
            case 2:
                for (uint32_t i = 0; i < s->used_entries * 2 - 2; i++) ((uint16_t*)s->dec_table.get())[i] = (uint16_t)((work[i] & 0x80000000UL) >> 16) | work[i];
                break;
        }
    } else {
        /* more complex; we have to do a two-pass repack that updates the node indexing. */
        uint32_t top = s->used_entries * 3 - 2;
        if (s->dec_nodeb == 1) {
            uint8_t* out = (uint8_t*)s->dec_table.get();

            for (int32_t i = s->used_entries * 2 - 4; i >= 0; i -= 2) {
                if (work[i] & 0x80000000UL) {
                    if (work[i + 1] & 0x80000000UL) {
                        top -= 4;
                        out[top] = (uint8_t)(work[i] >> 8 & 0x7f) | 0x80;
                        out[top + 1] = (uint8_t)(work[i + 1] >> 8 & 0x7f) | 0x80;
                        out[top + 2] = (uint8_t)work[i] & 0xff;
                        out[top + 3] = (uint8_t)work[i + 1] & 0xff;
                    } else {
                        top -= 3;
                        out[top] = (uint8_t)(work[i] >> 8 & 0x7f) | 0x80;
                        out[top + 1] = (uint8_t)work[work[i + 1] * 2];
                        out[top + 2] = (uint8_t)work[i] & 0xff;
                    }
                } else {
                    if (work[i + 1] & 0x80000000UL) {
                        top -= 3;
                        out[top] = (uint8_t)work[work[i] * 2];
                        out[top + 1] = (uint8_t)(work[i + 1] >> 8 & 0x7f) | 0x80;
                        out[top + 2] = (uint8_t)work[i + 1] & 0xff;
                    } else {
                        top -= 2;
                        out[top] = (uint8_t)work[work[i] * 2];
                        out[top + 1] = (uint8_t)work[work[i + 1] * 2];
                    }
                }
                work[i] = top;
            }
        } else {
            uint16_t* out = (uint16_t*)s->dec_table.get();
            for (int32_t i = s->used_entries * 2 - 4; i >= 0; i -= 2) {
                if (work[i] & 0x80000000UL) {
                    if (work[i + 1] & 0x80000000UL) {
                        top -= 4;
                        out[top] = (uint16_t)(work[i] >> 16 & 0x7fff) | 0x8000;
                        out[top + 1] = (uint16_t)(work[i + 1] >> 16 & 0x7fff) | 0x8000;
                        out[top + 2] = (uint16_t)work[i] & 0xffff;
                        out[top + 3] = (uint16_t)work[i + 1] & 0xffff;
                    } else {
                        top -= 3;
                        out[top] = (uint16_t)(work[i] >> 16 & 0x7fff) | 0x8000;
                        out[top + 1] = (uint16_t)work[work[i + 1] * 2];
                        out[top + 2] = (uint16_t)work[i] & 0xffff;
                    }
                } else {
                    if (work[i + 1] & 0x80000000UL) {
                        top -= 3;
                        out[top] = (uint16_t)work[work[i] * 2];
                        out[top + 1] = (uint16_t)(work[i + 1] >> 16 & 0x7fff) | 0x8000;
                        out[top + 2] = (uint16_t)work[i + 1] & 0xffff;
                    } else {
                        top -= 2;
                        out[top] = (uint16_t)work[work[i] * 2];
                        out[top + 1] = (uint16_t)work[work[i + 1] * 2];
                    }
                }
                work[i] = (uint32_t)top;
            }
        }
    }
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* given a list of word lengths, number of used entries, and byte width of a leaf, generate the decode table */
int32_t VorbisDecoder::_make_words(int32_t* l, uint16_t n, uint32_t* work, uint8_t quantvals, codebook_t* b, int32_t maptype) {

    int32_t  i, j, count = 0;
    uint32_t top = 0;
    uint32_t marker[33];

    if (n < 2) {
        work[0] = 0x80000000;
    } else {
        memset(marker, 0, sizeof(marker));

        for (i = 0; i < n; i++) {
            int32_t length = l[i];
            if (length) {
                uint32_t entry = marker[length];
                uint32_t chase = 0;
                if (count && !entry) return -1; /* overpopulated tree! */

                /* chase the tree as far as it's already populated, fill in past */
                for (j = 0; j < length - 1; j++) {
                    uint32_t bit = (entry >> (length - j - 1)) & 1;
                    if (chase >= top) {
                        top++;
                        work[chase * 2] = top;
                        work[chase * 2 + 1] = 0;
                    } else if (!work[chase * 2 + bit]) {
                        work[chase * 2 + bit] = top;
                    }
                    chase = work[chase * 2 + bit];
                }
                {
                    int32_t bit = (entry >> (length - j - 1)) & 1;
                    if (chase >= top) {
                        top++;
                        work[chase * 2 + 1] = 0;
                    }
                    work[chase * 2 + bit] = decpack(i, count++, quantvals, b, maptype) | 0x80000000;
                }

                /* Look to see if the next shorter marker points to the node above. if so, update it and repeat.  */
                for (j = length; j > 0; j--) {
                    if (marker[j] & 1) {
                        marker[j] = marker[j - 1] << 1;
                        break;
                    }
                    marker[j]++;
                }

                /* prune the tree; the implicit invariant says all the int32_ter markers were dangling from our
                   just-taken node. Dangle them from our *new* node. */
                for (j = length + 1; j < 33; j++)
                    if ((marker[j] >> 1) == entry) {
                        entry = marker[j];
                        marker[j] = marker[j - 1] << 1;
                    } else
                        break;
            }
        }
    }

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t VorbisDecoder::decpack(int32_t entry, int32_t used_entry, uint8_t quantvals, codebook_t* b, int32_t maptype) {
    uint32_t ret = 0;

    switch (b->dec_type) {
        case 0: return (uint32_t)entry;

        case 1:
            if (maptype == 1) {
                /* vals are already read into temporary column vector here */
                assert(b->dim >= 0);
                for (uint8_t j = 0; j < b->dim; j++) {
                    uint32_t off = (uint32_t)(entry % quantvals);
                    entry /= quantvals;
                    assert((b->q_bits * j) >= 0);
                    uint32_t shift = (uint32_t)b->q_bits * j;
                    ret |= ((uint16_t*)(b->q_val.get()))[off] << shift;
                }
            } else {
                assert(b->dim >= 0);
                for (uint8_t j = 0; j < b->dim; j++) {
                    assert((b->q_bits * j) >= 0);
                    uint32_t shift = (uint32_t)b->q_bits * j;
                    int32_t  _ret = bitReader(b->q_bits) << shift;
                    assert(_ret >= 0);
                    ret |= (uint32_t)_ret;
                }
            }
            return ret;

        case 2:
            assert(b->dim >= 0);
            for (uint8_t j = 0; j < b->dim; j++) {
                uint32_t off = uint32_t(entry % quantvals);
                entry /= quantvals;
                assert(b->q_pack * j >= 0);
                assert(b->q_pack * j <= 255);
                ret |= off << (uint8_t)(b->q_pack * j);
            }
            return ret;

        case 3: return (uint32_t)used_entry;
    }
    return 0; /* silence compiler */
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* most of the time, entries%dimensions == 0, but we need to be well defined.  We define that the possible vales at each scalar is values == entries/dim.  If entries%dim != 0, we'll have 'too few'
 values (values*dim<entries), which means that we'll have 'left over' entries; left over entries use zeroed values (and are wasted).  So don't generate codebooks like that */
/* there might be a straightforward one-line way to do the below that's portable and totally safe against roundoff, but I haven't thought of it.  Therefore, we opt on the side of caution */
uint8_t VorbisDecoder::_book_maptype1_quantvals(codebook_t* b) {
    /* get us a starting hint, we'll polish it below */
    uint8_t bits = _ilog(b->entries);
    uint8_t vals = b->entries >> ((bits - 1) * (b->dim - 1) / b->dim);

    while (1) {
        uint32_t acc = 1;
        uint32_t acc1 = 1;

        for (uint8_t i = 0; i < b->dim; i++) {
            acc *= vals;
            acc1 *= vals + 1;
        }
        if (acc <= b->entries && acc1 > b->entries) {
            return (vals);
        } else {
            if (acc > b->entries) {
                vals--;
            } else {
                vals++;
            }
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::oggpack_eop() {
    if (m_bitReader.headptr - m_bitReader.data > m_bitReader.length) {
        VORBIS_LOG_INFO("s_bitReader.headptr - data %i, bitReader.length %i", m_bitReader.headptr - m_bitReader.data, m_bitReader.length);
        return -1;
    }
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
ps_ptr<VorbisDecoder::vorbis_info_floor> VorbisDecoder::floor0_info_unpack() {

    int32_t j;

    ps_ptr<vorbis_info_floor> info;
    info.calloc_array(1, "info");

    info->order = bitReader(8);
    info->rate = bitReader(16);
    info->barkmap = bitReader(16);
    info->ampbits = bitReader(6);
    info->ampdB = bitReader(8);
    info->numbooks = bitReader(4) + 1;

    if (info->order < 1) goto err_out;
    if (info->rate < 1) goto err_out;
    if (info->barkmap < 1) goto err_out;

    for (j = 0; j < info->numbooks; j++) {
        info->books[j] = bitReader(8);
        if (info->books[j] >= m_nrOfCodebooks) goto err_out;
    }

    if (oggpack_eop()) goto err_out;
    return (info);

err_out:
    return {};
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
ps_ptr<VorbisDecoder::vorbis_info_floor> VorbisDecoder::floor1_info_unpack() {

    ps_ptr<uint8_t> B;
    int32_t         j, k, count = 0, maxclass = -1, rangebits;

    ps_ptr<vorbis_info_floor> info;
    info.calloc_array(1, "info");

    /* read partitions */
    info->partitions = bitReader(5); /* only 0 to 31 legal */
    info->partitionclass.alloc(info->partitions * sizeof(uint8_t), "partitionclass");
    for (j = 0; j < info->partitions; j++) {
        info->partitionclass[j] = bitReader(4); /* only 0 to 15 legal */
        if (maxclass < info->partitionclass[j]) maxclass = info->partitionclass[j];
    }

    /* read partition classes */
    info->_class.alloc((uint32_t)(maxclass + 1) * sizeof(floor1class_t), "_class");
    for (j = 0; j < maxclass + 1; j++) {
        info->_class[j].class_dim = bitReader(3) + 1; /* 1 to 8 */
        info->_class[j].class_subs = bitReader(2);    /* 0,1,2,3 bits */
        if (oggpack_eop() < 0) goto err_out;
        if (info->_class[j].class_subs) {
            info->_class[j].class_book = bitReader(8);
        } else {
            info->_class[j].class_book = 0;
        }
        if (info->_class[j].class_book >= m_nrOfCodebooks) goto err_out;
        for (k = 0; k < (1 << info->_class[j].class_subs); k++) {
            info->_class[j].class_subbook[k] = (uint8_t)bitReader(8) - 1;
            if (info->_class[j].class_subbook[k] >= m_nrOfCodebooks && info->_class[j].class_subbook[k] != 0xff) goto err_out;
        }
    }

    /* read the post list */
    info->mult = bitReader(2) + 1; /* only 1,2,3,4 legal now */
    rangebits = bitReader(4);

    for (j = 0, k = 0; j < info->partitions; j++) count += info->_class[info->partitionclass[j]].class_dim;
    info->postlist.alloc((count + 2) * sizeof(uint16_t), "postlist");
    info->forward_index.alloc((count + 2) * sizeof(uint8_t), "forward_index");
    info->loneighbor.alloc(count * sizeof(uint8_t), "loneighbor");
    info->hineighbor.alloc(count * sizeof(uint8_t), "hineighbor");

    count = 0;
    for (j = 0, k = 0; j < info->partitions; j++) {
        count += info->_class[info->partitionclass[j]].class_dim;
        if (count > VIF_POSIT) goto err_out;
        for (; k < count; k++) {
            int32_t t = info->postlist[k + 2] = bitReader(rangebits);
            if (t >= (1 << rangebits)) goto err_out;
        }
    }
    if (oggpack_eop()) goto err_out;
    info->postlist[0] = 0;
    info->postlist[1] = 1 << rangebits;
    info->posts = count + 2;

    /* also store a sorted position index */
    for (j = 0; j < info->posts; j++) info->forward_index[j] = j;

    // vorbis_mergesort--------------------------------------------------------------------------------
    vorbis_mergesort(info->forward_index.get(), info->postlist.get(), info->posts);
    //-------------------------------------------------------------------------------------------------
    /* discover our neighbors for decode where we don't use fit flags (that would push the neighbors outward) */
    for (j = 0; j < info->posts - 2; j++) {
        int32_t lo = 0;
        int32_t hi = 1;
        int32_t lx = 0;
        int32_t hx = info->postlist[1];
        int32_t currentx = info->postlist[j + 2];
        for (k = 0; k < j + 2; k++) {
            int32_t x = info->postlist[k];
            if (x > lx && x < currentx) {
                lo = k;
                lx = x;
            }
            if (x < hx && x > currentx) {
                hi = k;
                hx = x;
            }
        }
        info->loneighbor[j] = lo;
        info->hineighbor[j] = hi;
    }

    return (info);

err_out:
    return {};
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::vorbis_mergesort(uint8_t* index, uint16_t* vals, uint16_t n) {
    uint16_t        i, j;
    ps_ptr<uint8_t> B_mem;
    B_mem.alloc(n * sizeof(uint8_t));

    if (!B_mem.valid()) return;

    uint8_t* A = index;
    uint8_t* B = B_mem.get();
    bool     flipped = false;

    for (i = 1; i < n; i <<= 1) {
        for (j = 0; j + i < n;) {
            uint16_t k1 = j;
            uint16_t mid = j + i;
            uint16_t k2 = mid;
            uint16_t end = (j + i * 2 < n ? j + i * 2 : n);
            while (k1 < mid && k2 < end) {
                if (vals[A[k1]] < vals[A[k2]])
                    B[j++] = A[k1++];
                else
                    B[j++] = A[k2++];
            }
            while (k1 < mid) B[j++] = A[k1++];
            while (k2 < end) B[j++] = A[k2++];
        }

        for (; j < n; j++) B[j] = A[j];

        std::swap(A, B); // swap pointer
        flipped = !flipped;
    }

    if (A != index) {
        std::memcpy(index, A, n); // result back to index
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* vorbis_info is for range checking */
int32_t VorbisDecoder::res_unpack(vorbis_info_residue* info) {
    int32_t j, k;

    info->type = bitReader(16);
    if (info->type > 2 || info->type < 0) goto errout;
    info->begin = bitReader(24);
    info->end = bitReader(24);
    info->grouping = bitReader(24) + 1;
    info->partitions = bitReader(6) + 1;
    info->groupbook = bitReader(8);
    if (info->groupbook >= m_nrOfCodebooks) goto errout;

    info->stagemasks.alloc(info->partitions * sizeof(uint8_t));
    info->stagebooks.alloc(info->partitions * 8 * sizeof(uint8_t));

    for (j = 0; j < info->partitions; j++) {
        int32_t cascade = bitReader(3);
        if (bitReader(1)) cascade |= (bitReader(5) << 3);
        info->stagemasks[j] = cascade;
    }

    for (j = 0; j < info->partitions; j++) {
        for (k = 0; k < 8; k++) {
            if ((info->stagemasks[j] >> k) & 1) {
                uint8_t book = bitReader(8);
                if (book >= m_nrOfCodebooks) goto errout;
                info->stagebooks[j * 8 + k] = book;
                if (k + 1 > info->stages) info->stages = k + 1;
            } else
                info->stagebooks[j * 8 + k] = 0xff;
        }
    }

    if (oggpack_eop()) goto errout;

    return 0;
errout:
    return 1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* also responsible for range checking */
int32_t VorbisDecoder::mapping_info_unpack(vorbis_info_mapping* info) {
    int32_t i;

    if (bitReader(1))
        info->submaps = bitReader(4) + 1;
    else
        info->submaps = 1;

    if (bitReader(1)) {
        info->coupling_steps = bitReader(8) + 1;
        info->coupling.alloc(info->coupling_steps * sizeof(coupling_step_t));

        for (i = 0; i < info->coupling_steps; i++) {
            int32_t testM = info->coupling[i].mag = bitReader(ilog(m_vorbisChannels));
            int32_t testA = info->coupling[i].ang = bitReader(ilog(m_vorbisChannels));

            if (testM < 0 || testA < 0 || testM == testA || testM >= m_vorbisChannels || testA >= m_vorbisChannels) goto err_out;
        }
    }

    if (bitReader(2) > 0) goto err_out;
    /* 2,3:reserved */

    if (info->submaps > 1) {
        info->chmuxlist.alloc(sizeof(uint8_t) * m_vorbisChannels);
        for (i = 0; i < m_vorbisChannels; i++) {
            info->chmuxlist[i] = bitReader(4);
            if (info->chmuxlist[i] >= info->submaps) goto err_out;
        }
    }

    info->submaplist.alloc(sizeof(submap_t) * info->submaps);
    for (i = 0; i < info->submaps; i++) {
        int32_t temp = bitReader(8);
        (void)temp;
        info->submaplist[i].floor = bitReader(8);
        if (info->submaplist[i].floor >= m_nrOfFloors) goto err_out;
        info->submaplist[i].residue = bitReader(8);
        if (info->submaplist[i].residue >= m_nrOfResidues) goto err_out;
    }

    return 0;

err_out:
    return -1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//      ⏫⏫⏫    O G G      I M P L     A B O V E  ⏫⏫⏫
//      ⏬⏬⏬ V O R B I S   I M P L     B E L O W  ⏬⏬⏬
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
ps_ptr<VorbisDecoder::vorbis_dsp_state> VorbisDecoder::vorbis_dsp_create() {
    ps_ptr<vorbis_dsp_state> v;
    v.calloc_array(1);
    v->work.calloc_array(m_vorbisChannels);
    v->mdctright.calloc_array(m_vorbisChannels);

    for (uint8_t i = 0; i < m_vorbisChannels; ++i) {
        const size_t work_size = (m_blocksizes[1] >> 1) * sizeof(int32_t);
        v->work.at(i).calloc(work_size);

        const size_t mdct_size = (m_blocksizes[1] >> 2) * sizeof(int32_t);
        v->mdctright.at(i).calloc(mdct_size);
    }

    // Initialize state
    v->lW = 0;
    v->W = 0;
    v->out_begin = -1;
    v->out_end = -1;

    return v;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::vorbis_dsp_destroy(ps_ptr<vorbis_dsp_state>& v) {
    if (!v.valid()) return;

    for (uint8_t i = 0; i < m_vorbisChannels; ++i) {
        v->work.at(i).reset();
        v->mdctright.at(i).reset();
    }
    v->mdctright.reset();
    v->work.reset();
    v.reset();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::vorbis_book_clear(ps_ptr<codebook_t>& v) {
    if (!v.valid()) return;
    int s = 0;
    for (int i = 0; i < m_nrOfCodebooks; i++) {
        if (v[i].q_val.valid()) {
            s += v[i].q_val.size();
            v[i].q_val.reset();
        }
        if (v[i].dec_table.valid()) {
            s += v[i].dec_table.size();
            v[i].dec_table.reset();
        }
    }
    if (v->dec_table.valid()) {
        s += v->dec_table.size();
        v->dec_table.reset();
    }
    if (v->q_val.valid()) {
        s += v->q_val.size();
        v->q_val.reset();
    }
    s += v.size();
    v.reset();
    m_nrOfCodebooks = 0;
    // VORBIS_LOG_INFO("free codebook_t %i bytes", s);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::vorbis_dsp_synthesis(uint8_t* inbuf, uint16_t len, int16_t* outbuf) {

    int32_t mode, i;

    /* Check the packet type */
    if (bitReader(1) != 0) {
        /* Oops.  This is not an audio data packet */
        return OV_ENOTAUDIO;
    }

    /* read our mode and pre/post windowsize */
    mode = bitReader(ilog(m_nrOfModes));
    if (mode == -1 || mode >= m_nrOfModes) return OV_EBADPACKET;

    /* shift information we still need from last window */
    m_dsp_state->lW = m_dsp_state->W;
    m_dsp_state->W = m_mode_param[mode].blockflag;
    for (i = 0; i < m_vorbisChannels; i++) { mdct_shift_right(m_blocksizes[m_dsp_state->lW], m_dsp_state->work[i].get(), m_dsp_state->mdctright[i].get()); }
    if (m_dsp_state->W) {
        int32_t temp;
        bitReader(1);
        temp = bitReader(1);
        if (temp == -1) return OV_EBADPACKET;
    }

    /* packet decode and portions of synthesis that rely on only this block */
    {
        mapping_inverse(m_map_param.get() + m_mode_param[mode].mapping);

        if (m_dsp_state->out_begin == -1) {
            m_dsp_state->out_begin = 0;
            m_dsp_state->out_end = 0;
        } else {
            m_dsp_state->out_begin = 0;
            m_dsp_state->out_end = m_blocksizes[m_dsp_state->lW] / 4 + m_blocksizes[m_dsp_state->W] / 4;
        }
    }

    return (0);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::mdct_shift_right(int32_t n, int32_t* in, int32_t* right) {
    int32_t i;
    n >>= 2;
    in += 1;

    for (i = 0; i < n; i++) right[i] = in[i << 1];
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::mapping_inverse(vorbis_info_mapping* info) {

    int32_t i, j;
    int32_t n = m_blocksizes[m_dsp_state->W];

    ps_ptr<int32_t*> pcmbundle;
    pcmbundle.calloc_array(m_vorbisChannels);
    ps_ptr<int32_t> zerobundle;
    zerobundle.alloc(sizeof(int32_t) * m_vorbisChannels);
    ps_ptr<int32_t> nonzero;
    nonzero.alloc(sizeof(int32_t) * m_vorbisChannels);
    ps_ptr<ps_ptr<int32_t>> floormemo;
    floormemo.calloc_array(m_vorbisChannels);

    /* recover the spectral envelope; store it in the PCM vector for now */
    for (i = 0; i < m_vorbisChannels; i++) {

        int32_t submap = 0;
        int32_t floorno;

        if (info->submaps > 1) submap = info->chmuxlist[i];
        floorno = info->submaplist[submap].floor;

        if (m_floor_type[floorno]) {
            /* floor 1 */
            floormemo[i].alloc(sizeof(*floormemo[i]) * floor1_memosize(m_floor_param[floorno]));
            floormemo[i] = floor1_inverse1(m_floor_param[floorno], floormemo[i].get());
        } else {
            /* floor 0 */
            floormemo[i].alloc(sizeof(*floormemo[i]) * floor0_memosize(m_floor_param[floorno]));
            floormemo[i] = floor0_inverse1(m_floor_param[floorno], floormemo[i].get());
        }

        if (floormemo[i].get())
            nonzero[i] = 1;
        else
            nonzero[i] = 0;
        memset(m_dsp_state->work[i].get(), 0, sizeof(*m_dsp_state->work[i]) * n / 2);
    }

    /* channel coupling can 'dirty' the nonzero listing */
    for (i = 0; i < info->coupling_steps; i++) {
        if (nonzero[info->coupling[i].mag] || nonzero[info->coupling[i].ang]) {
            nonzero[info->coupling[i].mag] = 1;
            nonzero[info->coupling[i].ang] = 1;
        }
    }

    /* recover the residue into our working vectors */
    for (i = 0; i < info->submaps; i++) {
        uint8_t ch_in_bundle = 0;
        for (j = 0; j < m_vorbisChannels; j++) {
            if (!info->chmuxlist.get() || info->chmuxlist[j] == i) {
                if (nonzero[j])
                    zerobundle[ch_in_bundle] = 1;
                else
                    zerobundle[ch_in_bundle] = 0;
                pcmbundle[ch_in_bundle++] = m_dsp_state->work[j].get();
            }
        }
        res_inverse(m_residue_param.get() + info->submaplist[i].residue, pcmbundle.get(), zerobundle.get(), ch_in_bundle);
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("coupled",seq+j,vb->pcm[j],-8,n/2,0,0);

    /* channel coupling */
    for (i = info->coupling_steps - 1; i >= 0; i--) {
        int32_t* pcmM = m_dsp_state->work[info->coupling[i].mag].get();
        int32_t* pcmA = m_dsp_state->work[info->coupling[i].ang].get();

        for (j = 0; j < n / 2; j++) {
            int32_t mag = pcmM[j];
            int32_t ang = pcmA[j];

            if (mag > 0)
                if (ang > 0) {
                    pcmM[j] = mag;
                    pcmA[j] = mag - ang;
                } else {
                    pcmA[j] = mag;
                    pcmM[j] = mag + ang;
                }
            else if (ang > 0) {
                pcmM[j] = mag;
                pcmA[j] = mag + ang;
            } else {
                pcmA[j] = mag;
                pcmM[j] = mag - ang;
            }
        }
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("residue",seq+j,vb->pcm[j],-8,n/2,0,0);

    /* compute and apply spectral envelope */

    for (i = 0; i < m_vorbisChannels; i++) {
        int32_t* pcm = m_dsp_state->work[i].get();
        int32_t  submap = 0;
        int32_t  floorno;

        if (info->submaps > 1) submap = info->chmuxlist[i];
        floorno = info->submaplist[submap].floor;

        if (m_floor_type[floorno]) {
            /* floor 1 */
            floor1_inverse2(m_floor_param[floorno], floormemo[i].get(), pcm);
        } else {
            /* floor 0 */
            floor0_inverse2(m_floor_param[floorno], floormemo[i].get(), pcm);
        }
        floormemo[i].reset();
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("mdct",seq+j,vb->pcm[j],-24,n/2,0,1);

    /* transform the PCM data; takes PCM vector, vb; modifies PCM vector */
    /* only MDCT right now.... */
    for (i = 0; i < m_vorbisChannels; i++) { mdct_backward(n, m_dsp_state->work[i].get()); }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("imdct",seq+j,vb->pcm[j],-24,n,0,0);

    /* all done! */
    return (0);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::floor0_memosize(ps_ptr<vorbis_info_floor>& i) {
    return i.get()->order + 1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::floor1_memosize(ps_ptr<vorbis_info_floor>& i) {
    return i.get()->posts;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t* VorbisDecoder::floor0_inverse1(ps_ptr<vorbis_info_floor>& i, int32_t* lsp) {
    vorbis_info_floor* info = (vorbis_info_floor*)i.get();
    int32_t            j;

    int32_t ampraw = bitReader(info->ampbits);

    if (ampraw > 0) { /* also handles the -1 out of data case */
        int32_t maxval = (1 << info->ampbits) - 1;
        int32_t amp = ((ampraw * info->ampdB) << 4) / maxval;
        int32_t booknum = bitReader(_ilog(info->numbooks));

        if (booknum != -1 && booknum < info->numbooks) { /* be paranoid */
            codebook_t* b = m_codebooks.get() + info->books[booknum];
            int32_t     last = 0;

            if (vorbis_book_decodev_set(b, lsp, info->order, -24) == -1) goto eop;
            for (j = 0; j < info->order;) {
                for (uint8_t k = 0; j < info->order && k < b->dim; k++, j++) lsp[j] += last;
                last = lsp[j - 1];
            }

            lsp[info->order] = amp;
            return (lsp);
        }
    }
eop:
    return (NULL);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t* VorbisDecoder::floor1_inverse1(ps_ptr<vorbis_info_floor>& in, int32_t* fit_value) {
    vorbis_info_floor* info = (vorbis_info_floor*)in.get();

    int32_t     quant_look[4] = {256, 128, 86, 64};
    int32_t     i, j, k;
    int32_t     quant_q = quant_look[info->mult - 1];
    codebook_t* books = m_codebooks.get();

    /* unpack wrapped/predicted values from stream */
    if (bitReader(1) == 1) {
        fit_value[0] = bitReader(ilog(quant_q - 1));
        fit_value[1] = bitReader(ilog(quant_q - 1));

        /* partition by partition */
        for (i = 0, j = 2; i < info->partitions; i++) {
            int32_t classv = info->partitionclass[i];
            int32_t cdim = info->_class[classv].class_dim;
            int32_t csubbits = info->_class[classv].class_subs;
            int32_t csub = 1 << csubbits;
            int32_t cval = 0;

            /* decode the partition's first stage cascade value */
            if (csubbits) {
                cval = vorbis_book_decode(books + info->_class[classv].class_book);
                if (cval == -1) goto eop;
            }

            for (k = 0; k < cdim; k++) {
                int32_t book = info->_class[classv].class_subbook[cval & (csub - 1)];
                cval >>= csubbits;
                if (book != 0xff) {
                    if ((fit_value[j + k] = vorbis_book_decode(books + book)) == -1) goto eop;
                } else {
                    fit_value[j + k] = 0;
                }
            }
            j += cdim;
        }

        /* unwrap positive values and reconsitute via linear interpolation */
        for (i = 2; i < info->posts; i++) {
            int32_t predicted = render_point(info->postlist[info->loneighbor[i - 2]], info->postlist[info->hineighbor[i - 2]], fit_value[info->loneighbor[i - 2]], fit_value[info->hineighbor[i - 2]],
                                             info->postlist[i]);
            int32_t hiroom = quant_q - predicted;
            int32_t loroom = predicted;
            int32_t room = (hiroom < loroom ? hiroom : loroom) << 1;
            int32_t val = fit_value[i];

            if (val) {
                if (val >= room) {
                    if (hiroom > loroom) {
                        val = val - loroom;
                    } else {
                        val = -1 - (val - hiroom);
                    }
                } else {
                    if (val & 1) {
                        val = -((val + 1) >> 1);
                    } else {
                        val >>= 1;
                    }
                }

                fit_value[i] = val + predicted;
                fit_value[info->loneighbor[i - 2]] &= 0x7fff;
                fit_value[info->hineighbor[i - 2]] &= 0x7fff;
            } else {
                fit_value[i] = predicted | 0x8000;
            }
        }

        return (fit_value);
    } else {
        // VORBIS_LOG_ERROR("err in br");
        ;
    }
eop:
    return (NULL);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* returns the [original, not compacted] entry number or -1 on eof *********/
int32_t VorbisDecoder::vorbis_book_decode(codebook_t* book) {
    if (book->dec_type) return -1;
    return decode_packed_entry_number(book);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::decode_packed_entry_number(codebook_t* book) {
    uint32_t chase = 0;
    int32_t  read = book->dec_maxlength;
    int32_t  lok = bitReader_look(read), i;

    while (lok < 0 && read > 1) { lok = bitReader_look(--read); }

    if (lok < 0) {
        bitReader_adv(1); /* force eop */
        return -1;
    }

    /* chase the tree with the bits we got */
    if (book->dec_nodeb == 1) {
        if (book->dec_leafw == 1) {
            /* 8/8 */
            uint8_t* t = (uint8_t*)book->dec_table.get();
            for (i = 0; i < read; i++) {
                chase = t[chase * 2 + ((lok >> i) & 1)];
                if (chase & 0x80UL) break;
            }
            chase &= 0x7fUL;
        } else {
            /* 8/16 */
            uint8_t* t = (uint8_t*)book->dec_table.get();
            for (i = 0; i < read; i++) {
                int32_t bit = (lok >> i) & 1;
                int32_t next = t[chase + bit];
                if (next & 0x80) {
                    chase = (next << 8) | t[chase + bit + 1 + (!bit || (t[chase] & 0x80))];
                    break;
                }
                chase = next;
            }
            chase &= 0x7fffUL;
        }
    } else {
        if (book->dec_nodeb == 2) {
            if (book->dec_leafw == 1) {
                /* 16/16 */
                int32_t idx;
                for (i = 0; i < read; i++) {
                    idx = chase * 2 + ((lok >> i) & 1);
                    chase = ((uint16_t*)(book->dec_table.get()))[idx];
                    if (chase & 0x8000UL) { break; }
                }
                chase &= 0x7fffUL;
            } else {
                /* 16/32 */
                uint16_t* t = (uint16_t*)book->dec_table.get();
                for (i = 0; i < read; i++) {
                    int32_t bit = (lok >> i) & 1;
                    int32_t next = t[chase + bit];
                    if (next & 0x8000) {
                        chase = (next << 16) | t[chase + bit + 1 + (!bit || (t[chase] & 0x8000))];
                        break;
                    }
                    chase = next;
                }
                chase &= 0x7fffffffUL;
            }
        } else {
            for (i = 0; i < read; i++) {
                chase = ((uint32_t*)(book->dec_table.get()))[chase * 2 + ((lok >> i) & 1)];
                if (chase & 0x80000000UL) break;
            }
            chase &= 0x7fffffffUL;
        }
    }

    if (i < read) {
        bitReader_adv(i + 1);
        return chase;
    }
    bitReader_adv(read + 1);
    VORBIS_LOG_ERROR("read %i", read);
    return (VORBIS_ERR);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::render_point(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t x) {
    y0 &= 0x7fff; /* mask off flag */
    y1 &= 0x7fff;

    {
        int32_t dy = y1 - y0;
        int32_t adx = x1 - x0;
        int32_t ady = abs(dy);
        int32_t err = ady * (x - x0);

        int32_t off = err / adx;
        if (dy < 0) return (y0 - off);
        return (y0 + off);
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* unlike the others, we guard against n not being an integer number * of <dim> internally rather than in the upper
 layer (called only by * floor0) */
int32_t VorbisDecoder::vorbis_book_decodev_set(codebook_t* book, int32_t* a, int32_t n, int32_t point) {
    if (book->used_entries > 0) {
        int32_t* v = (int32_t*)alloca(sizeof(*v) * book->dim);
        int32_t  i;

        for (i = 0; i < n;) {
            if (decode_map(book, v, point)) return -1;
            for (uint8_t j = 0; i < n && j < book->dim; j++) a[i++] = v[j];
        }
    } else {
        int32_t i;

        for (i = 0; i < n;) { a[i++] = 0; }
    }

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::decode_map(codebook_t* s, int32_t* v, int32_t point) {

    uint32_t entry = decode_packed_entry_number(s);

    // if (oggpack_eop()) {return (-1);}

    /* according to decode type */
    switch (s->dec_type) {
        case 1: {
            /* packed vector of values */
            int32_t mask = (1 << s->q_bits) - 1;
            for (uint8_t i = 0; i < s->dim; i++) {
                v[i] = entry & mask;
                entry >>= s->q_bits;
            }
            break;
        }
        case 2: {
            /* packed vector of column offsets */
            int32_t mask = (1 << s->q_pack) - 1;
            for (uint8_t i = 0; i < s->dim; i++) {
                if (s->q_bits <= 8)
                    v[i] = ((uint8_t*)(s->q_val.get()))[entry & mask];
                else
                    v[i] = ((uint16_t*)(s->q_val.get()))[entry & mask];
                entry >>= s->q_pack;
            }
            break;
        }
        case 3: {
            /* offset into array */
            void* ptr = (int32_t*)s->q_val.get() + entry * s->q_pack;

            if (s->q_bits <= 8) {
                for (uint8_t i = 0; i < s->dim; i++) v[i] = ((uint8_t*)ptr)[i];
            } else {
                for (uint8_t i = 0; i < s->dim; i++) v[i] = ((uint16_t*)ptr)[i];
            }
            break;
        }
        default: return -1;
    }

    /* we have the unpacked multiplicands; compute final vals */
    {
        int32_t shiftM = point - s->q_delp;
        int32_t add = point - s->q_minp;
        if (add > 0)
            add = s->q_min >> add;
        else
            add = s->q_min << -add;

        if (shiftM > 0)
            for (uint8_t i = 0; i < s->dim; i++) v[i] = add + ((v[i] * s->q_del) >> shiftM);
        else
            for (uint8_t i = 0; i < s->dim; i++) v[i] = add + ((v[i] * s->q_del) << -shiftM);

        if (s->q_seq)
            for (uint8_t i = 1; i < s->dim; i++) v[i] += v[i - 1];
    }

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::res_inverse(vorbis_info_residue* info, int32_t** in, int32_t* nonzero, uint8_t ch) {
    int32_t     j, k, s;
    uint8_t     m = 0, n = 0;
    uint8_t     used = 0;
    codebook_t* phrasebook = m_codebooks.get() + info->groupbook;
    uint32_t    samples_per_partition = info->grouping;
    uint8_t     partitions_per_word = phrasebook->dim;
    uint32_t    pcmend = m_blocksizes[m_dsp_state->W];

    if (info->type < 2) {
        uint32_t max = pcmend >> 1;
        uint32_t end = (info->end < max ? info->end : max);
        uint32_t n1 = end - info->begin;

        if (n1 > 0) {
            uint32_t partvals = n1 / samples_per_partition;
            uint32_t partwords = (partvals + partitions_per_word - 1) / partitions_per_word;

            for (uint8_t i = 0; i < ch; i++) {
                if (nonzero[i]) in[used++] = in[i];
            }
            ch = used;

            if (used) {
                char** partword = (char**)alloca(ch * sizeof(*partword));
                for (j = 0; j < ch; j++) { partword[j] = (char*)alloca(partwords * partitions_per_word * sizeof(*partword[j])); }
                for (s = 0; s < info->stages; s++) {
                    for (uint32_t i = 0; i < partvals;) {
                        if (s == 0) {
                            /* fetch the partition word for each channel */
                            partword[0][i + partitions_per_word - 1] = 1;
                            for (k = partitions_per_word - 2; k >= 0; k--) { partword[0][i + k] = partword[0][i + k + 1] * info->partitions; }
                            for (j = 1; j < ch; j++) {
                                for (k = partitions_per_word - 1; k >= 0; k--) { partword[j][i + k] = partword[j - 1][i + k]; }
                            }
                            for (n = 0; n < ch; n++) {
                                int32_t temp = vorbis_book_decode(phrasebook);
                                if (temp == -1) goto eopbreak;
                                /* this can be done quickly in assembly due to the quotient
                                 always being at most six bits */
                                for (m = 0; m < partitions_per_word; m++) {
                                    char div = partword[n][i + m];
                                    partword[n][i + m] = temp / div;
                                    temp -= partword[n][i + m] * div;
                                }
                            }
                        }

                        /* now we decode residual values for the partitions */
                        for (k = 0; k < partitions_per_word && i < partvals; k++, i++) {
                            for (j = 0; j < ch; j++) {
                                uint32_t offset = info->begin + i * samples_per_partition;
                                if (info->stagemasks[(int32_t)partword[j][i]] & (1 << s)) {
                                    codebook_t* stagebook = m_codebooks.get() + info->stagebooks[(partword[j][i] << 3) + s];
                                    if (info->type) {
                                        if (vorbis_book_decodev_add(stagebook, in[j] + offset, samples_per_partition, -8) == -1) goto eopbreak;
                                    } else {
                                        if (vorbis_book_decodevs_add(stagebook, in[j] + offset, samples_per_partition, -8) == -1) goto eopbreak;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        uint32_t max = (pcmend * ch) >> 1;
        uint32_t end = (info->end < max ? info->end : max);
        uint32_t n = end - info->begin;

        if (n > 0) {
            uint32_t partvals = n / samples_per_partition;
            uint32_t partwords = (partvals + partitions_per_word - 1) / partitions_per_word;

            char*   partword = (char*)alloca(partwords * partitions_per_word * sizeof(*partword));
            int32_t beginoff = info->begin / ch;

            uint8_t i = 0;
            for (i = 0; i < ch; i++)
                if (nonzero[i]) break;
            if (i == ch) return (0); /* no nonzero vectors */

            samples_per_partition /= ch;

            for (s = 0; s < info->stages; s++) {
                for (uint32_t i = 0; i < partvals;) {
                    if (s == 0) {
                        int32_t temp;
                        partword[i + partitions_per_word - 1] = 1;
                        for (k = partitions_per_word - 2; k >= 0; k--) partword[i + k] = partword[i + k + 1] * info->partitions;

                        /* fetch the partition word */
                        temp = vorbis_book_decode(phrasebook);
                        if (temp == -1) goto eopbreak;

                        /* this can be done quickly in assembly due to the quotient always being at most six bits */
                        for (k = 0; k < partitions_per_word; k++) {
                            char div = partword[i + k];
                            partword[i + k] = (char)temp / div;
                            temp -= partword[i + k] * div;
                        }
                    }

                    /* now we decode residual values for the partitions */
                    for (k = 0; k < partitions_per_word && i < partvals; k++, i++)
                        if (info->stagemasks[(int32_t)partword[i]] & (1 << s)) {
                            codebook_t* stagebook = m_codebooks.get() + info->stagebooks[(partword[i] << 3) + s];
                            if (vorbis_book_decodevv_add(stagebook, in, i * samples_per_partition + beginoff, ch, samples_per_partition, -8) == -1) goto eopbreak;
                        }
                }
            }
        }
    }
eopbreak:

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* decode vector / dim granularity guarding is done in the upper layer */
int32_t VorbisDecoder::vorbis_book_decodev_add(codebook_t* book, int32_t* a, int32_t n, int32_t point) {
    if (book->used_entries > 0) {
        int32_t* v = (int32_t*)alloca(sizeof(*v) * book->dim);
        uint32_t i;

        for (i = 0; i < n;) {
            if (decode_map(book, v, point)) return -1;
            for (uint8_t j = 0; i < n && j < book->dim; j++) a[i++] += v[j];
        }
    }
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* returns 0 on OK or -1 on eof */
/* decode vector / dim granularity guarding is done in the upper layer */
int32_t VorbisDecoder::vorbis_book_decodevs_add(codebook_t* book, int32_t* a, int32_t n, int32_t point) {
    if (book->used_entries > 0) {
        int32_t  step = n / book->dim;
        int32_t* v = (int32_t*)alloca(sizeof(*v) * book->dim);
        int32_t  j;

        for (j = 0; j < step; j++) {
            if (decode_map(book, v, point)) return -1;
            for (uint8_t i = 0, o = j; i < book->dim; i++, o += step) a[o] += v[i];
        }
    }
    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::floor0_inverse2(ps_ptr<vorbis_info_floor>& i, int32_t* lsp, int32_t* out) {
    vorbis_info_floor* info = (vorbis_info_floor*)i.get();

    if (lsp) {
        int32_t amp = lsp[info->order];

        /* take the coefficients back to a spectral envelope curve */
        vorbis_lsp_to_curve(out, m_blocksizes[m_dsp_state->W] / 2, info->barkmap, lsp, info->order, amp, info->ampdB, info->rate >> 1);
        return (1);
    }
    memset(out, 0, sizeof(*out) * m_blocksizes[m_dsp_state->W] / 2);
    return (0);
}

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::floor1_inverse2(ps_ptr<vorbis_info_floor>& in, int32_t* fit_value, int32_t* out) {
    vorbis_info_floor* info = (vorbis_info_floor*)in.get();

    int32_t n = m_blocksizes[m_dsp_state->W] / 2;
    int32_t j;

    if (fit_value) {
        /* render the lines */
        int32_t hx = 0;
        int32_t lx = 0;
        int32_t ly = fit_value[0] * info->mult;

        for (j = 1; j < info->posts; j++) {
            int32_t current = info->forward_index[j];
            int32_t hy = fit_value[current] & 0x7fff;
            if (hy == fit_value[current]) {
                hy *= info->mult;
                hx = info->postlist[current];

                render_line(n, lx, hx, ly, hy, out);

                lx = hx;
                ly = hy;
            }
        }
        for (j = hx; j < n; j++) out[j] *= ly; /* be certain */
        return (1);
    }
    memset(out, 0, sizeof(*out) * n);
    return (0);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::render_line(int32_t n, int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t* d) {
    int32_t dy = y1 - y0;
    int32_t adx = x1 - x0;
    int32_t ady = abs(dy);
    int32_t base = dy / adx;
    int32_t sy = (dy < 0 ? base - 1 : base + 1);
    int32_t x = x0;
    int32_t y = y0;
    int32_t err = 0;

    if (n > x1) n = x1;
    ady -= abs(base * adx);

    if (x < n) { d[x] = MULT31_SHIFT15(d[x], FLOOR_fromdB_LOOKUP[y]); }

    while (++x < n) {
        err = err + ady;
        if (err >= adx) {
            err -= adx;
            y += sy;
        } else {
            y += base;
        }
        d[x] = MULT31_SHIFT15(d[x], FLOOR_fromdB_LOOKUP[y]);
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::vorbis_lsp_to_curve(int32_t* curve, int32_t n, int32_t ln, int32_t* lsp, int32_t m, int32_t amp, int32_t ampoffset, int32_t nyq) {
    /* 0 <= m < 256 */

    /* set up for using all int32_t later */
    int32_t  i;
    int32_t  ampoffseti = ampoffset * 4096;
    int32_t  ampi = amp;
    int32_t* ilsp = (int32_t*)alloca(m * sizeof(*ilsp));
    uint32_t imap = (1UL << 31) / ln;
    uint32_t tBnyq1 = toBARK(nyq) << 1;

    /* Besenham for frequency scale to avoid a division */
    int32_t f = 0;
    int32_t fdx = n;
    int32_t fbase = nyq / fdx;
    int32_t ferr = 0;
    int32_t fdy = nyq - fbase * fdx;
    int32_t map = 0;

    uint32_t nextbark = MULT31(imap >> 1, tBnyq1);

    int32_t nextf = barklook[nextbark >> 14] + (((nextbark & 0x3fff) * (barklook[(nextbark >> 14) + 1] - barklook[nextbark >> 14])) >> 14);

    /* lsp is in 8.24, range 0 to PI; coslook wants it in .16 0 to 1*/
    for (i = 0; i < m; i++) {
        int32_t val = MULT32(lsp[i], 0x517cc2);
        /* safeguard against a malicious stream */
        if (val < 0 || (val >> COS_LOOKUP_I_SHIFT) >= COS_LOOKUP_I_SZ) {
            memset(curve, 0, sizeof(*curve) * n);
            return;
        }

        ilsp[i] = vorbis_coslook_i(val);
    }

    i = 0;
    while (i < n) {
        int32_t  j;
        uint32_t pi = 46341; /* 2**-.5 in 0.16 */
        uint32_t qi = 46341;
        int32_t  qexp = 0, shift;
        int32_t  wi;

        wi = vorbis_coslook2_i((map * imap) >> 15);

        qi *= labs(ilsp[0] - wi);
        pi *= labs(ilsp[1] - wi);

        for (j = 3; j < m; j += 2) {
            if (!(shift = MLOOP_1[(pi | qi) >> 25]))
                if (!(shift = MLOOP_2[(pi | qi) >> 19])) shift = MLOOP_3[(pi | qi) >> 16];

            qi = (qi >> shift) * labs(ilsp[j - 1] - wi);
            pi = (pi >> shift) * labs(ilsp[j] - wi);
            qexp += shift;
        }
        if (!(shift = MLOOP_1[(pi | qi) >> 25]))
            if (!(shift = MLOOP_2[(pi | qi) >> 19])) shift = MLOOP_3[(pi | qi) >> 16];

        /* pi,qi normalized collectively, both tracked using qexp */

        if (m & 1) {
            /* odd order filter; slightly assymetric */
            /* the last coefficient */
            qi = (qi >> shift) * labs(ilsp[j - 1] - wi);
            pi = (pi >> shift) << 14;
            qexp += shift;

            if (!(shift = MLOOP_1[(pi | qi) >> 25]))
                if (!(shift = MLOOP_2[(pi | qi) >> 19])) shift = MLOOP_3[(pi | qi) >> 16];

            pi >>= shift;
            qi >>= shift;
            qexp += shift - 14 * ((m + 1) >> 1);

            pi = ((pi * pi) >> 16);
            qi = ((qi * qi) >> 16);
            qexp = qexp * 2 + m;

            pi *= (1 << 14) - ((wi * wi) >> 14);
            qi += pi >> 14;
        } else {
            /* even order filter; still symmetric */
            /* p*=p(1-w), q*=q(1+w), let normalization drift because it isn't     worth tracking step by step */

            pi >>= shift;
            qi >>= shift;
            qexp += shift - 7 * m;

            pi = ((pi * pi) >> 16);
            qi = ((qi * qi) >> 16);
            qexp = qexp * 2 + m;

            pi *= (1 << 14) - wi;
            qi *= (1 << 14) + wi;
            qi = (qi + pi) >> 14;
        }

        /* we've let the normalization drift because it wasn't important; however, for the lookup, things must be
     normalized again. We need at most one right shift or a number of left shifts */

        if (qi & 0xffff0000) { /* checks for 1.VorbisDecoderVorbisDecoderVorbisDecoderVorbisDecoderVorbisDecoderx */
            qi >>= 1;
            qexp++;
        } else
            while (qi && !(qi & 0x8000)) { /* checks for 0.0VorbisDecoderVorbisDecoderVorbisDecoderVorbisDecoderVorbisDecoder::or less*/
                qi <<= 1;
                qexp--;
            }

        amp = vorbis_fromdBlook_i(ampi * /*  n.4         */
                                      vorbis_invsqlook_i(qi, qexp) -
                                  /*  m.8, m+n<=8 */
                                  ampoffseti); /*  8.12[0]     */

        curve[i] = MULT31_SHIFT15(curve[i], amp);

        while (++i < n) {
            /* line plot to get new f */
            ferr += fdy;
            if (ferr >= fdx) {
                ferr -= fdx;
                f++;
            }
            f += fbase;

            if (f >= nextf) break;

            curve[i] = MULT31_SHIFT15(curve[i], amp);
        }

        while (1) {
            map++;

            if (map + 1 < ln) {
                nextbark = MULT31((map + 1) * (imap >> 1), tBnyq1);

                nextf = barklook[nextbark >> 14] + (((nextbark & 0x3fff) * (barklook[(nextbark >> 14) + 1] - barklook[nextbark >> 14])) >> 14);
                if (f <= nextf) break;
            } else {
                nextf = 9999999;
                break;
            }
        }
        if (map >= ln) {
            map = ln - 1; /* guard against the approximation */
            nextf = 9999999;
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* used in init only; interpolate the int32_t way */
int32_t VorbisDecoder::toBARK(int32_t n) {
    int32_t i;
    for (i = 0; i < 54; i++)
        if (n >= barklook[i] && n < barklook[i + 1]) break;

    if (i == 54) {
        return 54 << 14;
    } else {
        return (i << 14) + (((n - barklook[i]) * ((1UL << 31) / (barklook[i + 1] - barklook[i]))) >> 17);
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* interpolated lookup based cos function, domain 0 to PI only */
/* a is in 0.16 format, where 0==0, 2^^16-1==PI, return 0.14 */
int32_t VorbisDecoder::vorbis_coslook_i(int32_t a) {
    int32_t i = a >> COS_LOOKUP_I_SHIFT;
    int32_t d = a & COS_LOOKUP_I_MASK;
    return COS_LOOKUP_I[i] - ((d * (COS_LOOKUP_I[i] - COS_LOOKUP_I[i + 1])) >> COS_LOOKUP_I_SHIFT);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* interpolated half-wave lookup based cos function */
/* a is in 0.16 format, where 0==0, 2^^16==PI, return .LSP_FRACBITS */
int32_t VorbisDecoder::vorbis_coslook2_i(int32_t a) {
    int32_t i = a >> COS_LOOKUP_I_SHIFT;
    int32_t d = a & COS_LOOKUP_I_MASK;
    return ((COS_LOOKUP_I[i] << COS_LOOKUP_I_SHIFT) - d * (COS_LOOKUP_I[i] - COS_LOOKUP_I[i + 1])) >> (COS_LOOKUP_I_SHIFT - LSP_FRACBITS + 14);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* interpolated lookup based fromdB function, domain -140dB to 0dB only */
/* a is in n.12 format */

int32_t VorbisDecoder::vorbis_fromdBlook_i(int32_t a) {
    if (a > 0) return 0x7fffffff;
    if (a < -573440) return 0; // replacement for if(a < (-140 << 12)) return 0;
    return FLOOR_fromdB_LOOKUP[((a + (140 << 12)) * 467) >> 20];
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::vorbis_invsqlook_i(int32_t a, int32_t e) {
    int32_t i = (a & 0x7fff) >> (INVSQ_LOOKUP_I_SHIFT - 1);
    int32_t d = a & INVSQ_LOOKUP_I_MASK;                                /*  0.10 */
    int32_t val = INVSQ_LOOKUP_I[i] -                                   /*  1.16 */
                  ((INVSQ_LOOKUP_IDel[i] * d) >> INVSQ_LOOKUP_I_SHIFT); /* result 1.16 */
    val *= ADJUST_SQRT2[e & 1];
    e = (e >> 1) + 21;
    return (val >> e);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* partial; doesn't perform last-step deinterleave/unrolling. That can be done more efficiently during pcm output */
void VorbisDecoder::mdct_backward(int32_t n, int32_t* in) {
    int32_t shift;
    int32_t step;

    for (shift = 4; !(n & (1 << shift)); shift++);
    shift = 13 - shift;
    step = 2 << shift;

    presymmetry(in, n >> 1, step);
    mdct_butterflies(in, n >> 1, shift);
    mdct_bitreverse(in, n, shift);
    mdct_step7(in, n, step);
    mdct_step8(in, n, step);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::presymmetry(int32_t* in, int32_t n2, int32_t step) {
    int32_t*       aX;
    int32_t*       bX;
    const int32_t* T;
    int32_t        n4 = n2 >> 1;

    aX = in + n2 - 3;
    T = sincos_lookup0;

    do {
        int32_t r0 = aX[0];
        int32_t r2 = aX[2];
        XPROD31(r0, r2, T[0], T[1], &aX[0], &aX[2]);
        T += step;
        aX -= 4;
    } while (aX >= in + n4);
    do {
        int32_t r0 = aX[0];
        int32_t r2 = aX[2];
        XPROD31(r0, r2, T[1], T[0], &aX[0], &aX[2]);
        T -= step;
        aX -= 4;
    } while (aX >= in);

    aX = in + n2 - 4;
    bX = in;
    T = sincos_lookup0;
    do {
        int32_t ri0 = aX[0];
        int32_t ri2 = aX[2];
        int32_t ro0 = bX[0];
        int32_t ro2 = bX[2];

        XNPROD31(ro2, ro0, T[1], T[0], &aX[0], &aX[2]);
        T += step;
        XNPROD31(ri2, ri0, T[0], T[1], &bX[0], &bX[2]);

        aX -= 4;
        bX += 4;
    } while (aX >= in + n4);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::mdct_butterflies(int32_t* x, int32_t points, int32_t shift) {
    int32_t stages = 8 - shift;
    int32_t i, j;

    for (i = 0; --stages > 0; i++) {
        for (j = 0; j < (1 << i); j++) mdct_butterfly_generic(x + (points >> i) * j, points >> i, 4 << (i + shift));
    }

    for (j = 0; j < points; j += 32) mdct_butterfly_32(x + j);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* N/stage point generic N stage butterfly (in place, 2 register) */
void VorbisDecoder::mdct_butterfly_generic(int32_t* x, int32_t points, int32_t step) {
    const int32_t* T = sincos_lookup0;
    int32_t*       x1 = x + points - 4;
    int32_t*       x2 = x + (points >> 1) - 4;
    int32_t        r0, r1, r2, r3;

    do {
        r0 = x1[0] - x1[1];
        x1[0] += x1[1];
        r1 = x1[3] - x1[2];
        x1[2] += x1[3];
        r2 = x2[1] - x2[0];
        x1[1] = x2[1] + x2[0];
        r3 = x2[3] - x2[2];
        x1[3] = x2[3] + x2[2];
        XPROD31(r1, r0, T[0], T[1], &x2[0], &x2[2]);
        XPROD31(r2, r3, T[0], T[1], &x2[1], &x2[3]);
        T += step;
        x1 -= 4;
        x2 -= 4;
    } while (T < sincos_lookup0 + 1024);
    do {
        r0 = x1[0] - x1[1];
        x1[0] += x1[1];
        r1 = x1[2] - x1[3];
        x1[2] += x1[3];
        r2 = x2[0] - x2[1];
        x1[1] = x2[1] + x2[0];
        r3 = x2[3] - x2[2];
        x1[3] = x2[3] + x2[2];
        XNPROD31(r0, r1, T[0], T[1], &x2[0], &x2[2]);
        XNPROD31(r3, r2, T[0], T[1], &x2[1], &x2[3]);
        T -= step;
        x1 -= 4;
        x2 -= 4;
    } while (T > sincos_lookup0);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* 32 point butterfly (in place, 4 register) */
void VorbisDecoder::mdct_butterfly_32(int32_t* x) {
    int32_t r0, r1, r2, r3;

    r0 = x[16] - x[17];
    x[16] += x[17];
    r1 = x[18] - x[19];
    x[18] += x[19];
    r2 = x[1] - x[0];
    x[17] = x[1] + x[0];
    r3 = x[3] - x[2];
    x[19] = x[3] + x[2];
    XNPROD31(r0, r1, cPI3_8, cPI1_8, &x[0], &x[2]);
    XPROD31(r2, r3, cPI1_8, cPI3_8, &x[1], &x[3]);

    r0 = x[20] - x[21];
    x[20] += x[21];
    r1 = x[22] - x[23];
    x[22] += x[23];
    r2 = x[5] - x[4];
    x[21] = x[5] + x[4];
    r3 = x[7] - x[6];
    x[23] = x[7] + x[6];
    x[4] = MULT31((r0 - r1), cPI2_8);
    x[5] = MULT31((r3 + r2), cPI2_8);
    x[6] = MULT31((r0 + r1), cPI2_8);
    x[7] = MULT31((r3 - r2), cPI2_8);

    r0 = x[24] - x[25];
    x[24] += x[25];
    r1 = x[26] - x[27];
    x[26] += x[27];
    r2 = x[9] - x[8];
    x[25] = x[9] + x[8];
    r3 = x[11] - x[10];
    x[27] = x[11] + x[10];
    XNPROD31(r0, r1, cPI1_8, cPI3_8, &x[8], &x[10]);
    XPROD31(r2, r3, cPI3_8, cPI1_8, &x[9], &x[11]);

    r0 = x[28] - x[29];
    x[28] += x[29];
    r1 = x[30] - x[31];
    x[30] += x[31];
    r2 = x[12] - x[13];
    x[29] = x[13] + x[12];
    r3 = x[15] - x[14];
    x[31] = x[15] + x[14];
    x[12] = r0;
    x[13] = r3;
    x[14] = r1;
    x[15] = r2;

    mdct_butterfly_16(x);
    mdct_butterfly_16(x + 16);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* 16 point butterfly (in place, 4 register) */
void VorbisDecoder::mdct_butterfly_16(int32_t* x) {
    int32_t r0, r1, r2, r3;

    r0 = x[8] - x[9];
    x[8] += x[9];
    r1 = x[10] - x[11];
    x[10] += x[11];
    r2 = x[1] - x[0];
    x[9] = x[1] + x[0];
    r3 = x[3] - x[2];
    x[11] = x[3] + x[2];
    x[0] = MULT31((r0 - r1), cPI2_8);
    x[1] = MULT31((r2 + r3), cPI2_8);
    x[2] = MULT31((r0 + r1), cPI2_8);
    x[3] = MULT31((r3 - r2), cPI2_8);

    r2 = x[12] - x[13];
    x[12] += x[13];
    r3 = x[14] - x[15];
    x[14] += x[15];
    r0 = x[4] - x[5];
    x[13] = x[5] + x[4];
    r1 = x[7] - x[6];
    x[15] = x[7] + x[6];
    x[4] = r2;
    x[5] = r1;
    x[6] = r3;
    x[7] = r0;

    mdct_butterfly_8(x);
    mdct_butterfly_8(x + 8);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* 8 point butterfly (in place) */
void VorbisDecoder::mdct_butterfly_8(int32_t* x) {
    int32_t r0 = x[0] + x[1];
    int32_t r1 = x[0] - x[1];
    int32_t r2 = x[2] + x[3];
    int32_t r3 = x[2] - x[3];
    int32_t r4 = x[4] + x[5];
    int32_t r5 = x[4] - x[5];
    int32_t r6 = x[6] + x[7];
    int32_t r7 = x[6] - x[7];

    x[0] = r5 + r3;
    x[1] = r7 - r1;
    x[2] = r5 - r3;
    x[3] = r7 + r1;
    x[4] = r4 - r0;
    x[5] = r6 - r2;
    x[6] = r4 + r0;
    x[7] = r6 + r2;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::mdct_bitreverse(int32_t* x, int32_t n, int32_t shift) {
    int32_t  bit = 0;
    int32_t* w = x + (n >> 1);

    do {
        int32_t  b = bitrev12(bit++);
        int32_t* xx = x + (b >> shift);
        int32_t  r;

        w -= 2;

        if (w > xx) {
            r = xx[0];
            xx[0] = w[0];
            w[0] = r;

            r = xx[1];
            xx[1] = w[1];
            w[1] = r;
        }
    } while (w > x);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::bitrev12(int32_t x) {
    uint8_t bitrev[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    return bitrev[x >> 8] | (bitrev[(x & 0x0f0) >> 4] << 4) | (((int32_t)bitrev[x & 0x00f]) << 8);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::mdct_step7(int32_t* x, int32_t n, int32_t step) {
    int32_t*       w0 = x;
    int32_t*       w1 = x + (n >> 1);
    const int32_t* T = (step >= 4) ? (sincos_lookup0 + (step >> 1)) : sincos_lookup1;
    const int32_t* Ttop = T + 1024;
    int32_t        r0, r1, r2, r3;

    do {
        w1 -= 2;

        r0 = w0[0] + w1[0];
        r1 = w1[1] - w0[1];
        r2 = MULT32(r0, T[1]) + MULT32(r1, T[0]);
        r3 = MULT32(r1, T[1]) - MULT32(r0, T[0]);
        T += step;

        r0 = (w0[1] + w1[1]) >> 1;
        r1 = (w0[0] - w1[0]) >> 1;
        w0[0] = r0 + r2;
        w0[1] = r1 + r3;
        w1[0] = r0 - r2;
        w1[1] = r3 - r1;

        w0 += 2;
    } while (T < Ttop);
    do {
        w1 -= 2;

        r0 = w0[0] + w1[0];
        r1 = w1[1] - w0[1];
        T -= step;
        r2 = MULT32(r0, T[0]) + MULT32(r1, T[1]);
        r3 = MULT32(r1, T[0]) - MULT32(r0, T[1]);

        r0 = (w0[1] + w1[1]) >> 1;
        r1 = (w0[0] - w1[0]) >> 1;
        w0[0] = r0 + r2;
        w0[1] = r1 + r3;
        w1[0] = r0 - r2;
        w1[1] = r3 - r1;

        w0 += 2;
    } while (w0 < w1);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::mdct_step8(int32_t* x, int32_t n, int32_t step) {
    const int32_t* T;
    const int32_t* V;
    int32_t*       iX = x + (n >> 1);
    step >>= 2;

    switch (step) {
        default:
            T = (step >= 4) ? (sincos_lookup0 + (step >> 1)) : sincos_lookup1;
            do {
                int32_t r0 = x[0];
                int32_t r1 = -x[1];
                XPROD31(r0, r1, T[0], T[1], x, x + 1);
                T += step;
                x += 2;
            } while (x < iX);
            break;

        case 1: {
            /* linear interpolation between table values: offset=0.5, step=1 */
            int32_t t0, t1, v0, v1, r0, r1;
            T = sincos_lookup0;
            V = sincos_lookup1;
            t0 = (*T++) >> 1;
            t1 = (*T++) >> 1;
            do {
                r0 = x[0];
                r1 = -x[1];
                t0 += (v0 = (*V++) >> 1);
                t1 += (v1 = (*V++) >> 1);
                XPROD31(r0, r1, t0, t1, x, x + 1);

                r0 = x[2];
                r1 = -x[3];
                v0 += (t0 = (*T++) >> 1);
                v1 += (t1 = (*T++) >> 1);
                XPROD31(r0, r1, v0, v1, x + 2, x + 3);

                x += 4;
            } while (x < iX);
            break;
        }

        case 0: {
            /* linear interpolation between table values: offset=0.25, step=0.5 */
            int32_t t0, t1, v0, v1, q0, q1, r0, r1;
            T = sincos_lookup0;
            V = sincos_lookup1;
            t0 = *T++;
            t1 = *T++;
            do {
                v0 = *V++;
                v1 = *V++;
                t0 += (q0 = (v0 - t0) >> 2);
                t1 += (q1 = (v1 - t1) >> 2);
                r0 = x[0];
                r1 = -x[1];
                XPROD31(r0, r1, t0, t1, x, x + 1);
                t0 = v0 - q0;
                t1 = v1 - q1;
                r0 = x[2];
                r1 = -x[3];
                XPROD31(r0, r1, t0, t1, x + 2, x + 3);

                t0 = *T++;
                t1 = *T++;
                v0 += (q0 = (t0 - v0) >> 2);
                v1 += (q1 = (t1 - v1) >> 2);
                r0 = x[4];
                r1 = -x[5];
                XPROD31(r0, r1, v0, v1, x + 4, x + 5);
                v0 = t0 - q0;
                v1 = t1 - q1;
                r0 = x[6];
                r1 = -x[7];
                XPROD31(r0, r1, v0, v1, x + 5, x + 6);

                x += 8;
            } while (x < iX);
            break;
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* decode vector / dim granularity guarding is done in the upper layer */
int32_t VorbisDecoder::vorbis_book_decodevv_add(codebook_t* book, int32_t** a, int32_t offset, uint8_t ch, int32_t n, int32_t point) {
    if (book->used_entries > 0) {
        int32_t* v = (int32_t*)alloca(sizeof(*v) * book->dim);
        int32_t  i;
        uint8_t  chptr = 0;
        int32_t  m = offset + n;

        for (i = offset; i < m;) {
            if (decode_map(book, v, point)) return -1;
            for (uint8_t j = 0; i < m && j < book->dim; j++) {
                a[chptr++][i] += v[j];
                if (chptr == ch) {
                    chptr = 0;
                    i++;
                }
            }
        }
    }

    return 0;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* pcm==0 indicates we just want the pending samples, no more */
int32_t VorbisDecoder::vorbis_dsp_pcmout(int16_t* outBuff, int32_t outBuffSize) {
    if (m_dsp_state->out_begin > -1 && m_dsp_state->out_begin < m_dsp_state->out_end) {
        int32_t n = m_dsp_state->out_end - m_dsp_state->out_begin;

        if (outBuff) {
            int32_t i;
            if (n > outBuffSize) {
                n = outBuffSize;
                VORBIS_LOG_ERROR("outBufferSize too small, must be min %i (int16_t) words", n);
            }
            for (i = 0; i < m_vorbisChannels; i++) {
                mdct_unroll_lap(m_blocksizes[0], m_blocksizes[1], m_dsp_state->lW, m_dsp_state->W, m_dsp_state->work[i].get(), m_dsp_state->mdctright[i].get(), _vorbis_window(m_blocksizes[0] >> 1),
                                _vorbis_window(m_blocksizes[1] >> 1), outBuff + i, m_vorbisChannels, m_dsp_state->out_begin, m_dsp_state->out_begin + n);
            }
        }
        return (n);
    }
    return (0);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t* VorbisDecoder::_vorbis_window(int32_t left) {
    switch (left) {
        case 32: return (int32_t*)vwin64;
        case 64: return (int32_t*)vwin128;
        case 128: return (int32_t*)vwin256;
        case 256: return (int32_t*)vwin512;
        case 512: return (int32_t*)vwin1024;
        case 1024: return (int32_t*)vwin2048;
        case 2048: return (int32_t*)vwin4096;
        case 4096: return (int32_t*)vwin8192;
        default: return (0);
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::mdct_unroll_lap(int32_t n0, int32_t n1, int32_t lW, int32_t W, int32_t* in, int32_t* right, const int32_t* w0, const int32_t* w1, int16_t* out, int32_t step,
                                    int32_t start, /* samples, this frame */
                                    int32_t end /* samples, this frame */) {
    int32_t*       l = in + (W && lW ? n1 >> 1 : n0 >> 1);
    int32_t*       r = right + (lW ? n1 >> 2 : n0 >> 2);
    int32_t*       post;
    const int32_t* wR = (W && lW ? w1 + (n1 >> 1) : w0 + (n0 >> 1));
    const int32_t* wL = (W && lW ? w1 : w0);

    int32_t preLap = (lW && !W ? (n1 >> 2) - (n0 >> 2) : 0);
    int32_t halfLap = (lW && W ? (n1 >> 2) : (n0 >> 2));
    int32_t postLap = (!lW && W ? (n1 >> 2) - (n0 >> 2) : 0);
    int32_t n, off;

    /* preceeding direct-copy lapping from previous frame, if any */
    if (preLap) {
        n = (end < preLap ? end : preLap);
        off = (start < preLap ? start : preLap);
        post = r - n;
        r -= off;
        start -= off;
        end -= n;
        while (r > post) {
            *out = CLIP_TO_15((*--r) >> 9);
            out += step;
        }
    }

    /* cross-lap; two halves due to wrap-around */
    n = (end < halfLap ? end : halfLap);
    off = (start < halfLap ? start : halfLap);
    post = r - n;
    r -= off;
    l -= off * 2;
    start -= off;
    wR -= off;
    wL += off;
    end -= n;
    while (r > post) {
        l -= 2;
        *out = CLIP_TO_15((MULT31(*--r, *--wR) + MULT31(*l, *wL++)) >> 9);
        out += step;
    }

    n = (end < halfLap ? end : halfLap);
    off = (start < halfLap ? start : halfLap);
    post = r + n;
    r += off;
    l += off * 2;
    start -= off;
    end -= n;
    wR -= off;
    wL += off;
    while (r < post) {
        *out = CLIP_TO_15((MULT31(*r++, *--wR) - MULT31(*l, *wL++)) >> 9);
        out += step;
        l += 2;
    }

    /* preceeding direct-copy lapping from previous frame, if any */
    if (postLap) {
        n = (end < postLap ? end : postLap);
        off = (start < postLap ? start : postLap);
        post = l + n * 2;
        l += off * 2;
        while (l < post) {
            *out = CLIP_TO_15((-*l) >> 9);
            out += step;
            l += 2;
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::MULT32(int32_t x, int32_t y) {
    union magic magic;
    magic.whole = (int64_t)x * y;
    return magic.halves.hi;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::MULT31_SHIFT15(int32_t x, int32_t y) {
    union magic magic;
    magic.whole = (int64_t)x * y;
    return ((uint32_t)(magic.halves.lo) >> 15) | ((magic.halves.hi) << 17);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::MULT31(int32_t x, int32_t y) {
    return MULT32(x, y) << 1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::XPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t* x, int32_t* y) {
    *x = MULT31(a, t) + MULT31(b, v);
    *y = MULT31(b, t) - MULT31(a, v);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void VorbisDecoder::XNPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t* x, int32_t* y) {
    *x = MULT31(a, t) - MULT31(b, v);
    *y = MULT31(b, t) + MULT31(a, v);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::CLIP_TO_15(int32_t x) {
    int32_t ret = x;
    ret -= ((x <= 32767) - 1) & (x - 32767);
    ret -= ((x >= -32768) - 1) & (x + 32768);
    return (ret);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t VorbisDecoder::specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact) {
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
int32_t VorbisDecoder::specialIndexOf_icase(uint8_t* base, const char* str, int32_t baselen, bool exact) {
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
uint32_t VorbisDecoder::little_endian(uint8_t* data) {
    return (uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24));
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
