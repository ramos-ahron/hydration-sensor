#include "fsm/state_machine.h"

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

/**
 * @brief Input callback that changes button presses into FSM events.
 *
 * The function ignores all input events that are not key presses. INPUT_KEY_A posts
 * STATE_EVENT_BUTTON_SHORT and INPUT_KEY_X posts STATE_EVENT_BUTTON_LONG with fsm_post(). The
 * function does not block, and it ignores other key codes.
 *
 * @param[in] evt        Input event from the input subsystem.
 * @param[in] user_data  The function does not use this value.
 */
static void button_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY || evt->value != 1) {
        return;
    }

    state_event_e event;
    switch (evt->code) {
        case INPUT_KEY_A:
            event = STATE_EVENT_BUTTON_SHORT;
            break;
        case INPUT_KEY_X:
            event = STATE_EVENT_BUTTON_LONG;
            break;
        default:
            return;
    }

    fsm_post(event);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_ALIAS(btn_press)), button_cb, NULL);
