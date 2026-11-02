#include "ui/button.h"
#include "fsm/state_machine.h"
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

/**
 * @brief Input callback that turns button presses into FSM events.
 *
 * Ignores everything except key presses. INPUT_KEY_A posts STATE_EVENT_BUTTON_SHORT and
 * INPUT_KEY_X posts STATE_EVENT_BUTTON_LONG to fsm_msgq without blocking; other key codes
 * are ignored. The event is dropped if the queue is full.
 *
 * @param evt Input event from the input subsystem.
 * @param user_data Unused.
 */
static void button_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    // Ignore releases, focus on press
    if (evt->type != INPUT_EV_KEY || evt->value != 1) {
        return;
    }

    // Codes come from the overlay
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

    // Drop events if queue is full
    // TODO: Check if this prevents button mashing
    (void)k_msgq_put(&fsm_msgq, &event, K_NO_WAIT);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(btn_press)), button_cb, NULL);
