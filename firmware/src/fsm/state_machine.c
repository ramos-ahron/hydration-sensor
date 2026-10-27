#include "fsm/state_init.h"
#include "fsm/state_common.h"
#include "fsm/state_active.h"
#include "fsm/state_machine.h"
#include "fsm/state_init_fault.h"
#include "fsm/state_sleep_prep.h"
#include "fsm/state_sleep.h"
#include "fsm/state_shutdown.h"
#include "common/enum_to_string.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fsm, LOG_LEVEL_INF);

/**
 * @brief Defines the event-driven state machine message queue.
 *
 * Events are posted to @c fsm_msgq by button callbacks, timers, or state
 * logic. The main loop blocks on the queue and passes received events to
 * state_machine_process_event(), which looks up the transition and enters
 * the resulting state.
 *
 * @note The queue holds up to 8 events, each of type @c state_event_e.
 */
K_MSGQ_DEFINE(fsm_msgq, sizeof(state_event_e), 8, 4);

struct state_transition {
    state_e from;
    state_event_e event;
    state_e to;
};

static const struct state_transition state_transitions[] = {
    { STATE_INIT, STATE_EVENT_INIT_OK, STATE_ACTIVE },
    { STATE_INIT, STATE_EVENT_INIT_FAIL, STATE_INIT_FAULT },
    { STATE_INIT, STATE_EVENT_WATCHDOG_RESET, STATE_INIT },
    { STATE_INIT_FAULT, STATE_EVENT_BUTTON_SHORT, STATE_INIT },
    { STATE_ACTIVE, STATE_EVENT_SLEEP_GUARD, STATE_SLEEP_PREP },
    { STATE_ACTIVE, STATE_EVENT_BATTERY_CRITICAL, STATE_SHUTDOWN },
    { STATE_SLEEP_PREP, STATE_EVENT_PREP_COMPLETE, STATE_SLEEP },
    { STATE_SLEEP, STATE_EVENT_BUTTON_SHORT, STATE_INIT },
    { STATE_SLEEP, STATE_EVENT_BATTERY_CRITICAL, STATE_SHUTDOWN },
    { STATE_SHUTDOWN, STATE_EVENT_BATTERY_MARGIN_OK, STATE_INIT },
};

static state_e state;

// --------------------- STUBS -----------------------------------

/**
 * @brief Temporary stub infrastructure for state event simulation.
 *
 * Uses a shared delayable work item to post a timed event and stores the
 * events mapped to short and long button presses.
 *
 * @note Remove these variables when the fsm_stub() calls are replaced
 *       with real state logic.
 */
static struct k_work_delayable stub_work;
static state_event_e stub_timed = STATE_EVENT_NONE;
static state_event_e stub_short = STATE_EVENT_NONE;
static state_event_e stub_long = STATE_EVENT_NONE;

/**
 * @brief Handles expiration of the stub timer.
 *
 * Posts the configured timed event to the FSM message queue without
 * blocking. Does nothing if the queue is full.
 *
 * @param work Pointer to the work item; unused.
 */
static void stub_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    state_event_e event = stub_timed;
    (void)k_msgq_put(&fsm_msgq, &event, K_NO_WAIT);
}

/**
 * @brief Cancels the stub timer and posts an event to the FSM queue.
 *
 * Clears all configured stub events before posting @p event to prevent
 * further stub events from being scheduled.
 *
 * @param event Event to post to the FSM message queue.
 *
 * @note The event is discarded if the queue is full.
 */
static void post_stub_event(state_event_e event)
{
    // Clear timers and configurations
    fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
    
    // To be handled in main
    (void)k_msgq_put(&fsm_msgq, &event, K_NO_WAIT);
}

void fsm_stub(state_event_e timed_ev, state_event_e short_ev, state_event_e long_ev)
{
    static struct k_work_sync sync;

    (void)k_work_cancel_delayable_sync(&stub_work, &sync);
    stub_timed = timed_ev;
    stub_short = short_ev;
    stub_long = long_ev;
    if (timed_ev != STATE_EVENT_NONE) {
        (void)k_work_schedule(&stub_work, K_MSEC(FSM_STUB_DWELL_MS));
    }
}

// --------------------- STUBS -----------------------------------

static void state_enter(state_e from, state_event_e event, state_e to)
{
    // No Logic Yet
    if (from == to) {
        return;
    }

    LOG_INF("%s -> %s (%s)", state_to_string(from), state_to_string(to), 
            state_event_to_string(event));
    state = to;
    
    // Clear timers and configurations
    fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);

    switch (to) {
        case STATE_INIT:
            state_init_run(from);
            break;

        case STATE_INIT_FAULT:
            state_init_fault_run(from);
            break;

        case STATE_ACTIVE:
            state_active_run(from);
            break;

        case STATE_SLEEP_PREP:
            state_sleep_prep_run(from);
            break;

        case STATE_SLEEP:
            state_sleep_run(from);
            break;

        case STATE_SHUTDOWN:
            state_shutdown_run(from);
            break;
    }
}

void state_machine_init(void)
{
    k_work_init_delayable(&stub_work, stub_work_handler);
    state = STATE_INIT;
    LOG_INF("start -> %s", state_to_string(state));
    state_init_run(STATE_INIT);
}

void state_machine_process_event(state_event_e event)
{
    // Replace button event with the actual event and queue it again.
    if (event == STATE_EVENT_BUTTON_SHORT && stub_short != STATE_EVENT_NONE) {
        post_stub_event(stub_short);
        return;
    }
    if (event == STATE_EVENT_BUTTON_LONG && stub_long != STATE_EVENT_NONE) {
        post_stub_event(stub_long);
        return;
    }

    if (state == STATE_ACTIVE && state_active_process_event(event)) {
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(state_transitions); i ++) {
        if (state == state_transitions[i].from && event == state_transitions[i].event) {
            state_enter(state_transitions[i].from, event, state_transitions[i].to);
            return;
        }
    }
}
