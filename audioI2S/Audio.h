/*
 * Audio.h
 *
 *  Created on: 26.10.2018
 *      Author: Wolle (schreibfaul1)
 */

#ifndef AUDIO_H_
#define AUDIO_H_

#include "Arduino.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "mp3_decoder/mp3_decoder.h"
#include "driver/i2s.h"

extern __attribute__((weak)) void audio_info(const char*);
extern __attribute__((weak)) void audio_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void audio_eof_mp3(const char*); //end of mp3 file


class Audio {

public:
    Audio();
    ~Audio();
    bool connecttoSD(String sdfile);
    bool loop();
    uint32_t getFileSize();
    uint32_t getFilePos();
    bool setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT);
    void stop();
    void setVolume(uint8_t vol);
    uint8_t getVolume();
private:
    void readID3Metadata();
    void construct_OutBuf(int buffSizeSamples);
    void destruct_OutBuf();
    bool setSampleRate(int hz);
    bool setBitsPerSample(int bits);
    bool setChannels(int channels);
    bool playSample(int16_t sample[2]) ;
    int16_t Gain(int16_t s);
    bool fill_InputBuf();

private:
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;

    const uint8_t volumetable[22]={   0,  1,  2,  3,  4 , 6 , 8, 10, 12, 14, 17,
                                     20, 23, 27, 30 ,34, 38, 43 ,48, 52, 58, 64}; //22 elements

    File            mp3file;
    HMP3Decoder     helixMP3Decoder;                // Helix MP3 decoder

    char            chbuf[256];
    char            path[256];
    uint8_t         inBuff[1600];                   // size inputBuffer
    int16_t         m_buffValid;
    int16_t         m_lastFrameEnd;
    int16_t         m_outSample[1152 * 2];          // Interleaved L/R
    int16_t         m_validSamples;
    int16_t         m_curSample;
    String          m_mp3title="";                  // the name of the file
    boolean         m_f_unsync = false;
    boolean         m_f_exthdr = false;             // ID3 extended header
    int             m_id3Size=0;                    // length id3 tag
    uint8_t         m_rev=0;                        // revision
    uint8_t         m_BCLK=0;                       // Bit Clock
    uint8_t         m_LRC=0;                        // Left/Right Clock
    uint8_t         m_DOUT=0;                       // Data Out
    uint8_t         m_vol=64;                       // volume
    unsigned int    m_lastRate;
    int             m_lastChannels;
    bool            m_running;
    int16_t         m_lastSample[2];
    int             m_buffSize;                     // size outputBuffer
    int16_t*        m_leftSample;
    int16_t*        m_rightSample;
    int             m_writePtr;                     // ptr outputBuffer
    int             m_readPtr;                      // ptr outputBuffer
    bool            m_filled;                       // outputBuffer
    uint8_t         m_bps;                          // bitsPerSample
    uint8_t         m_channels;
    uint8_t         m_i2s_num= I2S_NUM_0;           // I2S_NUM_0 or I2S_NUM_1
    size_t          m_bytesWritten=0;               // set in i2s_write() but not used
};

#endif /* AUDIO_H_ */
