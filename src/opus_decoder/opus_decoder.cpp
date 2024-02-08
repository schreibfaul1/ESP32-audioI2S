/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on: 08.02.2024
 */
//----------------------------------------------------------------------------------------------------------------------
//                                     O G G / O P U S     I M P L.
//----------------------------------------------------------------------------------------------------------------------
#include "opus_decoder.h"
#include "celt.h"
#include "Arduino.h"
#include <vector>


// global vars
bool      s_f_opusParseOgg = false;
bool      s_f_newSteamTitle = false;  // streamTitle
bool      s_f_opusNewMetadataBlockPicture = false; // new metadata block picture
bool      s_f_opusStereoFlag = false;
bool      s_f_continuedPage = false;
bool      s_f_firstPage = false;
bool      s_f_lastPage = false;
bool      s_f_nextChunk = false;

uint8_t   s_opusChannels = 0;
uint8_t   s_opusCountCode =  0;
uint8_t   s_opusPageNr = 0;
uint16_t  s_opusOggHeaderSize = 0;
uint32_t  s_opusSamplerate = 0;
uint32_t  s_opusSegmentLength = 0;
uint32_t  s_opusCurrentFilePos = 0;
int32_t   s_opusBlockPicLen = 0;
int32_t   s_blockPicLenUntilFrameEnd = 0;
int32_t   s_opusRemainBlockPicLen = 0;
uint32_t  s_opusBlockPicPos = 0;
uint32_t  s_opusBlockLen = 0;
char     *s_opusChbuf = NULL;
int32_t   s_opusValidSamples = 0;

uint16_t *s_opusSegmentTable;
uint8_t   s_opusSegmentTableSize = 0;
int16_t   s_opusSegmentTableRdPtr = -1;
int8_t    s_opusError = 0;
float     s_opusCompressionRatio = 0;

std::vector <uint32_t>s_opusBlockPicItem;

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
    s_f_opusParseOgg = false;
    s_f_newSteamTitle = false;  // streamTitle
    s_f_opusNewMetadataBlockPicture = false;
    s_f_opusStereoFlag = false;
    s_opusChannels = 0;
    s_opusSamplerate = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
    s_opusBlockPicPos = 0;
    s_opusCurrentFilePos = 0;
    s_opusBlockPicLen = 0;
    s_opusRemainBlockPicLen = 0;
    s_blockPicLenUntilFrameEnd = 0;
    s_opusBlockLen = 0;
    s_opusPageNr = 0;
    s_opusError = 0;
    s_opusBlockPicItem.clear(); s_opusBlockPicItem.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------

int OPUSDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf){

    const uint32_t CELT_SET_END_BAND_REQUEST = 10012;
    static uint16_t fs = 0;
    static uint8_t M = 0;
    static uint8_t configNr = 31; // FULLBAND
    static uint16_t paddingBytes = 0;
    uint8_t paddingLength = 0;
    uint8_t endband = 21;
    static uint16_t samplesPerFrame = 0;
    int ret = ERR_OPUS_NONE;
    int len = 0;

    if(s_f_opusParseOgg){
        s_f_opusParseOgg = false;
        fs = 0;
        M = 0;
        paddingBytes = 0;
        s_opusCountCode = 0;
        ret = OPUSparseOGG(inbuf, bytesLeft);
        if(ret != ERR_OPUS_NONE) return ret; // error
        inbuf += s_opusOggHeaderSize;
    }

    if(s_opusPageNr == 0){   // OpusHead
        s_f_opusParseOgg = true;// goto next page
        ret = parseOpusHead(inbuf, s_opusSegmentTable[0]);
        *bytesLeft           -= s_opusSegmentTable[0];
        s_opusCurrentFilePos += s_opusSegmentTable[0];
        if(ret == 1){ s_opusPageNr++;}
        if(ret == 0){ log_e("OpusHead not found"); }
        return OPUS_PARSE_OGG_DONE;
    }
    if(s_opusPageNr == 1){   // OpusComment
        if(s_opusBlockLen > 0) {goto processChunk;}
        ret = parseOpusComment(inbuf, s_opusSegmentTable[0]);
        if(ret == 0) log_e("OpusCommemtPage not found");
        s_opusBlockLen = s_opusSegmentTable[0];
processChunk:     // we can't return more than 2* 4096 bytes at a time (max OPUS frame size)
        if(s_opusBlockLen <= 8192) {
            *bytesLeft           -= s_opusBlockLen;
            s_opusCurrentFilePos += s_opusBlockLen;
            s_opusBlockLen = 0;
            s_opusPageNr++;
            s_f_nextChunk = true;
            s_f_opusParseOgg = true;
        }
        else{
            s_opusBlockLen -= 8192;
            *bytesLeft -= 8192;
            s_opusCurrentFilePos += 8192;
        }
        return OPUS_PARSE_OGG_DONE;
    }
    if(s_opusPageNr == 2){  // OpusComment Subsequent Pages
        static int32_t opusSegmentLength = 0;
        if(s_f_nextChunk){
            s_f_nextChunk = false;
            opusSegmentLength = s_opusSegmentLength;
        }
        // log_i("page(2) opusSegmentLength %i, s_opusRemainBlockPicLen %i", opusSegmentLength, s_opusRemainBlockPicLen);

        if(s_opusRemainBlockPicLen <= 0 && opusSegmentLength <= 0){
            s_opusPageNr++; // fall through
            s_f_opusParseOgg = true;
            if(s_opusBlockPicItem.size() > 0){ // get blockpic data
                // log_i("---------------------------------------------------------------------------");
                // log_i("metadata blockpic found at pos %i, size %i bytes", s_opusBlockPicPos, s_opusBlockPicLen);
                // for(int i = 0; i < s_opusBlockPicItem.size(); i += 2){
                //     log_i("segment %02i, pos %07i, len %05i", i / 2, s_opusBlockPicItem[i], s_opusBlockPicItem[i + 1]);
                // }
                // log_i("---------------------------------------------------------------------------");
                s_f_opusNewMetadataBlockPicture = true;
            }
            return OPUS_PARSE_OGG_DONE;
        }
        if(opusSegmentLength == 0){
            s_f_opusParseOgg = true;
            s_f_nextChunk = true;
            return OPUS_PARSE_OGG_DONE;
        }

        int m = min(s_opusRemainBlockPicLen, opusSegmentLength);
        if(m > 8192){
            s_opusRemainBlockPicLen -= 8192;
            opusSegmentLength -= 8192;
            *bytesLeft -= 8192;
            s_opusCurrentFilePos += 8192;
            return OPUS_PARSE_OGG_DONE;
        }
        if(m == s_opusRemainBlockPicLen){
            *bytesLeft -= s_opusRemainBlockPicLen;
            s_opusCurrentFilePos += s_opusRemainBlockPicLen;
            opusSegmentLength -= s_opusRemainBlockPicLen;
            s_opusRemainBlockPicLen = 0;
            *bytesLeft -= opusSegmentLength; // paddind bytes
            s_opusCurrentFilePos += opusSegmentLength;
            opusSegmentLength = 0;
            return OPUS_PARSE_OGG_DONE;
        }
        if(m == opusSegmentLength){
            *bytesLeft -= opusSegmentLength;
            s_opusCurrentFilePos += opusSegmentLength;
            s_opusRemainBlockPicLen -= opusSegmentLength;
            opusSegmentLength = 0;
            return OPUS_PARSE_OGG_DONE;
        }
        log_e("never reach this!");
    }


    if(s_opusCountCode > 0) goto FramePacking; // more than one frame in the packet

    if(s_opusSegmentTableSize > 0){
        s_opusSegmentTableRdPtr++;
        s_opusSegmentTableSize--;
        len = s_opusSegmentTable[s_opusSegmentTableRdPtr];
    }

    configNr = parseOpusTOC(inbuf[0]);
    samplesPerFrame = opus_packet_get_samples_per_frame(inbuf, s_opusSamplerate);
    switch(configNr){
        case 16 ... 19: endband = 13; // OPUS_BANDWIDTH_NARROWBAND
                        break;
        case 20 ... 23: endband = 17; // OPUS_BANDWIDTH_WIDEBAND
                        break;
        case 24 ... 27: endband = 19; // OPUS_BANDWIDTH_SUPERWIDEBAND
                        break;
        case 28 ... 31: endband = 21; // OPUS_BANDWIDTH_FULLBAND
                        break;
        default:        log_e("unknown bandwidth, configNr is: %i", configNr);
                        break;
    }
    celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, endband);

FramePacking:            // https://www.tech-invite.com/y65/tinv-ietf-rfc-6716-2.html   3.2. Frame Packing


    switch(s_opusCountCode){
        case 0:  // Code 0: One Frame in the Packet
            *bytesLeft -= len;
            s_opusCurrentFilePos += len;
            len--;
            inbuf++;
            ec_dec_init((uint8_t *)inbuf, len);
            ret = celt_decode_with_ec(inbuf, len, (int16_t*)outbuf, samplesPerFrame);
            if(ret < 0) goto exit; // celt error
            s_opusValidSamples = ret;
            ret = ERR_OPUS_NONE;
            break;
        case 1:  // Code 1: Two Frames in the Packet, Each with Equal Compressed Size
            log_e("OPUS countCode 1 not supported yet"); vTaskDelay(1000); // todo
            break;
        case 2:  // Code 2: Two Frames in the Packet, with Different Compressed Sizes
            log_e("OPUS countCode 2 not supported yet"); vTaskDelay(1000); // todo
            break;
        case 3: // Code 3: A Signaled Number of Frames in the Packet
            if(M == 0){
                bool v = ((inbuf[1] & 0x80) == 0x80);  // VBR indicator
                if(v != 0) {log_e("OPUS countCode 3 with VBR not supported yet"); vTaskDelay(1000);} // todo
                bool p = ((inbuf[1] & 0x40) == 0x40);  // padding bit
                M = inbuf[1] & 0x3F;           // max framecount
            //    log_i("v %i, p %i, M %i", v, p, M);
                *bytesLeft -= 2;
                s_opusCurrentFilePos += 2;
                len        -= 2;
                inbuf      += 2;

                if(p){
                    paddingBytes = 0;
                    paddingLength = 1;

                    int i = 0;
                    while(inbuf[i] == 255){
                        paddingBytes += inbuf[i];
                        i++;
                    }
                    paddingBytes += inbuf[i];
                    paddingLength += i;

                    *bytesLeft -= paddingLength;
                    s_opusCurrentFilePos += paddingLength;
                    len        -= paddingLength;
                    inbuf      += paddingLength;
                }
                    fs = (len - paddingBytes) / M;
            }
            *bytesLeft -= fs;
            s_opusCurrentFilePos += fs;
            ec_dec_init((uint8_t *)inbuf, fs);
            ret = celt_decode_with_ec(inbuf, fs, (int16_t*)outbuf, samplesPerFrame);
            if(ret < 0) goto exit; // celt error
            s_opusValidSamples = ret;
            M--;
         //   log_i("M %i fs %i spf %i", M, fs, samplesPerFrame);
            ret = ERR_OPUS_NONE;
            if(M == 0) {s_opusCountCode = 0; *bytesLeft -= paddingBytes; paddingBytes = 0; goto exit;}
            return ret;
            break;
        default:
            log_e("unknown countCode %i", s_opusCountCode);
            break;

    }

exit:
    if(s_opusSegmentTableSize == 0){
        s_opusSegmentTableRdPtr = -1; // back to the parking position
        s_f_opusParseOgg = true;
    }
    return ret;
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
vector<uint32_t> OPUSgetMetadataBlockPicture(){
    if(s_f_opusNewMetadataBlockPicture){
        s_f_opusNewMetadataBlockPicture = false;
        return s_opusBlockPicItem;
    }
    if(s_opusBlockPicItem.size() > 0){
        s_opusBlockPicItem.clear();
        s_opusBlockPicItem.shrink_to_fit();
    }
    return s_opusBlockPicItem;
}

//----------------------------------------------------------------------------------------------------------------------
int parseOpusTOC(uint8_t TOC_Byte){  // https://www.rfc-editor.org/rfc/rfc6716  page 16 ff

    uint8_t configNr = 0;
    uint8_t s = 0;              // stereo flag
    uint8_t c = 0; (void)c;     // count code

    configNr = (TOC_Byte & 0b11111000) >> 3;
    s        = (TOC_Byte & 0b00000100) >> 2;
    c        = (TOC_Byte & 0b00000011);

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
    s_opusCountCode = c;
    s_f_opusStereoFlag = s;

    if(configNr < 12) return ERR_OPUS_SILK_MODE_UNSUPPORTED;
    if(configNr < 16) return ERR_OPUS_HYBRID_MODE_UNSUPPORTED;
    // if(configNr < 20) return ERR_OPUS_NARROW_BAND_UNSUPPORTED;
    // if(configNr < 24) return ERR_OPUS_WIDE_BAND_UNSUPPORTED;
    // if(configNr < 28) return ERR_OPUS_SUPER_WIDE_BAND_UNSUPPORTED;

    return configNr;
}
//----------------------------------------------------------------------------------------------------------------------
int parseOpusComment(uint8_t *inbuf, int nBytes){      // reference https://exiftool.org/TagNames/Vorbis.html#Comments
                                                       // reference https://www.rfc-editor.org/rfc/rfc7845#section-5
    int idx = OPUS_specialIndexOf(inbuf, "OpusTags", 10);
    if(idx != 0) return 0; // is not OpusTags

    char* artist = NULL;
    char* title  = NULL;

    uint16_t pos = 8;
             nBytes -= 8;
    uint32_t vendorLength       = *(inbuf + 11) << 24; // lengt of vendor string, e.g. Lavf58.65.101
             vendorLength      += *(inbuf + 10) << 16;
             vendorLength      += *(inbuf +  9) << 8;
             vendorLength      += *(inbuf +  8);
    pos += vendorLength + 4;
    nBytes -= (vendorLength + 4);
    uint32_t commentListLength  = *(inbuf + 3 + pos) << 24; // nr. of comment entries
             commentListLength += *(inbuf + 2 + pos) << 16;
             commentListLength += *(inbuf + 1 + pos) << 8;
             commentListLength += *(inbuf + 0 + pos);
    pos += 4;
    nBytes -= 4;
    for(int i = 0; i < commentListLength; i++){
        uint32_t commentStringLen   = *(inbuf + 3 + pos) << 24;
                 commentStringLen  += *(inbuf + 2 + pos) << 16;
                 commentStringLen  += *(inbuf + 1 + pos) << 8;
                 commentStringLen  += *(inbuf + 0 + pos);
        pos += 4;
        nBytes -= 4;
        idx = OPUS_specialIndexOf(inbuf + pos, "artist=", 10);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "ARTIST=", 10);
        if(idx == 0){ artist = strndup((const char*)(inbuf + pos + 7), commentStringLen - 7);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "title=", 10);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "TITLE=", 10);
        if(idx == 0){ title = strndup((const char*)(inbuf + pos + 6), commentStringLen - 6);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "metadata_block_picture=", 25);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "METADATA_BLOCK_PICTURE=", 25);
        if(idx == 0){
            s_opusBlockPicLen = commentStringLen - 23;
            s_opusBlockPicPos += s_opusCurrentFilePos + pos + 23;
            s_blockPicLenUntilFrameEnd = nBytes - 23;
        //  log_i("metadata block picture found at pos %i, length %i", s_opusBlockPicPos, s_opusBlockPicLen);
            uint32_t pLen = _min(s_blockPicLenUntilFrameEnd, s_opusBlockPicLen);
            if(pLen){
                s_opusBlockPicItem.push_back(s_opusBlockPicPos);
                s_opusBlockPicItem.push_back(pLen);
            }
            s_opusRemainBlockPicLen = s_opusBlockPicLen - s_blockPicLenUntilFrameEnd;
        }
        pos += commentStringLen;
        nBytes -= commentStringLen;
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

    if(channelCount == 0 || channelCount >2) return ERR_OPUS_CHANNELS_OUT_OF_RANGE;
    s_opusChannels = channelCount;
    if(sampleRate != 48000) return ERR_OPUS_INVALID_SAMPLERATE;
    s_opusSamplerate = sampleRate;
    if(channelMap > 1) return ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED;

    (void)outputGain;

    return 1;
}

//----------------------------------------------------------------------------------------------------------------------
int OPUSparseOGG(uint8_t *inbuf, int *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

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
            if(i == pageSegments) break;
            n+= *(inbuf + 27 + i);
        }
        segmentTableWrPtr++;
        s_opusSegmentTable[segmentTableWrPtr] = n;
        s_opusSegmentLength += n;
    }
    s_opusSegmentTableSize = segmentTableWrPtr + 1;
    s_opusCompressionRatio = (float)(960 * 2 * pageSegments)/s_opusSegmentLength;  // const 960 validBytes out

    s_f_continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    s_f_firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    s_f_lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)

//  log_i("firstPage %i, continuedPage %i, lastPage %i",s_f_firstPage, s_f_continuedPage, s_f_lastPage);

    uint16_t headerSize   = pageSegments + 27;
    *bytesLeft           -= headerSize;
    s_opusCurrentFilePos += headerSize;
    s_opusOggHeaderSize   = headerSize;

    int32_t pLen = _min((int32_t)s_opusSegmentLength, s_opusRemainBlockPicLen);
//  log_i("s_opusSegmentLength %i, s_opusRemainBlockPicLen %i", s_opusSegmentLength, s_opusRemainBlockPicLen);
    if(s_opusBlockPicLen && pLen > 0){
        s_opusBlockPicItem.push_back(s_opusCurrentFilePos);
        s_opusBlockPicItem.push_back(pLen);
    }
    return ERR_OPUS_NONE;
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
