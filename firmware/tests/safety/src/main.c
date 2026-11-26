#include "fsm/state_machine.h"
#include "hs/thresholds.h"
#include "safety/safety.h"

#include <errno.h>

#include <zephyr/ztest.h>

static int batt_events;

extern void safety_set_batt_override(int mv);
extern void safety_set_vbus_override(int v);

/**
 * @brief Sets the battery override and checks the level and the number of events it posted.
 *
 * @param[in] mv           Battery voltage in mV.
 * @param[in] want         Level that battery_level() must return.
 * @param[in] want_events  Number of BATTERY_CHANGED events that the override must post.
 */
static void step(int mv, battery_level_e want, int want_events)
{
    const int before = batt_events;

    safety_set_batt_override(mv);
    zassert_equal(battery_level(), want, "mv %d: level %d, want %d", mv, battery_level(), want);
    zassert_equal(batt_events - before, want_events, "mv %d: %d events", mv, batt_events - before);
    zassert_equal(battery_mv(), mv);
}

/**
 * @brief Starts the safety module one time before the suite.
 *
 * @return NULL. The suite has no fixture.
 */
static void *setup(void)
{
    zassert_equal(safety_init(), 0);
    return NULL;
}

/**
 * @brief Sets the battery to OK and the VBUS to absent before each test.
 *
 * @param[in] f  Test fixture. The function does not use this value.
 */
static void before(void *f)
{
    ARG_UNUSED(f);
    /* Go back to OK from any level. Set VBUS to absent, because the bench has no hardware VBUS. */
    safety_set_batt_override(BATT_LOW_MV + BATT_MARGIN_MV);
    safety_set_vbus_override(0);
}

ZTEST_SUITE(safety, NULL, setup, before, NULL, NULL);

/**
 * @brief Stand-in for the state machine. It counts battery events only.
 *
 * @param[in] event  Event that the safety module posts.
 */
void fsm_post(state_event_e event)
{
    if (event == STATE_EVENT_BATTERY_CHANGED) {
        batt_events++;
    }
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
    /* The test boards have no battery ADC channel. An unknown reading keeps the level. */
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
    /* Loop two times the channel count. If a call leaks a channel, the loop uses all channels. */
    for (int i = 0; i < 2 * CONFIG_TASK_WDT_CHANNELS; i++) {
        safety_drive_begin();
        safety_drive_begin();
        safety_drive_feed();
        safety_drive_end();
        safety_drive_end();
    }
    (void)safety_reset_was_watchdog();
}
