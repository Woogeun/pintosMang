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

// used when load the lazy loaded page and memory map file 
struct disk_info {
	struct file *file;					/* which file */
	uint32_t page_read_bytes;			/* how many bytes to be written */
	uint32_t page_zero_bytes;			/* how many bytes need not to be written */
	off_t ofs;							/* offset of this file */
};

// used when classify that pages current position
enum page_position {
	ON_DISK,							/* page is not loaded */
	ON_MEMORY,							/* page is on memory */
	ON_SWAP,							/* page is on swap disk */
};

// used when page fault occurs
struct page {
	void *upage;						/* virtual address of this page */
	bool writable;						/* if true, write is allowed, otherwise r/o */
	enum page_position position;		/* current position of this page */
	struct disk_info d_info;			/* information of file to be referred. */
	struct hash_elem elem;				/* element for hash management */
};

//init function
void page_init(void);

//page hash table management function
struct page *page_add_hash(void *, bool, struct file *, size_t, size_t, off_t);
void page_remove_hash(struct page *);
struct page *page_get_by_upage(struct thread *, void *);

//
bool page_load_segment(struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);
bool page_setup_stack(void **, int, char **);
bool install_page(void *, void *, bool);

//using when page fault occurs
void page_fault_handler(void *, void *, bool, bool, bool);
bool page_grow_stack(void *,void *);

//migrate data among disk, memory, swap disk
void page_load_from_swap(struct page *);
void page_load_from_disk(struct page *);
void page_to_swap(struct page *);
void page_to_disk(struct page *);

void page_print_table(void);
void page_print(struct page *);


#endif
