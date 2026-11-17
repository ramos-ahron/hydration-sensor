#ifndef BATTERY_H
#define BATTERY_H

#include "safety/safety.h"

/**
 * @brief Find the battery level from a voltage reading, with hysteresis.
 *
 * To leave a lower level, the voltage must be above the threshold plus BATT_MARGIN_MV.
 * This stops the level from changing back and forth near a threshold.
 *
 * @param[in] prev  Level from the last reading.
 * @param[in] mv    Battery voltage in mV. A negative value means the reading is not known.
 *
 * @return The new level. For an unknown reading, the function returns @p prev.
 */
battery_level_e batt_classify(battery_level_e prev, int mv);

#endif /* BATTERY_H */
