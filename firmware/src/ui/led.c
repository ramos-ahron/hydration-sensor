#include "ui/led.h"
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(red_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(green_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(blue_led), gpios),
};

static int led_init(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            return -ENODEV;
        }
        int err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (err) {
            return err;
        }
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
