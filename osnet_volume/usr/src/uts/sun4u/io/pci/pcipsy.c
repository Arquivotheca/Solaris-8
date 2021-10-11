/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcipsy.c	1.19	99/11/15 SMI"

/*
 * Psycho+ specifics implementation:
 *	interrupt mapping register
 *	PBM configuration
 *	ECC and PBM error handling
 *	Iommu mapping handling
 *	Streaming Cache flushing
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/async.h>
#include <sys/systm.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>
#include <sys/pci/pcipsy.h>

#ifdef _STARFIRE
#include <sys/starfire.h>
#endif /* _STARFIRE */

static	kstat_t *psycho_picN_ksp[PSYCHO_NUM_PICS];
static	int pci_attachcnt = 0;		/* number of instances attached */

/*LINTLIBRARY*/
/* called by pci_attach() DDI_ATTACH to initialize pci objects */
int
pci_obj_setup(pci_t *pci_p)
{
	pci_common_t *cmn_p;
	uintptr_t	pci_reg_base;
	config_header_t	*pbm_cfg_header;

	pci_preserve_iommu_tsb = 0;	/* psycho+ machines NEVER preserve */

	mutex_enter(&pci_global_mutex);
	cmn_p = get_pci_common_soft_state(pci_p->pci_id);
	if (cmn_p == NULL) {
		if (alloc_pci_common_soft_state(pci_p->pci_id) != DDI_SUCCESS)
			return (DDI_FAILURE);
		cmn_p = get_pci_common_soft_state(pci_p->pci_id);
	}
	ASSERT(cmn_p->pci_p[pci_p->pci_side] == NULL);

	cmn_p->pci_p[pci_p->pci_side] = pci_p;
	pci_p->pci_common_p = cmn_p;

	if (cmn_p->pci_common_refcnt == 0) {
		ib_create(pci_p);
		cmn_p->pci_common_ib_p = pci_p->pci_ib_p;

		cb_create(pci_p);
		cmn_p->pci_common_cb_p = pci_p->pci_cb_p;

		iommu_create(pci_p);
		cmn_p->pci_common_iommu_p = pci_p->pci_iommu_p;

		ecc_create(pci_p);
		cmn_p->pci_common_ecc_p = pci_p->pci_ecc_p;
	} else {
		ASSERT(cmn_p->pci_common_refcnt == 1);

		pci_p->pci_ib_p = cmn_p->pci_common_ib_p;
		pci_p->pci_cb_p = cmn_p->pci_common_cb_p;
		pci_p->pci_iommu_p = cmn_p->pci_common_iommu_p;
		pci_p->pci_ecc_p = cmn_p->pci_common_ecc_p;
	}

	pbm_create(pci_p);

	/*
	 * We need to create busstat performance kstats for this
	 * pci instance.
	 * We only create this for the first side to attach.
	 */
	if (cmn_p->pci_common_attachcnt == 0) {
		/*
		 * Sabre does not support these counters so we need
		 * to check the ch_device_id and ch_vendor_id of
		 * the pbm_config_header.
		 */
		pbm_cfg_header = pci_p->pci_pbm_p->pbm_config_header;

		if (pbm_cfg_header->ch_device_id == 0x108e &&
			pbm_cfg_header->ch_vendor_id == 0xa000) {
			/*
			 * we are on a Sabre so we do not create any
			 * kstats.
			 */
			cmn_p->psycho_counters_ksp = NULL;

		} else {
			/*
			 * create the busstat kstats
			 */
			pci_reg_base = get_reg_base(pci_p);
			cmn_p->psycho_pcr =
			    (uint64_t *)(pci_reg_base + PSYCHO_PERF_PCR_OFF);
			cmn_p->psycho_pic =
			    (uint64_t *)(pci_reg_base + PSYCHO_PERF_PIC_OFF);

			psycho_add_kstats(cmn_p, pci_p->pci_dip);
		}
	}

	sc_create(pci_p);

	if (cmn_p->pci_common_refcnt == 0) {

		cb_register_intr(pci_p->pci_cb_p);
		ecc_register_intr(pci_p->pci_ecc_p);

		cb_enable_intr(pci_p->pci_cb_p);
		ecc_enable_intr(pci_p->pci_ecc_p);
	}

	pbm_register_intr(pci_p->pci_pbm_p);
	pbm_enable_intr(pci_p->pci_pbm_p);

	pci_attachcnt ++;
	cmn_p->pci_common_attachcnt++;
	cmn_p->pci_common_refcnt++;

	mutex_exit(&pci_global_mutex);
	return (DDI_SUCCESS);
}

/* called by pci_detach() DDI_DETACH to destroy pci objects */
void
pci_obj_destroy(pci_t *pci_p)
{
	pci_common_t *cmn_p;
	int pic;

	mutex_enter(&pci_global_mutex);

	cmn_p = pci_p->pci_common_p;
	cmn_p->pci_common_refcnt--;
	cmn_p->pci_common_attachcnt--;
	pci_attachcnt --;

	sc_destroy(pci_p);
	pbm_destroy(pci_p);

	if (cmn_p->pci_common_refcnt != 0) {
		cmn_p->pci_p[pci_p->pci_side] = NULL;
		mutex_exit(&pci_global_mutex);
		return;
	}

	/*
	 * remove the "counters" kstat for this instance
	 */
	if (cmn_p->psycho_counters_ksp != (kstat_t *)NULL) {
		kstat_delete(cmn_p->psycho_counters_ksp);
		cmn_p->psycho_counters_ksp = NULL;
	}

	/*
	 * If we are the last instance to detach we need
	 * to remove the picN kstats. We use ac_attachcnt as a
	 * count of how many instances are still attached.
	 */
	if (pci_attachcnt == 0) {
		for (pic = 0; pic < PSYCHO_NUM_PICS; pic++) {
			if (psycho_picN_ksp[pic] != (kstat_t *)NULL) {
				kstat_delete(psycho_picN_ksp[pic]);
				psycho_picN_ksp[pic] = NULL;
			}
		}
	}

	free_pci_common_soft_state(pci_p->pci_id);
	mutex_exit(&pci_global_mutex);

	ecc_destroy(pci_p);
	iommu_destroy(pci_p);
	cb_destroy(pci_p);
	ib_destroy(pci_p);
}

/* called by pci_attach() DDI_RESUME to (re)initialize pci objects */
void
pci_obj_resume(pci_t *pci_p)
{
	mutex_enter(&pci_global_mutex);
	if (pci_p->pci_common_p->pci_common_attachcnt == 0) {

		ib_configure(pci_p->pci_ib_p);
		iommu_configure(pci_p->pci_iommu_p);
		ecc_configure(pci_p->pci_ecc_p);

		ib_restore_intr_map_regs(pci_p->pci_ib_p);
	}

	pbm_configure(pci_p->pci_pbm_p);
	sc_configure(pci_p->pci_sc_p);

	pci_p->pci_common_p->pci_common_attachcnt++;
	mutex_exit(&pci_global_mutex);
}

/* called by pci_detach() DDI_SUSPEND to suspend pci objects */
void
pci_obj_suspend(pci_t *pci_p)
{
	mutex_enter(&pci_global_mutex);
	pci_p->pci_common_p->pci_common_attachcnt--;
	mutex_exit(&pci_global_mutex);
}

/*
 * map_pci_registers
 *
 * This function is called from the attach routine to map the registers
 * accessed by this driver.
 *
 * used by: pci_attach()
 *
 * return value: DDI_FAILURE on failure
 */
int
map_pci_registers(pci_t *pci_p, dev_info_t *dip)
{
	ddi_device_acc_attr_t attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	if (ddi_regs_map_setup(dip, 0, &pci_p->pci_address[0], 0, 0,
	    &attr, &pci_p->pci_ac[0]) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "%s%d: unable to map reg entry 0\n",
			ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}
	/*
	 * if we don't have streaming buffer, then we don't have
	 * pci_address[2].
	 */
	if (pci_stream_buf_exists &&
	    ddi_regs_map_setup(dip, 2, &pci_p->pci_address[2], 0, 0,
	    &attr, &pci_p->pci_ac[2]) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "%s%d: unable to map reg entry 2\n",
			ddi_driver_name(dip), ddi_get_instance(dip));
		ddi_regs_map_free(&pci_p->pci_ac[0]);
		return (DDI_FAILURE);
	}

	/*
	 * The second register set contains the bridge's configuration
	 * header.  This header is at the very beginning of the bridge's
	 * configuration space.  This space has litte-endian byte order.
	 */
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	if (ddi_regs_map_setup(dip, 1, &pci_p->pci_address[1], 0,
	    PCI_CONF_HDR_SIZE, &attr, &pci_p->pci_ac[1]) != DDI_SUCCESS) {

		cmn_err(CE_WARN, "%s%d: unable to map reg entry 1\n",
			ddi_driver_name(dip), ddi_get_instance(dip));
		ddi_regs_map_free(&pci_p->pci_ac[0]);
		if (pci_stream_buf_exists)
			ddi_regs_map_free(&pci_p->pci_ac[2]);
		return (DDI_FAILURE);
	}
	DEBUG3(DBG_ATTACH, dip, "address (%x,%x,%x)\n",
	    pci_p->pci_address[0], pci_p->pci_address[1],
	    pci_p->pci_address[2]);

	return (DDI_SUCCESS);
}

/*
 * unmap_pci_registers:
 *
 * This routine unmap the registers mapped by map_pci_registers.
 *
 * used by: pci_detach()
 *
 * return value: none
 */
void
unmap_pci_registers(pci_t *pci_p)
{
	ddi_regs_map_free(&pci_p->pci_ac[0]);
	ddi_regs_map_free(&pci_p->pci_ac[1]);
	if (pci_stream_buf_exists)
		ddi_regs_map_free(&pci_p->pci_ac[2]);
}

static uint32_t
ib_make_ino(dev_info_t *dip, device_num_t dev, interrupt_t intr)
{
	uint32_t ino;
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));

	/*
	 * The ino for a given device id is derived as 0BSSNN where
	 *
	 *	B = 0 for bus A, 1 for bus B
	 *	SS = dev - 1 for bus A, dev - 2 for bus B
	 *	NN = 00 for INTA#, 01 for INTB#, 10 for INTC#, 11 for INTD#
	 */
	DEBUG3(DBG_IB, dip, "Psycho ib_make_ino: side=%c, dev=%x, intr=%x\n",
		pci_p->pci_side == B ? 'B' : 'A', dev, intr);
	ino = intr - 1;
	if (pci_p->pci_side == B)
		ino |= (0x10 | ((dev - 2) << 2));
	else
		ino |= ((dev - 1) << 2);
	DEBUG1(DBG_IB, dip, "Psycho ib_make_ino: ino=%x\n", ino);
	return (ino);
}

uint64_t
ib_get_map_reg(ib_mondo_t mondo, uint32_t cpu_id)
{
	return ((mondo) | (cpu_id << COMMON_INTR_MAP_REG_TID_SHIFT) |
	    COMMON_INTR_MAP_REG_VALID);

}

uint32_t
ib_map_reg_get_cpu(volatile uint64_t *reg)
{
	return ((*reg & COMMON_INTR_MAP_REG_TID) >>
	    COMMON_INTR_MAP_REG_TID_SHIFT);
}

uint64_t *
ib_intr_map_reg_addr(ib_t *ib_p, ib_ino_t ino)
{
	uint64_t *addr;

	if (ino & 0x20)
		addr = (uint64_t *)(ib_p->ib_obio_intr_map_regs +
		    (((uint_t)ino & 0x1f) << 3));
	else
		addr = (uint64_t *)(ib_p->ib_slot_intr_map_regs +
		    (((uint_t)ino & 0x3c) << 1));
	return (addr);
}

uint64_t *
ib_clear_intr_reg_addr(ib_t *ib_p, ib_ino_t ino)
{
	uint64_t *addr;

	if (ino & 0x20)
		addr = (uint64_t *)(ib_p->ib_obio_clear_intr_regs +
		    (((uint_t)ino & 0x1f) << 3));
	else
		addr = (uint64_t *)(ib_p->ib_slot_clear_intr_regs +
		    (((uint_t)ino & 0x1f) << 3));
	return (addr);
}

/*
 * psycho have one mapping register per slot
 */
void
ib_ino_map_reg_share(ib_t *ib_p, ib_ino_t ino, ib_ino_info_t *ino_p)
{
	if (!IB_IS_OBIO_INO(ino)) {
		ASSERT(ino_p->ino_slot_no < 8);
		ib_p->ib_map_reg_counters[ino_p->ino_slot_no]++;
	}
}

/*
 * return true if the ino shares mapping register with other interrupts
 * of the same slot, or is still shared by other On-board devices.
 */
int
ib_ino_map_reg_still_shared(ib_t *ib_p, ib_ino_t ino, ib_ino_info_t *ino_p)
{
	ASSERT(IB_IS_OBIO_INO(ino) || ino_p->ino_slot_no < 8);

	if (IB_IS_OBIO_INO(ino))
		return (ino_p->ino_ih_size);
	else
		return (--ib_p->ib_map_reg_counters[ino_p->ino_slot_no]);
}

uintptr_t
pci_ib_setup(ib_t *ib_p)
{
	pci_t *pci_p = ib_p->ib_pci_p;
	uintptr_t a = get_reg_base(pci_p);

	ib_p->ib_max_ino = PSYCHO_MAX_INO;
	ib_p->ib_slot_intr_map_regs = a + PSYCHO_IB_SLOT_INTR_MAP_REG_OFFSET;
	ib_p->ib_obio_intr_map_regs = a + PSYCHO_IB_OBIO_INTR_MAP_REG_OFFSET;
	ib_p->ib_obio_clear_intr_regs =
		a + PSYCHO_IB_OBIO_CLEAR_INTR_REG_OFFSET;
	return (a);
}

uint32_t
pci_xlate_intr(dev_info_t *dip, dev_info_t *rdip, ib_t *ib_p, uint32_t intr)
{
	int32_t len, i;

	/*
	 * Hack for pre 1275 imap machines e.g. quark & tazmo
	 * We need to turn any PCI interrupts into ino interrupts.  machines
	 * supporting imap will have this done in the map.
	 */
	if ((ddi_getproplen(DDI_DEV_T_ANY, rdip, NULL,
	    "interrupt-map", &len) != DDI_PROP_SUCCESS) &&
	    (intr <= PCI_INTD) && (intr >= PCI_INTA)) {
		dev_info_t *cdip;
		pci_regspec_t *pci_rp;

		cdip = get_my_childs_dip(dip, rdip);

		if (ddi_getlongprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&pci_rp, &i) != DDI_SUCCESS)
			return (0);

		intr = ib_make_ino(dip, PCI_REG_DEV_G(pci_rp->pci_phys_hi),
		    intr);

		kmem_free(pci_rp, i);
	}

	return (IB_MAKE_MONDO(ib_p, intr));
}

void
pbm_configure(pbm_t *pbm_p)
{
	pci_t *pci_p = pbm_p->pbm_pci_p;
	dev_info_t *dip = pci_p->pci_dip;
	int instance = ddi_get_instance(dip);
	uint64_t l;
	ushort_t s;

	/*
	 * Clear any PBM errors.
	 */
	l = (PSYCHO_PCI_AFSR_E_MASK << PSYCHO_PCI_AFSR_PE_SHIFT) |
		(PSYCHO_PCI_AFSR_E_MASK << PSYCHO_PCI_AFSR_SE_SHIFT);
	*pbm_p->pbm_async_flt_status_reg = l;

	/*
	 * Clear error bits in configuration status register.
	 */
	s = PCI_STAT_PERROR | PCI_STAT_S_PERROR |
		PCI_STAT_R_MAST_AB | PCI_STAT_R_TARG_AB |
		PCI_STAT_S_TARG_AB | PCI_STAT_S_PERROR;
	DEBUG1(DBG_ATTACH, dip,
	    "psycho_pbm_configure: writing %x to conf status register\n", s);
	pbm_p->pbm_config_header->ch_status_reg = s;
	DEBUG1(DBG_ATTACH, dip,
	    "psycho_pbm_configure: conf status reg now %x\n",
	    pbm_p->pbm_config_header->ch_status_reg);

	l = *pbm_p->pbm_ctrl_reg;	/* save control register state */
	DEBUG1(DBG_ATTACH, dip, "psycho_pbm_configure: ctrl reg=%x\n", l);

	/*
	 * See if any SERR# signals are asserted.  We'll clear them later.
	 */
	if (l & COMMON_PCI_CTRL_SERR)
		cmn_err(CE_WARN, "%s%d: SERR asserted on pci bus\n",
		    ddi_driver_name(dip), instance);

	/*
	 * Determine if PCI bus is running at 33 or 66 mhz.
	 */
	if (l & COMMON_PCI_CTRL_SPEED)
		pbm_p->pbm_speed = PBM_SPEED_66MHZ;
	else
		pbm_p->pbm_speed = PBM_SPEED_33MHZ;
	DEBUG1(DBG_ATTACH, dip, "psycho_pbm_configure: %d mhz\n",
	    pbm_p->pbm_speed  == PBM_SPEED_66MHZ ? 66 : 33);

	/*
	 * Enable error interrupts.
	 */
	if (pci_error_intr_enable & (1 << instance))
		l |= PSYCHO_PCI_CTRL_ERR_INT_EN;
	else
		l &= ~PSYCHO_PCI_CTRL_ERR_INT_EN;

	/*
	 * Enable pci streaming byte errors and error interrupts.
	 */
	if (pci_sbh_error_intr_enable & (1 << instance))
		l |= PSYCHO_PCI_CTRL_SBH_INT_EN;
	else
		l &= ~PSYCHO_PCI_CTRL_SBH_INT_EN;

	/*
	 * Enable/disable bus parking.
	 */
	if (pci_bus_parking_enable & (1 << instance))
		l |= PSYCHO_PCI_CTRL_ARB_PARK;
	else
		l &= ~PSYCHO_PCI_CTRL_ARB_PARK;

	/*
	 * Enable arbitration.
	 */
	if (pci_p->pci_side == B)
		l = (l & ~PSYCHO_PCI_CTRL_ARB_EN_MASK) | pci_b_arb_enable;
	else
		l = (l & ~PSYCHO_PCI_CTRL_ARB_EN_MASK) | pci_a_arb_enable;

	/*
	 * Make sure SERR is clear
	 */
	l |= COMMON_PCI_CTRL_SERR;

	/*
	 * Make sure power management interrupt is disabled.
	 */
	l &= ~PSYCHO_PCI_CTRL_WAKEUP_EN;

#ifdef _STARFIRE
	/*
	 * Hack to determine whether we do Starfire special handling
	 * For starfire, we simply program a constant odd-value
	 * (0x1D) in the MID field.
	 *
	 * Zero out the MID field before ORing. We leave the LSB of
	 * the MID field intact since we cannot have a zero (even)
	 * MID value.
	 */
	l &= 0xFF0FFFFFFFFFFFFFULL;
	l |= 0x1DULL << 51;

	/*
	 * Program in the Interrupt Group Number.  Here we have to
	 * convert the starfire 7bit upaid into a 5bit value.
	 */
	l |= (uint64_t)STARFIRE_UPAID2HWIGN(pbm_p->pbm_pci_p->pci_id)
		<< COMMON_CB_CONTROL_STATUS_IGN_SHIFT;
#endif /* _STARFIRE */

	/*
	 * Now finally write the control register with the appropriate value.
	 */
	DEBUG1(DBG_ATTACH, dip,
	    "psycho_pbm_configure: writing %x to ctrl register\n", l);
	*pbm_p->pbm_ctrl_reg = l;

	/*
	 * Allow the diag register to be set based upon variable that
	 * can be configured via /etc/system.
	 */
	l = *pbm_p->pbm_diag_reg;
	DEBUG1(DBG_ATTACH, dip, "psycho_pbm_configure: PCI diag reg=%x\n", l);
	if (pci_retry_disable & (1 << instance))
		l |= COMMON_PCI_DIAG_DIS_RETRY;
	if (pci_retry_enable & (1 << instance))
		l &= ~COMMON_PCI_DIAG_DIS_RETRY;
	if (pci_intsync_disable & (1 << instance))
		l |= COMMON_PCI_DIAG_DIS_INTSYNC;
	else
		l &= ~COMMON_PCI_DIAG_DIS_INTSYNC;
	if (pci_dwsync_disable & (1 << instance))
		l |= PSYCHO_PCI_DIAG_DIS_DWSYNC;
	else
		l &= ~PSYCHO_PCI_DIAG_DIS_DWSYNC;
	DEBUG1(DBG_ATTACH, dip,
	    "psycho_pbm_configure: writing %x to PCI diag register\n", l);
	*pbm_p->pbm_diag_reg = l;

	/*
	 * Enable SERR# and parity reporting via command register.
	 */
	if (pci_per_enable)
		s = PCI_COMM_SERR_ENABLE | PCI_COMM_PARITY_DETECT;
	else
		s = PCI_COMM_SERR_ENABLE;
	DEBUG1(DBG_ATTACH, dip,
		"psycho_pbm_configure: writing %x to conf command reg\n", s);
	pbm_p->pbm_config_header->ch_command_reg = s;
	DEBUG1(DBG_ATTACH, dip, "psycho_pbm_configure: conf command reg=%x\n",
		pbm_p->pbm_config_header->ch_command_reg);

	/*
	 * The current versions of the obp are suppose to set the latency
	 * timer register but do not.  Bug 1234181 is open against this
	 * problem.  Until this bug is fixed we check to see if the obp
	 * has attempted to set the latency timer register by checking
	 * for the existance of a "latency-timer" property.
	 */
	if (pci_set_latency_timer_register &&
		ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		"latency-timer", 0) == 0) {

		DEBUG1(DBG_ATTACH, dip,
			"psycho_pbm_configure: writing %x to lat timer reg\n",
			pci_latency_timer);
		pbm_p->pbm_config_header->ch_latency_timer_reg =
			pci_latency_timer;
		DEBUG1(DBG_ATTACH, dip,
			"psycho_pbm_configure: lat timer reg now %x\n",
			pci_latency_timer);
		(void) ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
			"latency-timer", (caddr_t)&pci_latency_timer,
			sizeof (uint_t));

	}
}

void
pbm_disable_pci_errors(pbm_t *pbm_p)
{
	ib_t *ib_p = pbm_p->pbm_pci_p->pci_ib_p;
	ib_ino_t ino;

	/*
	 * Disable SERR# and parity reporting via the configuration
	 * command register.
	 */
	pbm_p->pbm_config_header->ch_command_reg &=
		~(PCI_COMM_SERR_ENABLE | PCI_COMM_PARITY_DETECT);

	/*
	 * Disable error and streaming byte hole interrupts via the
	 * PBM control register.
	 */
	*pbm_p->pbm_ctrl_reg &=
		~(PSYCHO_PCI_CTRL_ERR_INT_EN | PSYCHO_PCI_CTRL_SBH_INT_EN);

	/*
	 * Disable error interrupts via the interrupt mapping register.
	 */
	ino = IB_MONDO_TO_INO(pbm_p->pbm_bus_error_mondo);
	ib_intr_disable(ib_p, ino, IB_INTR_NOWAIT);
}

char *ecc_main_fmt =
	"?%s error from pci%d (upa mid %llx) during \n\t%s transaction %s";
static char *ecc_blk_fmt = "?\tTransaction was a block operation.\n";
static char *ecc_bytemask_fmt = "?\tbyte mask is %llx.\n";
char *dw_fmt = "?\tAFSR=%08x.%08x AFAR=%08x.%08x,\n\t"
	"double word offset=%llx, Memory Module %s id %d.\n";
char *ecc_sec_fmt = "?\tsecondary error from %s transaction\n";
char *dvma_rd = "DVMA read";
char *dvma_wr = "DVMA write";
char *pio_wr = "PIO write";
static char *unknown = "unknown";
static char *dte_str = "(with DVMA translation error)";

/* ARGSUSED */
uint_t
ecc_log_ue_error(struct async_flt *ecc, char *unum)
{
	uint64_t afsr = ecc->flt_stat;
	uint64_t afar = ecc->flt_addr;
	ushort_t id = ecc->flt_bus_id;
	ushort_t inst = ecc->flt_inst;
	uint64_t dte_err, pri_err, sec_err, device_id, dw_offset;
	char *pe, *pe2 = "\0";

	int will_panic = 1;
	char *main_fmt = ecc_main_fmt;
	char *blk_fmt = ecc_blk_fmt;
	char *bytemask_fmt = ecc_bytemask_fmt;
	char *dword_fmt = dw_fmt;
	char *sec_fmt = ecc_sec_fmt;

	/*
	 * Determine the primary error type.
	 */
	pri_err = (afsr >> COMMON_ECC_UE_AFSR_PE_SHIFT) &
	    COMMON_ECC_UE_AFSR_E_MASK;
	switch (pri_err) {
	case COMMON_ECC_UE_AFSR_E_PIO:
		pe = pio_wr;
		break;
	case COMMON_ECC_UE_AFSR_E_DRD:
		pe = dvma_rd;
		break;
	case COMMON_ECC_UE_AFSR_E_DWR:
		pe = dvma_wr;
		break;
	default:
		pe = unknown;
		break;
	}

	dte_err = (uint_t)(afsr >> SABRE_UE_AFSR_PDTE_SHIFT) &
		SABRE_UE_ARSR_DTE_MASK;
	if (dte_err & SABRE_UE_AFSR_E_PDTE) {
		/*
		 * ignore translation errors on darwin platform
		 * as they don't cause data corruption
		 */
		will_panic = 0;
		pe2 = dte_str;
	}

	/*
	 * Get the parent bus id that caused the error.
	 */
	device_id = (afsr & PSYCHO_ECC_UE_AFSR_ID)
			>> PSYCHO_ECC_UE_AFSR_ID_SHIFT;

	/*
	 * Determine the doubleword offset of the error.
	 */
	dw_offset = (afsr & PSYCHO_ECC_UE_AFSR_DW_OFFSET)
			>> PSYCHO_ECC_UE_AFSR_DW_OFFSET_SHIFT;

	sec_err = (uint_t)(afsr >> COMMON_ECC_UE_AFSR_SE_SHIFT) &
		COMMON_ECC_UE_AFSR_E_MASK;

	/*
	 * Warning message printing:
	 *		console		msglog
	 * boot		NO		YES
	 * boot -v	YES		YES
	 * panic	YES		YES
	 * warning	NO		YES
	 */
	if (will_panic) {
		main_fmt++;
		blk_fmt++;
		bytemask_fmt++;
		dword_fmt++;
		sec_fmt++;
	}

	/*
	 * Log the errors.
	 */
	cmn_err(CE_CONT, main_fmt, "WARNING: uncorrectable",
		inst, device_id, pe, pe2);
	if (afsr & PSYCHO_ECC_UE_AFSR_BLK)
		cmn_err(CE_CONT, blk_fmt);
	else
		cmn_err(CE_CONT, bytemask_fmt,
			(afsr & PSYCHO_ECC_UE_AFSR_BYTEMASK)
				>> PSYCHO_ECC_UE_AFSR_BYTEMASK_SHIFT);
	cmn_err(CE_CONT, dword_fmt,
		(uint_t)(afsr >> 32), (uint_t)(afsr & 0xffffffff),
		(uint_t)(afar >> 32), (uint_t)(afar & 0xffffffff),
		dw_offset, unum, id);

	if (dte_err & SABRE_UE_AFSR_E_SDTE)
		cmn_err(CE_CONT, sec_fmt, dte_str);

	if (sec_err & COMMON_ECC_UE_AFSR_E_PIO)
		cmn_err(CE_CONT, sec_fmt, pio_wr);
	if (sec_err & COMMON_ECC_UE_AFSR_E_DRD)
		cmn_err(CE_CONT, sec_fmt, dvma_rd);
	if (sec_err & COMMON_ECC_UE_AFSR_E_DWR)
		cmn_err(CE_CONT, sec_fmt, dvma_wr);

	/* returning 2 causes an alert but not panic */
	return (will_panic ? 1 : 2);
}

/*
 * ecc_log_ce_error
 *
 * This function is called as a result of uncorrectable error interrupts
 * to log the error.
 *
 * used by: ce error handling interface
 *
 * return value: TBD
 */
/* ARGSUSED */
uint_t
ecc_log_ce_error(struct async_flt *ecc, char *unum)
{
	uint64_t afsr = ecc->flt_stat;
	uint64_t afar = ecc->flt_addr;
	ushort_t id = ecc->flt_bus_id;
	ushort_t inst = ecc->flt_inst;
	uint64_t err, device_id, dw_offset, ecc_synd;
	char *pe;
	uint_t memory_error = 0;

	/*
	 * Determine the primary error type.
	 */
	err = (afsr >> COMMON_ECC_CE_AFSR_PE_SHIFT) & COMMON_ECC_CE_AFSR_E_MASK;
	switch (err) {
	case COMMON_ECC_CE_AFSR_E_PIO:
		pe = pio_wr;
		break;
	case COMMON_ECC_CE_AFSR_E_DRD:
		pe = dvma_rd;
		break;
	case COMMON_ECC_CE_AFSR_E_DWR:
		pe = dvma_wr;
		break;
	}

	/*
	 * Get the parent bus id that caused the error and the
	 * ECC syndrome bits.
	 */
	device_id = (afsr & PSYCHO_ECC_CE_AFSR_UPA_MID)
			>> PSYCHO_ECC_CE_AFSR_UPA_MID_SHIFT;
	ecc_synd = (afsr & PSYCHO_ECC_CE_AFSR_SYND)
			>> PSYCHO_ECC_CE_AFSR_SYND_SHIFT;

	/*
	 * Determine the doubleword offset of the error.
	 */
	dw_offset = (afsr & PSYCHO_ECC_CE_AFSR_DW_OFFSET)
			>> PSYCHO_ECC_CE_AFSR_DW_OFFSET_SHIFT;

	/*
	 * Log the errors.
	 */
	cmn_err(CE_WARN, ecc_main_fmt,
		"correctable", inst, device_id, pe, "\0");
	cmn_err(CE_CONT, dw_fmt,
		(uint_t)(afsr >> 32), (uint_t)(afsr & 0xffffffff),
		(uint_t)(afar >> 32), (uint_t)(afar & 0xffffffff),
		dw_offset, unum, id);
	cmn_err(CE_CONT, "syndrome bits %x\n", (int)ecc_synd);

	err = (afsr >> COMMON_ECC_CE_AFSR_SE_SHIFT) & COMMON_ECC_CE_AFSR_E_MASK;
	if (err & COMMON_ECC_CE_AFSR_E_PIO)
		cmn_err(CE_CONT, ecc_sec_fmt, pio_wr);
	if (err & COMMON_ECC_CE_AFSR_E_DRD) {
		cmn_err(CE_CONT, ecc_sec_fmt, dvma_rd);
		memory_error = 1;
	}
	if (err & COMMON_ECC_CE_AFSR_E_DWR) {
		cmn_err(CE_CONT, ecc_sec_fmt, dvma_wr);
		memory_error = 1;
	}
	return (memory_error);
}

void
iommu_preserve_tsb(iommu_t *iommu_p)
{
	cmn_err(CE_PANIC, "iommu_preserve_tsb: Cannot preserve iommu tsb.\n");
}

void
iommu_obp_to_kernel(iommu_t *iommu_p)
{
	cmn_err(CE_PANIC, "iommu_preserve_tsb: Cannot preserve iommu tsb.\n");
}

/*
 * iommu_unmap_window
 *
 * This routine is called to break down the iommu mappings to a dvma window.
 * Non partial mappings are viewed as single window mapping.
 *
 * used by: pci_dma_unbindhdl(), pci_dma_window(),
 *	and pci_dma_mctl() - DDI_DMA_FREE, DDI_DMA_MOVWIN,
 *	and DDI_DMA_NEXTWIN
 *
 * return value: none
 */
/*ARGSUSED*/
void
iommu_unmap_window(iommu_t *iommu_p, ddi_dma_impl_t *mp)
{
	dvma_addr_t dvma_pg;
	uint_t npages;
#ifdef DEBUG
	dev_info_t *dip = iommu_p->iommu_pci_p->pci_dip;
#endif

	/*
	 * Invalidate each page of the mapping in the tsb and flush
	 * it from the tlb.
	 */
	dvma_pg = IOMMU_BTOP(mp->dmai_mapping);
	DEBUG3(DBG_UNMAP_WIN, dip, "mp=%x %x pfn(s) at %x - flush reg:",
		mp, mp->dmai_ndvmapages, dvma_pg);
	for (npages = mp->dmai_ndvmapages; npages; npages--, dvma_pg++) {
		DEBUG1(DBG_UNMAP_WIN|DBG_CONT, dip, " %x", dvma_pg);
		IOMMU_UNLOAD_TTE(iommu_p, dvma_pg);
		IOMMU_PAGE_FLUSH(iommu_p, dvma_pg);
	}
	DEBUG0(DBG_UNMAP_WIN|DBG_CONT, dip, "\n");
	if (pci_dvma_debug_on)
		pci_dvma_free_debug((char *)mp->dmai_mapping,
			mp->dmai_size, mp);
}


/*ARGSUSED*/
dvma_context_t
pci_iommu_get_dvma_context(iommu_t *iommu_p, dvma_addr_t dvma_pg_index)
{
	return (0);
}

/*ARGSUSED*/
void
pci_iommu_free_dvma_context(iommu_t *iommu_p, dvma_context_t ctx)
{
}

/*ARGSUSED*/
uint64_t
pci_iommu_ctx2tte(dvma_context_t ctx)
{
	return (0);
}

/*ARGSUSED*/
dvma_context_t
pci_iommu_tte2ctx(uint64_t tte)
{
	return (0);
}

/*ARGSUSED*/
void
sc_flush(sc_t *sc_p, ddi_dma_impl_t *mp, dvma_context_t context, off_t offset,
    size_t length)
{
	dev_info_t *dip = sc_p->sc_pci_p->pci_dip;
	dvma_addr_t dvma_addr, poffset;
	ulong_t npages;
	clock_t start_bolt;

	/*
	 * If the caches are disabled, there's nothing to do.
	 */
	DEBUG4(DBG_SC, dip,
	    "dmai_mapping=%x, dmai_size=%x offset=%x length=%x\n",
	    mp->dmai_mapping, mp->dmai_size, offset, length);

	ASSERT(pci_stream_buf_exists || !pci_stream_buf_enable);
	if (!(pci_stream_buf_enable & (1 << ddi_get_instance(dip)))) {
		DEBUG0(DBG_SC, dip, "cache disabled\n");
		return;
	}

	/*
	 * Interpret the offset parameter.
	 */
	if (offset == (uint_t)-1)
		offset = 0;
	if (offset >= mp->dmai_size) {
		DEBUG0(DBG_SC, dip, "offset > mapping size\n");
		return;
	}
	dvma_addr = mp->dmai_mapping + offset;

	/*
	 * Interpret the length parameter.
	 */
	switch (length) {
	case 0:
	case (uint_t)-1:
		length = mp->dmai_size;
		break;
	default:
		if (length > (mp->dmai_size - offset)) {
			DEBUG0(DBG_SC, dip, "length > mapping size - offset\n");
			length = mp->dmai_size - offset;
		}
		break;
	}

	/*
	 * Grap the flush mutex.
	 */
	mutex_enter(&sc_p->sc_sync_mutex);

	/*
	 * Initialize the sync flag to zero.
	 */
	*sc_p->sc_sync_flag_vaddr = 0x0ull;

	/*
	 * Cause the flush on all virtual pages of the transfer.
	 *
	 * We start flushing from the end and work our way back to the
	 * beginning.  This should minimize the time we need to spend
	 * polling the sync flag.
	 */
	poffset = dvma_addr & IOMMU_PAGE_OFFSET;
	dvma_addr &= ~poffset;
	npages = IOMMU_BTOPR(length + poffset);
	DEBUG2(DBG_SC, dip, "addr=%x size=%x - flush reg:", dvma_addr, length);
	while (npages) {
		DEBUG1(DBG_SC|DBG_CONT, dip, " %x",
		    dvma_addr & IOMMU_PAGE_MASK);
		*sc_p->sc_invl_reg =
			(uint64_t)(dvma_addr & IOMMU_PAGE_MASK);
		dvma_addr += IOMMU_PAGE_SIZE;
		npages--;
	}
	DEBUG0(DBG_SC|DBG_CONT, dip, "\n");

	/*
	 * Ask the hardware to flag when the flush is complete.
	 */
	DEBUG1(DBG_SC, dip, "writing %x to flush sync register\n",
	    sc_p->sc_sync_flag_addr);
	*sc_p->sc_sync_reg = sc_p->sc_sync_flag_addr;

	/*
	 * Poll the flush/sync flag.
	 */
	DEBUG0(DBG_SC, dip, "polling flush sync buffer\n");
	start_bolt = lbolt;
	while (*sc_p->sc_sync_flag_vaddr == 0x0ull) {
		if (lbolt - start_bolt >= pci_sync_buf_timeout) {
			if (*sc_p->sc_sync_flag_vaddr == 0x0ull)
				cmn_err(CE_PANIC,
				    "%s%d: streaming buffer flush timeout!",
				    ddi_driver_name(dip),
				    ddi_get_instance(dip));
		}
	}

	/*
	 * Release the flush mutex.
	 */
	mutex_exit(&sc_p->sc_sync_mutex);
}

void
pci_cb_setup(cb_t *cb_p)
{
	uint64_t version, l;
	uint_t i, mask;
	pci_t *pci_p = cb_p->cb_pci_p;
	dev_info_t *dip = pci_p->pci_dip;

	/*
	 * Get the virtual addresses for registers.
	 */
	uintptr_t a = get_reg_base(pci_p);
	cb_p->cb_id_reg = (uint64_t *)(a + PSYCHO_CB_DEVICE_ID_REG_OFFSET);
	cb_p->cb_control_status_reg = (uint64_t *)
		(a + PSYCHO_CB_CONTROL_STATUS_REG_OFFSET);

	/*
	 * Workarounds for hardware bugs:
	 *
	 * bus parking
	 *
	 *	Pass 2 psycho parts have a bug that requires bus
	 *	parking to be disabled.
	 *
	 *	Pass 1 cheerio parts have a bug which prevents them
	 *	from working on a PBM with bus parking enabled.
	 *
	 * rerun disable
	 *
	 *	Pass 1 and 2 psycho's require that the rerun's be
	 *	enabled.
	 *
	 * retry limit
	 *
	 *	For pass 1 and pass 2 psycho parts we disable the
	 *	retry limit.  This is because the limit of 16 seems
	 *	too restrictive for devices that are children of pci
	 *	to pci bridges.  For pass 3 this limit will be 64.
	 *
	 * DMA write/PIO read sync
	 *
	 *	For pass 2 psycho, the disable this feature.
	 */
	version = (*cb_p->cb_control_status_reg &
		PSYCHO_CB_CONTROL_STATUS_VER) >>
			PSYCHO_CB_CONTROL_STATUS_VER_SHIFT;
	i = ddi_get_instance(dip);
	mask = (1 << i) | (1 << (i & 0x1 ? (i - 1) : (i + 1)));
	DEBUG2(DBG_ATTACH, dip, "cb_create: ver=%d, mask=%x\n", version, mask);
	switch (version) {
	case 0:
		DEBUG0(DBG_ATTACH, dip, "cb_create: psycho pass 1\n");
		if (!pci_disable_pass1_workarounds) {
			if (pbm_has_pass_1_cheerio(pci_p))
				pci_bus_parking_enable &= ~mask;
			pci_rerun_disable &= ~mask;
			pci_retry_disable |= mask;
		}
		break;
	case 1:
		if (!pci_disable_pass2_workarounds) {
			pci_bus_parking_enable &= ~mask;
			pci_rerun_disable &= ~mask;
			pci_retry_disable |= mask;
			pci_dwsync_disable |= mask;
		}
		break;
	case 2:
		if (!pci_disable_pass3_workarounds) {
			pci_dwsync_disable |= mask;
			if (pbm_has_pass_1_cheerio(pci_p))
				pci_bus_parking_enable &= ~mask;
		}
		break;
	case 3:
		if (!pci_disable_plus_workarounds) {
			pci_dwsync_disable |= mask;
			if (pbm_has_pass_1_cheerio(pci_p))
				pci_bus_parking_enable &= ~mask;
		}
		break;
	default:
		if (!pci_disable_default_workarounds) {
			pci_dwsync_disable |= mask;
			if (pbm_has_pass_1_cheerio(pci_p))
				pci_bus_parking_enable &= ~mask;
		}
		break;
	}
	pci_sbh_error_intr_enable &= ~mask;

	/*
	 * Clear and pending address parity errors.
	 */
	l = *cb_p->cb_control_status_reg;
	if (l & COMMON_CB_CONTROL_STATUS_APERR) {
		l |= COMMON_CB_CONTROL_STATUS_APERR;
		cmn_err(CE_WARN, "clearing UPA address parity error\n");
	}
	l |= COMMON_CB_CONTROL_STATUS_APCKEN;
	l &= ~COMMON_CB_CONTROL_STATUS_IAP;
	*cb_p->cb_control_status_reg = l;
}

uintptr_t
pci_ecc_setup(ecc_t *ecc_p)
{
	ecc_p->ecc_ue.ecc_errpndg_mask = 0;
	ecc_p->ecc_ue.ecc_offset_mask = PSYCHO_ECC_UE_AFSR_DW_OFFSET;
	ecc_p->ecc_ue.ecc_offset_shift = PSYCHO_ECC_UE_AFSR_DW_OFFSET_SHIFT;
	ecc_p->ecc_ue.ecc_size_log2 = 3;

	ecc_p->ecc_ce.ecc_errpndg_mask = 0;
	ecc_p->ecc_ce.ecc_offset_mask = PSYCHO_ECC_CE_AFSR_DW_OFFSET;
	ecc_p->ecc_ce.ecc_offset_shift = PSYCHO_ECC_CE_AFSR_DW_OFFSET_SHIFT;
	ecc_p->ecc_ce.ecc_size_log2 = 3;

	return (get_reg_base(ecc_p->ecc_pci_p));
}

ushort_t
pci_ecc_get_synd(uint64_t afsr)
{
	return ((ushort_t)((afsr & PSYCHO_ECC_CE_AFSR_SYND)
		>> PSYCHO_ECC_CE_AFSR_SYND_SHIFT));
}

/*
 * The following data structure is used to map a tsb size (allocated
 * in startup.c) to a tsb size configuration parameter in the iommu
 * control register.
 */
static int pci_iommu_tsb_sizes[] = {
	0x2000,		/* 0 - 8 mb */
	0x4000,		/* 1 - 16 mb */
	0x8000,		/* 2 - 32 mb */
	0x10000,	/* 3 - 64 mb */
	0x20000,	/* 4 - 128 mb */
	0x40000,	/* 5 - 256 mb */
	0x80000,	/* 6 - 512 mb */
	0x100000	/* 7 - 1 gb */
};

uintptr_t
pci_iommu_setup(iommu_t *iommu_p)
{
	pci_t *pci_p = iommu_p->iommu_pci_p;
	int tsb_size, tsb_alloc_size;

	/*
	 * Determine the size and virtual address of the iommu tsb.
	 * Since there are currently no standard kernel interface
	 * for allocating memory that is guaranteed to be physically
	 * contiguous, the kernel uses bootop to allocate the table
	 * during startup.
	 */
	tsb_alloc_size = iommu_tsb_alloc_size[pci_p->pci_id];
	if ((tsb_alloc_size == 0) || ((iommu_tsb_spare[pci_p->pci_id] &
		IOMMU_TSB_ISASPARE) == IOMMU_TSB_ISASPARE)) {
		tsb_alloc_size = iommu_tsb_alloc(iommu_p);
	}

	tsb_size = sizeof (pci_iommu_tsb_sizes) /
		sizeof (pci_iommu_tsb_sizes[0]) - 1;
	while (tsb_size > 0) {
		if (tsb_alloc_size >= pci_iommu_tsb_sizes[tsb_size])
			break;
		tsb_size--;
	}
	if (PCI_VALID_ID(iommu_p->iommu_spare_slot)) {
		iommu_p->iommu_tsb_vaddr = (uint64_t *)
			iommu_tsb_vaddr[iommu_p->iommu_spare_slot];
	} else {
		iommu_p->iommu_tsb_vaddr =
			(uint64_t *)iommu_tsb_vaddr[pci_p->pci_id];
	}

	/*
	 * Psycho has no context support.
	 */
	iommu_p->iommu_ctx_bitmap = NULL;
	iommu_p->iommu_flush_ctx_reg = NULL;

	/*
	 * Determine the virtual address of the register block
	 * containing the iommu control registers.
	 */
	iommu_p->iommu_tsb_size = tsb_size;
	return (get_reg_base(pci_p));
}

/*ARGSUSED*/
void
pci_iommu_teardown(iommu_t *iommu_p)
{
}

/* The psycho+ PBM reg base is at 1fe.0000.2000 */
static uintptr_t
get_pbm_reg_base(pci_t *pci_p)
{
	return ((uintptr_t)(pci_p->pci_address[0] +
		(pci_stream_buf_exists ? 0 : PSYCHO_PCI_PBM_REG_BASE)));
}

void
pci_pbm_setup(pbm_t *pbm_p)
{
	pci_t *pci_p = pbm_p->pbm_pci_p;

	/*
	 * Get the base virtual address for the PBM control block.
	 */
	uintptr_t a = get_pbm_reg_base(pci_p);

	/*
	 * Get the virtual address of the PCI configuration header.
	 * This should be mapped little-endian.
	 */
	pbm_p->pbm_config_header =
		(config_header_t *)get_config_reg_base(pci_p);

	/*
	 * Get the virtual addresses for control, error and diag
	 * registers.
	 */
	pbm_p->pbm_ctrl_reg = (uint64_t *)(a + PSYCHO_PCI_CTRL_REG_OFFSET);
	pbm_p->pbm_diag_reg = (uint64_t *)(a + PSYCHO_PCI_DIAG_REG_OFFSET);
	pbm_p->pbm_async_flt_status_reg =
		(uint64_t *)(a + PSYCHO_PCI_ASYNC_FLT_STATUS_REG_OFFSET);
	pbm_p->pbm_async_flt_addr_reg =
		(uint64_t *)(a + PSYCHO_PCI_ASYNC_FLT_ADDR_REG_OFFSET);
}

void
pci_sc_setup(sc_t *sc_p)
{
	pci_t *pci_p = sc_p->sc_pci_p;

	/*
	 * Determine the virtual addresses of the streaming cache
	 * control/status and flush registers.
	 */
	uintptr_t a = get_pbm_reg_base(pci_p);
	sc_p->sc_ctrl_reg = (uint64_t *)(a + PSYCHO_SC_CTRL_REG_OFFSET);
	sc_p->sc_invl_reg = (uint64_t *)(a + PSYCHO_SC_INVL_REG_OFFSET);
	sc_p->sc_sync_reg = (uint64_t *)(a + PSYCHO_SC_SYNC_REG_OFFSET);

	/*
	 * Determine the virtual addresses of the streaming cache
	 * diagnostic access registers.
	 */
	a = get_reg_base(pci_p);
	if (pci_p->pci_bus_range.lo != 0) {
		sc_p->sc_data_diag_acc = (uint64_t *)
				(a + PSYCHO_SC_A_DATA_DIAG_OFFSET);
		sc_p->sc_tag_diag_acc = (uint64_t *)
				(a + PSYCHO_SC_A_TAG_DIAG_OFFSET);
		sc_p->sc_ltag_diag_acc = (uint64_t *)
				(a + PSYCHO_SC_A_LTAG_DIAG_OFFSET);
	} else {
		sc_p->sc_data_diag_acc = (uint64_t *)
				(a + PSYCHO_SC_B_DATA_DIAG_OFFSET);
		sc_p->sc_tag_diag_acc = (uint64_t *)
				(a + PSYCHO_SC_B_TAG_DIAG_OFFSET);
		sc_p->sc_ltag_diag_acc = (uint64_t *)
				(a + PSYCHO_SC_B_LTAG_DIAG_OFFSET);
	}
}

int
pci_get_numproxy(dev_info_t *dip)
{
	return (ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		"#upa-interrupt-proxies", 1));
}

int
pci_get_portid(dev_info_t *dip)
{
	return (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "upa-portid", -1));
}

/*
 * called from psycho_add_kstats() to create kstats for each %pic
 * the the PSYCHO supports. These (read-only) kstats export the
 * event names that each %pic supports.
 *
 * if we fail to create any of these kstats we must remove any
 * that we have already created and return;
 *
 * NOTE: because all psycho's use the same events we only need to
 *       create the picN kstats once. All instances can use
 *       the same picN kstats.
 *
 *       The flexibility exists to allow each device specify it's
 *       own events by creating picN kstats with the instance number
 *       set to ddi_get_instance(softsp->dip).
 *
 *       When searching for a picN kstat for a device you should
 *       first search for a picN kstat using the instance number
 *       of the device you are interested in. If that fails you
 *       should use the first picN kstat found for that device.
 */
void
psycho_add_picN_kstats(dev_info_t *dip)
{
	/*
	 * Psycho Performance Events.
	 *
	 * We declare an array of event-names and event-masks.
	 * The num of events in this array is PSYCHO_NUM_EVENTS
	 */
	psycho_event_mask_t
	psycho_events_arr[PSYCHO_NUM_EVENTS] = {
	    /* */ {"dvma_stream_rd_a", 0x0}, {"dvma_stream_wr_a", 0x1},
	    /* */ {"dvma_const_rd_a", 0x2}, {"dvma_const_wr_a", 0x3},
	    /* */ {"dvma_stream_buf_mis_a", 0x4}, {"dvma_cycles_a", 0x5},
	    /* */ {"dvma_wd_xfr_a", 0x6}, {"pio_cycles_a", 0x7},
	    /* */ {"dvma_stream_rd_b", 0x8}, {"dvma_stream_wr_b", 0x9},
	    /* */ {"dvma_const_rd_b", 0xa}, {"dvma_const_wr_b", 0xb},
	    /* */ {"dvma_stream_buf_mis_b", 0xc}, {"dvma_cycles_b", 0xd},
	    /* */ {"dvma_wd_xfr_b", 0xe}, {"pio_cycles_b", 0xf},
	    /* */ {"dvma_tlb_misses", 0x10}, {"interrupts", 0x11},
	    /* */ {"upa_inter_nack", 0x12}, {"pio_reads", 0x13},
	    /* */ {"pio_writes", 0x14}, {"merge_buffer", 0x15},
	    /* */ {"dma_tbwalk_a", 0x16}, {"dma_stc_a", 0x17},
	    /* */ {"dma_tbwalk_b", 0x18}, {"dma_stc_b", 0x19}
	};

	/*
	 * array of clear masks for each pic.
	 * These masks are used to clear the %pcr bits for
	 * each pic.
	 */
	psycho_event_mask_t psycho_clear_pic[PSYCHO_NUM_PICS] = {
		/* pic0 */ {"clear_pic", (uint64_t)~(0x1f)},
		/* pic1 */ {"clear_pic", (uint64_t)~(0x1f << 8)}
	};

	struct kstat_named *psycho_pic_named_data;
	int  		event, pic;
	char 		pic_name[30];
	int 		instance = ddi_get_instance(dip);
	char 		*drv_name = "pci";
	int		pic_shift = 0;

	for (pic = 0; pic < PSYCHO_NUM_PICS; pic++) {
		/*
		 * create the picN kstat. The size of this kstat is
		 * PSYCHO_NUM_EVENTS + 1 for the clear_event_mask
		 */
		(void) sprintf(pic_name, "pic%d", pic);
		if ((psycho_picN_ksp[pic] = kstat_create(drv_name,
			instance, pic_name, "bus", KSTAT_TYPE_NAMED,
			PSYCHO_NUM_EVENTS + 1, NULL)) == NULL) {

				cmn_err(CE_WARN, "%s %s: kstat_create failed",
					drv_name, pic_name);

				/* remove pic0 kstat if pic1 create fails */
				if (pic == 1) {
					kstat_delete(psycho_picN_ksp[0]);
					psycho_picN_ksp[0] = NULL;
				}
				return;
		}
		psycho_pic_named_data =
			(struct kstat_named *)(psycho_picN_ksp[pic]->ks_data);

		/*
		 * when we are writing pcr_masks to the kstat we need to
		 * shift bits left by 8 for pic1 events.
		 */
		if (pic == 1)
			pic_shift = 8;

		/*
		 * for each picN event we need to write a kstat record
		 * (name = EVENT, value.ui64 = PCR_MASK)
		 */
		for (event = 0; event < PSYCHO_NUM_EVENTS; event ++) {

			/* pcr_mask */
			psycho_pic_named_data[event].value.ui64 =
				psycho_events_arr[event].pcr_mask << pic_shift;

			/* event-name */
			kstat_named_init(&psycho_pic_named_data[event],
				psycho_events_arr[event].event_name,
				KSTAT_DATA_UINT64);
		}

		/*
		 * we add the clear_pic event and mask as the last
		 * record in the kstat
		 */
		/* pcr clear mask */
		psycho_pic_named_data[PSYCHO_NUM_EVENTS].value.ui64 =
			psycho_clear_pic[pic].pcr_mask;

		/* event-name */
		kstat_named_init(&psycho_pic_named_data[PSYCHO_NUM_EVENTS],
			psycho_clear_pic[pic].event_name,
			KSTAT_DATA_UINT64);

		kstat_install(psycho_picN_ksp[pic]);
	}
}

/*
 * A separate "counter" kstat is created for each PSYCHO
 * instance that provides access to the %pcr and %pic
 * registers for that instance.
 */
void
psycho_add_kstats(pci_common_t *softsp, dev_info_t *dip)
{
	struct kstat *psycho_counters_ksp;
	struct kstat_named *psycho_counters_named_data;
	int instance = ddi_get_instance(dip);
	char *drv_name = (char *)ddi_driver_name(dip);

	/*
	 * Create the picN kstats if we are the first instance
	 * to attach. We use pci_attachcnt as a count of how
	 * many instances have attached.
	 */
	if (pci_attachcnt == 0)
		psycho_add_picN_kstats(dip);

	/*
	 * The size of this kstat is PYSCHO_NUM_PICS + 1
	 * (pic0 + pic1 + pcr)
	 */
	if ((psycho_counters_ksp = kstat_create("pci",
		instance, "counters",
		"bus", KSTAT_TYPE_NAMED, PSYCHO_NUM_PICS + 1,
		KSTAT_FLAG_WRITABLE)) == NULL) {

			cmn_err(CE_WARN, "%s%d counters kstat_create"
				" failed", drv_name, instance);
			return;
	}

	psycho_counters_named_data =
		(struct kstat_named *)(psycho_counters_ksp->ks_data);

	/* initialize the named kstats */
	kstat_named_init(&psycho_counters_named_data[0],
		"pcr", KSTAT_DATA_UINT64);

	kstat_named_init(&psycho_counters_named_data[1],
		"pic0", KSTAT_DATA_UINT64);

	kstat_named_init(&psycho_counters_named_data[2],
		"pic1", KSTAT_DATA_UINT64);


	psycho_counters_ksp->ks_update = psycho_counters_kstat_update;
	psycho_counters_ksp->ks_private = (void *)softsp;

	kstat_install(psycho_counters_ksp);

	/* update the sofstate */
	softsp->psycho_counters_ksp = psycho_counters_ksp;
}


int
psycho_counters_kstat_update(kstat_t *ksp, int rw)
{
	struct kstat_named *psycho_counters_data;
	pci_common_t *softsp;
	uint64_t pic_register;

	psycho_counters_data = (struct kstat_named *)ksp->ks_data;
	softsp = (pci_common_t *)ksp->ks_private;

	if (rw == KSTAT_WRITE) {

		/*
		 * Write the pcr value to the softsp->psycho_pcr.
		 * This interface does not support writing to the
		 * %pic.
		 */
		*softsp->psycho_pcr =
			(uint32_t)psycho_counters_data[0].value.ui64;
	} else {
		/*
		 * Read %pcr and %pic register values and write them
		 * into counters kstat.
		 */
		psycho_counters_data[0].value.ui64 = *softsp->psycho_pcr;

		pic_register = *softsp->psycho_pic;
		/*
		 * psycho pic register:
		 *  (63:32) = pic0
		 *  (31:00) = pic1
		 */

		/* pic0 */
		psycho_counters_data[1].value.ui64 = pic_register >> 32;
		/* pic1 */
		psycho_counters_data[2].value.ui64 =
			pic_register & PSYCHO_PIC0_MASK;

	}
	return (0);
}

/*
 * decodes 4 bit primary (bit 63-60) or secondary (bit 59-56) error
 * in PBM AFSR
 */
static int
log_pbm_afsr_err(uint32_t e)
{
	int nerr = 0;
	if (e & PSYCHO_PCI_AFSR_E_MA) {
		nerr++;
		cmn_err(CE_CONT, "Master Abort\n");
	}
	if (e & PSYCHO_PCI_AFSR_E_TA)
		cmn_err(CE_CONT, "Target Abort\n");
	if (e & PSYCHO_PCI_AFSR_E_RTRY) {
		nerr++;
		cmn_err(CE_CONT, "Excessive Retries\n");
	}
	if (e & PSYCHO_PCI_AFSR_E_PERR) {
		nerr++;
		cmn_err(CE_CONT, "Parity Error\n");
	}
	return (nerr);
}

#define	PBM_AFSR_TO_PRIERR(afsr)	\
	(afsr >> PSYCHO_PCI_AFSR_PE_SHIFT & PSYCHO_PCI_AFSR_E_MASK)
#define	PBM_AFSR_TO_SECERR(afsr)	\
	(afsr >> PSYCHO_PCI_AFSR_SE_SHIFT & PSYCHO_PCI_AFSR_E_MASK)
#define	PBM_AFSR_TO_BYTEMASK(afsr)	\
	((afsr & PSYCHO_PCI_AFSR_BYTEMASK) >> PSYCHO_PCI_AFSR_BYTEMASK_SHIFT)

int
pci_fault(enum pci_fault_ops op, void *arg)
{
	pbm_t *pbm_p = ((pci_t *)arg)->pci_pbm_p;
	dev_info_t *dip = ((pci_t *)arg)->pci_dip;

	ushort_t pci_cfg_stat	= pbm_p->pbm_config_header->ch_status_reg;
	uint64_t pbm_ctl_stat	= *pbm_p->pbm_ctrl_reg;
	uint64_t pbm_afsr	= *pbm_p->pbm_async_flt_status_reg;
	uint64_t pbm_afar	= *pbm_p->pbm_async_flt_addr_reg;

	int nerr = 0;
	char nm[24];
	uint32_t e;
	(void) sprintf(nm, "%s-%d", ddi_driver_name(dip),
		ddi_get_instance(dip));

	DEBUG4(DBG_BUS_FAULT, dip, "pbm_ctl_stat=%x.%8x pbm_afsr=%x.%8x\n",
		(uint_t)(pbm_ctl_stat >> 32), (uint_t)pbm_ctl_stat,
		(uint_t)(pbm_afsr >> 32), (uint_t)pbm_afsr);
	DEBUG4(DBG_BUS_FAULT, dip, "op=%x pbm_afar=%x.%8x pci_cfg_stat=%4x\n",
		op, (uint_t)(pbm_afar >> 32), (uint_t)pbm_afar, pci_cfg_stat);

	switch (op) {
	case FAULT_LOG:
		cmn_err(CE_WARN, "%s: PCI fault log start:\n", nm);

		/* 1. log PBM control status reg error bits */
		if (pbm_ctl_stat & COMMON_PCI_CTRL_SERR) {
			nerr++;
			cmn_err(CE_CONT, "PCI SERR\n");
		}
		if (pbm_ctl_stat & COMMON_PCI_CTRL_SBH_ERR) {
			if (pci_panic_on_sbh_errors)
				nerr++;
			cmn_err(CE_CONT, "PCI streaming byte hole error\n");
		}

		/* 2. log PBM AFSR error bits */
		if (pbm_afsr & PSYCHO_PCI_AFSR_BLK)
			cmn_err(CE_CONT, "Transaction was a block operation\n");
		else
			cmn_err(CE_CONT, "bytemask=%x",
				(int)PBM_AFSR_TO_BYTEMASK(pbm_afsr));

		e = PBM_AFSR_TO_PRIERR(pbm_afsr);
		cmn_err(CE_WARN, "%s: PCI primary error (%x):\n", nm, e);
		nerr += log_pbm_afsr_err(e);

		/*
		 * secondary errors are counted towards nerr and are allowed
		 * to panic the system.
		 */
		e = PBM_AFSR_TO_SECERR(pbm_afsr);
		cmn_err(CE_WARN, "%s: PCI secondary error (%x):\n", nm, e);
		nerr += log_pbm_afsr_err(e);

		/* 3. log PBM AFAR */
		cmn_err(CE_WARN, "%s: PBM AFAR %x.%08x:\n", nm,
			(uint_t)(pbm_afar >> 32), (uint_t)pbm_afar);

		/* 4. log PBM PCI config space status reg error bits */
		cmn_err(CE_WARN, "%s: PCI config space error status (%x):\n",
			nm, pci_cfg_stat);
		nerr += log_pci_cfg_err(pci_cfg_stat, 0);

		/* 5. special case */
		if ((nerr == 1) && (pbm_ctl_stat & COMMON_PCI_CTRL_SERR) &&
			(pci_cfg_stat & PCI_STAT_S_TARG_AB)) {
			cmn_err(CE_CONT, "non-fatal error\n");
			nerr = 0;
		}

		cmn_err(CE_CONT, "%s: PCI fault log end.\n", nm);
		return (nerr);

	case FAULT_POKEFINI:
	case FAULT_RESET:
		DEBUG0(DBG_BUS_FAULT, dip, "cleaning up fault bits...\n");
		pbm_p->pbm_config_header->ch_status_reg = pci_cfg_stat;
		*pbm_p->pbm_ctrl_reg = pbm_ctl_stat;
		*pbm_p->pbm_async_flt_status_reg = pbm_afsr;
		break;

	case FAULT_POKEFLT:
		/*
		 * return non-0 value to indicate it is either not
		 * a poke fault or more than just a poke fault.
		 *
		 * 1. Make sure only MA on primary.
		 * 2. Make sure no err on secondary.
		 * 3. check pci config header stat reg to see MA is logged.
		 *	We cannot verify only MA is recorded since it gets
		 *	much more complicated when a PCI-to-PCI bridge is
		 *	present.
		 */
		e = PBM_AFSR_TO_PRIERR(pbm_afsr);
		if (e != PSYCHO_PCI_AFSR_E_MA)
			return (1);
		if (PBM_AFSR_TO_SECERR(pbm_afsr))
			return (1);
		if (!(pci_cfg_stat & PCI_STAT_R_MAST_AB))
			return (1);
		break;

	default:
#ifndef DEBUG
		cmn_err(CE_WARN, "%s: pci_fault: unknown op %x\n", nm, op);
#endif
		break;
	}
#ifdef DEBUG
	if (op != 0xbad)
		(void) pci_fault(0xbad, arg);
#endif
	return (DDI_SUCCESS);
}

void
pbm_clear_error(pbm_t *pbm_p)
{
	uint64_t l;

	l = *pbm_p->pbm_async_flt_status_reg;
	while ((l >> PSYCHO_PCI_AFSR_PE_SHIFT) & PSYCHO_PCI_AFSR_E_MA)
		l = *pbm_p->pbm_async_flt_status_reg;
}
