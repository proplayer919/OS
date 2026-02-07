#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>

void terminal_init(void);
void terminal_clear(void);
void terminal_write(const char *str);
void terminal_writeln(const char *str);
size_t terminal_readline(char *buffer, size_t max_len);
size_t terminal_readline_history(char *buffer, size_t max_len,
                                 const char **history, size_t history_len,
                                 size_t *history_pos,
                                 char *scratch, size_t scratch_len);

#endif
