/*
 * flac_decoder.h
 *
 * Created on: Jul 03,2020
 * Updated on: Mar 29,2025
 *
 *      Author: wolle
 *
 *  Restrictions:
 *  blocksize must not exceed 24576 bytes
 *  bits per sample must be 8 or 16
 *  num Channels must be 1 or 2
 *
 *
 */
#pragma once
#pragma GCC optimize("Ofast")

#include "../Audio.h"

#define ANSI_ESC_RESET   "\033[0m"
#define ANSI_ESC_BLACK   "\033[30m"
#define ANSI_ESC_RED     "\033[31m"
#define ANSI_ESC_GREEN   "\033[32m"
#define ANSI_ESC_YELLOW  "\033[33m"
#define ANSI_ESC_BLUE    "\033[34m"
#define ANSI_ESC_MAGENTA "\033[35m"
#define ANSI_ESC_CYAN    "\033[36m"
#define ANSI_ESC_WHITE   "\033[37m"

class FlacDecoder : public Decoder {

public:
    FlacDecoder(Audio& audioRef) : Decoder(audioRef), audio(audioRef) {}
    ~FlacDecoder() { reset(); }
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

    enum : int8_t {
        FLAC_PARSE_OGG_DONE = 100,
                    FLAC_DECODE_FRAMES_LOOP = 100,
                    FLAC_OGG_SYNC_FOUND = +2,
                    GIVE_NEXT_LOOP = +1,
                    FLAC_NONE = 0,
                    FLAC_ERR = -1,
                    FLAC_STOP = -100,
    };

private:
    Audio& audio;
#define FLAC_MAX_CHANNELS    2
#define FLAC_MAX_BLOCKSIZE   24576 // 24 * 1024
    #define FLAC_MAX_OUTBUFFSIZE 4096 * 2

    enum : uint8_t { FLACDECODER_INIT, FLACDECODER_READ_IN, FLACDECODER_WRITE_OUT };
    enum : uint8_t { DECODE_FRAME, DECODE_SUBFRAMES, OUT_SAMPLES };

    typedef struct FLACMetadataBlock_t {
                                  // METADATA_BLOCK_STREAMINFO
        uint16_t minblocksize;    // The minimum block size (in samples) used in the stream.
                                  //----------------------------------------------------------------------------------------
                                  // The maximum block size (in samples) used in the stream.
        uint16_t maxblocksize;    // (Minimum blocksize == maximum blocksize) implies a fixed-blocksize stream.
                                  //----------------------------------------------------------------------------------------
                                  // The minimum frame size (in bytes) used in the stream.
        uint32_t minframesize;    // May be 0 to imply the value is not known.
                                  //----------------------------------------------------------------------------------------
                                  // The maximum frame size (in bytes) used in the stream.
        uint32_t maxframesize;    // May be 0 to imply the value is not known.
                                  //----------------------------------------------------------------------------------------
                                  // Sample rate in Hz. Though 20 bits are available,
                                  // the maximum sample rate is limited by the structure of frame headers to 655350Hz.
        uint32_t sampleRate;      // Also, a value of 0 is invalid.
                                  //----------------------------------------------------------------------------------------
                                  // Number of channels FLAC supports from 1 to 8 channels
        uint8_t numChannels;      // 000 : 1 channel .... 111 : 8 channels
                                  //----------------------------------------------------------------------------------------
                                  // Sample size in bits:
                                  // 000 : get from STREAMINFO metadata block
                                  // 001 : 8 bits per sample
                                  // 010 : 12 bits per sample
                                  // 011 : reserved
                                  // 100 : 16 bits per sample
                                  // 101 : 20 bits per sample
                                  // 110 : 24 bits per sample
        uint8_t bitsPerSample;    // 111 : reserved
                                  //----------------------------------------------------------------------------------------
                                  // Total samples in stream. 'Samples' means inter-channel sample,
                                  // i.e. one second of 44.1Khz audio will have 44100 samples regardless of the number
        uint64_t totalSamples;    // of channels. A value of zero here means the number of total samples is unknown.
                                  //----------------------------------------------------------------------------------------
        uint32_t audioDataLength; // is not the filelength, is only the length of the audio datablock in bytes
    } FLACMetadataBlock_t;

    typedef struct FLACFrameHeader_t {
                                  // 0 : fixed-blocksize stream; frame header encodes the frame number
        uint8_t blockingStrategy; // 1 : variable-blocksize stream; frame header encodes the sample number
                                  //----------------------------------------------------------------------------------------
                                  // Block size in inter-channel samples:
                                  // 0000 : reserved
                                  // 0001 : 192 samples
                                  // 0010-0101 : 576 * (2^(n-2)) samples, i.e. 576/1152/2304/4608
                                  // 0110 : get 8 bit (blocksize-1) from end of header
                                  // 0111 : get 16 bit (blocksize-1) from end of header
        uint8_t blockSizeCode;    // 1000-1111 : 256 * (2^(n-8)) samples, i.e. 256/512/1024/2048/4096/8192/16384/32768
                                  //----------------------------------------------------------------------------------------
                                  // 0000 : get from STREAMINFO metadata block
                                  // 0001 : 88.2kHz
                                  // 0010 : 176.4kHz
                                  // 0011 : 192kHz
                                  // 0100 : 8kHz
                                  // 0101 : 16kHz
                                  // 0110 : 22.05kHz
                                  // 0111 : 24kHz
                                  // 1000 : 32kHz
                                  // 1001 : 44.1kHz
                                  // 1010 : 48kHz
                                  // 1011 : 96kHz
                                  // 1100 : get 8 bit sample rate (in kHz) from end of header
                                  // 1101 : get 16 bit sample rate (in Hz) from end of header
                                  // 1110 : get 16 bit sample rate (in tens of Hz) from end of header
        uint8_t sampleRateCode;   // 1111 : invalid, to prevent sync-fooling string of 1s
                                  //----------------------------------------------------------------------------------------
                                  // Channel assignment
                                  // 0000 1 channel: mono
                                  // 0001 2 channels: left, right
                                  // 0010 3 channels
                                  // 0011 4 channels
                                  // 0100 5 channels
                                  // 0101 6 channels
                                  // 0110 7 channels
                                  // 0111 8 channels
                                  // 1000 : left/side stereo: channel 0 is the left channel, channel 1 is the side(difference) channel
                                  // 1001 : right/side stereo: channel 0 is the side(difference) channel, channel 1 is the right channel
                                  // 1010 : mid/side stereo: channel 0 is the mid(average) channel, channel 1 is the side(difference) channel
        uint8_t chanAsgn;         // 1011-1111 : reserved
                                  //----------------------------------------------------------------------------------------
                                  // Sample size in bits:
                                  // 000 : get from STREAMINFO metadata block
                                  // 001 : 8 bits per sample
                                  // 010 : 12 bits per sample
                                  // 011 : reserved
                                  // 100 : 16 bits per sample
                                  // 101 : 20 bits per sample
                                  // 110 : 24 bits per sample
        uint8_t sampleSizeCode;   // 111 : reserved
                                  //----------------------------------------------------------------------------------------
        uint32_t totalSamples;    // totalSamplesInStream
                                  //----------------------------------------------------------------------------------------
        uint32_t bitrate;         // bitrate
    } FLACFrameHeader_t;

    const std::deque<std::deque<int>> FIXED_PREDICTION_COEFFICIENTS = {
        {},            // {}
        {1},           // {1}
        {2, -1},       // {2, -1}
        {3, -3, 1},    // {3, -3, 1}
        {4, -6, 4, -1} // {4, -6, 4, -1}
    };

//    std::deque<int> coefs;

    ps_ptr<FLACFrameHeader_t>   FLACFrameHeader;
    ps_ptr<FLACMetadataBlock_t> FLACMetadataBlock;

    std::vector<uint32_t> m_flacSegmTableVec;
    std::vector<int32_t>  coefs;
    std::vector<uint32_t> m_flacBlockPicItem;

    uint64_t        m_flac_bitBuffer = 0;
    uint32_t        m_flacBitrate = 0;
    uint32_t        m_flacBlockPicLenUntilFrameEnd = 0;
    uint32_t        m_flacCurrentFilePos = 0;
    uint32_t        m_flacBlockPicPos = 0;
    uint32_t        m_flacBlockPicLen = 0;
    uint32_t        m_segmLength = 0;
    uint32_t        m_flacAudioDataStart = 0;
    int32_t         m_flacRemainBlockPicLen = 0;
    uint32_t        m_segmLenTmp = 0;
    uint16_t        m_numOfOutSamples = 0;
    uint16_t        m_flacValidSamples = 0;
    uint16_t        m_rIndex = 0;
    uint16_t        m_offset = 0;
    uint8_t         m_flacStatus = 0;
    uint8_t*        m_flacInptr;
    float           m_flacCompressionRatio = 0;
    uint8_t         m_flacBitBufferLen = 0;
    bool            m_f_flacParseOgg = false;
    bool            m_f_bitReaderError = false;
    uint8_t         m_flac_pageSegments = 0;
    ps_ptr<char>    m_flacStreamTitle = {};
    ps_ptr<char>    m_flacVendorString = {};
    bool            m_f_flacNewStreamtitle = false;
    bool            m_f_flacFirstCall = true;
    bool            m_f_oggWrapper = false;
    bool            m_f_lastMetaDataBlock = false;
    bool            m_f_flacNewMetadataBlockPicture = false;
    bool            m_valid = false;
    uint8_t         m_flacPageNr = 0;
    ps_ptr<int64_t> m_samplesBuffer[2];
    uint16_t        m_maxBlocksize = FLAC_MAX_BLOCKSIZE;
    int32_t         m_nBytes = 0;

    boolean  FLACFindMagicWord(unsigned char* buf, int32_t nBytes);
    int32_t  parseOGG(uint8_t* inbuf, int32_t* bytesLeft);
    int32_t  parseFlacFirstPacket(uint8_t* inbuf, int16_t nBytes);
    int32_t  parseMetaDataBlockHeader(uint8_t* inbuf, int16_t nBytes);
    void     setDefaults();
    void     decoderReset();
    int8_t   decodeNative(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf);
    int8_t   decodeFrame(uint8_t* inbuf, int32_t* bytesLeft);
    uint64_t getTotoalSamplesInStream();
    uint32_t readUint(uint8_t nBits, int32_t* bytesLeft);
    void     alignToByte();
    int8_t   decodeSubframes(int32_t* bytesLeft);
    int8_t   decodeSubframe(uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft);
    int8_t   decodeFixedPredictionSubframe(uint8_t predOrder, uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft);
    int8_t   decodeLinearPredictiveCodingSubframe(int32_t lpcOrder, int32_t sampleDepth, uint8_t ch, int32_t* bytesLeft);
    int8_t   decodeResiduals(uint8_t warmup, uint8_t ch, int32_t* bytesLeft);
    void     restoreLinearPrediction(uint8_t ch, uint8_t shift);
    int32_t  specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact = false);


    inline int32_t readSignedInt(int32_t nBits, int32_t* bytesLeft) {
        int32_t temp = readUint(nBits, bytesLeft) << (32 - nBits);
        temp = temp >> (32 - nBits); // The C++ compiler uses the sign bit to fill vacated bit positions
        return temp;
    }

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // Macro for comfortable calls
    #define FLAC_LOG_ERROR(fmt, ...)   Audio::AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define FLAC_LOG_WARN(fmt, ...)    Audio::AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define FLAC_LOG_INFO(fmt, ...)    Audio::AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define FLAC_LOG_DEBUG(fmt, ...)   Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define FLAC_LOG_VERBOSE(fmt, ...) Audio::AUDIO_LOG_IMPL(5, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
