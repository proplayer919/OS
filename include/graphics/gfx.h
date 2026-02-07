#ifndef GRAPHICS_GFX_H
#define GRAPHICS_GFX_H

#include <stdint.h>

typedef struct
{
  uint16_t width;
  uint16_t height;
  uint16_t pitch;
  uint32_t *buffer;
  uint8_t active;
} gfx_display_t;

typedef struct
{
  int16_t x;
  int16_t y;
  uint16_t width;
  uint16_t height;
} gfx_window_t;

typedef struct
{
  uint32_t color;
} gfx_gc_t;

gfx_display_t *gfx_open_display(void);
void gfx_close_display(gfx_display_t *display);

gfx_window_t gfx_create_simple_window(gfx_display_t *display, int16_t x, int16_t y,
                                      uint16_t width, uint16_t height);

gfx_gc_t gfx_create_gc(gfx_display_t *display, uint32_t color);
void gfx_set_foreground(gfx_display_t *display, gfx_gc_t *gc, uint32_t color);
void gfx_map_window(gfx_display_t *display, gfx_window_t *window);
void gfx_unmap_window(gfx_display_t *display, gfx_window_t *window);
void gfx_clear_window(gfx_display_t *display, gfx_window_t *window, uint32_t color);
void gfx_draw_point(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                    int16_t x, int16_t y);
void gfx_draw_line(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                   int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void gfx_draw_rect(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                   int16_t x, int16_t y, uint16_t width, uint16_t height);
void gfx_fill_rect(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                   int16_t x, int16_t y, uint16_t width, uint16_t height);
void gfx_flush(gfx_display_t *display);

#endif
