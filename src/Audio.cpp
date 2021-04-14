/*
 * Audio.cpp
 *
 *  Created on: Oct 26,2018
 *  Updated on: Apr 10,2021
 *      Author: Wolle (schreibfaul1)   ¯\_(ツ)_/¯
 *
 *  This library plays mp3 files from SD card or icy-webstream  via I2S,
 *  play Google TTS and plays also aac-streams
 *
 *  etrernal HW on I2S nessesary, e.g.MAX98357A
 *
 */

#include "Audio.h"
#include "mp3_decoder/mp3_decoder.h"
#include "aac_decoder/aac_decoder.h"
#include "flac_decoder/flac_decoder.h"

//---------------------------------------------------------------------------------------------------------------------

///
/// @brief Construct a new Audio Buffer:: Audio Buffer object
///
/// @param maxBlockSize Maximum bloc size in bytes (1600 by default).
AudioBuffer::AudioBuffer(size_t maxBlockSize)
{
    // if maxBlockSize isn't set use defaultspace (1600 bytes) is enough for aac and mp3 player
    if (maxBlockSize)
        m_resBuffSize = maxBlockSize;
    if (maxBlockSize)
        m_maxBlockSize = maxBlockSize;
}

///
/// @brief Destroy the Audio Buffer:: Audio Buffer object
///
AudioBuffer::~AudioBuffer()
{
    if (m_buffer)
        free(m_buffer);
    m_buffer = NULL;
}

size_t AudioBuffer::init()
{
    if (m_buffer)
        free(m_buffer);
    m_buffer = NULL;
    if (psramInit())
    {
        // PSRAM found, AudioBuffer will be allocated in PSRAM
        m_buffSize = m_buffSizePSRAM;
        if (m_buffer == NULL)
        {
            m_buffer = (uint8_t *)ps_calloc(m_buffSize, sizeof(uint8_t));
            m_buffSize = m_buffSizePSRAM - m_resBuffSize;
            if (m_buffer == NULL)
            {
                // not enough space in PSRAM, use ESP32 Flash Memory instead
                m_buffer = (uint8_t *)calloc(m_buffSize, sizeof(uint8_t));
                m_buffSize = m_buffSizeRAM - m_resBuffSize;
            }
        }
    }
    else
    { // no PSRAM available, use ESP32 Flash Memory"
        m_buffSize = m_buffSizeRAM;
        m_buffer = (uint8_t *)calloc(m_buffSize, sizeof(uint8_t));
        m_buffSize = m_buffSizeRAM - m_resBuffSize;
    }
    if (!m_buffer)
        return 0;
    resetBuffer();
    return m_buffSize;
}

void AudioBuffer::changeMaxBlockSize(uint16_t mbs)
{
    m_resBuffSize = mbs;
    m_maxBlockSize = mbs;
    if (psramFound())
        m_buffSize = m_buffSizePSRAM - m_resBuffSize;
    else
        m_buffSize = m_buffSizeRAM - m_resBuffSize;
    init();
    return;
}

uint16_t AudioBuffer::getMaxBlockSize()
{
    return m_maxBlockSize;
}

size_t AudioBuffer::freeSpace()
{
    if (m_readPtr >= m_writePtr)
    {
        m_freeSpace = (m_readPtr - m_writePtr);
    }
    else
    {
        m_freeSpace = (m_endPtr - m_writePtr) + (m_readPtr - m_buffer);
    }
    if (m_f_start)
        m_freeSpace = m_buffSize;
    return m_freeSpace - 1;
}

size_t AudioBuffer::writeSpace()
{
    if (m_readPtr >= m_writePtr)
    {
        m_writeSpace = (m_readPtr - m_writePtr - 1); // readPtr must not be overtaken
    }
    else
    {
        if (getReadPos() == 0)
            m_writeSpace = (m_endPtr - m_writePtr - 1);
        else
            m_writeSpace = (m_endPtr - m_writePtr);
    }
    if (m_f_start)
        m_writeSpace = m_buffSize - 1;
    return m_writeSpace;
}

size_t AudioBuffer::bufferFilled()
{
    if (m_writePtr >= m_readPtr)
    {
        m_dataLength = (m_writePtr - m_readPtr);
    }
    else
    {
        m_dataLength = (m_endPtr - m_readPtr) + (m_writePtr - m_buffer);
    }
    return m_dataLength;
}

void AudioBuffer::bytesWritten(size_t bw)
{
    m_writePtr += bw;
    if (m_writePtr == m_endPtr)
    {
        m_writePtr = m_buffer;
    }
    if (bw && m_f_start)
        m_f_start = false;
}

void AudioBuffer::bytesWasRead(size_t br)
{
    m_readPtr += br;
    if (m_readPtr >= m_endPtr)
    {
        size_t tmp = m_readPtr - m_endPtr;
        m_readPtr = m_buffer + tmp;
    }
}

uint8_t *AudioBuffer::getWritePtr()
{
    return m_writePtr;
}

uint8_t *AudioBuffer::getReadPtr()
{
    size_t len = m_endPtr - m_readPtr;
    if (len < m_resBuffSize)
    {                                                    // be sure the last frame is completed
        memcpy(m_endPtr, m_buffer, m_resBuffSize - len); // cpy from m_buffer to m_endPtr with len
    }
    return m_readPtr;
}

void AudioBuffer::resetBuffer()
{
    m_writePtr = m_buffer;
    m_readPtr = m_buffer;
    m_endPtr = m_buffer + m_buffSize;
    m_f_start = true;
    // memset(m_buffer, 0, m_buffSize); //Clear Inputbuffer
}

uint32_t AudioBuffer::getWritePos()
{
    return m_writePtr - m_buffer;
}

uint32_t AudioBuffer::getReadPos()
{
    return m_readPtr - m_buffer;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief Construct a new Audio instance
///
Audio::Audio()
{
    clientsecure.setInsecure(); // if that can't be resolved update to ESP32 Arduino version 1.0.5-rc05 or higher
    //i2s configuration
    m_i2s_num = I2S_NUM_0; // i2s port number
    m_i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    m_i2s_config.sample_rate = 16000;
    m_i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    m_i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
    //m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S);
    m_i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1; // high interrupt priority
    m_i2s_config.dma_buf_count = 8;                       // max buffers
    m_i2s_config.dma_buf_len = 1024;                      // max value
    m_i2s_config.use_apll = APLL_ENABLE;
    m_i2s_config.tx_desc_auto_clear = true; // new in V1.0.1
    m_i2s_config.fixed_mclk = I2S_PIN_NO_CHANGE;

    i2s_driver_install((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL);

    m_f_forceMono = false;

    for (int i = 0; i < 3; i++)
    {
        m_filter[i].a0 = 1;
        m_filter[i].a1 = 0;
        m_filter[i].a2 = 0;
        m_filter[i].b1 = 0;
        m_filter[i].b2 = 0;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::initInBuff()
{
    static bool f_already_done = false;
    if (!f_already_done)
    {
        size_t size = InBuff.init();
        if (size == m_buffSizeRAM - m_resBuffSize)
        {
            sprintf(chbuf, "PSRAM not found, inputBufferSize: %u bytes", size - 1);
            if (audio_info)
                audio_info(chbuf);
            m_f_psram = false;
            f_already_done = true;
        }
        if (size == m_buffSizePSRAM - m_resBuffSize)
        {
            sprintf(chbuf, "PSRAM found, inputBufferSize: %u bytes", size - 1);
            if (audio_info)
                audio_info(chbuf);
            m_f_psram = true;
            f_already_done = true;
        }
    }
    changeMaxBlockSize(1600); // default size mp3 or aac
}
//---------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::I2Sstart(uint8_t i2s_num)
{
    // It is not necessary to call this function after i2s_driver_install() (it is started automatically),
    // however it is necessary to call it after i2s_stop()
    return i2s_start((i2s_port_t)i2s_num);
}

esp_err_t Audio::I2Sstop(uint8_t i2s_num)
{
    return i2s_stop((i2s_port_t)i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set current master clock pin.
///
/// @param pin Master clock pin (only 0, 1 or 3 is supported).
/// @return ESP_ERR_INVALID_ARG if it failed.
/// @return ESP_OK if it succeed.
esp_err_t Audio::i2s_mclk_pin_select(const uint8_t pin)
{
    if (pin != 0 && pin != 1 && pin != 3)
    {
        ESP_LOGE(TAG, "Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", pin);
        return ESP_ERR_INVALID_ARG;
    }
    switch (pin)
    {
    case 0:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
        WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
        break;
    case 1:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
        WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
        break;
    case 3:
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
        WRITE_PERI_REG(PIN_CTRL, 0xFF00);
        break;
    default:
        break;
    }
    return ESP_OK;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief Destroy an Audio instance
///
Audio::~Audio()
{
    I2Sstop(m_i2s_num);
    InBuff.~AudioBuffer();
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setDefaults()
{
    stopSong();
    initInBuff(); // initialize InputBuffer if not already done
    InBuff.resetBuffer();
    MP3Decoder_FreeBuffers();
    AACDecoder_FreeBuffers();
    FLACDecoder_FreeBuffers();
    client.stop();
    client.flush(); // release memory
    clientsecure.stop();
    clientsecure.flush();

    sprintf(chbuf, "buffers freed, free Heap: %u bytes", ESP.getFreeHeap());
    if (audio_info)
        audio_info(chbuf);

    m_f_chunked = false; // Assume not chunked
    m_f_ctseen = false;  // Contents type not seen yet
    m_f_firstmetabyte = false;
    m_f_firststream_ready = false;
    m_f_localfile = false; // SPIFFS or SD? (onnecttoFS)
    m_f_playing = false;
    m_f_ssl = false;
    m_f_stream = false;
    m_f_swm = true;      // Assume no metaint (stream without metadata)
    m_f_webfile = false; // Assume radiostream (connecttohost)
    m_f_webstream = false;
    m_f_firstCall = true; // InitSequence for processWebstream and processLokalFile
    m_f_running = false;
    m_f_loop = false;   // Set if audio file should loop
    m_f_unsync = false; // set within ID3 tag but not used
    m_f_exthdr = false; // ID3 extended header

    m_playlistFormat = FORMAT_NONE;
    m_id3Size = 0;
    m_wavHeaderSize = 0;
    m_audioCurrentTime = 0; // Reset playtimer
    m_audioFileDuration = 0;
    m_audioDataStart = 0;
    m_audioDataSize = 0;
    m_avr_bitrate = 0;     // the same as m_bitrate if CBR, median if VBR
    m_bitRate = 0;         // Bitrate still unknown
    m_bytesNotDecoded = 0; // counts all not decodable bytes
    m_chunkcount = 0;      // for chunked streams
    m_codec = CODEC_NONE;
    m_contentlength = 0; // If Content-Length is known, count it
    m_curSample = 0;
    m_metaCount = 0;      // count bytes between metadata
    m_metaint = 0;        // No metaint yet
    m_LFcount = 0;        // For end of header detection
    m_st_remember = 0;    // Delete the last streamtitle hash
    m_totalcount = 0;     // Reset totalcount
    m_controlCounter = 0; // Status within readID3data() and readWaveHeader()
    m_channels = 0;

    //TEST loop
    m_file_size = 0;
    //TEST loop
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that connect to an host that provide audio stream.
///
/// @param host Host URL.
/// @param user Host username if the host require authentification (empty string by default).
/// @param pwd Host password if the host require authentification (empty string by default).
/// @return true if it succeed to connect to host.
/// @return false if it failed to connect to host.
bool Audio::connecttohost(const char *host, const char *user, const char *pwd)
{
    // user and pwd for authentification only, can be empty

    if (strlen(host) == 0)
    {
        if (audio_info)
            audio_info("Hostaddress is empty");
        return false;
    }
    setDefaults();

    sprintf(chbuf, "Connect to new host: \"%s\"", host);
    if (audio_info)
        audio_info(chbuf);

    // authentification
    String toEncode = String(user) + ":" + String(pwd);
    String authorization = base64::encode(toEncode);

    // initializationsequence
    int16_t pos_colon;      // Position of ":" in hostname
    int16_t pos_ampersand;  // Position of "&" in hostname
    uint16_t port = 80;     // Port number for host
    String extension = "/"; // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext = "";  // Host without extension and portnumber
    String headerdata = "";
    m_f_webstream = true;
    setDatamode(AUDIO_HEADER); // Handle header

    if (startsWith(host, "http://"))
    {
        host = host + 7;
        m_f_ssl = false;
    }

    if (startsWith(host, "https://"))
    {
        host = host + 8;
        m_f_ssl = true;
        port = 443;
    }

    String s_host = host;

    // Is it a playlist?
    if (s_host.endsWith(".m3u"))
    {
        m_playlistFormat = FORMAT_M3U;
        m_datamode = AUDIO_PLAYLISTINIT;
    }
    if (s_host.endsWith(".pls"))
    {
        m_playlistFormat = FORMAT_PLS;
        m_datamode = AUDIO_PLAYLISTINIT;
    }
    if (s_host.endsWith(".asx"))
    {
        m_playlistFormat = FORMAT_ASX;
        m_datamode = AUDIO_PLAYLISTINIT;
    }

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    pos_colon = s_host.indexOf("/"); // Search for begin of extension
    if (pos_colon > 0)
    {                                               // Is there an extension?
        extension = s_host.substring(pos_colon);    // Yes, change the default
        hostwoext = s_host.substring(0, pos_colon); // Host without extension
    }
    // In the URL there may be a portnumber
    pos_colon = s_host.indexOf(":");     // Search for separator
    pos_ampersand = s_host.indexOf("&"); // Search for additional extensions
    if (pos_colon >= 0)
    { // Portnumber available?
        if ((pos_ampersand == -1) or (pos_ampersand > pos_colon))
        {                                                   // Portnumber is valid if ':' comes before '&' #82
            port = s_host.substring(pos_colon + 1).toInt(); // Get portnumber as integer
            hostwoext = s_host.substring(0, pos_colon);     // Host without portnumber
        }
    }
    sprintf(chbuf, "Connect to \"%s\" on port %d, extension \"%s\"", hostwoext.c_str(), port, extension.c_str());
    if (audio_info)
        audio_info(chbuf);

    extension.replace(" ", "%20");

    String resp = String("GET ") + extension + String(" HTTP/1.1\r\n") + String("Host: ") + hostwoext + String("\r\n") + String("Icy-MetaData:1\r\n") + String("Authorization: Basic " + authorization + "\r\n") + String("Connection: close\r\n\r\n");

    const uint32_t TIMEOUT_MS{250};
    if (m_f_ssl == false)
    {
        uint32_t t = millis();
        if (client.connect(hostwoext.c_str(), port, TIMEOUT_MS))
        {
            client.setNoDelay(true);
            client.print(resp);
            uint32_t dt = millis() - t;
            sprintf(chbuf, "Connected to server in %u ms", dt);
            if (audio_info)
                audio_info(chbuf);

            memcpy(m_lastHost, s_host.c_str(), s_host.length() + 1); // Remember the current s_host
            m_f_running = true;
            return true;
        }
    }

    const uint32_t TIMEOUT_MS_SSL{2500};
    if (m_f_ssl == true)
    {
        uint32_t t = millis();
        if (clientsecure.connect(hostwoext.c_str(), port, TIMEOUT_MS_SSL))
        {
            clientsecure.setNoDelay(true);
            // if(audio_info) audio_info("SSL/TLS Connected to server");
            clientsecure.print(resp);
            uint32_t dt = millis() - t;
            sprintf(chbuf, "SSL has been established in %u ms, free Heap: %u bytes", dt, ESP.getFreeHeap());
            if (audio_info)
                audio_info(chbuf);
            memcpy(m_lastHost, s_host.c_str(), s_host.length() + 1); // Remember the current s_host
            m_f_running = true;
            return true;
        }
    }
    sprintf(chbuf, "Request %s failed!", s_host.c_str());
    if (audio_info)
        audio_info(chbuf);
    if (audio_showstation)
        audio_showstation("");
    if (audio_showstreamtitle)
        audio_showstreamtitle("");
    m_lastHost[0] = 0;
    return false;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set loop on current file.
///
/// @param input true to enable loop, false to disable it.
/// @return true if it succeed to change loop state.
/// @return false if it failed to change loop state.
bool Audio::setFileLoop(bool input)
{
    m_f_loop = input;
    return input;
}
//---------------------------------------------------------------------------------------------------------------------
///
/// @brief A method that play audio file on SD (SPI).
///
/// @param sdfile File path.
/// @return true if it succeed to play audio file.
/// @return false if it failed to play audio file.
bool Audio::connecttoSD(const char *sdfile)
{
    return connecttoFS(SD, sdfile);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that play file from custom file system (SD_MMC, SPIFFS ...).
///
/// @param fs Targetted file system.
/// @param file Path of file to play.
/// @return true if it succeed to play file.
/// @return false if it failed to play file.
bool Audio::connecttoFS(fs::FS &fs, const char *file)
{

    String s_file = file;

    const uint8_t ascii[60] = {
        //129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148  // UTF8(C3)
        //                Ä    Å    Æ    Ç         É                                       Ñ                  // CHAR
        000, 000, 000, 142, 143, 146, 128, 000, 144, 000, 000, 000, 000, 000, 000, 000, 165, 000, 000, 000, // ASCII
                                                                                                            //149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168
                                                                                                            //      Ö                             Ü              ß    à                   ä    å    æ         è
        000, 153, 000, 000, 000, 000, 000, 154, 000, 000, 225, 133, 000, 000, 000, 132, 134, 145, 000, 138,
        //169, 170, 171, 172. 173. 174. 175, 176, 177, 179, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188
        //      ê    ë    ì         î    ï         ñ    ò         ô         ö              ù         û    ü
        000, 136, 137, 141, 000, 140, 139, 000, 164, 149, 000, 147, 000, 148, 000, 000, 151, 000, 150, 129};

    setDefaults(); // free buffers an set defaults

    uint16_t i = 0, j = 0, s = 0;
    bool f_C3_seen = false;
    m_f_localfile = true;

    if (!s_file.startsWith("/"))
        s_file = "/" + s_file;
    while (s_file[i] != 0)
    { // convert UTF8 to ASCII
        if (s_file[i] == 195)
        { // C3
            i++;
            f_C3_seen = true;
            continue;
        }
        path[j] = s_file[i];
        if (path[j] > 128 && path[j] < 189 && f_C3_seen == true)
        {
            s = ascii[path[j] - 129];
            if (s != 0)
                path[j] = s; // found a related ASCII sign
            f_C3_seen = false;
        }
        i++;
        j++;
    }
    path[j] = 0;
    memcpy(m_audioName, s_file.c_str() + 1, s_file.length()); // skip the first '/'
    sprintf(chbuf, "Reading file: \"%s\"", m_audioName);
    if (audio_info)
        audio_info(chbuf);

    if (fs.exists(path))
    {
        audiofile = fs.open(path); // #86
    }
    else
    {
        audiofile = fs.open(s_file);
    }

    if (!audiofile)
    {
        if (audio_info)
            audio_info("Failed to open file for reading");
        return false;
    }

    m_file_size = audiofile.size(); //TEST loop

    String afn = (String)audiofile.name(); // audioFileName
    afn.toLowerCase();
    if (afn.endsWith(".mp3"))
    { // MP3 section
        m_codec = CODEC_MP3;
        if (!MP3Decoder_AllocateBuffers())
        {
            audiofile.close();
            return false;
        }
        InBuff.changeMaxBlockSize(m_frameSizeMP3);
        sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
        if (audio_info)
            audio_info(chbuf);
        m_f_running = true;
        return true;
    } // end MP3 section

    if (afn.endsWith(".m4a"))
    { // M4A section, iTunes
        m_codec = CODEC_M4A;
        if (!AACDecoder_AllocateBuffers())
        {
            audiofile.close();
            return false;
        }
        InBuff.changeMaxBlockSize(m_frameSizeAAC);
        sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
        if (audio_info)
            audio_info(chbuf);
        m_f_running = true;
        return true;
    } // end M4A section

    if (afn.endsWith(".aac"))
    { // AAC section, without FileHeader
        m_codec = CODEC_AAC;
        if (!AACDecoder_AllocateBuffers())
        {
            audiofile.close();
            return false;
        }
        InBuff.changeMaxBlockSize(m_frameSizeAAC);
        sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
        if (audio_info)
            audio_info(chbuf);
        m_f_running = true;
        return true;
    } // end AAC section

    if (afn.endsWith(".wav"))
    { // WAVE section
        m_codec = CODEC_WAV;
        InBuff.changeMaxBlockSize(m_frameSizeWav);
        m_f_running = true;
        return true;
    } // end WAVE section

    if (afn.endsWith(".flac"))
    { // FLAC section
        if (!psramFound())
        {
            if (audio_info)
                audio_info("FLAC works only with PSRAM!");
            audiofile.close();
            return false;
        }
        m_codec = CODEC_FLAC;
        if (!FLACDecoder_AllocateBuffers())
        {
            audiofile.close();
            return false;
        }
        InBuff.changeMaxBlockSize(m_frameSizeFLAC);
        sprintf(chbuf, "FLACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
        if (audio_info)
            audio_info(chbuf);
        m_f_running = true;
        return true;
    } // end FLAC section

    sprintf(chbuf, "The %s format is not supported", afn.c_str() + afn.lastIndexOf(".") + 1);
    if (audio_info)
        audio_info(chbuf);
    audiofile.close();
    return false;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that play speech generated by Google TTS.
///
/// @param speech Speech to play.
/// @param lang Language of speech.
/// @return true if it succeed to play speech.
/// @return false if it failed to play speech.
bool Audio::connecttospeech(const char *speech, const char *lang)
{

    setDefaults();
    String host = "translate.google.com.vn";
    String path = "/translate_tts";
    uint32_t bytesCanBeWritten = 0;
    uint32_t bytesCanBeRead = 0;
    int32_t bytesAddedToBuffer = 0;
    int16_t bytesDecoded = 0;
    uint32_t contentLength = 0;

    String tts = path + "?ie=UTF-8&q=" + urlencode(speech) +
                 "&tl=" + lang + "&client=tw-ob";

    String resp = String("GET ") + tts + String(" HTTP/1.1\r\n") + String("Host: ") + host + String("\r\n") + String("User-Agent: GoogleTTS for ESP32/1.0.0\r\n") + String("Accept-Encoding: identity\r\n") + String("Accept: text/html\r\n") + String("Connection: close\r\n\r\n");

    if (!clientsecure.connect(host.c_str(), 443))
    {
        log_e("Connection failed");
        return false;
    }
    clientsecure.print(resp);
    sprintf(chbuf, "SSL has been established, free Heap: %u bytes", ESP.getFreeHeap());
    if (audio_info)
        audio_info(chbuf);
    while (clientsecure.connected())
    { // read the header
        String line = clientsecure.readStringUntil('\n');
        if (line.startsWith("Content-Length:"))
        { // content length seen
            m_contentlength = line.substring(15).toInt();
            sprintf(chbuf, "Content-Length: %u bytes", m_contentlength);
            if (audio_info)
                audio_info(chbuf);
            m_codec = CODEC_MP3;
            AACDecoder_FreeBuffers();
            if (!MP3Decoder_AllocateBuffers())
                return false;
            sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            if (audio_info)
                audio_info(chbuf);
        }
        line += "\n";
        if (line == "\r\n")
            break;
    }
    if (!m_contentlength)
        return false;

    while (!playI2Sremains())
    {
        ;
    }
    while (clientsecure.available() == 0)
    {
        ;
    }

    while (contentLength < m_contentlength)
    {
        bytesCanBeWritten = InBuff.writeSpace();
        bytesAddedToBuffer = clientsecure.read(InBuff.getWritePtr(), bytesCanBeWritten);
        contentLength += bytesAddedToBuffer;
        if (bytesAddedToBuffer > 0)
            InBuff.bytesWritten(bytesAddedToBuffer);
        bytesCanBeRead = InBuff.bufferFilled();
        if (bytesCanBeRead > InBuff.getMaxBlockSize())
            bytesCanBeRead = InBuff.getMaxBlockSize();
        if (bytesCanBeRead)
        {
            while (InBuff.bufferFilled() >= InBuff.getMaxBlockSize())
            { // mp3 frame complete?
                bytesDecoded = sendBytes(InBuff.getReadPtr(), InBuff.bufferFilled());
                InBuff.bytesWasRead(bytesDecoded);
            }
        }
    }
    while (InBuff.bufferFilled())
    {
        bytesDecoded = sendBytes(InBuff.getReadPtr(), InBuff.bufferFilled());
        InBuff.bytesWasRead(bytesDecoded);
    }

    while (!playI2Sremains())
    {
        ;
    }
    MP3Decoder_FreeBuffers();
    stopSong();
    clientsecure.stop();
    clientsecure.flush();
    m_codec = CODEC_NONE;
    if (audio_eof_speech)
        audio_eof_speech(speech);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
String Audio::urlencode(String str)
{
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++)
    {
        c = str.charAt(i);
        if (c == ' ')
            encodedString += '+';
        else if (isalnum(c))
            encodedString += c;
        else
        {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9)
                code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9)
                code0 = c - 10 + 'A';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    return encodedString;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showID3Tag(String tag, const char *value)
{

    chbuf[0] = 0;
    // V2.2
    if (tag == "CNT")
        sprintf(chbuf, "Play counter: %s", value);
    // if(tag == "COM") sprintf(chbuf, "Comments: %s", value);
    if (tag == "CRA")
        sprintf(chbuf, "Audio encryption: %s", value);
    if (tag == "CRM")
        sprintf(chbuf, "Encrypted meta frame: %s", value);
    if (tag == "ETC")
        sprintf(chbuf, "Event timing codes: %s", value);
    if (tag == "EQU")
        sprintf(chbuf, "Equalization: %s", value);
    if (tag == "IPL")
        sprintf(chbuf, "Involved people list: %s", value);
    if (tag == "PIC")
        sprintf(chbuf, "Attached picture: %s", value);
    if (tag == "SLT")
        sprintf(chbuf, "Synchronized lyric/text: %s", value);
    // if(tag == "TAL") sprintf(chbuf, "Album/Movie/Show title: %s", value);
    if (tag == "TBP")
        sprintf(chbuf, "BPM (Beats Per Minute): %s", value);
    if (tag == "TCM")
        sprintf(chbuf, "Composer: %s", value);
    if (tag == "TCO")
        sprintf(chbuf, "Content type: %s", value);
    if (tag == "TCR")
        sprintf(chbuf, "Copyright message: %s", value);
    if (tag == "TDA")
        sprintf(chbuf, "Date: %s", value);
    if (tag == "TDY")
        sprintf(chbuf, "Playlist delay: %s", value);
    if (tag == "TEN")
        sprintf(chbuf, "Encoded by: %s", value);
    if (tag == "TFT")
        sprintf(chbuf, "File type: %s", value);
    if (tag == "TIM")
        sprintf(chbuf, "Time: %s", value);
    if (tag == "TKE")
        sprintf(chbuf, "Initial key: %s", value);
    if (tag == "TLA")
        sprintf(chbuf, "Language(s): %s", value);
    if (tag == "TLE")
        sprintf(chbuf, "Length: %s", value);
    if (tag == "TMT")
        sprintf(chbuf, "Media type: %s", value);
    if (tag == "TOA")
        sprintf(chbuf, "Original artist(s)/performer(s): %s", value);
    if (tag == "TOF")
        sprintf(chbuf, "Original filename: %s", value);
    if (tag == "TOL")
        sprintf(chbuf, "Original Lyricist(s)/text writer(s): %s", value);
    if (tag == "TOR")
        sprintf(chbuf, "Original release year: %s", value);
    if (tag == "TOT")
        sprintf(chbuf, "Original album/Movie/Show title: %s", value);
    if (tag == "TP1")
        sprintf(chbuf, "Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group: %s", value);
    if (tag == "TP2")
        sprintf(chbuf, "Band/Orchestra/Accompaniment: %s", value);
    if (tag == "TP3")
        sprintf(chbuf, "Conductor/Performer refinement: %s", value);
    if (tag == "TP4")
        sprintf(chbuf, "Interpreted, remixed, or otherwise modified by: %s", value);
    if (tag == "TPA")
        sprintf(chbuf, "Part of a set: %s", value);
    if (tag == "TPB")
        sprintf(chbuf, "Publisher: %s", value);
    if (tag == "TRC")
        sprintf(chbuf, "ISRC (International Standard Recording Code): %s", value);
    if (tag == "TRD")
        sprintf(chbuf, "Recording dates: %s", value);
    if (tag == "TRK")
        sprintf(chbuf, "Track number/Position in set: %s", value);
    if (tag == "TSI")
        sprintf(chbuf, "Size: %s", value);
    if (tag == "TSS")
        sprintf(chbuf, "Software/hardware and settings used for encoding: %s", value);
    if (tag == "TT1")
        sprintf(chbuf, "Content group description: %s", value);
    if (tag == "TT2")
        sprintf(chbuf, "Title/Songname/Content description: %s", value);
    if (tag == "TT3")
        sprintf(chbuf, "Subtitle/Description refinement: %s", value);
    if (tag == "TXT")
        sprintf(chbuf, "Lyricist/text writer: %s", value);
    if (tag == "TXX")
        sprintf(chbuf, "User defined text information frame: %s", value);
    if (tag == "TYE")
        sprintf(chbuf, "Year: %s", value);
    if (tag == "UFI")
        sprintf(chbuf, "Unique file identifier: %s", value);
    if (tag == "ULT")
        sprintf(chbuf, "Unsychronized lyric/text transcription: %s", value);
    if (tag == "WAF")
        sprintf(chbuf, "Official audio file webpage: %s", value);
    if (tag == "WAR")
        sprintf(chbuf, "Official artist/performer webpage: %s", value);
    if (tag == "WAS")
        sprintf(chbuf, "Official audio source webpage: %s", value);
    if (tag == "WCM")
        sprintf(chbuf, "Commercial information: %s", value);
    if (tag == "WCP")
        sprintf(chbuf, "Copyright/Legal information: %s", value);
    if (tag == "WPB")
        sprintf(chbuf, "Publishers official webpage: %s", value);
    if (tag == "WXX")
        sprintf(chbuf, "User defined URL link frame: %s", value);

    // V2.3 V2.4 tags
    // if(tag == "COMM") sprintf(chbuf, "Comment: %s", value);
    if (tag == "OWNE")
        sprintf(chbuf, "Ownership: %s", value);
    // if(tag == "PRIV") sprintf(chbuf, "Private: %s", value);
    if (tag == "SYLT")
        sprintf(chbuf, "SynLyrics: %s", value);
    if (tag == "TALB")
        sprintf(chbuf, "Album: %s", value);
    if (tag == "TBPM")
        sprintf(chbuf, "BeatsPerMinute: %s", value);
    if (tag == "TCMP")
        sprintf(chbuf, "Compilation: %s", value);
    if (tag == "TCOM")
        sprintf(chbuf, "Composer: %s", value);
    if (tag == "TCON")
        sprintf(chbuf, "ContentType: %s", value);
    if (tag == "TCOP")
        sprintf(chbuf, "Copyright: %s", value);
    if (tag == "TDAT")
        sprintf(chbuf, "Date: %s", value);
    if (tag == "TEXT")
        sprintf(chbuf, "Lyricist: %s", value);
    if (tag == "TIME")
        sprintf(chbuf, "Time: %s", value);
    if (tag == "TIT1")
        sprintf(chbuf, "Grouping: %s", value);
    if (tag == "TIT2")
        sprintf(chbuf, "Title: %s", value);
    if (tag == "TIT3")
        sprintf(chbuf, "Subtitle: %s", value);
    if (tag == "TLAN")
        sprintf(chbuf, "Language: %s", value);
    if (tag == "TLEN")
        sprintf(chbuf, "Length (ms): %s", value);
    if (tag == "TMED")
        sprintf(chbuf, "Media: %s", value);
    if (tag == "TOAL")
        sprintf(chbuf, "OriginalAlbum: %s", value);
    if (tag == "TOPE")
        sprintf(chbuf, "OriginalArtist: %s", value);
    if (tag == "TORY")
        sprintf(chbuf, "OriginalReleaseYear: %s", value);
    if (tag == "TPE1")
        sprintf(chbuf, "Artist: %s", value);
    if (tag == "TPE2")
        sprintf(chbuf, "Band: %s", value);
    if (tag == "TPE3")
        sprintf(chbuf, "Conductor: %s", value);
    if (tag == "TPE4")
        sprintf(chbuf, "InterpretedBy: %s", value);
    if (tag == "TPOS")
        sprintf(chbuf, "PartOfSet: %s", value);
    if (tag == "TPUB")
        sprintf(chbuf, "Publisher: %s", value);
    if (tag == "TRCK")
        sprintf(chbuf, "Track: %s", value);
    if (tag == "TSSE")
        sprintf(chbuf, "SettingsForEncoding: %s", value);
    if (tag == "TRDA")
        sprintf(chbuf, "RecordingDates: %s", value);
    if (tag == "TXXX")
        sprintf(chbuf, "UserDefinedText: %s", value);
    if (tag == "TYER")
        sprintf(chbuf, "Year: %s", value);
    if (tag == "USER")
        sprintf(chbuf, "TermsOfUse: %s", value);
    if (tag == "USLT")
        sprintf(chbuf, "Lyrics: %s", value);
    if (tag == "WOAR")
        sprintf(chbuf, "OfficialArtistWebpage: %s", value);
    if (tag == "XDOR")
        sprintf(chbuf, "OriginalReleaseTime: %s", value);

    if (chbuf[0] != 0)
        if (audio_id3data)
            audio_id3data(chbuf);
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::unicode2utf8(char *buff, uint32_t len)
{
    // converts unicode in UTF-8, buff contains the string to be converted up to len
    // pos will be updated after the converting is complete
    bool bitorder = false;
    uint16_t j = 0;
    uint16_t k = 0;
    uint16_t m = 0;
    uint8_t uni_h = 0;
    uint8_t uni_l = 0;

    while (m < len - 1)
    {
        if ((buff[m] == 0xFE) && (buff[m + 1] == 0xFF))
        {
            bitorder = true;
            j = m + 2;
        } // MSB/LSB
        if ((buff[m] == 0xFF) && (buff[m + 1] == 0xFE))
        {
            bitorder = false;
            j = m + 2;
        } //LSB/MSB
        m++;
    } // seek for last bitorder
    m = 0;
    if (j > 0)
    {
        for (k = j; k < len - 1; k += 2)
        {
            if (bitorder == true)
            {
                uni_h = buff[k];
                uni_l = buff[k + 1];
            }
            else
            {
                uni_l = buff[k];
                uni_h = buff[k + 1];
            }
            uint16_t uni_hl = (uni_h << 8) + uni_l;
            uint8_t utf8_h = (uni_hl >> 6); // div64
            uint8_t utf8_l = uni_l;
            if (utf8_h > 3)
            {
                utf8_h += 0xC0;
                if (uni_l < 0x40)
                    utf8_l = uni_l + 0x80;
                else if (uni_l < 0x80)
                    utf8_l = uni_l += 0x40;
                else if (uni_l < 0xC0)
                    utf8_l = uni_l;
                else
                    utf8_l = uni_l - 0x40;
            }
            if (utf8_h > 3)
            {
                buff[m] = utf8_h;
                m++;
            }
            buff[m] = utf8_l;
            m++;
        }
    }
    buff[m] = 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_WAV_Header(uint8_t *data, size_t len)
{
    static uint32_t cs = 0;
    static uint8_t bts = 0;

    if (m_controlCounter == 0)
    {
        m_controlCounter++;
        if ((*data != 'R') || (*(data + 1) != 'I') || (*(data + 2) != 'F') || (*(data + 3) != 'F'))
        {
            if (audio_info)
                audio_info("file has no RIFF tag");
            m_wavHeaderSize = 0;
            return -1; //false;
        }
        else
        {
            m_wavHeaderSize = 4;
            return 4; // ok
        }
    }

    if (m_controlCounter == 1)
    {
        m_controlCounter++;
        cs = (uint32_t)(*data + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24) - 8);
        m_wavHeaderSize += 4;
        return 4; // ok
    }

    if (m_controlCounter == 2)
    {
        m_controlCounter++;
        if ((*data != 'W') || (*(data + 1) != 'A') || (*(data + 2) != 'V') || (*(data + 3) != 'E'))
        {
            if (audio_info)
                audio_info("format tag is not WAVE");
            return -1; //false;
        }
        else
        {
            m_wavHeaderSize += 4;
            return 4;
        }
    }

    if (m_controlCounter == 3)
    {
        if ((*data == 'f') && (*(data + 1) == 'm') && (*(data + 2) == 't'))
        {
            //if(audio_info) audio_info("format tag found");
            m_controlCounter++;
            m_wavHeaderSize += 4;
            return 4;
        }
        else
        {
            m_wavHeaderSize += 4;
            return 4;
        }
    }

    if (m_controlCounter == 4)
    {
        m_controlCounter++;
        cs = (uint32_t)(*data + (*(data + 1) << 8));
        if (cs > 40)
            return -1; //false, something going wrong
        bts = cs - 16; // bytes to skip if fmt chunk is >16
        m_wavHeaderSize += 4;
        return 4;
    }

    if (m_controlCounter == 5)
    {
        m_controlCounter++;
        uint16_t fc = (uint16_t)(*(data + 0) + (*(data + 1) << 8));  // Format code
        uint16_t nic = (uint16_t)(*(data + 2) + (*(data + 3) << 8)); // Number of interleaved channels
        uint32_t sr = (uint32_t)(*(data + 4) + (*(data + 5) << 8) +
                                 (*(data + 6) << 16) + (*(data + 7) << 24)); // Samplerate
        uint32_t dr = (uint32_t)(*(data + 8) + (*(data + 9) << 8) +
                                 (*(data + 10) << 16) + (*(data + 11) << 24)); // Datarate
        uint16_t dbs = (uint16_t)(*(data + 12) + (*(data + 13) << 8));         // Data block size
        uint16_t bps = (uint16_t)(*(data + 14) + (*(data + 15) << 8));         // Bits per sample
        if (audio_info)
        {
            sprintf(chbuf, "FormatCode: %u", fc);
            audio_info(chbuf);
            //    sprintf(chbuf, "Channel: %u", nic);       audio_info(chbuf);
            //    sprintf(chbuf, "SampleRate: %u", sr);     audio_info(chbuf);
            sprintf(chbuf, "DataRate: %u", dr);
            audio_info(chbuf);
            sprintf(chbuf, "DataBlockSize: %u", dbs);
            audio_info(chbuf);
            //    sprintf(chbuf, "BitsPerSample: %u", bps); audio_info(chbuf);
        }
        if (fc != 1)
        {
            if (audio_info)
                audio_info("format code is not 1 (PCM)");
            return -1; //false;
        }

        if (nic != 1 && nic != 2)
        {
            if (audio_info)
                audio_info("number of channels must be 1 or 2");
            return -1; //false;
        }

        if (bps != 8 && bps != 16)
        {
            if (audio_info)
                audio_info("bits per sample must be 8 or 16");
            return -1; //false;
        }
        setBitsPerSample(bps);
        setChannels(nic);
        setSampleRate(sr);
        setBitrate(nic * sr * bps);
        //    sprintf(chbuf, "BitRate: %u", m_bitRate);
        //    if(audio_info) audio_info(chbuf);
        m_wavHeaderSize += 16;
        return 16; // ok
    }

    if (m_controlCounter == 6)
    {
        m_controlCounter++;
        m_wavHeaderSize += bts;
        return bts; // skip to data
    }

    if (m_controlCounter == 7)
    {
        if ((*(data + 0) == 'd') && (*(data + 1) == 'a') && (*(data + 2) == 't') && (*(data + 3) == 'a'))
        {
            m_controlCounter++;
            vTaskDelay(30);
            m_wavHeaderSize += 4;
            return 4;
        }
        else
        {
            m_wavHeaderSize++;
            return 1;
        }
    }

    if (m_controlCounter == 8)
    {
        m_controlCounter++;
        size_t cs = *(data + 0) + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24); //read chunkSize
        m_wavHeaderSize += 4;
        if (m_f_localfile)
            m_contentlength = getFileSize();
        if (cs)
        {
            m_audioDataSize = cs - 44;
        }
        else
        { // sometimes there is nothing here
            if (m_f_localfile)
                m_audioDataSize = getFileSize() - m_wavHeaderSize;
            if (m_f_webfile)
                m_audioDataSize = m_contentlength - m_wavHeaderSize;
        }
        sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
        if (audio_info)
            audio_info(chbuf);
        return 4;
    }
    m_controlCounter = 100; // header succesfully read
    m_audioDataStart = m_wavHeaderSize;
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_FLAC_Header(uint8_t *data, size_t len)
{
    static size_t headerSize;
    static size_t retvalue;
    static bool f_lastMetaBlock;

    if (retvalue)
    {
        if (retvalue > len)
        { // if returnvalue > bufferfillsize
            if (len > InBuff.getMaxBlockSize())
                len = InBuff.getMaxBlockSize();
            retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else
        {
            size_t tmp = retvalue;
            retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_BEGIN)
    { // init
        headerSize = 0;
        retvalue = 0;
        m_audioDataStart = 0;
        f_lastMetaBlock = false;
        m_controlCounter = FLAC_MAGIC;
        if (m_f_localfile)
        {
            m_contentlength = getFileSize();
            sprintf(chbuf, "Content-Length: %u", m_contentlength);
            if (audio_info)
                audio_info(chbuf);
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_MAGIC)
    { /* check MAGIC STRING */
        if (specialIndexOf(data, "fLaC", 10) != 0)
        {
            log_e("Magic String 'fLaC' not found in header");
            stopSong();
            return -1;
        }
        //        log_i("Magig String found");
        m_controlCounter = FLAC_MBH;
        headerSize = 4;
        retvalue = 4;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_MBH)
    { /* METADATA_BLOCK_HEADER */
        uint8_t blockType = *data;
        if (!f_lastMetaBlock)
        {
            if (blockType & 128)
            {
                f_lastMetaBlock = true;
            }
            blockType &= 127;
            if (blockType == 0)
                m_controlCounter = FLAC_SINFO;
            if (blockType == 1)
                m_controlCounter = FLAC_PADDING;
            if (blockType == 2)
                m_controlCounter = FLAC_APP;
            if (blockType == 3)
                m_controlCounter = FLAC_SEEK;
            if (blockType == 4)
                m_controlCounter = FLAC_VORBIS;
            if (blockType == 5)
                m_controlCounter = FLAC_CUESHEET;
            if (blockType == 6)
                m_controlCounter = FLAC_PICTURE;
            headerSize += 1;
            retvalue = 1;
            return 0;
        }
        m_controlCounter = FLAC_OKAY;
        m_audioDataStart = headerSize;
        m_audioDataSize = m_contentlength - m_audioDataStart;
        sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
        if (audio_info)
            audio_info(chbuf);
        retvalue = 0;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_SINFO)
    { /* Stream info block */
        size_t l = bigEndian(data, 3);
        vTaskDelay(2);
        m_flacMaxBlockSize = bigEndian(data + 5, 2);
        sprintf(chbuf, "FLAC maxBlockSize: %u", m_flacMaxBlockSize);
        if (audio_info)
            audio_info(chbuf);
        //        vTaskDelay(2);
        //        log_i("minframesize %u", bigEndian(data + 7, 3));
        vTaskDelay(2);
        m_flacMaxFrameSize = bigEndian(data + 10, 3);
        sprintf(chbuf, "FLAC maxFrameSize: %u", m_flacMaxFrameSize);
        if (audio_info)
            audio_info(chbuf);
        if (m_flacMaxFrameSize > InBuff.getMaxBlockSize())
        {
            log_e("FLAC maxFrameSize too large!");
            stopSong();
            return -1;
        }
        //        InBuff.changeMaxBlockSize(m_flacMaxFrameSize);
        vTaskDelay(2);
        uint32_t nextval = bigEndian(data + 13, 3);
        m_flacSampleRate = nextval >> 4;
        sprintf(chbuf, "FLAC sampleRate: %u", m_flacSampleRate);
        if (audio_info)
            audio_info(chbuf);
        vTaskDelay(2);
        m_flacNumChannels = ((nextval & 0x06) >> 1) + 1;
        sprintf(chbuf, "FLAC numChannels: %u", m_flacNumChannels);
        if (audio_info)
            audio_info(chbuf);
        vTaskDelay(2);
        uint8_t bps = (nextval & 0x01) << 4;
        bps += (*(data + 16) >> 4) + 1;
        m_flacBitsPerSample = bps;
        sprintf(chbuf, "FLAC bitsPerSample: %u", m_flacBitsPerSample);
        if (audio_info)
            audio_info(chbuf);
        m_flacTotalSamplesInStream = bigEndian(data + 17, 4);
        sprintf(chbuf, "total samples in stream: %u", m_flacTotalSamplesInStream);
        if (audio_info)
            audio_info(chbuf);
        if (bps != 0 && m_flacTotalSamplesInStream)
        {
            sprintf(chbuf, "audio file duration: %u seconds", m_flacTotalSamplesInStream / m_flacSampleRate);
            if (audio_info)
                audio_info(chbuf);
        }
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_PADDING)
    { /* PADDING */
        size_t l = bigEndian(data, 3);
        //log_i("Metadata PADDING, len = %u", len);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_APP)
    { /* APPLICATION */
        size_t l = bigEndian(data, 3);
        // log_i("Metadata APPLICATION, len = %u", len);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_SEEK)
    { /* SEEKTABLE */
        size_t l = bigEndian(data, 3);
        // log_i("Metadata SEEKTABLE, len = %u", len);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_VORBIS)
    { /* VORBIS COMMENT */ // field names
        const char fn[7][12] = {"TITLE", "VERSION", "ALBUM", "TRACKNUMBER", "ARTIST", "PERFORMER", "GENRE"};
        int offset;
        size_t l = bigEndian(data, 3);

        for (int i = 0; i < 7; i++)
        {
            offset = specialIndexOf(data, fn[i], len);
            if (offset >= 0)
            {
                sprintf(chbuf, "%s: %s", fn[i], data + offset + strlen(fn[i]) + 1);
                chbuf[strlen(chbuf) - 1] = 0;
                if (audio_id3data)
                    audio_id3data(chbuf);
            }
        }
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_CUESHEET)
    { /* CUESHEET */
        size_t l = bigEndian(data, 3);
        // log_i("Metadata CUESHEET, len = %u", len);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == FLAC_PICTURE)
    { /* PICTURE */
        size_t l = bigEndian(data, 3);
        // log_i("Metadata PICTURE, len = %u", len);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_MP3_Header(uint8_t *data, size_t len)
{

    static int id3Size = 0;
    static int ehsz = 0;
    static String tag = "";
    static char frameid[5];
    static size_t framesize = 0;
    static bool compressed = false;
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 0)
    { /* read ID3 tag and ID3 header size */
        if (m_f_localfile)
        {
            m_contentlength = getFileSize();
            sprintf(chbuf, "Content-Length: %u", m_contentlength);
            if (audio_info)
                audio_info(chbuf);
        }
        m_controlCounter++;
        id3Size = 0;
        ehsz = 0;
        if (specialIndexOf(data, "ID3", 4) != 0)
        { // ID3 not found
            if (audio_info)
                audio_info("file has no mp3 tag, skip metadata");
            m_audioDataSize = m_contentlength;
            sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
            if (audio_info)
                audio_info(chbuf);
            return -1; // error, no ID3 signature found
        }
        m_ID3version = *(data + 3);
        switch (m_ID3version)
        {
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
        m_id3Size = bigEndian(data + 6, 4, 7); //  ID3v2 size  4 * %0xxxxxxx (shift left seven times!!)
        m_id3Size += 10;

        // Every read from now may be unsync'd
        sprintf(chbuf, "ID3 framesSize: %i", m_id3Size);
        if (audio_info)
            audio_info(chbuf);

        sprintf(chbuf, "ID3 version: 2.%i", m_ID3version);
        if (audio_info)
            audio_info(chbuf);

        if (m_ID3version == 2)
        {
            m_controlCounter = 10;
        }
        id3Size = m_id3Size;
        id3Size -= 10;

        return 10;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 1)
    { // compute extended header size if exists
        m_controlCounter++;
        if (m_f_exthdr)
        {
            if (audio_info)
                audio_info("ID3 extended header");
            ehsz = bigEndian(data, 4);
            id3Size -= 4;
            ehsz -= 4;
            return 4;
        }
        else
        {
            if (audio_info)
                audio_info("ID3 normal frames");
            return 0;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 2)
    { // skip extended header if exists
        if (ehsz > 256)
        {
            ehsz -= 256;
            id3Size -= 256;
            return 256;
        } // Throw it away
        else
        {
            m_controlCounter++;
            id3Size -= ehsz;
            return ehsz;
        } // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 3)
    { // read a ID3 frame, get the tag
        if (id3Size == 0)
        {
            m_controlCounter = 99;
            return 0;
        }
        m_controlCounter++;
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = *(data + 3);
        frameid[4] = 0;
        tag = frameid;
        id3Size -= 4;
        if (frameid[0] == 0 && frameid[1] == 0 && frameid[2] == 0 && frameid[3] == 0)
        {
            // We're in padding
            m_controlCounter = 98; // all ID3 metadata processed
        }
        return 4;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 4)
    { // get the frame size
        m_controlCounter = 6;

        if (m_ID3version == 4)
        {
            framesize = bigEndian(data, 4, 7); // << 7
        }
        else
        {
            framesize = bigEndian(data, 4); // << 8
        }
        id3Size -= 4;
        uint8_t flag = *(data + 4); // skip 1st flag
        (void)flag;
        id3Size--;
        compressed = (*(data + 5)) & 0x80; // Frame is compressed using [#ZLIB zlib] with 4 bytes for 'decompressed
                                           // size' appended to the frame header.
        id3Size--;
        uint32_t decompsize = 0;
        if (compressed)
        {
            log_i("iscompressed");
            decompsize = bigEndian(data + 6, 4);
            id3Size -= 4;
            (void)decompsize;
            log_i("decompsize=%u", decompsize);
            return 6 + 4;
        }
        return 6;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 5)
    { // If the frame is larger than 256 bytes, skip the rest
        if (framesize > 256)
        {
            framesize -= 256;
            id3Size -= 256;
            return 256;
        }
        else
        {
            m_controlCounter = 3; // check next frame
            id3Size -= framesize;
            return framesize;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 6)
    {                         // Read the value
        m_controlCounter = 5; // only read 256 bytes
        char value[256];
        char ch = *(data + 0);
        bool isUnicode = (ch == 1) ? true : false;

        if (tag == "APIC")
        { // a image embedded in file, passing it to external function
            log_i("framesize=%i", framesize);
            isUnicode = false;
            if (m_f_localfile)
            {
                size_t pos = m_id3Size - id3Size;
                if (audio_id3image)
                    audio_id3image(audiofile, pos, framesize);
            }
            return 0;
        }

        size_t fs = framesize;
        if (fs > 255)
            fs = 255;
        for (int i = 0; i < fs; i++)
        {
            value[i] = *(data + i);
        }
        framesize -= fs;
        id3Size -= fs;
        value[fs] = 0;
        if (isUnicode && fs > 1)
        {
            unicode2utf8(value, fs); // convert unicode to utf-8 U+0020...U+07FF
        }
        if (!isUnicode)
        {
            uint16_t j = 0, k = 0;
            j = 0;
            k = 0;
            while (j < fs)
            {
                if (value[j] == 0x0A)
                    value[j] = 0x20; // replace LF by space
                if (value[j] > 0x1F)
                {
                    value[k] = value[j];
                    k++;
                }
                j++;
            } //remove non printables
            if (k > 0)
                value[k] = 0;
            else
                value[0] = 0; // new termination
        }
        showID3Tag(tag, value);
        return fs;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    // -- section V2.2 only , greater Vers above ----
    if (m_controlCounter == 10)
    { // frames in V2.2, 3bytes identifier, 3bytes size descriptor
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = 0;
        tag = frameid;
        id3Size -= 3;
        size_t len = bigEndian(data + 3, 3);
        id3Size -= 3;
        id3Size -= len;
        char value[256];
        size_t tmp = len;
        if (tmp > 254)
            tmp = 254;
        memcpy(value, (data + 7), tmp);
        value[tmp + 1] = 0;
        chbuf[0] = 0;

        showID3Tag(tag, value);
        if (len == 0)
            m_controlCounter = 98;

        return 3 + 3 + len;
    }
    // -- end section V2.2 -----------

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 98)
    { // skip all ID3 metadata (mostly spaces)
        if (id3Size > 256)
        {
            id3Size -= 256;
            return 256;
        } // Throw it away
        else
        {
            m_controlCounter = 99;
            return id3Size;
        } // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == 99)
    { //  exist another ID3tag?
        m_audioDataStart += m_id3Size;
        vTaskDelay(30);
        if ((*(data + 0) == 'I') && (*(data + 1) == 'D') && (*(data + 2) == '3'))
        {
            m_controlCounter = 0;
            return 0;
        }
        else
        {
            m_controlCounter = 100; // ok
            m_audioDataSize = m_contentlength - m_audioDataStart;
            sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
            if (audio_info)
                audio_info(chbuf);

            return 0;
        }
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_M4A_Header(uint8_t *data, size_t len)
{
    /*
       ftyp
         | - moov  -> trak -> ... -> mp4a contains raw block parameters
         |    L... -> ilst  contains artist, composer ....
       free (optional)
         |
       mdat contains the audio data                                                      */

    static size_t headerSize = 0;
    static size_t retvalue = 0;
    static size_t atomsize = 0;
    static size_t audioDataPos = 0;

    if (retvalue)
    {
        if (retvalue > len)
        { // if returnvalue > bufferfillsize
            if (len > InBuff.getMaxBlockSize())
                len = InBuff.getMaxBlockSize();
            retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else
        {
            size_t tmp = retvalue;
            retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_BEGIN)
    { // init
        headerSize = 0;
        retvalue = 0;
        atomsize = 0;
        audioDataPos = 0;
        m_controlCounter = M4A_FTYP;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_FTYP)
    {                                  /* check_m4a_file */
        atomsize = bigEndian(data, 4); // length of first atom
        if (specialIndexOf(data, "ftyp", 10) != 4)
        {
            log_e("atom 'type' not found in header");
            stopSong();
            return -1;
        }
        if (specialIndexOf(data, "M4A ", 20) != 8)
        {
            log_e("subtype 'MA4 ' expected, but found '%s '", (data + 8));
            stopSong();
            return -1;
        }
        m_controlCounter = M4A_CHK;
        retvalue = atomsize;
        headerSize = atomsize;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_CHK)
    { /* check  Tag */
        //log_i("we are in chk, len=%i", len);
        atomsize = bigEndian(data, 4); // length of this atom
        if (specialIndexOf(data, "moov", 10) == 4)
        {
            m_controlCounter = M4A_MOOV;
            //log_i("atom 'moov' found ");
            return 0;
        }
        else if (specialIndexOf(data, "free", 10) == 4)
        {
            //log_i("atom 'free' found ");
            retvalue = atomsize;
            headerSize += atomsize;
            return 0;
        }
        else if (specialIndexOf(data, "mdat", 10) == 4)
        {
            //log_i("atom 'mdat' found ");
            m_controlCounter = M4A_MDAT;
            return 0;
        }
        else
        {
            char atomName[5];
            atomName[0] = *data;
            atomName[1] = *(data + 1);
            atomName[2] = *(data + 2);
            atomName[3] = *(data + 3);
            atomName[4] = 0;

            log_i("atom %s found", atomName);

            retvalue = atomsize;
            headerSize += atomsize;
            return 0;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_MOOV)
    { // moov
        // we are looking for track and ilst
        if (specialIndexOf(data, "trak", len) > 0)
        {
            //log_i("atom 'trak' found");
            int offset = specialIndexOf(data, "trak", len);
            retvalue = offset;
            atomsize -= offset;
            headerSize += offset;
            m_controlCounter = M4A_TRAK;
            return 0;
        }
        if (specialIndexOf(data, "ilst", len) > 0)
        {
            // log_i("atom 'ilst' found");
            int offset = specialIndexOf(data, "ilst", len);
            retvalue = offset;
            atomsize -= offset;
            headerSize += offset;
            m_controlCounter = M4A_ILST;
            return 0;
        }
        if (atomsize > len - 10)
        {
            atomsize -= (len - 10);
            headerSize += (len - 10);
            retvalue = (len - 10);
        }
        else
        {
            m_controlCounter = M4A_CHK;
            retvalue = atomsize;
            headerSize += atomsize;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_TRAK)
    { // trak
        //log_i("we are in track, len=%i", len);
        if (specialIndexOf(data, "esds", len) > 0)
        {
            int esds = specialIndexOf(data, "esds", len); // Packaging/Encapsulation And Setup Data
            uint8_t *pos = data + esds;
            uint8_t len_of_OD = *(pos + 12);  // length of this OD (which includes the next 2 tags)
            uint8_t len_of_ESD = *(pos + 20); // length of this Elementary Stream Descriptor

            uint8_t audioType = *(pos + 21);
            if (audioType == 0x40)
                sprintf(chbuf, "AudioType: MPEG4 / Audio"); // ObjectTypeIndication
            else if (audioType == 0x66)
                sprintf(chbuf, "AudioType: MPEG2 / Audio");
            else if (audioType == 0x69)
                sprintf(chbuf, "AudioType: MPEG2 / Audio Part 3"); // Backward Compatible Audio
            else if (audioType == 0x6B)
                sprintf(chbuf, "AudioType: MPEG1 / Audio");
            else
                sprintf(chbuf, "unknown Audio Type %x", audioType);
            if (audio_info)
                audio_info(chbuf);

            uint8_t streamType = *(pos + 22);
            streamType = streamType >> 2; // 6 bits
            if (streamType != 5)
                log_e("Streamtype is not audio!");

            uint32_t maxBr = bigEndian(pos + 26, 4); // max bitrate
            sprintf(chbuf, "max bitrate: %i", maxBr);
            if (audio_info)
                audio_info(chbuf);

            uint32_t avrBr = bigEndian(pos + 30, 4); // avg bitrate
            sprintf(chbuf, "avr bitrate: %i", avrBr);
            if (audio_info)
                audio_info(chbuf);

            uint16_t ASC = bigEndian(pos + 39, 2);

            uint8_t objectType = ASC >> 11; // first 5 bits
            if (objectType == 1)
                sprintf(chbuf, "AudioObjectType: AAC Main"); // Audio Object Types
            else if (objectType == 2)
                sprintf(chbuf, "AudioObjectType: AAC Low Complexity");
            else if (objectType == 3)
                sprintf(chbuf, "AudioObjectType: AAC Scalable Sample Rate");
            else if (objectType == 4)
                sprintf(chbuf, "AudioObjectType: AAC Long Term Prediction");
            else if (objectType == 5)
                sprintf(chbuf, "AudioObjectType: AAC Spectral Band Replication");
            else if (objectType == 6)
                sprintf(chbuf, "AudioObjectType: AAC Scalable");
            else
                sprintf(chbuf, "unknown Audio Type %x", audioType);
            if (audio_info)
                audio_info(chbuf);

            const uint32_t samplingFrequencies[13] = {
                96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
            uint8_t sRate = (ASC & 0x0600) >> 7; // next 4 bits Sampling Frequencies
            sprintf(chbuf, "Sampling Frequency: %lu", samplingFrequencies[sRate]);
            if (audio_info)
                audio_info(chbuf);

            uint8_t chConfig = (ASC & 0x78) >> 3; // next 4 bits
            if (chConfig == 0)
                if (audio_info)
                    audio_info("Channel Configurations: AOT Specifc Config");
            if (chConfig == 1)
                if (audio_info)
                    audio_info("Channel Configurations: front-center");
            if (chConfig == 2)
                if (audio_info)
                    audio_info("Channel Configurations: front-left, front-right");
            if (chConfig > 2)
                log_e("Channel Configurations with more than 2 channels is not allowed!");

            uint8_t frameLengthFlag = (ASC & 0x04);
            uint8_t dependsOnCoreCoder = (ASC & 0x02);
            uint8_t extensionFlag = (ASC & 0x01);

            if (frameLengthFlag == 0)
                if (audio_info)
                    audio_info("AAC FrameLength: 1024 bytes");
            if (frameLengthFlag == 1)
                if (audio_info)
                    audio_info("AAC FrameLength: 960 bytes");
        }
        if (specialIndexOf(data, "mp4a", len) > 0)
        {
            int offset = specialIndexOf(data, "mp4a", len);
            int channel = bigEndian(data + offset + 20, 2); // audio parameter must be set before starting
            int bps = bigEndian(data + offset + 22, 2);     // the aac decoder. There are RAW blocks only in m4a
            int srate = bigEndian(data + offset + 26, 4);   //
            setBitsPerSample(bps);
            setChannels(channel);
            setSampleRate(srate);
            setBitrate(bps * channel * srate);
            sprintf(chbuf, "ch; %i, bps: %i, sr: %i", channel, bps, srate);
            if (audio_info)
                audio_info(chbuf);
            if (audioDataPos && m_f_localfile)
            {
                m_controlCounter = M4A_AMRDY;
                setFilePos(audioDataPos);
                return 0;
            }
        }
        m_controlCounter = M4A_MOOV;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_ILST)
    { // ilst
        const char info[12][6] = {"nam\0", "ART\0", "alb\0", "too\0", "cmt\0", "wrt\0",
                                  "tmpo\0", "trkn\0", "day\0", "cpil\0", "aART\0", "gen\0"};
        int offset;
        for (int i = 0; i < 12; i++)
        {
            offset = specialIndexOf(data, info[i], len, true); // seek info[] with '\0'
            if (offset > 0)
            {
                offset += 19;
                if (*(data + offset) == 0)
                    offset++;
                char value[256];
                size_t tmp = strlen((const char *)data + offset);
                if (tmp > 254)
                    tmp = 254;
                memcpy(value, (data + offset), tmp);
                value[tmp] = 0;
                chbuf[0] = 0;
                if (i == 0)
                    sprintf(chbuf, "Title: %s", value);
                if (i == 1)
                    sprintf(chbuf, "Artist: %s", value);
                if (i == 2)
                    sprintf(chbuf, "Album: %s", value);
                if (i == 3)
                    sprintf(chbuf, "Encoder: %s", value);
                if (i == 4)
                    sprintf(chbuf, "Comment: %s", value);
                if (i == 5)
                    sprintf(chbuf, "Composer: %s", value);
                if (i == 6)
                    sprintf(chbuf, "BPM: %s", value);
                if (i == 7)
                    sprintf(chbuf, "Track Number: %s", value);
                if (i == 8)
                    sprintf(chbuf, "Year: %s", value);
                if (i == 9)
                    sprintf(chbuf, "Compile: %s", value);
                if (i == 10)
                    sprintf(chbuf, "Album Artist: %s", value);
                if (i == 11)
                    sprintf(chbuf, "Types of: %s", value);
                if (chbuf[0] != 0)
                {
                    if (audio_id3data)
                        audio_id3data(chbuf);
                }
            }
        }
        m_controlCounter = M4A_MOOV;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == M4A_MDAT)
    { // mdat
        //log_i("we are in mdat, len=%i", len);
        m_audioDataSize = bigEndian(data, 4); // length of this atom
        sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
        if (audio_info)
            audio_info(chbuf);
        retvalue = 8;
        headerSize += 8;
        m_controlCounter = M4A_AMRDY; // last step before starting the audio
        return 0;
    }

    if (m_controlCounter == M4A_AMRDY)
    { // almost ready
        m_audioDataStart = headerSize;
        m_contentlength = headerSize + m_audioDataSize; // after this mdat atom there may be other atoms
        log_i("begin mdat %i", headerSize);
        if (m_f_localfile)
        {
            sprintf(chbuf, "Content-Length: %u", m_contentlength);
            if (audio_info)
                audio_info(chbuf);
        }
        m_controlCounter = M4A_OKAY; // that's all
        return 0;
    }
    // this section should never be reached
    log_e("error");
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_OGG_Header(uint8_t *data, size_t len)
{
    static size_t headerSize;
    static size_t retvalue;
    static bool f_lastMetaBlock;

    if (retvalue)
    {
        if (retvalue > len)
        { // if returnvalue > bufferfillsize
            if (len > InBuff.getMaxBlockSize())
                len = InBuff.getMaxBlockSize();
            retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else
        {
            size_t tmp = retvalue;
            retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == OGG_BEGIN)
    { // init
        headerSize = 0;
        retvalue = 0;
        m_audioDataStart = 0;
        f_lastMetaBlock = false;
        m_controlCounter = OGG_MAGIC;
        if (m_f_localfile)
        {
            m_contentlength = getFileSize();
            sprintf(chbuf, "Content-Length: %u", m_contentlength);
            if (audio_info)
                audio_info(chbuf);
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_controlCounter == OGG_MAGIC)
    { /* check MAGIC STRING */
        if (specialIndexOf(data, "OggS", 10) != 0)
        {
            log_e("Magic String 'OggS' not found in header");
            stopSong();
            return -1;
        }
        //        log_i("Magig String found");
        //        m_controlCounter = OGG_MBH;
        //        headerSize = 4;
        //        retvalue = 4;
        //        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (audio_info)
        audio_info("OGG is not supportet yet");
    return -1;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that stop current playback.
///
void Audio::stopSong()
{
    if (m_f_running)
    {
        m_f_running = false;
        audiofile.close();
    }
    memset(m_outBuff, 0, sizeof(m_outBuff)); //Clear OutputBuffer
    i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playI2Sremains()
{ // returns true if all dma_buffs flushed
    static uint8_t dma_buf_count = 0;
    // there is no  function to see if dma_buff is empty. So fill the dma completely.
    // As soon as all remains played this function returned. Or you can take this to create a short silence.
    if (m_sampleRate == 0)
        setSampleRate(96000);
    if (m_channels == 0)
        setChannels(2);
    if (getBitsPerSample() > 8)
        memset(m_outBuff, 0, sizeof(m_outBuff)); //Clear OutputBuffer (signed)
    else
        memset(m_outBuff, 128, sizeof(m_outBuff)); //Clear OutputBuffer (unsigned, PCM 8u)
    //play remains and then flush dmaBuff
    m_validSamples = m_i2s_config.dma_buf_len;
    while (m_validSamples)
    {
        playChunk();
    }
    if (dma_buf_count < m_i2s_config.dma_buf_count)
    {
        dma_buf_count++;
        return false;
    }
    dma_buf_count = 0;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that pause / resume current playback.
///
/// @return true if it succeed to pause / resume current playback.
/// @return false if it failed to pause / resume current playback (no playback).
bool Audio::pauseResume()
{
    bool retVal = false;
    if (m_f_localfile || m_f_webstream)
    {
        m_f_running = !m_f_running;
        retVal = true;
        if (!m_f_running)
        {
            memset(m_outBuff, 0, sizeof(m_outBuff)); //Clear OutputBuffer
            i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
        }
    }
    return retVal;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playChunk()
{
    // If we've got data, try and pump it out..
    int16_t sample[2];
    if (getBitsPerSample() == 8)
    {
        if (m_channels == 1)
        {
            while (m_validSamples)
            {
                uint8_t x = m_outBuff[m_curSample] & 0x00FF;
                uint8_t y = (m_outBuff[m_curSample] & 0xFF00) >> 8;
                sample[LEFTCHANNEL] = x;
                sample[RIGHTCHANNEL] = x;
                while (1)
                {
                    if (playSample(sample))
                        break;
                } // Can't send?
                sample[LEFTCHANNEL] = y;
                sample[RIGHTCHANNEL] = y;
                while (1)
                {
                    if (playSample(sample))
                        break;
                } // Can't send?
                m_validSamples--;
                m_curSample++;
            }
        }
        if (m_channels == 2)
        {
            while (m_validSamples)
            {
                uint8_t x = m_outBuff[m_curSample] & 0x00FF;
                uint8_t y = (m_outBuff[m_curSample] & 0xFF00) >> 8;
                if (!m_f_forceMono)
                { // stereo mode
                    sample[LEFTCHANNEL] = x;
                    sample[RIGHTCHANNEL] = y;
                }
                else
                { // force mono
                    uint8_t xy = (x + y) / 2;
                    sample[LEFTCHANNEL] = xy;
                    sample[RIGHTCHANNEL] = xy;
                }

                while (1)
                {
                    if (playSample(sample))
                        break;
                } // Can't send?
                m_validSamples--;
                m_curSample++;
            }
        }
        m_curSample = 0;
        return true;
    }
    if (getBitsPerSample() == 16)
    {
        if (m_channels == 1)
        {
            while (m_validSamples)
            {
                sample[LEFTCHANNEL] = m_outBuff[m_curSample];
                sample[RIGHTCHANNEL] = m_outBuff[m_curSample];
                if (!playSample(sample))
                {
                    return false;
                } // Can't send
                m_validSamples--;
                m_curSample++;
            }
        }
        if (m_channels == 2)
        {
            while (m_validSamples)
            {
                if (!m_f_forceMono)
                { // stereo mode
                    sample[LEFTCHANNEL] = m_outBuff[m_curSample * 2];
                    sample[RIGHTCHANNEL] = m_outBuff[m_curSample * 2 + 1];
                }
                else
                { // mono mode, #100
                    int16_t xy = (m_outBuff[m_curSample * 2] + m_outBuff[m_curSample * 2 + 1]) / 2;
                    sample[LEFTCHANNEL] = xy;
                    sample[RIGHTCHANNEL] = xy;
                }
                if (!playSample(sample))
                {
                    return false;
                } // Can't send
                m_validSamples--;
                m_curSample++;
            }
        }
        m_curSample = 0;
        return true;
    }
    log_e("BitsPer Sample must be 8 or 16!");
    return false;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief Loop that process playing of the differents stream.
///
void Audio::loop()
{
    // - localfile - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_f_localfile)
    { // Playing file fron SPIFFS or SD?
        processLocalFile();
    }
    // - webstream - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_f_webstream)
    { // Playing file from URL?
        processWebStream();
    }
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that process audio file (from a file system).
///
void Audio::processLocalFile()
{

    if (!(audiofile && m_f_running && m_f_localfile))
        return;

    int bytesDecoded = 0;
    uint32_t bytesCanBeWritten = 0;
    uint32_t bytesCanBeRead = 0;
    int32_t bytesAddedToBuffer = 0;

    if (m_f_firstCall)
    { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        return;
    }
    bytesCanBeWritten = InBuff.writeSpace();
    //----------------------------------------------------------------------------------------------------
    // some files contain further data after the audio block (e.g. pictures).
    // In that case, the end of the audio block is not the end of the file. An 'eof' has to be forced.
    if ((m_controlCounter == 100) && (m_contentlength > 0))
    { // fileheader was read
        if (bytesCanBeWritten + getFilePos() >= m_contentlength)
        {
            if (m_contentlength > getFilePos())
                bytesCanBeWritten = m_contentlength - getFilePos();
            else
                bytesCanBeWritten = 0;
        }
    }
    //----------------------------------------------------------------------------------------------------

    bytesAddedToBuffer = audiofile.read(InBuff.getWritePtr(), bytesCanBeWritten);
    if (bytesAddedToBuffer > 0)
    {
        InBuff.bytesWritten(bytesAddedToBuffer);
    }

    //    if(psramFound() && bytesAddedToBuffer >4096)
    //        vTaskDelay(2);// PSRAM has a bottleneck in the queue, so wait a little bit

    if (bytesAddedToBuffer == -1)
        bytesAddedToBuffer = 0; // read error? eof?
    bytesCanBeRead = InBuff.bufferFilled();
    if (bytesCanBeRead > InBuff.getMaxBlockSize())
        bytesCanBeRead = InBuff.getMaxBlockSize();
    if (bytesCanBeRead == InBuff.getMaxBlockSize())
    { // mp3 or aac frame complete?
        if (!m_f_stream)
        {
            m_f_stream = true;
            if (audio_info)
                audio_info("stream ready");
        }
        if (m_controlCounter != 100)
        {
            if (m_codec == CODEC_WAV)
            {
                int res = read_WAV_Header(InBuff.getReadPtr(), bytesCanBeRead);
                if (res >= 0)
                    bytesDecoded = res;
                else
                { // error, skip header
                    m_controlCounter = 100;
                }
            }
            if (m_codec == CODEC_MP3)
            {
                int res = read_MP3_Header(InBuff.getReadPtr(), bytesCanBeRead);
                if (res >= 0)
                    bytesDecoded = res;
                else
                { // error, skip header
                    m_controlCounter = 100;
                }
            }
            if (m_codec == CODEC_M4A)
            {
                int res = read_M4A_Header(InBuff.getReadPtr(), bytesCanBeRead);
                if (res >= 0)
                    bytesDecoded = res;
                else
                { // error, skip header
                    m_controlCounter = 100;
                }
            }
            if (m_codec == CODEC_AAC)
            {
                // stream only, no header
                m_audioDataSize = getFileSize();
                m_controlCounter = 100;
            }

            if (m_codec == CODEC_FLAC)
            {
                int res = read_FLAC_Header(InBuff.getReadPtr(), bytesCanBeRead);
                if (res >= 0)
                    bytesDecoded = res;
                else
                { // error, skip header
                    stopSong();
                    m_controlCounter = 100;
                }
            }
        }
        else
        {
            bytesDecoded = sendBytes(InBuff.getReadPtr(), bytesCanBeRead);
        }
        if (bytesDecoded > 0)
        {
            InBuff.bytesWasRead(bytesDecoded);
            return;
        }
        if (bytesDecoded < 0)
        {                             // no syncword found or decode error, try next chunk
            InBuff.bytesWasRead(200); // try next chunk
            m_bytesNotDecoded += 200;
            return;
        }
        return;
    }
    if (!bytesAddedToBuffer)
    { // eof
        bytesCanBeRead = InBuff.bufferFilled();
        if (bytesCanBeRead > 200)
        {
            if (bytesCanBeRead > InBuff.getMaxBlockSize())
                bytesCanBeRead = InBuff.getMaxBlockSize();
            bytesDecoded = sendBytes(InBuff.getReadPtr(), bytesCanBeRead); // play last chunk(s)
            if (bytesDecoded > 0)
            {
                InBuff.bytesWasRead(bytesDecoded);
                return;
            }
        }
        InBuff.resetBuffer();
        if (!playI2Sremains())
            return;

        if (m_f_loop && m_f_stream)
        {                                                                           //eof
            sprintf(chbuf, "loop from: %u to: %u", getFilePos(), m_audioDataStart); //TEST loop
            if (audio_info)
                audio_info(chbuf);
            setFilePos(m_audioDataStart);
            if (m_codec == CODEC_FLAC)
                FLACDecoderReset();
            /*
                The current time of the loop mode is not reset,
                which will cause the total audio duration to be exceeded.
                For example: current time   ====progress bar====>  total audio duration
                                3:43        ====================>        3:33
            */
            m_audioCurrentTime = 0;
            return;
        } //TEST loop
        stopSong();
        m_f_stream = false;
        m_f_localfile = false;
        if (m_codec == CODEC_MP3)
            MP3Decoder_FreeBuffers();
        if (m_codec == CODEC_AAC)
            AACDecoder_FreeBuffers();
        if (m_codec == CODEC_M4A)
            AACDecoder_FreeBuffers();
        if (m_codec == CODEC_FLAC)
            FLACDecoder_FreeBuffers();
        sprintf(chbuf, "End of file \"%s\"", m_audioName);
        if (audio_info)
            audio_info(chbuf);
        if (audio_eof_mp3)
            audio_eof_mp3(m_audioName);
    }
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that process audio stream (from internet).
///
void Audio::processWebStream()
{

    if (!(m_f_running && m_f_webstream))
        return;

    uint32_t bytesCanBeWritten = 0;
    int16_t bytesAddedToBuffer = 0;
    const uint16_t maxFrameSize = InBuff.getMaxBlockSize(); // every mp3/aac frame is not bigger
    int32_t availableBytes = 0;                             // available bytes in stream
    bool f_tmr_1s = false;
    static int bytesDecoded;
    static uint32_t byteCounter; // count received data
    static uint32_t chunksize;   // chunkcount read from stream
    static uint32_t cnt0;
    static uint32_t tmr_1s = 0; // timer 1 sec

    if (m_f_firstCall)
    { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        byteCounter = 0;
        chunksize = 0;
        bytesDecoded = 0;
        cnt0 = 0;
        tmr_1s = 0;
    }

    if ((tmr_1s + 1000) < millis())
    {
        f_tmr_1s = true; // flag will be set every second for one loop only
        tmr_1s = millis();
    }

    if (m_f_ssl == false)
        availableBytes = client.available(); // available from stream
    if (m_f_ssl == true)
        availableBytes = clientsecure.available(); // available from stream

    if ((m_datamode == AUDIO_DATA || m_datamode == AUDIO_SWM) && m_f_webfile)
    {
        // normally there is nothing to do here, if byteCounter == contentLength
        // then the file is completely read, but:
        // m4a files can have more data  (e.g. pictures ..) after the audio Block
        // therefore it is bad to read anything else (this can generate noise)
        if (byteCounter >= m_contentlength)
        {
            availableBytes = 0;
        }
    }

    if ((m_f_firststream_ready == false) && (availableBytes > 0))
    { // first streamdata recognized
        m_f_firststream_ready = true;
        m_f_stream = false;
        //m_bytectr = 0;
    }

    if (((m_datamode == AUDIO_DATA) || (m_datamode == AUDIO_SWM)) && (m_metaCount > 0))
    {
        bytesCanBeWritten = InBuff.writeSpace();
        uint32_t len = min(m_metaCount, uint32_t(bytesCanBeWritten));
        if ((m_f_chunked) && (len > m_chunkcount))
        {
            len = m_chunkcount;
        }

        if ((m_f_ssl == false) && (availableBytes > 0))
        {
            bytesAddedToBuffer = client.read(InBuff.getWritePtr(), len);
        }

        if ((m_f_ssl == true) && (availableBytes > 0))
        {
            bytesAddedToBuffer = clientsecure.read(InBuff.getWritePtr(), len);
        }

        if (psramFound() && bytesAddedToBuffer > 4096)
            vTaskDelay(2); // PSRAM has a bottleneck in the queue, so wait a little bit

        if (bytesAddedToBuffer > 0)
        {
            if (m_f_webfile)
            {
                byteCounter += bytesAddedToBuffer; // Pull request #42
            }
            m_metaCount -= bytesAddedToBuffer;
            if (m_f_chunked)
            {
                m_chunkcount -= bytesAddedToBuffer;
            }
            InBuff.bytesWritten(bytesAddedToBuffer);
        }

        // waiting for buffer filled, set tresholds before the stream is starting
        if ((InBuff.bufferFilled() > 6000 && !m_f_psram) || (InBuff.bufferFilled() > 80000 && m_f_psram))
        {
            if (m_f_stream == false)
            {
                m_f_stream = true;
                cnt0 = 0;
                uint16_t filltime = millis() - m_t0;
                if (audio_info)
                    audio_info("stream ready");
                sprintf(chbuf, "buffer filled in %d ms", filltime);
                if (audio_info)
                    audio_info(chbuf);
            }
        }
        else
        {
            if (m_f_stream == false)
            {
                if (cnt0 == 0)
                {
                    m_t0 = millis();
                    if (audio_info)
                        audio_info("inputbuffer is being filled");
                }
                cnt0++;
            }
        }

        if ((InBuff.bufferFilled() > maxFrameSize) && (m_f_stream == true))
        { // fill > framesize?
            if (m_codec == CODEC_APP)
            { //application/ogg
                if (m_controlCounter != 100)
                {
                    int res = read_OGG_Header(InBuff.getReadPtr(), InBuff.bufferFilled());
                    if (res >= 0)
                        bytesDecoded = res;
                    else
                    { // error, skip header
                        stopSong();
                        m_controlCounter = 100;
                    }
                }
            }
            if (m_f_webfile)
            { // can contain ID3 metadata, issue #83
                if (m_controlCounter != 100)
                {
                    if (m_codec == CODEC_WAV)
                    {
                        int res = read_WAV_Header(InBuff.getReadPtr(), InBuff.bufferFilled());
                        if (res >= 0)
                            bytesDecoded = res;
                        else
                        { // error, skip header
                            m_controlCounter = 100;
                        }
                    }
                    if (m_codec == CODEC_MP3)
                    {
                        int res = read_MP3_Header(InBuff.getReadPtr(), InBuff.bufferFilled());
                        if (res >= 0)
                            bytesDecoded = res;
                        else
                        { // error, skip header
                            m_controlCounter = 100;
                        }
                    }
                    if (m_codec == CODEC_M4A)
                    {
                        int res = read_M4A_Header(InBuff.getReadPtr(), InBuff.bufferFilled());
                        if (res >= 0)
                            bytesDecoded = res;
                        else
                        { // error, skip header
                            m_controlCounter = 100;
                        }
                    }
                    if (m_codec == CODEC_FLAC)
                    {
                        int res = read_FLAC_Header(InBuff.getReadPtr(), InBuff.bufferFilled());
                        if (res >= 0)
                            bytesDecoded = res;
                        else
                        { // error, skip header
                            stopSong();
                            m_controlCounter = 100;
                        }
                    }
                }
                else
                {
                    bytesDecoded = sendBytes(InBuff.getReadPtr(), maxFrameSize);
                }
            }
            else
            { // not a webfile
                bytesDecoded = sendBytes(InBuff.getReadPtr(), InBuff.bufferFilled());
            }

            if (bytesDecoded < 0)
            {                             // no syncword found or decode error, try next chunk
                InBuff.bytesWasRead(200); // try next chunk
                m_bytesNotDecoded += 200;
                return;
            }
            else
            {
                InBuff.bytesWasRead(bytesDecoded);
            }
        }
        else
        { // InBuff almost empty, contains no complete mp3/aac frame
            static uint8_t cnt_slow = 0;
            if (m_f_stream == true)
            {
                cnt_slow++;

                if (f_tmr_1s)
                {
                    if (cnt_slow > 18)
                        if (audio_info)
                            audio_info("slow stream, dropouts are possible");
                    f_tmr_1s = false;
                    cnt_slow = 0;
                }
            }
        }

        if (m_metaCount == 0)
        {
            if (m_datamode == AUDIO_SWM)
            {
                m_metaCount = 16000; //mms has no metadata
            }
            else
            {
                m_datamode = AUDIO_METADATA;
                m_f_firstmetabyte = true;
            }
        }
    }
    else
    { //!=DATA
        if (m_datamode == AUDIO_PLAYLISTDATA)
        {
            if (m_t0 + 49 < millis())
            {
                processControlData('\n'); // send LF
            }
        }
        int16_t x = 0;
        if (m_f_chunked && (m_datamode != AUDIO_HEADER))
        {
            if (m_chunkcount > 0)
            {
                if (m_f_ssl == false)
                    x = client.read();
                else
                    x = clientsecure.read();

                if (x >= 0)
                {
                    processControlData(x);
                    m_chunkcount--;
                }
            }
        }
        else
        {
            if (m_f_ssl == false)
                x = client.read();
            else
                x = clientsecure.read();
            if (x >= 0)
            {
                processControlData(x);
            }
        }
        if (m_datamode == AUDIO_DATA)
        {
            m_metaCount = m_metaint;
            if (m_metaint == 0)
            {
                m_datamode = AUDIO_SWM; // stream without metadata, can be mms
            }
        }
        if (m_datamode == AUDIO_SWM)
        {
            m_metaCount = 16000;
        }
    }

    if (m_f_firststream_ready == true)
    {
        static uint32_t loopCnt = 0; // count loops if clientbuffer is empty
        if (availableBytes == 0)
        { // empty buffer, broken stream or bad bitrate?
            loopCnt++;
            if (loopCnt > 200000)
            { // wait several seconds
                loopCnt = 0;
                if (audio_info)
                    audio_info("Stream lost -> try new connection");
                connecttohost(m_lastHost); // try a new connection
            }
        }
        else
        {
            loopCnt = 0;
        }
        if (m_f_webfile && (byteCounter >= m_contentlength - 10) && (InBuff.bufferFilled() < maxFrameSize))
        {
            // it is stream from fileserver with known content-length? and
            // everything is received?  and
            // the buff is almost empty?, issue #66 then comes to an end
            playI2Sremains();
            stopSong(); // Correct close when play known length sound #74 and before callback #112
            sprintf(chbuf, "End of webstream: \"%s\"", m_lastHost);
            if (audio_info)
                audio_info(chbuf);
            if (audio_eof_stream)
                audio_eof_stream(m_lastHost);
        }
    }

    if (m_f_chunked)
    {
        if (m_datamode == AUDIO_HEADER)
        {
            chunksize = 0;
            return;
        }

        if (m_chunkcount == 0)
        { // Expecting a new chunkcount?
            int b;
            if (m_f_ssl == false)
            {
                b = client.read();
            }
            if (m_f_ssl == true)
            {
                b = clientsecure.read();
            }
            if (b < 1)
                return;

            if (b == '\r')
            {
                ;
            }
            else if (b == '\n')
            {
                m_chunkcount = chunksize;
                chunksize = 0;
            }
            else
            {
                // We have received a hexadecimal character.  Decode it and add to the result.
                b = toupper(b) - '0'; // Be sure we have uppercase
                if (b > 9)
                    b = b - 7; // Translate A..F to 10..15
                chunksize = (chunksize << 4) + b;
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processControlData(uint8_t b)
{
    if (m_datamode == AUDIO_NONE)
    {
        m_f_webfile = false;
        m_f_webstream = false;
        m_f_running = false;
        if (!m_f_ssl)
            client.stop();
        else
            clientsecure.stop();
        // error 404 occured, nothing to do, throw away all bytes
        return;
    }
    static char metaline[512];
    static uint16_t pos_ml = 0; // determines the current position in metaline

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_datamode == AUDIO_HEADER)
    { // Handle next byte of MP3 header
        if (b == '\n')
        {                // Linefeed ?
            m_LFcount++; // Count linefeeds
            metaline[pos_ml] = 0;
            parseAudioHeader(metaline);
            pos_ml = 0;
            metaline[pos_ml] = 0; // Reset this line
            if ((m_LFcount == 2) && m_f_ctseen)
            { // Some data seen and a double LF?
                if (getBitRate() == 0)
                {
                    //if(audio_bitrate) audio_bitrate("");
                } // no bitrate received
                if (m_f_swm == true)
                { // stream without metadata
                    m_datamode = AUDIO_SWM;
                    sprintf(chbuf, "Switch to SWM, bitrate is %d, metaint is %d", getBitRate(), m_metaint);
                    if (audio_info)
                        audio_info(chbuf);
                    memcpy(chbuf, m_lastHost, strlen(m_lastHost) + 1);
                    uint idx = indexOf(chbuf, "?", 0);
                    if (idx > 0)
                        chbuf[idx] = 0;
                    if (audio_lasthost)
                        audio_lasthost(chbuf);
                    m_f_swm = false;
                }
                else
                {
                    m_datamode = AUDIO_DATA; // Expecting data now
                    sprintf(chbuf, "Switch to DATA, metaint is %d", m_metaint);
                    if (audio_info)
                        audio_info(chbuf);
                    memcpy(chbuf, m_lastHost, strlen(m_lastHost) + 1);
                    uint idx = indexOf(chbuf, "?", 0);
                    if (idx > 0)
                        chbuf[idx] = 0;
                    if (audio_lasthost)
                        audio_lasthost(chbuf);
                }
                delay(50); // #77
            }
        }
        else
        {
            if ((b < 0x80) && (b != '\r') && (b != '\0'))
            {                  // Ignore unprintable characters, ignore CR, ignore NULL
                m_LFcount = 0; // Reset double CRLF detection
                metaline[pos_ml] = (char)b;
                if (pos_ml < 510)
                    pos_ml++;
                metaline[511] = 0; // for safety
                if (pos_ml == 509)
                    log_i("metaline overflow in AUDIO_HEADER! metaline=%s", metaline);
                if (pos_ml == 510)
                {
                    ; /* last current char in b */
                }
            }
        }
        return;
    } // end AUDIO_HEADER

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_datamode == AUDIO_METADATA)
    { // Handle next byte of metadata
        if (m_f_firstmetabyte)
        {                              // First byte of metadata?
            m_f_firstmetabyte = false; // Not the first anymore
            m_metalen = b * 16 + 1;    // New count for metadata including length byte
            if (m_metalen > 512)
            {
                if (audio_info)
                    audio_info("Metadata block to long! Skipping all Metadata from now on.");
                m_metaint = 16000;      // Probably no metadata
                m_datamode = AUDIO_SWM; // expect stream without metadata
            }
            pos_ml = 0;
            metaline[pos_ml] = 0; // Prepare for new line
        }
        else
        {
            if (m_datamode != AUDIO_SWM)
            {
                metaline[pos_ml] = (char)b; // Put new char in metaline
                if (pos_ml < 510)
                    pos_ml++;
                metaline[pos_ml] = 0;
                if (pos_ml == 509)
                    log_i("metaline overflow in AUDIO_METADATA! metaline=%s", metaline);
                if (pos_ml == 510)
                {
                    ; /* last current char in b */
                }
            }
        }
        if (--m_metalen == 0)
        {
            if (m_datamode != AUDIO_SWM)
                m_datamode = AUDIO_DATA; // Expecting data
            if (strlen(metaline))
            { // Any info present?
                // metaline contains artist and song name.  For example:
                // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
                // Sometimes it is just other info like:
                // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
                // Isolate the StreamTitle, remove leading and trailing quotes if present.
                //log_i("ST %s", m_metaline.c_str());

                int pos = indexOf(metaline, "song_spot", 0); // remove some irrelevant infos
                if (pos > 3)
                { // e.g. song_spot="T" MediaBaseId="0" itunesTrackId="0"
                    metaline[pos] = 0;
                }
                if (!m_f_localfile)
                    showstreamtitle(metaline); // Show artist and title if present in metadata
            }
        }
        return;
    } // end AUDIO_METADATA

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_datamode == AUDIO_PLAYLISTINIT)
    { // Initialize for receive .m3u file
        // We are going to use metadata to read the lines from the .m3u file
        // Sometimes this will only contain a single line

        pos_ml = 0;
        metaline[pos_ml] = 0;              // Prepare for new line
        m_LFcount = 0;                     // For detection end of header
        m_datamode = AUDIO_PLAYLISTHEADER; // Handle playlist data
        m_totalcount = 0;                  // Reset totalcount
        parsePlaylistData("clear_by_playlistinit");
        if (audio_info)
            audio_info("Read from playlist");
    } // end AUDIO_PLAYLISTINIT

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_datamode == AUDIO_PLAYLISTHEADER)
    { // Read header
        if (b == '\n')
        {                // Linefeed ?
            m_LFcount++; // Count linefeeds
            metaline[pos_ml] = 0;
            if (isascii(metaline[0]) && metaline[0] > 0x19)
            {
                sprintf(chbuf, "Playlistheader: %s", metaline); // Show playlistheader
                if (audio_info)
                    audio_info(chbuf);
            }

            char lc_ml[512]; // save temporarily
            memcpy(lc_ml, metaline, 511);
            lc_ml[511] = 0; // for safety

            int pos = indexOf(lc_ml, "404 Not Found", 0);
            if (pos >= 0)
            {
                m_datamode = AUDIO_NONE;
                if (audio_info)
                    audio_info("Error 404 Not Found");
                return;
            }

            pos = indexOf(lc_ml, "404 File Not Found", 0);
            if (pos >= 0)
            {
                m_datamode = AUDIO_NONE;
                if (audio_info)
                    audio_info("Error 404 File Not Found");
                return;
            }

            pos = indexOf(lc_ml, ":", 0); // lowercase all letters up to the colon
            if (pos >= 0)
            {
                for (int i = 0; i < pos; i++)
                {
                    lc_ml[i] = toLowerCase(lc_ml[i]);
                }
            }

            if (startsWith(lc_ml, "location:"))
            {
                const char *host;
                pos = indexOf(lc_ml, "http", 0);
                host = (lc_ml + pos);
                sprintf(chbuf, "redirect to new host %s", host);
                if (audio_info)
                    audio_info(chbuf);
                connecttohost(host);
            }
            pos_ml = 0;
            metaline[pos_ml] = 0; // Reset this line
            if (m_LFcount == 2)
            {
                if (audio_info)
                    audio_info("Switch to PLAYLISTDATA");
                m_datamode = AUDIO_PLAYLISTDATA; // Expecting data now
                m_t0 = millis();
                return;
            }
        }
        else
        {
            if ((b < 0x80) && (b != '\r') && (b != '\0'))
            {                  // Ignore unprintable characters, ignore CR, ignore NULL
                m_LFcount = 0; // Reset double CRLF detection
                metaline[pos_ml] = (char)b;
                if (pos_ml < 510)
                    pos_ml++;
                metaline[511] = 0; // for safety
                if (pos_ml == 509)
                    log_i("metaline overflow in AUDIO_PLAYLISTHEADER! metaline=%s", metaline);
                if (pos_ml == 510)
                {
                    ; /* last current char in b */
                }
            }
        }
        return;
    } // end AUDIO_PLAYLISTHEADER

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_datamode == AUDIO_PLAYLISTDATA)
    { // Read next byte of .m3u file data
        m_t0 = millis();
        if (b == '\n')
        { // Linefeed or end of string?
            metaline[pos_ml] = 0;
            if (isascii(metaline[0]) && metaline[0] > 0x19)
            {
                sprintf(chbuf, "Playlistdata: %s", metaline); // Show playlistdata
                if (audio_info)
                    audio_info(chbuf);
            }
            parsePlaylistData(metaline);
            pos_ml = 0;
            metaline[pos_ml] = 0; // Reset this line
        }

        else
        {
            if ((b < 0x80) && (b != '\r') && (b != '\0'))
            {                               // Ignore unprintable characters, ignore CR, ignore NULL
                metaline[pos_ml] = (char)b; // Normal character, add it to metaline
                if (pos_ml < 510)
                    pos_ml++;
                metaline[511] = 0; // for safety
                if (pos_ml == 509)
                    log_i("metaline overflow in AUDIO_PLAYLISTHEADER! metaline=%s", metaline);
                if (pos_ml == 510)
                {
                    ; /* last current char in b */
                }
            }
        }
        return;
    } // end AUDIO_PLAYLISTDATA
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::parsePlaylistData(const char *pd)
{
    static bool f_entry = false; // entryflag for asx playlist
    static bool f_title = false; // titleflag for asx playlist
    static bool f_ref = false;   // refflag   for asx playlist
    int pos = 0;                 // index (position in a string)
    char pd_buff[512];

    if (startsWith(pd, "clear_by_playlistinit"))
    {
        f_entry = false;
        f_title = false;
        f_ref = false;
        m_plsURL[0] = 0;
        //log_i("clear statics in playlistdata");
        return;
    }

    memcpy(pd_buff, pd, 512);

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_playlistFormat == FORMAT_M3U)
    {

        if (strlen(pd_buff) < 5)
        { // Skip short lines
            return;
        }
        if (indexOf(pd_buff, "#EXTINF:", 0) >= 0)
        {                                   // Info?
            pos = indexOf(pd_buff, ",", 0); // Comma in this line?
            if (pos > 0)
            {
                // Show artist and title if present in metadata
                if (audio_info)
                    audio_info(pd_buff + pos + 1);
            }
        }
        if (startsWith(pd_buff, "#"))
        { // Commentline?
            return;
        }

        pos = indexOf(pd_buff, "http://:@", 0); // ":@"??  remove that!
        if (pos >= 0)
        {
            sprintf(chbuf, "Entry in playlist found: %s", (pd_buff + pos + 9));
            connecttohost(pd_buff + pos + 9);
            return;
        }
        sprintf(chbuf, "Entry in playlist found: %s", pd_buff);
        if (audio_info)
            audio_info(chbuf);
        pos = indexOf(pd_buff, "http", 0); // Search for "http"
        const char *host;
        if (pos >= 0)
        { // Does URL contain "http://"?
            host = (pd_buff + pos);
            connecttohost(host);
        } // Yes, set new host
        return;
    } //m3u

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_playlistFormat == FORMAT_PLS)
    {
        if (startsWith(pd_buff, "File1"))
        {
            pos = indexOf(pd_buff, "http", 0); // File1=http://streamplus30.leonex.de:14840/;
            if (pos >= 0)
            {                                                         // yes, URL contains "http"?
                memcpy(m_plsURL, pd_buff + pos, strlen(pd_buff) + 1); // http://streamplus30.leonex.de:14840/;
                // Now we have an URL for a stream in host.
                f_ref = true;
            }
        }
        if (startsWith(pd_buff, "Title1"))
        { // Title1=Antenne Tirol
            const char *plsStationName = (pd_buff + 7);
            if (audio_showstation)
                audio_showstation(plsStationName);
            sprintf(chbuf, "StationName: \"%s\"", plsStationName);
            if (audio_info)
                audio_info(chbuf);
            f_title = true;
        }
        if (startsWith(pd_buff, "Length1"))
            f_title = true; // if no Title is available
        if ((f_ref == true) && (strlen(pd_buff) == 0))
            f_title = true;

        if (f_ref && f_title)
        {                            // we have both StationName and StationURL
            connecttohost(m_plsURL); // Connect to it
        }
    } // pls

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_playlistFormat == FORMAT_ASX)
    { // Advanced Stream Redirector
        if (indexOf(pd_buff, "<entry>", 0) >= 0)
            f_entry = true; // found entry tag (returns -1 if not found)
        if (f_entry)
        {
            if (indexOf(pd_buff, "ref href", 0) > 0)
            { // <ref href="http://87.98.217.63:24112/stream" />
                pos = indexOf(pd_buff, "http", 0);
                if (pos > 0)
                {
                    char *plsURL = (pd_buff + pos);      // http://87.98.217.63:24112/stream" />
                    int pos1 = indexOf(plsURL, "\"", 0); // http://87.98.217.63:24112/stream
                    if (pos1 > 0)
                    {
                        plsURL[pos1] = 0;
                    }
                    memcpy(m_plsURL, plsURL, strlen(plsURL)); // save url in array
                    log_i("m_plsURL = %s", m_plsURL);
                    // Now we have an URL for a stream in host.
                    //log_i("m_plsURL= %s", m_plsURL.c_str());
                    f_ref = true;
                }
            }
            pos = indexOf(pd_buff, "<title>", 0);
            if (pos < 0)
                pos = indexOf(pd_buff, "<Title>", 0);
            if (pos >= 0)
            {
                char *plsStationName = (pd_buff + pos + 7); // remove <Title>
                pos = indexOf(plsStationName, "</", 0);
                if (pos >= 0)
                {
                    *(plsStationName + pos) = 0; // remove </Title>
                }
                if (audio_showstation)
                    audio_showstation(plsStationName);
                sprintf(chbuf, "StationName: \"%s\"", plsStationName);
                if (audio_info)
                    audio_info(chbuf);
                f_title = true;
            }
        } //entry
        if (f_ref && f_title)
        {                            //we have both StationName and StationURL
            connecttohost(m_plsURL); // Connect to it
        }
    } //asx
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::parseAudioHeader(const char *ah)
{
    char ah_buff[512];
    memcpy(ah_buff, ah, 512);
    int pos = indexOf(ah_buff, ":", 0); // lowercase all letters up to the colon
    if (pos >= 0)
    {
        for (int i = 0; i < pos; i++)
        {
            ah_buff[i] = toLowerCase(ah_buff[i]);
        }
    }

    if (indexOf(ah_buff, "content-type:", 0) >= 0)
    {
        if (parseContentType(ah_buff))
            m_f_ctseen = true;
    }
    else if (startsWith(ah_buff, "location:"))
    {
        int pos = indexOf(ah_buff, "http", 0);
        const char *c_host = (ah_buff + pos);
        sprintf(chbuf, "redirect to new host \"%s\"", c_host);
        if (audio_info)
            audio_info(chbuf);
        connecttohost(c_host);
    }
    else if (startsWith(ah_buff, "set-cookie:") ||
             startsWith(ah_buff, "pragma:") ||
             startsWith(ah_buff, "expires:") ||
             startsWith(ah_buff, "cache-control:") ||
             startsWith(ah_buff, "icy-pub:") ||
             startsWith(ah_buff, "accept-ranges:"))
    {
        ; // do nothing
    }
    else if (startsWith(ah_buff, "connection:"))
    {
        if (indexOf(ah_buff, "close", 0) >= 0)
        {
            ; /* do nothing */
        }
    }
    else if (startsWith(ah_buff, "icy-genre:"))
    {
        ; // do nothing Ambient, Rock, etc
    }
    else if (startsWith(ah_buff, "icy-br:"))
    {
        const char *c_bitRate = (ah_buff + 7);
        int32_t br = atoi(c_bitRate); // Found bitrate tag, read the bitrate in Kbit
        br = br * 1000;
        setBitrate(br);
        sprintf(chbuf, "%d", getBitRate());
        if (audio_bitrate)
            audio_bitrate(chbuf);
    }
    else if (startsWith(ah_buff, "icy-metaint:"))
    {
        const char *c_metaint = (ah_buff + 12);
        int32_t i_metaint = atoi(c_metaint);
        m_metaint = i_metaint;
        if (m_metaint > 0)
            m_f_swm = false; // Multimediastream
    }
    else if (startsWith(ah_buff, "icy-name:"))
    {
        char *c_icyname = (ah_buff + 9); // Get station name
        pos = 0;
        while (c_icyname[pos] == ' ')
        {
            pos++;
        }
        c_icyname += pos; // Remove leading spaces
        pos = strlen(c_icyname);
        while (c_icyname[pos] == ' ')
        {
            pos--;
        }
        c_icyname[pos + 1] = 0; // Remove trailing spaces

        if (strlen(c_icyname) > 0)
        {
            sprintf(chbuf, "icy-name: %s", c_icyname);
            if (audio_info)
                audio_info(chbuf);
            if (audio_showstation)
                audio_showstation(c_icyname);
        }
    }
    else if (startsWith(ah_buff, "content-length:"))
    {
        const char *c_cl = (ah_buff + 15);
        int32_t i_cl = atoi(c_cl);
        m_contentlength = i_cl;
        m_f_webfile = true; // Stream comes from a fileserver
        sprintf(chbuf, "Content-Length: %i", m_contentlength);
        if (audio_info)
            audio_info(chbuf);
    }
    else if ((startsWith(ah_buff, "transfer-encoding:")))
    {
        if (endsWith(ah_buff, "chunked") || endsWith(ah_buff, "Chunked"))
        { // Station provides chunked transfer
            m_f_chunked = true;
            if (audio_info)
                audio_info("chunked data transfer");
            m_chunkcount = 0; // Expect chunkcount in DATA
        }
    }
    else if (startsWith(ah_buff, "icy-url:"))
    {
        const char *icyurl = (ah_buff + 8);
        pos = 0;
        while (icyurl[pos] == ' ')
        {
            pos++;
        }
        icyurl += pos; // remove leading blanks
        sprintf(chbuf, "icy-url: %s", icyurl);
        if (audio_info)
            audio_info(chbuf);
        if (audio_icyurl)
            audio_icyurl(icyurl);
    }
    else if (startsWith(ah_buff, "www-authenticate:"))
    {
        if (audio_info)
            audio_info("authentification failed, wrong credentials?");
        m_f_running = false;
        stopSong();
    }
    else
    {
        if (isascii(ah_buff[0]) && ah_buff[0] >= 0x20)
        { // all other
            sprintf(chbuf, "%s", ah_buff);
            if (audio_info)
                audio_info(chbuf);
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::parseContentType(const char *ct)
{
    bool ct_seen = false;
    if (indexOf(ct, "audio", 0) >= 0)
    {                   // Is ct audio?
        ct_seen = true; // Yes, remember seeing this
        if (indexOf(ct, "mpeg", 13) >= 0)
        {
            m_codec = CODEC_MP3;
            sprintf(chbuf, "%s, format is mp3", ct);
            if (audio_info)
                audio_info(chbuf); //ok is likely mp3
            if (!MP3Decoder_AllocateBuffers())
            {
                m_f_running = false;
                stopSong();
                return false;
            }
            sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeMP3);
            if (audio_info)
                audio_info(chbuf);
        }
        else if (indexOf(ct, "mp3", 13) >= 0)
        {
            m_codec = CODEC_MP3;
            sprintf(chbuf, "%s, format is mp3", ct);
            if (audio_info)
                audio_info(chbuf);
            if (!MP3Decoder_AllocateBuffers())
            {
                m_f_running = false;
                stopSong();
                return false;
            }
            sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeMP3);
            if (audio_info)
                audio_info(chbuf);
        }
        else if (indexOf(ct, "aac", 13) >= 0)
        {
            m_codec = CODEC_AAC;
            sprintf(chbuf, "%s, format is aac", ct);
            if (audio_info)
                audio_info(chbuf);
            if (!AACDecoder_AllocateBuffers())
            {
                m_f_running = false;
                stopSong();
                return false;
            }
            sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeAAC);
            if (audio_info)
                audio_info(chbuf);
        }
        else if (indexOf(ct, "mp4", 13) >= 0)
        { // audio/mp4a, audio/mp4a-latm
            m_codec = CODEC_M4A;
            sprintf(chbuf, "%s, format is aac", ct);
            if (audio_info)
                audio_info(chbuf);
            if (!AACDecoder_AllocateBuffers())
            {
                m_f_running = false;
                stopSong();
                return false;
            }
            sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeAAC);
            if (audio_info)
                audio_info(chbuf);
        }
        else if (indexOf(ct, "m4a", 13) >= 0)
        { // audio/x-m4a
            m_codec = CODEC_M4A;
            sprintf(chbuf, "%s, format is aac", ct);
            if (audio_info)
                audio_info(chbuf);
            if (!AACDecoder_AllocateBuffers())
            {
                m_f_running = false;
                stopSong();
                return false;
            }
            sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeAAC);
            if (audio_info)
                audio_info(chbuf);
        }
        else if (indexOf(ct, "wav", 13) >= 0)
        { // audio/x-wav
            m_codec = CODEC_WAV;
            sprintf(chbuf, "%s, format is wav", ct);
            if (audio_info)
                audio_info(chbuf);
            InBuff.changeMaxBlockSize(m_frameSizeWav);
        }
        else if (indexOf(ct, "ogg", 13) >= 0)
        {
            m_codec = CODEC_APP;
            sprintf(chbuf, "ContentType %s found", ct);
            if (audio_info)
                audio_info(chbuf);
        }
        else if (indexOf(ct, "flac", 13) >= 0)
        { // audio/flac, audio/x-flac
            m_codec = CODEC_FLAC;
            sprintf(chbuf, "%s, format is flac", ct);
            if (audio_info)
                audio_info(chbuf);
            if (!psramFound())
            {
                if (audio_info)
                    audio_info("FLAC works only with PSRAM!");
                m_f_running = false;
                return false;
            }
            if (!FLACDecoder_AllocateBuffers())
            {
                m_f_running = false;
                stopSong();
                return false;
            }
            sprintf(chbuf, "FLACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeFLAC);
            if (audio_info)
                audio_info(chbuf);
        }
        else
        {
            m_f_running = false;
            sprintf(chbuf, "%s, unsupported audio format", ct);
            if (audio_info)
                audio_info(chbuf);
        }
    }
    if (indexOf(ct, "application", 0) >= 0)
    {                   // Is ct application?
        ct_seen = true; // Yes, remember seeing this
        if (indexOf(ct, "ogg", 13) >= 0)
        {
            m_codec = CODEC_APP;
            sprintf(chbuf, "ContentType %s found", ct);
            if (audio_info)
                audio_info(chbuf);
        }
    }
    return ct_seen;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showstreamtitle(const char *ml)
{
    // example for ml:
    // StreamTitle='Oliver Frank - Mega Hitmix';StreamUrl='www.radio-welle-woerthersee.at';
    // or adw_ad='true';durationMilliseconds='10135';adId='34254';insertionType='preroll';
    int16_t pos1 = 0, pos2 = 0, pos3 = 0, pos4 = 0;
    String mline = ml, st = "", su = "", ad = "", artist = "", title = "", icyurl = "";
    pos1 = mline.indexOf("StreamTitle=");
    if (pos1 != -1)
    { // StreamTitle found
        pos1 = pos1 + 12;
        st = mline.substring(pos1); // remove "StreamTitle="
        if (st.startsWith("'{"))
        {
            // special coding like '{"t":"\u041f\u0438\u043a\u043d\u0438\u043a - \u0418...."m":"mdb","lAU":0,"lAuU":18}
            pos2 = st.indexOf('"', 8); // end of '{"t":".......", seek for double quote at pos 8
            st = st.substring(0, pos2);
            pos2 = st.lastIndexOf('"');
            st = st.substring(pos2 + 1); // remove '{"t":"
            pos2 = 0;
            String uni = "";
            String st1 = "";
            uint16_t u = 0;
            uint8_t v = 0, w = 0;
            for (int i = 0; i < st.length(); i++)
            {
                if (pos2 > 1)
                    pos2++;
                if ((st[i] == '\\') && (pos2 == 0))
                    pos2 = 1; // backslash found
                if ((st[i] == 'u') && (pos2 == 1))
                    pos2 = 2; // "\u" found
                if (pos2 > 2)
                    uni = uni + st[i]; // next 4 values are unicode
                if (pos2 == 0)
                    st1 += st[i]; // normal character
                if (pos2 > 5)
                {
                    pos2 = 0;
                    u = strtol(uni.c_str(), 0, 16); // convert hex to int
                    v = u / 64 + 0xC0;
                    st1 += char(v); // compute UTF-8
                    w = u % 64 + 0x80;
                    st1 += char(w);
                    uni = "";
                }
            }
            log_i("st1 %s", st1.c_str());
            st = st1;
        }
        else
        {
            // normal coding
            if (st.indexOf('&') != -1)
            { // maybe html coded
                st.replace("&Auml;", "Ä");
                st.replace("&auml;", "ä"); // HTML -> ASCII
                st.replace("&Ouml;", "Ö");
                st.replace("&ouml;", "o");
                st.replace("&Uuml;", "Ü");
                st.replace("&uuml;", "ü");
                st.replace("&szlig;", "ß");
                st.replace("&amp;", "&");
                st.replace("&quot;", "\"");
                st.replace("&lt;", "<");
                st.replace("&gt;", ">");
                st.replace("&apos;", "'");
            }
            pos2 = st.indexOf(';', 1); // end of StreamTitle, first occurrence of ';'
            if (pos2 != -1)
                st = st.substring(0, pos2); // extract StreamTitle
            if (st.startsWith("'"))
                st = st.substring(1, st.length() - 1); // if exists remove ' at the begin and end
            pos3 = st.lastIndexOf(" - ");              // separator artist - title
            if (pos3 != -1)
            {                                   // found separator? yes
                artist = st.substring(0, pos3); // artist not used yet
                title = st.substring(pos3 + 3); // title not used yet
            }
        }

        uint16_t i = 0;
        uint16_t hash = 0;
        while (i < st.length())
        {
            hash += st[i] * i + 1;
            i++;
        }

        if (m_st_remember != hash)
        { // show only changes
            if (audio_showstreamtitle)
                audio_showstreamtitle(st.c_str());
            st = "StreamTitle=\"" + st + '\"';
            if (audio_info)
                audio_info(st.c_str());
            m_st_remember = hash;
        }
    }
    pos4 = mline.indexOf("StreamUrl=");
    if (pos4 != -1)
    { // StreamUrl found
        pos4 = pos4 + 10;
        su = mline.substring(pos4); // remove "StreamUrl="
        pos2 = su.indexOf(';', 1);  // end of StreamUrl, first occurrence of ';'
        if (pos2 != -1)
            su = su.substring(0, pos2); // extract StreamUrl
        if (su.startsWith("'"))
            su = su.substring(1, su.length() - 1); // if exists remove ' at the begin and end
        su = "StreamUrl=\"" + su + '\"';
        if (audio_info)
            audio_info(su.c_str());
    }
    pos2 = mline.indexOf("adw_ad="); // advertising,
    if (pos2 != -1)
    {
        ad = mline.substring(pos2);
        if (audio_info)
            audio_info(ad.c_str());
        pos2 = mline.indexOf("durationMilliseconds=");
        if (pos2 != -1)
        {
            pos2 += 22;
            mline = mline.substring(pos2);
            mline = mline.substring(0, mline.indexOf("'") - 3); // extract duration in seconds
            if (audio_commercial)
                audio_commercial(mline.c_str());
        }
    }
    if (pos1 == -1 && pos4 == -1)
    {
        // Info probably from playlist
        st = mline;
        if (audio_showstreamtitle)
            audio_showstreamtitle(st.c_str());
        st = "Streamtitle=\"" + st + '\"';
        if (audio_info)
            audio_info(st.c_str());
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showCodecParams()
{
    // print Codec Parameter (mp3, aac) in audio_info()
    sprintf(chbuf, "Channels: %i", getChannels());
    if (audio_info)
        audio_info(chbuf);
    sprintf(chbuf, "SampleRate: %i", getSampleRate());
    if (audio_info)
        audio_info(chbuf);
    sprintf(chbuf, "BitsPerSample: %i", getBitsPerSample());
    if (audio_info)
        audio_info(chbuf);
    sprintf(chbuf, "BitRate: %i", getBitRate());
    if (audio_info)
        audio_info(chbuf);

    if (m_codec == CODEC_AAC || m_codec == CODEC_M4A)
    {
        uint8_t answ;
        if ((answ = AACGetFormat()) < 4)
        {
            const char hf[4][8] = {"unknown", "ADTS", "ADIF", "RAW"};
            sprintf(chbuf, "AAC HeaderFormat: %s", hf[answ]);
            if (audio_info)
                audio_info(chbuf);
        }
        if (answ == 1)
        { // ADTS Header
            const char co[2][23] = {"MPEG-4", "MPEG-2"};
            sprintf(chbuf, "AAC Codec: %s", co[AACGetID()]);
            if (audio_info)
                audio_info(chbuf);
            if (AACGetProfile() < 5)
            {
                const char pr[4][23] = {"Main", "LowComplexity", "Scalable Sampling Rate", "reserved"};
                sprintf(chbuf, "AAC Profile: %s", pr[answ]);
                if (audio_info)
                    audio_info(chbuf);
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::findNextSync(uint8_t *data, size_t len)
{
    // Mp3 and aac audio data are divided into frames. At the beginning of each frame there is a sync word.
    // The sync word is 0xFFF. This is followed by information about the structure of the frame.
    // Wav files have no frames
    // Return: 0 the synchronous word was found at position 0
    //         > 0 is the offset to the next sync word
    //         -1 the sync word was not found within the block with the length len

    int nextSync;
    static uint32_t swnf = 0;
    if (m_codec == CODEC_WAV)
    {
        m_f_playing = true;
        nextSync = 0;
    }
    if (m_codec == CODEC_MP3)
    {
        nextSync = MP3FindSyncWord(data, len);
    }
    if (m_codec == CODEC_AAC)
    {
        nextSync = AACFindSyncWord(data, len);
    }
    if (m_codec == CODEC_M4A)
    {
        AACSetRawBlockParams(0, 2, 44100, 1);
        m_f_playing = true;
        nextSync = 0;
    }
    if (m_codec == CODEC_FLAC)
    {
        FLACSetRawBlockParams(m_flacNumChannels, m_flacSampleRate,
                              m_flacBitsPerSample, m_flacTotalSamplesInStream, m_audioDataSize);
        nextSync = FLACFindSyncWord(data, len);
    }
    if (nextSync == -1)
    {
        if (audio_info && swnf == 0)
            audio_info("syncword not found");
        swnf++; // syncword not found counter, can be multimediadata
    }
    if (nextSync == 0)
    {
        if (audio_info && swnf > 0)
        {
            sprintf(chbuf, "syncword not found %i times", swnf);
            audio_info(chbuf);
            swnf = 0;
        }
        else
        {
            if (audio_info)
                audio_info("syncword found at pos 0");
        }
    }
    if (nextSync > 0)
    {
        sprintf(chbuf, "syncword found at pos %i", nextSync);
        if (audio_info)
            audio_info(chbuf);
    }
    return nextSync;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::sendBytes(uint8_t *data, size_t len)
{
    int bytesLeft;
    static bool f_setDecodeParamsOnce = true;
    int nextSync = 0;
    if (!m_f_playing)
    {
        f_setDecodeParamsOnce = true;
        nextSync = findNextSync(data, len);
        if (nextSync == 0)
        {
            m_f_playing = true;
        }
        return nextSync;
    }
    // m_f_playing is true at this pos
    bytesLeft = len;
    int ret = 0;
    int bytesDecoded = 0;
    if (m_codec == CODEC_WAV)
    { //copy len data in outbuff and set validsamples and bytesdecoded=len
        memmove(m_outBuff, data, len);
        if (getBitsPerSample() == 16)
            m_validSamples = len / (2 * getChannels());
        if (getBitsPerSample() == 8)
            m_validSamples = len / 2;
        bytesLeft = 0;
    }
    if (m_codec == CODEC_MP3)
        ret = MP3Decode(data, &bytesLeft, m_outBuff, 0);
    if (m_codec == CODEC_AAC)
        ret = AACDecode(data, &bytesLeft, m_outBuff);
    if (m_codec == CODEC_M4A)
        ret = AACDecode(data, &bytesLeft, m_outBuff);
    if (m_codec == CODEC_FLAC)
        ret = FLACDecode(data, &bytesLeft, m_outBuff);

    bytesDecoded = len - bytesLeft;
    if (bytesDecoded == 0 && ret == 0)
    { // unlikely framesize
        if (audio_info)
            audio_info("framesize is 0, start decoding again");
        m_f_playing = false; // seek for new syncword
        // we're here because there was a wrong sync word
        // so skip two sync bytes and seek for next
        return 1;
    }
    if (ret < 0)
    { // Error, skip the frame...
        //if(m_codec == CODEC_M4A){log_i("begin not found"); return 1;}
        i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
        if (!getChannels() && (ret == -2))
        {
            ; // suppress errorcode MAINDATA_UNDERFLOW
        }
        else
        {
            printDecodeError(ret);
            m_f_playing = false; // seek for new syncword
        }
        if (!bytesDecoded)
            bytesDecoded = 2;
        return bytesDecoded;
    }
    else
    { // ret>=0
        if (f_setDecodeParamsOnce)
        {
            f_setDecodeParamsOnce = false;
            m_PlayingStartTime = millis();

            if (m_codec == CODEC_MP3)
            {
                setChannels(MP3GetChannels());
                setSampleRate(MP3GetSampRate());
                setBitsPerSample(MP3GetBitsPerSample());
                setBitrate(MP3GetBitrate());
            }
            if (m_codec == CODEC_AAC || m_codec == CODEC_M4A)
            {
                setChannels(AACGetChannels());
                setSampleRate(AACGetSampRate());
                setBitsPerSample(AACGetBitsPerSample());
                setBitrate(AACGetBitrate());
            }
            if (m_codec == CODEC_FLAC)
            {
                setChannels(FLACGetChannels());
                setSampleRate(FLACGetSampRate());
                setBitsPerSample(FLACGetBitsPerSample());
                setBitrate(FLACGetBitRate());
            }
            showCodecParams();
        }
        if (m_codec == CODEC_MP3)
        {
            m_validSamples = MP3GetOutputSamps() / getChannels();
        }
        if (m_codec == CODEC_AAC || m_codec == CODEC_M4A)
        {
            m_validSamples = AACGetOutputSamps() / getChannels();
        }
        if (m_codec == CODEC_FLAC)
        {
            m_validSamples = FLACGetOutputSamps() / getChannels();
        }
    }
    compute_audioCurrentTime(bytesDecoded);
    while (m_validSamples)
    {
        playChunk();
    }
    return bytesDecoded;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::compute_audioCurrentTime(int bd)
{
    static uint16_t loop_counter = 0;
    static int old_bitrate = 0;
    static uint64_t sum_bitrate = 0;
    static boolean f_CBR = true; // constant bitrate

    if (m_codec == CODEC_MP3)
    {
        setBitrate(MP3GetBitrate());
    } // if not CBR, bitrate can be changed
    if (m_codec == CODEC_M4A)
    {
        setBitrate(AACGetBitrate());
    } // if not CBR, bitrate can be changed
    if (m_codec == CODEC_AAC)
    {
        setBitrate(AACGetBitrate());
    } // if not CBR, bitrate can be changed
    if (m_codec == CODEC_FLAC)
    {
        setBitrate(FLACGetBitRate());
    } // if not CBR, bitrate can be changed
    if (!getBitRate())
        return;

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (m_avr_bitrate == 0)
    { // first time
        loop_counter = 0;
        old_bitrate = 0;
        sum_bitrate = 0;
        f_CBR = true;
        m_avr_bitrate = getBitRate();
        old_bitrate = getBitRate();
    }
    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if (loop_counter < 1000)
        loop_counter++;

    if ((old_bitrate != getBitRate()) && f_CBR)
    {
        if (audio_info)
            audio_info("VBR recognized, audioFileDuration is estimated");
        f_CBR = false; // variable bitrate
    }
    old_bitrate = getBitRate();

    if (!f_CBR)
    {
        if (loop_counter > 20 && loop_counter < 200)
        {
            // if VBR: m_avr_bitrate is average of the first values of m_bitrate
            sum_bitrate += getBitRate();
            m_avr_bitrate = sum_bitrate / (loop_counter - 20);
        }
    }
    else
    {
        if (loop_counter == 2)
            m_avr_bitrate = getBitRate();
    }
    m_audioCurrentTime += (float)bd * 8 / m_avr_bitrate;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::printDecodeError(int r)
{
    String e = "";
    if (m_codec == CODEC_MP3)
    {
        switch (r)
        {
        case ERR_MP3_NONE:
            e = "NONE";
            break;
        case ERR_MP3_INDATA_UNDERFLOW:
            e = "INDATA_UNDERFLOW";
            break;
        case ERR_MP3_MAINDATA_UNDERFLOW:
            e = "MAINDATA_UNDERFLOW";
            break;
        case ERR_MP3_FREE_BITRATE_SYNC:
            e = "FREE_BITRATE_SYNC";
            break;
        case ERR_MP3_OUT_OF_MEMORY:
            e = "OUT_OF_MEMORY";
            break;
        case ERR_MP3_NULL_POINTER:
            e = "NULL_POINTER";
            break;
        case ERR_MP3_INVALID_FRAMEHEADER:
            e = "INVALID_FRAMEHEADER";
            break;
        case ERR_MP3_INVALID_SIDEINFO:
            e = "INVALID_SIDEINFO";
            break;
        case ERR_MP3_INVALID_SCALEFACT:
            e = "INVALID_SCALEFACT";
            break;
        case ERR_MP3_INVALID_HUFFCODES:
            e = "INVALID_HUFFCODES";
            break;
        case ERR_MP3_INVALID_DEQUANTIZE:
            e = "INVALID_DEQUANTIZE";
            break;
        case ERR_MP3_INVALID_IMDCT:
            e = "INVALID_IMDCT";
            break;
        case ERR_MP3_INVALID_SUBBAND:
            e = "INVALID_SUBBAND";
            break;
        default:
            e = "ERR_UNKNOWN";
        }
        sprintf(chbuf, "MP3 decode error %d : %s", r, e.c_str());
        if (audio_info)
            audio_info(chbuf);
    }
    if (m_codec == CODEC_AAC)
    {
        switch (r)
        {
        case ERR_AAC_NONE:
            e = "NONE";
            break;
        case ERR_AAC_INDATA_UNDERFLOW:
            e = "INDATA_UNDERFLOW";
            break;
        case ERR_AAC_NULL_POINTER:
            e = "NULL_POINTER";
            break;
        case ERR_AAC_INVALID_ADTS_HEADER:
            e = "INVALID_ADTS_HEADER";
            break;
        case ERR_AAC_INVALID_ADIF_HEADER:
            e = "INVALID_ADIF_HEADER";
            break;
        case ERR_AAC_INVALID_FRAME:
            e = "INVALID_FRAME";
            break;
        case ERR_AAC_MPEG4_UNSUPPORTED:
            e = "MPEG4_UNSUPPORTED";
            break;
        case ERR_AAC_CHANNEL_MAP:
            e = "CHANNEL_MAP";
            break;
        case ERR_AAC_SYNTAX_ELEMENT:
            e = "SYNTAX_ELEMENT";
            break;
        case ERR_AAC_DEQUANT:
            e = "DEQUANT";
            break;
        case ERR_AAC_STEREO_PROCESS:
            e = "STEREO_PROCESS";
            break;
        case ERR_AAC_PNS:
            e = "PNS";
            break;
        case ERR_AAC_SHORT_BLOCK_DEINT:
            e = "SHORT_BLOCK_DEINT";
            break;
        case ERR_AAC_TNS:
            e = "TNS";
            break;
        case ERR_AAC_IMDCT:
            e = "IMDCT";
            break;
        case ERR_AAC_SBR_INIT:
            e = "SBR_INIT";
            break;
        case ERR_AAC_SBR_BITSTREAM:
            e = "SBR_BITSTREAM";
            break;
        case ERR_AAC_SBR_DATA:
            e = "SBR_DATA";
            break;
        case ERR_AAC_SBR_PCM_FORMAT:
            e = "SBR_PCM_FORMAT";
            break;
        case ERR_AAC_SBR_NCHANS_TOO_HIGH:
            e = "SBR_NCHANS_TOO_HIGH";
            break;
        case ERR_AAC_SBR_SINGLERATE_UNSUPPORTED:
            e = "BR_SINGLERATE_UNSUPPORTED";
            break;
        case ERR_AAC_NCHANS_TOO_HIGH:
            e = "NCHANS_TOO_HIGH";
            break;
        case ERR_AAC_RAWBLOCK_PARAMS:
            e = "RAWBLOCK_PARAMS";
            break;
        default:
            e = "ERR_UNKNOWN";
        }
        sprintf(chbuf, "AAC decode error %d : %s", r, e.c_str());
        if (audio_info)
            audio_info(chbuf);
    }
    if (m_codec == CODEC_FLAC)
    {
        switch (r)
        {
        case ERR_FLAC_NONE:
            e = "NONE";
            break;
        case ERR_FLAC_BLOCKSIZE_TOO_BIG:
            e = "BLOCKSIZE TOO BIG";
            break;
        case ERR_FLAC_RESERVED_BLOCKSIZE_UNSUPPORTED:
            e = "Reserved Blocksize unsupported";
            break;
        case ERR_FLAC_SYNC_CODE_NOT_FOUND:
            e = "SYNC CODE NOT FOUND";
            break;
        case ERR_FLAC_UNKNOWN_CHANNEL_ASSIGNMENT:
            e = "UNKNOWN CHANNEL ASSIGNMENT";
            break;
        case ERR_FLAC_RESERVED_CHANNEL_ASSIGNMENT:
            e = "RESERVED CHANNEL ASSIGNMENT";
            break;
        case ERR_FLAC_RESERVED_SUB_TYPE:
            e = "RESERVED SUB TYPE";
            break;
        case ERR_FLAC_PREORDER_TOO_BIG:
            e = "PREORDER TOO BIG";
            break;
        case ERR_FLAC_RESERVED_RESIDUAL_CODING:
            e = "RESERVED RESIDUAL CODING";
            break;
        case ERR_FLAC_WRONG_RICE_PARTITION_NR:
            e = "WRONG RICE PARTITION NR";
            break;
        case ERR_FLAC_BITS_PER_SAMPLE_TOO_BIG:
            e = "BITS PER SAMPLE > 16";
            break;
        case ERR_FLAG_BITS_PER_SAMPLE_UNKNOWN:
            e = "BITS PER SAMPLE UNKNOWN";
            break;
        default:
            e = "ERR_UNKNOWN";
        }
        sprintf(chbuf, "FLAC decode error %d : %s", r, e.c_str());
        if (audio_info)
            audio_info(chbuf);
    }
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set pinout for an external DAC.
///
/// @param BCLK Clock pin of I2S DAC.
/// @param LRC Frame pin of I2S DAC.
/// @param DOUT Data out pin of I2S DAC.
/// @param DIN Data in pin of I2S DAC.
/// @return true if it succeed to set I2S Pins.
/// @return false if it failed to set I2S Pins.
bool Audio::setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t DIN)
{

    m_pin_config.bck_io_num = BCLK;
    m_pin_config.ws_io_num = LRC; //  wclk
    m_pin_config.data_out_num = DOUT;
    m_pin_config.data_in_num = DIN;

    const esp_err_t result = i2s_set_pin((i2s_port_t)m_i2s_num, &m_pin_config);
    return (result == ESP_OK);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return current playing file size.
///
/// @return uint32_t File size in bytes.
uint32_t Audio::getFileSize()
{
    if (!audiofile)
        return 0;
    return audiofile.size();
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return current file position.
///
/// @return uint32_t File position in bytes.
uint32_t Audio::getFilePos()
{
    if (!audiofile)
        return 0;
    return audiofile.position();
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return file duration.
///
/// @return uint32_t File duration in seconds, 0 if no file active.
uint32_t Audio::getAudioFileDuration()
{
    if (m_f_localfile)
    {
        if (!audiofile)
            return 0;
    }
    if (m_f_webfile)
    {
        if (!m_contentlength)
            return 0;
    }

    if (m_avr_bitrate && m_codec == CODEC_MP3)
        m_audioFileDuration = 8 * m_audioDataSize / m_avr_bitrate;
    else if (m_avr_bitrate && m_codec == CODEC_WAV)
        m_audioFileDuration = 8 * m_audioDataSize / m_avr_bitrate;
    else if (m_avr_bitrate && m_codec == CODEC_M4A)
        m_audioFileDuration = 8 * m_audioDataSize / m_avr_bitrate;
    else if (m_avr_bitrate && m_codec == CODEC_AAC)
        m_audioFileDuration = 8 * m_audioDataSize / m_avr_bitrate;
    else if (m_codec == CODEC_FLAC)
        m_audioFileDuration = FLACGetAudioFileDuration();
    else
        return 0;
    return m_audioFileDuration;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return current playing time.
///
/// @return uint32_t Current playing time in seconds, 0 if no file active.
uint32_t Audio::getAudioCurrentTime()
{ // return current time in seconds
    return (uint32_t)m_audioCurrentTime;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that jump to an absolute time position within a file (works only with wav and mp3).
///
/// @param sec Position in seconds.
/// @return true if it succeed to set position.
/// @return false if it failed to set position.
bool Audio::setAudioPlayPosition(uint16_t sec)
{
    // Jump to an absolute position in time within an audio file
    // e.g. setAudioPlayPosition(300) sets the pointer at pos 5 min
    // works only with format mp3 or wav
    if (m_codec == CODEC_M4A)
        return false;
    if (sec > getAudioFileDuration())
        sec = getAudioFileDuration();
    uint32_t filepos = m_audioDataStart + (m_avr_bitrate * sec / 8);

    return setFilePos(filepos);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return total playing time.
///
/// @return uint32_t Total playing time in milliseconds.
uint32_t Audio::getTotalPlayingTime()
{
    // Is set to zero by a connectToXXX() and starts as soon as the first audio data is available,
    // the time counting is not interrupted by a 'pause / resume' and is not reset by a fileloop
    return millis() - m_PlayingStartTime;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set time offset (fast forward / rewind, only works with mp3, aac and wav files)
///
/// @param sec Time offset in seconds
/// @return true if it succeed to set time offset.
/// @return false if it failed to set time offset.
bool Audio::setTimeOffset(int sec)
{
    // fast forward or rewind the current position in seconds
    // audiosource must be a mp3, aac or wav file

    if (!audiofile || !m_avr_bitrate)
        return false;

    uint32_t oneSec = m_avr_bitrate / 8;                 // bytes decoded in one sec
    int32_t offset = oneSec * sec;                       // bytes to be wind/rewind
    uint32_t startAB = m_audioDataStart;                 // audioblock begin
    uint32_t endAB = m_audioDataStart + m_audioDataSize; // audioblock end

    if (m_codec == CODEC_MP3 || m_codec == CODEC_AAC || m_codec == CODEC_WAV || m_codec == CODEC_FLAC)
    {
        int32_t pos = getFilePos();
        pos += offset;
        if (pos < (int32_t)startAB)
            pos = startAB;
        if (pos >= (int32_t)endAB)
            pos = endAB;
        setFilePos(pos);
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set current file position.
///
/// @param pos File positition in bytes.
/// @return true if it succeed to set file position.
/// @return false if it failed to set file position.
bool Audio::setFilePos(uint32_t pos)
{
    if (!audiofile)
        return false;
    if (!m_avr_bitrate)
        return false;
    if (m_codec == CODEC_M4A)
        return false;
    m_f_playing = false;
    if (m_codec == CODEC_MP3)
        MP3Decoder_ClearBuffer();
    if (m_codec == CODEC_WAV)
    {
        while ((pos % 4) != 0)
            pos++;
    } // must be divisible by four
    if (m_codec == CODEC_FLAC)
        FLACDecoderReset();
    InBuff.resetBuffer();
    if (pos < m_audioDataStart)
        pos = m_audioDataStart;                                        // issue #96
    m_audioCurrentTime = (pos - m_audioDataStart) * 8 / m_avr_bitrate; // #96
    return audiofile.seek(pos);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set current playing file speed.
///
/// @param speed Speed (0 - 1).
/// @return true if it succeed to set speed.
/// @return false if it failed to set speed.
bool Audio::audioFileSeek(const float speed)
{
    // 0.5 is half speed
    // 1.0 is normal speed
    // 1.5 is one and half speed
    if (speed > 1.5 || speed < 0.25)
        return false;

    uint32_t srate = m_sampleRate * speed;
    i2s_set_sample_rates((i2s_port_t)m_i2s_num, srate);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set sample rate.
///
/// @param sampRate Sample rate in Hertz.
/// @return true if it succeed to set sample rate.
/// @return false if it failed to set sample rate.
bool Audio::setSampleRate(uint32_t sampRate)
{
    i2s_set_sample_rates((i2s_port_t)m_i2s_num, sampRate);
    m_sampleRate = sampRate;
    IIR_calculateCoefficients(m_gain0, m_gain1, m_gain2); // must be recalculated after each samplerate change
    return true;
}

///
/// @brief A method that return current stream sample rate.
///
/// @return uint32_t Sample rate in Hertz.
uint32_t Audio::getSampleRate()
{
    return m_sampleRate;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setBitsPerSample(int bits)
{
    if ((bits != 16) && (bits != 8))
        return false;
    m_bitsPerSample = bits;
    return true;
}

///
/// @brief A function that return bit rate per sample.
///
/// @return uint8_t bits per sample.
uint8_t Audio::getBitsPerSample()
{
    return m_bitsPerSample;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setChannels(int ch)
{
    if ((ch < 1) || (ch > 2))
        return false;
    m_channels = ch;
    return true;
}

///
/// @brief A method that return current channel number.
///
/// @return uint8_t 1 if mono and 2 if stereo.
uint8_t Audio::getChannels()
{
    return m_channels;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set bit rate.
///
/// @param br Bit rate in bits per second.
/// @return true if it succeed to set bit rate.
/// @return false if it failed to set bit rate.
bool Audio::setBitrate(int br)
{
    m_bitRate = br;
    if (br)
        return true;
    return false;
}

///
/// @brief A method that return current bit rate.
///
/// @return uint32_t Bit rate in bits per seconds.
uint32_t Audio::getBitRate()
{
    return m_bitRate;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set output dac.
///
/// @param internalDAC true for internal DAC and false for external DAC.
void Audio::setInternalDAC(bool internalDAC)
{

    m_f_internalDAC = internalDAC;

    if (internalDAC)
    {
        log_i("internal DAC");
        m_i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
        m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S_MSB);
        //m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S);
        i2s_set_pin((i2s_port_t)m_i2s_num, NULL);
    }
    else
    { // external DAC
        m_i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
        //m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S);
        i2s_set_pin((i2s_port_t)m_i2s_num, &m_pin_config);
    }
    i2s_driver_uninstall((i2s_port_t)m_i2s_num);
    i2s_driver_install((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set I2S communication format (bits oder, by default : I2S_COMM_FORMAT_I2S_MSB)
///
/// @details I2S_COMM_FORMAT is suitable for AC101, PCM5102A etc. and I2S_COMM_FORMAT_I2S_LSB is suitable for PT8211 etc. A LSBJ (least significant bit justified) used by japanese DAC is supported by standard LSB.
///
/// @param commFMT true for I2S_COMM_FORMAT_I2S_MSB and false for I2S_COMM_FORMAT_I2S_LSB.
void Audio::setI2SCommFMT_LSB(bool commFMT)
{
    // false: I2S communication format is by default I2S_COMM_FORMAT_I2S_MSB, right->left (AC101, PCM5102A)
    // true:  changed to I2S_COMM_FORMAT_I2S_LSB for some DACs (PT8211)
    //        Japanese or called LSBJ (Least Significant Bit Justified) format

    if (commFMT)
    {
        log_i("commFMT LSB");
        m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB);
        //m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_MSB); // v2.0.0
    }
    else
    {
        log_i("commFMT MSB");
        m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
        //m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S);
    }
    log_i("commFMT = %i", m_i2s_config.communication_format);
    i2s_driver_uninstall((i2s_port_t)m_i2s_num);
    i2s_driver_install((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playSample(int16_t sample[2])
{

    int16_t sample1[2];
    int16_t *s1;
    int16_t sample2[2];
    int16_t *s2 = sample2;
    int16_t sample3[2];
    int16_t *s3 = sample3;

    if (getBitsPerSample() == 8)
    { // Upsample from unsigned 8 bits to signed 16 bits
        sample[LEFTCHANNEL] = ((sample[LEFTCHANNEL] & 0xff) - 128) << 8;
        sample[RIGHTCHANNEL] = ((sample[RIGHTCHANNEL] & 0xff) - 128) << 8;
    }

    sample[LEFTCHANNEL] = sample[LEFTCHANNEL] >> 1; // half Vin so we can boost up to 6dB in filters
    sample[RIGHTCHANNEL] = sample[RIGHTCHANNEL] >> 1;

    // Filterchain, can commented out if not used
    sample = IIR_filterChain0(sample);
    sample = IIR_filterChain1(sample);
    sample = IIR_filterChain2(sample);
    //-------------------------------------------

    uint32_t s32 = Gain(sample); // vosample2lume;

    if (m_f_internalDAC)
    {
        s32 += 0x80008000;
    }

    esp_err_t err = i2s_write((i2s_port_t)m_i2s_num, (const char *)&s32, sizeof(uint32_t), &m_i2s_bytesWritten, 1000);
    if (err != ESP_OK)
    {
        log_e("ESP32 Errorcode %i", err);
        return false;
    }
    if (m_i2s_bytesWritten < 4)
    {
        log_e("Can't stuff any more in I2S..."); // increase waitingtime or outputbuffer
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set the audio equilizer.
/// @details See : https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
///
/// @param gainLowPass Low pass gain (between -40 and +6 dB).
/// @param gainBandPass Band pass gain (between -40 and +6 dB).
/// @param gainHighPass High pass gain (between -40 and +6 dB).
void Audio::setTone(int8_t gainLowPass, int8_t gainBandPass, int8_t gainHighPass)
{
    // see https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
    // values can be between -40 ... +6 (dB)

    m_gain0 = gainLowPass;
    m_gain1 = gainBandPass;
    m_gain2 = gainHighPass;

    IIR_calculateCoefficients(m_gain0, m_gain1, m_gain2);

    int16_t tmp[2];
    tmp[0] = 0;
    tmp[1] = 0;

    IIR_filterChain0(tmp, true); // flush the filter
    IIR_filterChain1(tmp, true); // flush the filter
    IIR_filterChain2(tmp, true); // flush the filter
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set mono / stereo.
///
/// @param m true for mono, false for stereo.
void Audio::forceMono(bool m)
{                      // #100 mono option
    m_f_forceMono = m; // false stereo, true mono
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that set balance.
///
/// @param bal Balance value (Left only : -16, right only : 16).
void Audio::setBalance(int8_t bal)
{ // bal -16...16
    if (bal < -16)
        bal = -16;
    if (bal > 16)
        bal = 16;
    m_balance = bal;
}
//---------------------------------------------------------------------------------------------------------------------
///
/// @brief A method that set volume.
///
/// @param vol Volume level (0 - 21).
void Audio::setVolume(uint8_t vol)
{ // vol 22 steps, 0...21
    if (vol > 21)
        vol = 21;
    m_vol = volumetable[vol];
}
//---------------------------------------------------------------------------------------------------------------------
///
/// @brief A method that return current volume.
///
/// @return uint8_t Volume (0 - 21).
uint8_t Audio::getVolume()
{
    for (uint8_t i = 0; i < 22; i++)
    {
        if (volumetable[i] == m_vol)
            return i;
    }
    m_vol = 12; // if m_vol not found in table
    return m_vol;
}
//---------------------------------------------------------------------------------------------------------------------

int32_t Audio::Gain(int16_t s[2])
{
    int32_t v[2];
    float step = (float)m_vol / 64;
    uint8_t l = 0, r = 0;

    if (m_balance < 0)
    {
        step = step * abs(m_balance) * 4;
        l = (uint8_t)(step);
    }
    if (m_balance > 0)
    {
        step = step * m_balance * 4;
        r = (uint8_t)(step);
    }

    v[LEFTCHANNEL] = (s[LEFTCHANNEL] * (m_vol - l)) >> 6;
    v[RIGHTCHANNEL] = (s[RIGHTCHANNEL] * (m_vol - r)) >> 6;

    return (v[RIGHTCHANNEL] << 16) | (v[LEFTCHANNEL] & 0xffff);
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return the number of stored bytes in the input buffer.
///
/// @return uint32_t Number of stored bytes in the input.
uint32_t Audio::inBufferFilled()
{
    // current audio input buffer fillsize in bytes
    return InBuff.bufferFilled();
}
//---------------------------------------------------------------------------------------------------------------------

///
/// @brief A method that return the number of free bytes in the input buffer.
///
/// @return uint32_t Number of free bytes in the input buffer.
uint32_t Audio::inBufferFree()
{
    // current audio input buffer free space in bytes
    return InBuff.freeSpace();
}
//---------------------------------------------------------------------------------------------------------------------
//            ***     D i g i t a l   b i q u a d r a t i c     f i l t e r     ***
//---------------------------------------------------------------------------------------------------------------------
void Audio::IIR_calculateCoefficients(int8_t G0, int8_t G1, int8_t G2)
{ // Infinite Impulse Response (IIR) filters

    // G1 - gain low shelf   set between -40 ... +6 dB
    // G2 - gain peakEQ      set between -40 ... +6 dB
    // G3 - gain high shelf  set between -40 ... +6 dB
    // https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/

    if (m_sampleRate < 1000)
        return; // fuse

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if (G0 < -40)
        G0 = -40;
    if (G0 > 6)
        G0 = 6; //  -40dB -> Vin*0.01  +6dB -> Vin*2
    if (G1 < -40)
        G1 = -40;
    if (G1 > 6)
        G1 = 6;
    if (G2 < -40)
        G2 = -40;
    if (G2 > 6)
        G2 = 6;

    const float FcLS = 500;    // Frequency LowShelf[Hz]
    const float FcPKEQ = 3000; // Frequency PeakEQ[Hz]
    const float FcHS = 6000;   // Frequency HighShelf[Hz]

    float K, norm, Q, Fc;
    double V;

    // LOWSHELF
    Fc = (float)FcLS / (float)m_sampleRate; // Cutoff frequency
    K = tan(PI * Fc);
    V = pow(10, fabs(G0) / 20.0);

    if (G0 >= 0)
    { // boost
        norm = 1 / (1 + sqrt(2) * K + K * K);
        m_filter[LOWSHELF].a0 = (1 + sqrt(2 * V) * K + V * K * K) * norm;
        m_filter[LOWSHELF].a1 = 2 * (V * K * K - 1) * norm;
        m_filter[LOWSHELF].a2 = (1 - sqrt(2 * V) * K + V * K * K) * norm;
        m_filter[LOWSHELF].b1 = 2 * (K * K - 1) * norm;
        m_filter[LOWSHELF].b2 = (1 - sqrt(2) * K + K * K) * norm;
    }
    else
    { // cut
        norm = 1 / (1 + sqrt(2 * V) * K + V * K * K);
        m_filter[LOWSHELF].a0 = (1 + sqrt(2) * K + K * K) * norm;
        m_filter[LOWSHELF].a1 = 2 * (K * K - 1) * norm;
        m_filter[LOWSHELF].a2 = (1 - sqrt(2) * K + K * K) * norm;
        m_filter[LOWSHELF].b1 = 2 * (V * K * K - 1) * norm;
        m_filter[LOWSHELF].b2 = (1 - sqrt(2 * V) * K + V * K * K) * norm;
    }

    // PEAK EQ
    Fc = (float)FcPKEQ / (float)m_sampleRate; // Cutoff frequency
    K = tan(PI * Fc);
    V = pow(10, fabs(G1) / 20.0);
    Q = 2.5; // Quality factor
    if (G1 >= 0)
    { // boost
        norm = 1 / (1 + 1 / Q * K + K * K);
        m_filter[PEAKEQ].a0 = (1 + V / Q * K + K * K) * norm;
        m_filter[PEAKEQ].a1 = 2 * (K * K - 1) * norm;
        m_filter[PEAKEQ].a2 = (1 - V / Q * K + K * K) * norm;
        m_filter[PEAKEQ].b1 = m_filter[PEAKEQ].a1;
        m_filter[PEAKEQ].b2 = (1 - 1 / Q * K + K * K) * norm;
    }
    else
    { // cut
        norm = 1 / (1 + V / Q * K + K * K);
        m_filter[PEAKEQ].a0 = (1 + 1 / Q * K + K * K) * norm;
        m_filter[PEAKEQ].a1 = 2 * (K * K - 1) * norm;
        m_filter[PEAKEQ].a2 = (1 - 1 / Q * K + K * K) * norm;
        m_filter[PEAKEQ].b1 = m_filter[PEAKEQ].a1;
        m_filter[PEAKEQ].b2 = (1 - V / Q * K + K * K) * norm;
    }

    // HIGHSHELF
    Fc = (float)FcHS / (float)m_sampleRate; // Cutoff frequency
    K = tan(PI * Fc);
    V = pow(10, fabs(G2) / 20.0);
    if (G2 >= 0)
    { // boost
        norm = 1 / (1 + sqrt(2) * K + K * K);
        m_filter[HIFGSHELF].a0 = (V + sqrt(2 * V) * K + K * K) * norm;
        m_filter[HIFGSHELF].a1 = 2 * (K * K - V) * norm;
        m_filter[HIFGSHELF].a2 = (V - sqrt(2 * V) * K + K * K) * norm;
        m_filter[HIFGSHELF].b1 = 2 * (K * K - 1) * norm;
        m_filter[HIFGSHELF].b2 = (1 - sqrt(2) * K + K * K) * norm;
    }
    else
    {
        norm = 1 / (V + sqrt(2 * V) * K + K * K);
        m_filter[HIFGSHELF].a0 = (1 + sqrt(2) * K + K * K) * norm;
        m_filter[HIFGSHELF].a1 = 2 * (K * K - 1) * norm;
        m_filter[HIFGSHELF].a2 = (1 - sqrt(2) * K + K * K) * norm;
        m_filter[HIFGSHELF].b1 = 2 * (K * K - V) * norm;
        m_filter[HIFGSHELF].b2 = (V - sqrt(2 * V) * K + K * K) * norm;
    }

    //    log_i("LS a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[0].a0, m_filter[0].a1, m_filter[0].a2,
    //                                                  m_filter[0].b1, m_filter[0].b2);
    //    log_i("EQ a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[1].a0, m_filter[1].a1, m_filter[1].a2,
    //                                                  m_filter[1].b1, m_filter[1].b2);
    //    log_i("HS a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[2].a0, m_filter[2].a1, m_filter[2].a2,
    //                                                  m_filter[2].b1, m_filter[2].b2);
}
//---------------------------------------------------------------------------------------------------------------------
int16_t *Audio::IIR_filterChain0(int16_t iir_in[2], bool clear)
{ // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum : uint8_t
    {
        in = 0,
        out = 1
    };
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if (clear)
    {
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0] = 0;
        iir_in[1] = 0;
    }

    inSample[LEFTCHANNEL] = (float)(iir_in[LEFTCHANNEL]);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    outSample[LEFTCHANNEL] = m_filter[0].a0 * inSample[LEFTCHANNEL] + m_filter[0].a1 * m_filterBuff[0][z1][in][LEFTCHANNEL] + m_filter[0].a2 * m_filterBuff[0][z2][in][LEFTCHANNEL] - m_filter[0].b1 * m_filterBuff[0][z1][out][LEFTCHANNEL] - m_filter[0].b2 * m_filterBuff[0][z2][out][LEFTCHANNEL];

    m_filterBuff[0][z2][in][LEFTCHANNEL] = m_filterBuff[0][z1][in][LEFTCHANNEL];
    m_filterBuff[0][z1][in][LEFTCHANNEL] = inSample[LEFTCHANNEL];
    m_filterBuff[0][z2][out][LEFTCHANNEL] = m_filterBuff[0][z1][out][LEFTCHANNEL];
    m_filterBuff[0][z1][out][LEFTCHANNEL] = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)outSample[LEFTCHANNEL];

    outSample[RIGHTCHANNEL] = m_filter[0].a0 * inSample[RIGHTCHANNEL] + m_filter[0].a1 * m_filterBuff[0][z1][in][RIGHTCHANNEL] + m_filter[0].a2 * m_filterBuff[0][z2][in][RIGHTCHANNEL] - m_filter[0].b1 * m_filterBuff[0][z1][out][RIGHTCHANNEL] - m_filter[0].b2 * m_filterBuff[0][z2][out][RIGHTCHANNEL];

    m_filterBuff[0][z2][in][RIGHTCHANNEL] = m_filterBuff[0][z1][in][RIGHTCHANNEL];
    m_filterBuff[0][z1][in][RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[0][z2][out][RIGHTCHANNEL] = m_filterBuff[0][z1][out][RIGHTCHANNEL];
    m_filterBuff[0][z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t)outSample[RIGHTCHANNEL];

    return iir_out;
}
//---------------------------------------------------------------------------------------------------------------------
int16_t *Audio::IIR_filterChain1(int16_t iir_in[2], bool clear)
{ // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum : uint8_t
    {
        in = 0,
        out = 1
    };
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if (clear)
    {
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0] = 0;
        iir_in[1] = 0;
    }

    inSample[LEFTCHANNEL] = (float)(iir_in[LEFTCHANNEL]);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    outSample[LEFTCHANNEL] = m_filter[1].a0 * inSample[LEFTCHANNEL] + m_filter[1].a1 * m_filterBuff[1][z1][in][LEFTCHANNEL] + m_filter[1].a2 * m_filterBuff[1][z2][in][LEFTCHANNEL] - m_filter[1].b1 * m_filterBuff[1][z1][out][LEFTCHANNEL] - m_filter[1].b2 * m_filterBuff[1][z2][out][LEFTCHANNEL];

    m_filterBuff[1][z2][in][LEFTCHANNEL] = m_filterBuff[1][z1][in][LEFTCHANNEL];
    m_filterBuff[1][z1][in][LEFTCHANNEL] = inSample[LEFTCHANNEL];
    m_filterBuff[1][z2][out][LEFTCHANNEL] = m_filterBuff[1][z1][out][LEFTCHANNEL];
    m_filterBuff[1][z1][out][LEFTCHANNEL] = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)outSample[LEFTCHANNEL];

    outSample[RIGHTCHANNEL] = m_filter[1].a0 * inSample[RIGHTCHANNEL] + m_filter[1].a1 * m_filterBuff[1][z1][in][RIGHTCHANNEL] + m_filter[1].a2 * m_filterBuff[1][z2][in][RIGHTCHANNEL] - m_filter[1].b1 * m_filterBuff[1][z1][out][RIGHTCHANNEL] - m_filter[1].b2 * m_filterBuff[1][z2][out][RIGHTCHANNEL];

    m_filterBuff[1][z2][in][RIGHTCHANNEL] = m_filterBuff[1][z1][in][RIGHTCHANNEL];
    m_filterBuff[1][z1][in][RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[1][z2][out][RIGHTCHANNEL] = m_filterBuff[1][z1][out][RIGHTCHANNEL];
    m_filterBuff[1][z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t)outSample[RIGHTCHANNEL];

    return iir_out;
}
//---------------------------------------------------------------------------------------------------------------------
int16_t *Audio::IIR_filterChain2(int16_t iir_in[2], bool clear)
{ // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum : uint8_t
    {
        in = 0,
        out = 1
    };
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if (clear)
    {
        memset(m_filterBuff, 0, sizeof(m_filterBuff)); // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0] = 0;
        iir_in[1] = 0;
    }

    inSample[LEFTCHANNEL] = (float)(iir_in[LEFTCHANNEL]);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    outSample[LEFTCHANNEL] = m_filter[2].a0 * inSample[LEFTCHANNEL] + m_filter[2].a1 * m_filterBuff[2][z1][in][LEFTCHANNEL] + m_filter[2].a2 * m_filterBuff[2][z2][in][LEFTCHANNEL] - m_filter[2].b1 * m_filterBuff[2][z1][out][LEFTCHANNEL] - m_filter[2].b2 * m_filterBuff[2][z2][out][LEFTCHANNEL];

    m_filterBuff[2][z2][in][LEFTCHANNEL] = m_filterBuff[2][z1][in][LEFTCHANNEL];
    m_filterBuff[2][z1][in][LEFTCHANNEL] = inSample[LEFTCHANNEL];
    m_filterBuff[2][z2][out][LEFTCHANNEL] = m_filterBuff[2][z1][out][LEFTCHANNEL];
    m_filterBuff[2][z1][out][LEFTCHANNEL] = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)outSample[LEFTCHANNEL];

    outSample[RIGHTCHANNEL] = m_filter[2].a0 * inSample[RIGHTCHANNEL] + m_filter[2].a1 * m_filterBuff[2][z1][in][RIGHTCHANNEL] + m_filter[2].a2 * m_filterBuff[2][z2][in][RIGHTCHANNEL] - m_filter[2].b1 * m_filterBuff[2][z1][out][RIGHTCHANNEL] - m_filter[2].b2 * m_filterBuff[2][z2][out][RIGHTCHANNEL];

    m_filterBuff[2][z2][in][RIGHTCHANNEL] = m_filterBuff[2][z1][in][RIGHTCHANNEL];
    m_filterBuff[2][z1][in][RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[2][z2][out][RIGHTCHANNEL] = m_filterBuff[2][z1][out][RIGHTCHANNEL];
    m_filterBuff[2][z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t)outSample[RIGHTCHANNEL];

    return iir_out;
}
