#pragma once
#include "../Audio.h"

class WavDecoder : public Decoder {

  public:
    WavDecoder(Audio& audioRef) : Decoder(audioRef), audio(audioRef) {}
    ~WavDecoder() { reset(); }
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

  private:
    Audio&   audio;
    bool     m_valid = false;
    uint8_t  m_channels = 0;
    uint8_t  m_bits_per_sample = 0;
    uint32_t m_sampleRate = 0;
    uint32_t m_validSamples = 0;
};