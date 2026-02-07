#include "shell/shell.h"
#include "terminal/terminal.h"
#include "drivers/vga.h"
#include "proc/process.h"
#include "sys/panic.h"
#include "sys/log.h"
#include "sys/services.h"
#include "drivers/keyboard.h"
#include "sys/power.h"
#include "sys/watchdog.h"
#include "mm/heap.h"
#include "graphics/gfx.h"

#define SHELL_LINE_MAX 64
#define SHELL_HISTORY_MAX 8

static char shell_history[SHELL_HISTORY_MAX][SHELL_LINE_MAX];
static size_t shell_history_len;
static size_t shell_history_pos;

static int str_eq(const char *a, const char *b)
{
  while (*a && *b)
  {
    if (*a != *b)
    {
      return 0;
    }
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static int str_starts_with(const char *s, const char *prefix)
{
  while (*prefix)
  {
    if (*s != *prefix)
    {
      return 0;
    }
    s++;
    prefix++;
  }
  return 1;
}

static int is_service_log(const char *msg, const char *name);

static int parse_int(const char *s)
{
  int value = 0;
  while (*s >= '0' && *s <= '9')
  {
    value = value * 10 + (*s - '0');
    s++;
  }
  return value;
}

static size_t copy_line(char *dst, size_t dst_len, const char *src)
{
  size_t i = 0;
  if (dst_len == 0)
  {
    return 0;
  }
  while (src && src[i] && i < dst_len - 1)
  {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
  return i;
}

static void history_add(const char *line)
{
  if (!line || line[0] == '\0')
  {
    return;
  }

  if (shell_history_len > 0 && str_eq(shell_history[shell_history_len - 1], line))
  {
    return;
  }

  if (shell_history_len < SHELL_HISTORY_MAX)
  {
    copy_line(shell_history[shell_history_len], SHELL_LINE_MAX, line);
    shell_history_len++;
    return;
  }

  for (size_t i = 1; i < SHELL_HISTORY_MAX; ++i)
  {
    copy_line(shell_history[i - 1], SHELL_LINE_MAX, shell_history[i]);
  }
  copy_line(shell_history[SHELL_HISTORY_MAX - 1], SHELL_LINE_MAX, line);
}

static uint64_t u64_div_u32(uint64_t value, uint32_t divisor, uint32_t *remainder)
{
  uint64_t quotient = 0;
  uint64_t rem = 0;

  for (int i = 63; i >= 0; --i)
  {
    rem = (rem << 1) | ((value >> i) & 1u);
    if (rem >= divisor)
    {
      rem -= divisor;
      quotient |= (1ull << i);
    }
  }

  if (remainder)
  {
    *remainder = (uint32_t)rem;
  }
  return quotient;
}

static void print_uint64(uint64_t value)
{
  char buf[21];
  size_t i = 0;

  if (value == 0)
  {
    terminal_write("0");
    return;
  }

  while (value > 0 && i < sizeof(buf) - 1)
  {
    uint32_t rem = 0;
    value = u64_div_u32(value, 10, &rem);
    buf[i++] = (char)('0' + rem);
  }

  while (i > 0)
  {
    terminal_write((char[]){buf[--i], '\0'});
  }
}

static void print_padded(const char *text, size_t width)
{
  size_t len = 0;
  while (text && text[len])
  {
    len++;
  }

  if (text)
  {
    terminal_write(text);
  }

  while (len < width)
  {
    terminal_write(" ");
    len++;
  }
}

static uint32_t find_process_pid(const char *name, size_t *index_out)
{
  size_t count = process_count();
  for (size_t i = 0; i < count; ++i)
  {
    if (!process_is_used(i))
    {
      continue;
    }
    const char *proc_name = process_name(i);
    if (proc_name && str_eq(proc_name, name))
    {
      if (index_out)
      {
        *index_out = i;
      }
      return process_pid(i);
    }
  }
  return 0;
}

static const char *log_level_name(uint8_t level)
{
  switch (level)
  {
  case LOG_WARN:
    return "WARN";
  case LOG_ERROR:
    return "ERROR";
  default:
    return "INFO";
  }
}

static void set_log_color(uint8_t level)
{
  switch (level)
  {
  case LOG_WARN:
    vga_set_color(0x0E, 0x00);
    break;
  case LOG_ERROR:
    vga_set_color(0x0C, 0x00);
    break;
  default:
    vga_set_color(0x07, 0x00);
    break;
  }
}

static int log_matches_service(const log_entry_t *entry, const char *service)
{
  if (!service || !service[0])
  {
    return 1;
  }
  return is_service_log(entry->msg, service);
}

static void shell_logs_print(const char *service)
{
  log_entry_t entry;
  uint64_t seq = log_oldest_seq();
  uint8_t prev_color = vga_get_color();
  int found = 0;

  while (log_read(seq, &entry))
  {
    if (log_matches_service(&entry, service))
    {
      set_log_color(entry.level);
      terminal_write("[");
      terminal_write(log_level_name(entry.level));
      terminal_write("] ");
      terminal_writeln(entry.msg);
      found = 1;
    }
    seq++;
  }

  if (!found)
  {
    vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
    terminal_writeln("no logs");
  }
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
}

static void shell_logs_tail(const char *service)
{
  log_entry_t entry;
  uint64_t seq = log_oldest_seq();
  uint8_t prev_color = vga_get_color();

  shell_logs_print(service);
  seq = log_latest_seq();

  vga_set_color(0x0B, 0x00);
  terminal_writeln("-- tailing logs (Ctrl+C to exit) --");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
  for (;;)
  {
    watchdog_kick();
    int key = keyboard_poll_key();
    if (key == 3)
    {
      terminal_writeln("-- stopped --");
      return;
    }

    if (log_read(seq, &entry))
    {
      if (log_matches_service(&entry, service))
      {
        set_log_color(entry.level);
        terminal_write("[");
        terminal_write(log_level_name(entry.level));
        terminal_write("] ");
        terminal_writeln(entry.msg);
        vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
      }
      seq++;
      continue;
    }

    process_yield();
  }
}

static void shell_processes(void)
{
  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_writeln("PID  STATE    NAME");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
  size_t count = process_count();
  for (size_t i = 0; i < count; ++i)
  {
    if (!process_is_used(i))
    {
      continue;
    }
    const char *name = process_name(i);
    uint32_t pid = process_pid(i);
    terminal_write(" ");
    if (pid < 10)
    {
      terminal_write(" ");
    }
    if (pid < 100)
    {
      terminal_write(" ");
    }
    print_uint64(pid);
    terminal_write("  ");
    if (process_is_kill_requested(i))
    {
      terminal_write("killing ");
    }
    else
    {
      terminal_write(process_is_active(i) ? "running " : "stopped ");
    }
    terminal_writeln(name ? name : "(null)");
  }
}

static void shell_top(void)
{
  terminal_writeln("-- top-lite (Ctrl+C to exit) --");
  for (;;)
  {
    int key = keyboard_poll_key();
    if (key == 3)
    {
      terminal_writeln("-- stopped --");
      return;
    }

    terminal_clear();
    shell_processes();
    terminal_writeln("");
    terminal_writeln("Ctrl+C to exit");

    for (uint32_t i = 0; i < 20000; ++i)
    {
      process_yield();
    }
  }
}

static void shell_kill(const char *line)
{
  int force = 0;
  const char *args = line + 4;

  while (*args == ' ')
  {
    args++;
  }

  if (args[0] == '-' && args[1] == 'f')
  {
    force = 1;
    args += 2;
    while (*args == ' ')
    {
      args++;
    }
  }

  int pid = parse_int(args);
  if (pid <= 0)
  {
    terminal_writeln("usage: kill <pid> [-f]");
    return;
  }

  while (*args && *args != ' ')
  {
    args++;
  }
  while (*args == ' ')
  {
    args++;
  }
  if (args[0] == '-' && args[1] == 'f')
  {
    force = 1;
  }

  if (process_kill((uint32_t)pid, force) != 0)
  {
    terminal_writeln("kill failed");
    return;
  }

  terminal_writeln(force ? "killed" : "terminate requested");
}

static void shell_help(void)
{
  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_writeln("Commands:");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
  terminal_writeln("  help            Show this help");
  terminal_writeln("  clear           Clear the screen");
  terminal_writeln("  echo <text>     Print text");
  terminal_writeln("  color <fg> <bg> Set VGA color (0-15)");
  terminal_writeln("  ps              List processes");
  terminal_writeln("  top             Process monitor");
  terminal_writeln("  kill <pid> [-f]  Kill process");
  terminal_writeln("  services        List services");
  terminal_writeln("  services <name> status|start|stop|restart|info");
  terminal_writeln("  logs            View system logs");
  terminal_writeln("  logs <service>  View service logs");
  terminal_writeln("  logs -t [name]  Tail logs (system or service)");
  terminal_writeln("  mem             Show heap stats");
  terminal_writeln("  gfxdemo         Draw using gfx driver");
  terminal_writeln("  panic <msg>     Trigger panic screen");
  terminal_writeln("  reboot          Reboot the system");
  terminal_writeln("  shutdown        Power off (QEMU)");
}

static void shell_gfx_demo(void)
{
  gfx_display_t *display = gfx_open_display();
  gfx_window_t window = gfx_create_simple_window(display, 0, 0, display->width, display->height);
  gfx_gc_t gc = gfx_create_gc(display, 0x0F);

  terminal_writeln("Entering gfx demo. Press any key to return.");
  gfx_map_window(display, &window);

  if (display && display->buffer)
  {
    for (uint16_t y = 0; y < display->height; ++y)
    {
      uint8_t color = (uint8_t)((y * 255u) / display->height);
      for (uint16_t x = 0; x < display->width; ++x)
      {
        display->buffer[y * display->pitch + x] = color;
      }
    }
  }

  gfx_set_foreground(display, &gc, 0x1F);
  gfx_draw_rect(display, &window, &gc, 4, 4, (uint16_t)(display->width - 8), (uint16_t)(display->height - 8));

  for (uint16_t i = 0; i < 8; ++i)
  {
    uint16_t bar_width = (uint16_t)(display->width / 8);
    uint16_t x = (uint16_t)(i * bar_width);
    gfx_set_foreground(display, &gc, (uint8_t)(i * 32));
    gfx_fill_rect(display, &window, &gc, (int16_t)x, 16, bar_width, 40);
  }

  gfx_set_foreground(display, &gc, 0xE3);
  gfx_draw_line(display, &window, &gc, 0, 0, (int16_t)(display->width - 1), (int16_t)(display->height - 1));
  gfx_set_foreground(display, &gc, 0x1C);
  gfx_draw_line(display, &window, &gc, 0, (int16_t)(display->height - 1), (int16_t)(display->width - 1), 0);

  gfx_flush(display);

  for (uint8_t i = 0; i < 16; ++i)
  {
    if (keyboard_poll_key() == KEY_NONE)
    {
      break;
    }
    process_yield();
  }

  for (;;)
  {
    if (keyboard_poll_key() != KEY_NONE)
    {
      break;
    }
    process_yield();
  }

  gfx_unmap_window(display, &window);
  terminal_init();
  terminal_writeln("Exited gfx demo.");
}

static void shell_mem(void)
{
  heap_stats_t stats;
  heap_get_stats(&stats);

  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_writeln("Heap stats:");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));

  terminal_write("  total: ");
  print_uint64(stats.total_bytes);
  terminal_writeln(" bytes");

  terminal_write("  used:  ");
  print_uint64(stats.used_bytes);
  terminal_writeln(" bytes");
  terminal_write("         ( ");
  print_uint64(stats.used_percent);
  terminal_writeln("% )");

  terminal_write("  free:  ");
  print_uint64(stats.free_bytes);
  terminal_writeln(" bytes");
  terminal_write("         ( ");
  print_uint64(stats.free_percent);
  terminal_writeln("% )");

  terminal_write("  blocks: ");
  print_uint64(stats.blocks);
  terminal_writeln("");

  terminal_write("  free blocks: ");
  print_uint64(stats.free_blocks);
  terminal_writeln("");

  terminal_write("  largest free: ");
  print_uint64(stats.largest_free);
  terminal_writeln(" bytes");

  terminal_write("  fragmentation: ");
  print_uint64(stats.frag_percent);
  terminal_writeln("%");
}

static int is_service_log(const char *msg, const char *name)
{
  const char *prefix = "service:";
  if (!str_starts_with(msg, prefix))
  {
    return 0;
  }

  const char *p = msg + 8;
  while (*name)
  {
    if (*p != *name)
    {
      return 0;
    }
    p++;
    name++;
  }
  return *p == ' ';
}

static void shell_services_list(void)
{
  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_writeln("SERVICE   STATE     PID");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));

  size_t count = services_count();
  for (size_t i = 0; i < count; ++i)
  {
    const char *name = services_name(i);
    terminal_write(" ");
    print_padded(name ? name : "(null)", 9);
    terminal_write(services_is_running(i) ? "running  " : "stopped  ");
    size_t idx = 0;
    uint32_t pid = name ? find_process_pid(name, &idx) : 0;
    if (pid)
    {
      print_uint64(pid);
    }
    else
    {
      terminal_write("-");
    }
    terminal_writeln("");
  }
}

static void shell_services_info(const char *name)
{
  size_t index = 0;
  if (services_find(name, &index) != 0)
  {
    terminal_writeln("unknown service");
    return;
  }

  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_write("Service: ");
  terminal_writeln(name);
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));

  terminal_write("  state: ");
  terminal_writeln(services_is_running(index) ? "running" : "stopped");

  size_t proc_idx = 0;
  uint32_t pid = find_process_pid(name, &proc_idx);
  terminal_write("  pid: ");
  if (pid)
  {
    print_uint64(pid);
  }
  else
  {
    terminal_write("-");
  }
  terminal_writeln("");

  terminal_write("  deps: ");
  size_t dep_count = 0;
  const char **deps = services_deps(index, &dep_count);
  if (dep_count == 0 || !deps)
  {
    terminal_writeln("-");
  }
  else
  {
    for (size_t d = 0; d < dep_count; ++d)
    {
      if (deps[d])
      {
        terminal_write(deps[d]);
      }
      if (d + 1 < dep_count)
      {
        terminal_write(",");
      }
    }
    terminal_writeln("");
  }

  terminal_write("  autorestart: ");
  if (services_autorestart(index))
  {
    terminal_write("yes (");
    print_uint64((uint64_t)services_restart_attempts(index));
    terminal_write("/");
    print_uint64((uint64_t)services_restart_limit(index));
    terminal_writeln(")");
  }
  else
  {
    terminal_writeln("no");
  }
}

static void shell_services_status(const char *name)
{
  size_t index = 0;
  if (services_find(name, &index) != 0)
  {
    terminal_writeln("unknown service");
    return;
  }

  terminal_write(name);
  terminal_write(": ");
  terminal_writeln(services_is_running(index) ? "running" : "stopped");

  size_t proc_idx = 0;
  uint32_t pid = find_process_pid(name, &proc_idx);
  if (pid)
  {
    terminal_write("pid: ");
    print_uint64(pid);
    terminal_writeln("");
  }
}

static void shell_services_command(const char *line)
{
  const char *args = line + 8;
  while (*args == ' ')
  {
    args++;
  }

  if (*args == '\0' || str_eq(args, "list"))
  {
    shell_services_list();
    return;
  }

  char name_buf[16];
  size_t name_len = 0;
  while (*args && *args != ' ' && name_len < sizeof(name_buf) - 1)
  {
    name_buf[name_len++] = *args++;
  }
  name_buf[name_len] = '\0';

  while (*args == ' ')
  {
    args++;
  }

  const char *action = args;
  if (name_len == 0)
  {
    shell_services_list();
    return;
  }

  if (services_find(name_buf, 0) != 0)
  {
    terminal_writeln("unknown service");
    return;
  }

  if (*action == '\0' || str_eq(action, "status"))
  {
    shell_services_status(name_buf);
    return;
  }

  if (str_eq(action, "start"))
  {
    if (services_start(name_buf) != 0)
    {
      terminal_writeln("failed to start service");
      return;
    }
    terminal_writeln("service started");
    return;
  }

  if (str_eq(action, "stop"))
  {
    if (services_stop(name_buf) != 0)
    {
      terminal_writeln("failed to stop service");
      return;
    }
    terminal_writeln("service stopped");
    return;
  }

  if (str_eq(action, "restart"))
  {
    if (services_restart(name_buf) != 0)
    {
      terminal_writeln("failed to restart service");
      return;
    }
    terminal_writeln("service restarted");
    return;
  }

  if (str_eq(action, "info"))
  {
    shell_services_info(name_buf);
    return;
  }

  terminal_writeln("usage: services <name> status|start|stop|restart|info");
}

static void shell_logs_command(const char *line)
{
  const char *args = line + 4;
  while (*args == ' ')
  {
    args++;
  }

  int tail = 0;
  if (args[0] == '-' && args[1] == 't')
  {
    tail = 1;
    args += 2;
    while (*args == ' ')
    {
      args++;
    }
  }

  char name_buf[16];
  size_t name_len = 0;
  while (*args && *args != ' ' && name_len < sizeof(name_buf) - 1)
  {
    name_buf[name_len++] = *args++;
  }
  name_buf[name_len] = '\0';

  if (name_len == 0)
  {
    if (tail)
    {
      shell_logs_tail(0);
    }
    else
    {
      shell_logs_print(0);
    }
    return;
  }

  if (services_find(name_buf, 0) != 0)
  {
    terminal_writeln("unknown service");
    return;
  }

  if (tail)
  {
    shell_logs_tail(name_buf);
  }
  else
  {
    shell_logs_print(name_buf);
  }
}

static void shell_handle_command(const char *line)
{
  if (str_eq(line, ""))
  {
    return;
  }

  if (str_eq(line, "help"))
  {
    shell_help();
    return;
  }

  if (str_eq(line, "clear"))
  {
    terminal_clear();
    return;
  }

  if (str_starts_with(line, "echo "))
  {
    terminal_writeln(line + 5);
    return;
  }

  if (str_starts_with(line, "color "))
  {
    const char *args = line + 6;
    int fg = parse_int(args);
    while (*args && *args != ' ')
    {
      args++;
    }
    while (*args == ' ')
    {
      args++;
    }
    int bg = parse_int(args);
    vga_set_color((uint8_t)fg, (uint8_t)bg);
    terminal_writeln("Color updated");
    return;
  }

  if (str_eq(line, "ps") || str_eq(line, "processes"))
  {
    shell_processes();
    return;
  }

  if (str_eq(line, "top"))
  {
    shell_top();
    return;
  }

  if (str_starts_with(line, "kill"))
  {
    shell_kill(line);
    return;
  }

  if (str_starts_with(line, "services"))
  {
    shell_services_command(line);
    return;
  }

  if (str_starts_with(line, "logs"))
  {
    shell_logs_command(line);
    return;
  }

  if (str_eq(line, "mem"))
  {
    shell_mem();
    return;
  }

  if (str_eq(line, "gfxdemo"))
  {
    shell_gfx_demo();
    return;
  }

  if (str_eq(line, "reboot"))
  {
    terminal_writeln("Rebooting...");
    power_reboot();
    return;
  }

  if (str_eq(line, "shutdown"))
  {
    terminal_writeln("Shutting down...");
    power_shutdown();
    return;
  }

  if (str_starts_with(line, "panic"))
  {
    const char *msg = line + 5;
    while (*msg == ' ')
    {
      msg++;
    }
    panic(*msg ? msg : "panic requested");
  }

  terminal_writeln("Unknown command. Type 'help'.");
}

void shell_run(void)
{
  char line[SHELL_LINE_MAX];
  char scratch[SHELL_LINE_MAX];

  uint8_t prev_color = vga_get_color();
  vga_set_color(0x0B, 0x00);
  terminal_writeln("Shell started. Type 'help' for commands.");
  vga_set_color(prev_color & 0x0F, (uint8_t)(prev_color >> 4));
  for (;;)
  {
    terminal_write("os> ");
    shell_history_pos = shell_history_len;
    terminal_readline_history(line, sizeof(line),
                              (const char **)shell_history,
                              shell_history_len,
                              &shell_history_pos,
                              scratch, sizeof(scratch));
    history_add(line);
    shell_handle_command(line);
  }
}
