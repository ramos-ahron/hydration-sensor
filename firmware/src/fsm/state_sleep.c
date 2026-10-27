#include "fsm/state_sleep.h"
#include "fsm/state_machine.h"
#include "ui/led.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_sleep_run(state_e from)
{
    __ASSERT(from == STATE_SLEEP_PREP, "SLEEP entered from %d", from);
    ARG_UNUSED(from);

    led_set(LED_WHITE);
    
    /**
     * short: goes back to init, handled by transition table
     * long: goes to shutdown, send battery critical
     */
    fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_BATTERY_CRITICAL);
}