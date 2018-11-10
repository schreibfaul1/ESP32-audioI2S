/*
 * Audio.h
 *
 *  Created on: 26.10.2018
 *  Updated on: 04.11.2018
 *      Author: Wolle (schreibfaul1)
 */

#ifndef AUDIO_H_
#define AUDIO_H_

#include "Arduino.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "WiFiClientSecure.h"
#include "mp3_decoder/mp3_decoder.h"
#include "driver/i2s.h"

extern __attribute__((weak)) void audio_info(const char*);
extern __attribute__((weak)) void audio_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void audio_eof_mp3(const char*); //end of mp3 file
extern __attribute__((weak)) void audio_showstreamtitle(const char*);
extern __attribute__((weak)) void audio_showstation(const char*);
extern __attribute__((weak)) void audio_showstreaminfo(const char*);
extern __attribute__((weak)) void audio_bitrate(const char*);
extern __attribute__((weak)) void audio_commercial(const char*);
extern __attribute__((weak)) void audio_icyurl(const char*);
extern __attribute__((weak)) void audio_lasthost(const char*);
extern __attribute__((weak)) void audio_eof_speech(const char*);

#define AUDIO_HEADER          2    //const for datamode
#define AUDIO_DATA            4
#define AUDIO_METADATA        8
#define AUDIO_PLAYLISTINIT   16
#define AUDIO_PLAYLISTHEADER 32
#define AUDIO_PLAYLISTDATA   64
#define AUDIO_SWM           128

class Audio {

public:
    Audio();
    ~Audio();
    bool connecttoSD(String sdfile);
    bool connecttohost(String host);
    bool connecttospeech(String speech, String lang);
    void loop();
    uint32_t getFileSize();
    uint32_t getFilePos();
    bool setFilePos(uint32_t pos);
    bool setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT);
    void stopSong();
    void setVolume(uint8_t vol);
    uint8_t getVolume();
    uint16_t ringused();
    uint16_t ringfree();
private:
    int  sendBytes(uint8_t *data, size_t len);
    void readID3Metadata();
    void construct_OutBuf(int buffSizeSamples);
    void destruct_OutBuf();
    bool setSampleRate(int hz);
    bool setBitsPerSample(int bits);
    bool setChannels(int channels);
    bool playChunk();
    bool playSample(int16_t sample[2]) ;
    int16_t Gain(int16_t s);
    bool fill_InputBuf();
    void showstreamtitle(const char *ml, bool full);
    bool chkhdrline(const char* str);
    void handlebyte(uint8_t b);
    esp_err_t I2Sstart(uint8_t i2s_num);
    esp_err_t I2Sstop(uint8_t i2s_num);
    char* lltoa(long long val, int base);
    long long int XL (long long int a, const char* b);
    String urlencode(String str);

    inline uint8_t getDatamode(){return m_datamode;}
    inline void setDatamode(uint8_t dm){m_datamode=dm;}
    inline uint32_t streamavail() {if(m_f_ssl==false) return client.available(); else return clientsecure.available();}

private:
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;

    const uint8_t volumetable[22]={   0,  1,  2,  3,  4 , 6 , 8, 10, 12, 14, 17,
                                     20, 23, 27, 30 ,34, 38, 43 ,48, 52, 58, 64}; //22 elements

    File            mp3file;
    HMP3Decoder     helixMP3Decoder;                // Helix MP3 decoder
    WiFiClient      client;
    WiFiClientSecure clientsecure;

    uint8_t  m_ringbuf[0x5000]; // 20480d           // ringbuffer for mp3 stream
    const uint16_t m_ringbfsize=sizeof(m_ringbuf);  // ringbuffer size
    uint16_t m_rbwindex=1600;                          // ringbuffer writeindex
    uint16_t m_rbrindex=1600;                          // ringbuffer readindex
    uint16_t m_ringfree=0;                          // ringbuffer free space
    uint16_t m_ringused=0;                          // ringbuffer used space

    char            chbuf[256];
    char            path[256];
    int             m_id3Size=0;                    // length id3 tag
    int             m_LFcount;                      // Detection of end of header
    int             m_lastChannels;
    int             m_buffSize;                     // size outputBuffer
    int             m_nextSync=0;
    int             m_bytesLeft=0;
    int             m_writePtr;                     // ptr outputBuffer
    int             m_readPtr;                      // ptr outputBuffer
    int             m_bitrate=0;
    int             m_readbytes=0;                  // bytes read
    int             m_metacount=0;                  // Number of bytes in metadata
    int8_t          m_playlist_num = 0 ;            // Nonzero for selection from playlist
    uint8_t         inBuff[1600];                   // size inputBuffer
    uint8_t         m_rev=0;                        // revision
    uint8_t         m_BCLK=0;                       // Bit Clock
    uint8_t         m_LRC=0;                        // Left/Right Clock
    uint8_t         m_DOUT=0;                       // Data Out
    uint8_t         m_vol=64;                       // volume
    uint8_t         m_bps;                          // bitsPerSample
    uint8_t         m_channels;
    uint8_t         m_i2s_num= I2S_NUM_0;           // I2S_NUM_0 or I2S_NUM_1
    int16_t         m_buffValid;
    int16_t         m_lastFrameEnd;
    int16_t         m_outSample[1152 * 2];          // Interleaved L/R
    int16_t         m_validSamples;
    int16_t         m_curSample;
    int16_t         m_lastSample[2];
    int16_t*        m_leftSample;
    int16_t*        m_rightSample;
    uint16_t        m_datamode=0;                   // Statemaschine
    uint32_t        m_metaint = 0;                  // Number of databytes between metadata
    uint32_t        m_totalcount = 0;               // Counter mp3 data
    uint32_t        m_chunkcount = 0 ;              // Counter for chunked transfer
    uint32_t        m_t0;
    uint32_t        m_av=0;                         // available in stream or SD (uin16_t is to small by playing from SD)
    uint32_t        m_count=0;                      // Bytecounter between metadata
    String          m_mp3title="";                  // the name of the file
    String          m_playlist ;                    // The URL of the specified playlist
    String          m_lastHost="";                  // Store the last URL to a webstream
    String          m_metaline ;                    // Readable line in metadata
    String          m_icyname ;                     // Icecast station name
    String          m_st_remember="";               // Save the last streamtitle
    String          m_icyurl="";                    // Store ie icy-url if received
    String          m_plsURL;
    String          m_plsStationName;
    String          m_icystreamtitle ;              // Streamtitle from metadata
    boolean         m_ctseen=false;                 // First line of header seen or not
    boolean         m_f_unsync = false;
    boolean         m_f_exthdr = false;             // ID3 extended header
    boolean         m_f_localfile = false ;         // Play from local mp3-file
    boolean         m_f_webstream = false ;         // Play from URL
    boolean         m_f_ssl=false;
    boolean         m_f_running=false;
    boolean         m_f_stream_ready=false;         // Set after connecttohost and first streamdata are available
    boolean         m_f_ctseen=false;               // First line of header seen or not
    boolean         m_f_chunked = false ;           // Station provides chunked transfer
    boolean         m_f_filled;                     // outputBuffer
    boolean         m_f_swm=false;
    boolean         m_f_firstmetabyte=false;        // True if first metabyte (counter)
    boolean         m_f_plsFile=false;              // Set if URL is known
    boolean         m_f_plsTitle=false;             // Set if StationName is known
    boolean         m_f_stream=false;               // Set false if stream is lost
    unsigned int    m_lastRate;
    size_t          m_bytesWritten=0;               // set in i2s_write() but not used
};

#endif /* AUDIO_H_ */
