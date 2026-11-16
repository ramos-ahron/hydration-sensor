#ifndef LED_H
#define LED_H

#include <stdint.h>

/**
 * @brief Defines the available LED colors.
 */
typedef enum {
    LED_OFF = 0,
    LED_RED = 1,
    LED_GREEN = 2,
    LED_YELLOW = 3, /**< Red + green. */
    LED_BLUE = 4,
    LED_MAGENTA = 5, /**< Red + blue. */
    LED_CYAN = 6,    /**< Green + blue. */
    LED_WHITE = 7,   /**< Red + green + blue. */
} led_color_e;

/**
 * @brief Defines possible LED patterns
 */
typedef enum {
    LED_PAT_STEADY,
    LED_PAT_SLOW,
    LED_PAT_FAST,
    LED_PAT_TRIPLE,
    LED_PAT_BEAT,
} led_pattern_e;

/**
 * @brief Priority slots
 */
typedef enum {
    LED_PRIO_FAULT,
    LED_PRIO_STORAGE,
    LED_PRIO_BLE,
    LED_PRIO_MEASURE,
    LED_PRIO_RESULT,
    LED_PRIO_BATTERY,
    LED_PRIO_POWER,
    LED_PRIO_COUNT,
} led_prio_e;

/**
 * @brief Sets the LED to the specified color.
 *
 * @param color Color to display.
 */
void led_set(led_color_e color);

/**
 * @brief Posts or replaces LED request in @p prio
 *
 * @param period_ms BEAT interval
 * @param duration_ms The reuest clears itself after this long
 */
void led_request(led_prio_e prio, led_color_e color, led_pattern_e pattern, uint32_t period_ms,
                 uint32_t duration_ms);

/**
 * @brief Clears the request in @p prio
 */
void led_clear(led_prio_e prio);

/**
 * @brief Turn the LED off
 */
void led_clear_all(void);

/**
 * @brief Current LED color
 */
led_color_e led_current(void);

#endif  // LED_H
