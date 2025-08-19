/*
 *  aac_decoder.h
 *  faad2 - ESP32 adaptation
 *  Created on: 12.09.2023
 *  Updated on: 13.08.2024
*/


#pragma once

#include "Arduino.h"
#include "../psram_unique_ptr.hpp"
#include "libfaad/neaacdec.h"
#pragma GCC diagnostic warning "-Wunused-function"

struct AudioSpecificConfig {
    uint8_t audioObjectType;
    uint8_t samplingFrequencyIndex;
    uint8_t channelConfiguration;
};

bool        AACDecoder_IsInit();
bool        AACDecoder_AllocateBuffers();
void        AACDecoder_FreeBuffers();
uint8_t     AACGetFormat();
uint8_t     AACGetParametricStereo();
uint8_t     AACGetSBR();
int         AACFindSyncWord(uint8_t *buf, int nBytes);
int         AACSetRawBlockParams(int nChans, int sampRateCore, int profile);
int16_t     AACGetOutputSamps();
int         AACGetBitrate();
int         AACGetChannels();
int         AACGetSampRate();
int         AACGetBitsPerSample();
int         AACDecode(uint8_t *inbuf, int32_t *bytesLeft, short *outbuf);
const char* AACGetErrorMessage(int8_t err);

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  L O G G I N G   📌📌📌
extern __attribute__((weak)) void audio_info(const char*);

template <typename... Args>
void AAC_LOG_IMPL(uint8_t level, const char* path, int line, const char* fmt, Args&&... args) {
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
    if(audio_info){
        if     (level == 1 && CORE_DEBUG_LEVEL >= 1) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_RED " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 2 && CORE_DEBUG_LEVEL >= 2) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_YELLOW " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 3 && CORE_DEBUG_LEVEL >= 3) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_GREEN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 4 && CORE_DEBUG_LEVEL >= 4) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_CYAN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);  // debug
        else              if( CORE_DEBUG_LEVEL >= 5) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_WHITE " %s" ANSI_ESC_RESET, file.c_get(), line, dst); // verbose
        if(final.strlen()) audio_info(final.get());
    }
    else{
        std::snprintf(dest, total_len + 1, "%s:%d: %s", file.c_get(), line, dst);
        if     (level == 1) log_e("%s", final.c_get());
        else if(level == 2) log_w("%s", final.c_get());
        else if(level == 3) log_i("%s", final.c_get());
        else if(level == 4) log_d("%s", final.c_get());
        else                log_v("%s", final.c_get());
    }
    final.reset();
    result.reset();
}

// Macro for comfortable calls
#define AAC_LOG_ERROR(fmt, ...)   AAC_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_WARN(fmt, ...)    AAC_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_INFO(fmt, ...)    AAC_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_DEBUG(fmt, ...)   AAC_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AAC_LOG_VERBOSE(fmt, ...) AAC_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)