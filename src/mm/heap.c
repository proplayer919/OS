#include "mm/heap.h"
#include <stdint.h>

typedef struct heap_block
{
  size_t size;
  int free;
  struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head;
static size_t heap_total_bytes;

static size_t align8(size_t size)
{
  return (size + 7u) & ~7u;
}

void heap_init(void *base, size_t size)
{
  heap_head = (heap_block_t *)base;
  heap_head->size = size - sizeof(heap_block_t);
  heap_head->free = 1;
  heap_head->next = 0;
  heap_total_bytes = size;
}

void *kmalloc(size_t size)
{
  size = align8(size);
  heap_block_t *current = heap_head;

  while (current)
  {
    if (current->free && current->size >= size)
    {
      if (current->size >= size + sizeof(heap_block_t) + 8)
      {
        heap_block_t *next = (heap_block_t *)((uint8_t *)current + sizeof(heap_block_t) + size);
        next->size = current->size - size - sizeof(heap_block_t);
        next->free = 1;
        next->next = current->next;
        current->next = next;
        current->size = size;
      }
      current->free = 0;
      return (uint8_t *)current + sizeof(heap_block_t);
    }
    current = current->next;
  }

  return 0;
}

void kfree(void *ptr)
{
  if (!ptr)
  {
    return;
  }

  heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
  block->free = 1;

  heap_block_t *current = heap_head;
  while (current && current->next)
  {
    if (current->free && current->next->free)
    {
      current->size += sizeof(heap_block_t) + current->next->size;
      current->next = current->next->next;
      continue;
    }
    current = current->next;
  }
}

void heap_get_stats(heap_stats_t *out)
{
  if (!out)
  {
    return;
  }

  size_t used = 0;
  size_t free = 0;
  size_t blocks = 0;
  size_t free_blocks = 0;
  size_t largest_free = 0;

  heap_block_t *current = heap_head;
  while (current)
  {
    blocks++;
    if (current->free)
    {
      free_blocks++;
      free += current->size;
      if (current->size > largest_free)
      {
        largest_free = current->size;
      }
    }
    else
    {
      used += current->size;
    }
    current = current->next;
  }

  out->total_bytes = heap_total_bytes;
  out->used_bytes = used;
  out->free_bytes = free;
  if (heap_total_bytes > 0)
  {
    out->used_percent = (used * 100u) / heap_total_bytes;
    out->free_percent = (free * 100u) / heap_total_bytes;
  }
  else
  {
    out->used_percent = 0;
    out->free_percent = 0;
  }
  if (free > 0)
  {
    out->frag_percent = (size_t)(100u - ((largest_free * 100u) / free));
  }
  else
  {
    out->frag_percent = 0;
  }
  out->blocks = blocks;
  out->free_blocks = free_blocks;
  out->largest_free = largest_free;
}
