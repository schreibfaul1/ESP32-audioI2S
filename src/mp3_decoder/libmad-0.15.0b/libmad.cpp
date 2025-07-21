// libmad.cpp

/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2003 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: rq_table.dat,v 1.6 2003/05/27 22:40:36 rob Exp $
 */

#include "libmad.h"

uint32_t s_nsamples = 0;
uint32_t s_samplerate = 0;
uint8_t  s_channels   = 0;  // 1 Mono, 2 Stereo
uint16_t s_samples_per_frame = 0;
uint32_t s_bitrate = 0;
uint8_t  s_mpeg_version = 0;
uint8_t  s_layer = 0;
uint8_t  s_channel_mode = 0;
uint8_t  s_underflow_cnt = 0;

uint32_t s_xing_bitrate = 0;
uint32_t s_xing_total_bytes = 0;
uint32_t s_xing_duration_seconds = 0;
uint32_t s_xing_total_frames = 0;

ps_ptr<mad_stream_t>stream;
ps_ptr<mad_frame_t> frame;
ps_ptr<mad_synth_t> synth;

typedef int32_t FilterType[2][2][2][16][8]; FilterType *s_filter;
uint8_t  s_mad_channels = 2;
uint32_t s_mad_out_samples;
uint32_t s_mad_sampleRate;
ps_array2d<int32_t> s_samplesBuff;
ps_array3d<int32_t> s_sbsample;
ps_array3d<int32_t> s_overlap;
ps_ptr<uint8_t>s_main_data;
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool allocateBuffers(){
    s_nsamples = 0;
    s_samplerate = 0;
    s_channels   = 0;  // 1 Mono, 2 Stereo
    s_samples_per_frame = 0;
    s_bitrate = 0;
    s_mpeg_version = 0;
    s_layer = 0;
    s_channel_mode = 0;
    s_underflow_cnt = 0;

    s_xing_bitrate = 0;
    s_xing_total_bytes = 0;
    s_xing_duration_seconds = 0;
    s_xing_total_frames = 0;

    stream.alloc();
    frame.alloc();
    synth.alloc();
    s_filter = (FilterType*)ps_malloc(sizeof(FilterType));
    if (!s_filter) { printf("PSRAM allocation failed!\n"); }
    s_samplesBuff.alloc(2, 1152);

    s_main_data.alloc(MAD_BUFFER_MDLEN);
    s_sbsample.alloc(2, 36, 32); /* synthesis subband filter samples */
    mad_stream_init(stream.get());
    mad_frame_init(frame.get());
    mad_synth_init(synth.get());
    return true;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void clearBuffers(){
    stream.clear();
    frame.clear();
    synth.clear();
    s_samplesBuff.clear();
    s_sbsample.clear();
    s_overlap.clear();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void freeBuffers(){
    stream.reset();
    frame.reset();
    synth.reset();
    if(s_filter){free(s_filter); s_filter = nullptr;}
    s_main_data.reset();
    s_sbsample.reset();
    s_overlap.reset();
    s_samplesBuff.reset();
    s_sbsample.reset();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_bit_init(struct mad_bitptr* bitptr, uint8_t const* byte) { // initialize bit pointer struct
    bitptr->byte = byte;
    bitptr->cache = 0;
    bitptr->left = CHAR_BIT;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_bit_length(struct mad_bitptr const* begin, struct mad_bitptr const* end) {	// return number of bits between start and end points
    return begin->left + CHAR_BIT * (end->byte - (begin->byte + 1)) + (CHAR_BIT - end->left);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t const* mad_bit_nextbyte(struct mad_bitptr const* bitptr) { // return pointer to next unprocessed byte
    return bitptr->left == CHAR_BIT ? bitptr->byte : bitptr->byte + 1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_bit_skip(struct mad_bitptr* bitptr, uint32_t len) { // advance bit pointer
    bitptr->byte += len / CHAR_BIT;
    bitptr->left -= len % CHAR_BIT;
    if (bitptr->left > CHAR_BIT) {
        bitptr->byte++;
        bitptr->left += CHAR_BIT;
    }
    if (bitptr->left < CHAR_BIT) bitptr->cache = *bitptr->byte;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_bit_read(struct mad_bitptr* bitptr, uint32_t len) { // 	read an arbitrary number of bits and return their UIMSBF value
    uint32_t value;
    if (bitptr->left == CHAR_BIT) bitptr->cache = *bitptr->byte;
    if (len < bitptr->left) {
        value = (bitptr->cache & ((1 << bitptr->left) - 1)) >> (bitptr->left - len);
        bitptr->left -= len;
        return value;
    }
    /* remaining bits in current byte */
    value = bitptr->cache & ((1 << bitptr->left) - 1);
    len -= bitptr->left;
    bitptr->byte++;
    bitptr->left = CHAR_BIT;
    /* more bytes */
    while (len >= CHAR_BIT) {
        value = (value << CHAR_BIT) | *bitptr->byte++;
        len -= CHAR_BIT;
    }
    if (len > 0) {
        bitptr->cache = *bitptr->byte;
        value = (value << len) | (bitptr->cache >> (CHAR_BIT - len));
        bitptr->left -= len;
    }
    return value;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint16_t mad_bit_crc(struct mad_bitptr bitptr, uint32_t len, uint16_t init) { //  DESCRIPTION:	compute CRC-check word
    uint32_t crc;
    for (crc = init; len >= 32; len -= 32) {
        uint32_t data;
        data = mad_bit_read(&bitptr, 32);
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >> 24)) & 0xff];
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >> 16)) & 0xff];
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >> 8)) & 0xff];
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >> 0)) & 0xff];
    }
    switch (len / 8) {
        case 3: crc = (crc << 8) ^ crc_table[((crc >> 8) ^ mad_bit_read(&bitptr, 8)) & 0xff]; [[fallthrough]];
        case 2: crc = (crc << 8) ^ crc_table[((crc >> 8) ^ mad_bit_read(&bitptr, 8)) & 0xff]; [[fallthrough]];
        case 1:
            crc = (crc << 8) ^ crc_table[((crc >> 8) ^ mad_bit_read(&bitptr, 8)) & 0xff];
            len %= 8;
            [[fallthrough]];
        case 0: break;
    }
    while (len--) {
        uint32_t msb;
        msb = mad_bit_read(&bitptr, 1) ^ (crc >> 15);
        crc <<= 1;
        if (msb & 1) crc ^= CRC_POLY;
    }
    return crc & 0xffff;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t I_sample(struct mad_bitptr* ptr, uint32_t nb) { // decode one requantized Layer I sample from a bitstream
    int32_t sample;
    sample = mad_bit_read(ptr, nb);
    /* invert most significant bit, extend sign, then scale to fixed format */
    sample ^= 1 << (nb - 1);
    sample |= -(sample & (1 << (nb - 1)));
    sample <<= MAD_F_FRACBITS - (nb - 1);
    /* requantize the sample */
    /* s'' = (2^nb / (2^nb - 1)) * (s''' + 2^(-nb + 1)) */
    sample += MAD_F_ONE >> (nb - 1);
    return mad_f_mul(sample, linear_table[nb - 2]);
    /* s' = factor * s'' */
    /* (to be performed by caller) */
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_layer_I(mad_stream_t* stream, mad_frame_t* frame) { // decode a single Layer I frame
    struct mad_header* header = &frame->header;
    uint32_t       nch, bound, ch, s, sb, nb;
    ps_array2d<uint8_t> allocation; allocation.alloc(2, 32);
    ps_array2d<uint8_t> scalefactor; scalefactor.alloc(2, 32);
    nch = MAD_NCHANNELS(header);
    bound = 32;
    if (header->mode == MAD_MODE_JOINT_STEREO) {
        header->flags |= MAD_FLAG_I_STEREO;
        bound = 4 + header->mode_extension * 4;
    }
    /* check CRC word */
    if (header->flags & MAD_FLAG_PROTECTION) {
        header->crc_check = mad_bit_crc(stream->ptr, 4 * (bound * nch + (32 - bound)), header->crc_check);
        if (header->crc_check != header->crc_target && !(frame->options & MAD_OPTION_IGNORECRC)) {
            MP3_LOG_ERROR("CRC check failed");
            stream->error = MAD_ERROR_BADCRC;
            return -1;
        }
    }
    /* decode bit allocations */
    for (sb = 0; sb < bound; ++sb) {
        for (ch = 0; ch < nch; ++ch) {
            nb = mad_bit_read(&stream->ptr, 4);
            if (nb == 15) {
                MP3_LOG_ERROR("forbidden bit allocation value");
                stream->error = MAD_ERROR_BADBITALLOC;
                return -1;
            }
            allocation(ch, sb) = nb ? nb + 1 : 0;
        }
    }
    for (sb = bound; sb < 32; ++sb) {
        nb = mad_bit_read(&stream->ptr, 4);
        if (nb == 15) {
            MP3_LOG_ERROR("forbidden bit allocation value");
            stream->error = MAD_ERROR_BADBITALLOC;
            return -1;
        }
        allocation(0, sb) = allocation(1, sb) = nb ? nb + 1 : 0;
    }
    /* decode scalefactors */
    for (sb = 0; sb < 32; ++sb) {
        for (ch = 0; ch < nch; ++ch) {
            if (allocation(ch, sb)) {
                scalefactor(ch, sb) = mad_bit_read(&stream->ptr, 6);
#if defined(OPT_STRICT)
                /*
                 * Scalefactor index 63 does not appear in Table B.1 of
                 * ISO/IEC 11172-3. Nonetheless, other implementations accept it,
                 * so we only reject it if OPT_STRICT is defined.
                 */
                if (scalefactor[ch][sb] == 63) {
                    MP3_LOG_ERROR("bad scalefactor index");
                    stream->error = MAD_ERROR_BADSCALEFACTOR;
                    return -1;
                }
#endif
            }
        }
    }
    /* decode samples */
    for (s = 0; s < 12; ++s) {
        for (sb = 0; sb < bound; ++sb) {
            for (ch = 0; ch < nch; ++ch) {
                nb = allocation(ch, sb);
                s_sbsample(ch, s, sb) = nb ? mad_f_mul(I_sample(&stream->ptr, nb), sf_table[scalefactor(ch, sb)]) : 0;
            }
        }
        for (sb = bound; sb < 32; ++sb) {
            if ((nb = allocation(0, sb))) {
                int32_t sample;
                sample = I_sample(&stream->ptr, nb);
                for (ch = 0; ch < nch; ++ch) { s_sbsample(ch, s, sb) = mad_f_mul(sample, sf_table[scalefactor(ch, sb)]); }
            } else {
                for (ch = 0; ch < nch; ++ch) s_sbsample(ch, s, sb) = 0;
            }
        }
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void II_samples(struct mad_bitptr* ptr, struct quantclass const* quantclass, int32_t output[3]) { // decode three requantized Layer II samples from a bitstream
    uint32_t nb, s, sample[3];
    if ((nb = quantclass->group)) {
        uint32_t c, nlevels;
        /* degrouping */
        c = mad_bit_read(ptr, quantclass->bits);
        nlevels = quantclass->nlevels;
        for (s = 0; s < 3; ++s) {
            sample[s] = c % nlevels;
            c /= nlevels;
        }
    } else {
        nb = quantclass->bits;
        for (s = 0; s < 3; ++s) sample[s] = mad_bit_read(ptr, nb);
    }
    for (s = 0; s < 3; ++s) {
        int32_t requantized;
        /* invert most significant bit, extend sign, then scale to fixed format */
        requantized = sample[s] ^ (1 << (nb - 1));
        requantized |= -(requantized & (1 << (nb - 1)));
        requantized <<= MAD_F_FRACBITS - (nb - 1);
        /* requantize the sample */
        /* s'' = C * (s''' + D) */
        output[s] = mad_f_mul(requantized + quantclass->D, quantclass->C);
        /* s' = factor * s'' */
        /* (to be performed by caller) */
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_layer_II(mad_stream_t* stream, mad_frame_t* frame) { // decode a single Layer II frame
    struct mad_header*   header = &frame->header;
    struct mad_bitptr    start;
    uint32_t         index, sblimit, nbal, nch, bound, gr, ch, s, sb;
    uint8_t const* offsets;
    ps_array2d<uint8_t>allocation; allocation.alloc(2, 32);
    ps_array2d<uint8_t>scfsi; scfsi.alloc(2, 32);
    ps_array3d<uint8_t>scalefactor; scalefactor.alloc(2, 32, 3);
    int32_t              samples[3];
    nch = MAD_NCHANNELS(header);
    if (header->flags & MAD_FLAG_LSF_EXT)
        index = 4;
    else {
        switch (nch == 2 ? header->bitrate / 2 : header->bitrate) {
            case 32000:
            case 48000: index = (header->samplerate == 32000) ? 3 : 2; break;
            case 56000:
            case 64000:
            case 80000: index = 0; break;
            default: index = (header->samplerate == 48000) ? 0 : 1;
        }
    }
    sblimit = sbquant_table[index].sblimit;
    offsets = sbquant_table[index].offsets;
    bound = 32;
    if (header->mode == MAD_MODE_JOINT_STEREO) {
        header->flags |= MAD_FLAG_I_STEREO;
        bound = 4 + header->mode_extension * 4;
    }
    if (bound > sblimit) bound = sblimit;
    start = stream->ptr;
    /* decode bit allocations */
    for (sb = 0; sb < bound; ++sb) {
        nbal = bitalloc_table[offsets[sb]].nbal;
        for (ch = 0; ch < nch; ++ch) allocation(ch, sb) = mad_bit_read(&stream->ptr, nbal);
    }
    for (sb = bound; sb < sblimit; ++sb) {
        nbal = bitalloc_table[offsets[sb]].nbal;
        allocation(0, sb) = allocation(1, sb) = mad_bit_read(&stream->ptr, nbal);
    }
    /* decode scalefactor selection info */
    for (sb = 0; sb < sblimit; ++sb) {
        for (ch = 0; ch < nch; ++ch) {
            if (allocation(ch, sb)) scfsi(ch, sb) = mad_bit_read(&stream->ptr, 2);
        }
    }
    /* check CRC word */
    if (header->flags & MAD_FLAG_PROTECTION) {
        header->crc_check = mad_bit_crc(start, mad_bit_length(&start, &stream->ptr), header->crc_check);
        if (header->crc_check != header->crc_target && !(frame->options & MAD_OPTION_IGNORECRC)) {
            MP3_LOG_ERROR("CRC check failed");
            stream->error = MAD_ERROR_BADCRC;
            return -1;
        }
    }
    /* decode scalefactors */
    for (sb = 0; sb < sblimit; ++sb) {
        for (ch = 0; ch < nch; ++ch) {
            if (allocation(ch, sb)) {
                scalefactor(ch, sb, 0) = mad_bit_read(&stream->ptr, 6);
                switch (scfsi(ch, sb)) {
                    case 2: scalefactor(ch, sb, 2) = scalefactor(ch, sb, 1) = scalefactor(ch, sb, 0); break;
                    case 0:
                        scalefactor(ch, sb, 1) = mad_bit_read(&stream->ptr, 6);
                        /* fall through */
                    case 1:
                    case 3: scalefactor(ch, sb, 2) = mad_bit_read(&stream->ptr, 6);
                }
                if (scfsi(ch, sb) & 1) scalefactor(ch, sb, 1) = scalefactor(ch, sb, scfsi(ch, sb) - 1);
#if defined(OPT_STRICT)
                /*
                 * Scalefactor index 63 does not appear in Table B.1 of
                 * ISO/IEC 11172-3. Nonetheless, other implementations accept it,
                 * so we only reject it if OPT_STRICT is defined.
                 */
                if (scalefactor(ch, sb, 0) == 63 || scalefactor(ch, sb, 1) == 63 || scalefactor(ch, sb, 2) == 63) {
                    MP3_LOG_ERROR("bad scalefactor index");
                    stream->error = MAD_ERROR_BADSCALEFACTOR;
                    return -1;
                }
#endif
            }
        }
    }
    /* decode samples */
    for (gr = 0; gr < 12; ++gr) {
        for (sb = 0; sb < bound; ++sb) {
            for (ch = 0; ch < nch; ++ch) {
                if ((index = allocation(ch, sb))) {
                    index = offset_table[bitalloc_table[offsets[sb]].offset][index - 1];
                    II_samples(&stream->ptr, &qc_table[index], samples);
                    for (s = 0; s < 3; ++s) { s_sbsample(ch, 3 * gr + s, sb) = mad_f_mul(samples[s], sf_table[scalefactor(ch, sb, gr / 4)]); }
                } else {
                    for (s = 0; s < 3; ++s) s_sbsample(ch, 3 * gr + s, sb) = 0;
                }
            }
        }
        for (sb = bound; sb < sblimit; ++sb) {
            if ((index = allocation(0, sb))) {
                index = offset_table[bitalloc_table[offsets[sb]].offset][index - 1];
                II_samples(&stream->ptr, &qc_table[index], samples);
                for (ch = 0; ch < nch; ++ch) {
                    for (s = 0; s < 3; ++s) { s_sbsample(ch, 3 * gr + s, sb) = mad_f_mul(samples[s], sf_table[scalefactor(ch, sb, gr / 4)]); }
                }
            } else {
                for (ch = 0; ch < nch; ++ch) {
                    for (s = 0; s < 3; ++s) s_sbsample(ch, 3 * gr + s, sb) = 0;
                }
            }
        }
        for (ch = 0; ch < nch; ++ch) {
            for (s = 0; s < 3; ++s) {
                for (sb = sblimit; sb < 32; ++sb) s_sbsample(ch, 3 * gr + s, sb) = 0;
            }
        }
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
char const* mad_stream_errorstr(mad_stream_t const* stream) { // return a string description of the current error condition
    switch (stream->error) {
        case MAD_ERROR_NONE: return "no error";
        case MAD_ERROR_BUFLEN: return "input buffer too small (or EOF)";
        case MAD_ERROR_BUFPTR: return "invalid (null) buffer pointer";
        case MAD_ERROR_NOMEM: return "not enough memory";
        case MAD_ERROR_LOSTSYNC: return "lost synchronization";
        case MAD_ERROR_BADLAYER: return "reserved header layer value";
        case MAD_ERROR_BADBITRATE: return "forbidden bitrate value";
        case MAD_ERROR_BADSAMPLERATE: return "reserved sample frequency value";
        case MAD_ERROR_BADEMPHASIS: return "reserved emphasis value";
        case MAD_ERROR_BADCRC: return "CRC check failed";
        case MAD_ERROR_BADBITALLOC: return "forbidden bit allocation value";
        case MAD_ERROR_BADSCALEFACTOR: return "bad scalefactor index";
        case MAD_ERROR_BADFRAMELEN: return "bad frame length";
        case MAD_ERROR_BADBIGVALUES: return "bad big_values count";
        case MAD_ERROR_BADBLOCKTYPE: return "reserved block_type";
        case MAD_ERROR_BADSCFSI: return "bad scalefactor selection info";
        case MAD_ERROR_BADDATAPTR: return "bad main_data_begin pointer";
        case MAD_ERROR_BADPART3LEN: return "bad audio data length";
        case MAD_ERROR_BADHUFFTABLE: return "bad Huffman table select";
        case MAD_ERROR_BADHUFFDATA: return "Huffman data overrun";
        case MAD_ERROR_BADSTEREO: return "incompatible block_type for JS";
        case MAD_ERROR_CONTINUE: return "no error, needs more data";
    }
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_stream_sync(mad_stream_t* stream) { // locate the next stream sync word
    uint8_t const *ptr, *end;
    ptr = mad_bit_nextbyte(&stream->ptr);
    end = stream->bufend;
    while (ptr < end - 1 && !(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0)) ++ptr;
    if (end - ptr < MAD_BUFFER_GUARD) return -1;
    mad_bit_init(&stream->ptr, ptr);
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_stream_skip(mad_stream_t* stream, uint32_t length) { // arrange to skip bytes before the next frame
    stream->skiplen += length;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_stream_buffer(mad_stream_t* stream, uint8_t const* buffer, uint32_t length) { // set stream buffer pointers
    stream->buffer = buffer;
    stream->bufend = buffer + length;
    stream->this_frame = buffer;
    stream->next_frame = buffer;
    stream->sync = 1;
    mad_bit_init(&stream->ptr, buffer);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_stream_finish(mad_stream_t* stream) { // deallocate any dynamic memory associated with stream
    if (s_main_data.valid()) {
        s_main_data.reset();
    }
    mad_bit_finish(&stream->anc_ptr);
    mad_bit_finish(&stream->ptr);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_stream_init(mad_stream_t* stream) {
    stream->buffer = 0;
    stream->bufend = 0;
    stream->skiplen = 0;
    stream->sync = 0;
    stream->freerate = 0;
    stream->this_frame = 0;
    stream->next_frame = 0;
    mad_bit_init(&stream->ptr, 0);
    mad_bit_init(&stream->anc_ptr, 0);
    stream->anc_bitlen = 0;
    s_main_data.clear();
    stream->md_len = 0;
    stream->options = 0;
    stream->error = MAD_ERROR_NONE;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_timer_string(mad_timer_t timer, char* dest, char const* format, enum mad_units units, enum mad_units fracunits, uint32_t subparts) { // write a string representation of a timer using a template
    uint32_t hours, minutes, seconds, sub;
    uint32_t  frac;
    timer = mad_timer_abs(timer);
    seconds = timer.seconds;
    frac = sub = 0;
    switch (fracunits) {
    case MAD_UNITS_HOURS:
    case MAD_UNITS_MINUTES:
    case MAD_UNITS_SECONDS: break; // Diese Fälle machen nichts, da sie nur die Ganzzahl-Sekunden betreffen

    case MAD_UNITS_DECISECONDS:
    case MAD_UNITS_CENTISECONDS:
    case MAD_UNITS_MILLISECONDS:
    case MAD_UNITS_8000_HZ:
    case MAD_UNITS_11025_HZ:
    case MAD_UNITS_12000_HZ:
    case MAD_UNITS_16000_HZ:
    case MAD_UNITS_22050_HZ:
    case MAD_UNITS_24000_HZ:
    case MAD_UNITS_32000_HZ:
    case MAD_UNITS_44100_HZ:
    case MAD_UNITS_48000_HZ:
    case MAD_UNITS_24_FPS:
    case MAD_UNITS_25_FPS:
    case MAD_UNITS_30_FPS:
    case MAD_UNITS_48_FPS:
    case MAD_UNITS_50_FPS:
    case MAD_UNITS_60_FPS:
    case MAD_UNITS_75_FPS:
    case MAD_UNITS_FRAMES:   // Hinzugefügt
    case MAD_UNITS_SAMPLES:  // Hinzugefügt
    case MAD_UNITS_HZ:       // Hinzugefügt
    {
        uint32_t denom;
        denom = MAD_TIMER_RESOLUTION / fracunits;
        frac = timer.fraction / denom;
        sub = scale_rational(timer.fraction % denom, denom, subparts);
    } break;

    case MAD_UNITS_23_976_FPS:
    case MAD_UNITS_24_975_FPS:
    case MAD_UNITS_29_97_FPS:
    case MAD_UNITS_47_952_FPS:
    case MAD_UNITS_49_95_FPS:
    case MAD_UNITS_59_94_FPS:
        /* drop-frame encoding */
        /* N.B. this is only well-defined for MAD_UNITS_29_97_FPS */
        {
            uint32_t frame, cycle, d, m;
            frame = mad_timer_count(timer, fracunits);
            cycle = -fracunits * 60 * 10 - (10 - 1) * 2;
            d = frame / cycle;
            m = frame % cycle;
            frame += (10 - 1) * 2 * d;
            if (m > 2) frame += 2 * ((m - 2) / (cycle / 10));
            frac = frame % -fracunits;
            seconds = frame / -fracunits;
        }
        break;

    // Neue Fälle für Einheiten, deren Bruchteil in diesem Kontext nicht direkt sinnvoll ist
    case MAD_UNITS_BITFRAMES:   // Hinzugefügt
    case MAD_UNITS_BYTES:       // Hinzugefügt
    case MAD_UNITS_PERCENT:     // Hinzugefügt
    case MAD_UNITS_DECFRAMES:   // Hinzugefügt
        frac = 0; // Kein signifikanter Bruchteil oder Logik nicht direkt anwendbar
        sub = 0;  // Kein Sub-Bruchteil
        break;
}
    switch (units) {
        case MAD_UNITS_HOURS:
            minutes = seconds / 60;
            hours = minutes / 60;
            sprintf(dest, format, hours, (uint32_t)(minutes % 60), (uint32_t)(seconds % 60), frac, sub);
            break;
        case MAD_UNITS_MINUTES:
            minutes = seconds / 60;
            sprintf(dest, format, minutes, (uint32_t)(seconds % 60), frac, sub);
            break;
        case MAD_UNITS_SECONDS: sprintf(dest, format, seconds, frac, sub); break;
        case MAD_UNITS_DECFRAMES:
        case MAD_UNITS_FRAMES:
        case MAD_UNITS_SAMPLES:
        case MAD_UNITS_BITFRAMES:
        case MAD_UNITS_BYTES:
        case MAD_UNITS_PERCENT:
        case MAD_UNITS_HZ:
        case MAD_UNITS_23_976_FPS:
        case MAD_UNITS_24_975_FPS:
        case MAD_UNITS_29_97_FPS:
        case MAD_UNITS_47_952_FPS:
        case MAD_UNITS_49_95_FPS:
        case MAD_UNITS_59_94_FPS:
            if (fracunits < 0) {
                /* not yet implemented */
                sub = 0;
            }
            /* fall through */
        case MAD_UNITS_DECISECONDS:
        case MAD_UNITS_CENTISECONDS:
        case MAD_UNITS_MILLISECONDS:
        case MAD_UNITS_8000_HZ:
        case MAD_UNITS_11025_HZ:
        case MAD_UNITS_12000_HZ:
        case MAD_UNITS_16000_HZ:
        case MAD_UNITS_22050_HZ:
        case MAD_UNITS_24000_HZ:
        case MAD_UNITS_32000_HZ:
        case MAD_UNITS_44100_HZ:
        case MAD_UNITS_48000_HZ:
        case MAD_UNITS_24_FPS:
        case MAD_UNITS_25_FPS:
        case MAD_UNITS_30_FPS:
        case MAD_UNITS_48_FPS:
        case MAD_UNITS_50_FPS:
        case MAD_UNITS_60_FPS:
        case MAD_UNITS_75_FPS: sprintf(dest, format, mad_timer_count(timer, units), sub); break;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_timer_fraction(mad_timer_t timer, uint32_t denom) { // return fractional part of timer in arbitrary terms
    timer = mad_timer_abs(timer);
    switch (denom) {
        case 0: return timer.fraction ? MAD_TIMER_RESOLUTION / timer.fraction : MAD_TIMER_RESOLUTION + 1;
        case MAD_TIMER_RESOLUTION: return timer.fraction;
        default: return scale_rational(timer.fraction, MAD_TIMER_RESOLUTION, denom);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static mad_fixed_t mad_timer_d_base(mad_units units) {
    switch (units) {
        case MAD_UNITS_HOURS:      return MAD_F_ONE; // Base is 1 hour
        case MAD_UNITS_MINUTES:    return MAD_F_ONE; // Base is 1 minute
        case MAD_UNITS_SECONDS:    return MAD_F_ONE; // Base is 1 second

        case MAD_UNITS_DECISECONDS:  return MAD_F_ONE / 10;
        case MAD_UNITS_CENTISECONDS: return MAD_F_ONE / 100;
        case MAD_UNITS_MILLISECONDS: return MAD_F_ONE / 1000;

        // Diese Werte repräsentieren die Fixed-Point-Anzahl von 'Basis-Einheiten' pro Sekunde/Frame.
        // Sie müssen zur tatsächlichen Abtastrate und Frame-Rate Ihres Audio-Streams passen.
        // Wenn die mad_timer_t-Werte in HZ (Samples/Sekunde) gespeichert sind,
        // dann ist MAD_UNITS_HZ die Basis, und andere werden relativ dazu berechnet.
        // Die Original-libmad verwendet oft einen "current_stream->samplerate" oder ähnliches.
        // Da wir das nicht haben, nehme ich MAD_F_ONE als Basis und Sie müssen anpassen.

        // Annahme: Wenn MAD_UNITS_HZ als 1007 im Enum ist, es aber für Berechnungen den Wert 1 (MAD_F_ONE) braucht,
        // muss dieser Fall korrekt behandelt werden.
        case MAD_UNITS_HZ:          return MAD_F_ONE; // 1 sample per second in fixed-point

        case MAD_UNITS_8000_HZ:     return MAD_F_ONE; // These are also 1 unit per their rate
        case MAD_UNITS_11025_HZ:    return MAD_F_ONE;
        case MAD_UNITS_12000_HZ:    return MAD_F_ONE;
        case MAD_UNITS_16000_HZ:    return MAD_F_ONE;
        case MAD_UNITS_22050_HZ:    return MAD_F_ONE;
        case MAD_UNITS_24000_HZ:    return MAD_F_ONE;
        case MAD_UNITS_32000_HZ:    return MAD_F_ONE;
        case MAD_UNITS_44100_HZ:    return MAD_F_ONE;
        case MAD_UNITS_48000_HZ:    return MAD_F_ONE;

        // Für FPS-Einheiten: Anzahl der HZ (Samples) pro Frame.
        // Dies hängt von der aktuellen Abtastrate des Streams ab.
        // Als Platzhalter verwenden wir eine feste Rate (z.B. 44100 Hz).
        // Im realen Decoder würden Sie hier s_mad_sampleRate oder eine ähnliche Variable verwenden.
        // Beispiel: return (MAD_F_ONE * s_mad_sampleRate) / units_value_in_hz;

        // Da 'units' die FPS selbst ist, müssen wir den Umrechnungsfaktor zur Samples-Einheit finden.
        // z.B. Samples pro Frame = SampleRate / FPS
        case MAD_UNITS_24_FPS:     return MAD_F_ONE / 24;
        case MAD_UNITS_25_FPS:     return MAD_F_ONE / 25;
        case MAD_UNITS_30_FPS:     return MAD_F_ONE / 30;
        case MAD_UNITS_48_FPS:     return MAD_F_ONE / 48;
        case MAD_UNITS_50_FPS:     return MAD_F_ONE / 50;
        case MAD_UNITS_60_FPS:     return MAD_F_ONE / 60;
        case MAD_UNITS_75_FPS:     return MAD_F_ONE / 75;

        case MAD_UNITS_FRAMES:     return MAD_F_ONE / 25; // oder den tatsächlichen Standard-FPS-Wert für FRAMES
        case MAD_UNITS_SAMPLES:    return MAD_F_ONE; // 1 sample in fixed-point
        case MAD_UNITS_BITFRAMES:  return MAD_F_ONE / 8; // 1/8th of a byte
        case MAD_UNITS_BYTES:      return MAD_F_ONE; // 1 byte in fixed-point
        case MAD_UNITS_PERCENT:    return MAD_F_ONE / 100; // 1/100 of a percent

        // Die negativen FPS-Werte sollten hier nicht direkt erscheinen, da sie durch
        // die iterative Logik in mad_timer_count() in ihre positiven Gegenstücke umgewandelt werden.
        // Dennoch füge ich sie hier für den Fall ein, dass sie unerwarteterweise aufgerufen werden.
        case MAD_UNITS_23_976_FPS:
        case MAD_UNITS_24_975_FPS:
        case MAD_UNITS_29_97_FPS: // Assuming MAD_UNITS_29_97_FPS
        case MAD_UNITS_47_952_FPS:
        case MAD_UNITS_49_95_FPS:
        case MAD_UNITS_59_94_FPS:
            // This case should ideally not be reached if mad_timer_count normalizes 'units' first.
            // Returning the base unit's factor.
            return mad_timer_d_base((mad_units)(-units)); // Recursively call with positive unit (will terminate)
                                                         // Or, for iterative mad_timer_count, this branch
                                                         // should never be hit here. Default should be fine.


        default:                   return MAD_F_ONE; // Fallback, e.g., if units is 0 or unknown
    }
}

int32_t mad_fixed_to_long(mad_fixed_t value) {
    // Round to nearest integer (add 0.5 before truncating)
    // For positive numbers: (value + (MAD_F_ONE >> 1)) / MAD_F_ONE
    // For negative numbers: (value - (MAD_F_ONE >> 1)) / MAD_F_ONE
    // A more robust way (rounds halves away from zero):
    if (value >= 0) {
        return (value + (MAD_F_ONE / 2)) / MAD_F_ONE;
    } else {
        return (value - (MAD_F_ONE / 2)) / MAD_F_ONE;
    }
}

int32_t mad_timer_count(mad_timer_t timer, mad_units units) {
    int32_t final_count;

    // KORRIGIERT: Kombiniere seconds und fraction zu einem mad_fixed_t Wert
    // Die seconds werden mit MAD_F_ONE skaliert (MAD_F_ONE ist die fixed-point Darstellung von 1.0)
    // Die fraction wird entsprechend ihrer Resolution skaliert
    mad_fixed_t current_value = (mad_fixed_t)timer.seconds * MAD_F_ONE +
                                (mad_fixed_t)timer.fraction * MAD_F_SCALE / MAD_TIMER_RESOLUTION;

    // ... der Rest der Funktion bleibt wie zuvor vorgeschlagen ...

    // Use a loop to handle the "fractional FPS" conversions iteratively
    while (true) {
        switch (units) {
            case MAD_UNITS_23_976_FPS:
            case MAD_UNITS_24_975_FPS:
            case MAD_UNITS_29_97_FPS: // Korrigierter Enum-Name
            case MAD_UNITS_47_952_FPS:
            case MAD_UNITS_49_95_FPS:
            case MAD_UNITS_59_94_FPS:
                // Apply the inverse transformation: X = (result * 1001 / 1000) - 1
                current_value = (current_value * 1001) / 1000;
                current_value -= MAD_F_ONE; // Subtract 1 (as fixed-point)
                units = (mad_units)(-units); // Convert negative unit to its positive base
                break; // Break from switch, continue while loop

            default:
                goto end_conversion_loop; // Exit the while loop
        }
    }
end_conversion_loop:;

    // Nun, 'units' sollte eine standardmäßige positive Einheit sein
    // und 'current_value' entsprechend angepasst.
    switch (units) {
        case MAD_UNITS_HOURS:      final_count = mad_fixed_to_long(current_value / (MAD_F_ONE * 3600)); break;
        case MAD_UNITS_MINUTES:    final_count = mad_fixed_to_long(current_value / (MAD_F_ONE * 60)); break;
        case MAD_UNITS_SECONDS:    final_count = mad_fixed_to_long(current_value / MAD_F_ONE); break;
        case MAD_UNITS_DECISECONDS:  final_count = mad_fixed_to_long(current_value * 10 / MAD_F_ONE); break; // Corrected: multiply by 10
        case MAD_UNITS_CENTISECONDS: final_count = mad_fixed_to_long(current_value * 100 / MAD_F_ONE); break; // Corrected: multiply by 100
        case MAD_UNITS_MILLISECONDS: final_count = mad_fixed_to_long(current_value * 1000 / MAD_F_ONE); break; // Corrected: multiply by 1000

        // Für die folgenden Fälle ist die Division durch mad_timer_d_base(units) korrekt.
        // Stellen Sie sicher, dass mad_timer_d_base() auch alle diese Fälle abdeckt und die korrekten Skalierungsfaktoren liefert.
        case MAD_UNITS_8000_HZ:
        case MAD_UNITS_11025_HZ:
        case MAD_UNITS_12000_HZ:
        case MAD_UNITS_16000_HZ:
        case MAD_UNITS_22050_HZ:
        case MAD_UNITS_24000_HZ:
        case MAD_UNITS_32000_HZ:
        case MAD_UNITS_44100_HZ:
        case MAD_UNITS_48000_HZ:
        case MAD_UNITS_24_FPS:
        case MAD_UNITS_25_FPS:
        case MAD_UNITS_30_FPS:
        case MAD_UNITS_48_FPS:
        case MAD_UNITS_50_FPS:
        case MAD_UNITS_60_FPS:
        case MAD_UNITS_75_FPS:
        case MAD_UNITS_FRAMES:
        case MAD_UNITS_SAMPLES:
        case MAD_UNITS_BITFRAMES:
        case MAD_UNITS_BYTES:
        case MAD_UNITS_PERCENT:
        case MAD_UNITS_HZ:
            final_count = mad_fixed_to_long(current_value / mad_timer_d_base(units));
            break;

        default:
            final_count = mad_fixed_to_long(current_value); break;
    }

    return final_count;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_timer_multiply(mad_timer_t* timer, int32_t scalar) { // multiply a timer by a scalar value
    mad_timer_t   addend;
    uint32_t factor;
    factor = scalar;
    if (scalar < 0) {
        factor = -scalar;
        mad_timer_negate(timer);
    }
    addend = *timer;
    *timer = mad_timer_zero;
    while (factor) {
        if (factor & 1) mad_timer_add(timer, addend);
        mad_timer_add(&addend, addend);
        factor >>= 1;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_timer_add(mad_timer_t* timer, mad_timer_t incr) { // add one timer to another
    timer->seconds += incr.seconds;
    timer->fraction += incr.fraction;

    if (timer->fraction >= MAD_TIMER_RESOLUTION) reduce_timer(timer);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_timer_set(mad_timer_t* timer, uint32_t seconds, uint32_t numer, uint32_t denom) { // set timer to specific (positive) value
    timer->seconds = seconds;
    if (numer >= denom && denom > 0) {
        timer->seconds += numer / denom;
        numer %= denom;
    }
    switch (denom) {
        case 0:
        case 1: timer->fraction = 0; break;
        case MAD_TIMER_RESOLUTION: timer->fraction = numer; break;
        case 1000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 1000); break;
        case 8000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 8000); break;
        case 11025: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 11025); break;
        case 12000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 12000); break;
        case 16000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 16000); break;
        case 22050: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 22050); break;
        case 24000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 24000); break;
        case 32000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 32000); break;
        case 44100: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 44100); break;
        case 48000: timer->fraction = numer * (MAD_TIMER_RESOLUTION / 48000); break;
        default: timer->fraction = scale_rational(numer, denom, MAD_TIMER_RESOLUTION); break;
    }
    if (timer->fraction >= MAD_TIMER_RESOLUTION) reduce_timer(timer);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t scale_rational(uint32_t numer, uint32_t denom, uint32_t scale) { // solve numer/denom == ?/scale avoiding overflowing
    reduce_rational(&numer, &denom);
    reduce_rational(&scale, &denom);
    assert(denom != 0);
    if (denom < scale) return numer * (scale / denom) + numer * (scale % denom) / denom;
    if (denom < numer) return scale * (numer / denom) + scale * (numer % denom) / denom;
    return numer * scale / denom;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void reduce_rational(uint32_t* numer, uint32_t* denom) { // convert rational expression to lowest terms
    uint32_t factor;
    factor = gcd(*numer, *denom);
    assert(factor != 0);
    *numer /= factor;
    *denom /= factor;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t gcd(uint32_t num1, uint32_t num2) { // compute greatest common denominator
    uint32_t tmp;
    while (num2) {
        tmp = num2;
        num2 = num1 % num2;
        num1 = tmp;
    }
    return num1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void reduce_timer(mad_timer_t* timer) { // carry timer fraction into seconds
    timer->seconds += timer->fraction / MAD_TIMER_RESOLUTION;
    timer->fraction %= MAD_TIMER_RESOLUTION;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
mad_timer_t mad_timer_abs(mad_timer_t timer) { // return the absolute value of a timer
    if (timer.seconds < 0) mad_timer_negate(&timer);
    return timer;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_timer_negate(mad_timer_t* timer) { // invert the sign of a timer
    timer->seconds = -timer->seconds;
    if (timer->fraction) {
        timer->seconds -= 1;
        timer->fraction = MAD_TIMER_RESOLUTION - timer->fraction;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_timer_compare(mad_timer_t timer1, mad_timer_t timer2) { // indicate relative order of two timers
    int32_t diff;
    diff = timer1.seconds - timer2.seconds;
    if (diff < 0)
        return -1;
    else if (diff > 0)
        return +1;
    diff = timer1.fraction - timer2.fraction;
    if (diff < 0)
        return -1;
    else if (diff > 0)
        return +1;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#include <esp_heap_caps.h> // Für die PSRAM-Allokation

// Angenommen, diese Makros sind in einer Header-Datei definiert, die du bereits verwendest
// #define MAD_F(x) ...
// #define MUL(a, b) ...
// #define SHIFT(x) ...

void dct32(int32_t const in[32], uint32_t slot, int32_t lo[16][8], int32_t hi[16][8]) { // perform fast in[32]->out[32] DCT
    // PSRAM-Allokation für das Array 't'
    ps_ptr<int32_t>t; t.alloc(177 * sizeof(int32_t));

    /* costab[i] = cos(PI / (2 * 32) * i) */
    #define costab1  MAD_F(0x0ffb10f2) /* 0.998795456 */
    #define costab2  MAD_F(0x0fec46d2) /* 0.995184727 */
    #define costab3  MAD_F(0x0fd3aac0) /* 0.989176510 */
    #define costab4  MAD_F(0x0fb14be8) /* 0.980785280 */
    #define costab5  MAD_F(0x0f853f7e) /* 0.970031253 */
    #define costab6  MAD_F(0x0f4fa0ab) /* 0.956940336 */
    #define costab7  MAD_F(0x0f109082) /* 0.941544065 */
    #define costab8  MAD_F(0x0ec835e8) /* 0.923879533 */
    #define costab9  MAD_F(0x0e76bd7a) /* 0.903989293 */
    #define costab10 MAD_F(0x0e1c5979) /* 0.881921264 */
    #define costab11 MAD_F(0x0db941a3) /* 0.857728610 */
    #define costab12 MAD_F(0x0d4db315) /* 0.831469612 */
    #define costab13 MAD_F(0x0cd9f024) /* 0.803207531 */
    #define costab14 MAD_F(0x0c5e4036) /* 0.773010453 */
    #define costab15 MAD_F(0x0bdaef91) /* 0.740951125 */
    #define costab16 MAD_F(0x0b504f33) /* 0.707106781 */
    #define costab17 MAD_F(0x0abeb49a) /* 0.671558955 */
    #define costab18 MAD_F(0x0a267993) /* 0.634393284 */
    #define costab19 MAD_F(0x0987fbfe) /* 0.595699304 */
    #define costab20 MAD_F(0x08e39d9d) /* 0.555570233 */
    #define costab21 MAD_F(0x0839c3cd) /* 0.514102744 */
    #define costab22 MAD_F(0x078ad74e) /* 0.471396737 */
    #define costab23 MAD_F(0x06d74402) /* 0.427555093 */
    #define costab24 MAD_F(0x061f78aa) /* 0.382683432 */
    #define costab25 MAD_F(0x0563e69d) /* 0.336889853 */
    #define costab26 MAD_F(0x04a5018c) /* 0.290284677 */
    #define costab27 MAD_F(0x03e33f2f) /* 0.242980180 */
    #define costab28 MAD_F(0x031f1708) /* 0.195090322 */
    #define costab29 MAD_F(0x0259020e) /* 0.146730474 */
    #define costab30 MAD_F(0x01917a6c) /* 0.098017140 */
    #define costab31 MAD_F(0x00c8fb30) /* 0.049067674 */

    t[0] = in[0] + in[31];
    t[16] = MUL(in[0] - in[31], costab1);
    t[1] = in[15] + in[16];
    t[17] = MUL(in[15] - in[16], costab31);
    t[41] = t[16] + t[17];
    t[59] = MUL(t[16] - t[17], costab2);
    t[33] = t[0] + t[1];
    t[50] = MUL(t[0] - t[1], costab2);
    t[2] = in[7] + in[24];
    t[18] = MUL(in[7] - in[24], costab15);
    t[3] = in[8] + in[23];
    t[19] = MUL(in[8] - in[23], costab17);
    t[42] = t[18] + t[19];
    t[60] = MUL(t[18] - t[19], costab30);
    t[34] = t[2] + t[3];
    t[51] = MUL(t[2] - t[3], costab30);
    t[4] = in[3] + in[28];
    t[20] = MUL(in[3] - in[28], costab7);
    t[5] = in[12] + in[19];
    t[21] = MUL(in[12] - in[19], costab25);
    t[43] = t[20] + t[21];
    t[61] = MUL(t[20] - t[21], costab14);
    t[35] = t[4] + t[5];
    t[52] = MUL(t[4] - t[5], costab14);
    t[6] = in[4] + in[27];
    t[22] = MUL(in[4] - in[27], costab9);
    t[7] = in[11] + in[20];
    t[23] = MUL(in[11] - in[20], costab23);
    t[44] = t[22] + t[23];
    t[62] = MUL(t[22] - t[23], costab18);
    t[36] = t[6] + t[7];
    t[53] = MUL(t[6] - t[7], costab18);
    t[8] = in[1] + in[30];
    t[24] = MUL(in[1] - in[30], costab3);
    t[9] = in[14] + in[17];
    t[25] = MUL(in[14] - in[17], costab29);
    t[45] = t[24] + t[25];
    t[63] = MUL(t[24] - t[25], costab6);
    t[37] = t[8] + t[9];
    t[54] = MUL(t[8] - t[9], costab6);
    t[10] = in[6] + in[25];
    t[26] = MUL(in[6] - in[25], costab13);
    t[11] = in[9] + in[22];
    t[27] = MUL(in[9] - in[22], costab19);
    t[46] = t[26] + t[27];
    t[64] = MUL(t[26] - t[27], costab26);
    t[38] = t[10] + t[11];
    t[55] = MUL(t[10] - t[11], costab26);
    t[12] = in[2] + in[29];
    t[28] = MUL(in[2] - in[29], costab5);
    t[13] = in[13] + in[18];
    t[29] = MUL(in[13] - in[18], costab27);
    t[47] = t[28] + t[29];
    t[65] = MUL(t[28] - t[29], costab10);
    t[39] = t[12] + t[13];
    t[56] = MUL(t[12] - t[13], costab10);
    t[14] = in[5] + in[26];
    t[30] = MUL(in[5] - in[26], costab11);
    t[15] = in[10] + in[21];
    t[31] = MUL(in[10] - in[21], costab21);
    t[48] = t[30] + t[31];
    t[66] = MUL(t[30] - t[31], costab22);
    t[40] = t[14] + t[15];
    t[57] = MUL(t[14] - t[15], costab22);
    t[69] = t[33] + t[34];
    t[89] = MUL(t[33] - t[34], costab4);
    t[70] = t[35] + t[36];
    t[90] = MUL(t[35] - t[36], costab28);
    t[71] = t[37] + t[38];
    t[91] = MUL(t[37] - t[38], costab12);
    t[72] = t[39] + t[40];
    t[92] = MUL(t[39] - t[40], costab20);
    t[73] = t[41] + t[42];
    t[94] = MUL(t[41] - t[42], costab4);
    t[74] = t[43] + t[44];
    t[95] = MUL(t[43] - t[44], costab28);
    t[75] = t[45] + t[46];
    t[96] = MUL(t[45] - t[46], costab12);
    t[76] = t[47] + t[48];
    t[97] = MUL(t[47] - t[48], costab20);
    t[78] = t[50] + t[51];
    t[100] = MUL(t[50] - t[51], costab4);
    t[79] = t[52] + t[53];
    t[101] = MUL(t[52] - t[53], costab28);
    t[80] = t[54] + t[55];
    t[102] = MUL(t[54] - t[55], costab12);
    t[81] = t[56] + t[57];
    t[103] = MUL(t[56] - t[57], costab20);
    t[83] = t[59] + t[60];
    t[106] = MUL(t[59] - t[60], costab4);
    t[84] = t[61] + t[62];
    t[107] = MUL(t[61] - t[62], costab28);
    t[85] = t[63] + t[64];
    t[108] = MUL(t[63] - t[64], costab12);
    t[86] = t[65] + t[66];
    t[109] = MUL(t[65] - t[66], costab20);
    t[113] = t[69] + t[70];
    t[114] = t[71] + t[72];
    /* 0 */ hi[15][slot] = SHIFT(t[113] + t[114]);
    /* 16 */ lo[0][slot] = SHIFT(MUL(t[113] - t[114], costab16));
    t[115] = t[73] + t[74];
    t[116] = t[75] + t[76];
    t[32] = t[115] + t[116];
    /* 1 */ hi[14][slot] = SHIFT(t[32]);
    t[118] = t[78] + t[79];
    t[119] = t[80] + t[81];
    t[58] = t[118] + t[119];
    /* 2 */ hi[13][slot] = SHIFT(t[58]);
    t[121] = t[83] + t[84];
    t[122] = t[85] + t[86];
    t[67] = t[121] + t[122];
    t[49] = (t[67] * 2) - t[32];
    /* 3 */ hi[12][slot] = SHIFT(t[49]);
    t[125] = t[89] + t[90];
    t[126] = t[91] + t[92];
    t[93] = t[125] + t[126];
    /* 4 */ hi[11][slot] = SHIFT(t[93]);
    t[128] = t[94] + t[95];
    t[129] = t[96] + t[97];
    t[98] = t[128] + t[129];
    t[68] = (t[98] * 2) - t[49];
    /* 5 */ hi[10][slot] = SHIFT(t[68]);
    t[132] = t[100] + t[101];
    t[133] = t[102] + t[103];
    t[104] = t[132] + t[133];
    t[82] = (t[104] * 2) - t[58];
    /* 6 */ hi[9][slot] = SHIFT(t[82]);
    t[136] = t[106] + t[107];
    t[137] = t[108] + t[109];
    t[110] = t[136] + t[137];
    t[87] = (t[110] * 2) - t[67];
    t[77] = (t[87] * 2) - t[68];
    /* 7 */ hi[8][slot] = SHIFT(t[77]);
    t[141] = MUL(t[69] - t[70], costab8);
    t[142] = MUL(t[71] - t[72], costab24);
    t[143] = t[141] + t[142];
    /* 8 */ hi[7][slot] = SHIFT(t[143]);
    /* 24 */ lo[8][slot] = SHIFT((MUL(t[141] - t[142], costab16) * 2) - t[143]);
    t[144] = MUL(t[73] - t[74], costab8);
    t[145] = MUL(t[75] - t[76], costab24);
    t[146] = t[144] + t[145];
    t[88] = (t[146] * 2) - t[77];
    /* 9 */ hi[6][slot] = SHIFT(t[88]);
    t[148] = MUL(t[78] - t[79], costab8);
    t[149] = MUL(t[80] - t[81], costab24);
    t[150] = t[148] + t[149];
    t[105] = (t[150] * 2) - t[82];
    /* 10 */ hi[5][slot] = SHIFT(t[105]);
    t[152] = MUL(t[83] - t[84], costab8);
    t[153] = MUL(t[85] - t[86], costab24);
    t[154] = t[152] + t[153];
    t[111] = (t[154] * 2) - t[87];
    t[99] = (t[111] * 2) - t[88];
    /* 11 */ hi[4][slot] = SHIFT(t[99]);
    t[157] = MUL(t[89] - t[90], costab8);
    t[158] = MUL(t[91] - t[92], costab24);
    t[159] = t[157] + t[158];
    t[127] = (t[159] * 2) - t[93];
    /* 12 */ hi[3][slot] = SHIFT(t[127]);
    t[160] = (MUL(t[125] - t[126], costab16) * 2) - t[127];
    /* 20 */ lo[4][slot] = SHIFT(t[160]);
    /* 28 */ lo[12][slot] = SHIFT((((MUL(t[157] - t[158], costab16) * 2) - t[159]) * 2) - t[160]);
    t[161] = MUL(t[94] - t[95], costab8);
    t[162] = MUL(t[96] - t[97], costab24);
    t[163] = t[161] + t[162];
    t[130] = (t[163] * 2) - t[98];
    t[112] = (t[130] * 2) - t[99];
    /* 13 */ hi[2][slot] = SHIFT(t[112]);
    t[164] = (MUL(t[128] - t[129], costab16) * 2) - t[130];
    t[166] = MUL(t[100] - t[101], costab8);
    t[167] = MUL(t[102] - t[103], costab24);
    t[168] = t[166] + t[167];
    t[134] = (t[168] * 2) - t[104];
    t[120] = (t[134] * 2) - t[105];
    /* 14 */ hi[1][slot] = SHIFT(t[120]);
    t[135] = (MUL(t[118] - t[119], costab16) * 2) - t[120];
    /* 18 */ lo[2][slot] = SHIFT(t[135]);
    t[169] = (MUL(t[132] - t[133], costab16) * 2) - t[134];
    t[151] = (t[169] * 2) - t[135];
    /* 22 */ lo[6][slot] = SHIFT(t[151]);
    t[170] = (((MUL(t[148] - t[149], costab16) * 2) - t[150]) * 2) - t[151];
    /* 26 */ lo[10][slot] = SHIFT(t[170]);
    /* 30 */ lo[14][slot] = SHIFT((((((MUL(t[166] - t[167], costab16) * 2) - t[168]) * 2) - t[169]) * 2) - t[170]);
    t[171] = MUL(t[106] - t[107], costab8);
    t[172] = MUL(t[108] - t[109], costab24);
    t[173] = t[171] + t[172];
    t[138] = (t[173] * 2) - t[110];
    t[123] = (t[138] * 2) - t[111];
    t[139] = (MUL(t[121] - t[122], costab16) * 2) - t[123];
    t[117] = (t[123] * 2) - t[112];
    /* 15 */ hi[0][slot] = SHIFT(t[117]);
    t[124] = (MUL(t[115] - t[116], costab16) * 2) - t[117];
    /* 17 */ lo[1][slot] = SHIFT(t[124]);
    t[131] = (t[139] * 2) - t[124];
    /* 19 */ lo[3][slot] = SHIFT(t[131]);
    t[140] = (t[164] * 2) - t[131];
    /* 21 */ lo[5][slot] = SHIFT(t[140]);
    t[174] = (MUL(t[136] - t[137], costab16) * 2) - t[138];
    t[155] = (t[174] * 2) - t[139];
    t[147] = (t[155] * 2) - t[140];
    /* 23 */ lo[7][slot] = SHIFT(t[147]);
    t[156] = (((MUL(t[144] - t[145], costab16) * 2) - t[146]) * 2) - t[147];
    /* 25 */ lo[9][slot] = SHIFT(t[156]);
    t[175] = (((MUL(t[152] - t[153], costab16) * 2) - t[154]) * 2) - t[155];
    t[165] = (t[175] * 2) - t[156];
    /* 27 */ lo[11][slot] = SHIFT(t[165]);
    t[176] = (((((MUL(t[161] - t[162], costab16) * 2) - t[163]) * 2) - t[164]) * 2) - t[165];
    /* 29 */ lo[13][slot] = SHIFT(t[176]);
    /* 31 */ lo[15][slot] = SHIFT((((((((MUL(t[171] - t[172], costab16) * 2) - t[173]) * 2) - t[174]) * 2) - t[175]) * 2) - t[176]);
    /*
     * Totals:
     * 80 multiplies
     * 80 additions
     * 119 subtractions
     * 49 shifts (not counting SSO)
     */

    // Speicher freigeben, wenn er nicht mehr benötigt wird
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void synth_full(mad_synth_t* synth, mad_frame_t const* frame, uint32_t nch, uint32_t ns) {
    uint32_t phase, ch, s, sb, pe, po;
    int32_t *    pcm1, *pcm2, (*filter)[2][2][16][8];
    int32_t const(*sbsample)[36][32];
    int32_t (*fe)[8], (*fx)[8], (*fo)[8];
    int32_t const(*Dptr)[32], *ptr;
    int32_t  hi;
    uint32_t lo;
    for (ch = 0; ch < nch; ++ch) {
        sbsample = reinterpret_cast<int32_t const(*)[36][32]>(s_sbsample.get_raw_slice_ptr(ch));
        filter = &(*s_filter)[ch];
        phase = synth->phase;
        pcm1 = *reinterpret_cast<int32_t(*)[1152]>(s_samplesBuff.get_raw_row_ptr(ch));
        for (s = 0; s < ns; ++s) {
            dct32((*sbsample)[s], phase >> 1, (*filter)[0][phase & 1], (*filter)[1][phase & 1]);
            pe = phase & ~1;
            po = ((phase - 1) & 0xf) | 1;
            /* calculate 32 samples */
            fe = &(*filter)[0][phase & 1][0];
            fx = &(*filter)[0][~phase & 1][0];
            fo = &(*filter)[1][~phase & 1][0];
            Dptr = &D[0];
            ptr = *Dptr + po;
            ML0_16(hi, lo, (*fx)[0], ptr[0]);
            MLA_16(hi, lo, (*fx)[1], ptr[14]);
            MLA_16(hi, lo, (*fx)[2], ptr[12]);
            MLA_16(hi, lo, (*fx)[3], ptr[10]);
            MLA_16(hi, lo, (*fx)[4], ptr[8]);
            MLA_16(hi, lo, (*fx)[5], ptr[6]);
            MLA_16(hi, lo, (*fx)[6], ptr[4]);
            MLA_16(hi, lo, (*fx)[7], ptr[2]);
            MLN(hi, lo);
            ptr = *Dptr + pe;
            MLA_16(hi, lo, (*fe)[0], ptr[0]);
            MLA_16(hi, lo, (*fe)[1], ptr[14]);
            MLA_16(hi, lo, (*fe)[2], ptr[12]);
            MLA_16(hi, lo, (*fe)[3], ptr[10]);
            MLA_16(hi, lo, (*fe)[4], ptr[8]);
            MLA_16(hi, lo, (*fe)[5], ptr[6]);
            MLA_16(hi, lo, (*fe)[6], ptr[4]);
            MLA_16(hi, lo, (*fe)[7], ptr[2]);
            *pcm1++ = SHIFT(MLZ(hi, lo));
            pcm2 = pcm1 + 30;
            for (sb = 1; sb < 16; ++sb) {
                ++fe;
                ++Dptr;
                /* D[32 - sb][i] == -D[sb][31 - i] */
                ptr = *Dptr + po;
                ML0_16(hi, lo, (*fo)[0], ptr[0]);
                MLA_16(hi, lo, (*fo)[1], ptr[14]);
                MLA_16(hi, lo, (*fo)[2], ptr[12]);
                MLA_16(hi, lo, (*fo)[3], ptr[10]);
                MLA_16(hi, lo, (*fo)[4], ptr[8]);
                MLA_16(hi, lo, (*fo)[5], ptr[6]);
                MLA_16(hi, lo, (*fo)[6], ptr[4]);
                MLA_16(hi, lo, (*fo)[7], ptr[2]);
                MLN(hi, lo);
                ptr = *Dptr + pe;
                MLA_16(hi, lo, (*fe)[7], ptr[2]);
                MLA_16(hi, lo, (*fe)[6], ptr[4]);
                MLA_16(hi, lo, (*fe)[5], ptr[6]);
                MLA_16(hi, lo, (*fe)[4], ptr[8]);
                MLA_16(hi, lo, (*fe)[3], ptr[10]);
                MLA_16(hi, lo, (*fe)[2], ptr[12]);
                MLA_16(hi, lo, (*fe)[1], ptr[14]);
                MLA_16(hi, lo, (*fe)[0], ptr[0]);
                *pcm1++ = SHIFT(MLZ(hi, lo));
                ptr = *Dptr - pe;
                ML0_16(hi, lo, (*fe)[0], ptr[31 - 16]);
                MLA_16(hi, lo, (*fe)[1], ptr[31 - 14]);
                MLA_16(hi, lo, (*fe)[2], ptr[31 - 12]);
                MLA_16(hi, lo, (*fe)[3], ptr[31 - 10]);
                MLA_16(hi, lo, (*fe)[4], ptr[31 - 8]);
                MLA_16(hi, lo, (*fe)[5], ptr[31 - 6]);
                MLA_16(hi, lo, (*fe)[6], ptr[31 - 4]);
                MLA_16(hi, lo, (*fe)[7], ptr[31 - 2]);
                ptr = *Dptr - po;
                MLA_16(hi, lo, (*fo)[7], ptr[31 - 2]);
                MLA_16(hi, lo, (*fo)[6], ptr[31 - 4]);
                MLA_16(hi, lo, (*fo)[5], ptr[31 - 6]);
                MLA_16(hi, lo, (*fo)[4], ptr[31 - 8]);
                MLA_16(hi, lo, (*fo)[3], ptr[31 - 10]);
                MLA_16(hi, lo, (*fo)[2], ptr[31 - 12]);
                MLA_16(hi, lo, (*fo)[1], ptr[31 - 14]);
                MLA_16(hi, lo, (*fo)[0], ptr[31 - 16]);
                *pcm2-- = SHIFT(MLZ(hi, lo));
                ++fo;
            }
            ++Dptr;
            ptr = *Dptr + po;
            ML0_16(hi, lo, (*fo)[0], ptr[0]);
            MLA_16(hi, lo, (*fo)[1], ptr[14]);
            MLA_16(hi, lo, (*fo)[2], ptr[12]);
            MLA_16(hi, lo, (*fo)[3], ptr[10]);
            MLA_16(hi, lo, (*fo)[4], ptr[8]);
            MLA_16(hi, lo, (*fo)[5], ptr[6]);
            MLA_16(hi, lo, (*fo)[6], ptr[4]);
            MLA_16(hi, lo, (*fo)[7], ptr[2]);
            *pcm1 = SHIFT(-MLZ(hi, lo));
            pcm1 += 16;
            phase = (phase + 1) % 16;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void synth_half(mad_synth_t* synth, mad_frame_t const* frame, uint32_t nch, uint32_t ns) { // perform half frequency PCM synthesis
    uint32_t phase, ch, s, sb, pe, po;
    int32_t *    pcm1, *pcm2, (*filter)[2][2][16][8];
    int32_t const(*sbsample)[36][32];
    int32_t (*fe)[8], (*fx)[8], (*fo)[8];
    int32_t const(*Dptr)[32], *ptr;
    int32_t  hi;
    uint32_t lo;
    for (ch = 0; ch < nch; ++ch) {
        sbsample = reinterpret_cast<int32_t const(*)[36][32]>(s_sbsample.get_raw_slice_ptr(ch));
        filter = &(*s_filter)[ch];
        phase = synth->phase;
        pcm1 = *reinterpret_cast<int32_t(*)[1152]>(s_samplesBuff.get_raw_row_ptr(ch));
        for (s = 0; s < ns; ++s) {
            dct32((*sbsample)[s], phase >> 1, (*filter)[0][phase & 1], (*filter)[1][phase & 1]);
            pe = phase & ~1;
            po = ((phase - 1) & 0xf) | 1;
            /* calculate 16 samples */
            fe = &(*filter)[0][phase & 1][0];
            fx = &(*filter)[0][~phase & 1][0];
            fo = &(*filter)[1][~phase & 1][0];
            Dptr = &D[0];
            ptr = *Dptr + po;
            ML0_16(hi, lo, (*fx)[0], ptr[0]);
            MLA_16(hi, lo, (*fx)[1], ptr[14]);
            MLA_16(hi, lo, (*fx)[2], ptr[12]);
            MLA_16(hi, lo, (*fx)[3], ptr[10]);
            MLA_16(hi, lo, (*fx)[4], ptr[8]);
            MLA_16(hi, lo, (*fx)[5], ptr[6]);
            MLA_16(hi, lo, (*fx)[6], ptr[4]);
            MLA_16(hi, lo, (*fx)[7], ptr[2]);
            MLN(hi, lo);
            ptr = *Dptr + pe;
            MLA_16(hi, lo, (*fe)[0], ptr[0]);
            MLA_16(hi, lo, (*fe)[1], ptr[14]);
            MLA_16(hi, lo, (*fe)[2], ptr[12]);
            MLA_16(hi, lo, (*fe)[3], ptr[10]);
            MLA_16(hi, lo, (*fe)[4], ptr[8]);
            MLA_16(hi, lo, (*fe)[5], ptr[6]);
            MLA_16(hi, lo, (*fe)[6], ptr[4]);
            MLA_16(hi, lo, (*fe)[7], ptr[2]);
            *pcm1++ = SHIFT(MLZ(hi, lo));
            pcm2 = pcm1 + 14;
            for (sb = 1; sb < 16; ++sb) {
                ++fe;
                ++Dptr;
                /* D[32 - sb][i] == -D[sb][31 - i] */
                if (!(sb & 1)) {
                    ptr = *Dptr + po;
                    ML0_16(hi, lo, (*fo)[0], ptr[0]);
                    MLA_16(hi, lo, (*fo)[1], ptr[14]);
                    MLA_16(hi, lo, (*fo)[2], ptr[12]);
                    MLA_16(hi, lo, (*fo)[3], ptr[10]);
                    MLA_16(hi, lo, (*fo)[4], ptr[8]);
                    MLA_16(hi, lo, (*fo)[5], ptr[6]);
                    MLA_16(hi, lo, (*fo)[6], ptr[4]);
                    MLA_16(hi, lo, (*fo)[7], ptr[2]);
                    MLN(hi, lo);
                    ptr = *Dptr + pe;
                    MLA_16(hi, lo, (*fe)[7], ptr[2]);
                    MLA_16(hi, lo, (*fe)[6], ptr[4]);
                    MLA_16(hi, lo, (*fe)[5], ptr[6]);
                    MLA_16(hi, lo, (*fe)[4], ptr[8]);
                    MLA_16(hi, lo, (*fe)[3], ptr[10]);
                    MLA_16(hi, lo, (*fe)[2], ptr[12]);
                    MLA_16(hi, lo, (*fe)[1], ptr[14]);
                    MLA_16(hi, lo, (*fe)[0], ptr[0]);
                    *pcm1++ = SHIFT(MLZ(hi, lo));
                    ptr = *Dptr - po;
                    ML0_16(hi, lo, (*fo)[7], ptr[31 - 2]);
                    MLA_16(hi, lo, (*fo)[6], ptr[31 - 4]);
                    MLA_16(hi, lo, (*fo)[5], ptr[31 - 6]);
                    MLA_16(hi, lo, (*fo)[4], ptr[31 - 8]);
                    MLA_16(hi, lo, (*fo)[3], ptr[31 - 10]);
                    MLA_16(hi, lo, (*fo)[2], ptr[31 - 12]);
                    MLA_16(hi, lo, (*fo)[1], ptr[31 - 14]);
                    MLA_16(hi, lo, (*fo)[0], ptr[31 - 16]);
                    ptr = *Dptr - pe;
                    MLA_16(hi, lo, (*fe)[0], ptr[31 - 16]);
                    MLA_16(hi, lo, (*fe)[1], ptr[31 - 14]);
                    MLA_16(hi, lo, (*fe)[2], ptr[31 - 12]);
                    MLA_16(hi, lo, (*fe)[3], ptr[31 - 10]);
                    MLA_16(hi, lo, (*fe)[4], ptr[31 - 8]);
                    MLA_16(hi, lo, (*fe)[5], ptr[31 - 6]);
                    MLA_16(hi, lo, (*fe)[6], ptr[31 - 4]);
                    MLA_16(hi, lo, (*fe)[7], ptr[31 - 2]);
                    *pcm2-- = SHIFT(MLZ(hi, lo));
                }
                ++fo;
            }
            ++Dptr;
            ptr = *Dptr + po;
            ML0_16(hi, lo, (*fo)[0], ptr[0]);
            MLA_16(hi, lo, (*fo)[1], ptr[14]);
            MLA_16(hi, lo, (*fo)[2], ptr[12]);
            MLA_16(hi, lo, (*fo)[3], ptr[10]);
            MLA_16(hi, lo, (*fo)[4], ptr[8]);
            MLA_16(hi, lo, (*fo)[5], ptr[6]);
            MLA_16(hi, lo, (*fo)[6], ptr[4]);
            MLA_16(hi, lo, (*fo)[7], ptr[2]);
            *pcm1 = SHIFT(-MLZ(hi, lo));
            pcm1 += 8;
            phase = (phase + 1) % 16;
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_synth_frame(mad_synth_t* synth, mad_frame_t const* frame) { //perform PCM synthesis of frame subband samples
    uint32_t nch, ns;
    void (*synth_frame)(mad_synth_t*, mad_frame_t const*, uint32_t, uint32_t);
    nch = MAD_NCHANNELS(&frame->header);
    ns = MAD_NSBSAMPLES(&frame->header);
    synth->samplerate = frame->header.samplerate;
    synth->channels = nch;
    synth->length = 32 * ns;
    synth_frame = synth_full;
    if (frame->options & MAD_OPTION_HALFSAMPLERATE) {
        synth->samplerate /= 2;
        synth->length /= 2;
        synth_frame = synth_half;
    }
    s_mad_out_samples = synth->length;
    if(nch == 1) s_mad_out_samples *= 2;
    s_mad_channels = nch;
 //   s_mad_sampleRate = synth->samplerate;
    synth_frame(synth, frame, nch, ns);
    synth->phase = (synth->phase + ns) % 16;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_synth_mute(mad_synth_t* synth) { // zero all polyphase filterbank values, resetting synthesis
    uint32_t ch, s, v;
    for (ch = 0; ch < 2; ++ch) {
        for (s = 0; s < 16; ++s) {
            for (v = 0; v < 8; ++v) { (*s_filter)[ch][0][0][s][v] = (*s_filter)[ch][0][1][s][v] = (*s_filter)[ch][1][0][s][v] = (*s_filter)[ch][1][1][s][v] = 0; }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_synth_init(mad_synth_t* synth) { // initialize synth struct
    mad_synth_mute(synth);
    synth->phase = 0;
    synth->samplerate = 0;
    synth->channels = 0;
    synth->length = 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
enum mad_error III_sideinfo(struct mad_bitptr* ptr, uint32_t nch, int32_t lsf, struct sideinfo* si, uint32_t* data_bitlen, uint32_t* priv_bitlen) {
    uint32_t   ngr, gr, ch, i;
    enum mad_error result = MAD_ERROR_NONE;
    *data_bitlen = 0;
    *priv_bitlen = lsf ? ((nch == 1) ? 1 : 2) : ((nch == 1) ? 5 : 3);
    si->main_data_begin = mad_bit_read(ptr, lsf ? 8 : 9);
    si->private_bits = mad_bit_read(ptr, *priv_bitlen);
    ngr = 1;
    if (!lsf) {
        ngr = 2;
        for (ch = 0; ch < nch; ++ch) si->scfsi[ch] = mad_bit_read(ptr, 4);
    }
    for (gr = 0; gr < ngr; ++gr) {
        struct  granule* granule = &si->gr[gr];
        for (ch = 0; ch < nch; ++ch) {
            struct channel* channel = &granule->ch[ch];
            channel->part2_3_length = mad_bit_read(ptr, 12);
            channel->big_values = mad_bit_read(ptr, 9);
            channel->global_gain = mad_bit_read(ptr, 8);
            channel->scalefac_compress = mad_bit_read(ptr, lsf ? 9 : 4);
            *data_bitlen += channel->part2_3_length;
            if (channel->big_values > 288 && result == 0){
                MP3_LOG_ERROR("bad big_values count");
                result = MAD_ERROR_BADBIGVALUES;
            }
            channel->flags = 0;
            /* window_switching_flag */
            if (mad_bit_read(ptr, 1)) {
                channel->block_type = mad_bit_read(ptr, 2);
                if (channel->block_type == 0 && result == 0){
                    MP3_LOG_ERROR("reserved block_type");
                    result = MAD_ERROR_BADBLOCKTYPE;
                }
                if (!lsf && channel->block_type == 2 && si->scfsi[ch] && result == 0){
                    MP3_LOG_ERROR("bad scalefactor selection info");
                    result = MAD_ERROR_BADSCFSI;
                }
                channel->region0_count = 7;
                channel->region1_count = 36;
                if (mad_bit_read(ptr, 1))
                    channel->flags |= mixed_block_flag;
                else if (channel->block_type == 2)
                    channel->region0_count = 8;
                for (i = 0; i < 2; ++i) channel->table_select[i] = mad_bit_read(ptr, 5);
                for (i = 0; i < 3; ++i) channel->subblock_gain[i] = mad_bit_read(ptr, 3);
            } else {
                channel->block_type = 0;
                for (i = 0; i < 3; ++i) channel->table_select[i] = mad_bit_read(ptr, 5);
                channel->region0_count = mad_bit_read(ptr, 4);
                channel->region1_count = mad_bit_read(ptr, 3);
            }
            /* [preflag,] scalefac_scale, count1table_select */
            channel->flags |= mad_bit_read(ptr, lsf ? 2 : 3);
        }
    }
    return result;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t III_scalefactors_lsf(struct mad_bitptr* ptr, struct channel* channel, struct channel* gr1ch, int32_t mode_extension) {
    struct mad_bitptr start;
    uint32_t      scalefac_compress, index, slen[4], part, n, i;
    uint8_t const*    nsfb;
    start = *ptr;
    scalefac_compress = channel->scalefac_compress;
    index = (channel->block_type == 2) ? ((channel->flags & mixed_block_flag) ? 2 : 1) : 0;
    if (!((mode_extension & I_STEREO) && gr1ch)) {
        if (scalefac_compress < 400) {
            slen[0] = (scalefac_compress >> 4) / 5;
            slen[1] = (scalefac_compress >> 4) % 5;
            slen[2] = (scalefac_compress % 16) >> 2;
            slen[3] = scalefac_compress % 4;
            nsfb = nsfb_table[0][index];
        } else if (scalefac_compress < 500) {
            scalefac_compress -= 400;
            slen[0] = (scalefac_compress >> 2) / 5;
            slen[1] = (scalefac_compress >> 2) % 5;
            slen[2] = scalefac_compress % 4;
            slen[3] = 0;
            nsfb = nsfb_table[1][index];
        } else {
            scalefac_compress -= 500;
            slen[0] = scalefac_compress / 3;
            slen[1] = scalefac_compress % 3;
            slen[2] = 0;
            slen[3] = 0;
            channel->flags |= preflag;
            nsfb = nsfb_table[2][index];
        }
        n = 0;
        for (part = 0; part < 4; ++part) {
            for (i = 0; i < nsfb[part]; ++i) channel->scalefac[n++] = mad_bit_read(ptr, slen[part]);
        }
        while (n < 39) channel->scalefac[n++] = 0;
    } else { /* (mode_extension & I_STEREO) && gr1ch (i.e. ch == 1) */
        scalefac_compress >>= 1;
        if (scalefac_compress < 180) {
            slen[0] = scalefac_compress / 36;
            slen[1] = (scalefac_compress % 36) / 6;
            slen[2] = (scalefac_compress % 36) % 6;
            slen[3] = 0;
            nsfb = nsfb_table[3][index];
        } else if (scalefac_compress < 244) {
            scalefac_compress -= 180;
            slen[0] = (scalefac_compress % 64) >> 4;
            slen[1] = (scalefac_compress % 16) >> 2;
            slen[2] = scalefac_compress % 4;
            slen[3] = 0;
            nsfb = nsfb_table[4][index];
        } else {
            scalefac_compress -= 244;
            slen[0] = scalefac_compress / 3;
            slen[1] = scalefac_compress % 3;
            slen[2] = 0;
            slen[3] = 0;
            nsfb = nsfb_table[5][index];
        }
        n = 0;
        for (part = 0; part < 4; ++part) {
            uint32_t max, is_pos;
            max = (1 << slen[part]) - 1;
            for (i = 0; i < nsfb[part]; ++i) {
                is_pos = mad_bit_read(ptr, slen[part]);
                channel->scalefac[n] = is_pos;
                gr1ch->scalefac[n++] = (is_pos == max);
            }
        }
        while (n < 39) {
            channel->scalefac[n] = 0;
            gr1ch->scalefac[n++] = 0; /* apparently not illegal */
        }
    }
    return mad_bit_length(&start, ptr);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t III_scalefactors(struct mad_bitptr* ptr, struct channel* channel, struct channel const* gr0ch, uint32_t scfsi) {
    struct mad_bitptr start;
    uint32_t      slen1, slen2, sfbi;
    start = *ptr;
    slen1 = sflen_table[channel->scalefac_compress].slen1;
    slen2 = sflen_table[channel->scalefac_compress].slen2;
    if (channel->block_type == 2) {
        uint32_t nsfb;
        sfbi = 0;
        nsfb = (channel->flags & mixed_block_flag) ? 8 + 3 * 3 : 6 * 3;
        while (nsfb--) channel->scalefac[sfbi++] = mad_bit_read(ptr, slen1);
        nsfb = 6 * 3;
        while (nsfb--) channel->scalefac[sfbi++] = mad_bit_read(ptr, slen2);
        nsfb = 1 * 3;
        while (nsfb--) channel->scalefac[sfbi++] = 0;
    } else { /* channel->block_type != 2 */
        if (scfsi & 0x8) {
            for (sfbi = 0; sfbi < 6; ++sfbi) channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
        } else {
            for (sfbi = 0; sfbi < 6; ++sfbi) channel->scalefac[sfbi] = mad_bit_read(ptr, slen1);
        }
        if (scfsi & 0x4) {
            for (sfbi = 6; sfbi < 11; ++sfbi) channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
        } else {
            for (sfbi = 6; sfbi < 11; ++sfbi) channel->scalefac[sfbi] = mad_bit_read(ptr, slen1);
        }
        if (scfsi & 0x2) {
            for (sfbi = 11; sfbi < 16; ++sfbi) channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
        } else {
            for (sfbi = 11; sfbi < 16; ++sfbi) channel->scalefac[sfbi] = mad_bit_read(ptr, slen2);
        }
        if (scfsi & 0x1) {
            for (sfbi = 16; sfbi < 21; ++sfbi) channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
        } else {
            for (sfbi = 16; sfbi < 21; ++sfbi) channel->scalefac[sfbi] = mad_bit_read(ptr, slen2);
        }
        channel->scalefac[21] = 0;
    }
    return mad_bit_length(&start, ptr);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_exponents(struct channel const* channel, uint8_t const* sfbwidth, int32_t exponents[39]) {
    int32_t   gain;
    uint32_t scalefac_multiplier, sfbi;
    gain = (int32_t)channel->global_gain - 210;
    scalefac_multiplier = (channel->flags & scalefac_scale) ? 2 : 1;
    if (channel->block_type == 2) {
        uint32_t l;
        int32_t   gain0, gain1, gain2;
        sfbi = l = 0;
        if (channel->flags & mixed_block_flag) {
            uint32_t premask;
            premask = (channel->flags & preflag) ? ~0 : 0;
            /* long block subbands 0-1 */
            while (l < 36) {
                exponents[sfbi] = gain - (int32_t)((channel->scalefac[sfbi] + (pretab[sfbi] & premask)) << scalefac_multiplier);

                l += sfbwidth[sfbi++];
            }
        }
        /* this is probably wrong for 8000 Hz short/mixed blocks */
        gain0 = gain - 8 * (int32_t)channel->subblock_gain[0];
        gain1 = gain - 8 * (int32_t)channel->subblock_gain[1];
        gain2 = gain - 8 * (int32_t)channel->subblock_gain[2];
        while (l < 576) {
            exponents[sfbi + 0] = gain0 - (int32_t)(channel->scalefac[sfbi + 0] << scalefac_multiplier);
            exponents[sfbi + 1] = gain1 - (int32_t)(channel->scalefac[sfbi + 1] << scalefac_multiplier);
            exponents[sfbi + 2] = gain2 - (int32_t)(channel->scalefac[sfbi + 2] << scalefac_multiplier);
            l += 3 * sfbwidth[sfbi];
            sfbi += 3;
        }
    } else { /* channel->block_type != 2 */
        if (channel->flags & preflag) {
            for (sfbi = 0; sfbi < 22; ++sfbi) { exponents[sfbi] = gain - (int32_t)((channel->scalefac[sfbi] + pretab[sfbi]) << scalefac_multiplier); }
        } else {
            for (sfbi = 0; sfbi < 22; ++sfbi) { exponents[sfbi] = gain - (int32_t)(channel->scalefac[sfbi] << scalefac_multiplier); }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t III_requantize(uint32_t value, int32_t exp) {
    int32_t                  requantized;
    int32_t               frac;
    struct fixedfloat const* power;
    frac = exp % 4; /* assumes sign(frac) == sign(exp) */
    exp /= 4;
    power = &rq_table[value];
    requantized = power->mantissa;
    exp += power->exponent;
    if (exp < 0) {
        if (-exp >= sizeof(int32_t) * CHAR_BIT) {
            /* underflow */
            requantized = 0;
        } else {
            requantized += 1L << (-exp - 1);
            requantized >>= -exp;
        }
    } else {
        if (exp >= 5) {
            /* overflow */
            requantized = MAD_F_MAX;
        } else
            requantized <<= exp;
    }
    return frac ? mad_f_mul(requantized, root_table[3 + frac]) : requantized;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
enum mad_error III_huffdecode(struct mad_bitptr* ptr, int32_t xr[576], struct channel* channel, uint8_t const* sfbwidth, uint32_t part2_length) {
    int32_t             exponents[39], exp;
    int32_t const*      expptr;
    struct mad_bitptr      peek;
    int32_t             bits_left, cachesz;
    int32_t*      xrptr;
    int32_t const*         sfbound;
    uint32_t bitcache;

    bits_left = (signed)channel->part2_3_length - (signed)part2_length;
    if (bits_left < 0){
        MP3_LOG_ERROR("bad audio data length");
        return MAD_ERROR_BADPART3LEN;
    }
    III_exponents(channel, sfbwidth, exponents);
    peek = *ptr;
    mad_bit_skip(ptr, bits_left);
    /* align bit reads to byte boundaries */
    cachesz = mad_bit_bitsleft(&peek);
    cachesz += ((32 - 1 - 24) + (24 - cachesz)) & ~7;
    bitcache = mad_bit_read(&peek, cachesz);
    bits_left -= cachesz;
    xrptr = &xr[0];
    /* big_values */
    {
        uint32_t            region, rcount;
        struct hufftable const* entry;
        union huffpair const*   table;
        uint32_t            linbits, startbits, big_values, reqhits;
        int32_t                 reqcache[16];
        sfbound = xrptr + *sfbwidth++;
        rcount = channel->region0_count + 1;
        entry = &mad_huff_pair_table[channel->table_select[region = 0]];
        table = entry->table;
        linbits = entry->linbits;
        startbits = entry->startbits;
        if (table == 0){
            MP3_LOG_ERROR("bad Huffman table select");
            return MAD_ERROR_BADHUFFTABLE;
        }
        expptr = &exponents[0];
        exp = *expptr++;
        reqhits = 0;
        big_values = channel->big_values;
        while (big_values-- && cachesz + bits_left > 0) {
            union huffpair const* pair;
            uint32_t          clumpsz, value;
            int32_t      requantized;
            if (xrptr == sfbound) {
                sfbound += *sfbwidth++;
                /* change table if region boundary */
                if (--rcount == 0) {
                    if (region == 0)
                        rcount = channel->region1_count + 1;
                    else
                        rcount = 0; /* all remaining */

                    entry = &mad_huff_pair_table[channel->table_select[++region]];
                    table = entry->table;
                    linbits = entry->linbits;
                    startbits = entry->startbits;
                    if (table == 0){
                        MP3_LOG_ERROR("bad Huffman table select");
                        return MAD_ERROR_BADHUFFTABLE;
                    }
                }
                if (exp != *expptr) {
                    exp = *expptr;
                    reqhits = 0;
                }
                ++expptr;
            }
            if (cachesz < 21) {
                uint32_t bits;
                bits = ((32 - 1 - 21) + (21 - cachesz)) & ~7;
                bitcache = (bitcache << bits) | mad_bit_read(&peek, bits);
                cachesz += bits;
                bits_left -= bits;
            }
            /* hcod (0..19) */
            clumpsz = startbits;
            pair = &table[MASK(bitcache, cachesz, clumpsz)];
            while (!pair->final) {
                cachesz -= clumpsz;

                clumpsz = pair->ptr.bits;
                pair = &table[pair->ptr.offset + MASK(bitcache, cachesz, clumpsz)];
            }
            cachesz -= pair->value.hlen;
            if (linbits) {
                /* x (0..14) */
                value = pair->value.x;
                switch (value) {
                    case 0: xrptr[0] = 0; break;
                    case 15:
                        if (cachesz < linbits + 2) {
                            bitcache = (bitcache << 16) | mad_bit_read(&peek, 16);
                            cachesz += 16;
                            bits_left -= 16;
                        }
                        value += MASK(bitcache, cachesz, linbits);
                        cachesz -= linbits;
                        requantized = III_requantize(value, exp);
                        goto x_final;
                    default:
                        if (reqhits & (1 << value))
                            requantized = reqcache[value];
                        else {
                            reqhits |= (1 << value);
                            requantized = reqcache[value] = III_requantize(value, exp);
                        }
                    x_final:
                        xrptr[0] = MASK1BIT(bitcache, cachesz--) ? -requantized : requantized;
                }
                /* y (0..14) */
                value = pair->value.y;
                switch (value) {
                    case 0: xrptr[1] = 0; break;
                    case 15:
                        if (cachesz < linbits + 1) {
                            bitcache = (bitcache << 16) | mad_bit_read(&peek, 16);
                            cachesz += 16;
                            bits_left -= 16;
                        }
                        value += MASK(bitcache, cachesz, linbits);
                        cachesz -= linbits;
                        requantized = III_requantize(value, exp);
                        goto y_final;
                    default:
                        if (reqhits & (1 << value))
                            requantized = reqcache[value];
                        else {
                            reqhits |= (1 << value);
                            requantized = reqcache[value] = III_requantize(value, exp);
                        }
                    y_final:
                        xrptr[1] = MASK1BIT(bitcache, cachesz--) ? -requantized : requantized;
                }
            } else {
                /* x (0..1) */
                value = pair->value.x;
                if (value == 0)
                    xrptr[0] = 0;
                else {
                    if (reqhits & (1 << value))
                        requantized = reqcache[value];
                    else {
                        reqhits |= (1 << value);
                        requantized = reqcache[value] = III_requantize(value, exp);
                    }
                    xrptr[0] = MASK1BIT(bitcache, cachesz--) ? -requantized : requantized;
                }
                /* y (0..1) */
                value = pair->value.y;
                if (value == 0)
                    xrptr[1] = 0;
                else {
                    if (reqhits & (1 << value))
                        requantized = reqcache[value];
                    else {
                        reqhits |= (1 << value);
                        requantized = reqcache[value] = III_requantize(value, exp);
                    }

                    xrptr[1] = MASK1BIT(bitcache, cachesz--) ? -requantized : requantized;
                }
            }
            xrptr += 2;
        }
    }
    if (cachesz + bits_left < 0){
        MP3_LOG_ERROR("Huffman data overrun");
        return MAD_ERROR_BADHUFFDATA; /* big_values overrun */
    }
    /* count1 */
    {
        union huffquad const* table;
        int32_t      requantized;
        table = mad_huff_quad_table[channel->flags & count1table_select];
        requantized = III_requantize(1, exp);
        while (cachesz + bits_left > 0 && xrptr <= &xr[572]) {
            union huffquad const* quad;
            /* hcod (1..6) */
            if (cachesz < 10) {
                bitcache = (bitcache << 16) | mad_bit_read(&peek, 16);
                cachesz += 16;
                bits_left -= 16;
            }
            quad = &table[MASK(bitcache, cachesz, 4)];
            /* quad tables guaranteed to have at most one extra lookup */
            if (!quad->final) {
                cachesz -= 4;
                quad = &table[quad->ptr.offset + MASK(bitcache, cachesz, quad->ptr.bits)];
            }
            cachesz -= quad->value.hlen;
            if (xrptr == sfbound) {
                sfbound += *sfbwidth++;
                if (exp != *expptr) {
                    exp = *expptr;
                    requantized = III_requantize(1, exp);
                }
                ++expptr;
            }
            /* v (0..1) */
            xrptr[0] = quad->value.v ? (MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;
            /* w (0..1) */
            xrptr[1] = quad->value.w ? (MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;
            xrptr += 2;
            if (xrptr == sfbound) {
                sfbound += *sfbwidth++;
                if (exp != *expptr) {
                    exp = *expptr;
                    requantized = III_requantize(1, exp);
                }
                ++expptr;
            }
            /* x (0..1) */
            xrptr[0] = quad->value.x ? (MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;
            /* y (0..1) */
            xrptr[1] = quad->value.y ? (MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;
            xrptr += 2;
        }
        if (cachesz + bits_left < 0) {
            /* technically the bitstream is misformatted, but apparently
               some encoders are just a bit sloppy with stuffing bits */

            xrptr -= 4;
        }
    }
    assert(-bits_left <= MAD_BUFFER_GUARD * CHAR_BIT);
    /* rzero */
    while (xrptr < &xr[576]) {
        xrptr[0] = 0;
        xrptr[1] = 0;

        xrptr += 2;
    }
    return MAD_ERROR_NONE;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_reorder(int32_t xr[576], struct channel const* channel, uint8_t const sfbwidth[39]) {

    ps_array3d<int32_t> tmp;
    tmp.alloc(32, 3, 6); // tmp[sbw][w][sw] -> 32 ist dim1, 3 ist dim2, 6 ist dim3

    if (!tmp.is_allocated()) { // Handle Error: PSRAM allocation failed
        return;
    }

    uint32_t sb, l, f, w, sbw[3], sw[3];

    // Correct handling of the Const Sfbwidth pointer by a local non-controversy pointer is common practice if a const point is shown on data that you want to be.
    uint8_t *sfb = (uint8_t *)sfbwidth;

    sb = 0;
    if (channel->flags & mixed_block_flag) {
        sb = 2;
        l = 0;
        while (l < 36) {
            l += *sfb++; // increment local ptr
        }
    }

    for (w = 0; w < 3; ++w) {
        sbw[w] = sb;
        sw[w] = 0;
    }

    f = *sfb++;
    w = 0;
    for (l = 18 * sb; l < 576; ++l) {
        if (f-- == 0) {
            f = *sfb++ - 1;
            w = (w + 1) % 3;
        }
        tmp(sbw[w], w, sw[w]++) = xr[l];
        if (sw[w] == 6) {
            sw[w] = 0;
            ++sbw[w];
        }
    }
    memcpy(&xr[18 * sb], tmp.get_raw_slice_ptr(sb), (576 - 18 * sb) * sizeof(int32_t));
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
enum mad_error III_stereo(int32_t xr[2][576], struct granule const* granule, struct mad_header* header, uint8_t const* sfbwidth_param) {
    ps_ptr<int16_t> modes;
    modes.alloc(39 * sizeof(int16_t)); // Allokiert 39 int16_t im PSRAM

    if (!modes.valid()) {
        MP3_LOG_ERROR("incompatible block_type for JS");
        return MAD_ERROR_BADSTEREO;
    }
    uint8_t *sfb = (uint8_t *)sfbwidth_param;

    uint32_t sfbi_counter;
    uint32_t l, n, i;

    if (granule->ch[0].block_type != granule->ch[1].block_type || (granule->ch[0].flags & mixed_block_flag) != (granule->ch[1].flags & mixed_block_flag)){
        MP3_LOG_ERROR("incompatible block_type for JS");
        return MAD_ERROR_BADSTEREO;
    }

    for (i = 0; i < 39; ++i) {
        modes[i] = header->mode_extension;
    }

    /* intensity stereo */
    if (header->mode_extension & I_STEREO) {
        struct channel const* right_ch = &granule->ch[1];
        int32_t const* right_xr = xr[1];
        uint32_t          is_pos;
        header->flags |= MAD_FLAG_I_STEREO;

        /* first determine which scalefactor bands are to be processed */
        if (right_ch->block_type == 2) {
            uint32_t lower, start, max, bound[3], w;
            lower = start = max = bound[0] = bound[1] = bound[2] = 0;
            l = 0;
            sfbi_counter = 0;
            if (right_ch->flags & mixed_block_flag) {
                while (l < 36) {
                    n = *sfb++; // <--- increment sfb pointer
                    for (i = 0; i < n; ++i) {
                        if (right_xr[i]) {
                            //lower = sfbi; // not sfbi, but sfbi_counter
                            lower = sfbi_counter;
                            break;
                        }
                    }
                    right_xr += n;
                    l += n;
                    sfbi_counter++;
                }
                start = sfbi_counter;
            }
            w = 0;
            while (l < 576) {
                n = *sfb++;
                for (i = 0; i < n; ++i) {
                    if (right_xr[i]) {
                        //max = bound[w] = sfbi; // not sfbi
                        max = bound[w] = sfbi_counter;
                        break;
                    }
                }
                right_xr += n;
                l += n;
                w = (w + 1) % 3;
                sfbi_counter++;
            }
            if (max) lower = start;
            /* long blocks */
            for (i = 0; i < lower; ++i) modes[i] = header->mode_extension & ~I_STEREO;
            /* short blocks */
            w = 0;
            for (i = start; i < max; ++i) {
                if (i < bound[w]) modes[i] = header->mode_extension & ~I_STEREO;

                w = (w + 1) % 3;
            }
        } else { /* right_ch->block_type != 2 */
            uint32_t bound;
            bound = 0;
            // sfbi = l = 0; // sfbi is now sfbi_counter
            sfbi_counter = l = 0; // reset
            for (l = 0; l < 576; l += n) {
                n = *sfb++; // <--- sfb inc
                for (i = 0; i < n; ++i) {
                    if (right_xr[i]) {
                        bound = sfbi_counter;
                        break;
                    }
                }
                right_xr += n;
                sfbi_counter++;
            }
            for (i = 0; i < bound; ++i) modes[i] = header->mode_extension & ~I_STEREO;
        }

        /* now do the actual processing */
        // Init for "actual processing"-loop
        l = 0; // total-Sample-Pos

        for (sfbi_counter = 0; sfbi_counter < 39; ++sfbi_counter) {
            n = sfbwidth_param[sfbi_counter];
            if (l + n > 576) {
                          break;
            }

            if (!(modes[sfbi_counter] & I_STEREO)) {
                l += n;
                continue;
            }

            if (header->flags & MAD_FLAG_LSF_EXT) {
                uint8_t const* illegal_pos = granule[1].ch[1].scalefac;
                int32_t const* lsf_scale;
                /* intensity_scale */
                lsf_scale = is_lsf_table[right_ch->scalefac_compress & 0x1];

                if (illegal_pos[sfbi_counter]) { // sfbi_counter als Idx
                    modes[sfbi_counter] &= ~I_STEREO;
                    l += n; // Weiter zum nächsten Block
                    continue;
                }
                is_pos = right_ch->scalefac[sfbi_counter];
                for (i = 0; i < n; ++i) {
                    int32_t left;
                    left = xr[0][l + i];
                    if (is_pos == 0)
                        xr[1][l + i] = left;
                    else {
                        int32_t opposite;
                        opposite = mad_f_mul(left, lsf_scale[(is_pos - 1) / 2]);
                        if (is_pos & 1) {
                            xr[0][l + i] = opposite;
                            xr[1][l + i] = left;
                        } else
                            xr[1][l + i] = opposite;
                    }
                }
            } else { /* !(header->flags & MAD_FLAG_LSF_EXT) */
                is_pos = right_ch->scalefac[sfbi_counter];
                if (is_pos >= 7) { /* illegal intensity position */
                    modes[sfbi_counter] &= ~I_STEREO;
                    l += n;
                    continue;
                }
                for (i = 0; i < n; ++i) {
                    int32_t left;
                    left = xr[0][l + i];
                    xr[0][l + i] = mad_f_mul(left, is_table[is_pos]);
                    xr[1][l + i] = mad_f_mul(left, is_table[6 - is_pos]);
                }
            }
            l += n;
        }
    }

    /* middle/side stereo */
    if (header->mode_extension & MS_STEREO) {
        int32_t invsqrt2;
        header->flags |= MAD_FLAG_MS_STEREO;
        invsqrt2 = root_table[3 + -2];
        l = 0;
        for (sfbi_counter = 0; sfbi_counter < 39; ++sfbi_counter) {
            n = sfbwidth_param[sfbi_counter];
            if (l + n > 576) {
                break;
            }
            if (modes[sfbi_counter] != MS_STEREO) {
                l += n;
                continue;
            }
            for (i = 0; i < n; ++i) {
                int32_t m, s;
                m = xr[0][l + i];
                s = xr[1][l + i];
                xr[0][l + i] = mad_f_mul(m + s, invsqrt2); /* l = (m + s) / sqrt(2) */
                xr[1][l + i] = mad_f_mul(m - s, invsqrt2); /* r = (m - s) / sqrt(2) */
            }
            l += n; // Incremented lip at the end of the loop
        }
    }
    return MAD_ERROR_NONE;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_aliasreduce(int32_t xr[576], int32_t lines) {
    int32_t const* bound;
    int32_t            i;
    bound = &xr[lines];
    for (xr += 18; xr < bound; xr += 18) {
        for (i = 0; i < 8; ++i) {
            int32_t  a, b;
            int32_t  hi;
            uint32_t lo;
            a = xr[-1 - i];
            b = xr[i];
                MAD_F_ML0(hi, lo, a, cs[i]);
                MAD_F_MLA(hi, lo, -b, ca[i]);
                xr[-1 - i] = MAD_F_MLZ(hi, lo);
                MAD_F_ML0(hi, lo, b, cs[i]);
                MAD_F_MLA(hi, lo, a, ca[i]);
                xr[i] = MAD_F_MLZ(hi, lo);
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void imdct36(int32_t const X[18], int32_t x[36]) {
    int32_t           t0, t1, t2, t3, t4, t5, t6, t7;
    int32_t           t8, t9, t10, t11, t12, t13, t14, t15;
    int32_t  hi;
    uint32_t lo;
    MAD_F_ML0(hi, lo, X[4], MAD_F(0x0ec835e8));
    MAD_F_MLA(hi, lo, X[13], MAD_F(0x061f78aa));
    t6 = MAD_F_MLZ(hi, lo);
    MAD_F_MLA(hi, lo, (t14 = X[1] - X[10]), -MAD_F(0x061f78aa));
    MAD_F_MLA(hi, lo, (t15 = X[7] + X[16]), -MAD_F(0x0ec835e8));
    t0 = MAD_F_MLZ(hi, lo);
    MAD_F_MLA(hi, lo, (t8 = X[0] - X[11] - X[12]), MAD_F(0x0216a2a2));
    MAD_F_MLA(hi, lo, (t9 = X[2] - X[9] - X[14]), MAD_F(0x09bd7ca0));
    MAD_F_MLA(hi, lo, (t10 = X[3] - X[8] - X[15]), -MAD_F(0x0cb19346));
    MAD_F_MLA(hi, lo, (t11 = X[5] - X[6] - X[17]), -MAD_F(0x0fdcf549));
    x[7] = MAD_F_MLZ(hi, lo);
    x[10] = -x[7];
    MAD_F_ML0(hi, lo, t8, -MAD_F(0x0cb19346));
    MAD_F_MLA(hi, lo, t9, MAD_F(0x0fdcf549));
    MAD_F_MLA(hi, lo, t10, MAD_F(0x0216a2a2));
    MAD_F_MLA(hi, lo, t11, -MAD_F(0x09bd7ca0));
    x[19] = x[34] = MAD_F_MLZ(hi, lo) - t0;
    t12 = X[0] - X[3] + X[8] - X[11] - X[12] + X[15];
    t13 = X[2] + X[5] - X[6] - X[9] - X[14] - X[17];
    MAD_F_ML0(hi, lo, t12, -MAD_F(0x0ec835e8));
    MAD_F_MLA(hi, lo, t13, MAD_F(0x061f78aa));
    x[22] = x[31] = MAD_F_MLZ(hi, lo) + t0;
    MAD_F_ML0(hi, lo, X[1], -MAD_F(0x09bd7ca0));
    MAD_F_MLA(hi, lo, X[7], MAD_F(0x0216a2a2));
    MAD_F_MLA(hi, lo, X[10], -MAD_F(0x0fdcf549));
    MAD_F_MLA(hi, lo, X[16], MAD_F(0x0cb19346));
    t1 = MAD_F_MLZ(hi, lo) + t6;
    MAD_F_ML0(hi, lo, X[0], MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[3], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[5], -MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[6], MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[8], -MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[9], MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[11], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[15], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0f9ee890));
    x[6] = MAD_F_MLZ(hi, lo) + t1;
    x[11] = -x[6];
    MAD_F_ML0(hi, lo, X[0], -MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[2], -MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[3], MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[5], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[6], MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[8], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[9], -MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[15], MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[17], MAD_F(0x04cfb0e2));
    x[23] = x[30] = MAD_F_MLZ(hi, lo) + t1;
    MAD_F_ML0(hi, lo, X[0], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[3], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[5], MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[6], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[8], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[9], -MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[11], MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[15], MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0acf37ad));
    x[18] = x[35] = MAD_F_MLZ(hi, lo) - t1;
    MAD_F_ML0(hi, lo, X[4], MAD_F(0x061f78aa));
    MAD_F_MLA(hi, lo, X[13], -MAD_F(0x0ec835e8));
    t7 = MAD_F_MLZ(hi, lo);
    MAD_F_MLA(hi, lo, X[1], -MAD_F(0x0cb19346));
    MAD_F_MLA(hi, lo, X[7], MAD_F(0x0fdcf549));
    MAD_F_MLA(hi, lo, X[10], MAD_F(0x0216a2a2));
    MAD_F_MLA(hi, lo, X[16], -MAD_F(0x09bd7ca0));
    t2 = MAD_F_MLZ(hi, lo);
    MAD_F_MLA(hi, lo, X[0], MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[3], -MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[5], MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[6], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[8], -MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[9], MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[12], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[15], MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[17], MAD_F(0x0f426cb5));
    x[5] = MAD_F_MLZ(hi, lo);
    x[12] = -x[5];
    MAD_F_ML0(hi, lo, X[0], MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[2], -MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[3], MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[5], -MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[6], -MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[8], MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[9], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[11], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[15], MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0bcbe352));
    x[0] = MAD_F_MLZ(hi, lo) + t2;
    x[17] = -x[0];
    MAD_F_ML0(hi, lo, X[0], -MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[2], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[3], -MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[5], MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[6], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[8], MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[9], MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[14], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[15], -MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x03768962));
    x[24] = x[29] = MAD_F_MLZ(hi, lo) + t2;
    MAD_F_ML0(hi, lo, X[1], -MAD_F(0x0216a2a2));
    MAD_F_MLA(hi, lo, X[7], -MAD_F(0x09bd7ca0));
    MAD_F_MLA(hi, lo, X[10], MAD_F(0x0cb19346));
    MAD_F_MLA(hi, lo, X[16], MAD_F(0x0fdcf549));
    t3 = MAD_F_MLZ(hi, lo) + t7;
    MAD_F_ML0(hi, lo, X[0], MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[3], -MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[5], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[6], MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[8], MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[9], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[12], MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[15], -MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0ffc19fd));
    x[8] = MAD_F_MLZ(hi, lo) + t3;
    x[9] = -x[8];
    MAD_F_ML0(hi, lo, X[0], -MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[3], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[5], -MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[6], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[8], MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[9], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[14], -MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[15], MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[17], MAD_F(0x07635284));
    x[21] = x[32] = MAD_F_MLZ(hi, lo) + t3;
    MAD_F_ML0(hi, lo, X[0], -MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[3], MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[5], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[6], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[8], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[9], MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[12], MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[15], -MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0898c779));
    x[20] = x[33] = MAD_F_MLZ(hi, lo) - t3;
    MAD_F_ML0(hi, lo, t14, -MAD_F(0x0ec835e8));
    MAD_F_MLA(hi, lo, t15, MAD_F(0x061f78aa));
    t4 = MAD_F_MLZ(hi, lo) - t7;
    MAD_F_ML0(hi, lo, t12, MAD_F(0x061f78aa));
    MAD_F_MLA(hi, lo, t13, MAD_F(0x0ec835e8));
    x[4] = MAD_F_MLZ(hi, lo) + t4;
    x[13] = -x[4];
    MAD_F_ML0(hi, lo, t8, MAD_F(0x09bd7ca0));
    MAD_F_MLA(hi, lo, t9, -MAD_F(0x0216a2a2));
    MAD_F_MLA(hi, lo, t10, MAD_F(0x0fdcf549));
    MAD_F_MLA(hi, lo, t11, -MAD_F(0x0cb19346));
    x[1] = MAD_F_MLZ(hi, lo) + t4;
    x[16] = -x[1];
    MAD_F_ML0(hi, lo, t8, -MAD_F(0x0fdcf549));
    MAD_F_MLA(hi, lo, t9, -MAD_F(0x0cb19346));
    MAD_F_MLA(hi, lo, t10, -MAD_F(0x09bd7ca0));
    MAD_F_MLA(hi, lo, t11, -MAD_F(0x0216a2a2));
    x[25] = x[28] = MAD_F_MLZ(hi, lo) + t4;
    MAD_F_ML0(hi, lo, X[1], -MAD_F(0x0fdcf549));
    MAD_F_MLA(hi, lo, X[7], -MAD_F(0x0cb19346));
    MAD_F_MLA(hi, lo, X[10], -MAD_F(0x09bd7ca0));
    MAD_F_MLA(hi, lo, X[16], -MAD_F(0x0216a2a2));
    t5 = MAD_F_MLZ(hi, lo) - t6;
    MAD_F_ML0(hi, lo, X[0], MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[3], MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[5], MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[6], MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[8], -MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[9], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[12], MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[14], -MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[15], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0d7e8807));
    x[2] = MAD_F_MLZ(hi, lo) + t5;
    x[15] = -x[2];
    MAD_F_ML0(hi, lo, X[0], MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[2], MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[3], MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[5], MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[6], -MAD_F(0x00b2aa3e));
    MAD_F_MLA(hi, lo, X[8], MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[9], -MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[11], MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[14], MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[15], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[17], MAD_F(0x0e313245));
    x[3] = MAD_F_MLZ(hi, lo) + t5;
    x[14] = -x[3];
    MAD_F_ML0(hi, lo, X[0], -MAD_F(0x0ffc19fd));
    MAD_F_MLA(hi, lo, X[2], -MAD_F(0x0f9ee890));
    MAD_F_MLA(hi, lo, X[3], -MAD_F(0x0f426cb5));
    MAD_F_MLA(hi, lo, X[5], -MAD_F(0x0e313245));
    MAD_F_MLA(hi, lo, X[6], -MAD_F(0x0d7e8807));
    MAD_F_MLA(hi, lo, X[8], -MAD_F(0x0bcbe352));
    MAD_F_MLA(hi, lo, X[9], -MAD_F(0x0acf37ad));
    MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0898c779));
    MAD_F_MLA(hi, lo, X[12], -MAD_F(0x07635284));
    MAD_F_MLA(hi, lo, X[14], -MAD_F(0x04cfb0e2));
    MAD_F_MLA(hi, lo, X[15], -MAD_F(0x03768962));
    MAD_F_MLA(hi, lo, X[17], -MAD_F(0x00b2aa3e));
    x[26] = x[27] = MAD_F_MLZ(hi, lo) + t5;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_imdct_l(int32_t const X[18], int32_t z[36], uint32_t block_type) {
    uint32_t i;
    /* IMDCT */
    imdct36(X, z);
    /* windowing */
    switch (block_type) {
        case 0: /* normal window */
    #if defined(ASO_INTERLEAVE1)
        {
            register int32_t tmp1, tmp2;
            tmp1 = window_l[0];
            tmp2 = window_l[1];
            for (i = 0; i < 34; i += 2) {
                z[i + 0] = mad_f_mul(z[i + 0], tmp1);
                tmp1 = window_l[i + 2];
                z[i + 1] = mad_f_mul(z[i + 1], tmp2);
                tmp2 = window_l[i + 3];
            }
            z[34] = mad_f_mul(z[34], tmp1);
            z[35] = mad_f_mul(z[35], tmp2);
        }
    #elif defined(ASO_INTERLEAVE2)
        {
            register int32_t tmp1, tmp2;
            tmp1 = z[0];
            tmp2 = window_l[0];
            for (i = 0; i < 35; ++i) {
                z[i] = mad_f_mul(tmp1, tmp2);
                tmp1 = z[i + 1];
                tmp2 = window_l[i + 1];
            }
            z[35] = mad_f_mul(tmp1, tmp2);
        }
    #elif 1
            for (i = 0; i < 36; i += 4) {
                z[i + 0] = mad_f_mul(z[i + 0], window_l[i + 0]);
                z[i + 1] = mad_f_mul(z[i + 1], window_l[i + 1]);
                z[i + 2] = mad_f_mul(z[i + 2], window_l[i + 2]);
                z[i + 3] = mad_f_mul(z[i + 3], window_l[i + 3]);
            }
    #else
            for (i = 0; i < 36; ++i) z[i] = mad_f_mul(z[i], window_l[i]);
    #endif
        break;
        case 1: /* start block */
            for (i = 0; i < 18; ++i) z[i] = mad_f_mul(z[i], window_l[i]);
            /*  (i = 18; i < 24; ++i) z[i] unchanged */
            for (i = 24; i < 30; ++i) z[i] = mad_f_mul(z[i], window_s[i - 18]);
            for (i = 30; i < 36; ++i) z[i] = 0;
            break;
        case 3: /* stop block */
            for (i = 0; i < 6; ++i) z[i] = 0;
            for (i = 6; i < 12; ++i) z[i] = mad_f_mul(z[i], window_s[i - 6]);
            /*  (i = 12; i < 18; ++i) z[i] unchanged */
            for (i = 18; i < 36; ++i) z[i] = mad_f_mul(z[i], window_l[i]);
            break;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_imdct_s(int32_t const X[18], int32_t z[36]) {
    ps_ptr<int32_t>y;
    y.alloc(36 * sizeof(int32_t));
    int32_t          *yptr;
    int32_t const*    wptr;
    int32_t               w, i;
    int32_t  hi;
    uint32_t lo;
    /* IMDCT */
    yptr = &y[0];
    for (w = 0; w < 3; ++w) {
        int32_t const(*s)[6];
        s = imdct_s;
        for (i = 0; i < 3; ++i) {
            MAD_F_ML0(hi, lo, X[0], (*s)[0]);
            MAD_F_MLA(hi, lo, X[1], (*s)[1]);
            MAD_F_MLA(hi, lo, X[2], (*s)[2]);
            MAD_F_MLA(hi, lo, X[3], (*s)[3]);
            MAD_F_MLA(hi, lo, X[4], (*s)[4]);
            MAD_F_MLA(hi, lo, X[5], (*s)[5]);
            yptr[i + 0] = MAD_F_MLZ(hi, lo);
            yptr[5 - i] = -yptr[i + 0];
            ++s;
            MAD_F_ML0(hi, lo, X[0], (*s)[0]);
            MAD_F_MLA(hi, lo, X[1], (*s)[1]);
            MAD_F_MLA(hi, lo, X[2], (*s)[2]);
            MAD_F_MLA(hi, lo, X[3], (*s)[3]);
            MAD_F_MLA(hi, lo, X[4], (*s)[4]);
            MAD_F_MLA(hi, lo, X[5], (*s)[5]);
            yptr[i + 6] = MAD_F_MLZ(hi, lo);
            yptr[11 - i] = yptr[i + 6];
            ++s;
        }
        yptr += 12;
        X += 6;
    }
    /* windowing, overlapping and concatenation */
    yptr = &y[0];
    wptr = &window_s[0];
    for (i = 0; i < 6; ++i) {
        z[i + 0] = 0;
        z[i + 6] = mad_f_mul(yptr[0 + 0], wptr[0]);
        MAD_F_ML0(hi, lo, yptr[0 + 6], wptr[6]);
        MAD_F_MLA(hi, lo, yptr[12 + 0], wptr[0]);
        z[i + 12] = MAD_F_MLZ(hi, lo);
        MAD_F_ML0(hi, lo, yptr[12 + 6], wptr[6]);
        MAD_F_MLA(hi, lo, yptr[24 + 0], wptr[0]);
        z[i + 18] = MAD_F_MLZ(hi, lo);
        z[i + 24] = mad_f_mul(yptr[24 + 6], wptr[6]);
        z[i + 30] = 0;
        ++yptr;
        ++wptr;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_overlap(int32_t const output[36], int32_t overlap[18], int32_t sample[18][32], uint32_t sb) {
    uint32_t i;
#if defined(ASO_INTERLEAVE2)
    {
        register int32_t tmp1, tmp2;
        tmp1 = overlap[0];
        tmp2 = overlap[1];
        for (i = 0; i < 16; i += 2) {
            sample[i + 0][sb] = output[i + 0 + 0] + tmp1;
            overlap[i + 0] = output[i + 0 + 18];
            tmp1 = overlap[i + 2];
            sample[i + 1][sb] = output[i + 1 + 0] + tmp2;
            overlap[i + 1] = output[i + 1 + 18];
            tmp2 = overlap[i + 3];
        }
        sample[16][sb] = output[16 + 0] + tmp1;
        overlap[16] = output[16 + 18];
        sample[17][sb] = output[17 + 0] + tmp2;
        overlap[17] = output[17 + 18];
    }
#else
    for (i = 0; i < 18; ++i) {
        sample[i][sb] = output[i + 0] + overlap[i];
        overlap[i] = output[i + 18];
    }
#endif
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_overlap_z(int32_t overlap[18], int32_t sample[18][32], uint32_t sb) {
    uint32_t i;
#if defined(ASO_INTERLEAVE2)
    {
        register int32_t tmp1, tmp2;
        tmp1 = overlap[0];
        tmp2 = overlap[1];
        for (i = 0; i < 16; i += 2) {
            sample[i + 0][sb] = tmp1;
            overlap[i + 0] = 0;
            tmp1 = overlap[i + 2];
            sample[i + 1][sb] = tmp2;
            overlap[i + 1] = 0;
            tmp2 = overlap[i + 3];
        }
        sample[16][sb] = tmp1;
        overlap[16] = 0;
        sample[17][sb] = tmp2;
        overlap[17] = 0;
    }
#else
    for (i = 0; i < 18; ++i) {
        sample[i][sb] = overlap[i];
        overlap[i] = 0;
    }
#endif
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void III_freqinver(int32_t sample[18][32], uint32_t sb) {
    uint32_t i;
    {
        int32_t tmp1, tmp2;
        tmp1 = sample[1][sb];
        tmp2 = sample[3][sb];
        for (i = 1; i < 13; i += 4) {
            sample[i + 0][sb] = -tmp1;
            tmp1 = sample[i + 4][sb];
            sample[i + 2][sb] = -tmp2;
            tmp2 = sample[i + 6][sb];
        }
        sample[13][sb] = -tmp1;
        tmp1 = sample[17][sb];
        sample[15][sb] = -tmp2;
        sample[17][sb] = -tmp1;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
enum mad_error III_decode(struct mad_bitptr* ptr, mad_frame_t* frame, struct sideinfo* si, uint32_t nch) {
    struct mad_header* header = &frame->header;
    uint32_t       sfreqi, ngr, gr;
    {
        uint32_t sfreq;
        sfreq = header->samplerate;
        if (header->flags & MAD_FLAG_MPEG_2_5_EXT) sfreq *= 2;
        /* 48000 => 0, 44100 => 1, 32000 => 2,
           24000 => 3, 22050 => 4, 16000 => 5 */
        sfreqi = ((sfreq >> 7) & 0x000f) + ((sfreq >> 15) & 0x0001) - 8;
        if (header->flags & MAD_FLAG_MPEG_2_5_EXT) sfreqi += 3;
    }
    /* scalefactors, Huffman decoding, requantization */
    ngr = (header->flags & MAD_FLAG_LSF_EXT) ? 1 : 2;
    for (gr = 0; gr < ngr; ++gr) {
        struct granule* granule = &si->gr[gr];
        uint8_t const*  sfbwidth[2] = {0};
        // int32_t xr[2][576];
        ps_array2d<int32_t>xr;
        xr.alloc(2, 576);
        uint32_t    ch;
        enum mad_error  error;
        for (ch = 0; ch < nch; ++ch) {
            struct channel* channel = &granule->ch[ch];
            uint32_t    part2_length;
            sfbwidth[ch] = sfbwidth_table[sfreqi].l;
            if (channel->block_type == 2) { sfbwidth[ch] = (channel->flags & mixed_block_flag) ? sfbwidth_table[sfreqi].m : sfbwidth_table[sfreqi].s; }
            if (header->flags & MAD_FLAG_LSF_EXT) {
                part2_length = III_scalefactors_lsf(ptr, channel, ch == 0 ? 0 : &si->gr[1].ch[1], header->mode_extension);
            } else {
                part2_length = III_scalefactors(ptr, channel, &si->gr[0].ch[ch], gr == 0 ? 0 : si->scfsi[ch]);
            }
            error = III_huffdecode(ptr, xr.get_raw_row_ptr(ch), channel, sfbwidth[ch], part2_length);
            if (error) return error;
        }
        /* joint stereo processing */
        if (header->mode == MAD_MODE_JOINT_STEREO && header->mode_extension) {
            error = III_stereo(reinterpret_cast<int32_t (*)[576]>(xr.get_raw_row_ptr(0)), granule, header, sfbwidth[0]);
            if (error) return error;
        }
        /* reordering, alias reduction, IMDCT, overlap-add, frequency inversion */
        for (ch = 0; ch < nch; ++ch) {
            struct channel const* channel = &granule->ch[ch];

            int32_t (*sample)[32] = reinterpret_cast<int32_t(*)[32]>(s_sbsample.get_raw_row_ptr(ch, 18 * gr));
            uint32_t sb, l, i, sblimit;
            int32_t      output[36];
            if (channel->block_type == 2) {
                III_reorder(xr.get_raw_row_ptr(ch), channel, sfbwidth[ch]);
#if !defined(OPT_STRICT)
                /*
                 * According to ISO/IEC 11172-3, "Alias reduction is not applied for
                 * granules with block_type == 2 (short block)." However, other
                 * sources suggest alias reduction should indeed be performed on the
                 * lower two subbands of mixed blocks. Most other implementations do
                 * this, so by default we will too.
                 */
                if (channel->flags & mixed_block_flag) III_aliasreduce(xr.get_raw_row_ptr(ch), 36);
#endif
            } else
                III_aliasreduce(xr.get_raw_row_ptr(ch), 576);

            l = 0;
            /* subbands 0-1 */
            if (channel->block_type != 2 || (channel->flags & mixed_block_flag)) {
                uint32_t block_type;
                block_type = channel->block_type;
                if (channel->flags & mixed_block_flag) block_type = 0;
                /* long blocks */
                for (sb = 0; sb < 2; ++sb, l += 18) {
                    III_imdct_l(&xr(ch, l), output, block_type);
                    III_overlap(output, *reinterpret_cast<int32_t(*)[18]>(s_overlap.get_raw_row_ptr(ch, sb)), sample, sb);
                }
            } else {
                /* short blocks */
                for (sb = 0; sb < 2; ++sb, l += 18) {
                    III_imdct_s(&xr(ch, l), output);
                    III_overlap(output, *reinterpret_cast<int32_t(*)[18]>(s_overlap.get_raw_row_ptr(ch, sb)), sample, sb);
                }
            }
            III_freqinver(sample, 1);
            /* (nonzero) subbands 2-31 */
            i = 576;
            while (i > 36 && xr(ch, i - 1) == 0) --i;
            sblimit = 32 - (576 - i) / 18;
            if (channel->block_type != 2) {
                /* long blocks */
                for (sb = 2; sb < sblimit; ++sb, l += 18) {
                    III_imdct_l(&xr(ch, l), output, channel->block_type);
                    III_overlap(output, *reinterpret_cast<int32_t(*)[18]>(s_overlap.get_raw_row_ptr(ch, sb)), sample, sb);
                    if (sb & 1) III_freqinver(sample, sb);
                }
            } else {
                /* short blocks */
                for (sb = 2; sb < sblimit; ++sb, l += 18) {
                    III_imdct_s(&xr(ch, l), output);
                    III_overlap(output, *reinterpret_cast<int32_t(*)[18]>(s_overlap.get_raw_row_ptr(ch, sb)), sample, sb);
                    if (sb & 1) III_freqinver(sample, sb);
                }
            }
            /* remaining (zero) subbands */
            for (sb = sblimit; sb < 32; ++sb) {
                III_overlap_z(*reinterpret_cast<int32_t(*)[18]>(s_overlap.get_raw_row_ptr(ch, sb)), sample, sb);

                if (sb & 1) III_freqinver(sample, sb);
            }
        }
    }
    return MAD_ERROR_NONE;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_layer_III(mad_stream_t* stream, mad_frame_t* frame) {
    struct mad_header* header = &frame->header;
    uint32_t           nch, priv_bitlen, next_md_begin = 0;
    uint32_t           si_len, data_bitlen, md_len;
    uint32_t           frame_space, frame_used, frame_free;
    struct mad_bitptr  ptr;
    struct sideinfo    si;
    enum mad_error     error;
    int32_t            result = 0;

    /* allocate Layer III dynamic structures */
    if (!s_main_data.valid()) {
        s_main_data.alloc(MAD_BUFFER_MDLEN);
        if (!s_main_data.valid()) {
            MP3_LOG_ERROR("not enough memory");
            stream->error = MAD_ERROR_NOMEM;
            return -1;
        }
    }
    if (!s_overlap.is_allocated()) {
        s_overlap.alloc(2, 32, 18);
        if (!s_overlap.is_allocated()) {
            MP3_LOG_ERROR("not enough memory");
            stream->error = MAD_ERROR_NOMEM;
            return -1;
        }
    }
    nch = MAD_NCHANNELS(header);

    si_len = (header->flags & MAD_FLAG_LSF_EXT) ? (nch == 1 ? 9 : 17) : (nch == 1 ? 17 : 32);
    /* check frame sanity */
    if (stream->next_frame - mad_bit_nextbyte(&stream->ptr) < (int32_t)si_len) {
        MP3_LOG_ERROR("bad frame length");
        stream->error = MAD_ERROR_BADFRAMELEN;
        stream->md_len = 0;
        return -1;
    }
    /* check CRC word */
    if (header->flags & MAD_FLAG_PROTECTION) {
        header->crc_check = mad_bit_crc(stream->ptr, si_len * CHAR_BIT, header->crc_check);
        if (header->crc_check != header->crc_target && !(frame->options & MAD_OPTION_IGNORECRC)) {
            MP3_LOG_ERROR("CRC check failed");
            stream->error = MAD_ERROR_BADCRC;
            result = -1;
        }
    }
    /* decode frame side information */
    error = III_sideinfo(&stream->ptr, nch, header->flags & MAD_FLAG_LSF_EXT, &si, &data_bitlen, &priv_bitlen);
    if (error && result == 0) {
        stream->error = error;
        result = -1;
    }
    header->flags |= priv_bitlen;
    header->private_bits |= si.private_bits;
    /* find main_data of next frame */
    {
        struct mad_bitptr peek;
        uint32_t     header;
        mad_bit_init(&peek, stream->next_frame);
        header = mad_bit_read(&peek, 32);
        if ((header & 0xffe60000L) /* syncword | layer */ == 0xffe20000L) {
            if (!(header & 0x00010000L)) /* protection_bit */
                mad_bit_skip(&peek, 16); /* crc_check */
            next_md_begin = mad_bit_read(&peek, (header & 0x00080000L) /* ID */ ? 9 : 8);
        }
        mad_bit_finish(&peek);
    }
    /* find main_data of this frame */
    frame_space = stream->next_frame - mad_bit_nextbyte(&stream->ptr);
    if (next_md_begin > si.main_data_begin + frame_space) next_md_begin = 0;
    md_len = si.main_data_begin + frame_space - next_md_begin;
    frame_used = 0;
    if (si.main_data_begin == 0) {
        ptr = stream->ptr;
        stream->md_len = 0;
        frame_used = md_len;
    } else {
        if (si.main_data_begin > stream->md_len) {
            if (result == 0) {
                // MP3_LOG_ERROR("need more data");
                stream->error = MAD_ERROR_CONTINUE;   // need more data
                result = 100;
            }
        } else {
            mad_bit_init(&ptr, s_main_data.get() + stream->md_len - si.main_data_begin);
            if (md_len > si.main_data_begin) {
                assert(stream->md_len + md_len - si.main_data_begin <= MAD_BUFFER_MDLEN);
                memcpy(s_main_data.get() + stream->md_len, mad_bit_nextbyte(&stream->ptr), frame_used = md_len - si.main_data_begin);
                stream->md_len += frame_used;
            }
        }
    }
    frame_free = frame_space - frame_used;
    /* decode main_data */
    if (result == 0) {
        error = III_decode(&ptr, frame, &si, nch);
        if (error) {
            stream->error = error;
            result = -1;
        }
        /* designate ancillary bits */
        stream->anc_ptr = ptr;
        stream->anc_bitlen = md_len * CHAR_BIT - data_bitlen;
    }
    /* preload main_data buffer with up to 511 bytes for next frame(s) */
    if (frame_free >= next_md_begin) {
        memcpy(s_main_data.get(), stream->next_frame - next_md_begin, next_md_begin);
        stream->md_len = next_md_begin;
    } else {
        if (md_len < si.main_data_begin) {
            uint32_t extra;
            extra = si.main_data_begin - md_len;
            if (extra + frame_free > next_md_begin) extra = next_md_begin - frame_free;
            if (extra < stream->md_len) {
                memmove(s_main_data.get(), s_main_data.get() + stream->md_len - extra, extra);
                stream->md_len = extra;
            }
        } else
            stream->md_len = 0;
        memcpy(s_main_data.get() + stream->md_len, stream->next_frame - frame_free, frame_free);
        stream->md_len += frame_free;
    }
    return result;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
signed short mad_fixed_to_short(int32_t sample) {
    // clamp
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;
    // 28 Bit Fixed Point calculate down to 16 bit
    return (signed short)(sample >> (MAD_F_FRACBITS + 1 - 16));
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_header_init(struct mad_header* header) {
    header->layer = (mad_layer)0;
    header->mode = (mad_mode)0;
    header->mode_extension = 0;
    header->emphasis = (mad_emphasis)0;
    header->bitrate = 0;
    header->samplerate = 0;
    header->crc_check = 0;
    header->crc_target = 0;
    header->flags = 0;
    header->private_bits = 0;
    header->duration = mad_timer_zero;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_frame_init(mad_frame_t* frame) {
    mad_header_init(&frame->header);
    frame->options = 0;
    s_overlap.reset();
    mad_frame_mute(frame);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_frame_finish(mad_frame_t* frame) {// deallocate any dynamic memory associated with frame
    mad_header_finish(&frame->header);
    s_overlap.reset();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t decode_header(struct mad_header* header, mad_stream_t* stream) { // read header data and following CRC word
    uint32_t index;
    header->flags = 0;
    header->private_bits = 0;
    /* header() */
    /* syncword */
    mad_bit_skip(&stream->ptr, 11);
    /* MPEG 2.5 indicator (really part of syncword) */
    if (mad_bit_read(&stream->ptr, 1) == 0) header->flags |= MAD_FLAG_MPEG_2_5_EXT;
    /* ID */
    if (mad_bit_read(&stream->ptr, 1) == 0)
        header->flags |= MAD_FLAG_LSF_EXT;
    else if (header->flags & MAD_FLAG_MPEG_2_5_EXT) {
        MP3_LOG_ERROR("lost synchronization");
        stream->error = MAD_ERROR_LOSTSYNC;
        return -1;
    }
    /* layer */
    header->layer = (mad_layer)( 4 - mad_bit_read(&stream->ptr, 2));
    if (header->layer == 4) {
        MP3_LOG_ERROR("reserved header layer value");
        stream->error = MAD_ERROR_BADLAYER;
        return -1;
    }
    /* protection_bit */
    if (mad_bit_read(&stream->ptr, 1) == 0) {
        header->flags |= MAD_FLAG_PROTECTION;
        header->crc_check = mad_bit_crc(stream->ptr, 16, 0xffff);
    }
    /* bitrate_index */
    index = mad_bit_read(&stream->ptr, 4);
    if (index == 15) {
        MP3_LOG_ERROR("forbidden bitrate value");
        stream->error = MAD_ERROR_BADBITRATE;
        return -1;
    }
    if (header->flags & MAD_FLAG_LSF_EXT)
        header->bitrate = bitrate_table[3 + (header->layer >> 1)][index];
    else
        header->bitrate = bitrate_table[header->layer - 1][index];
    /* sampling_frequency */
    index = mad_bit_read(&stream->ptr, 2);

    if (index == 3) {
        MP3_LOG_ERROR("reserved sample frequency value");
        stream->error = MAD_ERROR_BADSAMPLERATE;
        return -1;
    }
    header->samplerate = samplerate_table[index];
    if (header->flags & MAD_FLAG_LSF_EXT) {
        header->samplerate /= 2;

        if (header->flags & MAD_FLAG_MPEG_2_5_EXT) header->samplerate /= 2;
    }
    /* padding_bit */
    if (mad_bit_read(&stream->ptr, 1)) header->flags |= MAD_FLAG_PADDING;
    /* private_bit */
    if (mad_bit_read(&stream->ptr, 1)) header->private_bits |= MAD_PRIVATE_HEADER;
    /* mode */
    header->mode = (mad_mode)(3 - mad_bit_read(&stream->ptr, 2));
    /* mode_extension */
    header->mode_extension = mad_bit_read(&stream->ptr, 2);
    /* copyright */
    if (mad_bit_read(&stream->ptr, 1)) header->flags |= MAD_FLAG_COPYRIGHT;
    /* original/copy */
    if (mad_bit_read(&stream->ptr, 1)) header->flags |= MAD_FLAG_ORIGINAL;
    /* emphasis */
    header->emphasis = (mad_emphasis)mad_bit_read(&stream->ptr, 2);
#if defined(OPT_STRICT)
    /*
     * ISO/IEC 11172-3 says this is a reserved emphasis value, but
     * streams exist which use it anyway. Since the value is not important
     * to the decoder proper, we allow it unless OPT_STRICT is defined.
     */
    if (header->emphasis == MAD_EMPHASIS_RESERVED) {
        MP3_LOG_ERROR("reserved emphasis value");
        stream->error = MAD_ERROR_BADEMPHASIS;
        return -1;
    }
#endif
    /* error_check() */
    /* crc_check */
    if (header->flags & MAD_FLAG_PROTECTION) header->crc_target = mad_bit_read(&stream->ptr, 16);
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t free_bitrate(mad_stream_t* stream, struct mad_header const* header) { // attempt to discover the bitstream's free bitrate
    struct mad_bitptr keep_ptr;
    uint32_t     rate = 0;
    uint32_t      pad_slot, slots_per_frame;
    uint8_t const*    ptr = 0;
    keep_ptr = stream->ptr;
    pad_slot = (header->flags & MAD_FLAG_PADDING) ? 1 : 0;
    slots_per_frame = (header->layer == MAD_LAYER_III && (header->flags & MAD_FLAG_LSF_EXT)) ? 72 : 144;
    while (mad_stream_sync(stream) == 0) {
        mad_stream_t peek_stream;
        struct mad_header peek_header;
        peek_stream = *stream;
        peek_header = *header;
        if (decode_header(&peek_header, &peek_stream) == 0 && peek_header.layer == header->layer && peek_header.samplerate == header->samplerate) {
            uint32_t N;
            ptr = mad_bit_nextbyte(&stream->ptr);
            N = ptr - stream->this_frame;
            if (header->layer == MAD_LAYER_I) {
                rate = (uint32_t)header->samplerate * (N - 4 * pad_slot + 4) / 48 / 1000;
            } else {
                rate = (uint32_t)header->samplerate * (N - pad_slot + 1) / slots_per_frame / 1000;
            }
            if (rate >= 8) break;
        }
        mad_bit_skip(&stream->ptr, 8);
    }
    stream->ptr = keep_ptr;
    if (rate < 8 || (header->layer == MAD_LAYER_III && rate > 640)) {
        MP3_LOG_ERROR("lost synchronization");
        stream->error = MAD_ERROR_LOSTSYNC;
        return -1;
    }
    stream->freerate = rate * 1000;
    return 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_header_decode(struct mad_header* header, mad_stream_t* stream) { // read the next frame header from the stream
    uint8_t const *ptr, *end;
    uint32_t  pad_slot, N;
    ptr = stream->next_frame;
    end = stream->bufend;
    if (ptr == 0) {
        MP3_LOG_ERROR("invalid (null) buffer pointer");
        stream->error = MAD_ERROR_BUFPTR;
        goto fail;
    }
    /* stream skip */
    if (stream->skiplen) {
        if (!stream->sync) ptr = stream->this_frame;
        if (end - ptr < stream->skiplen) {
            stream->skiplen -= end - ptr;
            stream->next_frame = end;
            MP3_LOG_ERROR("input buffer too small (or EOF)");
            stream->error = MAD_ERROR_BUFLEN;
            goto fail;
        }
        ptr += stream->skiplen;
        stream->skiplen = 0;
        stream->sync = 1;
    }
sync:
    /* synchronize */
    if (stream->sync) {
        if (end - ptr < MAD_BUFFER_GUARD) {
            stream->next_frame = ptr;
            MP3_LOG_ERROR("input buffer too small (or EOF)");
            stream->error = MAD_ERROR_BUFLEN;
            goto fail;
        } else if (!(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0)) {
            /* mark point where frame sync word was expected */
            stream->this_frame = ptr;
            stream->next_frame = ptr + 1;
            MP3_LOG_ERROR("lost synchronization");
            stream->error = MAD_ERROR_LOSTSYNC;
            goto fail;
        }
    } else {
        mad_bit_init(&stream->ptr, ptr);
        if (mad_stream_sync(stream) == -1) {
            if (end - stream->next_frame >= MAD_BUFFER_GUARD) stream->next_frame = end - MAD_BUFFER_GUARD;
            MP3_LOG_ERROR("input buffer too small (or EOF)");
            stream->error = MAD_ERROR_BUFLEN;
            goto fail;
        }
        ptr = mad_bit_nextbyte(&stream->ptr);
    }
    /* begin processing */
    stream->this_frame = ptr;
    stream->next_frame = ptr + 1; /* possibly bogus sync word */
    mad_bit_init(&stream->ptr, stream->this_frame);
    if (decode_header(header, stream) == -1) goto fail;
    /* calculate frame duration */
    mad_timer_set(&header->duration, 0, 32 * MAD_NSBSAMPLES(header), header->samplerate);
    /* calculate free bit rate */
    if (header->bitrate == 0) {
        if ((stream->freerate == 0 || !stream->sync) && free_bitrate(stream, header) == -1) goto fail;
        header->bitrate = stream->freerate;
        header->flags |= MAD_FLAG_FREEFORMAT;
    }
    /* calculate beginning of next frame */
    pad_slot = (header->flags & MAD_FLAG_PADDING) ? 1 : 0;
    if (header->layer == MAD_LAYER_I){
        N = ((12 * header->bitrate / header->samplerate) + pad_slot) * 4;
    }
    else {
        uint32_t slots_per_frame;
        slots_per_frame = (header->layer == MAD_LAYER_III && (header->flags & MAD_FLAG_LSF_EXT)) ? 72 : 144;
        N = (slots_per_frame * header->bitrate / header->samplerate) + pad_slot;
    }
    /* verify there is enough data left in buffer to decode this frame */
    if (N + MAD_BUFFER_GUARD > end - stream->this_frame) {
        stream->next_frame = stream->this_frame;
        if(s_underflow_cnt < 2){
            s_underflow_cnt++;
            stream->error = MAD_ERROR_CONTINUE;
            return MAD_ERROR_CONTINUE;
        }
        MP3_LOG_ERROR("input buffer too small (or EOF)");
        return 0;
        stream->error = MAD_ERROR_BUFLEN;
        goto fail;
    }
    stream->next_frame = stream->this_frame + N;
    if (!stream->sync) {
        /* check that a valid frame header follows this frame */
        ptr = stream->next_frame;
        if (!(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0)) {
            ptr = stream->next_frame = stream->this_frame + 1;
            goto sync;
        }
        stream->sync = 1;
    }
    header->flags |= MAD_FLAG_INCOMPLETE;
    return 0;
fail:
    stream->sync = 0;
    return -1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_frame_decode(mad_frame_t* frame, mad_stream_t* stream) { // decode a single frame from a bitstream
    frame->options = stream->options;
    /* header() */
    /* error_check() */
    if (!(frame->header.flags & MAD_FLAG_INCOMPLETE)){
        int res = mad_header_decode(&frame->header, stream);
        if (res == MAD_ERROR_CONTINUE) return MAD_ERROR_CONTINUE;
        if (res == -1) goto fail;
    }
    /* audio_data() */
    frame->header.flags &= ~MAD_FLAG_INCOMPLETE;
    if (decoder_table[frame->header.layer - 1](stream, frame) == -1) {
        if (!MAD_RECOVERABLE(stream->error)) stream->next_frame = stream->this_frame;
        goto fail;
    }
    /* ancillary_data() */
    if (frame->header.layer != MAD_LAYER_III) {
        struct mad_bitptr next_frame;
        mad_bit_init(&next_frame, stream->next_frame);
        stream->anc_ptr = stream->ptr;
        stream->anc_bitlen = mad_bit_length(&stream->ptr, &next_frame);
        mad_bit_finish(&next_frame);
    }
    return 0;
fail:
    stream->anc_bitlen = 0;
    return -1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void mad_frame_mute(mad_frame_t* frame) { // zero all subband values so the frame becomes silent
    uint32_t s, sb;
    for (s = 0; s < 36; ++s) {
        for (sb = 0; sb < 32; ++sb) { s_sbsample(0, s, sb) = s_sbsample(1, s, sb) = 0; }
    }
    if (s_overlap.is_allocated()) {
        for (s = 0; s < 18; ++s) {
            for (sb = 0; sb < 32; ++sb) { (s_overlap)(0, sb, s) = (s_overlap)(1, sb, s) = 0; }
        }
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_fill_outbuff(int16_t *outSamples){
        int32_t* leftChannel;  // internal variable for the PCM outBuff
        int32_t* rightChannel;
        leftChannel  = *reinterpret_cast<int32_t(*)[1152]>(s_samplesBuff.get_raw_row_ptr(0));
        rightChannel = (s_mad_channels == 2) ? *reinterpret_cast<int32_t(*)[1152]>(s_samplesBuff.get_raw_row_ptr(1)) : *reinterpret_cast<int32_t(*)[1152]>(s_samplesBuff.get_raw_row_ptr(0));

    for (uint32_t i = 0; i < s_mad_out_samples; i++) { // change PCM samples into 16-bit and write in target buffer
        outSamples[2*i + 0] = mad_fixed_to_short(leftChannel[i]);
        outSamples[2*i + 1] = mad_fixed_to_short(rightChannel[i]);
    }
    return s_mad_out_samples;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t mad_get_channels(){
    return synth->channels;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_get_sample_rate(){
    return synth->samplerate;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_get_bitrate(){
    return s_bitrate;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_decode(uint8_t *data, int32_t *bytesLeft, int16_t *outSamples){
    mad_stream_buffer(stream.get(), data, *bytesLeft);
    int32_t result = mad_frame_decode(frame.get(), stream.get());
    if (result == 0)    {
        mad_synth_frame(synth.get(), frame.get());
        s_nsamples = mad_fill_outbuff(outSamples);

        size_t frameBytesUsed = (stream->next_frame - stream->this_frame); // calculate how many bytes were used up
        if(frameBytesUsed > *bytesLeft) {
            *bytesLeft = 0; // should never occur, protection
        }
        else {
            *bytesLeft -= frameBytesUsed;
        }
        result = 0; // ok
    }
    else {
        if(stream->error == MAD_ERROR_CONTINUE) return MAD_ERROR_CONTINUE;
        if(stream->error == MAD_ERROR_BUFLEN) { // too little data, first load up
            result = -1;
        }
        else {  // recoverable or fatal
            result = -2;
        }
    }
    return result;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint16_t mad_get_output_samps(){
    return s_nsamples;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t mad_find_syncword(uint8_t *buf, int32_t nBytes) {
    // Synchronwort-Konstanten
    const uint8_t SYNCWORDH = 0xFF;
    const uint8_t SYNCWORDL = 0xF0; // Strengere Maske, prüft auch Layer-Bits

    // Struktur für MP3-Frame-Header
    typedef struct {
        uint8_t  mpeg_bits; // Rohbits 12-13: 3=MPEG1, 2=MPEG2, 0=MPEG2.5, 1=reserviert
        uint8_t  layer;     // 0=reserviert, 1=Layer III, 2=Layer II, 3=Layer I
        bool     crc_protected;
        uint8_t  bitrate_idx;
        uint8_t  sample_rate_idx;
        bool     padding;
        uint8_t  channel_mode;
        uint32_t frame_length; // In Bytes
        uint16_t sample_rate_hz; // Tatsächliche Abtastrate in Hz
        uint16_t bitrate_kbps;   // Tatsächliche Bitrate in kbps
        uint16_t samples_per_frame;
    } Mp3FrameHeader;

    // Sampling-Frequenz-Tabellen
    const uint16_t sampling_rates[3][4] = {
        {44100, 48000, 32000, 0}, // MPEG1
        {22050, 24000, 16000, 0}, // MPEG2
        {11025, 12000, 8000, 0}   // MPEG2.5
    };

    // Bitraten-Tabellen
    const uint16_t mpeg1_layer1_bitrates[16] = {
        0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0
    };
    const uint16_t mpeg1_layer2_bitrates[16] = {
        0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0
    };
    const uint16_t mpeg1_layer3_bitrates[16] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
    };
    const uint16_t mpeg2_layer1_bitrates[16] = {
        0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 192, 224, 256, 320, 0
    };
    const uint16_t mpeg2_layer2_bitrates[16] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
    };
    const uint16_t mpeg2_layer3_bitrates[16] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
    };

    // Funktion zum Parsen des Headers und Überprüfen der Gültigkeit
    auto parseMp3Header = [&](const uint8_t* header_data, Mp3FrameHeader* header_info) {
        header_info->mpeg_bits = (header_data[1] >> 3) & 0b11; // Bits 12, 13
        header_info->layer = (header_data[1] >> 1) & 0b11; // Bits 14, 15
        header_info->crc_protected = !((header_data[1] >> 0) & 0b1); // Bit 16
        header_info->bitrate_idx = (header_data[2] >> 4) & 0b1111; // Bits 17-20
        header_info->sample_rate_idx = (header_data[2] >> 2) & 0b11; // Bits 21-22
        header_info->padding = (header_data[2] >> 1) & 0b1; // Bit 23
        header_info->channel_mode = (header_data[3] >> 6) & 0b11; // Bits 24-25

        // Gültigkeitsprüfungen
        if (header_info->mpeg_bits == 1) {
            MP3_LOG_DEBUG("Reserved MPEG version: 0x%02X 0x%02X", header_data[0], header_data[1]);
            return false;
        }
        if (header_info->layer == 0) {
            MP3_LOG_DEBUG("Reserved Layer: 0x%02X 0x%02X", header_data[0], header_data[1]);
            return false;
        }
        if (header_info->bitrate_idx == 0 || header_info->bitrate_idx == 15) {
            MP3_LOG_DEBUG("Invalid bitrate index: %d", header_info->bitrate_idx);
            return false;
        }
        if (header_info->sample_rate_idx == 3) {
            MP3_LOG_DEBUG("Invalid sample rate index: %d", header_info->sample_rate_idx);
            return false;
        }

        // Bestimme Bitrate und Sampling-Rate
        uint16_t bitrate_kbps = 0;
        uint16_t sample_rate_hz = 0;
        uint8_t sr_table_idx = (header_info->mpeg_bits == 3) ? 0 : (header_info->mpeg_bits == 2) ? 1 : 2;
        sample_rate_hz = sampling_rates[sr_table_idx][header_info->sample_rate_idx];

        // Bitraten-Mapping
        if (header_info->mpeg_bits == 3) { // MPEG-1
            if (header_info->layer == 1) { // Layer III
                bitrate_kbps = mpeg1_layer3_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 2) { // Layer II
                bitrate_kbps = mpeg1_layer2_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 3) { // Layer I
                bitrate_kbps = mpeg1_layer1_bitrates[header_info->bitrate_idx];
            }
        } else if (header_info->mpeg_bits == 2 || header_info->mpeg_bits == 0) { // MPEG-2/2.5
            if (header_info->layer == 1) { // Layer III
                bitrate_kbps = mpeg2_layer3_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 2) { // Layer II
                bitrate_kbps = mpeg2_layer2_bitrates[header_info->bitrate_idx];
            } else if (header_info->layer == 3) { // Layer I
                bitrate_kbps = mpeg2_layer1_bitrates[header_info->bitrate_idx];
            }
        }

        if (bitrate_kbps == 0 || sample_rate_hz == 0) {
            MP3_LOG_DEBUG("Invalid bitrate or sample rate: bitrate=%d, sample_rate=%d", bitrate_kbps, sample_rate_hz);
            return false;
        }

        // Frame-Länge berechnen
        if (header_info->layer == 1) { // Layer III
            if (header_info->mpeg_bits == 3) { // MPEG-1
                header_info->frame_length = (144 * bitrate_kbps * 1000) / sample_rate_hz;
            } else { // MPEG-2/2.5
                header_info->frame_length = (72 * bitrate_kbps * 1000) / sample_rate_hz;
            }
        } else if (header_info->layer == 2) { // Layer II
            header_info->frame_length = (144 * bitrate_kbps * 1000) / sample_rate_hz;
        } else if (header_info->layer == 3) { // Layer I
            header_info->frame_length = (12 * bitrate_kbps * 1000) / sample_rate_hz;
        }

        if (header_info->padding) {
            header_info->frame_length += (header_info->layer == 3) ? 4 : 1; // Layer I: 4 Bytes, andere: 1 Byte
        }

        if (header_info->frame_length == 0) {
            MP3_LOG_DEBUG("Calculated frame length is zero");
            return false;
        }

        // Samples pro Frame
        if (header_info->mpeg_bits == 3) { // MPEG-1
            if (header_info->layer == 1 || header_info->layer == 2) { // Layer III oder II
                header_info->samples_per_frame = 1152;
            } else if (header_info->layer == 3) { // Layer I
                header_info->samples_per_frame = 384;
            }
        } else if (header_info->mpeg_bits == 2 || header_info->mpeg_bits == 0) { // MPEG-2/2.5
            if (header_info->layer == 1) { // Layer III
                header_info->samples_per_frame = 576;
            } else if (header_info->layer == 2) { // Layer II
                header_info->samples_per_frame = 1152;
            } else if (header_info->layer == 3) { // Layer I
                header_info->samples_per_frame = 384;
            }
        } else {
            MP3_LOG_DEBUG("Invalid MPEG bits for samples_per_frame: %d", header_info->mpeg_bits);
            return false;
        }

        header_info->sample_rate_hz = sample_rate_hz;
        header_info->bitrate_kbps = bitrate_kbps;
        return true;
    };

    const uint8_t mp3FHsize = 4; // Frame-Header-Größe

    // Lambda für Synchronwort-Suche
    auto findSync = [&](uint8_t* search_buf, uint16_t offset, uint16_t len) {
        for (int32_t i = 0; i < len - 1; i++) {
            if ((search_buf[i + offset] == SYNCWORDH) &&
                ((search_buf[i + offset + 1] & SYNCWORDL) == SYNCWORDL)) {
                return i;
            }
        }
        return (int32_t)-1;
    };

    int32_t current_pos = 0;

    while (nBytes >= mp3FHsize) {
        int32_t sync_offset = findSync(buf, current_pos, nBytes);
        if (sync_offset == -1) {
            MP3_LOG_DEBUG("No syncword found in remaining buffer");
            return -1;
        }

        current_pos += sync_offset;
        nBytes -= sync_offset;

        if (nBytes < mp3FHsize) {
            MP3_LOG_DEBUG("Not enough bytes for a full header after syncword");
            return -1;
        }

        Mp3FrameHeader header;
        if (parseMp3Header(&buf[current_pos], &header)) {
            if (current_pos + header.frame_length + mp3FHsize <= current_pos + nBytes) {
                Mp3FrameHeader next_header;
                if ((buf[current_pos + header.frame_length] == SYNCWORDH) &&
                    ((buf[current_pos + header.frame_length + 1] & SYNCWORDL) == SYNCWORDL) &&
                    parseMp3Header(&buf[current_pos + header.frame_length], &next_header)) {
                    MP3_LOG_DEBUG("Found reliable MP3 frame at pos: %d, length: %lu, mpeg_bits: %d, layer: %d",
                          current_pos, header.frame_length, header.mpeg_bits, header.layer);

                    // libmad-kompatible Zuordnung
                    s_mpeg_version = (header.mpeg_bits == 3) ? 0 : (header.mpeg_bits == 2) ? 1 : 2; // MPEG-1=0, MPEG-2=1, MPEG-2.5=2
                    s_layer = (header.layer == 3) ? 1 : (header.layer == 2) ? 2 : 3; // Layer I=1, Layer II=2, Layer III=3
                    s_samplerate = header.sample_rate_hz;
                    s_bitrate = header.bitrate_kbps * 1000; // bit/s
                    s_channel_mode = header.channel_mode;
                    s_samples_per_frame = header.samples_per_frame;
                    s_channels = (header.channel_mode == 0b11) ? 1 : 2;

                    return current_pos;
                } else {
                    MP3_LOG_DEBUG("Header valid, but next frame invalid at pos %d: 0x%02X 0x%02X",
                          current_pos + header.frame_length, buf[current_pos + header.frame_length], buf[current_pos + header.frame_length + 1]);
                }
            } else {
                MP3_LOG_DEBUG("Header valid, but not enough data for next frame check at pos %d", current_pos);
            }
        } else {
            MP3_LOG_DEBUG("Invalid header at pos %d: 0x%02X 0x%02X 0x%02X 0x%02X",
                  current_pos, buf[current_pos], buf[current_pos + 1], buf[current_pos + 2], buf[current_pos + 3]);
        }

        current_pos += 1;
        nBytes -= 1;
    }

    return -1;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool mad_parse_xing_header(uint8_t *buf, int32_t nBytes) {
    // In the first MP3 frame, this function searches for a XING or Info Block to determine the bit rate.
    // If the search is unsuccessful, the bit rate is returned from the MP3 frame.
    // If no MP3 frame is recognized, the return is -1

    typedef struct {
        bool found = 0;            // true when the header was found
        bool is_xing = 0;          // true when it is an Xing header (VBR)
        uint32_t flags = 0;
        uint32_t total_frames = 0;
        uint32_t total_bytes = 0;
        uint8_t toc[100] = {0};    // optional, when TOC flag is set
        uint8_t vbr_scale = 0;     // optional, when VBR scale flag set
    } xing_header_t;

    xing_header_t xing_info;  // initialize structure

    size_t xing_offset;
    if (s_channels == 1) { // Mono
        xing_offset = 21;
    }
    else if(s_channels == 2) { // Stereo/Joint Stereo
        xing_offset = 36;
    }
    else return false;

    if (nBytes < xing_offset + 4) { // min space for "Xing" oder "Info" signature
        // Frame too small, no Xing/info header expected
        return false;
    }

    if (memcmp(buf + xing_offset, "Xing", 4) == 0 ||
        memcmp(buf + xing_offset, "Info", 4) == 0) {

        //MP3_LOG_DEBUG("Xing/Info-Header found!");

        // read the flags (big-endian)
        xing_info.flags = (uint32_t)buf[xing_offset + 4] << 24 |
                          (uint32_t)buf[xing_offset + 5] << 16 |
                          (uint32_t)buf[xing_offset + 6] << 8  |
                          (uint32_t)buf[xing_offset + 7];

        size_t current_read_offset = xing_offset + 8;

        // checking and reading the data based on the flags
        if (xing_info.flags & 0x0001) { // FLG_FRAMES
            xing_info.total_frames = (uint32_t)buf[current_read_offset] << 24 |
                                     (uint32_t)buf[current_read_offset + 1] << 16 |
                                     (uint32_t)buf[current_read_offset + 2] << 8  |
                                     (uint32_t)buf[current_read_offset + 3];
            // MP3_LOG_DEBUG("total frames: %lu", xing_info.total_frames);
            s_xing_total_frames = xing_info.total_frames;
            current_read_offset += 4;
        }

        if (xing_info.flags & 0x0002) { // FLG_BYTES
            xing_info.total_bytes = (uint32_t)buf[current_read_offset] << 24 |
                                    (uint32_t)buf[current_read_offset + 1] << 16 |
                                    (uint32_t)buf[current_read_offset + 2] << 8  |
                                    (uint32_t)buf[current_read_offset + 3];
            // MP3_LOG_DEBUG("total bytes: %lu", xing_info.total_bytes);
            s_xing_total_bytes = xing_info.total_bytes;
            current_read_offset += 4;
        }

        if (xing_info.flags & 0x0004) { // FLG_TOC
            memcpy(xing_info.toc, buf + current_read_offset, 100);
            // MP3_LOG_DEBUG("TOC present.");
            current_read_offset += 100;
        }

        if (xing_info.flags & 0x0008) { // FLG_VBR_SCALE
            xing_info.vbr_scale = buf[current_read_offset];
            // MP3_LOG_DEBUG("VBR Scale: %d", xing_info.vbr_scale);
            current_read_offset += 1;
        }

        // optional: Calculate the duration
        if (xing_info.total_frames > 0 && s_samplerate > 0) {
            float duration_seconds = (float)xing_info.total_frames * s_samples_per_frame / s_samplerate;
            // MP3_LOG_DEBUG("estimated duration: %.2f seconds", duration_seconds);
            s_xing_duration_seconds = duration_seconds;
            s_xing_bitrate = (xing_info.total_bytes * 8) / duration_seconds; // bit/s
        }

    } else {
        // MP3_LOG_DEBUG("No Xing/info header found.");
        return false;
    }
    return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_xing_bitrate(){
    return s_xing_bitrate;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_xing_tota_bytes(){
    return s_xing_total_bytes;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_xing_duration_seconds(){
    return s_xing_duration_seconds;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t mad_xing_total_frames(){
    return s_xing_total_frames;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* mad_get_mpeg_version(){

    static const char* mpeg_version_table[] = {
        "MPEG-1",    // MAD_MPEG_1 = 0
        "MPEG-2",    // MAD_MPEG_2 = 1
        "MPEG-2.5"   // MAD_MPEG_25 = 2
    };

    const char* mpeg_version_str = mpeg_version_table[s_mpeg_version];
    return mpeg_version_str;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* mad_get_layer(){

    const char* layer_table[] = {
        "Unknown",   // 0 (reserviert)
        "Layer I",   // MAD_LAYER_I = 1
        "Layer II",  // MAD_LAYER_II = 2
        "Layer III"  // MAD_LAYER_III = 3
    };

    const char* layer_str = layer_table[s_layer];
    return layer_str;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
