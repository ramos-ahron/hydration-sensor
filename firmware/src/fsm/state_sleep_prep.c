#include "fsm/state_sleep_prep.h"
#include "ble/ble_service.h"
#include "fsm/state_machine.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_sleep_prep_run(state_e from)
{
    __ASSERT(from == STATE_ACTIVE, "SLEEP_PREP entered from %d", from);
    ARG_UNUSED(from);

    ble_adv_stop();
    ble_disconnect();
    fsm_post(STATE_EVENT_PREP_COMPLETE);
}
