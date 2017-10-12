#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h" 
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include <string.h>
#include <stdlib.h>

//
static void syscall_handler (struct intr_frame *);

static void halt(void);						//done
static void exit(int);						
static int exec(const char *);				
static tid_t wait(tid_t);					
static bool create(const char *, size_t);	//done					
static bool remove(const char *);			//maybe done			
static int open(const char *);				//done		
static int filesize(int);					//maybe done
static int read(int, char *, size_t);		//done
static int write(int, const char *, size_t);//done	
static void seek(int, unsigned);			//maybe done
static unsigned tell(int);					//maybe done
static void close(int);						//done

static int return_args(struct intr_frame *, int);
static bool valid_address(const void *);
static struct file_info *create_file_info(void);
static void remove_file_info(struct file_info *);
static int get_fd(struct list *);
static bool cmp_fd(const struct list_elem *, const struct list_elem *, void *);
static struct file_info *find_file_info_by_fd(int);

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
  	case SYS_WAIT:		f->eax = wait((tid_t) arg1); break;
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

	lock_acquire(&filesys_lock);
	filesys_done();
	lock_release(&filesys_lock);

	thread_exit();

	//return to kernel
}

static int exec(const char *cmd_line) {
	//char *cmd_line = (char *) return_args(f, 1);
	printf("exec!!!!!!!!!!!!!!!-----------------------------\n");
	if (!valid_address((void *) cmd_line))
		exit(-1);

	return 0;
}

static tid_t wait(tid_t tid UNUSED) {
	return 0;
}

static bool create(const char *file, unsigned size) {
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

static bool remove(const char *file) {
	if (!valid_address((void *) file))
		exit(-1);

	bool success;

	lock_acquire(&filesys_lock);
	success = filesys_remove(file);
	lock_release(&filesys_lock);

	return success;
}

static int open(const char *file_name) {
	if (!valid_address((void *) file_name))
		exit(-1);

	struct thread *curr = thread_current();
	int fd = -1;
	struct file *file;

	lock_acquire(&filesys_lock);
	file = filesys_open(file_name);
	lock_release(&filesys_lock);

	//printf("file::::::::::::0x%x\n", file);
	if (file == NULL) {
		//printf("filesys_open fail::::::::::::::::::::\n");
		return -1;
	}

	struct file_info *f_info = create_file_info();
	fd = get_fd(&curr->file_list);

	f_info->fd = fd;
	f_info->file = file;
	f_info->position = 0;
	
	list_insert_ordered(&curr->file_list, &f_info->elem, &cmp_fd, NULL);
	
	return fd;

}

static int filesize(int fd) {
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL)
		exit(-1);
	int size = 0;

	lock_acquire(&filesys_lock);
	size = file_length(f_info->file);
	lock_release(&filesys_lock);

	return size;
}

static int read(int fd UNUSED, char *buffer, size_t size UNUSED) {
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

static int write(int fd, const char *buffer, size_t size) {
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

static void seek(int fd, unsigned position) {
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL)
		exit(-1);

	lock_acquire(&filesys_lock);
	file_seek(f_info->file, position);
	lock_release(&filesys_lock);
}

static unsigned tell(int fd) {
	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info == NULL) 
		exit(-1);
	int pos = 0;

	lock_acquire(&filesys_lock);
	pos = file_tell(f_info->file);
	lock_release(&filesys_lock);

	return pos; 
}

static void close(int fd) {
	if (fd == 0 || fd == 1) {
		exit(-1);
	}

	struct file_info *f_info = find_file_info_by_fd(fd);
	if (f_info != NULL) {
		list_remove(&f_info->elem);
		remove_file_info(f_info);
	} else {
		exit(-1);
	}
}











//////////////////////////
static int return_args(struct intr_frame *f, int order) {
	return (int) *(((int *) f->esp) + order);
}

static bool valid_address(const void *address) {
	uint32_t *pd = active_pd();

	bool valid = pagedir_is_accessed(pd, address);
	if ((unsigned) address > 0xc0000000) valid = false;
	//printf("valid address: %x, %d\n", (unsigned) address, valid);
	return valid; 
}

static struct file_info *create_file_info() {
	struct file_info *ptr = (struct file_info *) malloc (sizeof(struct file_info));
	return ptr;
}

static void remove_file_info(struct file_info * ptr) {
	free(ptr);
}

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

static struct file_info *find_file_info_by_fd(int fd) {
	struct thread *curr = thread_current();
	struct list *file_list = &curr->file_list;
	struct list_elem *e;
	for (e = list_begin(file_list); e != list_end(file_list); e = list_next(e)) {
		struct file_info *f_info = list_entry(e, struct file_info, elem);
		if (f_info->fd == fd) return f_info;
	}
	return NULL;
}











