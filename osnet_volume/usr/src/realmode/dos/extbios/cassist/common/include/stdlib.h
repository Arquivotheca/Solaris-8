#ifndef	_STDLIB_H
#define	_STDLIB_H
#ident	"@(#)stdlib.h	1.3	95/04/20 SMI\n"

/*
 *  Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved
 *
 *  Standard ANSI library for Solaris x86 drivers:
 *
 *    This file provides function prototypes for a few ANSI-like functions
 *    available to Solaris x86 realmode drivers.  The functions provided,
 *    and the manner in which they differ from the strict ANSI definition,
 *    are:
 *
 *        rand   ...  Default seed is time of day (ANSI requires '1').
 *        ldiv   ...  Divisor (and deliverd remainder) is "int", not "long"
 *        qsort  ...  Comparison routine takes far ptrs (not constant).
 */

#include <dostypes.h>    /* Get "far" definition                             */

typedef unsigned size_t;
typedef struct { long quot, rem; } ldiv_t;

extern void qsort(void far *, size_t, size_t, int (*)(void far *, void far *));
extern ldiv_t ldiv(long, long);
extern void srand(unsigned);
extern int rand(void);

#define	abs(x) (((x) < 0) ? -(x) : (x))

extern void exit(int);   // Returns to driver loader; see "crt0.s"
#endif
