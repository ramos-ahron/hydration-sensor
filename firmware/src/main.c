#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>

// Allows severity LOG_LEVEL_* and above
LOG_MODULE_REGISTER(hydration_sensor, LOG_LEVEL_DBG);

#define SLEEP_TIME_MS   1000

static const struct device *const temp_dev = DEVICE_DT_GET(DT_ALIAS(temp0));

int main(void)
{
    struct sensor_value temp;

    if (!device_is_ready(temp_dev)) {
        printk("temp_dev not ready\n");
        return 0;
    }

    while (1) {
        int err = sensor_sample_fetch(temp_dev) ?:
                sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

        if (err) {
            printk("Read Failed: %d\n", err);
        } else {
            printk("Temperature: %.2f Cel\n", sensor_value_to_double(&temp));
        }

        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}