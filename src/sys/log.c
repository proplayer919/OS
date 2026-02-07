#include "sys/log.h"
#include "proc/process.h"

#define LOG_CAPACITY 128

static log_entry_t log_buf[LOG_CAPACITY];
static uint64_t log_seq;

static void log_copy_msg(char *dst, const char *src)
{
  size_t i = 0;
  for (; src[i] != '\0' && i < sizeof(log_buf[0].msg) - 1; ++i)
  {
    dst[i] = src[i];
  }
  dst[i] = '\0';
}

void log_init(void)
{
  log_seq = 0;
}

void log_write(log_level_t level, const char *msg)
{
  log_entry_t *entry = &log_buf[log_seq % LOG_CAPACITY];
  entry->seq = log_seq;
  entry->tick = process_get_ticks();
  entry->level = (uint8_t)level;
  log_copy_msg(entry->msg, msg ? msg : "");
  log_seq++;
}

void log_info(const char *msg)
{
  log_write(LOG_INFO, msg);
}

void log_warn(const char *msg)
{
  log_write(LOG_WARN, msg);
}

void log_error(const char *msg)
{
  log_write(LOG_ERROR, msg);
}

uint64_t log_latest_seq(void)
{
  return log_seq;
}

uint64_t log_oldest_seq(void)
{
  if (log_seq < LOG_CAPACITY)
  {
    return 0;
  }
  return log_seq - LOG_CAPACITY;
}

int log_read(uint64_t seq, log_entry_t *out)
{
  if (!out)
  {
    return 0;
  }

  if (seq >= log_seq)
  {
    return 0;
  }

  uint64_t oldest = log_oldest_seq();
  if (seq < oldest)
  {
    return 0;
  }

  *out = log_buf[seq % LOG_CAPACITY];
  return 1;
}
