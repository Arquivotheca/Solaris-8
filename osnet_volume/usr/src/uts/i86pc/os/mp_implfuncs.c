/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mp_implfuncs.c	1.24	99/04/14 SMI"

#include <sys/psm.h>
#include <sys/vmem.h>
#include <vm/hat.h>
#include <sys/modctl.h>
#include <vm/seg_kmem.h>
#define	PSMI_1_2
#include <sys/psm.h>
#include <sys/psm_modctl.h>
#include <sys/smp_impldefs.h>
#include <sys/reboot.h>

/*
 *	External reference functions
 */
extern void *get_next_mach(void *, char *);
extern void close_mach_list(void);
extern void open_mach_list(void);

int psm_add_intr(int lvl, avfunc xxintr, char *name, int vect, caddr_t arg);
int psm_add_nmintr(int lvl, avfunc xxintr, char *name, caddr_t arg);
processorid_t psm_get_cpu_id(void);
void psm_unmap(caddr_t addr, ulong len, ulong prot);
caddr_t psm_map(paddr_t addr, ulong len, ulong prot);
void psm_unmap_phys(caddr_t addr, long len);
caddr_t psm_map_phys(paddr_t addr, long len, u_long prot);
int psm_mod_init(void **modlpp, struct psm_info *infop);
int psm_mod_fini(void **modlpp, struct psm_info *infop);
int psm_mod_info(void **modlpp, struct psm_info *infop,
	struct modinfo *modinfop);
void psm_modloadonly(void);
void psm_install(void);

/*
 * Local Function Prototypes
 */
static struct modlinkage *psm_modlinkage_alloc(struct psm_info *infop);
static void psm_modlinkage_free(struct modlinkage *mlinkp);
static void psm_direct_unmap_phys(paddr_t addr, ulong len);
static void psm_direct_map_phys(paddr_t addr, ulong len, ulong prot);

static char *psm_get_impl_module(int first);

static int mod_installpsm(struct modlpsm *modl, struct modlinkage *modlp);
static int mod_removepsm(struct modlpsm *modl, struct modlinkage *modlp);
static int mod_infopsm(struct modlpsm *modl, struct modlinkage *modlp, int *p0);
struct mod_ops mod_psmops = {
	mod_installpsm, mod_removepsm, mod_infopsm
};

static struct psm_sw psm_swtab = {
	&psm_swtab, &psm_swtab, NULL, NULL
};

kmutex_t psmsw_lock;			/* lock accesses to psmsw 	*/
struct psm_sw *psmsw = &psm_swtab; 	/* start of all psm_sw		*/

static struct modlinkage *
psm_modlinkage_alloc(struct psm_info *infop)
{
	int	memsz;
	struct modlinkage *mlinkp;
	struct modlpsm *mlpsmp;
	struct psm_sw *swp;

	memsz = sizeof (struct modlinkage) + sizeof (struct modlpsm) +
		sizeof (struct psm_sw);
	mlinkp = (struct modlinkage *)kmem_zalloc(memsz, KM_NOSLEEP);
	if (!mlinkp) {
		cmn_err(CE_WARN, "!psm_mod_init: Cannot install %s",
			infop->p_mach_idstring);
		return (NULL);
	}
	mlpsmp = (struct modlpsm *)(mlinkp + 1);
	swp = (struct psm_sw *)(mlpsmp + 1);

	mlinkp->ml_rev = MODREV_1;
	mlinkp->ml_linkage[0] = (void *)mlpsmp;
	mlinkp->ml_linkage[1] = (void *)NULL;

	mlpsmp->psm_modops = &mod_psmops;
	mlpsmp->psm_linkinfo = infop->p_mach_desc;
	mlpsmp->psm_swp = swp;

	swp->psw_infop = infop;

	return (mlinkp);
}

static void
psm_modlinkage_free(struct modlinkage *mlinkp)
{
	if (!mlinkp)
		return;

	(void) kmem_free(mlinkp, (sizeof (struct modlinkage) +
		sizeof (struct modlpsm) + sizeof (struct psm_sw)));
}

int
psm_mod_init(void **handlepp, struct psm_info *infop)
{
	struct modlinkage **modlpp = (struct modlinkage **)handlepp;
	int	status;
	struct modlinkage *mlinkp;

	if (!*modlpp) {
		mlinkp = psm_modlinkage_alloc(infop);
		if (!mlinkp)
			return (ENOSPC);
	} else
		mlinkp = *modlpp;

	status = mod_install(mlinkp);
	if (status) {
		psm_modlinkage_free(mlinkp);
		*modlpp = NULL;
	} else
		*modlpp = mlinkp;

	return (status);
}

/*ARGSUSED1*/
int
psm_mod_fini(void **handlepp, struct psm_info *infop)
{
	struct modlinkage **modlpp = (struct modlinkage **)handlepp;
	int	status;

	status = mod_remove(*modlpp);
	if (status == 0) {
		psm_modlinkage_free(*modlpp);
		*modlpp = NULL;
	}
	return (status);
}

int
psm_mod_info(void **handlepp, struct psm_info *infop, struct modinfo *modinfop)
{
	struct modlinkage **modlpp = (struct modlinkage **)handlepp;
	int status;
	struct modlinkage *mlinkp;

	if (!*modlpp) {
		mlinkp = psm_modlinkage_alloc(infop);
		if (!mlinkp)
			return ((int)NULL);
	} else
		mlinkp = *modlpp;

	status =  mod_info(mlinkp, modinfop);

	if (!status) {
		psm_modlinkage_free(mlinkp);
		*modlpp = NULL;
	} else
		*modlpp = mlinkp;

	return (status);
}

int
psm_add_intr(int lvl, avfunc xxintr, char *name, int vect, caddr_t arg)
{
	return (add_avintr((void *)NULL, lvl, xxintr, name, vect, arg));
}

int
psm_add_nmintr(int lvl, avfunc xxintr, char *name, caddr_t arg)
{
	return (add_nmintr(lvl, xxintr, name, arg));
}

processorid_t
psm_get_cpu_id(void)
{
	return (CPU->cpu_id);
}

caddr_t
psm_map(paddr_t addr, ulong len, ulong prot)
{
	if (prot == PSM_PROT_READ)
		return (psm_map_phys(addr, (long)len, PROT_READ));
	else if (prot == PSM_PROT_WRITE)
		return (psm_map_phys(addr, (long)len, PROT_READ|PROT_WRITE));
	else if (prot & PSM_PROT_SAMEADDR) {
		if (prot & PSM_PROT_WRITE)
			psm_direct_map_phys(addr, len, PROT_READ|PROT_WRITE);
		else
			psm_direct_map_phys(addr, len, PROT_READ);

		return ((caddr_t)addr);
	} else
		return ((caddr_t)NULL);
}

void
psm_unmap(caddr_t addr, ulong len, ulong prot)
{
	if ((prot == PSM_PROT_WRITE) || (prot == PSM_PROT_READ))
		psm_unmap_phys(addr, (long)len);
	else if (prot & PSM_PROT_SAMEADDR)
		psm_direct_unmap_phys((paddr_t)addr, len);
}

caddr_t
psm_map_phys(paddr_t addr, long len, u_long prot)
{
	uint_t pgoffset;
	uintptr_t base;
	pgcnt_t npages;
	caddr_t cvaddr;

	if (len == 0)
		return (0);

	pgoffset = (uintptr_t)addr & MMU_PAGEOFFSET;
	base = (uintptr_t)addr - pgoffset;
	npages = mmu_btopr(len + pgoffset);
	cvaddr = vmem_alloc(heap_arena, ptob(npages), VM_NOSLEEP);
	if (cvaddr == NULL)
		return (0);
	hat_devload(kas.a_hat, cvaddr, mmu_ptob(npages), mmu_btop(base),
	    prot, HAT_LOAD_LOCK);
	return (cvaddr + pgoffset);
}

void
psm_unmap_phys(caddr_t addr, long len)
{
	uint_t pgoffset;
	caddr_t base;
	u_int npages;

	if (len == 0)
		return;

	pgoffset = (uintptr_t)addr & MMU_PAGEOFFSET;
	base = addr - pgoffset;
	npages = mmu_btopr(len + pgoffset);
	hat_unload(kas.a_hat, base, ptob(npages), HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, base, ptob(npages));
}

static void
psm_direct_map_phys(paddr_t addr, ulong len, ulong prot)
{
	hat_devload(kas.a_hat, (caddr_t)addr, len,
		(addr>>12), prot|HAT_NOSYNC, HAT_LOAD_LOCK);
}

static void
psm_direct_unmap_phys(paddr_t addr, ulong len)
{
	hat_unload(kas.a_hat, (caddr_t)addr, len, NULL);
}

/*ARGSUSED1*/
static int
mod_installpsm(struct modlpsm *modl, struct modlinkage *modlp)
{
	struct psm_sw *swp;

	swp = modl->psm_swp;
	mutex_enter(&psmsw_lock);
	psmsw->psw_back->psw_forw = swp;
	swp->psw_back = psmsw->psw_back;
	swp->psw_forw = psmsw;
	psmsw->psw_back = swp;
	swp->psw_flag |= PSM_MOD_INSTALL;
	mutex_exit(&psmsw_lock);
	return (0);
}

/*ARGSUSED1*/
static int
mod_removepsm(struct modlpsm *modl, struct modlinkage *modlp)
{
	struct psm_sw *swp;

	swp = modl->psm_swp;
	mutex_enter(&psmsw_lock);
	if (swp->psw_flag & PSM_MOD_IDENTIFY) {
		mutex_exit(&psmsw_lock);
		return (EBUSY);
	}
	if (!(swp->psw_flag & PSM_MOD_INSTALL)) {
		mutex_exit(&psmsw_lock);
		return (0);
	}

	swp->psw_back->psw_forw = swp->psw_forw;
	swp->psw_forw->psw_back = swp->psw_back;
	mutex_exit(&psmsw_lock);
	return (0);
}

/*ARGSUSED1*/
static int
mod_infopsm(struct modlpsm *modl, struct modlinkage *modlp, int *p0)
{
	*p0 = (int)modl->psm_swp->psw_infop->p_owner;
	return (0);
}

static char *
psm_get_impl_module(int first)
{
	static char **pnamep;
	static char *psm_impl_module_list[] = {
		"uppc",
		(char *)0
	};
	static void *mhdl = NULL;
	static char machname[MAXNAMELEN];

	if (first)
		pnamep = psm_impl_module_list;

	if (*pnamep != (char *)0)
		return (*pnamep++);

	mhdl = get_next_mach(mhdl, machname);
	if (mhdl)
		return (machname);
	return ((char *)0);
}

void
psm_modload(void)
{
	char *this;

	mutex_init(&psmsw_lock, NULL, MUTEX_DEFAULT, NULL);
	open_mach_list();

	for (this = psm_get_impl_module(1);
		this != (char *)NULL;
		this = psm_get_impl_module(0)) {
		if (modload("mach", this) == -1)
			cmn_err(CE_WARN, "!Cannot load psm %s", this);
	}
	close_mach_list();
}

void
psm_install(void)
{
	struct psm_sw *swp, *cswp;
	struct psm_ops *opsp;
	char machstring[15];
	int err;

	mutex_enter(&psmsw_lock);
	for (swp = psmsw->psw_forw; swp != psmsw; ) {
		opsp = swp->psw_infop->p_ops;
		if (opsp->psm_probe) {
			if ((*opsp->psm_probe)() == PSM_SUCCESS) {
				swp->psw_flag |= PSM_MOD_IDENTIFY;
				swp = swp->psw_forw;
				continue;
			}
		}
		/* remove the unsuccessful psm modules */
		cswp = swp;
		swp = swp->psw_forw;

		mutex_exit(&psmsw_lock);
		(void) strcpy(&machstring[0], cswp->psw_infop->p_mach_idstring);
		err = mod_remove_by_name(cswp->psw_infop->p_mach_idstring);
		if (err)
			cmn_err(CE_WARN, "%s: mod_remove_by_name failed %d",
				&machstring[0], err);
		mutex_enter(&psmsw_lock);
	}
	mutex_exit(&psmsw_lock);
	(*psminitf)();
}

/*
 * Return 1 if kernel debugger is present, and 0 if not.
 */
int
psm_debugger(void)
{
	return ((boothowto & RB_DEBUG) != 0);
}
