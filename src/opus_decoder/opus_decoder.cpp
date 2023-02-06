/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on: 06.02.2023
 */
//----------------------------------------------------------------------------------------------------------------------
//                                     O G G / O P U S     I M P L.
//----------------------------------------------------------------------------------------------------------------------
#include "opus_decoder.h"
#include "celt.h"

// global vars
bool      f_m_subsequentPage = false;
bool      f_m_parseOgg = false;
bool      f_m_newSt = false;  // streamTitle
bool      f_m_opusFramePacket = false;
uint8_t   m_channels = 0;
uint16_t  m_samplerate = 0;
uint32_t  m_segmentLength = 0;
char     *m_chbuf = NULL;
int32_t   s_validSamples = 0;
uint8_t   s_oldmode = 0;

uint16_t *m_segmentTable;
uint8_t   m_segmentTableSize = 0;
int16_t   s_segmentTableRdPtr = -1;
int8_t    error = 0;
float     m_CompressionRatio = 0;

bool OPUSDecoder_AllocateBuffers(){
    const uint32_t CELT_SET_END_BAND_REQUEST = 10012;
    const uint32_t CELT_SET_SIGNALLING_REQUEST = 10016;
    m_chbuf = (char*)malloc(512);
    if(!CELTDecoder_AllocateBuffers()) {log_e("CELT not init"); return false;}
    m_segmentTable = (uint16_t*)malloc(256 * sizeof(uint16_t));
    if(!m_segmentTable) {log_e("CELT not init"); return false;}
    CELTDecoder_ClearBuffer();
    OPUSDecoder_ClearBuffers();
    error = celt_decoder_init(2); if(error < 0) {log_e("CELT not init"); return false;}
    error = celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST,  0); if(error < 0) {log_e("CELT not init"); return false;}
    error = celt_decoder_ctl(CELT_SET_END_BAND_REQUEST,   21); if(error < 0) {log_e("CELT not init"); return false;}
    OPUSsetDefaults();
    return true;
}
void OPUSDecoder_FreeBuffers(){
    if(m_chbuf)        {free(m_chbuf);        m_chbuf = NULL;}
    if(m_segmentTable) {free(m_segmentTable); m_segmentTable = NULL;}
    CELTDecoder_FreeBuffers();
}
void OPUSDecoder_ClearBuffers(){
    if(m_chbuf)        memset(m_chbuf, 0, 512);
    if(m_segmentTable) memset(m_segmentTable, 0, 256 * sizeof(int16_t));
}
void OPUSsetDefaults(){
    f_m_subsequentPage = false;
    f_m_parseOgg = false;
    f_m_newSt = false;  // streamTitle
    f_m_opusFramePacket = false;
    m_channels = 0;
    m_samplerate = 0;
    m_segmentLength = 0;
    s_validSamples = 0;
    m_segmentTableSize = 0;
    s_oldmode = 0xFF;
    s_segmentTableRdPtr = -1;

    error = 0;
}

//----------------------------------------------------------------------------------------------------------------------

int OPUSDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf){

    if(f_m_parseOgg){
        int ret = OPUSparseOGG(inbuf, bytesLeft);
        if(ret == ERR_OPUS_NONE) return OPUS_PARSE_OGG_DONE; // ok
        else return ret;  // error
    }

    if(f_m_opusFramePacket){
        if(m_segmentTableSize > 0){
            s_segmentTableRdPtr++;
            m_segmentTableSize--;
            int len = m_segmentTable[s_segmentTableRdPtr];
            *bytesLeft -= len;
            int32_t ret = parseOpusTOC(inbuf[0]);
            if(ret < 0) return ret;
            int frame_size = opus_packet_get_samples_per_frame(inbuf, 48000);
            len--;
            inbuf++;

            ec_dec_init((uint8_t *)inbuf, len);
            ret = celt_decode_with_ec(inbuf, len, (int16_t*)outbuf, frame_size);
            if(ret < 0) return ret; // celt error
            s_validSamples = ret;

            if(m_segmentTableSize== 0){
                s_segmentTableRdPtr = -1; // back to the parking position
                f_m_opusFramePacket = false;
                f_m_parseOgg = true;
            }
        }
    }
    return ERR_OPUS_NONE;
}
//----------------------------------------------------------------------------------------------------------------------

int32_t opus_packet_get_samples_per_frame(const uint8_t *data, int32_t Fs) {
    int32_t audiosize;
    if (data[0] & 0x80) {
        audiosize = ((data[0] >> 3) & 0x3);
        audiosize = (Fs << audiosize) / 400;
    } else if ((data[0] & 0x60) == 0x60) {
        audiosize = (data[0] & 0x08) ? Fs / 50 : Fs / 100;
    } else {
        audiosize = ((data[0] >> 3) & 0x3);
        if (audiosize == 3)
            audiosize = Fs * 60 / 1000;
        else
            audiosize = (Fs << audiosize) / 100;
    }
    return audiosize;
}
//----------------------------------------------------------------------------------------------------------------------

uint8_t OPUSGetChannels(){
    return m_channels;
}
uint32_t OPUSGetSampRate(){
    return m_samplerate;
}
uint8_t OPUSGetBitsPerSample(){
    return 16;
}
uint32_t OPUSGetBitRate(){
    if(m_CompressionRatio != 0){
        return (16 * 2 * 48000) / m_CompressionRatio;  //bitsPerSample * channel* SampleRate/CompressionRatio
    }
    else return 0;
}
uint16_t OPUSGetOutputSamps(){
    return s_validSamples; // 1024
}
char* OPUSgetStreamTitle(){
    if(f_m_newSt){
        f_m_newSt = false;
        return m_chbuf;
    }
    return NULL;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusTOC(uint8_t TOC_Byte){  // https://www.rfc-editor.org/rfc/rfc6716  page 16 ff

    uint8_t mode = 0;
    uint8_t configNr = 0;
    uint8_t s = 0;
    uint8_t c = 0; (void)c;

    configNr = (TOC_Byte & 0b11111000) >> 3;
    s        = (TOC_Byte & 0b00000100) >> 2;
    c        = (TOC_Byte & 0b00000011);
    if(TOC_Byte & 0x80) mode = 2; else mode = 1;

    if(s_oldmode != mode) {
        s_oldmode = mode;
        if(mode == 2) log_i("opus mode is MODE_CELT_ONLY");
    }

    /*  Configuration       Mode  Bandwidth            FrameSizes         Audio Bandwidth   Sample Rate (Effective)
        configNr 16 ... 19  CELT  NB (narrowband)      2.5, 5, 10, 20ms   4 kHz             8 kHz
        configNr 20 ... 23  CELT  WB (wideband)        2.5, 5, 10, 20ms   8 kHz             16 kHz
        configNr 24 ... 27  CELT  SWB(super wideband)  2.5, 5, 10, 20ms   12 kHz            24 kHz
        configNr 28 ... 31  CELT  FB (fullband)        2.5, 5, 10, 20ms   20 kHz (*)        48 kHz     <-------

        (*) Although the sampling theorem allows a bandwidth as large as half the sampling rate, Opus never codes
        audio above 20 kHz, as that is the generally accepted upper limit of human hearing.

        s = 0: mono 1: stereo

        c = 0: 1 frame in the packet
        c = 1: 2 frames in the packet, each with equal compressed size
        c = 2: 2 frames in the packet, with different compressed sizes
        c = 3: an arbitrary number of frames in the packet
    */

    // log_i("configNr %i, s %i, c %i", configNr, s, c);

    if(configNr < 12) return ERR_OPUS_SILK_MODE_UNSUPPORTED;
    if(configNr < 16) return ERR_OPUS_HYBRID_MODE_UNSUPPORTED;

    return s;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusComment(uint8_t *inbuf, int nBytes){      // reference https://exiftool.org/TagNames/Vorbis.html#Comments
                                                       // reference https://www.rfc-editor.org/rfc/rfc7845#section-5
    int idx = OPUS_specialIndexOf(inbuf, "OpusTags", 10);
     if(idx != 0) return 0; // is not OpusTags
    char* artist = NULL;
    char* title  = NULL;

    uint16_t pos = 8;
    uint32_t vendorLength       = *(inbuf + 11) << 24; // lengt of vendor string, e.g. Lavf58.65.101
             vendorLength      += *(inbuf + 10) << 16;
             vendorLength      += *(inbuf +  9) << 8;
             vendorLength      += *(inbuf +  8);
    pos += vendorLength + 4;
    uint32_t commentListLength  = *(inbuf + 3 + pos) << 24; // nr. of comment entries
             commentListLength += *(inbuf + 2 + pos) << 16;
             commentListLength += *(inbuf + 1 + pos) << 8;
             commentListLength += *(inbuf + 0 + pos);
    pos += 4;
    for(int i = 0; i < commentListLength; i++){
        uint32_t commentStringLen   = *(inbuf + 3 + pos) << 24;
                 commentStringLen  += *(inbuf + 2 + pos) << 16;
                 commentStringLen  += *(inbuf + 1 + pos) << 8;
                 commentStringLen  += *(inbuf + 0 + pos);
        pos += 4;
        idx = OPUS_specialIndexOf(inbuf + pos, "artist=", 10);
        if(idx == 0){ artist = strndup((const char*)(inbuf + pos + 7), commentStringLen - 7);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "title=", 10);
        if(idx == 0){ title = strndup((const char*)(inbuf + pos + 6), commentStringLen - 6);
        }
        pos += commentStringLen;
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

    return 1;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusHead(uint8_t *inbuf, int nBytes){  // reference https://wiki.xiph.org/OggOpus

    int idx = OPUS_specialIndexOf(inbuf, "OpusHead", 10);
     if(idx != 0) return 0; //is not OpusHead
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

    if(channelCount == 0 or channelCount >2) return ERR_OPUS_CHANNELS_OUT_OF_RANGE;
    m_channels = channelCount;
    if(sampleRate != 48000) return ERR_OPUS_INVALID_SAMPLERATE;
    m_samplerate = sampleRate;
    if(channelMap > 1) return ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED;

    (void)outputGain;

    return 1;
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSparseOGG(uint8_t *inbuf, int *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    f_m_parseOgg = false;
    int ret = 0;
    int idx = OPUS_specialIndexOf(inbuf, "OggS", 6);
    if(idx != 0) return ERR_OPUS_DECODER_ASYNC;

    int16_t segmentTableWrPtr = -1;

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

    // read the segment table (contains pageSegments bytes),  1...251: Length of the frame in bytes,
    // 255: A second byte is needed.  The total length is first_byte + second byte
    m_segmentLength = 0;
    segmentTableWrPtr = -1;

    for(int i = 0; i < pageSegments; i++){
        int n = *(inbuf + 27 + i);
        while(*(inbuf + 27 + i) == 255){
            i++;
            n+= *(inbuf + 27 + i);
        }
        segmentTableWrPtr++;
        m_segmentTable[segmentTableWrPtr] = n;
        m_segmentLength += n;
    }
    m_segmentTableSize = segmentTableWrPtr + 1;
    m_CompressionRatio = (float)(960 * 2 * pageSegments)/m_segmentLength;  // const 960 validBytes out

    bool     continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    bool     firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    bool     lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)

    uint16_t headerSize    = pageSegments + 27;
    (void)continuedPage; (void)lastPage;
    *bytesLeft -= headerSize;

    if(firstPage || f_m_subsequentPage){ // OpusHead or OggComment may follows
        ret = parseOpusHead(inbuf + headerSize, m_segmentTable[0]);
        if(ret == 1) *bytesLeft -= m_segmentTable[0];
        if(ret < 0){ *bytesLeft -= m_segmentTable[0]; return ret;}
        ret = parseOpusComment(inbuf + headerSize, m_segmentTable[0]);
        if(ret == 1) *bytesLeft -= m_segmentTable[0];
        if(ret < 0){ *bytesLeft -= m_segmentTable[0]; return ret;}
        f_m_parseOgg = true;// goto next page
    }

    f_m_opusFramePacket = true;
    if(firstPage) f_m_subsequentPage = true; else f_m_subsequentPage = false;

    return ERR_OPUS_NONE; // no error
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSFindSyncWord(unsigned char *buf, int nBytes){
    // assume we have a ogg wrapper
    int idx = OPUS_specialIndexOf(buf, "OggS", nBytes);
    if(idx >= 0){ // Magic Word found
        log_i("OggS found at %i", idx);
        f_m_parseOgg = true;
        return idx;
    }
    log_i("find sync");
    f_m_parseOgg = false;
    return ERR_OPUS_OGG_SYNC_NOT_FOUND;
}
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
