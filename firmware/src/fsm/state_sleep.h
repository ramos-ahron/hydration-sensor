#ifndef STATE_SLEEP_H
#define STATE_SLEEP_H

#include "fsm/state_common.h"

/**
 * @brief Enters the system sleep state.
 *
 * @param from Previous state.
 */
void state_sleep_run(state_e from);

#endif // STATE_SLEEP_H
