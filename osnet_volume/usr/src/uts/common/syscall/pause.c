/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pause.c	1.2	94/10/13 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/debug.h>



/*
 * Indefinite wait.  No one should call wakeup() or t_release_all_chan()
 * with a chan of &u.
 */
int
pause()
{
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	while (cv_wait_sig_swap(&u.u_cv, &p->p_lock))
		;
	mutex_exit(&p->p_lock);
	return (set_errno(EINTR));
}
