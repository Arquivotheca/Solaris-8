/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KMEM_IMPL_H
#define	_SYS_KMEM_IMPL_H

#pragma ident	"@(#)kmem_impl.h	1.12	99/04/14 SMI"

#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/time.h>
#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * kernel memory allocator: implementation-private data structures
 */

#define	KMF_AUDIT	0x00000001	/* transaction auditing */
#define	KMF_DEADBEEF	0x00000002	/* deadbeef checking */
#define	KMF_REDZONE	0x00000004	/* redzone checking */
#define	KMF_CONTENTS	0x00000008	/* freed-buffer content logging */
#define	KMF_STICKY	0x00000010	/* if set, override /etc/system */
#define	KMF_NOMAGAZINE	0x00000020	/* disable per-cpu magazines */
#define	KMF_PAGEPERBUF	0x00000040	/* give each buf its own mapping */
#define	KMF_LITE	0x00000100	/* lightweight debugging */

#define	KMF_HASH	0x00000200	/* cache has hash table */
#define	KMF_RANDOMIZE	0x00000400	/* randomize other kmem_flags */

#define	KMF_BUFTAG	(KMF_AUDIT | KMF_DEADBEEF | KMF_REDZONE | KMF_CONTENTS)
#define	KMF_DBFLAGS	(KMF_BUFTAG | KMF_NOMAGAZINE | KMF_LITE)

#define	KMEM_STACK_DEPTH	14

#define	KMEM_FREE_PATTERN		0xdeadbeef
#define	KMEM_FREE_PATTERN_64		0xdeadbeefdeadbeefULL
#define	KMEM_UNINITIALIZED_PATTERN	0xbaddcafe
#define	KMEM_REDZONE_PATTERN_64		0xfeedfacefeedfaceULL
#define	KMEM_REDZONE_BYTE		0xbb

/*
 * The bufctl (buffer control) structure keeps some minimal information
 * about each buffer: its address, its slab, and its current linkage,
 * which is either on the slab's freelist (if the buffer is free), or
 * on the cache's buf-to-bufctl hash table (if the buffer is allocated).
 * In the case of non-hashed, or "raw", caches (the common case), only
 * the freelist linkage is necessary: the buffer address is at a fixed
 * offset from the bufctl address, and the slab is at the end of the page.
 *
 * NOTE: bc_next must be the first field; raw buffers have linkage only.
 */
typedef struct kmem_bufctl {
	struct kmem_bufctl	*bc_next;	/* next bufctl struct */
	void			*bc_addr;	/* address of buffer */
	struct kmem_slab	*bc_slab;	/* controlling slab */
	struct kmem_cache	*bc_cache;	/* controlling cache */
} kmem_bufctl_t;

/*
 * The KMF_AUDIT version of the bufctl structure.  The beginning of this
 * structure must be identical to the normal bufctl structure, so that
 * pointers are interchangeable.
 */
typedef struct kmem_bufctl_audit {
	struct kmem_bufctl	*bc_next;	/* next bufctl struct */
	void			*bc_addr;	/* address of buffer */
	struct kmem_slab	*bc_slab;	/* controlling slab */
	struct kmem_cache	*bc_cache;	/* controlling cache */
	hrtime_t		bc_timestamp;	/* transaction time */
	kthread_id_t		bc_thread;	/* thread doing transaction */
	struct kmem_bufctl_audit *bc_lastlog;	/* last log entry */
	char			*bc_contents;	/* contents at last free */
	int			bc_depth;	/* stack depth */
	uintptr_t		bc_stack[KMEM_STACK_DEPTH];	/* pc stack */
} kmem_bufctl_audit_t;

/*
 * A buftag structure is appended to each buffer whenever any of the
 * KMF_AUDIT, KMF_DEADBEEF, KMF_REDZONE, or KMF_CONTENTS flags are set.
 * The structure contents depend on whether KMF_LITE is set.
 */
typedef struct kmem_caller_info {
	uint32_t	_bt_lastalloc;
	uint32_t	_bt_lastfree;
} kmem_caller_info_t;

typedef struct kmem_bufctl_info {
	kmem_bufctl_t	*_bt_bufctl;
	intptr_t	_bt_bxstat;	/* bufctl ^ status (alloc or free) */
} kmem_bufctl_info_t;

typedef struct kmem_buftag {
	uint64_t	bt_redzone;
	union {
		kmem_bufctl_info_t	_bt_bufctl_info;
		kmem_caller_info_t	_bt_caller_info;
	} _bt_info_un;
} kmem_buftag_t;

#define	bt_lastalloc	_bt_info_un._bt_caller_info._bt_lastalloc
#define	bt_lastfree	_bt_info_un._bt_caller_info._bt_lastfree
#define	bt_bufctl	_bt_info_un._bt_bufctl_info._bt_bufctl
#define	bt_bxstat	_bt_info_un._bt_bufctl_info._bt_bxstat

#define	KMEM_BUFTAG(cp, buf)	\
	((kmem_buftag_t *)((char *)(buf) + (cp)->cache_offset))

#define	KMEM_BUFTAG_ALLOC	0xa110c8edUL
#define	KMEM_BUFTAG_FREE	0xf4eef4eeUL

typedef struct kmem_slab {
	struct kmem_cache	*slab_cache;	/* controlling cache */
	void			*slab_base;	/* base of allocated memory */
	struct kmem_slab	*slab_next;	/* next slab on freelist */
	struct kmem_slab	*slab_prev;	/* prev slab on freelist */
	struct kmem_bufctl	*slab_head;	/* first free buffer */
	struct kmem_bufctl	*slab_tail;	/* last free buffer */
	int			slab_refcnt;	/* outstanding allocations */
	int			slab_chunks;	/* chunks (bufs) in this slab */
} kmem_slab_t;

#define	KMEM_HASH_INITIAL	64

#define	KMEM_HASH(cp, buf)	\
	((cp)->cache_hash_table +	\
	(((uintptr_t)(buf) >> (cp)->cache_hash_shift) & (cp)->cache_hash_mask))

typedef struct kmem_magazine {
	void	*mag_next;
	void	*mag_round[1];		/* one or more rounds */
} kmem_magazine_t;

#define	KMEM_CPU_CACHE_SIZE	64	/* must be power of 2 */
#define	KMEM_CPU_PAD		(KMEM_CPU_CACHE_SIZE - sizeof (kmutex_t) - \
	4 * sizeof (int) - 3 * sizeof (void *))
#define	KMEM_CACHE_SIZE(ncpus)	\
	((size_t)(&((kmem_cache_t *)0)->cache_cpu[ncpus]))

typedef struct kmem_cpu_cache {
	kmutex_t	cc_lock;	/* protects this cpu's local cache */
	int		cc_alloc;	/* allocations from this cpu */
	int		cc_free;	/* frees to this cpu */
	int		cc_rounds;	/* number of buffers in magazine */
	int		cc_magsize;	/* max rounds per magazine */
	kmem_magazine_t	*cc_loaded_mag;	/* the currently loaded magazine */
	kmem_magazine_t	*cc_full_mag;	/* a spare full magazine */
	kmem_magazine_t	*cc_empty_mag;	/* a spare empty magazine */
	char		cc_pad[KMEM_CPU_PAD]; /* for nice alignment */
} kmem_cpu_cache_t;

#define	KMEM_CACHE_NAMELEN	31

struct kmem_cache {
	kmutex_t	cache_lock;	/* protects this cache only */
	int		cache_flags;	/* various cache state info */
	kmem_slab_t	*cache_freelist;	/* slab free list */
	int		cache_offset;	/* for buf-to-bufctl conversion */
	uint32_t	cache_global_alloc;	/* total global allocations */
	uint32_t	cache_global_free;	/* total global frees */
	uint32_t	cache_alloc_fail;	/* total failed allocations */
	int		cache_hash_shift;	/* get to interesting bits */
	int		cache_hash_mask;	/* hash table mask */
	kmem_bufctl_t	**cache_hash_table;	/* hash table base */
	kmem_slab_t	cache_nullslab;		/* end of freelist marker */
	int		(*cache_constructor)(void *, void *, int);
	void		(*cache_destructor)(void *, void *);
	void		(*cache_reclaim)(void *);
	void		*cache_private;	/* arg for constr/destr/reclaim */
	vmem_t		*cache_arena;	/* vmem arena for slab creates */
	int		cache_cflags;	/* cache creation flags */
	void		*cache_pad1;	/* unused */
	void		*cache_pad2;	/* unused */
	int		cache_bufsize;	/* size of buffers in this cache */
	int		cache_align;	/* minimum guaranteed alignment */
	int		cache_chunksize;	/* buf + alignment (+ debug) */
	int		cache_slabsize;	/* size of a slab */
	int		cache_color;	/* color to assign to next new slab */
	int		cache_maxcolor;	/* maximum slab color */
	int		cache_slab_create;	/* total slab creates */
	int		cache_slab_destroy;	/* total slab destroys */
	int		cache_buftotal;	/* total buffers (alloc + avail) */
	int		cache_bufmax;	/* max buffers ever in this cache */
	int		cache_rescale;	/* # of hash table rescales */
	int		cache_lookup_depth;	/* hash lookup depth */
	kstat_t		*cache_kstat;	/* statistics */
	kmem_cache_t	*cache_next;	/* forward cache linkage */
	kmem_cache_t	*cache_prev;	/* backward cache linkage */
	char		cache_name[KMEM_CACHE_NAMELEN + 1];
	kmem_cache_t	*cache_bufctl_cache;	/* source of bufctls */
	kmem_cache_t	*cache_magazine_cache;	/* magazine cache */
	int		cache_magazine_size;	/* rounds per magazine */
	int		cache_magazine_maxsize;	/* max rounds per magazine */
	kmutex_t	cache_depot_lock;	/* protects cache depot only */
	int		cache_pad3;	/* unused */
	int		cache_ncpus;	/* number of cpus in array below */
	int		cache_depot_contention;	/* failed mutex_tryenters */
	int		cache_depot_contention_last; /* value at last update */
	int		cache_depot_alloc;	/* allocs from the depot */
	int		cache_depot_free;	/* frees to the depot */
	kmem_magazine_t	*cache_fmag_list;	/* full magazines */
	int		cache_fmag_total;	/* number of full magazines */
	int		cache_fmag_min;		/* min since last update */
	int		cache_fmag_reaplimit;	/* max reapable magazines */
	kmem_magazine_t	*cache_emag_list;	/* empty magazines */
	int		cache_emag_total;	/* number of empty magazines */
	int		cache_emag_min;		/* min since last update */
	int		cache_emag_reaplimit;	/* max reapable magazines */
	kmem_bufctl_t	*cache_hash0[KMEM_HASH_INITIAL];
	kmem_cpu_cache_t cache_cpu[1];	/* cache_ncpus actually allocated */
};

typedef struct kmem_cpu_log_header {
	kmutex_t	clh_lock;
	char		*clh_current;
	size_t		clh_avail;
	int		clh_chunk;
	int		clh_hits;
	char		clh_pad[64 - sizeof (kmutex_t) - sizeof (char *) -
				sizeof (size_t) - 2 * sizeof (int)];
} kmem_cpu_log_header_t;

typedef struct kmem_log_header {
	kmutex_t		lh_lock;
	char			*lh_base;
	int			*lh_free;
	size_t			lh_chunksize;
	int			lh_nchunks;
	int			lh_head;
	int			lh_tail;
	int			lh_hits;
	kmem_cpu_log_header_t	lh_cpu[1];	/* ncpus actually allocated */
} kmem_log_header_t;

#define	KMEM_ALIGN		8	/* min guaranteed alignment */
#define	KMEM_ALIGN_SHIFT	3	/* log2(KMEM_ALIGN) */
#define	KMEM_VOID_FRACTION	8	/* never waste more than 1/8 of slab */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KMEM_IMPL_H */
