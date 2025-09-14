// based on Xiph.Org Foundation celt decoder
#pragma once

#include "Arduino.h"
#include "Audio.h"
#include "../psram_unique_ptr.hpp"
#include <vector>
#include "range_decoder.h"
#include "silk.h"
#include "celt.h"

class OpusDecoder : public Decoder{

public:
    OpusDecoder(Audio& audioRef);
    ~OpusDecoder(){reset();}
    bool             init() override;
    void             clear() override;
    void             reset() override;
    int32_t          findSyncWord(uint8_t* buf, int32_t nBytes) override;
    uint8_t          getChannels() override;
    uint32_t         getSampleRate() override;
    uint32_t         getOutputSamples();
    uint8_t          getBitsPerSample() override;
    uint32_t         getBitRate() override;
    uint32_t         getAudioDataStart() override;
    uint32_t         getAudioFileDuration() override;
    const char*      getStreamTitle() override;
    int32_t          decode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf) override;
    void             setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength) override;
    std::vector<uint32_t> getMetadataBlockPicture() override;
    const char*      arg1() override;
    const char*      arg2() override;

    std::unique_ptr<RangeDecoder> rangedec;
    std::unique_ptr<SilkDecoder> silkdec;
    std::unique_ptr<CeltDecoder> celtdec;

    enum : int8_t { OPUS_END = 120,
                    OPUS_CONTINUE = 10,
                    OPUS_PARSE_OGG_DONE = 100,
                    OPUS_NONE = 0,
                    OPUS_ERR = -1
    };

private:
    typedef struct _ofp2 {
        uint16_t firstFrameLength;
        uint16_t secondFrameLength;
    } ofp2_t;

    typedef struct _ofp3 { // opus_FramePacking_Code
        bool        firstCall = true;
        bool        v = false; // VBR indicator
        bool        p = false; // padding exists
        int16_t     fs = 0;    // frame size
        uint8_t     M = 0;     // nr of frames
        int32_t     spf = 0;   // samples per frame
        int32_t     paddingLength = 0;
        uint16_t    c1fs = 0;
        uint16_t    vfs[48];   // variable frame size
        uint32_t    idx;
    } ofp3_t;

    typedef struct _odp3 {
        int8_t configNr;
        uint16_t samplesPerFrame;
    } odp3_t;


    #define CELT_SET_END_BAND_REQUEST         10012
    #define CELT_SET_CHANNELS_REQUEST         10008
    #define CELT_SET_START_BAND_REQUEST       10010
    #define CELT_SET_SIGNALLING_REQUEST       10016
    #define CELT_GET_AND_CLEAR_ERROR_REQUEST  10007
    #define CELT_GET_MODE_REQUEST             10015

    enum {OPUS_BANDWIDTH_NARROWBAND = 1101,    OPUS_BANDWIDTH_MEDIUMBAND = 1102, OPUS_BANDWIDTH_WIDEBAND = 1103,
          OPUS_BANDWIDTH_SUPERWIDEBAND = 1104, OPUS_BANDWIDTH_FULLBAND = 1105};

    uint8_t          m_opusChannels = 0;
    uint8_t          m_opusCountCode = 0;
    uint8_t          m_opusPageNr = 0;
    uint8_t          m_frameCount = 0;
    uint8_t          m_opusSegmentTableSize = 0;
    uint16_t         m_mode = 0;
    uint16_t         m_opusOggHeaderSize = 0;
    uint16_t         m_bandWidth = 0;
    uint16_t         m_internalSampleRate = 0;
    uint16_t         m_endband = 0;
    uint32_t         m_opusSamplerate = 0;
    uint32_t         m_opusSegmentLength = 0;
    uint32_t         m_opusCurrentFilePos = 0;
    uint32_t         m_opusAudioDataStart = 0;
    uint32_t         m_opusBlockPicPos = 0;
    uint32_t         m_opusBlockLen = 0;
    bool             m_f_opusParseOgg = false;
    bool             m_f_newSteamTitle = false;               // streamTitle
    bool             m_f_opusNewMetadataBlockPicture = false; // new metadata block picture
    bool             m_f_opusStereoFlag = false;
    bool             m_f_continuedPage = false;
    bool             m_f_firstPage = false;
    bool             m_f_lastPage = false;
    bool             m_f_nextChunk = false;
    int8_t           m_opusError = 0;
    int16_t          m_opusSegmentTableRdPtr = -1;
    int16_t          m_prev_mode = 0;
    int32_t          m_opusValidSamples = 0;
    int32_t          m_opusBlockPicLen = 0;
    int32_t          m_blockPicLenUntilFrameEnd = 0;
    int32_t          m_opusRemainBlockPicLen = 0;
    int32_t          m_opusCommentBlockSize = 0;
    float            m_opusCompressionRatio = 0;

    ps_ptr<char>     m_streamTitle;
    ps_ptr<uint16_t> m_opusSegmentTable;

    ofp2_t  m_ofp2; // used in opus_FramePacking_Code2
    ofp3_t  m_ofp3; // used in opus_FramePacking_Code3
    odp3_t  m_odp3; // used in opusDecodePage3

    std::vector<uint32_t>m_opusBlockPicItem;


    void             OPUSsetDefaults();
    int32_t          opusDecodePage0(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength);
    int32_t          opusDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t *outbuf);
    int8_t           opus_FramePacking_Code0(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame);
    int8_t           opus_FramePacking_Code1(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
    int8_t           opus_FramePacking_Code2(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
    int8_t           opus_FramePacking_Code3(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount);
    int32_t          OPUSparseOGG(uint8_t* inbuf, int32_t* bytesLeft);
    int32_t          parseOpusHead(uint8_t* inbuf, int32_t nBytes);
    int32_t          parseOpusComment(uint8_t* inbuf, int32_t nBytes);
    int8_t           parseOpusTOC(uint8_t TOC_Byte);
    int32_t          opus_packet_get_samples_per_frame(const uint8_t* data, int32_t Fs);
    int32_t          opus_decode_frame(uint8_t *inbuf, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame);

    // some helper functions
    int32_t OPUS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);

    enum {MODE_NONE = 0, MODE_SILK_ONLY = 1000, MODE_HYBRID = 1001,  MODE_CELT_ONLY = 1002};
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  L O G G I N G   📌📌📌
extern __attribute__((weak)) bool audio_info(const char*);

template <typename... Args>
void OPUS_LOG_IMPL(uint8_t level, const char* path, int line, const char* fmt, Args&&... args) {
    #define ANSI_ESC_RESET          "\033[0m"
    #define ANSI_ESC_BLACK          "\033[30m"
    #define ANSI_ESC_RED            "\033[31m"
    #define ANSI_ESC_GREEN          "\033[32m"
    #define ANSI_ESC_YELLOW         "\033[33m"
    #define ANSI_ESC_BLUE           "\033[34m"
    #define ANSI_ESC_MAGENTA        "\033[35m"
    #define ANSI_ESC_CYAN           "\033[36m"
    #define ANSI_ESC_WHITE          "\033[37m"

    ps_ptr<char> result;
    ps_ptr<char> file;
    bool res = false;

    file.copy_from(path);
    while(file.contains("/")){
        file.remove_before('/', false);
    }

    // First run: determine size
    int len = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
    if (len <= 0) return;

    result.alloc(len + 1, "result");
    char* dst = result.get();
    if (!dst) return;
    std::snprintf(dst, len + 1, fmt, std::forward<Args>(args)...);

    // build a final string with file/line prefix
    ps_ptr<char> final;
    int total_len = std::snprintf(nullptr, 0, "%s:%d:" ANSI_ESC_RED " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
    if (total_len <= 0) return;
    final.calloc(total_len + 1, "final");
    final.clear();
    char* dest = final.get();
    if (!dest) return;  // Or error treatment

    if     (level == 1 && CORE_DEBUG_LEVEL >= 1) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_RED " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
    else if(level == 2 && CORE_DEBUG_LEVEL >= 2) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_YELLOW " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
    else if(level == 3 && CORE_DEBUG_LEVEL >= 3) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_GREEN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
    else if(level == 4 && CORE_DEBUG_LEVEL >= 4) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_CYAN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
    else              if( CORE_DEBUG_LEVEL >= 5) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_WHITE" %s" ANSI_ESC_RESET, file.c_get(), line, dst);

    if(final.strlen()){
        res = audio_info(final.get());

        if(!res){
            std::snprintf(dest, total_len + 1, "%s:%d: %s", file.c_get(), line, dst);
            if     (level == 1) log_e("%s", final.c_get());
            else if(level == 2) log_w("%s", final.c_get());
            else if(level == 3) log_i("%s", final.c_get());
            else if(level == 4) log_d("%s", final.c_get());
            else                log_v("%s", final.c_get());
        }
    }

    final.reset();
    result.reset();
}

// Macro for comfortable calls
#define OPUS_LOG_ERROR(fmt, ...)   OPUS_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_WARN(fmt, ...)    OPUS_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_INFO(fmt, ...)    OPUS_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_DEBUG(fmt, ...)   OPUS_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define OPUS_LOG_VERBOSE(fmt, ...) OPUS_LOG_IMPL(5, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

