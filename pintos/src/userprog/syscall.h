#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <list.h>

void syscall_init (void);


//syscall helper function
void        halt(void);						
void        exit(int);						
tid_t       exec(const char *);				
int         wait(tid_t);					
bool        create(const char *, size_t);		
bool        remove(const char *);				
int         open(const char *);					
int         filesize(int);						
int         read(int, char *, size_t);			
int         write(int, const char *, size_t);	
void        seek(int, unsigned);				
unsigned    tell(int);						
void        close(int);						


//shared data
struct lock filesys_lock;
struct lock free_lock;


//My Implementation
int return_args(struct intr_frame *, int);

//about validation
bool valid_address(const void *);
bool is_child(struct thread *, tid_t);

//about wait_info structure
struct 	wait_info *create_wait_info(void);
struct 	wait_info *find_wait_info_by_child_tid(tid_t);
void 	awake_wait_thread(int);
void 	free_wait_list(void);
void 	free_thread_list(void);

//about file_info structure
struct 	file_info *create_file_info(void);
struct 	file_info *find_file_info_by_fd(int);
int 	get_fd(struct list *);
bool 	cmp_fd(const struct list_elem *, const struct list_elem *, void *);
void 	free_file_list(void);

//about child_info structure
struct 	child_info *create_child_info(void);
struct 	child_info *find_child_info_by_tid(struct thread *, tid_t);
void 	free_child_list(void);



#endif /* userprog/syscall.h */
