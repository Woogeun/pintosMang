#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include <hash.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/init.h" 
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "devices/input.h"



static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
	lock_init(&filesys_lock);
	lock_init(&free_lock);
	lock_init(&validation_lock);
	lock_init(&mmap_lock);
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

#define arg1 return_args(f, 1)
#define arg2 return_args(f, 2)
#define arg3 return_args(f, 3)

static void
syscall_handler (struct intr_frame *f) 
{
  switch(*(int *) f->esp) {
  	case SYS_HALT:		halt(); 											break;
  	case SYS_EXIT:		exit(arg1); 										break;
  	case SYS_EXEC:		f->eax = exec((char *) arg1, f->esp);				break;
  	case SYS_WAIT:		f->eax = wait((tid_t) arg1); 						break;
  	case SYS_CREATE:	f->eax = create((char *) arg1, arg2, f->esp); 		break;
  	case SYS_REMOVE:	f->eax = remove((char *) arg1, f->esp); 			break;
  	case SYS_OPEN:		f->eax = open((char *) arg1, f->esp); 				break;
  	case SYS_FILESIZE:	f->eax = filesize(arg1); 							break;
  	case SYS_READ:		f->eax = read(arg1, (char *) arg2, arg3, f->esp); 	break;
  	case SYS_WRITE:		f->eax = write(arg1, (char *) arg2, arg3, f->esp); 	break;
  	case SYS_SEEK:		seek(arg1, arg2); 									break;
  	case SYS_TELL:		f->eax = tell(arg1); 								break;
  	case SYS_CLOSE:		close(arg1); 										break;
  	case SYS_MMAP:		f->eax = mmap(arg1, (void *) arg2, f->esp);			break;
  	case SYS_MUNMAP:	munmap(arg1);										break;
  	default:																break;
  	

  }
}

void halt() {
	power_off();
}

void exit(int status) {

	struct thread *curr = thread_current();

	char *file_name = curr->name;

	printf("%s: exit(%d)\n", file_name, status);

	if (filesys_lock.holder == curr)
		lock_release(&filesys_lock);

	free_child_list();
  	free_file_list();
  	free_page();

  	awake_wait_thread(status);

  	struct file *file = curr->file;
  
  	if (file != NULL) {
  		lock_acquire(&filesys_lock);
  		file_allow_write(file);
  		lock_release(&filesys_lock);
  	}

  	remove_thread(curr->tid);

  	ASSERT(list_empty(&curr->child_list));
  	ASSERT(list_empty(&curr->file_list));

	thread_exit();
}

tid_t exec(const char *cmd_line, void *esp) {
	lock_acquire(&filesys_lock);

	if (!valid_address((void *) cmd_line, esp)) {
		lock_release(&filesys_lock);
		exit(-1);
	}

	lock_release(&filesys_lock);
	tid_t tid = process_execute(cmd_line);
	return tid;
}

int wait(tid_t tid) {
	//printf("<<wait>>\n");
	int status = process_wait(tid);
	return status;
}

bool create(const char *file, unsigned size, void *esp) {
	lock_acquire(&filesys_lock);

	if (!valid_address((void *) file, esp)) {
		lock_release(&filesys_lock);
		exit(-1);
	}

	if (strlen(file) == 0) {
		lock_release(&filesys_lock);
		exit(-1);
	}

	bool success = false;

	if (file==NULL)	{
		lock_release(&filesys_lock);
		exit(-1);
	}
	if (strlen(file) > 14) {
		lock_release(&filesys_lock);
		return false; 
	}

	lock_release(&filesys_lock);

	lock_acquire(&filesys_lock);
	success = filesys_create(file, size);
	lock_release(&filesys_lock);

	return success;
}

bool remove(const char *file, void *esp) {
	lock_acquire(&filesys_lock);

	if (!valid_address((void *) file, esp)) {
		lock_release(&filesys_lock);
		exit(-1);
	}

	bool success;

	lock_release(&filesys_lock);

	lock_acquire(&filesys_lock);
	success = filesys_remove(file);
	lock_release(&filesys_lock);

	return success;
}

int open(const char *file_name, void *esp) {
	lock_acquire(&validation_lock);

	if (!valid_address((void *) file_name, esp)) {
		lock_release(&validation_lock);
		exit(-1);
	}

	struct thread *curr = thread_current();
	int fd = -1;
	struct file *file;

	lock_release(&validation_lock);

	lock_acquire(&filesys_lock);
	file = filesys_open(file_name);
	
	if (file == NULL){
		lock_release(&filesys_lock);
		return -1;
	}

	struct file_info *f_info = create_file_info();
	fd = get_fd();
	
	f_info->fd = fd;
	f_info->file = file;
	f_info->position = 0;
	
	list_insert_ordered(&curr->file_list, &f_info->elem, &cmp_fd, NULL);

	lock_release(&filesys_lock);
	return fd;
}

int filesize(int fd) {
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL)
		exit(-1);
	int size = 0;
	
	lock_acquire(&filesys_lock);
	size = file_length(f_info->file);
	lock_release(&filesys_lock);

	return size;
}

int read(int fd, char *buffer, size_t size, void *esp) {
	lock_acquire(&validation_lock);

	char *tmp = pg_round_down(buffer);
	while ((unsigned) tmp <= (unsigned) pg_round_down(buffer + size)) {
		if (!valid_address((void *) tmp, esp)){
			lock_release(&validation_lock);
			exit(-1);
		}
		tmp += PGSIZE;
	}
	lock_release(&validation_lock);

	//stdin
	if (fd == 0) {
		lock_acquire(&filesys_lock);
		int size = input_getc();
		lock_release(&filesys_lock);
		return size;
	}

	//stdout
	if (fd == 1) 
		exit(-1);

	//ordinary file
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL) 
		exit(-1);

	lock_acquire(&filesys_lock);
	size = file_read(f_info->file, buffer, size);
	lock_release(&filesys_lock);

	return size;
}

int write(int fd, const char *buffer, size_t size, void *esp) {
	lock_acquire(&validation_lock);

	char *tmp = pg_round_down(buffer);
	while ((unsigned) tmp <= (unsigned) pg_round_down(buffer + size)) {
		if (!valid_address((void *) tmp, esp)){
			lock_release(&validation_lock);
			exit(-1);
		}
		tmp += PGSIZE;
	}
	lock_release(&validation_lock);

	//stdin
	if (fd == 0)
		exit(-1);

	//stdout
	if (fd == 1) {
		lock_acquire(&filesys_lock);
		putbuf(buffer, size);
		lock_release(&filesys_lock);
		return size;
	}

	//ordinary file
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL)
		exit(-1);

	lock_acquire(&filesys_lock);
	size = file_write(f_info->file, buffer, size);
	lock_release(&filesys_lock);

	return size;
}

void seek(int fd, unsigned position) {
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL)
		exit(-1);

	lock_acquire(&filesys_lock);
	file_seek(f_info->file, position);
	lock_release(&filesys_lock);
}

unsigned tell(int fd) {
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL) 
		exit(-1);
	int pos = 0;

	lock_acquire(&filesys_lock);
	pos = file_tell(f_info->file);
	lock_release(&filesys_lock);

	return pos; 
}

void close(int fd) {

	if (fd == 0 || fd == 1) 
		exit(-1);

	struct file_info *f_info = find_file_info_by_fd(fd);

	if (f_info != NULL) {

		lock_acquire(&filesys_lock);
		file_close(f_info->file);
		lock_release(&filesys_lock);

		list_remove(&f_info->elem);
		free(f_info);
	} else {

		exit(-1);
	}
}

int mmap(int fd, void *addr, void *esp UNUSED) {
	// find open file with fd
	struct file_info *f = find_file_info_by_fd(fd);
	if (f == NULL) 
		exit(-1);

	unsigned size = filesize(fd);

	// check address validation
	lock_acquire(&validation_lock);

	if (pg_ofs(addr) != 0) {
		lock_release(&validation_lock);
		//printf("<<addr are not aligned>>\n");
		return -1;
	}

	void *tmp = addr;
	while ((unsigned) tmp <= (unsigned) pg_round_down(addr + size)) {
		if (!valid_mmap_address(tmp)) {
			lock_release(&validation_lock);
			//printf("<<invalid address>>\n");
			return -1;
		}
		tmp += PGSIZE;
	}

	lock_release(&validation_lock);

	// find file size and find page_read_bytes and page_zero_bytes
	uint32_t read_bytes, zero_bytes;
	read_bytes = size;
	zero_bytes = (unsigned) pg_round_up(size) - size;

	ASSERT(pg_ofs((void *) (read_bytes + zero_bytes)) == 0);

	// get new identical mapid
	int mapid = get_mapid();

	// page table add with ofs
	void *upage = addr;
	seek(fd, 0);
	off_t ofs = 0;
	while (read_bytes > 0 || zero_bytes > 0) {
	    lock_acquire(&mmap_lock);

	  	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
	  	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	  	//swap out directly from disk to swap
	    struct page *p = page_add_hash(upage, true, f->file, page_read_bytes, page_zero_bytes, ofs, mapid);
	    page_to_swap(p);

	    /* Advanced */
	  	read_bytes -= page_read_bytes;
	  	zero_bytes -= page_zero_bytes;
	  	upage += PGSIZE;
	    ofs += PGSIZE;
	    
	    lock_release(&mmap_lock);
  	}

	// add to threads mmap_list
	struct mmap_info *m_info = create_mmap_info();
	m_info->mapid = mapid;
	list_insert_ordered(&thread_current()->mmap_list, &m_info->elem, &cmp_mapid, NULL);

	//

	//

	//

	return mapid;
}

void munmap(int mapid UNUSED) {
	// find all such mapid in mmap_list

	// flush into disk only written page

	// free according pages from page table

	// remove mapid from mmap_list

	//

	//

	//

}






















/////////////////////////////////////////////////////////////////////
//My Implementation

int return_args(struct intr_frame *f, int order) {
	int *address = (int *) ((int *)f->esp) + order;
	if (!valid_address(address, f->esp)){
		exit(-1);
	}
	int arg = *address;
	
	return arg;
}


//about validation

bool valid_address(void *address, void *esp) {
	if (!is_user_vaddr(address) || (unsigned) address <= UPAGE_BOTTOM) {
		//printf("<<address first failure>>\n");
		return false;
	}

	struct page *p = page_get_by_upage(thread_current(), pg_round_down(address));
	if (p == NULL){
		//printf("<<address check stack grow>>\n");
		return page_grow_stack(esp, address);
	}

	if (p->position == ON_SWAP){
		//printf("<<address check swap>>\n");
		page_load_from_swap(p);
	} else if (p->position == ON_DISK){
		//printf("[%d] ", thread_current()->tid);
		//page_print(p);
		//printf("<<address check disk\n");
		page_load_from_disk(p);
	} else if (p->position == ON_MEMORY){
		//printf("<<on memory>>\n");
	}

	return true;
}

bool is_child(struct thread *t, tid_t tid) {
	//printf("<<4>>\n");
  struct child_info *c_info = find_child_info_by_tid(t, tid);
  //printf("<<5>>\n");
  if (c_info == NULL) return false;
  return true;
}

bool valid_mmap_address(void *address) {
	if (!is_user_vaddr(address) || (unsigned) address <= UPAGE_BOTTOM) {
		//printf("<<address first failure>>\n");
		return false;
	}

	ASSERT(pg_ofs(address) == 0);
	struct page *p = page_get_by_upage(thread_current(), address);
	if (p != NULL) {
		//printf("<<already such page in table>>\n");
		return false;
	}

	return true;
}


//about wait_info_structure

struct wait_info *create_wait_info() {
  struct wait_info *ptr = (struct wait_info *) malloc (sizeof(struct wait_info));
  return ptr;
}

struct wait_info *find_wait_info_by_child_tid(tid_t tid) {
  	struct list_elem *e;
  	lock_acquire(&wait_lock);
  	for (e = list_begin(&wait_list); e != list_end(&wait_list); e = list_next(e)) {
    	struct wait_info *w_info = list_entry(e, struct wait_info, elem);
    	if (w_info->waitee_tid == tid) {
    		lock_release(&wait_lock);
    		return w_info;
    	}
  	}
  	lock_release(&wait_lock);
  	return NULL;
}

void awake_wait_thread(int status) {
	tid_t curr_tid = thread_current()->tid;

	struct thread *parent = get_parent_by_tid(curr_tid);
	struct wait_info *w_info = find_wait_info_by_child_tid(curr_tid);

	if (parent != NULL) {
		struct child_info *c_info = find_child_info_by_tid(parent, curr_tid);
		c_info->is_exit = true;
		c_info->status = status;
	}

	if (w_info != NULL) {
		w_info->status = status;
		if (w_info->waiter_thread->status == THREAD_BLOCKED)
			thread_unblock(w_info->waiter_thread);
	}
}

void free_wait_list() {
  while(!list_empty(&wait_list)) {
    struct list_elem *e = list_pop_front(&wait_list);
    struct wait_info *w_info = list_entry(e, struct wait_info, elem);
    free(w_info);
  }
}

void free_thread_list() {
  while(!list_empty(&thread_all_list)) {
    struct list_elem *e = list_pop_front(&thread_all_list);
    struct thread_info *t_info = list_entry(e, struct thread_info, elem);
    free(t_info);
  }
}


//about file_info structure

struct file_info *create_file_info() {
	struct file_info *ptr = (struct file_info *) malloc (sizeof(struct file_info));
	return ptr;
}

struct file_info *find_file_info_by_fd(int fd) {
	lock_acquire(&filesys_lock);

	struct thread *curr = thread_current();
	struct list *file_list = &curr->file_list;
	struct list_elem *e;
	for (e = list_begin(file_list); e != list_end(file_list); e = list_next(e)) {
		struct file_info *f_info = list_entry(e, struct file_info, elem);
		if (f_info->fd == fd) {
			lock_release(&filesys_lock);
			return f_info;
		}
	}
	lock_release(&filesys_lock);
	return NULL;
}

int get_fd() {
	int fd = 2;
	struct list_elem *e;
	struct list *list = &thread_current()->file_list;
	if (list_empty(list)) {
		return fd;
	}
	for (e = list_begin(list); e != list_end(list); e = list_next(e)) {
		struct file_info *f = list_entry(e, struct file_info, elem);
		if (fd != f->fd) break;
		fd++;
	}
	return fd;
}

bool cmp_fd(const struct list_elem *a, const struct list_elem *b, void *aux) {
	struct file_info *f1_info = list_entry(a, struct file_info, elem);
	struct file_info *f2_info = list_entry(b, struct file_info, elem);

	if (f1_info->fd < f2_info->fd) {
		if (aux == NULL) return true;
		return false;
	}
	if (aux == NULL) return false;
	return true;
}

void free_file_list() {
	lock_acquire(&free_lock);

	struct thread *curr = thread_current();
	struct list *file_list = &curr->file_list;
	
	while(!list_empty(file_list)) {
		struct list_elem *e = list_pop_front(file_list);
		struct file_info *f_info = list_entry(e, struct file_info, elem);

		lock_acquire(&filesys_lock);
		file_close(f_info->file);
		lock_release(&filesys_lock);

		list_remove(&f_info->elem);
		free(f_info);
	}

	lock_release(&free_lock);
}


//about child_info structure

struct child_info *create_child_info() {
  struct child_info *ptr = (struct child_info *) malloc (sizeof(struct child_info));
  ptr->tid = -1;
  ptr->is_exit = false;
  ptr->status = -1;
  return ptr;
}

struct child_info *find_child_info_by_tid(struct thread *t, tid_t tid) {

  struct list *child_list = &t->child_list;
  struct list_elem *e;
  
  for (e = list_begin(child_list); e != list_end(child_list); e = list_next(e)) {
    struct child_info *c_info = list_entry(e, struct child_info, elem);
    if (c_info->tid == tid) {
    	
    	return c_info;
    }
  }
  
  return NULL;
}

void free_child_list() {
	lock_acquire(&free_lock);

	struct thread *curr = thread_current();
	struct list *child_list = &curr->child_list;

	while(!list_empty(child_list)) {
		struct list_elem *e = list_pop_front(child_list);
		struct child_info *c_info = list_entry(e, struct child_info, elem);

		list_remove(&c_info->elem);
		free(c_info);
	}

	lock_release(&free_lock);
}

void free_page() {
	lock_acquire(&page_lock);

	struct list_elem *e = list_begin(&frame_list);
	while (e != list_tail(&frame_list)) {
		struct frame *f = list_entry(e, struct frame, elem);
		e = list_next(e);
		struct thread *curr = thread_current();
		if (curr == f->thread){
			//printf("<<match>>\n");
			frame_free_page(f);
		}
	}

	tid_t curr_tid = thread_current()->tid;
	e = list_begin(&swap_list);
	while (e != list_tail(&swap_list)) {
		struct swap *s = list_entry(e, struct swap, elem);
		e = list_next(e);
		if (s->tid == curr_tid)
			swap_free(s);
	}

	lock_release(&page_lock);
}


//about mmap structure
struct mmap_info *create_mmap_info() {
	struct mmap_info *m_info = (struct mmap_info *) malloc (sizeof(struct mmap_info));
	return m_info;
}

int get_mapid() {
	int mapid = 2;
	struct list_elem *e;
	struct list *list = &thread_current()->mmap_list;
	if (list_empty(list)) {
		return mapid;
	}
	for (e = list_begin(list); e != list_end(list); e = list_next(e)) {
		struct mmap_info *m = list_entry(e, struct mmap_info, elem);
		if (mapid != m->mapid) break;
		mapid++;
	}
	return mapid;
}

bool cmp_mapid(const struct list_elem *a, const struct list_elem *b, void *aux) {
	struct mmap_info *m1_info = list_entry(a, struct mmap_info, elem);
	struct mmap_info *m2_info = list_entry(b, struct mmap_info, elem);

	if (m1_info->mapid < m2_info->mapid) {
		if (aux == NULL) return true;
		return false;
	}
	if (aux == NULL) return false;
	return true;
}
















