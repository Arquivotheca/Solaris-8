/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)putspent.c	1.4	96/12/04 SMI"

/*LINTLIBRARY*/

/*
 * format a shadow file entry
 *
 * This code used to live in getspent.c
 */

#pragma weak putspent = _putspent

#include "synonyms.h"
#include <stdio.h>
#include <shadow.h>

int
putspent(const struct spwd *p, FILE *f)
{
	(void) fprintf(f, "%s:%s:", p->sp_namp,
		p->sp_pwdp ? p->sp_pwdp : "");
		/* pwdp could be null for +/- entries */
	if (p->sp_lstchg >= 0)
		(void) fprintf(f, "%d:", p->sp_lstchg);
	else
		(void) fprintf(f, ":");
	if (p->sp_min >= 0)
		(void) fprintf(f, "%d:", p->sp_min);
	else
		(void) fprintf(f, ":");
	if (p->sp_max >= 0)
		(void) fprintf(f, "%d:", p->sp_max);
	else
		(void) fprintf(f, ":");
	if (p->sp_warn > 0)
		(void) fprintf(f, "%d:", p->sp_warn);
	else
		(void) fprintf(f, ":");
	if (p->sp_inact > 0)
		(void) fprintf(f, "%d:", p->sp_inact);
	else
		(void) fprintf(f, ":");
	if (p->sp_expire > 0)
		(void) fprintf(f, "%d:", p->sp_expire);
	else
		(void) fprintf(f, ":");
	if (p->sp_flag != 0)
		(void) fprintf(f, "%d\n", p->sp_flag);
	else
		(void) fprintf(f, "\n");

	(void) fflush(f);
	return (ferror(f));
}
