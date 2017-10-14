#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h" 
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"


static void syscall_handler (struct intr_frame *);

void halt(void);						//done
void exit(int);						
tid_t exec(const char *);				
int wait(tid_t);					
bool create(const char *, size_t);		//done					
bool remove(const char *);				//maybe done			
int open(const char *);					//done		
int filesize(int);						//maybe done
int read(int, char *, size_t);			//done
int write(int, const char *, size_t);	//done	
void seek(int, unsigned);				//maybe done
unsigned tell(int);						//maybe done
void close(int);						//done

static int return_args(struct intr_frame *, int);
//static bool valid_wait_tid(tid_t);
//static struct wait_info *create_wait_info(void);
//static void remove_wait_info(struct wait_info *);
static void awake_wait_thread(int);
//static struct child_info *create_child_info(void);
//static void remove_child_info(struct child_info *);
static bool valid_address(const void *);
static struct file_info *create_file_info(void);
//static void remove_file_info(struct file_info *);
static int get_fd(struct list *);
static bool cmp_fd(const struct list_elem *, const struct list_elem *, void *);
struct file_info *find_file_info_by_fd(int);
//static struct child_info *find_child_info_by_tid(tid_t);
static void free_child_list(void);
static void free_file_list(void);

//static struct list wait_list;

void
syscall_init (void) 
{
	//list_init(&wait_list);
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
	//printf("syscall!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! %d\n", *(int *) f->esp);
  switch(*(int *) f->esp) {
  	case SYS_HALT: 		halt(); 									break;
  	case SYS_EXIT:		exit(arg1); 								break;
  	case SYS_EXEC:		f->eax = exec((char *) arg1); 				break;
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

	//printf("<<1>>\n");
	lock_acquire(&free_lock);
	//printf("<<2>>\n");

	struct thread *curr = thread_current();
	char *file_name = curr->name;

	printf("%s: exit(%d)\n", file_name, status);

	//printf("<<3>>\n");
	free_child_list();
	//printf("<<3.5>>\n");
  	free_file_list();
  	//printf("<<4>>\n");

  	awake_wait_thread(status);

  	struct file *file = curr->file;
  
  	if (file != NULL) {
  		lock_acquire(&filesys_lock);
  		file_allow_write(file);
  		lock_release(&filesys_lock);
  	}

  	ASSERT(list_empty(&curr->child_list));
  	ASSERT(list_empty(&curr->file_list));

  	//printf("<<5>>\n");
  	lock_release(&free_lock);
  	//printf("<<6>>\n");

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
	lock_init(&f_info->lock);
	
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

int read(int fd UNUSED, char *buffer, size_t size UNUSED) {
	//printf("I`m read syscall-------------------------\n");
	//lock_acquire(&filesys_lock);
	if (!valid_address((void *) buffer)) {
		if ((unsigned) buffer < 0xc0000000) {
			uint32_t *pd = active_pd();
			pagedir_set_dirty(pd, buffer, true);
		}
		else 
			exit(-1);
	}

	//stdin
	if (fd == 0) {
		//lock_release(&filesys_lock);
		return input_getc();
	}

	//stdout
	if (fd == 1) 
		exit(-1);

	
	//ordinary file
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL) 
		exit(-1);

	//lock_acquire(&f_info->lock);
	lock_acquire(&filesys_lock);
	size = file_read(f_info->file, buffer, size);
	lock_release(&filesys_lock);

	return size;
}

int write(int fd, const char *buffer, size_t size) {
	//printf("I`m write syscall-------------------------\n");
	
	if (!valid_address((void *) buffer))
		exit(-1);

	//stdin
	if (fd == 0)
		exit(-1);

	//stdout
	if (fd == 1) {
		putbuf(buffer, size);
		//lock_release(&filesys_lock);
		return size;
	}

	//ordinary file
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL)
		exit(-1);

	//lock_acquire(&f_info->lock);
	lock_acquire(&filesys_lock);
	size = file_write(f_info->file, buffer, size);
	lock_release(&filesys_lock);
	//printf("write lock release================\n");
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
	//printf("I`m close syscall-------------------------\n");
	if (fd == 0 || fd == 1) 
		exit(-1);

	//lock_acquire(&f_info->lock);
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











//////////////////////////
static int return_args(struct intr_frame *f, int order) {
	int *address = ((int *) f->esp) + order;
	if (!valid_address(address)){
		//printf("invalid args==================================\n");
		exit(-1);
	}
	int arg = *address;
	
	return arg;
}

/*
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
*/

static void awake_wait_thread(int status) {
	struct list_elem *e;
	tid_t curr_tid = thread_current()->tid;

	enum intr_level old_level;
	old_level = intr_disable();

	for (e = list_begin(&wait_list); e != list_end(&wait_list); e = list_next(e)) {
		struct wait_info *w_info = list_entry(e, struct wait_info, elem);
		if (w_info->waitee_tid == curr_tid) {
			w_info->status = status;
			//printf("w_info in awake: %d\n", w_info->is_running);
			if (w_info->is_running == WAIT_RUNNING) w_info->is_running = WAIT_FINISHED;
			if (w_info->waiter_thread->status == THREAD_BLOCKED)
				thread_unblock(w_info->waiter_thread);
		}
	}

	intr_set_level(old_level);
}

/*
static struct child_info *create_child_info() {
	struct child_info *ptr = (struct child_info *) malloc (sizeof(struct child_info));
	return ptr;
}

static void remove_child_info(struct child_info *child){
	free(child);
}
*/
static bool valid_address(const void *address) {
	uint32_t *pd = active_pd();

	bool valid = pagedir_is_accessed(pd, address);
	if ((unsigned) address > PHYS_BASE) valid = false;
	//printf("valid address: %x, %d\n", (unsigned) address, valid);
	return valid; 
}

/*
static struct wait_info *create_wait_info() {
	struct wait_info *ptr = (struct wait_info *) malloc (sizeof(struct wait_info));
	return ptr;
}


static void remove_wait_info(struct wait_info *w_info) {
	free(w_info);
}
*/
static struct file_info *create_file_info() {
	struct file_info *ptr = (struct file_info *) malloc (sizeof(struct file_info));
	return ptr;
}
/*
static void remove_file_info(struct file_info * ptr) {
	free(ptr);
}
*/
static int get_fd(struct list *list) {
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

static bool cmp_fd(const struct list_elem *a, const struct list_elem *b, void *aux) {
	struct file_info *f1_info = list_entry(a, struct file_info, elem);
	struct file_info *f2_info = list_entry(b, struct file_info, elem);

	if (f1_info->fd < f2_info->fd) {
		if (aux == NULL) return true;
		return false;
	}
	if (aux == NULL) return false;
	return true;
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
/*
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
*/

static void free_child_list() {
	struct thread *curr = thread_current();
	struct list *child_list = &curr->child_list;

	while(!list_empty(child_list)) {
		struct list_elem *e = list_pop_front(child_list);
		struct child_info *c_info = list_entry(e, struct child_info, elem);

		list_remove(&c_info->elem);
		free(c_info);
	}
}

static void free_file_list() {
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








