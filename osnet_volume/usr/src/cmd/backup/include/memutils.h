/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MEMUTILS_H
#define	_MEMUTILS_H

#pragma ident	"@(#)memutils.h	1.5	98/08/24 SMI"

#include <note.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	bcopy(s1, s2, len)	(void) memcpy((s2), (s1), (size_t)(len))
#define	bzero(s, len)		(void) memset((s), 0, (size_t)(len))
#define	bcmp(s1, s2, len)	memcmp((s1), (s2), (size_t)(len))

#if defined(__STDC__)
extern void *xmalloc(size_t);
extern void *xcalloc(size_t, size_t);
extern void *xrealloc(void *, size_t);
#else
extern void *xmalloc();
extern void *xcalloc();
extern void *xrealloc();
#endif /* __STDC__ */

NOTE(ALIGNMENT(xmalloc, 8))
NOTE(ALIGNMENT(xcalloc, 8))
NOTE(ALIGNMENT(xrealloc, 8))

#ifdef	__cplusplus
}
#endif

#endif /* _MEMUTILS_H */
