
#ifndef LED_H
#define LED_H

typedef enum {
    LED_OFF = 0,
    LED_RED = 1,
    LED_GREEN = 2,
    LED_YELLOW = 3,  /* R + G */
    LED_BLUE = 4,
    LED_MAGENTA = 5, /* R + B */
    LED_CYAN = 6,    /* G + B */
    LED_WHITE = 7,   /* R + G + B */
} led_color_e;

void led_set(led_color_e color);

#endif // LED_H
