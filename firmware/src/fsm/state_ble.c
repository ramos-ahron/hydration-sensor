#include "fsm/state_ble.h"
#include "common/enum_to_string.h"
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
    { BLE_STATE_PAIRING, STATE_EVENT_BUTTON_SHORT, BLE_STATE_BLE_IDLE },
    { BLE_STATE_PAIRING, STATE_EVENT_CONNECTION_COMPLETE, BLE_STATE_CONNECTED },
    { BLE_STATE_PAIRING, STATE_EVENT_PAIRING_TIMEOUT, BLE_STATE_BLE_IDLE },
    { BLE_STATE_CONNECTED, STATE_EVENT_BUTTON_LONG, BLE_STATE_BLE_IDLE },
    // TODO: Is disconnected needed
    { BLE_STATE_CONNECTED, STATE_EVENT_LINK_LOST, BLE_STATE_DISCONNECTED },
    { BLE_STATE_DISCONNECTED, STATE_EVENT_READVERTISE, BLE_STATE_BLE_IDLE },
};

static ble_state_e ble_state;

static void ble_enter(ble_state_e from, ble_state_e to)
{
    ARG_UNUSED(from);

    switch (to) {
        case BLE_STATE_BLE_IDLE:
            __ASSERT(from == BLE_STATE_BLE_IDLE ||
                    from == BLE_STATE_PAIRING ||
                    from == BLE_STATE_CONNECTED ||
                    from == BLE_STATE_DISCONNECTED,
                    "BLE_IDLE entered from %d", from);
            break;
        case BLE_STATE_PAIRING:
            __ASSERT(from == BLE_STATE_BLE_IDLE,
                    "PAIRING entered from %d", from);
            break;
        case BLE_STATE_CONNECTED:
            __ASSERT(from == BLE_STATE_PAIRING,
                    "CONNECTED entered from %d", from);
            break;
        case BLE_STATE_DISCONNECTED:
            __ASSERT(from == BLE_STATE_CONNECTED,
                    "DISCONNECTED entered from %d", from);
            break;
    }
    ble_state = to;
}

bool state_ble_connected(void)
{
    return ble_state == BLE_STATE_CONNECTED;
}

bool state_ble_pairing(void)
{
    return ble_state == BLE_STATE_PAIRING;
}

bool state_ble_process_event(state_event_e event, bool measurement_busy)
{
    // Ensure measurement is not busy before transitioning
    if (event == STATE_EVENT_BUTTON_LONG && measurement_busy) {
        return false;
    }

    for (size_t i = 0; i < ARRAY_SIZE(ble_transitions); i ++) {
        const struct ble_transition *t = &ble_transitions[i];
        if (t->from == ble_state && t->event == event) {
            if (t->to == t->from) {
                return false;
            }
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
    // TODO: be careful about calling this function as it resets states
    // meant to be for initialization
    ble_state = BLE_STATE_BLE_IDLE;
    LOG_INF("BLE start -> %s", ble_state_to_string(ble_state));
}