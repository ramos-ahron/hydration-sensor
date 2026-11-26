#ifndef STATE_SLEEP_H
#define STATE_SLEEP_H

#include "fsm/state_common.h"

/**
 * @brief Enter the system sleep state.
 *
 * @param[in] from  Previous state.
 */
void state_sleep_run(state_top_e from);

#endif  // STATE_SLEEP_H
