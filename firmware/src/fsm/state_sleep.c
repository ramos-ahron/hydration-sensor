#include "fsm/state_sleep.h"
#include "fsm/state_machine.h"
#include "ui/led.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

// Todo: change sleep interval as necessary
#define SLEEP_BEAT_MS 4000

void state_sleep_run(state_e from)
{
    __ASSERT(from == STATE_SLEEP_PREP, "SLEEP entered from %d", from);
    ARG_UNUSED(from);

    led_set(LED_WHITE);

    /**
     * short: goes back to init, handled by transition table
     * long: goes to shutdown, send battery critical
     */
    led_request(LED_PRIO_POWER, LED_WHITE, LED_PAT_BEAT, SLEEP_BEAT_MS, 0);
    fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_BATTERY_CRITICAL);
}
