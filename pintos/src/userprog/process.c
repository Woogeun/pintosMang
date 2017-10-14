#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

static struct child_info *create_child_info(void);
static bool valid_wait_tid(tid_t);
static struct wait_info *find_wait_info_by_child_tid(tid_t);
static struct child_info *find_child_info_by_tid(tid_t);
static struct wait_info *create_wait_info(void);
static void awake_wait_thread(int);
static void free_child_list(void);
static void free_file_list(void);
//void remove_file_info(struct file_info *);

//struct list wait_list;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  //extract function name only
  size_t fn_len = strlen(file_name);
  char *fn = (char *) malloc (sizeof(char) * (fn_len + 1));
  char *tmp;
  memset(fn, 0, sizeof(char) * (fn_len + 1));
  strlcpy(fn, file_name, fn_len + 1);
  fn = strtok_r(fn, " ", &tmp);

  //set child process information
  struct thread *curr = thread_current();
  struct child_info *child = create_child_info();
  struct wait_info *l_w_info = create_wait_info();

  tid = thread_create (fn, PRI_DEFAULT, start_process, fn_copy);
  child->tid = tid;

  l_w_info->waiter_thread = curr;
  l_w_info->waitee_tid = tid;
  l_w_info->status = 0;

  list_push_back(&curr->child_list, &child->elem);
  list_push_back(&load_wait_list, &l_w_info->elem);
  

  printf("<<1>>\n");
  while(l_w_info->status == 0)
    thread_yield();
  printf("<<2>>\n");

  list_remove(&l_w_info->elem);
  free(l_w_info);
  free(fn);

  printf("<<3>>\n");
  if (l_w_info->status == -1){
    list_remove(&child->elem);
    free(child);
    printf("<<5>>\n");
    tid = -1;

  } else {
    printf("<<4>>\n");
  /* Create a new thread to execute FILE_NAME. */
    if (tid == TID_ERROR)
      palloc_free_page (fn_copy); 
  }

  return tid;
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *f_name)
{
  printf("<<6>>\n");
  char *file_name = f_name;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */

  printf("<<7>>\n");
  struct wait_info *l_w_info = find_wait_info_by_child_tid(thread_current()->tid);
  palloc_free_page (file_name);
  if (!success) {
    printf("<<8>>\n");
    l_w_info->status = -1;
    process_exit (-1);
  } else {
    printf("<<9>>\n");
    l_w_info->status = 1;
    thread_yield();
    //thread_current()->loaded = 1;
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t tid) 
{
  //while(true);
  //return -1;

  if (!valid_wait_tid(tid)) 
    return -1;

  enum intr_level old_level;
  old_level = intr_disable();

  struct thread *curr = thread_current();
  struct child_info *c_info = find_child_info_by_tid(tid);
  struct wait_info *w_info = create_wait_info();
  int status;

  w_info->waiter_thread = curr;
  w_info->waitee_tid = tid;
  w_info->status = -2;

  list_push_back(&wait_list, &w_info->elem);
  thread_block();

  status = w_info->status;

  list_remove(&c_info->elem);
  free(c_info);
  list_remove(&w_info->elem);
  free(w_info);

  intr_set_level(old_level);
  

  return status;
}

/* Free the current process's resources. */
void
process_exit (int status)
{
  struct thread *curr = thread_current();
  char *file_name = curr->name;

  printf("%s: exit(%d)\n", file_name, status);

  printf("<<11>>\n");
  enum intr_level old_level;
  old_level = intr_disable();

  //return to kernel
  awake_wait_thread(status);
  printf("<<12>>\n");
  //free all of the child and files
  free_child_list();
  free_file_list();
  printf("<<13>>\n");
  intr_set_level(old_level);

  //thread_exit();

  //original code
  //struct thread *curr = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = curr->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      curr->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
    printf("<<14>>\n");
    //intr_set_level(old_level);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, int argc, char **argv);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  //parse file_name
  size_t fn_len = strlen(file_name);
  char *fn = (char *) malloc (sizeof(char) * (fn_len + 1));
  strlcpy(fn, file_name, fn_len + 1);

  //count the arguments
  int argc = 0;
  char *ret, *tmp;
  ret = strtok_r(fn, " ", &tmp);
  while(ret != NULL) {
    argc++;
    ret = strtok_r(NULL, " ", &tmp);
  }

  strlcpy(fn, file_name, fn_len + 1);

  //store the arguments
  char **argv = (char **) malloc(sizeof(char *) * argc);

  ret = strtok_r(fn, " ", &tmp);
  for (i = 0; i < argc; i++) {
    argv[i] = ret;
    ret = strtok_r(NULL, " ", &tmp);
  }

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (argv[0]);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", argv[0]);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, argc, argv))
    goto done;
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);

  free(fn);
  free(argv);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, int argc, char **argv) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

  int i;
  size_t len;
  void **addresses = (void **) malloc(sizeof(void *) * argc);

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

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}






////////////////////////////////////////////////////
static struct child_info *create_child_info() {
  struct child_info *ptr = (struct child_info *) malloc (sizeof(struct child_info));
  return ptr;
}

static bool valid_wait_tid(tid_t tid) {
  struct thread *curr = thread_current();
  struct list *child_list = &curr->child_list;
  struct list_elem *e;
  for (e = list_begin(child_list); e != list_end(child_list); e = list_next(e)) {
    struct child_info *child = list_entry(e, struct child_info, elem);
    if (tid == child->tid)
      return true;
  }
  return false;
}

static struct wait_info *find_wait_info_by_child_tid(tid_t tid) {
  struct list_elem *e;
  for (e = list_begin(&load_wait_list); e != list_end(&load_wait_list); e = list_next(e)) {
    struct wait_info *l_w_info = list_entry(e, struct wait_info, elem);
    if (l_w_info->waitee_tid == tid) return l_w_info;
  }
  return NULL;
}

static struct child_info *find_child_info_by_tid(tid_t tid) {
  struct thread *curr = thread_current();
  struct list *child_list = &curr->child_list;
  struct list_elem *e;
  for (e = list_begin(child_list); e != list_end(child_list); e = list_next(e)) {
    struct child_info *c_info = list_entry(e, struct child_info, elem);
    if (c_info->tid == tid) return c_info;
  }
  return NULL;
}

static struct wait_info *create_wait_info() {
  struct wait_info *ptr = (struct wait_info *) malloc (sizeof(struct wait_info));
  return ptr;
}

static void awake_wait_thread(int status) {
  struct list_elem *e;
  tid_t curr_tid = thread_current()->tid;

  for (e = list_begin(&wait_list); e != list_end(&wait_list); ) {
    struct wait_info *w_info = list_entry(e, struct wait_info, elem);
    if (w_info->waitee_tid == curr_tid) {
      w_info->status = status;
      thread_unblock(w_info->waiter_thread);
      list_remove(&w_info->elem);
    }
    e = list_next(e);
  }
}

static void free_child_list() {
  struct thread *curr = thread_current();
  struct list *child_list = &curr->child_list;
  
  while(!list_empty(child_list)) {
    struct list_elem *e = list_pop_front(child_list);
    struct child_info *c_info = list_entry(e, struct child_info, elem);
    free(c_info);
  }
}

static void free_file_list() {
  struct thread *curr = thread_current();
  struct list *file_list = &curr->file_list;

  while(!list_empty(file_list)) {
    struct list_elem *e = list_pop_front(file_list);
    struct file_info *f_info = list_entry(e, struct file_info, elem);
    free(f_info);
  }
}


















