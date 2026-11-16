#ifndef STATE_ACTIVE_H
#define STATE_ACTIVE_H

#include "fsm/state_common.h"
#include <stdbool.h>

/**
 * @brief Initialize all regions of the active state.
 *
 * The function enters the initial substate of each region.
 *
 * @param[in] from  Previous top-level state.
 */
void state_active_run(state_e from);

/**
 * @brief Send an event to the regions of the active state.
 *
 * When BLE is pairing, only the BLE region gets BUTTON_SHORT. The active state blocks SLEEP_GUARD
 * when the measurement is not idle or when BLE is pairing. Every other event goes to the
 * measurement, BLE and battery monitor regions, in that order.
 *
 * @param[in] event  Event to process.
 *
 * @return true when a region made a transition or the active state blocked the event, and false
 *         when neither occurred.
 */
bool state_active_process_event(state_event_e event);

#endif  // STATE_ACTIVE_H
