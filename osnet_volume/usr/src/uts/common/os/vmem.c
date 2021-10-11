/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vmem.c	1.10	99/12/06 SMI"

/*
 * Big Theory Statement for the virtual memory allocator.
 *
 * 1. General Concepts
 * -------------------
 *
 * 1.1 Overview
 * ------------
 * We divide the kernel address space into a number of logically distinct
 * pieces, or *arenas*: text, data, heap, stack, and so on.  Within these
 * arenas we often subdivide further; for example, we use heap addresses
 * not only for the kernel heap (kmem_alloc() space), but also for DVMA,
 * bp_mapin(), /dev/kmem, and even some device mappings like the TOD chip.
 * The kernel address space, therefore, is most accurately described as
 * a tree of arenas in which each node of the tree *imports* some subset
 * of its parent.  The virtual memory allocator manages these arenas and
 * supports their natural hierarchical structure.
 *
 * 1.2 Arenas
 * ----------
 * An arena is nothing more than a set of integers.  These integers most
 * commonly represent virtual addresses, but in fact they can represent
 * anything at all.  For example, we could use an arena containing the
 * integers minpid through maxpid to allocate process IDs.  vmem_create()
 * and vmem_destroy() create and destroy vmem arenas (data type vmem_t).
 *
 * 1.3 Spans
 * ---------
 * We represent the integers in an arena as a collection of *spans*, or
 * contiguous ranges of integers.  For example, the kernel heap consists
 * of just one span: [kernelheap, ekernelheap).  [Note: the mathematical
 * convention for describing numeric intervals is to use brackets when
 * the end of the interval is included, parentheses when it is excluded.
 * Therefore, the integers 4, 5, 6, 7, 8 could be represented in any of
 * the following four ways: [4, 8], [4, 9), (3, 8], (3, 9).  For our
 * purposes, the notation [4, 9) is the most natural because we can
 * readily see both the starting address (4) and the size (9 - 4).]
 *
 * Spans can be added to an arena in two ways: explicitly, by vmem_add(),
 * or implicitly, by import (described below).
 *
 * 1.4 Imported memory (aka vmem sourcing)
 * ---------------------------------------
 * As mentioned in the overview, some arenas are logical subsets of
 * other arenas.  For example, kmem_va_arena (a virtual address cache
 * that satisfies most kmem_slab_create() requests) is just a subset
 * of heap_arena (the kernel heap) that provides caching for the most
 * common slab sizes.  When kmem_va_arena runs out of virtual memory,
 * it *imports* more from the heap; we say that heap_arena is the
 * *vmem source* for kmem_va_arena.  vmem_create() allows you to
 * specify any existing vmem arena as the source for your new arena.
 * Topologically, since every arena is a child of at most one source,
 * the set of all arenas forms a collection of trees.
 *
 * 1.5 Constrained allocations
 * ---------------------------
 * Some vmem clients are quite picky about the kind of address they want.
 * For example, the DVMA code may need an address that is at a particular
 * phase with respect to some alignment (to get good cache coloring), or
 * that lies within certain limits (the addressable range of a device),
 * or that doesn't cross some boundary (a DMA counter restriction) --
 * or all of the above.  The vmem_xalloc() interface, described below,
 * allows the client to specify any or all of these constraints.
 *
 * 1.6 The vmem quantum
 * --------------------
 * Every arena has a notion of 'quantum', specified at vmem_create() time,
 * which defines the minimum unit of memory the arena deals with.
 * Most commonly the quantum is either 1 or PAGESIZE, but any power of 2
 * is legal.  All vmem allocations are guaranteed to be quantum-aligned.
 *
 * 1.7 Quantum caching
 * -------------------
 * A vmem arena may be so 'hot' (frequently used) that the scalability
 * of vmem allocation is a significant concern.  We address this by
 * allowing the most common allocation sizes to be 'fronted' by the
 * kernel memory allocator, which provides low-latency per-cpu caching.
 * To get kmem caching for all allocations up to size S, simply pass S
 * as the qcache_max parameter to vmem_create().  The vmem allocator will
 * create kmem caches of size 1 * quantum, 2 * quantum, ... up to S,
 * so that any vmem_alloc() of size <= S will be handled by a kmem cache.
 *
 * At present, the implementation limits S to (VMEM_NQCACHE_MAX * quantum).
 *
 * 1.8 Fragmentation considerations
 * --------------------------------
 * Any memory allocator is vulnerable to fragmentation if presented with
 * a sufficiently hostile workload.  One way to prevent fragmentation is
 * to make all allocation requests the same size.  This isn't practical
 * in general, but if you use quantum caching, it happens as a side-effect
 * because all quantum caches for a given arena have the same slab size --
 * specifically, it will be the next power of 2 at or above 3 * qcache_max.
 * For example, if you cache up to 5 * quantum, then all slabs will be of
 * size 16 * quantum (5 * 3 = 15, round to 16).  The kernel memory allocator
 * will divide these slabs up differently for the different cache sizes, but
 * the load presented to vmem_xalloc() will always be the same: 16 * quantum.
 * Quantum caching has proven very effective against fragmentation of segkp,
 * which typically issues 2-page through 5-page requests (for thread stacks).
 *
 * 1.9 Relationship to kernel memory allocator
 * -------------------------------------------
 * Every kmem cache has a vmem arena as its memory source.  The kernel
 * memory allocator uses vmem_alloc() and vmem_free() to grow and shrink
 * its caches.  Thus vmem arenas replace the old notion of kmem backends.
 *
 *
 * 2. Implementation
 * -----------------
 *
 * 2.1 Segments
 * ------------
 * An arena's spans define the total address range under management.
 * Individual spans are subdivided into *segments*, each of which is
 * either allocated or free.  A segment, like a span, is a contiguous
 * range of integers.  Each allocated segment [addr, addr + size)
 * represents exactly one vmem_alloc(size) that returned addr.
 * Free segments represent the space between allocated segments.
 * If two free segments are adjacent, we coalesce them into one
 * larger segment; that is, if segments [a, b) and [b, c) are both
 * free, we merge them into a single segment [a, c).  The segments
 * within a span are linked together in increasing-address order so
 * we can easily determine whether coalescing is possible.
 *
 * Segments never cross span boundaries.  When all segments within
 * an imported span become free, we return the span to its source.
 *
 * 2.2 Segment lists and markers
 * -----------------------------
 * The segment structure (vmem_seg_t) contains two doubly-linked lists.
 *
 * The arena list (vs_anext/vs_aprev) links all segments in the arena.
 * In addition to the allocated and free segments, the arena contains
 * special marker segments at span boundaries.  Span markers simplify
 * coalescing and importing logic by making it easy to tell both when
 * we're at a span boundary (so we don't coalesce across it), and when
 * a span is completely free (its neighbors will both be span markers).
 *
 * The next-of-kin list (vs_knext/vs_kprev) links segments of the same type:
 * (1) for allocated segments, vs_knext is the hash chain linkage;
 * (2) for free segments, vs_knext is the freelist linkage;
 * (3) for span marker segments, vs_knext is the next span marker.
 *
 * 2.3 Allocation hashing
 * ----------------------
 * We maintain a hash table of all allocated segments, hashed by address.
 * This allows vmem_free() to discover the target segment in constant time.
 * vmem_update() periodically resizes hash tables to keep hash chains short.
 *
 * 2.4 Freelist management
 * -----------------------
 * We maintain power-of-2 freelists for free segments, i.e. free segments
 * of size >= 2^n reside in vmp->vm_freelist[n].  To ensure constant-time
 * allocation, vmem_xalloc() looks not in the first freelist that *might*
 * satisfy the allocation, but in the first freelist that *definitely*
 * satisfies the allocation (unless VM_BESTFIT is specified, or all larger
 * freelists are empty).  For example, a 1000-byte allocation will be
 * satisfied not from the 512..1023-byte freelist, whose members *might*
 * contains a 1000-byte segment, but from a 1024-byte or larger freelist,
 * the first member of which will *definitely* satisfy the allocation.
 * This ensures that vmem_xalloc() works in constant time.
 *
 * We maintain a bit map to determine quickly which freelists are non-empty.
 * vmp->vm_freemap & (1 << n) is non-zero iff vmp->vm_freelist[n] is non-empty.
 *
 * The different freelists are linked together into one large freelist,
 * with the freelist heads serving as markers.  Freelist markers simplify
 * the maintenance of vm_freemap by making it easy to tell when we're taking
 * the last member of a freelist (both of its neighbors will be markers).
 *
 * 2.5 vmem locking
 * ----------------
 * For simplicity, all arena state is protected by a per-arena lock.
 * If you need scalability, use quantum caching.
 *
 * 2.6 vmem population
 * -------------------
 * Any internal vmem routine that might need to allocate new segment
 * structures must prepare in advance by calling vmem_populate(nsegneeded).
 * The idea is to preallocate enough vmem_seg_t's that we don't have to
 * drop the lock protecting the arena, which makes the code much simpler.
 *
 * 2.7 Auditing
 * ------------
 * If KMF_AUDIT is set in kmem_flags, we audit vmem allocations as well.
 * Since virtual addresses cannot be scribbled on, there is no equivalent
 * in vmem to redzone checking, deadbeef, or other kmem debugging features.
 * Moreover, we do not audit frees because segment coalescing destroys the
 * association between an address and its segment structure.  Auditing is
 * thus intended primarily to keep track of who's consuming the arena.
 * Debugging support could certainly be extended in the future if it proves
 * necessary, but we do so much live checking via the allocation hash table
 * that even non-DEBUG systems get quite a bit of sanity checking already.
 *
 *
 * 3. vmem interfaces
 * ------------------
 *
 * vmp = vmem_create(name, base, size, quantum, afunc, ffunc, source,
 *	qcache_max, flag);
 *
 *	Create a vmem arena with the requested parameters:
 *
 *	name:		arena's name
 *	base, size:	initial span [base, base + size)
 *	quantum:	minimum alignment for all transactions on the arena
 *	afunc:		allocation routine to import memory
 *	ffunc:		free routine to return imported memory
 *	source:		source of imported memory
 *	qcache_max:	maximum allocation size to be 'fronted' by kmem caches
 *	flag:		VM_SLEEP or VM_NOSLEEP
 *
 * vmem_destroy(vmp);
 *
 *	Destroy arena vmp.
 *
 * addr = vmem_alloc(vmp, size, flag);
 *
 *	Allocate size bytes.
 *
 * addr = vmem_xalloc(vmp, size, align, phase, nocross, minaddr, maxaddr, flag);
 *
 *	Allocate size bytes at offset phase from an align boundary such that
 *	[addr, addr + size) is a subset of [minaddr, maxaddr) that does not
 *	straddle a nocross-aligned boundary.
 *
 * vmem_free(vmp, vaddr, size);
 *
 *	Free size bytes at vaddr.
 *
 * vmem_xfree(vmp, vaddr, size);
 *
 *	Free size bytes at vaddr, where vaddr is a constrained allocation.
 *
 * void *vmem_add(vmp, vaddr, size, flag);
 *
 *	Add the span [vaddr, vaddr + size) to arena vmp.
 *	Returns vaddr on success, NULL on failure (VM_NOSLEEP case only).
 *
 * int vmem_contains(vmp, vaddr, size);
 *
 *	Determine whether vmp contains the range [vaddr, vaddr + size).
 *
 * void vmem_walk(vmem_t *vmp, int typemask,
 *	void (*func)(void *, void *, size_t), void *arg);
 *
 *	Walk the vmp arena, applying func to each segment matching typemask.
 *	Note that func is called with vmp->vm_lock held.  This guarantees a
 *	consistent snapshot, at the price of requiring that func not block.
 *
 * size_t vmem_size(vmem_t *vmp, int typemask);
 *
 *	Return the total amount of memory whose type matches typemask.  Thus:
 *	- typemask VMEM_ALLOC yields total memory allocated (in use).
 *	- typemask VMEM_FREE yields total memory free (available).
 *	- typemask (VMEM_ALLOC | VMEM_FREE) yields total arena size.
 *
 *
 * 4. rmalloc(9F) compatibility
 * ----------------------------
 *
 * rmalloc(9F) and friends still exist, but are now just simple wrappers
 * around more general vmem services.
 */

#include <sys/vmem_impl.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/bitmap.h>
#include <sys/sysmacros.h>
#include <sys/disp.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mman.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

#define	VMEM_INITIAL		15	/* number of initial vmem pools */
#define	VMEM_SEG_INITIAL	100	/* number of initial segments */

static vmem_t vmem0[VMEM_INITIAL];
static vmem_seg_t vmem_seg0[VMEM_SEG_INITIAL];
static uint32_t vmem_id;
static kmutex_t vmem_list_lock;
static vmem_t *vmem_list;
static vmem_t *vmem_seg_arena;
static vmem_t *vmem_vmem_arena;
static size_t vmem_seg_size = sizeof (vmem_seg_t);
static long vmem_update_interval = 15;	/* vmem_update() every 15 seconds */

#ifdef DEBUG
int vmem_qcache_cflags = KMC_NOTOUCH | KMC_QCACHE | KMC_NOMAGAZINE;
#else
int vmem_qcache_cflags = KMC_NOTOUCH | KMC_QCACHE;
#endif

static vmem_kstat_t vmem_kstat_template = {
	{ "mem_inuse",		KSTAT_DATA_UINT64 },
	{ "mem_import",		KSTAT_DATA_UINT64 },
	{ "mem_total",		KSTAT_DATA_UINT64 },
	{ "vmem_source",	KSTAT_DATA_UINT32 },
	{ "alloc",		KSTAT_DATA_UINT64 },
	{ "free",		KSTAT_DATA_UINT64 },
	{ "wait",		KSTAT_DATA_UINT64 },
	{ "fail",		KSTAT_DATA_UINT64 },
	{ "lookup",		KSTAT_DATA_UINT64 },
	{ "search",		KSTAT_DATA_UINT64 },
	{ "populate_wait",	KSTAT_DATA_UINT64 },
	{ "populate_fail",	KSTAT_DATA_UINT64 },
	{ "contains",		KSTAT_DATA_UINT64 },
	{ "contains_search",	KSTAT_DATA_UINT64 },
};

/*
 * Insert/delete from arena list (type 'a') or next-of-kin list (type 'k').
 */
#define	VMEM_INSERT(vprev, vsp, type)					\
{									\
	vmem_seg_t *vnext = (vprev)->vs_##type##next;			\
	(vsp)->vs_##type##next = (vnext);				\
	(vsp)->vs_##type##prev = (vprev);				\
	(vprev)->vs_##type##next = (vsp);				\
	(vnext)->vs_##type##prev = (vsp);				\
}

#define	VMEM_DELETE(vsp, type)						\
{									\
	vmem_seg_t *vprev = (vsp)->vs_##type##prev;			\
	vmem_seg_t *vnext = (vsp)->vs_##type##next;			\
	(vprev)->vs_##type##next = (vnext);				\
	(vnext)->vs_##type##prev = (vprev);				\
}

#define	VMEM_AUDIT(vsp)							\
{									\
	if (vmem_seg_size == sizeof (vmem_seg_t)) {			\
		vsp->vs_depth = (uint8_t)getpcstack(vsp->vs_stack,	\
		    VMEM_STACK_DEPTH);					\
		vsp->vs_thread = curthread;				\
		vsp->vs_timestamp = gethrtime();			\
	} else {							\
		vsp->vs_depth = 0;					\
	}								\
}

/*
 * Get a vmem_seg_t structure from vmp's segfree list.
 */
static vmem_seg_t *
vmem_getseg(vmem_t *vmp)
{
	vmem_seg_t *newseg;

	ASSERT(vmp->vm_nsegfree > 0);

	newseg = vmp->vm_segfree;
	vmp->vm_segfree = newseg->vs_knext;
	vmp->vm_nsegfree--;

	return (newseg);
}

/*
 * Put a vmem_seg_t structure on vmp's segfree list.
 */
static void
vmem_putseg(vmem_t *vmp, vmem_seg_t *vsp)
{
	vsp->vs_knext = vmp->vm_segfree;
	vmp->vm_segfree = vsp;
	vmp->vm_nsegfree++;
}

/*
 * Add vsp to the appropriate freelist.
 */
static void
vmem_freelist_insert(vmem_t *vmp, vmem_seg_t *vsp)
{
	vmem_seg_t *vprev;

	ASSERT(*VMEM_HASH(vmp, vsp->vs_start) != vsp);

	vprev = (vmem_seg_t *)&vmp->vm_freelist[highbit(VS_SIZE(vsp)) - 1];
	vsp->vs_type = VMEM_FREE;
	vmp->vm_freemap |= VS_SIZE(vprev);
	VMEM_INSERT(vprev, vsp, k);

	cv_broadcast(&vmp->vm_cv);
}

/*
 * Take vsp from the freelist.
 */
static void
vmem_freelist_delete(vmem_t *vmp, vmem_seg_t *vsp)
{
	ASSERT(*VMEM_HASH(vmp, vsp->vs_start) != vsp);
	ASSERT(vsp->vs_type == VMEM_FREE);

	if (vsp->vs_knext->vs_start == 0 && vsp->vs_kprev->vs_start == 0) {
		/*
		 * The segments on both sides of 'vsp' are freelist heads,
		 * so taking vsp leaves the freelist at vsp->vs_kprev empty.
		 */
		ASSERT(vmp->vm_freemap & VS_SIZE(vsp->vs_kprev));
		vmp->vm_freemap ^= VS_SIZE(vsp->vs_kprev);
	}
	VMEM_DELETE(vsp, k);
}

/*
 * Add vsp to the allocated-segment hash table and update kstats.
 */
static void
vmem_hash_insert(vmem_t *vmp, vmem_seg_t *vsp)
{
	vmem_seg_t **bucket;

	vsp->vs_type = VMEM_ALLOC;
	bucket = VMEM_HASH(vmp, vsp->vs_start);
	vsp->vs_knext = *bucket;
	*bucket = vsp;

	VMEM_AUDIT(vsp);

	vmp->vm_kstat.vk_alloc.value.ui64++;
	vmp->vm_kstat.vk_mem_inuse.value.ui64 += VS_SIZE(vsp);
}

/*
 * Remove vsp from the allocated-segment hash table and update kstats.
 */
static vmem_seg_t *
vmem_hash_delete(vmem_t *vmp, uintptr_t addr, size_t size)
{
	vmem_seg_t *vsp, **prev_vspp;

	prev_vspp = VMEM_HASH(vmp, addr);
	while ((vsp = *prev_vspp) != NULL) {
		if (vsp->vs_start == addr) {
			*prev_vspp = vsp->vs_knext;
			break;
		}
		vmp->vm_kstat.vk_lookup.value.ui64++;
		prev_vspp = &vsp->vs_knext;
	}

	if (vsp == NULL)
		panic("vmem_hash_delete(%p, %lx, %lu): bad free",
		    vmp, addr, size);
	if (VS_SIZE(vsp) != size)
		panic("vmem_hash_delete(%p, %lx, %lu): wrong size (expect %lu)",
		    vmp, addr, size, VS_SIZE(vsp));

	vmp->vm_kstat.vk_free.value.ui64++;
	vmp->vm_kstat.vk_mem_inuse.value.ui64 -= size;

	return (vsp);
}

/*
 * Create a segment spanning the range [start, end) and add it to the arena.
 */
static vmem_seg_t *
vmem_seg_create(vmem_t *vmp, vmem_seg_t *vprev, uintptr_t start, uintptr_t end,
	uint8_t segtype)
{
	vmem_seg_t *newseg = vmem_getseg(vmp);

	newseg->vs_start = start;
	newseg->vs_end = end;
	newseg->vs_type = segtype;
	newseg->vs_import = 0;

	VMEM_INSERT(vprev, newseg, a);

	return (newseg);
}

/*
 * Remove segment vsp from the arena.
 */
static void
vmem_seg_destroy(vmem_t *vmp, vmem_seg_t *vsp)
{
	VMEM_DELETE(vsp, a);

	vmem_putseg(vmp, vsp);
}

/*
 * Add the span [vaddr, vaddr + size) to vmp and update kstats.
 */
static vmem_seg_t *
vmem_span_create(vmem_t *vmp, void *vaddr, size_t size, uint8_t import)
{
	vmem_seg_t *newseg, *span;
	uintptr_t start = (uintptr_t)vaddr;
	uintptr_t end = start + size;

	ASSERT(MUTEX_HELD(&vmp->vm_lock));

	if ((start | end) & (vmp->vm_quantum - 1))
		panic("vmem_span_create(%p, %p, %lu): misaligned",
		    vmp, vaddr, size);

	span = vmem_seg_create(vmp, vmp->vm_seg0.vs_aprev, start, end,
	    VMEM_SPAN);
	newseg = vmem_seg_create(vmp, span, start, end, VMEM_FREE);

	VMEM_INSERT(vmp->vm_seg0.vs_kprev, span, k);
	vmem_freelist_insert(vmp, newseg);

	VMEM_AUDIT(span);

	newseg->vs_import = import;
	if (import)
		vmp->vm_kstat.vk_mem_import.value.ui64 += size;
	vmp->vm_kstat.vk_mem_total.value.ui64 += size;

	return (newseg);
}

/*
 * Remove span vsp from vmp and update kstats.
 */
static void
vmem_span_destroy(vmem_t *vmp, vmem_seg_t *vsp)
{
	vmem_seg_t *span = vsp->vs_aprev;
	size_t size = VS_SIZE(vsp);

	ASSERT(MUTEX_HELD(&vmp->vm_lock));
	ASSERT(span->vs_type == VMEM_SPAN);

	if (vsp->vs_import)
		vmp->vm_kstat.vk_mem_import.value.ui64 -= size;
	vmp->vm_kstat.vk_mem_total.value.ui64 -= size;

	VMEM_DELETE(span, k);

	vmem_seg_destroy(vmp, vsp);
	vmem_seg_destroy(vmp, span);
}

/*
 * Allocate the subrange [start, end) from segment vsp.
 * If there are leftovers on either side, place them on the freelist.
 */
static void
vmem_seg_alloc(vmem_t *vmp, vmem_seg_t *vsp, uintptr_t start, uintptr_t end)
{
	vmem_seg_t *newseg;
	uintptr_t vs_start = vsp->vs_start;
	uintptr_t vs_end = vsp->vs_end;
	uintptr_t p2end = P2ROUNDUP(end, vmp->vm_quantum);

	ASSERT(P2PHASE(vs_start, vmp->vm_quantum) == 0);

	if (start < vs_start || end - 1 > vs_end - 1 ||
	    start - 1 >= end - 1 || vsp->vs_type != VMEM_FREE)
		panic("vmem_seg_alloc(%p, %p, %lx, %lx): bad arguments",
		    vmp, vsp, start, end);

	if (vs_end != p2end) {
		vsp->vs_end = p2end;
		newseg = vmem_seg_create(vmp, vsp, p2end, vs_end, VMEM_FREE);
		vmem_freelist_insert(vmp, newseg);
	}

	if (vs_start != start) {
		vsp->vs_end = start;
		newseg = vmem_seg_create(vmp, vsp, start, end, VMEM_ALLOC);
		vmem_freelist_insert(vmp, vsp);
		vsp = newseg;
	}

	vsp->vs_end = end;
	vmem_hash_insert(vmp, vsp);
}

/*
 * Populate vmp's segfree list with 'nsegneeded' vmem_seg_t structures.
 */
static int
vmem_populate(vmem_t *vmp, ssize_t nsegneeded, int vmflag)
{
	vmem_seg_t *vsp, *oldvsp, vtemp;
	vmem_t *hmp = heap_arena;
	vmem_t *smp = vmem_seg_arena;
	uintptr_t heappage, newpage;
	page_t *pp = NULL;
	vnode_t fake;
	ssize_t nseg;

	while (vmp->vm_nsegfree < nsegneeded) {
		mutex_exit(&vmp->vm_lock);
		mutex_enter(&smp->vm_lock);
		if (smp->vm_nsegfree > 0) {
			vsp = vmem_getseg(smp);
			mutex_exit(&smp->vm_lock);
			mutex_enter(&vmp->vm_lock);
			vmem_putseg(vmp, vsp);
			continue;
		}
		mutex_exit(&smp->vm_lock);
		/*
		 * To allocate a page of vmem_seg_t structures, we need
		 * a page of heap_arena and a page of physical memory.
		 * But we can't allocate from heap_arena unless we have
		 * some vmem_seg_t structures.  We can get around this by
		 * using a temporary vmem_seg_t allocated on our stack,
		 * which we can replace with a 'real' vmem_seg_t if the
		 * physical page allocation succeeds.  But if the physical
		 * page allocation fails, we have no memory with which we
		 * can replace our temporary segment.  Therefore, we have to
		 * know in advance whether page_create_va() will succeed.
		 * But we can't create a page in kvp unless we know what
		 * its address is, since that's used as the vnode offset.
		 * So, we instead create the page using a fake vnode, and
		 * later rename it into kvp once we have a heap address.
		 * If there's an easier way to do this, it eludes me.
		 */
		if (kvseg.s_base != NULL) {	/* VM system is ready */
			bzero(&fake, sizeof (fake));
			if (page_resv(1, vmflag & VM_KMFLAGS) == 0) {
				mutex_enter(&vmp->vm_lock);
				break;
			}

			/*
			 * Guess the 'heappage' VA so that page_create_va()
			 * will usually choose the right color.  It's only
			 * a guess because we drop hmp->vm_lock across the
			 * call to page_create_va().
			 */
			heappage = 0;
			mutex_enter(&hmp->vm_lock);
			for (vsp = hmp->vm_freelist[hmp->vm_qshift].vs_knext;
			    vsp != NULL; vsp = vsp->vs_knext) {
				if (vsp->vs_start != 0) {
					heappage = vsp->vs_end - PAGESIZE;
					break;
				}
			}
			mutex_exit(&hmp->vm_lock);
			pp = page_create_va(&fake, (u_offset_t)0, PAGESIZE,
			    PG_NORELOC | PG_EXCL |
			    ((vmflag & VM_NOSLEEP) ? 0 : PG_WAIT),
			    &kvseg, (void *)heappage);
			if (pp == NULL) {
				page_unresv(1);
				mutex_enter(&vmp->vm_lock);
				break;
			}
			page_io_unlock(pp);
		}
		/*
		 * Allocate a heap address 'by hand'.  We can't just call
		 * vmem_alloc(heap_arena, PAGESIZE, vmflag) for two reasons:
		 * (1) we'd immediately recurse into vmem_populate(), and
		 * (2) we need precise control over how 'vtemp' is used.
		 */
		mutex_enter(&hmp->vm_lock);
		for (;;) {
			for (vsp = hmp->vm_freelist[hmp->vm_qshift].vs_knext;
			    vsp != NULL; vsp = vsp->vs_knext)
				if (vsp->vs_start != 0)
					break;
			if (vsp != NULL || (vmflag & VM_NOSLEEP))
				break;
			/*
			 * Note: the populate_wait kstat for all arenas
			 * is protected by the lock for heap_arena.
			 */
			vmp->vm_kstat.vk_populate_wait.value.ui64++;
			cv_wait(&hmp->vm_cv, &hmp->vm_lock);
		}
		if (vsp == NULL) {
			mutex_exit(&hmp->vm_lock);
			page_destroy(pp, 0);
			page_unresv(1);
			mutex_enter(&vmp->vm_lock);
			break;
		}
		vmem_freelist_delete(hmp, vsp);

		/*
		 * If the size of 'vsp' is not exactly PAGESIZE, add
		 * the temporary segment 'vtemp' to the freelist so
		 * vmem_seg_alloc() can split 'vsp'.  Note: we are careful
		 * to allocate from the *end* of 'vsp' so that 'vtemp' will
		 * represent 'heappage', not the remaining free area.
		 * This simplifies the reclamation of 'vtemp' below.
		 */
		vtemp.vs_start = 0;
		if (VS_SIZE(vsp) != PAGESIZE)
			vmem_putseg(hmp, &vtemp);
		heappage = vsp->vs_end - PAGESIZE;
		vmem_seg_alloc(hmp, vsp, heappage, heappage + PAGESIZE);
		mutex_exit(&hmp->vm_lock);

		if (pp != NULL) {
			/*
			 * Now rename the page into the kernel vnode at offset
			 * 'heappage', downgrade, and load the translation.
			 */
			page_rename(pp, &kvp, heappage);
			page_downgrade(pp);
			ASSERT((uintptr_t)pp->p_offset == heappage);
			hat_memload(kas.a_hat, (caddr_t)pp->p_offset, pp,
			    (PROT_ALL & ~PROT_USER) | HAT_NOSYNC,
			    HAT_LOAD_LOCK);
			newpage = heappage;
		} else {
			newpage = (uintptr_t)boot_alloc((void *)heappage,
			    PAGESIZE, PAGESIZE);
		}

		nseg = PAGESIZE / vmem_seg_size;

		/*
		 * If we used 'vtemp' as a temporary segment,
		 * replace it with a real segment.
		 */
		if (vtemp.vs_start != 0) {
			vsp = (vmem_seg_t *)(newpage + vmem_seg_size * --nseg);
			mutex_enter(&hmp->vm_lock);
			oldvsp = vmem_hash_delete(hmp, heappage, PAGESIZE);
			ASSERT(oldvsp == &vtemp);
			bcopy(oldvsp, vsp, vmem_seg_size);
			VMEM_DELETE(oldvsp, a);
			VMEM_INSERT(vsp->vs_aprev, vsp, a);
			vmem_hash_insert(hmp, vsp);
			mutex_exit(&hmp->vm_lock);
			if (newpage != heappage)
				vmem_free(hmp, (void *)heappage, PAGESIZE);
		}

		/*
		 * Add the rest of our newly-minted segments to the pool
		 * and then fake up the accounting: make it appear that
		 * this page was imported from heap_arena to vmem_seg_arena.
		 */
		mutex_enter(&smp->vm_lock);
		while (--nseg >= 0)
			vmem_putseg(smp,
			    (vmem_seg_t *)(newpage + vmem_seg_size * nseg));
		vsp = vmem_span_create(smp, (void *)newpage, PAGESIZE, 1);
		vmem_freelist_delete(smp, vsp);
		vmem_seg_alloc(smp, vsp, newpage, newpage + PAGESIZE);
		mutex_exit(&smp->vm_lock);

		mutex_enter(&vmp->vm_lock);
	}
	if (vmp->vm_nsegfree >= nsegneeded)
		return (1);
	vmp->vm_kstat.vk_populate_fail.value.ui64++;
	return (0);
}

void *
vmem_xalloc(vmem_t *vmp, size_t size, size_t align, size_t phase,
	size_t nocross, void *minaddr, void *maxaddr, int vmflag)
{
	vmem_seg_t *vsp;
	vmem_seg_t *vbest = NULL;
	uintptr_t addr, taddr, start, end;
	void *vaddr;
	int flist;
	int reaped = 0;

	if (size == 0)
		panic("vmem_xalloc(): size == 0");

	if ((align | phase | nocross) & (vmp->vm_quantum - 1))
		panic("vmem_xalloc(%p, %lu, %lu, %lu, %lu, %p, %p, %x): "
		    "parameters not vm_quantum aligned",
		    (void *)vmp, size, align, phase, nocross,
		    minaddr, maxaddr, vmflag);

	if (nocross != 0 &&
	    (align > nocross || P2ROUNDUP(phase + size, align) > nocross))
		panic("vmem_xalloc(%p, %lu, %lu, %lu, %lu, %p, %p, %x): "
		    "overconstrained allocation",
		    (void *)vmp, size, align, phase, nocross,
		    minaddr, maxaddr, vmflag);

	mutex_enter(&vmp->vm_lock);
	for (;;) {
		/*
		 * We need at most 4 vmem_seg_t structures to satisfy an
		 * allocation, because the worst case is as follows:
		 * We're out of memory, so we have to have to import more;
		 * that means creating a new span, which takes 2 vmem_seg_t's
		 * (one for the span marker, one for the new segment).
		 * Then, if the allocation is constrained, it may come
		 * from the middle of the segment, which means we'll need
		 * 2 additional vmem_seg_t's to represent the leftovers
		 * on either side of the allocated region.  2 + 2 = 4.
		 */
		if (!vmem_populate(vmp, 4, vmflag))
			break;

		/*
		 * highbit() returns the highest bit + 1, which is exactly
		 * what we want: we want to search the first freelist whose
		 * members are *definitely* large enough to satisfy our
		 * allocation.  However, there are certain cases in which we
		 * want to look at the next-smallest freelist (which *might*
		 * be able to satisfy the allocation):
		 *
		 * (1)	The size is exactly a power of 2, in which case
		 *	the smaller freelist is always big enough;
		 *
		 * (2)	All other freelists are empty;
		 *
		 * (3)	We're in the highest possible freelist, which is
		 *	always empty (e.g. the 4GB freelist on 32-bit systems);
		 *
		 * (4)	We're doing a best-fit allocation.
		 */
		flist = highbit(size);
		if ((size & (size - 1)) == 0 || vmp->vm_freemap >> flist == 0 ||
		    flist == VMEM_FREELISTS || (vmflag & VM_BESTFIT))
			flist--;
		vbest = NULL;
		for (vsp = vmp->vm_freelist[flist].vs_knext;
		    vsp != NULL; vsp = vsp->vs_knext) {
			vmp->vm_kstat.vk_search.value.ui64++;
			if (vsp->vs_start == 0) {
				/*
				 * We're moving up to a larger freelist,
				 * so if we've already found a candidate,
				 * the fit can't possibly get any better.
				 */
				if (vbest != NULL)
					break;
				/*
				 * Find the next non-empty freelist.
				 */
				flist = lowbit(vmp->vm_freemap & -VS_SIZE(vsp));
				if (flist-- == 0)
					break;
				vsp = (vmem_seg_t *)&vmp->vm_freelist[flist];
				ASSERT(vsp->vs_knext->vs_type == VMEM_FREE);
				continue;
			}
			if (vsp->vs_end - 1 < (uintptr_t)minaddr)
				continue;
			if (vsp->vs_start > (uintptr_t)maxaddr - 1)
				continue;
			start = MAX(vsp->vs_start, (uintptr_t)minaddr);
			end = MIN(vsp->vs_end - 1, (uintptr_t)maxaddr - 1) + 1;
			taddr = P2PHASEUP(start, align, phase);
			if (P2CROSS(taddr, taddr + size - 1, nocross))
				taddr +=
				    P2ROUNDUP(P2NPHASE(taddr, nocross), align);
			if ((taddr - start) + size > end - start ||
			    (vbest != NULL && VS_SIZE(vsp) >= VS_SIZE(vbest)))
				continue;
			vbest = vsp;
			addr = taddr;
			if (!(vmflag & VM_BESTFIT) || VS_SIZE(vbest) == size)
				break;
		}
		if (vbest != NULL)
			break;
		if (vmp->vm_source_alloc != NULL && nocross == 0 &&
		    minaddr == NULL && maxaddr == NULL) {
			size_t asize = P2ROUNDUP(size + phase,
			    MAX(align, vmp->vm_source->vm_quantum));
			ASSERT(vmp->vm_nsegfree >= 4);
			vmp->vm_nsegfree -= 4;		/* reserve our segs */
			mutex_exit(&vmp->vm_lock);
			vaddr = vmp->vm_source_alloc(vmp->vm_source,
			    asize, vmflag);
			mutex_enter(&vmp->vm_lock);
			vmp->vm_nsegfree += 4;		/* claim reservation */
			if (vaddr != NULL) {
				vbest = vmem_span_create(vmp, vaddr, asize, 1);
				addr = P2PHASEUP(vbest->vs_start, align, phase);
				break;
			}
		}
		if (reaped++ == 0) {
			mutex_exit(&vmp->vm_lock);
			kmem_reap();
			mutex_enter(&vmp->vm_lock);
		}
		if (vmflag & VM_NOSLEEP)
			break;
		vmp->vm_kstat.vk_wait.value.ui64++;
		cv_wait(&vmp->vm_cv, &vmp->vm_lock);
	}
	if (vbest != NULL) {
		ASSERT(vbest->vs_type == VMEM_FREE);
		ASSERT(vbest->vs_knext != vbest);
		vmem_freelist_delete(vmp, vbest);
		vmem_seg_alloc(vmp, vbest, addr, addr + size);
		mutex_exit(&vmp->vm_lock);
		ASSERT(P2PHASE(addr, align) == phase);
		ASSERT(!P2CROSS(addr, addr + size - 1, nocross));
		ASSERT(addr >= (uintptr_t)minaddr);
		ASSERT(addr + size - 1 <= (uintptr_t)maxaddr - 1);
		return ((void *)addr);
	}
	vmp->vm_kstat.vk_fail.value.ui64++;
	mutex_exit(&vmp->vm_lock);
	if (vmflag & VM_PANIC)
		panic("vmem_xalloc(%p, %lu, %lu, %lu, %lu, %p, %p, %x): "
		    "cannot satisfy mandatory allocation",
		    (void *)vmp, size, align, phase, nocross,
		    minaddr, maxaddr, vmflag);
	return (NULL);
}

void
vmem_xfree(vmem_t *vmp, void *vaddr, size_t size)
{
	vmem_seg_t *vsp, *vnext, *vprev;

	mutex_enter(&vmp->vm_lock);

	vsp = vmem_hash_delete(vmp, (uintptr_t)vaddr, size);
	vsp->vs_end = P2ROUNDUP(vsp->vs_end, vmp->vm_quantum);

	/*
	 * Attempt to coalesce with the next segment.
	 */
	vnext = vsp->vs_anext;
	if (vnext->vs_type == VMEM_FREE) {
		ASSERT(vsp->vs_end == vnext->vs_start);
		vmem_freelist_delete(vmp, vnext);
		vsp->vs_end = vnext->vs_end;
		vmem_seg_destroy(vmp, vnext);
	}

	/*
	 * Attempt to coalesce with the previous segment.
	 */
	vprev = vsp->vs_aprev;
	if (vprev->vs_type == VMEM_FREE) {
		ASSERT(vprev->vs_end == vsp->vs_start);
		vmem_freelist_delete(vmp, vprev);
		vprev->vs_end = vsp->vs_end;
		vmem_seg_destroy(vmp, vsp);
		vsp = vprev;
	}

	/*
	 * If the entire span is free, return it to the source.
	 */
	if (vsp->vs_import && vmp->vm_source_free != NULL &&
	    vsp->vs_aprev->vs_type == VMEM_SPAN &&
	    vsp->vs_anext->vs_type == VMEM_SPAN) {
		vaddr = (void *)vsp->vs_start;
		size = VS_SIZE(vsp);
		ASSERT(size == VS_SIZE(vsp->vs_aprev));
		vmem_span_destroy(vmp, vsp);
		mutex_exit(&vmp->vm_lock);
		vmp->vm_source_free(vmp->vm_source, vaddr, size);
	} else {
		vmem_freelist_insert(vmp, vsp);
		mutex_exit(&vmp->vm_lock);
	}
}

void *
vmem_alloc(vmem_t *vmp, size_t size, int vmflag)
{
	if (size - 1 >= vmp->vm_qcache_max)
		return (vmem_xalloc(vmp, size, vmp->vm_quantum, 0, 0,
		    NULL, NULL, vmflag));
	return (kmem_cache_alloc(vmp->vm_qcache[(size - 1) >> vmp->vm_qshift],
	    vmflag & VM_KMFLAGS));
}

void
vmem_free(vmem_t *vmp, void *vaddr, size_t size)
{
	if (size - 1 >= vmp->vm_qcache_max)
		vmem_xfree(vmp, vaddr, size);
	else
		kmem_cache_free(vmp->vm_qcache[(size - 1) >> vmp->vm_qshift],
		    vaddr);
}

int
vmem_contains(vmem_t *vmp, void *vaddr, size_t size)
{
	uintptr_t start = (uintptr_t)vaddr;
	uintptr_t end = start + size;
	vmem_seg_t *vsp;
	vmem_seg_t *seg0 = &vmp->vm_seg0;

	mutex_enter(&vmp->vm_lock);
	vmp->vm_kstat.vk_contains.value.ui64++;
	for (vsp = seg0->vs_knext; vsp != seg0; vsp = vsp->vs_knext) {
		vmp->vm_kstat.vk_contains_search.value.ui64++;
		ASSERT(vsp->vs_type == VMEM_SPAN);
		if (start >= vsp->vs_start && end - 1 <= vsp->vs_end - 1)
			break;
	}
	mutex_exit(&vmp->vm_lock);
	return (vsp != seg0);
}

void *
vmem_add(vmem_t *vmp, void *vaddr, size_t size, int vmflag)
{
	if (vaddr == NULL || size == 0)
		panic("vmem_add(%p, %p, %lu): bad arguments", vmp, vaddr, size);

	ASSERT(!vmem_contains(vmp, vaddr, size));

	mutex_enter(&vmp->vm_lock);
	/*
	 * Creating a span requires 2 vmem_seg_t structures:
	 * one for the span marker and one for the new segment.
	 */
	if (vmem_populate(vmp, 2, vmflag))
		(void) vmem_span_create(vmp, vaddr, size, 0);
	else
		vaddr = NULL;
	mutex_exit(&vmp->vm_lock);
	return (vaddr);
}

void
vmem_walk(vmem_t *vmp, int typemask,
	void (*func)(void *, void *, size_t), void *arg)
{
	vmem_seg_t *vsp;
	vmem_seg_t *seg0 = &vmp->vm_seg0;

	mutex_enter(&vmp->vm_lock);
	for (vsp = seg0->vs_anext; vsp != seg0; vsp = vsp->vs_anext)
		if (vsp->vs_type & typemask)
			func(arg, (void *)vsp->vs_start, VS_SIZE(vsp));
	mutex_exit(&vmp->vm_lock);
}

size_t
vmem_size(vmem_t *vmp, int typemask)
{
	uint64_t size = 0;

	if (typemask & VMEM_ALLOC)
		size += vmp->vm_kstat.vk_mem_inuse.value.ui64;
	if (typemask & VMEM_FREE)
		size += vmp->vm_kstat.vk_mem_total.value.ui64 -
		    vmp->vm_kstat.vk_mem_inuse.value.ui64;
	return ((size_t)size);
}

static void
vmem_kstat_create(vmem_t *vmp)
{
	if ((vmp->vm_ksp = kstat_create("vmem", vmp->vm_id, vmp->vm_name,
	    "vmem", KSTAT_TYPE_NAMED, sizeof (vmem_kstat_t) /
	    sizeof (kstat_named_t), KSTAT_FLAG_VIRTUAL)) != NULL) {
		vmp->vm_ksp->ks_data = &vmp->vm_kstat;
		kstat_install(vmp->vm_ksp);
	}
}

vmem_t *
vmem_create(const char *name, void *base, size_t size, size_t quantum,
	void *(*afunc)(vmem_t *, size_t, int),
	void (*ffunc)(vmem_t *, void *, size_t),
	vmem_t *source, size_t qcache_max, int vmflag)
{
	int i;
	size_t nqcache;
	vmem_t *vmp, *cur, **vmpp;
	vmem_seg_t *vsp;
	vmem_freelist_t *vfp;
	uint32_t id = atomic_add_32_nv(&vmem_id, 1);

	if (vmem_vmem_arena != NULL) {
		vmp = vmem_alloc(vmem_vmem_arena, sizeof (vmem_t), vmflag);
	} else {
		ASSERT(id <= VMEM_INITIAL);
		vmp = &vmem0[id - 1];
	}

	if (vmp == NULL)
		return (NULL);
	bzero(vmp, sizeof (vmem_t));

	(void) snprintf(vmp->vm_name, VMEM_NAMELEN, "%s", name);
	mutex_init(&vmp->vm_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vmp->vm_cv, NULL, CV_DEFAULT, NULL);

	vmp->vm_quantum = quantum;
	vmp->vm_qshift = highbit(quantum) - 1;
	nqcache = MIN(qcache_max >> vmp->vm_qshift, VMEM_NQCACHE_MAX);

	for (i = 0; i <= VMEM_FREELISTS; i++) {
		vfp = &vmp->vm_freelist[i];
		vfp->vs_end = 1UL << i;
		vfp->vs_knext = (vmem_seg_t *)(vfp + 1);
		vfp->vs_kprev = (vmem_seg_t *)(vfp - 1);
	}
	vmp->vm_freelist[0].vs_kprev = NULL;
	vmp->vm_freelist[VMEM_FREELISTS].vs_knext = NULL;
	vmp->vm_hash_table = vmp->vm_hash0;
	vmp->vm_hash_mask = VMEM_HASH_INITIAL - 1;
	vmp->vm_hash_shift = highbit(vmp->vm_hash_mask);

	vsp = &vmp->vm_seg0;
	vsp->vs_anext = vsp;
	vsp->vs_aprev = vsp;
	vsp->vs_knext = vsp;
	vsp->vs_kprev = vsp;
	vsp->vs_type = VMEM_SPAN;

	bcopy(&vmem_kstat_template, &vmp->vm_kstat, sizeof (vmem_kstat_t));

	vmp->vm_id = id;
	if (source != NULL)
		vmp->vm_kstat.vk_source_id.value.ui32 = source->vm_id;
	vmp->vm_source = source;
	vmp->vm_source_alloc = afunc;
	vmp->vm_source_free = ffunc;

	if (nqcache != 0) {
		ASSERT(!(vmflag & VM_NOSLEEP));
		vmp->vm_qcache_max = nqcache << vmp->vm_qshift;
		for (i = 0; i < nqcache; i++) {
			char buf[VMEM_NAMELEN + 21];
			(void) sprintf(buf, "%s_%lu", vmp->vm_name,
			    (i + 1) * quantum);
			vmp->vm_qcache[i] = kmem_cache_create(buf,
			    (i + 1) * quantum, quantum, NULL, NULL, NULL,
			    NULL, vmp, vmem_qcache_cflags);
		}
	}

	if (kmem_ready)
		vmem_kstat_create(vmp);

	mutex_enter(&vmem_list_lock);
	vmpp = &vmem_list;
	while ((cur = *vmpp) != NULL)
		vmpp = &cur->vm_next;
	*vmpp = vmp;
	mutex_exit(&vmem_list_lock);

	if ((base || size) && vmem_add(vmp, base, size, vmflag) == NULL) {
		vmem_destroy(vmp);
		return (NULL);
	}

	return (vmp);
}

void
vmem_destroy(vmem_t *vmp)
{
	vmem_t *cur, **vmpp;
	vmem_t *smp = vmem_seg_arena;
	size_t leaked;
	int i;

	mutex_enter(&vmem_list_lock);
	vmpp = &vmem_list;
	while ((cur = *vmpp) != vmp)
		vmpp = &cur->vm_next;
	*vmpp = vmp->vm_next;
	mutex_exit(&vmem_list_lock);

	for (i = 0; i < VMEM_NQCACHE_MAX; i++)
		if (vmp->vm_qcache[i])
			kmem_cache_destroy(vmp->vm_qcache[i]);

	leaked = vmem_size(vmp, VMEM_ALLOC);
	if (leaked != 0)
		cmn_err(CE_WARN, "vmem_destroy('%s'): leaked %lu bytes",
		    vmp->vm_name, leaked);

	if (vmp->vm_hash_table != vmp->vm_hash0)
		kmem_free(vmp->vm_hash_table,
		    (vmp->vm_hash_mask + 1) * sizeof (void *));

	mutex_enter(&smp->vm_lock);
	while (vmp->vm_nsegfree > 0)
		vmem_putseg(smp, vmem_getseg(vmp));
	mutex_exit(&smp->vm_lock);

	if (vmp->vm_ksp != NULL)
		kstat_delete(vmp->vm_ksp);

	mutex_destroy(&vmp->vm_lock);
	cv_destroy(&vmp->vm_cv);
	vmem_free(vmem_vmem_arena, vmp, sizeof (vmem_t));
}

/*
 * Resize vmp's hash table to keep the average lookup depth near 1.0.
 */
static void
vmem_hash_rescale(vmem_t *vmp)
{
	vmem_seg_t **old_table, **new_table, *vsp;
	size_t old_size, new_size, h, nseg;

	nseg = (size_t)(vmp->vm_kstat.vk_alloc.value.ui64 -
	    vmp->vm_kstat.vk_free.value.ui64);

	new_size = MAX(VMEM_HASH_INITIAL, 1 << (highbit(3 * nseg + 4) - 2));
	old_size = vmp->vm_hash_mask + 1;

	if ((old_size >> 1) <= new_size && new_size <= (old_size << 1))
		return;

	new_table = kmem_zalloc(new_size * sizeof (void *), KM_NOSLEEP);
	if (new_table == NULL)
		return;

	mutex_enter(&vmp->vm_lock);

	old_size = vmp->vm_hash_mask + 1;
	old_table = vmp->vm_hash_table;

	vmp->vm_hash_mask = new_size - 1;
	vmp->vm_hash_table = new_table;
	vmp->vm_hash_shift = highbit(vmp->vm_hash_mask);

	for (h = 0; h < old_size; h++) {
		vsp = old_table[h];
		while (vsp != NULL) {
			uintptr_t addr = vsp->vs_start;
			vmem_seg_t *next_vsp = vsp->vs_knext;
			vmem_seg_t **hash_bucket = VMEM_HASH(vmp, addr);
			vsp->vs_knext = *hash_bucket;
			*hash_bucket = vsp;
			vsp = next_vsp;
		}
	}

	mutex_exit(&vmp->vm_lock);

	if (old_table != vmp->vm_hash0)
		kmem_free(old_table, old_size * sizeof (void *));
}

/*
 * Perform periodic maintenance on all vmem arenas.
 */
static void
vmem_update(void *dummy)
{
	vmem_t *vmp;

	mutex_enter(&vmem_list_lock);
	for (vmp = vmem_list; vmp != NULL; vmp = vmp->vm_next)
		vmem_hash_rescale(vmp);
	mutex_exit(&vmem_list_lock);

	(void) timeout(vmem_update, dummy, vmem_update_interval * hz);
}

void
vmem_init(void)
{
	int nseg = VMEM_SEG_INITIAL;
	uint32_t id;

	vmem_seg_arena = vmem_create("vmem_seg", NULL, 0, PAGESIZE,
	    segkmem_alloc, segkmem_free, heap_arena, 0, VM_SLEEP);

	while (--nseg >= 0)
		vmem_putseg(vmem_seg_arena, &vmem_seg0[nseg]);

	vmem_vmem_arena = vmem_create("vmem_vmem", vmem0, sizeof (vmem0), 1,
	    segkmem_alloc, segkmem_free, heap_arena, 0, VM_SLEEP);

	for (id = 0; id < vmem_id; id++)
		(void) vmem_xalloc(vmem_vmem_arena, sizeof (vmem_t), 1, 0, 0,
		    &vmem0[id], &vmem0[id + 1],
		    VM_NOSLEEP | VM_BESTFIT | VM_PANIC);
}

void
vmem_kstat_init(int audit)
{
	int i;

	for (i = 0; i < VMEM_INITIAL; i++)
		if (vmem0[i].vm_id != 0)
			vmem_kstat_create(&vmem0[i]);

	if (!audit)
		vmem_seg_size = offsetof(vmem_seg_t, vs_thread);
}

void
vmem_mp_init(void)
{
	vmem_update(NULL);
}
