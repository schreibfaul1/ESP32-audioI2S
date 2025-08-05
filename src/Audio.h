/*
 * Audio.h
 *
 */


// #define SR_48K



#pragma once
#pragma GCC optimize ("Ofast")
#include "esp_arduino_version.h"
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
#include <memory>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <driver/i2s_std.h>
#include "psram_unique_ptr.hpp"

#ifndef I2S_GPIO_UNUSED
  #define I2S_GPIO_UNUSED -1 // = I2S_PIN_NO_CHANGE in IDF < 5
#endif

extern __attribute__((weak)) void audio_info(const char*);
extern __attribute__((weak)) void audio_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void audio_id3image(File& file, const size_t pos, const size_t size); //ID3 metadata image
extern __attribute__((weak)) void audio_oggimage(File& file, std::vector<uint32_t> v); //OGG blockpicture
extern __attribute__((weak)) void audio_id3lyrics(const char* text); //ID3 metadata lyrics
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
extern __attribute__((weak)) void audio_process_i2s(int16_t* outBuff, int32_t validSamples, bool *continueI2S); // record audiodata or send via BT

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
    int32_t  getBufsize();
    bool     setBufsize(size_t mbs);            // default is m_buffSizePSRAM for psram, and m_buffSizeRAM without psram
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

protected:
    size_t            m_buffSize         = UINT16_MAX * 10;   // most webstreams limit the advance to 100...300Kbytes
    size_t            m_freeSpace        = 0;
    size_t            m_writeSpace       = 0;
    size_t            m_dataLength       = 0;
    size_t            m_resBuffSize      = 4096 * 6; // reserved buffspace, >= one flac frame
    size_t            m_maxBlockSize     = 1600;
    uint8_t*          m_buffer           = NULL;
    uint8_t*          m_writePtr         = NULL;
    uint8_t*          m_readPtr          = NULL;
    uint8_t*          m_endPtr           = NULL;
    bool              m_f_init           = false;
    bool              m_f_isEmpty        = true;
};
//----------------------------------------------------------------------------------------------------------------------

static const size_t AUDIO_STACK_SIZE = 3300;
static StaticTask_t __attribute__((unused)) xAudioTaskBuffer;
static StackType_t  __attribute__((unused)) xAudioStack[AUDIO_STACK_SIZE];
extern char audioI2SVers[];

class Audio{

    AudioBuffer InBuff; // instance of input buffer

private:
    typedef struct _SYLT{
        bool         seen;
        size_t       size;
        uint32_t     pos;
        char         lang[5];
        uint8_t      text_encoding;
        uint8_t      time_stamp_format;
        uint8_t      content_type;
    } sylt_t;
    typedef struct _ID3Hdr{ // used only in readID3header()
        size_t       id3Size;
        size_t       totalId3Size; // if we have more header, id3_1_size + id3_2_size + ....
        size_t       remainingHeaderBytes;
        size_t       universal_tmp;
        uint8_t      ID3version;
        int          ehsz;
        char         tag[5];
        char         frameid[5];
        char         lang[5];
        size_t       framesize;
        bool         compressed;
        size_t       APIC_size[3];
        uint32_t     APIC_pos[3];
        sylt_t       SYLT;
        uint8_t      numID3Header;
        uint16_t     iBuffSize;
        uint8_t      contentDescriptorTerminator_0;
        uint8_t      contentDescriptorTerminator_1;
        uint8_t      textStringTerminator_0;
        uint8_t      textStringTerminator_1;
        bool         byteOrderMark;
        ps_ptr<char> iBuff;
    } ID3Hdr_t;
    ID3Hdr_t m_ID3Hdr;

    typedef struct _pwsHLS{  // used in processWebStreamHLS()
        uint16_t     maxFrameSize;
        uint16_t     ID3BuffSize;
        uint32_t     availableBytes;
        bool         firstBytes;
        bool         f_chunkFinished;
        uint32_t     byteCounter;
        size_t       chunkSize;
        uint16_t     ID3WritePtr;
        uint16_t     ID3ReadPtr;
        ps_ptr<uint8_t>ID3Buff;
    } pwsHLS_t;
    pwsHLS_t m_pwsHLS;

    typedef struct _pplM3U8{ // used in parsePlaylist_M3U8
        uint64_t     xMedSeq;
        bool         f_mediaSeq_found;
    } pplM3u8_t;
    pplM3u8_t m_pplM3U8;

    typedef struct _m4aHdr{ // used in read_M4A_Header
        size_t      headerSize;
        size_t      retvalue;
        size_t      atomsize;
        size_t      sizeof_ftyp;
        size_t      sizeof_moov;
        size_t      sizeof_free;
        size_t      sizeof_mdat;
        size_t      sizeof_trak;
        size_t      sizeof_ilst;
        size_t      sizeof_esds;
        size_t      sizeof_mdia;
        size_t      sizeof_minf;
        size_t      sizeof_stbl;
        size_t      sizeof_stsd;
        size_t      sizeof_stsz;
        size_t      sizeof_mp4a;
        size_t      sizeof_udta;
        size_t      sizeof_meta;
        size_t      audioDataPos;
        size_t      cnt;
        size_t      offset;
        uint32_t    picPos;
        uint32_t    picLen;
        uint32_t    ilst_pos;
        uint8_t     channel_count;
        uint8_t     sample_size; // bps
        uint16_t    sample_rate;
        uint8_t     aac_profile;
        uint32_t    stsz_num_entries;
        uint32_t    stsz_table_pos;
        bool        progressive; // Progressive (moov before mdat)
        bool        version_flags;
    } m4aHdr_t;
    m4aHdr_t m_m4aHdr;

    typedef struct _plCh{ // used in playChunk
        int32_t     validSamples;
        int32_t     samples48K = 0;
        uint32_t    count = 0;
        size_t      i2s_bytesConsumed;
        int16_t*    sample[2];
        int16_t*    s2;
        int         sampleSize;
        esp_err_t   err;
        int         i;
    } plCh_t;
    plCh_t m_plCh;

    typedef struct _lVar{ // used in loop
        uint8_t     no_host_cnt;
        uint32_t    no_host_timer;
        uint8_t     count;
    } lVar_t;
    lVar_t m_lVar;

    typedef struct _hwoe{ // used in dismantle_host
        bool ssl;
        ps_ptr<char> hwoe;  // host without extension
        uint16_t     port;
        ps_ptr<char> extension;
        ps_ptr<char> query_string;
    } hwoe_t;

    typedef struct _prlf{ // used in processLocalFile
        uint32_t  ctime;
        int32_t   newFilePos;
        bool      audioHeaderFound;
        uint32_t  timeout;
        uint32_t  maxFrameSize;
        uint32_t  availableBytes;
        int32_t   bytesAddedToBuffer;
    } prlf_t;
    prlf_t m_prlf;

    typedef struct _cat { // used in computeAudioTime
        uint64_t sumBytesIn;
        uint64_t sumBytesOut;
        uint32_t sumBitRate;
        uint32_t counter;
        uint32_t timeStamp;
        uint32_t deltaBytesIn;
        uint32_t nominalBitRate;
        uint16_t syltIdx;
    } cat_t;
    cat_t m_cat;

    typedef struct _cVUl{ // used in computeVUlevel
        uint8_t sampleArray[2][4][8] = {0};
        uint8_t cnt0 = 0, cnt1 = 0, cnt2 = 0, cnt3 = 0, cnt4 = 0;
        bool    f_vu = false;
    } cVUl_t;
    cVUl_t m_cVUl;

    typedef struct _ifCh{ // used in IIR_filterChain0, 1, 2
        float   inSample0[2];
        float   outSample0[2];
        int16_t iir_out0[2];
        float   inSample1[2];
        float   outSample1[2];
        int16_t iir_out1[2];
        float   inSample2[2];
        float   outSample2[2];
        int16_t iir_out2[2];
    } ifCh_t;
    ifCh_t m_ifCh;

    typedef struct _tspp{ // used in ts_parsePacket
        int pidNumber = 0;
        int pids[4]; // PID_ARRAY_LEN
        int PES_DataLength = 0;
        int pidOfAAC = 0;
        uint8_t fillData = 0;
    } tspp_t;
    tspp_t m_tspp;

    typedef struct _pwst{ // used in processWebStream
        uint16_t  maxFrameSize;
        uint32_t  chunkSize =0;
        bool      f_skipCRLF = false;
        uint32_t  availableBytes;
        bool      f_clientIsConnected;
    } pwst_t;
    pwst_t m_pwst;

    typedef struct _pwf{ // used in processWebFile
        uint32_t  maxFrameSize;
        int32_t   newFilePos;
        bool      audioHeaderFound;
        uint32_t  chunkSize;
        size_t    audioDataCount;
        uint32_t  byteCounter;
        uint32_t  nextChunkCount;
        bool      f_waitingForPayload = false;
        bool      f_clientIsConnected;
        uint32_t  ctime;
        uint32_t  timeout;
        uint32_t  availableBytes;
        int32_t   bytesAddedToBuffer;
    } pwf_t;
    pwf_t m_pwf;

    typedef struct  _pad{ // used in playAudioData
        uint8_t  count = 0;
        size_t   oldAudioDataSize = 0;
        bool     lastFrames = false;
        int32_t  bytesToDecode;
        int16_t  bytesDecoded;
    } pad_t;
    pad_t m_pad;

    typedef struct _sbyt{ // used in sendBytes
        int32_t  bytesLeft;
        bool     f_setDecodeParamsOnce = true;
        uint8_t  channels = 0;
        int      nextSync = 0;
        uint8_t  isPS = 0;
    } sbyt_t;
    sbyt_t m_sbyt;

    typedef struct _rmet{ // used in readMetadata
        uint16_t pos_ml = 0; // determines the current position in metaline
        uint16_t metalen = 0;
        uint16_t res = 0;
    } rmet_t;
    rmet_t m_rmet;

    typedef struct _pwsts {                // used in processWebStreamTS
        uint32_t       availableBytes;     // available bytes in stream
        bool           f_firstPacket;
        bool           f_chunkFinished;
        bool           f_nextRound;
        uint32_t       byteCounter;        // count received data
        uint8_t        ts_packetStart = 0;
        uint8_t        ts_packetLength = 0;
        uint8_t        ts_packetPtr = 0;
        const uint8_t  ts_packetsize = 188;
        ps_ptr<uint8_t> ts_packet;
        size_t         chunkSize = 0;
    } pwsts_t;
    pwsts_t m_pwsst;

    typedef struct _rwh { // used in read_WAV_Header
        size_t   headerSize;
        uint32_t cs = 0;
        uint8_t  bts = 0;
    } rwh_t;
    rwh_t m_rwh;

    typedef struct _rflh { // used in read_FLAC_Header
        size_t   headerSize;
        size_t   retvalue = 0;
        bool     f_lastMetaBlock = false;
        uint32_t picPos = 0;
        uint32_t picLen = 0;
    } rflh_t;
    rflh_t m_rflh;

    typedef struct _phreh{ // used in parseHttpResponseHeader
        uint32_t ctime;
        uint32_t timeout;
        uint32_t stime;
        bool     f_time = false;
    } phreh_t;
    phreh_t m_phreh;

    typedef struct _phrah{ // used in parseHttpRangeHeader
        uint32_t ctime;
        uint32_t timeout;
        uint32_t stime;
        bool     f_time = false;
    } phrah_t;
    phrah_t m_phrah;

    typedef struct _sdet{ // used in streamDetection
        uint32_t tmr_slow = 0;
        uint32_t tmr_lost = 0;
        uint8_t  cnt_slow = 0;
        uint8_t  cnt_lost = 0;
    }sdet_t;
    sdet_t m_sdet;

    typedef struct _fnsy{ // used in findNextSync
        int      nextSync = 0;
        uint32_t swnf = 0;
    } fnsy_t;
    fnsy_t m_fnsy;

  public:
    Audio(uint8_t i2sPort = I2S_NUM_0);
    ~Audio();
    bool         openai_speech(const String& api_key, const String& model, const String& input, const String& instructions, const String& voice, const String& response_format, const String& speed);
    hwoe_t       dismantle_host(const char* host);
    bool         connecttohost(const char* host, const char* user = "", const char* pwd = "");
    bool         connecttospeech(const char* speech, const char* lang);
    bool         connecttoFS(fs::FS& fs, const char* path, int32_t m_fileStartPos = -1);
    void         setConnectionTimeout(uint16_t timeout_ms, uint16_t timeout_ms_ssl);
    bool         setAudioPlayPosition(uint16_t sec);
    bool         setFilePos(uint32_t pos);
    bool         setTimeOffset(int sec);
    bool         setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t MCLK = I2S_GPIO_UNUSED);
    bool         pauseResume();
    bool         isRunning() { return m_f_running; }
    void         loop();
    uint32_t     stopSong();
    void         forceMono(bool m);
    void         setBalance(int8_t bal = 0);
    void         setVolumeSteps(uint8_t steps);
    void         setVolume(uint8_t vol, uint8_t curve = 0);
    uint8_t      getVolume();
    uint8_t      maxVolume();
    uint8_t      getI2sPort();
    uint32_t     getFileSize();
    uint32_t     getSampleRate();
    uint8_t      getBitsPerSample();
    uint8_t      getChannels();
    uint32_t     getBitRate(bool avg = false);
    uint32_t     getAudioFileDuration();
    uint32_t     getAudioCurrentTime();
    uint32_t     getTotalPlayingTime();
    uint16_t     getVUlevel();
    uint32_t     inBufferFilled();            // returns the number of stored bytes in the inputbuffer
    uint32_t     inBufferFree();              // returns the number of free bytes in the inputbuffer
    uint32_t     getInBufferSize();           // returns the size of the inputbuffer in bytes
    bool         setInBufferSize(size_t mbs); // sets the size of the inputbuffer in bytes
    void         setTone(int8_t gainLowPass, int8_t gainBandPass, int8_t gainHighPass);
    void         setI2SCommFMT_LSB(bool commFMT);
    int          getCodec() { return m_codec; }
    const char*  getCodecname() { return codecname[m_codec]; }
    const char*  getVersion() { return audioI2SVers; }

  private:
    // ------- PRIVATE MEMBERS ----------------------------------------

    void         latinToUTF8(ps_ptr<char>& buff, bool UTF8check = true);
    void         htmlToUTF8(char* str);
    void         setDefaults(); // free buffers and set defaults
    int32_t      audioFileRead(uint8_t* buff = nullptr, size_t len = 0);
    int32_t      audioFileSeek(uint32_t position, size_t len = 0);
    void         initInBuff();
    bool         httpPrint(const char* host);
    bool         httpRange(uint32_t range, uint32_t length = UINT32_MAX);
    void         processLocalFile();
    void         processWebStream();
    void         processWebFile();
    void         processWebStreamTS();
    void         processWebStreamHLS();
    void         playAudioData();
    bool         readPlayListData();
    const char*  parsePlaylist_M3U();
    const char*  parsePlaylist_PLS();
    const char*  parsePlaylist_ASX();
    ps_ptr<char> parsePlaylist_M3U8();
    ps_ptr<char> m3u8redirection(uint8_t* codec);
    bool         m3u8_findMediaSeqInURL(std::vector<ps_ptr<char>>& linesWithSeqNrAndURL, uint64_t* mediaSeqNr);
    void         showCodecParams();
    int          findNextSync(uint8_t* data, size_t len);
    uint32_t     decodeError(int8_t res, uint8_t* data, int32_t bytesDecoded);
    uint32_t     decodeContinue(int8_t res, uint8_t* data, int32_t bytesDecoded);
    int          sendBytes(uint8_t* data, size_t len);
    void         setDecoderItems();
    void         computeAudioTime(uint16_t bytesDecoderIn, uint16_t bytesDecoderOut);
    void         showID3Tag(const char* tag, const char* val);
    size_t       readAudioHeader(uint32_t bytes);
    int          read_WAV_Header(uint8_t* data, size_t len);
    int          read_FLAC_Header(uint8_t* data, size_t len);
    int          read_ID3_Header(uint8_t* data, size_t len);
    int          read_M4A_Header(uint8_t* data, size_t len);
    size_t       process_m3u8_ID3_Header(uint8_t* packet);
    bool         setSampleRate(uint32_t hz);
    bool         setBitsPerSample(int bits);
    bool         setChannels(int channels);
    bool         setBitrate(int br);
    size_t       resampleTo48kStereo(const int16_t* input, size_t inputFrames);
    void         playChunk();
    void         computeVUlevel(int16_t sample[2]);
    void         computeLimit();
    void         Gain(int16_t* sample);
    void         showstreamtitle(char* ml);
    bool         parseContentType(char* ct);
    bool         parseHttpResponseHeader();
    bool         parseHttpRangeHeader();
    bool         initializeDecoder(uint8_t codec);
    esp_err_t    I2Sstart();
    esp_err_t    I2Sstop();
    void         zeroI2Sbuff();
    void         IIR_filterChain0(int16_t iir_in[2], bool clear = false);
    void         IIR_filterChain1(int16_t iir_in[2], bool clear = false);
    void         IIR_filterChain2(int16_t iir_in[2], bool clear = false);
    uint32_t     streamavail() { return m_client ? m_client->available() : 0; }
    void         IIR_calculateCoefficients(int8_t G1, int8_t G2, int8_t G3);
    bool         ts_parsePacket(uint8_t* packet, uint8_t* packetStart, uint8_t* packetLength);

    //+++ create a T A S K  for playAudioData(), output via I2S +++
  public:
    void         setAudioTaskCore(uint8_t coreID);
    uint32_t     getHighWatermark();

  private:
    void         startAudioTask(); // starts a task for decode and play
    void         stopAudioTask();  // stops task for audio
    static void  taskWrapper(void* param);
    void         audioTask();
    void         performAudioTask();

    //+++ H E L P   F U N C T I O N S +++
    uint16_t     readMetadata(uint16_t b, bool first = false);
    size_t       readChunkSize(uint8_t* bytes);
    bool         readID3V1Tag();
    int32_t      newInBuffStart(int32_t m_resumeFilePos);
    boolean      streamDetection(uint32_t bytesAvail);
    uint32_t     m4a_correctResumeFilePos();
    uint32_t     ogg_correctResumeFilePos();
    int32_t      flac_correctResumeFilePos();
    int32_t      mp3_correctResumeFilePos();
    uint8_t      determineOggCodec(uint8_t* data, uint16_t len);

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
    //fbi
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

    int find_utf16_null_terminator(const uint8_t* buf, int start, int max) {
        for (int i = start; i + 1 < max; i += 2) {
            if (buf[i] == 0x00 && buf[i + 1] == 0x00)
                return i; // Index to the first zero-byte
        }
        return -1; // not found
    }

    int32_t min3(int32_t a, int32_t b, int32_t c){
        uint32_t min_val = a;
        if (b < min_val) min_val = b;
        if (c < min_val) min_val = c;
        return min_val;
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
    void vector_clear_and_shrink(std::vector<ps_ptr<char>>& vec){
        for(int i = 0; i< vec.size(); i++) vec[i].reset();
        vec.clear();            // unique_ptr takes care of free()
        vec.shrink_to_fit();    // put back memory
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
    ps_ptr<char> urlencode(const char* str, bool spacesOnly) {
        if (!str) {return {};}  // Enter is zero

        // Reserve memory for the result (3x the length of the input string, worst-case)
        size_t inputLength = strlen(str);
        size_t bufferSize = inputLength * 3 + 1; // Worst-case-Szenario
        ps_ptr<char>encoded;
        encoded.alloc(bufferSize);
        if (!encoded.valid()) {return {}; } // memory allocation failed

        const char *p_input = str;  // Copy of the input pointer
        char *p_encoded = encoded.get();  // pointer of the output buffer
        size_t remainingSpace = bufferSize; // remaining space in the output buffer

        while (*p_input) {
            if (isalnum((unsigned char)*p_input)) {
                // adopt alphanumeric characters directly
                if (remainingSpace > 1) {
                    *p_encoded++ = *p_input;
                    remainingSpace--;
                } else {
                    return {}; // security check failed
                }
            } else if (spacesOnly && *p_input != 0x20) {
                // Nur Leerzeichen nicht kodieren
                if (remainingSpace > 1) {
                    *p_encoded++ = *p_input;
                    remainingSpace--;
                } else {
                    return {}; // security check failed
                }
            } else {
                // encode unsafe characters as '%XX'
                if (remainingSpace > 3) {
                    int written = snprintf(p_encoded, remainingSpace, "%%%02X", (unsigned char)*p_input);
                    if (written < 0 || written >= (int)remainingSpace) {
                        return {}; // error writing to buffer
                    }
                    p_encoded += written;
                    remainingSpace -= written;
                } else {
                    return {}; // security check failed
                }
            }
            p_input++;
        }

        // Null-terminieren
        if (remainingSpace > 0) {
            *p_encoded = '\0';
        } else {
            return {}; // security check failed
        }
        encoded.shrink_to_fit();
        return encoded;
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
    enum : int { FORMAT_NONE = 0, FORMAT_M3U = 1, FORMAT_PLS = 2, FORMAT_ASX = 3, FORMAT_M3U8 = 4}; // playlist formats
    enum : int { AUDIO_NONE, HTTP_RESPONSE_HEADER, HTTP_RANGE_HEADER, AUDIO_DATA, AUDIO_LOCALFILE,
                 AUDIO_PLAYLISTINIT, AUDIO_PLAYLISTHEADER,  AUDIO_PLAYLISTDATA};
    enum : int { FLAC_BEGIN = 0, FLAC_MAGIC = 1, FLAC_MBH =2, FLAC_SINFO = 3, FLAC_PADDING = 4, FLAC_APP = 5,
                 FLAC_SEEK = 6, FLAC_VORBIS = 7, FLAC_CUESHEET = 8, FLAC_PICTURE = 9, FLAC_OKAY = 100};
    enum : int { M4A_BEGIN = 0, M4A_FTYP = 1, M4A_CHK = 2, M4A_MOOV = 3, M4A_FREE = 4, M4A_TRAK = 5, M4A_MDAT = 6,
                 M4A_ILST = 7, M4A_MP4A = 8, M4A_ESDS = 9, M4A_MDIA = 10, M4A_MINF = 11, M4A_STBL = 12, M4A_STSD = 13, M4A_UDTA = 14,
                 M4A_STSZ = 15, M4A_META = 16,  M4A_AMRDY = 99, M4A_OKAY = 100};
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

    File                  m_audiofile;
#ifndef ETHERNET_IF
    WiFiClient            client;
    WiFiClientSecure      clientsecure;
    WiFiClient*           m_client = nullptr;
#else
    NetworkClient	      client;
    NetworkClientSecure	  clientsecure;
    NetworkClient*       m_client = nullptr;
#endif
    SemaphoreHandle_t     mutex_playAudioData;
    SemaphoreHandle_t     mutex_audioTask;
    TaskHandle_t          m_audioTaskHandle = nullptr;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

    i2s_chan_handle_t     m_i2s_tx_handle = {};
    i2s_chan_config_t     m_i2s_chan_cfg = {}; // stores I2S channel values
    i2s_std_config_t      m_i2s_std_cfg = {};  // stores I2S driver values

#pragma GCC diagnostic pop

    std::vector<ps_ptr<char>> m_playlistContent;        // m3u8 playlist buffer from responseHeader
    std::vector<ps_ptr<char>> m_playlistURL;            // m3u8 streamURLs buffer
    std::vector<ps_ptr<char>> m_linesWithSeqNrAndURL;   // extract from m_playlistContent, contains URL and MediaSequenceNumber
    std::vector<ps_ptr<char>> m_linesWithEXTINF;        // extract from m_playlistContent, contains length and metadata
    std::vector<ps_ptr<char>> m_syltLines;              // SYLT line table
    std::vector<uint32_t>     m_syltTimeStamp;          // SYLT time table
    std::vector<uint32_t>     m_hashQueue;

    const size_t    m_frameSizeWav       = 4096;
    const size_t    m_frameSizeMP3       = 1600 * 2;
    const size_t    m_frameSizeAAC       = 1600;
    const size_t    m_frameSizeFLAC      = 4096 * 6; // 24576
    const size_t    m_frameSizeOPUS      = 2048;
    const size_t    m_frameSizeVORBIS    = 4096 * 2;
    const size_t    m_outbuffSize        = 4096 * 2;
    const size_t    m_samplesBuff48KSize = m_outbuffSize * 8; // 131072KB  SRmin: 6KHz -> SRmax: 48K

    static const uint8_t m_tsPacketSize  = 188;
    static const uint8_t m_tsHeaderSize  = 4;

    ps_ptr<int16_t>  m_outBuff;        // Interleaved L/R
    ps_ptr<int16_t>  m_samplesBuff48K; // Interleaved L/R
    ps_ptr<char>     m_ibuff;          // used in log_info()
    ps_ptr<char>     m_lastHost;       // Store the last URL to a webstream
    ps_ptr<char>     m_currentHost;    // can be changed by redirection or playlist
    ps_ptr<char>     m_lastM3U8host;
    ps_ptr<char>     m_speechtxt;      // stores tts text
    ps_ptr<char>     m_streamTitle;    // stores the last StreamTitle


    filter_t        m_filter[3];                    // digital filters
    const uint16_t  m_plsBuffEntryLen = 256;        // length of each entry in playlistBuff
    int             m_LFcount = 0;                  // Detection of end of header
    uint32_t        m_sampleRate=48000;
    uint32_t        m_bitRate=0;                    // current bitrate given fom decoder
    uint32_t        m_avr_bitrate = 0;              // average bitrate, median computed by VBR
    uint32_t        m_audioFilePosition = 0;        // current position, counts every readed byte
    uint32_t        m_audioFileSize = 0;            // local and web files
    int             m_readbytes = 0;                // bytes read
    uint32_t        m_metacount = 0;                // counts down bytes between metadata
    int             m_controlCounter = 0;           // Status within readID3data() and readWaveHeader()
    int8_t          m_balance = 0;                  // -16 (mute left) ... +16 (mute right)
    uint16_t        m_vol = 21;                     // volume
    uint16_t        m_vol_steps = 21;               // default
    int16_t         m_inputHistory[6] = {0};        // used in resampleTo48kStereo()
    uint16_t        m_opus_mode = 0;                // celt_only, silk_only or hybrid
    double          m_limit_left = 0;               // limiter 0 ... 1, left channel
    double          m_limit_right = 0;              // limiter 0 ... 1, right channel
    uint8_t         m_timeoutCounter = 0;           // timeout counter
    uint8_t         m_curve = 0;                    // volume characteristic
    uint8_t         m_bitsPerSample = 16;           // bitsPerSample
    uint8_t         m_channels = 2;
    uint8_t         m_i2s_num = I2S_NUM_0;          // I2S_NUM_0 or I2S_NUM_1
    uint8_t         m_playlistFormat = 0;           // M3U, PLS, ASX
    uint8_t         m_codec = CODEC_NONE;           //
    uint8_t         m_m3u8Codec = CODEC_AAC;        // codec of m3u8 stream
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
    int16_t         m_validSamples = {0};           // #144
    int16_t         m_curSample{0};
    uint16_t        m_dataMode{0};                  // Statemaschine
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
    uint32_t        m_bytesNotConsumed = 0;          // pictures or something else that comes with the stream
    uint32_t        m_PlayingStartTime = 0;         // Stores the milliseconds after the start of the audio
    int32_t         m_resumeFilePos = -1;           // the return value from stopSong(), (-1) is idle
    int32_t         m_fileStartPos = -1;            // may be set in connecttoFS()
    uint16_t        m_m3u8_targetDuration = 10;     //
    uint32_t        m_stsz_numEntries = 0;          // num of entries inside stsz atom (uint32_t)
    uint32_t        m_stsz_position = 0;            // pos of stsz atom within file
    uint32_t        m_haveNewFilePos = 0;           // user changed the file position
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
    bool            m_f_ogg = false;                // OGG stream
    bool            m_f_forceMono = false;          // if true stereo -> mono
    bool            m_f_rtsp = false;               // set if RTSP is used (m3u8 stream)
    bool            m_f_m3u8data = false;           // used in processM3U8entries
    bool            m_f_continue = false;           // next m3u8 chunk is available
    bool            m_f_ts = true;                  // transport stream
    bool            m_f_m4aID3dataAreRead = false;  // has the m4a-ID3data already been read?
    bool            m_f_psramFound = false;         // set in constructor, result of psramInit()
    bool            m_f_timeout = false;            //
    bool            m_f_commFMT = false;            // false: default (PHILIPS), true: Least Significant Bit Justified (japanese format)
    bool            m_f_audioTaskIsRunning = false;
    bool            m_f_allDataReceived = false;
    bool            m_f_stream = false;             // stream ready for output?
    bool            m_f_decode_ready = false;       // if true data for decode are ready
    bool            m_f_eof = false;                // end of file
    bool            m_f_lockInBuffer = false;       // lock inBuffer for manipulation
    bool            m_f_audioTaskIsDecoding = false;
    bool            m_f_acceptRanges = false;
    bool            m_f_reset_m3u8Codec = true;     // reset codec for m3u8 stream
    bool            m_f_connectionClose = false;    // set in parseHttpResponseHeader
    uint32_t        m_audioFileDuration = 0;
    float           m_audioCurrentTime = 0;
    float           m_resampleError = 0.0f;
    float           m_resampleRatio = 1.0f;         // resample ratio for e.g. 44.1kHz to 48kHz
    float           m_resampleCursor = 0.0f;        // next frac in resampleTo48kStereo



    uint32_t        m_audioDataStart = 0;           // in bytes
    size_t          m_audioDataSize = 0;            //
    size_t          m_ibuffSize = 0;                // log buffer size for audio_info()
    float           m_filterBuff[3][2][2][2];       // IIR filters memory for Audio DSP
    float           m_corr = 1.0;					// correction factor for level adjustment
    size_t          m_i2s_bytesWritten = 0;         // set in i2s_write() but not used
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
