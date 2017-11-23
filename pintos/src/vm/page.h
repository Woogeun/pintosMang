#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/thread.h"

#define STACK_INITIAL_UPAGE PHYS_BASE - PGSIZE
#define UPAGE_BOTTOM 0x08048000

enum page_position {
	ON_DISK,
	ON_MEMORY,
	ON_SWAP,
};

struct page {
	void *upage;
	bool writable;
	enum page_position position;
	struct hash_elem elem;
};

//struct hash page_hash;

void page_init(void);

void page_add_hash(void *, bool, enum page_position);
void page_remove_hash(void *);
struct page *page_get_by_upage(void *);


bool page_load_segment(struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);
bool page_setup_stack(void **, int, char **);
bool install_page(void *, void *, bool);
void page_fault_handler(void *, void *, bool, bool);


#endif
