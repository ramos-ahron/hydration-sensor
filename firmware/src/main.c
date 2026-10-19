#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

// Allows severity LOG_LEVEL_* and above
LOG_MODULE_REGISTER(hydration_sensor, LOG_LEVEL_DBG);

#define SLEEP_TIME_MS   100

// Sensors
static const struct device *const temp_dev  = DEVICE_DT_GET(DT_ALIAS(temp0));
static const struct device *const accel_dev = DEVICE_DT_GET(DT_ALIAS(accel0));

// IO
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(ext_btn), gpios);
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(red_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(green_led), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(blue_led), gpios),
};

int main(void)
{
    struct sensor_value temp;
    struct sensor_value accel[3];

    // Device Initialization
    if (!device_is_ready(temp_dev)) {
        printk("temp_dev not ready\n");
        return 0;
    }

    if (!device_is_ready(accel_dev)) {
        printk("accel_dev not ready\n");
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

    int current = 0;
    int prev = 0;

    while (1) {
        int pressed = gpio_pin_get_dt(&btn);

        // Temperature Readings
        int err = sensor_sample_fetch(temp_dev) ?:
                sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

        if (err) {
            LOG_DBG("Read Failed: %d\n", err);
        } else {
            LOG_DBG("Temperature: %.2f Cel\n", sensor_value_to_double(&temp));
        }

        // Accelerometer Readings
        err = sensor_sample_fetch(accel_dev) ?:
            sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);

        if (err) {
            LOG_DBG("Accel read failed: %d\n", err);
        } else {
            LOG_DBG("Accel: %.2f %.2f %.2f m/s2\n",
                sensor_value_to_double(&accel[0]),
                sensor_value_to_double(&accel[1]),
                sensor_value_to_double(&accel[2]));
        }

        if (pressed && !prev) {
            current = (current + 1) % 3;
        }
        prev = pressed;

        gpio_pin_set_dt(&leds[0], current == 0 && pressed);
        gpio_pin_set_dt(&leds[1], current == 1 && pressed);
        gpio_pin_set_dt(&leds[2], current == 2 && pressed);

        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}