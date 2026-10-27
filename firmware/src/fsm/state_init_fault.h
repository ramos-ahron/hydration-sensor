#ifndef STATE_INIT_FAULT_H
#define STATE_INIT_FAULT_H

#include "fsm/state_common.h"

/**
 * @brief Enters the initialization fault state.
 *
 * @param from The state transitioning into STATE_INIT_FAULT.
 */
void state_init_fault_run(state_e from);

#endif // STATE_INIT_FAULT_H
