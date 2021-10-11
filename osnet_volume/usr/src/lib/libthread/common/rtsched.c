/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rtsched.c	1.5	99/10/25 SMI"

#include <sys/types.h>
#include <sched.h>
#include <sys/priocntl.h>
#include <sys/rtpriocntl.h>
#include <sys/tspriocntl.h>
#include <thread.h>
#include <sys/rt.h>
#include <sys/ts.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include "debug.h"
#include "underscore.h"

extern int _pthread_once(pthread_once_t *, void (*)(void));
extern void _panic(const char *);

/*
 * The following variables are used for caching information
 * for priocntl RT scheduling class.
 */
static struct pcclass {
	short		pcc_state;
	pri_t		pcc_primin;
	pri_t		pcc_primax;
	pcinfo_t	pcc_info;
} rt_class;

static rtdpent_t *rt_dptbl;	/* RT class parameter table */
static int rt_rrmin;
static int rt_rrmax;
static int rt_fifomin;
static int rt_fifomax;
static int rt_othermin;
static int rt_othermax;
static pthread_once_t validateonce = PTHREAD_ONCE_INIT;
static pthread_once_t maponce = PTHREAD_ONCE_INIT;

#ifdef DEBUG
int	_assfail(char *, char *, int);
#endif /* DEBUG */

/*
 * Get the RT class parameter table
 */
static void
_get_rt_dptbl()
{
	struct pcclass	*pccp;
	pcadmin_t	pcadmin;
	rtadmin_t	rtadmin;
	size_t		rtdpsize;

	pccp = &rt_class;
	/* get class's info */
	(void) strcpy(pccp->pcc_info.pc_clname, "RT");
	if (priocntl(P_PID, 0, PC_GETCID, (caddr_t)&(pccp->pcc_info)) < 0)
		goto out;
	/* get RT class dispatch table in rt_dptbl */
	pcadmin.pc_cid = rt_class.pcc_info.pc_cid;
	pcadmin.pc_cladmin = (caddr_t)&rtadmin;
	rtadmin.rt_cmd = RT_GETDPSIZE;
	if (priocntl(P_PID, 0, PC_ADMIN, (caddr_t)&pcadmin) < 0)
		goto out;
	rtdpsize = rtadmin.rt_ndpents * sizeof (rtdpent_t);
	if (rt_dptbl == NULL &&
	    (rt_dptbl = (rtdpent_t *)malloc(rtdpsize)) == NULL) {

		goto out;
	}
	rtadmin.rt_dpents = rt_dptbl;
	rtadmin.rt_cmd = RT_GETDPTBL;
	if (priocntl(P_PID, 0, PC_ADMIN, (caddr_t)&pcadmin) < 0)
		goto out;
	pccp->pcc_primin = 0;
	pccp->pcc_primax = ((rtinfo_t *)rt_class.pcc_info.pc_clinfo)->rt_maxpri;
	return;
out:
	_panic("get_rt_dptbl failed");
}

/*
 * Translate RT class's user priority to global scheduling priority.
 * This is for priorities coming from librt.
 */
int
_map_rtpri_to_gp(pri_t pri)
{
	rtdpent_t	*rtdp;
	pri_t		gpri;

	if (_pthread_once(&maponce, _get_rt_dptbl))
		_panic("_map_rtpri_to_gp failed");
	/* First case is the default case, other two are seldomly taken */
	if (pri <= rt_dptbl[rt_class.pcc_primin].rt_globpri) {
		gpri = pri + rt_dptbl[rt_class.pcc_primin].rt_globpri - \
		    rt_class.pcc_primin;
	} else if (pri >= rt_dptbl[rt_class.pcc_primax].rt_globpri) {
		gpri = pri + rt_dptbl[rt_class.pcc_primax].rt_globpri - \
		    rt_class.pcc_primax;
	} else {
		gpri =  rt_dptbl[rt_class.pcc_primin].rt_globpri + 1;
		for (rtdp = rt_dptbl+1; rtdp->rt_globpri < pri; ++rtdp, ++gpri)
			;
		if (rtdp->rt_globpri > pri)
			--gpri;
	}
	return (gpri);
}

/*
 * Set the RT priority/policy of a lwp/thread.
 */
int
_thrp_setlwpprio(lwpid_t lwpid, int policy, int pri)
{
	pcinfo_t	pcinfo;
	pcparms_t	pcparm;
	int rt = 0;

	ASSERT(((policy == SCHED_FIFO) || (policy == SCHED_RR) ||
	    (policy == SCHED_OTHER)));
	if ((policy == SCHED_FIFO) || (policy == SCHED_RR)) {
		rt = 1;
	}
	if (rt) {
		(void) strcpy(pcinfo.pc_clname, "RT");
	} else {
		(void) strcpy(pcinfo.pc_clname, "TS");
	}
	pcparm.pc_cid = PC_CLNULL;
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) < 0) {
		return (errno);
	}
	pcparm.pc_cid = pcinfo.pc_cid;
	if (rt) {
		((rtparms_t *)pcparm.pc_clparms)->rt_tqnsecs =
			(policy == SCHED_RR ? RT_TQDEF : RT_TQINF);
		((rtparms_t *)pcparm.pc_clparms)->rt_pri = pri;
	} else {
		((tsparms_t *)pcparm.pc_clparms)->ts_uprilim = TS_NOCHANGE;
		((tsparms_t *)pcparm.pc_clparms)->ts_upri = TS_NOCHANGE;
	}
	if (priocntl(P_LWPID, lwpid, PC_SETPARMS, (caddr_t)&pcparm) == -1) {
		return (errno);
	}
	return (0);
}

/*
 * Get librt priority ranges.
 */
static void
_init_rt_prio_ranges()
{
	void	*hdl;
	int	(*fname)(int);

	if ((hdl = _dlopen("librt.so", RTLD_LAZY)) == 0) {
		_panic("_init_rt_prio_ranges: dlopen librt failed");
	}
	if ((fname = (int (*)(int))_dlsym(hdl, "sched_get_priority_min"))
			!= NULL) {
		rt_fifomin = (*fname)(SCHED_FIFO);
		rt_rrmin = (*fname)(SCHED_RR);
		rt_othermin = (*fname)(SCHED_OTHER);
	} else {
		_panic("_init_rt_prio_ranges: _dlsym for symbols in librt");
	}
	if ((fname = (int (*)(int))_dlsym(hdl, "sched_get_priority_max"))
			!= NULL) {
		rt_fifomax = (*fname)(SCHED_FIFO);
		rt_rrmax = (*fname)(SCHED_RR);
		rt_othermax = (*fname)(SCHED_OTHER);
	} else {
		_panic("_init_rt_prio_ranges: _dlsym for symbols in librt");
	}
	_dlclose(hdl);
}

int
_getscheduler()
{
	void	*hdl;
	int	(*fname)(int);
	int policy;

	if ((hdl = _dlopen("librt.so", RTLD_LAZY)) == 0) {
		_panic("_getscheduler: dlopen librt failed");
	}
	if ((fname = (int (*)(int))_dlsym(hdl, "sched_getscheduler"))
			!= NULL) {
		policy = (*fname)(_getpid());
		_dlclose(hdl);
		return (policy);
	} else {
		_panic("_getscheduler: _dlsym for symbols in librt");
	}
}

/*
 * Validate priorities from librt.
 */
int
_validate_rt_prio(int policy, int pri)
{

	_pthread_once(&validateonce, _init_rt_prio_ranges);
	if (policy == SCHED_FIFO) {
		if (pri < rt_fifomin || pri > rt_fifomax) {
			return (1);
		}
	} else if (policy == SCHED_RR) {
		if (pri < rt_rrmin || pri > rt_rrmax) {
			return (1);
		}
	} else if (policy == SCHED_OTHER) {
		if (pri < rt_othermin || pri > rt_othermax) {
			return (1);
		}
	}
	return (0);
}
