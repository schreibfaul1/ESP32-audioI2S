
#include "common.h"
#include "structs.h"

#include "syntax.h"
#include "pulse.h"
//————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
uint8_t pulse_decode(ic_stream* ics, int16_t* spec_data, uint16_t framelen) {
    uint8_t     i;
    uint16_t    k;
    pulse_info* pul = &(ics->pul);

    k = min(ics->swb_offset[pul->pulse_start_sfb], ics->swb_offset_max);

    for (i = 0; i <= pul->number_pulse; i++) {
        k += pul->pulse_offset[i];

        if (k >= framelen) return 15; /* should not be possible */

        if (spec_data[k] > 0)
            spec_data[k] += pul->pulse_amp[i];
        else
            spec_data[k] -= pul->pulse_amp[i];
    }

    return 0;
}
//————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————