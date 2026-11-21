#include "fsm/state_active.h"
#include "fsm/state_ble.h"
#include "fsm/state_machine.h"
#include "fsm/state_measurement.h"
#include "hs/thresholds.h"
#include "safety/safety.h"
#include "ui/led.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

/**
 * @brief Coordinates the parallel regions of the active state.
 *
 * Active consists of two parallel regions: measurement and BLE. Each region
 * maintains its own private state enum and transition table in a separate file.
 * The battery has no region: safety.c reads it, this file shows the LOW beat and the
 * state machine maps a critical level to SHUTDOWN.
 *
 * This module owns no transition table or substates. It initializes
 * the regions, dispatches events to them, and leaves top-level exit
 * transitions (SLEEP_PREP and SHUTDOWN) to the state machine.
 */

/**
 *
 */
static void battery_led(void)
{
    if (battery_level() == BATTERY_LEVEL_LOW) {
        led_request(LED_PRIO_BATTERY, LED_YELLOW, LED_PAT_BEAT, 5000, 0);
    } else {
        led_clear(LED_PRIO_BATTERY);
    }
}

bool state_active_process_event(state_event_e event)
{
    if (event == STATE_EVENT_BUTTON_SHORT && state_ble_pairing()) {
        return state_ble_process_event(event, state_measurement_busy());
    }

    if (event == STATE_EVENT_SLEEP_GUARD && (!state_measurement_idle() || state_ble_pairing())) {
        if (state_measurement_idle()) {
            fsm_timer_start(FSM_TIMER_SLEEP_GUARD, STATE_EVENT_SLEEP_GUARD, SLEEP_GUARD_MS);
        }
        return true;
    }

    if (event == STATE_EVENT_BATTERY_CHANGED) {
        battery_led();
        return true;
    }

    /**
     * Every region sees the event. Order of priority:
     *  1. Measurement
     *  2. BLE
     */
    const bool measurement_busy = state_measurement_busy();
    bool consumed = false;
    // TODO: check if busy pairing in addition
    consumed |= state_measurement_process_event(event);
    consumed |= state_ble_process_event(event, measurement_busy);
    return consumed;
}

void state_active_run(state_e from)
{
    __ASSERT(from == STATE_INIT, "ACTIVE entered from %d", from);
    ARG_UNUSED(from);

    // Initial substates: [*] --> BLE_IDLE, [*] --> M_IDLE
    state_ble_run();
    battery_led();
    state_measurement_run();
}
