/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_STDLIB_H
#define	_MDB_STDLIB_H

#pragma ident	"@(#)mdb_stdlib.h	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern const char *longdoubletos(long double *, int, char);
extern const char *doubletos(double, int, char);
extern const char *timetos(const time_t *);
extern void delay(int);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_STDLIB_H */
