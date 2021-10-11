/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the sun referernce mmu (sfmmu)
 * specific hat data structures and the sfmmu specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 */

#ifndef	_VM_HAT_SFMMU_H
#define	_VM_HAT_SFMMU_H

#pragma ident	"@(#)hat_sfmmu.h	1.101	99/09/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

#include <sys/types.h>

#endif /* _ASM */

#ifdef	_KERNEL

#include <sys/pte.h>

/*
 * Don't alter these without considering changes to ism_map_t.
 */
#define	ISM_VBASE_SHIFT	MMU_PAGESHIFT4M		/* 4M */
#define	ISM_PG_SIZE	(1 << ISM_VBASE_SHIFT)
#define	ISM_SZ_MASK	(ISM_PG_SIZE - 1)
#define	ISM_MAP_SLOTS	8	/* Change this carefully. */

#ifndef _ASM

#include <sys/t_lock.h>
#include <vm/hat.h>
#include <sys/mmu.h>
#include <vm/seg.h>
#include <vm/mach_sfmmu.h>
#include <sys/machparam.h>
#include <sys/x_call.h>
#include <vm/mach_page.h>

typedef struct hat sfmmu_t;

/*
 * SFMMU attributes for hat_memload/hat_devload
 */
#define	SFMMU_UNCACHEPTTE	0x01000000	/* unencache in physical $ */
#define	SFMMU_UNCACHEVTTE	0x02000000	/* unencache in virtual $ */
#define	SFMMU_SIDEFFECT		0x04000000	/* set side effect bit */
#define	SFMMU_LOAD_ALLATTR	(HAT_PROT_MASK | HAT_ORDER_MASK |	\
		HAT_ENDIAN_MASK | HAT_NOFAULT | HAT_NOSYNC |		\
		SFMMU_UNCACHEPTTE | SFMMU_UNCACHEVTTE | SFMMU_SIDEFFECT)


/*
 * sfmmu flags for hat_memload/hat_devload
 */
#define	SFMMU_NO_TSBLOAD	0x08000000	/* do not preload tsb */
#define	SFMMU_LOAD_ALLFLAG	(HAT_LOAD | HAT_LOAD_LOCK |		\
		HAT_LOAD_ADV | HAT_LOAD_CONTIG | HAT_LOAD_NOCONSIST |	\
		HAT_LOAD_SHARE | HAT_LOAD_REMAP | SFMMU_NO_TSBLOAD |	\
		HAT_RELOAD_SHARE)

/*
 * mode for sfmmu_chgattr
 */
#define	SFMMU_SETATTR	0x0
#define	SFMMU_CLRATTR	0x1
#define	SFMMU_CHGATTR	0x2

/*
 * sfmmu specific flags for page_t
 */
#define	P_PNC	0x8		/* non-caching is permanent bit */
#define	P_TNC	0x10		/* non-caching is temporary bit */

#define	PP_GENERIC_ATTR(pp)	((pp)->p_nrm & (P_MOD | P_REF | P_RO))
#define	PP_ISMOD(pp)		((pp)->p_nrm & P_MOD)
#define	PP_ISREF(pp)		((pp)->p_nrm & P_REF)
#define	PP_ISNC(pp)		((pp)->p_nrm & (P_PNC|P_TNC))
#define	PP_ISPNC(pp)		((pp)->p_nrm & P_PNC)
#define	PP_ISTNC(pp)		((pp)->p_nrm & P_TNC)
#define	PP_ISRO(pp)		((pp)->p_nrm & P_RO)

#define	PP_SETMOD(pp)		((pp)->p_nrm |= P_MOD)
#define	PP_SETREF(pp)		((pp)->p_nrm |= P_REF)
#define	PP_SETREFMOD(pp)	((pp)->p_nrm |= (P_REF|P_MOD))
#define	PP_SETPNC(pp)		((pp)->p_nrm |= P_PNC)
#define	PP_SETTNC(pp)		((pp)->p_nrm |= P_TNC)
#define	PP_SETRO(pp)		((pp)->p_nrm |= P_RO)
#define	PP_SETREFRO(pp)		((pp)->p_nrm |= (P_REF|P_RO))

#define	PP_CLRMOD(pp)		((pp)->p_nrm &= ~P_MOD)
#define	PP_CLRREF(pp)		((pp)->p_nrm &= ~P_REF)
#define	PP_CLRREFMOD(pp)	((pp)->p_nrm &= ~(P_REF|P_MOD))
#define	PP_CLRPNC(pp)		((pp)->p_nrm &= ~P_PNC)
#define	PP_CLRTNC(pp)		((pp)->p_nrm &= ~P_TNC)
#define	PP_CLRRO(pp)		((pp)->p_nrm &= ~P_RO)

/*
 * All shared memory segments attached with the SHM_SHARE_MMU flag (ISM)
 * will be constrained to a 4M alignment. Also since every newly
 * created ISM segment is created out of a new address space at base va
 * of 0 we don't need to store it.
 */
#define	ISM_ALIGN	(1 << ISM_VBASE_SHIFT)	/* base va aligned to 4M  */
#define	ISM_ALIGNED(va)	(((uintptr_t)va & (ISM_ALIGN - 1)) == 0)
#define	ISM_SHIFT(x)	((uintptr_t)x >> (ISM_VBASE_SHIFT))

/*
 * All segments mapped with ISM are guarenteed to be 4M aligned.
 * Also size is guaranteed to be in 4M chunks.
 * ism_seg consists of the following members:
 * [XX..22] base address of ism segment. XX is 63 or 31 depending whether
 *	caddr_t is 64 bits or 32 bits.
 * [21..0] size of segment.
 *
 * NOTE: Don't alter this structure without changing defines above and
 * the tsb_miss and protection handlers.
 */
typedef struct ism_map {
	uintptr_t	imap_seg;  	/* base va + sz of ISM segment */
	sfmmu_t		*imap_ismhat; 	/* hat id of dummy ISM as */
	struct ism_ment	*imap_ment;	/* pointer to mapping list entry */
} ism_map_t;

#define	ism_start(map)	((caddr_t)((map).imap_seg & ~ISM_SZ_MASK))
#define	ism_size(map)	((map).imap_seg & ISM_SZ_MASK)
#define	ism_end(map)	((caddr_t)(ism_start(map) +		\
				(ism_size(map) * ISM_PG_SIZE)))
/*
 * ISM mapping entry. Used to link all hat's sharing a ism_hat.
 * Same function as the p_mapping list for a page.
 */
typedef struct ism_ment {
	sfmmu_t		*iment_hat;	/* back pointer to hat_share() hat */
	ism_map_t	*iment_map;	/* hat's mapping to this ism_hat */
	struct ism_ment	*iment_next;	/* next ism map entry */
	struct ism_ment	*iment_prev;	/* prev ism map entry */
} ism_ment_t;

/*
 * ISM segment block. One will be hung off the sfmmu structure if a
 * a process uses ISM.  More will be linked using ismblk_next if more
 * than ISM_MAP_SLOTS segments are attached to this proc.
 *
 * All modifications to fields in this structure will be protected
 * by the hat mutex.  In order to avoid grabbing
 * this lock in low level routines (tsb miss handlers, protection
 * handlers, and vatopfn) while not introducing any race conditions
 * with hat_unshare, we will set context reg of all cpus running
 * the process to INVALID_CONTEXT.  Processes will trap and end up in
 * sfmmu_tsb_miss where they will grab the hat mutex to synchronize
 * with the concurrent detach operation.
 */
typedef struct ism_blk {
	ism_map_t		iblk_maps[ISM_MAP_SLOTS];
	struct ism_blk		*iblk_next;
} ism_blk_t;

/*
 * The platform dependent hat structure.
 * tte counts should be protected by cas.
 * cpuset is protected by cas.
 */
struct hat {
	kmutex_t	sfmmu_mutex;	/* protects hat */
	cpuset_t	sfmmu_cpusran;	/* cpu bit mask for efficient xcalls */
	struct	as	*sfmmu_as;	/* as this hat provides mapping for */
	uint_t		sfmmu_rss;	/* approx # of pages used by as */
	uint_t		sfmmu_lttecnt;	/* # of large mappings in this hat */
	union _h_un {
		ism_blk_t	*sfmmu_iblkp;  /* maps to ismhat(s) */
		ism_ment_t	*sfmmu_imentp; /* ism hat's mapping list */
	} h_un;
	uint_t		sfmmu_free:1;	/* hat to be freed - set on as_free */
	uint_t		sfmmu_ismhat:1;	/* hat is dummy ism hatid */
	uint_t		sfmmu_ctxflushed:1;	/* hat is dummy ism hatid */
	uchar_t		sfmmu_rmstat;	/* refmod stats refcnt */
	uchar_t		sfmmu_clrstart;	/* start color bin for page coloring */
	ushort_t	sfmmu_clrbin;	/* per as phys page coloring bin */
	short		sfmmu_cnum;	/* context number */
};

#define	sfmmu_iblk	h_un.sfmmu_iblkp
#define	sfmmu_iment	h_un.sfmmu_imentp

/*
 * TSB information. We store the TSB register and the tte to lock in the DTLB
 * in this structure.
 */
struct tsb_info {
	uint64_t	tsb_reg;	/* tsb register */
	tte_t		tsb_tte;	/* tte to lock into DTLB */
};

/*
 * Software context structure.  The size of this structure is currently
 * hardwired into the tsb miss handlers in assembly code through the
 * CTX_SZ_SHIFT define.  Since this define is used in a shift we should keep
 * this structure a power of two.
 *
 * The only flag defined so far is LTTES_FLAG (large ttes).  This currently
 * means that at some point a large page mapping was created.  A future
 * optimization would be to reset the flag when sfmmu->lttecnt becomes
 * zero.
 */
struct ctx {
	union _c_un {
		sfmmu_t *c_sfmmup;	/* back pointer to hat id */
		struct ctx *c_freep;	/* next ctx in freelist */
	} c_un;

	/*
	 * Bit 0 : large pages flag.
	 * Bit 1 : large tsb flag.
	 * Bits 2 - 15 : index into tsb_bases array.
	 */
	ushort_t c_flags;	/* NOTE: keep c_flags/c_refcnt together */
				/* since we load/store them as an int */
	ushort_t c_refcnt;	/* used as rw-lock - for ctx-stealing */
				/* Usg: 0: free, 0xffff: w-lock, >0: r-lock */

	uint64_t c_ismblkpa;
				/* phys ptr to ISM blk. This is only for */
				/* performance. It allows us to service  */
				/* a tsb miss at tl > 0.		 */
#ifdef __sparcv9
	uint8_t	pad[8];		/* pad structure out to 32 bytes. */
#endif

};

#define	c_sfmmu	c_un.c_sfmmup
#define	c_free	c_un.c_freep

#ifdef	DEBUG
/*
 * For debugging purpose only. Maybe removed later.
 */
struct ctx_trace {
	sfmmu_t	*sc_sfmmu_stolen;
	sfmmu_t	*sc_sfmmu_stealing;
	clock_t		sc_time;
	ushort_t	sc_type;
	ushort_t	sc_cnum;
};
#define	CTX_STEAL	0x1
#define	CTX_FREE	0x0
#define	TRSIZE	0x400
#define	NEXT_CTXTR(ptr)	(((ptr) >= ctx_trace_last) ? \
		ctx_trace_first : ((ptr) + 1))
#define	TRACE_CTXS(ptr, cnum, stolen_sfmmu, stealing_sfmmu, type)	\
	(ptr)->sc_sfmmu_stolen = (stolen_sfmmu);		\
	(ptr)->sc_sfmmu_stealing = (stealing_sfmmu);		\
	(ptr)->sc_cnum = (cnum);				\
	(ptr)->sc_type = (type);				\
	(ptr)->sc_time = lbolt;					\
	(ptr) = NEXT_CTXTR(ptr);				\
	num_ctx_stolen += (type);
#else

#define	TRACE_CTXS(ptr, cnum, stolen_sfmmu, stealing_sfmmu, type)

#endif	DEBUG

#endif	/* !_ASM */

/*
 * Context flags
 */
#define	LTTES_FLAG	0x0001
#define	LTSB_FLAG	0x0002

#define	CTX_TSBINDEX_SHIFT	0x2
#define	CTX_FLAGS_MASK		0x3

#define	CTX_SET_LTTES(ctx)	\
	sfmmu_update_hword(&(ctx)->c_flags, (ctx)->c_flags | LTTES_FLAG)

#define	CTX_GET_TSBINDEX(ctx)	((ctx)->c_flags >> CTX_TSBINDEX_SHIFT)

#define	CTX_SET_TSBINDEX(ctx, num, flag)			\
{								\
	int tmp = (ctx)->c_flags & CTX_FLAGS_MASK;		\
	tmp |= (flag);						\
	tmp |= ((num) << CTX_TSBINDEX_SHIFT);			\
	sfmmu_update_hword(&(ctx)->c_flags, (ushort_t)tmp);	\
}

#define	CTX_GAP		2039	/* largest prime that will wrap every   */
				/* 4 ctxs. */

#define	ctxtoctxnum(ctx)	((ushort_t)((ctx) - ctxs))

/*
 * Busy bit in c_ismblkpa for hat_uhshare()
 */
#define	CTX_ISM_BUSY	0x1

/*
 * Defines needed for ctx stealing.
 */
#define	GET_CTX_RETRY_CNT	100

/*
 * Starting with context 0, the first NUM_LOCKED_CTXS contexts
 * are locked so that sfmmu_getctx can't steal any of these
 * contexts.  At the time this software was being developed, the
 * only context that needs to be locked is context 0 (the kernel
 * context), and context 1 (reserved for stolen context). So this constant
 * was originally defined to be 2.
 */
#define	NUM_LOCKED_CTXS 2
#define	INVALID_CONTEXT	1

#ifdef __sparcv9
#define	CTX_SZ_SHIFT	5
#else
#define	CTX_SZ_SHIFT	4
#endif

#ifndef	_ASM

/*
 * RFE: With multihat gone we gain back an int.  We could use this to
 * keep ref bits on a per cpu basis to eliminate xcalls.
 */
struct sf_hment {
	tte_t hme_tte;		/* tte for this hment */
	struct	machpage *hme_page;	/* what page this maps */
	struct	sf_hment *hme_next;	/* next hment */
	struct	sf_hment *hme_prev;	/* prev hment */
};

#define	hme_size(sfhmep)	((int)((sfhmep)->hme_tte.tte_size))

/*
 * hmeblk_tag structure
 * structure used to obtain a match on a hme_blk.  Currently consists of
 * the address of the sfmmu struct (or hatid), the base page address of the
 * hme_blk, and the rehash count.  The rehash count is actually only 2 bits
 * and has the following meaning:
 * 1 = 8k or 64k hash sequence.
 * 2 = 512k hash sequence.
 * 3 = 4M hash sequence.
 * We require this count because we don't want to get a false hit on a 512K or
 * 4M rehash with a base address corresponding to a 8k or 64k hmeblk.
 * Note:  The ordering and size of the hmeblk_tag members are implictly known
 * by the tsb miss handlers written in assembly.  Do not change this structure
 * without checking those routines.  See HTAG_SFMMUPSZ define.
 */

#ifdef __sparcv9

typedef union {
	struct {
		uint64_t	hblk_basepg: 51, /* hme_blk base pg # */
				hblk_rehash: 13; /* rehash number */
		sfmmu_t		*sfmmup;
	} hblk_tag_un;
	uint64_t		htag_tag[2];
} hmeblk_tag;

#else	/* ! __sparcv9 */

typedef union {
	struct {
		uint32_t	hblk_basepg: 19, /* hme_blk base pg # */
				hblk_rehash: 13; /* rehash number */
		sfmmu_t		*sfmmup;
	} hblk_tag_un;
	uint64_t	htag_tag;
} hmeblk_tag;

#endif 	/* __sparcv9 */

#define	htag_id		hblk_tag_un.sfmmup
#define	htag_bspage	hblk_tag_un.hblk_basepg
#define	htag_rehash	hblk_tag_un.hblk_rehash

#ifdef	__sparcv9
#define	HTAGS_EQ(tag1, tag2)	(((tag1.htag_tag[0] ^ tag2.htag_tag[0]) | \
				(tag1.htag_tag[1] ^ tag2.htag_tag[1])) == 0)
#else	/* ! __sparcv9 */
#define	HTAGS_EQ(tag1, tag2)	(tag1.htag_tag == tag2.htag_tag)
#endif	/* __sparcv9 */

#endif /* !_ASM */

#define	NHMENTS		8		/* # of hments in an 8k hme_blk */
					/* needs to be multiple of 2 */
#ifndef	_ASM

#ifdef	HBLK_TRACE

#define	HBLK_LOCK		1
#define	HBLK_UNLOCK		0
#define	HBLK_STACK_DEPTH	6
#define	HBLK_AUDIT_CACHE_SIZE	16
#define	HBLK_LOCK_PATTERN	0xaaaaaaaa
#define	HBLK_UNLOCK_PATTERN	0xbbbbbbbb

struct hblk_lockcnt_audit {
	int		flag;		/* lock or unlock */
	kthread_id_t	thread;
	int		depth;
	uintptr_t	stack[HBLK_STACK_DEPTH];
};

#endif	/* HBLK_TRACE */


/*
 * Hment block structure.
 * The hme_blk is the node data structure which the hash structure
 * mantains. An hme_blk can have 2 different sizes depending on the
 * number of hments it implicitly contains.  When dealing with 64K, 512K,
 * or 4M hments there is one hment per hme_blk.  When dealing with
 * 8k hments we allocate an hme_blk plus an additional 7 hments to
 * give us a total of 8 (NHMENTS) hments that can be referenced through a
 * hme_blk.
 *
 * The hmeblk structure contains 2 tte reference counters used to determine if
 * it is ok to free up the hmeblk.  Both counters have to be zero in order
 * to be able to free up hmeblk.  They are protected by cas.
 * hblk_hmecnt is the number of hments present on pp mapping lists.
 * hblk_vcnt reflects number of valid ttes in hmeblk.
 *
 * The hmeblk now also has per tte lock cnts.  This is required because
 * the counts can be high and there are not enough bits in the tte. When
 * physio is fixed to not lock the translations we should be able to move
 * the lock cnt back to the tte.  See bug id 1198554.
 */
struct hme_blk {
	uint64_t	hblk_nextpa;	/* physical address for hash list */

	hmeblk_tag	hblk_tag;	/* tag used to obtain an hmeblk match */

	struct hme_blk	*hblk_next;	/* on free list or on hash list */
					/* protected by hash lock */

	struct hme_blk	*hblk_shadow;	/* pts to shadow hblk */
					/* protected by hash lock */
	uint_t		hblk_span;	/* span of memory hmeblk maps */

	struct {
		ushort_t locked_cnt;	/* HAT_LOAD_LOCK ref cnt */
		uint_t	notused:12;
		uint_t	shadow_bit:1;	/* set for a shadow hme_blk */
		uint_t	nucleus_bit:1;	/* set for a nucleus hme_blk */
		uint_t	ttesize:2;	/* contains ttesz of hmeblk */
	} hblk_misc;

	union {
		struct {
			ushort_t hblk_hmecount;	/* hment on mlists counter */
			ushort_t hblk_validcnt;	/* valid tte reference count */
		} hblk_counts;
		uint_t		hblk_shadow_mask;
	} hblk_un;

#ifdef	HBLK_TRACE
	kmutex_t	hblk_audit_lock;	/* lock to protect index */
	uint_t		hblk_audit_index;	/* index into audit_cache */
	struct	hblk_lockcnt_audit hblk_audit_cache[HBLK_AUDIT_CACHE_SIZE];
#endif	/* HBLK_AUDIT */

	struct sf_hment hblk_hme[1];	/* hment array */
};

#define	hblk_lckcnt	hblk_misc.locked_cnt
#define	hblk_shw_bit	hblk_misc.shadow_bit
#define	hblk_nuc_bit	hblk_misc.nucleus_bit
#define	hblk_ttesz	hblk_misc.ttesize
#define	hblk_hmecnt	hblk_un.hblk_counts.hblk_hmecount
#define	hblk_vcnt	hblk_un.hblk_counts.hblk_validcnt
#define	hblk_shw_mask	hblk_un.hblk_shadow_mask

#define	MAX_HBLK_LCKCNT	0xFFFF
#define	HMEBLK_ALIGN	0x8		/* hmeblk has to be double aligned */

#ifdef	HBLK_TRACE

#define	HBLK_STACK_TRACE(hmeblkp, lock)					\
{									\
	int flag = lock;	/* to pacify lint */			\
	int audit_index;						\
									\
	mutex_enter(&hmeblkp->hblk_audit_lock);				\
	audit_index = hmeblkp->hblk_audit_index;			\
	hmeblkp->hblk_audit_index = ((hmeblkp->hblk_audit_index + 1) &	\
	    (HBLK_AUDIT_CACHE_SIZE - 1));				\
	mutex_exit(&hmeblkp->hblk_audit_lock);				\
									\
	if (flag)							\
		hmeblkp->hblk_audit_cache[audit_index].flag =		\
		    HBLK_LOCK_PATTERN;					\
	else								\
		hmeblkp->hblk_audit_cache[audit_index].flag =		\
		    HBLK_UNLOCK_PATTERN;				\
									\
	hmeblkp->hblk_audit_cache[audit_index].thread = curthread;	\
	hmeblkp->hblk_audit_cache[audit_index].depth =			\
	    getpcstack(hmeblkp->hblk_audit_cache[audit_index].stack,	\
	    HBLK_STACK_DEPTH);						\
}

#else

#define	HBLK_STACK_TRACE(hmeblkp, lock)

#endif	/* HBLK_TRACE */

#define	HMEHASH_FACTOR	16	/* used to calc # of buckets in hme hash */
/*
 * A maximum number of user hmeblks is defined in order to place an upper
 * limit on how much nuclues memory is required.  The number below
 * corresponds to the number of buckets for an average length of 4 in
 * a 16 machine.
 */
#define	MAX_UHME_BUCKETS 0x4000
#define	MAX_KHME_BUCKETS 0x2000
#define	MIN_KHME_BUCKETS 0x800

/*
 * There are 2 locks in the hmehash bucket.  The hmehash_mutex is
 * a regular mutex used to make sure operations on a hash link are only
 * done by one thread.  Any operation which comes into the hat with
 * a <vaddr, as> will grab the hmehash_mutex.  Normally one would expect
 * the tsb miss handlers to grab the hash lock to make sure the hash list
 * is consistent while we traverse it.  Unfortunately this can lead to
 * deadlocks or recursive mutex enters since it is possible for
 * someone holding the lock to take a tlb/tsb miss.
 * To solve this problem we have added the hmehash_listlock.  This lock
 * is only grabbed by the tsb miss handlers, vatopfn, and while
 * adding/removing a hmeblk from the hash list. The code is written to
 * guarantee we won't take a tlb miss while holding this lock.
 */
struct hmehash_bucket {
	kmutex_t	hmehash_mutex;
	uint64_t	hmeh_nextpa;	/* physical address for hash list */
	struct hme_blk *hmeblkp;
	uint_t		hmeh_listlock;
};

#endif /* !_ASM */


/*
 * The tsb miss handlers written in assembly know that sfmmup
 * is a either a 32 bit ptr for ILP32 or 64 bit ptr for LP64.
 *
 * For ILP32 machines we create the hmeblk tag by shifting the
 * bspage and re-hash (32 bits) up and OR in the sfmmup giving
 * us a 64 bit tag.
 *
 * For LP64 machines the bspage and re-hash part is now 64 bits.
 * with the sfmmup being another 64 bits.
 */
#ifdef __sparcv9
#define	HTAG_SFMMUPSZ		0	/* Not really used for LP64 */
#else
#define	HTAG_SFMMUPSZ		32
#endif

#define	HTAG_REHASHSZ		13

/*
 * Assembly routines need to be able to get to ttesz
 */
#define	HBLK_SZMASK		0x3

#ifndef _ASM

/*
 * Returns the number of bytes that an hmeblk spans given its tte size
 */
#define	get_hblk_span(hmeblkp) ((hmeblkp)->hblk_span)
#define	get_hblk_ttesz(hmeblkp)	((hmeblkp)->hblk_ttesz)
#define	HMEBLK_SPAN(ttesz)						\
	((ttesz == TTE8K)? (TTEBYTES(ttesz) * NHMENTS) : TTEBYTES(ttesz))

#define	set_hblk_sz(hmeblkp, ttesz)				\
	(hmeblkp)->hblk_ttesz = (ttesz);			\
	(hmeblkp)->hblk_span = HMEBLK_SPAN(ttesz)

#define	get_hblk_base(hmeblkp)	((uintptr_t)(hmeblkp)->hblk_tag.htag_bspage \
			<< MMU_PAGESHIFT)

#define	get_hblk_endaddr(hmeblkp)				\
	((caddr_t)(get_hblk_base(hmeblkp) + get_hblk_span(hmeblkp)))

#define	in_hblk_range(hmeblkp, vaddr)					\
	(((uintptr_t)(vaddr) >= get_hblk_base(hmeblkp)) &&		\
	((uintptr_t)(vaddr) < (get_hblk_base(hmeblkp) +			\
	get_hblk_span(hmeblkp))))

#define	tte_to_vaddr(hmeblkp, tte)	((caddr_t)(get_hblk_base(hmeblkp) \
	+ (TTEBYTES((tte).tte_size) * (tte).tte_hmenum)))

#define	vaddr_to_vshift(hblktag, vaddr, shwsz)				\
	((((uintptr_t)(vaddr) >> MMU_PAGESHIFT) - (hblktag.htag_bspage)) >>\
	TTE_BSZS_SHIFT((shwsz) - 1))

/*
 * Hment pool
 * The hment pool consists of 2 different linked lists of free hme_blks.
 * I need to differentiate between hmeblks having 8 vs 1 hment and between
 * nucleus and non-nucleus hmeblks. The nucleus hmeblks are queued up at the
 * front of the freelist and the dynamically allocated hmeblks at the rear.
 * The nucleus hmeblks originate from various chunks of nucleus memory at
 * boot time, they are reused frequently (hence better tlb hit rate).
 */
#define	HME8BLK_SZ	(sizeof (struct hme_blk) + \
			(NHMENTS - 1) * sizeof (struct sf_hment))

#define	HME1BLK_SZ	(sizeof (struct hme_blk))

#define	HME1_TRHOLD	15	/* threshold which triggers the */
				/* allocation of more hme1_blks */

#define	HME8_TRHOLD	15	/* threshold which triggers the */
				/* allocation of more hme8_blks */

#define	HBLK_GROW_NUM	10	/* number of hmeblks to kmem_alloc at a time */

/*
 * We have 2 mutexes to protect the hmeblk free lists.
 * One mutex protects the freelist for 8 hment hme_blks and the other
 * mutex protects the freelist for 1 hment hme_blks.
 */
#define	HBLK8_FLIST_LOCK()	(mutex_enter(&hblk8_lock))
#define	HBLK8_FLIST_UNLOCK()	(mutex_exit(&hblk8_lock))
#define	HBLK8_FLIST_ISHELD()	(mutex_owned(&hblk8_lock))

#define	HBLK1_FLIST_LOCK()	(mutex_enter(&hblk1_lock))
#define	HBLK1_FLIST_UNLOCK()	(mutex_exit(&hblk1_lock))
#define	HBLK1_FLIST_ISHELD()	(mutex_owned(&hblk1_lock))

/*
 * Hme_blk hash structure
 * Active mappings are kept in a hash structure of hme_blks.  The hash
 * function is based on (ctx, vaddr) The size of the hash table size is a
 * power of 2 such that the average hash chain lenth is HMENT_HASHAVELEN.
 * The hash actually consists of 2 separate hashes.  One hash is for the user
 * address space and the other hash is for the kernel address space.
 * The number of buckets are calculated at boot time and stored in the global
 * variables "uhmehash_num" and "khmehash_num".  By making the hash table size
 * a power of 2 we can use a simply & function to derive an index instead of
 * a divide.
 *
 * HME_HASH_FUNCTION(hatid, vaddr, shift) returns a pointer to a hme_hash
 * bucket.
 * An hme hash bucket contains a pointer to an hme_blk and the mutex that
 * protects the link list.
 * Spitfire supports 4 page sizes.  8k and 64K pages only need one hash.
 * 512K pages need 2 hashes and 4M pages need 3 hashes.
 * The 'shift' parameter controls how many bits the vaddr will be shifted in
 * the hash function. It is calculated in the HME_HASH_SHIFT(ttesz) function
 * and it varies depending on the page size as follows:
 *	8k pages:  	HBLK_RANGE_SHIFT
 *	64k pages:	MMU_PAGESHIFT64K
 *	512K pages:	MMU_PAGESHIFT512K
 *	4M pages:	MMU_PAGESHIFT4M
 * An assembly version of the hash function exists in sfmmu_ktsb_miss(). All
 * changes should be reflected in both versions.  This function and the TSB
 * miss handlers are the only places which know about the two hashes.
 *
 * HBLK_RANGE_SHIFT controls range of virtual addresses that will fall
 * into the same bucket for a particular process.  It is currently set to
 * be equivalent to 64K range or one hme_blk.
 *
 * The hme_blks in the hash are protected by a per hash bucket mutex
 * known as SFMMU_HASH_LOCK.
 * You need to acquire this lock before traversing the hash bucket link
 * list, while adding/removing a hme_blk to the list, and while
 * modifying an hme_blk.  A possible optimization is to replace these
 * mutexes by readers/writer lock but right now it is not clear whether
 * this is a win or not.
 *
 * The HME_HASH_TABLE_SEARCH will search the hash table for the
 * hme_blk that contains the hment that corresponds to the passed
 * ctx and vaddr.  It assumed the SFMMU_HASH_LOCK is held.
 */

#endif /* ! _ASM */

#define	KHATID			ksfmmup
#define	UHMEHASH_SZ		uhmehash_num
#define	KHMEHASH_SZ		khmehash_num
#define	HMENT_HASHAVELEN	4
#define	HBLK_RANGE_SHIFT	MMU_PAGESHIFT64K /* shift for HBLK_BS_MASK */
#define	MAX_HASHCNT		3

#ifndef _ASM

#define	HASHADDR_MASK(hashno)	TTE_PAGEMASK(hashno)

#define	HME_HASH_SHIFT(ttesz)						\
	((ttesz == TTE8K)? HBLK_RANGE_SHIFT : TTE_PAGE_SHIFT(ttesz))	\

#define	HME_HASH_ADDR(vaddr, hmeshift)					\
	((caddr_t)(((uintptr_t)(vaddr) >> (hmeshift)) << (hmeshift)))

#define	HME_HASH_BSPAGE(vaddr, hmeshift)				\
	(((uintptr_t)(vaddr) >> (hmeshift)) << ((hmeshift) - MMU_PAGESHIFT))

#define	HME_HASH_REHASH(ttesz)						\
	(((ttesz) < TTE512K)? 1 : (ttesz))

#define	HME_HASH_FUNCTION(hatid, vaddr, shift)				\
	((hatid != KHATID)?						\
	(&uhme_hash[ (((uintptr_t)(hatid) ^ ((uintptr_t)vaddr >> (shift))) \
		& UHMEHASH_SZ) ]):					\
	(&khme_hash[ (((uintptr_t)(hatid) ^ ((uintptr_t)vaddr >> (shift))) \
		& KHMEHASH_SZ) ]))

/*
 * This macro will traverse a hmeblk hash link list looking for an hme_blk
 * that owns the specified vaddr and hatid.  If if doesn't find one , hmeblkp
 * will be set to NULL, otherwise it will point to the correct hme_blk.
 * This macro also cleans empty hblks.
 */
#define	HME_HASH_SEARCH_PREV(hmebp, hblktag, hblkp, hblkpa, pr_hblk, prevpa) \
{									\
	struct hme_blk *nx_hblk;					\
	uint64_t 	nx_pa;						\
									\
	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));				\
	hblkp = hmebp->hmeblkp;						\
	hblkpa = hmebp->hmeh_nextpa;					\
	prevpa = 0;							\
	pr_hblk = NULL;							\
	while (hblkp) {							\
		if (HTAGS_EQ(hblkp->hblk_tag, hblktag)) {		\
			/* found hme_blk */				\
			break;						\
		}							\
		nx_hblk = hblkp->hblk_next;				\
		nx_pa = hblkp->hblk_nextpa;				\
		if (!hblkp->hblk_vcnt && !hblkp->hblk_hmecnt) {		\
			sfmmu_hblk_hash_rm(hmebp, hblkp, prevpa, pr_hblk); \
			sfmmu_hblk_free(hmebp, hblkp, hblkpa);		\
		} else {						\
			pr_hblk = hblkp;				\
			prevpa = hblkpa;				\
		}							\
		hblkp = nx_hblk;					\
		hblkpa = nx_pa;						\
	}								\
}

#define	HME_HASH_SEARCH(hmebp, hblktag, hblkp)				\
{									\
	struct hme_blk *pr_hblk;					\
	uint64_t hblkpa, prevpa;					\
									\
	HME_HASH_SEARCH_PREV(hmebp, hblktag, hblkp, hblkpa, pr_hblk,	\
		prevpa);						\
}

/*
 * This macro will traverse a hmeblk hash link list looking for an hme_blk
 * that owns the specified vaddr and hatid.  If if doesn't find one , hmeblkp
 * will be set to NULL, otherwise it will point to the correct hme_blk.
 * It doesn't remove empty hblks.
 */
#define	HME_HASH_FAST_SEARCH(hmebp, hblktag, hblkp)			\
	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));				\
	for (hblkp = hmebp->hmeblkp; hblkp;				\
	    hblkp = hblkp->hblk_next) {					\
		if (HTAGS_EQ(hblkp->hblk_tag, hblktag)) {		\
			/* found hme_blk */				\
			break;						\
		}							\
	}								\


#define	SFMMU_HASH_LOCK(hmebp)						\
		(mutex_enter(&hmebp->hmehash_mutex))

#define	SFMMU_HASH_UNLOCK(hmebp)					\
		(mutex_exit(&hmebp->hmehash_mutex))

#define	SFMMU_HASH_LOCK_TRYENTER(hmebp)					\
		(mutex_tryenter(&hmebp->hmehash_mutex))

#define	SFMMU_HASH_LOCK_ISHELD(hmebp)					\
		(mutex_owned(&hmebp->hmehash_mutex))

#define	SFMMU_XCALL_STATS(cpuset, ctxnum)				\
{									\
		int cpui;						\
									\
		for (cpui = 0; cpui < NCPU; cpui++) {			\
			if (CPU_IN_SET(cpuset, cpui)) {			\
				if (ctxnum == KCONTEXT) {		\
					SFMMU_STAT(sf_kernel_xcalls);	\
				} else {				\
					SFMMU_STAT(sf_user_xcalls);	\
				}					\
			}						\
		}							\
}

#define	astosfmmu(as)		((as)->a_hat)
#define	sfmmutoctxnum(sfmmup)	((sfmmup)->sfmmu_cnum)
#define	sfmmutoctx(sfmmup)	(&ctxs[sfmmutoctxnum(sfmmup)])
#define	astoctxnum(as)		(sfmmutoctxnum(astosfmmu(as)))
#define	astoctx(as)		(sfmmutoctx(astosfmmu(as)))
#define	hblktosfmmu(hmeblkp)	((sfmmu_t *)(hmeblkp)->hblk_tag.htag_id)
#define	sfmmutoas(sfmmup)	((sfmmup)->sfmmu_as)
#define	ctxnumtoctx(ctxnum)	(&ctxs[ctxnum])
/*
 * We use the sfmmu data structure to keep the per as page coloring info.
 */
#define	as_color_bin(as)	(astosfmmu(as)->sfmmu_clrbin)
#define	as_color_start(as)	(astosfmmu(as)->sfmmu_clrstart)

/*
 * TSB related structures
 *
 * The TSB is made up of tte entries.  Both the tag and data are present
 * in the TSB.  The TSB locking is managed as follows:
 * A software bit in the tsb tag is used to indicate that entry is locked.
 * If a cpu servicing a tsb miss reads a locked entry the tag compare will
 * fail forcing the cpu to go to the hat hash for the translation.
 * The cpu who holds the lock can then modify the data side, and the tag side.
 * The last write should be to the word containing the lock bit which will
 * clear the lock and allow the tsb entry to be read.  It is assumed that all
 * cpus reading the tsb will do so with atomic 128-bit loads.  An atomic 128
 * bit load is required to prevent the following from happening:
 *
 * cpu 0			cpu 1			comments
 *
 * ldx tag						tag unlocked
 *				ldstub lock		set lock
 *				stx data
 *				stx tag			unlock
 * ldx tag						incorrect tte!!!
 *
 * The software also maintains a bit in the tag to indicate an invalid
 * tsb entry.  The purpose of this bit is to allow the tsb invalidate code
 * to invalidate a tsb entry with a single cas.  See code for details.
 */

union tsb_tag {
	struct {
		uint32_t	tag_g:1;	/* copy of tte global bit */
		uint32_t	tag_inv:1;	/* sw - invalid tsb entry */
		uint32_t	tag_lock:1;	/* sw - locked tsb entry */
		uint32_t	tag_cnum:13;	/* context # for comparison */
		uint32_t	tag_res1:6;
		uint32_t	tag_va_hi:10;	/* va[63:54] */
		uint32_t	tag_va_lo;	/* va[53:22] */
	} tagbits;
	struct tsb_tagints {
		uint32_t	inthi;
		uint32_t	intlo;
	} tagints;
};
#define	tag_global		tagbits.tag_g
#define	tag_invalid		tagbits.tag_inv
#define	tag_locked		tagbits.tag_lock
#define	tag_ctxnum		tagbits.tag_cnum
#define	tag_vahi		tagbits.tag_va_hi
#define	tag_valo		tagbits.tag_va_lo
#define	tag_inthi		tagints.inthi
#define	tag_intlo		tagints.intlo

struct tsbe {
	union tsb_tag	tte_tag;
	tte_t		tte_data;
};

/*
 * A per cpu struct is kept that duplicates some info
 * used by the kernel tsb miss handlers plus it provides
 * a scratch area.  Its purpose is to minimize cache misses
 * in the tsb miss handler and it is 64 bytes (e$ line sz) on purpose.
 * There should be one allocated per cpu in nucleus memory and should be
 * aligned in an ecache line boundary.
 */
struct tsbmiss {
	sfmmu_t			*sfmmup;	/* kernel hat id */
	struct tsbe		*tsbptr;	/* hardware computed ptr */
	struct tsbe		*tsbptr4m;	/* hardware computed ptr */
	struct	ctx		*ctxs;
	struct hmehash_bucket	*khashstart;
	struct hmehash_bucket	*uhashstart;
	ushort_t 		khashsz;
	ushort_t		uhashsz;
	uint_t 			dcache_line_mask; /* used to flush dcache */
	uint32_t		itlb_misses;
	uint32_t		dtlb_misses;
	uint32_t		utsb_misses;
	uint32_t		ktsb_misses;
	uint16_t		uprot_traps;
	uint16_t		kprot_traps;

	/*
	 * For LP64 round up to 2 E$ lines.
	 *
	 * scratch[0] -> TSB_TAGACC
	 * scratch[1] -> TSBMISS_HMEBP
	 * scratch[2] -> TSBMISS_HATID
	 */
#ifdef __sparcv9
		uint64_t		scratch[3];
		uint8_t			unused[24];
#else
		uint32_t		scratch[3];
#endif
};

#endif /* !_ASM */


/*
 * The TSB
 * Only 128K, 256K and 512K sizes can be supported. Currently, we
 * implement 128K and 512K TSBs.
 * Only common configuration is supported.
 * Only 8k and 4mb entries are supported in the tsb.
 */
#define	TSB_MIN_SZCODE		TSB_128K_SZCODE	/* min. supported tsb size */
#define	TSB_MIN_OFFSET_MASK	(TSB_OFFSET_MASK(TSB_MIN_SZCODE))
#define	TSB_MIN_BASE_MASK	~TSB_MIN_OFFSET_MASK
#define	UTSB_MAX_SZCODE		TSB_512K_SZCODE	/* max. supported tsb size */
#define	UTSB_MAX_OFFSET_MASK	(TSB_OFFSET_MASK(UTSB_MAX_SZCODE))
#define	UTSB_MAX_BASE_MASK	~UTSB_MAX_OFFSET_MASK

#define	KTSBNUM		(tsb_num)		/* assign to last tsb */
#define	INVALTSBNUM	(0)			/* assign to first tsb */
#define	KTSBINFO	(&tsb_bases[KTSBNUM])	/* kernel tsb info */
#define	TSB_SIZE_FACTOR	4

/*
 * Context bits to use during TSB pointer computation.
 */
#define	TSB_MAX_NUM		256		/* max. number of TSBs */
#define	TSB_FREEMEM_MIN		0x1000		/* 32 mb */
#define	TSB_FREEMEM_LARGE	0x10000		/* 512 mb */
#define	TSB_128K_SZCODE		4		/* 8k entries */
#define	TSB_256K_SZCODE		5		/* 16k entries */
#define	TSB_512K_SZCODE		6		/* 32k entries */
#define	TSB_1MB_SZCODE		7		/* 64k entries */
#define	TSB_SPLIT_CODE		TSB_COMMON_CONFIG
#define	TSB_ENTRY_SHIFT		4	/* each entry = 128 bits = 16 bytes */
#define	TSB_ENTRY_SIZE		(1 << 4)
#define	TSB_START_SIZE		9
#define	TSB_ENTRIES(tsbsz) (1 << (TSB_START_SIZE + (tsbsz) + TSB_SPLIT_CODE))
#define	TSB_BYTES(tsbsz)	(TSB_ENTRIES(tsbsz) << TSB_ENTRY_SHIFT)
#define	TSB_OFFSET_MASK(tsbsz)	(TSB_ENTRIES(tsbsz) - 1)
#define	TSB_SIZESHIFT_MASK	0x3	/* used so tsb sz affects ctx shift */

#define	BIGKTSB_SZ_MASK		0xf
#define	TSB_SOFTSZ_MASK		BIGKTSB_SZ_MASK
#define	MIN_BIGKTSB_SZCODE	9	/* 256k entries */
#ifdef __sparcv9
#define	MAX_BIGKTSB_SZCODE	11	/* 1024k entries */
#else
#define	MAX_BIGKTSB_SZCODE	10	/* 512k entries */
#endif
#define	MAX_BIGKTSB_TTES	(TSB_BYTES(MAX_BIGKTSB_SZCODE) / MMU_PAGESIZE4M)

#define	TAG_VALO_SHIFT		22		/* tag's va are bits 63-22 */
/*
 * sw bits used on tsb_tag - bit masks used only in assembly
 * use only a sethi for these fields.
 */
#define	TSBTAG_CTXMASK	0x1fff0000
#define	TSBTAG_CTXSHIFT	16
#define	TSBTAG_INVALID	0x40000000		/* tsb_tag.tag_invalid */
#define	TSBTAG_LOCKED	0x20000000		/* tsb_tag.tag_locked */

#ifndef _ASM

/*
 * Page coloring
 * The p_vcolor field of the page struct (1 byte) is used to store the
 * virtual page color.  This provides for 255 colors.  The value zero is
 * used to mean the page has no color - never been mapped or somehow
 * purified.
 */

#define	PP_GET_VCOLOR(pp)	(((pp)->p_vcolor) - 1)
#define	PP_NEWPAGE(pp)		(!(pp)->p_vcolor)
#define	PP_SET_VCOLOR(pp, color)                                          \
	((pp)->p_vcolor = ((color) + 1))

/*
 * As mentioned p_vcolor == 0 means there is no color for this page.
 * But PP_SET_VCOLOR(pp, color) expects 'color' to be real color minus
 * one so we define this constant.
 */
#define	NO_VCOLOR	(-1)

#define	addr_to_vcolor(addr)						\
	((int)(((uintptr_t)(addr) & (shm_alignment - 1)) >> MMU_PAGESHIFT))

/*
 * The field p_index in the psm page structure is for large pages support.
 * P_index is a bit-vector of the different mapping sizes that a given page
 * is part of. An hme structure for a large mapping is only added in the
 * group leader page (first page). All pages covered by a given large mapping
 * have the corrosponding mapping bit set in their p_index field. This allows
 * us to only store an explicit hme structure in the leading page which
 * simplifies the mapping link list management. Furthermore, it provides us
 * a fast mechanism for determining the largest mapping a page is part of. For
 * exmaple, a page with a 64K and a 4M mappings has a p_index value of 0x0A.
 *
 * Implementation note: even though the first bit in p_index is reserved
 * for 8K mappings, it is NOT USED by the code and SHOULD NOT be set.
 * In addition, the upper four bits of the p_index field are used by the
 * code as temporaries
 */

/*
 * Defines for psm page struct fields and large page support
 */
#define	SFMMU_INDEX_SHIFT	4	/* sv old index in upper nibble */
#define	SFMMU_INDEX_MASK	((1 << SFMMU_INDEX_SHIFT) - 1)

/* Return the mapping index */
#define	PP_MAPINDEX(pp)	((pp)->p_index & SFMMU_INDEX_MASK)

/*
 * These macros rely on the following property:
 * All pages constituting a large page are covered by a virtually
 * contiguous set of machpage_t's.
 */

/* Return the leader for this mapping size */
#define	PP_GROUPLEADER(pp, sz) \
	(&(pp)[-(int)((pp)->p_pagenum & (TTEPAGES(sz)-1))])

/* Return the root page for this page based on p_cons */
#define	PP_PAGEROOT(pp)	(((pp))->p_cons == 0 ? (pp) : \
	PP_GROUPLEADER((pp), (pp)->p_cons))

#define	PP_PAGENEXT_N(pp, n)	((pp) + (n))
#define	PP_PAGENEXT(pp)		PP_PAGENEXT_N((pp), 1)

#define	PP_ISMAPPED_LARGE(pp)	(PP_MAPINDEX(pp) != 0)

/* Need function to test the page mappping which takes p_index into account */
#define	PP_ISMAPPED(pp)	((pp)->p_mapping || PP_ISMAPPED_LARGE(pp))

/*
 * Don't call this macro with sz equal to zero. 8K mappings SHOULD NOT
 * set p_index field.
*/
#define	PAGESZ_TO_INDEX(sz)	(1 << (sz))

/*
 * prototypes for hat assembly routines
 */
extern void	sfmmu_make_tsbreg(uint64_t *, caddr_t, int, int);
extern void	sfmmu_load_tsbstate(int);
extern void	sfmmu_load_tsbstate_tl1(uint64_t, uint64_t);
extern void	sfmmu_ctx_steal_tl1(uint64_t, uint64_t);
extern void	sfmmu_itlb_ld(caddr_t, int, tte_t *);
extern void	sfmmu_dtlb_ld(caddr_t, int, tte_t *);
extern void	sfmmu_copytte(tte_t *, tte_t *);
extern int	sfmmu_modifytte(tte_t *, tte_t *, tte_t *);
extern int	sfmmu_modifytte_try(tte_t *, tte_t *, tte_t *);
extern pfn_t	sfmmu_ttetopfn(tte_t *, caddr_t);

extern uint_t	get_color_flags(struct as *);
extern int	sfmmu_get_ppvcolor(struct machpage *);
extern int	sfmmu_get_addrvcolor(caddr_t);
extern void	sfmmu_hblk_hash_rm(struct hmehash_bucket *,
			struct hme_blk *, uint64_t, struct hme_blk *);
extern void	sfmmu_hblk_hash_add(struct hmehash_bucket *, struct hme_blk *,
			uint64_t);
extern void	sfmmu_init_tsbs(void);

/*
 * functions known to hat_sfmmu.c
 */
void	sfmmu_disallow_ctx_steal(sfmmu_t *);
void	sfmmu_allow_ctx_steal(sfmmu_t *);


/*
 * functions and data known to mach_sfmmu.c
 */
extern void	sfmmu_patch_ktsb(void);
extern void	sfmmu_patch_utsb(void);
extern int	getctx(void);
extern pfn_t	sfmmu_vatopfn(caddr_t, sfmmu_t *);
extern pfn_t	sfmmu_user_vatopfn(caddr_t, sfmmu_t *);
extern void	sfmmu_memtte(tte_t *, pfn_t, uint_t, int);
extern void	sfmmu_tteload(struct hat *, tte_t *, caddr_t, machpage_t *,
			uint_t);
extern caddr_t	sfmmu_tsb_alloc(caddr_t, pgcnt_t);
extern void	sfmmu_load_tsb(caddr_t, int, tte_t *);
extern void	sfmmu_load_tsb4m(caddr_t, int, tte_t *);
extern int	sfmmu_getctx_pri(void);
extern int	sfmmu_getctx_sec(void);
extern void	sfmmu_setctx_pri(int);
extern void	sfmmu_setctx_sec(int);
extern uint_t	sfmmu_get_dsfar(void);
extern uint_t	sfmmu_get_isfsr(void);
extern uint_t	sfmmu_get_dsfsr(void);
extern uint_t	sfmmu_get_itsb(void);
extern uint_t	sfmmu_get_dtsb(void);
extern void	sfmmu_set_itsb(pfn_t, uint_t, uint_t);
extern void	sfmmu_set_dtsb(pfn_t, uint_t, uint_t);
extern void	sfmmu_unload_tsb(caddr_t, int);
extern void	sfmmu_unload_tsb4m(caddr_t, int);
extern void	sfmmu_unload_tsbctx(uint_t);
extern void	sfmmu_inv_tsb(caddr_t, uint_t);
extern void	sfmmu_migrate_tsbctx(uint_t, uint64_t *, uint64_t *);
extern void	sfmmu_update_hword(ushort_t *, ushort_t);
extern void	hat_kern_setup(void);
extern void	sfmmu_hblk_init(void);
extern void	sfmmu_add_nucleus_hblks(caddr_t, size_t);
extern void	sfmmu_cache_flushall(void);

extern sfmmu_t 		*ksfmmup;
extern struct ctx	*ctxs, *ectxs;
extern uint_t		nctxs;
extern caddr_t		tsballoc_base;
extern int		tsb_num;
extern int		tsb512k_num;
extern caddr_t		ktsb_base;
extern int		ktsb_sz;
extern int		ktsb_szcode;
extern int		enable_bigktsb;
extern struct tsb_info	*tsb_bases;
extern struct tsb_info	*tsb512k_bases;
extern int		utsb_dtlb_ttenum;
extern int		utsb_4m_disable;
extern int		uhmehash_num;
extern int		khmehash_num;
extern struct hmehash_bucket *uhme_hash;
extern struct hmehash_bucket *khme_hash;
extern kmutex_t		*mml_table;
extern int		mml_table_sz;
extern struct tsbmiss	tsbmiss_area[NCPU];

#endif /* !_ASM */

#endif /* _KERNEL */

#ifndef _ASM
/*
 * ctx, hmeblk, mlistlock and other stats for sfmmu
 */
struct sfmmu_global_stat {
	int		sf_slow_tsbmiss;	/* # of slow tsb misses */
	int		sf_pagefaults;		/* # of pagefaults */
	int		sf_uhash_searches;	/* # of user hash searches */
	int		sf_uhash_links;		/* # of user hash links */
	int		sf_khash_searches;	/* # of kernel hash searches */
	int		sf_khash_links;		/* # of kernel hash links */

	int		sf_ctxfree;		/* ctx alloc from free list */
	int		sf_ctxdirty;		/* ctx alloc from dirty list */
	int		sf_ctxsteal;		/* ctx allocated by steal */

	int		sf_tteload8k;		/* calls to sfmmu_tteload */
	int		sf_tteload64k;		/* calls to sfmmu_tteload */
	int		sf_tteload512k;		/* calls to sfmmu_tteload */
	int		sf_tteload4m;		/* calls to sfmmu_tteload */
	int		sf_hblk_hit;		/* found hblk during tteload */
	int		sf_hblk8_nalloc;	/* alloc nucleus hmeblk */
	int		sf_hblk8_dalloc;	/* alloc dynamic hmeblk */
	int		sf_hblk8_dfree;		/* free dynamic hmeblk */
	int		sf_hblk1_nalloc;	/* alloc nucleus hmeblk */
	int		sf_hblk1_dalloc;	/* alloc dynamic hmeblk */
	int		sf_hblk1_dfree;		/* free dynamic hmeblk */
	int		sf_hblk8_startup_use;

	int		sf_pgcolor_conflict;	/* VAC conflict resolution */
	int		sf_uncache_conflict;	/* VAC conflict resolution */
	int		sf_unload_conflict;	/* VAC unload resolution */
	int		sf_ism_uncache;		/* VAC conflict resolution */
	int		sf_ism_recache;		/* VAC conflict resolution */
	int		sf_recache;		/* VAC conflict resolution */

	int		sf_steal_count;		/* # of hblks stolen */

	int		sf_mlist_enter;		/* calls to mlist_lock_enter */
	int		sf_mlist_exit;		/* calls to mlist_lock_exit */
	int		sf_pagesync;		/* # of pagesyncs */
	int		sf_pagesync_invalid;	/* pagesync with inv tte */
	int		sf_kernel_xcalls;	/* # of kernel cross calls */
	int		sf_user_xcalls;		/* # of user cross calls */
	int		sf_tsb_resize;		/* # of user tsb resize */
	int		sf_tsb_resize_failures;	/* # of user tsb resize */

	int		sf_user_vtop;		/* # of user vatopfn calls */
};

struct sfmmu_percpu_stat {
	int	sf_itlb_misses;		/* # of itlb misses */
	int	sf_dtlb_misses;		/* # of dtlb misses */
	int	sf_utsb_misses;		/* # of user tsb misses */
	int	sf_ktsb_misses;		/* # of kernel tsb misses */
	int	sf_tsb_hits;		/* # of tsb hits */
	int	sf_umod_faults;		/* # of mod (prot viol) flts */
	int	sf_kmod_faults;		/* # of mod (prot viol) flts */
};

#define	SFMMU_STAT(stat)		sfmmu_global_stat.stat++;
#define	SFMMU_STAT_ADD(stat, amount)	sfmmu_global_stat.stat += amount;
#define	SFMMU_STAT_SET(stat, count)	sfmmu_global_stat.stat = count;

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_SFMMU_H */
