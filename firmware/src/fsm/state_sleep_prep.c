#include "fsm/state_sleep_prep.h"
#include "ble/ble_service.h"
#include "fsm/state_machine.h"
#include "pipeline/pipeline.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_sleep_prep_run(state_top_e from)
{
    __ASSERT(from == STATE_ACTIVE, "SLEEP_PREP entered from %d", from);
    ARG_UNUSED(from);

    // A battery event can leave ACTIVE during a sweep. Turn the drive off first.
    pipeline_stop();
    ble_adv_stop();
    ble_disconnect();
    fsm_post(STATE_EVENT_PREP_COMPLETE);
}
