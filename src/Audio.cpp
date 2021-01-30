/*
 * Audio.cpp
 *
 *  Created on: Oct 26,2018
 *  Updated on: Jan 29,2021
 *      Author: Wolle (schreibfaul1)   ¯\_(ツ)_/¯
 *
 *  This library plays mp3 files from SD card or icy-webstream  via I2S,
 *  play Google TTS and plays also aac-streams
 *  no internal DAC, no DeltSigma
 *
 *  etrernal HW on I2S nessesary, e.g.MAX98357A
 *
 */

#include "Audio.h"
#include "mp3_decoder/mp3_decoder.h"
#include "aac_decoder/aac_decoder.h"

//---------------------------------------------------------------------------------------------------------------------
AudioBuffer::AudioBuffer() {
    ;
}

AudioBuffer::~AudioBuffer() {
    if(m_buffer)
        free(m_buffer);
    m_buffer = NULL;
}

size_t AudioBuffer::init() {
    if(psramInit()) {
        // PSRAM found, AudioBuffer will be allocated in PSRAM
        m_buffSize = m_buffSizePSRAM;
        if(m_buffer == NULL) {
            m_buffer = (uint8_t*) ps_calloc(m_buffSize, sizeof(uint8_t));
            m_buffSize = m_buffSizePSRAM - m_resBuffSize;
            if(m_buffer == NULL) {
                // not enough space in PSRAM, use ESP32 Flash Memory instead
                m_buffer = (uint8_t*) calloc(m_buffSize, sizeof(uint8_t));
                m_buffSize = m_buffSizeRAM - m_resBuffSize;
            }
        }
    } else {  // no PSRAM available, use ESP32 Flash Memory"
        m_buffSize = m_buffSizeRAM;
        m_buffer = (uint8_t*) calloc(m_buffSize, sizeof(uint8_t));
        m_buffSize = m_buffSizeRAM - m_resBuffSize;
    }
    if(!m_buffer)
        return 0;
    resetBuffer();
    return m_buffSize;
}

size_t AudioBuffer::freeSpace() {
    if(m_readPtr >= m_writePtr) {
        m_freeSpace = (m_readPtr - m_writePtr);
    } else {
        m_freeSpace = (m_endPtr - m_writePtr) + (m_readPtr - m_buffer);
    }
    if(m_f_start)
        m_freeSpace = m_buffSize;
    return m_freeSpace - 1;
}

size_t AudioBuffer::writeSpace() {
    if(m_readPtr >= m_writePtr) {
        m_writeSpace = (m_readPtr - m_writePtr - 1); // readPtr must not be overtaken
    } else {
        if(getReadPos() == 0)
            m_writeSpace = (m_endPtr - m_writePtr - 1);
        else
            m_writeSpace = (m_endPtr - m_writePtr);
    }
    if(m_f_start)
        m_writeSpace = m_buffSize - 1;
    return m_writeSpace;
}

size_t AudioBuffer::bufferFilled() {
    if(m_writePtr >= m_readPtr) {
        m_dataLength = (m_writePtr - m_readPtr);
    } else {
        m_dataLength = (m_endPtr - m_readPtr) + (m_writePtr - m_buffer);
    }
    return m_dataLength;
}

void AudioBuffer::bytesWritten(size_t bw) {
    m_writePtr += bw;
    if(m_writePtr == m_endPtr) {
        m_writePtr = m_buffer;
    }
    if(bw && m_f_start)
        m_f_start = false;
}

void AudioBuffer::bytesWasRead(size_t br) {
    m_readPtr += br;
    if(m_readPtr >= m_endPtr) {
        size_t tmp = m_readPtr - m_endPtr;
        m_readPtr = m_buffer + tmp;
    }
}

uint8_t* AudioBuffer::writePtr() {
    return m_writePtr;
}

uint8_t* AudioBuffer::readPtr() {
    size_t len = m_endPtr - m_readPtr;
    if(len < 1600) { // be sure the last frame is completed
        memcpy(m_endPtr, m_buffer, 1600);
    }
    return m_readPtr;
}

void AudioBuffer::resetBuffer() {
    m_writePtr = m_buffer;
    m_readPtr = m_buffer;
    m_endPtr = m_buffer + m_buffSize;
    m_f_start = true;
}

uint32_t AudioBuffer::getWritePos() {
    return m_writePtr - m_buffer;
}

uint32_t AudioBuffer::getReadPos() {
    return m_readPtr - m_buffer;
}
//---------------------------------------------------------------------------------------------------------------------
Audio::Audio(const uint8_t BCLK, const uint8_t LRC, const uint8_t DOUT) {
   clientsecure.setInsecure();  // if that can't be resolved update to ESP32 Arduino version 1.0.5-rc05 or higher
    //i2s configuration
    m_i2s_num = I2S_NUM_0; // i2s port number
    m_i2s_config.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    m_i2s_config.sample_rate          = 16000;
    m_i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    m_i2s_config.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
    m_i2s_config.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1; // high interrupt priority
    m_i2s_config.dma_buf_count        = 8;      // max buffers
    m_i2s_config.dma_buf_len          = 1024;   // max value
    m_i2s_config.use_apll             = APLL_ENABLE;
    m_i2s_config.tx_desc_auto_clear   = true;   // new in V1.0.1
    m_i2s_config.fixed_mclk           = I2S_PIN_NO_CHANGE;

    i2s_driver_install((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL);

    m_BCLK=BCLK;                       // Bit Clock
    m_LRC=LRC;                         // Left/Right Clock
    m_DOUT=DOUT;                       // Data Out
    setPinout(m_BCLK, m_LRC, m_DOUT, m_DIN);
    m_f_forceMono = false;

    m_filter[LEFTCHANNEL].a0  = 1;
    m_filter[LEFTCHANNEL].a1  = 0;
    m_filter[LEFTCHANNEL].a2  = 0;
    m_filter[LEFTCHANNEL].b1  = 0;
    m_filter[LEFTCHANNEL].b2  = 0;
    m_filter[RIGHTCHANNEL].a0 = 1;
    m_filter[RIGHTCHANNEL].a1 = 0;
    m_filter[RIGHTCHANNEL].a2 = 0;
    m_filter[RIGHTCHANNEL].b1 = 0;
    m_filter[RIGHTCHANNEL].b2 = 0;

}
//---------------------------------------------------------------------------------------------------------------------
void Audio::initInBuff() {
    static bool f_already_done = false;
    if(!f_already_done) {
        size_t size = InBuff.init();
        if(size == m_buffSizeRAM - m_resBuffSize) {
            sprintf(chbuf, "PSRAM not found, inputBufferSize = %u bytes", size - 1);
            if(audio_info)
                audio_info(chbuf);
            m_f_psram = false;
            f_already_done = true;
        }
        if(size == m_buffSizePSRAM - m_resBuffSize) {
            sprintf(chbuf, "PSRAM found, inputBufferSize = %u bytes", size - 1);
            if(audio_info)
                audio_info(chbuf);
            m_f_psram = true;
            f_already_done = true;
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::I2Sstart(uint8_t i2s_num) {
    return i2s_start((i2s_port_t) i2s_num);
}

esp_err_t Audio::I2Sstop(uint8_t i2s_num) {
    return i2s_stop((i2s_port_t) i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::i2s_mclk_pin_select(const uint8_t pin) {
    if(pin != 0 && pin != 1 && pin != 3) {
        ESP_LOGE(TAG, "Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", pin);
        return ESP_ERR_INVALID_ARG;
    }
    switch(pin){
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
Audio::~Audio() {
    I2Sstop(m_i2s_num);
    InBuff.~AudioBuffer();
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::reset() {
    stopSong();
    I2Sstop(0);
    I2Sstart(0);
    initInBuff(); // initialize InputBuffer if not already done
    InBuff.resetBuffer();
    MP3Decoder_FreeBuffers();
    AACDecoder_FreeBuffers();
    client.stop();
    client.flush(); // release memory
    clientsecure.stop();
    clientsecure.flush();

    sprintf(chbuf, "buffers freed, free Heap: %u bytes", ESP.getFreeHeap());
    if(audio_info) audio_info(chbuf);

    m_f_chunked = false;                                      // Assume not chunked
    m_f_ctseen = false;                                       // Contents type not seen yet
    m_f_firstmetabyte = false;
    m_f_firststream_ready = false;
    m_f_localfile = false;                                    // SPIFFS or SD? (onnecttoFS)
    m_f_playing = false;
    m_f_ssl = false;
    m_f_stream = false;
    m_f_swm = true;                                           // Assume no metaint (stream without metadata)
    m_f_webfile = false;                                      // Assume radiostream (connecttohost)
    m_f_webstream = false;

    m_playlistFormat = FORMAT_NONE;
    m_id3Size = 0;
    m_audioCurrentTime = 0;                                   // Reset playtimer
    m_audioFileDuration = 0;
    m_avr_bitrate = 0;                                        // the same as m_bitrate if CBR, median if VBR
    m_bitRate = 0;                                            // Bitrate still unknown
    m_bytesNotDecoded = 0;                                    // counts all not decodable bytes
    m_chunkcount = 0;                                         // for chunked streams
    m_codec = CODEC_NONE;
    m_contentlength = 0;                                      // If Content-Length is known, count it
    m_curSample = 0;
//    m_icyname = "";                                           // No StationName yet
    m_metaCount = 0;                                          // count bytes between metadata
    m_metaint = 0;                                            // No metaint yet
    m_LFcount = 0;                                            // For end of header detection
    m_st_remember = 0;                                        // Delete the last streamtitle hash
    m_totalcount = 0;                                         // Reset totalcount
    m_controlCounter = 0;                                     // Status within readID3data() and readWaveHeader()

    //TEST loop
    m_loop_point = 0;
    m_file_size = 0;
    //TEST loop

    memset(m_filterBuff, 0, sizeof(m_filterBuff));            // zero IIR filterbuffer
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttohost(const char* host, const char* user, const char* pwd) {
    // user and pwd for authentification only, can be empty

    if(strlen(host) == 0) {
        if(audio_info) audio_info("Hostaddress is empty");
        return false;
    }
    reset();

    sprintf(chbuf, "Connect to new host: \"%s\"", host);
    if(audio_info) audio_info(chbuf);

    // authentification
    String toEncode = String(user) + ":" + String(pwd);
    String authorization = base64::encode(toEncode);

    // initializationsequence
    int16_t pos_colon;                                        // Position of ":" in hostname
    int16_t pos_ampersand;                                    // Position of "&" in hostname
    uint16_t port = 80;                                       // Port number for host
    String extension = "/";                                   // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext = "";                                    // Host without extension and portnumber
    String headerdata = "";
    m_f_webstream = true;
    setDatamode(AUDIO_HEADER);                                // Handle header

    if(startsWith(host, "http://")) {
        host = host + 7;
        m_f_ssl = false;
    }

    if(startsWith(host, "https://")) {
        host = host +8;
        m_f_ssl = true;
        port = 443;
    }

    String s_host = host;

    // Is it a playlist?
    if(s_host.endsWith(".m3u")) {m_playlistFormat = FORMAT_M3U; m_datamode = AUDIO_PLAYLISTINIT;}
    if(s_host.endsWith(".pls")) {m_playlistFormat = FORMAT_PLS; m_datamode = AUDIO_PLAYLISTINIT;}
    if(s_host.endsWith(".asx")) {m_playlistFormat = FORMAT_ASX; m_datamode = AUDIO_PLAYLISTINIT;}

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    pos_colon = s_host.indexOf("/");                                  // Search for begin of extension
    if(pos_colon > 0) {                                             // Is there an extension?
        extension = s_host.substring(pos_colon);                      // Yes, change the default
        hostwoext = s_host.substring(0, pos_colon);                   // Host without extension
    }
    // In the URL there may be a portnumber
    pos_colon = s_host.indexOf(":");                                  // Search for separator
    pos_ampersand = s_host.indexOf("&");                              // Search for additional extensions
    if(pos_colon >= 0) {                                            // Portnumber available?
        if((pos_ampersand == -1) or (pos_ampersand > pos_colon)) {  // Portnumber is valid if ':' comes before '&' #82
            port = s_host.substring(pos_colon + 1).toInt();           // Get portnumber as integer
            hostwoext = s_host.substring(0, pos_colon);               // Host without portnumber
        }
    }
    sprintf(chbuf, "Connect to \"%s\" on port %d, extension \"%s\"", hostwoext.c_str(), port, extension.c_str());
    if(audio_info) audio_info(chbuf);

    extension.replace(" ", "%20");

    String resp = String("GET ") + extension + String(" HTTP/1.1\r\n")
                + String("Host: ") + hostwoext + String("\r\n")
                + String("Icy-MetaData:1\r\n")
                + String("Authorization: Basic " + authorization + "\r\n")
                + String("Connection: close\r\n\r\n");

    if(m_f_ssl == false) {
        if(client.connect(hostwoext.c_str(), port)) {
            if(audio_info) audio_info("Connected to server");
            client.print(resp);
            memcpy(m_lastHost, s_host.c_str(), s_host.length()+1);               // Remember the current s_host
            m_f_running = true;
            return true;
        }
    }
    if(m_f_ssl == true) {
        if(clientsecure.connect(hostwoext.c_str(), port)) {
            // if(audio_info) audio_info("SSL/TLS Connected to server");
            clientsecure.print(resp);
            sprintf(chbuf, "SSL has been established, free Heap: %u bytes", ESP.getFreeHeap());
            if(audio_info) audio_info(chbuf);
            memcpy(m_lastHost, s_host.c_str(), s_host.length()+1);               // Remember the current s_host
            m_f_running = true;
            return true;
        }
    }
    sprintf(chbuf, "Request %s failed!", s_host.c_str());
    if(audio_info) audio_info(chbuf);
    if(audio_showstation) audio_showstation("");
    if(audio_showstreamtitle) audio_showstreamtitle("");
    m_lastHost[0] = 0;
    return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------

//TEST loop
bool Audio::setFileLoop(bool input){
    m_f_loop = input;
    return input;
}
//-----------------------------------------------------------------------------------------------------------------------------------

bool Audio::connecttoSD(const char* sdfile) {
    return connecttoFS(SD, sdfile);
}
//-----------------------------------------------------------------------------------------------------------------------------------
bool Audio::connecttoFS(fs::FS &fs, const char* file) {

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

    reset(); // free buffers an set defaults

    uint16_t i = 0, j=0, s = 0;
    bool f_C3_seen = false;
    m_f_localfile = true;

    if(!s_file.startsWith("/")) s_file = "/" + s_file;
    while(s_file[i] != 0) {                                     // convert UTF8 to ASCII
        if(s_file[i] == 195){                                   // C3
            i++;
            f_C3_seen = true;
            continue;
        }
        path[j] = s_file[i];
        if(path[j] > 128 && path[j] < 189 && f_C3_seen == true) {
            s = ascii[path[j] - 129];
            if(s != 0) path[j] = s;                         // found a related ASCII sign
            f_C3_seen = false;
        }
        i++; j++;
    }
    path[j] = 0;
    memcpy(m_audioName, s_file.c_str() + 1, s_file.length()); // skip the first '/'
    sprintf(chbuf, "Reading file: \"%s\"", m_audioName);
    if(audio_info) audio_info(chbuf);
    
    if(fs.exists(path)) {
        audiofile = fs.open(path); // #86
    } else {
        audiofile = fs.open(s_file);
    }

    m_file_size = audiofile.size();//TEST loop
    
    if(!audiofile) {
        if(audio_info) audio_info("Failed to open file for reading");
        return false;
    }
    String afn = (String) audiofile.name();                   // audioFileName
    if(afn.endsWith(".mp3") || afn.endsWith(".MP3")) {        // MP3 section
        m_codec = CODEC_MP3;
        MP3Decoder_AllocateBuffers();
        sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
        if(audio_info) audio_info(chbuf);
        m_f_running = true;
        return true;
    } // end MP3 section

    if(afn.endsWith(".wav")) { // WAVE section
        m_codec = CODEC_WAV;
        m_f_running = true;
        return true;
    } // end WAVE section
    if(audio_info) audio_info("Neither wave nor mp3 format found");
    return false;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttospeech(const char* speech, const char* lang){

    reset();
    String   host = "translate.google.com.vn";
    String   path = "/translate_tts";
    uint32_t bytesCanBeWritten = 0;
    uint32_t bytesCanBeRead = 0;
    int32_t  bytesAddedToBuffer = 0;
    int16_t  bytesDecoded = 0;

    String tts =  path + "?ie=UTF-8&q=" + urlencode(speech) +
                  "&tl=" + lang + "&client=tw-ob";

    String resp = String("GET ") + tts + String(" HTTP/1.1\r\n")
                + String("Host: ") + host + String("\r\n")
                + String("User-Agent: GoogleTTS for ESP32/1.0.0\r\n")
                + String("Accept-Encoding: identity\r\n")
                + String("Accept: text/html\r\n")
                + String("Connection: close\r\n\r\n");

    if(!clientsecure.connect(host.c_str(), 443)) {
        Serial.println("Connection failed");
        return false;
    }
    clientsecure.print(resp);
    sprintf(chbuf, "SSL has been established, free Heap: %u bytes", ESP.getFreeHeap());
    if(audio_info) audio_info(chbuf);
    while(clientsecure.connected()) {  // read the header
        String line = clientsecure.readStringUntil('\n');
        line += "\n";
        // if(audio_info) audio_info(line.c_str());
        if(line == "\r\n") break;
    }

    m_codec = CODEC_MP3;
    AACDecoder_FreeBuffers();
    MP3Decoder_AllocateBuffers();
    sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
    if(audio_info) audio_info(chbuf);

    while(!playI2Sremains()) {
        ;
    }
    while(clientsecure.available() == 0) {
        ;
    }
    while(clientsecure.available() > 0) {

        bytesCanBeWritten = InBuff.writeSpace();
        bytesAddedToBuffer = clientsecure.read(InBuff.writePtr(), bytesCanBeWritten);
        if(bytesAddedToBuffer > 0) InBuff.bytesWritten(bytesAddedToBuffer);
        bytesCanBeRead = InBuff.bufferFilled();
        if(bytesCanBeRead > 1600) bytesCanBeRead = 1600;
        if(bytesCanBeRead == 1600) { // mp3 or aac frame complete?
            while(InBuff.bufferFilled() >= 1600) {
                bytesDecoded = sendBytes(InBuff.readPtr(), InBuff.bufferFilled());
                InBuff.bytesWasRead(bytesDecoded);
            }
        }
    }
    do {
        bytesDecoded = sendBytes(InBuff.readPtr(), InBuff.bufferFilled());
    } while(bytesDecoded > 100);

    memset(m_outBuff, 0, sizeof(m_outBuff));
    for(int i = 0; i < 4; i++) {
        m_validSamples = 2048;
        while(m_validSamples) {
            playChunk();
        }
    }
    while(!playI2Sremains()) {
        ;
    }
    MP3Decoder_FreeBuffers();
    stopSong();
    clientsecure.stop();
    clientsecure.flush();
    m_codec = CODEC_NONE;
    if(audio_eof_speech) audio_eof_speech(speech);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
String Audio::urlencode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for(int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if(c == ' ')
            encodedString += '+';
        else if(isalnum(c))
            encodedString += c;
        else {
            code1 = (c & 0xf) + '0';
            if((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if(c > 9) code0 = c - 10 + 'A';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    return encodedString;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::readWaveHeader(uint8_t* data, size_t len) {
    static uint32_t cs = 0;
    static uint8_t bts = 0;
    static size_t headerLength = 0;

    if(m_controlCounter == 0){
        m_controlCounter ++;
        if((*data != 'R') || (*(data + 1) != 'I') || (*(data + 2) != 'F') || (*(data + 3) != 'F')) {
            if(audio_info) audio_info("file has no RIFF tag");
            headerLength = 0;
            return -1; //false;
        }
        else{
            headerLength = 4;
            return 4; // ok
        }
    }

    if(m_controlCounter == 1){
        m_controlCounter ++;
        cs = (uint32_t) (*data + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24) - 8);
        headerLength += 4;
        return 4; // ok
    }

    if(m_controlCounter == 2){
        m_controlCounter ++;
        if((*data  != 'W') || (*(data + 1) != 'A') || (*(data + 2) != 'V') || (*(data + 3) != 'E')) {
            if(audio_info) audio_info("format tag is not WAVE");
            return -1;//false;
        }
        else {
            headerLength += 4;
            return 4;
        }
    }

    if(m_controlCounter == 3){
        if((*data  == 'f') && (*(data + 1) == 'm') && (*(data + 2) == 't')) {
            //if(audio_info) audio_info("format tag found");
            m_controlCounter ++;
            headerLength += 4;
            return 4;
        }
        else{
            headerLength += 4;
            return 4;
        }
    }

    if(m_controlCounter == 4){
        m_controlCounter ++;
        cs = (uint32_t) (*data + (*(data + 1) << 8));
        if(cs > 40) return -1; //false, something going wrong
        bts = cs - 16; // bytes to skip if fmt chunk is >16
        headerLength += 4;
        return 4;
    }

    if(m_controlCounter == 5){
        m_controlCounter ++;
        uint16_t fc  = (uint16_t) (*(data + 0)  + (*(data + 1)  << 8));         // Format code
        uint16_t nic = (uint16_t) (*(data + 2)  + (*(data + 3)  << 8));         // Number of interleaved channels
        uint32_t sr  = (uint32_t) (*(data + 4)  + (*(data + 5)  << 8) +
                                  (*(data + 6)  << 16) + (*(data + 7)  << 24)); // Samplerate
        uint32_t dr  = (uint32_t) (*(data + 8)  + (*(data + 9)  << 8) +
                                  (*(data + 10) << 16) + (*(data + 11) << 24)); // Datarate
        uint16_t dbs = (uint16_t) (*(data + 12) + (*(data + 13) << 8));         // Data block size
        uint16_t bps = (uint16_t) (*(data + 14) + (*(data + 15) << 8));         // Bits per sample
        if(audio_info) {
            sprintf(chbuf, "FormatCode=%u", fc);     audio_info(chbuf);
            sprintf(chbuf, "Channel=%u", nic);       audio_info(chbuf);
            sprintf(chbuf, "SampleRate=%u", sr);     audio_info(chbuf);
            sprintf(chbuf, "DataRate=%u", dr);       audio_info(chbuf);
            sprintf(chbuf, "DataBlockSize=%u", dbs); audio_info(chbuf);
            sprintf(chbuf, "BitsPerSample=%u", bps); audio_info(chbuf);
        }
        if(fc != 1) {
            if(audio_info) audio_info("format code is not 1 (PCM)");
            return -1 ; //false;
        }

        if(nic != 1 && nic != 2) {
            if(audio_info) audio_info("number of channels must be 1 or 2");
            return -1; //false;
        }

        if(bps != 8 && bps != 16) {
            if(audio_info) audio_info("bits per sample must be 8 or 16");
            return -1; //false;
        }
        setBitsPerSample(bps);
        setChannels(nic);
        setSampleRate(sr);
        m_bitRate = nic * sr * bps;
        sprintf(chbuf, "BitRate=%u", m_bitRate);
        if(audio_info) audio_info(chbuf);
        headerLength += 16;
        return 16; // ok
    }

    if(m_controlCounter == 6){
        m_controlCounter ++;
        headerLength += bts;
        return bts; // skip to data
    }

    if(m_controlCounter == 7){
        if((*(data + 0) == 'd') && (*(data + 1) == 'a') && (*(data + 2) == 't') && (*(data + 3) == 'a')){
            m_controlCounter ++;
            vTaskDelay(30);
            headerLength += 4;
            return 4;
        }
        else{
            headerLength ++;
            return 1;
        }
    }

    if(m_controlCounter == 8){
        m_controlCounter ++;
        cs = *(data + 0) + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24) - 44; //read chunkSize
        sprintf(chbuf, "DataLength = %u", cs);
        if(audio_info) audio_info(chbuf);
        headerLength += 4;
        return 4;

    }
    m_controlCounter = 100; // header succesfully read
    // log_i("headerLength %u", headerLength);
    m_loop_point = headerLength; // TEST loop, used in localFile
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::readID3Metadata(uint8_t *data, size_t len) {

    static int id3Size = 0;
    static int ehsz = 0;
    static String tag = "";
    static char frameid[5];
    static size_t framesize = 0;
    static bool compressed = false;

    if(m_controlCounter == 0){  /* read ID3 tag */
        m_controlCounter ++;
        id3Size = 0;
        ehsz = 0;
        if((*(data + 0) != 'I') || (*(data + 1) != 'D') || (*(data + 2) != '3')) {
        // if(audio_info) audio_info("file has no mp3 tag, skip metadata");
            m_loop_point = 0;//TEST loop
            return -1;
        }
        m_rev = *(data + 3);
        switch(m_rev){
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

        m_id3Size = *(data + 6);  //  ID3v2 size  4 * %0xxxxxxx
        m_id3Size = m_id3Size << 7;
        m_id3Size |= *(data + 7);
        m_id3Size = m_id3Size << 7;
        m_id3Size |= *(data + 8);
        m_id3Size = m_id3Size << 7;
        m_id3Size |= *(data + 9);

        m_id3Size += 10;

        // Every read from now may be unsync'd
        sprintf(chbuf, "ID3 framesSize=%i", m_id3Size);
        if(audio_info) audio_info(chbuf);

        sprintf(chbuf, "ID3 version=%i", m_rev);
        if(audio_info) audio_info(chbuf);

        if(m_rev == 2){
            if(audio_info) audio_info("ID3 version must be 3, skip all metadata");
            m_controlCounter = 98;
            return 0;
        }
        id3Size = m_id3Size;
        id3Size -= 10;

        return 10;
    }

    if(m_controlCounter == 1){
        m_controlCounter ++;
        if(m_f_exthdr) {
            if(audio_info) audio_info("ID3 extended header");
            ehsz = (*(data + 0) << 24) | (*(data + 1) << 16) | (*(data + 2) << 8) | (*(data + 3));
            id3Size -= 4;
            ehsz -= 4;
            return 4;
        }
        else{
            if(audio_info) audio_info("ID3 normal frames");
            return 0;
        }
    }

    if(m_controlCounter == 2){
        if(ehsz > 256) {
            ehsz -=256;
            id3Size -= 256;
            return 256;} // Throw it away
        else           {
            m_controlCounter ++;
            id3Size -= ehsz;
            return ehsz;} // Throw it away
    }

    if(m_controlCounter == 3){
        if(id3Size == 0){
            m_controlCounter = 99;
            return 0;
        }
        m_controlCounter ++;
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = *(data + 3);
        frameid[4] = 0;
        tag = frameid;
        id3Size -= 4;
        if(frameid[0] == 0 && frameid[1] == 0 && frameid[2] == 0 && frameid[3] == 0) {
            // We're in padding
            m_controlCounter = 98;
        }
        return 4;
    }

    if(m_controlCounter == 4){
        m_controlCounter = 6;
        framesize = (*(data + 0) << 24) | (*(data + 1) << 16) | (*(data + 2) << 8) | (*(data + 3));
        id3Size -= 4;
        uint8_t flag = *(data + 4); // skip 1st flag
        (void) flag;
        id3Size--;
        compressed = (*(data + 5)) & 0x80; // Frame is compressed using [#ZLIB zlib] with 4 bytes for 'decompressed
                                           // size' appended to the frame header.
        id3Size--;
        int decompsize = 0;
        if(compressed){
            log_i("iscompressed");
            decompsize = (*(data + 6) << 24) | (*(data + 7) << 16) | (*(data + 8) << 8) | (*(data + 9));
            id3Size -= 4;
            (void) decompsize;
            return 6 + 4;
        }
        return 6;
    }

    if(m_controlCounter == 5){ // skip frame
        if(framesize > 256){
            framesize -= 256;
            id3Size -= 256;
            return 256;
        }
        else {
            m_controlCounter = 3; // check next frame
            id3Size -= framesize;
            return framesize;
        }
    }

    if(m_controlCounter == 6){ // Read the value
        m_controlCounter = 5;  // only read 256 bytes
        uint32_t i = 0;
        uint16_t j = 0, k = 0, m = 0;
        bool bitorder = false;
        uint8_t uni_h = 0;
        uint8_t uni_l = 0;
        char value[256];
        char ch = *(data + 0);
        bool isUnicode = (ch==1) ? true : false;

        if(tag == "APIC") { // a image embedded in file, passing it to external function
            //log_i("it's a image");
            isUnicode = false;
            if(m_f_localfile){
                size_t pos = m_id3Size - id3Size;
                if(audio_id3image) audio_id3image(audiofile, pos, framesize);
            }
            return 0;
        }

        size_t fs = framesize;
        if(fs >255) fs = 255;
        for(int i=0; i<fs; i++){
            value[i] = *(data + i);
        }
        framesize -= fs;
        id3Size -= fs;
        value[fs] = 0;
        i = fs;
        if(isUnicode && fs > 1) {  // convert unicode to utf-8 U+0020...U+07FF
            j = 0;
            m = 0;

            while(m < i - 1) {
                if((value[m] == 0xFE) && (value[m + 1] == 0xFF)) {
                    bitorder = true;
                    j = m + 2;
                }  // MSB/LSB
                if((value[m] == 0xFF) && (value[m + 1] == 0xFE)) {
                    bitorder = false;
                    j = m + 2;
                }  //LSB/MSB
                m++;
            } // seek for last bitorder
            m = 0;
            if(j > 0) {
                for(k = j; k < i - 1; k += 2) {
                    if(bitorder == true) {
                        uni_h = value[k];
                        uni_l = value[k + 1];
                    }
                    else {
                        uni_l = value[k];
                        uni_h = value[k + 1];
                    }
                    uint16_t uni_hl = (uni_h << 8) + uni_l;
                    uint8_t utf8_h = (uni_hl >> 6); // div64
                    uint8_t utf8_l = uni_l;
                    if(utf8_h > 3) {
                        utf8_h += 0xC0;
                        if(uni_l < 0x40)
                            utf8_l = uni_l + 0x80;
                        else if(uni_l < 0x80)
                            utf8_l = uni_l += 0x40;
                        else if(uni_l < 0xC0)
                            utf8_l = uni_l;
                        else
                            utf8_l = uni_l - 0x40;
                    }
                    if(utf8_h > 3) {
                        value[m] = utf8_h;
                        m++;
                    }
                    value[m] = utf8_l;
                    m++;
                }
            }
            value[m] = 0;
            i = m;
        }
        if(!isUnicode){
            j = 0;
            k = 0;
            while(j < i) {
                if(value[j] == 0x0A) value[j] = 0x20; // replace LF by space
                if(value[j] > 0x1F) {
                    value[k] = value[j];
                    k++;
                }
                j++;
            } //remove non printables
            if(k>0) value[k] = 0; else value[0] = 0; // new termination
        }
        chbuf[0] = 0;
        //if(tag == "COMM") sprintf(chbuf, "Comment: %s", value);
        if(tag == "OWNE") sprintf(chbuf, "Ownership: %s", value);
        //if(tag == "PRIV") sprintf(chbuf, "Private: %s", value);
        if(tag == "SYLT") sprintf(chbuf, "SynLyrics: %s", value);
        if(tag == "TALB") sprintf(chbuf, "Album: %s", value);
        if(tag == "TBPM") sprintf(chbuf, "BeatsPerMinute: %s", value);
        if(tag == "TCMP") sprintf(chbuf, "Compilation: %s", value);
        if(tag == "TCOM") sprintf(chbuf, "Composer: %s", value);
        if(tag == "TCON") sprintf(chbuf, "ContentType: %s", value);
        if(tag == "TCOP") sprintf(chbuf, "Copyright: %s", value);
        if(tag == "TDAT") sprintf(chbuf, "Date: %s", value);
        if(tag == "TEXT") sprintf(chbuf, "Lyricist: %s", value);
        if(tag == "TIME") sprintf(chbuf, "Time: %s", value);
        if(tag == "TIT1") sprintf(chbuf, "Grouping: %s", value);
        if(tag == "TIT2") sprintf(chbuf, "Title: %s", value);
        if(tag == "TIT3") sprintf(chbuf, "Subtitle: %s", value);
        if(tag == "TLAN") sprintf(chbuf, "Language: %s", value);
        if(tag == "TLEN") sprintf(chbuf, "Length (ms): %s", value);
        if(tag == "TMED") sprintf(chbuf, "Media: %s", value);
        if(tag == "TOAL") sprintf(chbuf, "OriginalAlbum: %s", value);
        if(tag == "TOPE") sprintf(chbuf, "OriginalArtist: %s", value);
        if(tag == "TORY") sprintf(chbuf, "OriginalReleaseYear: %s", value);
        if(tag == "TPE1") sprintf(chbuf, "Artist: %s", value);
        if(tag == "TPE2") sprintf(chbuf, "Band: %s", value);
        if(tag == "TPE3") sprintf(chbuf, "Conductor: %s", value);
        if(tag == "TPE4") sprintf(chbuf, "InterpretedBy: %s", value);
        if(tag == "TPOS") sprintf(chbuf, "PartOfSet: %s", value);
        if(tag == "TPUB") sprintf(chbuf, "Publisher: %s", value);
        if(tag == "TRCK") sprintf(chbuf, "Track: %s", value);
        if(tag == "TSSE") sprintf(chbuf, "SettingsForEncoding: %s", value);
        if(tag == "TRDA") sprintf(chbuf, "RecordingDates: %s", value);
        if(tag == "TXXX") sprintf(chbuf, "UserDefinedText: %s", value);
        if(tag == "TYER") sprintf(chbuf, "Year: %s", value);
        if(tag == "USER") sprintf(chbuf, "TermsOfUse: %s", value);
        if(tag == "USLT") sprintf(chbuf, "Lyrics: %s", value);
        if(tag == "WOAR") sprintf(chbuf, "OfficialArtistWebpage: %s", value);
        if(tag == "XDOR") sprintf(chbuf, "OriginalReleaseTime: %s", value);

        if(chbuf[0] != 0) if(audio_id3data) audio_id3data(chbuf);

        return fs;
    }

    if(m_controlCounter == 98){ // skip all ID3 metadata
        if(id3Size > 256) {
            id3Size -=256;
            return 256;
        } // Throw it away
        else           {
            m_controlCounter = 99;
            return id3Size;
        } // Throw it away
    }

    if(m_controlCounter == 99){ //  exist another ID3tag?
        m_loop_point += m_id3Size;
        vTaskDelay(30);
        if((*(data + 0) == 'I') && (*(data + 1) == 'D') && (*(data + 2) == '3')) {
            m_controlCounter = 0;
            return 0;
        }
        else {
            m_controlCounter = 100; // ok
            return 0;
        }
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::stopSong() {
    if(m_f_running) {
        m_f_running = false;
        audiofile.close();
    }
    memset(m_outBuff, 0, sizeof(m_outBuff));     //Clear OutputBuffer
    i2s_zero_dma_buffer((i2s_port_t) m_i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playI2Sremains() { // returns true if all dma_buffs flushed
    static uint8_t dma_buf_count = 0;
    // there is no  function to see if dma_buff is empty. So fill the dma completely.
    // As soon as all remains played this function returned. Or you can take this to create a short silence.
    if(m_sampleRate == 0) setSampleRate(96000);
    if(m_channels == 0) setChannels(2);
    if(getBitsPerSample() > 8) memset(m_outBuff,   0, sizeof(m_outBuff));     //Clear OutputBuffer (signed)
    else                       memset(m_outBuff, 128, sizeof(m_outBuff));     //Clear OutputBuffer (unsigned, PCM 8u)

    //play remains and then flush dmaBuff
    m_validSamples = m_i2s_config.dma_buf_len;
    while(m_validSamples) {
        playChunk();
    }
    if(dma_buf_count < m_i2s_config.dma_buf_count){
        dma_buf_count++;
        return false;
    }
    dma_buf_count = 0;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::pauseResume() {
    bool retVal = false;
    if(m_f_localfile || m_f_webstream) {
        m_f_running = !m_f_running;
        retVal = true;
        if(!m_f_running) {
            memset(m_outBuff, 0, sizeof(m_outBuff));               //Clear OutputBuffer
            i2s_zero_dma_buffer((i2s_port_t) m_i2s_num);
        }
    }
    return retVal;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playChunk() {
    // If we've got data, try and pump it out..
    int16_t sample[2];
    if(getBitsPerSample() == 8) {
        if(m_channels == 1) {
            while(m_validSamples) {
                uint8_t x =  m_outBuff[m_curSample] & 0x00FF;
                uint8_t y = (m_outBuff[m_curSample] & 0xFF00) >> 8;
                sample[LEFTCHANNEL]  = x;
                sample[RIGHTCHANNEL] = x;
                while(1) {
                    if(playSample(sample)) break;
                } // Can't send?
                sample[LEFTCHANNEL]  = y;
                sample[RIGHTCHANNEL] = y;
                while(1) {
                    if(playSample(sample)) break;
                } // Can't send?
                m_validSamples--;
                m_curSample++;
            }
        }
        if(m_channels == 2) {
            while(m_validSamples) {
                uint8_t x =  m_outBuff[m_curSample] & 0x00FF;
                uint8_t y = (m_outBuff[m_curSample] & 0xFF00) >> 8;
                if(!m_f_forceMono) { // stereo mode
                    sample[LEFTCHANNEL]  = x;
                    sample[RIGHTCHANNEL] = y;
                }
                else { // force mono
                    uint8_t xy = (x + y) / 2;
                    sample[LEFTCHANNEL]  = xy;
                    sample[RIGHTCHANNEL] = xy;
                }

                while(1) {
                    if(playSample(sample)) break;
                } // Can't send?
                m_validSamples--;
                m_curSample++;
            }
        }
        m_curSample = 0;
        return true;
    }
    if(getBitsPerSample() == 16) {
        if(m_channels == 1) {
            while(m_validSamples) {
                sample[LEFTCHANNEL]  = m_outBuff[m_curSample];
                sample[RIGHTCHANNEL] = m_outBuff[m_curSample];
                if(!playSample(sample)) {
                    return false;
                } // Can't send
                m_validSamples--;
                m_curSample++;
            }
        }
        if(m_channels == 2) {
            while(m_validSamples) {
                if(!m_f_forceMono) { // stereo mode
                    sample[LEFTCHANNEL]  = m_outBuff[m_curSample * 2];
                    sample[RIGHTCHANNEL] = m_outBuff[m_curSample * 2 + 1];
                }
                else { // mono mode, #100
                    int16_t xy = (m_outBuff[m_curSample * 2] + m_outBuff[m_curSample * 2 + 1]) / 2;
                    sample[LEFTCHANNEL] = xy;
                    sample[RIGHTCHANNEL] = xy;
                }
                if(!playSample(sample)) {
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
void Audio::loop() {
    // - localfile - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_localfile) {                                      // Playing file fron SPIFFS or SD?
        processLocalFile();
    }
    // - webstream - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_webstream) {                                      // Playing file from URL?
        processWebStream();
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processLocalFile() {
    static boolean lastChunk = false;
    int bytesDecoded = 0;
    if(audiofile && m_f_running && m_f_localfile) {
        uint32_t bytesCanBeWritten = 0;
        uint32_t bytesCanBeRead = 0;
        int32_t bytesAddedToBuffer = 0;

        bytesCanBeWritten = InBuff.writeSpace();
        
        //TEST loop 
        if((m_file_size == getFilePos()) && m_f_loop /* && m_f_stream */){  //eof
            sprintf(chbuf, "loop from:%u to=%u", getFilePos(), m_loop_point);
            if(audio_info) audio_info(chbuf);
            setFilePos(m_loop_point);
        }
        //TEST loop
        
        bytesAddedToBuffer = audiofile.read(InBuff.writePtr(), bytesCanBeWritten);

        /*sprintf(chbuf, "Rp:%u Wp:%u Bw:%u Br:%u Bb:%u", InBuff.readPtr(), InBuff.writePtr(), bytesCanBeWritten, InBuff.bufferFilled(), bytesAddedToBuffer);//TEST
        if(audio_info) audio_info(chbuf);//TEST*/

        if(bytesAddedToBuffer > 0) InBuff.bytesWritten(bytesAddedToBuffer);
        bytesCanBeRead = InBuff.bufferFilled();
        if(bytesCanBeRead > 1600) bytesCanBeRead = 1600;
        if(bytesCanBeRead == 1600) { // mp3 or aac frame complete?
            if(!m_f_stream) {
                if(!playI2Sremains()) return; // release the thread, continue on the next pass
                m_f_stream = true;
                if(audio_info) audio_info("stream ready");
            }
            if(m_controlCounter != 100){
                if(m_codec == CODEC_WAV){
                    int res = readWaveHeader(InBuff.readPtr(), bytesCanBeRead);
                    if(res >= 0) bytesDecoded = res;
                    else{ // error, skip header
                        m_controlCounter = 100;
                    }
                }
                if(m_codec == CODEC_MP3){
                    int res = readID3Metadata(InBuff.readPtr(), bytesCanBeRead);
                    if(res >= 0) bytesDecoded = res;
                    else{ // error, skip header
                        m_controlCounter = 100;
                    }
                }
            }
            else {
                bytesDecoded = sendBytes(InBuff.readPtr(), bytesCanBeRead);
            }
            if(bytesDecoded > 0) InBuff.bytesWasRead(bytesDecoded);
            if(bytesDecoded < 0) {  // no syncword found or decode error, try next chunk
                InBuff.bytesWasRead(200); // try next chunk
                m_bytesNotDecoded += 200;
                return;
            }
            lastChunk = false;
            return;
        }
        if(!bytesAddedToBuffer) {  // eof
            if(lastChunk == false) {
                if(bytesCanBeRead) {
                    bytesDecoded = sendBytes(InBuff.readPtr(), bytesCanBeRead); // play last chunk(s)
                    if(bytesDecoded > 0) InBuff.bytesWasRead(bytesDecoded);
                    if(bytesDecoded < 100) { // unlikely framesize
                        lastChunk = true;
                        return;
                    }
                    return;
                }
                lastChunk = true;
                return; // release the thread, continue on the next pass
            }
            if(!playI2Sremains()) return;
            stopSong();
            m_f_stream = false;
            m_f_localfile = false;
            MP3Decoder_FreeBuffers();
            sprintf(chbuf, "End of file \"%s\"", m_audioName);
            if(audio_info) audio_info(chbuf);
            if(audio_eof_mp3) audio_eof_mp3(m_audioName);
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processWebStream() {
    if(m_f_running && m_f_webstream) {
        uint32_t bytesCanBeWritten = 0;
        int16_t bytesAddedToBuffer = 0;
        const uint16_t maxFrameSize = 1600;                 // every mp3/aac frame is not bigger
        static uint32_t chunksize = 0;                      // chunkcount read from stream
        int32_t availableBytes = 0;                         // available bytes in stream
        static int bytesDecoded = 0;
        static uint32_t cnt0 = 0;
        static uint32_t tmr_1s = 0;                         // timer 1 sec
        bool f_tmr_1s = false;
        if((tmr_1s + 1000) < millis()) {
            f_tmr_1s = true;                                // flag will be set every second for one loop only
            tmr_1s = millis();
        }

        if(m_f_ssl == false) availableBytes = client.available();            // available from stream
        if(m_f_ssl == true)  availableBytes = clientsecure.available();      // available from stream

        if((m_f_firststream_ready == false) && (availableBytes > 0)) { // first streamdata recognized
            m_f_firststream_ready = true;
            m_f_stream = false;
            m_bytectr = 0;
        }

        if(((m_datamode == AUDIO_DATA) || (m_datamode == AUDIO_SWM)) && (m_metaCount > 0)) {
            bytesCanBeWritten = InBuff.writeSpace();
            uint32_t x = min(m_metaCount, uint32_t(bytesCanBeWritten));
            if((m_f_chunked) && (x > m_chunkcount)) {
                x = m_chunkcount;
            }

            if((m_f_ssl == false) && (availableBytes > 0)) {
                bytesAddedToBuffer = client.read(InBuff.writePtr(), x);
            }

            if((m_f_ssl == true) && (availableBytes > 0)) {
                bytesAddedToBuffer = clientsecure.read(InBuff.writePtr(), x);
            }

            if(bytesAddedToBuffer > 0) {
                if(m_f_webfile) {
                    m_bytectr += bytesAddedToBuffer;  // Pull request #42
                }
                m_metaCount -= bytesAddedToBuffer;
                if(m_f_chunked) {
                    m_chunkcount -= bytesAddedToBuffer;
                }
                InBuff.bytesWritten(bytesAddedToBuffer);
            }

            // waiting for buffer filled, set tresholds before the stream is starting
            if((InBuff.bufferFilled() > 6000 && !m_f_psram) || (InBuff.bufferFilled() > 80000 && m_f_psram)) {
                if(m_f_stream == false) {
                    m_f_stream = true;
                    cnt0 = 0;
                    uint16_t filltime = millis() - m_t0;
                    if(audio_info) audio_info("stream ready");
                    sprintf(chbuf, "buffer filled in %d ms", filltime);
                    if(audio_info) audio_info(chbuf);
                }
            }
            else {
                if(m_f_stream == false) {
                    if(cnt0 == 0) {
                        m_t0 = millis();
                        if(audio_info) audio_info("inputbuffer is being filled");
                    }
                    cnt0++;
                }
            }

            if((InBuff.bufferFilled() > maxFrameSize) && (m_f_stream == true)) { // fill > framesize?
                if(m_f_webfile){  // can contain ID3 metadata, issue #83
                    if(m_controlCounter != 100){
                        if(m_codec == CODEC_WAV){
                            int res = readWaveHeader(InBuff.readPtr(), InBuff.bufferFilled());
                            if(res >= 0) bytesDecoded = res;
                            else{ // error, skip header
                                m_controlCounter = 100;
                            }
                        }
                        if(m_codec == CODEC_MP3){
                            int res = readID3Metadata(InBuff.readPtr(), InBuff.bufferFilled());
                            if(res >= 0) bytesDecoded = res;
                            else{ // error, skip header
                                m_controlCounter = 100;
                            }
                        }
                    }
                    else {
                        bytesDecoded = sendBytes(InBuff.readPtr(), maxFrameSize);
                    }
                }
                else { // not a webfile
                    bytesDecoded = sendBytes(InBuff.readPtr(), InBuff.bufferFilled());
                }

                if(bytesDecoded < 0) {  // no syncword found or decode error, try next chunk
                    InBuff.bytesWasRead(200); // try next chunk
                    m_bytesNotDecoded += 200;
                    return;
                }
                else {
                    InBuff.bytesWasRead(bytesDecoded);
                }
            }
            else { // InBuff almost empty, contains no complete mp3/aac frame
                static uint8_t cnt_slow = 0;
                if(m_f_stream == true) {
                    cnt_slow ++;

                    if(f_tmr_1s) {
                        if(cnt_slow > 18)
                            if(audio_info) audio_info("slow stream, dropouts are possible");
                        f_tmr_1s = false;
                        cnt_slow = 0;
                    }
                }
            }

            if(m_metaCount == 0) {
                if(m_datamode == AUDIO_SWM) {
                    m_metaCount = 16000; //mms has no metadata
                }
                else {
                    m_datamode = AUDIO_METADATA;
                    m_f_firstmetabyte = true;
                }
            }
        }
        else { //!=DATA
            if(m_datamode == AUDIO_PLAYLISTDATA) {
                if(m_t0 + 49 < millis()) {
                    processControlData('\n');                       // send LF
                }
            }
            int16_t x = 0;
            if(m_f_chunked && (m_datamode != AUDIO_HEADER)) {
                if(m_chunkcount > 0) {
                    if(m_f_ssl == false)
                        x = client.read();
                    else
                        x = clientsecure.read();

                    if(x >= 0) {
                        processControlData(x);
                        m_chunkcount--;
                    }
                }
            }
            else {
                if(m_f_ssl == false)
                    x = client.read();
                else
                    x = clientsecure.read();
                if(x >= 0) {
                    processControlData(x);
                }
            }
            if(m_datamode == AUDIO_DATA) {
                m_metaCount = m_metaint;
                if(m_metaint == 0) {
                    m_datamode = AUDIO_SWM;         // stream without metadata, can be mms
                }
            }
            if(m_datamode == AUDIO_SWM) {
                m_metaCount = 16000;
            }
        }

        if(m_f_firststream_ready == true) {
            static uint32_t loopCnt = 0;            // count loops if clientbuffer is empty
            if(availableBytes == 0) {              // empty buffer, broken stream or bad bitrate?
                loopCnt++;
                if(loopCnt > 200000) {             // wait several seconds
                    loopCnt = 0;
                    if(audio_info) audio_info("Stream lost -> try new connection");
                    connecttohost(m_lastHost);      // try a new connection
                }
                if(m_f_webfile) { // stream from fileserver with known content-length
                    if((uint32_t) m_bytectr >= (uint32_t) m_contentlength - 10) { // received everything?
                        if(InBuff.bufferFilled() < maxFrameSize) {   // and buff almost empty, issue #66
                            sprintf(chbuf, "End of webstream: \"%s\"", m_lastHost);
                            if(audio_info) audio_info(chbuf);
                            if(audio_eof_stream) audio_eof_stream(m_lastHost);
                            stopSong(); // Correct close when play known length sound (#74)
                        }
                    }
                }
            }
            else {
                loopCnt = 0;
            }
        }

        if(m_f_chunked) {
            if(m_datamode == AUDIO_HEADER){
                chunksize = 0;
                return;
            }

            if(m_chunkcount == 0) {             // Expecting a new chunkcount?
                int b;
                if(m_f_ssl == false) {
                    b = client.read();
                }
                if(m_f_ssl == true) {
                    b = clientsecure.read();
                }
                if(b < 1) return;

                if(b == '\r') {;}
                else if(b == '\n') {
                    m_chunkcount = chunksize;
                    chunksize = 0;
                }
                else {
                    // We have received a hexadecimal character.  Decode it and add to the result.
                    b = toupper(b) - '0';                       // Be sure we have uppercase
                    if(b > 9) b = b - 7;                        // Translate A..F to 10..15
                    chunksize = (chunksize << 4) + b;
                }
            }
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------
void Audio::processControlData(uint8_t b) {
    if(m_datamode == AUDIO_NONE){
        m_f_webfile = false;
        m_f_webstream = false;
        m_f_running = false;
        if(!m_f_ssl)client.stop();
        else clientsecure.stop();
        // error 404 occured, nothing to do, throw away all bytes
        return;
    }
    static char metaline[512];
    static uint16_t pos_ml = 0;                                 // determines the current position in metaline

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == AUDIO_HEADER) {                            // Handle next byte of MP3 header
        if(b == '\n') {                                         // Linefeed ?
            m_LFcount++;                                        // Count linefeeds
            metaline[pos_ml] = 0;
            parseAudioHeader(metaline);
            pos_ml = 0; metaline[pos_ml] = 0;                   // Reset this line
            if((m_LFcount == 2) && m_f_ctseen) {                // Some data seen and a double LF?
                if(m_bitRate == 0) {
                    if(audio_bitrate) audio_bitrate("");
                } // no bitrate received
                if(m_f_swm == true) { // stream without metadata
                    m_datamode = AUDIO_SWM;
                    sprintf(chbuf, "Switch to SWM, bitrate is %d, metaint is %d", m_bitRate, m_metaint);
                    if(audio_info) audio_info(chbuf);
                    memcpy(chbuf, m_lastHost, strlen(m_lastHost)+1);
                    uint idx = indexOf(chbuf, "?", 0);
                    if(idx > 0) chbuf[idx] = 0;
                    if(audio_lasthost) audio_lasthost(chbuf);
                    m_f_swm = false;
                }
                else {
                    m_datamode = AUDIO_DATA;                      // Expecting data now
                    sprintf(chbuf, "Switch to DATA, metaint is %d", m_metaint);
                    if(audio_info) audio_info(chbuf);
                    memcpy(chbuf, m_lastHost, strlen(m_lastHost)+1);
                    uint idx = indexOf(chbuf, "?", 0);
                    if(idx > 0) chbuf[idx] = 0;
                    if(audio_lasthost) audio_lasthost(chbuf);
                }
                delay(50);  // #77
            }
        }
        else {
            if((b < 0x80) && (b != '\r') && (b != '\0')) {      // Ignore unprintable characters, ignore CR, ignore NULL
                m_LFcount = 0;                                  // Reset double CRLF detection
                metaline[pos_ml] = (char) b;
                if(pos_ml < 510) pos_ml ++;
                metaline[511] = 0;                              // for safety
                if(pos_ml == 509) log_i("metaline overflow in AUDIO_HEADER! metaline=%s", metaline);
                if(pos_ml == 510) { ; /* last current char in b */}
            }
        }
        return;
    } // end AUDIO_HEADER

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == AUDIO_METADATA) {                            // Handle next byte of metadata
        if(m_f_firstmetabyte) {                                   // First byte of metadata?
            m_f_firstmetabyte = false;                            // Not the first anymore
            m_metalen = b * 16 + 1;                               // New count for metadata including length byte
            if(m_metalen >512){
                if(audio_info) audio_info("Metadata block to long! Skipping all Metadata from now on.");
                m_metaint = 16000;                                // Probably no metadata
                m_datamode = AUDIO_SWM;                           // expect stream without metadata
            }
            pos_ml = 0; metaline[pos_ml] = 0;                     // Prepare for new line
        }
        else {
            if(m_datamode != AUDIO_SWM) {
                metaline[pos_ml] = (char) b;                          // Put new char in metaline
                if(pos_ml < 510) pos_ml ++;
                metaline[pos_ml] = 0;
                if(pos_ml == 509) log_i("metaline overflow in AUDIO_METADATA! metaline=%s", metaline) ;
                if(pos_ml == 510) { ; /* last current char in b */}

            }
        }
        if(--m_metalen == 0) {
            if(m_datamode != AUDIO_SWM) m_datamode = AUDIO_DATA;                          // Expecting data
            if(strlen(metaline)) {                             // Any info present?
                // metaline contains artist and song name.  For example:
                // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
                // Sometimes it is just other info like:
                // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
                // Isolate the StreamTitle, remove leading and trailing quotes if present.
                //log_i("ST %s", m_metaline.c_str());

                int pos = indexOf(metaline, "song_spot", 0);    // remove some irrelevant infos
                if(pos > 3) {                                   // e.g. song_spot="T" MediaBaseId="0" itunesTrackId="0"
                    metaline[pos] = 0;
                }
                if(!m_f_localfile) showstreamtitle(metaline);   // Show artist and title if present in metadata
            }
        }
        return;
    } // end AUDIO_METADATA

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == AUDIO_PLAYLISTINIT) {                        // Initialize for receive .m3u file
        // We are going to use metadata to read the lines from the .m3u file
        // Sometimes this will only contain a single line

        pos_ml = 0; metaline[pos_ml] = 0;                           // Prepare for new line
        m_LFcount = 0;                                              // For detection end of header
        m_datamode = AUDIO_PLAYLISTHEADER;                          // Handle playlist data
        m_totalcount = 0;                                           // Reset totalcount
        parsePlaylistData("clear_by_playlistinit");
        if(audio_info) audio_info("Read from playlist");
    } // end AUDIO_PLAYLISTINIT

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == AUDIO_PLAYLISTHEADER) {                        // Read header
        if(b == '\n') {                                             // Linefeed ?
            m_LFcount++;                                            // Count linefeeds
            metaline[pos_ml] = 0;
            if(isascii(metaline[0]) && metaline[0] > 0x19){
                sprintf(chbuf, "Playlistheader: %s", metaline);     // Show playlistheader
                if(audio_info) audio_info(chbuf);
            }

            char lc_ml[512];                                        // save temporarily
            memcpy(lc_ml, metaline, 511);
            lc_ml[511] = 0;                                         // for safety

            int pos = indexOf(lc_ml, "404 Not Found", 0);
            if(pos >= 0) {
                m_datamode = AUDIO_NONE;
                if(audio_info) audio_info("Error 404 Not Found");
                return;
            }

            pos = indexOf(lc_ml, "404 File Not Found", 0);
            if(pos >= 0) {
                m_datamode = AUDIO_NONE;
                if(audio_info) audio_info("Error 404 File Not Found");
                return;
            }

            pos = indexOf(lc_ml, ":", 0);                       // lowercase all letters up to the colon
            if(pos >= 0) {
                for(int i=0; i<pos; i++) {
                    lc_ml[i] = toLowerCase(lc_ml[i]);
                }
            }

            if(startsWith(lc_ml, "location:")) {
                const char* host;
                pos = indexOf(lc_ml, "http", 0);
                host = (lc_ml + pos);
                sprintf(chbuf, "redirect to new host %s", host);
                if(audio_info) audio_info(chbuf);
                connecttohost(host);
            }
            pos_ml = 0; metaline[pos_ml] = 0;                   // Reset this line
            if(m_LFcount == 2) {
                if(audio_info) audio_info("Switch to PLAYLISTDATA");
                m_datamode = AUDIO_PLAYLISTDATA;                // Expecting data now
                m_t0 = millis();
                return;
            }
        }
        else {
            if((b < 0x80) && (b != '\r') && (b != '\0')) {      // Ignore unprintable characters, ignore CR, ignore NULL
                m_LFcount = 0;                                  // Reset double CRLF detection
                metaline[pos_ml] = (char) b;
                if(pos_ml < 510) pos_ml ++;
                metaline[511] = 0;                              // for safety
                if(pos_ml == 509) log_i("metaline overflow in AUDIO_PLAYLISTHEADER! metaline=%s", metaline);
                if(pos_ml == 510) { ; /* last current char in b */}
            }
        }
        return;
    } // end AUDIO_PLAYLISTHEADER

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == AUDIO_PLAYLISTDATA) {                      // Read next byte of .m3u file data
        m_t0 = millis();
        if(b == '\n') {                                         // Linefeed or end of string?
            metaline[pos_ml] = 0;
            if(isascii(metaline[0]) && metaline[0] > 0x19){
                sprintf(chbuf, "Playlistdata: %s", metaline);       // Show playlistdata
                if(audio_info) audio_info(chbuf);
            }
            parsePlaylistData(metaline);
            pos_ml = 0; metaline[pos_ml] = 0;                   // Reset this line
        }

        else {
            if((b < 0x80) && (b != '\r') && (b != '\0')) {      // Ignore unprintable characters, ignore CR, ignore NULL
                metaline[pos_ml] = (char) b;                    // Normal character, add it to metaline
                if(pos_ml < 510) pos_ml ++;
                metaline[511] = 0;                              // for safety
                if(pos_ml == 509) log_i("metaline overflow in AUDIO_PLAYLISTHEADER! metaline=%s", metaline);
                if(pos_ml == 510) { ; /* last current char in b */}
            }
        }
        return;
    } // end AUDIO_PLAYLISTDATA
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::parsePlaylistData(const char* pd){
    static bool f_entry = false;                            // entryflag for asx playlist
    static bool f_title = false;                            // titleflag for asx playlist
    static bool f_ref   = false;                            // refflag   for asx playlist
    int pos = 0;                                            // index (position in a string)
    char pd_buff[512];

    if(startsWith(pd, "clear_by_playlistinit")){
        f_entry = false;
        f_title = false;
        f_ref   = false;
        m_plsURL[0] = 0;
        //log_i("clear statics in playlistdata");
        return;
    }

    memcpy(pd_buff, pd, 512);

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_playlistFormat == FORMAT_M3U) {

        if(strlen(pd_buff) < 5) {                            // Skip short lines
            return;
        }
        if(indexOf(pd_buff, "#EXTINF:", 0) >= 0) {           // Info?
            pos = indexOf(pd_buff, ",", 0);                  // Comma in this line?
            if(pos > 0) {
                // Show artist and title if present in metadata
                if(audio_info) audio_info(pd_buff + pos + 1);
            }
        }
        if(startsWith(pd_buff, "#")) {                       // Commentline?
            return;
        }

        pos = indexOf(pd_buff, "http://:@", 0); // ":@"??  remove that!
        if(pos >= 0) {
            sprintf(chbuf, "Entry in playlist found: %s", (pd_buff + pos + 9));
            connecttohost(pd_buff + pos + 9);
            return;
        }
        sprintf(chbuf, "Entry in playlist found: %s", pd_buff);
        if(audio_info) audio_info(chbuf);
        pos = indexOf(pd_buff, "http", 0);                  // Search for "http"
        const char* host;
        if(pos >= 0) {                                      // Does URL contain "http://"?
            host = (pd_buff + pos);
            connecttohost(host);
        }                                                   // Yes, set new host
        return;
    } //m3u

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_playlistFormat == FORMAT_PLS) {
        if(startsWith(pd_buff, "File1")) {
            pos = indexOf(pd_buff, "http", 0);                          // File1=http://streamplus30.leonex.de:14840/;
            if(pos >= 0) {                                              // yes, URL contains "http"?
                memcpy(m_plsURL, pd_buff + pos, strlen(pd_buff) + 1);   // http://streamplus30.leonex.de:14840/;
                // Now we have an URL for a stream in host.
                f_ref = true;
            }
        }
        if(startsWith(pd_buff, "Title1")) {                             // Title1=Antenne Tirol
            const char* plsStationName = (pd_buff + 7);
            if(audio_showstation) audio_showstation(plsStationName);
            sprintf(chbuf, "StationName=\"%s\"", plsStationName);
            if(audio_info) audio_info(chbuf);
            f_title = true;
        }
        if(startsWith(pd_buff, "Length1")) f_title = true;              // if no Title is available
        if((f_ref == true) && (strlen(pd_buff) == 0)) f_title = true;

        if(f_ref && f_title) {                                          // we have both StationName and StationURL
            connecttohost(m_plsURL);                                    // Connect to it
        }
    } // pls

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_playlistFormat == FORMAT_ASX) { // Advanced Stream Redirector
        if(indexOf(pd_buff, "<entry>", 0) >= 0) f_entry = true; // found entry tag (returns -1 if not found)
        if(f_entry) {
            if(indexOf(pd_buff, "ref href", 0) > 0) {           // <ref href="http://87.98.217.63:24112/stream" />
                pos = indexOf(pd_buff, "http", 0);
                if(pos > 0) {
                    char* plsURL = (pd_buff + pos);             // http://87.98.217.63:24112/stream" />
                    int pos1 = indexOf(plsURL, "\"", 0);        // http://87.98.217.63:24112/stream
                    if(pos1 > 0) {
                        plsURL[pos1] = 0;
                    }
                    memcpy(m_plsURL, plsURL, strlen(plsURL));   // save url in array
                    log_i("m_plsURL=%s",m_plsURL);
                    // Now we have an URL for a stream in host.
                    //log_i("m_plsURL= %s", m_plsURL.c_str());
                    f_ref = true;
                }
            }
            pos = indexOf(pd_buff, "<title>", 0);
            if(pos < 0) pos = indexOf(pd_buff, "<Title>", 0);
            if(pos >= 0) {
                char* plsStationName = (pd_buff + pos + 7);     // remove <Title>
                pos = indexOf(plsStationName, "</", 0);
                if(pos >= 0){
                        *(plsStationName +pos) = 0;             // remove </Title>
                }
                if(audio_showstation) audio_showstation(plsStationName);
                sprintf(chbuf, "StationName=\"%s\"", plsStationName);
                if(audio_info) audio_info(chbuf);
                f_title = true;
            }
        } //entry
        if(f_ref && f_title) {   //we have both StationName and StationURL
            connecttohost(m_plsURL);                      // Connect to it
        }
    }  //asx
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::parseAudioHeader(const char* ah) {
    char ah_buff[512];
    memcpy(ah_buff, ah, 512);
    int pos = indexOf(ah_buff, ":", 0); // lowercase all letters up to the colon
    if(pos >= 0) {
        for(int i=0; i<pos; i++) {
            ah_buff[i] = toLowerCase(ah_buff[i]);
        }
    }

    if(indexOf(ah_buff, "content-type:", 0) >= 0) {
        if(parseContentType(ah_buff)) m_f_ctseen = true;
    }
    else if(startsWith(ah_buff, "location:")) {
        int pos = indexOf(ah_buff, "http", 0);
        const char* c_host = (ah_buff + pos);
        sprintf(chbuf, "redirect to new host \"%s\"", c_host);
        if(audio_info) audio_info(chbuf);
        connecttohost(c_host);
    }
    else if(startsWith(ah_buff, "set-cookie:")    ||
            startsWith(ah_buff, "pragma:")        ||
            startsWith(ah_buff, "expires:")       ||
            startsWith(ah_buff, "cache-control:") ||
            startsWith(ah_buff, "icy-pub:")       ||
            startsWith(ah_buff, "accept-ranges:") ){
        ; // do nothing
    }
    else if(startsWith(ah_buff, "connection:")) {
        if(indexOf(ah_buff, "close", 0) >= 0) {; /* do nothing */}
    }
    else if(startsWith(ah_buff, "icy-genre:")) {
        ; // do nothing Ambient, Rock, etc
    }
    else if(startsWith(ah_buff, "icy-br:")) {
        const char* c_bitRate = (ah_buff + 7);
        int32_t br = atoi(c_bitRate); // Found bitrate tag, read the bitrate in Kbit
        br = br * 1000;
        m_bitRate = br;
        sprintf(chbuf, "%d", m_bitRate);
        if(audio_bitrate) audio_bitrate(chbuf);
    }
    else if(startsWith(ah_buff, "icy-metaint:")) {
        const char* c_metaint = (ah_buff + 12);
        int32_t i_metaint = atoi(c_metaint);
        m_metaint = i_metaint;
        if(m_metaint > 0) m_f_swm = false;            // Multimediastream
    }
    else if(startsWith(ah_buff, "icy-name:")) {
        char* c_icyname = (ah_buff + 9); // Get station name
        pos = 0;
        while(c_icyname[pos] == ' '){pos++;} c_icyname += pos;      // Remove leading spaces
        pos = strlen(c_icyname);
        while(c_icyname[pos] == ' '){pos--;} c_icyname[pos+1] = 0;  // Remove trailing spaces

        if(strlen(c_icyname) > 0) {
            sprintf(chbuf, "icy-name: %s", c_icyname);
            if(audio_info) audio_info(chbuf);
            if(audio_showstation) audio_showstation(c_icyname);
        }
    }
    else if(startsWith(ah_buff, "content-length:")) {
        const char* c_cl = (ah_buff + 15);
        int32_t i_cl = atoi(c_cl);
        m_contentlength = i_cl;
        m_f_webfile = true; // Stream comes from a fileserver
        sprintf(chbuf, "Content-Length: %i", m_contentlength);
        if(audio_info) audio_info(chbuf);
    }
    else if((startsWith(ah_buff, "transfer-encoding:"))){
        if(endsWith(ah_buff, "chunked") || endsWith(ah_buff, "Chunked") ) { // Station provides chunked transfer
            m_f_chunked = true;
            if(audio_info) audio_info("chunked data transfer");
            m_chunkcount = 0;                         // Expect chunkcount in DATA
        }
    }
    else if(startsWith(ah_buff, "icy-url:")) {
        const char* icyurl = (ah_buff + 8);
        pos = 0;
        while(icyurl[pos] == ' ') {pos ++;} icyurl += pos; // remove leading blanks
        sprintf(chbuf, "icy-url: %s", icyurl);
        if(audio_info) audio_info("icyurl");
        if(audio_icyurl) audio_icyurl(icyurl);
    }
    else if(startsWith(ah_buff, "www-authenticate:")) {
        if(audio_info) audio_info("authentification failed, wrong credentials?");
        m_f_running = false;
        stopSong();
    }
    else {
        if(isascii(ah_buff[0]) && ah_buff[0] >= 0x20) {  // all other
            sprintf(chbuf, "%s", ah_buff);
            if(audio_info) audio_info(chbuf);
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::parseContentType(const char* ct) {
    bool ct_seen = false;
    if(indexOf(ct, "audio", 0) >= 0) {             // Is ct audio?
        ct_seen = true;                       // Yes, remember seeing this
//        if(audio_info) audio_info(chbuf);
        if(indexOf(ct, "mpeg", 13) >= 0) {
            m_codec = CODEC_MP3;
            sprintf(chbuf, "%s, format is mp3", ct);
            if(audio_info) audio_info(chbuf); //ok is likely mp3
            MP3Decoder_AllocateBuffers();
            sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            if(audio_info) audio_info(chbuf);
        }
        else if(indexOf(ct, "mp3", 13) >= 0) {
            m_codec = CODEC_MP3;
            sprintf(chbuf, "%s, format is mp3", ct);
            if(audio_info) audio_info(chbuf);
            MP3Decoder_AllocateBuffers();
            sprintf(chbuf, "MP3Decoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            if(audio_info) audio_info(chbuf);
        }
        else if(indexOf(ct, "aac", 13) >= 0) {
            m_codec = CODEC_AAC;
            sprintf(chbuf, "%s, format is aac", ct);
            if(audio_info) audio_info(chbuf);
            AACDecoder_AllocateBuffers();
            sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            if(audio_info) audio_info(chbuf);
        }
        else if(indexOf(ct, "mp4", 13) >= 0) {
            m_codec = CODEC_AAC;
            sprintf(chbuf, "%s, format is aac", ct);
            if(audio_info) audio_info(chbuf);
            AACDecoder_AllocateBuffers();
            sprintf(chbuf, "AACDecoder has been initialized, free Heap: %u bytes", ESP.getFreeHeap());
            if(audio_info) audio_info(chbuf);
        }
        else if(indexOf(ct, "wav", 13) >= 0) {        // audio/x-wav
            m_codec = CODEC_WAV;
            sprintf(chbuf, "%s, format is wav", ct);
            if(audio_info) audio_info(chbuf);
        }
        else if(indexOf(ct, "ogg", 13) >= 0) {
            m_f_running = false;
            sprintf(chbuf, "%s, can't play ogg", ct);
            if(audio_info) audio_info(chbuf);
            stopSong();
        }
        else {
            m_f_running = false;
            sprintf(chbuf, "%s, unsupported audio format", ct);
            if(audio_info) audio_info(chbuf);
        }
    }
    return ct_seen;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showstreamtitle(const char* ml) {
    // example for ml:
    // StreamTitle='Oliver Frank - Mega Hitmix';StreamUrl='www.radio-welle-woerthersee.at';
    // or adw_ad='true';durationMilliseconds='10135';adId='34254';insertionType='preroll';
    int16_t pos1 = 0, pos2 = 0, pos3 = 0, pos4 = 0;
    String mline = ml, st = "", su = "", ad = "", artist = "", title = "", icyurl = "";
    pos1 = mline.indexOf("StreamTitle=");
    if(pos1 != -1) {                                          // StreamTitle found
        pos1 = pos1 + 12;
        st = mline.substring(pos1);                           // remove "StreamTitle="
        if(st.startsWith("'{")) {
            // special coding like '{"t":"\u041f\u0438\u043a\u043d\u0438\u043a - \u0418...."m":"mdb","lAU":0,"lAuU":18}
            pos2 = st.indexOf('"', 8);                        // end of '{"t":".......", seek for double quote at pos 8
            st = st.substring(0, pos2);
            pos2 = st.lastIndexOf('"');
            st = st.substring(pos2 + 1);                      // remove '{"t":"
            pos2 = 0;
            String uni = "";
            String st1 = "";
            uint16_t u = 0;
            uint8_t v = 0, w = 0;
            for(int i = 0; i < st.length(); i++) {
                if(pos2 > 1) pos2++;
                if((st[i] == '\\') && (pos2 == 0)) pos2 = 1;  // backslash found
                if((st[i] == 'u') && (pos2 == 1)) pos2 = 2;   // "\u" found
                if(pos2 > 2) uni = uni + st[i];               // next 4 values are unicode
                if(pos2 == 0) st1 += st[i];                   // normal character
                if(pos2 > 5) {
                    pos2 = 0;
                    u = strtol(uni.c_str(), 0, 16);           // convert hex to int
                    v = u / 64 + 0xC0;
                    st1 += char(v);                           // compute UTF-8
                    w = u % 64 + 0x80;
                    st1 += char(w);
                    uni = "";
                }
            }
            log_i("st1 %s", st1.c_str());
            st = st1;
        }
        else {
            // normal coding
            if(st.indexOf('&') != -1) {                       // maybe html coded
                st.replace("&Auml;", "Ä");
                st.replace("&auml;", "ä");                    // HTML -> ASCII
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
            pos2 = st.indexOf(';', 1);                        // end of StreamTitle, first occurrence of ';'
            if(pos2 != -1) st = st.substring(0, pos2);        // extract StreamTitle
            if(st.startsWith("'")) st = st.substring(1, st.length() - 1); // if exists remove ' at the begin and end
            pos3 = st.lastIndexOf(" - ");                     // separator artist - title
            if(pos3 != -1) {                                  // found separator? yes
                artist = st.substring(0, pos3);               // artist not used yet
                title = st.substring(pos3 + 3);               // title not used yet
            }
        }

        uint16_t i = 0;
        uint16_t hash = 0;
        while(i < st.length()){
            hash += st[i] * i+1;
            i++;
        }

        if(m_st_remember != hash) {                             // show only changes
            if(audio_showstreamtitle) audio_showstreamtitle(st.c_str());
            st = "StreamTitle=\"" + st + '\"';
            if(audio_info) audio_info(st.c_str());
            m_st_remember = hash;
        }
    }
    pos4 = mline.indexOf("StreamUrl=");
    if(pos4 != -1) {                                          // StreamUrl found
        pos4 = pos4 + 10;
        su = mline.substring(pos4);                           // remove "StreamUrl="
        pos2 = su.indexOf(';', 1);                            // end of StreamUrl, first occurrence of ';'
        if(pos2 != -1) su = su.substring(0, pos2);            // extract StreamUrl
        if(su.startsWith("'")) su = su.substring(1, su.length() - 1); // if exists remove ' at the begin and end
        su = "StreamUrl=\"" + su + '\"';
        if(audio_info) audio_info(su.c_str());
    }
    pos2 = mline.indexOf("adw_ad=");                          // advertising,
    if(pos2 != -1) {
        ad = mline.substring(pos2);
        if(audio_info) audio_info(ad.c_str());
        pos2 = mline.indexOf("durationMilliseconds=");
        if(pos2 != -1) {
            pos2 += 22;
            mline = mline.substring(pos2);
            mline = mline.substring(0, mline.indexOf("'") - 3); // extract duration in seconds
            if(audio_commercial) audio_commercial(mline.c_str());
        }
    }
    if(pos1 == -1 && pos4 == -1) {
        // Info probably from playlist
        st = mline;
        if(audio_showstreamtitle) audio_showstreamtitle(st.c_str());
        st = "Streamtitle=\"" + st + '\"';
        if(audio_info) audio_info(st.c_str());
    }
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::sendBytes(uint8_t* data, size_t len) {

    static uint32_t lastRet = 0, count = 0, swnf = 0;
    static uint32_t lastSampleRate = 0, lastChannels = 0, lastBitsPerSeconds = 0, lastBitRate = 0;
    int nextSync = 0;
    if(!m_f_playing) {
        lastSampleRate = 0;
        lastChannels = 0;
        lastBitsPerSeconds = 0;
        lastBitRate = 0;
        if(m_codec == CODEC_WAV){m_f_playing = true; return 0;}
        if(m_codec == CODEC_MP3) nextSync = MP3FindSyncWord(data, len);
        if(m_codec == CODEC_AAC) nextSync = AACFindSyncWord(data, len);

        if(nextSync == -1) {
            if(audio_info && swnf<1) audio_info("syncword not found");
            swnf++; // syncword not found counter, can be multimediadata
            return -1;
        }
        else if (nextSync == 0){
            if(audio_info && swnf>0){
                sprintf(chbuf, "syncword not found %i times", swnf);
                audio_info(chbuf);
                swnf = 0;
            }
            else {
                if(audio_info) audio_info("syncword found at pos 0");
                count = 0;
                m_f_playing = true;
                return 0;
            }
        }
        else if(nextSync > 0){
            sprintf(chbuf, "syncword found at pos %i", nextSync);
            if(audio_info) audio_info(chbuf);
            return nextSync;
        }
        else {
            if(audio_info) audio_info("mp3 decoder syncword not found, unknown reason");
            return -1;
        }
    }

    m_bytesLeft = len;
    int ret = 0;
    int bytesDecoded = 0;
    if(m_codec == CODEC_WAV){ //copy len data in outbuff and set validsamples and bytesdecoded=len
        memmove(m_outBuff, data , len);
        if(getBitsPerSample() == 16) m_validSamples = len / (2 * getChannels());
        if(getBitsPerSample() == 8 ) m_validSamples = len / 2;
        m_bytesLeft = 0;
    }
    if(m_codec == CODEC_MP3) ret = MP3Decode(data, &m_bytesLeft, m_outBuff, 0);
    if(m_codec == CODEC_AAC) ret = AACDecode(data, &m_bytesLeft, m_outBuff);
    if(ret == 0) lastRet = 0;
    bytesDecoded = len-m_bytesLeft;
    // log_i("bytesDecoded %i", bytesDecoded);
    if(bytesDecoded == 0){ // unlikely framesize
        if(audio_info) audio_info("framesize is 0, start decoding again");
        m_f_playing = false; // seek for new syncword
        
        // we're here because there was a wrong sync word
        // so skip two sync bytes and seek for next
        return 2; 
    }
    if(ret) { // Error, skip the frame...
        m_f_playing = false; // seek for new syncword
        if(count == 0) {
            i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
            if(!lastChannels && (ret == -2)) {
                 ; // suppress errorcode MAINDATA_UNDERFLOW
            }
            else {
                printDecodeError(ret);
            }
        }
        if(lastRet == ret) count++;
        if(count > 10) {count = 0; lastRet = 0;} // more than 10 errors
        if(ret != lastRet) {
            lastRet = ret; count = 0;
        }
        return bytesDecoded;
    }
    else{  // ret==0
        if(m_codec == CODEC_MP3){
            if (MP3GetChannels() != lastChannels) {
                setChannels(MP3GetChannels());
                lastChannels = MP3GetChannels() ;
                sprintf(chbuf,"Channels=%i", MP3GetChannels() );
                if(audio_info) audio_info(chbuf);
            }
            if (MP3GetSampRate() != lastSampleRate) {
                setSampleRate(MP3GetSampRate());
                lastSampleRate = MP3GetSampRate();
                sprintf(chbuf,"SampleRate=%i",MP3GetSampRate());
                if(audio_info) audio_info(chbuf);
            }
            if((int) MP3GetSampRate() ==0){ // error can't be
                if(audio_info) audio_info("SampleRate=0, try new frame");
                m_f_playing = false;
                return 1;
            }
            if(MP3GetBitsPerSample() != lastBitsPerSeconds){
                lastBitsPerSeconds = MP3GetBitsPerSample();
                setBitsPerSample(MP3GetBitsPerSample());
                sprintf(chbuf,"BitsPerSample=%i", MP3GetBitsPerSample());
                if(audio_info) audio_info(chbuf);
            }
            if(MP3GetBitrate() != lastBitRate){
                if(lastBitRate == 0){
                    sprintf(chbuf,"BitRate=%i", MP3GetBitrate());
                    if(audio_info) audio_info(chbuf);
                }
                lastBitRate = MP3GetBitrate();
                m_bitRate=MP3GetBitrate();
            }
            m_validSamples = MP3GetOutputSamps() / lastChannels;
        }
        if(m_codec == CODEC_AAC){
            if (AACGetChannels() != lastChannels) {
                setChannels(AACGetChannels());
                lastChannels = AACGetChannels();
                sprintf(chbuf,"AAC Channels=%i",AACGetChannels());
                if(audio_info) audio_info(chbuf);
            }
            if (AACGetSampRate() != lastSampleRate) {
                setSampleRate(AACGetSampRate());
                lastSampleRate = AACGetSampRate();
                sprintf(chbuf,"AAC SampleRate=%i",AACGetSampRate() * getChannels());
                if(audio_info) audio_info(chbuf);
            }
            if(AACGetSampRate() == 0){ // error can't be
                if(audio_info) audio_info("AAC SampleRate=0, try new frame");
                m_f_playing = false;
                return 1;
            }
            if (AACGetBitsPerSample() != lastBitsPerSeconds){
                lastBitsPerSeconds = AACGetBitsPerSample();
                setBitsPerSample(AACGetBitsPerSample());
                sprintf(chbuf,"AAC BitsPerSample=%i", AACGetBitsPerSample());
                if(audio_info) audio_info(chbuf);
            }
//            (AACGetBitrate() is always 0
//            if (AACGetBitrate() != m_bitRate){
//                    sprintf(chbuf,"AAC Bitrate=%i", AACGetBitrate()); // show only the first rate
//                    if(audio_info) audio_info(chbuf);
//                m_bitRate=AACGetBitrate();
//            }
//            Workaround:

            if(m_bitRate != lastBitRate){ // m_bitRate from icy stream
                    sprintf(chbuf,"AAC Bitrate=%u", m_bitRate);
                    if(audio_info) audio_info(chbuf);
                    lastBitRate = m_bitRate;
            }
            m_validSamples = AACGetOutputSamps() / lastChannels;
        }
    }
    compute_audioCurrentTime(bytesDecoded);
    while(m_validSamples) {
        playChunk();
    }
    return bytesDecoded;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::compute_audioCurrentTime(int bd) {
    static uint16_t bitrate_counter = 0;
    static int old_bitrate = 0;
    static uint32_t sum_bitrate = 0;
    static boolean f_firstFrame = true;
    static boolean f_CBR = true; // constant bitrate

    if(m_bitRate > 0) {
        if(m_avr_bitrate == 0) {
            bitrate_counter = 0;
            old_bitrate = 0;
            sum_bitrate = 0;
            f_firstFrame = true;
            f_CBR = true;
        }
        if(old_bitrate != m_bitRate) {
            if(!f_firstFrame && bitrate_counter == 0) {
                if(audio_info) audio_info("VBR recognized, audioFileDuration is estimated");
                f_CBR = false; // variable bitrate
            }
        }
        old_bitrate = m_bitRate;
        if(bitrate_counter < 100 && !f_CBR) { // if VBR: m_avr_bitrate is average of the first 100 values of m_bitrate
            bitrate_counter++;
            sum_bitrate += m_bitRate;
            m_avr_bitrate = sum_bitrate / bitrate_counter;
        }
        if(f_firstFrame) {
            m_avr_bitrate = m_bitRate; // if CBR set m_avr_bitrate equal to m_bitrate
        }
        f_firstFrame = false;
        m_audioCurrentTime += (float) bd * 8 / m_bitRate;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::printDecodeError(int r) {
    String e = "";
    if(m_codec == CODEC_MP3){
        switch(r){
            case ERR_MP3_NONE:                 e = "NONE";                  break;
            case ERR_MP3_INDATA_UNDERFLOW:     e = "INDATA_UNDERFLOW";      break;
            case ERR_MP3_MAINDATA_UNDERFLOW:   e = "MAINDATA_UNDERFLOW";    break;
            case ERR_MP3_FREE_BITRATE_SYNC:    e = "FREE_BITRATE_SYNC";     break;
            case ERR_MP3_OUT_OF_MEMORY:        e = "OUT_OF_MEMORY";         break;
            case ERR_MP3_NULL_POINTER:         e = "NULL_POINTER";          break;
            case ERR_MP3_INVALID_FRAMEHEADER:  e = "INVALID_FRAMEHEADER";   break;
            case ERR_MP3_INVALID_SIDEINFO:     e = "INVALID_SIDEINFO";      break;
            case ERR_MP3_INVALID_SCALEFACT:    e = "INVALID_SCALEFACT";     break;
            case ERR_MP3_INVALID_HUFFCODES:    e = "INVALID_HUFFCODES";     break;
            case ERR_MP3_INVALID_DEQUANTIZE:   e = "INVALID_DEQUANTIZE";    break;
            case ERR_MP3_INVALID_IMDCT:        e = "INVALID_IMDCT";         break;
            case ERR_MP3_INVALID_SUBBAND:      e = "INVALID_SUBBAND";       break;
            default: e = "ERR_UNKNOWN";
        }
        sprintf(chbuf, "MP3 decode error %d : %s", r, e.c_str());
        if(audio_info) audio_info(chbuf);
    }
    if(m_codec == CODEC_AAC){
        switch(r){
            case ERR_AAC_NONE:                  e = "NONE";                 break;
            case ERR_AAC_INDATA_UNDERFLOW:      e = "INDATA_UNDERFLOW";     break;
            case ERR_AAC_NULL_POINTER:          e = "NULL_POINTER";         break;
            case ERR_AAC_INVALID_ADTS_HEADER:   e = "INVALID_ADTS_HEADER";  break;
            case ERR_AAC_INVALID_ADIF_HEADER:   e = "INVALID_ADIF_HEADER";  break;
            case ERR_AAC_INVALID_FRAME:         e = "INVALID_FRAME";        break;
            case ERR_AAC_MPEG4_UNSUPPORTED:     e = "MPEG4_UNSUPPORTED";    break;
            case ERR_AAC_CHANNEL_MAP:           e = "CHANNEL_MAP";          break;
            case ERR_AAC_SYNTAX_ELEMENT:        e = "SYNTAX_ELEMENT";       break;
            case ERR_AAC_DEQUANT:               e = "DEQUANT";              break;
            case ERR_AAC_STEREO_PROCESS:        e = "STEREO_PROCESS";       break;
            case ERR_AAC_PNS:                   e = "PNS";                  break;
            case ERR_AAC_SHORT_BLOCK_DEINT:     e = "SHORT_BLOCK_DEINT";    break;
            case ERR_AAC_TNS:                   e = "TNS";                  break;
            case ERR_AAC_IMDCT:                 e = "IMDCT";                break;
            case ERR_AAC_NCHANS_TOO_HIGH:       e = "NCHANS_TOO_HIGH";      break;
            case ERR_AAC_RAWBLOCK_PARAMS:       e = "RAWBLOCK_PARAMS";      break;
            default: e = "ERR_UNKNOWN";
        }
        sprintf(chbuf, "AAC decode error %d : %s", r, e.c_str());
        if(audio_info) audio_info(chbuf);
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t DIN) {
    m_BCLK = BCLK;            // Bit Clock
    m_LRC = LRC;              // Left/Right Clock
    m_DOUT = DOUT;            // Data Out
    m_DIN = DIN;              // Data In

    i2s_pin_config_t pins = {
            .bck_io_num   = m_BCLK,
            .ws_io_num    = m_LRC,     //  wclk,
            .data_out_num = m_DOUT,
            .data_in_num  = m_DIN
    };
    const esp_err_t result = i2s_set_pin((i2s_port_t) m_i2s_num, &pins);
    return (result == ESP_OK);
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFileSize() {
    if(!audiofile) return 0;
    return audiofile.size();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFilePos() {
    if(!audiofile) return 0;
    return audiofile.position();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioFileDuration() {
    if(m_f_localfile) {
        if(!audiofile) return 0;

        if(0 == m_audioFileDuration) { // calculate only once per file
            uint32_t fileSize = getFileSize();
            uint32_t bitRate = m_avr_bitrate;
            if(0 < bitRate) {
                m_audioFileDuration = 8 * (fileSize - m_id3Size) / bitRate;
            }
        }
        return m_audioFileDuration;
    }
    if(m_f_webfile) {
        if(m_contentlength > 0 && m_avr_bitrate > 0) {
            return ((m_contentlength - m_bytesNotDecoded) * 8 / m_avr_bitrate);
        }
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioCurrentTime() {  // return current time in seconds
    return (uint32_t) m_audioCurrentTime;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setFilePos(uint32_t pos) {
    if(!audiofile) return false;
    if((m_codec == CODEC_MP3) && (pos < m_id3Size)) pos = m_id3Size; // issue #96
    if((m_codec == CODEC_MP3) && m_avr_bitrate) m_audioCurrentTime = (pos-m_id3Size) * 8 / m_avr_bitrate; // #96
    return audiofile.seek(pos);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::audioFileSeek(const int8_t speed) {
    bool retVal = false;
    if(audiofile && speed) {
        retVal = true; //    
        int32_t steps = 20 * speed;

        if((steps < 0) && m_f_running) {
            // fast-rewind faster than fast-forward because we still playing
            steps = steps * 2;
        }
        uint32_t newPos = audiofile.position() + steps * MP3GetBitsPerSample();
        newPos = (newPos < m_id3Size) ? m_id3Size : newPos;
        newPos = (newPos >= audiofile.size()) ? audiofile.size() - 1 : newPos;
        if(audiofile.position() != newPos) {
            retVal = audiofile.seek(newPos);
        }
    }
    return retVal;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setSampleRate(uint32_t sampRate) {
    i2s_set_sample_rates((i2s_port_t)m_i2s_num, sampRate);
    m_sampleRate = sampRate;
    IIR_calculateCoefficients(); // must be recalculated after each samplerate change
    return true;
}
uint32_t Audio::getSampleRate(){
    return m_sampleRate;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setBitsPerSample(int bits) {
    if((bits != 16) && (bits != 8)) return false;
    m_bitsPerSample = bits;
    return true;
}
uint8_t Audio::getBitsPerSample(){
    return m_bitsPerSample;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setChannels(int ch) {
    if((ch < 1) || (ch > 2)) return false;
    m_channels = ch;
    return true;
}
uint8_t Audio::getChannels(){
    return m_channels;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playSample(int16_t sample[2]) {
    if (getBitsPerSample() == 8) { // Upsample from unsigned 8 bits to signed 16 bits
        sample[LEFTCHANNEL]  = ((sample[LEFTCHANNEL]  & 0xff) -128) << 8;
        sample[RIGHTCHANNEL] = ((sample[RIGHTCHANNEL] & 0xff) -128) << 8;
    }
    sample = IIR_filterChain(sample);

    uint32_t s32 = Gain(sample); // volume;

    esp_err_t err = i2s_write((i2s_port_t) m_i2s_num, (const char*) &s32, sizeof(uint32_t), &m_i2s_bytesWritten, 1000);
    if(err != ESP_OK) {
        log_e("ESP32 Errorcode %i", err);
        return false;
    }
    if(m_i2s_bytesWritten < 4) {
        log_e("Can't stuff any more in I2S..."); // increase waitingtime or outputbuffer
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setTone(uint8_t l_type, uint16_t l_freq, uint8_t r_type, uint16_t r_freq){
    // see https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
    // l_ left channel, r_ right channel
    // type: 0- lowpass, 1- highpass, 2 - lowshelf, 3 - highshelf
    // freq: frequency in Hz (between 10....samlerate/2), freq =0 - no filter
    // low/highpass have Qfactor = 0.707, low/highshelf have Gain -5dB
    m_filterType[LEFTCHANNEL]       = l_type;
    m_filterType[RIGHTCHANNEL]      = r_type;
    m_filterFrequency[LEFTCHANNEL]  = l_freq;
    m_filterFrequency[RIGHTCHANNEL] = r_freq;
    IIR_calculateCoefficients();
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::IIR_calculateCoefficients(){  // Infinite Impulse Response (IIR) filters
    // https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
    float K, G, norm, Q, Fc ;
    Q = 0.707; // Quality factor
    G = -5;    // Gain (dB)
    double V = pow(10, fabs(G) / 20.0);

    if(m_filterFrequency[LEFTCHANNEL]  == 0)               m_filterType[LEFTCHANNEL] = 254; // filter without effect
    else if(m_filterFrequency[LEFTCHANNEL]  < 10)          m_filterFrequency[LEFTCHANNEL] = 10;
    if(m_filterFrequency[LEFTCHANNEL]  > m_sampleRate / 2) m_filterFrequency[LEFTCHANNEL] = m_sampleRate / 2;
    if(m_filterFrequency[RIGHTCHANNEL] == 0)               m_filterType[RIGHTCHANNEL] = 254; // filter without effect
    else if(m_filterFrequency[RIGHTCHANNEL] < 10)          m_filterFrequency[RIGHTCHANNEL] = 10;
    if(m_filterFrequency[RIGHTCHANNEL] > m_sampleRate / 2) m_filterFrequency[RIGHTCHANNEL] = m_sampleRate / 2;

    if(m_sampleRate!=0){
        if(m_filterType[LEFTCHANNEL] == 0){ // lowpass
            Fc = (float)m_filterFrequency[LEFTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (1 + K / Q + K * K);
            m_filter[LEFTCHANNEL].a0 = K * K * norm;
            m_filter[LEFTCHANNEL].a1 = 2 * m_filter[LEFTCHANNEL].a0;
            m_filter[LEFTCHANNEL].a2 = m_filter[LEFTCHANNEL].a0;
            m_filter[LEFTCHANNEL].b1 = 2 * (K * K - 1) * norm;
            m_filter[LEFTCHANNEL].b2 = (1 - K / Q + K * K) * norm;
        }
        else if(m_filterType[LEFTCHANNEL] == 1){ // highpass
            Fc = (float)m_filterFrequency[LEFTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (1 + K / Q + K * K);
            m_filter[LEFTCHANNEL].a0 = 1 * norm;
            m_filter[LEFTCHANNEL].a1 = -2 * m_filter[LEFTCHANNEL].a0;
            m_filter[LEFTCHANNEL].a2 = m_filter[LEFTCHANNEL].a0;
            m_filter[LEFTCHANNEL].b1 = 2 * (K * K - 1) * norm;
            m_filter[LEFTCHANNEL].b2 = (1 - K / Q + K * K) * norm;
        }
        else if(m_filterType[LEFTCHANNEL] == 2){ // lowshelf
            Fc = (float)m_filterFrequency[LEFTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
            m_filter[LEFTCHANNEL].a0 = (1 + sqrt(2) * K + K * K) * norm;
            m_filter[LEFTCHANNEL].a1 = 2 * (K * K - 1) * norm;
            m_filter[LEFTCHANNEL].a2 = (1 - sqrt(2) * K + K * K) * norm;
            m_filter[LEFTCHANNEL].b1 = 2 * (V * K * K - 1) * norm;
            m_filter[LEFTCHANNEL].b2 = (1 - sqrt(2 * V) * K + V * K * K) * norm;
        }
        else if(m_filterType[LEFTCHANNEL] == 3){ // highshelf
            Fc = (float)m_filterFrequency[LEFTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (V + sqrt(2*V) * K + K * K);
            m_filter[LEFTCHANNEL].a0 = (1 + sqrt(2) * K + K * K) * norm;
            m_filter[LEFTCHANNEL].a1 = 2 * (K * K - 1) * norm;
            m_filter[LEFTCHANNEL].a2 = (1 - sqrt(2) * K + K * K) * norm;
            m_filter[LEFTCHANNEL].b1 = 2 * (K * K - V) * norm;
            m_filter[LEFTCHANNEL].b2 = (V - sqrt(2 * V) * K + K * K) * norm;
        }
        else { // IIR filter: out = in
            m_filter[RIGHTCHANNEL].a0 = 1;
            m_filter[RIGHTCHANNEL].a1 = 0;
            m_filter[RIGHTCHANNEL].a2 = 0;
            m_filter[RIGHTCHANNEL].b1 = 0;
            m_filter[RIGHTCHANNEL].b2 = 0;
        }

        if(m_filterType[RIGHTCHANNEL] == 0){ // lowpass
            Fc = (float)m_filterFrequency[RIGHTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (1 + K / Q + K * K);
            m_filter[RIGHTCHANNEL].a0 = K * K * norm;
            m_filter[RIGHTCHANNEL].a1 = 2 * m_filter[RIGHTCHANNEL].a0;
            m_filter[RIGHTCHANNEL].a2 = m_filter[RIGHTCHANNEL].a0;
            m_filter[RIGHTCHANNEL].b1 = 2 * (K * K - 1) * norm;
            m_filter[RIGHTCHANNEL].b2 = (1 - K / Q + K * K) * norm;
        }
        else if(m_filterType[RIGHTCHANNEL] == 1){ // highpass
            Fc = (float)m_filterFrequency[RIGHTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (1 + K / Q + K * K);
            m_filter[RIGHTCHANNEL].a0 = 1 * norm;
            m_filter[RIGHTCHANNEL].a1 = -2 * m_filter[RIGHTCHANNEL].a0;
            m_filter[RIGHTCHANNEL].a2 = m_filter[RIGHTCHANNEL].a0;
            m_filter[RIGHTCHANNEL].b1 = 2 * (K * K - 1) * norm;
            m_filter[RIGHTCHANNEL].b2 = (1 - K / Q + K * K) * norm;
        }
        else if(m_filterType[RIGHTCHANNEL] == 2){ // lowshelf
            Fc = (float)m_filterFrequency[RIGHTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
            m_filter[RIGHTCHANNEL].a0 = (1 + sqrt(2) * K + K * K) * norm;
            m_filter[RIGHTCHANNEL].a1 = 2 * (K * K - 1) * norm;
            m_filter[RIGHTCHANNEL].a2 = (1 - sqrt(2) * K + K * K) * norm;
            m_filter[RIGHTCHANNEL].b1 = 2 * (V * K * K - 1) * norm;
            m_filter[RIGHTCHANNEL].b2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
        }
        else if(m_filterType[RIGHTCHANNEL] == 3){ // highshelf
            Fc = (float)m_filterFrequency[RIGHTCHANNEL] / (float)m_sampleRate;
            K = tan(PI * Fc);
            norm = 1 / (V + sqrt(2*V) * K + K * K);
            m_filter[RIGHTCHANNEL].a0 = (1 + sqrt(2) * K + K * K) * norm;
            m_filter[RIGHTCHANNEL].a1 = 2 * (K * K - 1) * norm;
            m_filter[RIGHTCHANNEL].a2 = (1 - sqrt(2) * K + K * K) * norm;
            m_filter[RIGHTCHANNEL].b1 = 2 * (K * K - V) * norm;
            m_filter[RIGHTCHANNEL].b2 = (V - sqrt(2*V) * K + K * K) * norm;
        }
        else { // IIR filter: out = in
            m_filter[RIGHTCHANNEL].a0 = 1;
            m_filter[RIGHTCHANNEL].a1 = 0;
            m_filter[RIGHTCHANNEL].a2 = 0;
            m_filter[RIGHTCHANNEL].b1 = 0;
            m_filter[RIGHTCHANNEL].b2 = 0;
        }
    }

    int16_t tmp[2];
    IIR_filterChain(tmp, true ); // flush the filter

}
//---------------------------------------------------------------------------------------------------------------------
int16_t* Audio::IIR_filterChain(int16_t iir_in[2], bool clear){  // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum: uint8_t {in = 0, out = 1};
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if(clear){
        memset(m_filterBuff, 0, sizeof(m_filterBuff));            // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0]  = 0;
        iir_in[1]  = 0;
    }

    inSample[LEFTCHANNEL]  = (float)(iir_in[LEFTCHANNEL] >> 1);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL] >> 1);

    outSample[LEFTCHANNEL] =   m_filter[LEFTCHANNEL].a0  * inSample[LEFTCHANNEL]
                             + m_filter[LEFTCHANNEL].a1  * m_filterBuff[z1][in] [LEFTCHANNEL]
                             + m_filter[LEFTCHANNEL].a2  * m_filterBuff[z2][in] [LEFTCHANNEL]
                             - m_filter[LEFTCHANNEL].b1  * m_filterBuff[z1][out][LEFTCHANNEL]
                             - m_filter[LEFTCHANNEL].b2  * m_filterBuff[z2][out][LEFTCHANNEL];

    m_filterBuff[z2][in] [LEFTCHANNEL]  = m_filterBuff[z1][in][LEFTCHANNEL];
    m_filterBuff[z1][in] [LEFTCHANNEL]  = inSample[LEFTCHANNEL];
    m_filterBuff[z2][out][LEFTCHANNEL]  = m_filterBuff[z1][out][LEFTCHANNEL];
    m_filterBuff[z1][out][LEFTCHANNEL]  = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)outSample[LEFTCHANNEL];


    outSample[RIGHTCHANNEL] =  m_filter[RIGHTCHANNEL].a0 * inSample[RIGHTCHANNEL]
                             + m_filter[RIGHTCHANNEL].a1 * m_filterBuff[z1][in] [RIGHTCHANNEL]
                             + m_filter[RIGHTCHANNEL].a2 * m_filterBuff[z2][in] [RIGHTCHANNEL]
                             - m_filter[RIGHTCHANNEL].b1 * m_filterBuff[z1][out][RIGHTCHANNEL]
                             - m_filter[RIGHTCHANNEL].b2 * m_filterBuff[z2][out][RIGHTCHANNEL];

    m_filterBuff[z2][in] [RIGHTCHANNEL] = m_filterBuff[z1][in][RIGHTCHANNEL];
    m_filterBuff[z1][in] [RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[z2][out][RIGHTCHANNEL] = m_filterBuff[z1][out][RIGHTCHANNEL];
    m_filterBuff[z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t) outSample[RIGHTCHANNEL];

    return iir_out;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::forceMono(bool m) { // #100 mono option
    m_f_forceMono = m; // false stereo, true mono
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setBalance(int8_t bal){ // bal -16...16
    if(bal < -16) bal = -16;
    if(bal >  16) bal =  16;
    m_balance = bal;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setVolume(uint8_t vol) { // vol 22 steps, 0...21
    if(vol > 21) vol = 21;
    m_vol = volumetable[vol];
}
//---------------------------------------------------------------------------------------------------------------------
uint8_t Audio::getVolume() {
    for(uint8_t i = 0; i < 22; i++) {
        if(volumetable[i] == m_vol) return i;
    }
    m_vol = 12; // if m_vol not found in table
    return m_vol;
}
//---------------------------------------------------------------------------------------------------------------------
int32_t Audio::Gain(int16_t s[2]) {
    int32_t v[2];
    float step = (float)m_vol /64;
    uint8_t l = 0, r = 0;

    if(m_balance < 0){
        step = step * abs(m_balance) * 4;
        l = (uint8_t)(step);
    }
    if(m_balance > 0){
        step = step * m_balance * 4;
        r = (uint8_t)(step);
    }

    v[LEFTCHANNEL] = (s[LEFTCHANNEL]  * (m_vol - l)) >> 6;
    v[RIGHTCHANNEL]= (s[RIGHTCHANNEL] * (m_vol - r)) >> 6;
    return (v[RIGHTCHANNEL] << 16) | (v[LEFTCHANNEL] & 0xffff);
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::inBufferFilled() {
    return InBuff.bufferFilled();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::inBufferFree() {
    return InBuff.freeSpace();
}


