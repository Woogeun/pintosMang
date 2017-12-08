#include "filesys/cache.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <debug.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"


#define MAX_BUFFER 64

static int numofused = 0;
static int write_count = 0;
static void cache_print(void);

static void cache_print() {
	struct list_elem *e;
	int count = 1;
	printf("=========cache buffer========\n");
	for (e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
		struct buffer *b = list_entry(e, struct buffer, elem);
		printf("%2d | [%3d] %2x %2x %2x %2x %s\n", count++, b->sector, b->data[0], b->data[1], b->data[2], b->data[3], b->chance ? "chance" : "no chance");
	}
	printf("=============================\n");
}

void cache_init(void){
	list_init(&buffer_cache);
	lock_init(&cache_lock);
	lock_init(&cache_read_ahead_lock);
	evict_turn_cache = NULL;
}

struct buffer *cache_add_buffer(disk_sector_t sector) {
	ASSERT(numofused <= MAX_BUFFER);
	
	// evict cache if it is full
	if (numofused == MAX_BUFFER) {
		struct buffer *evict_buffer = cache_select_evict_buffer();
		ASSERT(evict_buffer != NULL);

		cache_flush_buffer(evict_buffer);
		list_remove(&evict_buffer->elem);
		numofused--;

		free(evict_buffer);
	}

	// asynchronous read ahead 
 //    disk_sector_t *sector_next = palloc_get_page(0);
 //    *sector_next = sector + 1;
	// thread_create("cache_read_ahead", PRI_DEFAULT, cache_read_ahead, sector_next);

	// allocate new cache buffer
	struct buffer *b = cache_alloc_buffer();
	b->sector = sector;
	b->is_written = false;
	b->chance = true;

	list_push_back(&buffer_cache, &b->elem);
	numofused++;

	//lock_release(&cache_lock);

	return b;
}

struct buffer *cache_alloc_buffer() {
	struct buffer *b = (struct buffer *) malloc (sizeof(struct buffer));
	b-> sector = 0;
	memset(b->data, 0, sizeof(b->data));
	return b;
}

struct buffer *cache_select_evict_buffer() {

	if (evict_turn_cache == NULL)
		evict_turn_cache = list_begin(&buffer_cache);

	while (true) {
		struct buffer *b = list_entry(evict_turn_cache, struct buffer, elem);

		if (evict_turn_cache == list_rbegin(&buffer_cache)) {
			evict_turn_cache = list_begin(&buffer_cache);
		}
		else {
			evict_turn_cache = list_next(evict_turn_cache);
		}

		if (!b->chance) {
			return b;
		} else {
			b->chance = false;
		}
	}
}

struct buffer *cache_find_buffer(disk_sector_t sector){
	//lock_acquire(&cache_lock);
	
	struct list_elem *e;
	for (e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
		struct buffer *b = list_entry(e, struct buffer, elem);
		if (b->sector == sector) {
			//lock_release(&cache_lock);
			return b;
		}
	}
	//lock_release(&cache_lock);
	return NULL;
}

void cache_flush_buffer(struct buffer *b) {
	if (b->is_written) {
		disk_write(filesys_disk, b->sector, b->data);
	}
}

void cache_flush(bool is_halt) {
	//lock_acquire(&cache_lock);

	struct list_elem *e;

	if (is_halt) {
		while (!list_empty(&buffer_cache)) {
			e = list_pop_front(&buffer_cache);
			struct buffer *b = list_entry(e, struct buffer, elem);
			cache_flush_buffer(b);
			free(b);
		}
	}

	else {
		for (e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
			struct buffer *b = list_entry(e, struct buffer, elem);
			cache_flush_buffer(b);
			b->is_written = false;
		}
	}

	//lock_release(&cache_lock);
}

void cache_write_increase() {
	write_count++;
	if (write_count >= 50) {
		cache_flush(false);
		write_count = 0;
	}
}

void cache_read_ahead(void *sector_address) {
	lock_acquire(&cache_lock);

	disk_sector_t sector = *((disk_sector_t *) sector_address);

	struct buffer *temp = cache_find_buffer(sector);
	if (temp == NULL) {
		temp = cache_add_buffer(sector);
		disk_read(filesys_disk, temp->sector, temp->data);
	}

	temp->chance = true;
	palloc_free_page(sector_address);

	lock_release(&cache_lock);
}












