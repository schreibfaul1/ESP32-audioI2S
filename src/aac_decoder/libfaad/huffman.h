#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__
#ifdef __cplusplus
extern "C" {
#endif
int8_t huffman_scale_factor(bitfile *ld);
uint8_t huffman_spectral_data(uint8_t cb, bitfile *ld, int16_t *sp);
#ifdef ERROR_RESILIENCE
int8_t huffman_spectral_data_2(uint8_t cb, bits_t *ld, int16_t *sp);
#endif
#ifdef __cplusplus
}
#endif
#endif
