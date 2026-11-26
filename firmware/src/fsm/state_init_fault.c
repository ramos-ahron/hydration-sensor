#include "fsm/state_init_fault.h"
#include "ui/led.h"

#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_init_fault_run(state_top_e from)
{
    __ASSERT(from == STATE_INIT, "INIT_FAULT entered from %d", from);
    ARG_UNUSED(from);

    led_request(LED_PRIO_FAULT, LED_RED, LED_PAT_FAST, 0, 0);
}
