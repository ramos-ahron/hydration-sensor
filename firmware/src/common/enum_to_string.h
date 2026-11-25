#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H

#include "fsm/state_ble.h"
#include "fsm/state_common.h"
#include "fsm/state_measurement.h"
#include "ui/led.h"

/**
 * @brief Enum names for the log.
 *
 * When CONFIG_FSM_ENUM_STRINGS is off, macros replace the conversion functions with an inline
 * stub that returns an empty string.
 */
#ifndef CONFIG_FSM_ENUM_STRINGS

/**
 * @brief Stub for the enum-to-string functions.
 *
 * @param[in] val  Enum value to convert. The function does not use this value.
 *
 * @return An empty string.
 */
static inline const char *empty_func(unsigned int val)
{
    (void)val;
    return "";
}

#define state_to_string(state)             empty_func(state)
#define state_event_to_string(event)       empty_func(event)
#define led_color_to_string(color)         empty_func(color)
#define measurement_state_to_string(state) empty_func(state)
#define ble_state_to_string(state)         empty_func(state)

#else

/**
 * @brief Convert a top-level state to its name for logging.
 *
 * @param[in] state  State to convert.
 *
 * @return The name of the state, for example "ACTIVE". The value is an empty string when
 *         @p state is not known.
 */
const char *state_to_string(state_top_e state);

/**
 * @brief Convert a state machine event to its name for logging.
 *
 * @param[in] event  Event to convert.
 *
 * @return The name of the event, for example "BUTTON_SHORT". The value is an empty string when
 *         @p event is not known.
 */
const char *state_event_to_string(state_event_e event);

/**
 * @brief Convert an LED color to its name for logging.
 *
 * @param[in] color  Color to convert.
 *
 * @return The name of the color, for example "MAGENTA". The value is an empty string when
 *         @p color is not known.
 */
const char *led_color_to_string(led_color_e color);

/**
 * @brief Convert a measurement substate to its name for logging.
 *
 * @param[in] state  Measurement state to convert.
 *
 * @return The name of the state, for example "M_IDLE". The value is an empty string when
 *         @p state is not known.
 */
const char *measurement_state_to_string(measurement_state_e state);

/**
 * @brief Convert a BLE substate to its name for logging.
 *
 * @param[in] state  BLE state to convert.
 *
 * @return The name of the state, for example "PAIRING". The value is an empty string when
 *         @p state is not known.
 */
const char *ble_state_to_string(ble_state_e state);

#endif

#endif  // ENUM_TO_STRING_H
