/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1986,1991  Sun Microsystems, Inc
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

#pragma ident	"@(#)callout.c	1.13	98/06/03 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/callo.h>
#include <sys/elf.h>
#include "crash.h"

static void prcallout(void);

#define	HEXWIDTH	(2 * sizeof (long))

/* get arguments for callout function */
void
getcallout()
{
	int    c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind])
		longjmp(syn, 0);

	fprintf(fp, "%-22s  %-*s  %-*s  %-*s\n",
	    "FUNCTION", HEXWIDTH, "ARGUMENT", HEXWIDTH, "TIME", HEXWIDTH, "ID");

	prcallout();
}

/*
 * Print all callout tables.
 */
static void
prcallout(void)
{
	callout_table_t	*callout_table[CALLOUT_TABLES];
	int		callout_fanout;
	callout_table_t	ct;   /* copy of a callout table */
	callout_t	c;    /* copy of a callout entry */
	callout_t	*cp;   /* address of callout entry in kernel mem */
	int		i, t, f, table_id;
	char		*name;

	readsym("callout_fanout", &callout_fanout, sizeof (callout_fanout));
	readsym("callout_table", &callout_table, sizeof (callout_table));

	for (t = 0; t < CALLOUT_NTYPES; t++) {
		for (f = 0; f < callout_fanout; f++) {
			table_id = CALLOUT_TABLE(t, f);
			readmem(callout_table[table_id], 1,
			    &ct, sizeof (ct), "callout table");
			for (i = 0; i < CALLOUT_BUCKETS; i++) {
				cp = ct.ct_idhash[i];
				while (cp != NULL) {
					readmem(cp, 1, &c, sizeof (c),
					    "callout table entry");
					name = addr2sym(c.c_func);
					fprintf(fp,
					    "%-22s  %0*lx  %0*lx  %0*lx\n",
					    name,
					    HEXWIDTH, c.c_arg,
					    HEXWIDTH, c.c_runtime,
					    HEXWIDTH, c.c_xid);
					cp = c.c_idnext;
				}
			}
		}
	}
}
