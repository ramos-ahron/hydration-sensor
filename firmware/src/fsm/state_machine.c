#include "common/enum_to_string.h"
#include "fsm/state_active.h"
#include "fsm/state_common.h"
#include "fsm/state_init.h"
#include "fsm/state_init_fault.h"
#include "fsm/state_machine.h"
#include "fsm/state_shutdown.h"
#include "fsm/state_sleep.h"
#include "fsm/state_sleep_prep.h"
#include "safety/safety.h"
#include "ui/led.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fsm, LOG_LEVEL_INF);

/**
 * @brief Message queue of the state machine.
 *
 * Button callbacks, timers and state logic post events to @c fsm_msgq. The main loop waits on
 * the queue and gives each event to state_machine_process_event(). That function finds the
 * transition and enters the new state.
 *
 * @note The queue holds a maximum of 16 events. Each event is a @c state_event_e.
 */
K_MSGQ_DEFINE(fsm_msgq, sizeof(state_event_e), 16, 4);

struct state_transition {
    state_top_e from;
    state_event_e event;
    state_top_e to;
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

static state_top_e state;

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
    // Only FSM_TIMER_INIT has two events, and INIT starts it again only after it expired, so the
    // slot->event read cannot see a new event. An expiry can still put an event in fsm_msgq
    // just before a restart. A cancel cannot remove that event. The worst result is an early
    // sleep, an early measurement or an early end of PAIRING, so the code accepts it.

    /**
     * CONTAINER_OF() gets the timer slot from the pointer to its dwork member.
     */
    const struct fsm_timer_slot *slot =
        CONTAINER_OF(k_work_delayable_from_work(work), struct fsm_timer_slot, dwork);

    fsm_post(slot->event);
}

void fsm_timer_start(fsm_timer_e timer, state_event_e event, uint32_t ms)
{
    timers[timer].event = event;
    (void)k_work_reschedule(&timers[timer].dwork, K_MSEC(ms));
}

void fsm_timer_stop(fsm_timer_e timer)
{
    (void)k_work_cancel_delayable(&timers[timer].dwork);
}

/**
 * @brief Make a top-level state transition.
 *
 * The function logs the transition and stores @p to as the current state. Then it stops every
 * timer and clears every LED request of the old state. Then it calls the run function of the new
 * state with @p from. The function does these steps also when @p from and @p to are the same.
 *
 * @param[in] from   State that the machine leaves.
 * @param[in] event  Event that caused the transition. Only the log uses this value.
 * @param[in] to     State that the machine enters.
 */
static void state_enter(state_top_e from, state_event_e event, state_top_e to)
{
    LOG_INF("%s -> %s (%s)", state_to_string(from), state_to_string(to),
            state_event_to_string(event));
    state = to;

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
    for (int i = 0; i < FSM_TIMER_COUNT; i++) {
        k_work_init_delayable(&timers[i].dwork, timer_handler);
    }
    state = STATE_INIT;
    LOG_INF("start -> %s", state_to_string(state));
    state_init_run(STATE_INIT);
}

void state_machine_process_event(state_event_e event)
{
    if (event == STATE_EVENT_BATTERY_CHANGED) {
        if ((state == STATE_INIT || state == STATE_ACTIVE || state == STATE_SLEEP)
            && battery_level() == BATTERY_LEVEL_CRITICAL) {
            event = STATE_EVENT_BATTERY_CRITICAL;
        } else if (state == STATE_SHUTDOWN && battery_level() != BATTERY_LEVEL_CRITICAL) {
            event = STATE_EVENT_BATTERY_MARGIN_OK;
        }
    } else if (state == STATE_SHUTDOWN && event == STATE_EVENT_BUTTON_SHORT) {
        led_request(LED_PRIO_FAULT, LED_RED, LED_PAT_FAST, 0, 2000);
        return;
    }

    if (state == STATE_ACTIVE && state_active_process_event(event)) {
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(state_transitions); i++) {
        if (state == state_transitions[i].from && event == state_transitions[i].event) {
            state_enter(state_transitions[i].from, event, state_transitions[i].to);
            return;
        }
    }
}
