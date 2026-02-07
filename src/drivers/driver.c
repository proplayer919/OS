#include "drivers/driver.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"

static driver_t kdrivers[] = {
    {"vga", vga_init},
    {"keyboard", keyboard_init},
};

void drivers_init(void)
{
  for (size_t i = 0; i < (sizeof(kdrivers) / sizeof(kdrivers[0])); ++i)
  {
    if (kdrivers[i].init)
    {
      (void)kdrivers[i].init();
    }
  }
}
