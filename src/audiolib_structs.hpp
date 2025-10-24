#pragma once
#include "psram_unique_ptr.hpp"
#include <cstdint>
#include <stddef.h>

// this file contains definitions of various structs used in Audio lib

namespace audiolib {
struct sylt_t {
    size_t   size;
    uint32_t pos;
    char     lang[5];
    uint8_t  text_encoding;
    uint8_t  time_stamp_format;
    uint8_t  content_type;
};

struct ID3Hdr_t { // used only in readID3header()
    size_t                retvalue;
    size_t                headerSize;
    size_t                tagSize;
    size_t                cnt;
    size_t                id3Size;
    size_t                totalId3Size; // if we have more header, id3_1_size + id3_2_size + ....
    size_t                remainingHeaderBytes;
    size_t                universal_tmp;
    uint8_t               ID3version;
    uint8_t               ID3revision;
    uint8_t               flags;
    bool                  unsync;
    bool                  extended_header;
    bool                  experimental_indicator;
    bool                  footer_present;
    size_t                offset;
    size_t                currentPosition;
    int                   ehsz;
    char                  tag[5];
    char                  frameid[5];
    char                  lang[5];
    size_t                framesize;
    bool                  compressed;
    std::vector<uint32_t> APIC_vec;
    sylt_t                SYLT;
    uint8_t               numID3Header;
    uint16_t              iBuffSize;
    uint8_t               contentDescriptorTerminator_0;
    uint8_t               contentDescriptorTerminator_1;
    uint8_t               textStringTerminator_0;
    uint8_t               textStringTerminator_1;
    bool                  byteOrderMark;
    ps_ptr<char>          iBuff;
};

struct pwsHLS_t { // used in processWebStreamHLS()
    uint16_t        maxFrameSize;
    uint16_t        ID3BuffSize;
    uint32_t        availableBytes;
    bool            firstBytes;
    bool            f_chunkFinished;
    uint32_t        byteCounter;
    int32_t         chunkSize;
    uint16_t        ID3WritePtr;
    uint16_t        ID3ReadPtr;
    ps_ptr<uint8_t> ID3Buff;
};

struct pplM3u8_t { // used in parsePlaylist_M3U8
    uint64_t xMedSeq;
    bool     f_mediaSeq_found;
};

struct m4aHdr_t { // used in read_M4A_Header
    size_t   headerSize;
    size_t   retvalue;
    size_t   atomsize;
    size_t   sizeof_ftyp;
    size_t   sizeof_moov;
    size_t   sizeof_free;
    size_t   sizeof_mdat;
    size_t   sizeof_trak;
    size_t   sizeof_ilst;
    size_t   sizeof_esds;
    size_t   sizeof_mdia;
    size_t   sizeof_minf;
    size_t   sizeof_mdhd;
    size_t   sizeof_stbl;
    size_t   sizeof_stsd;
    size_t   sizeof_stsz;
    size_t   sizeof_mp4a;
    size_t   sizeof_udta;
    size_t   sizeof_meta;
    size_t   audioDataPos;
    size_t   cnt;
    size_t   offset;
    uint32_t picPos;
    uint32_t picLen;
    uint32_t ilst_pos;
    uint8_t  channel_count;
    uint8_t  sample_size;         // bps
    uint8_t  objectTypeIndicator; // esds
    uint8_t  streamType;          // esds
    uint32_t bufferSizeDB;        // esds
    uint32_t maxBitrate;          // esds
    uint32_t nomBitrate;          // esds
    uint32_t timescale;           // mdhd
    uint32_t duration;            // mdhd
    uint16_t sample_rate;
    uint8_t  aac_profile;
    uint32_t stsz_num_entries;
    uint32_t stsz_table_pos;
    bool     progressive; // Progressive (moov before mdat)
    bool     version_flags;
};

struct plCh_t { // used in playChunk
    int32_t   validSamples;
    int32_t   samples48K = 0;
    uint32_t  count = 0;
    size_t    i2s_bytesConsumed;
    int16_t*  sample[2];
    int16_t*  s2;
    int       sampleSize;
    esp_err_t err;
    int       i;
};

struct lVar_t { // used in loop
    uint8_t  no_host_cnt;
    uint32_t no_host_timer;
    uint8_t  count;
};

struct hwoe_t { // used in dismantle_host
    bool         ssl;
    ps_ptr<char> hwoe;     // host without extension
    ps_ptr<char> rqh_host; // host in request header
    uint16_t     port;
    ps_ptr<char> extension;
    ps_ptr<char> query_string;
};

struct prlf_t { // used in processLocalFile
    uint32_t ctime;
    int32_t  newFilePos;
    bool     audioHeaderFound;
    uint32_t timeout;
    uint32_t maxFrameSize;
    uint32_t availableBytes;
    int32_t  bytesAddedToBuffer;
};

typedef struct _cat { // used in calculateAudioTime
    uint64_t sumBytesIn{};
    uint64_t sum_samples{};
    uint32_t counter{};
    uint32_t timeStamp{};
    uint32_t deltaBytesIn{};
    uint32_t nominalBitRate{};
    uint32_t tota_samples{};
    uint32_t avrBitRate{};
    uint16_t syltIdx{};
    uint32_t avrBitrateStable{};
    uint32_t oldAvrBitrate{};
    uint32_t brCounter{};

    void reset() {
        // Default-initialize alles neu (inklusive Array)
        *this = _cat{};
    }
} cat_t;

struct cVUl_t { // used in computeVUlevel
    uint8_t sampleArray[2][4][8] = {0};
    uint8_t cnt0 = 0, cnt1 = 0, cnt2 = 0, cnt3 = 0, cnt4 = 0;
    bool    f_vu = false;
};

struct ifCh_t { // used in IIR_filterChain0, 1, 2
    float   inSample0[2];
    float   outSample0[2];
    int16_t iir_out0[2];
    float   inSample1[2];
    float   outSample1[2];
    int16_t iir_out1[2];
    float   inSample2[2];
    float   outSample2[2];
    int16_t iir_out2[2];
};

typedef struct _tspp { // used in ts_parsePacket
    int     pidNumber{};
    int     pids[4]{}; // PID_ARRAY_LEN
    int     PES_DataLength{};
    int     pidOfAAC{};
    uint8_t fillData{};

    void reset() {
        // Default-initialize alles neu (inklusive Array)
        *this = _tspp{};
    }
}tspp_t;

struct pwst_t { // used in processWebStream
    uint16_t maxFrameSize;
    uint32_t chunkSize = 0;
    bool     f_skipCRLF = false;
    uint32_t availableBytes;
    bool     f_clientIsConnected;
    uint16_t readedBytes;
};

struct gchs_t { // used in getChunkSize
    bool   f_skipCRLF;
    bool   isHttpChunked;
    size_t transportLimit;
    bool   oneByteOfTwo;
};

struct pwf_t { // used in processWebFile
    uint32_t maxFrameSize;
    int32_t  newFilePos;
    bool     audioHeaderFound;
    uint32_t chunkSize;
    size_t   audioDataCount;
    uint32_t byteCounter;
    uint32_t nextChunkCount;
    bool     f_waitingForPayload = false;
    bool     f_clientIsConnected;
    uint32_t ctime;
    uint32_t timeout;
    uint32_t availableBytes;
    int32_t  bytesAddedToBuffer;
};

struct pad_t { // used in playAudioData
    uint8_t count = 0;
    size_t  oldAudioDataSize = 0;
    bool    lastFrames = false;
    int32_t bytesToDecode;
    int32_t bytesDecoded;
};

struct sbyt_t { // used in sendBytes
    int32_t     bytesLeft;
    bool        f_setDecodeParamsOnce = true;
    uint8_t     channels = 0;
    int         nextSync = 0;
    uint8_t     isPS = 0;
    const char* opus_mode = nullptr;
};

struct rmet_t {          // used in readMetadata
    uint16_t pos_ml = 0; // determines the current position in metaline
    uint16_t metaDataSize = 0;
    uint16_t res = 0;
};

struct pwsts_t {                    // used in processWebStreamTS
    uint32_t        availableBytes; // available bytes in stream
    bool            f_firstPacket;
    bool            f_chunkFinished;
    bool            f_nextRound;
    uint32_t        byteCounter; // count received data
    uint8_t         ts_packetStart = 0;
    uint8_t         ts_packetLength = 0;
    uint8_t         ts_packetPtr = 0;
    const uint8_t   ts_packetsize = 188;
    ps_ptr<uint8_t> ts_packet;
    size_t          chunkSize = 0;
};

struct rwh_t { // used in read_WAV_Header
    size_t   headerSize;
    uint32_t cs = 0;
    uint8_t  bts = 0;
};

typedef struct _rflh { // used in read_FLAC_Header
    std::vector<uint32_t> picVec{};
    size_t                headerSize{};
    size_t                retvalue{};
    bool                  f_lastMetaBlock{};
    uint32_t              picPos{};
    uint32_t              picLen{};
    uint32_t              duration{};
    uint32_t              nominalBitrate{};
    uint8_t               numChannels{};
    uint8_t               bitsPerSample{};
    uint32_t              sampleRate{};
    uint32_t              maxFrameSize{};
    uint32_t              maxBlockSize{};
    uint32_t              totalSamplesInStream{};

    void reset() {
        // Default-initialize alles neu (inklusive Array)
        *this = _rflh{};
    }
} rflh_t;

typedef struct _phreh { // used in parseHttpResponseHeader
    uint32_t ctime{};
    uint32_t timeout{};
    uint32_t stime{};
    uint32_t bitrate{};
    bool     f_time{};
    bool     f_icy_data{};

    void reset() {
        // Default-initialize alles neu (inklusive Array)
        *this = _phreh{};
    }
} phreh_t;

struct phrah_t { // used in parseHttpRangeHeader
    uint32_t ctime;
    uint32_t timeout;
    uint32_t stime;
    bool     f_time = false;
};

struct sdet_t { // used in streamDetection
    uint32_t tmr_slow = 0;
    uint32_t tmr_lost = 0;
    uint8_t  cnt_slow = 0;
    uint8_t  cnt_lost = 0;
};

struct fnsy_t { // used in findNextSync
    int      nextSync = 0;
    uint32_t swnf = 0;
};

} // namespace audiolib