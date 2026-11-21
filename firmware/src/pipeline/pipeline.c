#include "pipeline/pipeline.h"
#include "fsm/state_machine.h"
#include "hs/bioz.h"
#include "safety/safety.h"
#include "sensors/afe.h"
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pipeline, LOG_LEVEL_INF);

#define PIPELINE_STACK_SIZE 2048

// Lower priority than the FSM thread, so the FSM stays responsive during a step.
#define PIPELINE_PRIORITY 5

static struct result result;

/**
 * @brief Checks the return value of a stub.
 *
 * @param[in] err   Return value.
 * @param[in] name  Function name for the log.
 *
 * @return true when @p err is a failure. -ENOTSUP is not a failure.
 */
static bool failed(int err, const char *name)
{
    if (err == -ENOTSUP) {
        LOG_WRN("%s not implemented", name);
        return false;
    }
    if (err != 0) { LOG_ERR("%s failed (err %d)", name, err); }
    return err != 0;
}

/**
 * @brief Work handler that acquires the samples.
 *
 * The function clears the result. If safety_check() fails, it sets REJECT_CHARGING and posts
 * ACQUIRED_SAMPLES. Otherwise it runs afe_measure() with the drive on. A failed measurement stops
 * the AFE, sets REJECT_AFE, and posts SENSOR_FAULT. A good measurement posts ACQUIRED_SAMPLES.
 *
 * @param[in] work  Work item that ran. The function does not use this value.
 */
static void measure_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    result = (struct result){ .reject = REJECT_NONE };

    if (safety_check() != 0) {
        result.reject = REJECT_CHARGING;
        fsm_post(STATE_EVENT_ACQUIRED_SAMPLES);
        return;
    }

    safety_drive_begin();
    const int err = afe_measure();
    safety_drive_end();

    if (failed(err, "afe_measure")) {
        (void)afe_stop();
        result.reject = REJECT_AFE;
        fsm_post(STATE_EVENT_SENSOR_FAULT);
        return;
    }
    fsm_post(STATE_EVENT_ACQUIRED_SAMPLES);
}

/**
 * @brief Work handler that computes the result.
 *
 * The function runs bioz_calibrate(), bioz_fit(), bioz_quality() and bioz_score() in this order.
 * It stops at the first step that fails and sets REJECT_MATH. The function always posts
 * COMPUTE_DONE.
 *
 * @param[in] work  Work item that ran. The function does not use this value.
 */
static void process_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    static const struct {
        const char *name;
        int (*fn)(void);
    } steps[] = {
        { "bioz_calibrate", bioz_calibrate },
        { "bioz_fit", bioz_fit },
        { "bioz_quality", bioz_quality },
        { "bioz_score", bioz_score },
    };

    for (size_t i = 0; i < ARRAY_SIZE(steps) && result.reject == REJECT_NONE; i++) {
        if (failed(steps[i].fn(), steps[i].name)) { result.reject = REJECT_MATH; }
    }
    fsm_post(STATE_EVENT_COMPUTE_DONE);
}

/*
 * The steps block their queue while they run. They get their own queue, so a step that waits for
 * a MAX30009 trigger on the system work queue (MAX30009_TRIGGER_GLOBAL_THREAD) still wakes.
 */
static K_THREAD_STACK_DEFINE(pipeline_stack, PIPELINE_STACK_SIZE);
static struct k_work_q pipeline_q;

static K_WORK_DEFINE(measure_work, measure_handler);
static K_WORK_DEFINE(process_work, process_handler);

/**
 * @brief Starts the pipeline work queue.
 *
 * Zephyr calls this function at boot through SYS_INIT.
 *
 * @retval 0  Always.
 */
static int pipeline_init(void)
{
    static const struct k_work_queue_config cfg = { .name = "pipeline" };

    k_work_queue_init(&pipeline_q);
    k_work_queue_start(&pipeline_q, pipeline_stack, K_THREAD_STACK_SIZEOF(pipeline_stack),
                       PIPELINE_PRIORITY, &cfg);
    return 0;
}
SYS_INIT(pipeline_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void pipeline_measure(void) { (void)k_work_submit_to_queue(&pipeline_q, &measure_work); }

void pipeline_process(void) { (void)k_work_submit_to_queue(&pipeline_q, &process_work); }

const struct result *pipeline_result(void) { return &result; }

void pipeline_stop(void)
{
    struct k_work_sync sync;

    (void)k_work_cancel_sync(&measure_work, &sync);
    (void)k_work_cancel_sync(&process_work, &sync);
    (void)afe_stop();
}
