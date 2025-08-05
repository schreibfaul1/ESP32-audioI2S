
/*****************************************************************************************************************************************************
    audio.cpp

    Created on: Oct 28.2018                                                                                                  */char audioI2SVers[] ="\
    Version 3.4.1                                                                                                                                ";
/*  Updated on: Aug 05.2025

    Author: Wolle (schreibfaul1)
    Audio library for ESP32, ESP32-S3 or ESP32-P4
    Arduino Vers. V3 is mandatory
    PSRAM is mandatory
    external DAC is mandatory

*****************************************************************************************************************************************************/

#include "Audio.h"
#include "aac_decoder/aac_decoder.h"
#include "flac_decoder/flac_decoder.h"
#include "mp3_decoder/mp3_decoder.h"
#include "opus_decoder/opus_decoder.h"
#include "vorbis_decoder/vorbis_decoder.h"
#include "psram_unique_ptr.hpp"

template <typename... Args>
void AUDIO_INFO(const char* fmt, Args&&... args) {
    ps_ptr<char> result;

    // First run: determine size
    int len = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
    if (len <= 0) return;

    result.alloc(len + 1, "result");
    result.clear();
    char* dst = result.get();
    if (!dst) return;  // Or error treatment
    std::snprintf(dst, len + 1, fmt, std::forward<Args>(args)...);
    result.append(ANSI_ESC_RESET);
    if(audio_info) audio_info(result.c_get());
    result.reset();
}

template <typename... Args>
void AUDIO_LOG_IMPL(uint8_t level, const char* path, int line, const char* fmt, Args&&... args) {

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
    final.alloc(total_len + 1, "final");
    final.clear();
    char* dest = final.get();
    if (!dest) return;  // or error treatment
    if(audio_info){
        if     (level == 1 && CORE_DEBUG_LEVEL >= 1) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_RED " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 2 && CORE_DEBUG_LEVEL >= 2) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_YELLOW " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 3 && CORE_DEBUG_LEVEL >= 3) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_GREEN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);
        else if(level == 4 && CORE_DEBUG_LEVEL >= 4) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_CYAN " %s" ANSI_ESC_RESET, file.c_get(), line, dst);  // debug
        else              if( CORE_DEBUG_LEVEL >= 5) snprintf(dest, total_len + 1, "%s:%d:" ANSI_ESC_WHITE " %s" ANSI_ESC_RESET, file.c_get(), line, dst); // verbose
        if(final.strlen() > 0)  audio_info(final.get());
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
#define AUDIO_LOG_ERROR(fmt, ...) AUDIO_LOG_IMPL(1, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AUDIO_LOG_WARN(fmt, ...)  AUDIO_LOG_IMPL(2, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AUDIO_LOG_INFO(fmt, ...)  AUDIO_LOG_IMPL(3, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AUDIO_LOG_DEBUG(fmt, ...) AUDIO_LOG_IMPL(4, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AudioBuffer::AudioBuffer(size_t maxBlockSize) {
    if(maxBlockSize) m_resBuffSize = maxBlockSize;
    if(maxBlockSize) m_maxBlockSize = maxBlockSize;
}

AudioBuffer::~AudioBuffer() {
    if(m_buffer) free(m_buffer);
    m_buffer = NULL;
}

int32_t AudioBuffer::getBufsize() { return m_buffSize; }

bool AudioBuffer::setBufsize(size_t mbs) {
    if(mbs < 2 * m_resBuffSize) {
        AUDIO_LOG_ERROR("not allowed buffer size must be greater than %i", 2 * m_resBuffSize);
        return false;
    }
    m_buffSize = mbs;
    if(!init()) return false;
    return true;
}

size_t AudioBuffer::init() {
    if(m_buffer) free(m_buffer);
    m_buffer = NULL;
    m_buffer = (uint8_t*)ps_malloc(m_buffSize + m_resBuffSize);

    if(!m_buffer) return 0;
    m_f_init = true;
    resetBuffer();
    return m_buffSize;
}

void AudioBuffer::changeMaxBlockSize(uint16_t mbs) {
    m_maxBlockSize = mbs;
    return;
}

uint16_t AudioBuffer::getMaxBlockSize() { return m_maxBlockSize; }

size_t AudioBuffer::freeSpace() {
    if(m_readPtr == m_writePtr) {
        if(m_f_isEmpty == true) m_freeSpace = m_buffSize;
        else m_freeSpace = 0;
    }
    if(m_readPtr < m_writePtr) {
        m_freeSpace = (m_endPtr - m_writePtr) + (m_readPtr - m_buffer);
    }
    if(m_readPtr > m_writePtr) {
        m_freeSpace = m_readPtr - m_writePtr;
    }
    return m_freeSpace;
}

size_t AudioBuffer::writeSpace() {
    if(m_readPtr == m_writePtr) {
        if(m_f_isEmpty == true) m_writeSpace = m_endPtr - m_writePtr;
        else m_writeSpace = 0;
    }
    if(m_readPtr < m_writePtr) {
        m_writeSpace = m_endPtr - m_writePtr;
    }
    if(m_readPtr > m_writePtr) {
        m_writeSpace = m_readPtr - m_writePtr;
    }
    return m_writeSpace;
}

size_t AudioBuffer::bufferFilled() {
    if(m_readPtr == m_writePtr) {
        if(m_f_isEmpty == true) m_dataLength = 0;
        else m_dataLength = m_buffSize;
    }
    if(m_readPtr < m_writePtr) {
        m_dataLength = m_writePtr - m_readPtr;
    }
    if(m_readPtr > m_writePtr) {
        m_dataLength = (m_endPtr - m_readPtr) + (m_writePtr - m_buffer);
    }
    return m_dataLength;
}

size_t AudioBuffer::getMaxAvailableBytes() {
    if(m_readPtr == m_writePtr) {
    //   if(m_f_start)m_dataLength = 0;
        if(m_f_isEmpty == true) m_dataLength = 0;
        else m_dataLength = (m_endPtr - m_readPtr);
    }
    if(m_readPtr < m_writePtr) {
        m_dataLength = m_writePtr - m_readPtr;
    }
    if(m_readPtr > m_writePtr) {
        m_dataLength = (m_endPtr - m_readPtr);
    }
    return m_dataLength;
}

void AudioBuffer::bytesWritten(size_t bw) {
    if(!bw) return;
    m_writePtr += bw;
    if(m_writePtr == m_endPtr) { m_writePtr = m_buffer; }
    if(m_writePtr > m_endPtr) AUDIO_LOG_ERROR("AudioBuffer: m_writePtr %i > m_endPtr %i", m_writePtr, m_endPtr);
    m_f_isEmpty = false;
}

void AudioBuffer::bytesWasRead(size_t br) {
    if(!br) return;
    m_readPtr += br;
    if(m_readPtr >= m_endPtr) {
        size_t tmp = m_readPtr - m_endPtr;
        m_readPtr = m_buffer + tmp;
    }
    if(m_readPtr == m_writePtr) m_f_isEmpty = true;
}

uint8_t* AudioBuffer::getWritePtr() { return m_writePtr; }

uint8_t* AudioBuffer::getReadPtr() {
    int32_t len = m_endPtr - m_readPtr;
    if(len < m_maxBlockSize) {                            // be sure the last frame is completed
        memcpy(m_endPtr, m_buffer, m_maxBlockSize - (len)); // cpy from m_buffer to m_endPtr with len
    }
    return m_readPtr;
}

void AudioBuffer::resetBuffer() {
    m_writePtr = m_buffer;
    m_readPtr = m_buffer;
    m_endPtr = m_buffer + m_buffSize;
    m_f_isEmpty = true;
}

uint32_t AudioBuffer::getWritePos() { return m_writePtr - m_buffer; }

uint32_t AudioBuffer::getReadPos() { return m_readPtr - m_buffer; }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// clang-format off
Audio::Audio(uint8_t i2sPort) {

    mutex_playAudioData = xSemaphoreCreateMutex();
    mutex_audioTask     = xSemaphoreCreateMutex();

    if(!psramFound()) AUDIO_LOG_ERROR("audioI2S requires PSRAM!");

    clientsecure.setInsecure();
    m_i2s_num = i2sPort;  // i2s port number

    // -------- I2S configuration -------------------------------------------------------------------------------------------
    m_i2s_chan_cfg.id            = (i2s_port_t)m_i2s_num;  // I2S_NUM_AUTO, I2S_NUM_0, I2S_NUM_1
    m_i2s_chan_cfg.role          = I2S_ROLE_MASTER;        // I2S controller master role, bclk and lrc signal will be set to output
    m_i2s_chan_cfg.dma_desc_num  = 16;                     // number of DMA buffer
    m_i2s_chan_cfg.dma_frame_num = 512;                    // I2S frame number in one DMA buffer.
    m_i2s_chan_cfg.auto_clear    = true;                   // i2s will always send zero automatically if no data to send
    i2s_new_channel(&m_i2s_chan_cfg, &m_i2s_tx_handle, NULL);

    m_i2s_std_cfg.slot_cfg                = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO); // Set to enable bit shift in Philips mode
    m_i2s_std_cfg.gpio_cfg.bclk           = I2S_GPIO_UNUSED;           // BCLK, Assignment in setPinout()
    m_i2s_std_cfg.gpio_cfg.din            = I2S_GPIO_UNUSED;           // not used
    m_i2s_std_cfg.gpio_cfg.dout           = I2S_GPIO_UNUSED;           // DOUT, Assignment in setPinout()
    m_i2s_std_cfg.gpio_cfg.mclk           = I2S_GPIO_UNUSED;           // MCLK, Assignment in setPinout()
    m_i2s_std_cfg.gpio_cfg.ws             = I2S_GPIO_UNUSED;           // LRC,  Assignment in setPinout()
    m_i2s_std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    m_i2s_std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    m_i2s_std_cfg.gpio_cfg.invert_flags.ws_inv   = false;
    m_i2s_std_cfg.clk_cfg.sample_rate_hz = 48000;
    m_i2s_std_cfg.clk_cfg.clk_src        = I2S_CLK_SRC_DEFAULT;        // Select PLL_F160M as the default source clock
    m_i2s_std_cfg.clk_cfg.mclk_multiple  = I2S_MCLK_MULTIPLE_128;      // mclk = sample_rate * 256
    i2s_channel_init_std_mode(m_i2s_tx_handle, &m_i2s_std_cfg);
    I2Sstart();
    m_sampleRate = m_i2s_std_cfg.clk_cfg.sample_rate_hz;

    for(int i = 0; i < 3; i++) {
        m_filter[i].a0 = 1;
        m_filter[i].a1 = 0;
        m_filter[i].a2 = 0;
        m_filter[i].b1 = 0;
        m_filter[i].b2 = 0;
    }
    computeLimit();  // first init, vol = 21, vol_steps = 21
    startAudioTask();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Audio::~Audio() {
    stopSong();
    setDefaults();

    i2s_channel_disable(m_i2s_tx_handle);
    i2s_del_channel(m_i2s_tx_handle);
    stopAudioTask();
    vSemaphoreDelete(mutex_playAudioData);
    vSemaphoreDelete(mutex_audioTask);
}
// clang-format on
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::initInBuff() {
    if(!InBuff.isInitialized()) {
        size_t size = InBuff.init();
        if(size > 0) {
            AUDIO_INFO("inputBufferSize: %u bytes", size - 1);
        }
    }
    InBuff.changeMaxBlockSize(1600); // default size mp3 or aac
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::I2Sstart() {
    zeroI2Sbuff();
    return i2s_channel_enable(m_i2s_tx_handle);
}

esp_err_t Audio::I2Sstop() {
    m_outBuff.clear(); // Clear OutputBuffer
    m_samplesBuff48K.clear(); // Clear samplesBuff48K
    std::fill(std::begin(m_inputHistory), std::end(m_inputHistory), 0); // Clear history in samplesBuff48K
    return i2s_channel_disable(m_i2s_tx_handle);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::zeroI2Sbuff(){
    uint8_t buff[2] = {0, 0}; // From IDF V5 there is no longer the zero_dma_buff() function.
    size_t bytes_loaded = 0;  // As a replacement, we write a small amount of zeros in the buffer and thus reset the entire buffer.
    i2s_channel_preload_data(m_i2s_tx_handle, buff, 2, &bytes_loaded);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void Audio::setDefaults() {
    stopSong();
    initInBuff(); // initialize InputBuffer if not already done
    InBuff.resetBuffer();
    MP3Decoder_FreeBuffers();
    FLACDecoder_FreeBuffers();
    AACDecoder_FreeBuffers();
    OPUSDecoder_FreeBuffers();
    VORBISDecoder_FreeBuffers();
    m_outBuff.clear(); // Clear OutputBuffer
    m_samplesBuff48K.clear(); // Clear samplesBuff48K
    vector_clear_and_shrink(m_playlistURL);
    vector_clear_and_shrink(m_playlistContent);
    vector_clear_and_shrink(m_syltLines);
    m_syltTimeStamp.clear();
    m_hashQueue.clear();
    m_hashQueue.shrink_to_fit(); // uint32_t vector
    client.stop();
    clientsecure.stop();
    m_client = static_cast<WiFiClient*>(&client); /* default to *something* so that no NULL deref can happen */
    ts_parsePacket(0, 0, 0);                     // reset ts routine
    m_lastM3U8host.reset();

    AUDIO_INFO("buffers freed, free Heap: %lu bytes", (long unsigned int)ESP.getFreeHeap());

    m_f_timeout = false;
    m_f_chunked = false; // Assume not chunked
    m_f_firstmetabyte = false;
    m_f_playing = false;
//    m_f_ssl = false;
    m_f_metadata = false;
    m_f_tts = false;
    m_f_firstCall = true;        // InitSequence for processWebstream and processLocalFile
    m_f_firstCurTimeCall = true; // InitSequence for computeAudioTime
    m_f_firstM3U8call = true;    // InitSequence for parsePlaylist_M3U8
    m_f_firstPlayCall = true;    // InitSequence for playAudioData
//    m_f_running = false;       // already done in stopSong
    m_f_unsync = false;   // set within ID3 tag but not used
    m_f_exthdr = false;   // ID3 extended header
    m_f_rtsp = false;     // RTSP (m3u8)stream
    m_f_m3u8data = false; // set again in processM3U8entries() if necessary
    m_f_continue = false;
    m_f_ts = false;
    m_f_ogg = false;
    m_f_m4aID3dataAreRead = false;
    m_f_stream = false;
    m_f_decode_ready = false;
    m_f_eof = false;
    m_f_ID3v1TagFound = false;
    m_f_lockInBuffer = false;
    m_f_acceptRanges = false;
    m_f_connectionClose = false;

    m_streamType = ST_NONE;
    m_codec = CODEC_NONE;
    m_playlistFormat = FORMAT_NONE;
    m_dataMode = AUDIO_NONE;
    m_streamTitle.assign("");
    m_resumeFilePos = -1;
    m_audioCurrentTime = 0; // Reset playtimer
    m_audioFileDuration = 0;
    m_audioDataStart = 0;
    m_audioDataSize = 0;
    m_audioFileSize = 0;
    m_avr_bitrate = 0;     // the same as m_bitrate if CBR, median if VBR
    m_bitRate = 0;         // Bitrate still unknown
    m_bytesNotConsumed = 0; // counts all not decodable bytes
    m_chunkcount = 0;      // for chunked streams
    m_curSample = 0;
    m_metaint = 0;        // No metaint yet
    m_LFcount = 0;        // For end of header detection
    m_controlCounter = 0; // Status within readID3data() and readWaveHeader()
    m_channels = 2;       // assume stereo #209
    m_ID3Size = 0;
    m_haveNewFilePos = 0;
    m_validSamples = 0;
    m_M4A_chConfig = 0;
    m_M4A_objectType = 0;
    m_M4A_sampleRate = 0;
    m_opus_mode = 0;
    m_vuLeft = m_vuRight = 0; // #835
    std::fill(std::begin(m_inputHistory), std::end(m_inputHistory), 0);
    if(m_f_reset_m3u8Codec){m_m3u8Codec = CODEC_AAC;} // reset to default
    m_f_reset_m3u8Codec = true;
    m_resampleCursor = 0.0f;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setConnectionTimeout(uint16_t timeout_ms, uint16_t timeout_ms_ssl) {
    if(timeout_ms) m_timeout_ms = timeout_ms;
    if(timeout_ms_ssl) m_timeout_ms_ssl = timeout_ms_ssl;
}

/*
    Text to speech API provides a speech endpoint based on our TTS (text-to-speech) model.
    More info: https://platform.openai.com/docs/guides/text-to-speech/text-to-speech

    Request body:
    model (string) [Required] - One of the available TTS models: tts-1 or tts-1-hd
    input (string) [Required] - The text to generate audio for. The maximum length is 4096 characters.
    instructions (string) [Optional] - A description of the desired characteristics of the generated audio.
    voice (string) [Required] - The voice to use when generating the audio. Supported voices are alloy, echo, fable, onyx, nova, and shimmer.
    response_format (string) [Optional] - Defaults to mp3. The format to audio in. Supported formats are mp3, opus, aac, and flac.
    speed (number) [Optional] - Defaults to 1. The speed of the generated audio. Select a value from 0.25 to 4.0. 1.0 is the default.

    Usage: audio.openai_speech(OPENAI_API_KEY, "tts-1", input, instructions, "shimmer", "mp3", "1");
*/
bool Audio::openai_speech(const String& api_key, const String& model, const String& input, const String& instructions, const String& voice, const String& response_format, const String& speed) {
    ps_ptr<char> host;
    host.assign("api.openai.com");
    char path[] = "/v1/audio/speech";

    if (input == "") {
        AUDIO_INFO("input text is empty");
        stopSong();
        return false;
    }
    xSemaphoreTakeRecursive(mutex_playAudioData, 0.3 * configTICK_RATE_HZ);

    setDefaults();
    m_f_ssl = true;

    m_speechtxt.assign(input.c_str());

    // Escape special characters in input
    String input_clean = "";
    for (int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '\"') {
            input_clean += "\\\"";
        } else if (c == '\n') {
            input_clean += "\\n";
        } else if (c == '\r') {
            input_clean += "\\r";
        } else if (c == '\t') {
            input_clean += "\\t";
        } else if (c == '\\') {
            input_clean += "\\\\";
        } else if (c == '\b') {
            input_clean += "\\b";
        } else if (c == '\f') {
            input_clean += "\\f";
        } else {
            input_clean += c;
        }
    }

    // Escape special characters in instructions
    String instructions_clean = "";
    for (int i = 0; i < instructions.length(); i++) {
        char c = instructions.charAt(i);
        if (c == '\"') {
            instructions_clean += "\\\"";
        } else if (c == '\n') {
            instructions_clean += "\\n";
        } else if (c == '\r') {
            instructions_clean += "\\r";
        } else if (c == '\t') {
            instructions_clean += "\\t";
        } else if (c == '\\') {
            instructions_clean += "\\\\";
        } else if (c == '\b') {
            instructions_clean += "\\b";
        } else if (c == '\f') {
            instructions_clean += "\\f";
        } else {
            instructions_clean += c;
        }
    }

    String post_body = "{"
        "\"model\": \"" + model + "\"," +
        "\"stream\": true," +       // add
        "\"input\": \"" + input_clean + "\"," +
        "\"instructions\": \"" + instructions_clean + "\"," +
        "\"voice\": \"" + voice + "\"," +
        "\"response_format\": \"" + response_format + "\"," +
        "\"speed\": " + speed +
    "}";

    String http_request =
    //  "POST " + String(path) + " HTTP/1.0\r\n" // UNKNOWN ERROR CODE (0050) - crashing on HTTP/1.1 need to use HTTP/1.0
        "POST " + String(path) + " HTTP/1.1\r\n"
        + "Host: " + host.get() + "\r\n"
        + "Authorization: Bearer " + api_key + "\r\n"
        + "Accept-Encoding: identity;q=1,*;q=0\r\n"
        + "User-Agent: nArija/1.0\r\n"
        + "Content-Type: application/json; charset=utf-8\r\n"
        + "Content-Length: " + post_body.length() + "\r\n"
    //  + "Connection: close\r\n" + "\r\n"
        + "\r\n"
        + post_body + "\r\n"
    ;

    bool res = true;
    int port = 443;
    m_client = static_cast<WiFiClient*>(&clientsecure);

    uint32_t t = millis();
    AUDIO_INFO("Connect to: \"%s\"", host.get());
    res = m_client->connect(host.get(), port, m_timeout_ms_ssl);
    if (res) {
        uint32_t dt = millis() - t;
        m_lastHost.assign(host.get());
        m_currentHost.clone_from(host);
        AUDIO_INFO("%s has been established in %lu ms", m_f_ssl ? "SSL" : "Connection", (long unsigned int)dt);
        m_f_running = true;
    }

    m_expectedCodec = CODEC_NONE;
    m_expectedPlsFmt = FORMAT_NONE;

    if (res) {
        m_client->print(http_request);
        if (response_format == "mp3") m_expectedCodec  = CODEC_MP3;
        if (response_format == "opus") m_expectedCodec  = CODEC_OPUS;
        if (response_format == "aac") m_expectedCodec  = CODEC_AAC;
        if (response_format == "flac") m_expectedCodec  = CODEC_FLAC;

        m_dataMode = HTTP_RESPONSE_HEADER;
        m_f_tts = true;
    } else {
        AUDIO_INFO("Request %s failed!", host.get());
    }
    xSemaphoreGiveRecursive(mutex_playAudioData);
    return res;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Audio::hwoe_t Audio::dismantle_host(const char* host){
    if (!host) return {};

    hwoe_t result;

    const char* p = host;

    // 🔐 1. SSL check
    if (strncmp(p, "https://", 8) == 0) {
        result.ssl = true;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        result.ssl = false;
        p += 7;
    } else {  // No valid scheme -> error or acceptance http
        result.ssl = false;
    }

    // ❓ 2. extract host (from p to ':' or '/' or '?')
    const char* host_start = p;
    const char* port_sep = strchr(p, ':');
    const char* path_sep = strchr(p, '/');
    const char* query_sep = strchr(p, '?');

    const char* host_end = p + strlen(p);  // default: end of string
    if (port_sep  && port_sep  < host_end) host_end = port_sep;
    if (path_sep  && path_sep  < host_end) host_end = path_sep;
    if (query_sep && query_sep < host_end) host_end = query_sep;
    result.hwoe.copy_from(host_start, host_end - host_start);

    // ❓ 3. extract port
    result.port = result.ssl ? 443 : 80; // default
    if (port_sep && (!path_sep || port_sep < path_sep)) {
        result.port = atoi(port_sep + 1);
    }

    // ❓ 4. extract extension (path)
    if (path_sep) {
        const char* path_start = path_sep + 1;
        const char* path_end = query_sep ? query_sep : host + strlen(host);
        result.extension.copy_from(path_start, path_end - path_start);
    }

    // ❓ 5. extract query string
    if (query_sep) {
        result.query_string.assign(query_sep + 1);
    }

    return result;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::connecttohost(const char* host, const char* user, const char* pwd) { // user and pwd for authentification only, can be empty

    if(!host) { AUDIO_INFO("Hostaddress is empty");     stopSong(); return false;}
    if (strlen(host) > 2048) { AUDIO_INFO("Hostaddress is too long");  stopSong(); return false;} // max length in Chrome DevTools

    bool     res           = false; // return value
    uint16_t port          = 0;     // port number
    uint16_t authLen       = 0;     // length of authorization
    uint32_t timestamp     = 0;     // timeout surveillance

    ps_ptr<char> c_host;       // copy of host
    ps_ptr<char> hwoe;         // host without extension
    ps_ptr<char> extension;    // extension
    ps_ptr<char> query_string; // parameter
    ps_ptr<char> path;         // extension + '?' + parameter
    ps_ptr<char> rqh;          // request header

    xSemaphoreTakeRecursive(mutex_playAudioData, 0.3 * configTICK_RATE_HZ);

    c_host.copy_from(host);
    c_host.trim();
    auto dismantledHost = dismantle_host(c_host.get());

//  https://edge.live.mp3.mdn.newmedia.nacamar.net:8000/ps-charivariwb/livestream.mp3;?user=ps-charivariwb;&pwd=ps-charivariwb-------
//      |   |                                     |    |                              |
//      |   |                                     |    |                              |             (query string)
//  ssl?|   |<-----host without extension-------->|port|<----- --extension----------->|<-first parameter->|<-second parameter->.......
//                                                     |
//                                                     |<-----------------------------path------------------------------------>......

    m_f_ssl = dismantledHost.ssl;
    port = dismantledHost.port;
    if(dismantledHost.hwoe.valid()) hwoe.clone_from(dismantledHost.hwoe);
    if(dismantledHost.extension.valid()) extension.clone_from(dismantledHost.extension);
    if(dismantledHost.query_string.valid()) query_string.clone_from(dismantledHost.query_string);

    if(extension.valid()) path.assign(extension.get());
    if(query_string.valid()){path.append("?"); path.append(query_string.get());}
    if(!hwoe.valid()) hwoe.assign("");
    if(!extension.valid()) extension.assign("");
    if(!path.valid()) path.assign("");

    path = urlencode(path.get(), true);

    // optional basic authorization
    if(user && pwd) authLen = strlen(user) + strlen(pwd);
    ps_ptr <char>authorization;
    ps_ptr<char> toEncode;
    authorization.alloc(base64_encode_expected_len(authLen + 1) + 1, "authorization");
    authorization.clear();
    if(authLen > 0) {
        toEncode.assign(user);
        toEncode.append(":");
        toEncode.append(pwd);
        b64encode((const char*)toEncode.get(), toEncode.strlen(), authorization.get());
    }

    setDefaults();
                       rqh.assign("GET /");
                       rqh.append(path.get());
                       rqh.append(" HTTP/1.1\r\n");
                       rqh.append("Host: ");
                       rqh.append(hwoe.get());
                       rqh.append("\r\n");
                       rqh.append("Icy-MetaData:1\r\n");
                       rqh.append("Icy-MetaData:2\r\n");
                       rqh.append("Pragma: no-cache\r\n");
                       rqh.append("Cache-Control: no-cache\r\n");
                       rqh.append("Range: bytes=0-\r\n");
                       rqh.append("Accept: */*\r\n");
                       rqh.append("User-Agent: VLC/3.0.21 LibVLC/3.0.21 AppleWebKit/537.36 (KHTML, like Gecko)\r\n");
    if(authLen > 0) {  rqh.append("Authorization: Basic ");
                       rqh.append(authorization.get());
                       rqh.append("\r\n"); }
                       rqh.append("Accept-Encoding: identity;q=1,*;q=0\r\n");
                       rqh.append("Connection: keep-alive\r\n\r\n");

    if(m_f_ssl) { m_client = static_cast<WiFiClient*>(&clientsecure); if(port == 80) port = 443;}
    else        { m_client = static_cast<WiFiClient*>(&client); }

    timestamp = millis();
    m_client->setTimeout(m_f_ssl ? m_timeout_ms_ssl : m_timeout_ms);

    AUDIO_INFO("connect to: \"%s\" on port %d path \"/%s\"", hwoe.get(), port, path.get());
    res = m_client->connect(hwoe.get(), port);

    m_expectedCodec = CODEC_NONE;
    m_expectedPlsFmt = FORMAT_NONE;

    if(res) {
        uint32_t dt = millis() - timestamp;
        AUDIO_INFO("%s has been established in %lu ms", m_f_ssl ? "SSL" : "Connection", (long unsigned int)dt);
        m_f_running = true;
        m_client->print(rqh.get());
        if(extension.ends_with_icase( ".mp3" ))      m_expectedCodec  = CODEC_MP3;
        if(extension.ends_with_icase( ".aac" ))      m_expectedCodec  = CODEC_AAC;
        if(extension.ends_with_icase( ".wav" ))      m_expectedCodec  = CODEC_WAV;
        if(extension.ends_with_icase( ".m4a" ))      m_expectedCodec  = CODEC_M4A;
        if(extension.ends_with_icase( ".ogg" ))      m_expectedCodec  = CODEC_OGG;
        if(extension.ends_with_icase( ".flac"))      m_expectedCodec  = CODEC_FLAC;
        if(extension.ends_with_icase( "-flac"))      m_expectedCodec  = CODEC_FLAC;
        if(extension.ends_with_icase( ".opus"))      m_expectedCodec  = CODEC_OPUS;
        if(extension.ends_with_icase( "/opus"))      m_expectedCodec  = CODEC_OPUS;
        if(extension.ends_with_icase( ".asx" ))      m_expectedPlsFmt = FORMAT_ASX;
        if(extension.ends_with_icase( ".m3u" ))      m_expectedPlsFmt = FORMAT_M3U;
        if(extension.ends_with_icase( ".pls" ))      m_expectedPlsFmt = FORMAT_PLS;
        if(extension.contains(".m3u8"))              m_expectedPlsFmt = FORMAT_M3U8;

        m_currentHost.clone_from(c_host);
        m_lastHost.clone_from(c_host);
        if(audio_lasthost) audio_lasthost(m_lastHost.get());
        m_dataMode = HTTP_RESPONSE_HEADER; // Handle header
        m_streamType = ST_WEBSTREAM;
    }
    else {
        AUDIO_INFO("Request %s failed!", c_host.get());
        m_f_running = false;
        if(audio_showstation) audio_showstation("");
        if(audio_showstreamtitle) audio_showstreamtitle("");
        if(audio_icydescription) audio_icydescription("");
        if(audio_icyurl) audio_icyurl("");
    }
    xSemaphoreGiveRecursive(mutex_playAudioData);
    return res;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::httpPrint(const char* host) {
    // user and pwd for authentification only, can be empty
    if(!m_f_running) return false;
    if(host == NULL) {AUDIO_INFO("Hostaddress is empty"); stopSong(); return false;}

    uint16_t port = 0;         // port number
    ps_ptr<char> c_host;       // copy of host
    ps_ptr<char> hwoe;         // host without extension
    ps_ptr<char> extension;    // extension
    ps_ptr<char> query_string; // parameter
    ps_ptr<char> path;         // extension + '?' + parameter
    ps_ptr<char> rqh;          // request header
    ps_ptr<char> cur_hwoe;     // m_currenthost without extension

    c_host.copy_from(host);
    c_host.trim();
    auto dismantledHost = dismantle_host(c_host.get());

//  https://edge.live.mp3.mdn.newmedia.nacamar.net:8000/ps-charivariwb/livestream.mp3;?user=ps-charivariwb;&pwd=ps-charivariwb-------
//      |   |                                     |    |                              |
//      |   |                                     |    |                              |             (query string)
//  ssl?|   |<-----host without extension-------->|port|<----- --extension----------->|<-first parameter->|<-second parameter->.......
//                                                     |
//                                                     |<-----------------------------path------------------------------------------->

    m_f_ssl = dismantledHost.ssl;
    port = dismantledHost.port;
    if(dismantledHost.hwoe.valid()) hwoe.clone_from(dismantledHost.hwoe);
    if(dismantledHost.extension.valid()) extension.clone_from(dismantledHost.extension);
    if(dismantledHost.query_string.valid()) query_string.clone_from(dismantledHost.query_string);

    if(extension.valid()) path.assign(extension.get());
    if(query_string.valid()){path.append("?"); path.append(query_string.get());}
    if(!hwoe.valid()) hwoe.assign("");
    if(!extension.valid()) extension.assign("");
    if(!path.valid()) path.assign("");

    path = urlencode(path.get(), true);

    if(!m_currentHost.valid()) m_currentHost.assign("");
    auto dismantledLastHost = dismantle_host(m_currentHost.get());
    cur_hwoe.clone_from(dismantledLastHost.hwoe);

    bool f_equal = true;
    if(hwoe.equals(cur_hwoe)){f_equal = true;}
    else{                     f_equal = false;}

    rqh.assign("GET /");
    rqh.append(path.get());
    rqh.append(" HTTP/1.1\r\n");
    rqh.append("Host: ");
    rqh.append(hwoe.get());
    rqh.append("\r\n");
    rqh.append("Icy-MetaData:1\r\n");
    rqh.append("Icy-MetaData:2\r\n");
    rqh.append("Accept:*/*\r\n");
    rqh.append("User-Agent: VLC/3.0.21 LibVLC/3.0.21 AppleWebKit/537.36 (KHTML, like Gecko)\r\n");
    rqh.append("Accept-Encoding: identity;q=1,*;q=0\r\n");
    rqh.append("Connection: keep-alive\r\n\r\n");

    AUDIO_INFO("next URL: \"%s\"", c_host.get());

    if(f_equal == false){
        if(m_client->connected()) m_client->stop();
    }
    if(!m_client->connected() ) {
         if(m_f_ssl) { m_client = static_cast<WiFiClient*>(&clientsecure); if(m_f_ssl && port == 80) port = 443;}
         else        { m_client = static_cast<WiFiClient*>(&client); }
        if(f_equal) AUDIO_INFO("The host has disconnected, reconnecting");

        if(!m_client->connect(hwoe.get(), port)) {
            AUDIO_LOG_ERROR("connection lost %s", c_host.c_get());
            stopSong();
            return false;
        }
    }
    m_currentHost.clone_from(c_host);
    m_client->print(rqh.get());

    if(     extension.ends_with_icase(".mp3"))       m_expectedCodec  = CODEC_MP3;
    else if(extension.ends_with_icase(".aac"))       m_expectedCodec  = CODEC_AAC;
    else if(extension.ends_with_icase(".wav"))       m_expectedCodec  = CODEC_WAV;
    else if(extension.ends_with_icase(".m4a"))       m_expectedCodec  = CODEC_M4A;
    else if(extension.ends_with_icase(".flac"))      m_expectedCodec  = CODEC_FLAC;
    else                                             m_expectedCodec  = CODEC_NONE;

    if(     extension.ends_with_icase(".asx"))       m_expectedPlsFmt = FORMAT_ASX;
    else if(extension.ends_with_icase(".m3u"))       m_expectedPlsFmt = FORMAT_M3U;
    else if(extension.contains(".m3u8"))             m_expectedPlsFmt = FORMAT_M3U8;
    else if(extension.ends_with_icase(".pls"))       m_expectedPlsFmt = FORMAT_PLS;
    else                                             m_expectedPlsFmt = FORMAT_NONE;

    m_dataMode = HTTP_RESPONSE_HEADER; // Handle header
    m_streamType = ST_WEBSTREAM;
    m_f_chunked = false;

    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::httpRange(uint32_t seek, uint32_t length){

    if(!m_f_running) return false;

    uint16_t port = 0;         // port number
    ps_ptr<char> c_host;       // copy of host
    ps_ptr<char> hwoe;         // host without extension
    ps_ptr<char> extension;    // extension
    ps_ptr<char> query_string; // parameter
    ps_ptr<char> path;         // extension + '?' + parameter
    ps_ptr<char> rqh;          // request header
    ps_ptr<char> cur_hwoe;     // m_currenthost without extension
    ps_ptr<char> range;        // e.g. "Range: bytes=124-"

    c_host.clone_from(m_currentHost);
    c_host.trim();
    auto dismantledHost = dismantle_host(c_host.get());

//  https://edge.live.mp3.mdn.newmedia.nacamar.net:8000/ps-charivariwb/livestream.mp3;?user=ps-charivariwb;&pwd=ps-charivariwb-------
//      |   |                                     |    |                              |
//      |   |                                     |    |                              |             (query string)
//  ssl?|   |<-----host without extension-------->|port|<----- --extension----------->|<-first parameter->|<-second parameter->.......
//                                                     |
//                                                     |<-----------------------------path------------------------------------------->

    m_f_ssl = dismantledHost.ssl;
    port = dismantledHost.port;
    if(dismantledHost.hwoe.valid()) hwoe.clone_from(dismantledHost.hwoe);
    if(dismantledHost.extension.valid()) extension.clone_from(dismantledHost.extension);
    if(dismantledHost.query_string.valid()) query_string.clone_from(dismantledHost.query_string);

    if(extension.valid()) path.assign(extension.get());
    if(query_string.valid()){path.append("?"); path.append(query_string.get());}
    if(!hwoe.valid()) hwoe.assign("");
    if(!extension.valid()) extension.assign("");
    if(!path.valid()) path.assign("");

    path = urlencode(path.get(), true);

    if(!m_currentHost.valid()) m_currentHost.assign("");
    auto dismantledLastHost = dismantle_host(m_currentHost.get());
    cur_hwoe.clone_from(dismantledLastHost.hwoe);

    if(length == UINT32_MAX) range.assignf("Range: bytes=%li-\r\n",seek);
    else                     range.assignf("Range: bytes=%li-%li\r\n",seek, length);

    rqh.assignf("GET /%s HTTP/1.1\r\n", path.get());
    rqh.appendf("Host: %s\r\n", hwoe.get());
    rqh.append("Accept: */*\r\n");
    rqh.append("Accept-Encoding: identity;q=1,*;q=0\r\n");
    rqh.append("Cache-Control: no-cache\r\n");
    rqh.append("Connection: keep-alive\r\n");
    rqh.appendf(range.c_get());
    rqh.appendf("Referer: %s\r\n", m_currentHost.c_get());
    rqh.append("Sec-GPC: 1\r\n");
    rqh.append("User-Agent: VLC/3.0.21 LibVLC/3.0.21 AppleWebKit/537.36 (KHTML, like Gecko)\r\n\r\n");

    if(m_client->connected()) {m_client->stop();}
    if(m_f_ssl) { m_client = static_cast<WiFiClient*>(&clientsecure); if(m_f_ssl && port == 80) port = 443;}
    else        { m_client = static_cast<WiFiClient*>(&client); }

    if(!m_client->connect(hwoe.get(), port)) {
        AUDIO_LOG_ERROR("connection lost %s", c_host.c_get());
        stopSong();
        return false;
    }

    // AUDIO_LOG_INFO("rqh \n%s", rqh.get());

    m_client->print(rqh.c_get());

    m_dataMode = HTTP_RANGE_HEADER;
    m_streamType = ST_WEBFILE;
    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::connecttoFS(fs::FS& fs, const char* path, int32_t fileStartPos) {

    xSemaphoreTakeRecursive(mutex_playAudioData, 0.3 * configTICK_RATE_HZ);
    ps_ptr<char>c_path;
    ps_ptr<char> audioPath;
    bool res = false;
    m_fileStartPos = fileStartPos;
    uint8_t codec = CODEC_NONE;

    if(!path) {AUDIO_INFO("file path is not set"); goto exit;}  // guard
    c_path.copy_from(path); // copy from path
    c_path.trim();
    if(!c_path.contains(".")) {AUDIO_INFO("No file extension found"); goto exit;}  // guard
    setDefaults(); // free buffers an set defaults

    if(c_path.ends_with_icase(".mp3"))   codec = CODEC_MP3;
    if(c_path.ends_with_icase(".m4a"))   codec = CODEC_M4A;
    if(c_path.ends_with_icase(".aac"))   codec = CODEC_AAC;
    if(c_path.ends_with_icase(".wav"))   codec = CODEC_WAV;
    if(c_path.ends_with_icase(".flac"))  codec = CODEC_FLAC;
    if(c_path.ends_with_icase(".opus")) {codec = CODEC_OPUS; m_f_ogg = true;}
    if(c_path.ends_with_icase(".ogg"))  {codec = CODEC_OGG;  m_f_ogg = true;}
    if(c_path.ends_with_icase(".oga"))  {codec = CODEC_OGG;  m_f_ogg = true;}
    if(codec == CODEC_NONE) {   // guard
        int dotPos = c_path.last_index_of('.');
        AUDIO_INFO("The %s format is not supported", path + dotPos); goto exit;
    }

    if(!c_path.starts_with("/")) c_path.insert("/", 0);

    if(!fs.exists(c_path.get())) {AUDIO_INFO("file not found: %s", c_path.get()); goto exit;}
    AUDIO_INFO("Reading file: \"%s\"", c_path.get());
    m_audiofile = fs.open(c_path.get());
    m_dataMode = AUDIO_LOCALFILE;
    m_audioFileSize = m_audiofile.size();

    res = initializeDecoder(codec);
    m_codec = codec;
    if(res) m_f_running = true;
    else m_audiofile.close();

exit:
    xSemaphoreGiveRecursive(mutex_playAudioData);
    return res;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::connecttospeech(const char* speech, const char* lang) {
    xSemaphoreTakeRecursive(mutex_playAudioData, 0.3 * configTICK_RATE_HZ);

    setDefaults();
    char host[] = "translate.google.com.vn";
    char path[] = "/translate_tts";

    m_speechtxt.assign(speech); // unique pointer takes care of the memory management
    auto urlStr = urlencode(speech, false); // percent encoding

    ps_ptr<char> req; // request header
    req.assign("GET ");
    req.append(path);
    req.append("?ie=UTF-8&tl=");
    req.append(lang);
    req.append("&client=tw-ob&q=");
    req.append(urlStr.get());
    req.append(" HTTP/1.1\r\n");
    req.append("Host: ");
    req.append(host);
    req.append("\r\n");
    req.append("User-Agent: Mozilla/5.0 \r\n");
    req.append("Accept-Encoding: identity\r\n");
    req.append("Accept: text/html\r\n");
    req.append("Connection: close\r\n\r\n");

    m_client = static_cast<WiFiClient*>(&client);
    AUDIO_INFO("connect to \"%s\"", host);
    if(!m_client->connect(host, 80)) {
        AUDIO_LOG_ERROR("Connection failed");
        xSemaphoreGiveRecursive(mutex_playAudioData);
        return false;
    }
    m_client->print(req.get());

    m_f_running = true;
    m_f_ssl = false;
    m_f_tts = true;
    m_dataMode = HTTP_RESPONSE_HEADER;
    m_lastHost.assign(host);
    m_currentHost.copy_from(host);
    xSemaphoreGiveRecursive(mutex_playAudioData);
    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::showID3Tag(const char* tag, const char* value) {
    ps_ptr<char>id3tag = {};
    // V2.2
    if(!strcmp(tag, "CNT")) id3tag.appendf("Play counter: %s", value, "id3tag");
    if(!strcmp(tag, "COM")) id3tag.appendf("Comments: %s", value, "id3tag");
    if(!strcmp(tag, "CRA")) id3tag.appendf("Audio encryption: %s", value, "id3tag");
    if(!strcmp(tag, "CRM")) id3tag.appendf("Encrypted meta frame: %s", value, "id3tag");
    if(!strcmp(tag, "ETC")) id3tag.appendf("Event timing codes: %s", value, "id3tag");
    if(!strcmp(tag, "EQU")) id3tag.appendf("Equalization: %s", value, "id3tag");
    if(!strcmp(tag, "IPL")) id3tag.appendf("Involved people list: %s", value, "id3tag");
    if(!strcmp(tag, "PIC")) id3tag.appendf("Attached picture: %s", value, "id3tag");
    if(!strcmp(tag, "SLT")) id3tag.appendf("Synchronized lyric/text: %s", value, "id3tag");
    if(!strcmp(tag, "TAL")) id3tag.appendf("Album/Movie/Show title: %s", value, "id3tag");
    if(!strcmp(tag, "TBP")) id3tag.appendf("BPM (Beats Per Minute): %s", value, "id3tag");
    if(!strcmp(tag, "TCM")) id3tag.appendf("Composer: %s", value, "id3tag");
    if(!strcmp(tag, "TCO")) id3tag.appendf("Content type: %s", value, "id3tag");
    if(!strcmp(tag, "TCR")) id3tag.appendf("Copyright message: %s", value, "id3tag");
    if(!strcmp(tag, "TDA")) id3tag.appendf("Date: %s", value, "id3tag");
    if(!strcmp(tag, "TDY")) id3tag.appendf("Playlist delay: %s", value, "id3tag");
    if(!strcmp(tag, "TEN")) id3tag.appendf("Encoded by: %s", value, "id3tag");
    if(!strcmp(tag, "TFT")) id3tag.appendf("File type: %s", value, "id3tag");
    if(!strcmp(tag, "TIM")) id3tag.appendf("Time: %s", value, "id3tag");
    if(!strcmp(tag, "TKE")) id3tag.appendf("Initial key: %s", value, "id3tag");
    if(!strcmp(tag, "TLA")) id3tag.appendf("Language(s): %s", value, "id3tag");
    if(!strcmp(tag, "TLE")) id3tag.appendf("Length: %s", value, "id3tag");
    if(!strcmp(tag, "TMT")) id3tag.appendf("Media type: %s", value, "id3tag");
    if(!strcmp(tag, "TOA")) id3tag.appendf("Original artist(s)/performer(s): %s", value, "id3tag");
    if(!strcmp(tag, "TOF")) id3tag.appendf("Original filename: %s", value, "id3tag");
    if(!strcmp(tag, "TOL")) id3tag.appendf("Original Lyricist(s)/text writer(s): %s", value, "id3tag");
    if(!strcmp(tag, "TOR")) id3tag.appendf("Original release year: %s", value, "id3tag");
    if(!strcmp(tag, "TOT")) id3tag.appendf("Original album/Movie/Show title: %s", value, "id3tag");
    if(!strcmp(tag, "TP1")) id3tag.appendf("Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group: %s", value, "id3tag");
    if(!strcmp(tag, "TP2")) id3tag.appendf("Band/Orchestra/Accompaniment: %s", value, "id3tag");
    if(!strcmp(tag, "TP3")) id3tag.appendf("Conductor/Performer refinement: %s", value, "id3tag");
    if(!strcmp(tag, "TP4")) id3tag.appendf("Interpreted, remixed, or otherwise modified by: %s", value, "id3tag");
    if(!strcmp(tag, "TPA")) id3tag.appendf("Part of a set: %s", value, "id3tag");
    if(!strcmp(tag, "TPB")) id3tag.appendf("Publisher: %s", value, "id3tag");
    if(!strcmp(tag, "TRC")) id3tag.appendf("ISRC (International Standard Recording Code): %s", value, "id3tag");
    if(!strcmp(tag, "TRD")) id3tag.appendf("Recording dates: %s", value, "id3tag");
    if(!strcmp(tag, "TRK")) id3tag.appendf("Track number/Position in set: %s", value, "id3tag");
    if(!strcmp(tag, "TSI")) id3tag.appendf("Size: %s", value, "id3tag");
    if(!strcmp(tag, "TSS")) id3tag.appendf("Software/hardware and settings used for encoding: %s", value, "id3tag");
    if(!strcmp(tag, "TT1")) id3tag.appendf("Content group description: %s", value, "id3tag");
    if(!strcmp(tag, "TT2")) id3tag.appendf("Title/Songname/Content description: %s", value, "id3tag");
    if(!strcmp(tag, "TT3")) id3tag.appendf("Subtitle/Description refinement: %s", value, "id3tag");
    if(!strcmp(tag, "TXT")) id3tag.appendf("Lyricist/text writer: %s", value, "id3tag");
    if(!strcmp(tag, "TXX")) id3tag.appendf("User defined text information frame: %s", value, "id3tag");
    if(!strcmp(tag, "TYE")) id3tag.appendf("Year: %s", value, "id3tag");
    if(!strcmp(tag, "UFI")) id3tag.appendf("Unique file identifier: %s", value, "id3tag");
    if(!strcmp(tag, "ULT")) id3tag.appendf("Unsychronized lyric/text transcription: %s", value, "id3tag");
    if(!strcmp(tag, "WAF")) id3tag.appendf("Official audio file webpage: %s", value, "id3tag");
    if(!strcmp(tag, "WAR")) id3tag.appendf("Official artist/performer webpage: %s", value, "id3tag");
    if(!strcmp(tag, "WAS")) id3tag.appendf("Official audio source webpage: %s", value, "id3tag");
    if(!strcmp(tag, "WCM")) id3tag.appendf("Commercial information: %s", value, "id3tag");
    if(!strcmp(tag, "WCP")) id3tag.appendf("Copyright/Legal information: %s", value, "id3tag");
    if(!strcmp(tag, "WPB")) id3tag.appendf("Publishers official webpage: %s", value, "id3tag");
    if(!strcmp(tag, "WXX")) id3tag.appendf("User defined URL link frame: %s", value, "id3tag");

    // V2.3 V2.4 tags
    if(!strcmp(tag, "COMM")) id3tag.appendf("Comment: %s", value, "id3tag");
    if(!strcmp(tag, "OWNE")) id3tag.appendf("Ownership: %s", value, "id3tag");
    if(!strcmp(tag, "PRIV")) id3tag.appendf("Private: %s", value, "id3tag");
    if(!strcmp(tag, "SYLT")) id3tag.appendf("SynLyrics: %s", value, "id3tag");
    if(!strcmp(tag, "TALB")) id3tag.appendf("Album: %s", value, "id3tag");
    if(!strcmp(tag, "TBPM")) id3tag.appendf("BeatsPerMinute: %s", value, "id3tag");
    if(!strcmp(tag, "TCAT")) id3tag.appendf("PodcastCategory: %s", value, "id3tag");
    if(!strcmp(tag, "TCMP")) id3tag.appendf("Compilation: %s", value, "id3tag");
    if(!strcmp(tag, "TCOM")) id3tag.appendf("Composer: %s", value, "id3tag");
    if(!strcmp(tag, "TCON")) id3tag.appendf("ContentType: %s", value, "id3tag");
    if(!strcmp(tag, "TCOP")) id3tag.appendf("Copyright: %s", value, "id3tag");
    if(!strcmp(tag, "TDAT")) id3tag.appendf("Date: %s", value, "id3tag");
    if(!strcmp(tag, "TDES")) id3tag.appendf("PodcastDescription: %s", value, "id3tag");
    if(!strcmp(tag, "TDOR")) id3tag.appendf("Original Release Year: %s", value, "id3tag");
    if(!strcmp(tag, "TDRC")) id3tag.appendf("Publication date: %s", value, "id3tag");
    if(!strcmp(tag, "TDRL")) id3tag.appendf("ReleaseTime: %s", value, "id3tag");
    if(!strcmp(tag, "TENC")) id3tag.appendf("Encoded by: %s", value, "id3tag");
    if(!strcmp(tag, "TEXT")) id3tag.appendf("Lyricist: %s", value, "id3tag");
    if(!strcmp(tag, "TGID")) id3tag.appendf("PodcastID: %s", value, "id3tag");
    if(!strcmp(tag, "TIME")) id3tag.appendf("Time: %s", value, "id3tag");
    if(!strcmp(tag, "TIT1")) id3tag.appendf("Grouping: %s", value, "id3tag");
    if(!strcmp(tag, "TIT2")) id3tag.appendf("Title: %s", value, "id3tag");
    if(!strcmp(tag, "TIT3")) id3tag.appendf("Subtitle: %s", value, "id3tag");
    if(!strcmp(tag, "TLAN")) id3tag.appendf("Language: %s", value, "id3tag");
    if(!strcmp(tag, "TLEN")) id3tag.appendf("Length (ms): %s", value, "id3tag");
    if(!strcmp(tag, "TMED")) id3tag.appendf("Media: %s", value, "id3tag");
    if(!strcmp(tag, "TOAL")) id3tag.appendf("OriginalAlbum: %s", value, "id3tag");
    if(!strcmp(tag, "TOPE")) id3tag.appendf("OriginalArtist: %s", value, "id3tag");
    if(!strcmp(tag, "TOLY")) id3tag.appendf("OriginalLyricist: %s", value, "id3tag");
    if(!strcmp(tag, "TORY")) id3tag.appendf("OriginalReleaseYear: %s", value, "id3tag");
    if(!strcmp(tag, "TPE1")) id3tag.appendf("Artist: %s", value, "id3tag");
    if(!strcmp(tag, "TPE2")) id3tag.appendf("Band: %s", value, "id3tag");
    if(!strcmp(tag, "TPE3")) id3tag.appendf("Conductor: %s", value, "id3tag");
    if(!strcmp(tag, "TPE4")) id3tag.appendf("InterpretedBy: %s", value, "id3tag");
    if(!strcmp(tag, "TPOS")) id3tag.appendf("PartOfSet: %s", value, "id3tag");
    if(!strcmp(tag, "TPUB")) id3tag.appendf("Publisher: %s", value, "id3tag");
    if(!strcmp(tag, "TRCK")) id3tag.appendf("Track: %s", value, "id3tag");
    if(!strcmp(tag, "TRSN")) id3tag.appendf("InternetRadioStationName: %s", value, "id3tag");
    if(!strcmp(tag, "TSSE")) id3tag.appendf("SettingsForEncoding: %s", value, "id3tag");
    if(!strcmp(tag, "TRDA")) id3tag.appendf("RecordingDates: %s", value, "id3tag");
    if(!strcmp(tag, "TSO2")) id3tag.appendf("AlbumArtistSortOrder: %s", value, "id3tag");
    if(!strcmp(tag, "TSOA")) id3tag.appendf("AlbumSortOrder: %s", value, "id3tag");
    if(!strcmp(tag, "TSOP")) id3tag.appendf("PerformerSortOrder: %s", value, "id3tag");
    if(!strcmp(tag, "TSOC")) id3tag.appendf("ComposerSortOrder: %s", value, "id3tag");
    if(!strcmp(tag, "TSOT")) id3tag.appendf("TitleSortOrder: %s", value, "id3tag");
    if(!strcmp(tag, "TXXX")) id3tag.appendf("UserDefinedText: %s", value, "id3tag");
    if(!strcmp(tag, "TYER")) id3tag.appendf("Year: %s", value, "id3tag");
    if(!strcmp(tag, "USER")) id3tag.appendf("TermsOfUse: %s", value, "id3tag");
    if(!strcmp(tag, "USLT")) id3tag.appendf("Lyrics: %s", value, "id3tag");
    if(!strcmp(tag, "WCOM")) id3tag.appendf("Commercial URL: %s", value, "id3tag");
    if(!strcmp(tag, "WFED")) id3tag.appendf("Podcast URL: %s", value, "id3tag");
    if(!strcmp(tag, "WOAF")) id3tag.appendf("File URL: %s", value, "id3tag");
    if(!strcmp(tag, "WOAR")) id3tag.appendf("OfficialArtistWebpage: %s", value, "id3tag");
    if(!strcmp(tag, "WOAS")) id3tag.appendf("Source URL: %s", value, "id3tag");
    if(!strcmp(tag, "WORS")) id3tag.appendf("InternetRadioStationURL: %s", value, "id3tag");
    if(!strcmp(tag, "WXXX")) id3tag.appendf("User defined URL link frame: %s", value, "id3tag");
    if(!strcmp(tag, "XDOR")) id3tag.appendf("OriginalReleaseTime: %s", value, "id3tag");

    if(!id3tag.valid()){AUDIO_LOG_DEBUG("unknown tag: %s", tag); return;}
    latinToUTF8(id3tag);
    if(id3tag.contains("?xml")) {
        showstreamtitle(id3tag.get());
        return;
    }
    if(id3tag.strlen()) {
        if(audio_id3data) audio_id3data(id3tag.get());
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::latinToUTF8(ps_ptr<char>& buff, bool UTF8check) {
    // most stations send  strings in UTF-8 but a few sends in latin. To standardize this, all latin strings are
    // converted to UTF-8. If UTF-8 is already present, nothing is done and true is returned.
    // A conversion to UTF-8 extends the string. Therefore it is necessary to know the buffer size. If the converted
    // string does not fit into the buffer, false is returned

    uint16_t pos = 0;
    uint16_t in = 0;
    uint16_t out = 0;
    bool     isUTF8 = true;

    // We cannot detect if a given string (or byte sequence) is a UTF-8 encoded text as for example each and every series
    // of UTF-8 octets is also a valid (if nonsensical) series of Latin-1 (or some other encoding) octets.
    // However not every series of valid Latin-1 octets are valid UTF-8 series. So you can rule out strings that do not conform
    // to the UTF-8 encoding schema:

    if (UTF8check) {
        while (buff[pos] != '\0') {
            if (buff[pos] <= 0x7F) {// 0xxxxxxx: ASCII
                pos++;
            }
            else if ((buff[pos] & 0xE0) == 0xC0) {
                if (pos + 1 >= buff.strlen()) isUTF8 = false;
                // 110xxxxx 10xxxxxx: 2-byte
                if ((buff[pos + 1] & 0xC0) != 0x80) isUTF8 = false;
                if (buff[pos] < 0xC2) isUTF8 = false; // Overlong encoding
                pos += 2;
            }
            else if  ((buff[pos] & 0xF0) == 0xE0) {
                // 1110xxxx 10xxxxxx 10xxxxxx: 3-byte
                if ((buff[pos + 1] & 0xC0) != 0x80 || (buff[pos + 2] & 0xC0) != 0x80) isUTF8 = false;
                if (buff[pos] == 0xE0 && buff[pos + 1 ] < 0xA0) isUTF8 = false; // Overlong
                if (buff[pos] == 0xED && buff[pos + 1] >= 0xA0) isUTF8 = false; // UTF-16 surrogate
                pos += 3;
            }
            else if ((buff[pos] & 0xF8) == 0xF0) {
                // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx: 4-byte
                if ((buff[pos + 1] & 0xC0) != 0x80 ||
                    (buff[pos + 2] & 0xC0) != 0x80 ||
                    (buff[pos + 3] & 0xC0) != 0x80) isUTF8 = false;
                if (buff[pos] == 0xF0 && buff[pos + 1] < 0x90) isUTF8 = false; // Overlong
                if (buff[pos] > 0xF4 || (buff[pos] == 0xF4 && buff[pos + 1] > 0x8F)) isUTF8 = false; // > U+10FFFF
                pos += 4;
            }
            else{
                isUTF8 = false; // Invalid first byte
            }
            if(!isUTF8)break;
        }
        if (isUTF8) return; // is UTF-8, do nothing
    }
    ps_ptr<char> iso8859_1;
    iso8859_1.assign(buff.get());

    // Worst-case: all chars are latin1 > 0x7F became 2 Bytes → max length is twice +1
    std::size_t requiredSize = strlen(iso8859_1.get()) * 2 + 1;

    if (buff.size() < requiredSize) {
        buff.realloc(requiredSize, "utf8_conv");
    }

    // coding into UTF-8
    while (iso8859_1[in] != '\0') {
        if (iso8859_1[in] < 0x80) {
            buff[out++] = iso8859_1[in++];
        } else {
            buff[out++] = (0xC0 | (iso8859_1[in] >> 6));
            buff[out++] = (0x80 | (iso8859_1[in] & 0x3F));
            in++;
        }
    }
    buff[out] = '\0';
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::htmlToUTF8(char* str) { // convert HTML to UTF-8

    typedef struct { // --- EntityMap Definition ---
        const char *name;
        uint32_t codepoint;
    } EntityMap;

    const EntityMap entities[] = {
        {"amp",   0x0026}, // &
        {"lt",    0x003C}, // <
        {"gt",    0x003E}, // >
        {"quot",  0x0022}, // "
        {"apos",  0x0027}, // '
        {"nbsp",  0x00A0}, // non-breaking space
        {"euro",  0x20AC}, // €
        {"copy",  0x00A9}, // ©
        {"reg",   0x00AE}, // ®
        {"trade", 0x2122}, // ™
        {"hellip",0x2026}, // …
        {"ndash", 0x2013}, // –
        {"mdash", 0x2014}, // —
        {"sect",  0x00A7}, // §
        {"para",  0x00B6}  // ¶
    };

    // --- EntityMap Lookup ---
    auto find_entity = [&](const char *p, uint32_t *codepoint, int *entity_len) {
        for (size_t i = 0; i < sizeof(entities)/sizeof(entities[0]); i++) {
            const char *name = entities[i].name;
            size_t len = strlen(name);
            if (strncmp(p + 1, name, len) == 0 && p[len + 1] == ';') {
                *codepoint = entities[i].codepoint;
                *entity_len = (int)(len + 2); // &name;
                return 1;
            }
        }
        return 0;
    };


    auto codepoint_to_utf8 = [&](uint32_t cp, char *dst) { // Convert a Codepoint (Unicode) to UTF-8, writes in DST, there is number of bytes back
        if (cp <= 0x7F) {
            dst[0] = cp;
            return 1;
        } else if (cp <= 0x7FF) {
            dst[0] = 0xC0 | (cp >> 6);
            dst[1] = 0x80 | (cp & 0x3F);
            return 2;
        } else if (cp <= 0xFFFF) {
            dst[0] = 0xE0 | (cp >> 12);
            dst[1] = 0x80 | ((cp >> 6) & 0x3F);
            dst[2] = 0x80 | (cp & 0x3F);
            return 3;
        } else if (cp <= 0x10FFFF) {
            dst[0] = 0xF0 | (cp >> 18);
            dst[1] = 0x80 | ((cp >> 12) & 0x3F);
            dst[2] = 0x80 | ((cp >> 6) & 0x3F);
            dst[3] = 0x80 | (cp & 0x3F);
            return 4;
        }
        return -1; // invalid Codepoint
    };

    char *p = str;
    while (*p != '\0') {
        if(p[0] == '&'){
            uint32_t cp;
            int consumed;
            if (find_entity(p, &cp, &consumed)) { // looking for entity, such as &copy;
                char utf8[5] = {0};
                int len = codepoint_to_utf8(cp, utf8);
                if (len > 0) {
                    size_t tail_len = strlen(p + consumed);
                    memmove(p + len, p + consumed, tail_len + 1);
                    memcpy(p, utf8, len);
                    p += len;
                    continue;
                }
            }
        }
        if (p[0] == '&' && p[1] == '#') {
            char *endptr;
            uint32_t codepoint = strtol(p + 2, &endptr, 10);

            if (*endptr == ';' && codepoint <= 0x10FFFF) {
                char utf8[5] = {0};
                int utf8_len = codepoint_to_utf8(codepoint, utf8);
                if (utf8_len > 0) {
                //    size_t entity_len = endptr - p + 1;
                    size_t tail_len = strlen(endptr + 1);

                    // Show residual ring to the left
                    memmove(p + utf8_len, endptr + 1, tail_len + 1);  // +1 because of '\0'

                    // Copy UTF-8 characters
                    memcpy(p, utf8, utf8_len);

                    // weiter bei neuem Zeichen
                    p += utf8_len;
                    continue;
                }
            }
        }
        p++;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
size_t Audio::readAudioHeader(uint32_t bytes) {
    size_t bytesReaded = 0;
    if(m_codec == CODEC_WAV) {
        int res = read_WAV_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else { // error, skip header
            m_controlCounter = 100;
        }
    }
    if(m_codec == CODEC_MP3) {
        int res = read_ID3_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else { // error, skip header
            m_controlCounter = 100;
        }
    }
    if(m_codec == CODEC_M4A) {
        int res = read_M4A_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else { // error, skip header
            m_controlCounter = 100;
        }
    }
    if(m_codec == CODEC_AAC) {
        // stream only, no header
        m_audioDataSize = m_audioFileSize;
        m_controlCounter = 100;
    }
    if(m_codec == CODEC_FLAC) {
        int res = read_FLAC_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else { // error, skip header
            stopSong();
            m_controlCounter = 100;
        }
    }
    if(m_codec == CODEC_OPUS)   { m_controlCounter = 100; }
    if(m_codec == CODEC_VORBIS) { m_controlCounter = 100; }
    if(m_codec == CODEC_OGG)    { m_controlCounter = 100; }
    if(!isRunning()) {
        AUDIO_LOG_ERROR("Processing stopped due to invalid audio header");
        return 0;
    }
    return bytesReaded;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Audio::read_WAV_Header(uint8_t* data, size_t len) {

    if(m_controlCounter == 0) {
        m_rwh.cs = 0;
        m_rwh.bts = 0;
        m_controlCounter++;
        if((*data != 'R') || (*(data + 1) != 'I') || (*(data + 2) != 'F') || (*(data + 3) != 'F')) {
            AUDIO_INFO("file has no RIFF tag");
            m_rwh.headerSize = 0;
            return -1; // false;
        }
        else {
            m_rwh.headerSize = 4;
            return 4; // ok
        }
    }

    if(m_controlCounter == 1) {
        m_controlCounter++;
        m_rwh.cs = (uint32_t)(*data + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24) - 8);
        m_rwh.headerSize += 4;
        return 4; // ok
    }

    if(m_controlCounter == 2) {
        m_controlCounter++;
        if((*data != 'W') || (*(data + 1) != 'A') || (*(data + 2) != 'V') || (*(data + 3) != 'E')) {
            AUDIO_INFO("format tag is not WAVE");
            return -1; // false;
        }
        else {
            m_rwh.headerSize += 4;
            return 4;
        }
    }

    if(m_controlCounter == 3) {
        if((*data == 'f') && (*(data + 1) == 'm') && (*(data + 2) == 't')) {
            m_controlCounter++;
            m_rwh.headerSize += 4;
            return 4;
        }
        else {
            m_rwh.headerSize += 4;
            return 4;
        }
    }

    if(m_controlCounter == 4) {
        m_controlCounter++;
        m_rwh.cs = (uint32_t)(*data + (*(data + 1) << 8));
        if(m_rwh.cs > 40) return -1; // false, something going wrong
        m_rwh.bts = m_rwh.cs - 16;         // bytes to skip if fmt chunk is >16
        m_rwh.headerSize += 4;
        return 4;
    }

    if(m_controlCounter == 5) {
        m_controlCounter++;
        uint16_t fc = (uint16_t)(*(data + 0) + (*(data + 1) << 8));                                               // Format code
        uint16_t nic = (uint16_t)(*(data + 2) + (*(data + 3) << 8));                                              // Number of interleaved channels
        uint32_t sr = (uint32_t)(*(data + 4) + (*(data + 5) << 8) + (*(data + 6) << 16) + (*(data + 7) << 24));   // Samplerate
        uint32_t dr = (uint32_t)(*(data + 8) + (*(data + 9) << 8) + (*(data + 10) << 16) + (*(data + 11) << 24)); // Datarate
        uint16_t dbs = (uint16_t)(*(data + 12) + (*(data + 13) << 8));                                            // Data block size
        uint16_t bps = (uint16_t)(*(data + 14) + (*(data + 15) << 8));                                            // Bits per sample

        AUDIO_INFO("FormatCode: %u", fc);
        // AUDIO_INFO("Channel: %u", nic);
        // AUDIO_INFO("SampleRate: %u", sr);
        AUDIO_INFO("DataRate: %lu", (long unsigned int)dr);
        AUDIO_INFO("DataBlockSize: %u", dbs);
        AUDIO_INFO("BitsPerSample: %u", bps);

        if((bps != 8) && (bps != 16)) {
            AUDIO_INFO("BitsPerSample is %u,  must be 8 or 16", bps);
            stopSong();
            return -1;
        }
        if((nic != 1) && (nic != 2)) {
            AUDIO_INFO("num channels is %u,  must be 1 or 2", nic);
            stopSong();
            return -1;
        }
        if(fc != 1) {
            AUDIO_INFO("format code is not 1 (PCM)");
            stopSong();
            return -1; // false;
        }
        setBitsPerSample(bps);
        setChannels(nic);
        setSampleRate(sr);
        setBitrate(nic * sr * bps);
        //    AUDIO_INFO("BitRate: %u", m_bitRate);
        m_rwh.headerSize += 16;
        return 16; // ok
    }

    if(m_controlCounter == 6) {
        m_controlCounter++;
        m_rwh.headerSize += m_rwh.bts;
        return m_rwh.bts; // skip to data
    }

    if(m_controlCounter == 7) {
        if((*(data + 0) == 'd') && (*(data + 1) == 'a') && (*(data + 2) == 't') && (*(data + 3) == 'a')) {
            m_controlCounter++;
            //    vTaskDelay(30);
            m_rwh.headerSize += 4;
            return 4;
        }
        else {
            m_rwh.headerSize++;
            return 1;
        }
    }

    if(m_controlCounter == 8) {
        m_controlCounter++;
        size_t cs = *(data + 0) + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24); // read chunkSize
        m_rwh.headerSize += 4;
        if(cs) { m_audioDataSize = cs - 44; }
        else { // sometimes there is nothing here
            m_audioDataSize = m_audioFileSize -m_rwh. headerSize;
        }
        AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
        return 4;
    }
    m_controlCounter = 100; // header succesfully read
    m_audioDataStart = m_rwh.headerSize;
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Audio::read_FLAC_Header(uint8_t* data, size_t len) {

    if(m_rflh.retvalue) {
        if(m_rflh.retvalue > len) { // if returnvalue > bufferfillsize
            if(len > InBuff.getMaxBlockSize()) len = InBuff.getMaxBlockSize();
            m_rflh.retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else {
            size_t tmp = m_rflh.retvalue;
            m_rflh.retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_BEGIN) { // init
        m_rflh.headerSize = 0;
        m_rflh.retvalue = 0;
        m_audioDataStart = 0;
        m_rflh.picPos = 0;
        m_rflh.picLen = 0;
        m_rflh.f_lastMetaBlock = false;
        m_controlCounter = FLAC_MAGIC;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_MAGIC) {            /* check MAGIC STRING */
        if(specialIndexOf(data, "OggS", 10) == 0) { // is ogg
            m_rflh.headerSize = 0;
            m_rflh.retvalue = 0;
            m_controlCounter = FLAC_OKAY;
            return 0;
        }
        if(specialIndexOf(data, "fLaC", 10) != 0) {
            AUDIO_LOG_ERROR("Magic String 'fLaC' not found in header");
            stopSong();
            return -1;
        }
        m_controlCounter = FLAC_MBH; // METADATA_BLOCK_HEADER
        m_rflh.headerSize = 4;
        m_rflh.retvalue = 4;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_MBH) { /* METADATA_BLOCK_HEADER */
        uint8_t blockType = *data;
        if(!m_rflh.f_lastMetaBlock) {
            if(blockType & 128) {m_rflh.f_lastMetaBlock = true; }
            blockType &= 127;
            if(blockType == 0) m_controlCounter = FLAC_SINFO;
            if(blockType == 1) m_controlCounter = FLAC_PADDING;
            if(blockType == 2) m_controlCounter = FLAC_APP;
            if(blockType == 3) m_controlCounter = FLAC_SEEK;
            if(blockType == 4) m_controlCounter = FLAC_VORBIS;
            if(blockType == 5) m_controlCounter = FLAC_CUESHEET;
            if(blockType == 6) m_controlCounter = FLAC_PICTURE;
            m_rflh.headerSize += 1;
            m_rflh.retvalue = 1;
            return 0;
        }
        m_controlCounter = FLAC_OKAY;
        m_audioDataStart =m_rflh. headerSize;
        m_audioDataSize = m_audioFileSize - m_audioDataStart;
        FLACSetRawBlockParams(m_flacNumChannels, m_flacSampleRate, m_flacBitsPerSample, m_flacTotalSamplesInStream, m_audioDataSize);
        if(m_rflh.picLen) {
            size_t pos = m_audioFilePosition;
            if(audio_id3image) audio_id3image(m_audiofile, m_rflh.picPos, m_rflh.picLen);
            audioFileSeek(pos); // the filepointer could have been changed by the user, set it back
        }
        AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
        m_rflh.retvalue = 0;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_SINFO) { /* Stream info block */
        size_t l = bigEndian(data, 3);
        vTaskDelay(2);
        m_flacMaxBlockSize = bigEndian(data + 5, 2);
        AUDIO_INFO("FLAC maxBlockSize: %u", m_flacMaxBlockSize);
        vTaskDelay(2);
        m_flacMaxFrameSize = bigEndian(data + 10, 3);
        if(m_flacMaxFrameSize) { AUDIO_INFO("FLAC maxFrameSize: %u", m_flacMaxFrameSize); }
        else { AUDIO_INFO("FLAC maxFrameSize: N/A"); }
        if(m_flacMaxFrameSize > InBuff.getMaxBlockSize()) {
            AUDIO_LOG_ERROR("FLAC maxFrameSize too large!");
            stopSong();
            return -1;
        }
        //        InBuff.changeMaxBlockSize(m_flacMaxFrameSize);
        vTaskDelay(2);
        uint32_t nextval = bigEndian(data + 13, 3);
        m_flacSampleRate = nextval >> 4;
        AUDIO_INFO("FLAC sampleRate: %lu", (long unsigned int)m_flacSampleRate);
        vTaskDelay(2);
        m_flacNumChannels = ((nextval & 0x06) >> 1) + 1;
        AUDIO_INFO("FLAC numChannels: %u", m_flacNumChannels);
        vTaskDelay(2);
        uint8_t bps = (nextval & 0x01) << 4;
        bps += (*(data + 16) >> 4) + 1;
        m_flacBitsPerSample = bps;
        if((bps != 8) && (bps != 16)) {
            AUDIO_LOG_ERROR("bits per sample must be 8 or 16, is %i", bps);
            stopSong();
            return -1;
        }
        AUDIO_INFO("FLAC bitsPerSample: %u", m_flacBitsPerSample);
        m_flacTotalSamplesInStream = bigEndian(data + 17, 4);
        if(m_flacTotalSamplesInStream) { AUDIO_INFO("total samples in stream: %lu", (long unsigned int)m_flacTotalSamplesInStream); }
        else { AUDIO_INFO("total samples in stream: N/A"); }
        if(bps != 0 && m_flacTotalSamplesInStream) { AUDIO_INFO("audio file duration: %lu seconds", (long unsigned int)m_flacTotalSamplesInStream / (long unsigned int)m_flacSampleRate); }
        m_controlCounter = FLAC_MBH; // METADATA_BLOCK_HEADER
        m_rflh.retvalue = l + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_PADDING) { /* PADDING */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        m_rflh.retvalue = l + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_APP) { /* APPLICATION */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        m_rflh.retvalue = l + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_SEEK) { /* SEEKTABLE */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        m_rflh.retvalue = l + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_VORBIS) { /* VORBIS COMMENT */ // field names

        auto parse_flac_timestamp = [&](const char* s){
            if (!s || *s != '[') return -1;
            int mm = 0, ss = 0, hh = 0;
            if (sscanf(s, "[%d:%d.%d]", &mm, &ss, &hh) == 3) {
                return mm * 60000 + ss * 1000 + hh * 10;
            }
            return -1;
        };

        ps_ptr<char>vendorString = {};
        ps_ptr<char>commentString = {};
        ps_ptr<char>lyricsBuffer = {};
        size_t vendorLength = bigEndian(data, 3);
        size_t idx = 0;
        data += 3; idx += 3;
        size_t vendorStringLength = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
        if(vendorStringLength) {data += 4; idx += 4;}
        if(vendorStringLength > 495) vendorStringLength = 495; // guard
        vendorString.assign((const char*)data, vendorStringLength, "vendorString");
        vendorString.insert("VENDOR_STRING: ", 0);
        if(audio_id3data) audio_id3data(vendorString.get());
        data += vendorStringLength; idx += vendorStringLength;
        size_t commentListLength = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
        data += 4; idx += 4;

        for(int i = 0; i < commentListLength; i++) {
            (void)i;
            size_t commentLength = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
            data += 4; idx += 4;
            if(commentLength) { // guard
                commentString.assign((const char*)data , commentLength, "commentString");
                if(commentString.starts_with("LYRICS=")) lyricsBuffer.clone_from(commentString);
                else if(audio_id3data) audio_id3data(commentString.get());
            }
            data += commentLength; idx += commentLength;
            if(idx > vendorLength + 3) {AUDIO_LOG_ERROR("VORBIS COMMENT section is too long");}

        }
        ps_ptr<char> tmp;
        ps_ptr<char> timestamp;
        timestamp.alloc(12);
        if(lyricsBuffer.valid()){
            m_ID3Hdr.SYLT.seen = true;
            lyricsBuffer.remove_prefix("LYRICS=");
            idx = 0;
            while(idx < lyricsBuffer.size()){
                int pos = lyricsBuffer.index_of('\n', idx);
                if(pos == - 1) break;
                int len = pos - idx + 1;
                tmp.copy_from(lyricsBuffer.get() + idx, len);
                idx += len;
                if(tmp.ends_with("\n")) tmp.truncate_at('\n');
                // tmp content e.g.: [00:27.07]Jetzt kommt die Zeit
                //                   [00:28.84]Auf die ihr euch freut
                timestamp.copy_from(tmp.get(), 10);

                int ms = parse_flac_timestamp(timestamp.c_get());

                // timestamp.remove_chars("[]:.");
                // timestamp.append("0"); // to ms 0028840
                tmp.remove_before(10, true);
                if(tmp.strlen() > 0){
                    m_syltLines.push_back(std::move(tmp));
                    m_syltTimeStamp.push_back(ms);
                }
            }
            for(int i = 0; i < m_syltLines.size(); i++){
                AUDIO_INFO("%07i ms,   %s", m_syltTimeStamp[i], m_syltLines[i].c_get());
            }
        }
        m_controlCounter = FLAC_MBH;
        m_rflh.retvalue = vendorLength + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_CUESHEET) { /* CUESHEET */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        m_rflh.retvalue = l + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_PICTURE) { /* PICTURE */
        m_rflh.picLen = bigEndian(data, 3);
        m_rflh.picPos = m_rflh.headerSize;
        // AUDIO_LOG_INFO("FLAC PICTURE, size %i, pos %i", picLen, picPos);
        m_controlCounter = FLAC_MBH;
        m_rflh.retvalue = m_rflh.picLen + 3;
        m_rflh.headerSize += m_rflh.retvalue;
        return 0;
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Audio::read_ID3_Header(uint8_t* data, size_t len) {

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 0) { /* read ID3 tag and ID3 header size */
        m_controlCounter++;

        m_ID3Hdr.id3Size = 0;
        m_ID3Hdr.totalId3Size = 0; // if we have more header, id3_1_size + id3_2_size + ....
        m_ID3Hdr.remainingHeaderBytes = 0;
        m_ID3Hdr.universal_tmp = 0;
        m_ID3Hdr.ID3version = 0;
        m_ID3Hdr.ehsz = 0;
        m_ID3Hdr.framesize = 0;
        m_ID3Hdr.compressed = false;
        m_ID3Hdr.SYLT.seen = false;
        m_ID3Hdr.SYLT.size = 0;
        m_ID3Hdr.SYLT.pos = 0;
        m_ID3Hdr.numID3Header = 0;
        m_ID3Hdr.iBuffSize = 4096;
        m_ID3Hdr.iBuff.alloc(m_ID3Hdr.iBuffSize + 10, "iBuff");
        memset(m_ID3Hdr.tag, 0, sizeof(m_ID3Hdr.tag));
        memset(m_ID3Hdr.APIC_size, 0, sizeof(m_ID3Hdr.APIC_size));
        memset(m_ID3Hdr.APIC_pos, 0, sizeof(m_ID3Hdr.APIC_pos));
        memset(m_ID3Hdr.tag, 0, sizeof(m_ID3Hdr.tag));

        AUDIO_INFO("File-Size: %lu", m_audioFileSize);

        m_ID3Hdr.SYLT.seen = false;
        m_ID3Hdr.remainingHeaderBytes = 0;
        m_ID3Hdr.ehsz = 0;
        if(specialIndexOf(data, "ID3", 4) != 0) { // ID3 not found
            if(!m_f_m3u8data) {AUDIO_INFO("file has no ID3 tag, skip metadata");}
            m_audioDataSize = m_audioFileSize;
            if(!m_f_m3u8data) AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
            return -1; // error, no ID3 signature found
        }
        m_ID3Hdr.ID3version = *(data + 3);
        switch(m_ID3Hdr.ID3version) {
            case 2:
                m_f_unsync = (*(data + 5) & 0x80);
                m_f_exthdr = false;
                break;
            case 3:
            case 4:
                m_f_unsync = (*(data + 5) & 0x80); // bit7
                m_f_exthdr = (*(data + 5) & 0x40); // bit6 extended header
                break;
        };
        m_ID3Hdr.id3Size = bigEndian(data + 6, 4, 7); //  ID3v2 size  4 * %0xxxxxxx (shift left seven times!!)
        m_ID3Hdr.id3Size += 10;

        // Every read from now may be unsync'd
        if(!m_f_m3u8data) AUDIO_INFO("ID3 framesSize: %i", m_ID3Hdr.id3Size);
        if(!m_f_m3u8data) AUDIO_INFO("ID3 version: 2.%i", m_ID3Hdr.ID3version);

        if(m_ID3Hdr.ID3version == 2) { m_controlCounter = 10; }
        m_ID3Hdr.remainingHeaderBytes = m_ID3Hdr.id3Size;
        m_ID3Size = m_ID3Hdr.id3Size;
        m_ID3Hdr.remainingHeaderBytes -= 10;

        return 10;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 1) { // compute extended header size if exists
        m_controlCounter++;
        if(m_f_exthdr) {
            AUDIO_INFO("ID3 extended header");
            m_ID3Hdr.ehsz = bigEndian(data, 4);
            m_ID3Hdr.remainingHeaderBytes -= 4;
            m_ID3Hdr.ehsz -= 4;
            return 4;
        }
        else {
            if(!m_f_m3u8data) AUDIO_INFO("ID3 normal frames");
            return 0;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 2) { // skip extended header if exists
        if(m_ID3Hdr.ehsz > len) {
            m_ID3Hdr.ehsz -= len;
            m_ID3Hdr.remainingHeaderBytes -= len;
            return len;
        } // Throw it away
        else {
            m_controlCounter++;
            m_ID3Hdr.remainingHeaderBytes -= m_ID3Hdr.ehsz;
            return m_ID3Hdr.ehsz;
        } // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 3) { // read a ID3 frame, get the tag
        if(m_ID3Hdr.remainingHeaderBytes == 0) {
            m_controlCounter = 99;
            return 0;
        }
        m_controlCounter++;
        m_ID3Hdr.frameid[0] = *(data + 0);
        m_ID3Hdr.frameid[1] = *(data + 1);
        m_ID3Hdr.frameid[2] = *(data + 2);
        m_ID3Hdr.frameid[3] = *(data + 3);
        m_ID3Hdr.frameid[4] = 0;
        for(uint8_t i = 0; i < 4; i++) m_ID3Hdr.tag[i] = m_ID3Hdr.frameid[i]; // tag = frameid

        m_ID3Hdr.remainingHeaderBytes -= 4;
        if(m_ID3Hdr.frameid[0] == 0 && m_ID3Hdr.frameid[1] == 0 && m_ID3Hdr.frameid[2] == 0 && m_ID3Hdr.frameid[3] == 0) {
            // We're in padding
            m_controlCounter = 98; // all ID3 metadata processed
        }
        return 4;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 4) { // get the frame size
        m_controlCounter = 6;

        if(m_ID3Hdr.ID3version == 4) {
            m_ID3Hdr.framesize = bigEndian(data, 4, 7); // << 7
        }
        else {
            m_ID3Hdr.framesize = bigEndian(data, 4); // << 8
        }
        m_ID3Hdr.remainingHeaderBytes -= 4;
        uint8_t frameFlag_0 = *(data + 4);
        uint8_t frameFlag_1 = *(data + 5);
        (void)frameFlag_0;
        m_ID3Hdr.remainingHeaderBytes--;
        m_ID3Hdr.compressed = frameFlag_1 & 0x80; // Frame is compressed using [#ZLIB zlib] with 4 bytes for 'decompressed
        m_ID3Hdr.remainingHeaderBytes--;
        uint32_t decompsize = 0;
        if(m_ID3Hdr.compressed) {
            // AUDIO_LOG_INFO("iscompressed");
            decompsize = bigEndian(data + 6, 4);
            m_ID3Hdr.remainingHeaderBytes -= 4;
            (void)decompsize;
            // AUDIO_LOG_INFO("decompsize=%u", decompsize);
            return 6 + 4;
        }
        return 6;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 5) { // If the frame is larger than m_ID3Hdr.framesize, skip the rest
        if(m_ID3Hdr.framesize > len) {
            m_ID3Hdr.framesize -= len;
            m_ID3Hdr.remainingHeaderBytes -= len;
            return len;
        }
        else {
            m_controlCounter = 3; // check next frame
            m_ID3Hdr.remainingHeaderBytes -= m_ID3Hdr.framesize;
            return m_ID3Hdr.framesize;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 6) { // Read the value
        m_controlCounter = 5;   // only read 256 bytes

        uint8_t textEncodingByte = *(data + 0);  // ID3v2 Text-Encoding-Byte
        // $00 – ISO-8859-1 (LATIN-1, Identical to ASCII for values smaller than 0x80).
        // $01 – UCS-2 encoded Unicode with BOM (Byte Order Mark), in ID3v2.2 and ID3v2.3.
        // $02 – UTF-16BE encoded Unicode without BOM (Byte Order Mark) , in ID3v2.4.
        // $03 – UTF-8 encoded Unicode, in ID3v2.4.

        if(startsWith(m_ID3Hdr.tag, "APIC")) { // a image embedded in file, passing it to external function
        //    if(m_dataMode == AUDIO_LOCALFILE) {
                m_ID3Hdr.APIC_pos[m_ID3Hdr.numID3Header] = m_ID3Hdr.totalId3Size + m_ID3Hdr.id3Size - m_ID3Hdr.remainingHeaderBytes;
                m_ID3Hdr.APIC_size[m_ID3Hdr.numID3Header] = m_ID3Hdr.framesize;
                //    AUDIO_LOG_ERROR("APIC_pos %i APIC_size %i", APIC_pos[numID3Header], APIC_size[numID3Header]);
        //    }
            return 0;
        }

        if(startsWith(m_ID3Hdr.tag, "SYLT") || startsWith(m_ID3Hdr.tag, "USLT")) { // any lyrics embedded in file, passing it to external function
            m_controlCounter = 7;
            return 0;
        }


        if( // proprietary not standard information
            startsWith(m_ID3Hdr.tag, "PRIV")) {
                ;//AUDIO_LOG_ERROR("PRIV");
                return 0;
        }

        if(m_ID3Hdr.framesize == 0) return 0;

        ps_ptr<char> tmp;
        ps_ptr<char> content_descriptor;
        size_t fs = m_ID3Hdr.framesize; // fs = size of the frame data field as read from header
        size_t bytesToCopy = fs;
        size_t textDataLength = 0;
        uint16_t idx = 0;

        if (bytesToCopy >= m_ID3Hdr.iBuffSize) { bytesToCopy = m_ID3Hdr.iBuffSize - 1;} // make sure a zero terminator fits
        if (bytesToCopy > 0) { textDataLength = bytesToCopy - 1;}                       // Only if there are data that we can shorten
        for (int i = 0; i < textDataLength; i++) {
            m_ID3Hdr.iBuff[i] = *(data + i + 1);                                        // Skipped the first byte (Encoding)
        }

        if (textEncodingByte == 1 || textEncodingByte == 2) {                           // is UTF-16LE or UTF-16BE
            m_ID3Hdr.iBuff[textDataLength] = 0;                                         // UTF-16: set double zero terminator
            m_ID3Hdr.iBuff[textDataLength + 1] = 0;                                     // second '\0' for UTF-16
        } else {
            m_ID3Hdr.iBuff[textDataLength] = 0;                                         // only one '\0' for ISO-8859-1 or UTF-8
        }

        m_ID3Hdr.framesize -= fs;
        m_ID3Hdr.remainingHeaderBytes -= fs;
        uint16_t dataLength = fs - 1;
        bool isBigEndian = (textEncodingByte == 2);

        char encodingTab [4][12] = {"ISO-8859-1", "UTF-16", "UTF-16BE", "UTF-8"};

        if(startsWith(m_ID3Hdr.tag, "COMM")){ // language code
            m_ID3Hdr.lang[0] = m_ID3Hdr.iBuff[0];
            m_ID3Hdr.lang[1] = m_ID3Hdr.iBuff[1];
            m_ID3Hdr.lang[2] = m_ID3Hdr.iBuff[2];
            m_ID3Hdr.lang[3] = '\0';

            idx = 4;
            if(textEncodingByte == 1 || textEncodingByte == 2) idx++;

            uint16_t cd_len = 0;
            if     (textEncodingByte == 0) cd_len = 1 + content_descriptor.copy_from_iso8859_1((const uint8_t*)(m_ID3Hdr.iBuff.get() + idx), "content_descriptor");         // iso8859_1
            else if(textEncodingByte == 3) cd_len = 1 + content_descriptor.copy_from((const char*)(m_ID3Hdr.iBuff.get() + idx), "content_descriptor");                      // utf-8
            else                           cd_len = 2 + content_descriptor.copy_from_utf16((const uint8_t*)(m_ID3Hdr.iBuff.get() + idx), isBigEndian, "content_descriptor");// utf-16
            if(cd_len > 2) AUDIO_INFO("Comment: text encoding; %s, language %s, content_descriptor: %s", encodingTab[textEncodingByte], m_ID3Hdr.lang, content_descriptor.c_get());

            idx += cd_len;
        }

        // AUDIO_LOG_INFO("Tag: %s, Length: %i, Format: %s", m_ID3Hdr.tag, textDataLength, encodingTab[textEncodingByte]);

        if      (textEncodingByte == 0)                          {tmp.copy_from_iso8859_1((const uint8_t*)m_ID3Hdr.iBuff.get() + idx, "tmp");} // ISO-8859-1
        else if (textEncodingByte == 1 || textEncodingByte == 2) {tmp.copy_from_utf16((const uint8_t*)m_ID3Hdr.iBuff.get() + idx, isBigEndian, "tmp");} // UTF-16LE oder UTF-16BE
        else if (textEncodingByte == 3)                          {tmp.copy_from(m_ID3Hdr.iBuff.get() + idx);} // UTF-8 copy directly because no conversion is necessary

        showID3Tag(m_ID3Hdr.tag, tmp.c_get());

        return fs;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 7) { // SYLT
        m_controlCounter = 5;
        if(m_dataMode == AUDIO_LOCALFILE || (m_streamType == ST_WEBFILE && m_f_acceptRanges))  {
            ps_ptr<char> tmp;
            ps_ptr<char> content_descriptor;
            ps_ptr<char> syltBuff;
            bool isBigEndian = true;
            size_t len = 0;
            int idx = 0;
            m_ID3Hdr.SYLT.seen = true;
            m_ID3Hdr.SYLT.pos = m_ID3Hdr.id3Size - m_ID3Hdr.remainingHeaderBytes;
            m_ID3Hdr.SYLT.size = m_ID3Hdr.framesize;
            syltBuff.alloc(m_ID3Hdr.SYLT.size + 1, "syltBuff");
            if((m_streamType == ST_WEBFILE && m_f_acceptRanges) || (m_dataMode == AUDIO_LOCALFILE)){
                uint32_t pos = m_audioFilePosition;
            //    AUDIO_LOG_INFO("m_audioFilePosition %i, m_ID3Hdr.SYLT.pos %i", pos, m_ID3Hdr.SYLT.pos);
                audioFileSeek(m_ID3Hdr.SYLT.pos, m_ID3Hdr.SYLT.pos + m_ID3Hdr.SYLT.size);
                uint16_t bytesWritten = 0;
                while(bytesWritten < m_ID3Hdr.SYLT.size){
                    bytesWritten += audioFileRead((uint8_t*)syltBuff.get() + bytesWritten, m_ID3Hdr.SYLT.size);
                }
                audioFileSeek(pos);
            }
        //    syltBuff.hex_dump(10);
            m_ID3Hdr.SYLT.text_encoding = syltBuff[0]; // 0=ISO-8859-1, 1=UTF-16, 2=UTF-16BE, 3=UTF-8
            if(m_ID3Hdr.SYLT.text_encoding == 1) isBigEndian = false;
            if(m_ID3Hdr.SYLT.text_encoding > 3){AUDIO_LOG_ERROR("unknown text encoding: %i", m_ID3Hdr.SYLT.text_encoding), m_ID3Hdr.SYLT.text_encoding = 0;}
            char encodingTab [4][12] = {"ISO-8859-1", "UTF-16", "UTF-16BE", "UTF-8"};
            memcpy(m_ID3Hdr.SYLT.lang, syltBuff.get() + 1, 3); m_ID3Hdr.SYLT.lang[3] = '\0';
            AUDIO_INFO("Lyrics: text_encoding: %s, language: %s, size %i", encodingTab[m_ID3Hdr.SYLT.text_encoding], m_ID3Hdr.SYLT.lang, m_ID3Hdr.SYLT.size);
            m_ID3Hdr.SYLT.time_stamp_format =  syltBuff[4];
            m_ID3Hdr.SYLT.content_type =       syltBuff[5];
            idx = 6;
            uint16_t cd_len = 0;
            if     (m_ID3Hdr.SYLT.text_encoding == 0) cd_len = 1 + content_descriptor.copy_from_iso8859_1((const uint8_t*)(syltBuff.get() + idx), "content_descriptor");         // iso8859_1
            else if(m_ID3Hdr.SYLT.text_encoding == 3) cd_len = 1 + content_descriptor.copy_from((const char*)(syltBuff.get() + idx), "content_descriptor");                      // utf-8
            else                                      cd_len = 2 + content_descriptor.copy_from_utf16((const uint8_t*)(syltBuff.get() + idx), isBigEndian, "content_descriptor");// utf-16
            if(cd_len > 2) AUDIO_INFO("Lyrics: content_descriptor: %s", content_descriptor.c_get());

            idx += cd_len;
            while (idx < m_ID3Hdr.SYLT.size) {
                if      (m_ID3Hdr.SYLT.text_encoding == 0) idx += 1 + tmp.copy_from_iso8859_1((const uint8_t*)syltBuff.get() + idx, "sylt-text");            // ISO8859_1
                else if (m_ID3Hdr.SYLT.text_encoding == 3) idx += 1 + tmp.copy_from((const char*)syltBuff.get() + idx, "sylt-text");                         // UTF-8
                else                                       idx += 2 + tmp.copy_from_utf16((const uint8_t*)(syltBuff.get() + idx), isBigEndian, "sylt-text"); // UTF-16LE, UTF-16BE

                if (tmp.starts_with("\n")) tmp.remove_before(1);
                m_syltLines.push_back(std::move(tmp));
                if (idx + 4 > m_ID3Hdr.SYLT.size) break; // no more 4 bytes?
                uint32_t timestamp = bigEndian((uint8_t*)syltBuff.get() + idx, 4);
                m_syltTimeStamp.push_back(timestamp);
                idx += 4;
            }
            for(int i = 0; i < m_syltLines.size(); i++){
                AUDIO_INFO("%07i ms,   %s", m_syltTimeStamp[i], m_syltLines[i].c_get());
            }
        }
        return 0;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    // --- section V2.2 only , higher Vers above ----
    // see https://mutagen-specs.readthedocs.io/en/latest/id3/id3v2.2.html
    if(m_controlCounter == 10) { // frames in V2.2, 3bytes identifier, 3bytes size descriptor

        if(m_ID3Hdr.universal_tmp > 0) {
            if(m_ID3Hdr.universal_tmp > len) {
                m_ID3Hdr.universal_tmp -= len;
                return len;
            } // Throw it away
            else {
                uint32_t t = m_ID3Hdr.universal_tmp;
                m_ID3Hdr.universal_tmp = 0;
                return t;
            } // Throw it away
        }

        m_ID3Hdr.frameid[0] = *(data + 0);
        m_ID3Hdr.frameid[1] = *(data + 1);
        m_ID3Hdr.frameid[2] = *(data + 2);
        m_ID3Hdr.frameid[3] = 0;
        for(uint8_t i = 0; i < 4; i++) m_ID3Hdr.tag[i] = m_ID3Hdr.frameid[i]; // tag = frameid
        m_ID3Hdr.remainingHeaderBytes -= 3;
        size_t dataLen = bigEndian(data + 3, 3);
        m_ID3Hdr.universal_tmp = dataLen;
        m_ID3Hdr.remainingHeaderBytes -= 3;
        char value[256];
        if(dataLen > 249) { dataLen = 249; }
        memcpy(value, (data + 7), dataLen);
        value[dataLen + 1] = 0;
        if(startsWith(m_ID3Hdr.tag, "PIC")) { // image embedded in header
            if(m_dataMode == AUDIO_LOCALFILE) {
                m_ID3Hdr.APIC_pos[m_ID3Hdr.numID3Header] = m_ID3Hdr.id3Size - m_ID3Hdr.remainingHeaderBytes;
                m_ID3Hdr.APIC_size[m_ID3Hdr.numID3Header] = m_ID3Hdr.universal_tmp;
                // AUDIO_LOG_INFO("Attached picture seen at pos %d length %d", APIC_pos[0], APIC_size[0]);
            }
        }
        else if(startsWith(m_ID3Hdr.tag, "SLT")) { // lyrics embedded in header
            if(m_dataMode == AUDIO_LOCALFILE) {

                ps_ptr<char> tmp;
                ps_ptr<char> content_descriptor;
                ps_ptr<char> syltBuff;
                bool isBigEndian = true;
                size_t len = 0;
                int idx = 0;

                m_ID3Hdr.SYLT.seen = true; // #460
                m_ID3Hdr.SYLT.pos = m_ID3Hdr.id3Size - m_ID3Hdr.remainingHeaderBytes;
                m_ID3Hdr.SYLT.size = m_ID3Hdr.universal_tmp;

                syltBuff.alloc(m_ID3Hdr.SYLT.size, "syltBuff");
                uint32_t pos = m_audioFilePosition;
                audioFileSeek(m_ID3Hdr.SYLT.pos);
                uint16_t bytesWritten = 0;
                while(bytesWritten < m_ID3Hdr.SYLT.size){
                    bytesWritten += audioFileRead((uint8_t*)syltBuff.get() + bytesWritten, m_ID3Hdr.SYLT.size);
                }
                audioFileSeek(pos);
                m_ID3Hdr.SYLT.text_encoding = syltBuff[0];
                memcpy(m_ID3Hdr.SYLT.lang, syltBuff.get() + 1, 3); m_ID3Hdr.SYLT.lang[3] = '\0';
                AUDIO_INFO("Lyrics: text_encoding: %s, language: %s, size %i", m_ID3Hdr.SYLT.text_encoding == 0 ? "ASCII" : m_ID3Hdr.SYLT.text_encoding == 3 ? "UTF-8" : "?", m_ID3Hdr.SYLT.lang, m_ID3Hdr.SYLT.size);
                m_ID3Hdr.SYLT.time_stamp_format =  syltBuff[4];
                m_ID3Hdr.SYLT.content_type =       syltBuff[5];

                idx = 6;

                if(m_ID3Hdr.SYLT.text_encoding == 0 || m_ID3Hdr.SYLT.text_encoding == 3){ // utf-8
                    len = content_descriptor.copy_from((const char*)(syltBuff.get() + idx), "content_descriptor");
                }
                else{ // utf-16
                    len = content_descriptor.copy_from_utf16((const uint8_t*)(syltBuff.get() + idx), isBigEndian, "content_descriptor");
                }
                if(len > 2) AUDIO_INFO("Lyrics: content_descriptor: %s", content_descriptor.c_get());
                idx += len;

                while (idx < m_ID3Hdr.SYLT.size) {
                        // UTF-16LE, UTF-16BE
                    if (m_ID3Hdr.SYLT.text_encoding == 1 || m_ID3Hdr.SYLT.text_encoding == 2) {
                        idx += tmp.copy_from_utf16((const uint8_t*)(syltBuff.get() + idx), isBigEndian, "sylt-text");
                    } else {
                        // ISO-8859-1 / UTF-8
                        idx += tmp.copy_from((const char*)syltBuff.get() + idx, "sylt-text");
                    }
                    if (tmp.starts_with("\n")) tmp.remove_before(1);
                    m_syltLines.push_back(std::move(tmp));

                    if (idx + 4 > m_ID3Hdr.SYLT.size) break; // no more 4 bytes?

                    uint32_t timestamp = bigEndian((uint8_t*)syltBuff.get() + idx, 4);
                    m_syltTimeStamp.push_back(timestamp);

                    idx += 4;
                }
                // for(int i = 0; i < m_syltLines.size(); i++){
                //     AUDIO_INFO("%07i ms,   %s", m_syltTimeStamp[i], m_syltLines[i].c_get());
                // }
            }
        }
        else { showID3Tag(m_ID3Hdr.tag, value);}
        m_ID3Hdr.remainingHeaderBytes -= m_ID3Hdr.universal_tmp;
        m_ID3Hdr.universal_tmp -= dataLen;

        if(dataLen == 0) m_controlCounter = 98;
        if(m_ID3Hdr.remainingHeaderBytes == 0) m_controlCounter = 98;

        return 3 + 3 + dataLen;
    }
    // -- end section V2.2 -----------

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 98) { // skip all ID3 metadata (mostly spaces)
        if(m_ID3Hdr.remainingHeaderBytes > len) {
            m_ID3Hdr.remainingHeaderBytes -= len;
            return len;
        } // Throw it away
        else {
            m_controlCounter = 99;
            return m_ID3Hdr.remainingHeaderBytes;
        } // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 99) { //  exist another ID3tag?
        m_audioDataStart += m_ID3Hdr.id3Size;
        //    vTaskDelay(30);
        if((*(data + 0) == 'I') && (*(data + 1) == 'D') && (*(data + 2) == '3')) {
            m_controlCounter = 0;
            m_ID3Hdr.numID3Header++;
            m_ID3Hdr.totalId3Size += m_ID3Hdr.id3Size;
            return 0;
        }
        else {
            m_controlCounter = 100; // ok
            m_audioDataSize = m_audioFileSize - m_audioDataStart;
            if(!m_f_m3u8data) AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
            if(m_ID3Hdr.APIC_pos[0] && audio_id3image) { // if we have more than one APIC, output the first only
                size_t pos = m_audioFilePosition;
                audio_id3image(m_audiofile, m_ID3Hdr.APIC_pos[0], m_ID3Hdr.APIC_size[0]);
                audioFileSeek(pos); // the filepointer could have been changed by the user, set it back
            }
            m_ID3Hdr.numID3Header = 0;
            m_ID3Hdr.totalId3Size = 0;
            for(int i = 0; i < 3; i++) m_ID3Hdr.APIC_pos[i] = 0;  // delete all
            for(int i = 0; i < 3; i++) m_ID3Hdr.APIC_size[i] = 0; // delete all
            m_ID3Hdr.iBuff.reset();
            return 0;
        }
    }
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Audio::read_M4A_Header(uint8_t* data, size_t len) {

    ps_ptr<char>atom_name = {};
    ps_ptr<char>atom_size = {};
    uint32_t idx = 0;

    // Lambda function for Variant length determination
    auto parse_variant_length = [](uint8_t *&ptr) -> int {
        int length = 0;
        do {
            length = (length << 7) | (*ptr & 0x7F);  // Read the lower 7 bits of the current byte
        } while (*(ptr++) & 0x80);  //Increment the pointer after each byte
        return length;
    };

    /*
         ftyp
           | - moov  -> trak -> ... -> mp4a contains raw block parameters
           |    L... -> ilst  contains artist, composer ....
         free (optional) // jump to another atoms at the end of mdat
           |
         mdat contains the audio data                                                      */

    if(m_m4aHdr.retvalue) {
        if(m_m4aHdr.retvalue > UINT16_MAX){
            int res = audioFileSeek(m_m4aHdr.headerSize);
            if(res >= 0){
                AUDIO_LOG_INFO("skip %li bytes", m_m4aHdr.retvalue);
                InBuff.resetBuffer();
                m_m4aHdr.retvalue = 0;
               return 0;
            }
        }
        if(m_m4aHdr.retvalue > len) {
            m_m4aHdr.retvalue -= len;
            m_m4aHdr.cnt += len;
            return len;
        }
        else {
            size_t tmp = m_m4aHdr.retvalue;
            m_m4aHdr.retvalue = 0;
            m_m4aHdr.cnt += tmp;
            return tmp;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_BEGIN) { // init
        memset(&m_m4aHdr, 0, sizeof(m4aHdr_t));

        atom_size.big_endian(data, 4);
        atom_name.copy_from((const char*)data + 4, 4);

        if(atom_name.equals("ftyp")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_ftyp = atom_size.to_uint32(16);
            m_controlCounter = M4A_FTYP;
        }
        else{
            AUDIO_LOG_ERROR("begin: ftyp not found");
            stopSong();
            return -1;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_FTYP) { /* check_m4a_file */

        int m4a  = specialIndexOf(data, "M4A ", 20);
        int isom = specialIndexOf(data, "isom", 20);
        int mp42 = specialIndexOf(data, "mp42", 20);

        if((m4a != 8) && (isom != 8) && (mp42 != 8)) {
            AUDIO_LOG_ERROR("subtype 'MA4 ', 'isom' or 'mp42' expected, but found '%s '", (data + 8));
            stopSong();
            return -1;
        }

        m_m4aHdr.retvalue += m_m4aHdr.sizeof_ftyp;
        m_m4aHdr.headerSize += m_m4aHdr.sizeof_ftyp;
        m_controlCounter = M4A_CHK;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_CHK) {  /* check  Tag */
        atom_size.big_endian(data, 4);
        atom_name.copy_from((const char*)data + 4, 4);

        if(atom_name.equals("moov")){
            m_m4aHdr.progressive = true;
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_moov = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_MOOV;
            return 0;
        }

        if(atom_name.equals("mdat")){
            if(!m_m4aHdr.progressive){
                AUDIO_LOG_DEBUG("non progressive file can't be played yet"); // mdat before moov, todo: skip mdat
                stopSong();
                return -1;
            }
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_mdat = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_MDAT;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        m_m4aHdr.sizeof_moov -= atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_MOOV) { // moov
        AUDIO_LOG_DEBUG("moov size remain %i", m_m4aHdr.sizeof_moov);
        if(m_m4aHdr.sizeof_moov == 0) {m_controlCounter = M4A_CHK; return 0;} // go back

        atom_name.copy_from((const char*)data + 4, 4);
        atom_size.big_endian(data, 4);
        m_m4aHdr.sizeof_moov -= atom_size.to_uint32(16);

        if(atom_name.equals("trak")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_trak = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_TRAK;
            return 0;
        }

        if(atom_name.equals("udta")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_udta = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_UDTA;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_TRAK) { // trak
        AUDIO_LOG_DEBUG("trak size remain %i", m_m4aHdr.sizeof_trak);
        if(m_m4aHdr.sizeof_trak == 0){ m_controlCounter = M4A_MOOV; return 0;}// go back

        atom_name.copy_from((const char*)data + 4, 4);
        atom_size.big_endian(data, 4);
        m_m4aHdr.sizeof_trak -= atom_size.to_uint32(16);

        if(atom_name.equals("mdia")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_mdia = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_MDIA;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_MDIA) { // mdia
        AUDIO_LOG_DEBUG("mdia size remain %i", m_m4aHdr.sizeof_mdia);
        if(m_m4aHdr.sizeof_mdia == 0) {m_controlCounter = M4A_TRAK; return 0;} // go back

        atom_name.copy_from((const char*)data + 4, 4);
        atom_size.big_endian(data, 4);
        m_m4aHdr.sizeof_mdia -= atom_size.to_uint32(16);

        if(atom_name.equals("minf")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_minf = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_MINF;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_MINF) { // minf
        AUDIO_LOG_DEBUG("minf size remain %i", m_m4aHdr.sizeof_minf);
        if(m_m4aHdr.sizeof_minf == 0) {m_controlCounter = M4A_MDIA; return 0;} // go back

        atom_name.copy_from((const char*)data + 4, 4);
        atom_size.big_endian(data, 4);
        m_m4aHdr.sizeof_minf -= atom_size.to_uint32(16);

        if(atom_name.equals("stbl")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_stbl = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_STBL;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_STBL) { // minf
        AUDIO_LOG_DEBUG("stbl size remain %i", m_m4aHdr.sizeof_stbl);
        if(m_m4aHdr.sizeof_stbl == 0) {m_controlCounter = M4A_MINF; return 0;} // go back

        atom_size.big_endian(data, 4);
        atom_name.copy_from((const char*)data + 4, 4);
        m_m4aHdr.sizeof_stbl -= atom_size.to_uint32(16);

        if(atom_name.equals("stsd")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            uint8_t header_size = 8;
            m_m4aHdr.sizeof_stsd = atom_size.to_uint32(16) - 8 - header_size;
            m_m4aHdr.retvalue += 8 + header_size;
            m_m4aHdr.headerSize += 8 + header_size;
            m_controlCounter = M4A_STSD;
            return 0;
        }

        if(atom_name.equals("stsz")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            uint8_t header_size = 8;
            m_m4aHdr.sizeof_stsz = atom_size.to_uint32(16) - 8 - header_size;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_STSZ;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_STSD){
        AUDIO_LOG_DEBUG("stsd size remain %i", m_m4aHdr.sizeof_stsd);
        if(m_m4aHdr.sizeof_stsd == 0) {m_controlCounter = M4A_STBL; return 0;} // go back

        atom_size.big_endian(data, 4);
        atom_name.copy_from((const char*)data + 4, 4);
        m_m4aHdr.sizeof_stsd -= atom_size.to_uint32(16);

        if(atom_name.equals("mp4a")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_mp4a = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_MP4A;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_MP4A){
        AUDIO_LOG_DEBUG("mp4a size remain %i", m_m4aHdr.sizeof_mp4a);
        if(m_m4aHdr.sizeof_mp4a == 0) {m_controlCounter = M4A_STSD; return 0;} // go back

        if(!m_m4aHdr.sample_rate){ // read the header first
            // data[0]...[5] reserved
            // data[6] & [7] Data reference index
            // data[8]...[15] reserved
            m_m4aHdr.channel_count = data[16] * 256 + data[17]; AUDIO_LOG_DEBUG("channels %i", m_m4aHdr.channel_count);
            m_m4aHdr.sample_size = data [18] *256 + data[19]; AUDIO_LOG_DEBUG("bit per sample %i", m_m4aHdr.sample_size);
            // data[20]...data[23] reserved
            m_m4aHdr.sample_rate = data[24] * 256 + data[25]; AUDIO_LOG_DEBUG("sample rate %i", m_m4aHdr.sample_rate);
            // data[26]...data[27] sample rate fixed point is 0x00, 0x00
            m_m4aHdr.offset = 28;
            m_m4aHdr.retvalue += m_m4aHdr.offset;
            m_m4aHdr.headerSize += m_m4aHdr.offset;
            m_m4aHdr.sizeof_mp4a -= m_m4aHdr.offset;
            return 0;
        }
        atom_size.big_endian(data, 4);
        atom_name.copy_from((const char*)data + 4, 4);
        m_m4aHdr.sizeof_mp4a -= atom_size.to_uint32(16);

        if(atom_name.equals("esds")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_esds = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_ESDS;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_UDTA){ // udta
        AUDIO_LOG_DEBUG("udta size remain %i", m_m4aHdr.sizeof_udta);
        if(m_m4aHdr.sizeof_udta == 0) {m_controlCounter = M4A_MOOV; return 0;} // go back

        atom_size.big_endian(data, 4);
        atom_name.copy_from((const char*)data + 4, 4);
        m_m4aHdr.sizeof_udta -= atom_size.to_uint32(16);

       if(atom_name.equals("meta")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_meta = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_META;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_META){ // meta
        AUDIO_LOG_DEBUG("meta size remain %i", m_m4aHdr.sizeof_meta);
        if(m_m4aHdr.sizeof_meta == 0) {m_controlCounter = M4A_UDTA; return 0;} // go back,  4bytes typ + 4 bytes size

        if(!m_m4aHdr.version_flags){
            //0x00, 0x00, 0x00, 0x94, 0x6D, 0x65, 0x74, 0x61, 0x00, 0x00, 0x00, 0x00,
            // size                 |   m    e     t     a  |     flags
            m_m4aHdr.version_flags = true;
            m_m4aHdr.sizeof_meta -= 4;
            m_m4aHdr.retvalue += 4;
            m_m4aHdr.headerSize += 4;
            idx += 4;
        }
        atom_size.big_endian(data + idx, 4);
        atom_name.copy_from((const char*)data + 4 + idx, 4);
        m_m4aHdr.sizeof_meta -= atom_size.to_uint32(16);

       if(atom_name.equals("ilst")){
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
            m_m4aHdr.sizeof_ilst = atom_size.to_uint32(16) - 8;
            m_m4aHdr.retvalue += 8;
            m_m4aHdr.headerSize += 8;
            m_controlCounter = M4A_ILST;
            return 0;
        }

        AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), m_m4aHdr.headerSize, atom_size.to_uint32(16), m_m4aHdr.headerSize + atom_size.to_uint32(16));
        m_m4aHdr.retvalue += atom_size.to_uint32(16);
        m_m4aHdr.headerSize += atom_size.to_uint32(16);
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_ESDS){ // Elementary Stream Descriptor
        ps_ptr<char> esds_buffer; // read ESDS content in a buffer
        esds_buffer.copy_from((char*)data, m_m4aHdr.sizeof_esds); // read ESDS content (without header)
        // esds_buffer.hex_dump(m_m4aHdr.sizeof_esds);

        // search for decoderspecificinfo (tag 0x05)
        int32_t dec_specific_offset = esds_buffer.special_index_of("\x05\x80\x80\x80", 4, m_m4aHdr.sizeof_esds);
        if (dec_specific_offset >= 0) {// decoderSpecificInfo found
            uint8_t dec_specific_length = ((uint8_t*)esds_buffer.get())[dec_specific_offset + 4]; // Länge nach Tag + 3 Extended Length Bytes
            AUDIO_LOG_DEBUG("DecoderSpecificInfo found at offset %d, length: %u", dec_specific_offset, dec_specific_length);

            // extract decoderSpecificInfo-data
            ps_ptr<char> dec_specific_data;
            dec_specific_data.alloc(dec_specific_length);
            memcpy(dec_specific_data.get(), (uint8_t*)esds_buffer.get() + dec_specific_offset + 5, dec_specific_length);
            m_m4aHdr.aac_profile = (uint8_t)dec_specific_data.get()[0] >> 3;
            if(m_m4aHdr.aac_profile > 4) {AUDIO_LOG_ERROR("unsupported AAC Profile %i", m_m4aHdr.aac_profile); stopSong(); return 0;}
            AUDIO_LOG_DEBUG("DecoderSpecificInfo data: 0x%02X 0x%02X", (uint8_t)dec_specific_data.get()[0], (uint8_t)dec_specific_data.get()[1]);
            char profile[5][33] = {"unknown", "AAC Main", "AAC LC (Low Complexity)", "AAC SSR (Scalable Sample Rate)", "AAC LTP (Long Term Prediction)"};
            AUDIO_INFO("AAC Profile: %s", profile[m_m4aHdr.aac_profile]);
            AUDIO_INFO("AAC Channels; %i", m_m4aHdr.channel_count);
            AUDIO_INFO("AAC Sample Rate: %i", m_m4aHdr.sample_rate);
            AUDIO_INFO("AAC Bits Per Sample: %i", m_m4aHdr.sample_size);
            m_M4A_chConfig = m_m4aHdr.channel_count;
            m_M4A_sampleRate = m_m4aHdr.sample_rate;
            m_M4A_objectType = m_m4aHdr.aac_profile;
        } else {
            log_e("No DecoderSpecificInfo found in esds");
        }
        m_m4aHdr.retvalue += m_m4aHdr.sizeof_esds;
        m_m4aHdr.headerSize += m_m4aHdr.sizeof_esds;
        m_controlCounter = M4A_MP4A;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_ILST){

        struct TagInfo { // Definition of the Taginfo structure for iTunes-style metadata
            const uint8_t tag[4];
            const char* name;
            const char* descr;
        };
        const TagInfo tags[] = { // List of all usual tags
            {{ 0xA9, 0x6E, 0x61, 0x6D }, "©nam", "Title"},
            {{ 0xA9, 0x41, 0x52, 0x54 }, "©ART", "Artist"},
            {{ 0xA9, 0x61, 0x72, 0x74 }, "©art", "Artist"},
            {{ 0xA9, 0x61, 0x6C, 0x62 }, "©alb", "Album"},
            {{ 0xA9, 0x74, 0x6F, 0x6F }, "©too", "Encoder"},
            {{ 0xA9, 0x63, 0x6D, 0x74 }, "©cmt", "Comment"},
            {{ 0xA9, 0x77, 0x72, 0x74 }, "©wrt", "Composer"},
            {{ 0x74, 0x6D, 0x70, 0x6F }, "tmpo", "Tempo (BPM)"},
            {{ 0x74, 0x72, 0x6B, 0x6E }, "trkn", "Track-Number"},
            {{ 0xA9, 0x64, 0x61, 0x79 }, "©day", "Year"},
            {{ 0x63, 0x70, 0x69, 0x6C }, "cpil", "Compilation-Flag"},
            {{ 0x61, 0x41, 0x52, 0x54 }, "aART", "Album Artist"},
            {{ 0xA9, 0x67, 0x65, 0x6E }, "©gen", "Genre"},
            {{ 0x63, 0x6F, 0x76, 0x72 }, "covr", "Cover Art"},
            {{ 0x64, 0x69, 0x73, 0x6B }, "disk", "Disk-Nummer"},
            {{ 0xA9, 0x6C, 0x79, 0x72 }, "©lyr", "Songtext"},
            {{ 0xA9, 0x70, 0x72, 0x74 }, "cprt", "Copyright"},
            {{ 0x67, 0x6E, 0x72, 0x65 }, "gnre", "Genre-ID"},
            {{ 0x72, 0x74, 0x6E, 0x67 }, "rtng", "Evaluation"},
            {{ 0x70, 0x67, 0x61, 0x70 }, "pgap", "Gapless Playback"},
        };
        const size_t tags_count = sizeof(tags) / sizeof(tags[0]); // Number of tags

        ps_ptr<char>id3tag = {};
        uint32_t pos = m_m4aHdr.headerSize;
        uint32_t consumed = 0;
        while(true){
            atom_size.big_endian(data + consumed, 4);
            atom_name.copy_from((const char*)data + 4 + consumed, 4);
            uint32_t as = atom_size.to_uint32(16);
            AUDIO_LOG_DEBUG("atom %s @ %i, size: %i, ends @ %i", atom_name.c_get(), pos, as, pos + as);
            m_m4aHdr.ilst_pos += as;
            pos += as;
            consumed += as;


            // 0x00, 0x00, 0x00, 0x1C, 0xA9, 0x64, 0x61, 0x79, 0x00, 0x00, 0x00, 0x14, 0x64, 0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x32, 0x30, 0x32, 0x32,
            //     sub atom length   |   ©    d     a     y  |  sub sub atom length  |  d     a     t     a  | data type   1->UTF-8  |   reserved            |  2     0     2     2

            ps_ptr<char>sa; // sub atom
            sa.copy_from((const char*)data + consumed - as, min(as, 1024));
            char san [5] = {0}; // aub atom name
            char ssan[5] = {0}; // sub sub atom name (should be 'data')
            strncpy(san, &sa[4], 4);
            strncpy(ssan, &sa[12],4);
            uint32_t ssal = bigEndian((uint8_t*)&sa[8], 4); // sub sub atom length
            uint32_t dty  = bigEndian((uint8_t*)&sa[16], 4); // data type 1-UTF8
            sa.clear();
            if(strncmp(ssan, "data", 4) == 0){
                for(int i = 0; i < tags_count; i++){
                    if(memcmp(san, tags[i].tag, 4) == 0){
                        if     (dty == 0 && strlen(&sa[24]) > 0) id3tag.assignf("%s: %i", tags[i].descr, &sa[24]); // binary
                        else if(dty == 1 && strlen(&sa[24]) > 0) id3tag.assignf("%s: %s", tags[i].descr, &sa[24]); // UTF-8 Text
                        else if(dty == 0x0D) {m_m4aHdr.picLen = as - 24; m_m4aHdr.picPos = pos + 24 - as; AUDIO_LOG_DEBUG("cover jpeg start: %lu, len %lu", m_m4aHdr.picPos, m_m4aHdr.picLen);}  // jpeg
                        else if(dty == 0x0E) {m_m4aHdr.picLen = as - 24; m_m4aHdr.picPos = pos + 24 - as; AUDIO_LOG_DEBUG("cover png start: %lu, len %lu", m_m4aHdr.picPos, m_m4aHdr.picLen);}   // png
                        else if(dty == 21){
                            if(memcmp(tags[i].tag, "cpil", 4) == 0) {break;} // Compilation Flag
                            if(memcmp(tags[i].tag, "pgap", 4) == 0) {break;} // Gapless Playback
                            if(memcmp(tags[i].tag, "rtng", 4) == 0) {break;} // Evaluation
                        }
                        else break;
                        break;
                    }
                }
            }
            if(id3tag.valid()) {if(audio_id3data) audio_id3data(id3tag.get());}
            else  AUDIO_LOG_DEBUG("%s tag not supported", san);

            if(consumed > len) {
                m_m4aHdr.retvalue = consumed;
                m_m4aHdr.headerSize += consumed;
                return 0;
            }

            if(m_m4aHdr.sizeof_ilst <= m_m4aHdr.ilst_pos){
                m_m4aHdr.retvalue = consumed;
                m_m4aHdr.headerSize += consumed;
                m_controlCounter = M4A_META;
                break;
            }
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_STSZ){
        uint8_t version = data[0]; (void)version;
        uint32_t flags = bigEndian(data + 1, 3); (void)flags;
        uint32_t sample_size = bigEndian(data + 4, 4); (void)sample_size;
        m_m4aHdr.stsz_num_entries = bigEndian(data + 8, 4);
        m_m4aHdr.stsz_table_pos = m_m4aHdr.headerSize + 12;
        m_m4aHdr.retvalue += m_m4aHdr.sizeof_stsz + 8;
        m_m4aHdr.headerSize += m_m4aHdr.sizeof_stsz + 8;
        m_controlCounter = M4A_STBL;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    uint8_t extLen = 0;
    if(m_controlCounter == M4A_MDAT) {            // mdat
        m_audioDataSize = m_m4aHdr.sizeof_mdat; // length of this atom

        // Extended Size
        // 00 00 00 01 6D 64 61 74 00 00 00 00 00 00 16 64
        //        0001  m  d  a  t                    5732

        if(m_audioDataSize == 1){ // Extended Size
            m_audioDataSize = bigEndian(data + 8, 8);
            m_audioDataSize -= 16;
            extLen = 8;
        }
        else m_audioDataSize -= 8;
        AUDIO_INFO("Audio-Length: %i", m_audioDataSize);
        m_m4aHdr.retvalue = extLen;
        m_m4aHdr.headerSize += extLen;
        m_controlCounter = M4A_AMRDY; // last step before starting the audio
        return 0;
    }

    if(m_controlCounter == M4A_AMRDY) { // almost ready
        m_audioDataStart = m_m4aHdr.headerSize;
        if(m_m4aHdr.picLen) {
            size_t pos = m_audioFilePosition;
            audio_id3image(m_audiofile, m_m4aHdr.picPos, m_m4aHdr.picLen);
            audioFileSeek(pos); // the filepointer could have been changed by the user, set it back
        }
        m_stsz_numEntries = m_m4aHdr.stsz_num_entries;
        m_stsz_position = m_m4aHdr.stsz_table_pos;
        m_controlCounter = M4A_OKAY; // that's all
        return 0;
    }
    // this section should never be reached
    AUDIO_LOG_ERROR("error");
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
size_t Audio::process_m3u8_ID3_Header(uint8_t* packet) {
    uint8_t  ID3version;
    size_t   id3Size;
    bool     m_f_unsync = false, m_f_exthdr = false;
    uint64_t current_timestamp = 0;

    (void)m_f_unsync;        // suppress -Wunused-variable
    (void)current_timestamp; // suppress -Wunused-variable

    if(specialIndexOf(packet, "ID3", 4) != 0) { // ID3 not found
        // AUDIO_LOG_INFO("m3u8 file has no mp3 tag");
        return 0; // error, no ID3 signature found
    }
    ID3version = *(packet + 3);
    switch(ID3version) {
        case 2:
            m_f_unsync = (*(packet + 5) & 0x80);
            m_f_exthdr = false;
            break;
        case 3:
        case 4:
            m_f_unsync = (*(packet + 5) & 0x80); // bit7
            m_f_exthdr = (*(packet + 5) & 0x40); // bit6 extended header
            break;
    };
    id3Size = bigEndian(&packet[6], 4, 7); //  ID3v2 size  4 * %0xxxxxxx (shift left seven times!!)
    id3Size += 10;
    // AUDIO_LOG_INFO("ID3 framesSize: %i", id3Size);
    // AUDIO_LOG_INFO("ID3 version: 2.%i", ID3version);

    if(m_f_exthdr) {
        AUDIO_LOG_ERROR("ID3 extended header in m3u8 files not supported");
        return 0;
    }
    // AUDIO_LOG_INFO("ID3 normal frames");

    if(specialIndexOf(&packet[10], "PRIV", 5) != 0) { // tag PRIV not found
        AUDIO_LOG_ERROR("tag PRIV in m3u8 Id3 Header not found");
        return 0;
    }
    // if tag PRIV exists assume content is "com.apple.streaming.transportStreamTimestamp"
    // a time stamp is expected in the header.

    current_timestamp = (double)bigEndian(&packet[69], 4) / 90000; // seconds

    return id3Size;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::stopSong() {
    m_f_lockInBuffer = true; // wait for the decoding to finish
        uint8_t maxWait = 0;
        while(m_f_audioTaskIsDecoding) {vTaskDelay(1); maxWait++; if(maxWait > 100) break;} // in case of error wait max 100ms
        uint32_t pos = 0;
        if(m_f_running) {
            m_f_running = false;
            if(m_dataMode == AUDIO_LOCALFILE) {
                pos = m_audioFilePosition - inBufferFilled();
            }
            if(m_client->connected()) m_client->stop();
        }
        if(m_audiofile) {
            // added this before putting 'm_f_localfile = false' in stopSong(); shoulf never occur....
            AUDIO_INFO("Closing audio file \"%s\"", m_audiofile.name());
            m_audiofile.close();
        }
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // Clear FilterBuffer
        if(m_codec == CODEC_MP3) MP3Decoder_FreeBuffers();
        if(m_codec == CODEC_AAC) AACDecoder_FreeBuffers();
        if(m_codec == CODEC_M4A) AACDecoder_FreeBuffers();
        if(m_codec == CODEC_FLAC) FLACDecoder_FreeBuffers();
        if(m_codec == CODEC_OPUS) OPUSDecoder_FreeBuffers();
        if(m_codec == CODEC_VORBIS) VORBISDecoder_FreeBuffers();
        m_validSamples = 0;
        m_audioCurrentTime = 0;
        m_audioFileDuration = 0;
        m_codec = CODEC_NONE;
        m_dataMode = AUDIO_NONE;
        m_streamType = ST_NONE;
        m_playlistFormat = FORMAT_NONE;
        m_f_lockInBuffer = false;
    return pos;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::pauseResume() {
    xSemaphoreTake(mutex_audioTask, 0.3 * configTICK_RATE_HZ);
    bool retVal = false;
    if(m_dataMode == AUDIO_LOCALFILE || m_streamType == ST_WEBSTREAM || m_streamType == ST_WEBFILE) {
        m_f_running = !m_f_running;
        retVal = true;
        if(!m_f_running) {
            memset(m_outBuff.get(), 0, m_outbuffSize * sizeof(int16_t)); // Clear OutputBuffer
            memset(m_samplesBuff48K.get(), 0, m_samplesBuff48KSize * sizeof(int16_t)); // Clear SamplesBuffer
            m_validSamples = 0;
        }
    }
    xSemaphoreGive(mutex_audioTask);
    return retVal;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
size_t Audio::resampleTo48kStereo(const int16_t* input, size_t inputSamples) {

    float ratio = static_cast<float>(m_sampleRate) / 48000.0f;
    float cursor = m_resampleCursor;

    // Anzahl Input-Samples + 3 History-Samples (vorherige 3 Stereo-Frames)
    size_t extendedSamples = inputSamples + 3;

    // Temporärer Buffer: History + aktueller Input
    std::vector<int16_t> extendedInput(extendedSamples * 2);

    // Historie an den Anfang kopieren (6 Werte = 3 Stereo-Samples)
    memcpy(&extendedInput[0], m_inputHistory, 6 * sizeof(int16_t));

    // Aktuelles Input danach einfügen
    memcpy(&extendedInput[6], input, inputSamples * 2 * sizeof(int16_t));

    size_t outputIndex = 0;

    auto clipToInt16 = [](float value) -> int16_t {
        if (value > 32767.0f){AUDIO_LOG_ERROR("overflow +"); return 32767;}
        if (value < -32768.0f) {AUDIO_LOG_ERROR("overflow -");return -32768;}
        return static_cast<int16_t>(value);
    };

    for (size_t inIdx = 1; inIdx < extendedSamples - 2; ++inIdx) {
        int32_t xm1_l = clipToInt16(extendedInput[(inIdx - 1) * 2]);
        int32_t x0_l  = clipToInt16(extendedInput[(inIdx + 0) * 2]);
        int32_t x1_l  = clipToInt16(extendedInput[(inIdx + 1) * 2]);
        int32_t x2_l  = clipToInt16(extendedInput[(inIdx + 2) * 2]);

        int32_t xm1_r = clipToInt16(extendedInput[(inIdx - 1) * 2 + 1]);
        int32_t x0_r  = clipToInt16(extendedInput[(inIdx + 0) * 2 + 1]);
        int32_t x1_r  = clipToInt16(extendedInput[(inIdx + 1) * 2 + 1]);
        int32_t x2_r  = clipToInt16(extendedInput[(inIdx + 2) * 2 + 1]);

        while (cursor < 1.0f) {
            float t = cursor;

            // Catmull-Rom spline, construct a cubic curve
            auto catmullRom = [](float t, float xm1, float x0, float x1, float x2) {
                return 0.5f * (
                    (2.0f * x0) +
                    (-xm1 + x1) * t +
                    (2.0f * xm1 - 5.0f * x0 + 4.0f * x1 - x2) * t * t +
                    (-xm1 + 3.0f * x0 - 3.0f * x1 + x2) * t * t * t
                );
            };

            int16_t outLeft = static_cast<int16_t>(
                catmullRom(t, xm1_l, x0_l, x1_l, x2_l)
            );
            int16_t outRight = static_cast<int16_t>(
                catmullRom(t, xm1_r, x0_r, x1_r, x2_r)
            );

            m_samplesBuff48K[outputIndex * 2]     = clipToInt16(outLeft);
            m_samplesBuff48K[outputIndex * 2 + 1] = clipToInt16(outRight);

            ++outputIndex;
            cursor += ratio;
        }

        cursor -= 1.0f;
    }

    // Letzte 3 Stereo-Samples als neue Historie sichern
    for (int i = 0; i < 3; ++i) {
        size_t idx = inputSamples - 3 + i;
        m_inputHistory[i * 2]     = input[idx * 2];
        m_inputHistory[i * 2 + 1] = input[idx * 2 + 1];
    }
    m_resampleCursor = cursor;
    return outputIndex;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR Audio::playChunk() {
    if(m_validSamples == 0) return; // nothing to do
    int16_t* outBuff_ptr = nullptr;

    m_plCh.validSamples = 0;
    m_plCh.i2s_bytesConsumed = 0;
    m_plCh.sample[0] = 0;
    m_plCh.sample[1] = 0;
    m_plCh.s2 = 0;
    m_plCh.sampleSize = 4; // 2 bytes per sample (int16_t) * 2 channels
    m_plCh.err = ESP_OK;
    m_plCh.i = 0;

    if(m_plCh.count > 0) goto i2swrite;

    if(getChannels() == 1){
        for (int i = m_validSamples - 1; i >= 0; --i) {
            int16_t sample = m_outBuff[i];
            m_outBuff[2 * i] = sample;
            m_outBuff[2 * i + 1] = sample;
        }
    //    m_validSamples *= 2;
    }

    m_plCh.validSamples = m_validSamples;

    while(m_plCh.validSamples) {
        *m_plCh.sample = m_outBuff.get() + m_plCh.i;
        computeVUlevel(*m_plCh.sample);

        //---------- Filterchain, can commented out if not used-------------
        {
            if(m_corr > 1) {
                m_plCh.s2 = *m_plCh.sample;
                m_plCh.s2[LEFTCHANNEL] /= m_corr;
                m_plCh.s2[RIGHTCHANNEL] /= m_corr;
            }
            IIR_filterChain0(*m_plCh.sample);
            IIR_filterChain1(*m_plCh.sample);
            IIR_filterChain2(*m_plCh.sample);
        }
        //------------------------------------------------------------------
        if(m_f_forceMono && m_channels == 2){
            int32_t xy = ((*m_plCh.sample)[RIGHTCHANNEL] + (*m_plCh.sample)[LEFTCHANNEL]) / 2;
            (*m_plCh.sample)[RIGHTCHANNEL] = (int16_t)xy;
            (*m_plCh.sample)[LEFTCHANNEL]  = (int16_t)xy;
        }
        Gain(*m_plCh.sample);
        m_plCh.i += 2;
        m_plCh.validSamples -= 1;
    }
    //------------------------------------------------------------------------------------------
#ifdef SR_48K
    m_plCh.samples48K = resampleTo48kStereo(m_outBuff.get(), m_validSamples);
    m_validSamples = m_plCh.samples48K;
    outBuff_ptr = m_samplesBuff48K.get();

    if(m_i2s_std_cfg.clk_cfg.sample_rate_hz != 48000){
        m_i2s_std_cfg.clk_cfg.sample_rate_hz = 48000;
        i2s_channel_disable(m_i2s_tx_handle);
        i2s_channel_reconfig_std_clock(m_i2s_tx_handle, &m_i2s_std_cfg.clk_cfg);
        i2s_channel_enable(m_i2s_tx_handle);
    };
#else
    outBuff_ptr = m_outBuff.get();
#endif

    if(audio_process_i2s) {
        // processing the audio samples from external before forwarding them to i2s
        bool continueI2S = false;
        audio_process_i2s((int16_t*)outBuff_ptr, m_validSamples, &continueI2S); // 48KHz stereo 16bps
        if(!continueI2S) {
            m_validSamples = 0;
            m_plCh.count = 0;
            return;
        }
    }

i2swrite:

    m_plCh.err = i2s_channel_write(m_i2s_tx_handle, outBuff_ptr + m_plCh.count, m_validSamples * m_plCh.sampleSize, &m_plCh.i2s_bytesConsumed, 50);
    if( ! (m_plCh.err == ESP_OK || m_plCh.err == ESP_ERR_TIMEOUT)) goto exit;
    m_validSamples -= m_plCh.i2s_bytesConsumed / m_plCh.sampleSize;
    m_plCh.count += m_plCh.i2s_bytesConsumed / 2;
    if(m_validSamples <= 0) { m_validSamples = 0; m_plCh.count = 0; }

// ---- statistics, bytes written to I2S (every 10s)
    // static int cnt = 0;
    // static uint32_t t = millis();

    // if(t + 10000 < millis()){
    //     AUDIO_LOG_INFO("%i", cnt);
    //     cnt = 0;
    //     t = millis();
    // }
    // cnt+= i2s_bytesConsumed;
//-------------------------------------------


    return;
exit:
    if     (m_plCh.err == ESP_OK) return;
    else if(m_plCh.err == ESP_ERR_INVALID_ARG)   AUDIO_LOG_ERROR("NULL pointer or this handle is not tx handle");
    else if(m_plCh.err == ESP_ERR_TIMEOUT)       AUDIO_LOG_ERROR("Writing timeout, no writing event received from ISR within ticks_to_wait");
    else if(m_plCh.err == ESP_ERR_INVALID_STATE) AUDIO_LOG_ERROR("I2S is not ready to write");
    else AUDIO_LOG_ERROR("i2s err %i", m_plCh.err);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::loop() {
    if(!m_f_running) return;

    if(m_playlistFormat != FORMAT_M3U8) { // normal process
        switch(m_dataMode) {
            case AUDIO_LOCALFILE:
                processLocalFile(); break;
            case HTTP_RESPONSE_HEADER:
                if(!parseHttpResponseHeader()) {
                    if(m_f_timeout && m_lVar.count < 3) {m_f_timeout = false; m_lVar.count++; connecttohost(m_lastHost.get());}
                }
                else{
                    m_lVar.count = 0;
                }
                break;
            case AUDIO_PLAYLISTINIT: readPlayListData(); break;
            case AUDIO_PLAYLISTDATA:
                if(m_playlistFormat == FORMAT_M3U) httpPrint(parsePlaylist_M3U());
                if(m_playlistFormat == FORMAT_PLS) httpPrint(parsePlaylist_PLS());
                if(m_playlistFormat == FORMAT_ASX) httpPrint(parsePlaylist_ASX());
                break;
            case AUDIO_DATA:
                if(m_streamType == ST_WEBSTREAM) processWebStream();
                if(m_streamType == ST_WEBFILE)   processWebFile();
                break;
        }
    }
    else { // m3u8 datastream only
        ps_ptr<char> host;
        if(m_lVar.no_host_timer > millis()) {return;}
        switch(m_dataMode) {
            case HTTP_RESPONSE_HEADER:
                if(!parseHttpResponseHeader()) {
                    if(m_f_timeout && m_lVar.count < 3) {m_f_timeout = false; m_lVar.count++; m_f_reset_m3u8Codec = false; connecttohost(m_lastHost.get());}
                }
                else{
                    m_lVar.count = 0;
                    m_f_firstCall  = true;
                }
                break;
            case AUDIO_PLAYLISTINIT: readPlayListData(); break;
            case AUDIO_PLAYLISTDATA:
                host = parsePlaylist_M3U8();
                if(!host.valid()) m_lVar.no_host_cnt++;
                else {m_lVar.no_host_cnt = 0; m_lVar.no_host_timer = millis();}

                if(m_lVar.no_host_cnt == 2){m_lVar.no_host_timer = millis() + 2000;} // no new url? wait 2 seconds
                if(host.valid()) { // host contains the next playlist URL
                    httpPrint(host.get());
                    m_dataMode = HTTP_RESPONSE_HEADER;
                }
                else { // host == NULL means connect to m3u8 URL
                    if(m_lastM3U8host.valid()) {m_f_reset_m3u8Codec = false; httpPrint(m_lastM3U8host.get());}
                    else                       {httpPrint(m_lastHost.get());}      // if url has no first redirection
                    m_dataMode = HTTP_RESPONSE_HEADER;               // we have a new playlist now
                }
                break;
            case AUDIO_DATA:
                if(m_f_ts) { processWebStreamTS(); } // aac or aacp with ts packets
                else { processWebStreamHLS(); }      // aac or aacp normal stream

                if(m_f_continue) { // at this point m_f_continue is true, means processWebStream() needs more data
                    m_dataMode = AUDIO_PLAYLISTDATA;
                    m_f_continue = false;
                }
                break;
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::readPlayListData() {
    if(m_dataMode != AUDIO_PLAYLISTINIT) return false;
    if(m_client->available() == 0) return false;

    uint32_t chunksize = 0;
    uint8_t  readedBytes = 0;
    if(m_f_chunked) chunksize = readChunkSize(&readedBytes);

    ps_ptr<char>pl;
    uint32_t ctl = 0;
    int      lines = 0;
    // delete all memory in m_playlistContent
    if(m_playlistFormat == FORMAT_M3U8 && !psramFound()) { AUDIO_LOG_ERROR("m3u8 playlists requires PSRAM enabled!"); }
    vector_clear_and_shrink(m_playlistContent);
    while(true) { // outer while

        uint32_t ctime = millis();
        uint32_t timeout = 2000; // ms
        pl.alloc(1024,"pl"); pl.clear(); // playlistLine

        while(true) { // inner while
            uint16_t pos = 0;
            while(m_client->available()) { // super inner while :-))
                pl[pos] = audioFileRead();
                ctl++;
                if(pl[pos] == '\n') {
                    pl[pos] = '\0';
                    pos++;
                    break;
                }
                //    if(pl[pos] == '&' ) {pl[pos] = '\0'; pos++; break;}
                if(pl[pos] == '\r') {
                    pl[pos] = '\0';
                    pos++;
                    continue;
                    ;
                }
                pos++;
                if(pos == 1022) {
                    pos--;
                    continue;
                }
                if(pos == 509) { pl[pos] = '\0'; }
                if(ctl == chunksize) {
                    pl[pos] = '\0';
                    break;
                }
                if(ctl == m_audioFileSize) {
                    pl[pos] = '\0';
                    break;
                }
            }
            if(ctl == chunksize) break;
            if(ctl == m_audioFileSize) break;
            if(pos) {
                pl[pos] = '\0';
                break;
            }

            if(ctime + timeout < millis()) {
                AUDIO_LOG_ERROR("timeout");
                for(int i = 0; i < m_playlistContent.size(); i++) AUDIO_LOG_ERROR("pl%i = %s", i, m_playlistContent[i].get());
                goto exit;
            }
        } // inner while

        if(pl.starts_with_icase("<!DOCTYPE")) {
            AUDIO_INFO("url is a webpage!");
            goto exit;
        }
        if(pl.starts_with_icase("<html")) {
            AUDIO_INFO("url is a webpage!");
            goto exit;
        }
        if(pl.size() > 0) m_playlistContent.emplace_back(std::move(pl));

        if(m_playlistContent.size() && m_playlistContent.size() % 1000 == 0) { AUDIO_INFO("current playlist line: %lu", (long unsigned)m_playlistContent.size()); }
        // termination conditions
        // 1. The http response header returns a value for contentLength -> read chars until contentLength is reached
        // 2. no contentLength, but Transfer-Encoding:chunked -> compute chunksize and read until chunksize is reached
        // 3. no chunksize and no contentlengt, but Connection: close -> read all available chars
        if(ctl == m_audioFileSize) {
            while(m_client->available()) audioFileRead();
            break;
        } // read '\n\n' if exists
        if(ctl == chunksize) {
            while(m_client->available()) audioFileRead();
            break;
        }
        if(!m_client->connected() && m_client->available() == 0) break;

    } // outer while
    lines = m_playlistContent.size();
    for(int i = 0; i < lines; i++) { // print all string in first vector of 'arr'
    //    AUDIO_LOG_INFO("pl=%i \"%s\"", i, m_playlistContent[i]);
    }
    m_dataMode = AUDIO_PLAYLISTDATA;
    return true;

exit:
    vector_clear_and_shrink(m_playlistContent);
    m_f_running = false;
    m_dataMode = AUDIO_NONE;
    return false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_M3U() {
    uint8_t lines = m_playlistContent.size();
    int     pos = 0;
    char*   host = nullptr;

    for(int i = 0; i < lines; i++) {
        if(m_playlistContent[i].contains("#EXTINF:")) { // Info?
            pos = m_playlistContent[i].index_of(",");        // Comma in this line?
            if(pos > 0) {
                // Show artist and title if present in metadata
                AUDIO_INFO(m_playlistContent[i].get() + pos + 1);
            }
            continue;
        }
        if(m_playlistContent[i].starts_with("#")) { // Commentline?
            continue;
        }

        pos = m_playlistContent[i].index_of("http://:@", 0); // ":@"??  remove that!
        if(pos >= 0) {
            AUDIO_INFO("Entry in playlist found: %s", (m_playlistContent[i].get() + pos + 9));
            host = m_playlistContent[i].get() + pos + 9;
            break;
        }
        // AUDIO_INFO("Entry in playlist found: %s", pl);
        pos = m_playlistContent[i].index_of("http", 0); // Search for "http"
        if(pos >= 0) {                                  // Does URL contain "http://"?
                                                        //    AUDIO_LOG_ERROR(""%s pos=%i", m_playlistContent[i], pos);
            host = m_playlistContent[i].get() + pos;          // Yes, set new host
            break;
        }
    }
    //    vector_clear_and_shrink(m_playlistContent);
    return host;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_PLS() {
    uint8_t lines = m_playlistContent.size();
    int     pos = 0;
    char*   host = nullptr;

    for(int i = 0; i < lines; i++) {
        if(i == 0) {
            if(m_playlistContent[0].strlen() == 0) goto exit;     // empty line
            if(!m_playlistContent[0].starts_with("[playlist]")) { // first entry in valid pls
                m_dataMode = HTTP_RESPONSE_HEADER;                // pls is not valid
                AUDIO_INFO("pls is not valid, switch to HTTP_RESPONSE_HEADER");
                goto exit;
            }
            continue;
        }
        if(m_playlistContent[i].starts_with("File1")) {
            if(host) continue;                              // we have already a url
            pos = m_playlistContent[i].index_of("http", 0); // File1=http://streamplus30.leonex.de:14840/;
            if(pos >= 0) {                                  // yes, URL contains "http"?
                host = m_playlistContent[i].get() + pos;          // Now we have an URL for a stream in host.
            }
            continue;
        }
        if(m_playlistContent[i].starts_with("Title1")) { // Title1=Antenne Tirol
            const char* plsStationName = (m_playlistContent[i].get() + 7);
            if(audio_showstation) audio_showstation(plsStationName);
            AUDIO_INFO("StationName: \"%s\"", plsStationName);
            continue;
        }
        if(m_playlistContent[i].starts_with("Length1")) { continue; }
        if(m_playlistContent[i].contains("Invalid username")) { // Unable to access account:
            goto exit;                                          // Invalid username or password
        }
    }
    return host;

exit:
    m_f_running = false;
    stopSong();
    vector_clear_and_shrink(m_playlistContent);
    m_dataMode = AUDIO_NONE;
    return nullptr;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_ASX() { // Advanced Stream Redirector
    uint8_t lines = m_playlistContent.size();
    bool    f_entry = false;
    int     pos = 0;
    char*   host = nullptr;

    for(int i = 0; i < lines; i++) {
        int p1 = m_playlistContent[i].index_of("<", 0);
        int p2 = m_playlistContent[i].index_of(">", 1);
        if(p1 >= 0 && p2 > p1) { // #196 set all between "< ...> to lowercase
            for(uint8_t j = p1; j < p2; j++) { m_playlistContent[i][j] = toLowerCase(m_playlistContent[i][j]); }
        }
        if(m_playlistContent[i].contains("<entry>")) f_entry = true; // found entry tag (returns -1 if not found)
        if(f_entry) {
            if(m_playlistContent[i].contains("ref href")) { //  <ref href="http://87.98.217.63:24112/stream" />
                pos = m_playlistContent[i].index_of("http", 0);
                if(pos > 0) {
                    host = (m_playlistContent[i].get() + pos); // http://87.98.217.63:24112/stream" />
                    int pos1 = indexOf(host, "\"", 0);   // http://87.98.217.63:24112/stream
                    if(pos1 > 0) host[pos1] = '\0';      // Now we have an URL for a stream in host.
                }
            }
        }
        pos = m_playlistContent[i].index_of("<title>", 0);
        if(pos >= 0) {
            char* plsStationName = (m_playlistContent[i].get() + pos + 7); // remove <Title>
            pos = indexOf(plsStationName, "</", 0);
            if(pos >= 0) {
                *(plsStationName + pos) = 0; // remove </Title>
            }
            if(audio_showstation) audio_showstation(plsStationName);
            AUDIO_INFO("StationName: \"%s\"", plsStationName);
        }

        if(m_playlistContent[i].starts_with("http") && !f_entry) { // url only in asx
            host = m_playlistContent[i].get();
        }
    }
    return host;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ps_ptr<char> Audio::parsePlaylist_M3U8() {
    // example: audio chunks
    // #EXTM3U
    // #EXT-X-TARGETDURATION:10
    // #EXT-X-MEDIA-SEQUENCE:163374040
    // #EXT-X-DISCONTINUITY
    // #EXTINF:10,title="text=\"Spot Block End\" amgTrackId=\"9876543\"",artist=" ",url="length=\"00:00:00\""
    // http://n3fa-e2.revma.ihrhls.com/zc7729/63_sdtszizjcjbz02/main/163374038.aac
    // #EXTINF:10,title="text=\"Spot Block End\" amgTrackId=\"9876543\"",artist=" ",url="length=\"00:00:00\""
    // http://n3fa-e2.revma.ihrhls.com/zc7729/63_sdtszizjcjbz02/main/163374039.aac

    if(!m_lastHost.valid())      {AUDIO_LOG_ERROR("m_lastHost is NULL");     return {};} // guard

    uint8_t lines = m_playlistContent.size();
    bool    f_haveRedirection = false;
    char    llasc[21]; // uint64_t max = 18,446,744,073,709,551,615  thats 20 chars + \0

    if(lines){
        bool addNextLine = false;
        vector_clear_and_shrink(m_linesWithSeqNrAndURL);
        vector_clear_and_shrink(m_linesWithEXTINF);
        for(uint8_t i = 0; i < lines; i++) {
            //  AUDIO_LOG_INFO("pl%i = %s", i, m_playlistContent[i].get());
            if(m_playlistContent[i].starts_with("#EXT-X-STREAM-INF:")) { f_haveRedirection = true; /*AUDIO_LOG_ERROR("we have a redirection");*/}
            if(addNextLine) {
                addNextLine = false;
                // size_t len = strlen(linesWithSeqNr[idx].get()) + strlen(m_playlistContent[i].get()) + 1;
                m_linesWithSeqNrAndURL.emplace_back().clone_from(m_playlistContent[i]);
            }
            if(startsWith(m_playlistContent[i].get(), "#EXTINF:")) {
                m_linesWithEXTINF.emplace_back().clone_from(m_playlistContent[i]);
                addNextLine = true;
            }
        }
    }

    for(int i = 0; i < m_linesWithSeqNrAndURL.size(); i++) {/*AUDIO_LOG_INFO("%s", m_linesWithSeqNrAndURL[i].get())*/;}
    for(int i = 0; i < m_linesWithEXTINF.size(); i++)      {/*AUDIO_LOG_INFO("%s", m_linesWithEXTINF[i].get());*/ showstreamtitle(m_linesWithEXTINF[i].get());}

    if(f_haveRedirection) {
        ps_ptr<char> ret = m3u8redirection(&m_m3u8Codec);
        m_lastM3U8host.assign(ret.get());
        vector_clear_and_shrink(m_playlistContent);
        return {};
    }

    if(m_f_firstM3U8call) {
        m_f_firstM3U8call = false;
        m_pplM3U8.xMedSeq = 0;
        m_pplM3U8.f_mediaSeq_found = false;
    }

    if(!m_pplM3U8.f_mediaSeq_found) {m_pplM3U8.f_mediaSeq_found = m3u8_findMediaSeqInURL(m_linesWithSeqNrAndURL, &m_pplM3U8.xMedSeq); AUDIO_INFO("MediaSequenceNumber: %lli", m_pplM3U8.xMedSeq);}
    if(m_codec == CODEC_NONE) {m_codec = CODEC_AAC; if(m_m3u8Codec == CODEC_MP3) m_codec = CODEC_MP3;}  // if we have no redirection
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

    boolean f_EXTINF_found = false;

    if(lines) {
        for(uint16_t i = 0; i < m_linesWithSeqNrAndURL.size(); i++) {

            f_EXTINF_found = true;

            ps_ptr<char> tmp;
            if(!m_linesWithSeqNrAndURL[i].starts_with("http")) {

                //  playlist:   http://station.com/aaa/bbb/xxx.m3u8
                //  chunklist:  http://station.com/aaa/bbb/ddd.aac
                //  result:     http://station.com/aaa/bbb/ddd.aac

                if(m_lastM3U8host.valid()) {
                    tmp.clone_from(m_lastM3U8host, "tmp");
                }
                else {
                    tmp.clone_from(m_lastHost, "tmp");
                }
                if(m_linesWithSeqNrAndURL[i][0] != '/'){

                    //  playlist:   http://station.com/aaa/bbb/xxx.m3u8  // tmp
                    //  chunklist:  ddd.aac                              // m_linesWithSeqNrAndURL[i]
                    //  result:     http://station.com/aaa/bbb/ddd.aac   // m_linesWithSeqNrAndURL[i]

                    int idx = tmp.last_index_of('/');
                    tmp[idx  + 1] = '\0';
                    tmp.append(m_linesWithSeqNrAndURL[i].get());
                }
                else{

                    //  playlist:   http://station.com/aaa/bbb/xxx.m3u8
                    //  chunklist:  /aaa/bbb/ddd.aac
                    //  result:     http://station.com/aaa/bbb/ddd.aac
                    int idx = tmp.index_of('/', 8);
                    tmp[idx] = '\0';
                    tmp.append(m_linesWithSeqNrAndURL[i].get());
                }
            }
            else {tmp.append(m_linesWithSeqNrAndURL[i].get());}

            if(m_pplM3U8.f_mediaSeq_found) {
                lltoa(m_pplM3U8.xMedSeq, llasc, 10);
                if(tmp.contains(llasc)) {
                    m_playlistURL.insert(m_playlistURL.begin(), std:: move(tmp));
                    m_pplM3U8.xMedSeq++;
                }
                else{
                    lltoa(m_pplM3U8.xMedSeq + 1, llasc, 10);
                    if(tmp.contains(llasc)) {
                        m_playlistURL.insert(m_playlistURL.begin(), std::move(tmp));
                        AUDIO_LOG_WARN("mediaseq %llu skipped", m_pplM3U8.xMedSeq);
                        m_pplM3U8.xMedSeq+= 2;
                    }
                }
            }
            else { // without mediaSeqNr, with hash
                uint32_t hash = simpleHash(tmp.get());
                if(m_hashQueue.size() == 0) {
                    m_hashQueue.insert(m_hashQueue.begin(), hash);
                    m_playlistURL.insert(m_playlistURL.begin(), std::move(tmp));
                }
                else {
                    bool known = false;
                    for(int i = 0; i < m_hashQueue.size(); i++) {
                        if(hash == m_hashQueue[i]) {
                            // AUDIO_LOG_INFO("file already known %s", tmp);
                            known = true;
                        }
                    }
                    if(!known) {
                        m_hashQueue.insert(m_hashQueue.begin(), hash);
                        m_playlistURL.insert(m_playlistURL.begin(), std::move(tmp));
                    }
                }
                if(m_hashQueue.size() > 20) m_hashQueue.pop_back();
            }
            continue;
        }
        vector_clear_and_shrink(m_playlistContent); // clear after reading everything, m_playlistContent.size is now 0
    }
    if(m_playlistURL.size() > 0) {
        ps_ptr<char>m_playlistBuff;
        if(m_playlistURL[m_playlistURL.size() - 1].valid()) {
            m_playlistBuff.append(m_playlistURL[m_playlistURL.size() - 1].get());
            m_playlistURL.pop_back();
            m_playlistURL.shrink_to_fit();
        }
        // AUDIO_LOG_INFO("now playing %s", m_playlistBuff);
        if(endsWith(m_playlistBuff.get(), "ts")) m_f_ts = true;
        if(m_playlistBuff.contains(".ts?") > 0)  m_f_ts = true;
        return m_playlistBuff;
    }
    else {
        if(f_EXTINF_found) {
            if(m_pplM3U8.f_mediaSeq_found) {
                if(m_playlistContent.size() == 0) return {};

                uint64_t mediaSeq;
                m_pplM3U8.f_mediaSeq_found = m3u8_findMediaSeqInURL(m_linesWithSeqNrAndURL, &mediaSeq);
                if(!m_pplM3U8.f_mediaSeq_found) {
                    AUDIO_LOG_ERROR("xMediaSequence not found");
                    httpPrint(m_lastHost.get());
                }
                if(mediaSeq < m_pplM3U8.xMedSeq) {
                    uint64_t diff = m_pplM3U8.xMedSeq - mediaSeq;
                    if(diff < 10) { ; }
                    else {
                        if(m_playlistContent.size() > 0) {
                            for(int j = 0; j < lines; j++) {
                                // AUDIO_LOG_INFO("lines %i, %s", lines, m_playlistContent[j]);
                            }
                        }
                        else { ; }

                        if(m_playlistURL.size() > 0) {
                            for(int j = 0; j < m_playlistURL.size(); j++) {
                                // AUDIO_LOG_INFO("m_playlistURL lines %i, %s", j, m_playlistURL[j]);
                            }
                        }
                        else { ; }

                        if(m_playlistURL.size() == 0) {
                            m_f_reset_m3u8Codec = false;
                            httpPrint(m_lastHost.get());
                        }
                    }
                }
                else {
                    if(mediaSeq != UINT64_MAX) { AUDIO_LOG_ERROR("err, %u packets lost from %u, to %u", mediaSeq - m_pplM3U8.xMedSeq, m_pplM3U8.xMedSeq, mediaSeq); }
                    m_pplM3U8.xMedSeq = mediaSeq;
                }
            } // f_medSeq_found
        }
    }
    return {};
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ps_ptr<char>Audio::m3u8redirection(uint8_t* codec) {
    // example: redirection
    // #EXTM3U
    // #EXT-X-STREAM-INF:BANDWIDTH=117500,AVERAGE-BANDWIDTH=117000,CODECS="mp4a.40.2"
    // 112/playlist.m3u8?hlssid=7562d0e101b84aeea0fa35f8b963a174
    // #EXT-X-STREAM-INF:BANDWIDTH=69500,AVERAGE-BANDWIDTH=69000,CODECS="mp4a.40.5"
    // 64/playlist.m3u8?hlssid=7562d0e101b84aeea0fa35f8b963a174
    // #EXT-X-STREAM-INF:BANDWIDTH=37500,AVERAGE-BANDWIDTH=37000,CODECS="mp4a.40.29"
    // 32/playlist.m3u8?hlssid=7562d0e101b84aeea0fa35f8b963a174

    if(!m_lastHost.valid()) {AUDIO_LOG_ERROR("m_lastHost is empty"); return {};} // guard
    const char codecString[9][11]={
        "mp4a.40.34", // mp3 stream
        "mp4a.40.01", // AAC Main
        "mp4a.40.2",  // MPEG-4 AAC LC
        "mp4a.40.02", // MPEG-4 AAC LC, leading 0 for Aud-OTI compatibility
        "mp4a.40.29", // MPEG-4 HE-AAC v2 (AAC LC + SBR + PS)
        "mp4a.40.42", // xHE-AAC
        "mp4a.40.5",  // MPEG-4 HE-AAC v1 (AAC LC + SBR)
        "mp4a.40.05", // MPEG-4 HE-AAC v1 (AAC LC + SBR), leading 0 for Aud-OTI compatibility
        "mp4a.67",    // MPEG-2 AAC LC
    };

    uint16_t choosenLine = 0;
    uint16_t plcSize = m_playlistContent.size();
    int8_t   cS = 100;

    for(uint16_t i = 0; i < plcSize; i++) { // looking for lowest codeString
        if(m_playlistContent[i].contains("CODECS=\"mp4a")){
            for (uint8_t j = 0; j < 9; j++) {
                if (m_playlistContent[i].contains(codecString[j])) {
                    if (j < cS) {
                        cS = j;
                        choosenLine = i;
                    }
                }
            }
        }
    }
    if (cS == 0)            *codec = CODEC_MP3;
    else if (cS < 100)      *codec = CODEC_AAC;



    if(cS == 100) {                             // "mp4a.xx.xx" not found
        *codec = CODEC_AAC;                     // assume AAC
        for(uint16_t i = 0; i < plcSize; i++) { // we have no codeString, looking for "http"
            if(m_playlistContent[i].contains("#EXT-X-STREAM-INF")){
                choosenLine = i;
            }
        }
    }

    choosenLine++; // next line is the redirection url

    ps_ptr<char> result = {};
    ps_ptr<char>line; line.clone_from(m_playlistContent[choosenLine]);

    if(line.starts_with("../")){
        // ../../2093120-b/RISMI/stream01/streamPlaylist.m3u8
        while (line.starts_with("../")){
            line.remove_prefix("../");
        }
        result.clone_from(m_lastHost);
        int idx = result.last_index_of('/');
        if(idx > 0 && (size_t)idx != result.strlen() - 1) result.truncate_at((size_t)idx + 1);
        result.append(line.get());
    }
    else if(!line.starts_with_icase("http")) {

        // http://arcast.com.ar:1935/radio/radionar.stream/playlist.m3u8               m_lastHost
        // chunklist_w789468022.m3u8                                                   line
        // http://arcast.com.ar:1935/radio/radionar.stream/chunklist_w789468022.m3u8   --> result

        result.clone_from(m_lastHost);
        int pos = m_lastHost.last_index_of('/');
        if(pos > 0){
            result.truncate_at((size_t)pos + 1);
            result.append(line.get());
        }
    }
    else {
        result.clone_from(line);
    }

    return result; // it's a redirection, a new m3u8 playlist
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::m3u8_findMediaSeqInURL(std::vector<ps_ptr<char>>&linesWithSeqNr, uint64_t* mediaSeqNr) { // We have no clue what the media sequence is

    // example: linesWithSeqNr
    // p:,<p:,<10.032,media_w630738364_405061.mp3
    // p:,<p:,<9.952,media_w630738364_405062.mp3
    // p:,<p:,<10.031,media_w630738364_405063.mp3

    if(linesWithSeqNr.size() < 3) return false;
    //     for (uint16_t i = 0; i < linesWithSeqNr.size(); i++) {
    //         AUDIO_LOG_INFO("linesWithSeqNr[%i] = %s", i, linesWithSeqNr[i].get());
    //     }

    uint16_t pos;
    std::array<std::vector<int64_t>, 3>numbers;
    for(int i = 0; i < 3; i++){
        pos = 0;
        for(pos = 0; pos < linesWithSeqNr[i].strlen(); pos++){
            if(isdigit(linesWithSeqNr[i][pos])){
                numbers[i].push_back(strtoull(linesWithSeqNr[i].get() + pos, nullptr, 10));
                // AUDIO_LOG_INFO("%lli", strtoull(linesWithSeqNr[i].get() + pos, nullptr, 10));
                while(pos < linesWithSeqNr[i].strlen()){ // skip to next number or line end
                    pos++;
                    if(!isdigit(linesWithSeqNr[i][pos])) break;
                }
            }
        }
    }

    // There must be three valid lines with a sequencer.Then the integers are sought for each line.For the example, these are:
    // numbers[0] 10 32 630738364 405061
    // numbers[1] 9 952 630738364 405062
    // numbers[2] 10 31 630738364 405063
    // then the absolute value is formed (-234 is also possible if there is a hyphen) and it is searched for three consecutive numbers

    uint16_t idx = 0;
    while(idx < numbers[0].size()){
        // AUDIO_LOG_INFO("%lli, %lli, %lli", abs(numbers[0][idx]), abs(numbers[1][idx]), abs(numbers[2][idx]));
        if(abs(numbers[0][idx]) + 1 == abs(numbers[1][idx]) && abs(numbers[1][idx]) + 1 == abs(numbers[2][idx])){
            *mediaSeqNr = abs(numbers[0][idx]);
            return true;
        }
        idx++;
    }
    return false;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::processLocalFile() {
    if(!(m_audiofile && m_f_running && m_dataMode == AUDIO_LOCALFILE)) return; // guard
    m_prlf.maxFrameSize = InBuff.getMaxBlockSize(); // every mp3/aac frame is not bigger maxFrameSize = InBuff.getMaxBlockSize(); // every mp3/aac frame is not bigger
    m_prlf.availableBytes = 0;
    m_prlf.bytesAddedToBuffer = 0;

    if(m_f_firstCall) { // runs only one time per connection, prepare for start
        m_f_firstCall = false;
        m_f_stream = false;
        m_prlf.audioHeaderFound = false;
        m_prlf.newFilePos = 0;
        m_prlf.ctime = millis();
        m_audioFilePosition = 0;
        m_audioDataSize = m_audioFileSize;
        m_audioDataStart = 0;
        m_f_allDataReceived = false;
        m_prlf.timeout = 8000; // ms
    }

    if(m_resumeFilePos >= 0 ) {  // we have a resume file position
        m_prlf.newFilePos = newInBuffStart(m_resumeFilePos);
        if(m_prlf.newFilePos < 0) AUDIO_LOG_WARN("skip to new position was not successful");
        m_haveNewFilePos  = m_prlf.newFilePos;
        m_resumeFilePos = -1;
        m_f_allDataReceived = false;
        return;
    }

    m_prlf.availableBytes = InBuff.writeSpace();
    m_prlf.bytesAddedToBuffer = audioFileRead(InBuff.getWritePtr(), m_prlf.availableBytes);
    if(m_prlf.bytesAddedToBuffer > 0) {InBuff.bytesWritten(m_prlf.bytesAddedToBuffer);}
    if(m_audioDataSize && m_audioFilePosition >= m_audioDataSize){if(!m_f_allDataReceived) m_f_allDataReceived = true;}
    if(!m_audioDataSize && m_audioFilePosition == m_audioFileSize){if(!m_f_allDataReceived) m_f_allDataReceived = true;}
    // AUDIO_LOG_ERROR("m_audioFilePosition %u >= m_audioDataSize %u, m_f_allDataReceived % i", m_audioFilePosition, m_audioDataSize, m_f_allDataReceived);

    if(!m_f_stream) {
        if(m_codec == CODEC_OGG) { // AUDIO_LOG_ERROR("determine correct codec here");
            uint8_t codec = determineOggCodec(InBuff.getReadPtr(), m_prlf.maxFrameSize);
            if     (codec == CODEC_FLAC)   {initializeDecoder(codec); m_codec = CODEC_FLAC;   return;}
            else if(codec == CODEC_OPUS)   {initializeDecoder(codec); m_codec = CODEC_OPUS;   return;}
            else if(codec == CODEC_VORBIS) {initializeDecoder(codec); m_codec = CODEC_VORBIS; return;}
            else                           {stopSong(); return;}
        }
        if(m_controlCounter != 100) {
            if((millis() - m_prlf.ctime) > m_prlf.timeout) {
                AUDIO_LOG_ERROR("audioHeader reading timeout");
                m_f_running = false;
                goto exit;
            }
            if(InBuff.bufferFilled() > m_prlf.maxFrameSize || (InBuff.bufferFilled() == m_audioFileSize) || m_f_allDataReceived) { // at least one complete frame or the file is smaller
                InBuff.bytesWasRead(readAudioHeader(InBuff.getMaxAvailableBytes()));
            }
            if(m_controlCounter == 100){
                if(m_audioDataStart > 0){ m_prlf.audioHeaderFound = true; }
                if(!m_audioDataSize) m_audioDataSize = m_audioFileSize;
            }
            return;
        }
        else {
            m_f_stream = true;
            AUDIO_INFO("stream ready");
        }
    }

    if(m_fileStartPos > 0){
        setFilePos(m_fileStartPos);
        m_fileStartPos = -1;
    }

    // end of file reached? - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_eof){ // m_f_eof and m_f_ID3v1TagFound will be set in playAudioData()
        if(m_f_ID3v1TagFound) readID3V1Tag();
exit:
        ps_ptr<char>afn; // audio file name
        if(m_audiofile) afn.assign(m_audiofile.name()); // store temporary the name
        stopSong();
        m_audioCurrentTime = 0;
        m_audioFileDuration = 0;
        m_resumeFilePos = -1;
        m_haveNewFilePos = 0;
        m_codec = CODEC_NONE;

        if(afn.valid()) {
            if(audio_eof_mp3) audio_eof_mp3(afn.c_get());
            AUDIO_INFO("End of file \"%s\"", afn.c_get());
        }
        return;
    }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::processWebStream() {
    if(m_dataMode != AUDIO_DATA) return; // guard

    m_pwst.maxFrameSize = InBuff.getMaxBlockSize(); // every mp3/aac frame is not bigger
    m_pwst.availableBytes = 0; // available from stream
    m_pwst.f_clientIsConnected = m_client->connected();

    // first call, set some values to default  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        m_f_stream = false;
        m_pwst.chunkSize = 0;
        m_metacount = m_metaint;
        m_pwst.f_skipCRLF = false;
        m_f_allDataReceived = false;
        readMetadata(0, true);
        m_audioFilePosition = 0;
    }
    if(m_pwst.f_clientIsConnected) m_pwst.availableBytes = m_client->available(); // available from stream

    // chunked data tramsfer - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_chunked && m_pwst.availableBytes > 0) {
        uint8_t readedBytes = 0;
        if(!m_pwst.chunkSize){
            if(m_pwst.f_skipCRLF){
                if(m_client->available() < 2) { // avoid getting out of sync
                    if(!m_f_tts) AUDIO_INFO("webstream chunked: not enough bytes available for skipCRLF");
                    return;
                }
                int a =audioFileRead(); if(a != 0x0D) AUDIO_LOG_WARN("chunk count error, expected: 0x0D, received: 0x%02X", a); // skipCR
                int b =audioFileRead(); if(b != 0x0A) AUDIO_LOG_WARN("chunk count error, expected: 0x0A, received: 0x%02X", b); // skipLF
                m_pwst.f_skipCRLF = false;
            }
            if(m_client->available()){
                m_pwst.chunkSize = readChunkSize(&readedBytes);
                if(m_pwst.chunkSize > 0) {
                    m_pwst.f_skipCRLF = true; // skip next CRLF
                }
                // AUDIO_LOG_WARN("chunk size: %ld", m_pwst.chunkSize);
            }
        }
        m_pwst.availableBytes = min(m_pwst.availableBytes, m_pwst.chunkSize);
    }

    // we have metadata  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_metadata && m_pwst.availableBytes) {
        if(m_metacount == 0) {
            int metaLen = readMetadata(m_pwst.availableBytes);
            m_pwst.chunkSize -= metaLen; // reduce chunkSize by metadata length
            return;
        }
        m_pwst.availableBytes = min(m_pwst.availableBytes, m_metacount);
    }

    // if the buffer is often almost empty issue a warning - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_stream) {
        if(!m_f_allDataReceived) if(streamDetection(m_pwst.availableBytes)) return;
        if(!m_pwst.f_clientIsConnected) {if(m_f_tts && !m_f_allDataReceived) m_f_allDataReceived = true;} // connection closed (OpenAi)
    }

    // buffer fill routine - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_pwst.availableBytes) {
        m_pwst.availableBytes = min(m_pwst.availableBytes, (uint32_t)InBuff.writeSpace());
        int32_t bytesAddedToBuffer = audioFileRead(InBuff.getWritePtr(), m_pwst.availableBytes);
        if(bytesAddedToBuffer > 0) {
            if(m_f_metadata) m_metacount -= bytesAddedToBuffer;
            if(m_f_chunked) m_pwst.chunkSize -= bytesAddedToBuffer;
            InBuff.bytesWritten(bytesAddedToBuffer);
        }
    }

    // start audio decoding - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(InBuff.bufferFilled() > m_pwst.maxFrameSize && !m_f_stream) { // waiting for buffer filled
        if(m_codec == CODEC_OGG) { // AUDIO_LOG_INFO("determine correct codec here");
            uint8_t codec = determineOggCodec(InBuff.getReadPtr(), m_pwst.maxFrameSize);
            if(codec == CODEC_FLAC) {initializeDecoder(codec); m_codec = codec;}
            if(codec == CODEC_OPUS) {initializeDecoder(codec); m_codec = codec;}
            if(codec == CODEC_VORBIS) {initializeDecoder(codec); m_codec = codec;}
        }
        AUDIO_INFO("stream ready");
        m_f_stream = true;  // ready to play the audio data
    }

    if(m_f_eof) {
        AUDIO_INFO("End of webstream: \"%s\"", m_lastHost.c_get());
        if(audio_eof_stream) audio_eof_stream(m_lastHost.c_get());
        stopSong();
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::processWebFile() {
    if(!m_lastHost.valid()) {AUDIO_LOG_ERROR("m_lastHost is empty"); return;}  // guard
    m_pwf.maxFrameSize = InBuff.getMaxBlockSize();        // every frame is not bigger
    uint32_t availableBytes = 0;
    int32_t bytesAddedToBuffer = 0;
    // m_pwf.f_clientIsConnected = m_client->connected();     // we are not connected

    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        m_f_stream = false;
        m_pwf.audioHeaderFound = false;
        m_pwf.newFilePos = 0;
        m_pwf.ctime = millis();
        m_audioFilePosition = 0;
        m_audioDataSize = m_audioFileSize;
        m_audioDataStart = 0;
        m_f_allDataReceived = false;
        m_pwf.timeout = 8000; // ms
    }

    if(m_resumeFilePos >= 0 ) {  // we have a resume file position
        m_pwf.newFilePos = newInBuffStart(m_resumeFilePos);
        if(m_pwf.newFilePos < 0) AUDIO_LOG_WARN("skip to new position was not successful");
        m_haveNewFilePos  = m_pwf.newFilePos;
        m_resumeFilePos = -1;
        m_f_allDataReceived = false;
        return;
    }

    m_pwf.availableBytes = min(m_client->available(), InBuff.writeSpace());
    m_pwf.bytesAddedToBuffer = audioFileRead(InBuff.getWritePtr(), m_pwf.availableBytes);
    if(m_pwf.bytesAddedToBuffer > 0) {InBuff.bytesWritten(m_pwf.bytesAddedToBuffer);}
    if(m_audioDataSize && m_audioFilePosition >= m_audioDataSize){if(!m_f_allDataReceived) m_f_allDataReceived = true;}
    if(!m_audioDataSize && m_audioFilePosition == m_audioFileSize){if(!m_f_allDataReceived) m_f_allDataReceived = true;}
    // AUDIO_LOG_ERROR("m_audioFilePosition %u >= m_audioDataSize %u, m_f_allDataReceived % i", m_audioFilePosition, m_audioDataSize, m_f_allDataReceived);

    if(!m_f_stream) {
        if(m_codec == CODEC_OGG) { // AUDIO_LOG_ERROR("determine correct codec here");
            uint8_t codec = determineOggCodec(InBuff.getReadPtr(), m_pwf.maxFrameSize);
            if     (codec == CODEC_FLAC)   {initializeDecoder(codec); m_codec = CODEC_FLAC;   return;}
            else if(codec == CODEC_OPUS)   {initializeDecoder(codec); m_codec = CODEC_OPUS;   return;}
            else if(codec == CODEC_VORBIS) {initializeDecoder(codec); m_codec = CODEC_VORBIS; return;}
            else                           {stopSong(); return;}
        }
        if(m_controlCounter != 100) {
            if((millis() - m_pwf.ctime) > m_pwf.timeout) {
                AUDIO_LOG_ERROR("audioHeader reading timeout");
                m_f_running = false;
                goto exit;
            }
            if(InBuff.bufferFilled() > m_pwf.maxFrameSize || (InBuff.bufferFilled() == m_audioFileSize) || m_f_allDataReceived) { // at least one complete frame or the file is smaller
                InBuff.bytesWasRead(readAudioHeader(InBuff.getMaxAvailableBytes()));
            }
            if(m_controlCounter == 100){
                if(m_audioDataStart > 0){ m_pwf.audioHeaderFound = true; }
                if(!m_audioDataSize) m_audioDataSize = m_audioFileSize;
            }
            return;
        }
        else {
            m_f_stream = true;
            AUDIO_INFO("stream ready");
        }
    }

    // end of file reached? - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_eof){ // m_f_eof and m_f_ID3v1TagFound will be set in playAudioData()
        if(m_f_ID3v1TagFound) readID3V1Tag();
exit:
        stopSong();
        if(m_f_tts) {
            AUDIO_INFO("End of speech \"%s\"", m_speechtxt.c_get());
            if(audio_eof_speech) audio_eof_speech(m_speechtxt.c_get());
        }
        else {
            AUDIO_INFO("End of webstream: \"%s\"", m_lastHost.c_get());
            if(audio_eof_stream) audio_eof_stream(m_lastHost.c_get());
        }

        m_audioCurrentTime = 0;
        m_audioFileDuration = 0;
        m_resumeFilePos = -1;
        m_haveNewFilePos = 0;
        m_codec = CODEC_NONE;
        return;
    }
}

// void Audio::processWebFile() {
//     if(!m_lastHost.valid()) {AUDIO_LOG_ERROR("m_lastHost is empty"); return;}  // guard
//     m_pwf.maxFrameSize = InBuff.getMaxBlockSize();        // every frame is not bigger
//     m_pwf.f_clientIsConnected = m_client->connected();     // we are not connected
//     int32_t bytesAddedToBuffer = 0;
//     uint32_t availableBytes = 0;

//     // first call, set some values to default - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//     if(m_f_firstCall) { // runs only ont time per connection, prepare for start
//         m_f_firstCall = false;
//         m_f_stream = false;
//         m_pwf.audioHeaderFound = false;
//         m_pwf.newFilePos = 0;
//         m_pwf.f_waitingForPayload = true;
//         m_t0 = millis();
//         m_pwf.byteCounter = 0;
//         m_pwf.chunkSize = 0;
//         m_pwf.audioDataCount = 0;
//         m_pwf.nextChunkCount = 0;
//         m_audioDataSize = m_audioFileSize;
//         m_controlCounter = 0;
//         m_f_allDataReceived = false;
//         m_audioFilePosition = 0;
//     }

//     if(m_resumeFilePos >= 0 ) {  // we have a resume file position
//         m_pwf.newFilePos = newInBuffStart(m_resumeFilePos);
//         int x = MP3FindSyncWord(InBuff.getReadPtr(), InBuff.getMaxAvailableBytes());
//         log_w("x %i", x);
//         if(m_pwf.newFilePos < 0) AUDIO_LOG_WARN("skip to new position was not successful");
//         m_haveNewFilePos  = m_pwf.newFilePos;
//         m_resumeFilePos = -1;
//         m_pwf.byteCounter = m_pwf.newFilePos + m_audioDataStart;
//         m_pwf.audioDataCount = m_pwf.newFilePos;
//         return;
//     }

//     if(m_f_allDataReceived == true) {goto all_data_received;}

//     if(m_pwf.f_clientIsConnected) availableBytes = m_client->available(); // available from stream
//     // else AUDIO_LOG_ERROR("client not connected");

//     // waiting for payload - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//     if(m_pwf.f_waitingForPayload){
//         if(availableBytes == 0){
//             if(m_t0 + 3000 < millis()) {
//                 m_pwf.f_waitingForPayload = false;
//                 AUDIO_LOG_ERROR("no payload received, timeout");
//                 stopSong();
//                 m_f_running = false;
//             }
//             return;
//         }
//         else {
//             m_pwf.f_waitingForPayload = false;
//         }
//     }

//     // chunked data tramsfer - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//     if(m_f_chunked && availableBytes) {
//         uint8_t readedBytes = 0;
//         if(m_f_chunked && m_pwf.nextChunkCount == m_pwf.byteCounter) {
//             if(m_pwf.chunkSize > 0){
//                 if(m_client->available() < 2) { // avoid getting out of sync
//                     AUDIO_INFO("webfile chunked: not enough bytes available for skipCRLF");
//                     return;
//                 }
//                 int a =audioFileRead(); if(a != 0x0D) AUDIO_LOG_WARN("chunk count error, expected: 0x0D, received: 0x%02X", a); // skipCR
//                 int b =audioFileRead(); if(b != 0x0A) AUDIO_LOG_WARN("chunk count error, expected: 0x0A, received: 0x%02X", b); // skipLF
//             }
//             m_pwf.chunkSize = readChunkSize(&readedBytes);
//             if(m_pwf.chunkSize == 0) m_f_allDataReceived = true; // last chunk
//             AUDIO_LOG_INFO("chunk size: %d", m_pwf.chunkSize);
//             m_pwf.nextChunkCount += m_pwf.chunkSize;
//             m_audioDataSize += m_pwf.chunkSize;
//         }
//         availableBytes = min(availableBytes, m_pwf.nextChunkCount - m_pwf.byteCounter);
//     }
//     if(!m_f_chunked && m_pwf.byteCounter >= m_audioFileSize) {m_f_allDataReceived = true; goto all_data_received;}
//     if(!m_pwf.f_clientIsConnected) {if(!m_f_allDataReceived)  m_f_allDataReceived = true; goto all_data_received;} // connection closed
//     // AUDIO_LOG_INFO("byteCounter %u >= m_audioDataSize %u, byteCounter, m_pwf.nextChunkCount);

//     // if the buffer is often almost empty issue a warning - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//     if(m_f_stream) {if(streamDetection(availableBytes)) return;}
//     availableBytes = min(availableBytes, (uint32_t)InBuff.writeSpace());
//     bytesAddedToBuffer = 0;
//     if(m_pwf.f_clientIsConnected) bytesAddedToBuffer = audioFileRead(InBuff.getWritePtr(), availableBytes);
//     if(bytesAddedToBuffer > 0) {
//         m_pwf.byteCounter += bytesAddedToBuffer;
//         if(m_f_chunked) m_chunkcount -= bytesAddedToBuffer;
//         if(m_controlCounter == 100) m_pwf.audioDataCount += bytesAddedToBuffer;
//         InBuff.bytesWritten(bytesAddedToBuffer);
//     }

//     // we have a webfile, read the file header first - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//     if(m_controlCounter != 100) {
//         if(InBuff.bufferFilled() > m_pwf.maxFrameSize || (m_pwf.nextChunkCount && (InBuff.bufferFilled() == m_pwf.nextChunkCount))) { // at least one complete frame or the file is smaller
//             int32_t bytesRead = readAudioHeader(InBuff.getMaxAvailableBytes());
//             if(bytesRead > 0) InBuff.bytesWasRead(bytesRead);
//         }
//         return;
//     }

//     if(m_codec == CODEC_OGG) { // AUDIO_LOG_INFO("determine correct codec here");
//         uint8_t codec = determineOggCodec(InBuff.getReadPtr(), m_pwf.maxFrameSize);
//         if     (codec == CODEC_FLAC)   {initializeDecoder(codec); m_codec = codec; return;}
//         else if(codec == CODEC_OPUS)   {initializeDecoder(codec); m_codec = codec; return;}
//         else if(codec == CODEC_VORBIS) {initializeDecoder(codec); m_codec = codec; return;}
//         else {stopSong(); return;}
//     }

//     if(!m_f_stream && m_controlCounter == 100) {
//         m_f_stream = true; // ready to play the audio data
//         if(m_audioDataStart > 0){ m_pwf.audioHeaderFound = true; }
//         uint16_t filltime = millis() - m_t0;
//         AUDIO_INFO("Webfile: stream ready, buffer filled in %d ms", filltime);
//         return;
//     }
// all_data_received:
//     // end of webfile reached? - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//     if(m_f_eof) { // m_f_eof and m_f_ID3v1TagFound will be set in playAudioData()
//         if(m_f_ID3v1TagFound) readID3V1Tag();
// exit:
//         stopSong();
//         if(m_f_tts) {
//             AUDIO_INFO("End of speech \"%s\"", m_speechtxt.c_get());
//             if(audio_eof_speech) audio_eof_speech(m_speechtxt.c_get());
//         }
//         else {
//             AUDIO_INFO("End of webstream: \"%s\"", m_lastHost.c_get());
//             if(audio_eof_stream) audio_eof_stream(m_lastHost.c_get());
//         }
//         return;
//     }
//     return;
// }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void Audio::processWebStreamTS() {
    uint32_t        availableBytes;     // available bytes in stream
    uint8_t         ts_packetStart = 0;
    uint8_t         ts_packetLength = 0;

    // first call, set some values to default ———————————————————————————————————
    if(m_f_firstCall) {   // runs only ont time per connection, prepare for start
        m_audioFilePosition = 0;
        m_f_firstCall = false;
        m_f_m3u8data = true;
        m_pwsst.f_firstPacket = true;
        m_pwsst.f_chunkFinished = false;
        m_pwsst.f_nextRound = false;
        m_pwsst.byteCounter = 0;
        m_pwsst.chunkSize = 0;
        m_t0 = millis();
        m_pwsst.ts_packetPtr = 0;
        if(!m_pwsst.ts_packet.valid()) m_pwsst.ts_packet.alloc_array(m_pwsst.ts_packetsize, "TS Packet"); // first init
    } //—————————————————————————————————————————————————————————————————————————

    if(m_dataMode != AUDIO_DATA) return; // guard

nextRound:
    availableBytes = m_client->available();
    if(availableBytes) {
        /* If the m3u8 stream uses 'chunked data transfer' no content length is supplied. Then the chunk size determines the audio data to be processed.
           However, the chunk size in some streams is limited to 32768 bytes, although the chunk can be larger. Then the chunk size is
           calculated again. The data used to calculate (here readedBytes) the chunk size is not part of it.
        */
        uint8_t readedBytes = 0;
        uint32_t minAvBytes = 0;
        if(m_f_chunked && m_pwsst.chunkSize == m_pwsst.byteCounter) {m_pwsst.chunkSize += readChunkSize(&readedBytes); goto exit;}
        if(m_pwsst.chunkSize) minAvBytes = min3(availableBytes, m_pwsst.ts_packetsize - m_pwsst.ts_packetPtr, m_pwsst.chunkSize - m_pwsst.byteCounter);
        else          minAvBytes = min(availableBytes, (uint32_t)(m_pwsst.ts_packetsize - m_pwsst.ts_packetPtr));

        int res = audioFileRead(m_pwsst.ts_packet.get() + m_pwsst.ts_packetPtr, minAvBytes);
        if(res > 0) {
            m_pwsst.ts_packetPtr += res;
            m_pwsst.byteCounter += res;
            if(m_pwsst.ts_packetPtr < m_pwsst.ts_packetsize) return; // not enough data yet, the process must be repeated if the packet size (188 bytes) is not reached
            m_pwsst.ts_packetPtr = 0;
            if(m_pwsst.f_firstPacket) { // search for ID3 Header in the first packet
                m_pwsst.f_firstPacket = false;
                uint8_t ID3_HeaderSize = process_m3u8_ID3_Header(m_pwsst.ts_packet.get());
                if(ID3_HeaderSize > m_pwsst.ts_packetsize) {
                    AUDIO_LOG_ERROR("ID3 Header is too big");
                    stopSong();
                    return;
                }
                if(ID3_HeaderSize) {
                    memcpy(m_pwsst.ts_packet.get(), &m_pwsst.ts_packet.get()[ID3_HeaderSize], m_pwsst.ts_packetsize - ID3_HeaderSize);
                    m_pwsst.ts_packetPtr = m_pwsst.ts_packetsize - ID3_HeaderSize;
                    return;
                }
            }
            ts_parsePacket(&m_pwsst.ts_packet.get()[0], &ts_packetStart, &ts_packetLength); // todo: check for errors

            if(ts_packetLength) {
                size_t ws = InBuff.writeSpace();
                if(ws >= ts_packetLength) {
                    memcpy(InBuff.getWritePtr(), m_pwsst.ts_packet.get() + ts_packetStart, ts_packetLength);
                    InBuff.bytesWritten(ts_packetLength);
                    m_pwsst.f_nextRound = true;
                }
                else {
                    memcpy(InBuff.getWritePtr(), m_pwsst.ts_packet.get() + ts_packetStart, ws);
                    InBuff.bytesWritten(ws);
                    memcpy(InBuff.getWritePtr(), &m_pwsst.ts_packet.get()[ws + ts_packetStart], ts_packetLength - ws);
                    InBuff.bytesWritten(ts_packetLength - ws);
                }
            }
            if (m_pwsst.byteCounter == m_audioFileSize || m_pwsst.byteCounter == m_pwsst.chunkSize) {
                m_pwsst.f_chunkFinished = true;
                m_pwsst.f_nextRound = false;
                m_pwsst.byteCounter = 0;
                int av = m_client->available();
                if(av == 7) for(int i = 0; i < av; i++) audioFileRead(); // waste last chunksize: 0x0D 0x0A 0x30 0x0D 0x0A 0x0D 0x0A (==0, end of chunked data transfer)
            }
            if(m_audioFileSize && m_pwsst.byteCounter > m_audioFileSize) {AUDIO_LOG_ERROR("byteCounter overflow, byteCounter: %d, contentlength: %d", m_pwsst.byteCounter, m_audioFileSize); return;}
            if(m_pwsst.chunkSize       && m_pwsst.byteCounter > m_pwsst.chunkSize)       {AUDIO_LOG_ERROR("byteCounter overflow, byteCounter: %d, chunkSize: %d", m_pwsst.byteCounter, m_pwsst.chunkSize); return;}
        }
    }
    if(m_pwsst.f_chunkFinished) {
        if(InBuff.bufferFilled() < 60000) {
            m_pwsst.f_chunkFinished = false;
            m_f_continue = true;
        }
    }

    // if the buffer is often almost empty issue a warning - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_stream) {
        if(streamDetection(availableBytes)) goto exit;
    }

    // buffer fill routine  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(true) {                                                  // statement has no effect
        if(InBuff.bufferFilled() > 60000 && !m_f_stream) {     // waiting for buffer filled
            m_f_stream = true;                                  // ready to play the audio data
            uint16_t filltime = millis() - m_t0;
            AUDIO_INFO("stream ready");
            AUDIO_INFO("buffer filled in %d ms", filltime);
        }
    }
    if(m_pwsst.f_nextRound){goto nextRound;}
exit:
    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::processWebStreamHLS() {

    // first call, set some values to default - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        m_f_m3u8data = true;
        m_t0 = millis();
        m_controlCounter = 0;
        m_audioFilePosition = 0;

        m_pwsHLS.maxFrameSize = InBuff.getMaxBlockSize(); // every mp3/aac frame is not bigger
        m_pwsHLS.ID3BuffSize = 4096;
        m_pwsHLS.availableBytes = 0;
        m_pwsHLS.firstBytes = true;
        m_pwsHLS.f_chunkFinished = false;
        m_pwsHLS.byteCounter = 0;
        m_pwsHLS.chunkSize = 0;
        m_pwsHLS.ID3WritePtr = 0;
        m_pwsHLS.ID3ReadPtr = 0;
        m_pwsHLS.ID3Buff.alloc(m_pwsHLS.ID3BuffSize, "ID3Buff");
    }

    if(m_dataMode != AUDIO_DATA) return; // guard

    m_pwsHLS.availableBytes = m_client->available();
    if(m_pwsHLS.availableBytes) { // an ID3 header could come here
        uint8_t readedBytes = 0;

        if(m_f_chunked && !m_pwsHLS.chunkSize) {
            m_pwsHLS.chunkSize = readChunkSize(&readedBytes);
            m_pwsHLS.byteCounter += readedBytes;
        }

        if(m_pwsHLS.firstBytes) {
            if(m_pwsHLS.ID3WritePtr < m_pwsHLS.ID3BuffSize) {
                m_pwsHLS.ID3WritePtr += audioFileRead(&m_pwsHLS.ID3Buff[m_pwsHLS.ID3WritePtr], m_pwsHLS.ID3BuffSize - m_pwsHLS.ID3WritePtr);
                return;
            }
            if(m_controlCounter < 100) {
                int res = read_ID3_Header(&m_pwsHLS.ID3Buff[m_pwsHLS.ID3ReadPtr], m_pwsHLS.ID3BuffSize - m_pwsHLS.ID3ReadPtr);
                if(res >= 0) m_pwsHLS.ID3ReadPtr += res;
                if(m_pwsHLS.ID3ReadPtr > m_pwsHLS.ID3BuffSize) {
                    AUDIO_LOG_ERROR("buffer overflow");
                    stopSong();
                    return;
                }
                return;
            }
            if(m_controlCounter != 100) return;

            size_t ws = InBuff.writeSpace();
            if(ws >= m_pwsHLS.ID3BuffSize - m_pwsHLS.ID3ReadPtr) {
                memcpy(InBuff.getWritePtr(), &m_pwsHLS.ID3Buff[m_pwsHLS.ID3ReadPtr], m_pwsHLS.ID3BuffSize - m_pwsHLS.ID3ReadPtr);
                InBuff.bytesWritten(m_pwsHLS.ID3BuffSize - m_pwsHLS.ID3ReadPtr);
            }
            else {
                memcpy(InBuff.getWritePtr(), &m_pwsHLS.ID3Buff[m_pwsHLS.ID3ReadPtr], ws);
                InBuff.bytesWritten(ws);
                memcpy(InBuff.getWritePtr(), &m_pwsHLS.ID3Buff[ws + m_pwsHLS.ID3ReadPtr], m_pwsHLS.ID3BuffSize - (m_pwsHLS.ID3ReadPtr + ws));
                InBuff.bytesWritten(m_pwsHLS.ID3BuffSize - (m_pwsHLS.ID3ReadPtr + ws));
            }
            m_pwsHLS.ID3Buff.reset();
            m_pwsHLS.byteCounter += m_pwsHLS.ID3BuffSize;
            m_pwsHLS.firstBytes = false;
        }

        size_t bytesWasWritten = 0;
        if(InBuff.writeSpace() >= m_pwsHLS.availableBytes) {
        //    if(availableBytes > 1024) availableBytes = 1024; // 1K throttle
            bytesWasWritten = audioFileRead(InBuff.getWritePtr(), m_pwsHLS.availableBytes);
        }
        else { bytesWasWritten = audioFileRead(InBuff.getWritePtr(), InBuff.writeSpace()); }
        InBuff.bytesWritten(bytesWasWritten);

        m_pwsHLS.byteCounter += bytesWasWritten;

        if(m_pwsHLS.byteCounter == m_audioFileSize || m_pwsHLS.byteCounter == m_pwsHLS.chunkSize) {
            m_pwsHLS.f_chunkFinished = true;
            m_pwsHLS.byteCounter = 0;
        }
    }

    if(m_pwsHLS.f_chunkFinished) {
        if(InBuff.bufferFilled() < 50000) {
            m_pwsHLS.f_chunkFinished = false;
            m_f_continue = true;
        }
    }

    // if the buffer is often almost empty issue a warning or try a new connection - - - - - - - - - - - - - - - - - - -
    if(m_f_stream) {
        if(streamDetection(m_pwsHLS.availableBytes)) return;
    }

    if(InBuff.bufferFilled() > m_pwsHLS.maxFrameSize && !m_f_stream) { // waiting for buffer filled
        m_f_stream = true;                                    // ready to play the audio data
        //uint16_t filltime = millis() - m_t0;
        AUDIO_INFO("stream ready");
        // AUDIO_INFO("buffer filled in %u ms", filltime);
    }
    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::playAudioData() {

    if(!m_f_stream || m_f_eof || m_f_lockInBuffer || !m_f_running){m_validSamples = 0; return;} // guard, stream not ready or eof reached or InBuff is locked or not running
    if(m_validSamples) {playChunk();                                                   return;} // guard, play samples first
    //--------------------------------------------------------------------------------
    m_pad.count = 0;
    m_pad.bytesToDecode = InBuff.bufferFilled();
    m_pad.bytesDecoded = 0;

    if(m_f_firstPlayCall) {
        m_f_firstPlayCall = false;
        m_pad.count = 0;
        m_pad.oldAudioDataSize = 0;
        m_bytesNotConsumed = 0;
        m_pad.lastFrames = false;
        m_f_eof = false;
    }
    //--------------------------------------------------------------------------------

    m_f_audioTaskIsDecoding = true;

    if((m_dataMode == AUDIO_LOCALFILE || m_streamType == ST_WEBFILE) && m_playlistFormat != FORMAT_M3U8)  { // local file or webfile but not m3u8 file
        if(!m_audioDataSize) goto exit; // no data to decode if filesize is 0
        if(m_audioDataSize != m_pad.oldAudioDataSize) { // Special case: Metadata in ogg files are recognized by the decoder,
            if(m_f_ogg)m_bytesNotConsumed = 0;
            m_pad.oldAudioDataSize = m_audioDataSize;
        }

        m_pad.bytesToDecode = min(m_audioFileSize - m_audioFilePosition + InBuff.bufferFilled(), InBuff.getMaxBlockSize());

        if(m_audioFileSize - m_audioFilePosition == 0) m_f_allDataReceived = true;
        if(m_f_allDataReceived && InBuff.bufferFilled() < InBuff.getMaxBlockSize()){ // last frames to decode
            m_pad.lastFrames = true;
        }
        if(m_pad.bytesToDecode == 0)            {m_f_eof = true; goto exit;} // file end reached

        if(m_codec == CODEC_MP3 && m_pad.lastFrames && m_pad.bytesToDecode == 128){
            m_f_ID3v1TagFound = true; m_f_eof = true;
            goto exit;
        }
    }
    else{
        if(InBuff.bufferFilled() < InBuff.getMaxBlockSize() && m_f_allDataReceived) {m_pad.lastFrames = true;}
        m_pad.bytesToDecode = min(InBuff.bufferFilled(), InBuff.getMaxBlockSize());
    }

    if(m_pad.lastFrames){
        m_pad.bytesDecoded = sendBytes(InBuff.getReadPtr(), m_pad.bytesToDecode);
    }
    else{
        if(InBuff.bufferFilled() >= InBuff.getMaxBlockSize()) m_pad.bytesDecoded = sendBytes(InBuff.getReadPtr(), m_pad.bytesToDecode);
        else m_pad.bytesDecoded = 0; // Inbuff not filled enough
    }

    if(m_pad.bytesDecoded <= 0) {
        if(m_pad.lastFrames) {m_f_eof = true; goto exit;} // end of file reached
        m_pad.count++;
        vTaskDelay(50); // wait for data
        if(m_pad.count == 10) {if(m_f_allDataReceived) m_f_eof = true;}  // maybe slow stream
        goto exit; // syncword at pos0
    }
    m_pad.count = 0;

    if(m_pad.bytesDecoded > 0) {
        InBuff.bytesWasRead(m_pad.bytesDecoded);

        if(m_f_allDataReceived && InBuff.bufferFilled() == 0) {
            m_f_eof = true; // end of file reached
        }
    }

exit:
    m_f_audioTaskIsDecoding = false;
    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::parseHttpResponseHeader() { // this is the response to a GET / request

    if(m_dataMode != HTTP_RESPONSE_HEADER) return false;
    if(!m_currentHost.valid()) {AUDIO_LOG_ERROR("m_currentHost is empty"); return false;}

    m_phreh.ctime = millis();
    m_phreh.timeout = 4500; // ms

    if(m_client->available() == 0) {
        if(!m_phreh.f_time) {
            m_phreh.stime = millis();
            m_phreh.f_time = true;
        }
        if((millis() - m_phreh.stime) > m_phreh.timeout) {
            AUDIO_LOG_ERROR("timeout");
            m_phreh.f_time = false;
            return false;
        }
    }
    m_phreh.f_time = false;

    ps_ptr<char>rhl;
    rhl.alloc(1024, "rhl"); // responseHeaderline
    rhl.clear();
    bool ct_seen = false;

    while(true) { // outer while
        uint16_t pos = 0;
        if((millis() - m_phreh.ctime) > m_phreh.timeout) {
            AUDIO_LOG_ERROR("timeout");
            m_f_timeout = true;
            goto exit;
        }
        while(m_client->available()) {
            uint8_t b = audioFileRead();
            if(b == '\n') {
                if(!pos) { // empty line received, is the last line of this responseHeader
                    if(ct_seen) goto lastToDo;
                    else {if(!m_client->available())  goto exit;}
                }
                break;
            }
            if(b == '\r') rhl[pos] = 0;
            if(b < 0x20) continue;
            rhl[pos] = b;
            pos++;
            if(pos == 1023) {
                pos = 1022;
                continue;
            }
            if(pos == 1022) {
                rhl[pos] = '\0';
                AUDIO_LOG_WARN("responseHeaderline overflow");
            }
        } // inner while
        if(!pos) {
            vTaskDelay(5);
            continue;
        }
        if(rhl.starts_with_icase("HTTP/")) { // HTTP status error code
            char statusCode[5];
            statusCode[0] = rhl[9];
            statusCode[1] = rhl[10];
            statusCode[2] = rhl[11];
            statusCode[3] = '\0';
            int sc = atoi(statusCode);
            if(sc > 310) { // e.g. HTTP/1.1 301 Moved Permanently
                if(audio_showstreamtitle) audio_showstreamtitle(rhl.get());
                goto exit;
            }
        }

        else if(rhl.starts_with_icase("content-type:")) { // content-type: text/html; charset=UTF-8
            int idx = rhl.index_of(';', 13);
            if(idx > 0) rhl[idx] = '\0';
            if(parseContentType(rhl.get() + 13)) ct_seen = true;
            else{
                AUDIO_LOG_WARN("unknown contentType %s", rhl.get() + 13);
                goto exit;
            }
        }

        else if(rhl.starts_with_icase("location:")) {
            int pos = rhl.index_of_icase("http", 0);
            if(pos >= 0) {
                const char* c_host = (rhl.get() + pos);
                if(!m_currentHost.equals(c_host)) { // prevent a loop
                    int pos_slash = indexOf(c_host, "/", 9);
                    if(pos_slash > 9) {
                        if(!strncmp(c_host, m_currentHost.get(), pos_slash)) {
                            AUDIO_INFO("redirect to new extension at existing host \"%s\"", c_host);
                            if(m_playlistFormat == FORMAT_M3U8) {
                                m_lastHost.assign(c_host);
                                m_f_m3u8data = true;
                            }
                            httpPrint(c_host);
                            while(m_client->available()) audioFileRead(); // empty client buffer
                            return true;
                        }
                    }
                    AUDIO_INFO("redirect to new host \"%s\"", c_host);
                    m_f_reset_m3u8Codec = false;
                    httpPrint(c_host);
                    return true;
                }
            }
            else{
                AUDIO_LOG_WARN("unknown redirection: %s", rhl.c_get());
            }
        }

        else if(rhl.starts_with_icase("content-encoding:")) {
            AUDIO_INFO("%s", rhl.get());
            if(rhl.contains("gzip")) {
                AUDIO_INFO("can't extract gzip");
                goto exit;
            }
        }

        else if(rhl.starts_with_icase("content-disposition:")) { // e.g we have this headerline:  content-disposition: attachment; filename=stream.asx
            int idx = rhl.index_of_icase("filename=");
            if (idx >= 0)  {
                ps_ptr<char> fn;
                fn.assign(rhl.get() + idx + 9); // Position directly after "filename="
                fn.replace("\"", ""); // remove '\"' around filename if present
                AUDIO_INFO("Filename is %s", fn.get());
            }
        }
        else if(rhl.starts_with_icase("connection:")) {
            if(rhl.contains_with_icase("close")) {m_f_connectionClose = true; /* AUDIO_LOG_ERROR("connection will be closed"); */} // ends after ogg last Page is set
        }

        else if(rhl.starts_with_icase("icy-genre:")) {
            AUDIO_INFO("icy-genre: %s", rhl.get() + 10 ); // Ambient, Rock, etc
        }

        else if(rhl.starts_with_icase("icy-logo:")) {
            ps_ptr<char> icyLogo;
            icyLogo.assign(rhl.get() + 9); // Get logo URL
            icyLogo.trim();
            if(icyLogo.strlen() > 0){
                AUDIO_INFO("icy-logo: %s", icyLogo.get());
                if(audio_icylogo) audio_icylogo(icyLogo.get());
            }
        }

        else if(rhl.starts_with_icase("icy-br:")) {
            ps_ptr<char>c_bitRate; c_bitRate.assign(rhl.get() + 7);
            c_bitRate.trim();
            c_bitRate.append("000"); // Found bitrate tag, read the bitrate in Kbit
            setBitrate(c_bitRate.to_uint32(10));
            AUDIO_INFO("icy-bitrate: %s", c_bitRate.get());
            if(audio_bitrate) audio_bitrate(c_bitRate.get());
        }

        else if(rhl.starts_with_icase("icy-metaint:")) {
            const char* c_metaint = (rhl.get() + 12);
            int32_t     i_metaint = atoi(c_metaint);
            m_metaint = i_metaint;
            if(m_metaint) m_f_metadata = true; // Multimediastream
        }

        else if(rhl.starts_with_icase("icy-name:")) {
        //  AUDIO_LOG_INFO("%s", rhl.get());
            ps_ptr<char> icyName;
            icyName.assign(rhl.get() + 9); // Get station name
            icyName.trim();
            if(icyName.strlen() > 0) {
                AUDIO_INFO("icy-name: %s", icyName.get());
                if(audio_showstation) audio_showstation(icyName.get());
            }
        }

        else if(rhl.starts_with_icase("content-length:")) {
            const char* c_cl = (rhl.get() + 15);
            int32_t     i_cl = atoi(c_cl);
            m_audioFileSize = i_cl;
            // AUDIO_INFO("content-length: %lu", (long unsigned int)m_audioFileSize);
        }

        else if(rhl.starts_with_icase("icy-description:")) {
            const char* c_idesc = (rhl.get() + 16);
            while(c_idesc[0] == ' ') c_idesc++;
            latinToUTF8(rhl); // if already UTF-8 do nothing, otherwise convert to UTF-8
            if(strlen(c_idesc) > 0 && specialIndexOf((uint8_t*)c_idesc, "24bit", 0) > 0) {
                AUDIO_INFO("icy-description: %s has to be 8 or 16", c_idesc);
                stopSong();
            }
            if(audio_icydescription) audio_icydescription(c_idesc);
        }

        else if(rhl.starts_with_icase("transfer-encoding:")) {
            if(rhl.ends_with_icase("chunked")) { // Station provides chunked transfer
                m_f_chunked = true;
                AUDIO_INFO("chunked data transfer");
                m_chunkcount = 0; // Expect chunkcount in DATA
            }
        }

        else if(rhl.starts_with_icase("accept-ranges:")) {
            if(rhl.ends_with_icase("bytes")) m_f_acceptRanges = true;
        //    AUDIO_LOG_INFO("%s", rhl.c_get());
        }

        else if(rhl.starts_with_icase("content-range:")) {
            AUDIO_LOG_INFO("%s", rhl.c_get());
        }

        else if(rhl.starts_with_icase("icy-url:")) {
            char* icyurl = (rhl.get() + 8);
            trim(icyurl);
            if(audio_icyurl) audio_icyurl(icyurl);
        }

        else if(rhl.starts_with_icase("www-authenticate:")) {
            AUDIO_INFO("authentification failed, wrong credentials?");
            goto exit;
        }
        else { ; }
    } // outer while

exit: // termination condition
    if(audio_showstation) audio_showstation("");
    if(audio_icydescription) audio_icydescription("");
    if(audio_icyurl) audio_icyurl("");
    m_dataMode = AUDIO_NONE;
    stopSong();
    return false;

lastToDo:
    m_streamType = ST_WEBSTREAM;
    if(m_audioFileSize > 0)          m_streamType = ST_WEBFILE;
    if(m_f_chunked)                  m_streamType = ST_WEBFILE; // Stream comes from a fileserver, metadata have webstreams
    if(m_f_chunked && m_f_metadata)  m_streamType = ST_WEBSTREAM;


    if(m_codec != CODEC_NONE) {
        m_dataMode = AUDIO_DATA; // Expecting data now
        if(!(m_codec == CODEC_OGG)){
            if(!initializeDecoder(m_codec)) return false;
        }
    }
    else if(m_playlistFormat != FORMAT_NONE) {
        m_dataMode = AUDIO_PLAYLISTINIT; // playlist expected
        // AUDIO_LOG_INFO("now parse playlist");
    }
    else {
        AUDIO_INFO("unknown content found at: %s", m_currentHost.c_get());
        goto exit;
    }
    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::parseHttpRangeHeader() { // this is the response to a Range request

    ps_ptr<char>rhl;
    rhl.alloc(1024, "rhl"); // responseHeaderline
    bool ct_seen = false;

    if(m_dataMode != HTTP_RANGE_HEADER){
        AUDIO_LOG_ERROR("wrong datamode %i", m_dataMode);
        goto exit;
    }

    m_phrah.ctime = millis();
    m_phrah.timeout = 4500; // ms

    if(m_client->available() == 0) {
        if(!m_phrah.f_time) {
            m_phrah.stime = millis();
            m_phrah.f_time = true;
        }
        if((millis() - m_phrah.stime) > m_phrah.timeout) {
            AUDIO_LOG_ERROR("timeout");
            m_phrah.f_time = false;
            return false;
        }
    }
    m_phrah.f_time = false;

    rhl.clear();

    while(true) { // outer while
        uint16_t pos = 0;
        if((millis() - m_phrah.ctime) > m_phrah.timeout) {
            AUDIO_LOG_ERROR("timeout");
            m_f_timeout = true;
            goto exit;
        }
        while(m_client->available()) {
            uint8_t b = audioFileRead();
            if(b == '\n') {
                if(!pos) { // empty line received, is the last line of this responseHeader
                    goto lastToDo;
                }
                break;
            }
            if(b == '\r') rhl[pos] = 0;
            if(b < 0x20) continue;
            rhl[pos] = b;
            pos++;
            if(pos == 1023) {
                pos = 1022;
                continue;
            }
            if(pos == 1022) {
                rhl[pos] = '\0';
                AUDIO_LOG_WARN("responseHeaderline overflow");
            }
        } // inner while
        if(!pos) {
            vTaskDelay(5);
            continue;
        }
        // AUDIO_LOG_WARN("rh %s", rhl.c_get());
        if(rhl.starts_with_icase("HTTP/")) { // HTTP status error code
            char statusCode[5];
            statusCode[0] = rhl[9];
            statusCode[1] = rhl[10];
            statusCode[2] = rhl[11];
            statusCode[3] = '\0';
            int sc = atoi(statusCode);
            if(sc > 310) { // e.g. HTTP/1.1 301 Moved Permanently
                if(audio_showstreamtitle) audio_showstreamtitle(rhl.get());
                goto exit;
            }
        }
        if(rhl.starts_with_icase("Server:")) {
            AUDIO_INFO("%s", rhl.c_get());
        }
        if(rhl.starts_with_icase("Content-Length:")) {
        //    AUDIO_INFO("%s", rhl.c_get());
        }
        if(rhl.starts_with_icase("Content-Range:")) {
            AUDIO_INFO("%s", rhl.c_get());
        }
        if(rhl.starts_with_icase("Content-Type:")) {
        //    AUDIO_INFO("%s", rhl.c_get());
        }
    }
exit:
    return false;
lastToDo:
    m_dataMode = AUDIO_DATA;
    m_streamType = ST_WEBFILE;
return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::initializeDecoder(uint8_t codec) {
    switch(codec) {
        case CODEC_MP3:
            if(!MP3Decoder_IsInit()){
                if(!MP3Decoder_AllocateBuffers()) {
                    AUDIO_INFO("The MP3Decoder could not be initialized");
                    goto exit;
                }
                AUDIO_INFO("MP3Decoder has been initialized");
                InBuff.changeMaxBlockSize(m_frameSizeMP3);
            }
            break;
        case CODEC_AAC:
            if(!AACDecoder_IsInit()) {
                if(!AACDecoder_AllocateBuffers()) {
                    AUDIO_INFO("The AACDecoder could not be initialized");
                    goto exit;
                }
                AUDIO_INFO("AACDecoder has been initialized");
                InBuff.changeMaxBlockSize(m_frameSizeAAC);
            }
            break;
        case CODEC_M4A:
            if(!AACDecoder_IsInit()) {
                if(!AACDecoder_AllocateBuffers()) {
                    AUDIO_INFO("The AACDecoder could not be initialized");
                    goto exit;
                }
                AUDIO_INFO("AACDecoder has been initialized");
                InBuff.changeMaxBlockSize(m_frameSizeAAC);
            }
            break;
        case CODEC_FLAC:
            if(!psramFound()) {
                AUDIO_INFO("FLAC works only with PSRAM!");
                goto exit;
            }
            if(!FLACDecoder_AllocateBuffers()) {
                AUDIO_INFO("The FLACDecoder could not be initialized");
                goto exit;
            }
            InBuff.changeMaxBlockSize(m_frameSizeFLAC);
            AUDIO_INFO("FLACDecoder has been initialized");
            break;
        case CODEC_OPUS:
            if(!OPUSDecoder_AllocateBuffers()) {
                AUDIO_INFO("The OPUSDecoder could not be initialized");
                goto exit;
            }
            AUDIO_INFO("OPUSDecoder has been initialized");
            InBuff.changeMaxBlockSize(m_frameSizeOPUS);
            break;
        case CODEC_VORBIS:
            if(!psramFound()) {
                AUDIO_INFO("VORBIS works only with PSRAM!");
                goto exit;
            }
            if(!VORBISDecoder_AllocateBuffers()) {
                AUDIO_INFO("The VORBISDecoder could not be initialized");
                goto exit;
            }
            AUDIO_INFO("VORBISDecoder has been initialized");
            InBuff.changeMaxBlockSize(m_frameSizeVORBIS);
            break;
        case CODEC_WAV: InBuff.changeMaxBlockSize(m_frameSizeWav); break;
        case CODEC_OGG: // the decoder will be determined later (vorbis, flac, opus?)
            break;
        default: goto exit; break;
    }
    return true;

exit:
    stopSong();
    return false;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// clang-format off
bool Audio::parseContentType(char* ct) {
    enum : int { CT_NONE, CT_MP3, CT_AAC, CT_M4A, CT_WAV, CT_FLAC, CT_PLS, CT_M3U, CT_ASX, CT_M3U8, CT_TXT, CT_AACP, CT_OPUS, CT_OGG, CT_VORBIS };

    strlower(ct);
    trim(ct);

    m_codec = CODEC_NONE;
    int ct_val = CT_NONE;

    if(!strcmp(ct, "audio/mpeg")) ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/mpeg3"))                   ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/x-mpeg"))                  ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/x-mpeg-3"))                ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/mp3"))                     ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/aac"))                     ct_val = CT_AAC;
    else if(!strcmp(ct, "audio/x-aac"))                   ct_val = CT_AAC;
    else if(!strcmp(ct, "audio/aacp"))                    ct_val = CT_AAC;
    else if(!strcmp(ct, "video/mp2t")){                   ct_val = CT_AAC;  if(m_m3u8Codec == CODEC_MP3) ct_val = CT_MP3;} // see m3u8redirection()
    else if(!strcmp(ct, "audio/mp4"))                     ct_val = CT_M4A;
    else if(!strcmp(ct, "audio/m4a"))                     ct_val = CT_M4A;
    else if(!strcmp(ct, "audio/x-m4a"))                   ct_val = CT_M4A;
    else if(!strcmp(ct, "audio/wav"))                     ct_val = CT_WAV;
    else if(!strcmp(ct, "audio/x-wav"))                   ct_val = CT_WAV;
    else if(!strcmp(ct, "audio/flac"))                    ct_val = CT_FLAC;
    else if(!strcmp(ct, "audio/x-flac"))                  ct_val = CT_FLAC;
    else if(!strcmp(ct, "audio/scpls"))                   ct_val = CT_PLS;
    else if(!strcmp(ct, "audio/x-scpls"))                 ct_val = CT_PLS;
    else if(!strcmp(ct, "application/pls+xml"))           ct_val = CT_PLS;
    else if(!strcmp(ct, "audio/mpegurl")) {               ct_val = CT_M3U;  if(m_expectedPlsFmt == FORMAT_M3U8) ct_val = CT_M3U8;}
    else if(!strcmp(ct, "audio/x-mpegurl"))               ct_val = CT_M3U;
    else if(!strcmp(ct, "audio/ms-asf"))                  ct_val = CT_ASX;
    else if(!strcmp(ct, "video/x-ms-asf"))                ct_val = CT_ASX;
    else if(!strcmp(ct, "audio/x-ms-asx"))                ct_val = CT_ASX;  // #413
    else if(!strcmp(ct, "application/ogg"))               ct_val = CT_OGG;
    else if(!strcmp(ct, "audio/ogg"))                     ct_val = CT_OGG;
    else if(!strcmp(ct, "application/vnd.apple.mpegurl")) ct_val = CT_M3U8;
    else if(!strcmp(ct, "application/x-mpegurl"))         ct_val = CT_M3U8;
    else if(!strcmp(ct, "application/octet-stream"))      ct_val = CT_TXT;  // ??? listen.radionomy.com/1oldies before redirection
    else if(!strcmp(ct, "text/html"))                     ct_val = CT_TXT;
    else if(!strcmp(ct, "text/plain"))                    ct_val = CT_TXT;
    else if(ct_val == CT_NONE) {
        AUDIO_INFO("ContentType %s not supported", ct);
        return false; // nothing valid had been seen
    }
    else { ; }
    switch(ct_val) {
        case CT_MP3:
            m_codec = CODEC_MP3;
            // { AUDIO_LOG_INFO("ContentType %s, format is mp3", ct); } // ok is likely mp3
            break;
        case CT_AAC:
            m_codec = CODEC_AAC;
            // { AUDIO_LOG_INFO("ContentType %s, format is aac", ct); }
            break;
        case CT_M4A:
            m_codec = CODEC_M4A;
            // { AUDIO_LOG_INFO("ContentType %s, format is aac", ct); }
            break;
        case CT_FLAC:
            m_codec = CODEC_FLAC;
            // { AUDIO_LOG_INFO("ContentType %s, format is flac", ct); }
            break;
        case CT_OPUS:
            m_codec = CODEC_OPUS;
            m_f_ogg = true; // opus is ogg
            // { AUDIO_LOG_INFO("ContentType %s, format is opus", ct); }
            break;
        case CT_VORBIS:
            m_codec = CODEC_VORBIS;
            m_f_ogg = true; // vorbis is ogg
            // AUDIO_LOG_INFO("ContentType %s, format is vorbis", ct);
            break;
        case CT_WAV:
            m_codec = CODEC_WAV;
            // { AUDIO_LOG_INFO("ContentType %s, format is wav", ct); }
            break;
        case CT_OGG:
            m_codec = CODEC_OGG; // determine in first OGG packet -OPUS, VORBIS, FLAC
            m_f_ogg = true; // maybe flac or opus or vorbis
            break;
        case CT_PLS: m_playlistFormat = FORMAT_PLS; break;
        case CT_M3U: m_playlistFormat = FORMAT_M3U; break;
        case CT_ASX: m_playlistFormat = FORMAT_ASX; break;
        case CT_M3U8: m_playlistFormat = FORMAT_M3U8; break;
        case CT_TXT: // overwrite text/plain
            if(m_expectedCodec == CODEC_AAC) {
                m_codec = CODEC_AAC;
                // AUDIO_LOG_INFO("set ct from M3U8 to AAC");
            }
            if(m_expectedCodec == CODEC_MP3) {
                m_codec = CODEC_MP3;
                // AUDIO_LOG_INFO("set ct from M3U8 to MP3");
            }

            if(m_expectedPlsFmt == FORMAT_ASX) {
                m_playlistFormat = FORMAT_ASX;
                // AUDIO_LOG_INFO("set playlist format to ASX");
            }
            if(m_expectedPlsFmt == FORMAT_M3U) {
                m_playlistFormat = FORMAT_M3U;
                // AUDIO_LOG_INFO("set playlist format to M3U");
            }
            if(m_expectedPlsFmt == FORMAT_M3U8) {
                m_playlistFormat = FORMAT_M3U8;
                // AUDIO_LOG_INFO("set playlist format to M3U8");
            }
            if(m_expectedPlsFmt == FORMAT_PLS) {
                m_playlistFormat = FORMAT_PLS;
                // AUDIO_LOG_INFO("set playlist format to PLS");
            }
            break;
        default:
            AUDIO_INFO("%s, unsupported audio format", ct);
            return false;
            break;
    }
    return true;
}
// clang-format on
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::showstreamtitle(char* ml) {
    // example for ml:
    // StreamTitle='Oliver Frank - Mega Hitmix';StreamUrl='www.radio-welle-woerthersee.at';
    // or adw_ad='true';durationMilliseconds='10135';adId='34254';insertionType='preroll';
    // html: 'Bielszy odcie&#324; bluesa 682 cz.1' --> 'Bielszy odcień bluesa 682 cz.1'

    if(!ml) return;
    ps_ptr<char>title = {};
    ps_ptr<char>artist = {};
    ps_ptr<char>streamTitle = {};
    ps_ptr<char>sUrl = {};

    htmlToUTF8(ml); // convert to UTF-8

    int16_t  idx1 = 0, idx2, idx4, idx5, idx6, idx7, titleLen = 0, artistLen = 0, titleStart = 0, artistStart = 0;

    // if(idx1 < 0) idx1 = indexOf(ml, "Title:", 0); // Title found (e.g. https://stream-hls.bauermedia.pt/comercial.aac/playlist.m3u8)
    // if(idx1 < 0) idx1 = indexOf(ml, "title:", 0); // Title found (e.g. #EXTINF:10,title="The Dan Patrick Show (M-F 9a-12p ET)",artist="zc1401"

    if(indexOf(ml, "StreamTitle='<?xml version=", 0) == 0) {
        /* e.g. xmlStreamTitle
              StreamTitle='<?xml version="1.0" encoding="utf-8"?><RadioInfo><Table><DB_ALBUM_ID>37364</DB_ALBUM_ID>
              <DB_ALBUM_IMAGE>00000037364.jpg</DB_ALBUM_IMAGE><DB_ALBUM_NAME>Boyfriend</DB_ALBUM_NAME>
              <DB_ALBUM_TYPE>Single</DB_ALBUM_TYPE><DB_DALET_ARTIST_NAME>DOVE CAMERON</DB_DALET_ARTIST_NAME>
              <DB_DALET_ITEM_CODE>CD4161</DB_DALET_ITEM_CODE><DB_DALET_TITLE_NAME>BOYFRIEND</DB_DALET_TITLE_NAME>
              <DB_FK_SITE_ID>2</DB_FK_SITE_ID><DB_IS_MUSIC>1</DB_IS_MUSIC><DB_LEAD_ARTIST_ID>26303</DB_LEAD_ARTIST_ID>
              <DB_LEAD_ARTIST_NAME>Dove Cameron</DB_LEAD_ARTIST_NAME><DB_RADIO_IMAGE>cidadefm.jpg</DB_RADIO_IMAGE>
              <DB_RADIO_NAME>Cidade</DB_RADIO_NAME><DB_SONG_ID>120126</DB_SONG_ID><DB_SONG_LYRIC>60981</DB_SONG_LYRIC>
              <DB_SONG_NAME>Boyfriend</DB_SONG_NAME></Table><AnimadorInfo><TITLE>Cidade</TITLE>
              <START_TIME_UTC>2022-11-15T22:00:00+00:00</START_TIME_UTC><END_TIME_UTC>2022-11-16T06:59:59+00:00
              </END_TIME_UTC><SHOW_NAME>Cidade</SHOW_NAME><SHOW_HOURS>22h às 07h</SHOW_HOURS><SHOW_PANEL>0</SHOW_PANEL>
              </AnimadorInfo></RadioInfo>';StreamUrl='';
        */
        idx4 = indexOf(ml, "<DB_DALET_TITLE_NAME>");
        idx5 = indexOf(ml, "</DB_DALET_TITLE_NAME>");
        idx6 = indexOf(ml, "<DB_LEAD_ARTIST_NAME>");
        idx7 = indexOf(ml, "</DB_LEAD_ARTIST_NAME>");
        if(idx4 == -1 || idx5 == -1) return;
        titleStart = idx4 + 21; // <DB_DALET_TITLE_NAME>
        titleLen = idx5 - titleStart;

        if(idx6 != -1 && idx7 != -1) {
            artistStart = idx6 + 21; // <DB_LEAD_ARTIST_NAME>
            artistLen = idx7 - artistStart;
        }
        if(titleLen) title.assign(ml + titleStart, titleLen);
        if(artistLen) artist.assign(ml + artistStart, artistLen);

        if(title.valid() && artist.valid()){
            streamTitle.assign(title.get());
            streamTitle.append(" - ");
            streamTitle.append(artist.get());
        }
        else if(title.valid()){
            streamTitle.assign(title.get());
        }
        else if(artist.valid()){
            streamTitle.assign(artist.get());
        }
    }

    else if(indexOf(ml, "StreamTitle='") == 0){
        titleStart = 13;
        idx2 = indexOf(ml, ";", 12);
        if(idx2 > titleStart + 1){
            titleLen = idx2 - 1 - titleStart;
            streamTitle.assign(ml + 13, titleLen);
        }
    }

    else if(startsWith(ml, "#EXTINF")){
        // extraxt StreamTitle from m3u #EXTINF line to icy-format
        // orig: #EXTINF:10,title="text="TitleName",artist="ArtistName"
        // conv: StreamTitle=TitleName - ArtistName
        // orig: #EXTINF:10,title="text=\"Spot Block End\" amgTrackId=\"9876543\"",artist=" ",url="length=\"00:00:00\""
        // conv: StreamTitle=text=\"Spot Block End\" amgTrackId=\"9876543\" -

        idx1 = indexOf(ml, "title=\"");
        idx2 = indexOf(ml, "artist=\"");
        if(idx1 > 0){
            int titleStart = idx1 + 7;
            int idx3 = indexOf(ml, "\"", titleStart);
            if(idx3 > titleStart){
                int titleLength = idx3 - titleStart;
                title.assign(ml + titleStart, titleLength);
                 if(title.starts_with("text=\\")){ // #EXTINF:10,title="text=\"Spot Block End\"",artist=" ",
                    titleStart += 7;
                    idx3 = indexOf(ml, "\\", titleStart);
                    if(idx3 > titleStart){
                        int titleLength = idx3 - titleStart;
                        title.assign(ml + titleStart, titleLength);
                    }
                }
            }
        }
        if(idx2 > 0){
            int artistStart = idx2 + 8;
            int idx3 = indexOf(ml, "\"", artistStart);
            if(idx3 > artistStart){
                int artistLength = idx3 - artistStart;
                artist.assign(ml + artistStart, artistLength);
                if(strcmp(artist.get(), " ") == 0) artist.reset();
            }
        }
        if(title.valid() && artist.valid()){
            streamTitle.assign(title.get());
            streamTitle.append(" - ");
            streamTitle.append(artist.get());
        }
        else if(title.valid()){
            streamTitle.assign(title.get());
        }
        else if(artist.valid()){
            streamTitle.assign(artist.get());
        }
    }
    else{
        ;
    }

    if(indexOf(ml, "StreamUrl=", 0) > 0){
        idx1 = indexOf(ml, "StreamUrl=", 0);
        idx2 = indexOf(ml, ";", idx1);
        if(idx1 >= 0 && idx2 > idx1) { // StreamURL found
            uint16_t len = idx2 - idx1;
            sUrl.assign(ml + idx1, len);
            if(sUrl.valid()){
                AUDIO_INFO("%s", sUrl.get());
            }
        }
    }

    if(indexOf(ml, "adw_ad=", 0) > 0){
        idx1 = indexOf(ml, "adw_ad=", 0);
        if(idx1 >= 0) { // Advertisement found
            idx1 = indexOf(ml, "durationMilliseconds=", 0);
            idx2 = indexOf(ml, ";", idx1);
            if(idx1 >= 0 && idx2 > idx1) {
                uint16_t len = idx2 - idx1;
                ps_ptr<char> sAdv;
                sAdv.assign(ml + idx1, len + 1);
                AUDIO_INFO("%s", sAdv.get());
                uint8_t pos = 21;                                                 // remove "StreamTitle="
                if(sAdv[pos] == '\'') pos++;                                      // remove leading  \'
                if(sAdv[strlen(sAdv.get()) - 1] == '\'') sAdv[strlen(sAdv.get()) - 1] = '\0'; // remove trailing \'
                if(audio_commercial) audio_commercial(sAdv.get() + pos);
            }
        }
        return;
    }
    if(!streamTitle.valid()) return;

    if(!m_streamTitle.equals(streamTitle)){
        m_streamTitle.clone_from(streamTitle);
    }
    else{
        return; // is equal
    }

    if(m_streamTitle.starts_with("{\"")){ // maybe a json string
        // "{\"status\":0,\"error\":{\"info\":\"\\u042d\\u0442\\u043e \\u0440\\u0435\\u043a\\u043b\\u0430\\u043c\\u0430 \\u0438\\u043b\\u0438 \\u0434\\u0436\\u0438\\u043d\\u0433\\u043b\",\"code\":201},\"result\":\"\\u042d\\u0442\\u043e \\u0440\\u0435\\u043a\\u043b\\u0430\\u043c\\u0430 \\u0438\\u043b\\u0438 \\u0434\\u0436\\u0438\\u043d\\u0433\\u043b\"}\n0";
        ps_ptr<char> jsonIn;
        jsonIn.clone_from(m_streamTitle);
        jsonIn.truncate_at('\n'); // can be '{"status":1,"message":"Ok","result":"Ok","errorCode":0}\n0'
        if(!jsonIn.isJson()) return;
        int idx = jsonIn.index_of_icase("\"result\"");
        if(idx < 0) return;
        jsonIn.remove_before(idx + 10); // remove "result":
        jsonIn.truncate_at('"'); // remove after '"'    Ok","result":"Ok","errorCode":0} --> OK
        // AUDIO_LOG_INFO("jsonIn: %s", jsonIn.c_get());
        m_streamTitle.unicodeToUTF8(jsonIn.c_get());
    }

    if(audio_showstreamtitle) audio_showstreamtitle(m_streamTitle.c_get());
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::showCodecParams() {
    // print Codec Parameter (mp3, aac) in audio_info()

    AUDIO_INFO("Channels: %u", getChannels());
    AUDIO_INFO("SampleRate: %lu", getSampleRate());
    AUDIO_INFO("BitsPerSample: %u", getBitsPerSample());
    if(getBitRate()) { AUDIO_INFO("BitRate: %lu", getBitRate()); }
    else { AUDIO_INFO("BitRate: N/A"); }

    if(m_codec == CODEC_AAC) {
        uint8_t answ = AACGetFormat();
        if(answ < 3) {
            const char hf[4][8] = {"unknown", "ADIF", "ADTS"};
            AUDIO_INFO("AAC HeaderFormat: %s", hf[answ]);
        }
        answ = AACGetSBR();
        if(answ > 0 && answ < 4) {
            const char sbr[4][50] = {"without SBR", "upsampled SBR", "downsampled SBR", "no SBR used, but file is upsampled by a factor 2"};
            AUDIO_INFO("Spectral band replication: %s", sbr[answ]);
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Audio::findNextSync(uint8_t* data, size_t len) {
    // Mp3 and aac audio data are divided into frames. At the beginning of each frame there is a sync word.
    // The sync word is 0xFFF. This is followed by information about the structure of the frame.
    // Wav files have no frames
    // Return: 0 the synchronous word was found at position 0
    //         > 0 is the offset to the next sync word
    //         -1 the sync word was not found within the block with the length len

    m_fnsy.nextSync = 0;

    if(m_codec == CODEC_WAV) {
        m_f_playing = true;
        m_fnsy.nextSync = 0;
    }
    if(m_codec == CODEC_MP3) {
        m_fnsy.nextSync = MP3FindSyncWord(data, len);
        if(m_fnsy.nextSync == -1) return len; // syncword not found, search next block
        MP3Decoder_ClearBuffer();
    }
    if(m_codec == CODEC_AAC) { m_fnsy.nextSync = AACFindSyncWord(data, len); }
    if(m_codec == CODEC_M4A) {
        if(!m_M4A_chConfig)m_M4A_chConfig = 2; // guard
        if(!m_M4A_sampleRate)m_M4A_sampleRate = 44100;
        if(!m_M4A_objectType)m_M4A_objectType = 2;
        AACSetRawBlockParams(m_M4A_chConfig, m_M4A_sampleRate, m_M4A_objectType);
        m_f_playing = true;
        m_fnsy.nextSync = 0;
    }
    if(m_codec == CODEC_FLAC) {
        m_fnsy.nextSync = FLACFindSyncWord(data, len);
        if(m_fnsy.nextSync == -1) return len; // OggS not found, search next block
    }
    if(m_codec == CODEC_OPUS) {
        m_fnsy.nextSync = OPUSFindSyncWord(data, len);
        if(m_fnsy.nextSync == -1) return len; // OggS not found, search next block
    }
    if(m_codec == CODEC_VORBIS) {
        m_fnsy.nextSync = VORBISFindSyncWord(data, len);
        if(m_fnsy.nextSync == -1) return len; // OggS not found, search next block
    }
    if(m_fnsy.nextSync == -1) {
        if(m_fnsy.swnf == 0) AUDIO_INFO("syncword not found");
        else {
            m_fnsy.swnf++; // syncword not found counter, can be multimediadata
        }
    }
    if(m_fnsy.nextSync == 0) {
        if(m_fnsy.swnf){
            AUDIO_INFO("syncword not found %lu times", (long unsigned int)m_fnsy.swnf);
            m_fnsy.swnf = 0;
        }
        else {
            AUDIO_INFO("syncword found at pos 0");
            m_f_decode_ready = true;
        }
    }
    if(m_fnsy.nextSync > 0) { AUDIO_INFO("syncword found at pos %i", m_fnsy.nextSync); }
    return m_fnsy.nextSync;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setDecoderItems() {
    if(m_codec == CODEC_MP3) {
        setChannels(MP3GetChannels());
        setSampleRate(MP3GetSampRate());
        setBitsPerSample(MP3GetBitsPerSample());
        setBitrate(MP3GetBitrate());
        AUDIO_INFO("%s %s", MP3GetMPEGVersion(), MP3GetLayer());
    }
    if(m_codec == CODEC_AAC || m_codec == CODEC_M4A) {
        setChannels(AACGetChannels());
        setSampleRate(AACGetSampRate());
        setBitsPerSample(AACGetBitsPerSample());
        setBitrate(AACGetBitrate());
    }
    if(m_codec == CODEC_FLAC) {
        setChannels(FLACGetChannels());
        setSampleRate(FLACGetSampRate());
        setBitsPerSample(FLACGetBitsPerSample());
        setBitrate(FLACGetBitRate());
        if(FLACGetAudioDataStart() > 0){ // only flac-ogg, native flac sets audioDataStart in readFlacHeader()
            m_audioDataStart = FLACGetAudioDataStart();
            if(m_audioFileSize) m_audioDataSize = m_audioFileSize - m_audioDataStart;
        }
    }
    if(m_codec == CODEC_OPUS) {
        setChannels(OPUSGetChannels());
        setSampleRate(OPUSGetSampRate());
        setBitsPerSample(OPUSGetBitsPerSample());
        setBitrate(OPUSGetBitRate());
        if(OPUSGetAudioDataStart() > 0){
            m_audioDataStart = OPUSGetAudioDataStart();
            if(m_audioFileSize) m_audioDataSize = m_audioFileSize - m_audioDataStart;
        }
    }
    if(m_codec == CODEC_VORBIS) {
        setChannels(VORBISGetChannels());
        setSampleRate(VORBISGetSampRate());
        setBitsPerSample(VORBISGetBitsPerSample());
        setBitrate(VORBISGetBitRate());
        if(VORBISGetAudioDataStart() > 0){
            m_audioDataStart = VORBISGetAudioDataStart();
            if(m_audioFileSize) m_audioDataSize = m_audioFileSize - m_audioDataStart;
        }
    }
    if(getBitsPerSample() != 8 && getBitsPerSample() != 16) {
        AUDIO_INFO("Bits per sample must be 8 or 16, found %i", getBitsPerSample());
        stopSong();
    }
    if(getChannels() != 1 && getChannels() != 2) {
        AUDIO_INFO("Num of channels must be 1 or 2, found %i", getChannels());
        stopSong();
    }
    memset(m_filterBuff, 0, sizeof(m_filterBuff)); // Clear FilterBuffer
    IIR_calculateCoefficients(m_gain0, m_gain1, m_gain2); // must be recalculated after each samplerate change
    showCodecParams();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::decodeError(int8_t res, uint8_t* data, int32_t bytesDecoded){
        // for(int i = 0; i < 10; i++){printf("0x%02X ", data[i]);} printf("\n");
        if(res == -100){stopSong(); return bytesDecoded;} // serious error, e.g. decoder could not be initialized
        if(m_codec == CODEC_AAC && res == -21){ // mono <-> stereo change
            //  According to the specification, the channel configuration is transferred in the first ADTS header and no longer changes in the entire
            //  stream. Some streams send short mono blocks in a stereo stream. e.g. http://mp3.ffh.de/ffhchannels/soundtrack.aac
            //  This triggers error -21 because the faad2 decoder cannot switch automatically.
            m_sbyt.channels = 0;
            if ((data[0] == 0xFF) || ((data[1] & 0xF0) == 0xF0)){
                int channel_config = ((data[2] & 0x01) << 2) | ((data[3] & 0xC0) >> 6);
                if(channel_config != m_sbyt.channels) {
                    m_sbyt.channels = channel_config;
                    AUDIO_INFO("AAC channel config changed to %d", m_sbyt.channels);
                }
            }
            AACDecoder_FreeBuffers();
            AACDecoder_AllocateBuffers();
            return 0;
        }
        m_f_playing = false; // seek for new syncword
        if(bytesDecoded == 0) return 1; // skip one byte and seek for the next sync word
        return bytesDecoded;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::decodeContinue(int8_t res, uint8_t* data, int32_t bytesDecoded){
    // if(m_codec == CODEC_MP3){   if(res == MAD_ERROR_CONTINUE)    return bytesDecoded;} // nothing to play, mybe eof
    if(m_codec == CODEC_FLAC){  if(res == FLAC_PARSE_OGG_DONE)   return bytesDecoded;} // nothing to play
    if(m_codec == CODEC_OPUS){  if(res == OPUS_PARSE_OGG_DONE)   return bytesDecoded;  // nothing to play
                                if(res == OPUS_END)              return bytesDecoded;} // nothing to play
    if(m_codec == CODEC_VORBIS){if(res == VORBIS_PARSE_OGG_DONE) return bytesDecoded;} // nothing to play
    return 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int Audio::sendBytes(uint8_t* data, size_t len) {
    if(!m_f_running) return 0; // guard
    m_sbyt.bytesLeft = 0;
    m_sbyt.nextSync = 0;

    if(!m_f_playing) {
        m_sbyt.channels = 2; // assume aac stereo
        m_sbyt.isPS = 0;
        m_sbyt.f_setDecodeParamsOnce = true;
        m_sbyt.nextSync = findNextSync(data, len);
        if(m_sbyt.nextSync <  0) return len; // no syncword found
        if(m_sbyt.nextSync == 0) { m_f_playing = true; }
        if(m_sbyt.nextSync >  0) return m_sbyt.nextSync;
    }
    // m_f_playing is true at this pos
    int res = 0;
    m_sbyt.bytesLeft = len;
    int bytesDecoded = 0;

    if(m_codec == CODEC_NONE && m_playlistFormat == FORMAT_M3U8) return 0; // can happen when the m3u8 playlist is loaded
    if(!m_f_decode_ready) return 0; // find sync first

    switch(m_codec) {
        case CODEC_WAV:    res = 0; m_sbyt.bytesLeft = 0; break;
        case CODEC_MP3:    res = MP3Decode(   data, &m_sbyt.bytesLeft, m_outBuff.get()); break;
        case CODEC_AAC:    res = AACDecode(   data, &m_sbyt.bytesLeft, m_outBuff.get()); break;
        case CODEC_M4A:    res = AACDecode(   data, &m_sbyt.bytesLeft, m_outBuff.get()); break;
        case CODEC_FLAC:   res = FLACDecode(  data, &m_sbyt.bytesLeft, m_outBuff.get()); break;
        case CODEC_OPUS:   res = OPUSDecode(  data, &m_sbyt.bytesLeft, m_outBuff.get()); break;
        case CODEC_VORBIS: res = VORBISDecode(data, &m_sbyt.bytesLeft, m_outBuff.get()); break;
        default: {
            AUDIO_LOG_ERROR("no valid codec found codec = %d", m_codec);
            stopSong();
        }
    }
    bytesDecoded = len - m_sbyt.bytesLeft;

    // res - possible values are:
    //          0: okay, no error
    //        >= 100: the decoder needs more data
    //        < 0: there has been an error
    //       -100: serious error, stop song

    if(res <  0){ return decodeError(res, data, bytesDecoded);} // Error, skip the frame...
    if(res > 99){ return decodeContinue(res, data, bytesDecoded);} // decoder needs more data...

    if(bytesDecoded == 0 && res == 0) { // unlikely framesize
        AUDIO_INFO("framesize is 0, start decoding again");
        m_f_playing = false; // seek for new syncword
        // we're here because there was a wrong sync word so skip one byte and seek for the next
        return 1;
    }
    // status: bytesDecoded > 0 and res >= 0
    const char* st = NULL;
    std::vector<uint32_t> vec;
    switch(m_codec) {
        case CODEC_WAV:     if(getBitsPerSample() == 16){
                                memmove(m_outBuff.get(), data, len); // copy len data in outbuff and set validsamples and bytesdecoded=len
                                m_validSamples = len / (2 * getChannels());
                            }
                            else{
                                for(int i = 0; i < len; i++) {
                                    int16_t sample1 = (data[i] & 0x00FF)      - 128;
                                    int16_t sample2 = (data[i] & 0xFF00 >> 8) - 128;
                                    m_outBuff[i * 2 + 0] = sample1 << 8;
                                    m_outBuff[i * 2 + 1] = sample2 << 8;
                                }
                                m_validSamples = len;
                            }
                            break;
        case CODEC_MP3:     m_validSamples = MP3GetOutputSamps();
                            break;
        case CODEC_AAC:     m_validSamples = AACGetOutputSamps() / getChannels();
                            if(!m_sbyt.isPS && AACGetParametricStereo()){ // only change 0 -> 1
                                m_sbyt.isPS = 1;
                                AUDIO_INFO("Parametric Stereo");
                            }
                            else m_sbyt.isPS = AACGetParametricStereo();
                            break;
        case CODEC_M4A:     m_validSamples = AACGetOutputSamps() / getChannels();
                            break;
        case CODEC_FLAC:    m_validSamples = FLACGetOutputSamps() / getChannels();
                            st = FLACgetStreamTitle();
                            if(st) {
                                AUDIO_INFO(st);
                                if(audio_showstreamtitle) audio_showstreamtitle(st);
                            }
                            vec = FLACgetMetadataBlockPicture();
                            if(vec.size() > 0){ // get blockpic data
                                // AUDIO_LOG_INFO("---------------------------------------------------------------------------");
                                // AUDIO_LOG_INFO("ogg metadata blockpicture found:");
                                // for(int i = 0; i < vec.size(); i += 2) { AUDIO_LOG_INFO("segment %02i, pos %07i, len %05i", i / 2, vec[i], vec[i + 1]); }
                                // AUDIO_LOG_INFO("---------------------------------------------------------------------------");
                                if(audio_oggimage) audio_oggimage(m_audiofile, vec);
                            }
                            break;
        case CODEC_OPUS:    m_validSamples = OPUSGetOutputSamps();
                            st = OPUSgetStreamTitle();
                            if(st){
                                AUDIO_INFO(st);
                                if(audio_showstreamtitle) audio_showstreamtitle(st);
                            }
                            vec = OPUSgetMetadataBlockPicture();
                            if(vec.size() > 0){ // get blockpic data
                                // AUDIO_LOG_INFO("---------------------------------------------------------------------------");
                                // AUDIO_LOG_INFO("ogg metadata blockpicture found:");
                                // for(int i = 0; i < vec.size(); i += 2) { AUDIO_LOG_INFO("segment %02i, pos %07i, len %05i", i / 2, vec[i], vec[i + 1]); }
                                // AUDIO_LOG_INFO("---------------------------------------------------------------------------");
                                if(audio_oggimage) audio_oggimage(m_audiofile, vec);
                            }

                            if(m_opus_mode != OPUSgetMode()){
                                m_opus_mode = OPUSgetMode();
                                if(m_opus_mode == MODE_CELT_ONLY) AUDIO_INFO("Opus Mode: CELT_ONLY");
                                if(m_opus_mode == MODE_HYBRID)    AUDIO_INFO("Opus Mode: HYBRID");
                                if(m_opus_mode == MODE_SILK_ONLY) AUDIO_INFO("Opus Mode: SILK_ONLY");
                                if(m_opus_mode == MODE_NONE)      AUDIO_INFO("Opus Mode: NONE");
                            }
                            break;

        case CODEC_VORBIS:  m_validSamples = VORBISGetOutputSamps();
                            st = VORBISgetStreamTitle();
                            if(st) {
                                AUDIO_INFO(st);
                                if(audio_showstreamtitle) audio_showstreamtitle(st);
                            }
                            vec = VORBISgetMetadataBlockPicture();
                            if(vec.size() > 0){ // get blockpic data
                                // AUDIO_LOG_INFO("---------------------------------------------------------------------------");
                                // AUDIO_LOG_INFO("ogg metadata blockpicture found:");
                                // for(int i = 0; i < vec.size(); i += 2) { AUDIO_LOG_INFO("segment %02i, pos %07i, len %05i", i / 2, vec[i], vec[i + 1]); }
                                // AUDIO_LOG_INFO("---------------------------------------------------------------------------");
                                if(audio_oggimage) audio_oggimage(m_audiofile, vec);
                            }
                            break;
    }
    if(m_sbyt.f_setDecodeParamsOnce && m_validSamples) {
        m_sbyt.f_setDecodeParamsOnce = false;
        setDecoderItems();
        m_PlayingStartTime = millis();
    }

    uint16_t bytesDecoderOut = m_validSamples;
    if(m_channels == 2) bytesDecoderOut /= 2;
    if(m_bitsPerSample == 16) bytesDecoderOut *= 2;
    computeAudioTime(bytesDecoded, bytesDecoderOut);

    m_curSample = 0;
    playChunk();
    return bytesDecoded;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::computeAudioTime(uint16_t bytesDecoderIn, uint16_t bytesDecoderOut) {

    if(m_dataMode != AUDIO_LOCALFILE && m_streamType != ST_WEBFILE) return; //guard

    float audioCurrentTime;

    if(m_f_firstCurTimeCall) { // first call
        m_f_firstCurTimeCall = false;
        m_cat.sumBytesIn = 0;
        m_cat.sumBytesOut = 0;
        m_cat.sumBitRate  = 0;
        m_cat.counter = 0;
        m_cat.timeStamp = millis();
        m_cat.deltaBytesIn = 0;
        m_cat.nominalBitRate = 0;
        m_cat.syltIdx = 0;

        if(m_codec == CODEC_FLAC && FLACGetAudioFileDuration()){
            m_audioFileDuration = FLACGetAudioFileDuration();
            m_cat.nominalBitRate = (m_audioDataSize / FLACGetAudioFileDuration()) * 8;
            m_avr_bitrate = m_cat.nominalBitRate;
        }
        if(m_codec == CODEC_WAV){
            m_cat.nominalBitRate = getBitRate();
            m_avr_bitrate = m_cat.nominalBitRate;
            m_audioFileDuration = m_audioDataSize  / (getSampleRate() * getChannels());
            if(getBitsPerSample() == 16) m_audioFileDuration /= 2;
        }
        if(m_codec == CODEC_MP3){
            // if(MP3GetAudioFileDuration() > 0){ // XING header present?
            //     m_audioFileDuration = MP3GetAudioFileDuration();
            //     m_cat.nominalBitRate  = MP3GetBitrate();
            //     m_avr_bitrate = m_cat.nominalBitRate;
            // }
        }
    }

    m_cat.sumBytesIn   += bytesDecoderIn;
    m_cat.deltaBytesIn += bytesDecoderIn;
    m_cat.sumBytesOut  += bytesDecoderOut;


    if(m_cat.timeStamp + 50 < millis()){
        uint32_t t       = millis();      // time tracking
        uint32_t delta_t = t - m_cat.timeStamp; //    ---"---
        m_cat.timeStamp = t;                    //    ---"---

        uint32_t bitRate = ((m_cat.deltaBytesIn * 8000) / delta_t);  // we know the time and bytesIn to compute the bitrate

        m_cat.sumBitRate += bitRate;
        m_cat.counter ++;
        if(m_cat.nominalBitRate){
            audioCurrentTime = (float)m_cat.sumBytesIn * 8 / m_avr_bitrate;
        }
        else{
            m_avr_bitrate = m_cat.sumBitRate / m_cat.counter;
            audioCurrentTime = (float)m_cat.sumBytesIn * 8 / m_avr_bitrate;
            m_audioFileDuration = round(((float)m_audioDataSize * 8 / m_avr_bitrate));
        }
        m_cat.deltaBytesIn = 0;
        m_audioCurrentTime = round(audioCurrentTime);
    }

    if(m_haveNewFilePos && m_avr_bitrate){
        uint32_t posWhithinAudioBlock =  m_haveNewFilePos - m_audioDataStart;
        float newTime = (float)posWhithinAudioBlock / (m_avr_bitrate / 8);
        m_audioCurrentTime = round(newTime);
        m_cat.sumBytesIn = posWhithinAudioBlock;
        m_haveNewFilePos = 0;
        m_cat.syltIdx = 0;
        if(m_ID3Hdr.SYLT.seen){
            while(m_cat.syltIdx < m_syltLines.size()){
                if(newTime * 1000 < m_syltTimeStamp[m_cat.syltIdx]) break;
                m_cat.syltIdx++;
            }
        }
    }

    if(m_ID3Hdr.SYLT.seen){
        //  AUDIO_LOG_INFO("%f", audioCurrentTime * 1000); // ms
        if(m_cat.syltIdx >= m_syltLines.size()) return;
        if(m_audioCurrentTime * 1000 > m_syltTimeStamp[m_cat.syltIdx]){
        //  AUDIO_INFO(ANSI_ESC_CYAN "%s", m_syltLines[m_cat.syltIdx].c_get());
            if(audio_id3lyrics) audio_id3lyrics(m_syltLines[m_cat.syltIdx].c_get());
            m_cat.syltIdx++;
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t MCLK) {

    m_f_psramFound = psramInit();

    m_outBuff.alloc(m_outbuffSize * sizeof(int16_t), "m_outbuff");
    m_samplesBuff48K.alloc(m_samplesBuff48KSize * sizeof(int16_t), "m_samplesBuff48K");

    esp_err_t result = ESP_OK;

#if(ESP_ARDUINO_VERSION_MAJOR < 3)
    AUDIO_LOG_ERROR("Arduino Version must be 3.0.0 or higher!");
#endif
    trim(audioI2SVers);
    AUDIO_INFO("audioI2S %s", audioI2SVers);

    i2s_std_gpio_config_t gpio_cfg = {};
    gpio_cfg.bclk = (gpio_num_t)BCLK;
    gpio_cfg.din = (gpio_num_t)I2S_GPIO_UNUSED;
    gpio_cfg.dout = (gpio_num_t)DOUT;
    gpio_cfg.mclk = (gpio_num_t)MCLK;
    gpio_cfg.ws = (gpio_num_t)LRC;
    I2Sstop();
    result = i2s_channel_reconfig_std_gpio(m_i2s_tx_handle, &gpio_cfg);
    I2Sstart();

    return (result == ESP_OK);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFileSize() { // returns the size of webfile or local file
    if(!m_audiofile) {
        if (m_audioFileSize > 0) { return m_audioFileSize;}
        return 0;
    }
    return m_audiofile.size();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioFileDuration() {
    if(!m_avr_bitrate)                                      return 0;
    if(m_playlistFormat == FORMAT_M3U8)                     return 0;
    if(!m_audioDataSize)                                    return 0;
    return m_audioFileDuration;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioCurrentTime() { // return current time in seconds
    return round(m_audioCurrentTime);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setAudioPlayPosition(uint16_t sec) {

    if((m_dataMode != AUDIO_LOCALFILE) && (m_streamType != ST_WEBFILE)) return false;  // guard
    if(!m_avr_bitrate) return false;  // guard

    // if(m_codec == CODEC_OPUS) return false;   // not impl. yet
    // if(m_codec == CODEC_VORBIS) return false; // not impl. yet
    // Jump to an absolute position in time within an audio file
    // e.g. setAudioPlayPosition(300) sets the pointer at pos 5 min
    if(sec > getAudioFileDuration()) sec = getAudioFileDuration();
    uint32_t filepos = m_audioDataStart + (m_avr_bitrate * sec / 8);
    if(m_dataMode == AUDIO_LOCALFILE) return setFilePos(filepos);
    // if(m_streamType == ST_WEBFILE) return httpRange(m_lastHost, filepos);
    return false;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setVolumeSteps(uint8_t steps) {
    m_vol_steps = steps;
    if(steps < 1) m_vol_steps = 64; /* avoid div-by-zero :-) */
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t Audio::maxVolume() { return m_vol_steps; };
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getTotalPlayingTime() {
    // Is set to zero by a connectToXXX() and starts as soon as the first audio data is available,
    // the time counting is not interrupted by a 'pause / resume'
    return millis() - m_PlayingStartTime;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setTimeOffset(int sec) { // fast forward or rewind the current position in seconds
    // AUDIO_INFO("time offset %li sec", sec);
    if((m_dataMode != AUDIO_LOCALFILE) && (m_streamType != ST_WEBFILE)){ AUDIO_LOG_WARN("%s","not a file");                 return false;}  // guard
    if((m_dataMode == AUDIO_LOCALFILE) && !m_audiofile){                 AUDIO_LOG_WARN("%s","local file not accessibble"); return false;}  // guard
    if((m_streamType == ST_WEBFILE) && !m_f_acceptRanges){               AUDIO_LOG_WARN("%s","server don't accept ranges"); return false;}  // guard
    if(!m_avr_bitrate){                                                  AUDIO_LOG_WARN("%s","unknown bitrate");            return false;}  // guard
    if(!m_f_stream){                                                     AUDIO_LOG_WARN("%s","strean is not ready");        return false;}  // guard
    if(m_codec == CODEC_AAC){                                            AUDIO_LOG_WARN("%s","aac not impl yet");           return false;}
    uint32_t oneSec = m_avr_bitrate / 8;                 // bytes decoded in one sec
    int32_t  offset = oneSec * sec;                      // bytes to be wind/rewind
    int32_t pos = m_audioFilePosition - inBufferFilled();
    pos += offset;
    setFilePos(pos);
    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t Audio::audioFileRead(uint8_t* buff, size_t len){
    if(buff && len == 0) return 0; // nothing to do
    // This method standardized reading files, regardless of the source (local or web) and the correct number of the bytes read must be determined.
    int32_t res = -1;

    if(m_dataMode == AUDIO_LOCALFILE){
        if(!buff && !len){
            res = m_audiofile.read();
            if(res >= 0) m_audioFilePosition ++;
        }
        if(buff){
            res = m_audiofile.read(buff, len);
            if(res >= 0) m_audioFilePosition += res;
        }
    }
    else{
        if(len > 16384) len = 16384; // limit
        if(!buff && !len){
            res = m_client->read();
            if(res >= 0) m_audioFilePosition ++;
        }
        if(buff){
            res = m_client->read(buff, len);
            if(res >= 0) m_audioFilePosition += res;
        }
    }
    return res;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t Audio::audioFileSeek(uint32_t position, size_t len){
    int32_t res = -1;

    if(m_dataMode == AUDIO_LOCALFILE){
        uint32_t actualPos = m_audiofile.position();
        if(actualPos != m_audioFilePosition){
            AUDIO_LOG_WARN("actualPos != m_audioFilePosition %lu != %lu", actualPos, m_audioFilePosition);
            m_audioFilePosition = actualPos;
        }
        if(!m_audiofile) return -1;
        if(position > m_audiofile.size()){
            AUDIO_LOG_WARN("position larger than size %lu > %lu", position, m_audiofile.size());
            position = m_audiofile.size();
        }
        bool r = m_audiofile.seek(position);
        m_audioFilePosition = m_audiofile.position();
        if(r == false){
            AUDIO_LOG_ERROR("something went wrong");
            return -1;
        }
        else{
            return position;
        }
    }
    else{
        if(m_f_acceptRanges){
            bool r;
            if(len == 0) len = UINT32_MAX;
            r = httpRange(position, len);
            if(res == false){AUDIO_LOG_ERROR("http range request was not successful"); return 0;}
            r = parseHttpRangeHeader();
            if(r == false){AUDIO_LOG_ERROR("http range response was not successful"); return 0;}
            m_audioFilePosition = position;
            return position;
        }
    }
    return res;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setFilePos(uint32_t pos) {

    if((m_dataMode != AUDIO_LOCALFILE) && (m_streamType != ST_WEBFILE)){ AUDIO_LOG_WARN("%s","not a file");                 return false;}  // guard
    if((m_dataMode == AUDIO_LOCALFILE) && !m_audiofile){                 AUDIO_LOG_WARN("%s","local file not accessibble"); return false;}  // guard
    if((m_streamType == ST_WEBFILE) && !m_f_acceptRanges){               AUDIO_LOG_WARN("%s","server don't accept ranges"); return false;}  // guard
    if(!m_avr_bitrate){                                                  AUDIO_LOG_WARN("%s","unknown bitrate");            return false;}  // guard
    if(!m_f_stream){                                                     AUDIO_LOG_WARN("%s","strean is not ready");        return false;}  // guard
    if(m_codec == CODEC_AAC){                                            AUDIO_LOG_WARN("%s","aac not impl yet");           return false;}

    uint32_t startAB = m_audioDataStart;                 // audioblock begin
    uint32_t endAB = m_audioDataStart + m_audioDataSize; // audioblock end
    if(pos < (int32_t)startAB) {pos = startAB;}
    if(pos >= (int32_t)endAB)  {pos = endAB;}

    m_validSamples = 0;
    m_resumeFilePos = pos;  // used in processLocalFile()
    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setSampleRate(uint32_t sampRate) {
    if(!sampRate) sampRate = 48000;
    if(sampRate < 8000 ) {
        AUDIO_INFO("Sample rate must not be smaller than 8kHz, found: %lu", sampRate);
        m_sampleRate = 8000;
    }
    m_sampleRate = sampRate;
    m_resampleRatio = (float)m_sampleRate / 48000.0f;

    m_i2s_std_cfg.clk_cfg.sample_rate_hz = m_sampleRate;
    i2s_channel_disable(m_i2s_tx_handle);
    i2s_channel_reconfig_std_clock(m_i2s_tx_handle, &m_i2s_std_cfg.clk_cfg);
    i2s_channel_enable(m_i2s_tx_handle);
    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getSampleRate() { return m_sampleRate; }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setBitsPerSample(int bits) {
    if((bits != 16) && (bits != 8)) return false;
    m_bitsPerSample = bits;
    return true;
}
uint8_t Audio::getBitsPerSample() { return m_bitsPerSample; }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setChannels(int ch) {
    m_channels = ch;
    return true;
}
uint8_t Audio::getChannels() {
    if(m_channels == 0) { // this should not happen! #209
        m_channels = 2;
    }
    return m_channels;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setBitrate(int br) {
    m_bitRate = br;
    if(br) return true;
    return false;
}
uint32_t Audio::getBitRate(bool avg) {
    if(avg) return m_avr_bitrate;
    return m_bitRate;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setI2SCommFMT_LSB(bool commFMT) {
    // false: I2S communication format is by default I2S_COMM_FORMAT_I2S_MSB, right->left (AC101, PCM5102A)
    // true:  changed to I2S_COMM_FORMAT_I2S_LSB for some DACs (PT8211)
    //        Japanese or called LSBJ (Least Significant Bit Justified) format

    m_f_commFMT = commFMT;

    i2s_channel_disable(m_i2s_tx_handle);
    if(commFMT) {
        AUDIO_INFO("commFMT = LSBJ (Least Significant Bit Justified)");
        m_i2s_std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    }
    else {
        AUDIO_INFO("commFMT = Philips");
        m_i2s_std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    }
    i2s_channel_reconfig_std_slot(m_i2s_tx_handle, &m_i2s_std_cfg.slot_cfg);
    i2s_channel_enable(m_i2s_tx_handle);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::computeVUlevel(int16_t sample[2]) {

    auto avg = [&](uint8_t* sampArr) { // lambda, inner function, compute the average of 8 samples
        uint16_t av = 0;
        for(int i = 0; i < 8; i++) { av += sampArr[i]; }
        return av >> 3;
    };

    auto largest = [&](uint8_t* sampArr) { // lambda, inner function, compute the largest of 8 samples
        uint16_t maxValue = 0;
        for(int i = 0; i < 8; i++) {
            if(maxValue < sampArr[i]) maxValue = sampArr[i];
        }
        return maxValue;
    };

    if(m_cVUl.cnt0 == 64) {
        m_cVUl.cnt0 = 0;
        m_cVUl.cnt1++;
    }
    if(m_cVUl.cnt1 == 8) {
        m_cVUl.cnt1 = 0;
        m_cVUl.cnt2++;
    }
    if(m_cVUl.cnt2 == 8) {
        m_cVUl.cnt2 = 0;
        m_cVUl.cnt3++;
    }
    if(m_cVUl.cnt3 == 8) {
        m_cVUl.cnt3 = 0;
        m_cVUl.cnt4++;
        m_cVUl.f_vu = true;
    }
    if(m_cVUl.cnt4 == 8) { m_cVUl.cnt4 = 0; }

    if(!m_cVUl.cnt0) { // store every 64th sample in the array[0]
        m_cVUl.sampleArray[LEFTCHANNEL][0][m_cVUl.cnt1] = abs(sample[LEFTCHANNEL] >> 7);
        m_cVUl.sampleArray[RIGHTCHANNEL][0][m_cVUl.cnt1] = abs(sample[RIGHTCHANNEL] >> 7);
    }
    if(!m_cVUl.cnt1) { // store argest from 64 * 8 samples in the array[1]
        m_cVUl.sampleArray[LEFTCHANNEL][1][m_cVUl.cnt2] = largest(m_cVUl.sampleArray[LEFTCHANNEL][0]);
        m_cVUl.sampleArray[RIGHTCHANNEL][1][m_cVUl.cnt2] = largest(m_cVUl.sampleArray[RIGHTCHANNEL][0]);
    }
    if(!m_cVUl.cnt2) { // store avg from 64 * 8 * 8 samples in the array[2]
        m_cVUl.sampleArray[LEFTCHANNEL][2][m_cVUl.cnt3] = largest(m_cVUl.sampleArray[LEFTCHANNEL][1]);
        m_cVUl.sampleArray[RIGHTCHANNEL][2][m_cVUl.cnt3] = largest(m_cVUl.sampleArray[RIGHTCHANNEL][1]);
    }
    if(!m_cVUl.cnt3) { // store avg from 64 * 8 * 8 * 8 samples in the array[3]
        m_cVUl.sampleArray[LEFTCHANNEL][3][m_cVUl.cnt4] = avg(m_cVUl.sampleArray[LEFTCHANNEL][2]);
        m_cVUl.sampleArray[RIGHTCHANNEL][3][m_cVUl.cnt4] = avg(m_cVUl.sampleArray[RIGHTCHANNEL][2]);
    }
    if(m_cVUl.f_vu) {
        m_cVUl.f_vu = false;
        m_vuLeft = avg(m_cVUl.sampleArray[LEFTCHANNEL][3]);
        m_vuRight = avg(m_cVUl.sampleArray[RIGHTCHANNEL][3]);
    }
    m_cVUl.cnt1++;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint16_t Audio::getVUlevel() {
    // avg 0 ... 127
    if(!m_f_running) return 0;
    return (m_vuLeft << 8) + m_vuRight;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setTone(int8_t gainLowPass, int8_t gainBandPass, int8_t gainHighPass) {
    // see https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
    // values can be between -40 ... +6 (dB)

    m_gain0 = gainLowPass;
    m_gain1 = gainBandPass;
    m_gain2 = gainHighPass;

    // gain, attenuation (set in digital filters)
    int db = max(m_gain0, max(m_gain1, m_gain2));
    m_corr = pow10f((float)db / 20);

    IIR_calculateCoefficients(m_gain0, m_gain1, m_gain2);

    /*
          This will cause a clicking sound when adjusting the EQ.
          Because when the EQ is adjusted, the IIR filter will be cleared and played,
          mixed in the audio data frame, and a click-like sound will be produced.

          int16_t tmp[2]; tmp[0] = 0; tmp[1]= 0;

          IIR_filterChain0(tmp, true ); // flush the filter
          IIR_filterChain1(tmp, true ); // flush the filter
          IIR_filterChain2(tmp, true ); // flush the filter
        */
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::forceMono(bool m) { // #100 mono option
    m_f_forceMono = m;          // false stereo, true mono
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setBalance(int8_t bal) { // bal -16...16
    if(bal < -16) bal = -16;
    if(bal > 16) bal = 16;
    m_balance = bal;

    computeLimit();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::setVolume(uint8_t vol, uint8_t curve) { // curve 0: default, curve 1: flat at the beginning

    if(vol > m_vol_steps) m_vol = m_vol_steps;
    else m_vol = vol;

    if(curve > 1) m_curve = 1;
    else m_curve = curve;

    computeLimit();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t Audio::getVolume() { return m_vol; }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t Audio::getI2sPort() { return m_i2s_num; }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::computeLimit() {    // is calculated when the volume or balance changes
    double l = 1, r = 1, v = 1; // assume 100%

    /* balance is left -16...+16 right */
    /* TODO: logarithmic scaling of balance, too? */
    if(m_balance > 0) { r -= (double)abs(m_balance) / 16; }
    else if(m_balance < 0) { l -= (double)abs(m_balance) / 16; }

    switch(m_curve) {
        case 0:
            v = (double)pow(m_vol, 2) / pow(m_vol_steps, 2); // square (default)
            break;
        case 1: // logarithmic
            double log1 = log(1);
            if(m_vol > 0) { v = m_vol * ((std::exp(log1 + (m_vol - 1) * (std::log(m_vol_steps) - log1) / (m_vol_steps - 1))) / m_vol_steps) / m_vol_steps; }
            else { v = 0; }
            break;
    }

    m_limit_left = l * v;
    m_limit_right = r * v;

    // AUDIO_LOG_INFO("m_limit_left %f,  m_limit_right %f ",m_limit_left, m_limit_right);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::Gain(int16_t* sample) {
    /* important: these multiplications must all be signed ints, or the result will be invalid */
    sample[LEFTCHANNEL]  *= m_limit_left ;
    sample[RIGHTCHANNEL] *= m_limit_right;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::inBufferFilled() {
    // current audio input buffer fillsize in bytes
    return InBuff.bufferFilled();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::inBufferFree() {
    // current audio input buffer free space in bytes
    return InBuff.freeSpace();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getInBufferSize() {
    // current audio input buffer size in bytes
    return InBuff.getBufsize();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::setInBufferSize(size_t mbs){
    size_t oldBuffSize = InBuff.getBufsize();
    stopSong();
    bool res = InBuff.setBufsize(mbs);
    if(res == false){
        AUDIO_LOG_ERROR("%i bytes is not possible, back to %i bytes", mbs, oldBuffSize);
        InBuff.setBufsize(oldBuffSize);
    }
    InBuff.init();
    return res;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//            ***     D i g i t a l   b i q u a d r a t i c     f i l t e r     ***
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::IIR_calculateCoefficients(int8_t G0, int8_t G1, int8_t G2) { // Infinite Impulse Response (IIR) filters

    // G1 - gain low shelf   set between -40 ... +6 dB
    // G2 - gain peakEQ      set between -40 ... +6 dB
    // G3 - gain high shelf  set between -40 ... +6 dB
    // https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/

    if(getSampleRate() < 1000) return; // fuse

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if(G0 < -40) G0 = -40; // -40dB -> Vin*0.01
    if(G0 > 6) G0 = 6;     // +6dB -> Vin*2
    if(G1 < -40) G1 = -40;
    if(G1 > 6) G1 = 6;
    if(G2 < -40) G2 = -40;
    if(G2 > 6) G2 = 6;

    const float FcLS = 500;    // Frequency LowShelf[Hz]
    const float FcPKEQ = 3000; // Frequency PeakEQ[Hz]
    float       FcHS = 6000;   // Frequency HighShelf[Hz]

    if(getSampleRate() < FcHS * 2 - 100) { // Prevent HighShelf filter from clogging
        FcHS = getSampleRate() / 2 - 100;
        // according to the sampling theorem, the sample rate must be at least 2 * 6000 >= 12000Hz for a filter
        // frequency of 6000Hz. If this is not the case, the filter frequency (plus a reserve of 100Hz) is lowered
        AUDIO_INFO("Highshelf frequency lowered, from 6000Hz to %luHz", (long unsigned int)FcHS);
    }
    float K, norm, Q, Fc, V;

    // LOWSHELF
    Fc = (float)FcLS / (float)getSampleRate(); // Cutoff frequency
    K = tanf((float)PI * Fc);
    V = powf(10, fabs(G0) / 20.0);

    if(G0 >= 0) { // boost
        norm = 1 / (1 + sqrtf(2) * K + K * K);
        m_filter[LOWSHELF].a0 = (1 + sqrtf(2 * V) * K + V * K * K) * norm;
        m_filter[LOWSHELF].a1 = 2 * (V * K * K - 1) * norm;
        m_filter[LOWSHELF].a2 = (1 - sqrtf(2 * V) * K + V * K * K) * norm;
        m_filter[LOWSHELF].b1 = 2 * (K * K - 1) * norm;
        m_filter[LOWSHELF].b2 = (1 - sqrtf(2) * K + K * K) * norm;
    }
    else { // cut
        norm = 1 / (1 + sqrtf(2 * V) * K + V * K * K);
        m_filter[LOWSHELF].a0 = (1 + sqrtf(2) * K + K * K) * norm;
        m_filter[LOWSHELF].a1 = 2 * (K * K - 1) * norm;
        m_filter[LOWSHELF].a2 = (1 - sqrtf(2) * K + K * K) * norm;
        m_filter[LOWSHELF].b1 = 2 * (V * K * K - 1) * norm;
        m_filter[LOWSHELF].b2 = (1 - sqrtf(2 * V) * K + V * K * K) * norm;
    }

    // PEAK EQ
    Fc = (float)FcPKEQ / (float)getSampleRate(); // Cutoff frequency
    K = tanf((float)PI * Fc);
    V = powf(10, fabs(G1) / 20.0);
    Q = 2.5;      // Quality factor
    if(G1 >= 0) { // boost
        norm = 1 / (1 + 1 / Q * K + K * K);
        m_filter[PEAKEQ].a0 = (1 + V / Q * K + K * K) * norm;
        m_filter[PEAKEQ].a1 = 2 * (K * K - 1) * norm;
        m_filter[PEAKEQ].a2 = (1 - V / Q * K + K * K) * norm;
        m_filter[PEAKEQ].b1 = m_filter[PEAKEQ].a1;
        m_filter[PEAKEQ].b2 = (1 - 1 / Q * K + K * K) * norm;
    }
    else { // cut
        norm = 1 / (1 + V / Q * K + K * K);
        m_filter[PEAKEQ].a0 = (1 + 1 / Q * K + K * K) * norm;
        m_filter[PEAKEQ].a1 = 2 * (K * K - 1) * norm;
        m_filter[PEAKEQ].a2 = (1 - 1 / Q * K + K * K) * norm;
        m_filter[PEAKEQ].b1 = m_filter[PEAKEQ].a1;
        m_filter[PEAKEQ].b2 = (1 - V / Q * K + K * K) * norm;
    }

    // HIGHSHELF
    Fc = (float)FcHS / (float)getSampleRate(); // Cutoff frequency
    K = tanf((float)PI * Fc);
    V = powf(10, fabs(G2) / 20.0);
    if(G2 >= 0) { // boost
        norm = 1 / (1 + sqrtf(2) * K + K * K);
        m_filter[HIFGSHELF].a0 = (V + sqrtf(2 * V) * K + K * K) * norm;
        m_filter[HIFGSHELF].a1 = 2 * (K * K - V) * norm;
        m_filter[HIFGSHELF].a2 = (V - sqrtf(2 * V) * K + K * K) * norm;
        m_filter[HIFGSHELF].b1 = 2 * (K * K - 1) * norm;
        m_filter[HIFGSHELF].b2 = (1 - sqrtf(2) * K + K * K) * norm;
    }
    else {
        norm = 1 / (V + sqrtf(2 * V) * K + K * K);
        m_filter[HIFGSHELF].a0 = (1 + sqrtf(2) * K + K * K) * norm;
        m_filter[HIFGSHELF].a1 = 2 * (K * K - 1) * norm;
        m_filter[HIFGSHELF].a2 = (1 - sqrtf(2) * K + K * K) * norm;
        m_filter[HIFGSHELF].b1 = 2 * (K * K - V) * norm;
        m_filter[HIFGSHELF].b2 = (V - sqrtf(2 * V) * K + K * K) * norm;
    }

    //    AUDIO_LOG_INFO("LS a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[0].a0, m_filter[0].a1, m_filter[0].a2,
    //                                                  m_filter[0].b1, m_filter[0].b2);
    //    AUDIO_LOG_INFO("EQ a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[1].a0, m_filter[1].a1, m_filter[1].a2,
    //                                                  m_filter[1].b1, m_filter[1].b2);
    //    AUDIO_LOG_INFO("HS a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[2].a0, m_filter[2].a1, m_filter[2].a2,
    //                                                  m_filter[2].b1, m_filter[2].b2);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// clang-format off
void Audio::IIR_filterChain0(int16_t iir_in[2], bool clear) { // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum : uint8_t { in = 0, out = 1 };

    if(clear) {
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // zero IIR filterbuffer
        m_ifCh.iir_out0[0] = 0;
        m_ifCh.iir_out0[1] = 0;
        iir_in[0] = 0;
        iir_in[1] = 0;
    }

    m_ifCh.inSample0[LEFTCHANNEL] = (float)(iir_in[LEFTCHANNEL]);
    m_ifCh.inSample0[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    m_ifCh.outSample0[LEFTCHANNEL] =
        m_filter[0].a0 * m_ifCh.inSample0[LEFTCHANNEL] + m_filter[0].a1 * m_filterBuff[0][z1][in][LEFTCHANNEL] +
        m_filter[0].a2 * m_filterBuff[0][z2][in][LEFTCHANNEL] - m_filter[0].b1 * m_filterBuff[0][z1][out][LEFTCHANNEL] -
        m_filter[0].b2 * m_filterBuff[0][z2][out][LEFTCHANNEL];

    m_filterBuff[0][z2][in][LEFTCHANNEL] = m_filterBuff[0][z1][in][LEFTCHANNEL];
    m_filterBuff[0][z1][in][LEFTCHANNEL] = m_ifCh.inSample0[LEFTCHANNEL];
    m_filterBuff[0][z2][out][LEFTCHANNEL] = m_filterBuff[0][z1][out][LEFTCHANNEL];
    m_filterBuff[0][z1][out][LEFTCHANNEL] = m_ifCh.outSample0[LEFTCHANNEL];
    m_ifCh.iir_out0[LEFTCHANNEL] = (int16_t)m_ifCh.outSample0[LEFTCHANNEL];

    m_ifCh.outSample0[RIGHTCHANNEL] = m_filter[0].a0 * m_ifCh.inSample0[RIGHTCHANNEL] +
                              m_filter[0].a1 * m_filterBuff[0][z1][in][RIGHTCHANNEL] +
                              m_filter[0].a2 * m_filterBuff[0][z2][in][RIGHTCHANNEL] -
                              m_filter[0].b1 * m_filterBuff[0][z1][out][RIGHTCHANNEL] -
                              m_filter[0].b2 * m_filterBuff[0][z2][out][RIGHTCHANNEL];

    m_filterBuff[0][z2][in][RIGHTCHANNEL] = m_filterBuff[0][z1][in][RIGHTCHANNEL];
    m_filterBuff[0][z1][in][RIGHTCHANNEL] = m_ifCh.inSample0[RIGHTCHANNEL];
    m_filterBuff[0][z2][out][RIGHTCHANNEL] = m_filterBuff[0][z1][out][RIGHTCHANNEL];
    m_filterBuff[0][z1][out][RIGHTCHANNEL] = m_ifCh.outSample0[RIGHTCHANNEL];
    m_ifCh.iir_out0[RIGHTCHANNEL] = (int16_t)m_ifCh.outSample0[RIGHTCHANNEL];

    iir_in[LEFTCHANNEL] = m_ifCh.iir_out0[LEFTCHANNEL];
    iir_in[RIGHTCHANNEL] = m_ifCh.iir_out0[RIGHTCHANNEL];
    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::IIR_filterChain1(int16_t iir_in[2], bool clear) { // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum : uint8_t { in = 0, out = 1 };

    if(clear) {
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // zero IIR filterbuffer
        m_ifCh.iir_out1[0] = 0;
        m_ifCh.iir_out1[1] = 0;
        iir_in[0] = 0;
        iir_in[1] = 0;
    }

    m_ifCh.inSample1[LEFTCHANNEL] = (float)(iir_in[LEFTCHANNEL]);
    m_ifCh.inSample1[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    m_ifCh.outSample1[LEFTCHANNEL] =
        m_filter[1].a0 * m_ifCh.inSample1[LEFTCHANNEL] + m_filter[1].a1 * m_filterBuff[1][z1][in][LEFTCHANNEL] +
        m_filter[1].a2 * m_filterBuff[1][z2][in][LEFTCHANNEL] - m_filter[1].b1 * m_filterBuff[1][z1][out][LEFTCHANNEL] -
        m_filter[1].b2 * m_filterBuff[1][z2][out][LEFTCHANNEL];

    m_filterBuff[1][z2][in][LEFTCHANNEL] = m_filterBuff[1][z1][in][LEFTCHANNEL];
    m_filterBuff[1][z1][in][LEFTCHANNEL] = m_ifCh.inSample1[LEFTCHANNEL];
    m_filterBuff[1][z2][out][LEFTCHANNEL] = m_filterBuff[1][z1][out][LEFTCHANNEL];
    m_filterBuff[1][z1][out][LEFTCHANNEL] = m_ifCh.outSample1[LEFTCHANNEL];
    m_ifCh.iir_out1[LEFTCHANNEL] = (int16_t)m_ifCh.outSample1[LEFTCHANNEL];

    m_ifCh.outSample1[RIGHTCHANNEL] = m_filter[1].a0 * m_ifCh.inSample1[RIGHTCHANNEL] +
                              m_filter[1].a1 * m_filterBuff[1][z1][in][RIGHTCHANNEL] +
                              m_filter[1].a2 * m_filterBuff[1][z2][in][RIGHTCHANNEL] -
                              m_filter[1].b1 * m_filterBuff[1][z1][out][RIGHTCHANNEL] -
                              m_filter[1].b2 * m_filterBuff[1][z2][out][RIGHTCHANNEL];

    m_filterBuff[1][z2][in][RIGHTCHANNEL] = m_filterBuff[1][z1][in][RIGHTCHANNEL];
    m_filterBuff[1][z1][in][RIGHTCHANNEL] = m_ifCh.inSample1[RIGHTCHANNEL];
    m_filterBuff[1][z2][out][RIGHTCHANNEL] = m_filterBuff[1][z1][out][RIGHTCHANNEL];
    m_filterBuff[1][z1][out][RIGHTCHANNEL] = m_ifCh.outSample1[RIGHTCHANNEL];
    m_ifCh.iir_out1[RIGHTCHANNEL] = (int16_t)m_ifCh.outSample1[RIGHTCHANNEL];

    iir_in[LEFTCHANNEL] = m_ifCh.iir_out1[LEFTCHANNEL];
    iir_in[RIGHTCHANNEL] = m_ifCh.iir_out1[RIGHTCHANNEL];
    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Audio::IIR_filterChain2(int16_t iir_in[2], bool clear) { // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum : uint8_t { in = 0, out = 1 };

    if(clear) {
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // zero IIR filterbuffer
        m_ifCh.iir_out2[0] = 0;
        m_ifCh.iir_out2[1] = 0;
        iir_in[0] = 0;
        iir_in[1] = 0;
    }

    m_ifCh.inSample2[LEFTCHANNEL] = (float)(iir_in[LEFTCHANNEL]);
    m_ifCh.inSample2[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    m_ifCh.outSample2[LEFTCHANNEL] =
        m_filter[2].a0 * m_ifCh.inSample2[LEFTCHANNEL] + m_filter[2].a1 * m_filterBuff[2][z1][in][LEFTCHANNEL] +
        m_filter[2].a2 * m_filterBuff[2][z2][in][LEFTCHANNEL] - m_filter[2].b1 * m_filterBuff[2][z1][out][LEFTCHANNEL] -
        m_filter[2].b2 * m_filterBuff[2][z2][out][LEFTCHANNEL];

    m_filterBuff[2][z2][in][LEFTCHANNEL] = m_filterBuff[2][z1][in][LEFTCHANNEL];
    m_filterBuff[2][z1][in][LEFTCHANNEL] = m_ifCh.inSample2[LEFTCHANNEL];
    m_filterBuff[2][z2][out][LEFTCHANNEL] = m_filterBuff[2][z1][out][LEFTCHANNEL];
    m_filterBuff[2][z1][out][LEFTCHANNEL] = m_ifCh.outSample2[LEFTCHANNEL];
    m_ifCh.iir_out2[LEFTCHANNEL] = (int16_t)m_ifCh.outSample2[LEFTCHANNEL];

    m_ifCh.outSample2[RIGHTCHANNEL] = m_filter[2].a0 * m_ifCh.inSample2[RIGHTCHANNEL] +
                              m_filter[2].a1 * m_filterBuff[2][z1][in][RIGHTCHANNEL] +
                              m_filter[2].a2 * m_filterBuff[2][z2][in][RIGHTCHANNEL] -
                              m_filter[2].b1 * m_filterBuff[2][z1][out][RIGHTCHANNEL] -
                              m_filter[2].b2 * m_filterBuff[2][z2][out][RIGHTCHANNEL];

    m_filterBuff[2][z2][in][RIGHTCHANNEL] = m_filterBuff[2][z1][in][RIGHTCHANNEL];
    m_filterBuff[2][z1][in][RIGHTCHANNEL] = m_ifCh.inSample2[RIGHTCHANNEL];
    m_filterBuff[2][z2][out][RIGHTCHANNEL] = m_filterBuff[2][z1][out][RIGHTCHANNEL];
    m_filterBuff[2][z1][out][RIGHTCHANNEL] = m_ifCh.outSample2[RIGHTCHANNEL];
    m_ifCh.iir_out2[RIGHTCHANNEL] = (int16_t)m_ifCh.outSample2[RIGHTCHANNEL];

    iir_in[LEFTCHANNEL] = m_ifCh.iir_out2[LEFTCHANNEL];
    iir_in[RIGHTCHANNEL] = m_ifCh.iir_out2[RIGHTCHANNEL];
    return;
}
// clang-format on
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//    AAC - T R A N S P O R T S T R E A M
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::ts_parsePacket(uint8_t* packet, uint8_t* packetStart, uint8_t* packetLength) {

    bool log = false;

    const uint8_t TS_PACKET_SIZE = 188;
    const uint8_t PAYLOAD_SIZE = 184;
    const uint8_t PID_ARRAY_LEN = 4;

    (void)PAYLOAD_SIZE; // suppress [-Wunused-variable]

    if(packet == NULL) {
        if(log) AUDIO_LOG_WARN("parseTS reset");
        for(int i = 0; i < PID_ARRAY_LEN; i++) m_tspp.pids[i] = 0;
        m_tspp.PES_DataLength = 0;
        m_tspp.pidOfAAC = 0;
        return true;
    }

    // --------------------------------------------------------------------------------------------------------
    // 0. Byte SyncByte  | 0 | 1 | 0 | 0 | 0 | 1 | 1 | 1 | always bit pattern of 0x47
    //---------------------------------------------------------------------------------------------------------
    // 1. Byte           |PUSI|TP|   |PID|PID|PID|PID|PID|
    //---------------------------------------------------------------------------------------------------------
    // 2. Byte           |PID|PID|PID|PID|PID|PID|PID|PID|
    //---------------------------------------------------------------------------------------------------------
    // 3. Byte           |TSC|TSC|AFC|AFC|CC |CC |CC |CC |
    //---------------------------------------------------------------------------------------------------------
    // 4.-187. Byte      |Payload data if AFC==01 or 11  |
    //---------------------------------------------------------------------------------------------------------

    // PUSI Payload unit start indicator, set when this packet contains the first byte of a new payload unit.
    //      The first byte of the payload will indicate where this new payload unit starts.
    // TP   Transport priority, set when the current packet has a higher priority than other packets with the same PID.
    // PID  Packet Identifier, describing the payload data.
    // TSC  Transport scrambling control, '00' = Not scrambled.
    // AFC  Adaptation field control, 01 – no adaptation field, payload only, 10 – adaptation field only, no payload,
    //                                11 – adaptation field followed by payload, 00 – RESERVED for future use
    // CC   Continuity counter, Sequence number of payload packets (0x00 to 0x0F) within each stream (except PID 8191)

    // for(int i = 1; i < 188; i++) {printf("%02X ", packet[i - 1]); if(i && (i % 16 == 0)) printf("\n");}
    // printf("\n----------\n");

    if(packet[0] != 0x47) {
        AUDIO_LOG_ERROR("ts SyncByte not found, first bytes are 0x%02X 0x%02X 0x%02X 0x%02X", packet[0], packet[1], packet[2], packet[3]);
        // stopSong();
        return false;
    }
    int PID = (packet[1] & 0x1F) << 8 | (packet[2] & 0xFF);
    if(log) AUDIO_LOG_DEBUG("PID: 0x%04X(%d)", PID, PID);
    int PUSI = (packet[1] & 0x40) >> 6;
    if(log) AUDIO_LOG_DEBUG("Payload Unit Start Indicator: %d", PUSI);
    int AFC = (packet[3] & 0x30) >> 4;
    if(log) AUDIO_LOG_DEBUG("Adaption Field Control: %d", AFC);

    int AFL = -1;
    if((AFC & 0b10) == 0b10) {  // AFC '11' Adaptation Field followed
        AFL = packet[4] & 0xFF; // Adaptation Field Length
        if(log) AUDIO_LOG_DEBUG("Adaptation Field Length: %d", AFL);
    }
    int PLS = PUSI ? 5 : 4;     // PayLoadStart, Payload Unit Start Indicator
    if(AFL > 0) PLS += AFL + 1; // skip adaption field

    if(AFC == 2){ // The TS package contains only an adaptation Field and no user data.
        *packetStart = AFL + 1;
        *packetLength = 0;
        return true;
    }

    if(PID == 0) {
        // Program Association Table (PAT) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if(log) AUDIO_LOG_DEBUG("PAT");
        m_tspp.pidNumber = 0;
        m_tspp.pidOfAAC = 0;

        int startOfProgramNums = 8;
        int lengthOfPATValue = 4;
        int sectionLength = ((packet[PLS + 1] & 0x0F) << 8) | (packet[PLS + 2] & 0xFF);
        if(log) AUDIO_LOG_DEBUG("Section Length: %d", sectionLength);
        int program_number, program_map_PID;
        int indexOfPids = 0;
        (void)program_number; // [-Wunused-but-set-variable]
        for(int i = startOfProgramNums; i <= sectionLength; i += lengthOfPATValue) {
            program_number = ((packet[PLS + i] & 0xFF) << 8) | (packet[PLS + i + 1] & 0xFF);
            program_map_PID = ((packet[PLS + i + 2] & 0x1F) << 8) | (packet[PLS + i + 3] & 0xFF);
            if(log) AUDIO_LOG_DEBUG("Program Num: 0x%04X(%d) PMT PID: 0x%04X(%d)", program_number, program_number, program_map_PID, program_map_PID);
            m_tspp.pids[indexOfPids++] = program_map_PID;
        }
        m_tspp.pidNumber = indexOfPids;
        *packetStart = 0;
        *packetLength = 0;
        return true;
    }
    else if(PID == m_tspp.pidOfAAC) {
        if(log) AUDIO_LOG_DEBUG("AAC");
        uint8_t posOfPacketStart = 4;
        if(AFL >= 0) {
            posOfPacketStart = 5 + AFL;
            if(log) AUDIO_LOG_DEBUG("posOfPacketStart: %d", posOfPacketStart);
        }
        // Packetized Elementary Stream (PES) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if(log) AUDIO_LOG_DEBUG("PES_DataLength %i", m_tspp.PES_DataLength);
        if(m_tspp.PES_DataLength > 0) {
            *packetStart = posOfPacketStart + m_tspp.fillData;
            *packetLength = TS_PACKET_SIZE - posOfPacketStart - m_tspp.fillData;
            if(log) AUDIO_LOG_DEBUG("packetlength %i", *packetLength);
            m_tspp.fillData = 0;
            m_tspp.PES_DataLength -= (*packetLength);
            return true;
        }
        else {
            int firstByte = packet[posOfPacketStart] & 0xFF;
            int secondByte = packet[posOfPacketStart + 1] & 0xFF;
            int thirdByte = packet[posOfPacketStart + 2] & 0xFF;
            if(log) AUDIO_LOG_DEBUG ("First 3 bytes: 0x%02X, 0x%02X, 0x%02X", firstByte, secondByte, thirdByte);
            if(firstByte == 0x00 && secondByte == 0x00 && thirdByte == 0x01) { // Packet start code prefix
                // --------------------------------------------------------------------------------------------------------
                // posOfPacketStart + 0...2     0x00, 0x00, 0x01                                          PES-Startcode
                //---------------------------------------------------------------------------------------------------------
                // posOfPacketStart + 3         0xE0 (Video) od 0xC0 (Audio)                              StreamID
                //---------------------------------------------------------------------------------------------------------
                // posOfPacketStart + 4...5     0xLL, 0xLL                                                PES Packet length
                //---------------------------------------------------------------------------------------------------------
                // posOfPacketStart + 6...7                                                               PTS/DTS Flags
                //---------------------------------------------------------------------------------------------------------
                // posOfPacketStart + 8         0xXX                                                      header length
                //---------------------------------------------------------------------------------------------------------

                uint8_t StreamID = packet[posOfPacketStart + 3] & 0xFF;
                if(StreamID >= 0xC0 && StreamID <= 0xDF) { ; } // okay ist audio stream
                if(StreamID >= 0xE0 && StreamID <= 0xEF) {AUDIO_LOG_ERROR("video stream!"); return false; }
                int PES_PacketLength = ((packet[posOfPacketStart + 4] & 0xFF) << 8) + (packet[posOfPacketStart + 5] & 0xFF);
                if(log) AUDIO_LOG_DEBUG("PES PacketLength: %d", PES_PacketLength);
                bool PTS_flag = false;
                bool DTS_flag = false;
                int flag_byte1 = packet[posOfPacketStart + 6] & 0xFF;
                int flag_byte2 = packet[posOfPacketStart + 7] & 0xFF;  (void) flag_byte2; // unused yet
                if(flag_byte1 & 0b10000000) PTS_flag = true;
                if(flag_byte1 & 0b00000100) DTS_flag = true;
                if(log && PTS_flag) AUDIO_LOG_DEBUG("PTS_flag is set");
                if(log && DTS_flag) AUDIO_LOG_DEBUG("DTS_flag is set");
                uint8_t PES_HeaderDataLength = packet[posOfPacketStart + 8] & 0xFF;
                if(log) AUDIO_LOG_DEBUG("PES_headerDataLength %d", PES_HeaderDataLength);

                m_tspp.PES_DataLength = PES_PacketLength;
                int startOfData = PES_HeaderDataLength + 9;
                if(posOfPacketStart + startOfData >= 188) { // only fillers in packet
                    if(log) AUDIO_LOG_DEBUG("posOfPacketStart + startOfData %i", posOfPacketStart + startOfData);
                    *packetStart = 0;
                    *packetLength = 0;
                    m_tspp.PES_DataLength -= (PES_HeaderDataLength + 3);
                    m_tspp.fillData = (posOfPacketStart + startOfData) - 188;
                    if(log) AUDIO_LOG_DEBUG("fillData %i", m_tspp.fillData);
                    return true;
                }
                if(log) AUDIO_LOG_DEBUG("First AAC data byte: %02X", packet[posOfPacketStart + startOfData]);
                if(log) AUDIO_LOG_DEBUG("Second AAC data byte: %02X", packet[posOfPacketStart + startOfData + 1]);
                *packetStart = posOfPacketStart + startOfData;
                *packetLength = TS_PACKET_SIZE - posOfPacketStart - startOfData;
                m_tspp.PES_DataLength -= (*packetLength);
                m_tspp.PES_DataLength -= (PES_HeaderDataLength + 3);
                return true;
            }
            if(firstByte == 0 && secondByte == 0 && thirdByte == 0){
                // PES packet startcode prefix is 0x000000
                // skip such packets
                return true;
            }
        }
        *packetStart = 0;
        *packetLength = 0;
        AUDIO_LOG_ERROR("PES not found");
        return false;
    }
    else if(m_tspp.pidNumber) {
        //  Program Map Table (PMT) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        for(int i = 0; i < m_tspp.pidNumber; i++) {
            if(PID == m_tspp.pids[i]) {
                if(log) AUDIO_LOG_DEBUG("PMT");
                int staticLengthOfPMT = 12;
                int sectionLength = ((packet[PLS + 1] & 0x0F) << 8) | (packet[PLS + 2] & 0xFF);
                if(log) AUDIO_LOG_DEBUG("Section Length: %d", sectionLength);
                int programInfoLength = ((packet[PLS + 10] & 0x0F) << 8) | (packet[PLS + 11] & 0xFF);
                if(log) AUDIO_LOG_DEBUG("Program Info Length: %d", programInfoLength);
                int cursor = staticLengthOfPMT + programInfoLength;
                while(cursor < sectionLength - 1) {
                    int streamType = packet[PLS + cursor] & 0xFF;
                    int elementaryPID = ((packet[PLS + cursor + 1] & 0x1F) << 8) | (packet[PLS + cursor + 2] & 0xFF);
                    if(log) AUDIO_LOG_DEBUG("Stream Type: 0x%02X Elementary PID: 0x%04X", streamType, elementaryPID);

                    if(streamType == 0x0F || streamType == 0x11 || streamType == 0x04) {
                        if(log) AUDIO_LOG_DEBUG("AAC PID discover");
                        m_tspp.pidOfAAC = elementaryPID;
                    }
                    int esInfoLength = ((packet[PLS + cursor + 3] & 0x0F) << 8) | (packet[PLS + cursor + 4] & 0xFF);
                    if(log) AUDIO_LOG_DEBUG("ES Info Length: 0x%04X", esInfoLength);
                    cursor += 5 + esInfoLength;
                }
            }
        }
        *packetStart = 0;
        *packetLength = 0;
        return true;
    }
    // PES received before PAT and PMT seen
    *packetStart = 0;
    *packetLength = 0;
    return false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//    W E B S T R E A M  -  H E L P   F U N C T I O N S
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint16_t Audio::readMetadata(uint16_t maxBytes, bool first) {
    m_rmet.res = 0;
    ps_ptr<char>buff; buff.alloc(4096); buff.clear(); // is max 256 *16
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(first) {
        m_rmet.pos_ml = 0; // determines the current position in metaline
        m_rmet.metalen = 0;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(!maxBytes) return 0; // guard

    if(!m_rmet.metalen) {
        int b = audioFileRead(); // First byte of metadata?
        if (b < 0) {
            AUDIO_INFO("client->read() failed (%d)", b);
            return 0;
        }
        m_rmet.metalen = b * 16;        // New count for metadata including length byte, max 4096
        m_rmet.pos_ml = 0;
        buff[m_rmet.pos_ml] = 0; // Prepare for new line
        m_rmet.res = 1;
    }
    if(!m_rmet.metalen) {
        m_metacount = m_metaint;
        return m_rmet.res;
    } // metalen is 0
    uint16_t a = audioFileRead((uint8_t*)&buff[m_rmet.pos_ml], min((uint16_t)(m_rmet.metalen - m_rmet.pos_ml), (uint16_t)(maxBytes)));
    m_rmet.res += a;
    m_rmet.pos_ml += a;

    if(m_rmet.pos_ml == m_rmet.metalen) {
        buff[m_rmet.pos_ml] = '\0';
        if(buff.strlen() > 0) { // Any info present?
            // metaline contains artist and song name.  For example:
            // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
            // Sometimes it is just other info like:
            // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
            // Isolate the StreamTitle, remove leading and trailing quotes if present.
            latinToUTF8(buff);          // convert to UTF-8 if necessary
            int pos = buff.index_of_icase("song_spot", 0); // remove some irrelevant infos
            if(pos > 3) {                               // e.g. song_spot="T" MediaBaseId="0" itunesTrackId="0"
                buff[pos] = 0;
            }
            showstreamtitle(buff.get()); // Show artist and title if present in metadata
        }
        m_metacount = m_metaint;
        m_rmet.metalen = 0;
        m_rmet.pos_ml = 0;
    }
    return m_rmet.res;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
size_t Audio::readChunkSize(uint8_t* bytes) {
    uint8_t  byteCounter = 0;
    size_t   chunksize = 0;
    // bool     parsingChunkSize = true;
    int      b = 0;
    std::string chunkLine;
    uint32_t ctime = millis();
    uint32_t timeout = 2000; // ms

    while (true) {
        if ((millis() - ctime) > timeout) {
            AUDIO_LOG_ERROR("chunkedDataTransfer: timeout");
            stopSong();
            return 0;
        }
        if(!m_client->available()) continue; // no data available yet
        b = audioFileRead();
        if (b < 0) continue; // -1 = no data

        byteCounter++;
        if (b == '\n') break; // End of chunk-size line
        if (b == '\r') continue;

        chunkLine += static_cast<char>(b);
    }

    // chunkLine z.B.: "2A", oder "2A;foo=bar"
    size_t semicolonPos = chunkLine.find(';');
    std::string hexSize = (semicolonPos != std::string::npos) ? chunkLine.substr(0, semicolonPos) : chunkLine;

    // Converted hex number
    chunksize = strtoul(hexSize.c_str(), nullptr, 16);
    *bytes = byteCounter;

    if (chunksize == 0) { // Special case: Last chunk recognized (0) => Next read and reject "\ r \ n"
        // Reading to complete "\ r \ n" was received
        uint8_t crlf[2] = {0}; (void)crlf; // suppress [-Wunused-variable]
        uint8_t idx = 0;
        ctime = millis();
        while (idx < 2 && (millis() - ctime) < timeout) {
            int ch = audioFileRead();
            if (ch < 0) continue;
            crlf[idx++] = static_cast<uint8_t>(ch);
        }
    }

    return chunksize;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool Audio::readID3V1Tag() {
    if (m_codec != CODEC_MP3) return false;
    ps_ptr<char>chBuff;
    chBuff.alloc(256);

    const uint8_t* p = InBuff.getReadPtr();

    // Lambda for simplification
    auto readID3Field = [&](ps_ptr<char>& field, const uint8_t* src, size_t len, const char* label = nullptr) {
        field.alloc(len + 1);
        memcpy(field.get(), src, len);
        field[len] = '\0';
        latinToUTF8(field);
        if (field.strlen() > 0 && label && audio_id3data) {
            field.insert(label, 0);
            audio_id3data(field.get());
        }
    };

    if (InBuff.bufferFilled() == 128 && startsWith((const char*)p, "TAG")) {
        ps_ptr<char> title, artist, album, year, comment;

        readID3Field(title,   p + 3,     30, "Title: ");
        readID3Field(artist,  p + 33,    30, "Artist: ");
        readID3Field(album,   p + 63,    30, "Album: ");
        readID3Field(year,    p + 93,    4,  "Year: ");
        readID3Field(comment, p + 97,    30, "Comment: ");

        uint8_t zeroByte = p[125];
        uint8_t track = p[126];
        uint8_t genre8 = p[127];

        AUDIO_INFO(zeroByte ? "ID3 version: 1" : "ID3 Version 1.1");

        if (zeroByte == 0) {
            sprintf(chBuff.get(), "Track Number: %d", track);
            if (audio_id3data) audio_id3data(chBuff.get());
        }

        if (genre8 < 192) {
            sprintf(chBuff.get(), "Genre: %d", genre8);
            if (audio_id3data) audio_id3data(chBuff.get());
        }

        return true;
    }

    if (InBuff.bufferFilled() == 227 && startsWith((const char*)p, "TAG+")) { // "TAG+" "can exist as an extension, does not overwrite" TAG"
        AUDIO_INFO("ID3 version: 1 - Enhanced TAG");

        ps_ptr<char> title, artist, album, genre;

        readID3Field(title,  p + 4,     60, "Title: ");
        readID3Field(artist, p + 64,    60, "Artist: ");
        readID3Field(album,  p + 124,   60, "Album: ");
        readID3Field(genre,  p + 185,   30, "Genre: ");

        // optional expansion: speed, start-time, end-time
        return true;
    }

    return false;
    // [1] https://en.wikipedia.org/wiki/List_of_ID3v1_Genres
    // [2] https://en.wikipedia.org/wiki/ID3#ID3v1_and_ID3v1.1[5]
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t Audio::newInBuffStart(int32_t m_resumeFilePos){
        uint16_t remaining = 0;
        int32_t  offset = 0, buffFillSize = 0, res = 0;
        uint32_t timeOut = 0;

        if(m_controlCounter != 100){AUDIO_LOG_WARN("timeOffset not possible"); m_resumeFilePos = -1; offset = -1; goto exit;}
        if(m_resumeFilePos >= (int32_t)m_audioDataStart + m_audioDataSize) {   m_resumeFilePos = -1; offset = -1; goto exit;}
        if(m_codec == CODEC_M4A && ! m_stsz_position){                         m_resumeFilePos = -1; offset = -1; goto exit;}

        if(m_resumeFilePos <  (int32_t)m_audioDataStart) m_resumeFilePos = m_audioDataStart;
        buffFillSize = min(m_audioDataSize - m_resumeFilePos, UINT16_MAX);

        m_f_lockInBuffer = true;                          // lock the buffer, the InBuffer must not be re-entered in playAudioData()
            while(m_f_audioTaskIsDecoding) vTaskDelay(1); // We can't reset the InBuffer while the decoding is in progress
            m_f_allDataReceived = false;

/* process before */
            if(m_codec == CODEC_M4A) m_resumeFilePos += m4a_correctResumeFilePos(); {if(m_resumeFilePos == -1) goto exit;}

/* skip to position */
            res = audioFileSeek(m_resumeFilePos);
            InBuff.resetBuffer();
            remaining = buffFillSize;
            offset = 0;
            timeOut = millis();
            while (remaining > 0) {
                int bytesRead = audioFileRead(InBuff.getReadPtr() + offset, remaining);
                if (bytesRead <= 0) {
                    if(millis() > timeOut + 2000){AUDIO_LOG_ERROR("timeout, not enough data from host"); buffFillSize = offset; break;}
                    continue; // ggf. Sleep oder Timeout prüfen, um Endlosschleife zu vermeiden
                }
                remaining -= bytesRead;
                offset += bytesRead;
            }
            InBuff.bytesWritten(buffFillSize);
/* process after */
            offset = 0;
            if(m_codec == CODEC_OPUS || m_codec == CODEC_VORBIS) {if(InBuff.bufferFilled() < 0xFFFF) return - 1;} // ogg frame <= 64kB
            if(m_codec == CODEC_WAV)   {while((m_resumeFilePos % 4) != 0){m_resumeFilePos++; offset++; if(m_resumeFilePos >= m_audioFileSize) goto exit;}}  // must divisible by four
            if(m_codec == CODEC_MP3)   {offset = mp3_correctResumeFilePos();  if(offset == -1) goto exit; MP3Decoder_ClearBuffer();}
            if(m_codec == CODEC_FLAC)  {offset = flac_correctResumeFilePos(); if(offset == -1) goto exit; FLACDecoderReset();}
            if(m_codec == CODEC_VORBIS){offset = ogg_correctResumeFilePos();  if(offset == -1) goto exit; VORBISDecoder_ClearBuffers();}
            if(m_codec == CODEC_OPUS)  {offset = ogg_correctResumeFilePos();  if(offset == -1) goto exit; OPUSDecoder_ClearBuffers();}


            InBuff.bytesWasRead(offset);

        m_f_lockInBuffer = false;
        return res + offset;
exit:
        m_f_lockInBuffer = false;
        stopSong();
        return offset;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
boolean Audio::streamDetection(uint32_t bytesAvail) {
    if(!m_lastHost.valid()) {AUDIO_LOG_ERROR("m_lastHost is empty"); return false;}

    // if within one second the content of the audio buffer falls below the size of an audio frame 100 times,
    // issue a message
    if(m_sdet.tmr_slow + 1000 < millis()) {
        m_sdet.tmr_slow = millis();
        if(m_sdet.cnt_slow > 100) AUDIO_INFO("slow stream, dropouts are possible");
        m_sdet.cnt_slow = 0;
    }
    if(InBuff.bufferFilled() < InBuff.getMaxBlockSize()) m_sdet.cnt_slow++;
    if(bytesAvail) {
        m_sdet.tmr_lost = millis() + 1000;
        m_sdet.cnt_lost = 0;
    }
    if(InBuff.bufferFilled() > InBuff.getMaxBlockSize() * 2) return false; // enough data available to play

    // if no audio data is received within three seconds, a new connection attempt is started.
    if(m_sdet.tmr_lost < millis()) {
        m_sdet.cnt_lost++;
        m_sdet.tmr_lost = millis() + 1000;
        if(m_sdet.cnt_lost == 5) { // 5s no data?
            m_sdet.cnt_lost = 0;
            AUDIO_INFO("Stream lost -> try new connection");
            m_f_reset_m3u8Codec = false;
            httpPrint(m_lastHost.get());
            return true;
        }
    }
    return false;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::m4a_correctResumeFilePos() {
    // In order to jump within an m4a file, the exact beginning of an aac block must be found. Since m4a cannot be
    // streamed, i.e. there is no syncword, an imprecise jump can lead to a crash.

    if(!m_stsz_position) return m_audioDataStart; // guard

    typedef union {
        uint8_t  u8[4];
        uint32_t u32;
    } tu;
    tu uu;

    uint32_t i = 0, pos = m_audioDataStart;
    uint32_t filePtr = m_audioFilePosition;
    bool found = false;
    audioFileSeek(m_stsz_position);

    auto read_next = [&]() -> int {
        int r;
        int cnt = 0;
        while(true){
            r = audioFileRead();
            if(r >= 0) break;
            vTaskDelay(10);
            cnt++;
            if(cnt > 300) {AUDIO_LOG_ERROR("timeout"); break;}
        }
        return r;
    };

    while(i < m_stsz_numEntries) {
        i++;
        uu.u8[3] = read_next();
        uu.u8[2] = read_next();
        uu.u8[1] = read_next();
        uu.u8[0] = read_next();

        pos += uu.u32;
        if(pos >= m_resumeFilePos) {found = true; break;}
    }
    if(!found)  return -1; // not found

    // audioFileSeek(filePtr); // restore file pointer
    return pos - m_resumeFilePos; // return the number of bytes to jump
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t Audio::ogg_correctResumeFilePos() {
    // The starting point is the next OggS magic word
    vTaskDelay(1);
    // // AUDIO_LOG_INFO("in_resumeFilePos %i", resumeFilePos);

    auto find_sync_word = [&](uint8_t* pos, uint32_t av) -> int {
        int steps = 0;
        while(av--) {
            if(pos[steps] == 'O'){
                if(pos[steps + 1] == 'g') {
                    if(pos[steps + 2] == 'g') {
                        if(pos[steps + 3] == 'S') { // Check for the second part of magic word
                            return steps;   // Magic word found, return the number of steps
                        }
                    }
                }
            }
            steps++;
        }
        return -1; // Return -1 if OggS magic word is not found
    };

    uint8_t*       readPtr = InBuff.getReadPtr();
    size_t         av = InBuff.getMaxAvailableBytes();
    int32_t        steps = 0;

    if(av < InBuff.getMaxBlockSize()) return -1; // guard

    steps = find_sync_word(readPtr, av);
    if(steps == -1) return -1;
    return steps; // Return the number of steps to the sync word
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t Audio::flac_correctResumeFilePos() {

    auto find_sync_word = [&](uint8_t* pos, uint32_t av) -> int {
        int steps = 0;
        while(av--) {
            char c = pos[steps];
            steps++;
            if(c == 0xFF) { // First byte of sync word found
                char nextByte = pos[steps];
                steps++;
                if(nextByte == 0xF8) {     // Check for the second part of sync word
                    return steps - 2;       // Sync word found, return the number of steps
                }
            }
        }
        return -1; // Return -1 if sync word is not found
    };

    uint8_t*       readPtr = InBuff.getReadPtr();
    size_t         av = InBuff.getMaxAvailableBytes();
    int32_t        steps = 0;

    if(av < InBuff.getMaxBlockSize()) return -1; // guard

    steps = find_sync_word(readPtr, av);
    if(steps == -1) return -1;
    return steps; // Return the number of steps to the sync word
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
int32_t Audio::mp3_correctResumeFilePos() {

    int32_t        steps = 0;
    int32_t        sumSteps = 0;
    uint8_t*       pos = InBuff.getReadPtr();
    size_t         av = InBuff.getMaxAvailableBytes();

    if(av < InBuff.getMaxBlockSize()) return -1; // guard

    while(true) {
        steps = MP3FindSyncWord(pos, av);
        if(steps == 0)break;
        if(steps == -1) return -1;
        pos += steps;
        sumSteps += steps;
    }
    // AUDIO_LOG_INFO("found sync word at %i  sync1 = 0x%02X, sync2 = 0x%02X", readPtr - pos, *readPtr, *(readPtr + 1));
    return sumSteps; // return the position of the first byte of the frame
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t Audio::determineOggCodec(uint8_t* data, uint16_t len) {
    // if we have contentType == application/ogg; codec cn be OPUS, FLAC or VORBIS
    // let's have a look, what it is
    int idx = specialIndexOf(data, "OggS", 6);
    if(idx != 0) {
        if(specialIndexOf(data, "fLaC", 6)) return CODEC_FLAC;
        return CODEC_NONE;
    }
    data += 27;
    idx = specialIndexOf(data, "OpusHead", 40);
    if(idx >= 0) { return CODEC_OPUS; }
    idx = specialIndexOf(data, "fLaC", 40);
    if(idx >= 0) { return CODEC_FLAC; }
    idx = specialIndexOf(data, "vorbis", 40);
    if(idx >= 0) { return CODEC_VORBIS; }
    return CODEC_NONE;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// separate task for decoding and outputting the data. 'playAudioData()' is started periodically and fetches the data from the InBuffer. This ensures
// that the I2S-DMA is always sufficiently filled, even if the Arduino 'loop' is stuck.
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void Audio::setAudioTaskCore(uint8_t coreID){  // Recommendation:If the ARDUINO RUNNING CORE is 1, the audio task should be core 0 or vice versa
    if(coreID > 1) return;
    stopAudioTask();
    xSemaphoreTake(mutex_audioTask, 0.3 * configTICK_RATE_HZ);
    m_audioTaskCoreId = coreID;
    xSemaphoreGive(mutex_audioTask);
    startAudioTask();
}

void Audio::startAudioTask() {
    if (m_f_audioTaskIsRunning) {
        AUDIO_LOG_INFO("Task is already running.");
        return;
    }
    m_f_audioTaskIsRunning = true;


    m_audioTaskHandle = xTaskCreateStaticPinnedToCore(
        &Audio::taskWrapper,    /* Function to implement the task */
        "PeriodicTask",         /* Name of the task */
        AUDIO_STACK_SIZE,       /* Stack size in words */
        this,                   /* Task input parameter */
        2,                      /* Priority of the task */
        xAudioStack,            /* Task stack */
        &xAudioTaskBuffer,      /* Memory for the task's control block */
        m_audioTaskCoreId       /* Core where the task should run */
    );
}

void Audio::stopAudioTask()  {
    if (!m_f_audioTaskIsRunning) {
        AUDIO_LOG_INFO("audio task is not running.");
        return;
    }
    xSemaphoreTake(mutex_audioTask, 0.3 * configTICK_RATE_HZ);
    m_f_audioTaskIsRunning = false;
    if (m_audioTaskHandle != nullptr) {
        vTaskDelete(m_audioTaskHandle);
        m_audioTaskHandle = nullptr;
    }
    xSemaphoreGive(mutex_audioTask);
}

void Audio::taskWrapper(void *param) {
    Audio *runner = static_cast<Audio*>(param);
    runner->audioTask();
}

void Audio::audioTask() {
    while (m_f_audioTaskIsRunning) {
        vTaskDelay(1 / portTICK_PERIOD_MS);  // periodically every x ms
        performAudioTask();
    }
    vTaskDelete(nullptr);  // Delete this task
}

void Audio::performAudioTask() {
    if(!m_f_running) return;
    if(!m_f_stream) return;
    if(m_codec == CODEC_NONE) return; // wait for codec is  set
    if(m_codec == CODEC_OGG)  return; // wait for FLAC, VORBIS or OPUS
    xSemaphoreTake(mutex_audioTask, 0.3 * configTICK_RATE_HZ);
    while(m_validSamples) {vTaskDelay(20 / portTICK_PERIOD_MS); playChunk();} // I2S buffer full
    playAudioData();
    xSemaphoreGive(mutex_audioTask);
}
uint32_t Audio::getHighWatermark(){
    UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(m_audioTaskHandle);
    return highWaterMark; // dwords
}
