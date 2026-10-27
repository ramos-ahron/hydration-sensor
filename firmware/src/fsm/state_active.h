#ifndef STATE_ACTIVE_H
#define STATE_ACTIVE_H

#include "fsm/state_common.h"
#include <stdbool.h>

/**
 * @brief Initializes all regions of the active state.
 *
 * Enters the initial substate of each region.
 *
 * @param from Previous top-level state.
 */
void state_active_run(state_e from);

/**
 * @brief Dispatches an event to the active state's regions.
 *
 * @param event Event to process.
 * @return true if a nested state machine handled the event; false otherwise.
 */
bool state_active_process_event(state_event_e event);

#endif // STATE_ACTIVE_H