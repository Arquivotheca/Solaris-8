/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)ty_num.c	1.5	97/09/17 SMI" /* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdlib.h>
#include "utility.h"

/*
 *	TYPE_NUMERIC standard type
 *
 *	usage:
 *		set_field_type(f, TYPE_NUMERIC, precision, vmin, vmax);
 *
 *		int precision;	digits to right of decimal point
 *		double vmin;	minimum acceptable value
 *		double vmax;	maximum acceptable value
 */
static char *make_num(va_list *);
static char *copy_num(char *);
static void free_num(char *);
static int fcheck_num(FIELD *, char *);
static int ccheck_num(int, char *);

typedef struct {

	int	prec;
	double	vmin;
	double	vmax;
} NUMERIC;

static FIELDTYPE typeNUMERIC =
{
				ARGS,			/* status	*/
				1,			/* ref		*/
				(FIELDTYPE *) 0,	/* left		*/
				(FIELDTYPE *) 0,	/* right	*/
				make_num,		/* makearg	*/
				copy_num,		/* copyarg	*/
				free_num,		/* freearg	*/
				fcheck_num,		/* fcheck	*/
				ccheck_num,		/* ccheck	*/
				(PTF_int) 0,		/* next		*/
				(PTF_int) 0,		/* prev		*/
};

FIELDTYPE * TYPE_NUMERIC = &typeNUMERIC;

static char *
make_num(va_list *ap)
{
	NUMERIC * n;

	if (Alloc(n, NUMERIC)) {
		n -> prec = va_arg(*ap, int);
		n -> vmin = va_arg(*ap, double);
		n -> vmax = va_arg(*ap, double);
	}
	return ((char *) n);
}

static char *
copy_num(char *arg)
{
	NUMERIC *n;

	if (Alloc(n, NUMERIC))
		*n = *((NUMERIC *) arg);
	return ((char *) n);
}

static void
free_num(char *arg)
{
	Free(arg);
}

static int
fcheck_num(FIELD *f, char *arg)
{
	NUMERIC *	n = (NUMERIC *) arg;
	double		vmin = n -> vmin;
	double		vmax = n -> vmax;
	int		prec = n -> prec;
	char *		x = field_buffer(f, 0);
	char		buf[80];

	while (*x && *x == ' ')
		++x;
	if (*x) {
		char * t = x;

		if (*x == '-')
			++x;
		while (*x && isdigit(*x))
			++x;
		if (*x == '.') {
			++x;
			while (*x && isdigit(*x))
				++x;
		}
		while (*x && *x == ' ')
			++x;
		if (! *x) {
			double v = atof(t);

			if (vmin >= vmax || (v >= vmin && v <= vmax)) {
				(void) sprintf(buf, "%.*f", prec, v);
				(void) set_field_buffer(f, 0, buf);
				return (TRUE);
			}
		}
	}
	return (FALSE);
}

#define	charok(c)	(isdigit(c) || c == '-' || c == '.')

/*ARGSUSED*/

static int
ccheck_num(int c, char *arg)
{
	return (charok(c));
}
