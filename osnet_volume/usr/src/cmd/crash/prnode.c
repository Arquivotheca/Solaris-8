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

#pragma ident	"@(#)prnode.c	1.9	98/06/06 SMI"

/*
 * This file contains code for the crash function:  prnode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/proc/prdata.h>
#include "crash.h"

static void prprnode(int, long, int, void *, char *, int);

/* get arguments for prnode function */
int
getprnode()
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
"SLOT    pr_next   pr_flags    pr_type    pr_mode     pr_ino   pr_hatid\n"
"      pr_common pr_pcommon  pr_parent   pr_files   pr_index pr_pidfile\n"
"      pr_realvp   pr_owner\n";

	optind = 1;
	while ((c = getopt(argcnt, args, "eflpw:")) != EOF) {
		switch (c) {
			case 'e' :
					break;
			case 'f' :	full = 1;
					break;
			case 'l' :	lock = 1;
					break;
			case 'p' :	phys = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
					break;
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
					prprnode(full, slot, phys, addr,
						heading, lock);
			else {
				if ((unsigned long)arg1 < vbuf.v_proc)
					slot = arg1;
				else
					addr = (void *)arg1;
				prprnode(full, slot, phys, addr,
					heading, lock);
			}
			slot = arg1 = arg2 = -1;
			addr = (void *)-1;
		} while (args[++optind]);
	} else for (slot = 0; slot < vbuf.v_proc; slot++)
		prprnode(full, slot, phys, addr, heading, lock);
	return (0);
}

/* print prnode */
static void
prprnode(int full, long slot, int phys, void *addr, char *heading, int lock)
{
	struct proc pbuf;
	struct vnode vnbuf;
	struct prnode prnbuf;
	proc_t *procaddr;

	if (addr != (void *)-1) {
		readbuf(addr, 0, phys, (char *)&prnbuf,
		    sizeof (prnbuf), "prnode");
	} else {
		procaddr = slot_to_proc(slot);
		if (procaddr)
			readbuf(procaddr, 0, phys, &pbuf,
			    sizeof (pbuf), "proc table");
		else
			return;
		if (pbuf.p_trace == 0)
			return;
		readmem(pbuf.p_trace, 1, &vnbuf, sizeof (vnbuf), "vnode");
		readmem(vnbuf.v_data, 1, &prnbuf, sizeof (prnbuf),
		    "prnode");
	}

	if (full)
		fprintf(fp, "%s", heading);
	if (slot == -1)
		fprintf(fp, "  - ");
	else fprintf(fp, "%4ld", slot);

	fprintf(fp, " 0x%.8lx 0x%.8x %10d %10#o 0x%.8lx 0x%.8x\n",
		(uintptr_t)prnbuf.pr_next,
		prnbuf.pr_flags,
		prnbuf.pr_type,
		(uint_t)prnbuf.pr_mode,
		prnbuf.pr_ino,
		prnbuf.pr_hatid);
	fprintf(fp, "     0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx %10u 0x%.8lx\n",
		(uintptr_t)prnbuf.pr_common,
		(uintptr_t)prnbuf.pr_pcommon,
		(uintptr_t)prnbuf.pr_parent,
		(uintptr_t)prnbuf.pr_files,
		prnbuf.pr_index,
		(uintptr_t)prnbuf.pr_pidfile);
	fprintf(fp, "     0x%.8lx 0x%.8lx\n",
		(uintptr_t)prnbuf.pr_realvp,
		(uintptr_t)prnbuf.pr_owner);

	if (!full)
		return;
	/* print vnode info */
	fprintf(fp, "\nVNODE :\n");
	fprintf(fp, "VCNT VFSMNTED   VFSP   STREAMP VTYPE   RDEV VDATA    ");
	fprintf(fp, "   VFILOCKS   VFLAG \n");
	prvnode(&prnbuf.pr_vnode, lock);
	fprintf(fp, "\n");
}
