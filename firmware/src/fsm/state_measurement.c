#include "fsm/state_measurement.h"
#include "ble/packet.h"
#include "common/enum_to_string.h"
#include "fsm/state_machine.h"
#include "hs/thresholds.h"
#include "pipeline/pipeline.h"
#include "safety/safety.h"
#include "storage/record.h"
#include "ui/led.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(fsm, LOG_LEVEL_INF);

struct measurement_transition {
    measurement_state_e from;
    state_event_e event;
    measurement_state_e to;
};

static const struct measurement_transition measurement_transitions[] = {
    { MEASUREMENT_STATE_M_IDLE, STATE_EVENT_BUTTON_SHORT, MEASUREMENT_STATE_MEASURING },
    { MEASUREMENT_STATE_M_IDLE, STATE_EVENT_AUTO_TRIGGER, MEASUREMENT_STATE_MEASURING },
    { MEASUREMENT_STATE_M_IDLE, STATE_EVENT_TIMER_TRIGGER, MEASUREMENT_STATE_MEASURING },
    { MEASUREMENT_STATE_MEASURING, STATE_EVENT_RETRY, MEASUREMENT_STATE_MEASURING },
    { MEASUREMENT_STATE_MEASURING, STATE_EVENT_ACQUIRED_SAMPLES, MEASUREMENT_STATE_PROCESSING },
    { MEASUREMENT_STATE_MEASURING, STATE_EVENT_SENSOR_FAULT, MEASUREMENT_STATE_SENSOR_ERROR },
    { MEASUREMENT_STATE_PROCESSING, STATE_EVENT_SENSOR_FAULT, MEASUREMENT_STATE_SENSOR_ERROR },
    { MEASUREMENT_STATE_PROCESSING, STATE_EVENT_COMPUTE_DONE, MEASUREMENT_STATE_RESULT_READY },
    { MEASUREMENT_STATE_SENSOR_ERROR, STATE_EVENT_BUTTON_SHORT, MEASUREMENT_STATE_M_IDLE },
    { MEASUREMENT_STATE_RESULT_READY, STATE_EVENT_RESULT_REJECTED, MEASUREMENT_STATE_M_IDLE },
    { MEASUREMENT_STATE_RESULT_READY, STATE_EVENT_RESULT_TO_STORE, MEASUREMENT_STATE_STORE_LOCAL },
    { MEASUREMENT_STATE_STORE_LOCAL, STATE_EVENT_STORE_DONE_CONTINUOUS,
      MEASUREMENT_STATE_MEASURING },
    { MEASUREMENT_STATE_STORE_LOCAL, STATE_EVENT_STORE_DONE_OTHER, MEASUREMENT_STATE_M_IDLE },
    { MEASUREMENT_STATE_STORE_LOCAL, STATE_EVENT_WRITE_ERROR, MEASUREMENT_STATE_STORAGE_ERROR },
    { MEASUREMENT_STATE_STORAGE_ERROR, STATE_EVENT_BUTTON_SHORT, MEASUREMENT_STATE_M_IDLE },
};

static measurement_state_e measurement_state;

/**
 * @brief Stores the result as an unsent packet. The BLE region sends it later.
 *
 * @return 0 on success, or the error from record_put().
 */
static int store_result(void)
{
    struct packet pkt;
    uint8_t buf[PACKET_SIZE];

    packet_init(&pkt, record_session_id(), (uint32_t)(k_uptime_get() / MSEC_PER_SEC),
                record_next_id());
    const size_t len = packet_serialize(&pkt, buf);

    return record_put(buf, len, false, NULL);
}

/**
 * @brief Enter a measurement substate.
 *
 * The function first stores @p to as the current state. Then it asserts that the transition is
 * legal. Then it sets the LED and prepares for the new state:
 * - M_IDLE: LED off.
 * - MEASURING: LED magenta. The function calls pipeline_measure().
 * - PROCESSING: LED magenta. The function calls pipeline_process().
 * - SENSOR_ERROR: LED red. The transition table handles the acknowledgement.
 * - RESULT_READY: The function posts RESULT_REJECTED when the pipeline result has a reject
 *   reason. Otherwise it sets the LED green and posts RESULT_TO_STORE.
 * - STORE_LOCAL: The LED does not change. The function stores the result as unsent and posts
 *   TX_KICK, so the BLE region sends it when a phone is connected. A failed write posts
 *   WRITE_ERROR.
 * - STORAGE_ERROR: LED yellow.
 *
 * @param[in] from  State that the machine leaves.
 * @param[in] to    State that the machine enters.
 */
static void measurement_enter(measurement_state_e from, measurement_state_e to)
{
    ARG_UNUSED(from);
    measurement_state = to;

    switch (to) {
        case MEASUREMENT_STATE_M_IDLE:
            __ASSERT(from == MEASUREMENT_STATE_M_IDLE || from == MEASUREMENT_STATE_SENSOR_ERROR ||
                         from == MEASUREMENT_STATE_RESULT_READY ||
                         from == MEASUREMENT_STATE_STORE_LOCAL ||
                         from == MEASUREMENT_STATE_STORAGE_ERROR,
                     "M_IDLE entered from %d", from);

            // Change when we are able to get hydration status
            led_set(LED_OFF);

            // Transitions
            if (IS_ENABLED(CONFIG_FSM_MEASURE_MODE_TIMER)) {
                fsm_timer_start(FSM_TIMER_MEASURE, STATE_EVENT_TIMER_TRIGGER,
                                CONFIG_MEASURE_INTERVAL_S * MSEC_PER_SEC);
            } else if (IS_ENABLED(CONFIG_FSM_MEASURE_MODE_CONTINUOUS)) {
                fsm_timer_start(FSM_TIMER_MEASURE, STATE_EVENT_AUTO_TRIGGER,
                                CONFIG_MEASURE_INTERVAL_S * MSEC_PER_SEC);
            }
            if (!IS_ENABLED(CONFIG_FSM_MEASURE_MODE_CONTINUOUS)) {
                uint32_t guard_ms = SLEEP_GUARD_MS;

                // Leave extra time to show the result LED after a measurement was stored
                if (from == MEASUREMENT_STATE_STORE_LOCAL) { guard_ms += RESULT_DISPLAY_MS; }

                fsm_timer_start(FSM_TIMER_SLEEP_GUARD, STATE_EVENT_SLEEP_GUARD, guard_ms);
            }

            break;

        case MEASUREMENT_STATE_MEASURING:
            __ASSERT(from == MEASUREMENT_STATE_M_IDLE || from == MEASUREMENT_STATE_MEASURING ||
                         from == MEASUREMENT_STATE_STORE_LOCAL,
                     "MEASURING entered from %d", from);

            led_set(LED_MAGENTA);

            fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
            pipeline_measure();

            break;

        case MEASUREMENT_STATE_PROCESSING:
            __ASSERT(from == MEASUREMENT_STATE_MEASURING, "PROCESSING entered from %d", from);

            led_set(LED_MAGENTA);

            fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
            pipeline_process();

            break;

        case MEASUREMENT_STATE_SENSOR_ERROR:
            __ASSERT(from == MEASUREMENT_STATE_MEASURING || from == MEASUREMENT_STATE_PROCESSING,
                     "SENSOR_ERROR entered from %d", from);

            led_set(LED_RED);

            // Transition table handles ack
            fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);

            break;

        case MEASUREMENT_STATE_RESULT_READY: {
            __ASSERT(from == MEASUREMENT_STATE_PROCESSING, "RESULT_READY entered from %d", from);

            if (pipeline_result()->reject != REJECT_NONE) {
                fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);
                fsm_post(STATE_EVENT_RESULT_REJECTED);
                break;
            }

            led_set(LED_GREEN);
            fsm_post(STATE_EVENT_RESULT_TO_STORE);

            break;
        }

        case MEASUREMENT_STATE_STORE_LOCAL: {
            __ASSERT(from == MEASUREMENT_STATE_RESULT_READY, "STORE_LOCAL entered from %d", from);

            // Every result is stored before it is sent. The BLE region sends unsent records.
            if (store_result() != 0) {
                fsm_post(STATE_EVENT_WRITE_ERROR);
                break;
            }

            fsm_post(IS_ENABLED(CONFIG_FSM_MEASURE_MODE_CONTINUOUS)
                         ? STATE_EVENT_STORE_DONE_CONTINUOUS
                         : STATE_EVENT_STORE_DONE_OTHER);
            fsm_post(STATE_EVENT_TX_KICK);

            break;
        }

        case MEASUREMENT_STATE_STORAGE_ERROR:
            __ASSERT(from == MEASUREMENT_STATE_STORE_LOCAL, "STORAGE_ERROR entered from %d", from);

            led_set(LED_YELLOW);

            fsm_stub(STATE_EVENT_NONE, STATE_EVENT_NONE, STATE_EVENT_NONE);

            break;
    }
}

bool state_measurement_busy(void)
{
    return measurement_state == MEASUREMENT_STATE_MEASURING ||
           measurement_state == MEASUREMENT_STATE_PROCESSING;
}

bool state_measurement_idle(void) { return measurement_state == MEASUREMENT_STATE_M_IDLE; }

bool state_measurement_process_event(state_event_e event)
{
    if (event == STATE_EVENT_BUTTON_SHORT && measurement_state == MEASUREMENT_STATE_M_IDLE &&
        !IS_ENABLED(CONFIG_FSM_MEASURE_MODE_MANUAL)) {
        return false;
    }

    if (measurement_state == MEASUREMENT_STATE_M_IDLE &&
        battery_level() == BATTERY_LEVEL_CRITICAL) {
        return false;
    }

    for (size_t i = 0; i < ARRAY_SIZE(measurement_transitions); i++) {
        const struct measurement_transition *t = &measurement_transitions[i];
        if (t->from == measurement_state && t->event == event) {
            LOG_INF("%s -> %s (%s)", measurement_state_to_string(t->from),
                    measurement_state_to_string(t->to), state_event_to_string(event));
            measurement_enter(t->from, t->to);
            return true;
        }
    }
    return false;
}

void state_measurement_run(void)
{
    LOG_INF("MEASUREMENT start -> %s", measurement_state_to_string(MEASUREMENT_STATE_M_IDLE));
    measurement_enter(MEASUREMENT_STATE_M_IDLE, MEASUREMENT_STATE_M_IDLE);
}
