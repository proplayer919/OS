#ifndef VGA_H
#define VGA_H

#include <stdint.h>

int vga_init(void);
void vga_clear(void);
void vga_write(const char *str, uint16_t row, uint16_t col);
void vga_putc(char ch);
void vga_set_color(uint8_t fg, uint8_t bg);
uint8_t vga_get_color(void);
void vga_set_cursor(uint16_t row, uint16_t col);
void vga_move_cursor(int16_t drow, int16_t dcol);
void vga_get_cursor(uint16_t *row, uint16_t *col);

#endif
