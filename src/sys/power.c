#include "sys/power.h"
#include "arch/io.h"

void power_reboot(void)
{
  outb(0x64, 0xFE);

  for (;;)
  {
    __asm__ __volatile__("hlt");
  }
}

void power_shutdown(void)
{
  outw(0x604, 0x2000);
  outw(0xB004, 0x2000);
  outw(0x4004, 0x3400);

  for (;;)
  {
    __asm__ __volatile__("hlt");
  }
}
