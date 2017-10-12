#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>

void syscall_init (void);

struct file_info {
    int fd;
    struct file *file;
    int position;
    struct list_elem elem;
};

#endif /* userprog/syscall.h */
