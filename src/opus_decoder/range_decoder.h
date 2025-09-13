#include "Arduino.h"
#pragma once

#define EC_WINDOW_SIZE ((int32_t)sizeof(uint32_t)*CHAR_BIT)
#define EC_UINT_BITS   (8)
#define EC_MINI(_a,_b)      ((_a)+(((_b)-(_a))&-((_b)<(_a))))
#define EC_CLZ0s    ((int32_t)sizeof(uint32_t)*CHAR_BIT)
#define EC_CLZ(_x) (__builtin_clz(_x))
#define EC_ILOG(_x) (EC_CLZ0s-EC_CLZ(_x))
#define EC_BITRES 3

typedef struct _ec_ctx {
    uint8_t *buf; /*Buffered input/output.*/
    uint32_t storage; /*The size of the buffer.*/
    uint32_t end_offs; /*The offset at which the last byte containing raw bits was read/written.*/
    uint32_t end_window; /*Bits that will be read from/written at the end.*/
    int32_t nend_bits; /*Number of valid bits in end_window.*/
    int32_t nbits_total;
    uint32_t offs; /*The offset at which the next range coder byte will be read/written.*/
    uint32_t rng; /*The number of values in the current range.*/
    uint32_t val;
    uint32_t ext;
    int32_t rem; /*A buffered input/output symbol, awaiting carry propagation.*/
    int32_t error; /*Nonzero if an error occurred.*/
} ec_ctx_t;

int32_t ec_tell();
uint32_t ec_tell_frac();
int32_t ec_read_byte();
void ec_dec_normalize();
void ec_dec_init(uint8_t *_buf, uint32_t _storage);
uint32_t ec_decode(uint32_t _ft);
uint32_t ec_decode_bin(uint32_t _bits);
void ec_dec_update(uint32_t _fl, uint32_t _fh, uint32_t _ft);
int32_t ec_dec_bit_logp( uint32_t _logp);
int32_t ec_dec_icdf(const uint8_t *_icdf, uint32_t _ftb);
uint32_t ec_dec_uint(uint32_t _ft);
uint32_t ec_dec_bits(uint32_t _bits);
void ec_add_nbits_total(int32_t nbits_total);
uint32_t ec_get_storage();
int32_t ec_get_error();
uint32_t ec_get_rng();