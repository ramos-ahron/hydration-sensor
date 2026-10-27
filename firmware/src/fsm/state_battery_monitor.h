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
 * @brief Initializes the battery monitoring state machine.
 *
 * Sets the initial battery monitoring state and performs any required
 * setup.
 */
void state_battery_monitor_run(void);

/**
 * @brief Processes an event in the battery monitoring state machine.
 *
 * Evaluates the event against the current battery state and performs
 * a state transition if a matching transition exists.
 *
 * @param event Event to process.
 * @return true if the event was handled; false otherwise.
 */
bool state_battery_monitor_process_event(state_event_e event);

#endif // STATE_BATTERY_MONITOR_H