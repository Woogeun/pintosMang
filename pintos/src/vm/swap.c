#include "vm/swap.h"

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "devices/disk.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>

struct bitmap swap_bitmap;
struct disk *d;

static void swap_print_table(void);
static int swap_bitmap_scan_and_flip(void);
static struct swap *swap_alloc(int, void *, struct thread *);

void swap_init(void) {

	list_init(&swap_list);
	lock_init(&swap_lock);

	d = disk_get(1,1);

	swap_bitmap.count = 0;
	swap_bitmap.max = disk_size(d) / (PGSIZE / DISK_SECTOR_SIZE);
	swap_bitmap.used_map = (int *) malloc (sizeof(int) * swap_bitmap.max);
	memset(swap_bitmap.used_map, 0, swap_bitmap.max);
}



void swap_out(struct frame *f) {

	void *kpage = f->kpage;
	void *upage = f->upage;

	size_t remained = PGSIZE;
	disk_sector_t sec_no;

	
	lock_acquire(&swap_lock);

	int id = swap_bitmap_scan_and_flip();
	int count = 0;
	
	if (id == -1)
		PANIC("No empty space in swap disk");

	sec_no = id * (PGSIZE / DISK_SECTOR_SIZE);

	struct swap *s = swap_alloc(id, upage, f->thread);

	while (remained > 0) {
		disk_write(d, sec_no, kpage);
		s->sec_nos[count] = sec_no;
		sec_no++;
		kpage += DISK_SECTOR_SIZE;
		remained -= DISK_SECTOR_SIZE;
		count ++;
	}
	ASSERT(count == 8);

	lock_release(&swap_lock);
}

void swap_in(struct frame *f) {

	void *kpage = f->kpage;
	void *upage = f->upage;

	lock_acquire(&swap_lock);

	struct swap *s = swap_get_by_upage(f->thread, upage);

	if (s == NULL)
		PANIC("[%d] No such user page \"0x%8x\" in swap disk", f->thread->tid, (unsigned) upage);

	int index;

	for (index = 0; index < 8; index ++) {
		disk_read(d, s->sec_nos[index], kpage);
		kpage += DISK_SECTOR_SIZE;
	}

	swap_free(s);

	lock_release(&swap_lock);
}

static int swap_bitmap_scan_and_flip() {
	int *map = swap_bitmap.used_map;
	int i, max = swap_bitmap.max;
	for (i = 0; i < max; i++)
		if (map[i] == 0) {
			map[i] = 1;
			swap_bitmap.count++;
			return i;
		}
	return -1;
}


struct swap *swap_get_by_upage(struct thread *t, void *upage) {

	tid_t curr_tid = t->tid;
	struct list_elem *e;
	for (e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
		struct swap *s = list_entry(e, struct swap, elem);
		if (s->tid == curr_tid)
			if (s->upage == upage) {
				return s;
			}
	}

	return NULL;
}


static void swap_print_table() {
	struct list_elem *e;
	printf("====================== swap table ====================\n");
	for (e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
		struct swap *s = list_entry(e, struct swap, elem);
		printf("id: %2d, tid: %2d, upage: 0x%8x\n", s->id, s->tid, (unsigned) s->upage);
	}
	printf("======================================================\n");
}



static struct swap *swap_alloc(int id, void *upage, struct thread *t) {

	struct list_elem *e;
	for (e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
		struct swap *s = list_entry(e, struct swap, elem);
		if (s->id == id)
			PANIC("there are same id in swap disk");
	}

	struct swap *s = (struct swap *) malloc (sizeof(struct swap));
	s->id = id;
	s->tid = t->tid;
	s->upage = upage;

	list_push_back(&swap_list, &s->elem);

	return s;
}

void swap_free(struct swap *s) {

	swap_bitmap.used_map[s->id] = 0;
	swap_bitmap.count--;
	list_remove(&s->elem);
	free(s);
}












