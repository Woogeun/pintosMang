#include "vm/page.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/interrupt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"

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
          printf("upage: 0x%8x, position: %6s, writable: %s\n", p->upage, p->position == ON_DISK ? "disk" : p->position == ON_MEMORY ? "memory" : "swap", p->writable ? "writable" : "non-writable");
        }
    }
    printf("==========================================\n");   
    intr_set_level(old_level); 
}

void page_init(void) {
	lock_init(&page_lock);
}

void page_add_hash(void *upage, bool writable, enum page_position position) {
  //lock_acquire(&page_lock);

	struct page *p = (struct page *) malloc (sizeof(struct page));
	p->upage = upage;
	p->writable = writable;
  p->position = position;
	hash_insert(&thread_current()->page_hash, &p->elem);

  //lock_release(&page_lock);
}

void page_remove_hash(void *upage) {
	struct page *p = page_get_by_upage(thread_current(), upage);
	if (p == NULL)
		PANIC("no such page in current thread");
	hash_delete(&thread_current()->page_hash, &p->elem);
	free(p);
}

struct page *page_get_by_upage(struct thread *t, void *upage) {
  //lock_acquire(&page_lock);

  struct page p;
  p.upage = upage;
  ASSERT(&t->page_hash != NULL);
  struct hash_elem *he = hash_find(&t->page_hash, &p.elem);

  if (he == NULL){
    
    //printf("<< no such page : 0x%8x >>\n", upage);

    //lock_release(&page_lock);
    return NULL;
  }

  //lock_release(&page_lock);

  return hash_entry(he, struct page, elem);
  
}

bool page_load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
  
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
    lock_acquire(&page_lock);

    /* Do calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    	size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory. */
      struct frame *f = frame_get_page (PAL_USER, upage);

    /* Load this page. */
    	if (file_read (file, f->kpage, page_read_bytes) != (int) page_read_bytes) {
        	frame_free_page(f);
        	return false; 
      }

    memset (f->kpage + page_read_bytes, 0, page_zero_bytes);

    /* Add the page to the process's address space. */
    	if (!install_page (f->upage, f->kpage, writable)) {
        	frame_free_page(f);
        	return false; 
      }
      
    /* Advance. */
    	read_bytes -= page_read_bytes;
    	zero_bytes -= page_zero_bytes;
    	upage += PGSIZE;

      
      page_add_hash(f->upage, writable, ON_MEMORY);
      lock_release(&page_lock);
  }

	return true;
}

bool page_setup_stack (void **esp, int argc, char **argv) 
{
  lock_acquire(&page_lock);

  struct frame *f = frame_get_page(PAL_USER | PAL_ZERO, STACK_INITIAL_UPAGE);
  bool success = false;

  success = install_page (STACK_INITIAL_UPAGE, f->kpage, true);
  if (success)
    *esp = PHYS_BASE;
  else {
    //palloc_free_page (kpage);
    frame_free_page(f);
    return false;
  }
  
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

  

  page_add_hash(STACK_INITIAL_UPAGE, true, ON_MEMORY);
  lock_release(&page_lock);
  return success;
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

void page_fault_handler(void *esp, void *fault_addr, bool write, bool user, bool not_present) {

  struct thread *curr = thread_current();
  struct page *p = page_get_by_upage(curr, pg_round_down(fault_addr));

  // invalid access detection
  if (!not_present){
    if (!p->writable && write) {
      //printf("You tried to write to non-writable page\n");
      exit(-1);
    }
    //printf("<< present invalid access 0x%8x >>\n", p->upage);  
    exit(-1);
  }
  if (!user){
    //printf("<< !user invalid access 0x%8x >>\n", p->upage);
    exit(-1);
  }

  // page fault handler

  if (p != NULL) {
    if (p->position == ON_MEMORY){
      //page_print_table();
      //printf("<< faulted address : 0x%8x >>\n", p->upage);
      PANIC("ON_MEMORY error");
    }
    
    if (p->position == ON_SWAP) {                             // if page is on swap disk, alloc new page and link them together
      page_load_from_swap(p);
    }

  } else if (is_stack_growth(esp, fault_addr)) {              // stack_growth : 
    page_grow_stack(fault_addr);
  } else {                                                    // bad stack grow                
    //printf("<< invalid stack growth >>\n");
    //printf(" esp: 0x%8x\naddr: 0x%8x\n", esp, fault_addr);
    exit(-1);
  }
}

static bool is_stack_growth(void *esp, void *fault_addr) {
  bool compare = fault_addr - esp + 32 >= 0;                  // check valid address difference
  bool result = compare && fault_addr >= UPAGE_BOTTOM;        // check address is user address
  return result;
}

void page_grow_stack(void *fault_addr) {
  lock_acquire(&page_lock);

  struct frame *f = frame_get_page(PAL_USER | PAL_ZERO, pg_round_down(fault_addr));
  if (!install_page(f->upage, f->kpage, true)) {
    PANIC("stack growth failure");
  }
  page_add_hash(f->upage, true, ON_MEMORY);

  lock_release(&page_lock);
}

void page_load_from_swap(struct page *p) {
  lock_acquire(&page_lock);

  struct frame *f = frame_get_page(PAL_USER, p->upage);
  ASSERT(p->position == ON_SWAP);
  swap_in(f);
        
  if (!install_page(p->upage, f->kpage, p->writable))
    PANIC("install fail!!! why??");
  
  p->position = ON_MEMORY;

  lock_release(&page_lock);
}




















