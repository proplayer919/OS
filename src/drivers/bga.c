#include "drivers/bga.h"
#include "arch/io.h"

#define BGA_INDEX 0x1CE
#define BGA_DATA 0x1CF

#define BGA_REG_ID 0x00
#define BGA_REG_XRES 0x01
#define BGA_REG_YRES 0x02
#define BGA_REG_BPP 0x03
#define BGA_REG_ENABLE 0x04
#define BGA_REG_VIRT_WIDTH 0x05
#define BGA_REG_VIRT_HEIGHT 0x06
#define BGA_REG_X_OFFSET 0x07
#define BGA_REG_Y_OFFSET 0x08

#define BGA_ENABLE 0x01
#define BGA_LFB_ENABLE 0x40
#define BGA_NOCLEAR 0x80

#define BGA_LFB_ADDR 0xE0000000u
#define BGA_DEFAULT_WIDTH 800
#define BGA_DEFAULT_HEIGHT 600
#define BGA_DEFAULT_BPP 32
#define BGA_ID_VBE 0xB0C5

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_VENDOR_BOCHS 0x1234
#define PCI_DEVICE_BOCHS_VGA 0x1111
#define PCI_VENDOR_VBOX 0x80EE
#define PCI_DEVICE_VBOX_VGA 0xBEEF

static uint32_t bga_lfb_base;

static uint16_t bga_width_value;
static uint16_t bga_height_value;
static uint16_t bga_pitch_value;
static uint16_t bga_bpp_value;
static uint8_t bga_active;

static void bga_write_reg(uint16_t index, uint16_t value)
{
  outw(BGA_INDEX, index);
  outw(BGA_DATA, value);
}

static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
  uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                     ((uint32_t)func << 8) | (offset & 0xFCu);
  outl(PCI_CONFIG_ADDRESS, address);
  return inl(PCI_CONFIG_DATA);
}

static uint32_t bga_detect_lfb_base(void)
{
  for (uint8_t slot = 0; slot < 32; ++slot)
  {
    uint32_t id = pci_read_dword(0, slot, 0, 0x00);
    uint16_t vendor = (uint16_t)(id & 0xFFFFu);
    uint16_t device = (uint16_t)((id >> 16) & 0xFFFFu);
    if ((vendor == PCI_VENDOR_BOCHS && device == PCI_DEVICE_BOCHS_VGA) ||
        (vendor == PCI_VENDOR_VBOX && device == PCI_DEVICE_VBOX_VGA))
    {
      uint32_t bar0 = pci_read_dword(0, slot, 0, 0x10);
      if ((bar0 & 0x01u) == 0)
      {
        return bar0 & 0xFFFFFFF0u;
      }
    }
  }
  return BGA_LFB_ADDR;
}

int bga_init(void)
{
  bga_width_value = BGA_DEFAULT_WIDTH;
  bga_height_value = BGA_DEFAULT_HEIGHT;
  bga_bpp_value = BGA_DEFAULT_BPP;
  bga_pitch_value = bga_width_value;
  bga_active = 0;
  bga_lfb_base = bga_detect_lfb_base();
  bga_write_reg(BGA_REG_ID, BGA_ID_VBE);
  return 0;
}

static void bga_set_mode(uint16_t width, uint16_t height, uint16_t bpp)
{
  bga_write_reg(BGA_REG_ENABLE, 0);
  bga_write_reg(BGA_REG_XRES, width);
  bga_write_reg(BGA_REG_YRES, height);
  bga_write_reg(BGA_REG_BPP, bpp);
  bga_write_reg(BGA_REG_VIRT_WIDTH, width);
  bga_write_reg(BGA_REG_VIRT_HEIGHT, height);
  bga_write_reg(BGA_REG_X_OFFSET, 0);
  bga_write_reg(BGA_REG_Y_OFFSET, 0);
  bga_write_reg(BGA_REG_ENABLE, BGA_ENABLE | BGA_LFB_ENABLE | BGA_NOCLEAR);
  bga_pitch_value = width;
}

void bga_enable(void)
{
  if (bga_active)
  {
    return;
  }
  bga_set_mode(bga_width_value, bga_height_value, bga_bpp_value);
  bga_active = 1;
}

void bga_disable(void)
{
  if (!bga_active)
  {
    return;
  }
  bga_write_reg(BGA_REG_ENABLE, 0);
  bga_active = 0;
}

int bga_is_active(void)
{
  return bga_active != 0;
}

uint32_t *bga_framebuffer(void)
{
  return (uint32_t *)bga_lfb_base;
}

uint16_t bga_width(void)
{
  return bga_width_value;
}

uint16_t bga_height(void)
{
  return bga_height_value;
}

uint16_t bga_pitch(void)
{
  return bga_pitch_value;
}
