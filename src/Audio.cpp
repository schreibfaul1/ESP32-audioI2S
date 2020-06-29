/*
 * Audio.cpp
 *
 *  Created on: Oct 26,2018
 *  Updated on: Jun 28,2020
 *      Author: Wolle
 *
 *  This library plays mp3 files from SD card or icy-webstream  via I2S,
 *  play Google TTS and plays also aac-streams
 *  no DAC, no DeltSigma
 *
 *  etrernal HW on I2S nessesary, e.g.MAX98357A
 *
 */

#include "Audio.h"
#include "mp3_decoder/mp3_decoder.h"
#include "aac_decoder/aac_decoder.h"

//---------------------------------------------------------------------------------------------------------------------
Audio::Audio() {
    //i2s configuration
    m_i2s_num = I2S_NUM_0; // i2s port number
    i2s_config_t i2s_config = {
         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
         .sample_rate = 16000,
         .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
         .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
         .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
         .dma_buf_count = 8,  // max buffers
         .dma_buf_len = 1024, // max value
         .use_apll=APLL_DISABLE,
         .tx_desc_auto_clear= true,  // new in V1.0.1
         .fixed_mclk=-1
    };
    i2s_driver_install((i2s_port_t)m_i2s_num, &i2s_config, 0, NULL);

    m_BCLK=26;                       // Bit Clock
    m_LRC=25;                        // Left/Right Clock
    m_DOUT=27;                       // Data Out
    setPinout(m_BCLK, m_LRC, m_DOUT, m_DIN);
}

//---------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::I2Sstart(uint8_t i2s_num){
    return i2s_start((i2s_port_t) i2s_num);
}

esp_err_t Audio::I2Sstop(uint8_t i2s_num){
    return i2s_stop((i2s_port_t) i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------
Audio::~Audio() {
    I2Sstop(m_i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttohost(String host){

    if(host.length()==0){
        if(audio_info) audio_info("Hostaddress is empty");
        return false;
    }
    stopSong();
    I2Sstop(0);
    I2Sstart(0);
    MP3Decoder_FreeBuffers();
    AACDecoder_FreeBuffers();

    int16_t inx;                                          // Position of ":" in hostname
    uint16_t port=80;                                     // Port number for host
    String extension="/";                                 // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext;                                     // Host without extension and portnumber
    String headerdata="";
    m_f_localfile=false;
    m_f_webstream=true;
    if(m_lastHost!=host){                                 // New host or reconnection?
        m_lastHost=host;                                  // Remember the current host
    }
    sprintf(chbuf, "Connect to new host: %s", host.c_str());
    if(audio_info) audio_info(chbuf);

    // initializationsequence

    m_f_ctseen=false;                                       // Contents type not seen yet
    m_f_mp3=false;                                          // Set if stream is mp3
    m_f_aac=false;                                          // Set if stream is aac
    m_f_firststream_ready=false;
    m_f_firstmetabyte=false;
    m_f_webfile = false;                                    // Assume radiostream
    m_f_chunked=false;                                      // Assume not chunked
    m_f_ssl=false;
    m_f_swm=true;                                           // Assume no metaint
    m_f_playing=false;
    m_count=0;
    m_chunkcount=0;
    m_metaint=0;                                            // No metaint yet
    m_LFcount=0;                                            // For detection end of header
    m_bitrate=0;                                            // Bitrate still unknown
    m_totalcount=0;                                         // Reset totalcount
    m_bytesNotDecoded=0;                                    // counts all not decodable bytes
    m_metaline="";                                          // No metadata yet
    m_icyname="";                                           // No StationName yet
    m_st_remember="";                                       // Delete the last streamtitle
    m_inBuffwindex=0;
    m_inBuffrindex=0;
    m_contentlength=0;                                      // If Content-Length is known, count it
    m_audioCurrentTime=0;                                   // Reset playtimer
    m_avr_bitrate=0;                                        // the same as m_bitrate if CBR, median if VBR
    setDatamode(AUDIO_HEADER);                              // Handle header

    if(host.startsWith("http://")) {host=host.substring(7); m_f_ssl=false; ;}
    if(host.startsWith("https://")){host=host.substring(8); m_f_ssl=true; port=443;}
    client.stop(); client.flush(); // release memory
    clientsecure.stop(); clientsecure.flush();

    // Is it a playlist?
    if(host.endsWith(".m3u")|| host.endsWith(".pls")|| host.endsWith("asx")){
        m_playlist=host;                                    // Save copy of playlist URL
        m_datamode=AUDIO_PLAYLISTINIT;                      // Yes, start in PLAYLIST mode
        if(m_playlist_num == 0){                            // First entry to play?
            m_playlist_num=1;                               // Yes, set index
        }
        sprintf(chbuf, "Playlist request, entry %d", m_playlist_num);
        if(audio_info) audio_info(chbuf);                   // Most of the time there are zero bytes of metadata
    }

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    inx=host.indexOf("/");                                  // Search for begin of extension
    if(inx > 0){                                            // Is there an extension?
        extension=host.substring(inx);                      // Yes, change the default
        hostwoext=host.substring(0, inx);                   // Host without extension

    }
    // In the URL there may be a portnumber
    inx=host.indexOf(":");                                  // Search for separator
    if(inx >= 0){                                           // Portnumber available?
        port=host.substring(inx + 1).toInt();               // Get portnumber as integer
        hostwoext=host.substring(0, inx);                   // Host without portnumber
    }
    sprintf(chbuf, "Connect to %s on port %d, extension %s",
            hostwoext.c_str(), port, extension.c_str());
    if(audio_info) audio_info(chbuf);
    if(audio_showstreaminfo) audio_showstreaminfo(chbuf);

    String resp=String("GET ") + extension +
                String(" HTTP/1.1\r\n") +
                String("Host: ") + hostwoext +
                String("\r\n") +
                String("Icy-MetaData:1\r\n") +
                String("Connection: close\r\n\r\n");

    if(m_f_ssl==false){
        if(client.connect(hostwoext.c_str(), port)){
            if(audio_info) audio_info("Connected to server");
            client.print(resp);
            m_f_running=true;
            return true;
        }
    }
    if(m_f_ssl==true){
        if(clientsecure.connect(hostwoext.c_str(), port)){
            if(audio_info) audio_info("SSL/TLS Connected to server");
            clientsecure.print(resp);
            m_f_running=true;
            return true;
        }
    }
    sprintf(chbuf, "Request %s failed!", host.c_str());
    if(audio_info) audio_info(chbuf);
    if(audio_showstation) audio_showstation("");
    if(audio_showstreamtitle) audio_showstreamtitle("");
    if(audio_showstreaminfo) audio_showstreaminfo("");
    return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------
bool Audio::connecttoSD(String sdfile){
    return connecttoFS(SD, sdfile);
}
//-----------------------------------------------------------------------------------------------------------------------------------
bool Audio::connecttoFS(fs::FS &fs, String file){
    const uint8_t ascii[60]={
          //196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215,   ISO
            142, 143, 146, 128, 000, 144, 000, 000, 000, 000, 000, 000, 000, 165, 000, 000, 000, 000, 153, 000, //ASCII
          //216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,   ISO
            000, 000, 000, 000, 154, 000, 000, 225, 133, 000, 000, 000, 132, 143, 145, 135, 138, 130, 136, 137, //ASCII
          //236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255    ISO
            000, 161, 140, 139, 000, 164, 000, 162, 147, 000, 148, 000, 000, 000, 163, 150, 129, 000, 000, 152};//ASCII

    uint16_t i=0, s=0;
    stopSong();
    I2Sstop(0);
    I2Sstart(0);
    m_f_playing=false;
    m_inBuffwindex=0;
    m_inBuffrindex=0;
    m_audioFileDuration = 0;
    m_audioCurrentTime = 0;
    m_avr_bitrate=0;                                        // the same as m_bitrate if CBR, median if VBR
    clientsecure.stop(); clientsecure.flush();              // release memory if allocated
    m_f_localfile=true;
    m_f_webstream=false;
    m_f_stream=false;
    m_f_mp3=true;
    m_f_aac=false;
    AACDecoder_FreeBuffers();
    MP3Decoder_AllocateBuffers();
    if(!file.startsWith("/")) file="/"+file;
    while(file[i] != 0){                                  //convert UTF8 to ASCII
        path[i]=file[i];
        if(path[i] > 195){
            s=ascii[path[i]-196];
            if(s!=0) path[i]=s;                             // found a related ASCII sign
        } i++;
    }
    path[i]=0;
    m_mp3title=file.substring(file.lastIndexOf('/') + 1, file.length());
    sprintf(chbuf, "Reading file: %s", m_mp3title.c_str());
    if(audio_info) audio_info(chbuf);
//    fs::FS &fs=SD;
    audiofile=fs.open(path);
    if(!audiofile){
        if(audio_info) audio_info("Failed to open file for reading");
        return false;
    }
    audiofile.readBytes(chbuf, 10);
    if ((chbuf[0] != 'I') || (chbuf[1] != 'D') || (chbuf[2] != '3')) {
        if(audio_info) audio_info("file has no mp3 tag, skip metadata");
        setFilePos(0);
        m_f_running=true;
        return false;
    }
    m_rev = chbuf[3];
    switch (m_rev) {
    case 2:
        m_f_unsync = (chbuf[5] & 0x80);
        m_f_exthdr = false;
        break;
    case 3:
    case 4:
        m_f_unsync = (chbuf[5] & 0x80); // bit7
        m_f_exthdr = (chbuf[5] & 0x40); // bit6 extended header
        break;
    };

    m_id3Size  = chbuf[6]; m_id3Size = m_id3Size << 7;
    m_id3Size |= chbuf[7]; m_id3Size = m_id3Size << 7;
    m_id3Size |= chbuf[8]; m_id3Size = m_id3Size << 7;
    m_id3Size |= chbuf[9];

    // Every read from now may be unsync'd
    sprintf(chbuf, "ID3 version=%i", m_rev);
    if(audio_info) audio_info(chbuf);
    sprintf(chbuf,"ID3 framesSize=%i", m_id3Size);
    if(audio_info) audio_info(chbuf);
    readID3Metadata();
    m_f_running=true;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttospeech(String speech, String lang){
    String host="translate.google.com";
    String path="/translate_tts";
    uint16_t free=0;
    m_f_playing=false;
//    m_f_firststream_ready=false;
    m_f_localfile=false;
    m_f_webstream=false;
    m_f_ssl=true;
    stopSong();
    clientsecure.stop(); clientsecure.flush();  // release memory if allocated

    String resp=   String("GET / HTTP/1.0\r\n") +
                   String("Host: ") + host + String("\r\n") +
                   String("User-Agent: GoogleTTS for ESP32/1.0.0\r\n") +
                   String("Accept-Encoding: identity\r\n") +
                   String("Accept: text/html\r\n\r\n");

    if (!clientsecure.connect(host.c_str(), 443)) {
        Serial.println("Connection failed");
        return false;
    }
    clientsecure.print(resp);

    while (clientsecure.connected()) {  // read the header
        String line = clientsecure.readStringUntil('\n');
        line+="\n";
        if (line == "\r\n") break;
    }

    String tkkFunc="";
    char ch;

    do {  // search for TKK
        tkkFunc = "";
        clientsecure.readBytes(&ch, 1);
        if (ch != 't') continue;
        tkkFunc += String(ch);
        clientsecure.readBytes(&ch, 1);
        if (ch != 'k') continue;
        tkkFunc += String(ch);
        clientsecure.readBytes(&ch, 1);
        if (ch != 'k') continue;
        tkkFunc += String(ch);
        clientsecure.readBytes(&ch, 1);
        if (ch != ':') continue;
        tkkFunc += String(ch);
    } while(tkkFunc.length() < 4);

    tkkFunc +=  clientsecure.readStringUntil(',');  // "TKK='xxxxxxxxx.yyyyyyyyy'"
    tkkFunc = tkkFunc.substring(5 /* length of "TKK='" */, tkkFunc.lastIndexOf('\''));

//    log_i("tkkFunc_%s", tkkFunc.c_str());

    // create token
    int16_t periodPos = tkkFunc.indexOf('.');
    String key1 = tkkFunc.substring(0,periodPos);
    String key2 = tkkFunc.substring(periodPos + 1);
    int64_t a, b;
    a = b = strtoll(key1.c_str(), NULL, 10);

    int f;
    int len = strlen(speech.c_str());
    for (f = 0; f < len; f++) {
      a += speech[f];
      a = XL(a, "+-a^+6");
    }
    a = XL(a, "+-3^+b+-f");
    a = a ^ (strtoll(key2.c_str(), NULL, 10));
    if (0 > a) {
      a = (a & 2147483647) + 2147483648;
    }
    a = a % 1000000;
    String token=String(lltoa(a, 10)) + '.' + lltoa(a ^ b, 10);

    int i,j;
    const char* t = speech.c_str();
    for(i=0,j=0;i<strlen(t);i++) {
      if (t[i] < 0x80 || t[i] > 0xbf) {
        j++;
      }
    }
//    log_i("Token=%s", token.c_str());
    String tts= String("https://") + host + path +
                        "?ie=UTF-8&q=" + urlencode(speech) +
                        "&tl=" + lang +
                        "&textlen=" + String(j) +
                        "&tk=" + token +
                        "&total=1&idx=0&client=t&prev=input&ttsspeed=1";

    clientsecure.stop();  clientsecure.flush();

    resp=   String("GET ") + tts + String("HTTP/1.1\r\n") +
            String("Host: ") + host + String("\r\n") +
            String("Connection: close\r\n\r\n");

    if (!clientsecure.connect(host.c_str(), 443)) {
        Serial.println("Connection failed");
        return false;
    }
    clientsecure.print(resp);

    while (clientsecure.connected()) {
        String line = clientsecure.readStringUntil('\n');
        line+="\n";
//      if(audio_info) audio_info(line.c_str());
        if (line == "\r\n") break;
    }
    m_inBuffwindex=0;
    m_f_mp3=true;
    AACDecoder_FreeBuffers();
    MP3Decoder_AllocateBuffers();
    uint16_t res;
    uint16_t x=0;
    playI2Sremains();
    while(clientsecure.available()==0){;}
    while(clientsecure.available() > 0) {
            free=m_inBuffsize-m_inBuffwindex;// free space
            res=clientsecure.read(m_inBuff + m_inBuffwindex, free);
            if(res>0){
                m_inBuffwindex+=res;
                free-=res;
            }
            if(free==0){
                x=sendBytes(m_inBuff, m_inBuffsize);
                m_inBuffwindex-=x;
                memmove(m_inBuff, m_inBuff + x , m_inBuffsize-x);
           }
    }
    sendBytes(m_inBuff, m_inBuffsize -free);
    memset(m_outBuff, 0, sizeof(m_outBuff));
    for(int i=0; i<4; i++){
        m_validSamples = 2048;
        while(m_validSamples) {
            playChunk();
        }
    }
    playI2Sremains();
    MP3Decoder_FreeBuffers();
    stopSong();
    clientsecure.stop();  clientsecure.flush();
    m_f_mp3=false;
    if(audio_eof_speech) audio_eof_speech(speech.c_str());
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
long long int Audio::XL (long long int a, const char* b) {
    int len = strlen(b);
    for (int c = 0; c < len - 2; c += 3) {
        int  d = (long long int)b[c + 2];
        d = d >= 97 ? d - 87 : d - 48;
        d = (b[c + 1] == '+' ? a >> d : a << d);
        a = b[c] == '+' ? (a + d) & 4294967295 : a ^ d;
    }
    return a;
}
//---------------------------------------------------------------------------------------------------------------------
char* Audio::lltoa(long long val, int base){

    static char buf[64] = {0};
    static char chn=0;
    int i = 62;
    int sign = (val < 0);
    if(sign) val = -val;

    if(val == 0) return &chn;

    for(; val && i ; --i, val /= base) {
        buf[i] = "0123456789abcdef"[val % base];
    }

    if(sign) {
        buf[i--] = '-';
    }
    return &buf[i+1];
}
//---------------------------------------------------------------------------------------------------------------------

String Audio::urlencode(String str){
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
        if (c == ' ') encodedString+= '+';
        else if (isalnum(c)) encodedString+=c;
        else{
            code1=(c & 0xf)+'0';
            if ((c & 0xf) >9) code1=(c & 0xf) - 10 + 'A';
            c=(c>>4)&0xf;
            code0=c+'0';
            if (c > 9) code0=c - 10 + 'A';
            encodedString+='%';
            encodedString+=code0;
            encodedString+=code1;
        }
    }
    return encodedString;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::readID3Metadata(){
    char frameid[5];
    int framesize=0;
    bool compressed;
    char value[256];
    bool bitorder=false;
    uint8_t uni_h=0;
    uint8_t uni_l=0;
    int id3Size = m_id3Size;
    String tag="";
    if (m_f_exthdr) {
        if(audio_info) audio_info("ID3 extended header");
        int ehsz = (audiofile.read() << 24) | (audiofile.read() << 16)
                | (audiofile.read() << 8) | (audiofile.read());
        id3Size -= 4;
        for (int j = 0; j < ehsz - 4; j++) {
            audiofile.read();
            id3Size--;
        } // Throw it away
    } else
        if(audio_info) audio_info("ID3 normal frames");

    do {
        frameid[0] = audiofile.read();
        frameid[1] = audiofile.read();
        frameid[2] = audiofile.read();
        id3Size -= 3;
        if   (m_rev == 2) frameid[3] = 0;
        else {frameid[3] = audiofile.read(); id3Size--;}
        frameid[4]=0; // terminate the string
        tag=frameid;
        if(frameid[0]==0 && frameid[1]==0 && frameid[2]==0 && frameid[3]==0){
            // We're in padding
            while (id3Size != 0){audiofile.read(); id3Size--;}
        }
        else{
            if(m_rev==2){
                framesize = (audiofile.read() << 16) | (audiofile.read() << 8) | (audiofile.read());
                id3Size -= 3;
                compressed = false;
            }
            else{
                framesize = (audiofile.read() << 24) | (audiofile.read() << 16) | (audiofile.read() << 8) | (audiofile.read());
                id3Size -= 4;
                audiofile.read(); // skip 1st flag
                id3Size--;
                compressed = audiofile.read() & 0x80;
                id3Size--;
            }
            if(compressed){
                log_i("iscompressed");
                int decompsize=(audiofile.read()<<24) | (audiofile.read()<<16) | (audiofile.read()<<8) | (audiofile.read());
                id3Size -= 4;
                (void) decompsize;
                for (int j = 0; j < framesize; j++){
                    audiofile.read();
                    id3Size--;
                }
            }
            // Read the value
            uint32_t i=0; uint16_t j=0, k=0, m=0;
            bool isUnicode;
            if(framesize>0){
                isUnicode = (audiofile.read() == 1) ? true : false;
                id3Size--;
                if(framesize<256){
                    audiofile.readBytes(value, framesize-1); id3Size-=framesize-1;
                    i=framesize-1; value[framesize-1]=0;
                }
                else{
                    if(tag=="APIC"){ // a image embedded in file, skip it
                        //log_i("it's a image");
                        isUnicode = false;
                        setFilePos(getFilePos()+framesize-1); id3Size-=framesize-1;
                    }
                    else{
                        // store the first 255 bytes in buffer and cut the remains
                        audiofile.readBytes(value, 255); id3Size-=255;
                        value[255]=0; i=255;
                        // big block, skip it
                        setFilePos(getFilePos()+framesize-1-255); id3Size-=framesize-1;
                    }
                }
                if(isUnicode && framesize > 1){  // convert unicode to utf-8 U+0020...U+07FF
                    j=0; m=0;
                    while(m<i-1){
                        if((value[m]==0xFE)&&(value[m+1]==0xFF)){bitorder=true; j=m+2;}// MSB/LSB
                        if((value[m]==0xFF)&&(value[m+1]==0xFE)){bitorder=false; j=m+2;}//LSB/MSB
                        m++;
                    } // seek for last bitorder
                    m=0;
                    if(j>0){
                        for(k=j; k<i-1; k+=2){
                            if(bitorder==true){uni_h=value[k]; uni_l=value[k+1];}
                            else              {uni_l=value[k]; uni_h=value[k+1];}
                            uint16_t uni_hl=(uni_h<<8)+uni_l;
                            uint8_t utf8_h=(uni_hl>>6); // div64
                            uint8_t utf8_l=uni_l;
                            if(utf8_h>3){
                                utf8_h+=0xC0;
                                if     (uni_l<0x40) utf8_l=uni_l+0x80;
                                else if(uni_l<0x80) utf8_l=uni_l+=0x40;
                                else if(uni_l<0xC0) utf8_l=uni_l;
                                else    utf8_l=uni_l-0x40;
                            }
                            if(utf8_h>3) {value[m]= utf8_h; m++;}
                            value[m]=utf8_l; m++;
                        }
                    }value[m]=0; i=m;
                }
            }
            chbuf[0]=0; j=0; k=0;
            while(j<i){if(value[j]>0x19){value[k]=value[j]; k++;}else{i--;} j++;} //remove non printables
            value[i]=0; // new termination
            // Revision 2
            if(tag=="CNT") sprintf(chbuf, "Play counter: %s", value);
            if(tag=="COM") sprintf(chbuf, "Comments: %s", value);
            if(tag=="CRA") sprintf(chbuf, "Audio encryption: %s", value);
            if(tag=="CRM") sprintf(chbuf, "Encrypted meta frame: %s", value);
            if(tag=="ETC") sprintf(chbuf, "Event timing codes: %s", value);
            if(tag=="EQU") sprintf(chbuf, "Equalization: %s", value);
            if(tag=="IPL") sprintf(chbuf, "Involved people list: %s", value);
            if(tag=="PIC") sprintf(chbuf, "Attached picture: %s", value);
            if(tag=="SLT") sprintf(chbuf, "Synchronized lyric/text: %s", value);
            if(tag=="TAL") sprintf(chbuf, "Album/Movie/Show title: %s", value);
            if(tag=="TBP") sprintf(chbuf, "BPM (Beats Per Minute): %s", value);
            if(tag=="TCM") sprintf(chbuf, "Composer: %s", value);
            if(tag=="TCO") sprintf(chbuf, "Content type: %s", value);
            if(tag=="TCR") sprintf(chbuf, "Copyright message: %s", value);
            if(tag=="TDA") sprintf(chbuf, "Date: %s", value);
            if(tag=="TDY") sprintf(chbuf, "Playlist delay: %s", value);
            if(tag=="TEN") sprintf(chbuf, "Encoded by: %s", value);
            if(tag=="TFT") sprintf(chbuf, "File type: %s", value);
            if(tag=="TIM") sprintf(chbuf, "Time: %s", value);
            if(tag=="TKE") sprintf(chbuf, "Initial key: %s", value);
            if(tag=="TLA") sprintf(chbuf, "Language(s): %s", value);
            if(tag=="TLE") sprintf(chbuf, "Length: %s", value);
            if(tag=="TMT") sprintf(chbuf, "Media type: %s", value);
            if(tag=="TOA") sprintf(chbuf, "Original artist(s)/performer(s): %s", value);
            if(tag=="TOF") sprintf(chbuf, "Original filename: %s", value);
            if(tag=="TOL") sprintf(chbuf, "Original Lyricist(s)/text writer(s): %s", value);
            if(tag=="TOR") sprintf(chbuf, "Original release year: %s", value);
            if(tag=="TOT") sprintf(chbuf, "Original album/Movie/Show title: %s", value);
            if(tag=="TP1") sprintf(chbuf, "Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group: %s", value);
            if(tag=="TP2") sprintf(chbuf, "Band/Orchestra/Accompaniment: %s", value);
            if(tag=="TP3") sprintf(chbuf, "Conductor/Performer refinement: %s", value);
            if(tag=="TP4") sprintf(chbuf, "Interpreted, remixed, or otherwise modified by: %s", value);
            if(tag=="TPA") sprintf(chbuf, "Part of a set: %s", value);
            if(tag=="TPB") sprintf(chbuf, "Publisher: %s", value);
            if(tag=="TRC") sprintf(chbuf, "ISRC (International Standard Recording Code): %s", value);
            if(tag=="TRD") sprintf(chbuf, "Recording dates: %s", value);
            if(tag=="TRK") sprintf(chbuf, "Track number/Position in set: %s", value);
            if(tag=="TSI") sprintf(chbuf, "Size: %s", value);
            if(tag=="TSS") sprintf(chbuf, "Software/hardware and settings used for encoding: %s", value);
            if(tag=="TT1") sprintf(chbuf, "Content group description: %s", value);
            if(tag=="TT2") sprintf(chbuf, "Title/Songname/Content description: %s", value);
            if(tag=="TT3") sprintf(chbuf, "Subtitle/Description refinement: %s", value);
            if(tag=="TXT") sprintf(chbuf, "Lyricist/text writer: %s", value);
            if(tag=="TXX") sprintf(chbuf, "User defined text information frame: %s", value);
            if(tag=="TYE") sprintf(chbuf, "Year: %s", value);
            if(tag=="UFI") sprintf(chbuf, "Unique file identifier: %s", value);
            if(tag=="ULT") sprintf(chbuf, "Unsychronized lyric/text transcription: %s", value);
            if(tag=="WAF") sprintf(chbuf, "Official audio file webpage: %s", value);
            if(tag=="WAR") sprintf(chbuf, "Official artist/performer webpage: %s", value);
            if(tag=="WAS") sprintf(chbuf, "Official audio source webpage: %s", value);
            if(tag=="WCM") sprintf(chbuf, "Commercial information: %s", value);
            if(tag=="WCP") sprintf(chbuf, "Copyright/Legal information: %s", value);
            if(tag=="WPB") sprintf(chbuf, "Publishers official webpage: %s", value);
            if(tag=="WXX") sprintf(chbuf, "User defined URL link frame: %s", value);
            // Revision 3
            if(tag=="COMM") sprintf(chbuf, "Comment: %s", value);
            if(tag=="OWNE") sprintf(chbuf, "Ownership: %s", value);
            if(tag=="PRIV") sprintf(chbuf, "Private: %s", value);
            if(tag=="SYLT") sprintf(chbuf, "SynLyrics: %s", value);
            if(tag=="TALB") sprintf(chbuf, "Album: %s", value);
            if(tag=="TBPM") sprintf(chbuf, "BeatsPerMinute: %s", value);
            if(tag=="TCMP") sprintf(chbuf, "Compilation: %s", value);
            if(tag=="TCOM") sprintf(chbuf, "Composer: %s", value);
            if(tag=="TCOP") sprintf(chbuf, "Copyright: %s", value);
            if(tag=="TDAT") sprintf(chbuf, "Date: %s", value);
            if(tag=="TEXT") sprintf(chbuf, "Lyricist: %s", value);
            if(tag=="TIME") sprintf(chbuf, "Time: %s", value);
            if(tag=="TIT1") sprintf(chbuf, "Grouping: %s", value);
            if(tag=="TIT2") sprintf(chbuf, "Title: %s", value);
            if(tag=="TIT3") sprintf(chbuf, "Subtitle: %s", value);
            if(tag=="TLAN") sprintf(chbuf, "Language: %s", value);
            if(tag=="TLEN") sprintf(chbuf, "Length: %s", value);
            if(tag=="TMED") sprintf(chbuf, "Media: %s", value);
            if(tag=="TOAL") sprintf(chbuf, "OriginalAlbum: %s", value);
            if(tag=="TOPE") sprintf(chbuf, "OriginalArtist: %s", value);
            if(tag=="TORY") sprintf(chbuf, "OriginalReleaseYear: %s", value);
            if(tag=="TPE1") sprintf(chbuf, "Artist: %s", value);
            if(tag=="TPE2") sprintf(chbuf, "Band: %s", value);
            if(tag=="TPE3") sprintf(chbuf, "Conductor: %s", value);
            if(tag=="TPE4") sprintf(chbuf, "InterpretedBy: %s", value);
            if(tag=="TPOS") sprintf(chbuf, "PartOfSet: %s", value);
            if(tag=="TPUB") sprintf(chbuf, "Publisher: %s", value);
            if(tag=="TRCK") sprintf(chbuf, "Track: %s", value);
            if(tag=="TRDA") sprintf(chbuf, "RecordingDates: %s", value);
            if(tag=="TXXX") sprintf(chbuf, "UserDefinedText: %s", value);
            if(tag=="TYER") sprintf(chbuf, "Year: %s", value);
            if(tag=="USER") sprintf(chbuf, "TermsOfUse: %s", value);
            if(tag=="USLT") sprintf(chbuf, "Lyrics: %s", value);
            if(tag=="XDOR") sprintf(chbuf, "OriginalReleaseTime: %s", value);
            if(chbuf[0]!=0) if(audio_id3data) audio_id3data(chbuf);
        }
    } while (id3Size > 0);
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::stopSong(){
    if(m_f_running){
        m_f_running = false;
        audiofile.close();
    }
    memset(m_outBuff, 0, sizeof(m_outBuff));     //Clear OutputBuffer
    i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::playI2Sremains(){
    // there is no  function to see if dma_buff is empty. So fill the dma completely.
    // As soon as all remains played this function returned. Or you can take this to create a short silence.
    if(m_bitrate==0)
    if(m_lastRate==0) setSampleRate(96000);
    if(m_channels==0) setChannels(2);
    memset(m_outBuff, 0, sizeof(m_outBuff));     //Clear OutputBuffer
    for(int i=0; i<5; i++){                      //play remains and then flush dmaBuff
        m_validSamples = 2048;
        while(m_validSamples) {
            playChunk();
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::pauseResume()
{
    bool retVal = false;
    if(m_f_localfile || m_f_webstream)
    {
        m_f_running = !m_f_running;
        retVal = true;
        if(!m_f_running) {
            memset(m_outBuff, 0, sizeof(m_outBuff));               //Clear OutputBuffer
            i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
        }
    }
    return retVal;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playChunk(){
    // If we've got data, try and pump it out...
    if(m_channels==1){
        while (m_validSamples) {
            m_lastSample[0] = m_outBuff[m_curSample];
            m_lastSample[1] = m_outBuff[m_curSample];
            if (!playSample(m_lastSample)) {return false;} // Can't send
            m_validSamples--;
            m_curSample++;
        }
    }
    if(m_channels==2){
        while (m_validSamples) {
            m_lastSample[0] = m_outBuff[m_curSample * 2];
            m_lastSample[1] = m_outBuff[m_curSample * 2 + 1];
            if (!playSample(m_lastSample)) {return false;} // Can't send
            m_validSamples--;
            m_curSample++;
        }
    }
    m_curSample=0;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::loop() 
{
    // - localfile - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_localfile)
    {
        processLocalFile();
    }
    // - webstream - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_webstream)
    {                                      // Playing file from URL?
        processWebStream();
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processLocalFile()
{
    if ( audiofile && m_f_running && m_f_localfile )
    {
        uint16_t bytesCanBeWritten = 0;
        int16_t  bytesAddedToBuffer = 0;

        bytesCanBeWritten = m_inBuffsize-m_inBuffwindex;
        bytesAddedToBuffer = audiofile.read(m_inBuff + m_inBuffwindex, bytesCanBeWritten);

        if( bytesAddedToBuffer > 0 )
        {
            m_inBuffwindex += bytesAddedToBuffer;
        }
        if ( m_inBuffwindex == m_inBuffsize )
        {
            if ( false == m_f_stream )
            {
                m_f_stream = true;
                playI2Sremains(); // create a short silence
                if(audio_info) audio_info("stream ready");
            }
            m_inBuffrindex = sendBytes(m_inBuff, m_inBuffsize);
            memmove(m_inBuff, m_inBuff + m_inBuffrindex , m_inBuffsize-m_inBuffrindex);
            m_inBuffwindex -= m_inBuffrindex;
            return;
        }
        else// eof
        {
            sendBytes(m_inBuff, m_inBuffwindex);         // write last chunk
            memset(m_outBuff, 0, sizeof(m_outBuff));     //Clear OutputBuffer
            for(int i=0; i<5; i++){
                m_validSamples = 2048;
                while(m_validSamples) {
                    playChunk();
                }
            }
            playI2Sremains();
            stopSong();
            m_f_stream=false;
            m_f_localfile=false;
            MP3Decoder_FreeBuffers();
            sprintf(chbuf,"End of mp3file %s", m_mp3title.c_str());
            if(audio_info) audio_info(chbuf);
            if(audio_eof_mp3) audio_eof_mp3(m_mp3title.c_str());

        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processWebStream()
{
    if(m_f_running && m_f_webstream)
    {
        uint16_t bytesCanBeWritten = 0;
        int16_t  bytesAddedToBuffer = 0;
        static uint32_t chunksize = 0;      // Chunkcount read from stream
        static uint16_t inBuffSize = m_inBuffsize;
        int32_t availableBytes = 0;         // Available bytes in stream
        static int bytesdecoded = 0;

        if (m_f_ssl==false) availableBytes = client.available();         // Available from stream
        if (m_f_ssl==true)  availableBytes = clientsecure.available();   // Available from stream
        if ( ( m_f_firststream_ready == false ) && ( availableBytes > 0 ) )
        { // first streamdata recognized
            m_f_firststream_ready = true;
            m_f_stream = false;
            inBuffSize = m_inBuffsize;
            m_bytectr=0;
        }

        if ((   ( m_datamode == AUDIO_DATA ) || ( m_datamode == AUDIO_SWM ) )
            && ( m_count > 0 )
        )
        {
            bytesCanBeWritten = inBuffSize - m_inBuffwindex;
            uint x = min( m_count , uint32_t(bytesCanBeWritten) );

            if ( ( m_f_chunked ) && ( x > m_chunkcount ) )
            {
                x = m_chunkcount;
            }

            if ( m_f_ssl == false )
            { 
                bytesAddedToBuffer = client.read(m_inBuff + m_inBuffwindex, x);
            }

            if ( m_f_ssl == true )
            {
                bytesAddedToBuffer=clientsecure.read(m_inBuff + m_inBuffwindex, x);
            }

            if ( bytesAddedToBuffer > 0 )
            {
                if(m_f_webfile){
                    m_bytectr+=bytesAddedToBuffer;  // Pull request #42
                }
                m_count -= bytesAddedToBuffer;
                if ( m_f_chunked )
                { 
                    m_chunkcount-=bytesAddedToBuffer;
                }
                m_inBuffwindex += bytesAddedToBuffer;
            }

            if ( m_inBuffwindex == inBuffSize )
            {
                if ( m_f_stream == false )
                {
                    m_f_stream = true;
                    playI2Sremains();
                    if(audio_info) audio_info("stream ready");
                }
                bytesdecoded = sendBytes(m_inBuff, inBuffSize);
                if ( bytesdecoded < 0 )   // no syncword found or decode error, try next chunk
                {
                    m_inBuffrindex = 200; // try next chunk
                    m_bytesNotDecoded += 200;
                    memmove(m_inBuff, m_inBuff + m_inBuffrindex , inBuffSize-m_inBuffrindex);
                    m_inBuffwindex -= m_inBuffrindex;
                    inBuffSize = m_inBuffsize;
                    return;
                }
                else
                {
                    m_inBuffrindex = bytesdecoded;
                    memmove(m_inBuff, m_inBuff + m_inBuffrindex , inBuffSize-m_inBuffrindex);
                    m_inBuffwindex -= m_inBuffrindex;
                }
            }
            if ( bytesAddedToBuffer == 0 )
            {
                if ( m_f_stream )
                {
//              seems no longer necessary, stream recognition over loopcounter loopCnt
//                    m_f_stream = false;
//                    i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
//                    if(audio_info) audio_info("stream lost");
                }
            }

            if ( m_count == 0 )
            {
                if ( m_datamode == AUDIO_SWM )
                {
                    m_count = 16000; //mms has no metadata
                }
                else
                {
                    m_datamode = AUDIO_METADATA;
                    m_f_firstmetabyte = true;
                }
            }
        }
        else //!=DATA
        {
            if ( m_datamode == AUDIO_PLAYLISTDATA ) 
            {
                if ( m_t0 + 49 < millis()) 
                {
                    handlebyte('\n');                       // send LF
                }
            }
            int16_t x = 0;
            if ( m_f_chunked && ( m_datamode != AUDIO_HEADER ) )
            {
                if ( m_chunkcount > 0 )
                {
                    if ( m_f_ssl == false )
                        x = client.read();
                    else
                        x = clientsecure.read();

                    if (x >= 0)
                    {
                        handlebyte(x);
                        m_chunkcount--;
                    }
                }
            }
            else
            {
                if ( m_f_ssl == false ) 
                    x = client.read();
                else    
                    x = clientsecure.read();

                if (x >= 0)
                {
                    handlebyte(x);
                }
            }
            if ( m_datamode == AUDIO_DATA )
            {
                m_count = m_metaint;
                if ( m_metaint == 0 )
                {
                    m_datamode = AUDIO_SWM; // stream without metadata, can be mms
                }
            }
            if ( m_datamode == AUDIO_SWM )
            {
                m_count =16000;
            }
        }

        if ( m_f_firststream_ready == true)
        {
            static uint32_t loopCnt = 0;      // Count loops if clientbuffer is empty
            if ( availableBytes == 0 )
            {   // empty buffer, broken stream or bad bitrate?
                loopCnt++;
                if( loopCnt > 200000)
                {  // wait several seconds
                    loopCnt = 0;
                    if(audio_info) audio_info("Stream lost -> try new connection");
                    connecttohost(m_lastHost); // try a new connection
                }
                if(m_f_webfile) // stream from fileserver with known content-length
                {
                    if((uint32_t)m_bytectr >= (uint32_t)m_contentlength -10){
                        sprintf(chbuf,"End of webstream: %s", m_lastHost.c_str());
                        if(audio_info) audio_info(chbuf);
                        if(audio_eof_stream) audio_eof_stream(m_lastHost.c_str());
                        m_f_running = false;
                    }
                }
            }
            else
            {
                loopCnt = 0;
            }
        }

        if ( m_f_chunked )
        {
            if ( m_datamode == AUDIO_HEADER )
                return;

            if ( m_chunkcount == 0)
            {             // Expecting a new chunkcount?
                int b;
                if (m_f_ssl == false)
                {
                    b = client.read ();
                }
                if (m_f_ssl == true)
                {
                     b = clientsecure.read ();
                }
                if ( b < 1 )
                    return;

                if( b == '\r' )
                {}
                else if ( b == '\n' )
                {
                    m_chunkcount = chunksize;
                    chunksize = 0;
                }
                else
                {
                    // We have received a hexadecimal character.  Decode it and add to the result.
                    b = toupper(b) - '0';                         // Be sure we have uppercase

                    if ( b > 9)
                         b = b - 7;                        // Translate A..F to 10..15

                    chunksize = (chunksize << 4) + b;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::handlebyte(uint8_t b){
    static uint16_t playlistcnt;                                // Counter to find right entry in playlist
    String lcml;                                                // Lower case metaline
    static String ct;                                           // Contents type
    static String host;
    int inx;                                                    // Pointer in metaline
    static boolean f_entry=false;                               // entryflag for asx playlist

    if(m_datamode == AUDIO_HEADER){                             // Handle next byte of MP3 header
        if((b>0x7F)||(b=='\r')||(b =='\0')){                    // Ignore unprintable characters, ignore CR, ignore NULL
           ;                                                    // Yes, ignore
        }
        else if(b == '\n'){                                     // Linefeed ?
            m_LFcount++;                                        // Count linefeeds
            if(chkhdrline(m_metaline.c_str())){                 // Reasonable input?
                lcml=m_metaline;                                // Use lower case for compare
                lcml.toLowerCase();
                lcml.trim();
                if(lcml.indexOf("content-type:") >= 0){         // Line with "Content-Type: xxxx/yyy"
                    if(lcml.indexOf("audio") >= 0){             // Is ct audio?
                        m_ctseen=true;                          // Yes, remember seeing this
                        ct=m_metaline.substring(13);            // Set contentstype. Not used yet
                        ct.trim();
                        sprintf(chbuf, "%s seen.", ct.c_str());
                        if(audio_info) audio_info(chbuf);
                        if(ct.indexOf("mpeg")>=0){
                            m_f_mp3=true;
                            MP3Decoder_AllocateBuffers();
                            if(audio_info) audio_info("format is mp3"); //ok is likely mp3
                        }
                        else if(ct.indexOf("aac")>=0){
                            m_f_aac=true;
                            AACDecoder_AllocateBuffers();
                            if(audio_info) audio_info("format is aac");
//                            stopSong(); // if no aac decoder available
                        }
                        else if(ct.indexOf("mp4")>=0){
                            m_f_aac=true;
                            AACDecoder_AllocateBuffers();
                            if(audio_info) audio_info("format is aac");
//                            stopSong(); // if no aac decoder available
                        }
                        else if(ct.indexOf("ogg")>=0){
                            m_f_running=false;
                            if(audio_info) audio_info("can't play ogg");
                            stopSong();
                        }
                        else{
                            m_f_running=false;
                            if(audio_info) audio_info("unknown audio format");
                        }
                    }
                    if(lcml.indexOf("ogg") >= 0){               // Is ct ogg?
                        m_f_running=false;
                        m_ctseen=true;                          // Yes, remember seeing this
                        ct=m_metaline.substring(13);
                        ct.trim();
                        sprintf(chbuf, "%s seen.", ct.c_str());
                        if(audio_info) audio_info(chbuf);
                        if(audio_info) audio_info("no ogg decoder implemented");
                        stopSong();
//                        m_metaint=0;                          // has no metadata
//                        m_bitrate=0;
//                        m_icyname=="";
//                        m_f_swm=true;
                    }
                }
                else if(lcml.startsWith("location:")){
                    host=m_metaline.substring(lcml.indexOf("http"),lcml.length()); // use metaline instead lcml
                    if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                    sprintf(chbuf, "redirect to new host %s", host.c_str());
                    if(audio_info) audio_info(chbuf);
                    connecttohost(host);
                }
                else if(lcml.startsWith("icy-br:")){
                    m_bitrate=m_metaline.substring(7).toInt();  // Found bitrate tag, read the bitrate
                    sprintf(chbuf,"%d", m_bitrate);
                    if(audio_bitrate) audio_bitrate(chbuf);
                }
                else if(lcml.startsWith("icy-metaint:")){
                    m_metaint=m_metaline.substring(12).toInt(); // Found metaint tag, read the value
                    if(m_metaint>0) m_f_swm=false;              // Multimediastream
                }
                else if(lcml.startsWith("icy-name:")){
                    m_icyname=m_metaline.substring(9);          // Get station name
                    m_icyname.trim();                           // Remove leading and trailing spaces
                    if(m_icyname!=""){
                        if(audio_showstation) audio_showstation(m_icyname.c_str());
                    }
                }
                else if(lcml.startsWith("content-length:")){
                    //log_i("%s",lcml.c_str());
                    m_contentlength= m_metaline.substring(15).toInt();
                    m_f_webfile=true; // Stream comes from a fileserver
                    sprintf(chbuf, "Contnent-Length: %i", m_contentlength);
                    if(audio_info) audio_info(chbuf);
                }
                else if(lcml.startsWith("transfer-encoding:")){ // Station provides chunked transfer
                    if(m_metaline.endsWith("chunked")){
                        m_f_chunked=true;
                        if(audio_info) audio_info("chunked data transfer");
                        m_chunkcount=0;                         // Expect chunkcount in DATA
                    }
                }
                else if(lcml.startsWith("icy-url:")){
                    m_icyurl=m_metaline.substring(8);           // Get the URL
                    m_icyurl.trim();
                    if(audio_icyurl) audio_icyurl(m_icyurl.c_str());
                }
                else{                                           // all other

                    sprintf(chbuf, "%s", m_metaline.c_str());
                    if(audio_info) audio_info(chbuf);
                }
            }
            m_metaline="";                                      // Reset this line
            if((m_LFcount == 2) && m_ctseen){                   // Some data seen and a double LF?
                if(m_icyname==""){if(audio_showstation) audio_showstation("");} // no icyname available
                if(m_bitrate==0){if(audio_bitrate) audio_bitrate("");} // no bitrate received
                if(m_f_swm==true){ // stream without metadata
                    m_datamode=AUDIO_SWM;                      // Overwrite m_datamode
                    sprintf(chbuf, "Switch to SWM, bitrate is %d, metaint is %d", m_bitrate, m_metaint); // Show bitrate and metaint
                    if(audio_info) audio_info(chbuf);
                    String lasthost=m_lastHost;
                    uint idx=lasthost.indexOf('?');
                    if(idx>0) lasthost=lasthost.substring(0, idx);
                    if(audio_lasthost) audio_lasthost(lasthost.c_str());
                    m_f_swm=false;
                }
                else{
                    m_datamode=AUDIO_DATA;                      // Expecting data now
                    sprintf(chbuf, "Switch to DATA, bitrate is %d, metaint is %d", m_bitrate, m_metaint); // Show bitrate and metaint
                    if(audio_info) audio_info(chbuf);
                    String lasthost=m_lastHost;
                    uint idx=lasthost.indexOf('?');
                    if(idx>0) lasthost=lasthost.substring(0, idx);
                    if(audio_lasthost) audio_lasthost(lasthost.c_str());
                }
                delay(1000);
            }
        }
        else{
            m_metaline+=(char)b;                                // Normal character, put new char in metaline
            m_LFcount=0;                                        // Reset double CRLF detection
        }
        return;
    } // end AUDIO_HEADER
    if(m_datamode == AUDIO_METADATA){                           // Handle next byte of metadata
        if(m_f_firstmetabyte){                                  // First byte of metadata?
            m_f_firstmetabyte=false;                            // Not the first anymore
            m_metacount=b*16+1;                                 // New count for metadata including length byte
            if(m_metacount > 1){
                sprintf(chbuf, "Metacount=%d bytes",            // Most of the time there are zero bytes of metadata
                        m_metacount-1);
                if(audio_info) audio_info(chbuf);
            }
            m_metaline="";                                      // Set to empty
        }
        else{
            m_metaline+=(char)b;                                // Normal character, put new char in metaline
        }
        if(--m_metacount == 0){
            if(m_metaline.length() > 1500){                     // Unlikely metaline length?
                if(audio_info) audio_info("Metadata block to long! Skipping all Metadata from now on.");
                m_metaint=16000;                                // Probably no metadata
                m_metaline="";                                  // Do not waste memory on this
                m_datamode=AUDIO_SWM;                           // expect stream without metadata
            }
            else{
                m_datamode=AUDIO_DATA;                          // Expecting data
            }
            if(m_metaline.length()){                            // Any info present?
                // metaline contains artist and song name.  For example:
                // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
                // Sometimes it is just other info like:
                // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
                // Isolate the StreamTitle, remove leading and trailing quotes if present.
                //log_i("ST %s", m_metaline.c_str());
                if( !m_f_localfile) showstreamtitle(m_metaline.c_str(), true);  // Show artist and title if present in metadata
            }

        }
    }
    if(m_datamode == AUDIO_PLAYLISTINIT)                       // Initialize for receive .m3u file
    {
        // We are going to use metadata to read the lines from the .m3u file
        // Sometimes this will only contain a single line
        f_entry=false;                                          // no entry found yet (asx playlist)
        m_metaline="";                                          // Prepare for new line
        m_LFcount=0;                                            // For detection end of header
        m_datamode=AUDIO_PLAYLISTHEADER;                        // Handle playlist data
        playlistcnt=1;                                          // Reset for compare
        m_totalcount=0;                                         // Reset totalcount
        if(audio_info) audio_info("Read from playlist");
    }
    if(m_datamode == AUDIO_PLAYLISTHEADER){                     // Read header
        if((b>0x7F)||(b=='\r')||(b == '\0')){                   // Ignore unprintable characters, ignore CR, ignore NULL                                                  // Ignore CR
            ;                                                   // Yes, ignore
        }
        else if(b == '\n'){                                     // Linefeed ?
            m_LFcount++;                                        // Count linefeeds
            sprintf(chbuf, "Playlistheader: %s", m_metaline.c_str());  // Show playlistheader
            if(audio_info) audio_info(chbuf);
            lcml=m_metaline;                                    // Use lower case for compare
            lcml.toLowerCase();
            lcml.trim();
            if(lcml.startsWith("location:")){
                host=m_metaline.substring(lcml.indexOf("http"),lcml.length()); // use metaline instead lcml
                if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                sprintf(chbuf, "redirect to new host %s", host.c_str());
                if(audio_info) audio_info(chbuf);
                connecttohost(host);
            }
            m_metaline="";                                      // Ready for next line
            if(m_LFcount == 2)
                    {
                if(audio_info) audio_info("Switch to PLAYLISTDATA");
                m_datamode=AUDIO_PLAYLISTDATA;                 // Expecting data now
                m_t0=millis();
                return;
            }
        }
        else{
            m_metaline+=(char)b;                                // Normal character, put new char in metaline
            m_LFcount=0;                                        // Reset double CRLF detection
        }
    }
    if(m_datamode == AUDIO_PLAYLISTDATA){                       // Read next byte of .m3u file data
        m_t0=millis();
        if((b > 0x7F) ||                                        // Ignore unprintable characters
                (b == '\r') ||                                  // Ignore CR
                (b == '\0'))                                    // Ignore NULL
                { /* Yes, ignore */ }

        else if(b == '\n'){                                     // Linefeed or end of string?
            sprintf(chbuf, "Playlistdata: %s", m_metaline.c_str());  // Show playlistdata
            if(audio_info) audio_info(chbuf);
            if(m_playlist.endsWith("m3u")){
                if(m_metaline.length() < 5) {                   // Skip short lines
                    m_metaline="";                              // Flush line
                    return;}
                if(m_metaline.indexOf("#EXTINF:") >= 0){        // Info?
                    if(m_playlist_num == playlistcnt){          // Info for this entry?
                        inx=m_metaline.indexOf(",");            // Comma in this line?
                        if(inx > 0){
                            // Show artist and title if present in metadata
                            //if(vs1053_showstation) vs1053_showstation(m_metaline.substring(inx + 1).c_str());
                            if(audio_info) audio_info(m_metaline.substring(inx + 1).c_str());
                        }
                    }
                }
                if(m_metaline.startsWith("#")){                 // Commentline?
                    m_metaline="";
                    return;}                                    // Ignore commentlines
                // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
                //if(metaline.indexOf("&")>0)metaline=host.substring(0,metaline.indexOf("&"));
                sprintf(chbuf, "Entry %d in playlist found: %s", playlistcnt, m_metaline.c_str());
                if(audio_info) audio_info(chbuf);
                if(m_metaline.indexOf("&")){
                    m_metaline=m_metaline.substring(0, m_metaline.indexOf("&"));}
                if(m_playlist_num == playlistcnt){
                    inx=m_metaline.indexOf("http://");          // Search for "http://"
                    if(inx >= 0){                               // Does URL contain "http://"?
                        host=m_metaline.substring(inx + 7);}    // Yes, remove it and set host
                    else{
                        host=m_metaline;}                       // Yes, set new host
                    //log_i("connecttohost %s", host.c_str());
                    connecttohost(host);                        // Connect to it
                }
                m_metaline="";
                host=m_playlist;                                // Back to the .m3u host
                playlistcnt++;                                  // Next entry in playlist
            } //m3u
            if(m_playlist.endsWith("pls")){
                if(m_metaline.startsWith("File1")){
                    inx=m_metaline.indexOf("http://");          // Search for "http://"
                    if(inx >= 0){                               // Does URL contain "http://"?
                        m_plsURL=m_metaline.substring(inx + 7); // Yes, remove it
                        if(m_plsURL.indexOf("&")>0)m_plsURL=m_plsURL.substring(0,m_plsURL.indexOf("&")); // remove parameter
                        // Now we have an URL for a .mp3 file or stream in host.

                        m_f_plsFile=true;
                    }
                }
                if(m_metaline.startsWith("Title1")){
                    m_plsStationName=m_metaline.substring(7);
                    if(audio_showstation) audio_showstation(m_plsStationName.c_str());
                    sprintf(chbuf, "StationName: %s", m_plsStationName.c_str());
                    if(audio_info) audio_info(chbuf);
                    m_f_plsTitle=true;
                }
                if(m_metaline.startsWith("Length1")) m_f_plsTitle=true; // if no Title is available
                if((m_f_plsFile==true)&&(m_metaline.length()==0)) m_f_plsTitle=true;
                m_metaline="";
                if(m_f_plsFile && m_f_plsTitle){    //we have both StationName and StationURL
                    m_f_plsFile=false; m_f_plsTitle=false;
                    //log_i("connecttohost %s", m_plsURL.c_str());
                    connecttohost(m_plsURL);        // Connect to it
                }
            }//pls
            if(m_playlist.endsWith("asx")){
                String ml=m_metaline;
                ml.toLowerCase();                               // use lowercase
                if(ml.indexOf("<entry>")>=0) f_entry=true;      // found entry tag (returns -1 if not found)
                if(f_entry){
                    if(ml.indexOf("ref href")>0){
                        inx=ml.indexOf("http://");
                        if(inx>0){
                            m_plsURL=m_metaline.substring(inx + 7); // Yes, remove it
                            if(m_plsURL.indexOf('"')>0)m_plsURL=m_plsURL.substring(0,m_plsURL.indexOf('"')); // remove rest
                            // Now we have an URL for a stream in host.
                            m_f_plsFile=true;
                        }
                    }
                    if(ml.indexOf("<title>")>=0){
                        m_plsStationName=m_metaline.substring(7);
                        if(m_plsURL.indexOf('<')>0)m_plsURL=m_plsURL.substring(0,m_plsURL.indexOf('<')); // remove rest
                        if(audio_showstation) audio_showstation(m_plsStationName.c_str());
                        sprintf(chbuf, "StationName: %s", m_plsStationName.c_str());
                        if(audio_info) audio_info(chbuf);
                        m_f_plsTitle=true;
                    }
                }//entry
                m_metaline="";
                if(m_f_plsFile && m_f_plsTitle){   //we have both StationName and StationURL
                    m_f_plsFile=false; m_f_plsTitle=false;
                    //log_i("connecttohost %s", host.c_str());
                    connecttohost(m_plsURL);        // Connect to it
                }
            }//asx
        }
        else{
            m_metaline+=(char)b;                    // Normal character, add it to metaline
        }
        return;
    }
}
//---------------------------------------------------------------------------------------------------------------------

void Audio::showstreamtitle(const char *ml, bool full){
    // example for ml:
    // StreamTitle='Oliver Frank - Mega Hitmix';StreamUrl='www.radio-welle-woerthersee.at';
    // or adw_ad='true';durationMilliseconds='10135';adId='34254';insertionType='preroll';

    int16_t pos1=0, pos2=0, pos3=0, pos4=0;
    String mline=ml, st="", su="", ad="", artist="", title="", icyurl="";
    //log_i("%s",mline.c_str());
    pos1=mline.indexOf("StreamTitle=");
    if(pos1!=-1){                                       // StreamTitle found
        pos1=pos1+12;
        st=mline.substring(pos1);                       // remove "StreamTitle="
//      log_i("st_orig %s", st.c_str());
        if(st.startsWith("'{")){
            // special coding like '{"t":"\u041f\u0438\u043a\u043d\u0438\u043a - \u0418...."m":"mdb","lAU":0,"lAuU":18}
            pos2= st.indexOf('"', 8);                   // end of '{"t":".......", seek for double quote at pos 8
            st=st.substring(0, pos2);
            pos2= st.lastIndexOf('"');
            st=st.substring(pos2+1);                    // remove '{"t":"
            pos2=0;
            String uni="";
            String st1="";
            uint16_t u=0;
            uint8_t v=0, w=0;
            for(int i=0; i<st.length(); i++){
                if(pos2>1) pos2++;
                if((st[i]=='\\')&&(pos2==0)) pos2=1;    // backslash found
                if((st[i]=='u' )&&(pos2==1)) pos2=2;    // "\u" found
                if(pos2>2) uni=uni+st[i];               // next 4 values are unicode
                if(pos2==0) st1+=st[i];                 // normal character
                if(pos2>5){
                    pos2=0;
                    u=strtol(uni.c_str(), 0, 16);       // convert hex to int
                    v=u/64 + 0xC0; st1+=char(v);        // compute UTF-8
                    w=u%64 + 0x80; st1+=char(w);
                     //log_i("uni %i  %i", v, w );
                    uni="";
                }
            }
            log_i("st1 %s", st1.c_str());
            st=st1;
        }
        else{
            // normal coding
            if(st.indexOf('&')!=-1){                // maybe html coded
                st.replace("&Auml;", "Ä" ); st.replace("&auml;", "ä"); //HTML -> ASCII
                st.replace("&Ouml;", "Ö" ); st.replace("&ouml;", "o");
                st.replace("&Uuml;", "Ü" ); st.replace("&uuml;", "ü");
                st.replace("&szlig;","ß" ); st.replace("&amp;",  "&");
                st.replace("&quot;", "\""); st.replace("&lt;",   "<");
                st.replace("&gt;",   ">" ); st.replace("&apos;", "'");
            }
            pos2= st.indexOf(';',1);                // end of StreamTitle, first occurrence of ';'
            if(pos2!=-1) st=st.substring(0,pos2);   // extract StreamTitle
            if(st.startsWith("'")) st=st.substring(1,st.length()-1); // if exists remove ' at the begin and end
            pos3=st.lastIndexOf(" - ");             // separator artist - title
            if(pos3!=-1){                           // found separator? yes
                artist=st.substring(0,pos3);        // artist not used yet
                title=st.substring(pos3+3);         // title not used yet
            }
        }

        if(m_st_remember!=st){ // show only changes
            if(audio_showstreamtitle) audio_showstreamtitle(st.c_str());
        }

        m_st_remember=st;
        st="StreamTitle=" + st;
        if(audio_info) audio_info(st.c_str());
    }
    pos4=mline.indexOf("StreamUrl=");
    if(pos4!=-1){                               // StreamUrl found
        pos4=pos4+10;
        su=mline.substring(pos4);               // remove "StreamUrl="
        pos2= su.indexOf(';',1);                // end of StreamUrl, first occurrence of ';'
        if(pos2!=-1) su=su.substring(0,pos2);   // extract StreamUrl
        if(su.startsWith("'")) su=su.substring(1,su.length()-1); // if exists remove ' at the begin and end
        su="StreamUrl=" + su;
        if(audio_info) audio_info(su.c_str());
    }
    pos2=mline.indexOf("adw_ad=");              // advertising,
    if(pos2!=-1){
       ad=mline.substring(pos2);
       if(audio_info) audio_info(ad.c_str());
       pos2=mline.indexOf("durationMilliseconds=");
       if(pos2!=-1){
          pos2+=22;
          mline=mline.substring(pos2);
          mline=mline.substring(0, mline.indexOf("'")-3); // extract duration in seconds
          if(audio_commercial) audio_commercial(mline.c_str());
       }
    }
    if(!full){
        m_icystreamtitle="";                    // Unknown type
        return;                                 // Do not show
    }
    if(pos1==-1 && pos4==-1){
        // Info probably from playlist
        st=mline;
        if(audio_showstreamtitle) audio_showstreamtitle(st.c_str());
        st="Streamtitle: " + st;
        if(audio_info) audio_info(chbuf);
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::chkhdrline(const char* str){
    char b;                                            // Byte examined
    int len=0;                                         // Length of the string

    while((b= *str++)){                                // Search to end of string
        len++;                                         // Update string length
        if( !isalpha(b)){                              // Alpha (a-z, A-Z)
            if(b != '-'){                              // Minus sign is allowed
                if((b == ':') || (b == ';')){          // Found a colon or semicolon?
                    return ((len > 5) && (len < 200)); // Yes, okay if length is okay
                }
                else{
                    return false;                      // Not a legal character
                }
            }
        }
    }
    return false;                                      // End of string without colon
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::sendBytes(uint8_t *data, size_t len) {

    static int lastret=0, count=0, swnf=0;
    int nextSync=0;
    if(!m_f_playing){
        if(m_f_mp3) nextSync = MP3FindSyncWord(data, len);
        if(m_f_aac) nextSync = AACFindSyncWord(data, len);
        if(nextSync==-1) {
            swnf++; // syncword not found counter, can be multimediadata
            if(audio_info && swnf<1) audio_info("syncword not found");
            return -1;
        }
        else{
            if(audio_info && swnf>0){
                sprintf(chbuf, "syncword not found %i times", swnf);
                audio_info(chbuf);
            }
            swnf=0;
        }
        if(nextSync > 0){
            sprintf(chbuf, "syncword found at pos %i", nextSync);
            if(audio_info) audio_info(chbuf);
            return nextSync;
        }
        if(audio_info) audio_info("syncword found at pos 0");
        count=0;
        m_lastRate=0;
        m_lastChannels=0;
        m_bps=0;
        m_bitrate=0;
        m_f_playing=true;
        return nextSync;
    }

    m_bytesLeft=len;
    int ret=0;
    if(m_f_mp3) ret = MP3Decode(data, &m_bytesLeft, m_outBuff, 0);
    if(m_f_aac) ret = AACDecode(data, &m_bytesLeft, m_outBuff);
    if(ret==0) lastret=0;
    int bytesDecoded=len-m_bytesLeft;
    if(bytesDecoded==0){ // unlikely framesize
        if(audio_info) audio_info("mp3 framesize is 0, start decoding again");
        m_f_playing=false; // seek for new syncword
        
        // we're here because there was a wrong sync word
        // so skip two sync bytes and seek for next
        return 2; 
    }
    if(ret){ // Error, skip the frame...
        m_f_playing=false; // seek for new syncword
        if(count==0){
            i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
            String e = "";
            if(m_f_mp3){
                switch(ret){
                    case ERR_MP3_NONE:                 e="NONE";                  break;
                    case ERR_MP3_INDATA_UNDERFLOW:     e="INDATA_UNDERFLOW";      break;
                    case ERR_MP3_MAINDATA_UNDERFLOW:   e="MAINDATA_UNDERFLOW";    break;
                    case ERR_MP3_FREE_BITRATE_SYNC:    e="FREE_BITRATE_SYNC";     break;
                    case ERR_MP3_OUT_OF_MEMORY:        e="OUT_OF_MEMORY";         break;
                    case ERR_MP3_NULL_POINTER:         e="NULL_POINTER";          break;
                    case ERR_MP3_INVALID_FRAMEHEADER:  e="INVALID_FRAMEHEADER";   break;
                    case ERR_MP3_INVALID_SIDEINFO:     e="INVALID_SIDEINFO";      break;
                    case ERR_MP3_INVALID_SCALEFACT:    e="INVALID_SCALEFACT";     break;
                    case ERR_MP3_INVALID_HUFFCODES:    e="INVALID_HUFFCODES";     break;
                    case ERR_MP3_INVALID_DEQUANTIZE:   e="INVALID_DEQUANTIZE";    break;
                    case ERR_MP3_INVALID_IMDCT:        e="INVALID_IMDCT";         break;
                    case ERR_MP3_INVALID_SUBBAND:      e="INVALID_SUBBAND";       break;
                    default: e="ERR_UNKNOWN";
                }
                sprintf(chbuf, "MP3 decode error %d : %s", ret, e.c_str());
                if(audio_info) audio_info(chbuf);
            }
            if(m_f_aac){
                switch(ret){
                    case ERR_AAC_NONE:                  e="NONE";                 break;
                    case ERR_AAC_INDATA_UNDERFLOW:      e="INDATA_UNDERFLOW";     break;
                    case ERR_AAC_NULL_POINTER:          e="NULL_POINTER";         break;
                    case ERR_AAC_INVALID_ADTS_HEADER:   e="INVALID_ADTS_HEADER";  break;
                    case ERR_AAC_INVALID_ADIF_HEADER:   e="INVALID_ADIF_HEADER";  break;
                    case ERR_AAC_INVALID_FRAME:         e="INVALID_FRAME";        break;
                    case ERR_AAC_MPEG4_UNSUPPORTED:     e="MPEG4_UNSUPPORTED";    break;
                    case ERR_AAC_CHANNEL_MAP:           e="CHANNEL_MAP";          break;
                    case ERR_AAC_SYNTAX_ELEMENT:        e="SYNTAX_ELEMENT";       break;
                    case ERR_AAC_DEQUANT:               e="DEQUANT";              break;
                    case ERR_AAC_STEREO_PROCESS:        e="STEREO_PROCESS";       break;
                    case ERR_AAC_PNS:                   e="PNS";                  break;
                    case ERR_AAC_SHORT_BLOCK_DEINT:     e="SHORT_BLOCK_DEINT";    break;
                    case ERR_AAC_TNS:                   e="TNS";                  break;
                    case ERR_AAC_IMDCT:                 e="IMDCT";                break;
                    case ERR_AAC_NCHANS_TOO_HIGH:       e="NCHANS_TOO_HIGH";      break;
                    case ERR_AAC_RAWBLOCK_PARAMS:       e="RAWBLOCK_PARAMS";      break;
                    default: e="ERR_UNKNOWN";
                }
                sprintf(chbuf, "AAC decode error %d : %s", ret, e.c_str());
                if(audio_info) audio_info(chbuf);
            }
        }
        if(lastret==ret) count++;
        if(count>10){ count=0; lastret=0;} // more than 10 errors
        if(ret!=lastret){
            lastret=ret; count=0;
        }
        return bytesDecoded;
    }
    else{  // ret==0
        if(m_f_mp3){
            if (MP3GetChannels() != m_lastChannels) {
                setChannels(MP3GetChannels());
                m_lastChannels = MP3GetChannels() ;
                sprintf(chbuf,"Channels=%i", MP3GetChannels() );
                if(audio_info) audio_info(chbuf);
            }
            if (MP3GetSampRate() != (int) m_lastRate) {
                m_lastRate = MP3GetSampRate();
                setSampleRate(MP3GetSampRate());
                sprintf(chbuf,"SampleRate=%i",MP3GetSampRate());
                if(audio_info) audio_info(chbuf);
            }
            if((int) MP3GetSampRate() ==0){ // error can't be
                if(audio_info) audio_info("SampleRate=0, try new frame");
                m_f_playing=false;
                return 1;
            }
            if (MP3GetBitsPerSample() != (int)m_bps){
                m_bps=MP3GetBitsPerSample();
                sprintf(chbuf,"BitsPerSample=%i", MP3GetBitsPerSample());
                if(audio_info) audio_info(chbuf);
            }
            if (MP3GetBitrate()!= m_bitrate){
                if(m_bitrate==0){ // no bitrate received from icy stream
                    sprintf(chbuf,"Bitrate=%i", MP3GetBitrate()); // show only the first rate
                    if(audio_info) audio_info(chbuf);
                    if(m_contentlength >0 && MP3GetBitrate()>0){
                        uint16_t duration=m_contentlength*8/MP3GetBitrate();
                        sprintf(chbuf,"Duration=%is", duration);
                        if(audio_info) audio_info(chbuf);
                    }
                }
                m_bitrate=MP3GetBitrate();
            }
            m_validSamples = MP3GetOutputSamps() / m_lastChannels;
        }
        if(m_f_aac){
            if (AACGetChannels() != m_lastChannels) {
                setChannels(AACGetChannels());
                m_lastChannels = AACGetChannels();
                sprintf(chbuf,"AAC Channels=%i",AACGetChannels());
                if(audio_info) audio_info(chbuf);
            }
            if (AACGetSampRate() != m_lastRate) {
                setSampleRate(AACGetSampRate());
                m_lastRate = AACGetSampRate();
                sprintf(chbuf,"AAC SampleRate=%i",AACGetSampRate()*2);
                if(audio_info) audio_info(chbuf);
            }
            if(AACGetSampRate() == 0){ // error can't be
                if(audio_info) audio_info("AAC SampleRate=0, try new frame");
                m_f_playing=false;
                return 1;
            }
            if (AACGetBitsPerSample() != m_bps){
                m_bps=AACGetBitsPerSample();
                sprintf(chbuf,"AAC BitsPerSample=%i", AACGetBitsPerSample());
                if(audio_info) audio_info(chbuf);
            }
            if (AACGetBitrate() != m_bitrate){
                    sprintf(chbuf,"AAC Bitrate=%i", AACGetBitrate()); // show only the first rate
                    if(audio_info) audio_info(chbuf);
                m_bitrate=AACGetBitrate();
            }
            m_validSamples = AACGetOutputSamps() / m_lastChannels;
        }
    }
    static uint16_t bitrate_counter=0;
    static int      old_bitrate = 0;
    static uint32_t sum_bitrate = 0;
    static boolean  f_firstFrame = true;
    static boolean  f_CBR = true;

    if(m_bitrate>0){
        if(m_avr_bitrate==0){
            bitrate_counter = 0;
            old_bitrate = 0;
            sum_bitrate = 0;
            f_firstFrame = true;
            f_CBR = true;
        }
        if(old_bitrate != m_bitrate){
            if(!f_firstFrame && bitrate_counter == 0){
                if(audio_info) audio_info("VBR recognized, audioFileDuration is estimated");
                f_CBR = false;
            }
        }
        old_bitrate = m_bitrate;
        if(bitrate_counter < 100 && !f_CBR){
            bitrate_counter++;
            sum_bitrate += m_bitrate;
            m_avr_bitrate = sum_bitrate /  bitrate_counter;
        }
        if(f_firstFrame){
            m_avr_bitrate = m_bitrate; // if CBR set m_avr_bitrate only once
        }
        f_firstFrame = false;
        m_audioCurrentTime += (float) bytesDecoded*8/m_bitrate;
    }
    while(m_validSamples) {
        playChunk();
    }
    return bytesDecoded;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t DIN){
    m_BCLK=BCLK;            // Bit Clock
    m_LRC=LRC;              // Left/Right Clock
    m_DOUT=DOUT;            // Data Out
    m_DIN=DIN;              // Data In

    i2s_pin_config_t pins = {
      .bck_io_num = m_BCLK,
      .ws_io_num =  m_LRC,              //  wclk,
      .data_out_num = m_DOUT,
      .data_in_num = m_DIN
    };
    i2s_set_pin((i2s_port_t)m_i2s_num, &pins);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFileSize(){
    if (!audiofile) return 0;
    return audiofile.size();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFilePos(){
    if (!audiofile) return 0;
    return audiofile.position();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioFileDuration(){
    if(m_f_localfile){
        if (!audiofile) return 0;

        if ( 0 == m_audioFileDuration ) // calculate only once per file
        {
            uint32_t fileSize = getFileSize();
            uint32_t bitRate = m_avr_bitrate; //MP3GetBitrate();
            if(0 < bitRate)
            {
                m_audioFileDuration = 8 * (fileSize - m_id3Size) / bitRate;
            }
        }
        return m_audioFileDuration;
    }
    if(m_f_webfile){
        if(m_contentlength >0 &&  m_avr_bitrate > 0){
            return ((m_contentlength-m_bytesNotDecoded) * 8 / m_avr_bitrate);
        }
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
    // return current time in seconds
uint32_t Audio::getAudioCurrentTime(){
    return (uint32_t) m_audioCurrentTime;
}

//---------------------------------------------------------------------------------------------------------------------
bool Audio::setFilePos(uint32_t pos){
    if (!audiofile) return false;
    return audiofile.seek(pos);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::audioFileSeek(const int8_t speed)
{
    bool retVal = false;
    if ( audiofile &&  speed )
    {
        retVal = true; //    
        int32_t steps = 20 * speed;

        if ( ( steps < 0 ) && m_f_running ) 
        {
            // fast-rewind faster than fast-forward because we still playing
            steps = steps * 2;
        }

        uint32_t newPos = audiofile.position() + steps * MP3GetBitsPerSample();
        newPos = ( newPos < m_id3Size) ? m_id3Size : newPos;
        newPos = ( newPos >= audiofile.size() ) ? audiofile.size() - 1 : newPos;
        if ( audiofile.position() != newPos )
        {
            retVal = audiofile.seek(newPos);
        }
    }
    return retVal;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setSampleRate(int freq) {
    i2s_set_sample_rates((i2s_port_t)m_i2s_num, freq);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setBitsPerSample(int bits) {
    if ( (bits != 16) && (bits != 8) ) return false;
    m_bps = bits;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setChannels(int ch) {
    if ( (ch < 1) || (ch > 2) ) return false;
    m_channels = ch;
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::playSample(int16_t sample[2]) {
    if (m_bps == 8) {
      // Upsample from unsigned 8 bits to signed 16 bits
      sample[LEFTCHANNEL] = (((int16_t)(sample[LEFTCHANNEL]&0xff)) - 128) << 8;
      sample[RIGHTCHANNEL] = (((int16_t)(sample[RIGHTCHANNEL]&0xff)) - 128) << 8;
    }
    uint32_t s32;
    s32 = ((Gain(sample[RIGHTCHANNEL]))<<16) | (Gain(sample[LEFTCHANNEL]) & 0xffff); // volume
    esp_err_t err=i2s_write((i2s_port_t)m_i2s_num, (const char*)&s32, sizeof(uint32_t), &m_bytesWritten, 1000);
    if(err!=ESP_OK){
        log_e("ESP32 Errorcode %i", err);
        return false;
    }
    if(m_bytesWritten<4){
        log_e("Can't stuff any more in I2S..."); // increase waitingtime or outputbuffer
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setVolume(uint8_t vol){ // vol 22 steps, 0...21
    if(vol>21) vol=21;
    m_vol=volumetable[vol];
}
//---------------------------------------------------------------------------------------------------------------------
uint8_t Audio::getVolume(){
    for(uint8_t i = 0; i < 22; i++){
      if(volumetable[i] == m_vol) return i;
    }
    m_vol=12; // if m_vol not found in table
    return m_vol;
}
//---------------------------------------------------------------------------------------------------------------------
int16_t Audio::Gain(int16_t s) {
    int32_t v;
    v= (s * m_vol)>>6;
    return (int16_t)(v&0xffff);
}
//---------------------------------------------------------------------------------------------------------------------

