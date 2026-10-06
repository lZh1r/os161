#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/errno.h>
#include <synch.h>

static
int
get_free_fd(struct proc *proc) {
    int result;
    lock_acquire(proc->p_sleeplock);
    unsigned amount = openfile_array_num(proc->p_fdtable);
    result = -1;
    for (unsigned i = 0; i < amount; i++) {
        if (openfile_array_get(proc->p_fdtable, i) == NULL) {
            result = (int) i;
            break;
        }
    }
    lock_release(proc->p_sleeplock);
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
    if (openfile_array_num(proc->p_fdtable) >= 1024) {
        return EMFILE;
    }

    struct openfile *openfile = kmalloc(sizeof(struct openfile));
    if (openfile == NULL) {
        return EMFILE;
    }
    
    struct vnode *vn;

    int result = vfs_open(path, openflags, mode, &vn);
    if (result) {
        kfree(openfile);
        return result;
    }

    openfile->of_lock = lock_create("openfile lock");
    if (openfile->of_lock == NULL) {
        vfs_close(vn);
        kfree(openfile);
    }
    openfile->of_vn = vn;
    openfile->of_seekpos = 0;
    openfile->of_refcount = 1;
    openfile->of_openflags = openflags;

    int fd = get_free_fd(proc);
    if (fd < 0) {
        openfile_array_add(proc->p_fdtable, openfile, (unsigned *)ret);
    } else {
        *ret = fd;
        openfile_array_set(proc->p_fdtable, (unsigned)fd, openfile);
    }

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
        return EBADF;
    }
    struct openfile *of = openfile_array_get(proc->p_fdtable, (unsigned)fd);
    if (of == NULL) {
        return EBADF;
    }
    lock_acquire(of->of_lock);
    size_t before = uio->uio_resid;
    uio->uio_offset = of->of_seekpos;
    int result = VOP_WRITE(of->of_vn, uio);
    if (result) {
        return result;
    }
    size_t written = before - uio->uio_resid;
    *ret = written;
    of->of_seekpos += written;
    lock_release(of->of_lock);
    lock_release(proc->p_sleeplock);
    return 0;
}
