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
	//printf("<<frame_get_page>>\n");

	void *kpage = palloc_get_page(flags);

	if (kpage == NULL) {
		struct frame *evicted = frame_evict_page();
		void *evicted_upage;
		if (evicted == NULL)
			PANIC("no frame to evict");

		evicted_upage = evicted->upage;
		swap_out(evicted_upage);
		frame_free_page(evicted);

		struct page *p = page_get_by_upage(evicted_upage);
		if (p == NULL)
			PANIC("no such page in page table. ");
		p->position = ON_SWAP;
		kpage = palloc_get_page (flags);

		if (kpage == NULL)
			PANIC("no frame to alloc after evict frame");
	}

	struct frame *f = (struct frame *) malloc (sizeof(struct frame));
	f->kpage = kpage;
	f->upage = upage;

	frame_add_list(f);
	//printf("<<frame_get_page<kpage, upage>: <0x%8x, 0x%8x>>>\n", f->kpage, f->upage);

	return f;
}


void frame_free_page(struct frame *f) {
	
	palloc_free_page(f->kpage);
	list_remove(&f->elem);
	free(f);
}

struct frame *frame_evict_page(void) {
	//printf("<<frame_evict_page>>\n");

	struct frame *f = list_entry(list_begin(&frame_list), struct frame, elem);
	if (f == NULL)
		PANIC("No frame to evict");
	return f;
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














