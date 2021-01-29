/*
 * Audio.h
 *
 *  Created on: Oct 26,2018
 *  Updated on: Jan 29,2021
 *      Author: Wolle (schreibfaul1)   ¯\_(ツ)_/¯
 */

#ifndef AUDIO_H_
#define AUDIO_H_
#define FF_LFN_UNICODE      2
#include "Arduino.h"
#include "base64.h"
#include "SPI.h"
#include "SD.h"
#include "SD_MMC.h"
#include "SPIFFS.h"
#include "FS.h"
#include "WiFiClientSecure.h"
#include "driver/i2s.h"

extern __attribute__((weak)) void audio_info(const char*);
extern __attribute__((weak)) void audio_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void audio_id3image(File& file, const size_t pos, const size_t size); //ID3 metadata image
extern __attribute__((weak)) void audio_eof_mp3(const char*); //end of mp3 file
extern __attribute__((weak)) void audio_showstreamtitle(const char*);
extern __attribute__((weak)) void audio_showstation(const char*);
extern __attribute__((weak)) void audio_bitrate(const char*);
extern __attribute__((weak)) void audio_commercial(const char*);
extern __attribute__((weak)) void audio_icyurl(const char*);
extern __attribute__((weak)) void audio_lasthost(const char*);
extern __attribute__((weak)) void audio_eof_speech(const char*);
extern __attribute__((weak)) void audio_eof_stream(const char*); // The webstream comes to an end

//----------------------------------------------------------------------------------------------------------------------

class AudioBuffer {
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
    Audio(const uint8_t BCLK=27, const uint8_t LRC=26, const uint8_t DOUT=25); // #99
    ~Audio();
    bool connecttoFS(fs::FS &fs, const char* file);
    bool connecttoSD(const char* sdfile);
    bool setFileLoop(bool input);//TEST loop
    bool connecttohost(const char* host, const char* user = "", const char* pwd = "");
    bool connecttospeech(const char* speech, const char* lang);
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
    void forceMono(bool m);
    void setBalance(int8_t bal = 0);
    void setVolume(uint8_t vol);
    uint8_t getVolume();
    inline uint8_t getDatamode(){return m_datamode;}
    inline void setDatamode(uint8_t dm){m_datamode=dm;}
    inline uint32_t streamavail() {if(m_f_ssl==false) return client.available(); else return clientsecure.available();}
    bool isRunning() {return m_f_running;}
    esp_err_t i2s_mclk_pin_select(const uint8_t pin);
    uint32_t inBufferFilled(); // returns the number of stored bytes in the inputbuffer
    uint32_t inBufferFree();   // returns the number of free bytes in the inputbuffer
    void setTone(uint8_t l_type = 0, uint16_t l_freq = 0, uint8_t r_type = 0, uint16_t r_freq = 0);

private:
    void reset(); // free buffers and set defaults
    void initInBuff();
    void processLocalFile();
    void processWebStream();
    int  sendBytes(uint8_t* data, size_t len);
    void compute_audioCurrentTime(int bd);
    void printDecodeError(int r);
    int  readWaveHeader(uint8_t* data, size_t len);
    int  readID3Metadata(uint8_t* data, size_t len);
    bool setSampleRate(uint32_t hz);
    bool setBitsPerSample(int bits);
    bool setChannels(int channels);
    bool playChunk();
    bool playSample(int16_t sample[2]) ;
    bool playI2Sremains();
    int32_t Gain(int16_t s[2]);
    bool fill_InputBuf();
    void showstreamtitle(const char* ml);
    void parsePlaylistData(const char* pd);
    void parseAudioHeader(const char* ah);
    bool parseContentType(const char* ct);
    void processControlData(uint8_t b);
    esp_err_t I2Sstart(uint8_t i2s_num);
    esp_err_t I2Sstop(uint8_t i2s_num);
    String urlencode(String str);
    int16_t* IIR_filterChain(int16_t iir_in[2], bool clear = false);
    void IIR_calculateCoefficients();

    // implement several function with respect to the index of string
    bool startsWith (const char* base, const char* str) { return (strstr(base, str) - base) == 0;}
    bool endsWith (const char* base, const char* str) {
        int blen = strlen(base);
        int slen = strlen(str);
        return (blen >= slen) && (0 == strcmp(base + blen - slen, str));
    }
    int indexOf (const char* base, const char* str, int startIndex) {
        int result;
        int baselen = strlen(base);
        if (strlen(str) > baselen || startIndex > baselen) result = -1;
        else {
            char* pos = strstr(base + startIndex, str);
            if (pos == NULL) result = -1;
            else result = pos - base;
        }
        return result;
    }

private:
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    enum : int { CODEC_NONE = 0, CODEC_WAV = 1, CODEC_MP3 = 2, CODEC_AAC = 4, CODEC_FLAC = 5};
    enum : int { FORMAT_NONE = 0, FORMAT_M3U = 1, FORMAT_PLS = 2, FORMAT_ASX = 3};
    enum : int { AUDIO_NONE, AUDIO_HEADER , AUDIO_DATA, AUDIO_METADATA, AUDIO_PLAYLISTINIT,
                 AUDIO_PLAYLISTHEADER,  AUDIO_PLAYLISTDATA, AUDIO_SWM };
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;

    const uint8_t volumetable[22]={   0,  1,  2,  3,  4 , 6 , 8, 10, 12, 14, 17,
                                     20, 23, 27, 30 ,34, 38, 43 ,48, 52, 58, 64}; //22 elements

    typedef struct _filter{
        float a0;
        float a1;
        float a2;
        float b1;
        float b2;
    } filter_t;

    File              audiofile;    // @suppress("Abstract class cannot be instantiated")
    WiFiClient        client;       // @suppress("Abstract class cannot be instantiated")
    WiFiClientSecure  clientsecure; // @suppress("Abstract class cannot be instantiated")
    i2s_config_t      m_i2s_config; // stores values for I2S driver


    char            chbuf[256];
    char            path[256];
    char            m_plsURL[256];                  // URL found in playlist
    char            m_lastHost[256];                // Store the last URL to a webstream
    char            m_audioName[256];               // the name of the file
    filter_t        m_filter[2];
    int             m_id3Size=0;                    // length id3 tag
    int             m_LFcount;                      // Detection of end of header
    uint32_t        m_sampleRate=16000;
    int             m_bytesLeft=0;
    uint32_t        m_bitRate=0;                    // current bitrate given fom decoder
    uint32_t        m_avr_bitrate;                  // average bitrate, median computed by VBR
    int             m_readbytes=0;                  // bytes read
    int             m_metalen=0;                    // Number of bytes in metadata
    int             m_controlCounter = 0;           // Status within readID3data() and readWaveHeader()
    int8_t          m_balance = 0;                  // -16 (mute left) ... +16 (mute right)
    int8_t          m_DIN=0;                        // Data In, can be negative if unused (I2S_PIN_NO_CHANGE is -1)
    uint8_t         m_rev=0;                        // revision, ID3 version
    uint8_t         m_BCLK=0;                       // Bit Clock
    uint8_t         m_LRC=0;                        // Left/Right Clock
    uint8_t         m_DOUT=0;                       // Data Out
    uint8_t         m_vol=64;                       // volume
    uint8_t         m_bitsPerSample = 16;           // bitsPerSample
    uint8_t         m_channels=2;
    uint8_t         m_i2s_num = I2S_NUM_0;          // I2S_NUM_0 or I2S_NUM_1
    uint8_t         m_playlistFormat = 0;           // M3U, PLS, ASX
    uint8_t         m_codec = CODEC_NONE;           //
    uint8_t         m_filterType[2];                // lowpass, highpass
    int16_t         m_outBuff[2048*2];              // [1152 * 2];          // Interleaved L/R
    int16_t         m_validSamples = 0;
    int16_t         m_curSample;
    uint16_t        m_st_remember = 0;              // Save hash from the last streamtitle
    uint16_t        m_datamode = 0;                 // Statemaschine
    uint32_t        m_metaint = 0;                  // Number of databytes between metadata
    uint32_t        m_totalcount = 0;               // Counter mp3 data
    uint32_t        m_chunkcount = 0 ;              // Counter for chunked transfer
    uint32_t        m_t0;                           // store millis(), is needed for a small delay
    uint32_t        m_metaCount = 0;                // Bytecounter between metadata
    uint32_t        m_contentlength = 0;            // Stores the length if the stream comes from fileserver
    uint32_t        m_bytectr = 0;                  // count received data
    uint32_t        m_bytesNotDecoded = 0;          // pictures or something else that comes with the stream
    bool            m_f_unsync = false;             // set within ID3 tag but not used
    bool            m_f_exthdr = false;             // ID3 extended header
    bool            m_f_localfile = false ;         // Play from local mp3-file
    bool            m_f_webstream = false ;         // Play from URL
    bool            m_f_ssl = false;
    bool            m_f_running = false;
    bool            m_f_firststream_ready = false;  // Set after connecttohost and first streamdata are available
    bool            m_f_ctseen = false;             // First line of header seen or not
    bool            m_f_chunked = false ;           // Station provides chunked transfer
    bool            m_f_swm = false;
    bool            m_f_firstmetabyte = false;      // True if first metabyte (counter)
    bool            m_f_stream = false;             // Set false if stream is lost
    bool            m_f_playing = false;            // valid mp3 stream recognized
    bool            m_f_webfile= false;             // assume it's a radiostream, not a podcast
    bool            m_f_psram = false;              // set if PSRAM is availabe
    bool            m_f_loop = false;               // Set if audio file should loop
    bool            m_f_forceMono = false;          // if true stereo -> mono
    uint32_t        m_audioFileDuration = 0;
    float           m_audioCurrentTime = 0;
    float           m_filterBuff[2][2][2];          // IIR filters memory for Audio DSP
    size_t          m_i2s_bytesWritten = 0;         // set in i2s_write() but not used
    size_t          m_loop_point = 0;               // Point in the file where the audio data starts
    size_t          m_file_size = 0;                // size of the file
    uint16_t        m_filterFrequency[2];
};

#endif /* AUDIO_H_ */
