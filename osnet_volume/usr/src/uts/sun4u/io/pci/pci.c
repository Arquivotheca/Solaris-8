/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci.c	1.189	99/10/11 SMI"

/*
 * PCI nexus driver interface
 */
#include <sys/types.h>
#include <sys/conf.h>		/* nulldev */
#include <sys/stat.h>		/* devctl */
#include <sys/kmem.h>
#include <sys/ivintr.h>
#include <sys/async.h>		/* ecc_flt for pci_ecc.h */
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

/*
 * function prototypes for bus ops routines:
 */
static int
pci_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *addrp);
static int
pci_dma_map(dev_info_t *dip, dev_info_t *rdip,
	struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);
static int
pci_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attrp,
	int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep);
static int
pci_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle);
static int
pci_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
	ddi_dma_cookie_t *cookiep, uint_t *ccountp);
static int
pci_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle);
static int
pci_dma_flush(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, off_t off, size_t len,
	uint_t cache_flags);
static int
pci_dma_win(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, uint_t win, off_t *offp,
	size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp);
static int
pci_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result);
static int
pci_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t ctlop,
    void *arg, void *result);

/*
 * function prototypes for dev ops routines:
 */
static int pci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int pci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

/*
 * bus ops and dev ops structures:
 */
static struct bus_ops pci_bus_ops = {
	BUSO_REV,
	pci_map,
	0,
	0,
	0,
	i_ddi_map_fault,
	pci_dma_map,
	pci_dma_allochdl,
	pci_dma_freehdl,
	pci_dma_bindhdl,
	pci_dma_unbindhdl,
	pci_dma_flush,
	pci_dma_win,
	pci_dma_mctl,
	pci_ctlops,
	ddi_bus_prop_op,
	NULL,
	NULL,
	NULL,
	NULL,
	pci_intr_ctlops
};

extern struct cb_ops pci_cb_ops;

static struct dev_ops pci_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	nulldev,
	0,
	pci_attach,
	pci_detach,
	nodev,
	&pci_cb_ops,
	&pci_bus_ops
};

/*
 * module definitions:
 */
#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module - driver */
	"PCI Bus nexus driver",	/* Name of module. */
	&pci_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

/*
 * driver global data:
 */
void *per_pci_state;		/* per-pbm soft state pointer */
void *per_pci_common_state;	/* per-psycho soft state pointer */
kmutex_t pci_global_mutex;	/* attach/detach common struct lock */

int
_init(void)
{
	int e;

	/*
	 * Initialize per-pci bus soft state pointer.
	 */
	e = ddi_soft_state_init(&per_pci_state, sizeof (pci_t), 1);
	if (e != 0)
		return (e);

	/*
	 * Initialize per-psycho soft state pointer.
	 */
	e = ddi_soft_state_init(&per_pci_common_state,
	    sizeof (pci_common_t), 1);
	if (e != 0) {
		ddi_soft_state_fini(&per_pci_state);
		return (e);
	}

	/*
	 * Initialize global mutexes.
	 */
	mutex_init(&pci_global_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&dvma_active_list_mutex, NULL, MUTEX_DRIVER, NULL);

	/*
	 * Install the module.
	 */
	e = mod_install(&modlinkage);
	if (e != 0) {
		ddi_soft_state_fini(&per_pci_state);
		ddi_soft_state_fini(&per_pci_common_state);
		mutex_destroy(&dvma_active_list_mutex);
		mutex_destroy(&pci_global_mutex);
	}
	return (e);
}

int
_fini(void)
{
	int e;

	/*
	 * Remove the module.
	 */
	e = mod_remove(&modlinkage);
	if (e != 0)
		return (e);

	/*
	 * Free the per-pci and per-psycho soft state info and destroy
	 * mutex for per-psycho soft state.
	 */
	ddi_soft_state_fini(&per_pci_state);
	ddi_soft_state_fini(&per_pci_common_state);
	mutex_destroy(&dvma_active_list_mutex);
	mutex_destroy(&pci_global_mutex);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/* device driver entry points */
/*
 * attach entry point:
 */
static int
pci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	pci_t *pci_p;			/* per bus state pointer */
	int instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_ATTACH:
		DEBUG0(DBG_ATTACH, dip, "DDI_ATTACH\n");

		/*
		 * Allocate and get the per-pci soft state structure.
		 */
		if (alloc_pci_soft_state(instance) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: can't allocate pci state\n",
				ddi_driver_name(dip), instance);
			goto err_bad_pci_softstate;
		}
		pci_p = get_pci_soft_state(instance);
		pci_p->pci_dip = dip;
		mutex_init(&pci_p->pci_mutex, NULL, MUTEX_DRIVER, NULL);
		pci_p->pci_soft_state = PCI_SOFT_STATE_CLOSED;
		pci_p->pci_open_count = 0;

		/*
		 * Get key properties of the pci bridge node and
		 * determine it's type (psycho, schizo, etc ...).
		 */
		if (get_pci_properties(pci_p, dip) == DDI_FAILURE)
			goto err_bad_pci_prop;

		/*
		 * Map in the registers.
		 */
		if (map_pci_registers(pci_p, dip) == DDI_FAILURE)
			goto err_bad_reg_prop;

		/*
		 * Create the "devctl" node for hotplug support.
		 */
		if (ddi_create_minor_node(dip, "devctl", S_IFCHR, instance,
			DDI_NT_NEXUS, 0) != DDI_SUCCESS)
			goto err_bad_devctl_node;

		if (pci_obj_setup(pci_p) != DDI_SUCCESS)
			goto err_bad_objs;

		/*
		 * Initialize fault handling
		 */
		fault_init(pci_p);

		ddi_report_dev(dip);

		pci_p->pci_state = PCI_ATTACHED;
		DEBUG0(DBG_ATTACH, dip, "attach success\n");
		break;
err_bad_objs:
		ddi_remove_minor_node(dip, "devctl");
err_bad_devctl_node:
		unmap_pci_registers(pci_p);
err_bad_reg_prop:
		free_pci_properties(pci_p);
err_bad_pci_prop:
		mutex_destroy(&pci_p->pci_mutex);
		free_pci_soft_state(instance);
err_bad_pci_softstate:
		return (DDI_FAILURE);

	case DDI_RESUME:
		DEBUG0(DBG_ATTACH, dip, "DDI_RESUME\n");

		/*
		 * Make sure the Psycho control registers and IOMMU
		 * are configured properly.
		 */
		pci_p = get_pci_soft_state(instance);
		mutex_enter(&pci_p->pci_mutex);

		/*
		 * Make sure this instance has been suspended.
		 */
		if (pci_p->pci_state != PCI_SUSPENDED) {
			DEBUG0(DBG_ATTACH, dip, "instance NOT suspended\n");
			mutex_exit(&pci_p->pci_mutex);
			return (DDI_FAILURE);
		}

		pci_obj_resume(pci_p);
		pci_p->pci_state = PCI_ATTACHED;

		/*
		 * Restore child devices configuration headers
		 */
		restore_config_regs(pci_p);

		mutex_exit(&pci_p->pci_mutex);
		break;

	default:
		DEBUG0(DBG_ATTACH, dip, "unsupported attach op\n");
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * detach entry point:
 */
static int
pci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	pci_t *pci_p = get_pci_soft_state(instance);

	/*
	 * Make sure we are currently attached
	 */
	if (pci_p->pci_state != PCI_ATTACHED) {
		DEBUG0(DBG_ATTACH, dip, "failed - instance not attached\n");
		return (DDI_FAILURE);
	}

	mutex_enter(&pci_p->pci_mutex);

	switch (cmd) {
	case DDI_DETACH:
		DEBUG0(DBG_DETACH, dip, "DDI_DETACH\n");
		pci_obj_destroy(pci_p);

		/*
		 * Free the pci soft state structure and the rest of the
		 * resources it's using.
		 */
		free_pci_properties(pci_p);
		unmap_pci_registers(pci_p);
		fault_fini(pci_p);
		mutex_exit(&pci_p->pci_mutex);
		mutex_destroy(&pci_p->pci_mutex);
		free_pci_soft_state(instance);

		/* Free the interrupt-priorities prop if we created it. */
		{
			int len;

			if (ddi_getproplen(DDI_DEV_T_ANY, dip,
			    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS,
			    "interrupt-priorities", &len) == DDI_PROP_SUCCESS)
				(void) ddi_prop_remove(DDI_DEV_T_NONE, dip,
				    "interrupt-priorities");
		}

		return (DDI_SUCCESS);

	case DDI_SUSPEND:

		/*
		 * Save the state of the interrupt mapping registers.
		 */
		ib_save_intr_map_regs(pci_p->pci_ib_p);

		/*
		 * Save the state of the configuration headers of our
		 * child nodes.
		 */
		save_config_regs(pci_p);

		pci_obj_suspend(pci_p);
		pci_p->pci_state = PCI_SUSPENDED;

		mutex_exit(&pci_p->pci_mutex);
		return (DDI_SUCCESS);

	default:
		DEBUG0(DBG_DETACH, dip, "unsupported detach op\n");
		mutex_exit(&pci_p->pci_mutex);
		return (DDI_FAILURE);
	}
}


/* bus driver entry points */

/*
 * bus map entry point:
 *
 * 	if map request is for an rnumber
 *		get the corresponding regspec from device node
 * 	build a new regspec in our parent's format
 *	build a new map_req with the new regspec
 *	call up the tree to complete the mapping
 */
static int
pci_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t off, off_t len, caddr_t *addrp)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	struct regspec regspec;
	ddi_map_req_t p_map_request;
	int rnumber;
	int rval;

	/*
	 * User level mappings are not supported yet.
	 */
	if (mp->map_flags & DDI_MF_USER_MAPPING) {
		DEBUG2(DBG_MAP, dip, "rdip=%s%d: no user level mappings yet!\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		return (DDI_ME_UNIMPLEMENTED);
	}

	/*
	 * Now handle the mapping according to its type.
	 */
	switch (mp->map_type) {
	case DDI_MT_REGSPEC:

		/*
		 * We assume the register specification is in PCI format.
		 * We must convert it into a regspec of our parent's
		 * and pass the request to our parent.
		 */
		DEBUG2(DBG_MAP, dip, "rdip=%s%d: REGSPEC\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		rval = xlate_reg_prop(pci_p, rdip,
			(pci_regspec_t *)mp->map_obj.rp, off, len, &regspec);
		break;

	case DDI_MT_RNUMBER:

		/*
		 * Get the "reg" property from the device node and convert
		 * it to our parent's format.
		 */
		DEBUG3(DBG_MAP, dip, "rdip=%s%d: rnumber=%x\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    mp->map_obj.rnumber);
		rnumber = mp->map_obj.rnumber;
		if (rnumber < 0)
			return (DDI_ME_RNUMBER_RANGE);
		rval = get_reg_set(pci_p, rdip,  rnumber, off, len, &regspec);
		break;

	default:
		return (DDI_ME_INVAL);

	}
	if (rval != DDI_SUCCESS)
		return (rval);

	/*
	 * Now we have a copy of the PCI regspec converted to our parent's
	 * format.  Build a new map request based on this regspec and pass
	 * it to our parent.
	 */
	p_map_request = *mp;
	p_map_request.map_type = DDI_MT_REGSPEC;
	p_map_request.map_obj.rp = &regspec;
	return (ddi_map(dip, &p_map_request, 0, 0, addrp));
}

/*
 * bus dma map entry point
 */
static int
pci_dma_map(dev_info_t *dip, dev_info_t *rdip,
	struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	/* temporary out */
	DEBUG3(DBG_DMA_MAP, dip, "mapping - rdip=%s%d type=%s\n",
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    handlep ? "alloc" : "advisory");

	return (pci_dma_map_impl(dip, rdip, dmareq, handlep, 0));
}


/*
 * bus dma alloc handle entry point:
 */
static int
pci_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attrp,
	int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	ddi_dma_impl_t *mp;
	int rval;

	DEBUG2(DBG_DMA_ALLOCH, dip, "rdip=%s%d\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip));

	if (attrp->dma_attr_version != DMA_ATTR_V0)
		return (DDI_DMA_BADATTR);

	/*
	 * Allocate the handle.
	 */
	if (rval = pci_alloc_mp(dip, rdip, handlep, waitfp, arg, pci_p))
		return (rval);

	/*
	 * Save requestor's information
	 */
	mp = (ddi_dma_impl_t *)(*handlep);
	mp->dmai_attr		= *attrp; /* the whole thing */
	mp->dmai_minxfer	= (uint_t)attrp->dma_attr_minxfer;
	mp->dmai_burstsizes	= (uint_t)attrp->dma_attr_burstsizes;
	DEBUG1(DBG_DMA_ALLOCH, dip, "mp=%x\n", mp);

	/*
	 * Check the dma attributes.
	 */
	if (rval = check_dma_attr(pci_p, mp, attrp)) {
		pci_free_mp(mp);
		return (rval);
	}
	return (DDI_SUCCESS);
}


/*
 * bus dma free handle entry point:
 */
/*ARGSUSED*/
static int
pci_dma_freehdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));

	DEBUG3(DBG_DMA_FREEH, dip, "rdip=%s%d mp=%x\n",
	    ddi_driver_name(rdip), ddi_get_instance(rdip), handle);
	pci_free_mp((ddi_dma_impl_t *)handle);

	/*
	 * Now that we've freed some resources,
	 * if there is anybody waiting for it
	 * try and get them going.
	 */
	if (pci_p->pci_handle_pool_call_list_id != 0) {
		DEBUG0(DBG_DMA_FREEH, dip, "run handle callback\n");
		ddi_run_callback(&pci_p->pci_handle_pool_call_list_id);
	}
	return (DDI_SUCCESS);
}


/*
 * bus dma bind handle entry point:
 */
static int
pci_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
	ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	int rval;

	DEBUG3(DBG_DMA_BINDH, dip, "rdip=%s%d, mp=%x\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip), mp);
	if (mp->dmai_flags & DMAI_FLAGS_INUSE)
		return (DDI_DMA_INUSE);

	rval = pci_dma_map_impl(dip, rdip, dmareq, &handle, 1);
	DEBUG3(DBG_DMA_BINDH, dip, "rdip=%s%d, dma_map_impl rval=%x\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip), rval);
	if (rval < 0)
		return (rval);
	ASSERT(mp == handle);

	/* create dma cookies - XXX untested code path */
	if (mp->dmai_flags & DMAI_FLAGS_BYPASS) {
		pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
		iommu_t *iommu_p = pci_p->pci_iommu_p;
		pci_dmai_nexus_private_t *np =
			(pci_dmai_nexus_private_t *)mp->dmai_nexus_private;

		rval = iommu_create_bypass_cookies(iommu_p, dip, mp, cookiep);
		if (rval) {
			pci_free_np_pages(np, 1);
			DEBUG1(DBG_DMA_BINDH, dip, "freeing dma np(%x) pages\n",
				np);
			return (rval);
		}
		*ccountp = PCI_GET_NP_NCOOKIES(np);
	} else { /* IOMMU_XLATE or DMAI_FLAGS_PEER_TO_PEER */
		/*
		 * Initialize the handle's cookie information based on the
		 * newly established mapping.
		 */
		*ccountp = 1;
		MAKE_DMA_COOKIE(cookiep, mp->dmai_mapping, mp->dmai_size);
		DEBUG2(DBG_DMA_BINDH, dip,
			"1xlate/peer cookie: dmac_address=%x dmac_size=%x\n\n",
			cookiep->dmac_address, cookiep->dmac_size);
	}
	return (rval);
}


/*
 * bus dma unbind handle entry point:
 */
/*ARGSUSED*/
static int
pci_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	pci_dmai_nexus_private_t *np =
	    (pci_dmai_nexus_private_t *)mp->dmai_nexus_private;
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	iommu_t *iommu_p = pci_p->pci_iommu_p;
	uint_t npages;

	DEBUG3(DBG_DMA_UNBINDH, dip, "rdip=%s%d, mp=%x\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip), handle);
	if ((mp->dmai_flags & DMAI_FLAGS_INUSE) == 0) {
		DEBUG0(DBG_DMA_UNBINDH, dip, "handle not bound\n");
		return (DDI_FAILURE);
	}

	/*
	 * Here if the handle is using the iommu.  Unload all the iommu
	 * translations.
	 */
	if (mp->dmai_ndvmapages) {
		iommu_unmap_window(iommu_p, mp);
		STREAMING_BUF_FLUSH(pci_p, mp, MP2CTX(mp), 0, 0);

		/*
		 * Free the correspsonding dvma space.
		 */
		if (mp->dmai_nwin > 1) {
			/*
			 * In this case, the mapping for the last window
			 * may not require the full amount of mapping
			 * space allocated to the window.
			 */
			npages = (mp->dmai_winsize / IOMMU_PAGE_SIZE) +
				((mp->dmai_mapping & IOMMU_PAGE_OFFSET) ? 1:0);
		} else
			npages = mp->dmai_ndvmapages;

		/*
		 * Put the dvma space back in the resource map.
		 */
		iommu_free_dvma_pages(iommu_p, npages + HAS_REDZONE(mp),
			(dvma_addr_t)mp->dmai_mapping);

		if ((mp->dmai_flags & DMAI_FLAGS_CONTEXT) &&
		    !(mp->dmai_flags & DMAI_FLAGS_FASTTRACK))
			pci_iommu_free_dvma_context(iommu_p,
			    pci_iommu_tte2ctx(np->tte));
	}

	pci_free_np_pages(np, 1);

	/*
	 * Now that we've freed some dvma space, see if there is anybody
	 * waiting for it.
	 */
	if (iommu_p->iommu_dvma_call_list_id != 0) {
		DEBUG0(DBG_DMA_UNBINDH, dip, "run dvma callback\n");
		ddi_run_callback(&iommu_p->iommu_dvma_call_list_id);
	}
	mp->dmai_flags = 0;
	return (DDI_SUCCESS);
}


/*
 * bus dma flush entry point:
 */
/*ARGSUSED*/
static int
pci_dma_flush(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, off_t off, size_t len,
	uint_t cache_flags)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));

	DEBUG2(DBG_DMA_FLUSH, dip, "rdip=%s%d\n",
	    ddi_driver_name(rdip), ddi_get_instance(rdip));
	STREAMING_BUF_FLUSH(pci_p, mp, MP2CTX(mp), off, len);
	return (DDI_SUCCESS);
}


/*
 * bus dma win entry point:
 */
/*ARGSUSED*/
pci_dma_win(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, uint_t win, off_t *offp,
	size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	window_t curwin;
	pci_t *pci_p;
	iommu_t *iommu_p;

	/*
	 * Make sure the handle was set up for partial mappings.
	 */
	DEBUG2(DBG_DMA_WIN, dip, "rdip=%s%d\n",
	    ddi_driver_name(rdip), ddi_get_instance(rdip));
	dump_dma_handle(DBG_DMA_WIN, dip, mp);
	if ((mp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
		DEBUG0(DBG_DMA_WIN, dip, "no partial mapping\n");
		return (DDI_FAILURE);
	}

	/*
	 * Check to be sure the window is in range.
	 */
	if (win >= mp->dmai_nwin) {
		DEBUG1(DBG_DMA_WIN, dip, "%x out of range\n", win);
		return (DDI_FAILURE);
	}

	/*
	 * Before moving the window, make sure the current window and
	 * new window are not the same.
	 */
	curwin = mp->dmai_offset / mp->dmai_winsize;
	if (win != curwin) {

		/*
		 * Free the iommu mapping for the current window.
		 */
		pci_p = get_pci_soft_state(ddi_get_instance(dip));
		iommu_p = pci_p->pci_iommu_p;
		iommu_unmap_window(iommu_p, mp);
		STREAMING_BUF_FLUSH(pci_p, mp, MP2CTX(mp), 0, 0);

		/*
		 * Map the new window into the iommu.
		 */
		iommu_map_window(iommu_p, mp, win);
	}

	/*
	 * Construct the cookie for new window and adjust offset, length
	 * and cookie counter parameters.
	 */
	MAKE_DMA_COOKIE(cookiep, mp->dmai_mapping, mp->dmai_size);
	DEBUG2(DBG_DMA_WIN, dip, "cookie - dmac_address=%x dmac_size=%x\n",
		cookiep->dmac_address, cookiep->dmac_size);
	if (ccountp)
		*ccountp = 1;
	*offp = (off_t)mp->dmai_offset;
	*lenp = IOMMU_PTOB(mp->dmai_ndvmapages -
		IOMMU_BTOPR(mp->dmai_mapping & IOMMU_PAGE_OFFSET));
	DEBUG2(DBG_DMA_WIN, dip, "*offp=%x *lenp=%x\n", *offp, *lenp);
	return (DDI_SUCCESS);
}


/*
 * bus dma control entry point:
 */
/*ARGSUSED*/
int
pci_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
	off_t *offp, size_t *lenp,
	caddr_t *objp, uint_t cache_flags)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ddi_dma_seg_impl_t *sp;
	ddi_dma_cookie_t *cp;
	iommu_t *iommu_p;

	switch (request) {
	case DDI_DMA_FREE:

		DEBUG2(DBG_DMA_CTL, dip, "DDI_DMA_FREE: rdip=%s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		(void) pci_dma_unbindhdl(dip, rdip, handle);
		(void) pci_dma_freehdl(dip, rdip, handle);
		return (DDI_SUCCESS);

	case DDI_DMA_SYNC:

		/*
		 * Flush the streaming cache if the mapping is not consistent.
		 */
		DEBUG2(DBG_DMA_CTL, dip, "DDI_DMA_SYNC: rdip=%s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		STREAMING_BUF_FLUSH(pci_p, mp, MP2CTX(mp), *offp, *lenp);
		return (DDI_SUCCESS);

	case DDI_DMA_HTOC:

		/*
		 * Translate a DMA handle to DMA cookie.
		 */
		DEBUG2(DBG_DMA_CTL, dip, "DDI_DMA_HTOC: rdip=%s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		cp = (ddi_dma_cookie_t *)objp;
		if ((ulong_t)*offp >= (ulong_t)mp->dmai_size)
			return (DDI_FAILURE);
		MAKE_DMA_COOKIE(cp, mp->dmai_mapping + (ulong_t)*offp,
			mp->dmai_mapping + mp->dmai_size - cp->dmac_address);
		DEBUG2(DBG_DMA_CTL, dip,
		    "HTOC: cookie - dmac_address=%x dmac_size=%x\n",
		    cp->dmac_address, cp->dmac_size);
		return (DDI_SUCCESS);

	case DDI_DMA_REPWIN:

		/*
		 * The mapping must be a partial one.
		 */
		DEBUG2(DBG_DMA_CTL, dip, "DDI_DMA_REPWIN: rdip=%s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		if ((mp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
			DEBUG0(DBG_DMA_CTL, dip,
			    "REPWIN: no partial mapping\n");
			return (DDI_FAILURE);
		}

		*offp = (off_t)mp->dmai_offset;
		*lenp =
		    IOMMU_PTOB(mp->dmai_ndvmapages -
			IOMMU_BTOPR(mp->dmai_mapping & IOMMU_PAGE_OFFSET));
		DEBUG2(DBG_DMA_CTL, dip, "REPWIN: *offp=%x *lenp=%x\n",
		    *offp, *lenp);
		return (DDI_SUCCESS);

	case DDI_DMA_MOVWIN:
		/*
		 * Determine the current and new windows.
		 */
		DEBUG4(DBG_DMA_CTL, dip,
		    "DDI_DMA_MOVWIN: rdip=%s%d - lenp=%x offp-%x\n",
			ddi_driver_name(rdip), ddi_get_instance(rdip),
			*lenp, *offp);
		dump_dma_handle(DBG_DMA_CTL, dip, mp);

		/*
		 * XXX could be problematic if we need to support the
		 * lenp = transfer size vs. absolute window size
		 */
		if (*lenp != mp->dmai_winsize || *offp & (mp->dmai_winsize - 1))
			return (DDI_FAILURE);

		return (pci_dma_win(dip, rdip, handle, *offp / mp->dmai_winsize,
			offp, lenp, (ddi_dma_cookie_t *)objp, NULL));

	case DDI_DMA_NEXTWIN: {
		ddi_dma_win_t *nwin, *owin;
		window_t newwin;

		owin = (ddi_dma_win_t *)offp;
		nwin = (ddi_dma_win_t *)objp;
		DEBUG5(DBG_DMA_CTL, dip,
		    "DDI_DMA_NEXTWIN: rdip=%s%d - mp=%x owin=%x nwin=%x\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    mp, owin, nwin);
		dump_dma_handle(DBG_DMA_CTL, dip, mp);

		/*
		 * If we don't have partial mappings, all we can only
		 * honor requests for the first window.
		 */
		if (!(mp->dmai_rflags & DDI_DMA_PARTIAL))
			if (*owin != NULL)
				return (DDI_DMA_DONE);

		/*
		 * See if this is the first nextwin request for this handle.
		 * If it is just return the handle.
		 */
		if (*owin == NULL) {
			mp->dmai_offset = 0;
			*nwin = (ddi_dma_win_t)mp;
			return (DDI_SUCCESS);
		}

		/*
		 * Make sure there really is a next window.
		 */
		newwin = (mp->dmai_offset / mp->dmai_winsize) + 1;
		if (newwin >= mp->dmai_nwin)
			return (DDI_DMA_DONE);

		/*
		 * Map in the next dma window, handling streaming cache
		 * flushing for the current dma window.
		 */
		iommu_p = pci_p->pci_iommu_p;
		iommu_unmap_window(iommu_p, mp);
		STREAMING_BUF_FLUSH(pci_p, mp, MP2CTX(mp), 0, 0);
		iommu_map_window(iommu_p, mp, newwin);

		return (DDI_SUCCESS);
	}

	case DDI_DMA_NEXTSEG: {
		ddi_dma_seg_t *nseg, *oseg;

		oseg = (ddi_dma_seg_t *)lenp;
		nseg = (ddi_dma_seg_t *)objp;
		DEBUG5(DBG_DMA_CTL, dip,
		    "DDI_DMA_NEXTSEG: rdip=%s%d - win=%x oseg=%x nseg=%x\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    offp, oseg, nseg);

		/*
		 * Currently each window should have only one segment.
		 */
		if (*oseg != NULL)
			return (DDI_DMA_DONE);
		*nseg = *((ddi_dma_seg_t *)offp);
		return (DDI_SUCCESS);
	}

	case DDI_DMA_SEGTOC:

		sp = (ddi_dma_seg_impl_t *)handle;
		MAKE_DMA_COOKIE((ddi_dma_cookie_t *)objp,
		    sp->dmai_mapping, sp->dmai_size);
		*offp = sp->dmai_offset;
		*lenp = sp->dmai_size;
		return (DDI_SUCCESS);

	case DDI_DMA_COFF:

		/*
		 * Return the mapping offset for a DMA cookie.  We process
		 * this request here to save a call to our parent.
		 */
		cp = (ddi_dma_cookie_t *)offp;
		if (cp->dmac_address < mp->dmai_mapping ||
		    cp->dmac_address >= mp->dmai_mapping + mp->dmai_size) {
			DEBUG0(DBG_DMA_CTL, dip, "DDI_DMA_COFF DDI_FAILURE\n");
			return (DDI_FAILURE);
		}
		*objp = (caddr_t)(cp->dmac_address - mp->dmai_mapping);
		return (DDI_SUCCESS);

	case DDI_DMA_RESERVE:
		return (pci_fdvma_reserve(dip, rdip, pci_p,
			(struct ddi_dma_req *)offp, (ddi_dma_handle_t *)objp));

	case DDI_DMA_RELEASE:
		DEBUG2(DBG_DMA_CTL, dip, "DDI_DMA_RELEASE: rdip=%s%d\n",
			ddi_driver_name(rdip), ddi_get_instance(rdip));
		return (pci_fdvma_release(dip, pci_p, mp));

	default:
		DEBUG3(DBG_DMA_CTL, dip, "unknown command (%x): rdip=%s%d\n",
		    request, ddi_driver_name(rdip), ddi_get_instance(rdip));
		return (DDI_FAILURE);
	}
}


/*
 * control ops entry point:
 *
 * Requests handled completely:
 *	DDI_CTLOPS_INITCHILD	see init_child() for details
 *	DDI_CTLOPS_UNINITCHILD
 *	DDI_CTLOPS_REPORTDEV	see report_dev() for details
 *	DDI_CTLOPS_XLATE_INTRS	nothing to do
 *	DDI_CTLOPS_IOMIN	cache line size if streaming otherwise 1
 *	DDI_CTLOPS_REGSIZE
 *	DDI_CTLOPS_NREGS
 *	DDI_CTLOPS_NINTRS
 *	DDI_CTLOPS_DVMAPAGESIZE
 *	DDI_CTLOPS_POKE_INIT
 *	DDI_CTLOPS_POKE_FLUSH
 *	DDI_CTLOPS_POKE_FINI
 *
 * All others passed to parent.
 */
static int
pci_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	pbm_t *pbm_p;

	switch (op) {
	case DDI_CTLOPS_INITCHILD:
		return (init_child(pci_p, (dev_info_t)arg));

	case DDI_CTLOPS_UNINITCHILD:
		DEBUG2(DBG_CTLOPS, pci_p->pci_dip,
			"DDI_CTLOPS_UNINITCHILD: arg=%s%d\n",
			ddi_driver_name(arg), ddi_get_instance(arg));

		ddi_set_name_addr((dev_info_t)arg, NULL);
		ddi_remove_minor_node((dev_info_t)arg, NULL);
		impl_rem_dev_props((dev_info_t)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REPORTDEV:
		return (report_dev(rdip));

	case DDI_CTLOPS_IOMIN:

		/*
		 * If we are using the streaming cache, align at
		 * least on a cache line boundary. Otherwise use
		 * whatever alignment is passed in.
		 */

		if ((int)arg) {
			int val = *((int *)result);

			val = maxbit(val, PCI_SBUF_LINE_SIZE);
			*((int *)result) = val;
		}
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
		*((off_t *)result) = get_reg_set_size(rdip, *((int *)arg));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_NREGS:
		*((uint_t *)result) = get_nreg_set(rdip);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_DVMAPAGESIZE:
		*((ulong_t *)result) = IOMMU_PAGE_SIZE;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_POKE_INIT: {
		ddi_nofault_data_t *nofault_data = arg;

		pbm_p = pci_p->pci_pbm_p;
		mutex_enter(&pbm_p->pbm_pokefault_mutex);

		if (nofault_data && nofault_data->op_type == POKE_START) {
			pbm_p->nofault_data = nofault_data;
			return (DDI_SUCCESS);
		}

		mutex_exit(&pbm_p->pbm_pokefault_mutex);
		return (DDI_FAILURE);
	}

	case DDI_CTLOPS_POKE_FLUSH: {
		ddi_nofault_data_t *nofault_data;

		pbm_p = pci_p->pci_pbm_p;
		nofault_data = pbm_p->nofault_data;
		/*
		 * Read the async fault register for the PBM to see it sees
		 * a master-abort.
		 */
		pbm_clear_error(pbm_p);
		ASSERT(nofault_data != 0);
		return (nofault_data->op_type & POKE_FAULT
		    ? DDI_FAILURE : DDI_SUCCESS);
	}

	case DDI_CTLOPS_POKE_FINI: {

		pbm_p = pci_p->pci_pbm_p;
		pbm_p->nofault_data = 0;
		mutex_exit(&pbm_p->pbm_pokefault_mutex);
		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_AFFINITY:
		/*
		 * Major hack alert!  We're overloading this ctlop to handle
		 * bus faulting in a shwp release where we can't create a new
		 * ctlop.  The normal affinity call will zero the result arg,
		 * but our implementation uses it, this is how we can
		 * determine if we ever had a "real" affinity call.
		 *
		 * We're using this ctlop to allow nexus drivers underneath
		 * the PCI bus to install fault handlers. The sequence of
		 * adding handler to the beginning of the list is significant
		 * because that allows subordinate bridges clean up error
		 * conditions before host bridge fault handler gets called.
		 */
		if (result != NULL) {
			struct pci_fault_handle *fhp;

			fhp = kmem_alloc(sizeof (struct pci_fault_handle),
				KM_SLEEP);
			fhp->fh_dip = rdip;
			fhp->fh_f   = (int (*)())result;
			fhp->fh_arg = arg;

			mutex_enter(&pci_p->pci_fh_lst_mutex);
			fhp->fh_next = pci_p->pci_fh_lst;
			pci_p->pci_fh_lst = fhp;
			mutex_exit(&pci_p->pci_fh_lst_mutex);

			return (DDI_SUCCESS);
		}
		break;

	case DDI_CTLOPS_POWER: {
		power_req_t	*reqp = (power_req_t *)arg;
		/*
		 * We currently understand reporting of PCI_PM_IDLESPEED
		 * capability. Everything else is passed up.
		 */
		if ((reqp->request_type == PMR_REPORT_PMCAP) &&
		    (reqp->req.report_pmcap_req.cap ==  PCI_PM_IDLESPEED)) {
			int	idlespd = (int)reqp->req.report_pmcap_req.arg;
			/*
			 * Nothing to do here at this time, so just return
			 * success. We don't accept obviously bad values
			 * so that a leaf driver gets a consistent view
			 * on this platform and the platform doing real
			 * processing.
			 */
			if ((idlespd >= 0 && idlespd <= PCI_CLK_33MHZ / 1024) ||
			    (idlespd == (int)PCI_PM_IDLESPEED_ANY) ||
			    (idlespd == (int)PCI_PM_IDLESPEED_NONE)) {
				return (DDI_SUCCESS);
			} else {
				return (DDI_FAILURE);
			}
		}
		break;
	}

	default:
		break;
	}

	/*
	 * Now pass the request up to our parent.
	 */
	DEBUG2(DBG_CTLOPS, dip, "passing request to parent: rdip=%s%d\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip));
	return (ddi_ctlops(dip, rdip, op, arg, result));
}


/* Consolidated interrupt processing interface */
int
pci_intr_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_intr_ctlop_t op, void *arg, void *result)
{
	ddi_intr_info_t *intr_info;
	int rval = DDI_SUCCESS;

	switch (op) {
	case DDI_INTR_CTLOPS_ALLOC_ISPEC: {
		uint32_t inumber = (uint32_t)arg;
		ddi_ispec_t **ispecp = result;

		i_ddi_alloc_ispec(rdip, inumber, (ddi_intrspec_t *)ispecp);

		return (rval);
	}

	case DDI_INTR_CTLOPS_FREE_ISPEC:
		i_ddi_free_ispec((ddi_intrspec_t)arg);
		return (rval);

	case DDI_INTR_CTLOPS_NINTRS:
		*(int *)result = i_ddi_get_nintrs(rdip);
		return (rval);

	default:
		break;
	}


	intr_info = (ddi_intr_info_t *)arg;

	switch (intr_info->ii_kind) {
	case IDDI_INTR_TYPE_NORMAL:
		switch (op) {
		case DDI_INTR_CTLOPS_ADD:
			return (pci_add_intr_impl(dip, rdip, intr_info));

		case DDI_INTR_CTLOPS_REMOVE:
			return (pci_remove_intr_impl(dip, rdip, intr_info));

		case DDI_INTR_CTLOPS_HILEVEL: {
			ddi_ispec_t *ispecp =
			    (ddi_ispec_t *)intr_info->ii_ispec;

			if (ispecp->is_pil == 0)
				ispecp->is_pil = iline_to_pil(rdip);

		}

		default:
			break;
		}

		break;

	default:
		/* Only handle normal interrupts */
		return (DDI_FAILURE);
	}

	/* Pass up ctlops */
	rval = i_ddi_intr_ctlops(dip, rdip, op, arg, result);

	return (rval);
}
