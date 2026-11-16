#include "ui/led.h"
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#define LED_HAVE_PINS                                                                              \
    (DT_NODE_EXISTS(DT_ALIAS(red_led)) && DT_NODE_EXISTS(DT_ALIAS(green_led)) &&                   \
     DT_NODE_EXISTS(DT_ALIAS(blue_led)))
#define LED_FLASH_MS 100
#define LED_NEVER    INT64_MAX

#if LED_HAVE_PINS
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(red_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(green_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(blue_led), gpios),
};
#endif  // LED_HAVE_PINS

/**
 * @brief One LED request, held in the slot for its priority.
 */
struct led_slot {
    bool active;
    led_color_e color;
    led_pattern_e pattern;
    uint32_t period_ms;
    int64_t start_ms;
    int64_t end_ms;
};

static struct led_slot slots[LED_PRIO_COUNT];
static led_color_e cur_color;
static struct k_spinlock lock;
static void led_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_work, led_work_fn);

/**
 * @brief Phase of one slot at @p now: on/off and the absolute time of its next edge.
 *
 * @param[in]  s     Slot to evaluate.
 * @param[in]  now   Current uptime in ms.
 * @param[out] next  Uptime in ms of the next on/off edge. Not written when @p *done is set.
 * @param[out] done  Set to true when the pattern has finished and the slot can be dropped.
 *
 * @return true if the LED should be on at @p now, false if it should be off.
 */
static bool led_phase(const struct led_slot *s, int64_t now, int64_t *next, bool *done)
{
    int64_t t = now - s->start_ms;
    int64_t half = 0;
    *done = false;

    switch (s->pattern) {
        case LED_PAT_SLOW:
            // 500 ms on, 500 ms off.
            half = 500;
            break;

        case LED_PAT_FAST:
            // 125 ms on, 125 ms off.
            half = 125;
            break;

        case LED_PAT_TRIPLE:
            // Three flashes with gaps of the same length use five steps. Then the pattern ends.
            if (t >= 5 * LED_FLASH_MS) {
                *done = true;
                return false;
            }
            half = LED_FLASH_MS;
            break;

        case LED_PAT_BEAT: {
            // One flash at the start of each period. The period is at least LED_FLASH_MS.
            int64_t p = MAX((int64_t)s->period_ms, (int64_t)LED_FLASH_MS);
            int64_t base = s->start_ms + (t / p) * p;
            bool on = (t % p) < LED_FLASH_MS;

            // The next change is the end of the flash or the start of the next period.
            *next = on ? base + LED_FLASH_MS : base + p;
            return on;
        }

        default:
            *next = LED_NEVER;
            return true;
    }
    *next = s->start_ms + (t / half + 1) * half;
    return ((t / half) & 1) == 0
}

/**
 * @brief Find the color to show at @p now_ms and the time of the next change.
 *
 * The function checks the slots from highest to lowest priority. The first active slot that has
 * not ended sets the color.
 *
 * @param[in]  now_ms          Current uptime in ms.
 * @param[out] next_change_ms  Uptime in ms of the next change. The value is LED_NEVER when no
 *                             change is scheduled.
 *
 * @return The color to show.
 */
static led_color_e led_resolve(int64_t now_ms, int64_t *next_change_ms)
{
    led_color_e color = LED_OFF;
    bool found = false;
    *next_change_ms = LED_NEVER;

    // Check the slots from highest to lowest priority.
    for (int i = 0; i < LED_PRIO_COUNT; i++) {
        struct led_slot *s = &slots[i];
        int64_t next;
        bool done;
        bool on;

        if (!s->active) { continue; }
        on = led_phase(s, now_ms, &next, &done);

        // Clear the slot when its pattern is done or its end time has passed.
        if (done || now_ms >= s->end_ms) {
            s->active = false;
            continue;
        }

        // The first active slot that has not ended sets the color.
        if (!found) {
            found = true;
            color = on ? s->color : LED_OFF;
            *next_change_ms = MIN(next, s->end_ms);
        }
    }
    return color
}

/**
 * @brief Show the correct color and schedule the next update.
 *
 * The function finds the color for the current time and sets the LED pins to that color. Then it
 * sets @c led_work to run at the next change. If no change is scheduled, it cancels @c led_work.
 *
 * The function takes the lock itself. The caller must not hold the lock.
 */
static void led_update(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    int64_t now = k_uptime_get();
    int64_t next;

    cur_color = led_resolve(now, &next);
#if LED_HAVE_PINS
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        (void)gpio_pin_set_dt(&leds[i], (cur_color >> i) & 1);
    }
#endif
    if (next == LED_NEVER) {
        k_work_cancel_delayable(&led_work);

    } else {
        k_work_reschedule(&led_work, K_MSEC(MAX(next - now, 1)));
    }
    k_spin_unlock(&lock, key);
}

/**
 * @brief Work handler for @c led_work. Update the LED when the timer expires.
 *
 * The system work queue thread runs this function when @c led_work expires. The function calls
 * led_update(), which sets the LED pins and schedules the next expiry.
 *
 * @param[in] work  Work item that expired. The function does not use this argument.
 */
static void led_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    led_update();
}

void led_request(led_prio_e prio, led_color_e color, led_pattern_e pattern, uint32_t period_ms,
                 uint32_t duration_ms)
{
    if ((unsigned int)prio >= LED_PRIO_COUNT) { return; }
    k_spinlock_key_t key = k_spin_lock(&lock);
    int64_t now = k_uptime_get();

    slots[prio] = (struct led_slot){
        .active = true,
        .color = color,
        .pattern = pattern,
        .period_ms = period_ms,
        .start_ms = now,
        .end_ms = duration_ms ? now + duration_ms : LED_NEVER,
    };
    k_spin_unlock(&lock, key);
    led_update();
}

void led_clear(led_prio_e prio)
{
    if ((unsined int)prio >= LED_PRIO_COUNT) { return; }
    k_spinlock_key_t key = k_spin_lock(&lock);
    slots[prio].active = false;
    k_spin_unlock(&lock, key);
    led_update();
}

void led_clear_all(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    for (int i = 0; i < LED_PRIO_COUNT; i++) {
        slots[i].active = false;
    }
    k_spin_unlock(&lock, key);
    led_update();
}

led_color_e led_current(void) { return cur_color; }

#if LED_HAVE_PINS
/**
 * @brief Configures the red, green and blue LED GPIOs as outputs, initially off.
 *
 * Runs at APPLICATION init level through SYS_INIT().
 *
 * @return 0 on success, -ENODEV if an LED GPIO is not ready, or the error returned by
 *         gpio_pin_configure_dt().
 */
static int led_init(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(&leds[i])) { return -ENODEV; }
        int err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (err) { return err; }
    }
    return 0;
}
SYS_INIT(led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void led_set(led_color_e color)
{
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        (void)gpio_pin_set_dt(&leds[i], (color >> i) & 1);
    }
}
#endif
