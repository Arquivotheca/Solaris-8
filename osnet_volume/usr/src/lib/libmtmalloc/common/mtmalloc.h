/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MTMALLOC_H
#define	_MTMALLOC_H

#pragma ident	"@(#)mtmalloc.h	1.1	98/04/02 SMI"

/*
 * Public interface for multi-threadead malloc user land library
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/* commands for mallocctl(int cmd, long value) */

#define	MTDOUBLEFREE	1	/* core dumps on double free */
#define	MTDEBUGPATTERN	2	/* write misaligned data after free. */
#define	MTINITBUFFER	4	/* write misaligned data at allocation */
#define	MTCHUNKSIZE	32	/* How much to alloc when backfilling caches. */

void mallocctl(int, long);

#ifdef __cplusplus
}
#endif

#endif /* _MTMALLOC_H */
