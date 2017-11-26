#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

#define STACK_INITIAL_UPAGE PHYS_BASE - PGSIZE
#define UPAGE_BOTTOM 0x08048000
#define STACK_MAX_SIZE 8000000

struct lock page_lock;

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
struct page *page_get_by_upage(struct thread *, void *);


bool page_load_segment(struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);
bool page_setup_stack(void **, int, char **);
bool install_page(void *, void *, bool);
void page_fault_handler(void *, void *, bool, bool, bool);

bool page_grow_stack(void *,void *);
void page_load_from_swap(struct page *);

void page_print_table(void);
void page_print(struct page *);


#endif
