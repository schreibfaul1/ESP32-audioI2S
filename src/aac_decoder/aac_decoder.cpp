/*
 *  aac_decoder.cpp
 *  faad2 - ESP32 adaptation
 *  Created on: 12.09.2023
 *  Updated on: 17.10.2025
 */

#include "aac_decoder.h"
#include "Arduino.h"
#include "libfaad/neaacdec.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
AACDecoder::AACDecoder(Audio& audioRef) : Decoder(audioRef), audio(audioRef), m_neaacdec(std::make_unique<NeaacDecoder>()) {}

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool AACDecoder::init() {
    m_hAac = m_neaacdec->NeAACDecOpen();
    m_conf = m_neaacdec->NeAACDecGetCurrentConfiguration(m_hAac);
    m_out16.alloc_array(4608 * 2, "m_out16");

    if (m_hAac && m_out16.valid()) m_f_decoderIsInit = true;
    m_f_firstCall = false;
    m_f_setRaWBlockParams = false;
    return m_f_decoderIsInit;
}
void AACDecoder::clear() {
    m_out16.clear();
    return; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void AACDecoder::reset() {
    m_neaacdec->NeAACDecClose(m_hAac);
    m_hAac = NULL;
    m_f_decoderIsInit = false;
    m_f_firstCall = false;
    m_out16.reset();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool AACDecoder::isValid() {
    return m_f_decoderIsInit;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t AACDecoder::findSyncWord(uint8_t* buf, int32_t nBytes) {
    const int MIN_ADTS_HEADER_SIZE = 7;
    auto      validate = [](const uint8_t* buf) -> bool { // check the ADTS header for validity
        // Layer (bits 14-15) must be 00
        if ((buf[1] & 0x06) != 0x00) { return false; }

        // Sampling Frequency Index (Bits 18-21) cannot be invalid
        uint8_t sampling_frequency_index = (buf[2] & 0x3C) >> 2;
        if (sampling_frequency_index > 12) { return false; }

        // Frame length (bits 30-42) must be at least the header size
        int frame_length = ((buf[3] & 0x03) << 11) | (buf[4] << 3) | ((buf[5] & 0xE0) >> 5);
        if (frame_length < MIN_ADTS_HEADER_SIZE) { return false; }

        return true;
    };

    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (int i = 0; i < nBytes - 1; i++) {
        if ((buf[i + 0] & SYNCWORDH) == SYNCWORDH && (buf[i + 1] & SYNCWORDL) == SYNCWORDL) {
            int frame_length = ((buf[i + 3] & 0x03) << 11) | (buf[i + 4] << 3) | ((buf[i + 5] & 0xE0) >> 5);
            if (i + frame_length + MIN_ADTS_HEADER_SIZE > nBytes) {
                return -1; // Puffergrenze überschritten, kein gültiger Header
            }
            /* find a second byte-aligned syncword (12 bits = 0xFFF) */
            if ((buf[i + frame_length + 0] & SYNCWORDH) == SYNCWORDH && (buf[i + frame_length + 1] & SYNCWORDL) == SYNCWORDL) { return validate(&buf[i]) ? i : -1; }
        }
    }

    return -1;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t AACDecoder::getChannels() {
    return m_aacChannels;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t AACDecoder::getSampleRate() {
    return m_aacSamplerate;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t AACDecoder::getOutputSamples() {
    return m_validSamples;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t AACDecoder::getBitsPerSample() {
    return 16;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t AACDecoder::getBitRate() {
    uint32_t br = getBitsPerSample() * getChannels() * getSampleRate();
    return (br / m_compressionRatio);
    ;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t AACDecoder::getAudioDataStart() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t AACDecoder::getAudioFileDuration() {
    return 0; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* AACDecoder::getStreamTitle() {
    return nullptr; // nothing todo
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* AACDecoder::whoIsIt() {
    return "AAC";
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* AACDecoder::getErrorMessage(int8_t err) {
    return m_neaacdec->NeAACDecGetErrorMessage(abs(err));
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t AACDecoder::decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf) {

    uint8_t* ob = (uint8_t*)m_out16.get();
    if (m_f_firstCall == false) {
        if (m_f_setRaWBlockParams) { // set raw AAC values, e.g. for M4A config.
            m_f_setRaWBlockParams = false;
            m_conf->defSampleRate = m_aacSamplerate;
            m_conf->outputFormat = FAAD_FMT_16BIT;
            m_conf->useOldADTSFormat = 1;
            m_conf->defObjectType = 2;
            int8_t ret = m_neaacdec->NeAACDecSetConfiguration(m_hAac, m_conf);
            (void)ret;

            uint8_t specificInfo[2];
            createAudioSpecificConfig(specificInfo, m_aacProfile, m_neaacdec->get_sr_index(m_aacSamplerate), m_aacChannels);
            int8_t err = m_neaacdec->NeAACDecInit2(m_hAac, specificInfo, 2, &m_aacSamplerate, &m_aacChannels);
            (void)err;
        } else {
            m_neaacdec->NeAACDecSetConfiguration(m_hAac, m_conf);
            int8_t err = m_neaacdec->NeAACDecInit(m_hAac, inbuf, *bytesLeft, &m_aacSamplerate, &m_aacChannels);
            (void)err;
        }
        m_f_firstCall = true;
    }

    m_neaacdec->NeAACDecDecode2(m_hAac, &m_frameInfo, inbuf, *bytesLeft, (void**)&ob, 2048 * 2 * sizeof(int16_t));
    *bytesLeft -= m_frameInfo.bytesconsumed;
    m_validSamples = m_frameInfo.samples;
    int8_t err = 0 - m_frameInfo.error;
    m_compressionRatio = (float)m_frameInfo.samples * 2 / m_frameInfo.bytesconsumed;
    if (err < 0) {
        if (err == -100) return AAC_ID3_HDR; // ID3 header found
        if (err == -21) {
            AAC_LOG_INFO("%s", getErrorMessage(abs(err)));
        } else {
            AAC_LOG_ERROR("%s", getErrorMessage(abs(err)));
        }
    } else {

        if (m_aacChannels == 1) {
            for (int i = 0; i < m_validSamples; i++) {
                outbuf[i * 2] = m_out16[i] << 16;
                outbuf[i * 2 + 1] = m_out16[i] << 16;
            }
        }

        if (m_aacChannels == 2) {
            for (int i = 0; i < m_validSamples * 2; i++) { outbuf[i] = m_out16[i] << 16; }
        }
    }
    return err;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void AACDecoder::setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t dummy1, uint32_t profile, uint32_t dummy2) {
    m_f_setRaWBlockParams = true;
    m_aacChannels = channels;     // 1: Mono, 2: Stereo
    m_aacSamplerate = sampleRate; // 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
    m_aacProfile = profile;       // 1: AAC Main, 2: AAC LC (Low Complexity), 3: AAC SSR (Scalable Sample Rate), 4: AAC LTP (Long Term Prediction)
    return;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
std::vector<uint32_t> AACDecoder::getMetadataBlockPicture() {
    std::vector<uint32_t> a;
    return a;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* AACDecoder::arg1() { // AAC format
    m_arg1.assign("AAC HeaderFormat: ");
    if (m_frameInfo.header_type == 0)
        m_arg1.append("RAW");
    else if (m_frameInfo.header_type == 1)
        m_arg1.append("ADIF"); /* single ADIF header at the beginning of the file */
    else if (m_frameInfo.header_type == 2)
        m_arg1.append("ADTS"); /* ADTS header at the beginning of each frame */
    else
        m_arg1.append("unknown");
    return m_arg1.c_get();
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
const char* AACDecoder::arg2() {
    if (m_frameInfo.sbr == 1) return "upsampled SBR";
    if (m_frameInfo.sbr == 2) return "downsampled SBR";
    if (m_frameInfo.sbr == 3) return "no SBR used, but file is upsampled by a factor 2";
    return "without SBR";
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t AACDecoder::val1() { // Parametric Stereo
    return m_frameInfo.isPS;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t AACDecoder::val2() { // Spectral Band Replication
    return m_frameInfo.sbr;  // NO_SBR           0 /* no SBR used in this file */
                             // SBR_UPSAMPLED    1 /* upsampled SBR used */
                             // SBR_DOWNSAMPLED  2 /* downsampled SBR used */
                             // NO_SBR_UPSAMPLED 3 /* no SBR used, but file is upsampled by a factor 2 anyway */
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void AACDecoder::createAudioSpecificConfig(uint8_t* config, uint8_t audioObjectType, uint8_t samplingFrequencyIndex, uint8_t channelConfiguration) {
    config[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1);
    config[1] = (samplingFrequencyIndex << 7) | (channelConfiguration << 3);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// extern uint8_t NeaacDecoder::get_sr_index(const uint32_t samplerate);
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
