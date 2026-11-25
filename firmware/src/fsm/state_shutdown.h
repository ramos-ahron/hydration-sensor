#ifndef STATE_SHUTDOWN_H
#define STATE_SHUTDOWN_H

#include "fsm/state_common.h"

// TODO: Decide if SHUTDOWN needs a preparation state.

/**
 * @brief Enter the shutdown state.
 *
 * @param[in] from  Previous state.
 */
void state_shutdown_run(state_e from);

#endif  // STATE_SHUTDOWN_H
