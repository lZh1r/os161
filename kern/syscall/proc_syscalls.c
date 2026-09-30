#include <types.h>
#include <syscall.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>

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
