#pragma once

#include "Arduino.h"

class RangeDecoder {
public:
    RangeDecoder();
    ~RangeDecoder(){}
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
    void add_nbits_total(int32_t nbits_total);
    int32_t get_error();
    uint32_t get_rng();
    int32_t laplace_decode(uint32_t fs, int32_t decay);

private:
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

    #define LAPLACE_LOG_MINP (0)
    #define LAPLACE_MINP (1<<LAPLACE_LOG_MINP)
    #define LAPLACE_NMIN (16)

    uint8_t* m_buf = nullptr;     /*Buffered input/output */
    uint32_t m_storage = 0; /*The size of the buffer.*/
    uint32_t m_end_offs = 0; /*The offset at which the last byte containing raw bits was read/written.*/
    uint32_t m_end_window = 0; /*Bits that will be read from/written at the end.*/
    int32_t  m_nend_bits = 0; /*Number of valid bits in end_window.*/
    int32_t  m_nbits_total = 0;
    uint32_t m_offs = 0; /*The offset at which the next range coder byte will be read/written.*/
    uint32_t m_rng = 0; /*The number of values in the current range.*/
    uint32_t m_val = 0;
    uint32_t m_ext = 0;
    int32_t  m_rem = 0; /*A buffered input/output symbol, awaiting carry propagation.*/
    int32_t  m_error = 0; /*Nonzero if an error occurred.*/


    int32_t read_byte();
    int32_t read_byte_from_end();
    void dec_normalize();
    uint32_t laplace_get_freq1(uint32_t fs0, int32_t decay);
};

#if defined(__GNUC__)
#define RANGE_DECODER_INLINE inline __attribute__((always_inline))
#else
#define RANGE_DECODER_INLINE inline
#endif

RANGE_DECODER_INLINE int32_t RangeDecoder::read_byte() { return m_offs < m_storage ? m_buf[m_offs++] : 0; }

RANGE_DECODER_INLINE int32_t RangeDecoder::read_byte_from_end() {
    return m_end_offs < m_storage ? m_buf[m_storage - ++(m_end_offs)] : 0;
}

RANGE_DECODER_INLINE uint32_t RangeDecoder::decode(uint32_t _ft) {
    uint32_t s;
    m_ext = m_rng / _ft;
    s = (uint32_t)(m_val / m_ext);
    return _ft - EC_MINI(s + 1, _ft);
}

RANGE_DECODER_INLINE uint32_t RangeDecoder::decode_bin(uint32_t _bits) {
    uint32_t s;
    m_ext = m_rng >> _bits;
    s = (uint32_t)(m_val / m_ext);
    return (1U << _bits) - EC_MINI(s + 1U, 1U << _bits);
}

RANGE_DECODER_INLINE void RangeDecoder::dec_update(uint32_t _fl, uint32_t _fh, uint32_t _ft) {
    uint32_t s;
    s = m_ext * (_ft - _fh);
    m_val -= s;

    if (_fl > 0) {
        m_rng = m_ext * (_fh - _fl);
    } else {
        m_rng = m_rng - s;
    }
    dec_normalize();
}

/*The probability of having a "one" is 1/(1<<_logp).*/
RANGE_DECODER_INLINE int32_t RangeDecoder::dec_bit_logp(uint32_t _logp) {
    uint32_t r;
    uint32_t d;
    uint32_t s;
    int32_t ret;
    r = m_rng;
    d = m_val;
    s = r >> _logp;
    ret = d < s;
    if (!ret) m_val = d - s;
    m_rng = ret ? s : r - s;
    dec_normalize();
    return ret;
}

RANGE_DECODER_INLINE int32_t RangeDecoder::dec_icdf(const uint8_t *_icdf, uint32_t _ftb) {
    uint32_t r;
    uint32_t d;
    uint32_t s;
    uint32_t t;
    int32_t ret;
    s = m_rng;
    d = m_val;
    r = s >> _ftb;
    ret = -1;
    do {
        t = s;
        s = r * _icdf[++ret];
    } while (d < s);
    m_val = d - s;
    m_rng = t - s;
    dec_normalize();
    return ret;
}

RANGE_DECODER_INLINE int32_t RangeDecoder::tell() { return m_nbits_total - EC_ILOG(m_rng); }
RANGE_DECODER_INLINE void RangeDecoder::add_nbits_total(int32_t nbits_total) { m_nbits_total += nbits_total; }
RANGE_DECODER_INLINE uint32_t RangeDecoder::get_storage() { return m_storage; }
RANGE_DECODER_INLINE int32_t RangeDecoder::get_error() { return m_error; }
RANGE_DECODER_INLINE uint32_t RangeDecoder::get_rng() { return m_rng; }

/* When called, decay is positive and at most 11456. */
RANGE_DECODER_INLINE uint32_t RangeDecoder::laplace_get_freq1(uint32_t fs0, int32_t decay) {
    uint32_t ft;
    ft = 32768 - LAPLACE_MINP * (2 * LAPLACE_NMIN) - fs0;
    return ft * (int32_t)(16384 - decay) >> 15;
}

#undef RANGE_DECODER_INLINE
