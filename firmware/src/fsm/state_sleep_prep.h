#ifndef STATE_SLEEP_PREP_H
#define STATE_SLEEP_PREP_H

#include "fsm/state_common.h"

/**
 * @brief Prepare the system for the low-power sleep state.
 *
 * The function stops the pipeline and the advertising, disconnects the link and posts
 * PREP_COMPLETE.
 *
 * @param[in] from  Previous state.
 */
void state_sleep_prep_run(state_e from);

#endif  // STATE_SLEEP_PREP_H
