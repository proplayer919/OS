#include "sys/services.h"
#include "proc/process.h"
#include "sys/watchdog.h"
#include "terminal/terminal.h"
#include "sys/log.h"

typedef struct
{
  const char *name;
  int (*start)(void);
  int (*stop)(void);
  const char **deps;
  size_t dep_count;
  int autorestart;
  int starting;
  int stop_requested;
  int restart_attempts;
  int restart_limit;
  int running;
} service_t;

static int str_eq(const char *a, const char *b)
{
  while (*a && *b)
  {
    if (*a != *b)
    {
      return 0;
    }
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static void service_log_event(const char *name, const char *event)
{
  char buf[64];
  size_t i = 0;
  const char *prefix = "service:";

  while (*prefix && i < sizeof(buf) - 1)
  {
    buf[i++] = *prefix++;
  }
  while (*name && i < sizeof(buf) - 1)
  {
    buf[i++] = *name++;
  }
  if (i < sizeof(buf) - 1)
  {
    buf[i++] = ' ';
  }
  while (*event && i < sizeof(buf) - 1)
  {
    buf[i++] = *event++;
  }
  buf[i] = '\0';
  log_info(buf);
}

static uint32_t find_process_pid(const char *name, size_t *index_out)
{
  size_t count = process_count();
  for (size_t i = 0; i < count; ++i)
  {
    if (!process_is_used(i))
    {
      continue;
    }
    const char *proc_name = process_name(i);
    if (proc_name && str_eq(proc_name, name))
    {
      if (index_out)
      {
        *index_out = i;
      }
      return process_pid(i);
    }
  }
  return 0;
}

static int service_tty_start(void)
{
  log_info("service:tty start");
  terminal_init();
  return 0;
}

static int service_tty_stop(void)
{
  return 0;
}

static int service_watchdog_start(void)
{
  if (process_create("watchdog", watchdog_process, 0, 2048) != 0)
  {
    return -1;
  }
  watchdog_reset();
  return 0;
}

static int service_watchdog_stop(void)
{
  size_t idx = 0;
  uint32_t pid = find_process_pid("watchdog", &idx);
  if (pid)
  {
    (void)process_kill(pid, 1);
  }
  return 0;
}

static const char *watchdog_deps[] = {"tty"};

static service_t kservices[] = {
    {"tty", service_tty_start, service_tty_stop, 0, 0, 0, 0, 0, 0, 0, 0},
    {"watchdog", service_watchdog_start, service_watchdog_stop,
     watchdog_deps, 1, 1, 0, 0, 0, 3, 0},
};

void services_init(void)
{
  for (size_t i = 0; i < (sizeof(kservices) / sizeof(kservices[0])); ++i)
  {
    kservices[i].running = 0;
    kservices[i].starting = 0;
    kservices[i].stop_requested = 0;
    kservices[i].restart_attempts = 0;
  }
}

size_t services_count(void)
{
  return (sizeof(kservices) / sizeof(kservices[0]));
}

const char *services_name(size_t index)
{
  if (index >= services_count())
  {
    return 0;
  }
  return kservices[index].name;
}

int services_is_running(size_t index)
{
  if (index >= services_count())
  {
    return 0;
  }
  return kservices[index].running;
}

const char **services_deps(size_t index, size_t *count_out)
{
  if (count_out)
  {
    *count_out = 0;
  }
  if (index >= services_count())
  {
    return 0;
  }
  if (count_out)
  {
    *count_out = kservices[index].dep_count;
  }
  return kservices[index].deps;
}

int services_autorestart(size_t index)
{
  if (index >= services_count())
  {
    return 0;
  }
  return kservices[index].autorestart;
}

int services_restart_limit(size_t index)
{
  if (index >= services_count())
  {
    return 0;
  }
  return kservices[index].restart_limit;
}

int services_restart_attempts(size_t index)
{
  if (index >= services_count())
  {
    return 0;
  }
  return kservices[index].restart_attempts;
}

int services_find(const char *name, size_t *index_out)
{
  for (size_t i = 0; i < services_count(); ++i)
  {
    if (kservices[i].name && name && str_eq(kservices[i].name, name))
    {
      if (index_out)
      {
        *index_out = i;
      }
      return 0;
    }
  }
  return -1;
}

int services_start(const char *name)
{
  size_t index = 0;
  if (services_find(name, &index) != 0)
  {
    log_error("service:start unknown");
    return -1;
  }
  if (kservices[index].starting)
  {
    return 0;
  }
  if (kservices[index].running)
  {
    return 0;
  }
  kservices[index].starting = 1;
  kservices[index].stop_requested = 0;
  for (size_t i = 0; i < kservices[index].dep_count; ++i)
  {
    const char *dep = kservices[index].deps[i];
    if (dep && services_start(dep) != 0)
    {
      kservices[index].starting = 0;
      log_error("service:dep start failed");
      return -1;
    }
  }
  if (kservices[index].start && kservices[index].start() != 0)
  {
    log_error("service:start failed");
    kservices[index].starting = 0;
    return -1;
  }
  kservices[index].running = 1;
  kservices[index].starting = 0;
  kservices[index].restart_attempts = 0;
  service_log_event(name, "started");
  return 0;
}

int services_stop(const char *name)
{
  size_t index = 0;
  if (services_find(name, &index) != 0)
  {
    return -1;
  }
  kservices[index].stop_requested = 1;
  if (!kservices[index].running)
  {
    return 0;
  }
  if (kservices[index].stop && kservices[index].stop() != 0)
  {
    return -1;
  }
  kservices[index].running = 0;
  service_log_event(name, "stopped");
  return 0;
}

int services_restart(const char *name)
{
  if (services_stop(name) != 0)
  {
    return -1;
  }
  if (services_start(name) != 0)
  {
    return -1;
  }
  service_log_event(name, "restarted");
  return 0;
}

void services_on_process_exit(const char *name)
{
  if (!name)
  {
    return;
  }
  for (size_t i = 0; i < services_count(); ++i)
  {
    if (kservices[i].name && str_eq(kservices[i].name, name))
    {
      kservices[i].running = 0;
      service_log_event(name, "exited");
      if (kservices[i].autorestart && !kservices[i].stop_requested)
      {
        if (kservices[i].restart_attempts < kservices[i].restart_limit)
        {
          kservices[i].restart_attempts++;
          service_log_event(name, "restarting");
          (void)services_start(name);
        }
        else
        {
          service_log_event(name, "restart-limit");
        }
      }
      return;
    }
  }
}
