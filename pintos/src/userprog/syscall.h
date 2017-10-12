#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include <list.h>

void syscall_init (void);

struct file_info {
    int fd;
    struct file *file;
    int position;
    struct list_elem elem;
};

struct wait_info {
	struct thread *waiter_thread;
	tid_t waitee_tid;
	int status;
	struct list_elem elem;
};

#endif /* userprog/syscall.h */
