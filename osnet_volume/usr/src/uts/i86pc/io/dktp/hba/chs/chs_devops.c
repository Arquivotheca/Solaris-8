/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)chs_devops.c	1.7	99/05/20 SMI"

#include "chs.h"

/* The name of the device, for identify */
#define	HBANAME	"chs"

/*
 * Local static data
 */

/* Autoconfiguration routines */

/*
 * identify(9E).  See if driver matches this dev_info node.
 * Return DDI_IDENTIFIED if ddi_get_name(devi) matches your
 * name, otherwise return DDI_NOT_IDENTIFIED.
 */

int
chs_identify(dev_info_t *devi)
{
	char *dname = ddi_get_name(devi);

	MDBG3(("chs_identify\n"));

	if (strcmp(dname, HBANAME) == 0)
		return (DDI_IDENTIFIED);
	else if (strcmp(dname, "pci1014,2e") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}


/*
 * probe(9E).  Examine hardware to see if HBA device is actually present.
 * Do no permanent allocations or permanent settings of device state,
 * as probe may be called more than once.
 * Return DDI_PROBE_SUCCESS if device is present and operable,
 * else return DDI_PROBE_FAILURE.
 */

int
chs_probe(dev_info_t *devi)
{
	MDBG4(("chs_probe\n"));
	/* Check for override */
	if (chs_forceload < 0)
		return (DDI_PROBE_FAILURE);

	/* Check for a valid address and type of CHS HBA */
	if (chs_hbatype(devi, NULL, NULL, TRUE) == NULL) {
		MDBG4(("chs_probe: reg failed\n"));
		return (DDI_PROBE_FAILURE);
	}

	MDBG4(("chs_probe: okay\n"));
	return (DDI_PROBE_SUCCESS);
}

/*
 * attach(9E).  Set up all device state and allocate data structures,
 * mutexes, condition variables, etc. for device operation.  Set mt-attr
 * property for driver to indicate MT-safety.  Add interrupts needed.
 * Return DDI_SUCCESS if device is ready,
 * else return DDI_FAILURE.
 */

/*
 * _attach() is serialized and single-threaded for all the channels, simply
 * because interrupts are shared by all the channels and we should disable
 * the interrupts at the beginning of _attach() and enable them at the end
 * of it.
 */
int
chs_attach(dev_info_t	*dip,
		ddi_attach_cmd_t cmd)
{
	chs_unit_t 	*unit;
	chs_hba_t 	*hba;
	scsi_hba_tran_t	*scsi_tran;

	ASSERT(dip != NULL);
	switch (cmd) {
	case DDI_ATTACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	/* Make a chs_unit_t and chs_hba_t instance for this HBA */
	unit = (chs_unit_t *)kmem_zalloc(sizeof (*unit) + sizeof (*hba),
	    KM_NOSLEEP);
	if (unit == NULL)
		return (DDI_FAILURE);

	hba = (chs_hba_t *)(unit + 1);
	unit->hba = hba;
	hba->dip = dip;

	/* find card structure */
	mutex_enter(&chs_global_mutex);
	hba->chs = chs_cardfind(dip, unit);

	if (!hba->chs) {
		kmem_free(unit, sizeof (*unit) + sizeof (*hba));
		mutex_exit(&chs_global_mutex);
		return (DDI_FAILURE);
	}

	/* initialize card structure if needed */
	if (!(hba->chs->flags & CHS_CARD_CREATED)) {
		if (!chs_cardinit(dip, unit)) {
			chs_carduninit(dip, unit);
			mutex_exit(&chs_global_mutex);
			return (DDI_FAILURE);
		}
	} else {
		/* enter card mutex */
		mutex_enter(&hba->chs->mutex);
	}

	/* fail attach of nonexistent nodes */
	if (hba->chn == CHS_DAC_CHN_NUM) {
#ifndef MSCSI_FEATURE
		/*
		 * When the mscsi bus child nexus driver is used,
		 * system drive instance must persist until all scsi
		 * channels are probed.
		 */
		if (!chs_get_nsd(hba->chs)) {
			MDBG3(("chs_attach: causing attach failure to "
				"unload virtual channel instance"));
			mutex_exit(&hba->chs->mutex);
			chs_carduninit(dip, unit);
			mutex_exit(&chs_global_mutex);
			return (DDI_FAILURE);
		}
#endif
		hba->flags = CHS_HBA_DAC;
	} else if (hba->chn >= hba->chs->nchn) {
		mutex_exit(&hba->chs->mutex);
		chs_carduninit(dip, unit);
		mutex_exit(&chs_global_mutex);
		return (DDI_FAILURE);
	}

	/*
	 * check chs.conf properties, set defaults.
	 */
	if (!chs_propinit(dip, unit)) {
		mutex_exit(&hba->chs->mutex);
		chs_carduninit(dip, unit);
		mutex_exit(&chs_global_mutex);
		return (DDI_FAILURE);
	}

	/* allocate transport */
	scsi_tran = scsi_hba_tran_alloc(dip, 0);
	unit->scsi_tran = scsi_tran;
	if (scsi_tran == NULL) {
		mutex_exit(&hba->chs->mutex);
		chs_carduninit(dip, unit);
		mutex_exit(&chs_global_mutex);
		cmn_err(CE_WARN, "chs_attach: failed to alloc scsi tran");
		return (DDI_FAILURE);
	}

	scsi_tran->tran_hba_private = unit;
	scsi_tran->tran_tgt_private = NULL;
	scsi_tran->tran_tgt_init = chs_tgt_init;
	scsi_tran->tran_tgt_free = chs_tgt_free;

	if (CHS_DAC(hba)) {
		unit->dac_unit.lkarg = (void *)hba->chs->iblock_cookie;
	} else {
		scsi_tran->tran_tgt_probe = scsi_hba_probe;
		scsi_tran->tran_start = chs_transport;
		scsi_tran->tran_reset = chs_reset;
		scsi_tran->tran_abort = chs_abort;
		scsi_tran->tran_getcap = chs_getcap;
		scsi_tran->tran_setcap = chs_setcap;
		scsi_tran->tran_init_pkt = chs_init_pkt;
		scsi_tran->tran_destroy_pkt = chs_destroy_pkt;
		scsi_tran->tran_dmafree = chs_dmafree;
		scsi_tran->tran_sync_pkt = chs_sync_pkt;
	}

	if (scsi_hba_attach(dip,
	    CHS_DAC(hba) ? &chs_dac_dma_lim : &chs_dma_lim,
	    scsi_tran, SCSI_HBA_TRAN_CLONE, NULL) != DDI_SUCCESS) {

		mutex_exit(&hba->chs->mutex);
		chs_carduninit(dip, unit);
		mutex_exit(&chs_global_mutex);
		cmn_err(CE_WARN, "chs_attach: "
				"failed to scsi_hba_attach(%p)", (void*)dip);
		return (DDI_FAILURE);
	}

	ddi_report_dev(dip);

	/*
	 * Although we can reset the channel here it is not safe to do so,
	 * in case the System-Drive hba is operational.
	 */
	hba->callback_id = 0;
	hba->flags |= CHS_HBA_ATTACHED;

	CHS_ENABLE_INTR(hba->chs);
	mutex_exit(&hba->chs->mutex);
	mutex_exit(&chs_global_mutex);

	return (DDI_SUCCESS);
}

/*
 * detach(9E).  Remove all device allocations and system resources;
 * disable device interrupts.
 * Return DDI_SUCCESS if done; DDI_FAILURE if there's a problem.
 */
int
chs_detach(dev_info_t	*dip,
		ddi_detach_cmd_t cmd)
{
	register chs_unit_t *unit;
	register chs_hba_t *hba;
	register scsi_hba_tran_t *scsi_tran;

	ASSERT(dip != NULL);

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	scsi_tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (scsi_tran == NULL)
		return (DDI_SUCCESS);

	unit = CHS_SCSI_TRAN2UNIT(scsi_tran);
	if (unit == NULL)
		return (DDI_SUCCESS);

	MDBG1(("chs_detach: dip=%p, unit=%p", (void*)dip, (void*)unit));
	hba = unit->hba;
	ASSERT(hba != NULL);

	mutex_enter(&chs_global_mutex);
	if ((hba->refcount-1) > 0) {
		mutex_exit(&chs_global_mutex);
		return (DDI_FAILURE);
	}

	mutex_destroy(&hba->mutex);
	ddi_prop_remove_all(dip);
	if (scsi_hba_detach(dip) != DDI_SUCCESS)
		cmn_err(CE_WARN, "chs_detach: "
				"failed to scsi_hba_detach(%p)", (void*)dip);

	chs_carduninit(dip, unit);
	mutex_exit(&chs_global_mutex);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
chs_flush_cache(register dev_info_t *const dip, const ddi_reset_cmd_t cmd)
{
	register chs_unit_t *unit;
	register scsi_hba_tran_t *scsi_tran;
	register chs_hba_t *hba;


	ASSERT(dip != NULL);

	if (cmd != (ddi_reset_cmd_t)DDI_DETACH)
		return (DDI_FAILURE);

	scsi_tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (scsi_tran == NULL)
		return (DDI_SUCCESS);
	unit = CHS_SCSI_TRAN2UNIT(scsi_tran);
	if (unit == NULL)
		return (DDI_SUCCESS);

	MDBG1(("chs_flush_cache: dip=%p, unit=%p", (void*)dip, (void*)unit));

	hba = unit->hba;
	ASSERT(hba != NULL);

	return (CHS_SCSI(hba) ? 0 :
	    chs_dacioc(unit, CHS_DACIOC_FLUSH, NULL, FKIOCTL));
}

/*ARGSUSED*/
bool_t
chs_propinit(dev_info_t	*dip,
		chs_unit_t	*unitp)
{
/*	set up default customary properties 				*/
	if (unitp->hba->chn == CHS_DAC_CHN_NUM) {
		if (chs_prop_default(dip, "flow_control", "dmult") == 0 ||
		    chs_prop_default(dip, "queue", (caddr_t)"qfifo") == 0 ||
#ifdef MSCSI_FEATURE
			/*
			 * when the mscsi bus nexus child driver is used, the
			 * MSCSI_CALLPROP property must be set indicating the
			 * mscsi child should callback the chs parent devops
			 * for channel init and uninit.
			 */
		    chs_prop_default(dip, MSCSI_CALLPROP, (caddr_t)"y") == 0 ||
#endif
		    chs_prop_default(dip, "disk", (caddr_t)"dadk") == 0)
			return (FALSE);
	} else if (chs_prop_default(dip, "flow_control", "dsngl") == 0 ||
		chs_prop_default(dip, "queue", (caddr_t)"qsort") == 0 ||
		chs_prop_default(dip, "tape", (caddr_t)"sctp") == 0 ||
		chs_prop_default(dip, "tag_fctrl", (caddr_t)"adapt") == 0 ||
		chs_prop_default(dip, "tag_queue", (caddr_t)"qtag") == 0) {
			return (FALSE);
	}

	MDBG5(("chs_propinit: okay\n"));
	return (TRUE);
}

/*
 * chs_prop_default: set default property if unset.
 * if plen > 0, search globally, and add non-string property.
 */
bool_t
chs_prop_default(dev_info_t	*dip,
			caddr_t		 propname,
			caddr_t		 propdefault)
{
	caddr_t	val;
	int len;

	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
		propname, (caddr_t)&val, &len) == DDI_PROP_SUCCESS) {
		kmem_free(val, len);
		return (TRUE);
	}

	if (ddi_prop_create(DDI_DEV_T_NONE, dip, 0, propname, propdefault,
		strlen(propdefault) + 1) != DDI_PROP_SUCCESS) {
		MDBG4(("chs_prop_default: property create failed %s=%s\n",
			propname, propdefault));
		return (FALSE);
	}
	return (TRUE);
}

chs_t *
chs_cardfind(dev_info_t	*dip,
		chs_unit_t	*unit)
{
	register chs_t *chs;
	register chs_t *chs_prev;
	int chn;
	int len;
	int *regp;
	int reglen;
	dev_info_t *pdip = dip;
	chs_t tchs;

#ifdef MSCSI_FEATURE
	/*
	 * when the mscsi bus nexus child driver is used, the parent
	 * dip must be used for hardware probe functions.
	 */
	if (strcmp(MSCSI_NAME, ddi_get_name(dip)) == 0)
		pdip = ddi_get_parent(dip);
#endif

	/* Determine type of chs card */
	if ((tchs.ops = chs_hbatype(pdip, &regp, &reglen, FALSE)) == NULL) {
		MDBG4(("chs_cardfind: no hbatype match\n"));
		return (NULL);
	}

	/* Determine physical ioaddr from reg of appropriate dip */
	tchs.dip = pdip;
	if (CHS_RNUMBER(&tchs, regp, reglen) < 0) {
		MDBG4(("chs_cardfind: no reg match\n"));
		return (NULL);
	}

	/* Determine which channel the HBA should use */
	len = sizeof (chn);
	if (HBA_INTPROP(dip, MSCSI_BUSPROP, &chn, &len) != DDI_PROP_SUCCESS) {
		len = sizeof (chn);
		if (ddi_getlongprop_buf(DDI_DEV_T_NONE, ddi_get_parent(dip),
		    DDI_PROP_DONTPASS, MSCSI_BUSPROP, (caddr_t)&chn, &len) !=
		    DDI_PROP_SUCCESS) {
			/*
			 * If no MSCSI_BUSPROP property exists,
			 * set default channel based on ops and reg property
			 */
			chn = CHS_CHN(&tchs, *regp);
		}
		len = sizeof (chn);
		if (ddi_prop_create(DDI_DEV_T_NONE, dip, 0, "mscsi-bus",
			(caddr_t)&chn, len) != DDI_PROP_SUCCESS) {
			MDBG4(("chs_cardfind: "
				"property create failed %s=%d\n",
					MSCSI_BUSPROP, chn));
			kmem_free(regp, reglen);
			return (NULL);
		}
	}

	/* find or add card struct of matching type and reg address  */
	for (chs_prev = chs = chs_cards;
		;
		chs_prev = chs, chs = chs->next)
		if (chs == NULL || (chs->ops == tchs.ops &&
		    chs->reg == tchs.reg))
			break;
	if (chs == NULL) {
		chs = (chs_t *)
				kmem_zalloc(sizeof (*chs), KM_NOSLEEP);
		if (chs == NULL) {
			MDBG4(("chs_cardfind: no chs\n"));
			kmem_free(regp, reglen);
			return (NULL);
		}
		chs->ops = tchs.ops;
		chs->reg = tchs.reg;
		chs->regp = regp;
		chs->reglen = reglen;
		chs->dip = chs->idip = dip;
		chs->attach_calls = -1;

		if (chs_prev != NULL)
			chs_prev->next = chs;
		else
			chs_cards = chs;
	} else
		kmem_free(regp, reglen);

	chs->refcount++;

	unit->hba->chs = chs;
	unit->hba->chn = (u_char)chn;

	return (chs);
}

/*
 * Creates and initializes an chs structure representing an IBM PCI RAID
 * card and its software state.
 */
bool_t
chs_cardinit(dev_info_t	*dip,
		chs_unit_t	*unitp)
{
	int val;
	int len;
	chs_t *chsp = unitp->hba->chs;

	/*
	 * check chs.conf properties, and initialize all potential
	 * per-target structures.  Also, initialize chip's operating
	 * registers to a known state.
	 */
	if (!chs_cfg_init(chsp))
		return (FALSE);
	chsp->flags = CHS_CARD_CREATED;


	/* setup interrupts */
	if (!chs_intr_init(chsp->idip, chsp, (caddr_t)chsp))
		return (FALSE);

	/* assert card mutex */
	ASSERT(mutex_owned(&chsp->mutex));

	chsp->flags |= (CHS_INTR_SET|CHS_INTR_IDX_SET);

	if (!CHS_INIT(chsp, dip)) {
		MDBG4(("chs_cardinit: init failed\n"));
		return (FALSE);
	}

	/* setup sema */
	sema_init(&chsp->scsi_ncdb_sema, CHS_SCSI_MAX_NCDB,
	    NULL, SEMA_DRIVER, NULL);

	/* set global property to allow access to READY drives via scsi */
	len = sizeof (val);
	if (HBA_INTPROP(dip, "ready_disks_scsi", &val, &len)
				== DDI_PROP_SUCCESS && val != 0)
		chs_disks_scsi = 1;

	/* pre-allocate ccb */
	if (chs_ccbinit(unitp->hba) == DDI_FAILURE) {
		mutex_exit(&chsp->mutex);
		return (FALSE);
	}

	/* determine the maximum channel and target */
	if (!chs_getnchn_maxtgt(chsp, unitp->hba->ccb)) {
		mutex_exit(&chsp->mutex);
		return (FALSE);
	}

	if (chs_getconf(chsp, unitp->hba->ccb) == DDI_FAILURE) {
		mutex_exit(&chsp->mutex);
		return (FALSE);
	}
	chsp->flags |= CHS_GOT_ROM_CONF;

	if (chs_getenquiry(chsp, unitp->hba->ccb) == DDI_FAILURE) {
		mutex_exit(&chsp->mutex);
		return (FALSE);
	}
	chsp->flags |= CHS_GOT_ENQUIRY;

	if (chs_ccb_stkinit(chsp) == DDI_FAILURE) {
		mutex_exit(&chsp->mutex);
		return (FALSE);
	}
	chsp->flags |= CHS_CCB_STK_CREATED;

	chsp->attach_calls++;

	return (TRUE);
}

/*
 * Frees all the non-shared resources of an chs_unit_t and enables
 * interrupts only if enable_intr is set.
 */
void
chs_carduninit(dev_info_t	*dip,
		chs_unit_t	*unitp)
{
	register chs_t *chs = unitp->hba->chs;
	register chs_hba_t *hba = unitp->hba;

	if (unitp->scsi_tran != NULL)
		scsi_hba_tran_free(unitp->scsi_tran);
	if (hba->ccb != NULL)
		ddi_iopb_free((caddr_t)hba->ccb);

	if (chs != NULL && !--chs->refcount &&
	    /* not useful info */
	    (!(chs->flags & CHS_CCB_STK_CREATED) ||
	    /* very last one */
	    chs->attach_calls >= chs->nchn)) {

		ASSERT(chs_cards != NULL);
		if (chs_cards == chs) {
			chs_cards = chs->next;
		} else {
			register chs_t *chs_tmp;
			register chs_t *chs_prev;

			for (chs_prev = chs_cards, chs_tmp = chs_cards->next;
			    chs_tmp != NULL;
			    chs_prev = chs_tmp, chs_tmp = chs_tmp->next)
				if (chs_tmp == chs)
					break;
			if (chs_tmp == NULL) {
				cmn_err(CE_WARN, "chs_unsetup: %x not in "
				    "cards list %x", (int)chs, (int)chs_cards);
				chs->refcount++;
				return;
			}
			chs_prev->next = chs->next;
		}

		if (chs->flags & CHS_GOT_ROM_CONF) {
			size_t mem = sizeof (chs_dac_conf_t) -
				sizeof (chs_dac_tgt_info_t) +
				((chs->nchn * chs->max_tgt) *
				sizeof (chs_dac_tgt_info_t));

			kmem_free((caddr_t)chs->conf, mem);
		}

		if (chs->flags & CHS_GOT_ENQUIRY) {
			kmem_free((caddr_t)chs->enq,
				(sizeof (chs_dac_enquiry_t) -
				sizeof (deadinfo) +
				((chs->nchn * (chs->max_tgt - 1)) *
				sizeof (deadinfo))));
		}

		if (chs->flags & CHS_CCB_STK_CREATED) {
			kmem_free((caddr_t)chs->ccb_stk,
			    (chs->max_cmd + 1) *
				sizeof (*chs->ccb_stk));
		}

		if (chs->flags & CHS_INTR_SET) {
			ddi_remove_intr(chs->idip, chs->intr_idx,
			    chs->iblock_cookie);
			mutex_destroy(&chs->mutex);
			sema_destroy(&chs->scsi_ncdb_sema);
		}

		if (chs->regp)
			kmem_free(chs->regp, chs->reglen);

		if (chs->flags & CHS_CARD_CREATED)
			CHS_UNINIT(chs, dip);

		/*
		 * No need to reset all the channels the F/W
		 * takes care of that.
		 */
		kmem_free((caddr_t)chs, sizeof (*chs));

		/*
		 * No need to set unitp->hba->chs to NULL,
		 * the whole hba and unit is going to be freed.
		 */
	} else {
		/* re-enable intrs */
		CHS_ENABLE_INTR(chs);
	}
	kmem_free((caddr_t)unitp, sizeof (*unitp) + sizeof (*hba));
}
