/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_VM_HAT_I86MMU_H
#define	_VM_HAT_I86MMU_H

#pragma ident	"@(#)hat_i86.h	1.50	99/12/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the Intel 80x86 MMU
 * specific hat data structures and its specific hat procedures.
 * The machine independent interface is described in <common/vm/hat.h>.
 */
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/pte.h>
#include <sys/debug/debug.h>
#include <sys/x_call.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <sys/vmparam.h>
#include <sys/vm_machparam.h>

extern	uint_t		kernel_only_cr3;

#define	HAT_SUPPORTED_LOAD_FLAGS (HAT_LOAD | HAT_LOAD_LOCK | HAT_LOAD_ADV |\
		HAT_LOAD_CONTIG | HAT_LOAD_NOCONSIST |\
		HAT_LOAD_SHARE | HAT_LOAD_REMAP)

struct hat {
	kmutex_t	hat_mutex;	/* protects hat, hatops */
	struct	as	*hat_as;
	struct	hwptepage *hat_hwpp;
	ulong_t		hat_cpusrunning; /* CPUs having xlations 4 this hat */
	uint_t		hat_rss;	/* resident set size */
	uint_t		hat_numhwpp;
	uint_t		hat_stat;
	caddr_t		hat_critical;	/* Prevent loading on other CPUs */
	struct hatppp 	*hat_ppp;	/* pointer to ppp related stuff */
	struct	hat	*hat_next;
	struct	hat	*hat_prev;
	ushort_t	hat_index;	/* index into hatidx[] */
	char		hat_pagedir;	/* true == process with pvt page dir */
	uchar_t		hat_flags;
	struct	cr3ctx	hat_ctx;	/* for processes with pvt page dir */
};

#define	HAT_RSRVHAT	(2)			/* hats not assoc with a proc */
#define	HAT_MAXHAT	(65536 - HAT_RSRVHAT)	/* max given sizeof hat_index */

#define	hat_pdepfn	hat_ctx.ct_cr3
/*
 * hat->hat_flags
 */
#define	I86MMU_FREEING			0x01
#define	I86MMU_SPTAS			0x080

/*
 * The hment entry, hat mapping entry.
 * The mmu independent translation on the mapping list for a page
 */
struct hment {
	struct	hment		*hme_next;	/* next hment */
	pteval_t		*hme_pte;	/* pte */
};
/*
 * The ppp struct. Points to prvt pool of pages, mapped appropriately
 */
struct hatppp {
	struct	hatppp		*hp_next;	/* next hatppp */
	uint64_t 		*hp_pgdirpttblent; /* L1 pagedir entries */
	page_t			*hp_prvtpp;	/* list of pages alloced */
	uint_t			hp_pdepfn;	/* pfn of page dir */
	void			*hp_pteasaddr;	/* address in pteasmap */
};

#define	HMEALIGN	8

#ifdef	_KERNEL

#define	P_NC	0x08

#define	PP_ISMOD(pp)		(((machpage_t *)(pp))->p_nrm & P_MOD)
#define	PP_ISREF(pp)		(((machpage_t *)(pp))->p_nrm & P_REF)
#define	PP_ISRO(pp)		(((machpage_t *)(pp))->p_nrm & P_RO)
#define	PP_ISNC(pp)		(((machpage_t *)(pp))->p_nrm & P_NC)

#define	PP_SETRM(pp, rm)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), rm)
#define	PP_SETMOD(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_MOD)
#define	PP_SETREF(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_REF)
#define	PP_SETRO(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_RO)
#define	PP_SETREFRO(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), R_REF|P_RO)
#define	PP_SETPNC(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_NC)

#define	PP_CLRRM(pp, rm)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~rm)
#define	PP_CLRMOD(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_MOD)
#define	PP_CLRREF(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_REF)
#define	PP_CLRRO(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_RO)
#define	PP_CLRPNC(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_NC)
#define	PP_CLRALL(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), \
				~(P_MOD|P_REF|P_RO|P_NC))

#define	PP_GETRM(pp, rmmask)	(((machpage_t *)(pp))->p_nrm & rmmask)
#endif /* _KERNEL */


#define	I86MMU_LGPAGE_LOCKMAP	1
#define	I86MMU4KLOCKMAP		2
#define	I86MMU_OWN_PTBL		4


/*
 * The hwptepage structure contains one entry for each potential page of
 * real (hardware) page table entries.
 *
 */
typedef struct hwptepage {
	/*
	 * When free, either on list of structs
	 * with attached page or without.
	 */
	struct hwptepage *hwpp_next;
	struct hwptepage *hwpp_prev;
	pteval_t	*hwpp_pte;	/* ptes as mapped from pteasmap */
	pteval_t	*hwpp_lpte;	/* pte address in local as */
	page_t		*hwpp_pte_pp;	/* page pointer */
	uint_t		hwpp_pfn;	/* Page frame number */
	struct		hat *hwpp_hat;
	uint_t		hwpp_index;	/* index */
	uint_t		hwpp_lockcnt;	/* # of locks on ptes in this hwpp */
	uint_t		hwpp_numptes;	/* number of valid ptes in this hwpp */
	uint_t		hwpp_mapping; 	/* This hwpp maps a 4Mb page */
	pteval_t	hwpp_pde;	/* page directory entry */
} hwptepage_t;

enum hatstat {				/* per cpu stats */
	HATSTAT_CR3_LOAD,
	HATSTAT_PGTBL_RECLAIMED,
	HATSTAT_PGTBLALLOC,		/* page table alloc */
	HATSTAT_PGTBLALLOC_PGDIR,	/* alloc with per process pagedir */
	HATSTAT_STEAL,			/* total of the following 4 */
	HATSTAT_STEAL_VMEM,
	HATSTAT_STEAL_KMEM,
	HATSTAT_STEAL_MEM,
	HATSTAT_STEAL_PTEAS,
	HATSTAT_STEAL_RESTART,		/* loop a 2nd time */
	HATSTAT_STEAL_ABORT,		/* abort after looping twice */
	HATSTAT_HATSETUP,
	HATSTAT_HATSETUP_PGDIR,		/* setup with per process pagedir */
	HATSTAT_TLBFLUSHWAIT,
	HATSTAT_TLBFLUSHWAIT_IMM,
	HATSTAT_CAPTURE_CPUS_ALL,
	HATSTAT_CAPTURE_CPUS,
	HATSTAT_4K_LOCKMAP,
	HATSTAT_LGPAGE_LOCKMAP,
	HATSTAT_PHYSMEMALLOC,
	HATSTAT_VIRTMEMALLOC_HEAP,
	HATSTAT_VIRTMEMALLOC_PTEAS,
	HATSTAT_PPP,

	HATSTAT_SZ			/* must be last entry */
};

#define	HATSTAT_INC_CP(cp, x)	cp->cpu_hat_infop[(x)]++
#define	HATSTAT_INC(x)		CPU->cpu_hat_infop[(x)]++
#define	HATSTAT_DEC(x)		CPU->cpu_hat_infop[(x)]--
#define	HATSTAT_ADD(x, cnt)	CPU->cpu_hat_infop[(x)] += (cnt)
#define	HATSTAT_SUB(x, cnt)	CPU->cpu_hat_infop[(x)] -= (cnt)

#define	KMEM_ALLOC_STAT(addr, sz, flag) {				\
	addr = kmem_alloc(sz, flag);					\
	if (addr) {							\
		HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_HEAP, sz);		\
		HATSTAT_ADD(HATSTAT_PHYSMEMALLOC, sz);			\
	}								\
}

#define	KMEM_ZALLOC_STAT(addr, sz, flag) {				\
	addr = kmem_zalloc(sz, flag);					\
	if (addr) {							\
		HATSTAT_ADD(HATSTAT_VIRTMEMALLOC_HEAP, sz);		\
		HATSTAT_ADD(HATSTAT_PHYSMEMALLOC, sz);			\
	}								\
}

#define	KMEM_FREE_STAT(addr, sz) {					\
	kmem_free(addr, sz);						\
	HATSTAT_SUB(HATSTAT_VIRTMEMALLOC_HEAP, sz);			\
	HATSTAT_SUB(HATSTAT_PHYSMEMALLOC, sz);				\
}

typedef	ulong_t		*hatstat2src_t;

/* internal stats */
enum hatstat2 {
	HATSTAT2_PPPLIST_CNT = HATSTAT_SZ,	/* continue from hatstat */
	HATSTAT2_PAGELIST_CNT,
	HATSTAT2_HAT_CNT,
	HATSTAT2_KMEM_CNT,
	HATSTAT2_MLIST_SLEEP_CNT,
	HATSTAT2_MLIST_RESLEEP_CNT,
	HATSTAT2_HWPP_CNT,
	HATSTAT2_HWPPLPTE_CNT,

	HATSTAT2_SZ			/* must be last entry */
};

#define	HATSTAT2_INC(x)		x86hatstat2_src[(x)]++
#define	HATSTAT2_DEC(x)		x86hatstat2_src[(x)]--
#define	HATSTAT2_ADD(x, cnt)	x86hatstat2_src[(x)] += (cnt)
#define	HATSTAT2_SUB(x, cnt)	x86hatstat2_src[(x)] -= (cnt)

/*
 * Various hat statistics for x86
 */
extern kstat_named_t	x86hatstat[];
extern kstat_named_t	x86hatstat2[];
extern hatstat2src_t	x86hatstat2_src[];

/*
 * Convert an address to its base hwpp portion
 */
#define	addrtohwppbase(addr) ((uint_t)(addr) >> LARGE_PAGESHIFT)

/*
 * Convert an hwpp base to a virtual address
 */
#define	hwppbasetoaddr(base) ((caddr_t)((base) << LARGE_PAGESHIFT))

/*
 * Flags to pass to i86mmu_pteunload().
 */
#define	HAT_RMSYNC	0x01
#define	HAT_NCSYNC	0x02
#define	HAT_INVSYNC	0x04
#define	HAT_VADDR	0x08
#define	HAT_RMSTAT	0x10

/*
 * Flags to pass to hat_pagecachectl().
 */
#define	HAT_CACHE	0x1
#define	HAT_UNCACHE	0x2

#define	PAGETABLE_INDEX(a)	MMU_L2_INDEX(a)
#define	PAGEDIR_INDEX(a)	MMU_L1_INDEX(a)

extern	void	xc_broadcast_tlbflush(cpuset_t, caddr_t, uint_t *);
extern	void	xc_waitfor_tlbflush(uint_t);

/*
 * Macros for implementing MP critical sections in the hat layer.
 */
extern int flushes_require_xcalls;

/*
 * OPTIMAL_CACHE can be defined to cause flushing to occur only on those
 * CPUs that have accessed the address space.  During the debugging process,
 * this code can be selectively enabled or disabled.
 */
#define	OPTIMAL_CACHE

#ifndef	OPTIMAL_CACHE
/*
 * The CAPTURE_CPUS and RELEASE_CPUS macros can be used to implement
 * critical code sections where a set of CPUs are held while ptes are
 * updated and the TLB and VAC caches are flushed.  This prevents races
 * where a pte is being updated while another CPU is accessing this
 * pte or past instances of this pte in its TLB.  The current feeling is
 * that it may be possible to avoid these races without this explicit
 * capture-release protocol.  However, keep these macros around just in
 * case they are needed.
 * flushes_require_xcalls is set during the start-up sequence once MP
 * start-up is about to begin, and once the x-call mechanisms have been
 * initialized.
 * Note that some I86MMU-based machines may not need critical section
 * support, and they will never set flushes_require_xcalls.
 */
#define	CAPTURE_CPUS    if (flushes_require_xcalls) \
				xc_capture_cpus(-1);

#define	CAPTURE_SELECTED_CPUS(mask)    if (flushes_require_xcalls) \
					xc_capture_cpus(-1);

#define	RELEASE_CPUS    if (flushes_require_xcalls) \
				xc_release_cpus();

/*
 * The XCALL_PROLOG and XCALL_EPILOG macros are used before and after
 * code which performs x-calls.  XCALL_PROLOG will obtain exclusive access
 * to the x-call mailbox (for the particular x-call level) and will specify
 * the set of CPUs involved in the x-call.  XCALL_EPILOG will release
 * exclusive access to the x-call mailbox.
 * Note that some I86MMU machines may not need to perform x-calls for
 * cache flush operations, so they will not set flushes_require_xcalls.
 */
#define	XCALL_PROLOG    if (flushes_require_xcalls) \
				xc_prolog(-1);

#define	XCALL_EPILOG    if (flushes_require_xcalls) \
				xc_epilog();
#else	OPTIMAL_CACHE
#define	CAPTURE_CPUS(arg)						\
	if (flushes_require_xcalls) {					\
		if ((arg)) {						\
			HATSTAT_INC(HATSTAT_CAPTURE_CPUS);		\
			xc_capture_cpus((arg)->hat_cpusrunning);	\
		} else {						\
			HATSTAT_INC(HATSTAT_CAPTURE_CPUS_ALL);		\
			xc_capture_cpus(-1);				\
		}							\
	}

#define	CAPTURE_SELECTED_CPUS(mask)					\
	if (flushes_require_xcalls) {					\
		HATSTAT_INC(HATSTAT_CAPTURE_CPUS);			\
		xc_capture_cpus(mask);					\
	}

#define	RELEASE_CPUS	if (flushes_require_xcalls) { \
				xc_release_cpus(); \
			}

#define	TLBFLUSH_BRDCST(arg, addr, gen)					\
	if (flushes_require_xcalls) {					\
		xc_broadcast_tlbflush((arg)->hat_cpusrunning,		\
			addr, &(gen));					\
		HATTLBGENDEBUG(1, gen);					\
	}

extern uint32_t	tlb_flush_gen;

#define	TLBFLUSH_WAIT(gen)				\
	if (tlb_flush_gen != gen) {			\
		HATSTAT_INC(HATSTAT_TLBFLUSHWAIT_IMM);	\
	} else {					\
		xc_waitfor_tlbflush(gen);		\
		HATSTAT_INC(HATSTAT_TLBFLUSHWAIT);	\
	}						\
	HATTLBGENDEBUG(0, gen);

#endif	OPTIMAL_CACHE

#if defined(_KERNEL)

/*
 * functions to atomically manipulate hat_cpusrunning mask
 * (defined in i86/ml/i86.il)
 */
extern void atomic_orl(unsigned long *addr, unsigned long val);
extern void atomic_andl(unsigned long *addr, unsigned long val);
extern void atomic_orb(unsigned char *addr, unsigned char val);
extern void atomic_andb(unsigned char *addr, unsigned char val);
extern int intr_clear(void);
extern void intr_restore(uint_t);

#ifndef HATDEBUG
#define	HATPRINTF(level, args)
#define	HATTLBGENDEBUG(id, gen)
#else
extern int	hatarea;
extern void	hatprintf(const char *, ...);
#define	HATPRINTF(area, args)    if (area & hatarea) hatprintf args
#define	HATTLBGENDEBUG(id, gen)		hattlbgendebug(id, gen)
#endif

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_I86MMU_H */
