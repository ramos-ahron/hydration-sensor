#include "ui/led.h"
#include <zephyr/ztest.h>

/**
 * @brief Clears all LED requests before each test.
 *
 * @param[in] f  Test fixture. The function does not use this value.
 */
static void before(void *f)
{
    ARG_UNUSED(f);
    led_clear_all();
}
ZTEST_SUITE(led, NULL, NULL, before, NULL, NULL);

ZTEST(led, test_steady_and_clear)
{
    led_request(LED_PRIO_RESULT, LED_GREEN, LED_PAT_STEADY, 0, 0);
    k_msleep(50);
    zassert_equal(led_current(), LED_GREEN);
    led_clear(LED_PRIO_RESULT);
    k_msleep(20);
    zassert_equal(led_current(), LED_OFF);
}

ZTEST(led, test_slow)
{
    led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_SLOW, 0, 0);
    k_msleep(250);
    zassert_equal(led_current(), LED_BLUE);
    k_msleep(500);
    zassert_equal(led_current(), LED_OFF);
    k_msleep(500);
    zassert_equal(led_current(), LED_BLUE);
}

ZTEST(led, test_fast)
{
    led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_FAST, 0, 0);
    k_msleep(60);
    zassert_equal(led_current(), LED_BLUE);
    k_msleep(130);
    zassert_equal(led_current(), LED_OFF);
}

ZTEST(led, test_triple)
{
    led_request(LED_PRIO_POWER, LED_WHITE, LED_PAT_STEADY, 0, 0);
    led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_TRIPLE, 0, 0);
    k_msleep(50);
    zassert_equal(led_current(), LED_BLUE);
    k_msleep(100);
    zassert_equal(led_current(), LED_OFF);
    k_msleep(100);
    zassert_equal(led_current(), LED_BLUE);
    k_msleep(100);
    zassert_equal(led_current(), LED_OFF);
    k_msleep(100);
    zassert_equal(led_current(), LED_BLUE);
    k_msleep(150);
    zassert_equal(led_current(), LED_WHITE);
    led_clear(LED_PRIO_POWER);
    k_msleep(20);
    zassert_equal(led_current(), LED_OFF);
}

ZTEST(led, test_beat)
{
    led_request(LED_PRIO_BATTERY, LED_YELLOW, LED_PAT_BEAT, 1000, 0);
    k_msleep(50);
    zassert_equal(led_current(), LED_YELLOW);
    k_msleep(450);
    zassert_equal(led_current(), LED_OFF);
    k_msleep(550);
    zassert_equal(led_current(), LED_YELLOW);
}

ZTEST(led, test_duration)
{
    led_request(LED_PRIO_FAULT, LED_RED, LED_PAT_STEADY, 0, 300);
    k_msleep(150);
    zassert_equal(led_current(), LED_RED);
    k_msleep(300);
    zassert_equal(led_current(), LED_OFF);
}

ZTEST(led, test_priority_order)
{
    static const led_color_e col[LED_PRIO_COUNT] = { LED_RED,   LED_YELLOW, LED_BLUE, LED_MAGENTA,
                                                     LED_GREEN, LED_CYAN,   LED_WHITE };

    for (int p = LED_PRIO_COUNT - 1; p >= 0; p--) {
        led_request(p, col[p], LED_PAT_STEADY, 0, 0);
    }
    for (int p = 0; p < LED_PRIO_COUNT; p++) {
        k_msleep(20);
        zassert_equal(led_current(), col[p], "prio %d", p);
        led_clear(p);
    }
    k_msleep(20);
    zassert_equal(led_current(), LED_OFF);
}

ZTEST(led, test_resume_and_battery_hidden)
{
    led_request(LED_PRIO_POWER, LED_WHITE, LED_PAT_STEADY, 0, 0);
    led_request(LED_PRIO_BATTERY, LED_YELLOW, LED_PAT_BEAT, 1000, 0);
    led_request(LED_PRIO_RESULT, LED_GREEN, LED_PAT_STEADY, 0, 0);
    k_msleep(50);
    zassert_equal(led_current(), LED_GREEN);
    k_msleep(500);
    zassert_equal(led_current(), LED_GREEN);
    led_clear(LED_PRIO_RESULT);
    k_msleep(20);
    zassert_equal(led_current(), LED_OFF);
    k_msleep(500);
    zassert_equal(led_current(), LED_YELLOW);
}

ZTEST(led, test_replace_and_clear_all)
{
    led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_STEADY, 0, 0);
    k_msleep(20);
    zassert_equal(led_current(), LED_BLUE);
    led_request(LED_PRIO_BLE, LED_CYAN, LED_PAT_STEADY, 0, 0);
    k_msleep(20);
    zassert_equal(led_current(), LED_CYAN);
    led_request(LED_PRIO_POWER, LED_WHITE, LED_PAT_STEADY, 0, 0);
    led_clear_all();
    k_msleep(20);
    zassert_equal(led_current(), LED_OFF);
}
