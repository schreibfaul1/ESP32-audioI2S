/*
 *  aac_decoder.h
 *  faad2 - ESP32 adaptation
 *  Created on: 12.09.2023
 *  Updated on: 13.08.2024
 */

#pragma once

#include "../Audio.h"
#include "libfaad/neaacdec.h"

#pragma GCC diagnostic warning "-Wunused-function"

class AACDecoder : public Decoder {

  public:
    enum : int8_t {
        AAC_ID3_HDR = 100,
        AAC_NONE = 0,
        AAC_ERR = -1,
    };

    AACDecoder(Audio& audioRef);
    ~AACDecoder() { reset(); }
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
    virtual int32_t       val1() override; // Paramertric Stereo
    virtual int32_t       val2() override; // SBR

  private:
    Audio&       audio;
    ps_ptr<char> m_arg1;
    ps_ptr<int16_t> m_out16;
    void         createAudioSpecificConfig(uint8_t* config, uint8_t audioObjectType, uint8_t samplingFrequencyIndex, uint8_t channelConfiguration);
    const char*  getErrorMessage(int8_t err);

    NeAACDecHandle                m_hAac;
    NeAACDecFrameInfo             m_frameInfo;
    NeAACDecConfigurationPtr      m_conf;
    const uint8_t                 SYNCWORDH = 0xff; /* 12-bit syncword */
    const uint8_t                 SYNCWORDL = 0xf0;
    bool                          m_f_decoderIsInit = false;
    bool                          m_f_firstCall = false;
    bool                          m_f_setRaWBlockParams = false;
    uint32_t                      m_aacSamplerate = 0;
    uint8_t                       m_aacChannels = 0;
    uint8_t                       m_aacProfile = 0;
    uint16_t                      m_validSamples = 0;
    float                         m_compressionRatio = 1;
    std::unique_ptr<NeaacDecoder> m_neaacdec;

    struct AudioSpecificConfig {
        uint8_t audioObjectType;
        uint8_t samplingFrequencyIndex;
        uint8_t channelConfiguration;
    };

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//  Macro for comfortable calls
#define AAC_LOG_ERROR(fmt, ...)   Audio::AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_WARN(fmt, ...)    Audio::AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_INFO(fmt, ...)    Audio::AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_DEBUG(fmt, ...)   Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_VERBOSE(fmt, ...) Audio::AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
};