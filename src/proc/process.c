#include "proc/process.h"
#include "mm/heap.h"
#include "sys/watchdog.h"

#define MAX_PROCESSES 8

typedef struct
{
  const char *name;
  void (*entry)(void *);
  void *arg;
  uint32_t *sp;
  uint8_t *stack_base;
  size_t stack_size;
  uint32_t pid;
  int used;
  int kill_requested;
  int reap;
  int active;
} process_t;

extern void switch_context(uint32_t **old_sp, uint32_t *new_sp);

static process_t processes[MAX_PROCESSES];
static size_t process_total;
static size_t current_process;
static volatile uint64_t process_ticks;

__attribute__((weak)) void process_on_exit(const char *name)
{
  (void)name;
}

static int pid_in_use(uint32_t pid)
{
  if (pid == 0)
  {
    return 1;
  }
  for (size_t i = 0; i < process_total; ++i)
  {
    if (processes[i].used && processes[i].pid == pid)
    {
      return 1;
    }
  }
  return 0;
}

static uint32_t alloc_pid(void)
{
  uint32_t pid = 1;
  while (pid_in_use(pid))
  {
    pid++;
  }
  return pid;
}

static size_t find_free_slot(void)
{
  for (size_t i = 1; i < process_total; ++i)
  {
    if (!processes[i].used)
    {
      return i;
    }
  }
  return process_total;
}

static void process_cleanup(size_t index)
{
  if (processes[index].stack_base)
  {
    kfree(processes[index].stack_base);
  }
  processes[index].stack_base = 0;
  processes[index].stack_size = 0;
  processes[index].used = 0;
  processes[index].active = 0;
  processes[index].kill_requested = 0;
  processes[index].reap = 0;
  processes[index].pid = 0;
  processes[index].name = 0;
  processes[index].entry = 0;
  processes[index].arg = 0;
  processes[index].sp = 0;
}

static void reap_zombies(void)
{
  for (size_t i = 1; i < process_total; ++i)
  {
    if (i == current_process)
    {
      continue;
    }
    if (processes[i].reap)
    {
      const char *name = processes[i].name;
      process_cleanup(i);
      process_on_exit(name);
    }
  }
}

static void process_trampoline(void)
{
  process_t *process = &processes[current_process];
  if (process->entry)
  {
    process->entry(process->arg);
  }
  processes[current_process].kill_requested = 1;
  process_yield();
  for (;;)
  {
  }
}

void process_init(void)
{
  process_total = 1;
  current_process = 0;
  process_ticks = 0;
  processes[0].name = "kernel";
  processes[0].entry = 0;
  processes[0].arg = 0;
  processes[0].sp = 0;
  processes[0].stack_base = 0;
  processes[0].stack_size = 0;
  processes[0].pid = 1;
  processes[0].used = 1;
  processes[0].kill_requested = 0;
  processes[0].reap = 0;
  processes[0].active = 1;
}

int process_create(const char *name, void (*entry)(void *), void *arg, size_t stack_size)
{
  if (!entry || stack_size < 256)
  {
    return -1;
  }

  size_t slot = find_free_slot();
  if (slot >= MAX_PROCESSES)
  {
    return -1;
  }

  uint8_t *stack = (uint8_t *)kmalloc(stack_size);
  if (!stack)
  {
    return -1;
  }

  uintptr_t top = (uintptr_t)(stack + stack_size);
  top &= ~((uintptr_t)0x0F);
  uint32_t *sp = (uint32_t *)top;
  *--sp = (uint32_t)process_trampoline; /* return address */
  *--sp = 0x002;                        /* EFLAGS (IF=0, bit1 set) */
  *--sp = 0;                            /* EAX */
  *--sp = 0;                            /* ECX */
  *--sp = 0;                            /* EDX */
  *--sp = 0;                            /* EBX */
  *--sp = 0;                            /* ESP (ignored by popa) */
  *--sp = 0;                            /* EBP */
  *--sp = 0;                            /* ESI */
  *--sp = 0;                            /* EDI */

  processes[slot].name = name;
  processes[slot].entry = entry;
  processes[slot].arg = arg;
  processes[slot].sp = sp;
  processes[slot].stack_base = stack;
  processes[slot].stack_size = stack_size;
  processes[slot].pid = alloc_pid();
  processes[slot].used = 1;
  processes[slot].kill_requested = 0;
  processes[slot].reap = 0;
  processes[slot].active = 1;

  if (slot == process_total)
  {
    process_total++;
  }
  return 0;
}

void process_yield(void)
{
  process_ticks++;
  watchdog_kick();
  if (process_total <= 1)
  {
    return;
  }

  if (current_process != 0 && processes[current_process].kill_requested)
  {
    processes[current_process].active = 0;
    processes[current_process].reap = 1;
  }

  size_t next = current_process;
  for (size_t i = 0; i < process_total; ++i)
  {
    next = (next + 1) % process_total;
    if (processes[next].active)
    {
      break;
    }
  }

  if (next == current_process || !processes[next].active)
  {
    reap_zombies();
    return;
  }

  size_t prev = current_process;
  current_process = next;
  switch_context(&processes[prev].sp, processes[next].sp);

  if (prev != 0 && processes[prev].reap)
  {
    const char *name = processes[prev].name;
    process_cleanup(prev);
    process_on_exit(name);
  }

  reap_zombies();
}

void process_run(void)
{
  for (;;)
  {
    process_yield();
  }
}

size_t process_count(void)
{
  return process_total;
}

int process_is_used(size_t index)
{
  if (index >= process_total)
  {
    return 0;
  }
  return processes[index].used;
}

const char *process_name(size_t index)
{
  if (index >= process_total)
  {
    return 0;
  }
  return processes[index].name;
}

int process_is_active(size_t index)
{
  if (index >= process_total)
  {
    return 0;
  }
  return processes[index].active;
}

int process_is_kill_requested(size_t index)
{
  if (index >= process_total)
  {
    return 0;
  }
  return processes[index].kill_requested;
}

uint32_t process_pid(size_t index)
{
  if (index >= process_total)
  {
    return 0;
  }
  return processes[index].pid;
}

int process_kill(uint32_t pid, int force)
{
  if (pid == 1 && !force)
  {
    return -1;
  }

  for (size_t i = 0; i < process_total; ++i)
  {
    if (processes[i].used && processes[i].pid == pid)
    {
      if (force && i != current_process)
      {
        const char *name = processes[i].name;
        process_cleanup(i);
        process_on_exit(name);
        return 0;
      }

      processes[i].kill_requested = 1;
      return 0;
    }
  }

  return -1;
}

uint64_t process_get_ticks(void)
{
  return process_ticks;
}
