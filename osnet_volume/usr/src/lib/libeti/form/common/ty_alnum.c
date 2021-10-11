/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)ty_alnum.c	1.5	97/09/17 SMI" /* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdlib.h>
#include "utility.h"

/*
 *	TYPE_ALNUM
 *
 *	usage:
 *		set_field_type(f, TYPE_ALNUM, width);
 *
 *		int width;	minimum token width
 */
static char * make_alnum(va_list *);
static char * copy_alnum(char *);
static void free_alnum(char *);
static int fcheck_alnum(FIELD *, char *);
static int ccheck_alnum(int, char *);

static FIELDTYPE typeALNUM =
{
				ARGS,			/* status	*/
				1,			/* ref		*/
				(FIELDTYPE *) 0,	/* left		*/
				(FIELDTYPE *) 0,	/* right	*/
				make_alnum,		/* makearg	*/
				copy_alnum,		/* copyarg	*/
				free_alnum,		/* freearg	*/
				fcheck_alnum,		/* fcheck	*/
				ccheck_alnum,		/* ccheck	*/
				(PTF_int) 0,		/* next		*/
				(PTF_int) 0,		/* prev		*/
};

FIELDTYPE * TYPE_ALNUM = &typeALNUM;

static char *
make_alnum(va_list *ap)
{
	int * width;

	if (Alloc(width, int))
		*width = va_arg(*ap, int);
	return ((char *) width);
}

static char *
copy_alnum(char *arg)
{
	int * width;

	if (Alloc(width, int))
		*width = *((int *) arg);
	return ((char *) width);
}

static void
free_alnum(char *arg)
{
	Free(arg);
}

static int
fcheck_alnum(FIELD *f, char *arg)
{
	int	width	= *((int *) arg);
	int	n	= 0;
	char 	*v	= field_buffer(f, 0);

	while (*v && *v == ' ')
		++v;
	if (*v) {
		char * vbeg = v;
		while (*v && isalnum(*v))
			++v;
		n = (int) (v - vbeg);
		while (*v && *v == ' ')
			++v;
	}
	return (*v || n < width ? FALSE : TRUE);
}

/*ARGSUSED*/
static int
ccheck_alnum(int c, char *arg)
{
	return (isalnum(c));
}
