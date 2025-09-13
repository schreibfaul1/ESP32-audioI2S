#include "Arduino.h"
#pragma once

#define EC_WINDOW_SIZE ((int32_t)sizeof(uint32_t)*CHAR_BIT)
#define EC_UINT_BITS   (8)
#define EC_MINI(_a,_b)      ((_a)+(((_b)-(_a))&-((_b)<(_a))))
#define EC_CLZ0s    ((int32_t)sizeof(uint32_t)*CHAR_BIT)
#define EC_CLZ(_x) (__builtin_clz(_x))
#define EC_ILOG(_x) (EC_CLZ0s-EC_CLZ(_x))
#define EC_BITRES 3

#define EC_SYM_BITS       8
#define EC_CODE_BITS      32
#define EC_SYM_MAX        ((1U << EC_SYM_BITS) - 1)
#define EC_CODE_TOP       1U << (EC_CODE_BITS - 1)
#define EC_CODE_BOT       EC_CODE_TOP >> EC_SYM_BITS
#define EC_CODE_EXTRA     ((EC_CODE_BITS-2) % EC_SYM_BITS + 1)

typedef struct _ec_ctx {
    
    
    
    int32_t nbits_total;
    uint32_t offs; /*The offset at which the next range coder byte will be read/written.*/
    uint32_t rng; /*The number of values in the current range.*/
    uint32_t val;
    uint32_t ext;
    int32_t rem; /*A buffered input/output symbol, awaiting carry propagation.*/
    int32_t error; /*Nonzero if an error occurred.*/
} ec_ctx_t;


class RangeDecoder {
private:
    uint8_t* m_buf;     /*Buffered input/output */
    uint32_t m_storage; /*The size of the buffer.*/
    uint32_t m_end_offs; /*The offset at which the last byte containing raw bits was read/written.*/
    uint32_t m_end_window; /*Bits that will be read from/written at the end.*/
    int32_t  m_nend_bits; /*Number of valid bits in end_window.*/
public:
    int32_t read_byte();
    int32_t read_byte_from_end();
    void dec_normalize();

    public:
    RangeDecoder();
    ~RangeDecoder(){;}
    void dec_init(uint8_t *_buf, uint32_t _storage);
    uint32_t get_storage();
    uint32_t dec_bits(uint32_t _bits);
    uint32_t dec_uint(uint32_t _ft);
    uint32_t decode(uint32_t _ft);
    uint32_t decode_bin(uint32_t _bits);
    void dec_update(uint32_t _fl, uint32_t _fh, uint32_t _ft);
    int32_t dec_bit_logp( uint32_t _logp);
    uint32_t tell_frac();
    int32_t dec_icdf(const uint8_t *_icdf, uint32_t _ftb);
    int32_t tell();
};



void ec_add_nbits_total(int32_t nbits_total);

int32_t ec_get_error();
uint32_t ec_get_rng();