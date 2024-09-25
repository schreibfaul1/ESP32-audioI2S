

#include "common.h"


#ifdef ERROR_RESILIENCE
/* Modified bit reading functions for HCR */
typedef struct {
    /* bit input */
    uint32_t bufa;
    uint32_t bufb;
    int8_t   len;
} bits_t;
#endif /*ERROR_RESILIENCE*/