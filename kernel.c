#include "drivers/driver.h"
#include "sys/init.h"
#include "sys/log.h"
#include "sys/panic.h"
#include "sys/power.h"
#include "sys/services.h"
#include "terminal/terminal.h"
#include "shell/shell.h"
#include "mm/heap.h"
#include "proc/process.h"
#include "drivers/keyboard.h"

#define KERNEL_HEAP_SIZE (64u * 1024u)

static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static volatile int init_done;

typedef int (*kernel_module_fn)(void);

typedef struct
{
  const char *name;
  kernel_module_fn load;
  kernel_module_fn start;
} kernel_module_t;

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

void process_on_exit(const char *name)
{
  services_on_process_exit(name);
  if (name && str_eq(name, "init"))
  {
    init_done = 1;
  }
}

static void init_process(void *arg)
{
  (void)arg;
  init_run();
  terminal_writeln("Init complete. Starting shell...");
  shell_run();
  init_done = 1;
}

static int module_drivers_load(void)
{
  drivers_init();
  return 0;
}

static int module_services_load(void)
{
  services_init();
  return 0;
}

static int module_services_start(void)
{
  if (services_start("tty") != 0)
  {
    log_error("service:tty start failed");
    return -1;
  }
  if (services_start("watchdog") != 0)
  {
    log_error("service:watchdog start failed");
    return -1;
  }
  return 0;
}

static int module_init_start(void)
{
  if (process_create("init", init_process, 0, 4096) != 0)
  {
    log_error("process:init start failed");
    return -1;
  }
  return 0;
}

static int run_kernel_modules(const kernel_module_t *modules, size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    if (modules[i].load && modules[i].load() != 0)
    {
      log_error(modules[i].name ? modules[i].name : "module load failed");
      return -1;
    }
    if (modules[i].start && modules[i].start() != 0)
    {
      log_error(modules[i].name ? modules[i].name : "module start failed");
      return -1;
    }
  }
  return 0;
}

void kernel_main(void)
{
  heap_init(kernel_heap, KERNEL_HEAP_SIZE);
  process_init();
  log_init();

  init_done = 0;
  kernel_module_t modules[] = {
      {"drivers", module_drivers_load, 0},
      {"services", module_services_load, module_services_start},
      {"init", 0, module_init_start},
  };

  PANIC_IF(run_kernel_modules(modules, sizeof(modules) / sizeof(modules[0])) != 0,
           "kernel module failure");

  terminal_writeln("Booting OS...");

  while (!init_done)
  {
    process_yield();
  }

  terminal_writeln("Shell exited.");
  terminal_writeln("Press any key to shut down.");
  for (;;)
  {
    if (keyboard_poll_key() != 0)
    {
      power_shutdown();
    }
  }
}
