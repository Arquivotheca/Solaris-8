/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)usb_mid.c	1.8	99/10/22 SMI"

/*
 * usb multi interface and common class driver
 *
 *	this driver attempts to attach each interface to a driver
 *	and may eventually handle common class features such as
 *	shared endpoints
 */

#if defined(lint) && !defined(DEBUG)
#define	DEBUG	1
#endif

#include <sys/usb/usba.h>
#include <sys/ddi_impldefs.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/usb/usb_mid/usb_midvar.h>
#include <sys/usb/usba/hubdi.h>

/* Debugging support */
static uint_t usb_mid_errlevel = USB_LOG_L4;
static uint_t usb_mid_errmask = (uint_t)DPRINT_MASK_ALL;
static uint_t usb_mid_instance_debug = (uint_t)-1;
static uint_t usb_mid_show_label = USB_ALLOW_LABEL;

#ifdef DEBUG
/*
 * Dump support
 */
void   usb_mid_dump(uint_t, usb_opaque_t);
static void usb_mid_dump_state(usb_mid_t *, uint_t);
static kmutex_t usb_mid_dump_mutex;
#endif	/* DEBUG */


_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_errlevel))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_errmask))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_instance_debug))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_show_label))

_NOTE(SCHEME_PROTECTS_DATA("unique", msgb))
_NOTE(SCHEME_PROTECTS_DATA("unique", dev_info))

/*
 * Hotplug support
 * Leaf ops (hotplug controls for client devices)
 */
static int usb_mid_open(dev_t *, int, int, cred_t *);
static int usb_mid_close(dev_t, int, int, cred_t *);
static int usb_mid_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static struct cb_ops usb_mid_cb_ops = {
	usb_mid_open,
	usb_mid_close,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	usb_mid_ioctl,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* prop_op */
	NULL,
	D_NEW | D_MP | D_HOTPLUG
};

static int usb_mid_busop_get_eventcookie(dev_info_t *dip,
			dev_info_t *rdip,
			char *eventname,
			ddi_eventcookie_t *cookie,
			ddi_plevel_t *plevelp,
			ddi_iblock_cookie_t *iblock_cookie);
static int usb_mid_busop_add_eventcall(dev_info_t *dip,
			dev_info_t *rdip,
			ddi_eventcookie_t cookie,
			int (*callback)(dev_info_t *dip,
				ddi_eventcookie_t cookie, void *arg,
				void *bus_impldata),
			void *arg);
static int usb_mid_busop_remove_eventcall(dev_info_t *dip,
			dev_info_t *rdip,
			ddi_eventcookie_t cookie);
static int usb_mid_busop_post_event(dev_info_t *dip,
			dev_info_t *rdip,
			ddi_eventcookie_t cookie,
			void *bus_impldata);


/*
 * autoconfiguration data and routines.
 */
static int	usb_mid_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
				void *arg, void **result);
static int	usb_mid_attach(dev_info_t *dev, ddi_attach_cmd_t cmd);
static int	usb_mid_detach(dev_info_t *dev, ddi_detach_cmd_t cmd);

/* other routines */
static void usb_mid_create_pm_components(dev_info_t *dip, usb_mid_t *usb_mid);
static int usb_mid_bus_ctl(dev_info_t *dip, dev_info_t	*rdip,
		ddi_ctl_enum_t	op, void *arg, void *result);
static int usb_mid_power(dev_info_t *dip, int comp, int level);
static int usb_mid_restore_device_state(dev_info_t *dip, usb_mid_t *usb_mid);

/*
 * Busops vector
 */
static struct bus_ops usb_mid_busops = {
	BUSO_REV,
	nullbusmap,			/* bus_map */
	NULL,				/* bus_get_intrspec */
	NULL,				/* bus_add_intrspec */
	NULL,				/* bus_remove_intrspec */
	NULL,				/* XXXX bus_map_fault */
	ddi_dma_map,			/* bus_dma_map */
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,			/* bus_dma_ctl */
	usb_mid_bus_ctl,		/* bus_ctl */
	ddi_bus_prop_op,		/* bus_prop_op */
	usb_mid_busop_get_eventcookie,
	usb_mid_busop_add_eventcall,
	usb_mid_busop_remove_eventcall,
	usb_mid_busop_post_event
};


static struct dev_ops usb_mid_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	usb_mid_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	usb_mid_attach,		/* attach */
	usb_mid_detach,		/* detach */
	nodev,			/* reset */
	&usb_mid_cb_ops,	/* driver operations */
	&usb_mid_busops,	/* bus operations */
	usb_mid_power		/* power */
};

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module. This one is a driver */
	"USB Multi Interface Driver 1.8", /* Name of the module. */
	&usb_mid_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

#define	USB_MID_INITIAL_SOFT_SPACE 4
static	void	*usb_mid_state;


/*
 * prototypes
 */
static void usb_mid_attach_child_drivers(usb_mid_t *usb_mid);
static int usb_mid_cleanup(dev_info_t *dip, usb_mid_t	*usb_mid);
static void usb_mid_register_events(usb_mid_t *usb_mid);
static void usb_mid_deregister_events(usb_mid_t *usb_mid);
static void usb_mid_check_same_device(dev_info_t *dip, usb_mid_t *usb_mid);


/*
 * event definition
 */
/*
 * removal and insertion events
 */
#define	USB_MID_EVENT_TAG_HOT_REMOVAL	0
#define	USB_MID_EVENT_TAG_HOT_INSERTION	1

static ndi_event_definition_t usb_mid_ndi_event_defs[] = {
	{USB_MID_EVENT_TAG_HOT_REMOVAL, DDI_DEVI_REMOVE_EVENT, EPL_KERNEL,
						NDI_EVENT_POST_TO_ALL},
	{USB_MID_EVENT_TAG_HOT_INSERTION, DDI_DEVI_INSERT_EVENT, EPL_KERNEL,
						NDI_EVENT_POST_TO_ALL}
};

#define	USB_MID_N_NDI_EVENTS \
	(sizeof (usb_mid_ndi_event_defs) / sizeof (ndi_event_definition_t))

static	ndi_events_t usb_mid_ndi_events = {
	NDI_EVENTS_REV0, USB_MID_N_NDI_EVENTS, usb_mid_ndi_event_defs};


/*
 * standard driver entry points
 */
int
_init(void)
{
	int rval;

	rval = ddi_soft_state_init(&usb_mid_state, sizeof (struct usb_mid),
	    USB_MID_INITIAL_SOFT_SPACE);
	if (rval != 0) {
		return (rval);
	}

	if ((rval = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&usb_mid_state);
		return (rval);
	}

#ifdef	DEBUG
	mutex_init(&usb_mid_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif	/* DEBUG */

	return (rval);
}


int
_fini(void)
{
	int	rval;

	rval = mod_remove(&modlinkage);

	if (rval) {
		return (rval);
	}

	ddi_soft_state_fini(&usb_mid_state);

#ifdef	DEBUG
	mutex_destroy(&usb_mid_dump_mutex);
#endif	/* DEBUG */

	return (rval);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*ARGSUSED*/
static int
usb_mid_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register dev_t dev;
	usb_mid_t	*usb_mid;
	register int instance, error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		instance = USB_MID_UNIT(dev);
		if ((usb_mid = ddi_get_soft_state(usb_mid_state,
		    instance)) == NULL) {

			return (DDI_FAILURE);
		}
		*result = (void *)usb_mid->mi_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		dev = (dev_t)arg;
		instance = USB_MID_UNIT(dev);
		*result = (void *)(intptr_t)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}

	return (error);
}


/*
 * PM support for this instance and its children
 */
static void
usb_mid_device_idle(usb_mid_t *usb_mid)
{
	int			rval;
	usb_mid_power_t		*midpm;

	USB_DPRINTF_L4(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
		"usb_mid_device_idle : usb_mid : %p", usb_mid);

	mutex_enter(&usb_mid->mi_mutex);
	midpm = usb_mid->mi_pm;
	mutex_exit(&usb_mid->mi_mutex);

	if ((usb_is_pm_enabled(usb_mid->mi_dip) == USB_SUCCESS) &&
	    (midpm->mip_wakeup_enabled)) {
		rval = pm_idle_component(usb_mid->mi_dip, 0);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * track power level changes for children of this instance
 */
static void
usb_mid_set_child_pwrlvl(usb_mid_t *usb_mid, uint8_t ifno, uint8_t power)
{
	USB_DPRINTF_L4(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "usb_mid_set_child_pwrlvl: interface = %d power = %d",
	    ifno, power);

	mutex_enter(&usb_mid->mi_mutex);
	usb_mid->mi_pm->mip_child_pwrstate[ifno] = power;
	mutex_exit(&usb_mid->mi_mutex);
}


/*
 * if this composite device can be put into low power mode
 * return success
 */
usb_mid_can_suspend(usb_mid_t *usb_mid)
{
	uint8_t			ifno;
	int			total_power = 0;

	for (ifno = 0; (total_power == 0) && (ifno < usb_mid->mi_n_interfaces);
	    ifno++) {
		total_power += usb_mid->mi_pm->mip_child_pwrstate[ifno];
	}

	USB_DPRINTF_L4(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "usb_mid_can_suspend: total_power = %d", total_power);

	return (total_power ? USB_FAILURE : USB_SUCCESS);
}


/*
 * child  post attach/detach notification
 */
static void
usb_mid_post_attach(usb_mid_t *usb_mid, uint8_t ifno, struct attachspec *as)
{
	USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "usb_mid_post_attach : interface : %d result = %d",
	    ifno, as->result);

	/* if child successfully attached, set power */
	if (as->result == DDI_SUCCESS) {
		/*
		 * We set power of the new child by default
		 * to full power because if we have a child that
		 * does not have pm, we should never suspend
		 */
		usb_mid_set_child_pwrlvl(usb_mid, ifno, USB_DEV_OS_FULL_POWER);
	}

	/* move timestamp for PM */
	usb_mid_device_idle(usb_mid);
}


static void
usb_mid_post_detach(usb_mid_t *usb_mid, uint8_t ifno, struct detachspec *ds)
{
	USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "hubd_post_detach : ifno = %d result = %d", ifno, ds->result);

	/*
	 * if the device is successfully detached,
	 * mark component as idle
	 */
	if (ds->result == DDI_SUCCESS) {
		/*
		 * We set power of the detached child
		 * to off, so that we can suspend if all
		 * our children are gone
		 */
		usb_mid_set_child_pwrlvl(usb_mid, ifno,
					USB_DEV_OS_POWER_OFF);
	}

	/* move timestamp for PM */
	usb_mid_device_idle(usb_mid);
}


/*
 * child power change notification
 */
static void
usb_mid_post_power(usb_mid_t *usb_mid, uint8_t ifno, power_req_t *reqp)
{
	USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "usb_mid_post_power : interface : %d", ifno);

	if (reqp->req.post_set_power_req.result == DDI_SUCCESS) {

		/* record new power level in our local struct */
		usb_mid_set_child_pwrlvl(usb_mid, ifno,
			reqp->req.post_set_power_req.new_level);

	} else {
		/* record old power in our local struct */
		usb_mid_set_child_pwrlvl(usb_mid, ifno,
			reqp->req.post_set_power_req.old_level);
	}

	/* move timestamp for PM */
	usb_mid_device_idle(usb_mid);
}


/*
 * bus ctl support. we handle notifications here and the
 * rest goes up to root hub/hcd
 */
/*ARGSUSED*/
static int
usb_mid_bus_ctl(dev_info_t *dip,
	dev_info_t	*rdip,
	ddi_ctl_enum_t	op,
	void		*arg,
	void		*result)
{
	usb_device_t *hub_usb_device = usba_get_usb_device(rdip);
	dev_info_t *root_hub_dip = hub_usb_device->usb_root_hub_dip;
	usb_mid_t  *usb_mid;
	power_req_t *reqp;
	struct attachspec *as;
	struct detachspec *ds;
	uint8_t		ifno;

	usb_mid = (usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));
	USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "usb_mid_bus_ctl:\n\t"
	    "dip = 0x%p, rdip = 0x%p, op = 0x%p, arg = 0x%p",
	    dip, rdip, op, arg);

	switch (op) {
	case DDI_CTLOPS_ATTACH:
		as = (struct attachspec *)arg;

		switch (as->when) {
		case DDI_PRE :
			/* nothing to do basically */
			USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
				"DDI_PRE DDI_CTLOPS_ATTACH");
			break;
		case DDI_POST :
			usb_mid_post_attach(usb_mid,
			    usb_get_interface_number(rdip),
			    (struct attachspec *)arg);
			break;
		}
		break;
	case DDI_CTLOPS_DETACH:
		ds = (struct detachspec *)arg;

		switch (ds->when) {
		case DDI_PRE :
			/* nothing to do basically */
			USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
				"DDI_PRE DDI_CTLOPS_DETACH");
			break;
		case DDI_POST :
			usb_mid_post_detach(usb_mid,
			    usb_get_interface_number(rdip),
			    (struct detachspec *)arg);
			break;
		}
		break;
	case DDI_CTLOPS_POWER:
		reqp = (power_req_t *)arg;
		ifno = usb_get_interface_number(rdip);

		switch (reqp->request_type) {
		case PMR_PRE_SET_POWER:
			/* nothing to do basically */
			USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
				"PMR_PRE_SET_POWER");
			break;
		case PMR_POST_SET_POWER:
			usb_mid_post_power(usb_mid, ifno, reqp);
			break;
		}
		break;
	default:

		/* pass to root hub to handle */
		return (usba_bus_ctl(root_hub_dip, rdip, op, arg, result));
	}

	return (DDI_SUCCESS);
}


/*
 * functions to handle power transition for OS levels 0 -> 3
 */
static int
usb_mid_pwrlvl0(usb_mid_t *usb_mid)
{
	int	rval;

	switch (usb_mid->mi_dev_state) {
	case USB_DEV_ONLINE:
		if (usb_mid_can_suspend(usb_mid) == USB_SUCCESS) {
			/* Issue USB D3 command to the device here */
			rval = usb_set_device_pwrlvl3(usb_mid->mi_dip);
			ASSERT(rval == USB_SUCCESS);

			usb_mid->mi_dev_state = USB_DEV_POWERED_DOWN;
			usb_mid->mi_pm->mip_current_power =
						USB_DEV_OS_POWER_OFF;

			return (DDI_SUCCESS);
		} else {
			mutex_exit(&usb_mid->mi_mutex);
			usb_mid_device_idle(usb_mid);
			mutex_enter(&usb_mid->mi_mutex);

			return (DDI_FAILURE);
		}
	case USB_DEV_DISCONNECTED:
	case USB_DEV_CPR_SUSPEND:
	case USB_DEV_POWERED_DOWN:
	default:
		return (DDI_SUCCESS);
	}
}


/* ARGSUSED */
static int
usb_mid_pwrlvl1(usb_mid_t *usb_mid)
{
	int	rval;

	/* Issue USB D2 command to the device here */
	rval = usb_set_device_pwrlvl2(usb_mid->mi_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


/* ARGSUSED */
static int
usb_mid_pwrlvl2(usb_mid_t *usb_mid)
{
	int	rval;

	/* Issue USB D1 command to the device here */
	rval = usb_set_device_pwrlvl1(usb_mid->mi_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


static int
usb_mid_pwrlvl3(usb_mid_t *usb_mid)
{
	int	rval;

	/*
	 * PM framework tries to put us in full power
	 * during system shutdown. If we are disconnected
	 * return success anyways
	 */
	if ((usb_mid->mi_dev_state != USB_DEV_DISCONNECTED) &&
		(usb_mid->mi_dev_state != USB_DEV_CPR_SUSPEND)) {
		/* Issue USB D0 command to the device here */
		rval = usb_set_device_pwrlvl0(usb_mid->mi_dip);
		ASSERT(rval == USB_SUCCESS);

		usb_mid->mi_dev_state = USB_DEV_ONLINE;
		usb_mid->mi_pm->mip_current_power = USB_DEV_OS_FULL_POWER;
	}

	return (DDI_SUCCESS);
}


/* power entry point */
/* ARGSUSED */
static int
usb_mid_power(dev_info_t *dip, int comp, int level)
{
	usb_mid_t	*usb_mid;
	usb_mid_power_t	*midpm;
	int		rval = DDI_FAILURE;

	usb_mid = (usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L4(DPRINT_MASK_PM, usb_mid->mi_log_handle,
	    "usb_mid_power : Begin usb_mid (%p): level = %d",
	    usb_mid, level);

	mutex_enter(&usb_mid->mi_mutex);
	midpm = usb_mid->mi_pm;

	/* check if we are transitioning to a legal power level */
	if (USB_DEV_PWRSTATE_OK(midpm->mip_pwr_states, level)) {

		USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
		    "usb_mid_power: illegal power level = %d "
		    "mip_pwr_states: %x", level, midpm->mip_pwr_states);

		mutex_exit(&usb_mid->mi_mutex);

		return (rval);
	}

	/*
	 * If we are about to raise power and we get this call to lower
	 * power, we return failure
	 */
	if ((midpm->mip_raise_power == B_TRUE) &&
		(level < (int)midpm->mip_current_power)) {

		mutex_exit(&usb_mid->mi_mutex);

		return (DDI_FAILURE);
	}

	switch (level) {
	case USB_DEV_OS_POWER_OFF :
		rval = usb_mid_pwrlvl0(usb_mid);
		break;
	case USB_DEV_OS_POWER_1 :
		rval = usb_mid_pwrlvl1(usb_mid);
		break;
	case USB_DEV_OS_POWER_2 :
		rval = usb_mid_pwrlvl2(usb_mid);
		break;
	case USB_DEV_OS_FULL_POWER :
		rval = usb_mid_pwrlvl3(usb_mid);
		break;
	}

	mutex_exit(&usb_mid->mi_mutex);

	return (rval);
}


/*
 * attach/resume entry point
 */
static int
usb_mid_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	instance = ddi_get_instance(dip);
	usb_mid_t	*usb_mid = NULL;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		usb_mid = (usb_mid_t *)ddi_get_soft_state(usb_mid_state,
								instance);
		(void) usb_mid_restore_device_state(dip, usb_mid);

		return (DDI_SUCCESS);

	default:

		return (DDI_FAILURE);
	}

	/*
	 * Attach:
	 *
	 * Allocate soft state and initialize
	 */
	if (ddi_soft_state_zalloc(usb_mid_state, instance) != DDI_SUCCESS) {
		goto fail;
	}

	usb_mid = (usb_mid_t *)ddi_get_soft_state(usb_mid_state, instance);
	if (usb_mid == NULL) {
		goto fail;
	}

	/* allocate handle for logging of messages */
	usb_mid->mi_log_handle = usb_alloc_log_handle(dip, NULL,
				&usb_mid_errlevel,
				&usb_mid_errmask, &usb_mid_instance_debug,
				&usb_mid_show_label, 0);

	usb_mid->mi_usb_device = usba_get_usb_device(dip);
	usb_mid->mi_dip	= dip;
	usb_mid->mi_instance = instance;
	usb_mid->mi_n_interfaces = usb_mid->mi_usb_device->usb_n_interfaces;

#ifdef	DEBUG
	/* dump support */
	mutex_enter(&usb_mid_dump_mutex);
	usb_mid->mi_dump_ops = usba_alloc_dump_ops();
	usb_mid->mi_dump_ops->usb_dump_ops_version = USBA_DUMP_OPS_VERSION_0;
	usb_mid->mi_dump_ops->usb_dump_func = usb_mid_dump;
	usb_mid->mi_dump_ops->usb_dump_cb_arg = (usb_opaque_t)usb_mid;
	usb_mid->mi_dump_ops->usb_dump_order = USB_DUMPOPS_USB_MID_ORDER;
	usba_dump_register(usb_mid->mi_dump_ops);
	mutex_exit(&usb_mid_dump_mutex);
#endif	/* DEBUG */

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
	    "usb_mid = 0x%x", usb_mid);

	if (ddi_create_minor_node(dip, "devctl", S_IFCHR, instance,
	    DDI_NT_NEXUS, 0) != DDI_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
		    "cannot create devctl minor node");
		goto fail;
	}

	usb_mid->mi_init_state |= USB_MID_MINOR_NODE_CREATED;

	mutex_init(&usb_mid->mi_mutex, NULL, MUTEX_DRIVER, NULL);

	/*
	 * Event handling: definition and registration
	 * get event handle for events that we have defined
	 */
	(void) ndi_event_alloc_hdl(dip, 0, &usb_mid->mi_ndi_event_hdl,
								NDI_SLEEP);

	/* bind event set to the handle */
	if (ndi_event_bind_set(usb_mid->mi_ndi_event_hdl, &usb_mid_ndi_events,
	    NDI_SLEEP)) {
		USB_DPRINTF_L2(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
		    "cannot define events");

		goto fail;
	}

	/* event registration for events from our parent */
	usb_mid_register_events(usb_mid);

	usb_mid->mi_init_state |= USB_MID_EVENTS_REGISTERED;

	usb_mid->mi_dev_state = USB_DEV_ONLINE;

	/*
	 * now create components to power manage this device
	 * before attaching children
	 */
	usb_mid_create_pm_components(dip, usb_mid);

	/* attach driver to each interface below us */
	usb_mid_attach_child_drivers(usb_mid);

	ddi_report_dev(dip);

	return (DDI_SUCCESS);

fail:
	USB_DPRINTF_L1(DPRINT_MASK_ATTA, NULL, "usb_mid%d cannot attach",
		instance);

	if (usb_mid) {
		(void) usb_mid_cleanup(dip, usb_mid);
	}

	return (DDI_FAILURE);
}


/* detach or suspend this instance */
static int
usb_mid_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int	instance = ddi_get_instance(dip);
	usb_mid_t	*usb_mid = (usb_mid_t *)
			ddi_get_soft_state(usb_mid_state, instance);

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
	    "usb_mid_detach: cmd = 0x%x", cmd);

	switch (cmd) {
	case DDI_DETACH:

		return (usb_mid_cleanup(dip, usb_mid));
	case DDI_SUSPEND:
		/* nothing to do */

		return (DDI_SUCCESS);
	default:

		return (DDI_FAILURE);
	}

	_NOTE(NOT_REACHED)
	/* NOTREACHED */
}


/*
 * usb_mid_cleanup:
 *	cleanup usb_mid and deallocate. this function is called for
 *	handling attach failures and detaching including dynamic
 *	reconfiguration
 */
/*ARGSUSED*/
static int
usb_mid_cleanup(dev_info_t *dip, usb_mid_t	*usb_mid)
{
	usb_mid_power_t	*midpm;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
	    "usb_mid_cleanup:");

	if (usb_mid == NULL) {

		return (DDI_SUCCESS);
	}

	/*
	 * deallocate events, if events are still registered
	 * (ie. children still attached) then we have to fail the detach
	 */
	if (usb_mid->mi_ndi_event_hdl &&
	    (ndi_event_free_hdl(usb_mid->mi_ndi_event_hdl) != NDI_SUCCESS)) {

		return (DDI_FAILURE);
	}

	mutex_enter(&usb_mid->mi_mutex);
	midpm = usb_mid->mi_pm;

	if (midpm) {
		if (midpm->mip_child_pwrstate) {
			kmem_free(midpm->mip_child_pwrstate,
			    usb_mid->mi_n_interfaces + 1);
		}
		kmem_free(midpm, sizeof (usb_mid_power_t));
	}
	mutex_exit(&usb_mid->mi_mutex);

	/* event deregistration */
	if (usb_mid->mi_init_state &  USB_MID_EVENTS_REGISTERED) {
		usb_mid_deregister_events(usb_mid);
	}

	/* free children list */
	if (usb_mid->mi_children_dips) {
		kmem_free(usb_mid->mi_children_dips,
					usb_mid->mi_cd_list_length);
	}

	if (usb_mid->mi_init_state & USB_MID_MINOR_NODE_CREATED) {
		ddi_remove_minor_node(dip, NULL);
	}

	mutex_destroy(&usb_mid->mi_mutex);

#ifdef	DEBUG
	mutex_enter(&usb_mid_dump_mutex);
	if (usb_mid->mi_dump_ops) {
		usba_dump_deregister(usb_mid->mi_dump_ops);
		usba_free_dump_ops(usb_mid->mi_dump_ops);
	}
	mutex_exit(&usb_mid_dump_mutex);
#endif	/* DEBUG */

	usb_free_log_handle(usb_mid->mi_log_handle);

	ddi_soft_state_free(usb_mid_state, ddi_get_instance(dip));

	ddi_prop_remove_all(dip);

	return (DDI_SUCCESS);
}


/*
 * usb_mid_attach_child_drivers:
 */
static void
usb_mid_attach_child_drivers(usb_mid_t *usb_mid)
{
	usb_device_t		*child_ud;
	uint_t			n_interfaces;
	uint_t			i;
	size_t			size;
	dev_info_t		*dip;

	child_ud = usba_get_usb_device(usb_mid->mi_dip);

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
		"usb_mid_attach_child_drivers: port = %d, address = %d",
		child_ud->usb_port, child_ud->usb_addr);

	mutex_enter(&usb_mid->mi_mutex);
	n_interfaces = usb_mid->mi_n_interfaces;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
	    "usb_mid_attach_child_drivers: #interfaces = %d",
	    n_interfaces);

	/*
	 * allocate array for keeping track of child dips
	 */
	usb_mid->mi_cd_list_length =
		size = (sizeof (dev_info_t *)) * n_interfaces;

	usb_mid->mi_children_dips = (dev_info_t **)kmem_zalloc(
					size, KM_SLEEP);

	for (i = 0; i < n_interfaces; i++) {
		dip = usb_mid->mi_dip;

		mutex_exit(&usb_mid->mi_mutex);
		dip = usba_bind_driver_to_interface(dip, i, 0);
		mutex_enter(&usb_mid->mi_mutex);

		usb_mid->mi_children_dips[i] = dip;
	}

	mutex_exit(&usb_mid->mi_mutex);
}


/*
 * cleanup interface children
 */
static int
usb_mid_detach_child_drivers(usb_mid_t *usb_mid)
{
	dev_info_t		*dip;
	usb_device_t		*child_ud;
	uint_t			i;
	int			rval;

	child_ud = usba_get_usb_device(usb_mid->mi_dip);

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, usb_mid->mi_log_handle,
	    "usb_mid_detach_child_drivers: port = %d, address = %d",
	    child_ud->usb_port, child_ud->usb_addr);

	mutex_enter(&usb_mid->mi_mutex);

	for (i = 0; i < usb_mid->mi_n_interfaces; i++) {
		dip = usb_mid->mi_children_dips[i];
		if (usba_child_exists(usb_mid->mi_dip, dip) != USB_SUCCESS) {
			continue;
		}

		mutex_exit(&usb_mid->mi_mutex);
		rval = ndi_devi_offline(dip, NDI_DEVI_REMOVE);
		mutex_enter(&usb_mid->mi_mutex);

		if (rval == NDI_SUCCESS) {
			usb_mid->mi_children_dips[i] = NULL;
		}
		/* if the offline fails, continue with other children */
	}

	mutex_exit(&usb_mid->mi_mutex);

	return (USB_SUCCESS);
}


/*
 * event support
 */
static int
usb_mid_busop_get_eventcookie(dev_info_t *dip,
	dev_info_t *rdip,
	char *eventname,
	ddi_eventcookie_t *cookie,
	ddi_plevel_t *plevelp,
	ddi_iblock_cookie_t *iblock_cookie)
{
	usb_mid_t  *usb_mid =
		(usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "usb_mid_busop_get_eventcookie: dip=0x%p, rdip=0x%p, "
	    "event=%s", (void *)dip, (void *)rdip, eventname);
	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "(dip=%s%d rdip=%s%d)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip));

	/* return event cookie, iblock cookie, and level */
	return (ndi_event_retrieve_cookie(usb_mid->mi_ndi_event_hdl,
		rdip, eventname, cookie, plevelp, iblock_cookie,
		NDI_EVENT_NOPASS));
}


static int
usb_mid_busop_add_eventcall(dev_info_t *dip,
	dev_info_t *rdip,
	ddi_eventcookie_t cookie,
	int (*callback)(dev_info_t *dip,
	    ddi_eventcookie_t cookie, void *arg,
	    void *bus_impldata),
	void *arg)
{
	usb_mid_t  *usb_mid =
		(usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "usb_mid_busop_add_eventcall: dip=0x%p, rdip=0x%p "
	    "cookie=0x%x, cb=0xp, arg=0x%p",
	    (void *)dip, (void *)rdip, cookie, (void *)callback, arg);
	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "(dip=%s%d rdip=%s%d event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ndi_event_cookie_to_name(usb_mid->mi_ndi_event_hdl, cookie));

	/* add callback to our event set */
	return (ndi_event_add_callback(usb_mid->mi_ndi_event_hdl,
		rdip, cookie, callback, arg, NDI_EVENT_NOPASS));
}


static int
usb_mid_busop_remove_eventcall(dev_info_t *dip,
	dev_info_t *rdip,
	ddi_eventcookie_t cookie)
{
	usb_mid_t  *usb_mid =
		(usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "usb_mid_busop_remove_eventcall: dip=0x%p, rdip=0x%p "
	    "cookie=0x%x", (void *)dip, (void *)rdip, cookie);
	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "(dip=%s%d rdip=%s%d event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ndi_event_cookie_to_name(usb_mid->mi_ndi_event_hdl, cookie));

	/* remove event registration from our event set */
	return (ndi_event_remove_callback(usb_mid->mi_ndi_event_hdl,
		rdip, cookie, NDI_EVENT_NOPASS));
}


static int
usb_mid_busop_post_event(dev_info_t *dip,
	dev_info_t *rdip,
	ddi_eventcookie_t cookie,
	void *bus_impldata)
{
	usb_mid_t  *usb_mid =
		(usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "usb_mid_busop_post_event: dip=0x%p, rdip=0x%p "
	    "cookie=0x%x, impl=%xp",
	    (void *)dip, (void *)rdip, cookie, bus_impldata);
	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "(dip=%s%d rdip=%s%d event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ndi_event_cookie_to_name(usb_mid->mi_ndi_event_hdl, cookie));

	/* post event to all children registered for this event */
	return (ndi_event_run_callbacks(usb_mid->mi_ndi_event_hdl, rdip,
		cookie, bus_impldata, NDI_EVENT_NOPASS));
}


/*
 * usb_mid_check_same_device():
 *	check if after a reconnect event, the device is the same
 *	and if not, warn the user
 */
static void
usb_mid_check_same_device(dev_info_t *dip, usb_mid_t *usb_mid)
{
	char	*ptr;

	if (usb_check_same_device(dip) == USB_FAILURE) {
		if (ptr = usb_get_usbdev_strdescr(dip)) {
			USB_DPRINTF_L1(DPRINT_MASK_EVENTS,
			    usb_mid->mi_log_handle,
			    "Cannot access device. "
			    "Please reconnect %s ", ptr);
		} else {
			USB_DPRINTF_L1(DPRINT_MASK_EVENTS,
			    usb_mid->mi_log_handle,
			    "Devices not identical to the "
			    "previous one on this port.\n"
			    "Please disconnect and reconnect");
		}
	}
}


/* Mark the device busy by setting mip_raise_power flag */
static void
usb_mid_set_device_busy(usb_mid_t *usb_mid)
{
	USB_DPRINTF_L4(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
		"usb_mid_set_device_busy : usb_mid : %p", usb_mid);

	mutex_enter(&usb_mid->mi_mutex);
	usb_mid->mi_pm->mip_raise_power = B_TRUE;
	mutex_exit(&usb_mid->mi_mutex);

	/* reset the timestamp for PM framework */
	usb_mid_device_idle(usb_mid);
}


/*
 * usb_mid_raise_device_power
 *	raises the power level of the device to the specified power level
 *
 */
static void
usb_mid_raise_device_power(usb_mid_t *usb_mid, int comp, int level)
{
	int rval;

	USB_DPRINTF_L4(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
		"usb_mid_raise_device_power : usb_mid : %p", usb_mid);

	if (usb_mid->mi_pm->mip_wakeup_enabled) {

		usb_mid_set_device_busy(usb_mid);

		rval = pm_raise_power(usb_mid->mi_dip, comp, level);
		if (rval != DDI_SUCCESS) {
			USB_DPRINTF_L2(DPRINT_MASK_EVENTS,
				usb_mid->mi_log_handle,
				"usb_mid_raise_device_power : pm_raise_power "
				"returns : %d", rval);
		}

		mutex_enter(&usb_mid->mi_mutex);
		usb_mid->mi_pm->mip_raise_power = B_TRUE;
		mutex_exit(&usb_mid->mi_mutex);
	}
}


/*
 * usb_mid_restore_device_state
 *	set the original configuration of the device
 */
static int
usb_mid_restore_device_state(dev_info_t *dip, usb_mid_t *usb_mid)
{
	usb_mid_power_t	*midpm;

	USB_DPRINTF_L4(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
		"usb_mid_restore_device_state : usb_mid : %p", usb_mid);

	mutex_enter(&usb_mid->mi_mutex);
	ASSERT((usb_mid->mi_dev_state == USB_DEV_DISCONNECTED) ||
		(usb_mid->mi_dev_state == USB_DEV_CPR_SUSPEND));

	midpm = usb_mid->mi_pm;
	mutex_exit(&usb_mid->mi_mutex);

	/* First bring the device to full power */
	usb_mid_raise_device_power(usb_mid, 0, USB_DEV_OS_FULL_POWER);

	usb_mid_check_same_device(dip, usb_mid);

	mutex_enter(&usb_mid->mi_mutex);
	usb_mid->mi_dev_state = USB_DEV_ONLINE;
	mutex_exit(&usb_mid->mi_mutex);

	/*
	 * if the device had remote wakeup earlier,
	 * enable it again
	 */
	if (midpm->mip_wakeup_enabled) {
		(void) usb_enable_remote_wakeup(usb_mid->mi_dip);
	}

	return (USB_SUCCESS);
}


/*
 * usb_mid_event_callback():
 *	handle disconnect and connect events
 */
static int
usb_mid_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	int	rval;
	usb_mid_t  *usb_mid =
		(usb_mid_t *)ddi_get_soft_state(usb_mid_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "usb_mid_event_callback: dip=0x%p, cookie=0x%x, "
	    "arg=0x%p, impl=0x%p",
	    (void *)dip, cookie, arg, bus_impldata);
	USB_DPRINTF_L3(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
	    "(dip=%s%d event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ndi_event_cookie_to_name(usb_mid->mi_ndi_event_hdl, cookie));

	mutex_enter(&usb_mid->mi_mutex);
	if (cookie == usb_mid->mi_remove_cookie) {
		usb_mid->mi_dev_state = USB_DEV_DISCONNECTED;
		mutex_exit(&usb_mid->mi_mutex);

	} else if (cookie == usb_mid->mi_insert_cookie) {
		mutex_exit(&usb_mid->mi_mutex);
		rval = usb_mid_restore_device_state(dip, usb_mid);

		if (rval == USB_FAILURE) {
			/*
			 * do not pass event to children
			 * since they will start complaining as well
			 */

			return (DDI_EVENT_CLAIMED);
		}
		USB_DPRINTF_L2(DPRINT_MASK_EVENTS, usb_mid->mi_log_handle,
			"device is online again");
	} else {
		mutex_exit(&usb_mid->mi_mutex);
	}

	/* pass all events to the children */
	(void) ndi_event_run_callbacks(usb_mid->mi_ndi_event_hdl, NULL,
		cookie, bus_impldata, NDI_EVENT_NOPASS);

	return (DDI_EVENT_CLAIMED);
}


/*
 * register and deregister for events from our parent
 */
static void
usb_mid_register_events(usb_mid_t *usb_mid)
{
	int rval;
	ddi_plevel_t level;
	ddi_iblock_cookie_t icookie;

	/* get event cookie, discard levl and icookie for now */
	rval = ddi_get_eventcookie(usb_mid->mi_dip, DDI_DEVI_REMOVE_EVENT,
		&usb_mid->mi_remove_cookie, &level, &icookie);

	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(usb_mid->mi_dip,
		    usb_mid->mi_remove_cookie, usb_mid_event_callback, NULL);

		ASSERT(rval == DDI_SUCCESS);
	}
	rval = ddi_get_eventcookie(usb_mid->mi_dip, DDI_DEVI_INSERT_EVENT,
		&usb_mid->mi_insert_cookie, &level, &icookie);
	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(usb_mid->mi_dip,
		    usb_mid->mi_insert_cookie, usb_mid_event_callback, NULL);

		ASSERT(rval == DDI_SUCCESS);
	}
}


static void
usb_mid_deregister_events(usb_mid_t *usb_mid)
{
	int rval;

	if (usb_mid->mi_remove_cookie) {
		rval = ddi_remove_eventcall(usb_mid->mi_dip,
						usb_mid->mi_remove_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}

	if (usb_mid->mi_insert_cookie) {
		rval = ddi_remove_eventcall(usb_mid->mi_dip,
						usb_mid->mi_insert_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * create the pm components required for power management
 */
static void
usb_mid_create_pm_components(dev_info_t *dip, usb_mid_t *usb_mid)
{
	usb_mid_power_t	*midpm;

	USB_DPRINTF_L4(DPRINT_MASK_PM, usb_mid->mi_log_handle,
		"usb_mid_create_pm_components: Begin");

	/* Allocate the PM state structure */
	midpm = kmem_zalloc(sizeof (usb_mid_power_t), KM_SLEEP);

	mutex_enter(&usb_mid->mi_mutex);
	usb_mid->mi_pm = midpm;
	midpm->mip_usb_mid = usb_mid;
	midpm->mip_pm_capabilities = 0; /* XXXX should this be 0?? */
	midpm->mip_raise_power = B_FALSE;
	midpm->mip_current_power = USB_DEV_OS_FULL_POWER;

	/*
	 * alloc memory to save power states of children
	 */
	midpm->mip_child_pwrstate = (uint8_t *)
			kmem_zalloc(usb_mid->mi_n_interfaces + 1, KM_SLEEP);
	mutex_exit(&usb_mid->mi_mutex);

	if (usb_is_pm_enabled(dip) == USB_SUCCESS) {

		usb_enable_parent_notification(dip);

		if (usb_enable_remote_wakeup(dip) == USB_SUCCESS) {
			uint_t		pwr_states;

			/*
			 * if SetFeature(RemoteWakep) failed,
			 * do not create the pm components,
			 * the device will not be power managed now
			 */
			USB_DPRINTF_L2(DPRINT_MASK_PM, usb_mid->mi_log_handle,
			    "usb_mid_create_pm_components: "
			    "Remote Wakeup Enabled");

			if (usb_create_pm_components(dip, &pwr_states) ==
			    USB_SUCCESS) {
				midpm->mip_wakeup_enabled = 1;
				midpm->mip_pwr_states = (uint8_t)pwr_states;
			}
		}
	}

	USB_DPRINTF_L4(DPRINT_MASK_PM, usb_mid->mi_log_handle,
		"usb_mid_create_pm_components: END");
}


/*
 * Device / Hotplug control
 */
/* ARGSUSED3 */
static int
usb_mid_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	int instance;
	struct usb_mid *usb_mid;

	if (otyp != OTYP_CHR) {
		return (EINVAL);
	}

	instance = getminor(*devp);
	usb_mid = (struct usb_mid *)
			ddi_get_soft_state(usb_mid_state, instance);

	if (usb_mid == NULL) {
		return (ENXIO);
	}

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, usb_mid->mi_log_handle,
	    "usb_mid_open:");

	mutex_enter(&usb_mid->mi_mutex);
	if ((flags & FEXCL) && (usb_mid->mi_softstate & USB_MID_SS_ISOPEN)) {
		mutex_exit(&usb_mid->mi_mutex);

		return (EBUSY);
	}

	usb_mid->mi_softstate |= USB_MID_SS_ISOPEN;
	mutex_exit(&usb_mid->mi_mutex);

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, usb_mid->mi_log_handle, "opened");

	return (0);
}


/* ARGSUSED */
static int
usb_mid_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	int instance;
	struct usb_mid *usb_mid;

	if (otyp != OTYP_CHR) {

		return (EINVAL);
	}

	instance = getminor(dev);
	usb_mid = (struct usb_mid *)ddi_get_soft_state(usb_mid_state, instance);

	if (usb_mid == NULL) {

		return (ENXIO);
	}

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, usb_mid->mi_log_handle,
	    "usb_mid_close:");

	mutex_enter(&usb_mid->mi_mutex);
	usb_mid->mi_softstate &= ~USB_MID_SS_ISOPEN;
	mutex_exit(&usb_mid->mi_mutex);

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, usb_mid->mi_log_handle, "closed");

	return (0);
}


/*
 * usb_mid_ioctl: devctl hotplug controls
 */
/* ARGSUSED */
static int
usb_mid_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp)
{
	usb_mid_t *usb_mid;
	dev_info_t *self;
	dev_info_t *child_dip = NULL;
	struct devctl_iocdata *dcp;
	int instance;
	int rv = 0;
	int nrv = 0;

	instance = getminor(dev);
	usb_mid = (struct usb_mid *)ddi_get_soft_state(usb_mid_state, instance);

	if (usb_mid == NULL) {

		return (ENXIO);
	}

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, usb_mid->mi_log_handle,
	    "cmd=%x, arg=%x, mode=%x, cred=%x, rval=%x",
	    cmd, arg, mode, credp, rvalp);

	self = (dev_info_t *)usb_mid->mi_dip;

	/*
	 * read devctl ioctl data
	 */
	if (ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS) {

		return (EFAULT);
	}

	switch (cmd) {
		case DEVCTL_DEVICE_GETSTATE:
			if (ndi_dc_getname(dcp) == NULL ||
			    ndi_dc_getaddr(dcp) == NULL) {
				rv = EINVAL;
				break;
			}

			/*
			 * lookup and hold child device
			 */
			child_dip = ndi_devi_find(self,
			    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));
			if (child_dip == NULL) {
				rv = ENXIO;
				break;
			}

			if (ndi_dc_return_dev_state(child_dip, dcp) !=
			    NDI_SUCCESS) {
				rv = EFAULT;
			}
			break;

		case DEVCTL_DEVICE_ONLINE:
			if (ndi_dc_getname(dcp) == NULL ||
			    ndi_dc_getaddr(dcp) == NULL) {
				rv = EINVAL;
				break;
			}

			/*
			 * lookup and hold child device
			 */
			child_dip = ndi_devi_find(self,
			    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));
			if (child_dip == NULL) {
				rv = ENXIO;
				break;
			}

			if (ndi_devi_online(child_dip, NDI_ONLINE_ATTACH) !=
			    NDI_SUCCESS) {
				rv = EIO;
			}

			break;

		case DEVCTL_DEVICE_OFFLINE:
			if (ndi_dc_getname(dcp) == NULL ||
			    ndi_dc_getaddr(dcp) == NULL) {
				rv = EINVAL;
				break;
			}

			/*
			 * lookup child device
			 */
			child_dip = ndi_devi_find(self,
			    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));

			if (child_dip == NULL) {
				rv = ENXIO;
				break;
			}

			nrv = ndi_devi_offline(child_dip, 0);

			if (nrv == NDI_BUSY) {
				rv = EBUSY;
			} else if (nrv == NDI_FAILURE) {
				rv = EIO;
			}
			break;

		case DEVCTL_DEVICE_REMOVE:
			if (ndi_dc_getname(dcp) == NULL ||
			    ndi_dc_getaddr(dcp) == NULL) {
				rv = EINVAL;
				break;
			}

			/*
			 * lookup child device
			 */
			child_dip = ndi_devi_find(self,
			    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));

			if (child_dip == NULL) {
				rv = ENXIO;
				break;
			}

			nrv = ndi_devi_offline(child_dip, NDI_DEVI_REMOVE);

			if (nrv == NDI_BUSY) {
				rv = EBUSY;
			} else if (nrv == NDI_FAILURE) {
				rv = EIO;
			}
			break;

		case DEVCTL_BUS_CONFIGURE:
			usb_mid_attach_child_drivers(usb_mid);
			break;

		case DEVCTL_BUS_UNCONFIGURE:
			rv = usb_mid_detach_child_drivers(usb_mid);
			if (rv != USB_SUCCESS) {
				rv = EIO;
			}
			break;

		case DEVCTL_DEVICE_RESET:
			rv = ENOTSUP;
			break;


		case DEVCTL_BUS_QUIESCE:
		case DEVCTL_BUS_UNQUIESCE:
		case DEVCTL_BUS_RESET:
		case DEVCTL_BUS_RESETALL:
			rv = ENOTSUP;	/* or call up the tree? */
			break;

		case DEVCTL_BUS_GETSTATE:
			if (ndi_dc_return_bus_state(self, dcp) !=
			    NDI_SUCCESS) {
				rv = EFAULT;
			}
			break;

		default:
			rv = ENOTTY;
	}

	ndi_dc_freehdl(dcp);
	return (rv);
}


#ifdef	DEBUG
/*
 * usb_mid_dump:
 *	Dump all usb_mid related information
 */
void
usb_mid_dump(uint_t flag, usb_opaque_t arg)
{
	usb_mid_t	*usb_mid;

	mutex_enter(&usb_mid_dump_mutex);
	usb_mid = (struct usb_mid *)arg;
	usb_mid_show_label = USB_DISALLOW_LABEL;
	USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
	    "\n***** USB_MID Information *****");

	usb_mid_dump_state(usb_mid, flag);
	usb_mid_show_label = USB_ALLOW_LABEL;
	mutex_exit(&usb_mid_dump_mutex);
}


/*
 * usb_mid_dump_state:
 *	Dump usb_mid state information
 */
static void
usb_mid_dump_state(usb_mid_t *usb_mid, uint_t flag)
{
	usb_mid_power_t	*midpm;
	char	pathname[MAXNAMELEN];

	_NOTE(NO_COMPETING_THREADS_NOW);
	if (flag & USB_DUMP_STATE) {
		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "usb_mid_dump_state: usb_mid = 0x%p", (void *)usb_mid);

		(void) ddi_pathname(usb_mid->mi_dip, pathname);
		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "****** DEVICE: %s", pathname);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_instance: 0x%x\tmi_init_state: 0x%x",
		    usb_mid->mi_instance, usb_mid->mi_init_state);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_dip: 0x%p\tmi_usb_device: 0x%p",
		    usb_mid->mi_dip, usb_mid->mi_usb_device);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_softstate: 0x%x\tmi_n_interfaces: 0x%x",
		    usb_mid->mi_softstate, usb_mid->mi_n_interfaces);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_dev_state: 0x%x", usb_mid->mi_dev_state);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_children_dips: 0x%p\tmi_cd_list_length: 0x%x",
		    usb_mid->mi_children_dips, usb_mid->mi_cd_list_length);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_dump_ops: 0x%p", usb_mid->mi_dump_ops);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_log_handle: 0x%p", usb_mid->mi_log_handle);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_ndi_event_hdl: 0x%p", usb_mid->mi_ndi_event_hdl);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_remove_cookie: 0x%p", usb_mid->mi_remove_cookie);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "\tmi_insert_cookie: 0x%p", usb_mid->mi_insert_cookie);

		midpm = usb_mid->mi_pm;

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
			"\n\nusb_mid power structure at 0x%p", midpm);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
			"mip_usb_mid 0x%p", midpm->mip_usb_mid);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
			"mip_wakeup_enabled 0x%x\t\tmip_pwr_states 0x%x",
			midpm->mip_wakeup_enabled, midpm->mip_pwr_states);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
			"mip_pm_capabilities 0x%x\t\tmip_current_power 0x%x",
			midpm->mip_pm_capabilities,
			midpm->mip_current_power);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
			"mip_raise_power 0x%x\t\tmip_child_pwrstate 0%p",
			midpm->mip_raise_power, midpm->mip_child_pwrstate);
	}

	if ((flag & USB_DUMP_DESCRIPTORS) && usb_mid->mi_dip) {
		USB_DPRINTF_L3(DPRINT_MASK_DUMPING, usb_mid->mi_log_handle,
		    "****** USB_MID descriptors******");

		usba_dump_descriptors(usb_mid->mi_dip, USB_DISALLOW_LABEL);
	}

	if ((flag & USB_DUMP_USB_DEVICE) && usb_mid->mi_usb_device) {
		usba_dump_usb_device(usb_mid->mi_usb_device, flag);
	}

	_NOTE(COMPETING_THREADS_NOW);
}
#endif	/* DEBUG */
