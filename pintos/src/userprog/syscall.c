#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
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


static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
	lock_init(&filesys_lock);
	lock_init(&free_lock);
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

#define arg1 return_args(f, 1)
#define arg2 return_args(f, 2)
#define arg3 return_args(f, 3)

static void
syscall_handler (struct intr_frame *f) 
{
  switch(*(int *) f->esp) {
  	case SYS_HALT:		halt(); 									break;
  	case SYS_EXIT:		exit(arg1); 								break;
  	case SYS_EXEC:		f->eax = exec((char *) arg1);				break;
  	case SYS_WAIT:		f->eax = wait((tid_t) arg1); 				break;
  	case SYS_CREATE:	f->eax = create((char *) arg1, arg2); 		break;
  	case SYS_REMOVE:	f->eax = remove((char *) arg1); 			break;
  	case SYS_OPEN:		f->eax = open((char *) arg1); 				break;
  	case SYS_FILESIZE:	f->eax = filesize(arg1); 					break;
  	case SYS_READ:		f->eax = read(arg1, (char *) arg2, arg3); 	break;
  	case SYS_WRITE:		f->eax = write(arg1, (char *) arg2, arg3); 	break;
  	case SYS_SEEK:		seek(arg1, arg2); 							break;
  	case SYS_TELL:		f->eax = tell(arg1); 						break;
  	case SYS_CLOSE:		close(arg1); 								break;
  	default:														break;
  }
}

void halt() {
	power_off();
}

void exit(int status) {

	lock_acquire(&free_lock);

	struct thread *curr = thread_current();
	char *file_name = curr->name;

	printf("%s: exit(%d)\n", file_name, status);

	free_child_list();
  	free_file_list();

  	awake_wait_thread(status);

  	struct file *file = curr->file;
  
  	if (file != NULL) {
  		lock_acquire(&filesys_lock);
  		file_allow_write(file);
  		lock_release(&filesys_lock);
  	}

  	ASSERT(list_empty(&curr->child_list));
  	ASSERT(list_empty(&curr->file_list));

  	lock_release(&free_lock);

	thread_exit();
}

tid_t exec(const char *cmd_line) {

	if (!valid_address((void *) cmd_line))
		exit(-1);

	tid_t tid = process_execute(cmd_line);
	return tid;
}

int wait(tid_t tid) {
	int status = process_wait(tid);
	return status;
}

bool create(const char *file, unsigned size) {
	if (!valid_address((void *) file))
		exit(-1);

	if (strlen(file) == 0) 
		exit(-1);
	bool success = false;

	if (file==NULL)	exit(-1);
	if (strlen(file)>14) {
		return false; 
	}

	lock_acquire(&filesys_lock);
	success = filesys_create(file, size);
	lock_release(&filesys_lock);

	return success;
}

bool remove(const char *file) {
	if (!valid_address((void *) file))
		exit(-1);

	bool success;

	lock_acquire(&filesys_lock);
	success = filesys_remove(file);
	lock_release(&filesys_lock);

	return success;
}

int open(const char *file_name) {
	if (!valid_address((void *) file_name))
		exit(-1);

	struct thread *curr = thread_current();
	int fd = -1;
	struct file *file;

	lock_acquire(&filesys_lock);
	file = filesys_open(file_name);
	lock_release(&filesys_lock);
	
	if (file == NULL)
		return -1;

	struct file_info *f_info = create_file_info();
	fd = get_fd(&curr->file_list);
	
	f_info->fd = fd;
	f_info->file = file;
	f_info->position = 0;
	
	list_insert_ordered(&curr->file_list, &f_info->elem, &cmp_fd, NULL);
	
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

int read(int fd, char *buffer, size_t size) {
	if (!valid_address((void *) buffer)) {
		if ((unsigned) buffer < (unsigned) PHYS_BASE) {
			uint32_t *pd = active_pd();
			pagedir_set_accessed(pd, buffer, true);
		}
		else 
			exit(-1);
	}

	//stdin
	if (fd == 0) {
		return input_getc();
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

int write(int fd, const char *buffer, size_t size) {
	if (!valid_address((void *) buffer))
		exit(-1);

	//stdin
	if (fd == 0)
		exit(-1);

	//stdout
	if (fd == 1) {
		putbuf(buffer, size);
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
























/////////////////////////////////////////////////////////////////////
//My Implementation

int return_args(struct intr_frame *f, int order) {
	int *address = (int *) ((int *)f->esp) + order;
	if (!valid_address(address)){
		exit(-1);
	}
	int arg = *address;
	
	return arg;
}


//about validation

bool valid_address(const void *address) {
	uint32_t *pd = active_pd();

	bool valid = pagedir_is_accessed(pd, address);
	if ((unsigned) address > (unsigned) PHYS_BASE) valid = false;
	return valid; 
}

bool is_child(tid_t tid) {
  struct child_info *c_info = find_child_info_by_tid(tid);
  if (c_info == NULL) return false;
  return true;
}


//about wait_info_structure

struct wait_info *create_wait_info() {
  struct wait_info *ptr = (struct wait_info *) malloc (sizeof(struct wait_info));
  return ptr;
}

struct wait_info *find_wait_info_by_child_tid(tid_t tid) {
  struct list_elem *e;
  for (e = list_begin(&wait_list); e != list_end(&wait_list); e = list_next(e)) {
    struct wait_info *w_info = list_entry(e, struct wait_info, elem);
    if (w_info->waitee_tid == tid) return w_info;
  }
  return NULL;
}

void awake_wait_thread(int status) {
	struct list_elem *e;
	tid_t curr_tid = thread_current()->tid;

	enum intr_level old_level;
	old_level = intr_disable();

	for (e = list_begin(&wait_list); e != list_end(&wait_list); e = list_next(e)) {
		struct wait_info *w_info = list_entry(e, struct wait_info, elem);
		if (w_info->waitee_tid == curr_tid) {
			w_info->status = status;
			if (w_info->is_running == WAIT_RUNNING) w_info->is_running = WAIT_FINISHED;
			if (w_info->waiter_thread->status == THREAD_BLOCKED)
				thread_unblock(w_info->waiter_thread);
		}
	}

	intr_set_level(old_level);
}

void free_wait_list() {
  while(!list_empty(&wait_list)) {
    struct list_elem *e = list_pop_front(&wait_list);
    struct wait_info *w_info = list_entry(e, struct wait_info, elem);
    free(w_info);
  }
}


//about file_info structure

struct file_info *create_file_info() {
	struct file_info *ptr = (struct file_info *) malloc (sizeof(struct file_info));
	return ptr;
}

struct file_info *find_file_info_by_fd(int fd) {
	struct thread *curr = thread_current();
	struct list *file_list = &curr->file_list;
	struct list_elem *e;
	for (e = list_begin(file_list); e != list_end(file_list); e = list_next(e)) {
		struct file_info *f_info = list_entry(e, struct file_info, elem);
		if (f_info->fd == fd) return f_info;
	}
	return NULL;
}

int get_fd(struct list *list) {
	int fd = 2;
	struct list_elem *e;
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
}


//about child_info structure

struct child_info *create_child_info() {
  struct child_info *ptr = (struct child_info *) malloc (sizeof(struct child_info));
  return ptr;
}

struct child_info *find_child_info_by_tid(tid_t tid) {
  struct thread *curr = thread_current();
  struct list *child_list = &curr->child_list;
  struct list_elem *e;
  for (e = list_begin(child_list); e != list_end(child_list); e = list_next(e)) {
    struct child_info *c_info = list_entry(e, struct child_info, elem);
    if (c_info->tid == tid) return c_info;
  }
  return NULL;
}

void free_child_list() {
	struct thread *curr = thread_current();
	struct list *child_list = &curr->child_list;

	while(!list_empty(child_list)) {
		struct list_elem *e = list_pop_front(child_list);
		struct child_info *c_info = list_entry(e, struct child_info, elem);

		list_remove(&c_info->elem);
		free(c_info);
	}
}




