#include "drivers/vga_gfx.h"
#include "arch/io.h"

#define VGA_MISC_WRITE 0x3C2
#define VGA_SEQ_INDEX 0x3C4
#define VGA_SEQ_DATA 0x3C5
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_GC_INDEX 0x3CE
#define VGA_GC_DATA 0x3CF
#define VGA_AC_INDEX 0x3C0
#define VGA_AC_WRITE 0x3C0
#define VGA_INSTAT_READ 0x3DA
#define VGA_PEL_MASK 0x3C6
#define VGA_DAC_WRITE 0x3C8
#define VGA_DAC_DATA 0x3C9

#define VGA_GFX_WIDTH 320
#define VGA_GFX_HEIGHT 200
#define VGA_GFX_PITCH 320

static uint8_t gfx_active;

static const uint8_t vga_mode_320x200x256[] = {
    0x63,
    0x03, 0x01, 0x0F, 0x00, 0x0E,
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C,
    0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
    0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00};

static const uint8_t vga_mode_text_80x25[] = {
    0x67,
    0x03, 0x00, 0x03, 0x00, 0x02,
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
    0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
    0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x0C, 0x00, 0x0F, 0x08, 0x00};

static void vga_write_regs(const uint8_t *regs)
{
  outb(VGA_MISC_WRITE, *regs++);

  for (uint8_t i = 0; i < 5; ++i)
  {
    outb(VGA_SEQ_INDEX, i);
    outb(VGA_SEQ_DATA, *regs++);
  }

  outb(VGA_CRTC_INDEX, 0x03);
  outb(VGA_CRTC_DATA, (uint8_t)(inb(VGA_CRTC_DATA) | 0x80));
  outb(VGA_CRTC_INDEX, 0x11);
  outb(VGA_CRTC_DATA, (uint8_t)(inb(VGA_CRTC_DATA) & ~0x80));

  for (uint8_t i = 0; i < 25; ++i)
  {
    outb(VGA_CRTC_INDEX, i);
    outb(VGA_CRTC_DATA, *regs++);
  }

  for (uint8_t i = 0; i < 9; ++i)
  {
    outb(VGA_GC_INDEX, i);
    outb(VGA_GC_DATA, *regs++);
  }

  for (uint8_t i = 0; i < 21; ++i)
  {
    (void)inb(VGA_INSTAT_READ);
    outb(VGA_AC_INDEX, i);
    outb(VGA_AC_WRITE, *regs++);
  }

  (void)inb(VGA_INSTAT_READ);
  outb(VGA_AC_INDEX, 0x20);
}

static void vga_gfx_set_palette_332(void)
{
  outb(VGA_DAC_WRITE, 0);
  for (uint16_t i = 0; i < 256; ++i)
  {
    uint8_t r = (uint8_t)(((i >> 5) & 0x07) * 9);
    uint8_t g = (uint8_t)(((i >> 2) & 0x07) * 9);
    uint8_t b = (uint8_t)((i & 0x03) * 21);
    outb(VGA_DAC_DATA, r);
    outb(VGA_DAC_DATA, g);
    outb(VGA_DAC_DATA, b);
  }
}

int vga_gfx_init(void)
{
  gfx_active = 0;
  return 0;
}

void vga_gfx_enter(void)
{
  if (gfx_active)
  {
    return;
  }
  vga_write_regs(vga_mode_320x200x256);
  outb(VGA_PEL_MASK, 0xFF);
  vga_gfx_set_palette_332();
  gfx_active = 1;
}

void vga_gfx_leave(void)
{
  if (!gfx_active)
  {
    return;
  }
  vga_write_regs(vga_mode_text_80x25);
  gfx_active = 0;
}

int vga_gfx_is_active(void)
{
  return gfx_active != 0;
}

uint8_t *vga_gfx_framebuffer(void)
{
  return (uint8_t *)0xA0000;
}

uint16_t vga_gfx_width(void)
{
  return VGA_GFX_WIDTH;
}

uint16_t vga_gfx_height(void)
{
  return VGA_GFX_HEIGHT;
}

uint16_t vga_gfx_pitch(void)
{
  return VGA_GFX_PITCH;
}
