#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct list wait_list;
struct list load_wait_list;

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

struct child_info {
    tid_t tid;
    //int loaded; 			/* 0: not yet loaded, 1: loaded successfully, -1: load failure */
    struct list_elem elem;
};

#endif /* userprog/process.h */
