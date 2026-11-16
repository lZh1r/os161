#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <cpu.h>
#include <thread.h>
#include <synch.h>
#include <vm.h>
#include <test.h>
#include <kern/test161.h>
#include <mainbus.h>
#include <proc.h>
#include <addrspace.h>

int 
vmt1(int nargs, char **args) {
    (void)nargs;
    (void)args;

    int test_result = TEST161_SUCCESS;

    struct addrspace *as = as_create();

    (void)as_define_region(as, (vaddr_t) 10000, 1000,1,1,1);

    if (as_complete_load(as)) {
        panic("as_complete_load failed\n");
    }
    vaddr_t ptr;
    if (as_define_stack(as, &ptr)) {
        panic("as_define_stack failed\n");
    }
    if (!ptr) {
        test_result = TEST161_FAIL;
        kprintf("stack is corrupted!\n");
    }

    //fails
    KASSERT(as_define_region(as, (vaddr_t) 10000, 1000,1,1,1));

    int a;
    if ((a = as_define_region(as, (vaddr_t) 13000, 1000,1,1,1))) {
        kprintf("13000 failed %d\n", a);
    }
    (void)as_define_region(as, (vaddr_t) 1000, 1000,1,1,1);

    int reg_count = 0;
    struct addrspace *copy;
    if (as_copy(as, &copy)) {
        panic("as_copy failed\n");
    }
    struct vm_region *cur = copy->regions;
    while (cur != NULL) {
        reg_count++;
        if (cur->next != NULL && cur->start >= cur->next->start) {
            kprintf("ordering is violated\n");
            test_result = TEST161_FAIL;
        }
        cur = cur->next;
    }
    if (reg_count != 5) {
        kprintf("wrong amount of regions: %d\n", reg_count);
        test_result = TEST161_FAIL;
    }

    as_destroy(as);
    as_destroy(copy);

    success(test_result, SECRET, "vmt1");

    return 0;
}
