/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)scsi_hba.c	1.65	99/10/07 SMI"

#include <sys/note.h>

/*
 * Generic SCSI Host Bus Adapter interface implementation
 */
#include <sys/scsi/scsi.h>
#include <sys/file.h>
#include <sys/ddi_impldefs.h>

static kmutex_t	scsi_hba_mutex;

kmutex_t scsi_log_mutex;


struct scsi_hba_inst {
	dev_info_t		*inst_dip;
	scsi_hba_tran_t		*inst_hba_tran;
	struct scsi_hba_inst	*inst_next;
	struct scsi_hba_inst	*inst_prev;
};

static struct scsi_hba_inst	*scsi_hba_list		= NULL;
static struct scsi_hba_inst	*scsi_hba_list_tail	= NULL;


#if !defined(lint)
_NOTE(READ_ONLY_DATA(dev_ops))
#endif

kmutex_t	scsi_flag_nointr_mutex;
kcondvar_t	scsi_flag_nointr_cv;

/*
 * Prototypes for static functions
 */
static int	scsi_hba_bus_ctl(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			ddi_ctl_enum_t		op,
			void			*arg,
			void			*result);

static int	scsi_hba_map_fault(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			struct hat		*hat,
			struct seg		*seg,
			caddr_t			addr,
			struct devpage		*dp,
			uint_t			pfn,
			uint_t			prot,
			uint_t			lock);

static int	scsi_hba_get_eventcookie(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			char			*name,
			ddi_eventcookie_t	*eventp,
			ddi_plevel_t		*plevelp,
			ddi_iblock_cookie_t	*iblkcookiep);

static int	scsi_hba_add_eventcall(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			ddi_eventcookie_t	event,
			int			(*callback)(
					dev_info_t *dip,
					ddi_eventcookie_t event,
					void *arg,
					void *bus_impldata),
			void			*arg);

static int	scsi_hba_remove_eventcall(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			ddi_eventcookie_t	event);

static int	scsi_hba_post_event(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			ddi_eventcookie_t	event,
			void			*bus_impldata);

static int	scsi_hba_info(
			dev_info_t		*dip,
			ddi_info_cmd_t		infocmd,
			void			*arg,
			void			**result);

/*
 * Busops vector for SCSI HBA's.
 */
static struct bus_ops scsi_hba_busops = {
	BUSO_REV,
	nullbusmap,			/* bus_map */
	NULL,				/* bus_get_intrspec */
	NULL,				/* bus_add_intrspec */
	NULL,				/* bus_remove_intrspec */
	scsi_hba_map_fault,		/* bus_map_fault */
	ddi_dma_map,			/* bus_dma_map */
	ddi_dma_allochdl,		/* bus_dma_allochdl */
	ddi_dma_freehdl,		/* bus_dma_freehdl */
	ddi_dma_bindhdl,		/* bus_dma_bindhdl */
	ddi_dma_unbindhdl,		/* bus_unbindhdl */
	ddi_dma_flush,			/* bus_dma_flush */
	ddi_dma_win,			/* bus_dma_win */
	ddi_dma_mctl,			/* bus_dma_ctl */
	scsi_hba_bus_ctl,		/* bus_ctl */
	ddi_bus_prop_op,		/* bus_prop_op */
	scsi_hba_get_eventcookie,	/* bus_get_eventcookie */
	scsi_hba_add_eventcall,		/* bus_add_eventcall */
	scsi_hba_remove_eventcall,	/* bus_remove_eventcall */
	scsi_hba_post_event		/* bus_post_event */
};


static struct cb_ops scsi_hba_cbops = {
	scsi_hba_open,
	scsi_hba_close,
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	scsi_hba_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,			/* stream */
	D_NEW|D_MP|D_HOTPLUG,	/* cb_flag */
	CB_REV,			/* rev */
	nodev,			/* int (*cb_aread)() */
	nodev			/* int (*cb_awrite)() */
};


/*
 * Called from _init() when loading scsi module
 */
void
scsi_initialize_hba_interface()
{
	mutex_init(&scsi_hba_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&scsi_flag_nointr_mutex, NULL, MUTEX_DRIVER, NULL);
	cv_init(&scsi_flag_nointr_cv, NULL, CV_DRIVER, NULL);
	mutex_init(&scsi_log_mutex, NULL, MUTEX_DRIVER, NULL);
}

#ifdef	NO_SCSI_FINI_YET
/*
 * Called from _fini() when unloading scsi module
 */
void
scsi_uninitialize_hba_interface()
{
	mutex_destroy(&scsi_hba_mutex);
	cv_destroy(&scsi_flag_nointr_cv);
	mutex_destroy(&scsi_flag_nointr_mutex);
	mutex_destroy(&scsi_log_mutex);
}
#endif	/* NO_SCSI_FINI_YET */



/*
 * Called by an HBA from _init()
 */
int
scsi_hba_init(struct modlinkage *modlp)
{
	struct dev_ops *hba_dev_ops;

	/*
	 * Get the devops structure of the hba,
	 * and put our busops vector in its place.
	 */
	hba_dev_ops = ((struct modldrv *)
		(modlp->ml_linkage[0]))->drv_dev_ops;
	ASSERT(hba_dev_ops->devo_bus_ops == NULL);
	hba_dev_ops->devo_bus_ops = &scsi_hba_busops;

	/*
	 * Provide getinfo and hotplugging ioctl if driver
	 * does not provide them already
	 */
	if (hba_dev_ops->devo_cb_ops == NULL) {
		hba_dev_ops->devo_cb_ops = &scsi_hba_cbops;
	}
	if (hba_dev_ops->devo_cb_ops->cb_open == scsi_hba_open) {
		ASSERT(hba_dev_ops->devo_cb_ops->cb_close == scsi_hba_close);
		hba_dev_ops->devo_getinfo = scsi_hba_info;
	}

	return (0);
}


/*
 * Implement this older interface in terms of the new.
 * This is hardly in the critical path, so avoiding
 * unnecessary code duplication is more important.
 */
/*ARGSUSED*/
int
scsi_hba_attach(
	dev_info_t		*dip,
	ddi_dma_lim_t		*hba_lim,
	scsi_hba_tran_t		*hba_tran,
	int			flags,
	void			*hba_options)
{
	ddi_dma_attr_t		hba_dma_attr;

	bzero((caddr_t)&hba_dma_attr, sizeof (ddi_dma_attr_t));

	hba_dma_attr.dma_attr_burstsizes = hba_lim->dlim_burstsizes;
	hba_dma_attr.dma_attr_minxfer = hba_lim->dlim_minxfer;

	return (scsi_hba_attach_setup(dip, &hba_dma_attr, hba_tran, flags));
}


/*
 * Called by an HBA to attach an instance of the driver
 */
int
scsi_hba_attach_setup(
	dev_info_t		*dip,
	ddi_dma_attr_t		*hba_dma_attr,
	scsi_hba_tran_t		*hba_tran,
	int			flags)
{
	struct dev_ops		*hba_dev_ops;
	struct scsi_hba_inst	*elem;
	int			value;
	int			len;
	char			*prop_name;
	char			*errmsg =
		"scsi_hba_attach: cannot create property '%s' for %s%d\n";

	/*
	 * Link this instance into the scsi_hba_list
	 */
	elem = kmem_alloc(sizeof (struct scsi_hba_inst), KM_SLEEP);

	elem->inst_dip = dip;
	elem->inst_hba_tran = hba_tran;

	mutex_enter(&scsi_hba_mutex);
	elem->inst_next = NULL;
	elem->inst_prev = scsi_hba_list_tail;
	if (scsi_hba_list == NULL) {
		scsi_hba_list = elem;
	}
	if (scsi_hba_list_tail) {
		scsi_hba_list_tail->inst_next = elem;
	}
	scsi_hba_list_tail = elem;
	mutex_exit(&scsi_hba_mutex);

	/*
	 * Save all the important HBA information that must be accessed
	 * later by scsi_hba_bus_ctl(), and scsi_hba_map().
	 */
	hba_tran->tran_hba_dip = dip;
	hba_tran->tran_hba_flags = flags;

	/*
	 * Note: we only need dma_attr_minxfer and dma_attr_burstsizes
	 * from the DMA attributes.  scsi_hba_attach(9f) only
	 * guarantees that these two fields are initialized properly.
	 * If this changes, be sure to revisit the implementation
	 * of scsi_hba_attach(9F).
	 */
	hba_tran->tran_min_xfer = hba_dma_attr->dma_attr_minxfer;
	hba_tran->tran_min_burst_size =
		(1<<(ddi_ffs(hba_dma_attr->dma_attr_burstsizes)-1));
	hba_tran->tran_max_burst_size =
		(1<<(ddi_fls(hba_dma_attr->dma_attr_burstsizes)-1));

	/*
	 * Attach scsi configuration property parameters
	 * to this instance of the hba.
	 */
	prop_name = "scsi-reset-delay";
	len = 0;
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN, 0, prop_name,
			NULL, &len) == DDI_PROP_NOT_FOUND) {
		value = scsi_reset_delay;
		if (ddi_prop_create(DDI_MAJOR_T_UNKNOWN, dip,
			DDI_PROP_CANSLEEP, prop_name, (caddr_t)&value,
				sizeof (int)) != DDI_PROP_SUCCESS) {
			cmn_err(CE_CONT, errmsg, prop_name,
				ddi_get_name(dip), ddi_get_instance(dip));
		}
	}

	prop_name = "scsi-tag-age-limit";
	len = 0;
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN, 0, prop_name,
			NULL, &len) == DDI_PROP_NOT_FOUND) {
		value = scsi_tag_age_limit;
		if (ddi_prop_create(DDI_MAJOR_T_UNKNOWN, dip,
			DDI_PROP_CANSLEEP, prop_name, (caddr_t)&value,
				sizeof (int)) != DDI_PROP_SUCCESS) {
			cmn_err(CE_CONT, errmsg, prop_name,
				ddi_get_name(dip), ddi_get_instance(dip));
		}
	}

	prop_name = "scsi-watchdog-tick";
	len = 0;
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN, 0, prop_name,
			NULL, &len) == DDI_PROP_NOT_FOUND) {
		value = scsi_watchdog_tick;
		if (ddi_prop_create(DDI_MAJOR_T_UNKNOWN, dip,
			DDI_PROP_CANSLEEP, prop_name, (caddr_t)&value,
				sizeof (int)) != DDI_PROP_SUCCESS) {
			cmn_err(CE_CONT, errmsg, prop_name,
				ddi_get_name(dip), ddi_get_instance(dip));
		}
	}

	prop_name = "scsi-options";
	len = 0;
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN, 0, prop_name,
			NULL, &len) == DDI_PROP_NOT_FOUND) {
		value = scsi_options;
		if (ddi_prop_create(DDI_MAJOR_T_UNKNOWN, dip,
			DDI_PROP_CANSLEEP, prop_name, (caddr_t)&value,
				sizeof (int)) != DDI_PROP_SUCCESS) {
			cmn_err(CE_CONT, errmsg, prop_name,
				ddi_get_name(dip), ddi_get_instance(dip));
		}
	}

	prop_name = "scsi-selection-timeout";
	len = 0;
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN, 0, prop_name,
			NULL, &len) == DDI_PROP_NOT_FOUND) {
		value = scsi_selection_timeout;
		if (ddi_prop_create(DDI_MAJOR_T_UNKNOWN, dip,
			DDI_PROP_CANSLEEP, prop_name, (caddr_t)&value,
				sizeof (int)) != DDI_PROP_SUCCESS) {
			cmn_err(CE_CONT, errmsg, prop_name,
				ddi_get_name(dip), ddi_get_instance(dip));
		}
	}

	ddi_set_driver_private(dip, (caddr_t)hba_tran);

	/*
	 * Create devctl minor node unless driver supplied its own
	 * open/close entry points
	 */
	hba_dev_ops = ddi_get_driver(dip);
	ASSERT(hba_dev_ops != NULL);
	if (hba_dev_ops->devo_cb_ops->cb_open == scsi_hba_open) {
		/*
		 * Make sure that instance number doesn't overflow
		 * when forming minor numbers.
		 */
		ASSERT(ddi_get_instance(dip) <=
		    (L_MAXMIN >> INST_MINOR_SHIFT));

		if ((ddi_create_minor_node(dip, "devctl", S_IFCHR,
		    INST2DEVCTL(ddi_get_instance(dip)),
		    DDI_NT_SCSI_NEXUS, 0) != DDI_SUCCESS) ||
		    (ddi_create_minor_node(dip, "scsi", S_IFCHR,
		    INST2SCSI(ddi_get_instance(dip)),
		    DDI_NT_SCSI_ATTACHMENT_POINT, 0) != DDI_SUCCESS)) {
			ddi_remove_minor_node(dip, "devctl");
			ddi_remove_minor_node(dip, "scsi");
			cmn_err(CE_WARN, "scsi_hba_attach: "
			    "cannot create devctl/scsi minor nodes");
		}
	}

	return (DDI_SUCCESS);
}


/*
 * Called by an HBA to detach an instance of the driver
 */
int
scsi_hba_detach(dev_info_t *dip)
{
	struct dev_ops		*hba_dev_ops;
	scsi_hba_tran_t		*hba;
	struct scsi_hba_inst	*elem;


	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	ddi_set_driver_private(dip, NULL);
	ASSERT(hba != NULL);
	ASSERT(hba->tran_open_flag == 0);

	hba_dev_ops = ddi_get_driver(dip);
	ASSERT(hba_dev_ops != NULL);
	if (hba_dev_ops->devo_cb_ops->cb_open == scsi_hba_open) {
		ddi_remove_minor_node(dip, "devctl");
		ddi_remove_minor_node(dip, "scsi");
	}

	/*
	 * XXX - scsi_transport.h states that these data fields should not be
	 *	 referenced by the HBA. However, to be consistent with
	 *	 scsi_hba_attach(), they are being reset.
	 */
	hba->tran_hba_dip = (dev_info_t *)NULL;
	hba->tran_hba_flags = 0;
	hba->tran_min_burst_size = (uchar_t)0;
	hba->tran_max_burst_size = (uchar_t)0;

	/*
	 * Remove HBA instance from scsi_hba_list
	 */
	mutex_enter(&scsi_hba_mutex);
	for (elem = scsi_hba_list; elem != (struct scsi_hba_inst *)NULL;
		elem = elem->inst_next) {
		if (elem->inst_dip == dip)
			break;
	}

	if (elem == (struct scsi_hba_inst *)NULL) {
		cmn_err(CE_CONT, "scsi_hba_attach: unknown HBA instance\n");
		mutex_exit(&scsi_hba_mutex);
		return (DDI_FAILURE);
	}
	if (elem == scsi_hba_list) {
		scsi_hba_list = elem->inst_next;
		if (scsi_hba_list) {
			scsi_hba_list->inst_prev = (struct scsi_hba_inst *)NULL;
		}
		if (elem == scsi_hba_list_tail) {
			scsi_hba_list_tail = NULL;
		}
	} else if (elem == scsi_hba_list_tail) {
		scsi_hba_list_tail = elem->inst_prev;
		if (scsi_hba_list_tail) {
			scsi_hba_list_tail->inst_next =
					(struct scsi_hba_inst *)NULL;
		}
	} else {
		elem->inst_prev->inst_next = elem->inst_next;
		elem->inst_next->inst_prev = elem->inst_prev;
	}
	mutex_exit(&scsi_hba_mutex);

	kmem_free(elem, sizeof (struct scsi_hba_inst));

	return (DDI_SUCCESS);
}


/*
 * Called by an HBA from _fini()
 */
void
scsi_hba_fini(struct modlinkage *modlp)
{
	struct dev_ops *hba_dev_ops;

	/*
	 * Get the devops structure of this module
	 * and clear bus_ops vector.
	 */
	hba_dev_ops = ((struct modldrv *)
		(modlp->ml_linkage[0]))->drv_dev_ops;

	if (hba_dev_ops->devo_cb_ops == &scsi_hba_cbops) {
		hba_dev_ops->devo_cb_ops = NULL;
	}

	if (hba_dev_ops->devo_getinfo == scsi_hba_info) {
		hba_dev_ops->devo_getinfo = NULL;
	}

	hba_dev_ops->devo_bus_ops = (struct bus_ops *)NULL;
}


/*
 * Generic bus_ctl operations for SCSI HBA's,
 * hiding the busctl interface from the HBA.
 */
/*ARGSUSED*/
static int
scsi_hba_bus_ctl(
	dev_info_t		*dip,
	dev_info_t		*rdip,
	ddi_ctl_enum_t		op,
	void			*arg,
	void			*result)
{

	switch (op) {
	case DDI_CTLOPS_REPORTDEV:
	{
		struct scsi_device	*devp;
		scsi_hba_tran_t		*hba;

		hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		ASSERT(hba != NULL);

		devp = (struct scsi_device *)ddi_get_driver_private(rdip);

		if ((hba->tran_get_bus_addr == NULL) ||
		    (hba->tran_get_name == NULL)) {
			cmn_err(CE_CONT, "?%s%d at %s%d: target %x lun %x\n",
			    ddi_driver_name(rdip), ddi_get_instance(rdip),
			    ddi_driver_name(dip), ddi_get_instance(dip),
			    devp->sd_address.a_target, devp->sd_address.a_lun);
		} else {
			char name[SCSI_MAXNAMELEN];
			char bus_addr[SCSI_MAXNAMELEN];

			if ((*hba->tran_get_name)(devp, name,
			    SCSI_MAXNAMELEN) != 1) {
				return (DDI_FAILURE);
			}
			if ((*hba->tran_get_bus_addr)(devp, bus_addr,
			    SCSI_MAXNAMELEN) != 1) {
				return (DDI_FAILURE);
			}
			cmn_err(CE_CONT,
			    "?%s%d at %s%d: name %s, bus address %s\n",
			    ddi_driver_name(rdip), ddi_get_instance(rdip),
			    ddi_driver_name(dip), ddi_get_instance(dip),
			    name, bus_addr);
		}
		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_IOMIN:
	{
		int		val;
		scsi_hba_tran_t	*hba;

		hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		ASSERT(hba != NULL);

		val = *((int *)result);
		val = maxbit(val, hba->tran_min_xfer);
		/*
		 * The 'arg' value of nonzero indicates 'streaming'
		 * mode.  If in streaming mode, pick the largest
		 * of our burstsizes available and say that that
		 * is our minimum value (modulo what minxfer is).
		 */
		*((int *)result) = maxbit(val, ((int)arg ?
			hba->tran_max_burst_size :
			hba->tran_min_burst_size));

		return (ddi_ctlops(dip, rdip, op, arg, result));
	}

	case DDI_CTLOPS_INITCHILD:
	{
		dev_info_t		*child_dip = (dev_info_t *)arg;
		struct scsi_device	*sd;
		char			name[SCSI_MAXNAMELEN];
		scsi_hba_tran_t		*hba;
		dev_info_t		*ndip;

		hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		ASSERT(hba != NULL);

		sd = kmem_zalloc(sizeof (struct scsi_device), KM_SLEEP);

		/*
		 * Clone transport structure if requested, so
		 * the HBA can maintain target-specific info, if
		 * necessary. At least all SCSI-3 HBAs will do this.
		 */
		if (hba->tran_hba_flags & SCSI_HBA_TRAN_CLONE) {
			scsi_hba_tran_t	*clone =
				kmem_alloc(sizeof (scsi_hba_tran_t),
					KM_SLEEP);
			bcopy((caddr_t)hba, (caddr_t)clone,
				sizeof (scsi_hba_tran_t));
			hba = clone;
			hba->tran_sd = sd;
		} else {
			ASSERT(hba->tran_sd == NULL);
		}

		sd->sd_dev = child_dip;
		sd->sd_address.a_hba_tran = hba;

		/*
		 * Make sure that HBA either supports both or none
		 * of tran_get_name/tran_get_addr
		 */
		if ((hba->tran_get_name != NULL) ||
		    (hba->tran_get_bus_addr != NULL)) {
			if ((hba->tran_get_name == NULL) ||
			    (hba->tran_get_bus_addr == NULL)) {
				cmn_err(CE_CONT,
				    "%s%d: should support both or none of "
				    "tran_get_name and tran_get_bus_addr\n",
				    ddi_get_name(dip), ddi_get_instance(dip));
				goto failure;
			}
		}

		/*
		 * In case HBA doesn't support tran_get_name/tran_get_bus_addr
		 * (e.g. most pre-SCSI-3 HBAs), we have to continue
		 * to provide old semantics. In case a HBA driver does
		 * support it, a_target and a_lun fields of scsi_address
		 * are not defined and will be 0 except for parallel bus.
		 */
		{
			int	t_len;
			int	targ = 0;
			int	lun = 0;

			t_len = sizeof (targ);
			if (ddi_prop_op(DDI_DEV_T_ANY, child_dip,
			    PROP_LEN_AND_VAL_BUF, DDI_PROP_DONTPASS |
			    DDI_PROP_CANSLEEP, "target", (caddr_t)&targ,
			    &t_len) != DDI_SUCCESS) {
				if (hba->tran_get_name == NULL) {
					kmem_free(sd,
						sizeof (struct scsi_device));
					if (hba->tran_hba_flags &
					    SCSI_HBA_TRAN_CLONE) {
						kmem_free(hba,
						    sizeof (scsi_hba_tran_t));
					}
					return (DDI_NOT_WELL_FORMED);
				}
			}

			t_len = sizeof (lun);
			(void) ddi_prop_op(DDI_DEV_T_ANY, child_dip,
			    PROP_LEN_AND_VAL_BUF, DDI_PROP_DONTPASS |
			    DDI_PROP_CANSLEEP, "lun", (caddr_t)&lun,
			    &t_len);

			/*
			 * If the HBA does not implement tran_get_name then it
			 * doesn't have any hope of supporting a LUN >= 256.
			 */
			if (lun >= 256 && hba->tran_get_name == NULL) {
				goto failure;
			}

			/*
			 * This is also to make sure that if someone plugs in
			 * a SCSI-2 disks to a SCSI-3 parallel bus HBA,
			 * his SCSI-2 target driver still continue to work.
			 */
			sd->sd_address.a_target = (ushort_t)targ;
			sd->sd_address.a_lun = (uchar_t)lun;
		}

		/*
		 * In case HBA support tran_get_name (e.g. all SCSI-3 HBAs),
		 * give it a chance to tell us the name.
		 * If it doesn't support this entry point, a name will be
		 * fabricated
		 */
		if (scsi_get_name(sd, name, SCSI_MAXNAMELEN) != 1) {
			goto failure;
		}

		/*
		 * Prevent duplicate nodes.
		 */
		ndip = ndi_devi_find(dip, ddi_node_name(child_dip), name);

		if (ndip && (ndip != child_dip)) {
			goto failure;
		}

		ddi_set_name_addr(child_dip, name);

		/*
		 * This is a grotty hack that allows direct-access
		 * (non-scsi) drivers using this interface to
		 * put its own vector in the 'a_hba_tran' field.
		 * When the drivers are fixed, remove this hack.
		 */
		sd->sd_reserved = hba;

		/*
		 * call hba's target init entry point if it exists
		 */
		if (hba->tran_tgt_init != NULL) {
			if ((*hba->tran_tgt_init)
			    (dip, child_dip, hba, sd) != DDI_SUCCESS) {
				ddi_set_name_addr(child_dip, NULL);
				goto failure;
			}

			/*
			 * Another grotty hack to undo initialization
			 * some hba's think they have authority to
			 * perform.
			 *
			 * XXX - Pending dadk_probe() semantics
			 *	 change.  (Re: 1171432)
			 */
			if (hba->tran_tgt_probe != NULL)
				sd->sd_inq = NULL;
		}

		mutex_init(&sd->sd_mutex, NULL, MUTEX_DRIVER, NULL);

		ddi_set_driver_private(child_dip, (caddr_t)sd);

		return (DDI_SUCCESS);

failure:
		kmem_free(sd, sizeof (struct scsi_device));
		if (hba->tran_hba_flags & SCSI_HBA_TRAN_CLONE) {
			kmem_free(hba, sizeof (scsi_hba_tran_t));
		}
		return (DDI_FAILURE);
	}

	case DDI_CTLOPS_UNINITCHILD:
	{
		struct scsi_device	*sd;
		dev_info_t		*child_dip = (dev_info_t *)arg;
		scsi_hba_tran_t		*hba;

		hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		ASSERT(hba != NULL);

		sd = (struct scsi_device *)ddi_get_driver_private(child_dip);
		ASSERT(sd != NULL);

		if (hba->tran_hba_flags & SCSI_HBA_TRAN_CLONE) {
			/*
			 * This is a grotty hack, continued.  This
			 * should be:
			 *	hba = sd->sd_address.a_hba_tran;
			 */
			hba = sd->sd_reserved;
			ASSERT(hba->tran_hba_flags & SCSI_HBA_TRAN_CLONE);
			ASSERT(hba->tran_sd == sd);
		} else {
			ASSERT(hba->tran_sd == NULL);
		}

		scsi_unprobe(sd);
		if (hba->tran_tgt_free != NULL) {
			(*hba->tran_tgt_free) (dip, child_dip, hba, sd);
		}
		mutex_destroy(&sd->sd_mutex);
		if (hba->tran_hba_flags & SCSI_HBA_TRAN_CLONE) {
			kmem_free(hba, sizeof (scsi_hba_tran_t));
		}
		kmem_free((caddr_t)sd, sizeof (*sd));

		ddi_set_driver_private(child_dip, NULL);
		ddi_set_name_addr(child_dip, NULL);

		return (DDI_SUCCESS);
	}

	/* XXX these should be handled */
	case DDI_CTLOPS_POWER:
	case DDI_CTLOPS_ATTACH:
	case DDI_CTLOPS_DETACH:

		return (DDI_SUCCESS);

	/*
	 * These ops correspond to functions that "shouldn't" be called
	 * by a SCSI target driver.  So we whinge when we're called.
	 */
	case DDI_CTLOPS_DMAPMAPC:
	case DDI_CTLOPS_REPORTINT:
	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
	case DDI_CTLOPS_NINTRS:
	case DDI_CTLOPS_SIDDEV:
	case DDI_CTLOPS_SLAVEONLY:
	case DDI_CTLOPS_AFFINITY:
	case DDI_CTLOPS_POKE_INIT:
	case DDI_CTLOPS_POKE_FLUSH:
	case DDI_CTLOPS_POKE_FINI:
	case DDI_CTLOPS_INTR_HILEVEL:
	case DDI_CTLOPS_XLATE_INTRS:
		cmn_err(CE_CONT, "%s%d: invalid op (%d) from %s%d\n",
			ddi_get_name(dip), ddi_get_instance(dip),
			op, ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_FAILURE);

	/*
	 * Everything else (e.g. PTOB/BTOP/BTOPR requests) we pass up
	 */
	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}


/*
 * Called by an HBA to allocate a scsi_hba_tran structure
 */
/*ARGSUSED*/
scsi_hba_tran_t *
scsi_hba_tran_alloc(
	dev_info_t		*dip,
	int			flags)
{
	return (kmem_zalloc(sizeof (scsi_hba_tran_t),
		(flags & SCSI_HBA_CANSLEEP) ? KM_SLEEP : KM_NOSLEEP));
}



/*
 * Called by an HBA to free a scsi_hba_tran structure
 */
void
scsi_hba_tran_free(
	scsi_hba_tran_t		*hba_tran)
{
	kmem_free(hba_tran, sizeof (scsi_hba_tran_t));
}



/*
 * Private wrapper for scsi_pkt's allocated via scsi_hba_pkt_alloc()
 */
struct scsi_pkt_wrapper {
	struct scsi_pkt		scsi_pkt;
	int			pkt_wrapper_len;
};

#if !defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per thread", scsi_pkt_wrapper))
#endif

/*
 * Round up all allocations so that we can guarantee
 * long-long alignment.  This is the same alignment
 * provided by kmem_alloc().
 */
#define	ROUNDUP(x)	(((x) + 0x07) & ~0x07)

/*
 * Called by an HBA to allocate a scsi_pkt
 */
/*ARGSUSED*/
struct scsi_pkt *
scsi_hba_pkt_alloc(
	dev_info_t		*dip,
	struct scsi_address	*ap,
	int			cmdlen,
	int			statuslen,
	int			tgtlen,
	int			hbalen,
	int			(*callback)(caddr_t arg),
	caddr_t			arg)
{
	struct scsi_pkt		*pkt;
	struct scsi_pkt_wrapper	*hba_pkt;
	caddr_t			p;
	int			pktlen;

	/*
	 * Sanity check
	 */
	if (callback != SLEEP_FUNC && callback != NULL_FUNC) {
		cmn_err(CE_PANIC, "scsi_hba_pkt_alloc: callback must be"
			" either SLEEP or NULL\n");
	}

	/*
	 * Round up so everything gets allocated on long-word boundaries
	 */
	cmdlen = ROUNDUP(cmdlen);
	tgtlen = ROUNDUP(tgtlen);
	hbalen = ROUNDUP(hbalen);
	statuslen = ROUNDUP(statuslen);
	pktlen = sizeof (struct scsi_pkt_wrapper)
		+ cmdlen + tgtlen + hbalen + statuslen;

	hba_pkt = kmem_zalloc(pktlen,
		(callback == SLEEP_FUNC) ? KM_SLEEP : KM_NOSLEEP);
	if (hba_pkt == NULL) {
		ASSERT(callback == NULL_FUNC);
		return (NULL);
	}

	/*
	 * Set up our private info on this pkt
	 */
	hba_pkt->pkt_wrapper_len = pktlen;
	pkt = &hba_pkt->scsi_pkt;
	p = (caddr_t)(hba_pkt + 1);

	/*
	 * Set up pointers to private data areas, cdb, and status.
	 */
	if (hbalen > 0) {
		pkt->pkt_ha_private = (opaque_t)p;
		p += hbalen;
	}
	if (tgtlen > 0) {
		pkt->pkt_private = (opaque_t)p;
		p += tgtlen;
	}
	if (statuslen > 0) {
		pkt->pkt_scbp = (uchar_t *)p;
		p += statuslen;
	}
	if (cmdlen > 0) {
		pkt->pkt_cdbp = (uchar_t *)p;
	}

	/*
	 * Initialize the pkt's scsi_address
	 */
	pkt->pkt_address = *ap;

	return (pkt);
}


/*
 * Called by an HBA to free a scsi_pkt
 */
/*ARGSUSED*/
void
scsi_hba_pkt_free(
	struct scsi_address	*ap,
	struct scsi_pkt		*pkt)
{
	kmem_free((struct scsi_pkt_wrapper *)pkt,
		((struct scsi_pkt_wrapper *)pkt)->pkt_wrapper_len);
}



/*
 * Called by an HBA to map strings to capability indices
 */
int
scsi_hba_lookup_capstr(
	char			*capstr)
{
	/*
	 * Capability strings, masking the the '-' vs. '_' misery
	 */
	static struct cap_strings {
		char	*cap_string;
		int	cap_index;
	} cap_strings[] = {
		{ "dma_max",		SCSI_CAP_DMA_MAX		},
		{ "dma-max",		SCSI_CAP_DMA_MAX		},
		{ "msg_out",		SCSI_CAP_MSG_OUT		},
		{ "msg-out",		SCSI_CAP_MSG_OUT		},
		{ "disconnect",		SCSI_CAP_DISCONNECT		},
		{ "synchronous",	SCSI_CAP_SYNCHRONOUS		},
		{ "wide_xfer",		SCSI_CAP_WIDE_XFER		},
		{ "wide-xfer",		SCSI_CAP_WIDE_XFER		},
		{ "parity",		SCSI_CAP_PARITY			},
		{ "initiator-id",	SCSI_CAP_INITIATOR_ID		},
		{ "untagged-qing",	SCSI_CAP_UNTAGGED_QING		},
		{ "tagged-qing",	SCSI_CAP_TAGGED_QING		},
		{ "auto-rqsense",	SCSI_CAP_ARQ			},
		{ "linked-cmds",	SCSI_CAP_LINKED_CMDS		},
		{ "sector-size",	SCSI_CAP_SECTOR_SIZE		},
		{ "total-sectors",	SCSI_CAP_TOTAL_SECTORS		},
		{ "geometry",		SCSI_CAP_GEOMETRY		},
		{ "reset-notification",	SCSI_CAP_RESET_NOTIFICATION	},
		{ "qfull-retries",	SCSI_CAP_QFULL_RETRIES		},
		{ "qfull-retry-interval", SCSI_CAP_QFULL_RETRY_INTERVAL	},
		{ "scsi-version", 	SCSI_CAP_SCSI_VERSION		},
		{ "interconnect-type", 	SCSI_CAP_INTERCONNECT_TYPE	},
		{ NULL,			0				}
	};
	struct cap_strings	*cp;

	for (cp = cap_strings; cp->cap_string != NULL; cp++) {
		if (strcmp(cp->cap_string, capstr) == 0) {
			return (cp->cap_index);
		}
	}

	return (-1);
}


/*
 * Called by an HBA to determine if the system is in 'panic' state.
 */
int
scsi_hba_in_panic()
{
	return (panicstr != NULL);
}



/*
 * If a SCSI target driver attempts to mmap memory,
 * the buck stops here.
 */
/*ARGSUSED*/
static int
scsi_hba_map_fault(
	dev_info_t		*dip,
	dev_info_t		*rdip,
	struct hat		*hat,
	struct seg		*seg,
	caddr_t			addr,
	struct devpage		*dp,
	uint_t			pfn,
	uint_t			prot,
	uint_t			lock)
{
	return (DDI_FAILURE);
}


static int
scsi_hba_get_eventcookie(
	dev_info_t		*dip,
	dev_info_t		*rdip,
	char			*name,
	ddi_eventcookie_t	*eventp,
	ddi_plevel_t		*plevelp,
	ddi_iblock_cookie_t	*iblkcookiep)
{
	scsi_hba_tran_t		*hba;

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (hba->tran_get_eventcookie && ((*hba->tran_get_eventcookie)(dip,
	    rdip, name, eventp, plevelp, iblkcookiep) == DDI_SUCCESS)) {
		return (DDI_SUCCESS);
	}

	return (ndi_busop_get_eventcookie(dip, rdip, name, eventp,
		plevelp, iblkcookiep));
}


static int
scsi_hba_add_eventcall(
	dev_info_t		*dip,
	dev_info_t		*rdip,
	ddi_eventcookie_t	event,
	int			(*callback)(
					dev_info_t *dip,
					ddi_eventcookie_t event,
					void *arg,
					void *bus_impldata),
	void			*arg)
{
	scsi_hba_tran_t		*hba;

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (hba->tran_add_eventcall && ((*hba->tran_add_eventcall)(dip,
	    rdip, event, callback, arg) == DDI_SUCCESS)) {
		return (DDI_SUCCESS);
	}

	return (ndi_busop_add_eventcall(dip, rdip, event, callback, arg));
}


static int
scsi_hba_remove_eventcall(
	dev_info_t		*dip,
	dev_info_t		*rdip,
	ddi_eventcookie_t	event)
{
	scsi_hba_tran_t		*hba;

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (hba->tran_remove_eventcall && ((*hba->tran_remove_eventcall)(dip,
	    rdip, event) == DDI_SUCCESS)) {
		return (DDI_SUCCESS);
	}

	return (ndi_busop_remove_eventcall(dip, rdip, event));
}


static int
scsi_hba_post_event(
	dev_info_t		*dip,
	dev_info_t		*rdip,
	ddi_eventcookie_t	event,
	void			*bus_impldata)
{
	scsi_hba_tran_t		*hba;

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (hba->tran_post_event && ((*hba->tran_post_event)(dip,
	    rdip, event, bus_impldata) == DDI_SUCCESS)) {
		return (DDI_SUCCESS);
	}

	return (ndi_post_event(dip, rdip, event, bus_impldata));
}

static dev_info_t *
devt_to_devinfo(dev_t dev)
{
	int count;
	dev_info_t *dip;
	struct devnames *dnp;
	major_t major = getmajor(dev);
	int instance = MINOR2INST(getminor(dev));

	if ((major > devcnt) || (major == (major_t)-1)) {
		return (NULL);
	}

	/*
	 * Should be held in dev_get_dev_info() or spec_open().
	 */
	ASSERT(DEV_OPS_HELD(devopsp[major]));

	dnp = &devnamesp[major];
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &count);
	dip = dnp->dn_head;
	while (dip && (ddi_get_instance(dip) != instance)) {
		dip = ddi_get_next(dip);
	}
	e_ddi_exit_driver_list(dnp, count);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	return (dip);
}

/*
 * Default getinfo(9e) for scsi_hba
 */
/* ARGSUSED */
static int
scsi_hba_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	int error = DDI_SUCCESS;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)devt_to_devinfo((dev_t)arg);
		if (*result == NULL) {
			error = DDI_FAILURE;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(MINOR2INST(getminor((dev_t)arg)));
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * Default open and close routine for scsi_hba
 */

/* ARGSUSED */
int
scsi_hba_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	int rv = 0;
	dev_info_t *dip;
	scsi_hba_tran_t *hba;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	dip = devt_to_devinfo(*devp);
	if (dip == NULL) {
		return (ENXIO);
	}

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (hba == NULL) {
		return (ENXIO);
	}

	/*
	 * tran_open_flag bit field:
	 *	0:	closed
	 *	1:	shared open by minor at bit position
	 *	1 at 31st bit:	exclusive open
	 */
	mutex_enter(&(hba->tran_open_lock));
	if (flags & FEXCL) {
		if (hba->tran_open_flag != 0) {
			rv = EBUSY;		/* already open */
		} else {
			hba->tran_open_flag = TRAN_OPEN_EXCL;
		}
	} else {
		if (hba->tran_open_flag == TRAN_OPEN_EXCL) {
			rv = EBUSY;		/* already excl. open */
		} else {
			int minor = getminor(*devp) & TRAN_MINOR_MASK;
			hba->tran_open_flag |= (1 << minor);
			/*
			 * Ensure that the last framework reserved minor
			 * is unused. Otherwise, the exclusive open
			 * mechanism may break.
			 */
			ASSERT(minor != 31);
		}
	}
	mutex_exit(&(hba->tran_open_lock));

	return (rv);
}

/* ARGSUSED */
int
scsi_hba_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	dev_info_t *dip;
	scsi_hba_tran_t *hba;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	dip = devt_to_devinfo(dev);
	if (dip == NULL)
		return (ENXIO);

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if (hba == NULL)
		return (ENXIO);

	mutex_enter(&(hba->tran_open_lock));
	if (hba->tran_open_flag == TRAN_OPEN_EXCL) {
		hba->tran_open_flag = 0;
	} else {
		int minor = getminor(dev) & TRAN_MINOR_MASK;
		hba->tran_open_flag &= ~(1 << minor);
	}
	mutex_exit(&(hba->tran_open_lock));
	return (0);
}

/*
 * standard ioctl commands for SCSI hotplugging
 */

/* ARGSUSED */
int
scsi_hba_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
	int *rvalp)
{
	dev_info_t *self;
	dev_info_t *child;
	struct scsi_device *sd;
	scsi_hba_tran_t *hba;
	struct devctl_iocdata *dcp;
	uint_t bus_state;
	int rv = 0;

	self = devt_to_devinfo(dev);
	if (self == NULL)
		return (ENXIO);

	hba = (scsi_hba_tran_t *)ddi_get_driver_private(self);
	if (hba == NULL)
		return (ENXIO);

	/*
	 * read devctl ioctl data
	 */
	if (ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS)
		return (EFAULT);

	switch (cmd) {
	case DEVCTL_DEVICE_GETSTATE:
	case DEVCTL_DEVICE_ONLINE:
	case DEVCTL_DEVICE_OFFLINE:
	case DEVCTL_DEVICE_RESET:
	case DEVCTL_DEVICE_REMOVE:
		if (ndi_dc_getname(dcp) == NULL ||
		    ndi_dc_getaddr(dcp) == NULL) {
			rv = EINVAL;
			break;
		}

		/*
		 * lookup and hold child device
		 */
		child = ndi_devi_find(self,
		    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));
		if (child == NULL) {
			rv = ENXIO;
			break;
		}

		switch (cmd) {
		case DEVCTL_DEVICE_GETSTATE:
			if (ndi_dc_return_dev_state(child, dcp)
			    != NDI_SUCCESS)
			rv = EFAULT;
			break;

		case DEVCTL_DEVICE_ONLINE:
			if (ndi_devi_online(child, NDI_ONLINE_ATTACH) !=
			    NDI_SUCCESS) {
				rv = EIO;
			}
			break;

		case DEVCTL_DEVICE_OFFLINE:
			if ((rv = ndi_devi_offline(child, 0)) !=
			    NDI_SUCCESS) {
				rv = (rv == NDI_BUSY)? EBUSY : EIO;
			}
			break;

		case DEVCTL_DEVICE_RESET:
			if (hba->tran_reset == NULL) {
				rv = ENOTSUP;
				break;
			}

			/*
			 * See DDI_CTLOPS_INITCHILD above
			 */
			sd = (struct scsi_device *)
			    ddi_get_driver_private(child);
			if ((sd == NULL) || hba->tran_reset(
			    &sd->sd_address, RESET_TARGET) == 0) {
				rv = EIO;
			}
			break;

		case DEVCTL_DEVICE_REMOVE:
			if ((rv = ndi_devi_offline(child, NDI_DEVI_REMOVE)) !=
			    NDI_SUCCESS) {
				rv = (rv == NDI_BUSY) ? EBUSY : EIO;
			}
			break;
		} /* end of inner switch */

		break;

	case DEVCTL_BUS_QUIESCE:
		if ((ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS) &&
		    (bus_state == BUS_QUIESCED)) {
			rv = EALREADY;
			break;
		}

		if (hba->tran_quiesce == NULL) {
			rv = ENOTSUP;
		} else if ((*hba->tran_quiesce)(self) != 0) {
			rv = EIO;
		} else {
			(void) ndi_set_bus_state(self, BUS_QUIESCED);
		}
		break;

	case DEVCTL_BUS_UNQUIESCE:
		if ((ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS) &&
		    (bus_state == BUS_ACTIVE)) {
			rv = EALREADY;
			break;
		}

		if (hba->tran_unquiesce == NULL) {
			rv = ENOTSUP;
		} else if ((*hba->tran_unquiesce)(self) != 0) {
			rv = EIO;
		} else {
			(void) ndi_set_bus_state(self, BUS_ACTIVE);
		}
		break;

	case DEVCTL_BUS_RESET:
		/*
		 * Use tran_bus_reset
		 */
		if (hba->tran_bus_reset == NULL) {
			rv = ENOTSUP;
		} else if ((*hba->tran_bus_reset)(self, RESET_BUS) == 0) {
			rv = EIO;
		}
		break;

	case DEVCTL_BUS_RESETALL:
		if (hba->tran_reset == NULL) {
			rv = ENOTSUP;
			break;
		}
		/*
		 * Find a child's scsi_address and invoke tran_reset
		 *
		 * XXX If no child exists, one may to able to fake a child.
		 *	This will be a enhancement for the future.
		 *	For now, we fall back to BUS_RESET.
		 */
		rw_enter(&devinfo_tree_lock, RW_READER);
		child = ddi_get_child(self);
		while (child) {
			if ((sd = (struct scsi_device *)
			    ddi_get_driver_private(child)) != NULL) {
				break;
			}
			child = ddi_get_next_sibling(child);
		}
		rw_exit(&devinfo_tree_lock);

		if (sd != NULL) {
			if ((*hba->tran_reset)(&sd->sd_address, RESET_ALL)
			    == 0) {
				rv = EIO;
			}
		} else if ((hba->tran_bus_reset == NULL) ||
		    ((*hba->tran_bus_reset)(self, RESET_BUS) == 0)) {
			rv = EIO;
		}
		break;

	case DEVCTL_BUS_GETSTATE:
		if (ndi_dc_return_bus_state(self, dcp) != NDI_SUCCESS) {
			rv = EFAULT;
		}
		break;

	case DEVCTL_BUS_CONFIGURE:
		if (ndi_devi_config(self,
		    NDI_ONLINE_ATTACH|NDI_DEVI_PERSIST) != NDI_SUCCESS) {
			rv = EIO;
		}
		break;

	case DEVCTL_BUS_UNCONFIGURE:
		if (ndi_devi_unconfig(self, NDI_DEVI_REMOVE) != NDI_SUCCESS) {
			rv = EIO;
		}
		break;

	default:
		rv = ENOTTY;
	} /* end of outer switch */

	ndi_dc_freehdl(dcp);
	return (rv);
}
