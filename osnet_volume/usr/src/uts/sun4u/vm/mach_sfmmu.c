/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mach_sfmmu.c	1.112	98/10/25 SMI"

#include <sys/types.h>
#include <vm/hat.h>
#include <vm/hat_sfmmu.h>
#include <vm/page.h>
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
#include <sys/bitmap.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <sys/t_lock.h>
#include <sys/vm_machparam.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/prom_debug.h>
#include <sys/privregs.h>
#include <sys/bootconf.h>
#include <sys/memlist.h>
#include <sys/memlist_plat.h>

/*
 * Static routines
 */
static void	sfmmu_remap_kernel(void);
static void	sfmmu_set_tlb(void);
static void	sfmmu_map_prom_mappings(struct translation *, size_t);
static struct translation *read_prom_mappings(size_t *);
static int	sfmmu_is_tsbaddr(caddr_t vaddr);

/*
 * Global Data:
 */
caddr_t	textva, datava;
tte_t	ktext_tte, kdata_tte;		/* ttes for kernel text and data */
int	max_utsb = TSB_MAX_NUM;		/* max. number of user TSBs */

int	enable_bigktsb = 1;

static	tte_t bigktsb_ttes[MAX_BIGKTSB_TTES];
static	int bigktsb_nttes = 0;

/*
 * XXX Need to understand how crash uses this routine and get rid of it
 * if possible.
 */
void
mmu_setctx(struct ctx *ctx)
{
#ifdef lint
	ctx = ctx;
#endif /* lint */

	STUB(mmu_setctx);
}

/*
 * Global Routines called from within:
 *	usr/src/uts/sun4u
 *	usr/src/uts/sfmmu
 *	usr/src/uts/sun
 */

pfn_t
va_to_pfn(void *vaddr)
{
	u_longlong_t physaddr;
	int mode, valid;

	if (tba_taken_over) {
		return (sfmmu_vatopfn(vaddr, KHATID));
	}

	if ((prom_translate_virt(vaddr, &valid, &physaddr, &mode) != -1) &&
	    (valid == -1)) {
		return ((pfn_t)(physaddr >> MMU_PAGESHIFT));
	}
	return (PFN_INVALID);
}

uint64_t
va_to_pa(void *vaddr)
{
	pfn_t pfn;

	if ((pfn = va_to_pfn(vaddr)) == PFN_INVALID)
		return ((uint64_t)-1);
	return (((uint64_t)pfn << MMU_PAGESHIFT) |
		((uint64_t)vaddr & MMU_PAGEOFFSET));
}

void
hat_kern_setup(void)
{
	uint_t tsbsz;
	struct translation *trans_root;
	size_t ntrans_root;
	extern void startup_fixup_physavail(void);

	/*
	 * These are the steps we take to take over the mmu from the prom.
	 *
	 * (1)	Read the prom's mappings through the translation property.
	 * (2)	Remap the kernel text and kernel data with 2 locked 4MB ttes.
	 *	Create the the hmeblks for these 2 ttes at this time.
	 * (3)	Create hat structures for all other prom mappings.  Since the
	 *	kernel text and data hme_blks have already been created we
	 *	skip the equivalent prom's mappings.
	 * (4)	Initialize the tsb and its corresponding hardware regs.
	 * (5)	Take over the trap table (currently in startup).
	 * (6)	Up to this point it is possible the prom required some of its
	 *	locked tte's.  Now that we own the trap table we remove them.
	 */

	sfmmu_patch_ktsb();
	/*
	 * patch trap table if we are not going to support 4M
	 * tte's in user tsb's.
	 */
	if (utsb_4m_disable)
		sfmmu_patch_utsb();
	sfmmu_init_tsbs();
	trans_root = read_prom_mappings(&ntrans_root);
	sfmmu_remap_kernel();
	startup_fixup_physavail();
	sfmmu_map_prom_mappings(trans_root, ntrans_root);
	tsbsz = TSB_BYTES(ktsb_szcode);
	/*
	 * We inv kernel tsb because we used it in
	 * sfmmu_map_prom_mappings()
	 */
	sfmmu_inv_tsb(ktsb_base, tsbsz);
	sfmmu_load_tsbstate(INVALID_CONTEXT);

}

/*
 * This routine remaps the kernel using large ttes
 * All entries except locked ones will be removed from the tlb.
 * It assumes that both the text and data segments reside in a separate
 * 4mb virtual and physical contigous memory chunk.  This routine
 * is only executed by the first cpu.  The remaining cpus execute
 * sfmmu_mp_startup() instead.
 * XXX It assumes that the start of the text segment is KERNELBASE.  It should
 * actually be based on start.
 */
static void
sfmmu_remap_kernel(void)
{
	pfn_t	pfn;
	u_int	attr;
	int	flags;

	extern char end[];
	extern struct as kas;

	textva = (caddr_t)(KERNELBASE & MMU_PAGEMASK4M);
	pfn = va_to_pfn(textva);
	if (pfn == PFN_INVALID)
		prom_panic("can't find kernel text pfn");
	pfn &= TTE_PFNMASK(TTE4M);

	attr = PROC_TEXT | HAT_NOSYNC;
	flags = HAT_LOAD_LOCK | SFMMU_NO_TSBLOAD;
	sfmmu_memtte(&ktext_tte, pfn, attr, TTE4M);
	/*
	 * We set the lock bit in the tte to lock the translation in
	 * the tlb.
	 */
	ktext_tte.tte_lock = 1;
	sfmmu_tteload(kas.a_hat, &ktext_tte, textva, (struct machpage *)NULL,
		flags);

	datava = (caddr_t)((uintptr_t)end & MMU_PAGEMASK4M);
	pfn = va_to_pfn(datava);
	if (pfn == PFN_INVALID)
		prom_panic("can't find kernel data pfn");
	pfn &= TTE_PFNMASK(TTE4M);

	attr = PROC_DATA | HAT_NOSYNC;
	sfmmu_memtte(&kdata_tte, pfn, attr, TTE4M);
	/*
	 * We set the lock bit in the tte to lock the translation in
	 * the tlb.  We also set the mod bit to avoid taking dirty bit
	 * traps on kernel data.
	 */
	TTE_SET_LOFLAGS(&kdata_tte, TTE_LCK_INT | TTE_HWWR_INT,
		TTE_LCK_INT | TTE_HWWR_INT);
	sfmmu_tteload(kas.a_hat, &kdata_tte, datava, (struct machpage *)NULL,
		flags);

	/*
	 * create bigktsb ttes if necessary.
	 */
	if (enable_bigktsb) {
		int i = 0;
		caddr_t va = ktsb_base;
		size_t tsbsz = ktsb_sz;
		tte_t tte;

		ASSERT(va >= datava + MMU_PAGESIZE4M);
		ASSERT(tsbsz >= MMU_PAGESIZE4M);
		ASSERT(IS_P2ALIGNED(tsbsz, tsbsz));
		ASSERT(IS_P2ALIGNED(va, tsbsz));
		attr = PROC_DATA | HAT_NOSYNC;
		while (tsbsz != 0) {
			ASSERT(i < MAX_BIGKTSB_TTES);
			pfn = va_to_pfn(va);
			ASSERT(pfn != PFN_INVALID);
			ASSERT((pfn & ~TTE_PFNMASK(TTE4M)) == 0);
			sfmmu_memtte(&tte, pfn, attr, TTE4M);
			ASSERT(TTE_IS_MOD(&tte));
			tte.tte_lock = 1;
			sfmmu_tteload(kas.a_hat, &tte, va, NULL, flags);
			bigktsb_ttes[i] = tte;
			va += MMU_PAGESIZE4M;
			tsbsz -= MMU_PAGESIZE4M;
			i++;
		}
		bigktsb_nttes = i;
	}

	sfmmu_set_tlb();
}

/*
 * Setup the kernel's locked tte's
 */
static void
sfmmu_set_tlb(void)
{
	dnode_t node;
	u_int len, index;

	node = cpunodes[getprocessorid()].nodeid;
	len = prom_getprop(node, "#itlb-entries", (caddr_t)&index);
	if (len != sizeof (index))
		prom_panic("bad #itlb-entries property");
	(void) prom_itlb_load(index - 1, *(uint64_t *)&ktext_tte, textva);
	len = prom_getprop(node, "#dtlb-entries", (caddr_t)&index);
	if (len != sizeof (index))
		prom_panic("bad #dtlb-entries property");
	(void) prom_dtlb_load(index - 1, *(uint64_t *)&kdata_tte, datava);
	(void) prom_dtlb_load(index - 2, *(uint64_t *)&ktext_tte, textva);
	index -= 3;

	if (enable_bigktsb) {
		int i;
		caddr_t va = ktsb_base;
		uint64_t tte;

		ASSERT(bigktsb_nttes <= MAX_BIGKTSB_TTES);
		for (i = 0; i < bigktsb_nttes; i++) {
			tte = *(uint64_t *)&bigktsb_ttes[i];
			(void) prom_dtlb_load(index, tte, va);
			va += MMU_PAGESIZE4M;
			index--;
		}
	}

	utsb_dtlb_ttenum = index;
}

/*
 * This routine is executed by all other cpus except the first one
 * at initialization time.  It is responsible for taking over the
 * mmu from the prom.  We follow these steps.
 * Lock the kernel's ttes in the TLB
 * Initialize the tsb hardware registers
 * Take over the trap table
 * Flush the prom's locked entries from the TLB
 */
void
sfmmu_mp_startup(void)
{
	extern struct scb trap_table;

	sfmmu_set_tlb();
	sfmmu_load_tsbstate(INVALID_CONTEXT);
	setwstate(WSTATE_KERN);
	prom_set_traptable((caddr_t)&trap_table);
	install_va_to_tte();
}

/*
 * Macro used below to convert the prom's 32-bit high and low fields into
 * a value appropriate for either the 32-bit or 64-bit kernel.  Ignore the
 * upper 32-bits on 32-bit machines.
 */
#ifdef __sparcv9
#define	COMBINE(hi, lo) (((uint64_t)(uint32_t)(hi) << 32) | (uint32_t)(lo))
#else
#define	COMBINE(hi, lo) (lo)
#endif

/*
 * This function traverses the prom mapping list and creates equivalent
 * mappings in the sfmmu mapping hash.
 */
static void
sfmmu_map_prom_mappings(struct translation *trans_root, size_t ntrans_root)
{
	struct translation *promt;
	tte_t	tte, *ttep;
	pfn_t	pfn, oldpfn, basepfn;
	caddr_t vaddr;
	size_t	size, offset;
	unsigned long i;
	u_int	attr;
	int flags = HAT_LOAD_LOCK | SFMMU_NO_TSBLOAD;
	struct machpage *pp;
	extern struct memlist *virt_avail;

	ttep = &tte;
	for (i = 0, promt = trans_root; i < ntrans_root; i++, promt++) {
		ASSERT(promt->tte_hi != 0);
		ASSERT32(promt->virt_hi == 0 && promt->size_hi == 0);

		/*
		 * hack until we get rid of map-for-unix
		 */
		if (COMBINE(promt->virt_hi, promt->virt_lo) < KERNELBASE)
			continue;

		ttep->tte_inthi = promt->tte_hi;
		ttep->tte_intlo = promt->tte_lo;
		attr = PROC_DATA | HAT_NOSYNC;
		if (TTE_IS_GLOBAL(ttep)) {
			/*
			 * The prom better not use global translations
			 * because a user process might use the same
			 * virtual addresses
			 */
			cmn_err(CE_PANIC, "map_prom: global translation");
			TTE_SET_LOFLAGS(ttep, TTE_GLB_INT, 0);
		}
		if (TTE_IS_LOCKED(ttep)) {
			/* clear the lock bits */
			TTE_SET_LOFLAGS(ttep, TTE_LCK_INT, 0);
		}
		if (!TTE_IS_VCACHEABLE(ttep)) {
			attr |= SFMMU_UNCACHEVTTE;
		}
		if (!TTE_IS_PCACHEABLE(ttep)) {
			attr |= SFMMU_UNCACHEPTTE;
		}
		if (TTE_IS_SIDEFFECT(ttep)) {
			attr |= SFMMU_SIDEFFECT;
		}
		if (TTE_IS_IE(ttep)) {
			attr |= HAT_STRUCTURE_LE;
		}

		size = COMBINE(promt->size_hi, promt->size_lo);
		offset = 0;
		basepfn = TTE_TO_PFN((caddr_t)COMBINE(promt->virt_hi,
		    promt->virt_lo), ttep);
		while (size) {
			vaddr = (caddr_t)(COMBINE(promt->virt_hi,
			    promt->virt_lo) + offset);
			/*
			 * make sure address is not in virt-avail list
			 */
			if (address_in_memlist(virt_avail, (uint64_t)vaddr,
			    size)) {
				cmn_err(CE_PANIC, "map_prom: inconsistent "
				    "translation/avail lists");
			}

			pfn = basepfn + mmu_btop(offset);
			if (pf_is_memory(pfn)) {
				if (attr & SFMMU_UNCACHEPTTE) {
					cmn_err(CE_PANIC, "map_prom: "
					    "uncached prom memory page");
				}
			} else {
				if (!(attr & SFMMU_SIDEFFECT)) {
					cmn_err(CE_PANIC, "map_prom: prom "
					    "i/o page without side-effect");
				}
			}
			oldpfn = sfmmu_vatopfn(vaddr, KHATID);
			if (oldpfn != PFN_INVALID) {
				/*
				 * mapping already exists.
				 * Verify they are equal
				 */
				if (pfn != oldpfn) {
					cmn_err(CE_PANIC, "map_prom: mapping "
					    "conflict");
				}
				size -= MMU_PAGESIZE;
				offset += MMU_PAGESIZE;
				continue;
			}
			pp = PP2MACHPP(page_numtopp_nolock(pfn));
			if (pp != NULL && PP_ISFREE((page_t *)pp)) {
				cmn_err(CE_PANIC, "map_prom: prom page "
				    "on free list");
			}

			if (sfmmu_is_tsbaddr(vaddr)) {
				if (!pp && size >= MMU_PAGESIZE512K &&
				    !((uintptr_t)vaddr & MMU_PAGEOFFSET512K) &&
				    !(mmu_ptob(pfn) & MMU_PAGEOFFSET512K)) {
					sfmmu_memtte(ttep, pfn, attr, TTE512K);
					sfmmu_tteload(kas.a_hat, ttep,
					    vaddr, pp, flags);
					size -= MMU_PAGESIZE512K;
					offset += MMU_PAGESIZE512K;
					continue;
				}
				cmn_err(CE_PANIC, "Bad TSB addr %p\n",
				    (void *)vaddr);
			}

			sfmmu_memtte(ttep, pfn, attr, TTE8K);
			sfmmu_tteload(kas.a_hat, ttep, vaddr, pp, flags);
			size -= MMU_PAGESIZE;
			offset += MMU_PAGESIZE;
		}
	}
}

#undef COMBINE	/* local to previous routine */

/*
 * This routine reads in the "translations" property in to a buffer and
 * returns a pointer to this buffer and the number of translations.
 */
static struct translation *
read_prom_mappings(size_t *ntransrootp)
{
	char *prop = "translations";
	size_t translen;
	dnode_t node;
	struct translation *transroot;

	/*
	 * the "translations" property is associated with the mmu node
	 */
	node = (dnode_t)prom_getphandle(prom_mmu_ihandle());

	/*
	 * We use the TSB space to read in the prom mappings.  This space
	 * is currently not being used because we haven't taken over the
	 * trap table yet.  It should be big enough to hold the mappings.
	 */
	if ((translen = prom_getproplen(node, prop)) == -1)
		cmn_err(CE_PANIC, "no translations property");
	*ntransrootp = translen / sizeof (*transroot);
	translen = roundup(translen, MMU_PAGESIZE);
	PRM_DEBUG(translen);
	if (translen > TSB_BYTES(ktsb_szcode))
		cmn_err(CE_PANIC, "not enough space for translations");

	transroot = (struct translation *)ktsb_base;
	ASSERT(transroot);
	if (prom_getprop(node, prop, (caddr_t)transroot) == -1) {
		cmn_err(CE_PANIC, "translations getprop failed");
	}
	return (transroot);
}


/*
 * Allocate hat structs from the nucleus data memory.
 */
caddr_t
ndata_alloc_hat(caddr_t hat_alloc_base, caddr_t nalloc_end,
	pgcnt_t npages, long *more_hblks)
{
	size_t 	hmehash_sz, ctx_sz, tsb_bases_sz;
	int	thmehash_num;
	caddr_t	wanted_endva, nextra_base, nextra_end;
	size_t	nextra_size, wanted_hblksz;
	long	wanted_hblks, max_hblks;
	int	max_uhme_buckets = MAX_UHME_BUCKETS;
	int	max_khme_buckets = MAX_KHME_BUCKETS;

	PRM_DEBUG(npages);
	/*
	 * If the total amount of freemem is less than 32mb then share
	 * the kernel tsb so more memory is available to the user.
	 * We allocate 1 512k TSB for every 32mb.
	 */
	if (npages <= TSB_FREEMEM_MIN) {
		/* Share the kernel tsb */
		tsb512k_num = 0;
		ktsb_szcode = TSB_128K_SZCODE;
		enable_bigktsb = 0;
	} else {
		tsb512k_num = npages / TSB_FREEMEM_MIN;
		if (npages <= TSB_FREEMEM_LARGE / 2) {
			ktsb_szcode = TSB_256K_SZCODE;
			enable_bigktsb = 0;
		} else if (npages <= TSB_FREEMEM_LARGE) {
			ktsb_szcode = TSB_512K_SZCODE;
			enable_bigktsb = 0;
		} else if (npages <= TSB_FREEMEM_LARGE * 2 ||
		    enable_bigktsb == 0) {
			ktsb_szcode = TSB_1MB_SZCODE;
			enable_bigktsb = 0;
		} else {
			ktsb_szcode = highbit(npages - 1);
			ktsb_szcode -= TSB_START_SIZE;
			ktsb_szcode = MAX(ktsb_szcode, MIN_BIGKTSB_SZCODE);
			ktsb_szcode = MIN(ktsb_szcode, MAX_BIGKTSB_SZCODE);
		}
	}

	/* check if we exceed the max. */
	if (tsb512k_num > max_utsb) {
		tsb512k_num = max_utsb;
	}

	/*
	 * Compute number of smallest user tsbs.
	 * If we are sharing 1 tsb between user and kernel
	 * we must make sure to disable support for user 4M
	 * tte's in the utsb. The trap handler assume that
	 * the small tsbs are contained inside a large 512k
	 * tsb which is not true in the small memory configs
	 * that share a single tsb.
	 */
	if (tsb512k_num == 0) {
		tsb_num = 1;		/* share kernel tsb */
		utsb_4m_disable = 1;
	} else {
		tsb_num = tsb512k_num * TSB_SIZE_FACTOR;
	}

	ktsb_sz = TSB_BYTES(ktsb_szcode);	/* kernel tsb size */

	/*
	 * We first allocate the TSB by finding the first correctly
	 * aligned chunk of nucleus memory.
	 */
	if (enable_bigktsb == 0) {
		ktsb_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
		    ktsb_sz);
		ASSERT(!((uintptr_t)ktsb_base & (ktsb_sz - 1)));
		nextra_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
		    ecache_linesize);
		nextra_end = ktsb_base;
		nextra_size = nextra_end - nextra_base;
		hat_alloc_base = ktsb_base + ktsb_sz;
		PRM_DEBUG(ktsb_base);
		PRM_DEBUG(ktsb_sz);
		PRM_DEBUG(ktsb_szcode);
		PRM_DEBUG(nextra_base);
		PRM_DEBUG(nextra_size);
	} else {
		hat_alloc_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
		    ecache_linesize);
		nextra_base = hat_alloc_base;
		nextra_end = nextra_base;
		nextra_size = 0;
		ASSERT((max_uhme_buckets + max_khme_buckets) *
		    sizeof (struct hmehash_bucket) <=
			TSB_BYTES(TSB_1MB_SZCODE));
		max_uhme_buckets *= 2;
		max_khme_buckets *= 2;
	}

	ASSERT(hat_alloc_base < nalloc_end);

	/*
	 * Allocate page mapping list mutex array.
	 * For every 128 pages or 1Meg allocate 1 mutex,
	 * with a minimum of 64 entries. This doesn't have
	 * to be allocated from the nucleus.
	 */
	mml_table_sz = npages / 128;
	if (mml_table_sz < 64)
		mml_table_sz = 64;
	if (nextra_size >= (mml_table_sz * sizeof (kmutex_t))) {
		mml_table = (kmutex_t *)nextra_base;
		nextra_base += (mml_table_sz * sizeof (kmutex_t));
		nextra_base = (caddr_t)roundup((uintptr_t)nextra_base,
			ecache_linesize);
		nextra_size = nextra_end - nextra_base;
	} else {
		mml_table = (kmutex_t *)hat_alloc_base;
		hat_alloc_base += (mml_table_sz * sizeof (kmutex_t));
		hat_alloc_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
			ecache_linesize);
	}
	PRM_DEBUG(mml_table);

	ASSERT(hat_alloc_base < nalloc_end);
	/*
	 * Allocate tsb_base pool arrays
	 */
	tsb_bases_sz = (tsb512k_num + (tsb_num + 1)) *
		sizeof (struct tsb_info);

	if (nextra_size >= tsb_bases_sz) {
		tsb_bases = (struct tsb_info *)nextra_base;
		nextra_base += tsb_bases_sz;
		nextra_base = (caddr_t)roundup((uintptr_t)nextra_base,
			ecache_linesize);
		nextra_size = nextra_end - nextra_base;
	} else {
		tsb_bases = (struct tsb_info *)hat_alloc_base;
		hat_alloc_base += tsb_bases_sz;
		hat_alloc_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
			ecache_linesize);
	}
	tsb512k_bases = (tsb_bases + (tsb_num + 1));
	PRM_DEBUG(tsb_bases);
	PRM_DEBUG(tsb512k_bases);

	ASSERT(hat_alloc_base < nalloc_end);

	/*
	 * Allocate ctx structures
	 *
	 * based on v_proc to calculate how many ctx structures
	 * is not possible;
	 * use whatever module_setup() assigned to nctxs
	 */
	PRM_DEBUG(nctxs);
	ctx_sz = nctxs * sizeof (struct ctx);
	if (nextra_size >= ctx_sz) {
		ctxs = (struct ctx *)nextra_base;
		nextra_base += ctx_sz;
		nextra_base = (caddr_t)roundup((uintptr_t)nextra_base,
			ecache_linesize);
		nextra_size = nextra_end - nextra_base;
	} else {
		ctxs = (struct ctx *)hat_alloc_base;
		hat_alloc_base += ctx_sz;
		hat_alloc_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
			ecache_linesize);
	}
	PRM_DEBUG(ctxs);

	ASSERT(hat_alloc_base < nalloc_end);

	/*
	 * The number of buckets in the hme hash tables
	 * is a power of 2 such that the average hash chain length is
	 * HMENT_HASHAVELEN.  The number of buckets for the user hash is
	 * a function of physical memory and a predefined overmapping factor.
	 * The number of buckets for the kernel hash is a function of
	 * KERNELSIZE.
	 */
	uhmehash_num = (npages * HMEHASH_FACTOR) /
		(HMENT_HASHAVELEN * (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT));
	uhmehash_num = 1 << highbit(uhmehash_num - 1);
	uhmehash_num = MIN(uhmehash_num, max_uhme_buckets);
	khmehash_num = npages /
		(HMENT_HASHAVELEN * (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT));
	khmehash_num = MAX(MIN(khmehash_num, max_khme_buckets),
	    MIN_KHME_BUCKETS);
	khmehash_num = 1 << highbit(khmehash_num - 1);
	thmehash_num = uhmehash_num + khmehash_num;
	hmehash_sz = thmehash_num * sizeof (struct hmehash_bucket);
	if (nextra_size >= hmehash_sz) {
		khme_hash = (struct hmehash_bucket *)nextra_base;
		nextra_base += hmehash_sz;
		nextra_base = (caddr_t)roundup((uintptr_t)nextra_base,
			ecache_linesize);
		nextra_size = nextra_end - nextra_base;
	} else {
		khme_hash = (struct hmehash_bucket *)hat_alloc_base;
		hat_alloc_base += hmehash_sz;
		hat_alloc_base = (caddr_t)roundup((uintptr_t)hat_alloc_base,
		    ecache_linesize);
	}
	uhme_hash = (struct hmehash_bucket *)((caddr_t)khme_hash +
		khmehash_num * sizeof (struct hmehash_bucket));
	PRM_DEBUG(khme_hash);
	PRM_DEBUG(khmehash_num);
	PRM_DEBUG(uhme_hash);
	PRM_DEBUG(uhmehash_num);
	PRM_DEBUG(hmehash_sz);
	PRM_DEBUG(hat_alloc_base);
	ASSERT(hat_alloc_base < nalloc_end);

	/*
	 * Allocate nucleus hme_blks
	 * We only use hme_blks out of the nucleus pool when we are mapping
	 * other hme_blks.  The absolute worse case if we were to use all of
	 * physical memory for hme_blks so we allocate enough nucleus
	 * hme_blks to map all of physical memory.  This is real overkill
	 * so might want to divide it by a certain factor.
	 * RFE: notice that I will only allocate as many hmeblks as
	 * there is space in the nucleus.  We should add a check at the
	 * end of sfmmu_tteload to check how many "nucleus" hmeblks we have.
	 * If we go below a certain threshold we kmem alloc more.  The
	 * "nucleus" hmeblks need not be part of the nucleus.  They just
	 * need to be preallocated to avoid the recursion on kmem alloc'ed
	 * hmeblks.
	 * For platforms that may have huge amount of memory (64G), it
	 * will be incorrect to assume that whatever we can allocate from
	 * the nucleus will be enough for the initial coverage before
	 * kvm_init(). To guard against this scenario, we'll pass back
	 * the amount we did not allocate so that it can be bop_alloc'ed
	 * later.
	 */
#ifdef __sparcv9
	/*
	 * SYSLIMIT and KERNELBASE are no longer part of a contiguous virtual
	 * span in a 64-bit kernel.
	 */
	wanted_hblks = npages / (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT);
#else
	wanted_hblks = MIN(npages, mmu_btop(SYSLIMIT - KERNELBASE)) /
	    (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT);
#endif
	PRM_DEBUG(wanted_hblks);

	/*
	 * Use any unused space for hmeblks
	 */
	if (nextra_size >= HME8BLK_SZ) {
		wanted_hblks -= (nextra_size / HME8BLK_SZ);
		sfmmu_add_nucleus_hblks(nextra_base, nextra_size);
	}

	*more_hblks = MAX(0, wanted_hblks);
	if (wanted_hblks > 0) {
		max_hblks = ((uintptr_t)nalloc_end -
		    (uintptr_t)hat_alloc_base) / HME8BLK_SZ;
		wanted_hblks = MIN(wanted_hblks, max_hblks);
		*more_hblks -= wanted_hblks;
		PRM_DEBUG(wanted_hblks);
		wanted_hblksz = wanted_hblks * HME8BLK_SZ;
		wanted_endva = (caddr_t)roundup((uintptr_t)hat_alloc_base +
		    wanted_hblksz, MMU_PAGESIZE);
		wanted_hblksz = wanted_endva - hat_alloc_base;
		sfmmu_add_nucleus_hblks(hat_alloc_base, wanted_hblksz);
		PRM_DEBUG(wanted_hblksz);
		hat_alloc_base += wanted_hblksz;
		ASSERT(!((uintptr_t)hat_alloc_base & MMU_PAGEOFFSET));
	}

	ASSERT(hat_alloc_base <= nalloc_end);
	PRM_DEBUG(hat_alloc_base);
	PRM_DEBUG(HME8BLK_SZ);
	return (hat_alloc_base);
}

/*
 * Initial hblks allocation in the nucleus was not enough.
 * We need to bop_alloc more to meet the needs before
 * kvm_init() is done.
 */
caddr_t
alloc_more_hblks(caddr_t hat_alloc_base, long more_hblks)
{
	size_t more_hblksz = 0;
	caddr_t endva, vaddr;

	if (more_hblks <= 0)
		return (hat_alloc_base);

	more_hblksz = more_hblks * HME8BLK_SZ;
	endva = (caddr_t)roundup((uintptr_t)hat_alloc_base +
	    more_hblksz, MMU_PAGESIZE);
	more_hblksz = endva - hat_alloc_base;

	/*
	 * allocate the physical memory for the extra hblks
	 */
	if ((vaddr = (caddr_t)BOP_ALLOC(bootops, hat_alloc_base,
	    more_hblksz, MMU_PAGESIZE)) == NULL)
		cmn_err(CE_PANIC, "Cannot bop_alloc more hblk space.");
	ASSERT(vaddr == hat_alloc_base);

	/*
	 * Now link these additional hblks onto the freelist,
	 * treating them as "nucleus" hblks. This just means
	 * that they are not associated with a kmem cache.
	 */
	sfmmu_add_nucleus_hblks(hat_alloc_base, more_hblksz);
	PRM_DEBUG(more_hblksz);
	hat_alloc_base += more_hblksz;
	ASSERT(!((uintptr_t)hat_alloc_base & MMU_PAGEOFFSET));
	PRM_DEBUG(hat_alloc_base);
	return (hat_alloc_base);
}

/*
 * This function bop allocs a pool of tsbs.
 */
caddr_t
sfmmu_tsb_alloc(caddr_t tsbbase, pgcnt_t npages)
{
	size_t tsbsz;
	caddr_t vaddr;
	extern caddr_t tsballoc_base;

#ifdef lint
	npages = npages;
#endif

	if (enable_bigktsb) {
		ktsb_base = (caddr_t)roundup((uintptr_t)tsbbase, ktsb_sz);
		vaddr = (caddr_t)BOP_ALLOC(bootops, ktsb_base, ktsb_sz,
		    ktsb_sz);
		if (vaddr != ktsb_base)
			cmn_err(CE_PANIC, "sfmmu_tsb_alloc: can't alloc"
			    " bigktsb");
		ktsb_base = vaddr;
		tsbbase = ktsb_base + ktsb_sz;
	}

	if (tsb512k_num) {
		tsbsz = TSB_BYTES(TSB_512K_SZCODE);
		tsballoc_base = (caddr_t)roundup((uintptr_t)tsbbase, tsbsz);
		if ((vaddr = (caddr_t)BOP_ALLOC(bootops, tsballoc_base,
		    tsbsz * tsb512k_num, tsbsz)) == NULL) {
			cmn_err(CE_PANIC, "sfmmu_tsb_alloc: can't alloc tsbs");
		}
		ASSERT(vaddr == tsballoc_base);
		return (tsballoc_base + (tsbsz * tsb512k_num));
	} else {
		/*
		 * Share kernel tsb.
		 */
		tsballoc_base = (caddr_t)ktsb_base;
		return (tsbbase);
	}
}

/*
 * XXX: The prom doesn't support large pages. When this is fixed, we can
 * eliminate this function.
 */
static int
sfmmu_is_tsbaddr(caddr_t vaddr)
{
	int tsbsz;

	tsbsz = TSB_BYTES(TSB_512K_SZCODE) * tsb512k_num;
	return (vaddr >= tsballoc_base && vaddr < (tsballoc_base + tsbsz));
}
