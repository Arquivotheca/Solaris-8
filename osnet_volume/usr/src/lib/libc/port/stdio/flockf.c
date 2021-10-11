/*
 * Copyright (c) 1992,1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)flockf.c	1.26	99/10/25 SMI"	/* SVr4.0 1.2   */

/* LINTLIBRARY */

#pragma weak flockfile = _flockfile
#pragma weak ftrylockfile = _ftrylockfile
#pragma weak funlockfile = _funlockfile

#define	flockfile _flockfile
#define	ftrylockfile _ftrylockfile
#define	funlockfile _funlockfile

#include "synonyms.h"
#include "mtlib.h"
#include "file64.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio_ext.h>
#include "stdiom.h"

/*
 * The _rmutex_lock/_rmutex_unlock routines are only called (within libc !)
 * by _flockget, _flockfile, and _flockrel, _funlockfile, respectively.
 * _flockget and _flockrel are only called by the FLOCKFILE/FUNLOCKFILE
 * macros in mtlib.h. We place the "if (__threaded)" check for
 * threads there, and remove it from:
 *	_rmutex_lock(rm)
 *	_rmutex_unlock(rm)
 *	_flockget(FILE *iop)
 *	_ftrylockfile(FILE *iop)
 */

static void
_rmutex_lock(rmutex_t *rm)
{
	thread_t self = _thr_self();
	mutex_t *lk = &rm->_mutex;

	(void) _mutex_lock(lk);
	if (rm->_owner != 0 && rm->_owner != self) {
		rm->_wait_cnt++;
		do {
			(void) _cond_wait(&rm->_cond, lk);
		} while (rm->_owner != 0 && rm->_owner != self);
		rm->_wait_cnt--;
	}
	/* lock is now available to this thread */
	rm->_owner = self;
	rm->_lock_cnt++;
	(void) _mutex_unlock(lk);
}


int
_rmutex_trylock(rmutex_t *rm)
{
	/*
	 * Treat like a stub if not linked with libthread
	 * as indicated by the __threaded variable.
	 */
	if (__threaded) {
		thread_t self = _thr_self();
		mutex_t *lk = &rm->_mutex;

		(void) _mutex_lock(lk);
		if (rm->_owner != 0 && rm->_owner != self) {
			(void) _mutex_unlock(lk);
			return (-1);
		}
		/* lock is now available to this thread */
		rm->_owner = self;
		rm->_lock_cnt++;
		(void) _mutex_unlock(lk);
	}
	return (0);
}


/*
 * recursive mutex unlock function
 */

static void
_rmutex_unlock(rmutex_t *rm)
{
	thread_t self = _thr_self();
	mutex_t *lk = &rm->_mutex;

	(void) _mutex_lock(lk);
	if (rm->_owner == self) {
		rm->_lock_cnt--;
		if (rm->_lock_cnt == 0) {
			rm->_owner = 0;
			if (rm->_wait_cnt)
				(void) cond_signal(&rm->_cond);
		}
	} else {
		(void) abort();
	}
	(void) _mutex_unlock(lk);
}


/*
 * compute the lock's position, acquire it and return its pointer
 */

rmutex_t *
_flockget(FILE *iop)
{
	rmutex_t *rl = NULL;

	rl = IOB_LCK(iop);
	_rmutex_lock(rl);
	return (rl);
}


/*
 * POSIX.1c version of ftrylockfile().
 * It returns 0 if it gets the lock else returns -1 to indicate the error.
 */

int
_ftrylockfile(FILE *iop)
{
	return (_rmutex_trylock(IOB_LCK(iop)));
}


void
_flockrel(rmutex_t *rl)
{
	_rmutex_unlock(rl);
}


void
flockfile(FILE *iop)
{
	_rmutex_lock(IOB_LCK(iop));
}


void
funlockfile(FILE *iop)
{
	_rmutex_unlock(IOB_LCK(iop));
}

int
__fsetlocking(FILE *iop, int type)
{
	int	ret = 0;

	ret = GET_IONOLOCK(iop) ? FSETLOCKING_BYCALLER : FSETLOCKING_INTERNAL;

	switch (type) {

	case FSETLOCKING_QUERY:
		break;

	case FSETLOCKING_INTERNAL:
		CLEAR_IONOLOCK(iop);
		break;

	case FSETLOCKING_BYCALLER:
		SET_IONOLOCK(iop);
		break;

	default:
		errno = EINVAL;
		return (-1);
		/* break; causes compiler warnings */
	}

	return (ret);
}
