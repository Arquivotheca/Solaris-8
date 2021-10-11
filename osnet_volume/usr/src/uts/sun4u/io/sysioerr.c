/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sysioerr.c	1.59	99/10/01 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/async.h>
#include <sys/sysiosbus.h>
#include <sys/sysioerr.h>
#include <sys/x_call.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <sys/cpu_module.h>

/*
 * Set the following variable in /etc/system to tell the kernel
 * not to shutdown the machine if the temperature reaches
 * the Thermal Warning limit.
 */
int oven_test = 0;

/*
 * To indicate if the prom has the property of "thermal-interrupt".
 */
static int thermal_interrupt_enabled = 0;

/*
 * Sbus nexus callback routine for interrupt distribution.
 */
extern void sbus_intrdist(void *, int, uint_t);

/* Global lock which protects the interrupt distribution lists */
extern kmutex_t intr_dist_lock;

#ifdef	_STARFIRE
#include <sys/starfire.h>

int
pc_translate_tgtid(caddr_t, int, volatile uint64_t *);

void
pc_ittrans_cleanup(caddr_t, volatile uint64_t *);
#endif	/* _STARFIRE */

/*
 * adb debug_sysio_errs to 1 if you don't want your system to panic on
 * sbus ue errors. adb sysio_err_flag to 0 if you don't want your system
 * to check for sysio errors at all.
 */
int sysio_err_flag = 1;
uint_t debug_sysio_errs = 0;

/*
 * bto_cnt = number of bus errors and timeouts allowed within bto_secs
 * use /etc/system to change the bto_cnt to a very large number if
 * it's a problem!
 */
int bto_secs = 10;
int bto_cnt = 10;

static uint_t
sysio_ue_intr(struct sbus_soft_state *softsp);

static uint_t
sysio_ce_intr(struct sbus_soft_state *softsp);

static uint_t
sbus_err_intr(struct sbus_soft_state *softsp);

static int
sysio_log_ce_err(struct async_flt *ecc, char *unum);

static int
sysio_log_ue_err(struct async_flt *ecc, char *unum);

static void
sbus_clear_intr(struct sbus_soft_state *softsp, uint64_t *pafsr);

static void
sbus_log_error(struct sbus_soft_state *softsp, uint64_t *pafsr, uint64_t *pafar,
    ushort_t id, ushort_t inst, int cleared,
    ddi_nofault_data_t *nofault_data, pfn_t pfn);

static int
sbus_check_bto(struct sbus_soft_state *softsp);

static void
sbus_log_csr_error(uint64_t *psb_csr, ushort_t id, ushort_t inst);

static uint_t
sbus_ctrl_ecc_err(struct sbus_soft_state *softsp);

static uint_t
sysio_dis_err(struct sbus_soft_state *softsp);

static uint_t
sysio_init_err(struct sbus_soft_state *softsp);

static uint_t
sysio_thermal_warn_intr(struct sbus_soft_state *softsp);

static int sbus_pil[] = {SBUS_UE_PIL, SBUS_CE_PIL, SBUS_ERR_PIL, SBUS_PF_PIL,
	SBUS_THERMAL_PIL, SBUS_PM_PIL};
int
sysio_err_init(struct sbus_soft_state *softsp, caddr_t address)
{
	if (sysio_err_flag == 0) {
		cmn_err(CE_CONT, "Warning: sysio errors not initialized\n");
		return (DDI_SUCCESS);
	}

	/*
	 * Get the address of the already mapped-in sysio/sbus error registers.
	 * Simply add each registers offset to the already mapped in address
	 * that was retrieved from the device node's "address" property,
	 * and passed as an argument to this function.
	 *
	 * Define a macro for the pointer arithmetic ...
	 */

#define	REG_ADDR(b, o)	(uint64_t *)((caddr_t)(b) + (o))

	softsp->sysio_ecc_reg = REG_ADDR(address, OFF_SYSIO_ECC_REGS);
	softsp->sysio_ue_reg = REG_ADDR(address, OFF_SYSIO_UE_REGS);
	softsp->sysio_ce_reg = REG_ADDR(address, OFF_SYSIO_CE_REGS);
	softsp->sbus_err_reg = REG_ADDR(address, OFF_SBUS_ERR_REGS);

#undef	REG_ADDR

	/*
	 * create the interrupt-priorities property if it doesn't
	 * already exist to provide a hint as to the PIL level for
	 * our interrupt.
	 */
	{
		int len;

		if (ddi_getproplen(DDI_DEV_T_ANY, softsp->dip,
		    DDI_PROP_DONTPASS, "interrupt-priorities",
		    &len) != DDI_PROP_SUCCESS) {
				/* Create the interrupt-priorities property. */
			(void) ddi_prop_create(DDI_DEV_T_NONE, softsp->dip,
			    DDI_PROP_CANSLEEP, "interrupt-priorities",
			    (caddr_t)sbus_pil, sizeof (sbus_pil));
		}
	}

	(void) ddi_add_intr(softsp->dip, 0, NULL, NULL,
	    (uint_t (*)())sysio_ue_intr, (caddr_t)softsp);
	(void) ddi_add_intr(softsp->dip, 1, NULL, NULL,
	    (uint_t (*)())sysio_ce_intr, (caddr_t)softsp);
	(void) ddi_add_intr(softsp->dip, 2, NULL, NULL,
	    (uint_t (*)())sbus_err_intr, (caddr_t)softsp);
	/*
	 * If the thermal-interrupt property is in place,
	 * then register the thermal warning interrupt handler and
	 * program its mapping register
	 */
	thermal_interrupt_enabled = ddi_getprop(DDI_DEV_T_ANY, softsp->dip,
		DDI_PROP_DONTPASS, "thermal-interrupt", -1);

	if (thermal_interrupt_enabled == 1) {
		(void) ddi_add_intr(softsp->dip, 4, NULL, NULL,
		    (uint_t (*)())sysio_thermal_warn_intr, (caddr_t)softsp);
	}

	register_bus_func(UE_ECC_FTYPE, sbus_ctrl_ecc_err, (caddr_t)softsp);
	register_bus_func(DIS_ERR_FTYPE, sysio_dis_err, (caddr_t)softsp);

	(void) sysio_init_err(softsp);

	return (DDI_SUCCESS);
}

int
sysio_err_resume_init(struct sbus_soft_state *softsp)
{
	(void) sysio_init_err(softsp);
	return (DDI_SUCCESS);
}

int
sysio_err_uninit(struct sbus_soft_state *softsp)
{
	/* remove the interrupts from the interrupt list */
	(void) sysio_dis_err(softsp);

	mutex_enter(&intr_dist_lock);

	ddi_remove_intr(softsp->dip, 0, NULL);
	ddi_remove_intr(softsp->dip, 1, NULL);
	ddi_remove_intr(softsp->dip, 2, NULL);

	if (thermal_interrupt_enabled == 1) {
		ddi_remove_intr(softsp->dip, 4, NULL);
	}
	unregister_bus_func((caddr_t)softsp);

	mutex_exit(&intr_dist_lock);

	return (DDI_SUCCESS);
}

static uint_t
sysio_init_err(struct sbus_soft_state *softsp)
{
	volatile uint64_t tmp_mondo_vec, tmpreg;
	volatile uint64_t *mondo_vec_reg;
	uint_t cpu_id;

	/*
	 * Hold the global interrupt distribution lock.
	 */
	mutex_enter(&intr_dist_lock);

	/*
	 * Program the mondo vector accordingly.  This MUST be the
	 * last thing we do.  Once we program the mondo, the device
	 * may begin to interrupt. Store it in the hardware reg.
	 */
	mondo_vec_reg = (uint64_t *)(softsp->intr_mapping_reg + UE_ECC_MAPREG);
	cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
		softsp->intr_mapping_ign | UE_ECC_MONDO, 0);
#ifdef	_STARFIRE
	cpu_id = pc_translate_tgtid(softsp->ittrans_cookie, cpu_id,
				mondo_vec_reg);
#endif	/* _STARFIRE */
	tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	mondo_vec_reg = (uint64_t *)(softsp->intr_mapping_reg + CE_ECC_MAPREG);
	cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
		softsp->intr_mapping_ign | CE_ECC_MONDO, 0);
#ifdef	_STARFIRE
	cpu_id = pc_translate_tgtid(softsp->ittrans_cookie, cpu_id,
				mondo_vec_reg);
#endif	/* _STARFIRE */
	tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	mondo_vec_reg =
	    (uint64_t *)(softsp->intr_mapping_reg + SBUS_ERR_MAPREG);

	cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
		softsp->intr_mapping_ign | SBUS_ERR_MONDO, 0);
#ifdef	_STARFIRE
	cpu_id = pc_translate_tgtid(softsp->ittrans_cookie, cpu_id,
				mondo_vec_reg);
#endif	/* _STARFIRE */
	tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	if (thermal_interrupt_enabled == 1) {
		mondo_vec_reg = (softsp->intr_mapping_reg + THERMAL_MAPREG);
		cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
			softsp->intr_mapping_ign | THERMAL_MONDO, 0);
		tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) |
			INTERRUPT_VALID;
		*mondo_vec_reg = tmp_mondo_vec;
	}

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;

	mutex_exit(&intr_dist_lock);

	/*
	 * XXX - This may already be set by the OBP.
	 */
	tmpreg = SYSIO_APCKEN;
	*softsp->sysio_ctrl_reg |= tmpreg;
	tmpreg = (SECR_ECC_EN | SECR_UE_INTEN | SECR_CE_INTEN);
	*softsp->sysio_ecc_reg = tmpreg;
	tmpreg = SB_CSR_ERRINT_EN;
	*softsp->sbus_err_reg |= tmpreg;

	/* Initialize timeout/bus error counter */
	timerclear(&softsp->bto_timestamp);
	softsp->bto_ctr = 0;

	return (0);
}

static uint_t
sysio_dis_err(struct sbus_soft_state *softsp)
{
	volatile uint64_t tmpreg;
	volatile uint64_t *mondo_vec_reg, *clear_vec_reg;

	*softsp->sysio_ctrl_reg &= ~SYSIO_APCKEN;
	*softsp->sysio_ecc_reg = 0;
	*softsp->sbus_err_reg &= ~SB_CSR_ERRINT_EN;

	/* Lock the global interrupt distribution list lock */
	mutex_enter(&intr_dist_lock);

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;
#ifdef lint
	tmpreg = tmpreg;
#endif

	/* Unmap mapping registers */
	mondo_vec_reg = (softsp->intr_mapping_reg + UE_ECC_MAPREG);
	clear_vec_reg = (softsp->clr_intr_reg + UE_ECC_CLEAR);

	*mondo_vec_reg = 0;
	intr_rem_cpu(softsp->intr_mapping_ign | UE_ECC_MONDO);

#ifdef	_STARFIRE
	/* do cleanup for starfire interrupt target translation */
	pc_ittrans_cleanup(softsp->ittrans_cookie, mondo_vec_reg);
#endif	/* _STARFIRE */

	*clear_vec_reg = 0;

	mondo_vec_reg = (softsp->intr_mapping_reg + CE_ECC_MAPREG);
	clear_vec_reg = (softsp->clr_intr_reg + CE_ECC_CLEAR);

	*mondo_vec_reg = 0;

	intr_rem_cpu(softsp->intr_mapping_ign | CE_ECC_MONDO);

#ifdef	_STARFIRE
	/* Do cleanup for starfire interrupt target translation */
	pc_ittrans_cleanup(softsp->ittrans_cookie, mondo_vec_reg);
#endif	/* _STARFIRE */

	*clear_vec_reg = 0;

	mondo_vec_reg = (softsp->intr_mapping_reg + SBUS_ERR_MAPREG);
	clear_vec_reg = (softsp->clr_intr_reg + SBUS_ERR_CLEAR);

	*mondo_vec_reg = 0;

	intr_rem_cpu(softsp->intr_mapping_ign | SBUS_ERR_MONDO);

#ifdef	_STARFIRE
	/* Do cleanup for starfire interrupt target translation */
	pc_ittrans_cleanup(softsp->ittrans_cookie, mondo_vec_reg);
#endif	/* _STARFIRE */

	*clear_vec_reg = 0;

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;

	mutex_exit(&intr_dist_lock);

	return (0);
}

/*
 * gather the information about the error, plus a pointer to
 * the callback logging function, and call the generic ue_error handler.
 */
static uint_t
sysio_ue_intr(struct sbus_soft_state *softsp)
{
	volatile uint64_t t_afsr;
	volatile uint64_t t_afar;
	volatile uint64_t *ue_reg, *afar_reg, *clear_reg;
	struct async_flt ecc;

	/*
	 * Disable all further sbus errors, for this sbus instance, for
	 * what is guaranteed to be a fatal error. And grab any other cpus.
	 */
	(void) sysio_dis_err(softsp);		/* disabled sysio errors */

	/*
	 * Then read and clear the afsr/afar and clear interrupt regs.
	 */
	ue_reg = (uint64_t *)softsp->sysio_ue_reg;
	t_afsr = *ue_reg;
	afar_reg = (uint64_t *)ue_reg + 1;
	t_afar = *afar_reg;
	*ue_reg = t_afsr;

	clear_reg = (softsp->clr_intr_reg + UE_ECC_CLEAR);
	*clear_reg = 0;

	ecc.flt_stat = t_afsr;
	ecc.flt_addr = t_afar;
	ecc.flt_status = ECC_IOBUS;
	ecc.flt_size = (ushort_t)((t_afsr & SB_UE_AFSR_SIZE) >>
	    SB_UE_SIZE_SHIFT);
	ecc.flt_offset = (ushort_t)((t_afsr & SB_UE_AFSR_OFF) >>
	    SB_UE_DW_SHIFT);
	if (ecc.flt_size > 3)
		ecc.flt_offset *= 8;
	ecc.flt_bus_id = softsp->upa_id;
	ecc.flt_inst = ddi_get_instance(softsp->dip);
	ecc.flt_func = (afunc)sysio_log_ue_err;
	ecc.flt_in_memory =
		(pf_is_memory(ecc.flt_addr >> MMU_PAGESHIFT)) ? 1: 0;

	(void) ue_error(&ecc);
	return (DDI_INTR_CLAIMED);
}

/*
 * callback logging function from the common error handling code
 */
static int
sysio_log_ue_err(struct async_flt *ecc, char *unum)
{
	uint64_t t_afsr;
	uint64_t t_afar;

	short verbose = 1, ce_err = 0;
	ushort_t id = ecc->flt_bus_id;
	ushort_t inst = ecc->flt_inst;

	t_afsr = ecc->flt_stat;
	t_afar = ecc->flt_addr;

	if (debug_sysio_errs) {
		if (t_afsr & SB_UE_AFSR_P_PIO) {
			cmn_err(CE_CONT, "SBus%d UE Primary Error from PIO: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d\n",
			    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
		}
		if (t_afsr & SB_UE_AFSR_P_DRD) {
			cmn_err(CE_CONT, "SBus%d UE Primary Error DMA read: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s "
			    "Id %d\n", inst, (uint32_t)(t_afsr>>32),
			    (uint32_t)t_afsr, (uint32_t)(t_afar>>32),
			    (uint32_t)t_afar, unum, id);
		}
		if (t_afsr & SB_UE_AFSR_P_DWR) {
			cmn_err(CE_CONT, "SBus%d UE Primary Error DMA write: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s Id %d\n",
				inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
				(uint32_t)(t_afar>>32), (uint32_t)t_afar,
				unum, id);
		}
		(void) read_ecc_data(ecc, verbose, ce_err);

		cmn_err(CE_CONT, "\tOffset 0x%x, Size %d, UPA MID 0x%x\n",
		(uint32_t)((t_afsr & SB_UE_AFSR_OFF) >> SB_UE_DW_SHIFT),
		(uint32_t)((t_afsr & SB_UE_AFSR_SIZE) >> SB_UE_SIZE_SHIFT),
		(uint32_t)((t_afsr & SB_UE_AFSR_MID) >> SB_UE_MID_SHIFT));

		return (UE_DEBUG); /* XXX - hack alert, should be fatal */
	}

	if (t_afsr & SB_UE_AFSR_P_PIO) {
		cmn_err(CE_PANIC, "SBus%d UE Primary Error from PIO: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_UE_AFSR_P_DRD) {
		cmn_err(CE_PANIC, "SBus%d UE Primary Error DMA read: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
	}
	if (t_afsr & SB_UE_AFSR_P_DWR) {
		cmn_err(CE_PANIC, "SBus%d UE Primary Error DVMA write: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
	}
	/*
	 * We should never hit the secondary error panics.
	 */
	if (t_afsr & SB_UE_AFSR_S_PIO) {
		cmn_err(CE_PANIC, "SBus%d UE Secondary Error from PIO: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_UE_AFSR_S_DRD) {
		cmn_err(CE_PANIC, "SBus%d UE Secondary Error DMA read: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
	}
	if (t_afsr & SB_UE_AFSR_S_DWR) {
		cmn_err(CE_PANIC, "SBus%d UE Secondary  Error DMA write: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
	}
	/* NOTREACHED */
	return (UE_FATAL);		/* should be always fatal */
}

/*
 * gather the information about the error, plus a pointer to
 * the callback logging function, and call the generic ce_error handler.
 */
static uint_t
sysio_ce_intr(struct sbus_soft_state *softsp)
{
	volatile uint64_t t_afsr;
	volatile uint64_t t_afar;
	volatile uint64_t *afar_reg, *clear_reg, *ce_reg;
	struct async_flt ecc;

	ce_reg = (uint64_t *)softsp->sysio_ce_reg;
	t_afsr = *ce_reg;
	afar_reg = (uint64_t *)ce_reg + 1;
	t_afar = *afar_reg;
	*ce_reg = t_afsr;

	clear_reg = (softsp->clr_intr_reg + CE_ECC_CLEAR);
	*clear_reg = 0;

	ecc.flt_stat = t_afsr;
	ecc.flt_addr = t_afar;
	ecc.flt_status = ECC_IOBUS;
	ecc.flt_synd = (ushort_t)((t_afsr & SB_CE_AFSR_SYND) >>
	    SB_CE_SYND_SHIFT);
	ecc.flt_size = (ushort_t)((t_afsr & SB_CE_AFSR_SIZE) >>
	    SB_CE_SIZE_SHIFT);
	ecc.flt_offset = (ushort_t)((t_afsr & SB_CE_AFSR_OFF) >>
	    SB_CE_OFFSET_SHIFT);
	if (ecc.flt_size > 3)
		ecc.flt_offset *= 8;

	ecc.flt_bus_id = softsp->upa_id;
	ecc.flt_inst = ddi_get_instance(softsp->dip);
	ecc.flt_func = (afunc)sysio_log_ce_err;
	ecc.flt_in_memory =
		(pf_is_memory(ecc.flt_addr >> MMU_PAGESHIFT)) ? 1: 0;

	(void) ce_error(&ecc);
	return (DDI_INTR_CLAIMED);
}

/*
 * callback logging function from the common error handling code
 */
static int
sysio_log_ce_err(struct async_flt *ecc, char *unum)
{
	uint64_t t_afsr;
	uint64_t t_afar;
	ushort_t id = ecc->flt_bus_id;
	ushort_t inst = ecc->flt_inst;
	int memory_err = 0;

	t_afsr = ecc->flt_stat;
	t_afar = ecc->flt_addr;

	if (t_afsr & SB_CE_AFSR_P_PIO) {
		cmn_err(CE_CONT, "SBus%d CE Primary Error from PIO: "
		    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d\n",
		    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_CE_AFSR_P_DRD) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Primary Error DMA read: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s "
			    "Id %d\n",
			    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
		memory_err = 1;
	}
	if (t_afsr & SB_CE_AFSR_P_DWR) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Primary Error DMA write: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s "
			    "Id %d\n",
			    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
		memory_err = 1;
	}
	if (t_afsr & SB_CE_AFSR_S_PIO) {
		cmn_err(CE_CONT, "SBus%d CE Secondary Error from PIO: "
		    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d\n",
		    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_CE_AFSR_S_DRD) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Secondary Error DMA read: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s "
			    "Id %d\n",
			    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
	}
	if (t_afsr & SB_CE_AFSR_S_DWR) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Secondary Error DMA write: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x MemMod %s "
			    "Id %d\n",
			    inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar>>32), (uint32_t)t_afar, unum, id);
	}
	if ((memory_err == 0) || (debug_sysio_errs) || (ce_verbose))
		cmn_err(CE_CONT, "\tSyndrome 0x%x, Offset 0x%x, Size %d, "
		"UPA MID 0x%x\n",
		(uint32_t)((t_afsr & SB_CE_AFSR_SYND) >> SB_CE_SYND_SHIFT),
		(uint32_t)((t_afsr & SB_CE_AFSR_OFF) >> SB_CE_OFFSET_SHIFT),
		(uint32_t)((t_afsr & SB_CE_AFSR_SIZE) >> SB_CE_SIZE_SHIFT),
		(uint32_t)((t_afsr & SB_CE_AFSR_MID) >> SB_CE_MID_SHIFT));
	return (memory_err);
}

static uint_t
sbus_err_intr(struct sbus_soft_state *softsp)
{
	volatile uint64_t t_afsr;
	volatile uint64_t t_afar;
	ushort_t id, inst;
	int cleared = 0;
	volatile uint64_t *afar_reg;
	ddi_nofault_data_t *nofault_data = softsp->nofault_data;
	pfn_t pfn;

	t_afsr = *softsp->sbus_err_reg;
	afar_reg = (uint64_t *)softsp->sbus_err_reg + 1;
	t_afar = *afar_reg;

	if (!nofault_data || (nofault_data->op_type != POKE_START)) {
		sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
		cleared = 1;
	}

	id = (ushort_t)softsp->upa_id;
	inst = (ushort_t)ddi_get_instance(softsp->dip);
	pfn = t_afar >> MMU_PAGESHIFT;
	if (debug_sysio_errs) {
		if (nofault_data && (nofault_data->op_type == POKE_START) &&
		    (pfn == nofault_data->pfn)) {
			nofault_data->op_type = POKE_FAULT;
		}
		if (!cleared)
			sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
		cmn_err(CE_CONT, "SBus%d Error: AFSR 0x%08x.%08x "
			"AFAR 0x%08x.%08x Id %d\n",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
		debug_enter("sbus_err_intr");
	} else {
		sbus_log_error(softsp, (uint64_t *)&t_afsr,
		    (uint64_t *)&t_afar, id, inst, cleared,
		    nofault_data, pfn);
	}
	if (!cleared) {
		sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
	}

	return (DDI_INTR_CLAIMED);
}

static void
sbus_clear_intr(struct sbus_soft_state *softsp, uint64_t *pafsr)
{
	volatile uint64_t *clear_reg;

	*softsp->sbus_err_reg = *pafsr;
	clear_reg = (softsp->clr_intr_reg + SBUS_ERR_CLEAR);
	*clear_reg = 0;
}

static void
sbus_log_error(struct sbus_soft_state *softsp, uint64_t *pafsr, uint64_t *pafar,
    ushort_t id, ushort_t inst, int cleared,
    ddi_nofault_data_t *nofault_data, pfn_t pfn)
{
	uint64_t t_afsr;
	uint64_t t_afar;
	int level = CE_WARN;

	t_afsr = *pafsr;
	t_afar = *pafar;
	if (t_afsr & SB_AFSR_P_LE) {
		if (!cleared)
			sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
		cmn_err(CE_PANIC, "SBus%d Primary Error Late PIO: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_AFSR_P_TO) {
		if (nofault_data && (nofault_data->op_type == POKE_START) &&
		    (pfn == nofault_data->pfn)) {
			nofault_data->op_type = POKE_FAULT;
			return;
		}
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Primary Error Timeout: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_AFSR_P_BERR) {
		if (nofault_data && (nofault_data->op_type == POKE_START) &&
		    (pfn == nofault_data->pfn)) {
			nofault_data->op_type = POKE_FAULT;
			return;
		}
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Primary Error Bus Error: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d\n",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}

	if (t_afsr & SB_AFSR_S_LE) {
		if (!cleared)
			sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
		cmn_err(CE_PANIC, "SBus%d Secondary Late PIO Error: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_AFSR_S_TO) {
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Secondary Timeout Error: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
	if (t_afsr & SB_AFSR_S_BERR) {
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (uint64_t *)&t_afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Secondary Bus Error: "
			"AFSR 0x%08x.%08x AFAR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_afsr>>32), (uint32_t)t_afsr,
			(uint32_t)(t_afar>>32), (uint32_t)t_afar, id);
	}
}


static int
sbus_check_bto(struct sbus_soft_state *softsp)
{
	struct timeval now, now_diff;

	(void) uniqtime(&now);
	if (timerisset(&softsp->bto_timestamp)) {
		/* CSTYLED */
		if (timercmp(&softsp->bto_timestamp, &now, >)) {
			/* timer has wrapped, so restart it from now */
			softsp->bto_timestamp = now;
			softsp->bto_ctr = 1;
		} else {
			/*
			 * subtract (smaller) initial saved time from the
			 * (larger) now/current time, to get the difference
			 */
			now_diff = now;
			timevalsub(&now_diff, &softsp->bto_timestamp);
			if ((now_diff.tv_sec <= bto_secs) &&
			    (softsp->bto_ctr > bto_cnt)) {
				/* thanks for playing, sorry you lose */
				return (1);
			} else if (now_diff.tv_sec > bto_secs) {
				/* restart timer due to over */
				softsp->bto_timestamp = now;
				softsp->bto_ctr = 1;
			} else {
				softsp->bto_ctr++;
			}
		}
	} else {
		/* initial start of timer */
		softsp->bto_timestamp = now;
		softsp->bto_ctr = 1;
	}
	return (0);
}

static uint_t
sbus_ctrl_ecc_err(struct sbus_soft_state *softsp)
{
	int fatal = 0;
	ushort_t id, inst;
	uint64_t t_sb_csr;

	t_sb_csr = *softsp->sbus_ctrl_reg;
	id = (ushort_t)softsp->upa_id;
	inst = (ushort_t)ddi_get_instance(softsp->dip);
	if (debug_sysio_errs) {
		cmn_err(CE_CONT, "sbus_ctrl_ecc_error: SBus%d Control "
		    "Reg 0x%08x.%08x Id %d\n", inst,
		    (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr &
		(SB_CSR_DPERR_S14 | SB_CSR_DPERR_S13 | SB_CSR_DPERR_S3 |
		SB_CSR_DPERR_S2 | SB_CSR_DPERR_S1 | SB_CSR_DPERR_S0 |
		SB_CSR_PIO_PERRS)) {		/* clear errors */
		*softsp->sbus_ctrl_reg = t_sb_csr;
		sbus_log_csr_error((uint64_t *)&t_sb_csr, id, inst);
		/* NOTREACHED */
	}
	return (fatal);
}

static void
sbus_log_csr_error(uint64_t *psb_csr, ushort_t id, ushort_t inst)
{
	uint64_t t_sb_csr;

	/*
	 * Print out SBus error information.
	 */

	t_sb_csr = *psb_csr;
	if (t_sb_csr & SB_CSR_DPERR_S14) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 14 DVMA Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_DPERR_S13) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 13 DVMA Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_DPERR_S3) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 3 DVMA Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_DPERR_S2) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 2 DVMA Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_DPERR_S1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 1 DVMA Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_DPERR_S0) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 0 DVMA Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S15) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 15 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S14) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 14 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S13) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 13 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S3) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 3 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S2) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 2 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 1 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
	if (t_sb_csr & SB_CSR_PPERR_S0) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 0 PIO Parity Error: AFSR 0x%08x.%08x Id %d",
			inst, (uint32_t)(t_sb_csr>>32), (uint32_t)t_sb_csr, id);
	}
}

/*
 * Sysio Thermal Warning interrupt handler
 */
static uint_t
sysio_thermal_warn_intr(struct sbus_soft_state *softsp)
{
	volatile uint64_t *clear_reg;
	volatile uint64_t tmp_mondo_vec;
	volatile uint64_t *mondo_vec_reg;
	const char thermal_warn_msg[] =
	    "Severe over-temperature condition detected!";

	/*
	 * Hold the global interrupt distribution lock.
	 */
	mutex_enter(&intr_dist_lock);

	/*
	 * Take off the Thermal Warning interrupt and
	 * remove its interrupt handler.
	 */
	mondo_vec_reg = (softsp->intr_mapping_reg + THERMAL_MAPREG);
	tmp_mondo_vec = *mondo_vec_reg;
	tmp_mondo_vec &= ~INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	intr_rem_cpu(softsp->intr_mapping_ign | THERMAL_MONDO);

	ddi_remove_intr(softsp->dip, 4, NULL);

	clear_reg = (softsp->clr_intr_reg + THERMAL_CLEAR);
	*clear_reg = 0;

	mutex_exit(&intr_dist_lock);

	if (oven_test) {
		cmn_err(CE_NOTE, "OVEN TEST: %s", thermal_warn_msg);
		return (DDI_INTR_CLAIMED);
	}

	cmn_err(CE_WARN, "%s", thermal_warn_msg);
	cmn_err(CE_WARN, "Powering down...");

	do_shutdown();

	/*
	 * just in case do_shutdown() fails
	 */
	(void) timeout((void(*)(void *))power_down, NULL,
	    thermal_powerdown_delay * hz);

	return (DDI_INTR_CLAIMED);
}
