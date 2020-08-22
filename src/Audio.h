/*
 * Audio.h
 *
 *  Created on: Oct 26,2018
 *  Updated on: Aug 22,2020
 *      Author: Wolle (schreibfaul1)
 */

#ifndef AUDIO_H_
#define AUDIO_H_

#include "Arduino.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "WiFiClientSecure.h"
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
extern __attribute__((weak)) void audio_eof_stream(const char*); // The webstream comes to an end

#define AUDIO_HEADER          2    //const for datamode
#define AUDIO_DATA            4
#define AUDIO_METADATA        8
#define AUDIO_PLAYLISTINIT   16
#define AUDIO_PLAYLISTHEADER 32
#define AUDIO_PLAYLISTDATA   64
#define AUDIO_SWM           128

//----------------------------------------------------------------------------------------------------------------------

class AudioBuffer{
// AudioBuffer will be allocated in PSRAM, If PSRAM not available or has not enough space AudioBuffer will be
// allocated in FlashRAM with reduced size
//
//                      m_readPtr                 m_writePtr                 m_endPtr
//                           |<------dataLength------->|<------ writeSpace ----->|
//                           ▼                         ▼                         ▼
// ---------------------------------------------------------------------------------------------------------------
// |                       <--m_buffSize-->                                      |      <--m_resBuffSize -->     |
// ---------------------------------------------------------------------------------------------------------------
// |<------freeSpace-------->|                         |<------freeSpace-------->|
//
//
//
// if the space between m_readPtr and buffend < 1600 bytes copy data from the beginning to resBuff
// so that the mp3 frame is always completed
//
//                               m_writePtr                 m_readPtr        m_endPtr
//                                    |<-------writeSpace-1 --->|<--dataLength-->|
//                                    ▼                         ▼                ▼
// ---------------------------------------------------------------------------------------------------------------
// |                       <--m_buffSize-->                                      |      <--m_resBuffSize -->     |
// ---------------------------------------------------------------------------------------------------------------
// |<---  ------dataLength--  ------>|<-------freeSpace------->|
//
//

public:
    AudioBuffer();                      // constructor
    ~AudioBuffer();                     // frees the buffer
    size_t   init();                    // set default values
    size_t   freeSpace();               // number of free bytes to overwrite
    size_t   writeSpace();              // space fom writepointer to bufferend
    size_t   bufferFilled();            // returns the number of filled bytes
    void     bytesWritten(size_t bw);   // update writepointer
    void     bytesWasRead(size_t br);   // update readpointer
    uint8_t* writePtr();                // returns the current writepointer
    uint8_t* readPtr();                 // returns the current readpointer
    uint32_t getWritePos();             // write position relative to the beginning
    uint32_t getReadPos();              // read position relative to the beginning
    void     resetBuffer();             // restore defaults

protected:
    const size_t m_buffSizePSRAM = 300000; // most webstreams limit the advance to 100...300Kbytes
    const size_t m_buffSizeRAM   = 1600 * 5;
    size_t       m_buffSize      = 0;
    size_t       m_freeSpace     = 0;
    size_t       m_writeSpace    = 0;
    size_t       m_dataLength    = 0;
    size_t       m_resBuffSize   = 1600; // reserved buffspace, >= one mp3 frame
    uint8_t*     m_buffer        = NULL;
    uint8_t*     m_writePtr      = NULL;
    uint8_t*     m_readPtr       = NULL;
    uint8_t*     m_endPtr        = NULL;
    bool         m_f_start       = true;
};
//----------------------------------------------------------------------------------------------------------------------

class Audio : private AudioBuffer{

    AudioBuffer InBuff; // instance of input buffer

public:
    Audio();
    ~Audio();
    bool connecttoFS(fs::FS &fs, String file);
    bool connecttoSD(String sdfile);
    bool connecttohost(String host);
    bool connecttospeech(String speech, String lang);
    void loop();
    uint32_t getFileSize();
    uint32_t getFilePos();
    uint32_t getSampleRate();
    uint8_t  getBitsPerSample();
    uint8_t  getChannels();

    /**
     * @brief Get the audio file duration in seconds
     * 
     * @return uint32_t file duration in seconds, 0 if no file active
     */
    uint32_t getAudioFileDuration();
    /**
     * @brief Get the current plying time in seconds
     * 
     * @return uint32_t current second of audio file, 0 if no file active
     */
    uint32_t getAudioCurrentTime();
    bool setFilePos(uint32_t pos);
    /**
     * @brief audioFileSeek seeks the file in both directions
     * 
     * @param[in] speed
     *      speed > 0 : fast-forward
     *      speed < 0 : fast-rewind
     * @return true if audio file active and speed is valid, otherwise false
     */
    bool audioFileSeek(const int8_t speed);
    bool setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t DIN=I2S_PIN_NO_CHANGE);
    void stopSong();
    /**
     * @brief pauseResume pauses current playback 
     * 
     * @return true if audio file or stream is active, false otherwise
     */
    bool pauseResume();

    void setVolume(uint8_t vol);
    uint8_t getVolume();
    inline uint8_t getDatamode(){return m_datamode;}
    inline void setDatamode(uint8_t dm){m_datamode=dm;}
    inline uint32_t streamavail() {if(m_f_ssl==false) return client.available(); else return clientsecure.available();}
    bool isRunning() {return m_f_running;}
    uint32_t inBufferFilled(); // returns the number of stored bytes in the inputbuffer
    uint32_t inBufferFree();   // returns the number of free bytes in the inputbuffer

private:
    void reset(); // free buffers and set defaults
    void processLocalFile();
    void processWebStream();
    int  sendBytes(uint8_t *data, size_t len);
    void compute_audioCurrentTime(int bd);
    void printDecodeError(int r);
    void readID3Metadata();
    bool setSampleRate(uint32_t hz);
    bool setBitsPerSample(int bits);
    bool setChannels(int channels);
    bool playChunk();
    bool playSample(int16_t sample[2]) ;
    bool playI2Sremains();
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

private:
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    enum : int { CODEC_NONE = 0, CODEC_WAV = 1, CODEC_MP3 = 2, CODEC_AAC = 4, CODEC_FLAC = 5};
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;

    const uint8_t volumetable[22]={   0,  1,  2,  3,  4 , 6 , 8, 10, 12, 14, 17,
                                     20, 23, 27, 30 ,34, 38, 43 ,48, 52, 58, 64}; //22 elements



    File              audiofile;    // @suppress("Abstract class cannot be instantiated")
    WiFiClient        client;       // @suppress("Abstract class cannot be instantiated")
    WiFiClientSecure  clientsecure; // @suppress("Abstract class cannot be instantiated")
    i2s_config_t      m_i2s_config; // stores values for I2S driver
    char            chbuf[256];
    char            path[256];
    int             m_id3Size=0;                    // length id3 tag
    int             m_LFcount;                      // Detection of end of header
    uint32_t        m_sampleRate=16000;
    int             m_bytesLeft=0;
    uint32_t        m_bitRate=0;                    // current bitrate given fom decoder
    uint32_t        m_avr_bitrate;                  // average bitrate, median computed by VBR
    int             m_readbytes=0;                  // bytes read
    int             m_metalen=0;                    // Number of bytes in metadata
    int8_t          m_playlist_num = 0 ;            // Nonzero for selection from playlist
    uint8_t         m_rev=0;                        // revision
    uint8_t         m_BCLK=0;                       // Bit Clock
    uint8_t         m_LRC=0;                        // Left/Right Clock
    uint8_t         m_DOUT=0;                       // Data Out
    int8_t          m_DIN=0;                        // Data In, can be negative if unused (I2S_PIN_NO_CHANGE is -1)
    uint8_t         m_vol=64;                       // volume
    uint8_t         m_bitsPerSample=16;             // bitsPerSample
    uint8_t         m_channels=2;
    uint8_t         m_i2s_num= I2S_NUM_0;           // I2S_NUM_0 or I2S_NUM_1
    int16_t         m_buffValid;
    int16_t         m_lastFrameEnd;
    int16_t         m_outBuff[2048*2];              //[1152 * 2];          // Interleaved L/R
    int16_t         m_validSamples = 0;
    int16_t         m_curSample;
    int16_t         m_Sample[2];
    int16_t*        m_leftSample;
    int16_t*        m_rightSample;
    uint16_t        m_datamode=0;                   // Statemaschine
    uint32_t        m_metaint = 0;                  // Number of databytes between metadata
    uint32_t        m_totalcount = 0;               // Counter mp3 data
    uint32_t        m_chunkcount = 0 ;              // Counter for chunked transfer
    uint32_t        m_t0;                           // store millis(), is needed for a small delay
    uint32_t        m_metaCount=0;                  // Bytecounter between metadata
    uint32_t        m_contentlength = 0;            // Stores the length if the stream comes from fileserver
    uint32_t        m_bytectr = 0;                  // count received data
    uint32_t        m_bytesNotDecoded=0;            // pictures or something else that comes with the stream
    String          m_audioName="";                 // the name of the file
    String          m_playlist ;                    // The URL of the specified playlist
    String          m_lastHost="";                  // Store the last URL to a webstream
    String          m_metaline ;                    // Readable line in metadata
    String          m_icyname ;                     // Icecast station name
    String          m_st_remember="";               // Save the last streamtitle
    String          m_icyurl="";                    // Store ie icy-url if received
    String          m_plsURL;
    String          m_plsStationName;
    String          m_icystreamtitle ;              // Streamtitle from metadata
    bool            m_f_unsync = false;
    bool            m_f_exthdr = false;             // ID3 extended header
    bool            m_f_localfile = false ;         // Play from local mp3-file
    bool            m_f_webstream = false ;         // Play from URL
    bool            m_f_ssl=false;
    bool            m_f_running=false;
    bool            m_f_firststream_ready=false;    // Set after connecttohost and first streamdata are available
    bool            m_f_ctseen=false;               // First line of header seen or not
    bool            m_f_chunked = false ;           // Station provides chunked transfer
    bool            m_f_filled;                     // outputBuffer
    bool            m_f_swm=false;
    bool            m_f_firstmetabyte=false;        // True if first metabyte (counter)
    bool            m_f_plsFile=false;              // Set if URL is known
    bool            m_f_plsTitle=false;             // Set if StationName is known
    bool            m_ctseen=false;                 // First line of header seen or not
    bool            m_f_stream=false;               // Set false if stream is lost
    uint8_t         m_codec = CODEC_NONE;           //
    bool            m_f_playing = false;            // valid mp3 stream recognized
    bool            m_f_webfile= false;             // assume it's a radiostream, not a podcast
    bool            m_f_psram = false;              // set if PSRAM is availabe
    size_t          m_i2s_bytesWritten=0;           // set in i2s_write() but not used
    uint32_t        m_audioFileDuration=0;
    float           m_audioCurrentTime=0;
};

#endif /* AUDIO_H_ */
