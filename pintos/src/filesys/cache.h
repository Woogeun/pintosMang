#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "devices/disk.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct list_elem *evict_turn_cache;

struct buffer {
	disk_sector_t sector;
	bool is_written;
	bool chance;
	uint8_t data[512];
	struct list_elem elem;
};

struct list buffer_cache;
struct lock cache_lock;
struct lock cache_read_ahead_lock;

void cache_init(void);
void cache_flush_buffer(struct buffer *);
struct buffer *cache_add_buffer(disk_sector_t sector);
struct buffer *cache_alloc_buffer(void);
struct buffer *cache_select_evict_buffer(void);
struct buffer *cache_find_buffer(disk_sector_t);

void cache_flush(bool);
void cache_write_increase(void);
thread_func cache_read_ahead;

#endif /* filesys/cache.h */
