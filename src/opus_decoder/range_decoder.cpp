#include "range_decoder.h"

RangeDecoder::RangeDecoder() : m_buf(nullptr) {}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This is a faster version of ec_tell_frac() that takes advantage of the low (1/8 bit) resolution to use just a linear function followed by a lookup to determine the exact transition thresholds. */
uint32_t RangeDecoder::tell_frac() {
    const uint32_t correction[8] = {35733, 38967, 42495, 46340, 50535, 55109, 60097, 65535};
    uint32_t nbits;
    uint32_t r;
    int32_t l;
    uint32_t b;
    nbits = m_nbits_total << EC_BITRES;
    l = EC_ILOG(m_rng);
    r = m_rng >> (l - 16);
    b = (r >> 12) - 8;
    b += r > correction[b];
    l = (l << 3) + b;
    return nbits - l;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t RangeDecoder::read_byte() { return m_offs < m_storage ? m_buf[m_offs++] : 0; }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t RangeDecoder::read_byte_from_end() {
    return m_end_offs < m_storage ? m_buf[m_storage - ++(m_end_offs)] : 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*Normalizes the contents of val and rng so that rng lies entirely in the high-order symbol.*/
void RangeDecoder::dec_normalize() {
    /*If the range is too small, rescale it and input some bits.*/
    while (m_rng <= EC_CODE_BOT) {
        int32_t sym;
        m_nbits_total += EC_SYM_BITS;
        m_rng <<= EC_SYM_BITS;
        /*Use up the remaining bits from our last symbol.*/
        sym = m_rem;
        /*Read the next value from the input.*/
        m_rem = read_byte();
        /*Take the rest of the bits we need from this new symbol.*/
        sym = (sym << EC_SYM_BITS | m_rem) >> (EC_SYM_BITS - EC_CODE_EXTRA);
        /*And subtract them from val, capped to be less than EC_CODE_TOP.*/
        m_val = ((m_val << EC_SYM_BITS) + (EC_SYM_MAX & ~sym)) & ((EC_CODE_TOP) - 1);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void RangeDecoder::dec_init(uint8_t *_buf, uint32_t _storage) {

    m_buf = _buf;
    m_storage = _storage;
    m_end_offs = 0;
    m_end_window = 0;
    m_nend_bits = 0;
    m_nbits_total = EC_CODE_BITS + 1 - ((EC_CODE_BITS - EC_CODE_EXTRA) / EC_SYM_BITS) * EC_SYM_BITS;
    m_offs = 0;
    m_rng = 1U << EC_CODE_EXTRA;
    m_rem = read_byte();
    m_val = m_rng - 1 - (m_rem >> (EC_SYM_BITS - EC_CODE_EXTRA));
    m_error = 0;
    /*Normalize the interval.*/
    dec_normalize();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t RangeDecoder::decode(uint32_t _ft) {
    uint32_t s;
    m_ext = m_rng / _ft;
    s = (uint32_t)(m_val / m_ext);
    return _ft - EC_MINI(s + 1, _ft);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t RangeDecoder::decode_bin(uint32_t _bits) {
    uint32_t s;
    m_ext = m_rng >> _bits;
    s = (uint32_t)(m_val / m_ext);
    return (1U << _bits) - EC_MINI(s + 1U, 1U << _bits);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void RangeDecoder::dec_update(uint32_t _fl, uint32_t _fh, uint32_t _ft) {
    uint32_t s;
    s = m_ext *  (_ft - _fh);
    m_val -= s;

    if(_fl > 0){
        m_rng = m_ext * (_fh - _fl);
    }
    else{
        m_rng = m_rng - s;
    }
    dec_normalize();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*The probability of having a "one" is 1/(1<<_logp).*/
int32_t RangeDecoder::dec_bit_logp( uint32_t _logp) {
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
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t RangeDecoder::dec_icdf(const uint8_t *_icdf, uint32_t _ftb) {
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
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t RangeDecoder::dec_uint(uint32_t _ft) {
    uint32_t ft;
    uint32_t s;
    int32_t ftb;
    /*In order to optimize EC_ILOG(), it is undefined for the value 0.*/
    assert(_ft > 1);
    _ft--;
    ftb = EC_ILOG(_ft);
    if (ftb > EC_UINT_BITS) {
        uint32_t t;
        ftb -= EC_UINT_BITS;
        ft = (uint32_t)(_ft >> ftb) + 1;
        s = decode(ft);
        dec_update(s, s + 1, ft);
        t = (uint32_t)s << ftb | dec_bits(ftb);
        if (t <= _ft) return t;
        m_error = 1;
        return _ft;
    } else {
        _ft++;
        s = decode((uint32_t)_ft);
        dec_update(s, s + 1, (uint32_t)_ft);
        return s;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t RangeDecoder::dec_bits(uint32_t _bits) {
    uint32_t window;
    int32_t available;
    uint32_t ret;
    window = m_end_window;
    available = m_nend_bits;
    if ((uint32_t)available < _bits) {
        do {
            window |= (uint32_t)read_byte_from_end() << available;
            available += EC_SYM_BITS;
        } while (available <= EC_WINDOW_SIZE - EC_SYM_BITS);
    }
    ret = (uint32_t)window & (((uint32_t)1 << _bits) - 1U);
    window >>= _bits;
    available -= _bits;
    m_end_window = window;
    m_nend_bits = available;
    m_nbits_total += _bits;
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t RangeDecoder::tell(){return m_nbits_total-EC_ILOG(m_rng);}
void RangeDecoder::add_nbits_total(int32_t nbits_total){m_nbits_total += nbits_total;}
uint32_t RangeDecoder::get_storage(){return m_storage;}
int32_t RangeDecoder::get_error(){return m_error;}
uint32_t RangeDecoder::get_rng(){return m_rng;}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* When called, decay is positive and at most 11456. */
uint32_t RangeDecoder::laplace_get_freq1(uint32_t fs0, int32_t decay) {
    uint32_t ft;
    ft = 32768 - LAPLACE_MINP * (2 * LAPLACE_NMIN) - fs0;
    return ft * (int32_t)(16384 - decay) >> 15;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t RangeDecoder::laplace_decode(uint32_t fs, int32_t decay) {
    int32_t val = 0;
    uint32_t fl;
    uint32_t fm;
    fm = decode_bin(15);
    fl = 0;
    if (fm >= fs) {
        val++;
        fl = fs;
        fs = laplace_get_freq1(fs, decay) + LAPLACE_MINP;
        /* Search the decaying part of the PDF.*/
        while (fs > LAPLACE_MINP && fm >= fl + 2 * fs) {
            fs *= 2;
            fl += fs;
            fs = ((fs - 2 * LAPLACE_MINP) * (int32_t)decay) >> 15;
            fs += LAPLACE_MINP;
            val++;
        }
        /* Everything beyond that has probability LAPLACE_MINP. */
        if (fs <= LAPLACE_MINP) {
            int32_t di;
            di = (fm - fl) >> (LAPLACE_LOG_MINP + 1);
            val += di;
            fl += 2 * di * LAPLACE_MINP;
        }
        if (fm < fl + fs)
            val = -val;
        else
            fl += fs;
    }
    assert(fl < 32768);
    assert(fs > 0);
    assert(fl <= fm);
    assert(fm < min((uint32_t)(fl + fs), (uint32_t)32768));
    dec_update(fl, min((uint32_t)(fl + fs), (uint32_t)32768), (uint32_t)32768);
    return val;
}