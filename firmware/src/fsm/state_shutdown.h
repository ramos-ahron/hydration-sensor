#ifndef STATE_SHUTDOWN_H
#define STATE_SHUTDOWN_H

#include "fsm/state_common.h"

// TODO: Add a shutdown prep?

/**
 * @brief Executes the system shutdown state.
 *
 * @param from Previous state.
 */
void state_shutdown_run(state_e from);

#endif  // STATE_SHUTDOWN_H
