/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * adopted from ../sun4m/vm/mach_srmmu.c
 */

#pragma ident	"@(#)cpr_sun4m.c 1.33	98/07/21 SMI"

#include <vm/hat_srmmu.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>

static void cpr_adjust_ptp0(u_int opa, u_int ptp);
static void cpr_adjust_ptp1(u_int pa, u_int opa, u_int ptp);
static void cpr_adjust_ptp2(u_int va, u_int pa, u_int opa, u_int ptp);
static void cpr_relocate_ptp1(union ptpe *rp0, u_int pa0);
static void cpr_relocate_ptp2(union ptpe *rp1, u_int pa1, u_int va1);
static void cpr_relocate_ptp3(union ptpe *rp2, u_int pa2, u_int va2);
static void cpr_move_ptpe(u_int *, u_int, u_int *);

extern void level15(int, int, int);
extern int srmmu_mmu_getctp(void);
extern int cpr_kpage_is_free(int);
extern int ldphys(int);
extern void prom_unmap(caddr_t, u_int);
extern int cpr_relocate_page(u_int, u_int, u_int);
extern void stphys(int, int);
extern int srmmu_mmu_probe(int);
extern u_int getpsr(void);


/*
 * most of these are defined in ../srmmu/vm/hat_srmmu.c
 */

#if !defined(lint)
struct scb *mon_tbr;	/* storage for %tbr's */
#endif /* lint */

static u_int ptpcnt;	/* for debug only */

extern u_int ptva;	/* XXXX: used to map in the page table */

extern void srmmu_mmu_flushall();

/*
 * go through the entire prom page table,relocate any page that might
 * conflict with the incoming kernel pages
 */
void
cpr_relocate_page_tables()
{
	u_int pa0;
	register union ptpe ct;
	union ptpe rp0;

	DEBUG4(errp("*** relocating page tables\n"));

	/*
	 * fetch prom's root pointer
	 */
	ct.ptpe_int = srmmu_mmu_getctp();
	pa0 = ct.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
	DEBUG4(errp("CTX pages: pa=%x\n", pa0));

	if (!cpr_kpage_is_free(ADDR_TO_PN(pa0))) {
		errp("**** ERROR: CTX PT is used pa=%x\n", pa0);
		cpr_move_ptpe(&pa0, pa0, &(rp0.ptpe_int));
	}

	rp0.ptpe_int = ldphys(pa0);

	cpr_relocate_ptp1(&rp0, pa0);
	srmmu_mmu_flushall();
	DEBUG4(errp("cpr_relocate_page_tables: %d pages relocated\n", ptpcnt));
}

static void
cpr_relocate_ptp1(union ptpe *rp0, u_int pa0)
{
	u_int pa1, va1;
	union ptpe rp1;
	u_int i;

	/*
	 * read in the level one page table pointer
	 */
	pa1 = rp0->ptp.PageTablePointer << MMU_STD_PTPSHIFT;

	if (!cpr_kpage_is_free(ADDR_TO_PN(pa1))) {
		DEBUG4(errp("L1PT used pa0=%x ptp=%x\n", pa0, rp0->ptpe_int));
		cpr_move_ptpe(&pa1, pa0, &(rp0->ptpe_int));
		ptpcnt++;
	}

	for (i = 0; i < NL1PTEPERPT; i++, pa1 += sizeof (struct ptp)) {

		rp1.ptpe_int = ldphys(pa1);

		va1 = MMU_L1_VA(i);

		if (rp1.ptp.EntryType ==  MMU_ET_PTP)
			cpr_relocate_ptp2(&rp1, pa1, va1);
	}
}

static void
cpr_relocate_ptp2(union ptpe *rp1, u_int pa1, u_int va1)
{
	u_int pa2, va2;
	union ptpe rp2;
	u_int i;

	pa2 = rp1->ptp.PageTablePointer << MMU_STD_PTPSHIFT;

	if (!cpr_kpage_is_free(ADDR_TO_PN(pa2))) {
		DEBUG4(errp("\tL2PT used va=%x pa1=%x pa2=%x ptp=%x\n",
			va1, pa1, pa2, rp1->ptpe_int));
		cpr_move_ptpe(&pa2, pa1, &(rp1->ptpe_int));
		ptpcnt++;
	}

	for (i = 0; i < NL2PTEPERPT; i++, pa2 += sizeof (struct ptp)) {

		rp2.ptpe_int = ldphys(pa2);
		va2 = va1 + MMU_L2_VA(i);

		if (rp2.ptp.EntryType == MMU_ET_PTP)
			cpr_relocate_ptp3(&rp2, pa2, va2);
	}
}

static void
cpr_relocate_ptp3(union ptpe *rp2, u_int pa2, u_int va2)
{
	u_int pa3;

	pa3 = rp2->ptp.PageTablePointer << MMU_STD_PTPSHIFT;
	if (!cpr_kpage_is_free(ADDR_TO_PN(pa3))) {
		DEBUG4(errp("\t\tL3PT used va=%x pa2=%x ptp=%x\n",
			va2, pa2, rp2->ptpe_int));
		cpr_move_ptpe(&pa3, pa2, &(rp2->ptpe_int));
		ptpcnt++;
	}
}

/*
 * move a physical page containing the MMU page table to
 * a free page not occupied by the incoming kernel, and
 * adjust all references to this entry.
 */
static void
cpr_move_ptpe(u_int *opap, u_int ptepa, u_int *ptpp)
{
	register u_int ptp, opa;
	u_int pa, offset;
	u_int pfn;

	opa = *opap;	/* use reg, faster access */
	offset = MMU_L3_OFF(opa);
	prom_unmap((caddr_t)ptva, MMU_PAGESIZE);
	if (prom_map((caddr_t)ptva, 0,
	    (opa & 0xfffff000), MMU_PAGESIZE)  == 0) {
		errp("cpr_move_ptpe: prom_map failed. Please Reboot.\n");
		prom_exit_to_mon();
	}
	pfn = cpr_relocate_page(ADDR_TO_PN(opa), ptepa, ptva);

	if (pfn == (int)-1)
		errp("cpr_move_ptpe: CAN'T relcoate page %x\n",
			ADDR_TO_PN(opa));

	pa = PN_TO_ADDR(pfn);

	if (pfn == ADDR_TO_PN(opa)) {
		errp("cpr_move_ptpe: failed to find a new pa for pa %x\n",
			opa);
		return;
	}

	*opap = pa + offset;	/* so back there we have the right pa */
	ptp = (pa >> PATOPTP_SHIFT);
	*ptpp = (*ptpp & 0xff) | ptp;
	stphys(ptepa, *ptpp);

	DEBUG4(errp("**  aft remap ptp=%x ptpp=%x pa=%x\n", ptp, *ptpp, pa));
	cpr_adjust_ptp0(opa, ptp);
}

static void
cpr_adjust_ptp0(u_int opa, u_int ptp)
{
	union ptpe rp0, ct;
	u_int rp, pa0, pa1;

	ct.ptpe_int = srmmu_mmu_getctp();
	pa0 = ct.ptp.PageTablePointer << MMU_STD_PTPSHIFT;

	rp0.ptpe_int = ldphys(pa0);

	pa1 = rp0.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
	if (ADDR_TO_PN(pa1) == ADDR_TO_PN(opa)) {
		rp = (rp0.ptpe_int & 0xff) | ptp;
		stphys(pa0, rp);

		DEBUG4(errp("   +++ L0 Adj: pa=%x ptp=%x\n", pa1, rp));
	}
	cpr_adjust_ptp1(pa1, opa, ptp);
}

static void
cpr_adjust_ptp1(u_int pa1, u_int opa, u_int ptp)
{
	union ptpe rp1;
	u_int rp, va1, pa2;
	int i;

	for (i = 0; i < NL1PTEPERPT; i++, pa1 += sizeof (struct ptp)) {

		rp1.ptpe_int = ldphys(pa1);

		va1 = MMU_L1_VA(i);
		pa2 = rp1.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
		if (ADDR_TO_PN(pa2) == ADDR_TO_PN(opa)) {
			rp = (rp1.ptpe_int & 0xff) | ptp;
			stphys(pa1, rp);
			DEBUG4(errp("L1 Adj: va=%x pa=%x ptp=%x\n",
				va1, pa1, rp));
		}

		if (rp1.ptp.EntryType ==  MMU_ET_PTP)
			cpr_adjust_ptp2(va1, pa2, opa, ptp);
	}
}

static void
cpr_adjust_ptp2(u_int va1, u_int pa2, u_int opa, u_int ptp)
{
	union ptpe rp2;
	u_int rp, va2, pa3;
	int i;

	for (i = 0; i < NL2PTEPERPT; i++, pa2 += sizeof (struct ptp)) {

		rp2.ptpe_int = ldphys(pa2);

		va2 = va1 + MMU_L2_VA(i);
		pa3 = rp2.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
		if (ADDR_TO_PN(pa3) == ADDR_TO_PN(opa)) {
			rp = (rp2.ptpe_int & 0xff) | ptp;
			stphys(pa2, rp);
			DEBUG4(errp("L2 Adj: va=%x pa=%x ptp=%x\n",
				va2, pa2, rp));
		}
	}
}

/*
 * Returns a pointer to the pte struct for the given virtual address.
 * If the necessary page tables do not exist, return NULL.
 *
 * REF: Code adopted from srmmu/vm/hat_srmmu.c
 */
struct pte *
cpr_srmmu_ptefind(register caddr_t va)
{
	union ptpe ptp;
	u_int pa;

	ptp.ptpe_int = srmmu_mmu_getctp();

	pa = ptp.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
	ptp.ptpe_int = ldphys(pa);

	pa = (ptp.ptp.PageTablePointer << MMU_STD_PTPSHIFT) +
		(MMU_L1_INDEX(va) << 2);
	ptp.ptpe_int = ldphys(pa);

	switch (ptp.ptp.EntryType) {
	case MMU_ET_INVALID:
	case MMU_ET_PTE:
		return ((struct pte *)-1);

	case MMU_ET_PTP:
		break;

	default:
		prom_printf("srmmu_ptefind1");
	}

	pa = (ptp.ptp.PageTablePointer << MMU_STD_PTPSHIFT) +
		(MMU_L2_INDEX(va) << 2);
	ptp.ptpe_int = ldphys(pa);

	switch (ptp.ptp.EntryType) {
	case MMU_ET_INVALID:
	case MMU_ET_PTE:
		return ((struct pte *)-1);

	case MMU_ET_PTP:
		break;

	default:
		prom_printf("srmmu_ptefind2");
	}

	/*
	 * Return the address of the level 3 entry.
	 */
	pa = (ptp.ptp.PageTablePointer << MMU_STD_PTPSHIFT) +
		(MMU_L3_INDEX(va) << 2);

	return ((struct pte *)(pa));
}

u_int
cpr_va_to_pfn(u_int va)
{
	union ptpe pt;

	pt.ptpe_int = srmmu_mmu_probe(va);
	if (pt.ptpe_int != 0 && pt.pte.EntryType != MMU_ET_INVALID)
		return (pt.pte.PhysicalPageNumber);
	else
		return (PFN_INVALID);
}

uint64_t
va_to_pa(void *va)
{
	pfn_t pfn;

	if ((pfn = cpr_va_to_pfn((uintptr_t)va)) != PFN_INVALID)
		return (PN_TO_ADDR(pfn) | ((uintptr_t)va & MMU_PAGEOFFSET));
	else
		return ((uint64_t)-1);
}

#if !defined(lint)
/*
 * Sys_trap trap handlers.
 *
 * level15 (memory error) interrupt.
 */
/* ARGSUSED */
void
level15(int trap, int trappc, int trapnpc)
{
	errp("memory error\n");
}

/*
 * miscellaneous fault handler
 */
/* ARGSUSED */
void
fault(register int trap, register int trappc, register int trapnpc)
{
	/*
	 * XXXX: when encountering an error we should abort resume
	 *	   and pop back to boot and reboot normally
	 */
	errp("fault: type=%d  pc=%x\n", trap, trappc);
	prom_enter_mon();
}

#endif	/* lint */
