# ASST2 design document
This document's purpose is to describe what needs to be done and how to do it in order to complete ASST2.

The assignment separates the system calls I need to implement into 2 groups: process support and filesystem support.

## Processes support
A big part of the assignment and the thing that syscalls will be manipulating is process support.

The calls related to process support are:
- getpid
- fork
- execv
- waitpid
- \_exit

In order to get an idea of what kind of state the process structure should hold I will examine each syscall the assignment requires implementing.

### waitpid
For this syscall the obvious piece of state needed is a **process ID** to identify exiting processes.

Another bit of state is an **exit code** which this system call also reports back to the waiter.

An important part of this system call is making sure that the ID of the process which is being waited on by other processes does not get reused before all of the waiters return from waitpid call.

This means that processes should have some **state marker** which can be used in these cases (and to manage them in general).

This very state marker should help the kernel differentiate a process that has exited and is not needed by anyone from a process which has exited but is still being waited upon.

The call itself might be implemented by simply waiting on a condition variable or even on a wait channel.

At the end of the call the process being waited on will be destroyed via proc_destroy, as in Unix model there can be only one process waiting for it to exit.

### getpid
Super simple one. Just return the process ID field of the process that invoked this system call.

### \_exit
Set the exit code of the current process and notify all the waiting processes.

We also need to set its state marker appropriately, so that this process's ID can get reused immediately or to postpone this reusal.

### fork
The second hardest syscall to implement from the process side of things.

It will copy most of the parent's state except for the process ID and address space.

The call itself might not be that bad, as we just create a new process and attach it to the caller as its child.

The tricky part is making both processes return different values.

### execv
Final boss of process related syscalls.

The part of running the new program is probably not the biggest hurdle, as that is handled by pre-existing functions like runprogram.

The difficulty comes from handling the arguments passed to the new program.

Since they reside in a null-terminated array of null-terminated strings we need a way of copying these arguments from the caller's address space into the one owned by the new process.

## Filesystem support
The filesystem operations introduce some new state to the processes right of the bat.

For instance, we need to keep track and modify a list of open file descriptors (probably a **descriptor table**).

We also need the descriptor table to be useful, so each entry should be a pointer to some open file object or something like that.

This object should contain things like:
- refcount (to close the underlying vnode when needed)
- seek position
- pointer to the vnode

Filesystem related systemcalls that are required in order to complete ASST2 are as follows:
- open
- read
- write
- lseek
- close
- dup2
- chdir
- \_\_getcwd

### open
Will be mostly implemented by calling vfs\_open.

The rest of the work is process state related, like allocating a new open file object and adding a new entry to the descriptor table.

### close
Also mostly implemented by vfs\_close.

What's left is removing the file descriptor from the table and decreasing refcount for the open file object it references.

### read
VOP\_READ into a uio and then uio\_move the desired amount of data into the user supplied buffer.

Should advance the affected file's seek offset.

### write
The same as write but in the opposite direction and with VOP\_WRITE.

### lseek
Just modifies file's seek offset.

### dup2
Changes the pointer to the open file object to another open file object pointer.

### chdir
Implies that we need to keep track of **currently open directory**.

Implemented by calling set\_curdir and changing the process state.

### \_\_getcwd
Probably just return the relevant process state field which first gets retrieved by calling get\_curdir.

## Error handling
A major part of a functioning system call implementation is graceful error handling.

The system should continue to operate despite the failure of any individual system call (if not a symptome of a deeper problem, but we cannot deal with that).

A good syscall handler is essential for that.

Another big problem is user level fatal exceptions.

In order to handle those the kernel needs a good kill\_process implementation which might be tricky.

## Process structure summary
The process structure (excluding existing fields) so far should be something like:
- p\_id
- p\_fdtable
- p\_status
- p\_exit\_code
- p\_curdir
- p\_sleeplock (for exclusive access to its structure)
- p\_parent
- p\_children
