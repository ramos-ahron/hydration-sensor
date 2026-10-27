#ifndef STATE_INIT_FAULT_H
#define STATE_INIT_FAULT_H

#include "fsm/state_common.h"

// On init fail, LED turns red
void state_init_fault_run(state_e from);

#endif // STATE_INIT_FAULT_H
