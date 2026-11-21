#ifndef STATE_MEASUREMENT_H
#define STATE_MEASUREMENT_H

#include "fsm/state_common.h"
#include <stdbool.h>

/**
 * @brief Defines the states of the measurement state machine.
 */
typedef enum {
    MEASUREMENT_STATE_M_IDLE,
    MEASUREMENT_STATE_MEASURING,
    MEASUREMENT_STATE_PROCESSING,
    MEASUREMENT_STATE_SENSOR_ERROR,
    MEASUREMENT_STATE_RESULT_READY,
    MEASUREMENT_STATE_STORE_LOCAL,
    MEASUREMENT_STATE_STORAGE_ERROR,
} measurement_state_e;

/**
 * @brief Initialize the measurement state machine.
 */
void state_measurement_run(void);

/**
 * @brief Process an event in the measurement state machine.
 *
 * @param[in] event  Event to process.
 *
 * @return true when the event caused a transition, and false when it did not.
 */
bool state_measurement_process_event(state_event_e event);

/**
 * @brief Check if a measurement operation is in progress.
 *
 * @return true when the measurement state machine is busy, and false when it is not.
 */
bool state_measurement_busy(void);

/**
 * @brief Check if the measurement state machine is in M_IDLE.
 *
 * @return true when the measurement state machine is idle, and false when it is not.
 */
bool state_measurement_idle(void);

#endif  // STATE_MEASUREMENT_H
