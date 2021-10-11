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

#pragma ident	"@(#)init.c	1.16	98/06/03 SMI"

/*
 * This file contains code for the crash initialization.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include "crash.h"

kvm_t *kd;

long _userlimit;

/* initialize buffers, symbols, and global variables for crash session */
void
init(void)
{
	kd = kvm_open(namelist, dumpfile, NULL, O_RDONLY, "crash");
	if (kd == NULL)
		fatal("cannot open kvm - dump file %s\n", dumpfile);

	rdsymtab(); /* open and read the symbol table */

	if (!(V = symsrch("v")))
		fatal("var structure not found in symbol table\n");
	if (!(Start = symsrch("_start")))
		fatal("start not found in symbol table\n");
	readsym("_userlimit", &_userlimit, sizeof (_userlimit));
	if (!(Panic = symsrch("panicstr")))
		fatal("panicstr not found in symbol table\n");

	readmem((void *)V->st_value, 1, (char *)&vbuf,
		sizeof (vbuf), "var structure");

	Curthread = getcurthread();
	Procslot = getcurproc();

	/* setup break signal handling */
	if (signal(SIGINT, sigint) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
}
