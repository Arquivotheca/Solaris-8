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

#pragma ident	"@(#)pcfs.c	1.5	98/03/30 SMI"

/*
 * This file contains code for the crash functions:  pcfs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/vnode.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>
#include "crash.h"

static struct pcnode *prpcfsnode(void *);
static void prpcfsnodes(char *);

getpcfsnode()
{
	int c;
	intptr_t addr;
	char *heading = "FLAGS	VNODE		SIZE	SCLUSTER\n";

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	fprintf(fp, "%s\n", heading);
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			prpcfsnode((void *)addr);
		} while (args[++optind]);
	} else {
		prpcfsnodes("pcfhead");
		prpcfsnodes("pcdhead");
	}
	return (0);
}


#define	NPCHASH	1		/* from fs/pc_node.h */

static void
prpcfsnodes(char *listname)
{
	void *pc_sym;
	struct pchead pch;
	struct pcnode *pcp;

	pc_sym = sym2addr(listname);
	readmem(pc_sym, 1, &pch, sizeof (pch), listname);
	pcp = pch.pch_forw;
	while (pcp != NULL && pcp != pc_sym)
		pcp = prpcfsnode(pcp);
}

static struct pcnode *
prpcfsnode(void *addr)
{
	struct pcnode pc;

	readmem(addr, 1, (char *)&pc, sizeof (pc), "pcfs node");
	fprintf(fp, "%x	%p	%d	%x\n",
		pc.pc_flags, pc.pc_vn, pc.pc_size, pc.pc_scluster);
	return (pc.pc_forw);
}
