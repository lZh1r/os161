/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _PROC_H_
#define _PROC_H_

/*
 * Definition of a process.
 *
 * Note: curproc is defined by <current.h>.
 */

#include <spinlock.h>
#include <synch.h>
#include <array.h>
#include <syscall.h>
#include <uio.h>

#define PROC_MAX_ID 1024
#define PROC_MAX_FD 1024
#define PROC_MAX_CHILDREN 32

#ifndef PROC_INLINE
#define PROC_INLINE INLINE
#endif

DECLARRAY_BYTYPE(proc_array, struct proc, PROC_INLINE);
DEFARRAY_BYTYPE(proc_array, struct proc, PROC_INLINE);

DECLARRAY_BYTYPE(openfile_array, struct openfile, PROC_INLINE);
DEFARRAY_BYTYPE(openfile_array, struct openfile, PROC_INLINE);

struct addrspace;
struct thread;
struct vnode;

/*
 * Process status
 * PROC_NEW - brand new process (maybe?)
 * PROC_ACTIVE - a process that has been properly initialized and can be run by the system
 * PROC_EXIT - a process has exited, but its resources cannot yet be reclaimed, as its parent has not returned from waitpid call
 * PROC_DESTROY - a process can be destroyed
 */
enum proc_status {
    PROC_NEW,
    PROC_ACTIVE,
    PROC_EXIT,
    PROC_DESTROY
};

/*
 * Process structure.
 *
 * Note that we only count the number of threads in each process.
 * (And, unless you implement multithreaded user processes, this
 * number will not exceed 1 except in kproc.) If you want to know
 * exactly which threads are in the process, e.g. for debugging, add
 * an array and a sleeplock to protect it. (You can't use a spinlock
 * to protect an array because arrays need to be able to call
 * kmalloc.)
 *
 * You will most likely be adding stuff to this structure, so you may
 * find you need a sleeplock in here for other reasons as well.
 * However, note that p_addrspace must be protected by a spinlock:
 * thread_switch needs to be able to fetch the current address space
 * without sleeping.
 */
struct proc {
	char *p_name;			/* Name of this process */
	struct spinlock p_lock;		/* Lock for this structure */
	unsigned p_numthreads;		/* Number of threads in this process */

	/* VM */
	struct addrspace *p_addrspace;	/* virtual address space */

	/* VFS */
	struct vnode *p_cwd;		/* current working directory */

	/* add more material here as needed */
    struct lock *p_sleeplock;    /* a sleeplock for waitpid and other things */

    struct cv *p_cv; /* condition variable for waiting on other processes */

    pid_t p_id;     /* process id */

    enum proc_status p_status; /* process status */

    int p_exitcode; /* exit code */

    struct proc *p_parent; /* pointer to the parent process */

    struct proc_array *p_children; /* array of child struct proc pointers */

    struct openfile_array *p_fdtable; /* file descriptor table */
};

/* This is the process structure for the kernel and for kernel-only threads. */
extern struct proc *kproc;

/* Call once during system startup to allocate data structures. */
void proc_bootstrap(void);

/* Create a fresh process for use by runprogram(). */
struct proc *proc_create_runprogram(const char *name);

/* Destroy a process. */
void proc_destroy(struct proc *proc);

/* Attach a thread to a process. Must not already have a process. */
int proc_addthread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. */
void proc_remthread(struct thread *t);

/* Fetch the address space of the current process. */
struct addrspace *proc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *proc_setas(struct addrspace *);

/* Make current process exit with some exit code */
void proc_exit(int exitcode);

/* Wait for a process to exit, cleaning up after it */
int proc_wait(struct proc *proc);

pid_t proc_get_free_id(void);
unsigned proc_get_proc_amount(void);
unsigned proc_get_child_amount(struct proc *);
struct proc *proc_get_by_id(pid_t pid);

struct proc *proc_create(const char *);

struct proc *proc_fork(struct trapframe *tf);

/* 
 * A struct representing an open file which file descriptors point to.
 * Basically a vnode wrapper
 */
struct openfile {
    struct lock *of_lock;
    // reference counter for this structure
    int of_refcount;
    // underlying vnode
    struct vnode *of_vn;
    // seek position
    size_t of_seekpos;
    // open flags
    int of_openflags;
};

void of_decref(struct openfile *of);
void of_incref(struct openfile *of);

/* Open a file and add a file descriptor to the proc's fdtable */
int proc_open(struct proc *proc, char *path, int openflags, mode_t mode, int *ret);
/* Remove all of the entries from the proc's fdtable and decrement their reference count. */
void proc_free_fdtable(struct proc *proc);
/* Close a file descriptor fd for process proc */
int proc_close(struct proc *proc, int fd);
/* Write to a file specified under file descriptor fd */
int proc_write(struct proc *proc, int fd, struct uio *uio, size_t *ret);
int proc_read(struct proc *proc, int fd, struct uio *uio, size_t *ret);

#endif /* _PROC_H_ */
