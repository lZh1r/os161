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

#define NTHREADS 20
#define NREADS 100

static struct rwlock *thelock;
static struct semaphore *firstsem;
static struct semaphore *secondsem;
static struct semaphore *megasem;
static struct semaphore *rwsem;
static volatile int data = 1337; 

static
void
writer_thread(void *junk, unsigned long junk2) {
    (void)junk;
    (void)junk2;
    V(megasem);
    rwlock_acquire_write(thelock);
    kprintf("writer acquired the lock\n");
    P(rwsem);
    for (int i = 0; i < NREADS; i++) {
        random_yielder(4);
    }
    data = 28;
    rwlock_release_write(thelock);
}

static
void
reader_thread1(void *junk, unsigned long index) {
    (void)junk;

    rwlock_acquire_read(thelock);
    kprintf("reader %ld acquired the lock\n", index);
    V(firstsem);
    for (int i = 0; i < NREADS; i++) {
        random_yielder(4);
        KASSERT(1337 == data);
    }
    rwlock_release_read(thelock);
}

static
void
reader_thread2(void *junk, unsigned long junk2) {
    (void)junk;
    (void)junk2;

    kprintf("second batch reader started\n");
    V(rwsem);
    rwlock_acquire_read(thelock);
    for (int i = 0; i < NREADS; i++) {
        random_yielder(4);
        KASSERT(28 == data);
    }
    rwlock_release_read(thelock);
    V(secondsem);
}

/*
 * Use these stubs to test your reader-writer locks.
 */

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

    kprintf("Starting rwt1");

    thelock = rwlock_create("thelock");
    if (thelock == NULL) {
        panic("rwt1: could not create an rwlock");
    }

    firstsem = sem_create("fistsem", 0);
    if (firstsem == NULL) {
        panic("rwt1: could not create a semaphore");
    }
    secondsem = sem_create("secondsem", 0);
    if (firstsem == NULL) {
        panic("rwt1: could not create a semaphore");
    }
    megasem = sem_create("megasem", 0);
    if (megasem == NULL) {
        panic("rwt1: could not create a semaphore");
    }
    rwsem = sem_create("rwsem", 0);
    if (rwsem == NULL) {
        panic("rwt1: could not create a semaphore");
    }
    
    for (int i = 0; i < NTHREADS; i++) {
        int result = thread_fork("rwt1-reader1", NULL, reader_thread1, NULL, i);
        if (result) {
            panic("rwt1: thread creation failed");
        }
    }

    // all readers should acquire the lock
    for (int i = 0; i < NTHREADS; i++) {
        P(firstsem);
    }

    int result = thread_fork("rwt1-writer", NULL, writer_thread, NULL, 0);
    if (result) {
        panic("rwt1: thread creation failed");
    }

    P(megasem);

    for (int i = 0; i < NTHREADS; i++) {
        int result = thread_fork("rwt1-reader2", NULL, reader_thread2, NULL, 0);
        random_yielder(4);
        if (result) {
            panic("rwt1: thread creation failed");
        }
    }

    for (int i = 0; i < NTHREADS; i++) {
        P(secondsem);
    }

	success(TEST161_SUCCESS, SECRET, "rwt1");

    data = 1337;

	return 0;
}

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
