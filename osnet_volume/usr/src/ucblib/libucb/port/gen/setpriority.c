/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma ident	"@(#)setpriority.c	1.11	98/05/10 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/procset.h>
#include <sys/priocntl.h>
#include <sys/tspriocntl.h>
#include <sys/iapriocntl.h>
#include <errno.h>

/*
 * forward declarations
 */
static int	prio_to_idtype(int);
static int	init_in_set(idtype_t, id_t);
static int	get_clinfo(char *, idtype_t, id_t, pcinfo_t *, pcparms_t *);

int
getpriority(int which, int who)
{
	id_t id;
	idtype_t idtype;
	pcinfo_t pcinfo;
	pcparms_t pcparms;
	int scale, ts_scale, ia_scale;	/* amount to scale priority by */
	int pri, ts_pri, ia_pri;	/* user priority of process */
	int ts_found = 0, ia_found = 0;	/* process match found? */

	if (who == 0)
		id = P_MYID;
	else
		id = who;

	idtype = prio_to_idtype(which);
	if (idtype == -1) {
		errno = EINVAL;
		return (-1);
	}
	if (who < 0) {
		errno = ESRCH;
		return (-1);
	}

	/*
	 * get class ID, name, and attributes for the process(es)
	 */
	if (get_clinfo("TS", idtype, id, &pcinfo, &pcparms) != -1) {
		ts_scale = ((tsinfo_t *)pcinfo.pc_clinfo)->ts_maxupri;
		ts_pri = ((tsparms_t *)pcparms.pc_clparms)->ts_upri;
		if (ts_pri <= ts_scale && ts_pri >= -ts_scale)
			ts_found = 1;		/* upri is in valid range */
	}

	if (get_clinfo("IA", idtype, id, &pcinfo, &pcparms) != -1) {
		ia_scale = ((iainfo_t *)pcinfo.pc_clinfo)->ia_maxupri;
		ia_pri = ((iaparms_t *)pcparms.pc_clparms)->ia_upri;
		if (ia_pri <= ia_scale && ia_pri >= -ia_scale)
			ia_found = 1;		/* upri is in valid range */
	}

	if (!ia_found && !ts_found) {
		/*
		 * Nothing matches in either class.
		 * errno should have been set by the
		 * IA call, so we can just return.
		 */
		return (-1);
	} else if (!ia_found) {
		/* only in TS */
		errno = 0;
		scale = ts_scale;
		pri = ts_pri;
	} else if (!ts_found) {
		/* only in IA */
		errno = 0;
		scale = ia_scale;
		pri = ia_pri;
	} else {
		/* some processes that match in both TS and IA */
		if (ts_pri > ia_pri) {
			scale = ts_scale;
			pri = ts_pri;
		} else {
			scale = ia_scale;
			pri = ia_pri;
		}
	}

	if (scale == 0)
		return (0);
	else
		return (-(pri * 20) / scale);
}

int
setpriority(int which, int who, int prio)
{
	id_t id;
	idtype_t idtype;
	pcinfo_t pcinfo;
	pcparms_t pcparms;
	tsparms_t *tsp;
	iaparms_t *iap;
	procset_t procset;
	id_t ts_cid, ia_cid;		/* class IDs */
	int ts_scale, ia_scale;		/* amount to scale priority by */
	int ts_found = 0, ia_found = 0;	/* process match found? */

	if (who == 0)
		id = P_MYID;
	else
		id = who;

	idtype = prio_to_idtype(which);
	if (idtype == -1) {
		errno = EINVAL;
		return (-1);
	}
	if (who < 0) {
		errno = ESRCH;
		return (-1);
	}

	/*
	 * get class ID, name, and attributes for the process
	 */
	if (get_clinfo("TS", idtype, id, &pcinfo, &pcparms) != -1) {
		ts_scale = ((tsinfo_t *)pcinfo.pc_clinfo)->ts_maxupri;
		ts_cid = pcinfo.pc_cid;
		ts_found = 1;
	}

	if (get_clinfo("IA", idtype, id, &pcinfo, &pcparms) != -1) {
		ia_scale = ((iainfo_t *)pcinfo.pc_clinfo)->ia_maxupri;
		ia_cid = pcinfo.pc_cid;
		ia_found = 1;
	}

	if (!ts_found && !ia_found)
		return (-1);
	errno = 0;		/* clear errno if the IA call failed */

	if (prio > 19)
		prio = 19;
	else if (prio < -20)
		prio = -20;

	if (ts_found) {
		pcparms.pc_cid = ts_cid;
		tsp = (tsparms_t *)pcparms.pc_clparms;
		tsp->ts_uprilim = tsp->ts_upri = -(ts_scale * prio) / 20;

		setprocset(&procset, POP_AND, idtype, id, P_CID, ts_cid);
		if (priocntlset(&procset, PC_SETPARMS, (caddr_t)&pcparms) == -1)
			return (-1);

		if (init_in_set(idtype, id)) {
			setprocset(&procset, POP_AND, P_PID, P_INITPID,
			    P_CID, ts_cid);
			if (priocntlset(&procset, PC_SETPARMS,
			    (caddr_t)&pcparms) == -1)
				return (-1);
		}
	}

	if (ia_found) {
		pcparms.pc_cid = ia_cid;
		iap = (iaparms_t *)pcparms.pc_clparms;
		iap->ia_uprilim = iap->ia_upri = -(ia_scale * prio) / 20;
		iap->ia_mode = IA_NOCHANGE;

		setprocset(&procset, POP_AND, idtype, id, P_CID, ia_cid);
		if (priocntlset(&procset, PC_SETPARMS, (caddr_t)&pcparms) == -1)
			return (-1);

		if (init_in_set(idtype, id)) {
			setprocset(&procset, POP_AND, P_PID, P_INITPID,
			    P_CID, ia_cid);
			if (priocntlset(&procset, PC_SETPARMS,
			    (caddr_t)&pcparms) == -1)
				return (-1);
		}
	}

	return (0);

}

static int
prio_to_idtype(int which)
{
	switch (which) {
	case PRIO_PROCESS:
		return (P_PID);

	case PRIO_PGRP:
		return (P_PGID);

	case PRIO_USER:
		return (P_UID);

	default:
		return (-1);
	}
}

static int
init_in_set(idtype_t idtype, id_t id)
{
	switch (idtype) {

	case P_PID:
		if (id == P_INITPID)
			return (1);
		else
			return (0);

	case P_PGID:
		if (id == P_INITPGID)
			return (1);
		else
			return (0);

	case P_UID:
		if (id == P_INITUID)
			return (1);
		else
			return (0);

	default:
		return (0);
	}
}

static int
get_clinfo(char *clname, idtype_t idtype, id_t id, pcinfo_t *pcinfo,
	pcparms_t *pcparms)
{
	(void) strcpy(pcinfo->pc_clname, clname);
	if (priocntl(0, 0, PC_GETCID, (caddr_t)pcinfo) == -1)
		return (-1);

	pcparms->pc_cid = pcinfo->pc_cid;
	if (priocntl(idtype, id, PC_GETPARMS, (caddr_t)pcparms) == -1)
		return (-1);

	return (1);
}
