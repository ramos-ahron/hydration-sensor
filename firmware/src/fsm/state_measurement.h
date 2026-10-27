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
    MEASUREMENT_STATE_TRANSMIT,
    MEASUREMENT_STATE_STORE_LOCAL,
    MEASUREMENT_STATE_STORAGE_ERROR,
} measurement_state_e;

/**
 * @brief Initializes the measurement state machine.
 */
void state_measurement_run(void);

/**
 * @brief Processes an event in the measurement state machine.
 *
 * @param event Event to process.
 * @param ble_connected Whether the BLE subsystem is connected.
 * @return true if the event was handled; false otherwise.
 */
bool state_measurement_process_event(state_event_e event, bool ble_connected);

/**
 * @brief Checks whether a measurement operation is in progress.
 *
 * @return true if the measurement state machine is busy; false otherwise.
 */
bool state_measurement_busy(void);

#endif // STATE_MEASUREMENT_H