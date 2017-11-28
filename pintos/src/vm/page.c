#include "vm/page.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/interrupt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

#include "vm/frame.h"
#include "vm/swap.h"
#include <debug.h>
#include <hash.h>
#include <string.h>
#include <stdio.h>



static bool is_stack_growth(void *, void *);

void page_print_table(void) {

  enum intr_level old_level = intr_disable();

  struct hash *h = &thread_current()->page_hash;

  printf("=============== page table %d ==============\n", thread_current()->tid);
  size_t i;
    for (i = 0; i < h->bucket_cnt; i++) {
        struct list *bucket = &h->buckets[i];
        struct list_elem *elem, *next;

        for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
          next = list_next(elem);
          struct hash_elem *he = list_entry(elem, struct hash_elem, list_elem);
          struct page *p = hash_entry(he, struct page, elem);
          page_print(p);
        }
    }
    printf("==========================================\n");   
    intr_set_level(old_level); 
}

void page_print(struct page *p) {
  printf("upage: 0x%8x, position: %6s, writable: %s\n", (unsigned) p->upage, p->position == ON_DISK ? "disk" : p->position == ON_MEMORY ? "memory" : "swap", p->writable ? "writable" : "non-writable");
}

void page_init(void) {
	lock_init(&page_lock);
}

struct page *page_add_hash(void *upage, bool writable, struct file *file, uint32_t page_read_bytes, uint32_t page_zero_bytes, off_t ofs) {

	struct page *p = (struct page *) malloc (sizeof(struct page));
	p->upage = upage;
	p->writable = writable;
  p->position = ON_DISK;

  struct disk_info *d_info = &p->d_info;
  d_info->file = file;
  d_info->page_read_bytes = page_read_bytes;
  d_info->page_zero_bytes = page_zero_bytes;
  d_info->ofs = ofs;

	hash_insert(&thread_current()->page_hash, &p->elem);

  return p;
}

void page_remove_hash(struct page *p) {

	hash_delete(&thread_current()->page_hash, &p->elem);
	free(p);

}

struct page *page_get_by_upage(struct thread *t, void *upage) {

  struct page p;
  p.upage = upage;
  ASSERT(&t->page_hash != NULL);
  struct hash_elem *he = hash_find(&t->page_hash, &p.elem);

  if (he == NULL){
    return NULL;
  }

  return hash_entry(he, struct page, elem);
  
}

bool page_load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
  
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

  lock_acquire(&filesys_lock);
	file_seek (file, ofs);
  lock_release(&filesys_lock);

	while (read_bytes > 0 || zero_bytes > 0) {

  	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
  	size_t page_zero_bytes = PGSIZE - page_read_bytes;

    page_add_hash(upage, writable, file, page_read_bytes, page_zero_bytes, ofs);

  	read_bytes -= page_read_bytes;
  	zero_bytes -= page_zero_bytes;
  	upage += PGSIZE;
    ofs += PGSIZE;
  }

	return true;
}

bool page_setup_stack (void **esp, int argc, char **argv) 
{

  lock_acquire(&page_lock);
  if (!page_grow_stack(STACK_INITIAL_UPAGE, STACK_INITIAL_UPAGE)) {
    lock_release(&page_lock);
    return false;
  }
  lock_release(&page_lock);
  
  *esp = PHYS_BASE;
  int i;
  size_t len;
  void **addresses = (void **) malloc(sizeof(void *) * argc);

  //add argv to stack reversely
  for (i = argc-1; i >= 0; i--) {
    len = strlen(argv[i]);
    *esp -= len + 1; 
    addresses[i] = memcpy(*esp, argv[i], len + 1);
  }

  //align set zero
  int align = (int) *esp % 4;
  if (align < 0)
    align += 4;
  *esp -= align; 
  memset(*esp, 0, align);

  //set imaginary argument
  *esp -= sizeof(int); 
  memset(*esp, 0, sizeof(int));

  //add argv`s address to the stack 
  for (i = argc - 1; i >= 0; i--) {
    *esp -= sizeof(int); 
    memcpy(*esp, &addresses[i], sizeof(int));
  }

  //push argv address
  int argv_address = (int) *esp;
  *esp -= sizeof(int); 
  memcpy(*esp, &argv_address, sizeof(int));

  //push argc
  *esp -= sizeof(int); 
  memcpy(*esp, &argc, sizeof(int));

  //push return address zero
  *esp -= sizeof(int); 
  memset(*esp, 0, sizeof(int));

  free(addresses);

  return true;
}

bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void page_fault_handler(void *esp, void *fault_addr, bool write, bool user UNUSED, bool not_present) {

  // invalid access detection
  if (!not_present){
    exit(-1);
  }

  // page fault handler
  lock_acquire(&page_lock);
  struct page *p = page_get_by_upage(thread_current(), pg_round_down(fault_addr));
  
  if (p != NULL) {
    ASSERT(p->position != ON_MEMORY);
    if (p->position == ON_SWAP) {                             // if page is on swap disk, alloc new page and link them together
      page_load_from_swap(p);
    } else {
      page_load_from_disk(p);
    }
  } 
  else if (!page_grow_stack(esp, fault_addr)){
    lock_release(&page_lock);
    exit(-1);
  }
  lock_release(&page_lock);
}

static bool is_stack_growth(void *esp, void *fault_addr) {
  int diff = fault_addr - esp;
  bool upper_bound = diff < STACK_MAX_SIZE;                                             // check max stack size
  bool under_bound = diff + 32 >= 0;                                                    // check valid address difference
  bool result = upper_bound && under_bound && ((unsigned) fault_addr >= UPAGE_BOTTOM);  // check address is user address
  // printf("<< fault_addr : 0x%8x, esp : 0x%8x, diff : %d -> %s >>\n", 
  //   fault_addr, esp, fault_addr - esp, result ? "grow!" : "not grow!");
  return result;
}

bool page_grow_stack(void *esp, void *fault_addr) {

  bool success;
  
  if (is_stack_growth(esp, fault_addr)) {
    
    struct frame *f = frame_get_page(PAL_USER, pg_round_down(fault_addr));
    if (!install_page(f->upage, f->kpage, true)) {
      PANIC("stack growth failure");
    }
    struct page *p = page_add_hash(f->upage, true, NULL, 0, 0, 0);

    p->position = ON_MEMORY;
    
    success = true;
  } else {
    success = false;
  }

  return success;
}

void page_load_from_swap(struct page *p) {

  ASSERT(p->position == ON_SWAP);
  struct frame *f = frame_get_page(PAL_USER, p->upage);
  swap_in(f);
        
  if (!install_page(p->upage, f->kpage, p->writable))
    PANIC("install fail!!! why??");
  
  p->position = ON_MEMORY;
}

void page_load_from_disk(struct page *p) {

  ASSERT(p->position == ON_DISK);
  struct frame *f = frame_get_page(PAL_USER, p->upage);
  struct disk_info *d_info = &p->d_info;

  ASSERT(d_info->file != NULL);
  
  lock_acquire(&filesys_lock);
  if (file_read_at (d_info->file, f->kpage, d_info->page_read_bytes, d_info->ofs) != (int) d_info->page_read_bytes) {
    PANIC("file read didn`t read all the bytes");
  }
  lock_release(&filesys_lock);
  
  memset (f->kpage + d_info->page_read_bytes, 0, d_info->page_zero_bytes);

  if (!install_page(p->upage, f->kpage, p->writable)) {
    PANIC("install fail!!! why?");
  }

  p->position = ON_MEMORY;
}

void page_to_swap(struct page *p) {

  page_load_from_disk(p);

  struct frame *f = frame_get_by_upage(p->upage);
  ASSERT(f != NULL);

  swap_out(f);
  p->position = ON_SWAP;
  pagedir_clear_page(f->thread->pagedir, f->upage);
  palloc_free_page(f->kpage);
  list_remove(&f->elem);
  free(f);
}

void page_to_disk(struct page *p) {

  struct disk_info *d_info = &p->d_info;
  struct file *file = d_info->file;
  size_t size = d_info->page_read_bytes;
  off_t ofs = d_info->ofs;

  if (p->position == ON_SWAP) {
    page_load_from_swap(p);
  }

  ASSERT(p->position == ON_MEMORY);
  struct frame *f = frame_get_by_upage(p->upage);
  ASSERT(f != NULL);

  if (pagedir_is_dirty(thread_current()->pagedir, p->upage)){
    lock_acquire(&filesys_lock);  
    file_write_at(file, f->kpage, size, ofs);
    lock_release(&filesys_lock);
  }

  frame_free_page(f);
  page_remove_hash(p);
}














