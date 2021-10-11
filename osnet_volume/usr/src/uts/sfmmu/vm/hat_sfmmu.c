/*
 * Copyright (c) 1989-1991,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hat_sfmmu.c	1.203	99/09/23 SMI"

/*
 * VM - Hardware Address Translation management for Spitfire MMU.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <vm/hat.h> while the machine dependent interface
 * and data structures are described in <vm/hat_sfmmu.h>.
 *
 * The hat layer manages the address translation hardware as a cache
 * driven by calls from the higher levels in the VM system.
 */

#include <sys/types.h>
#include <vm/hat.h>
#include <vm/hat_sfmmu.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <sys/pte.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <sys/t_lock.h>
#include <sys/obpdefs.h>
#include <sys/vm_machparam.h>
#include <sys/var.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/scb.h>
#include <sys/bitmap.h>
#include <sys/machlock.h>
#include <sys/membar.h>
#include <sys/atomic.h>
#include <sys/atomic_prim.h>
#include <sys/cpu_module.h>
#include <sys/prom_debug.h>
#include <sys/vmsystm.h>

#if defined(__sparcv9) && defined(SF_ERRATA_57)
extern caddr_t errata57_limit;
#endif

/*
 * SFMMU specific hat functions
 */
void	hat_pagecachectl(struct page *, int);

/* flags for hat_pagecachectl */
#define	HAT_CACHE	0x1
#define	HAT_UNCACHE	0x2
#define	HAT_TMPNC	0x4

/*
 * Flag to disable large page support.
 * 	value of 1 => disable all large pages.
 *	bits 2, 3, and 4 are to disable 64K, 512K and 4M pages respectively.
 *
 * For example, use the value 0x4 to disable 512K pages.
 *
 * For now don't use 512k tte. See Bugid: 4012109
 */
#define	LARGE_PAGES_OFF		0x1

int	disable_large_pages = (1 << TTE512K);

/*
 * Private sfmmu data structures for hat management
 */
static struct kmem_cache *sfmmuid_cache;

/*
 * Private sfmmu data structures for ctx management
 */
static struct ctx	*ctxhand;	/* hand used while stealing ctxs */
static struct ctx	*ctxfree;	/* head of free ctx list */
static struct ctx	*ctxdirty;	/* head of dirty ctx list */

/*
 * sfmmu static variables for hmeblk resource management.
 */
static struct kmem_cache *sfmmu8_cache;
static struct kmem_cache *sfmmu1_cache;

static struct hme_blk 	*hblk1_flist;	/* freelist for 1 hment hme_blks */
static struct hme_blk 	*hblk8_flist;	/* freelist for 8 hment hme_blks */
static struct hme_blk 	*hblk1_flist_t;	/* tail of hblk1 freelist */
static struct hme_blk 	*hblk8_flist_t;	/* tail of hblk8 freelist */
static uint_t 		hblk1_avail;	/* number of free 1 hme hme_blks */
static uint_t 		hblk8_avail;	/* number of free 8 hme hme_blks */
static kmutex_t 	hblk1_lock;	/* mutex for hblk1 freelist */
static kmutex_t 	hblk8_lock;	/* mutex for hblk8 freelist */
static kmutex_t 	ctx_lock;	/* mutex for ctx structures */
static kmutex_t 	ism_mlist_lock;	/* mutex for ism mapping list */

static uint_t 	hblk8_allocated, hblk1_allocated;
static uint_t	hblk8_prealloc_count;
static uint_t	hblkalloc_inprog;

#ifdef DEBUG
static uint_t 	hblk8_inuse, hblk1_inuse;
static uint_t	nhblk8_allocated, nhblk1_allocated;
#define		HBLK_DEBUG_COUNTER_INCR(counter, value)	counter += value
#define		HBLK_DEBUG_COUNTER_DECR(counter, value)	counter -= value
#else
#define		HBLK_DEBUG_COUNTER_INCR(counter, value)
#define		HBLK_DEBUG_COUNTER_DECR(counter, value)
#endif /* DEBUG */

/*
 * private data for ism
 */
static struct kmem_cache *ism_blk_cache;
static struct kmem_cache *ism_ment_cache;
#define	ISMID_STARTADDR	NULL

/*
 * Private sfmmu routines (prototypes)
 */
static struct hme_blk *sfmmu_shadow_hcreate(sfmmu_t *, caddr_t, int);
static struct 	hme_blk *sfmmu_hblk_alloc(sfmmu_t *, caddr_t,
			struct hmehash_bucket *, uint_t, hmeblk_tag);
static struct sf_hment *
		sfmmu_hblktohme(struct hme_blk *, caddr_t, int *);
static caddr_t	sfmmu_hblk_unload(struct hat *, struct hme_blk *, caddr_t,
			caddr_t, uint_t);
static caddr_t	sfmmu_hblk_sync(struct hat *, struct hme_blk *, caddr_t,
			caddr_t, int);
static void	sfmmu_hblk_free(struct hmehash_bucket *, struct hme_blk *,
			uint64_t);
static struct hme_blk *sfmmu_hblk_grow(int, int);
static struct hme_blk *sfmmu_hblk_steal(int);
static int	sfmmu_steal_this_hblk(struct hmehash_bucket *,
			struct hme_blk *, uint64_t, uint64_t,
			struct hme_blk *);

static struct hme_blk *
		sfmmu_hmetohblk(struct sf_hment *);

static void	sfmmu_memload_batchsmall(struct hat *, caddr_t, machpage_t **,
		    uint_t, uint_t, pgcnt_t);
void		sfmmu_tteload(struct hat *, tte_t *, caddr_t, machpage_t *,
			uint_t);
static int	sfmmu_tteload_array(sfmmu_t *, tte_t *, caddr_t, machpage_t **,
			uint_t);
static struct hmehash_bucket *sfmmu_tteload_acquire_hashbucket(sfmmu_t *,
					caddr_t, int);
static struct hme_blk *sfmmu_tteload_find_hmeblk(sfmmu_t *,
				struct hmehash_bucket *, caddr_t, uint_t);
static int	sfmmu_tteload_addentry(sfmmu_t *, struct hme_blk *, tte_t *,
			caddr_t, machpage_t **, uint_t);
static void	sfmmu_tteload_release_hashbucket(struct hmehash_bucket *);

static int	sfmmu_pagearray_setup(caddr_t, machpage_t **, tte_t *, int);
pfn_t		sfmmu_uvatopfn(caddr_t, sfmmu_t *);
void		sfmmu_memtte(tte_t *, pfn_t, uint_t, int);
static void	sfmmu_vac_conflict(struct hat *, caddr_t, machpage_t *);
static int	sfmmu_vacconflict_array(caddr_t, machpage_t *, int *);
static int	tst_tnc(machpage_t *pp, pgcnt_t);
static void	conv_tnc(machpage_t *pp, int);

static struct ctx *sfmmu_get_ctx(sfmmu_t *);
static void	sfmmu_free_ctx(sfmmu_t *, struct ctx *);
static void	sfmmu_free_sfmmu(sfmmu_t *);
static tte_t	sfmmu_gettte(struct hat *, caddr_t);
static caddr_t	sfmmu_hblk_unlock(struct hme_blk *, caddr_t, caddr_t);
static void	sfmmu_chgattr(struct hat *, caddr_t, size_t, uint_t, int);
static cpuset_t	sfmmu_pageunload(machpage_t *, struct sf_hment *, int);
static cpuset_t	sfmmu_pagesync(machpage_t *, struct sf_hment *, uint_t);
static void	sfmmu_ttesync(struct hat *, caddr_t, tte_t *, machpage_t *);
static void	sfmmu_page_cache_array(machpage_t *, int, int, pgcnt_t);
static void	sfmmu_page_cache(machpage_t *, int, int, int);
static void	sfmmu_tlbcache_demap(caddr_t, sfmmu_t *, struct hme_blk *,
			pfn_t, int, int, int, int);
static void	sfmmu_ismtlbcache_demap(caddr_t, sfmmu_t *, struct hme_blk *,
			pfn_t, int);
static void	sfmmu_tlb_demap(caddr_t, sfmmu_t *, struct hme_blk *, int, int);
static void	sfmmu_tlb_ctx_demap(sfmmu_t *);

static void	sfmmu_cache_flush(pfn_t, int);
static void	sfmmu_cache_flushcolor(int, pfn_t);
static caddr_t	sfmmu_hblk_chgattr(struct hme_blk *, caddr_t, caddr_t, uint_t,
			int);

static uint64_t	sfmmu_vtop_attr(uint_t, int mode, tte_t *);
static uint_t	sfmmu_ptov_attr(tte_t *);
static caddr_t	sfmmu_hblk_chgprot(struct hme_blk *, caddr_t, caddr_t, uint_t);
static uint_t	sfmmu_vtop_prot(uint_t, uint_t *);
static int	sfmmu_idcache_constructor(void *, void *, int);
static void	sfmmu_idcache_destructor(void *, void *);
static int	sfmmu_hblkcache_constructor(void *, void *, int);
static void	sfmmu_hblkcache_destructor(void *, void *);
static void	sfmmu_hblkcache_reclaim(void *);
static void	sfmmu_tte_unshare(struct hat *, struct hat *, caddr_t, size_t);
static void	sfmmu_hblk_tofreelist(struct hme_blk *, uint64_t);
static void	sfmmu_reuse_ctx(struct ctx *, sfmmu_t *);
static void	sfmmu_shadow_hcleanup(sfmmu_t *, struct hme_blk *,
			struct hmehash_bucket *);
static void	sfmmu_free_hblks(sfmmu_t *, caddr_t, caddr_t, int);

static void	hme_add(struct sf_hment *, machpage_t *);
static void	hme_sub(struct sf_hment *, machpage_t *);
static void	sfmmu_rm_large_mappings(machpage_t *, int);

static void	hat_lock_init(void);
static void	hat_kstat_init(void);
static int	sfmmu_kstat_percpu_update(kstat_t *ksp, int rw);
static void	sfmmu_expand_tsbsize(sfmmu_t *);
static int	fnd_mapping_sz(machpage_t *);
static void	iment_add(struct ism_ment *,  struct hat *);
static void	iment_sub(struct ism_ment *, struct hat *);

#ifdef DEBUG
static void	sfmmu_check_hblk_flist();
#endif

/*
 * Semi-private sfmmu data structures.  Some of them are initialize in
 * startup or in hat_init. Some of them are private but accessed by
 * assembly code or mach_sfmmu.c
 */
struct hmehash_bucket *uhme_hash;	/* user hmeblk hash table */
struct hmehash_bucket *khme_hash;	/* kernel hmeblk hash table */
int 		uhmehash_num;		/* # of buckets in user hash table */
int 		khmehash_num;		/* # of buckets in kernel hash table */
struct ctx	*ctxs, *ectxs;		/* used by <machine/mmu.c> */
uint_t		nctxs;			/* total number of contexts */

int		cache;			/* describes system cache */

caddr_t		tsballoc_base;		/* base of bopalloced tsbs */
struct tsb_info *tsb_bases;		/* pool of small tsbs */
struct tsb_info *tsb512k_bases;		/* pool of 512K tsbs */


/*
 * Protect by global context mutex.
 */
static int	tsb_next;			/* next small tsb to use */
int		tsb_num;			/* number of small TSBs */
int		tsb512k_num;			/* number of large TSBs */

caddr_t		ktsb_base;			/* kernel tsb base address */
int		ktsb_szcode;			/* kernel tsb size code */
int		ktsb_sz;			/* kernel tsb size */
uint64_t	ktsb_reg;			/* kernel tsb register */

int		utsb_dtlb_ttenum;	/* index in TLB for utsb locked tte */
int		utsb_4m_disable;

#define	ALLOCATE_CTX_TSB(ctx)			\
{						\
	CTX_SET_TSBINDEX(ctx, tsb_next, 0);	\
	if (++tsb_next >= tsb_num)		\
		tsb_next = 0;			\
}

#define	ALLOCATE_CTX_TSB512K(ctx)				\
{								\
	int tsb512k_next = tsb_next / TSB_SIZE_FACTOR;		\
	tsb_next = (tsb512k_next + 1) * TSB_SIZE_FACTOR;	\
	if (tsb_next >= tsb_num)				\
		tsb_next = 0;					\
	CTX_SET_TSBINDEX(ctx, tsb512k_next, LTSB_FLAG);		\
}

#define	FREE_CTX_TSB(ctx)				\
{ /* there is nothing to do */ }

#define	TSB_RSS_FACTOR		(TSB_ENTRIES(TSB_MIN_SZCODE) * 0.75)

int	tsb_rss_factor	= (int)TSB_RSS_FACTOR;

/*
 * We use a larger tsb if
 * 1- our rss is larger than a certain factor OR
 *    this ctx is using large pages.
 * 2- we are not already using a large tsb.
 */
#define	SFMMU_SELECT_TSBSIZE(sfmmup)			\
	(tsb512k_num > 0 &&				\
	((sfmmup)->sfmmu_rss >= tsb_rss_factor ||	\
	sfmmutoctx(sfmmup)->c_flags & LTTES_FLAG))

#define	SFMMU_CHECK_TSBSIZE(sfmmup)			\
	(SFMMU_SELECT_TSBSIZE(sfmmup) &&		\
	!(sfmmutoctx(sfmmup)->c_flags & LTSB_FLAG))

/*
 * kstat data
 */
struct sfmmu_global_stat sfmmu_global_stat;

/*
 * Global data
 */
sfmmu_t *ksfmmup;			/* kernel's hat id */
struct ctx *kctx;			/* kernel's context */

#ifdef DEBUG
static void		chk_tte(tte_t *, tte_t *, tte_t *, struct hme_blk *);
#endif

/* sfmmu locking operations */
static kmutex_t *sfmmu_page_enter(machpage_t *);
static void	sfmmu_page_exit(kmutex_t *);
static int	sfmmu_mlist_held(machpage_t *);

/* sfmmu internal locking operations - accessed directly */
static kmutex_t	*sfmmu_mlist_enter(machpage_t *);
static void	sfmmu_mlist_exit(kmutex_t *);

/* array of mutexes protecting a page's mapping list and p_nrm field */
#define	MLIST_SIZE	mml_table_sz
#define	MLIST_HASH(pp)	&mml_table[(((uintptr_t)(pp))>>6) % mml_table_sz]

kmutex_t		*mml_table;
int			mml_table_sz;

#define	SPL_TABLE_SIZE	64
#define	SPL_SHIFT	6
#define	SPL_HASH(pp)	\
	&sfmmu_page_lock[(((uintptr_t)pp) >> SPL_SHIFT) & (SPL_TABLE_SIZE - 1)]

static	kmutex_t	sfmmu_page_lock[SPL_TABLE_SIZE];

/*
 * bit mask for managing vac conflicts on large pages.
 * bit 1 is for uncache flag.
 * bits 2 through min(num of cache colors + 1,31) are
 * for cache colors that have already been flushed.
 */
#define	CACHE_UNCACHE		1
#define	CACHE_NUM_COLOR		(shm_alignment >> MMU_PAGESHIFT)

#define	CACHE_VCOLOR_MASK(vcolor)	(2 << (vcolor & (CACHE_NUM_COLOR - 1)))

#define	CacheColor_IsFlushed(flag, vcolor) \
					((flag) & CACHE_VCOLOR_MASK(vcolor))

#define	CacheColor_SetFlushed(flag, vcolor) \
					((flag) |= CACHE_VCOLOR_MASK(vcolor))
/*
 * Flags passed to sfmmu_page_cache to flush page from vac or not.
 */
#define	CACHE_FLUSH	0
#define	CACHE_NO_FLUSH	1

/*
 * Flags passed to sfmmu_tlbcache_demap
 */
#define	FLUSH_NECESSARY_CPUS	0
#define	FLUSH_ALL_CPUS		1

#ifdef DEBUG

struct ctx_trace stolen_ctxs[TRSIZE];
struct ctx_trace *ctx_trace_first = &stolen_ctxs[0];
struct ctx_trace *ctx_trace_last = &stolen_ctxs[TRSIZE-1];
struct ctx_trace *ctx_trace_ptr = &stolen_ctxs[0];
uint_t	num_ctx_stolen = 0;

int	ism_debug = 0;

#endif /* DEBUG */


tte_t	hw_tte;

/*
 * Initialize the hardware address translation structures.
 * Called by hat_init() after the vm structures have been allocated
 * and mapped in.
 */
void
hat_init()
{
	register struct ctx	*ctx;
	register struct ctx	*cur_ctx = NULL;
	int 			i, j, one_pass_over;

	hat_lock_init();
	hat_kstat_init();

	/*
	 * HW bits only in a TTE
	 */
	hw_tte.tte_bit.v = 1;
	hw_tte.tte_bit.sz = 3;
	hw_tte.tte_bit.nfo = 1;
	hw_tte.tte_bit.ie = 1;
	hw_tte.tte_bit.pahi = 0x3FF;
	hw_tte.tte_bit.palo = 0x7FFFF;
	hw_tte.tte_bit.l = 1;
	hw_tte.tte_bit.cp = 1;
	hw_tte.tte_bit.cv = 1;
	hw_tte.tte_bit.e = 1;
	hw_tte.tte_bit.p = 1;
	hw_tte.tte_bit.w = 1;
	hw_tte.tte_bit.g = 1;
	/* Initialize the hash locks */
	for (i = 0; i < khmehash_num; i++) {
		mutex_init(&khme_hash[i].hmehash_mutex, NULL,
		    MUTEX_DEFAULT, NULL);
	}
	for (i = 0; i < uhmehash_num; i++) {
		mutex_init(&uhme_hash[i].hmehash_mutex, NULL,
		    MUTEX_DEFAULT, NULL);
	}
	khmehash_num--;		/* make sure counter starts from 0 */
	uhmehash_num--;		/* make sure counter starts from 0 */

	/*
	 * Initialize ctx structures and lock.
	 * We keep a free list of ctxs. That will be used to get/free ctxs.
	 * The first NUM_LOCKED_CTXS (0, .. NUM_LOCKED_CTXS-1)
	 * contexts are always not available. The rest of the contexts
	 * are put in a free list in the following fashion:
	 * Adjacent ctxs are not chained together - every (CTX_GAP)th one
	 * is chained next to each other. This results in a better hashing
	 * on ctxs at the begining. Later on the free list becomes random
	 * as processes exit randomly.
	 */
	mutex_init(&ctx_lock, NULL, MUTEX_DEFAULT, NULL);
	kctx = &ctxs[KCONTEXT];
	ctx = &ctxs[NUM_LOCKED_CTXS];
	ctxhand = ctxfree = ctx;		/* head of free list */
	one_pass_over = 0;
	for (j = 0; j < CTX_GAP; j++) {
		for (i = NUM_LOCKED_CTXS + j; i < nctxs; i = i + CTX_GAP) {
			if (one_pass_over) {
				cur_ctx->c_free = &ctxs[i];
				cur_ctx->c_refcnt = 0;
				one_pass_over = 0;
			}
			cur_ctx = &ctxs[i];
			cur_ctx->c_ismblkpa = (uint64_t)-1;
			if ((i + CTX_GAP) < nctxs) {
				cur_ctx->c_free = &ctxs[i + CTX_GAP];
				cur_ctx->c_refcnt = 0;
			}
		}
		one_pass_over = 1;
	}
	cur_ctx->c_free = NULL;		/* tail of free list */

	/*
	 * Intialize ism mapping list lock.
	 */
	mutex_init(&ism_mlist_lock, NULL, MUTEX_DEFAULT, NULL);

	sfmmuid_cache = kmem_cache_create("sfmmuid_cache", sizeof (sfmmu_t),
		0, sfmmu_idcache_constructor, sfmmu_idcache_destructor,
		NULL, NULL, NULL, 0);

	sfmmu8_cache = kmem_cache_create("sfmmu8_cache", HME8BLK_SZ,
		HMEBLK_ALIGN, sfmmu_hblkcache_constructor,
		sfmmu_hblkcache_destructor,
		sfmmu_hblkcache_reclaim, (void *)HME8BLK_SZ, NULL, 0);

	sfmmu1_cache = kmem_cache_create("sfmmu1_cache", HME1BLK_SZ,
		HMEBLK_ALIGN, sfmmu_hblkcache_constructor,
		sfmmu_hblkcache_destructor,
		NULL, (void *)HME1BLK_SZ, NULL, 0);

	ism_blk_cache = kmem_cache_create("ism_blk_cache",
		sizeof (ism_blk_t), ecache_linesize, NULL, NULL,
		NULL, NULL, NULL, 0);

	ism_ment_cache = kmem_cache_create("ism_ment_cache",
		sizeof (ism_ment_t), 0, NULL, NULL,
		NULL, NULL, NULL, 0);

	/*
	 * We grab the first hat for the kernel,
	 */
	AS_LOCK_ENTER(&kas, &kas.a_lock, RW_WRITER);
	kas.a_hat = hat_alloc(&kas);
	AS_LOCK_EXIT(&kas, &kas.a_lock);

	/*
	 * The big page VAC handling code assumes VAC
	 * will not be bigger than the smallest big
	 * page- which is 64K.
	 */
	if (TTEPAGES(TTE64K) < CACHE_NUM_COLOR) {
		cmn_err(CE_PANIC, "VAC too big!\n");
	}
}

/*
 * Initialize locking for the hat layer, called early during boot.
 */
static void
hat_lock_init()
{
	int i;

	/*
	 * initialize the array of mutexes protecting a page's mapping
	 * list and p_nrm field.
	 */
	for (i = 0; i < MLIST_SIZE; i++)
		mutex_init(&mml_table[i], NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Allocate initial hmeblks. would be nice to do this as part of
 * hat_init(), but bootops were still being used for allocation then.
 */
void
sfmmu_hblk_init(void)
{
	struct hme_blk *first, *hmeblkp;
	int i, nhblks;

	/*
	 * record the # of hmeblks used so far in sfmmu stat.
	 * we can tune the nucleus hmeblk allocation based on this value.
	 */
	SFMMU_STAT_SET(sf_hblk8_startup_use,
	    (uint_t)(hblk8_allocated - hblk8_avail));

	/*
	 * Make sure we allocate enough hme_blks to map the entire kernel,
	 * or the entire physical memory, which ever is lower. Also add
	 * some margin to be safe.
	 */
	hblk8_prealloc_count = MIN(physmem, mmu_btop(SYSLIMIT -
	    KERNELBASE)) / (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT);
	hblk8_prealloc_count += hblk8_prealloc_count >> 1;

	/*
	 * we have allocated (nucleus) hmeblks earlier, subtract them.
	 */
	nhblks = hblk8_prealloc_count - hblk8_allocated;

	while (nhblks > 0) {
		first = kmem_cache_alloc(sfmmu8_cache, KM_SLEEP);
		SFMMU_STAT(sf_hblk8_dalloc);
		for (i = 1, hmeblkp = first; i < nhblks && hmeblkp; i++) {
			hmeblkp->hblk_next = kmem_cache_alloc(
				sfmmu8_cache, KM_SLEEP);
			SFMMU_STAT(sf_hblk8_dalloc);
			hmeblkp = hmeblkp->hblk_next;

			if (hblk8_avail <= HME8_TRHOLD)
				break;	/* add allocated hblks in freelist */
		}

		if (hmeblkp == NULL) {
			panic("Cannot allocate initial hmeblks");
		}
		hmeblkp->hblk_next = NULL;

		HBLK8_FLIST_LOCK();
		if (hblk8_avail == 0) {
			hblk8_flist = first;
		} else {
			hblk8_flist_t->hblk_next = first;
		}
		hblk8_flist_t = hmeblkp;
		hblk8_avail += i;
		hblk8_allocated += i;
		HBLK8_FLIST_UNLOCK();
		nhblks -= i;
	}
}

/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 */
struct hat *
hat_alloc(struct as *as)
{
	sfmmu_t *sfmmup;
	struct ctx *ctx;
	extern uint_t get_color_start(struct as *);

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	sfmmup = kmem_cache_alloc(sfmmuid_cache, KM_SLEEP);
	sfmmup->sfmmu_as = as;

	if (as == &kas) {			/* XXX - 1 time only */
		ctx = kctx;
		ksfmmup = sfmmup;
		sfmmup->sfmmu_cnum = ctxtoctxnum(ctx);
		ctx->c_sfmmu = sfmmup;
		sfmmup->sfmmu_clrstart = 0;
	} else {

		/*
		 * Just set to invalid ctx. When it faults, it will
		 * get a valid ctx. This would avoid the situation
		 * where we get a ctx, but it gets stolen and then
		 * we fault when we try to run and so have to get
		 * another ctx.
		 */
		sfmmup->sfmmu_cnum = INVALID_CONTEXT;
		/* initialize original physical page coloring bin */
		sfmmup->sfmmu_clrstart = get_color_start(as);
	}
	sfmmup->sfmmu_rss = 0;
	sfmmup->sfmmu_lttecnt = 0;
	sfmmup->sfmmu_iblk = NULL;
	sfmmup->sfmmu_ismhat = 0;
	if (sfmmup == ksfmmup)
		CPUSET_ALL(sfmmup->sfmmu_cpusran);
	else
		CPUSET_ZERO(sfmmup->sfmmu_cpusran);
	sfmmup->sfmmu_free = 0;
	sfmmup->sfmmu_rmstat = 0;
	sfmmup->sfmmu_clrbin = sfmmup->sfmmu_clrstart;
	return (sfmmup);
}

/*
 * Hat_setup, makes an address space context the current active one.
 * In sfmmu this translates to setting the secondary context with the
 * corresponding context.
 */
void
hat_setup(struct hat *sfmmup, int allocflag)
{
	struct ctx *ctx;
	uint_t ctx_num;

#ifdef lint
	allocflag = allocflag;			/* allocflag is not used */
#endif /* lint */

	/*
	 * Make sure that we have a valid ctx and it doesn't get stolen
	 * after this point.
	 */
	if (sfmmup != ksfmmup)
		sfmmu_disallow_ctx_steal(sfmmup);

	ctx = sfmmutoctx(sfmmup);
	kpreempt_disable();
	CPUSET_ADD(sfmmup->sfmmu_cpusran, CPU->cpu_id);
	ctx_num = ctxtoctxnum(ctx);
	ASSERT(sfmmup == ctx->c_sfmmu);

	/* curiosity check - delete someday */
	if (sfmmup == ksfmmup) {
		panic("hat_setup called with kas");
	}

	ASSERT(ctx_num);
	sfmmu_setctx_sec(ctx_num);

	kpreempt_enable();

	/*
	 * Allow ctx to be stolen.
	 */
	if (sfmmup != ksfmmup)
		sfmmu_allow_ctx_steal(sfmmup);

	curthread->t_mmuctx = 0;	/* XXX not used - use it resume */
}

/*
 * Free all the translation resources for the specified address space.
 * Called from as_free when an address space is being destroyed.
 */
void
hat_free_start(struct hat *sfmmup)
{
	ASSERT(AS_WRITE_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT(sfmmup != ksfmmup);

	sfmmup->sfmmu_free = 1;
}

void
hat_free_end(struct hat *sfmmup)
{
	if (sfmmup->sfmmu_ismhat) {
		sfmmup->sfmmu_rss = 0;
		sfmmup->sfmmu_lttecnt = 0;
	} else {
		/* EMPTY */
		ASSERT(sfmmup->sfmmu_lttecnt == 0);
		ASSERT(sfmmup->sfmmu_rss == 0);
	}

	if (sfmmup->sfmmu_rmstat) {
		hat_freestat(sfmmup->sfmmu_as, NULL);
	}
	sfmmu_tlb_ctx_demap(sfmmup);
	xt_sync(sfmmup->sfmmu_cpusran);
	sfmmu_free_ctx(sfmmup, sfmmutoctx(sfmmup));
	sfmmu_free_sfmmu(sfmmup);

	kmem_cache_free(sfmmuid_cache, sfmmup);
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 */
/* ARGSUSED */
void
hat_swapin(struct hat *hat)
{
}

/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 * Also, free up the ctx that this process was using.
 */
void
hat_swapout(struct hat *sfmmup)
{
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	struct hme_blk *pr_hblk = NULL;
	struct hme_blk *nx_hblk;
	struct ctx *ctx;
	int i;
	uint64_t hblkpa, prevpa, nx_pa;

	/*
	 * There is no way to go from an as to all its translations in sfmmu.
	 * Here is one of the times when we take the big hit and traverse
	 * the hash looking for hme_blks to free up.  Not only do we free up
	 * this as hme_blks but all those that are free.  We are obviously
	 * swaping because we need memory so let's free up as much
	 * as we can.
	 */
	ASSERT(sfmmup != KHATID);
	for (i = 0; i <= UHMEHASH_SZ; i++) {
		hmebp = &uhme_hash[i];

		SFMMU_HASH_LOCK(hmebp);
		hmeblkp = hmebp->hmeblkp;
		hblkpa = hmebp->hmeh_nextpa;
		prevpa = 0;
		pr_hblk = NULL;
		while (hmeblkp) {
			if ((hmeblkp->hblk_tag.htag_id == sfmmup) &&
			    !hmeblkp->hblk_shw_bit && !hmeblkp->hblk_lckcnt) {
				(void) sfmmu_hblk_unload(sfmmup, hmeblkp,
					(caddr_t)get_hblk_base(hmeblkp),
					get_hblk_endaddr(hmeblkp), HAT_UNLOAD);
			}
			nx_hblk = hmeblkp->hblk_next;
			nx_pa = hmeblkp->hblk_nextpa;
			if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
				ASSERT(!hmeblkp->hblk_lckcnt);
				sfmmu_hblk_hash_rm(hmebp, hmeblkp,
					prevpa, pr_hblk);
				sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			} else {
				pr_hblk = hmeblkp;
				prevpa = hblkpa;
			}
			hmeblkp = nx_hblk;
			hblkpa = nx_pa;
		}
		SFMMU_HASH_UNLOCK(hmebp);
	}

	/*
	 * Now free up the ctx so that others can reuse it.
	 */
	mutex_enter(&ctx_lock);
	ctx = sfmmutoctx(sfmmup);

	if (sfmmup->sfmmu_cnum != INVALID_CONTEXT &&
		rwlock_hword_enter(&ctx->c_refcnt, WRITER_LOCK) == 0) {
		sfmmu_reuse_ctx(ctx, sfmmup);
		/*
		 * Put ctx back to the free list.
		 */
		ctx->c_free = ctxfree;
		ctxfree = ctx;
		rwlock_hword_exit(&ctx->c_refcnt, WRITER_LOCK);
	}
	mutex_exit(&ctx_lock);
}

/*
 * Duplicate the translations of an as into another newas
 */
/* ARGSUSED */
int
hat_dup(struct hat *hat, struct hat *newhat, caddr_t addr, size_t len,
	uint_t flag)
{
	ASSERT((flag == 0) || (flag == HAT_DUP_ALL) || (flag == HAT_DUP_COW));

	if (flag == HAT_DUP_COW) {
		panic("hat_dup: HAT_DUP_COW not supported");
	}
	return (0);
}

/*
 * Set up addr to map to page pp with protection prot.
 * As an optimization we also load the TSB with the
 * corresponding tte but it is no big deal if  the tte gets kicked out.
 */
void
hat_memload(struct hat *hat, caddr_t addr, struct page *gen_pp,
	uint_t attr, uint_t flags)
{
	tte_t tte;
	machpage_t *pp = PP2MACHPP(gen_pp);

	ASSERT(hat != NULL);
	ASSERT((hat == ksfmmup) ||
		AS_LOCK_HELD(hat->sfmmu_as, &hat->sfmmu_as->a_lock));
	ASSERT(PAGE_LOCKED(gen_pp));
	ASSERT(!((uintptr_t)addr & MMU_PAGEOFFSET));
	ASSERT(!(flags & ~SFMMU_LOAD_ALLFLAG));
	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));

	if (PP_ISFREE(gen_pp)) {
		panic("hat_memload: loading a mapping to free page %p",
		    (void *)pp);
	}
	if (flags & ~SFMMU_LOAD_ALLFLAG)
		cmn_err(CE_NOTE, "hat_memload: unsupported flags %d",
		    flags & ~SFMMU_LOAD_ALLFLAG);

	if (hat->sfmmu_rmstat)
		hat_resvstat(MMU_PAGESIZE, hat->sfmmu_as, addr);

#if defined(__sparcv9) && defined(SF_ERRATA_57)
	if ((hat != ksfmmup) && AS_TYPE_64BIT(hat->sfmmu_as) &&
	    (addr < errata57_limit) && (attr & PROT_EXEC) &&
	    !(flags & HAT_LOAD_SHARE)) {
		cmn_err(CE_WARN, "hat_memload: illegal attempt to make user "
		    " page executable");
		attr &= ~PROT_EXEC;
	}
#endif

	sfmmu_memtte(&tte, (pfn_t)pp->p_pagenum, attr, TTE8K);
	(void) sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
}

/*
 * hat_devload can be called to map real memory (e.g.
 * /dev/kmem) and even though hat_devload will determine pf is
 * for memory, it will be unable to get a shared lock on the
 * page (because someone else has it exclusively) and will
 * pass dp = NULL.  If tteload doesn't get a non-NULL
 * page pointer it can't cache memory.
 */
void
hat_devload(struct hat *hat, caddr_t addr, size_t len, pfn_t pfn,
	uint_t attr, int flags)
{
	tte_t tte;
	struct machpage *pp = NULL;
	int use_lgpg = 0;

	ASSERT(hat != NULL);
	ASSERT(!(flags & ~SFMMU_LOAD_ALLFLAG));
	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));
	ASSERT((hat == ksfmmup) ||
		AS_LOCK_HELD(hat->sfmmu_as, &hat->sfmmu_as->a_lock));
	if (len == 0)
		panic("hat_devload: zero len");
	if (flags & ~SFMMU_LOAD_ALLFLAG)
		cmn_err(CE_NOTE, "hat_devload: unsupported flags %d",
		    flags & ~SFMMU_LOAD_ALLFLAG);

#if defined(__sparcv9) && defined(SF_ERRATA_57)
	if ((hat != ksfmmup) && AS_TYPE_64BIT(hat->sfmmu_as) &&
	    (addr < errata57_limit) && (attr & PROT_EXEC) &&
	    !(flags & HAT_LOAD_SHARE)) {
		cmn_err(CE_WARN, "hat_devload: illegal attempt to make user "
		    " page executable");
		attr &= ~PROT_EXEC;
	}
#endif

	/*
	 * If it's a memory page find its pp
	 */
	if (!(flags & HAT_LOAD_NOCONSIST) && pf_is_memory(pfn)) {
		pp = (machpage_t *)page_numtopp_nolock(pfn);
		if (pp == NULL) {
			flags |= HAT_LOAD_NOCONSIST;
		} else {
			if (PP_ISFREE((struct page *)pp)) {
				panic("hat_memload: loading "
				    "a mapping to free page %p",
				    (void *)pp);
			}
			ASSERT(PAGE_LOCKED((struct page *)pp));
			ASSERT(len == MMU_PAGESIZE);
		}
	}

	if (hat->sfmmu_rmstat)
		hat_resvstat(len, hat->sfmmu_as, addr);

	if (flags & HAT_LOAD_NOCONSIST) {
		attr |= SFMMU_UNCACHEVTTE;
		use_lgpg = 1;
	}
	if (!pf_is_memory(pfn)) {
		attr |= SFMMU_UNCACHEPTTE | HAT_NOSYNC;
		use_lgpg = 1;
		switch (attr & HAT_ORDER_MASK) {
			case HAT_STRICTORDER:
			case HAT_UNORDERED_OK:
				/*
				 * we set the side effect bit for all non
				 * memory mappings unless merging is ok
				 */
				attr |= SFMMU_SIDEFFECT;
				break;
			case HAT_MERGING_OK:
			case HAT_LOADCACHING_OK:
			case HAT_STORECACHING_OK:
				break;
			default:
				panic("hat_devload: bad attr");
				break;
		}
	}
	while (len) {
		if (!use_lgpg) {
			sfmmu_memtte(&tte, pfn, attr, TTE8K);
			(void) sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE;
			addr += MMU_PAGESIZE;
			pfn++;
			continue;
		}
		/*
		 *  try to use large pages, check va/pa alignments
		 */
		if ((len >= MMU_PAGESIZE4M) &&
		    !((uintptr_t)addr & MMU_PAGEOFFSET4M) &&
		    !(mmu_ptob(pfn) & MMU_PAGEOFFSET4M)) {
			sfmmu_memtte(&tte, pfn, attr, TTE4M);
			(void) sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE4M;
			addr += MMU_PAGESIZE4M;
			pfn += MMU_PAGESIZE4M / MMU_PAGESIZE;
		} else if ((len >= MMU_PAGESIZE512K) &&
		    !((uintptr_t)addr & MMU_PAGEOFFSET512K) &&
		    !(mmu_ptob(pfn) & MMU_PAGEOFFSET512K)) {
			sfmmu_memtte(&tte, pfn, attr, TTE512K);
			(void) sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE512K;
			addr += MMU_PAGESIZE512K;
			pfn += MMU_PAGESIZE512K / MMU_PAGESIZE;
		} else if ((len >= MMU_PAGESIZE64K) &&
		    !((uintptr_t)addr & MMU_PAGEOFFSET64K) &&
		    !(mmu_ptob(pfn) & MMU_PAGEOFFSET64K)) {
			sfmmu_memtte(&tte, pfn, attr, TTE64K);
			(void) sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE64K;
			addr += MMU_PAGESIZE64K;
			pfn += MMU_PAGESIZE64K / MMU_PAGESIZE;
		} else {
			sfmmu_memtte(&tte, pfn, attr, TTE8K);
			(void) sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE;
			addr += MMU_PAGESIZE;
			pfn++;
		}
	}
}

/*
 * Map the largest extend possible out of the page array. The array may NOT
 * be in order.  The largest possible mapping a page can have
 * is specified in the p_cons field.  The p_cons field
 * cannot change as long as there any mappings (large or small)
 * to any of the pages that make up the large page. (ie. any
 * promotion/demotion of page size is not up to the hat but up to
 * the page free list manager).  The array
 * should consist of properly aligned contigous pages that are
 * part of a big page for a large mapping to be created.
 */
void
hat_memload_array(struct hat *hat, caddr_t addr, size_t len,
	struct page **gen_pps, uint_t attr, uint_t flags)
{
	int  ttesz;
	size_t mapsz;
	pgcnt_t	numpg, npgs;
	tte_t tte;
	machpage_t *pp;
	machpage_t **pps = (machpage_t **)gen_pps;

	ASSERT(!((uintptr_t)addr & MMU_PAGEOFFSET));

	if (hat->sfmmu_rmstat)
		hat_resvstat(len, hat->sfmmu_as, addr);

#if defined(__sparcv9) && defined(SF_ERRATA_57)
	if ((hat != ksfmmup) && AS_TYPE_64BIT(hat->sfmmu_as) &&
	    (addr < errata57_limit) && (attr & PROT_EXEC) &&
	    !(flags & HAT_LOAD_SHARE)) {
		cmn_err(CE_WARN, "hat_memload_array: illegal attempt to make "
		    "user page executable");
		attr &= ~PROT_EXEC;
	}
#endif

	/* Get number of pages */
	npgs = len >> MMU_PAGESHIFT;

	if (npgs < NHMENTS || disable_large_pages == LARGE_PAGES_OFF) {
		sfmmu_memload_batchsmall(hat, addr, pps, attr, flags, npgs);
		return;
	}

	while (npgs >= NHMENTS) {
		pp = *pps;
		for (ttesz = pp->p_cons; ttesz != TTE8K; ttesz--) {
			/*
			 * Check if this page size is disabled.
			 */
			if (disable_large_pages & (1 << ttesz))
				continue;

			numpg = TTEPAGES(ttesz);
			mapsz = numpg << MMU_PAGESHIFT;
			if ((npgs >= numpg) &&
			    IS_P2ALIGNED(addr, mapsz) &&
			    IS_P2ALIGNED(pp->p_pagenum, numpg)) {
				/*
				 * At this point we have enough pages and
				 * we know the virtual address and the pfn
				 * are properly aligned.  We still need
				 * to check for physical contiguity but since
				 * it is very likely that this is the case
				 * we will assume they are so and undo
				 * the request if necessary.  It would
				 * be great if we could get a hint flag
				 * like HAT_CONTIG which would tell us
				 * the pages are contigous for sure.
				 */
				sfmmu_memtte(&tte, (*pps)->p_pagenum,
					attr, ttesz);
				if (!sfmmu_tteload_array(hat, &tte, addr,
				    pps, flags)) {
					break;
				}
			}
		}
		if (ttesz == TTE8K) {
			/*
			 * We were not able to map array using a large page
			 * batch a hmeblk or fraction at a time.
			 */
			numpg = ((uintptr_t)addr >> MMU_PAGESHIFT)
				& (NHMENTS-1);
			numpg = NHMENTS - numpg;
			ASSERT(numpg <= npgs);
			mapsz = numpg * MMU_PAGESIZE;
			sfmmu_memload_batchsmall(hat, addr, pps, attr, flags,
							numpg);
		}
		addr += mapsz;
		npgs -= numpg;
		pps += numpg;
	}

	if (npgs) {
		sfmmu_memload_batchsmall(hat, addr, pps, attr, flags, npgs);
	}
}

/*
 * Function tries to batch 8K pages into the same hme blk.
 */
static void
sfmmu_memload_batchsmall(struct hat *hat, caddr_t vaddr, machpage_t **pps,
		    uint_t attr, uint_t flags, pgcnt_t npgs)
{
	tte_t	tte;
	machpage_t *pp;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	int	index;

	while (npgs) {
		/*
		 * Acquire the hash bucket.
		 */
		hmebp = sfmmu_tteload_acquire_hashbucket(hat, vaddr, TTE8K);
		ASSERT(hmebp);

		/*
		 * Find the hment block.
		 */
		hmeblkp = sfmmu_tteload_find_hmeblk(hat, hmebp, vaddr, TTE8K);
		ASSERT(hmeblkp);

		do {
			/*
			 * Make the tte.
			 */
			pp = *pps;
			sfmmu_memtte(&tte, pp->p_pagenum, attr, TTE8K);

			/*
			 * Add the translation.
			 */
			(void) sfmmu_tteload_addentry(hat, hmeblkp, &tte,
					vaddr, pps, flags);

			/*
			 * Goto next page.
			 */
			pps++;
			npgs--;

			/*
			 * Goto next address.
			 */
			vaddr += MMU_PAGESIZE;

			/*
			 * Don't crossover into a different hmentblk.
			 */
			index = (int)(((uintptr_t)vaddr >> MMU_PAGESHIFT) &
			    (NHMENTS-1));

		} while (index != 0 && npgs != 0);

		/*
		 * Release the hash bucket.
		 */

		sfmmu_tteload_release_hashbucket(hmebp);
	}
}

/*
 * Construct a tte for a page:
 *
 * tte_valid = 1
 * tte_size = size
 * tte_nfo = attr & HAT_NOFAULT
 * tte_ie = attr & HAT_STRUCTURE_LE
 * tte_hmenum = hmenum
 * tte_pahi = pp->p_pagenum >> TTE_PASHIFT;
 * tte_palo = pp->p_pagenum & TTE_PALOMASK;
 * tte_ref = 1 (optimization)
 * tte_wr_perm = attr & PROT_WRITE;
 * tte_no_sync = attr & HAT_NOSYNC
 * tte_lock = attr & SFMMU_LOCKTTE
 * tte_cp = !(attr & SFMMU_UNCACHEPTTE)
 * tte_cv = !(attr & SFMMU_UNCACHEVTTE)
 * tte_e = attr & SFMMU_SIDEFFECT
 * tte_priv = !(attr & PROT_USER)
 * tte_hwwr = if nosync is set and it is writable we set the mod bit (opt)
 * tte_glb = 0
 */
void
sfmmu_memtte(tte_t *ttep, pfn_t pfn, uint_t attr, int tte_sz)
{
	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));

	ttep->tte_inthi =
	    MAKE_TTE_INTHI(tte_sz, 0, pfn, attr);   /* hmenum = 0 */

	ttep->tte_intlo = MAKE_TTE_INTLO(pfn, attr);
	if (TTE_IS_NOSYNC(ttep)) {
		TTE_SET_REF(ttep);
		if (TTE_IS_WRITABLE(ttep)) {
			TTE_SET_MOD(ttep);
		}
	}
	if (TTE_IS_NFO(ttep) && TTE_IS_EXECUTABLE(ttep)) {
		panic("sfmmu_memtte: can't set both NFO and EXEC bits");
	}
}

/*
 * This function will add a translation to the hme_blk and allocate the
 * hme_blk if one does not exist.
 * If a page structure is specified then it will add the
 * corresponding hment to the mapping list.
 * It will also update the hmenum field for the tte.
 */
void
sfmmu_tteload(struct hat *sfmmup, tte_t *ttep, caddr_t vaddr, machpage_t *pp,
	uint_t flags)
{
	(void) sfmmu_tteload_array(sfmmup, ttep, vaddr, &pp, flags);
}

/*
 * This function will add a translation to the hme_blk and allocate the
 * hme_blk if one does not exist.
 * If a page structure is specified then it will add the
 * corresponding hment to the mapping list.
 * It will also update the hmenum field for the tte.
 * Furthermore, it attempts to create a large page translation
 * for <addr,hat> at page array pps.  It assumes addr and first
 * pp is correctly aligned.  It returns 0 if successful and 1 otherwise.
 */
static int
sfmmu_tteload_array(sfmmu_t *sfmmup, tte_t *ttep, caddr_t vaddr,
	machpage_t **pps, uint_t flags)
{
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	int 	ret;
	uint_t	size;

	/*
	 * Get mapping size.
	 */
	size = ttep->tte_size;
	ASSERT(!((uintptr_t)vaddr & TTE_PAGE_OFFSET(size)));

	/*
	 * Acquire the hash bucket.
	 */
	hmebp = sfmmu_tteload_acquire_hashbucket(sfmmup, vaddr, size);
	ASSERT(hmebp);

	/*
	 * Find the hment block.
	 */
	hmeblkp = sfmmu_tteload_find_hmeblk(sfmmup, hmebp, vaddr, size);
	ASSERT(hmeblkp);

	/*
	 * Add the translation.
	 */
	ret = sfmmu_tteload_addentry(sfmmup, hmeblkp, ttep, vaddr, pps, flags);

	/*
	 * Release the hash bucket.
	 */

	sfmmu_tteload_release_hashbucket(hmebp);

	return (ret);
}

/*
 * Function locks and returns a pointer to the hash bucket for vaddr and size.
 */
static struct hmehash_bucket *
sfmmu_tteload_acquire_hashbucket(sfmmu_t *sfmmup, caddr_t vaddr, int size)
{
	struct hmehash_bucket *hmebp;
	int hmeshift;

	hmeshift = HME_HASH_SHIFT(size);

	hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

	SFMMU_HASH_LOCK(hmebp);

	return (hmebp);
}

/*
 * Function returns a pointer to an hmeblk in the hash bucket, hmebp. If the
 * hmeblk doesn't exists for the [sfmmup, vaddr & size] signature, a hmeblk is
 * allocated.
 */
static struct hme_blk *
sfmmu_tteload_find_hmeblk(sfmmu_t *sfmmup, struct hmehash_bucket *hmebp,
	caddr_t vaddr, uint_t size)
{
	hmeblk_tag hblktag;
	int hmeshift;
	struct hme_blk *hmeblkp, *pr_hblk;
	uint64_t hblkpa, prevpa;

	hblktag.htag_id = sfmmup;
	hmeshift = HME_HASH_SHIFT(size);
	hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
	hblktag.htag_rehash = HME_HASH_REHASH(size);

ttearray_realloc:

	HME_HASH_SEARCH_PREV(hmebp, hblktag, hmeblkp, hblkpa, pr_hblk, prevpa);
	if (hmeblkp == NULL) {
		hmeblkp = sfmmu_hblk_alloc(sfmmup, vaddr, hmebp, size, hblktag);
	} else {
		/*
		 * It is possible for 8k and 64k hblks to collide since they
		 * have the same rehash value. This is because we
		 * lazily free hblks and 8K/64K blks could be lingering.
		 * If we find size mismatch we free the block and & try again.
		 */
		if (get_hblk_ttesz(hmeblkp) != size) {
			ASSERT(!hmeblkp->hblk_vcnt);
			ASSERT(!hmeblkp->hblk_hmecnt);
			sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa, pr_hblk);
			sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			goto ttearray_realloc;
		}
		if (hmeblkp->hblk_shw_bit) {
			/*
			 * if the hblk was previously used as a shadow hblk then
			 * we will change it to a normal hblk
			 */
			if (hmeblkp->hblk_shw_mask) {
				sfmmu_shadow_hcleanup(sfmmup, hmeblkp, hmebp);
				ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
				goto ttearray_realloc;
			} else {
				hmeblkp->hblk_shw_bit = 0;
			}
		}
		SFMMU_STAT(sf_hblk_hit);
	}

	ASSERT(get_hblk_ttesz(hmeblkp) == size);
	ASSERT(!hmeblkp->hblk_shw_bit);

	return (hmeblkp);
}

/*
 * Function adds a tte entry into the hmeblk. It returns 0 if successful and 1
 * otherwise.
 */
static int
sfmmu_tteload_addentry(sfmmu_t *sfmmup, struct hme_blk *hmeblkp, tte_t *ttep,
	caddr_t vaddr, machpage_t **pps, uint_t flags)
{
	machpage_t *pp = *pps;
	int hmenum, size;
	int remap;
	tte_t tteold, flush_tte;
#ifdef DEBUG
	tte_t orig_old;
#endif DEBUG
	struct sf_hment *sfhme;
	kmutex_t *pml, *pmtx;
	struct ctx *ctx;

	/*
	 * remove this panic when we decide to let user virtual address
	 * space be >= USERLIMIT.
	 */
	if (!TTE_IS_PRIVILEGED(ttep) && vaddr >= (caddr_t)USERLIMIT)
		panic("user addr %p in kernel space", vaddr);
	if (TTE_IS_GLOBAL(ttep))
		panic("sfmmu_tteload: creating global tte");

#ifdef	DEBUG
	if (pf_is_memory(sfmmu_ttetopfn(ttep, vaddr)) &&
	    !TTE_IS_PCACHEABLE(ttep))
		panic("sfmmu_tteload: non cacheable memory tte");
#endif /* DEBUG */


	if (flags & HAT_LOAD_SHARE) {
		/*
		 * Don't load TSB for dummy as in ISM
		 */
		flags |= SFMMU_NO_TSBLOAD;
	}

	size = ttep->tte_size;
	switch (size) {
	case (TTE8K):
		SFMMU_STAT(sf_tteload8k);
		break;
	case (TTE64K):
		SFMMU_STAT(sf_tteload64k);
		break;
	case (TTE512K):
		SFMMU_STAT(sf_tteload512k);
		break;
	case (TTE4M):
		SFMMU_STAT(sf_tteload4m);
		break;
	}

	ASSERT(!((uintptr_t)vaddr & TTE_PAGE_OFFSET(size)));

	sfhme = sfmmu_hblktohme(hmeblkp, vaddr, &hmenum);

	/*
	 * Need to grab mlist lock here so that pageunload
	 * will not change tte behind us.
	 */
	if (pp) {
		pml = sfmmu_mlist_enter(pp);
	}

	sfmmu_copytte(&sfhme->hme_tte, &tteold);
	/*
	 * Look for corresponding hment and if valid verify
	 * pfns are equal.
	 */
	remap = TTE_IS_VALID(&tteold);
	if (remap) {
		pfn_t	new_pfn, old_pfn;

		old_pfn = TTE_TO_PFN(vaddr, &tteold);
		new_pfn = TTE_TO_PFN(vaddr, ttep);

		if (flags & HAT_LOAD_REMAP) {
			/* make sure we are remapping same type of pages */
			if (pf_is_memory(old_pfn) != pf_is_memory(new_pfn)) {
				panic("sfmmu_tteload - tte remap io<->memory");
			}
			if (old_pfn != new_pfn &&
			    (pp != NULL || sfhme->hme_page != NULL)) {
				panic("sfmmu_tteload - tte remap pp != NULL");
			}
		} else if (old_pfn != new_pfn) {
			panic("sfmmu_tteload - tte remap, hmeblkp 0x%p",
			    (void *)hmeblkp);
		}
		ASSERT(tteold.tte_size == ttep->tte_size);
	}

	if (pp) {
		if (size == TTE8K) {
			/*
			 * Handle VAC consistency
			 */
			if (!remap && (cache & CACHE_VAC) && !PP_ISNC(pp)) {
				sfmmu_vac_conflict(sfmmup, vaddr, pp);
			}

			if (TTE_IS_WRITABLE(ttep) && PP_ISRO(pp)) {
				pmtx = sfmmu_page_enter(pp);
				PP_CLRRO(pp);
				sfmmu_page_exit(pmtx);
			} else if (!PP_ISMAPPED(pp) &&
			    (!TTE_IS_WRITABLE(ttep)) && !(PP_ISMOD(pp))) {
				pmtx = sfmmu_page_enter(pp);
				if (!(PP_ISMOD(pp))) {
					PP_SETRO(pp);
				}
				sfmmu_page_exit(pmtx);
			}

		} else if (sfmmu_pagearray_setup(vaddr, pps, ttep, remap)) {
			/*
			 * sfmmu_pagearray_setup failed so return
			 */
			sfmmu_mlist_exit(pml);
			return (1);
		}
	}

	/*
	 * Make sure hment is not on a mapping list.
	 */
	ASSERT(remap || (sfhme->hme_page == NULL));

	/* if it is not a remap then hme->next better be NULL */
	ASSERT((!remap) ? sfhme->hme_next == NULL : 1);

	if (flags & HAT_LOAD_LOCK) {
		if (((int)hmeblkp->hblk_lckcnt + 1) >= MAX_HBLK_LCKCNT) {
			panic("too high lckcnt-hmeblk %p",
			    (void *)hmeblkp);
		}
		atomic_add_16(&hmeblkp->hblk_lckcnt, 1);

		HBLK_STACK_TRACE(hmeblkp, HBLK_LOCK);
	}

	if (pp && PP_ISNC(pp)) {
		/*
		 * If the physical page is marked to be uncacheable, like
		 * by a vac conflict, make sure the new mapping is also
		 * uncacheable.
		 */
		TTE_CLR_VCACHEABLE(ttep);
		ASSERT(PP_GET_VCOLOR(pp) == NO_VCOLOR);
	}
	ttep->tte_hmenum = hmenum;

#ifdef DEBUG
	orig_old = tteold;
#endif DEBUG

	while (sfmmu_modifytte_try(&tteold, ttep, &sfhme->hme_tte) < 0) {
		;
#ifdef DEBUG
		chk_tte(&orig_old, &tteold, ttep, hmeblkp);
#endif DEBUG
	}

	if (!TTE_IS_VALID(&tteold)) {
		atomic_add_16(&hmeblkp->hblk_vcnt, 1);
		if (!(flags & HAT_RELOAD_SHARE)) {
			/*
			 * We will not increment the rss and lttecnt
			 * on reloads of shared mappings, i.e. ism
			 * mappings.
			 */
			if (size == TTE8K) {
				atomic_add_32(&sfmmup->sfmmu_rss, 1);
			} else {

				/*
				 * Make sure that we have a valid ctx and
				 * it doesn't get stolen after this point.
				 */
				if (sfmmup != ksfmmup)
					sfmmu_disallow_ctx_steal(sfmmup);

				ctx = sfmmutoctx(sfmmup);
				atomic_add_32(&sfmmup->sfmmu_lttecnt, 1);
				CTX_SET_LTTES(ctx);
				/*
				 * Now we can allow our ctx to be stolen.
				 */
				if (sfmmup != ksfmmup)
					sfmmu_allow_ctx_steal(sfmmup);
			}
		}
	}
	ASSERT(TTE_IS_VALID(&sfhme->hme_tte));

	flush_tte.tte_intlo = (tteold.tte_intlo ^ ttep->tte_intlo) &
	    hw_tte.tte_intlo;
	flush_tte.tte_inthi = (tteold.tte_inthi ^ ttep->tte_inthi) &
	    hw_tte.tte_inthi;

	if (remap && (flush_tte.tte_inthi || flush_tte.tte_intlo)) {
		/*
		 * If remap and new tte differs from old tte we need
		 * to sync the mod bit and flush tlb/tsb.  We don't
		 * need to sync ref bit because we currently always set
		 * ref bit in tteload.
		 */
		ASSERT(TTE_IS_REF(ttep));
		if (TTE_IS_MOD(&tteold)) {
			sfmmu_ttesync(sfmmup, vaddr, &tteold, pp);
		}
		sfmmu_tlb_demap(vaddr, sfmmup, hmeblkp, 0, 0);
		xt_sync(sfmmup->sfmmu_cpusran);
	}

	if (size == TTE8K && ((flags & SFMMU_NO_TSBLOAD) == 0)) {
		if (sfmmup != ksfmmup) {
			/*
			 * Do we need to expand the tsb size?
			 */
			if (SFMMU_CHECK_TSBSIZE(sfmmup)) {
				sfmmu_expand_tsbsize(sfmmup);
			}

			/*
			 * Make sure that we have a valid ctx and
			 * it doesn't get stolen after this point.
			 */
			sfmmu_disallow_ctx_steal(sfmmup);
		}

		sfmmu_load_tsb(vaddr, sfmmup->sfmmu_cnum, &sfhme->hme_tte);

		/*
		 * Now we can allow our ctx to be stolen.
		 */
		if (sfmmup != ksfmmup)
			sfmmu_allow_ctx_steal(sfmmup);
	}
	if (pp) {
		if (!remap) {
			hme_add(sfhme, pp);
			atomic_add_16(&hmeblkp->hblk_hmecnt, 1);
			ASSERT(hmeblkp->hblk_hmecnt > 0);

			/*
			 * Cannot ASSERT(hmeblkp->hblk_hmecnt <= NHMENTS)
			 * see pageunload() for comment.
			 */
		}
		sfmmu_mlist_exit(pml);
	}

	return (0);
}

/*
 * Function unlocks hash bucket.
 */
static void
sfmmu_tteload_release_hashbucket(struct hmehash_bucket *hmebp)
{
	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
	SFMMU_HASH_UNLOCK(hmebp);
}

/*
 * function which checks and sets up page array for a large
 * translation.  Will set p_vcolor, p_index, p_ro fields.
 * Assumes addr and pfnum of first page are properly aligned.
 * Will check for physical contiguity. If check fails it return
 * non null.
 */
static int
sfmmu_pagearray_setup(caddr_t addr, machpage_t **pps, tte_t *ttep, int remap)
{
	int 	i, index, ttesz, osz;
	pfn_t	pfnum;
	pgcnt_t	npgs;
	int cflags = 0;
	machpage_t *pp, *pp1;
	kmutex_t *pmtx;
	int vac_err = 0;

	ttesz = ttep->tte_size;

	ASSERT(ttesz > TTE8K);

	npgs = TTEPAGES(ttesz);
	index = PAGESZ_TO_INDEX(ttesz);

	pfnum = (*pps)->p_pagenum;
	ASSERT(IS_P2ALIGNED(pfnum, npgs));

	/*
	 * Save the first pp so we can do HAT_TMPNC at the end.
	 */
	pp1 = *pps;
	osz = fnd_mapping_sz(pp1);

	for (i = 0; i < npgs; i++, pps++) {
		pp = *pps;
		ASSERT(sfmmu_mlist_held(pp));

		/*
		 * XXX is it possible to maintain P_RO on the root only?
		 */
		if (TTE_IS_WRITABLE(ttep) && PP_ISRO(pp)) {
			pmtx = sfmmu_page_enter(pp);
			PP_CLRRO(pp);
			sfmmu_page_exit(pmtx);
		} else if (!PP_ISMAPPED(pp) && !TTE_IS_WRITABLE(ttep) &&
		    !PP_ISMOD(pp)) {
			pmtx = sfmmu_page_enter(pp);
			if (!(PP_ISMOD(pp))) {
				PP_SETRO(pp);
			}
			sfmmu_page_exit(pmtx);
		}

		/*
		 * If this is a remap we skip vac & contiguity checks.
		 */
		if (remap)
			continue;

		/*
		 * set p_vcolor and detect any vac conflicts.
		 */
		if (vac_err == 0) {
			vac_err = sfmmu_vacconflict_array(addr, pp, &cflags);

		}

		/*
		 * save current index in case we need to undo it.
		 */
		pp->p_index = ((pp->p_index << SFMMU_INDEX_SHIFT) | index |
		    PP_MAPINDEX(pp));

		/*
		 * contiguity check
		 */
		if (pp->p_pagenum != pfnum) {
			/*
			 * If we fail the contiguity test then
			 * the only thing we need to fix is the p_index field.
			 * We might get a few extra flushes but since this
			 * path is rare that is ok.  The p_ro field will
			 * get automatically fixed on the next tteload to
			 * the page.  NO TNC bit is set yet.
			 */
			while (i >= 0) {
				pp = *pps;
				pp->p_index = pp->p_index >> SFMMU_INDEX_SHIFT;
				pps--;
				i--;
			}
			return (1);
		}
		pfnum++;
		addr += MMU_PAGESIZE;
	}

	if (vac_err) {
		if (ttesz > osz) {
			/*
			 * There are some smaller mappings that causes vac
			 * conflicts. Convert all existing small mappings to
			 * TNC.
			 */
			SFMMU_STAT_ADD(sf_uncache_conflict, npgs);
			sfmmu_page_cache_array(pp1, HAT_TMPNC, CACHE_FLUSH,
				npgs);
		} else {
			/* EMPTY */
			/*
			 * If there exists an big page mapping,
			 * that means the whole existing big page
			 * has TNC setting already. No need to covert to
			 * TNC again.
			 */
			ASSERT(PP_ISTNC(pp1));
		}
	}

	return (0);
}

/*
 * Routine that detects vac consistency for a large page. It also
 * sets virtual color for all pp's for this big mapping.
 */
static int
sfmmu_vacconflict_array(caddr_t addr, machpage_t *pp, int *cflags)
{
	int vcolor, ocolor;

	ASSERT(sfmmu_mlist_held(pp));

	if (PP_ISNC(pp)) {
		return (HAT_TMPNC);
	}

	vcolor = addr_to_vcolor(addr);
	if (PP_NEWPAGE(pp)) {
		PP_SET_VCOLOR(pp, vcolor);
		return (0);
	}

	ocolor = PP_GET_VCOLOR(pp);
	if (ocolor == vcolor) {
		return (0);
	}

	if (!PP_ISMAPPED(pp)) {
		/*
		 * Previous user of page had a differnet color
		 * but since there are no current users
		 * we just flush the cache and change the color.
		 * As an optimization for large pages we flush the
		 * entire cache of that color and set a flag.
		 */
		SFMMU_STAT(sf_pgcolor_conflict);
		if (!CacheColor_IsFlushed(*cflags, ocolor)) {
			CacheColor_SetFlushed(*cflags, ocolor);
			sfmmu_cache_flushcolor(ocolor, pp->p_pagenum);
		}
		PP_SET_VCOLOR(pp, vcolor);
		return (0);
	}

	/*
	 * We got a real conflict with a current mapping.
	 * set flags to start unencaching all mappings
	 * and return failure so we restart looping
	 * the pp array from the beginning.
	 */
	return (HAT_TMPNC);
}

/*
 * creates a large page shadow hmeblk for a tte.
 * The purpose of this routine is to allow us to do quick unloads because
 * the vm layer can easily pass a very large but sparsely populated range.
 */
static struct hme_blk *
sfmmu_shadow_hcreate(sfmmu_t *sfmmup, caddr_t vaddr, int ttesz)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, size, vshift;
	uint_t shw_mask, newshw_mask;
	struct hme_blk *hmeblkp;

	ASSERT(sfmmup != KHATID);
	ASSERT(ttesz < TTE4M);

	size = (ttesz == TTE8K)? TTE512K : ++ttesz;

	hblktag.htag_id = sfmmup;
	hmeshift = HME_HASH_SHIFT(size);
	hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
	hblktag.htag_rehash = HME_HASH_REHASH(size);
	hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

	SFMMU_HASH_LOCK(hmebp);

	HME_HASH_FAST_SEARCH(hmebp, hblktag, hmeblkp);
	if (hmeblkp == NULL) {
		hmeblkp = sfmmu_hblk_alloc(sfmmup, vaddr, hmebp, size, hblktag);
	}
	ASSERT(hmeblkp);
	if (!hmeblkp->hblk_shw_mask) {
		/*
		 * if this is a unused hblk it was just allocated or could
		 * potentially be a previous large page hblk so we need to
		 * set the shadow bit.
		 */
		hmeblkp->hblk_shw_bit = 1;
	}
	ASSERT(hmeblkp->hblk_shw_bit == 1);
	vshift = vaddr_to_vshift(hblktag, vaddr, size);
	ASSERT(vshift < 8);
	/*
	 * Atomically set shw mask bit
	 */
	do {
		shw_mask = hmeblkp->hblk_shw_mask;
		newshw_mask = shw_mask | (1 << vshift);
		newshw_mask = cas32(&hmeblkp->hblk_shw_mask, shw_mask,
			newshw_mask);
	} while (newshw_mask != shw_mask);

	SFMMU_HASH_UNLOCK(hmebp);

	return (hmeblkp);
}

/*
 * This routine cleanup a previous shadow hmeblk and changes it to
 * a regular hblk.  This happens rarely but it is possible
 * when a process wants to use large pages and there are hblks still
 * lying around from the previous as that used these hmeblks.
 * The alternative was to cleanup the shadow hblks at unload time
 * but since so few user processes actually use large pages, it is
 * better to be lazy and cleanup at this time.
 */
static void
sfmmu_shadow_hcleanup(sfmmu_t *sfmmup, struct hme_blk *hmeblkp,
	struct hmehash_bucket *hmebp)
{
	caddr_t addr, endaddr;
	int hashno, size;

	ASSERT(hmeblkp->hblk_shw_bit);

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));

	if (!hmeblkp->hblk_shw_mask) {
		hmeblkp->hblk_shw_bit = 0;
		return;
	}
	addr = (caddr_t)get_hblk_base(hmeblkp);
	endaddr = get_hblk_endaddr(hmeblkp);
	size = get_hblk_ttesz(hmeblkp);
	hashno = size - 1;
	ASSERT(hashno > 0);
	SFMMU_HASH_UNLOCK(hmebp);

	sfmmu_free_hblks(sfmmup, addr, endaddr, hashno);

	SFMMU_HASH_LOCK(hmebp);
}

static void
sfmmu_free_hblks(sfmmu_t *sfmmup, caddr_t addr, caddr_t endaddr,
	int hashno)
{
	int hmeshift, shadow = 0;
	hmeblk_tag hblktag;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	struct hme_blk *nx_hblk, *pr_hblk;
	uint64_t hblkpa, prevpa, nx_pa;

	ASSERT(hashno > 0);
	hblktag.htag_id = sfmmup;
	hblktag.htag_rehash = hashno;

	hmeshift = HME_HASH_SHIFT(hashno);

	while (addr < endaddr) {
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		/* inline HME_HASH_SEARCH */
		hmeblkp = hmebp->hmeblkp;
		hblkpa = hmebp->hmeh_nextpa;
		prevpa = 0;
		pr_hblk = NULL;
		while (hmeblkp) {
			ASSERT(hblkpa == va_to_pa((caddr_t)hmeblkp));
			if (HTAGS_EQ(hmeblkp->hblk_tag, hblktag)) {
				/* found hme_blk */
				if (hmeblkp->hblk_shw_bit) {
					if (hmeblkp->hblk_shw_mask) {
						shadow = 1;
						sfmmu_shadow_hcleanup(sfmmup,
							hmeblkp, hmebp);
						break;
					} else {
						hmeblkp->hblk_shw_bit = 0;
					}
				}

				/*
				 * Hblk_hmecnt and hblk_vcnt could be non zero
				 * since hblk_unload() does not gurantee that.
				 *
				 * XXX - this could cause tteload() to spin
				 * where sfmmu_shadow_hcleanup() is called.
				 */
			}

			nx_hblk = hmeblkp->hblk_next;
			nx_pa = hmeblkp->hblk_nextpa;
			if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
				sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa,
					pr_hblk);
				sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			} else {
				pr_hblk = hmeblkp;
				prevpa = hblkpa;
			}
			hmeblkp = nx_hblk;
			hblkpa = nx_pa;
		}

		SFMMU_HASH_UNLOCK(hmebp);

		if (shadow) {
			/*
			 * We found another shadow hblk so cleaned its
			 * children.  We need to go back and cleanup
			 * the original hblk so we don't change the
			 * addr.
			 */
			shadow = 0;
		} else {
			addr = (caddr_t)roundup((uintptr_t)addr + 1,
				(1 << hmeshift));
		}
	}
}

/*
 * Release one hardware address translation lock on the given address range.
 */
void
hat_unlock(struct hat *sfmmup, caddr_t addr, size_t len)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;

	ASSERT(sfmmup != NULL);
	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	/*
	 * Spitfire supports 4 page sizes.
	 * Most pages are expected to be of the smallest page size (8K) and
	 * these will not need to be rehashed. 64K pages also don't need to be
	 * rehashed because an hmeblk spans 64K of address space. 512K pages
	 * might need 1 rehash and and 4M pages might need 2 rehashes.
	 */
	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			/*
			 * If we encounter a shadow hmeblk then
			 * we know there are no valid hmeblks mapping
			 * this address at this size or larger.
			 * Just increment address by the smallest
			 * page size.
			 */
			if (hmeblkp->hblk_shw_bit) {
				addr += MMU_PAGESIZE;
			} else {
				addr = sfmmu_hblk_unlock(hmeblkp, addr,
				    endaddr);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (sfmmup->sfmmu_lttecnt == 0 || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without finding the address to unlock
			 * which should never happen.
			 */
			panic("sfmmu_unlock: addr not found. "
			    "addr %p hat %p", (void *)addr, (void *)sfmmup);
		} else {
			hashno++;
		}
	}
}

/*
 * Function to unlock a range of addresses in an hmeblk.  It returns the
 * next address that needs to be unlocked.
 * Should be called with the hash lock held.
 */
static caddr_t
sfmmu_hblk_unlock(struct hme_blk *hmeblkp, caddr_t addr, caddr_t endaddr)
{
	struct sf_hment *sfhme;
	tte_t tteold;
	int ttesz;

	ASSERT(in_hblk_range(hmeblkp, addr));
	ASSERT(hmeblkp->hblk_shw_bit == 0);

	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));
	ttesz = get_hblk_ttesz(hmeblkp);

	sfhme = sfmmu_hblktohme(hmeblkp, addr, NULL);
	while (addr < endaddr) {
		sfmmu_copytte(&sfhme->hme_tte, &tteold);
		if (TTE_IS_VALID(&tteold)) {

			if (hmeblkp->hblk_lckcnt == 0)
				panic("zero tte lckcnt");

			if (((uintptr_t)addr + TTEBYTES(ttesz)) >
			    (uintptr_t)endaddr)
				panic("can't unlock large tte");

			ASSERT(hmeblkp->hblk_lckcnt > 0);
			atomic_add_16(&hmeblkp->hblk_lckcnt, -1);
			HBLK_STACK_TRACE(hmeblkp, HBLK_UNLOCK);
		} else {
			panic("sfmmu_hblk_unlock: invalid tte");
		}
		addr += TTEBYTES(ttesz);
		sfhme++;
	}
	return (addr);
}

/*
 * hat_probe returns 1 if the translation for the address 'addr' is
 * loaded, zero otherwise.
 *
 * hat_probe should be used only for advisorary purposes because it may
 * occasionally return the wrong value. The implementation must guarantee that
 * returning the wrong value is a very rare event. hat_probe is used
 * to implement optimizations in the segment drivers.
 *
 */
int
hat_probe(struct hat *sfmmup, caddr_t addr)
{
	pfn_t pfn;

	ASSERT(sfmmup != NULL);
	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));

	if (sfmmup == ksfmmup) {
		pfn = sfmmu_vatopfn(addr, sfmmup);
	} else {
		pfn = sfmmu_uvatopfn(addr, sfmmup);
	}

	if (pfn != PFN_INVALID)
		return (1);
	else
		return (0);
}

ssize_t
hat_getpagesize(struct hat *sfmmup, caddr_t addr)
{
	tte_t tte;

	tte = sfmmu_gettte(sfmmup, addr);
	if (TTE_IS_VALID(&tte)) {
		return (TTEBYTES(tte.tte_size));
	}
	return (-1);
}

static tte_t
sfmmu_gettte(struct hat *sfmmup, caddr_t addr)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	struct sf_hment *sfhmep;
	tte_t tte;


	ASSERT(!((uintptr_t)addr & MMU_PAGEOFFSET));

	hblktag.htag_id = sfmmup;
	tte.ll = 0;

	do {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);
			sfmmu_copytte(&sfhmep->hme_tte, &tte);
			SFMMU_HASH_UNLOCK(hmebp);
			return (tte);
		}
		SFMMU_HASH_UNLOCK(hmebp);
		hashno++;
	} while (sfmmup->sfmmu_lttecnt && (hashno <= MAX_HASHCNT));
	/*
	 * We have traversed the entire hmeblk list and
	 * rehashed if necessary without finding the addr.
	 */
	return (tte);
}

uint_t
hat_getattr(struct hat *sfmmup, caddr_t addr, uint_t *attr)
{
	tte_t tte;

	tte = sfmmu_gettte(sfmmup, addr);
	if (TTE_IS_VALID(&tte)) {
		*attr = sfmmu_ptov_attr(&tte);
		return (0);
	}
	*attr = 0;
	return ((uint_t)0xffffffff);
}

/*
 * Enables more attributes on specified address range (ie. logical OR)
 */
void
hat_setattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	sfmmu_chgattr(hat, addr, len, attr, SFMMU_SETATTR);
}

/*
 * Assigns attributes to the specified address range.  All the attributes
 * are specified.
 */
void
hat_chgattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	sfmmu_chgattr(hat, addr, len, attr, SFMMU_CHGATTR);
}

/*
 * Remove attributes on the specified address range (ie. loginal NAND)
 */
void
hat_clrattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	sfmmu_chgattr(hat, addr, len, attr, SFMMU_CLRATTR);
}

/*
 * Change attributes on an address range to that specified by attr and mode.
 */
static void
sfmmu_chgattr(struct hat *sfmmup, caddr_t addr, size_t len, uint_t attr,
	int mode)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;
	cpuset_t cpuset;

	CPUSET_ZERO(cpuset);

	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(((uintptr_t)addr & MMU_PAGEOFFSET) == 0);

	if ((attr & PROT_USER) && (mode != SFMMU_CLRATTR) &&
	    ((addr + len) > (caddr_t)USERLIMIT)) {
		panic("user addr %p in kernel space",
		    (void *)addr);
	}

	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			/*
			 * If we encounter a shadow hmeblk then
			 * we know there are no valid hmeblks mapping
			 * this address at this size or larger.
			 * Just increment address by the smallest
			 * page size.
			 */
			if (hmeblkp->hblk_shw_bit) {
				addr += MMU_PAGESIZE;
			} else {
				addr = sfmmu_hblk_chgattr(hmeblkp, addr,
				    endaddr, attr, mode);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (sfmmup->sfmmu_lttecnt == 0 || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without finding the address to chgattr.
			 * This is ok so we increment the address by the
			 * smallest page size and continue.
			 */
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}

	cpuset = sfmmup->sfmmu_cpusran;
	xt_sync(cpuset);
}

/*
 * This function chgattr on a range of addresses in an hmeblk.  It returns the
 * next addres that needs to be chgattr.
 * It should be called with the hash lock held.
 * XXX It should be possible to optimize chgattr by not flushing every time but
 * on the other hand:
 * 1. do one flush crosscall.
 * 2. only flush if we are increasing permissions (make sure this will work)
 */
static caddr_t
sfmmu_hblk_chgattr(struct hme_blk *hmeblkp, caddr_t addr,
	caddr_t endaddr, uint_t attr, int mode)
{
	tte_t tte, tteattr, tteflags, ttemod;
	struct sf_hment *sfhmep;
	sfmmu_t *sfmmup;
	int ttesz;
	struct machpage *pp = NULL;
	kmutex_t *pml, *pmtx;
	int ret;
#if defined(__sparcv9) && defined(SF_ERRATA_57)
	int check_exec;
#endif

	ASSERT(in_hblk_range(hmeblkp, addr));
	ASSERT(hmeblkp->hblk_shw_bit == 0);

	sfmmup = hblktosfmmu(hmeblkp);
	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));
	ttesz = get_hblk_ttesz(hmeblkp);

	tteattr.ll = sfmmu_vtop_attr(attr, mode, &tteflags);
#if defined(__sparcv9) && defined(SF_ERRATA_57)
	check_exec = (sfmmup != ksfmmup) &&
	    AS_TYPE_64BIT(sfmmup->sfmmu_as) &&
	    TTE_IS_EXECUTABLE(&tteattr);
#endif
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);
	while (addr < endaddr) {
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			if ((tte.ll & tteflags.ll) == tteattr.ll) {
				/*
				 * if the new attr is the same as old
				 * continue
				 */
				addr += TTEBYTES(ttesz);
				sfhmep++;
				continue;
			}
			if (!TTE_IS_WRITABLE(&tteattr)) {
				/*
				 * make sure we clear hw modify bit if we
				 * removing write protections
				 */
				tteflags.tte_intlo |= TTE_HWWR_INT;
			}

			pml = NULL;
			pp = sfhmep->hme_page;
			if (pp) {
				pml = sfmmu_mlist_enter(pp);
			}

			if (pp != sfhmep->hme_page) {
				/*
				 * tte must have been unloaded.
				 */
				ASSERT(pml);
				sfmmu_mlist_exit(pml);
				continue;
			}

			ttemod = tte;
			ttemod.ll = (ttemod.ll & ~tteflags.ll) | tteattr.ll;
			ASSERT(TTE_TO_TTEPFN(&ttemod) == TTE_TO_TTEPFN(&tte));

#if defined(__sparcv9) && defined(SF_ERRATA_57)
			if (check_exec && addr < errata57_limit)
				ttemod.tte_exec_perm = 0;
#endif
			ret = sfmmu_modifytte_try(&tte, &ttemod,
			    &sfhmep->hme_tte);

			if (ret < 0) {
				/* tte changed underneath us */
				if (pml) {
					sfmmu_mlist_exit(pml);
				}
				continue;
			}

			if (tteflags.tte_intlo & TTE_HWWR_INT) {
				/*
				 * need to sync if we are clearing modify bit.
				 */
				sfmmu_ttesync(sfmmup, addr, &tte, pp);
			}

			if (pp && PP_ISRO(pp)) {
				if (tteattr.tte_intlo & TTE_WRPRM_INT) {
					pmtx = sfmmu_page_enter(pp);
					PP_CLRRO(pp);
					sfmmu_page_exit(pmtx);
				}
			}

			if (ret > 0) {
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0, 0);
			}

			if (pml) {
				sfmmu_mlist_exit(pml);
			}
		}
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * This routine converts virtual attributes to physical ones.  It will
 * update the tteflags field with the tte mask corresponding to the attributes
 * affected and it returns the new attributes.  It will also clear the modify
 * bit if we are taking away write permission.  This is necessary since the
 * modify bit is the hardware permission bit and we need to clear it in order
 * to detect write faults.
 */
static uint64_t
sfmmu_vtop_attr(uint_t attr, int mode, tte_t *ttemaskp)
{
	tte_t ttevalue;

	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));

	switch (mode) {
	case SFMMU_CHGATTR:
		/* all attributes specified */
		ttevalue.tte_inthi = MAKE_TTEATTR_INTHI(attr);
		ttevalue.tte_intlo = MAKE_TTEATTR_INTLO(attr);
		ttemaskp->tte_inthi = TTEINTHI_ATTR;
		ttemaskp->tte_intlo = TTEINTLO_ATTR;
		break;
	case SFMMU_SETATTR:
		ASSERT(!(attr & ~HAT_PROT_MASK));
		ttemaskp->ll = 0;
		ttevalue.ll = 0;
		/*
		 * a valid tte implies exec and read for sfmmu
		 * so no need to do anything about them.
		 * since priviledged access implies user access
		 * PROT_USER doesn't make sense either.
		 */
		if (attr & PROT_WRITE) {
			ttemaskp->tte_intlo |= TTE_WRPRM_INT;
			ttevalue.tte_intlo |= TTE_WRPRM_INT;
		}
		break;
	case SFMMU_CLRATTR:
		/* attributes will be nand with current ones */
		if (attr & ~(PROT_WRITE | PROT_USER)) {
			panic("sfmmu: attr %x not supported", attr);
		}
		ttemaskp->ll = 0;
		ttevalue.ll = 0;
		if (attr & PROT_WRITE) {
			/* clear both writable and modify bit */
			ttemaskp->tte_intlo |= TTE_WRPRM_INT | TTE_HWWR_INT;
		}
		if (attr & PROT_USER) {
			ttemaskp->tte_intlo |= TTE_PRIV_INT;
			ttevalue.tte_intlo |= TTE_PRIV_INT;
		}
		break;
	default:
		panic("sfmmu_vtop_attr: bad mode %x", mode);
	}
	ASSERT(TTE_TO_TTEPFN(&ttevalue) == 0);
	return (ttevalue.ll);
}

static uint_t
sfmmu_ptov_attr(tte_t *ttep)
{
	uint_t attr;

	ASSERT(TTE_IS_VALID(ttep));

	attr = PROT_READ;

	if (TTE_IS_WRITABLE(ttep)) {
		attr |= PROT_WRITE;
	}
	if (TTE_IS_EXECUTABLE(ttep)) {
		attr |= PROT_EXEC;
	}
	if (!TTE_IS_PRIVILEGED(ttep)) {
		attr |= PROT_USER;
	}
	if (TTE_IS_NFO(ttep)) {
		attr |= HAT_NOFAULT;
	}
	if (TTE_IS_NOSYNC(ttep)) {
		attr |= HAT_NOSYNC;
	}
	if (TTE_IS_SIDEFFECT(ttep)) {
		attr |= SFMMU_SIDEFFECT;
	}
	if (!TTE_IS_VCACHEABLE(ttep)) {
		attr |= SFMMU_UNCACHEVTTE;
	}
	if (!TTE_IS_PCACHEABLE(ttep)) {
		attr |= SFMMU_UNCACHEPTTE;
	}
	return (attr);
}

/*
 * hat_chgprot is a deprecated hat call.  New segment drivers
 * should store all attributes and use hat_*attr calls.
 *
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If vprot is ~PROT_WRITE,
 * then remove write permission, leaving the other
 * permissions unchanged.  If vprot is ~PROT_USER, remove user permissions.
 *
 */
void
hat_chgprot(struct hat *sfmmup, caddr_t addr, size_t len, uint_t vprot)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;
	cpuset_t cpuset;

	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(((uintptr_t)addr & MMU_PAGEOFFSET) == 0);

	CPUSET_ZERO(cpuset);

	if ((vprot != (uint_t)~PROT_WRITE) && (vprot & PROT_USER) &&
	    ((addr + len) > (caddr_t)USERLIMIT)) {
		panic("user addr %p vprot %x in kernel space",
		    (void *)addr, vprot);
	}
	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			/*
			 * If we encounter a shadow hmeblk then
			 * we know there are no valid hmeblks mapping
			 * this address at this size or larger.
			 * Just increment address by the smallest
			 * page size.
			 */
			if (hmeblkp->hblk_shw_bit) {
				addr += MMU_PAGESIZE;
			} else {
				addr = sfmmu_hblk_chgprot(hmeblkp, addr,
				    endaddr, vprot);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (sfmmup->sfmmu_lttecnt == 0 || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without finding the address to chgprot.
			 * This is ok so we increment the address by the
			 * smallest page size and continue.
			 */
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}

	cpuset = sfmmup->sfmmu_cpusran;
	xt_sync(cpuset);
}

/*
 * This function chgprots a range of addresses in an hmeblk.  It returns the
 * next addres that needs to be chgprot.
 * It should be called with the hash lock held.
 * XXX It shold be possible to optimize chgprot by not flushing every time but
 * on the other hand:
 * 1. do one flush crosscall.
 * 2. only flush if we are increasing permissions (make sure this will work)
 */
static caddr_t
sfmmu_hblk_chgprot(struct hme_blk *hmeblkp, caddr_t addr, caddr_t endaddr,
	uint_t vprot)
{
	uint_t pprot;
	tte_t tte, ttemod;
	sfmmu_t *sfmmup;
	struct sf_hment *sfhmep;
	uint_t tteflags;
	int ttesz;
	struct machpage *pp = NULL;
	kmutex_t *pml, *pmtx;
	int ret;
#if defined(__sparcv9) && defined(SF_ERRATA_57)
	int check_exec;
#endif

	ASSERT(in_hblk_range(hmeblkp, addr));
	ASSERT(hmeblkp->hblk_shw_bit == 0);

#ifdef DEBUG
	if (get_hblk_ttesz(hmeblkp) != TTE8K &&
	    (endaddr < get_hblk_endaddr(hmeblkp))) {
		panic("sfmmu_hblk_chgprot: partial chgprot of large page");
	}
#endif DEBUG

	sfmmup = hblktosfmmu(hmeblkp);
	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));
	ttesz = get_hblk_ttesz(hmeblkp);

	pprot = sfmmu_vtop_prot(vprot, &tteflags);
#if defined(__sparcv9) && defined(SF_ERRATA_57)
	check_exec = (sfmmup != ksfmmup) &&
	    AS_TYPE_64BIT(sfmmup->sfmmu_as) &&
	    ((vprot & PROT_EXEC) == PROT_EXEC);
#endif
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);
	while (addr < endaddr) {
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			if (TTE_GET_LOFLAGS(&tte, tteflags) == pprot) {
				/*
				 * if the new protection is the same as old
				 * continue
				 */
				addr += TTEBYTES(ttesz);
				sfhmep++;
				continue;
			}
			pml = NULL;
			pp = sfhmep->hme_page;
			if (pp) {
				pml = sfmmu_mlist_enter(pp);
			}
			if (pp != sfhmep->hme_page) {
				/*
				 * tte most have been unloaded
				 * underneath us.  Recheck
				 */
				ASSERT(pml);
				sfmmu_mlist_exit(pml);
				continue;
			}

			ttemod = tte;
			TTE_SET_LOFLAGS(&ttemod, tteflags, pprot);
#if defined(__sparcv9) && defined(SF_ERRATA_57)
			if (check_exec && addr < errata57_limit)
				ttemod.tte_exec_perm = 0;
#endif
			ret = sfmmu_modifytte_try(&tte, &ttemod,
			    &sfhmep->hme_tte);

			if (ret < 0) {
				/* tte changed underneath us */
				if (pml) {
					sfmmu_mlist_exit(pml);
				}
				continue;
			}

			if (tteflags & TTE_HWWR_INT) {
				/*
				 * need to sync if we are clearing modify bit.
				 */
				sfmmu_ttesync(sfmmup, addr, &tte, pp);
			}

			if (pp && PP_ISRO(pp)) {
				if (pprot & TTE_WRPRM_INT) {
					pmtx = sfmmu_page_enter(pp);
					PP_CLRRO(pp);
					sfmmu_page_exit(pmtx);
				}
			}

			if (ret > 0) {
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0, 0);
			}

			if (pml) {
				sfmmu_mlist_exit(pml);
			}
		}
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * This routine is deprecated and should only be used by hat_chgprot.
 * The correct routine is sfmmu_vtop_attr.
 * This routine converts virtual page protections to physical ones.  It will
 * update the tteflags field with the tte mask corresponding to the protections
 * affected and it returns the new protections.  It will also clear the modify
 * bit if we are taking away write permission.  This is necessary since the
 * modify bit is the hardware permission bit and we need to clear it in order
 * to detect write faults.
 * It accepts the following special protections:
 * ~PROT_WRITE = remove write permissions.
 * ~PROT_USER = remove user permissions.
 */
static uint_t
sfmmu_vtop_prot(uint_t vprot, uint_t *tteflagsp)
{
	if (vprot == (uint_t)~PROT_WRITE) {
		*tteflagsp = TTE_WRPRM_INT | TTE_HWWR_INT;
		return (0);		/* will cause wrprm to be cleared */
	}
	if (vprot == (uint_t)~PROT_USER) {
		*tteflagsp = TTE_PRIV_INT;
		return (0);		/* will cause privprm to be cleared */
	}
	if ((vprot == 0) || (vprot == PROT_USER) ||
		((vprot & PROT_ALL) != vprot)) {
		panic("sfmmu_vtop_prot -- bad prot %x", vprot);
	}

	switch (vprot) {
	case (PROT_READ):
	case (PROT_EXEC):
	case (PROT_EXEC | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT | TTE_HWWR_INT;
		return (TTE_PRIV_INT); 		/* set prv and clr wrt */
	case (PROT_WRITE):
	case (PROT_WRITE | PROT_READ):
	case (PROT_EXEC | PROT_WRITE):
	case (PROT_EXEC | PROT_WRITE | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT;
		return (TTE_PRIV_INT | TTE_WRPRM_INT); 	/* set prv and wrt */
	case (PROT_USER | PROT_READ):
	case (PROT_USER | PROT_EXEC):
	case (PROT_USER | PROT_EXEC | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT | TTE_HWWR_INT;
		return (0); 			/* clr prv and wrt */
	case (PROT_USER | PROT_WRITE):
	case (PROT_USER | PROT_WRITE | PROT_READ):
	case (PROT_USER | PROT_EXEC | PROT_WRITE):
	case (PROT_USER | PROT_EXEC | PROT_WRITE | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT;
		return (TTE_WRPRM_INT); 	/* clr prv and set wrt */
	default:
		panic("sfmmu_vtop_prot -- bad prot %x", vprot);
	}
	return (0);
}


/*
 * Unload all the mappings in the range [addr..addr+len). addr and len must
 * be MMU_PAGESIZE aligned.
 */
void
hat_unload(struct hat *sfmmup, caddr_t addr, size_t len, uint_t flags)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno, iskernel;
	struct hme_blk *hmeblkp, *pr_hblk;
	caddr_t endaddr;
	cpuset_t cpuset;
	uint64_t hblkpa, prevpa;

	ASSERT((sfmmup == ksfmmup) || (flags & HAT_UNLOAD_OTHER) || \
	    AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));


	ASSERT(sfmmup != NULL);
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(!((uintptr_t)addr & MMU_PAGEOFFSET));

	CPUSET_ZERO(cpuset);

	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	/*
	 * It is likely for the vm to call unload over a wide range of
	 * addresses that are actually very sparsely populated by
	 * translations.  In order to speed this up the sfmmu hat supports
	 * the concept of shadow hmeblks. Dummy large page hmeblks that
	 * correspond to actual small translations are allocated at tteload
	 * time and are referred to as shadow hmeblks.  Now, during unload
	 * time, we first check if we have a shadow hmeblk for that
	 * translation.  The absence of one means the corresponding address
	 * range is empty and can be skipped.
	 *
	 * The kernel is an exception to above statement and that is why
	 * we don't use shadow hmeblks and hash starting from the smallest
	 * page size.
	 */
	if (sfmmup == KHATID) {
		iskernel = 1;
		hashno = TTE64K;
	} else {
		iskernel = 0;
		hashno = TTE4M;
	}
	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH_PREV(hmebp, hblktag, hmeblkp, hblkpa, pr_hblk,
			prevpa);
		if (hmeblkp == NULL) {
			/*
			 * didn't find an hmeblk. skip the appropiate
			 * address range.
			 */
			SFMMU_HASH_UNLOCK(hmebp);
			if (iskernel) {
				if (hashno < MAX_HASHCNT) {
					hashno++;
					continue;
				} else {
					hashno = TTE64K;
					addr = (caddr_t)roundup((uintptr_t)addr
						+ 1, MMU_PAGESIZE64K);
					continue;
				}
			}
			addr = (caddr_t)roundup((uintptr_t)addr + 1,
				(1 << hmeshift));
			if ((uintptr_t)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((uintptr_t)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		ASSERT(hmeblkp);
		if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
			/*
			 * If the valid count is zero we can skip the range
			 * mapped by this hmeblk.
			 * We free hblks in the case of HAT_UNMAP.  HAT_UNMAP
			 * is used by segment drivers as a hint
			 * that the mapping resource won't be used any longer.
			 * The best example of this is during exit().
			 */
			addr = (caddr_t)roundup((uintptr_t)addr + 1,
				get_hblk_span(hmeblkp));
			if ((flags & HAT_UNLOAD_UNMAP) || iskernel) {
				sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa,
				    pr_hblk);
				sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			if (iskernel) {
				hashno = TTE64K;
				continue;
			}
			if ((uintptr_t)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((uintptr_t)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		if (hmeblkp->hblk_shw_bit) {
			/*
			 * If we encounter a shadow hmeblk we know there is
			 * smaller sized hmeblks mapping the same address space.
			 * Decrement the hash size and rehash.
			 */
			ASSERT(sfmmup != KHATID);
			hashno--;
			SFMMU_HASH_UNLOCK(hmebp);
			continue;
		}
		addr = sfmmu_hblk_unload(sfmmup, hmeblkp, addr, endaddr, flags);
		if (((flags & HAT_UNLOAD_UNMAP) || iskernel) &&
		    !hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
			sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa,
			    pr_hblk);
			sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
		}
		SFMMU_HASH_UNLOCK(hmebp);
		if (iskernel) {
			hashno = TTE64K;
			continue;
		}
		if ((uintptr_t)addr & MMU_PAGEOFFSET512K) {
			ASSERT(hashno == TTE64K);
			continue;
		}
		if ((uintptr_t)addr & MMU_PAGEOFFSET4M) {
			hashno = TTE512K;
			continue;
		}
		hashno = TTE4M;
	}
	cpuset = sfmmup->sfmmu_cpusran;
	xt_sync(cpuset);
}

/*
 * Find the largest mapping size for this page.
 */
static int
fnd_mapping_sz(machpage_t *pp)
{
	int sz;
	int p_index;

	p_index = PP_MAPINDEX(pp);

	sz = 0;
	p_index >>= 1;	/* don't care about 8K bit */
	for (; p_index; p_index >>= 1) {
		sz++;
	}

	return (sz);
}

/*
 * This function unloads a range of addresses for an hmeblk.
 * It returns the next addres to be unloaded.
 * It should be called with the hash lock held.
 */
static caddr_t
sfmmu_hblk_unload(struct hat *sfmmup, struct hme_blk *hmeblkp, caddr_t addr,
	caddr_t endaddr, uint_t flags)
{
	tte_t	tte, ttemod;
	struct	sf_hment *sfhmep;
	int	ttesz;
	struct	machpage *pp;
	kmutex_t *pml;
	int ret;

	ASSERT(in_hblk_range(hmeblkp, addr));
	ASSERT(!hmeblkp->hblk_shw_bit);
#ifdef DEBUG
	if (get_hblk_ttesz(hmeblkp) != TTE8K &&
	    (endaddr < get_hblk_endaddr(hmeblkp))) {
		panic("sfmmu_hblk_unload: partial unload of large page");
	}
#endif DEBUG

	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));

	ttesz = get_hblk_ttesz(hmeblkp);
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, NULL);

	while (addr < endaddr) {
		pml = NULL;
again:
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			pp = sfhmep->hme_page;
			if (pp && pml == NULL) {
				pml = sfmmu_mlist_enter(pp);
			}

			/*
			 * Verify if hme still points to 'pp' now that
			 * we have p_mapping lock.
			 */
			if (sfhmep->hme_page != pp) {
				if (pp != NULL && sfhmep->hme_page != NULL) {
					if (pml) {
						sfmmu_mlist_exit(pml);
					}
					/* Re-start this iteration. */
					continue;
				}
				ASSERT((pp != NULL) && (sfhmep->hme_page
				    == NULL));
				goto tte_unloaded;
			}

			/*
			 * This point on we have both HASH and p_mapping
			 * lock.
			 */
			ASSERT(pp == sfhmep->hme_page);
			ASSERT(pp == NULL || sfmmu_mlist_held(pp));

			/*
			 * We need to loop on modify tte because it is
			 * possible for pagesync to come along and
			 * change the software bits beneath us.
			 *
			 * Page_unload can also invalidate the tte after
			 * we read tte outside of p_mapping lock.
			 */
			ttemod = tte;
			TTE_SET_INVALID(&ttemod);
			ret = sfmmu_modifytte_try(&tte, &ttemod,
			    &sfhmep->hme_tte);

			if (ret <= 0) {
				if (TTE_IS_VALID(&tte)) {
					goto again;
				} else {
					/*
					 * We read in a valid pte, but it
					 * is unloaded by page_unload.
					 * hme_page has become NULL and
					 * we hold no p_mapping lock.
					 */
					ASSERT(pp == NULL && pml == NULL);
					goto tte_unloaded;
				}
			}

			if (!(flags & HAT_UNLOAD_NOSYNC)) {
				sfmmu_ttesync(sfmmup, addr, &tte, pp);
			}

			/*
			 * Ok- we invalidated the tte. Do the rest of the job.
			 */

			if (ttesz == TTE8K) {
				atomic_add_32(&sfmmup->sfmmu_rss, -1);
			} else {
				atomic_add_32(&sfmmup->sfmmu_lttecnt, -1);
			}

			if (flags & HAT_UNLOAD_UNLOCK) {
				ASSERT(hmeblkp->hblk_lckcnt > 0);
				atomic_add_16(&hmeblkp->hblk_lckcnt, -1);
				HBLK_STACK_TRACE(hmeblkp, HBLK_UNLOCK);
			}

			/*
			 * Normally we would need to flush the page
			 * from the virtual cache at this point in
			 * order to prevent a potential cache alias
			 * inconsistency.
			 * The particular scenario we need to worry
			 * about is:
			 * Given:  va1 and va2 are two virtual address
			 * that alias and map the same physical
			 * address.
			 * 1.	mapping exists from va1 to pa and data
			 * has been read into the cache.
			 * 2.	unload va1.
			 * 3.	load va2 and modify data using va2.
			 * 4	unload va2.
			 * 5.	load va1 and reference data.  Unless we
			 * flush the data cache when we unload we will
			 * get stale data.
			 * Fortunately, page coloring eliminates the
			 * above scenario by remembering the color a
			 * physical page was last or is currently
			 * mapped to.  Now, we delay the flush until
			 * the loading of translations.  Only when the
			 * new translation is of a different color
			 * are we forced to flush.
			 */
			if (do_virtual_coloring) {
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp,
				    sfmmup->sfmmu_free, 0);
			} else {
				pfn_t pfnum;

				pfnum = TTE_TO_PFN(addr, &tte);
				sfmmu_tlbcache_demap(addr, sfmmup,
				    hmeblkp, pfnum, sfmmup->sfmmu_free,
				    FLUSH_NECESSARY_CPUS, CACHE_FLUSH, 0);
			}

			if (pp) {
				/*
				 * Remove the hment from the mapping list
				 */
				ASSERT(hmeblkp->hblk_hmecnt > 0);

				/*
				 * Again, we cannot
				 * ASSERT(hmeblkp->hblk_hmecnt <= NHMENTS);
				 */
				hme_sub(sfhmep, pp);
				membar_stst();
				atomic_add_16(&hmeblkp->hblk_hmecnt, -1);
			}

			ASSERT(hmeblkp->hblk_vcnt > 0);
			atomic_add_16(&hmeblkp->hblk_vcnt, -1);

			ASSERT(hmeblkp->hblk_hmecnt || hmeblkp->hblk_vcnt ||
			    !hmeblkp->hblk_lckcnt);

			if (pp && PP_ISTNC(pp)) {
				/*
				 * if page was temporary
				 * unencached, try to recache
				 * it. Note that hme_sub() was
				 * called above so p_index and
				 * mlist had been updated.
				 */
				conv_tnc(pp, ttesz);
			}
		} else {
			if ((pp = sfhmep->hme_page) != NULL) {
				/*
				 * Tte is invalid but the hme
				 * still exists. let pageunload
				 * complete its job.
				 */
				ASSERT(pml == NULL);
				pml = sfmmu_mlist_enter(pp);
				if (sfhmep->hme_page != NULL) {
					sfmmu_mlist_exit(pml);
					pml = NULL;
					goto again;
				}
				ASSERT(sfhmep->hme_page == NULL);
				/*
				 * Pageunload should be done now,
				 * fall through.
				 */
			}
		}

tte_unloaded:
		/*
		 * At this point, the tte we are looking at
		 * should be unloaded, and hme has been unlinked
		 * from page too. This is important because in
		 * pageunload, it does ttesync() then hme_sub.
		 * We need to make sure hme_sub has been completed
		 * so we know ttesync() has been completed. Otherwise,
		 * at exit time, after return from hat layer, VM will
		 * release as structure which hat_setstat() (called
		 * by ttesync()) needs.
		 */
#ifdef DEBUG
		{
			tte_t	dtte;

			ASSERT(sfhmep->hme_page == NULL);

			sfmmu_copytte(&sfhmep->hme_tte, &dtte);
			ASSERT(!TTE_IS_VALID(&dtte));
		}
#endif

		if (pml) {
			sfmmu_mlist_exit(pml);
		}

		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * Synchronize all the mappings in the range [addr..addr+len).
 * Can be called with clearflag having two states:
 * HAT_SYNC_DONTZERO means just return the rm stats
 * HAT_SYNC_ZERORM means zero rm bits in the tte and return the stats
 */
void
hat_sync(struct hat *sfmmup, caddr_t addr, size_t len, uint_t clearflag)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;
	cpuset_t cpuset;

	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT((clearflag == HAT_SYNC_DONTZERO) ||
		(clearflag == HAT_SYNC_ZERORM));

	CPUSET_ZERO(cpuset);

	endaddr = addr + len;
	hblktag.htag_id = sfmmup;
	/*
	 * Spitfire supports 4 page sizes.
	 * Most pages are expected to be of the smallest page
	 * size (8K) and these will not need to be rehashed. 64K
	 * pages also don't need to be rehashed because the an hmeblk
	 * spans 64K of address space. 512K pages might need 1 rehash and
	 * and 4M pages 2 rehashes.
	 */
	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			/*
			 * If we encounter a shadow hmeblk then
			 * we know there are no valid hmeblks mapping
			 * this address at this size or larger.
			 * Just increment address by the smallest
			 * page size.
			 */
			if (hmeblkp->hblk_shw_bit) {
				addr += MMU_PAGESIZE;
			} else {
				addr = sfmmu_hblk_sync(sfmmup, hmeblkp,
				    addr, endaddr, clearflag);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (sfmmup->sfmmu_lttecnt == 0 || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without unloading so we assume it
			 * has already been unloaded
			 */
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}
	cpuset = sfmmup->sfmmu_cpusran;
	xt_sync(cpuset);
}

static caddr_t
sfmmu_hblk_sync(struct hat *sfmmup, struct hme_blk *hmeblkp, caddr_t addr,
	caddr_t endaddr, int clearflag)
{
	tte_t	tte, ttemod;
	struct sf_hment *sfhmep;
	int ttesz;
	struct machpage *pp;
	kmutex_t *pml;
	int ret;

	ASSERT(hmeblkp->hblk_shw_bit == 0);

	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));

	ttesz = get_hblk_ttesz(hmeblkp);
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);

	while (addr < endaddr) {
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			pml = NULL;
			pp = sfhmep->hme_page;
			if (pp) {
				pml = sfmmu_mlist_enter(pp);
			}
			if (pp != sfhmep->hme_page) {
				/*
				 * tte most have been unloaded
				 * underneath us.  Recheck
				 */
				ASSERT(pml);
				sfmmu_mlist_exit(pml);
				continue;
			}
			if (clearflag == HAT_SYNC_ZERORM) {
				ttemod = tte;
				TTE_CLR_RM(&ttemod);
				ret = sfmmu_modifytte_try(&tte, &ttemod,
				    &sfhmep->hme_tte);
				if (ret < 0) {
					if (pml) {
						sfmmu_mlist_exit(pml);
					}
					continue;
				}

				if (ret > 0) {
					sfmmu_tlb_demap(addr, sfmmup,
						hmeblkp, 0, 0);
				}
			}
			sfmmu_ttesync(sfmmup, addr, &tte, pp);
			if (pml) {
				sfmmu_mlist_exit(pml);
			}
		}
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * This function will sync a tte to the page struct and it will
 * update the hat stats. Currently it allows us to pass a NULL pp
 * and we will simply update the stats.  We may want to change this
 * so we only keep stats for pages backed by pp's.
 */
static void
sfmmu_ttesync(struct hat *sfmmup, caddr_t addr, tte_t *ttep, machpage_t *pp)
{
	uint_t rm = 0;
	int   	sz;
	pgcnt_t	npgs;
	kmutex_t *pmtx;

	ASSERT(TTE_IS_VALID(ttep));

	if (TTE_IS_NOSYNC(ttep)) {
		return;
	}

	if (TTE_IS_REF(ttep))  {
		rm = P_REF;
	}
	if (TTE_IS_MOD(ttep))  {
		rm |= P_MOD;
	}
	if (rm && sfmmup->sfmmu_rmstat) {
		hat_setstat(sfmmup->sfmmu_as, addr, MMU_PAGESIZE, rm);
	}
	/*
	 * XXX I want to use cas to update nrm bits but they
	 * currently belong in common/vm and not in hat where
	 * they should be.
	 * The nrm bits are protected by the same mutex as
	 * the one that protects the page's mapping list.
	 */
	if (!pp)
		return;
	/*
	 * If the tte is for a large page, we need to sync all the
	 * pages covered by the tte.
	 */
	sz = ttep->tte_size;
	if (sz != TTE8K) {
		ASSERT(pp->p_cons > 0);
		pp = PP_GROUPLEADER(pp, sz);
	}

	/* Get number of pages from tte size. */
	npgs = TTEPAGES(sz);

	do {
		ASSERT(pp);
		ASSERT(sfmmu_mlist_held(pp));
		pmtx = sfmmu_page_enter(pp);
		if ((rm == P_REF) && !PP_ISREF(pp)) {
			PP_SETREF(pp);
		} else if ((rm == P_MOD) && !PP_ISMOD(pp)) {
			PP_SETMOD(pp);
		} else if ((rm == (P_REF | P_MOD)) &&
		    (!PP_ISREF(pp) || !PP_ISMOD(pp))) {
			PP_SETREFMOD(pp);
		}
		sfmmu_page_exit(pmtx);

		/*
		 * Are we done? If not, we must have a large mapping.
		 * For large mappings we need to sync the rest of the pages
		 * covered by this tte; goto the next page.
		 */
	} while (--npgs > 0 && (pp = PP_PAGENEXT(pp)));
}

/*
 * Remove all mappings to page 'pp'.
 * XXXmh support forceflag
 */
/* ARGSUSED */
int
hat_pageunload(struct page *gen_pp, uint_t forceflag)
{
	struct machpage *pp = PP2MACHPP(gen_pp);
#ifdef DEBUG
	struct machpage *origpp = pp;
#endif /* DEBUG */
	struct sf_hment *sfhme, *tmphme = NULL;
	kmutex_t *pml, *pmtx;
	cpuset_t cpuset, tset;
	int	index, cons;

	ASSERT(PAGE_LOCKED(gen_pp));

	CPUSET_ZERO(cpuset);

	pml = sfmmu_mlist_enter(pp);
	index = PP_MAPINDEX(pp);
	cons = TTE8K;
retry:

	for (sfhme = pp->p_mapping; sfhme; sfhme = tmphme) {
		tmphme = sfhme->hme_next;
		tset = sfmmu_pageunload(pp, sfhme, cons);
		CPUSET_OR(cpuset, tset);
	}
	while (index != 0) {
		index = index >> 1;
		if (index != 0)
			cons++;
		if (index & 0x1) {
			/* Go to leading page */
			pp = PP_GROUPLEADER(pp, cons);
			ASSERT(sfmmu_mlist_held(pp));
			goto retry;
		}
	}
	xt_sync(cpuset);

	ASSERT(!PP_ISMAPPED(origpp));

	if (PP_ISTNC(pp)) {
		if (cons == TTE8K) {
			pmtx = sfmmu_page_enter(pp);
			PP_CLRTNC(pp);
			sfmmu_page_exit(pmtx);
		} else {
			conv_tnc(pp, cons);
		}
	}

	sfmmu_mlist_exit(pml);
	return (0);
}

static cpuset_t
sfmmu_pageunload(machpage_t *pp, struct sf_hment *sfhme, int cons)
{
	struct hme_blk *hmeblkp;
	sfmmu_t *sfmmup;
	tte_t tte, ttemod;
#ifdef DEBUG
	tte_t orig_old;
#endif DEBUG
	caddr_t addr;
	int ttesz;
	int ret;
	cpuset_t cpuset;

	ASSERT(pp != NULL);
	ASSERT(sfmmu_mlist_held(pp));

	CPUSET_ZERO(cpuset);

	hmeblkp = sfmmu_hmetohblk(sfhme);

readtte:
	sfmmu_copytte(&sfhme->hme_tte, &tte);
	if (TTE_IS_VALID(&tte)) {
		sfmmup = hblktosfmmu(hmeblkp);
		ttesz = get_hblk_ttesz(hmeblkp);
		/*
		 * Only unload mappings of 'cons' size.
		 */
		if (ttesz != cons)
			return (cpuset);

		/*
		 * Note that we have p_mapping lock, but no hash lock here.
		 * hblk_unload() has to have both hash lock AND p_mapping
		 * lock before it tries to modify tte. So, the tte could
		 * not become invalid in the sfmmu_modifytte_try() below.
		 */
		ttemod = tte;
#ifdef DEBUG
		orig_old = tte;
#endif DEBUG
		TTE_SET_INVALID(&ttemod);
		ret = sfmmu_modifytte_try(&tte, &ttemod, &sfhme->hme_tte);
		if (ret < 0) {
#ifdef DEBUG
			/* only R/M bits can change. */
			chk_tte(&orig_old, &tte, &ttemod, hmeblkp);
#endif DEBUG
			goto readtte;
		}

		if (ret == 0) {
			panic("pageunload: cas failed?");
		}

		addr = tte_to_vaddr(hmeblkp, tte);

		sfmmu_ttesync(sfmmup, addr, &tte, pp);

		/*
		 * We will not decrement the rss or lttecnt on
		 * the dummy as when we pageunload ism pages.
		 * They will only be decremented when the ism page
		 * is destroyed.
		 */
		if (sfmmup->sfmmu_ismhat == 0) {
			if (ttesz == TTE8K) {
				atomic_add_32(&sfmmup->sfmmu_rss, -1);
			} else {
				atomic_add_32(&sfmmup->sfmmu_lttecnt, -1);
			}
		}

		/*
		 * We need to flush the page from the virtual cache
		 * in order to prevent a virtual cache alias
		 * inconsistency. The particular scenario we need
		 * to worry about is:
		 * Given:  va1 and va2 are two virtual address that
		 * alias and will map the same physical address.
		 * 1.	mapping exists from va1 to pa and data has
		 *	been read into the cache.
		 * 2.	unload va1.
		 * 3.	load va2 and modify data using va2.
		 * 4	unload va2.
		 * 5.	load va1 and reference data.  Unless we flush
		 *	the data cache when we unload we will get
		 *	stale data.
		 * This scenario is taken care of by using virtual
		 * page coloring.
		 */
		if (sfmmup->sfmmu_ismhat) {
			/*
			 * Flush tsb's, tlb's and caches
			 * of every process
			 * sharing this ism segment.
			 */
			mutex_enter(&ctx_lock);
			mutex_enter(&ism_mlist_lock);
			kpreempt_disable();
			if (do_virtual_coloring)
				sfmmu_ismtlbcache_demap(addr, sfmmup, hmeblkp,
					pp->p_pagenum, CACHE_NO_FLUSH);
			else
				sfmmu_ismtlbcache_demap(addr, sfmmup, hmeblkp,
					pp->p_pagenum, CACHE_FLUSH);
			kpreempt_enable();
			mutex_exit(&ism_mlist_lock);
			mutex_exit(&ctx_lock);
			cpuset = cpu_ready_set;
		} else if (do_virtual_coloring) {
			sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0, 0);
			cpuset = sfmmup->sfmmu_cpusran;
		} else {
			sfmmu_tlbcache_demap(addr, sfmmup, hmeblkp,
				pp->p_pagenum, 0, FLUSH_NECESSARY_CPUS,
				CACHE_FLUSH, 0);
			cpuset = sfmmup->sfmmu_cpusran;
		}

		/*
		 * Hme_sub has to run after ttesync() and a_rss update.
		 * See hblk_unload().
		 */
		hme_sub(sfhme, pp);
		membar_stst();

		/*
		 * We can not make ASSERT(hmeblkp->hblk_hmecnt <= NHMENTS)
		 * since pteload may have done a hme_add() right after
		 * we did the hme_sub() above. Hmecnt is now maintained
		 * by cas only. no lock guranteed its value. The only
		 * gurantee we have is the hmecnt should not be less than
		 * what it should be so the hblk will not be taken away.
		 *
		 * It's also important that we decremented the hmecnt after
		 * we are done with hmeblkp so that this hmeblk won't be
		 * stolen.
		 */
		ASSERT(hmeblkp->hblk_hmecnt > 0);
		ASSERT(hmeblkp->hblk_vcnt > 0);
		atomic_add_16(&hmeblkp->hblk_vcnt, -1);
		atomic_add_16(&hmeblkp->hblk_hmecnt, -1);
		/*
		 * This is bug 4063182.
		 * XXX: fixme
		 * ASSERT(hmeblkp->hblk_hmecnt || hmeblkp->hblk_vcnt ||
		 *	!hmeblkp->hblk_lckcnt);
		 */
	} else {
		panic("invalid tte? pp %p &tte %p",
		    (void *)pp, (void *)&tte);
	}

	return (cpuset);
}


uint_t
hat_pagesync(struct page *gen_pp, uint_t clearflag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);
	struct sf_hment *sfhme, *tmphme = NULL;
	kmutex_t *pml, *pmtx;
	cpuset_t cpuset, tset;
	int	index, cons;
	extern	ulong_t po_share;

	CPUSET_ZERO(cpuset);

	if (PP_ISRO(pp) && (clearflag & HAT_SYNC_STOPON_MOD)) {
		return (PP_GENERIC_ATTR(pp));
	}

	if ((clearflag == (HAT_SYNC_STOPON_REF | HAT_SYNC_DONTZERO)) &&
	    PP_ISREF(pp)) {
		return (PP_GENERIC_ATTR(pp));
	}

	if ((clearflag == (HAT_SYNC_STOPON_MOD | HAT_SYNC_DONTZERO)) &&
	    PP_ISMOD(pp)) {
		return (PP_GENERIC_ATTR(pp));
	}

	if ((pp->p_share > po_share) &&
	    !(clearflag & HAT_SYNC_ZERORM)) {
		if (PP_ISRO(pp)) {
			pmtx = sfmmu_page_enter(pp);
			PP_SETREF(pp);
			sfmmu_page_exit(pmtx);
		}
		return (PP_GENERIC_ATTR(pp));
	}

	pml = sfmmu_mlist_enter(pp);
	index = PP_MAPINDEX(pp);
	cons = TTE8K;
retry:
	for (sfhme = pp->p_mapping; sfhme; sfhme = tmphme) {
		/*
		 * We need to save the next hment on the list since
		 * it is possible for pagesync to remove an invalid hment
		 * from the list.
		 */
		tmphme = sfhme->hme_next;
		/*
		 * If we are looking for large mappings and this hme doesn't
		 * reach the range we are seeking, just ignore its.
		 */
		if (hme_size(sfhme) < cons)
			continue;
		tset = sfmmu_pagesync(pp, sfhme,
			clearflag & ~HAT_SYNC_STOPON_RM);
		CPUSET_OR(cpuset, tset);
		/*
		 * If clearflag is HAT_SYNC_DONTZERO, break out as soon
		 * as the "ref" or "mod" is set.
		 */
		if ((clearflag & ~HAT_SYNC_STOPON_RM) == HAT_SYNC_DONTZERO &&
		    ((clearflag & HAT_SYNC_STOPON_MOD) && PP_ISMOD(pp)) ||
		    ((clearflag & HAT_SYNC_STOPON_REF) && PP_ISREF(pp))) {
			index = 0;
			break;
		}
	}

	while (index) {
		index = index >> 1;
		cons++;
		if (index & 0x1) {
			/* Go to leading page */
			pp = PP_GROUPLEADER(pp, cons);
			goto retry;
		}
	}

	xt_sync(cpuset);
	sfmmu_mlist_exit(pml);
	return (PP_GENERIC_ATTR(pp));
}

/*
 * Get all the hardware dependent attributes for a page struct
 */
static cpuset_t
sfmmu_pagesync(struct machpage *pp, struct sf_hment *sfhme,
	uint_t clearflag)
{
	caddr_t addr;
	tte_t tte, ttemod;
	struct hme_blk *hmeblkp;
	int ret;
	sfmmu_t *sfmmup;
	cpuset_t cpuset;

	ASSERT(pp != NULL);
	ASSERT(sfmmu_mlist_held(pp));
	ASSERT((clearflag == HAT_SYNC_DONTZERO) ||
		(clearflag == HAT_SYNC_ZERORM));

	SFMMU_STAT(sf_pagesync);

	CPUSET_ZERO(cpuset);

sfmmu_pagesync_retry:

	sfmmu_copytte(&sfhme->hme_tte, &tte);
	if (TTE_IS_VALID(&tte)) {
		hmeblkp = sfmmu_hmetohblk(sfhme);
		sfmmup = hblktosfmmu(hmeblkp);
		addr = tte_to_vaddr(hmeblkp, tte);
		if (clearflag == HAT_SYNC_ZERORM) {
			ttemod = tte;
			TTE_CLR_RM(&ttemod);
			ret = sfmmu_modifytte_try(&tte, &ttemod,
				&sfhme->hme_tte);
			if (ret < 0) {
				/*
				 * cas failed and the new value is not what
				 * we want.
				 */
				goto sfmmu_pagesync_retry;
			}

			if (ret > 0) {
				/* we win the cas */
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0, 0);
				cpuset = sfmmup->sfmmu_cpusran;
			}
		}

		sfmmu_ttesync(sfmmup, addr, &tte, pp);
	}
	return (cpuset);
}

void
hat_page_setattr(page_t *gen_pp, uint_t flag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);
	kmutex_t *pmtx;

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	if ((pp->p_nrm & flag) == flag) {
		/* attribute already set */
		return;
	}
	pmtx = sfmmu_page_enter(pp);
	pp->p_nrm |= flag;
	sfmmu_page_exit(pmtx);
}
void
hat_page_clrattr(page_t *gen_pp, uint_t flag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);
	kmutex_t *pmtx;

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	pmtx = sfmmu_page_enter(pp);
	pp->p_nrm &= ~flag;
	sfmmu_page_exit(pmtx);
}

uint_t
hat_page_getattr(page_t *gen_pp, uint_t flag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));
	return ((uint_t)(pp->p_nrm & flag));
}

/*
 * Returns a page frame number for a given user virtual address.
 * Returns PFN_INVALID to indicate an invalid mapping
 */
pfn_t
hat_getpfnum(struct hat *hat, caddr_t addr)
{
	/*
	 * We would like to
	 * ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	 * but we can't because the iommu driver will call this
	 * routine at interrupt time and it can't grab the as lock
	 * or it will deadlock: A thread could have the as lock
	 * and be waiting for io.  The io can't complete
	 * because the interrupt thread is blocked trying to grab
	 * the as lock.
	 */
	if (hat == ksfmmup) {
		return (sfmmu_vatopfn(addr, hat));
	} else {
		return (sfmmu_uvatopfn(addr, hat));
	}
}

pfn_t
sfmmu_uvatopfn(caddr_t vaddr, struct hat *sfmmup)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp = NULL;

	struct sf_hment *sfhmep;
	tte_t tte;
	pfn_t pfn;

	/* support for ISM */
	ism_map_t	*ism_map;
	ism_blk_t	*ism_blkp;
	int		i;
	kmutex_t	*sfmmulock = NULL;
	sfmmu_t *ism_hatid = NULL;


	ASSERT(sfmmup != ksfmmup);
	SFMMU_STAT(sf_user_vtop);
	/*
	 * Set ism_hatid if vaddr falls in a ISM segment.
	 */
	ism_blkp = sfmmup->sfmmu_iblk;
	if (ism_blkp) {
		sfmmulock = &sfmmup->sfmmu_mutex;
		mutex_enter(sfmmulock);
	}
	while (ism_blkp && ism_hatid == NULL) {
		ism_map = ism_blkp->iblk_maps;
		for (i = 0; ism_map[i].imap_ismhat && i < ISM_MAP_SLOTS; i++) {
			if (vaddr >= ism_start(ism_map[i]) &&
			    vaddr < ism_end(ism_map[i])) {
				sfmmup = ism_hatid = ism_map[i].imap_ismhat;
				vaddr = (caddr_t)(vaddr -
					ism_start(ism_map[i]));
				break;
			}
		}
		ism_blkp = ism_blkp->iblk_next;
	}
	if (sfmmulock) {
		mutex_exit(sfmmulock);
	}

	hblktag.htag_id = sfmmup;
	do {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_FAST_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			sfhmep = sfmmu_hblktohme(hmeblkp, vaddr, 0);
			sfmmu_copytte(&sfhmep->hme_tte, &tte);
			if (TTE_IS_VALID(&tte)) {
				pfn = TTE_TO_PFN(vaddr, &tte);
			} else {
				pfn = PFN_INVALID;
			}
			SFMMU_HASH_UNLOCK(hmebp);
			return (pfn);
		}
		SFMMU_HASH_UNLOCK(hmebp);
		hashno++;
	} while (sfmmup->sfmmu_lttecnt && (hashno <= MAX_HASHCNT));
	return (PFN_INVALID);
}


/*
 * For compatability with AT&T and later optimizations
 */
/* ARGSUSED */
void
hat_map(struct hat *hat, caddr_t addr, size_t len, uint_t flags)
{
	ASSERT(hat != NULL);
}

/*
 * Return the number of mappings to a particular page.
 * This number is an approximation of the number of
 * number of people sharing the page.
 */
ulong_t
hat_page_getshare(page_t *gen_pp)
{
	int sz, index;
	machpage_t *pp = PP2MACHPP(gen_pp);
	ulong_t	cnt;
	kmutex_t *pml;

	/*
	 * We need to grab the mlist lock to make sure any outstanding
	 * load/unloads complete.  Otherwise we could return zero
	 * even though the unload(s) hasn't finished yet.
	 */
	pml = sfmmu_mlist_enter(pp);

	if (!PP_ISMAPPED_LARGE(pp)) {
		cnt = pp->p_share;
		sfmmu_mlist_exit(pml);
		return (cnt);
	}


	/*
	 * If we have a large mapping, we count the number of
	 * mappings that this large page is part.
	 */
	ASSERT(pp->p_cons > 0);
	sz = TTE8K;
	cnt = pp->p_share;
	index = PP_MAPINDEX(pp);
	while (index) {
		index >>= 1;
		sz++;
		if (index & 0x1) {
			pp = PP_GROUPLEADER(pp, sz);
			cnt += pp->p_share;
		}
	}
	sfmmu_mlist_exit(pml);
	return (cnt);
}

/*
 * Yield the memory claim requirement for an address space.
 *
 * This is currently implemented as the number of bytes that have active
 * hardware translations that have page structures.  Therefore, it can
 * underestimate the traditional resident set size, eg, if the
 * physical page is present and the hardware translation is missing;
 * and it can overestimate the rss, eg, if there are active
 * translations to a frame buffer with page structs.
 * Also, it does not take sharing into account.
 */
size_t
hat_get_mapped_size(struct hat *hat)
{
	/*
	 * XXX: FIX this code when 64K and 512K are supported.
	 */
	if (hat != NULL)
		return (ptob((pgcnt_t)hat->sfmmu_rss +
			(pgcnt_t)hat->sfmmu_lttecnt * TTEPAGES(TTE4M)));
	else
		return (0);
}

int
hat_stats_enable(struct hat *hat)
{
	mutex_enter(&hat->sfmmu_mutex);
	hat->sfmmu_rmstat++;
	mutex_exit(&hat->sfmmu_mutex);
	return (1);
}

void
hat_stats_disable(struct hat *hat)
{
	mutex_enter(&hat->sfmmu_mutex);
	hat->sfmmu_rmstat--;
	mutex_exit(&hat->sfmmu_mutex);
}

/*
 * Routines for entering or removing  ourselves from the
 * ism_hat's mapping list.
 */
static void
iment_add(struct ism_ment *iment,  struct hat *ism_hat)
{
	ASSERT(MUTEX_HELD(&ism_mlist_lock));

	iment->iment_prev = NULL;
	iment->iment_next = ism_hat->sfmmu_iment;
	if (ism_hat->sfmmu_iment) {
		ism_hat->sfmmu_iment->iment_prev = iment;
	}
	ism_hat->sfmmu_iment = iment;
}

static void
iment_sub(struct ism_ment *iment, struct hat *ism_hat)
{
	ASSERT(MUTEX_HELD(&ism_mlist_lock));

	if (ism_hat->sfmmu_iment == NULL) {
		panic("ism map entry remove - no entries");
	}

	if (iment->iment_prev) {
		ASSERT(ism_hat->sfmmu_iment != iment);
		iment->iment_prev->iment_next = iment->iment_next;
	} else {
		ASSERT(ism_hat->sfmmu_iment == iment);
		ism_hat->sfmmu_iment = iment->iment_next;
	}

	if (iment->iment_next) {
		iment->iment_next->iment_prev = iment->iment_prev;
	}

	/*
	 * zero out the entry
	 */
	iment->iment_next = NULL;
	iment->iment_prev = NULL;
	iment->iment_hat =  NULL;
}

/*
 * Hat_share()/unshare() return an (non-zero) error
 * when saddr and daddr are not properly aligned.
 *
 * The top level mapping element determines the alignment
 * requirement for saddr and daddr, depending on different
 * architectures.
 *
 * When hat_share()/unshare() are not supported,
 * HATOP_SHARE()/UNSHARE() return 0
 */
int
hat_share(struct hat *sfmmup, caddr_t addr,
	struct hat *ism_hatid, caddr_t sptaddr, size_t len)
{
	struct ctx	*ctx;
	ism_blk_t	*ism_blkp;
	ism_map_t 	*ism_map;
	ism_ment_t	*ism_ment;
	int		i, added;
	size_t		sh_size = ISM_SHIFT(len);
#ifdef DEBUG
	caddr_t		eaddr = addr + len;
#endif /* DEBUG */

	ASSERT(ism_hatid != NULL && sfmmup != NULL);
	ASSERT(sptaddr == ISMID_STARTADDR);
	/*
	 * Check the alignment.
	 */
	if (! ISM_ALIGNED(addr) || ! ISM_ALIGNED(sptaddr))
		return (EINVAL);

	/*
	 * Check size alignment.
	 */
	if (! ISM_ALIGNED(len))
		return (EINVAL);

	mutex_enter(&sfmmup->sfmmu_mutex);
	/*
	 * Make sure that during the time ism-mappings are setup, this
	 * process doesn't allow it's context to be stolen.
	 */
	sfmmu_disallow_ctx_steal(sfmmup);
	ctx = sfmmutoctx(sfmmup);

	/*
	 * Allocate an ism map blk if necessary.
	 */
	if (sfmmup->sfmmu_iblk == NULL) {
		ASSERT(ctx->c_ismblkpa == (uint64_t)-1);
		sfmmup->sfmmu_iblk = kmem_cache_alloc(ism_blk_cache, KM_SLEEP);
		bzero(sfmmup->sfmmu_iblk, sizeof (*sfmmup->sfmmu_iblk));
		ctx->c_ismblkpa = va_to_pa((caddr_t)sfmmup->sfmmu_iblk);
	}
	/*
	 * Allocate ism_ment for the ism_hat's mapping list.
	 */
	ism_ment = kmem_cache_alloc(ism_ment_cache, KM_SLEEP);

#ifdef DEBUG
	/*
	 * Make sure mapping does not already exist.
	 */
	ism_blkp = sfmmup->sfmmu_iblk;
	while (ism_blkp) {
		ism_map = ism_blkp->iblk_maps;
		for (i = 0; i < ISM_MAP_SLOTS && ism_map[i].imap_ismhat; i++) {
			if ((addr >= ism_start(ism_map[i]) &&
			    addr < ism_end(ism_map[i])) ||
			    eaddr > ism_start(ism_map[i]) &&
			    eaddr <= ism_end(ism_map[i])) {
				panic("sfmmu_share: Already mapped!");
			}
		}
		ism_blkp = ism_blkp->iblk_next;
	}
#endif /* DEBUG */

	/*
	 * Add mapping to first available mapping slot.
	 */
	ism_blkp = sfmmup->sfmmu_iblk;
	added = 0;
	while (!added) {
		ism_map = ism_blkp->iblk_maps;
		for (i = 0; i < ISM_MAP_SLOTS; i++)  {
			if (ism_map[i].imap_ismhat == NULL) {

				ism_map[i].imap_ismhat = ism_hatid;
				ism_map[i].imap_seg = (uintptr_t)addr | sh_size;
				ism_map[i].imap_ment = ism_ment;

				/*
				 * Now add ourselves to the ism_hat's
				 * mapping list.
				 */
				ism_ment->iment_hat = sfmmup;
				ism_ment->iment_map = &ism_map[i];
				ism_hatid->sfmmu_ismhat = 1;
				mutex_enter(&ism_mlist_lock);
				iment_add(ism_ment, ism_hatid);
				mutex_exit(&ism_mlist_lock);
				added = 1;
				break;
			}
		}
		if (!added && ism_blkp->iblk_next == NULL) {
			ism_blkp->iblk_next = kmem_cache_alloc(ism_blk_cache,
			    KM_SLEEP);
			bzero(ism_blkp->iblk_next,
			    sizeof (*ism_blkp->iblk_next));
		}
		ism_blkp = ism_blkp->iblk_next;
	}

	atomic_add_32(&sfmmup->sfmmu_rss, ism_hatid->sfmmu_rss);
	atomic_add_32(&sfmmup->sfmmu_lttecnt, ism_hatid->sfmmu_lttecnt);

	/*
	 * Since the tl=1 tsb miss handler can hash for 4M
	 * entries we need to set this only if we have
	 * large pages.
	 */
	if (sfmmup->sfmmu_lttecnt)
		CTX_SET_LTTES(ctx);

	/*
	 * Now the ctx can be stolen.
	 */
	sfmmu_allow_ctx_steal(sfmmup);
	mutex_exit(&sfmmup->sfmmu_mutex);

	/*
	 * Do we need to expand the tsb size?
	 */
	if (SFMMU_CHECK_TSBSIZE(sfmmup)) {
		sfmmu_expand_tsbsize(sfmmup);
	}

	return (0);
}

/*
 * hat_unshare removes exactly one ism_map from
 * this process's as.  It expects multiple calls
 * to hat_unshare for multiple shm segments.
 */
void
hat_unshare(struct hat *sfmmup, caddr_t addr, size_t len)
{
	ism_map_t 	*ism_map;
	ism_blk_t	*ism_blkp;
	size_t		sh_size = ISM_SHIFT(len);
	int 		found, i;
	int		spt_rss = 0, spt_lttecnt = 0;
	struct hat	*ism_hatid;
	int		cnum;
	struct ctx	*ctx;
	cpuset_t	cpuset;

	ASSERT(ISM_ALIGNED(addr));
	ASSERT(ISM_ALIGNED(len));
	ASSERT(sfmmup != NULL);
	ASSERT(sfmmup != ksfmmup);


	/*
	 * Make sure that during the time ism-mappings are removed, this
	 * process doesn't allow it's context to be stolen.
	 */
	mutex_enter(&sfmmup->sfmmu_mutex);
	sfmmu_disallow_ctx_steal(sfmmup);

	/*
	 * Remove the mapping.
	 *
	 * We can't have any holes in the ism map.
	 * The tsb miss code while searching the ism map will
	 * stop on an empty map slot.  So we must move
	 * everyone past the hole up 1 if any.
	 *
	 * Also empty ism map blks are not freed until the
	 * process exits. This is to prevent a MT race condition
	 * between sfmmu_unshare() and sfmmu_tsb_miss() and
	 * sfmmu_user_vatopfn().
	 */
	found = 0;
	ism_blkp = sfmmup->sfmmu_iblk;
	while (!found && ism_blkp) {
		ism_map = ism_blkp->iblk_maps;
		for (i = 0; i < ISM_MAP_SLOTS; i++) {
			if (addr == ism_start(ism_map[i]) &&
			    sh_size == ism_size(ism_map[i])) {
				found = 1;
				break;
			}
		}
		if (!found)
			ism_blkp = ism_blkp->iblk_next;
	}

	if (found) {
		ism_hatid = ism_map[i].imap_ismhat;
		ASSERT(ism_hatid != NULL);
		ASSERT(ism_hatid->sfmmu_ismhat == 1);
		spt_rss = ism_hatid->sfmmu_rss;
		spt_lttecnt = ism_hatid->sfmmu_lttecnt;

		/*
		 * First remove ourselves from the ism mapping list.
		 */
		mutex_enter(&ism_mlist_lock);
		iment_sub(ism_map[i].imap_ment, ism_hatid);
		mutex_exit(&ism_mlist_lock);
		kmem_cache_free(ism_ment_cache, ism_map[i].imap_ment);

		/*
		 * Now gurantee that any other cpu
		 * that tries to process an ISM miss
		 * will go to tl=0.
		 */
		kpreempt_disable();
		cnum = sfmmutoctxnum(sfmmup);
		ctx = sfmmutoctx(sfmmup);
		ASSERT(ctx->c_sfmmu == sfmmup);
		ctx->c_ismblkpa |= (uint64_t)CTX_ISM_BUSY;
		membar_stld();
		cpuset = sfmmup->sfmmu_cpusran;
		CPUSET_DEL(cpuset, CPU->cpu_id);
		CPUSET_AND(cpuset, cpu_ready_set);
		SFMMU_XCALL_STATS(cpuset, cnum);

		/*
		 * 2 xt_sync()'s are necessary. The second one
		 * ensures that the first xt_sync() has started
		 * execution on other cpus. Therefore old tl>0
		 * tsb miss handlers that missed busy bit update
		 * in ISM_CHECK() can't possibly be running
		 * on those cpus.
		 */
		xt_sync(cpuset);
		xt_sync(cpuset);

		/*
		 * We delete the ism map by copying
		 * the next map over the current one.
		 * We will take the next one in the maps
		 * array or from the next ism_blk.
		 */
		while (ism_blkp) {
			ism_map = ism_blkp->iblk_maps;
			while (i < (ISM_MAP_SLOTS - 1)) {
				ism_map[i] = ism_map[i + 1];
				i++;
			}
			/* i == (ISM_MAP_SLOTS - 1) */
			ism_blkp = ism_blkp->iblk_next;
			if (ism_blkp) {
				ism_map[i] = ism_blkp->iblk_maps[0];
				i = 0;
			} else {
				ism_map[i].imap_seg = 0;
				ism_map[i].imap_ismhat = NULL;
				ism_map[i].imap_ment = NULL;
			}
		}
		membar_stst();

		/*
		 * Clear ism busy bit.
		 */
		ctx->c_ismblkpa &= (uint64_t)~CTX_ISM_BUSY;
		kpreempt_enable();
	}
	sfmmu_allow_ctx_steal(sfmmup);
	mutex_exit(&sfmmup->sfmmu_mutex);

	/*
	 * Now de-map the tsb and tlb's.
	 */
	if (found) {
		sfmmu_tte_unshare(ism_hatid, sfmmup, addr, len);
		atomic_add_32(&sfmmup->sfmmu_rss, -spt_rss);
		atomic_add_32(&sfmmup->sfmmu_lttecnt, -spt_lttecnt);
	}
}

/*
 * This function will invalidate the tsb/tlb for <addr, len> in sfmmup
 * according to the mappings found in ism_hatid.
 */
void
sfmmu_tte_unshare(struct hat *ism_hatid, struct hat *sfmmup, caddr_t realaddr,
	size_t len)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno;
	struct hme_blk *hmeblkp;
	caddr_t endaddr, hblkend, addr = ISMID_STARTADDR;
	int ttesz;

	/*
	 * We need to invalidate the tsb and the tlb for above address
	 * range.  In order to do this efficiently we traverse the dummy as
	 * hmeblks and only invalidate those entries required.  Remember
	 * that the start of the dummy as shm area is always 0.
	 */
	ASSERT(sfmmup != ksfmmup);
	ASSERT(ism_hatid != ksfmmup);

	endaddr = addr + len;
	hblktag.htag_id = ism_hatid;
	hashno = TTE4M;

	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(ism_hatid, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp == NULL) {
			/*
			 * didn't find an hmeblk. skip the appropiate
			 * address range.
			 */
			SFMMU_HASH_UNLOCK(hmebp);
			addr = (caddr_t)roundup((uintptr_t)addr + 1,
				(1 << hmeshift));
			if ((uintptr_t)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((uintptr_t)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		ASSERT(hmeblkp);
		if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
			/*
			 * If the valid count is zero we can skip the range
			 * mapped by this hmeblk.
			 */
			addr = (caddr_t)roundup((uintptr_t)addr + 1,
				get_hblk_span(hmeblkp));
			SFMMU_HASH_UNLOCK(hmebp);
			if ((uintptr_t)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((uintptr_t)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		if (hmeblkp->hblk_shw_bit) {
			/*
			 * If we encounter a shadow hmeblk we know there is
			 * smaller sized hmeblks mapping the same address space.
			 * Decrement the hash size and rehash.
			 */
			hashno--;
			SFMMU_HASH_UNLOCK(hmebp);
			continue;
		}

		/*
		 * We now invalidate the tsb/tlb for all entries in this
		 * hmeblk
		 */
#ifdef DEBUG
		if (get_hblk_ttesz(hmeblkp) != TTE8K &&
		    (endaddr < get_hblk_endaddr(hmeblkp))) {
			panic("sfmmu_tte_unshare: partial unload of big pg");
		}
#endif DEBUG

		hblkend = MIN(endaddr, get_hblk_endaddr(hmeblkp));
		ttesz = get_hblk_ttesz(hmeblkp);

		while (addr < hblkend) {
			sfmmu_tlb_demap(realaddr + (size_t)addr, sfmmup,
			    hmeblkp, sfmmup->sfmmu_free, 0);
			addr += TTEBYTES(ttesz);
		}

		addr = hblkend;

		SFMMU_HASH_UNLOCK(hmebp);
		if ((uintptr_t)addr & MMU_PAGEOFFSET512K) {
			ASSERT(hashno == TTE64K);
			continue;
		}
		if ((uintptr_t)addr & MMU_PAGEOFFSET4M) {
			hashno = TTE512K;
			continue;
		}
		hashno = TTE4M;
	}
	xt_sync(sfmmup->sfmmu_cpusran);
}

/* ARGSUSED */
static int
sfmmu_idcache_constructor(void *buf, void *cdrarg, int kmflags)
{
	sfmmu_t *sfmmup = (sfmmu_t *)buf;

	mutex_init(&sfmmup->sfmmu_mutex, NULL, MUTEX_DEFAULT, NULL);
	return (0);
}

/* ARGSUSED */
static void
sfmmu_idcache_destructor(void *buf, void *cdrarg)
{
	sfmmu_t *sfmmup = (sfmmu_t *)buf;

	mutex_destroy(&sfmmup->sfmmu_mutex);
}

/*
 * setup kmem hmeblks by bzeroing all members and initializing the nextpa
 * field to be the pa of this hmeblk
 */
/* ARGSUSED */
static int
sfmmu_hblkcache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct hme_blk *hmeblkp;

	bzero(buf, (size_t)cdrarg);
	hmeblkp = (struct hme_blk *)buf;
	hmeblkp->hblk_nextpa = va_to_pa((caddr_t)hmeblkp);

#ifdef	HBLK_TRACE
	mutex_init(&hmeblkp->hblk_audit_lock, NULL, MUTEX_DEFAULT, NULL);
#endif	/* HBLK_TRACE */

	return (0);
}

/* ARGSUSED */
static void
sfmmu_hblkcache_destructor(void *buf, void *cdrarg)
{

#ifdef	HBLK_TRACE

	struct hme_blk *hmeblkp;

	hmeblkp = (struct hme_blk *)buf;
	mutex_destroy(&hmeblkp->hblk_audit_lock);

#endif	/* HBLK_TRACE */
}

#define	SFMMU_CACHE_RECLAIM_SCAN_RATIO 8
static int sfmmu_cache_reclaim_scan_ratio = SFMMU_CACHE_RECLAIM_SCAN_RATIO;
/*
 * The kmem allocator will callback into our reclaim routine when the system
 * is running low in memory.  We traverse the hash and free up all unused but
 * still cached hme_blks.  We also traverse the free list and free them up
 * as well.
 */
/*ARGSUSED*/
static void
sfmmu_hblkcache_reclaim(void *cdrarg)
{
	int i;
	uint64_t hblkpa, prevpa, nx_pa;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp, *nx_hblk, *pr_hblk = NULL;
	struct hme_blk *head = NULL;
	static struct hmehash_bucket *uhmehash_reclaim_hand;
	static struct hmehash_bucket *khmehash_reclaim_hand;
	int nhblks;

	hmebp = uhmehash_reclaim_hand;
	if (hmebp == NULL || hmebp > &uhme_hash[UHMEHASH_SZ])
		uhmehash_reclaim_hand = hmebp = uhme_hash;
	uhmehash_reclaim_hand += UHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio;

	for (i = UHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio; i; i--) {
		if (SFMMU_HASH_LOCK_TRYENTER(hmebp) != 0) {
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				nx_hblk = hmeblkp->hblk_next;
				nx_pa = hmeblkp->hblk_nextpa;
				if (!hmeblkp->hblk_vcnt &&
				    !hmeblkp->hblk_hmecnt) {
					sfmmu_hblk_hash_rm(hmebp, hmeblkp,
						prevpa, pr_hblk);
					sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
				} else {
					pr_hblk = hmeblkp;
					prevpa = hblkpa;
				}
				hmeblkp = nx_hblk;
				hblkpa = nx_pa;
			}
			SFMMU_HASH_UNLOCK(hmebp);
		}
		if (hmebp++ == &uhme_hash[UHMEHASH_SZ])
			hmebp = uhme_hash;
	}

	hmebp = khmehash_reclaim_hand;
	if (hmebp == NULL || hmebp > &khme_hash[KHMEHASH_SZ])
		khmehash_reclaim_hand = hmebp = khme_hash;
	khmehash_reclaim_hand += KHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio;

	for (i = KHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio; i; i--) {
		if (SFMMU_HASH_LOCK_TRYENTER(hmebp) != 0) {
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				nx_hblk = hmeblkp->hblk_next;
				nx_pa = hmeblkp->hblk_nextpa;
				if (!hmeblkp->hblk_vcnt &&
				    !hmeblkp->hblk_hmecnt) {
					sfmmu_hblk_hash_rm(hmebp, hmeblkp,
						prevpa, pr_hblk);
					sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
				} else {
					pr_hblk = hmeblkp;
					prevpa = hblkpa;
				}
				hmeblkp = nx_hblk;
				hblkpa = nx_pa;
			}
			SFMMU_HASH_UNLOCK(hmebp);
		}
		if (hmebp++ == &khme_hash[KHMEHASH_SZ])
			hmebp = khme_hash;
	}

	/*
	 * kmem free all dynamically allocated hme_blks beyond threshold
	 */
	if (hblk8_avail > HME8_TRHOLD &&
		hblk8_allocated > hblk8_prealloc_count) {
		HBLK8_FLIST_LOCK();
		/*
		 * subtract the hblks used in hash lists to find out how many
		 * we want to keep in the freelist. keep atleast HME8_TRHOLD
		 */
		nhblks = hblk8_prealloc_count - (hblk8_allocated - hblk8_avail);
		if (nhblks < HME8_TRHOLD)
			nhblks = HME8_TRHOLD;
		if (hblk8_avail > nhblks) {
			/*
			 * keep all the nucleus hmeblks, they are queued up
			 * at the front of the freelist. Also keep atleast
			 * nhblks number of hmeblks.
			 */
			for (i = 1, hmeblkp = hblk8_flist; (hmeblkp &&
			    hmeblkp->hblk_nuc_bit) || (i < nhblks);
				hmeblkp = hmeblkp->hblk_next, i++);
			if (hmeblkp) {
				head = hmeblkp->hblk_next;
				hmeblkp->hblk_next = NULL;
				hblk8_flist_t = hmeblkp;
				hblk8_allocated -= (hblk8_avail - i);
				hblk8_avail = i;
			}
		}
		HBLK8_FLIST_UNLOCK();
		while (head) {
			hmeblkp = head;
			head = head->hblk_next;
			kmem_cache_free(sfmmu8_cache, hmeblkp);
			SFMMU_STAT(sf_hblk8_dfree);
		}
	}

	/*
	 * follow the same method given above
	 */
	if (hblk1_avail > HME1_TRHOLD) {
		HBLK1_FLIST_LOCK();
		if (hblk1_avail > HME1_TRHOLD) {
			for (i = 1, hmeblkp = hblk1_flist; (hmeblkp &&
			    hmeblkp->hblk_nuc_bit) || (i < HME1_TRHOLD);
				hmeblkp = hmeblkp->hblk_next, i++);
			if (hmeblkp) {
				head = hmeblkp->hblk_next;
				hmeblkp->hblk_next = NULL;
				hblk1_flist_t = hmeblkp;
				hblk1_allocated -= (hblk1_avail - i);
				hblk1_avail = i;
			}
		}
		HBLK1_FLIST_UNLOCK();
		while (head) {
			hmeblkp = head;
			head = head->hblk_next;
			kmem_cache_free(sfmmu1_cache, hmeblkp);
			SFMMU_STAT(sf_hblk1_dfree);
		}
	}
#ifdef DEBUG
	sfmmu_check_hblk_flist();
#endif
}

/*
 * sfmmu_get_ppvcolor should become a vm_machdep or hatop interface.
 * same goes for sfmmu_get_addrvcolor().
 *
 * This function will return the virtual color for the specified page. The
 * virtual color corresponds to this page current mapping or its last mapping.
 * It is used by memory allocators to choose addresses with the correct
 * alignment so vac consistency is automatically maintained.  If the page
 * has no color it returns -1.
 */
int
sfmmu_get_ppvcolor(struct machpage *pp)
{
	int color;

	if (!(cache & CACHE_VAC) || PP_NEWPAGE(pp)) {
		return (-1);
	}
	color = PP_GET_VCOLOR(pp);
	ASSERT(color < mmu_btop(shm_alignment));
	return (color);
}

/*
 * This function will return the desired alignment for vac consistency
 * (vac color) given a virtual address.  If no vac is present it returns -1.
 */
int
sfmmu_get_addrvcolor(caddr_t vaddr)
{
	if (cache & CACHE_VAC) {
		return (addr_to_vcolor(vaddr));
	} else {
		return (-1);
	}

}

/*
 * Check for conflicts.
 * A conflict exists if the new and existant mappings do not match in
 * their "shm_alignment fields. If conflicts exist, the existant mappings
 * are flushed unless one of them is locked. If one of them is locked, then
 * the mappings are flushed and converted to non-cacheable mappings.
 */
static void
sfmmu_vac_conflict(struct hat *hat, caddr_t addr, machpage_t *pp)
{
	struct hat *tmphat;
	struct sf_hment *sfhmep, *tmphme = NULL;
	struct hme_blk *hmeblkp;
	int vcolor;
	tte_t tte;

	ASSERT(sfmmu_mlist_held(pp));
	ASSERT(!PP_ISNC(pp));		/* page better be cacheable */

	vcolor = addr_to_vcolor(addr);
	if (PP_NEWPAGE(pp)) {
		PP_SET_VCOLOR(pp, vcolor);
		return;
	}

	if (PP_GET_VCOLOR(pp) == vcolor) {
		return;
	}

	if (!PP_ISMAPPED(pp)) {
		/*
		 * Previous user of page had a differnet color
		 * but since there are no current users
		 * we just flush the cache and change the color.
		 */
		SFMMU_STAT(sf_pgcolor_conflict);
		sfmmu_cache_flush(pp->p_pagenum, PP_GET_VCOLOR(pp));
		PP_SET_VCOLOR(pp, vcolor);
		return;
	}
	/*
	 * If we get here we have a vac conflict with a current
	 * mapping.  VAC conflict policy is as follows.
	 * - The default is to unload the other mappings unless:
	 * - If we have a large mapping we uncache the page.
	 * We need to uncache the rest of the large page too.
	 * - If any of the mappings are locked we uncache the page.
	 * - If the requested mapping is inconsistent
	 * with another mapping and that mapping
	 * is in the same address space we have to
	 * make it non-cached.  The default thing
	 * to do is unload the inconsistent mapping
	 * but if they are in the same address space
	 * we run the risk of unmapping the pc or the
	 * stack which we will use as we return to the user,
	 * in which case we can then fault on the thing
	 * we just unloaded and get into an infinite loop.
	 */
	if (PP_ISMAPPED_LARGE(pp)) {
		int sz;

		/*
		 * Existing mapping is for big pages. We don't unload
		 * existing big mappings to satisfy new mappings.
		 * Always convert all mappings to TNC.
		 */
		sz = fnd_mapping_sz(pp);
		pp = PP_GROUPLEADER(pp, sz);
		SFMMU_STAT_ADD(sf_uncache_conflict, TTEPAGES(sz));
		sfmmu_page_cache_array(pp, HAT_TMPNC, CACHE_FLUSH,
			TTEPAGES(sz));

		return;
	}

	/*
	 * check if any mapping is in same as or if it is locked
	 * since in that case we need to uncache.
	 */
	for (sfhmep = pp->p_mapping; sfhmep; sfhmep = tmphme) {
		tmphme = sfhmep->hme_next;
		hmeblkp = sfmmu_hmetohblk(sfhmep);
		tmphat = hblktosfmmu(hmeblkp);
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		ASSERT(TTE_IS_VALID(&tte));
		if ((tmphat == hat) || hmeblkp->hblk_lckcnt) {
			/*
			 * We have an uncache conflict
			 */
			SFMMU_STAT(sf_uncache_conflict);
			sfmmu_page_cache_array(pp, HAT_TMPNC, CACHE_FLUSH, 1);
			return;
		}
	}
	/*
	 * We have an unload conflict
	 * We have already checked for LARGE mappings, therefore
	 * the remaining mapping(s) must be TTE8K.
	 */
	SFMMU_STAT(sf_unload_conflict);
	while ((sfhmep = pp->p_mapping) != NULL) {
		(void) sfmmu_pageunload(pp, sfhmep, TTE8K);
	}
	ASSERT(!PP_ISMAPPED(pp));
	/*
	 * unloads only does tlb flushes so we need to flush the
	 * cache here.
	 */
	sfmmu_cache_flush(pp->p_pagenum, PP_GET_VCOLOR(pp));
	PP_SET_VCOLOR(pp, vcolor);
}

/*
 * Whenever a mapping is unloaded and the page is in TNC state,
 * we see if the page can be made cacheable again. 'pp' is
 * the page that we just unloaded a mapping from, the size
 * of mapping that was unloaded is 'ottesz'.
 */
static void
conv_tnc(machpage_t *pp, int ottesz)
{
	int cursz, dosz;
	pgcnt_t curnpgs, dopgs;
	pgcnt_t pg64k;
	machpage_t *pp2;

	/*
	 * Determine how big a range we check for TNC and find
	 * leader page. cursz is the size of the biggest
	 * mapping that still exist on 'pp'.
	 */
	if (PP_ISMAPPED_LARGE(pp)) {
		cursz = fnd_mapping_sz(pp);
	} else {
		cursz = TTE8K;
	}

	if (ottesz >= cursz) {
		dosz = ottesz;
		pp2 = pp;
	} else {
		dosz = cursz;
		pp2 = PP_GROUPLEADER(pp, dosz);
	}

	pg64k = TTEPAGES(TTE64K);
	dopgs = TTEPAGES(dosz);

	ASSERT(dopgs == 1 || ((dopgs & (pg64k - 1)) == 0));

	while (dopgs != 0) {
		curnpgs = TTEPAGES(cursz);
		if (tst_tnc(pp2, curnpgs)) {
			SFMMU_STAT_ADD(sf_recache, curnpgs);
			sfmmu_page_cache_array(pp2, HAT_CACHE, CACHE_NO_FLUSH,
				curnpgs);
		}

		ASSERT(dopgs >= curnpgs);
		dopgs -= curnpgs;

		if (dopgs == 0) {
			break;
		}

		pp2 = PP_PAGENEXT_N(pp2, curnpgs);
		if (((dopgs & (pg64k - 1)) == 0) && PP_ISMAPPED_LARGE(pp2)) {
			cursz = fnd_mapping_sz(pp2);
		} else {
			cursz = TTE8K;
		}
	}
}

/*
 * Returns 1 if page(s) can be converted from TNC to cacheable setting,
 * returns 0 otherwise. Note that oaddr argument is valid for only
 * 8k pages.
 */
static int
tst_tnc(machpage_t *pp, pgcnt_t npages)
{
	struct	sf_hment *sfhme;
	struct	hme_blk *hmeblkp;
	tte_t	tte;
	caddr_t	vaddr;
	int	clr_valid = 0;
	int 	color, color1, bcolor;
	int	i, ncolors;

	ASSERT(pp != NULL);
	ASSERT(!(cache & CACHE_WRITEBACK));

	if (npages > 1) {
		ncolors = CACHE_NUM_COLOR;
	}

	for (i = 0; i < npages; i++) {
		ASSERT(sfmmu_mlist_held(pp));
		ASSERT(PP_ISTNC(pp));
		ASSERT(PP_GET_VCOLOR(pp) == NO_VCOLOR);

		if (PP_ISPNC(pp)) {
			return (0);
		}

		for (sfhme = pp->p_mapping; sfhme; sfhme = sfhme->hme_next) {
			hmeblkp = sfmmu_hmetohblk(sfhme);
			sfmmu_copytte(&sfhme->hme_tte, &tte);
			ASSERT(TTE_IS_VALID(&tte));

			vaddr = tte_to_vaddr(hmeblkp, tte);
			color = addr_to_vcolor(vaddr);

			if (npages > 1) {
				/*
				 * If there is a big mapping, make sure
				 * 8K mapping is consistent with the big
				 * mapping.
				 */
				bcolor = i % ncolors;
				if (color != bcolor) {
					return (0);
				}
			}
			if (!clr_valid) {
				clr_valid = 1;
				color1 = color;
			}

			if (color1 != color) {
				return (0);
			}
		}

		pp = PP_PAGENEXT(pp);
	}

	return (1);
}

static void
sfmmu_page_cache_array(machpage_t *pp, int flags, int cache_flush_flag,
	pgcnt_t npages)
{
	kmutex_t *pmtx;
	int i, ncolors, bcolor;

	ASSERT(pp != NULL);
	ASSERT(!(cache & CACHE_WRITEBACK));

	pmtx = sfmmu_page_enter(pp);

	/*
	 * We need to capture all cpus in order to change cacheability
	 * because we can't allow one cpu to access the same physical
	 * page using a cacheable and a non-cachebale mapping at the same
	 * time. Since we may end up walking the ism mapping list
	 * have to grab it's lock now since we can't after all the
	 * cpus have been captured.
	 */
	mutex_enter(&ctx_lock);
	mutex_enter(&ism_mlist_lock);
	kpreempt_disable();
	xc_attention(cpu_ready_set);

	if (npages > 1) {
		/*
		 * Make sure all colors are flushed since the
		 * sfmmu_page_cache() only flushes one color-
		 * it does not know big pages.
		 */
		ncolors = CACHE_NUM_COLOR;
		if (flags & HAT_TMPNC) {
			for (i = 0; i < ncolors; i++) {
				sfmmu_cache_flushcolor(i, pp->p_pagenum);
			}
			cache_flush_flag = CACHE_NO_FLUSH;
		}
	}

	for (i = 0; i < npages; i++) {

		ASSERT(sfmmu_mlist_held(pp));

		if (!(flags == HAT_TMPNC && PP_ISTNC(pp))) {

			if (npages > 1) {
				bcolor = i % ncolors;
				ASSERT(flags != HAT_TMPNC ||
				    bcolor == PP_GET_VCOLOR(pp));
			} else {
				bcolor = NO_VCOLOR;
			}

			sfmmu_page_cache(pp, flags, cache_flush_flag,
			    bcolor);
		}

		pp = PP_PAGENEXT(pp);
	}

	xt_sync(cpu_ready_set);
	xc_dismissed(cpu_ready_set);
	mutex_exit(&ism_mlist_lock);
	mutex_exit(&ctx_lock);
	sfmmu_page_exit(pmtx);
	kpreempt_enable();
}

/*
 * This function changes the virtual cacheability of all mappings to a
 * particular page.  When changing from uncache to cacheable the mappings will
 * only be changed if all of them have the same virtual color.
 * We need to flush the cache in all cpus.  It is possible that
 * a process referenced a page as cacheable but has sinced exited
 * and cleared the mapping list.  We still to flush it but have no
 * state so all cpus is the only alternative.
 */
static void
sfmmu_page_cache(machpage_t *pp, int flags, int cache_flush_flag, int bcolor)
{
	struct	sf_hment *sfhme;
	struct	hme_blk *hmeblkp;
	sfmmu_t *sfmmup;
	tte_t	tte, ttemod;
	caddr_t	vaddr;
	int	ret, color;
	pfn_t	pfn;

	color = bcolor;
	pfn = pp->p_pagenum;

	for (sfhme = pp->p_mapping; sfhme; sfhme = sfhme->hme_next) {

		hmeblkp = sfmmu_hmetohblk(sfhme);
		sfmmu_copytte(&sfhme->hme_tte, &tte);
		ASSERT(TTE_IS_VALID(&tte));
		vaddr = tte_to_vaddr(hmeblkp, tte);
		color = addr_to_vcolor(vaddr);

#ifdef DEBUG
		if ((flags & HAT_CACHE) && bcolor != NO_VCOLOR) {
			ASSERT(color == bcolor);
		}
#endif

		ASSERT(flags != HAT_TMPNC || color == PP_GET_VCOLOR(pp));

		ttemod = tte;
		if (flags & (HAT_UNCACHE | HAT_TMPNC)) {
			TTE_CLR_VCACHEABLE(&ttemod);
		} else {	/* flags & HAT_CACHE */
			TTE_SET_VCACHEABLE(&ttemod);
		}
		ret = sfmmu_modifytte_try(&tte, &ttemod, &sfhme->hme_tte);
		if (ret < 0) {
			/*
			 * Since all cpus are captured modifytte should not
			 * fail.
			 */
			panic("sfmmu_page_cache: write to tte failed");
		}

		sfmmup = hblktosfmmu(hmeblkp);
		if (cache_flush_flag == CACHE_FLUSH) {
			/*
			 * Flush tsb's, tlb's and caches
			 */
			if (sfmmup->sfmmu_ismhat) {
				if (flags & HAT_CACHE) {
					SFMMU_STAT(sf_ism_recache);
				} else {
					SFMMU_STAT(sf_ism_uncache);
				}
				sfmmu_ismtlbcache_demap(vaddr, sfmmup, hmeblkp,
				    pfn, CACHE_FLUSH);
			} else {
				sfmmu_tlbcache_demap(vaddr, sfmmup, hmeblkp,
				    pfn, 0, FLUSH_ALL_CPUS, CACHE_FLUSH, 1);
			}

			/*
			 * all cache entries belonging to this pfn are
			 * now flushed.
			 */
			cache_flush_flag = CACHE_NO_FLUSH;
		} else {

			/*
			 * Flush only tsb's and tlb's.
			 */
			if (sfmmup->sfmmu_ismhat) {
				if (flags & HAT_CACHE) {
					SFMMU_STAT(sf_ism_recache);
				} else {
					SFMMU_STAT(sf_ism_uncache);
				}
				sfmmu_ismtlbcache_demap(vaddr, sfmmup, hmeblkp,
				    pfn, CACHE_NO_FLUSH);
			} else {
				sfmmu_tlb_demap(vaddr, sfmmup, hmeblkp, 0, 1);
			}
		}
	}

	switch (flags) {

		default:
			panic("sfmmu_pagecache: unknown flags");
			break;

		case HAT_CACHE:
			PP_CLRTNC(pp);
			PP_CLRPNC(pp);
			PP_SET_VCOLOR(pp, color);
			break;

		case HAT_TMPNC:
			PP_SETTNC(pp);
			PP_SET_VCOLOR(pp, NO_VCOLOR);
			break;

		case HAT_UNCACHE:
			PP_SETPNC(pp);
			PP_CLRTNC(pp);
			PP_SET_VCOLOR(pp, NO_VCOLOR);
			break;
	}
}

/*
 * This routine gets called when the system has run out of free contexts.
 * This will simply choose context passed to it to be stolen and reused.
 */
/* ARGSUSED */
static void
sfmmu_reuse_ctx(struct ctx *ctx, sfmmu_t *sfmmup)
{
	sfmmu_t *stolen_sfmmup;
	cpuset_t cpuset;
	ushort_t	cnum = ctxtoctxnum(ctx);

	ASSERT(cnum != KCONTEXT);
	ASSERT(MUTEX_HELD(&ctx_lock));
	ASSERT(ctx->c_refcnt == HWORD_WLOCK);

	/*
	 * simply steal and reuse the ctx passed to us.
	 */
	stolen_sfmmup = ctx->c_sfmmu;
	ASSERT(stolen_sfmmup->sfmmu_cnum == cnum);

	TRACE_CTXS(ctx_trace_ptr, cnum, stolen_sfmmup, sfmmup, CTX_STEAL);
	SFMMU_STAT(sf_ctxsteal);


	/*
	 * Update sfmmu and ctx structs. After this point all threads
	 * belonging to this hat/proc will fault and not use the ctx
	 * being stolen.
	 */
	kpreempt_disable();
	stolen_sfmmup->sfmmu_cnum = INVALID_CONTEXT;
	ctx->c_sfmmu = NULL;
	membar_stld();

	/*
	 * 1. flush TLB in all CPUs that ran the process whose ctx
	 * we are stealing.
	 * 2. change context for all other CPUs to INVALID_CONTEXT,
	 * if they are running in the context that we are going to steal.
	 */
	cpuset = stolen_sfmmup->sfmmu_cpusran;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	CPUSET_AND(cpuset, cpu_ready_set);
	SFMMU_XCALL_STATS(cpuset, cnum);
	xt_some(cpuset, sfmmu_ctx_steal_tl1, cnum, INVALID_CONTEXT);
	xt_sync(cpuset);

	/*
	 * flush tsb of process & tlb of local processor
	 */
	sfmmu_unload_tsbctx(cnum);
	vtag_flushctx(cnum);

	/*
	 * If we just stole the ctx from the current process
	 * on local cpu then we also invalidate his context
	 * here.
	 */
	if (sfmmu_getctx_sec() == cnum)
		sfmmu_setctx_sec(INVALID_CONTEXT);

	kpreempt_enable();
}


/*
 * Returns with context reader lock.
 * We maintain 2 different list of contexts.  The first list
 * is the free list and it is headed by ctxfree.  These contexts
 * are ready to use.  The second list is the dirty list and is
 * headed by ctxdirty. These contexts have been freed but haven't
 * been flushed from the tsb.
 */
static struct ctx *
sfmmu_get_ctx(sfmmu_t *sfmmup)
{
	struct ctx *ctx;
	ushort_t cnum;
	struct ctx *lastctx = &ctxs[nctxs-1];
	struct ctx *firstctx = &ctxs[NUM_LOCKED_CTXS];
	uint_t	found_stealable_ctx;
	uint_t	retry_count = 0;

#define	NEXT_CTX(ctx)   (((ctx) >= lastctx) ? firstctx : ((ctx) + 1))

retry:
	mutex_enter(&ctx_lock);

	/*
	 * Check to see if this process has already got a ctx.
	 * In that case just set the sec-ctx, release ctx_lock and return.
	 */
	if (sfmmup->sfmmu_cnum >= NUM_LOCKED_CTXS) {
		ctx = sfmmutoctx(sfmmup);
		(void) rwlock_hword_enter(&ctx->c_refcnt, READER_LOCK);
		mutex_exit(&ctx_lock);
		return (ctx);
	}

	found_stealable_ctx = 0;
	if ((ctx = ctxfree) != NULL) {
		/*
		 * Found a ctx in free list. Delete it from the list and
		 * use it.
		 */
		SFMMU_STAT(sf_ctxfree);
		ctxfree = ctx->c_free;
	} else if ((ctx = ctxdirty) != NULL) {
		/*
		 * No free contexts.  If we have at least one dirty ctx
		 * then flush tsb on all cpus and move dirty list to
		 * free list.
		 */
		SFMMU_STAT(sf_ctxdirty);
		ctxfree = ctx->c_free;
		ctxdirty = NULL;
	} else {
		/*
		 * no free context available, steal approp ctx.
		 * The policy to choose the aprop context is very simple.
		 * Just sweep all the ctxs using ctxhand. This will steal
		 * the LRU ctx.
		 *
		 * We however only steal a context whose c_refcnt rlock can
		 * be grabbed. Keep searching till we find a stealable ctx.
		 */
		ctx = ctxhand;
		do {
			/*
			 * If you get the writers lock, you can steal this
			 * ctx.
			 */
			if (rwlock_hword_enter(&ctx->c_refcnt, WRITER_LOCK)
				== 0) {
				found_stealable_ctx = 1;
				break;
			}
			ctx = NEXT_CTX(ctx);
		} while (ctx != ctxhand);

		if (found_stealable_ctx) {
			/*
			 * Try and reuse the ctx.
			 */
			sfmmu_reuse_ctx(ctx, sfmmup);

		} else if (retry_count++ < GET_CTX_RETRY_CNT) {
			mutex_exit(&ctx_lock);
			goto retry;

		} else {
			panic("Can't find any stealable context");
		}
	}

	/*
	 * If this sfmmu has an ism-map, setup the ctx struct.
	 */
	if (sfmmup->sfmmu_iblk) {
		ctx->c_ismblkpa = va_to_pa((caddr_t)sfmmup->sfmmu_iblk);
	} else {
		ctx->c_ismblkpa = (uint64_t)-1;
	}

	ctx->c_flags = 0;

	/*
	 * Set up the c_flags field.
	 */
	if (sfmmup->sfmmu_lttecnt) {
		CTX_SET_LTTES(ctx);
	}

	/*
	 * We must set the new tsb before
	 * assigning this ctx to sfmmup passed in.
	 * This is to avoid a race with another thread
	 * on another cpu seeing that this sfmmup has
	 * valid ctx. It could then store tte's into
	 * wrong tsb because of the stale tsb pointer.
	 */
	if (SFMMU_SELECT_TSBSIZE(sfmmup)) {
		ALLOCATE_CTX_TSB512K(ctx);
	} else {
		ALLOCATE_CTX_TSB(ctx);
	}

	ctx->c_sfmmu = sfmmup;		/* clears c_freep at the same time */
	cnum = ctxtoctxnum(ctx);
	sfmmup->sfmmu_cnum = cnum;


	/*
	 * If ctx stolen, release the writers lock.
	 */
	if (found_stealable_ctx)
		rwlock_hword_exit(&ctx->c_refcnt, WRITER_LOCK);

	/*
	 * Set the reader lock.
	 */
	(void) rwlock_hword_enter(&ctx->c_refcnt, READER_LOCK);

	ctxhand = NEXT_CTX(ctx);

	ASSERT(sfmmup == sfmmutoctx(sfmmup)->c_sfmmu);
	mutex_exit(&ctx_lock);

	return (ctx);

#undef NEXT_CTX
}

static void
sfmmu_expand_tsbsize(sfmmu_t *sfmmup)
{
	struct ctx *ctx;
	int	cnum;
	cpuset_t cpuset;
	int	tsbindex;

	ASSERT(sfmmup != ksfmmup);

	/*
	 * Acquire lock.
	 */
	mutex_enter(&ctx_lock);

	ctx = sfmmutoctx(sfmmup);
	cnum = ctxtoctxnum(ctx);

	/*
	 * Recheck flag again. Also need to become
	 * a writer since other readers at tl=0 could
	 * be storing new tte's into this ctx's current
	 * tsb.
	 */
	if (cnum == INVALID_CONTEXT || (ctx->c_flags & LTSB_FLAG) ||
	    rwlock_hword_enter(&ctx->c_refcnt, WRITER_LOCK) != 0) {
		mutex_exit(&ctx_lock);
		if (cnum != INVALID_CONTEXT &&
		    (ctx->c_flags & LTSB_FLAG) == 0)
			SFMMU_STAT(sf_tsb_resize_failures);
		return;
	}

	kpreempt_disable();

	/*
	 * Because we are expanding the TSB from 128K to 512K and each
	 * TSB entry contains 63:22 of the virtual address, we don't have
	 * to worry about aliasing.
	 */
	SFMMU_STAT(sf_tsb_resize);

	/*
	 * save current tsb pointer
	 */
	tsbindex = CTX_GET_TSBINDEX(ctx);

	/*
	 * Allocate a larger tsb size.
	 */
	ALLOCATE_CTX_TSB512K(ctx);

	ASSERT(ctx->c_flags & LTSB_FLAG);
	ASSERT(CTX_GET_TSBINDEX(ctx) < tsb512k_num);
	ASSERT(tsbindex < tsb_num);

	/*
	 * Tell other cpus to update tsbreg if running this context
	 */
	cpuset = sfmmup->sfmmu_cpusran;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	CPUSET_AND(cpuset, cpu_ready_set);
	SFMMU_XCALL_STATS(cpuset, cnum);
	xt_some(cpuset, sfmmu_load_tsbstate_tl1, cnum,
	    (uint64_t)&tsb512k_bases[CTX_GET_TSBINDEX(ctx)]);
	xt_sync(cpuset);

	/*
	 * ONLY re-set local tsb register on behalf of the currently
	 * running process.
	 */
	if (sfmmup == astosfmmu(curthread->t_procp->p_as)) {
		sfmmu_load_tsbstate(cnum);
	}

	/*
	 * Copy and invalidate old tsb.
	 */
	sfmmu_migrate_tsbctx(cnum, &tsb_bases[tsbindex].tsb_reg,
				&tsb512k_bases[CTX_GET_TSBINDEX(ctx)].tsb_reg);
	/*
	 * Release locks.
	 */
	rwlock_hword_exit(&ctx->c_refcnt, WRITER_LOCK);
	mutex_exit(&ctx_lock);

	kpreempt_enable();
}

/*
 * Free up a ctx
 */
static void
sfmmu_free_ctx(sfmmu_t *sfmmup, struct ctx *ctx)
{
	int ctxnum;

	mutex_enter(&ctx_lock);

	TRACE_CTXS(ctx_trace_ptr, sfmmup->sfmmu_cnum, sfmmup,
	    0, CTX_FREE);

	if (sfmmup->sfmmu_cnum == INVALID_CONTEXT) {
		CPUSET_ZERO(sfmmup->sfmmu_cpusran);
		sfmmup->sfmmu_cnum = 0;
		mutex_exit(&ctx_lock);
		return;
	}

	ASSERT(sfmmup == ctx->c_sfmmu);

	FREE_CTX_TSB(ctx);
	ctx->c_ismblkpa = (uint64_t)-1;
	ctx->c_sfmmu = NULL;
	ctx->c_refcnt = 0;
	ctx->c_flags = 0;
	CPUSET_ZERO(sfmmup->sfmmu_cpusran);
	sfmmup->sfmmu_cnum = 0;
	ctxnum = sfmmu_getctx_sec();
	if (ctxnum == ctxtoctxnum(ctx)) {
		sfmmu_setctx_sec(INVALID_CONTEXT);
	}

	/*
	 * Put the freed ctx on the dirty list since tsb needs to be flushed.
	 */
	ctx->c_free = ctxdirty;
	ctxdirty = ctx;

	mutex_exit(&ctx_lock);
}

/*
 * Free up a sfmmu
 * Since the sfmmu is currently embedded in the hat struct we simply zero
 * out our fields and free up the ism map blk list if any.
 */
static void
sfmmu_free_sfmmu(sfmmu_t *sfmmup)
{
	ism_blk_t	*blkp, *nx_blkp;
#ifdef	DEBUG
	ism_map_t	*map;
	int 		i;
#endif

	ASSERT(sfmmup->sfmmu_lttecnt == 0);
	sfmmup->sfmmu_cnum = 0;
	sfmmup->sfmmu_free = 0;
	sfmmup->sfmmu_ismhat = 0;

	blkp = sfmmup->sfmmu_iblk;
	sfmmup->sfmmu_iblk = NULL;

	while (blkp) {
#ifdef	DEBUG
		map = blkp->iblk_maps;
		for (i = 0; i < ISM_MAP_SLOTS; i++) {
			ASSERT(map[i].imap_seg == 0);
			ASSERT(map[i].imap_ismhat == NULL);
			ASSERT(map[i].imap_ment == NULL);
		}
#endif
		nx_blkp = blkp->iblk_next;
		blkp->iblk_next = NULL;
		kmem_cache_free(ism_blk_cache, blkp);
		blkp = nx_blkp;
	}
}

/*
 * Locking primitves accessed by HATLOCK macros
 */
static kmutex_t *
sfmmu_page_enter(struct machpage *pp)
{
	kmutex_t	*spl;

	/* The lock lives in the root page */
	pp = PP_PAGEROOT(pp);
	ASSERT(pp != NULL);

	spl = SPL_HASH(pp);
	mutex_enter(spl);

	return (spl);
}

static void
sfmmu_page_exit(kmutex_t *spl)
{
	mutex_exit(spl);
}

/*
 * Sfmmu internal version of mlist enter/exit.
 */
static kmutex_t *
sfmmu_mlist_enter(struct machpage *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);

	/* The lock lives in the root page */
	pp = PP_PAGEROOT(pp);
	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	mutex_enter(mml);

	return (mml);
}

static void
sfmmu_mlist_exit(kmutex_t *mml)
{
	mutex_exit(mml);
}


static int
sfmmu_mlist_held(struct machpage *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);
	/* The lock lives in the root page */
	pp = PP_PAGEROOT(pp);
	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	return (MUTEX_HELD(mml));
}


/*
 * return a free hmeblk with 8 hments or with 1 hment depending on size.
 * Usually we take from freelist. if we are running low on hmeblks we
 * dynamically allocate more.  We finally call hmeblk stealer if none
 * available. Note that using nucleus hmeblks (placed in the front of
 * of the freelist) reduces the number of tlb misses while in the hat.
 * Of course, an even better rfe would be to modify kmem_alloc so it
 * understands nucleus memory and tries to allocate from it first.
 * This way the kernel tlb miss rate would drop further.
 */
static struct hme_blk *
sfmmu_hblk_alloc(sfmmu_t *sfmmup, caddr_t vaddr,
	struct hmehash_bucket *hmebp, uint_t size, hmeblk_tag hblktag)
{
	struct hme_blk *hmeblkp = NULL;
	struct hme_blk *newhblkp;
	struct hme_blk *shw_hblkp = NULL;
	int hmelock_held = 1;
	uint64_t hblkpa;

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));

	if ((sfmmup != KHATID) && (size < TTE4M)) {
		SFMMU_HASH_UNLOCK(hmebp);
		hmelock_held = 0;
		shw_hblkp = sfmmu_shadow_hcreate(sfmmup, vaddr, size);
	}

	if (size == TTE8K) {
		/*
		 * Allocate more hmeblks if we are running low.
		 * check if the address is in kernel allocatable memory range
		 * so that we don't try to call kmem_alloc while we are called
		 * from kmem_alloc.
		 */
		if ((hblk8_avail <= HME8_TRHOLD) &&
		    (sfmmup != KHATID || hblkalloc_inprog == 0)) {
			if (hmelock_held) {
				SFMMU_HASH_UNLOCK(hmebp);
				hmelock_held = 0;
			}
			hmeblkp = sfmmu_hblk_grow(size,
			    (sfmmup == KHATID) ? KM_NOSLEEP : KM_SLEEP);
		}
		if (hmeblkp == NULL) {
			HBLK8_FLIST_LOCK();
			if (hblk8_avail) {
				hmeblkp = hblk8_flist;
				hblk8_flist = hmeblkp->hblk_next;
				if (--hblk8_avail == 0) {
					hblk8_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk8_inuse, 1);
			}
			HBLK8_FLIST_UNLOCK();
		}
	} else {
		if ((hblk1_avail <= HME1_TRHOLD) &&
		    (sfmmup != KHATID || hblkalloc_inprog == 0)) {
			if (hmelock_held) {
				SFMMU_HASH_UNLOCK(hmebp);
				hmelock_held = 0;
			}
			hmeblkp = sfmmu_hblk_grow(size,
			    (sfmmup == KHATID) ? KM_NOSLEEP : KM_SLEEP);
		}
		if (hmeblkp == NULL) {
			HBLK1_FLIST_LOCK();
			if (hblk1_avail) {
				hmeblkp = hblk1_flist;
				hblk1_flist = hmeblkp->hblk_next;
				if (--hblk1_avail == 0) {
					hblk1_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk1_inuse, 1);
			}
			HBLK1_FLIST_UNLOCK();
		}
	}

	if (hmeblkp == NULL) {
		/*
		 * Could not find a hmeblk in free list, and probably we
		 * could not allocate more. Call hmeblk stealer to get one.
		 */
		if (hmelock_held) {
			SFMMU_HASH_UNLOCK(hmebp);
			hmelock_held = 0;
		}

		hmeblkp = sfmmu_hblk_steal(size);
	}

	/*
	 * make sure hmeblk doesn't cross a page boundary
	 */
	ASSERT(hmeblkp != NULL);
	ASSERT(hmeblkp->hblk_nuc_bit ||
		((((uintptr_t)hmeblkp & MMU_PAGEOFFSET)
		+ (size == TTE8K ? HME8BLK_SZ : HME1BLK_SZ)) <= MMU_PAGESIZE));

	set_hblk_sz(hmeblkp, size);

	if (!hmelock_held) {
		/*
		 * can only do this assert if hash lock is not held because
		 * we could deadlock otherwise.
		 */
		ASSERT(hmeblkp->hblk_nextpa == va_to_pa((caddr_t)hmeblkp));
		SFMMU_HASH_LOCK(hmebp);
		HME_HASH_FAST_SEARCH(hmebp, hblktag, newhblkp);
		if (newhblkp != NULL) {
			sfmmu_hblk_tofreelist(hmeblkp, hmeblkp->hblk_nextpa);
			return (newhblkp);
		}
	}

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
	hmeblkp->hblk_next = (struct hme_blk *)NULL;
	hmeblkp->hblk_tag = hblktag;
	hmeblkp->hblk_shadow = shw_hblkp;
	hblkpa = hmeblkp->hblk_nextpa;
	hmeblkp->hblk_nextpa = 0;

	ASSERT(get_hblk_ttesz(hmeblkp) == size);
	ASSERT(get_hblk_span(hmeblkp) == HMEBLK_SPAN(size));
	ASSERT(hmeblkp->hblk_hmecnt == 0);
	ASSERT(hmeblkp->hblk_vcnt == 0);
	ASSERT(hmeblkp->hblk_lckcnt == 0);
	ASSERT(hblkpa == va_to_pa((caddr_t)hmeblkp));
	sfmmu_hblk_hash_add(hmebp, hmeblkp, hblkpa);
	return (hmeblkp);
}

/*
 * This function performs any cleanup required on the hme_blk
 * and returns it to the free list.
 */
/* ARGSUSED */
static void
sfmmu_hblk_free(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	uint64_t hblkpa)
{
	int shw_size, vshift;
	struct hme_blk *shw_hblkp;
	uint_t		shw_mask, newshw_mask;
	uintptr_t	vaddr;

	ASSERT(hmeblkp);
	ASSERT(!hmeblkp->hblk_hmecnt);
	ASSERT(!hmeblkp->hblk_vcnt);
	ASSERT(!hmeblkp->hblk_lckcnt);
	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
	ASSERT(hblkpa == va_to_pa((caddr_t)hmeblkp));

	shw_hblkp = hmeblkp->hblk_shadow;
	if (shw_hblkp) {
		ASSERT(hblktosfmmu(hmeblkp) != KHATID);
		ASSERT(get_hblk_ttesz(hmeblkp) < TTE4M);

		shw_size = get_hblk_ttesz(shw_hblkp);
		vaddr = get_hblk_base(hmeblkp);
		vshift = vaddr_to_vshift(shw_hblkp->hblk_tag, vaddr, shw_size);
		ASSERT(vshift < 8);
		/*
		 * Atomically clear shadow mask bit
		 */
		do {
			shw_mask = shw_hblkp->hblk_shw_mask;
			ASSERT(shw_mask & (1 << vshift));
			newshw_mask = shw_mask & ~(1 << vshift);
			newshw_mask = cas32(&shw_hblkp->hblk_shw_mask,
				shw_mask, newshw_mask);
		} while (newshw_mask != shw_mask);
		hmeblkp->hblk_shadow = NULL;
	}
	sfmmu_hblk_tofreelist(hmeblkp, hblkpa);
}

/*
 * This function puts an hmeblk back in the freelist, either in hblk8_flist
 * or hblk1_flist based on size. Note that the nucleus hmeblks go to the
 * front of the list and the dynamically allocated ones go to the rear,
 * the nucleus hmeblks are reused frequently (for better tlb hit rate).
 * Also note that we keep the hmeblk pa in the nextpa field in order
 * to save calls to vatopa.
 */
static void
sfmmu_hblk_tofreelist(struct hme_blk *hmeblkp, uint64_t hblkpa)
{
	ASSERT(hmeblkp);
	ASSERT(!hmeblkp->hblk_hmecnt);
	ASSERT(!hmeblkp->hblk_vcnt);
	ASSERT(hmeblkp->hblk_shadow == NULL);

	hmeblkp->hblk_next = NULL;
	hmeblkp->hblk_nextpa = hblkpa;	/* set nextpa field to this hblk pa */
	hmeblkp->hblk_shw_bit = 0;

	if (get_hblk_ttesz(hmeblkp) == TTE8K) {
		ASSERT(hmeblkp->hblk_hme[1].hme_page == NULL);
		ASSERT(hmeblkp->hblk_hme[1].hme_next == NULL);
		ASSERT(!hmeblkp->hblk_hme[1].hme_tte.tte_inthi ||
			hmeblkp->hblk_hme[1].hme_tte.tte_hmenum == 1);

		HBLK8_FLIST_LOCK();
		if (hblk8_avail++ == 0) {
			hblk8_flist = hblk8_flist_t = hmeblkp;
		} else {
			if (hmeblkp->hblk_nuc_bit) {
				hmeblkp->hblk_next = hblk8_flist;
				hblk8_flist = hmeblkp;
			} else {
				hblk8_flist_t->hblk_next = hmeblkp;
				hblk8_flist_t = hmeblkp;
			}
		}
		HBLK_DEBUG_COUNTER_DECR(hblk8_inuse, 1);
		HBLK8_FLIST_UNLOCK();
	} else {
		HBLK1_FLIST_LOCK();
		if (hblk1_avail++ == 0) {
			hblk1_flist = hblk1_flist_t = hmeblkp;
		} else {
			if (hmeblkp->hblk_nuc_bit) {
				hmeblkp->hblk_next = hblk1_flist;
				hblk1_flist = hmeblkp;
			} else {
				hblk1_flist_t->hblk_next = hmeblkp;
				hblk1_flist_t = hmeblkp;
			}
		}
		HBLK_DEBUG_COUNTER_DECR(hblk1_inuse, 1);
		HBLK1_FLIST_UNLOCK();
	}
}

/*
 * dynamically allocate hmeblks. Allocate the first one (probably
 * with KM_SLEEP flag if user allocation), and then the rest and
 * stash them in the free list.
 */
static struct hme_blk *
sfmmu_hblk_grow(int size, int sleep)
{
	struct hme_blk *hmeblkp, *first, *head;
	int i;

	/*
	 * automically record that hmeblk allocation is in progress.
	 */
	atomic_add_32(&hblkalloc_inprog, 1);

	if (size == TTE8K) {
		if ((first = kmem_cache_alloc(sfmmu8_cache, sleep)) != NULL) {
			SFMMU_STAT(sf_hblk8_dalloc);
			hmeblkp = first;
			for (i = 0; i < HBLK_GROW_NUM &&
			    hblk8_avail <= HME8_TRHOLD; i++) {
				if ((hmeblkp->hblk_next = kmem_cache_alloc(
				    sfmmu8_cache, KM_NOSLEEP)) == NULL) {
					break;
				}
				SFMMU_STAT(sf_hblk8_dalloc);
				hmeblkp = hmeblkp->hblk_next;
			}

			HBLK8_FLIST_LOCK();
			if (i) {
				head = first->hblk_next;
				hmeblkp->hblk_next = NULL;
				if (hblk8_avail == 0) {
					hblk8_flist = head;
				} else {
					hblk8_flist_t->hblk_next = head;
				}
				hblk8_flist_t = hmeblkp;
				hblk8_avail += i;
			}
			/*
			 * we have allocated 'i+1' hmeblks, 'i' hmeblks added
			 * in freelist, one (first) will be used in hash list
			 */
			hblk8_allocated += (i + 1);
			HBLK_DEBUG_COUNTER_INCR(hblk8_inuse, 1);
			HBLK8_FLIST_UNLOCK();
			first->hblk_next = NULL;
		}
	} else {
		if ((first = kmem_cache_alloc(sfmmu1_cache, sleep)) != NULL) {
			SFMMU_STAT(sf_hblk1_dalloc);
			hmeblkp = first;
			for (i = 0; i < HBLK_GROW_NUM &&
			    hblk1_avail <= HME1_TRHOLD; i++) {
				if ((hmeblkp->hblk_next = kmem_cache_alloc(
				    sfmmu1_cache, KM_NOSLEEP)) == NULL) {
					break;
				}
				SFMMU_STAT(sf_hblk1_dalloc);
				hmeblkp = hmeblkp->hblk_next;
			}

			HBLK1_FLIST_LOCK();
			if (i) {
				head = first->hblk_next;
				hmeblkp->hblk_next = NULL;
				if (hblk1_avail == 0) {
					hblk1_flist = head;
				} else {
					hblk1_flist_t->hblk_next = head;
				}
				hblk1_flist_t = hmeblkp;
				hblk1_avail += i;
			}
			hblk1_allocated += (i + 1);
			HBLK_DEBUG_COUNTER_INCR(hblk1_inuse, 1);
			HBLK1_FLIST_UNLOCK();
			first->hblk_next = NULL;
		}
	}

	atomic_add_32(&hblkalloc_inprog, -1);

	return (first);
}

#define	BUCKETS_TO_SEARCH_BEFORE_UNLOAD	30

static uint_t sfmmu_hblk_steal_twice;
static uint_t sfmmu_hblk_steal_count, sfmmu_hblk_steal_unload_count;

/*
 * Steal a hmeblk
 * Enough hmeblks were allocated at startup (nucleus hmeblks) and also
 * hmeblks were added dynamically. We should never ever not be able to
 * find one. Look for an unused/unlocked hmeblk in user hash table.
 */
static struct hme_blk *
sfmmu_hblk_steal(int size)
{
	static struct hmehash_bucket *uhmehash_steal_hand = NULL;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp = NULL, *pr_hblk;
	uint64_t hblkpa, prevpa;
	int i;

	for (;;) {
		hmebp = (uhmehash_steal_hand == NULL) ? uhme_hash :
			uhmehash_steal_hand;
		ASSERT(hmebp >= uhme_hash && hmebp <= &uhme_hash[UHMEHASH_SZ]);

		for (i = 0; hmeblkp == NULL && i <= UHMEHASH_SZ +
		    BUCKETS_TO_SEARCH_BEFORE_UNLOAD; i++) {
			SFMMU_HASH_LOCK(hmebp);
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				/*
				 * check if it is a hmeblk that is not locked
				 * and not shared. skip shadow hmeblks with
				 * shadow_mask set i.e valid count non zero.
				 */
				if ((get_hblk_ttesz(hmeblkp) == size) &&
				    (hmeblkp->hblk_shw_bit == 0 ||
					hmeblkp->hblk_vcnt == 0) &&
				    (hmeblkp->hblk_lckcnt == 0)) {
					/*
					 * there is a high probability that we
					 * will find a free one. search some
					 * buckets for a free hmeblk initially
					 * before unloading a valid hmeblk.
					 */
					if ((hmeblkp->hblk_vcnt == 0 &&
					    hmeblkp->hblk_hmecnt == 0) || (i >=
					    BUCKETS_TO_SEARCH_BEFORE_UNLOAD)) {
						if (sfmmu_steal_this_hblk(hmebp,
						    hmeblkp, hblkpa, prevpa,
						    pr_hblk)) {
							/*
							 * Hblk is unloaded
							 * successfully
							 */
							break;
						}
					}
				}
				pr_hblk = hmeblkp;
				prevpa = hblkpa;
				hblkpa = hmeblkp->hblk_nextpa;
				hmeblkp = hmeblkp->hblk_next;
			}

			SFMMU_HASH_UNLOCK(hmebp);
			if (hmebp++ == &uhme_hash[UHMEHASH_SZ])
				hmebp = uhme_hash;
		}
		uhmehash_steal_hand = hmebp;

		if (hmeblkp != NULL)
			break;

		/*
		 * we could not steal a hmeblk from user hash, check the
		 * freelist in case some hmeblk showed up there meanwhile.
		 */
		if (size == TTE8K) {
			HBLK8_FLIST_LOCK();
			if (hblk8_avail) {
				hmeblkp = hblk8_flist;
				hblk8_flist = hmeblkp->hblk_next;
				if (--hblk8_avail == 0) {
					hblk8_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk8_inuse, 1);
			}
			HBLK8_FLIST_UNLOCK();
		} else {
			HBLK1_FLIST_LOCK();
			if (hblk1_avail) {
				hmeblkp = hblk1_flist;
				hblk1_flist = hmeblkp->hblk_next;
				if (--hblk1_avail == 0) {
					hblk1_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk1_inuse, 1);
			}
			HBLK1_FLIST_UNLOCK();
		}

		if (hmeblkp != NULL)
			break;

		/*
		 * in the worst case, look for a free one in the kernel
		 * hash table.
		 */
		for (i = 0, hmebp = khme_hash; i <= KHMEHASH_SZ; i++) {
			SFMMU_HASH_LOCK(hmebp);
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				/*
				 * check if it is free hmeblk
				 */
				if ((get_hblk_ttesz(hmeblkp) == size) &&
				    (hmeblkp->hblk_lckcnt == 0) &&
				    (hmeblkp->hblk_vcnt == 0) &&
				    (hmeblkp->hblk_hmecnt == 0)) {
					if (sfmmu_steal_this_hblk(hmebp,
					    hmeblkp, hblkpa, prevpa, pr_hblk)) {
						break;
					} else {
						/*
						 * Cannot fail since we have
						 * hash lock.
						 */
						panic("fail to steal?");
					}
				}

				pr_hblk = hmeblkp;
				prevpa = hblkpa;
				hblkpa = hmeblkp->hblk_nextpa;
				hmeblkp = hmeblkp->hblk_next;
			}

			SFMMU_HASH_UNLOCK(hmebp);
			if (hmebp++ == &khme_hash[KHMEHASH_SZ])
				hmebp = khme_hash;
		}

		if (hmeblkp != NULL)
			break;
		sfmmu_hblk_steal_twice++;
	}
	return (hmeblkp);
}

/*
 * This routine does real work to prepare a hblk to be "stolen" by
 * unloading the mappings, updating shadow counts ....
 * It returns 1 if the block is ready to be reused (stolen), or 0
 * means the block cannot be stolen yet- pageunload is still working
 * on this hblk.
 */
static int
sfmmu_steal_this_hblk(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	uint64_t hblkpa, uint64_t prevpa, struct hme_blk *pr_hblk)
{
	int shw_size, vshift;
	struct hme_blk *shw_hblkp;
	uintptr_t vaddr;
	uint_t shw_mask, newshw_mask;

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));

	/*
	 * check if the hmeblk is free, unload if necessary
	 */
	if (hmeblkp->hblk_vcnt || hmeblkp->hblk_hmecnt) {
		(void) sfmmu_hblk_unload(hblktosfmmu(hmeblkp), hmeblkp,
		    (caddr_t)get_hblk_base(hmeblkp),
			get_hblk_endaddr(hmeblkp), HAT_UNLOAD);

		if (hmeblkp->hblk_vcnt || hmeblkp->hblk_hmecnt) {
			/*
			 * Pageunload is working on the same hblk.
			 */
			return (0);
		}

		sfmmu_hblk_steal_unload_count++;
	}

	ASSERT(hmeblkp->hblk_lckcnt == 0);
	ASSERT(hmeblkp->hblk_vcnt == 0 && hmeblkp->hblk_hmecnt == 0);

	sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa, pr_hblk);
	hmeblkp->hblk_nextpa = hblkpa;

	shw_hblkp = hmeblkp->hblk_shadow;
	if (shw_hblkp) {
		shw_size = get_hblk_ttesz(shw_hblkp);
		vaddr = get_hblk_base(hmeblkp);
		vshift = vaddr_to_vshift(shw_hblkp->hblk_tag, vaddr, shw_size);
		ASSERT(vshift < 8);
		/*
		 * Atomically clear shadow mask bit
		 */
		do {
			shw_mask = shw_hblkp->hblk_shw_mask;
			ASSERT(shw_mask & (1 << vshift));
			newshw_mask = shw_mask & ~(1 << vshift);
			newshw_mask = cas32(&shw_hblkp->hblk_shw_mask,
				shw_mask, newshw_mask);
		} while (newshw_mask != shw_mask);
		hmeblkp->hblk_shadow = NULL;
	}

	/*
	 * remove shadow bit if we are stealing an unused shadow hmeblk.
	 * sfmmu_hblk_alloc needs it that way, will set shadow bit later if
	 * we are indeed allocating a shadow hmeblk.
	 */
	hmeblkp->hblk_shw_bit = 0;

	sfmmu_hblk_steal_count++;
	SFMMU_STAT(sf_steal_count);

	return (1);
}


/*
 * HME_BLK HASH PRIMITIVES
 */

/*
 * This function returns the hment given the hme_blk and a vaddr.
 * It assumes addr has already been checked to belong to hme_blk's
 * range.  If hmenump is passed then we update it with the index.
 */
static struct sf_hment *
sfmmu_hblktohme(struct hme_blk *hmeblkp, caddr_t addr, int *hmenump)
{
	int index = 0;

	ASSERT(in_hblk_range(hmeblkp, addr));

	if (get_hblk_ttesz(hmeblkp) == TTE8K) {
		index = (((uintptr_t)addr >> MMU_PAGESHIFT) & (NHMENTS-1));
	}

	if (hmenump) {
		*hmenump = index;
	}

	return (&hmeblkp->hblk_hme[index]);
}

static struct hme_blk *
sfmmu_hmetohblk(struct sf_hment *sfhme)
{
	struct hme_blk *hmeblkp;
	struct sf_hment *sfhme0;
	struct hme_blk *hblk_dummy = 0;

	sfhme0 = sfhme - sfhme->hme_tte.tte_hmenum;
	hmeblkp = (struct hme_blk *)((uintptr_t)sfhme0 -
		(uintptr_t)&hblk_dummy->hblk_hme[0]);

	return (hmeblkp);
}

/*
 * Make sure that there is a valid ctx, if not get a ctx.
 * Also, get a readers lock on refcnt, so that the ctx cannot
 * be stolen underneath us.
 */
void
sfmmu_disallow_ctx_steal(sfmmu_t *sfmmup)
{
	struct	ctx *ctx;

	ASSERT(sfmmup != ksfmmup);
	/*
	 * If ctx has been stolen, get a ctx.
	 */
	if (sfmmup->sfmmu_cnum == INVALID_CONTEXT) {
		/*
		 * Our ctx was stolen. Get a ctx with rlock.
		 */
		ctx = sfmmu_get_ctx(sfmmup);
		return;
	} else {
		ctx = sfmmutoctx(sfmmup);
	}

	/*
	 * Try to get the reader lock.
	 */
	if (rwlock_hword_enter(&ctx->c_refcnt, READER_LOCK) == 0) {
		/*
		 * Successful in getting r-lock.
		 * Does ctx still point to sfmmu ?
		 * If NO, the ctx got stolen meanwhile.
		 * 	Release r-lock and try again.
		 * If YES, we are done - just exit
		 */
		if (ctx->c_sfmmu != sfmmup) {
			rwlock_hword_exit(&ctx->c_refcnt, READER_LOCK);
			/*
			 * Our ctx was stolen. Get a ctx with rlock.
			 */
			ctx = sfmmu_get_ctx(sfmmup);
		}
	} else {
		/*
		 * Our ctx was stolen. Get a ctx with rlock.
		 */
		ctx = sfmmu_get_ctx(sfmmup);
	}

	ASSERT(sfmmup->sfmmu_cnum >= NUM_LOCKED_CTXS);
	ASSERT(sfmmutoctx(sfmmup)->c_refcnt > 0);
}

/*
 * Decrement reference count for our ctx. If the reference count
 * becomes 0, our ctx can be stolen by someone.
 */
void
sfmmu_allow_ctx_steal(sfmmu_t *sfmmup)
{
	struct	ctx *ctx;

	ASSERT(sfmmup != ksfmmup);
	ctx = sfmmutoctx(sfmmup);

	ASSERT(ctx->c_refcnt > 0);
	ASSERT(sfmmup == ctx->c_sfmmu);
	ASSERT(sfmmup->sfmmu_cnum != INVALID_CONTEXT);
	rwlock_hword_exit(&ctx->c_refcnt, READER_LOCK);

}


/*
 * TLB Handling Routines
 * These routines get called from the trap vector table.
 * In some cases an optimized assembly handler has already been
 * executed.
 */

/*
 * We get here after we have missed in the TSB or taken a mod bit trap.
 * The TL1 assembly routine passes the contents of the tag access register.
 * Since we are only
 * supporting a 32 bit address space we manage this register as an uint.
 * This routine will try to find the hment for this address in the hment
 * hash and if found it will place the corresponding entry on the TSB and
 * If it fails then we will call trap which will call pagefault.
 * This routine is called via sys_trap and thus, executes at TL0
 */
void
sfmmu_tsb_miss(struct regs *rp, uintptr_t tagaccess, uint_t traptype)
{
	struct hmehash_bucket *hmebp;
	sfmmu_t *sfmmup, *sfmmup_orig;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;

	caddr_t vaddr;
	uint_t ctxnum;
	struct sf_hment *sfhmep;
	struct ctx *ctx;
	tte_t tte, ttemod;
	machpage_t *pp;
	kmutex_t *pml = NULL;

	caddr_t		tmp_vaddr;
	int		i;
	kmutex_t	*sfmmulock = NULL;
	sfmmu_t 	*ism_hatid = NULL;
	ism_blk_t	*ism_blkp = NULL;
	ism_map_t	*ism_map;

	SFMMU_STAT(sf_slow_tsbmiss);
	tmp_vaddr = vaddr = (caddr_t)(tagaccess & TAGACC_VADDR_MASK);
	ctxnum = tagaccess & TAGACC_CTX_MASK;

	/*
	 * Make sure we have a valid ctx and that our context doesn't get
	 * stolen after this point.
	 */
	if (ctxnum == KCONTEXT) {
		extern caddr_t datava;

		/*
		 * It is possible for us to get prot and data misses
		 * on kernel text.  This is because kernel text is currently
		 * marked read only so if we try to write kernel text
		 * (adb -kw) we will end up taking a mod bit fault.  The
		 * mod bit handler will invalidate the text entry in the
		 * dtlb.  Next time we reference the kernel text as data
		 * we will get a data mmu miss as well.  See bugid 4030241.
		 */
		if (((uintptr_t)vaddr & MMU_PAGEMASK4M) ==
		    (uintptr_t)datava) {
			panic("tsb miss on locked kernel tte");
		}

		sfmmup_orig = ksfmmup;
		kpreempt_disable();
	} else {
		sfmmup_orig = astosfmmu(curthread->t_procp->p_as);
		sfmmulock = &sfmmup_orig->sfmmu_mutex;
		mutex_enter(sfmmulock);
		sfmmu_disallow_ctx_steal(sfmmup_orig);
		ctxnum = sfmmup_orig->sfmmu_cnum;
		kpreempt_disable();
		sfmmu_setctx_sec(ctxnum);
	}
	ASSERT(sfmmup_orig == ksfmmup || ctxnum >= NUM_LOCKED_CTXS);

	ctx = ctxnumtoctx(ctxnum);
	sfmmup = ctx->c_sfmmu;
	ASSERT(sfmmup_orig == sfmmup);

	/*
	 * Set ism_hatid if vaddr falls in a ISM segment.
	 */
	ism_blkp = sfmmup_orig->sfmmu_iblk;
	while (ism_blkp && ism_hatid == NULL) {
		ism_map = ism_blkp->iblk_maps;
		for (i = 0; ism_map[i].imap_ismhat && i < ISM_MAP_SLOTS; i++) {
			if (vaddr >= ism_start(ism_map[i]) &&
			    vaddr < ism_end(ism_map[i])) {
				sfmmup = ism_hatid = ism_map[i].imap_ismhat;
				vaddr = (caddr_t)(vaddr -
					ism_start(ism_map[i]));
				break;
			}
		}
		ism_blkp = ism_blkp->iblk_next;
	}
	if (sfmmulock && ism_hatid == NULL) {
		mutex_exit(sfmmulock);
		sfmmulock = NULL;
	}

	hblktag.htag_id = sfmmup;
	do {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_FAST_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			sfhmep = sfmmu_hblktohme(hmeblkp, vaddr, 0);
		again:
			pp = sfhmep->hme_page;
			if (pp) {
				/*
				 * We grab mlist lock to prevent
				 * pageunload from unloading mapping
				 * underneath us
				 */
				pml = sfmmu_mlist_enter(pp);
				/*
				 * Check for a page unload.
				 */
				if (pp != sfhmep->hme_page) {
					sfmmu_mlist_exit(pml);
					goto again;
				}
			} else {
				pml = NULL;
			}
			sfmmu_copytte(&sfhmep->hme_tte, &tte);
			if (TTE_IS_VALID(&tte) &&
			    (TTE_IS_EXECUTABLE(&tte) ||
			    traptype != T_INSTR_MMU_MISS)) {
				ttemod = tte;
				if (traptype == T_DATA_PROT) {
					/*
					 * We don't need to flush our tlb
					 * because we did it in our trap
					 * handler.  We also don't need to
					 * unload our tsb because the new entry
					 * will replace it.
					 */
					if (TTE_IS_WRITABLE(&tte)) {
						TTE_SET_MOD(&ttemod);
					} else {
						if (pml)
							sfmmu_mlist_exit(pml);
						SFMMU_HASH_UNLOCK(hmebp);
						break;
					}
				}
				TTE_SET_REF(&ttemod);
				if (ism_hatid)
					vaddr = tmp_vaddr;

				if (get_hblk_ttesz(hmeblkp) == TTE8K) {
					sfmmu_load_tsb(vaddr, ctxnum, &ttemod);
				} else if (get_hblk_ttesz(hmeblkp) == TTE4M &&
					    sfmmup != KHATID &&
					    traptype != T_INSTR_MMU_MISS) {
					sfmmu_load_tsb4m(vaddr, ctxnum,
					    &ttemod);
				}
				if (traptype == T_INSTR_MMU_MISS) {
					sfmmu_itlb_ld(vaddr, ctxnum, &ttemod);
				} else {
					sfmmu_dtlb_ld(vaddr, ctxnum, &ttemod);
				}
				while (sfmmu_modifytte_try(&tte, &ttemod,
				    &sfhmep->hme_tte) < 0) {
					/* red/mod bits can change */
					;
					ASSERT(TTE_IS_VALID(&sfhmep->hme_tte));
				}
				if (pml)
					sfmmu_mlist_exit(pml);
				SFMMU_HASH_UNLOCK(hmebp);
				/*
				 * This assert can't be before loading the
				 * tsb/tlb or a recursive tlb miss is possible
				 * since the hats are kmemalloced.
				 */
				ASSERT(ism_hatid ||
					(ctxnum == sfmmup->sfmmu_cnum));

				if (sfmmulock) {
					ASSERT(ism_hatid != NULL);
					mutex_exit(sfmmulock);
					sfmmulock = NULL;
				}
				/*
				 * Now we can allow context to be stolen.
				 */
				if (sfmmup_orig != ksfmmup)
					sfmmu_allow_ctx_steal(sfmmup_orig);
				kpreempt_enable();
				return;
			} else {
				if (pml)
					sfmmu_mlist_exit(pml);
				SFMMU_HASH_UNLOCK(hmebp);
				hmeblkp = NULL;
				break;
			}
		}
		SFMMU_HASH_UNLOCK(hmebp);
		hashno++;
	} while (sfmmup->sfmmu_lttecnt && (hashno <= MAX_HASHCNT));
	if (sfmmulock) {
		ASSERT(ism_hatid != NULL);
		mutex_exit(sfmmulock);
	}
	kpreempt_enable();
	ASSERT(ism_hatid || (ctxnum == sfmmup->sfmmu_cnum));

	/*
	 * Now we can allow our context to be stolen.
	 */
	if (sfmmup_orig != ksfmmup)
		sfmmu_allow_ctx_steal(sfmmup_orig);

	/*
	 * If hment was not found in the hash on a protection fault
	 * or it was found but not valid, then it's really a data mmu
	 * miss; fix up the traptype before calling trap.
	 */
	if (hmeblkp == NULL && traptype == T_DATA_PROT)
		traptype = T_DATA_MMU_MISS;

	/* will call pagefault */
	trap(rp, (caddr_t)tagaccess, traptype, 0);
}

/*
 * Special routine to flush out ism mappings- TSBs, TLBs and D-caches.
 * This routine may be called with all cpu's captured. Therefore, the
 * caller is responsible for holding all locks and disabling kernel
 * preemption.
 */
static void
sfmmu_ismtlbcache_demap(caddr_t addr, sfmmu_t *ism_sfmmup,
	struct hme_blk *hmeblkp, pfn_t pfnum, int cache_flush_flag)
{
	cpuset_t 	cpuset;
	caddr_t 	va;
	ism_ment_t	*ment;
	sfmmu_t		*sfmmup;
	int 		ctxnum;
	int 		vcolor;

	/*
	 * Walk the ism_hat's mapping list and flush the page
	 * from every hat sharing this ism_hat. This routine
	 * may be called while all cpu's have been captured.
	 * Therefore we can't attempt to grab any locks. For now
	 * this means we will protect the ism mapping list under
	 * a single lock which will be grabbed by the caller.
	 * If hat_share/unshare scalibility becomes a performance
	 * problem then we may need to re-think ism mapping list locking.
	 */
	ASSERT(ism_sfmmup->sfmmu_ismhat);
	ASSERT(MUTEX_HELD(&ism_mlist_lock));
	addr = addr - ISMID_STARTADDR;
	for (ment = ism_sfmmup->sfmmu_iment; ment; ment = ment->iment_next) {

		sfmmup = ment->iment_hat;
		ctxnum = sfmmup->sfmmu_cnum;
		va =  ism_start(*ment->iment_map);
		va = (caddr_t)((uintptr_t)va  + (uintptr_t)addr);

		if (ctxnum != INVALID_CONTEXT) {

			/*
			 * Flush tsb.
			 */
			if (get_hblk_ttesz(hmeblkp) == TTE8K) {
				sfmmu_unload_tsb(va, ctxnum);
			} else if ((get_hblk_ttesz(hmeblkp) == TTE4M) &&
			    sfmmup != KHATID) {
				sfmmu_unload_tsb4m(va, ctxnum);
			}

			/*
			 * Flush tlb's
			 */
			cpuset = sfmmup->sfmmu_cpusran;
			CPUSET_AND(cpuset, cpu_ready_set);
			CPUSET_DEL(cpuset, CPU->cpu_id);
			SFMMU_XCALL_STATS(cpuset, ctxnum);
			xt_some(cpuset, vtag_flushpage_tl1, (uint64_t)va,
			    ctxnum);
			vtag_flushpage(va, ctxnum);
		}

		/*
		 * Flush D$
		 * When flushing D$ we must flush all
		 * cpu's. See sfmmu_cache_flush().
		 */
		if (cache_flush_flag == CACHE_FLUSH) {
			cpuset = cpu_ready_set;
			CPUSET_DEL(cpuset, CPU->cpu_id);
			SFMMU_XCALL_STATS(cpuset, ctxnum);
			vcolor = addr_to_vcolor(va);
			xt_some(cpuset, vac_flushpage_tl1, pfnum, vcolor);
			vac_flushpage(pfnum, vcolor);
		}
	}
}

/*
 * Flushes caches and tlbs on all cpus for a particular virtual address
 * and ctx.
 */
static void
sfmmu_tlbcache_demap(caddr_t addr, sfmmu_t *sfmmup, struct hme_blk *hmeblkp,
	pfn_t pfnum, int tlb_noflush, int cpu_flag, int cache_flush_flag,
	int ctx_lock_held)
{
	int ctxnum, vcolor;
	cpuset_t cpuset;

	ctxnum = (int)sfmmutoctxnum(sfmmup);
	vcolor = addr_to_vcolor(addr);

	/*
	 * There is no need to protect against ctx being stolen.  Even if the
	 * ctx is stolen, we need to flush the cache. Our ctx stealer only
	 * flushes the tlbs/tsb.
	 */
	kpreempt_disable();
	if (ctxnum != INVALID_CONTEXT) {

		/*
		 * Flush tsb.
		 */
		if (get_hblk_ttesz(hmeblkp) == TTE8K) {
			sfmmu_unload_tsb(addr, ctxnum);
		} else if ((get_hblk_ttesz(hmeblkp) == TTE4M) &&
			    sfmmup != KHATID) {
			sfmmu_unload_tsb4m(addr, ctxnum);
		}

		/*
		 * Flush tlb's
		 */
		if (!tlb_noflush) {
			cpuset = sfmmup->sfmmu_cpusran;
			CPUSET_AND(cpuset, cpu_ready_set);
			CPUSET_DEL(cpuset, CPU->cpu_id);
			SFMMU_XCALL_STATS(cpuset, ctxnum);
			xt_some(cpuset, vtag_flushpage_tl1, (uint64_t)addr,
				ctxnum);
			vtag_flushpage(addr, ctxnum);
		}
	} else if (ctx_lock_held == 0) {

		/*
		 * It's rare but possible that sfmmu_reuse_ctx()
		 * just set our ctx to invalid but has yet to
		 * flush the mappings. So to avoid this race
		 * we grab the ctx_lock since sfmmu_reuse_ctx()'s
		 * caller must be holding the ctx_lock. Except
		 * in the case of resolving vac conflicts.
		 */
		mutex_enter(&ctx_lock);
		mutex_exit(&ctx_lock);
	}

	/*
	 * Flush D$
	 */
	if (cache_flush_flag == CACHE_FLUSH) {
		if (cpu_flag & FLUSH_ALL_CPUS) {
			cpuset = cpu_ready_set;
		} else {
			cpuset = sfmmup->sfmmu_cpusran;
			CPUSET_AND(cpuset, cpu_ready_set);
		}
		CPUSET_DEL(cpuset, CPU->cpu_id);
		SFMMU_XCALL_STATS(cpuset, ctxnum);
		xt_some(cpuset, vac_flushpage_tl1, pfnum, vcolor);
		vac_flushpage(pfnum, vcolor);
	}
	kpreempt_enable();
}

/*
 * Demaps the tsb and flushes all tlbs on all cpus for a particular virtual
 * address and ctx. if noflush is set we do not flush the tlb.
 */
static void
sfmmu_tlb_demap(caddr_t addr, sfmmu_t *sfmmup, struct hme_blk *hmeblkp,
	int tlb_noflush, int ctx_lock_held)
{
	int ctxnum;
	cpuset_t cpuset;

	ctxnum = (int)sfmmutoctxnum(sfmmup);
	/*
	 * if ctx was stolen then simply return
	 * whoever stole ctx is responsible for flush.
	 */
	if (ctxnum == INVALID_CONTEXT) {

		/*
		 * It's rare but possible that sfmmu_reuse_ctx()
		 * just set our ctx to invalid but has yet to
		 * flush the mappings. So to avoid this race
		 * we grab the ctx_lock since sfmmu_reuse_ctx()'s
		 * caller must be holding the ctx_lock.
		 */
		if (ctx_lock_held == 0) {
			mutex_enter(&ctx_lock);
			mutex_exit(&ctx_lock);
		}
		return;
	}

	/*
	 * Flush tsb
	 */
	if (get_hblk_ttesz(hmeblkp) == TTE8K) {
		sfmmu_unload_tsb(addr, ctxnum);
	} else if ((get_hblk_ttesz(hmeblkp) == TTE4M) &&
		    sfmmup != KHATID) {
		sfmmu_unload_tsb4m(addr, ctxnum);
	}

	/*
	 * There is no need to protect against ctx being stolen.  If the
	 * ctx is stolen we will simply get an extra flush.
	 */
	if (!tlb_noflush) {
		/*
		 * if process is exiting then delay flush.
		 */
		kpreempt_disable();
		cpuset = sfmmup->sfmmu_cpusran;
		CPUSET_AND(cpuset, cpu_ready_set);
		CPUSET_DEL(cpuset, CPU->cpu_id);
		SFMMU_XCALL_STATS(cpuset, ctxnum);
		xt_some(cpuset, vtag_flushpage_tl1, (uint64_t)addr, ctxnum);
		vtag_flushpage(addr, ctxnum);
		kpreempt_enable();
	}
}

/*
 * Flushes only TLB.
 */
static
void
sfmmu_tlb_ctx_demap(sfmmu_t *sfmmup)
{
	int ctxnum;
	cpuset_t cpuset;

	ctxnum = (int)sfmmutoctxnum(sfmmup);
	if (ctxnum == INVALID_CONTEXT) {
		/*
		 * if ctx was stolen then simply return
		 * whoever stole ctx is responsible for flush.
		 */
		return;
	}
	ASSERT(ctxnum != KCONTEXT);
	/*
	 * There is no need to protect against ctx being stolen.  If the
	 * ctx is stolen we will simply get an extra flush.
	 */
	kpreempt_disable();

	cpuset = sfmmup->sfmmu_cpusran;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	CPUSET_AND(cpuset, cpu_ready_set);
	SFMMU_XCALL_STATS(cpuset, ctxnum);

	/*
	 * Flush tlb.
	 * RFE: it might be worth delaying the tlb flush as well. In that
	 * case each cpu would have to traverse the dirty list and flush
	 * each one of those ctx from the tlb.
	 */
	vtag_flushctx(ctxnum);
	xt_some(cpuset, vtag_flushctx_tl1, ctxnum, 0);

	kpreempt_enable();
}

/*
 * We need to flush the cache in all cpus.  It is possible that
 * a process referenced a page as cacheable but has sinced exited
 * and cleared the mapping list.  We still to flush it but have no
 * state so all cpus is the only alternative.
 */
void
sfmmu_cache_flush(pfn_t pfnum, int vcolor)
{
	cpuset_t cpuset;
	extern cpuset_t cpu_ready_set;
	int	ctxnum = INVALID_CONTEXT;

	kpreempt_disable();
	cpuset = cpu_ready_set;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	SFMMU_XCALL_STATS(cpuset, ctxnum);	/* account to any ctx */
	xt_some(cpuset, vac_flushpage_tl1, pfnum, vcolor);
	xt_sync(cpuset);
	vac_flushpage(pfnum, vcolor);
	kpreempt_enable();
}

void
sfmmu_cache_flushcolor(int vcolor, pfn_t pfnum)
{
	cpuset_t cpuset;
	extern cpuset_t cpu_ready_set;
	int	ctxnum = INVALID_CONTEXT;

	ASSERT(vcolor >= 0);

	kpreempt_disable();
	cpuset = cpu_ready_set;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	SFMMU_XCALL_STATS(cpuset, ctxnum);	/* account to any ctx */
	xt_some(cpuset, vac_flushcolor_tl1, vcolor, pfnum);
	xt_sync(cpuset);
	vac_flushcolor(vcolor, pfnum);
	kpreempt_enable();
}

/*
 * Completely flush the D-cache on all cpus.
 */
void
sfmmu_cache_flushall()
{
	int i;

	for (i = 0; i < CACHE_NUM_COLOR; i++)
		sfmmu_cache_flushcolor(i, 0);
}

void
sfmmu_inv_tsb(caddr_t tsb_bs, uint_t tsb_bytes)
{
	struct tsbe *tsbaddr;

	for (tsbaddr = (struct tsbe *)tsb_bs;
	    (uintptr_t)tsbaddr < (uintptr_t)(tsb_bs + tsb_bytes);
	    tsbaddr++) {
		tsbaddr->tte_tag.tag_inthi = TSBTAG_INVALID;
	}
}

static void
sfmmu_init_tsbinfo(struct tsb_info *tsbinfo, caddr_t vaddr, int tsbsz,
	int ttesz, int tsbcode)
{
	pfn_t	pfn;

	ASSERT(ttesz >= TTE8K && ttesz <= TTE4M);
	ASSERT(!((uintptr_t)vaddr & (tsbsz - 1)));

	/*
	 * Make tsb register. We use virtual space for the TSB.
	 */
	sfmmu_make_tsbreg(&tsbinfo->tsb_reg, vaddr, TSB_SPLIT_CODE, tsbcode);

	/*
	 * Make tte for this TSB.
	 */
	pfn = va_to_pfn(vaddr);
	sfmmu_memtte(&tsbinfo->tsb_tte, pfn, PROT_WRITE|PROT_READ, ttesz);
	TTE_SET_LOCKED(&tsbinfo->tsb_tte);	/* lock the tte into dtlb */
	TTE_SET_MOD(&tsbinfo->tsb_tte);		/* enable writes */

	ASSERT(TTE_IS_PRIVILEGED(&tsbinfo->tsb_tte));
	ASSERT(TTE_IS_LOCKED(&tsbinfo->tsb_tte));

	sfmmu_inv_tsb(vaddr, tsbsz);
}

/*
 * Initialize per cpu tsb and per cpu tsbmiss_area
 */
void
sfmmu_init_tsbs()
{
	int i;
	caddr_t vaddr;
	struct tsbmiss *tsbmissp;
	extern int	dcache_line_mask;

	/*
	 * Init. kernel tsb information.
	 */
	sfmmu_make_tsbreg(&KTSBINFO->tsb_reg, (caddr_t)ktsb_base,
	    TSB_SPLIT_CODE, ktsb_szcode);
	ktsb_reg = KTSBINFO->tsb_reg;

	/*
	 * Zero out tte since kernel tsb lives in the nucleus
	 */
	KTSBINFO->tsb_tte.tte_inthi = 0;
	KTSBINFO->tsb_tte.tte_intlo = 0;

	/*
	 * Setup Kernel and invalid context tsb's.
	 * The invalid context will share the first
	 * small user tsb.
	 */
	CTX_SET_TSBINDEX(ctxnumtoctx(KCONTEXT), KTSBNUM, 0);
	CTX_SET_TSBINDEX(ctxnumtoctx(INVALID_CONTEXT), INVALTSBNUM, 0);

	/*
	 * Invalidate kernel tsb
	 */
	sfmmu_inv_tsb((caddr_t)ktsb_base, ktsb_sz);

	/*
	 * Is the kernel tsb also shared by users?
	 */
	if (tsb512k_num == 0) {
		ASSERT(tsb_num == 1);
		ASSERT(utsb_4m_disable == 1);
		tsb_bases[0] = *KTSBINFO;
	} else {
		int i, j;
		/*
		 * Init. user tsb information.
		 */
		vaddr = tsballoc_base;
		for (i = 0; i < tsb512k_num; i++) {
			sfmmu_init_tsbinfo(&tsb512k_bases[i], vaddr,
			    TSB_BYTES(TSB_512K_SZCODE), TTE512K,
			    TSB_512K_SZCODE);
			vaddr += TSB_BYTES(TSB_512K_SZCODE);
		}
		vaddr = tsballoc_base;
		for (i = 0; i < tsb_num; i++) {
			sfmmu_make_tsbreg(&tsb_bases[i].tsb_reg, vaddr,
			    TSB_SPLIT_CODE, TSB_MIN_SZCODE);
			j = i / TSB_SIZE_FACTOR;
			tsb_bases[i].tsb_tte = tsb512k_bases[j].tsb_tte;
			vaddr += TSB_BYTES(TSB_MIN_SZCODE);
		}
	}

	/*
	 * Init. tsb miss area.
	 */
	tsbmissp = tsbmiss_area;

	if (khmehash_num > USHRT_MAX || uhmehash_num > USHRT_MAX)
		panic("HmeHash sizes are larger than expected");

	for (i = 0; i < NCPU; tsbmissp++, i++) {
		/*
		 * initialize the tsbmiss area.
		 * Do this for all possible CPUs as some may be added
		 * while the system is running. There is no cost to this.
		 */
		tsbmissp->sfmmup = ksfmmup;
		tsbmissp->khashsz = (ushort_t)khmehash_num;
		tsbmissp->khashstart = khme_hash;
		tsbmissp->uhashsz = (ushort_t)uhmehash_num;
		tsbmissp->uhashstart = uhme_hash;
		tsbmissp->dcache_line_mask = dcache_line_mask;
		tsbmissp->ctxs = ctxs;
	}
}

/*
 * this function creates nucleus hmeblks and adds them to the freelists.
 * It returns the approximate number of 8k hmeblks we could create with
 * the given segment of nucleus memory.
 */
void
sfmmu_add_nucleus_hblks(caddr_t addr, size_t size)
{
	struct hme_blk *hmeblkp;
	size_t hme8blk_sz, hme1blk_sz;
	size_t i;
	ulong_t j = 0, k = 0;

	ASSERT(addr != NULL && size != 0);
	hme8blk_sz = roundup(HME8BLK_SZ, sizeof (int64_t));
	hme1blk_sz = roundup(HME1BLK_SZ, sizeof (int64_t));

	HBLK8_FLIST_LOCK();
	/*
	 * create nucleus hmeblks and add to the freelist. Try to allocate
	 * 8 hme8blks for every hme1blk. Note that hme8blk is about three
	 * times the size of hme1blk.
	 */
	for (i = 0; i <= (size - size/24 - hme8blk_sz); i += hme8blk_sz, j++) {
		hmeblkp = (struct hme_blk *)addr;
		addr += hme8blk_sz;
		hmeblkp->hblk_nuc_bit = 1;
		hmeblkp->hblk_nextpa = va_to_pa((caddr_t)hmeblkp);
		if (hblk8_avail++ == 0) {
			hblk8_flist = hblk8_flist_t = hmeblkp;
			hmeblkp->hblk_next = NULL;
		} else {
			hmeblkp->hblk_next = hblk8_flist;
			hblk8_flist = hmeblkp;
		}
		SFMMU_STAT(sf_hblk8_nalloc);
	}
	hblk8_allocated += j;
	HBLK_DEBUG_COUNTER_INCR(nhblk8_allocated, j);
	HBLK8_FLIST_UNLOCK();

	HBLK1_FLIST_LOCK();
	for (; i <= (size - hme1blk_sz); i += hme1blk_sz, k++) {
		hmeblkp = (struct hme_blk *)addr;
		addr += hme1blk_sz;
		hmeblkp->hblk_nuc_bit = 1;
		hmeblkp->hblk_nextpa = va_to_pa((caddr_t)hmeblkp);
		if (hblk1_avail++ == 0) {
			hblk1_flist = hblk1_flist_t = hmeblkp;
			hmeblkp->hblk_next = NULL;
		} else {
			hmeblkp->hblk_next = hblk1_flist;
			hblk1_flist = hmeblkp;
		}
		SFMMU_STAT(sf_hblk1_nalloc);
	}
	hblk1_allocated += k;
	HBLK_DEBUG_COUNTER_INCR(nhblk1_allocated, k);
	HBLK1_FLIST_UNLOCK();

	PRM_DEBUG(hblk8_avail);
	PRM_DEBUG(hblk1_avail);
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/* ARGSUSED */
faultcode_t
hat_softlock(hat, addr, lenp, ppp, flags)
	struct hat *hat;
	caddr_t	addr;
	size_t	*lenp;
	page_t	**ppp;
	uint_t	flags;
{
	return (FC_NOSUPPORT);
}


/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/* ARGSUSED */
faultcode_t
hat_pageflip(struct hat *hat, caddr_t addr_to, caddr_t kaddr,
		size_t *lenp, page_t **pp_to, page_t **pp_from)
{
	return (FC_NOSUPPORT);
}

/*
 * Enter a hme on the mapping list for page pp.
 * When large pages are more prevalent in the system we might want to
 * keep the mapping list in ascending order by the hment size. For now,
 * small pages are more frequent, so don't slow it down.
 */
static void
hme_add(struct sf_hment *hme, machpage_t *pp)
{
	ASSERT(sfmmu_mlist_held(pp));

	hme->hme_prev = NULL;
	hme->hme_next = pp->p_mapping;
	hme->hme_page = pp;
	if (pp->p_mapping) {
		pp->p_mapping->hme_prev = hme;
		ASSERT(pp->p_share > 0);
	} else  {
		/* EMPTY */
		ASSERT(pp->p_share == 0);
	}
	pp->p_mapping = hme;
	/*
	 * Update number of mappings.
	 */
	pp->p_share++;
}

/*
 * Enter a hme on the mapping list for page pp.
 * If we are unmapping a large translation, we need to make sure that the
 * change is reflect in the corresponding bit of the p_index field.
 */
static void
hme_sub(struct sf_hment *hme, machpage_t *pp)
{
	ASSERT(sfmmu_mlist_held(pp));
	ASSERT(hme->hme_page == pp);

	if (pp->p_mapping == NULL) {
		panic("hme_remove - no mappings");
	}

	membar_stst();		/* make sure previous stores finish */

	ASSERT(pp->p_share > 0);
	pp->p_share--;

	if (hme->hme_prev) {
		ASSERT(pp->p_mapping != hme);
		ASSERT(hme->hme_prev->hme_page == pp);
		hme->hme_prev->hme_next = hme->hme_next;
	} else {
		ASSERT(pp->p_mapping == hme);
		pp->p_mapping = hme->hme_next;
		ASSERT((pp->p_mapping == NULL) ?
			(pp->p_share == 0) : 1);
	}

	if (hme->hme_next) {
		ASSERT(hme->hme_next->hme_page == pp);
		hme->hme_next->hme_prev = hme->hme_prev;
	}

	/*
	 * zero out the entry
	 */
	hme->hme_next = NULL;
	hme->hme_prev = NULL;
	hme->hme_page = NULL;

	if (hme_size(hme) > TTE8K) {
		/*
		 * remove mappings for the
		 * reset of the large page.
		 */
		sfmmu_rm_large_mappings(pp, hme_size(hme));
	}
}

/*
 * Searchs the mapping list of the page for a mapping of the same size. If not
 * found the corresponding bit is cleared in the p_index field. When large
 * pages are more prevalent in the system, we can maintain the mapping list
 * in order and we don't have to traverse the list each time. Just check the
 * next and prev entries, and if both are of different size, we clear the bit.
 */
static void
sfmmu_rm_large_mappings(machpage_t *pp, int ttesz)
{
	struct sf_hment *sfhmep;
	int 	index;
	pgcnt_t	npgs;

	ASSERT(ttesz > TTE8K);

	ASSERT(sfmmu_mlist_held(pp));

	ASSERT(PP_ISMAPPED_LARGE(pp));

	/*
	 * Traverse mapping list looking for another mapping of same size.
	 * since we only want to clear index field if all mappings of
	 * that size are gone.
	 */

	for (sfhmep = pp->p_mapping; sfhmep; sfhmep = sfhmep->hme_next) {
		if (hme_size(sfhmep) == ttesz) {
			/*
			 * another mapping of the same size. don't clear index.
			 */
			return;
		}
	}

	/*
	 * Clear the p_index bit for large page.
	 */
	index = PAGESZ_TO_INDEX(ttesz);
	npgs = TTEPAGES(ttesz);
	while (npgs-- > 0) {
		ASSERT(pp->p_index & index);
		pp->p_index &= ~index;
		pp = PP_PAGENEXT(pp);
	}
}

/*
 * return supported features
 */
/* ARGSUSED */
int
hat_supported(enum hat_features feature, void *arg)
{
	switch (feature) {
	case    HAT_SHARED_PT:
		return (1);
	case	HAT_DYNAMIC_ISM_UNMAP:
		return (1);
	default:
		return (0);
	}
}

void
hat_enter(struct hat *hat)
{
	mutex_enter(&hat->sfmmu_mutex);
}

void
hat_exit(struct hat *hat)
{
	mutex_exit(&hat->sfmmu_mutex);
}

pfn_t
hat_getkpfnum(caddr_t addr)
{
	return (hat_getpfnum((struct hat *)kas.a_hat, addr));
}

/*ARGSUSED*/
void
hat_reserve(struct as *as, caddr_t addr, size_t len)
{
}

static void
hat_kstat_init(void)
{
	kstat_t *ksp;

	ksp = kstat_create("unix", 0, "sfmmu_global_stat", "hat",
		KSTAT_TYPE_RAW, sizeof (struct sfmmu_global_stat),
		KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &sfmmu_global_stat;
		kstat_install(ksp);
	}
	ksp = kstat_create("unix", 0, "sfmmu_percpu_stat", "hat",
		KSTAT_TYPE_RAW, sizeof (struct sfmmu_percpu_stat) * NCPU,
		KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_update = sfmmu_kstat_percpu_update;
		kstat_install(ksp);
	}
}

/* ARGSUSED */
static int
sfmmu_kstat_percpu_update(kstat_t *ksp, int rw)
{
	struct sfmmu_percpu_stat *cpu_kstat = ksp->ks_data;
	struct tsbmiss *tsbm;
	int i;

	ASSERT(cpu_kstat);
	if (rw == KSTAT_READ) {
		tsbm = tsbmiss_area;
		for (i = 0; i < NCPU; cpu_kstat++, tsbm++, i++) {
			cpu_kstat->sf_itlb_misses = tsbm->itlb_misses;
			cpu_kstat->sf_dtlb_misses = tsbm->dtlb_misses;
			cpu_kstat->sf_utsb_misses = tsbm->utsb_misses;
			cpu_kstat->sf_ktsb_misses = tsbm->ktsb_misses;
			if (tsbm->itlb_misses > 0 && tsbm->dtlb_misses > 0) {
				cpu_kstat->sf_tsb_hits =
				(tsbm->itlb_misses + tsbm->dtlb_misses) -
				(tsbm->utsb_misses + tsbm->ktsb_misses);
			} else {
				cpu_kstat->sf_tsb_hits = 0;
			}
			cpu_kstat->sf_umod_faults = tsbm->uprot_traps;
			cpu_kstat->sf_kmod_faults = tsbm->kprot_traps;
		}
	} else {
		/* KSTAT_WRITE is used to clear stats */
		tsbm = tsbmiss_area;
		for (i = 0; i < NCPU; tsbm++, i++) {
			tsbm->itlb_misses = 0;
			tsbm->dtlb_misses = 0;
			tsbm->utsb_misses = 0;
			tsbm->ktsb_misses = 0;
			tsbm->uprot_traps = 0;
			tsbm->kprot_traps = 0;
		}
	}
	return (0);
}

#ifdef DEBUG
/*
 * Debug code that verifies hblk lists are correct
 */
static void
sfmmu_check_hblk_flist()
{
	int i;
	struct hme_blk *hmeblkp, *pr_hmeblkp;

	HBLK8_FLIST_LOCK();
	for (i = 0, pr_hmeblkp = NULL, hmeblkp = hblk8_flist; hmeblkp; i++) {
			pr_hmeblkp = hmeblkp;
			hmeblkp = hmeblkp->hblk_next;
	}
	if (i != hblk8_avail || pr_hmeblkp != hblk8_flist_t ||
		hblk8_allocated != (hblk8_avail + hblk8_inuse)) {
		panic("sfmmu_check_hblk_flist: inconsistent hblk8_flist");
	}
	HBLK8_FLIST_UNLOCK();

	HBLK1_FLIST_LOCK();
	for (i = 0, pr_hmeblkp = NULL, hmeblkp = hblk1_flist; hmeblkp; i++) {
		pr_hmeblkp = hmeblkp;
		hmeblkp = hmeblkp->hblk_next;
	}
	if (i != hblk1_avail || pr_hmeblkp != hblk1_flist_t ||
		hblk1_allocated != (hblk1_avail + hblk1_inuse)) {
		panic("sfmmu_check_hblk_flist: inconsistent hblk1_flist");
	}
	HBLK1_FLIST_UNLOCK();
}

tte_t  *gorig[NCPU], *gcur[NCPU], *gnew[NCPU];

/*
 * A tte checker. *orig_old is the value we read before cas.
 *	*cur is the value returned by cas.
 *	*new is the desired value when we do the cas.
 *
 *	*hmeblkp is currently unused.
 */

/* ARGSUSED */
void
chk_tte(tte_t *orig_old, tte_t *cur, tte_t *new, struct hme_blk *hmeblkp)
{
	uint_t i, j, k;
	int cpuid = CPU->cpu_id;

	gorig[cpuid] = orig_old;
	gcur[cpuid] = cur;
	gnew[cpuid] = new;

#ifdef lint
	hmeblkp = hmeblkp;
#endif

	if (TTE_IS_VALID(orig_old)) {
		if (TTE_IS_VALID(cur)) {
			i = TTE_TO_TTEPFN(orig_old);
			j = TTE_TO_TTEPFN(cur);
			k = TTE_TO_TTEPFN(new);
			if (i != j) {
				/* remap error? */
				panic("chk_tte: bad pfn, 0x%x, 0x%x",
					i, j);
			}

			if (i != k) {
				/* remap error? */
				panic("chk_tte: bad pfn2, 0x%x, 0x%x",
					i, k);
			}
		} else {
			if (TTE_IS_VALID(new)) {
				panic("chk_tte: invalid cur? ");
			}

			i = TTE_TO_TTEPFN(orig_old);
			k = TTE_TO_TTEPFN(new);
			if (i != k) {
				panic("chk_tte: bad pfn3, 0x%x, 0x%x",
					i, k);
			}
		}
	} else {
		if (TTE_IS_VALID(cur)) {
			j = TTE_TO_TTEPFN(cur);
			if (TTE_IS_VALID(new)) {
				k = TTE_TO_TTEPFN(new);
				if (j != k) {
					panic("chk_tte: bad pfn4, 0x%x, 0x%x",
						j, k);
				}
			} else {
				panic("chk_tte: why here?");
			}
		} else {
			if (!TTE_IS_VALID(new)) {
				panic("chk_tte: why here2 ?");
			}
		}
	}
}

#endif /* DEBUG */
