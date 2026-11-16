#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "fsm/state_common.h"
#include <zephyr/kernel.h>

/**
 * @brief Every event source puts state_event_e values here.
 */
extern struct k_msgq fsm_msgq;

typedef enum {
    FSM_TIMER_INIT,
    FSM_TIMER_SLEEP_GUARD,
    FSM_TIMER_MEASURE,
    FSM_TIMER_PAIRING,
    FSM_TIMER_COUNT,
} fsm_timer_e;

/**
 * @brief Post an event to the state machine. The function does not block.
 *
 * @param[in] event  Event to post. When the queue is full, the function drops the event and logs a
 *                   warning.
 */

/**
 * @brief Initialize the state machine.
 */
void state_machine_init(void);

/**
 * @brief Process an event and make the related state transition.
 *
 * When a stub event is set for a short or long button event, the function uses the stub event in
 * place of the button event. In the ACTIVE state, the active regions get the event first. If they
 * do not use it, the function finds the matching state and event in the transition table and
 * enters the target state. The function ignores an event that has no matching transition.
 *
 * @param[in] event  Event to process.
 */
void state_machine_process_event(state_event_e event);

/**
 * @brief Post @p event after @p ms.
 *
 * @param[in] timer  Timer to start.
 * @param[in] event  Event to post when the timer expires.
 * @param[in] ms     Delay in ms.
 */
void fsm_timer_start(fsm_timer_e timer, state_event_e event, uint32_t ms);

/**
 * @brief Cancel a timer.
 *
 * @param[in] timer  Timer to cancel.
 */
void fsm_timer_stop(fsm_timer_e timer);

/**
 * @brief Set the shared stub timer and the button event mappings.
 *
 * The function cancels the stub timer before it sets the new events. Use STATE_EVENT_NONE for an
 * argument that is not used, or for an event that the transition table handles. This function is
 * a temporary helper. Remove it when the real state logic is available.
 *
 * @param[in] timed_ev  Event to post after FSM_STUB_DWELL_MS.
 * @param[in] short_ev  Event to use in place of BUTTON_SHORT. This event does not wait for the
 *                      timer.
 * @param[in] long_ev   Event to use in place of BUTTON_LONG, usually for a fault or a different
 *                      transition.
 */
void fsm_stub(state_event_e timed_ev, state_event_e short_ev, state_event_e long_ev);

#endif  // STATE_MACHINE_H
