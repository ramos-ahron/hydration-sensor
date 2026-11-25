#include "sensors/sensor.h"
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <zephyr/ztest.h>

/**
 * @brief Removes the sensor override before each test.
 *
 * @param[in] f  Test fixture. The function does not use this value.
 */
static void before(void *f)
{
    ARG_UNUSED(f);
    set_override(NULL);
}
ZTEST_SUITE(a_rms, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(sensor_io, NULL, NULL, before, NULL, NULL);

ZTEST(a_rms, test_constant_is_zero)
{
    const float a[4][3] = { { 1, 2, 3 }, { 1, 2, 3 }, { 1, 2, 3 }, { 1, 2, 3 } };

    zassert_within(a_rms(a, 4), 0.0f, 1e-6f);
}

ZTEST(a_rms, test_empty_is_nan)
{
    const float a[1][3] = { { 0 } };

    zassert_true(isnan(a_rms(a, 0)));
}

ZTEST(a_rms, test_alternating)
{
    const float a[4][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };

    zassert_within(a_rms(a, 4), 1.0f, 1e-6f);
}

ZTEST(a_rms, test_gravity_offset_cancels)
{
    const float a[4][3] = { { 1, 0, 9.81f }, { -1, 0, 9.81f }, { 1, 0, 9.81f }, { -1, 0, 9.81f } };

    zassert_within(a_rms(a, 4), 1.0f, 1e-4f); /* the constant z offset cancels */
}

ZTEST(a_rms, test_three_axis)
{
    const float a[2][3] = { { 1, 1, 1 }, { 3, 5, 7 } };

    /* mean (2, 3, 4), deviations +/-(1, 2, 3): sum of squares 28, / 2 = 14 */
    zassert_within(a_rms(a, 2), sqrtf(14.0f), 1e-5f);
}

ZTEST(sensor_io, test_no_device)
{
    float rms = -1.0f;
    int16_t t = 0;

    zassert_equal(get_motion(&rms), -ENODEV);
    zassert_within(rms, -1.0f, 0.0f); /* unchanged on failure */
    zassert_equal(get_temp(&t), -ENODEV);
    zassert_equal(t, INT16_MIN);
}

ZTEST(sensor_io, test_overrides)
{
    float rms = 0.0f;
    int16_t t = 0;
    struct override o = {
        .accel_active = true, .a_rms = 1.5f, .temp_active = true, .temp_centi_c = 3150
    };

    set_override(&o);
    zassert_equal(get_motion(&rms), 0);
    zassert_within(rms, 1.5f, 1e-6f);
    zassert_equal(get_temp(&t), 0);
    zassert_equal(t, 3150);

    o.accel_fault = true;
    o.temp_fault = true;
    set_override(&o);
    zassert_equal(get_motion(&rms), -EIO);
    zassert_within(rms, 1.5f, 1e-6f); /* unchanged on failure */
    zassert_equal(get_temp(&t), -EIO);
    zassert_equal(t, INT16_MIN);
}

ZTEST(sensor_io, test_override_is_copied)
{
    float rms = 0.0f;
    struct override o = { .accel_active = true, .a_rms = 2.0f };

    set_override(&o);
    o.a_rms = 9.0f; /* a change to the caller's copy has no effect */
    zassert_equal(get_motion(&rms), 0);
    zassert_within(rms, 2.0f, 1e-6f);
}

ZTEST(sensor_io, test_clear_restores_device)
{
    float rms = 0.0f;
    int16_t t = 0;
    struct override o = { .accel_active = true, .temp_active = true };

    set_override(&o);
    set_override(NULL);
    zassert_equal(get_motion(&rms), -ENODEV);
    zassert_equal(get_temp(&t), -ENODEV);
}
