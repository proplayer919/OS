#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <stddef.h>

void heap_init(void *base, size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr);

typedef struct
{
	size_t total_bytes;
	size_t used_bytes;
	size_t free_bytes;
	size_t used_percent;
	size_t free_percent;
	size_t frag_percent;
	size_t blocks;
	size_t free_blocks;
	size_t largest_free;
} heap_stats_t;

void heap_get_stats(heap_stats_t *out);

#endif
