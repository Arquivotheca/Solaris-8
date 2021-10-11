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

#pragma ident	"@(#)class.c	1.7	98/03/30 SMI"

/*
 * This file contains code for the crash functions:  class, claddr
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/priocntl.h>
#include <sys/class.h>
#include <sys/elf.h>
#include <stdlib.h>
#include "crash.h"

static Sym *Cls;	/* namelist symbol */

static void prclass();

/* get arguments for class function */
int
getclass()
{
	long slot = -1;
	int c;
	long arg1 = -1;
	long arg2 = -1;
	sclass_t *classaddr;

	char *classhdg = "SLOT\tCLASS\tINIT FUNCTION\tCLASS FUNCTION\n\n";
	long nclass;

	if (!Cls)
		if (!(Cls = symsrch("sclass")))
			error("sclass not found in symbol table\n");

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	/* Determine how many entries are in the class table */

	nclass = priocntl(0, 0, PC_GETCLINFO, NULL);

	/* Allocate enough space to read in the whole table at once */

	classaddr = (sclass_t *)malloc(nclass * sizeof (sclass_t));

	/* Read in the entire class table */

	readmem((void *)Cls->st_value, 1, (char *)classaddr,
		nclass * sizeof (sclass_t), "class table");

	fprintf(fp, "%s", classhdg);

	if (args[optind]) {
		do {
			getargs(nclass, &arg1, &arg2, 0);
			if (arg1 == -1)
				continue;
			if (arg2 != -1)
				for (slot = arg1; slot <= arg2; slot++)
					prclass(slot, classaddr);
			else {
				if (arg1 < nclass)
					slot = arg1;
				prclass(slot, classaddr);
			}
			slot = arg1 = arg2 = -1;
		} while (args[++optind]);
	} else for (slot = 0; slot < nclass; slot++)
		prclass(slot, classaddr);

	free(classaddr);
	return (0);
}

/* print class table  */
static void
prclass(slot, classaddr)
long slot;
sclass_t *classaddr;
{
	char name[PC_CLNMSZ];

	readmem(classaddr[slot].cl_name, 1, name, sizeof (name), "class name");

	fprintf(fp, "%ld\t%s\t%p\t%p\n", slot, name, classaddr[slot].cl_init,
		classaddr[slot].cl_funcs);
}
