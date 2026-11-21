#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef enum {
    REJECT_NONE,
    REJECT_CHARGING,
    REJECT_AFE,
    REJECT_MATH,
} reject_e;

struct result {
    reject_e reject;
    int16_t temp_centi_c; /**< get_temp() in 0.01 C. INT16_MIN if not available. */
    int16_t a_rms_mm_s2;  /**< get_motion() in mm/s^2. INT16_MIN if not available. */
    int16_t battery_mv;   /**< battery_mv() in mV. INT16_MIN if not available. */
};

#endif  // TYPES_H
