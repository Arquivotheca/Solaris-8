/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hat_i86.c	1.137	99/12/06 SMI"

/*
 * VM - Hardware Address Translation management.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <common/vm/hat.h> while the machine dependent interface
 * and data structures are described in <i86/vm/hat_i86.h>.
 *
 * The hat layer manages the address translation hardware
 * driven by calls from the higher levels in the VM system.  Nearly
 * all the details of how the hardware is managed should not be visible
 * about this layer except for miscellanous machine specific functions
 * (e.g. mapin/mapout) that work in conjunction with this code.  Other
 * than a small number of machine specific places, the hat data
 * structures seen by the higher levels in the VM system are opaque
 * and are only operated on by the hat routines.  Each address space
 * contains a struct hat and a page contains an opaque pointer which
 * is used by the hat code to hold a list of active translations to
 * that page.
 */

/*
 * Hardware address translation routines for the Intel 80x86 MMU.
 * Originally based upon sun4m (srmmu) hat code.
 * x86 processors use a control register called cr3 to point to the tables
 * that provide virtual to physical mapping (see Intel's SDM system programmers
 * Guide (order # 243192-001 for more details.) We have 4 different categories
 * of cr3s - 1) kernel_only_cr3 which maps addresses above kernelbase.
 * 2) pteas_cr3 which maps all of kernel + all the pagetables (all ptes) +
 * all pagedirectories and userhwppmaps. pteas_cr3 can be considered to map
 * an "extended kas" with the addresses below kernelbase belonging to the
 * extended part. The only ways in which the rest of ther kernel will associate
 * these with kas is through segpt's DUMPOP or by calling hat_getkpfnum on
 * a valid address below kernelbase.
 * 3) per process cr3 - which maps kernel + all pagetables for the process
 * + all user space for the process. Any process which has LOCKed mappings
 * or has more than certain number of pagetables will get a per process cr3.
 * (a.k.a per process pagedir a.k.a ppp in this source).
 * 4) per cpu cr3 - which maps kernel + the user space of the process
 * associated with CPU->cpu_current_hat (which will be  a process that does
 * not have a ppp). The page directory entries for cpu_current_hat are
 * loaded/unloaded into the per cpu pagetable on any context switch involving
 * a non ppp process.
 * Note that pagetables get mapped at 2 different addresses - one from
 * ptearena (mapped by pteas_cr3) and the other from either a per process
 * virtual address between usermapend and kernelbase or from kernel heap.
 *
 * This implemenation also caches the following
 * 1. hmes - through kmem_cache_alloc. Provides better cache locality for
 * the 8 byte allocations & also provides us the callback when the system
 * is low on memory so that we can reap all other caches too
 * 2. pagetables. This avoids having to get exlusive locks, downgrade, bzero
 * etc. every time we have to alloc a pagetable.
 * 3. hwpps with associated pagetables & pte memory. Avoids our having to
 * switch to pteashat for every pagetable alloc & free (to update pt_hwpp
 * and pt_ptable)
 * 4. ppps. Avoids the costly exercise of allocing per process pagedir,
 * associated page directories and filling them up.
 */

#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/vtrace.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/proc.h>

#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/vmem.h>
#include <sys/vmsystm.h>
#include <sys/promif.h>

#include <vm/hat.h>
#include <vm/hat_i86.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/rm.h>
#include <vm/seg_kp.h>
#include <vm/seg_kmem.h>
#include <vm/seg_spt.h>
#include <sys/var.h>
#include <sys/x86_archext.h>
#include <vm/faultcode.h>
#include <sys/atomic.h>
#include <sys/bitmap.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_implfuncs.h>
#include <sys/dumphdr.h>


/*
 * Create attribute structure to allocate page directory table ptr entries
 * which need to be 32 byte aligned and below 4GB.
 */
ddi_dma_attr_t	lt4gb_attr = {
		DMA_ATTR_V0,
		0,			/* low DMA addr */
		(uint_t)0xFFFFFFFF,	/* hi DMA addr */
		(uint_t)0xFFFFFFFF,	/* count max */
		32,			/* align */
		1,			/* burst size */
		1,			/* Min xfer */
		(uint_t)0xFFFFFFFF,	/* Max xfer */
		1,			/* seg boundary */
		1,			/* sgllen */
		1,			/* granularity */
		0 			/* flags */
};

/*
 * pages with a single mapping has
 *	p_mapping -> pte
 *
 * pages with multiple mappings has
 *
 * 	p_mapping -> hment.next -> hment.next -> ... -> NULL
 *			  .pte		.pte
 *
 * to distinguish whether p_mapping points to a pte or to an hment, PTE_MARK
 * set in unused bits in p_mapping (pte are 4 or 8 byte aligned which means
 * least significant bits must be 0)
 *
 * ROC_MARK may also be set in p_mapping if the entire chain is read only
 */

#define	PTE_MARK	0x01		/* pte pointer as opposed to hme ptr */
#define	ROC_MARK	0x02		/* read only chain */

#define	IS_HMEPTR(mapval)	(!(((uint32_t)(mapval)) & PTE_MARK))
#define	IS_PTEPTR(mapval)	(((uint32_t)(mapval)) & PTE_MARK)
#define	IS_ROC(mapval)		(((uint32_t)(mapval)) & ROC_MARK)

#define	PPMAPPING(ptr, ispte, isro)	\
	((uint32_t)(ptr) | ((ispte) ? PTE_MARK : 0) | ((isro) ? ROC_MARK : 0))

#define	PPMAPPTR(ptr)	(((uint32_t)(ptr)) & ~(PTE_MARK | ROC_MARK))
#define	I86_MLIST_LOCKED(mach_pp)	(mach_pp->p_flags & P_MLISTLOCKED)

struct	hat	*hats;
int		hat_gb_enable = 0;
int		use_hat_gb = 0;

#define	PMTX_HASH(pp)	\
	(mlistlock + (((machpage_t *)pp)->p_pagenum & PSMHASHMASK))

#define	PCV_HASH(pp)	\
	(mlistcondvar + (((machpage_t *)pp)->p_pagenum & PSMHASHMASK))

#define	PSMHASHMAX	32
#define	PSMHASHMASK	(PSMHASHMAX - 1)

kcondvar_t		mlistcondvar[PSMHASHMAX];
kmutex_t		mlistlock[PSMHASHMAX];

#define	MLISTHELD	1
#define	MLISTNOTHELD	0

#define	HOLDMLIST	1	/* leave mlist lock and mlist as is */
#define	NOHOLDMLIST	0	/* free mlist lock and purge mlist */

#define	MAXHATSTEALCNT	16

/*
 * hat hat layer tunables
 *
 * hat_accurate_pmapping	Add mappings even for NOSYNC mappings.
 * hat_perprocess_pagedir	Allocate a page directory when the process
 *				has more than N hwpps. 1 implies all processes
 *				get a private page directory.
 *
 * hat_hmereap_watermark	physically remove freed hmes
 */

int	hat_accurate_pmapping = 0;
int	hat_perprocess_pagedir;
int	hat_perprocess_pagedir_max = 8;
/*
 * most of the time, we will reap the hmes from p_mapping list
 * when the number of invalid mappings is more than the valid
 * ones by lowatermark. But, we don't want more than hiwatermark
 * invalid mappings, irrespective of number of valid ones (Normally
 * should be kept very high and use just to ensure p_deleted does
 * not overflow.
 */
uint_t	hat_hmereap_hiwatermark = 8000;
uint_t	hat_hmereap_lowatermark = 10;

int	hat_devload_cache_system_memory = 1;
int	hat_devload_cache_device_memory = 0;
/*
 * End of kernelmap Virtual address for which
 * pagetables have been allocated
 */
extern uint_t		phys_syslimit;
extern int		kadb_is_running;

extern pteval_t		*userptemap;
extern hwptepage_t	**userhwppmap;
extern pteval_t		*userptetable;	/* ptes to pages in userptemap */
extern pteval_t		*userpagedir;
extern caddr_t		usermapend;

struct hat		all_hat;
struct hat		*kernel_hat;
struct hat		*hat_searchp = &all_hat;

page_t	*page_searchp;	/* pp to start with, for trying to reclaim hmes */
int	in_hat_hme_reclaim = 0;

/*
 * Freelist & count for pagetable cache & ppp cache.
 */


page_t		*hat_page_freelist;
ulong_t		hat_pagecount;
ulong_t		hat_page_cache_max;

hwptepage_t	*hat_hwpp_freelist;		/* hwpp w/ pteas pte */
static kmutex_t	hat_hwpp_cache_lock;
ulong_t		hat_hwppcount;
ulong_t		hat_hwpp_cache_max;

hwptepage_t	*hat_hwpplpte_freelist;		/* hwpp w/ pte/lpte */
static kmutex_t	hat_hwpplpte_cache_lock;
ulong_t		hat_hwpplptecount;
ulong_t		hat_hwpplpte_cache_max;

#define	HWPPLIST		0x01
#define	HWPPLPTELIST		0x02

struct hatppp	*hat_ppplist;
ulong_t		hat_pppcount;
ulong_t		hat_ppp_cache_max;
caddr_t		hat_page_addr;

ulong_t		hat_hatcount;	/* hats in free list */
ulong_t		hat_kmemcount;	/* kmem usage during setup and hat_alloc */
ulong_t		hat_mlistsleepcount;	/* contention in mlist */
ulong_t		hat_mlistresleepcount;	/* repeated contention in mlist */


uint_t		user_pdelen;	/* len of user pdes in perprocess pagedir */
uint_t		kernel_pdelen;	/* len of kernel pdes in perprocess pagedir */
uint_t		user_pgdirpttblents;
uint_t		npdirs;		/* pages needed as pdir for a ppp process */
uint_t		hat_ppp_pteas_pages; /* # of pages in pteasmap for ppp */

#define	addrtoptemapindex(addr)		(((uint_t)(addr)) / PAGESIZE)
#define	addrtohwppindex(addr)		(((uint_t)(addr)) / PTSIZE)
#define	addrtoptetableindex(addr)	(((uint_t)(addr)) / PTSIZE)

#define	hwppindextoaddr(index)		((caddr_t)((index) * PTSIZE))

/*
 * i86MMU has 2 levels of locking.  The locks must be acquired
 * in the correct order to prevent deadlocks.
 *
 * The hat structs are locked with the per-hat hat_mutex.  This
 * lock must be held to change any resources that are allocated to
 * the hat.
 *
 * hat->hat_mutex  - protects user hat structure.
 *
 * For kernel address space we do not grab kas.a_hat->hat_mutex, since all
 * pagetables are allocated at startup time and the kernel hat does not
 * change in any fashion.
 * pp's mapping list lock locks p_mapping, p_share and p_deleted fields in
 * the page structure in addition to the linked list of hmes.
 * There are also various cache_locks which protect the freelist for the
 * resource.
 * Accquire locks in the following order
 *
 * hat->hat_mutex lock
 * pp's mapping list lock, any of the cache locks.
 * We should only do a try_enter for hat_mutex after holding mapping list lock
 *
 */
static kmutex_t	hat_hat_lock;
static kmutex_t	hat_page_cache_lock;

extern kmutex_t	hat_page_lock;


/*
 * Public function and data referenced by this hat
 */
extern pgcnt_t 		physmem;
extern struct seg	*segkmap;
extern struct seg	*segkp;
extern caddr_t		econtig;
extern caddr_t		eecontig;
extern struct as	kas;		/* kernel's address space */
extern struct cpu	cpus[];
extern int		ncpus;
extern int 		pentiumpro_bug4064495;
extern ulong_t		po_share;
extern		uchar_t	cas8(uchar_t *, uchar_t, uchar_t);

/*
 * Private vm_hat data structures.
 */

vmem_t			*ptearena;
pteval_t		*pt_pdir;
pteval_t		*pt_ptable;
pteval_t		pt_invalid_pteptr;
struct hwptepage	pt_invalid_hwpp;
struct hwptepage	**pt_hwpp;
int 			pteas_cr3;
size_t			pteas_size;
struct	hat		*pteashat;

struct	hat		*hatfree;	/* free list */

size_t			pteasvmemsz;
size_t			pteaslowater;
size_t			pteashiwater;

#define	PTEASLO()	(pteasvmemsz >> 1)
#define	PTEASHI()	((pteasvmemsz >> 1) + (pteasvmemsz >> 2))
static int		getpppthresh();

static int		hme_purge_dups(page_t *, struct hment **);
static void		hat_setup_internal(struct hat *);
static void 		hat_pteunload(struct hat *, struct page *, caddr_t,
				pteval_t *, pteval_t *, int);
static void		hat_pteload(struct hat *hat, caddr_t addr,
				struct page *pp, pteval_t *pte, int flags);
static void 		hat_ptesync(struct hat *, struct page *, caddr_t,
				pteval_t *, int, int);
static uint_t 		hat_update_pte(struct hat *, pteval_t *, pteval_t,
			    caddr_t, uint_t);
static struct hwptepage *hat_hwppalloc(struct hat *, int);
static void 		hat_hwppfree(struct hat *, struct hwptepage *, int);
static void 		hat_hwppdelete(struct hat *, struct hwptepage *);
static void		hat_pagetablealloc(struct hat *,
				struct hwptepage *hwpp);
static void 		hat_pagetablefree(struct hat *, struct hwptepage *);
static void		hat_hwppunload(struct hat *, struct hwptepage *);
static void 		hat_update_pde(struct hat *, uint_t, pteval_t);
static struct hwptepage	*hat_hwppfind(struct hat *, uint_t);
#ifdef HATDEBUG
static void		hattlbgendebug(int id, uint_t gen);
#endif
static void		hat_unload_pdes(struct cpu *cpup);
static int		i86_switch_hat(struct hat *);
static void		i86_restore_hat(uint_t);
static void		hat_page_cache_reclaim();
static void		hat_hwpp_cache_reclaim(int);
static void		hme_purge(page_t *, struct hment *);
static void		hme_sub(page_t *, pteval_t *, int);
static void		hme_cache_free(struct hment *);
static void		i86_mlist_enter(page_t *);
static int		i86_mlist_tryenter(page_t *);
static void		i86_mlist_exit(page_t *);
static void		hat_deallocppp(struct hatppp *);
static uint_t		hat_switch2ppp(struct hat *);
static void		segpt_badop(void);
static void		segpt_dump(struct seg *);

/*
 * segpt's only mission in life is to catch the dump operation
 * to dump all the pte pages, page directory pages and hwppmaps.
 * (These pages are mapped into pteasmap and does not overlap with
 * any other kernel address. Think of pteasmap as an extension of
 * kas used only by the i86 hat layer.)
 * Mark all else as bad operations
 */
#define	SEGPT_BADOP(t)	(t(*)())segpt_badop
static struct	seg_ops segpt_ops = {
	SEGPT_BADOP(int),		/* dup */
	SEGPT_BADOP(int),		/* unmap */
	SEGPT_BADOP(void),		/* free */
	SEGPT_BADOP(faultcode_t),	/* fault */
	SEGPT_BADOP(faultcode_t),	/* faulta */
	SEGPT_BADOP(int),		/* setprot */
	SEGPT_BADOP(int),		/* chkprot */
	SEGPT_BADOP(int),		/* kluster */
	SEGPT_BADOP(size_t),		/* swapout */
	SEGPT_BADOP(int),		/* sync */
	SEGPT_BADOP(size_t),		/* incore */
	SEGPT_BADOP(int),		/* lockop */
	SEGPT_BADOP(int),		/* getprot */
	SEGPT_BADOP(u_offset_t),	/* getoffset */
	SEGPT_BADOP(int),		/* gettype */
	SEGPT_BADOP(int),		/* getvp */
	SEGPT_BADOP(int),		/* advise */
	segpt_dump,			/* dump */
	SEGPT_BADOP(int),		/* pagelock */
	SEGPT_BADOP(int),		/* getmemid */
};

static struct seg *segpt;	/* Segment for pagetable memory */

static	struct	kmem_cache	*hat_hmecache;
/*
 * full fledged debug on 8 byte hmes create too much overhead making
 * debug kernel not very useful on large machines. Make it a variable
 * so that we can patch in other flags as needed.
 */
int	hat_hmecache_flags = KMC_NODEBUG;

/*
 * Semi-private data
 */
struct hment	*khmes;		/* prealloced hmes for kvseg */
struct hment	*ekhmes;	/* end of prealloced hmes for kvseg */
pteval_t	*kptes;		/* prealloced ptes for all of kernel */
pteval_t	*kheappte;	/* part of kptes for kvseg */
pteval_t	*ekheappte;	/* end of part of kptes for kvseg */
uint_t		kernel_pts;
uint_t		eecontigindex;	/* # of ptes in Sysmap */
pteval_t	*Sysmap;	/* pointer to ptes mapping from kernelbase */
pteval_t	*KERNELmap;	/* pointer to ptes mapping from SEGKMAP_START */

/*
 * Global data
 */

uint_t total_hwptepages = 0;	/* total no. of hwptepages allocated */

pteval_t	*kernel_only_pagedir;
pteval_t	*kernel_only_pttbl;
uint_t		kernel_only_cr3;

#define	LOAD_CR3(cpup, cr3) {					\
	load_cr3((cpup), (uint_t)(cr3));			\
	HATSTAT_INC_CP(cpup, HATSTAT_CR3_LOAD);			\
}

#define	LOAD_CR3_OPTIM(cpup, cr3)  				\
	if (cpup->cpu_curcr3 != cr3) {				\
		load_cr3((cpup), (uint_t)(cr3));		\
		HATSTAT_INC_CP(cpup, HATSTAT_CR3_LOAD);		\
	}

#define	HAT_INVALID_ADDR	(caddr_t)-1
#define	HAT_NOT_SWITCHED	-1

#define	LOCK		0x0
#define	DONTLOCK	0x01
#define	TRYLOCK		0x02

#define	HMEFREE(hmefree)					\
	while (hmefree) {					\
		struct hment	*hmenext = hmefree->hme_next;	\
		hme_cache_free(hmefree);			\
		hmefree = hmenext;				\
	}

/*
 * 4 Mb virtual address space that some psm modules use for
 * the purpose of 1-1 mapping
 */
#define	PROM_SIZE	FOURMB_PAGESIZE
#define	KADB_SIZE	FOURMB_PAGESIZE


/*
 * This array converts virtual page protections to physical ones.
 */
uchar_t	hat_vprot2p[] = {
	MMU_STD_SRX, MMU_STD_SRX, MMU_STD_SRWX,
	MMU_STD_SRWX, MMU_STD_SRX, MMU_STD_SRX, MMU_STD_SRWX,
	MMU_STD_SRWX, MMU_STD_SRX, MMU_STD_SRXURX,
	MMU_STD_SRWXURWX, MMU_STD_SRWXURWX, MMU_STD_SRXURX,
	MMU_STD_SRXURX, MMU_STD_SRWXURWX, MMU_STD_SRWXURWX
};

/* And this one from physical to virtual */
uchar_t	hat_pprot2v[] = {
	PROT_READ | PROT_EXEC,
	PROT_READ | PROT_WRITE | PROT_EXEC,
	PROT_USER | PROT_READ | PROT_EXEC,
	PROT_USER | PROT_READ | PROT_WRITE | PROT_EXEC
};

int		syncnopmap;			/* ### stat ? */

int		hat_steal_enable = 1;
int		hat_stealcnt = 4;	/* number of hwpps/hmes to steal */
int		hat_page_stealcnt = 16;	/* number of pages to reclaim */

/* pps to search before giving up  when called from kmem* */
int		hat_searchcnt = 4;

/* hat statistics - must match enum in hat_i86.h */

kstat_named_t	x86hatstat[HATSTAT2_SZ] = {
	{ "cr3 load",				KSTAT_DATA_ULONG },
	{ "page table reclaimed",		KSTAT_DATA_ULONG },
	{ "page table alloc",			KSTAT_DATA_ULONG },
	{ "page table alloc with pagedir",	KSTAT_DATA_ULONG },
	{ "hat steal",				KSTAT_DATA_ULONG },
	{ "hat steal vmem",			KSTAT_DATA_ULONG },
	{ "hat steal kmem",			KSTAT_DATA_ULONG },
	{ "hat steal mem",			KSTAT_DATA_ULONG },
	{ "hat steal pteas",			KSTAT_DATA_ULONG },
	{ "hat steal restart",			KSTAT_DATA_ULONG },
	{ "hat steal abort",			KSTAT_DATA_ULONG },
	{ "hat setup",				KSTAT_DATA_ULONG },
	{ "hat setup with pagedir",		KSTAT_DATA_ULONG },
	{ "tlbflush wait",			KSTAT_DATA_ULONG },
	{ "tlbflush wait immediate",		KSTAT_DATA_ULONG },
	{ "capture all cpus",			KSTAT_DATA_ULONG },
	{ "capture cpus",			KSTAT_DATA_ULONG },
	{ "locked large page mapings",		KSTAT_DATA_ULONG },
	{ "locked 4k mappings",			KSTAT_DATA_ULONG },
	{ "physical memory allocated",		KSTAT_DATA_ULONG },
	{ "heap virtual memory allocated",	KSTAT_DATA_ULONG },
	{ "pteas virtual memory allocated",	KSTAT_DATA_ULONG },
	{ "per process pagedir",		KSTAT_DATA_ULONG },

	/*
	 * HATSTAT_SZ - above are stats kept in the cpu structures
	 * below are lock protected stat variables.
	 */

	{ "ppp cnt in ppp free list",		KSTAT_DATA_ULONG },
	{ "page cnt in page free list",		KSTAT_DATA_ULONG },
	{ "hat cnt in hat free list",		KSTAT_DATA_ULONG },
	{ "kernel memory allocated",		KSTAT_DATA_ULONG },
	{ "mlist sleep cnt",			KSTAT_DATA_ULONG },
	{ "mlist repeated sleep cnt",		KSTAT_DATA_ULONG },
	{ "hwpp free list cnt",			KSTAT_DATA_ULONG },
	{ "hwpplpte free list cnt",		KSTAT_DATA_ULONG }
};

/*
 * hatstat2 stats can be modified through direct access or may be modified
 * through HATSTAT2 macros:
 *	hat_kmemcount += sizeof (something);
 * 		- or -
 *	HATSTAT2_ADD(HATSTAT2_KMEM_CNT, sizeof (something));
 */
hatstat2src_t	x86hatstat2_src[HATSTAT2_SZ - HATSTAT_SZ] =
{
	&hat_pppcount,
	&hat_pagecount,
	&hat_hatcount,
	&hat_kmemcount,
	&hat_mlistsleepcount,
	&hat_mlistresleepcount,
	&hat_hwppcount,
	&hat_hwpplptecount
};

/*
 * provide the means to get from pte to hat without going through hwpp
 * (which may be freed while looking up the hwpp_hat)
 */
ushort_t	*pte2hati;

struct hat	**hatida;
ushort_t	hatidai;

/*
 * A macro used to access machpage fields
 */
#define	mach_pp	((machpage_t *)pp)

static void
load_cr3(cpup, cr3)
struct	cpu	*cpup;
uint_t	cr3;
{
	struct hat *cthat = ((cr3ctx_t *)cr3)->ct_hat;
	cpup->cpu_curcr3 = cr3;
	setcr3(((cr3ctx_t *)cr3)->ct_cr3);
	if (cthat) {
		atomic_orl((unsigned long *)&cthat->hat_cpusrunning,
			(unsigned long)cpup->cpu_mask);
	}
}

void
set_pageout_scanner_context()
{
	(void) i86_switch_hat(pteashat);
}

/*
 * Routine that is called from resume_from_intr to set cr3 of
 * thread that was just unpinned, if needed.
 */
void
hat_load_mmuctx(kthread_t *t)
{
	struct	cpu	*cpup = CPU;
	uint_t		cr3 = t->t_mmuctx;
	kthread_t	*t1 = t->t_intr;

	HATPRINTF(0x10, ("load_mmuctx: thr %x cr3ctx %x cpuctx%x\n", t, cr3,
		cpup->cpu_curcr3));
	while (!cr3) {
		if (t1) {
			cr3 = t1->t_mmuctx;
			t1 = t1->t_intr;
		} else
			cr3 = (uint_t)&cpup->cpu_ctx;
	}

	if (cpup->cpu_curcr3 == cr3) {
		/* ASSERT(cr3() == cr3->ct_cr3); race here wiht load_cr3 ? */
		return;
	}
	LOAD_CR3(cpup, cr3);
}

#define	HAT_STEALVMEM	0x1
#define	HAT_STEALMEM	0x2
#define	HAT_STEALKMEM	0x4
#define	HAT_STEALPTEAS	0x8

static void
hat_steal(int what)
{
	struct hat	*hat;
	struct hat	*curhat;
	int		stealcnt = 0;
	int		loopcnt = 0, looped = 0, skip;
	hwptepage_t	*hwpp, *hwppnext;
	int		oldhat;

	if (!hat_steal_enable)
		return;

	HATSTAT_INC(HATSTAT_STEAL);

	switch (what) {
	case HAT_STEALVMEM:
		HATSTAT_INC(HATSTAT_STEAL_VMEM);
		break;
	case HAT_STEALMEM:
		HATSTAT_INC(HATSTAT_STEAL_MEM);
		break;
	case HAT_STEALKMEM:
		HATSTAT_INC(HATSTAT_STEAL_KMEM);
		break;
	case HAT_STEALPTEAS:
		HATSTAT_INC(HATSTAT_STEAL_PTEAS);
		break;
	}

	mutex_enter(&hat_hat_lock);

	if (hat_searchp == &all_hat)
		hat_searchp = hat_searchp->hat_next;

	if (hat_searchp == &all_hat)
		goto stealdone;

	curhat = hat_searchp->hat_next;

	do {
		if (!loopcnt) {
			/*
			 * find candidate to steal from; bypass hats that
			 * are active
			 */
			for (; ; ) {
				while (curhat->hat_cpusrunning && !looped) {
					if (curhat == hat_searchp)
						looped++;
					curhat = curhat->hat_next;
				}
				if (curhat == &all_hat)
					curhat = curhat->hat_next;
				else
					break;
			}
		}
		hat = curhat;

		/*
		 * hat == &all_hat is true only if loopcnt > 0 in which case
		 * steal from hat_next even if cpusrunning is set
		 */
		if (hat == &all_hat)
			hat = hat->hat_next;

		curhat = hat->hat_next;

		if (hat == hat_searchp || looped) {

			/* looped thru all hats w/o satisfying hat_stealcnt */
			loopcnt++;
			looped = 0;

			if (loopcnt > 1) {
				HATSTAT_INC(HATSTAT_STEAL_ABORT);
				/*
				 * looped through a second time looking at all
				 * hats and still not satisfy hat_stealcnt
				 */
				break;
			}

			HATPRINTF(0x01, ("stealhwpp: loopcnt %x\n", loopcnt));

			/*
			 * free and reacquire the hat_hat_lock to give others
			 * a chance. Make sure we get a new hat if hat is freed
			 */
			hat_searchp = hat;

			mutex_exit(&hat_hat_lock);
			HATSTAT_INC(HATSTAT_STEAL_RESTART);

			mutex_enter(&hat_hat_lock);
			if (hat_searchp == &all_hat)
				hat_searchp = hat_searchp->hat_next;

			if (hat_searchp == &all_hat)
				goto stealdone;

			hat = hat_searchp;
			curhat = hat->hat_next;
		}

		if ((what == HAT_STEALKMEM) && hat->hat_pagedir && !loopcnt) {
			/* ppp is not likely to release KMEM */
			continue;
		}
		if (!mutex_tryenter(&hat->hat_mutex))
			continue;

		oldhat = i86_switch_hat(hat);

		hwpp = hat->hat_hwpp;
		while (hwpp) {
			uint_t		index = hwpp->hwpp_index;
			int		numptes;
			pteval_t	*pte;
			int		rm, lrm;
			page_t		*pp;

			hwppnext = hwpp->hwpp_next;

			/* skip hwpps already freed or locked */
			if (index == -1 || hwpp->hwpp_lockcnt ||
				!hwpp->hwpp_pte_pp) {
				hwpp = hwppnext;
				continue;
			}

			numptes = hwpp->hwpp_numptes;
			pte = hwpp->hwpp_lpte;

			HATPRINTF(0x01, ("stealhwpp: hat %x hwpp %x"
					"  numptes %x\n",
					hat, hwpp, numptes));

			hat->hat_rss -= numptes;
			hwpp->hwpp_numptes = 0;

			hat->hat_critical = hwppindextoaddr(index);
			/*
			 * invalidate associated page dir entry on
			 * all cpus.
			 */
			hat_update_pde(hat, index, MMU_STD_INVALIDPTE);

			/* sync up the RM bits */
			skip = 0;
			while (numptes) {
				while (!pte_valid(pte)) {
					pte++;
					ASSERT(btop((uint_t)pte) ==
						btop((uint_t)hwpp->hwpp_lpte));
				}
				numptes--;
				rm = *pte & PTE_RM_MASK;
				if (rm & PTE_REF_MASK)
					lrm = P_REF;
				else
					lrm = 0;
				if (rm & PTE_MOD_MASK)
					lrm |= P_MOD;
				pp = page_numtopp_nolock(PTEVAL2PFN(*pte));
				if (!pp || getpte_noconsist(pte)) {
					/*
					 * INVALIDATE_PTE not necessary here
					 * since page dir invalidated
					 */
					*pte = MMU_STD_INVALIDPTE;
					pte++;
					continue;
				}
				if (!getpte_nosync(pte)) {
					PP_SETRM(pp, lrm);
				}
				if (i86_mlist_tryenter(pp)) {
					pteval_t	*pteaspte;
					pteaspte  = hwpp->hwpp_pte +
						(pte - hwpp->hwpp_lpte);
					*pte = MMU_STD_INVALIDPTE;
					hme_sub(pp, pteaspte, NOHOLDMLIST);
				} else {
					skip++;
				}
				pte++;
			}

			hat->hat_critical = HAT_INVALID_ADDR;

			/*
			 * free hwpp and resources - set updatepde
			 * false. Cannot just call hat_hwppfree with
			 * updatepde true since hwpp is freed before
			 * call to hat_update_pde with assumption that
			 * ptes are already unloaded.
			 */
			if (skip) {
				hat->hat_rss += skip;
				hwpp->hwpp_numptes = skip;
			} else
				hat_hwppfree(hat, hwpp, 0);

			stealcnt++;

			hwpp = hwppnext;
		}
		i86_restore_hat(oldhat);
		mutex_exit(&hat->hat_mutex);

	} while (stealcnt < hat_stealcnt);

	hat_searchp = curhat;

stealdone:

	mutex_exit(&hat_hat_lock);
	if (what == HAT_STEALMEM) {
		hat_page_cache_reclaim();
		hat_hwpp_cache_reclaim(HWPPLIST);
	} else {
		hat_hwpp_cache_reclaim(HWPPLIST | HWPPLPTELIST);
	}
}

void
hat_vmem_free(vmem_t *arena, void *addr, size_t sz)
{
	vmem_free(arena, addr, sz);

	if (arena == heap_arena)
		HATSTAT_SUB(HATSTAT_VIRTMEMALLOC_HEAP, sz);
	else {
		if (pteashiwater) {
			if (vmem_size(arena, VMEM_FREE) > pteashiwater)	{
				/* reset to original values */
				pteashiwater = 0;
				hat_perprocess_pagedir = getpppthresh();
				pteaslowater = PTEASLO();
			}
		}
		HATSTAT_SUB(HATSTAT_VIRTMEMALLOC_PTEAS, sz);
	}
}


void *
hat_vmem_alloc(vmem_t *arena, size_t size)
{
	void	*addr;
	addr = vmem_alloc(arena, size, VM_NOSLEEP);
	while (addr == NULL) {
		if (arena == heap_arena)
			hat_steal(HAT_STEALKMEM);
		else
			hat_steal(HAT_STEALPTEAS);

		addr = vmem_alloc(arena, size, VM_NOSLEEP);
	}
	if (arena == heap_arena)
		HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_HEAP, size);
	else {
		HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_PTEAS, size);

		if (vmem_size(arena, VMEM_FREE) < pteaslowater) {
			hat_perprocess_pagedir =
				min(hat_perprocess_pagedir_max,
					hat_perprocess_pagedir * 2);

			if (!pteashiwater)
				pteashiwater = PTEASHI();

			HATPRINTF(0x01, ("vmem_alloc: lower ppp %x\n",
					hat_perprocess_pagedir));

			if (hat_perprocess_pagedir < hat_perprocess_pagedir_max)
				pteaslowater /= 2;
			else
				pteaslowater = 0;
		}
	}
	return (addr);
}

void *
hat_kmem_alloc(size_t size)
{
	void	*addr;
	addr = kmem_alloc(size, KM_NOSLEEP);
	while (addr == NULL) {
		hat_steal(HAT_STEALKMEM);
		addr = kmem_alloc(size, KM_NOSLEEP);
	}

	HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_HEAP, size);
	HATSTAT_ADD(HATSTAT_PHYSMEMALLOC, size);
	return (addr);
}

void *
hat_kmem_zalloc(size_t size)
{
	void	*addr;
	addr = kmem_zalloc(size, KM_NOSLEEP);
	while (addr == NULL) {
		hat_steal(HAT_STEALKMEM);
		addr = kmem_zalloc(size, KM_NOSLEEP);
	}

	HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_HEAP, size);
	HATSTAT_ADD(HATSTAT_PHYSMEMALLOC, size);
	return (addr);
}

static hwptepage_t *
hat_hwppfind(hat, hwpp_index)
struct hat *hat;
uint_t	hwpp_index;	/* Index in userhwppmap */
{
	hwptepage_t *hwpp;

	ASSERT(hwppindextoaddr(hwpp_index) < usermapend);

	if (hat->hat_pagedir)
		return (userhwppmap[hwpp_index]);

	hwpp = hat->hat_hwpp;
	while (hwpp) {
		if (hwpp->hwpp_index == hwpp_index)
			break;
		hwpp = hwpp->hwpp_next;
	}
	return (hwpp);
}

/*
 * Construct a pte for a page. Called from ppmapin, ppzero.
 */
void
hat_mempte(register struct page *pp, uint_t vprot, pteval_t *pte, caddr_t addr)
{
#ifdef lint
	addr = addr;
#endif
	*pte = PTEOF_C(page_pptonum(pp), hat_vprot2p[vprot & HAT_PROT_MASK]);
}


/*
 * Switch the thread to use the hat's cr3 so as to map in all page tables.
 * set mmuctx so as to move the cr3 if thread migrates.
 */
static int
i86_switch_hat(struct hat *hat)
{
	kthread_t 	*t;
	uint_t		oldcr3;
	cr3ctx_t	*newcr3;
	struct	hat	*oldhat;
	struct	cpu	*cpup;

	newcr3 = &hat->hat_ctx;
	if (!newcr3->ct_hat)
		return (HAT_NOT_SWITCHED);

	t = curthread;
	oldcr3 = t->t_mmuctx;

	HATPRINTF(0x01, ("switch_hat: thr %x oldcr3 %x newcr3 %x caller %x\n",
			t, oldcr3, newcr3, caller()));
	if ((oldcr3) == (uint_t)newcr3) {
		ASSERT(cr3() == newcr3->ct_cr3);
		return (HAT_NOT_SWITCHED);
	}

	kpreempt_disable();
	cpup = CPU;
	if (!oldcr3) {
		/*
		 * We are a kernel thread or it is the first time with
		 * a per process page dir (on a diff thread from the one
		 * which did switch2pp.)
		 */
		if ((uint_t)&cpup->cpu_ctx == cpup->cpu_curcr3) {
			/* return cr3 if ppp. Else 0 */
			oldhat = t->t_procp->p_as->a_hat;
			if (oldhat && oldhat->hat_pagedir)
				oldcr3 = (uint_t)&oldhat->hat_ctx;
		} else {
			ASSERT(t->t_flag & T_INTR_THREAD);
			oldcr3 = cpup->cpu_curcr3;
		}
	}

	t->t_mmuctx = (uint_t)newcr3;

	LOAD_CR3(cpup, newcr3);
	kpreempt_enable();

	return (oldcr3);
}


/*
 * switch back to the process' regular cr3 (hat_pdepfn for ppp
 * and cpu_cr3 for others)
 */
static void
i86_restore_hat(uint_t oldcr3)
{
	kthread_t	*t;
	struct	cpu	*cpup;
	struct	hat	*hat;

	if (oldcr3 == HAT_NOT_SWITCHED) {
		return;
	}

	t = curthread;

	HATPRINTF(0x01, ("restore_hat: thr %x oldcr3 %x ctxcr3 %x caller %x\n",
			t, oldcr3, t->t_mmuctx, caller()));

	kpreempt_disable();
	cpup = CPU;
	if ((!oldcr3) || (((cr3ctx_t *)oldcr3)->ct_hat == NULL)) {
		t->t_mmuctx = 0;
		LOAD_CR3(cpup, &cpup->cpu_ctx);
		kpreempt_enable();
		return;
	}

	t->t_mmuctx = oldcr3;

	if ((hat = ((cr3ctx_t *)cpup->cpu_curcr3)->ct_hat) != NULL) {
		atomic_andl((unsigned long *)&hat->hat_cpusrunning,
			~(unsigned long)cpup->cpu_mask);
	}
	LOAD_CR3(cpup, oldcr3);
	kpreempt_enable();

}

#ifdef	DEBUG
int hat_checkchain = 0;
int breakme;

static void
hme_checkchain(page_t *pp)
{
	struct hment	*hmecur;
	int		expected, actual = 0;

	expected = mach_pp->p_deleted + mach_pp->p_share;
	if (IS_PTEPTR((uint32_t)mach_pp->p_mapping)) {
		ASSERT(expected == 1);
		return;
	}
	if (hat_checkchain & 0x2 && pteashat) {
		struct hment	*hmefree = NULL;
		int		share = 0, deleted = 0;
		pteval_t	*pte;
		uint_t		pfn = mach_pp->p_pagenum;
		int		oldhat;
		int		ndel;

		oldhat = i86_switch_hat(pteashat);
		ndel = hme_purge_dups(pp, &hmefree);

		if (ndel > mach_pp->p_deleted) {
			breakme++;
			mach_pp->p_deleted = 0;
			mach_pp->p_share -= (ndel - mach_pp->p_deleted);
		} else
			mach_pp->p_deleted -= ndel;

		expected = mach_pp->p_deleted + mach_pp->p_share;

		hmecur = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
		while (hmecur) {
			pte = hmecur->hme_pte;
			if (!pte_valid(pte) || PTEVAL2PFN(*pte) != pfn)
				deleted++;
			else
				share++;
			hmecur = hmecur->hme_next;
		}
		HMEFREE(hmefree);

		if (share != mach_pp->p_share ||
			deleted != mach_pp->p_deleted || actual != expected)
			breakme++;

		ASSERT(share == mach_pp->p_share);
		ASSERT(deleted == mach_pp->p_deleted);

		i86_restore_hat(oldhat);

	} else {
		hmecur = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
		while (hmecur) {
			actual++;
			hmecur = hmecur->hme_next;
		}
		ASSERT(actual == expected);
	}
}

#endif

/*
 * called from page_coloring_init
 */

void
i86_mlist_init()
{
	int	i;

	for (i = 0; i < PSMHASHMAX; i++) {
		mutex_init(mlistlock + i, NULL, MUTEX_DEFAULT, NULL);
		cv_init(mlistcondvar + i, NULL, CV_DEFAULT, NULL);
	}
}

/*
 * protect pp when we are deleting hmes from the mapping list or
 * scanning it
 */
static void
i86_mlist_enter(page_t *pp)
{
	kmutex_t	*pmtx;
	int		repeat = 0;
	uchar_t		oldflag, newflag;

	oldflag = mach_pp->p_flags;
	if (!(oldflag & P_MLISTLOCKED)) {
		newflag = oldflag | P_MLISTLOCKED;
		if (cas8(&mach_pp->p_flags, oldflag, newflag) == oldflag) {
#ifdef DEBUG
			if (hat_checkchain)
				hme_checkchain(pp);
#endif
			return;
		}
	}
	pmtx = PMTX_HASH(pp);
	mutex_enter(pmtx);
	atomic_orb(&mach_pp->p_flags, P_MLISTWAIT);
	oldflag = mach_pp->p_flags;
	/* Try once more after setting WAIT */
	newflag = oldflag | P_MLISTLOCKED;
	while ((oldflag & P_MLISTLOCKED) ||
		cas8(&mach_pp->p_flags, oldflag, newflag) != oldflag) {
		/*
		 * we depend on the fact that cv_wait will not make
		 * calls like kmem_alloc which could cause another mutex_enter
		 * on pmtx causing a deadlock.
		 */
		if ((mach_pp->p_flags & (P_MLISTWAIT | P_MLISTLOCKED)) ==
			(P_MLISTWAIT | P_MLISTLOCKED)) {
			cv_wait(PCV_HASH(pp), pmtx);
			hat_mlistsleepcount++;
			if (repeat++)
				hat_mlistresleepcount++;
		}
		atomic_orb(&mach_pp->p_flags, P_MLISTWAIT);
		oldflag = mach_pp->p_flags;
		/* Try once more after setting WAIT */
		newflag = oldflag | P_MLISTLOCKED;
	}
	mutex_exit(pmtx);

#ifdef DEBUG
	if (hat_checkchain)
		hme_checkchain(pp);
#endif
}

static int
i86_mlist_tryenter(page_t *pp)
{
	uchar_t		oldflag, newflag;

	oldflag = mach_pp->p_flags;
	if (!(oldflag & P_MLISTLOCKED)) {
		newflag = oldflag | P_MLISTLOCKED;
		if (cas8(&mach_pp->p_flags, oldflag, newflag) == oldflag)
			return (1);
	}
	return (0);

}


static void
i86_mlist_exit(page_t *pp)
{
	kmutex_t	*pmtx;

#ifdef DEBUG
	if (hat_checkchain)
		hme_checkchain(pp);
#endif
	atomic_andb(&mach_pp->p_flags, ~P_MLISTLOCKED);

	if (!(mach_pp->p_flags & P_MLISTWAIT))
		return;

	pmtx = PMTX_HASH(pp);
	mutex_enter(pmtx);
	if (mach_pp->p_flags & P_MLISTWAIT) {
		atomic_andb(&mach_pp->p_flags, ~P_MLISTWAIT);
		cv_broadcast(PCV_HASH(pp));
	}
	mutex_exit(pmtx);
}

/*
 * Find hat and addr for given pte. Lock hat based on lockflag and
 * return. If TRYLOCK is passed in and we cannot lock, we return
 * NULL for hat & -1 for addr. If pte does not map this pfn anymore, both hat
 * and addr are returned as NULL. If DONTLOCK, don't bother to return address.
 */
struct hat *
pte2hat_addr(pte, pfn, addrp, lockflag)
	pteval_t *pte;
	int	pfn;
	caddr_t	*addrp;
	int	lockflag;
{
	/*
	 * kptes are above kernelbase since kmem_alloc'ed; user pte below
	 * kernelbase - userptemap or pteas.
	 */
	uint_t			pteindex, hati;
	struct hwptepage	*hwpp;
	struct hat 		*hat;

	if ((uint_t)pte > (uint_t)kernelbase) {
		pteindex = ((uint_t)pte - (uint_t)kptes)/sizeof (*pte);
		ASSERT(pteindex < kernel_pts * NPTEPERPT);
		/* We can ignore lockflag for kernel_addresses */
		if (pteindex < eecontigindex)
			*addrp = (caddr_t)(kernelbase + ptob(pteindex));
		else
			*addrp = (caddr_t) (SEGKMAP_START +
					ptob(pteindex - eecontigindex));
		return (kernel_hat);
	}

	/* user pte */
	ASSERT(cr3() == pteas_cr3);
	*addrp = NULL;
	/*
	 * Dereferencing pte like this is safe. The addr is never made invalid.
	 * It is just made to point to a page of invalid ptes on freeing.
	 */
	if ((PTEVAL2PFN(*pte) != pfn) || !pte_valid(pte)) {
		return (NULL);
	}
	/* Find index into the pt_hwpp array. There is one for each pte page */
	pteindex = btop((uintptr_t)pte);

	hati = pte2hati[pteindex];
	hat = hatida[hati];

	if (!hat)
		return (NULL);

	ASSERT(hat->hat_index == hati);
	ASSERT(hat != kernel_hat);

	if (mutex_tryenter(&hat->hat_mutex) == 0) {
		if (lockflag == TRYLOCK) {
			/* indicate failure of tryenter */
			*addrp = HAT_INVALID_ADDR;
			return (NULL);
		}
		mutex_enter(&hat->hat_mutex);
	}
	hwpp = pt_hwpp[pteindex];
	if (!hwpp || (hat != hwpp->hwpp_hat)) {
		mutex_exit(&hat->hat_mutex);
		return (NULL);
	}
	/*
	 * Make sure we get new xlation in case it has just been
	 * changed on some other cpu
	 */
	mmu_tlbflush_entry((caddr_t)pte);

	if ((PTEVAL2PFN(*pte) != pfn) || !pte_valid(pte)) {
		mutex_exit(&hat->hat_mutex);
		return (NULL);
	}
	*addrp = (caddr_t)hwppindextoaddr(hwpp->hwpp_index) +
		ptob(pte - hwpp->hwpp_pte);
	return (hat);
}

static void
hme_cache_free(hme)
struct hment *hme;
{
	if ((hme >= khmes) && (hme < ekhmes)) {
		hme->hme_next = NULL;
		return;
	}
	kmem_cache_free(hat_hmecache, hme);
}

/*
 * Cache reclaim routine that kmem allocator will callback into when
 * system is running low on memory. We try and free up as much as we
 * can
 * Set hmereap_watermark to 0 (or v. low), go thru the hat list, try_enter
 * each hat. For each successful mutex_enter, go thru hwpp list, and do
 * hwppunload on each hwpp without lockcnt. keep stats on hats scanned
 * and hwpps unloaded.
 */

static void
hat_hmecache_reclaim(void *cdrarg)
{
	int	stealcnt = 0;
	int	searchcnt = 0;
	int	oldcnt, oldhat, loopcnt = 0;
	page_t	*pp;

	HATPRINTF(8, ("hat_hmecache_reclaim: freemem %x %x\n",
		freemem, cdrarg));


	if (in_hat_hme_reclaim)
		return;
	in_hat_hme_reclaim = 1;

	/*
	 * if we are called because we are low on kernel virtual memory,
	 * the only thing which will really help is freeing hwpplpte cache.
	 * If we have nothing in that cache, try hat_steal.
	 */
	if (!hat_hwpplptecount)
		hat_steal(HAT_STEALKMEM);
	hat_page_cache_reclaim();
	hat_hwpp_cache_reclaim(HWPPLIST | HWPPLPTELIST);

	if (!page_searchp)
		page_searchp = page_first();
	pp = page_next(page_searchp);
	oldhat = i86_switch_hat(pteashat);
	while (stealcnt < hat_stealcnt) {
		if ((mach_pp->p_mapping) &&
			(IS_HMEPTR((uint32_t)mach_pp->p_mapping))) {
			/* Don't worry about freak cases first time around */
			if (mach_pp->p_deleted || loopcnt) {
				oldcnt = mach_pp->p_deleted;
				if (i86_mlist_tryenter(pp))
					/* hme_purge frees the mlist_lock */
					hme_purge(pp, NULL);
				stealcnt += (oldcnt - mach_pp->p_deleted);
			}
		}
		if ((cdrarg != (void *)1) && (searchcnt++ > hat_searchcnt))
			/* limit search if it is not an internal request */
			break;
		pp = page_next(pp);
		if (pp == page_searchp) {
			if (loopcnt++ > 1)
				break;
		}
	}
	page_searchp = pp;
	in_hat_hme_reclaim = 0;
	i86_restore_hat(oldhat);
}


void
hme_add(pp, pte, ro)
	page_t		*pp;
	pteval_t	*pte;
	int		ro;
{
	struct hment	*hme1 = NULL, *hme2 = NULL;
	uint32_t	mapval;
	uint32_t	ppmapping;

	ASSERT(((uint_t)pte < (uint_t)userptemap) ||
	    (((uint_t)pte - (uint_t)kptes)/sizeof (*pte) <
					kernel_pts * NPTEPERPT));

	ASSERT(pte);

	ASSERT(I86_MLIST_LOCKED(mach_pp));

	if ((pte >= kheappte) && (pte < ekheappte)) {
		hme1 = &khmes[pte - kheappte];
		ASSERT(hme1 < ekhmes);
		ppmapping = PPMAPPING(hme1, 0, ro);
		hme1->hme_pte = pte;
		ASSERT(hme1->hme_next == NULL);
	} else {
		ppmapping = PPMAPPING(pte, 1, ro);
	}

	mach_pp->p_share++;

	mapval = (uint32_t)mach_pp->p_mapping;
	if (mapval == NULL) {
		ASSERT(!mach_pp->p_deleted);
		ASSERT(mach_pp->p_share == 1);
		mach_pp->p_mapping = (struct hment *)ppmapping;
		return;
	}

	if (!hme1) {
		/* If not khme, alloc one now */
		hme1 = kmem_cache_alloc(hat_hmecache, KM_NOSLEEP);
		while (hme1 == NULL) {
			hat_hmecache_reclaim((void *)1);
			hme1 = kmem_cache_alloc(hat_hmecache, KM_NOSLEEP);
		}
	}
	hme1->hme_pte = pte;

	hme1->hme_next = (struct hment *)PPMAPPTR(mapval);

	if (IS_HMEPTR(mapval)) {
		mach_pp->p_mapping = (struct hment *) PPMAPPING(hme1, 0,
						ro && IS_ROC(mapval));
		return;
	}
	/*
	 * IS_HMEPTR above failed so p_mapping is a pte pointer and
	 * hme1->hme_next is not proper and we need another hme
	 * to point to the pte originally pointed to by p_mapping.
	 */
	ASSERT(mach_pp->p_share == 2);
	hme2 = kmem_cache_alloc(hat_hmecache, KM_NOSLEEP);
	while (hme2 == NULL) {
		hat_hmecache_reclaim((void *)1);
		hme2 = kmem_cache_alloc(hat_hmecache, KM_NOSLEEP);
	}
	hme2->hme_pte = (pteval_t *)PPMAPPTR(mapval);
	hme2->hme_next = NULL;
	hme1->hme_next = hme2;
	mach_pp->p_mapping = (struct hment *)PPMAPPING(hme1, 0,
						ro && IS_ROC(mapval));
}

/*
 * Get the next valid hme in pp's mapping list. Called with mlist held
 * for this pp. All invalid hmes in between are freed.
 * BUT the hme is not guaranteed to be valid till the caller
 * uses it. Caller's responsibility to grab hat lock and recheck.
 * May return pte pointer if only 1 mapping exists.
 */
static struct hment *
hme_getnext(pp, hmein, hmefree)
	page_t *pp;
	struct hment	*hmein;
	struct hment	**hmefree;
{
	pteval_t	*pte;
	int		pfn = mach_pp->p_pagenum;
	struct hment	*hmeprev, *hmecur, *hmetmp, **hmepp;
	int		isroc, ndel;

	ASSERT(cr3() == pteas_cr3);
	ASSERT(I86_MLIST_LOCKED(mach_pp));

	if (IS_PTEPTR((uint32_t)hmein))
		return (NULL);
	if (hmein == NULL) {
		hmecur = mach_pp->p_mapping;
		if (PPMAPPTR(hmecur) == NULL)
			return (NULL);
		/* If pteptr, unlikely to be an invalid mapping */
		if (IS_PTEPTR(hmecur)) {
			ASSERT(!mach_pp->p_deleted);
			ASSERT(mach_pp->p_share == 1);
			return ((struct hment *)hmecur);
		}
		isroc = IS_ROC(mach_pp->p_mapping);
		hmecur = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
		hmepp = (struct hment **)&mach_pp->p_mapping;
	} else {
		hmepp = &hmein->hme_next;
		hmecur = hmein->hme_next;
		if (!hmecur)
			return (NULL);
		isroc = 0;		/* middle of chain should not be ro */
	}

	ndel = mach_pp->p_deleted;
	ASSERT(ndel >= 0);
	hmeprev = hmecur;
	hmetmp = NULL;
	pte = hmecur->hme_pte;
	while ((PTEVAL2PFN(*pte) != pfn) || !pte_valid(pte)) {

		hmetmp = hmecur;	/* save last invalid hme */
		ndel--;

		hmecur = hmecur->hme_next;
		if (!hmecur) {
			isroc = 0;	/* No roc for NULL mapping */
			break;
		}
		pte = hmecur->hme_pte;
	}

	ASSERT(ndel >= 0);
	mach_pp->p_deleted = (ushort_t) ndel;

	/* Delete invalid hmes if any */

	*hmepp = (struct hment *)PPMAPPING(hmecur, 0, isroc);

	if (hmetmp && (hmeprev != hmecur)) {
		hmetmp->hme_next = *hmefree;
		*hmefree = hmeprev;
	}
	return (hmecur);
}


/*
 * remove a hme from the mapping list for page pp.
 * We just increment p_deleted and leave it at that unless
 * there are lots of deleted hmes or it is a khme that we are deleting.
 */
static void
hme_sub(page_t *pp, pteval_t *pte, int holdmlist)
{
	struct hment	*hme1, *hmefree;
	uint_t		mapval, ndel;

	ASSERT(I86_MLIST_LOCKED(mach_pp));

	mapval = PPMAPPTR(mach_pp->p_mapping);
	if (pte == (pteval_t *)mapval) {
		/* This is the first mapping. nuke it */
		mach_pp->p_mapping = NULL;
		ASSERT(mach_pp->p_share == 1);
		mach_pp->p_share = 0;
		ASSERT(mach_pp->p_deleted == 0);
		if (!holdmlist)
			i86_mlist_exit(pp);
		return;
	}

	if ((pte >= kheappte) && (pte < ekheappte)) {
		/* It is a khme. Make sure it is deleted */
		hme1 = &khmes[pte - kheappte];
	} else
		hme1 = NULL;

	ASSERT(mach_pp->p_share);
	mach_pp->p_share--;
	ndel = ++mach_pp->p_deleted;
	if (!mapval) {
		/* some other thread has deleted for us already */
		if (!holdmlist)
			i86_mlist_exit(pp);
		return;
	}
	ASSERT(!IS_PTEPTR(mach_pp->p_mapping));


	/* Don't purge if caller expects the chain to be preserved */
	if (holdmlist)
		return;

	if (!mach_pp->p_share) {
		/* Do a fast delete if all of them are invalid */
		hmefree = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
		mach_pp->p_mapping = NULL;
		mach_pp->p_deleted = 0;
		i86_mlist_exit(pp);
		HMEFREE(hmefree);
		return;
	}

	/*
	 * If there arent lowatermark more invalid mappings than valid,
	 * the watermark has not been reached & it is not a khme,
	 * we can return.
	 */
	if (((mach_pp->p_share + hat_hmereap_lowatermark) > ndel) &&
		(ndel < hat_hmereap_hiwatermark) && !hme1) {
		i86_mlist_exit(pp);
		return;
	}

	hme_purge(pp, hme1);
}

/*
 * last 12 bits of pte address is not interesting to us
 * as the same page will tend to get mapped at the same
 * virtual address & hence the same offset within pagetable
 */
#define	HPHASH(pte, hashmask) (((uint_t)pte >> 12) & hashmask)

/*
 * Purge the mapping list for pp of duplicate hmes (those that
 * point to the same pte) and return the number of entries deleted.
 * We do this by (bucket) sorting the hmes based on their pte address
 * and dropping duplicates as we sort into buckets.
 */
static int
hme_purge_dups(pp, hmefree)
	page_t	*pp;
	struct hment **hmefree;
{
	struct hment	**hmepp, **hhbtp, **hhbhp, *hmecur, *hmenext;
	struct hment	*hmetail, *hmehead;
	uint_t		ndel = 0, i;
	uint_t		hhbps, hhbpi; /* hme hash bucket size & index */
	uint_t		hashmask;
	pteval_t	*ppte, *cpte, *lastpte;

	ASSERT(I86_MLIST_LOCKED(mach_pp));

	/* Alloc a bucket for every 4 - 8 valid hme */
	if (mach_pp->p_share >= 8) {
		hhbps = 1 << (highbit(mach_pp->p_share) - 3);
		hhbtp = kmem_zalloc(sizeof (hhbtp) * hhbps * 2, KM_NOSLEEP);
		if (hhbtp == NULL)
			return (0);
		hhbhp = &hhbtp[hhbps];
	} else {
		hmetail = hmehead = NULL;
		hhbtp = &hmetail;
		hhbhp = &hmehead;
		hhbps = 1;
	}
	hashmask = hhbps - 1;
	hmecur = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
	while (hmecur) {
		cpte = hmecur->hme_pte;
		hhbpi = HPHASH(cpte, hashmask);
		ASSERT(hhbpi < hhbps);
		if (hhbtp[hhbpi] == NULL) {
			hhbtp[hhbpi] = hhbhp[hhbpi] = hmecur;
			/* No need to NULL next as we never go past tail */
			hmecur = hmecur->hme_next;
			continue;
		}
		/*
		 * First check if beyond or equal to tail.
		 * May be true frequently due to a previous sort.
		 */
		lastpte = hhbtp[hhbpi]->hme_pte;
		if (lastpte == cpte) {
			/* delete current one */
			hmenext = hmecur->hme_next;
			hmecur->hme_next = *hmefree;
			*hmefree = hmecur;
			ndel++;
			hmecur = hmenext;
			continue;
		}
		if (cpte > lastpte) {
			/* link to the end */
			hhbtp[hhbpi]->hme_next = hmecur;
			hhbtp[hhbpi] = hmecur;
			hmecur = hmecur->hme_next;
			continue;
		} else {
			hmepp = &hhbhp[hhbpi];
			hmenext = hhbhp[hhbpi];
			ppte = hmenext->hme_pte;
			while (cpte >= ppte) {
				/* we know this will terminate b4 lastpte */
				if (cpte == ppte) {
					hmenext = hmecur->hme_next;
					hmecur->hme_next = *hmefree;
					*hmefree = hmecur;
					ndel++;
					hmecur = hmenext;
					break;
				}
				hmepp = &hmenext->hme_next;
				hmenext = hmenext->hme_next;
				ppte = hmenext->hme_pte;
			}
			if (hmecur != hmenext) {
				struct hment	*hmetmp;
				/* no deletion took place */
				hmetmp = hmecur->hme_next;
				hmecur->hme_next = hmenext;
				*hmepp = hmecur;
				hmecur = hmetmp;
			}
		}
	}
	hmepp = &mach_pp->p_mapping;
	/* Now merge all of them */
	for (i = 0; i < hhbps; i++) {
		if (hhbtp[i] == NULL)
			continue;
		*hmepp = hhbhp[i];
		hmepp = &hhbtp[i]->hme_next;
	}
	*hmepp = NULL;
	if (hhbtp != &hmetail)
		kmem_free(hhbtp, sizeof (hhbtp) * hhbps * 2);

	HATPRINTF(4, ("purdup: pp %x ndel %x\n", pp, ndel));

	ASSERT(ndel <= mach_pp->p_deleted);

	return (ndel);
}

/*
 * Go thru the mapping list & purge the invalid hmes.
 * Called with mlist held.
 */
static void
hme_purge(page_t *pp, struct hment *hme)
{
	int		pfn = mach_pp->p_pagenum;
	struct hment	*hmeprev, *hmetmp, *hmecur, *hmefree = NULL;
	int		isroc, ndel;
	pteval_t	*cpte;
	struct hment	**hmepp;
	int		oldhat;

#ifdef lint
	hme = hme;
#endif

	ASSERT(I86_MLIST_LOCKED(mach_pp));

	hmeprev = hmecur = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
	if (!hmecur || IS_PTEPTR(mach_pp->p_mapping)) {
		i86_mlist_exit(pp);
		return;
	}

	isroc = IS_ROC(mach_pp->p_mapping);
	oldhat = i86_switch_hat(pteashat);
	hmepp = (struct hment **)&mach_pp->p_mapping;
	ndel = mach_pp->p_deleted;
	cpte = hmecur->hme_pte;
	for (; ; ) {
		while ((PTEVAL2PFN(*cpte) != pfn) || !pte_valid(cpte)) {
			hmetmp = hmecur;	/* save last invalid hme */
			hmecur = hmecur->hme_next;
			if ((!--ndel) || (!hmecur))
				break;
			cpte = hmecur->hme_pte;
		}

		/* Delete invalid hmes if any */

		if (hmeprev != hmecur) {
			hmetmp->hme_next = hmefree;
			hmefree = hmeprev;
			*hmepp = hmecur;
		}

		if (!ndel)
			break;

		/*
		 * skip through valid ptes.
		 */
		while (hmecur) {
			cpte = hmecur->hme_pte;
			if (pte_valid(cpte) && PTEVAL2PFN(*cpte) == pfn) {
				hmepp = &hmecur->hme_next;
				hmecur = hmecur->hme_next;
			} else {
				hmeprev = hmecur;
				break;
			}
		}
		if (!hmecur)
			break;
	}

	ASSERT(ndel <= hat_hmereap_hiwatermark);

	if (ndel && ((ndel > hat_hmereap_hiwatermark/2) ||
		(((mach_pp->p_share + hat_hmereap_lowatermark)/2) < ndel))) {
		ndel -= hme_purge_dups(pp, &hmefree);
	}

	if (isroc && mach_pp->p_mapping)
		mach_pp->p_mapping = (struct hment *)
					PPMAPPING(mach_pp->p_mapping, 0, isroc);
	HATPRINTF(4, ("purge: pp %x p_del %x ndel %x\n", pp, mach_pp->p_deleted,
		ndel));
	mach_pp->p_deleted = (ushort_t) ndel;
	ASSERT(mach_pp->p_deleted <= hat_hmereap_hiwatermark);
	i86_mlist_exit(pp);
	HMEFREE(hmefree);
	i86_restore_hat(oldhat);
}

uint_t
hat_pagesync(struct page *pp, uint_t clrflg)
{
	caddr_t		addr;
	int		oldhat;
	struct	hat	*hat;
	struct hment	*hmecur, *hmefree;
	uint32_t	zeroflag;
	int		pfn = mach_pp->p_pagenum;
	pteval_t	*pte;
	uint32_t	mapval;

	mapval = (uint32_t)mach_pp->p_mapping;

	if (mapval == NULL)
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));

	ASSERT(PAGE_LOCKED(pp) || panicstr);

	if (IS_ROC(mapval) && (clrflg & HAT_SYNC_STOPON_MOD)) {
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}

	if ((clrflg == (HAT_SYNC_STOPON_REF | HAT_SYNC_DONTZERO)) &&
		PP_ISREF(pp)) {
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}

	if ((clrflg == (HAT_SYNC_STOPON_MOD | HAT_SYNC_DONTZERO)) &&
		PP_ISMOD(pp)) {
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}

	if (mach_pp->p_share > po_share &&
	    !(clrflg & HAT_SYNC_ZERORM)) {
		if (PP_ISRO(pp))
			PP_SETREF(pp);
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}

	zeroflag = clrflg & ~HAT_SYNC_STOPON_RM;
	hmecur = NULL;
	hmefree = NULL;


	i86_mlist_enter(pp);
	if (!mach_pp->p_mapping) {
		ASSERT(mach_pp->p_share == 0);
		i86_mlist_exit(pp);
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}
	oldhat = i86_switch_hat(pteashat);
	HATPRINTF(4, ("hat_pagesync: oldhat %x page %x flag %x\n",
			curthread->t_procp->p_as->a_hat, pp, clrflg));
	/* If we have lots of deleted ones, maybe better off purging dups */
	if (mach_pp->p_share < mach_pp->p_deleted)
		mach_pp->p_deleted -= hme_purge_dups(pp, &hmefree);
	while (hmecur = hme_getnext(pp, hmecur, &hmefree)) {
		if (IS_PTEPTR((uint32_t)hmecur))
			pte = (pteval_t *)PPMAPPTR((uint32_t)hmecur);
		else
			pte = hmecur->hme_pte;
		if (!(hat = pte2hat_addr(pte, pfn, &addr, TRYLOCK))) {
			if (addr == NULL)
				continue;
			/*
			 * drop the mlist lock & then grab the hat lock
			 */
			i86_mlist_exit(pp);
			HMEFREE(hmefree);
			hmefree = NULL;
			hmecur = NULL;
			hat = pte2hat_addr(pte, pfn, &addr, LOCK);
			i86_mlist_enter(pp);
			if (hat == NULL)
				continue;
		}
		if ((PTEVAL2PFN(*pte) != pfn) || !pte_valid(pte))
			continue;

		hat_ptesync(hat, pp, addr, pte,
				zeroflag ? HAT_RMSYNC : HAT_RMSTAT, MLISTHELD);

		if (hat != kernel_hat)
			mutex_exit(&hat->hat_mutex);
		/*
		 * If clrflg is HAT_DONTZERO, break out as soon
		 * as the "ref" or "mod" is set.
		 */
		if ((zeroflag == HAT_SYNC_DONTZERO) &&
			(((clrflg & HAT_SYNC_STOPON_MOD) && PP_ISMOD(pp)) ||
			((clrflg & HAT_SYNC_STOPON_REF) && PP_ISREF(pp))))
			break;
	}

	i86_mlist_exit(pp);
	HMEFREE(hmefree);
	i86_restore_hat(oldhat);
	return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
}
ulong_t
hat_page_getshare(pp)
	page_t *pp;
{
	return (mach_pp->p_share);
}

#ifdef	PTE36
/*
 * Allocate page directory table entries. Should be 32 bytes long
 * and aligned to 32 byte address. Fill enough entries to make
 * the kernel entries valid in pdir.
 */
static pteptr_t
hat_alloc_pttblent(pteptr_t pdir)
{
	pteptr_t	pttblent;
	caddr_t		addr;
	uint_t		i;

	if (kvseg.s_base) {
		(void) i_ddi_mem_alloc(NULL, &lt4gb_attr,
			sizeof (pteval_t) * NPDPERAS, 1, 0, NULL,
			(caddr_t *)&pttblent, NULL, NULL);

		HATSTAT_ADD(HATSTAT_PHYSMEMALLOC, sizeof (pteval_t) * NPDPERAS);
		HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_HEAP,
			sizeof (pteval_t) * NPDPERAS);

		ASSERT(hat_getkpfnum((caddr_t)pttblent) < (1 << 20));
	} else {
		/*
		 * This is early in boot. Cannot use ddi_mem_alloc. Should
		 * be called only by those allocating permenantly. kmem_alloc
		 * would do fine here as it knows only of pages below 4GB.
		 */
		pttblent = kmem_alloc(2 * sizeof (pteval_t) * NPDPERAS,
			KM_NOSLEEP);
		/* ### should not fail during SOD */
		hat_kmemcount += 2 * sizeof (pteval_t) *NPDPERAS;
		pttblent = (pteptr_t)roundup((uintptr_t)pttblent,
			(sizeof (pteval_t) * NPDPERAS));
		ASSERT(va_to_pfn((caddr_t)pttblent) < (1 << 20));
	}
	if (pdir == kernel_only_pagedir) {
		bzero((caddr_t)pttblent, MMU_PTTBL_SIZE);
		i = user_pgdirpttblents - (kernel_pdelen?1:0);
		for (addr = (caddr_t)pdir; i < NPDPERAS; i++) {
			pttblent[i] = PTBL_ENT(va_to_pfn(addr));
			addr += MMU_PAGESIZE;
		}
		return (pttblent);
	}
	bzero(pdir, user_pdelen);
	if (kernel_pdelen) {
		/*
		 * We cannot use only pgdirpttblent entries to point to kernel
		 * Need to copy mappings
		 */
		addr = ((caddr_t)kernel_only_pagedir +
			(user_pdelen % MMU_PAGESIZE));
		bcopy(addr, ((caddr_t)pdir + user_pdelen), kernel_pdelen);
	}
	for (i = 0, addr = (caddr_t)pdir; i < user_pgdirpttblents;
		i++, addr += MMU_PAGESIZE) {
		if (kvseg.s_base)
			pttblent[i] = PTBL_ENT(hat_getkpfnum(addr));
		else
			pttblent[i] = PTBL_ENT(va_to_pfn(addr));
	}
	ASSERT(kernel_pdelen + user_pdelen == i * MMU_PAGESIZE);
	for (; i < NPDPERAS; i++) {
		pttblent[i] = kernel_only_pttbl[i];
	}
	return (pttblent);
}

static void
hat_free_pttblent(pteptr_t pttblent)
{
	i_ddi_mem_free((caddr_t)pttblent, 1);
}

#endif

/*
 * shared page table between multiple processes
 */
static void
hat_mark_sharedpgtbl(struct hat *hat, struct hwptepage *hwpp)
{
#ifdef lint
	hat = hat;
#endif

	hwpp->hwpp_mapping = I86MMU4KLOCKMAP | I86MMU_OWN_PTBL;
	HATSTAT_INC(HATSTAT_4K_LOCKMAP);
}

void
hat_page_free(pp)
page_t		*pp;
{

	if (!page_tryupgrade(pp)) {
		/*
		 * someone else, most likely a [k]mem reader (?) has the
		 * page locked too. Drop our lock & wait for exclusive access
		 */
		page_unlock(pp);
		while (!page_lock(pp, SE_EXCL, (kmutex_t *)NULL, P_RECLAIM)) {
			continue;
		}
	}
	page_free(pp, 1);
	HATSTAT_SUB(HATSTAT_PHYSMEMALLOC, PAGESIZE);
}

/*
 * Create a page for ptes, pdirs etc
 */
page_t *
hat_page_create(hat, addr)
struct hat	*hat;
pteptr_t		addr;
{
	page_t		*pp;
	u_offset_t	off;
	struct seg	tmpseg;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));

	/*
	 * Use an offset of hat << 32 + addr to distribute the pages in
	 * different page hash chains
	 */
	off = (u_offset_t)&hat;
	off = (off << 32) + (uint_t)addr;

	bzero(&tmpseg, sizeof (struct seg));
	tmpseg.s_as = hat->hat_as;

	pp = page_create_va(&kvp, off, PAGESIZE, PG_EXCL, &tmpseg,
		(caddr_t)addr);
	while (pp == NULL) {
		hat_steal(HAT_STEALMEM);
		pp = page_create_va(&kvp, off, PAGESIZE,  PG_EXCL,
			&tmpseg, (caddr_t)addr);
	}
	page_io_unlock(pp);
	page_hashout(pp, NULL);
	pagezero(pp, 0, PAGESIZE);
	page_downgrade(pp);

	HATSTAT_ADD(HATSTAT_PHYSMEMALLOC, PAGESIZE);

	return (pp);
}


/*
 * Alocate a page to be used as page directory/pagetable or as
 * a page with pointers to hwpps (list = 1)
 */

static page_t *
hat_page_cache_alloc(hat)
struct	hat *hat;
{
	page_t		*pp;

	mutex_enter(&hat_page_cache_lock);
	if (hat_page_freelist) {
		pp = hat_page_freelist;
		page_list_break(&pp, &hat_page_freelist, 1);
		hat_pagecount--;
		mutex_exit(&hat_page_cache_lock);
	} else {
		mutex_exit(&hat_page_cache_lock);
		hat_page_addr += PAGESIZE;
		/*
		 * These pages need not really have a virt addr.
		 * So 2nd param to hat_page_create is bogus
		 */
		pp = hat_page_create(hat, (pteptr_t)hat_page_addr);
	}
	return (pp);
}


static void
hat_hwppinit(struct hat *hat, hwptepage_t * hwpp, int hwpp_index)
{

	hwpp->hwpp_hat = hat;
	hwpp->hwpp_numptes = 0;
	hwpp->hwpp_lockcnt = 0;
	hwpp->hwpp_index = hwpp_index;
	hwpp->hwpp_mapping = 0;

	hwpp->hwpp_next = hat->hat_hwpp;
	if (hwpp->hwpp_next)
		hwpp->hwpp_next->hwpp_prev = hwpp;
	hwpp->hwpp_prev = NULL;
	hat->hat_hwpp = hwpp;

	if (hat->hat_pagedir)
		userhwppmap[hwpp_index] = hwpp;
}


static struct hwptepage *
hat_hwpp_cache_alloc(struct hat *hat, int hwpp_index, uint_t *oldhat,
		int allocpt)
{
	hwptepage_t	*hwpp;

	ASSERT((hat == kernel_hat) || MUTEX_HELD(&hat->hat_mutex));
	ASSERT(hwpp_index != -1);

	hat->hat_numhwpp++;

	if (!hat->hat_pagedir && hat->hat_numhwpp >= hat_perprocess_pagedir) {
		ASSERT(*oldhat == HAT_NOT_SWITCHED);
		*oldhat = hat_switch2ppp(hat);
	}

	if (allocpt) {
		if (hat->hat_pagedir && hat_hwpp_freelist) {
			mutex_enter(&hat_hwpp_cache_lock);
			hwpp = hat_hwpp_freelist;
			if (hwpp) {
				hat_hwpp_freelist = hwpp->hwpp_next;
				hat_hwppcount--;
				mutex_exit(&hat_hwpp_cache_lock);
				hat_hwppinit(hat, hwpp, hwpp_index);
				pte2hati[btop((uintptr_t)hwpp->hwpp_pte)] =
					hat->hat_index;
				hwpp->hwpp_lpte =
					&userptemap[hwpp_index * NPTEPERPT];
				userptetable[hwpp_index] = hwpp->hwpp_pde;
				return (hwpp);
			}
			mutex_exit(&hat_hwpp_cache_lock);
		} else if (!hat->hat_pagedir && hat_hwpplpte_freelist) {
			mutex_enter(&hat_hwpplpte_cache_lock);
			hwpp = hat_hwpplpte_freelist;
			if (hwpp) {
				hat_hwpplpte_freelist = hwpp->hwpp_next;
				hat_hwpplptecount--;
				mutex_exit(&hat_hwpplpte_cache_lock);
				hat_hwppinit(hat, hwpp, hwpp_index);
				pte2hati[btop((uintptr_t)hwpp->hwpp_pte)] =
					hat->hat_index;

				return (hwpp);
			}
			/* ### grab from other list and memload - put above */
			mutex_exit(&hat_hwpplpte_cache_lock);
		}
	}

	hwpp = hat_hwppalloc(hat, hwpp_index);
	if (allocpt)
		hat_pagetablealloc(hat, hwpp);

	return (hwpp);
}

static void
hat_hwpp_cache_free(struct hat *hat, hwptepage_t *hwpp, int updatepde)
{

	int		index = hwpp->hwpp_index;
#ifdef DEBUG
	int		oldhat, i;
	pteptr_t	pte;
#endif

	ASSERT((hat == kernel_hat)||MUTEX_HELD(&hat->hat_mutex));
	ASSERT(index != -1);

	hat_hwppdelete(hat, hwpp);
	if (hwpp->hwpp_mapping && !(hwpp->hwpp_mapping & I86MMU_OWN_PTBL)) {
		if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
			HATSTAT_DEC(HATSTAT_LGPAGE_LOCKMAP);
		}
		ASSERT(cr3() == hat->hat_pdepfn);
		INVALIDATE_PTE(&userptetable[index]);
		userhwppmap[index] = NULL;
		INVALIDATE_PTE(&userpagedir[index]);
		if (updatepde)
			hat_update_pde(hat, index, MMU_STD_INVALIDPTE);
		KMEM_FREE_STAT(hwpp, sizeof (*hwpp));
		return;
	}
	pte2hati[btop((uintptr_t)hwpp->hwpp_pte)] = 0;
#ifdef	DEBUG
	oldhat = i86_switch_hat(pteashat);
	pte = hwpp->hwpp_pte;
	for (i = 0; i < NPTEPERPT; i++) {
		ASSERT(!pte_valid(pte));
	}
	i86_restore_hat(oldhat);
#endif
	if (hat->hat_pagedir) {

		/* put on free list */
		mutex_enter(&hat_hwpp_cache_lock);
		hwpp->hwpp_next = hat_hwpp_freelist;
		hat_hwpp_freelist = hwpp;
		hat_hwppcount++;
		mutex_exit(&hat_hwpp_cache_lock);

		/* clean up */
		ASSERT(cr3() == hat->hat_pdepfn);
		INVALIDATE_PTE(&userptetable[index]);
		userhwppmap[index] = NULL;
		INVALIDATE_PTE(&userpagedir[index]);

		if (hat_hwppcount > hat_hwpp_cache_max)
			hat_hwpp_cache_reclaim(HWPPLIST);

	} else {	/* !hat->hat_pagedir */

		mutex_enter(&hat_hwpplpte_cache_lock);
		hwpp->hwpp_next = hat_hwpplpte_freelist;
		hat_hwpplpte_freelist = hwpp;
		hat_hwpplptecount++;
		mutex_exit(&hat_hwpplpte_cache_lock);

		if (hat_hwpplptecount > hat_hwpplpte_cache_max)
			hat_hwpp_cache_reclaim(HWPPLPTELIST);
	}

	if (updatepde)
		hat_update_pde(hat, index, MMU_STD_INVALIDPTE);
}

/*
 * free up to hat_page_stealcnt hwpp and pagetable in hwpp cache
 */
static void
hat_hwpp_cache_reclaim(int what)
{
	hwptepage_t	*hwpp;
	int	index, i, oldhat;
	uint_t		gen;

	oldhat = i86_switch_hat(pteashat);

	for (i = 0; i < hat_page_stealcnt; i++) {
		if (what & HWPPLIST) {
			mutex_enter(&hat_hwpp_cache_lock);
			hwpp = hat_hwpp_freelist;
			if (hwpp) {
				hat_hwpp_freelist = hwpp->hwpp_next;
				hat_hwppcount--;
				mutex_exit(&hat_hwpp_cache_lock);
				hat_page_free(hwpp->hwpp_pte_pp);
				index = btop((uintptr_t)hwpp->hwpp_pte);
				pt_hwpp[index] = &pt_invalid_hwpp;
				pt_ptable[index] = pt_invalid_pteptr;
				TLBFLUSH_BRDCST(pteashat,
					(caddr_t) hwpp->hwpp_pte, gen);
				hat_vmem_free(ptearena, hwpp->hwpp_pte,
					MMU_PAGESIZE);
				KMEM_FREE_STAT(hwpp, sizeof (hwptepage_t));
				HATSTAT_DEC(HATSTAT_PGTBLALLOC);
				TLBFLUSH_WAIT(gen);
			} else {
				mutex_exit(&hat_hwpp_cache_lock);
				if ((what & HWPPLIST) == what)
					break;
			}
		}
		if (what & HWPPLPTELIST) {
			mutex_enter(&hat_hwpplpte_cache_lock);
			hwpp = hat_hwpplpte_freelist;
			if (hwpp) {
				hat_hwpplpte_freelist = hwpp->hwpp_next;
				hat_hwpplptecount--;
				mutex_exit(&hat_hwpplpte_cache_lock);
				hat_unload(kas.a_hat, (caddr_t)hwpp->hwpp_lpte,
					MMU_PAGESIZE, HAT_UNLOAD_UNLOCK);
				hat_vmem_free(heap_arena, hwpp->hwpp_lpte,
					MMU_PAGESIZE);
				index = btop((uintptr_t)hwpp->hwpp_pte);
				hat_page_free(hwpp->hwpp_pte_pp);
				pt_hwpp[index] = &pt_invalid_hwpp;
				pt_ptable[index] = pt_invalid_pteptr;
				TLBFLUSH_BRDCST(pteashat,
					(caddr_t) hwpp->hwpp_pte, gen);
				hat_vmem_free(ptearena, hwpp->hwpp_pte,
					MMU_PAGESIZE);
				KMEM_FREE_STAT(hwpp, sizeof (hwptepage_t));
				HATSTAT_DEC(HATSTAT_PGTBLALLOC);
				TLBFLUSH_WAIT(gen);
			} else {
				mutex_exit(&hat_hwpplpte_cache_lock);
				if ((what & HWPPLPTELIST) == what)
					break;
			}
		}
	}
	i86_restore_hat(oldhat);
}

static void
hat_ppp_reclaim()
{
	struct hatppp 	*ppp;
	mutex_enter(&hat_page_lock);
	if (!hat_ppplist) {
		mutex_exit(&hat_page_lock);
		return;
	}
	hat_pppcount--;
	ppp = hat_ppplist;
	hat_ppplist = ppp->hp_next;
	mutex_exit(&hat_page_lock);
	hat_deallocppp(ppp);
}

static void
hat_page_cache_reclaim()
{
	page_t		*pp, *pplist;
	int		i, count;

	mutex_enter(&hat_page_cache_lock);
	count = hat_pagecount;
	if (!count) {
		mutex_exit(&hat_page_cache_lock);
		hat_ppp_reclaim();
		return;
	}
	if (count > 1)
		count = MIN(count/2, hat_page_stealcnt);
	pp = hat_page_freelist;
	page_list_break(&pp, &hat_page_freelist, count);
	hat_pagecount -= count;
	mutex_exit(&hat_page_cache_lock);
	pplist = pp;
	for (i = 0; i < count; i++) {
		page_list_break(&pp, &pplist, 1);
		hat_page_free(pp);
		pp = pplist;
	}
	/* Now free one ppp if any are present (should be 8 pages) */
	hat_ppp_reclaim();
}

static void
hat_page_cache_free(pp)
page_t		*pp;
{
	if (hat_pagecount > hat_page_cache_max) {
		hat_page_free(pp);
		return;
	}
	mutex_enter(&hat_page_cache_lock);
	page_list_concat(&hat_page_freelist, &pp);
	hat_pagecount++;
	mutex_exit(&hat_page_cache_lock);
}

static void
hat_deallocppp(ppp)
struct hatppp *ppp;
{
	page_t		*pp;
	uint_t		pt_ptableindex, endindex;
	int		oldhat;
#ifdef	PTE36
	if (ppp->hp_pgdirpttblent)
		hat_free_pttblent(ppp->hp_pgdirpttblent);
#endif
	pt_ptableindex = btop((uintptr_t)ppp->hp_pteasaddr);
	endindex = pt_ptableindex + hat_ppp_pteas_pages;
	oldhat = i86_switch_hat(pteashat);
	for (; pt_ptableindex < endindex; pt_ptableindex++) {
		pt_ptable[pt_ptableindex] = pt_invalid_pteptr;
	}
	i86_restore_hat(oldhat);
	hat_vmem_free(ptearena, ppp->hp_pteasaddr, ptob(hat_ppp_pteas_pages));
	pp = ppp->hp_prvtpp;
	while (pp) {
		page_list_break(&pp, &ppp->hp_prvtpp, 1);
		hat_page_free(pp);
		pp = ppp->hp_prvtpp;
	}
	KMEM_FREE_STAT(ppp, sizeof (*ppp));
}

static void
hat_freeppp(ppp)
struct hatppp *ppp;
{
	HATSTAT_DEC(HATSTAT_PPP);

	if (hat_pppcount < hat_ppp_cache_max) {
		mutex_enter(&hat_page_lock);
		ppp->hp_next = hat_ppplist;
		hat_ppplist = ppp;
		hat_pppcount++;
		mutex_exit(&hat_page_lock);
		return;
	}
	hat_deallocppp(ppp);
}

static struct hatppp *
hat_allocppp(hat)
struct hat *hat;
{
	struct hatppp 	*ppp;
	page_t		*pp;
	int		i;
	pteptr_t	pagedir, ptable;
	caddr_t		addr, *ptablearray;
	uint_t 		pfn, pdirindex, pt_ptableindex;
	int		oldhat;


	HATSTAT_INC(HATSTAT_PPP);

	mutex_enter(&hat_page_lock);
	if (hat_ppplist) {
		ppp = hat_ppplist;
		hat_ppplist = ppp->hp_next;
		hat_pppcount--;
		mutex_exit(&hat_page_lock);
		ppp->hp_next = NULL;
		HATPRINTF(0x02, ("hat_allocppp: %x from freelist\n", ppp));
		return (ppp);
	}
	mutex_exit(&hat_page_lock);

	ppp = hat_kmem_zalloc(sizeof (struct hatppp));
	pagedir = hat_vmem_alloc(heap_arena, ptob(user_pgdirpttblents));

	/*
	 * Allocate address for ppp from pteasmap so that we can dump
	 * all this useful information using segpt. This address will not
	 * be used to dereference the pages by hat.
	 */
	ppp->hp_pteasaddr = hat_vmem_alloc(ptearena, ptob(hat_ppp_pteas_pages));
	pt_ptableindex = btop((uintptr_t)ppp->hp_pteasaddr);

	oldhat = i86_switch_hat(pteashat);
	for (i = 0; i < user_pgdirpttblents; i++) {
		pp = hat_page_cache_alloc(hat);
		page_list_concat(&ppp->hp_prvtpp, &pp);
		hat_memload(kas.a_hat, (caddr_t)pagedir + (i * MMU_PAGESIZE),
			pp, PROT_READ | PROT_WRITE | HAT_NOSYNC,
			HAT_LOAD_NOCONSIST | HAT_LOAD_LOCK);
		pt_ptable[pt_ptableindex++] =
			PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
	}
#ifdef	PTE36
	ppp->hp_pgdirpttblent = hat_alloc_pttblent(pagedir);
	ppp->hp_pdepfn = (uint_t)(hat_getkpfnum(
		(caddr_t)ppp->hp_pgdirpttblent) << MMU_STD_PAGESHIFT) +
			((uint_t)ppp->hp_pgdirpttblent & PAGEOFFSET);
#else
	bzero(pagedir, user_pdelen);
	bcopy((caddr_t)kernel_only_pagedir + user_pdelen,
		(caddr_t)pagedir + user_pdelen, kernel_pdelen);
	ppp->hp_pdepfn = (hat_getkpfnum((caddr_t)pagedir) <<
				MMU_STD_PAGESHIFT);
#endif

	/*
	 * Allocate page dir pages for mapping in hwpp pointers &
	 * ptables for ptemap.
	 */
	ptablearray = hat_kmem_alloc(npdirs * sizeof (caddr_t));

	pdirindex = MMU_L1_INDEX(usermapend);
	for (i = 0; i < npdirs; i++) {
		pp = hat_page_cache_alloc(hat);
		page_list_concat(&ppp->hp_prvtpp, &pp);
		ptablearray[i] = ppmapin(pp, PROT_READ| PROT_WRITE, 0);
		pfn = mach_pp->p_pagenum;
		pagedir[pdirindex + i] = PTEOF_C(pfn, MMU_STD_SRWX);
		pt_ptable[pt_ptableindex++] =
			PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
	}

	/*
	 * create mappings for userptetable to map userptemap. mapout the pps.
	 */
	addr = (caddr_t)userptetable;
	i = MMU_L1_INDEX(userptemap) - pdirindex;

	for (; addr < (caddr_t)userptemap; addr += PAGESIZE, i++) {
		pfn = hat_getkpfnum(ptablearray[i]);
		ptable = (pteptr_t)ptablearray[MMU_L1_INDEX(addr) - pdirindex];
		ptable[MMU_L2_INDEX(addr)] = PTEOF_C(pfn, MMU_STD_SRWX);
	}
	/* map in pagedir */
	addr = (caddr_t)userpagedir;

	for (i = 0; i < user_pgdirpttblents; addr += PAGESIZE, i++) {
		pfn = hat_getkpfnum((caddr_t)&pagedir[i*NPTEPERPT]);
		hat_unload(kas.a_hat, (caddr_t)&pagedir[i*NPTEPERPT], PAGESIZE,
			HAT_UNLOAD_UNLOCK);
		ptable = (pteptr_t)ptablearray[MMU_L1_INDEX(addr) - pdirindex];
		ptable[MMU_L2_INDEX(addr)] = PTEOF_C(pfn, MMU_STD_SRWX);
	}
	hat_vmem_free(heap_arena, pagedir, ptob(user_pgdirpttblents));

	/* Now allocate pages for hwppmap */
	for (addr = (caddr_t)userhwppmap; addr < (caddr_t)kernelbase; ) {
		pp = hat_page_create(hat, (pteptr_t)addr);
		page_list_concat(&ppp->hp_prvtpp, &pp);
		ptable = (pteptr_t)ptablearray[MMU_L1_INDEX(addr) - pdirindex];
		ptable[MMU_L2_INDEX(addr)] =
				PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
		addr += PAGESIZE;
		pt_ptable[pt_ptableindex++] =
			PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
	}
	i86_restore_hat(oldhat);
	for (i = 0; i < npdirs; i++) {
		ppmapout(ptablearray[i]);
	}
	KMEM_FREE_STAT(ptablearray, npdirs * sizeof (caddr_t));
	return (ppp);
}

/*
 * The user address space below kernelbase is split into following regions
 * for use by hat (with defaults for mmu32 & 36 given)
 *						mmu32		mmu36
 *	kernelbase				E0000000	C0000000
 *	userhwppmap (ptrs to hwpp for each ptedir entry, rounded down to
 *		a page boundary)		DFFFF000	BFFFE000
 *	userptemap (for addressing ptes)	DFC80000	BFA00000
 *	userptetable (ptes which map userptemap)
 *						DFC7E000	BF9FD000
 *	userpagedir				DFC7D000	BF9F9000
 *	usermapend (rounded down to large page boundary)
 *						DFC00000	BF800000
 *	(definition in vm_machdep.c)
 */

static void
hat_unload_all_pdes(hat)
struct hat *hat;
{
	pteptr_t	ptable;
	struct cpu 	*cpup;
	int		i, j;
	ulong_t		mask;


	if (!(hat->hat_cpusrunning & ~CPU->cpu_mask))
		return;
	/*
	 * This 'as' is currently active on couple of more
	 * cpu's. We have to clear per cpu page directory
	 * entries on all those cpu's.
	 */
	HATPRINTF(0x02, ("unload_all_pdes: CAP CPU %x\n",
			hat->hat_cpusrunning));

	mask = hat->hat_cpusrunning;
	CAPTURE_SELECTED_CPUS(mask);
	while (i = highbit(mask)) {
		i--;
		ASSERT(cpu[i]);
		cpup = cpu[i];
		mask &= ~cpup->cpu_mask;
		if (cpup->cpu_current_hat != hat)
			continue;
		ASSERT(hat->hat_cpusrunning & cpup->cpu_mask);
		ptable = (pteptr_t)cpup->cpu_pagedir;
		ASSERT(cpup->cpu_numpdes < hat_perprocess_pagedir_max);
		for (j = 0; j < cpup->cpu_numpdes; j++) {
			ASSERT(cpup->cpu_pde_index[j]*sizeof (pteval_t)
					< user_pdelen);
			INVALIDATE_PTE(&ptable[cpup->cpu_pde_index[j]]);
		}
		cpup->cpu_numpdes = 0;
		cpup->cpu_current_hat = NULL;
	}
	RELEASE_CPUS;
}
/*
 * create all necessary tables and switch to a per process page dir.
 * returns old cr3 in case it is called from a thread which uses
 * a different hat
 */
static uint_t
hat_switch2ppp(hat)
struct hat *hat;
{
	kthread_t	*t = curthread;
	struct		hwptepage  *hwpp;
	pteptr_t	pagedir;
	struct cpu 	*cpup;
	caddr_t		addr;
	uint_t		oldcr3;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));

	HATPRINTF(0x02, ("hat_switch2ppp: hat %x\n", hat));
	hat->hat_ppp = hat_allocppp(hat);
	pagedir = userpagedir;
	hat->hat_pdepfn = hat->hat_ppp->hp_pdepfn;

	kpreempt_disable();

	cpup = CPU;
	hat_unload_pdes(cpup);

	oldcr3 = t->t_mmuctx;	/* We may switch for another hat */
	hat->hat_ctx.ct_hat = hat;
	t->t_mmuctx = (uint_t)&hat->hat_ctx;
	LOAD_CR3(cpup, &hat->hat_ctx);

	/* load all entries in to our own pagedirectory & rm from percpu one */
	hwpp = hat->hat_hwpp;
	while (hwpp) {
		uint_t	index;
		index = hwpp->hwpp_index;
		pagedir[index] = hwpp->hwpp_pde;
		userhwppmap[index] = hwpp;
		if (hwpp->hwpp_pte_pp) {
			userptetable[index] = hwpp->hwpp_pde;
			addr = (caddr_t)hwpp->hwpp_lpte;
			hwpp->hwpp_lpte = &userptemap[index * NPTEPERPT];
			ppmapout(addr);
			HATSTAT_SUB(HATSTAT_VIRTMEMALLOC_HEAP, PAGESIZE);
		}
		hwpp = hwpp->hwpp_next;
	}

	hat_unload_all_pdes(hat);

	cpup->cpu_current_hat = hat;
	hat->hat_pagedir = 1;
	kpreempt_enable();
	return (oldcr3);
}

/*
 * Initialize the hardware address translation structures.
 * Called by startup() after the vm structures have been allocated
 * and mapped in.
 */
void
hat_init(void)
{
	uint_t	kernelsize;
	uint_t	kheappages;

	mutex_init(&hat_hat_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_page_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_page_cache_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_hwpp_cache_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_hwpplpte_cache_lock, NULL, MUTEX_DEFAULT, NULL);

	all_hat.hat_next = all_hat.hat_prev = &all_hat;

	/*
	 * There could be three chunks of kernel virtual address space
	 * 1. kernelbase to eecontig:  includes segkp and kvseg
	 * 2. SEGKMAP_START to 4Gb. includes kernel text+data.
	 * In PTE36 mode,
	 * during the process of booting there is  a time frame when we
	 * allocate memory ourselves (so we need 64 bit pte's) and we
	 * require boot services for I/O. So, we need to map boot code
	 * (of size BOOT_SIZE) using 64 bit pte's.
	 */

	/*
	 * eecontigindex is # of ptes between eecontig & kernelbase
	 * rounded up to a pagetable size
	 *
	 * kptes:	kernel page table entries for kernelbase and above
	 * 		minus the 'hole' between eecontig and SEGKMAPSTART.
	 * +---+
	 * |   |		-> page right below 4 GB
	 * +---+
	 *  ...
	 * +---+
	 * | e | <- KERNELmap	-> page at SEGKMAPSTART (e == eecontigindex)
	 * +---+
	 * |e-1|		-> page right below eecontig
	 * +---+
	 *  ...
	 * +---+
	 * | 0 | <- Sysmap	-> page at kernelbase
	 * +---+
	 */

	eecontigindex = roundup(((uintptr_t)eecontig - kernelbase), PTSIZE)/
			MMU_PAGESIZE;
	kernelsize = (eecontigindex * MMU_PAGESIZE) + (0 - SEGKMAP_START) +
		BOOT_SIZE;

	if (kadb_is_running)
		kernelsize += KADB_SIZE;

	/* number of pages to hold all of the kernel ptes */
	kernel_pts = (kernelsize + PTSIZE - 1) / PTSIZE;
	kernel_pts++;

	HATPRINTF(4, ("hatinit: eecontig %x eecontigindex %x kernel_pts %x\n",
			eecontig, eecontigindex, kernel_pts));

	KMEM_ZALLOC_STAT(kptes, kernel_pts * MMU_PAGESIZE, KM_NOSLEEP);

	Sysmap = kptes;
	KERNELmap = &kptes[eecontigindex];

	/*
	 * hmes needed to map all of kernelheap need pre-allocation
	 * as kmem_cache_alloc may need to map in a page in kernelheap
	 */

	kheappages = btop((uint_t)(ekernelheap - kernelheap));
	KMEM_ZALLOC_STAT(khmes, kheappages * sizeof (struct hment), KM_SLEEP);
	ekhmes = khmes + kheappages;

	kheappte = &Sysmap[mmu_btop((uintptr_t)kernelheap - kernelbase)];
	ekheappte = kheappte + kheappages;

	/* v_proc based on HAT_MAXHAT which allows for ushort_t hat_index */
	KMEM_ZALLOC_STAT(hatida,
		(v.v_proc + HAT_RSRVHAT) * sizeof (struct hat *), KM_SLEEP);

	/*
	 * We grab the first hat for the kernel,
	 */
	kas.a_hat = kernel_hat = hat_alloc(&kas);

	hat_hmecache = kmem_cache_create("hme_cache", sizeof (struct hment),
			HMEALIGN, NULL, NULL, hat_hmecache_reclaim,
			NULL, NULL, hat_hmecache_flags);

	npdirs = MMU_L1_INDEX(kernelbase - 1) - MMU_L1_INDEX(usermapend) + 1;
	hat_ppp_pteas_pages = btopr(kernelbase - (uintptr_t)userhwppmap) +
			npdirs + user_pgdirpttblents;

	if ((x86_feature & X86_PGE) && use_hat_gb)
		hat_gb_enable = PTE_GB_MASK;	/* 0x100 */

	/* limit page cache to <1/2% of total memory & pppmap cache to <2% */
	if (hat_ppp_cache_max == 0) {
		hat_page_cache_max = physmem/256;
		hat_ppp_cache_max = physmem/512; /* each 1 is ~ 8 pages */
		if (hat_perprocess_pagedir == 1) {
			hat_hwpp_cache_max = physmem/128;
			hat_hwpplpte_cache_max = 0;
		} else {
			/* lpte cache also locks kernel mem. Limit it to 2 MB */
			hat_hwpplpte_cache_max = MAX(physmem/128, 512);
			hat_hwpp_cache_max = physmem/128;
			hat_ppp_cache_max = 4;
		}
		hat_searchcnt = hat_page_stealcnt = hat_page_cache_max/16;
		hat_stealcnt = min(v.v_proc/64, MAXHATSTEALCNT);
	}

}

static uint_t
hat_update_pte(hat, pte, value, addr, rmkeep)
struct hat *hat;
pteval_t *pte;
pteval_t value;
caddr_t addr;
uint_t rmkeep;
{
	register uint_t		oldrm = *pte & PTE_RM_MASK;
	int 			newvalid = pte_valid(&value);
	int			freeing, caching_changed;
	int 			flush, operms, perm_changed;
	uint_t			gen;
	pteval_t		prev_value;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	ASSERT((hat == kernel_hat) || MUTEX_HELD(&hat->hat_mutex));

	if (*pte == value)
		return (oldrm);

	HATPRINTF(4, ("update_pte: pte %x ptev %x value %x caller %x\n",
			pte, *pte, value, caller()));

	if (ncpus == 1) {
		freeing = hat->hat_flags & I86MMU_FREEING;
		flush  = pte_valid(pte) &&
			(addr >= (caddr_t)kernelbase || !freeing);
		value |= *pte & rmkeep;
		LOAD_PTE(pte, value);
		if (flush) {
			mmu_tlbflush_entry(addr);
		}
		return (oldrm);
	}

	/*
	 * We have to flush tlb when any of the following is true
	 * 1. valid bit is being made invalid.
	 * 2. protection bits change.
	 * 3. ref and/or mod bits change.
	 *
	 * If we are dealing with kernel address we need to flush tlb on
	 * cpu's. In the case of user address we have to flush tlb on all
	 * those cpu's where the page directory holds mappings for the
	 * current address space.
	 *    When we make a pte invalid, we broadcast a tlb flush
	 *    message to all cpu's. We note the tlb_flush_gen number
	 *    in the pte placeholder contained in the pagetable.
	 *    tlb_flush_gen is incremented by a value of 2 each time we
	 *    broadcast a tlb flush. Even numbers == valid bit not set.
	 *    We do not wait for the tlbflush to complete on other cpu's.
	 *    We wait for tlbflush only when we load a valid pte.
	 *
	 * For kernel addresses,
	 * When we reset mod bit we broadcast tlbflush message and wait for
	 * tlbflush before we return.
	 *
	 * In the case of user  address we wait for tlbflush to
	 * complete when
	 * 1. We make a pte invalid
	 * 2. We reset mod or change permission bits.
	 */

	operms = *pte & PTE_PERMMASK;


	if ((uint_t)addr >= kernelbase) {
		ASSERT(hat == kernel_hat);
		if (!newvalid) {
			HATPRINTF(4, ("hat_update_pte: !newvalid - pte %x %x\n",
					pte, *pte));
			/*
			 * invalidate pte with a generation number with no
			 * wait for flush to complete. flush wait check when
			 * pte is loaded.
			 */
			TLBFLUSH_BRDCST(hat, addr, *(uint_t *)pte);
			return (oldrm);
		}
		value |= hat_gb_enable;
		if (!pte_valid(pte)) {
			/*
			 * We are loading a pte, make sure that the previous
			 * tlbflush for this address is complete.
			 * generation number is a 32 bit quantity stored
			 * in the pte.
			 */
			HATPRINTF(4, ("hat_update_pte: !pte_valid - pte %x\n",
					pte));
			TLBFLUSH_WAIT((uint_t)*pte);
			LOAD_PTE(pte, value);
			return (0);
		} else {
			HATPRINTF(4, ("hat_update_pte: else - pte %x\n",
					pte));
			/*
			 * We need an atomic update here. If the pte was
			 * not dirty and we were only changing the ref bit
			 * then we dont wait for the tlbflush to complete.
			 */
			do {
				prev_value = *pte;
				value |= prev_value & rmkeep;
			} while (CAS_PTE(pte, prev_value, value) != prev_value);
			TLBFLUSH_BRDCST(hat, addr, gen);
			if ((ptevalue_dirty(prev_value)) ||
				operms != (value & PTE_PERMMASK))
				TLBFLUSH_WAIT(gen);
			return (prev_value & PTE_RM_MASK);
		}

	}

	/* We are dealing with user addresses now */

	/* indicate that we don't want addr to be accessed by any new cpu */
	hat->hat_critical = addr;

	if (!(hat->hat_cpusrunning & ~CPU->cpu_mask) ||
		(hat->hat_flags & I86MMU_FREEING)) {
		/*
		 * We are either not running on any cpu or we are
		 * executing only on this cpu. We dont need an
		 * atomic update to pte.
		 */
		prev_value = *pte;
		LOAD_PTE(pte, (value|(prev_value & rmkeep)));
		oldrm = prev_value & PTE_RM_MASK;
		if (!(hat->hat_flags & I86MMU_FREEING))
			mmu_tlbflush_entry(addr);
	} else {
		/*
		 * Check for the conditions to apply the Pentium Pro bug
		 * workaround.
		 */
		if (pentiumpro_bug4064495 &&
			((operms & PTE_PERMMASK) == PG_UW) && (!newvalid ||
				((value & PTE_PERMMASK) == PG_UR))) {

			/*
			 * Apply the workaround.
			 */

			HATPRINTF(4, ("hat_update_pte: CAP' CPU %x\n",
				hat->hat_cpusrunning));

			CAPTURE_CPUS(hat);
			do {
				prev_value = *pte;
				value |= prev_value & rmkeep;
			} while (CAS_PTE(pte, prev_value, value) != prev_value);
			RELEASE_CPUS;
			mmu_tlbflush_entry(addr);
			oldrm |= prev_value & PTE_RM_MASK;
		} else {
			do {
				prev_value = *pte;
				value |= prev_value & rmkeep;
			} while (CAS_PTE(pte, prev_value, value) != prev_value);
			TLBFLUSH_BRDCST(hat, addr, gen);
			oldrm = prev_value & PTE_RM_MASK;
			caching_changed =
				    ((prev_value & PTE_NONCACHEABLE(1)) !=
					(value & PTE_NONCACHEABLE(1)));
			perm_changed = ((value & PTE_PERMMASK) != operms);
			if (!newvalid || ptevalue_dirty(prev_value) ||
					caching_changed || perm_changed)
				TLBFLUSH_WAIT(gen);
		}
	}
	hat->hat_critical = HAT_INVALID_ADDR;

	return (oldrm);

}


/*
 * Returns a pointer to the pte struct (dereferencable in process context)
 * for the given virtual address.
 * If the necessary page tables do not exist, return NULL.
 */
pteval_t *
hat_ptefind(struct hat *hat, caddr_t addr)
{
	pteval_t	*pte;
	struct hwptepage *hwpp;


	if (addr >= (caddr_t)kernelbase) {
	    if (addr >= (caddr_t)SEGKMAP_START)
		pte = &KERNELmap[mmu_btop(addr - (caddr_t)SEGKMAP_START)];
	    else
		pte = (addr > eecontig ? NULL :
		    &Sysmap[mmu_btop((uintptr_t)addr - kernelbase)]);
		return (pte);
	}


	hwpp = hat_hwppfind(hat, addrtohwppindex(addr));
	return ((hwpp && hwpp->hwpp_lpte) ?
		(&hwpp->hwpp_lpte[PAGETABLE_INDEX((uint_t)addr)]) : NULL);
}

/*
 * Project private interface between the kernel and kadb to implement kadb's
 * "@" command to display/modify memory given physical addresses.
 *
 * Map "vaddr" (a kadb virtual address) to the physical page whose 64-bit
 * address is contained in new, saving the old 64-bit address in *old.
 * Return "0" (indicating an error) if the virtual address is not part of the
 * kernel debugger.
 */


int
kadb_map(caddr_t vaddr, uint64_t new, uint64_t *old)
{
	pteval_t	*pte;
	pteval_t	value;

#ifdef	lint
	new = new;
#endif
	if (vaddr < (caddr_t)kernelbase)
		return (0);
#ifndef	PTE36
	if (new >= (1ULL << 32))
		return (0);
#endif
	pte = hat_ptefind(kernel_hat, vaddr);
	if (old)
		*old = *pte & ~MMU_STD_PAGEMASK;

	value = PTEOF_C((new >> MMU_STD_PAGESHIFT), MMU_STD_SRWX);
	LOAD_PTE(pte, value);
	mmu_tlbflush_entry(vaddr);
	return (1);
}

/*
 * Simple and fast function to remap an address
 * Can bypass many checks
 * Returns 1 if the mapping is currently not valid.
 * 0 if reloaded successfully.
 */
static int
hat_ptereload(struct hat *hat, caddr_t addr, struct page *pp, uint_t flags)
{
	pteval_t	*pte, *lpte;
	struct	hwptepage *hwpp;
	pteval_t	pteval;
	uint_t		pfn, oldpfn;
	int		index = MMU_L1_INDEX(addr);
	int		oldhat;
	uint_t		gen;
	page_t		*oldpp;

	if ((addr < (caddr_t)kernelbase)) {
		oldhat = i86_switch_hat(hat);
		hwpp = hat_hwppfind(hat, index);
		if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP)
			lpte = hwpp->hwpp_lpte;
		else
			lpte = &hwpp->hwpp_lpte[PAGETABLE_INDEX((uint_t)addr)];
		pteval = *lpte;
		pfn = mach_pp->p_pagenum;
		oldpfn = PTEVAL2PFN(pteval);
		CHG_PFN(pteval, pfn);

		if (!(flags & HAT_LOAD_NOCONSIST)) {
			if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP)
				pte = hwpp->hwpp_pte;
			else
				pte = &hwpp->hwpp_pte[
					PAGETABLE_INDEX((uint_t)addr)];
			oldpp = page_numtopp_nolock(oldpfn);
			i86_mlist_enter(oldpp);
			INVALIDATE_PTE(lpte);
			hme_sub(oldpp, pte, NOHOLDMLIST);

			i86_mlist_enter(pp);
			LOAD_PTE(lpte, pteval);
			hme_add(pp, pte, pte_ro(lpte));
			i86_mlist_exit(pp);
		} else {
			setpte_noconsist(lpte);
			LOAD_PTE(lpte, pteval);
		}
		hat->hat_critical = addr;
		if (hat->hat_cpusrunning  & ~CPU->cpu_mask) {
			HATPRINTF(4, ("hat_ptereload: BRDCST %x\n",
				hat->hat_cpusrunning));
			TLBFLUSH_BRDCST(hat, addr, gen);
			TLBFLUSH_WAIT(gen);
		} else
			mmu_tlbflush_entry(addr);
		hat->hat_critical = HAT_INVALID_ADDR;
		i86_restore_hat(oldhat);
		return (0);
	}

	return (1);
}

/*
 * Loads the pte for the given address with the given pte. Also sets it
 * up as a mapping for page pp, if there is one.
 */
static void
hat_pteload(struct hat *hat, caddr_t addr, struct page *pp,
	pteval_t *pte, int flags)
{
	pteval_t		*a_pte, *a_lpte;
	int			remap = 0, large_page = 0;
	struct	hwptepage	*hwpp;
	int			hwpp_index, index;
	pteval_t		*pagedir;
	uint_t			oldhat;

	HATPRINTF(4, ("hat_pteload: hat %x addr %x ptev %x\n",
			hat, addr, *pte));

	ASSERT((hat == kernel_hat)||MUTEX_HELD(&hat->hat_mutex));

	if (addr >= (caddr_t)kernelbase) {
		/* All ptes are already present for kernel addrs */
		a_pte = a_lpte = hat_ptefind(hat, addr);
		/* Lot of segmap_faults return this way */
		if (*pte == (*a_pte & ~PTE_RM_MASK))
			return;
	} else {
		if (LARGE_PAGE(pte))
			large_page = 1;
		/*
		 * We may be called due to SOFTLOCK from another
		 * process. procfs needs this, see prusrio()
		 */
		oldhat = i86_switch_hat(hat);
		hwpp_index = addrtohwppindex(addr);
		hwpp = hat_hwppfind(hat, hwpp_index);
		if (!hwpp) {
			if ((flags & HAT_LOAD_LOCK) && !hat->hat_pagedir)
				oldhat = hat_switch2ppp(hat);
			if (large_page) {
				hwpp = hat_hwpp_cache_alloc(hat, hwpp_index,
					&oldhat, 0);
				if (!(flags & HAT_LOAD_SHARE))
					hat->hat_rss += NPTEPERPT;
				hwpp->hwpp_pde = *pte;
				hwpp->hwpp_mapping = I86MMU_LGPAGE_LOCKMAP;
				ASSERT(flags & HAT_LOAD_LOCK);
				a_pte = a_lpte = &userpagedir[hwpp_index];
				if (pp)
					i86_mlist_enter(pp);
				LOAD_PTE(a_pte, *pte);
				hwpp->hwpp_pte = hwpp->hwpp_lpte = a_pte;
				HATSTAT_INC(HATSTAT_LGPAGE_LOCKMAP);
				goto skip_update_pte;
			}
			hwpp = hat_hwpp_cache_alloc(hat, hwpp_index,
					&oldhat, 1);
			if ((flags & (HAT_LOAD_LOCK|HAT_LOAD_SHARE)) ==
				(HAT_LOAD_LOCK|HAT_LOAD_SHARE)) {
				hat_mark_sharedpgtbl(hat, hwpp);
			}
		}
		index = PAGETABLE_INDEX((uint_t)addr);
		a_pte = &hwpp->hwpp_pte[index];
		a_lpte = &hwpp->hwpp_lpte[index];
	}

	if (pp)
		i86_mlist_enter(pp);

	remap = pte_valid(a_lpte);
	if (pp != NULL) {
		((pte_t *)pte)->NonCacheable = (PP_ISNC(pp));
		ASSERT(!(remap && (MAKE_PFNUM(a_lpte) != MAKE_PFNUM(pte))));
	}

	if (remap) {
		*pte |= *a_lpte & PTE_RM_MASK;
	} else {
		if (hat != kernel_hat) {
			hwpp->hwpp_numptes++;
			hat->hat_rss++;
		}
	}
	if (flags & HAT_LOAD_LOCK) {
		if (hat != kernel_hat) {
			hwpp->hwpp_lockcnt++;
			if (!hat->hat_pagedir) {
				oldhat = hat_switch2ppp(hat);
				a_lpte = &hwpp->hwpp_lpte[index];
			}
		}
	}
	if (flags & HAT_LOAD_NOCONSIST) {
		if (!remap || getpte_noconsist(a_lpte))
			setpte_noconsist(pte);
	} else {
		/*
		 * if remapping a pte w/o noconsist that was previously set
		 * with noconsist, set remap=0 so that the mapping is hme_added
		 */
		if (remap && getpte_noconsist(a_lpte))
			remap = 0;
	}
	(void) hat_update_pte(hat, a_lpte, *pte, addr, PTE_RM_MASK);

skip_update_pte:
	if (pp && !remap && !large_page && !(flags & HAT_LOAD_NOCONSIST)) {
		/*
		 * We are loading a new translation. Add the
		 * translation to the list for this page.
		 */
		if (flags & HAT_LOAD_SHARE) {
			if (hat_accurate_pmapping)
				hme_add(pp, a_pte, pte_ro(a_lpte));
		} else {
			hme_add(pp, a_pte, pte_ro(a_lpte));
		}
	}

	if (pp)
		i86_mlist_exit(pp);

	if (hat == kernel_hat)
		return;

	if (hat->hat_as != curthread->t_procp->p_as) {
		i86_restore_hat(oldhat);
		return;
	}

	if (!hat->hat_pagedir) {
		kpreempt_disable();
		pagedir = CPU->cpu_pagedir;
		if ((pagedir[index] & ~PTE_RM_MASK) != hwpp->hwpp_pde) {
			hat_setup_internal(hat);
		}
		kpreempt_enable();
		return;
	}
	index = hwpp->hwpp_index;
	ASSERT(index * sizeof (pteval_t) < user_pdelen);
	ASSERT(!curthread->t_mmuctx || (curthread->t_mmuctx ==
		(uint_t)&curthread->t_procp->p_as->a_hat->hat_ctx) ||
		(flags & HAT_LOAD_SHARE));
	if ((userpagedir[index] & ~PTE_RM_MASK) != hwpp->hwpp_pde) {
#ifdef PTE36
		/*
		 * userpagedir is inconsistent in between the 2 32 bit updates
		 * in LOAD_PTE if currently valid. Invalidate it first.
		 */
		INVALIDATE_PTE(&userpagedir[index]);
#endif
		LOAD_PTE(&userpagedir[index], hwpp->hwpp_pde);
	}
}


/*
 * Sync the referenced and modified bits of the page struct with the pte.
 * Clears the bits in the pte.  Also, synchronizes the Cacheable bit in
 * the pte with the noncacheable bit in the page struct.
 *
 * Any change to the PTE requires the TLBs to be flushed, so subsequent
 * accesses or modifies will cause the memory image of the PTE to be
 * modified.
 */
static void
hat_ptesync(hat, pp, vaddr, pte, flags, mlistheld)
	struct hat	*hat;
	page_t		*pp;
	caddr_t 	vaddr;
	pteval_t	*pte;
	int		flags;
	int		mlistheld;
{
	pteval_t		newpte;
	int 			nosync, rm, lrm, skip_hat_update_pte;

	HATPRINTF(4, ("hat_ptesync: hat %x pp %x addr %x ptev %x\n",
			hat, pp, vaddr, *pte));
	/*
	 * Get the ref/mod bits from the hardware page table,
	 */
	rm = *pte & PTE_RM_MASK;
	skip_hat_update_pte = 0;
	nosync = getpte_nosync(pte);
	if (flags & HAT_INVSYNC)
		newpte = MMU_STD_INVALIDPTE;
	else {
		if (rm == 0)
			/*
			 * If REF & MOD are not set, we have nothing to do
			 */
			return;

		newpte = *pte;
		if (nosync || !(flags & HAT_RMSYNC)) {
			/*
			 * we are not changing the ref or mod bit so dont
			 * update just pick those bits from hwpte
			 */
			skip_hat_update_pte = 1;
			HATPRINTF(4, ("hat_ptesync: skipping update pte\n"));
		} else {
			newpte &= ~PTE_RM_MASK;
		}
	}

	/*
	 * Get the hardware ref/mod bits, combine them with the software
	 * bits, and use the combination as the current ref/mod bits.
	 */
	if (!skip_hat_update_pte) {
		if (pp && !mlistheld)
			i86_mlist_enter(pp);
		rm = hat_update_pte(hat, pte, newpte, vaddr, 0);
	}


	if (pp != NULL) {
		if ((flags & (HAT_RMSYNC|HAT_RMSTAT)) && rm) {
			if (rm & PTE_REF_MASK)
				lrm = P_REF;
			else lrm = 0;
			if (rm & PTE_MOD_MASK)
				lrm |= P_MOD;
			if (flags & HAT_RMSYNC && hat->hat_stat)
				hat_setstat(hat->hat_as, vaddr, PAGESIZE, lrm);
			if (!nosync)
				PP_SETRM(pp, lrm);
		}
		if (!skip_hat_update_pte && !mlistheld)
			i86_mlist_exit(pp);
	}

}

/*
 * Unload the pte. If required, sync the referenced & modified bits.
 * If it's the last pte in the page table, and the table isn't locked,
 * free it up.
 */
static void
hat_pteunload(hat, pp, vaddr, lpte, pte, holdmlist)
	struct hat	*hat;
	struct page	*pp;
	caddr_t		vaddr;
	pteval_t	*lpte;
	pteval_t	*pte;
	int		holdmlist;
{
	struct hwptepage *hwpp;
	int		freeing, consist;
	uint_t		gen;
	int		oldhat;

	ASSERT((hat == kernel_hat)||MUTEX_HELD(&hat->hat_mutex));
	ASSERT(pp == NULL || I86_MLIST_LOCKED(mach_pp));
	ASSERT(pp ? pte_valid(lpte) : 1);

	HATPRINTF(4, ("pteunload: hat %x adr %x lpte %x pte %x val %x\n",
			hat, vaddr, lpte, pte, *lpte));

	consist = !getpte_noconsist(lpte);

	if (pte_readonly_and_notdirty(lpte)) {
		/* We dont have to sync up MOD bits, just clear the entry */
		INVALIDATE_PTE(lpte);
		freeing = hat->hat_flags & I86MMU_FREEING;
		kpreempt_disable();
		if (freeing || !(hat->hat_cpusrunning & ~CPU->cpu_mask)) {
			mmu_tlbflush_entry(vaddr);
		} else {
			TLBFLUSH_BRDCST(hat, vaddr, gen);
			TLBFLUSH_WAIT(gen);
		}
		kpreempt_enable();
	} else hat_ptesync(hat, pp, vaddr, lpte, HAT_INVSYNC | HAT_RMSYNC,
		MLISTHELD);

	/* No hme if it was a NOCONSIST mapping  or no pp */
	if (pp && consist) {
		hme_sub(pp, pte, holdmlist);
	}

	if (vaddr < (caddr_t)kernelbase) {
		hat->hat_rss--;
		oldhat = i86_switch_hat(hat);
		hwpp = hat_hwppfind(hat, addrtohwppindex(vaddr));
		if (!--hwpp->hwpp_numptes) {
			hat_hwpp_cache_free(hat, hwpp, 1);
		}
		i86_restore_hat(oldhat);
	}
}

/*
 * Unload all hardware translations that maps page `pp'.
 */
int
hat_pageunload(struct page *pp, uint_t forceflag)
{
	caddr_t		addr;
	struct hment	*hmecur, *hmefree;
	int		pfn = mach_pp->p_pagenum;
	int		oldhat;
	struct hat	*hat;
	pteval_t	*pte;

#ifdef	lint
	forceflag = forceflag;
#endif

	i86_mlist_enter(pp);

	if (mach_pp->p_mapping == NULL) {
		ASSERT(!mach_pp->p_share);
		i86_mlist_exit(pp);
		return (0);
	}

	hmefree = NULL;
	hmecur = NULL;

	HATPRINTF(4, ("hat_pageunload: oldhat %x page %x\n",
				curthread->t_procp->p_as->a_hat, pp));
	oldhat = i86_switch_hat(pteashat);


	while (hmecur = hme_getnext(pp, hmecur, &hmefree)) {

		if (IS_PTEPTR((uint32_t)hmecur))
			pte = (pteval_t *)PPMAPPTR((uint32_t)hmecur);
		else
			pte = hmecur->hme_pte;

		/* pte2hat_addr grabs hat_mutex */
		if (!(hat = pte2hat_addr(pte, pfn, &addr, TRYLOCK))) {
			if (addr == NULL)
				continue;
			/*
			 * drop the mlist lock & then grab the hat lock
			 */
			i86_mlist_exit(pp);
			HMEFREE(hmefree);
			hmefree = NULL;
			hmecur = NULL;
			hat = pte2hat_addr(pte, pfn, &addr, LOCK);
			i86_mlist_enter(pp);
			if (hat == NULL) {
				continue;
			}
			/*
			 * since mlist was dropped, verify that the pte
			 * has not been changed before calling pteunload
			 */
			if ((PTEVAL2PFN(*pte) != pfn) || !pte_valid(pte)) {
				continue;
			}
		}
		hat_pteunload(hat, pp, addr, pte, pte, HOLDMLIST);
		if (hat != kernel_hat)
			mutex_exit(&hat->hat_mutex);
	}
	mach_pp->p_share = 0;		/* ### ASSERT */
	/* Do a fast delete since all of them are invalid */
	if (!IS_PTEPTR(mach_pp->p_mapping))
		hmecur = (struct hment *)PPMAPPTR(mach_pp->p_mapping);
	mach_pp->p_mapping = NULL;
	mach_pp->p_deleted = 0;
	i86_mlist_exit(pp);
	i86_restore_hat(oldhat);
	HMEFREE(hmefree);
	HMEFREE(hmecur);
	return (0);
}

/*
 * Allocates hardware pte page
 */
static struct hwptepage *
hat_hwppalloc(hat, hwpp_index)
struct hat	*hat;
int		hwpp_index;
{
	hwptepage_t	*hwpp;

	hwpp = hat_kmem_zalloc(sizeof (*hwpp));
	hat_hwppinit(hat, hwpp, hwpp_index);
	return (hwpp);
}


int	hat_optimfree = 1;

static void
hat_hwppdelete(hat, hwpp)
struct	hat	*hat;
hwptepage_t	*hwpp;
{
	hat->hat_numhwpp--;
	if (hwpp->hwpp_next)
		hwpp->hwpp_next->hwpp_prev = hwpp->hwpp_prev;
	if (hwpp->hwpp_prev) {
		hwpp->hwpp_prev->hwpp_next = hwpp->hwpp_next;
	} else {
		hat->hat_hwpp = hwpp->hwpp_next;
	}
}

/*
 * free the hwpp back to freelist.
 */
static void
hat_hwppfree(hat, hwpp, updatepde)
struct	hat	*hat;
hwptepage_t	*hwpp;
int		updatepde;
{
	uint_t		index;
	int		freeing;

	ASSERT((hat == kernel_hat)||MUTEX_HELD(&hat->hat_mutex));
	ASSERT(hwpp->hwpp_index != -1);
	freeing = hat->hat_flags & I86MMU_FREEING;

	if (hwpp->hwpp_mapping) {
		if (hwpp->hwpp_mapping & I86MMU_OWN_PTBL) {
			HATSTAT_DEC(HATSTAT_4K_LOCKMAP);
			hat_pagetablefree(hat, hwpp);
		} else {
			if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
				HATSTAT_DEC(HATSTAT_LGPAGE_LOCKMAP);
			}
		}
	} else
		hat_pagetablefree(hat, hwpp);

	if (hat->hat_pagedir) {
		index = hwpp->hwpp_index;
		INVALIDATE_PTE(&userpagedir[index]);
		INVALIDATE_PTE(&userptetable[index]);
		ASSERT(cr3() == hat->hat_pdepfn);
		userhwppmap[index] = NULL;
	}
	if (hat_optimfree && freeing) {
		/* make sure we won't return this in hwppfind */
		hwpp->hwpp_index = (uint_t)-1;
		/* and in setup */
		hwpp->hwpp_pde = MMU_STD_INVALIDPTE;
		return;
	}

	index = hwpp->hwpp_index;
	hat_hwppdelete(hat, hwpp);
	KMEM_FREE_STAT(hwpp, sizeof (*hwpp));

	if (updatepde)
		hat_update_pde(hat, index, MMU_STD_INVALIDPTE);
}

/*
 * allocate a 4K pagetable for 'hwpp'. Make a new mapping for this in
 * userptemap and in ptearena.
 */
void
hat_pagetablealloc(struct hat *hat, struct hwptepage *hwpp)
{
	pteptr_t	ptepageaddr;
	int		i;
	uint_t		oldhat;
	uint_t		gen;

	ASSERT((hat == kernel_hat)||MUTEX_HELD(&hat->hat_mutex));

	/* get pp to back the virtual address range in userptemap & ptearena */
	hwpp->hwpp_pte_pp = hat_page_cache_alloc(hat);


	hwpp->hwpp_pte = hat_vmem_alloc(ptearena, MMU_PAGESIZE);
	hwpp->hwpp_pfn = ((machpage_t *)(hwpp->hwpp_pte_pp))->p_pagenum;
	hwpp->hwpp_pde = PTEOF_C(hwpp->hwpp_pfn, MMU_STD_SRWXURWX);

	i = btop((uintptr_t)hwpp->hwpp_pte);

	HATPRINTF(4, ("pgtblalloc: hat %x hwpp %x hwpp_pte %x\n",
		hat, hwpp, hwpp->hwpp_pte));

	oldhat = i86_switch_hat(pteashat);
	/* Find index into pt_ptable and map it */
	pt_ptable[i] = PTEOF_C(hwpp->hwpp_pfn, MMU_STD_SRWX);
	/* let other CPUs see this change */
	TLBFLUSH_BRDCST(pteashat, (caddr_t) hwpp->hwpp_pte, gen);
	/* Fill index into the pt_hwpp array. There is one for each pte page */
	pt_hwpp[i] = hwpp;
	/* setup pte2hat mapping */
	pte2hati[i] = hat->hat_index;
	i86_restore_hat(oldhat);

	HATSTAT_INC(HATSTAT_PGTBLALLOC);

	/* add to userptemap */
	if (hat->hat_pagedir) {
		HATSTAT_INC(HATSTAT_PGTBLALLOC_PGDIR);
		ptepageaddr = &userptemap[hwpp->hwpp_index * NPTEPERPT];
		hwpp->hwpp_lpte = ptepageaddr;
		userptetable[hwpp->hwpp_index] = hwpp->hwpp_pde;
	} else {
		hwpp->hwpp_lpte = hat_vmem_alloc(heap_arena, MMU_PAGESIZE);
		hat_memload(kas.a_hat, (caddr_t)hwpp->hwpp_lpte,
			hwpp->hwpp_pte_pp, PROT_READ | PROT_WRITE | HAT_NOSYNC,
			HAT_LOAD_NOCONSIST | HAT_LOAD_LOCK);
	}
	/* Make sure other CPUs have seen the change in pte mapping */
	TLBFLUSH_WAIT(gen);
}

/*
 * free 4K pagetable attached to 'hwpp'
 */
static void
hat_pagetablefree(hat, hwpp)
	struct hat *hat;
	struct hwptepage *hwpp;
{
	int		i;
	uint_t		oldhat;

	i = btop((uintptr_t)hwpp->hwpp_pte);

	ASSERT((cr3() == hat->hat_pdepfn)||(hat->hat_pagedir == NULL));
	ASSERT((hat == kernel_hat)||MUTEX_HELD(&hat->hat_mutex));

	oldhat = i86_switch_hat(pteashat);
	/* Free index into the pt_hwpp array. There is one for each pte page */
	pt_hwpp[i] = &pt_invalid_hwpp;
	pt_ptable[i] = pt_invalid_pteptr;
	pte2hati[i] = 0;			/* index to kernel hat */
	i86_restore_hat(oldhat);

	hat_vmem_free(ptearena, hwpp->hwpp_pte, MMU_PAGESIZE);
	hwpp->hwpp_pfn = NULL;
	hwpp->hwpp_pde = MMU_STD_INVALIDPTE;
	hwpp->hwpp_pte = NULL;
	HATSTAT_DEC(HATSTAT_PGTBLALLOC);
	if (!hat->hat_pagedir) {
		hat_unload(kas.a_hat, (caddr_t)hwpp->hwpp_lpte, MMU_PAGESIZE,
			HAT_UNLOAD_UNLOCK);
		hat_vmem_free(heap_arena, hwpp->hwpp_lpte, MMU_PAGESIZE);
	}
	hwpp->hwpp_lpte = NULL;

	hat_page_cache_free(hwpp->hwpp_pte_pp);
	hwpp->hwpp_pte_pp = NULL;
}

/*
 * change pde at index to ptev. Change cpu_numpdes if ptev is invalid
 * We have either captured the cpu or we are on it. we can clear the pde
 * now. We dont care if this cpu has kernel_only_cr3 loaded. As long as
 * the cpu_current_hat is pointing to 'hat' its safe to clear the pde.
 */
static void
hat_change_pde(hat, cpup, index, ptev)
struct hat	*hat;
struct cpu	*cpup;
uint_t		index;
pteval_t	ptev;
{
	uint_t		j;
	pteval_t	*pagedir;

	if (cpup->cpu_current_hat == hat) {
		pagedir = cpup->cpu_pagedir;

		/* if load or remap, no decrement */
		if (pte_valid(&ptev)) {
			LOAD_PTE(&pagedir[index], ptev);
			return;
		}
		INVALIDATE_PTE(&pagedir[index]);

		ASSERT(cpup->cpu_numpdes < hat_perprocess_pagedir_max);
		for (j = 0; j < cpup->cpu_numpdes; j++) {
			if (index != cpup->cpu_pde_index[j])  {
				ASSERT(pte_valid(
					&pagedir[cpup->cpu_pde_index[j]]));
				continue;
			}
			cpup->cpu_pde_index[j] =
				cpup->cpu_pde_index[cpup->cpu_numpdes - 1];
			cpup->cpu_numpdes--;
		}
	}
}

/*
 * Called when we load/remap large page xlations
 * We need to clear a page directory entry on all those cpu's that 'as'
 * is currently active.
 *
 * called from hwppfree/alloc
 */
static void
hat_update_pde(struct hat *hat, uint_t index, pteval_t ptev)
{
	int		i;
	struct cpu	*cpup, *curcpu;
	pteval_t	*pagedir;
	ulong_t		mask;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	kpreempt_disable();
	if (hat->hat_pagedir) {
		pagedir = userpagedir;
		curcpu = CPU;
		if (pte_valid(&ptev)) {
			LOAD_PTE(&pagedir[index], ptev);
		} else {
			INVALIDATE_PTE(&pagedir[index]);
		}
		/*
		 * We expect the following CAPTURE_CPUS() to force all cpu's on
		 * which as is currently running to invalidate all of its tlb.
		 */
		if (hat->hat_cpusrunning & ~curcpu->cpu_mask) {
			HATPRINTF(2, ("hat_update_pde: CAP CPU mask %x\n",
				hat->hat_cpusrunning));
			CAPTURE_CPUS(hat);
			RELEASE_CPUS;
		}
		/* reload cpu's cr3 context (culd be different from hat's) */
		LOAD_CR3(curcpu, curcpu->cpu_curcr3);
		kpreempt_enable();

		HATSTAT_INC(HATSTAT_CR3_LOAD);
		return;
	}
	/*
	 * No private page table. Deal with unloading pde from
	 * our cpu_pagedir & other CPUs
	 */
	ASSERT(index * sizeof (pteval_t) < user_pdelen);
	HATPRINTF(2, ("hat_update_pde: CAP CPU mask %x\n",
		hat->hat_cpusrunning));

	curcpu = CPU;
	hat_change_pde(hat, curcpu, index, ptev);
	LOAD_CR3(curcpu, curcpu->cpu_curcr3);
	mask = hat->hat_cpusrunning & ~curcpu->cpu_mask;

	if (!mask) {
		kpreempt_enable();
		HATSTAT_INC(HATSTAT_CR3_LOAD);
		return;
	}
	CAPTURE_SELECTED_CPUS(mask);
	while (i = highbit(mask)) {
		i--;
		ASSERT(cpu[i]);
		cpup = cpu[i];
		mask &= ~cpup->cpu_mask;
		hat_change_pde(hat, cpup, index, ptev);
	}
	RELEASE_CPUS;
	LOAD_CR3(curcpu, curcpu->cpu_curcr3);
	kpreempt_enable();

	HATSTAT_INC(HATSTAT_CR3_LOAD);
}


static ulong_t
hat_getpfnum_locked(struct hat *hat, caddr_t addr)
{
	struct	hwptepage 	*hwpp;
	pteval_t	*pte;
	pteval_t	pteval;
	uint_t		pfn;
	int		oldhat;

	if ((addr < (caddr_t)kernelbase) && hat->hat_pagedir) {
		oldhat = i86_switch_hat(hat);
		hwpp = hat_hwppfind(hat, addrtohwppindex(addr));
		if (!hwpp) {
			i86_restore_hat(oldhat);
			return (PFN_INVALID);
		}
		if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
			pteval = *hwpp->hwpp_lpte;
			pfn = PTEVAL2PFN(pteval) +
				PAGETABLE_INDEX((uint_t)addr);
		} else {
			pteval =
				hwpp->hwpp_lpte[PAGETABLE_INDEX((uint_t)addr)];
			pfn = PTEVAL2PFN(pteval);
		}
		if (!pte_valid(&pteval))
			pfn = (uint_t)PFN_INVALID;
		i86_restore_hat(oldhat);
		return ((ulong_t)pfn);
	}
	pte = hat_ptefind(hat, addr);
	pfn = (pte == NULL || !pte_valid(pte))
	    ? PFN_INVALID : MAKE_PFNUM(pte);
	return (pfn);
}


static int
hat_is_pp_largepage(page_t **ppa)
{
	int pfn;
	int i;

	/* easy if the page is already marked as largepage */
	if (((machpage_t *)ppa[0])->p_flags & P_LARGEPAGE)
		return (1);
	pfn = ((machpage_t *)ppa[0])->p_pagenum;
	for (i = 0; i < NPTEPERPT; i++, pfn++) {
		if (((machpage_t *)ppa[i])->p_pagenum != pfn) {
			return (0);
		}
	}
	return (1);
}


/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 */
struct hat *
hat_alloc(struct as *as)
{
	register struct hat *hat;

	mutex_enter(&hat_hat_lock);
	hat = hatfree;
	if (!hat) {
		ushort_t	i = hatidai++;

		mutex_exit(&hat_hat_lock);

		/* None in our free list. kmem_alloc one */
		hat = kmem_zalloc(sizeof (struct hat), KM_SLEEP);
		hat_kmemcount += sizeof (struct hat);

		/* hatida init by hat_init - cpu hats not inserted */

		if (hatida) {
			hat->hat_index = i;
			hatida[i] = hat;
		}

		mutex_init(&hat->hat_mutex, NULL, MUTEX_DEFAULT, NULL);
	} else {
		hatfree = hat->hat_next;
		hat_hatcount--;
		mutex_exit(&hat_hat_lock);
		/* cannot bzero hat as someone may refer to it's old cr3 */
		hat->hat_flags = 0;
		hat->hat_cpusrunning = 0;
		hat->hat_rss = 0;
		hat->hat_next = 0;
		hat->hat_prev = 0;
		hat->hat_stat = 0;
	}

	hat->hat_as = as;
	hat->hat_critical = HAT_INVALID_ADDR;

	if (as == &kas) {
		return (hat);
	}
	mutex_enter(&hat_hat_lock);
	hat->hat_next = all_hat.hat_next;
	all_hat.hat_next = hat;
	hat->hat_next->hat_prev = hat;
	hat->hat_prev = &all_hat;
	mutex_exit(&hat_hat_lock);
	return (hat);
}

void
hat_free_start(hat)
register struct hat *hat;
{

	HATPRINTF(0x02, ("hat_free_start: hat %x thread %x\n", hat, curthread));
	mutex_enter(&hat->hat_mutex);

	hat->hat_flags |= I86MMU_FREEING;
	if (!hat->hat_pagedir) {
		kpreempt_disable();
		/* unload pdes on this CPU */
		hat_unload_pdes(CPU);
		/* unload pdes on all other CPUs */
		hat_unload_all_pdes(hat);
		kpreempt_enable();
	}
	/* set hat_cpusrunning to 0 to avoid bogus x-calls */
	hat->hat_cpusrunning = 0;

	mutex_exit(&hat->hat_mutex);
}

/*
 * called after hat_free_start() and seg_unmap()
 */
void
hat_free_end(struct hat *hat)
{
	struct	hwptepage *hwpp, *hwppn;
	struct	cpu	*cpup;
#ifdef DEBUG
	uint_t		index = 0;
#endif

	HATPRINTF(0x02, ("hat_free_end: hat %x thread %x\n", hat, curthread));
	mutex_enter(&hat->hat_mutex);

	hwpp = hat->hat_hwpp;
	while (hwpp) {
		hwppn = hwpp->hwpp_next;
		ASSERT(hwpp->hwpp_numptes == 0);
		KMEM_FREE_STAT(hwpp, sizeof (*hwpp));
		hwpp = hwppn;
#ifdef	DEBUG
		index++;
#endif
	}
	ASSERT(index == hat->hat_numhwpp);
	hat->hat_hwpp = NULL;
	hat->hat_numhwpp = 0;

	if (hat->hat_pagedir) {
		kpreempt_disable();
		cpup = CPU;
		hat->hat_pagedir = NULL;
		hat->hat_pdepfn = kernel_only_cr3;
		hat->hat_ctx.ct_hat = NULL;
		curthread->t_mmuctx = 0;
		LOAD_CR3(cpup, (&cpup->cpu_ctx));
		if (hat->hat_cpusrunning & ~cpup->cpu_mask) {
			/*
			 * capture cpus below will cause a reload of cpu_cr3 on
			 * all cpus running with our cr3. Needed before freeing
			 */
			CAPTURE_CPUS(hat);
			RELEASE_CPUS;
		}
		kpreempt_enable();
	}
	hat->hat_cpusrunning = 0;
	/* Now free pages used for mapping in pagedir, hwpp pointers & ptes */
	if (hat->hat_ppp) {
		hat_freeppp(hat->hat_ppp);
		hat->hat_ppp = NULL;
	}
	mutex_exit(&hat->hat_mutex);

	mutex_enter(&hat_hat_lock);
	hat->hat_prev->hat_next = hat->hat_next;
	hat->hat_next->hat_prev = hat->hat_prev;
	if (hat == hat_searchp)
		hat_searchp = hat->hat_next;
	hat->hat_next = hatfree;
	hatfree = hat;
	hat_hatcount++;
	mutex_exit(&hat_hat_lock);
}

/*
 * Duplicate the translations of an as into another newas
 */
/*ARGSUSED*/
int
hat_dup(struct hat *hat, struct hat *hat_c, caddr_t addr,
    size_t len, uint_t flags)
{
	return (0);
}


/*
 * VM layer does not make this call
 */
void
hat_swapin(struct hat *hat)
{
#ifdef	lint
	hat = hat;
#endif
}


/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 */
void
hat_swapout(struct hat *hat)
{
	struct hwptepage *hwpp, *nexthwpp;
	mutex_enter(&hat->hat_mutex);
	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		if (!hwpp->hwpp_lockcnt)
			hat_hwppunload(hat, hwpp);
	}
	mutex_exit(&hat->hat_mutex);
}


void
hat_map(struct hat *hat, caddr_t addr, size_t len, uint_t flags)
{

#ifdef	lint
	hat = hat;
	addr = addr;
	len = len;
	flags = flags;
#endif
}

void
hat_sync(struct hat *hat, caddr_t addr, size_t len, uint_t clrflg)
{
	caddr_t			a, ea;
	pteval_t		*pte;
	page_t			*pp;
	int		oldhat;

	mutex_enter(&hat->hat_mutex);

	oldhat = i86_switch_hat(hat);
	for (a = addr, ea = addr + len; a < ea; a += MMU_PAGESIZE) {
		pte = hat_ptefind(hat, a);
		if ((pte == NULL) || !pte_valid(pte))
			continue;
		pp = page_numtopp_nolock(hat_getpfnum_locked(hat, a));

		hat_ptesync(hat, pp, a, pte,
				clrflg ? HAT_RMSYNC : HAT_RMSTAT, MLISTNOTHELD);
	}

	i86_restore_hat(oldhat);
	mutex_exit(&hat->hat_mutex);
}

/*
 * Make a mapping at addr to map page pp with protection prot.
 */
void
hat_memload(struct hat *hat, caddr_t addr, page_t *pp,
    uint_t attr, uint_t flags)
{
	pteval_t	ptev;
	uint_t		prot = attr & HAT_PROT_MASK;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	ASSERT(PAGE_LOCKED(pp));
	if (PP_ISFREE(pp))
		cmn_err(CE_PANIC,
		    "hat_memload: loading a mapping to free page %x", (int)pp);
	if (hat->hat_stat)
		hat_resvstat(MMU_PAGESIZE, hat->hat_as, addr);

	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE,
		    "hat_memload: called with unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	ASSERT(prot < sizeof (hat_vprot2p));
	ptev = PTEOF_CS(page_pptonum(pp), hat_vprot2p[prot],
		((attr & HAT_NOSYNC) ? 1 : 0));
	if (hat != kernel_hat) {
		mutex_enter(&hat->hat_mutex);
		hat_pteload(hat, addr, pp, &ptev, flags);
		mutex_exit(&hat->hat_mutex);
	} else {
		hat_pteload(hat, addr, pp, &ptev, flags);
	}

}

/*
 * Cons up a pteval_t using the device's pf bits and protection
 * prot to load into the hardware for address addr; treat as minflt.
 */
void
hat_devload(struct hat *hat, caddr_t addr, size_t len, ulong_t pf,
    uint_t attr, int flags)
{
	uint_t 		prot = attr & HAT_PROT_MASK;
	uint_t		mem_order =  attr & HAT_ORDER_MASK;
	pteval_t 	ptev;
	page_t		*pp;
	int		pf_mem, cache_enable, write_thru, pati;
	int		pagesize, pp_incr;

	ASSERT(prot < sizeof (hat_vprot2p));
	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	if (len == 0)
		cmn_err(CE_PANIC, "hat_devload: zero len");
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_devload: unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	if (hat->hat_stat)
		hat_resvstat(len, hat->hat_as, addr);

	if (IS_P2ALIGNED(addr, LARGEPAGESIZE) &&
	    IS_P2ALIGNED(len, LARGEPAGESIZE) &&
	    IS_P2ALIGNED(pf, NPTEPERPT)) {
		pagesize = LARGEPAGESIZE;
		pp_incr = NPTEPERPT;
	} else {
		pagesize = MMU_PAGESIZE;
		pp_incr = 1;
	}
	pati = 0;
	switch (mem_order) {
	case HAT_STRICTORDER:
	case HAT_UNORDERED_OK:
		cache_enable = 0;
		write_thru = 1;
		break;
	case HAT_MERGING_OK:
		cache_enable = 0;
		if (x86_feature & X86_PAT) {
			/*
			 * PAT6 is programmed to provide Write-Combine mode.
			 * Please see i86/sys/x86_archext.h
			 */
			pati = 1;
			write_thru = 0;
		} else
			write_thru = 1;
		break;
	case HAT_LOADCACHING_OK:
		cache_enable = 1;
		write_thru = 1;
		break;
	case HAT_STORECACHING_OK:
		cache_enable = 1;
		write_thru = 0;
		break;
	}

	/*
	 * Device memory not backed by devpage structs.
	 */
	if (hat != kernel_hat)
		mutex_enter(&hat->hat_mutex);
	while (len) {
		if (((pf_mem = pf_is_memory(pf)) != 0) &&
		    !(flags & HAT_LOAD_NOCONSIST)) {
			pp = page_numtopp_nolock(pf);
			if (!pp) {
				flags |= HAT_LOAD_NOCONSIST;
			}
		} else
			pp = NULL;
		if (pf_mem && hat_devload_cache_system_memory) {
			cache_enable = 1;
			write_thru = 0;
		}
		/*
		 * In 2.6, hat_devload() would setup pte with PCD bit
		 * turned on for system memory and with the bit turned off
		 * for device memory. If the global tunables have been
		 * configured this way, we will ignore information from
		 * ddi_device_acc_attr.
		 */
		if (!hat_devload_cache_system_memory &&
		    hat_devload_cache_device_memory) {
		    if (pf_mem)
			cache_enable = hat_devload_cache_system_memory;
		    else
			cache_enable = hat_devload_cache_device_memory;
		    write_thru = 0;
		    pati = 0;
		}
		if (pagesize == LARGEPAGESIZE) {
			/*
			 * Make a device pte with pfn = pp, no os spec bits,
			 * mod & ref = 0, pati, ce, wt, accessperms according to
			 * prot and marked present
			 */
			ptev = LGPG_PDE(pf, 0, hat_vprot2p[prot],
				pati, cache_enable, write_thru, 1);
		} else {
			/*
			 * Make a device pte with pfn = pp, no os spec bits,
			 * mod & ref = 0, pati, ce, wt, accessperms according to
			 * prot and marked present
			 */
			ptev = MRDPTEOF(pf, 0, 0, 0, pati,
			    cache_enable, write_thru, hat_vprot2p[prot], 1);
		}
		if (attr & HAT_NOSYNC)
			setpte_nosync(&ptev);
		hat_pteload(hat, addr, pp, &ptev, flags);
		pf += pp_incr;
		addr += pagesize;
		len -= pagesize;
	}
	if (hat != kernel_hat)
		mutex_exit(&hat->hat_mutex);
}

/*
 * Set up translations to a range of virtual addresses backed by
 * physically contiguous memory. The MMU driver can take advantage
 * of underlying hardware to set up translations using larger than
 * 4K bytes size pages. The caller must ensure that the pages are
 * locked down and that their identity will not change.
 */
void
hat_memload_array(struct hat *hat, caddr_t addr, size_t len,
    page_t **ppa, uint_t attr, uint_t flags)
{

	page_t		*pp;
	uint_t 		prot = attr & HAT_PROT_MASK;
	pteval_t 	ptev;
	int		pagesize, pp_incr;


	ASSERT(prot < sizeof (hat_vprot2p));
	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_memload: unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	if (hat->hat_stat)
		hat_resvstat(len, hat->hat_as, addr);

	ASSERT((len & MMU_PAGEOFFSET) == 0);
	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);
	for (; len; ppa += pp_incr, addr += pagesize, len -= pagesize) {
		pp = *ppa;
		/*
		 * We ignore HAT_LOAD_CONTIG below as we know better
		 * what we can do.
		 */
		if ((len >= LARGEPAGESIZE) && (addr < (caddr_t)kernelbase) &&
						hat_is_pp_largepage(ppa)) {
			ASSERT(IS_P2ALIGNED(addr, LARGEPAGESIZE));
			ASSERT(IS_P2ALIGNED(mach_pp->p_pagenum, NPTEPERPT));
			pagesize = LARGEPAGESIZE;
			pp_incr = NPTEPERPT;
		} else {
			pagesize = MMU_PAGESIZE;
			pp_incr = 1;
		}
		if (flags & HAT_LOAD_REMAP) {
			if (hat_ptereload(hat, addr, pp, flags) == 0) {
				continue;
			}
		}
		if (pagesize == LARGEPAGESIZE) {
			int	ce = 1;
		/* pati = 0, cache_enable = 1, write_thru = 0, present =1 */
			ptev = LGPG_PDE(page_pptonum(pp), 0, hat_vprot2p[prot],
				0, ce, 0, 1);
		} else {
			ptev = PTEOF_CS(page_pptonum(pp), hat_vprot2p[prot],
				((attr & HAT_NOSYNC) ? 1 : 0));
		}
		hat_pteload(hat, addr, pp, &ptev, flags);
	}
	if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
}

/*
 * Release one hardware address translation lock on the given address.
 */
void
hat_unlock(struct hat *hat, caddr_t addr, size_t len)
{

	pteval_t		*pte;
	uint_t 			a, newa;
	int			hwpp_index;
	struct	hwptepage 	*hwpp;
	int			span;
	int		oldhat;

	ASSERT((len & MMU_PAGEOFFSET) == 0);

	if (hat == kernel_hat)
		return;
	mutex_enter(&hat->hat_mutex);

	hwpp_index = 0;
#ifdef	lint
	span = MMU_PAGESIZE;
#endif
	/*
	 * We may be called due to SOFTUNLOCK from another
	 * process. procfs needs this, see prusrio()
	 */
	oldhat = i86_switch_hat(hat);
	for (a = (uint_t)addr; a < (uint_t)addr + len; a += span) {

		span = MMU_PAGESIZE;
		hwpp_index = addrtohwppindex(a);

		hwpp = hat_hwppfind(hat, hwpp_index);

		if (hwpp->hwpp_mapping) {
			newa = ALIGN_TONEXT_LGPAGE(a);
			span = newa - a;
			continue;
		}
		pte = &hwpp->hwpp_lpte[PAGETABLE_INDEX(a)];
		/*
		 * If the address was mapped, we need to unlock
		 * the page table it resides in.
		 */
		if (pte_valid(pte))
			hwpp->hwpp_lockcnt--;
	}
	i86_restore_hat(oldhat);
	mutex_exit(&hat->hat_mutex);
}

#define	HAT_LOAD_ATTR		1
#define	HAT_SET_ATTR		2
#define	HAT_CLR_ATTR		3
#define	HAT_LOAD_ATTR_PGDIR	4


static void
hat_updateattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr, int what)
{
	register caddr_t 	ea;
	pteval_t 		*pte = NULL, newpte, *pde;
	register uint_t 	pprot, ppprot;
	int			span;
	uint_t   		a, newa;
	struct	hwptepage 	*hwpp = NULL;
	pteval_t		*pagedir;
	uint_t			hwpp_index;
	uint_t 			prot = attr & HAT_PROT_MASK;
	int			oldhat;
#ifdef PTE36
	uint_t			udircnt;
#endif


	ASSERT(prot < sizeof (hat_vprot2p));
	ppprot = hat_vprot2p[prot];
	HATPRINTF(0x20, ("updateattr: hat %x addr %x len %x attr %x what %x\n",
			hat, addr, len, attr, what));

	mutex_enter(&hat->hat_mutex);
#ifdef	lint
	span = MMU_PAGESIZE;
#endif
	oldhat = i86_switch_hat(hat);
	for (a = (uint_t)addr, ea = addr + len; a < (uint_t)ea; a += span) {
		pprot = ppprot;
		span = MMU_PAGESIZE;
		hwpp_index = addrtohwppindex(a);
		if (a >= (uint_t)kernelbase) {
			if (a >= (uint_t)eecontig && a < (uint_t)SEGKMAP_START)
				/*
				 * On machines with memory less than 64MB
				 * we do not allocate pagetables for the entire
				 * range of space between eecontig to
				 * SEGKMAP_START.
				 */
				continue;
			pagedir = kernel_only_pagedir;
#ifdef PTE36
			/*
			 * kernel_only_pagedir does not cover the entire
			 * 4GB address space for PTE36. Adjust hwpp_index.
			 */
			udircnt = user_pgdirpttblents -
					(kernel_pdelen ? 1 : 0);

			pde = &pagedir[hwpp_index - addrtohwppindex(
						udircnt * PAGEDIRPTR_MAPSZ)];
#else
			pde = &pagedir[hwpp_index];
#endif
			/*
			 * Following will not work if called after user programs
			 * start due to xlations which are there in processes
			 * with own pagedir. Needs work if we support
			 * largepage segkmem.
			 */
			if (LARGE_PAGE(pde)) {
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;
				continue;
			}
			if (what == HAT_LOAD_ATTR_PGDIR) {
				kpreempt_disable();
				pagedir = CPU->cpu_pagedir;
				pde = &pagedir[hwpp_index];
				((pte_t *)pde)->AccessPermissions = pprot;
				reload_cr3();
				HATSTAT_INC(HATSTAT_CR3_LOAD);
				kpreempt_enable();
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;
				continue;
			}
			/* End potentially broken code */
			pte = hat_ptefind(kernel_hat, (caddr_t)a);
		} else {
			hwpp = hat_hwppfind(hat, hwpp_index);
			/* skip this 4 MB if no hardware page allocated */
			if (!hwpp) {
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;
				continue;
			}
			if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
				/*
				 * These guys have a per process page directory
				 */
				pagedir = userpagedir;
				pte = &pagedir[hwpp->hwpp_index];
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;

			} else {
				pte = &hwpp->hwpp_lpte[PAGETABLE_INDEX(a)];
			}
		}
		if (!pte_valid(pte))
			continue;
		newpte = *pte;
		switch (what) {
		case HAT_SET_ATTR:
			if (pte_ro(pte) && (attr & PROT_WRITE)) {
				page_t		*pp;

				pp = page_numtopp_nolock(PTEVAL2PFN(*pte));
				i86_mlist_enter(pp);
				if (IS_ROC((uint32_t) mach_pp->p_mapping))
					atomic_andl((ulong_t *)
							&mach_pp->p_mapping,
							~ROC_MARK);
				i86_mlist_exit(pp);
			}
			pprot |= ((pte_t *)pte)->AccessPermissions;
			if (attr & HAT_NOSYNC) {
				setpte_nosync(&newpte);
			}
			break;
		case HAT_CLR_ATTR:
			pprot = ~pprot & ((pte_t *)pte)->AccessPermissions;
			if (attr & HAT_NOSYNC) {
				clrpte_nosync(&newpte);
			}
			break;
		}

		if ((((pte_t *)pte)->AccessPermissions != pprot) ||
			(attr & HAT_NOSYNC)) {
			((pte_t *)&newpte)->AccessPermissions = pprot;
			if ((hat != kernel_hat) &&
				(hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP))
				hat_update_pde(hat, hwpp->hwpp_index, newpte);
			else {
				page_t		*pp;

				pp = page_numtopp_nolock(PTEVAL2PFN(*pte));

				if (pp)
					i86_mlist_enter(pp);

				(void) hat_update_pte(hat, pte, newpte,
						(caddr_t)a, PTE_RM_MASK);
				if (pp)
					i86_mlist_exit(pp);
			}
		}
	}

	mutex_exit(&hat->hat_mutex);
	i86_restore_hat(oldhat);
}
/*
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If
 * vprot == ~PROT_WRITE, then all the write permission
 * is taken away for the current translations. If
 * vprot == ~PROT_USER, then all the user permissions
 * are taken away for the current translations, otherwise
 * vprot gives the new virtual protections to load up.
 *
 * addr and len must be MMU_PAGESIZE aligned.
 */
void
hat_chgprot(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr & HAT_PROT_MASK, HAT_LOAD_ATTR);
}
void
hat_chgattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_LOAD_ATTR);
}
void
hat_setattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_SET_ATTR);
}
void
hat_clrattr(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_CLR_ATTR);
}

void
hat_chgattr_pagedir(struct hat *hat, caddr_t addr, size_t len, uint_t attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_LOAD_ATTR_PGDIR);
}
uint_t
hat_getattr(struct hat *hat, caddr_t addr, uint_t *attr)
{
	pteval_t		*pte;
	uint_t			prot, hwpp_index;
	struct	hwptepage	*hwpp;
	int		oldhat;

	mutex_enter(&hat->hat_mutex);
	oldhat = i86_switch_hat(hat);
	pte = hat_ptefind(hat, addr);
	*attr = 0;
	if (pte == NULL || !pte_valid(pte)) {
		ASSERT((uint_t)addr < kernelbase);
		hwpp_index = addrtohwppindex(addr);

		hwpp = hat_hwppfind(hat, hwpp_index);
		if ((!hwpp) || (!hwpp->hwpp_mapping)) {
			mutex_exit(&hat->hat_mutex);
			i86_restore_hat(oldhat);
			return ((uint_t)0xffffffff);
		}
		pte = &userpagedir[hwpp_index];
	}
	prot = ((pte_t *)pte)->AccessPermissions;
	if (getpte_nosync(pte))
		*attr = HAT_NOSYNC;
	mutex_exit(&hat->hat_mutex);
	*attr |=  hat_pprot2v[prot];
	i86_restore_hat(oldhat);
	return (0);
}

ssize_t
hat_getpagesize(struct hat *hat, caddr_t addr)
{
	struct	hwptepage 	*hwpp;
	pteval_t	*pte;
	ssize_t		ret = -1;
	int		oldhat;

	mutex_enter(&hat->hat_mutex);
	if ((addr < (caddr_t)kernelbase) && hat->hat_pagedir) {
		oldhat = i86_switch_hat(hat);
		hwpp = hat_hwppfind(hat, addrtohwppindex(addr));
		if (!hwpp) {
			i86_restore_hat(oldhat);
			return (ret);
		}
		if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
			if (pte_valid(hwpp->hwpp_lpte))
				ret = LARGEPAGESIZE;
		} else {
			pte = &hwpp->hwpp_lpte[PAGETABLE_INDEX((uint_t)addr)];
			if (pte_valid(hwpp->hwpp_lpte))
				ret = MMU_PAGESIZE;
		}
		i86_restore_hat(oldhat);
		mutex_exit(&hat->hat_mutex);
		return (ret);
	}
	pte = hat_ptefind(hat, addr);
	ret  = ((pte == NULL || !pte_valid(pte)) ?
					MMU_STD_INVALIDPTE : MMU_PAGESIZE);
	mutex_exit(&hat->hat_mutex);
	return (ret);
}

void
hat_page_setattr(page_t *pp, uint_t flag)
{
	HATPRINTF(0x20, ("page_setattr: pp %x flag %x\n", pp, flag));
	PP_SETRM(pp, flag);
}
void
hat_page_clrattr(page_t *pp, uint_t flag)
{
	HATPRINTF(0x20, ("page_clrattr: pp %x flag %x\n", pp, flag));
	PP_CLRRM(pp, flag);
}
uint_t
hat_page_getattr(page_t *pp, uint_t flag)
{
	return (PP_GETRM(pp, flag));
}

/*
 * Unload all the mappings in the range [addr..addr+len).
 */
void
hat_unload(struct hat *hat, caddr_t addr, size_t len, uint_t flags)
{

	pteval_t		*pte;
	struct hment		*hme;
	register uint_t 	span = 0;
	int			hwpp_index, pt_index;
	uint_t   		curaddr, newa, pfn;
	struct	hwptepage	*hwpp;
	struct	page		*pp;
	int			llen;
	int			oldhat;

	ASSERT(hat == kernel_hat || (flags & HAT_UNLOAD_OTHER) || \
	    AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));

	if (flags & HAT_UNLOAD_UNMAP)
		flags = (flags & ~HAT_UNLOAD_UNMAP) | HAT_UNLOAD;

	if (hat != kernel_hat) {
		mutex_enter(&hat->hat_mutex);
		oldhat = i86_switch_hat(hat);
	}
	ASSERT((len & MMU_PAGEOFFSET) == 0);

	HATPRINTF(4, ("hat_unload: hat %x addr %x len %x\n", hat, addr, len));

begin:
	for (curaddr = (uint_t)addr, llen = len; llen;
					llen -= span, curaddr += span) {
		span = MMU_PAGESIZE;
		if (curaddr >= (uint_t)kernelbase) {
			pte = (pteval_t *) hat_ptefind(kas.a_hat,
				(caddr_t)curaddr);
try_again:
			pfn = PTEVAL2PFN(*pte);
			if (pte_valid(pte)) {
				pp = page_numtopp_nolock(pfn);
				if (pp) {
					i86_mlist_enter(pp);
					if (pfn != PTEVAL2PFN(*pte)) {
						/*
						 * probably being paranoid
						 * here.
						 */
						i86_mlist_exit(pp);
						goto try_again;
					}
					if (!pte_valid(pte)) {
						i86_mlist_exit(pp);
						continue;
					}
				}
				hat_pteunload(hat, pp, (caddr_t)curaddr,
					pte, pte, HOLDMLIST);
				if (pp) {
					if ((pte >= kheappte) &&
						(pte < ekheappte)) {
						hme = &khmes[pte - kheappte];
						hme_purge(pp, hme);
					} else
						i86_mlist_exit(pp);
				}
			}
			continue;
		}
		hwpp_index = addrtohwppindex(curaddr);

		hwpp = hat_hwppfind(hat, hwpp_index);

		/* skip this 4 MB if no hardware page allocated */
		if (!hwpp) {
			newa = ALIGN_TONEXT_LGPAGE(curaddr);
			if (llen <= newa - curaddr)
				break;
			span = newa - curaddr;
			continue;
		}
		if (hwpp->hwpp_mapping) {
			if (hat->hat_pagedir != NULL) {
				hat_update_pde(hat, hwpp->hwpp_index,
					MMU_STD_INVALIDPTE);
			}
			/*
			 * We are depending on the fact that 4KLOCKMAPs
			 * currently come only from seg_spt and that will
			 * not be a partial hwpp
			 */
			hat->hat_rss -= hwpp->hwpp_numptes;
			if (hwpp->hwpp_mapping ==
				(I86MMU4KLOCKMAP | I86MMU_OWN_PTBL)) {
				pte = hwpp->hwpp_lpte;
				uzero((caddr_t)pte, MMU_PAGESIZE);
			}
			hat_hwpp_cache_free(hat, hwpp, 1);
			newa = ALIGN_TONEXT_LGPAGE(curaddr);
			if (llen <= newa - curaddr)
				break;
			span = newa - curaddr;
			continue;
		}

		pt_index = PAGETABLE_INDEX(curaddr);
		pte = &hwpp->hwpp_lpte[pt_index];
		if (!pte_valid(pte))
			continue;

		pp = page_numtopp_nolock(PTEVAL2PFN(*pte));
		if (pp) {
			i86_mlist_enter(pp);
			if (!pte_valid(pte)) {
				i86_mlist_exit(pp);
				continue;
			}
		}
		hat_pteunload(hat, pp, (caddr_t)curaddr, pte,
			&hwpp->hwpp_pte[pt_index], NOHOLDMLIST);
	}

	if (hat != kernel_hat) {
		mutex_exit(&hat->hat_mutex);
		i86_restore_hat(oldhat);
	}

}

/*
 * Get the page frame number for a particular user virtual address.
 * Walk the hat list for the address space and call the getpfnum
 * op for each one; the first one to return a non-zero value is used
 * since they should all point to the same page.
 */
ulong_t
hat_getpfnum(struct hat *hat, caddr_t addr)
{
	ulong_t	pfn;

	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);
	pfn = hat_getpfnum_locked(hat, addr);
	if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
	return (pfn);
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
 * hat_probe doesn't wait for hat_mutex.
 */
int
hat_probe(struct hat *hat, caddr_t addr)
{
	ulong_t	pfn;

	if (hat != kernel_hat)
		if (mutex_tryenter(&hat->hat_mutex) == 0)
			return ((ulong_t)PFN_INVALID);
	pfn = hat_getpfnum_locked(hat, addr);
	if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
	return ((pfn == -1) ? 0 : 1);
}

/*
 * Copy top level mapping elements (L1 ptes or whatever)
 * that map from saddr to (saddr + len) in sas
 * to top level mapping elements from daddr in das.
 *
 * Hat_share()/unshare() return an (non-zero) error
 * when saddr and daddr are not properly aligned.
 *
 * The top level mapping element determines the alignment
 * requirement for saddr and daddr, depending on different
 * architectures.
 *
 * When hat_share()/unshare() are not supported,
 * HATOP_SHARE()/UNSHARE() return 0.
 */

/*ARGSUSED*/
int
hat_share(struct hat *hat, caddr_t addr, struct hat *hat_s,
caddr_t srcaddr, size_t size)
{
	struct		hwptepage  *hwpp, *hwpp_s;
	uint_t		hwpp_index;
	int		i, numpgtbl;
	uint_t		oldhat;

	if ((uint_t)addr & (LARGEPAGESIZE - 1))
		return (-1);
	mutex_enter(&hat->hat_mutex);
	mutex_enter(&hat_s->hat_mutex);

	numpgtbl = (size + LARGEPAGESIZE - 1) >> LARGE_PAGESHIFT;

	if (hat->hat_pagedir == NULL) {
		oldhat = hat_switch2ppp(hat);
	} else {
		oldhat = i86_switch_hat(hat);
	}
	for (i = 0; i < numpgtbl; i++, addr += LARGEPAGESIZE,
		srcaddr += LARGEPAGESIZE) {

		hwpp_index = addrtohwppindex(srcaddr);
		(void) i86_switch_hat(hat_s);
		hwpp_s = hat_hwppfind(hat_s, hwpp_index);
		(void) i86_switch_hat(hat);
		if (!hwpp_s)
			continue;

		hwpp_index = addrtohwppindex(addr);
		hwpp = hat_hwppfind(hat, hwpp_index);
		if (hwpp)
			continue;

		hwpp = hat_hwpp_cache_alloc(hat, hwpp_index, &oldhat, 0);
		hwpp->hwpp_mapping = hwpp_s->hwpp_mapping & ~I86MMU_OWN_PTBL;
		hwpp->hwpp_pte = hwpp_s->hwpp_pte;
		hwpp->hwpp_lpte = hwpp_s->hwpp_lpte;
		hwpp->hwpp_pfn = hwpp_s->hwpp_pfn;
		hwpp->hwpp_pde = hwpp_s->hwpp_pde;
		hwpp->hwpp_numptes = hwpp_s->hwpp_numptes;
		hat->hat_rss += hwpp_s->hwpp_numptes;
		LOAD_PTE(&userpagedir[hwpp->hwpp_index], hwpp->hwpp_pde);
	}
	mutex_exit(&hat_s->hat_mutex);

	i86_restore_hat(oldhat);
	mutex_exit(&hat->hat_mutex);
	return (0);
}

/*ARGSUSED*/
void
hat_unshare(struct hat *hat, caddr_t addr, size_t size)
{
	hat_unload(hat, addr, size, HAT_UNLOAD_UNMAP);
}


size_t
hat_get_mapped_size(struct hat *hat)
{
	return ((size_t)ptob(hat->hat_rss));
}

int
hat_stats_enable(struct hat *hat)
{
	atomic_add_32(&hat->hat_stat, 1);
	return (1);
}

void
hat_stats_disable(struct hat *hat)
{
	atomic_add_32(&hat->hat_stat, -1);
}

/* unload pdes on current CPU. cpup is passed in only as perf optim */
static void
hat_unload_pdes(struct cpu *cpup)
{
	struct hat	*oldhat = cpup->cpu_current_hat;
	pteval_t	*pagedir;
	uint_t		cr3;

	HATPRINTF(0x10, ("unload_pde: oldhat %x current hat %x numpdes %x\n",
			oldhat, cpup->cpu_current_hat, cpup->cpu_numpdes));
	ASSERT(CPU == cpup);

	cpup->cpu_current_hat = NULL;

	if (oldhat) {
		atomic_andl((unsigned long *)&oldhat->hat_cpusrunning,
			~(unsigned long)cpup->cpu_mask);
	}

	if (cpup->cpu_numpdes) {
		int	j;
		ASSERT(cpup->cpu_numpdes < hat_perprocess_pagedir_max);
		pagedir = cpup->cpu_pagedir;
		for (j = 0; j < cpup->cpu_numpdes; j++) {
			ASSERT(cpup->cpu_pde_index[j]*sizeof (pteval_t)
					< user_pdelen);
			INVALIDATE_PTE(&pagedir[cpup->cpu_pde_index[j]]);
		}
		cpup->cpu_numpdes = 0;
		cr3 = cpup->cpu_cr3;
		if (cr3 == ((cr3ctx_t *)cpup->cpu_curcr3)->ct_cr3)
			setcr3(cr3);	/* Flush TLB for next guy */
	}
}

/*
 * called on context switch. Load hat for the thread (cpu_pagedir entries
 * if needed, cpu_currrent_hat and hat_cpusrunning) and then set the cr3
 * to the one pointed by t_mmuctx which could be for pteashat or for another
 * process (when procfs and friends do stuff for other processes).
 * we have to be careful with use of LOAD_CR3_OPTIM
 * as assumptions are made in handling pteashat, that anyone with mmuctx
 * pointing to pteas_cr3 will have their TLBs flushed on thread switch.
 */
void
hat_setup4thread(kthread_t *t)
{
	struct	hat	*hat;
	uint_t		cr3;
	struct	cpu	*cpup = CPU;

	ASSERT(t);

	/* get real hat for the thread */
	hat = t->t_procp->p_as->a_hat;
	/* get temporary hat. (Could be real one for ppp threads) */
	cr3 = t->t_mmuctx;

	HATPRINTF(0x10, ("setup4thread: hat %x cr3ctx %x\n", hat, cr3));


	if (hat == kernel_hat) {
		if (!cr3)
			cr3 = (uint_t)&cpup->cpu_ctx;
		LOAD_CR3_OPTIM(cpup, cr3);
		return;
	}

	if (hat->hat_pagedir) {
		hat_setup_internal(hat);
		if (!cr3) {
			cr3 = (uint_t)&hat->hat_ctx;
			t->t_mmuctx = cr3;
		}
		LOAD_CR3_OPTIM(cpup, cr3);
		return;
	}

	if (!cr3)
		cr3 = (uint_t)&cpup->cpu_ctx;
	/*
	 * Avoid hassles of unloading & reloading if same hat
	 */
	if (cpup->cpu_current_hat == hat &&
		hat->hat_critical == HAT_INVALID_ADDR) {
		LOAD_CR3(cpup, cr3);
		return;
	}
	if ((mutex_tryenter(&hat->hat_mutex) == 0)) {
		/*
		 * cannot call setup_internal without hat lock (hwpp chain).
		 * unload_pde NULLs out cpu_current_hat so that setup_internal
		 * may be called in pteload.
		 */
		hat_unload_pdes(cpup);
		LOAD_CR3(cpup, cr3);
		return;
	}
	hat_setup_internal(hat);
	mutex_exit(&hat->hat_mutex);
	LOAD_CR3_OPTIM(cpup, cr3);

}

/*
 * Called from resume() (thru hat_setup4thread) and hat_ptealloc().
 * Called with kpreempt_disable() to setup mmu for a user thread.
 * This function does not get called when we swtch to a kernel thread.
 */
/*ARGSUSED1*/
static void
hat_setup_internal(struct hat *hat)
{
	struct cpu	*cpup;
	struct	hwptepage *hwpp;
	uint_t		index;
	pteval_t	*pagedir;
	caddr_t		addr;



	if (hat == kernel_hat)
		return;

	cpup = CPU;
	HATPRINTF(0x10, ("setup_internal: hat %x current hat %x numpdes %x\n",
			hat, cpup->cpu_current_hat, cpup->cpu_numpdes));

	HATSTAT_INC_CP(cpup, HATSTAT_HATSETUP);
	/*
	 * we are OK even if cpu_current_hat is freed as it will only
	 * be recycled as another hat and it is safe to remove that
	 * hat's cpusrunning bit as it could not have been run on
	 * this CPU.
	 */
	hat_unload_pdes(cpup);
	cpup->cpu_current_hat = hat;

	if (hat->hat_pagedir == NULL) {
		ASSERT(MUTEX_HELD(&hat->hat_mutex));
		ASSERT(cpup->cpu_numpdes == 0);
		LOAD_CR3(cpup, (&cpup->cpu_ctx));
		if (hat->hat_flags & I86MMU_FREEING)
			return;
		pagedir = cpup->cpu_pagedir;
		hwpp = hat->hat_hwpp;
		while (hwpp) {
			index = hwpp->hwpp_index;
			ASSERT(index * sizeof (pteval_t) < user_pdelen);
			if (hwpp->hwpp_pde) {
				pagedir[index] = hwpp->hwpp_pde;
				cpup->cpu_pde_index[cpup->cpu_numpdes] = index;
				cpup->cpu_numpdes++;
			}
			hwpp = hwpp->hwpp_next;
		}
		ASSERT(cpup->cpu_numpdes < hat_perprocess_pagedir_max);
		atomic_orl((unsigned long *)&hat->hat_cpusrunning,
			(unsigned long)cpup->cpu_mask);
	} else {
		/* cpu mask for the hat will be orred in by LOAD_CR3 */
		LOAD_CR3(cpup, (&hat->hat_ctx));
		if ((addr = hat->hat_critical) != HAT_INVALID_ADDR) {
			/* make sure we don't access addr without faulting */
			INVALIDATE_PTE(&userpagedir[addrtohwppindex(addr)]);
		}
		HATSTAT_INC_CP(cpup, HATSTAT_HATSETUP_PGDIR);
	}
}

/* Called from icode and fastbuildstack to setup hat initially */
/*ARGSUSED1*/
void
hat_setup(struct hat *hat, int allocflag)
{
	if (allocflag)
		curthread->t_mmuctx = 0;
	kpreempt_disable();
	mutex_enter(&hat->hat_mutex);
	hat_setup_internal(hat);
	mutex_exit(&hat->hat_mutex);
	kpreempt_enable();
}

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


/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_softlock(hat, addr, lenp, ppp, flags)
	struct	hat *hat;
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
/*ARGSUSED*/
faultcode_t
hat_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t addr_to, kaddr;
	size_t   *lenp;
	page_t  **pp_to, **pp_from;
{
	return (FC_NOSUPPORT);
}

ulong_t
hat_getkpfnum(caddr_t addr)
{
	pteptr_t	pte;
	int		oldhat;
	ulong_t		pfn;

	if (addr >= (caddr_t)kernelbase)
		return (hat_getpfnum(kas.a_hat, addr));
	pfn = PFN_INVALID;
	if ((addr >= segpt->s_base) &&
		(addr < (segpt->s_base + segpt->s_size))) {
		oldhat = i86_switch_hat(pteashat);
		pte = &pt_ptable[btop((uintptr_t)addr)];
		if (pte_valid(pte))
			pfn = PTEVAL2PFN(*pte);
		i86_restore_hat(oldhat);
	}
	return (pfn);
}

#define	ASCHUNK	64

/*
 * Kill process(es) that use the given page. (Used for parity recovery)
 * If we encounter the kernel's address space, give up (return -1).
 * Otherwise, we return 0.
 */
hat_kill_procs(page_t *pp, caddr_t addr)
{
#ifdef lint
	pp = pp;
	addr = addr;
#endif
	ASSERT(0);
	/* Need fixing. Never called now. */

	return (0);
}

/* ARGSUSED */
void
mmu_loadcr3(struct cpu *cpup, void *arg)
{
	if (((cr3ctx_t *)cpup->cpu_curcr3)->ct_hat == NULL) {
		LOAD_CR3(cpup, &cpup->cpu_ctx);
	} else {
		LOAD_CR3(cpup, cpup->cpu_curcr3);
	}
}

/* ARGSUSED */
void
hat_map_kernel_stack_local(caddr_t saddr, caddr_t eaddr)
{}

/*ARGSUSED*/
void
hat_reserve(struct as *as, caddr_t addr, size_t len)
{
}

static int
hat_x86stat_update(kstat_t *ksp, int rw)
{
	long			*hatinfo;
	kstat_named_t		*hatstat;
	int			i;
	struct cpu		*cpup;
	int			cpuid = 0;
	hatstat2src_t		*hatsrc;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	hatstat = (kstat_named_t *)ksp->ks_data;

	/* sum stats kept in cpu structure */

	cpup = cpu[cpuid];

	hatinfo = (long *)cpup->cpu_hat_infop;
	for (i = 0; i < HATSTAT_SZ; i++)
		hatstat[i].value.ul = hatinfo[i];

	while ((cpup = cpu[++cpuid]) != NULL) {
		hatinfo = (long *)cpup->cpu_hat_infop;
		for (i = 0; i < HATSTAT_SZ; i++)
			hatstat[i].value.ul += hatinfo[i];
	}

	hatsrc = (hatstat2src_t *)ksp->ks_private;

	for (i = 0; i < HATSTAT2_SZ - HATSTAT_SZ; i++)
		hatstat[HATSTAT_SZ + i].value.ul = *(hatsrc[i]);

	return (0);
}


static int
hat_kstat_create(struct cpu *cpup)
{
	static kstat_t	*ksp;
	int		size = 0;

	if (cpup) {
		size = HATSTAT_SZ * sizeof (ulong_t);
		cpup->cpu_hat_infop = kmem_zalloc(size, KM_SLEEP);
	}

	if (!ksp) {
		ksp = kstat_create("x86_hat", 0, NULL, "hat",
			KSTAT_TYPE_NAMED, HATSTAT2_SZ, KSTAT_FLAG_VIRTUAL);
		if (ksp) {
			ksp->ks_update = hat_x86stat_update;
			ksp->ks_data = x86hatstat;
			ksp->ks_private = (void *)x86hatstat2_src;
			kstat_install(ksp);
		}
	}
	return (size);
}


/*
 * Scan the pte page and for each valid pte, unload pte, grab pplist lock
 * and clear hmes. Free pte page
 */
static void
hat_hwppunload(struct hat *hat, struct hwptepage *hwpp)
{
#ifdef lint
	hat = hat;
	hwpp = hwpp;
#endif
	/* fill in if we want to free more resources in swapout */
}

typedef struct {
	pgcnt_t		physmem;
	int		pppthresh;
} ppptab_t;

ppptab_t	ppptab[] = {
	{0x8000,	8},	/* 0 - 128 MB. phys < virt. */
	{0x20000,	4},	/* 128 MB - 512 MB */
	{0,		1}	/* > 512 MB. phys > virt */
};

static int
getpppthresh()
{
	int		i = 0;
	pgcnt_t		pmem;

	while ((pmem = ppptab[i].physmem) != NULL) {
		if (physmem <= pmem)
			break;
		i++;
	}
	return (ppptab[i].pppthresh);
}

#ifdef PTE36


void
setup_kernel_page_directory(struct cpu *cpup)
{
	int 		copy = 1;
	uint_t		kpgdircnt;

	extern pteval_t	boot_pte0;

	if (kernel_only_pagedir == NULL) {
		user_pgdirpttblents = howmany(kernelbase, PAGEDIRPTR_MAPSZ);
		if (!user_pgdirpttblents)
			/* overflow case */
			user_pgdirpttblents = kernelbase/PAGEDIRPTR_MAPSZ + 1;
		/* compute kernel pdes overflowing into user's pttblents */
		kernel_pdelen = (((user_pgdirpttblents*PAGEDIRPTR_MAPSZ)
			- kernelbase)/ PTSIZE) * (sizeof (pteval_t));
		user_pdelen = (kernelbase/PTSIZE) * (sizeof (pteval_t));
		ASSERT(user_pdelen > (user_pgdirpttblents - 1) * MMU_PAGESIZE);
		/*
		 * Now compute number of pdes in cpu_pagedir. More for
		 * systems with less physical memory as we use more virtual
		 * and less physical with each extra numpde.
		 * Drop it down to 4 (lowest) when we have 16GB of memory
		 */
		if (!hat_perprocess_pagedir)
			hat_perprocess_pagedir = getpppthresh();

		kpgdircnt = NPDPERAS - user_pgdirpttblents +
			(kernel_pdelen ? 1 : 0);
		kernel_only_pagedir = kmem_zalloc(ptob(kpgdircnt), KM_SLEEP);
		hat_kmemcount += ptob(kpgdircnt);

		kernel_only_pttbl = hat_alloc_pttblent(kernel_only_pagedir);

		kernel_only_cr3 = (uint_t)(va_to_pfn(kernel_only_pttbl) <<
			MMU_STD_PAGESHIFT) +
			((uint_t)kernel_only_pttbl & PAGEOFFSET);
		/*
		 * called from startup() the first time for CPU 0.
		 * kernel_only_pagedir not initialized until later in
		 * startup on call to hat_kern_setup. Therefore no copy.
		 * And we need pages for all of virt address.
		 */
		copy = 0;
		cpup->cpu_pagedir = kmem_zalloc(ptob(NPDPERAS), KM_SLEEP);
		hat_kmemcount += ptob(NPDPERAS);
	} else {
		cpup->cpu_pagedir = kmem_zalloc(
			ptob(user_pgdirpttblents), KM_SLEEP);
		hat_kmemcount += ptob(user_pgdirpttblents);
	}

	cpup->cpu_pde_index = kmem_zalloc(hat_perprocess_pagedir_max
						* sizeof (uint_t), KM_SLEEP);
	hat_kmemcount += hat_perprocess_pagedir_max * sizeof (uint_t);

	cpup->cpu_numpdes = 0;

	ASSERT((((uint_t)cpup->cpu_pagedir) & PAGEOFFSET) == 0);

	cpup->cpu_pgdirpttbl = hat_alloc_pttblent(cpup->cpu_pagedir);

	/*
	 * Alloc a dummy hat for each cpu. 1 for all would work,
	 * but could cause lots of dirty cache line migrations
	 */
	cpup->cpu_ctx.ct_hat = hat_alloc(&kas);
	cpup->cpu_curcr3 = (uint_t)&cpup->cpu_ctx;

	if (!copy) {
		hat_kmemcount += hat_kstat_create(cpup);
		return;
	}

	/*
	 * We need to map the startup code in the first 4Mb chunk 1-1
	 */
	cpup->cpu_pagedir[0] = boot_pte0;

	cpup->cpu_cr3 = (uint_t)(hat_getkpfnum((caddr_t)cpup->cpu_pgdirpttbl) <<
			MMU_STD_PAGESHIFT) +
			((uint_t)cpup->cpu_pgdirpttbl & PAGEOFFSET);

	hat_kmemcount += hat_kstat_create(cpup);
	kas.a_hat->hat_cpusrunning |= 1 << cpup->cpu_id;
}

#else

void
setup_kernel_page_directory(struct cpu *cpup)
{
	int copy = 1;

	if (kernel_only_pagedir == NULL) {
		user_pgdirpttblents = 1;
		kernel_pdelen = ((0-kernelbase)/ PTSIZE) * (sizeof (pteval_t));
		user_pdelen = (kernelbase/PTSIZE) * (sizeof (pteval_t));
		/*
		 * Now compute number of pdes in cpu_pagedir. More for
		 * systems with less physical memory as we use more virtual
		 * and less physical with each extra numpde.
		 * Drop it down to 4 (lowest) when we have 1GB of memory
		 */
		if (!hat_perprocess_pagedir)
			hat_perprocess_pagedir = getpppthresh();

		kernel_only_pagedir = kmem_zalloc(ptob(1), KM_SLEEP);
		kernel_only_cr3 = (uint_t)(va_to_pfn(kernel_only_pagedir) <<
		    MMU_STD_PAGESHIFT);
		copy = 0;
	}
	cpup->cpu_pagedir = kmem_alloc(ptob(1), KM_SLEEP);

	cpup->cpu_pde_index = kmem_alloc(hat_perprocess_pagedir_max
						* sizeof (uint_t), KM_SLEEP);
	cpup->cpu_numpdes = 0;

	/*
	 * Alloc a dummy hat for each cpu. 1 for all would work,
	 * but could cause lots of dirty cache line migrations
	 */
	cpup->cpu_ctx.ct_hat = hat_alloc(&kas);
	cpup->cpu_curcr3 = (uint_t)&cpup->cpu_ctx;

	if (copy) {
	    kas.a_hat->hat_cpusrunning |= 1 << cpup->cpu_id;
	    bcopy((caddr_t)kernel_only_pagedir, (caddr_t)cpup->cpu_pagedir,
		ptob(1));
	    cpup->cpu_cr3 = ptob(hat_getkpfnum((caddr_t)cpup->cpu_pagedir));
	}
	hat_kmemcount += hat_kstat_create(cpup);
}

#endif	/* PTE36 */

void
setup_pteasmap()
{
	page_t		*pp;
	uint_t		addr = LARGEPAGESIZE;
	uint_t		pvtmapsize, pvtptables, i;
	pteval_t	*ptable;
	caddr_t		paddr, addrmax;

#ifdef PTE36
	pteptr_t	pttbl;
#endif

	if (ptearena)
		return;
	/*
	 * Assume about 6% density in page table usage.
	 * That makes for about 1 for every 16 pages of memory.
	 * Hence the >> 4 below.
	 * No need to allocate more for PAE as the entries tend to bunch.
	 * It is also limited by usermapend/PAGESIZE due to pteas
	 * limitations. Hence the MIN.
	 */
	pteas_size = MIN(((uint_t)usermapend - (2*LARGEPAGESIZE))/PAGESIZE,
			(physmem >> 4));
	pteas_size = roundup(pteas_size, NPTEPERPT);

	pvtmapsize = pteas_size * (sizeof (pteval_t) + sizeof (hwptepage_t *));
	pvtptables = howmany(pvtmapsize, LARGEPAGESIZE);
	/* Make sure we have enough space beneath usermapend for the above */
	pteas_size = MIN(
		((uint_t)usermapend - (2*LARGEPAGESIZE) - pvtptables)/PAGESIZE,
		pteas_size);

	/* Assume reasonable fragmentation */
	ptearena = vmem_create("pte arena", (void *)LARGEPAGESIZE,
		pteas_size * PAGESIZE, PAGESIZE, NULL, NULL, NULL, 0, VM_SLEEP);

	pteasvmemsz = vmem_size(ptearena, VMEM_ALLOC | VMEM_FREE);
	ASSERT(pteasvmemsz == pteas_size * PAGESIZE);
	pteaslowater = PTEASLO();

	/*
	 * Now bump pteas_size to provide virtual address for mapping in boot
	 * & space in pt_ptable for mapping invalid ptes.
	 */
	pteas_size += NPTEPERPT;

	KMEM_ALLOC_STAT(pt_pdir, user_pgdirpttblents * PAGESIZE, KM_SLEEP);
#ifdef	PTE36
	pttbl = hat_alloc_pttblent(pt_pdir);
	/*
	 * get pp to back the virtual address range in userptemap & ptearena
	 */
	pteas_cr3 = (hat_getkpfnum((caddr_t)pttbl) << MMU_STD_PAGESHIFT) +
			((uint_t)pttbl & PAGEOFFSET);
	/* Now copy boot's pagedir entries */
	pt_pdir[0] = CPU->cpu_pagedir[0];
#else
	bcopy((caddr_t)kernel_only_pagedir, pt_pdir, MMU_PAGESIZE);
	pteas_cr3 = (hat_getkpfnum((caddr_t)pt_pdir) << MMU_STD_PAGESHIFT);
#endif
	pteashat = hat_alloc(&kas);
	KMEM_ZALLOC_STAT(ptable, pvtptables * PAGESIZE, KM_SLEEP);
	KMEM_ZALLOC_STAT(pte2hati, pteas_size * sizeof (ushort_t), KM_SLEEP);
	paddr = (caddr_t)roundup(pteas_size * PAGESIZE, LARGEPAGESIZE) +
		LARGEPAGESIZE;
	addrmax = paddr;	/* just save it away till after the loop */
	for (i = 0; i < pvtptables; i++) {
		pt_pdir[MMU_L1_INDEX(paddr)] =
			PTEOF_C(hat_getkpfnum((caddr_t)
			(caddr_t)ptable + (i * PAGESIZE)), MMU_STD_SRWX);
		paddr += LARGEPAGESIZE;
	}
	paddr = addrmax;	/* restore start of private address */
	/* Grab mutex just to keep hat_page_create happy */
	mutex_enter(&pteashat->hat_mutex);

	pt_ptable = (pteptr_t)paddr;
	pp = hat_page_create(pteashat, (pteptr_t) paddr);
	/* Use first page of pt_ptable to point freed hwpp_ptes */
	pt_invalid_pteptr = PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRX);
	paddr += PAGESIZE;
	ptable[0] = PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRX);

	addrmax = paddr + (pteas_size * sizeof (pteval_t));
	for (i = 1; paddr < addrmax; paddr += PAGESIZE, i++) {
		pp = hat_page_create(pteashat, (pteptr_t) paddr);
		ptable[i] = PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
		pt_pdir[MMU_L1_INDEX(addr)] =
			PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
		addr += LARGEPAGESIZE;
	}
	pt_hwpp = (hwptepage_t **)paddr;
	addrmax = paddr + (pteas_size * sizeof (hwptepage_t *));
	for (; paddr < addrmax; paddr += PAGESIZE, i++) {
		pp = hat_page_create(pteashat, (pteptr_t) paddr);
		ptable[i] = PTEOF_C(mach_pp->p_pagenum, MMU_STD_SRWX);
	}
	mutex_exit(&pteashat->hat_mutex);

	/*
	 * Now allocate a segment with all the pagetables and required
	 * support pages mapped in pteasmap. This segment is used only to
	 * ensure that these pages are dumped on PANIC
	 */
	rw_enter(&kas.a_lock, RW_WRITER);
	segpt = seg_alloc(&kas, (caddr_t)LARGEPAGESIZE,
		(size_t)(addrmax - LARGEPAGESIZE));
	if (segpt == NULL)
		cmn_err(CE_PANIC, "setup_pteasmap: cannot allocate segpt");
	segpt->s_ops = &segpt_ops;
	rw_exit(&kas.a_lock);

	pteashat->hat_pagedir = 1;
	pteashat->hat_pdepfn = pteas_cr3;
	pteashat->hat_ctx.ct_hat = pteashat;
}

static void
segpt_badop(void)
{
	cmn_err(CE_PANIC, "segpt_badop");
	/* NOTREACHED */
}

/*
 * Dump out all pages in pteasmap
 */
static void
segpt_dump(seg)
	struct seg *seg;
{
	caddr_t		paddr, addrmax;
	int i;
	pteptr_t	pte;
	int		oldhat;
	struct hatppp 	*ppp, *pppn;
	uint_t		pt_ptableindex, endindex;

	oldhat = i86_switch_hat(pteashat);
	/*
	 * Go through free list and set noconsist on all free pages
	 */
	pppn = hat_ppplist;
	while (pppn) {
		ppp = pppn;
		pppn = ppp->hp_next;
		pt_ptableindex = btop((uintptr_t)ppp->hp_pteasaddr);
		endindex = pt_ptableindex + hat_ppp_pteas_pages;
		for (; pt_ptableindex < endindex; pt_ptableindex++) {
			setpte_noconsist(&pt_ptable[pt_ptableindex]);
		}
	}
	paddr = seg->s_base;
	addrmax = paddr + seg->s_size;
	i = btop((uintptr_t)paddr);

	for (; paddr < addrmax; paddr += PAGESIZE, i++) {
		pte = &pt_ptable[i];
		if (pte_valid(pte) && !getpte_noconsist(pte) &&
			*pte != pt_invalid_pteptr) {
			dump_addpage(&kas, paddr, PTEVAL2PFN(*pte));
		}
	}
	i86_restore_hat(oldhat);
}

/*
 * See if fault is a result of stolen address (address may be LOCKMAP)
 * or hat_critical invalidation on hat_setup_internal.
 *
 * returns true if address resolved to bypass as_fault
 */
int
hat_addrchk(kthread_t *t, struct hat *hat, caddr_t addr, enum fault_type type)
{
	pteval_t	*pte;
	hwptepage_t	*hwpp;
	int		pdi, pti;	/* pagedir and pagetable indices */
	int		resolved = 0;
	uint_t		mmuctx;

	if (addr >= usermapend)
		return (0);

	if (hat->hat_pagedir) {
		mmuctx = t->t_mmuctx;
		if (!mmuctx) {
			mmuctx = (uint_t)&hat->hat_ctx;
			t->t_mmuctx = mmuctx;
			kpreempt_disable();
			LOAD_CR3(CPU, mmuctx);
			kpreempt_enable();
			resolved = 1;
		}
		ASSERT(cr3() == ((cr3ctx_t *)mmuctx)->ct_cr3);
		pdi = addrtohwppindex(addr);
		mutex_enter(&hat->hat_mutex);
		hwpp = userhwppmap[pdi];
		if (hwpp) {
			LOAD_PTE(&userpagedir[pdi], hwpp->hwpp_pde);
			ASSERT(pte_valid(&hwpp->hwpp_pde));

			if (!LARGE_PAGE(&hwpp->hwpp_pde)) {
				pti = PAGETABLE_INDEX((uint_t)addr);
				pte = &hwpp->hwpp_lpte[pti];
				if (pte_valid(pte)) {
					if (type == F_INVAL) {
						resolved = 1;
					} else {
						if (!pte_ro(pte))
							resolved = 1;
					}
				}
			} else {
				resolved = 1;
			}
		}
		mutex_exit(&hat->hat_mutex);
	}
	return (resolved);
}

#ifdef DEBUG

/* routine to find multiple mappings to same page For calling from kadb */
int
hat_reportmaps(int pfn, int print)
{
	int		oldhat;
	int		count = 0;
	pteval_t	*pte;
	int		i, j;
	struct hwptepage	*hwpp;

	/* Look in kernel space first */
	pte = (pteval_t *)KERNELmap;
	for (i = 0; i < (mmu_btop(0 - SEGKMAP_START)); i++, pte++)
		if ((PTEVAL2PFN(*pte) == pfn) && pte_valid(pte)) {
			if (print)
				prom_printf("addr %x in KMAP maps pfn\n",
					ptob(i) + (int)SEGKMAP_START);
			count++;
		}

	pte = (pteval_t *)Sysmap;
	for (i = 0; i < (mmu_btop((uintptr_t)eecontig - kernelbase));
		i++, pte++)
		if ((PTEVAL2PFN(*pte) == pfn) && pte_valid(pte)) {
			if (print) {
				prom_printf("addr %x in Sys maps pfn\n",
					ptob(i) + (int)kernelbase);
			}
			count++;
		}

	/* Now for user address space */
	oldhat = i86_switch_hat(pteashat);
	pte = (pteval_t *)LARGEPAGESIZE;
	for (i = (int)btop(LARGEPAGESIZE); i < pteas_size; i++) {
		hwpp = pt_hwpp[i];
		if ((!hwpp) || !hwpp->hwpp_hat) {
			pte += NPTEPERPT;
			continue;
		}
		for (j = 0; j < NPTEPERPT; j++) {
			if ((PTEVAL2PFN(*pte) != pfn) || !pte_valid(pte)) {
				pte++;
				continue;
			}
			if (print)
			    prom_printf("hat %x's addr %x maps pfn at pte %x\n",
				(int)hwpp->hwpp_hat,
				(int)hwppindextoaddr(hwpp->hwpp_index) +
					ptob(pte - hwpp->hwpp_pte), pte);
			count++;
			pte++;
		}
	}
	i86_restore_hat(oldhat);
	return (count);
}

#endif	/* DEBUG */

#ifdef HATDEBUG
static void
hattlbgendebug(int who, uint_t gen)
{
	switch (who) {
	case 0:
		HATPRINTF(4, ("TLBFLUSH_WAIT: tlb_gen %x gen %x caller %x\n",
			tlb_flush_gen, gen, caller()));
		break;
	case 1:
		HATPRINTF(4, ("TLBFLUSH_BRDCST: tlb_gen %x gen %x caller %x\n",
			tlb_flush_gen, gen, caller()));
		break;
	}
}



#define	HATBUFSZ	32768
#define	HATPAD		128		/* larger than max len hat string */

char		hatbuf[HATBUFSZ + HATPAD];
uint_t		hatbufi;
kmutex_t	hatbuflock;
int		hatarea = 0x2c;
int		hatbuflockinit;
int		hat_print_console = 0;

void
hatprintf(const char *fmt, ...)
{
	va_list		args;
	int		len;
	char		localbuf[HATPAD];
	uint_t		newval, oldhatbufi;

	va_start(args, fmt);

	/*
	 * length calculated to not include '\0' to allow entire hatbuf to
	 * displayed as a single string in kadb. Need to write '\0' to
	 * hatbufi...
	 */
	len = snprintf(localbuf, INT_MAX, "%d: ", (int)CPU->cpu_id);
	len += vsnprintf(localbuf + len, INT_MAX, fmt, args);

	do {
		oldhatbufi = hatbufi;
		newval = oldhatbufi + len;
		if (newval > HATBUFSZ)
			newval = 0;
	} while (cas32(&hatbufi, oldhatbufi, newval) != oldhatbufi);

	bcopy(localbuf, hatbuf + oldhatbufi, len);

	/* use print_console only as desperate measure & set hatarea to ~13 */
	if (hat_print_console) {
		if (mutex_tryenter(&hatbuflock)) {
			prom_printf("^%s", localbuf);
			mutex_exit(&hatbuflock);
		}
	}
}

#endif	/* HATDEBUG */
