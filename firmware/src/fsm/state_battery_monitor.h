#ifndef STATE_BATTERY_MONITOR_H
#define STATE_BATTERY_MONITOR_H

#include "fsm/state_common.h"
#include <stdbool.h>

// TODO: Is this needed? Just set timer?

/**
 * @brief Defines the states of the battery monitoring state machine.
 */
typedef enum {
    BATTERY_STATE_BATTERY_OK,
    BATTERY_STATE_BATTERY_LOW,
    BATTERY_STATE_BATTERY_CRITICAL,
} battery_state_e;

/**
 * @brief Initialize the battery monitor state machine.
 *
 * The function sets the initial battery monitor state and does the necessary setup.
 */
void state_battery_monitor_run(void);

/**
 * @brief Process an event in the battery monitor state machine.
 *
 * The function compares the event and the current battery state with the transition table. If a
 * transition matches, the function makes that transition.
 *
 * @param[in] event  Event to process.
 *
 * @return true when the event caused a transition, and false when it did not.
 */
bool state_battery_monitor_process_event(state_event_e event);

#endif  // STATE_BATTERY_MONITOR_H
