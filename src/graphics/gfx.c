#include "graphics/gfx.h"
#include "drivers/vga_gfx.h"

static gfx_display_t display0;

gfx_display_t *gfx_open_display(void)
{
  display0.width = vga_gfx_width();
  display0.height = vga_gfx_height();
  display0.pitch = vga_gfx_pitch();
  display0.buffer = vga_gfx_framebuffer();
  display0.active = (uint8_t)vga_gfx_is_active();
  return &display0;
}

void gfx_close_display(gfx_display_t *display)
{
  (void)display;
}

gfx_window_t gfx_create_simple_window(gfx_display_t *display, int16_t x, int16_t y,
                                      uint16_t width, uint16_t height)
{
  gfx_window_t win;
  (void)display;
  win.x = x;
  win.y = y;
  win.width = width;
  win.height = height;
  return win;
}

gfx_gc_t gfx_create_gc(gfx_display_t *display, uint8_t color)
{
  gfx_gc_t gc;
  (void)display;
  gc.color = color;
  return gc;
}

void gfx_set_foreground(gfx_display_t *display, gfx_gc_t *gc, uint8_t color)
{
  (void)display;
  if (gc)
  {
    gc->color = color;
  }
}

void gfx_map_window(gfx_display_t *display, gfx_window_t *window)
{
  (void)window;
  vga_gfx_enter();
  if (display)
  {
    display->active = 1;
    display->buffer = vga_gfx_framebuffer();
  }
}

void gfx_unmap_window(gfx_display_t *display, gfx_window_t *window)
{
  (void)window;
  vga_gfx_leave();
  if (display)
  {
    display->active = 0;
  }
}

static void gfx_put_pixel(gfx_display_t *display, int16_t x, int16_t y, uint8_t color)
{
  if (!display || !display->buffer)
  {
    return;
  }
  if (x < 0 || y < 0)
  {
    return;
  }
  if ((uint16_t)x >= display->width || (uint16_t)y >= display->height)
  {
    return;
  }
  display->buffer[(uint16_t)y * display->pitch + (uint16_t)x] = color;
}

void gfx_clear_window(gfx_display_t *display, gfx_window_t *window, uint8_t color)
{
  if (!display || !display->buffer || !window)
  {
    return;
  }

  for (uint16_t row = 0; row < window->height; ++row)
  {
    uint16_t y = (uint16_t)(window->y + row);
    if (y >= display->height)
    {
      break;
    }
    uint16_t base = (uint16_t)(y * display->pitch);
    uint16_t xstart = (uint16_t)window->x;
    uint16_t xend = (uint16_t)(window->x + window->width);
    if (xstart >= display->width)
    {
      continue;
    }
    if (xend > display->width)
    {
      xend = display->width;
    }
    for (uint16_t x = xstart; x < xend; ++x)
    {
      display->buffer[base + x] = color;
    }
  }
}

void gfx_draw_point(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                    int16_t x, int16_t y)
{
  if (!window || !gc)
  {
    return;
  }
  gfx_put_pixel(display, (int16_t)(window->x + x), (int16_t)(window->y + y), gc->color);
}

void gfx_draw_line(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                   int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  if (!window || !gc)
  {
    return;
  }

  int16_t dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t dy = (int16_t)((y1 > y0) ? (y0 - y1) : (y1 - y0));
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t err = (int16_t)(dx + dy);

  for (;;)
  {
    gfx_draw_point(display, window, gc, x0, y0);
    if (x0 == x1 && y0 == y1)
    {
      break;
    }
    int16_t e2 = (int16_t)(2 * err);
    if (e2 >= dy)
    {
      err = (int16_t)(err + dy);
      x0 = (int16_t)(x0 + sx);
    }
    if (e2 <= dx)
    {
      err = (int16_t)(err + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }
}

void gfx_draw_rect(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                   int16_t x, int16_t y, uint16_t width, uint16_t height)
{
  if (!window || !gc || width == 0 || height == 0)
  {
    return;
  }
  gfx_draw_line(display, window, gc, x, y, (int16_t)(x + width - 1), y);
  gfx_draw_line(display, window, gc, x, (int16_t)(y + height - 1),
                (int16_t)(x + width - 1), (int16_t)(y + height - 1));
  gfx_draw_line(display, window, gc, x, y, x, (int16_t)(y + height - 1));
  gfx_draw_line(display, window, gc, (int16_t)(x + width - 1), y,
                (int16_t)(x + width - 1), (int16_t)(y + height - 1));
}

void gfx_fill_rect(gfx_display_t *display, gfx_window_t *window, gfx_gc_t *gc,
                   int16_t x, int16_t y, uint16_t width, uint16_t height)
{
  if (!window || !gc || width == 0 || height == 0)
  {
    return;
  }

  for (uint16_t row = 0; row < height; ++row)
  {
    for (uint16_t col = 0; col < width; ++col)
    {
      gfx_draw_point(display, window, gc, (int16_t)(x + col), (int16_t)(y + row));
    }
  }
}

void gfx_flush(gfx_display_t *display)
{
  (void)display;
}
