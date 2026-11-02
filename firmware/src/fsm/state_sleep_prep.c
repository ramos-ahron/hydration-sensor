#include "fsm/state_sleep_prep.h"
#include "fsm/state_machine.h"
#include "ui/led.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_sleep_prep_run(state_e from)
{
    __ASSERT(from == STATE_ACTIVE, "SLEEP_PREP entered from %d", from);
    ARG_UNUSED(from);

    led_set(LED_OFF);

    fsm_stub(STATE_EVENT_PREP_COMPLETE, STATE_EVENT_PREP_COMPLETE, STATE_EVENT_NONE);
}
