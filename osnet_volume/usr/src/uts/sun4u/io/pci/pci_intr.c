/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_intr.c	1.15	99/06/30 SMI"

/*
 * PCI nexus interrupt handling:
 *	get_intrspec implementation
 *	add_intrspec implementation
 *	PCI device interrupt handler wrapper
 *	pil lookup routine
 *	PCI device interrupt related initchild code
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/spl.h>
#include <sys/sunddi.h>
#include <sys/ivintr.h>
#include <sys/machsystm.h>	/* e_ddi_nodeid_to_dip() */
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>

#ifdef _STARFIRE
#include <sys/starfire.h>

int pc_translate_tgtid(caddr_t, int, volatile uint64_t *);
#endif /* _STARFIRE */

/*LINTLIBRARY*/

static uint_t pci_intr_wrapper(caddr_t arg);

#ifdef NOT_DEFINED
/*
 * This array is used to determine the sparc PIL at the which the
 * handler for a given INO will execute.  This table is for onboard
 * devices only.  A different scheme will be used for plug-in cards.
 */

uint_t ino_to_pil[] = {

	/* pil */		/* ino */

	0, 0, 0, 0,  		/* 0x00 - 0x03: bus A slot 0 int#A, B, C, D */
	0, 0, 0, 0,		/* 0x04 - 0x07: bus A slot 1 int#A, B, C, D */
	0, 0, 0, 0,  		/* 0x08 - 0x0B: unused */
	0, 0, 0, 0,		/* 0x0C - 0x0F: unused */

	0, 0, 0, 0,  		/* 0x10 - 0x13: bus B slot 0 int#A, B, C, D */
	0, 0, 0, 0,		/* 0x14 - 0x17: bus B slot 1 int#A, B, C, D */
	0, 0, 0, 0,  		/* 0x18 - 0x1B: bus B slot 2 int#A, B, C, D */
	4, 0, 0, 0,		/* 0x1C - 0x1F: bus B slot 3 int#A, B, C, D */

	4,			/* 0x20: SCSI */
	6,			/* 0x21: ethernet */
	3,			/* 0x22: parallel port */
	9,			/* 0x23: audio record */
	9,			/* 0x24: audio playback */
	14,			/* 0x25: power fail */
	4,			/* 0x26: 2nd SCSI */
	8,			/* 0x27: floppy */
	14,			/* 0x28: thermal warning */
	12,			/* 0x29: keyboard */
	12,			/* 0x2A: mouse */
	12,			/* 0x2B: serial */
	0,			/* 0x2C: timer/counter 0 */
	0,			/* 0x2D: timer/counter 1 */
	14,			/* 0x2E: uncorrectable ECC errors */
	14,			/* 0x2F: correctable ECC errors */
	14,			/* 0x30: PCI bus A error */
	14,			/* 0x31: PCI bus B error */
	14,			/* 0x32: power management wakeup */
	14,			/* 0x33 */
	14,			/* 0x34 */
	14,			/* 0x35 */
	14,			/* 0x36 */
	14,			/* 0x37 */
	14,			/* 0x38 */
	14,			/* 0x39 */
	14,			/* 0x3a */
	14,			/* 0x3b */
	14,			/* 0x3c */
	14,			/* 0x3d */
	14,			/* 0x3e */
	14,			/* 0x3f */
	14			/* 0x40 */
};
#endif /* NOT_DEFINED */


#define	PCI_SIMBA_VENID		0x108e	/* vendor id for simba */
#define	PCI_SIMBA_DEVID		0x5000	/* device id for simba */

/*
 * map_pcidev_cfg_reg - create mapping to pci device configuration registers
 *			if we have a simba AND a pci to pci bridge along the
 *			device path.
 *			Called with corresponding mutexes held!!
 *
 * XXX	  XXX	XXX	The purpose of this routine is to overcome a hardware
 *			defect in Sabre CPU and Simba bridge configuration
 *			which does not drain DMA write data stalled in
 *			PCI to PCI bridges (such as the DEC bridge) beyond
 *			Simba. This routine will setup the data structures
 *			to allow the pci_intr_wrapper to perform a manual
 *			drain data operation before passing the control to
 *			interrupt handlers of device drivers.
 * return value:
 * DDI_SUCCESS
 * DDI_FAILURE		if unable to create mapping
 */
static int
map_pcidev_cfg_reg(dev_info_t *dip, dev_info_t *rdip, ddi_acc_handle_t *hdl_p)
{
	dev_info_t *cdip;
	dev_info_t *pci_dip = NULL;
#ifdef DEBUG
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
#endif
	int simba_found = 0, pci_bridge_found = 0;

	for (cdip = rdip; cdip && cdip != dip; cdip = ddi_get_parent(cdip)) {
		ddi_acc_handle_t config_handle;
		uint32_t vendor_id = ddi_getprop(DDI_DEV_T_ANY, cdip,
			DDI_PROP_DONTPASS, "vendor-id", 0xffff);

		DEBUG4(DBG_A_ISPEC, pci_p->pci_dip,
			"map dev cfg reg for %s%d: @%s%d\n",
			ddi_driver_name(rdip), ddi_get_instance(rdip),
			ddi_driver_name(cdip), ddi_get_instance(cdip));

		if (ddi_getprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
				"no-dma-interrupt-sync", -1) == -1)
			continue;

		/* continue to search up-stream if not a PCI device */
		if (vendor_id == 0xffff)
			continue;

		/* record the deepest pci device */
		if (!pci_dip)
			pci_dip = cdip;

		/* look for simba */
		if (vendor_id == PCI_SIMBA_VENID) {
			uint32_t device_id = ddi_getprop(DDI_DEV_T_ANY,
			    cdip, DDI_PROP_DONTPASS, "device-id", -1);
			if (device_id == PCI_SIMBA_DEVID) {
				simba_found = 1;
				DEBUG0(DBG_A_ISPEC, pci_p->pci_dip,
					"\tFound simba\n");
				continue; /* do not check bridge if simba */
			}
		}

		/* look for pci to pci bridge */
		if (pci_config_setup(cdip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
			    "%s%d: can't get brdg cfg space for %s%d\n",
				ddi_driver_name(dip), ddi_get_instance(dip),
				ddi_driver_name(cdip), ddi_get_instance(cdip));
			return (DDI_FAILURE);
		}
		if (pci_config_get8(config_handle, PCI_CONF_BASCLASS)
		    == PCI_CLASS_BRIDGE) {
			DEBUG0(DBG_A_ISPEC, pci_p->pci_dip,
				"\tFound PCI to xBus bridge\n");
			pci_bridge_found = 1;
		}
		pci_config_teardown(&config_handle);
	}

	if (simba_found && pci_bridge_found &&
		pci_config_setup(pci_dip, hdl_p) != DDI_SUCCESS) {

		cmn_err(CE_WARN, "%s%d: can not get config space for %s%d\n",
			ddi_driver_name(dip), ddi_get_instance(dip),
			ddi_driver_name(cdip), ddi_get_instance(cdip));
			return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

#ifdef lint
#define	READ_DMA_SYNC_REG(pci_p) {}
#else
#define	READ_DMA_SYNC_REG(pbm_p) \
{ \
	volatile uint64_t x = *pbm_p->pbm_dma_sync_reg; \
	DEBUG1(DBG_DMASYNC_FLUSH, pci_p->pci_dip, \
		"dma_sync_flush_reg addr: %x\n", pbm_p->pbm_dma_sync_reg); \
}
#endif

/*
 * pci_intr_wrapper
 *
 * This routine is used as wrapper around interrupt handlers installed by child
 * device drivers.  This routine invokes the associated interrupt handler then
 * clears the corresponding interrupt through the interrupt's clear interrupt
 * register.
 *
 * return value: DDI_INTR_CLAIMED if any handlers claimed the interrupt,
 *	DDI_INTR_UNCLAIMED otherwise.
 */
static uint_t
pci_intr_wrapper(caddr_t arg)
{
	ib_ino_info_t *ino_p = (ib_ino_info_t *)arg;
	uint_t result = 0;
	pci_t *pci_p = ino_p->ino_ib_p->ib_pci_p;
	pbm_t *pbm_p = pci_p->pci_pbm_p;
	ih_t *ih_p = ino_p->ino_ih_start;
	int i;

#ifdef lint
	pbm_p = pbm_p;
#endif
	for (i = 0; i < ino_p->ino_ih_size; i++, ih_p = ih_p->ih_next) {

		/* perform the DMA drain flush protocol -darwin kludge */
		if (ih_p->ih_config_handle) {
			(void) pci_config_get16(ih_p->ih_config_handle,
				PCI_CONF_VENID);
			READ_DMA_SYNC_REG(pbm_p);
		}
		result += (*ih_p->ih_handler)(ih_p->ih_handler_arg);
		if (pci_check_all_handlers)
			continue;
		if (result)
			break;
	}
	ino_p->ino_ih_start = ih_p;

	/* Clear the interrupt */
	IB_INO_INTR_CLEAR(ino_p->ino_clr_reg);

	return (result ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
}

dev_info_t *
get_my_childs_dip(dev_info_t *dip, dev_info_t *rdip)
{
	dev_info_t *cdip = rdip;

	for (; ddi_get_parent(cdip) != dip; cdip = ddi_get_parent(cdip))
		;

	return (cdip);
}

/*
 * iline_to_pil
 *
 * This routine returns a sparc pil for a given PCI device.  The routine
 * read the class code and sub class code from the devices configuration
 * header and uses this information to derive the pil.
 *
 * used by: pci_get_ispec_impl()
 *
 * return value: sparc pil for the given device
 */
uint_t
iline_to_pil(dev_info_t *rdip)
{
	int class_code;
	uchar_t base_class_code;
	uchar_t sub_class_code;
	uint_t pil = 0;

	/*
	 * Use the "class-code" property to get the base and sub class
	 * codes for the requesting device.
	 */
	class_code = ddi_prop_get_int(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
					"class-code", -1);
	if (class_code != -1) {
		base_class_code = ((uint_t)class_code & 0xff0000) >> 16;
		sub_class_code = ((uint_t)class_code & 0xff00) >> 8;
	} else {
		cmn_err(CE_WARN, "%s%d has no class-code property\n",
			ddi_driver_name(rdip), ddi_get_instance(rdip));
		return (pil);
	}

	/*
	 * Use the class code values to construct an pil for the device.
	 */
	switch (base_class_code) {
	default:
	case PCI_CLASS_NONE:
		pil = 1;
		break;

	case PCI_CLASS_MASS:
		switch (sub_class_code) {
		case PCI_MASS_SCSI:
		case PCI_MASS_IDE:
		case PCI_MASS_FD:
		case PCI_MASS_IPI:
		case PCI_MASS_RAID:
		default:
			pil = 0x4;
			break;
		}
		break;

	case PCI_CLASS_NET:
		pil = 0x6;
		break;

	case PCI_CLASS_DISPLAY:
		pil = 0x9;
		break;

	case PCI_CLASS_MM:
		pil = 0xb;
		break;

	case PCI_CLASS_MEM:
		pil = 0xb;
		break;

	case PCI_CLASS_BRIDGE:
		switch (sub_class_code) {
		case PCI_BRIDGE_HOST:
		case PCI_BRIDGE_ISA:
		case PCI_BRIDGE_EISA:
		case PCI_BRIDGE_MC:
		case PCI_BRIDGE_PCI:
		case PCI_BRIDGE_PCMCIA:
		case PCI_BRIDGE_CARDBUS:
		default:
			pil = 0xb;
		}
		break;

	case PCI_CLASS_SERIALBUS:
		switch (sub_class_code) {
		case PCI_SERIAL_FIRE:
			pil = 0x9;
			break;
		case PCI_SERIAL_ACCESS:
		case PCI_SERIAL_SSA:
			pil = 0x4;
			break;
		case PCI_SERIAL_USB:
			pil = 0x9;
			break;
		case PCI_SERIAL_FIBRE:
			pil = 0x6;
			break;
		default:
			pil = 0x4;
			break;
		}
		break;
	}
	return (pil);
}


int
pci_add_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	ib_t *ib_p = pci_p->pci_ib_p;
	ih_t *ih_p = NULL;
	ddi_ispec_t *ip = (ddi_ispec_t *)intr_info->ii_ispec;
	ib_ino_t ino = IB_MONDO_TO_INO(*ip->is_intr); /* trim pulse&upa bits */
	ib_ino_info_t *ino_p;
	ib_mondo_t mondo;
	uint32_t cpu_id;
	uint32_t pil;

	DEBUG2(DBG_A_ISPEC, dip, "rdip=%s%d add_ispec_impl()\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip));
	DEBUG3(DBG_A_ISPEC, dip, "intrspec=%x pri=%x interrupt=%x\n",
		ip, ip->is_pil, *ip->is_intr);
	DEBUG2(DBG_A_ISPEC, dip, "handler=%x arg=%x\n",
		intr_info->ii_int_handler, intr_info->ii_int_handler_arg);

	if (ino > ib_p->ib_max_ino) {
		DEBUG1(DBG_A_ISPEC, dip, "ino %x is invalid\n", ino);
		return (DDI_INTR_NOTFOUND);
	}

	if (*ip->is_intr & PCI_PULSE_INO)	{ /* pulse interrupt */
		uint64_t *map_reg_addr;
		int id;
		uint64_t tmp;

		/* Get the portid of the interrupt requestor */
		if ((id = pci_get_portid(rdip)) == -1)
			goto fail1;

		/* Translate the interrupts property */
		mondo = ino | (id << 6);
		/* Store the translated interrupts for our parent */
		*ip->is_intr = mondo;

		if (i_ddi_intr_ctlops(dip, rdip, DDI_INTR_CTLOPS_ADD,
		    (void *)intr_info, (void *) NULL) != DDI_SUCCESS)
			goto fail1;

		mutex_enter(&intr_dist_lock);
		cpu_id = intr_add_cpu(ib_intr_dist, ib_p,
		    mondo, 0);
		mutex_exit(&intr_dist_lock);

		map_reg_addr = ib_intr_map_reg_addr(ib_p, ino);
		*map_reg_addr = ib_get_map_reg(mondo, cpu_id);
		tmp = *map_reg_addr;
#ifdef lint
		tmp = tmp;
#endif lint
		pil = ip->is_pil;
	} else {
		mutex_enter(&ib_p->ib_ino_lst_mutex);

		mondo = pci_xlate_intr(dip, rdip, ib_p, ino);
		if (mondo == 0)
			goto fail1;
		ino = IB_MONDO_TO_INO(mondo);

		ino_p = ib_locate_ino(ib_p, ino);
		if (ino_p && ib_ino_locate_intr(ino_p, rdip,
		    intr_info->ii_inum)) {
			DEBUG0(DBG_A_ISPEC, dip, "duplicate intr\n");
			goto fail2;
		}

		ih_p = ib_alloc_ih(rdip, intr_info->ii_inum,
		    intr_info->ii_int_handler, intr_info->ii_int_handler_arg);

		if (map_pcidev_cfg_reg(dip, rdip, &ih_p->ih_config_handle))
			goto fail3;

		if (ino_p) { /* sharing ino */
			ib_ino_add_intr(ino_p, ih_p);
			pil = ino_p->pil;
		} else {
			ino_p = ib_new_ino(ib_p, ino, ih_p);
			/* If we don't have an assigned PIL, try and set one */
			if (ip->is_pil == 0)
				ip->is_pil = iline_to_pil(rdip);
			/*
			 * Translate the interrupt before we pass it to the
			 * parent.
			 */
			*ip->is_intr = mondo;

			/* Set up the PCI wrapper routine */
			intr_info->ii_int_handler = pci_intr_wrapper;
			intr_info->ii_int_handler_arg = (caddr_t)ino_p;
			/*
			 * Install the nexus driver interrupt handler
			 * for this INO.
			 */
			if (i_ddi_intr_ctlops(dip, rdip, DDI_INTR_CTLOPS_ADD,
			    (void *)intr_info, (void *) NULL) != DDI_SUCCESS)
				goto fail4;

			mutex_enter(&intr_dist_lock);
			cpu_id = intr_add_cpu(ib_intr_dist, ib_p, mondo, 0);
			mutex_exit(&intr_dist_lock);

			/* Save the pil for this ino */
			pil = ino_p->pil = ip->is_pil;

			/* clear and enable interrupt */
			IB_INO_INTR_CLEAR(ino_p->ino_clr_reg);
#ifdef _STARFIRE
			cpu_id = pc_translate_tgtid(ib_p->ib_ittrans_cookie,
			    cpu_id, ino_p->ino_map_reg);
#endif /* _STARFIRE */
			*ino_p->ino_map_reg = ib_get_map_reg(mondo, cpu_id);
		}

		ib_ino_map_reg_share(ib_p, ino, ino_p);
		mutex_exit(&ib_p->ib_ino_lst_mutex);
	}

	if (intr_info->ii_iblock_cookiep)
		*intr_info->ii_iblock_cookiep = (ddi_iblock_cookie_t)pil;
	if (intr_info->ii_idevice_cookiep) {
		intr_info->ii_idevice_cookiep->idev_vector = mondo;
		intr_info->ii_idevice_cookiep->idev_priority = pil;
	}

	DEBUG3(DBG_A_ISPEC, dip, "done! Interrupt 0x%x pil=%x mondo=%x\n",
	    (uint32_t)*ip->is_intr, pil, mondo);

	return (DDI_SUCCESS);

fail4:
	intr_info->ii_int_handler = ih_p->ih_handler;
	intr_info->ii_int_handler_arg = ih_p->ih_handler_arg;
	if (ih_p->ih_config_handle)
		pci_config_teardown(ih_p->ih_config_handle);
	ib_delete_ino(ib_p, ino_p);
fail3:
	kmem_free(ih_p, sizeof (ih_t));
fail2:
	mutex_exit(&ib_p->ib_ino_lst_mutex);
fail1:
	DEBUG3(DBG_A_ISPEC, dip,
	    "Failed! Interrupt 0x%x pil=0x%x mondo=0x%x\n",
	    *ip->is_intr, pil, mondo);
	return (DDI_FAILURE);
}


int
pci_remove_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info)
{
	ib_ino_t ino;
	ib_mondo_t mondo;
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	ib_t *ib_p = pci_p->pci_ib_p;
	ddi_ispec_t *ip = (ddi_ispec_t *)intr_info->ii_ispec;
	ib_ino_info_t *ino_p;
	ih_t *ih_p;
	int32_t shared;
	int ret = DDI_SUCCESS;
	volatile uint64_t junk;

	DEBUG2(DBG_R_ISPEC, dip, "rdip=%s%d\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip));

	ino = IB_MONDO_TO_INO(*ip->is_intr);

	if (*ip->is_intr & PCI_PULSE_INO) { /* pulse interrupt */
		uint64_t *map_reg_addr;
		int id;

		map_reg_addr = ib_intr_map_reg_addr(ib_p, ino);
		IB_INO_INTR_RESET(map_reg_addr);
		junk = *map_reg_addr;

		/* Get the portid of the interrupt requestor */
		if ((id = pci_get_portid(rdip)) == -1)
			/* Invalid portid */
			return (DDI_FAILURE);

		/* Translate the interrupt property */
		mondo = ino | (id << 6);
		*ip->is_intr = mondo;

		/* Remove the interrupt handler for this INO. */
		if (i_ddi_intr_ctlops(dip, rdip, DDI_INTR_CTLOPS_REMOVE,
		    (void *)intr_info, (void *) NULL) != DDI_SUCCESS) {
			ret = DDI_FAILURE;
			goto done;
		}

		mutex_enter(&intr_dist_lock);
		intr_rem_cpu(mondo);
		mutex_exit(&intr_dist_lock);

		DEBUG3(DBG_R_ISPEC, dip,
			"pulse intr imap-ino=%x dev-mondo=%x map_reg=%x\n",
			ino, mondo, map_reg_addr);
		goto done;
	}

	mutex_enter(&ib_p->ib_ino_lst_mutex);
	/* translate the interrupt before it passes up to it's parent */
	mondo = pci_xlate_intr(dip, rdip, ib_p, ino);
	if (mondo == 0) {
		DEBUG2(DBG_R_ISPEC, dip,
		    "Can't xlate interrupt. mondo 0x%x, ino 0x%x", mondo, ino);
		mutex_exit(&ib_p->ib_ino_lst_mutex);
		goto done;
	}
	ino = IB_MONDO_TO_INO(mondo);

	ino_p = ib_locate_ino(ib_p, ino);
	if (!ino_p) {
		DEBUG1(DBG_R_ISPEC, dip, "ino %x is invalid\n", ino);
		mutex_exit(&ib_p->ib_ino_lst_mutex);
		ret = DDI_FAILURE;
		goto done;
	}

	ih_p = ib_ino_locate_intr(ino_p, rdip, intr_info->ii_inum);
	ib_ino_rem_intr(ino_p, ih_p);
	shared = ib_ino_map_reg_still_shared(ib_p, ino, ino_p);

	if (ino_p->ino_ih_size == 0) {
		*ip->is_intr = mondo;
		/* Pass up ctlops */
		(void) i_ddi_intr_ctlops(dip, rdip, DDI_INTR_CTLOPS_REMOVE,
		    (void *) intr_info, (void *)NULL);

		mutex_enter(&intr_dist_lock);
		intr_rem_cpu(mondo);
		mutex_exit(&intr_dist_lock);

		ib_delete_ino(ib_p, ino_p);
	}

	/*
	 * re-enable interrupt only if it is still shared (chip specific).
	 */
	if (shared) {
		IB_INO_INTR_ON(ino_p->ino_map_reg);
		junk = *ino_p->ino_map_reg;
#ifdef lint
		junk = junk;
#endif lint
	}

	mutex_exit(&ib_p->ib_ino_lst_mutex);

	if (ino_p->ino_ih_size == 0) {
		kmem_free(ino_p, sizeof (ib_ino_info_t));
	}

done:
	return (ret);
}
