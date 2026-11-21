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
void fsm_post(state_event_e event);

/**
 * @brief Initialize the state machine.
 */
void state_machine_init(void);

/**
 * @brief Process an event and make the related state transition.
 *
 * In the ACTIVE state, the active regions get the event first. If they do not use it, the function
 * finds the matching state and event in the transition table and enters the target state. The
 * function ignores an event that has no matching transition.
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

#endif  // STATE_MACHINE_H
