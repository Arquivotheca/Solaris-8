/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MTMALLOC_IMPL_H
#define	_MTMALLOC_IMPL_H

#pragma ident	"@(#)mtmalloc_impl.h	1.2	98/04/10 SMI"

/*
 * Various data structures that define the guts of the mt malloc
 * library.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM

#include <sys/types.h>
#include <synch.h>

typedef struct {
	uintptr_t	owner;
	uchar_t		lock;
} _mtmutex_t;

#else /* _ASM */

/*
 * Offsets for lock structure
 */
#define	OWNER	0
#define	LOCK	4

#endif

#ifndef _ASM
typedef struct cache {
	mutex_t mt_cache_lock;	/* lock for this data structure */
	caddr_t mt_freelist;	/* free block bit mask */
	caddr_t mt_arena;	/* addr of arena for actual dblks */
	size_t  mt_nfree;	/* how many freeblocks do we have */
	size_t mt_size;		/* size of this cache */
	size_t mt_span;		/* how long is this cache */
	struct cache *mt_next;	/* next cache in list */
	int mt_hunks;		/* at creation time what chunk size */
} cache_t;

typedef struct oversize {
	struct oversize *mt_next;
	struct oversize *mt_prev;
	caddr_t mt_addr;
	size_t mt_size;
} oversize_t;

typedef struct percpu {
	mutex_t mt_parent_lock;	/* used for hooking in new caches */
	cache_t ** mt_caches;
} percpu_t;

extern void _mtlock(_mtmutex_t *);
extern void _mtunlock(_mtmutex_t *);

#endif /* _ASM */

#ifdef __cplusplus
}
#endif

#endif /* _MTMALLOC_IMPL_H */
