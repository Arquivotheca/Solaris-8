/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rt.c	1.68	99/07/29 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/priocntl.h>
#include <sys/class.h>
#include <sys/disp.h>
#include <sys/procset.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/rt.h>
#include <sys/rtpriocntl.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cpuvar.h>
#include <sys/vmsystm.h>
#include <sys/time.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static pri_t	rt_init(id_t, int, classfuncs_t **);

static struct sclass csw = {
	"RT",
	rt_init,
	0
};

extern struct mod_ops mod_schedops;

/*
 * Module linkage information for the kernel.
 */
static struct modlsched modlsched = {
	&mod_schedops, "realtime scheduling class", &csw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsched, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	return (EBUSY);		/* don't remove RT for now */
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Class specific code for the real-time class
 */

/*
 * Extern declarations for variables defined in the rt master file
 */
#define	RTMAXPRI 59
#define	RTNPROCS 60

rtdpent_t *rt_dptbl;	  /* real-time dispatcher parameter table */

pri_t rt_maxpri = RTMAXPRI;	/* maximum real-time priority */
int rt_nprocs = RTNPROCS;

#define	RTPMEMSZ	1024	/* request size for rtproc memory allocation */

static int	rt_admin(caddr_t, cred_t *);
static int	rt_enterclass(kthread_id_t, id_t, void *, cred_t *, void *);
static int	rt_fork(kthread_id_t, kthread_id_t, void *);
static int	rt_getclinfo(void *);
static int	rt_getclpri(pcpri_t *);
static int	rt_parmsin(void *, id_t, cred_t *, id_t, cred_t *, void *);
static int	rt_parmsout(void *, id_t, cred_t *, id_t, cred_t *, void *);
static int	rt_parmsset(kthread_id_t, void *, id_t, cred_t *);
static int	rt_donice(kthread_id_t, cred_t *, int, int *);
static void	rt_exitclass(void *);
static int	rt_canexit(kthread_id_t, cred_t *);
static void	rt_forkret(kthread_id_t, kthread_id_t);
static void	rt_nullsys();
static void	rt_parmsget(kthread_id_t, void *);
static void	rt_preempt(kthread_id_t);
static void	rt_setrun(kthread_id_t);
static void	rt_tick(kthread_id_t);
static void	rt_wakeup(kthread_id_t);
static pri_t	rt_swappri(kthread_id_t, int);
static pri_t	rt_globpri(kthread_id_t);
static void	rt_yield(kthread_id_t);
static int	rt_alloc(void **, int);
static void	rt_free(void *);

static id_t	rt_cid;		/* real-time class ID */
static rtproc_t	rt_plisthead;	/* dummy rtproc at head of rtproc list */
static kmutex_t	rt_dptblock;	/* protects realtime dispatch table */
static kmutex_t	rt_list_lock;	/* protects RT thread list */

extern rtdpent_t *rt_getdptbl(void);

static struct classfuncs rt_classfuncs = {
	/* class ops */
	rt_admin,
	rt_getclinfo,
	rt_parmsin,
	rt_parmsout,
	rt_getclpri,
	rt_alloc,
	rt_free,
	/* thread ops */
	rt_enterclass,
	rt_exitclass,
	rt_canexit,
	rt_fork,
	rt_forkret,
	rt_parmsget,
	rt_parmsset,
	rt_nullsys,	/* stop */
	rt_nullsys,	/* exit */
	rt_nullsys,	/* active */
	rt_nullsys,	/* inactive */
	rt_swappri,
	rt_swappri,
	rt_nullsys,
	rt_preempt,
	rt_setrun,
	rt_nullsys,
	rt_tick,
	rt_wakeup,
	rt_donice,
	rt_globpri,
	rt_nullsys,
	rt_yield,
};

/*
 * Real-time class initialization. Called by dispinit() at boot time.
 * We can ignore the clparmsz argument since we know that the smallest
 * possible parameter buffer is big enough for us.
 */
/* ARGSUSED */
pri_t
rt_init(cid, clparmsz, clfuncspp)
id_t		cid;
int		clparmsz;
classfuncs_t	**clfuncspp;
{
	rt_dptbl = rt_getdptbl();
	rt_cid = cid;	/* Record our class ID */

	/*
	 * Initialize the rtproc list.
	 */
	rt_plisthead.rt_next = rt_plisthead.rt_prev = &rt_plisthead;

	/*
	 * We're required to return a pointer to our classfuncs
	 * structure and the highest global priority value we use.
	 */
	*clfuncspp = &rt_classfuncs;
	mutex_init(&rt_dptblock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&rt_list_lock, NULL, MUTEX_DEFAULT, NULL);
	return (rt_dptbl[rt_maxpri].rt_globpri);
}

/*
 * Get or reset the rt_dptbl values per the user's request.
 */
/* ARGSUSED */
static int
rt_admin(uaddr, reqpcredp)
caddr_t	uaddr;
cred_t	*reqpcredp;
{
	rtadmin_t	rtadmin;
	rtdpent_t	*tmpdpp;
	size_t		userdpsz;
	size_t		rtdpsz;
	int		i;

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(uaddr, &rtadmin, sizeof (rtadmin_t)))
			return (EFAULT);
	}
#ifdef _SYSCALL32_IMPL
	else {
		/* rtadmin struct from ILP32 callers */
		rtadmin32_t rtadmin32;
		if (copyin(uaddr, &rtadmin32, sizeof (rtadmin32_t)))
			return (EFAULT);
		rtadmin.rt_dpents = (struct rtdpent *)rtadmin32.rt_dpents;
		rtadmin.rt_ndpents = rtadmin32.rt_ndpents;
		rtadmin.rt_cmd = rtadmin32.rt_cmd;
	}
#endif /* _SYSCALL32_IMPL */

	rtdpsz = (rt_maxpri + 1) * sizeof (rtdpent_t);

	switch (rtadmin.rt_cmd) {

	case RT_GETDPSIZE:
		rtadmin.rt_ndpents = rt_maxpri + 1;

		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyout(&rtadmin, uaddr, sizeof (rtadmin_t)))
				return (EFAULT);
		}
#ifdef _SYSCALL32_IMPL
		else {
			/* return rtadmin struct to ILP32 callers */
			rtadmin32_t rtadmin32;
			rtadmin32.rt_dpents = (caddr32_t)rtadmin.rt_dpents;
			rtadmin32.rt_ndpents = rtadmin.rt_ndpents;
			rtadmin32.rt_cmd = rtadmin.rt_cmd;
			if (copyout(&rtadmin32, uaddr, sizeof (rtadmin32_t)))
				return (EFAULT);
		}
#endif /* _SYSCALL32_IMPL */

		break;

	case RT_GETDPTBL:
		userdpsz = MIN(rtadmin.rt_ndpents * sizeof (rtdpent_t),
		    rtdpsz);
		if (copyout(rt_dptbl, rtadmin.rt_dpents, userdpsz))
			return (EFAULT);
		rtadmin.rt_ndpents = userdpsz / sizeof (rtdpent_t);

		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyout(&rtadmin, uaddr, sizeof (rtadmin_t)))
				return (EFAULT);
		}
#ifdef _SYSCALL32_IMPL
		else {
			/* return rtadmin struct to ILP32 callers */
			rtadmin32_t rtadmin32;
			rtadmin32.rt_dpents = (caddr32_t)rtadmin.rt_dpents;
			rtadmin32.rt_ndpents = rtadmin.rt_ndpents;
			rtadmin32.rt_cmd = rtadmin.rt_cmd;
			if (copyout(&rtadmin32, uaddr, sizeof (rtadmin32_t)))
				return (EFAULT);
		}
#endif /* _SYSCALL32_IMPL */
		break;

	case RT_SETDPTBL:
		/*
		 * We require that the requesting process have super user
		 * priveleges.  We also require that the table supplied by
		 * the user exactly match the current rt_dptbl in size.
		 */
		if (!suser(reqpcredp))
			return (EPERM);
		if (rtadmin.rt_ndpents * sizeof (rtdpent_t) != rtdpsz)
			return (EINVAL);

		/*
		 * We read the user supplied table into a temporary buffer
		 * where the time quantum values are validated before
		 * being copied to the rt_dptbl.
		 */
		tmpdpp = kmem_alloc(rtdpsz, KM_SLEEP);
		if (copyin(rtadmin.rt_dpents, tmpdpp, rtdpsz)) {
			kmem_free(tmpdpp, rtdpsz);
			return (EFAULT);
		}
		for (i = 0; i < rtadmin.rt_ndpents; i++) {

			/*
			 * Validate the user supplied time quantum values.
			 */
			if (tmpdpp[i].rt_quantum <= 0 &&
			    tmpdpp[i].rt_quantum != RT_TQINF) {
				kmem_free(tmpdpp, rtdpsz);
				return (EINVAL);
			}
		}

		/*
		 * Copy the user supplied values over the current rt_dptbl
		 * values.  The rt_globpri member is read-only so we don't
		 * overwrite it.
		 */
		mutex_enter(&rt_dptblock);
		for (i = 0; i < rtadmin.rt_ndpents; i++)
			rt_dptbl[i].rt_quantum = tmpdpp[i].rt_quantum;
		mutex_exit(&rt_dptblock);
		kmem_free(tmpdpp, rtdpsz);
		break;

	default:
		return (EINVAL);
	}
	return (0);
}


/*
 * Allocate a real-time class specific proc structure and
 * initialize it with the parameters supplied. Also move thread
 * to specified real-time priority.
 */
/* ARGSUSED */
static int
rt_enterclass(t, cid, parmsp, reqpcredp, bufp)
	kthread_id_t	t;
	id_t		cid;
	void		*parmsp;
	cred_t		*reqpcredp;
	void		*bufp;
{
	register rtkparms_t	*rtkparmsp = (rtkparms_t *)parmsp;
	register rtproc_t	*rtpp;

	/*
	 * For a thread to enter the real-time class the thread
	 * which initiates the request must be super-user.
	 * This may have been checked previously but if our
	 * caller passed us a credential structure we assume it
	 * hasn't and we check it here.
	 */
	if (reqpcredp != NULL && !suser(reqpcredp))
		return (EPERM);

	rtpp = (rtproc_t *)bufp;
	ASSERT(rtpp != NULL);

	/*
	 * If this thread's lwp is swapped out, it will be brought in
	 * when it is put onto the runqueue.
	 *
	 * Now, Initialize the rtproc structure.
	 */
	if (rtkparmsp == NULL) {
		/*
		 * Use default values
		 */
		rtpp->rt_pri = 0;
		rtpp->rt_pquantum = rt_dptbl[0].rt_quantum;
	} else {
		/*
		 * Use supplied values
		 */
		if (rtkparmsp->rt_pri == RT_NOCHANGE) {
			rtpp->rt_pri = 0;
		} else {
			rtpp->rt_pri = rtkparmsp->rt_pri;
		}
		if (rtkparmsp->rt_tqntm == RT_TQINF)
			rtpp->rt_pquantum = RT_TQINF;
		else if (rtkparmsp->rt_tqntm == RT_TQDEF ||
		    rtkparmsp->rt_tqntm == RT_NOCHANGE)
			rtpp->rt_pquantum = rt_dptbl[rtpp->rt_pri].rt_quantum;
		else
			rtpp->rt_pquantum = rtkparmsp->rt_tqntm;
	}
	rtpp->rt_flags = 0;
	rtpp->rt_tp = t;
	/*
	 * Reset thread priority
	 */
	thread_lock(t);
	t->t_clfuncs = &(sclass[cid].cl_funcs->thread);
	t->t_cid = cid;
	t->t_cldata = (void *)rtpp;
	if (t == curthread || t->t_state == TS_ONPROC) {
		cpu_t	*cp = t->t_disp_queue->disp_cpu;
		t->t_pri = rt_dptbl[rtpp->rt_pri].rt_globpri;
		if (t == cp->cpu_dispthread)
			cp->cpu_dispatch_pri = DISP_PRIO(t);
		if (DISP_PRIO(t) > DISP_MAXRUNPRI(t)) {
			rtpp->rt_timeleft = rtpp->rt_pquantum;
		} else {
			rtpp->rt_flags |= RTBACKQ;
			cpu_surrender(t);
		}
	} else {
		pri_t	new_pri;

		new_pri = rt_dptbl[rtpp->rt_pri].rt_globpri;
		if (thread_change_pri(t, new_pri, 0)) {
			rtpp->rt_timeleft = rtpp->rt_pquantum;
		} else {
			rtpp->rt_flags |= RTBACKQ;
		}
	}
	thread_unlock(t);
	/*
	 * Link new structure into rtproc list
	 */
	mutex_enter(&rt_list_lock);
	rtpp->rt_next = rt_plisthead.rt_next;
	rtpp->rt_prev = &rt_plisthead;
	rt_plisthead.rt_next->rt_prev = rtpp;
	rt_plisthead.rt_next = rtpp;
	mutex_exit(&rt_list_lock);
	return (0);
}


/*
 * Free rtproc structure of thread.
 */
static void
rt_exitclass(void *procp)
{
	rtproc_t *rtprocp = (rtproc_t *)procp;

	mutex_enter(&rt_list_lock);
	rtprocp->rt_prev->rt_next = rtprocp->rt_next;
	rtprocp->rt_next->rt_prev = rtprocp->rt_prev;
	mutex_exit(&rt_list_lock);
	kmem_free(rtprocp, sizeof (rtproc_t));
}


/*
 * Allocate and initialize real-time class specific
 * proc structure for child.
 */
/* ARGSUSED */
static int
rt_fork(t, ct, bufp)
	kthread_id_t t, ct;
	void	*bufp;
{
	rtproc_t	*prtpp;
	rtproc_t	*crtpp;

	ASSERT(MUTEX_HELD(&ttoproc(t)->p_lock));

	/*
	 * Initialize child's rtproc structure
	 */
	crtpp = (rtproc_t *)bufp;
	ASSERT(crtpp != NULL);
	prtpp = (rtproc_t *)t->t_cldata;
	thread_lock(t);
	crtpp->rt_timeleft = crtpp->rt_pquantum = prtpp->rt_pquantum;
	crtpp->rt_pri = prtpp->rt_pri;
	crtpp->rt_flags = prtpp->rt_flags & ~RTBACKQ;
	crtpp->rt_tp = ct;
	thread_unlock(t);

	/*
	 * Link new structure into rtproc list
	 */
	ct->t_cldata = (void *)crtpp;
	mutex_enter(&rt_list_lock);
	crtpp->rt_next = rt_plisthead.rt_next;
	crtpp->rt_prev = &rt_plisthead;
	rt_plisthead.rt_next->rt_prev = crtpp;
	rt_plisthead.rt_next = crtpp;
	mutex_exit(&rt_list_lock);
	return (0);
}


/*
 * The child goes to the back of its dispatcher queue while the
 * parent continues to run after a real time thread forks.
 */
/* ARGSUSED */
static void
rt_forkret(t, ct)
	kthread_id_t t;
	kthread_id_t ct;
{
	register proc_t		*pp = ttoproc(t);
	register proc_t		*cp = ttoproc(ct);

	ASSERT(t == curthread);
	ASSERT(MUTEX_HELD(&pidlock));

	/*
	 * Grab the child's p_lock before dropping pidlock to ensure
	 * the process does not disappear before we set it running.
	 */
	mutex_enter(&cp->p_lock);
	mutex_exit(&pidlock);
	continuelwps(cp);
	mutex_exit(&cp->p_lock);

	mutex_enter(&pp->p_lock);
	continuelwps(pp);
	mutex_exit(&pp->p_lock);
}


/*
 * Get information about the real-time class into the buffer
 * pointed to by rtinfop.  The maximum configured real-time
 * priority is the only information we supply.  We ignore the
 * class and credential arguments because anyone can have this
 * information.
 */
/* ARGSUSED */
static int
rt_getclinfo(void *infop)
{
	rtinfo_t *rtinfop = (rtinfo_t *)infop;
	rtinfop->rt_maxpri = rt_maxpri;
	return (0);
}

/*
 * Return the global scheduling priority ranges of the realtime
 * class in pcpri_t structure.
 */
static int
rt_getclpri(pcpri_t *pcprip)
{
	pcprip->pc_clpmax = rt_dptbl[rt_maxpri].rt_globpri;
	pcprip->pc_clpmin = rt_dptbl[0].rt_globpri;
	return (0);
}
static void
rt_nullsys()
{
}

/* ARGSUSED */
static int
rt_canexit(kthread_id_t t, cred_t *cred)
{
	/*
	 * Thread can always leave RT class
	 */
	return (0);
}

/*
 * Get the real-time scheduling parameters of the thread pointed to by
 * rtprocp into the buffer pointed to by rtkparmsp.
 */
static void
rt_parmsget(kthread_id_t t, void *parmsp)
{
	rtproc_t	*rtprocp = (rtproc_t *)t->t_cldata;
	rtkparms_t	*rtkparmsp = (rtkparms_t *)parmsp;

	rtkparmsp->rt_pri = rtprocp->rt_pri;
	rtkparmsp->rt_tqntm = rtprocp->rt_pquantum;
}



/*
 * Check the validity of the real-time parameters in the buffer
 * pointed to by rtprmsp.  If our caller passes us a non-NULL
 * reqpcredp pointer we also verify that the requesting thread
 * (whose class and credentials are indicated by reqpcid and reqpcredp)
 * has the necessary permissions to set these parameters for a
 * target thread with class targpcid. We also convert the
 * rtparms buffer from the user supplied format to our internal
 * format (i.e. time quantum expressed in ticks).
 */
/* ARGSUSED */
static int
rt_parmsin(prmsp, reqpcid, reqpcredp, targpcid, targpcredp, tpclpp)
	void	*prmsp;
	id_t	reqpcid;
	cred_t	*reqpcredp;
	id_t	targpcid;
	cred_t	*targpcredp;
	void	*tpclpp;
{
	rtparms_t *rtprmsp = (rtparms_t *)prmsp;
	/*
	 * First check the validity of parameters and convert
	 * the buffer to kernel format.
	 */
	if ((rtprmsp->rt_pri < 0 || rtprmsp->rt_pri > rt_maxpri) &&
	    rtprmsp->rt_pri != RT_NOCHANGE)
		return (EINVAL);

	if ((rtprmsp->rt_tqsecs == 0 && rtprmsp->rt_tqnsecs == 0) ||
	    rtprmsp->rt_tqnsecs >= NANOSEC)
		return (EINVAL);

	if (rtprmsp->rt_tqnsecs >= 0) {
		((rtkparms_t *)rtprmsp)->rt_tqntm =
			SEC_TO_TICK(rtprmsp->rt_tqsecs) +
			NSEC_TO_TICK_ROUNDUP(rtprmsp->rt_tqnsecs);
	} else {
		if (rtprmsp->rt_tqnsecs != RT_NOCHANGE &&
		    rtprmsp->rt_tqnsecs != RT_TQINF &&
		    rtprmsp->rt_tqnsecs != RT_TQDEF)
			return (EINVAL);
		((rtkparms_t *)rtprmsp)->rt_tqntm = rtprmsp->rt_tqnsecs;
	}

	/*
	 * If our caller passed us non-NULL cred pointers
	 * we are being asked to check permissions as well
	 * as the validity of the parameters. In order to
	 * set any parameters the real-time class requires
	 * that the requesting thread be real-time or
	 * super-user.  If the target thread is currently in
	 * a class other than real-time the requesting thread
	 * must be super-user.
	 */
	if (reqpcredp != NULL) {
		if (targpcid == rt_cid) {
			if (reqpcid != rt_cid && !suser(reqpcredp))
				return (EPERM);
		} else {  /* target thread is not real-time */
			if (!suser(reqpcredp))
				return (EPERM);
		}
	}

	return (0);
}

/*
 * Do required processing on the real-time parameter buffer
 * before it is copied out to the user. We ignore the class
 * and credential arguments passed by our caller because we
 * don't require any special permissions to read real-time
 * scheduling parameters.  All we have to do is convert the
 * buffer from kernel to user format (i.e. convert time quantum
 * from ticks to seconds-nanoseconds).
 */
/* ARGSUSED */
static int
rt_parmsout(prmsp, reqpcid, reqpcredp, targpcid, targpcredp, tpclpp)
	void	*prmsp;
	id_t	reqpcid;
	cred_t	*reqpcredp;
	id_t	targpcid;
	cred_t	*targpcredp;
	void	*tpclpp;
{
	register rtkparms_t	*rtkprmsp = (rtkparms_t *)prmsp;

	if (rtkprmsp->rt_tqntm < 0) {
		/*
		 * Quantum field set to special value (e.g. RT_TQINF)
		 */
		((rtparms_t *)rtkprmsp)->rt_tqnsecs = rtkprmsp->rt_tqntm;
		((rtparms_t *)rtkprmsp)->rt_tqsecs = 0;
	} else {
		/* Convert quantum from ticks to seconds-nanoseconds */

		timestruc_t ts;
		TICK_TO_TIMESTRUC(rtkprmsp->rt_tqntm, &ts);
		((rtparms_t *)rtkprmsp)->rt_tqsecs = ts.tv_sec;
		((rtparms_t *)rtkprmsp)->rt_tqnsecs = ts.tv_nsec;
	}

	return (0);
}


/*
 * Set the scheduling parameters of the thread pointed to by rtprocp
 * to those specified in the buffer pointed to by rtkprmsp.
 * Note that the parameters are expected to be in kernel format
 * (i.e. time quantm expressed in ticks).  Real time parameters copied
 * in from the user should be processed by rt_parmsin() before they are
 * passed to this function.
 */
static int
rt_parmsset(tx, prmsp, reqpcid, reqpcredp)
	kthread_id_t	tx;
	void		*prmsp;
	id_t		reqpcid;
	cred_t		*reqpcredp;
{
	register rtkparms_t	*rtkprmsp = (rtkparms_t *)prmsp;
	register rtproc_t	*rtpp = (rtproc_t *)tx->t_cldata;

	ASSERT(MUTEX_HELD(&(ttoproc(tx))->p_lock));

	/*
	 * Basic permissions enforced by generic kernel code
	 * for all classes require that a thread attempting
	 * to change the scheduling parameters of a target thread
	 * be super-user or have a real or effective UID
	 * matching that of the target thread. We are not
	 * called unless these basic permission checks have
	 * already passed. The real-time class requires in addition
	 * that the requesting thread be real-time unless it is super-user.
	 * This may also have been checked previously but if our caller
	 * passes us a credential structure we assume it hasn't and
	 * we check it here.
	 */
	if (reqpcredp != NULL && reqpcid != rt_cid && !suser(reqpcredp))
		return (EPERM);

	thread_lock(tx);
	if (rtkprmsp->rt_pri != RT_NOCHANGE) {
		rtpp->rt_pri = rtkprmsp->rt_pri;
		if (tx == curthread || tx->t_state == TS_ONPROC) {
			cpu_t	*cp = tx->t_disp_queue->disp_cpu;
			tx->t_pri = rt_dptbl[rtpp->rt_pri].rt_globpri;
			if (tx == cp->cpu_dispthread)
				cp->cpu_dispatch_pri = DISP_PRIO(tx);
			if (DISP_PRIO(tx) <= DISP_MAXRUNPRI(tx)) {
				rtpp->rt_flags |= RTBACKQ;
				cpu_surrender(tx);
			}
		} else {
			pri_t	new_pri;

			new_pri = rt_dptbl[rtpp->rt_pri].rt_globpri;
			if (!thread_change_pri(tx, new_pri, 0)) {
				rtpp->rt_flags |= RTBACKQ;
			}
		}
	}
	if (rtkprmsp->rt_tqntm == RT_TQINF)
		rtpp->rt_pquantum = RT_TQINF;
	else if (rtkprmsp->rt_tqntm == RT_TQDEF)
		rtpp->rt_timeleft = rtpp->rt_pquantum =
		    rt_dptbl[rtpp->rt_pri].rt_quantum;
	else if (rtkprmsp->rt_tqntm != RT_NOCHANGE)
		rtpp->rt_timeleft = rtpp->rt_pquantum = rtkprmsp->rt_tqntm;
	thread_unlock(tx);
	return (0);
}


/*
 * Arrange for thread to be placed in appropriate location
 * on dispatcher queue.  Runs at splhi() since the clock
 * interrupt can cause RTBACKQ to be set.
 */
static void
rt_preempt(tid)
kthread_id_t	tid;
{
	rtproc_t	*rtpp = (rtproc_t *)(tid->t_cldata);
	register klwp_t *lwp;

	ASSERT(THREAD_LOCK_HELD(tid));

	/*
	 * If the state is user I allow swapping because I know I won't
	 * be holding any locks.
	 */
	if ((lwp = curthread->t_lwp) != NULL && lwp->lwp_state == LWP_USER)
		tid->t_schedflag &= ~TS_DONT_SWAP;
	if ((rtpp->rt_flags & RTBACKQ) != 0) {
		rtpp->rt_timeleft = rtpp->rt_pquantum;
		rtpp->rt_flags &= ~RTBACKQ;
		setbackdq(tid);
	} else
		setfrontdq(tid);

}

/*
 * Return the global priority associated with this rt_pri.
 */
static pri_t
rt_globpri(kthread_id_t t)
{
	rtproc_t *rtprocp = (rtproc_t *)t->t_cldata;
	return (rt_dptbl[rtprocp->rt_pri].rt_globpri);
}

static void
rt_setrun(tid)
kthread_id_t	tid;
{
	rtproc_t	*rtpp = (rtproc_t *)(tid->t_cldata);

	ASSERT(THREAD_LOCK_HELD(tid));

	rtpp->rt_timeleft = rtpp->rt_pquantum;
	rtpp->rt_flags &= ~RTBACKQ;
	setbackdq(tid);
}

/*
 * Return an effective priority for swapin/swapout.
 */
/* ARGSUSED */
static pri_t
rt_swappri(t, flags)
	kthread_id_t	t;
	int		flags;
{
	ASSERT(THREAD_LOCK_HELD(t));

	return (-1);
}

/*
 * Check for time slice expiration (unless thread has infinite time
 * slice).  If time slice has expired arrange for thread to be preempted
 * and placed on back of queue.
 */
static void
rt_tick(kthread_id_t tid)
{
	register rtproc_t *rtpp = (rtproc_t *)(tid->t_cldata);

	ASSERT(MUTEX_HELD(&(ttoproc(tid))->p_lock));

	thread_lock(tid);
	if ((rtpp->rt_pquantum != RT_TQINF && --rtpp->rt_timeleft == 0) ||
	    (tid->t_pri < tid->t_disp_queue->disp_maxrunpri)) {
		rtpp->rt_flags |= RTBACKQ;
		cpu_surrender(tid);
	}
	thread_unlock(tid);
}


/*
 * Place the thread waking up on the dispatcher queue.
 */
static void
rt_wakeup(tid)
kthread_id_t	tid;
{
	rtproc_t	*rtpp = (rtproc_t *)(tid->t_cldata);

	ASSERT(THREAD_LOCK_HELD(tid));

	rtpp->rt_timeleft = rtpp->rt_pquantum;
	rtpp->rt_flags &= ~RTBACKQ;
	setbackdq(tid);
}

static void
rt_yield(t)
	kthread_id_t	t;
{
	rtproc_t	*rtpp = (rtproc_t *)(t->t_cldata);

	ASSERT(t == curthread);
	ASSERT(THREAD_LOCK_HELD(t));

	rtpp->rt_flags &= ~RTBACKQ;
	setbackdq(t);
}

/* ARGSUSED */
static int
rt_donice(t, cr, incr, retvalp)
	kthread_id_t	t;
	cred_t		*cr;
	int		incr;
	int		*retvalp;
{
	return (EINVAL);
}

static int
rt_alloc(void **p, int flag)
{
	void *bufp;
	bufp = kmem_alloc(sizeof (rtproc_t), flag);
	if (bufp == NULL) {
		return (ENOMEM);
	} else {
		*p = bufp;
		return (0);
	}
}

static void
rt_free(void *bufp)
{
	if (bufp)
		kmem_free(bufp, sizeof (rtproc_t));
}
