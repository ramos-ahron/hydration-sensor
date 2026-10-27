#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H

#include "fsm/state_common.h"
#include "ui/led.h"

// Functions to convert enum to readable string.
#ifndef CONFIG_FSM_ENUM_STRINGS
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
