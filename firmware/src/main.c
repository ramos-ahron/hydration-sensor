#include "fsm/state_machine.h"
#include <zephyr/kernel.h>

/**
 * @brief Application entry point.
 *
 * Initializes the state machine, then blocks on fsm_msgq forever and passes each received
 * event to state_machine_process_event().
 *
 * @return Never returns; the final return statement is unreachable.
 */
int main(void)
{
    state_machine_init();

    for (;;) {
        state_event_e event;
        k_msgq_get(&fsm_msgq, &event, K_FOREVER);
        state_machine_process_event(event);
    }
    return 0;
}
