/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.
   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.
   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:
   - down or "P": wait for the value to become positive, then
   decrement it.
   - up or "V": increment the value (and wake up one waiting
   thread, if any). */

struct semaphore_elem {
	struct list_elem elem;
	struct semaphore semaphore;
};

bool
compare_priority(const struct list_elem *x, const struct list_elem *y, void *aux) {
	struct thread *t1 = list_entry(x, struct thread, elem);
	struct thread *t2 = list_entry(y, struct thread, elem);

	if(t1 != NULL && t2 != NULL) {
		if(t1->priority > t2->priority) {
			return true;
		} else {
			return false;
		}
	}

	return false;
}


bool
compare_sema_priority(const struct list_elem *x, const struct list_elem *y, void *aux) {
	struct semaphore_elem *sx = list_entry(x, struct semaphore_elem, elem);
	struct semaphore_elem *sy = list_entry(y, struct semaphore_elem, elem);
	
	struct list *lx = &(sx->semaphore.waiters);
	struct list *ly = &(sy->semaphore.waiters);

	struct list_elem *lex = list_begin(lx);
	struct list_elem *ley = list_begin(ly);

	struct thread *t1 = list_entry(lex, struct thread, elem);
	struct thread *t2 = list_entry(ley, struct thread, elem);

	if(t1 != NULL && t2 != NULL) {
		if(t1->priority >= t2->priority) {
			return true;
		} else {
			return false;
		}
	}
	return false;
}

static inline bool
is_head (struct list_elem *elem) {
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

static inline bool
is_tail (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

list_sanity_check(struct list *list) {
	ASSERT(list != NULL);
	struct list_elem *b = list_begin(list);
	struct list_elem *t = list_end(list);
	struct list_elem *h = b->prev;
	
	//head check
	if(h == NULL) {
		printf("head가 널\n");
	} else {
		if(h->prev != NULL) {
			printf("head->prev가 널이 아님\n");
		} else if(h->next == NULL) {
			printf("head->next가 널\n");
		}
	}
	
	//tail check
	if(t == NULL) {
		printf("tail이 널\n");
		if(t->next != NULL) {
			printf("tail->next가 널\n");
		} else if(t->prev == NULL) {
			printf("tail->prev가 널\n");
		}
	}

	//interior check
	struct list_elem *temp = list_begin(list);
	while(temp != t) {
		if(temp == NULL) {
			printf("interior가 널\n");
		} else if(temp->prev == NULL) {
			printf("interior->prev가 널\n");
		} else if(temp->next == NULL) {
			printf("interior->next가 널\n");
		}
		temp = list_next(temp);
	}
}


void
push_waiters_in_priority_order(struct condition *cond, const struct list_elem *elem) {
	struct list_elem *waiters_i = list_begin(&cond->waiters);
	while(waiters_i != list_end(&cond->waiters)) {
		struct semaphore_elem *se = list_entry(waiters_i, struct semaphore_elem, elem);
		struct semaphore_elem *insert_se = list_entry(elem, struct semaphore_elem, elem);

		struct list *le = &(se->semaphore.waiters);
		struct list *insert_le = &(insert_se->semaphore.waiters);
		struct list_elem *lee = list_begin(le);
		struct list_elem *insert_lee = list_begin(insert_le);
		
		if(!list_empty(&le)) {
			struct thread *t1 = list_entry(lee, struct thread, elem);
			struct thread *t2 = list_entry(insert_lee, struct thread, elem);

			if((t1->priority >= t2->priority) && (lee != list_end(le))) {
				waiters_i = list_next(waiters_i);
			} else {
				break;
			}
		}
	}
	list_insert(waiters_i, elem);
}

void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.
   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		/*list_push_back (&sema->waiters, &thread_current ()->elem);*/
		struct list_elem *e = list_begin(&sema->waiters);
		if(!list_empty(&sema->waiters)) {
			struct thread *t1 = list_entry(e, struct thread, elem);
			struct thread *t2 = list_entry(&thread_current()->elem, struct thread, elem);

			while((t1->priority >= t2->priority) && (e != list_end(&sema->waiters))) {
				e = list_next(e);
				t1 = list_entry(e, struct thread, elem);
			}
		}
		list_insert(e, &thread_current()->elem);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.
   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.
   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)) {
		list_sort(&sema->waiters, compare_priority, NULL);
		thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));	
	}
	sema->value++;

	check_highest_priority();

	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.
   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.
   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

void
push_donated_by_in_priority_order(struct list *donated_by, const struct list_elem *elem) {
	struct list_elem *e = list_begin(donated_by);
	if(!list_empty(donated_by)) {
		struct thread *t1 = list_entry(e, struct thread, donator);
		struct thread *t2 = list_entry(elem, struct thread, donator);
		while((t1->priority >= t2->priority) && (e != list_end(donated_by))) {
			e = list_next(e);
			t1 = list_entry(e, struct thread, donator);
		}
	}
	list_insert(e, elem);
}


void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	if(thread_mlfqs) {
		sema_down(&lock->semaphore);
		lock->holder = thread_current();
		return;
	}

	struct thread *current_thread = thread_current();
	if(lock->holder != NULL) {
		current_thread->waiting_lock = lock;
		push_donated_by_in_priority_order(&lock->holder->donated_by, &current_thread->donator);
		donation(current_thread, 8, 1);
	}
	
	sema_down (&lock->semaphore);

	current_thread->waiting_lock = NULL;
	lock->holder = current_thread;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.
   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;

	if(thread_mlfqs) {
		sema_up(&lock->semaphore);
		return;
	}

	remove_lock(lock);
	update_priority();

	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}


/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.
   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.
   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.
   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));
	sema_init (&waiter.semaphore, 0); 

	/*list_push_back (&cond->waiters, &waiter.elem);*/
	/*struct semaphore_elem *testlist = list_entry(list_begin(&cond->waiters), struct semaphore_elem, elem);
	list_sanity_check(&testlist->semaphore.waiters);*/
	push_waiters_in_priority_order(cond, &waiter.elem);

	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		list_sort(&cond->waiters, compare_sema_priority, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}