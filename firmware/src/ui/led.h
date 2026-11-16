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
 * @brief Set the LED to a color.
 *
 * @param[in] color  Color to show.
 */
void led_set(led_color_e color);

/**
 * @brief Set the LED request for one priority.
 *
 * The new request replaces any request that is already in the slot for @p prio. The pattern
 * starts at the time of this call. The function ignores a @p prio that is not valid.
 *
 * @param[in] prio         Priority slot. A slot with a lower number has a higher priority.
 * @param[in] color        Color to show during the "on" phase of the pattern.
 * @param[in] pattern      Blink pattern.
 * @param[in] period_ms    Time between flashes in ms. Only LED_PAT_BEAT uses this value.
 * @param[in] duration_ms  Time in ms after which the request ends. Use 0 to keep the request
 *                         until led_clear() is called.
 */
void led_request(led_prio_e prio, led_color_e color, led_pattern_e pattern, uint32_t period_ms,
                 uint32_t duration_ms);

/**
 * @brief Remove the LED request for one priority.
 *
 * The LED shows the next active slot of lower priority, or turns off if there is none. The
 * function ignores a @p prio that is not valid.
 *
 * @param[in] prio  Priority slot to clear.
 */
void led_clear(led_prio_e prio);

/**
 * @brief Remove all LED requests.
 *
 * The function clears the slot of every priority. The LED turns off.
 */
void led_clear_all(void);

/**
 * @brief Get the color that the LED shows now.
 *
 * @return The color that the last update wrote to the LED pins.
 */
led_color_e led_current(void);

#endif  // LED_H
