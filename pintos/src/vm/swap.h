#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/disk.h"
#include "vm/frame.h"
#include <list.h>

struct lock swap_lock;
struct list swap_list;

//new structure in swap.c
struct bitmap {
	int count;						/* number of using swap pages */
	int max;						/* number of max usable swap pages */
	int *used_map;					/* int array. if k is used, k indexed set 1, otherwise 0. */
};

struct swap {
	int id;							/* id is equal to index of bitmap */
	tid_t tid;						/* owner thread pointer of this swap page */
	void *upage;					/* virtual page address */
	disk_sector_t sec_nos[8];		/* sector informations of this swap structure */
	struct list_elem elem;			/* element for list management */
};

//init function
void swap_init(void);

//read and write to swap disk
void swap_out(struct frame *);
void swap_in(struct frame *);

//swap list management function
void swap_add_list(struct list *, void *, disk_sector_t);
void swap_free(struct swap *);
struct swap *swap_get_by_upage(struct thread *, void *);


#endif
