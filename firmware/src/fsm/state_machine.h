#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "fsm/state_common.h"
#include <zephyr/kernel.h>

/**
 * @brief Every event source puts state_event_e values here.
 */
extern struct k_msgq fsm_msgq;

/**
 * @brief Posts an event to the state machine without blocking.
 *
 * @param event Event to post. Dropped with a warning if the queue is full.
 */

/**
 * @brief Initializes the state machine
 */
void state_machine_init(void);

/**
 * @brief Processes an event and executes the corresponding state transition.
 *
 * Replaces short and long button events with their configured events
 * when applicable. Otherwise, searches the transition table for a matching
 * state and event, then enters the target state.
 *
 * @param event Event to process.
 *
 * @note Events with no matching transition are ignored.
 */
void state_machine_process_event(state_event_e event);

/**
 * @brief Configures the shared stub timer and button event mappings.
 *
 * Cancels any previously set timer before configuring the new events.
 * Pass STATE_EVENT_NONE for unused slots or events handled by the
 * transition table.
 *
 * @param timed_ev Event posted after FSM_STUB_DWELL_MS.
 * @param short_ev Event posted instead of BUTTON_SHORT, bypassing the wait.
 * @param long_ev Event posted instead of BUTTON_LONG, typically for fault
 *                or alternate transitions.
 *
 * @note Temporary helper; remove once the real state logic is implemented.
 */
void fsm_stub(state_event_e timed_ev, state_event_e short_ev, state_event_e long_ev);

#endif  // STATE_MACHINE_H
