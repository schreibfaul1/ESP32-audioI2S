/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on:
 ************************************************************************************/

#include "opus_decoder.h"

// global vars
bool    f_m_magicWordFound = false;
bool    f_m_firstPage = false;
bool    f_m_commentHeader = false;
bool    f_m_opusHeadWasRead = false;
bool    f_m_nextPage = false;
bool    f_m_newSt = false; // streamTitle
bool    f_m_opusFramePacket = false;
uint8_t m_channels = 0;
char*   m_chbuf = NULL;


bool OPUSDecoder_AllocateBuffers(){
    m_chbuf = (char*)malloc(512);
    log_i("Allocate Buffers");
    return true;
}
void OPUSDecoder_FreeBuffers(){
    if(m_chbuf) {free(m_chbuf); m_chbuf = NULL;}
    log_i("free Buffers");
}
//----------------------------------------------------------------------------------------------------------------------

int OPUSDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf){
    if(f_m_magicWordFound  || f_m_nextPage || f_m_opusHeadWasRead){
        int ret = OPUSparseOGG(inbuf, bytesLeft);
        return ret;
    }
    if(f_m_firstPage){
        int ret = parseOpusHead(inbuf, bytesLeft);
        return ret;
    }
    if(f_m_commentHeader){
        int ret = parseOpusComment(inbuf, bytesLeft);
        return ret;
    }
    if(f_m_opusFramePacket){
        int ret = parseOpusFramePacket(inbuf, bytesLeft);

    }

    *bytesLeft = 0;
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------

uint8_t OPUSGetChannels(){
    return m_channels;
}
uint32_t OPUSGetSampRate(){
    return 48000;
}
uint8_t OPUSGetBitsPerSample(){
    return 16;
}
uint32_t OPUSGetBitRate(){
    return 1;
}
uint16_t OPUSGetOutputSamps(){
    return 1024;
}
char* OPUSgetStreamTitle(){
    if(f_m_newSt){
        f_m_newSt = false;
        return m_chbuf;
    }
    return NULL;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusFramePacket(uint8_t *inbuf, int *bytesLeft){  // https://www.rfc-editor.org/rfc/rfc6716  page 16 ff

    uint8_t configNr = 0;
    uint8_t s = 0;
    uint8_t c = 0;
    //
    uint8_t    TOC_Byte  = *(inbuf);

    configNr = (TOC_Byte & 0b11111000) >> 3;
    s        = (TOC_Byte & 0b00000100) >> 2;
    c        = (TOC_Byte & 0b00000011);

    /*  Configuration       Mode  Bandwidth  FrameSizes
        configNr 16 ... 19  CELT     NB      2.5, 5, 10, 20ms
        configNr 20 ... 23  CELT     WB      2.5, 5, 10, 20ms
        configNr 24 ... 27  CELT     SWB     2.5, 5, 10, 20ms
        configNr 28 ... 31  CELT     FB      2.5, 5, 10, 20ms

        s = 0: mono 1: stereo

        c = 0: 1 frame in the packet
        c = 1: 2 frames in the packet, each with equal compressed size
        c = 2: 2 frames in the packet, with different compressed sizes
        c = 3: an arbitrary number of frames in the packet
    */



    log_i("configNr %i", configNr);
    log_i("s %i", s);
    log_i("c %i", c);

    if(configNr < 12) return ERR_OPUS_SILK_MODE_UNSUPPORTED;
    if(configNr < 16) return ERR_OPUS_HYBRID_MODE_UNSUPPORTED;

    f_m_opusFramePacket = false;
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusComment(uint8_t *inbuf, int *bytesLeft){  // reference https://exiftool.org/TagNames/Vorbis.html#Comments
                                                       // reference https://www.rfc-editor.org/rfc/rfc7845#section-5
    int idx = OPUS_specialIndexOf(inbuf, "OpusTags", 10);
    if(idx != 0) return 0; //ERR_OPUS_DECODER_ASYNC;
    char* artist = NULL;
    char* title  = NULL;

    uint16_t pos = 8;
    *bytesLeft -= 8;
    uint32_t vendorLength       = *(inbuf + 11) << 24; // lengt of vendor string, e.g. Lavf58.65.101
             vendorLength      += *(inbuf + 10) << 16;
             vendorLength      += *(inbuf +  9) << 8;
             vendorLength      += *(inbuf +  8);
    pos += vendorLength + 4;
    *bytesLeft -= (vendorLength + 4);
    uint32_t commentListLength  = *(inbuf + 3 + pos) << 24; // nr. of comment entries
             commentListLength += *(inbuf + 2 + pos) << 16;
             commentListLength += *(inbuf + 1 + pos) << 8;
             commentListLength += *(inbuf + 0 + pos);
    pos += 4;
    *bytesLeft -= 4;
    for(int i = 0; i < commentListLength; i++){
        uint32_t commentStringLen   = *(inbuf + 3 + pos) << 24;
                 commentStringLen  += *(inbuf + 2 + pos) << 16;
                 commentStringLen  += *(inbuf + 1 + pos) << 8;
                 commentStringLen  += *(inbuf + 0 + pos);
        pos += 4;
        *bytesLeft -= 4;
        idx = OPUS_specialIndexOf(inbuf + pos, "artist=", 10);
        if(idx == 0){ artist = strndup((const char*)(inbuf + pos + 7), commentStringLen - 7);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "title=", 10);
        if(idx == 0){ title = strndup((const char*)(inbuf + pos + 6), commentStringLen - 6);
        }
        pos += commentStringLen;
        *bytesLeft -= commentStringLen;
    }
    if(artist && title){
        strcpy(m_chbuf, artist);
        strcat(m_chbuf, " - ");
        strcat(m_chbuf, title);
        f_m_newSt = true;
    }
    else if(artist){
        strcpy(m_chbuf, artist);
        f_m_newSt = true;
    }
    else if(title){
        strcpy(m_chbuf, title);
        f_m_newSt = true;
    }
    if(artist){free(artist); artist = NULL;}
    if(title) {free(title);  title = NULL;}

    f_m_commentHeader = false;
    f_m_nextPage = true;

    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusHead(uint8_t *inbuf, int *bytesLeft){  // reference https://wiki.xiph.org/OggOpus

    int idx = OPUS_specialIndexOf(inbuf, "OpusHead", 10);
    if(idx != 0) return 0; //ERR_OPUS_DECODER_ASYNC;

    uint8_t  version            = *(inbuf +  8); (void) version;
    uint8_t  channelCount       = *(inbuf +  9); // nr of channels
    uint16_t preSkip            = *(inbuf + 11) << 8;
             preSkip           += *(inbuf + 10);
    uint32_t sampleRate         = *(inbuf + 15) << 24;  // informational only
             sampleRate        += *(inbuf + 14) << 16;
             sampleRate        += *(inbuf + 13) << 8;
             sampleRate        += *(inbuf + 12);
    uint16_t outputGain         = *(inbuf + 17) << 8;  // Q7.8 in dB
             outputGain        += *(inbuf + 16);
    uint8_t  channelMap         = *(inbuf + 18);

    if(channelCount == 0 or channelCount >2) return ERR_OPUS_NR_OF_CHANNELS_UNSUPPORTED;
    m_channels = channelCount;
    if(sampleRate != 48000) return ERR_OPUS_INVALID_SAMPLERATE;
    if(channelMap > 1) return ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED;

    (void)outputGain;
    f_m_opusHeadWasRead = true; // The next Opus packet MUST contain the comment header.

    f_m_firstPage = false;
    f_m_nextPage = true;

    *bytesLeft -= 19;
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSparseOGG(uint8_t *inbuf, int *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    int idx = OPUS_specialIndexOf(inbuf, "OggS", 6);
    log_i("idx %i", idx);
    if(idx != 0) return 0; //ERR_OPUS_DECODER_ASYNC;

    uint8_t  version            = *(inbuf +  4); (void) version;
    uint8_t  headerType         = *(inbuf +  5); (void) headerType;
    uint64_t granulePosition    = (uint64_t)*(inbuf + 13) << 56;  // granule_position: an 8 Byte field containing -
             granulePosition   += (uint64_t)*(inbuf + 12) << 48;  // position information. For an audio stream, it MAY
             granulePosition   += (uint64_t)*(inbuf + 11) << 40;  // contain the total number of PCM samples encoded
             granulePosition   += (uint64_t)*(inbuf + 10) << 32;  // after including all frames finished on this page.
             granulePosition   += *(inbuf +  9) << 24;  // This is a hint for the decoder and gives it some timing
             granulePosition   += *(inbuf +  8) << 16;  // and position information. A special value of -1 (in two's
             granulePosition   += *(inbuf +  7) << 8;   // complement) indicates that no packets finish on this page.
             granulePosition   += *(inbuf +  6); (void) granulePosition;
    uint32_t bitstreamSerialNr  = *(inbuf + 17) << 24;  // bitstream_serial_number: a 4 Byte field containing the
             bitstreamSerialNr += *(inbuf + 16) << 16;  // unique serial number by which the logical bitstream
             bitstreamSerialNr += *(inbuf + 15) << 8;   // is identified.
             bitstreamSerialNr += *(inbuf + 14); (void) bitstreamSerialNr;
    uint32_t pageSequenceNr     = *(inbuf + 21) << 24;  // page_sequence_number: a 4 Byte field containing the sequence
             pageSequenceNr    += *(inbuf + 20) << 16;  // number of the page so the decoder can identify page loss
             pageSequenceNr    += *(inbuf + 19) << 8;   // This sequence number is increasing on each logical bitstream
             pageSequenceNr    += *(inbuf + 18); (void) pageSequenceNr;
    uint32_t CRCchecksum        = *(inbuf + 25) << 24;
             CRCchecksum       += *(inbuf + 24) << 16;
             CRCchecksum       += *(inbuf + 23) << 8;
             CRCchecksum       += *(inbuf + 22); (void) CRCchecksum;
    uint8_t  pageSegments       = *(inbuf + 26);        // giving the number of segment entries
    uint8_t  segmentTable       = *(inbuf + 27);        // number_page_segments

    bool     continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    bool     firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    bool     lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)
    uint16_t headerSize    = pageSegments + 27;

    (void)segmentTable; (void)continuedPage; (void)lastPage;

    log_i("headerType %i", headerType);
    log_i("firstPage %i", firstPage);
    log_i("continuedPage %i", continuedPage);
    log_i("lastPage %i", lastPage);
    log_i("headerSize %i", headerSize);
    log_i("granulePosition %u", granulePosition);
    if(firstPage) f_m_firstPage = true; // OpusHead follows
    if(f_m_opusHeadWasRead) f_m_commentHeader = true; // OpusComment follows

    if(!f_m_firstPage && !f_m_commentHeader) f_m_opusFramePacket = true; // await opus frame packet

    f_m_magicWordFound = false;
    f_m_nextPage = false;
    f_m_opusHeadWasRead = false;

    *bytesLeft -= headerSize;
    return 0; // no error
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSFindSyncWord(unsigned char *buf, int nBytes){
    // assume we have a ogg wrapper
    int idx = OPUS_specialIndexOf(buf, "OggS", nBytes);
    if(idx >= 0){ // Magic Word found
        log_i("OggS found at %i", idx);
        f_m_magicWordFound = true;
        return idx;
    }
    f_m_magicWordFound = false;
    return -1;
}
//----------------------------------------------------------------------------------------------------------------------












//----------------------------------------------------------------------------------------------------------------------
int OPUS_specialIndexOf(uint8_t* base, const char* str, int baselen, bool exact){
    int result;  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int i = 0; i < baselen - strlen(str); i++){
        result = i;
        for (int j = 0; j < strlen(str) + exact; j++){
            if (*(base + i + j) != *(str + j)){
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
