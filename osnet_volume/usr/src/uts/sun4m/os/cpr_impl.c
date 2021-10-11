/*
 * Copyright (c) 1992-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_impl.c	1.74	99/10/19 SMI"

/*
 * Platform specific implementation code
 */

#include <sys/types.h>
#include <sys/promif.h>
#include <vm/hat.h>
#include <sys/mmu.h>
#include <sys/iommu.h>
#include <sys/scb.h>
#include <sys/cpuvar.h>
#include <sys/cpu.h>
#include <sys/intreg.h>
#include <sys/pte.h>
#include <sys/clock.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <vm/seg.h>
#include <vm/mach_srmmu.h>
#include <vm/hat_srmmu.h>
#include <vm/seg_kmem.h>
#include <sys/cpr.h>
#include <sys/vmem.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <sys/callb.h>
#include <sys/panic.h>

extern int cprboot_magic;
extern int i_ddi_splaudio(void);
extern int spl0(void);
extern int vac;
extern int xc_level_ignore;
extern struct vnode prom_ppages;
extern u_int cpr_kas_page_cnt;
extern void start_other_cpus(int);

u_short cpr_mach_type = CPR_MACHTYPE_4M;
static csm_md_t m_info;


/*
 * Stop real time clock and all interrupt activities in the system
 */
void
i_cpr_stop_intr()
{
	(void) splzs(); /* block all network traffic and everything */
}

/*
 * Set machine up to take interrupts
 */
void
i_cpr_enable_intr()
{
	set_intmask(IR_ENA_INT, 0);

	(void) spl0();			/* allow all interrupts */
}

/*
 * Write necessary machine dependent information to cpr state file,
 * eg. sun4m pte table ...
 */
int
i_cpr_write_machdep(vnode_t *vp)
{
	cmd_t cmach;
	int rc;

	cmach.md_magic = (u_int) CPR_MACHDEP_MAGIC;
	cmach.md_size = sizeof (m_info);

	if (rc = cpr_write(vp, (caddr_t)&cmach, sizeof (cmd_t))) {
		errp("cpr_write_machdep: Failed to write desc\n");
		return (rc);
	}

	m_info.func = (u_int)i_cpr_resume_setup;
	m_info.func_pfn = (u_int)va_to_pfn((caddr_t)m_info.func);

	m_info.thrp = (u_int)curthread;
	m_info.thrp_pfn = (u_int)va_to_pfn((caddr_t)m_info.thrp);

	m_info.qsav = (u_int)&ttolwp(curthread)->lwp_qsav;
	m_info.qsav_pfn = (u_int)va_to_pfn((caddr_t)m_info.qsav);

	/*
	 * Get info for current mmu ctx and l1 ptp pointer and
	 * write them out to the state file.
	 * Pack them into one buf and do 1 write.
	 */
	m_info.mmu_ctp = mmu_getctp();
	m_info.mmu_ctx = mmu_getctx();
	m_info.mmu_ctl = mmu_getcr();

	rc = cpr_write(vp, (caddr_t)&m_info, sizeof (m_info));

	return (rc);
}

void
i_cpr_save_machdep_info()
{
}

/*
 * Initial setup to get the system back to an operable state.
 * 	1. Restore IOMMU
 */
void
i_cpr_machdep_setup()
{
	union iommu_ctl_reg ioctl_reg;
	union iommu_base_reg iobase_reg;
	extern iommu_pte_t *phys_iopte;
	cpu_t *cp;

	if (cache & CACHE_VAC) {
		if (use_cache) {
			cache_init();
			turn_cache_on(getprocessorid());
		}
	}

	/* load iommu base addr */
	iobase_reg.base_uint = 0;
	iobase_reg.base_reg.base = ((u_int)phys_iopte) >> IOMMU_PTE_BASE_SHIFT;
	iommu_set_base(iobase_reg.base_uint);

	/* set control reg and turn it on */
	ioctl_reg.ctl_uint = 0;
	ioctl_reg.ctl_reg.enable = 1;
	ioctl_reg.ctl_reg.range = IOMMU_CTL_RANGE;
	iommu_set_ctl(ioctl_reg.ctl_uint);

	DEBUG2(errp("IOMMU:iobs_reg.base_reg.base %x "
			"ioctl_reg.ctl_reg.range %x\n",
			iobase_reg.base_reg.base, ioctl_reg.ctl_reg.range));

	/* kindly flush all tlbs for any leftovers */
	iommu_flush_all();

	/*
	 * Install PROM callback handler (give both, promlib picks the
	 * appropriate handler).
	 */
	if (prom_sethandler(NULL, vx_handler) != 0)
		cmn_err(CE_PANIC, "CPR cannot re-register sync callback");

	DEBUG2(errp("i_cpr_machdep_setup: Set up IOMMU\n"));

	(void) callb_execute_class(CB_CL_CPR_OBP, CB_CODE_CPR_RESUME);

	/*
	 * Note: On sun4m we MUST perform all of the above machine-
	 * dependent initialization before bringing up the other
	 * cpus.
	 */
	if (ncpus > 1) {
		/*
		 * All of the non-boot cpus are not ready at this time,
		 * yet the cpu structures have various cpu_flags set;
		 * clear the cpu_flags to match the hw status, otherwise
		 * x_calls are not going to work when using VAC.
		 */
		for (cp = CPU->cpu_next; cp != CPU; cp = cp->cpu_next)
			cp->cpu_flags = 0;
		start_other_cpus(1);
		DEBUG1(errp("MP started.\n"));
	}
}

u_int
i_cpr_va_to_pfn(caddr_t vaddr)
{
	return (va_to_pfn((caddr_t)vaddr));
}

void
i_cpr_set_tbr()
{
	(void) set_tbr(&scb);
}

/*
 * The bootcpu is always 0 on sun4m.
 */
i_cpr_find_bootcpu()
{
	return (0);
}


/*
 * XXX These should live in the cpr state struct
 * XXX as impl private void * thingies
 */
caddr_t cpr_vaddr = NULL;
struct pte *cpr_pte = NULL;

/*
 * Return the virtual address of the mapping area
 */
caddr_t
i_cpr_map_setup(void)
{
	caddr_t vaddr, end;

	/*
	 * Allocate a chunk of kernel virtual that spans exactly one l3 pte.
	 */
	vaddr = vmem_xalloc(heap_arena, mmu_ptob(NL3PTEPERPT),
	    mmu_ptob(NL3PTEPERPT), 0, 0, NULL, NULL, VM_NOSLEEP);
	if (vaddr == NULL)
		return (NULL);
	end = vaddr + mmu_ptob(NL3PTEPERPT);

	/*
	 * Now get ptes for the range, mark each invalid
	 */
	while (vaddr < end) {
		struct pte *pte;
		union ptes *ptep;
		kmutex_t *mtx;
		ptbl_t *ptbl;
		extern struct pte *srmmu_ptealloc(struct as *, caddr_t,
		    int, ptbl_t **, kmutex_t **, int);
#ifdef	DEBUG
		struct pte *prev_pte;
		struct ptbl *prev_ptbl;
		/*LINTED*/
		prev_pte = pte;		/* used before set, but value ignored */
		/*LINTED*/
		prev_ptbl = ptbl;	/* used before set, but value ignored */
#endif
		pte = srmmu_ptealloc(&kas, vaddr, 3, &ptbl, &mtx, 0);
		ptep = (union ptes *)pte;
		ptep->pte_int = MMU_ET_INVALID;

		if (cpr_vaddr == NULL) {
			cpr_vaddr = vaddr;
			cpr_pte = pte;
#ifdef	DEBUG
			prev_ptbl = ptbl;
#endif
		} else {
			/*EMPTY*/
			ASSERT(pte == prev_pte + 1);
			ASSERT(ptbl == prev_ptbl);
		}
		ASSERT(ptbl->ptbl_flags & PTBL_KEEP);
		ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == 3);

		unlock_ptbl(ptbl, mtx);
		vaddr += MMU_PAGESIZE;
	}
	return (cpr_vaddr);
}

/*
 * Map pages into the kernel's address space at the  location computed
 * by i_cpr_map_init above
 * We have already allocated a ptbl, a pte and a piece of kernel heap.
 * All we need to do is to plug in the new mapping and deal with
 * the TLB and vac.
 */

void
i_cpr_mapin(caddr_t vaddr, u_int len, pfn_t pf)
{
	struct pte *pte = cpr_pte;
	union ptpe rp;
	register int i;

	ASSERT(cpr_pte != NULL);
	ASSERT(vaddr == cpr_vaddr);
	ASSERT(len <= NL3PTEPERPT);

#ifdef	VAC
	if (vac) {
		XCALL_PROLOG
		vac_allflush(FL_CACHE);
		XCALL_EPILOG
		/*
		 * Flush the value of "xc_mbox_lock" that we just released.
		 * Bug 1191279
		 */
		vac_ctxflush(0, FL_LOCALCPU);
	}
#endif
	for (i = 0; i < len; i++, pf++, pte++, vaddr += MMU_PAGESIZE) {
		/*
		 * XXX tlbflush probably only is  useful on unmapping
		 */
		srmmu_tlbflush(3, vaddr, KCONTEXT, FL_LOCALCPU);
		rp.ptpe_int = PTEOF(0, pf, MMU_STD_SRX, 1);
		*pte = rp.pte;
	}
}

void
i_cpr_mapout(caddr_t vaddr, u_int len)
{
	struct pte *pte = cpr_pte;
	union ptpe rp;
	u_int pf = pte->PhysicalPageNumber;
	register int i;

	ASSERT(vaddr == cpr_vaddr);
	ASSERT(len <= NL3PTEPERPT);

	/*
	 * It is a bit excessive to invalidate the pte given that we will
	 * do this when we're all done, but it does at least give us
	 * a consistent red zone effect
	 */
	for (i = 0; i < len; i++, pf++, pte++, vaddr += MMU_PAGESIZE) {
		ASSERT(pte->PhysicalPageNumber == pf);
		rp.ptpe_int = MMU_ET_INVALID;
		*pte = rp.pte;
		srmmu_tlbflush(3, vaddr, KCONTEXT, FL_LOCALCPU);
	}
}

/*
 * We're done using the mapping area, clean it up and give it back
 * We re-invalidate all the l3 ptes, since the page they live in will
 * have been copied out while they were in use, and the kernel will panic
 * if it finds a valid entry when it tries to reuse it after we free
 * up the kernel virtual space that corresponds to it
 */
void
i_cpr_map_destroy(void)
{
	struct pte *pte = cpr_pte;
	union ptpe rp;
	caddr_t vaddr = cpr_vaddr;
	register int i;

	for (i = 0; i < NL3PTEPERPT; i++, pte++, vaddr += MMU_PAGESIZE) {
		rp.ptpe_int = MMU_ET_INVALID;
		*pte = rp.pte;
		srmmu_tlbflush(3, vaddr, KCONTEXT, FL_LOCALCPU);
	}
	/*
	 * It looks like we don't have to do anything about the ptbl,
	 * it will just get reused when needed
	 */
	vmem_free(heap_arena, cpr_vaddr, mmu_ptob(NL3PTEPERPT));
	cpr_vaddr = NULL;
	cpr_pte = NULL;
}

void
i_cpr_handle_xc(u_int flag)
{
	xc_level_ignore = flag;
}

pgcnt_t
i_cpr_count_special_kpages(char *bitmap, bitfunc_t bitfunc)
{
	struct page *pp;
	pfn_t pfn;
	pgcnt_t tot_pages = 0;

	/*
	 * Hack: page 2 & 3 (msgbuf kluge) are not counted as kas in
	 * startup, so we need to manually tag them.
	 */
	if (bitfunc == cpr_setbit) {
		if ((*bitfunc)(2, bitmap) == 0 &&
		    (*bitfunc)(3, bitmap) == 0)
			tot_pages += 2;
	} else
		tot_pages += 2;

	/*
	 * prom allocated kernel mem is hardcoded into prom_ppages vnode.
	 */
	pp = prom_ppages.v_pages;
	while (pp) {
		pfn = ((machpage_t *)pp)->p_pagenum;
		if (pfn != (u_int)-1 && pf_is_memory(pfn)) {
			if (bitfunc == cpr_setbit) {
				if ((*bitfunc)(pfn, bitmap) == 0)
					tot_pages++;
			} else
				tot_pages++;
		}
		if ((pp = pp->p_vpnext) == prom_ppages.v_pages)
			break;
	}

	return (tot_pages);
}

/*
 * Free up memory-related resources here.
 */
void
i_cpr_free_memory_resources(void)
{
	i_cpr_map_destroy();
}

/*ARGSUSED*/
pgcnt_t
i_cpr_count_sensitive_kpages(char *bitmap, bitfunc_t bitfunc)
{
	return (0);
}

/*ARGSUSED*/
int
i_cpr_compress_and_save(int chunks, pfn_t spfn, pgcnt_t pages)
{
	return (0);
}

int
i_cpr_save_sensitive_kpages(void)
{
	return (0);
}

/*ARGSUSED*/
int
i_cpr_dump_sensitive_kpages(vnode_t *vp)
{
	return (0);
}

/*ARGSUSED*/
pgcnt_t
i_cpr_count_storage_pages(char *bitmap, bitfunc_t bitfunc)
{
	return (0);
}

int
i_cpr_check_pgs_dumped(u_int pgs_expected, u_int regular_pgs_dumped)
{
	DEBUG7(errp("cpr_pgs_tobe_dumped: 0x%x, regular_pgs_dumped: 0x%x\n",
	    pgs_expected, regular_pgs_dumped));

	if (pgs_expected == regular_pgs_dumped)
		return (0);

	return (EINVAL);
}

int
i_cpr_reuseinit(void)
{
	return (ENOTSUP);
}

int
i_cpr_reusefini(void)
{
	return (ENOTSUP);
}

int
i_cpr_reusable_supported(void)
{
	return (0);
}

i_cpr_check_cprinfo()
{
	ASSERT(0);
	return (ENOTSUP);
}

/* ARGSUSED */
int
i_cpr_prom_pages(int action)
{
	return (0);
}


/*
 * set dump_reserve to indicate a series of VOP_DUMP() requests;
 * a leading VOP_DUMP() is called to alloc dvma space using private
 * dma attributes passed in by the HBA.  the leading write data is
 * unimportant and will be overwritten by later VOP_DUMP().
 *
 * since UFS partitions VOP_DUMP() requests at non-contig boundaries,
 * VOP_DUMPCTL() is needed to find the start of contig fs space;
 * the returned blkno is used for the leading VOP_DUMP() to ensure
 * the size of the dvma alloc.
 *
 * note: if it were simpler to access HBAs dma attributes,
 * direct dvma alloc and free could be done instead.
 */
int
i_cpr_dump_setup(vnode_t *vp)
{
	extern size_t cpr_buf_size, dump_reserve;
	extern u_char *cpr_buf;
	int blkno, dblks, err;
	size_t size;

	size = mmu_ptob(cpr_buf_size);
	blkno = dblks = btodb(size);
	if (err = VOP_DUMPCTL(vp, DUMP_SCAN, &blkno))
		return (err);

	dump_reserve = size;
	do_polled_io = 1;
	err = VOP_DUMP(vp, (caddr_t)cpr_buf, blkno, dblks);
	do_polled_io = 0;

	return (err);
}


int
i_cpr_is_supported(void)
{
	return (1);
}


/* ARGSUSED */
int
i_cpr_blockzero(u_char *base, u_char **bufpp, int *blkno, vnode_t *vp)
{
	if ((cpr_debug & LEVEL1) && blkno)
		errp("statefile data size: %ld\n\n", dbtob(*blkno));
	return (0);
}
