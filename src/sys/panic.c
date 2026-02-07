#include "sys/panic.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "sys/power.h"
#include "sys/log.h"

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static char log_level_char(uint8_t level)
{
  switch (level)
  {
  case LOG_WARN:
    return 'W';
  case LOG_ERROR:
    return 'E';
  default:
    return 'I';
  }
}

static uint64_t u64_div_u32(uint64_t value, uint32_t divisor, uint32_t *remainder)
{
  uint64_t quotient = 0;
  uint64_t rem = 0;

  for (int i = 63; i >= 0; --i)
  {
    rem = (rem << 1) | ((value >> i) & 1u);
    if (rem >= divisor)
    {
      rem -= divisor;
      quotient |= (1ull << i);
    }
  }

  if (remainder)
  {
    *remainder = (uint32_t)rem;
  }
  return quotient;
}

static void u64_to_str(uint64_t value, char *buf, size_t len)
{
  if (!buf || len == 0)
  {
    return;
  }

  size_t i = 0;
  if (value == 0)
  {
    buf[i++] = '0';
    buf[i] = '\0';
    return;
  }

  while (value > 0 && i < len - 1)
  {
    uint32_t rem = 0;
    value = u64_div_u32(value, 10, &rem);
    buf[i++] = (char)('0' + rem);
  }

  for (size_t j = 0; j < i / 2; ++j)
  {
    char tmp = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j] = tmp;
  }

  buf[i] = '\0';
}

static size_t str_len(const char *text)
{
  size_t len = 0;
  if (!text)
  {
    return 0;
  }
  while (text[len] != '\0')
  {
    len++;
  }
  return len;
}

static void write_center(uint16_t row, const char *text)
{
  size_t len = str_len(text);
  uint16_t col = 0;
  if (len < VGA_WIDTH)
  {
    col = (uint16_t)((VGA_WIDTH - len) / 2);
  }
  vga_write(text, row, col);
}

static void write_hline(uint16_t row, char ch)
{
  char line[VGA_WIDTH + 1];
  for (size_t i = 0; i < VGA_WIDTH; ++i)
  {
    line[i] = ch;
  }
  line[VGA_WIDTH] = '\0';
  vga_write(line, row, 0);
}

static void write_kv_line(uint16_t row, const char *label, uint64_t value)
{
  char num[21];
  u64_to_str(value, num, sizeof(num));
  vga_write(label, row, 0);
  vga_write(num, row, 12);
}

void panic(const char *message)
{
  vga_set_color(0x0F, 0x04);
  vga_clear();
  write_hline(0, '=');
  vga_set_color(0x0E, 0x04);
  write_center(1, "KERNEL PANIC");
  vga_set_color(0x0F, 0x04);
  write_center(2, "System halted due to a fatal error");
  write_hline(3, '=');

  uint16_t row = 5;
  if (message && message[0])
  {
    vga_set_color(0x0B, 0x04);
    vga_write("Reason:", row++, 2);
    vga_set_color(0x0F, 0x04);
    vga_write(message, row++, 4);
  }

  uint64_t latest = log_latest_seq();
  if (latest > 0)
  {
    log_entry_t entry;
    uint64_t last_seq = latest - 1;
    if (log_read(last_seq, &entry))
    {
      vga_set_color(0x0B, 0x04);
      write_kv_line(row++, "Last tick:", entry.tick);
      vga_set_color(0x0F, 0x04);
    }
  }

  row++;
  vga_set_color(0x0B, 0x04);
  vga_write("Recent logs:", row++, 2);
  vga_set_color(0x0F, 0x04);

  uint64_t count = 5;
  uint64_t start = 0;
  if (latest > count)
  {
    start = latest - count;
  }

  for (uint64_t seq = start; seq < latest && row < 22; ++seq)
  {
    log_entry_t entry;
    if (!log_read(seq, &entry))
    {
      continue;
    }
    char line[80];
    size_t idx = 0;
    line[idx++] = '[';
    line[idx++] = log_level_char(entry.level);
    line[idx++] = ']';
    line[idx++] = ' ';
    for (size_t i = 0; entry.msg[i] != '\0' && idx < sizeof(line) - 1; ++i)
    {
      line[idx++] = entry.msg[i];
    }
    line[idx] = '\0';
    switch (entry.level)
    {
    case LOG_WARN:
      vga_set_color(0x0E, 0x04);
      break;
    case LOG_ERROR:
      vga_set_color(0x0C, 0x04);
      break;
    default:
      vga_set_color(0x07, 0x04);
      break;
    }
    vga_write(line, row++, 0);
    vga_set_color(0x0F, 0x04);
  }

  if (row < (VGA_HEIGHT - 2))
  {
    row = (uint16_t)(VGA_HEIGHT - 2);
  }
  write_hline((uint16_t)(VGA_HEIGHT - 1), '=');
  vga_set_color(0x0E, 0x04);
  write_center(row, "Press any key to reboot");
  vga_set_color(0x0F, 0x04);

  for (;;)
  {
    if (keyboard_poll_key() != 0)
    {
      power_reboot();
    }
  }
}
