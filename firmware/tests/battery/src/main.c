#include "hs/thresholds.h"
#include "safety/battery.h"

#include <zephyr/ztest.h>

#define OK   BATTERY_LEVEL_OK
#define LOW  BATTERY_LEVEL_LOW
#define CRIT BATTERY_LEVEL_CRITICAL

#define L BATT_LOW_MV
#define C BATT_CRITICAL_MV
#define H BATT_MARGIN_MV

ZTEST_SUITE(battery, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Feeds a voltage many times and checks that the level settles and stays.
 *
 * @param[in] prev  Level before the first reading.
 * @param[in] mv    Battery voltage in mV.
 * @param[in] want  Level that batt_classify() must return for every reading.
 */
static void hold(battery_level_e prev, int mv, battery_level_e want)
{
    battery_level_e lvl = prev;

    for (int i = 0; i < 100; i++) {
        lvl = batt_classify(lvl, mv);
        zassert_equal(lvl, want, "prev %d, %d mV, iter %d: got %d, want %d", prev, mv, i, lvl,
                      want);
    }
}

ZTEST(battery, test_transitions)
{
    zassert_equal(batt_classify(OK, L - 1), LOW);   /* OK -> LOW */
    zassert_equal(batt_classify(OK, C - 1), CRIT);  /* OK -> CRITICAL in one reading */
    zassert_equal(batt_classify(LOW, C - 1), CRIT); /* LOW -> CRITICAL */
    zassert_equal(batt_classify(LOW, L + H), OK);   /* LOW -> OK needs low + margin */
    zassert_equal(batt_classify(CRIT, C + H), LOW); /* CRITICAL -> LOW needs critical + margin */
    zassert_equal(batt_classify(CRIT, L + H), OK);  /* CRITICAL -> OK in one reading */
}

ZTEST(battery, test_unknown_reading_keeps_level)
{
    zassert_equal(batt_classify(OK, -1), OK);
    zassert_equal(batt_classify(LOW, -1), LOW);
    zassert_equal(batt_classify(CRIT, -1), CRIT);
}

ZTEST(battery, test_no_chatter_low)
{
    hold(OK, L, OK);   /* at LOW threshold: not below it */
    hold(LOW, L, LOW); /* inside the band: stays LOW */
    hold(LOW, L + H - 1, LOW);
    hold(LOW, L + H, OK);
    hold(OK, L + H - 1, OK);
    hold(OK, L - H, LOW);
    hold(LOW, L - H, LOW);
}

ZTEST(battery, test_no_chatter_critical)
{
    hold(LOW, C, LOW); /* at CRITICAL threshold: not below it */
    hold(CRIT, C, CRIT);
    hold(CRIT, C + H - 1, CRIT);
    hold(CRIT, C + H, LOW);
    hold(LOW, C + H - 1, LOW);
    hold(LOW, C - H, CRIT);
    hold(CRIT, C - H, CRIT);
}
