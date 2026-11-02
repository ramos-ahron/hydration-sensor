#include "fsm/state_battery_monitor.h"
#include "common/enum_to_string.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(fsm, LOG_LEVEL_INF);

struct battery_transition {
    battery_state_e from;
    state_event_e event;
    battery_state_e to;
};

static const struct battery_transition battery_transitions[] = {
    { BATTERY_STATE_BATTERY_OK, STATE_EVENT_BATTERY_BELOW, BATTERY_STATE_BATTERY_LOW },
    { BATTERY_STATE_BATTERY_LOW, STATE_EVENT_BATTERY_RECOVERED, BATTERY_STATE_BATTERY_OK },
    { BATTERY_STATE_BATTERY_LOW, STATE_EVENT_BATTERY_BELOW_CRITICAL,
      BATTERY_STATE_BATTERY_CRITICAL },
    { BATTERY_STATE_BATTERY_CRITICAL, STATE_EVENT_BATTERY_ABOVE_CRITICAL,
      BATTERY_STATE_BATTERY_LOW },
};

static battery_state_e battery_state;

/**
 * @brief Enters a battery monitor state.
 *
 * Asserts that the transition is legal and stores @p to
 * as the current battery state.
 *
 * @param from State being left; only used by the assertions.
 * @param to State being entered.
 */
static void battery_enter(battery_state_e from, battery_state_e to)
{
    ARG_UNUSED(from);

    switch (to) {
        case BATTERY_STATE_BATTERY_OK:
            __ASSERT(from == BATTERY_STATE_BATTERY_OK || from == BATTERY_STATE_BATTERY_LOW,
                     "BATTERY_OK entered from %d", from);
            break;
        case BATTERY_STATE_BATTERY_LOW:
            __ASSERT(from == BATTERY_STATE_BATTERY_LOW || from == BATTERY_STATE_BATTERY_OK ||
                         from == BATTERY_STATE_BATTERY_CRITICAL,
                     "BATTERY_LOW entered from %d", from);
            break;
        case BATTERY_STATE_BATTERY_CRITICAL:
            __ASSERT(from == BATTERY_STATE_BATTERY_LOW, "BATTERY_CRITICAL entered from %d", from);
            break;
    }
    battery_state = to;
}

bool state_battery_monitor_process_event(state_event_e event)
{
    for (size_t i = 0; i < ARRAY_SIZE(battery_transitions); i++) {
        const struct battery_transition *t = &battery_transitions[i];
        if (t->from == battery_state && t->event == event) {
            if (t->to == t->from) {
                return false;
            }
            LOG_INF("%s -> %s (%s)", battery_state_to_string(t->from),
                    battery_state_to_string(t->to), state_event_to_string(event));
            battery_enter(t->from, t->to);
            return true;
        }
    }
    return false;
}

void state_battery_monitor_run(void)
{
    // TODO: be careful about calling this function as it resets states
    // meant to be for initialization
    battery_state = BATTERY_STATE_BATTERY_OK;
    LOG_INF("BATTERY start -> %s", battery_state_to_string(battery_state));
}
