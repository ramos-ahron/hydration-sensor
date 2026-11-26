#ifndef CONTEXT_SENSOR_H
#define CONTEXT_SENSOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Calculates a_rms for a window of 3-axis samples.
 *
 * a_rms is the square root of the mean squared distance between each sample and the mean
 * sample. The function has no side effects.
 *
 * @param[in] a  Samples. Each row is one x, y, z sample.
 * @param[in] n  Number of rows in @p a.
 *
 * @return a_rms in the units of @p a.
 * @retval NAN  @p n is 0.
 */
float a_rms(const float (*a)[3], size_t n);

/**
 * @brief Checks that the accelerometer and the temperature sensor are ready.
 *
 * The function logs the state of each device. A missing device is not an error here.
 * get_motion() and get_temp() report it when they read.
 *
 * @retval 0  Always.
 */
int sensor_init(void);

/**
 * @brief Measures motion as a_rms in m/s^2.
 *
 * The function reads ACCEL_WINDOW_SAMPLES samples, ACCEL_SAMPLE_PERIOD_MS apart, and
 * calculates a_rms(). It blocks for the full window. If the accelerometer override is
 * active, the function uses the override and does not block.
 *
 * @param[out] rms  Receives a_rms in m/s^2. Does not change on failure.
 *
 * @retval 0        Success.
 * @retval -ENODEV  The accelerometer is missing or not ready.
 * @retval -EIO     A read failed, all samples are identical (stuck sensor), or the
 *                  override sets a fault.
 */
int get_motion(float *rms);

/**
 * @brief Reads the fixture temperature from the temperature sensor.
 *
 * The function rounds the reading to the nearest 0.01 C. If the temperature override is
 * active, the function uses the override and does not read the sensor.
 *
 * @param[out] centi_c  Receives the temperature in 0.01 C. Set to INT16_MIN on failure.
 *
 * @retval 0        Success.
 * @retval -ENODEV  The temperature sensor is missing or not ready.
 * @retval -EIO     A read failed, the value is out of the int16_t range, or the override
 *                  sets a fault.
 */
int get_temp(int16_t *centi_c);

// Test hooks
/** @brief Test overrides for the simulation scenarios and the shell. */
struct override {
    bool accel_active;    /**< If true, get_motion() uses the two fields below. */
    bool accel_fault;     /**< If true, get_motion() returns -EIO. */
    float a_rms;          /**< Value that get_motion() returns, in m/s^2. */
    bool temp_active;     /**< If true, get_temp() uses the two fields below. */
    bool temp_fault;      /**< If true, get_temp() returns -EIO. */
    int16_t temp_centi_c; /**< Value that get_temp() returns, in 0.01 C. */
};

/**
 * @brief Sets or clears the test overrides.
 *
 * @param[in] ovr  Overrides to copy. NULL clears every override.
 */
void set_override(const struct override *ovr);

#endif  // CONTEXT_SENSOR_H
