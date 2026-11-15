#include "fsm/state_active.h"
#include "fsm/state_battery_monitor.h"
#include "fsm/state_ble.h"
#include "fsm/state_machine.h"
#include "fsm/state_measurement.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

/**
 * @brief Coordinates the parallel regions of the active state.
 *
 * Active consists of three parallel regions: measurement, BLE, and
 * battery monitoring. Each region maintains its own private state enum
 * and transition table in a separate file.
 *
 * This module owns no transition table or substates. It initializes
 * the regions, dispatches events to them, and leaves top-level exit
 * transitions (SLEEP_PREP and SHUTDOWN) to the state machine.
 */

bool state_active_process_event(state_event_e event)
{
    if (event == STATE_EVENT_BUTTON_SHORT && state_ble_pairing()) {
        return state_ble_process_event(event, state_measurement_busy());
    }

    if (event == STATE_EVENT_SLEEP_GUARD && (!state_measurement_idle() || state_ble_pairing())) {
        if (state_measurement_idle() && IS_ENABLED(CONFIG_FSM_MEASURE_MODE_MANUAL)) {
            fsm_stub(STATE_EVENT_SLEEP_GUARD, STATE_EVENT_NONE, STATE_EVENT_NONE);
        }
        return true;
    }

    /**
     * Every region sees the event. Order of priority:
     *  1. Measurement
     *  2. BLE
     *  3. Battery Monitor
     */
    const bool ble_connected = state_ble_connected();
    const bool measurement_busy = state_measurement_busy();
    bool consumed = false;
    // TODO: check if busy pairing in addition
    consumed |= state_measurement_process_event(event, ble_connected);
    consumed |= state_ble_process_event(event, measurement_busy);
    consumed |= state_battery_monitor_process_event(event);
    return consumed;
}

void state_active_run(state_e from)
{
    __ASSERT(from == STATE_INIT, "ACTIVE entered from %d", from);
    ARG_UNUSED(from);

    // Initial substates: [*] --> BLE_IDLE, [*] --> BatteryInit, [*] --> M_IDLE
    state_ble_run();
    state_battery_monitor_run();
    state_measurement_run();
}
