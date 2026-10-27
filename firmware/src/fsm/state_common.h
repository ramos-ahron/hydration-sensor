#ifndef STATE_COMMON_H
#define STATE_COMMON_H

/**
 * @brief Defines the top-level states of the state machine.
 */
typedef enum {
    STATE_INIT,
    STATE_INIT_FAULT,
    STATE_ACTIVE,
    STATE_SLEEP_PREP,
} state_e;

/**
 * @brief Defines events that trigger state machine transitions.
 */
typedef enum {
    // Buttons
    STATE_EVENT_BUTTON_SHORT,
    STATE_EVENT_BUTTON_LONG,

    // Top level
    STATE_EVENT_INIT_OK,
    STATE_EVENT_INIT_FAIL,
    STATE_EVENT_WATCHDOG_RESET,
    STATE_EVENT_PREP_COMPLETE,
    STATE_EVENT_SLEEP_GUARD,
    STATE_EVENT_BATTERY_CRITICAL,
    STATE_EVENT_BATTERY_MARGIN_OK,

    // Measurement
    STATE_EVENT_AUTO_TRIGGER,
    STATE_EVENT_TIMER_TRIGGER,
    STATE_EVENT_RETRY,
    STATE_EVENT_ACQUIRED_SAMPLES,
    STATE_EVENT_SENSOR_FAULT,
    STATE_EVENT_COMPUTE_DONE,
    STATE_EVENT_RESULT_REJECTED,
    STATE_EVENT_RESULT_TO_BLE,
    STATE_EVENT_RESULT_TO_STORE,
    STATE_EVENT_TRANSMIT_DONE,
    STATE_EVENT_STORE_DONE_CONTINUOUS,
    STATE_EVENT_STORE_DONE_OTHER,
    STATE_EVENT_WRITE_ERROR,

    // BLE
    STATE_EVENT_CONNECTION_COMPLETE,
    STATE_EVENT_PAIRING_TIMEOUT,
    STATE_EVENT_LINK_LOST,
    STATE_EVENT_READVERTISE,

    // Battery
    STATE_EVENT_BATTERY_BELOW,
    STATE_EVENT_BATTERY_RECOVERED,
    STATE_EVENT_BATTERY_BELOW_CRITICAL,
    STATE_EVENT_BATTERY_ABOVE_CRITICAL,

    // Temp
    STATE_EVENT_NONE,
} state_event_e;

/**
 * @brief Defines the temporary delay for stubbed state transitions.
 *
 * Used to simulate timed transitions until the actual state logic
 * is implemented.
 */
#define FSM_STUB_DWELL_MS 5000

#endif // STATE_COMMON_H
