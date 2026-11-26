#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef enum {
    REJECT_NONE,
    REJECT_CHARGING,
    REJECT_AFE,
    REJECT_MATH,
} hs_reject_e;

struct result {
    hs_reject_e reject;
    int16_t temp_centi_c;
    int16_t a_rms_mm_s2;
    int16_t battery_mv;
};

#endif  // TYPES_H
