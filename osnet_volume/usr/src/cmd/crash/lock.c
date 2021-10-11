/*
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lock.c	1.15	98/03/30 SMI"

/*
 * This file contains code for the crash functions: mutex, sema, rwlock, cv.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/mutex_impl.h>
#include <sys/sema_impl.h>
#include <sys/rwlock_impl.h>
#include <sys/condvar_impl.h>
#include "crash.h"

int
getmutexinfo()
{
	long addr;
	kmutex_t mb;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			readbuf((void *)addr, 0, 0, &mb, sizeof (mb),
			    "mutex");
			prmutex(&mb);
		} while (args[optind]);
	} else longjmp(syn, 0);
	return (0);
}

void
prmutex(kmutex_t *m)
{
	mutex_impl_t *mp = (mutex_impl_t *)m;

	if (MUTEX_TYPE_SPIN(mp)) {
		fprintf(fp, " minspl %x oldspl %x lock %x\n",
		    mp->m_spin.m_minspl, mp->m_spin.m_oldspl,
		    mp->m_spin.m_spinlock);
	} else {
		kthread_id_t owner = MUTEX_OWNER(mp);
		if (owner == MUTEX_NO_OWNER)
			owner = NULL;
		fprintf(fp, " owner %p waiters %lu\n",
		    mp->m_owner, MUTEX_HAS_WAITERS(mp));
	}
}

int
getsemainfo(void)
{
	long addr;
	int c;
	struct _ksema sb;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			readbuf((void *)addr, 0, 0, &sb, sizeof (sb),
			    "semaphore");
			prsema(&sb);
		} while (args[optind]);
	} else longjmp(syn, 0);
	return (0);
}

void
prsema(struct _ksema *sp)
{
	sema_impl_t *s	= (sema_impl_t *)sp;

	fprintf(fp, "\tcount %d waiting: 0x%p\n",
		s->s_count, s->s_slpq);
}

int
getrwlockinfo(void)
{
	long addr;
	struct _krwlock rwb;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			readbuf((void *)addr, 0, 0, &rwb, sizeof (rwb),
			    "rwlock");
			prrwlock(&rwb);
		} while (args[optind]);
	} else longjmp(syn, 0);
	return (0);
}

void
prrwlock(struct _krwlock *rwp)
{
	rwlock_impl_t	*rp = (rwlock_impl_t *)rwp;

	fprintf(fp, "\towner %lu, holdcnt %lu, waiters %lu, writewanted %lu\n",
		(rp->rw_wwwh & RW_WRITE_LOCKED) ?
		    (rp->rw_wwwh & RW_OWNER) : 0,
		(rp->rw_wwwh & RW_WRITE_LOCKED) ?
		    -1 : rp->rw_wwwh >> RW_HOLD_COUNT_SHIFT,
		(rp->rw_wwwh & RW_HAS_WAITERS),
		(rp->rw_wwwh & RW_WRITE_WANTED) != 0);
}

void
prcondvar(struct _kcondvar *cvp, char *cv_name)
{
	condvar_impl_t *cv = (condvar_impl_t *)cvp;

	fprintf(fp, "Condition variable %s: %d\n", cv_name, cv->cv_waiters);
}
