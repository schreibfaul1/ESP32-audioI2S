/*
 *  aac_decoder.cpp
 *  faad2 - ESP32 adaptation
 *  Created on: 12.09.2023
 *  Updated on: 14.01.2025
 */

#include "aac_decoder.h"
#include "Arduino.h"
#include "libfaad/neaacdec.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

auto neaacdec = std::make_unique<NeaacDecoder>();

//----------------------------------------------------------------------------------------------------------------------
bool AACDecoder::init() {
    before = clock();
    hAac = neaacdec->NeAACDecOpen();
    conf = neaacdec->NeAACDecGetCurrentConfiguration(hAac);

    if (hAac) f_decoderIsInit = true;
    f_firstCall = false;
    f_setRaWBlockParams = false;
    return f_decoderIsInit;
}
void AACDecoder::clear() {
    return; // nothing todo
}
//----------------------------------------------------------------------------------------------------------------------
void AACDecoder::reset() {
    neaacdec->NeAACDecClose(hAac);
    hAac = NULL;
    f_decoderIsInit = false;
    f_firstCall = false;
    clock_t difference = clock() - before;
    int     msec = difference / CLOCKS_PER_SEC;
    (void)msec;
    //    printf("ms %li\n", difference);
}
//----------------------------------------------------------------------------------------------------------------------
bool AACDecoder::isValid() {
    return f_decoderIsInit;
}
//----------------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------------
uint8_t AACDecoder::getChannels() {
    return aacChannels;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t AACDecoder::getSampleRate() {
    return aacSamplerate;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t AACDecoder::getOutputSamples() {
    return validSamples;
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t AACDecoder::getBitsPerSample() {
    return 16;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t AACDecoder::getBitRate() {
    uint32_t br = getBitsPerSample() * getChannels() * getSampleRate();
    return (br / compressionRatio);
    ;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t AACDecoder::getAudioDataStart() {
    return 0; // nothing todo
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t AACDecoder::getAudioFileDuration() {
    return 0; // nothing todo
}
//----------------------------------------------------------------------------------------------------------------------
const char* AACDecoder::getStreamTitle() {
    return nullptr; // nothing todo
}
//----------------------------------------------------------------------------------------------------------------------
const char* AACDecoder::getErrorMessage(int8_t err) {
    return neaacdec->NeAACDecGetErrorMessage(abs(err));
}
//----------------------------------------------------------------------------------------------------------------------
int32_t AACDecoder::decode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf) {
    uint8_t* ob = (uint8_t*)outbuf;
    if (f_firstCall == false) {
        if (f_setRaWBlockParams) { // set raw AAC values, e.g. for M4A config.
            f_setRaWBlockParams = false;
            conf->defSampleRate = aacSamplerate;
            conf->outputFormat = FAAD_FMT_16BIT;
            conf->useOldADTSFormat = 1;
            conf->defObjectType = 2;
            int8_t ret = neaacdec->NeAACDecSetConfiguration(hAac, conf);
            (void)ret;

            uint8_t specificInfo[2];
            createAudioSpecificConfig(specificInfo, aacProfile, neaacdec->get_sr_index(aacSamplerate), aacChannels);
            int8_t err = neaacdec->NeAACDecInit2(hAac, specificInfo, 2, &aacSamplerate, &aacChannels);
            (void)err;
        } else {
            neaacdec->NeAACDecSetConfiguration(hAac, conf);
            int8_t err = neaacdec->NeAACDecInit(hAac, inbuf, *bytesLeft, &aacSamplerate, &aacChannels);
            (void)err;
        }
        f_firstCall = true;
    }

    neaacdec->NeAACDecDecode2(hAac, &frameInfo, inbuf, *bytesLeft, (void**)&ob, 2048 * 2 * sizeof(int16_t));
    *bytesLeft -= frameInfo.bytesconsumed;
    validSamples = frameInfo.samples;
    int8_t err = 0 - frameInfo.error;
    compressionRatio = (float)frameInfo.samples * 2 / frameInfo.bytesconsumed;
    if (err < 0) AAC_LOG_INFO(getErrorMessage(abs(err)));
    return err;
}
//----------------------------------------------------------------------------------------------------------------------
void AACDecoder::setRawBlockParams(uint8_t channels, uint32_t sampleRate, uint8_t dummy1, uint32_t profile, uint32_t dummy2) {
    f_setRaWBlockParams = true;
    aacChannels = channels;     // 1: Mono, 2: Stereo
    aacSamplerate = sampleRate; // 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
    aacProfile = profile;       // 1: AAC Main, 2: AAC LC (Low Complexity), 3: AAC SSR (Scalable Sample Rate), 4: AAC LTP (Long Term Prediction)
    return;
}
//----------------------------------------------------------------------------------------------------------------------
std::vector<uint32_t> AACDecoder::getMetadataBlockPicture() {
    std::vector<uint32_t> a;
    return a;
}
//----------------------------------------------------------------------------------------------------------------------
const char* AACDecoder::arg1() { // AAC format
    m_arg1.assign("AAC HeaderFormat: ");
    if (frameInfo.header_type == 0) m_arg1.append("RAW");
    if (frameInfo.header_type == 1) m_arg1.append("ADIF"); /* single ADIF header at the beginning of the file */
    if (frameInfo.header_type == 2) m_arg1.append("ADTS"); /* ADTS header at the beginning of each frame */
    return m_arg1.c_get();
}
//----------------------------------------------------------------------------------------------------------------------
const char* AACDecoder::arg2() {
    return "arg2";
}
//----------------------------------------------------------------------------------------------------------------------
int32_t AACDecoder::val1() { //Parametric Stereo
    return frameInfo.isPS;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t AACDecoder::val2() { // Spectral Band Replication
    return frameInfo.sbr; // NO_SBR           0 /* no SBR used in this file */
                          // SBR_UPSAMPLED    1 /* upsampled SBR used */
                          // SBR_DOWNSAMPLED  2 /* downsampled SBR used */
                          // NO_SBR_UPSAMPLED 3 /* no SBR used, but file is upsampled by a factor 2 anyway */
}
//----------------------------------------------------------------------------------------------------------------------
void AACDecoder::createAudioSpecificConfig(uint8_t* config, uint8_t audioObjectType, uint8_t samplingFrequencyIndex, uint8_t channelConfiguration) {
    config[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1);
    config[1] = (samplingFrequencyIndex << 7) | (channelConfiguration << 3);
}
//----------------------------------------------------------------------------------------------------------------------
// extern uint8_t NeaacDecoder::get_sr_index(const uint32_t samplerate);
//----------------------------------------------------------------------------------------------------------------------
