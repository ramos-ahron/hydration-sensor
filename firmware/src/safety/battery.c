#include "safety/battery.h"
#include "hs/thresholds.h"

battery_level_e batt_classify(battery_level_e prev, int mv)
{
    if (mv < 0) { return prev; }
    if (mv < BATT_CRITICAL_MV ||
        (prev == BATTERY_LEVEL_CRITICAL && mv < BATT_CRITICAL_MV + BATT_MARGIN_MV)) {
        return BATTERY_LEVEL_CRITICAL;
    }
    if (mv < BATT_LOW_MV || (prev != BATTERY_LEVEL_OK && mv < BATT_LOW_MV + BATT_MARGIN_MV)) {
        return BATTERY_LEVEL_LOW;
    }
    return BATTERY_LEVEL_OK;
}
