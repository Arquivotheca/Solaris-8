/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UTILS_H
#define	_UTILS_H

#pragma ident	"@(#)utils.h	1.1	98/05/01 SMI"

#include <libintl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	E_SUCCESS	0		/* Exit status for success */
#define	E_ERROR		1		/* Exit status for error */
#define	E_USAGE		2		/* Exit status for usage error */

extern void warn(const char *, ...);
extern void die(const char *, ...);

extern const char *getpname(const char *);

extern int valid_abspath(const char *);
extern int valid_str2int(const char *, int *);
extern int valid_str2ull(const char *, unsigned long long *);

#ifdef	__cplusplus
}
#endif

#endif	/* _UTILS_H */
