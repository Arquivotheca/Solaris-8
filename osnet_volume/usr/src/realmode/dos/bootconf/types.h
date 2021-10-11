/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * types.h -- common unix/c type definitions
 */

#ifndef	_TYPES_H
#define	_TYPES_H

#ident "@(#)types.h   1.7   97/11/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef char *caddr_t;

typedef unsigned char unchar;
typedef unsigned short ushort;

#ifndef max			/* Microsoft defines them, Sun doesn't!	    */
#define	max(x, y) (((x) > (y)) ? (x) : (y))
#define	min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _TYPES_H */
