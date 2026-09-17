#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

// Allows severity LOG_LEVEL_* and above
LOG_MODULE_REGISTER(hydration_sensor, LOG_LEVEL_DBG);

int main(void)
{
    while (1)
    {
        printk("Hello World!\n");

        // Lets use logging for more information and debug gating in the future
        LOG_INF("nRF Connect SDK");
        LOG_DBG("A log message in debug level");
        LOG_WRN("A log message in warning level!");
        LOG_ERR("A log message in Error level!");

        k_msleep(1000);
    }
}