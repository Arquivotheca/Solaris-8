/*
 * Copyright (c) 1987,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the sun referernce mmu (srmmu)
 * specific hat data structures and the srmmu specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 */

#ifndef	_VM_HAT_SRMMU_H
#define	_VM_HAT_SRMMU_H

#pragma ident	"@(#)hat_srmmu.h	1.76	98/06/18 SMI"

#include <sys/t_lock.h>
#include <vm/hat.h>
#include <vm/mhat.h>
#include <sys/types.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <vm/seg.h>
#include <vm/mach_srmmu.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Software context structure. There is a one to one map from these context
 * structures to entries into the context (root) table of the Sparc Reference
 * MMU.
 * To conserve memory, efforts have been made to eliminate a few
 * of the things that were once stored in this structure.
 */

struct ctx {
	struct as *c_as;	/* back pointer to the address space */
};

#define	VALID_CTX(ctx)	((ctx) >= KCONTEXT && (ctx) < nctxs)
#define	VALID_UCTX(ctx)	((ctx) > KCONTEXT && (ctx) < nctxs)

/*
 * flags for ptbl_flags field.
 *	PTBL_LOCKED- ptbl is locked. this protects all fields in ptbl and the
 *	PTBL_KEEP- this ptbl is not to be stolen.
 *	PTBL_SHARED- this ptbl is shared by more than one address space (ISM).
 *	PTBL_ALLOCED- this ptbl has been allocated, ie. not free.
 *	PTBL_TMP- XXX don't know yet ....
 *
 *	PTBL_LEVEL_[123]- the level this ptbl is used for.
 *
 * In general, to update this field one has to have the PTBL_LOCKED set.
 * The only exception is when the page table is just being kma_alloc'ed
 * and they are not on any list yet.
 */

#define	PTBL_LOCKED	0x80
#define	PTBL_KEEP	0x40
#define	PTBL_SHARED	0x20
#define	PTBL_ALLOCED	0x10
#define	PTBL_TMP	0x08
#define	PTBL_UNUSED	0x04

#define	PTBL_LEVEL_1	0x01
#define	PTBL_LEVEL_2	0x02
#define	PTBL_LEVEL_3	0x03

#define	PTBL_LEVEL_MSK	0x03

#define	PTBL_LEVEL(flags)	((flags) &  PTBL_LEVEL_MSK)

#define	PTBL_IS_LOCKED(flags)	(((flags) & (PTBL_LOCKED | PTBL_ALLOCED))\
	== (PTBL_LOCKED | PTBL_ALLOCED))

#define	PTBL_IS_FAKE(flags)	((flags) & PTBL_TMP)

#define	FREE_UP_PTBL(flags)	(((flags) & PTBL_KEEP) == 0)
#define	PTBL_VALIDCNT(vcnt)	((vcnt) <= NL3PTEPERPT)

struct ptbl {
	union {
		struct ptbl	*u_parent;
		struct ptbl	*u_next;
	} ptbl_u;
	struct as	*ptbl_as;
	ushort_t	ptbl_base;
	ushort_t	ptbl_lockcnt;
	uchar_t		ptbl_unused;
	uchar_t		ptbl_index;
	uchar_t		ptbl_validcnt;
	uchar_t		ptbl_flags;
};
typedef struct ptbl	ptbl_t;

#define	ptbl_parent	ptbl_u.u_parent
#define	ptbl_next	ptbl_u.u_next

struct l3pt {
	struct pte pte[MMU_NPTE_THREE];
};
typedef struct l3pt	pt_t;

struct l2pt {
	union ptpe ptpe[MMU_NPTE_TWO];
};

struct l1pt {
	union ptpe ptpe[MMU_NPTE_ONE];
};

/*
 * The srmmu structure is the mmu dependent hardware address translation
 * structure linked to the address space structure to show the translation.
 * provided by the srmmu for an address space.
 */
struct srmmu {
	union {
		struct hat   *_su_hat;	/* back ptr to the hat */
		struct srmmu *_su_next;	/* next srmmu on free/lru list */
	} srmmu_u;
	ptbl_t		*s_l1ptbl;	/* first ptbl of the 4 for the l1 pt */
	uint_t		srmmu_root;	/* Left around for the leo pigs */
	short		s_ctx;		/* context for this hat */
	uchar_t		s_rmstat;	/* do rm stat accounting? */
	uchar_t		s_unused;
	ptbl_t		*s_l3ptbl;	/* last used L3 ptbl */
	caddr_t		s_addr;		/* base address for s_l3ptbl */
	uint_t		s_rss;		/* # of mappings for this as */
#if defined(SRMMU_TMPUNLOAD)
	ptbl_t		*s_tmpl1ptbl;	/* first ptbl of the 4 for the l1 pt */
#endif /* SRMMU_TMPUNLOAD */
};
typedef	struct srmmu	srmmu_t;

#define	su_hat	srmmu_u._su_hat
#define	su_next	srmmu_u._su_next

#define	HMENT_LINK_OFF	(1020)

/*
 * a wrapper around the generic hment.
 */
struct srhment {
	struct hment	ghme;
	struct ptbl	*hme_ptbl;
	struct srhment	*hme_hash;
};
typedef struct srhment	srhment_t;

#define	hme_index	ghme.hme_impl

struct hment_pool {
	kmutex_t	hp_mtx;
	srhment_t	*hp_list;
	uint_t		hp_count;
	uint_t		hp_max_count;
	uint_t		hp_empty_count;
	uint_t		hp_stole;
	uint_t		hp_pad;
};
typedef struct hment_pool	hment_pool_t;

/*
 * Starting with context 0, the first NUM_LOCKED_CTXS contexts
 * are locked so that srmmu_getctx can't steal any of these
 * contexts.  At the time this software was being developed, the
 * only context that needs to be locked is context 0, the kernel
 * context, and so this constant was originally defined to be 1.
 */
#define	NUM_LOCKED_CTXS 1

/*
 * Flags to pass to srmmu_pteunload()/srmmu_ptesync().
 * Note that since srmmu_pteunload() is called by
 * hat_unload() which has it own set of HAT_XXXX flags
 * so we use higher order bits to avoid flag collision.
 * This way we can piggy back these internal flags on
 * top of the generic hat flags.
 */
#define	SR_RMSYNC	0x01000000
#define	SR_INVSYNC	0x02000000
#define	SR_RMSTAT	0x04000000
#define	SR_NOFLUSH	0x08000000
#define	SR_NOPGFLUSH	0x10000000 /* only tlb flush for vac & pg_color */

/*
 * A flag used in srmmu_ptealloc() to allocate ptbls on
 * the fake tree for TMPUNLOAD stuff. This means this
 * flag should not be conflicting with the ones passed
 * in to srmmu_pteload(). But I suppose the same flag
 * could be use in unload routines to tell which tree
 * is to be operated on.
 */
#define	SR_TMP_TREE	0x20000000
#define	SR_UNLOAD_ALL	0x40000000
#define	SR_TMPUNLOAD	SX_TMPUNLOAD

#define	SR_FLAGS	0xff000000

/*
 * pte macros
 *	pte to containing ptbl
 *	pte to corresponding hment
 *	pte to offset in ptbl
 *	pte to virtual offset in ptbl
 *	pte to virtal address it maps
 *	pte to table address (for making a ptp point to pte)
 *	table address to pte (for finding pte from a ptp)
 *	tmp table address to pte
 *	pte to tmp table address
 */
#define	ptetooff(a)	(((uint_t)(a) & 0xff) >> 2)

#define	PTBL_GROUP	16

struct ptbl_gr {
	uint_t		pg_pt_pfn;	/* pfn for first pt in the group */
	pt_t		*pg_pt_va;	/* Va of pt group */
	struct page	*pg_page;	/* page_t for the pt group */
	struct ptbl_gr	*pg_next;	/* next lump */
	ptbl_t		pg_ptbl[PTBL_GROUP];	/* the actual ptbl's */
};
typedef struct ptbl_gr	ptbl_gr_t;

#define	ptbltopt_va(p) \
	((pte_t *)((((ptbl_gr_t *)((p)-((p)->ptbl_index)-1))->pg_pt_va) + \
	    (p)->ptbl_index))

#define	ptbltopt_ptp(p) \
	(((((ptbl_gr_t *)((p)-((p)->ptbl_index)-1))->pg_pt_pfn) << 8) + \
	((p)->ptbl_index << 4) + \
	MMU_ET_PTP)

/*
 * hme macros
 *	hme to corresponfing pte
 *	hme to virtual address it maps
 */
#define	hmetoptbl(a)	((a)->hme_ptbl)

/*
 * struct hat defines an array hat_data[4] for machine dependent use.
 * this is what the 4 entries are used for on srmmu machines
 */
#define	HAT_DATA_SRMMU	0
#define	HAT_DATA_COLOR_FLAGS	1
#define	HAT_DATA_COLOR_BIN	2
#define	HAT_DATA_AVAIL3		3

#define	astosrmmu(as) ((srmmu_t *)((as)->a_hat->hat_data[HAT_DATA_SRMMU]))

#define	hattosrmmu(hat) ((srmmu_t *)((hat)->hat_data[HAT_DATA_SRMMU]))

#if defined(_KERNEL)
/*
 * These operations are not supported by the SRMMU hat code.
 */
#define	srmmu_asload(vaddr)
#define	srmmu_exec(oas, ostka, stksz, nas, nstka, flag)	0

/*
 * These routines are all MMU-SPECIFIC interfaces to the srmmu routines.
 * These routines are called from machine specific places to do the
 * dirty work for things like boot initialization, mapin/mapout and
 * first level fault handling.
 */
#ifdef __STDC__
void srmmu_memload(struct hat *, struct as *, caddr_t, struct page *,
		    uint_t, int);
void srmmu_devload(struct hat *, struct as *, caddr_t, devpage_t *,
		    uint_t, uint_t, int);
void srmmu_mempte(struct page *, uint_t, struct pte *, caddr_t);
uint_t srmmu_vtop_prot(caddr_t, uint_t);
uint_t srmmu_ptov_prot(struct pte *);
struct pte *srmmu_ptefind(struct as *, caddr_t, int *, struct ptbl **,
	kmutex_t **, int);
struct pte *srmmu_ptefind_nolock(struct  as *, caddr_t, int *);
void srmmu_reserve(struct as *as, caddr_t addr, uint_t len, uint_t load_flag);
ulong_t hat_getkpfnum(caddr_t);

void lock_l1ptbl(struct ptbl *, uint_t, struct as *, kmutex_t **);
void unlock_l1ptbl(struct ptbl *, kmutex_t **);

void init_srmmu_var();



caddr_t ialloc_l1ptbl(caddr_t start, int npg);
caddr_t ialloc_l3ptbl(caddr_t start, int npg);
caddr_t ialloc_srmmu(caddr_t start, int npg);
caddr_t ialloc_hment(caddr_t start, int npg);
void ialloc_ptbl();

struct ptbl *srmmu_ptblreserve(caddr_t, uint_t, struct ptbl *, kmutex_t **);
void srmmu_unload(struct as *, caddr_t, uint_t, int);
void srmmu_ptbl_free(ptbl_t *, kmutex_t *);

/* Flags value for lock_ptbl */
#define	LK_PTBL_NOWAIT  0x1
#define	LK_PTBL_FAILOK  0x2

/* allow a shread ptbl to be locked by non-owner */
#define	LK_PTBL_SHARED  0x4

/* It's a on a tmpunload'ed Tree. */
#define	LK_PTBL_TMP  	0x8

/* Return values from lock_ptbl */
#define	LK_PTBL_OK		0x0
#define	LK_PTBL_MISMATCH	0x1
#define	LK_PTBL_FAIL_SHARED	0x2
#define	LK_PTBL_FAILED		0x3	/* only if LK_PTBL_NOWAIT is set. */

int lock_ptbl(struct ptbl *pptbl, uint_t flag, struct as *pas, caddr_t va,
	int level, kmutex_t **);
void unlock_ptbl(struct ptbl *pptbl, kmutex_t *);

/*
 * Special bits may be stuffed into p_mapping field to indicate
 * if it's a pointer to a pte or a pointer to a hment.
 *
 * We know ptes are 4 bytes long, so the last two bits are free.
 */
#define	PP_PTBL		0x1
#define	PP_NOSYNC	0x2
#define	CLR_LAST_2BITS  (~(PP_PTBL | PP_NOSYNC))

#endif

/*
 * This data is used in MACHINE-SPECIFIC places.
 */
extern	struct ctx *ctxs, *ectxs;
extern	struct ptp *contexts, *econtexts;	/* The real context table */

extern	struct ctx *kctx;
extern	struct l1pt *kl1pt;
extern	struct ptbl *kl1ptbl;
extern	union ptpe kl1ptp;

extern	srhment_t  **pte2hme_hash;	/* pointer to pte2hme hash table */
extern	int pte2hme_hashsz;		/* size in entries */
extern	int pte2hmehash_sz;		/* size in bytes */


extern	uint_t nctxs;

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_SRMMU_H */
