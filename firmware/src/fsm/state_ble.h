#ifndef STATE_BLE_H
#define STATE_BLE_H

#include "fsm/state_common.h"
#include <stdbool.h>

/**
 * @brief Defines the substates of the BLE state machine.
 *
 * Represents the BLE connection lifecycle, from idle and pairing to
 * connected or disconnected states.
 */
typedef enum {
    BLE_STATE_BLE_IDLE,
    BLE_STATE_PAIRING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCONNECTED,
} ble_state_e;

/**
 * @brief Enters the initial BLE substate.
 *
 * Currently logs the transition to BLE_IDLE without controlling the LED.
 */
void state_ble_run(void);

/**
 * @brief Processes an event within the BLE state machine.
 *
 * Handles nested state transitions and indicates whether the event was
 * consumed. Uses @p measurement_busy as a guard for BLE state transitions.
 *
 * @param event Event to process.
 * @param measurement_busy True if the measurement state is Processing,
 *                         Measuring, or Transmitting.
 *
 * @return True if a nested transition consumed the event, false otherwise.
 */
bool state_ble_process_event(state_event_e event, bool measurement_busy);

/**
 * @brief Checks whether the BLE subsystem is connected.
 *
 * @return true if BLE is connected, false otherwise.
 */
bool state_ble_connected(void);

/**
 * @brief Checks whether the BLE subsystem is pairing.
 *
 * @return true if BLE is pairing, false otherwise.
 */
bool state_ble_pairing(void);

#endif  // STATE_BLE_H
