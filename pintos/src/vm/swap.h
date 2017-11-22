#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/disk.h"
#include <hash.h>


struct lock swap_lock;
struct hash swap_hash;

static int max_disk_size;
static disk_sector_t sec_no;
static int cnt;

struct swap {
	tid_t tid;
	void *upage;
	disk_sector_t sec_no;
	struct hash_elem elem;
};

void swap_init(void);
void swap_out(void *);
void *swap_in(void);
void swap_add_hash(void *);
void swap_remove_hash(void);
struct swap *swap_get_by_upage(void *);

#endif
