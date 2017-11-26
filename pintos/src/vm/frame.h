#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include <list.h>


struct list frame_list;
struct lock frame_lock;
//struct list_elem evict_turn;

struct frame {
	struct thread *thread;
	void *upage;
	void *kpage;
	struct list_elem elem;
};

void frame_init(void);
struct frame *frame_get_page(enum palloc_flags, void *);
void frame_free_page(struct frame *);
struct frame *frame_evict_page(void);

void frame_print_table(int);

#endif
