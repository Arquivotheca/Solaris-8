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

#pragma ident	"@(#)search.c	1.8	98/03/30 SMI"

/*
 * This file contains code for the crash function search.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/elf.h>
#include "crash.h"

extern void *calloc();
extern void free();

static void prsearch(long, long, long, long, int);

#define	min(a, b) (a > b? b : a)
#define	pageround(x) ((x + _mmu_pagesize) & ~(_mmu_pagesize - 1))

/* get arguments for search function */
void
getsearch()
{
	long mask = -1;
	int phys = 0;
	int c;
	long pat, start, len;
	Sym *sp;

	optind = 1;
	while ((c = getopt(argcnt, args, "pw:m:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			case 'p' :	phys = 1;
					break;
			case 'm' :	if ((mask = strcon(optarg, 'h')) == -1)
						error("\n");
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		pat = strcon(args[optind++], 'h');
		if (pat == -1)
			error("\n");
		if (args[optind]) {
			if (*args[optind] == '(') {
				if ((start = eval(++args[optind])) == -1)
					error("\n");
			} else if (sp = symsrch(args[optind]))
				start = (long)sp->st_value;
			else if (isasymbol(args[optind]))
				error("%s not found in symbol table\n",
					args[optind]);
				else if ((start = strcon(args[optind], 'h'))
									== -1)
						error("\n");
			if (args[++optind]) {
				if ((len = strcon(args[optind++], 'h')) == -1)
					error("\n");
				prsearch(mask, pat, start, len, phys);
			} else longjmp(syn, 0);
		} else longjmp(syn, 0);
	} else longjmp(syn, 0);
}

/* print results of search */
void
prsearch(mask, pat, start, len, phys)
long mask, pat, start, len;
int phys;
{
	ulong_t *buf;
	int i;
	unsigned long n;
	long remainder;
	long _mmu_pagesize;

	readsym("_mmu_pagesize", &_mmu_pagesize, sizeof (_mmu_pagesize));
	buf = calloc((_mmu_pagesize / sizeof (_mmu_pagesize)), sizeof (*buf));
	fprintf(fp, "MASK = %ld, PATTERN = %ld, START = %ld, LENGTH = %ld\n\n",
		mask, pat, start, len);
	while (len > 0)  {
		remainder = pageround(start) - start;
		n = min(remainder, len);
		readbuf((void *)start, (long)start, phys, buf, n, "buffer");
		for (i = 0; i < n / sizeof (int); i++)
			if ((buf[i] & mask) == (pat & mask)) {
				fprintf(fp, "MATCH AT %8lu: %8lu\n",
					start + i * sizeof (int), buf[i]);
			}
		start += n;
		len -= n;
	}
	free(buf);
}
