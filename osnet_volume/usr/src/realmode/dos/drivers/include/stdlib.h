/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)stdlib.h	1.1	97/03/14 SMI"
 */

#ifndef	_STDLIB_H
#define	_STDLIB_H

/*
 *  Standard ANSI library for Solaris x86 drivers:
 *
 *    This file provides function prototypes for a few ANSI-like functions
 *    available to Solaris x86 realmode drivers.  The functions provided,
 *    and the manner in which they differ from the strict ANSI definition,
 *    are:
 *
 *	   ldiv	 ...  Divisor (and delivered remainder) is "int", not "long"
 */

#include <dostypes.h>	/* Get "far" definition */

typedef unsigned size_t;
typedef struct { long quot, rem; } ldiv_t;

extern ldiv_t ldiv(long, long);

#define	abs(x) (((x) < 0) ? -(x) : (x))

extern void exit(int);   // Returns to driver loader; see "crt0.s"
#endif
