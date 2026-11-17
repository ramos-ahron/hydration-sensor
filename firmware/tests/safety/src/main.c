#include "fsm/state_machine.h"
#include "hs/thresholds.h"
#include "safety/safety.h"
#include <errno.h>
#include <zephyr/ztest.h>

extern void safety_set_batt_override(int mv);
extern void safety_set_vbus_override(int v);

static int batt_events;

/** Stand-in for the state machine: counts battery events only. */
void fsm_post(state_event_e event)
{
    if (event == STATE_EVENT_BATTERY_CHANGED) { batt_events++; }
}

/** Sets the override and checks the level and the number of events it posted. */
static void step(int mv, battery_level_e want, int want_events)
{
    const int before = batt_events;

    safety_set_batt_override(mv);
    zassert_equal(battery_level(), want, "mv %d: level %d, want %d", mv, battery_level(), want);
    zassert_equal(batt_events - before, want_events, "mv %d: %d events", mv, batt_events - before);
    zassert_equal(battery_mv(), mv);
}

static void *setup(void)
{
    zassert_equal(safety_init(), 0);
    return NULL;
}

static void before(void *f)
{
    ARG_UNUSED(f);
    /* Back to OK from any level, no hardware VBUS on the bench either. */
    safety_set_batt_override(BATT_LOW_MV + BATT_MARGIN_MV);
    safety_set_vbus_override(0);
}

ZTEST(safety, test_init_idempotent)
{
    zassert_equal(safety_init(), 0);
    zassert_equal(safety_init(), 0);
}

ZTEST(safety, test_low_hysteresis)
{
    step(BATT_LOW_MV, BATTERY_LEVEL_OK, 0);
    step(BATT_LOW_MV - 1, BATTERY_LEVEL_LOW, 1);
    step(BATT_LOW_MV + BATT_MARGIN_MV - 1, BATTERY_LEVEL_LOW, 0);
    step(BATT_LOW_MV + BATT_MARGIN_MV, BATTERY_LEVEL_OK, 1);
}

ZTEST(safety, test_critical_hysteresis)
{
    step(BATT_CRITICAL_MV, BATTERY_LEVEL_LOW, 1);
    step(BATT_CRITICAL_MV - 1, BATTERY_LEVEL_CRITICAL, 1);
    step(BATT_CRITICAL_MV + BATT_MARGIN_MV - 1, BATTERY_LEVEL_CRITICAL, 0);
    zassert_false(battery_above_margin());
    step(BATT_CRITICAL_MV + BATT_MARGIN_MV, BATTERY_LEVEL_LOW, 1);
    zassert_true(battery_above_margin());
}

ZTEST(safety, test_critical_to_ok)
{
    step(BATT_CRITICAL_MV - 1, BATTERY_LEVEL_CRITICAL, 1);
    step(BATT_LOW_MV + BATT_MARGIN_MV - 1, BATTERY_LEVEL_LOW, 1);
    step(BATT_CRITICAL_MV - 1, BATTERY_LEVEL_CRITICAL, 1);
    step(BATT_LOW_MV + BATT_MARGIN_MV, BATTERY_LEVEL_OK, 1);
}

ZTEST(safety, test_auto_restores_unknown)
{
    step(BATT_LOW_MV - 1, BATTERY_LEVEL_LOW, 1);
    const int before = batt_events;

    safety_set_batt_override(-1);
    /* The test boards have no battery ADC channel: unknown keeps the level. */
    zassert_true(battery_mv() < 0);
    zassert_equal(battery_level(), BATTERY_LEVEL_LOW);
    zassert_equal(batt_events, before);
    zassert_true(battery_above_margin());
}

ZTEST(safety, test_charging_lockout)
{
    zassert_false(safety_vbus_present());
    zassert_equal(safety_check(), 0);
    safety_set_vbus_override(1);
    zassert_true(safety_vbus_present());
    zassert_equal(safety_check(), -EPERM);
    safety_set_vbus_override(0);
    zassert_equal(safety_check(), 0);
}

ZTEST(safety, test_drive_watchdog_smoke)
{
    safety_drive_feed();
    safety_drive_end();
    /* Twice the channel count: a leak would exhaust the channels. */
    for (int i = 0; i < 2 * CONFIG_TASK_WDT_CHANNELS; i++) {
        safety_drive_begin();
        safety_drive_begin();
        safety_drive_feed();
        safety_drive_end();
        safety_drive_end();
    }
    (void)safety_reset_was_watchdog();
}

ZTEST_SUITE(safety, NULL, setup, before, NULL, NULL);
