#ifndef DRIVER_H
#define DRIVER_H

#include <stddef.h>

typedef struct driver {
    const char *name;
    int (*init)(void);
} driver_t;

void drivers_init(void);

#endif
