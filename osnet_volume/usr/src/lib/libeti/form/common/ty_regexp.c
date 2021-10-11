/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)ty_regexp.c	1.5	97/09/17 SMI" /* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdlib.h>
#include "utility.h"

/*
 *	TYPE_REGEXP standard type
 *
 *	usage:
 *		set_field_type(f, TYPE_REGEXP, expression);
 *
 *		char * expression;	regular expression REGCMP(3X)
 */
static char *make_rexp(va_list *);
static char *copy_rexp(char *);
static void free_rexp(char *);
static int fcheck_rexp(FIELD *, char *);

static FIELDTYPE typeREGEXP =
{
				ARGS,			/* status	*/
				1,			/* ref		*/
				(FIELDTYPE *) 0,	/* left		*/
				(FIELDTYPE *) 0,	/* right	*/
				make_rexp,		/* makearg	*/
				copy_rexp,		/* copyarg	*/
				free_rexp,		/* freearg	*/
				fcheck_rexp,		/* fcheck	*/
				(PTF_int) 0,		/* ccheck	*/
				(PTF_int) 0,		/* next		*/
				(PTF_int) 0,		/* prev		*/
};

FIELDTYPE * TYPE_REGEXP = &typeREGEXP;

static char *
make_rexp(va_list *ap)
{
	return (regcmp_p2(va_arg(*ap, char *), 0)); /* (...)$n will dump core */
}

static char *
copy_rexp(char *arg)
{
	char * rexp;

	if (arrayAlloc(rexp, (strlen(arg) + 1), char))
		(void) strcpy(rexp, arg);
	return (rexp);
}

static void
free_rexp(char *arg)
{
	Free(arg);
}

static int
fcheck_rexp(FIELD *f, char *arg)
{
	return (regex_p2(arg, field_buffer(f, 0)) ? TRUE : FALSE);
}
