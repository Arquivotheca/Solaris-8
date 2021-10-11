/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)tsd.c	1.28	99/08/10 SMI"

#ifdef __STDC__

#pragma weak thr_keycreate = _thr_keycreate
#pragma weak thr_setspecific = _thr_setspecific
#pragma weak thr_getspecific = _thr_getspecific

#pragma weak pthread_key_delete = _thr_key_delete
#pragma weak pthread_key_create = _thr_keycreate
#pragma weak pthread_setspecific = _thr_setspecific
#pragma weak _pthread_key_delete = _thr_key_delete
#pragma weak _pthread_key_create = _thr_keycreate
#pragma weak _pthread_setspecific = _thr_setspecific

#pragma	weak _ti_thr_keycreate = _thr_keycreate
#pragma	weak _ti_thr_setspecific = _thr_setspecific
#pragma	weak _ti_thr_getspecific = _thr_getspecific

#pragma weak _ti_pthread_key_delete = _thr_key_delete
#pragma weak _ti_pthread_key_create = _thr_keycreate
#pragma weak _ti_pthread_setspecific = _thr_setspecific

#endif /* __STDC__ */

#include <stdlib.h>
#include "libthread.h"

#ifdef TLS
static struct tsd_thread tsd_thread;
#pragma unshared(tsd_thread)
#else
#define	tsd_thread	(*(tsd_t *)(curthread->t_tls))
#endif

/*
 * Information common to all threads' TSD.
 */
struct tsd_common tsd_common;

/*
 * Evil implementation-specific knowledge of libc.
 */
#pragma weak _libc_tsd_common
extern struct _libc_tsd_common {
	uint_t	_keys_used;
	uint_t	_keys_allocated;
	PFrV	*_destructors;
	void	**_values;
} _libc_tsd_common;

#define	DELETED	((PFrV)1)

static void nulldestructor(void *);

/*
 * tsd_init() is called from libthread's _init section to transfer
 * thread-specific data from libc to libthread.  This is needed if
 * libthread is dlopen()ed by a single-threaded process after it has
 * already allocated some thread-specific data.
 */
void
tsd_init(uthread_t *t)
{
	struct _libc_tsd_common *libc_tsd = &_libc_tsd_common;
	tsd_t *tsdp;
	int i;

	if (libc_tsd == NULL ||			/* mismatched libc */
	    libc_tsd->_keys_allocated == 0) {	/* or nothing to do */
		t->t_tls = NULL;
		return;
	}
	/*
	 * libc has allocated some TSD; transfer it to the primoridal thread.
	 * It is OK to call calloc() because libc has been initialiced.
	 */
	tsd_common.nkeys = libc_tsd->_keys_used;
	tsd_common.max_keys = libc_tsd->_keys_allocated;
	tsd_common.destructors = libc_tsd->_destructors;
	tsdp = calloc(1, sizeof (tsd_t));
	if (tsdp != NULL) {
		tsdp->count = tsd_common.nkeys;
		tsdp->array = libc_tsd->_values;
	}
	t->t_tls = (char *)tsdp;
	/*
	 * Convert libc's DELETED destructors into nulldestructor()
	 */
	for (i = 0; i < tsd_common.nkeys; i++)
		if (tsd_common.destructors[i] == DELETED)
			tsd_common.destructors[i] = nulldestructor;
	(void) atexit(_destroy_tsd);
	/*
	 * libc is no longer in charge of TSD.
	 * Make its keys_destruct() function do nothing.
	 */
	libc_tsd->_keys_used = 0;
	libc_tsd->_keys_allocated = 0;
	libc_tsd->_destructors = NULL;
	libc_tsd->_values = NULL;
}

/*
 * PROBE_SUPPORT begin
 * interface to register thread probe specific data
 */
void
thr_probe_setup(void *data)
{
	curthread->t_tpdp = data;
}
/* PROBE_SUPPORT end */

/*
 * Null destructor assigned in case the key is deleted
 * It is better to have some proper function address rather
 * than having -1 or some thing like that.
 * if for any key, destructor is found to equal to this function
 * then it is considered invalid (deleted) key.
 */
static void
nulldestructor(void *value)
{
}


/*
 * Should we check to see if key is already allocated?
 */
int
_thr_keycreate(thread_key_t *pkey, PFrV destructor)
{
	PFrV *p;
	int nkeys;

	TRACE_0(UTR_FAC_THREAD, UTR_THR_KEYCREATE_START, "thr_keycreate start");
	/* XXX - This lock can be converted into mutex_lock */
	_rw_wrlock(&tsd_common.lock);
	if (tsd_common.nkeys >= tsd_common.max_keys) {
		if (tsd_common.max_keys == 0) {
			nkeys = 1;
		} else {
			/*
			 * Reallocate, doubling size.
			 */
			nkeys = tsd_common.max_keys * 2;
		}
		p = (PFrV *) realloc(tsd_common.destructors,
			nkeys * sizeof (void *));
		if (p == NULL) {
			_rw_unlock(&tsd_common.lock);
			TRACE_1(UTR_FAC_THREAD, UTR_THR_KEYCREATE_END,
			    "thr_keycreate end:key 0x%x", 0);
			return (ENOMEM);
		}
		tsd_common.max_keys = nkeys;
		tsd_common.destructors = p;
	}
	tsd_common.destructors[tsd_common.nkeys] = destructor;
	*pkey = ++tsd_common.nkeys;
	_rw_unlock(&tsd_common.lock);
	TRACE_1(UTR_FAC_THREAD, UTR_THR_KEYCREATE_END,
	    "thr_keycreate end:key 0x%x", *pkey);
	return (0);
}

int
_thr_key_delete(thread_key_t key)
{
	int nkeys;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_KEY_DELETE_START,
				"thr_delete start:key 0x%x", key);
	if (key == 0) {
		return (EINVAL);
	}

	/* XXX - This lock seems unnecessary, can be removed */
	_rw_rdlock(&tsd_common.lock);
	nkeys = tsd_common.nkeys;
	_rw_unlock(&tsd_common.lock);

	if (key > nkeys ||
	    tsd_common.destructors[key - 1] == nulldestructor) {

		TRACE_0(UTR_FAC_THREAD, UTR_THR_KEY_DELETE_END,
						"thr_delete end");
		return (EINVAL);
	}
	tsd_common.destructors[key - 1] = nulldestructor;

	return (0);
}

int
_thr_getspecific(thread_key_t key, void **valuep)
{
	int nkeys;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_START,
	    "thr_getspecific start:key 0x%x", key);
	if (key == 0) {
		return (EINVAL);
	}

	if (curthread->t_tls == NULL || key > tsd_thread.count) {
		/* XXX - This lock seems unnecessary, can be removed */
		_rw_rdlock(&tsd_common.lock);
		nkeys = tsd_common.nkeys;
		_rw_unlock(&tsd_common.lock);
		if (key > nkeys ||
		    tsd_common.destructors[key - 1] == nulldestructor) {
			TRACE_0(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_END,
					"thr_getspecific end");
			return (EINVAL);
		}
		*valuep = 0;
		TRACE_0(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_END,
			"thr_getspecific end:value 0");
		return (0);
	} else if (tsd_common.destructors[key - 1] == nulldestructor) {
		TRACE_0(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_END,
				"thr_getspecific end");
		return (EINVAL);
	}
	*valuep = tsd_thread.array[key - 1];
	TRACE_1(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_END,
	    "thr_getspecific end:value 0x%x", *valuep);
	return (0);
}

/*
 * This version of _thr_setspecific does not automatically call the
 * destructor for the key, since that is an expensive, irrelevant C++ism that
 * does not belong in a "system interface". If that feature remains in POSIX,
 * then we'll insert this underneath.
 */
int
_thr_setspecific(unsigned int key, void *value)
{
	void **p;
	int nkeys;

	TRACE_2(UTR_FAC_THREAD, UTR_THR_SETSPECIFIC_START,
	    "thr_setspecific start:key 0x%x value 0x%x", key, value);

	if (key == 0) {
		return (EINVAL);
	}


#ifndef TLS
	{
	tsd_t *tsdp;

		if (curthread->t_tls == NULL) {
			tsdp = (tsd_t *)calloc(1, sizeof (tsd_t));
			if (tsdp == NULL)
				return (ENOMEM);
			curthread->t_tls = (char *)tsdp;
		}
	}
#endif

	if (key > tsd_thread.count) {
		/* XXX - This lock seems unnecessary, can be removed */
		_rw_rdlock(&tsd_common.lock);
		nkeys = tsd_common.nkeys;
		_rw_unlock(&tsd_common.lock);
		if (key > nkeys ||
			tsd_common.destructors[key - 1] == nulldestructor) {
			TRACE_0(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_END,
					"thr_setspecific end");
			return (EINVAL);
		}
		p = (void **) realloc(tsd_thread.array,
			nkeys * sizeof (void *));
		if (p == NULL) {
			TRACE_0(UTR_FAC_THREAD, UTR_THR_SETSPECIFIC_END,
			    "thr_setspecific end");
			return (ENOMEM);
		}
		_memset((void *) &p[tsd_thread.count], 0,
		    (nkeys - tsd_thread.count) * sizeof (void *));
		tsd_thread.array = p;
		tsd_thread.count = nkeys;
	} else if (tsd_common.destructors[key - 1] == nulldestructor) {
		TRACE_0(UTR_FAC_THREAD, UTR_THR_GETSPECIFIC_END,
				"thr_setspecific end");
		return (EINVAL);
	}
	tsd_thread.array[key - 1] = value;
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SETSPECIFIC_END, "thr_setspecific end");
	return (0);
}

/*
 * Currently, there's a couple of glitches in the POSIX 1/26/1990
 * spec for thread behavior. I'll pick some semantics that ought
 * to usually yield tolerable behavior.
 *
 * SECOND NOTE -- Those glitches remain in POSIX 8/10/1990; the
 * order in which destructors are run remains undefined.
 *
 * What I mean by "tolerable behavior":
 *
 * It's not possible to know quite what a destructor might choose to
 * do, and it is easily argued that a destructor might choose to make
 * use of other thread-specific-data (and it is difficult and expensive
 * to prevent this). It is also the case that there's no point
 * running a destructor more than once on the same value, and there's
 * no point in running a destructor at all on zero.
 *
 * Therefore, what we do is this: if there is a destructor, and the
 * value is not zero, then zero the value and run the destructor on
 * the old value. as long as there is a destructor-key pair that
 * meets this test, continue running destructors.
 *
 * This creates the possibility of an infinite loop, but allows great
 * freedom in the use of constructors, destructors, and
 * thread-specific-data. "We provide rope."
 *
 * SECOND NOTE -- More elaborate justification for why it should be
 * done this way: TSD client software using destructors written in
 * this style need only deal with two possible TSD "states". The
 * first state is "my TSD is zero, initialize it", and the second
 * state is "my TSD is not zero, use it". IF TSD cannot be used
 * during _thr_exit, then at least three states result, maybe four
 * -- 0, ~0, 0&exiting, ~0&exiting. Ponder building a thread-local
 * cache on top of that for a minute; clearly, the two-state
 * semantics are easier. Infinite loops at exit are still a problem,
 * but those can be dealt with by studying the use of resources among
 * the various TSD clients.
 *
 * Note too that correct code written for some pathological TSD
 * implementation (one that does not permit thread_setspecific to be
 * called during thread exit, for example) will work correctly and
 * efficiently using this implementation. Thus, we don't any
 * problems for people porting code to this implementation.
 *
 * A paranoid person might decrement a count or something to break the
 * infinite loop. At least tsd_thread.count iterations of the big loop
 * should run.
 */

/*
 * Called from thread_exit() to destroy TSD.
 *
 * Note: we don't need locking here because we are either dealing
 * with thread-local data or the common destructors. The locks in
 * thread_setspecific ensure that the destructors will settle before
 * the common nkeys variable is interrogated locally and we know that
 * the per-thread count of valid keys will not increase beyond the
 * interrogated value of the common nkeys variable.
 */
void
_destroy_tsd(void)
{
	PFrV	func;
	void	*value;
	int	new_tsd;
	int	i;

#ifndef TLS
	if (curthread->t_tls == NULL)
		return;
#endif
	do {
		new_tsd = 0;
		for (i = 0; i < tsd_thread.count; i++) {
			_rw_rdlock(&tsd_common.lock);
			func = tsd_common.destructors[i];
			_rw_unlock(&tsd_common.lock);
			value = tsd_thread.array[i];
			if (func && value) {
				tsd_thread.array[i] = NULL;
				(*func) (value);
				/*
				 * New TSD may appear during calls to
				 * destructors, so check again.
				 */
				new_tsd = 1;
			}
		}
	} while (new_tsd);

	free(tsd_thread.array);
	tsd_thread.array = NULL;
#ifndef TLS
	free(&(tsd_thread));
	curthread->t_tls = NULL;
#endif

}


#if defined(UTRACE) || defined(ITRACE)

#ifndef TLS
int
trace_thr_keycreate(thread_key_t *pkey, PFrV destructor)
{
	PFrV *p;
	int nkeys;
	tsd_t *tsdp;

	if (curthread->t_tls == NULL) {
		tsdp = (tsd_t *)calloc(1, sizeof (tsd_t));
		if (tsdp == NULL)
			return (ENOMEM);
		curthread->t_tls = (char *)tsdp;
	}
	trace_rw_wrlock(&tsd_common.lock);
	if (*pkey != 0) {
		trace_rw_unlock(&tsd_common.lock);
		return (0);
	}
	if (tsd_common.nkeys >= tsd_common.max_keys) {
		if (tsd_common.max_keys == 0) {
			nkeys = 1;
		} else {
			/*
			 * Reallocate, doubling size.
			 */
			nkeys = tsd_common.max_keys * 2;
		}
		p = (PFrV *) realloc(tsd_common.destructors,
			nkeys * sizeof (void *));
		if (p == NULL) {
			trace_rw_unlock(&tsd_common.lock);
			return (ENOMEM);
		}
		tsd_common.max_keys = nkeys;
		tsd_common.destructors = p;
	}
	tsd_common.destructors[tsd_common.nkeys] = destructor;
	*pkey = ++tsd_common.nkeys;
	trace_rw_unlock(&tsd_common.lock);
	return (0);
}

int
trace_thr_getspecific(thread_key_t key, void **valuep)
{
	if (curthread->t_tls == NULL)
		return (EINVAL);
	if (key == 0 || key > tsd_thread.count) {
		*valuep = 0;
		return (EINVAL);
	}
	*valuep = tsd_thread.array[key - 1];
	return (0);
}

/*
 * This version of _thr_setspecific does not automatically call the
 * destructor for the key, since that is an expensive, irrelevant C++ism that
 * does not belong in a "system interface". If that feature remains in POSIX,
 * then we'll insert this underneath.
 */
int
trace_thr_setspecific(unsigned int key, void *value)
{
	void **p;
	int nkeys;

	if (curthread->t_tls == NULL)
		return (EINVAL);
	if (key > tsd_thread.count) {
		/*
		 * The lock here ensures that we see the most recent
		 * value of tsd_common.nkeys, and that the destructor
		 * array is up to date wrt tsd_common.nkeys.
		 */
		trace_rw_rdlock(&tsd_common.lock);
		nkeys = tsd_common.nkeys;
		trace_rw_unlock(&tsd_common.lock);
		if (key > nkeys) {
			return (EINVAL);
		}
		p = (void **) realloc(tsd_thread.array,
			nkeys * sizeof (void *));
		if (p == NULL) {
			return (ENOMEM);
		}
		_memset((void *) &p[tsd_thread.count], 0,
		    (nkeys - tsd_thread.count) * sizeof (void *));
		tsd_thread.array = p;
		tsd_thread.count = nkeys;
	}
	tsd_thread.array[key - 1] = value;
	return (0);
}
#endif
#endif
