/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

/*
 * Use these stubs to test your reader-writer locks.
 */

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

    kprintf("Starting rwt1");
	success(TEST161_FAIL, SECRET, "rwt1");

	return 0;
}

#define NTHREADS 50
#define NREADS 100
static struct rwlock *testlock;
static volatile int testdata;
static struct semaphore *testsem;
static struct spinlock stat_lock;
static volatile int concurrent = 0;
static volatile int max_concurrent = 0;

static
void
read_thread(void *junk, unsigned long num) {
    (void)junk;
    (void)num;
    rwlock_acquire_read(testlock);

    spinlock_acquire(&stat_lock);
    concurrent += 1;
    if (concurrent > max_concurrent) {
        max_concurrent = concurrent;
    }
    spinlock_release(&stat_lock);

    for (int i = 0; i < NREADS; i++) {
        thread_yield();    
    }
    rwlock_release_read(testlock);

    spinlock_acquire(&stat_lock);
    concurrent -= 1;
    spinlock_release(&stat_lock);
    V(testsem);
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("Starting rwt2\n");
    testdata = 5;

    testsem = sem_create("testsemaphore", 0);
    if (testsem == NULL) {
        panic("rwt2: failed to create a test semaphore");
    }

    testlock = rwlock_create("testlock");
    if (testlock == NULL) {
        panic("rwt2: failed to create a rw_lock");
    }

    spinlock_init(&stat_lock);

    rwlock_acquire_write(testlock);
    rwlock_release_write(testlock);

    for (int i = 0; i < NTHREADS; i++) {
        random_yielder(4);
        int result = thread_fork("rwt2-reader", NULL, read_thread, NULL, i);
        if (result) {
            panic("rwt2: thread_fork failed\n");
        }
    }

    for (int i = 0; i < NTHREADS; i++) {
        P(testsem);
    }
	
    if (max_concurrent != NTHREADS) {
        success(TEST161_FAIL, SECRET, "rwt2");
    } else {
        success(TEST161_SUCCESS, SECRET, "rwt2");
    }

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

    kprintf("Starting rwt3");
    kprintf("This test panics on success");

    struct rwlock *lock = rwlock_create("testlock");
    if (lock == NULL) {
        panic("rwt3: failed to create a rw_lock");
    }

	secprintf(SECRET, "Should panic...", "rwt3");
    rwlock_release_write(lock);
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

    kprintf("Starting rwt4");
    kprintf("This test panics on success");

    struct rwlock *lock = rwlock_create("testlock");
    if (lock == NULL) {
        panic("rwt3: failed to create a rw_lock");
    }

    rwlock_acquire_read(lock);

	secprintf(SECRET, "Should panic...", "rwt4");
    rwlock_release_write(lock);
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

    kprintf("Starting rwt5");
    kprintf("This test panics on success");

    struct rwlock *lock = rwlock_create("testlock");
    if (lock == NULL) {
        panic("rwt3: failed to create a rw_lock");
    }

    rwlock_acquire_write(lock);

	secprintf(SECRET, "Should panic...", "rwt5");
    rwlock_release_read(lock);
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
