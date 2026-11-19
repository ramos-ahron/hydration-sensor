#include "fsm/state_init.h"
#include "ble/ble_service.h"
#include "fsm/state_machine.h"
#include "safety/safety.h"
#include "sensors/sensor.h"
#include "storage/record.h"
#include "ui/led.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(fsm, LOG_LEVEL_INF);

// How long INIT stays visible
#define INIT_DWELL_MS 500

void state_init_run(state_e from)
{
    __ASSERT(from == STATE_INIT || from == STATE_INIT_FAULT || from == STATE_SLEEP ||
                 from == STATE_SHUTDOWN,
             "INIT entered from %d", from);

    led_request(LED_PRIO_POWER, LED_WHITE, LED_PAT_STEADY, 0, 0);
    if (from == STATE_INIT && safety_reset_was_watchdog()) { LOG_WRN("INIT after watchdog reset"); }

    int err = safety_init();
    err = err ? err : record_init();
    err = err ? err : ble_init();
    err = err ? err : sensor_init();

    if (err) { LOG_ERR("init failed (err %d)", err); }
    fsm_timer_start(FSM_TIMER_INIT, err ? STATE_EVENT_INIT_FAIL : STATE_EVENT_INIT_OK,
                    INIT_DWELL_MS);
}
