#include "sys/watchdog.h"
#include "sys/panic.h"
#include "proc/process.h"

static volatile uint64_t watchdog_last_kick_ticks;
static const uint64_t watchdog_timeout = 2000000;

void watchdog_kick(void)
{
  watchdog_last_kick_ticks = process_get_ticks();
}

void watchdog_reset(void)
{
  watchdog_last_kick_ticks = process_get_ticks();
}

uint64_t watchdog_last_kick(void)
{
  return watchdog_last_kick_ticks;
}

uint64_t watchdog_timeout_ticks(void)
{
  return watchdog_timeout;
}

void watchdog_process(void *arg)
{
  (void)arg;
  watchdog_reset();

  for (;;)
  {
    uint64_t now = process_get_ticks();
    if ((now > watchdog_last_kick_ticks) && (now - watchdog_last_kick_ticks > watchdog_timeout))
    {
      panic("watchdog timeout");
    }
    process_yield();
  }
}
