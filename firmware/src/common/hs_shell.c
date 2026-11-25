#ifdef CONFIG_SHELL
#include "common/enum_to_string.h"
#include "fsm/state_machine.h"
#include "ui/led.h"

#include <string.h>

#include <zephyr/shell/shell.h>

/**
 * @brief Root shell command "hs".
 *
 * Other modules add a subcommand with
 * SHELL_SUBCMD_ADD((hs), name, subcmds, help, handler, mandatory, optional).
 */
SHELL_SUBCMD_SET_CREATE(hs_cmds, (hs));
SHELL_CMD_REGISTER(hs, &hs_cmds, "Sensor commands", NULL);

/**
 * @brief Shell command "hs press <short|long>". Send a button event to the state machine.
 *
 * The command posts the same event as the physical button. Use it for bench checks without the
 * button.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments. The function does not use this value.
 * @param[in] argv  Arguments. @c argv[1] is "short" or "long".
 *
 * @retval 0        The function posted the event.
 * @retval -EINVAL  @c argv[1] is not "short" or "long". The function prints the usage.
 */
static int cmd_press(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    if (strcmp(argv[1], "short") == 0) {
        fsm_post(STATE_EVENT_BUTTON_SHORT);
    } else if (strcmp(argv[1], "long") == 0) {
        fsm_post(STATE_EVENT_BUTTON_LONG);
    } else {
        shell_error(sh, "usage: hs press <short|long>");
        return -EINVAL;
    }
    return 0;
}
SHELL_SUBCMD_ADD((hs), press, NULL, "Inject a button press: hs press <short|long>", cmd_press, 2,
                 0);

/**
 * @brief Shell command "hs led". Print the color that the LED shows now.
 *
 * The value is OFF during the dark phase of a blink pattern. Two calls can give different
 * results for a blinking LED.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments. The function does not use this value.
 * @param[in] argv  Arguments. The function does not use these values.
 *
 * @retval 0  The function printed the color.
 */
static int cmd_led(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "led=%s", led_color_to_string(led_current()));
    return 0;
}
SHELL_SUBCMD_ADD((hs), led, NULL, "Show the LED color being driven", cmd_led, 1, 0);
#endif  // CONFIG_SHELL
