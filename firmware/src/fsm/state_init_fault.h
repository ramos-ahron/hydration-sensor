#ifndef STATE_INIT_FAULT_H
#define STATE_INIT_FAULT_H

#include "fsm/state_common.h"

/**
 * @brief Enter the initialization fault state.
 *
 * @param[in] from  State that the machine leaves to enter STATE_INIT_FAULT.
 */
void state_init_fault_run(state_top_e from);

#endif  // STATE_INIT_FAULT_H
