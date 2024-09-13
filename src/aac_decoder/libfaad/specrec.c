
/*
  Spectral reconstruction:
   - grouping/sectioning
   - inverse quantization
   - applying scalefactors
*/
#include "Arduino.h"
#include "common.h"
#include "structs.h"
#include <string.h>
#include <stdlib.h>


#ifdef SSR_DEC
#include "ssr.h"
#include "ssr_fb.h"
#endif
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* static function declarations */

