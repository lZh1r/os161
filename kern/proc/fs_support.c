#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/errno.h>
#include <synch.h>
#include <kern/fcntl.h>

static
int
get_free_fd(struct proc *proc, bool locked) {
    int result;
    // We did not hold the lock when entering this function
    if (!locked) {
        lock_acquire(proc->p_sleeplock);
    }
    unsigned amount = openfile_array_num(proc->p_fdtable);
    result = -1;
    for (unsigned i = 0; i < amount; i++) {
        if (openfile_array_get(proc->p_fdtable, i) == NULL) {
            result = (int) i;
            break;
        }
    }
    if (!locked) {
        lock_release(proc->p_sleeplock);
    }
    return result;
}

static
void
release_fd(struct proc *proc, int fd) {
    int index = fd;

    lock_acquire(proc->p_sleeplock);

    unsigned length = openfile_array_num(proc->p_fdtable);

    if ((unsigned) fd == length - 1) {
        struct openfile *value;
        do {
            openfile_array_remove(proc->p_fdtable, (unsigned) index);
            index--;
            if (index < 0) {
                break;
            }
            
        } while ((value = openfile_array_get(proc->p_fdtable, index)) == NULL);
    } else {
        openfile_array_set(proc->p_fdtable, index, NULL);
    }

    lock_release(proc->p_sleeplock);
}

/*
 * Open a file and add a file descriptor to the descriptor table of process proc.
 * Returns 0 on success, otherwise an error code.
 */
int
proc_open(struct proc *proc, char *path, int openflags, mode_t mode, int *ret) {
    lock_acquire(proc->p_sleeplock);
    if (openfile_array_num(proc->p_fdtable) >= PROC_MAX_FD) {
        lock_release(proc->p_sleeplock);
        return EMFILE;
    }

    struct openfile *openfile = kmalloc(sizeof(struct openfile));
    if (openfile == NULL) {
        lock_release(proc->p_sleeplock);
        return EMFILE;
    }
    
    struct vnode *vn;

    int result = vfs_open(path, openflags, mode, &vn);
    if (result) {
        kfree(openfile);
        lock_release(proc->p_sleeplock);
        return result;
    }

    openfile->of_lock = lock_create("openfile lock");
    if (openfile->of_lock == NULL) {
        vfs_close(vn);
        kfree(openfile);
        lock_release(proc->p_sleeplock);
        return EMFILE;
    }
    openfile->of_vn = vn;
    openfile->of_seekpos = 0;
    openfile->of_refcount = 1;
    openfile->of_openflags = openflags;

    int fd = get_free_fd(proc, true);
    if (fd < 0) {
        openfile_array_add(proc->p_fdtable, openfile, (unsigned *)ret);
    } else {
        *ret = fd;
        openfile_array_set(proc->p_fdtable, (unsigned)fd, openfile);
    }
    lock_release(proc->p_sleeplock);

    return 0;
}

int
proc_close(struct proc *proc, int fd) {
    lock_acquire(proc->p_sleeplock);
    unsigned num = openfile_array_num(proc->p_fdtable);
    if (fd < 0 || (unsigned)fd >= num) {
        return 1;
    }
    struct openfile *of = openfile_array_get(proc->p_fdtable, (unsigned)fd);
    of_decref(of);
    lock_release(proc->p_sleeplock);
    release_fd(proc, fd);

    return 0;
}

/* Free all of the fd table entries */
void
proc_free_fdtable(struct proc *proc) {
    KASSERT(proc != NULL);
    lock_acquire(proc->p_sleeplock);
    unsigned num = openfile_array_num(proc->p_fdtable);
    for (unsigned i = 0; i < num; i++) {
        struct openfile *of = openfile_array_get(proc->p_fdtable, 0);
        of_decref(of);
        openfile_array_remove(proc->p_fdtable, 0);
    }
    lock_release(proc->p_sleeplock);
}

void
of_decref(struct openfile *of) {
    lock_acquire(of->of_lock);
    of->of_refcount--;
    lock_release(of->of_lock);
    if (of->of_refcount == 0) {
        vfs_close(of->of_vn);
        lock_destroy(of->of_lock);
        kfree(of);
    }
}

void
of_incref(struct openfile *of) {
    lock_acquire(of->of_lock);
    of->of_refcount++;
    lock_release(of->of_lock);
}

int
proc_write(struct proc *proc, int fd, struct uio *uio, size_t *ret) {
    lock_acquire(proc->p_sleeplock);
    unsigned num = openfile_array_num(proc->p_fdtable);
    if (fd < 0 || (unsigned)fd >= num) {
        lock_release(proc->p_sleeplock);
        return EBADF;
    }
    struct openfile *of = openfile_array_get(proc->p_fdtable, (unsigned)fd);
    if (of == NULL) {
        lock_release(proc->p_sleeplock);
        return EBADF;
    }
    lock_acquire(of->of_lock);
    int accessmode = of->of_openflags && O_ACCMODE;
    if (!(accessmode == O_WRONLY || accessmode == O_RDWR)) {
        lock_release(of->of_lock);
        lock_release(proc->p_sleeplock);
        return EBADF;
    }
    size_t before = uio->uio_resid;
    uio->uio_offset = of->of_seekpos;
    int result = VOP_WRITE(of->of_vn, uio);
    if (result) {
        lock_release(of->of_lock);
        lock_release(proc->p_sleeplock);
        return result;
    }
    size_t written = before - uio->uio_resid;
    *ret = written;
    of->of_seekpos += written;
    lock_release(of->of_lock);
    lock_release(proc->p_sleeplock);
    return 0;
}

int
proc_read(struct proc *proc, int fd, struct uio *uio, size_t *ret) {
    lock_acquire(proc->p_sleeplock);
    unsigned num = openfile_array_num(proc->p_fdtable);
    if (fd < 0 || (unsigned)fd >= num) {
        lock_release(proc->p_sleeplock);
        return EBADF;
    }
    struct openfile *of = openfile_array_get(proc->p_fdtable, (unsigned)fd);
    if (of == NULL) {
        lock_release(proc->p_sleeplock);
        return EBADF;
    }
    lock_acquire(of->of_lock);
    int accessmode = of->of_openflags && O_ACCMODE;
    if (!(accessmode == O_RDONLY || accessmode == O_RDWR)) {
        lock_release(of->of_lock);
        lock_release(proc->p_sleeplock);
        return EBADF;
    }
    size_t before = uio->uio_resid;
    uio->uio_offset = of->of_seekpos;
    int result = VOP_READ(of->of_vn, uio);
    if (result) {
        lock_release(of->of_lock);
        lock_release(proc->p_sleeplock);
        return result;
    }
    size_t read = before - uio->uio_resid;
    *ret = read;
    of->of_seekpos += read;
    lock_release(of->of_lock);
    lock_release(proc->p_sleeplock);
    return 0;
}

int
proc_dup2(int oldfd, int newfd) {
    if (newfd > PROC_MAX_FD || newfd < 0 || oldfd < 0) {
        return EBADF;
    }

    lock_acquire(curproc->p_sleeplock);
    struct openfile *old = openfile_array_get(curproc->p_fdtable, oldfd);
    if (old == NULL) {
        lock_release(curproc->p_sleeplock);
        return EBADF;
    }

    unsigned num = openfile_array_num(curproc->p_fdtable);
    /* we need to allocate all of the empty entried in between (I cry) */
    if ((unsigned)newfd >= num) {
        while (num != (unsigned)newfd) {
            if (openfile_array_add(curproc->p_fdtable, NULL, &num)) {
                lock_release(curproc->p_sleeplock);
                return EMFILE;
            }
        }
        num++;
    }
    struct openfile *new = openfile_array_get(curproc->p_fdtable, newfd);
    if (new != NULL) {
        of_decref(new);
    }
    openfile_array_set(curproc->p_fdtable, newfd, old);
    of_incref(old);

    lock_release(curproc->p_sleeplock);
    return 0;
}
