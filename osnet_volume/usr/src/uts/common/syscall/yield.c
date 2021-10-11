/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)yield.c	1.3	96/05/20 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>


/*
 * The calling LWP is preempted in favor of some other LWP.
 */
int
yield()
{
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);

	thread_lock(t);
	lwp->lwp_ru.nivcsw++;
	THREAD_TRANSITION(t);
	CL_YIELD(t);		/* does setbackdq */

	/*
	 * update stat under thread_lock so no migration can occur.
	 */
	CPU_STAT_ADDQ(CPU, cpu_sysinfo.inv_swtch, 1);
	thread_unlock_nopreempt(t);
	swtch();		/* clears cpu_runrun and cpu_kprunrun */

	return (0);
}
