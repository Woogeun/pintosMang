#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include <debug.h>
#include "vm/page.h"
#include "vm/swap.h"

void frame_print_table(int count) {
	struct list_elem *e;
	int c = 0;
	printf("===================== frame list ==================\n");
	for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
		if (c ++ >= count)
			break;
		struct frame *f = list_entry(e, struct frame, elem);	
		printf("upage : 0x%8x, kpage : 0x%8x\n", f->upage, f->kpage);

	}
	printf("====================================================\n");
}

void frame_init(void) {

	list_init(&frame_list);
	lock_init(&frame_lock);
}

// frame_get_page : alloc new page (not link with kpage and upage). 
struct frame *frame_get_page(enum palloc_flags flags, void *upage) {

	lock_acquire(&frame_lock);

	// try alloc page. NULL if no free page
	void *kpage = palloc_get_page(flags);

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
	if (f == -1)
		PANIC("malloc failure");

	f->thread = thread_current();
	f->kpage = kpage;
	f->upage = upage;
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

struct frame *frame_evict_page(void) {
	struct list_elem *e = list_begin(&frame_list);
	return list_entry(e, struct frame, elem);
	// for ( ; ; ) {
	// 	struct frame *f = list_entry(e, struct frame, elem);

	// 	if (pagedir_is_accessed(f->thread->pagedir, f->upage)) {
	// 		pagedir_set_accessed(f->thread->pagedir, f->upage, false);
	// 		if (pagedir_is_dirty(f->thread->pagedir, f->upage))
	// 			pagedir_set_dirty(f->thread->pagedir, f->upage, false);
	// 	}
	// 	else
	// 		return f;

	// 	if (e == list_end(&frame_list)) {
	// 		e = list_begin(&frame_list);
	// 	}
	// 	else 
	// 		e = list_next(e);
	// }
}














