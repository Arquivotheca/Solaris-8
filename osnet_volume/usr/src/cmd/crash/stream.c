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

#pragma ident	"@(#)stream.c	1.27	98/07/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/fstyp.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/elf.h>
#include <sys/poll_impl.h>
#include <sys/stream.h>
#define	STREAMS_DEBUG
#include <sys/systm.h>		/* XXX for rval_t -- sigh */
#include <sys/strsubr.h>
#undef STREAMS_DEBUG
#define	_STRING_H		/* XXX strsubr defines strsignal */
#include <sys/stropts.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include "crash.h"

static void prstream(int, void *, int);
static void kmstream(void *kaddr, void *buf);
static void prmblk(void *, int);
static void kmmblk(void *kaddr, void *buf);
static void prmblkuser(void *kaddr, void *buf,
	size_t, kmem_bufctl_audit_t *bcp);
static void prlinkblk(int, void *);
static void kmlinkblk(void *kaddr, void *buf);
static void prqueue(int, void *, void *, int);
static void kmqueue(void *kaddr, void *buf);
static void prstrstat();
static void prqrun();

static int strfull;
static char *strheading =
" ADDRESS      WRQ   STRTAB    VNODE  PUSHCNT  RERR/WERR FLAG\n";
static char *mblkheading =
" ADDRESS     NEXT     PREV     CONT     RPTR     WPTR   DATAP BAND/FLAG\n"
"            CACHE     BASE      LIM  UIOBASE   UIOLIM  UIOPTR REF/TYPE/FLG\n";

/* get arguments for stream function */
void
getstream()
{
	int phys = 0;
	intptr_t addr;
	int c;

	strfull = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "fpw:")) != EOF) {
		switch (c) {
			case 'f':
				strfull = 1;
				break;
			case 'w':
				redirect();
				break;
			case 'p':
				phys = 1;
				break;
			default:
				longjmp(syn, 0);
		}
	}
	if (!strfull)
		fprintf(fp, "%s", strheading);
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				error("\n");
			prstream(1, (void *)addr, phys);
		} while (args[optind]);
	} else {
		kmem_cache_apply(kmem_cache_find("stream_head_cache"),
			kmstream);
	}
}

/*ARGSUSED1*/
static void
kmstream(void *kaddr, void *buf)
{
	prstream(0, kaddr, 0);
}

static void
prstream(int all, void *addr, int phys)
{
	stdata_t st, *stp;
	struct strsig sigbuf, *signext;
	struct polldat *pdp;
	struct polldat pdat;
	int pollevents;
	int events;

	readbuf(addr, 0, phys, &st, sizeof (st), "stream table slot");
	stp = (struct stdata *)&st;
	if (!stp->sd_wrq && !all)
		return;
	if (strfull)
		fprintf(fp, "%s", strheading);
	fprintf(fp, "%8p %8lx %8lx %8lx %6d    %d/%d", addr,
			(long)stp->sd_wrq, (long)stp->sd_strtab,
			(long)stp->sd_vnode, stp->sd_pushcnt, stp->sd_rerror,
			stp->sd_werror);
	fprintf(fp,
		"      %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		((stp->sd_flag & IOCWAIT) ? "iocw " : ""),
		((stp->sd_flag & RSLEEP) ? "rslp " : ""),
		((stp->sd_flag & WSLEEP) ? "wslp " : ""),
		((stp->sd_flag & STRPRI) ? "pri " : ""),
		((stp->sd_flag & STRHUP) ? "hup " : ""),
		((stp->sd_flag & STREOF) ? "eof " : ""),
		((stp->sd_flag & STWOPEN) ? "stwo " : ""),
		((stp->sd_flag & STPLEX) ? "plex " : ""),
		((stp->sd_flag & STRISTTY) ? "istty " : ""),
		((stp->sd_flag & STRQNEXTLESS) ? "qnl " : ""),
		((stp->sd_read_opt & RD_MSGDIS) ? "mdis " : ""),
		((stp->sd_read_opt & RD_MSGNODIS) ? "mnds " : ""),
		((stp->sd_flag & STRDERR) ? "rerr " : ""),
		((stp->sd_flag & STWRERR) ? "werr " : ""),
		((stp->sd_flag & STRCLOSE) ? "clos " : ""),
		((stp->sd_flag & SNDMREAD) ? "mrd " : ""),
		((stp->sd_flag & OLDNDELAY) ? "ondel " : ""),
		((stp->sd_flag & SNDZERO) ? "sndz " : ""),
		((stp->sd_flag & STRTOSTOP) ? "tstp " : ""),
		((stp->sd_read_opt & RD_PROTDAT) ? "pdat " : ""),
		((stp->sd_read_opt & RD_PROTDIS) ? "pdis " : ""),
		((stp->sd_flag & STRMOUNT) ? "mnt " : ""),
		((stp->sd_flag & STRDELIM) ? "delim " : ""),
		((stp->sd_wput_opt & SW_SIGPIPE) ? "spip " : ""));

	if (strfull) {
		fprintf(fp, "\t     SID     PGID   IOCBLK    IOCID  IOCWAIT\n");
		if (stp->sd_sidp) {
			fprintf(fp, "\t%8d", readpid(stp->sd_sidp));
		} else
			fprintf(fp, "\t    -   ");
		if (stp->sd_pgidp) {
			fprintf(fp, "\t%8d", readpid(stp->sd_pgidp));
		} else
			fprintf(fp, "    -   ");
		fprintf(fp, " %8lx %8d \n",
			(long)stp->sd_iocblk, stp->sd_iocid);
		fprintf(fp, "\t  WOFF     MARK CLOSTIME\n");
		fprintf(fp, "\t%6d %8lx %8d\n", stp->sd_wroff,
			(long)stp->sd_mark, stp->sd_closetime);
		fprintf(fp, "\tSIGFLAGS:  %s%s%s%s%s%s%s%s%s\n",
			((stp->sd_sigflags & S_INPUT) ? " input" : ""),
			((stp->sd_sigflags & S_HIPRI) ? " hipri" : ""),
			((stp->sd_sigflags & S_OUTPUT) ? " output" : ""),
			((stp->sd_sigflags & S_RDNORM) ? " rdnorm" : ""),
			((stp->sd_sigflags & S_RDBAND) ? " rdband" : ""),
			((stp->sd_sigflags & S_WRBAND) ? " wrband" : ""),
			((stp->sd_sigflags & S_ERROR) ? " err" : ""),
			((stp->sd_sigflags & S_HANGUP) ? " hup" : ""),
			((stp->sd_sigflags & S_MSG) ? " msg" : ""));
		fprintf(fp, "\tSIGLIST:\n");
		signext = stp->sd_siglist;
		while (signext) {
			readmem(signext, 1, &sigbuf,
				sizeof (sigbuf), "stream event buffer");
			events = sigbuf.ss_events;
			fprintf(fp, "\t\tPROC:  %3d   %s%s%s%s%s%s%s%s%s\n",
				sigbuf.ss_pid,
				((events & S_INPUT) ? " input" : ""),
				((events & S_HIPRI) ? " hipri" : ""),
				((events & S_OUTPUT) ? " output" : ""),
				((events & S_RDNORM) ? " rdnorm" : ""),
				((events & S_RDBAND) ? " rdband" : ""),
				((events & S_WRBAND) ? " wrband" : ""),
				((events & S_ERROR) ? " err" : ""),
				((events & S_HANGUP) ? " hup" : ""),
				((events & S_MSG) ? " msg" : ""));
			signext = sigbuf.ss_next;
		}
		fprintf(fp, "\tPOLLIST:\n");
		pdp = stp->sd_pollist.ph_list;
		pollevents = 0;
		while (pdp) {
			readmem(pdp, 1, &pdat, sizeof (pdat), "poll buffer");
			fprintf(fp, "\t\tTHREAD:  %#.8lx   %s%s%s%s%s%s\n",
				(long)pdat.pd_thread,
				((pdat.pd_events & POLLIN) ? " in" : ""),
				((pdat.pd_events & POLLPRI) ? " pri" : ""),
				((pdat.pd_events & POLLOUT) ? " out" : ""),
				((pdat.pd_events & POLLRDNORM) ?
							" rdnorm" : ""),
				((pdat.pd_events & POLLRDBAND) ?
							" rdband" : ""),
				((pdat.pd_events & POLLWRBAND) ?
							" wrband" : ""));
			pollevents |= pdat.pd_events;
			pdp = pdat.pd_next;
		}
		fprintf(fp, "\tPOLLFLAGS:  %s%s%s%s%s%s\n",
			((pollevents & POLLIN) ? " in" : ""),
			((pollevents & POLLPRI) ? " pri" : ""),
			((pollevents & POLLOUT) ? " out" : ""),
			((pollevents & POLLRDNORM) ? " rdnorm" : ""),
			((pollevents & POLLRDBAND) ? " rdband" : ""),
			((pollevents & POLLWRBAND) ? " wrband" : ""));
		fprintf(fp, "\n");
	}
}

static int qfull;
static char *qheading =
	" QUEADDR     INFO     NEXT     LINK      PTR     RCNT FLAG\n";

/* get arguments for queue function */
void
getqueue()
{
	int phys = 0;
	intptr_t addr;
	int c;
	qfull = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "fpw:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			case 'p':
				phys = 1;
				break;
			case 'f':
				qfull = 1;
				break;
			default:
				longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		if (!qfull)
			fprintf(fp, qheading);
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				error("\n");
			prqueue(1, (void *)addr, NULL, phys);
		} while (args[optind]);
	} else {
		if (!qfull)
			fprintf(fp, qheading);
		kmem_cache_apply(kmem_cache_find("queue_cache"), kmqueue);
	}
}

static void
kmqueue(void *kaddr, void *buf)
{
	queinfo_t *kqip = kaddr;
	queinfo_t *qip = buf;
	prqueue(0, &kqip->qu_rqueue, &qip->qu_rqueue, 0);
	prqueue(0, &kqip->qu_wqueue, &qip->qu_wqueue, 0);
}

/* print queue table */
static void
prqueue(int all, void *qaddr, void *uqaddr, int phys)
{
	queue_t q, *qp;

	if (uqaddr) {
		qp = uqaddr;
	} else {
		readmem(qaddr, !phys, &q, sizeof (q), "queue");
		qp = &q;
	}
	if (!(qp->q_flag & QUSE) && !all)
		return;

	if (qfull)
		fprintf(fp, qheading);
	fprintf(fp, "%8p ", qaddr);
	fprintf(fp, "%8lx ", (long)qp->q_qinfo);
	if (qp->q_next)
		fprintf(fp, "%8lx ", (long)qp->q_next);
	else
		fprintf(fp, "       - ");

	if (qp->q_link)
		fprintf(fp, "%8lx ", (long)qp->q_link);
	else
		fprintf(fp, "       - ");

	fprintf(fp, "%8lx", (long)qp->q_ptr);
	fprintf(fp, " %8lu ", qp->q_count);
	fprintf(fp, "%s%s%s%s%s%s%s\n",
		((qp->q_flag & QENAB) ? "en " : ""),
		((qp->q_flag & QWANTR) ? "wr " : ""),
		((qp->q_flag & QWANTW) ? "ww " : ""),
		((qp->q_flag & QFULL) ? "fl " : ""),
		((qp->q_flag & QREADR) ? "rr " : ""),
		((qp->q_flag & QUSE) ? "us " : ""),
		((qp->q_flag & QNOENB) ? "ne " : ""));
	if (!qfull)
		return;
	fprintf(fp, "\t    HEAD     TAIL     MINP     MAXP     ");
	fprintf(fp, "HIWT     LOWT BAND BANDADDR\n");
	if (qp->q_first)
		fprintf(fp, "\t%8lx ", (long)qp->q_first);
	else
		fprintf(fp, "\t       - ");
	if (qp->q_last)
		fprintf(fp, "%8lx ", (long)qp->q_last);
	else
		fprintf(fp, "       - ");
	fprintf(fp, "%8ld %8ld %8lu %8lu ",
		qp->q_minpsz,
		qp->q_maxpsz,
		qp->q_hiwat,
		qp->q_lowat);
	fprintf(fp, " %3d %8lx\n\n",
		qp->q_nband, (long)qp->q_bandp);
}

/* get arguments for mblk function */
int
getmblk()
{
	int phys = 0;
	intptr_t addr;
	int c;

	strfull = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "efpw:")) != EOF) {
		switch (c) {
			case 'e':
				break;	/* doesn't actually do anything */
			case 'f':
				strfull = 1;
				break;
			case 'w':
				redirect();
				break;
			case 'p':
				phys = 1;
				break;
			default:
				longjmp(syn, 0);
		}
	}
	if (!strfull)
		fprintf(fp, "%s", mblkheading);
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				error("\n");
			prmblk((void *)addr, phys);
		} while (args[optind]);
	} else {
		kmem_cache_apply(kmem_cache_find("streams_mblk"), kmmblk);
	}
	return (0);
}

/*ARGSUSED1*/
static void
kmmblk(void *kaddr, void *buf)
{
	prmblk(kaddr, 0);
}

/* ARGSUSED */
static void
prmblk(void *addr, int phys)
{
	mblk_t *mp, mb;
	dblk_t *dp, db;

	readbuf(addr, 0, phys, &mb, sizeof (mb), "mblk");
	dp = mb.b_datap;
	readbuf(dp, 0, phys, &db, sizeof (db), "dblk");
	mp = &mb;
	dp = &db;
	if (strfull)
		fprintf(fp, "%s", mblkheading);
	fprintf(fp, "%8p %8lx %8lx %8lx %8lx %8lx %8lx %4d/%x\n", addr,
			(long)mp->b_next, (long)mp->b_prev,
			(long)mp->b_cont, (long)mp->b_rptr, (long)mp->b_wptr,
			(long)mp->b_datap, mp->b_band, mp->b_flag);
	fprintf(fp, "\t %8lx %8lx %8lx %8lx %8lx %8lx %4d/%x/%x\n",
			(long)dp->db_cache, (long)dp->db_base,
			(long)dp->db_lim,
			(long)dp->db_struiobase,
			(long)dp->db_struiolim, (long)dp->db_struioptr,
			dp->db_ref, dp->db_type,
			dp->db_struioflag);
	if (dp->db_frtnp != NULL) {
		frtn_t fr;
		char *fname;
		readbuf(dp->db_frtnp, 0, phys, &fr,
			sizeof (frtn_t), "frtn");
		fname = try_addr2sym((void *)fr.free_func);
		if (fname)
			fprintf(fp, "\t frtn %p = %s(%p)\n",
			    dp->db_frtnp, fname, fr.free_arg);
		else
			fprintf(fp, "\t frtn %p = %p(%p)\n",
			    dp->db_frtnp, fr.free_func, fr.free_arg);
	}
}


/*
 * Print mblk/dblk usage with stack traces when KMF_AUDIT is enabled
 */
int
getmblkusers()
{
	int c;
	int mem_threshold = 1024;	/* Minimum # bytes for printing */
	int cnt_threshold = 10;		/* Minimum # blocks for printing */

	strfull = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "efw:")) != EOF) {
		switch (c) {
			case 'e':
				mem_threshold = 0;
				cnt_threshold = 0;
				break;
			case 'f':
				strfull = 1;
				break;
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}

	init_owner();
	if (kmem_cache_audit_apply(kmem_cache_find("streams_mblk"),
	    prmblkuser) != -1)
		print_owner("messages", mem_threshold, cnt_threshold);
	return (0);
}

/* ARGSUSED */
static void
prmblkuser(void *kaddr, void *buf, size_t size, kmem_bufctl_audit_t *bcp)
{
	mblk_t *mp;
	dblk_t *dp, db;
	int i;
	long data_size;

	mp = buf;
	readbuf(mp->b_datap, 0, 0, &db, sizeof (db), "dblk");
	dp = &db;
	data_size = dp->db_lim - dp->db_base;
	if (strfull) {
		fprintf(fp, "%s", mblkheading);
		fprintf(fp, "%8lx %8lx %8lx %8lx %8lx %8lx %8lx %4d/%x\n",
			(long)kaddr,
			(long)mp->b_next, (long)mp->b_prev,
			(long)mp->b_cont, (long)mp->b_rptr, (long)mp->b_wptr,
			(long)mp->b_datap, mp->b_band, mp->b_flag);
		fprintf(fp, "\t %8lx %8lx %8lx %8lx %8lx %8lx %4d/%x/%x\n",
			(long)dp->db_cache, (long)dp->db_base,
			(long)dp->db_lim,
			(long)dp->db_struiobase,
			(long)dp->db_struiolim, (long)dp->db_struioptr,
			dp->db_ref, dp->db_type,
			dp->db_struioflag);
		fprintf(fp, "\t size %ld, stack depth %d\n",
			data_size, bcp->bc_depth);
		for (i = 0; i < bcp->bc_depth; i++) {
			fprintf(fp, "\t ");
			prsymbol(NULL, bcp->bc_stack[i]);
		}
	}
	add_owner(bcp, size + data_size, data_size);
}

/* get arguments for qrun function */
int
getqrun()
{
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}
	prqrun();
	return (0);
}

/* print qrun information */
static void
prqrun()
{
	queue_t que, *q;

	readsym("qhead", &q, sizeof (q));
	fprintf(fp, "Queue slots scheduled for service: ");
	if (!q)
		fprintf(fp, "\n  NONE");
	while (q) {
		fprintf(fp, "%8lx ", (long)q);
		readmem(q, 1, &que, sizeof (que), "scanning queue list");
		q = que.q_link;
	}
	fprintf(fp, "\n");
}

/* get arguments for strstat function */
int
getstrstat()
{
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}
	prstrstat();
	return (0);
}

static void
showstrstat(kmem_cache_t *cp, int adjust_inuse)
{
	kmem_cache_stat_t kcs;
	kmem_cache_t c;

	kmem_cache_getstats(cp, &kcs);
	if (kvm_read(kd, (u_long)cp, (char *)&c, sizeof (c)) == -1)
		return;

	fprintf(fp, "%-21s %7d %7d %7d %10d %7d\n",
		c.cache_name,
		kcs.kcs_buf_total - kcs.kcs_buf_avail - adjust_inuse,
		kcs.kcs_buf_avail,
		kcs.kcs_buf_max,
		kcs.kcs_alloc,
		kcs.kcs_alloc_fail);
}

/* print stream statistics */
static void
prstrstat()
{
	queue_t q, *qp;
	int qruncnt = 0;
	int i, ncaches;
	kmem_cache_t *msg_cache[100], *cp;
	int constructed_dblks = 0;

	fprintf(fp, "ITEM                    ALLOC    FREE     MAX");
	fprintf(fp, "      TOTAL    FAIL\n");

	ncaches = kmem_cache_find_all("streams_dblk", msg_cache, 100);
	for (i = 0; i < ncaches; i++) {
		kmem_cache_stat_t kcs;
		kmem_cache_getstats(msg_cache[i], &kcs);
		constructed_dblks += kcs.kcs_buf_constructed;
	}

	showstrstat(kmem_cache_find("streams_mblk"), constructed_dblks);
	if ((cp = kmem_cache_find("streams_mblk.DEBUG")) != NULL) {
		showstrstat(kmem_cache_find("streams_mblk"), 0);
		showstrstat(cp, constructed_dblks);
	} else {
		showstrstat(kmem_cache_find("streams_mblk"), constructed_dblks);
	}
	for (i = 0; i < ncaches; i++)
		showstrstat(msg_cache[i], 0);
	showstrstat(kmem_cache_find("stream_head_cache"), 0);
	showstrstat(kmem_cache_find("queue_cache"), 0);
	showstrstat(kmem_cache_find("syncq_cache"), 0);
	showstrstat(kmem_cache_find("linkinfo_cache"), 0);
	showstrstat(kmem_cache_find("strevent_cache"), 0);
	showstrstat(kmem_cache_find("qband_cache"), 0);

	readsym("qhead", &qp, sizeof (qp));
	while (qp) {
		qruncnt++;
		readmem(qp, 1, &q, sizeof (q), "queue run list");
		qp = q.q_link;
	}
	fprintf(fp, "\nCount of scheduled queues:%4d\n", qruncnt);
}

/* get arguments for linkblk function */
void
getlinkblk()
{
	intptr_t addr;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "pw:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			case 'p':
				/* do nothing */
				break;
			default:
				longjmp(syn, 0);
		}
	}
	fprintf(fp, "LBLKADDR     QTOP     QBOT FILEADDR    MUXID\n");
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				error("\n");
			prlinkblk(1, (void *)addr);
		} while (args[optind]);
	} else {
		kmem_cache_apply(kmem_cache_find("linkinfo_cache"), kmlinkblk);
	}
}

/* ARGSUSED1 */
static void
kmlinkblk(void *kaddr, void *buf)
{
	prlinkblk(0, kaddr);
}

/* print linkblk table */
static void
prlinkblk(int all, void *addr)
{
	struct linkinfo linkbuf;
	struct linkinfo *lp;

	readmem(addr, 1, &linkbuf, sizeof (linkbuf), "linkblk table");
	lp = &linkbuf;
	if (!lp->li_lblk.l_qbot && !all)
		return;
	fprintf(fp, "%8p", addr);
	fprintf(fp, " %8lx", (long)lp->li_lblk.l_qtop);
	fprintf(fp, " %8lx", (long)lp->li_lblk.l_qbot);
	fprintf(fp, " %8lx", (long)lp->li_fpdown);
	if (lp->li_lblk.l_qbot)
		fprintf(fp, " %8d\n", lp->li_lblk.l_index);
	else
		fprintf(fp, "        -\n");
}
