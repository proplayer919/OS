#include "drivers/vga.h"
#include "arch/io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_PORT_CMD 0x3D4
#define VGA_PORT_DATA 0x3D5
#define VGA_TAB_WIDTH 4

static volatile uint16_t *const vga_buffer = (uint16_t *)0xB8000;
static uint8_t vga_color = 0x0F;
static uint16_t cursor_row;
static uint16_t cursor_col;

static void vga_update_cursor(void)
{
  uint16_t pos = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);
  outb(VGA_PORT_CMD, 0x0F);
  outb(VGA_PORT_DATA, (uint8_t)(pos & 0xFF));
  outb(VGA_PORT_CMD, 0x0E);
  outb(VGA_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_scroll_if_needed(void)
{
  if (cursor_row < VGA_HEIGHT)
  {
    return;
  }

  for (uint32_t row = 1; row < VGA_HEIGHT; ++row)
  {
    for (uint32_t col = 0; col < VGA_WIDTH; ++col)
    {
      vga_buffer[(row - 1) * VGA_WIDTH + col] =
          vga_buffer[row * VGA_WIDTH + col];
    }
  }

  for (uint32_t col = 0; col < VGA_WIDTH; ++col)
  {
    vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
        ((uint16_t)vga_color << 8) | ' ';
  }

  cursor_row = VGA_HEIGHT - 1;
}

static void vga_clamp_cursor(void)
{
  if (cursor_row >= VGA_HEIGHT)
  {
    cursor_row = (uint16_t)(VGA_HEIGHT - 1);
  }
  if (cursor_col >= VGA_WIDTH)
  {
    cursor_col = (uint16_t)(VGA_WIDTH - 1);
  }
}

int vga_init(void)
{
  vga_clear();
  return 0;
}

void vga_clear(void)
{
  for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
  {
    vga_buffer[i] = ((uint16_t)vga_color << 8) | ' ';
  }
  cursor_row = 0;
  cursor_col = 0;
  vga_update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
  vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

uint8_t vga_get_color(void)
{
  return vga_color;
}

void vga_write(const char *str, uint16_t row, uint16_t col)
{
  uint16_t offset = (uint16_t)(row * VGA_WIDTH + col);
  uint16_t i = 0;
  for (; str[i] != '\0'; ++i)
  {
    vga_buffer[offset + i] = ((uint16_t)vga_color << 8) | (uint8_t)str[i];
  }
  cursor_row = row;
  cursor_col = (uint16_t)(col + i);
  if (cursor_col >= VGA_WIDTH)
  {
    cursor_col = (uint16_t)(cursor_col % VGA_WIDTH);
    cursor_row = (uint16_t)(cursor_row + 1);
  }
  vga_scroll_if_needed();
  vga_update_cursor();
}

void vga_set_cursor(uint16_t row, uint16_t col)
{
  cursor_row = row;
  cursor_col = col;
  vga_clamp_cursor();
  vga_update_cursor();
}

void vga_get_cursor(uint16_t *row, uint16_t *col)
{
  if (row)
  {
    *row = cursor_row;
  }
  if (col)
  {
    *col = cursor_col;
  }
}

void vga_move_cursor(int16_t drow, int16_t dcol)
{
  int32_t new_row = (int32_t)cursor_row + drow;
  int32_t new_col = (int32_t)cursor_col + dcol;

  if (new_row < 0)
  {
    new_row = 0;
  }
  if (new_row >= VGA_HEIGHT)
  {
    new_row = VGA_HEIGHT - 1;
  }
  if (new_col < 0)
  {
    new_col = 0;
  }
  if (new_col >= VGA_WIDTH)
  {
    new_col = VGA_WIDTH - 1;
  }

  cursor_row = (uint16_t)new_row;
  cursor_col = (uint16_t)new_col;
  vga_update_cursor();
}

void vga_putc(char ch)
{
  if (ch == '\n')
  {
    cursor_col = 0;
    cursor_row = (uint16_t)(cursor_row + 1);
    vga_scroll_if_needed();
    vga_update_cursor();
    return;
  }

  if (ch == '\t')
  {
    uint16_t next = (uint16_t)((cursor_col + VGA_TAB_WIDTH) & ~(VGA_TAB_WIDTH - 1));
    if (next >= VGA_WIDTH)
    {
      cursor_col = 0;
      cursor_row = (uint16_t)(cursor_row + 1);
      vga_scroll_if_needed();
    }
    else
    {
      cursor_col = next;
    }
    vga_update_cursor();
    return;
  }

  if (ch == '\r')
  {
    cursor_col = 0;
    vga_update_cursor();
    return;
  }

  if (ch == '\b')
  {
    if (cursor_col > 0)
    {
      cursor_col--;
      vga_buffer[cursor_row * VGA_WIDTH + cursor_col] =
          ((uint16_t)vga_color << 8) | ' ';
    }
    vga_update_cursor();
    return;
  }

  vga_buffer[cursor_row * VGA_WIDTH + cursor_col] =
      ((uint16_t)vga_color << 8) | (uint8_t)ch;
  cursor_col++;
  if (cursor_col >= VGA_WIDTH)
  {
    cursor_col = 0;
    cursor_row = (uint16_t)(cursor_row + 1);
    vga_scroll_if_needed();
  }
  vga_update_cursor();
}
