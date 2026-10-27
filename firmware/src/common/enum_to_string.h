#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H

#include "fsm/state_common.h"

/**
 * 
 */
#ifndef CONFIG_FSM_ENUM_STRINGS
static inline const char *empty_func(unsigned int val)
{
    (void)val;
    return "";
}
#define state_event_to_string(event) empty_func(event)
#else
const char *state_event_to_string(state_event_e event);
#endif

#endif // ENUM_TO_STRING_H
