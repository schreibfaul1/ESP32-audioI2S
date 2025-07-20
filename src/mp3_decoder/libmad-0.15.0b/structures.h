// libmad mp3 decoder
// structures.h

#pragma once
#include "Arduino.h"

enum mad_layer {
    MAD_LAYER_I = 1,  /* Layer I */
    MAD_LAYER_II = 2, /* Layer II */
    MAD_LAYER_III = 3 /* Layer III */
};

enum mad_mode {
    MAD_MODE_SINGLE_CHANNEL = 0, /* single channel */
    MAD_MODE_DUAL_CHANNEL = 1,   /* dual channel */
    MAD_MODE_JOINT_STEREO = 2,   /* joint (MS/intensity) stereo */
    MAD_MODE_STEREO = 3          /* normal LR stereo */
};

enum mad_emphasis {
    MAD_EMPHASIS_NONE = 0,       /* no emphasis */
    MAD_EMPHASIS_50_15_US = 1,   /* 50/15 microseconds emphasis */
    MAD_EMPHASIS_CCITT_J_17 = 3, /* CCITT J.17 emphasis */
    MAD_EMPHASIS_RESERVED = 2    /* unknown emphasis */
};

enum {
    MAD_FLAG_NPRIVATE_III = 0x0007, /* number of Layer III private bits */
    MAD_FLAG_INCOMPLETE = 0x0008,   /* header but not data is decoded */

    MAD_FLAG_PROTECTION = 0x0010, /* frame has CRC protection */
    MAD_FLAG_COPYRIGHT = 0x0020,  /* frame is copyright */
    MAD_FLAG_ORIGINAL = 0x0040,   /* frame is original (else copy) */
    MAD_FLAG_PADDING = 0x0080,    /* frame has additional slot */

    MAD_FLAG_I_STEREO = 0x0100,   /* uses intensity joint stereo */
    MAD_FLAG_MS_STEREO = 0x0200,  /* uses middle/side joint stereo */
    MAD_FLAG_FREEFORMAT = 0x0400, /* uses free format bitrate */

    MAD_FLAG_LSF_EXT = 0x1000,     /* lower sampling freq. extension */
    MAD_FLAG_MC_EXT = 0x2000,      /* multichannel audio extension */
    MAD_FLAG_MPEG_2_5_EXT = 0x4000 /* MPEG 2.5 (unofficial) extension */
};

enum {
    MAD_PRIVATE_HEADER = 0x0100, /* header private bit */
    MAD_PRIVATE_III = 0x001f     /* Layer III private bits (up to 5) */
};

enum { MAD_PCM_CHANNEL_SINGLE = 0 }; /* single channel PCM selector */
enum { MAD_PCM_CHANNEL_DUAL_1 = 0, MAD_PCM_CHANNEL_DUAL_2 = 1 }; /* dual channel PCM selector */
enum { MAD_PCM_CHANNEL_STEREO_LEFT = 0, MAD_PCM_CHANNEL_STEREO_RIGHT = 1 }; /* stereo PCM selector */

enum mad_units {
    MAD_UNITS_HOURS = -2,
    MAD_UNITS_MINUTES = -1,
    MAD_UNITS_SECONDS = 0,

    /* metric units */
    MAD_UNITS_DECISECONDS = 10,
    MAD_UNITS_CENTISECONDS = 100,
    MAD_UNITS_MILLISECONDS = 1000,

    /* common libmad time/frame/byte units */
    // Diese Einheiten sind hier ohne explizite Werte hinzugefügt.
    // C/C++ weist ihnen dann automatisch aufeinanderfolgende Werte zu, beginnend nach MAD_UNITS_MILLISECONDS.
    // Wenn libmad dafür spezifische Werte vorsieht, sollten diese verwendet werden.
    MAD_UNITS_DECFRAMES,  /* fraction of frames (1/10) */
    MAD_UNITS_FRAMES,     /* integer frames */
    MAD_UNITS_SAMPLES,    /* individual audio samples */
    MAD_UNITS_BITFRAMES,  /* fraction of bytes (1/8) */
    MAD_UNITS_BYTES,      /* individual bytes */
    MAD_UNITS_PERCENT,    /* percentage of total stream (1/1000th usually) */
    MAD_UNITS_HZ,         /* Hertz (samples per second) - this might overlap with 8000_HZ etc. */
                          // Beachten Sie, dass MAD_UNITS_HZ in der originalen libmad oft auch als
                          // eine Art Basiseinheit für alle anderen Frequenzeinheiten dient.
                          // Sein numerischer Wert muss nicht zwingend 1 sein, wenn die anderen explizite Werte haben.
                          // Wenn dies die einzige 'HZ'-Einheit ist, könnte sie auch 1 sein.

    /* audio sample units (fixed frequencies) */
    MAD_UNITS_8000_HZ = 8000,
    MAD_UNITS_11025_HZ = 11025,
    MAD_UNITS_12000_HZ = 12000,
    MAD_UNITS_16000_HZ = 16000,
    MAD_UNITS_22050_HZ = 22050,
    MAD_UNITS_24000_HZ = 24000,
    MAD_UNITS_32000_HZ = 32000,
    MAD_UNITS_44100_HZ = 44100,
    MAD_UNITS_48000_HZ = 48000,

    /* video frame/field units (fixed integer FPS) */
    MAD_UNITS_24_FPS = 24,
    MAD_UNITS_25_FPS = 25,
    MAD_UNITS_30_FPS = 30,
    MAD_UNITS_48_FPS = 48,
    MAD_UNITS_50_FPS = 50,
    MAD_UNITS_60_FPS = 60,

    /* CD audio frames */
    MAD_UNITS_75_FPS = 75,

    /* video drop-frame units (fixed fractional FPS, negative of base FPS) */
    MAD_UNITS_23_976_FPS = -24,
    MAD_UNITS_24_975_FPS = -25,
    MAD_UNITS_29_97_FPS = -30,
    MAD_UNITS_47_952_FPS = -48,
    MAD_UNITS_49_95_FPS = -50,
    MAD_UNITS_59_94_FPS = -60
};

enum mad_error {
    MAD_ERROR_NONE = 0x0000,           /* no error */
    MAD_ERROR_BUFLEN = 0x0001,         /* input buffer too small (or EOF) */
    MAD_ERROR_BUFPTR = 0x0002,         /* invalid (null) buffer pointer */
    MAD_ERROR_NOMEM = 0x0031,          /* not enough memory */
    MAD_ERROR_CONTINUE = 0x0064,       // need more data (100 decimal)
    MAD_ERROR_LOSTSYNC = 0x0101,       /* lost synchronization */
    MAD_ERROR_BADLAYER = 0x0102,       /* reserved header layer value */
    MAD_ERROR_BADBITRATE = 0x0103,     /* forbidden bitrate value */
    MAD_ERROR_BADSAMPLERATE = 0x0104,  /* reserved sample frequency value */
    MAD_ERROR_BADEMPHASIS = 0x0105,    /* reserved emphasis value */
    MAD_ERROR_BADCRC = 0x0201,         /* CRC check failed */
    MAD_ERROR_BADBITALLOC = 0x0211,    /* forbidden bit allocation value */
    MAD_ERROR_BADSCALEFACTOR = 0x0221, /* bad scalefactor index */
    MAD_ERROR_BADFRAMELEN = 0x0231,    /* bad frame length */
    MAD_ERROR_BADBIGVALUES = 0x0232,   /* bad big_values count */
    MAD_ERROR_BADBLOCKTYPE = 0x0233,   /* reserved block_type */
    MAD_ERROR_BADSCFSI = 0x0234,       /* bad scalefactor selection info */
    MAD_ERROR_BADDATAPTR = 0x0235,     /* bad main_data_begin pointer */
    MAD_ERROR_BADPART3LEN = 0x0236,    /* bad audio data length */
    MAD_ERROR_BADHUFFTABLE = 0x0237,   /* bad Huffman table select */
    MAD_ERROR_BADHUFFDATA = 0x0238,    /* Huffman data overrun */
    MAD_ERROR_BADSTEREO = 0x0239       /* incompatible block_type for JS */
};

enum {
    MAD_OPTION_IGNORECRC = 0x0001,     /* ignore CRC errors */
    MAD_OPTION_HALFSAMPLERATE = 0x0002 /* generate PCM at 1/2 sample rate */
};

enum mad_decoder_mode { MAD_DECODER_MODE_SYNC = 0, MAD_DECODER_MODE_ASYNC };

enum mad_flow {
    MAD_FLOW_CONTINUE = 0x0000, /* continue normally */
    MAD_FLOW_STOP = 0x0010,     /* stop decoding normally */
    MAD_FLOW_BREAK = 0x0011,    /* stop decoding and signal an error */
    MAD_FLOW_IGNORE = 0x0020    /* ignore the current frame */
};

struct mad_decoder_async {
    int32_t pid;
    int32_t  in, out;
};

typedef struct {
    int32_t   seconds;  /* whole seconds */
    uint32_t fraction; /* 1/MAD_TIMER_RESOLUTION seconds */
} mad_timer_t;

typedef struct fixedfloat {
  uint32_t mantissa  : 27;
  uint16_t exponent :  5;
} fixedfloat_t;

struct mad_header {
    enum mad_layer    layer;          /* audio layer (1, 2, or 3) */
    enum mad_mode     mode;           /* channel mode (see above) */
    int32_t           mode_extension; /* additional mode info */
    enum mad_emphasis emphasis;       /* de-emphasis to use (see above) */
    uint32_t          bitrate;        /* stream bitrate (bps) */
    uint32_t          samplerate;     /* sampling frequency (Hz) */
    uint16_t          crc_check;      /* frame CRC accumulator */
    uint16_t          crc_target;     /* final target CRC checksum */
    int32_t           flags;          /* flags (see below) */
    int32_t           private_bits;   /* private bits (see below) */
    mad_timer_t       duration;       /* audio playing time of frame */
};

/* --- Layer III ----------------------------------------------------------- */
enum { count1table_select = 0x01, scalefac_scale = 0x02, preflag = 0x04, mixed_block_flag = 0x08 };

enum { I_STEREO = 0x1, MS_STEREO = 0x2 };
/*---------------------------------------------------------------------------*/

struct channel {
    /* from side info */
    uint16_t part2_3_length;
    uint16_t big_values;
    uint16_t global_gain;
    uint16_t scalefac_compress;

    uint8_t flags;
    uint8_t block_type;
    uint8_t table_select[3];
    uint8_t subblock_gain[3];
    uint8_t region0_count;
    uint8_t region1_count;

    /* from main_data */
    uint8_t scalefac[39]; /* scalefac_l and/or scalefac_s */
};

struct granule {
    struct channel ch[2];
};
struct sideinfo {
    uint32_t main_data_begin;
    uint32_t private_bits;
    uint8_t scfsi[2];
    struct granule gr[2];
};

typedef struct quantclass {
  uint16_t nlevels;
  uint8_t group;
  uint8_t bits;
  int32_t C;
  int32_t D;
} quantclass_t;

typedef struct sbquant{
    uint32_t        sblimit;
    uint8_t const offsets[30];
} sbquant_t;

typedef struct bitalloc {
    uint16_t nbal;
    uint16_t offset;
} bitalloc_t;

union huffquad {
  struct {
    uint16_t final  :  1;
    uint16_t bits   :  3;
    uint16_t offset : 12;
  } ptr;
  struct {
    uint16_t final  :  1;
    uint16_t hlen   :  3;
    uint16_t v      :  1;
    uint16_t w      :  1;
    uint16_t x      :  1;
    uint16_t y      :  1;
  } value;
  uint16_t final    :  1;
};

union huffpair {
  struct {
    uint16_t final  :  1;
    uint16_t bits   :  3;
    uint16_t offset : 12;
  } ptr;
  struct {
    uint16_t final  :  1;
    uint16_t hlen   :  3;
    uint16_t x      :  4;
    uint16_t y      :  4;
  } value;
  uint16_t final    :  1;
};

struct hufftable {
  union huffpair const *table;
  uint16_t linbits;
  uint16_t startbits;
};

struct mad_bitptr {
  uint8_t const *byte;
  uint16_t cache;
  uint16_t left;
};

typedef struct sflen {
    uint8_t slen1;
    uint8_t slen2;
} sflen_t;

typedef struct sfbwidth{
    uint8_t const* l;
    uint8_t const* s;
    uint8_t const* m;
} const sfbwidth_t;

typedef struct _mad_stream {
    uint8_t const*    buffer;     /* input bitstream buffer */
    uint8_t const*    bufend;     /* end of buffer */
    uint32_t          skiplen;    /* bytes to skip before next frame */
    int32_t               sync;       /* stream sync found */
    uint32_t          freerate;   /* free bitrate (fixed) */
    uint8_t const*    this_frame; /* start of current frame */
    uint8_t const*    next_frame; /* start of next frame */
    struct mad_bitptr ptr;        /* current processing bit pointer */
    struct mad_bitptr anc_ptr;    /* ancillary bits pointer */
    uint32_t          anc_bitlen; /* number of ancillary bits */
                                  //    uint8_t (*main_data)[MAD_BUFFER_MDLEN];
    /* Layer III main_data() */
    uint32_t       md_len;  /* bytes in main_data */
    int32_t            options; /* decoding options (see below) */
    enum mad_error error;   /* error code (see above) */
} mad_stream_t;

typedef struct _mad_synth {
    //    int32_t filter[2][2][2][16][8];  /* polyphase filterbank outputs [ch][eo][peo][s][v] */
    uint32_t phase;      /* current processing phase */
    uint32_t samplerate; /* sampling frequency (Hz) */
    uint16_t channels;   /* number of channels */
    uint16_t length;     /* number of samples per channel */
    //    int32_t        samples[2][1152]; /* PCM output samples [ch][sample] */
} mad_synth_t;

typedef struct _mad_frame {
    struct mad_header header;          /* MPEG audio header */
    int32_t               options;         /* decoding options (from stream) */
    //  int32_t sbsample[2][36][32];   /* synthesis subband filter samples */
    //  int32_t (*overlap)[2][32][18]; /* Layer III block overlap data */
} mad_frame_t;

typedef int64_t     mad_fixed_t;
typedef mad_fixed_t mad_sample_t;