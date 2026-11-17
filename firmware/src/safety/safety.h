#ifndef SAFETY_H
#define SAFETY_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Battery level. */
typedef enum {
    BATTERY_LEVEL_OK,
    BATTERY_LEVEL_LOW,
    BATTERY_LEVEL_CRITICAL,
} battery_level_e;

/**
 * @brief Start the safety functions.
 *
 * The function starts the task watchdog, arms the hardware watchdog fallback, and starts the
 * periodic battery check. The function logs a warning if the watchdog or the battery ADC is not
 * available.
 *
 * @return 0.
 */
int safety_init(void);

/**
 * @brief Check if external power is connected.
 *
 * @return true while VBUS or charger power is present.
 */
bool safety_vbus_present(void);

/**
 * @brief Check if a measurement is safe.
 *
 * A measurement is not safe while the device is charging.
 *
 * @retval 0       The measurement can start.
 * @retval -EPERM  The device is charging and CONFIG_BENCH_ALLOW_USB is off.
 */
int safety_check(void);

/**
 * @brief Start the drive watchdog.
 */
void safety_drive_begin(void);

/**
 * @brief Feed the drive watchdog.
 *
 * Call this at least every DRIVE_WDT_MS.
 */
void safety_drive_feed(void);

/**
 * @brief Stop the drive watchdog.
 */
void safety_drive_end(void);

/**
 * @brief Check the cause of the last reset.
 *
 * @return true if the watchdog caused the last reset.
 */
bool safety_reset_was_watchdog(void);

/**
 * @brief Get the last battery voltage.
 *
 * @return Battery voltage in mV. A negative value means the voltage is not known.
 */
int battery_mv(void);

/**
 * @brief Get the battery level
 *
 * @return The current battery level.
 */
battery_level_e battery_level(void);

/**
 * @brief Check if the voltage is high enough to leave SHUTDOWN.
 *
 * @return true if the voltage is at or above the critical threshold plus the margin, or if the
 *         voltage is not known.
 */
bool battery_above_margin(void);

#endif  // SAFETY_H
