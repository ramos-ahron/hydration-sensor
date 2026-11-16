#include "fsm/state_machine.h"
#include <zephyr/kernel.h>

/**
 * @brief Application entry point.
 *
 * The function initializes the state machine. Then it waits on fsm_msgq forever and sends each
 * event that it receives to state_machine_process_event().
 *
 * @return The function does not return. The return statement at the end is not reachable.
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
