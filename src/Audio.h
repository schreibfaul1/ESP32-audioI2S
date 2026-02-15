/*
 * Audio.h
 *
 */

#define DMA_DESC_NUM  32  // number of I2S DMA buffer
#define DMA_FRAME_NUM 222 // number of frames in one DMA buffer
// #define SR_48K         // only for I2S devices (DAC, BT etc.) that require 48KHz

#pragma once
#pragma GCC optimize("Ofast")
#include "audiolib_structs.hpp"
#include "esp_arduino_version.h"
#include "esp_dsp.h"
#include "psram_unique_ptr.hpp"
#include <Arduino.h>
#include <FFat.h>
#include <FS.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <SD.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <atomic>
#include <charconv>
#include <codecvt>
#include <deque>
#include <driver/i2s_std.h>
#include <esp32-hal-log.h>
#include <functional>
#include <libb64/cencode.h>
#include <locale>
#include <memory>
#include <span>
#include <vector>

#ifndef I2S_GPIO_UNUSED
    #define I2S_GPIO_UNUSED -1 // = I2S_PIN_NO_CHANGE in IDF < 5
#endif

extern __attribute__((weak)) void audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S); // record audiodata or send via BT
extern char                       audioI2SVers[];
class Decoder; // prototype

// Audio event type descriptions
static constexpr std::array<const char*, 13> eventStr = {"info",    "id3data",  "eof",      "station_name", "icy_description", "streamtitle", "bitrate",
                                                         "icy_url", "icy_logo", "lasthost", "cover_image",  "lyrics",          "log"};
//----------------------------------------------------------------------------------------------------------------------
class AudioBuffer {

  public:
    AudioBuffer();   // constructor
    ~AudioBuffer();  // frees the buffer
    size_t   init(); // set default values
    bool     isInitialized() { return m_init; };
    size_t   getBufsize();
    size_t   getMaxBlockSize(); // returns maxBlockSize
    void     setMaxBlocksize(uint32_t mbs);
    size_t   freeSpace();             // number of free bytes to overwrite
    size_t   writeSpace();            // space fom writepointer to bufferend
    size_t   bufferFilled();          // returns the number of filled bytes
    size_t   readSpace();             // max readable bytes in one block
    void     bytesWritten(size_t bw); // update writepointer
    void     bytesWasRead(size_t br); // update readpointer
    uint8_t* getWritePtr();           // returns the current writepointer
    uint8_t* getReadPtr();            // returns the current readpointer
    void     reset();                 // restore defaults
    void     showStatus();

  protected:
    size_t          m_mainBuffSize = 0; // most webstreams limit the advance to 100...300Kbytes
    size_t          m_freeSpace = 0;
    size_t          m_writeSpace = 0;
    size_t          m_resBuffSize = 0;
    size_t          m_maxBlockSize = 0;
    size_t          m_readSpace = 0;
    const size_t    m_maxRet = UINT16_MAX;
    ps_ptr<uint8_t> m_buffer;
    uint8_t*        m_buffEnd = nullptr;
    uint8_t*        m_writePtr = nullptr;
    uint8_t*        m_readPtr = nullptr;
    uint8_t*        m_endPtr = nullptr;
    uint8_t*        m_startPtr = nullptr;
    bool            m_init = false;
    bool            m_isEmpty = true;
    bool            m_isFull = false;

  private:
    SemaphoreHandle_t m_mutex = nullptr;
};
//----------------------------------------------------------------------------------------------------------------------

class Audio {
  private:
    AudioBuffer InBuff;    // instance of input buffer
    uint8_t     m_i2s_num; // I2S_NUM_0 or I2S_NUM_1

  public:
    Audio(uint8_t i2sPort = I2S_NUM_0);
    ~Audio();
    std::mutex mutex_info; // mutex_info as member

    // callbacks ---------------------------------------------------------
    typedef enum { evt_info = 0, evt_id3data, evt_eof, evt_name, evt_icydescription, evt_streamtitle, evt_bitrate, evt_icyurl, evt_icylogo, evt_lasthost, evt_image, evt_lyrics, evt_log } event_t;
    typedef struct _msg { // used in info(audio_info_callback());
        const char*           msg = nullptr;
        const char*           s = nullptr;
        event_t               e = (event_t)0; // event type
        uint8_t               i2s_num = 0;
        int32_t               arg1 = 0;
        int32_t               arg2 = 0;
        std::vector<uint32_t> vec = {}; // apic [pos, len, pos, len, pos, len, ....]
    } msg_t;
    inline static std::function<void(msg_t i)> audio_info_callback;
    // -------------------------------------------------------------------

    bool openai_speech(const String& api_key, const String& model, const String& input, const String& instructions, const String& voice, const String& response_format, const String& speed);
    audiolib::hwoe_t dismantle_host(const char* host);
    bool             connecttohost(const char* host, const char* user = "", const char* pwd = "");
    bool             connecttospeech(const char* speech, const char* lang);
    bool             connecttoFS(fs::FS& fs, const char* path, int32_t fileStartTime = -1);
    void             setConnectionTimeout(uint16_t timeout_ms, uint16_t timeout_ms_ssl);
    bool             setAudioPlayTime(uint16_t sec);
    bool             setTimeOffset(int sec);
    bool             setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t MCLK = I2S_GPIO_UNUSED);
    bool             pauseResume();
    bool             isRunning() { return m_f_running; }
    void             loop();
    uint32_t         stopSong();
    void             forceMono(bool m);
    void             setBalance(float balance = 0.0f);
    void             setVolumeSteps(uint8_t steps);
    uint8_t          getVolumeSteps();
    void             setVolume(uint8_t vol, uint8_t curve = 0);
    uint8_t          getVolume();
    void             setMute(bool mute);
    bool             getMute();
    uint8_t          getI2sPort();
    uint32_t         getFileSize();
    uint32_t         getSampleRate();
    uint8_t          getBitsPerSample();
    uint8_t          getChannels();
    uint32_t         getBitRate();
    uint32_t         getAudioFileDuration();
    uint32_t         getAudioCurrentTime();
    uint32_t         getAudioFilePosition();
    bool             setAudioFilePosition(uint32_t pos);
    uint16_t         getVUlevel();
    uint32_t         inBufferFilled();  // returns the number of stored bytes in the inputbuffer
    uint32_t         inBufferFree();    // returns the number of free bytes in the inputbuffer
    uint32_t         getInBufferSize(); // returns the size of the inputbuffer in bytes
    void             inBufferStatus() { InBuff.showStatus(); }
    void             setTone(float gainLowPass, float gainBandPass, float gainHighPass);
    void             setI2SCommFMT_LSB(bool commFMT);
    int              getCodec() { return m_codec; }
    const char*      getCodecname() { return codecname[m_codec]; }
    const char*      getVersion() { return audioI2SVers; }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

  private:
    // ------- PRIVATE MEMBERS ----------------------------------------
    std::unique_ptr<Decoder> createDecoder(const std::string& type);
    void                     destroy_decoder();
    bool                     fsRange(uint32_t range);
    void                     latinToUTF8(ps_ptr<char>& buff, bool UTF8check = true);
    void                     htmlToUTF8(char* str);
    void                     setDefaults(); // free buffers and set defaults
    int32_t                  audioFileRead(uint8_t* buff = nullptr, size_t len = 0);
    int32_t                  audioFileSeek(uint32_t position, size_t len = 0);
    void                     initInBuff();
    bool                     httpPrint(const char* host);
    bool                     httpRange(uint32_t range, uint32_t length = UINT32_MAX);
    void                     processLocalFile();
    void                     processWebStream();
    void                     processWebFile();
    void                     processWebStreamTS();
    void                     processWebStreamHLS();
    void                     playAudioData();
    bool                     readPlayListData();
    const char*              parsePlaylist_M3U();
    const char*              parsePlaylist_PLS();
    const char*              parsePlaylist_ASX();
    ps_ptr<char>             parsePlaylist_M3U8();
    uint16_t                 accomplish_m3u8_url();
    int16_t                  prepare_first_m3u8_url(ps_ptr<char>& playlistBuff);
    ps_ptr<char>             m3u8redirection(uint8_t* codec);
    void                     showCodecParams();
    int                      findNextSync(uint8_t* data, size_t len);
    uint32_t                 decodeError(int8_t res, uint8_t* data, int32_t bytesDecoded);
    uint32_t                 decodeContinue(int8_t res, uint8_t* data, int32_t bytesDecoded, int32_t* bytesLeft);
    int                      sendBytes(uint8_t* data, size_t len);
    void                     setDecoderItems();
    void                     calculateAudioTime(uint16_t bytesDecoderIn, uint16_t bytesDecoderOut);
    void                     showID3Tag(const char* tag, const char* val);
    size_t                   readAudioHeader(uint32_t bytes);
    int                      read_WAV_Header(uint8_t* data, size_t len);
    int                      read_FLAC_Header(uint8_t* data, size_t len);
    int                      read_ID3_Header(uint8_t* data, size_t len);
    int                      read_M4A_Header(uint8_t* data, size_t len);
    size_t                   process_m3u8_ID3_Header(uint8_t* packet);
    bool                     setSampleRate(uint32_t hz);
    bool                     setBitsPerSample(int bits);
    bool                     setChannels(int channels);
    size_t                   resampleTo48kStereo(const int32_t* input, size_t inputFrames);
    void                     playChunk();
    void                     calculateVUlevel(int32_t* sample);
    void                     processSpectrum(int32_t* sample);
    void                     updateSpectrum();
    void                     gain_ramp();
    void                     calculateVolumeLimits();
    void                     Gain(int32_t* sample);
    void                     showstreamtitle(char* ml);
    bool                     parseContentType(char* ct);
    bool                     parseHttpResponseHeader();
    bool                     parseHttpRangeHeader();
    bool                     initializeDecoder();
    esp_err_t                I2Sstart();
    esp_err_t                I2Sstop();
    void                     zeroI2Sbuff();
    void                     reconfigI2S();
    void                     stereo2mono(int32_t* buff, uint16_t validSamples);
    void                     IIR_calculateCoefficients();
    void                     IIR_filter(int32_t* iir_in);
    uint32_t                 streamavail() { return m_client ? m_client->available() : 0; }
    bool                     ts_parsePacket(uint8_t* packet, uint8_t* packetStart, uint8_t* packetLength);
    uint64_t                 getLastGranulePosition();

    //+++ create a T A S K  for playAudioData(), output via I2S +++
  public:
    void     setAudioTaskCore(uint8_t coreID);
    uint32_t getHighWatermark();

  private:
    void        startAudioTask(); // starts a task for decode and play
    void        stopAudioTask();  // stops task for audio
    static void audioTaskWrapper(void* param);
    void        audioTask();
    void        performAudioTask();

    //+++ H E L P   F U N C T I O N S +++
    bool         readMetadata(uint16_t b, uint16_t* readedBytes, bool first = false);
    int32_t      getChunkSize(uint16_t* readedBytes, bool first = false);
    bool         readID3V1Tag();
    int32_t      newInBuffStart(int32_t resumeFilePos);
    boolean      streamDetection(uint32_t bytesAvail);
    uint32_t     m4a_correctResumeFilePos();
    uint32_t     ogg_correctResumeFilePos();
    int32_t      flac_correctResumeFilePos();
    int32_t      mp3_correctResumeFilePos();
    int32_t      wav_correctResumeFilePos();
    uint8_t      determineOggCodec();
    void         strlower(char* str);
    void         trim(char* str);
    bool         startsWith(const char* base, const char* str);
    bool         endsWith(const char* base, const char* searchString);
    int          indexOf(const char* base, const char* str, int startIndex = 0);
    int          indexOf(const char* base, char ch, int startIndex = 0);
    int          lastIndexOf(const char* haystack, const char* needle);
    int          lastIndexOf(const char* haystack, const char needle);
    int          specialIndexOf(uint8_t* base, const char* str, int baselen, bool exact = false);
    int          specialIndexOfLast(uint8_t* base, const char* str, int baselen);
    int          find_utf16_null_terminator(const uint8_t* buf, int start, int max);
    int32_t      min3(int32_t a, int32_t b, int32_t c);
    uint64_t     bigEndian(uint8_t* base, uint8_t numBytes, uint8_t shiftLeft = 8);
    bool         b64encode(const char* source, uint16_t sourceLength, char* dest);
    void         vector_clear_and_shrink(std::vector<ps_ptr<char>>& vec);
    void         deque_clear_and_shrink(std::deque<ps_ptr<char>>& deq);
    uint32_t     simpleHash(const char* str);
    ps_ptr<char> urlencode(const char* str, bool spacesOnly);
    uint32_t     bswap32(uint32_t x);
    uint64_t     bswap64(uint64_t x);

  private:
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    enum : int { EXTERNAL_I2S = 0, INTERNAL_DAC = 1, INTERNAL_PDM = 2 };
    enum : int { FORMAT_NONE = 0, FORMAT_M3U = 1, FORMAT_PLS = 2, FORMAT_ASX = 3, FORMAT_M3U8 = 4 }; // playlist formats
    const char* plsFmtStr[5] = {"NONE", "M3U", "PLS", "ASX", "M3U8"};                                // playlist format string
    enum : int { AUDIO_NONE, HTTP_RESPONSE_HEADER, HTTP_RANGE_HEADER, AUDIO_DATA, AUDIO_LOCALFILE, AUDIO_PLAYLISTINIT, AUDIO_PLAYLISTHEADER, AUDIO_PLAYLISTDATA };
    const char* dataModeStr[8] = {"AUDIO_NONE", "HTTP_RESPONSE_HEADER", "HTTP_RANGE_HEADER", "AUDIO_DATA", "AUDIO_LOCALFILE", "AUDIO_PLAYLISTINIT", "AUDIO_PLAYLISTHEADER", "AUDIO_PLAYLISTDATA"};
    enum : int { FLAC_BEGIN = 0, FLAC_MAGIC = 1, FLAC_MBH = 2, FLAC_SINFO = 3, FLAC_PADDING = 4, FLAC_APP = 5, FLAC_SEEK = 6, FLAC_VORBIS = 7, FLAC_CUESHEET = 8, FLAC_PICTURE = 9, FLAC_OKAY = 100 };
    enum : int { MP3_BEGIN = 0, MP3_ID3HEADER, MP3_NEXTID3, MP3_EXTHEADER, MP3_ID3FRAME, MP3_FRAMESIZE, MP3_SKIP, MP3_TAG, MP3_SYLT, MP3_ID3V22, MP3_LASTFRAMES, MP3_XING, MP3_OKAY = 100 };
    enum : int {
        M4A_BEGIN = 0,
        M4A_FTYP = 1,
        M4A_CHK = 2,
        M4A_MOOV = 3,
        M4A_FREE = 4,
        M4A_TRAK = 5,
        M4A_MDAT = 6,
        M4A_ILST = 7,
        M4A_MP4A = 8,
        M4A_ESDS = 9,
        M4A_MDIA = 10,
        M4A_MINF = 11,
        M4A_STBL = 12,
        M4A_STSD = 13,
        M4A_UDTA = 14,
        M4A_STSZ = 15,
        M4A_META = 16,
        M4A_MDHD = 17,
        M4A_CHPL = 18,
        M4A_AMRDY = 99,
        M4A_OKAY = 100,
    };
    enum : int { CODEC_NONE = 0, CODEC_WAV = 1, CODEC_MP3 = 2, CODEC_AAC = 3, CODEC_M4A = 4, CODEC_FLAC = 5, CODEC_AACP = 6, CODEC_OPUS = 7, CODEC_OGG = 8, CODEC_VORBIS = 9 };
    const char* codecname[10] = {"unknown", "WAV", "MP3", "AAC", "M4A", "FLAC", "AACP", "OPUS", "OGG", "VORBIS"};
    enum : int { ST_NONE = 0, ST_WEBFILE = 1, ST_WEBSTREAM = 2 };
    const char* streamTypeStr[3] = {"NONE", "WEBFILE", "WEBSTREAM"};
    typedef enum { LEFTCHANNEL = 0, RIGHTCHANNEL = 1 } SampleIndex;
    typedef enum { LOWSHELF = 0, PEAKINGEQ = 1, HIFGSHELF = 2 } FilterType;

  private:
    typedef struct _filter {
        float a0;
        float a1;
        float a2;
        float b1;
        float b2;
    } filter_t;

    typedef struct _pis_array {
        int number;
        int pids[4];
    } pid_array;

    File                m_audiofile;
    NetworkClient       client;
    NetworkClientSecure clientsecure;
    NetworkClient*      m_client = nullptr;

    SemaphoreHandle_t mutex_playAudioData;
    SemaphoreHandle_t mutex_audioTask;
    SemaphoreHandle_t mutex_audioTaskIsDecoding;
    TaskHandle_t      m_audioTaskHandle = nullptr;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

    i2s_chan_handle_t m_i2s_tx_handle = {};
    i2s_chan_config_t m_i2s_chan_cfg = {}; // stores I2S channel values
    i2s_std_config_t  m_i2s_std_cfg = {};  // stores I2S driver values

#pragma GCC diagnostic pop

  private: // ---------- FFT section -----------
#define FFT_SIZE  256
#define FFT_BANDS 3
    float m_fft_in[FFT_SIZE];                   // FFT input (real)
    alignas(16) float m_fft_work[FFT_SIZE * 2]; // FFT work buffer (complex interleaved)
    float    m_fft_window[FFT_SIZE];            // FFT window
    uint16_t m_fft_pos = 0;                     // FFT state
    bool     m_fft_ready = false;               // FFT state
    bool     m_fft_initialized = false;         // FFT state
    uint8_t  m_spectrum[FFT_BANDS] = {0};       // Spectrum result (0..255)
    float    m_spec_smooth[FFT_BANDS] = {0};    // smoothing
    uint32_t m_fft_last_ms = 0;                 // timing (10 Hz)
    float    m_fft_gain = 1.0f;                 // AGC in process()
    bool     m_lr_switch = false;               // start/stop
    int32_t  l_sample = 0;
    int32_t  r_sample = 0;

    std::vector<ps_ptr<char>> m_playlistContent; // m3u8 playlist buffer from responseHeader
    std::vector<ps_ptr<char>> m_playlistURL;     // m3u8 streamURLs buffer
    std::deque<ps_ptr<char>>  m_linesWithURL;    // extract from m_playlistContent, contains URL and MediaSequenceNumber
    std::vector<ps_ptr<char>> m_linesWithEXTINF; // extract from m_playlistContent, contains length and metadata
    std::vector<ps_ptr<char>> m_syltLines;       // SYLT line table
    std::vector<uint32_t>     m_syltTimeStamp;   // SYLT time table
    std::vector<uint32_t>     m_hashQueue;

    static const uint8_t m_tsPacketSize = 188;
    static const uint8_t m_tsHeaderSize = 4;

    std::unique_ptr<Decoder> m_decoder = {};
    ps_ptr<int32_t>          m_outBuff;        // Interleaved L/R
    ps_ptr<int32_t>          m_samplesBuff48K; // Interleaved L/R
    ps_ptr<char>             m_ibuff;          // used in log_info()
    ps_ptr<char>             m_lastHost;       // Store the last URL to a webstream
    ps_ptr<char>             m_currentHost;    // can be changed by redirection or playlist
    ps_ptr<char>             m_lastM3U8host;
    ps_ptr<char>             m_speechtxt;   // stores tts text
    ps_ptr<char>             m_streamTitle; // stores the last StreamTitle
    ps_ptr<char>             m_playlistBuff;

    const uint16_t m_plsBuffEntryLen = 256; // length of each entry in playlistBuff
    int            m_LFcount = 0;           // Detection of end of header
    uint32_t       m_sampleRate = 48000;
    uint32_t       m_avr_bitrate = 0;       // average bitrate, median calculated by VBR
    uint32_t       m_nominal_bitrate = 0;   // given br from header
    uint32_t       m_audioFilePosition = 0; // current position, counts every readed byte
    uint32_t       m_audioDataReadPtr = 0;  // used in playAudioData
    uint32_t       m_audioFileSize = 0;     // local and web files
    int            m_readbytes = 0;         // bytes read
    uint32_t       m_metacount = 0;         // counts down bytes between metadata
    int            m_controlCounter = 0;    // Status within readID3data() and readWaveHeader()
    float          m_balance = 0.0f;        // -16 (mute left) ... +16 (mute right)
    int32_t        m_inputHistory[6] = {0}; // used in resampleTo48kStereo()
    uint16_t       m_opus_mode = 0;         // celt_only, silk_only or hybrid
    float          m_limiter[2] = {0};
    uint8_t        m_timeoutCounter = 0; // timeout counter
    uint8_t        m_curve = 0;          // volume characteristic
    uint8_t        m_bitsPerSample = 16; // bitsPerSample
    uint8_t        m_channels = 2;

    uint8_t  m_playlistFormat = 0;           // M3U, PLS, ASX
    uint8_t  m_codec = CODEC_NONE;           //
    uint8_t  m_m3u8Codec = CODEC_AAC;        // codec of m3u8 stream
    uint8_t  m_expectedCodec = CODEC_NONE;   // set in connecttohost (e.g. http://url.mp3 -> CODEC_MP3)
    uint8_t  m_expectedPlsFmt = FORMAT_NONE; // set in connecttohost (e.g. streaming01.m3u) -> FORMAT_M3U)
    uint8_t  m_streamType = ST_NONE;
    uint8_t  m_ID3Size = 0; // lengt of ID3frame - ID3header
    uint8_t  m_audioTaskCoreId = 0;
    uint8_t  m_M4A_objectType = 0; // set in read_M4A_Header
    uint8_t  m_M4A_chConfig = 0;   // set in read_M4A_Header
    uint16_t m_M4A_sampleRate = 0; // set in read_M4A_Header
    int16_t  m_validSamples = 0;
    int16_t  m_curSample = 0;
    uint16_t m_dataMode = 0;        // Statemaschine
    uint16_t m_streamTitleHash = 0; // remember streamtitle, ignore multiple occurence in metadata
    uint16_t m_timeout_ms = 250;
    uint16_t m_timeout_ms_ssl = 2700;
    uint32_t m_metaint = 0;                 // Number of databytes between metadata
    uint32_t m_chunkcount = 0;              // Counter for chunked transfer
    uint32_t m_t0 = 0;                      // store millis(), is needed for a small delay
    uint32_t m_bytesNotConsumed = 0;        // pictures or something else that comes with the stream
    uint64_t m_lastGranulePosition = 0;     // necessary to calculate the duration in OPUS and VORBIS
    int32_t  m_resumeFilePos = -1;          // the return value from stopSong(), (-1) is idle
    int32_t  m_fileStartTime = -1;          // may be set in connecttoFS()
    uint16_t m_m3u8_targetDuration = 10;    //
    uint32_t m_stsz_numEntries = 0;         // num of entries inside stsz atom (uint32_t)
    uint32_t m_stsz_position = 0;           // pos of stsz atom within file
    uint32_t m_haveNewFilePos = 0;          // user changed the file position
    bool     m_f_metadata = false;          // assume stream without metadata
    bool     m_f_I2S_init = false;          //
    bool     m_f_unsync = false;            // set within ID3 tag but not used
    bool     m_f_exthdr = false;            // ID3 extended header
    bool     m_f_ssl = false;               //
    bool     m_f_running = false;           //
    bool     m_f_firstCall = false;         // InitSequence for processWebstream and processLokalFile
    bool     m_f_firstLoop = false;         // InitSequence in loop()
    bool     m_f_firstPlayCall = false;     // InitSequence for playAudioData
    bool     m_f_ID3v1TagFound = false;     // ID3v1 tag found
    bool     m_f_chunked = false;           // Station provides chunked transfer
    bool     m_f_firstmetabyte = false;     // True if first metabyte (counter)
    bool     m_f_playing = false;           // valid mp3 stream recognized
    bool     m_f_tts = false;               // text to speech
    bool     m_f_ogg = false;               // OGG stream
    bool     m_f_forceMono = false;         // if true stereo -> mono
    bool     m_f_rtsp = false;              // set if RTSP is used (m3u8 stream)
    bool     m_f_m3u8data = false;          // used in processM3U8entries
    bool     m_f_continue = false;          // next m3u8 chunk is available
    bool     m_f_ts = true;                 // transport stream
    bool     m_f_m4aID3dataAreRead = false; // has the m4a-ID3data already been read?
    bool     m_f_psramFound = false;        // set in constructor, result of psramInit()
    bool     m_f_timeout = false;           //
    bool     m_f_commFMT = false;           // false: default (PHILIPS), true: Least Significant Bit Justified (japanese format)
    bool     m_f_audioTaskIsRunning = false;
    bool     m_f_allDataReceived = false;
    bool     m_f_stream = false;       // stream ready for output?
    bool     m_f_decode_ready = false; // if true data for decode are ready
    bool     m_f_eof = false;          // end of file
    bool     m_f_lockInBuffer = false; // lock inBuffer for manipulation
    bool     m_f_audioTaskIsDecoding = false;
    bool     m_f_acceptRanges = false;
    bool     m_f_reset_m3u8Codec = true;      // reset codec for m3u8 stream
    bool     m_f_connectionClose = false;     // set in parseHttpResponseHeader
    bool     m_f_i2s_channel_enabled = false; // true if enabled
    uint32_t m_audioFileDuration = 0;         // seconds
    uint32_t m_audioCurrentTime = 0;          // seconds
    float    m_resampleError = 0.0f;
    float    m_resampleRatio = 1.0f;  // resample ratio for e.g. 44.1kHz to 48kHz
    float    m_resampleCursor = 0.0f; // next frac in resampleTo48kStereo

    uint32_t m_audioDataStart = 0;   // in bytes
    size_t   m_audioDataSize = 0;    //
    size_t   m_ibuffSize = 0;        // log buffer size for audio_info()
    float    m_corr = 1.0;           // correction factor for level adjustment
    size_t   m_i2s_bytesWritten = 0; // set in i2s_write() but not used
    int8_t   m_gain0 = 0;            // cut or boost filters (EQ)
    int8_t   m_gain1 = 0;
    int8_t   m_gain2 = 0;

    pid_array m_pidsOfPMT;
    int16_t   m_pidOfAAC;
    uint8_t   m_packetBuff[m_tsPacketSize];
    int16_t   m_pesDataLength = 0;

    // audiolib structs
    audiolib::ID3Hdr_t     m_ID3Hdr;
    audiolib::pwsHLS_t     m_pwsHLS;
    audiolib::pplM3u8_t    m_pplM3U8;
    audiolib::m4aHdr_t     m_m4aHdr;
    audiolib::plCh_t       m_plCh;
    audiolib::lVar_t       m_lVar;
    audiolib::prlf_t       m_prlf;
    audiolib::cat_t        m_cat;
    audiolib::ifCh_t       m_ifCh;
    audiolib::tspp_t       m_tspp;
    audiolib::pwst_t       m_pwst;
    audiolib::gchs_t       m_gchs;
    audiolib::pwf_t        m_pwf;
    audiolib::pad_t        m_pad;
    audiolib::sbyt_t       m_sbyt;
    audiolib::rmet_t       m_rmet;
    audiolib::pwsts_t      m_pwsst;
    audiolib::rwh_t        m_rwh;
    audiolib::rflh_t       m_rflh;
    audiolib::phreh_t      m_phreh;
    audiolib::phrah_t      m_phrah;
    audiolib::sdet_t       m_sdet;
    audiolib::fnsy_t       m_fnsy;
    audiolib::audioItems_t m_audio_items;

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
  public:
    // 🎯 overload for char*-Pointer (maybe nullptr)
    static const char* safe_arg(const char* v) { return v ? v : "(null)"; }
    static char*       safe_arg(char* v) { return v ? v : (char*)"(null)"; }

    // 🎯 overload for char-Arrays (never nullptr)
    template <size_t N> static const char* safe_arg(const char (&v)[N]) { return v; }

    // 🎯 overload for string
    static const char* safe_arg(const std::string& v) { return v.c_str(); }

    // 🎯 overload for string_view
    static const char* safe_arg(std::string_view v) {
        return v.data(); // Achtung: evtl. nicht nullterminiert
    }

    // 🎯 Catch-all for all other types
    template <typename T> static auto safe_arg(T&& v) -> decltype(auto) { return std::forward<T>(v); }

    // specialization for nullptr / NULL
    static const char* safe_arg(std::nullptr_t) { return "(null)"; }

    template <typename... Args> static bool info(Audio& instance, event_t e, const char* fmt, Args&&... args) {
        std::lock_guard<std::mutex> lock(instance.mutex_info); // lock mutex
                                                               // -------------------------------------------------------------------------------------------------------------------
        auto extract_last_number = [](std::string_view s) -> int32_t {
            // start from the back
            auto it = s.end();
            // skip whitespaces at the end (if available)
            while (it != s.begin() && std::isspace(static_cast<unsigned char>(*(it - 1)))) { --it; }

            auto end = it; // potential end of the number

            // Now only search digits backwards
            while (it != s.begin() && std::isdigit(static_cast<unsigned char>(*(it - 1)))) { --it; }

            // Check: is there a space before it?
            if (it != s.begin() && std::isspace(static_cast<unsigned char>(*(it - 1)))) {
                std::string_view number{it, static_cast<size_t>(end - it)};
                uint32_t         value{};
                auto [p, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
                if (ec == std::errc{}) { return static_cast<int32_t>(value); }
            }

            return -1; // no number found in the end
        };
        // -------------------------------------------------------------------------------------------------------------------
        if (!fmt) return false;
        if (!audio_info_callback) return false;
        ps_ptr<char> result;
        // First run: determine size
        int len = std::snprintf(nullptr, 0, fmt, safe_arg(std::forward<Args>(args))...);
        if (len <= 0) return false;
        result.calloc(len + 1);
        char* p = result.get();
        if (!p) return false;
        std::snprintf(p, len + 1, fmt, safe_arg(std::forward<Args>(args))...);
        msg_t i = {0};
        i.msg = result.c_get();
        i.e = e;
        i.s = eventStr[e];
        i.arg1 = extract_last_number(result.c_get());
        i.i2s_num = instance.m_i2s_num;
        audio_info_callback(i);
        result.reset();
        return true;
    }

    static bool info(Audio& instance, event_t e, std::vector<uint32_t>& v) {
        if (!audio_info_callback) return false;
        ps_ptr<char> apic;
        apic.assignf("APIC found at pos %lu", v[0]);
        msg_t i;
        i.msg = apic.c_get();
        i.e = e;
        i.s = eventStr[e];
        i.i2s_num = instance.m_i2s_num;
        i.vec = v;
        audio_info_callback(i);
        return true;
    }
    //----------------------------------------------------------------------------------------------------------------------

    template <typename... Args> static void AUDIO_LOG_IMPL(uint8_t level, const char* path, int line, const char* fmt, Args&&... args) {

#define ANSI_ESC_RESET   "\033[0m"
#define ANSI_ESC_BLACK   "\033[30m"
#define ANSI_ESC_RED     "\033[31m"
#define ANSI_ESC_GREEN   "\033[32m"
#define ANSI_ESC_YELLOW  "\033[33m"
#define ANSI_ESC_BLUE    "\033[34m"
#define ANSI_ESC_MAGENTA "\033[35m"
#define ANSI_ESC_CYAN    "\033[36m"
#define ANSI_ESC_WHITE   "\033[37m"

        ps_ptr<char> logStr;
        logStr.copy_from(path);
        while (logStr.contains("/")) { logStr.remove_before('/', false); }
        logStr.appendf(":%i ", line);

        if (level == 1 && CORE_DEBUG_LEVEL >= 1) {
            logStr.append(ANSI_ESC_RED);
        } else if (level == 2 && CORE_DEBUG_LEVEL >= 2) {
            logStr.append(ANSI_ESC_YELLOW);
        } else if (level == 3 && CORE_DEBUG_LEVEL >= 3) {
            logStr.append(ANSI_ESC_GREEN);
        } else if (level == 4 && CORE_DEBUG_LEVEL >= 4) {
            logStr.append(ANSI_ESC_CYAN);
        } // debug
        else if (level == 5 && CORE_DEBUG_LEVEL >= 4) {
            logStr.append(ANSI_ESC_WHITE);
        } // verbose
        else
            return;

        int add_len = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
        if (add_len > 0) {
            logStr.appendf(fmt, std::forward<Args>(args)...); // <-- neue appendf()
        }
        logStr.append(ANSI_ESC_RESET);

        msg_t msg;
        msg.msg = logStr.get();
        const char* tag[7] = {"", "LOGE", "LOGW", "LOGI", "LOGD", "LOGV", ""};
        msg.s = tag[level];
        msg.e = evt_log;

        if (audio_info_callback)
            audio_info_callback(msg);
        else {
            if (level == 1)
                log_e("%s", logStr.c_get());
            else if (level == 2)
                log_w("%s", logStr.c_get());
            else if (level == 3)
                log_i("%s", logStr.c_get());
            else if (level == 4)
                log_d("%s", logStr.c_get());
            else
                log_v("%s", logStr.c_get());
        }
        logStr.reset();
    }

// Macro for comfortable calls
#define AUDIO_LOG_ERROR(fmt, ...) AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AUDIO_LOG_WARN(fmt, ...)  AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AUDIO_LOG_INFO(fmt, ...)  AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AUDIO_LOG_DEBUG(fmt, ...) AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  D E C O D E R  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
class Decoder {
  public:
    virtual ~Decoder() = default;
    virtual bool                  init() = 0;
    virtual void                  clear() = 0;
    virtual void                  reset() = 0;
    virtual bool                  isValid() = 0;
    virtual int32_t               findSyncWord(uint8_t* buf, int32_t nBytes) = 0;
    virtual uint8_t               getChannels() = 0;
    virtual uint32_t              getSampleRate() = 0;
    virtual uint8_t               getBitsPerSample() = 0;
    virtual uint32_t              getBitRate() = 0;
    virtual uint32_t              getAudioDataStart() = 0;
    virtual uint32_t              getAudioFileDuration() = 0;
    virtual uint32_t              getOutputSamples() = 0;
    virtual int32_t               decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf1) = 0;
    virtual void                  setRawBlockParams(uint8_t param1, uint32_t param2, uint8_t param3, uint32_t param4, uint32_t param5) = 0;
    virtual const char*           getStreamTitle();
    virtual const char*           whoIsIt();
    virtual std::vector<uint32_t> getMetadataBlockPicture() = 0;
    virtual const char*           arg1() = 0; // decoder specific
    virtual const char*           arg2() = 0; // decoder specific
    virtual int32_t               val1() = 0; // decoder specific
    virtual int32_t               val2() = 0; // decoder specific

  protected:
    Decoder(Audio& audioRef) : audio(audioRef) {}
    Audio& audio; // protected reference, usable by all subclasses
  private:
    Decoder() = delete; // Deactivate default constructor explicitly (optional but good against abuse)
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  A U T O L O G G E R     for detecting memory leaks  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* usage
    void myFunction(){
        HEAP_GUARD();  // <--- automatic check
        my code ...
        my code ...
    }

    { // Or in small critical code blocks:
        HEAP_GUARD();
        fill_content(inbuf, to_read);
    }
*/

struct _HeapGuardSnapshot {
    size_t      free_dram_before{};
    size_t      free_psram_before{};
    bool        integrity_before{};
    const char* func{};
    bool        active{false};

    _HeapGuardSnapshot(const char* f) : func(f), active(true) {
        free_dram_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        free_psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        integrity_before = heap_caps_check_integrity_all(true);
        if (!integrity_before) {
            printf(ANSI_ESC_RED "HEAPGUARD [%s] ❌ Heap corruption detected BEFORE!" ANSI_ESC_RESET "\n", func);
        } else {
            printf(ANSI_ESC_GREEN "HEAPGUARD [%s] Begin: DRAM=%u, PSRAM=%u" ANSI_ESC_RESET "\n", func, (unsigned)free_dram_before, (unsigned)free_psram_before);
        }
    }

    ~_HeapGuardSnapshot() {
        if (!active) return; // falls moved / deaktiviert
        size_t free_dram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t free_psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        bool   ok = heap_caps_check_integrity_all(true);

        int delta_dram = (int)(free_dram_after - free_dram_before);
        int delta_psram = (int)(free_psram_after - free_psram_before);

        if (!ok) {
            printf(ANSI_ESC_RED "HEAPGUARD [%s] ❌ Heap corruption detected AFTER!" ANSI_ESC_RESET "\n", func);
        } else {
            printf(ANSI_ESC_GREEN "HEAPGUARD [%s] ✅ Heap OK | ΔDRAM=%+d | ΔPSRAM=%+d" ANSI_ESC_RESET "\n", func, delta_dram, delta_psram);
        }
    }
};
#define HEAP_GUARD() _HeapGuardSnapshot _heapguard_instance_##__LINE__(__func__)
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  A U T O P R O F I L E R    RAII-class for timekeeping  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* usage
    void decodeNative(uint8_t* inbuf, int bytesLeft, uint8_t* outbuf) {
        PROFILE_SCOPE_N(1000);  // automatically measures 1000 views on average

        // ... my code ...
    }

    {   // Or in small critical code blocks:
        PROFILE_SCOPE_N(100);  // measures this block over 100 runs
        do_fft_processing(data);
    }
*/
class _AutoProfiler {
  public:
    _AutoProfiler(const char* name, uint32_t report_interval) : tag(name), N(report_interval) { start = esp_timer_get_time(); }

    ~_AutoProfiler() {
        uint64_t elapsed = esp_timer_get_time() - start;
        sum += elapsed;
        count++;

        if (count >= N) {
            double avg_us = (double)sum / count;
            printf(ANSI_ESC_CYAN "PROFILER [%s] avg: %.2f µs over %lu runs" ANSI_ESC_RESET "\n", tag, avg_us, count);
            sum = 0;
            count = 0;
        }
    }

  private:
    const char*            tag;
    uint32_t               N;
    uint64_t               start;
    static inline uint64_t sum = 0;
    static inline uint32_t count = 0;
};

// Macro for automatic use with function name
#define PROFILE_SCOPE_N(N) _AutoProfiler _prof_instance_##__LINE__(__func__, N)
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
