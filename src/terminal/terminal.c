#include "terminal/terminal.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "proc/process.h"
#include "sys/watchdog.h"

static void terminal_puts(const char *str)
{
  while (*str)
  {
    vga_putc(*str++);
  }
}

static void terminal_redraw_line(const char *buffer, size_t len, size_t cursor,
                                 uint16_t start_row, uint16_t start_col, size_t prev_len)
{
  vga_set_cursor(start_row, start_col);
  for (size_t i = 0; i < prev_len; ++i)
  {
    vga_putc(' ');
  }

  vga_set_cursor(start_row, start_col);
  for (size_t i = 0; i < len; ++i)
  {
    vga_putc(buffer[i]);
  }

  uint16_t cursor_row = start_row;
  uint16_t cursor_col = (uint16_t)(start_col + cursor);
  vga_set_cursor(cursor_row, cursor_col);
}

static size_t terminal_str_copy(char *dst, size_t dst_len, const char *src)
{
  size_t i = 0;
  if (dst_len == 0)
  {
    return 0;
  }
  while (src && src[i] && i < dst_len - 1)
  {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
  return i;
}

void terminal_init(void)
{
  vga_set_color(0x0F, 0x00);
  vga_clear();
}

void terminal_clear(void)
{
  vga_clear();
}

void terminal_write(const char *str)
{
  terminal_puts(str);
}

void terminal_writeln(const char *str)
{
  terminal_puts(str);
  vga_putc('\n');
}

size_t terminal_readline(char *buffer, size_t max_len)
{
  size_t len = 0;
  size_t cursor = 0;
  size_t prev_len = 0;
  uint16_t start_row = 0;
  uint16_t start_col = 0;

  vga_get_cursor(&start_row, &start_col);

  for (;;)
  {
    int key = keyboard_poll_key();
    if (key == KEY_NONE)
    {
      process_yield();
      continue;
    }

    if (key == '\n' || key == '\r')
    {
      buffer[len] = '\0';
      vga_putc('\n');
      return len;
    }

    if (key == '\b')
    {
      if (cursor > 0)
      {
        for (size_t i = cursor - 1; i < len - 1; ++i)
        {
          buffer[i] = buffer[i + 1];
        }
        cursor--;
        len--;
        terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
        prev_len = len;
      }
      continue;
    }

    if (key == KEY_LEFT)
    {
      if (cursor > 0)
      {
        cursor--;
        terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
      }
      continue;
    }

    if (key == KEY_RIGHT)
    {
      if (cursor < len)
      {
        cursor++;
        terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
      }
      continue;
    }

    if (key < 32 || key >= 127)
    {
      continue;
    }

    if (len + 1 >= max_len)
    {
      continue;
    }

    for (size_t i = len; i > cursor; --i)
    {
      buffer[i] = buffer[i - 1];
    }
    buffer[cursor] = (char)key;
    cursor++;
    len++;

    terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
    prev_len = len;
  }
}

size_t terminal_readline_history(char *buffer, size_t max_len,
                                 const char **history, size_t history_len,
                                 size_t *history_pos,
                                 char *scratch, size_t scratch_len)
{
  size_t len = 0;
  size_t cursor = 0;
  size_t prev_len = 0;
  uint16_t start_row = 0;
  uint16_t start_col = 0;
  int using_history = 0;

  if (history_pos)
  {
    *history_pos = history_len;
  }

  vga_get_cursor(&start_row, &start_col);

  for (;;)
  {
    int key = keyboard_poll_key();
    if (key == KEY_NONE)
    {
      process_yield();
      continue;
    }

    if (key == '\n' || key == '\r')
    {
      buffer[len] = '\0';
      vga_putc('\n');
      if (history_pos)
      {
        *history_pos = history_len;
      }
      return len;
    }

    if (key == '\b')
    {
      if (cursor > 0)
      {
        for (size_t i = cursor - 1; i < len - 1; ++i)
        {
          buffer[i] = buffer[i + 1];
        }
        cursor--;
        len--;
        terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
        prev_len = len;
      }
      continue;
    }

    if (key == KEY_LEFT)
    {
      if (cursor > 0)
      {
        cursor--;
        terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
      }
      continue;
    }

    if (key == KEY_RIGHT)
    {
      if (cursor < len)
      {
        cursor++;
        terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
      }
      continue;
    }

    if (key == KEY_UP || key == KEY_DOWN)
    {
      if (!history || history_len == 0 || !history_pos)
      {
        continue;
      }

      if (!using_history)
      {
        terminal_str_copy(scratch, scratch_len, buffer);
        using_history = 1;
      }

      if (key == KEY_DOWN)
      {
        if (*history_pos > 0)
        {
          *history_pos -= 1;
        }
      }
      else
      {
        if (*history_pos < history_len)
        {
          *history_pos += 1;
        }
      }

      if (*history_pos >= history_len)
      {
        len = terminal_str_copy(buffer, max_len, scratch);
        using_history = 0;
      }
      else
      {
        len = terminal_str_copy(buffer, max_len, history[*history_pos]);
      }
      cursor = len;
      terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
      prev_len = len;
      continue;
    }

    if (key < 32 || key >= 127)
    {
      continue;
    }

    if (len + 1 >= max_len)
    {
      continue;
    }

    for (size_t i = len; i > cursor; --i)
    {
      buffer[i] = buffer[i - 1];
    }
    buffer[cursor] = (char)key;
    cursor++;
    len++;

    terminal_redraw_line(buffer, len, cursor, start_row, start_col, prev_len);
    prev_len = len;
  }
}
