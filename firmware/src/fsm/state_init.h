#ifndef STATE_INIT_H
#define STATE_INIT_H

#include "fsm/state_common.h"

/**
 * @brief Enters the initialization state.
 *
 * @param from The state transitioning into STATE_INIT.
 */
void state_init_run(state_e from);

#endif // STATE_INIT_H
