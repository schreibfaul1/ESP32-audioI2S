/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on: 21.07.2025
 */
//----------------------------------------------------------------------------------------------------------------------
//                                     O G G / O P U S     I M P L.
//----------------------------------------------------------------------------------------------------------------------
#include "opus_decoder.h"
#include "celt.h"
#include "silk.h"
#include "Arduino.h"
#include <vector>

// global vars
const uint32_t CELT_SET_END_BAND_REQUEST        = 10012;
const uint32_t CELT_SET_CHANNELS_REQUEST        = 10008;
const uint32_t CELT_SET_START_BAND_REQUEST      = 10010;
const uint32_t CELT_SET_SIGNALLING_REQUEST      = 10016;
const uint32_t CELT_GET_AND_CLEAR_ERROR_REQUEST = 10007;
const uint32_t CELT_GET_MODE_REQUEST            = 10015;

extern silk_ptr_obj<silk_DecControlStruct_t>    s_silk_DecControlStruct;

enum {OPUS_BANDWIDTH_NARROWBAND = 1101,    OPUS_BANDWIDTH_MEDIUMBAND = 1102, OPUS_BANDWIDTH_WIDEBAND = 1103,
      OPUS_BANDWIDTH_SUPERWIDEBAND = 1104, OPUS_BANDWIDTH_FULLBAND = 1105};


uint8_t          s_opusChannels = 0;
uint8_t          s_opusCountCode = 0;
uint8_t          s_opusPageNr = 0;
uint8_t          s_frameCount = 0;
uint8_t          s_opusSegmentTableSize = 0;
uint16_t         s_mode = 0;
uint16_t         s_opusOggHeaderSize = 0;
uint16_t         s_bandWidth = 0;
uint16_t         s_internalSampleRate = 0;
uint16_t         s_endband = 0;
uint32_t         s_opusSamplerate = 0;
uint32_t         s_opusSegmentLength = 0;
uint32_t         s_opusCurrentFilePos = 0;
uint32_t         s_opusAudioDataStart = 0;
uint32_t         s_opusBlockPicPos = 0;
uint32_t         s_opusBlockLen = 0;
bool             s_f_opusParseOgg = false;
bool             s_f_newSteamTitle = false;               // streamTitle
bool             s_f_opusNewMetadataBlockPicture = false; // new metadata block picture
bool             s_f_opusStereoFlag = false;
bool             s_f_continuedPage = false;
bool             s_f_firstPage = false;
bool             s_f_lastPage = false;
bool             s_f_nextChunk = false;
int8_t           s_opusError = 0;
int16_t          s_opusSegmentTableRdPtr = -1;
int16_t          s_prev_mode = 0;
int32_t          s_opusValidSamples = 0;
int32_t          s_opusBlockPicLen = 0;
int32_t          s_blockPicLenUntilFrameEnd = 0;
int32_t          s_opusRemainBlockPicLen = 0;
int32_t          s_opusCommentBlockSize = 0;
float            s_opusCompressionRatio = 0;

ps_ptr<char>     s_streamTitle;
ps_ptr<uint16_t> s_opusSegmentTable;

ofp2  s_ofp2; // used in opus_FramePacking_Code2
ofp3  s_ofp3; // used in opus_FramePacking_Code3
odp3  s_odp3; // used in opusDecodePage3

std::vector <uint32_t>s_opusBlockPicItem;

bool OPUSDecoder_AllocateBuffers(){
    if(!SILKDecoder_AllocateBuffers()) {return false; /*ERR_OPUS_SILK_DEC_NOT_INIT*/}
    if(!CELTDecoder_AllocateBuffers()) {return false; /*ERR_OPUS_CELT_NOT_INIT*/}
    s_opusSegmentTable.alloc_array(256);
    CELTDecoder_ClearBuffer();
    SILKDecoder_ClearBuffers();
    OPUSDecoder_ClearBuffers();
    // allocate CELT buffers after OPUS head (nr of channels is needed)
    s_opusError = celt_decoder_init(2); if(s_opusError < 0) {return false; /*ERR_OPUS_CELT_NOT_INIT;*/}
    s_opusError = celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST,  0); if(s_opusError < 0) {return false; /*ERR_OPUS_CELT_NOT_INIT;*/}
    s_opusError = celt_decoder_ctl(CELT_SET_END_BAND_REQUEST,   21); if(s_opusError < 0) {return false; /*ERR_OPUS_CELT_NOT_INIT;*/}
    OPUSsetDefaults();

    int32_t ret = 0, silkDecSizeBytes = 0;
    (void) ret;
    (void) silkDecSizeBytes;
    silk_InitDecoder();
    return true;
}
void OPUSDecoder_FreeBuffers(){
    s_opusSegmentTable.reset();
    s_streamTitle.reset();
    s_frameCount = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
    SILKDecoder_FreeBuffers();
    CELTDecoder_FreeBuffers();
}
void OPUSDecoder_ClearBuffers(){
    s_streamTitle.clear();
    s_opusSegmentTable.clear();
    s_frameCount = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
}
void OPUSsetDefaults(){
    memset(&s_ofp2, 0, sizeof(s_ofp2));
    memset(&s_ofp3, 0, sizeof(s_ofp3));
    memset(&s_odp3, 0, sizeof(s_odp3));
    s_ofp3.firstCall = true;
    s_f_opusParseOgg = false;
    s_f_newSteamTitle = false;  // streamTitle
    s_f_opusNewMetadataBlockPicture = false;
    s_f_opusStereoFlag = false;
    s_f_lastPage = false;
    s_opusChannels = 0;
    s_frameCount = 0;
    s_mode = 0;
    s_opusSamplerate = 0;
    s_internalSampleRate = 0;
    s_bandWidth = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
    s_opusBlockPicPos = 0;
    s_opusCurrentFilePos = 0;
    s_opusAudioDataStart = 0;
    s_opusBlockPicLen = 0;
    s_opusCommentBlockSize = 0;
    s_opusRemainBlockPicLen = 0;
    s_blockPicLenUntilFrameEnd = 0;
    s_opusBlockLen = 0;
    s_opusPageNr = 0;
    s_opusError = 0;
    s_endband = 0;
    s_prev_mode = MODE_NONE;
    s_opusBlockPicItem.clear(); s_opusBlockPicItem.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------

int32_t OPUSDecode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf) {
    int32_t ret = OPUS_NONE;
    int32_t segmLen = 0;

    if(s_opusCommentBlockSize) {
        if(s_opusCommentBlockSize > 8192) {
            s_opusRemainBlockPicLen -= 8192;
            *bytesLeft -= 8192;
            s_opusCurrentFilePos += 8192;
            s_opusCommentBlockSize -= 8192;
        }
        else {
            s_opusRemainBlockPicLen -= s_opusCommentBlockSize;
            *bytesLeft -= s_opusCommentBlockSize;
            s_opusCurrentFilePos += s_opusCommentBlockSize;
            s_opusCommentBlockSize = 0;
        }
        if(s_opusRemainBlockPicLen <= 0) {
            if(s_opusBlockPicItem.size() > 0) { // get blockpic data
                // OPUS_LOG_INFO("---------------------------------------------------------------------------");
                // OPUS_LOG_INFO("metadata blockpic found at pos %i, size %i bytes", s_opusBlockPicPos, s_opusBlockPicItem);
                // for(int32_t i = 0; i < s_opusBlockPicItem.size(); i += 2) { OPUS_LOG_INFO("segment %02i, pos %07i, len %05i", i / 2, s_opusBlockPicItem[i], s_opusBlockPicItem[i + 1]); }
                // OPUS_LOG_INFO("---------------------------------------------------------------------------");
                s_f_opusNewMetadataBlockPicture = true;
            }
        }
        return OPUS_PARSE_OGG_DONE;
    }

    if(s_f_lastPage && s_opusSegmentTableSize == 0) {
        *bytesLeft = segmLen; ret = OPUS_END;
        return ret;
    }
    if(s_frameCount > 0) return opusDecodePage3(inbuf, bytesLeft, segmLen, outbuf); // decode audio, next part
    if(!s_opusSegmentTableSize) {
        s_f_opusParseOgg = false;
        s_opusCountCode = 0;
        ret = OPUSparseOGG(inbuf, bytesLeft);
        if(ret != OPUS_NONE) return ret; // error
        inbuf += s_opusOggHeaderSize;
    }

    if(s_opusSegmentTableSize > 0) {
        s_opusSegmentTableRdPtr++;
        s_opusSegmentTableSize--;
        segmLen = s_opusSegmentTable[s_opusSegmentTableRdPtr];
    }

    if(s_opusPageNr == 0) { // OpusHead
        ret = opusDecodePage0(inbuf, bytesLeft, segmLen);
    }
    else if(s_opusPageNr == 1) { // OpusComment
        ret = parseOpusComment(inbuf, segmLen);
        if(ret == 0){OPUS_LOG_ERROR("Opus comment page not found"); return OPUS_ERR;}
        s_opusRemainBlockPicLen = s_opusBlockPicLen;
        *bytesLeft -= (segmLen - s_blockPicLenUntilFrameEnd);
        s_opusCommentBlockSize = s_blockPicLenUntilFrameEnd;
        s_opusPageNr++;
        ret = OPUS_PARSE_OGG_DONE;
    }
    else if(s_opusPageNr == 2) { // OpusComment Subsequent Pages
        s_opusCommentBlockSize = segmLen;
        if(s_opusRemainBlockPicLen <= segmLen) s_opusPageNr++;
        ;
        ret = OPUS_PARSE_OGG_DONE;
    }
    else if(s_opusPageNr == 3) {
        ret = opusDecodePage3(inbuf, bytesLeft, segmLen, outbuf); // decode audio
    }
    else { ; }

    if(s_opusSegmentTableSize == 0) {
        s_opusSegmentTableRdPtr = -1; // back to the parking position
    }
    return ret;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t opusDecodePage0(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength){
    int32_t ret = 0;
    ret = parseOpusHead(inbuf, segmentLength);
    *bytesLeft           -= segmentLength;
    s_opusCurrentFilePos += segmentLength;
    if(ret == 1){ s_opusPageNr++;}
    if(ret == 0){OPUS_LOG_ERROR("Opus head not found"); return OPUS_ERR;}
    if(ret < 0) return ret;
    return OPUS_PARSE_OGG_DONE;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t opusDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t *outbuf){

    if(s_opusAudioDataStart == 0){
        s_opusAudioDataStart = s_opusCurrentFilePos;
    }
    s_endband = 21;

    int32_t ret = 0;

    if(s_frameCount > 0) goto FramePacking; // more than one frame in the packet

    s_odp3.configNr = parseOpusTOC(inbuf[0]);
    if(s_odp3.configNr < 0) {OPUS_LOG_ERROR("wrong config number: %i", s_odp3.configNr); return OPUS_ERR;} // SILK or Hybrid mode

    switch(s_odp3.configNr){
        case  0 ... 3:  s_endband  = 0; // OPUS_BANDWIDTH_SILK_NARROWBAND
                        s_mode = MODE_SILK_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_NARROWBAND;
                        s_internalSampleRate = 8000;
                        break;
        case  4 ... 7:  s_endband  = 0; // OPUS_BANDWIDTH_SILK_MEDIUMBAND
                        s_mode = MODE_SILK_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_MEDIUMBAND;
                        s_internalSampleRate = 12000;
                        break;
        case  8 ... 11: s_endband  = 0; // OPUS_BANDWIDTH_SILK_WIDEBAND
                        s_mode = MODE_SILK_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_WIDEBAND;
                        s_internalSampleRate = 16000;
                        break;
        case 12 ... 13: s_endband  = 0; // OPUS_BANDWIDTH_HYBRID_SUPERWIDEBAND
                        s_mode = MODE_HYBRID;
                        s_bandWidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
                        break;
        case 14 ... 15: s_endband  = 0; // OPUS_BANDWIDTH_HYBRID_FULLBAND
                        s_mode = MODE_HYBRID;
                        s_bandWidth = OPUS_BANDWIDTH_FULLBAND;
                        break;
        case 16 ... 19: s_endband = 13; // OPUS_BANDWIDTH_CELT_NARROWBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_NARROWBAND;
                        break;
        case 20 ... 23: s_endband = 17; // OPUS_BANDWIDTH_CELT_WIDEBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_WIDEBAND;
                        break;
        case 24 ... 27: s_endband = 19; // OPUS_BANDWIDTH_CELT_SUPERWIDEBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
                        break;
        case 28 ... 31: s_endband = 21; // OPUS_BANDWIDTH_CELT_FULLBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_FULLBAND;
                        break;
        default:        OPUS_LOG_WARN("unknown bandwifth %i, s_odp3.configNr");
                        s_endband = 21; // assume OPUS_BANDWIDTH_FULLBAND
                        break;
    }

//    celt_decoder_ctl(CELT_SET_START_BAND_REQUEST, s_endband);
    if (s_mode == MODE_CELT_ONLY){
        celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, s_endband);
    }
    else if(s_mode == MODE_SILK_ONLY){
        // silk_InitDecoder();
    }

    s_odp3.samplesPerFrame = opus_packet_get_samples_per_frame(inbuf, /*s_opusSamplerate*/ 48000);

FramePacking:            // https://www.tech-invite.com/y65/tinv-ietf-rfc-6716-2.html   3.2. Frame Packing
// OPUS_LOG_INFO("s_opusCountCode %i, configNr %i", s_opusCountCode, configNr);

    switch(s_opusCountCode){
        case 0:  // Code 0: One Frame in the Packet
            ret = opus_FramePacking_Code0(inbuf, bytesLeft, outbuf, segmentLength, s_odp3.samplesPerFrame);
            break;
        case 1:  // Code 1: Two Frames in the Packet, Each with Equal Compressed Size
            ret = opus_FramePacking_Code1(inbuf, bytesLeft, outbuf, segmentLength, s_odp3.samplesPerFrame, &s_frameCount);
            break;
        case 2:  // Code 2: Two Frames in the Packet, with Different Compressed Sizes
            ret = opus_FramePacking_Code2(inbuf, bytesLeft, outbuf, segmentLength, s_odp3.samplesPerFrame, &s_frameCount);
            break;
        case 3: // Code 3: A Signaled Number of Frames in the Packet
            ret = opus_FramePacking_Code3(inbuf, bytesLeft, outbuf, segmentLength, s_odp3.samplesPerFrame, &s_frameCount);
            break;
        default:
            OPUS_LOG_ERROR("Opus unknown count code %i", s_opusCountCode);
            return OPUS_ERR;
            break;
    }
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t opus_decode_frame(uint8_t *inbuf, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame) {
    if(!packetLen) {OPUS_LOG_WARN("Opus packetLen is 0"); return 0;}
    int i, silk_ret = 0, celt_ret = 0;
    uint16_t audiosize = 960;
    uint8_t payloadSize_ms = max(10, 1000 * samplesPerFrame / 48000);  /* The SILK PLC cannot produce frames of less than 10 ms */
    int decoded_samples = 0;
    int32_t silk_frame_size;
    uint8_t start_band = 17;
    uint8_t end_band = 21;

    s_silk_DecControlStruct->nChannelsAPI = s_opusChannels;
    s_silk_DecControlStruct->nChannelsInternal = s_opusChannels;
    s_silk_DecControlStruct->API_sampleRate = 48000;

    if (     s_bandWidth == OPUS_BANDWIDTH_NARROWBAND) {s_internalSampleRate = 8000;}
    else if (s_bandWidth == OPUS_BANDWIDTH_MEDIUMBAND) {s_internalSampleRate = 12000;}
    else if (s_bandWidth == OPUS_BANDWIDTH_WIDEBAND)   {s_internalSampleRate = 16000;}
    else                                               {s_internalSampleRate = 16000;}

    if(s_prev_mode == MODE_NONE) celt_decoder_ctl((int32_t)OPUS_RESET_STATE);

    if (s_mode == MODE_CELT_ONLY){
        if(s_prev_mode != s_mode){
            celt_decoder_ctl((int32_t)OPUS_RESET_STATE);
            ec_dec_init((uint8_t *)inbuf, packetLen);
            celt_decoder_ctl((int32_t)CELT_SET_START_BAND_REQUEST, 0);
        }
        s_prev_mode = s_mode;
        ec_dec_init((uint8_t *)inbuf, packetLen);
        celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, s_endband);
        return celt_decode_with_ec((int16_t*)outbuf, samplesPerFrame);
    }

    if (s_mode == MODE_SILK_ONLY) {
        if(s_prev_mode == MODE_CELT_ONLY) silk_InitDecoder();
        decoded_samples = 0;
        ec_dec_init((uint8_t *)inbuf, samplesPerFrame);
        silk_setRawParams(s_opusChannels, 2, payloadSize_ms, s_internalSampleRate, 48000);
        do {  /* Call SILK decoder */
            int first_frame = decoded_samples == 0;
            int silk_ret = silk_Decode(0, first_frame, (int16_t*)outbuf + decoded_samples, &silk_frame_size);
            if(silk_ret < 0) return silk_ret;
            decoded_samples += silk_frame_size;
        } while(decoded_samples < samplesPerFrame);

        return decoded_samples;
    }

    if (s_mode == MODE_HYBRID) {
        ec_dec_init((uint8_t*)inbuf, packetLen);
        int      pcm_silk_size = samplesPerFrame * 4;
        ps_ptr<int16_t>pcm_silk; pcm_silk.alloc_array(pcm_silk_size);
        int16_t* pcm_ptr;
        pcm_ptr = pcm_silk.get();
        if (s_prev_mode == MODE_CELT_ONLY || s_prev_mode == MODE_NONE) silk_InitDecoder();
        decoded_samples = 0;
        silk_setRawParams(s_opusChannels, 2, payloadSize_ms, s_internalSampleRate, 48000);
        do { /* Call SILK decoder */
            int     first_frame = decoded_samples == 0;
            int32_t nSamplesOut;
            silk_ret = silk_Decode(0, first_frame, pcm_ptr, &nSamplesOut);
            if (silk_ret < 0) return silk_ret;
            pcm_ptr += nSamplesOut * s_opusChannels;
            decoded_samples += nSamplesOut;
        } while (decoded_samples < audiosize);

        if (ec_tell() + 17 + 20 <= 8 * packetLen) {
            /* Check if we have a redundant 0-8 kHz band */
            ec_dec_bit_logp(12);
        }
        if (s_bandWidth) {
            switch (s_bandWidth) {
                case OPUS_BANDWIDTH_NARROWBAND:    end_band = 13; break;
                case OPUS_BANDWIDTH_MEDIUMBAND:
                case OPUS_BANDWIDTH_WIDEBAND:      end_band = 17; break;
                case OPUS_BANDWIDTH_SUPERWIDEBAND: end_band = 19; break;
                case OPUS_BANDWIDTH_FULLBAND:      end_band = 21; break;
                default: break;
            }
            celt_decoder_ctl((int32_t)CELT_SET_END_BAND_REQUEST, (end_band));
            celt_decoder_ctl((int32_t)CELT_SET_CHANNELS_REQUEST, (s_opusChannels));
        }

        /* MUST be after PLC */
        celt_decoder_ctl((int32_t)CELT_SET_START_BAND_REQUEST, start_band);

        /* Make sure to discard any previous CELT state */
        if (s_mode != s_prev_mode && s_prev_mode > 0) celt_decoder_ctl((int32_t)OPUS_RESET_STATE);
        celt_ret = celt_decode_with_ec(outbuf, audiosize);

        for (i = 0; i < audiosize * s_opusChannels; i++) outbuf[i] = SAT16(ADD32(outbuf[i], pcm_silk[i]));

        s_prev_mode = MODE_HYBRID;
        return celt_ret < 0 ? celt_ret : audiosize;
    }
    s_prev_mode = MODE_NONE;
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code0(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame){

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
    s_opusCurrentFilePos += packetLen;
    packetLen--;
    inbuf++;
    ret = opus_decode_frame(inbuf, outbuf, packetLen, samplesPerFrame);

    if(ret < 0){
        return ret; // decode err
    }
    s_opusValidSamples = ret;
    return OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code1(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount){

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
    if(*frameCount == 0){
        packetLen--;
        inbuf++;
        *bytesLeft -= 1;
        s_opusCurrentFilePos += 1;
        s_ofp3.c1fs = packetLen / 2;
        // OPUS_LOG_WARN("OPUS countCode 1 len %i, c1fs %i", len, c1fs);
        *frameCount = 2;
    }
    if(*frameCount > 0){
        ret = opus_decode_frame(inbuf, outbuf, s_ofp3.c1fs, samplesPerFrame);
        // OPUS_LOG_WARN("code 1, ret %i", ret);
        if(ret < 0){
            *frameCount = 0;
            return ret;  // decode err
        }
        s_opusValidSamples = ret;
        *bytesLeft -= s_ofp3.c1fs;
        s_opusCurrentFilePos += s_ofp3.c1fs;
    }
    *frameCount -= 1;
    return OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code2(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount){

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

    if(*frameCount == 0){
        uint8_t b1 = inbuf[1];
        uint8_t b2 = inbuf[2];
        if(b1 < 252){
            s_ofp2.firstFrameLength = b1;
            packetLen -= 2;
            *bytesLeft -= 2;
            s_opusCurrentFilePos += 2;
            inbuf += 2;
        }
        else{
            s_ofp2.firstFrameLength = b1 + (b2 * 4);
            packetLen -= 3;
            *bytesLeft -= 3;
            s_opusCurrentFilePos += 3;
            inbuf += 3;
        }
        s_ofp2.secondFrameLength = packetLen - s_ofp2.firstFrameLength;
        *frameCount = 2;
    }
    if(*frameCount == 2){
        ret = opus_decode_frame(inbuf, outbuf, s_ofp2.firstFrameLength, samplesPerFrame);
        // OPUS_LOG_WARN("code 2, ret %i", ret);
        if(ret < 0){
            *frameCount = 0;
            return ret;  // decode err
        }
        s_opusValidSamples = ret;
        *bytesLeft -= s_ofp2.firstFrameLength;
        s_opusCurrentFilePos += s_ofp2.firstFrameLength;
    }
    if(*frameCount == 1){
        ret = opus_decode_frame(inbuf, outbuf, s_ofp2.secondFrameLength, samplesPerFrame);
        // OPUS_LOG_WARN("code 2, ret %i", ret);
        if(ret < 0){
            *frameCount = 0;
            return ret;  // decode err
        }
        s_opusValidSamples = ret;
        *bytesLeft -= s_ofp2.secondFrameLength;
        s_opusCurrentFilePos += s_ofp2.secondFrameLength;
    }
    *frameCount -= 1;
    return OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code3(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount){

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
    s_ofp3.idx = 0;

    if (s_ofp3.firstCall) {
    //    OPUS_LOG_WARN("0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X ",
    //          inbuf[0], inbuf[1], inbuf[2], inbuf[3], inbuf[4], inbuf[5], inbuf[6], inbuf[7], inbuf[8], inbuf[9]);

        // Reset all relevant state for a new packet
        s_ofp3.firstCall = false;
        s_ofp3.paddingLength = 0;
        s_ofp3.v = false;
        s_ofp3.p = false;
        s_ofp3.M = 0;
        s_ofp3.fs = 0;
        // Ensure vfs is cleared or handled appropriately if it's a static member
        // memset(s_ofp3.vfs, 0, sizeof(s_ofp3.vfs)); // Only if s_ofp3.vfs is part of s_ofp3

        s_opusCurrentFilePos += packetLen;
        s_ofp3.idx = 1; // Start reading after TOC byte (inbuf[0])
        s_ofp3.spf = samplesPerFrame;

        // Parse Frame Count Byte (inbuf[1])
        if (s_ofp3.idx >= packetLen) { // Check bounds before accessing
            *bytesLeft -= packetLen; // Consume this potentially malformed packet
            *frameCount = 0;
            s_ofp3.firstCall = true;
            return OPUS_NONE; // Packet too short
        }
        if (inbuf[s_ofp3.idx] & 0b10000000) s_ofp3.v = true; // VBR indicator
        if (inbuf[s_ofp3.idx] & 0b01000000) s_ofp3.p = true; // padding bit
        s_ofp3.M = inbuf[s_ofp3.idx] & 0b00111111;           // framecount
        *frameCount = s_ofp3.M; // Set the output frameCount for this packet
        s_ofp3.idx++; // Move past the frame count byte

        // M MUST NOT be zero (from spec)
        if (s_ofp3.M == 0) {
            // OPUS_LOG_INFO("Error: Opus Code 3 packet with M = 0 (no frames)");
            *bytesLeft -= packetLen;
            *frameCount = 0;
            s_ofp3.firstCall = true;
            OPUS_LOG_ERROR("Opus code 3; packet with no frames");
            return OPUS_ERR;
        }

        // Parse Padding Length
        if (s_ofp3.p) {
            uint32_t current_padding_chunk_val;
            do {
                if (s_ofp3.idx >= packetLen) { // Check bounds
                    // OPUS_LOG_INFO("Error: Packet truncated during padding length parsing");
                    *bytesLeft -= packetLen;
                    *frameCount = 0;
                    s_ofp3.firstCall = true;
                    OPUS_LOG_ERROR("Opus packet is truncated during padding length parsing");
                    return OPUS_ERR;
                }
                current_padding_chunk_val = inbuf[s_ofp3.idx];
                s_ofp3.idx++;
                s_ofp3.paddingLength += current_padding_chunk_val;
            } while (current_padding_chunk_val == 255); // Continue if the last byte read was 255
            // OPUS_LOG_WARN("we have %i padding bytes", s_ofp3.paddingLength);
        }

        // Parse Variable Frame Sizes (N1 to N[M-1] for VBR)
        if (s_ofp3.v && s_ofp3.M > 1) { // Only M-1 lengths are signaled if M > 1
            for(int m = 0; m < (s_ofp3.M - 1); m++) {
                if (s_ofp3.idx >= packetLen) { // Check bounds
                    *bytesLeft -= packetLen;
                    *frameCount = 0;
                    s_ofp3.firstCall = true;
                    OPUS_LOG_ERROR("Opus packet has been truncated at VBR parsing");
                    return OPUS_ERR;
                }
                uint16_t current_frame_len_val = inbuf[s_ofp3.idx];
                s_ofp3.idx++;
                if(current_frame_len_val == 255){
                    if (s_ofp3.idx >= packetLen) { // Check bounds for second byte
                        // OPUS_LOG_INFO("Error: Packet truncated during VBR frame length parsing (second byte)");
                        *bytesLeft -= packetLen;
                        *frameCount = 0;
                        s_ofp3.firstCall = true;
                        OPUS_LOG_ERROR("Opus packet has been truncated at VBR parsing");
                        return OPUS_ERR;
                    }
                    current_frame_len_val += inbuf[s_ofp3.idx]; // Add the next byte's value
                    s_ofp3.idx++;
                }
                s_ofp3.vfs[m] = current_frame_len_val;
                // OPUS_LOG_INFO("VFS[%i]: %i", m, s_ofp3.vfs[m]);
            }
        }

        // Calculate bytes available for compressed audio frames
        int32_t total_header_bytes = s_ofp3.idx; // idx now points to start of first compressed frame data
        int32_t remaining_bytes_for_data_and_padding = packetLen - total_header_bytes;

        // Verify enough data for padding
        if (remaining_bytes_for_data_and_padding < s_ofp3.paddingLength) {
            // OPUS_LOG_INFO("Error: Padding length %i exceeds remaining packet bytes %i", s_ofp3.paddingLength, remaining_bytes_for_data_and_padding);
            *bytesLeft -= packetLen;
            *frameCount = 0;
            s_ofp3.firstCall = true;
            OPUS_LOG_ERROR("Too many parsing bytes: %i, padding length; %i", remaining_bytes_for_data_and_padding, s_ofp3.paddingLength);
            return OPUS_ERR;
        }

        // Bytes containing actual compressed data (excluding padding at the end)
        int32_t compressed_data_bytes = remaining_bytes_for_data_and_padding - s_ofp3.paddingLength;

        // OPUS_LOG_INFO("packetLen %i, total_header_bytes %i, compressed_data_bytes %i, paddingLength %i, framecount %u",
        //      packetLen, total_header_bytes, compressed_data_bytes, s_ofp3.paddingLength, *frameCount);

        if(!s_ofp3.v){  // Constant Bitrates (CBR)
            // R = N - 2 - P
            // R = packetLen (N) - (TOC + FrameCountByte) - (Padding Header Bytes + Padding Data Bytes)
            // R = packetLen - s_ofp3.idx (which is total header bytes) - s_ofp3.paddingLength (which is actual padding data)
            // But simplified: R = compressed_data_bytes (calculated above)

            if (s_ofp3.M == 0) { // Already checked, but good for robustness
                // OPUS_LOG_INFO("Error: CBR with 0 frames (should not happen based on spec M>0)");
                *bytesLeft -= packetLen;
                *frameCount = 0;
                s_ofp3.firstCall = true;
                OPUS_LOG_ERROR("Opus CBR wihtout frames");
                return OPUS_ERR;
            }

            s_ofp3.fs = compressed_data_bytes / s_ofp3.M;
            int r = compressed_data_bytes % s_ofp3.M;
            if(r > 0) {
                OPUS_LOG_WARN("CBR data not perfectly divisible by frame count. remainingBytes %i, frames %i, remainder %i",
                      compressed_data_bytes, s_ofp3.M, r);
                // This might indicate a malformed packet, or a small rounding difference for very short packets.
                // For strict compliance, R MUST be a non-negative integer multiple of M.
                *bytesLeft -= packetLen;
                *frameCount = 0;
                s_ofp3.firstCall = true;
                return OPUS_NONE;
            }

            // In CBR, all frames have size s_ofp3.fs. We don't use vfs here.
        } else { // Variable Bitrates (VBR)
            // Calculate the length of the last frame (M)
            uint32_t sum_of_signaled_lengths = 0;
            for (int m = 0; m < (s_ofp3.M - 1); m++) {
                sum_of_signaled_lengths += s_ofp3.vfs[m];
            }

            if (sum_of_signaled_lengths > compressed_data_bytes) {
                OPUS_LOG_ERROR("Opus wrong VBR length, sum_of_signaled_lengths: %i, compressed_data_bytes: %i", sum_of_signaled_lengths, compressed_data_bytes);
                return OPUS_ERR;
                *bytesLeft -= packetLen;
                *frameCount = 0;
                s_ofp3.firstCall = true;
                return OPUS_NONE;
            }
            s_ofp3.vfs[s_ofp3.M - 1] = compressed_data_bytes - sum_of_signaled_lengths;
            // OPUS_LOG_INFO("Calculated VFS[%i] (last frame): %i", s_ofp3.M - 1, s_ofp3.vfs[s_ofp3.M - 1]);
        }
        current_payload_offset = total_header_bytes; // This is where the first frame data starts
        (void)current_payload_offset;
        (*bytesLeft) -= total_header_bytes; // Account for all header bytes consumed
    }

    // Decoding loop (outside the firstCall block)
    if (*frameCount > 0) {
        int32_t frame_len;
        if (s_ofp3.v) {
            // Get the length of the current frame to decode
            uint8_t current_frame_idx = s_ofp3.M - (*frameCount); // 0 for first, M-1 for last
            if (current_frame_idx >= s_ofp3.M) { // Safety check
                // OPUS_LOG_INFO("Error: Invalid VFS index access. current_frame_idx %i, M %i", current_frame_idx, s_ofp3.M);
                *bytesLeft -= (*bytesLeft > 0 ? *bytesLeft : 0); // Consume remaining bytes to reset
                *frameCount = 0;
                s_ofp3.firstCall = true;
                OPUS_LOG_ERROR("opus invalid VFS index access, current_frame_idx; %i, nr of frames: %i", current_frame_idx, s_ofp3.M);
                return OPUS_ERR;
            }
            frame_len = s_ofp3.vfs[current_frame_idx];
        } else {
            frame_len = s_ofp3.fs;
        }
        // Check if enough bytes are left for the current frame
        if (*bytesLeft < frame_len) {
            OPUS_LOG_ERROR("Opus not enough bytes: %i, required: %i", *bytesLeft, frame_len);
            return OPUS_ERR;
            *bytesLeft -= (*bytesLeft > 0 ? *bytesLeft : 0); // Consume remaining bytes to reset
            *frameCount = 0;
            s_ofp3.firstCall = true;
            return OPUS_NONE;
        }

        // Decode the frame
        // The inbuf + current_payload_offset points to the start of the current frame data

        ret = opus_decode_frame(inbuf + s_ofp3.idx, outbuf, frame_len, s_ofp3.spf);
        // OPUS_LOG_INFO("code 3, fs %i, spf %i, ret %i, offs %i", frame_len, s_ofp3.spf, ret, s_ofp3.idx);
        // Update bytesLeft and frameCount
        *bytesLeft -= frame_len;
        *frameCount -= 1;
        s_opusValidSamples = ret;

        if (*frameCount > 0) {
            return OPUS_CONTINUE; // More frames in this packet
        }
    }

    // After all frames are decoded, account for padding bytes if any remain.
    // The *bytesLeft at this point should ideally be equal to s_ofp3.paddingLength
    // because total_header_bytes and frame_len (for all frames) have been subtracted.
    // If there's a mismatch, it indicates an issue or just consume the rest.
    *bytesLeft -= s_ofp3.paddingLength; // Consume padding bytes from *bytesLeft for the packet
    if (*bytesLeft < 0) {
        OPUS_LOG_WARN("Warning: Negative bytesLeft after consuming padding. Remaining: %i", *bytesLeft);
        *bytesLeft = 0; // Prevent negative
    }

    *frameCount = 0; // All frames processed for this packet
    s_opusValidSamples = samplesPerFrame; // Reset for next packet's first frame
    s_ofp3.firstCall = true; // Signal for next packet
    return OPUS_NONE; // Packet finished
}
//----------------------------------------------------------------------------------------------------------------------

int32_t opus_packet_get_samples_per_frame(const uint8_t *data, int32_t Fs) {
    int32_t audiosize;
    if ((data[0] & 0x80) == 0x080) {
        audiosize = ((data[0] >> 3) & 0x03);
        audiosize = (Fs << audiosize) / 400;
    } else if ((data[0] & 0x60) == 0x60) {
        audiosize = (data[0] & 0x08) ? Fs / 50 : Fs / 100;
    } else {
        audiosize = ((data[0] >> 3) & 0x3);
        if (audiosize == 3){
            audiosize = Fs * 60 / 1000;
        }
        else{
            audiosize = (Fs << audiosize) / 100;
        }
    }
    return audiosize;
}
//----------------------------------------------------------------------------------------------------------------------

uint8_t OPUSGetChannels(){
    return s_opusChannels;
}
uint32_t OPUSGetSampRate(){
    return 48000;
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
uint32_t OPUSGetAudioDataStart(){
    return s_opusAudioDataStart;
}
const char* OPUSgetStreamTitle(){
    if(s_f_newSteamTitle){
        s_f_newSteamTitle = false;
        return s_streamTitle.c_get();
    }
    return NULL;
}
uint16_t OPUSgetMode(){
    return s_mode;
}
std::vector<uint32_t> OPUSgetMetadataBlockPicture(){
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
int8_t parseOpusTOC(uint8_t TOC_Byte){  // https://www.rfc-editor.org/rfc/rfc6716  page 16 ff

    uint8_t configNr = 0;
    uint8_t s = 0;              // stereo flag
    uint8_t c = 0; (void)c;     // count code

    configNr = (TOC_Byte & 0b11111000) >> 3;
    s        = (TOC_Byte & 0b00000100) >> 2;
    c        = (TOC_Byte & 0b00000011);

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
    s_opusCountCode = c;
    s_f_opusStereoFlag = s;

    // if(configNr < 12) return ERR_OPUS_SILK_MODE_UNSUPPORTED;
    // if(configNr < 16) return ERR_OPUS_HYBRID_MODE_UNSUPPORTED;
    // if(configNr < 20) return ERR_OPUS_NARROW_BAND_UNSUPPORTED;
    // if(configNr < 24) return ERR_OPUS_WIDE_BAND_UNSUPPORTED;
    // if(configNr < 28) return ERR_OPUS_SUPER_WIDE_BAND_UNSUPPORTED;

    return configNr;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t parseOpusComment(uint8_t *inbuf, int32_t nBytes){      // reference https://exiftool.org/TagNames/Vorbis.html#Comments
                                                               // reference https://www.rfc-editor.org/rfc/rfc7845#section-5
    int32_t idx = OPUS_specialIndexOf(inbuf, "OpusTags", 10);
    if(idx != 0) return 0; // is not OpusTags

    ps_ptr<char>artist = {};
    ps_ptr<char>title = {};

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
    for(int32_t i = 0; i < commentListLength; i++){
        uint32_t commentStringLen   = *(inbuf + 3 + pos) << 24;
                 commentStringLen  += *(inbuf + 2 + pos) << 16;
                 commentStringLen  += *(inbuf + 1 + pos) << 8;
                 commentStringLen  += *(inbuf + 0 + pos);
        pos += 4;
        nBytes -= 4;
        idx = OPUS_specialIndexOf(inbuf + pos, "artist=", 10);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "ARTIST=", 10);
        if(idx == 0){ artist.append((char*)inbuf + pos + 7, commentStringLen - 7);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "title=", 10);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "TITLE=", 10);
        if(idx == 0){ title.append((char*)inbuf + pos + 6, commentStringLen - 6);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "metadata_block_picture=", 25);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "METADATA_BLOCK_PICTURE=", 25);
        if(idx == 0){
            s_opusBlockPicLen = commentStringLen - 23;
            s_opusCurrentFilePos += pos + 23;
            s_opusBlockPicPos += s_opusCurrentFilePos;
            s_blockPicLenUntilFrameEnd = nBytes - 23;
        //  OPUS_LOG_INFO("metadata block picture found at pos %i, length %i", s_opusBlockPicPos, s_opusBlockPicLen);
            uint32_t pLen = _min(s_blockPicLenUntilFrameEnd, s_opusBlockPicLen);
            if(pLen){
                s_opusBlockPicItem.push_back(s_opusBlockPicPos);
                s_opusBlockPicItem.push_back(pLen);
            }
        }
        pos += commentStringLen;
        nBytes -= commentStringLen;
    }
    if(artist.valid() && title.valid()){
        s_streamTitle.clone_from(artist);
        s_streamTitle.append(" - ");
        s_streamTitle.append(title.c_get());
        s_f_newSteamTitle = true;
    }
    else if(artist.valid()){
        s_streamTitle.clone_from(artist);
        s_f_newSteamTitle = true;
    }
    else if(title.valid()){
        s_streamTitle.clone_from(title);
        s_f_newSteamTitle = true;
    }
    return 1;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t parseOpusHead(uint8_t *inbuf, int32_t nBytes){  // reference https://wiki.xiph.org/OggOpus


    int32_t idx = OPUS_specialIndexOf(inbuf, "OpusHead", 10);
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

    if(channelCount == 0 || channelCount >2) {OPUS_LOG_ERROR("Opus channels out of range, ch: %i", channelCount); return OPUS_ERR;}
    s_opusChannels = channelCount;
//    OPUS_LOG_INFO("sampleRate %i", sampleRate);
//    if(sampleRate != 48000 && sampleRate != 44100) return ERR_OPUS_INVALID_SAMPLERATE;
    s_opusSamplerate = sampleRate;
    if(channelMap > 1) {OPUS_LOG_ERROR("Opus extra channels not supported"); return OPUS_ERR;}

    (void)outputGain;

    CELTDecoder_ClearBuffer();
    s_opusError = celt_decoder_init(s_opusChannels); if(s_opusError < 0)                 {OPUS_LOG_ERROR("The CELT Decoder could not be initialized"); return OPUS_ERR;}
    s_opusError = celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST,  0); if(s_opusError < 0) {OPUS_LOG_ERROR("The CELT Decoder could not be initialized"); return OPUS_ERR;}
    s_opusError = celt_decoder_ctl(CELT_SET_END_BAND_REQUEST,   21); if(s_opusError < 0) {OPUS_LOG_ERROR("The CELT Decoder could not be initialized"); return OPUS_ERR;}

    return 1;
}

//----------------------------------------------------------------------------------------------------------------------
int32_t OPUSparseOGG(uint8_t *inbuf, int32_t *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    int32_t idx = OPUS_specialIndexOf(inbuf, "OggS", 6);
    if(idx != 0) {OPUS_LOG_ERROR("Opus dec async, OGG capture pattern \"OggS\" not found"); return OPUS_ERR;}

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

    for(int32_t i = 0; i < pageSegments; i++){
        int32_t n = *(inbuf + 27 + i);
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

//  OPUS_LOG_INFO("firstPage %i, continuedPage %i, lastPage %i",s_f_firstPage, s_f_continuedPage, s_f_lastPage);

    uint16_t headerSize   = pageSegments + 27;
    *bytesLeft           -= headerSize;
    s_opusCurrentFilePos += headerSize;
    s_opusOggHeaderSize   = headerSize;

    int32_t pLen = _min((int32_t)s_opusSegmentLength, s_opusRemainBlockPicLen);
//  OPUS_LOG_INFO("s_opusSegmentLength %i, s_opusRemainBlockPicLen %i", s_opusSegmentLength, s_opusRemainBlockPicLen);
    if(s_opusBlockPicLen && pLen > 0){
        s_opusBlockPicItem.push_back(s_opusCurrentFilePos);
        s_opusBlockPicItem.push_back(pLen);
    }
    return OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------
int32_t OPUSFindSyncWord(unsigned char *buf, int32_t nBytes){
    // assume we have a ogg wrapper
    int32_t idx = OPUS_specialIndexOf(buf, "OggS", nBytes);
    if(idx >= 0){ // Magic Word found
    //    OPUS_LOG_INFO("OggS found at %i", idx);
        s_f_opusParseOgg = true;
        return idx;
    }
    s_f_opusParseOgg = false;
    OPUS_LOG_ERROR("Opus syncword not found");
    return OPUS_ERR;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t OPUS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact){
    int32_t result = -1;  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int32_t i = 0; i < baselen - strlen(str); i++){
        result = i;
        for (int32_t j = 0; j < strlen(str) + exact; j++){
            if (*(base + i + j) != *(str + j)){
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
//----------------------------------------------------------------------------------------------------------------------
