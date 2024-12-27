#include "esp_arduino_version.h"
/*
 * Audio.h
 *
 *  Created on: Oct 28,2018
 *
 *  Version 3.0.13z
 *  Updated on: Dec 16.2024
 *      Author: Wolle (schreibfaul1)
 */

#pragma once
#pragma GCC optimize ("Ofast")
#include <vector>
#include <Arduino.h>
#include <libb64/cencode.h>
#include <esp32-hal-log.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPIFFS.h>
#include <FS.h>
#include <FFat.h>
#include <atomic>
#include <codecvt>
#include <locale>

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#endif

#if ESP_IDF_VERSION_MAJOR == 5
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif

#ifndef I2S_GPIO_UNUSED
  #define I2S_GPIO_UNUSED -1 // = I2S_PIN_NO_CHANGE in IDF < 5
#endif
using namespace std;

extern __attribute__((weak)) void audio_info(const char*);
extern __attribute__((weak)) void audio_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void audio_id3image(File& file, const size_t pos, const size_t size); //ID3 metadata image
extern __attribute__((weak)) void audio_oggimage(File& file, std::vector<uint32_t> v); //OGG blockpicture
extern __attribute__((weak)) void audio_id3lyrics(File& file, const size_t pos, const size_t size); //ID3 metadata lyrics
extern __attribute__((weak)) void audio_eof_mp3(const char*); //end of mp3 file
extern __attribute__((weak)) void audio_showstreamtitle(const char*);
extern __attribute__((weak)) void audio_showstation(const char*);
extern __attribute__((weak)) void audio_bitrate(const char*);
extern __attribute__((weak)) void audio_commercial(const char*);
extern __attribute__((weak)) void audio_icyurl(const char*);
extern __attribute__((weak)) void audio_icylogo(const char*);
extern __attribute__((weak)) void audio_icydescription(const char*);
extern __attribute__((weak)) void audio_lasthost(const char*);
extern __attribute__((weak)) void audio_eof_speech(const char*);
extern __attribute__((weak)) void audio_eof_stream(const char*); // The webstream comes to an end
extern __attribute__((weak)) void audio_process_i2s(int16_t* outBuff, uint16_t validSamples, uint8_t bitsPerSample, uint8_t channels, bool *continueI2S); // record audiodata or send via BT
extern __attribute__((weak)) void audio_log(uint8_t logLevel, const char* msg, const char* arg);

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
    bool     isInitialized() { return m_f_init; };
    void     setBufsize(int ram, int psram);
    int32_t  getBufsize();
    void     changeMaxBlockSize(uint16_t mbs);  // is default 1600 for mp3 and aac, set 16384 for FLAC
    uint16_t getMaxBlockSize();                 // returns maxBlockSize
    size_t   freeSpace();                       // number of free bytes to overwrite
    size_t   writeSpace();                      // space fom writepointer to bufferend
    size_t   bufferFilled();                    // returns the number of filled bytes
    size_t   getMaxAvailableBytes();            // max readable bytes in one block
    void     bytesWritten(size_t bw);           // update writepointer
    void     bytesWasRead(size_t br);           // update readpointer
    uint8_t* getWritePtr();                     // returns the current writepointer
    uint8_t* getReadPtr();                      // returns the current readpointer
    uint32_t getWritePos();                     // write position relative to the beginning
    uint32_t getReadPos();                      // read position relative to the beginning
    void     resetBuffer();                     // restore defaults
    bool     havePSRAM() { return m_f_psram; };

protected:
    size_t            m_buffSizePSRAM    = UINT16_MAX * 10;   // most webstreams limit the advance to 100...300Kbytes
    size_t            m_buffSizeRAM      = 1600 * 10;
    size_t            m_buffSize         = 0;
    size_t            m_freeSpace        = 0;
    size_t            m_writeSpace       = 0;
    size_t            m_dataLength       = 0;
    size_t            m_resBuffSizeRAM   = 2048;     // reserved buffspace, >= one wav  frame
    size_t            m_resBuffSizePSRAM = 4096 * 4; // reserved buffspace, >= one flac frame
    size_t            m_maxBlockSize     = 1600;
    uint8_t*          m_buffer           = NULL;
    uint8_t*          m_writePtr         = NULL;
    uint8_t*          m_readPtr          = NULL;
    uint8_t*          m_endPtr           = NULL;
    bool              m_f_init           = false;
    bool              m_f_isEmpty        = true;
    bool              m_f_psram          = false;    // PSRAM is available (and used...)
};
//----------------------------------------------------------------------------------------------------------------------

static const size_t AUDIO_STACK_SIZE = 3300;
static StaticTask_t __attribute__((unused)) xAudioTaskBuffer;
static StackType_t  __attribute__((unused)) xAudioStack[AUDIO_STACK_SIZE];

class Audio : private AudioBuffer{

    AudioBuffer InBuff; // instance of input buffer

public:
    Audio(bool internalDAC = false, uint8_t channelEnabled = 3, uint8_t i2sPort = I2S_NUM_0); // #99
    ~Audio();
    void setBufsize(int rambuf_sz, int psrambuf_sz);
    bool openai_speech(const String& api_key, const String& model, const String& input, const String& voice, const String& response_format, const String& speed);
    bool connecttohost(const char* host, const char* user = "", const char* pwd = "");
    bool connecttospeech(const char* speech, const char* lang);
    bool connecttoFS(fs::FS &fs, const char* path, int32_t m_fileStartPos = -1);
    bool setFileLoop(bool input);//TEST loop
    void setConnectionTimeout(uint16_t timeout_ms, uint16_t timeout_ms_ssl);
    bool setAudioPlayPosition(uint16_t sec);
    bool setFilePos(uint32_t pos);
    bool audioFileSeek(const float speed);
    bool setTimeOffset(int sec);
    bool setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t MCLK = I2S_GPIO_UNUSED);
    bool pauseResume();
    bool isRunning() {return m_f_running;}
    void loop();
    uint32_t stopSong();
    void forceMono(bool m);
    void setBalance(int8_t bal = 0);
    void setVolumeSteps(uint8_t steps);
    void setVolume(uint8_t vol, uint8_t curve = 0);
    uint8_t getVolume();
    uint8_t maxVolume();
    uint8_t getI2sPort();

    uint32_t getAudioDataStartPos();
    uint32_t getFileSize();
    uint32_t getFilePos();
    uint32_t getSampleRate();
    uint8_t  getBitsPerSample();
    uint8_t  getChannels();
    uint32_t getBitRate(bool avg = false);
    uint32_t getAudioFileDuration();
    uint32_t getAudioCurrentTime();
    uint32_t getTotalPlayingTime();
    uint16_t getVUlevel();

    uint32_t inBufferFilled(); // returns the number of stored bytes in the inputbuffer
    uint32_t inBufferFree();   // returns the number of free bytes in the inputbuffer
    uint32_t inBufferSize();   // returns the size of the inputbuffer in bytes
    void setTone(int8_t gainLowPass, int8_t gainBandPass, int8_t gainHighPass);
    void setI2SCommFMT_LSB(bool commFMT);
    int getCodec() {return m_codec;}
    const char *getCodecname() {return codecname[m_codec];}

private:

    #ifndef ESP_ARDUINO_VERSION_VAL
        #define ESP_ARDUINO_VERSION_MAJOR 0
        #define ESP_ARDUINO_VERSION_MINOR 0
        #define ESP_ARDUINO_VERSION_PATCH 0
    #endif

  enum : int8_t { AUDIOLOG_PATH_IS_NULL = -1, AUDIOLOG_FILE_NOT_FOUND = -2, AUDIOLOG_OUT_OF_MEMORY = -3, AUDIOLOG_FILE_READ_ERR = -4,
                  AUDIOLOG_M4A_ATOM_NOT_FOUND = -5,  AUDIOLOG_ERR_UNKNOWN = -127 };

  void            UTF8toASCII(char* str);
  bool            latinToUTF8(char* buff, size_t bufflen, bool UTF8check = true);
  void            setDefaults(); // free buffers and set defaults
  void            initInBuff();
  bool            httpPrint(const char* host);
  bool            httpRange(const char* host, uint32_t range);
  void            processLocalFile();
  void            processWebStream();
  void            processWebFile();
  void            processWebStreamTS();
  void            processWebStreamHLS();
  void            playAudioData();
  bool            readPlayListData();
  const char*     parsePlaylist_M3U();
  const char*     parsePlaylist_PLS();
  const char*     parsePlaylist_ASX();
  const char*     parsePlaylist_M3U8();
  const char*     m3u8redirection(uint8_t* codec);
  uint64_t        m3u8_findMediaSeqInURL();
  bool            STfromEXTINF(char* str);
  void            showCodecParams();
  int             findNextSync(uint8_t* data, size_t len);
  int             sendBytes(uint8_t* data, size_t len);
  void            setDecoderItems();
  void            computeAudioTime(uint16_t bytesDecoderIn, uint16_t bytesDecoderOut);
  void            printProcessLog(int r, const char* s = "");
  void            printDecodeError(int r);
  void            showID3Tag(const char* tag, const char* val);
  size_t          readAudioHeader(uint32_t bytes);
  int             read_WAV_Header(uint8_t* data, size_t len);
  int             read_FLAC_Header(uint8_t* data, size_t len);
  int             read_ID3_Header(uint8_t* data, size_t len);
  int             read_M4A_Header(uint8_t* data, size_t len);
  size_t          process_m3u8_ID3_Header(uint8_t* packet);
  bool            setSampleRate(uint32_t hz);
  bool            setBitsPerSample(int bits);
  bool            setChannels(int channels);
  void            reconfigI2S();
  bool            setBitrate(int br);
  void            playChunk();
  void            computeVUlevel(int16_t sample[2]);
  void            computeLimit();
  void            Gain(int16_t* sample);
  void            showstreamtitle(const char* ml);
  bool            parseContentType(char* ct);
  bool            parseHttpResponseHeader();
  bool            initializeDecoder(uint8_t codec);
  esp_err_t       I2Sstart(uint8_t i2s_num);
  esp_err_t       I2Sstop(uint8_t i2s_num);
  void            IIR_filterChain0(int16_t iir_in[2], bool clear = false);
  void            IIR_filterChain1(int16_t iir_in[2], bool clear = false);
  void            IIR_filterChain2(int16_t iir_in[2], bool clear = false);
  inline uint32_t streamavail() { return _client ? _client->available() : 0; }
  void            IIR_calculateCoefficients(int8_t G1, int8_t G2, int8_t G3);
  bool            ts_parsePacket(uint8_t* packet, uint8_t* packetStart, uint8_t* packetLength);
  uint32_t        find_m4a_atom(uint32_t fileSize, const char* atomType, uint32_t depth = 0);

  //+++ create a T A S K  for playAudioData(), output via I2S +++
public:
  void            setAudioTaskCore(uint8_t coreID);
  uint32_t        getHighWatermark();
private:
  void            startAudioTask(); // starts a task for decode and play
  void            stopAudioTask();  // stops task for audio
  static void     taskWrapper(void *param);
  void            audioTask();
  void            performAudioTask();

  //+++ W E B S T R E A M  -  H E L P   F U N C T I O N S +++
  uint16_t readMetadata(uint16_t b, bool first = false);
  size_t   chunkedDataTransfer(uint8_t* bytes);
  bool     readID3V1Tag();
  boolean  streamDetection(uint32_t bytesAvail);
  void     seek_m4a_stsz();
  void     seek_m4a_ilst();
  uint32_t m4a_correctResumeFilePos(uint32_t resumeFilePos);
  uint32_t ogg_correctResumeFilePos(uint32_t resumeFilePos);
  int32_t  flac_correctResumeFilePos(uint32_t resumeFilePos);
  int32_t  mp3_correctResumeFilePos(uint32_t resumeFilePos);
  uint8_t  determineOggCodec(uint8_t* data, uint16_t len);

  //++++ implement several function with respect to the index of string ++++
  void strlower(char* str) {
      unsigned char* p = (unsigned char*)str;
      while(*p) {
          *p = tolower((unsigned char)*p);
          p++;
      }
    }


void trim(char *str) {
    char *start = str;  // keep the original pointer
    char *end;
    while (isspace((unsigned char)*start)) start++; // find the first non-space character

    if (*start == 0) {  // all characters were spaces
        str[0] = '\0';  // return a empty string
        return;
    }

    end = start + strlen(start) - 1;  // find the end of the string

    while (end > start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';  // Null-terminate the string after the last non-space character

    // Move the trimmed string to the beginning of the memory area
    memmove(str, start, strlen(start) + 1);  // +1 for '\0'
}

    bool startsWith (const char* base, const char* str) {
    //fb
        char c;
        while ( (c = *str++) != '\0' )
          if (c != *base++) return false;
        return true;
    }

    bool endsWith(const char *base, const char *searchString) {
        int32_t slen = strlen(searchString);
        if(slen == 0) return false;
        const char *p = base + strlen(base);
    //  while(p > base && isspace(*p)) p--;  // rtrim
        p -= slen;
        if(p < base) return false;
        return (strncmp(p, searchString, slen) == 0);
    }

    int indexOf (const char* base, const char* str, int startIndex = 0) {
    //fb
        const char *p = base;
        for (; startIndex > 0; startIndex--)
            if (*p++ == '\0') return -1;
        char* pos = strstr(p, str);
        if (pos == nullptr) return -1;
        return pos - base;
    }

    int indexOf (const char* base, char ch, int startIndex = 0) {
    //fb
        const char *p = base;
        for (; startIndex > 0; startIndex--)
            if (*p++ == '\0') return -1;
        char *pos = strchr(p, ch);
        if (pos == nullptr) return -1;
        return pos - base;
    }

    int lastIndexOf(const char* haystack, const char* needle) {
    //fb
        int nlen = strlen(needle);
        if (nlen == 0) return -1;
        const char *p = haystack - nlen + strlen(haystack);
        while (p >= haystack) {
          int i = 0;
          while (needle[i] == p[i])
            if (++i == nlen) return p - haystack;
          p--;
        }
        return -1;
    }

    int lastIndexOf(const char* haystack, const char needle) {
    //fb
        const char *p = strrchr(haystack, needle);
        return (p ? p - haystack : -1);
    }

    int specialIndexOf (uint8_t* base, const char* str, int baselen, bool exact = false){
        int result = 0;  // seek for str in buffer or in header up to baselen, not nullterninated
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
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // some other functions
uint64_t bigEndian(uint8_t* base, uint8_t numBytes, uint8_t shiftLeft = 8) {
    uint64_t result = 0;  // Use uint64_t for greater caching
    if(numBytes < 1 || numBytes > 8) return 0;
    for (int i = 0; i < numBytes; i++) {
        result |= (uint64_t)(*(base + i)) << ((numBytes - i - 1) * shiftLeft); //Make sure the calculation is done correctly
    }
    if(result > SIZE_MAX) {
        log_e("range overflow");
        return 0;
    }
    return result;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    bool b64encode(const char* source, uint16_t sourceLength, char* dest){
        size_t size = base64_encode_expected_len(sourceLength) + 1;
        char * buffer = (char *) malloc(size);
        if(buffer) {
            base64_encodestate _state;
            base64_init_encodestate(&_state);
            int len = base64_encode_block(&source[0], sourceLength, &buffer[0], &_state);
            base64_encode_blockend((buffer + len), &_state);
            memcpy(dest, buffer, strlen(buffer));
            dest[strlen(buffer)] = '\0';
            free(buffer);
            return true;
        }
        return false;
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    char* urlencode(const char* str, bool spacesOnly){
        // Reserve memory for the result (3x the length of the input string, worst-case)
        char *encoded = x_ps_malloc(strlen(str) * 3 + 1);
        char *p_encoded = encoded;

        if (encoded == NULL) {
            return NULL;  // Memory allocation failed
        }

        while (*str) {
            // Adopt alphanumeric characters and secure characters directly
            if (isalnum((unsigned char)*str)) {
                *p_encoded++ = *str;
            }
            else if(spacesOnly && *str != 0x20) {
                *p_encoded++ = *str;
            }
            else {
                p_encoded += sprintf(p_encoded, "%%%02X", (unsigned char)*str);
            }
            str++;
        }
        *p_encoded = '\0';  // Null-terminieren
        return encoded;
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    void vector_clear_and_shrink(vector<char*>&vec){
        uint size = vec.size();
        for (int i = 0; i < size; i++) {
            if(vec[i]){
                free(vec[i]);
                vec[i] = NULL;
            }
        }
        vec.clear();
        vec.shrink_to_fit();
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    uint32_t simpleHash(const char* str){
        if(str == NULL) return 0;
        uint32_t hash = 0;
        for(int i=0; i<strlen(str); i++){
		    if(str[i] < 32) continue; // ignore control sign
		    hash += (str[i] - 31) * i * 32;
        }
        return hash;
	  }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    char* x_ps_malloc(uint16_t len) {
        char* ps_str = NULL;
        if(psramFound()){ps_str = (char*) ps_malloc(len);}
        else            {ps_str = (char*)    malloc(len);}
        if(!ps_str) log_e("oom");
        return ps_str;
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    char* x_ps_calloc(uint16_t len, uint8_t size) {
        char* ps_str = NULL;
        if(psramFound()){ps_str = (char*) ps_calloc(len, size);}
        else            {ps_str = (char*)    calloc(len, size);}
        if(!ps_str) log_e("oom");
        return ps_str;
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    char* x_ps_strdup(const char* str) {
        if(!str) {log_e("str is NULL"); return NULL;};
        char* ps_str = NULL;
        if(psramFound()) { ps_str = (char*)ps_malloc(strlen(str) + 1); }
        else { ps_str = (char*)malloc(strlen(str) + 1); }
        if(!ps_str){log_e("oom"); return NULL;}
        strcpy(ps_str, str);
        return ps_str;
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    void x_ps_free(void* b){
        if(b){free(b); b = NULL;}
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// Function to reverse the byte order of a 32-bit value (big-endian to little-endian)
    uint32_t bswap32(uint32_t x) {
        return ((x & 0xFF000000) >> 24) |
               ((x & 0x00FF0000) >> 8)  |
               ((x & 0x0000FF00) << 8)  |
               ((x & 0x000000FF) << 24);
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// Function to reverse the byte order of a 64-bit value (big-endian to little-endian)
    uint64_t bswap64(uint64_t x) {
        return ((x & 0xFF00000000000000ULL) >> 56) |
               ((x & 0x00FF000000000000ULL) >> 40) |
               ((x & 0x0000FF0000000000ULL) >> 24) |
               ((x & 0x000000FF00000000ULL) >> 8)  |
               ((x & 0x00000000FF000000ULL) << 8)  |
               ((x & 0x0000000000FF0000ULL) << 24) |
               ((x & 0x000000000000FF00ULL) << 40) |
               ((x & 0x00000000000000FFULL) << 56);
    }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————



private:
    const char *codecname[10] = {"unknown", "WAV", "MP3", "AAC", "M4A", "FLAC", "AACP", "OPUS", "OGG", "VORBIS" };
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    enum : int { FORMAT_NONE = 0, FORMAT_M3U = 1, FORMAT_PLS = 2, FORMAT_ASX = 3, FORMAT_M3U8 = 4};
    enum : int { AUDIO_NONE, HTTP_RESPONSE_HEADER, AUDIO_DATA, AUDIO_LOCALFILE,
                 AUDIO_PLAYLISTINIT, AUDIO_PLAYLISTHEADER,  AUDIO_PLAYLISTDATA};
    enum : int { FLAC_BEGIN = 0, FLAC_MAGIC = 1, FLAC_MBH =2, FLAC_SINFO = 3, FLAC_PADDING = 4, FLAC_APP = 5,
                 FLAC_SEEK = 6, FLAC_VORBIS = 7, FLAC_CUESHEET = 8, FLAC_PICTURE = 9, FLAC_OKAY = 100};
    enum : int { M4A_BEGIN = 0, M4A_FTYP = 1, M4A_CHK = 2, M4A_MOOV = 3, M4A_FREE = 4, M4A_TRAK = 5, M4A_MDAT = 6,
                 M4A_ILST = 7, M4A_MP4A = 8, M4A_AMRDY = 99, M4A_OKAY = 100};
    enum : int { CODEC_NONE = 0, CODEC_WAV = 1, CODEC_MP3 = 2, CODEC_AAC = 3, CODEC_M4A = 4, CODEC_FLAC = 5,
                 CODEC_AACP = 6, CODEC_OPUS = 7, CODEC_OGG = 8, CODEC_VORBIS = 9};
    enum : int { ST_NONE = 0, ST_WEBFILE = 1, ST_WEBSTREAM = 2};
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;
    typedef enum { LOWSHELF = 0, PEAKEQ = 1, HIFGSHELF =2 } FilterType;

    typedef struct _filter{
        float a0;
        float a1;
        float a2;
        float b1;
        float b2;
    } filter_t;

    typedef struct _pis_array{
        int number;
        int pids[4];
    } pid_array;

    File                  audiofile;
#ifndef ETHERNET_IF
    WiFiClient            client;
    WiFiClientSecure      clientsecure;
    WiFiClient*           _client = nullptr;
#else
    NetworkClient	      client;
    NetworkClientSecure	  clientsecure;
    NetworkClient*       _client = nullptr;
#endif
    SemaphoreHandle_t     mutex_playAudioData;
    SemaphoreHandle_t     mutex_audioTask;
    TaskHandle_t          m_audioTaskHandle = nullptr;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#if ESP_IDF_VERSION_MAJOR == 5
    i2s_chan_handle_t     m_i2s_tx_handle = {};
    i2s_chan_config_t     m_i2s_chan_cfg = {}; // stores I2S channel values
    i2s_std_config_t      m_i2s_std_cfg = {};  // stores I2S driver values
#else
    i2s_config_t          m_i2s_config = {};
    i2s_pin_config_t      m_pin_config = {};
#endif
#pragma GCC diagnostic pop

    std::vector<char*>    m_playlistContent;  // m3u8 playlist buffer
    std::vector<char*>    m_playlistURL;      // m3u8 streamURLs buffer
    std::vector<uint32_t> m_hashQueue;

    const size_t    m_frameSizeWav    = 4096;
    const size_t    m_frameSizeMP3    = 1600;
    const size_t    m_frameSizeAAC    = 1600;
    const size_t    m_frameSizeFLAC   = 4096 * 4;
    const size_t    m_frameSizeOPUS   = 1024;
    const size_t    m_frameSizeVORBIS = 4096 * 2;
    const size_t    m_outbuffSize     = 4096 * 2;

    static const uint8_t m_tsPacketSize  = 188;
    static const uint8_t m_tsHeaderSize  = 4;

    char*           m_ibuff = nullptr;              // used in audio_info()
    char*           m_chbuf = NULL;
    uint16_t        m_chbufSize = 0;                // will set in constructor (depending on PSRAM)
    uint16_t        m_ibuffSize = 0;                // will set in constructor (depending on PSRAM)
    char*           m_lastHost = NULL;              // Store the last URL to a webstream
    char*           m_lastM3U8host = NULL;
    char*           m_playlistBuff = NULL;          // stores playlistdata
    const uint16_t  m_plsBuffEntryLen = 256;        // length of each entry in playlistBuff
    filter_t        m_filter[3];                    // digital filters
    int             m_LFcount = 0;                  // Detection of end of header
    uint32_t        m_sampleRate=16000;
    uint32_t        m_bitRate=0;                    // current bitrate given fom decoder
    uint32_t        m_avr_bitrate = 0;              // average bitrate, median computed by VBR
    int             m_readbytes = 0;                // bytes read
    uint32_t        m_metacount = 0;                // counts down bytes between metadata
    int             m_controlCounter = 0;           // Status within readID3data() and readWaveHeader()
    int8_t          m_balance = 0;                  // -16 (mute left) ... +16 (mute right)
    uint16_t        m_vol = 21;                     // volume
    uint8_t         m_vol_steps = 21;               // default
    double          m_limit_left = 0;               // limiter 0 ... 1, left channel
    double          m_limit_right = 0;              // limiter 0 ... 1, right channel
    uint8_t         m_timeoutCounter = 0;           // timeout counter
    uint8_t         m_curve = 0;                    // volume characteristic
    uint8_t         m_bitsPerSample = 16;           // bitsPerSample
    uint8_t         m_channels = 2;
    uint8_t         m_i2s_num = I2S_NUM_0;          // I2S_NUM_0 or I2S_NUM_1
    uint8_t         m_playlistFormat = 0;           // M3U, PLS, ASX
    uint8_t         m_codec = CODEC_NONE;           //
    uint8_t         m_expectedCodec = CODEC_NONE;   // set in connecttohost (e.g. http://url.mp3 -> CODEC_MP3)
    uint8_t         m_expectedPlsFmt = FORMAT_NONE; // set in connecttohost (e.g. streaming01.m3u) -> FORMAT_M3U)
    uint8_t         m_filterType[2];                // lowpass, highpass
    uint8_t         m_streamType = ST_NONE;
    uint8_t         m_ID3Size = 0;                  // lengt of ID3frame - ID3header
    uint8_t         m_vuLeft = 0;                   // average value of samples, left channel
    uint8_t         m_vuRight = 0;                  // average value of samples, right channel
    uint8_t         m_audioTaskCoreId = 0;
    uint8_t         m_M4A_objectType = 0;           // set in read_M4A_Header
    uint8_t         m_M4A_chConfig = 0;             // set in read_M4A_Header
    uint16_t        m_M4A_sampleRate = 0;           // set in read_M4A_Header
    int16_t*        m_outBuff = NULL;               // Interleaved L/R
    int16_t         m_validSamples = {0};           // #144
    int16_t         m_curSample{0};
    uint16_t        m_dataMode{0};                  // Statemaschine
    int16_t         m_decodeError = 0;              // Stores the return value of the decoder
    uint16_t        m_streamTitleHash = 0;          // remember streamtitle, ignore multiple occurence in metadata
    uint16_t        m_timeout_ms = 250;
    uint16_t        m_timeout_ms_ssl = 2700;
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
    int32_t         m_resumeFilePos = -1;           // the return value from stopSong(), (-1) is idle
    int32_t         m_fileStartPos = -1;            // may be set in connecttoFS()
    uint16_t        m_m3u8_targetDuration = 10;     //
    uint32_t        m_stsz_numEntries = 0;          // num of entries inside stsz atom (uint32_t)
    uint32_t        m_stsz_position = 0;            // pos of stsz atom within file
    uint32_t        m_haveNewFilePos = 0;           // user changed the file position
    uint32_t        m_sumBytesDecoded = 0;          // used for streaming
    uint32_t        m_webFilePos = 0;               // same as audiofile.position() for SD files
    bool            m_f_metadata = false;           // assume stream without metadata
    bool            m_f_unsync = false;             // set within ID3 tag but not used
    bool            m_f_exthdr = false;             // ID3 extended header
    bool            m_f_ssl = false;
    bool            m_f_running = false;
    bool            m_f_firstCall = false;          // InitSequence for processWebstream and processLokalFile
    bool            m_f_firstCurTimeCall = false;   // InitSequence for computeAudioTime
    bool            m_f_firstPlayCall = false;      // InitSequence for playAudioData
    bool            m_f_firstM3U8call = false;      // InitSequence for m3u8 parsing
    bool            m_f_ID3v1TagFound = false;      // ID3v1 tag found
    bool            m_f_chunked = false ;           // Station provides chunked transfer
    bool            m_f_firstmetabyte = false;      // True if first metabyte (counter)
    bool            m_f_playing = false;            // valid mp3 stream recognized
    bool            m_f_tts = false;                // text to speech
    bool            m_f_loop = false;               // Set if audio file should loop
    bool            m_f_forceMono = false;          // if true stereo -> mono
    bool            m_f_internalDAC = false;        // false: output vis I2S, true output via internal DAC
    bool            m_f_rtsp = false;               // set if RTSP is used (m3u8 stream)
    bool            m_f_m3u8data = false;           // used in processM3U8entries
    bool            m_f_Log = false;                // set in platformio.ini  -DAUDIO_LOG and -DCORE_DEBUG_LEVEL=3 or 4
    bool            m_f_continue = false;           // next m3u8 chunk is available
    bool            m_f_ts = true;                  // transport stream
    bool            m_f_m4aID3dataAreRead = false;  // has the m4a-ID3data already been read?
    bool            m_f_psramFound = false;         // set in constructor, result of psramInit()
    bool            m_f_timeout = false;            //
    bool            m_f_commFMT = false;            // false: default (PHILIPS), true: Least Significant Bit Justified (japanese format)
    bool            m_f_audioTaskIsRunning = false;
    bool            m_f_stream = false;             // stream ready for output?
    bool            m_f_decode_ready = false;       // if true data for decode are ready
    bool            m_f_eof = false;                // end of file
    bool            m_f_lockInBuffer = false;       // lock inBuffer for manipulation
    bool            m_f_audioTaskIsDecoding = false;
    bool            m_f_acceptRanges = false;
    uint8_t         m_f_channelEnabled = 3;         // internal DAC, both channels
    uint32_t        m_audioFileDuration = 0;
    float           m_audioCurrentTime = 0;
    uint32_t        m_audioDataStart = 0;           // in bytes
    size_t          m_audioDataSize = 0;            //
    float           m_filterBuff[3][2][2][2];       // IIR filters memory for Audio DSP
    float           m_corr = 1.0;					// correction factor for level adjustment
    size_t          m_i2s_bytesWritten = 0;         // set in i2s_write() but not used
    size_t          m_fileSize = 0;                 // size of the file
    uint16_t        m_filterFrequency[2];
    int8_t          m_gain0 = 0;                    // cut or boost filters (EQ)
    int8_t          m_gain1 = 0;
    int8_t          m_gain2 = 0;

    pid_array       m_pidsOfPMT;
    int16_t         m_pidOfAAC;
    uint8_t         m_packetBuff[m_tsPacketSize];
    int16_t         m_pesDataLength = 0;
};

//----------------------------------------------------------------------------------------------------------------------
