#include <types.h>
#include <syscall.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>
#include <copyinout.h>
#include <addrspace.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <limits.h>

int
sys_execv(userptr_t progname, userptr_t argv) {
    char kpath[PATH_MAX];
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, argc = 0;
    size_t offset = 0;
    struct array *offsets;
    struct addrspace *old_as = NULL;

    //kprintf("entered\n");

    char *kargv = kmalloc(ARG_MAX);
    if (kargv == NULL) {
        return ENOMEM;
    }

    /* Get the pathname into the kernel space */
    result = copyinstr(progname, kpath, PATH_MAX, NULL);
    
    if (result) {
        kfree(kargv);
        return result;
    }

    //kprintf("progname copied\n");

    offsets = array_create();
    array_init(offsets);

    /* Copy all of the argument strings into kargv */
    while (1) {
        userptr_t arg;
        
        // adding beforehand to also have the offset for the final NULL in the argv
        result = array_add(offsets, (void *)offset, NULL);
        if (result) {
            goto fail;
        }

        result = copyin(argv + sizeof(char *) * argc, &arg, sizeof(char *));
        if (result) {
            goto fail;
        }

        if (arg == NULL) {
            break;
        }
        size_t read;
        result = copyinstr(arg, kargv + offset, ARG_MAX, &read);
        if (result) {
            goto fail;

        }
        offset += read;
        if (offset >= ARG_MAX) {
            result = E2BIG;
            goto fail;
        }
        argc++;
    }

    //kprintf("args copied\n");

	/* Open the file. */
	result = vfs_open(kpath, O_RDONLY, 0, &v);
	if (result) {
        goto fail;
	}

    /* ADDRESS SPACE SWITCH START !!! */

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
        result = ENOMEM;
        goto fail;
	}

    old_as = proc_getas();
    as_deactivate();

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();
    
    /* ADDRESS SPACE SWITCH END !!! */

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
        goto fail;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
        goto fail;
	}

    // argv strings base pointer
    userptr_t uargv_vals = (userptr_t)(stackptr - offset);
    // calculate argv pointer
    userptr_t uargv = (userptr_t)(((vaddr_t)uargv_vals & (~7)) - (argc + 1) * sizeof(char *));

    // copy each argument and its pointer into the new address space
    for (int i = 0; i < argc; i++) {
        size_t o = (size_t)array_get(offsets, 0);
        userptr_t uargp = (userptr_t)(uargv_vals + o);
        
        result = copyoutstr(kargv + o, uargp, ARG_MAX, NULL);
        if (result) {
            goto fail;
        }
        result = copyout(&uargp, uargv + i * sizeof(char *), sizeof(char *));
        if (result) {
            goto fail;
        }
        array_remove(offsets, 0);
    }

    void *null = NULL;
    result = copyout(&null, uargv + argc * sizeof(char *), sizeof(char *));
    if (result) {
        goto fail;
    }

    kfree(kargv);
    array_remove(offsets, 0);
    array_cleanup(offsets);
    array_destroy(offsets);
    as_destroy(old_as);

    vaddr_t stack = (vaddr_t)uargv & (~7);

    enter_new_process(argc, uargv, NULL, stack, entrypoint);

fail:
    kfree(kargv);
    unsigned num = array_num(offsets);
    while (num != 0) {
        array_remove(offsets, --num);
    }
    array_cleanup(offsets);
    array_destroy(offsets);

    /* restore previous address space if we have already switched to a new one */
    if (old_as != NULL) {
        struct addrspace *new_as = proc_getas();
        as_deactivate();
        proc_setas(old_as);
        as_activate();
        as_destroy(new_as);
    }
    return result;
}
