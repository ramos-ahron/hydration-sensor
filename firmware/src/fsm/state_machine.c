#include "fsm/state_machine.h"
#include "common/enum_to_string.h"
#include "fsm/state_active.h"
#include "fsm/state_common.h"
#include "fsm/state_init.h"
#include "fsm/state_init_fault.h"
#include "fsm/state_shutdown.h"
#include "fsm/state_sleep.h"
#include "fsm/state_sleep_prep.h"
#include "safety/safety.h"
#include "ui/led.h"
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
 * @note The queue holds up to 16 events, each of type @c state_event_e.
 */
K_MSGQ_DEFINE(fsm_msgq, sizeof(state_event_e), 16, 4);

/** Delay before a stub event that hit a full queue is posted again. */
#define STUB_QUEUE_RETRY_MS 50

struct state_transition {
    state_e from;
    state_event_e event;
    state_e to;
};

static const struct state_transition state_transitions[] = {
    { STATE_INIT, STATE_EVENT_INIT_OK, STATE_ACTIVE },
    { STATE_INIT, STATE_EVENT_INIT_FAIL, STATE_INIT_FAULT },
    { STATE_INIT, STATE_EVENT_BATTERY_CRITICAL, STATE_SHUTDOWN },
    { STATE_INIT_FAULT, STATE_EVENT_BUTTON_SHORT, STATE_INIT },
    { STATE_ACTIVE, STATE_EVENT_SLEEP_GUARD, STATE_SLEEP_PREP },
    { STATE_ACTIVE, STATE_EVENT_BATTERY_CRITICAL, STATE_SHUTDOWN },
    { STATE_SLEEP_PREP, STATE_EVENT_PREP_COMPLETE, STATE_SLEEP },
    { STATE_SLEEP, STATE_EVENT_BUTTON_SHORT, STATE_INIT },
    { STATE_SLEEP, STATE_EVENT_BATTERY_CRITICAL, STATE_SHUTDOWN },
    { STATE_SHUTDOWN, STATE_EVENT_BATTERY_MARGIN_OK, STATE_INIT },
};

static state_e state;

struct fsm_timer_slot {
    struct k_work_delayable dwork;
    state_event_e event;
};

static struct fsm_timer_slot timers[FSM_TIMER_COUNT];

void fsm_post(state_event_e event)
{
    if (k_msgq_put(&fsm_msgq, &event, K_NO_WAIT) != 0) {
        LOG_WRN("FSM queue full, dropped %s", state_event_to_string(event));
    }
}

/**
 * @brief Post the event of an FSM timer when the timer expires.
 *
 * The system work queue thread runs this function. The function finds the timer slot that holds
 * @p work and posts the event of that slot.
 *
 * @param[in] work  Work item of the timer that expired.
 */
static void timer_handler(struct k_work *work)
{
    // TODO: Restarting a timer as it expires can post the new event early. Cancel before restart.

    /**
     * Given a pointer to a member of the struct. It gives back the pointer to the struct.
     */
    const struct fsm_timer_slot *slot =
        CONTAINER_OF(k_work_delayable_from_work(work), struct fsm_timer_slot, dwork);

    // Post the event tied to the expired timer.
    fsm_post(slot->event);
}

void fsm_timer_start(fsm_timer_e timer, state_event_e event, uint32_t ms)
{
    timers[timer].event = event;
    (void)k_work_reschedule(&timers[timer].dwork, K_MSEC(ms));
}

void fsm_timer_stop(fsm_timer_e timer) { (void)k_work_cancel_delayable(&timers[timer].dwork); }

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
 * @brief Post the timed stub event when the stub timer expires.
 *
 * The function posts the timed event to the FSM message queue and does not block. If the queue is
 * full, the function logs a warning and tries again after STUB_QUEUE_RETRY_MS. The retry prevents
 * a stall of the state machine while it waits for the event.
 *
 * @param[in] work  Work item. The function does not use this value.
 */
static void stub_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    state_event_e event = stub_timed;
    if (k_msgq_put(&fsm_msgq, &event, K_NO_WAIT) != 0) {
        LOG_WRN("FSM queue full, retrying %s", state_event_to_string(event));
        (void)k_work_reschedule(&stub_work, K_MSEC(STUB_QUEUE_RETRY_MS));
    }
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

/**
 * @brief Make a top-level state transition.
 *
 * When @p from and @p to are the same, the function does nothing. Otherwise, the function logs
 * the transition, stops every timer and clears ever LED request of the state being left,
 * stores @p to as the current state. Then it calls the run function of the new state with @p from.
 *
 * @param[in] from   State that the machine leaves.
 * @param[in] event  Event that caused the transition. Only the log uses this value.
 * @param[in] to     State that the machine enters.
 */
static void state_enter(state_e from, state_event_e event, state_e to)
{
    LOG_INF("%s -> %s (%s)", state_to_string(from), state_to_string(to),
            state_event_to_string(event));
    state = to;

    // Clear timers and configurations
    fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
    for (int i = 0; i < FSM_TIMER_COUNT; i++) {
        fsm_timer_stop(i);
    }
    led_clear_all();

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
    for (int i = 0; i < FSM_TIMER_COUNT; i++) {
        k_work_init_delayable(&timers[i].dwork, timer_handler);
    }
    state = STATE_INIT;
    LOG_INF("start -> %s", state_to_string(state));
    state_init_run(STATE_INIT);
}

void state_machine_process_event(state_event_e event)
{
    if (event == STATE_EVENT_BUTTON_SHORT && stub_short != STATE_EVENT_NONE) {
        event = stub_short;
        fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
    } else if (event == STATE_EVENT_BUTTON_LONG && stub_long != STATE_EVENT_NONE) {
        event = stub_long;
        fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
    }

    if (event == STATE_EVENT_BATTERY_CHANGED) {
        if ((state == STATE_INIT || state == STATE_ACTIVE || state == STATE_SLEEP) &&
            battery_level() == BATTERY_LEVEL_CRITICAL) {
            event = STATE_EVENT_BATTERY_CRITICAL;
        } else if (state == STATE_SHUTDOWN && event == STATE_EVENT_BUTTON_SHORT) {
            led_request(LED_PRIO_FAULT, LED_RED, LED_PAT_FAST, 0, 2000);
            return;
        }
    }

    if (state == STATE_ACTIVE && state_active_process_event(event)) { return; }

    for (size_t i = 0; i < ARRAY_SIZE(state_transitions); i++) {
        if (state == state_transitions[i].from && event == state_transitions[i].event) {
            state_enter(state_transitions[i].from, event, state_transitions[i].to);
            return;
        }
    }
}
