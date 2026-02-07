#ifndef PROC_PROCESS_H
#define PROC_PROCESS_H

#include <stddef.h>
#include <stdint.h>

void process_init(void);
int process_create(const char *name, void (*entry)(void *), void *arg, size_t stack_size);
void process_yield(void);
void process_run(void);
size_t process_count(void);
int process_is_used(size_t index);
const char *process_name(size_t index);
int process_is_active(size_t index);
int process_is_kill_requested(size_t index);
uint32_t process_pid(size_t index);
int process_kill(uint32_t pid, int force);
uint64_t process_get_ticks(void);

#endif
