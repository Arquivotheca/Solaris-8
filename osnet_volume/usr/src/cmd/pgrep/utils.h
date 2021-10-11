/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UTILS_H
#define	_UTILS_H

#pragma ident	"@(#)utils.h	1.2	98/04/19 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void warn(const char *, ...);
extern void die(const char *, ...);

extern const char *getpname(const char *);
extern char *mbstrip(char *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _UTILS_H */
