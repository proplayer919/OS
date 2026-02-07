#ifndef VGA_GFX_H
#define VGA_GFX_H

#include <stdint.h>

int vga_gfx_init(void);
void vga_gfx_enter(void);
void vga_gfx_leave(void);
int vga_gfx_is_active(void);
uint8_t *vga_gfx_framebuffer(void);
uint16_t vga_gfx_width(void);
uint16_t vga_gfx_height(void);
uint16_t vga_gfx_pitch(void);

#endif
