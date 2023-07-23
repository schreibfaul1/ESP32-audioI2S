/*
 * flac_decoder.cpp
 * Java source code from https://www.nayuki.io/page/simple-flac-implementation
 * adapted to ESP32
 *
 * Created on: Jul 03,2020
 * Updated on: Jul 23,2023
 *
 * Author: Wolle
 *
 *
 */
#include "flac_decoder.h"
#include "vector"
using namespace std;


FLACFrameHeader_t   *FLACFrameHeader;
FLACMetadataBlock_t *FLACMetadataBlock;
FLACsubFramesBuff_t *FLACsubFramesBuff;

vector<int32_t> coefs;
const uint16_t  outBuffSize = 2048;
uint16_t        m_blockSize = 0;
uint16_t        m_blockSizeLeft = 0;
uint16_t        m_validSamples = 0;
uint8_t         m_status = 0;
uint8_t        *m_inptr;
uint16_t       *s_flacSegmentTable = NULL;
float           m_compressionRatio = 0;
uint32_t        m_bitrate = 0;
uint16_t        m_rIndex = 0;
uint64_t        m_bitBuffer = 0;
uint8_t         m_bitBufferLen = 0;
bool            s_f_flacParseOgg = false;
uint8_t         m_flacPageSegments = 0;
uint8_t         m_page0_len = 0;
char           *m_streamTitle = NULL;
boolean         s_f_newSt = false;

//----------------------------------------------------------------------------------------------------------------------
//          FLAC INI SECTION
//----------------------------------------------------------------------------------------------------------------------

// prefer PSRAM
#define __malloc_heap_psram(size) \
    heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT|MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT|MALLOC_CAP_INTERNAL)

bool FLACDecoder_AllocateBuffers(void){

    if(!FLACFrameHeader)    {FLACFrameHeader    = (FLACFrameHeader_t*)    __malloc_heap_psram(sizeof(FLACFrameHeader_t));}
    if(!FLACMetadataBlock)  {FLACMetadataBlock  = (FLACMetadataBlock_t*)  __malloc_heap_psram(sizeof(FLACMetadataBlock_t));}
    if(!FLACsubFramesBuff)  {FLACsubFramesBuff  = (FLACsubFramesBuff_t*)  __malloc_heap_psram(sizeof(FLACsubFramesBuff_t));}
    if(!m_streamTitle)      {m_streamTitle      = (char*)                 __malloc_heap_psram(256);}
    if(!s_flacSegmentTable) {s_flacSegmentTable = (uint16_t*)             __malloc_heap_psram(256 * sizeof(uint16_t));}

    if(!FLACFrameHeader || !FLACMetadataBlock || !FLACsubFramesBuff || !m_streamTitle || !s_flacSegmentTable){
        log_e("not enough memory to allocate flacdecoder buffers");
        return false;
    }
    FLACDecoder_ClearBuffer();
    return true;
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoder_ClearBuffer(){
    memset(FLACFrameHeader,   0, sizeof(FLACFrameHeader_t));
    memset(FLACMetadataBlock, 0, sizeof(FLACMetadataBlock_t));
    memset(FLACsubFramesBuff, 0, sizeof(FLACsubFramesBuff_t));
    m_status = DECODE_FRAME;
    return;
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoder_FreeBuffers(){
    if(FLACFrameHeader)    {free(FLACFrameHeader);    FLACFrameHeader    = NULL;}
    if(FLACMetadataBlock)  {free(FLACMetadataBlock);  FLACMetadataBlock  = NULL;}
    if(FLACsubFramesBuff)  {free(FLACsubFramesBuff);  FLACsubFramesBuff  = NULL;}
    if(m_streamTitle)      {free(m_streamTitle);      m_streamTitle      = NULL;}
    if(s_flacSegmentTable) {free(s_flacSegmentTable); s_flacSegmentTable = NULL;}
}
//----------------------------------------------------------------------------------------------------------------------
//            B I T R E A D E R
//----------------------------------------------------------------------------------------------------------------------
const uint32_t mask[] = {0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f,
                         0x0000007f, 0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff,
                         0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
                         0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
                         0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

uint32_t readUint(uint8_t nBits, int *bytesLeft){
    while (m_bitBufferLen < nBits){
        uint8_t temp = *(m_inptr + m_rIndex);
        m_rIndex++;
        (*bytesLeft)--;
        if(*bytesLeft < 0) { log_i("error in bitreader"); }
        m_bitBuffer = (m_bitBuffer << 8) | temp;
        m_bitBufferLen += 8;
    }
    m_bitBufferLen -= nBits;
    uint32_t result = m_bitBuffer >> m_bitBufferLen;
    if (nBits < 32)
        result &= mask[nBits];
    return result;
}

int32_t readSignedInt(int nBits, int* bytesLeft){
    int32_t temp = readUint(nBits, bytesLeft) << (32 - nBits);
    temp = temp >> (32 - nBits); // The C++ compiler uses the sign bit to fill vacated bit positions
    return temp;
}

int64_t readRiceSignedInt(uint8_t param, int* bytesLeft){
    long val = 0;
    while (readUint(1, bytesLeft) == 0)
        val++;
    val = (val << param) | readUint(param, bytesLeft);
    return (val >> 1) ^ -(val & 1);
}

void alignToByte() {
    m_bitBufferLen -= m_bitBufferLen % 8;
}
//----------------------------------------------------------------------------------------------------------------------
//              F L A C - D E C O D E R
//----------------------------------------------------------------------------------------------------------------------
void FLACSetRawBlockParams(uint8_t Chans, uint32_t SampRate, uint8_t BPS, uint32_t tsis, uint32_t AuDaLength){
    FLACMetadataBlock->numChannels = Chans;
    FLACMetadataBlock->sampleRate = SampRate;
    FLACMetadataBlock->bitsPerSample = BPS;
    FLACMetadataBlock->totalSamples = tsis;  // total samples in stream
    FLACMetadataBlock->audioDataLength = AuDaLength;
}
//----------------------------------------------------------------------------------------------------------------------
void FLACDecoderReset(){ // set var to default
    m_status = DECODE_FRAME;
    m_bitBuffer = 0;
    m_bitBufferLen = 0;
}
//----------------------------------------------------------------------------------------------------------------------
int FLACFindSyncWord(unsigned char *buf, int nBytes) {
    int i;
    i = FLAC_specialIndexOf(buf, "OggS", nBytes);
    if(i == 0){
        // flag has ogg wrapper
        return 0;
    }
     /* find byte-aligned sync code - need 14 matching bits */
    for (i = 0; i < nBytes - 1; i++) {
        if ((buf[i + 0] & 0xFF) == 0xFF  && (buf[i + 1] & 0xFC) == 0xF8) { // <14> Sync code '11111111111110xx'
            FLACDecoderReset();
            return i;
        }
    }
    return -1;
}
//----------------------------------------------------------------------------------------------------------------------
boolean FLACFindMagicWord(unsigned char* buf, int nBytes){
    int idx = FLAC_specialIndexOf(buf, "fLaC", nBytes);
    if(idx >0){ // Metadatablock follows
        idx += 4;
        boolean lmdbf = ((buf[idx + 1] & 0x80) == 0x80); // Last-metadata-block flag
        uint8_t bt = (buf[idx + 1] & 0x7F); // block type
        uint32_t lomd = (buf[idx + 2] << 16) + (buf[idx + 3] << 8) + buf[idx + 4]; // Length of metadata to follow

        // TODO - parse metadata block data
        (void)lmdbf; (void)bt; (void)lomd;
        // log_i("Last-metadata-block flag: %d", lmdbf);
        // log_i("block type: %d", bt);
        // log_i("Length (in bytes) of metadata to follow: %d", lomd);
        return true;
    }
    return false;
}
//----------------------------------------------------------------------------------------------------------------------
char* FLACgetStreamTitle(){
    if(s_f_newSt){
        s_f_newSt = false;
        return m_streamTitle;
    }
    return NULL;
}
//----------------------------------------------------------------------------------------------------------------------
int FLACparseOGG(uint8_t *inbuf, int *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    s_f_flacParseOgg = false;
    int idx = FLAC_specialIndexOf(inbuf, "OggS", 6);
    if(idx != 0) return ERR_FLAC_DECODER_ASYNC;

    uint8_t  version            = *(inbuf +  4); (void) version;
    uint8_t  headerType         = *(inbuf +  5); (void) headerType;
    uint64_t granulePosition    = (uint64_t)*(inbuf + 13) << 56;  // granule_position: an 8 Byte field containing -
             granulePosition   += (uint64_t)*(inbuf + 12) << 48;  // position information. For an audio stream, it MAY
             granulePosition   += (uint64_t)*(inbuf + 11) << 40;  // contain the total number of PCM samples encoded
             granulePosition   += (uint64_t)*(inbuf + 10) << 32;  // after including all frames finished on this page.
             granulePosition   += *(inbuf +  9) << 24;  // This is a hint for the decoder and gives it some timing
             granulePosition   += *(inbuf +  8) << 16;  // and position information. A special value of -1 (in two's
             granulePosition   += *(inbuf +  7) << 8;   // complement) indicates that no packets finish on this page.
             granulePosition   += *(inbuf +  6); (void) granulePosition;
    uint32_t bitstreamSerialNr  = *(inbuf + 17) << 24;  // bitstream_serial_number: a 4 Byte field containing the
             bitstreamSerialNr += *(inbuf + 16) << 16;  // unique serial number by which the logical bitstream
             bitstreamSerialNr += *(inbuf + 15) << 8;   // is identified.
             bitstreamSerialNr += *(inbuf + 14); (void) bitstreamSerialNr;
    uint32_t pageSequenceNr     = *(inbuf + 21) << 24;  // page_sequence_number: a 4 Byte field containing the sequence
             pageSequenceNr    += *(inbuf + 20) << 16;  // number of the page so the decoder can identify page loss
             pageSequenceNr    += *(inbuf + 19) << 8;   // This sequence number is increasing on each logical bitstream
             pageSequenceNr    += *(inbuf + 18); (void) pageSequenceNr;
    uint32_t CRCchecksum        = *(inbuf + 25) << 24;
             CRCchecksum       += *(inbuf + 24) << 16;
             CRCchecksum       += *(inbuf + 23) << 8;
             CRCchecksum       += *(inbuf + 22); (void) CRCchecksum;
    uint8_t  pageSegments       = *(inbuf + 26);        // giving the number of segment entries

    // read the segment table (contains pageSegments bytes),  1...251: Length of the frame in bytes,
    // 255: A second byte is needed.  The total length is first_byte + second byte

    int16_t segmentTableWrPtr = 0;

    for(int i = 0; i < pageSegments; i++){
        int n = *(inbuf + 27 + i);
        while(*(inbuf + 27 + i) == 255){
            i++;
            n+= *(inbuf + 27 + i);
        }
        s_flacSegmentTable[segmentTableWrPtr] = n;
        segmentTableWrPtr++;
    //    s_flacSegmentLength += n;
    }
    m_page0_len = s_flacSegmentTable[0];

    // for(int i = 0; i<pageSegments; i++){
    //     log_i("%i %i", i, s_flacSegmentTable[i]);
    // }


    bool     continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    bool     firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    bool     lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)
    static   uint8_t secondPage = 0; (void)continuedPage; (void)lastPage;

    if(firstPage) secondPage = 3;
    if(secondPage) secondPage--;

    uint16_t headerSize = 0;
    uint8_t aLen = 0, tLen = 0;
    uint8_t *aPos = NULL, *tPos = NULL;
    if(firstPage || secondPage == 1){
        // log_i("s_flacSegmentTable[0] %i", s_flacSegmentTable[0]);
        headerSize = pageSegments + s_flacSegmentTable[0] +27;
        idx = FLAC_specialIndexOf(inbuf + 28, "ARTIST", s_flacSegmentTable[0]);
        if(idx > 0){
            aPos = inbuf + 28 + idx + 7;
            aLen = *(inbuf + 28 +idx -4) -  7;
        }
        idx = FLAC_specialIndexOf(inbuf + 28, "TITLE", s_flacSegmentTable[0]);
        if(idx > 0){
            tPos = inbuf + 28 + idx + 6;
            tLen = *(inbuf + 28 + idx -4) - 6;
        }
        int pos = 0;
        if(aLen) {memcpy(m_streamTitle, aPos, aLen); m_streamTitle[aLen] = '\0'; pos = aLen;}
        if(aLen && tLen) {strcat(m_streamTitle, " - "); pos += 3;}
        if(tLen) {memcpy(m_streamTitle + pos, tPos, tLen); m_streamTitle[pos + tLen] = '\0';}
        if(tLen || aLen) s_f_newSt = true;
    }
    else{
        headerSize = pageSegments + 27;
    }
    *bytesLeft -= headerSize;
    return ERR_FLAC_NONE; // no error
}
//----------------------------------------------------------------------------------------------------------------------
int8_t FLACDecode(uint8_t *inbuf, int *bytesLeft, short *outbuf){ //  MAIN LOOP

    if(s_f_flacParseOgg == true){
        int ret = FLACparseOGG(inbuf, bytesLeft);
        if(ret == ERR_FLAC_NONE) return FLAC_PARSE_OGG_DONE; // ok
        else return ret;  // error
    }

    if ((inbuf[0] == 'O') && (inbuf[1] == 'g') && (inbuf[2] == 'g') && (inbuf[3] == 'S')){
        s_f_flacParseOgg = true;
        return FLAC_PARSE_OGG_DONE;
    }
    int ret = FLACDecodeNative(inbuf, bytesLeft, outbuf);
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t FLACDecodeNative(uint8_t *inbuf, int *bytesLeft, short *outbuf){

    int bl = *bytesLeft;
    static int sbl = 0;

    if(m_status != OUT_SAMPLES){
        m_rIndex = 0;
        m_inptr = inbuf;
    }

    while(m_status == DECODE_FRAME){// Read a ton of header fields, and ignore most of them
        int ret = flacDecodeFrame (inbuf, bytesLeft);
        if(ret != 0) return ret;
        if(*bytesLeft < MAX_BLOCKSIZE) return FLAC_DECODE_FRAMES_LOOP; // need more data
    }

    if(m_status == DECODE_SUBFRAMES){

        // Decode each channel's subframe, then skip footer
        int ret = decodeSubframes(bytesLeft);
        sbl = bl - *bytesLeft;
        if(ret != 0) return ret;
        m_status = OUT_SAMPLES;
    }

    if(m_status == OUT_SAMPLES){  // Write the decoded samples
        // blocksize can be much greater than outbuff, so we can't stuff all in once
        // therefore we need often more than one loop (split outputblock into pieces)
        uint16_t blockSize;
        static uint16_t offset = 0;
        if(m_blockSize < outBuffSize + offset) blockSize = m_blockSize - offset;
        else blockSize = outBuffSize;

        for (int i = 0; i < blockSize; i++) {
            for (int j = 0; j < FLACMetadataBlock->numChannels; j++) {
                int val = FLACsubFramesBuff->samplesBuffer[j][i + offset];
                if (FLACMetadataBlock->bitsPerSample == 8) val += 128;
                outbuf[2*i+j] = val;
            }
        }

        m_validSamples = blockSize * FLACMetadataBlock->numChannels;
        offset += blockSize;
        m_compressionRatio = (float)sbl / (m_validSamples * FLACMetadataBlock->numChannels);
        m_bitrate = FLACMetadataBlock->sampleRate * FLACMetadataBlock->bitsPerSample * FLACMetadataBlock->numChannels;
        m_bitrate /= m_compressionRatio;

        if(offset != m_blockSize) return GIVE_NEXT_LOOP;
        offset = 0;
        if(offset > m_blockSize) { log_e("offset has a wrong value"); }
    }

    alignToByte();
    readUint(16, bytesLeft);

//    m_compressionRatio = (float)m_bytesDecoded / (float)m_blockSize * FLACMetadataBlock->numChannels * (16/8);
//    log_i("m_compressionRatio % f", m_compressionRatio);
    m_status = DECODE_FRAME;
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t flacDecodeFrame(uint8_t *inbuf, int *bytesLeft){
    readUint(14 + 1, bytesLeft); // synccode + reserved bit
    FLACFrameHeader->blockingStrategy = readUint(1, bytesLeft);
    FLACFrameHeader->blockSizeCode = readUint(4, bytesLeft);
    FLACFrameHeader->sampleRateCode = readUint(4, bytesLeft);
    FLACFrameHeader->chanAsgn = readUint(4, bytesLeft);
    FLACFrameHeader->sampleSizeCode = readUint(3, bytesLeft);
    if(!FLACMetadataBlock->numChannels){
        if(FLACFrameHeader->chanAsgn == 0) FLACMetadataBlock->numChannels = 1;
        if(FLACFrameHeader->chanAsgn == 1) FLACMetadataBlock->numChannels = 2;
        if(FLACFrameHeader->chanAsgn > 7)  FLACMetadataBlock->numChannels = 2;
    }
    if(FLACMetadataBlock->numChannels < 1) return ERR_FLAC_UNKNOWN_CHANNEL_ASSIGNMENT;
    if(!FLACMetadataBlock->bitsPerSample){
        if(FLACFrameHeader->sampleSizeCode == 1) FLACMetadataBlock->bitsPerSample =  8;
        if(FLACFrameHeader->sampleSizeCode == 2) FLACMetadataBlock->bitsPerSample = 12;
        if(FLACFrameHeader->sampleSizeCode == 4) FLACMetadataBlock->bitsPerSample = 16;
        if(FLACFrameHeader->sampleSizeCode == 5) FLACMetadataBlock->bitsPerSample = 20;
        if(FLACFrameHeader->sampleSizeCode == 6) FLACMetadataBlock->bitsPerSample = 24;
    }
    if(FLACMetadataBlock->bitsPerSample > 16) return ERR_FLAC_BITS_PER_SAMPLE_TOO_BIG;
    if(FLACMetadataBlock->bitsPerSample < 8 ) return ERR_FLAG_BITS_PER_SAMPLE_UNKNOWN;
    if(!FLACMetadataBlock->sampleRate){
        if(FLACFrameHeader->sampleRateCode == 1)  FLACMetadataBlock->sampleRate =  88200;
        if(FLACFrameHeader->sampleRateCode == 2)  FLACMetadataBlock->sampleRate = 176400;
        if(FLACFrameHeader->sampleRateCode == 3)  FLACMetadataBlock->sampleRate = 192000;
        if(FLACFrameHeader->sampleRateCode == 4)  FLACMetadataBlock->sampleRate =   8000;
        if(FLACFrameHeader->sampleRateCode == 5)  FLACMetadataBlock->sampleRate =  16000;
        if(FLACFrameHeader->sampleRateCode == 6)  FLACMetadataBlock->sampleRate =  22050;
        if(FLACFrameHeader->sampleRateCode == 7)  FLACMetadataBlock->sampleRate =  24000;
        if(FLACFrameHeader->sampleRateCode == 8)  FLACMetadataBlock->sampleRate =  32000;
        if(FLACFrameHeader->sampleRateCode == 9)  FLACMetadataBlock->sampleRate =  44100;
        if(FLACFrameHeader->sampleRateCode == 10) FLACMetadataBlock->sampleRate =  48000;
        if(FLACFrameHeader->sampleRateCode == 11) FLACMetadataBlock->sampleRate =  96000;
    }
    readUint(1, bytesLeft);
    uint32_t temp = (readUint(8, bytesLeft) << 24);
    temp = ~temp;
    uint32_t shift = 0x80000000; // Number of leading zeros
    int8_t count = 0;
    for(int i=0; i<32; i++){
        if((temp & shift) == 0) {count++; shift >>= 1;}
        else break;
    }
    count--;
    for (int i = 0; i < count; i++) readUint(8, bytesLeft);
    m_blockSize = 0;
    if (FLACFrameHeader->blockSizeCode == 1)
        m_blockSize = 192;
    else if (2 <= FLACFrameHeader->blockSizeCode && FLACFrameHeader->blockSizeCode <= 5)
        m_blockSize = 576 << (FLACFrameHeader->blockSizeCode - 2);
    else if (FLACFrameHeader->blockSizeCode == 6)
        m_blockSize = readUint(8, bytesLeft) + 1;
    else if (FLACFrameHeader->blockSizeCode == 7)
        m_blockSize = readUint(16, bytesLeft) + 1;
    else if (8 <= FLACFrameHeader->blockSizeCode && FLACFrameHeader->blockSizeCode <= 15)
        m_blockSize = 256 << (FLACFrameHeader->blockSizeCode - 8);
    else{
        return ERR_FLAC_RESERVED_BLOCKSIZE_UNSUPPORTED;
    }
    uint16_t maxBS = 8192;
    if(psramFound()) maxBS = 8192 * 4;
    if(m_blockSize > maxBS){
        log_e("Error: blockSize too big ,%i bytes", m_blockSize);
        return ERR_FLAC_BLOCKSIZE_TOO_BIG;
    }
    if(FLACFrameHeader->sampleRateCode == 12)
        readUint(8, bytesLeft);
    else if (FLACFrameHeader->sampleRateCode == 13 || FLACFrameHeader->sampleRateCode == 14){
        readUint(16, bytesLeft);
    }
    readUint(8, bytesLeft);
    m_status = DECODE_SUBFRAMES;
    m_blockSizeLeft = m_blockSize;
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
uint16_t FLACGetOutputSamps(){
    int vs = m_validSamples;
    m_validSamples=0;
    return vs;
}
//----------------------------------------------------------------------------------------------------------------------
uint64_t FLACGetTotoalSamplesInStream(){
    return FLACMetadataBlock->totalSamples;
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t FLACGetBitsPerSample(){
    return FLACMetadataBlock->bitsPerSample;
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t FLACGetChannels(){
    return FLACMetadataBlock->numChannels;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetSampRate(){
    return FLACMetadataBlock->sampleRate;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetBitRate(){
    return m_bitrate;
}
//----------------------------------------------------------------------------------------------------------------------
uint32_t FLACGetAudioFileDuration() {
    if(FLACGetSampRate()){
        uint32_t afd = FLACGetTotoalSamplesInStream()/ FLACGetSampRate(); // AudioFileDuration
        return afd;
    }
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeSubframes(int* bytesLeft){
    if(FLACFrameHeader->chanAsgn <= 7) {
        for (int ch = 0; ch < FLACMetadataBlock->numChannels; ch++)
            decodeSubframe(FLACMetadataBlock->bitsPerSample, ch, bytesLeft);
    }
    else if (8 <= FLACFrameHeader->chanAsgn && FLACFrameHeader->chanAsgn <= 10) {
        decodeSubframe(FLACMetadataBlock->bitsPerSample + (FLACFrameHeader->chanAsgn == 9 ? 1 : 0), 0, bytesLeft);
        decodeSubframe(FLACMetadataBlock->bitsPerSample + (FLACFrameHeader->chanAsgn == 9 ? 0 : 1), 1, bytesLeft);
        if(FLACFrameHeader->chanAsgn == 8) {
            for (int i = 0; i < m_blockSize; i++)
                FLACsubFramesBuff->samplesBuffer[1][i] = (
                        FLACsubFramesBuff->samplesBuffer[0][i] -
                        FLACsubFramesBuff->samplesBuffer[1][i]);
        }
        else if (FLACFrameHeader->chanAsgn == 9) {
            for (int i = 0; i < m_blockSize; i++)
                FLACsubFramesBuff->samplesBuffer[0][i] += FLACsubFramesBuff->samplesBuffer[1][i];
        }
        else if (FLACFrameHeader->chanAsgn == 10) {
            for (int i = 0; i < m_blockSize; i++) {
                long side =  FLACsubFramesBuff->samplesBuffer[1][i];
                long right = FLACsubFramesBuff->samplesBuffer[0][i] - (side >> 1);
                FLACsubFramesBuff->samplesBuffer[1][i] = right;
                FLACsubFramesBuff->samplesBuffer[0][i] = right + side;
            }
        }
        else {
            log_e("unknown channel assignment, %i", FLACFrameHeader->chanAsgn);
            return ERR_FLAC_UNKNOWN_CHANNEL_ASSIGNMENT;
        }
    }
    else{
        log_e("Reserved channel assignment, %i", FLACFrameHeader->chanAsgn);
        return ERR_FLAC_RESERVED_CHANNEL_ASSIGNMENT;
    }
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeSubframe(uint8_t sampleDepth, uint8_t ch, int* bytesLeft) {
    int8_t ret = 0;
    readUint(1, bytesLeft);
    uint8_t type = readUint(6, bytesLeft);
    int shift = readUint(1, bytesLeft);
    if (shift == 1) {
        while (readUint(1, bytesLeft) == 0)
            shift++;
    }
    sampleDepth -= shift;

    if(type == 0){  // Constant coding
        int16_t s= readSignedInt(sampleDepth, bytesLeft);
        for(int i=0; i < m_blockSize; i++){
            FLACsubFramesBuff->samplesBuffer[ch][i] = s;
        }
    }
    else if (type == 1) {  // Verbatim coding
        for (int i = 0; i < m_blockSize; i++)
            FLACsubFramesBuff->samplesBuffer[ch][i] = readSignedInt(sampleDepth, bytesLeft);
    }
    else if (8 <= type && type <= 12){
        ret = decodeFixedPredictionSubframe(type - 8, sampleDepth, ch, bytesLeft);
        if(ret) return ret;
    }
    else if (32 <= type && type <= 63){
        ret = decodeLinearPredictiveCodingSubframe(type - 31, sampleDepth, ch, bytesLeft);
        if(ret) return ret;
    }
    else{
        return ERR_FLAC_RESERVED_SUB_TYPE;
    }
    if(shift>0){
        for (int i = 0; i < m_blockSize; i++){
            FLACsubFramesBuff->samplesBuffer[ch][i] <<= shift;
        }
    }
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeFixedPredictionSubframe(uint8_t predOrder, uint8_t sampleDepth, uint8_t ch, int* bytesLeft) {
    uint8_t ret = 0;
    for(uint8_t i = 0; i < predOrder; i++)
        FLACsubFramesBuff->samplesBuffer[ch][i] = readSignedInt(sampleDepth, bytesLeft);
    ret = decodeResiduals(predOrder, ch, bytesLeft);
    if(ret) return ret;
    coefs.clear();
    if(predOrder == 0) coefs.resize(0);
    if(predOrder == 1) coefs.push_back(1);  // FIXED_PREDICTION_COEFFICIENTS
    if(predOrder == 2){coefs.push_back(2); coefs.push_back(-1);}
    if(predOrder == 3){coefs.push_back(3); coefs.push_back(-3); coefs.push_back(1);}
    if(predOrder == 4){coefs.push_back(4); coefs.push_back(-6); coefs.push_back(4); coefs.push_back(-1);}
    if(predOrder > 4) return ERR_FLAC_PREORDER_TOO_BIG; // Error: preorder > 4"
    restoreLinearPrediction(ch, 0);
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeLinearPredictiveCodingSubframe(int lpcOrder, int sampleDepth, uint8_t ch, int* bytesLeft){
    int8_t ret = 0;
    for (int i = 0; i < lpcOrder; i++)
        FLACsubFramesBuff->samplesBuffer[ch][i] = readSignedInt(sampleDepth, bytesLeft);
    int precision = readUint(4, bytesLeft) + 1;
    int shift = readSignedInt(5, bytesLeft);
    coefs.resize(0);
    for (uint8_t i = 0; i < lpcOrder; i++)
        coefs.push_back(readSignedInt(precision, bytesLeft));
    ret = decodeResiduals(lpcOrder, ch, bytesLeft);
    if(ret) return ret;
    restoreLinearPrediction(ch, shift);
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
int8_t decodeResiduals(uint8_t warmup, uint8_t ch, int* bytesLeft) {

    int method = readUint(2, bytesLeft);
    if (method >= 2)
        return ERR_FLAC_RESERVED_RESIDUAL_CODING; // Reserved residual coding method
    uint8_t paramBits = method == 0 ? 4 : 5;
    int escapeParam = (method == 0 ? 0xF : 0x1F);
    int partitionOrder = readUint(4, bytesLeft);

    int numPartitions = 1 << partitionOrder;
    if (m_blockSize % numPartitions != 0)
        return ERR_FLAC_WRONG_RICE_PARTITION_NR; //Error: Block size not divisible by number of Rice partitions
    int partitionSize = m_blockSize/ numPartitions;

    for (int i = 0; i < numPartitions; i++) {
        int start = i * partitionSize + (i == 0 ? warmup : 0);
        int end = (i + 1) * partitionSize;

        int param = readUint(paramBits, bytesLeft);
        if (param < escapeParam) {
            for (int j = start; j < end; j++){
                FLACsubFramesBuff->samplesBuffer[ch][j] = readRiceSignedInt(param, bytesLeft);
            }
        } else {
            int numBits = readUint(5, bytesLeft);
            for (int j = start; j < end; j++){
                FLACsubFramesBuff->samplesBuffer[ch][j] = readSignedInt(numBits, bytesLeft);
            }
        }
    }
    return ERR_FLAC_NONE;
}
//----------------------------------------------------------------------------------------------------------------------
void restoreLinearPrediction(uint8_t ch, uint8_t shift) {

    for (int i = coefs.size(); i < m_blockSize; i++) {
        int32_t sum = 0;
        for (int j = 0; j < coefs.size(); j++){
            sum += FLACsubFramesBuff->samplesBuffer[ch][i - 1 - j] * coefs[j];
        }
        FLACsubFramesBuff->samplesBuffer[ch][i] += (sum >> shift);
    }
}
//----------------------------------------------------------------------------------------------------------------------
int FLAC_specialIndexOf(uint8_t* base, const char* str, int baselen, bool exact){
    int result;  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int i = 0; i < baselen - strlen(str); i++){
        result = i;
        for (int j = 0; j < strlen(str) + exact; j++){
            if (*(base + i + j) != *(str + j)){
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
