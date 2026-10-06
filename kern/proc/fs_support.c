#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/errno.h>
#include <synch.h>

/*
 * Open a file and add a file descriptor to the descriptor table of process proc.
 * Returns 0 on success, otherwise an error code.
 */
int
proc_open(struct proc *proc, char *path, int openflags, mode_t mode, unsigned *index) {
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

    openfile_array_add(proc->p_fdtable, openfile, index);

    return 0;
}

/* Free all of the fd table entries */
void
proc_free_fdtable(struct proc *proc) {
    KASSERT(proc != NULL);
    unsigned num = openfile_array_num(proc->p_fdtable);
    for (unsigned i = 0; i < num; i++) {
        struct openfile *of = openfile_array_get(proc->p_fdtable, 0);
        of_decref(of);
        openfile_array_remove(proc->p_fdtable, 0);
    }
}

void
of_decref(struct openfile *of) {
    lock_acquire(of->of_lock);
    of->of_refcount--;
    lock_release(of->of_lock);
    if (of->of_refcount == 0) {
        vfs_close(of->of_vn);
        kfree(of);
    }
}

void
of_incref(struct openfile *of) {
    lock_acquire(of->of_lock);
    of->of_refcount++;
    lock_release(of->of_lock);
}
