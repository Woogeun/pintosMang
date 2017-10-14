#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "threads/synch.h"
#include <list.h>

void syscall_init (void);

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

struct lock filesys_lock;
struct lock free_lock;
struct file_info *find_file_info_by_fd(int);
/*
struct file_info {
    int fd;
    struct file *file;
    int position;
    struct list_elem elem;
};
*?
/*
struct wait_info {
	struct thread *waiter_thread;
	tid_t waitee_tid;
	int status;
	struct list_elem elem;
};

struct child_info {
    tid_t tid;
    struct list_elem elem;
};
*/
#endif /* userprog/syscall.h */
