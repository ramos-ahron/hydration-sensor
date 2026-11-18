#include "fsm/state_ble.h"
#include "ble/ble_service.h"
#include "common/enum_to_string.h"
#include "fsm/state_machine.h"
#include "ui/led.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(fsm, LOG_LEVEL_INF);

struct ble_transition {
    ble_state_e from;
    state_event_e event;
    ble_state_e to;
};

static const struct ble_transition ble_transitions[] = {
    { BLE_STATE_BLE_IDLE, STATE_EVENT_BUTTON_LONG, BLE_STATE_PAIRING },
    { BLE_STATE_BLE_IDLE, STATE_EVENT_CONNECTION_COMPLETE, BLE_STATE_CONNECTED },
    { BLE_STATE_PAIRING, STATE_EVENT_BUTTON_SHORT, BLE_STATE_BLE_IDLE },
    { BLE_STATE_PAIRING, STATE_EVENT_CONNECTION_COMPLETE, BLE_STATE_CONNECTED },
    { BLE_STATE_PAIRING, STATE_EVENT_PAIRING_TIMEOUT, BLE_STATE_BLE_IDLE },
    { BLE_STATE_CONNECTED, STATE_EVENT_BUTTON_LONG, BLE_STATE_BLE_IDLE },
    // TODO: Is disconnected needed
    { BLE_STATE_CONNECTED, STATE_EVENT_LINK_LOST, BLE_STATE_DISCONNECTED },
    { BLE_STATE_DISCONNECTED, STATE_EVENT_BLE_IDLE, BLE_STATE_BLE_IDLE },
};

static ble_state_e ble_state;

/**
 * @brief Enter a BLE substate.
 *
 * The function asserts that the transition is legal. Then it stores @p to as the current BLE
 * state.
 *
 * @param[in] from  State that the BLE region leaves. Only the assertions use this value.
 * @param[in] to    State that the BLE region enters.
 */
static void ble_enter(ble_state_e from, ble_state_e to)
{
    ARG_UNUSED(from);

    fsm_timer_stop(FSM_TIMER_PAIRING);
    led_clear(LED_PRIO_BLE);

    switch (to) {
        case BLE_STATE_BLE_IDLE:
            __ASSERT(from == BLE_STATE_BLE_IDLE || from == BLE_STATE_PAIRING ||
                         from == BLE_STATE_CONNECTED || from == BLE_STATE_DISCONNECTED,
                     "BLE_IDLE entered from %d", from);
            ble_disconnect();
            ble_adv_bonded();
            break;
        case BLE_STATE_PAIRING:
            __ASSERT(from == BLE_STATE_BLE_IDLE, "PAIRING entered from %d", from);
            ble_adv_pairing();
            led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_SLOW, 0, 0);
            fsm_timer_start(FSM_TIMER_PAIRING, STATE_EVENT_PAIRING_TIMEOUT,
                            CONFIG_PAIRING_TIMEOUT_S * MSEC_PER_SEC);
            break;
        case BLE_STATE_CONNECTED:
            __ASSERT(from == BLE_STATE_PAIRING || from == BLE_STATE_BLE_IDLE,
                     "CONNECTED entered from %d", from);
            led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_TRIPLE, 0, 0);
            break;
        case BLE_STATE_DISCONNECTED:
            __ASSERT(from == BLE_STATE_CONNECTED, "DISCONNECTED entered from %d", from);
            fsm_post(STATE_EVENT_BLE_IDLE);
            break;
    }
    ble_state = to;
}

bool state_ble_connected(void) { return ble_state == BLE_STATE_CONNECTED; }

bool state_ble_pairing(void) { return ble_state == BLE_STATE_PAIRING; }

bool state_ble_process_event(state_event_e event, bool measurement_busy)
{
    // Ensure measurement is not busy before transitioning
    if (event == STATE_EVENT_BUTTON_LONG && measurement_busy) { return false; }

    for (size_t i = 0; i < ARRAY_SIZE(ble_transitions); i++) {
        const struct ble_transition *t = &ble_transitions[i];
        if (t->from == ble_state && t->event == event) {
            LOG_INF("%s -> %s (%s)", ble_state_to_string(t->from), ble_state_to_string(t->to),
                    state_event_to_string(event));
            ble_enter(t->from, t->to);
            return true;
        }
    }
    return false;
}

void state_ble_run(void)
{
    ble_state = BLE_STATE_BLE_IDLE;
    ble_disconnect();
    ble_adv_bonded();
    LOG_INF("BLE start -> %s", ble_state_to_string(ble_state));
}
