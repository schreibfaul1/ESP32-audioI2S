/*
 *  mp3_decoder.cpp
 *  libmad decoder v0.15.0b
 *  ESP32 impl. needs PSRAM
 *
 *  Created on: 04.07.2025
 *  Updated on: 21.07.2025
 */


#include "mp3_decoder.h"

const int32_t* leftChannel;  // internal variable for the PCM outBuff
const int32_t* rightChannel;

bool s_isInit = false;
bool s_first_call = false;
bool s_stream_items = false;
bool s_xing_items = false;

// must be initialized once
bool MP3Decoder_AllocateBuffers(){
    allocateBuffers();
    s_first_call = true;
    s_isInit = true;
    return true;
}

void MP3Decoder_ClearBuffer(){
    clearBuffers();
}

uint32_t    MP3GetOutputSamps(){return mad_get_output_samps();}
void        MP3Decoder_FreeBuffers(){freeBuffers(); s_isInit = false;};
int32_t     MP3GetSampRate(){return mad_get_sample_rate();}
int32_t     MP3GetChannels(){return mad_get_channels();}
uint8_t     MP3GetBitsPerSample(){return 16;}
bool        MP3Decoder_IsInit(){return s_isInit;}
const char* MP3GetMPEGVersion(){return mad_get_mpeg_version();}
const char* MP3GetLayer(){return mad_get_layer();}
uint32_t    MP3GetBitrate(){if(mad_xing_bitrate()) return mad_xing_bitrate(); else return mad_get_bitrate();}
uint32_t    MP3GetAudioFileDuration(){return mad_xing_duration_seconds();}


//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3Decode(uint8_t *data, int32_t *bytesLeft, int16_t *outSamples){
    if(s_first_call){
        int32_t res = MP3FindSyncWord(data, *bytesLeft);
        if(res != 0) {MP3_ERROR("res: %i", res); return res;}
        s_stream_items = true;

        bool xing = mad_parse_xing_header(data, *bytesLeft);
        if(xing){
            s_xing_items = true;
        }
        s_first_call = false;
    }
    int32_t res = mad_decode(data, bytesLeft, outSamples);
    if(stream->error == MAD_ERROR_CONTINUE) {/*MP3_ERROR("BufLen %i", res);*/ return res;} // need more data
    vTaskDelay(1);
    if(res != 0) {MP3_ERROR("res: %i", res); return res;}
    if(s_stream_items){
        s_stream_items = false;
        // log_w("samplerate: %i", MP3GetSampRate());
        // log_w("channels: %i", MP3GetChannels());
        // log_w("bitRate: %i", MP3GetBitrate());
        // log_w("mpeg_version: %s", MP3GetMPEGVersion());
        // log_w("layer: %s", MP3GetLayer());
        // log_w("bits_per_sample: %i", MP3GetBitsPerSample());
    }
    if(s_xing_items){
        s_xing_items = false;
        // log_w("xing_bitrate: %i b/s", mad_xing_bitrate());
        // log_w("xing_duration: %i s", mad_xing_duration_seconds());
        // log_w("xing_audiosize: %i bytes", mad_xing_tota_bytes());
        // log_w("xing_total_mp3_frames: %i ", mad_xing_total_frames());
    }
    return res;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t MP3FindSyncWord(uint8_t *buf, int32_t nBytes){
    return mad_find_syncword(buf, nBytes);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
