#include "range_decoder.h"



ec_ctx_t s_ec;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* This is a faster version of ec_tell_frac() that takes advantage of the low (1/8 bit) resolution to use just a linear function followed by a lookup to determine the exact transition thresholds. */
uint32_t ec_tell_frac() {
    const uint32_t correction[8] = {35733, 38967, 42495, 46340, 50535, 55109, 60097, 65535};
    uint32_t nbits;
    uint32_t r;
    int32_t l;
    uint32_t b;
    nbits = s_ec.nbits_total << EC_BITRES;
    l = EC_ILOG(s_ec.rng);
    r = s_ec.rng >> (l - 16);
    b = (r >> 12) - 8;
    b += r > correction[b];
    l = (l << 3) + b;
    return nbits - l;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t ec_read_byte() { return s_ec.offs < s_ec.storage ? s_ec.buf[s_ec.offs++] : 0; }
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t ec_read_byte_from_end() {
    return s_ec.end_offs < s_ec.storage ? s_ec.buf[s_ec.storage - ++(s_ec.end_offs)] : 0;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*Normalizes the contents of val and rng so that rng lies entirely in the high-order symbol.*/
void ec_dec_normalize() {
    /*If the range is too small, rescale it and input some bits.*/
    while (s_ec.rng <= EC_CODE_BOT) {
        int32_t sym;
        s_ec.nbits_total += EC_SYM_BITS;
        s_ec.rng <<= EC_SYM_BITS;
        /*Use up the remaining bits from our last symbol.*/
        sym = s_ec.rem;
        /*Read the next value from the input.*/
        s_ec.rem = ec_read_byte();
        /*Take the rest of the bits we need from this new symbol.*/
        sym = (sym << EC_SYM_BITS | s_ec.rem) >> (EC_SYM_BITS - EC_CODE_EXTRA);
        /*And subtract them from val, capped to be less than EC_CODE_TOP.*/
        s_ec.val = ((s_ec.val << EC_SYM_BITS) + (EC_SYM_MAX & ~sym)) & ((EC_CODE_TOP) - 1);
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void ec_dec_init(uint8_t *_buf, uint32_t _storage) {

    s_ec.buf = _buf;
    s_ec.storage = _storage;
    s_ec.end_offs = 0;
    s_ec.end_window = 0;
    s_ec.nend_bits = 0;
    s_ec.nbits_total = EC_CODE_BITS + 1 - ((EC_CODE_BITS - EC_CODE_EXTRA) / EC_SYM_BITS) * EC_SYM_BITS;
    s_ec.offs = 0;
    s_ec.rng = 1U << EC_CODE_EXTRA;
    s_ec.rem = ec_read_byte();
    s_ec.val = s_ec.rng - 1 - (s_ec.rem >> (EC_SYM_BITS - EC_CODE_EXTRA));
    s_ec.error = 0;
    /*Normalize the interval.*/
    ec_dec_normalize();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t ec_decode(uint32_t _ft) {
    uint32_t s;
    s_ec.ext = s_ec.rng / _ft;
    s = (uint32_t)(s_ec.val / s_ec.ext);
    return _ft - EC_MINI(s + 1, _ft);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t ec_decode_bin(uint32_t _bits) {
    uint32_t s;
    s_ec.ext = s_ec.rng >> _bits;
    s = (uint32_t)(s_ec.val / s_ec.ext);
    return (1U << _bits) - EC_MINI(s + 1U, 1U << _bits);
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void ec_dec_update(uint32_t _fl, uint32_t _fh, uint32_t _ft) {
    uint32_t s;
    s = s_ec.ext *  (_ft - _fh);
    s_ec.val -= s;

    if(_fl > 0){
        s_ec.rng = s_ec.ext * (_fh - _fl);
    }
    else{
        s_ec.rng = s_ec.rng - s;
    }
    ec_dec_normalize();
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/*The probability of having a "one" is 1/(1<<_logp).*/
int32_t ec_dec_bit_logp( uint32_t _logp) {
    uint32_t r;
    uint32_t d;
    uint32_t s;
    int32_t ret;
    r = s_ec.rng;
    d = s_ec.val;
    s = r >> _logp;
    ret = d < s;
    if (!ret) s_ec.val = d - s;
    s_ec.rng = ret ? s : r - s;
    ec_dec_normalize();
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t ec_dec_icdf(const uint8_t *_icdf, uint32_t _ftb) {
    uint32_t r;
    uint32_t d;
    uint32_t s;
    uint32_t t;
    int32_t ret;
    s = s_ec.rng;
    d = s_ec.val;
    r = s >> _ftb;
    ret = -1;
    do {
        t = s;
        s = r * _icdf[++ret];
    } while (d < s);
    s_ec.val = d - s;
    s_ec.rng = t - s;
    ec_dec_normalize();
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t ec_dec_uint(uint32_t _ft) {
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
        s = ec_decode(ft);
        ec_dec_update(s, s + 1, ft);
        t = (uint32_t)s << ftb | ec_dec_bits(ftb);
        if (t <= _ft) return t;
        s_ec.error = 1;
        return _ft;
    } else {
        _ft++;
        s = ec_decode((uint32_t)_ft);
        ec_dec_update(s, s + 1, (uint32_t)_ft);
        return s;
    }
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint32_t ec_dec_bits(uint32_t _bits) {
    uint32_t window;
    int32_t available;
    uint32_t ret;
    window = s_ec.end_window;
    available = s_ec.nend_bits;
    if ((uint32_t)available < _bits) {
        do {
            window |= (uint32_t)ec_read_byte_from_end() << available;
            available += EC_SYM_BITS;
        } while (available <= EC_WINDOW_SIZE - EC_SYM_BITS);
    }
    ret = (uint32_t)window & (((uint32_t)1 << _bits) - 1U);
    window >>= _bits;
    available -= _bits;
    s_ec.end_window = window;
    s_ec.nend_bits = available;
    s_ec.nbits_total += _bits;
    return ret;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* When called, decay is positive and at most 11456. */
uint32_t laplace_get_freq1(uint32_t fs0, int32_t decay) {
    uint32_t ft;
    ft = 32768 - LAPLACE_MINP * (2 * LAPLACE_NMIN) - fs0;
    return ft * (int32_t)(16384 - decay) >> 15;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t laplace_decode(uint32_t fs, int32_t decay) {
    int32_t val = 0;
    uint32_t fl;
    uint32_t fm;
    fm = ec_decode_bin(15);
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
    ec_dec_update(fl, min((uint32_t)(fl + fs), (uint32_t)32768), (uint32_t)32768);
    return val;
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t ec_tell(){return s_ec.nbits_total-EC_ILOG(s_ec.rng);}
void ec_add_nbits_total(int32_t nbits_total){s_ec.nbits_total += nbits_total;}
uint32_t ec_get_storage(){return s_ec.storage;}
int32_t ec_get_error(){return s_ec.error;}
uint32_t ec_get_rng(){return s_ec.rng;}