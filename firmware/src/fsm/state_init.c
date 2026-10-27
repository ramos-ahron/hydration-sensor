#include "fsm/state_init.h"
#include "fsm/state_machine.h"
#include "ui/led.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_init_run(state_e from)
{
    __ASSERT(from == STATE_INIT || from == STATE_INIT_FAULT || from == STATE_SLEEP ||
                from == STATE_SHUTDOWN,
             "INIT entered from %d", from);
    ARG_UNUSED(from);

    led_set(LED_WHITE);
    /**
     * On Timed / Short Events: Init OK
     * On Long Events: Init fault
     */
    fsm_stub(STATE_EVENT_INIT_OK, STATE_EVENT_INIT_OK, STATE_EVENT_INIT_FAIL);
}