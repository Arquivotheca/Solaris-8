/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef	_MONETARY_H
#define	_MONETARY_H

#pragma ident	"@(#)monetary.h	1.1	94/01/27 SMI"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern ssize_t	strfmon(char *, size_t, const char *, ...);
#else
extern ssize_t	strfmon();
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _MONETARY_H */
