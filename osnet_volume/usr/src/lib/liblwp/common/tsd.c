/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tsd.c	1.2	99/12/06 SMI"

#include "liblwp.h"

/*
 * Interfaces for thread-specific data (tsd)
 */

tsd_common_t tsd_common;	/* known to libthread_db */
mutex_t	tsd_create_mtx = DEFAULTMUTEX;

/* ARGSUSED */
void
null_destructor(void *arg)
{
}

/*
 * Evil implementation-specific knowledge of libc.
 */
#pragma weak _libc_tsd_common
extern struct _libc_tsd_common {
	uint_t	_keys_used;
	uint_t	_keys_allocated;
	void	(**_destructors)(void *);
	void	**_values;
} _libc_tsd_common;

#define	DELETED	((void (*)(void *))1)

/*
 * tsd_init() is called from libthread's _init section to transfer
 * thread-specific data from libc to libthread.  This is needed if
 * libthread is dlopen()ed by a single-threaded process after it has
 * already allocated some thread-specific data.
 */
void
tsd_init()
{
	struct _libc_tsd_common *libc_tsd = &_libc_tsd_common;
	void (*destructor)(void *);
	int i;

	if (libc_tsd == NULL ||
	    libc_tsd->_keys_allocated == 0)	/* nothing to do */
		return;
	/*
	 * libc has allocated some TSD; transfer it to the primoridal thread.
	 */
	tsd_common.numkeys = libc_tsd->_keys_used;
	tsd_common.maxkeys = libc_tsd->_keys_allocated;
	tsd_common.destructors = libc_tsd->_destructors;
	ulwp_one.ul_tsd.nkey = libc_tsd->_keys_used;
	ulwp_one.ul_tsd.tsd = libc_tsd->_values;
	/*
	 * Convert libc's NULL and DELETED destructors.
	 */
	for (i = 0; i < tsd_common.numkeys; i++) {
		if ((destructor = tsd_common.destructors[i]) == NULL)
			tsd_common.destructors[i] = null_destructor;
		else if (destructor == DELETED)
			tsd_common.destructors[i] = NULL;
	}
	(void) atexit(tsd_exit);
	/*
	 * libc is no longer in charge of TSD.
	 * Make its keys_destruct() function do nothing.
	 */
	libc_tsd->_keys_used = 0;
	libc_tsd->_keys_allocated = 0;
	libc_tsd->_destructors = NULL;
	libc_tsd->_values = NULL;
}

#pragma weak thr_keycreate = _thr_keycreate
#pragma weak _liblwp_thr_keycreate = _thr_keycreate
#pragma weak pthread_key_create = _thr_keycreate
#pragma weak _pthread_key_create = _thr_keycreate
#pragma weak _liblwp_pthread_key_create = _thr_keycreate
int
_thr_keycreate(thread_key_t *keyp, void (*destructor)(void *))
{
	void (**old_destructors)(void *) = NULL;
	int old_maxkeys;
	void (**my_destructors)(void *);
	int my_maxkeys;

	lmutex_lock(&tsd_create_mtx);
	while (tsd_common.numkeys == tsd_common.maxkeys) {
		old_destructors = tsd_common.destructors;
		old_maxkeys = tsd_common.maxkeys;
		lmutex_unlock(&tsd_create_mtx);

		/* double the size on each new allocation */
		if ((my_maxkeys = old_maxkeys) == 0)
			my_maxkeys = 2;
		else
			my_maxkeys *= 2;
		my_destructors = malloc(my_maxkeys * sizeof (void (*)(void *)));
		if (my_destructors == NULL)
			return (ENOMEM);
		(void) _memset(my_destructors, 0,
			my_maxkeys * sizeof (void (*)(void *)));

		lmutex_lock(&tsd_create_mtx);
		if (old_maxkeys == tsd_common.maxkeys) {
			if (old_destructors != NULL)
				(void) _memcpy(my_destructors, old_destructors,
				    old_maxkeys * sizeof (void (*)(void *)));
			tsd_common.destructors = my_destructors;
			tsd_common.maxkeys = my_maxkeys;
		} else {
			/* another thread got here first */
			lmutex_unlock(&tsd_create_mtx);
			liblwp_free(my_destructors);
			old_destructors = NULL;
			lmutex_lock(&tsd_create_mtx);
		}
	}
	if (destructor == NULL)
		destructor = null_destructor;
	tsd_common.destructors[tsd_common.numkeys] = destructor;
	*keyp = ++tsd_common.numkeys;	/* keys start at 1; 0 is not valid */
	lmutex_unlock(&tsd_create_mtx);
	if (old_destructors != NULL)
		liblwp_free(old_destructors);
	return (0);
}

#pragma weak thr_key_delete = _thr_key_delete
#pragma weak _liblwp_thr_key_delete = _thr_key_delete
#pragma weak pthread_key_delete = _thr_key_delete
#pragma weak _pthread_key_delete = _thr_key_delete
#pragma weak _liblwp_pthread_key_delete = _thr_key_delete
int
_thr_key_delete(thread_key_t key)
{
	lmutex_lock(&tsd_create_mtx);
	if (key == 0 || key > tsd_common.numkeys ||
	    tsd_common.destructors[key-1] == NULL) {
		lmutex_unlock(&tsd_create_mtx);
		return (EINVAL);
	}
	tsd_common.destructors[key-1] = NULL;
	lmutex_unlock(&tsd_create_mtx);
	return (0);
}

#pragma weak thr_setspecific = _thr_setspecific
#pragma weak _liblwp_thr_setspecific = _thr_setspecific
#pragma weak pthread_setspecific = _thr_setspecific
#pragma weak _pthread_setspecific = _thr_setspecific
#pragma weak _liblwp_pthread_setspecific = _thr_setspecific
int
_thr_setspecific(thread_key_t key, void *value)
{
	ulwp_t *self = curthread;
	void **free_tsd = NULL;
	void **new_tsd;
	int nkey;

	if (key == 0 || key > tsd_common.numkeys ||
	    tsd_common.destructors[key-1] == NULL)
		return (EINVAL);

	enter_critical();	/* defer signals */
	if (key > self->ul_tsd.nkey) {
		if (value == NULL) {
			exit_critical();
			return (0);
		}
		if ((nkey = self->ul_tsd.nkey) == 0)
			nkey = 2;
		while (key > nkey)	/* next higher power of two */
			nkey *= 2;

		/*
		 * We must allow signals while calling malloc(), else we
		 * would be fork1() unsafe here.  Likewise, we must defer
		 * freeing memory until we are out of the critical region.
		 */
		exit_critical();
		if ((new_tsd = malloc(nkey * sizeof (void *))) == NULL)
			return (ENOMEM);
		enter_critical();

		/*
		 * Reverify -- thr_setspecific() might have been
		 * called from an application signal handler.
		 */
		if (nkey <= self->ul_tsd.nkey)
			free_tsd = new_tsd;
		else {
			(void) _memset(new_tsd, 0, nkey * sizeof (void *));
			if ((free_tsd = self->ul_tsd.tsd) != NULL) {
				(void) _memcpy(new_tsd, free_tsd,
					self->ul_tsd.nkey * sizeof (void *));
			}
			self->ul_tsd.tsd = new_tsd;
			self->ul_tsd.nkey = nkey;
		}
	}
	self->ul_tsd.tsd[key-1] = value;
	exit_critical();
	if (free_tsd)
		liblwp_free(free_tsd);
	return (0);
}

#pragma weak thr_getspecific = _thr_getspecific
#pragma weak _liblwp_thr_getspecific = _thr_getspecific
int
_thr_getspecific(thread_key_t key, void **valuep)
{
	ulwp_t *self = curthread;

	if (key == 0 || key > tsd_common.numkeys ||
	    tsd_common.destructors[key-1] == NULL)
		return (EINVAL);

	enter_critical();	/* defer signals */
	if (key > self->ul_tsd.nkey)
		*valuep = NULL;
	else
		*valuep = self->ul_tsd.tsd[key-1];
	exit_critical();
	return (0);
}

/*
 * pthread_getspecific(): Get the tsd value for a specific key value.
 * It is same as thr_getspecific() except that the value is passed back as
 * the return value whereas thr_getspecific() passes it through an argument.
 */
#pragma weak	pthread_getspecific		= _pthread_getspecific
#pragma weak	_liblwp_pthread_getspecific	= _pthread_getspecific
void *
_pthread_getspecific(pthread_key_t key)
{
	void	*value;

	if (_thr_getspecific(key, &value) != 0)
		return (NULL);
	return (value);
}

/*
 * This is called by _thrp_exit() to apply destructors to the thread's tsd.
 */
void
tsd_exit()
{
	ulwp_t *self = curthread;
	void *value;
	void (*destructor)(void *);
	int i;
	int new_tsd;
#if 0
	int count = 0;
#endif

	do {
		for (new_tsd = 0, i = 0;
		    i < tsd_common.numkeys && i < self->ul_tsd.nkey; i++) {
			if ((value = self->ul_tsd.tsd[i]) != NULL &&
			    (destructor = tsd_common.destructors[i]) != NULL) {
				self->ul_tsd.tsd[i] = NULL;
				/*
				 * New TSD may appear during calls to
				 * destructors.  Repeat the scan.
				 */
				if (destructor != null_destructor) {
					destructor(value);
					new_tsd = 1;
				}
			}
		}
#if 0	/* XXX: do we need/want to do this? */
		if (_libpthread_loaded &&
		    ++count == _POSIX_THREAD_DESTRUCTOR_ITERATIONS)
			break;
#endif
	} while (new_tsd);

	if (self->ul_tsd.tsd)
		liblwp_free(self->ul_tsd.tsd);
	self->ul_tsd.nkey = 0;
	self->ul_tsd.tsd = NULL;
}
