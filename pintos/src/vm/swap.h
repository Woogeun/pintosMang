#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/disk.h"
#include "vm/frame.h"
#include <list.h>

struct lock swap_lock;
struct list swap_list;

struct bitmap {
	int count;
	int max;
	int *used_map;
};

struct swap {
	int id;
	tid_t tid;
	void *upage;
	disk_sector_t sec_nos[8];
	struct list_elem elem;
};

void swap_init(void);
void swap_out(struct frame *);
void swap_in(struct frame *);
void swap_add_list(struct list *, void *, disk_sector_t);
void swap_remove_list(void);
struct swap *swap_get_by_upage(struct thread *, void *);
void swap_free(struct swap *);

#endif
