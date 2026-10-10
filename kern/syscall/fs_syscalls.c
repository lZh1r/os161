#include <types.h>
#include <syscall.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>
#include <copyinout.h>
#include <limits.h>
#include <vfs.h>

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

int
sys_write(int fd, userptr_t buf, size_t len, size_t *ret) {
    struct iovec iov;
    struct uio u;
    iov.iov_ubase = buf;
    iov.iov_len = len;
    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_offset = 0; // set it later
    u.uio_resid = len;
    u.uio_segflg = UIO_USERSPACE;
    u.uio_rw = UIO_WRITE;
    u.uio_space = proc_getas();
    return proc_write(curproc, fd, &u, ret);
}

int
sys_read(int fd, userptr_t buf, size_t len, size_t *ret) {
    struct iovec iov;
    struct uio u;
    iov.iov_ubase = buf;
    iov.iov_len = len;
    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_offset = 0; // set it later
    u.uio_resid = len;
    u.uio_segflg = UIO_USERSPACE;
    u.uio_rw = UIO_READ;
    u.uio_space = proc_getas();
    return proc_read(curproc, fd, &u, ret);
}

int
sys_chdir(const userptr_t pathname) {
    char kpath[PATH_MAX];
    if (copyinstr(pathname, kpath, PATH_MAX, NULL)) {
        return EFAULT;
    }
    return vfs_chdir(kpath);
}

int sys___getcwd(userptr_t buf, size_t len, size_t *ret) {
    struct iovec iov;
    struct uio u;
    iov.iov_ubase = buf;
    iov.iov_len = len;
    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_offset = 0;
    u.uio_resid = len;
    u.uio_segflg = UIO_USERSPACE;
    u.uio_rw = UIO_READ;
    u.uio_space = proc_getas();

    int result = vfs_getcwd(&u);
    if (result) {
        return result;
    }
    *ret = u.uio_offset;
    return 0;
}
