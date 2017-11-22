#include "vm/swap.h"

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/disk.h"
#include <debug.h>
#include <hash.h>
#include <stdio.h>

static unsigned hash_func (const struct hash_elem *, void *);
static bool less_func (const struct hash_elem *, const struct hash_elem *, void *);

static unsigned hash_func (const struct hash_elem *e, void *aux UNUSED) {

	struct swap *s = hash_entry(e, struct swap, elem);
	return hash_int((int) s->upage);
}

static bool less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){

	struct swap *cmp1 = hash_entry(a, struct swap, elem);
	struct swap *cmp2 = hash_entry(b, struct swap, elem);
	return cmp1->upage < cmp2->upage;
}

void swap_init(void) {
	hash_init(&swap_hash, hash_func, less_func, NULL);
	lock_init(&swap_lock);

	max_disk_size = disk_size(disk_get(1, 1));
	sec_no = 0;
	cnt = 0;

	//printf("max_disk_size: %d\n", max_disk_size); -> 8064
}

void swap_out(void *upage) {

	//printf("used_disk_size: %d\n", used_disk_size);
	//printf("<<1>>\n");
	//void *tmp = upage;
	struct disk *d = disk_get(1, 1);
	//disk_sector_t sec_no = 512;
	//printf("sec_no: %d\n", sec_no);
	size_t remained = PGSIZE;
	// while (remained > 0) {
	// 	disk_write(d, sec_no, tmp);
	// 	tmp += sec_no;
	// 	remained -= sec_no;
	// 	used_disk_size += sec_no;
	// }
	lock_acquire(&swap_lock);

	while (remained > 0) {
		disk_write(d, sec_no, upage);
		swap_add_hash(upage);
		sec_no ++;
		upage += DISK_SECTOR_SIZE;
		remained -= DISK_SECTOR_SIZE;
	}
	lock_release(&swap_lock);
	//printf("sec_no: %d\n", sec_no);
	//printf("<<2>>\n");
	//printf("swap_list_count: %d\n", cnt); 
}

void *swap_in() {
	return NULL;
}

void swap_add_hash(void *upage) {
	struct swap *s = (struct swap *) malloc (sizeof(struct swap));
	s->tid = thread_current()->tid;
	s->upage = upage;
	s->sec_no = sec_no;
	hash_insert(&swap_hash, &s->elem);
	cnt++;
}

void swap_remove_hash() {

}

struct swap *swap_get_by_upage(void *upage) {
	
	return NULL;
}

