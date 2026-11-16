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
 * @brief Enter the initial BLE substate.
 *
 * At this time, the function logs the transition to BLE_IDLE. It does not change the LED.
 */
void state_ble_run(void);

/**
 * @brief Process an event in the BLE state machine.
 *
 * The function makes the nested transition that matches the event and tells if it used the event.
 * When @p measurement_busy is true, the function ignores BUTTON_LONG.
 *
 * @param[in] event             Event to process.
 * @param[in] measurement_busy  true when the measurement state is MEASURING, PROCESSING or
 *                              TRANSMIT.
 *
 * @return true when a nested transition used the event, and false when no transition used it.
 */
bool state_ble_process_event(state_event_e event, bool measurement_busy);

/**
 * @brief Check if the BLE subsystem is connected.
 *
 * @return true when BLE is connected, and false when it is not.
 */
bool state_ble_connected(void);

/**
 * @brief Check if the BLE subsystem is pairing.
 *
 * @return true when BLE is pairing, and false when it is not.
 */
bool state_ble_pairing(void);

#endif  // STATE_BLE_H
