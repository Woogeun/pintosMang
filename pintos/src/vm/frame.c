#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <debug.h>
#include <stdio.h>
#include "vm/page.h"
#include "vm/swap.h"

void frame_print(struct frame *f) {

	printf("upage : 0x%8x, kpage : 0x%8x %s\n", (unsigned) f->upage, (unsigned) f->kpage, f->chance ? "chance" : "no chance");
}

void frame_print_table() {
	struct list_elem *e;
	printf("===================== frame list ==================\n");
	for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
		struct frame *f = list_entry(e, struct frame, elem);	
		frame_print(f);

	}
	printf("====================================================\n");
}

void frame_init(void) {

	list_init(&frame_list);
	lock_init(&frame_lock);
	evict_turn = NULL;
}

// frame_get_page : alloc new page (not link with kpage and upage). 
struct frame *frame_get_page(enum palloc_flags flags, void *upage) {

	// try alloc page. NULL if no free page
	void *kpage = palloc_get_page(flags);

	lock_acquire(&frame_lock);
	if (kpage == NULL) {

		// just find frame to be evicted
		struct frame *evicted = frame_evict_page();
		ASSERT(evicted != NULL);

		// write to swap disk
		swap_out(evicted); 

		// change page table entry position MEMORY->SWAP
		struct page *p = page_get_by_upage(evicted->thread, evicted->upage);
		ASSERT(p != NULL);
		p->position = ON_SWAP;

		// remove frame info from kernel
		pagedir_clear_page(evicted->thread->pagedir, evicted->upage);
		palloc_free_page(evicted->kpage);
		list_remove(&evicted->elem);
		free(evicted);

		// try alloc page again
		kpage = palloc_get_page (flags);
	}

	ASSERT(kpage != NULL);

	// add to frame list 
	struct frame *f = (struct frame *) malloc (sizeof(struct frame));
	if (f == NULL)
		PANIC("malloc failure");

	f->thread = thread_current();
	f->kpage = kpage;
	f->upage = upage;
	f->chance = true;
	list_push_back(&frame_list, &f->elem);

	lock_release(&frame_lock);

	return f;
}

void frame_free_page(struct frame *f) {

	lock_acquire(&frame_lock);

	ASSERT(f->thread != NULL);
	ASSERT(f->thread->pagedir != NULL);
	pagedir_clear_page(f->thread->pagedir, f->upage);
	palloc_free_page(f->kpage);
	list_remove(&f->elem);
	free(f);

	lock_release(&frame_lock);
}

// clock algorithm
struct frame *frame_evict_page(void) {
	
	if (evict_turn == NULL)
		evict_turn = list_begin(&frame_list);
	while (true) {
		struct frame *f = list_entry(evict_turn, struct frame, elem);

		if (evict_turn == list_rbegin(&frame_list)) {
			evict_turn = list_begin(&frame_list);
		}
		else {
			evict_turn = list_next(evict_turn);
		}

		if (!f->chance) {
				return f;
		} else {
			f->chance = false;
		}
	}
}

struct frame *frame_get_by_upage(void *upage) {

	lock_acquire(&frame_lock);

	struct thread *curr = thread_current();
	struct list_elem *e = list_begin(&frame_list);
	for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
		struct frame *f = list_entry(e, struct frame, elem);
		if (f->thread == curr)
			if (f->upage == upage) {
				lock_release(&frame_lock);
				return f;
			}
	}

	lock_release(&frame_lock);
	
	return NULL;
}












