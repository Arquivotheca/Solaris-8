/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hat_srmmu.c	1.236	99/09/22 SMI"

/*
 * VM - Hardware Address Translation management.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <vm/hat.h> while the machine dependent interface
 * and data structures are described in <vm/hat_srmmu.h>.
 *
 * The hat layer manages the address translation hardware as a cache
 * driven by calls from the higher levels in the VM system.  Nearly
 * all the details of how the hardware is managed shound not be visable
 * about this layer except for miscellanous machine specific functions
 * (e.g. mapin/mapout) that work in conjunction with this code.  Other
 * than a small number of machine specific places, the hat data
 * structures seen by the higher levels in the VM system are opaque
 * and are only operated on by the hat routines.  Each address space
 * contains a struct hat and a page contains an opaque pointer which
 * is used by the hat code to hold a list of active translations to
 * that page.
 *
 * Hardware address translation routines for the SPARC(tm) Reference MMU.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/devaddr.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/promif.h>

#include <sys/var.h>

#include <vm/page.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_spt.h>
#include <vm/seg_kp.h>
#include <vm/seg_kmem.h>
#include <vm/rm.h>
#include <vm/hat_srmmu.h>
#include <vm/mach_page.h>

extern	int	do_pg_coloring;

/*
 * If the following flag is set then whenever a PTE is written with user
 * write permission enabled, it will have its mbit set so as to work
 * around the problem caused by bug 1220902 for redundant systems
 * which watch bus traffic and freak if one CPU does a table walk but
 * another does not.  Since the pte mbit will always be set, a tablewalk will
 * never be initiated by the MMU to set it.
 *
 * There is only a very minor performance penalty (the cost of the checks for
 * the condition) for doing this, since we know that the modified bit in the
 * page struct will already have been set by the segvn workaround, so we won't
 * force any more page I/O.  (We've already paid the price of the extra minor
 * fault).
 *
 * It makes no sense to set this unless the processor is a viking so we
 * auto-enable this flag only if this CPU type is detected.
 */
extern int enable_mbit_wa;

/*
 * The following defines make it possible to cstyle the area where
 * they are used
 */
#define	URWX(p) (((struct pte *)(p))->AccessPermissions == MMU_STD_SRWXURWX)
#define	SET_PTE_AP(p, perm) (((struct pte *)(p))->AccessPermissions = (perm))

/*
 * Private vm_hat data structures.
 */
static struct ctx	*ctxhand;	/* hand for allocating contexts */

int		static_ctx;	/* are ctxs bound to srmmus? */

#ifdef MORE_DEBUG
static void vrfy_ptbl_vcnt(ptbl_t *ptbl);
#define	VRFY_PTBL_VCNT(ptbl)	vrfy_ptbl_vcnt(ptbl)
#else
#define	VRFY_PTBL_VCNT(ptbl)
#endif

static void	srmmu_install(struct as *);
static int	srmmu_pteload(struct as *, caddr_t, struct page *,
		    struct pte *, int, int, int);
static page_t	*srmmu_pteunload(ptbl_t *, struct pte *, int, int);
static struct	srhment	*srmmu_ptesync(ptbl_t *, struct pte *, int, int);
struct	pte	*srmmu_ptealloc(struct as *, caddr_t, int,
		    ptbl_t **, kmutex_t **, int);
static void	srmmu_getctx(struct as *);
static int	srmmu_unload_loop(ptbl_t *, int, int, u_int, page_t *rpp[]);
static void	srmmu_ptechgprot(struct pte *, u_int, int, caddr_t, int,
		    struct hat *, int);
static int	srmmu_vtop_clrattr(int, u_int, int *);
static void 	srmmu_unload_tree(struct as *, caddr_t, u_int, int, srmmu_t *);
static void	srmmu_set_rm(page_t *, u_int);
static caddr_t	ptetovaddr(ptbl_t *, struct pte *);

#ifdef VAC
static void	srmmu_ptecachectl(ptbl_t *, struct pte *, struct page *);
static int	srmmu_page_cache(struct page *, int);
static int	srmmu_vac_conflict(struct as *, caddr_t, struct page *,
		    ptbl_t *);
static int	srmmu_vac_alias(page_t *);
static void	srmmu_unload_vacpgs(page_t *rpp[]);

int fd_page_vac_align(page_t *);

extern u_int	vac_mask;
extern void	vac_color_sync();
extern struct   seg *segkmap;

extern caddr_t	segkmap_hi, segkmap_lo;

#define	VAC_ALIGNED(a1, a2) ((((u_int)(a1) ^ (u_int)(a2)) & vac_mask) == 0)
#define	VCOLOR_2_ADDR(pp)   (pp->p_vcolor << MMU_PAGESHIFT)
#define	ADDR_2_VCOLOR(addr) (((u_int)addr & vac_mask) >> MMU_PAGESHIFT)

/*
 * NOTE- This macro assumes segkmap is aligned at L3ptbl boundary,
 * which it is now. If this is ever changed, we'll have to figure
 * out the exact vaddr instead of just based it on the ptbl.
 */
#define	KAS_NO_FAULT(ptbl)  ((ptbl->ptbl_as == &kas) && \
	(BASE2VA(ptbl) < segkmap_lo || BASE2VA(ptbl) >= segkmap_hi))

#endif

/*
 * Macro used to access machpage_t fields from page_t
 */
#define	mach_pp	((machpage_t *)pp)

#if defined(SRMMU_TMPUNLOAD)
static void	srmmu_tmpmergel2(ptbl_t *, ptbl_t *, caddr_t);
static void	mod_l3tmp_bits(ptbl_t *, struct as *, caddr_t, int);
static kmutex_t	*cv_locked_ptbl(ptbl_t *, kmutex_t *);

#define	CLR_TMP	1
#define	SET_TMP	2
#endif

void srmmu_convert_pmapping(page_t *);

/*
 * Semi-private data
 */
struct ctx	*ctxs, *ectxs;		/* used by <machine/mmu.c> */
struct ptp	*contexts, *econtexts;	/* The hardware context table */

u_int		nctxs = 0;		/* total number of contexts */
int		cache = 0;		/* describes system cache */
int		mmu_l3only = 0;		/* only use l3 ptes */

/*
 * ctx, ptbl, mlistlock and other stats for srmmu
 */

struct vmhatstat {
	kstat_named_t	vh_ctxstealflush;
	kstat_named_t	vh_ptblstolen;
	kstat_named_t	vh_uncache_conflict;
	kstat_named_t	vh_unload_conflict;
#ifdef DEBUG
	kstat_named_t	vh_mt_pteload;
#endif /* DEBUG */
};

struct vmhatstat vmhatstat = {
	{ "vh_ctxstealflush",		KSTAT_DATA_ULONG },
	{ "vh_ptblstolen",		KSTAT_DATA_ULONG },
	{ "vh_uncache_conflict",	KSTAT_DATA_ULONG },
	{ "vh_unload_conflict",		KSTAT_DATA_ULONG },
#ifdef DEBUG
	{ "vh_mt_pteload",		KSTAT_DATA_ULONG },
#endif /* DEBUG */
};

/*
 * kstat data
 */
kstat_named_t *vmhatstat_ptr = (kstat_named_t *)&vmhatstat;
ulong_t vmhatstat_ndata = sizeof (vmhatstat) / sizeof (kstat_named_t);

/*
 * Global data for the kernel
 */
extern struct as kas;			/* kernel's address space */
struct ctx *kctx;			/* kernel's context */
struct l1pt *kl1pt;			/* kernel's l1 page table */
union ptpe kl1ptp;			/* kernel's l1 ptp */
ptbl_t *kl1ptbl;			/* kernel's l1 ptbl */
static srmmu_t		ksrmmu;		/* kernel's srmmu struct */


/* XXX - should ne in header file */
#define	PTE_RM_SHIFT 5

#define	NO_MLIST_LOCK	0
#define	MLIST_LOCKED	1

/*
 * SRMMU has several levels of locking.  The locks must be acquired
 * in the correct order to prevent deadlocks.
 *
 * The srmmu_cache_lock mutexs uncaching pages.  This lock is used to
 * load big ptes to memory to prevent the pages from changing cachability
 * while the page lists are being manipulated.  No other mutex or inuse
 * bit can be held when attempting to lock srmmu_cache_lock.
 *
 * Page mapping lists are locked by srmmu_mlist_enter() and srmmu_mlist_exit().
 * The only lock that can be held in srmmu_mlist_enter() is srmmu_cache_lock.
 *
 * The srmmu structs are also locked by the hat_mutex[]. This
 * lock must be held to change any resources that are allocated to
 * the srmmu, such as ctxs, l1pt.
 *
 * Changing entries in a ptbl requires the per-ptbl ptbl_locked bit.
 * In order to lock this bit, the corresponding ptbl_mutex[] must be held.
 * To wait for this bit to be unlocked, use srmmu_ptbl_wait().
 *
 * The srmmu_page_lock array of mutexes protect all pages' p_nrm fields.
 * To make life more interesting, the p_nrm bits may be played with without
 * holding the appropriate srmmu_page_lock if and only if the particular
 * page is not mapped.  This is typically done in page_create() after a
 * call to page_hashout().
 */

#define	SPL_TABLE_SIZE	64		/* must be a power of 2 */
#define	SPL_SHIFT	6
#define	SPL_HASH(pp)	\
	&srmmu_page_lock[(((u_int)pp) >> SPL_SHIFT) & (SPL_TABLE_SIZE - 1)]

static kmutex_t	srmmu_page_lock[SPL_TABLE_SIZE];

#ifdef DEMAP
kmutex_t srmmu_demap_mutex;		/* demapping mutex, only on at a time */
#endif

static void		srmmu_init(void);
static void		srmmu_alloc(struct hat *, struct as *);
static struct as 	*srmmu_setup(struct as *, int);
static void		srmmu_free(struct hat *, struct as *);
static void		srmmu_swapin(struct hat *, struct as *);
static void		srmmu_swapout(struct hat *, struct as *);
static int		srmmu_dup(struct hat *, struct as *, struct as *);
void			srmmu_memload(struct hat *, struct as *, caddr_t,
				struct page *, u_int, int);
void			srmmu_devload(struct hat *, struct as *, caddr_t,
				devpage_t *, u_int, u_int, int);
static void		srmmu_contig_memload(struct hat *, struct as *,
				caddr_t, struct page *, u_int, int, u_int);
static void		srmmu_contig_devload(struct hat *, struct as *,
				caddr_t, devpage_t *, u_int, u_int, int, u_int);
static void		srmmu_unlock(struct hat *, struct as *, caddr_t, u_int);
static faultcode_t	srmmu_fault(struct hat *, caddr_t);
static int		srmmu_probe(struct hat *, struct as *, caddr_t);
int			srmmu_share(struct as *, caddr_t, struct as *,
				caddr_t, u_int);
void			srmmu_unshare(struct as *, caddr_t, u_int);
void			srmmu_chgprot(struct as *, caddr_t, u_int, u_int);

void			srmmu_unload(struct as *, caddr_t, u_int, int);
static void		srmmu_sync(struct as *, caddr_t, u_int, u_int);

int			srmmu_pageunload(struct page *, struct srhment *, int);
static void		srmmu_sys_pageunload(struct page *, struct hment *);

int			srmmu_pagesync(struct hat *, struct page *,
				struct srhment *, u_int);
static int		srmmu_sys_pagesync(struct hat *, struct page *,
				struct hment *, u_int);

static void		srmmu_pagecachectl(struct page *, u_int);
static void		srmmu_sys_pagecachectl(struct page *, u_int);

static u_int		srmmu_getpfnum(struct as *, caddr_t);
static int		srmmu_map(struct hat *, struct as *, caddr_t, u_int,
					int);
static void		srmmu_lock_init(void);

static u_int		srmmu_getattr(struct as *, caddr_t, u_int *);
static void		srmmu_do_attr(struct as *, caddr_t, size_t, u_int, int);

static faultcode_t	srmmu_softlock(struct hat *, caddr_t, size_t *,
				struct page **, u_int);
static faultcode_t	srmmu_pageflip(struct hat *, caddr_t, caddr_t, size_t *,
				struct page **, struct page **);


int vrfy_is_mem(u_int pfn);

struct hatops srmmu_hatops = {
	srmmu_init,
	srmmu_alloc,
	srmmu_setup,
	srmmu_free,
	srmmu_dup,
	srmmu_swapin,
	srmmu_swapout,
	srmmu_memload,
	srmmu_devload,
	srmmu_contig_memload,
	srmmu_contig_devload,
	srmmu_unlock,
	srmmu_fault,
	srmmu_chgprot,
	srmmu_unload,
	srmmu_sync,
	srmmu_sys_pageunload,
	srmmu_sys_pagesync,
	srmmu_sys_pagecachectl,
	srmmu_getpfnum,
	srmmu_map,
	srmmu_probe,
	srmmu_lock_init,
	srmmu_share,
	srmmu_unshare,
	srmmu_do_attr,
	srmmu_getattr,
	srmmu_softlock,
	srmmu_pageflip
};

/*
 * Public function and data referenced by this hat
 */
struct	hatops	*sys_hatops = &srmmu_hatops;

/* srmmu locking operations */
static void	srmmu_page_enter(struct page *);
static void	srmmu_page_exit(struct page *);
static kmutex_t	*srmmu_mlist_enter(struct page *);
static void	srmmu_mlist_exit(kmutex_t *);

u_int	srmmu_sizes[] = {
	0,			/* level 0 == 4G (unrepresentable) */
	MMU_L1_SIZE,		/* level 1 == 16M */
	MMU_L2_SIZE,		/* level 2 == 256k */
	MMU_L3_SIZE		/* level 3 == 4k */
};

/*
 * Each L1 page table (containig 256 ptes) maps onto 4 L2/L3 (64 ptes) page
 * tables. As a result, there are 4 ptbl strucures backing up each L1
 * page table. So effectively, each ptbl protects L1_VA_PER_PTBL of virtual
 * address.
 */
#define	L1_VA_PER_PTBL		(MMU_L1_SIZE * 64)

/*
 * Flags for VAC consistency operations
 */
#define	NO_CONFLICT		0

#ifdef VAC
#define	UNLOAD_CONFLICT		1
#define	UNCACHE_CONFLICT	2
#define	ANY_CONFLICT		(UNLOAD_CONFLICT | UNCACHE_CONFLICT)

extern u_int stklo, stkhi;
#endif

#define	VAC_UNLOAD		(1)

/*
 * Prototypes for some internal routines.
 */
static void srmmu_l1inval(struct as *, ptbl_t *, int);
static int srmmu_l2inval(struct as *, caddr_t, int, ptbl_t *, kmutex_t **);
static int srmmu_l3inval(struct as *, caddr_t, int, ptbl_t *, kmutex_t **,
	page_t *rpp[]);

/*
 * srmmu pools and locks
 */
static srmmu_t *free_l1srmmu;
static kmutex_t free_l1srmmu_lock;

/*
 * ptbl pools and locks
 */
static ptbl_t *free_ptbl;

static int free_ptbl_23, total_ptbl_23;
static int free_ptbl_1, total_ptbl_1;
static int max_ptbl = 0;
static int min_ptbl = 10;

static kmutex_t free_ptbl_lock;
static kmutex_t srmmu_ctx_mtx;
static int cur_avl_ctx = 1;	/* context 0 is for kas. */

static struct kmem_cache *srmmu_cache;
static struct kmem_cache *srhme_cache;
static struct kmem_cache *srpt_cache;
static struct kmem_cache *srptbl_cache;

/*
 * Flag for get_XXX() routines to indicate not to
 * allocate more resources. If no resource is available, just
 * go ahead to steal one or return failure (NULL).
 */
#define	NO_ALLOC	0x2

/*
 * Flag for srmmu_ptbl_alloc() to indicate that new ptbl should be
 * a TMP ptbl.
 */
#define	GET_TMP_PTBL	0x4

#ifdef	sun4d

#define	N_PTE2HME_MTX   64		/* must be power of 2 */

#else	/* sun4d */

#define	N_PTE2HME_MTX   4		/* must be power of 2 */

#endif	/* sun4d */

static kmutex_t hme_hash_mtx[N_PTE2HME_MTX];

srhment_t **pte2hme_hash;		/* actual pointer to the hash table */
int pte2hme_hashsz;			/* size in entries */
int pte2hmehash_sz;			/* size in bytes */

#define	PTE2HME_BIN(p)		(((u_int)(p) >> 2) & (pte2hme_hashsz - 1))
#define	PTE2HME_BIN_MTX(b)	(&hme_hash_mtx[((b) & (N_PTE2HME_MTX - 1))])
#define	PTE2INDEX(p)		(((u_int)(p) >> 2) & 0x3f)

static struct srhment nullhme;

static kmutex_t *fd_ptbl_mtx(int level, ptbl_t *ptbl, int flag);
static int lock_this_ptbl(ptbl_t *, int, kmutex_t **);

#ifdef sun4d
#define	N_L3PTBL_MTX	(0x200)
#define	MLIST_SIZE	(0x40)
#else
#define	N_L3PTBL_MTX	(0x20)
#define	MLIST_SIZE	(0x4)
#endif

#define	N_L2PTBL_MTX	(0x40)
#define	N_L1PTBL_MTX	(0x20)

#define	L3PTBL_MTX_HASH(p) \
	((((u_int)(p) >> 12) ^ ((u_int)(p) >> 2)) & (N_L3PTBL_MTX - 1))
static	kmutex_t l3ptbl_lock[N_L3PTBL_MTX];

#define	L2PTBL_MTX_HASH(p)   \
	((((u_int)(p) >> 12) ^ ((u_int)(p) >> 2)) & (N_L2PTBL_MTX - 1))
static	kmutex_t l2ptbl_lock[N_L2PTBL_MTX];

#define	L1PTBL_MTX_HASH(p)   \
	((((u_int)(p) >> 12) ^ ((u_int)(p) >> 2)) & (N_L1PTBL_MTX - 1))
static	kmutex_t l1ptbl_lock[N_L1PTBL_MTX];


#if defined(SRMMU_TMPUNLOAD)

#define	N_TMPL3PTBL_MTX	(0x8)
#define	N_TMPL2PTBL_MTX	(0x4)
#define	N_TMPL1PTBL_MTX	(0x4)

#define	TMPL3PTBL_MTX_HASH(p)   \
	((((u_int)(p) >> 12) ^ ((u_int)(p) >> 2)) & (N_TMPL3PTBL_MTX - 1))
static	kmutex_t tmpl3ptbl_lock[N_TMPL3PTBL_MTX];

#define	TMPL2PTBL_MTX_HASH(p)   \
	((((u_int)(p) >> 12) ^ ((u_int)(p) >> 2)) & (N_TMPL2PTBL_MTX - 1))
static	kmutex_t tmpl2ptbl_lock[N_TMPL2PTBL_MTX];

#define	TMPL1PTBL_MTX_HASH(p)   \
	((((u_int)(p) >> 12) ^ ((u_int)(p) >> 2)) & (N_TMPL1PTBL_MTX - 1))
static	kmutex_t tmpl1ptbl_lock[N_TMPL1PTBL_MTX];

#endif	/* SRMMU_TMPUNLOAD */


#define	MLIST_HASH(pp)		&mml_table[(((u_int)(pp))>>6) & (MLIST_SIZE-1)]
kmutex_t			mml_table[MLIST_SIZE];

/*
 * A pile of conversion macros
 */
#define	VA2L1PTBL(base, va)	((base) + ((u_int)(va) >> 30))

#define	BASE2VA(p)	((caddr_t)((p)->ptbl_base << 16))
#define	VA2BASE(v)	((u_short)(((u_int)(v))  >> 16))

#define	PP2PTBL(pp)	((ptbl_t *)((u_int)((pp)->p_mapping) & 0xfffffffc))
#define	MAP2PTBL(map)	((ptbl_t *)((u_int)(map) & 0xfffffffc))

static page_t	*pfn2pp_hash(u_int pfn);

#ifdef DEBUG

#define	N_SRMMU_LOG	(0x1000)
static struct srmmu_log {
	u_int data[8];
} srmmu_log[N_SRMMU_LOG];

static struct srmmu_log *psrmmu_log = srmmu_log;
static kmutex_t srmmu_log_mtx;
const char *srmmu_version = "09/22/99 1.236";

static void
srmmu_logger(u_int a, u_int b, u_int c, u_int d, u_int e, u_int f,
	u_int g, u_int h)
{
	struct srmmu_log *p;

	mutex_enter(&srmmu_log_mtx);
	p = psrmmu_log;
	psrmmu_log++;
	if (psrmmu_log >= &srmmu_log[N_SRMMU_LOG]) {
		psrmmu_log = srmmu_log;
	}
	mutex_exit(&srmmu_log_mtx);

	p->data[0] = a; p->data[1] = b; p->data[2] = c;
	p->data[3] = d; p->data[4] = e; p->data[5] = f;
	p->data[6] = g; p->data[7] = h;
}

#define	SRMMU_LOG(a, b, c, d, e, f) \
	(srmmu_logger((u_int) __LINE__, (u_int) curthread, (u_int) (a), \
	    (u_int) (b), (u_int) (c), (u_int) (d), (u_int) (e), (u_int) (f)))

#else	/* DEBUG */
#define	SRMMU_LOG(a, b, c, d, e, f)
#endif	/* DEBUG */

/*
 * tatoptbl - translates the physical page table address into a virtual
 * ptbl address.
 */
static void
tatoptbl_pte(u_int ta, ptbl_t **ptbl, /* struct pte */ union ptpe **pte)
{
	page_t		*pp;
	ptbl_gr_t	*ptbl_gr;

	pp = pfn2pp_hash((u_int)(ta >> MMU_STD_PTPSHIFT));
	ASSERT(mach_pp->p_pagenum == (ta >> MMU_STD_PTPSHIFT));
	ASSERT(pp->p_next == (page_t *)0xdeaddead);
	*pte = (/* struct pte */ union ptpe *)((caddr_t)pp->p_offset +
	    (((u_int)ta << 6) & 0xfff));
	ptbl_gr = (ptbl_gr_t *)pp->p_prev;
	*ptbl = &ptbl_gr->pg_ptbl[((u_int)ta  >> 2) & 0xf];
}

static struct ptbl *
tatoptbl(u_int ta)
{
	page_t		*pp;
	ptbl_gr_t	*ptbl_gr;
	ptbl_t		*ptbl;

	pp = pfn2pp_hash((u_int)(ta >> MMU_STD_PTPSHIFT));
	ASSERT(mach_pp->p_pagenum == (ta >> MMU_STD_PTPSHIFT));
	ASSERT(pp->p_next == (page_t *)0xdeaddead);
	ptbl_gr = (ptbl_gr_t *)pp->p_prev;
	ptbl = &ptbl_gr->pg_ptbl[((u_int)ta >> 2) & 0xf];

	return (ptbl);
}

/*
 * Prototypes for srmmu resource allocation routines.
 */
static u_int		get_static_ctx(u_int);

static srmmu_t		*srmmu_srmmu_alloc();
static void		srmmu_srmmu_free(srmmu_t *);

static struct pte	*build_subtree(ptbl_t *, int, struct as *, caddr_t,
			    ptbl_t **, kmutex_t **, int);

static ptbl_t	*srmmu_l1_ptbl_alloc();
static void	srmmu_l1_ptbl_free(ptbl_t *);
static ptbl_t	*srmmu_ptbl_alloc(u_int, u_int, int, kmutex_t **);
void		srmmu_ptbl_free(ptbl_t *, kmutex_t *);

/*
 * this puppy must fit in one page
 */
struct pt_gr {
	pt_t		pg_pt[PTBL_GROUP];
};
typedef struct pt_gr	pt_gr_t;

kmutex_t	free_ptbl_gr_lock;
ptbl_t		*free_ptbl_gr;

kmutex_t	free_l1_ptbl_lock;
ptbl_t		*free_l1_ptbl;
kmutex_t	ptbl_gr_l1_pool_lock;
ptbl_gr_t	*ptbl_gr_l1_pool;

static ptbl_gr_t	*ptbl_gr_pool;
static ptbl_gr_t	*ptbl_gr_hand;
static ptbl_t		*ptbl_last_stolen;
static kmutex_t		ptbl_gr_pool_lock;

static struct srhment *get_hment();
static void put_hment(struct srhment *);

static void hashin_ptetohme(struct pte *, struct srhment *);
static struct srhment *hashout_ptetohme(ptbl_t *, struct pte *);

/* Rename ptetohme() to pte2hme(). */
static struct srhment *pte2hme(ptbl_t *, struct pte *);

static void hme_may_sub(ptbl_t *, struct pte *, struct srhment *, page_t *);

static int hme_may_add(ptbl_t *, struct pte *, page_t *,
    u_int flags, struct hat *, struct pte *, struct srhment **);

static void hme_list_add(struct srhment *, page_t *);
static void hme_list_sub(struct srhment *, page_t *);
static void convert_one_mapping(page_t *, srhment_t *);

static void fill_l1_group(ptbl_t *, struct as *, int);

#if defined(SRMMU_TMPUNLOAD)
static void tmpunload_l2(struct as *, caddr_t, struct ptp, ptbl_t *);
static void tmpunload_l3(struct as *, caddr_t, struct ptp, ptbl_t *);


static struct ptp *recover_l2tmp(srmmu_t *, struct as *,
	caddr_t, struct ptp *, ptbl_t **);

static struct pte *recover_l3tmp(struct as *, caddr_t,
	struct ptp *, ptbl_t *, ptbl_t **, kmutex_t **);
#endif	/* SRMMU_TMPUNLOAD */

extern int swapl(int, int *);
extern int ldphys(int);

static	hment_pool_t	hment_pool[NCPU];

#ifdef VAC

#define	MAX_SPT_SHMSEG	20

static	kmutex_t	spt_as_mtx;

struct  spt_seglist {
	struct  seg		*spt_seg;
	struct  spt_seglist	*spt_next;
};

struct	spt_aslist {
	struct  as		*spt_as;
	struct  spt_seglist	*spt_list;
	struct  spt_aslist	*spt_next;
	int			spt_count;
};

static struct	spt_aslist *spt_ashead = NULL;
static int	max_spt_shmseg = MAX_SPT_SHMSEG;

void    spt_mtxinit();
void    spt_vacsync(char *, struct as *);
static  void    spt_addsptas(struct as *);
static  void    spt_delsptas(struct as *);
static  void    spt_addsptseg(struct seg *, struct as *);
static  void    spt_delsptseg(struct seg *, struct as *);
#endif /* VAC */

/*
 * Initilaize ourselves.
 * Called early during boot, before any srmmu service is needed.
 */
static void
srmmu_lock_init(void)
{
#ifdef VAC

	/*
	 * initialize a mutex for the list of address spaces needed
	 * to insure cache consistency for non-iocoherent VAC machines
	 */
	if (cache & CACHE_VAC)
		spt_mtxinit();
#endif /* VAC */
}

/*
 * Initialize the hardware address translation structures.
 * Called by hat_init() after the vm structures have been allocated
 * and mapped in.
 */
static void
srmmu_init(void)
{
	/* srmmu_ctx is a signed short. */
	ASSERT(nctxs <= 0x8000);

	/*
	 * Initialize kmem allocator pools.
	 *
	 * We have 4 pools:
	 *	srmmu_cache - for srmmu structures.
	 *	srhme_cache - for srhment_t structures.
	 *	srptbl_cache - for ptbl_gr_t's
	 *	srpt_cache - for the pt's
	 */
	srmmu_cache = kmem_cache_create("srmmu_cache", sizeof (srmmu_t), 0,
	    NULL, NULL, NULL, NULL, NULL, 0);

	srhme_cache = kmem_cache_create("srhme_cache", sizeof (srhment_t), 0,
	    NULL, NULL, NULL, NULL, NULL, KMC_NODEBUG);

	/*
	 * All the arrays were zero filled in startup(), so there is no
	 * need to initialize them here (luckily 0 is an invalid pte).
	 */
	kctx = &ctxs[KCONTEXT];
	kctx->c_as = &kas;

	kl1ptbl = srmmu_l1_ptbl_alloc();
	fill_l1_group(kl1ptbl, &kas, 0);

	kl1pt = (struct l1pt *)ptbltopt_va(kl1ptbl);

	ksrmmu.s_l1ptbl = kl1ptbl;
	ksrmmu.s_ctx = KCONTEXT;

	kl1ptp.ptpe_int = ptbltopt_ptp(ksrmmu.s_l1ptbl);

	/*
	 * The first NUM_LOCKED_CTXS (0, .. NUM_LOCKED_CTXS-1)
	 * contexts are always locked, so we start allocating
	 * contexts at the context whose number is NUM_LOCKED_CTXS.
	 */
	ctxhand = &ctxs[NUM_LOCKED_CTXS];

}

static void
fill_l1_group(ptbl_t *ptbl, struct as *as, int flags)
{
	int i, j;

	ASSERT((flags & ~PTBL_TMP) == 0);

	for (j = i = 0; i < 4; i++, ptbl++) {
		ptbl->ptbl_as = as;
		ASSERT((j & 0xffff) == 0);
		ptbl->ptbl_base = VA2BASE(j);
		ptbl->ptbl_flags = flags | PTBL_LEVEL_1 | PTBL_ALLOCED;
		ptbl->ptbl_parent = 0;
		j += L1_VA_PER_PTBL;
	}
}

/*
 * Choose initial page coloring bin based based on *as.
 * Since sizeof(struct as) is 48, contig struct as' get seeds
 * starting 12 bins apart.  Seeds are masked for overflow on use.
 */
#define	AS_2_COLOR_SEED(as)	((u_int)(as) >> 2)

/*
 * Allocate a level-1 page table for this address space.
 * Initialize it so that it has the kernel mappings.
 * If static_ctx is set, we also set up context[] to
 * point to the L1. But- the h/w context register is
 * NOT set to this new context yet.
 */
static void
srmmu_alloc(hat, as)
	register struct hat *hat;
	register struct as *as;
{
	srmmu_t		*srmmu;
	extern u_int	get_color_flags(struct as *);
	int		ctx;
	pt_t		*root;

	if (as == &kas) {
		ksrmmu.su_hat = as->a_hat;
		hat->hat_data[HAT_DATA_SRMMU] = (u_int)&ksrmmu;
		hat->hat_data[HAT_DATA_COLOR_FLAGS] = get_color_flags(as);
		hat->hat_data[HAT_DATA_COLOR_BIN] = AS_2_COLOR_SEED(as);
		return;
	}

	mutex_enter(&hat->hat_mutex);

	srmmu = srmmu_srmmu_alloc();

	srmmu->su_hat = hat;

	/*
	 * Make sure the L1pt starts with the known kernel entries.
	 */
	root = (pt_t *)ptbltopt_va(srmmu->s_l1ptbl);
	bcopy((caddr_t)kl1pt, (caddr_t)root, sizeof (struct l1pt));

	fill_l1_group(srmmu->s_l1ptbl, as, 0);

	if (static_ctx) {
		union ptpe	l1ptp;

		ctx = srmmu->s_ctx;

		ASSERT(VALID_UCTX(ctx));
		ctxs[ctx].c_as = as;

		l1ptp.ptpe_int = ptbltopt_ptp(srmmu->s_l1ptbl);

		mmu_writeptp_locked(&contexts[ctx], l1ptp.ptpe_int,
			NULL, 0, -1, 0);
	}

	ASSERT(hattosrmmu(hat) == NULL);

	hat->hat_data[HAT_DATA_SRMMU] = (u_int)srmmu;
	hat->hat_data[HAT_DATA_COLOR_FLAGS] = get_color_flags(as);
	hat->hat_data[HAT_DATA_COLOR_BIN] = AS_2_COLOR_SEED(as);

	mutex_exit(&hat->hat_mutex);
}

static void
srmmu_free(hat, as)
	register struct hat *hat;
	register struct as *as;
{
	register srmmu_t *srmmu;
	register int ctx;
	int flag = 0;

#ifdef VAC
	if (cache & CACHE_VAC)
		spt_delsptas(as);
#endif /* VAC */

	mutex_enter(&hat->hat_unload_other);
	mutex_enter(&hat->hat_mutex);
	srmmu = hattosrmmu(hat);
	ctx = srmmu->s_ctx;

	/*
	 * Disconnect from the context table so hardware
	 * table walk can not get to any ptes after this point.
	 */
	if (ctx != -1) {
		ASSERT(VALID_UCTX(ctx));
#ifdef VAC
		if (do_pg_coloring && (cache & CACHE_VAC)) {
			flag = SR_NOPGFLUSH;
		}
#endif
		mmu_writeptp_locked(&contexts[ctx], kl1ptp.ptpe_int,
		    NULL, 0, ctx, flag);

		if (!static_ctx) {
			srmmu->s_ctx = -1;
		}
	}
	mutex_exit(&hat->hat_mutex);

	if (srmmu->s_rmstat) {
		hat_freestat(as, NULL);
	}

	/* Switch to kernel context */
	mmu_setctxreg(KCONTEXT);

	srmmu_l1inval(as, srmmu->s_l1ptbl, 0);

#if defined(SRMMU_TMPUNLOAD)
	SRMMU_LOG(srmmu, as, ctx, srmmu->s_l1ptbl, srmmu->s_tmpl1ptbl, flag);

	if (srmmu->s_tmpl1ptbl != NULL) {
		srmmu_l1inval(as, srmmu->s_tmpl1ptbl, 0);
	}
#else /* defined(SRMMU_TMPUNLOAD) */
	SRMMU_LOG(srmmu, as, ctx, srmmu->s_l1ptbl, 0, flag);
#endif /* defined(SRMMU_TMPUNLOAD) */

	/*
	 * Disassociate the ctx from its as.
	 * Reclaim the ctx to the srmmu if they're static.
	 */
	if (ctx != -1) {
		if (static_ctx) {
			ctxs[ctx].c_as = NULL;	/* This frees the context */
		} else {
			mutex_enter(&srmmu_ctx_mtx);
			/*
			 * Ctx was read above under the hat
			 * mutex, but we already dropped hat
			 * mutex so this ctx can be stolen.
			 * This means we need to check to
			 * see if this ctx is still ours before
			 * nulling it.
			 */
			if (ctxs[ctx].c_as == as) {
				/* This frees the context */
				ctxs[ctx].c_as = NULL;
			}
			mutex_exit(&srmmu_ctx_mtx);
		}
	}

	/* invalidate srmmu pointer. */
	hat->hat_data[HAT_DATA_SRMMU] = 0;
	mutex_exit(&hat->hat_unload_other);
	srmmu_srmmu_free(srmmu);
}

/*
 * For now, we do not duplicate the level-1 page table when
 * we are duplicating address spaces.  Rather, we let the
 * forked process fault in the mappings that the parent process
 * had.
 */
/* ARGSUSED */
int
srmmu_dup(hat, as, newas)
	register struct hat *hat;
	register struct as *as;
	register struct as *newas;
{
	return (0);
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 * We know L1/srmmu/hat are always there. So just need to make sure
 * context[] is pointing to our l1pt.
 */
/* ARGSUSED */
void
srmmu_swapin(hat, as)
	struct hat *hat;
	struct as *as;
{
}

/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 * Differences from srmmu_free():
 *
 *	1) The level 1 page table is not freed and consequently the root ptp for
 *	   the context is not invalidated in the context table and the srmmu
 *	   struct is not freed.
 *	2) The cpusran mask in the srmmu struct is not reset to
 *	   zero.
 */


/* ARGSUSED */
void
srmmu_swapout(hat, as)
	struct hat *hat;
	struct as *as;
{
	register srmmu_t *srmmu;
	register int ctx;
	union ptpe l1ptp;

	mutex_enter(&hat->hat_mutex);
	srmmu = hattosrmmu(hat);

	if (srmmu == NULL) {
		cmn_err(CE_PANIC, "no srmmu ptr");
	}
	ctx = srmmu->s_ctx;

	/*
	 * Null out context table entry and flushes all TLBs
	 * associated with this ctx.
	 */
	if (ctx != -1) {
		ASSERT(VALID_UCTX(ctx));
		ASSERT(ctxs[ctx].c_as == as);

		mmu_writeptp_locked(&contexts[ctx], kl1ptp.ptpe_int,
			NULL, 0, ctx, 0);

		if (!static_ctx) {
			srmmu->s_ctx = -1;
		}
	}
	mutex_exit(&hat->hat_mutex);

	/*
	 * Free up mapping resources now.
	 */
	srmmu_l1inval(as, srmmu->s_l1ptbl, 1);

#if defined(SRMMU_TMPUNLOAD)
	if (srmmu->s_tmpl1ptbl != NULL) {
		srmmu_l1inval(as, srmmu->s_tmpl1ptbl, 1);
	}
#endif defined(SRMMU_TMPUNLOAD)

	if (static_ctx) {
		/*
		 * Now that we've done with releasing all page table,
		 * put back context pointer so when we swap in again
		 * we'll be ready. No need to flush, there should
		 * be nothing left.
		 */
		l1ptp.ptpe_int = ptbltopt_ptp(srmmu->s_l1ptbl);
		SET_NEW_PTP(&contexts[ctx], l1ptp.ptpe_int, 0, 0, 0);
	} else {
		if (ctx != -1) {
			/*
			 * Disassociate the ctx from its as.
			 */
			mutex_enter(&srmmu_ctx_mtx);
			/*
			 * See comment in srmmu_free() about
			 * why we need to verify c_as here.
			 */
			if (ctxs[ctx].c_as == as) {
				/* This frees the context */
				ctxs[ctx].c_as = NULL;
			}
			mutex_exit(&srmmu_ctx_mtx);
		}
	}
}

/*
 * Set up addr to map to page pp with protection prot.
 */
/* ARGSUSED */
void
srmmu_memload(hat, as, addr, pp, attr, flags)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
	struct page *pp;
	u_int attr;
	int flags;
{
	struct pte apte;

	ASSERT(PAGE_LOCKED(pp));

#ifdef VAC
	if ((cache & CACHE_VAC) && (flags & HAT_LOAD_SHARE))
		spt_addsptas(as);
#endif /* VAC */

	if (hattosrmmu(hat)->s_rmstat)
		hat_resvstat(MMU_PAGESIZE, as, addr);

	srmmu_mempte(pp, attr, &apte, addr);
	(void) srmmu_pteload(as, addr, pp, &apte, 3, attr, flags);
}

/*
 * Cons up a struct pte using the device's pf bits and protection
 * prot to load into the hardware for address addr; treat as minflt.
 *
 * Note: hat_devload can be called to map real memory (e.g.
 * /dev/kmem) and even though hat_devload will determine pf is
 * for memory, it will be unable to get a shared lock on the
 * page (because someone else has it exclusively) and will
 * pass dp = NULL.  If srmmu_pteload doesn't get a non-NULL
 * page pointer it can't cache memory.  Since all memory must
 * always be cached for sun4d, we call vrfy_is_mem() to cover
 * this case.
 */
/* ARGSUSED */
void
srmmu_devload(hat, as, addr, dp, pf, attr, flags)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
	devpage_t *dp;
	u_int pf;
	u_int attr;
	int flags;
{
	union	ptpe ptpe;

#ifdef VAC
	if ((cache & CACHE_VAC) && (flags & HAT_LOAD_SHARE))
		spt_addsptas(as);
#endif /* VAC */

	if (hattosrmmu(hat)->s_rmstat)
		hat_resvstat(MMU_PAGESIZE, as, addr);

	ptpe.ptpe_int = MMU_STD_INVALIDPTP;
	ptpe.pte.PhysicalPageNumber = pf;
	ptpe.pte.AccessPermissions = srmmu_vtop_prot(addr, attr);
#ifdef sun4d
	/* sun4d memory is always cacheable */
	ptpe.pte.Cacheable = vrfy_is_mem(pf);
#else
	if (!(cache & CACHE_VAC)) {
		/*
		 * If PAC, no aliasing problem. cacheable is it's
		 * memory.
		 */
		ptpe.pte.Cacheable = vrfy_is_mem(pf);
	} else {
		/*
		 * If VAC, aliasing problem, cacheable only if it's
		 * memory backed up by page_t (so p_mapping can be
		 * tracked) and no HAT_LOAD_NOCONSIST flag.
		 */
		ptpe.pte.Cacheable = (!(flags & HAT_LOAD_NOCONSIST) &&
		    (pfn2pp_hash(pf) != NULL));
	}
#endif
	ptpe.pte.EntryType = MMU_ET_PTE;

	/* support only L3 for now ... */
	(void) srmmu_pteload(as, addr, (struct page *)dp,
		(struct pte *)&ptpe, 3, attr, flags);
}

/*
 * Set up translations to a range of virtual addresses backed by
 * physically contiguous memory.  If the pages satisfy the length
 * and alignment criterion, use l1 or l2 ptes to do the mapping.
 */
/*ARGSUSED*/
static void
srmmu_contig_memload(hat, as, addr, pp, prot, flags, len)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
	struct page *pp;
	u_int prot;
	int flags;
	u_int len;
{
	struct pte apte;
	int ret;

	ASSERT((len & MMU_PAGEOFFSET) == 0);
#ifdef VAC
	if ((cache & CACHE_VAC) && (flags & HAT_LOAD_SHARE))
		spt_addsptas(as);
#endif /* VAC */
	while (len) {
		srmmu_mempte(pp, prot, &apte, addr);

		if (mmu_l3only) {
			goto load_l3;
		}
		if (len >= MMU_L1_SIZE && MMU_L1_OFF(addr) == 0 &&
		    (page_pptonum(pp) & MMU_STD_FIRSTMASK) == 0) {
			ret = srmmu_pteload(as, addr, pp, &apte, 1, prot,
			    flags);
			if (ret == 0) {
				cmn_err(CE_PANIC, "load l1 failed?");
			}

			pp = (page_t *)
			    (((machpage_t *)pp) + mmu_btop(MMU_L1_SIZE));
			addr += MMU_L1_SIZE;
			len -= MMU_L1_SIZE;

		} else if (len >= MMU_L2_SIZE && MMU_L2_OFF(addr) == 0 &&
			(page_pptonum(pp) & MMU_STD_SECONDMASK) == 0) {
			ret = srmmu_pteload(as, addr, pp, &apte, 2, prot,
			    flags);
			if (ret == 0) {
				cmn_err(CE_PANIC, "load l2 failed?");
			}

			pp = page_nextn_raw(pp, mmu_btop(MMU_L2_SIZE));
			addr += MMU_L2_SIZE;
			len -= MMU_L2_SIZE;

		} else {
load_l3:
			(void) srmmu_pteload(as, addr, pp, &apte, 3, prot,
			    flags);
			pp = page_next_raw(pp);
			addr += MMU_PAGESIZE;
			len -= MMU_PAGESIZE;
		}
	}
}

/*
 * Set up translations to a range of virtual addresses backed by
 * physically contiguous device memory.  'pf' is the base physical page
 * frame number of the physically contiguous memory and 'dp' is non-null for
 * device memory backed by devpages.  If the pf satisfies the length
 * and alignment criterion, use l1 or l2 ptes to do the mapping.
 */
/*ARGSUSED*/
void
srmmu_contig_devload(hat, as, addr, dp, pf, prot, flags, len)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
	devpage_t *dp;
	u_int pf;
	u_int prot;
	int flags;
	u_int len;
{
	union ptpe ptpe;

	ASSERT((len & MMU_PAGEOFFSET) == 0);

	if (hattosrmmu(hat)->s_rmstat)
		hat_resvstat(len, as, addr);

	while (len) {
		ASSERT(!pf_is_memory(pf) || (flags & HAT_LOAD_NOCONSIST));
		ptpe.ptpe_int = MMU_STD_INVALIDPTE;
		ptpe.pte.AccessPermissions = srmmu_vtop_prot(addr, prot);
#ifdef sun4d
		/* sun4d memory is always cacheable */
		ptpe.pte.Cacheable = vrfy_is_mem(pf);
#else
		if (!(cache & CACHE_VAC)) {
			/*
			 * If PAC, no aliasing problem. cacheable is it's
			 * memory.
			 */
			ptpe.pte.Cacheable = vrfy_is_mem(pf);
		} else {
			/*
			 * If VAC, aliasing problem, cacheable only if it's
			 * memory backed up by page_t (so p_mapping can be
			 * tracked) and no HAT_LOAD_NOCONSIST flag.
			 */
			ptpe.pte.Cacheable = (!(flags & HAT_LOAD_NOCONSIST) &&
			    (pfn2pp_hash(pf) != NULL));
		}
#endif
		ptpe.pte.EntryType = MMU_ET_PTE;
		ptpe.pte.PhysicalPageNumber = pf;

		if (mmu_l3only) {
			goto load_l3;
		}

		if (len >= MMU_L1_SIZE && MMU_L1_OFF(addr) == 0 &&
		    (pf & MMU_STD_FIRSTMASK) == 0) {
			if (srmmu_pteload(as, addr, NULL, &ptpe.pte, 1,
			    prot, flags) == 0) {
				cmn_err(CE_PANIC, "load l1 failed");
			}
			pf += mmu_btop(MMU_L1_SIZE);
			addr += MMU_L1_SIZE;
			len -= MMU_L1_SIZE;
		} else if (len >= MMU_L2_SIZE && MMU_L2_OFF(addr) == 0 &&
		    (pf & MMU_STD_SECONDMASK) == 0) {
			if (srmmu_pteload(as, addr, NULL, &ptpe.pte, 2,
			    prot, flags) == 0) {
				cmn_err(CE_PANIC, "load l2 failed");
			}
			pf += mmu_btop(MMU_L2_SIZE);
			addr += MMU_L2_SIZE;
			len -= MMU_L2_SIZE;
		} else {
load_l3:
			(void) srmmu_pteload(as, addr, NULL, &ptpe.pte, 3, prot,
			    flags);
			pf++;
			addr += MMU_PAGESIZE;
			len -= MMU_PAGESIZE;
		}
	}
}

/*
 * Release one hardware address translation lock on the given address.
 * This means decrementing the keep count on the page table.
 */
/* ARGSUSED */
static void
srmmu_unlock(hat, as, addr, len)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
	u_int len;
{
	register struct pte *pte;
	ptbl_t *ptbl;
	struct pte tpte;
	u_int span;
	int level;
	kmutex_t *mtx;

	ASSERT(as->a_hat == hat);
	ASSERT((len & MMU_PAGEOFFSET) == 0);

#ifndef VAC
	if (as == &kas) {
		return;
	}
#endif

	for (; len; len -= span, addr += span) {
		pte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx,
		    LK_PTBL_SHARED);
		mmu_readpte(pte, &tpte);
		if (!pte_valid(&tpte)) {
			cmn_err(CE_PANIC, "srmmu_unlock %p %p %x",
			    (void *)ptbl, (void *)pte, *(int *)&tpte);
		}
		if (level != 3) {
			/*
			 * non-level 3's are always locked against stealing,
			 * so just adjust loop counters.
			 */
			span = srmmu_sizes[level];
			span -= ((u_int)addr & (span - 1));
			span = MIN(len, span);
		} else {
			span = MIN(len, L3PTSIZE - MMU_L2_OFF(addr));
			ptbl->ptbl_lockcnt -= (mmu_btop(span));

#ifdef VAC
#ifdef RECOVER_AT_UNLOAD
			/*
			 * The ptbl can't go away because we have ptbl locked.
			 * Check to see if there are TNC mappings that can be
			 * unloaded so they will fault back in with the correct
			 * cacheable bit.
			 */
			if ((cache & CACHE_VAC) && (ptbl->ptbl_lockcnt == 0)) {
				u_int	i;

				pte = ptbltopt_va(ptbl);
				for (i = 0; i < NL3PTEPERPT; i++, pte++) {
					mmu_readpte(pte, &tpte);
					if (pte_valid(&tpte)) {
						page_t *pp;

						pp = pfn2pp_hash
						    (tpte.PhysicalPageNumber);

						if (pp && PP_ISTNC(pp)) {
							(void) srmmu_pteunload(
							    ptbl, pte, 0,
							    NO_MLIST_LOCK);
						}
					}

					if (ptbl->ptbl_validcnt == 0) {
						break;
					}
				}
			}
#endif
#endif
		}

		unlock_ptbl(ptbl, mtx);
	}
}

/* ARGSUSED */
faultcode_t
srmmu_fault(hat, addr)
	struct hat *hat;
	caddr_t addr;
{
	return (FC_NOMAP);
}

#define	N_L2_FLUSH_TRIGGER	(0x2000)

static void
srmmu_do_attr(struct as *as, caddr_t addr, size_t len, u_int attr, int mode)
{
	struct pte *pte, *lastpte, *tmppte;
	int pprot;
	struct pte old;
	int level, span;
	caddr_t a, base;
	ptbl_t *ptbl, *l2ptbl;
	kmutex_t *mtx, *l2mtx;
	int flag;
	union ptpe savel2;
	struct ptp *l2ptp;
	struct hat *hat;

	ASSERT((attr & ~(HAT_PROT_MASK)) == 0);

	hat = as->a_hat;

	if (mode != MHAT_CLRATTR) {
		pprot = srmmu_vtop_prot(addr, attr);
	} else {
		ASSERT((attr & ~(PROT_WRITE | PROT_USER)) == 0);
		pprot = -1;
	}

	for (; len; len -= span, addr += span) {
		pte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx, 0);
		flag = 0;
		savel2.ptpe_int = 0;

		/*
		 * If there is no page table, move the address up
		 * to the start of the next page table to avoid
		 * searching a bunch of invalid ptes.
		 */
		mmu_readpte(pte, &old);
		span = srmmu_sizes[level];
		if (pte_valid(&old)) {
			if (level == 3) {
				span = MIN(len, L3PTSIZE -
					((u_int)addr & L3PTOFFSET));
				lastpte = pte + mmu_btop(span);
				a = addr;
				tmppte = pte;
				if (span > N_L2_FLUSH_TRIGGER && as != &kas) {
					l2ptbl = ptbl->ptbl_parent;

					/*
					 * Must use NOWAIT option since we
					 * already got L3 lock.
					 */
					if (lock_ptbl(l2ptbl, LK_PTBL_NOWAIT,
					    as, addr, 2, &l2mtx) ==
					    LK_PTBL_OK) {
						l2ptp = (struct ptp *)
							ptbltopt_va(l2ptbl) +
							MMU_L2_INDEX(addr);
						mmu_readptp(l2ptp,
						    (struct ptp *)&savel2);
						ASSERT(savel2.ptpe_int != 0);
						mmu_writeptp(l2ptp,
						    MMU_ET_INVALID,
						    MMU_L2_BASE(addr), 2, hat,
						    0);
						flag |= SR_NOFLUSH;
					}
				}

				while (tmppte < lastpte) {
					if (pte_valid(tmppte)) {
						srmmu_ptechgprot(tmppte, attr,
							pprot, a, 3, hat, flag);
					}
					a += MMU_PAGESIZE;
					tmppte++;
				}

				if (savel2.ptpe_int) {
					SET_NEW_PTP(l2ptp, savel2.ptpe_int,
					    MMU_L2_BASE(addr), 2, 0);
					unlock_ptbl(l2ptbl, l2mtx);
				}
			} else {
				span = srmmu_sizes[level];
				base = (level == 1) ?
					MMU_L1_BASE(addr) : MMU_L2_BASE(addr);
				if ((addr != base) || (len < span)) {
					cmn_err(CE_PANIC,
					    "chgprot in big page");
					span -= (addr - base);
					span = MIN(len, span);
					continue;
				}

				srmmu_ptechgprot(pte, attr, pprot, addr, level,
					hat, flag);
			}
		} else {
			span = srmmu_sizes[level];
			span -= ((u_int)addr & (span - 1));
			span = MIN(len, span);
		}

		unlock_ptbl(ptbl, mtx);
	}
}

/*
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If vprot is ~PROT_WRITE,
 * then remove write permission, leaving the other
 * permissions unchanged.  If vprot is ~PROT_USER, remove user permissions.
 */
void
srmmu_chgprot(struct as *as, caddr_t addr, u_int len, u_int vprot)
{
	if (vprot == (u_int)~PROT_USER) {
		srmmu_do_attr(as, addr, len, PROT_USER, MHAT_CLRATTR);
	} else {
		if (vprot == (u_int)~PROT_WRITE) {
			srmmu_do_attr(as, addr, len, PROT_WRITE, MHAT_CLRATTR);
		} else {
			srmmu_do_attr(as, addr, len, vprot, MHAT_CHGATTR);
		}
	}
}

static int
srmmu_vtop_clrattr(int opprot, u_int vprot, int *pprot)
{
	if (vprot == PROT_WRITE) {
		switch (opprot) {
		case MMU_STD_SRWURW:
		case MMU_STD_SRWUR:
			*pprot = MMU_STD_SRUR;
			return (1);

		case MMU_STD_SRWXURWX:
			*pprot = MMU_STD_SRXURX;
			return (1);

		case MMU_STD_SRWX:
			*pprot = MMU_STD_SRX;
			return (1);

		case MMU_STD_SRUR:
		case MMU_STD_SRXURX:
		case MMU_STD_SXUX:
		case MMU_STD_SRX:
			return (0);
		}
	}

	if (vprot == PROT_USER) {
		switch (opprot) {
		case MMU_STD_SRWURW:
		case MMU_STD_SRWXURWX:
		case MMU_STD_SRWUR:
			*pprot = MMU_STD_SRWX;
			return (1);

		case MMU_STD_SRUR:
		case MMU_STD_SXUX:
		case MMU_STD_SRXURX:
			*pprot = MMU_STD_SRX;
			return (1);

		case MMU_STD_SRWX:
		case MMU_STD_SRX:
			return (0);
		}
	}

	if (vprot == (PROT_USER | PROT_WRITE)) {
		switch (opprot) {
		case MMU_STD_SRWURW:
		case MMU_STD_SRWXURWX:
		case MMU_STD_SRWUR:
		case MMU_STD_SRUR:
		case MMU_STD_SXUX:
		case MMU_STD_SRXURX:
		case MMU_STD_SRWX:
			*pprot = MMU_STD_SRX;
			return (1);

		case MMU_STD_SRX:
			return (0);
		}
	}

	cmn_err(CE_PANIC, "srmmu_vtop_clrattr: bad vprot %x", vprot);

	/* NOT REACHED */
	return (0);
}

/*
 * Change the AccessPermissions of the given pte.
 */
static void
srmmu_ptechgprot(struct pte *pte, u_int vprot, int pprot, caddr_t addr,
	int level, struct hat *hat, int flag)
{
	int newprot, oldprot;
	struct pte old;

	mmu_readpte(pte, &old);

	ASSERT(pte_valid(&old));

	oldprot = old.AccessPermissions;
	if (pprot == -1) {
		newprot = srmmu_vtop_clrattr(oldprot, vprot, &pprot);
	} else if (oldprot != pprot) {
			newprot = 1;
	} else {
		newprot = 0;
	}

	if (newprot) {
		if (pprot & PTE_ACC_WRITE) {
			page_t *pp;

			pp = pfn2pp_hash(old.PhysicalPageNumber);
			if (pp && PP_ISRO(pp)) {
				srmmu_page_enter(pp);
				PP_CLRRO(pp);
				srmmu_page_exit(pp);
			}
		}

		old.AccessPermissions = (u_char)pprot;

		if (enable_mbit_wa && old.AccessPermissions == MMU_STD_SRWXURWX)
				old.Modified = 1;

		MOD_VALID_PTE(flag, pte, *(u_int *)&old, addr,
				level, hat, PTE_RM_MASK);
	}
}

#if defined(SRMMU_TMPUNLOAD)

/*
 * This routine puts a `real' L2 ptbl (rl2ptbl) onto the fake tree.
 */
static void
tmpunload_l2(struct as *as, caddr_t addr, struct ptp rl1ptp, ptbl_t *rl2ptbl)
{
	struct ptp *fl1ptp;
	struct ptp tmp;
	ptbl_t *fl1ptbl, *fl2ptbl;
	kmutex_t *rl2mtx, *fl1mtx, *fl2mtx;
	srmmu_t		*srmmu;
	u_char		flags;

	ASSERT(PTBL_LEVEL(rl2ptbl->ptbl_flags) == 2);

	(void) lock_ptbl(rl2ptbl, 0, as, addr, 2, &rl2mtx);

	/* Mark PTBL_TMP on all L3s connecting to this L2. */
	mod_l3tmp_bits(rl2ptbl, as, addr, SET_TMP);

	srmmu = astosrmmu(as);
	fl1ptbl = srmmu->s_tmpl1ptbl;
	fl1ptp = (struct ptp *)ptbltopt_va(fl1ptbl) + MMU_L1_INDEX(addr);
	fl1ptbl = VA2L1PTBL(srmmu->s_tmpl1ptbl, addr);

	(void) lock_ptbl(fl1ptbl, LK_PTBL_TMP, as, addr, 1, &fl1mtx);

	mmu_readptp(fl1ptp, &tmp);
	switch (tmp.EntryType) {
		case MMU_ET_INVALID:
			/*
			 * Link rl2ptbl to the fake tree. Note
			 * that as soon as PTBL_TMP is set, the
			 * rl2mtx no longer protects the rl2ptbl.
			 */
			rl2ptbl->ptbl_parent = fl1ptbl;
			flags = rl2ptbl->ptbl_flags;
			flags |= PTBL_TMP;
			flags &= ~PTBL_LOCKED;
			rl2ptbl->ptbl_flags = flags;
			mutex_exit(rl2mtx);

			SRMMU_LOG(as, addr, rl2ptbl, fl1ptbl, flags, 0);

			SET_TNEW_PTP(fl1ptp, *(u_int *) &rl1ptp,
			    MMU_L1_BASE(addr), 1, 0);
			break;

		case MMU_ET_PTP:
			/*
			 * There's already a fake l2 in the tmp tables so
			 * merge all of entries on rl2ptbl into existing fake
			 * l2.
			 */
			fl2ptbl = tatoptbl(tmp.PageTablePointer);
			(void) lock_ptbl(fl2ptbl, LK_PTBL_TMP, as, addr, 2,
			    &fl2mtx);
			srmmu_tmpmergel2(fl2ptbl, rl2ptbl, addr);
			unlock_ptbl(fl2ptbl, fl2mtx);

			SRMMU_LOG(as, addr, rl2ptbl, fl1ptbl, 0, fl2ptbl);

			srmmu_ptbl_free(rl2ptbl, rl2mtx);
			break;

		default:
			cmn_err(CE_PANIC, "tmpunload_l2: l1pte %x",
			    *(u_int *)&tmp);
	}

	unlock_ptbl(fl1ptbl, fl1mtx);
}

/*
 * This routine puts a L3 ptbl (rl3ptbl) onto the fake tree.
 */
static void
tmpunload_l3(struct as *as, caddr_t addr,
	struct ptp rl2ptp, ptbl_t *rl3ptbl)
{
	struct ptp *fl2ptp;
	struct ptp tmp;
	ptbl_t *fl2ptbl;
	kmutex_t *rl3mtx, *fl2mtx;
	u_char		flags;

	/*
	 * The lock must not fail since the owning l2 is locked.
	 */
	(void) lock_ptbl(rl3ptbl, 0, as, addr, 3, &rl3mtx);

	/*
	 * Find the L2 entry in the fake tree.
	 * Allocate L2 ptbl if needed.
	 */
	fl2ptp = (struct ptp *)srmmu_ptealloc(as, addr, 2, &fl2ptbl,
		&fl2mtx, SR_TMP_TREE);

	mmu_readptp(fl2ptp, &tmp);
	switch (tmp.EntryType) {
	case MMU_ET_INVALID:
		rl3ptbl->ptbl_parent = fl2ptbl;
		flags = rl3ptbl->ptbl_flags;
		flags |= PTBL_TMP;
		flags &= ~PTBL_LOCKED;
		rl3ptbl->ptbl_flags = flags;
		mutex_exit(rl3mtx);

		SET_TNEW_PTP(fl2ptp, *(u_int *) &rl2ptp, MMU_L2_BASE(addr),
		    2, 0);

		fl2ptbl->ptbl_validcnt++;
		unlock_ptbl(fl2ptbl, fl2mtx);

		SRMMU_LOG(as, addr, *(u_int *)&rl2ptp, fl2ptbl,
		    flags, rl3ptbl);

		break;

	default:
		cmn_err(CE_PANIC, "tmpunload_l3: l2pte %x",
		    *(u_int *)&tmp);
	}
	/*NOTREACHED*/
}

#endif defined(SRMMU_TMPUNLOAD)

/*
 * Unload all the mappings in the range [addr..addr+len).
 * addr and len must be MMU_PAGESIZE aligned.
 */
void
srmmu_unload(as, addr, len, flags)
	struct	as *as;
	caddr_t	addr;
	u_int	len;
	int	flags;
{
	srmmu_t		*srmmu;

	ASSERT((len & MMU_PAGEOFFSET) == 0);

	if (flags & HAT_UNLOAD_UNMAP)
		flags = (flags & ~HAT_UNLOAD_UNMAP) | HAT_UNLOAD;

	if (flags & HAT_UNLOAD_OTHER) {
		mutex_enter(&as->a_hat->hat_unload_other);
	}
	srmmu = astosrmmu(as);
	if (srmmu == NULL) {
		/*
		 * No srmmu struc means we've through srmmu_free().
		 * This happens in as_free(). It does a hat_free()
		 * first, then goes through segments to unmap them,
		 * which ends up here. XXXX why????
		 */
		ASSERT(as != &kas);
		if (flags & HAT_UNLOAD_OTHER) {
			mutex_exit(&as->a_hat->hat_unload_other);
		}
		return;
	}


#if defined(SRMMU_TMPUNLOAD)
	ASSERT((flags & SR_FLAGS & ~SR_TMPUNLOAD) == 0);

	/*
	 * If tmpunloading and there is no tmp l1 table for this hat
	 * then allocate one.  Only consider tmpunloading for 256k or
	 * more.
	 */
	if ((flags & SR_TMPUNLOAD) && (len >= (256 * 1024))) {
		struct hat	*hat;

		hat = as->a_hat;
		mutex_enter(&hat->hat_mutex);
		if (srmmu->s_tmpl1ptbl == NULL) {
			ptbl_t	*ptbl;

			ptbl = srmmu_l1_ptbl_alloc();
			fill_l1_group(ptbl, as, PTBL_TMP);
			srmmu->s_tmpl1ptbl = ptbl;
		}
		mutex_exit(&hat->hat_mutex);
	}
	SRMMU_LOG(as, addr, len, flags, srmmu, srmmu->s_tmpl1ptbl);
#else
	ASSERT((flags & SR_FLAGS) == 0);
	SRMMU_LOG(as, addr, len, flags, srmmu, 0);
#endif /* SRMMU_TMPUNLOAD */

	srmmu_unload_tree(as, addr, len, flags, srmmu);

#if defined(SRMMU_TMPUNLOAD)
	/*
	 * Make sure we also unload the fake tree when not doing
	 * TMPUNLOAD.
	 */
	if (!(flags & SR_TMPUNLOAD) && (srmmu->s_tmpl1ptbl != NULL)) {
		srmmu_unload_tree(as, addr, len,
			SR_NOFLUSH | SR_TMP_TREE | flags,
			srmmu);
	}
#endif /* SRMMU_TMPUNLOAD */

	if (flags & HAT_UNLOAD_OTHER) {
		mutex_exit(&as->a_hat->hat_unload_other);
	}
}

static void
srmmu_unload_tree(as, addr, len, flags, srmmu)
	struct	as *as;
	caddr_t	addr;
	u_int	len;
	int	flags;
	srmmu_t	*srmmu;
{
	register u_int span = 0;
	ptbl_t *l1ptbl, *l2ptbl, *l3ptbl, *l1base;
	kmutex_t *l1mtx, *l2mtx, *l3mtx;
	struct hat *hat = as->a_hat;
	int ret;
	union ptpe *l1pte, *l2pte, *l3pte;
	union ptpe tl1pte, tl2pte;
	u_int savel2ptp;
	int lkflag;
	struct  l1pt *root;
	page_t	*rpp[NL3PTEPERPT];
	int vcnt;

	ASSERT((flags & (SR_TMPUNLOAD | SR_TMP_TREE)) !=
		(SR_TMPUNLOAD | SR_TMP_TREE));

	if (!(flags & SR_TMP_TREE)) {
		lkflag =  0;
		root = (struct l1pt *)ptbltopt_va(srmmu->s_l1ptbl);
		l1base = srmmu->s_l1ptbl;
	} else {
#ifdef SRMMU_TMPUNLOAD
		lkflag =  LK_PTBL_TMP;
		root = (struct l1pt *)ptbltopt_va(srmmu->s_tmpl1ptbl);
		l1base = srmmu->s_tmpl1ptbl;
#else /* SRMMU_TMPUNLOAD */
		cmn_err(CE_PANIC, "no tmp unload");
#endif /* SRMMU_TMPUNLOAD */
	}

retryl1:

	l1pte = (union ptpe *)root + MMU_L1_INDEX(addr);
	l2pte = NULL;

	for (; len; len -= span) {

		mmu_readpte((struct pte *)l1pte, (struct pte *)&tl1pte);
		if (MMU_L1_OFF(addr) == 0 && len >= MMU_L1_SIZE) {
			l1ptbl = VA2L1PTBL(l1base, addr);
			(void) lock_ptbl(l1ptbl, lkflag, as, addr, 1, &l1mtx);

			switch (tl1pte.ptp.EntryType) {
			case MMU_ET_PTP:
				/* Invalid the whole L1 range. */
				MOD_VALID_PTP(flags, &l1pte->ptp,
					    MMU_ET_INVALID, addr, 1, hat, 0);

				l2ptbl = tatoptbl(tl1pte.ptp.PageTablePointer);

#ifdef SRMMU_TMPUNLOAD
				if (flags & SR_TMPUNLOAD) {
					tmpunload_l2(as, addr, tl1pte.ptp,
					    l2ptbl);
					break;
				}
#endif SRMMU_TMPUNLOAD

				ret = srmmu_l2inval(as, addr,
				    flags | SR_NOFLUSH, l2ptbl, &l2mtx);

				if (ret == LK_PTBL_OK) {
					if (!(l2ptbl->ptbl_flags & PTBL_KEEP)) {
						srmmu_ptbl_free(l2ptbl, l2mtx);
					} else {
						/* restore L1 ptp ptr */
						SET_NEW_PTP(&(l1pte->ptp),
						    tl1pte.ptpe_int, addr,
						    1, 0);
						unlock_ptbl(l2ptbl, l2mtx);
					}
				}
				break;

			case MMU_ET_PTE:
				(void) srmmu_pteunload(l1ptbl, &l1pte->pte,
				    flags, NO_MLIST_LOCK);
				break;

			case MMU_ET_INVALID:
				break;
			}
			unlock_ptbl(l1ptbl, l1mtx);

			span = MMU_L1_SIZE;
			goto update_ptr;
		}

		/*
		 * We are unloading less than a L1 page (or a L2 ptbl) worth
		 * of vaddr. This means it should be a L2 page or smaller.
		 * So we need find the L2 entry.
		 */
		if (tl1pte.ptp.EntryType != MMU_ET_PTP) {
			l1ptbl = VA2L1PTBL(l1base, addr);
			(void) lock_ptbl(l1ptbl, lkflag, as, addr, 1, &l1mtx);

			/* read the entry again under lock. */
			mmu_readpte((struct pte *)l1pte, (struct pte *)&tl1pte);
			switch (tl1pte.ptp.EntryType) {
			case MMU_ET_PTP:
				unlock_ptbl(l1ptbl, l1mtx);
				break;

			case MMU_ET_PTE:
				cmn_err(CE_PANIC, "unload part of a L1 page");
				/*NOTREACHED*/

			case MMU_ET_INVALID:
				/* Nothing to do. */
				unlock_ptbl(l1ptbl, l1mtx);

				span = MMU_L1_SIZE;
				span -= ((u_int)addr & (span - 1));
				span = MIN(len, span);
				goto update_ptr;
			}
		}

		if (l2pte == NULL) {
			tatoptbl_pte(tl1pte.ptp.PageTablePointer,
			    &l2ptbl, &l2pte);
			l2pte += MMU_L2_INDEX(addr);
		}
		mmu_readpte((struct pte *)l2pte, (struct pte *)&tl2pte);
		if (MMU_L2_OFF(addr) == 0 && len >= MMU_L2_SIZE) {
			(void) lock_ptbl(l2ptbl, lkflag, as, addr, 2, &l2mtx);

			/* Read again with Lock on. */
			mmu_readpte((struct pte *)l2pte, (struct pte *)&tl2pte);
			switch (tl2pte.ptp.EntryType) {
				case MMU_ET_PTP:
					/* invalid the whole 256K range. */
					MOD_VALID_PTP(flags, &l2pte->ptp,
						    MMU_ET_INVALID, addr, 2,
						    hat, 0);

					l3ptbl = tatoptbl(
					    tl2pte.ptp.PageTablePointer);

#ifdef SRMMU_TMPUNLOAD
					if (flags & SR_TMPUNLOAD) {
						tmpunload_l3(as, addr,
						    tl2pte.ptp, l3ptbl);

						ASSERT(PTBL_VALIDCNT(
						    l2ptbl->ptbl_validcnt));
						l2ptbl->ptbl_validcnt--;
						break;
					}
#endif SRMMU_TMPUNLOAD

					ret = srmmu_l3inval(as, addr,
						flags | SR_NOFLUSH, l3ptbl,
						&l3mtx, rpp);

					if (ret == LK_PTBL_OK) {
						if (FREE_UP_PTBL(
						    l3ptbl->ptbl_flags)) {
							ASSERT(PTBL_VALIDCNT(
							l2ptbl->ptbl_validcnt));
							l2ptbl->ptbl_validcnt--;
							srmmu_ptbl_free(l3ptbl,
							    l3mtx);
						} else {
							/* restore L2 ptp ptr */
							SET_NEW_PTP(&l2pte->ptp,
							    tl2pte.ptpe_int,
							    addr, 2, 0);
							unlock_ptbl(l3ptbl,
							    l3mtx);
						}
					}

#ifdef VAC
					if (cache & CACHE_VAC) {
						srmmu_unload_vacpgs(rpp);
					}
#endif VAC
					break;

				case MMU_ET_PTE:
					(void) srmmu_pteunload(l2ptbl,
					    &l2pte->pte, flags, NO_MLIST_LOCK);
					break;

				case MMU_ET_INVALID:
					break;
			}
			unlock_ptbl(l2ptbl, l2mtx);

			span = MMU_L2_SIZE;
			goto update_ptr;
		}

		if (tl2pte.ptp.EntryType != MMU_ET_PTP) {
			(void) lock_ptbl(l2ptbl, lkflag, as, addr, 2, &l2mtx);

			/* read the entry again under lock. */
			mmu_readpte((struct pte *)l2pte, (struct pte *)&tl2pte);
			switch (tl2pte.ptp.EntryType) {
			case MMU_ET_PTP:
				unlock_ptbl(l2ptbl, l2mtx);
				break;

			case MMU_ET_PTE:
				cmn_err(CE_PANIC,
				    "unload part of a L2 page");
				/*NOTREACHED*/

			case MMU_ET_INVALID:
				unlock_ptbl(l2ptbl, l2mtx);

				span = MMU_L2_SIZE;
				span -= ((u_int)addr & (span - 1));
				span = MIN(len, span);
				goto update_ptr;
			}
		}

		tatoptbl_pte(tl2pte.ptp.PageTablePointer, &l3ptbl, &l3pte);
		l3pte += MMU_L3_INDEX(addr);

		span = MIN(len, L3PTSIZE - ((u_int)addr & L3PTOFFSET));

		/*
		 * Don't want to cause page fault for kas as it may
		 * upset the kernel until we make kernel deal with
		 * minor/hat faults.
		 */
		if (((mmu_btop(span) >= l3ptbl->ptbl_validcnt) ||
		    (span > N_L2_FLUSH_TRIGGER)) && (as != &kas)) {
			(void) lock_ptbl(l2ptbl, lkflag, as, addr, 2, &l2mtx);

			/* Read ptp again under the lock. */
			mmu_readpte((pte_t *)l2pte, (pte_t *)&tl2pte);
			savel2ptp = tl2pte.ptpe_int;
			if (tl2pte.ptpe_int != MMU_ET_INVALID) {
				/* invalid the whole 256K range. */
				MOD_VALID_PTP(flags, &l2pte->ptp,
					    MMU_ET_INVALID, MMU_L2_BASE(addr),
					    2, hat, 0);
			} else {
				/*
				 * The L3 is now gone!! Someone had stolen
				 * it.
				 */
				unlock_ptbl(l2ptbl, l2mtx);
				goto update_ptr;
			}
		} else {
			savel2ptp = 0;
		}

		if ((ret = lock_ptbl(l3ptbl, lkflag | LK_PTBL_FAILOK,
		    as, addr, 3, &l3mtx)) != LK_PTBL_OK) {
			if (ret == LK_PTBL_FAIL_SHARED) {
				cmn_err(CE_PANIC, "unmap shared l3");
			}

			if (savel2ptp) {
				/* put back the L2 entry, no need to flush. */
				SET_NEW_PTP(&(l2pte->ptp), savel2ptp,
				    MMU_L2_BASE(addr), 2, 0);
				unlock_ptbl(l2ptbl, l2mtx);
			}
			goto retryl1;
		}

		vcnt = srmmu_unload_loop(l3ptbl,
		    (&l3pte->pte) - ptbltopt_va(l3ptbl),
		    mmu_btop(span), (savel2ptp)? flags | SR_NOFLUSH : flags,
		    rpp);

		if (savel2ptp) {
			if (l3ptbl->ptbl_validcnt != 0) {
				/* put back the L2 entry, no need to flush. */
				SET_NEW_PTP(&(l2pte->ptp), savel2ptp,
				    MMU_L2_BASE(addr), 2, 0);
				unlock_ptbl(l3ptbl, l3mtx);
				unlock_ptbl(l2ptbl, l2mtx);
			} else {
				l2ptbl->ptbl_validcnt--;
				unlock_ptbl(l2ptbl, l2mtx);
				srmmu_ptbl_free(l3ptbl, l3mtx);
			}
		} else {
			unlock_ptbl(l3ptbl, l3mtx);
		}

#ifdef VAC
		if (vcnt && (cache & CACHE_VAC)) {
			srmmu_unload_vacpgs(rpp);
		}
#else	/* VAC */
#if defined(lint)
		vcnt = vcnt + 1;
#endif /* lint */
#endif /* VAC */

update_ptr:
		addr += span;
		if (MMU_L1_OFF(addr) == 0) {
			l1pte++;
			l2pte = NULL;
		} else {
			ASSERT(MMU_L2_OFF(addr) == 0 || span == len);
			l2pte++;
		}
	}
}

/*
 * A routine to confirm that the relationship between a page
 * and its ptbl, pte and hme has not changed.
 * This is used by the mapping list walking routines after they
 * have dropped the mlist lock.  Once the lock is dropped, the
 * page's state may have changed dramatically.  If it has changed
 * the best thing to do is start over.
 */
static int
confirm_page_mapping(page_t *pp, srhment_t *hme, ptbl_t *ptbl, pte_t **pte)
{
	if (hme == NULL) {
		if (((u_int)mach_pp->p_mapping & PP_PTBL) == 0) {
			/*
			 * not a single mapping anymore.
			 */
			return (HAT_RESTART);
		}
		if (ptbl != PP2PTBL(mach_pp)) {
			/*
			 * a single mapping, but not with this ptbl
			 */
			return (HAT_RESTART);
		}
		*pte = ptbltopt_va(ptbl) + mach_pp->p_index;
	} else {
		if ((u_int)mach_pp->p_mapping & PP_PTBL) {
			/*
			 * if this page is now singly-mapped,
			 * the old hme had better not be pointing
			 * at it.
			 */
			ASSERT(hme->ghme.hme_page != pp);
			return (HAT_RESTART);
		}
		if (hme->ghme.hme_page != pp) {
			/*
			 * This hme does not belong to this page
			 * anymore.  Which means that the mlist lock
			 * we are holding does not protect it.  Hence
			 * we have no idea if this ptbl has anything
			 * to do with us.  Start over.
			 */
			return (HAT_RESTART);
		}
		if (ptbl != hme->hme_ptbl) {
			/*
			 * Here, the hme must have been put on the
			 * free list, taken off, re-assigned to
			 * this page, but is associated with a
			 * different ptbl.
			 * Again, drop the ptbl lock, and start over.
			 */
			return (HAT_RESTART);
		}

		/*
		 * we are on the list, but might not be in the
		 * same position.  This does not really matter,
		 * since the hardware is busy setting all the
		 * R & M bits behind us anyway.  We are just
		 * trying to do the best job we can.
		 */
		*pte = ptbltopt_va(ptbl) + hme->hme_index;
	}

	return (0);
}

static void
srmmu_sys_pageunload(pp, vf)
	struct page *pp;
	struct hment *vf;
{
	register struct hat *hat;
	register u_int p_mapping;
	extern struct hatops srmmu_hatops;
	kmutex_t *pml;
	struct hment *hme;
	u_int vacflag = (u_int) vf;
	int ret;

	ASSERT((vacflag == VAC_UNLOAD) || PAGE_LOCKED(pp));
	ASSERT((vacflag == 0) ||
	    ((vacflag == VAC_UNLOAD) && (cache & CACHE_VAC)));

	SRMMU_LOG(pp, vf, 0, 0, 0, 0);

	pml = srmmu_mlist_enter(pp);

	while ((p_mapping = (u_int)mach_pp->p_mapping) != NULL) {
		if (p_mapping & PP_PTBL) {
			ret = srmmu_pageunload(pp, NULL, vacflag);
		} else {
			hme = (struct hment *)p_mapping;
			ASSERT(hme->hme_page == pp);
			hat = &hats[hme->hme_hat];
			if (hat->hat_op == &srmmu_hatops) {
				ret = srmmu_pageunload(pp, (srhment_t *)hme,
				    vacflag);
			} else {
				HATOP_PAGEUNLOAD(hat, pp, hme);
				ret = HAT_DONE;
			}
		}

		if (ret == HAT_VAC_DONE) {
			break;
		}
	}

	srmmu_mlist_exit(pml);
}

/*
 * Unload all the hardware translations that map page `pp'.
 * In order to deal with a potential deadlock where we hold
 * the mapping list lock and are waiting for the page table
 * lock while someone else (srmmu_unload)
 * has the page table lock and is waiting for the mapping
 * list lock, we first drop the mapping list lock before
 * waiting for the page table lock.  Once we finish waiting
 * for the page table lock, we drop the per-hat lock,
 * re-acquire the mapping list lock, and then reacquire the
 * per-hat lock in order to preserve lock ordering.
 */
int
srmmu_pageunload(pp, hme, vacflag)
	struct page *pp;
	struct srhment *hme;
	int vacflag;
{
	struct pte	*pte;
	ptbl_t		*ptbl;
	kmutex_t	*mtx;
	int		flags;

	ASSERT(ohat_mlist_held(pp));
	ASSERT(pp != NULL);

	if (hme == NULL) {
		ASSERT(((u_int)mach_pp->p_mapping) & PP_PTBL);
		ptbl = PP2PTBL(mach_pp);
		pte = ptbltopt_va(ptbl) + mach_pp->p_index;
	} else {
		ASSERT(hme->ghme.hme_page == pp);
		ptbl = hme->hme_ptbl;
		pte = ptbltopt_va(ptbl) + hme->hme_index;
	}

	if (lock_this_ptbl(ptbl, LK_PTBL_NOWAIT, &mtx) == LK_PTBL_FAILED) {
		ohat_mlist_exit(pp);
		if (lock_this_ptbl(ptbl, 0, &mtx) != LK_PTBL_OK) {
			ohat_mlist_enter(pp);
			return (HAT_RESTART);
		}

		ohat_mlist_enter(pp);

		if (confirm_page_mapping(pp, hme, ptbl, &pte) == HAT_RESTART) {
			unlock_ptbl(ptbl, mtx);
			return (HAT_RESTART);
		}
	}

	if (PTBL_LEVEL(ptbl->ptbl_flags) != PTBL_LEVEL_3) {
		cmn_err(CE_PANIC, "srmmu_pageunload - bigpte");
	}

	/*
	 * Vacflag is set when we are unloading all mapppings after
	 * a pte has been unloaded and there is no vac conflict
	 * anymore, and there is no locked mapping. But to avoid
	 * holding piles of locks, by the time we got here, there
	 * might be new locked mapping was added to the page, don't
	 * unload them.
	 */
	if ((vacflag != VAC_UNLOAD) || (ptbl->ptbl_lockcnt == 0)) {
		if (!(ptbl->ptbl_flags & PTBL_TMP)) {
			flags = SR_UNLOAD_ALL;
		} else {
			flags = SR_TMP_TREE | SR_NOFLUSH | SR_UNLOAD_ALL;
		}

		(void) srmmu_pteunload(ptbl, pte, flags, MLIST_LOCKED);

		/*
		 * We may consider trying to return the L3 ptbl, if its
		 * validcnt is zero. But for now, let exit, unload or
		 * ptbl stealer find it.
		 */
		unlock_ptbl(ptbl, mtx);

		return (HAT_DONE);
	} else {
		ASSERT((cache & CACHE_VAC) != 0);

		/*
		 * Cannot unload the page.
		 */

		unlock_ptbl(ptbl, mtx);
		return (HAT_VAC_DONE);
	}

}


/* ARGSUSED */
static int
srmmu_sys_pagesync(notused, pp, hme, clearflag)
	struct hat *notused;
	struct page *pp;
	struct hment *hme;
	u_int clearflag;
{
	u_int		ret;
	u_int		p_mapping;
	kmutex_t	*pml;
	struct hat	*hat;
	extern	u_long	po_share;

	ASSERT(hme == NULL);

	if (PP_ISRO(pp) && (clearflag & HAT_SYNC_STOPON_MOD)) {
		return (HAT_DONE);
	}

	if ((clearflag == (HAT_SYNC_STOPON_REF | HAT_SYNC_DONTZERO)) &&
	    PP_ISREF(pp)) {
		return (HAT_DONE);
	}

	if ((clearflag == (HAT_SYNC_STOPON_MOD | HAT_SYNC_DONTZERO)) &&
	    PP_ISMOD(pp)) {
		return (HAT_DONE);
	}

	if (mach_pp->p_share > po_share &&
	    !(clearflag & HAT_SYNC_ZERORM)) {
		if (PP_ISRO(pp)) {
			srmmu_page_enter(pp);
			PP_SETREF(pp);
			srmmu_page_exit(pp);
		}
		return (HAT_DONE);
	}

	pml = srmmu_mlist_enter(pp);

again:
	p_mapping = (u_int)mach_pp->p_mapping;

	SRMMU_LOG(pp, clearflag, p_mapping, 0, 0, 0);

	while (p_mapping != NULL) {
		if (p_mapping & PP_PTBL) {
			ptbl_t		*ptbl;

			ptbl = MAP2PTBL(p_mapping);
			hme = NULL;
			hat = ptbl->ptbl_as->a_hat;
		} else {
			hme = (struct hment *)p_mapping;
			ASSERT(hme->hme_page == pp);
			hat = &hats[hme->hme_hat];
		}

		if (hat->hat_op == &srmmu_hatops) {
			ret = srmmu_pagesync(hat, pp, (struct srhment *)hme,
				clearflag & ~HAT_SYNC_STOPON_RM);
		} else {
			srhment_t  t_hme;

			if (hme == NULL) {
				ASSERT(p_mapping & PP_PTBL);
				t_hme = nullhme;
				t_hme.ghme.hme_page = pp;
				t_hme.ghme.hme_hat  = hat - hats;
				t_hme.hme_ptbl = MAP2PTBL(p_mapping);

				hme = (struct hment *)&t_hme;
			}

			ret = HATOP_PAGESYNC(hat, pp, hme,
				clearflag & ~HAT_SYNC_STOPON_RM);
		}
		ASSERT(ohat_mlist_held(pp));
		if (ret != HAT_RESTART) {
			/*
			 * If clearflag is HAT_SYNC_DONTZERO, break out as soon
			 * as the "ref" or "mod" is set.
			 */
			if ((clearflag & ~HAT_SYNC_STOPON_RM) ==
			    HAT_SYNC_DONTZERO &&
			    ((clearflag & HAT_SYNC_STOPON_MOD) &&
				PP_ISMOD(pp)) ||
			    ((clearflag & HAT_SYNC_STOPON_REF) &&
				PP_ISREF(pp))) {
					break;
			}
			if (hme) {
				/*
				 * Move to the next one now.  If the
				 * service routine dropped the lock, the
				 * earlier version of hme_next might not
				 * be valid anymore.  Since we hold the
				 * lock now, both hme and hme_next must
				 * be on the list.
				 */
				p_mapping = (u_int) hme->hme_next;
			} else {
				/*
				 * There was only one mapping.
				 */
				break;
			}
		} else {
			goto again;
		}
	}
	srmmu_mlist_exit(pml);

	return (HAT_DONE);
}

/*
 * Get all the hardware dependent attributes from a given mapping
 */
/* ARGSUSED */
int
srmmu_pagesync(hat, pp, hme, clearflag)
	struct hat *hat;
	struct page *pp;
	struct srhment *hme;
	u_int clearflag;
{
	struct pte	*pte;
	ptbl_t		*ptbl;
	kmutex_t	*mtx;
	u_int		apte;

	ASSERT(ohat_mlist_held(pp));
	ASSERT(PAGE_LOCKED(pp));

	if (hme == NULL) {
		ASSERT((u_int)mach_pp->p_mapping & PP_PTBL);
		if ((u_int)mach_pp->p_mapping & PP_NOSYNC) {
			return (HAT_DONE);
		}
		ptbl = PP2PTBL(mach_pp);
		pte = ptbltopt_va(ptbl) + mach_pp->p_index;
	} else {
		ASSERT(hme->ghme.hme_page == pp);
		if (hme->ghme.hme_nosync) {
			return (HAT_DONE);
		}
		ptbl = hme->hme_ptbl;
		pte = ptbltopt_va(ptbl) + hme->hme_index;
	}

	/*
	 * If we don't have to clear any bits,
	 * then just peak, and set the page_t copy.
	 */
	if (!clearflag) {
		mmu_readpte(pte, (struct pte *)&apte);
		apte &= PTE_RM_MASK;
		if (apte) {
			srmmu_set_rm(pp, apte);
		}
		return (HAT_DONE);
	}

	if (lock_this_ptbl(ptbl, LK_PTBL_NOWAIT, &mtx) == LK_PTBL_FAILED) {
		ohat_mlist_exit(pp);
		if (lock_this_ptbl(ptbl, 0, &mtx) != LK_PTBL_OK) {
			ohat_mlist_enter(pp);
			return (HAT_RESTART);
		}
		ohat_mlist_enter(pp);

		if (confirm_page_mapping(pp, hme, ptbl, &pte) == HAT_RESTART) {
			unlock_ptbl(ptbl, mtx);
			return (HAT_RESTART);
		}
		/*
		 * Make sure we are still supposed to sync.
		 */
		if (hme == NULL) {
			if ((u_int)mach_pp->p_mapping & PP_NOSYNC) {
				unlock_ptbl(ptbl, mtx);
				return (HAT_DONE);
			}
		} else {
			if (hme->ghme.hme_nosync) {
				unlock_ptbl(ptbl, mtx);
				return (HAT_DONE);
			}
		}
	}

	if (PTBL_LEVEL(ptbl->ptbl_flags) != PTBL_LEVEL_3) {
		/*
		 * Panic for now. syncing in the middle of a big page
		 * should not be fatal.
		 */
		cmn_err(CE_PANIC, "sync in a big page");
	}

	(void) srmmu_ptesync(ptbl, pte, clearflag ? SR_RMSYNC : SR_RMSTAT,
		MLIST_LOCKED);

	unlock_ptbl(ptbl, mtx);

	return (HAT_DONE);
}

static u_int
srmmu_getattr(struct as *as, caddr_t addr, u_int *attr)
{
	struct pte *pte, tpte;
	int level;
	ptbl_t *ptbl;
	kmutex_t *mtx, *pml;
	page_t *pp;
	int h_nosync = 0;
	int prot;
	u_int p_mapping;
	struct srhment *hme;

	pte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx, LK_PTBL_SHARED);
	mmu_readpte(pte, &tpte);

	if (tpte.EntryType == MMU_ET_PTE) {
		pp = pfn2pp_hash(tpte.PhysicalPageNumber);
		if (pp) {
			pml = srmmu_mlist_enter(pp);
			p_mapping = (u_int) mach_pp->p_mapping;
			if (p_mapping & PP_PTBL) {
				if ((ptbltopt_va(MAP2PTBL(p_mapping))
				    + mach_pp->p_index) == pte) {
					h_nosync = p_mapping & PP_NOSYNC;
				}
			} else {
				hme = pte2hme(ptbl, pte);
				ASSERT(hme == NULL || hme->ghme.hme_page == pp);
				if (hme) {
					h_nosync = hme->ghme.hme_nosync;
				}
			}
			srmmu_mlist_exit(pml);
		}
		unlock_ptbl(ptbl, mtx);

		prot = srmmu_ptov_prot(&tpte);
		*attr = (h_nosync == 0)? prot : prot | HAT_NOSYNC;

		return (0);
	} else {
		unlock_ptbl(ptbl, mtx);
		*attr = 0;

		return ((u_int)0xffffffff);
	}
}

/*
 * Returns a page frame number for a given user virtual address.
 */
u_int
srmmu_getpfnum(as, addr)
	struct as *as;
	caddr_t	addr;
{
	struct pte *pte, tpte;
	int level;
	ptbl_t *ptbl;
	kmutex_t *mtx;
	u_int pfn;

	pte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx, LK_PTBL_SHARED);
	mmu_readpte(pte, &tpte);
	unlock_ptbl(ptbl, mtx);

	if (tpte.EntryType == MMU_ET_PTE) {
		pfn = tpte.PhysicalPageNumber;

		switch (level) {
		case 3:
			return (pfn);
		case 2:
			return (pfn +
			    (((u_int)addr & 0x3FFFF) >> MMU_PAGESHIFT));
		case 1:
			return (pfn +
			    (((u_int)addr & 0xFFFFFF) >> MMU_PAGESHIFT));
		}
	}

	return ((u_int)-1);
}

/*ARGSUSED*/
static void
srmmu_sys_pagecachectl(pp, flag)
	struct page *pp;
	u_int flag;
{
	u_int p_mapping;
	register struct hatsw *hsw;
	kmutex_t *pml;

	pml = srmmu_mlist_enter(pp);

	srmmu_page_enter(pp);
	if (flag & HAT_TMPNC)
		PP_SETTNC(pp);
	else if (flag & HAT_UNCACHE)
		PP_SETPNC(pp);
	else {
		PP_CLRPNC(pp);
		PP_CLRTNC(pp);
	}
	srmmu_page_exit(pp);

	p_mapping = (u_int) mach_pp->p_mapping;

	SRMMU_LOG(pp, flag, p_mapping, 0, 0, 0);

	if (p_mapping & PP_PTBL) {
		srmmu_pagecachectl(pp, flag);
	} else {
		for (hsw = hattab; hsw->hsw_name && hsw->hsw_ops; hsw++) {
			if (hsw->hsw_ops == &srmmu_hatops) {
				srmmu_pagecachectl(pp, flag);
			} else {
				HATOP_PAGECACHECTL(hsw, pp, flag);
			}
		}
	}

	srmmu_mlist_exit(pml);
}

/*ARGSUSED*/
static void
srmmu_pagecachectl(pp, flag)
	struct page *pp;
	u_int flag;
{
#if sun4d
	cmn_err(CE_PANIC, "srmmu_pagecachectl");
#elif sun4m
	u_int		p_mapping;
	struct pte	*pte;
	ptbl_t *ptbl;
	extern void pac_pageflush(u_int);

	ASSERT(PAGE_LOCKED(pp));
	ASSERT(ohat_mlist_held(pp));

	/*
	 * Check every hme on this page's mapping list that belongs to the
	 * SRMMU HAT to enforce cache consistency
	 */
	CAPTURE_CPUS;
	p_mapping = (u_int) mach_pp->p_mapping;
	while (p_mapping != 0) {
		if (p_mapping & PP_PTBL) {
			ptbl = MAP2PTBL(p_mapping);
			pte = ptbltopt_va(ptbl) + mach_pp->p_index;
			p_mapping = 0;
		} else {
			struct srhment	*hme;
			struct hat	*hat;

			hme = (struct srhment *)p_mapping;
			ASSERT(hme->ghme.hme_page == pp);

			p_mapping = (u_int)hme->ghme.hme_next;
			hat = &hats[hme->ghme.hme_hat];
			if (hat->hat_op != &srmmu_hatops) {
				continue;
			}

			ptbl = hme->hme_ptbl;
			pte = ptbltopt_va(ptbl) + hme->hme_index;
		}

		srmmu_ptecachectl(ptbl, pte, pp);
	}

	if ((cache & CACHE_PAC) && (flag & HAT_UNCACHE)) {
		pac_pageflush(page_pptonum(pp));
	}

	RELEASE_CPUS;
#else
#error	Unknown architecture
#endif
}

/*
 * For compatability with AT&T and later optimizations
 */
/*ARGSUSED*/
static int
srmmu_map(hat, as, addr, len, flags)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
	u_int len;
	int flags;
{
	return (0);
}

/*
 * This routine is called for kernel initialization to cause a page table
 * to be allocated for the given address, locked, and linked into the
 * kernel address space.
 */
ptbl_t *
srmmu_ptblreserve(caddr_t va, u_int level, ptbl_t *parent, kmutex_t **pmtx)
{
	ptbl_t *ptbl;

	ASSERT(level == 2 || level == 3);
	ASSERT(parent->ptbl_as == &kas);
	ASSERT(PTBL_LEVEL(parent->ptbl_flags) == (level - 1));
	ASSERT(PTBL_IS_LOCKED(parent->ptbl_flags));
	ASSERT(((u_int)va & 0xffff) == 0);

	ptbl = srmmu_ptbl_alloc(KM_NOSLEEP, NO_ALLOC, level, pmtx);
	ASSERT(ptbl->ptbl_lockcnt == 0);
	ASSERT(ptbl->ptbl_validcnt == 0);

	ptbl->ptbl_as = &kas;
	ptbl->ptbl_base = VA2BASE(va);
	ptbl->ptbl_parent = parent;
	ptbl->ptbl_flags |= PTBL_KEEP;

	return (ptbl);
}

/*
 * Allocate and lock level3 (and possibly level 2) page tables
 * for the range of addresses used by ppmapin() and ppmapout().
 */
void
srmmu_reserve(struct as *as, caddr_t addr, u_int len, u_int load_flag)
{
	struct pte *pte;
	struct pte tpte;
	ptbl_t *ptbl;
	caddr_t end;
	kmutex_t *pml, *mtx;

	ASSERT(as == &kas);

	end = (caddr_t)((u_int)addr + len);
	while (addr < end) {
		pte = srmmu_ptealloc(as, addr, 3, &ptbl, &mtx, 0);

		/*
		 * When load_flag is set, load/initialize each extant
		 * mapping with a mapping list.  During booting, mappings
		 * are made to memory pages without setting up the mapping
		 * list, so we correct that here. Also, we need to bump up
		 * the lockcnt of ptbl for VAC case.  When load_flag isn't
		 * set, mappings start in a state that is normally invalid.
		 */
		if (load_flag) {
			struct page *pp;
			u_int p_mapping;

			mmu_readpte(pte, &tpte);

			if (pte_valid(&tpte)) {
				pp = page_numtopp_nolock(
					tpte.PhysicalPageNumber);

				if (pp && vrfy_is_mem(MAKE_PFNUM(&tpte))) {
					pml = srmmu_mlist_enter(pp);
					p_mapping = (u_int) mach_pp->p_mapping;
					if (((p_mapping != 0) &&
					    (p_mapping !=
						((u_int)ptbl | PP_PTBL)) &&
					    (mach_pp->p_index !=
						((u_int)pte & 0xff)>>2)) ||
					    (PP_ISFREE(pp))) {

				prom_printf("pp %x mapping %x pfnum %x \n",
				pp, mach_pp->p_mapping,
				pte->PhysicalPageNumber);

						panic("srmmu_reserve check");
					} else {
						mach_pp->p_mapping =
						    (void *)((u_int)ptbl |
						    PP_PTBL);
						mach_pp->p_index =
						    ((u_int)pte & 0xff)>>2;
						mach_pp->p_share = 1;
						if (pte_ronly(&tpte)) {
							srmmu_page_enter(pp);
							PP_SETRO(pp);
							srmmu_page_exit(pp);
						}
#ifdef VAC
						if ((cache & CACHE_VAC) &&
						    do_pg_coloring &&
						    tpte.Cacheable) {
							mach_pp->p_vcolor =
							    ADDR_2_VCOLOR(addr);
						}
#endif /* VAC */
					}
					srmmu_mlist_exit(pml);
				}
#ifdef VAC
				if (PTBL_LEVEL(ptbl->ptbl_flags)
					== PTBL_LEVEL_3 &&
					(addr >= kernelheap &&
					addr < ekernelheap)) {
					ptbl->ptbl_lockcnt++;
				}
#endif /* VAC */
			}
		}

		ASSERT(ptbl->ptbl_flags & PTBL_KEEP);
		ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == 3);

		unlock_ptbl(ptbl, mtx);

		addr += MMU_PAGESIZE;
	}
}

/*
 * Make 'as' the current MMU context.
 */
/*ARGSUSED*/
static struct as *
srmmu_setup(struct as *as, int allocflag)
{
	srmmu_install(as);

	return (NULL);
}

/*
 * Loads the pte for the given address with the given pte. Also sets it
 * up as a mapping for page pp, if there is one.  The keep count of the
 * page table is incremented if the translation is to be locked.
 */
static
srmmu_pteload(as, addr, pp, pte, level, attr, flags)
	struct	as *as;
	caddr_t	addr;
	struct	page *pp;
	struct	pte *pte;
	int	level;
	int	flags, attr;
{
	struct pte *opte = NULL, oldpte;
	ptbl_t *ptbl;
	struct srhment *myhme[2];
	int rc;
	kmutex_t *mtx;
	kmutex_t *pml;
	int	locked = 0;
#ifdef	VAC
	u_int	vcolor;
#endif
	struct pte value;

	if (mmu_l3only && level != 3) {
		cmn_err(CE_PANIC, "pteload, no big pages. %d",
			level);
	}

	/*
	 * The following VAC code assumes pp is a memory pp,
	 * not a fake device page_t.  There *should* not be
	 * any user of a device page_t yet.
	 */
#ifdef DEBUG
	ASSERT(pp == NULL || page_numtopp_nolock(mach_pp->p_pagenum) != NULL);
#endif /* DEBUG */

#ifdef sun4m
	if (pp == NULL && !(flags & HAT_LOAD_NOCONSIST)) {
		/*
		 * Make sure pp is set if it's really for page_t
		 * backed memory.
		 */
		u_int sxpfn = pte->PhysicalPageNumber;
		u_int a;

		a = sxpfn >> 20;
		if (a == 0x8 || a == 0xf) {
			pp = pfn2pp_hash(sxpfn);
		}
	}
#endif

	SRMMU_LOG(as, addr, pp, pte, level, flags);

	ASSERT((pp == NULL) || PAGE_LOCKED(pp) || panicstr);

#ifdef sun4m
	ASSERT((pp == NULL) ||
	    (mach_pp->p_pagenum == (pte->PhysicalPageNumber & 0xfffff)));
#else sun4m
	ASSERT(pp == NULL || (mach_pp->p_pagenum == pte->PhysicalPageNumber));
#endif sun4m


#if 0 /* XXX - put this back when the leo driver is fixed */
	/*
	 * On a VAC machine if the pte is cacheable there must be a pp
	 * or consistency checking won't work.
	 */
	ASSERT(!(cache & CACHE_VAC) ||
	    ((!pte->Cacheable) ||
	    (vrfy_is_mem(pte->PhysicalPageNumber) && pp != NULL)));
#endif

	ASSERT((flags & SR_TMP_TREE) == 0);


	rc = 0;

hme_retry:
	/*
	 * Allocate hmenet up front. We need to do this since
	 * alloacating hment itself may require pte to map the
	 * new hments. Don't nest them.
	 */
	if (pp) {
		u_int t_mapping;

		if (rc == 0) {
			myhme[0] = NULL;
			myhme[1] = NULL;

			t_mapping = (u_int) mach_pp->p_mapping;
			if (t_mapping != 0) {
				myhme[0] = get_hment();
				if (t_mapping & PP_PTBL) {
					myhme[1] = get_hment();
				}
			}
		} else {
			/*
			 * We guessed wrong. Just give it two hments
			 * and it should not fail agian.
			 */
			if (myhme[0] == NULL) {
				myhme[0] = get_hment();
			}

			if (myhme[1] == NULL) {
				myhme[1] = get_hment();
			}
		}
	}

vac_retry:
	/*
	 * Allocate resources for the pte.
	 */
	opte = srmmu_ptealloc(as, addr, level, &ptbl, &mtx, flags);

#ifdef sun4d
	/*
	 * mappings to memory must be cached and mappings
	 * to i/o must be non-cached.
	 */
	ASSERT(!pte_valid(pte) || (vrfy_is_mem(MAKE_PFNUM(pte)) ?
	    pte->Cacheable : !pte->Cacheable));
#endif

	ASSERT(ptbl->ptbl_as == as);
	ASSERT(PTBL_VALIDCNT(ptbl->ptbl_validcnt));
	ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == level);

	/*
	 * Make sure the transition is legal.
	 * The old pte must either be invalid or mapping the same page.
	 */
	mmu_readpte(opte, &oldpte);

	if (pp) {
		/*
		 * Grab mlist lock so UNCACHE_CONFLICT would
		 * not get in the way.
		 */
		pml = srmmu_mlist_enter(pp);
	}

	/*
	 * Clear P_RO under mlist lock since it was set
	 * inside mlist lock too. Chgprot() path does
	 * not have to grab mlist lock since it only
	 * deals with existing mappings.
	 */
	if (pp && (!pte_ronly(pte)) && PP_ISRO(pp)) {
		srmmu_page_enter(pp);
		PP_CLRRO(pp);
		srmmu_page_exit(pp);
	}

	/*
	 * Update lockcnt. But do it only once. This has to be done
	 * before setting up the mapping.
	 */
#ifndef VAC
	if ((flags & HAT_LOAD_LOCK) && (as != &kas) && (level == 3) &&
	    !locked) {
		ptbl->ptbl_lockcnt++;
		locked = 1;
	}
#else
	/* Maintain kas lockcnt for VAC */
	if ((flags & HAT_LOAD_LOCK) && (level == 3) && !locked) {
		ptbl->ptbl_lockcnt++;
		locked = 1;
	}
#endif

	if (pte_valid(&oldpte)) {
		u_int *x, *y;

		if (oldpte.PhysicalPageNumber != pte->PhysicalPageNumber) {
			if ((flags & HAT_LOAD_REMAP) == 0)
				cmn_err(CE_PANIC, "srmmu_pteload - pte remap");
		}

#ifdef VAC
		/*
		 * Set cachable bit.
		 * We are mapping the same pp to the same vaddr here.
		 * This can not cause vac alias problem. Just use
		 * current cacheable bit setting.
		 */
		pte->Cacheable = opte->Cacheable;
#endif VAC

		x = (u_int *)&oldpte;
		y = (u_int *)pte;
		if (((*x) & ~PTE_RM_MASK) != ((*y) & ~PTE_RM_MASK)) {
			/*
			 * Load the pte.
			 * Here, we are changing a valid pte.  To make
			 * sure all of the CPU's see the change use
			 * mmu_writepte().  It will take care of
			 * demapping all of the other TLB's.
			 */
			mmu_readpte(pte, &value);
			if (enable_mbit_wa &&
			    value.AccessPermissions == MMU_STD_SRWXURWX)
				value.Modified = 1;
			(void) mmu_writepte(opte, *(u_int *)&value, addr,
			    level, as->a_hat,
			    (flags & HAT_LOAD_REMAP) ? 0 : PTE_RM_MASK);
		} else {
#ifdef DEBUG
			vmhatstat.vh_mt_pteload.value.ul++;
#endif /* DEBUG */
		}

	} else if ((flags & HAT_LOAD_REMAP) != 0) {
		cmn_err(CE_PANIC, "srmmu_pteload - reload an invalid pte");
	} else {
		if (oldpte.EntryType == MMU_ET_PTP) {
			cmn_err(CE_PANIC, "srmmu_pteload - pt overlay");
		}

		if (pp) {
#ifdef VAC
			/*
			 * VAC consistency check is done inside mlist lock
			 * so no mapping on that page can change.
			 */
			if ((cache & CACHE_VAC) && (!PP_ISNC(pp)) &&
			    (mach_pp->p_mapping != NULL)) {
				if (srmmu_vac_conflict(as, addr, pp, ptbl) !=
				    HAT_DONE) {
					ASSERT(!ohat_mlist_held(pp));
					goto vac_retry;
				}
				ASSERT(ohat_mlist_held(pp));
			}
			pte->Cacheable = !PP_ISNC(pp);

			/*
			 * Now all VAC conflicts are resolved. We can
			 * set the p_vcolor accordingly.
			 */
			if ((cache & CACHE_VAC) && do_pg_coloring) {
				ASSERT((flags & HAT_LOAD_NOCONSIST)?
				    pte->Cacheable == 0 : 1);

				/*
				 * Vcolor changes only when a new
				 * cacheable mapping is added. Uncacheable
				 * mappings don't have a color.
				 */
				if (pte->Cacheable) {
					vcolor = ADDR_2_VCOLOR(addr);
					if (vcolor != mach_pp->p_vcolor) {
						vac_color_sync(
						    VCOLOR_2_ADDR(mach_pp),
						    mach_pp->p_pagenum);
						mach_pp->p_vcolor = vcolor;
					}
				}
			}
#endif

			/*
			 * About to add a new mapping to the page. Update
			 * p_mapping list now. It will also do the write
			 * to the pte itself while it has the mlist lock
			 * held due to VAC.
			 */
			rc = hme_may_add(ptbl, opte, pp, attr, as->a_hat, pte,
			    myhme);
			if (rc != 0) {
				srmmu_mlist_exit(pml);
				unlock_ptbl(ptbl, mtx);
				goto hme_retry;
			}

			if (level == 3) {
				astosrmmu(as)->s_rss++;
			} else {
				astosrmmu(as)->s_rss +=
				    mmu_btop(srmmu_sizes[level]);
			}

		} else {
			/*
			 * Load the pte.
			 *
			 * Here, we are changing an invalid pte into a valid
			 * pte.
			 * Since there is no pte in any CPU's TLB to update, we
			 * do not need to force them to invalidate their entries
			 * as in the above case.  Just stuff the pte into the
			 * page table.
			 */
			SET_NEW_PTE(opte, *(u_int *) pte, addr, level, 0);
		}

		if (level != 1) {
			ptbl->ptbl_validcnt++;
			ASSERT(PTBL_VALIDCNT(ptbl->ptbl_validcnt));
		}
	}

	if (pp) {
		srmmu_mlist_exit(pml);
	}

	unlock_ptbl(ptbl, mtx);

	if (pp) {
		/*
		 * Return any unused hments.
		 */
		if (myhme[0]) {
			put_hment(myhme[0]);
		}

		if (myhme[1]) {
			put_hment(myhme[1]);
		}
	}

	/*
	 * Make sure the context is set up for the address space.
	 */
	srmmu_install(as);

	return (1);
}

#ifdef VAC

static void
srmmu_unload_vacpgs(page_t *rpp[])
{
	int i;

	ASSERT(cache & CACHE_VAC);

	for (i = 0; i < NL3PTEPERPT; i++) {
		if (rpp[i] != NULL && PP_ISTNC(rpp[i])) {
			srmmu_sys_pageunload(rpp[i],
			    (struct hment *)VAC_UNLOAD);
		}
	}
}

/*
 * If (vac) active, then check for conflicts.
 * A conflict exists if the new and existant mappings
 * do not match in their "shm_alignment fields,
 * XXX and one of them is writable XXX. If conflicts
 * exist, the existant mappings are flushed UNLESS
 * one of them is locked.  If one of them is locked,
 * then the mappings are flushed and converted to
 * non-cacheable mappings.  This conversion is reversed
 * in srmmu_pteunload.
 *
 * In order to protect ourselves from processes that
 * use mmap on /dev/mem (like the sundiag "pmem" test),
 * treat the current process' stack and text pages
 * as if they were locked.
 */
static int
srmmu_vac_conflict(as, addr, pp, ptbl)
	struct	as *as;
	caddr_t	addr;
	struct	page *pp;
	ptbl_t *ptbl;
{
	register struct pte *opte;
	register struct srhment *ahme;
	struct hat *tmp_hat;
	int conflict = NO_CONFLICT;
	caddr_t oaddr;
	u_int p_mapping;
	ptbl_t		*optbl;

	ASSERT(ohat_mlist_held(pp));
	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));

	/*
	 * If the requested mapping is inconsistent
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
	p_mapping = (u_int) mach_pp->p_mapping;

	SRMMU_LOG(as, addr, pp, ptbl, p_mapping, 0);

	if (p_mapping & PP_PTBL) {
		optbl = MAP2PTBL(p_mapping);
		opte = ptbltopt_va(optbl) + mach_pp->p_index;
		ahme = NULL;	/* no real hme. */
	} else {
		for (ahme = (struct srhment *)p_mapping; ahme != NULL;
		    ahme = (struct srhment *)ahme->ghme.hme_next) {
			ASSERT(ahme->ghme.hme_page == pp);
			tmp_hat = &hats[ahme->ghme.hme_hat];
			if (tmp_hat->hat_op == &srmmu_hatops) {
				break;
			}
		}
		optbl = ahme->hme_ptbl;
		opte = ptbltopt_va(optbl) + ahme->hme_index;
	}
	oaddr = ptetovaddr(optbl, opte);

	/*
	 * check for a mapping that is not consistent
	 * XXX - No one sets: hme_noconsist and hme_ncpref.
	 */
	if (ahme != NULL && ahme->ghme.hme_noconsist) {
		if (ahme->ghme.hme_ncpref) {
			conflict = UNCACHE_CONFLICT;
		} else {
			conflict = UNLOAD_CONFLICT;
		}
	} else {
		/*
		 * No known inconsistent mappings,
		 * Now, check vac consistency modulus;
		 * if the first entry is aligned,
		 * then so are all others
		 */
		if (!VAC_ALIGNED(addr, oaddr)) {
			/*
			 * Ahme should be pointing to the first
			 * srmmu hment. So, we don't have to
			 * start all over.
			 */
			if (ahme == NULL) {
				/*
				 * If there is even 1 uncache conflict,
				 * we don't want to unload the mapping.
				 */
				if ((optbl->ptbl_as == as) ||
				    (optbl->ptbl_lockcnt) ||
				    KAS_NO_FAULT(optbl)) {
					srmmu_page_enter(pp);
					PP_SETTNC(pp);
					srmmu_page_exit(pp);
					conflict = UNCACHE_CONFLICT;
				} else {
					conflict = UNLOAD_CONFLICT;
				}
				goto out;
			}

			ASSERT(ahme != NULL);

			for (; ahme;
			    ahme = (struct srhment *)(ahme->ghme.hme_next)) {
				ptbl_t *tmp_ptbl;

				tmp_hat = &hats[ahme->ghme.hme_hat];
				if (tmp_hat->hat_op != &srmmu_hatops) {
					continue;
				}

				tmp_ptbl = hmetoptbl(ahme);
				/*
				 * If there is even 1 uncache conflict,
				 * we don't want to unload the mapping.
				 */
				if ((tmp_ptbl->ptbl_as == as) ||
				    (tmp_ptbl->ptbl_lockcnt) ||
				    KAS_NO_FAULT(tmp_ptbl)) {
					srmmu_page_enter(pp);
					PP_SETTNC(pp);
					srmmu_page_exit(pp);
					conflict = UNCACHE_CONFLICT;
					break;
				} else {
					conflict = UNLOAD_CONFLICT;
				}
			}
		}
	}

out:
	/*
	 * IF WE WANT TO AVOID USING CAPTURE-RELEASE, IT
	 * IS ABSOLUTELY NECESSARY TO DO AN UNLOAD.
	 * FOR MORE DETAILS, SEE THE COMMENTS ABOVE
	 * WHERE THIS PATCHEABLE VARIABLE IS DEFINED.
	 */
	if (avoid_capture_release && conflict == UNCACHE_CONFLICT) {
		PP_CLRTNC(pp);
		conflict = UNLOAD_CONFLICT;
	}

	if (conflict != NO_CONFLICT) {
		if (srmmu_page_cache(pp, conflict) != HAT_DONE) {
			kmutex_t *mtx;

			ohat_mlist_exit(pp);

			/*
			 * We hold the ptbl lock, and we are not changing
			 * its type, so just read ptbl_flags and find the lock.
			 */
			mtx = fd_ptbl_mtx(3, ptbl,
			    (ptbl->ptbl_flags & PTBL_TMP)? LK_PTBL_TMP : 0);

			unlock_ptbl(ptbl, mtx);
			srmmu_sys_pageunload(pp, (struct hment *)VAC_UNLOAD);

			return (HAT_RESTART);
		}
	}

	return (HAT_DONE);
}

/*
 * VAC cache/unload conflict code.
 */
static int
srmmu_page_cache(pp, resolution)
	struct page *pp;
	int resolution;
{
	struct srhment *hme;
	struct hat *hat;
	u_int pg;
	u_int p_mapping;
	struct pte *pte;
	ptbl_t *tptbl;
	int flags;
	kmutex_t *tmtx;

	SRMMU_LOG(pp, resolution, 0, 0, 0, 0);

	ASSERT(pp != NULL);
	ASSERT(ohat_mlist_held(pp));

	switch (resolution) {
	case UNCACHE_CONFLICT:
		vmhatstat.vh_uncache_conflict.value.ul++;

		/*
		 * While traversing the mapping list and uncaching mappings
		 * we do not want anyone to reference the ptes we are
		 * dealing with. srmmu_ptecachectl() should not try to
		 * acquire any locks as it is being called within a
		 * capture/release as that could create a deadlock.
		 */
		CAPTURE_CPUS
		p_mapping = (u_int) mach_pp->p_mapping;
		while (p_mapping != NULL) {
			if (p_mapping & PP_PTBL) {
				tptbl = MAP2PTBL(p_mapping);
				pte = ptbltopt_va(tptbl) + mach_pp->p_index;
				p_mapping = NULL;
			} else {
				hme = (struct srhment *)p_mapping;
				ASSERT(hme->ghme.hme_page == pp);
				hat = &hats[hme->ghme.hme_hat];
				p_mapping = (u_int)(hme->ghme.hme_next);
				if (hat->hat_op != &srmmu_hatops) {
					continue;
				}
				tptbl = hme->hme_ptbl;
				pte = ptbltopt_va(tptbl) + hme->hme_index;
			}

			/*
			 * Don't even think about uncaching
			 * any kernel stacks.
			 */
			pg = btop((u_int)(ptetovaddr(tptbl, pte)));
			if (pg >= stklo && pg < stkhi) {
				continue;
			}

			/*
			 * XXX - Should use the
			 * HATOP_PAGECACHECTL macro but using
			 * it causes a deadlock since the
			 * "mail box" lock is already held.
			 */
			srmmu_ptecachectl(tptbl, pte, pp);
		}
		RELEASE_CPUS
		return (HAT_DONE);

	case UNLOAD_CONFLICT:
		vmhatstat.vh_unload_conflict.value.ul++;
		/*
		 * Remember we are here while holding both the
		 * mlist lock and a ptbl lock for the pte that
		 * are about to load.
		 */
		while ((p_mapping = (u_int) mach_pp->p_mapping) != 0) {

			ASSERT(ohat_mlist_held(pp));

			if (p_mapping & PP_PTBL) {
				tptbl = MAP2PTBL(p_mapping);
				pte = ptbltopt_va(tptbl) + mach_pp->p_index;
				hme = NULL;
			} else {
				hme = (struct srhment *)p_mapping;
				ASSERT(hme->ghme.hme_page == pp);
				hat = &hats[hme->ghme.hme_hat];
				if (hat->hat_op != &srmmu_hatops) {
					/*
					 * There could not be a dead lock
					 * problem here since non srmmu hats
					 * should not use srmmu ptbl locks
					 * at all.
					 */
					HATOP_PAGEUNLOAD(hat, pp,
						(struct hment *)hme);
					continue;
				}
				tptbl = hme->hme_ptbl;
				pte = ptbltopt_va(tptbl) + hme->hme_index;
			}

			if (lock_this_ptbl(tptbl, LK_PTBL_NOWAIT, &tmtx) ==
			    LK_PTBL_FAILED) {
				/*
				 * We failed to get the lock. This could be some
				 * other thread holding that lock or could be
				 * us colliding with the ptbl lock that we
				 * already hold.
				 */
				return (HAT_RESTART);
			}

			if (!(tptbl->ptbl_flags & PTBL_TMP)) {
				flags = SR_UNLOAD_ALL;
			} else {
				flags =
				    SR_TMP_TREE | SR_NOFLUSH | SR_UNLOAD_ALL;
			}

			(void) srmmu_pteunload(tptbl, pte, flags, MLIST_LOCKED);
			unlock_ptbl(tptbl, tmtx);
		}

		ASSERT(mach_pp->p_mapping == NULL);
		return (HAT_DONE);
	}
	/*NOTREACHED*/
}
#endif	/* VAC */

/*
 * Construct a pte for a page.
 */
void
srmmu_mempte(pp, vprot, pte, addr)
	register struct page *pp;
	u_int	vprot;
	register struct pte *pte;
	caddr_t	addr;
{

	ASSERT(PAGE_LOCKED(pp));

	*(int *)pte = PTEOF(0, page_pptonum(pp),
	    srmmu_vtop_prot(addr, vprot), 1);
}

/*
 * Returns a pointer to the pte struct for the given virtual address.
 *
 * This does not mess around with PTBL_TMP flag.
 */
struct pte *
srmmu_ptefind_nolock(as, addr, level)
	struct	as *as;
	register caddr_t addr;
	int *level;
{
	srmmu_t		*srmmu;
	union ptpe	*ptp, tptp;
	ptbl_t		*ptbl;
	pt_t		*root;

	srmmu = astosrmmu(as);

	if ((MMU_L2_BASE(addr) == srmmu->s_addr) &&
	    ((ptbl = srmmu->s_l3ptbl) != NULL)) {
		if ((ptbl->ptbl_as == as) &&
		    (ptbl->ptbl_base == VA2BASE(addr)) &&
		    (PTBL_LEVEL(ptbl->ptbl_flags) == 3)) {
			struct pte	*pte;
			pte = ptbltopt_va(ptbl) + MMU_L3_INDEX(addr);
			*level = 3;
			return (pte);
		}
		srmmu->s_l3ptbl = NULL;
	}

	root = (pt_t *)ptbltopt_va(srmmu->s_l1ptbl);
	ptp = (union ptpe *)root + MMU_L1_INDEX(addr);
	mmu_readptp((struct ptp *)ptp, (struct ptp *)&tptp);

	switch (tptp.ptp.EntryType) {
	case MMU_ET_INVALID:
	case MMU_ET_PTE:
		*level = 1;
		return (&ptp->pte);

	case MMU_ET_PTP:
		break;

	default:
		cmn_err(CE_PANIC, "srmmu_ptefind_nowait");
	}

	tatoptbl_pte(tptp.ptp.PageTablePointer, &ptbl, &ptp);
	ptp += MMU_L2_INDEX(addr);
	mmu_readptp((struct ptp *)ptp, (struct ptp *)&tptp);

	switch (tptp.ptp.EntryType) {
	case MMU_ET_INVALID:
	case MMU_ET_PTE:
		*level = 2;
		return (&ptp->pte);

	case MMU_ET_PTP:
		break;

	default:
		cmn_err(CE_PANIC, "srmmu_ptefind_nowait2");
	}

	/*
	 * Return the address of the level 3 entry.
	 */
	*level = 3;
	tatoptbl_pte(tptp.ptp.PageTablePointer, &ptbl, &ptp);
	ptp += MMU_L3_INDEX(addr);
	srmmu->s_l3ptbl = ptbl;
	srmmu->s_addr = MMU_L2_BASE(addr);
	return (&ptp->pte);
}

/*
 * Returns a pointer to the pte struct for the given virtual address.
 */
struct pte *
srmmu_ptefind(as, addr, level, pptbl, pmtx, flags)
	struct	as *as;
	register caddr_t addr;
	int *level;
	ptbl_t **pptbl;
	kmutex_t **pmtx;
	int flags;
{
	srmmu_t		*srmmu;
	union ptpe	*ptp, tptp;
	ptbl_t		*ptbl;
	kmutex_t	*mtx;
	pt_t		*root;

	SRMMU_LOG(as, addr, flags, 0, 0, 0);

	srmmu = astosrmmu(as);
	ASSERT((flags & ~LK_PTBL_SHARED) == 0);
	ASSERT(((u_int) addr < KERNELBASE) || as == &kas);

	if ((MMU_L2_BASE(addr) == srmmu->s_addr) &&
	    ((ptbl = srmmu->s_l3ptbl) != NULL) &&
	    ((flags & SR_TMP_TREE) == 0)) {
		if (lock_ptbl(ptbl, LK_PTBL_FAILOK, as, addr, 3, &mtx)
		    == LK_PTBL_OK) {
			struct pte	*pte;

			pte = ptbltopt_va(ptbl) + MMU_L3_INDEX(addr);
			*pptbl = ptbl;
			*pmtx = mtx;
			*level = 3;
			return (pte);
		}
		srmmu->s_l3ptbl = NULL;
	}

retryl1:

	/* Start with L1 */
	root = (pt_t *)ptbltopt_va(srmmu->s_l1ptbl);
	ptp = (union ptpe *)root + MMU_L1_INDEX(addr);
	mmu_readptp((struct ptp *)ptp, (struct ptp *)&tptp);

	switch (tptp.ptp.EntryType) {
	case MMU_ET_INVALID:
	case MMU_ET_PTE:
		ptbl = VA2L1PTBL(srmmu->s_l1ptbl, addr);
		(void) lock_ptbl(ptbl, flags, as, addr, 1, &mtx);
		/*
		 * Verify again now we have ptbl locked.
		 */
		if (tptp.ptp.EntryType == ptp->ptp.EntryType) {
			/* return with ptbl locked. */
			*pmtx = mtx;
			*level = 1;
			*pptbl = ptbl;
			return (&ptp->pte);
		}
		unlock_ptbl(ptbl, mtx);
		goto retryl1;

	case MMU_ET_PTP:
		break;

	default:
		cmn_err(CE_PANIC, "srmmu_ptefind1");
	}

	/* Now L2 */
	tatoptbl_pte(tptp.ptp.PageTablePointer, &ptbl, &ptp);
	ptp += MMU_L2_INDEX(addr);
	mmu_readptp((struct ptp *)ptp, (struct ptp *)&tptp);

	switch (tptp.ptp.EntryType) {
	case MMU_ET_INVALID:
	case MMU_ET_PTE:
		(void) lock_ptbl(ptbl, flags, as, addr, 2, &mtx);
		/*
		 * Verify again now we have ptbl locked.
		 */
		if (tptp.pte.EntryType == ptp->pte.EntryType) {
			/* return with ptbl locked. */
			*level = 2;
			*pptbl = ptbl;
			*pmtx = mtx;
			return (&ptp->pte);
		}
		unlock_ptbl(ptbl, mtx);
		goto retryl1;

	case MMU_ET_PTP:
		break;

	default:
		cmn_err(CE_PANIC, "srmmu_ptefind2");
	}

	/*
	 * Return the address of the level 3 entry.
	 */
	tatoptbl_pte(tptp.ptp.PageTablePointer, &ptbl, &ptp);
	ptp += MMU_L3_INDEX(addr);
	switch (lock_ptbl(ptbl, flags | LK_PTBL_FAILOK, as, addr, 3, &mtx)) {
	case LK_PTBL_OK:
		/* we return to caller with ptbl locked. */
		*pmtx = mtx;
		*level = 3;
		*pptbl = ptbl;
		srmmu->s_l3ptbl = ptbl;
		srmmu->s_addr = MMU_L2_BASE(addr);
		return (&ptp->pte);

	case LK_PTBL_FAILED:
		cmn_err(CE_PANIC, "l3 lock fail");
		/*NOTREACHED*/

	case LK_PTBL_MISMATCH:
		/*
		 * We are very trusting as this
		 * could be an infinite loop if
		 * we somehow have a wrong L3 attached!!
		 *
		 * XXXX- need a "get real mode" that grabs
		 * all locks all the way from the top.
		 */
		goto retryl1;

	case LK_PTBL_FAIL_SHARED:
		cmn_err(CE_PANIC, "l3 shared");
	}
	/*NOTREACHED*/
}

/*
 * Make sure `as' has a ctx.
 * If it's the current `as', install its ctx as the current ctx.
 */
static void
srmmu_install(as)
	struct as *as;
{
	struct proc *p;
	register srmmu_t *srmmu;
	register int ctx;
	union ptpe pl1;

	/*
	 * kas is already set up: it is part of every address space,
	 * so we can stay in whatever context that is currently active.
	 */
	if (as == &kas) {
		return;
	}

	/*
	 * Set ctx if we're dealing with the current as.
	 */
	p = ttoproc(curthread);
	if (p != NULL && p->p_as != as) {
		/*
		 * If we are doing something on someone else's behalf,
		 * then we don't care if he has a ctx or not. We let
		 * himself take care of his ctx when he runs.
		 */
		return;
	}

	if (static_ctx == 0) {
		mutex_enter(&as->a_hat->hat_mutex);
	}

	srmmu = astosrmmu(as);

	ASSERT(srmmu->s_l1ptbl->ptbl_as == as);

	ctx = srmmu->s_ctx;

	/*
	 * Allocate ctx, if necessary.
	 */
	if (ctx == -1) {
		ASSERT(static_ctx == 0);

		srmmu_getctx(as);
		ctx = srmmu->s_ctx;

		pl1.ptpe_int = ptbltopt_ptp(srmmu->s_l1ptbl);
		mmu_writeptp_locked(&contexts[ctx], pl1.ptpe_int, 0, 0, ctx, 0);
	}

	ASSERT(VALID_UCTX(ctx));
	if (p != NULL) {
		mmu_setctxreg(ctx);
	}

	if (static_ctx == 0) {
		mutex_exit(&as->a_hat->hat_mutex);
	}
}

/*
 * Allocate a ctx for use by the specified address space:
 * When there is at least one free context, this algorithm will
 * always allocate a free context.  When there are no free contexts
 * this algorithm will approximately allocate one of the oldest
 * contexts, by stealing this context from a process that has had
 * it for a long time.	The algorithm does not need to keep track
 * of how old a context is.  This is an improvement over the
 * previous implementation which kept track of some rough time
 * figure, because keeping track of the time wasted extra storage
 * in each ctx structure (there are alot of these - one for each
 * context).  Since there are so many contexts (ie usually there
 * will be a free one anyway), and since this algorithm will still
 * tend to allocate old contexts when there are no free ones
 * left, this memory saving algorithm is superior.
 *
 * Another algorithm considered was one where the ctx structures
 * are kept on a free list.  Freed ctx structures would be put on the
 * head, and the next ctx structure would be obtained by searching
 * for a free one starting at the head, and putting it on the tail,
 * once it is allocated.  This approach will also tend to allocate
 * old contexts when there are no more free ones, but it requires
 * forward and backward pointers, and thus it would consume more
 * memory.
 */

static void
srmmu_getctx(as)
	struct as *as;
{
	register struct ctx *ctx;
	register srmmu_t *srmmu;
	register struct ctx *lastctx = &ctxs[nctxs-1];
	register struct ctx *firstctx = &ctxs[NUM_LOCKED_CTXS];
	register struct hat *hat = as->a_hat;

#define	NEXT_CTX(ctx)	(((ctx) >= lastctx) ? firstctx : ((ctx) + 1))

	ASSERT(MUTEX_HELD(&hat->hat_mutex));

	if (static_ctx) {
		cmn_err(CE_PANIC, "srmmu_getctx");
	}

	srmmu = hattosrmmu(hat);

	mutex_enter(&srmmu_ctx_mtx);
	/*
	 * ctxhand was init. to &ctxs[NUM_LOCKED_CTXS] in
	 * srmmu_init.  This is the first non-locked context,
	 * so it will be allocated first.
	 */
again:
	ctx = ctxhand;

	/*
	 * Starting with ctxhand, search for the next free
	 * context.  If we don't find any free contexts, just
	 * use ctxhand because the chances are it's old enough.
	 * This shouldn't happen very often because there are
	 * more than enough contexts.
	 */
	do {
		ASSERT(ctx->c_as != as);

		/*
		 * This context is free, so use it.
		 */
		if (ctx->c_as == NULL) {
			break;
		}
		ctx = NEXT_CTX(ctx);
	} while (ctx != ctxhand);

	/*
	 * Update the starting point for the next search.
	 * Note that if we have just allocated a context which
	 * is one of the oldest contexts, then the new value
	 * of ctxhand is also likely to point to an old context.
	 * NOTE: have to wrap ctxhand, too, or we walk on down
	 * memory past the right place.
	 */
	ctxhand = NEXT_CTX(ctx);

	if (ctx->c_as) {
		srmmu_t *tmp_srmmu;
		struct hat *old_hat = ctx->c_as->a_hat;

		if (mutex_tryenter(&old_hat->hat_mutex)) {
			tmp_srmmu = hattosrmmu(old_hat);

			/*
			 * First indicate that the context has been stolen
			 * so that resume does not reset this up in the
			 * context that is being stolen after we have
			 * put it in kernel context
			 */
/*
 * XXXX
 * in resume(), it does not hold hat_mutex to set the context reg.
 */

			ASSERT(ctx->c_as == old_hat->hat_as);

			ctx->c_as = NULL;
			vmhatstat.vh_ctxstealflush.value.ul++;
			if (tmp_srmmu != NULL) {
				tmp_srmmu->s_ctx = -1;
			}

			mutex_exit(&old_hat->hat_mutex);

			SRMMU_LOG(as, tmp_srmmu, ctx - ctxs, 0, 0, 0);

		} else {
			goto again;
		}

		/*
		 * It should be fairly rare that contexts need to be stolen,
		 * so the following code is not expected to be a performance
		 * problem.  However, it is needed for system integrity.
		 * Basically, since we are stealing this context, we need to
		 * tell all CPUs, (which may have threads running in this
		 * context) except the CPU running here, to switch into the
		 * kernel context.  Otherwise, they could continue running
		 * using this context, and thus operate in the context of
		 * another address space, the address space for which this
		 * context is currently being stolen.
		 */
#ifdef sun4m
		{
			extern int ncpus;

			if (ncpus > 1) {
				xc_call(KCONTEXT, 0, 0, X_CALL_MEDPRI,
					~(1 << (getprocessorid())),
					(int (*)())mmu_setctxreg);
			}
		}

		/*
		 * Now flush data (VAC) and TLBs cached for this context
		 */
		XCALL_PROLOG;
		vac_ctxflush(ctx - ctxs, FL_TLB_CACHE);
		XCALL_EPILOG;
#else
		cmn_err(CE_PANIC, "srmmu_getctx: attempted context steal");
#endif
	}

	ctx->c_as = as;
	srmmu->s_ctx = ctx - ctxs;
	ASSERT(VALID_UCTX(srmmu->s_ctx));
	mutex_exit(&srmmu_ctx_mtx);
#undef	NEXT_CTX

}

/*
 * This routine converts virtual page protections to physical ones.
 * The value returned is only ro/rw; the supervisor mode is simply
 * checked for sanity (since the actual bit is in the level 1 page table).
 */
u_int
srmmu_vtop_prot(caddr_t addr, u_int vprot)
{
	if (vprot & ~(PROT_ALL | HAT_NOSYNC)) {
		cmn_err(CE_PANIC, "srmmu_vtop_prot - bad prot %x", vprot);
	}

	vprot &= PROT_ALL;

	if (vprot & PROT_USER) {
		/*
		 * user permission
		 */
		if (addr >= (caddr_t)KERNELBASE) {
			cmn_err(CE_PANIC,
				"user addr %p vprot %x in kernel space",
				(void *)addr, vprot);
		}
	}

	switch (vprot) {
	case 0:
	case PROT_USER:
		/*
		 * XXX - No way to tell the difference between 0
		 * and MMU_STD_SRUR
		 *
		 * Since 0 might be a valid protection,
		 * the caller should not set valid bit
		 * if vprot == 0 to be sure.
		 */
		return (0);
	case PROT_READ:
	case PROT_EXEC:
	case PROT_READ | PROT_EXEC:
		return (MMU_STD_SRX);
	case PROT_WRITE:
	case PROT_WRITE | PROT_EXEC:
	case PROT_READ | PROT_WRITE:
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		return (MMU_STD_SRWX);
	case PROT_EXEC | PROT_USER:
		return (MMU_STD_SXUX);
	case PROT_READ | PROT_USER:
		return (MMU_STD_SRUR);
	case PROT_READ | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRXURX);
	case PROT_WRITE | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_USER:
		return (MMU_STD_SRWURW);
	case PROT_WRITE | PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRWXURWX);
	default:
		cmn_err(CE_PANIC, "srmmu_vtop_prot: bad vprot %x", vprot);
		/* NOTREACHED */
	}
}

u_int
srmmu_ptov_prot(pte)
	struct pte *pte;
{
	u_int pprot;

	switch (pte->AccessPermissions) {
	case MMU_STD_SRUR:
		pprot = PROT_READ | PROT_USER;
		break;
	case MMU_STD_SRWURW:
		pprot = PROT_READ | PROT_WRITE | PROT_USER;
		break;
	case MMU_STD_SRXURX:
		pprot = PROT_READ | PROT_EXEC | PROT_USER;
		break;
	case MMU_STD_SRWXURWX:
		pprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
		break;
	case MMU_STD_SXUX:
		pprot = PROT_EXEC | PROT_USER;
		break;
	case MMU_STD_SRWUR:	/* Hmmm, doesn't map nicely, demote */
		pprot = PROT_READ | PROT_USER;
		break;
	case MMU_STD_SRX:
		pprot = PROT_READ | PROT_EXEC;
		break;
	case MMU_STD_SRWX:
		pprot = PROT_READ | PROT_WRITE | PROT_EXEC;
		break;
	default:
		pprot = 0;
		break;
	}
	return (pprot);
}

#ifdef VAC
/*
 * This routine syncs the virtual address cache for all mappings to
 * a particular physical page.	This routine originated in the
 * campus software, where it was used to keep I/O consistent with
 * what is in the vac, since hardware does not guarantee such
 * consistency.	 Since such consistency will probably be achieved
 * via hardware on Galaxy, this routine is probably not needed, but
 * it doesn't hurt to leave it around, just in case, for a little
 * while.
 * XXX - check if VAC is defined.
 */
void
srmmu_vacsync(u_int pfnum)
{
	register page_t *pp;
	register struct hat *hat;
	caddr_t vaddr;
	int nctx;
	srmmu_t *srmmu;
	u_int p_mapping;
	struct pte *pte;
	ptbl_t		*ptbl;

	pp = page_numtopp_nolock(pfnum);

	ASSERT(pp == NULL || ohat_mlist_held(pp));

	/*
	 * If the cache is off, the page isn't memory, or the page is
	 * non-cacheable, then none of the page could be in the cache
	 * in the first place.
	 */
	if (!(cache & CACHE_VAC) || (pp == NULL) || PP_ISNC(pp)) {
		return;
	}


	p_mapping = (u_int) mach_pp->p_mapping;
	while (p_mapping != 0) {
		if (p_mapping & PP_PTBL) {
			ptbl = MAP2PTBL(p_mapping);
			pte = ptbltopt_va(ptbl) + mach_pp->p_index;
			hat = ptbl->ptbl_as->a_hat;
			p_mapping = 0;
		} else {
			struct srhment *hme;

			hme = (struct srhment *)p_mapping;
			ASSERT(hme->ghme.hme_page == pp);

			p_mapping = (u_int) (hme->ghme.hme_next);
			hat = &hats[hme->ghme.hme_hat];
			if (hat->hat_op != &srmmu_hatops) {
				continue;
			}

			ptbl = hme->hme_ptbl;
			pte = ptbltopt_va(ptbl) + hme->hme_index;
		}

		/*
		 * If the translation has no context, it can't be
		 * in the cache.
		 */
		if (!static_ctx) {
			mutex_enter(&hat->hat_mutex);
		}

		/*
		 * If there are multiple mappings established
		 * thru ISM, they are not visible here, so use this
		 * new routine to flush all va mappings to this page
		 */
		if (ptbl->ptbl_flags & PTBL_SHARED) {
			vaddr = ptetovaddr(ptbl, pte);
			spt_vacsync(vaddr, hat->hat_as);
		}

		srmmu = hattosrmmu(hat);
		nctx = srmmu->s_ctx;
		if (nctx != -1) {
			ASSERT(VALID_CTX(nctx));

			/*
			 * Calculate the virtual address, switch to the
			 * correct context, and flush the page.
			 */
			vaddr = ptetovaddr(ptbl, pte);
			XCALL_PROLOG;
			srmmu_vacflush(3, vaddr, nctx, FL_TLB_CACHE);
			XCALL_EPILOG;
		} else {
			ASSERT(static_ctx == 0);
		}

		if (!static_ctx) {
			mutex_exit(&hat->hat_mutex);
		}
	}
}
#endif /* VAC */

/* ARGSUSED */
static int
srmmu_probe(hat, as, addr)
	struct hat *hat;
	struct as *as;
	caddr_t addr;
{
	struct pte *pte, tpte;
	int level;
	ptbl_t *ptbl;
	kmutex_t *mtx;

	pte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx, LK_PTBL_SHARED);
	mmu_readpte(pte, &tpte);
	unlock_ptbl(ptbl, mtx);

	return ((tpte.EntryType == MMU_ET_PTE));
}

/*
 * Unload a pte. If required, sync the referenced & modified bits.
 */
/*ARGSUSED*/
page_t *
srmmu_pteunload(ptbl, pte, flags, got_mlist_lock)
	ptbl_t *ptbl;
	struct pte *pte;
	int flags;
	int got_mlist_lock;
{
	page_t *pp;
	srhment_t *hme;
	int level;
	struct as *as;
	kmutex_t *pml;
	page_t *rpp = NULL;

	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));
	level = PTBL_LEVEL(ptbl->ptbl_flags);
	as = ptbl->ptbl_as;

	ASSERT(as != NULL);
	ASSERT(level != 0);

	pp = pfn2pp_hash(pte->PhysicalPageNumber);

	/*
	 * Sync ref and mod bits back to the page and invalidate pte.
	 */
	if (flags & SR_NOFLUSH) {
		flags |= (SR_INVSYNC | SR_RMSTAT);
	} else {
		flags |= (SR_INVSYNC | SR_RMSYNC);
	}

#ifdef VAC
	if (do_pg_coloring && (cache & CACHE_VAC) && pte->Cacheable) {
		flags |= SR_NOPGFLUSH;
		ASSERT(ADDR_2_VCOLOR(ptetovaddr(ptbl, pte)) ==
		    mach_pp->p_vcolor);
	}
#endif /* VAC */

	hme = srmmu_ptesync(ptbl, pte, flags, got_mlist_lock);

	/*
	 * Remove the pte from the list of mappings for the page.
	 */
	if ((pp != NULL) && (hme != (struct srhment *)-1)) {
		/* mappings exist on p_mapping list */
		if (got_mlist_lock == NO_MLIST_LOCK) {
			pml = srmmu_mlist_enter(pp);
		}
		hme_may_sub(ptbl, pte, hme, pp);

#ifdef VAC
		if ((cache & CACHE_VAC) && PP_ISTNC(pp)) {
			if ((flags & SR_UNLOAD_ALL) == 0) {
				if (srmmu_vac_alias(pp) != HAT_DONE) {
					/*
					 * Notify caller this page need
					 * more vac work.
					 */
					rpp = pp;
				}
			} else {
				if (mach_pp->p_mapping == NULL) {
					srmmu_page_enter(pp);
					PP_CLRTNC(pp);
					srmmu_page_exit(pp);
				}
			}
		}
#endif
		if (got_mlist_lock == NO_MLIST_LOCK) {
			srmmu_mlist_exit(pml);
		}
	}

#ifndef VAC
	if ((flags & HAT_UNLOAD_UNLOCK) && (as != &kas) && (level == 3)) {
		ptbl->ptbl_lockcnt--;
	}
#else
	/* Maintain kas lockcnt for VAC. */
	if ((flags & HAT_UNLOAD_UNLOCK) && (level == 3)) {
		ptbl->ptbl_lockcnt--;
	}
#endif

	if ((pp != NULL) && (hme != (struct srhment *)-1)) {
		int rss = astosrmmu(as)->s_rss;

		if (level == 3) {
			rss--;
		} else {
			rss -= mmu_btop(srmmu_sizes[level]);
		}

		/*
		 * Since we no longer hold the hat_mutex for this,
		 * and there is no serious user of this information,
		 * we just make sure the number doesn't go negative.
		 */
		if (rss >= 0) {
			astosrmmu(as)->s_rss = rss;
		} else {
			astosrmmu(as)->s_rss = 0;
		}
	}

	/*
	 * Decrement the valid count on page table. Level one does
	 * not maintain valid count.
	 */
	if (level != 1) {
		ASSERT(PTBL_VALIDCNT(ptbl->ptbl_validcnt));
		ptbl->ptbl_validcnt--;
	}

	return (rpp);
}

/*
 * helper routine for srmmu_ptesync.
*/
static void
srmmu_set_rm(pp, rm)
page_t	*pp;
u_int rm;
{
	srmmu_page_enter(pp);
	if ((rm & PTE_REF_MASK) && !PP_ISREF(pp)) {
		PP_SETREF(pp);
	}
	if ((rm & PTE_MOD_MASK) && !PP_ISMOD(pp)) {
		PP_SETMOD(pp);
	}
	srmmu_page_exit(pp);
}

/*
 * Sync the referenced and modified bits of the page struct with the
 * pte. Clears the bits in the pte.  Also, synchronizes the Cacheable bit
 * in the pte with the noncacheable bit in the page struct.  Any change
 * to the PTE requires the TLBs to be flushed, so subsequent accesses or
 * modifies will really cause the memory image of the PTE to be modified.
 */
static struct srhment *
srmmu_ptesync(ptbl, pte, flags, got_mlist_lock)
	ptbl_t		*ptbl;
	struct pte *pte;
	int flags;
	int got_mlist_lock;
{
	register struct page *pp;
	int h_nosync, level;
	caddr_t vaddr;
	u_int apte, rm;
	int inval_done = 0;
	struct srhment *hme = NULL;
	u_int p_mapping;
	kmutex_t *pml;
	u_int tlbflush = 0;

	/*
	 * if SR_NOFLUSH is set then neither the cache nor TLB will be
	 * flushed.  For vac using page coloring, we can skip cache flush
	 * but not TLB flush, if HAT_NOPGFLUSH is set
	 */
	if (!(flags & SR_NOFLUSH)) {
		tlbflush = flags & SR_NOPGFLUSH;
	}

	vaddr = ptetovaddr(ptbl, pte);

	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));
	level = PTBL_LEVEL(ptbl->ptbl_flags);

	pp = pfn2pp_hash(pte->PhysicalPageNumber);
	if (pp == NULL) {
		goto do_flush;
	}

	/* Need to look up nosync bit. */
	if (got_mlist_lock != MLIST_LOCKED) {
		pml = srmmu_mlist_enter(pp);
	}

	p_mapping = (u_int) mach_pp->p_mapping;
	if (p_mapping & PP_PTBL) {
		if ((ptbltopt_va(MAP2PTBL(p_mapping)) + mach_pp->p_index)
		    == pte) {
			h_nosync = p_mapping & PP_NOSYNC;
			hme = NULL;
		} else {
			h_nosync = 1;
			/*
			 * prevent call to hme_may_sub(). This
			 * could be due to HAT_LOAD_NOCONSIST load.
			 */
			hme = (struct srhment *)-1;
		}
	} else {
		if (flags & SR_INVSYNC) {
			/* find & remove for hem_may_sub() */
			hme = hashout_ptetohme(ptbl, pte);
		} else {
			hme = pte2hme(ptbl, pte);
		}

		ASSERT(hme == NULL || hme->ghme.hme_page == pp);
		if (hme) {
			h_nosync = hme->ghme.hme_nosync;
		} else {
			/*
			 * prevent call to hme_may_sub(). This
			 * could be due to HAT_LOAD_NOCONSIST load.
			 */
			hme = (struct srhment *)-1;
			h_nosync = 1;
		}
	}

	if (got_mlist_lock != MLIST_LOCKED) {
		srmmu_mlist_exit(pml);
	}

	/* test exclusive flags. */
	ASSERT((flags & (SR_RMSYNC | SR_RMSTAT)) != (SR_RMSYNC | SR_RMSTAT));
	ASSERT((flags & (SR_RMSYNC | SR_RMSTAT)) != 0);

	if (!h_nosync) {
		if (flags & SR_RMSYNC) {
			struct as *pas;

			pas = ptbl->ptbl_as;

			if (flags & SR_INVSYNC) {
				inval_done = 1;
				rm = mmu_writepte(pte, MMU_STD_INVALIDPTE,
					vaddr, level, pas->a_hat, tlbflush);
			} else {
				u_int pfn =  mach_pp->p_pagenum;
				mmu_readpte(pte, (struct pte *)&apte);
				if (apte & PTE_RM_MASK) {
					if (enable_mbit_wa && URWX(pte) &&
					    pfn <=  (u_int)physmax &&
					    pf_is_memory(pfn)) {
						SET_PTE_AP(&apte,
						    MMU_STD_SRXURX);
					}
					apte &= ~PTE_RM_MASK;
					rm = mmu_writepte(pte, apte,
						vaddr, level, pas->a_hat,
						tlbflush);
				} else {
					rm = 0;
				}
			}

			if (rm) {
				if (astosrmmu(pas)->s_rmstat) {
					/*
					 * The r and m bits are reversed
					 * from the sunmmu, setstat expects
					 * them in that order.
					 */
					hat_setstat(pas, vaddr, MMU_PAGESIZE,
					    ((rm & PTE_REF_MASK) >>
					    PTE_RM_SHIFT - 1) |
					    ((rm & PTE_MOD_MASK) >>
					    PTE_RM_SHIFT + 1));
				}

				if (((rm & PTE_REF_MASK) && !PP_ISREF(pp)) ||
				    ((rm & PTE_MOD_MASK) && !PP_ISMOD(pp))) {
					srmmu_set_rm(pp, rm);
				}
			}
		} else if (flags & SR_RMSTAT) {
			mmu_readpte(pte, (struct pte *)&apte);
			apte &= PTE_RM_MASK;

			if ((apte) &&
			    (((apte & PTE_REF_MASK) && !PP_ISREF(pp)) ||
			    ((apte & PTE_MOD_MASK) && !PP_ISMOD(pp)))) {
				srmmu_set_rm(pp, apte);
			}
		}
	}

do_flush:
	if (flags & SR_INVSYNC && !inval_done) {
		MOD_VALID_PTE(flags, pte, MMU_STD_INVALIDPTE, vaddr,
		    level, ptbl->ptbl_as->a_hat, tlbflush);
	}

	ASSERT((flags & SR_INVSYNC) == 0 || ! pte_valid(pte));

	return (hme);
}

#ifdef VAC
/*
 * Check a page's address aliases.
 * Return value is:
 *	NO_CONLICT:		page mappings are correct
 *	UNLOAD_CONFLICT:	page conflicts resolved, unload mappings
 *	UNCACHE_CONFLICT:	page conflicts resolved, re-cache mappings
 */
static int
srmmu_vac_alias(pp)
	struct page *pp;
{
	struct srhment *hme;
	int retval = NO_CONFLICT;
	u_int p_mapping;

	ASSERT(ohat_mlist_held(pp));

	if (mach_pp->p_mapping == NULL) {
		ASSERT(PP_ISTNC(pp));

		srmmu_page_enter(pp);
		PP_CLRTNC(pp);
		srmmu_page_exit(pp);
	} else {
		register struct srhment *ahme, *bhme;
		ptbl_t *aptbl;
		int ccf = 0;
		int locked_mapping = 0;

		p_mapping = (u_int) mach_pp->p_mapping;
		if ((p_mapping & PP_PTBL) || (mach_pp->p_share == 1)) {
			if (p_mapping & PP_PTBL) {
				aptbl = MAP2PTBL(p_mapping);
			} else {
				hme = (struct srhment *)p_mapping;
				ASSERT(hme->ghme.hme_page == pp);
				if (hme->ghme.hme_noconsist) {
					return (HAT_DONE);
				}
				aptbl = hme->hme_ptbl;
			}

			ccf = 0;
			if ((aptbl->ptbl_lockcnt > 0) ||
				KAS_NO_FAULT(aptbl)) {
				locked_mapping = 1;
			}

		} else {
			hme = (struct srhment *)p_mapping;
			ASSERT(hme->ghme.hme_page == pp);
			if (hme->ghme.hme_noconsist) {
				return (HAT_DONE);
			}

			for (ahme = hme; ahme != NULL;
			    ahme = (struct srhment *)(ahme->ghme.hme_next)) {
				struct hat *hat;

				hat = &hats[ahme->ghme.hme_hat];
				if (hat->hat_op != &srmmu_hatops) {
					continue;
				}

				aptbl = ahme->hme_ptbl;
				ASSERT(PTBL_LEVEL(aptbl->ptbl_flags) == 3);

				if (ahme->ghme.hme_noconsist) {
					/* XXX - should be first on list */
					ccf = 1;
					break;
				}

				if ((aptbl->ptbl_lockcnt > 0) ||
				    KAS_NO_FAULT(aptbl)) {
					locked_mapping = 1;
				}

				if ((bhme = (struct srhment *)
				    (ahme->ghme.hme_next)) != NULL) {
					ptbl_t	*bptbl;
					caddr_t aa, ba;

					bptbl = bhme->hme_ptbl;
					ASSERT(PTBL_LEVEL(bptbl->ptbl_flags)
					    == 3);

					aa = BASE2VA(aptbl)
					    + (ahme->hme_index <<
					    MMU_STD_PAGESHIFT);
					ba = BASE2VA(bptbl)
					    + (bhme->hme_index <<
					    MMU_STD_PAGESHIFT);

					if (!VAC_ALIGNED(aa, ba)) {
						ccf = 1;
						break;
					}

					if ((bptbl->ptbl_lockcnt > 0) ||
					    KAS_NO_FAULT(bptbl)) {
						locked_mapping = 1;
					}
				}

			}
		}

		if (!ccf) {
			if (locked_mapping) {
				retval = UNCACHE_CONFLICT;
			} else {
				retval = UNLOAD_CONFLICT;
			}

			if (avoid_capture_release) {
				retval = UNLOAD_CONFLICT;
			}
		}

		SRMMU_LOG(pp, retval, 0, 0, 0, 0);

		if (retval & ANY_CONFLICT) {
			if (retval == UNCACHE_CONFLICT) {
				/*
				 * srmmu_page_cache(UNCACHE_CONFLICT)
				 * expects the bit be cleared. There is
				 * no problem in this case since it
				 * never fails in this case.
				 */
				srmmu_page_enter(pp);
				PP_CLRTNC(pp);
				srmmu_page_exit(pp);
			}
			return (srmmu_page_cache(pp, retval));
		}
	}

	return (HAT_DONE);
}

/*
 * VAC specific routine to do the same thing as ptesync. MUST BE CALLED
 * WITHIN A CAPTURE/RELEASE.
 */
static void
srmmu_ptecachectl(ptbl, pte, pp)
	register ptbl_t *ptbl;
	register struct pte *pte;
	register struct page *pp;
{
	struct as *as;
	int cxn;
	caddr_t vaddr;
	srmmu_t *srmmu;
	struct pte oldpte;

	vaddr = ptetovaddr(ptbl, pte);
	as = ptbl->ptbl_as;
	srmmu = astosrmmu(as);
	cxn = srmmu->s_ctx;

	/*
	 * No per hat_mutex protection below as this is within a capture/
	 * release and we are only reading the data otherwise protected by
	 * the per hat_mutex. Grabbing a per hat_mutex here can cause
	 * deadlocks.
	 *
	 * NOTE THAT SINCE THIS CODE CHANGES MAPPINGS FROM
	 * CACHED TO UNCACHED IT MUST BE DONE INSIDE OF
	 * A CAPTURE-RELEASE.  THIS IS TAKEN CARE OF BY THE
	 * CALLER.
	 */

	mmu_readpte(pte, &oldpte);
	if (oldpte.Cacheable && PP_ISNC(pp)) {
		/*
		 * Need to convert from a cached translation to a
		 * non-cached translation. There are lots of potential
		 * races here in the kernel's address space.  We assume
		 * that between the time that we flush the virtual
		 * address and reset the MMU that nothing will be
		 * getting into the cache from things like ethernet.
		 */
		oldpte.Cacheable = 0;
		*pte = oldpte;
		if (cxn != -1)
			vac_pageflush(vaddr, cxn, FL_TLB_CACHE);
#ifdef sun4m
	/*
	 * The test for a page frame number in SX address space is a
	 * temporary fix for bug #4027042. The real fix for the bug is
	 * a new SX hat interface (see bug #1257737) which did not make
	 * it into 2.6. When that interface is installed, the test for
	 * page frame number should be able to be removed.
	 */
	} else if (!oldpte.Cacheable && !PP_ISNC(pp) &&
		((oldpte.PhysicalPageNumber & 0xf00000) != 0x800000)) {
#else
	} else if (!oldpte.Cacheable && !PP_ISNC(pp)) {
#endif
		oldpte.Cacheable = 1;
		*pte = oldpte;
		if (cxn != -1)
			srmmu_tlbflush(3, vaddr, cxn, FL_ALLCPUS);

		/*
		 * A new cacheable mapping is added. Update
		 * the color. We don't need to need to flush
		 * the cache again here since it's flushed
		 * when they were converted from Cacheable to
		 * non cacheable in code above.
		 */
		if ((cache & CACHE_VAC) && do_pg_coloring)
			mach_pp->p_vcolor = ADDR_2_VCOLOR(vaddr);
	}
}
#endif	/* VAC */

static struct pte *
build_subtree(parent, level, as, addr, pptbl, pmtx, flags)
	ptbl_t *parent;
	struct as *as;
	caddr_t	addr;
	int level;
	ptbl_t **pptbl;
	kmutex_t **pmtx;
	int flags;
{
	int		plevel;		/* parent ptbl level */
	ptbl_t		*ptbl[4];	/* level starts from 1, not 0. */
	kmutex_t	*mtx[4];
	union ptpe	*pentry[4], ptpe; /* level starts from 1, not 0. */
	u_int		base[4];
	int		i;
	struct ptp	tmppar;
	int		lkflag;
	int		aflag;

	plevel = PTBL_LEVEL(parent->ptbl_flags);

	ASSERT(plevel == 1 || plevel == 2);
	ASSERT(level > plevel);
	ASSERT((u_int) addr < KERNELBASE || as == &kas);
	ASSERT((u_int) addr >= KERNELBASE || as != &kas);

	/*
	 * If it's on tmpunload tree, we are here for a L2,
	 * never a L3.
	 */
	ASSERT((flags & SR_TMP_TREE) == 0 ||
		(level == 2 && plevel == 1 && as != &kas));

	if (!(flags & SR_TMP_TREE)) {
		lkflag = 0;
		aflag = 0;
	} else {
		lkflag = LK_PTBL_TMP;
		aflag = GET_TMP_PTBL | NO_ALLOC;
	}

	/*
	 * Allocate new ptbl(s).
	 */
	i = plevel + 1;

	ptbl[plevel] = parent;
	base[plevel] = ((u_int)addr & ~(srmmu_sizes[plevel - 1] -1));

	do {
		ASSERT(i == 2 || i == 3);

		if (as != &kas) {
			if (i == 2 || plevel == 2) {
				ptbl[i] = srmmu_ptbl_alloc(KM_SLEEP,
				    aflag, i, &mtx[i]);
			} else {
				/*
				 * Don't allocate new ptbls if we are
				 * allocating a L3 ptbl and a L2 ptbl
				 * at the same time. While allocating
				 * a L3 we might need to get another
				 * L2 lock which might be hashed into
				 * the L2 lock that we are holding.
				 * Also, this could be on a TMP TREE.
				 */
				ASSERT(aflag == 0);
				ptbl[i] = srmmu_ptbl_alloc(KM_SLEEP,
				    NO_ALLOC, i, &mtx[i]);
			}
		} else {
			/*
			 * NOSLEEP for kas. Also set NO_ALLOC flag for
			 * kernelmap area so that we don't try to call
			 * kmem_alloc() while we are called from
			 * kmem_alloc().
			 */
			u_int flag;

			if (addr < kernelheap || addr >= ekernelheap) {
				flag = 0;
			} else {
				flag = NO_ALLOC;
			}
			ptbl[i] = srmmu_ptbl_alloc(KM_NOSLEEP,
				    flag, i, &mtx[i]);
		}

		ASSERT(PTBL_IS_LOCKED(ptbl[i]->ptbl_flags));
		ASSERT(ptbl[i]->ptbl_lockcnt == 0);
		ASSERT(ptbl[i]->ptbl_validcnt == 0);

		base[i] = ((u_int)addr & ~(srmmu_sizes[i - 1] -1));
		ptbl[i]->ptbl_as = as;
		ptbl[i]->ptbl_base = VA2BASE(base[i]);
		ptbl[i]->ptbl_parent = ptbl[i-1];

		if (as == &kas) {
			ptbl[i]->ptbl_flags |= PTBL_KEEP; /* mark kernel ptbl */
			ASSERT((flags & (HAT_LOAD_SHARE | SR_TMP_TREE)) == 0);
		} else {
			ASSERT((flags & (HAT_LOAD_SHARE | SR_TMP_TREE)) !=
			    (HAT_LOAD_SHARE | SR_TMP_TREE));
			if (flags & HAT_LOAD_SHARE) {
				ptbl[i]->ptbl_flags |= PTBL_SHARED;
			}
		}
	} while (++i <= level);

	if ((level == 3) && (plevel == 1)) {
		/*
		 * Connect L3 pt to the brand spanking new L2 pt/ptbl.
		 */
		pentry[2] = (union ptpe *)ptbltopt_va(ptbl[2]) +
			MMU_L2_INDEX(addr);
		/* Attach the new L3 now. */
		ptpe.ptpe_int = ptbltopt_ptp(ptbl[3]);
		SET_NEW_PTP(&(pentry[2]->ptp), ptpe.ptpe_int,
		    (caddr_t)base[2], 2, 0);
		ptbl[2]->ptbl_validcnt = 1;

		unlock_ptbl(ptbl[2], mtx[2]);
		mtx[2] = NULL;
	}

	ptpe.ptpe_int = ptbltopt_ptp(ptbl[plevel+1]);
	if (plevel == 1) {
		/*
		 * We are attaching both an L2 and an L3.
		 * Note that L1 pt's have 4 ptbl's so we
		 * need to take care of ptbl_base here.
		 */
		pentry[1] = (union ptpe *)ptbltopt_va(parent) +
			MMU_L1_INDEX(addr - BASE2VA(parent));
	} else {
		/* We are attaching just an L3 */
		pentry[2] = (union ptpe *)ptbltopt_va(parent) +
			MMU_L2_INDEX(addr);
	}

	/*
	 * Attach the child(ren) to the parent.  We hold the child's
	 * mutex, and need the parent's.  This is the reverse order.
	 * So, if we can't get the parent's lock, drop the child's
	 * lock, get the parent's, re-acquire the child's, and then
	 * attach the child(ren).  To prevent L3 children from being
	 * stolen, we set the PTBL_KEEP bit (which normally means the
	 * ptbl is a kernel ptbl) just before we drop the child's mutex.
	 * Regardless of which address space (kas or other) L1 and L2
	 * ptbl's are not stolen.  We do not have to worry about the
	 * parent disappearing.
	 */
	if (lock_ptbl(parent, lkflag | LK_PTBL_NOWAIT, as, addr,
	    plevel, &mtx[plevel]) != LK_PTBL_OK) {
		ptbl_t *child;

		child = ptbl[level];
		if (as != &kas) {
			child->ptbl_flags |= PTBL_KEEP;
		}
		unlock_ptbl(child, mtx[level]);
		mtx[level] = NULL;

		(void) lock_ptbl(parent, lkflag, as, addr, plevel,
		    &mtx[plevel]);
		(void) lock_ptbl(child, lkflag, as, (caddr_t)base[level],
		    level, &mtx[level]);

		if (as != &kas) {
			child->ptbl_flags &= ~PTBL_KEEP;
		}
	}

	/*
	 * After that dance both the child and the parent are locked.
	 */

	mmu_readptp((struct ptp *)pentry[plevel], &tmppar);
	if (tmppar.EntryType == MMU_ET_INVALID) {
		SET_NEW_PTP(&(pentry[plevel]->ptp), ptpe.ptpe_int,
		    (caddr_t)base[plevel], plevel, 0);
		if (plevel == 2) {
			parent->ptbl_validcnt++;
			ASSERT(PTBL_VALIDCNT(parent->ptbl_validcnt));
		}
		unlock_ptbl(parent, mtx[plevel]);
		mtx[plevel] = NULL;

		*pptbl = ptbl[level];
		*pmtx = mtx[level];

		if (level == 3) {
			i = MMU_L3_INDEX(addr);
		} else {
			i = MMU_L2_INDEX(addr);
		}
		return (ptbltopt_va(ptbl[level]) + i);
	} else {
		/*
		 * Someone has allocated ptbls before us!
		 */
		unlock_ptbl(parent, mtx[plevel]);

		/*
		 * Release L3 ptbl. Otherwise, Lock_ptbl(L2)
		 * below would violate lock ordering- ie. trying
		 * to get a L2 lock while holding a L3 lock.
		 */
		if (level == 3) {
			ptbl[3]->ptbl_flags &= ~PTBL_KEEP;
			srmmu_ptbl_free(ptbl[3], mtx[3]);
			mtx[3] = NULL;
		}

		if (plevel == 1) {
			if (level == 3) {
				/*
				 * Clear out the L2 entry pointing to
				 * the L3 ptbl. We dropped L2 ptbl
				 * lock already.
				 */
				ASSERT((flags & SR_TMP_TREE) == 0);
				(void) lock_ptbl(ptbl[2], 0, as, addr, 2,
				    &mtx[2]);
				pentry[2]-> ptpe_int = 0;
				ptbl[2]->ptbl_validcnt = 0;
			}
			srmmu_ptbl_free(ptbl[2], mtx[2]);
		}

		return (NULL);
	}
	/*NOTREACHED*/
}

#if defined(SRMMU_TMPUNLOAD)

/*
 * This routine converts a locked ptbl into a TMP
 * or a NONTMP ptbl. The ptbl remained locked when it's done.
 * It also assmues it can write to the ptbl_flag field.
 */
static kmutex_t *
cv_locked_ptbl(ptbl_t *ptbl, kmutex_t *omtx)
{
	kmutex_t *mtx;
	u_char pflag;

	pflag = ptbl->ptbl_flags;
	ASSERT(PTBL_IS_LOCKED(pflag));
	ASSERT((pflag & PTBL_KEEP) == 0);

	/* Set the new TMP and PTBL_KEEP bits so it cannot be stolen. */
	pflag ^= (PTBL_TMP | PTBL_KEEP | PTBL_LOCKED);
	ptbl->ptbl_flags = pflag;

	mutex_exit(omtx);

	/*
	 * Get the new lock and set the ptbl_flags field.
	 */
	(void) lock_this_ptbl(ptbl, 0, &mtx);
	ptbl->ptbl_flags &= ~PTBL_KEEP;

	return (mtx);
}

/*
 * This routine convert a L2 worth of L3 ptbls either
 * from TMP to NONTMP or back. The L3s are NOT locked
 * when its done.
 */
static void
mod_l3tmp_bits(fl2ptbl, as, addr, action)
	ptbl_t *fl2ptbl;
	int action;
	struct as *as;
	caddr_t addr;
{
	int i;
	struct ptp *fl2ptp, tl2ptp;
	ptbl_t *fl3ptbl;
	kmutex_t *fl3mtx;
	u_char pflag;
	int lockflag;

	fl2ptp = (struct ptp *)ptbltopt_va(fl2ptbl);
	if (action == CLR_TMP) {
		lockflag = LK_PTBL_TMP;
	} else {
		lockflag = 0;
	}

	/* start at the beginning */
	addr = (caddr_t)((u_int)(addr) & ~(MMU_L1_SIZE - 1));

	for (i = 0; i < NL2PTEPERPT; i++, fl2ptp++, addr += MMU_L2_SIZE) {
		mmu_readptp(fl2ptp, &tl2ptp);
		switch (tl2ptp.EntryType) {
		case MMU_ET_PTP:
			fl3ptbl = tatoptbl(tl2ptp.PageTablePointer);
			(void) lock_ptbl(fl3ptbl, lockflag, as, addr, 3,
			    &fl3mtx);

			pflag = fl3ptbl->ptbl_flags;
			if (action == CLR_TMP) {
				pflag &= ~PTBL_TMP;
			} else {
				pflag |= PTBL_TMP;
			}
			pflag &= ~PTBL_LOCKED;
			fl3ptbl->ptbl_flags = pflag;
			mutex_exit(fl3mtx);
			break;

		case MMU_ET_INVALID:
			break;

		case MMU_ET_PTE:
		default:
			cmn_err(CE_PANIC, "bad fl2ptp %p", (void *)fl2ptp);
		}
	}
}

static struct ptp *
recover_l2tmp(srmmu_t *srmmu, struct as *as,
	caddr_t addr, struct ptp *rl1ptp, ptbl_t **l2ptbl)

{
	struct ptp *fl1ptp, *fl2ptp, *rl2ptp;
	struct ptp tfl1ptp, trl1ptp;
	ptbl_t *fl1ptbl, *fl2ptbl;
	ptbl_t *rl1ptbl;
	kmutex_t *rl1mtx, *fl1mtx, *fl2mtx;
	u_char flags;

	/*
	 * The order of locks here is to lock on the real tree
	 * first then the fake tree.
	 */
	rl1ptbl = VA2L1PTBL(srmmu->s_l1ptbl, addr);
	(void) lock_ptbl(rl1ptbl, 0, as, addr, 1, &rl1mtx);

	mmu_readptp(rl1ptp, &trl1ptp);
	switch (trl1ptp.EntryType) {
	case MMU_ET_INVALID:
		break;

	case MMU_ET_PTP:
		/*
		 * Mmmm, a real L2 pt now exists.
		 */
		unlock_ptbl(rl1ptbl, rl1mtx);
		tatoptbl_pte(trl1ptp.PageTablePointer, l2ptbl,
		    (union ptpe **)&rl2ptp);
		rl2ptp += MMU_L2_INDEX(addr);
		return (rl2ptp);

	default:
		cmn_err(CE_PANIC, "recover_l2tmp: l1pte %x",
		    *(u_int*)&trl1ptp);
	}

	fl1ptp = (struct ptp *)ptbltopt_va(srmmu->s_tmpl1ptbl);
	fl1ptp += MMU_L1_INDEX(addr);

	fl1ptbl = VA2L1PTBL(srmmu->s_tmpl1ptbl, addr);

	(void) lock_ptbl(fl1ptbl, LK_PTBL_TMP, as, addr, 1, &fl1mtx);
	ASSERT(PTBL_IS_FAKE(fl1ptbl->ptbl_flags));

	mmu_readptp(fl1ptp, &tfl1ptp);
	switch (tfl1ptp.EntryType) {
	case MMU_ET_PTP:
		tatoptbl_pte(tfl1ptp.PageTablePointer, &fl2ptbl,
		    (union ptpe **)&fl2ptp);
		fl2ptp += MMU_L2_INDEX(addr);
		(void) lock_ptbl(fl2ptbl, LK_PTBL_TMP, as, addr, 2, &fl2mtx);
		/*
		 * Clear TMP bit in all L3s. None of the L3 can be stolen
		 * since L2 is locked.
		 */
		mod_l3tmp_bits(fl2ptbl, as, addr, CLR_TMP);

		/*
		 * Unlink the L2 from the fake tree and link it on
		 * the real tree.
		 */
		/* No validcnt for L1 ptbl, just write ptps. */
		fl2ptbl->ptbl_parent = rl1ptbl;

		flags = fl2ptbl->ptbl_flags;
		flags &= ~(PTBL_TMP | PTBL_LOCKED);
		fl2ptbl->ptbl_flags = flags;
		mutex_exit(fl2mtx);

		SET_NEW_PTP(rl1ptp, *(int *)fl1ptp, MMU_L1_BASE(addr), 1, 0);
		/*LINTED*/
		MOD_VALID_PTP(SR_TMP_TREE, fl1ptp, MMU_ET_INVALID,
		    MMU_L1_BASE(addr), 1, as->a_hat, 0);

		unlock_ptbl(fl1ptbl, fl1mtx);
		unlock_ptbl(rl1ptbl, rl1mtx);
		*l2ptbl = fl2ptbl;

		return (fl2ptp);

	case MMU_ET_INVALID:
		unlock_ptbl(fl1ptbl, fl1mtx);
		unlock_ptbl(rl1ptbl, rl1mtx);
		return (NULL);

	case MMU_ET_PTE:
	default:
		cmn_err(CE_PANIC, "recover_l2tmp: fl1pte 0x%x",
		    *(u_int*)&tfl1ptp);
	}
	/*NOTREACHED*/
}

static struct pte *
recover_l3tmp(
	struct as	*as,
	caddr_t		addr,
	struct ptp	*rl2ptp,
	ptbl_t		*rl2ptbl,
	ptbl_t		**pptbl,
	kmutex_t	**pmtx)
{
	struct ptp *fl1ptp, *fl2ptp;
	struct pte *l3pte;
	struct ptp tfl1ptp, tfl2ptp, trl2ptp;
	ptbl_t *fl2ptbl, *fl3ptbl;
	ptbl_t *rl3ptbl;
	kmutex_t *fl1mtx, *fl2mtx, *fl3mtx, *rl2mtx, *rl3mtx;
	ptbl_t		*tmpl1ptbl;
	srmmu_t		*srmmu = astosrmmu(as);

	tmpl1ptbl = srmmu->s_tmpl1ptbl;
again:
	/*
	 * The order of locks here is to lock on the real tree
	 * first then the fake tree.
	 */
	(void) lock_ptbl(rl2ptbl, 0, as, addr, 2, &rl2mtx);

	/*
	 * Now that we have locked the ptbl, see if it's what
	 * we expected it to be.
	 */
	mmu_readptp(rl2ptp, &trl2ptp);
	switch (trl2ptp.EntryType) {
	case MMU_ET_INVALID:
		break;

	case MMU_ET_PTP:
		/*
		 * Mmmm, the L2 ptbl now exists.
		 */
		unlock_ptbl(rl2ptbl, rl2mtx);
		tatoptbl_pte(trl2ptp.PageTablePointer, &rl3ptbl,
		    (union ptpe **)&l3pte);
		l3pte += MMU_L3_INDEX(addr);

		if (lock_ptbl(rl3ptbl, LK_PTBL_FAILOK, as, addr, 3,
		    &rl3mtx) != LK_PTBL_OK) {
			/* Someone just stole it. */
			goto again;
		}
		*pptbl = rl3ptbl;
		*pmtx = rl3mtx;
		return (l3pte);

	default:
		cmn_err(CE_PANIC, "recover_l3tmp: rl2ptp %x",
		    *(u_int *) &trl2ptp);
	}

	fl1ptp = (struct ptp *)ptbltopt_va(tmpl1ptbl);
	fl1ptp += MMU_L1_INDEX(addr);

	ASSERT(PTBL_IS_FAKE(tmpl1ptbl->ptbl_flags));

fake_again:
	mmu_readptp(fl1ptp, &tfl1ptp);
	switch (tfl1ptp.EntryType) {
	case MMU_ET_PTP:
		tatoptbl_pte(tfl1ptp.PageTablePointer, &fl2ptbl,
		    (union ptpe **)&fl2ptp);
		fl2ptp += MMU_L2_INDEX(addr);
		(void) lock_ptbl(fl2ptbl, LK_PTBL_TMP, as, addr, 2, &fl2mtx);

		mmu_readptp(fl2ptp, &tfl2ptp);
		switch (tfl2ptp.EntryType) {
		case MMU_ET_INVALID:
			unlock_ptbl(rl2ptbl, rl2mtx);
			unlock_ptbl(fl2ptbl, fl2mtx);
			return (NULL);

		case MMU_ET_PTP:
			tatoptbl_pte(tfl2ptp.PageTablePointer,
			    &fl3ptbl, (union ptpe **)&l3pte);
			l3pte += MMU_L3_INDEX(addr);
			(void) lock_ptbl(fl3ptbl, LK_PTBL_TMP, as, addr, 3,
			    &fl3mtx);

			/* Unlink the L3 from the fake tree */
			fl3ptbl->ptbl_parent = rl2ptbl;

			/* Covert it to a NONTMP ptbl. */
			fl3mtx = cv_locked_ptbl(fl3ptbl, fl3mtx);

			/* link the L3 onto the real tree */
			SET_NEW_PTP(rl2ptp, *(int *)fl2ptp, MMU_L2_BASE(addr),
			    2, 0);

			/*LINTED*/
			MOD_VALID_PTP(SR_TMP_TREE, fl2ptp, MMU_ET_INVALID,
			    MMU_L2_BASE(addr), 2, as->a_hat, 0);

			/*
			 * we moved the l3 from fake and put
			 * it on the real, adjust the counts
			 * accordingly.
			 */
			fl2ptbl->ptbl_validcnt--;
			rl2ptbl->ptbl_validcnt++;

			unlock_ptbl(fl2ptbl, fl2mtx);
			unlock_ptbl(rl2ptbl, rl2mtx);

			*pptbl = fl3ptbl;
			*pmtx = fl3mtx;
			return (l3pte);

		case MMU_ET_PTE:
		default:
			cmn_err(CE_PANIC, "recover_l3tmp: fl2ptp %x",
			    *(u_int *)&tfl2ptp);
		}
		/*NOTREACHED*/

	case MMU_ET_INVALID:
		(void) lock_ptbl(VA2L1PTBL(tmpl1ptbl, addr), LK_PTBL_TMP,
		    as, addr, 1, &fl1mtx);

		mmu_readptp(fl1ptp, &tfl1ptp);
		unlock_ptbl(VA2L1PTBL(tmpl1ptbl, addr), fl1mtx);
		if (tfl1ptp.EntryType == MMU_ET_INVALID) {
			unlock_ptbl(rl2ptbl, rl2mtx);
			return (NULL);
		} else {
			goto fake_again;
		}
		/*NOTREACHED*/

	case MMU_ET_PTE:
	default:
		cmn_err(CE_PANIC, "recover_l3tmp: fl1ptp %x",
		    *(u_int *)&tfl1ptp);
	}
	/*NOTREACHED*/
}

#endif defined(SRMMU_TMPUNLOAD)

/*
 * Return a pointer to the pte struct for the given virtual address.
 * If necessary, both ptbl's and pt's are allocated to hold the pte.
 */
struct pte *
srmmu_ptealloc(as, addr, level, pptbl, pmtx, flags)
	struct	as *as;
	caddr_t	addr;
	int level;
	ptbl_t **pptbl;
	kmutex_t **pmtx;
	int flags;
{
	struct ptp *l1ptp, *l2ptp, tl1ptp, tl2ptp;
	struct pte *pte;
	ptbl_t		*ptbl, *l1ptbl;
	srmmu_t *srmmu;
	kmutex_t *l1mtx, *l2mtx, *l3mtx;
	int lkflag;
	pt_t		*l1pt;

#if defined(SRMMU_TMPUNLOAD)
	int tmptree;
#endif defined(SRMMU_TMPUNLOAD)

	srmmu = astosrmmu(as);
	ASSERT(static_ctx == 0 || srmmu->s_ctx != -1);

	/*
	 * It turns out that most of the time, we fault inside of
	 * the previous l3 pt.  So, to save the bother of walking
	 * the tree, we look in the srmmu structure where we cleverly
	 * saved away a pointer to the last used l3 ptbl.
	 * When we miss in the cache, we walk and reload the cache.
	 */
	if ((MMU_L2_BASE(addr) == srmmu->s_addr) &&
	    ((ptbl = srmmu->s_l3ptbl) != NULL) &&
	    (level == 3) &&
	    ((flags & SR_TMP_TREE) == 0)) {
		if (lock_ptbl(ptbl, LK_PTBL_FAILOK, as, addr, 3, &l3mtx) ==
		    LK_PTBL_OK) {
			pte = ptbltopt_va(ptbl) + MMU_L3_INDEX(addr);
			*pptbl = ptbl;
			*pmtx = l3mtx;
			return (pte);
		}
		srmmu->s_l3ptbl = NULL;
	}

	/*
	 * Walk the as's page tables for the requested pte.
	 * Allocate page tables if we find an invalid entry at a level
	 * less than requested.
	 * It is an error to find a ptp at the requested level,
	 * or a pte at a level less than requested.
	 */
	l1pt = (pt_t *)ptbltopt_va(srmmu->s_l1ptbl);
	l1ptp = (struct ptp *)l1pt + MMU_L1_INDEX(addr);
	l1ptbl = VA2L1PTBL(srmmu->s_l1ptbl, addr);
	lkflag = 0;

#if defined(SRMMU_TMPUNLOAD)
	if (! (flags & SR_TMP_TREE)) {
		tmptree = 0;
	} else {
		tmptree = 1;
		l1pt = (pt_t *)ptbltopt_va(srmmu->s_tmpl1ptbl);
		l1ptp = (struct ptp *)l1pt + MMU_L1_INDEX(addr);
		l1ptbl = VA2L1PTBL(srmmu->s_tmpl1ptbl, addr);
		lkflag = LK_PTBL_TMP;
	}
#endif defined(SRMMU_TMPUNLOAD)

retryl1:
	if (level == 1) {
		(void) lock_ptbl(l1ptbl, lkflag, as, addr, 1, &l1mtx);
		*pptbl = l1ptbl;
		*pmtx = l1mtx;
		return ((struct pte *)l1ptp);
	}

	mmu_readptp(l1ptp, &tl1ptp);

retryl1ptp:
	switch (tl1ptp.EntryType) {
	case MMU_ET_PTE:
		(void) lock_ptbl(l1ptbl, lkflag, as, addr, 1, &l1mtx);
		mmu_readptp(l1ptp, &tl1ptp);
		if (tl1ptp.EntryType == MMU_ET_PTE) {
			cmn_err(CE_PANIC, "ptealloc: l1 pte");
		}
		unlock_ptbl(l1ptbl, l1mtx);
		goto retryl1ptp;

	case MMU_ET_PTP:
		tatoptbl_pte(tl1ptp.PageTablePointer,
		    &ptbl, (union ptpe **)&l2ptp);
		l2ptp += MMU_L2_INDEX(addr);
		break;

	case MMU_ET_INVALID:

#if defined(SRMMU_TMPUNLOAD)
		/*
		 * Look for a tmpunloaded l2 table...
		 */
		if (srmmu->s_tmpl1ptbl != NULL && !tmptree) {
			l2ptp = recover_l2tmp(srmmu, as, addr, l1ptp,
				&ptbl);
			if (l2ptp != NULL) {
				/* A level 2 page table was recovered. */
				break;
			}
		}

#endif /* SRMMU_TMPUNLOAD */

		/*
		 * Allocate new ptbls.
		 */
		if ((pte = build_subtree(l1ptbl, level, as, addr,
		    pptbl, pmtx, flags)) != NULL) {
			return (pte);
		}
		goto retryl1;

	default:
		cmn_err(CE_PANIC, "srmmu_ptealloc - bad l1");
	}

	if (level == 2) {
		(void) lock_ptbl(ptbl, lkflag, as, addr, 2, &l2mtx);
		*pmtx = l2mtx;
		*pptbl = ptbl;
		return ((struct pte *)l2ptp);
	}

	mmu_readptp(l2ptp, &tl2ptp);

retryl2ptp:
	switch (tl2ptp.EntryType) {
	case MMU_ET_PTE:
		(void) lock_ptbl(ptbl, lkflag, as, addr, 2, &l2mtx);
		mmu_readptp(l2ptp, &tl2ptp);
		if (tl2ptp.EntryType == MMU_ET_PTE) {
			cmn_err(CE_PANIC, "ptealloc: l2 pte");
		}
		unlock_ptbl(ptbl, l2mtx);
		goto retryl2ptp;

	case MMU_ET_PTP:
		tatoptbl_pte(tl2ptp.PageTablePointer,
		    &ptbl, (union ptpe **)&pte);
		pte += MMU_L3_INDEX(addr);
		if (lock_ptbl(ptbl, lkflag | LK_PTBL_FAILOK, as,
		    addr, 3, &l3mtx) == LK_PTBL_OK) {
			*pptbl = ptbl;
			*pmtx = l3mtx;
			srmmu->s_l3ptbl = ptbl;
			srmmu->s_addr = MMU_L2_BASE(addr); /* table base */
			return (pte);
		}
		goto retryl1;

	case MMU_ET_INVALID:
#if defined(SRMMU_TMPUNLOAD)
		/*
		 * Look for a tmpunloaded L3 table.
		 */
		if ((srmmu->s_tmpl1ptbl != NULL) && !tmptree) {
			pte = recover_l3tmp(as, addr, l2ptp,
				ptbl, pptbl, pmtx);
			if (pte != NULL) {
				return (pte);
			}
		}
#endif /* SRMMU_TMPUNLOAD */

		if ((pte = build_subtree(ptbl, level, as, addr, pptbl,
		    pmtx, flags)) != NULL) {
			return (pte);
		}
		goto retryl1;

	default:
		cmn_err(CE_PANIC, "srmmu_ptealloc - bad l2");
	}

	cmn_err(CE_PANIC, "srmmu_ptealloc - NOT HERE");
	/*NOTREACHED*/
}

/*
 * Common unload logic for srmmu_unload.
 */
static int
srmmu_unload_loop(ptbl, first_valid, nptes, flags, rpp)
	ptbl_t *ptbl;
	int first_valid;
	int nptes;
	u_int flags;
	page_t *rpp[];
{
	register short i;
	register struct pte *pte;
	int last_possible = first_valid + nptes;
	page_t *vpp;
	int cnt;

	ASSERT(nptes <= NL3PTEPERPT);
	ASSERT(first_valid + nptes <= NL3PTEPERPT);
	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));
	ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == PTBL_LEVEL_3);
	ASSERT(first_valid >= 0);

#ifdef VAC
	if (cache & CACHE_VAC) {
		for (i = 0;  i < NL3PTEPERPT; i++)
			rpp[i] = NULL;
	}
#endif /* VAC */

	cnt = 0;
	pte = ptbltopt_va(ptbl) + first_valid;

	/*
	 * First loop: lock all of the pages that we can in the given range.
	 */
	for (i = first_valid; i < last_possible; i++, pte++) {
		struct pte tpte;

		mmu_readpte(pte, &tpte);
		if (!pte_valid(&tpte)) {

			if (first_valid == i) {
				first_valid++;
			}
			continue;
		}

		/*
		 * We own this page.  Deal with it.
		 */
		if ((vpp = srmmu_pteunload(ptbl, pte, flags, NO_MLIST_LOCK)) !=
		    NULL) {
			ASSERT((cache & CACHE_VAC));
			rpp[i] = vpp;
			cnt++;
		}

		/*
		 * If the valid count is 0, we are done.
		 */
		if (ptbl->ptbl_validcnt == 0) {
			break;
		}
	}

	return (cnt);
}


static void
srmmu_sync(as, addr, len, clearflag)
	struct as *as;
	caddr_t addr;
	u_int len;
	u_int clearflag;
{
	struct pte *pte, tpte;
	ptbl_t *ptbl;
	kmutex_t *mtx;
	u_int span;
	int level, hatflag = clearflag ? SR_RMSYNC : SR_RMSTAT;

	for (; len; addr += span, len -= span) {

		pte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx,
			LK_PTBL_SHARED);
		mmu_readpte(pte, &tpte);

		span = srmmu_sizes[level];
		span -= ((u_int)addr & (span - 1));
		span = MIN(len, span);

		if (! pte_valid(&tpte)) {
			unlock_ptbl(ptbl, mtx);
			continue;
		}

		/*
		 * XXX Need to do better ...
		 */
		(void) srmmu_ptesync(ptbl, pte, hatflag, NO_MLIST_LOCK);

		unlock_ptbl(ptbl, mtx);
	}
}

void
mmu_setctx(ctx)
	struct ctx *ctx;
{
	extern void flush_user_windows(void);

	/*
	 * We must make sure that there are no user windows
	 * in the register file when we switch contexts.
	 * Otherwise the flushed windows will go to the
	 * wrong place.
	 */
	flush_user_windows();
	mmu_setctxreg(ctx - ctxs);
}

#define	NUSRL1PTEPERPT		(MMU_L1_INDEX(KERNELBASE))

/*
 * Free all the translation resources for the specified L1.
 * A context-wide flush has been commited by the caller. The
 * context[] entry for this L1 was also cleared. So there is
 * no need for any more TLB flushes. This is called from two
 * places: srmmu_free() and srmmu_swapout().
 */
static void
srmmu_l1inval(as, pl1tbl, from_swapout)
	register struct as *as;
	register ptbl_t *pl1tbl;
	int from_swapout;
{
	union ptpe *l1ptr, l1tmp;
	caddr_t addr;
	int i;
	ptbl_t *l2ptbl;
	int ret, tmpptbl, flags;
	kmutex_t *l2mtx;

	ASSERT(as != &kas);

	/*
	 * Release all page tables owned by the address space.
	 */
	l1ptr = (union ptpe *)ptbltopt_va(pl1tbl);
	tmpptbl = pl1tbl->ptbl_flags & PTBL_TMP;

	if (!tmpptbl) {
		flags = SR_NOFLUSH;
	} else {
		flags = SR_TMP_TREE | SR_NOFLUSH;
	}

	addr = 0;
	for (i = 0; i < NUSRL1PTEPERPT; i++, addr += MMU_L1_SIZE, l1ptr++) {

		l1tmp = *l1ptr;

		switch (l1tmp.ptp.EntryType) {
		case MMU_ET_INVALID:
			break;

		case MMU_ET_PTP:
			l2ptbl = tatoptbl(l1tmp.ptp.PageTablePointer);
			if ((!from_swapout) ||
				    !(l2ptbl->ptbl_flags & PTBL_SHARED)) {
				MOD_VALID_PTP(flags, &(l1ptr->ptp),
				    MMU_ET_INVALID, addr, 1, as->a_hat, 0);
				ret = srmmu_l2inval(as, (caddr_t)addr,
				    flags, l2ptbl, &l2mtx);
				ASSERT((l2ptbl->ptbl_flags & PTBL_KEEP)
				    == 0);
				if (ret == LK_PTBL_OK) {
				    srmmu_ptbl_free(l2ptbl, l2mtx);
				}
			}
			break;

		case MMU_ET_PTE:
			{
			kmutex_t *mtx;
			ptbl_t *curp = VA2L1PTBL(pl1tbl, addr);

			ASSERT(tmpptbl == 0);

			(void) lock_ptbl(curp, flags|tmpptbl, as,
					(caddr_t)addr, 1, &mtx);
			(void) srmmu_pteunload(pl1tbl, (struct pte *)l1ptr,
				flags, NO_MLIST_LOCK);
			unlock_ptbl(curp, mtx);
			}
			break;

		default:
			cmn_err(CE_PANIC, "bad l1 entry at %x",
				l1tmp.ptpe_int);
		}
	}

	if (tmpptbl) {
#if defined(SRMMU_TMPUNLOAD)
		astosrmmu(as)->s_tmpl1ptbl = NULL;
		srmmu_l1_ptbl_free(pl1tbl);
#else defined(SRMMU_TMPUNLOAD)
		cmn_err(CE_PANIC, "no tmp tree allowed");
#endif
	}
}

/*
 * Destroy a level 2 page table.  Invalidate the l1 ptp for the l2 table.
 */
/*ARGSUSED*/
static int
srmmu_l2inval(as, addr, flags, l2ptbl, pmtx)
	struct as *as;
	caddr_t addr;
	int flags;
	ptbl_t *l2ptbl;
	kmutex_t **pmtx;
{
	struct ptp *ptp, tptp;
	struct pte *pte;
	ptbl_t *l3ptbl;
	kmutex_t *l3mtx, *l2mtx;
	int i;
	int ret, retl3;
	int lkflag;
	page_t *rpp[NL3PTEPERPT];
	caddr_t va;

	ASSERT(PTBL_LEVEL(l2ptbl->ptbl_flags) == 2);
	ASSERT(PTBL_LEVEL(l2ptbl->ptbl_parent->ptbl_flags) == 1);

	lkflag = (flags & SR_TMP_TREE)? LK_PTBL_TMP : 0;

	ret = lock_ptbl(l2ptbl, lkflag | LK_PTBL_FAILOK, as, addr, 2, &l2mtx);
	if (ret == LK_PTBL_FAIL_SHARED) {
		return (ret);
	}

	if (ret != LK_PTBL_OK) {
		cmn_err(CE_PANIC, "failed to lock l2!");
	}

	ASSERT(l2ptbl->ptbl_as == as);
	ASSERT(l2ptbl->ptbl_parent->ptbl_as == as);

	pte = ptbltopt_va(l2ptbl);
	va = BASE2VA(l2ptbl);
	for (i = 0, ptp = (struct ptp *)pte; i < NL2PTEPERPT;
	    i++, ptp++, va += MMU_L2_SIZE) {
		mmu_readptp(ptp, &tptp);
		switch (tptp.EntryType) {
		case MMU_ET_PTE:
			(void) srmmu_pteunload(l2ptbl, (struct pte *)ptp,
			    flags, NO_MLIST_LOCK);
			break;

		case MMU_ET_INVALID:
			break;

		case MMU_ET_PTP:
			l3ptbl = tatoptbl(tptp.PageTablePointer);
			MOD_VALID_PTP(flags, ptp, MMU_ET_INVALID, va, 2,
			    as->a_hat, 0);
			retl3 = srmmu_l3inval(as, addr, flags, l3ptbl, &l3mtx,
			    rpp);
			if (retl3 == LK_PTBL_OK) {
				if (FREE_UP_PTBL(l3ptbl->ptbl_flags)) {
					ASSERT(PTBL_VALIDCNT(
					    l2ptbl->ptbl_validcnt));
					l2ptbl->ptbl_validcnt--;
					srmmu_ptbl_free(l3ptbl, l3mtx);
				} else {
					/* restore L2 ptp ptr */
					SET_NEW_PTP(ptp, *(int *)(&tptp),
					    va, 2, 0);
					unlock_ptbl(l3ptbl, l3mtx);
				}
#ifdef VAC
				if (cache & CACHE_VAC) {
					srmmu_unload_vacpgs(rpp);
				}
#endif /* VAC */

			} else {
				ASSERT(PTBL_VALIDCNT(l2ptbl->ptbl_validcnt));
				l2ptbl->ptbl_validcnt--;
			}
			break;

		default:
			cmn_err(CE_PANIC, "srmmu_l2inval - bad l2");
		}

		if (l2ptbl->ptbl_validcnt == 0) {
			break;
		}
		addr += MMU_L2_SIZE;
	}

	ASSERT(PTBL_IS_LOCKED(l2ptbl->ptbl_flags));
	ASSERT(!FREE_UP_PTBL(l2ptbl->ptbl_flags) ||
	    ((l2ptbl->ptbl_validcnt == 0) && (l2ptbl->ptbl_lockcnt == 0)));

	*pmtx = l2mtx;

	return (ret);
}

/*
 * Destroy a level 3 page table.  Invalidate the l2 ptp for the l3 table.
 */
static int
srmmu_l3inval(as, addr, flags, l3ptbl, pmtx, rpp)
	struct as *as;
	caddr_t addr;
	int flags;
	ptbl_t *l3ptbl;
	kmutex_t **pmtx;
	page_t *rpp[];
{
	int ret;
	kmutex_t *mtx;
	int lkflag;

	ASSERT(PTBL_IS_LOCKED(l3ptbl->ptbl_parent->ptbl_flags));
	ASSERT(PTBL_LEVEL(l3ptbl->ptbl_parent->ptbl_flags) == 2);
	ASSERT(l3ptbl->ptbl_parent->ptbl_as == as);
	ASSERT(l3ptbl->ptbl_parent->ptbl_base == VA2BASE(MMU_L1_BASE(addr)));

	lkflag = (flags & SR_TMP_TREE)? LK_PTBL_TMP : 0;
	ret = lock_ptbl(l3ptbl, lkflag | LK_PTBL_FAILOK, as, addr, 3, &mtx);
	if (ret == LK_PTBL_FAIL_SHARED) {
		return (ret);
	}

	if (ret != LK_PTBL_OK) {
		cmn_err(CE_PANIC, "fail to lock l3");
	}

	ASSERT(PTBL_LEVEL(l3ptbl->ptbl_flags) == 3);

	*pmtx = mtx;
	(void) srmmu_unload_loop(l3ptbl, 0, NL3PTEPERPT, flags, rpp);

	ASSERT(PTBL_IS_LOCKED(l3ptbl->ptbl_flags));
	ASSERT(!FREE_UP_PTBL(l3ptbl->ptbl_flags) ||
	    ((l3ptbl->ptbl_validcnt == 0) && (l3ptbl->ptbl_lockcnt == 0)));

	return (ret);
}

/*
 * Locking primitves accessed by HATLOCK macros
 */

/* ARGSUSED */
static void
srmmu_page_enter(pp)
	struct page *pp;
{
	kmutex_t	*spl;

	spl = SPL_HASH(pp);
	mutex_enter(spl);
}

/* ARGSUSED */
static void
srmmu_page_exit(pp)
	struct page *pp;
{
	kmutex_t	*spl;

	spl = SPL_HASH(pp);
	mutex_exit(spl);
}

/*
 * Srmmu internal version of mlist enter/exit.
 */
static kmutex_t *
srmmu_mlist_enter(pp)
	struct page *pp;
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	mutex_enter(mml);

	return (mml);
}

static void
srmmu_mlist_exit(kmutex_t *mml)
{
	mutex_exit(mml);
}

int
srmmu_share(as, addr, sptas, sptaddr, size)
	struct as	*as, *sptas;
	caddr_t		addr, sptaddr;
	u_int		size;
{
	union  ptpe	ptpe;
	struct ptp	*l1ptpa, *l1ptps;
	struct ptp	tl1ptpa, tl1ptps;
	int		i;
	srmmu_t		*srmmus, *srmmua;
	ptbl_t		*l1ptbls, *l1ptbla, *l1ptbl, *l2ptbl;
	kmutex_t	*mtx, *l2mtx;
#ifdef VAC
	caddr_t		saddr;
#endif

	/*
	 * Check the alignment for L1PT (16 MB)
	 */
	if (((int)addr & L2PTOFFSET) || ((int)sptaddr & L2PTOFFSET)) {
		return (EINVAL);
	}

#ifdef VAC
	saddr = addr;	/* save seg address for segspt_shmattach call */
#endif

	/*
	 * Currently assume that HAT of sptas is not changed
	 * in the middle of L1PT copying.
	 * If this is not true, we need to use two step copying.
	 */
	srmmus = astosrmmu(sptas);
	srmmua = astosrmmu(as);

	l1ptbls = srmmus->s_l1ptbl;
	l1ptbla = srmmua->s_l1ptbl;

	/*
	 * since both l1ptbl's could hash to the same lock, we
	 * need to be careful about trying to get the same lock
	 * twice.
	 */
	l1ptbl = VA2L1PTBL(l1ptbla, addr);
	(void) lock_ptbl(l1ptbl, 0, as, addr, 1, &mtx);
	l1ptps = (struct ptp *)ptbltopt_va(l1ptbls) + MMU_L1_INDEX(sptaddr);
	l1ptpa = (struct ptp *)ptbltopt_va(l1ptbla) + MMU_L1_INDEX(addr);

	for (i = 0; i < ((size + L2PTOFFSET) >> MMU_STD_RGNSHIFT);
	    l1ptps++, i++, l1ptpa++, addr += MMU_L1_SIZE) {

		ptbl_t *nl1ptbl;

		nl1ptbl = VA2L1PTBL(srmmua->s_l1ptbl, addr);
		if (nl1ptbl != l1ptbl) {
			unlock_ptbl(l1ptbl, mtx);
			(void) lock_ptbl(nl1ptbl, 0, as, addr, 1, &mtx);
			l1ptbl = nl1ptbl;
		}

		mmu_readptp(l1ptps, &tl1ptps);
		if (tl1ptps.EntryType == MMU_ET_INVALID) {
			panic("invalid shared memory l1 ptp");
		}

		mmu_readptp(l1ptpa, &tl1ptpa);
		if (tl1ptpa.EntryType != MMU_ET_INVALID) {
			if (tl1ptpa.EntryType != tl1ptps.EntryType ||
			    tl1ptpa.PageTablePointer !=
			    tl1ptps.PageTablePointer) {
				l2ptbl = tatoptbl(tl1ptpa.PageTablePointer);
			(void) lock_ptbl(l2ptbl, 0, as, addr, 2, &l2mtx);
				if (l2ptbl->ptbl_validcnt == 0)
					srmmu_ptbl_free(l2ptbl, l2mtx);
				else
				panic("already allocated shared memory l1 ptp");
			} else {
				continue;
			}
		}

		ptpe.ptp.PageTablePointer = tl1ptps.PageTablePointer;
		ptpe.ptp.EntryType = MMU_ET_PTP;

		SET_NEW_PTP(l1ptpa, ptpe.ptpe_int, addr, 1, 0);
	}

	unlock_ptbl(l1ptbl, mtx);

#ifdef VAC
	if (cache & CACHE_VAC) {
		struct seg *seg;

		seg = as_segat(as, saddr);
		spt_addsptseg(seg, sptas);
	}
#endif /* VAC */
	srmmua->s_rss +=  btop(size);

	/*
	 * Make sure the context is set up for the address space.
	 */
	srmmu_install(as);

	return (0);
}

void
srmmu_unshare(as, addr, size)
	struct as	*as;
	caddr_t		addr;
	u_int		size;
{
	int		i;
	struct ptp	*l1ptp;
	srmmu_t    *srmmua;
	ptbl_t *l1ptbla, *l1ptbl;
	kmutex_t	*mtx;

#ifdef VAC
	if (cache & CACHE_VAC) {
		struct seg *seg;
		struct sptshm_data *ssd;

		seg = as_segat(as, addr);
		ssd = (struct sptshm_data *)seg->s_data;
		spt_delsptseg(seg, ssd->sptas);
	}
#endif /* VAC */

	mutex_enter(&as->a_hat->hat_mutex);
	srmmua = astosrmmu(as);
	if (srmmua == NULL) {
		/*
		 * as_free() is already done.
		 */
		mutex_exit(&as->a_hat->hat_mutex);
		return;
	}
	l1ptbla = srmmua->s_l1ptbl;
	mutex_exit(&as->a_hat->hat_mutex);

	l1ptp = (struct ptp *)ptbltopt_va(l1ptbla) + MMU_L1_INDEX(addr);

	l1ptbl = VA2L1PTBL(l1ptbla, addr);
	(void) lock_ptbl(l1ptbl, 0, as, addr, 1, &mtx);

	for (i = 0; i < ((size + L2PTOFFSET) >> MMU_STD_RGNSHIFT);
	    addr += MMU_L1_SIZE, i++, l1ptp++) {
		ptbl_t *nl1ptbl;

		nl1ptbl = VA2L1PTBL(srmmua->s_l1ptbl, addr);
		if (nl1ptbl != l1ptbl) {
			unlock_ptbl(l1ptbl, mtx);
			(void) lock_ptbl(nl1ptbl, 0, as, addr, 1, &mtx);
			l1ptbl = nl1ptbl;
		}

		mmu_writeptp(l1ptp, MMU_STD_INVALIDPTP, addr + (i << L1PTSHIFT),
					1, as->a_hat, 0);
	}

	unlock_ptbl(l1ptbl, mtx);
	srmmua->s_rss -=  btop(size);
}

#if defined(SRMMU_TMPUNLOAD)

/*
 * Copy all of the entries from the real L2 to the fake L2.
 */
static void
srmmu_tmpmergel2(ptbl_t *fptbl, ptbl_t *rptbl, caddr_t va)
{
	int i;
	struct ptp *rptp, *fptp;
	struct ptp rtmp, ftmp;

	ASSERT(PTBL_IS_LOCKED(rptbl->ptbl_flags));
	ASSERT(PTBL_IS_LOCKED(fptbl->ptbl_flags));

	fptp = (struct ptp *)ptbltopt_va(fptbl);
	rptp = (struct ptp *)ptbltopt_va(rptbl);

	for (i = 0; i < NL2PTEPERPT;
	    i++, fptp++, rptp++, va += MMU_L2_SIZE) {
		mmu_readptp(rptp, &rtmp);

		switch (rtmp.EntryType) {
		case MMU_ET_INVALID:
			break;

		case MMU_ET_PTE:
		case MMU_ET_PTP:
			mmu_readptp(fptp, &ftmp);
			if (ftmp.EntryType != MMU_ET_INVALID) {
				cmn_err(CE_PANIC, "srmmu_tmpmergel2 - "
				    "conflicting table entries");
				/*NOTREACHED*/
			}

			/* Copy the entry into existing fake L2. */
			SET_TNEW_PTP(fptp, *(u_int *)&rptp, va, 2, 0);

			/* Clear entry in the real L2 */
			/*LINTED*/
			MOD_VALID_PTP(SR_NOFLUSH, rptp, MMU_ET_INVALID,
				va, 2, rptbl->ptbl_as->a_hat, 0);
			ASSERT(rptbl->ptbl_validcnt > 0);
			rptbl->ptbl_validcnt--;
			break;

		default:
			cmn_err(CE_PANIC, "srmmu_tmpmergel2 - "
			    "unexpected table entry");
			/*NOTREACHED*/
		}
	}
}
#endif /* SRMMU_TMPUNLOAD */

static u_int
get_static_ctx(u_int nwanted)
{
	u_int n;

	ASSERT(static_ctx);

	mutex_enter(&srmmu_ctx_mtx);
	n = cur_avl_ctx;
	cur_avl_ctx += nwanted;
	mutex_exit(&srmmu_ctx_mtx);

	if (cur_avl_ctx > nctxs)
		panic("not enough contexts");

	return (n);
}

static srmmu_t *
srmmu_srmmu_alloc()
{
	srmmu_t	*srmmu = NULL;

	/*
	 * look for one with an L1 first.
	 */
	mutex_enter(&free_l1srmmu_lock);
	if (free_l1srmmu) {
		srmmu = free_l1srmmu;
		free_l1srmmu = srmmu->su_next;
	}
	mutex_exit(&free_l1srmmu_lock);

	if (srmmu == NULL) {
		/*
		 * No used ones, time to allocate one.
		 */
		srmmu = kmem_cache_alloc(srmmu_cache, KM_SLEEP);
		if (static_ctx) {
			srmmu->s_ctx = get_static_ctx(1);
		} else {
			srmmu->s_ctx = -1;
		}
		srmmu->s_l1ptbl = srmmu_l1_ptbl_alloc();
		srmmu->srmmu_root = (u_int)(ptbltopt_ptp(srmmu->s_l1ptbl));
#if defined(SRMMU_TMPUNLOAD)
		srmmu->s_tmpl1ptbl = NULL;
#endif defined(SRMMU_TMPUNLOAD)
	}

	srmmu->s_l3ptbl = NULL;
	srmmu->s_addr = (caddr_t)-1;
	srmmu->s_unused = 0x25;
	srmmu->s_rmstat = 0;

	return (srmmu);
}

static void
srmmu_srmmu_free(srmmu_t *srmmu)
{
	ASSERT(srmmu != &ksrmmu);
	ASSERT(static_ctx ? srmmu->s_ctx > 0 : srmmu->s_ctx == -1);

#if defined(SRMMU_TMPUNLOAD)
	ASSERT(srmmu->s_tmpl1ptbl == NULL);
#endif defined(SRMMU_TMPUNLOAD)

	mutex_enter(&free_l1srmmu_lock);
	srmmu->su_next = free_l1srmmu;
	free_l1srmmu = srmmu;
	mutex_exit(&free_l1srmmu_lock);
}


#ifdef sun4m

static int sr_ialloc = 1;

/*
 * As jgj points out, the 4m tlb's don't always look
 * in the caches when they are doing the table walk.
 * The page we just got from kmem_cache_alloc() may or may
 * not be in the cache.  We have to both be sure it
 * never gets in the cache again, and gets out if it
 * happens to be in.
 */
static void
uncache_4m_ptes(caddr_t ppage)
{

	struct pte	*pte;
	struct pte	tmp_pte;
	int		level;
	ptbl_t *ptbl;
	page_t		*pp;
	kmutex_t 	*mtx;
	int		tmp;

	extern void   (*v_uncache_pt_page)();

	if (sr_ialloc == 0) {

		pte = srmmu_ptefind(&kas, ppage, &level, &ptbl, &mtx, 0);
		ASSERT(pte_valid(pte));
		ASSERT(level == 3);
		mmu_readpte(pte, &tmp_pte);
		tmp_pte.Cacheable = 0;	/* don't cache it again! */

		/*
		 * Mark pte as NON cacheable. On VAC machines this
		 * also flushes VAC.
		 */
		(void) mmu_writepte(pte, *(u_int *)&tmp_pte, ppage,
			3, kas.a_hat, PTE_RM_MASK);

		unlock_ptbl(ptbl, mtx);
	} else {
		tmp = mmu_probe(ppage, NULL);
		/*
		 * Mmu_probe() returns non-zero when it's a
		 * valid pte.
		 */
		ASSERT(tmp != 0);
		tmp_pte = *(struct pte *)&tmp;

		/*
		 * PROM always has it uncached.
		 */
		ASSERT(tmp_pte.Cacheable == 0);
	}

	pp = pfn2pp_hash(tmp_pte.PhysicalPageNumber);
	srmmu_page_enter(pp);
	PP_SETPNC(pp);	/* tell other users of this page */
	srmmu_page_exit(pp);

	/*
	 * No need to flush VAC again - mmu_writepte() above did it.
	 * For PAC machines, we need to make sure that no cache lines
	 * remained in cache.
	 */
	if (sr_ialloc == 0 && ((cache & CACHE_VAC) == 0)) {
		XCALL_PROLOG;
		(*v_uncache_pt_page)(ppage, tmp_pte.PhysicalPageNumber);
		XCALL_EPILOG;
	}

}
#endif sun4m

#define	PTBL_CAN_STEAL(flag, lcnt) \
	(((flag) & (PTBL_KEEP | PTBL_SHARED)) == 0 && \
		((flag) & PTBL_ALLOCED) && PTBL_LEVEL(flag) == 3 && \
		(lcnt) == 0)

#define	PTBL_MAY_STEAL(flag, lcnt) \
	(((flag) & (PTBL_KEEP | PTBL_LOCKED | PTBL_SHARED)) == 0 && \
		((flag) & PTBL_ALLOCED) && PTBL_LEVEL(flag) == 3 && \
		(lcnt) == 0)

/*
 * connect up the ptbl's with their pt's and the page_t that
 * goes along with all of this.
 */
static void
srmmu_fillin_ptbl_group(ptbl_gr_t *ptbl_gr, pt_gr_t *ptg, int level)
{
	page_t		*pp;
	extern struct vnode kvp;
	ptbl_t		*ptbl;
	int		i;

	pp = page_lookup(&kvp, (u_int)ptg, SE_SHARED);
	if (pp == NULL) {
		cmn_err(CE_PANIC, "srmmu: could not find page %p",
		    (void *)ptg);
	}

	/*
	 * Things in the kernel are attached to kvp with
	 * their virtual address equal to the offset.
	 */
	ASSERT(pp->p_offset == (u_int)ptg);
	page_io_lock(pp);
	pp->p_prev = (page_t *)ptbl_gr;
	pp->p_next = (page_t *)0xdeaddead;

#ifdef sun4m
	if (mxcc == 0 || use_table_walk == 0) {
		uncache_4m_ptes((caddr_t)ptg);
	}
#endif sun4m

	/*
	 * Make sure all entries in both ptbls and pts are cleared.
	 */
	bzero((caddr_t)ptg, MMU_PAGESIZE);
	bzero((caddr_t)ptbl_gr, sizeof (ptbl_gr_t));

	ptbl_gr->pg_pt_va = (pt_t *)ptg;
	ptbl_gr->pg_pt_pfn = mach_pp->p_pagenum;
	ptbl_gr->pg_page = pp;
	ptbl_gr->pg_next = NULL;

	ptbl = ptbl_gr->pg_ptbl;
	for (i = 0; i < PTBL_GROUP; i++) {
		ptbl->ptbl_index = (u_char)i;
		ptbl->ptbl_next = ptbl+1;
		ptbl++;
	}
	ptbl--;
	if (level == PTBL_LEVEL_1) {
		ptbl = ptbl_gr->pg_ptbl;
		for (i = 0; i < PTBL_GROUP; i++) {
			ptbl->ptbl_next = (ptbl_t *)0xdeaddead;
			ptbl++;
		}
		ptbl = ptbl_gr->pg_ptbl;
		for (i = 0; i < PTBL_GROUP; i += 4) {
			ptbl->ptbl_next = ptbl+4;
			ptbl += 4;
		}
		ptbl -= 4;
	}
	ptbl->ptbl_next = NULL;
}

/*
 * Convert a given page into pt's.
 *
 * The caller supplies the page for the pt's.  We try
 * to provide the ptbl's.  This makes startup() possible.
 */
static ptbl_t *
srmmu_create_pts(caddr_t start, int level, int sleep, int keep)
{
	ptbl_gr_t	*ptbl_gr;
	ptbl_t		*ptbl;
	ptbl_t		*first;

	ptbl_gr = kmem_cache_alloc(srptbl_cache, sleep);
	if (ptbl_gr == NULL) {
		return (NULL);
	}

	srmmu_fillin_ptbl_group(ptbl_gr, (pt_gr_t *)start, level);

	first = ptbl_gr->pg_ptbl;
	if (keep) {
		ptbl = first;
		first = first->ptbl_next;
	} else {
		ptbl = NULL;
	}

	if (level == PTBL_LEVEL_1) {
		mutex_enter(&free_l1_ptbl_lock);
		total_ptbl_1 += PTBL_GROUP;
		free_ptbl_1 += PTBL_GROUP;
		if (keep) {
			free_ptbl_1--;
		}
		ptbl_gr->pg_ptbl[PTBL_GROUP-5].ptbl_next = free_l1_ptbl;
		free_l1_ptbl = first;
		mutex_exit(&free_l1_ptbl_lock);

		mutex_enter(&ptbl_gr_l1_pool_lock);
		ptbl_gr->pg_next = ptbl_gr_l1_pool;
		ptbl_gr_l1_pool = ptbl_gr;
		mutex_exit(&ptbl_gr_l1_pool_lock);
	} else {
		mutex_enter(&free_ptbl_lock);
		total_ptbl_23 += PTBL_GROUP;
		free_ptbl_23 += PTBL_GROUP;
		if (keep) {
			free_ptbl_23--;
		}
		ptbl_gr->pg_ptbl[PTBL_GROUP-1].ptbl_next = free_ptbl;
		free_ptbl = first;
		mutex_exit(&free_ptbl_lock);

		mutex_enter(&ptbl_gr_pool_lock);
		ptbl_gr->pg_next = ptbl_gr_pool;
		ptbl_gr_pool = ptbl_gr;
		mutex_exit(&ptbl_gr_pool_lock);
	}
	return (ptbl);
}

/*
 * Both of these are called at startup.
 * The starting address and the number of pages to use are
 * passed in.  The pages are converted into l1 ptbl/pt pairs.
 *
 * The first page of every 16 is for ptbls, the remaining
 * 15 pages are for the pts.  Left over ptbls are hung of
 * of a separate list.  Later, when more ptbls are needed,
 * this list is drained.
 */
void
ialloc_ptbl()
{
	int		n_pages;
	int		n_l3pts;
	int		i;
	ptbl_t		*ptbl;
	ptbl_t		*first_ptbl;
	kmutex_t	*mtx;

	srptbl_cache = kmem_cache_create("srptbl_cache", sizeof (ptbl_gr_t), 0,
	    NULL, NULL, NULL, NULL, NULL, 0);

	srpt_cache = kmem_cache_create("srpt_cache", MMU_PAGESIZE, MMU_PAGESIZE,
	    NULL, NULL, NULL, NULL, NULL, 0);

	first_ptbl = ptbl = srmmu_l1_ptbl_alloc();
	for (i = 0; i < 4; i++, ptbl++) {
		ptbl->ptbl_flags = PTBL_LEVEL_1;
	}
	srmmu_l1_ptbl_free(first_ptbl);

	/*
	 * yes, the 0 is correct.  It overflows!
	 */
	n_pages = MIN(physmem << 1, mmu_btop(0-KERNELBASE));
	n_l3pts = (n_pages/NL3PTEPERPT);

	/*
	 * Now make sure enough l3ptbls to cover all of
	 * the kernel exist.
	 */
	first_ptbl = NULL;
	for (i = 0; i < n_l3pts; i++) {
		if (first_ptbl == NULL) {
			first_ptbl = srmmu_ptbl_alloc(KM_SLEEP,
			    PTBL_KEEP, 3, &mtx);
			ptbl = first_ptbl;
		} else {
			ptbl->ptbl_next = srmmu_ptbl_alloc(KM_SLEEP,
			    PTBL_KEEP, 3, &mtx);
			ptbl = ptbl->ptbl_next;
		}
		if (ptbl == NULL) {
			cmn_err(CE_PANIC, "cannot allocate initial ptbls");
		}
		ptbl->ptbl_as = (struct as *)mtx;
		mutex_exit(mtx);
	}

	/* Terminate the list. */
	ptbl->ptbl_next = NULL;

	while (first_ptbl) {
		ptbl = first_ptbl;
		first_ptbl = first_ptbl->ptbl_next;
		mtx = (kmutex_t *)ptbl->ptbl_as;
		mutex_enter(mtx);
		srmmu_ptbl_free(ptbl, mtx);
	}

#ifdef sun4m
	sr_ialloc = 0;
#endif	/* sun4m */
}

/*
 * Allocate and set up a new lump of ptbl structures
 * and their associated pt structures if there is
 * space to be had.
 *
 * Be sure to get the pt's out of the cache on silly
 * machines. The page_t structure representing the
 * page of pt's is also used to move from physical
 * addresses to virtual.  The blocks of pages used
 * for ptbl/pt's are linked together via the p_next
 * field in the page_t.  The p_prev field is overlayed
 * and points to the ptbl structures. The p_iolock
 * is held to protect both p_next and p_prev.
 *
 * If we manage to allocate some, we return one to
 * the caller and put the rest on the ptbl free list.
 */
static ptbl_t *
srmmu_ptbl_create(u_int sleep)
{
	caddr_t		ptg;
	ptbl_t		*ptbl;

	ptg = (caddr_t)kmem_cache_alloc(srpt_cache, sleep);
	if (ptg == NULL) {
		return (NULL);
	}
	ptbl = srmmu_create_pts(ptg, PTBL_LEVEL_3, sleep, 1);
	if (ptbl == NULL) {
		kmem_cache_free(srpt_cache, ptg);
	}
	return (ptbl);
}

static int
srmmu_steal_this_ptbl(ptbl_t *ptbl, kmutex_t *p_mtx)
{
	ptbl_t		*l2ptbl;
	kmutex_t	*l2mtx, *uomtx;
	struct ptp	*l2ptp;
	caddr_t		addr;
	page_t		*rpp[NL3PTEPERPT];
	int		vcnt;

	l2ptbl = ptbl->ptbl_parent;
	addr = BASE2VA(ptbl);

	if (PTBL_CAN_STEAL(ptbl->ptbl_flags, ptbl->ptbl_lockcnt) &&
	    (lock_ptbl(l2ptbl, LK_PTBL_NOWAIT, ptbl->ptbl_as, addr, 2, &l2mtx)
	    == LK_PTBL_OK)) {
		/*
		 * Both the L3 and L2 are locked, this prevents srmmu_free
		 * from freeing up the srmmu/hat. We'll drop L2 lock
		 * below as soon as we get the L3 off the L2 ptbl. At that
		 * moment, hat structure could be freed by srmmu_free() and
		 * reused by other process. To keep the 'as' pointer in ptbl
		 * valid, we aquire hat_unload_other mutex to keep the
		 * process from exiting. When we finish unloading with L3
		 * ptbl it is no longer associated with any as/hat so we
		 * can safely drop the hat_unload_other lock.
		 */
		uomtx = &(l2ptbl->ptbl_as->a_hat->hat_unload_other);
		if (! mutex_tryenter(uomtx)) {
			unlock_ptbl(l2ptbl, l2mtx);
			return (0);
		}

		/*
		 * Unlink the L3 from the L2 and then unload the L3.
		 */
		l2ptp = (struct ptp *)ptbltopt_va(l2ptbl) + MMU_L2_INDEX(addr);
		mmu_writeptp(l2ptp, MMU_ET_INVALID, MMU_L2_BASE(addr), 2,
		    ptbl->ptbl_as->a_hat, 0);

		l2ptbl->ptbl_validcnt--;
		unlock_ptbl(l2ptbl, l2mtx);

		vcnt = srmmu_unload_loop(ptbl, 0, NL3PTEPERPT, SR_NOFLUSH,
		    rpp);
		ptbl->ptbl_flags = 0;
		mutex_exit(p_mtx);

		mutex_exit(uomtx);

#ifdef VAC
		if (vcnt && (cache & CACHE_VAC)) {
			srmmu_unload_vacpgs(rpp);
		}
#else	/* VAC */
#if defined(lint)
		vcnt = vcnt + 1;
#endif	/* lint */
#endif	/* VAC */


		vmhatstat.vh_ptblstolen.value.ul++;
		return (1);
	}

	return (0);
}

#ifdef DEBUG
static u_int srmmu_ptbl_steal_twice;
#endif

/*
 * Steal a ptbl.
 * Enough ptbls were allocated at startup such that
 * we should never ever not be able to find one.
 * If we can't find one, keep hunting, one will show up.
 *
 * The basic rules are:
 *	1. Don't steal kernel ptbl's (PTBL_KEEP = 1).
 *	2. Only steal level 3 ptbl's (PTBL_LEVEL = 3).
 *	3. Don't steal free ptbl's (PTBL_ALLOC = 0), take
 *		them off the free list.
 *
 * Start at the ptbl_steal_hand, go to the end, and then
 * start at the beginning. Each time through the loop, the
 * ptbl free list is checked. We don't have to be fast, we
 * just need to get one.
 */
static ptbl_t *
srmmu_ptbl_steal(u_int sleep, u_int alloc)
{
	u_int		i;
	ptbl_t		*ptbl;
	ptbl_gr_t	*ptbl_gr;
	kmutex_t	*p_mtx;
	u_int		examined;

	ptbl_gr = ptbl_gr_hand;
	if (ptbl_gr == NULL) {
		ASSERT(ptbl_gr_pool != NULL);
		ptbl_gr = ptbl_gr_pool;
	}
	examined = 0;

	for (;;) {
		while (ptbl_gr) {
			ptbl = ptbl_gr->pg_ptbl;
			for (i = 0; i < PTBL_GROUP; i++) {
				examined++;
#ifdef DEBUG
				if (ptbl == ptbl_last_stolen) {
					srmmu_ptbl_steal_twice++;
				}
#endif
				if ((PTBL_MAY_STEAL(ptbl->ptbl_flags,
				    ptbl->ptbl_lockcnt)) &&
				    (ptbl_last_stolen != ptbl) &&
				    (lock_this_ptbl(ptbl, LK_PTBL_NOWAIT,
				    &p_mtx) == LK_PTBL_OK)) {
					if (srmmu_steal_this_ptbl(ptbl,
					    p_mtx)) {

						ptbl_gr_hand = ptbl_gr->pg_next;
						ptbl_last_stolen = ptbl;

						/*
						 * triple break!
						 */
						goto stole_a_ptbl;
					}
					unlock_ptbl(ptbl, p_mtx);
				}
				ptbl++;
			}
			ptbl_gr = ptbl_gr->pg_next;
		}

		if (free_ptbl) {
			ptbl = NULL;
			mutex_enter(&free_ptbl_lock);
			if (free_ptbl) {
				ptbl = free_ptbl;
				free_ptbl = ptbl->ptbl_next;
				free_ptbl_23--;
			} else if (free_ptbl_23 != 0) {
				cmn_err(CE_PANIC, "ptbl accounting error");
			}
			mutex_exit(&free_ptbl_lock);
			if (ptbl) {
				break;
			}
		}

		if (!(alloc & NO_ALLOC)) {
			if (ptbl = srmmu_ptbl_create(KM_NOSLEEP)) {
				break;
			}
		}

		/*
		 * Give things a moment to change, then do it again.
		 */
		if (sleep == KM_SLEEP) {
			delay(hz/50);
		}
		ptbl_gr = ptbl_gr_pool;
	}

stole_a_ptbl:

	return (ptbl);
}

/*
 * Provide the caller with a locked ptbl of the correct type and level.
 *
 * It also returns a pointer to the mutex protecting the ptbl.
 * The caller must drop the mutex when done with the ptbl.
 */
static ptbl_t *
srmmu_ptbl_alloc(u_int sleep, u_int alloc, int level, kmutex_t **pmtx)
{
	u_char		flags;
	ptbl_t		*ptbl;

	/*
	 * The locking protocol for ptbls requires that
	 * we pull the ptbl from the free list, then
	 * set its tag.  Once it has a tag it is protected
	 * by one of the pools of mutexes.  We return
	 * a pointer to the ptbl and the mutex protecting
	 * the ptbl.
	 */
	ptbl = NULL;

	if (!(alloc & NO_ALLOC) &&
	    (total_ptbl_23 < max_ptbl) &&
	    (free_ptbl_23 < min_ptbl)) {
		ptbl = srmmu_ptbl_create(sleep);
	}

	if (ptbl == NULL) {
		mutex_enter(&free_ptbl_lock);
		ptbl = free_ptbl;
		if (ptbl != NULL) {
			ASSERT(ptbl->ptbl_flags == 0);
			free_ptbl = ptbl->ptbl_next;
			free_ptbl_23--;
		}
		mutex_exit(&free_ptbl_lock);
	}

	if (ptbl == NULL) {
		ptbl = srmmu_ptbl_steal(sleep, alloc);
	}

	/*
	 * We now have a tagless ptbl.
	 * The stealer will only steal
	 * ptbls with the alloc bit set.
	 * Figure out which ptbl lock will
	 * protect this ptbl, what the flags
	 * will be, get the lock,
	 * tag the ptbl, and return.
	 */
	*pmtx = fd_ptbl_mtx(level, ptbl,
	    (alloc & GET_TMP_PTBL) ? LK_PTBL_TMP : 0);
	flags = level | PTBL_ALLOCED | PTBL_LOCKED;
	if (alloc & GET_TMP_PTBL) {
		flags |= PTBL_TMP;
	}

	mutex_enter(*pmtx);

	ASSERT(ptbl->ptbl_flags == 0);
	ptbl->ptbl_flags = flags;
	return (ptbl);
}

/*
 * Put a locked ptbl on the free list
 */
void
srmmu_ptbl_free(ptbl_t *ptbl, kmutex_t *mtx)
{

	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));
	ASSERT(ptbl->ptbl_flags & PTBL_ALLOCED);

	ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == 2 ||
	    PTBL_LEVEL(ptbl->ptbl_flags) == 3);
	ASSERT((ptbl->ptbl_flags & PTBL_KEEP) == 0);
	ASSERT(ptbl->ptbl_validcnt == 0);
	ASSERT(ptbl->ptbl_lockcnt == 0);

	VRFY_PTBL_VCNT(ptbl);

#ifdef DEBUG
#ifdef sun4d
	if (PTBL_LEVEL(ptbl->ptbl_flags) != 0) {
		kmutex_t	*tmtx;

		tmtx = fd_ptbl_mtx(PTBL_LEVEL(ptbl->ptbl_flags), ptbl, 0);
		ASSERT(tmtx == mtx);
	}
#endif sun4d
#endif /* DEBUG */

	/*
	 * Unlock the ptbl by hand here.
	 * All the asserts in unlock_ptbl() have been confirmed.
	 * If unlock_ptbl() is called, then just the locked bit
	 * is turned off and the mutex is dropped.  This allows
	 * lock_this_ptbl() to think that it has the correct
	 * mutex because the level and tmp_flag may still be set.
	 * A moment later, when unlock_ptbl() returns and the level
	 * and the other flags are
	 * cleared, lock_this_ptbl() will return to its caller
	 * convinced it has done the right thing.
	 *
	 * For the moment this ptbl is dangling (not on any lists)
	 * the stealer will not take it because the PTBL_ALLOCED
	 * bit is off.
	 */
	ptbl->ptbl_flags = 0;
	mutex_exit(mtx);

	mutex_enter(&free_ptbl_lock);
	ptbl->ptbl_next = free_ptbl;
	free_ptbl = ptbl;
	free_ptbl_23++;
	mutex_exit(&free_ptbl_lock);
}

/*
 * Allocate a new l1 ptbl.
 * These are needed when a process gets off the ground
 * or adds a tmp l1 ptbl while doing some fancy graphics.
 *
 * This is basically the same as srmmu_ptbl_alloc() except for the
 * different pools and no stealing.  Almost all of the
 * routines between here and fork use kmem_*(* , KM_SLEEP).
 * We don't want to be left out. The returned ptbl is not locked.
 */
static ptbl_t *
srmmu_l1_ptbl_alloc()
{
	ptbl_t		*ptbl;
	caddr_t		ptg;

	ptbl = NULL;
	mutex_enter(&free_l1_ptbl_lock);
	if (free_l1_ptbl) {
		ptbl = free_l1_ptbl;
		free_l1_ptbl = ptbl->ptbl_next;
		free_ptbl_1--;
	}
	mutex_exit(&free_l1_ptbl_lock);

	if (ptbl == NULL) {
		ptg = (caddr_t)kmem_cache_alloc(srpt_cache, KM_SLEEP);
		ptbl = srmmu_create_pts(ptg, PTBL_LEVEL_1, KM_SLEEP, 1);
	}
	if (ptbl == NULL) {
		cmn_err(CE_PANIC, "srmmu_create_pts failed with KM_SLEEP");
	}

	return (ptbl);
}

static void
srmmu_l1_ptbl_free(ptbl_t *ptbl)
{
	int i;
	ptbl_t *tmp;

	ASSERT(ptbl->ptbl_lockcnt == 0);
	ASSERT(ptbl->ptbl_validcnt == 0);

	/*
	 * clear out all flags.
	 */
	tmp = ptbl;
	for (i = 0; i < 4; i++, tmp++) {
		ASSERT(PTBL_LEVEL(tmp->ptbl_flags) == 1);
		ASSERT((tmp->ptbl_flags & PTBL_KEEP) == 0);
		tmp->ptbl_flags = 0;
	}

	mutex_enter(&free_l1_ptbl_lock);
	free_ptbl_1++;
	ptbl->ptbl_next = free_l1_ptbl;
	free_l1_ptbl = ptbl;
	mutex_exit(&free_l1_ptbl_lock);
}

#define	HME_TRIGGER	(NCPU*10)
static hment_pool_t	*hment_steal_pool;

static struct srhment *
get_hment()
{
	srhment_t	*hme;
	u_int		i;
	hment_pool_t	*hp;
	hment_pool_t	*steal;

	hp = &hment_pool[CPU->cpu_id];
	mutex_enter(&hp->hp_mtx);
	if ((hme = hp->hp_list) != NULL) {
		hp->hp_list = (srhment_t *)hme->ghme.hme_next;
		hp->hp_count--;
		mutex_exit(&hp->hp_mtx);
		ASSERT(hme->ghme.hme_page == (page_t *)0xdefec8ed);
		*hme = nullhme;
		return (hme);
	}
	hp->hp_empty_count++;
	mutex_exit(&hp->hp_mtx);

	steal = hment_steal_pool;
	if (steal) {
		for (i = 0; i < NCPU; i++) {
			if (steal->hp_count > HME_TRIGGER) {
				mutex_enter(&steal->hp_mtx);
				if ((hme = steal->hp_list) != NULL) {
					ASSERT(hme->ghme.hme_page ==
					    (page_t *)0xdefec8ed);
					steal->hp_list =
					    (srhment_t *)hme->ghme.hme_next;
					ASSERT(steal->hp_list ?
					    (steal->hp_list->ghme.hme_page ==
					    (page_t *)0xdefec8ed) : 1);
					steal->hp_count--;
				}
				mutex_exit(&steal->hp_mtx);
				if (hme) {
					hment_steal_pool = steal;
					hp->hp_stole++;
					break;
				}
			}
			steal++;
			if (steal >= &hment_pool[NCPU]) {
				steal = hment_pool;
			}
		}
	}

	if (hme == NULL) {
		hment_steal_pool = NULL;
		hme = kmem_cache_alloc(srhme_cache, KM_SLEEP);
	}

	*hme = nullhme;
	return (hme);
}


static void
put_hment(struct srhment *hme)
{
	hment_pool_t	*hp;

	ASSERT(hme->ghme.hme_valid == 0);
	ASSERT(hme->ghme.hme_page == (page_t *)0xdefec8ed);

	hp = &hment_pool[CPU->cpu_id];
	mutex_enter(&hp->hp_mtx);
	ASSERT(hp->hp_list ?
	    (hp->hp_list->ghme.hme_page == (page_t *)0xdefec8ed) : 1);
	hme->ghme.hme_next = (struct hment *)hp->hp_list;
	hp->hp_list = hme;
	hp->hp_count++;
	mutex_exit(&hp->hp_mtx);

	if (hp->hp_count > HME_TRIGGER) {
		if (hp->hp_count > hp->hp_max_count) {
			hp->hp_max_count = hp->hp_count;
		}
		hment_steal_pool = hp;
	}
}

void
unlock_ptbl(ptbl_t *ptbl, kmutex_t *mtx)
{

#ifdef DEBUG
#ifdef sun4d
{
	int level;

	level = PTBL_LEVEL(ptbl->ptbl_flags);

	if (level != 0) {
		ASSERT(fd_ptbl_mtx(level, ptbl, 0) == mtx);
	}
}
#endif sun4d
#endif /* DEBUG */

	ASSERT(ptbl->ptbl_flags & PTBL_LOCKED);
	VRFY_PTBL_VCNT(ptbl);

	ptbl->ptbl_flags &= ~PTBL_LOCKED;
	mutex_exit(mtx);
}

static kmutex_t *
fd_ptbl_mtx(int level, ptbl_t *ptbl, int flag)
{
	kmutex_t *plock;
	int i;

	if (!(flag & LK_PTBL_TMP)) {
		switch (level) {
		case 3:
			i = L3PTBL_MTX_HASH(ptbl);
			ASSERT(i >= 0 && i < N_L3PTBL_MTX);
			plock = &l3ptbl_lock[i];
			break;

		case 2:
			i = L2PTBL_MTX_HASH(ptbl);
			ASSERT(i >= 0 && i < N_L2PTBL_MTX);
			plock = &l2ptbl_lock[i];
			break;

		case 1:
			i = L1PTBL_MTX_HASH(ptbl);
			ASSERT(i >= 0 && i < N_L1PTBL_MTX);
			plock = &l1ptbl_lock[i];
			break;

		default:
			cmn_err(CE_PANIC, "bad level");
		}
	} else {
#if defined(SRMMU_TMPUNLOAD)
		switch (level) {
		case 3:
			i = TMPL3PTBL_MTX_HASH(ptbl);
			ASSERT(i >= 0 && i < N_TMPL3PTBL_MTX);
			plock = &tmpl3ptbl_lock[i];
			break;

		case 2:
			i = TMPL2PTBL_MTX_HASH(ptbl);
			ASSERT(i >= 0 && i < N_TMPL2PTBL_MTX);
			plock = &tmpl2ptbl_lock[i];
			break;

		case 1:
			i = TMPL1PTBL_MTX_HASH(ptbl);
			ASSERT(i >= 0 && i < N_TMPL1PTBL_MTX);
			plock = &tmpl1ptbl_lock[i];
			break;

		default:
			cmn_err(CE_PANIC, "bad level");
		}
#else
		cmn_err(CE_PANIC, "no LK_PTBL_TMP");
#endif
	}

	return (plock);
}

/*
 * There are times when caller does not know if the ptbl
 * is a TMP ptbl or not. Or a level three for that matter.
 *
 * This happens whenever we get to
 * a ptbl by a p_mapping list, or the L3 stealer. Since
 * srmmu_pageunload/srmmu_pagesync are only called to
 * deal with L3 pages and the L3 stealer only deals with L3
 * ptbls, level is hardcoded to 3.
 */
static int
lock_this_ptbl(ptbl, flag, pmtx)
	ptbl_t *ptbl;
	kmutex_t **pmtx;
	int flag;
{
	int		locked;
	kmutex_t	*mtx;
	int		ontmp;

top:
	ontmp = ptbl->ptbl_flags & PTBL_TMP;
	mtx = fd_ptbl_mtx(3, ptbl, (ontmp) ? LK_PTBL_TMP : 0);

	if ((flag & LK_PTBL_NOWAIT) == 0) {
		mutex_enter(mtx);
		locked = 1;
	} else if (mutex_tryenter(mtx)) {
		locked = 1;
	} else {
		locked = 0;
	}

	if (locked) {
		if (PTBL_LEVEL(ptbl->ptbl_flags) != 3) {
			mutex_exit(mtx);
			return (LK_PTBL_FAILED);
		}

		if ((ptbl->ptbl_flags & PTBL_TMP) == ontmp) {
			ASSERT((ptbl->ptbl_flags & PTBL_LOCKED) == 0);
			ptbl->ptbl_flags |= PTBL_LOCKED;
			*pmtx = mtx;

			VRFY_PTBL_VCNT(ptbl);
			return (LK_PTBL_OK);
		}
		mutex_exit(mtx);
		goto top;
	}
	return (LK_PTBL_FAILED);
}

int
lock_ptbl(ptbl, flag, pas, va, level, pmtx)
	ptbl_t *ptbl;
	u_int flag;
	struct as *pas;
	caddr_t va;
	int level;
	kmutex_t **pmtx;
{
	kmutex_t *mtx;
	int ret;

	ASSERT(((u_int) va < KERNELBASE) || pas == &kas);
	ASSERT((flag & (LK_PTBL_NOWAIT | LK_PTBL_FAILOK)) !=
	    (LK_PTBL_NOWAIT | LK_PTBL_FAILOK));

	ret = LK_PTBL_MISMATCH;
	mtx = fd_ptbl_mtx(level, ptbl, flag);

	if ((flag & LK_PTBL_NOWAIT) == 0) {
		mutex_enter(mtx);
	} else {
		if (mutex_tryenter(mtx) == 0) {
			return (LK_PTBL_FAILED);
		}
	}

#if defined(SRMMU_TMPUNLOAD)
	if (! (flag & LK_PTBL_TMP)) {
		if (ptbl->ptbl_flags & PTBL_TMP) {
			ret = LK_PTBL_MISMATCH;
			goto mismatch;
		}
	} else {
		if (! (ptbl->ptbl_flags & PTBL_TMP)) {
			ret = LK_PTBL_MISMATCH;
			goto mismatch;
		}
	}
#endif defined(SRMMU_TMPUNLOAD)

	if (PTBL_LEVEL(ptbl->ptbl_flags) != level) {
		ret = LK_PTBL_MISMATCH;
		goto mismatch;
	}

	/*
	 * We have the right mutex, now check to see if we
	 * have the ptbl the caller wants.
	 *
	 * Ptbl_as/ptbl_base cannot be verified
	 * if it's shared by many clients.
	 */
	if ((ptbl->ptbl_flags & PTBL_SHARED) &&
	    (flag & LK_PTBL_SHARED)) {
		/*
		 * We are going to trust the caller on this one.
		 */
		goto match;
	}

	switch (level) {
	case 3:
		va = (caddr_t)((u_int)va & MMU_L2_MASK);
		break;
	case 2:
		va = (caddr_t)((u_int)va & MMU_L1_MASK);
		break;
	case 1:
		va = (caddr_t)((u_int)va & ~(L1_VA_PER_PTBL - 1));
		break;
	}

	if ((ptbl->ptbl_as != pas) || (ptbl->ptbl_base != VA2BASE(va))) {
		if (!(ptbl->ptbl_flags & PTBL_SHARED)) {
			ret = LK_PTBL_MISMATCH;
		} else {
			/*
			 * Not owner of the shared ptbl.
			 */
			ret = LK_PTBL_FAIL_SHARED;
		}
		goto mismatch;
	}

	/*
	 * Finally, this is the ptbl the caller wants.
	 */
match:
	ASSERT((ptbl->ptbl_flags & PTBL_LOCKED) == 0);
	ptbl->ptbl_flags |= PTBL_LOCKED;

	VRFY_PTBL_VCNT(ptbl);

	*pmtx = mtx;
	return (LK_PTBL_OK);

mismatch:
	if (flag & LK_PTBL_FAILOK) {
		mutex_exit(mtx);
		return (ret);
	}
	cmn_err(CE_PANIC, "fail to lock: ptbl %p flag %x "
		"as %p va %p level %d mtx %p ret %d",
		(void *)ptbl, flag, (void *)pas, (void *)va, level,
		(void *)mtx, ret);
	/* NOTREACHED */
}

static void
hashin_ptetohme(struct pte *ppte, struct srhment *phme)
{
	int bin;
	kmutex_t *mtx;

	bin = PTE2HME_BIN(ppte);
	mtx = PTE2HME_BIN_MTX(bin);

	mutex_enter(mtx);
	phme->hme_hash = pte2hme_hash[bin];
	pte2hme_hash[bin] = phme;
	mutex_exit(mtx);
}

static struct srhment *
hashout_ptetohme(ptbl_t *ptbl, struct pte *ppte)
{
	int bin;
	kmutex_t *mtx;
	struct srhment **pphme;
	struct srhment *myhme;
	u_char		index;

	index = ((u_int)ppte & 0xff) >> 2;
	bin = PTE2HME_BIN(ppte);
	mtx = PTE2HME_BIN_MTX(bin);

	pphme = &pte2hme_hash[bin];
	mutex_enter(mtx);

	while (*pphme != NULL) {

		if (((*pphme)->hme_ptbl == ptbl) &&
		    ((*pphme)->hme_index == index)) {
			myhme = *pphme;
			*pphme = (*pphme)->hme_hash;

			mutex_exit(mtx);
			return (myhme);
		}
		pphme = &((*pphme)->hme_hash);
	}

	mutex_exit(mtx);
	return (NULL);
}

static caddr_t
ptetovaddr(ptbl_t *ptbl, struct pte *pte)
{
	int off, level;
	caddr_t taddr;

	off = ptetooff(pte);	/* Offset inside a ptbl. */

	level = PTBL_LEVEL(ptbl->ptbl_flags);
	ASSERT(level > 0);

	taddr = BASE2VA(ptbl);
	return (taddr + (srmmu_sizes[level] * off));
}

static struct srhment *
pte2hme(ptbl_t *ptbl, struct pte *pte)
{
	int bin;
	kmutex_t *mtx;
	struct srhment *phme;
	int locked = 0;
	u_char		index;

	mtx = NULL;
	bin = PTE2HME_BIN(pte);
	index = PTE2INDEX(pte);

again:
	phme = pte2hme_hash[bin];
	while (phme != NULL) {
		if ((phme->hme_ptbl == ptbl) &&
		    (phme->hme_index == index)) {
			if (locked) {
				mutex_exit(mtx);
			}
			return (phme);
		}
		phme = phme->hme_hash;
	}

	if (locked == 0) {
		mtx = PTE2HME_BIN_MTX(bin);
		mutex_enter(mtx);
		locked = 1;
		goto again;
	} else {
		mutex_exit(mtx);
		return (NULL);
	}
	/* NOTREACHED */
}

#ifdef	sun4m
static u_int
clr_sx_bits(u_int pfn)
{
	int bustype, x;

	/*
	 * Clear high bits for SX. This is safe to do since no
	 * real memory is at that high phyical address.
	 */

	bustype = pfn >> 20;
	switch (bustype) {
	case 0x8:
		pfn &= 0xfffff;
		break;

	case 0xf:
		x = pfn >> 16;
		if ((x == 0xf0) && (pfn != 0xf00000) && (pfn != 0xf00001)) {
			cmn_err(CE_PANIC, "bad pfn %x", pfn);
		}
		break;
	}

	return (pfn);
}
#endif	/* sun4m */

int
vrfy_is_mem(u_int pfn)
{
#ifdef sun4m
	pfn = clr_sx_bits(pfn);
	ASSERT((pfn & 0xf00000) != 0x800000);
#endif

	return (pf_is_memory(pfn));
}


/*
 * This routine does faster pfn to pp translation. This
 * operation becomes important because:
 * 	- for singly mapped pages (majority), there is no
 *	  hme. The old code used to find pp via pte2hme(), then
 *	  hme->hme_page. This path no longer exists.
 * 	- hme_page is deleted since the old code path does not
 *	  exist anymore.
 */
static page_t *
pfn2pp_hash(u_int pfn)
{
#ifdef sun4m
	pfn = clr_sx_bits(pfn);
#endif /* sun4m */

	if (!pf_is_memory(pfn)) {
		ASSERT(page_numtopp_nolock(pfn) == NULL);
		return (NULL);
	}

	return (page_numtopp_nolock(pfn));  /* this is fast now */
}

/*
 * This routine is called by SRMMU to handle the p_mapping list.
 */
static void
hme_may_sub(ptbl, pte, hme, pp)
	ptbl_t		*ptbl;
	struct pte	*pte;
	struct srhment *hme;
	page_t *pp;
{
	u_int p_mapping;

	ASSERT(ohat_mlist_held(pp));
	ASSERT(mach_pp->p_mapping);

	p_mapping = (u_int) mach_pp->p_mapping;
	if (p_mapping & PP_PTBL) {
		ASSERT(mach_pp->p_share == 1);
		ASSERT(pte == ptbltopt_va(PP2PTBL(mach_pp)) + mach_pp->p_index);
		ASSERT(hme == NULL);

		mach_pp->p_index = 0xfe;
		mach_pp->p_share = 0;
		mach_pp->p_mapping = NULL;
	} else {
		if (hme == NULL) {
			/*
			 * This means it had one mapping while at
			 * ptesync() time, but after ptesync() drops
			 * mlist lock, someone adds a new mapping to
			 * it. So we have to find our hment now.
			 */
			hme = hashout_ptetohme(ptbl, pte);
		}
		if (hme != NULL) {
			ASSERT(mach_pp->p_share >= 1);
			mach_pp->p_share--;

			hme_list_sub(hme, pp);
			put_hment(hme);
		}
	}
}

void
srmmu_convert_pmapping(page_t *pp)
{
	u_int t_map;
	kmutex_t *mtx;
	srhment_t *hme = NULL;

again:
	/*
	 * Have to allocate srhment outside of mlist lock.
	 */
	t_map = (u_int) mach_pp->p_mapping;
	if ((t_map & PP_PTBL) != 0) {
		hme = get_hment();
	}

	mtx = srmmu_mlist_enter(pp);
	t_map = (u_int) mach_pp->p_mapping;
	if ((t_map & PP_PTBL) != 0) {
		if (hme) {
			convert_one_mapping(pp, hme);
			/* Mark hme used. */
			hme = NULL;
		} else {
			srmmu_mlist_exit(mtx);
			goto again;
		}
	}
	srmmu_mlist_exit(mtx);

	if (hme) {
		/* we did not use it. */
		put_hment(hme);
	}
}

static void
convert_one_mapping(page_t *pp, srhment_t *hme)
{
	ptbl_t		*optbl;
	struct pte	*opte;
	struct hat	*srhat;
	u_int		t_map;

	ASSERT(PAGE_LOCKED(pp) || panicstr);
	ASSERT(ohat_mlist_held(pp));
	ASSERT(mach_pp->p_share == 1);

	t_map = (u_int) mach_pp->p_mapping;

	ASSERT((t_map & PP_PTBL) != 0);

	/*
	 * Covert the one and only existing srmmu
	 * mapping.
	 */
	if (t_map & PP_NOSYNC) {
		hme->ghme.hme_nosync = 1;
	}
	optbl = MAP2PTBL(t_map);
	hme->hme_ptbl = optbl;
	hme->hme_index = mach_pp->p_index;
	opte = ptbltopt_va(optbl) + mach_pp->p_index;

	srhat = optbl->ptbl_as->a_hat;

	hme->ghme.hme_hat = srhat - hats;
	hashin_ptetohme(opte, hme);

	mach_pp->p_mapping = NULL;
	mach_pp->p_index = 0xff;

	hme_list_add(hme, pp);
}

/*
 * SRMMU version to add a mapping to p_mapping. The first pte pointer
 * (pte) is pointing to the actual pte that we are going to write to,
 * the 2nd pte pointer (npte) is pointing to the 'template' pte that
 * contains the value of the pte that we will write to 'pte'.
 */
static int
hme_may_add(
	ptbl_t		*ptbl,
	pte_t		*pte,
	page_t		*pp,
	u_int		attr,
	struct hat	*myhat,
	pte_t		*npte,
	srhment_t	*myhme[])
{
	u_int		t_map;
	struct srhment	*hme;

	ASSERT(PAGE_LOCKED(pp) || panicstr);
	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));
	ASSERT(ohat_mlist_held(pp));

	t_map = (u_int) mach_pp->p_mapping;

	/*
	 * Add the new mapping.
	 */
	if (t_map == NULL) {
		/*
		 * we are the very first mapping.
		 */
		ASSERT(mach_pp->p_share == 0);
		t_map = (u_int)ptbl | PP_PTBL;
		if (attr & HAT_NOSYNC) {
			t_map |= PP_NOSYNC;
		}
		mach_pp->p_mapping = (void *)t_map;
		mach_pp->p_index = (u_char)((u_int)pte & 0xff) >> 2;
		mach_pp->p_share = 1;
		if (pte_ronly(npte) && !PP_ISMOD(pp)) {
			srmmu_page_enter(pp);
			if (!PP_ISMOD(pp)) {
				PP_SETRO(pp);
			}
			srmmu_page_exit(pp);
		}
	} else {
		/*
		 * We are here because there is already at least one
		 * mapping already exists.
		 */
		if (t_map & PP_PTBL) {
			hme = myhme[1];
			if (hme == NULL) {
				return (2);
			}

			ASSERT(myhme[0] != NULL);
			myhme[1] = NULL;
			convert_one_mapping(pp, hme);
		}

		hme = myhme[0];
		if (hme == NULL) {
			return (1);
		}
		myhme[0] = NULL;

		/*
		 * These fields probably mean nothing to other hats.
		 */
		hme->hme_ptbl = ptbl;
		hme->hme_index = ((u_int)pte & 0xff)>>2;
		hme->ghme.hme_hat = myhat - hats;
		hme->ghme.hme_nosync = (attr & HAT_NOSYNC) != 0;
		hashin_ptetohme(pte, hme);
		hme_list_add(hme, pp);

		/*
		 * Note, we don't do accounting inside hme_list_add()
		 * since a hme may be added due to a PP_PTBL to hme
		 * convertion.
		 */
		mach_pp->p_share++;
		ASSERT(mach_pp->p_share >= 2);
	}

	SET_NEW_PTE(pte, *(u_int *)npte, ptetovaddr(ptbl, pte),
	    PTBL_LEVEL(ptbl->ptbl_flags), 0);

	return (0);
}


static void
hme_list_add(hme, pp)
	register struct srhment *hme;
	register page_t *pp;
{
	struct hment *tmp;

	ASSERT(ohat_mlist_held(pp));
	ASSERT(hme->hme_ptbl != NULL);
	ASSERT(hme->hme_index != 0xff);
	ASSERT(hme->ghme.hme_page == (page_t *)0xdefec8ed);

	hme->ghme.hme_page = pp;
	tmp = mach_pp->p_mapping;

	hme->ghme.hme_next = tmp;
	hme->ghme.hme_prev = NULL;
	if (tmp) {
		ASSERT(tmp->hme_page == pp);
		tmp->hme_prev = (struct hment *)hme;
	}
	mach_pp->p_mapping = (struct hment *)hme;
}

static void
hme_list_sub(hme, pp)
	register struct srhment *hme;
	register page_t *pp;
{
	register struct hment *tmp;

	ASSERT(ohat_mlist_held(pp));
	ASSERT(hme->ghme.hme_page == pp);

	tmp = hme->ghme.hme_prev;
	if (tmp) {
		ASSERT(mach_pp->p_mapping != (struct hment *)hme);
		ASSERT(tmp->hme_page == pp);
		tmp->hme_next = hme->ghme.hme_next;
	} else {
		ASSERT(mach_pp->p_mapping == (struct hment *)hme);
		mach_pp->p_mapping = hme->ghme.hme_next;
	}

	tmp = hme->ghme.hme_next;
	if (tmp) {
		ASSERT(tmp->hme_page == pp);
		tmp->hme_prev = hme->ghme.hme_prev;
	}
	hme->ghme.hme_page = (page_t *)0xdefec8ed;
}

#define	MAX_PTBL	(0x60000)
#define	MIN_MAXPTBL	(0x1000)

void
init_srmmu_var()
{
	/*
	 * Sets up max of srmmu resources.
	 * 	- h/w ptbl won't take more than ~3% of
	 *	phsysical memory.
	 */
	if (max_ptbl == 0) {
		max_ptbl = (physmem >> 5) * 16;
	} else {
		if (max_ptbl < MIN_MAXPTBL) {
			/*
			 * make sure max_ptbl is not too small. it may
			 * have been patched by /etc/system. Map 1G at
			 * least.
			 */
			max_ptbl = MIN_MAXPTBL;
		}
	}

	if (max_ptbl > MAX_PTBL) {
		/*
		 * Don't grow ptbl beyond 96M bytes of
		 * memory. Need to preserve kernelmap for
		 * others.
		 */
		max_ptbl = MAX_PTBL;
	}

	nullhme.ghme.hme_page = (page_t *)0xdefec8ed;

	/*
	 * Increase nctxs to minimize the possibility of
	 * running out of context structures due to ISM.
	 */
	if (nctxs >= v.v_proc + 1) {
		nctxs = v.v_proc + (v.v_proc >> 3);
		static_ctx = 1;
#ifdef sun4m
		while (nctxs & (nctxs - 1)) {
			nctxs = (nctxs | (nctxs - 1)) + 1;
		}
#endif	/* sun4m */
	}
}

#ifdef VAC

/*
 * Find the correct alias based on the first mapping on
 * srmmu. The return value is in the unit of page, not
 * in byte address. Returns -1 when no srmmu mapping is
 * found.
 */
int
fd_page_vac_align(page_t *pp)
{
	int align = -1;
	u_int p_mapping;
	struct srhment *hme;
	struct hat *hat;
	extern u_int vac_mask;
	ptbl_t		*ptbl;
	extern	int vacszpg;

	ASSERT(ohat_mlist_held(pp));

	p_mapping = (u_int) mach_pp->p_mapping;
	if (p_mapping == 0) {
		goto out;
	}

	if (p_mapping & PP_PTBL) {
		ptbl = MAP2PTBL(p_mapping);
		ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == 3);
		align = (int)(BASE2VA(ptbl) +
		    (mach_pp->p_index << MMU_STD_PAGESHIFT));
	} else {
		for (hme = (struct srhment *)p_mapping; hme;
		    hme = (struct srhment *)hme->ghme.hme_next) {
			hat = &hats[hme->ghme.hme_hat];
			if (hat->hat_op == &srmmu_hatops) {
				ptbl = hme->hme_ptbl;
				ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == 3);
				align = (int)(BASE2VA(ptbl) +
				    (hme->hme_index << MMU_STD_PAGESHIFT));
				break;
			}
		}
	}

	align = mmu_btop(align & vac_mask);
	ASSERT(align >= 0 && align < vacszpg);

out:

	return (align);
}
#endif

/*
 * Walk the p_mapping list to see if there is hment
 * belonging to the hat with ops vector of 'hat_op'.
 */
int
srmmu_fd_hment_exist(page_t *pp, struct hatops *hat_op)
{
	u_int p_mapping;
	struct hment *hme;
	struct hat *hat;
	kmutex_t *pml;
	int		rc;

	rc = 0;
	pml = srmmu_mlist_enter(pp);

	p_mapping = (u_int) mach_pp->p_mapping;
	if (p_mapping & PP_PTBL) {
		/* Only one srmmu mapping. */
		if (hat_op == &srmmu_hatops) {
			rc = 1;
		}
	} else {
		for (hme = mach_pp->p_mapping; hme; hme = hme->hme_next) {
			hat = &hats[hme->hme_hat];
			if (hat->hat_op == hat_op) {
				rc = 1;
				break;
			}
		}
	}
	srmmu_mlist_exit(pml);
	return (rc);
}

/*
 * Return a list pp being mapped behind addr to addr+ *lenp in "ppp".
 * Mappings have to be user-readable. If HAT_COW is set in flags,
 * user-write protection will be disabled. This is for those callers that
 * try to set up copy-on-write protection.
 * Note that for a VAC copy-back cache, we have to flush the cache.
 */
static faultcode_t
srmmu_softlock(hat, addr, lenp, ppp, flags)
	struct  hat *hat;
	caddr_t	addr;
	size_t	*lenp;
	page_t	**ppp;
	u_int	flags;
{
	struct	pte *pte = NULL, apte;
	page_t	*pp;
	struct  ptbl *ptbl = NULL;
	int	level;
	kmutex_t *mtx = NULL;
	faultcode_t res = 0;

	do {
		if (!pte || MMU_L2_OFF(addr) == 0) {
			if (pte)
				unlock_ptbl(ptbl, mtx);
			pte = srmmu_ptefind(hat->hat_as, addr, &level, &ptbl,
			    &mtx, 0);
		} else {
			pte++;
		}
		mmu_readpte(pte, &apte);
		if (!pte_valid(&apte)) {
			res = FC_NOMAP;
			break;
		} else if (level != 3) {
			res = FC_NOSUPPORT;
			break;
		}
		if ((apte.AccessPermissions > 3) &&
			(apte.AccessPermissions != 5)) {
			res = FC_PROT;
			break;
		}
		pp = pfn2pp_hash(apte.PhysicalPageNumber);
		if (!pp || !page_trylock(pp, SE_SHARED)) {
			res = FC_OBJERR;
			break;
		}
		if ((flags & HAT_COW) &&
		    ((apte.AccessPermissions == MMU_STD_SRWURW) ||
		    (apte.AccessPermissions == MMU_STD_SRWXURWX))) {

			apte.AccessPermissions = MMU_STD_SRUR;
#ifdef VAC
			(void) mmu_writepte(pte, *(u_int *)&apte, addr, 3,
			    hat, (vac & VAC_WRITETHRU) ?
			    PTE_RM_MASK|SR_NOPGFLUSH : PTE_RM_MASK);
			/*
			 * For vac, since cow_mapin() will sets up a
			 * HAT_NOCONSIST kernel addr, we have to flush
			 * the cache either on a
			 * 1. VAC_WRITEBACK cache for the kernel addr
			 *    to see the dirty data, or
			 * 2. VAC_IOCOHERENT cache for DVMA to get
			 *    the right data.
			 */
#else
			(void) mmu_writepte(pte, *(u_int *)&apte, addr, 3,
			    hat, PTE_RM_MASK);
#endif
			/*
			 * It's important to set PTE_RM_MASK to keep the
			 * R M bits.
			 */
		}
#ifdef VAC
		else if (vac & (VAC_WRITEBACK|VAC_IOCOHERENT)) {
			vac_flush(addr, PAGESIZE);
		}
#endif
		addr += PAGESIZE;
		*ppp++ = pp;
		*lenp -= PAGESIZE;
	} while (*lenp > 0);
	unlock_ptbl(ptbl, mtx);
	return (res);
}

#ifdef ZC_TEST
#include <sys/strsubr.h>
#endif

/*
 * Flip mappings behind addr_to and kaddr. kaddr is assumed to map to a
 * kvp page. Current implementation only supports level 3 entries, and
 * pages with one mapping. This function works closely with page_flip(),
 * and should be used together. See comments in hat_pageflip() for how
 * the work is divided between the two.
 */
static faultcode_t
srmmu_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t addr_to, kaddr;
	size_t	*lenp;
	page_t	**pp_to, **pp_from;
{
	page_t	*kpp, *pp;
	int	level;
	struct	pte *pte = NULL, *kpte = NULL, t1pte, t2pte;
	struct	ptbl *ptbl_to = NULL, *ptbl_from;
	struct	hment *p_mapping;
	u_char	tmp;
	int	flags = 0;
	kmutex_t *mtx_to = NULL;
	faultcode_t res = 0;
	extern struct vnode kvp;

	/*
	 * Assumption: kaddr mapped to a kvp page, so the page table
	 * is locked down already.
	 */
	do {
		if (!pte || MMU_L2_OFF(addr_to) == 0) {
			if (pte)
				unlock_ptbl(ptbl_to, mtx_to);
			pte = srmmu_ptefind(hat->hat_as, addr_to, &level,
			    &ptbl_to, &mtx_to, 0);
		} else {
			pte++;
		}
		mmu_readpte(pte, &t1pte);

		if (!pte_valid(&t1pte)) {
			res = FC_NOMAP;
			break;
		}
		if (level != 3) {
			res = FC_NOSUPPORT;
			break;
		}

		pp = pfn2pp_hash(t1pte.PhysicalPageNumber);

		/*
		 * We have to hold an exclusive lock on the page in order
		 * to change its identity. VM system imposes a rule that
		 * anyone stopped by the se lock must go back and recheck
		 * the identity. (E.g. in page_lookup). So this should
		 * work for us.
		 */
		if (!pp || !page_trylock(pp, SE_EXCL)) {
			res = FC_OBJERR;
			break;
		}

		/*
		 * We only handle the majority case - there is only one
		 * mapping.
		 * With excl lock on pp, # of mapping can't go up. So we
		 * don't have to hold the mlist lock.
		 * TODO: Potentially, we could handle two mappings and
		 * still outperform bcopy.
		 */
		if (mach_pp->p_share != 1) {
			page_unlock(pp);
			res = FC_OBJERR;
			break;
		}

		if (t1pte.AccessPermissions != MMU_STD_SRWURW &&
			t1pte.AccessPermissions != MMU_STD_SRWXURWX) {
			page_unlock(pp);
			res = FC_PROT;
			break;
		}

		kpp = page_find(&kvp, (u_offset_t)(u_int)kaddr);
		if (!page_tryupgrade(kpp) ||
		    (((machpage_t *)kpp)->p_share != 1)) {
			page_unlock(pp);
			res = FC_OBJERR;
			break;
		}
		ASSERT(((u_int)((machpage_t *)kpp)->p_mapping) & PP_PTBL);
		ptbl_from = (ptbl_t *)
		    ((u_int)(((machpage_t *)kpp)->p_mapping)&CLR_LAST_2BITS);
		kpte = (struct pte *)ptbltopt_va(ptbl_from) +
		    ((machpage_t *)kpp)->p_index;
		/* XXX - should check and fail any non-level-3 ptes */
		mmu_readpte(kpte, &t2pte);
		if (!pte_valid(&t2pte)) {
			cmn_err(CE_PANIC,
			    "srmmu_pageflip: invalid kernel addr %p",
			    (void *)kaddr);
			/*NOTREACHED*/
		}

		/* XXX Is p_vcolor being used for physical coloring also? */
#ifdef VAC
		if (do_pg_coloring && (cache & CACHE_VAC) &&
		    ((machpage_t *)kpp)->p_vcolor == mach_pp->p_vcolor) {
			flags = SR_NOPGFLUSH;
		}
#endif
		t1pte.Modified = t1pte.Referenced = 0;
		t1pte.PhysicalPageNumber = ((machpage_t *)kpp)->p_pagenum;
		(void) mmu_writepte(pte, *(u_int *)&t1pte, addr_to,
		    3, hat, flags);
		t2pte.Modified = t2pte.Referenced = 0;
		t2pte.PhysicalPageNumber = mach_pp->p_pagenum;
		(void) mmu_writepte(kpte, *(u_int *)&t2pte, kaddr, 3,
		    kas.a_hat, flags);
		/*
		 * rmkeep can't be PTE_RM_MASK. Otherwise it will trigger
		 * "local" in vik_mp_mmu_writepte.
		 * If it's a VAC, mmu_writepte will flush the cache.
		 */
		tmp = ((machpage_t *)kpp)->p_index;
		((machpage_t *)kpp)->p_index = mach_pp->p_index;
		mach_pp->p_index = tmp;

		tmp = ((machpage_t *)kpp)->p_vcolor;
		((machpage_t *)kpp)->p_vcolor = mach_pp->p_vcolor;
		mach_pp->p_vcolor = tmp;

		p_mapping = ((machpage_t *)kpp)->p_mapping;
		((machpage_t *)kpp)->p_mapping = mach_pp->p_mapping;
		mach_pp->p_mapping = p_mapping;
		addr_to += PAGESIZE;
		kaddr += PAGESIZE;
		*pp_to++ = pp;
		*pp_from++ = kpp;
		*lenp -= PAGESIZE;
	} while (*lenp > 0);

#ifdef ZC_TEST
	if (res && res != FC_NOMAP) {
		if (zcdebug & ZC_DEBUG && res == FC_OBJERR) {
			prom_printf("pp/kpp=%X/%X\n", pp, kpp);
			debug_enter("srmmu_pageflip");
		} else if (zcdebug & ZC_WARN && res != FC_OBJERR) {
			prom_printf("res =%X addr=%X\n", res, addr_to);
			debug_enter("srmmu_pageflip");
		}
	}
#endif
	unlock_ptbl(ptbl_to, mtx_to);
	return (res);
}

#ifdef MORE_DEBUG

static int vrlvl0;

static void
vrfy_ptbl_vcnt(ptbl_t *ptbl)

{
	int i, lvl, tpte;
	struct pte *pte;
	int inv, nptp, npte, bad;

	ASSERT(PTBL_IS_LOCKED(ptbl->ptbl_flags));

	lvl = PTBL_LEVEL(ptbl->ptbl_flags);

	if (lvl == 1) {
		return;
	}

	if (lvl == 0) {
		vrlvl0++;
		return;
	}

	pte = ptbltopt_va(ptbl);

	inv = nptp = npte = bad = 0;
	for (i = 0; i < 64; i++, pte++) {
		mmu_readpte(pte, (struct pte *)&tpte);

		switch (tpte & 0x3) {
		case 1:
			nptp++;
			break;

		case 2:
			npte++;
			break;

		case 3:
			bad++;
			break;

		case 0:
			inv++;
			break;
		}
	}

	ASSERT((nptp + npte + bad + inv) == 64);

	if (bad) {
		cmn_err(CE_PANIC, "bad entry! bad %d ptbl %p",
		    bad, (void *)ptbl);
	}

	if (lvl == 3) {
		if (nptp) {
			cmn_err(CE_PANIC, "bad ptp! nptp %d ptbl %p",
			    nptp, (void *)ptbl);
		}

		if (npte != ptbl->ptbl_validcnt) {
			cmn_err(CE_PANIC, "bad validcnt %d npte %d "
			    "ptbl %p",
			    ptbl->ptbl_validcnt, npte, (void *)ptbl);
		}

		return;
	}

	if (lvl == 2) {
		if ((npte + nptp) != ptbl->ptbl_validcnt) {
			cmn_err(CE_PANIC, "bad validcnt %d npte %d "
			    "nptp %d ptbl %p", ptbl->ptbl_validcnt,
			    npte, nptp, (void *)ptbl);
		}

	}

}
#endif

#ifdef VAC
/*
 * initialize a mutex for the list of spt address spaces
 */
void
spt_mtxinit()
{
	mutex_init(&spt_as_mtx, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Add entry for the sptas onto list of shm address spaces
 */
static void
spt_addsptas(struct as *as)
{
	register struct  spt_aslist *spt;

	mutex_enter(&spt_as_mtx);

	if (spt_ashead) {
		for (spt = spt_ashead; spt; spt = spt->spt_next) {
			if (spt->spt_as == as) {
				mutex_exit(&spt_as_mtx);
				return;
			}
		}
	}

	spt = (struct spt_aslist *)
		kmem_zalloc(sizeof (struct spt_aslist), KM_SLEEP);
	spt->spt_as = as;
	spt->spt_list = NULL;
	spt->spt_count = 0;

	spt->spt_next = spt_ashead;
	spt_ashead = spt;
	mutex_exit(&spt_as_mtx);
}
/*
 * Delete the requested sptas from the list of shm address spaces
 */
static void
spt_delsptas(struct as *as)
{
	register struct  spt_aslist *spt, *last;

	mutex_enter(&spt_as_mtx);
	if (spt_ashead == NULL) {
		mutex_exit(&spt_as_mtx);
		return;
		/*
		 * cmn_err(CE_PANIC, "spt_delsptas: NULL spt_ashead\n");
		 * now this is called from srmmu_free() so we may not have
		 * any spt as to delete, so we don't do anything
		 */
	}

	/*
	* Search list for this sptas
	*/
	for (last = spt = spt_ashead; spt; spt = spt->spt_next) {
		if (spt->spt_as == as) {
			if (spt == spt_ashead)
				spt_ashead = spt->spt_next;
			else
				last->spt_next = spt->spt_next;
			mutex_exit(&spt_as_mtx);
			kmem_free(spt, sizeof (struct spt_aslist));
			return;
		}
		last = spt;
	}

	mutex_exit(&spt_as_mtx);

	/*
	 * cmn_err(CE_PANIC, "spt_delsptas: sptas not found\n");
	 * since this is called from srmmu_free() we may not find
	 * spt as
	 */
}
/*
 * Add entry for this segment onto list of shm address spaces
 */
static void
spt_addsptseg(struct seg *seg, struct as *sptas)
{
	register struct	spt_aslist	*spt;
	register struct	spt_seglist	*sp, *tsp;

	/*
	 * Allocate segment info
	 */
	sp = (struct spt_seglist *)
	kmem_zalloc(sizeof (struct spt_seglist), KM_SLEEP);

	/*
	 * find which sptas this segment is attached to
	 */
	mutex_enter(&spt_as_mtx);
	for (spt = spt_ashead; spt; spt = spt->spt_next) {
		if (spt->spt_as == sptas)
			break;
	}

	if (spt == NULL) {
		mutex_exit(&spt_as_mtx);
		kmem_free(sp, sizeof (struct spt_seglist));
		cmn_err(CE_PANIC, "spt_addsptseg: sptas not found");
	}

	/*
	 * Should not add duplicate entries
	 */
	for (tsp = spt->spt_list; tsp; tsp = tsp->spt_next) {
		if (tsp->spt_seg == seg) {
			/*
			 * duplicate entry from segspt_shmdup()
			 */
			mutex_exit(&spt_as_mtx);
			kmem_free(sp, sizeof (struct spt_seglist));
			return;
		}
	}

	/*
	 * Link the segment info
	 */
	sp->spt_seg = seg;
	sp->spt_next = spt->spt_list;
	spt->spt_list = sp;
	spt->spt_count++;
	mutex_exit(&spt_as_mtx);
}

/*
 * Delete entry for this segment from list of shm address spaces
 */
static void
spt_delsptseg(struct seg *seg, struct as *sptas)
{
	register struct	spt_aslist	*spt;
	register struct	spt_seglist	*sp, *last;

	/*
	* find which sptas this segment is attached to
	*/
	mutex_enter(&spt_as_mtx);
	for (spt = spt_ashead; spt; spt = spt->spt_next) {
		if (spt->spt_as == sptas)
			break;
	}

	if (spt == NULL) {
		mutex_exit(&spt_as_mtx);
		cmn_err(CE_PANIC, "spt_delsptseg: sptas not found");
	}

	/*
	 * Remove seg info from list and deallocate
	 */
	for (sp = last = spt->spt_list; sp; sp = sp->spt_next) {
		if (sp->spt_seg == seg) {
			spt->spt_count--;
			if (sp == spt->spt_list)
				spt->spt_list = sp->spt_next;
			else
				last->spt_next = sp->spt_next;
			mutex_exit(&spt_as_mtx);
			kmem_free(sp, sizeof (struct spt_seglist));
			return;
		}
		last = sp;
	}
	mutex_exit(&spt_as_mtx);
	cmn_err(CE_PANIC, "spt_delsptseg: spt seg not found");
}


void
spt_vacsync(char *vaddr, struct as *sptas)
{
	register struct	spt_aslist	*spt;
	register struct	spt_seglist	*sp;
	register struct	as		*as;
	register struct	srmmu		*srmmu;
	char    *addr;
	short   ctxn;

	mutex_enter(&spt_as_mtx);
	/*
	* find index into list of segments - from sptas
	*/
	for (spt = spt_ashead; spt; spt = spt->spt_next) {
		if (spt->spt_as == sptas)
			break;
	}

	if (spt == NULL)  {
		mutex_exit(&spt_as_mtx);
		cmn_err(CE_PANIC, "spt_vacsync: sptas not found");
	}

	/*
	 * If the number of attached processes is equal to or greater than
	 * max_spt_shmseg, then flush the entire cache
	 */
	if (spt->spt_count >= max_spt_shmseg)  {
	mutex_exit(&spt_as_mtx);
	XCALL_PROLOG;
	vac_allflush(FL_TLB_CACHE);
	XCALL_EPILOG;
	return;
	}
	/*
	* now use segment info to flush vaddr in all contexts
	*/
	for (sp = spt->spt_list; sp; sp = sp->spt_next) {
		as = sp->spt_seg->s_as;
		hat_enter(as->a_hat);
		srmmu = (struct srmmu *)as->a_hat->hat_data[HAT_DATA_SRMMU];
		ctxn = srmmu->s_ctx;
		if (ctxn != -1) {
			addr = sp->spt_seg->s_base + (int)vaddr;
			XCALL_PROLOG;
			srmmu_vacflush(3, addr, ctxn, FL_TLB_CACHE);
			XCALL_EPILOG;
		}
		hat_exit(as->a_hat);
	}
	mutex_exit(&spt_as_mtx);
}
#endif /* VAC */


/* ************ XXXX NEED HATOPS???? *********** */

/*
 * Yield the memory claim requirement for an address space.
 *
 * This is currently implemented as the number of bytes that have
 * active hardware translations that have page structures.  Therefore,
 * it can underestimate the traditional resident set size, eg, if the
 * physical page is present and the hardware translation is missing;
 * and it can overestimate the rss, eg, if there are active
 * translations to a frame buffer with page structs.
 * Also, it does not take sharing into account.
 */
size_t
hat_get_mapped_size(struct hat *hat)
{
/*	ASSERT(hat->hat_op == sys_hatops); leo replaces hatops in system hat */

	if (hat != NULL) {
		return ((size_t)ptob(hattosrmmu(hat)->s_rss));
	} else {
		return (0);
	}
}

int
hat_stats_enable(struct hat *hat)
{
/*	ASSERT(hat->hat_op == sys_hatops); leo replaces hatops in system hat */

	mutex_enter(&hat->hat_mutex);
	hattosrmmu(hat)->s_rmstat++;
	mutex_exit(&hat->hat_mutex);
	return (1);
}

void
hat_stats_disable(struct hat *hat)
{
/*	ASSERT(hat->hat_op == sys_hatops); leo replaces hatops in system hat */

	mutex_enter(&hat->hat_mutex);
	hattosrmmu(hat)->s_rmstat--;
	mutex_exit(&hat->hat_mutex);
}

void
hat_page_setattr(page_t *pp, u_int flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	if ((mach_pp->p_nrm & flag) == flag) {
		/* attribute already set */
		return;
	}
	srmmu_page_enter(pp);
	mach_pp->p_nrm |= flag;
	srmmu_page_exit(pp);
}

void
hat_page_clrattr(page_t *pp, u_int flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	srmmu_page_enter(pp);
	mach_pp->p_nrm &= ~flag;
	srmmu_page_exit(pp);
}

u_int
hat_page_getattr(page_t *pp, u_int flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));
	return (mach_pp->p_nrm & flag);
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
	default:
		return (0);
	}
}

ssize_t
hat_getpagesize(struct hat *hat, caddr_t addr)
{
	struct pte *pte, tpte;
	ptbl_t *ptbl;
	kmutex_t *mtx;
	ssize_t size;
	int level;

	pte = srmmu_ptefind(hat->hat_as, addr, &level, &ptbl,
	    &mtx, LK_PTBL_SHARED);
	mmu_readpte(pte, &tpte);
	unlock_ptbl(ptbl, mtx);

	if (tpte.EntryType == MMU_ET_PTE) {
		size = srmmu_sizes[level];
	} else {
		size = -1;
	}

	return (size);
}

/*
 * Functions to allow locking from outside.
 */
void
hat_enter(struct hat *hat)
{
	mutex_enter(&hat->hat_mutex);
}

void
hat_exit(struct hat *hat)
{
	mutex_exit(&hat->hat_mutex);
}

/*
 * Srmmu external version of mlist enter/exit.
 */
void
ohat_mlist_enter(struct page *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	mutex_enter(mml);
}

void
ohat_mlist_exit(struct page *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	mutex_exit(mml);
}

int
ohat_mlist_held(struct page *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	return (MUTEX_HELD(mml));
}

u_long
hat_getkpfnum(caddr_t addr)
{
	return (hat_getpfnum((struct hat *)kas.a_hat, addr));
}

void
hat_reserve(struct as *as, caddr_t addr, size_t len)
{
	srmmu_reserve(as, addr, len, 1);
}
