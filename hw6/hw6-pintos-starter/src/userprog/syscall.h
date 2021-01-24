#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/*
 * The syscall handler initializes this pointer, which locates the intr_frame
 * of the user thread issuing the syscall, and sets it to NULL if the thread is
 * not in a syscall
 */
void *USER_INTR_FRAME_PTR;

void syscall_exit (int status);
void syscall_init (void);

#endif /* userprog/syscall.h */
