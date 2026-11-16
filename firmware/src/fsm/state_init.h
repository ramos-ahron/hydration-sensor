#ifndef STATE_INIT_H
#define STATE_INIT_H

#include "fsm/state_common.h"

/**
 * @brief Enter the initialization state.
 *
 * @param[in] from  State that the machine leaves to enter STATE_INIT.
 */
void state_init_run(state_e from);

#endif  // STATE_INIT_H
