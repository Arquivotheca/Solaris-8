/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)nice.c	1.2	94/09/13 SMI"	/* from SVr4.0 1.15 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/class.h>
#include <sys/mutex.h>

/*
 * We support the nice system call for compatibility although
 * the priocntl system call supports a superset of nice's functionality.
 * We support nice only for time sharing threads.  It will fail
 * if called by a thread from another class.
 */

int
nice(int niceness)
{
	int error = 0;
	int err, retval;
	kthread_id_t t;
	proc_t	*p = curproc;

	mutex_enter(&p->p_lock);
	t = p->p_tlist;
	do {
		err = CL_DONICE(t, CRED(), niceness, &retval);
		if (error == 0 && err)
			error = set_errno(err);
	} while ((t = t->t_forw) != p->p_tlist);
	mutex_exit(&p->p_lock);
	if (error)
		return (error);
	return (retval);
}
