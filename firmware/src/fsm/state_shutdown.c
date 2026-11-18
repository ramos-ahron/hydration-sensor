#include "fsm/state_shutdown.h"
#include "ble/ble_service.h"
#include "fsm/state_machine.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

void state_shutdown_run(state_e from)
{
    __ASSERT(from == STATE_INIT || from == STATE_ACTIVE || from == STATE_SLEEP,
             "SHUTDOWN entered from %d", from);
    ARG_UNUSED(from);

    ble_adv_stop();
    ble_disconnect();
}
