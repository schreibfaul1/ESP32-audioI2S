/*
 * Audio.cpp
 *
 *  Created on: 26.10.2018
 *      Author: Wolle
 *
 *  This library plays mp3 files from SD cart via I2S
 *  no DAC, no DeltSigma
 *
 *  etrernal HW on I2S nessesary, e.g.MAX98357A
 */

#include "Audio.h"
/******************************************************************************************************/
Audio::Audio() {
    helixMP3Decoder = MP3InitDecoder();
    if (!helixMP3Decoder) {
        sprintf(chbuf, "Reading file: %s", path);
        if(audio_info) audio_info("MP3Decoder Out of memory error!");
    }
    //i2s configuration
    m_i2s_num = I2S_NUM_0; // i2s port number
    i2s_config_t i2s_config = {
         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
         .sample_rate = 16000,
         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
         .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
         .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
         .dma_buf_count = 8,
         .dma_buf_len = 64,   //Interrupt level 1
         .use_apll=APLL_DISABLE,
         .fixed_mclk=-1
    };
    i2s_driver_install((i2s_port_t)m_i2s_num, &i2s_config, 0, NULL);

    m_BCLK=26;                       // Bit Clock
    m_LRC=25;                        // Left/Right Clock
    m_DOUT=27;                       // Data Out
    setPinout(m_BCLK, m_LRC, m_DOUT);
    construct_OutBuf(1600);
}
/******************************************************************************************************/
Audio::~Audio() {
    MP3FreeDecoder(helixMP3Decoder);
    destruct_OutBuf();
}
/******************************************************************************************************/
bool Audio::connecttoSD(String sdfile){
    const uint8_t ascii[60]={
          //196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215,   ISO
            142, 143, 146, 128, 000, 144, 000, 000, 000, 000, 000, 000, 000, 165, 000, 000, 000, 000, 153, 000, //ASCII
          //216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,   ISO
            000, 000, 000, 000, 154, 000, 000, 225, 133, 000, 000, 000, 132, 143, 145, 135, 138, 130, 136, 137, //ASCII
          //236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255    ISO
            000, 161, 140, 139, 000, 164, 000, 162, 147, 000, 148, 000, 000, 000, 163, 150, 129, 000, 000, 152};//ASCII

    uint16_t i=0, s=0;

    if(!sdfile.startsWith("/")) sdfile="/"+sdfile;
    while(sdfile[i] != 0){                      //convert UTF8 to ASCII
        path[i]=sdfile[i];
        if(path[i] > 195){
            s=ascii[path[i]-196];
            if(s!=0) path[i]=s;                 // found a related ASCII sign
        } i++;
    }
    path[i]=0;
    m_mp3title=sdfile.substring(sdfile.lastIndexOf('/') + 1, sdfile.length());
    sprintf(chbuf, "Reading file: %s", m_mp3title.c_str());
    if(audio_info) audio_info(chbuf);
    fs::FS &fs=SD;
    mp3file=fs.open(path);
    if(!mp3file){
        if(audio_info) audio_info("Failed to open file for reading");
        return false;
    }
    mp3file.readBytes(chbuf, 10);
    if ((chbuf[0] != 'I') || (chbuf[1] != 'D') || (chbuf[2] != '3')) {
        if(audio_info) audio_info("file is not mp3");
        return false;
    }
    m_rev = chbuf[3];
    switch (m_rev) {
    case 2:
        m_f_unsync = (chbuf[5] & 0x80);
        m_f_exthdr = false;
        break;
    case 3:
    case 4:
        m_f_unsync = (chbuf[5] & 0x80); // bit7
        m_f_exthdr = (chbuf[5] & 0x40); // bit6 extended header
        break;
    };

    m_id3Size  = chbuf[6]; m_id3Size = m_id3Size << 7;
    m_id3Size |= chbuf[7]; m_id3Size = m_id3Size << 7;
    m_id3Size |= chbuf[8]; m_id3Size = m_id3Size << 7;
    m_id3Size |= chbuf[9];

    // Every read from now may be unsync'd
    sprintf(chbuf, "ID3 version=%i", m_rev);
    if(audio_info) audio_info(chbuf);
    sprintf(chbuf,"ID3 framesSize=%i", m_id3Size);
    if(audio_info) audio_info(chbuf);
    readID3Metadata();
    m_running=true;
    return true;
}
/******************************************************************************************************/
void Audio::readID3Metadata(){
    if (m_f_exthdr) {
        if(audio_info) audio_info("ID3 extended header");
        int ehsz = (mp3file.read() << 24) | (mp3file.read() << 16)
                | (mp3file.read() << 8) | (mp3file.read());
        m_id3Size -= 4;
        for (int j = 0; j < ehsz - 4; j++) {
            mp3file.read();
            m_id3Size--;
        } // Throw it away
    } else
        if(audio_info) audio_info("ID3 normal frames");

    do {
        unsigned char frameid[4];
        int framesize;
        bool compressed;
        frameid[0] = mp3file.read();
        frameid[1] = mp3file.read();
        frameid[2] = mp3file.read();
        m_id3Size -= 3;
        if (m_rev == 2)
            frameid[3] = 0;
        else {
            frameid[3] = mp3file.read();
            m_id3Size--;
        }
        if (frameid[0] == 0 && frameid[1] == 0 && frameid[2] == 0
                && frameid[3] == 0) {
            // We're in padding
            while (m_id3Size != 0) {
                mp3file.read();
                m_id3Size--;
            }
        } else {
            if (m_rev == 2) {
                framesize = (mp3file.read() << 16) | (mp3file.read() << 8)
                        | (mp3file.read());
                m_id3Size -= 3;
                compressed = false;
            } else {
                framesize = (mp3file.read() << 24) | (mp3file.read() << 16)
                        | (mp3file.read() << 8) | (mp3file.read());
                m_id3Size -= 4;
                mp3file.read(); // skip 1st flag
                m_id3Size--;
                compressed = mp3file.read() & 0x80;
                m_id3Size--;
            }
            if (compressed) {
                int decompsize = (mp3file.read() << 24) | (mp3file.read() << 16)
                        | (mp3file.read() << 8) | (mp3file.read());
                m_id3Size -= 4;
                (void) decompsize;

                for (int j = 0; j < framesize; j++)
                    mp3file.read();
                m_id3Size--;
            }

            // Read the value and send to callback
            char value[64];
            uint16_t i;

            bool isUnicode;
            isUnicode = (mp3file.read() == 1) ? true : false;
            std::ignore = isUnicode; // not used, suppress warning
            m_id3Size--;
            for (i = 0; i < framesize - 1; i++) {
                if (i < sizeof(value) - 1) {
                    value[i] = mp3file.read();
                    m_id3Size--;
                } else {
                    mp3file.read();
                    m_id3Size--;
                }
            }
            value[i < sizeof(value) - 1 ? i : sizeof(value) - 1] = 0; // Terminate the string...
            if ((frameid[0] == 'T' && frameid[1] == 'A' && frameid[2] == 'L' && frameid[3] == 'B')
                    || (frameid[0] == 'T' && frameid[1] == 'A' && frameid[2] == 'L' && m_rev == 2)) {
                sprintf(chbuf, "Album: %s", value);
                if(audio_id3data) audio_id3data(chbuf);
            } else if ((frameid[0] == 'T' && frameid[1] == 'I' && frameid[2] == 'T' && frameid[3] == '2')
                    || (frameid[0] == 'T' && frameid[1] == 'T' && frameid[2] == '2' && m_rev == 2)) {
                sprintf(chbuf, "Title: %s", value);
                if(audio_id3data) audio_id3data(chbuf);
            } else if ((frameid[0] == 'T' && frameid[1] == 'P' && frameid[2] == 'E' && frameid[3] == '1')
                    || (frameid[0] == 'T' && frameid[1] == 'P' && frameid[2] == '1' && m_rev == 2)) {
                sprintf(chbuf, "Performer: %s", value);
                if(audio_id3data) audio_id3data(chbuf);
            } else if ((frameid[0] == 'T' && frameid[1] == 'Y' && frameid[2] == 'E' && frameid[3] == 'R')
                    || (frameid[0] == 'T' && frameid[1] == 'Y' && frameid[2] == 'E' && m_rev == 2)) {
                sprintf(chbuf, "Year: %s", value);
                if(audio_id3data) audio_id3data(chbuf);
            }
        }
        // log_i("m_id3Size=%i\n", m_id3Size);
    } while (m_id3Size > 0);
}
/******************************************************************************************************/
void Audio::stop(){
    if(m_running){
        m_running = false;
        mp3file.close();
        i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
    }
}
/******************************************************************************************************/
bool Audio::loop() {
    if (!m_running) goto done;  // Nothing to do here!
    // If we've got data, try and pump it out...
    while (m_validSamples) {
        m_lastSample[0] = m_outSample[m_curSample * 2];
        m_lastSample[1] = m_outSample[m_curSample * 2 + 1];
        if (!playSample(m_lastSample))
            goto done;
        // Can't send, but no error detected
        m_validSamples--;
        m_curSample++;
    }
    if (fill_InputBuf()) { // No samples available, need to decode a new frame
        // buff[0] start of frame, decode it...
        unsigned char *inbuffer = reinterpret_cast<unsigned char *>(inBuff);
        int bytesLeft = m_buffValid;
        int ret = MP3Decode(helixMP3Decoder, &inbuffer, &bytesLeft, m_outSample, 0);
        if (ret) { // Error, skip the frame...
            sprintf(chbuf, "MP3 decode error %d", ret);
            if(audio_info) audio_info(chbuf);
        } else {
            m_lastFrameEnd = m_buffValid - bytesLeft;
            MP3FrameInfo fi;
            MP3GetLastFrameInfo(helixMP3Decoder, &fi);
            if ((int) fi.samprate != (int) m_lastRate) {
                //i2s_set_sample_rates((i2s_port_t)portNo, fi.samprate);
                m_lastRate = fi.samprate;
                setSampleRate(fi.samprate);
                sprintf(chbuf,"SampleRate=%i",fi.samprate);
                if(audio_info) audio_info(chbuf);
            }
            if (fi.nChans != m_lastChannels) {
                setChannels(fi.nChans);
                m_lastChannels = fi.nChans;
                sprintf(chbuf,"Channels=%i", fi.nChans);
                if(audio_info) audio_info(chbuf);
            }
            if (fi.bitsPerSample != (int)m_bps){
                m_bps=fi.bitsPerSample;
                sprintf(chbuf,"BitsPerSample=%i", fi.bitsPerSample);
                if(audio_info) audio_info(chbuf);
            }
            m_curSample = 0;
            m_validSamples = fi.outputSamps / m_lastChannels;
        }
    } else {
        m_running = false; // No more data, we're done here...
        //log_i("STOP");
    }
    done:
    return m_running;
}
/******************************************************************************************************/
bool Audio::setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT){
    m_BCLK=BCLK;            // Bit Clock
    m_LRC=LRC;              // Left/Right Clock
    m_DOUT=DOUT;            // Data Out

    i2s_pin_config_t pins = {
      .bck_io_num = m_BCLK,
      .ws_io_num =  m_LRC,              //  wclk,
      .data_out_num = m_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin((i2s_port_t)m_i2s_num, &pins);
    return true;
}
/******************************************************************************************************/
uint32_t Audio::getFileSize(){
    if (!mp3file) return 0;
    return mp3file.size();
}
/******************************************************************************************************/
uint32_t Audio::getFilePos(){
    if (!mp3file) return 0;
    return mp3file.position();
}
/******************************************************************************************************/
void Audio::construct_OutBuf(int buffSizeSamples) {
    m_buffSize = buffSizeSamples;
    m_leftSample = (int16_t*) malloc(sizeof(int16_t) * m_buffSize);
    m_rightSample = (int16_t*) malloc(sizeof(int16_t) * m_buffSize);
    m_writePtr = 0;
    m_readPtr = 0;
    m_filled = false;
}
/******************************************************************************************************/
void Audio::destruct_OutBuf() {
    free (m_leftSample);
    free (m_rightSample);
}
/******************************************************************************************************/
bool Audio::setSampleRate(int freq) {
    i2s_set_sample_rates((i2s_port_t)m_i2s_num, freq);
    return true;
}
/******************************************************************************************************/
bool Audio::setBitsPerSample(int bits) {
    //return sink->SetBitsPerSample(bits);
    if ( (bits != 16) && (bits != 8) ) return false;
    m_bps = bits;
    return true;
}
/******************************************************************************************************/
bool Audio::setChannels(int ch) {
    if ( (ch < 1) || (ch > 2) ) return false;
    m_channels = ch;
    return true;
}
/******************************************************************************************************/
bool Audio::playSample(int16_t sample[2]) {
    // First, try and fill I2S...
    if (m_filled) {
        while (m_readPtr != m_writePtr) {
            int16_t s[2] = { m_leftSample[m_readPtr], m_rightSample[m_readPtr] };
            // Mono to "stereo" conversion
            if (m_channels == 1)
              s[RIGHTCHANNEL] = s[LEFTCHANNEL];
            if (m_bps == 8) {
              // Upsample from unsigned 8 bits to signed 16 bits
              s[LEFTCHANNEL] = (((int16_t)(s[LEFTCHANNEL]&0xff)) - 128) << 8;
              s[RIGHTCHANNEL] = (((int16_t)(s[RIGHTCHANNEL]&0xff)) - 128) << 8;
            }
            uint32_t s32;
            s32 = ((Gain(s[RIGHTCHANNEL]))<<16) | (Gain(s[LEFTCHANNEL]) & 0xffff); // volume
            esp_err_t err=i2s_write((i2s_port_t)m_i2s_num, (const char*)&s32, sizeof(uint32_t), &m_bytesWritten, 100);
            if(err!=ESP_OK){
                log_e("ESP32 Errorcode %i", err);
                break; // Can't stuff any more in I2S...
            }
            m_readPtr = (m_readPtr + 1) % m_buffSize;
        }
    } // Now, do we have space for a new sample?
    int nextWritePtr = (m_writePtr + 1) % m_buffSize;
    if (nextWritePtr == m_readPtr) {
        m_filled = true;
        return false;
    }
    m_leftSample[m_writePtr] = sample[LEFTCHANNEL];
    m_rightSample[m_writePtr] = sample[RIGHTCHANNEL];
    m_writePtr = nextWritePtr;
    return true;
}
/******************************************************************************************************/
void Audio::setVolume(uint8_t vol){ // vol 0...21
    if(vol>21) vol=21;
    m_vol=volumetable[vol];
}
/******************************************************************************************************/
uint8_t Audio::getVolume(){
    return m_vol;
}
/******************************************************************************************************/
int16_t Audio::Gain(int16_t s) {
    int32_t v;
    if(s>0){
       v= (s * m_vol)>>6;
    }
    else{
       v= (s * m_vol)>>6;
    }
    return (int16_t)(v&0xffff);
}
/******************************************************************************************************/
bool Audio::fill_InputBuf() {
    inBuff[0] = 0; // Destroy any existing sync word @ 0
    int nextSync;
    do {
        nextSync = MP3FindSyncWord(inBuff + m_lastFrameEnd, m_buffValid - m_lastFrameEnd);
        if (nextSync >= 0)
            nextSync += m_lastFrameEnd;
        m_lastFrameEnd = 0;
        if (nextSync == -1) {
            if (inBuff[m_buffValid - 1] == 0xff) { // Could be 1st half of syncword, preserve it...
                inBuff[0] = 0xff;
                m_buffValid = mp3file.read(inBuff + 1, sizeof(inBuff) - 1);
                if (m_buffValid == 0){
                    sprintf(chbuf,"EOF %s", m_mp3title.c_str());
                    if(audio_eof_mp3) audio_eof_mp3(chbuf);
                    return false; // No data available, EOF
                }
            }
            else { // Try a whole new buffer
                m_buffValid = mp3file.read(inBuff, sizeof(inBuff));
                if (m_buffValid == 0){
                     sprintf(chbuf,"EOF %s", m_mp3title.c_str());
                    if(audio_eof_mp3) audio_eof_mp3(chbuf);
                    return false; // No data available, EOF
                }
            }
        }
    }
    while (nextSync == -1);
    // Move the frame to start at offset 0 in the buffer
    m_buffValid -= nextSync; // Throw out prior to nextSync
    memmove(inBuff, inBuff + nextSync, m_buffValid);
    // We have a sync word at 0 now, try and fill remainder of buffer
    m_buffValid += mp3file.read(inBuff + m_buffValid, sizeof(inBuff) - m_buffValid);
    if(m_buffValid==0){
        sprintf(chbuf,"EOF %s", m_mp3title.c_str());
        if(audio_eof_mp3) audio_eof_mp3(chbuf);
        return false;
    }
    return true;
}
/******************************************************************************************************/
