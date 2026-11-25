#include "fsm/state_machine.h"
#include "hs/thresholds.h"
#include "safety/battery.h"
#include "safety/safety.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

#if defined(CONFIG_HAS_NRFX)
#include <hal/nrf_power.h>
#endif

LOG_MODULE_REGISTER(safety, LOG_LEVEL_INF);

#define BATT_UNKNOWN (-1)

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#define HAS_BATT_ADC 1
static const struct adc_dt_spec batt_adc = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
static bool batt_adc_ok;
#endif

static bool inited;
static bool reset_read;
static bool reset_wdt;
static bool wdt_ok;
static int wdt_id = -1;

static int batt_mv_last = BATT_UNKNOWN;
static int batt_override = BATT_UNKNOWN;
static int vbus_override = -1; /* -1 auto, 0 absent, 1 present */
static battery_level_e batt_lvl = BATTERY_LEVEL_OK;
static K_MUTEX_DEFINE(batt_lock);

static void batt_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(batt_work, batt_work_fn);

/**
 * @brief Read the reset cause one time and clear it.
 *
 * The function can run before safety_init(). A second call does nothing.
 */
static void reset_cause_read(void)
{
    uint32_t cause = 0;

    if (reset_read) {
        return;
    }
    reset_read = true;
    if (hwinfo_get_reset_cause(&cause) == 0) {
        reset_wdt = (cause & RESET_WATCHDOG) != 0;
        (void)hwinfo_clear_reset_cause();
    }
}

/**
 * @brief Read the supply voltage with the ADC.
 *
 * @return Voltage in mV. The value is BATT_UNKNOWN if the build has no ADC channel, the ADC is
 *         not ready, or a read fails.
 */
static int batt_read_hw(void)
{
#ifdef HAS_BATT_ADC
    int16_t raw;
    struct adc_sequence seq = { .buffer = &raw, .buffer_size = sizeof(raw) };
    int32_t mv;

    if (!batt_adc_ok || adc_sequence_init_dt(&batt_adc, &seq) || adc_read_dt(&batt_adc, &seq)) {
        return BATT_UNKNOWN;
    }
    mv = raw;
    if (adc_raw_to_millivolts_dt(&batt_adc, &mv)) {
        return BATT_UNKNOWN;
    }
    return mv < 0 ? 0 : mv;
#else
    return BATT_UNKNOWN;
#endif
}

/**
 * @brief Read the battery, find the level, and tell the state machine about a change.
 *
 * The function holds @c batt_lock while it reads, classifies and posts.
 *
 * @param[in] periodic  true when the periodic work item calls the function.
 */
static void batt_evaluate(bool periodic)
{
    k_mutex_lock(&batt_lock, K_FOREVER);
    const int mv = batt_override >= 0 ? batt_override : batt_read_hw();
    const battery_level_e lvl = batt_classify(batt_lvl, mv);
    const bool changed = lvl != batt_lvl;

    batt_mv_last = mv;
    batt_lvl = lvl;
    if (changed || (periodic && lvl == BATTERY_LEVEL_CRITICAL)) {
        fsm_post(STATE_EVENT_BATTERY_CHANGED);
    }
    k_mutex_unlock(&batt_lock);
    if (changed) {
        LOG_INF("battery %d mV, level %d", mv, lvl);
    }
}

/**
 * @brief Work handler for @c batt_work. Check the battery, then schedule the next check.
 *
 * The system work queue thread runs this function every BATTERY_PERIOD_MS.
 *
 * @param[in] work  Work item that expired. The function does not use this argument.
 */
static void batt_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    batt_evaluate(true);
    (void)k_work_schedule(&batt_work, K_MSEC(BATTERY_PERIOD_MS));
}

/**
 * @brief Set a fixed battery voltage for tests, or return to the hardware reading.
 *
 * The function checks the battery again at once.
 *
 * @param[in] mv  Voltage in mV to use. A negative value means use the ADC.
 */
void safety_set_batt_override(int mv)
{
    batt_override = mv < 0 ? BATT_UNKNOWN : mv;
    batt_evaluate(false);
}

/**
 * @brief Set a fixed VBUS state for tests, or return to the hardware reading.
 *
 * @param[in] v  0 means VBUS is absent. Any positive value means VBUS is present. A negative
 *               value means use the hardware.
 */
void safety_set_vbus_override(int v)
{
    vbus_override = v < 0 ? -1 : (v != 0);
}

int safety_init(void)
{
    const struct device *hw_wdt = NULL;
    int err;

    if (inited) {
        return 0;
    }
    inited = true;
    reset_cause_read();

#if defined(CONFIG_TASK_WDT_HW_FALLBACK) && defined(CONFIG_WATCHDOG)
    hw_wdt = DEVICE_DT_GET_OR_NULL(DT_ALIAS(watchdog0));
    if (hw_wdt != NULL && !device_is_ready(hw_wdt)) {
        hw_wdt = NULL;
    }
#endif
    err = task_wdt_init(hw_wdt);
    wdt_ok = err == 0;
    if (!wdt_ok) {
        LOG_WRN("task watchdog init failed (err %d); drive runs unguarded", err);
    }
    /*
     * Arm the hardware fallback now. The nRF WDT continues to run after a soft reset, and
     * nothing can stop it. Thus the firmware must feed it from boot, and not only from the
     * first measurement.
     */
    safety_drive_begin();
    safety_drive_end();

#ifdef HAS_BATT_ADC
    batt_adc_ok = adc_is_ready_dt(&batt_adc) && adc_channel_setup_dt(&batt_adc) == 0;
    if (!batt_adc_ok) {
        LOG_WRN("battery ADC unavailable");
    }
#else
    LOG_WRN("no battery ADC channel");
#endif

    batt_evaluate(false);
    (void)k_work_schedule(&batt_work, K_MSEC(BATTERY_PERIOD_MS));
    return 0;
}

bool safety_vbus_present(void)
{
    if (vbus_override >= 0) {
        return vbus_override != 0;
    }
#if defined(NRF_POWER) && NRF_POWER_HAS_USBREG
    return nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
#else
    return false;
#endif
}

int safety_check(void)
{
    if (!safety_vbus_present()) {
        return 0;
    }
    if (IS_ENABLED(CONFIG_BENCH_ALLOW_USB)) {
        LOG_INF("VBUS present, bench override lets the measurement through");
        return 0;
    }
    LOG_INF("VBUS present, measurement locked out");
    return -EPERM;
}

void safety_drive_begin(void)
{
    if (!wdt_ok) {
        return;
    }
    if (wdt_id >= 0) {
        (void)task_wdt_feed(wdt_id);
        return;
    }
    wdt_id = task_wdt_add(DRIVE_WDT_MS, NULL, NULL);
    if (wdt_id < 0) {
        LOG_WRN("drive watchdog add failed (err %d)", wdt_id);
    }
}

void safety_drive_feed(void)
{
    if (wdt_id >= 0) {
        (void)task_wdt_feed(wdt_id);
    }
}

void safety_drive_end(void)
{
    if (wdt_id >= 0) {
        (void)task_wdt_delete(wdt_id);
        wdt_id = -1;
    }
}

bool safety_reset_was_watchdog(void)
{
    reset_cause_read();
    return reset_wdt;
}

int battery_mv(void)
{
    return batt_mv_last;
}

battery_level_e battery_level(void)
{
    return batt_lvl;
}

bool battery_above_margin(void)
{
    const int mv = batt_mv_last;

    return mv < 0 || mv >= BATT_CRITICAL_MV + BATT_MARGIN_MV;
}

#ifdef CONFIG_SHELL
#include <string.h>

#include <zephyr/shell/shell.h>

static const char *const level_name[] = { "OK", "LOW", "CRITICAL" };

/**
 * @brief Shell command "hs batt [<mv>|auto]". Show the battery, and optionally set the override.
 *
 * With an argument, the command sets the override to a voltage, or to the ADC with "auto". It
 * then prints the voltage, the level and the override.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments.
 * @param[in] argv  Arguments. The optional @c argv[1] is a voltage in mV or "auto".
 *
 * @retval 0        The function printed the state.
 * @retval -EINVAL  @c argv[1] is not "auto" or a number that is zero or more. The function
 *                  prints the usage.
 */
static int cmd_batt(const struct shell *sh, size_t argc, char **argv)
{
    int err = 0;

    if (argc > 1) {
        if (strcmp(argv[1], "auto") == 0) {
            safety_set_batt_override(-1);
        } else {
            const long mv = shell_strtol(argv[1], 10, &err);

            if (err || mv < 0) {
                shell_error(sh, "usage: hs batt [<mv>|auto]");
                return -EINVAL;
            }
            safety_set_batt_override((int)mv);
        }
    }
    if (batt_override >= 0) {
        shell_print(sh, "batt mv=%d level=%s override=%d", battery_mv(),
                    level_name[battery_level()], batt_override);
    } else {
        shell_print(sh, "batt mv=%d level=%s override=off", battery_mv(),
                    level_name[battery_level()]);
    }
    return 0;
}

/**
 * @brief Shell command "hs vbus [0|1|auto]". Show VBUS, and optionally set the override.
 *
 * With an argument, the command sets the override. It then prints the VBUS state, the override
 * and the setting of CONFIG_BENCH_ALLOW_USB.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments.
 * @param[in] argv  Arguments. The optional @c argv[1] is "0", "1" or "auto".
 *
 * @retval 0        The function printed the state.
 * @retval -EINVAL  @c argv[1] is not "0", "1" or "auto". The function prints the usage.
 */
static int cmd_vbus(const struct shell *sh, size_t argc, char **argv)
{
    if (argc > 1) {
        if (strcmp(argv[1], "auto") == 0) {
            safety_set_vbus_override(-1);
        } else if (strcmp(argv[1], "0") == 0 || strcmp(argv[1], "1") == 0) {
            safety_set_vbus_override(argv[1][0] - '0');
        } else {
            shell_error(sh, "usage: hs vbus [0|1|auto]");
            return -EINVAL;
        }
    }
    const int bench = IS_ENABLED(CONFIG_BENCH_ALLOW_USB);

    if (vbus_override >= 0) {
        shell_print(sh, "vbus present=%d override=%d bench_allow=%d", safety_vbus_present(),
                    vbus_override, bench);
    } else {
        shell_print(sh, "vbus present=%d override=off bench_allow=%d", safety_vbus_present(),
                    bench);
    }
    return 0;
}

SHELL_SUBCMD_ADD((hs), batt, NULL, "Battery: hs batt [<mv>|auto]", cmd_batt, 1, 1);
SHELL_SUBCMD_ADD((hs), vbus, NULL, "VBUS: hs vbus [0|1|auto]", cmd_vbus, 1, 1);
#endif  // CONFIG_SHELL
