#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

int keyboard_init(void);
int keyboard_has_data(void);
uint8_t keyboard_read_scancode(void);

typedef enum {
	KEY_NONE = 0,
	KEY_UP = 0x80,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
} keyboard_key_t;

int keyboard_poll_key(void);

#endif
