#ifndef SYS_SERVICES_H
#define SYS_SERVICES_H

#include <stddef.h>

void services_init(void);
size_t services_count(void);
const char *services_name(size_t index);
int services_is_running(size_t index);
const char **services_deps(size_t index, size_t *count_out);
int services_autorestart(size_t index);
int services_restart_limit(size_t index);
int services_restart_attempts(size_t index);
int services_start(const char *name);
int services_stop(const char *name);
int services_restart(const char *name);
int services_find(const char *name, size_t *index_out);
void services_on_process_exit(const char *name);

#endif
