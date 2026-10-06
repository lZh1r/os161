#include <types.h>
#include <syscall.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>
#include <copyinout.h>
#include <limits.h>

int
sys_close(int fd) {
    return proc_close(curproc, fd);
}

int
sys_open(userptr_t path, int openflags, mode_t mode, int *ret) {
    *ret = -1;
    char *kpath = kmalloc(PATH_MAX);
    if (kpath == NULL) {
        return EMFILE;
    }
    if (copyinstr(path, kpath, PATH_MAX, NULL)) {
        return EFAULT;
    }
    return proc_open(curproc, kpath, openflags, mode, ret);
}
