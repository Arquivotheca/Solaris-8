/*	Copyright (c) 1984 AT&T	*/
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

#pragma ident	"@(#)vtop.c	1.10	98/04/04 SMI"

/*
 * This file contains code for the crash functions:  vtop and mode, as well as
 * the virtual to physical offset conversion routine vtop.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <vm/as.h>
#include <sys/elf.h>
#include "crash.h"

static void prvtop(intptr_t, long);
static void prmode(char *);

/* virtual to physical offset address translation */
longlong_t
vtop(intptr_t vaddr, long slot)
{
	struct proc procbuf;

	readbuf((void *)-1, (off_t)slottab[slot].p, 0,
	    &procbuf, sizeof (procbuf), "proc table");
	if (!procbuf.p_stat)
		return (-1LL);
	if (procbuf.p_as == NULL)
		return (-1LL);
	return (kvm_physaddr(kd, procbuf.p_as, vaddr));
}

/* get arguments for vtop function */
int
getvtop(void)
{
	long proc = 0;
	Sym *sp;
	intptr_t addr;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:s:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			case 's' :	proc = setproc();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		fprintf(fp, " VIRTUAL  PHYSICAL\n");
		do {
			if (*args[optind] == '(') {
				if ((addr = eval(++args[optind])) == -1)
					continue;
				prvtop(addr, proc);
			} else if (sp = symsrch(args[optind]))
				prvtop((intptr_t)sp->st_value, proc);
			else if (isasymbol(args[optind]))
				error("%s not found in symbol table\n",
					args[optind]);
			else {
				if ((addr = strcon(args[optind], 'h')) == -1)
					continue;
				prvtop(addr, proc);
			}
		} while (args[++optind]);
	}
	else
		longjmp(syn, 0);
	return (0);
}

/* print vtop information */
static void
prvtop(intptr_t addr, long proc)
{
	longlong_t paddr;

	paddr = vtop(addr, proc);
	if (paddr == -1LL)
		fprintf(fp, "%8lx not mapped\n", addr);
	else
		fprintf(fp, "%8lx %16llx\n", addr, paddr);
}

/* get arguments for mode function */
int
getmode(void)
{
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind])
		prmode(args[optind]);
	else
		prmode("s");
	return (0);
}

/* print mode information */
static void
prmode(char *mode)
{
	switch (*mode) {
		case 'p' :  Virtmode = 0;
			    break;
		case 'v' :  Virtmode = 1;
			    break;
		case 's' :  break;
		default  :  longjmp(syn, 0);
	}
	if (Virtmode)
		fprintf(fp, "Mode = virtual\n");
	else
		fprintf(fp, "Mode = physical\n");
}
