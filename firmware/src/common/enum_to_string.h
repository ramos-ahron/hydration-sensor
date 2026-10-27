#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H

#include "fsm/state_common.h"
#include "ui/led.h"

/**
 * @brief Provides string representations of enums for logging.
 *
 * When CONFIG_FSM_ENUM_STRINGS is disabled, the conversion functions
 * are replaced with inline stubs that return an empty string.
 */
#ifndef CONFIG_FSM_ENUM_STRINGS

/**
 * @brief Stub enum-to-string conversion.
 *
 * @param val Enum value to convert; unused.
 * @return An empty string.
 */
static inline const char *empty_func(unsigned int val)
{
    (void)val;
    return "";
}

#define state_to_string(state) empty_func(state)
#define state_event_to_string(event) empty_func(event)
#define led_color_to_string(color) empty_func(color)

#else

const char *state_to_string(state_e state);
const char *state_event_to_string(state_event_e event);
const char *led_color_to_string(led_color_e color);

#endif

#endif // ENUM_TO_STRING_H
