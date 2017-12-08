#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include <list.h>


struct list frame_list;
struct lock frame_lock;
struct list_elem *evict_turn_frame;

//new structure in frame.c
struct frame {
	struct thread *thread;			/* thread pointer of occupying frame */
	void *upage;					/* viftual page address */
	void *kpage;					/* physical page address */
	bool chance;					/* second chance information */
	struct list_elem elem;			/* element for list management */
};

//init function
void frame_init(void);

//improved palloc function to interact with page.c
struct frame *frame_get_page(enum palloc_flags, void *);
void frame_free_page(struct frame *);
struct frame *frame_evict_page(void);
struct frame *frame_get_by_upage(void *);

void frame_print(struct frame *);
void frame_print_table(void);

#endif
