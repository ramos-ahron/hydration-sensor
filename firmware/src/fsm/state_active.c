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
 * @brief Sets the battery LED request from the battery level.
 *
 * When the level is LOW, the function requests a yellow BEAT pattern with a 5 s period. For any
 * other level, the function clears the request.
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
     * Every region gets the event, in this order:
     *  1. Measurement
     *  2. BLE
     */
    const bool measurement_busy = state_measurement_busy();
    bool consumed = false;
    // TODO: Also check if BLE is busy with pairing.
    consumed |= state_measurement_process_event(event);
    consumed |= state_ble_process_event(event, measurement_busy);
    return consumed;
}

void state_active_run(state_top_e from)
{
    __ASSERT(from == STATE_INIT, "ACTIVE entered from %d", from);
    ARG_UNUSED(from);

    state_ble_run();
    battery_led();
    state_measurement_run();
}
