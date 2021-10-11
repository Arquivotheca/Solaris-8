/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KMEM_H
#define	_SYS_KMEM_H

#pragma ident	"@(#)kmem.h	1.29	99/04/14 SMI"

#include <sys/types.h>
#include <sys/vmem.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Kernel memory allocator: DDI interfaces.
 * See kmem_alloc(9F) for details.
 */

#define	KM_SLEEP	0x0000	/* can block for memory; success guaranteed */
#define	KM_NOSLEEP	0x0001	/* cannot block for memory; may fail */
#define	KM_PANIC	0x0002	/* if memory cannot be allocated, panic */
#define	KM_VMFLAGS	0x00ff	/* flags that must match VM_* flags */

#define	KM_FLAGS	0xffff	/* all settable kmem flags */

#ifdef _KERNEL

extern void *kmem_alloc(size_t size, int flags);
extern void *kmem_zalloc(size_t size, int flag);
extern void kmem_free(void *buf, size_t size);

#endif	/* _KERNEL */

/*
 * Kernel memory allocator: private interfaces.
 * These interfaces are still evolving.
 * Do not use them in unbundled drivers.
 */

/*
 * Flags for kmem_cache_create()
 */
#define	KMC_NOTOUCH	0x00010000
#define	KMC_NODEBUG	0x00020000
#define	KMC_NOMAGAZINE	0x00040000
#define	KMC_NOHASH	0x00080000
#define	KMC_QCACHE	0x00100000

struct kmem_cache;		/* cache structure is opaque to kmem clients */

typedef struct kmem_cache kmem_cache_t;

#ifdef _KERNEL

extern int kmem_ready;
extern pgcnt_t kmem_reapahead;

extern void kmem_init(void);
extern void kmem_thread_init(void);
extern void kmem_mp_init(void);
extern void kmem_reap(void);
extern pgcnt_t kmem_avail(void);
extern size_t kmem_maxavail(void);

extern kmem_cache_t *kmem_cache_create(char *, size_t, int,
	int (*)(void *, void *, int), void (*)(void *, void *),
	void (*)(void *), void *, vmem_t *, int);
extern void kmem_cache_destroy(kmem_cache_t *);
extern void *kmem_cache_alloc(kmem_cache_t *, int);
extern void kmem_cache_free(kmem_cache_t *, void *);
extern ulong_t kmem_cache_stat(kmem_cache_t *, char *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KMEM_H */
