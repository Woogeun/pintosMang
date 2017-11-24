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
static int swap_bitmap_scan(void);
static struct swap *swap_alloc(int, void *);
static void swap_free(struct swap *);

void swap_init(void) {

	list_init(&swap_list);
	lock_init(&swap_lock);

	d = disk_get(1,1);

	swap_bitmap.count = 0;
	swap_bitmap.max = disk_size(d) / (PGSIZE / DISK_SECTOR_SIZE);
	swap_bitmap.used_map = (int *) malloc (sizeof(int) * swap_bitmap.max);
	memset(swap_bitmap.used_map, 0, swap_bitmap.max);
}



void swap_out(void *kpage, void *upage) {

	size_t remained = PGSIZE;
	disk_sector_t sec_no;

	lock_acquire(&swap_lock);
	int id = swap_bitmap_scan();
	int count = 0;
	
	if (id == -1)
		PANIC("No empty space in swap disk");

	sec_no = id * (PGSIZE / DISK_SECTOR_SIZE);

	struct swap *s = swap_alloc(id, upage);

	while (remained > 0) {
		disk_write(d, sec_no, kpage);
		s->sec_nos[count] = sec_no;
		sec_no++;
		kpage += DISK_SECTOR_SIZE;
		remained -= DISK_SECTOR_SIZE;
		count ++;
	}
	ASSERT(count == 8);

	swap_bitmap.used_map[id] = 1;
	swap_bitmap.count ++;

	lock_release(&swap_lock);

	//swap_print_table();
}

void swap_in(void *kpage, void *upage) {

	struct swap *s = swap_get_by_upage(upage);

	if (s == NULL)
		PANIC("No such user page in swap disk");

	int index, id = s->id;

	for (index = 0; index < 8; index ++) {
		disk_read(d, s->sec_nos[index], kpage);
		kpage += DISK_SECTOR_SIZE;
	}

	swap_free(s);
	swap_bitmap.used_map[id] = 0;
	swap_bitmap.count--;
}

static int swap_bitmap_scan() {
	int *map = swap_bitmap.used_map;
	int i, max = swap_bitmap.max;
	for (i = 0; i < max; i++)
		if (map[i] == 0)
			return i;
	return -1;
}


struct swap *swap_get_by_upage(void *upage) {
	
	tid_t curr_tid = thread_current()->tid;
	struct list_elem *e;
	for (e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
		struct swap *s = list_entry(e, struct swap, elem);
		if (s->tid == curr_tid)
			if (s->upage == upage)
				return s;
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



static struct swap *swap_alloc(int id, void *upage) {

	struct list_elem *e;
	for (e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
		struct swap *s = list_entry(e, struct swap, elem);
		if (s->id == id)
			PANIC("there are same id in swap disk");
	}

	struct swap *s = (struct swap *) malloc (sizeof(struct swap));
	s->id = id;
	s->tid = thread_current()->tid;
	s->upage = upage;

	list_push_back(&swap_list, &s->elem);

	return s;
}

static void swap_free(struct swap *s) {
	lock_acquire(&swap_lock);
	list_remove(&s->elem);
	lock_release(&swap_lock);
	free(s);
}












