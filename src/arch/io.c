#include "arch/io.h"

uint8_t inb(uint16_t port)
{
  uint8_t ret;
  __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

void outb(uint16_t port, uint8_t value)
{
  __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

void outw(uint16_t port, uint16_t value)
{
  __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}
