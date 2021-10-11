/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1991, 1996 by Sun Microsystems, Inc
 *	All rights reserved.
 *
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

#pragma ident	"@(#)page.c	1.18	98/03/30 SMI"

/*
 * This file contains code for the crash function: as.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/elf.h>
#include <vm/as.h>
#include <vm/page.h>
#include "crash.h"

/* symbol pointers */
extern Sym *V;		/* ptr to var structure */

static void pras(int, long, int, void *, char *, int);
static void prsegs(struct as *, struct as *, int, void *);


/* get arguments for as function */
int
getas()
{
	long slot = -1;
	int full = 0;
	int lock = 0;
	int phys = 0;
	void *addr = (void *)-1;
	long arg1 = -1;
	long arg2 = -1;
	int c;
	char *heading =
	    "PROC        PAGLCK   CLGAP  VBITS HAT        HRM         RSS\n"
	    " SEGLST     LOCK        SEGS       SIZE     LREP TAIL     NSEGS\n";

	optind = 1;
	while ((c = getopt(argcnt, args, "eflpw:")) != EOF) {
		switch (c) {
			case 'e' :
					break;
			case 'f' :	full = 1;
					break;
			case 'l' :	lock = 1;
					break;
			case 'w' :	redirect();
					break;
			case 'p' :	phys = 1;
					break;
			default  :	longjmp(syn, 0);
		}
	}

	if (!full)
		fprintf(fp, "%s", heading);

	if (args[optind]) {
		do {
			getargs(vbuf.v_proc, &arg1, &arg2, phys);
			if (arg1 == -1)
				continue;
			if (arg2 != -1)
				for (slot = arg1; slot <= arg2; slot++)
					pras(full, slot, phys, addr,
						heading, lock);
			else {
				if ((unsigned long)arg1 < vbuf.v_proc)
					slot = arg1;
				else
					addr = (void *)arg1;
				pras(full, slot, phys, addr, heading, lock);
			}
			slot = arg1 = arg2 = -1;
		} while (args[++optind]);
	} else {
		readmem((void *)V->st_value, 1, (char *)&vbuf,
			sizeof (vbuf), "var structure");
		for (slot = 0; slot < vbuf.v_proc; slot++)
			pras(full, slot, phys, addr, heading, lock);
	}
	return (0);
}


/* print address space structure */
static void
pras(int full, long slot, int phys, void *addr, char *heading, int lock)
{
	struct proc prbuf, *procaddr;
	struct as asbuf;
	struct seg *seg, *seglast;

	if (slot != -1)
		procaddr = slot_to_proc(slot);
	else
		procaddr = addr;

	if (procaddr) {
		readbuf(procaddr, 0, phys, &prbuf, sizeof (prbuf),
		    "proc table");
	} else {
		return;
	}

	if (full)
		fprintf(fp, "\n%s", heading);

	if (slot == -1)
		fprintf(fp, "%p  ", procaddr);
	else
		fprintf(fp, "%4ld  ", slot);

	if (prbuf.p_as == NULL) {
		fprintf(fp, "- no address space.\n");
		return;
	}

	readbuf((void *)-1, (long)(prbuf.p_as), phys, (char *)&asbuf,
		sizeof (asbuf), "as structure");
	if (asbuf.a_lrep == AS_LREP_LINKEDLIST) {
		seg = asbuf.a_segs.list;
		seglast = asbuf.a_cache.seglast;
	} else {
		seg_skiplist ssl;
		ssl_spath spath;

		readbuf(addr, (intptr_t)asbuf.a_segs.skiplist, phys,
		    &ssl, sizeof (ssl), "skiplist structure");
		seg = ssl.segs[0];
		readbuf(addr, (intptr_t)asbuf.a_cache.spath, phys,
		    &spath, sizeof (spath), "spath structure");
		readbuf(addr, (intptr_t)spath.ssls[0], phys,
		    &ssl, sizeof (ssl), "skiplist structure");
		seglast = ssl.segs[0];
	}

	fprintf(fp, "%7d  %7d      0x%x   0x%-8x   0x%-8x\n",
		AS_ISPGLCK(&asbuf),
		AS_ISCLAIMGAP(&asbuf),
		asbuf.a_vbits,
		asbuf.a_hat,
		asbuf.a_hrm);
	fprintf(fp, "0x%-7x  0x%-7x  0x%-7x  %7d  %d     0x%-7x  %4d\n",
		seglast,
		&asbuf.a_lock,
		seg,
		asbuf.a_size,
		asbuf.a_lrep,
		asbuf.a_tail,
		asbuf.a_nsegs);

	if (full) {
		prsegs(prbuf.p_as, (struct as *)&asbuf, phys, addr);
	}
	if (lock) {
		fprintf(fp, "\na_contents: ");
		prmutex(&(asbuf.a_contents));
		prcondvar(&asbuf.a_cv, "a_cv");
		fprintf(fp, "a_lock: ");
		prrwlock(&(asbuf.a_lock));
	}
}


/* print list of seg structures */
static void
prsegs(struct as *as, struct as *asbuf, int phys, void *addr)
{
	struct seg *seg;
	struct seg  segbuf;
	seg_skiplist ssl;
	Sym *sp;
	extern char *strtbl;

	if (asbuf->a_lrep == AS_LREP_LINKEDLIST) {
		seg = asbuf->a_segs.list;
	} else {
		readbuf(addr, (intptr_t)asbuf->a_segs.skiplist, phys,
		    &ssl, sizeof (ssl), "skiplist structure");
		seg = ssl.segs[0];
	}

	if (seg == NULL)
		return;

	fprintf(fp,
"    BASE       SIZE     AS       NEXT        PREV         OPS        DATA\n");

	do {
		readbuf(addr, (intptr_t)seg, phys, (char *)&segbuf,
			sizeof (segbuf), "seg structure");
		if (asbuf->a_lrep == AS_LREP_LINKEDLIST) {
			seg = segbuf.s_next.list;
		} else {
			readbuf(addr, (intptr_t)segbuf.s_next.skiplist,
				phys, (char *)&ssl,
				sizeof (ssl), "skiplist structure");
			seg = ssl.segs[0];
		}
		fprintf(fp, "   0x%8p %6ld 0x%8p 0x%8p 0x%8p ",
			segbuf.s_base,
			segbuf.s_size,
			segbuf.s_as,
			seg,
			segbuf.s_prev);

		/*
		 * Try to find a symbolic name for the sops vector. If
		 * can't find one print the hex address.
		 */
		sp = findsym((unsigned long)segbuf.s_ops);
		if ((!sp) || ((unsigned long)segbuf.s_ops != sp->st_value))
			fprintf(fp, "0x%08p  ", segbuf.s_ops);
		else
			fprintf(fp, "%10.10s  ", strtbl+sp->st_name);

		fprintf(fp, "0x%08p\n", segbuf.s_data);

		if (segbuf.s_as != as) {
			fprintf(fp,
"WARNING - seg was not pointing to the correct as struct: 0x%8p\n",
				segbuf.s_as);
			fprintf(fp, "          seg list traversal aborted.\n");
			return;
		}
	} while (seg != NULL);
}
