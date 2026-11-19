#ifndef TYPES_H
#define TYPES_H

typedef enum {
    REJECT_NONE,
    REJECT_CHARGING,
    REJECT_AFE,
    REJECT_MATH,
} reject_e;

struct result {
    reject_e reject;
};

#endif  // TYPES_H
