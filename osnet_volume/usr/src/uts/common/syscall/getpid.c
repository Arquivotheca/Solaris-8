/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getpid.c	1.3	97/09/22 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/debug.h>


int64_t
getpid(void)
{
	rval_t	r;
	proc_t	*p;

	p = ttoproc(curthread);
	r.r_val1 = p->p_pid;
	r.r_val2 = p->p_ppid;
	return (r.r_vals);
}
