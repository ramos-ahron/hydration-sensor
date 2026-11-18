#include "common/enum_to_string.h"

#ifdef CONFIG_FSM_ENUM_STRINGS

const char *state_to_string(state_e value)
{
    switch (value) {
        case STATE_INIT:
            return "INIT";
        case STATE_INIT_FAULT:
            return "INIT_FAULT";
        case STATE_ACTIVE:
            return "ACTIVE";
        case STATE_SLEEP_PREP:
            return "SLEEP_PREP";
        case STATE_SLEEP:
            return "SLEEP";
        case STATE_SHUTDOWN:
            return "SHUTDOWN";
    }
    return "";
}

const char *state_event_to_string(state_event_e value)
{
    switch (value) {
        case STATE_EVENT_BUTTON_SHORT:
            return "BUTTON_SHORT";
        case STATE_EVENT_BUTTON_LONG:
            return "BUTTON_LONG";
        case STATE_EVENT_INIT_OK:
            return "INIT_OK";
        case STATE_EVENT_INIT_FAIL:
            return "INIT_FAIL";
        case STATE_EVENT_PREP_COMPLETE:
            return "PREP_COMPLETE";
        case STATE_EVENT_SLEEP_GUARD:
            return "SLEEP_GUARD";
        case STATE_EVENT_BATTERY_CRITICAL:
            return "BATTERY_CRITICAL";
        case STATE_EVENT_BATTERY_MARGIN_OK:
            return "BATTERY_MARGIN_OK";
        case STATE_EVENT_AUTO_TRIGGER:
            return "AUTO_TRIGGER";
        case STATE_EVENT_TIMER_TRIGGER:
            return "TIMER_TRIGGER";
        case STATE_EVENT_RETRY:
            return "RETRY";
        case STATE_EVENT_ACQUIRED_SAMPLES:
            return "ACQUIRED_SAMPLES";
        case STATE_EVENT_SENSOR_FAULT:
            return "SENSOR_FAULT";
        case STATE_EVENT_COMPUTE_DONE:
            return "COMPUTE_DONE";
        case STATE_EVENT_RESULT_REJECTED:
            return "RESULT_REJECTED";
        case STATE_EVENT_RESULT_TO_BLE:
            return "RESULT_TO_BLE";
        case STATE_EVENT_RESULT_TO_STORE:
            return "RESULT_TO_STORE";
        case STATE_EVENT_TRANSMIT_DONE:
            return "TRANSMIT_DONE";
        case STATE_EVENT_STORE_DONE_CONTINUOUS:
            return "STORE_DONE_CONTINUOUS";
        case STATE_EVENT_STORE_DONE_OTHER:
            return "STORE_DONE_OTHER";
        case STATE_EVENT_WRITE_ERROR:
            return "WRITE_ERROR";
        case STATE_EVENT_CONNECTION_COMPLETE:
            return "CONNECTION_COMPLETE";
        case STATE_EVENT_PAIRING_TIMEOUT:
            return "PAIRING_TIMEOUT";
        case STATE_EVENT_LINK_LOST:
            return "LINK_LOST";
        case STATE_EVENT_BLE_IDLE:
            return "BLE_IDLE";
        case STATE_EVENT_BLE_SUBSCRIBED:
            return "BLE_SUBSCRIBED";
        case STATE_EVENT_TX_KICK:
            return "TX_KICK";
        case STATE_EVENT_TX_DONE:
            return "TX_DONE";
        case STATE_EVENT_BATTERY_CHANGED:
            return "BATTERY_CHANGED";
        case STATE_EVENT_NONE:
            return "NONE";
    }
    return "";
}

const char *led_color_to_string(led_color_e value)
{
    switch (value) {
        case LED_OFF:
            return "OFF";
        case LED_RED:
            return "RED";
        case LED_GREEN:
            return "GREEN";
        case LED_YELLOW:
            return "YELLOW";
        case LED_BLUE:
            return "BLUE";
        case LED_MAGENTA:
            return "MAGENTA";
        case LED_CYAN:
            return "CYAN";
        case LED_WHITE:
            return "WHITE";
    }
    return "";
}

const char *measurement_state_to_string(measurement_state_e value)
{
    switch (value) {
        case MEASUREMENT_STATE_M_IDLE:
            return "M_IDLE";
        case MEASUREMENT_STATE_MEASURING:
            return "MEASURING";
        case MEASUREMENT_STATE_PROCESSING:
            return "PROCESSING";
        case MEASUREMENT_STATE_SENSOR_ERROR:
            return "SENSOR_ERROR";
        case MEASUREMENT_STATE_RESULT_READY:
            return "RESULT_READY";
        case MEASUREMENT_STATE_TRANSMIT:
            return "TRANSMIT";
        case MEASUREMENT_STATE_STORE_LOCAL:
            return "STORE_LOCAL";
        case MEASUREMENT_STATE_STORAGE_ERROR:
            return "STORAGE_ERROR";
    }
    return "";
}

const char *ble_state_to_string(ble_state_e value)
{
    switch (value) {
        case BLE_STATE_BLE_IDLE:
            return "BLE_IDLE";
        case BLE_STATE_PAIRING:
            return "PAIRING";
        case BLE_STATE_CONNECTED:
            return "CONNECTED";
        case BLE_STATE_DISCONNECTED:
            return "DISCONNECTED";
    }
    return "";
}

#endif  // CONFIG_FSM_ENUM_STRINGS
