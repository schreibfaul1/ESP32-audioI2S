// based on Xiph.Org Foundation celt decoder
#pragma once

#include "../Audio.h"
#include "../psram_unique_ptr.hpp"
#include "celt.h"
#include "range_decoder.h"
#include "silk.h"
#include <vector>

#define ANSI_ESC_RESET   "\033[0m"
#define ANSI_ESC_BLACK   "\033[30m"
#define ANSI_ESC_RED     "\033[31m"
#define ANSI_ESC_GREEN   "\033[32m"
#define ANSI_ESC_YELLOW  "\033[33m"
#define ANSI_ESC_BLUE    "\033[34m"
#define ANSI_ESC_MAGENTA "\033[35m"
#define ANSI_ESC_CYAN    "\033[36m"
#define ANSI_ESC_WHITE   "\033[37m"

class OpusDecoder : public Decoder {

  public:
    OpusDecoder(Audio& audioRef)
        : Decoder(audioRef), rangedec(std::make_unique<RangeDecoder>()), silkdec(std::make_unique<SilkDecoder>(*rangedec)), celtdec(std::make_unique<CeltDecoder>(*rangedec)), audio(audioRef) {}
    ~OpusDecoder() { reset(); }
    bool                  init() override;
    void                  clear() override;
    void                  reset() override;
    bool                  isValid() override;
    int32_t               findSyncWord(uint8_t* buf, int32_t nBytes) override;
    uint8_t               getChannels() override;
    uint32_t              getSampleRate() override;
    uint32_t              getOutputSamples();
    uint8_t               getBitsPerSample() override;
    uint32_t              getBitRate() override;
    uint32_t              getAudioDataStart() override;
    uint32_t              getAudioFileDuration() override;
    const char*           getStreamTitle() override;
    const char*           whoIsIt() override;
    int32_t               decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf) override;
    void                  setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength) override;
    std::vector<uint32_t> getMetadataBlockPicture() override;
    const char*           arg1() override;
    const char*           arg2() override;
    virtual int32_t       val1() override;
    virtual int32_t       val2() override;

    std::unique_ptr<RangeDecoder> rangedec;
    std::unique_ptr<SilkDecoder>  silkdec;
    std::unique_ptr<CeltDecoder>  celtdec;

    enum : int8_t { OPUS_END = 120, OPUS_CONTINUE = 10, OPUS_PARSE_OGG_DONE = 100, OPUS_NONE = 0, OPUS_ERR = -1 };

  private:
    Audio& audio;
    typedef struct _ofp2 {
        uint16_t firstFrameLength{};
        uint16_t secondFrameLength{};

        void reset() {
            *this = _ofp2{}; // sauber neu initialisieren
        }
    } ofp2_t;

    typedef struct _ofp3 { // opus_FramePacking_Code
        bool     firstCall{};
        bool     v{};   // VBR indicator
        bool     p{};   // padding exists
        int16_t  fs{};  // frame size
        uint8_t  M{};   // nr of frames
        int32_t  spf{}; // samples per frame
        int32_t  paddingLength{};
        uint16_t c1fs{};
        uint16_t vfs[48]{}; // variable frame size
        uint32_t idx{};

        void reset() {
            *this = _ofp3{}; // sauber neu initialisieren
        }
    } ofp3_t;

    typedef struct _odp3 {
        int8_t   configNr{};
        uint16_t samplesPerFrame{};

        void reset() {
            // Default-initialize alles neu (inklusive Array)
            *this = _odp3{};
        }

    } odp3_t;

#define CELT_SET_END_BAND_REQUEST        10012
#define CELT_SET_CHANNELS_REQUEST        10008
#define CELT_SET_START_BAND_REQUEST      10010
#define CELT_SET_SIGNALLING_REQUEST      10016
#define CELT_GET_AND_CLEAR_ERROR_REQUEST 10007
#define CELT_GET_MODE_REQUEST            10015

    enum { OPUS_BANDWIDTH_NARROWBAND = 1101, OPUS_BANDWIDTH_MEDIUMBAND = 1102, OPUS_BANDWIDTH_WIDEBAND = 1103, OPUS_BANDWIDTH_SUPERWIDEBAND = 1104, OPUS_BANDWIDTH_FULLBAND = 1105 };
    enum ParseResult { OPUS_COMMENT_INVALID = -1, OPUS_COMMENT_NEED_MORE = 1, OPUS_COMMENT_DONE = 2 };

    uint8_t         m_opusChannels = 0;
    uint8_t         m_opusCountCode = 0;
    uint8_t         m_opusPageNr = 0;
    uint8_t         m_frameCount = 0;
    uint8_t         m_opusSegmentTableSize = 0;
    uint16_t        m_mode = 0;
    uint16_t        m_opusOggHeaderSize = 0;
    uint16_t        m_bandWidth = 0;
    uint16_t        m_internalSampleRate = 0;
    uint16_t        m_endband = 0;
    uint32_t        m_opusSamplerate = 0;
    uint32_t        m_opusSegmentLength = 0;
    uint32_t        m_opusCurrentFilePos = 0;
    uint32_t        m_opusAudioDataStart = 0;
    uint32_t        m_opusBlockPicPos = 0;
    uint32_t        m_opusBlockLen = 0;
    bool            m_f_opusParseOgg = false;
    bool            m_f_newSteamTitle = false;               // streamTitle
    bool            m_f_opusNewMetadataBlockPicture = false; // new metadata block picture
    bool            m_f_opusStereoFlag = false;
    bool            m_f_continuedPage = false;
    bool            m_f_firstPage = false;
    bool            m_f_lastPage = false;
    bool            m_f_nextChunk = false;
    bool            m_isValid = false;
    int8_t          m_opusError = 0;
    int16_t         m_opusSegmentTableRdPtr = -1;
    int16_t         m_prev_mode = 0;
    int32_t         m_opusValidSamples = 0;
    int32_t         m_opusBlockPicLen = 0;
    int32_t         m_blockPicLenUntilFrameEnd = 0;
    int32_t         m_opusRemainBlockPicLen = 0;
    int32_t         m_opusCommentBlockSize = 0;
    float           m_opusCompressionRatio = 0;
    ps_ptr<int16_t> m_out16;

    struct picture_segment_t {
        uint32_t start_page_index{};
        uint32_t start_offset{};
        uint32_t end_page_index{};
        uint32_t end_offset{};
        bool     in_progress{};
    };

    typedef struct _comment {
        uint32_t pointer{};
        uint32_t list_length{};
        bool     oob{}; // out of bounds (block overflow)
        uint32_t save_len{};
        uint32_t comment_size{};
        uint32_t start_pos{};       // comment start file position
        uint32_t end_pos{};         // comment end file position
        uint8_t  length_bytes[4]{}; // 🆕 Addition for split 4-byte length fields
        uint8_t  partial_length{};  // how many of the 4 bytes have already been read
        uint32_t bytes_available{};

        ps_ptr<char>          stream_title{};
        ps_ptr<char>          comment_content{};
        std::vector<uint32_t> item_vec;
        std::vector<uint32_t> pic_vec;

        void reset() { *this = _comment{}; }
    } comment_t;
    comment_t m_comment;

    ps_ptr<uint16_t> m_opusSegmentTable;

    ofp2_t m_ofp2; // used in opus_FramePacking_Code2
    ofp3_t m_ofp3; // used in opus_FramePacking_Code3
    odp3_t m_odp3; // used in opusDecodePage3

    std::vector<uint32_t> m_opusBlockPicItem;

    void    OPUSsetDefaults();
    int32_t opusDecodePage0(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
    int32_t opusDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t* outbuf);
    int8_t  opus_FramePacking_Code0(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame);
    int8_t  opus_FramePacking_Code1(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
    int8_t  opus_FramePacking_Code2(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
    int8_t  opus_FramePacking_Code3(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
    int32_t parseOGG(uint8_t* inbuf, int32_t* bytesLeft);
    int32_t parseOpusHead(uint8_t* inbuf, int32_t nBytes);
    int32_t parseOpusComment(uint8_t* inbuf, int32_t nBytes, uint32_t current_file_pos);
    int8_t  parseOpusTOC(uint8_t TOC_Byte);
    int32_t opus_packet_get_samples_per_frame(const uint8_t* data, int32_t Fs);
    int32_t opus_decode_frame(uint8_t* inbuf, int16_t* outbuf, int32_t packetLen, uint16_t samplesPerFrame);

    // some helper functions
    int32_t  OPUS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
    int32_t  OPUS_specialIndexOf_icase(uint8_t* base, const char* str, int32_t baselen, bool exact = false);
    uint32_t little_endian(uint8_t* data);
    enum { MODE_NONE = 0, MODE_SILK_ONLY = 1000, MODE_HYBRID = 1001, MODE_CELT_ONLY = 1002 };

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // Macro for comfortable calls
#define OPUS_LOG_ERROR(fmt, ...)   Audio::AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_WARN(fmt, ...)    Audio::AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_INFO(fmt, ...)    Audio::AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_DEBUG(fmt, ...)   Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_VERBOSE(fmt, ...) Audio::AUDIO_LOG_IMPL(5, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // Macro for time measuring
    // PROFILE_START(decodeNative);
    // ret = decodeNative(inbuf, bytesLeft, outbuf);
    // PROFILE_END_N(decodeNative, 1000);

#define PROFILE_START(name)                   \
    static uint64_t _prof_##name##_start = 0; \
    _prof_##name##_start = esp_timer_get_time()

#define PROFILE_END_N(name, N)                                                                                                           \
    do {                                                                                                                                 \
        static uint64_t _prof_##name##_sum = 0;                                                                                          \
        static uint32_t _prof_##name##_count = 0;                                                                                        \
        uint64_t        _prof_##name##_elapsed = esp_timer_get_time() - _prof_##name##_start;                                            \
        _prof_##name##_sum += _prof_##name##_elapsed;                                                                                    \
        _prof_##name##_count++;                                                                                                          \
        if (_prof_##name##_count >= (N)) {                                                                                               \
            printf("%-20s avg: %.2f µs over %u runs\n", #name, (double)_prof_##name##_sum / _prof_##name##_count, _prof_##name##_count); \
            _prof_##name##_sum = 0;                                                                                                      \
            _prof_##name##_count = 0;                                                                                                    \
        }                                                                                                                                \
    } while (0)
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
