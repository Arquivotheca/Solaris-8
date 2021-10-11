/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
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
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc.c	1.23	98/03/30 SMI"

/*
 * This file contains code for the crash functions:  proc, defproc.
 */

#include <sys/param.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include "crash.h"

struct user *ubp;		/* pointer to the ublock */

static void prproc(int, long, pid_t, int, int, void *, char *, int);
static void prdefproc(int, int, int);
static int readuid(struct cred *);

/* get arguments for proc function */
int
getproc(void)
{
	long slot = -1;
	int full = 0;
	int lock = 0;
	int phys = 0;
	int run = 0;
	void *addr = (void *)-1;
	long arg1 = -1;
	long arg2 = -1;
	pid_t id = -1;
	int c;
	char *heading =
"SLOT ST  PID  PPID  PGID   SID   UID PRI   NAME        FLAGS\n";
	optind = 1;
	while ((c = getopt(argcnt, args, "eflparw:s:")) != EOF) {
		switch (c) {
			case 'e' :
					break;
			case 'f' :	full = 1;
					break;
			case 'l' :	lock = 1;
					break;
			case 'p' :	phys = 1;
					break;
			case 'r' :	run = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	fprintf(fp, "PROC TABLE SIZE = %d\n", vbuf.v_proc);
	if (!full)
		fprintf(fp, "%s", heading);
	if (args[optind]) {
		do {
			if (*args[optind] == '#') {
				if ((id = (pid_t)
					strcon(++args[optind], 'd')) == -1)
					error("\n");
				prproc(full, slot, id, phys, run,
					addr, heading, lock);
			} else {
				getargs(vbuf.v_proc, &arg1, &arg2, phys);
				if (arg1 == -1)
					continue;
				if (arg2 != -1)
					for (slot = arg1; slot <= arg2; slot++)
						prproc(full, slot, id,
							phys, run, addr,
							heading, lock);
				else {
					if ((unsigned long)arg1 < vbuf.v_proc)
						slot = arg1;
					else
						addr = (void *)arg1;
					prproc(full, slot, id, phys, run,
					    addr, heading, lock);
				}
			}
			id = slot = arg1 = arg2 = -1;
			addr = (void *)-1;
		} while (args[++optind]);
	} else for (slot = 0; slot < vbuf.v_proc; slot++)
		prproc(full, slot, id, phys, run, addr, heading, lock);
	return (0);
}


/* print proc table */
static void
prproc(int full, long slot, pid_t id, int phys, int run, void *addr,
    char *heading, int lock)
{
	char ch;
	char cp[MAXCOMLEN+1];
	struct proc procbuf, *procaddr;
	struct cred uc;
	struct sess sess;
	kthread_t tb, *t;
	int i, state, pri, cid;

	if (id != -1) {
		for (slot = 0; ; slot++) {
			if (slot == vbuf.v_proc) {
				fprintf(fp, "%d not valid process id\n", id);
				return;
			}
			if (slot_to_pid(slot) == id) {
				procaddr = slot_to_proc(slot);
				break;
			}
		}
	} else if (slot != -1)
		procaddr = slot_to_proc(slot);
	else {
		slot = 0;
		for (;;) {
			if (slot == vbuf.v_proc) {
				fprintf(fp,
				    "%p not valid process address\n", addr);
				return;
			}
			procaddr = slot_to_proc(slot);
			if (phys || !Virtmode) {
				/*
				 * Though vtop() might return a physical
				 * address >32 bits, the rest of the program
				 * has no concept that the system physical
				 * memory space might ever exceed 32 bits.
				 * Welcome to the world of grown-up computers.
				 *
				 * Workaround: use virtual mode.
				 */
				if ((longlong_t)addr == vtop((intptr_t)procaddr,
				    Procslot))
					break;
			} else {
				if (addr == procaddr)
					break;
			}
			slot++;
		}
	}
	if (!procaddr)
		return;

	readbuf(addr, (off_t)procaddr, phys,
	    &procbuf, sizeof (procbuf), "proc table");

	/*
	 * For now, get priority and state of first thread on list,
	 * if there is one. Should take care of most cases nicely.
	 */
	if ((t = procbuf.p_tlist) != NULL) {
		readbuf(t, 0, phys, &tb, sizeof (tb), "thread entry");
		switch (tb.t_state) {
		case TS_SLEEP:	state = SSLEEP;		break;
		case TS_RUN:	state = SRUN;		break;
		case TS_ONPROC:	state = SONPROC;	break;
		case TS_ZOMB:	state = SZOMB;		break;
		case TS_STOPPED: state = SSTOP;		break;
		default:	state = 0;		break;
		}
		pri = tb.t_pri;
		cid = tb.t_cid;
	} else {
		state = procbuf.p_stat;
		pri = 99;	/* Undefined */
		cid = -1;
	}

	if (run)
		if (!(state == SRUN || state == SONPROC))
			return;

	if (full)
		fprintf(fp, "%s", heading);

	switch (state) {
		case NULL:   ch = ' '; break;
		case SSLEEP: ch = 's'; break;
		case SRUN:   ch = 'r'; break;
		case SIDL:   ch = 'i'; break;
		case SZOMB:  ch = 'z'; break;
		case SSTOP:  ch = 't'; break;
		case SONPROC:  ch = 'p'; break;
		default:  ch = '?'; break;
	}
	if (slot == -1)
		fprintf(fp, "  - ");
	else fprintf(fp, "%4ld", slot);
	fprintf(fp, " %c %5u %5u %5u %5u %5u  %2u",
		ch,
		slot_to_pid(slot),
		procbuf.p_ppid,
		readpid(procbuf.p_pgidp),
		readsid(procbuf.p_sessp),
		readuid(procbuf.p_cred),
		pri);
	fprintf(fp, " ");
	for (i = 0; i < MAXCOMLEN+1; i++)
		cp[i] = '\0';
	if (procbuf.p_stat == SZOMB)
		strcpy(cp, "zombie");
	else if (!(procbuf.p_flag & SULOAD)) {
		strcpy(cp, "swapped");
	} else {
		ubp = &procbuf.p_user;
		strncpy(cp, ubp->u_comm, MAXCOMLEN+1);
	}
	for (i = 0; i < MAXCOMLEN && cp[i]; i++) {
		if (cp[i] < 040 || cp[i] > 0176) {
			strcpy(cp, "unprint");
			break;
		}
	}
	fprintf(fp, "%-14s", cp);

	fprintf(fp, "%s%s%s%s%s%s%s%s%s%s%s\n",
		procbuf.p_flag & SLOAD ? " load" : "",
		(procbuf.p_flag & (SLOAD|SULOAD)) == SULOAD ? " uload" : "",
		procbuf.p_flag & SSYS ? " sys" : "",
		procbuf.p_flag & SLOCK ? " lock" : "",
		procbuf.p_flag & STRC ? " trc" : "",
		procbuf.p_flag & SPROCTR ? " prtr" : "",
		procbuf.p_flag & SPRFORK ? " prfo" : "",
		procbuf.p_flag & SRUNLCL ? " runl" : "",
		procbuf.p_flag & SNOWAIT ? " nowait" : "",
		procbuf.p_flag & SJCTL ? " jctl" : "",
		procbuf.p_flag & SVFORK ? " vfrk" : "");

	if (lock) {
		if (procbuf.p_lockp != NULL) {
			struct plock pl;
			readbuf(procbuf.p_lockp, 0, phys,
				&pl, sizeof (pl), "p_lock entry");
			fprintf(fp, "\np_lock: ");
			prmutex(&(pl.pl_lock));
		}
		fprintf(fp, "cr_lock: ");
		prmutex(&(procbuf.p_crlock));
		prcondvar(&procbuf.p_cv, "p_cv");
		prcondvar(&procbuf.p_flag_cv, "p_flag_cv");
		prcondvar(&procbuf.p_lwpexit, "p_lwpexit");
		prcondvar(&procbuf.p_holdlwps, "p_holdlwps");
		prcondvar(&procbuf.p_srwchan_cv, "p_srwchan_cv");
	}
	if (!full)
		return;

	readmem(procbuf.p_sessp, 1, &sess, sizeof (sess), "session");
	fprintf(fp, "\n\tSession: ");
	fprintf(fp, "sid: %u, ctty: ", readsid(procbuf.p_sessp));
	if (sess.s_vp)
		fprintf(fp, "vnode(%p) maj(%4u) min(%5u)\n",
			sess.s_vp, getemajor(sess.s_dev),
			geteminor(sess.s_dev));
	else
		fprintf(fp, "-\n");

	readmem(procbuf.p_cred, 1, &uc, sizeof (uc), "process credentials");

	fprintf(fp, "\tProcess Credentials: ");
	fprintf(fp, "uid: %u, gid: %u, real uid: %u, real gid: %u\n",
		uc.cr_uid,
		uc.cr_gid,
		uc.cr_ruid,
		uc.cr_rgid);
	fprintf(fp, "\tas: %p\n",
		procbuf.p_as);
	fprintf(fp, "\twait code: %x, wait data: %x\n",
		procbuf.p_wcode,
		procbuf.p_wdata);
	fprintf(fp, "\tsig: %x\tlink %p\n",
		procbuf.p_sig,
		procbuf.p_link);
	fprintf(fp, "\tparent: %p\tchild: %p\n\tsibling: %p threadp: %p\n",
		procbuf.p_parent,
		procbuf.p_child,
		procbuf.p_sibling,
		procbuf.p_tlist);
	if (procbuf.p_link)
		fprintf(fp, "\tlink: %d\n", proc_to_slot((long)procbuf.p_link));
	fprintf(fp, "\tutime: %d\tstime: %d\tcutime: %d\tcstime: %d\n",
		procbuf.p_utime, procbuf.p_stime, procbuf.p_cutime,
		procbuf.p_cstime);
	fprintf(fp, "\ttrace: %p\tsigmask: %x",
		procbuf.p_trace,
		procbuf.p_sigmask);

	/* print class information */

	fprintf(fp, "\tclass: %d\n", cid);

	fprintf(fp, "\tlwptotal: %d	lwpcnt: %d	lwprcnt: %d\n",
		procbuf.p_lwptotal, procbuf.p_lwpcnt, procbuf.p_lwprcnt);

	fprintf(fp, "\n");
}


/* get arguments for defproc function */
int
getdefproc(void)
{
	int c;
	int proc = -1;
	int change = 0;
	int reset = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "crw:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			case 'c' :	change = 1;
					break;
			case 'r' :	reset = 1;
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind])
		if ((proc = (int)strcon(args[optind], 'd')) == -1)
			error("\n");
	prdefproc(proc, change, reset);
	return (0);
}

/* print results of defproc function */
static void
prdefproc(int proc, int change, int reset)
{
	if (change)
		Procslot = getcurproc();
	else if (proc > -1) {
		if ((proc > vbuf.v_proc) || (proc < 0))
			error("%d out of range\n", proc);
		Procslot = proc;
	} else if (reset) {
		Curthread = getcurthread();
		Procslot = getcurproc();
		fprintf(fp, "Current Thread %p\n", Curthread);
	}
	fprintf(fp, "Procslot = %d\n", Procslot);
}

int
readsid(struct sess *sessp)
{
	struct sess s;

	readmem(sessp, 1, &s, sizeof (struct sess), "session structure");

	return (readpid(s.s_sidp));
}

int
readpid(struct pid *pidp)
{
	struct pid p;

	readmem(pidp, 1, &p, sizeof (struct pid), "pid structure");

	return (p.pid_id);
}

int
readuid(struct cred *credp)
{
	struct cred uc;

	readmem(credp, 1, &uc, sizeof (uc), "process credentials");
	return (uc.cr_ruid);
}
