#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include "threads/palloc.h"
#include <list.h>


struct list frame_list;
struct lock frame_lock;

struct frame {
	void *upage;
	void *kpage;
	struct list_elem elem;
};

void frame_init(void);
void *frame_get_page(enum palloc_flags, void *);
void frame_free_page(void *);
void *frame_evict_page(void);

void frame_add_list(struct frame *);
struct frame *frame_pop_front_list(void);
void frame_accessed(void *);
struct frame *frame_get_by_kpage(void *, bool);

#endif
