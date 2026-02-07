#ifndef SYS_LOG_H
#define SYS_LOG_H

#include <stdint.h>

typedef enum {
  LOG_INFO = 0,
  LOG_WARN = 1,
  LOG_ERROR = 2
} log_level_t;

typedef struct {
  uint64_t seq;
  uint64_t tick;
  uint8_t level;
  char msg[64];
} log_entry_t;

void log_init(void);
void log_write(log_level_t level, const char *msg);
void log_info(const char *msg);
void log_warn(const char *msg);
void log_error(const char *msg);

uint64_t log_oldest_seq(void);
uint64_t log_latest_seq(void);
int log_read(uint64_t seq, log_entry_t *out);

#endif
