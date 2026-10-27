#ifndef LED_H
#define LED_H

/**
 * @brief Defines the available LED colors.
 */
typedef enum {
    LED_OFF = 0,
    LED_RED = 1,
    LED_GREEN = 2,
    LED_YELLOW = 3,  /**< Red + green. */
    LED_BLUE = 4,
    LED_MAGENTA = 5, /**< Red + blue. */
    LED_CYAN = 6,    /**< Green + blue. */
    LED_WHITE = 7,   /**< Red + green + blue. */
} led_color_e;

/**
 * @brief Sets the LED to the specified color.
 *
 * @param color Color to display.
 */
void led_set(led_color_e color);

#endif // LED_H
