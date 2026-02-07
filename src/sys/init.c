#include "sys/init.h"
#include "terminal/terminal.h"
#include "sys/log.h"
#include "drivers/vga.h"
#include "sys/services.h"

void init_run(void)
{
  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_writeln("Starting services:");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
  size_t count = services_count();
  for (size_t i = 0; i < count; ++i)
  {
    const char *name = services_name(i);
    terminal_write("  * ");
    terminal_write(name ? name : "(null)");
    terminal_write(" ... ");

    int status = name ? services_start(name) : -1;

    if (status == 0)
    {
      vga_set_color(0x0A, 0x00);
      terminal_writeln("OK");
    }
    else
    {
      log_error(name ? name : "service");
      vga_set_color(0x0C, 0x00);
      terminal_writeln("FAILED");
    }
    vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
  }
  terminal_writeln("");
}

size_t init_unit_count(void)
{
  return services_count();
}

const char *init_unit_name(size_t index)
{
  return services_name(index);
}
