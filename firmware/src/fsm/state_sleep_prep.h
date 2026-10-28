#ifndef STATE_SLEEP_PREP_H
#define STATE_SLEEP_PREP_H

#include "fsm/state_common.h"

/**
 * @brief Prepares the system to enter a low-power sleep state.
 *
 * Performs the required shutdown or cleanup operations before the
 * system enters sleep.
 *
 * @param from Previous state.
 */
void state_sleep_prep_run(state_e from);

#endif  // STATE_SLEEP_PREP_H
