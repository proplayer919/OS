#ifndef SYS_INIT_H
#define SYS_INIT_H

#include <stddef.h>

void init_run(void);
size_t init_unit_count(void);
const char *init_unit_name(size_t index);

#endif
