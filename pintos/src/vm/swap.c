#include "vm/swap.h"

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "devices/disk.h"
#include <debug.h>
#include <stdio.h>

struct bitmap swap_bitmap;
struct disk *d;

static void swap_print_table(void);
static struct swap *swap_alloc(int, void *);

void swap_init(void) {

	list_init(&swap_list);
	lock_init(&swap_lock);

	d = disk_get(1,1);
	max_disk_size = disk_size(d);
	number_of_page = PGSIZE / DISK_SECTOR_SIZE;

	swap_bitmap.count = 0;
	swap_bitmap.max = max_disk_size / number_of_page;
	swap_bitmap.used_map = (int *) malloc (sizeof(int) * swap_bitmap.max);
	memset(swap_bitmap.used_map, 0, swap_bitmap.max);
}



void swap_out(void *upage) {

	size_t remained = PGSIZE;
	disk_sector_t sec_no;

	lock_acquire(&swap_lock);
	int index = swap_bitmap_scan();
	int count = 0;
	
	if (index == -1)
		PANIC("No empty space in swap disk");

	sec_no = index * number_of_page;

	struct swap *s = swap_alloc(index, upage);

	void *upage_offs = upage;

	while (remained > 0) {
		disk_write(d, sec_no, upage_offs);
		s->sec_nos[count] = sec_no;
		sec_no++;
		upage_offs += DISK_SECTOR_SIZE;
		remained -= DISK_SECTOR_SIZE;
		count ++;
	}
	ASSERT(count == 8);

	swap_bitmap.used_map[index] = 1;
	swap_bitmap.count ++;

	lock_release(&swap_lock);

	//swap_print_table();
}

void swap_in(void *upage, void *dest) {

	struct swap *s = swap_get_by_upage(upage);

	if (s == NULL)
		PANIC("No such user page in swap disk");

	// copy data
	//disk_read(d, );
	// remove list
	// bitmap update
	
}

int swap_bitmap_scan() {
	int *map = swap_bitmap.used_map;
	int i, max = swap_bitmap.max;
	for (i = 0; i < max; i++)
		if (map[i] == 0)
			return i;
	return -1;
}

// void swap_add_list(struct list *slot_list, void *upage, disk_sector_t sec_no) {
// 	struct swap_slot *s = (struct swap_slot *) malloc (sizeof(struct swap_slot));
// 	s->tid = thread_current()->tid;
// 	s->upage = upage;
// 	s->sec_no = sec_no;
// 	list_push_back(slot_list, &s->elem);
// }

// void swap_remove_list() {

// }

struct swap *swap_get_by_upage(void *upage) {
	
	return NULL;
}


static void swap_print_table() {
	struct list_elem *e;
	printf("====================== swap table ====================\n");
	for (e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
		struct swap *s = list_entry(e, struct swap, elem);
		printf("id: %2d, tid: %2d, upage: 0x%8x\n", s->id, s->tid, s->upage);
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














