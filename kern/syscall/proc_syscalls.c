#include <types.h>
#include <syscall.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>
#include <copyinout.h>

int
sys_fork(struct trapframe *tf, int *retval) {
    if (proc_get_proc_amount() >= PROC_MAX_ID) {
        return ENPROC;
    }
    if (proc_get_child_amount(curproc) >= PROC_MAX_CHILDREN) {
        return EMPROC;
    }
    struct proc *result = proc_fork(tf);
    if (result == NULL) {
        return ENOMEM;
    }
    *retval = (int) result->p_id;
    return 0;
}

int
sys__exit(int code) {
    proc_exit(code);
    return 1;
}

pid_t
sys_getpid(void) {
    // we can reasonably assume that reading process id is atomic,
    // as it does not really change unless the process struct gets deallocated
    return curproc->p_id;
}

int
sys_waitpid(pid_t pid, userptr_t status, int options) {
    if (pid <= 0 || options != 0) {
        return EINVAL;
    }
    struct proc *target = proc_get_by_id(pid);
    if (target == NULL) {
        return ESRCH;
    }

    if (target->p_parent != curproc) {
        return ECHILD;
    }

    int exitcode = proc_wait(target, false);

    if (status != NULL && copyout(&exitcode, status, sizeof(exitcode))) {
        return EFAULT;
    }
    
    return 0;
}
