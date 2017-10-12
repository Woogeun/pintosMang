#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h" 
#include "filesys/filesys.h"
#include "threads/synch.h"
#include <string.h>

// arg number check

static void syscall_handler (struct intr_frame *);

static void halt(void);						//done
static void exit(int);		
static int exec(const char *);					
static int wait(int);					
static bool create(const char *, size_t);	//done					
static bool remove(const char *);			//done			
static int open(const char *);						
static int filesize(int);				
static int read(int, char *, size_t);						
static int write(int, char *, size_t);		
static void seek(int, unsigned);			
static unsigned tell(int);			
static void close(int);		

static int return_args(struct intr_frame *, int);
static int get_fd(struct list *);
static bool cmp_fd(const struct list_elem *, const struct list_elem *, void *);
static struct list_elem *find_elem_by_fd(struct list *, int);

static struct list wait_list;
struct lock filesys_lock;

void
syscall_init (void) 
{
	list_init(&wait_list);
	lock_init(&filesys_lock);
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

#define arg1 return_args(f, 1)
#define arg2 return_args(f, 2)
#define arg3 return_args(f, 3)

static void
syscall_handler (struct intr_frame *f) 
{
  switch(*(int *) f->esp) {
  	case SYS_HALT: 		halt(); break;
  	case SYS_EXIT:		exit(arg1); break;
  	case SYS_EXEC:		f->eax = exec((char *) arg1); break;
  	case SYS_WAIT:		f->eax = wait(arg1); break;
  	case SYS_CREATE:	f->eax = create((char *) arg1, arg2); break;
  	case SYS_REMOVE:	f->eax = remove((char *) arg1); break;
  	case SYS_OPEN:		f->eax = open((char *) arg1); break;
  	case SYS_FILESIZE:	f->eax = filesize(arg1); break;
  	case SYS_READ:		f->eax = read(arg1, (char *) arg2, arg3); break;
  	case SYS_WRITE:		f->eax = write(arg1, (char *) arg2, arg3); break;
  	case SYS_SEEK:		seek(arg1, arg2); break;
  	case SYS_TELL:		f->eax = tell(arg1); break;
  	case SYS_CLOSE:		close(arg1); break;
  	default:	break;
  }
  //printf ("system call!: %d\n", *(int *) f->esp);
  //printf(" esp: 0x%x\n", (int *) f->esp);
  //printf("*esp + 4: 0x%x\n", *(((int *) f->esp) + 1));
  //printf("*esp + 8: %s\n", *(((int *) f->esp) + 2));
  //printf("*esp + 12: %d\n", *(((int *) f->esp) + 3));
  //printf("*esp + 16: 0x%x\n", *(((int *) f->esp) + 4));
  //hex_dump((int *) f->esp, (int *) f->esp, 0xc0000000 - (int) f->esp, true);

  //thread_exit ();
}

static void halt() {
	power_off();
}

static void exit(int status) {
	
	char *file_name = thread_current()->name;
	printf("%s: exit(%d)\n",file_name,status);

	thread_exit();

	//return to kernel
}

static int exec(const char *cmd_line UNUSED) {
	//char *cmd_line = (char *) return_args(f, 1);
	return 0;
}

static int wait(int pid UNUSED) {
	return 0;
}

static bool create(const char *file, unsigned size) {
	bool success = false;

	if (file==NULL)	exit(-1);
	if (strlen(file)>14) {
		return false; //return false;
	}

	lock_acquire(&filesys_lock);
	success = filesys_create(file, size);
	lock_release(&filesys_lock);

	return success;
}

static bool remove(const char *file) {
	bool success;

	lock_acquire(&filesys_lock);
	success = filesys_remove(file);
	lock_release(&filesys_lock);

	return success;
}

static int open(const char *file_name) {
	struct thread *curr = thread_current();
	int fd = -1;
	struct file *file;

	lock_acquire(&filesys_lock);
	file = filesys_open(file_name);
	lock_release(&filesys_lock);

	if (file == NULL) return -1;

	fd = get_fd(&curr->file_list);
	struct file_info *f_info = NULL;
	f_info->fd = fd;
	f_info->file = file;
	f_info->position = 0;
	
	list_insert_ordered(&curr->file_list, &f_info->elem, &cmp_fd, NULL);
	
	return fd;

}

static int filesize(int fd UNUSED) {
	return 0;
}

static int read(int fd UNUSED, char *buffer UNUSED, size_t size UNUSED) {
	return 0;
}

static int write(int fd, char *buffer, size_t size) {
	if (fd == 1) {
		putbuf(buffer, size);
		return size;
	}
	return size;
}

static void seek(int fd UNUSED, unsigned position UNUSED) {

}

static unsigned tell(int fd UNUSED) {
	return 0;
}

static void close(int fd) {
	struct thread *curr = thread_current();
	struct list *file_list = &curr->file_list;
	struct list_elem *e = find_elem_by_fd(file_list, fd);
	if (e != NULL) list_remove(e);
}

static int return_args(struct intr_frame *f, int order) {
	return (int) *(((int *) f->esp) + order);
}


static int get_fd(struct list *list) {
	int fd = 2;
	struct list_elem *e;
	if (list_empty(list)) return fd;
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
		if (aux != NULL) return true;
		return false;
	}
	if (aux != NULL) return false;
	return true;
}

static struct list_elem *find_elem_by_fd(struct list *list, int fd) {
	struct list_elem *e;
	for (e = list_begin(list); e != list_end(list); e = list_next(e)) {
		struct file_info *f_info = list_entry(e, struct file_info, elem);
		if (f_info->fd == fd) return e;
	}
	return NULL;
}











