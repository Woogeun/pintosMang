#include "vm/page.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/interrupt.h"
#include "userprog/process.h"

#include "vm/frame.h"
#include "vm/swap.h"
#include <debug.h>
#include <hash.h>
#include <string.h>
#include <stdio.h>

static void page_print_table(struct thread *);
static bool is_stack_growth(void *, void *);

static void page_print_table(struct thread *t) {

  enum intr_level old_level = intr_disable();

  printf("===============thread %2d==============\n", t->tid);
  size_t i;
  struct hash *h = &t->page_table;
    for (i = 0; i < h->bucket_cnt; i++) {
        struct list *bucket = &h->buckets[i];
        struct list_elem *elem, *next;

        for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
          next = list_next(elem);
          struct hash_elem *he = list_entry(elem, struct hash_elem, list_elem);
          struct page *p = hash_entry(he, struct page, elem);
          printf("upage: 0x%8x, position: %6s\n", p->upage, p->position == ON_DISK ? "disk" : p->position == ON_MEMORY ? "memory" : "swap");
        }
    }
    printf("======================================%d\n", h->elem_cnt);   
    intr_set_level(old_level); 
}

void page_init(void) {
	
}

void page_add_hash(void *upage, bool writable, enum page_position position) {
	struct page *p = (struct page *) malloc (sizeof(struct page));
	p->upage = upage;
	p->writable = writable;
  p->position = position;
	hash_insert(&thread_current()->page_table, &p->elem);
}

void page_remove_hash(void *upage) {
	struct page *p = page_get_by_upage(upage);
	if (p == NULL)
		PANIC("no such page in current thread");
	hash_delete(&thread_current()->page_table, &p->elem);
	frame_free_page(upage);
	free(p);
}

struct page *page_get_by_upage(void *upage) {
	size_t i;
	struct hash *h = &thread_current()->page_table;
  	for (i = 0; i < h->bucket_cnt; i++) {
       	struct list *bucket = &h->buckets[i];
      	struct list_elem *elem, *next;

      	for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
      		next = list_next(elem);
      		struct hash_elem *he = list_entry(elem, struct hash_elem, list_elem);
      		struct page *p = hash_entry(he, struct page, elem);
          	if (p->upage == upage)
          		return p;
        }
    }    
    return NULL;
}

bool page_load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  	ASSERT (pg_ofs (upage) == 0);
  	ASSERT (ofs % PGSIZE == 0);
    //printf("<<1>>\n");

    //page_print_table(thread_current());

  	file_seek (file, ofs);
  	while (read_bytes > 0 || zero_bytes > 0) {
      //printf("<<2>>\n");
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      	size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      	uint8_t *kpage = frame_get_page (PAL_USER, upage);

      /* Load this page. */
      	if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
          	frame_free_page(kpage);
          	return false; 
        }
      //printf("<<7>>\n");
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      	if (!install_page (upage, kpage, writable)) {
          	frame_free_page(kpage);
          	return false; 
        }
        
        page_add_hash(upage, writable, ON_MEMORY);

      /* Advance. */
      	read_bytes -= page_read_bytes;
      	zero_bytes -= page_zero_bytes;
      	upage += PGSIZE;
    }

    
  	return true;
}

bool page_setup_stack (void **esp, int argc, char **argv) 
{

  uint8_t *kpage;
  bool success = false;

  //kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  kpage = frame_get_page(PAL_USER | PAL_ZERO, STACK_INITIAL_UPAGE);

  success = install_page (STACK_INITIAL_UPAGE, kpage, true);
  if (success)
    *esp = PHYS_BASE;
  else {
    //palloc_free_page (kpage);
    frame_free_page(kpage);
    return false;
  }

  page_add_hash(kpage, true, ON_MEMORY);
  
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

void page_fault_handler(void *esp, void *fault_addr, bool write, bool user) {

  if (is_stack_growth(esp, fault_addr)) {  //stack_growth
    //printf("==========stack_growth!!!!!\n");
    uint8_t *kpage = frame_get_page(PAL_USER, STACK_INITIAL_UPAGE);
    
    if (!install_page(pg_round_down(fault_addr), kpage, write)) {
      frame_free_page(kpage);
      PANIC("stack growth failure");
    }

    page_add_hash(kpage, write, ON_MEMORY);

  } else {                  //swap_in
    //page_print_table(thread_current());

    struct page *p = page_get_by_upage(pg_round_down(fault_addr));
    if (p == NULL) 
      exit(-1);
    //printf("<<p is not NULL>>\n");
    //struct swap *s = swap_get_by_upage(fault_addr);
    //printf("is there in disk?: %s\n", s==NULL ? "no" : "yes");
  }
}

static bool is_stack_growth(void *esp, void *fault_addr) {
  return fault_addr >= esp - 32;
}







