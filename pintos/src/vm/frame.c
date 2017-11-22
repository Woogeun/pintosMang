#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include <debug.h>
#include "vm/page.h"
#include "vm/swap.h"

void frame_init(void) {

	list_init(&frame_list);
	lock_init(&frame_lock);
}

void *frame_get_page(enum palloc_flags flags, void *upage) {

	void *kpage = palloc_get_page(flags);

	if (kpage == NULL) {
		void *evicted_kpage = frame_evict_page();

		if (evicted_kpage == NULL)
			PANIC("no frame to evict");

		swap_out(evicted_kpage);
		frame_free_page(evicted_kpage);

		kpage = palloc_get_page (flags);
	}

	if (kpage == NULL)
		PANIC("no frame to alloc after evict frame");

	struct frame *f = (struct frame *) malloc (sizeof(struct frame));
	f->kpage = kpage;
	f->upage = upage;

	frame_add_list(f);

	return kpage;
}


void frame_free_page(void *kpage) {

	struct frame *f = frame_get_by_kpage(kpage, true);
	if (f == NULL)
		PANIC("no such upage in frame list");
	palloc_free_page(f->kpage);
	free(f);
}

void *frame_evict_page(void) {

	struct frame *f = list_entry(list_begin(&frame_list), struct frame, elem);
	if (f == NULL)
		PANIC("No frame to evict");
	struct page *p = page_get_by_upage(f->upage);
	p->position = ON_SWAP;
	return f->kpage;
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

void frame_accessed(void *upage) {

	struct frame *f = frame_get_by_kpage(upage, true);
	if (f == NULL)
		PANIC("no such upage in frame list");
	frame_add_list(f);
}

struct frame *frame_get_by_kpage(void *kpage, bool is_remove) {

	lock_acquire(&frame_lock);
	
	struct list_elem *e;
	for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
		struct frame *f = list_entry(e, struct frame, elem);
		if (f->kpage == kpage) {
			if (is_remove) 
				list_remove(&f->elem);
			lock_release(&frame_lock);
			return f;
		}
	}

	lock_release(&frame_lock);

	return NULL;
}














