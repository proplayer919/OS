#ifndef SYS_WATCHDOG_H
#define SYS_WATCHDOG_H

#include <stdint.h>

void watchdog_kick(void);
void watchdog_reset(void);
uint64_t watchdog_last_kick(void);
uint64_t watchdog_timeout_ticks(void);
void watchdog_process(void *arg);

#endif
