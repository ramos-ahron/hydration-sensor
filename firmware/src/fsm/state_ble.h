#ifndef STATE_BLE_H
#define STATE_BLE_H

#include "fsm/state_common.h"
#include <stdbool.h>

/**
 * @brief Substates of the BLE state machine.
 *
 * The substates follow the BLE link: idle, pairing, connected and disconnected.
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
 * The function disconnects the current link and advertises to bonded peers only.
 */
void state_ble_run(void);

/**
 * @brief Process an event in the BLE state machine.
 *
 * The function makes the nested transition that matches the event and tells if it used the event.
 * When @p measurement_busy is true, the function ignores BUTTON_LONG.
 *
 * @param[in] event             Event to process.
 * @param[in] measurement_busy  true when the measurement state is MEASURING or PROCESSING.
 *
 * @return true when a nested transition used the event, and false when no transition used it.
 */
bool state_ble_process_event(state_event_e event, bool measurement_busy);

/**
 * @brief Check if the BLE subsystem is pairing.
 *
 * @return true when BLE is pairing, and false when it is not.
 */
bool state_ble_pairing(void);

#endif  // STATE_BLE_H
