/*
 * Copyright (c) 1998, Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)profil.c	1.6	98/01/29 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/debug.h>

/*
 * Profiling.
 */
int
profil(unsigned short *bufbase, size_t bufsize, u_long pcoffset, u_int pcscale)
{
	struct proc *p = ttoproc(curthread);

	if (pcscale == 1)
		pcscale = 0;

	mutex_enter(&p->p_pflock);
	p->p_prof.pr_base = bufbase;
	p->p_prof.pr_size = bufsize;
	p->p_prof.pr_off = pcoffset;
	p->p_prof.pr_scale = pcscale;

	/* pcsample and profil are mutually exclusive */
	p->p_prof.pr_samples = 0;

	mutex_exit(&p->p_pflock);
	mutex_enter(&p->p_lock);
	set_proc_post_sys(p);	/* activate post_syscall profiling code */
	mutex_exit(&p->p_lock);
	return (0);
}


/*
 * PC Sampling
 */
long
pcsample(void *buf, long nsamples)
{
	struct proc *p = ttoproc(curthread);
	long count = 0;

	if (nsamples < 0 ||
	    ((get_udatamodel() != DATAMODEL_NATIVE) && (nsamples > INT32_MAX)))
		return (set_errno(EINVAL));

	mutex_enter(&p->p_pflock);
	p->p_prof.pr_base = buf;
	p->p_prof.pr_size = nsamples;
	p->p_prof.pr_scale = 1;
	count = p->p_prof.pr_samples;
	p->p_prof.pr_samples = 0;
	mutex_exit(&p->p_pflock);

	mutex_enter(&p->p_lock);
	set_proc_post_sys(p);	/* activate post_syscall profiling code */
	mutex_exit(&p->p_lock);

	return (count);
}
