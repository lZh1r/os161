#include <types.h>
#include <lib.h>
#include <proc.h>
#include <test.h>
#include <kern/test161.h>
#include <current.h>

#define NPROCS 10
#define NTIMES 1024

static bool test_status = TEST161_SUCCESS;
static struct proc *procs[NPROCS];

static
void
failif(bool expr) {
    if (expr) {
        test_status = TEST161_FAIL;
    }
}

static
void
procidtest_internal() {
    for (int i = 0; i < NPROCS; i++) {
        procs[i] = proc_create("testproc");
        if (procs[i] == NULL) {
            panic("procidtest: failed to create a process");
        }
        failif(procs[i]->p_id != i + 1);
        failif(procs[i]->p_parent != curproc);
        // since we create these processes pretty crudely we have to partially set them up manually
        procs[i]->p_status = PROC_EXIT;
        proc_array_add(curproc->p_children, procs[i], NULL);
    }

    proc_wait(procs[NPROCS - 1], false);

    failif(proc_get_free_id() != -1);

    proc_wait(procs[0], false);

    // 0 is kproc
    failif(proc_get_free_id() != 1);

    for (int i = 1; i < NPROCS - 1; i++) {
        proc_wait(procs[i], false);
    }
}

/* test process id reusability and cleanup logic as well as basic correctness */
int
procidtest(int nargs, char **args) {
    (void)nargs;
    (void)args;
    kprintf("Starting procidtest\n");

    for (int i = 0; i < NTIMES; i++) {
        procidtest_internal();
    }

    kprintf("process amount: %d (should be 1)\n", proc_get_proc_amount());
    failif(proc_get_proc_amount() != 1);

	success(test_status, SECRET, "procidtest");
    return 0;
}

int
procwaittest(int nargs, char **args) {
    (void)nargs;
    (void)args;

	success(test_status, SECRET, "procwaittest");
    return 0;
}
