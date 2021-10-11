/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)thread.c	1.8	98/03/30 SMI"

/*
 * This file contains code for the crash functions:  class, claddr
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/elf.h>
#include <sys/thread.h>
#include "crash.h"

static void *prthread(void *, int, int, int, char *);
static void prdefthread();

static Sym *Thread;

/* get arguments for class function */
int
getthread()
{
	int all = 0;
	int full = 0;
	int phys = 0;
	int c;
	intptr_t addr, oaddr;
	char *heading =
	    "    LINK     FLAG SCHEDFLG    STATE "
	    "     PRI   WCHAN0    WCHAN     INTR\n    "
	    "     TID      LWP     FORW     BACK "
	    "   PROCP     CRED  KPRIREQ  PREEMPT\n";

	optind = 1;
	while ((c = getopt(argcnt, args, "efpw:")) != EOF) {
		switch (c) {
			case 'e' :	all = 1;
					break;
			case 'f' :	full = 1;
					break;
			case 'p' :	phys = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	if (!full)
		fprintf(fp, "%s", heading);

	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind], 'h')) == -1)
				continue;
			(void) prthread((void *)addr, phys, all, full, heading);
		} while (args[++optind]);
	} else if (all) {
		if (!Thread)
			if (!(Thread = symsrch("allthreads")))
				error("thread not found in symbol table\n");

		readmem((void *)Thread->st_value, 1, &addr, sizeof (addr),
				"address of head of thread list");
		oaddr = addr;
		do {
			addr = (intptr_t)prthread((void *)addr, phys, 0,
			    full, heading);
		} while (addr != oaddr);
	} else
		(void) prthread(Curthread, 0, all, full, heading);
	return (0);
}

/* print class table  */
static void *
prthread(void *addr, int phys, int all, int full, char *heading)
{
	kthread_t tb;
	void *orig = addr;

	do {
		if (full)
			fprintf(fp, "\n%s", heading);

		readbuf(addr, 0, phys, &tb, sizeof (tb), "thread");

		fprintf(fp, "%8p %8x %8x %8x %8x %8ld %8p %8p\n",
		    tb.t_link,
		    tb.t_flag,
		    tb.t_schedflag,
		    tb.t_state,
		    tb.t_pri,
		    tb.t_wchan0,
		    tb.t_wchan,
		    tb.t_intr);
		fprintf(fp, "    %8x %8p %8p %8p %8p %8p %8x %8x\n",
		    tb.t_tid,
		    tb.t_lwp,
		    tb.t_forw,
		    tb.t_next,
		    tb.t_procp,
		    tb.t_cred,
		    tb.t_kpri_req,
		    tb.t_preempt);
		if (full) {
			fprintf(fp, "          PC       SP      CID      CTX "
			    "     CPU      SIG     HOLD\n");
			fprintf(fp, "    %8ld %8ld %8x %8p %8p %8x %8x\n",
			    tb.t_pc,
			    tb.t_sp,
			    tb.t_cid,
			    tb.t_ctx,
			    tb.t_cpu,
			    tb.t_sig,
			    tb.t_hold);
		}
		if (all)
			addr = tb.t_forw;
	} while (all && addr != orig);
	return (tb.t_next);
}

int
getdefthread()
{
	int c;
	long thread = -1;
	int reset = 0;
	int change = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "cprw:")) != EOF) {
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
		if ((thread = strcon(args[optind], 'h')) == -1)
			error("\n");
	prdefthread(thread, change, reset);
	return (0);
}

/* print results of defproc function */
void
prdefthread(thread, change, reset)
long thread;
int change, reset;
{
	struct _kthread t;

	if (change)
		Curthread = getcurthread();
	else if (thread != -1)
		Curthread = (kthread_id_t)thread;
	if (reset) {
		readmem(Curthread, 1, &t, sizeof (t), "thread struct");
		Procslot = proc_to_slot((long)t.t_procp);
	}
	fprintf(fp, "Current Thread = %p\n", Curthread);
}
