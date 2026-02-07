#include "drivers/keyboard.h"
#include "arch/io.h"

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_CMD_PORT 0x64

static const char kbd_us_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0};

static const char kbd_us_shift_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '{', '}', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':',
    '"', '~', 0, '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0};

static uint8_t kbd_shift;
static uint8_t kbd_caps;
static uint8_t kbd_e0;
static uint8_t kbd_ctrl;

int keyboard_init(void)
{
  outb(KBD_CMD_PORT, 0xAE);
  return 0;
}

int keyboard_has_data(void)
{
  return (inb(KBD_STATUS_PORT) & 0x01) != 0;
}

uint8_t keyboard_read_scancode(void)
{
  return inb(KBD_DATA_PORT);
}

int keyboard_poll_key(void)
{
  if (!keyboard_has_data())
  {
    return 0;
  }

  uint8_t scancode = keyboard_read_scancode();
  if (scancode == 0xE0)
  {
    kbd_e0 = 1;
    return 0;
  }

  if (kbd_e0)
  {
    kbd_e0 = 0;
    if (scancode & 0x80)
    {
      if ((scancode & 0x7F) == 0x1D)
      {
        kbd_ctrl = 0;
      }
      return 0;
    }

    switch (scancode)
    {
    case 0x1D:
      kbd_ctrl = 1;
      return 0;
    case 0x48:
      return KEY_UP;
    case 0x50:
      return KEY_DOWN;
    case 0x4B:
      return KEY_LEFT;
    case 0x4D:
      return KEY_RIGHT;
    default:
      return 0;
    }
  }

  if (scancode & 0x80)
  {
    uint8_t code = scancode & 0x7F;
    if (code == 0x1D)
    {
      kbd_ctrl = 0;
    }
    if (code == 0x2A || code == 0x36)
    {
      kbd_shift = 0;
    }
    kbd_e0 = 0;
    return 0;
  }

  uint8_t code = scancode & 0x7F;
  if (code == 0x2A || code == 0x36)
  {
    kbd_shift = 1;
    return 0;
  }
  if (code == 0x1D)
  {
    kbd_ctrl = 1;
    return 0;
  }
  if (code == 0x3A)
  {
    kbd_caps = (uint8_t)(kbd_caps ^ 1);
    return 0;
  }

  kbd_e0 = 0;

  char ch = kbd_us_map[code];
  if (ch == 0)
  {
    return 0;
  }

  if (ch >= 'a' && ch <= 'z')
  {
    if ((kbd_caps ^ kbd_shift) != 0)
    {
      ch = (char)(ch - 'a' + 'A');
    }
    if (kbd_ctrl && (ch == 'c' || ch == 'C'))
    {
      return 3;
    }
    return (int)ch;
  }

  if (kbd_shift)
  {
    char shifted = kbd_us_shift_map[code];
    if (shifted != 0)
    {
      if (kbd_ctrl && (shifted == 'C'))
      {
        return 3;
      }
      return (int)shifted;
    }
  }

  return (int)ch;
}
