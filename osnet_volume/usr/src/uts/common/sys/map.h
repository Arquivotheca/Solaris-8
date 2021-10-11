/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MAP_H
#define	_SYS_MAP_H

#pragma ident	"@(#)map.h	1.26	99/09/13 SMI"

#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct map;

#ifdef _KERNEL

extern	void	rmfree(void *, size_t, ulong_t);
extern	ulong_t	rmalloc(void *, size_t);
extern	ulong_t	rmalloc_wait(void *, size_t);
extern	ulong_t	rmalloc_locked(void *, size_t);
extern	void	*rmallocmap(size_t);
extern	void	*rmallocmap_wait(size_t);
extern	void	rmfreemap(void *);

#endif /* KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MAP_H */
