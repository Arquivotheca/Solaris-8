/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VMEM_IMPL_H
#define	_SYS_VMEM_IMPL_H

#pragma ident	"@(#)vmem_impl.h	1.1	99/04/14 SMI"

#include <sys/vmem.h>
#include <sys/kstat.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct vmem_seg vmem_seg_t;

#define	VMEM_STACK_DEPTH	20

struct vmem_seg {
	/*
	 * The first four fields must match vmem_freelist_t exactly.
	 */
	uintptr_t	vs_start;	/* start of segment (inclusive) */
	uintptr_t	vs_end;		/* end of segment (exclusive) */
	vmem_seg_t	*vs_knext;	/* next of kin (alloc, free, span) */
	vmem_seg_t	*vs_kprev;	/* prev of kin */

	vmem_seg_t	*vs_anext;	/* next in arena */
	vmem_seg_t	*vs_aprev;	/* prev in arena */
	uint8_t		vs_type;	/* alloc, free, span */
	uint8_t		vs_import;	/* non-zero if segment was imported */
	uint8_t		vs_depth;	/* stack depth if KMF_AUDIT active */
	/*
	 * The following fields are present only when KMF_AUDIT is set.
	 */
	kthread_t	*vs_thread;
	hrtime_t	vs_timestamp;
	uintptr_t	vs_stack[VMEM_STACK_DEPTH];
};

typedef struct vmem_freelist {
	uintptr_t	vs_start;	/* always zero */
	uintptr_t	vs_end;		/* segment size */
	vmem_seg_t	*vs_knext;	/* next of kin */
	vmem_seg_t	*vs_kprev;	/* prev of kin */
} vmem_freelist_t;

#define	VS_SIZE(vsp)	((vsp)->vs_end - (vsp)->vs_start)

/*
 * Segment hashing
 */
#define	VMEM_HASH_INDEX(a, s, q, m)					\
	((((a) + ((a) >> (s)) + ((a) >> ((s) << 1))) >> (q)) & (m))

#define	VMEM_HASH(vmp, addr)						\
	(&(vmp)->vm_hash_table[VMEM_HASH_INDEX(addr,			\
	(vmp)->vm_hash_shift, (vmp)->vm_qshift, (vmp)->vm_hash_mask)])

#define	VMEM_NAMELEN		32
#define	VMEM_HASH_INITIAL	64
#define	VMEM_NQCACHE_MAX	16
#define	VMEM_FREELISTS		(sizeof (void *) * 8)

typedef struct vmem_kstat {
	kstat_named_t	vk_mem_inuse;	/* memory in use */
	kstat_named_t	vk_mem_import;	/* memory imported */
	kstat_named_t	vk_mem_total;	/* total memory in arena */
	kstat_named_t	vk_source_id;	/* vmem id of vmem source */
	kstat_named_t	vk_alloc;	/* number of allocations */
	kstat_named_t	vk_free;	/* number of frees */
	kstat_named_t	vk_wait;	/* number of allocations that waited */
	kstat_named_t	vk_fail;	/* number of allocations that failed */
	kstat_named_t	vk_lookup;	/* hash lookup count */
	kstat_named_t	vk_search;	/* freelist search count */
	kstat_named_t	vk_populate_wait;	/* populates that waited */
	kstat_named_t	vk_populate_fail;	/* populates that failed */
	kstat_named_t	vk_contains;		/* vmem_contains() calls */
	kstat_named_t	vk_contains_search;	/* vmem_contains() search cnt */
} vmem_kstat_t;

struct vmem {
	char		vm_name[VMEM_NAMELEN];	/* arena name */
	kmutex_t	vm_lock;	/* arena lock */
	kcondvar_t	vm_cv;		/* cv for blocking allocations */
	uint32_t	vm_id;		/* vmem id */
	int		vm_qshift;	/* log2(vm_quantum) */
	size_t		vm_quantum;	/* vmem quantum */
	size_t		vm_qcache_max;	/* maximum size to front by kmem */
	void		*(*vm_source_alloc)(vmem_t *, size_t, int);
	void		(*vm_source_free)(vmem_t *, void *, size_t);
	vmem_t		*vm_source;	/* vmem source for imported memory */
	vmem_t		*vm_next;	/* next in vmem_list */
	kstat_t		*vm_ksp;	/* kstat */
	ssize_t		vm_nsegfree;	/* number of free vmem_seg_t's */
	vmem_seg_t	*vm_segfree;	/* free vmem_seg_t list */
	vmem_seg_t	**vm_hash_table; /* allocated-segment hash table */
	size_t		vm_hash_mask;	/* hash_size - 1 */
	size_t		vm_hash_shift;	/* log2(vm_hash_mask + 1) */
	ulong_t		vm_freemap;	/* bitmap of non-empty freelists */
	vmem_seg_t	vm_seg0;	/* anchor segment */
	vmem_seg_t	*vm_hash0[VMEM_HASH_INITIAL];	/* initial hash table */
	void		*vm_qcache[VMEM_NQCACHE_MAX];	/* quantum caches */
	vmem_freelist_t	vm_freelist[VMEM_FREELISTS + 1]; /* power-of-2 flists */
	vmem_kstat_t	vm_kstat;	/* kstat data */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VMEM_IMPL_H */
