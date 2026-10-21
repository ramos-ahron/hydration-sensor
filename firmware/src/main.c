#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

// Allows severity LOG_LEVEL_* and above
LOG_MODULE_REGISTER(hydration_sensor, LOG_LEVEL_DBG);

#define SLEEP_TIME_MS   100

// Sensors
static const struct device *const temp_dev  = DEVICE_DT_GET(DT_ALIAS(temp0));

// IO
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(ext_btn), gpios);
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(red_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(green_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(blue_led), gpios),
};

static const uint8_t colors[][3] = {
    {0, 0, 0},
    {1, 0, 0},
    {0, 1, 0},
    {1, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {0, 1, 1},
    {1, 1, 1},
};

int main(void)
{
    struct sensor_value temp;

    // Device Initialization
    if (!device_is_ready(temp_dev)) {
        printk("temp_dev not ready\n");
        return 0;
    }

    if (!gpio_is_ready_dt(&btn)) {
        return 0;
    }
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    for (int i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            return 0;
        }
        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
    }

    int color = 0;
    int prev = 0;

    while (1) {
        int pressed = gpio_pin_get_dt(&btn);

        // Temperature Readings
        int err = sensor_sample_fetch(temp_dev) ?:
                sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

        if (pressed && !prev) {
            color = (color + 1) % ARRAY_SIZE(colors);
            for (int i = 0; i < ARRAY_SIZE(leds); i++) {
                gpio_pin_set_dt(&leds[i], colors[color][i]);
        }
        LOG_INF("color %d: r=%d g=%d b=%d", color, colors[color][0], colors[color][1], colors[color][2]);}
        prev = pressed;

        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}