/*
 * Audio.h
 *
 *  Created on: Oct 26,2018
 *  Updated on: Sep 18,2021
 *      Author: Wolle (schreibfaul1)
 */

//#define SDFATFS_USED  // activate for SdFat


#pragma once
#pragma GCC optimize ("Ofast")

#include "Arduino.h"
#include "libb64/cencode.h"
#include "SPI.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "driver/i2s.h"

#ifdef SDFATFS_USED
#include "SdFat.h"  // use https://github.com/greiman/SdFat-beta for UTF-8 filenames
#else
#include "SD.h"
#include "SD_MMC.h"
#include "SPIFFS.h"
#include "FS.h"
#include "FFat.h"
#endif // SDFATFS_USED


#ifdef SDFATFS_USED
typedef File32 File;

namespace fs {
    class FS : public SdFat {
    public:
        // bool begin(SdCsPin_t csPin = SS, uint32_t maxSck = SD_SCK_MHZ(25)) { return SdFat::begin(csPin, maxSck); }
        //25MHz Failed to open file for reading. The  format is not supported
  		//SD_SCK = 16MHz is normal
        // support unicode encoding filename
        bool begin(SdCsPin_t csPin = SS, uint32_t maxSck = SD_SCK_MHZ(16)) {
            sd.begin(SdSpiConfig(csPin, DEDICATED_SPI, maxSck));
        }
    private:
        SdFat sd; //SdFat does not work without this.
    };

    class SDFATFS : public fs::FS {
    public:
        // sdcard_type_t cardType();
        uint64_t cardSize() {
            return totalBytes();
        }
        uint64_t usedBytes() {
            // set SdFatConfig MAINTAIN_FREE_CLUSTER_COUNT non-zero. Then only the first call will take time.
            return (uint64_t)(clusterCount() - freeClusterCount()) * (uint64_t)bytesPerCluster();
        }
        uint64_t totalBytes() {
            return (uint64_t)clusterCount() * (uint64_t)bytesPerCluster();
        }
    };
}

extern fs::SDFATFS SD_SDFAT;

using namespace fs;
#define SD SD_SDFAT
#endif //SDFATFS_USED




extern __attribute__((weak)) void audio_info(const char*);
extern __attribute__((weak)) void audio_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void audio_id3image(File& file, const size_t pos, const size_t size); //ID3 metadata image
extern __attribute__((weak)) void audio_eof_mp3(const char*); //end of mp3 file
extern __attribute__((weak)) void audio_showstreamtitle(const char*);
extern __attribute__((weak)) void audio_showstation(const char*);
extern __attribute__((weak)) void audio_bitrate(const char*);
extern __attribute__((weak)) void audio_commercial(const char*);
extern __attribute__((weak)) void audio_icyurl(const char*);
extern __attribute__((weak)) void audio_icydescription(const char*);
extern __attribute__((weak)) void audio_lasthost(const char*);
extern __attribute__((weak)) void audio_eof_speech(const char*);
extern __attribute__((weak)) void audio_eof_stream(const char*); // The webstream comes to an end

//----------------------------------------------------------------------------------------------------------------------

class AudioBuffer {
// AudioBuffer will be allocated in PSRAM, If PSRAM not available or has not enough space AudioBuffer will be
// allocated in FlashRAM with reduced size
//
//  m_buffer            m_readPtr                 m_writePtr                 m_endPtr
//   |                       |<------dataLength------->|<------ writeSpace ----->|
//   ▼                       ▼                         ▼                         ▼
//   ---------------------------------------------------------------------------------------------------------------
//   |                     <--m_buffSize-->                                      |      <--m_resBuffSize -->     |
//   ---------------------------------------------------------------------------------------------------------------
//   |<-----freeSpace------->|                         |<------freeSpace-------->|
//
//
//
//   if the space between m_readPtr and buffend < m_resBuffSize copy data from the beginning to resBuff
//   so that the mp3/aac/flac frame is always completed
//
//  m_buffer                      m_writePtr                 m_readPtr        m_endPtr
//   |                                 |<-------writeSpace------>|<--dataLength-->|
//   ▼                                 ▼                         ▼                ▼
//   ---------------------------------------------------------------------------------------------------------------
//   |                        <--m_buffSize-->                                    |      <--m_resBuffSize -->     |
//   ---------------------------------------------------------------------------------------------------------------
//   |<---  ------dataLength--  ------>|<-------freeSpace------->|
//
//

public:
    AudioBuffer(size_t maxBlockSize = 0);       // constructor
    ~AudioBuffer();                             // frees the buffer
    size_t   init();                            // set default values
    void     changeMaxBlockSize(uint16_t mbs);  // is default 1600 for mp3 and aac, set 16384 for FLAC
    uint16_t getMaxBlockSize();                 // returns maxBlockSize
    size_t   freeSpace();                       // number of free bytes to overwrite
    size_t   writeSpace();                      // space fom writepointer to bufferend
    size_t   bufferFilled();                    // returns the number of filled bytes
    void     bytesWritten(size_t bw);           // update writepointer
    void     bytesWasRead(size_t br);           // update readpointer
    uint8_t* getWritePtr();                     // returns the current writepointer
    uint8_t* getReadPtr();                      // returns the current readpointer
    uint32_t getWritePos();                     // write position relative to the beginning
    uint32_t getReadPos();                      // read position relative to the beginning
    void     resetBuffer();                     // restore defaults

protected:
    const size_t m_buffSizePSRAM    = 300000; // most webstreams limit the advance to 100...300Kbytes
    const size_t m_buffSizeRAM      = 1600 * 5;
    size_t       m_buffSize         = 0;
    size_t       m_freeSpace        = 0;
    size_t       m_writeSpace       = 0;
    size_t       m_dataLength       = 0;
    size_t       m_resBuffSizeRAM   = 1600;     // reserved buffspace, >= one mp3  frame
    size_t       m_resBuffSizePSRAM = 4096 * 4; // reserved buffspace, >= one flac frame
    size_t       m_maxBlockSize     = 1600;
    uint8_t*     m_buffer           = NULL;
    uint8_t*     m_writePtr         = NULL;
    uint8_t*     m_readPtr          = NULL;
    uint8_t*     m_endPtr           = NULL;
    bool         m_f_start          = true;
};
//----------------------------------------------------------------------------------------------------------------------

class Audio : private AudioBuffer{

    AudioBuffer InBuff; // instance of input buffer

public:
    Audio(bool internalDAC = false, i2s_dac_mode_t channelEnabled = I2S_DAC_CHANNEL_LEFT_EN); // #99
    ~Audio();
    bool connecttohost(const char* host, const char* user = "", const char* pwd = "");
    bool connecttospeech(const char* speech, const char* lang);
    bool connecttoFS(fs::FS &fs, const char* path);
    bool connecttoSD(const char* path);
    bool setFileLoop(bool input);//TEST loop
    bool setAudioPlayPosition(uint16_t sec);
    bool setFilePos(uint32_t pos);
    bool audioFileSeek(const float speed);
    bool setTimeOffset(int sec);
    bool setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t DIN=I2S_PIN_NO_CHANGE);
    bool pauseResume();
    bool isRunning() {return m_f_running;}
    void loop();
    void stopSong();
    void forceMono(bool m);
    void setBalance(int8_t bal = 0);
    void setVolume(uint8_t vol);
    uint8_t getVolume();

    uint32_t getAudioDataStartPos();
    uint32_t getFileSize();
    uint32_t getFilePos();
    uint32_t getSampleRate();
    uint8_t  getBitsPerSample();
    uint8_t  getChannels();
    uint32_t getBitRate();
    uint32_t getAudioFileDuration();
    uint32_t getAudioCurrentTime();
    uint32_t getTotalPlayingTime();

    esp_err_t i2s_mclk_pin_select(const uint8_t pin);
    uint32_t inBufferFilled(); // returns the number of stored bytes in the inputbuffer
    uint32_t inBufferFree();   // returns the number of free bytes in the inputbuffer
    void setTone(int8_t gainLowPass, int8_t gainBandPass, int8_t gainHighPass);
    [[deprecated]]void setInternalDAC(bool internalDAC = true, i2s_dac_mode_t channelEnabled = I2S_DAC_CHANNEL_LEFT_EN);
    void setI2SCommFMT_LSB(bool commFMT);

private:
    void UTF8toASCII(char* str);
    bool latinToUTF8(char* buff, size_t bufflen);
    void httpPrint(const char* url);
    void setDefaults(); // free buffers and set defaults
    void initInBuff();
    void processLocalFile();
    void processWebStream();
    void processPlayListData();
    void processM3U8entries(uint8_t nrOfEntries = 0, uint32_t seqNr = 0, uint8_t pos = 0, uint16_t targetDuration = 0);
    void showCodecParams();
    int  findNextSync(uint8_t* data, size_t len);
    int  sendBytes(uint8_t* data, size_t len);
    void compute_audioCurrentTime(int bd);
    void printDecodeError(int r);
    void showID3Tag(const char* tag, const char* val);
    void unicode2utf8(char* buff, uint32_t len);
    int  read_WAV_Header(uint8_t* data, size_t len);
    int  read_FLAC_Header(uint8_t *data, size_t len);
    int  read_MP3_Header(uint8_t* data, size_t len);
    int  read_M4A_Header(uint8_t* data, size_t len);
    int  read_OGG_Header(uint8_t *data, size_t len);
    bool setSampleRate(uint32_t hz);
    bool setBitsPerSample(int bits);
    bool setChannels(int channels);
    bool setBitrate(int br);
    bool playChunk();
    bool playSample(int16_t sample[2]) ;
    bool playI2Sremains();
    int32_t Gain(int16_t s[2]);
    bool fill_InputBuf();
    void showstreamtitle(const char* ml);
    bool parseContentType(const char* ct);
    void processAudioHeaderData();
    bool readMetadata(uint8_t b, bool first = false);
    esp_err_t I2Sstart(uint8_t i2s_num);
    esp_err_t I2Sstop(uint8_t i2s_num);
    void urlencode(char* buff, uint16_t buffLen, bool spacesOnly = false);
    int16_t* IIR_filterChain0(int16_t iir_in[2], bool clear = false);
    int16_t* IIR_filterChain1(int16_t* iir_in, bool clear = false);
    int16_t* IIR_filterChain2(int16_t* iir_in, bool clear = false);
    inline void setDatamode(uint8_t dm){m_datamode=dm;}
    inline uint8_t getDatamode(){return m_datamode;}
    inline uint32_t streamavail() {if(m_f_ssl==false) return client.available(); else return clientsecure.available();}
    void IIR_calculateCoefficients(int8_t G1, int8_t G2, int8_t G3);

    // implement several function with respect to the index of string
    bool startsWith (const char* base, const char* str) { return (strstr(base, str) - base) == 0;}
    bool endsWith (const char* base, const char* str) {
        int blen = strlen(base);
        int slen = strlen(str);
        while(isspace(*(base + blen - 1))) blen--;  // rtrim
        char* pos = strstr(base + blen - slen, str);
        if (pos == NULL) return false;
        else return true;
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
    int lastIndexOf(const char* base, const char* str){
        int pos = -1, lastIndex = -1;
        while(true){
            pos = indexOf(base, str, pos + 1);
            if(pos >= 0) lastIndex = pos;
            else break;
        }
        return lastIndex;
    }
    int specialIndexOf (uint8_t* base, const char* str, int baselen, bool exact = false){
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
    size_t bigEndian(uint8_t* base, uint8_t numBytes, uint8_t shiftLeft = 8){
        size_t result = 0;
        if(numBytes < 1 or numBytes > 4) return 0;
        for (int i = 0; i < numBytes; i++) {
                result += *(base + i) << (numBytes -i - 1) * shiftLeft;
        }
        return result;
    }
    bool b64encode(const char* source, uint16_t sourceLength, char* dest){
        size_t size = base64_encode_expected_len(sourceLength) + 1;
        char * buffer = (char *) malloc(size);
        if(buffer) {
            base64_encodestate _state;
            base64_init_encodestate(&_state);
            int len = base64_encode_block(&source[0], sourceLength, &buffer[0], &_state);
            len = base64_encode_blockend((buffer + len), &_state);
            memcpy(dest, buffer, strlen(buffer));
            dest[strlen(buffer)] = '\0';
            free(buffer);
            return true;
        }
        return false;
    }
    size_t urlencode_expected_len(const char* source){
        size_t expectedLen = strlen(source);
        for(int i = 0; i < strlen(source); i++) {
            if(isalnum(source[i])){;}
            else expectedLen += 2;
        }
        return expectedLen;
    }
    void trim(char* s){
        uint8_t l = 0;
        while(isspace(*(s + l))) l++;
        for(uint16_t i = 0; i< strlen(s) - l; i++)  *(s + i) = *(s + i + l); // ltrim
        char* back = s + strlen(s);
        while(isspace(*--back));
        *(back + 1) = '\0';      // rtrim
    }


private:
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    enum : int { CODEC_NONE, CODEC_WAV, CODEC_MP3, CODEC_AAC, CODEC_M4A, CODEC_FLAC, CODEC_OGG,
                 CODEC_OGG_FLAC, CODEC_OGG_OPUS};
    enum : int { FORMAT_NONE = 0, FORMAT_M3U = 1, FORMAT_PLS = 2, FORMAT_ASX = 3, FORMAT_M3U8 = 4};
    enum : int { AUDIO_NONE, AUDIO_HEADER, AUDIO_DATA,
                 AUDIO_PLAYLISTINIT, AUDIO_PLAYLISTHEADER,  AUDIO_PLAYLISTDATA};
    enum : int { FLAC_BEGIN = 0, FLAC_MAGIC = 1, FLAC_MBH =2, FLAC_SINFO = 3, FLAC_PADDING = 4, FLAC_APP = 5,
                 FLAC_SEEK = 6, FLAC_VORBIS = 7, FLAC_CUESHEET = 8, FLAC_PICTURE = 9, FLAC_OKAY = 100};
    enum : int { M4A_BEGIN = 0, M4A_FTYP = 1, M4A_CHK = 2, M4A_MOOV = 3, M4A_FREE = 4, M4A_TRAK = 5, M4A_MDAT = 6,
                 M4A_ILST = 7, M4A_MP4A = 8, M4A_AMRDY = 99, M4A_OKAY = 100};
    enum : int { OGG_BEGIN = 0, OGG_MAGIC = 1, OGG_HEADER = 2, OGG_FIRST = 3, OGG_AMRDY = 99, OGG_OKAY = 100};
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;
    typedef enum { LOWSHELF = 0, PEAKEQ = 1, HIFGSHELF =2 } FilterType;

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
    WiFiUDP           udpclient;    // @suppress("Abstract class cannot be instantiated")
    i2s_config_t      m_i2s_config; // stores values for I2S driver
    i2s_pin_config_t  m_pin_config;

    const size_t    m_frameSizeWav  = 1600;
    const size_t    m_frameSizeMP3  = 1600;
    const size_t    m_frameSizeAAC  = 1600;
    const size_t    m_frameSizeFLAC = 4096 * 4;

    char            chbuf[512];
    char            m_lastHost[256];                // Store the last URL to a webstream
    char*           m_playlistBuff = NULL;          // stores playlistdata
    const uint16_t  m_plsBuffEntryLen = 256;        // length of each entry in playlistBuff
    filter_t        m_filter[3];                    // digital filters
    int             m_LFcount = 0;                  // Detection of end of header
    uint32_t        m_sampleRate=16000;
    uint32_t        m_bitRate=0;                    // current bitrate given fom decoder
    uint32_t        m_avr_bitrate = 0;              // average bitrate, median computed by VBR
    int             m_readbytes=0;                  // bytes read
    int             m_metalen=0;                    // Number of bytes in metadata
    int             m_controlCounter = 0;           // Status within readID3data() and readWaveHeader()
    int8_t          m_balance = 0;                  // -16 (mute left) ... +16 (mute right)
    uint8_t         m_vol=64;                       // volume
    uint8_t         m_bitsPerSample = 16;           // bitsPerSample
    uint8_t         m_channels=2;
    uint8_t         m_i2s_num = I2S_NUM_0;          // I2S_NUM_0 or I2S_NUM_1
    uint8_t         m_playlistFormat = 0;           // M3U, PLS, ASX
    uint8_t         m_m3u8codec = CODEC_NONE;       // M4A
    uint8_t         m_codec = CODEC_NONE;           //
    uint8_t         m_filterType[2];                // lowpass, highpass
    int16_t         m_outBuff[2048*2];              // Interleaved L/R
    int16_t         m_validSamples = 0;
    int16_t         m_curSample = 0;
    uint16_t        m_st_remember = 0;              // Save hash from the last streamtitle
    uint16_t        m_datamode = 0;                 // Statemaschine
    uint8_t         m_flacBitsPerSample = 0;        // bps should be 16
    uint8_t         m_flacNumChannels = 0;          // can be read out in the FLAC file header
    uint32_t        m_flacSampleRate = 0;           // can be read out in the FLAC file header
    uint16_t        m_flacMaxFrameSize = 0;         // can be read out in the FLAC file header
    uint16_t        m_flacMaxBlockSize = 0;         // can be read out in the FLAC file header
    uint32_t        m_flacTotalSamplesInStream = 0; // can be read out in the FLAC file header
    uint32_t        m_metaint = 0;                  // Number of databytes between metadata
    uint32_t        m_chunkcount = 0 ;              // Counter for chunked transfer
    uint32_t        m_t0 = 0;                       // store millis(), is needed for a small delay
    uint32_t        m_contentlength = 0;            // Stores the length if the stream comes from fileserver
    uint32_t        m_bytesNotDecoded = 0;          // pictures or something else that comes with the stream
    uint32_t        m_PlayingStartTime = 0;         // Stores the milliseconds after the start of the audio
    bool            m_f_swm = true;                 // Stream without metadata
    bool            m_f_unsync = false;             // set within ID3 tag but not used
    bool            m_f_exthdr = false;             // ID3 extended header
    bool            m_f_localfile = false ;         // Play from local mp3-file
    bool            m_f_webstream = false ;         // Play from URL
    bool            m_f_ssl = false;
    bool            m_f_running = false;
    bool            m_f_firstCall = false;          // InitSequence for processWebstream and processLokalFile
    bool            m_f_ctseen = false;             // First line of header seen or not
    bool            m_f_chunked = false ;           // Station provides chunked transfer
    bool            m_f_firstmetabyte = false;      // True if first metabyte (counter)
    bool            m_f_playing = false;            // valid mp3 stream recognized
    bool            m_f_webfile = false;            // assume it's a radiostream, not a podcast
    bool            m_f_tts = false;                // text to speech
    bool            m_f_psram = false;              // set if PSRAM is availabe
    bool            m_f_loop = false;               // Set if audio file should loop
    bool            m_f_forceMono = false;          // if true stereo -> mono
    bool            m_f_internalDAC = false;        // false: output vis I2S, true output via internal DAC
    bool            m_f_rtsp = false;               // set if RTSP is used (m3u8 stream)
    bool            m_f_m3u8data = false;           // used in processM3U8entries
    bool            m_f_Log = true;                 // if m3u8: log is cancelled
    bool            m_f_continue = false;           // next m3u8 chunk is available
    i2s_dac_mode_t  m_f_channelEnabled = I2S_DAC_CHANNEL_LEFT_EN;  // internal DAC on GPIO26 for M5StickC/Plus
    uint32_t        m_audioFileDuration = 0;
    float           m_audioCurrentTime = 0;
    uint32_t        m_audioDataStart = 0;           // in bytes
    size_t          m_audioDataSize = 0;            //
    float           m_filterBuff[3][2][2][2];       // IIR filters memory for Audio DSP
    size_t          m_i2s_bytesWritten = 0;         // set in i2s_write() but not used
    size_t          m_file_size = 0;                // size of the file
    uint16_t        m_filterFrequency[2];
    int8_t          m_gain0 = 0;                    // cut or boost filters (EQ)
    int8_t          m_gain1 = 0;
    int8_t          m_gain2 = 0;
};

//----------------------------------------------------------------------------------------------------------------------

