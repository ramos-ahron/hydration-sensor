#include "fsm/state_machine.h"
#include <zephyr/kernel.h>

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
