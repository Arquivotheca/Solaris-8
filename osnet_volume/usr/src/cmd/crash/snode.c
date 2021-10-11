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

#pragma ident	"@(#)snode.c	1.10	98/03/30 SMI"

/*
 * This file contains code for the crash functions:  snode.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/vnode.h>
#define	_KERNEL
#include <sys/fs/snode.h>
#undef _KERNEL
#include "crash.h"

static void *Stable;

static void prsnode();

/* get arguments for snode function */
int
getsnode()
{
	long slot = -1;
	int phys = 0;
	int full = 0;
	int all = 0;
	int lock = 0;
	long addr = -1;
	long arg1 = -1;
	long arg2 = -1;
	int c;
	char *heading =
"HASH-SLOT MAJ/MIN  REALVP    COMMONVP  NEXTR  SIZE   COUNT FLAGS\n";

	Stable = sym2addr("stable");
	optind = 1;
	while ((c = getopt(argcnt, args, "eflpw:")) != EOF) {
		switch (c) {
			case 'e' :	all = 1;
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
		}
	}

	fprintf(fp, "SNODE TABLE SIZE = %d\n", STABLESIZE);
	if (!full && !lock)
		fprintf(fp, "%s", heading);
	if (args[optind]) {
		all = 1;
		do {
			getargs(STABLESIZE, &arg1, &arg2, phys);
			if (arg1 == -1)
				continue;
			if (arg2 != -1)
				for (slot = arg1; slot <= arg2; slot++)
					prsnode(all, full, slot, phys, addr,
						heading, lock);
			else {
				if ((unsigned long)arg1 < STABLESIZE)
					slot = arg1;
				else
					addr = arg1;
				prsnode(all, full, slot, phys, addr, heading,
					lock);
			}
			slot = addr = arg1 = arg2 = -1;
		} while (args[++optind]);
	} else for (slot = 0; slot < STABLESIZE; slot++)
		prsnode(all, full, slot, phys, addr, heading, lock);
	return (0);
}



/* print snode table */
static void
prsnode(all, full, slot, phys, addr, heading, lock)
int all, full;
long slot;
int phys;
long addr;
char *heading;
int lock;
{
	struct snode *snp, snbuf;

	if (addr == -1) {
		readbuf((void *)addr,
		    (long)Stable + slot * sizeof (snp), phys,
		    &snp, sizeof (snp), "snode address");
		if (snp == 0)
			return;
	}
	readbuf((void *)addr, (long)snp, phys, &snbuf, sizeof (snbuf),
	    "snode table");

	for (;;) {
		if (!snbuf.s_count && !all)
			return;
		if (full || lock)
			fprintf(fp, "\n%s", heading);
		if (slot == -1)
			fprintf(fp, "  - ");
		else fprintf(fp, "%4ld", slot);
		fprintf(fp, "    %4u,%-3u %8p    %8p %4lu %5lu    %5d ",
			getemajor(snbuf.s_dev),
			geteminor(snbuf.s_dev),
			snbuf.s_realvp,
			snbuf.s_commonvp,
			snbuf.s_nextr,
			snbuf.s_size,
			snbuf.s_count);

		fprintf(fp, "%s%s%s%s%s%s%s\n",
			snbuf.s_flag & SUPD ? " up" : "",
			snbuf.s_flag & SACC ? " ac" : "",
			snbuf.s_flag & SCHG ? " ch" : "",
			snbuf.s_flag & SPRIV ? " pv" : "",
			snbuf.s_flag & SLOFFSET ? " loff" : "",
			snbuf.s_flag & SLOCKED ? " lock" : "",
			snbuf.s_flag & SWANT ? " want" : "");
		if (lock) {
			fprintf(fp, "\ns_lock: ");
			prmutex(&(snbuf.s_lock));
		}
		if (full) {
			/* print vnode info */
			fprintf(fp, "\nVNODE :\n");
			fprintf(fp, "VCNT VFSMNTED   VFSP   STREAMP VTYPE   ");
			fprintf(fp, "RDEV VDATA       VFILOCKS   VFLAG \n");
			prvnode(&snbuf.s_vnode, lock);
			fprintf(fp, "\n");
		}

		if ((addr != -1) || (snbuf.s_next == NULL))
			return;
		snp = snbuf.s_next;
		readbuf((void *)addr, (long)snp, phys, (char *)&snbuf,
		    sizeof (snbuf), "snode table");
	}
}
