/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on: 07.04.2023
 */
//----------------------------------------------------------------------------------------------------------------------
//                                     O G G / O P U S     I M P L.
//----------------------------------------------------------------------------------------------------------------------
#include "opus_decoder.h"
#include "celt.h"

// global vars
bool      s_f_opusSubsequentPage = false;
bool      s_f_opusParseOgg = false;
bool      s_f_newSteamTitle = false;  // streamTitle
bool      s_f_opusFramePacket = false;
uint8_t   s_opusChannels = 0;
uint16_t  s_opusSamplerate = 0;
uint32_t  s_opusSegmentLength = 0;
char     *s_opusChbuf = NULL;
int32_t   s_opusValidSamples = 0;
uint8_t   s_opusOldMode = 0;

uint16_t *s_opusSegmentTable;
uint8_t   s_opusSegmentTableSize = 0;
int16_t   s_opusSegmentTableRdPtr = -1;
int8_t    s_opusError = 0;
float     s_opusCompressionRatio = 0;

bool OPUSDecoder_AllocateBuffers(){
    const uint32_t CELT_SET_END_BAND_REQUEST = 10012;
    const uint32_t CELT_SET_SIGNALLING_REQUEST = 10016;
    s_opusChbuf = (char*)malloc(512);
    if(!CELTDecoder_AllocateBuffers()) {log_e("CELT not init"); return false;}
    s_opusSegmentTable = (uint16_t*)malloc(256 * sizeof(uint16_t));
    if(!s_opusSegmentTable) {log_e("CELT not init"); return false;}
    CELTDecoder_ClearBuffer();
    OPUSDecoder_ClearBuffers();
    s_opusError = celt_decoder_init(2); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    s_opusError = celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST,  0); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    s_opusError = celt_decoder_ctl(CELT_SET_END_BAND_REQUEST,   21); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    OPUSsetDefaults();
    return true;
}
void OPUSDecoder_FreeBuffers(){
    if(s_opusChbuf)        {free(s_opusChbuf);        s_opusChbuf = NULL;}
    if(s_opusSegmentTable) {free(s_opusSegmentTable); s_opusSegmentTable = NULL;}
    CELTDecoder_FreeBuffers();
}
void OPUSDecoder_ClearBuffers(){
    if(s_opusChbuf)        memset(s_opusChbuf, 0, 512);
    if(s_opusSegmentTable) memset(s_opusSegmentTable, 0, 256 * sizeof(int16_t));
}
void OPUSsetDefaults(){
    s_f_opusSubsequentPage = false;
    s_f_opusParseOgg = false;
    s_f_newSteamTitle = false;  // streamTitle
    s_f_opusFramePacket = false;
    s_opusChannels = 0;
    s_opusSamplerate = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOldMode = 0xFF;
    s_opusSegmentTableRdPtr = -1;

    s_opusError = 0;
}

//----------------------------------------------------------------------------------------------------------------------

int OPUSDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf){

    if(s_f_opusParseOgg){
        int ret = OPUSparseOGG(inbuf, bytesLeft);
        if(ret == ERR_OPUS_NONE) return OPUS_PARSE_OGG_DONE; // ok
        else return ret;  // error
    }

    if(s_f_opusFramePacket){
        if(s_opusSegmentTableSize > 0){
            s_opusSegmentTableRdPtr++;
            s_opusSegmentTableSize--;
            int len = s_opusSegmentTable[s_opusSegmentTableRdPtr];
            *bytesLeft -= len;
            int32_t ret = parseOpusTOC(inbuf[0]);
            if(ret < 0) return ret;
            int frame_size = opus_packet_get_samples_per_frame(inbuf, 48000);
            len--;
            inbuf++;

            ec_dec_init((uint8_t *)inbuf, len);
            ret = celt_decode_with_ec(inbuf, len, (int16_t*)outbuf, frame_size);
            if(ret < 0) return ret; // celt error
            s_opusValidSamples = ret;

            if(s_opusSegmentTableSize== 0){
                s_opusSegmentTableRdPtr = -1; // back to the parking position
                s_f_opusFramePacket = false;
                s_f_opusParseOgg = true;
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
    return s_opusChannels;
}
uint32_t OPUSGetSampRate(){
    return s_opusSamplerate;
}
uint8_t OPUSGetBitsPerSample(){
    return 16;
}
uint32_t OPUSGetBitRate(){
    if(s_opusCompressionRatio != 0){
        return (16 * 2 * 48000) / s_opusCompressionRatio;  //bitsPerSample * channel* SampleRate/CompressionRatio
    }
    else return 0;
}
uint16_t OPUSGetOutputSamps(){
    return s_opusValidSamples; // 1024
}
char* OPUSgetStreamTitle(){
    if(s_f_newSteamTitle){
        s_f_newSteamTitle = false;
        return s_opusChbuf;
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

    if(s_opusOldMode != mode) {
        s_opusOldMode = mode;
    //    if(mode == 2) log_i("opus mode is MODE_CELT_ONLY");
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
        strcpy(s_opusChbuf, artist);
        strcat(s_opusChbuf, " - ");
        strcat(s_opusChbuf, title);
        s_f_newSteamTitle = true;
    }
    else if(artist){
        strcpy(s_opusChbuf, artist);
        s_f_newSteamTitle = true;
    }
    else if(title){
        strcpy(s_opusChbuf, title);
        s_f_newSteamTitle = true;
    }
    if(artist){free(artist); artist = NULL;}
    if(title) {free(title);  title = NULL;}

    return 1;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusHead(uint8_t *inbuf, int nBytes){  // reference https://wiki.xiph.org/OggOpus


    int idx = OPUS_specialIndexOf(inbuf, "OpusHead", 10);
     if(idx != 0) {
        return 0; //is not OpusHead
     }
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
    s_opusChannels = channelCount;
    if(sampleRate != 48000) return ERR_OPUS_INVALID_SAMPLERATE;
    s_opusSamplerate = sampleRate;
    if(channelMap > 1) return ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED;

    (void)outputGain;

    return 1;
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSparseOGG(uint8_t *inbuf, int *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    s_f_opusParseOgg = false;
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
    s_opusSegmentLength = 0;
    segmentTableWrPtr = -1;

    for(int i = 0; i < pageSegments; i++){
        int n = *(inbuf + 27 + i);
        while(*(inbuf + 27 + i) == 255){
            i++;
            n+= *(inbuf + 27 + i);
        }
        segmentTableWrPtr++;
        s_opusSegmentTable[segmentTableWrPtr] = n;
        s_opusSegmentLength += n;
    }
    s_opusSegmentTableSize = segmentTableWrPtr + 1;
    s_opusCompressionRatio = (float)(960 * 2 * pageSegments)/s_opusSegmentLength;  // const 960 validBytes out

    bool     continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    bool     firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    bool     lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)

    uint16_t headerSize    = pageSegments + 27;
    (void)continuedPage; (void)lastPage;
    *bytesLeft -= headerSize;

    if(firstPage || s_f_opusSubsequentPage){ // OpusHead or OggComment may follows
        ret = parseOpusHead(inbuf + headerSize, s_opusSegmentTable[0]);
        if(ret == 1) *bytesLeft -= s_opusSegmentTable[0];
        if(ret < 0){ *bytesLeft -= s_opusSegmentTable[0]; return ret;}
        ret = parseOpusComment(inbuf + headerSize, s_opusSegmentTable[0]);
        if(ret == 1) *bytesLeft -= s_opusSegmentTable[0];
        if(ret < 0){ *bytesLeft -= s_opusSegmentTable[0]; return ret;}
        s_f_opusParseOgg = true;// goto next page
    }

    s_f_opusFramePacket = true;
    if(firstPage) s_f_opusSubsequentPage = true; else s_f_opusSubsequentPage = false;

    return ERR_OPUS_NONE; // no error
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSFindSyncWord(unsigned char *buf, int nBytes){
    // assume we have a ogg wrapper
    int idx = OPUS_specialIndexOf(buf, "OggS", nBytes);
    if(idx >= 0){ // Magic Word found
    //    log_i("OggS found at %i", idx);
        s_f_opusParseOgg = true;
        return idx;
    }
    log_i("find sync");
    s_f_opusParseOgg = false;
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
