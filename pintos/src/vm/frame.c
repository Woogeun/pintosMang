#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include <debug.h>

void frame_init(void) {

	list_init(&frame_list);
	lock_init(&frame_lock);
}

void *frame_alloc(enum palloc_flags flags UNUSED) {

	// void *vaddr = NULL;
	// struct frame *f = (struct frame *) malloc (sizeof(struct frame));
	// f->vaddr = vaddr;

	return NULL;
}


void frame_free(void *vaddr) {

	struct frame *f = frame_get_by_addr(vaddr, true);
	free(f);
}

void *frame_evict(void) {

	return frame_pop_front_list();
}

void frame_add_list(struct frame *f) {

	lock_acquire(&frame_lock);

	list_push_back(&frame_list, &f->elem);

	lock_release(&frame_lock);
}


struct frame *frame_pop_front_list(void) {

	lock_acquire(&frame_lock);

	struct frame *f = list_entry(list_pop_front(&frame_list), struct frame, elem);

	lock_acquire(&frame_lock);

	return f;
}

void frame_accessed(void *vaddr) {

	struct frame *f = frame_get_by_addr(vaddr, true);
	frame_add_list(f);
}

struct frame *frame_get_by_addr(void *vaddr, bool is_remove) {

	lock_acquire(&frame_lock);
	
	struct list_elem *e;
	for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
		struct frame *f = list_entry(e, struct frame, elem);
		if (f->vaddr == vaddr) {
			if (is_remove) 
				list_remove(&f->elem);
			lock_release(&frame_lock);
			return f;
		}
	}

	lock_release(&frame_lock);

	return NULL;
}














