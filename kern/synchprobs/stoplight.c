/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */
static struct lock *stat_lock;
static struct cv *stat_cv;
static volatile bool occupancy[4];
static volatile int waiting[4];
static struct lock *locks[4];
static volatile uint32_t turn = 0;
static volatile int cars = 0;

static
int
straight_quadrant(uint32_t direction) {
    return (direction + 3) % 4;
}

static
int
left_quadrant(uint32_t direction) {
    return ((int)direction + 2) % 4;
}

void
stoplight_init() {
    stat_lock = lock_create("statlock");
    if (stat_lock == NULL) {
        panic("cannot create lock");
    }

    stat_cv = cv_create("statcv");
    if (stat_cv == NULL) {
        panic("cannot create cv");
    }

    for (int i = 0; i < 4; i++) {
        locks[i] = lock_create("lockxd");
        if (locks[i] == NULL) {
            panic("cannot create a lock");
        }
        occupancy[i] = false;
        waiting[i] = 0;
    }

	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
    for (int i = 0; i<4; i++) {
        lock_destroy(locks[i]);
    }
    lock_destroy(stat_lock);
    cv_destroy(stat_cv);
	return;
}

// Some helpers to reduce boilerplate

static
bool
can_enter(uint32_t direction) {
    return (turn == direction || !waiting[turn]) && !occupancy[direction] && cars < 3;
}

static
void
enter(uint32_t direction) {
    bool slept = false;
    lock_acquire(stat_lock);
    while (!can_enter(direction)) {
        if (!slept) {
            waiting[direction]++;
            slept = true;
        }
        cv_wait(stat_cv, stat_lock);
    }
    if (slept) {
        waiting[direction]--;
    }
    cars++;
    turn = (turn + 1) % 4;
    occupancy[direction] = true;
    lock_release(stat_lock);
}

static
void
leave(uint32_t direction) {
    lock_acquire(stat_lock);
    occupancy[direction] = false;
    cars--;
    cv_broadcast(stat_cv, stat_lock);
    lock_release(stat_lock);
}

////////////////////////////////
// Solution

void
turnright(uint32_t direction, uint32_t index)
{
    enter(direction);

    // Mark that we are at quadrant direction and leave the intersection
    lock_acquire(locks[direction]);
    inQuadrant(direction, index);
    leaveIntersection(index);
    lock_release(locks[direction]);

    leave(direction);
	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
    uint32_t target = straight_quadrant(direction);

    enter(direction);

    // Mark that we entered the intersection
    lock_acquire(locks[direction]);
    inQuadrant(direction, index);

    // Get a hold of the next quadrant
    lock_acquire(locks[target]);
    // Mark that quadrant
    inQuadrant(target, index);
    // DO NOT RELEASE THIS LOCK BEFORE MARKING THE QUADRANT!!!
    lock_release(locks[direction]);
    // Leave
    leaveIntersection(index);
    lock_release(locks[target]);

    leave(direction);
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
    uint32_t midpoint = straight_quadrant(direction);
    uint32_t target = left_quadrant(direction);

    enter(direction);

    lock_acquire(locks[direction]);
    inQuadrant(direction, index);

    lock_acquire(locks[midpoint]);
    // Mark that quadrant
    inQuadrant(midpoint, index);
    // DO NOT RELEASE THIS LOCK BEFORE MARKING THE QUADRANT!!!
    lock_release(locks[direction]);

    lock_acquire(locks[target]);
    // Mark that quadrant
    inQuadrant(target, index);
    // DO NOT RELEASE THIS LOCK BEFORE MARKING THE QUADRANT!!!
    lock_release(locks[midpoint]);
    // Leave
    leaveIntersection(index);
    lock_release(locks[target]);

    leave(direction);
	return;
}
