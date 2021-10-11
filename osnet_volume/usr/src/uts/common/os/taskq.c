/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)taskq.c	1.3	99/07/07 SMI"

/*
 * Task queues: general-purpose asynchronous task scheduling.
 *
 * A common problem in kernel programming is the need to schedule tasks
 * to be performed later, by another thread.  There are several reasons
 * you may want or need to do this:
 *
 * (1) The task isn't time-critical, but your current code path is.
 *
 * (2) The task may require grabbing locks that you already hold.
 *
 * (3) The task may need to block (e.g. to wait for memory), but you
 *     cannot block in your current context.
 *
 * (4) You just want a simple way to launch multiple tasks in parallel.
 *
 * Task queues provide such a facility.  A task queue consists of a single
 * queue of tasks, together with one or more threads to service the queue.
 * The interfaces are as follows:
 *
 * taskq_t *taskq_create(name, nthreads, pri, minalloc, maxalloc, flags):
 *
 *	Creates a task queue with 'nthreads' threads at priority 'pri'.
 *	task_alloc() and task_free() try to keep the task population between
 *	'minalloc' and 'maxalloc', but the latter limit is only advisory
 *	for KM_SLEEP allocations.  If TASKQ_PREPOPULATE is set in 'flags',
 *	the taskq will be prepopulated with 'minalloc' task structures.
 *
 *	Since taskqs are queues, tasks are guaranteed to be
 *	executed in the order they are scheduled if nthreads == 1.
 *	If nthreads > 1, task execution order is not predictable.
 *
 * void taskq_destroy(tq):
 *
 *	Waits for any scheduled tasks to complete, then destroys the taskq.
 *
 * int taskq_dispatch(tq, func, arg, kmflags):
 *
 *	Dispatches the task "func(arg)" to taskq.  kmflags indicates whether
 *	the caller is willing to block for memory.  If kmflags is KM_NOSLEEP
 *	and memory cannot be allocated, taskq_dispatch() fails and returns 0.
 *	If kmflags is KM_SLEEP, taskq_dispatch() always succeeds.
 *
 * void taskq_wait(tq):
 *
 *	Waits for all previously scheduled tasks to complete.
 *
 * krwlock_t *taskq_lock(tq):
 *
 *	Returns a pointer to the task queue's thread lock, which is always
 *	held as RW_READER by taskq threads while executing tasks.  There are
 *	two intended uses for this: (1) to ASSERT that a given function is
 *	called in taskq context only, and (2) to allow the caller to suspend
 *	all task execution temporarily by grabbing the lock as RW_WRITER.
 */

#include <sys/taskq_impl.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/callb.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

kmem_cache_t *task_cache, *taskq_cache;

/*ARGSUSED*/
static int
taskq_constructor(void *buf, void *cdrarg, int kmflags)
{
	taskq_t *tq = buf;

	bzero(tq, sizeof (taskq_t));

	mutex_init(&tq->tq_lock, NULL, MUTEX_DEFAULT, NULL);
	rw_init(&tq->tq_threadlock, NULL, RW_DEFAULT, NULL);
	cv_init(&tq->tq_dispatch_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&tq->tq_wait_cv, NULL, CV_DEFAULT, NULL);

	tq->tq_task.task_next = &tq->tq_task;
	tq->tq_task.task_prev = &tq->tq_task;

	return (0);
}

/*ARGSUSED*/
static void
taskq_destructor(void *buf, void *cdrarg)
{
	taskq_t *tq = buf;

	mutex_destroy(&tq->tq_lock);
	rw_destroy(&tq->tq_threadlock);
	cv_destroy(&tq->tq_dispatch_cv);
	cv_destroy(&tq->tq_wait_cv);
}

void
taskq_init(void)
{
	task_cache = kmem_cache_create("task_cache", sizeof (task_t),
	    0, NULL, NULL, NULL, NULL, NULL, 0);
	taskq_cache = kmem_cache_create("taskq_cache", sizeof (taskq_t),
	    0, taskq_constructor, taskq_destructor, NULL, NULL, NULL, 0);
}

static task_t *
task_alloc(taskq_t *tq, int kmflags)
{
	task_t *t;

	ASSERT(MUTEX_HELD(&tq->tq_lock));

	if ((t = tq->tq_freelist) != NULL && tq->tq_nalloc >= tq->tq_minalloc) {
		tq->tq_freelist = t->task_next;
	} else {
		mutex_exit(&tq->tq_lock);
		if (tq->tq_nalloc >= tq->tq_maxalloc) {
			if (kmflags & KM_NOSLEEP) {
				mutex_enter(&tq->tq_lock);
				return (NULL);
			}
			/*
			 * We don't want to exceed tq_maxalloc, but we can't
			 * wait for other tasks to complete (and thus free up
			 * task structures) without risking deadlock with
			 * the caller.  So, we just delay for one second
			 * to throttle the allocation rate.
			 */
			delay(hz);
		}
		t = kmem_cache_alloc(task_cache, kmflags);
		mutex_enter(&tq->tq_lock);
		if (t != NULL)
			tq->tq_nalloc++;
	}
	return (t);
}

static void
task_free(taskq_t *tq, task_t *t)
{
	ASSERT(MUTEX_HELD(&tq->tq_lock));

	if (tq->tq_nalloc <= tq->tq_minalloc) {
		t->task_next = tq->tq_freelist;
		tq->tq_freelist = t;
	} else {
		tq->tq_nalloc--;
		mutex_exit(&tq->tq_lock);
		kmem_cache_free(task_cache, t);
		mutex_enter(&tq->tq_lock);
	}
}

int
taskq_dispatch(taskq_t *tq, task_func_t func, void *arg, int kmflags)
{
	task_t *t;

	mutex_enter(&tq->tq_lock);
	if ((t = task_alloc(tq, kmflags)) == NULL) {
		mutex_exit(&tq->tq_lock);
		return (0);
	}
	t->task_next = &tq->tq_task;
	t->task_prev = tq->tq_task.task_prev;
	t->task_next->task_prev = t;
	t->task_prev->task_next = t;
	t->task_func = func;
	t->task_arg = arg;
	cv_signal(&tq->tq_dispatch_cv);
	mutex_exit(&tq->tq_lock);
	return (1);
}

void
taskq_wait(taskq_t *tq)
{
	mutex_enter(&tq->tq_lock);
	while (tq->tq_task.task_next != &tq->tq_task || tq->tq_active != 0)
		cv_wait(&tq->tq_wait_cv, &tq->tq_lock);
	mutex_exit(&tq->tq_lock);
}

static void
taskq_thread(void *arg)
{
	taskq_t *tq = arg;
	task_t *t;
	callb_cpr_t cprinfo;

	if (tq->tq_flags & TASKQ_CPR_SAFE) {
		CALLB_CPR_INIT_SAFE(curthread, tq->tq_name);
	} else {
		CALLB_CPR_INIT(&cprinfo, &tq->tq_lock, callb_generic_cpr,
		    tq->tq_name);
	}
	mutex_enter(&tq->tq_lock);
	while (tq->tq_flags & TASKQ_ACTIVE) {
		if ((t = tq->tq_task.task_next) == &tq->tq_task) {
			if (--tq->tq_active == 0)
				cv_broadcast(&tq->tq_wait_cv);
			if (tq->tq_flags & TASKQ_CPR_SAFE) {
				cv_wait(&tq->tq_dispatch_cv, &tq->tq_lock);
			} else {
				CALLB_CPR_SAFE_BEGIN(&cprinfo);
				cv_wait(&tq->tq_dispatch_cv, &tq->tq_lock);
				CALLB_CPR_SAFE_END(&cprinfo, &tq->tq_lock);
			}
			tq->tq_active++;
			continue;
		}
		t->task_prev->task_next = t->task_next;
		t->task_next->task_prev = t->task_prev;
		mutex_exit(&tq->tq_lock);

		rw_enter(&tq->tq_threadlock, RW_READER);
		t->task_func(t->task_arg);
		rw_exit(&tq->tq_threadlock);

		mutex_enter(&tq->tq_lock);
		task_free(tq, t);
	}
	tq->tq_nthreads--;
	cv_broadcast(&tq->tq_wait_cv);
	ASSERT(!(tq->tq_flags & TASKQ_CPR_SAFE));
	CALLB_CPR_EXIT(&cprinfo);
	thread_exit();
}

krwlock_t *
taskq_lock(taskq_t *tq)
{
	return (&tq->tq_threadlock);
}

taskq_t *
taskq_create(const char *name, int nthreads, pri_t pri,
	int minalloc, int maxalloc, int flags)
{
	taskq_t *tq = kmem_cache_alloc(taskq_cache, KM_SLEEP);

	(void) strncpy(tq->tq_name, name, TASKQ_NAMELEN + 1);
	tq->tq_name[TASKQ_NAMELEN] = '\0';
	tq->tq_flags = flags | TASKQ_ACTIVE;
	tq->tq_active = nthreads;
	tq->tq_nthreads = nthreads;
	tq->tq_minalloc = minalloc;
	tq->tq_maxalloc = maxalloc;

	if (flags & TASKQ_PREPOPULATE) {
		mutex_enter(&tq->tq_lock);
		while (minalloc-- > 0)
			task_free(tq, task_alloc(tq, KM_SLEEP));
		mutex_exit(&tq->tq_lock);
	}

	while (nthreads-- > 0)
		if (thread_create(NULL, 0, taskq_thread, (void *)tq,
		    0, &p0, TS_RUN, pri) == NULL)
			panic("taskq_create failed for %s", name);

	return (tq);
}

void
taskq_destroy(taskq_t *tq)
{
	ASSERT((tq->tq_flags & TASKQ_CPR_SAFE) == 0);

	taskq_wait(tq);

	mutex_enter(&tq->tq_lock);

	tq->tq_flags &= ~TASKQ_ACTIVE;
	cv_broadcast(&tq->tq_dispatch_cv);
	while (tq->tq_nthreads != 0)
		cv_wait(&tq->tq_wait_cv, &tq->tq_lock);

	tq->tq_minalloc = 0;
	while (tq->tq_nalloc != 0)
		task_free(tq, task_alloc(tq, KM_SLEEP));

	mutex_exit(&tq->tq_lock);

	kmem_cache_free(taskq_cache, tq);
}
