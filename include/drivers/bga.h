#ifndef DRIVERS_BGA_H
#define DRIVERS_BGA_H

#include <stdint.h>

int bga_init(void);
void bga_enable(void);
void bga_disable(void);
int bga_is_active(void);
uint32_t *bga_framebuffer(void);
uint16_t bga_width(void);
uint16_t bga_height(void);
uint16_t bga_pitch(void);

#endif
