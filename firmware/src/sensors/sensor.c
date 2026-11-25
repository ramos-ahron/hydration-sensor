#include "sensors/sensor.h"
#include "hs/thresholds.h"

#include <errno.h>
#include <math.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);

static const struct device *const accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel0));
static const struct device *const temp_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(temp0));

static struct override ovr;
static float window[ACCEL_WINDOW_SAMPLES][3];

float a_rms(const float (*a)[3], size_t n)
{
    if (n == 0) {
        return NAN;
    }
    float mean[3] = { 0 };
    for (size_t i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            mean[k] += a[i][k];
        }
    }
    for (int k = 0; k < 3; k++) {
        mean[k] /= (float)n;
    }
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            float d = a[i][k] - mean[k];
            sum += d * d;
        }
    }
    return sqrtf(sum / (float)n);
}

/**
 * @brief Checks that a device can be used.
 *
 * @param[in] dev  Device to check. Can be NULL.
 *
 * @return true when @p dev is not NULL and is ready, and false when it is not.
 */
static bool dev_usable(const struct device *dev)
{
    return dev != NULL && device_is_ready(dev);
}

int sensor_init(void)
{
    LOG_INF("accel0 %s, temp0 %s", dev_usable(accel_dev) ? "ready" : "missing",
            dev_usable(temp_dev) ? "ready" : "missing");
    return 0;
}

int get_motion(float *rms)
{
    if (ovr.accel_active) {
        if (ovr.accel_fault) {
            return -EIO;
        }
        *rms = ovr.a_rms;
        return 0;
    }
    if (!dev_usable(accel_dev)) {
        return -ENODEV;
    }
    for (size_t i = 0; i < ACCEL_WINDOW_SAMPLES; i++) {
        struct sensor_value v[3];
        if (i > 0) {
            k_msleep(ACCEL_SAMPLE_PERIOD_MS);
        }
        if (sensor_sample_fetch(accel_dev) != 0
            || sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, v) != 0) {
            return -EIO;
        }
        for (int k = 0; k < 3; k++) {
            window[i][k] = sensor_value_to_float(&v[k]);
        }
    }
    /* A stuck or disconnected sensor returns the same value for every sample. With real MEMS
     * noise, the three axes do not stay bit-identical for a full window. */
    bool stuck = true;
    for (size_t i = 1; i < ACCEL_WINDOW_SAMPLES && stuck; i++) {
        stuck = memcmp(window[i], window[0], sizeof(window[0])) == 0;
    }
    if (stuck) {
        return -EIO;
    }
    *rms = a_rms(window, ACCEL_WINDOW_SAMPLES);
    return 0;
}

int get_temp(int16_t *centi_c)
{
    *centi_c = INT16_MIN;
    if (ovr.temp_active) {
        if (ovr.temp_fault) {
            return -EIO;
        }
        *centi_c = ovr.temp_centi_c;
        return 0;
    }
    if (!dev_usable(temp_dev)) {
        return -ENODEV;
    }
    struct sensor_value v;
    if (sensor_sample_fetch(temp_dev) != 0
        || sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &v) != 0) {
        return -EIO;
    }
    /* v.val1 is the integer part in C. v.val2 is the fraction in millionths of 1 C.
     * The +/-5000 before the division rounds to the nearest 0.01 C. */
    int32_t cc = v.val1 * 100 + (v.val2 + (v.val2 >= 0 ? 5000 : -5000)) / 10000;
    if (cc <= INT16_MIN || cc > INT16_MAX) {
        return -EIO;
    }
    *centi_c = (int16_t)cc;
    return 0;
}

void set_override(const struct override *o)
{
    if (o == NULL) {
        memset(&ovr, 0, sizeof(ovr));
    } else {
        ovr = *o;
    }
}

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

/**
 * @brief Shell command "hs ctx". Read both context sensors one time and print the results.
 *
 * The command blocks for the motion window if the accelerometer override is not active.
 * It prints a_rms as an integer in mm/s^2, so the output needs no float support in printf.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments. The function does not use this value.
 * @param[in] argv  Arguments. The function does not use these values.
 *
 * @retval 0  Always.
 */
static int cmd_ctx(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    float rms = 0.0f;
    int16_t centi_c;
    int rm = get_motion(&rms);
    int rt = get_temp(&centi_c);

    shell_print(sh, "motion rc=%d a_rms=%d mm/s^2", rm, rm == 0 ? (int)(rms * 1000.0f) : 0);
    shell_print(sh, "temp rc=%d centi_c=%d", rt, (int)centi_c);
    return 0;
}

SHELL_SUBCMD_ADD((hs), ctx, NULL, "Read context sensors", cmd_ctx, 1, 0);
#endif  // CONFIG_SHELL
