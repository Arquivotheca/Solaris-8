/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hubdi.c	1.16	99/11/18 SMI"

/*
 * USBA: Solaris USB Architecture support for the hub
 * including root hub
 */
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/ddi_impldefs.h>
#include <sys/usb/hubd/hub.h>
#include <sys/usb/hubd/hubdvar.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/ndi_impldefs.h>
#include <sys/taskq.h>

/*
 * Prototypes for static functions
 */
static	int	usba_hubdi_bus_ctl(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			ddi_ctl_enum_t		op,
			void			*arg,
			void			*result);

static int	usba_hubdi_map_fault(
			dev_info_t		*dip,
			dev_info_t		*rdip,
			struct hat		*hat,
			struct seg		*seg,
			caddr_t 		addr,
			struct devpage		*dp,
			uint_t			pfn,
			uint_t			prot,
			uint_t			lock);

static int hubd_busop_get_eventcookie(dev_info_t *dip,
			dev_info_t *rdip,
			char *eventname,
			ddi_eventcookie_t *cookie,
			ddi_plevel_t *plevelp,
			ddi_iblock_cookie_t *iblock_cookie);
static int hubd_busop_add_eventcall(dev_info_t *dip,
			dev_info_t *rdip,
			ddi_eventcookie_t cookie,
			int (*callback)(dev_info_t *dip,
				ddi_eventcookie_t cookie, void *arg,
				void *bus_impldata),
			void *arg);
static int hubd_busop_remove_eventcall(dev_info_t *dip,
			dev_info_t *rdip,
			ddi_eventcookie_t cookie);
static int hubd_busop_post_event(dev_info_t *dip,
			dev_info_t *rdip,
			ddi_eventcookie_t cookie,
			void *bus_impldata);


/*
 * Busops vector for USB HUB's
 */
struct bus_ops usba_hubdi_busops =	{
	BUSO_REV,
	nullbusmap,			/* bus_map */
	NULL,				/* bus_get_intrspec */
	NULL,				/* bus_add_intrspec */
	NULL,				/* bus_remove_intrspec */
	usba_hubdi_map_fault,		/* bus_map_fault */
	ddi_dma_map,			/* bus_dma_map */
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,			/* bus_dma_ctl */
	usba_hubdi_bus_ctl,		/* bus_ctl */
	ddi_bus_prop_op,		/* bus_prop_op */
	hubd_busop_get_eventcookie,
	hubd_busop_add_eventcall,
	hubd_busop_remove_eventcall,
	hubd_busop_post_event
};


/*
 * local variables
 */
static kmutex_t	usba_hubdi_mutex;	/* protects USBA HUB data structures */

static usba_list_entry_t	usba_hubdi_list;

usb_log_handle_t 	hubdi_log_handle;
uint_t			hubdi_errlevel = USB_LOG_L4;
uint_t			hubdi_errmask = (uint_t)-1;
uint_t			hubdi_show_label = USB_ALLOW_LABEL;

_NOTE(DATA_READABLE_WITHOUT_LOCK(hubdi_show_label))

#ifdef	DEBUG
/*
 * Dump support
 */
static void hubd_dump(uint_t, usb_opaque_t);
static void hubd_dump_state(hubd_t *, uint_t);
static kmutex_t	hubd_dump_mutex;
#endif	/* DEBUG */


/*
 * initialize private data
 */
void
usba_hubdi_initialization()
{
	hubdi_log_handle = usb_alloc_log_handle(NULL, "hubdi", &hubdi_errlevel,
				&hubdi_errmask, NULL, &hubdi_show_label, 0);

	USB_DPRINTF_L4(DPRINT_MASK_HUBDI, hubdi_log_handle,
	    "usba_hubdi_initialization");

	mutex_init(&usba_hubdi_mutex, NULL, MUTEX_DRIVER, NULL);

	mutex_init(&usba_hubdi_list.list_mutex, NULL, MUTEX_DRIVER, NULL);

#ifdef	DEBUG
	mutex_init(&hubd_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif	/* DEBUG */
}


void
usba_hubdi_destroy()
{
	USB_DPRINTF_L4(DPRINT_MASK_HUBDI, hubdi_log_handle,
	    "usba_hubdi_destroy");

#ifdef	DEBUG
	mutex_destroy(&hubd_dump_mutex);
#endif	/* DEBUG */

	mutex_destroy(&usba_hubdi_mutex);
	mutex_destroy(&usba_hubdi_list.list_mutex);

	usb_free_log_handle(hubdi_log_handle);
}


/*
 * Called by an	HUB to attach an instance of the driver
 *	make this instance known to USBA
 *	the HUB	should initialize usb_hubdi structure prior
 *	to calling this	interface
 */
static int
usba_hubdi_register(dev_info_t	*dip,
		usb_hubdi_ops_t *hubdi_ops,
		uint_t		flags)
{
	usb_hubdi_t *hubdi = kmem_zalloc(sizeof (usb_hubdi_t), KM_SLEEP);
	usb_device_t *usb_device = usba_get_usb_device(dip);

	USB_DPRINTF_L4(DPRINT_MASK_HUBDI, hubdi_log_handle,
	    "usb_hubdi_register: %s", ddi_node_name(dip));

	hubdi->hubdi_dip = dip;
	hubdi->hubdi_flags = flags;
	hubdi->hubdi_ops = hubdi_ops;

	usb_device->usb_hubdi = hubdi;

	/*
	 * add this hubdi instance to the list of known hubdi's
	 */
	mutex_init(&hubdi->hubdi_list.list_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_enter(&usba_hubdi_mutex);
	usba_add_list(&usba_hubdi_list, &hubdi->hubdi_list);
	mutex_exit(&usba_hubdi_mutex);

	return (DDI_SUCCESS);
}


/*
 * Called by an	HUB to detach an instance of the driver
 */
static int
usba_hubdi_deregister(dev_info_t *dip)
{
	usb_device_t *usb_device = usba_get_usb_device(dip);
	usb_hubdi_t *hubdi = usb_device->usb_hubdi;

	USB_DPRINTF_L4(DPRINT_MASK_HUBDI, hubdi_log_handle,
	    "usb_hubdi_deregister: %s", ddi_node_name(dip));

	mutex_enter(&usba_hubdi_mutex);
	usba_remove_list(&usba_hubdi_list, &hubdi->hubdi_list);
	mutex_exit(&usba_hubdi_mutex);

	mutex_destroy(&hubdi->hubdi_list.list_mutex);

	kmem_free(hubdi, sizeof (usb_hubdi_t));

	return (DDI_SUCCESS);
}


/*
 * allocate and free and retrieve hubdi_ops structure
 */
static usb_hubdi_ops_t *
usba_alloc_hubdi_ops()
{
	return (kmem_zalloc(sizeof (usb_hubdi_ops_t), KM_SLEEP));
}


static void
usba_free_hubdi_ops(usb_hubdi_ops_t *usb_hubdi_ops)
{
	kmem_free(usb_hubdi_ops, sizeof (usb_hubdi_ops_t));
}


/*
 * misc bus routines currently not used
 */
/*ARGSUSED*/
static int
usba_hubdi_map_fault(dev_info_t *dip,
	dev_info_t	*rdip,
	struct hat	*hat,
	struct seg	*seg,
	caddr_t 	addr,
	struct devpage	*dp,
	uint_t		pfn,
	uint_t		prot,
	uint_t		lock)
{
	return (DDI_FAILURE);
}


/*
 * root hub support. the root hub uses the same devi as the HCD
 */
int
usba_hubdi_bind_root_hub(dev_info_t *dip,
	uchar_t	*root_hub_config_descriptor,
	size_t config_length,
	usb_device_descr_t *root_hub_device_descriptor)
{
	usb_device_t *usb_device;
	usb_hcdi_t *hcdi = usba_hcdi_get_hcdi(dip);
	hubd_t	*root_hubd;

	if (ndi_prop_create_boolean(DDI_DEV_T_NONE, dip,
				"root-hub") != NDI_SUCCESS) {
		return (USB_FAILURE);
	}

	root_hubd = kmem_zalloc(sizeof (hubd_t), KM_SLEEP);

	/*
	 * create and initialize a usb_device structure
	 */
	usb_device = usba_alloc_usb_device();

	mutex_enter(&usb_device->usb_mutex);
	usb_device->usb_hcdi_ops = hcdi->hcdi_ops;
	usb_device->usb_parent_hubdi_ops = NULL;
	usb_device->usb_root_hub_dip = dip;
	usb_device->usb_port_status = USB_HIGH_SPEED_DEV;
	usb_device->usb_config = root_hub_config_descriptor;
	usb_device->usb_config_length = config_length;
	usb_device->usb_dev_descr = root_hub_device_descriptor;
	usb_device->usb_n_configs = 1;
	usb_device->usb_n_interfaces = 1;
	usb_device->usb_port = 1;
	usb_device->usb_addr = ROOT_HUB_ADDR;
	usb_device->usb_root_hubd = root_hubd;
	mutex_exit(&usb_device->usb_mutex);

	usba_set_usb_device(dip, usb_device);

	/*
	 * inform the hcd about this new usb device
	 */
	if (hcdi->hcdi_ops->usb_hcdi_client_init(usb_device) !=
	    USB_SUCCESS) {
		usba_free_usb_device(usb_device);

		return (USB_FAILURE);
	}

	/*
	 * "attach" the root hub driver
	 */
	if (usba_hubdi_attach(dip, DDI_ATTACH) != DDI_SUCCESS) {

		hcdi->hcdi_ops->usb_hcdi_client_free(usb_device);
		usba_free_usb_device(usb_device);
		usba_set_usb_device(dip, NULL);

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


int
usba_hubdi_unbind_root_hub(dev_info_t *dip)
{
	usb_device_t *usb_device = usba_get_usb_device(dip);
	usb_hcdi_t *hcdi = usba_hcdi_get_hcdi(dip);

	if (usba_hubdi_detach(dip, DDI_DETACH) != DDI_SUCCESS) {

		return (USB_FAILURE);
	}

	hcdi->hcdi_ops->usb_hcdi_client_free(usb_device);

	kmem_free(usb_device->usb_root_hubd, sizeof (hubd_t));
	usba_free_usb_device(usb_device);

	return (USB_SUCCESS);
}


/*
 * Actual Hub Driver support code:
 *	shared by root hub and non-root hubs
 */

/* Debugging support */
static uint_t hubd_errlevel = USB_LOG_L4;
static uint_t hubd_errmask = (uint_t)DPRINT_MASK_ALL;
static uint_t hubd_instance_debug = (uint_t)-1;
static uint_t hubd_show_label = USB_ALLOW_LABEL;

_NOTE(DATA_READABLE_WITHOUT_LOCK(hubd_errlevel))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hubd_errmask))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hubd_instance_debug))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hubd_show_label))

_NOTE(SCHEME_PROTECTS_DATA("unique", msgb))
_NOTE(SCHEME_PROTECTS_DATA("unique", dev_info))


/*
 * local variables:
 *
 * Amount of time to wait between resetting the port and accessing
 * the device.	The value is in microseconds.
 */
static uint_t hubd_device_delay = 1000000;

/*
 * do not service future hotplug events until all drivers have
 * been attached and dacf has completed its work
 * this allows things to settle down and avoids nasty races in
 * client drivers or dacf framework
 */
static uint_t hubd_hotplug_delay = 5;

/*
 * enumeration retry
 */
#define	HUBD_PORT_RETRY	3
static uint_t hubd_retry_enumerate = HUBD_PORT_RETRY;

void	*hubd_state;

/*
 * prototypes
 */
static int hubd_cleanup(dev_info_t *dip, hubd_t  *hubd);
static int hubd_check_ports(hubd_t  *hubd);

static int hubd_start_polling(hubd_t *hubd);
static void hubd_stop_polling(hubd_t *hubd);

static int hubd_read_callback(usb_pipe_handle_t pipe,
		usb_opaque_t	callback_arg,
		mblk_t		*data);
static int hubd_exception_callback(usb_pipe_handle_t pipe,
		usb_opaque_t	callback_arg,
		uint_t		completion_reason,
		mblk_t		*data,
		uint_t		flag);
static void hubd_intr_pipe_reset_callback(usb_opaque_t arg1,
		int rval,
		uint_t dummy_arg3);
static void hubd_hotplug_thread(void *arg);
static int hubd_create_child(dev_info_t *dip,
		hubd_t		*hubd,
		usb_device_t	*usb_device,
		usb_hubdi_ops_t *usb_hubdi_ops,
		usb_port_status_t port_status,
		usb_port_t	port,
		int		iteration);
static int hubd_delete_child(hubd_t *hubd, usb_port_t port, uint_t flag);
static int hubd_delete_all_children(hubd_t *hubd);

static int hubd_open_default_pipe(hubd_t *hubd, uint_t flag);
static int hubd_close_default_pipe(hubd_t *hubd);

static int hubd_get_hub_descriptor(hubd_t *hubd);

static int hubd_reset_port(hubd_t *hubd, usb_port_t port);

static int hubd_get_hub_status(hubd_t *hubd);

static void hubd_handle_port_connect(hubd_t *hubd, usb_port_t port);

static void hubd_no_powerswitch_check(hubd_t *hubd);

static int hubd_disable_port(hubd_t *hubd, usb_port_t port);

static int hubd_enable_port(hubd_t *hubd, usb_port_t port);
static void hubd_recover_disabled_port(hubd_t *hubd, usb_port_t port);

static int hubd_determine_port_status(hubd_t *hubd, usb_port_t port,
		uint16_t *status, uint16_t *change, uint_t ack_flag);

static int hubd_enable_all_port_power(hubd_t *hubd);
static int hubd_disable_all_port_power(hubd_t *hubd);
static int hubd_disable_port_power(hubd_t *hubd, usb_port_t port);

static void hubd_free_usb_device(hubd_t *hubd, usb_device_t *usb_device);

static int hubd_devctl_bus_configure(hubd_t *hubd);

static int hubd_cpr_suspend(hubd_t *hubd);
static int hubd_restore_device_state(dev_info_t *dip, hubd_t *hubd);
static int hubd_rstport(hubd_t *hubd, usb_port_t port);
static int hubd_setdevaddr(hubd_t *hubd, usb_port_t port);
static void hubd_setdevconfig(hubd_t *hubd, usb_port_t port);

static void hubd_register_events(hubd_t *hubd);
static void hubd_deregister_events(hubd_t *hubd);
static void hubd_post_disconnect_event(hubd_t *hubd, usb_port_t port);
static void hubd_post_connect_event(hubd_t *hubd, usb_port_t port);
static void hubd_create_pm_components(dev_info_t *dip, hubd_t *hubd);


/*
 * removal and insertion events
 */
#define	HUBD_EVENT_TAG_HOT_REMOVAL	0
#define	HUBD_EVENT_TAG_HOT_INSERTION	1

static ndi_event_definition_t hubd_ndi_event_defs[] = {
	{HUBD_EVENT_TAG_HOT_REMOVAL, DDI_DEVI_REMOVE_EVENT, EPL_KERNEL,
						NDI_EVENT_POST_TO_ALL},
	{HUBD_EVENT_TAG_HOT_INSERTION, DDI_DEVI_INSERT_EVENT, EPL_KERNEL,
						NDI_EVENT_POST_TO_ALL}
	};

#define	HUBD_N_NDI_EVENTS \
	(sizeof (hubd_ndi_event_defs) / sizeof (ndi_event_definition_t))

static ndi_events_t hubd_ndi_events = {
	NDI_EVENTS_REV0, HUBD_N_NDI_EVENTS, hubd_ndi_event_defs};


/*
 * hubd_get_soft_state() returns the hubd soft state
 */
static hubd_t *
hubd_get_soft_state(dev_info_t *dip)
{
	if (dip == NULL) {
		return (NULL);
	}

	if (usba_is_root_hub(dip)) {
		usb_device_t *usb_device = usba_get_usb_device(dip);

		return (usb_device->usb_root_hubd);
	} else {
		int instance = ddi_get_instance(dip);

		return (ddi_get_soft_state(hubd_state, instance));
	}
}


/*
 * PM support functions:
 */

/* mark the device as idle */
static void
hubd_device_idle(hubd_t *hubd)
{
	int		rval;

	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
		"hubd_device_idle : hubd : %p", hubd);

	if ((usb_is_pm_enabled(hubd->h_dip) == USB_SUCCESS) &&
	    (hubd->h_hubpm->hubp_wakeup_enabled)) {
		rval = pm_idle_component(hubd->h_dip, 0);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/* Mark the device busy by setting hubp_raise_power flag */
static void
hubd_set_device_busy(hubd_t *hubd)
{
	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
		"hubd_set_device_busy : hubd : %p", hubd);

	mutex_enter(HUBD_MUTEX(hubd));
	hubd->h_hubpm->hubp_raise_power = B_TRUE;
	mutex_exit(HUBD_MUTEX(hubd));

	/* now reset the timestamp for PM framework */
	hubd_device_idle(hubd);
}


/*
 * hubd_raise_device_power
 *	raises the power level of the device to the specified power level
 */
static void
hubd_raise_device_power(hubd_t *hubd, int comp, int level)
{
	int rval;


	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
		"hubd_raise_device_power : hubd : %p", hubd);

	if (hubd->h_hubpm->hubp_wakeup_enabled) {

		hubd_set_device_busy(hubd);

		rval = pm_raise_power(hubd->h_dip, comp, level);
		if (rval != DDI_SUCCESS) {
			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
				"hubd_raise_device_power : pm_raise_power "
				"returns : %d", rval);
		}

		mutex_enter(HUBD_MUTEX(hubd));
		hubd->h_hubpm->hubp_raise_power = B_FALSE;
		mutex_exit(HUBD_MUTEX(hubd));
	}
}


/*
 * track power level changes for children of this instance
 */
static void
hubd_set_child_pwrlvl(hubd_t *hubd, usb_port_t port, uint8_t power)
{
	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_set_child_pwrlvl: port : %d power : %d",
	    port, power);

	mutex_enter(HUBD_MUTEX(hubd));
	hubd->h_hubpm->hubp_child_pwrstate[port] = power;
	mutex_exit(HUBD_MUTEX(hubd));
}


/*
 * given a child dip, locate its port number
 */
static usb_port_t
hubd_child_dip2port(hubd_t *hubd, dev_info_t *dip)
{
	usb_port_t	port;

	mutex_enter(HUBD_MUTEX(hubd));

	for (port = 1; port <= hubd->h_hub_descr.bNbrPorts; port++) {

		if (hubd->h_children_dips[port] == dip) {
			break;
		}
	}

	mutex_exit(HUBD_MUTEX(hubd));

	return (port);
}


/*
 * if the hub can be put into low power mode, return success
 */
static int
hubd_can_suspend(hubd_t *hubd)
{
	hub_power_t	*hubpm;
	int		total_power = 0;
	usb_port_t	port;

	hubpm = hubd->h_hubpm;

	for (port = 1; (total_power == 0) &&
	    (port <= hubd->h_hub_descr.bNbrPorts); port++) {
		total_power += hubpm->hubp_child_pwrstate[port];
	}

	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
		"hubd_can_suspend: %d", total_power);

	return (total_power ? USB_FAILURE : USB_SUCCESS);
}


/*
 * resume port depending on current device state
 */
static void
hubd_resume_port(hubd_t *hubd, usb_port_t port)
{
	int	rval;
	uint_t	completion_reason;
	uint16_t	status;
	uint16_t	change;

	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
		"hubd_resume_port: hubd : %p port : %d", hubd, port);

	mutex_enter(HUBD_MUTEX(hubd));

	switch (hubd->h_dev_state) {
	case USB_DEV_HUB_CHILD_PWRLVL:

		rval = hubd_open_default_pipe(hubd,
			USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
		ASSERT(rval == USB_SUCCESS);

		/*
		 * Device has initiated a wakeup.
		 * Issue a ClearFeature(PortSuspend)
		 */
		mutex_exit(HUBD_MUTEX(hubd));
		rval = usb_pipe_sync_device_ctrl_send(hubd->h_default_pipe,
			CLEAR_PORT_FEATURE,
			USB_REQ_CLEAR_FEATURE,
			CFS_PORT_SUSPEND,
			port,
			0,
			NULL,
			&completion_reason,
			USB_FLAGS_ENQUEUE);

		if (rval != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
			    "ClearFeature(PortSuspend) fails"
			    "rval = %d cr = %d", rval, completion_reason);

			rval = usb_pipe_reset(hubd->h_default_pipe,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);
		}

		mutex_enter(HUBD_MUTEX(hubd));

		/* either way ack changes on the port */
		(void) hubd_determine_port_status(hubd, port,
			&status, &change, HUBD_ACK_CHANGES);

		rval = hubd_close_default_pipe(hubd);
		ASSERT(rval == USB_SUCCESS);

		break;
	case USB_DEV_POWERED_DOWN:

		/*
		 * To resume a port on a suspended hub, we need to resume
		 * the hub itself before resuming the port
		 */
		mutex_exit(HUBD_MUTEX(hubd));
		hubd_raise_device_power(hubd, 0, USB_DEV_OS_FULL_POWER);
		mutex_enter(HUBD_MUTEX(hubd));

		/*
		 * after a change of power level, we check the devstate
		 * and fallthru
		 */
		ASSERT(hubd->h_dev_state == USB_DEV_ONLINE);

		/* FALLTHRU */
	case USB_DEV_HUB_DISCONNECT_RECOVER:
		/*
		 * When hubd's connect event callback posts a connect
		 * event to its child, it results in this busctl call
		 * which is valid
		 */

		/* FALLTHRU */
	case USB_DEV_ONLINE:

		/* open the default pipe */
		rval = hubd_open_default_pipe(hubd,
			USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
		ASSERT(rval == USB_SUCCESS);

		/*
		 * stop polling on the intr pipe so that we do not
		 * kick off the hotplug thread
		 */
		while (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {
			mutex_exit(HUBD_MUTEX(hubd));
			delay(1);
			mutex_enter(HUBD_MUTEX(hubd));
		}

		mutex_exit(HUBD_MUTEX(hubd));
		rval = USB_FAILURE;
		if (hubd->h_ep1_ph && (rval == USB_FAILURE)) {
			rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
				USB_FLAGS_SLEEP, NULL, NULL);
			if (rval == USB_FAILURE) {
				delay(1);
			}
		}

		/* Now ClearFeature(PortSuspend) */
		rval = usb_pipe_sync_device_ctrl_send(hubd->h_default_pipe,
			CLEAR_PORT_FEATURE,
			USB_REQ_CLEAR_FEATURE,
			CFS_PORT_SUSPEND,
			port,
			0,
			NULL,
			&completion_reason,
			USB_FLAGS_ENQUEUE);

		if (rval != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
				"ClearFeature(PortSuspend) fails"
				"rval = %d cr = %d", rval, completion_reason);

			rval = usb_pipe_reset(hubd->h_default_pipe,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);
		}

		mutex_enter(HUBD_MUTEX(hubd));

		(void) hubd_determine_port_status(hubd, port,
			&status, &change, HUBD_ACK_CHANGES);

		/* wait for the resume to finish */
		while (status & PORT_STATUS_PSS) {
			delay(2);
			(void) hubd_determine_port_status(hubd, port,
				&status, &change, HUBD_ACK_CHANGES);
		}

		rval = hubd_close_default_pipe(hubd);
		ASSERT(rval == USB_SUCCESS);

		/*
		 * now restart polling on the intr pipe
		 * only if hotplug thread is not running
		 */
		if ((hubd->h_hotplug_thread == NULL) && hubd->h_ep1_ph) {
			mutex_exit(HUBD_MUTEX(hubd));

			rval = usb_pipe_start_polling(hubd->h_ep1_ph,
				USB_FLAGS_SHORT_XFER_OK);
			ASSERT(rval == USB_SUCCESS);

			mutex_enter(HUBD_MUTEX(hubd));
		}

		break;
	case USB_DEV_DISCONNECTED:
		/* Ignore - NO Operation */
		break;

	case USB_DEV_CPR_SUSPEND:
	default:
		USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
		    "Improper state for port Resume");
		break;
	}

	mutex_exit(HUBD_MUTEX(hubd));
}


/*
 * suspend port depending on device state
 */
static void
hubd_suspend_port(hubd_t *hubd, usb_port_t port)
{
	int		rval;
	int		retry;
	uint_t		completion_reason;
	uint16_t	status;
	uint16_t	change;

	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_suspend_port: hubd : %p port : %d", hubd, port);

	mutex_enter(HUBD_MUTEX(hubd));

	switch (hubd->h_dev_state) {
	case USB_DEV_HUB_DISCONNECT_RECOVER:
		/*
		 * When hubd's connect event callback posts a connect
		 * event to its child, it results in this busctl call
		 * which is valid
		 */
		/* FALLTHRU */
	case USB_DEV_HUB_CHILD_PWRLVL:
		/*
		 * When one child is resuming, the other could timeout
		 * and go to low power mode, which is valid
		 */
		/* FALLTHRU */
	case USB_DEV_ONLINE:
		/* open the default pipe */
		rval = hubd_open_default_pipe(hubd,
			USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
		ASSERT(rval == USB_SUCCESS);

		/*
		 * stop polling on the intr pipe so that we do not
		 * kick off the hotplug thread
		 */
		while (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {
			mutex_exit(HUBD_MUTEX(hubd));
			delay(1);
			mutex_enter(HUBD_MUTEX(hubd));
		}

		mutex_exit(HUBD_MUTEX(hubd));
		rval = USB_FAILURE;
		if (hubd->h_ep1_ph && (rval == USB_FAILURE)) {
			rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
				USB_FLAGS_SLEEP, NULL, NULL);
			if (rval == USB_FAILURE) {
				delay(1);
			}
		}

		/*
		 * Some devices kick off an unprovoked resume
		 * We try our best to suspend them by retrying
		 */
		for (retry = 0; retry < 5; retry++) {
			/* Now SetFeature(PortSuspend) */
			rval = usb_pipe_sync_device_ctrl_send(
				hubd->h_default_pipe,
				SET_PORT_FEATURE,
				USB_REQ_SET_FEATURE,
				CFS_PORT_SUSPEND,
				port,
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE);

			if (rval != USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PM,
					hubd->h_log_handle,
					"SetFeature(PortSuspend) fails"
					"rval = %d cr = %d", rval,
					completion_reason);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}

			/*
			 * some devices start an unprovoked resume
			 * wait and check port status after some time
			 */
			delay(2);

			/* either ways ack changes on the port */
			mutex_enter(HUBD_MUTEX(hubd));
			(void) hubd_determine_port_status(hubd, port,
				&status, &change, HUBD_ACK_CHANGES);
			mutex_exit(HUBD_MUTEX(hubd));

			if (status & PORT_STATUS_PSS) {

				/* the port is indeed suspended */
				break;
			}
		}

		mutex_enter(HUBD_MUTEX(hubd));

		rval = hubd_close_default_pipe(hubd);
		ASSERT(rval == USB_SUCCESS);

		/*
		 * now restart polling on the intr pipe
		 * only if hotplug thread is not running
		 */
		if ((hubd->h_hotplug_thread == NULL) && hubd->h_ep1_ph) {
			mutex_exit(HUBD_MUTEX(hubd));

			if (hubd->h_ep1_ph) {
				rval = usb_pipe_start_polling(hubd->h_ep1_ph,
					USB_FLAGS_SHORT_XFER_OK);
				ASSERT(rval == USB_SUCCESS);
			}

			mutex_enter(HUBD_MUTEX(hubd));
		}

		break;

	case USB_DEV_DISCONNECTED:
		/* Ignore - NO Operation */
		break;

	case USB_DEV_CPR_SUSPEND:
	case USB_DEV_POWERED_DOWN:
	default:
		USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
		    "Improper state for port Suspend");
		break;
	}

	mutex_exit(HUBD_MUTEX(hubd));
}


/*
 * child post attach/detach notifications
 */
static void
hubd_post_attach(hubd_t *hubd, usb_port_t port, struct attachspec *as)
{
	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_post_attach : port : %d result = %d",
	    port, as->result);

	/*
	 * if the device is successfully attached and is the
	 * first device to attach, mark component as busy
	 */
	if (as->result == DDI_SUCCESS) {

		/*
		 * We set power of the new child by default
		 * to 3. Because if we have a child that
		 * does not have pm, we should never suspend
		 */
		hubd_set_child_pwrlvl(hubd, port, USB_DEV_OS_FULL_POWER);
	}

	/* move timestamp for PM */
	hubd_device_idle(hubd);
}


static void
hubd_post_detach(hubd_t *hubd, usb_port_t port, struct detachspec *ds)
{
	USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_post_detach : port : %d result = %d", port, ds->result);

	/*
	 * if the device is successfully detached and is the
	 * last device to detach, mark component as idle
	 */
	if (ds->result == DDI_SUCCESS) {

		/*
		 * We set power of the detached child
		 * to 0, so that we can suspend if all
		 * our children are gone
		 */
		hubd_set_child_pwrlvl(hubd, port, USB_DEV_OS_POWER_OFF);
	}

	/* move timestamp for PM */
	hubd_device_idle(hubd);
}


/*
 * hubd_post_power
 *	After the child's power entry point has been called
 *	we record its power level in our local struct.
 *	If the device has powered off, we suspend port
 */
static void
hubd_post_power(hubd_t *hubd, usb_port_t port, power_req_t *reqp)
{
	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_post_power : port : %d", port);

	if (reqp->req.post_set_power_req.result == DDI_SUCCESS) {

		/* record this power in our local struct */
		hubd_set_child_pwrlvl(hubd, port,
			reqp->req.post_set_power_req.new_level);

		if (reqp->req.post_set_power_req.new_level ==
			USB_DEV_OS_POWER_OFF) {

			/* now suspend the port */
			hubd_suspend_port(hubd, port);
		}
	} else {

		/* record old power in our local struct */
		hubd_set_child_pwrlvl(hubd, port,
		    reqp->req.post_set_power_req.old_level);

		if (reqp->req.post_set_power_req.old_level ==
		    USB_DEV_OS_POWER_OFF) {

			/*
			 * As this device failed to transition from
			 * power off state, suspend the port again
			 */
			hubd_suspend_port(hubd, port);
		}
	}

	/* move timestamp for PM */
	hubd_device_idle(hubd);
}


/*
 * bus ctl notifications are handled here, the rest goes up to root hub/hcd
 */
static int
usba_hubdi_bus_ctl(dev_info_t *dip,
	dev_info_t	*rdip,
	ddi_ctl_enum_t	op,
	void		*arg,
	void		*result)
{
	usb_device_t *hub_usb_device = usba_get_usb_device(rdip);
	dev_info_t *root_hub_dip = hub_usb_device->usb_root_hub_dip;
	power_req_t *reqp;
	struct attachspec *as;
	struct detachspec *ds;
	hubd_t		*hubd;
	usb_port_t	port;

	hubd = hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HUBDI, hubd->h_log_handle,
	    "usba_hubdi_bus_ctl:\n\t"
	    "dip = 0x%p, rdip = 0x%p, op = 0x%p, arg = 0x%p",
	    dip, rdip, op, arg);

	switch (op) {
	case DDI_CTLOPS_ATTACH:
		as = (struct attachspec *)arg;
		port = hubd_child_dip2port(hubd, rdip);

		switch (as->when) {
		case DDI_PRE:
			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
				"DDI_PRE DDI_CTLOPS_ATTACH");

			/*
			 * if we suspended the port previously
			 * because child went to low power state, and
			 * someone unloaded the driver, the port would
			 * still be suspended and needs to be resumed
			 */
			hubd_resume_port(hubd, port);

			break;
		case DDI_POST:

			hubd_post_attach(hubd, port, (struct attachspec *)arg);

			break;
		}
		break;
	case DDI_CTLOPS_DETACH:
		ds = (struct detachspec *)arg;

		switch (ds->when) {
		case DDI_PRE:
			/* nothing to do basically */
			USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
				"DDI_PRE DDI_CTLOPS_DETACH");
			break;
		case DDI_POST:
			hubd_post_detach(hubd,
				hubd_child_dip2port(hubd, rdip),
				(struct detachspec *)arg);
			break;
		}

		break;
	case DDI_CTLOPS_POWER:
		reqp = (power_req_t *)arg;
		port = hubd_child_dip2port(hubd, rdip);

		switch (reqp->request_type) {
		case PMR_PRE_SET_POWER:
			USB_DPRINTF_L4(DPRINT_MASK_HUBDI, hubd->h_log_handle,
				"PMR_PRE_SET_POWER");

			if ((reqp->req.pre_set_power_req.old_level == 0) &&
				(reqp->req.pre_set_power_req.new_level >
				reqp->req.pre_set_power_req.old_level)) {

				/*
				 * this child is transitioning from power off
				 * to power on state - resume port
				 */
				hubd_resume_port(hubd, port);

			}
			break;
		case PMR_POST_SET_POWER:

			/* record child's pwr and suspend port if required */
			hubd_post_power(hubd, port, reqp);

			break;
		}

		break;
	default:

		return (usba_bus_ctl(root_hub_dip, rdip, op, arg, result));
	}

	return (DDI_SUCCESS);
}


/*
 * functions to handle power transition for OS levels 0 -> 3
 */
static int
hubd_pwrlvl0(hubd_t *hubd)
{
	hub_power_t	*hubpm;
	int		rval;

	/* We can't power down if hotplug thread is running */
	if ((hubd->h_hotplug_thread) ||
	    (hubd_can_suspend(hubd) == USB_FAILURE)) {

		/* move timestamp for PM */
		mutex_exit(HUBD_MUTEX(hubd));
		hubd_device_idle(hubd);
		mutex_enter(HUBD_MUTEX(hubd));

		return (DDI_FAILURE);
	}

	switch (hubd->h_dev_state) {
	case USB_DEV_ONLINE:
		ASSERT(hubd->h_ep1_ph != NULL);

		hubpm = hubd->h_hubpm;

		/*
		 * if we are the root hub, do not stop polling
		 * otherwise, we will never see a resume
		 */
		if (usba_is_root_hub(hubd->h_dip)) {
			/* place holder to implement Global Suspend */
			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
			    "Global Suspend : Not Yet Implemented");
		} else {

			/* now stop polling on the intr pipe */
			mutex_exit(HUBD_MUTEX(hubd));

			if (hubd->h_ep1_ph) {
				rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}

			mutex_enter(HUBD_MUTEX(hubd));
		}

		/* Issue USB D3 command to the device here */
		rval = usb_set_device_pwrlvl3(hubd->h_dip);
		ASSERT(rval == USB_SUCCESS);

		hubd->h_dev_state = USB_DEV_POWERED_DOWN;
		hubpm->hubp_current_power = USB_DEV_OS_POWER_OFF;

		/* FALLTHRU */
	case USB_DEV_DISCONNECTED:
	case USB_DEV_CPR_SUSPEND:
	case USB_DEV_POWERED_DOWN:
	default:

		return (DDI_SUCCESS);
	}
}


/* ARGSUSED */
static int
hubd_pwrlvl1(hubd_t *hubd)
{
	int rval;

	/* Issue USB D2 command to the device here */
	rval = usb_set_device_pwrlvl2(hubd->h_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


/* ARGSUSED */
static int
hubd_pwrlvl2(hubd_t *hubd)
{
	int rval;

	/* Issue USB D1 command to the device here */
	rval = usb_set_device_pwrlvl1(hubd->h_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


static int
hubd_pwrlvl3(hubd_t *hubd)
{
	hub_power_t	*hubpm;
	int		rval;

	hubpm = hubd->h_hubpm;

	/*
	 * PM framework tries to put you in full power
	 * during system shutdown. If we are disconnected
	 * return success. Also, we should not change state
	 * when we are disconnected or suspended
	 */
	if ((hubd->h_dev_state == USB_DEV_DISCONNECTED) ||
		(hubd->h_dev_state == USB_DEV_CPR_SUSPEND)) {

		return (DDI_SUCCESS);
	}

	if (usba_is_root_hub(hubd->h_dip)) {

		if (hubpm->hubp_current_power == USB_DEV_OS_POWER_OFF) {
			/* implement global resume here */
			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
				"Global Resume : Not Yet Implemented");
		}

	} else if (hubd->h_ep1_ph != NULL) {
		/* Issue USB D0 command to the device here */
		rval = usb_set_device_pwrlvl0(hubd->h_dip);
		ASSERT(rval == USB_SUCCESS);

		/* now start polling on the intr pipe */
		mutex_exit(HUBD_MUTEX(hubd));

		rval = usb_pipe_start_polling(hubd->h_ep1_ph,
			USB_FLAGS_SHORT_XFER_OK);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));
	}

	hubd->h_dev_state = USB_DEV_ONLINE;
	hubpm->hubp_current_power = USB_DEV_OS_FULL_POWER;

	return (DDI_SUCCESS);
}


/* power entry point */
/* ARGSUSED */
int
usba_hubdi_power(dev_info_t *dip, int comp, int level)
{
	hubd_t		*hubd;
	hub_power_t	*hubpm;
	int		retval;

	hubd = hubd_get_soft_state(dip);
	USB_DPRINTF_L3(DPRINT_MASK_HUBDI, hubd->h_log_handle,
	    "usba_hubdi_power : Begin hubd (%p) level : %d", hubd, level);

	mutex_enter(HUBD_MUTEX(hubd));
	hubpm = hubd->h_hubpm;

	/* check if we are transitioning to a legal power level */
	if (USB_DEV_PWRSTATE_OK(hubpm->hubp_pwr_states, level)) {

		USB_DPRINTF_L2(DPRINT_MASK_HUBDI, hubd->h_log_handle,
		    "usba_hubdi_power : illegal power level : %d "
		    "hubp_pwr_states : %x", level, hubpm->hubp_pwr_states);

		mutex_exit(HUBD_MUTEX(hubd));

		return (DDI_FAILURE);
	}

	/*
	 * If we are about to raise power and we get this call to lower
	 * power, we return failure
	 */
	if ((hubpm->hubp_raise_power == B_TRUE) &&
		(level < (int)hubpm->hubp_current_power)) {

		mutex_exit(HUBD_MUTEX(hubd));

		return (DDI_FAILURE);
	}


	switch (level) {
	case USB_DEV_OS_POWER_OFF:
		retval = hubd_pwrlvl0(hubd);
		break;
	case USB_DEV_OS_POWER_1:
		retval = hubd_pwrlvl1(hubd);
		break;
	case USB_DEV_OS_POWER_2:
		retval = hubd_pwrlvl2(hubd);
		break;
	case USB_DEV_OS_FULL_POWER:
		retval = hubd_pwrlvl3(hubd);
		break;
	}

	mutex_exit(HUBD_MUTEX(hubd));

	return (retval);
}


/* power entry point for the root hub */
int
usba_hubdi_root_hub_power(dev_info_t *dip, int comp, int level)
{
	return (usba_hubdi_power(dip, comp, level));
}


/*
 * standard driver entry points support code
 */
int
usba_hubdi_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	instance = ddi_get_instance(dip);
	hubd_t	*hubd = NULL;
	int	rval;
	int	minor;
	char	*log_name = NULL;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubdi_log_handle,
		"hubd_attach instance %d, cmd = 0x%x", instance, cmd);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		hubd = hubd_get_soft_state(dip);
		rval = hubd_restore_device_state(dip, hubd);

		return (rval == USB_SUCCESS ? DDI_SUCCESS : DDI_FAILURE);
	case DDI_PM_RESUME:
	default:
		return (DDI_FAILURE);
	}

	/*
	 * Allocate softc information.
	 */
	if (usba_is_root_hub(dip)) {
		/* soft state has already been allocated */
		hubd = hubd_get_soft_state(dip);
		minor = HUBD_IS_ROOT_HUB;
		log_name = "usb";
	} else {
		rval = ddi_soft_state_zalloc(hubd_state, instance);
		minor = 0;

		if (rval != DDI_SUCCESS) {
			USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubdi_log_handle,
			    "cannot allocate soft state (%d)", instance);
			goto fail;
		}

		hubd = hubd_get_soft_state(dip);
		if (hubd == NULL) {
			goto fail;
		}
	}

	hubd->h_log_handle = usb_alloc_log_handle(dip, log_name, &hubd_errlevel,
				&hubd_errmask, &hubd_instance_debug,
				&hubd_show_label, 0);

	hubd->h_usb_device = usba_get_usb_device(dip);
	hubd->h_dip	= dip;
	hubd->h_instance = instance;
	hubd->h_total_hotplug_success = 0;
	hubd->h_total_hotplug_failure = 0;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd = 0x%p", (void *)hubd);

	mutex_init(HUBD_MUTEX(hubd), NULL, MUTEX_DRIVER, NULL);

	cv_init(&hubd->h_cv_reset_port, NULL, CV_DRIVER, NULL);

	cv_init(&hubd->h_cv_default_pipe, NULL, CV_DRIVER, NULL);

	hubd->h_init_state |= HUBD_LOCKS_DONE;

	/*
	 * alloc and init hubdi_ops
	 */
	hubd->h_hubdi_ops = usba_alloc_hubdi_ops();

	/*
	 * register this hub instance with usba
	 */
	rval = usba_hubdi_register(dip, hubd->h_hubdi_ops, 0);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "usba_hubdi_register failed");
		goto fail;
	}

	hubd->h_init_state |= HUBD_ATTACH_DONE;

	if (ddi_create_minor_node(dip, "devctl", S_IFCHR,
	    instance | minor, DDI_NT_NEXUS, 0) != DDI_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
			"cannot create devctl minor node (%d)", instance);
		goto fail;
	}

	hubd->h_init_state |= HUBD_MINOR_NODE_CREATED;


	/* now create components to power manage this device */
	hubd_create_pm_components(dip, hubd);

	mutex_enter(HUBD_MUTEX(hubd));

	hubd->h_dev_state = USB_DEV_ONLINE;

	/* initialize and create children */
	if (hubd_check_ports(hubd) != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_check_ports failed");
		mutex_exit(HUBD_MUTEX(hubd));

		goto fail;
	}

	mutex_exit(HUBD_MUTEX(hubd));

#ifdef	DEBUG
	mutex_enter(&hubd_dump_mutex);
	hubd->h_dump_ops = usba_alloc_dump_ops();
	hubd->h_dump_ops->usb_dump_ops_version = USBA_DUMP_OPS_VERSION_0;
	hubd->h_dump_ops->usb_dump_func = hubd_dump;
	hubd->h_dump_ops->usb_dump_cb_arg = (usb_opaque_t)hubd;
	hubd->h_dump_ops->usb_dump_order = USB_DUMPOPS_HUB_ORDER;
	usba_dump_register(hubd->h_dump_ops);
	mutex_exit(&hubd_dump_mutex);
#endif	/* DEBUG */

	/*
	 * Event handling: definition and registration
	 *
	 * first the  definition:
	 * get event handle
	 */
	(void) ndi_event_alloc_hdl(dip, 0, &hubd->h_ndi_event_hdl, NDI_SLEEP);

	/* bind event set to the handle */
	if (ndi_event_bind_set(hubd->h_ndi_event_hdl, &hubd_ndi_events,
	    NDI_SLEEP)) {
		USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "binding event set failed");

		goto fail;
	}

	/* event registration */
	hubd_register_events(hubd);

	hubd->h_init_state |= HUBD_EVENTS_REGISTERED;

	/*
	 * host controller driver has already reported this dev
	 * if we are the root hub
	 */
	if (!usba_is_root_hub(dip)) {
		ddi_report_dev(dip);
	}

	return (DDI_SUCCESS);

fail:
	USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "cannot attach (%d)", instance);

	if (hubd) {
		(void) hubd_cleanup(dip, hubd);
	}

	return (DDI_FAILURE);
}


int
usba_hubdi_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	hubd_t	*hubd = hubd_get_soft_state(dip);

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd_detach: cmd = 0x%x", cmd);

	switch (cmd) {
	case DDI_DETACH:
		return (hubd_cleanup(dip, hubd));

	case DDI_SUSPEND:
		return (hubd_cpr_suspend(hubd));

	case DDI_PM_SUSPEND:
		return (DDI_FAILURE);
	default:
		return (DDI_FAILURE);
	}
}


/*
 * hubd_cpr_suspend:
 *	flag suspend
 *	wait for hotplug thread to exit and close intr pipe
 */
static int
hubd_cpr_suspend(hubd_t *hubd)
{
	uint_t	old_state;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
		"hubd_cpr_suspend:");
	mutex_enter(HUBD_MUTEX(hubd));
	old_state = hubd->h_dev_state;
	hubd->h_dev_state = USB_DEV_CPR_SUSPEND;

	/*
	 * if the hotplug thread is running, the control pipe must be open.
	 * As the thread would have been frozen by cpr, there is no way
	 * to close the control pipe. Then fail suspend
	 */
	if (hubd->h_hotplug_thread) {
		USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
		"hubd_cpr_suspend: hotplug thread is active - fail suspend");

		hubd->h_dev_state = old_state;
		mutex_exit(HUBD_MUTEX(hubd));

		return (DDI_FAILURE);
	}

	/*
	 * if any async ops is in progress on intr pipe, the thread won't
	 * finish at this stage, so fail the suspend
	 */
	if (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {

		hubd->h_dev_state = old_state;
		mutex_exit(HUBD_MUTEX(hubd));

		return (DDI_FAILURE);
	}

	hubd_stop_polling(hubd);
	mutex_exit(HUBD_MUTEX(hubd));

	return (DDI_SUCCESS);
}


/*
 * hubd_rstport
 *	reset this port
 */
static int
hubd_rstport(hubd_t *hubd, usb_port_t port)
{
	int		rval, i;
	uint_t		completion_reason;
	uint16_t	status;
	uint16_t	change;
	long		time_delay;

	mutex_exit(HUBD_MUTEX(hubd));

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
		"hubd_rstport: Reset port#%d", port);
	rval = usb_pipe_sync_device_ctrl_send(hubd->h_default_pipe,
		SET_PORT_FEATURE,
		USB_REQ_SET_FEATURE,
		CFS_PORT_RESET,
		port,
		0,
		NULL,
		&completion_reason,
		USB_FLAGS_ENQUEUE);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
			"hubd_rstport: Unable to issue port reset");
		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	ASSERT(completion_reason == 0);

	/* time taken to reset a port on hub is 20ms max */
	time_delay = drv_usectohz(hubd_device_delay / 50);

	mutex_enter(HUBD_MUTEX(hubd));
	for (i = 0; i < HUBD_PORT_RETRY; i++) {
		delay(time_delay);

		/* get port status */
		(void) hubd_determine_port_status(hubd, port, &status,
			&change, HUBD_ACK_CHANGES);

		if (change & PORT_CHANGE_PRSC) {

			return (USB_SUCCESS);
		}
	}
	USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd_rstport: reset failed, status=%x, change=%x",
	    status, change);

	return (USB_FAILURE);
}


/*
 * hubd_setdevaddr
 *	set the device addrs on this port
 */
static int
hubd_setdevaddr(hubd_t *hubd, usb_port_t port)
{
	int		rval, rval1;
	uint_t		completion_reason;
	usb_pipe_handle_t	ph = NULL;
	dev_info_t	*child_dip = NULL;
	uchar_t		address = 0;
	usb_device_t	*usb_dev;
	int		retry = 0;
	long		time_delay;

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));

	child_dip = hubd->h_children_dips[port];
	address = hubd->h_usb_devices[port]->usb_addr;

	/*
	 * As this device has been reset, temporarily
	 * assign the default address
	 */
	usb_dev = hubd->h_usb_devices[port];
	mutex_enter(&usb_dev->usb_mutex);
	address = usb_dev->usb_addr;
	usb_dev->usb_addr = 0;
	mutex_exit(&usb_dev->usb_mutex);

	mutex_exit(HUBD_MUTEX(hubd));

	time_delay = drv_usectohz(hubd_device_delay / 20);
	for (retry = 0; retry < hubd_retry_enumerate; retry++) {
		/* open default pipe for communicating to the device */
		rval = usb_pipe_open(child_dip, NULL, NULL,
			USB_FLAGS_OPEN_EXCL | USB_FLAGS_SLEEP, &ph);
		if (rval != USB_SUCCESS) {
			USB_DPRINTF_L2(DPRINT_MASK_ATTA, hubd->h_log_handle,
			    "hubd_setdevaddr: Unable to open default pipe");
			break;
		}

		/* Set the address of the device */
		rval = usb_pipe_sync_device_ctrl_send(ph,
			USB_DEV_REQ_HOST_TO_DEV,
			USB_REQ_SET_ADDRESS,		/* bRequest */
			address,			/* wValue */
			0,				/* wIndex */
			0,				/* wLength */
			NULL,
			&completion_reason,
			0);

		USB_DPRINTF_L2(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_setdevaddr(%d): rval=%d cr=%d",
		    retry, rval, completion_reason);

		rval1 = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval1 == USB_SUCCESS);

		if (rval == USB_SUCCESS) {
			break;
		}

		delay(time_delay);
	}

	/* reset to the old addrs */
	mutex_enter(&usb_dev->usb_mutex);
	usb_dev->usb_addr = address;
	mutex_exit(&usb_dev->usb_mutex);

	mutex_enter(HUBD_MUTEX(hubd));

	return (rval);
}


/*
 * hubd_setdevconfig
 *	set the device addrs on this port
 */
static void
hubd_setdevconfig(hubd_t *hubd, usb_port_t port)
{
	int			rval;
	uint_t			completion_reason;
	usb_pipe_handle_t	ph = NULL;
	dev_info_t		*child_dip = NULL;
	usb_config_descr_t	*config_descriptor;
	uint16_t		config_number = 0;

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));

	child_dip = hubd->h_children_dips[port];
	config_descriptor =
		(usb_config_descr_t *)hubd->h_usb_devices[port]->usb_config;
	config_number = config_descriptor->bConfigurationValue;
	mutex_exit(HUBD_MUTEX(hubd));

	/* open default pipe for communicating to the device */
	rval = usb_pipe_open(child_dip, NULL, NULL, USB_FLAGS_OPEN_EXCL, &ph);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_setdevconfig: Unable to open default pipe");
		mutex_enter(HUBD_MUTEX(hubd));

		return;
	}

	/* Set the default configuration of the device */
	rval = usb_pipe_sync_device_ctrl_send(ph,
		USB_DEV_REQ_HOST_TO_DEV,
		USB_REQ_SET_CONFIGURATION,	/* bRequest */
		config_number,			/* wValue */
		0,				/* wIndex */
		0,				/* wLength */
		NULL,
		&completion_reason,
		0);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_ATTA, hubd->h_log_handle,
			"hubd_setdevconfig: set device config failed");
	}
	rval = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);

	mutex_enter(HUBD_MUTEX(hubd));
}


/*
 * hubd_check_same_device()
 *	check if the same device and if not, warn the user
 */
static int
hubd_check_same_device(dev_info_t *dip, hubd_t *hubd)
{
	char		*ptr;

	if (usb_check_same_device(dip) == USB_FAILURE) {
		if (ptr = usb_get_usbdev_strdescr(dip)) {
			USB_DPRINTF_L0(DPRINT_MASK_HOTPLUG,
			    hubd->h_log_handle,
			    "Cannot access device. "
			    "Please reconnect %s ", ptr);
		} else {
			USB_DPRINTF_L0(DPRINT_MASK_HOTPLUG,
			    hubd->h_log_handle,
			    "Devices not identical to the "
			    "previous one on this port.\n"
			    "Please disconnect and reconnect");
		}

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/*
 * hubd_restore_device_state:
 *	- set config for the hub
 *	- power cycle all the ports
 *	- for each port that was connected
 *		- reset port
 *		- assign addrs to the device on this port
 *	- restart polling
 *	- reset suspend flag
 */
static int
hubd_restore_device_state(dev_info_t *dip, hubd_t *hubd)
{
	int		rval;
	int		retry;
	uint_t		hub_prev_state;
	usb_port_t	port;
	uint16_t	status;
	uint16_t	change;
	hub_power_t	*hubpm;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd_restore_device_state:");

	mutex_enter(HUBD_MUTEX(hubd));
	hub_prev_state = hubd->h_dev_state;
	ASSERT((hub_prev_state == USB_DEV_DISCONNECTED) ||
		(hub_prev_state == USB_DEV_CPR_SUSPEND));

	hubpm = hubd->h_hubpm;
	mutex_exit(HUBD_MUTEX(hubd));

	/* First bring the device to full power */
	hubd_raise_device_power(hubd, 0, USB_DEV_OS_FULL_POWER);

	if (!usba_is_root_hub(dip) &&
	    (hubd_check_same_device(dip, hubd) == USB_FAILURE)) {

		/* change the device state from suspended to disconnected */
		mutex_enter(HUBD_MUTEX(hubd));
		hubd->h_dev_state = USB_DEV_DISCONNECTED;
		mutex_exit(HUBD_MUTEX(hubd));

		return (USB_SUCCESS);
	}

	mutex_enter(HUBD_MUTEX(hubd));

	/* Open the default pipe */
	rval = hubd_open_default_pipe(hubd, 0);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_restore_device_state: open default pipe failed");

		(void) hubd_close_default_pipe(hubd);
		mutex_exit(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	/* First turn off all port power */
	rval = hubd_disable_all_port_power(hubd);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_restore_device_state:"
		    "turning off port power failed");
	}

	/* Settling time before turning on again */
	delay(drv_usectohz(hubd_device_delay / 1000));

	/* enable power on all ports so we can see connects */
	rval = hubd_enable_all_port_power(hubd);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
			"hubd_restore_device_state: turn on port power failed");

		/* disable whatever was enabled */
		(void) hubd_disable_all_port_power(hubd);
		(void) hubd_close_default_pipe(hubd);
		mutex_exit(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	/* wait 3 frames before accessing devices */
	delay(drv_usectohz(3000));

	if (hub_prev_state == USB_DEV_DISCONNECTED) {

		/* start polling the hub intr pipe */
		rval = hubd_start_polling(hubd);
		ASSERT(rval == USB_SUCCESS);

		/* Now change the device state */
		hubd->h_dev_state = USB_DEV_HUB_DISCONNECT_RECOVER;

		/*
		 * we need to stop polling on the intr pipe to that
		 * port resets can proceed normally
		 */
		mutex_exit(HUBD_MUTEX(hubd));
		rval = usb_pipe_stop_polling(hubd->h_ep1_ph, USB_FLAGS_SLEEP,
			NULL, NULL);
		ASSERT(rval == USB_SUCCESS);
		mutex_enter(HUBD_MUTEX(hubd));

	}

	for (port = 1; port <= hubd->h_hub_descr.bNbrPorts; port++) {

		/* did this port have a device connected? */
		if ((hubd->h_children_dips[port]) &&
		    (usba_child_exists(hubd->h_dip,
		    hubd->h_children_dips[port]) == USB_SUCCESS)) {
			/* get port status */
			(void) hubd_determine_port_status(hubd, port,
				&status, &change, HUBD_ACK_CHANGES);

			/* check if it is truly connected */
			if (status & PORT_STATUS_CCS) {

				/*
				 * Now reset port and assign the device
				 * its original address
				 */
				retry = 0;
				do {
					if (hub_prev_state ==
						USB_DEV_CPR_SUSPEND) {
						rval = hubd_rstport(hubd, port);
					} else {
						rval = hubd_reset_port(hubd,
							port);
					}
					ASSERT(rval == USB_SUCCESS);

					/* required for ppx */
					(void) hubd_enable_port(hubd,
						port);

					if (retry) {
						delay(drv_usectohz(
							hubd_device_delay/2));
					}

					rval = hubd_setdevaddr(hubd,
						port);
					retry++;
				} while ((rval != USB_SUCCESS) &&
					(retry < hubd_retry_enumerate));

				hubd_setdevconfig(hubd, port);

				if (hub_prev_state == USB_DEV_DISCONNECTED) {
					/*
					 * close the default pipe
					 * for incoming bus ctls
					 */
					(void) hubd_close_default_pipe(hubd);
					mutex_exit(HUBD_MUTEX(hubd));

					hubd_post_connect_event(hubd, port);

					mutex_enter(HUBD_MUTEX(hubd));

					/* reopen the pipe */
					rval = hubd_open_default_pipe(hubd,
					USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
				}
			}
		} else {
			rval = hubd_delete_child(hubd, port,
				NDI_DEVI_REMOVE|NDI_DEVI_FORCE);
			ASSERT(rval == USB_SUCCESS);
		}
	}

	(void) hubd_close_default_pipe(hubd);

	/* if the device had remote wakeup earlier, enable it again */
	if (hubpm->hubp_wakeup_enabled) {

		mutex_exit(HUBD_MUTEX(hubd));

		rval = usb_enable_remote_wakeup(hubd->h_dip);

		mutex_enter(HUBD_MUTEX(hubd));
	}

	if (hub_prev_state == USB_DEV_CPR_SUSPEND) {
		if (hubd_start_polling(hubd) != USB_SUCCESS) {
			USB_DPRINTF_L3(DPRINT_MASK_ATTA, hubd->h_log_handle,
			    "hubd_restore_device_state: start polling failed");
			mutex_exit(HUBD_MUTEX(hubd));

			return (USB_FAILURE);
		}
	} else {
		/*
		 * now restart polling on the intr pipe so that we can enumerate
		 * devices on the remaining ports
		 */
		mutex_exit(HUBD_MUTEX(hubd));
		rval = usb_pipe_start_polling(hubd->h_ep1_ph,
			USB_FLAGS_SHORT_XFER_OK);
		ASSERT(rval == USB_SUCCESS);
		mutex_enter(HUBD_MUTEX(hubd));
	}

	hubd->h_dev_state = USB_DEV_ONLINE;
	mutex_exit(HUBD_MUTEX(hubd));

	return (USB_SUCCESS);
}


/*
 * hubd_cleanup:
 *	cleanup hubd and deallocate. this function is called for
 *	handling attach failures and detaching including dynamic
 *	reconfiguration
 */
/*ARGSUSED*/
static int
hubd_cleanup(dev_info_t *dip, hubd_t	*hubd)
{
	int		rval;
	hub_power_t	*hubpm;

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd_cleanup:");

	if (hubd == NULL) {

		return (DDI_SUCCESS);
	}

	/*
	 * if the locks have been initialized, grab the lock because
	 * all lower level functions assert that the lock is owned
	 */
	if (hubd->h_init_state & HUBD_LOCKS_DONE) {
		usb_port_t port;
		uint16_t nports;

		mutex_enter(HUBD_MUTEX(hubd));

		USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_cleanup: stop polling");

		if (hubd->h_ep1_ph) {
			mutex_exit(HUBD_MUTEX(hubd));
			rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
						USB_FLAGS_SLEEP, NULL, NULL);
			if (rval != USB_SUCCESS) {
				USB_DPRINTF_L3(DPRINT_MASK_ATTA,
				    hubd->h_log_handle,
				    "stop polling failed (%d)", rval);

				rval = usb_pipe_reset(hubd->h_ep1_ph,
						USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);

				rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
						USB_FLAGS_SLEEP, NULL, NULL);
				if (rval != USB_SUCCESS) {
					return (DDI_FAILURE);
				}
			}
			mutex_enter(HUBD_MUTEX(hubd));
		}

		/* clean up children, if any, and stop polling */
		if (hubd_delete_all_children(hubd) != USB_SUCCESS) {
			mutex_exit(HUBD_MUTEX(hubd));

			return (DDI_FAILURE);
		}

		/* default pipe should be closed but just in case */
		if (hubd->h_default_pipe) {
			(void) hubd_close_default_pipe(hubd);
		}

		if (hubd_open_default_pipe(hubd, USB_FLAGS_SLEEP) !=
		    USB_SUCCESS) {
			mutex_exit(HUBD_MUTEX(hubd));

			return (DDI_FAILURE);
		}

		nports = hubd->h_hub_descr.bNbrPorts;

		USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "hubd_cleanup: disabling ports");

		/* disable all ports */
		for (port = 1; port <= nports; port++) {
			if (hubd_disable_port(hubd, port) !=
			    USB_SUCCESS) {
				break;
			}
		}

		/*
		 * if all ports were successfully disabled
		 * disable port power
		 */
		if (port > nports) {
			USB_DPRINTF_L4(DPRINT_MASK_ATTA,
			    hubd->h_log_handle,
			    "hubd_cleanup: remove port power");

			(void) hubd_disable_all_port_power(hubd);
		}

		/* deallocate children dips array */
		if (hubd->h_children_dips) {

			kmem_free(hubd->h_children_dips,
				hubd->h_cd_list_length);
			kmem_free(hubd->h_usb_devices,
				hubd->h_cd_list_length);
		}

		if (hubd->h_default_pipe) {
			(void) hubd_close_default_pipe(hubd);
		}

		mutex_exit(HUBD_MUTEX(hubd));
	}


	ddi_prop_remove_all(hubd->h_dip);

	mutex_enter(HUBD_MUTEX(hubd));
	hubpm = hubd->h_hubpm;
	if (hubpm) {
		if (hubpm->hubp_child_pwrstate) {
			kmem_free(hubpm->hubp_child_pwrstate,
			    MAX_PORTS + 1);
		}
		kmem_free(hubpm, sizeof (hub_power_t));
	}
	mutex_exit(HUBD_MUTEX(hubd));

	/*
	 * deallocate events, if events are still registered
	 * (ie. children still attached) then we have to fail the detach
	 */
	if (hubd->h_ndi_event_hdl &&
	    (ndi_event_free_hdl(hubd->h_ndi_event_hdl) != NDI_SUCCESS)) {

		return (DDI_FAILURE);
	}

	/* event deregistration */
	if (hubd->h_init_state &  HUBD_EVENTS_REGISTERED) {
		hubd_deregister_events(hubd);
	}

#ifdef	DEBUG
	mutex_enter(&hubd_dump_mutex);
	if (hubd->h_dump_ops) {
		usba_dump_deregister(hubd->h_dump_ops);
		usba_free_dump_ops(hubd->h_dump_ops);
	}
	mutex_exit(&hubd_dump_mutex);
#endif	/* DEBUG */

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd_cleanup: freeing space");

	if (hubd->h_hubdi_ops) {
		usba_free_hubdi_ops(hubd->h_hubdi_ops);
	}

	if (hubd->h_init_state & HUBD_ATTACH_DONE) {
		rval = usba_hubdi_deregister(dip);
		ASSERT(rval == USB_SUCCESS);
	}

	if (hubd->h_init_state & HUBD_LOCKS_DONE) {
		mutex_destroy(HUBD_MUTEX(hubd));
		cv_destroy(&hubd->h_cv_reset_port);
		cv_destroy(&hubd->h_cv_default_pipe);
	}

	if (hubd->h_init_state & HUBD_MINOR_NODE_CREATED) {
		ddi_remove_minor_node(dip, NULL);
	}

	usb_free_log_handle(hubd->h_log_handle);

	if (!usba_is_root_hub(dip)) {
		ddi_soft_state_free(hubd_state, ddi_get_instance(dip));
	}

	ddi_prop_remove_all(dip);

	return (DDI_SUCCESS);
}


/*
 * wrappers for opening and closing the default pipe. For simplicity
 * we serialize access to the default pipe
 */
static int
hubd_open_default_pipe(hubd_t *hubd, uint_t flags)
{
	int rval;

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "hubd_open_default_pipe:");

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));

	if (hubd->h_default_pipe && ((flags & USB_FLAGS_SLEEP) == 0)) {

		return (USB_FAILURE);
	}

	while (hubd->h_default_pipe) {
		cv_wait(&hubd->h_cv_default_pipe, HUBD_MUTEX(hubd));
	}

	/* Open the default pipe on the hub */
	mutex_exit(HUBD_MUTEX(hubd));
	if ((rval = usb_pipe_open(hubd->h_dip, NULL, NULL,
	    flags | USB_FLAGS_OPEN_EXCL,
	    &hubd->h_default_pipe)) != USB_SUCCESS) {

		mutex_enter(HUBD_MUTEX(hubd));

		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "default pipe open failed (%d)", rval);

		hubd->h_default_pipe = NULL;
		cv_signal(&hubd->h_cv_default_pipe);
	} else {
		mutex_enter(HUBD_MUTEX(hubd));
	}

	return (rval);
}


static int
hubd_close_default_pipe(hubd_t *hubd)
{
	int rval;

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "hubd_close_default_pipe:");

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_close(&hubd->h_default_pipe,
	    USB_FLAGS_SLEEP, NULL, NULL)) != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "closing default pipe failed (%d)", rval);
		ASSERT(rval == USB_SUCCESS);
	}
	mutex_enter(HUBD_MUTEX(hubd));

	/* even if it fails, zero h_default_pipe anyway and release */
	hubd->h_default_pipe = NULL;

	cv_signal(&hubd->h_cv_default_pipe);

	return (rval);
}


/*
 * hubd_check_ports:
 *	- get hub descriptor
 *	- check initial port status
 *	- enable power on all ports
 *	- enable polling on ep1
 */
static int
hubd_check_ports(hubd_t  *hubd)
{
	int		rval;

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_check_ports: hubd = 0x%p instance = 0x%x addr = 0x%x",
	    hubd, hubd->h_instance, usb_get_addr(hubd->h_dip));

	if ((rval = hubd_open_default_pipe(hubd, USB_FLAGS_SLEEP)) !=
	    USB_SUCCESS) {

		return (rval);
	}

	if ((rval = hubd_get_hub_descriptor(hubd)) != USB_SUCCESS) {

		(void) hubd_close_default_pipe(hubd);

		return (rval);
	}

	hubd->h_port_connected = 0;

	/*
	 * First turn off all port power
	 */
	if ((rval = hubd_disable_all_port_power(hubd)) != USB_SUCCESS) {

		/* disable whatever was enabled */
		(void) hubd_disable_all_port_power(hubd);

		(void) hubd_close_default_pipe(hubd);

		return (rval);
	}

	/*
	 * do not switch on immediately (instantly on root hub)
	 * and allow time to settle
	 */
	delay(drv_usectohz(1000));

	/*
	 * enable power on all ports so we can see connects
	 */
	if ((rval = hubd_enable_all_port_power(hubd)) != USB_SUCCESS) {

		/* disable whatever was enabled */
		(void) hubd_disable_all_port_power(hubd);

		(void) hubd_close_default_pipe(hubd);

		return (rval);
	}

	/* wait 3 frames before accessing devices */
	delay(drv_usectohz(3000));

	if ((rval = hubd_close_default_pipe(hubd)) != USB_SUCCESS) {

		return (rval);
	}

	/*
	 * allocate arrays for saving the dips of each child per port
	 *
	 * ports go from 1 - n, allocate 1 more entry
	 */
	hubd->h_cd_list_length =
		(sizeof (dev_info_t **)) * (hubd->h_hub_descr.bNbrPorts + 1);

	hubd->h_children_dips = (dev_info_t **)kmem_zalloc(
			hubd->h_cd_list_length, KM_SLEEP);
	hubd->h_usb_devices = (usb_device_t **)kmem_zalloc(
			hubd->h_cd_list_length, KM_SLEEP);

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "hubd_check_ports done");

	/*
	 * start polling ep1 for status changes
	 */
	if ((rval = hubd_start_polling(hubd)) != USB_SUCCESS) {

		return (rval);
	}

	return (rval);
}


/*
 * hubd_get_hub_descriptor:
 */
static int
hubd_get_hub_descriptor(hubd_t *hubd)
{
	usb_hub_descr_t	*hub_descr = &hubd->h_hub_descr;
	mblk_t		*data = NULL;
	uint_t		completion_reason;
	uint16_t	length;
	int		rval;

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "hubd_get_hub_descriptor:");

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	/* get hub descriptor length first by requesting 8 bytes only */
	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_sync_device_ctrl_receive(
			hubd->h_default_pipe,
			GET_HUB_DESCRIPTOR,
			USB_REQ_GET_DESCRIPTOR,	/* bRequest */
			USB_DESCR_TYPE_SETUP_HUB, /* wValue */
			0,			/* wIndex */
			8,			/* wLength */
			&data,
			&completion_reason,
			USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "get hub descriptor failed (%d)", rval);

		if (data) {
			freemsg(data);
			data = NULL;
		}

		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (rval);
	}

	ASSERT(completion_reason == 0);

	length = *(data->b_rptr);

	if (length > 8) {
		freemsg(data);

		data = NULL;

		/* get complete hub descriptor */
		if ((rval = usb_pipe_sync_device_ctrl_receive(
				hubd->h_default_pipe,
				GET_HUB_DESCRIPTOR,
				USB_REQ_GET_DESCRIPTOR,	/* bRequest */
				USB_DESCR_TYPE_SETUP_HUB, /* wValue */
				0,			/* wIndex */
				length,			/* wLength */
				&data,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
			    "get hub descriptor failed (%d)", rval);

			if (data) {
				freemsg(data);
				data = NULL;
			}

			rval = usb_pipe_reset(hubd->h_default_pipe,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);

			mutex_enter(HUBD_MUTEX(hubd));

			return (rval);
		}
	}

	ASSERT(completion_reason == 0);
	mutex_enter(HUBD_MUTEX(hubd));

	/* parse the hub descriptor */
	/* only 32 ports are supported at present */
	ASSERT(*(data->b_rptr + 2) <= 32);
	if (usb_parse_CV_descr("cccscccccc",
			data->b_rptr,
			data->b_wptr - data->b_rptr,
			(void *)hub_descr,
			sizeof (usb_hub_descr_t)) == 0) {

		USB_DPRINTF_L1(DPRINT_MASK_ATTA, hubd->h_log_handle,
		    "parsing hub descriptor failed");

		freemsg(data);

		return (USB_FAILURE);
	}

	freemsg(data);

	USB_DPRINTF_L4(DPRINT_MASK_ATTA, hubd->h_log_handle,
	    "rval = 0x%x bNbrPorts = 0x%x wHubChars = 0x%x "
	    "PwrOn2PwrGood = 0x%x", rval,
	    hub_descr->bNbrPorts, hub_descr->wHubCharacteristics,
	    hub_descr->bPwrOn2PwrGood);

	ASSERT(hub_descr->bNbrPorts <= MAX_PORTS);

	return (USB_SUCCESS);
}


/*
 * hubd_start_polling:
 *	polling interrupt ep1 for status changes
 */
static int
hubd_start_polling(hubd_t	*hubd)
{
	int			rval;
	usb_interface_descr_t	interface_descriptor;
	usb_endpoint_descr_t	*endpoint_descriptor = &hubd->h_ep1_descr;
	usb_device_t		*ud = hubd->h_usb_device;
	size_t			size;

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "hubd_start_polling: config length = 0x%x",
	    ud->usb_config_length);

	hubd->h_intr_pipe_state = HUBD_INTR_PIPE_OPEN;

	/* Parse the interface descriptor */
	size = usb_parse_interface_descr(ud->usb_config,
			ud->usb_config_length,
			0,		/* interface index */
			0,		/* alt interface index */
			&interface_descriptor,
			USB_IF_DESCR_SIZE);

	if (size != USB_IF_DESCR_SIZE) {

		USB_DPRINTF_L1(DPRINT_MASK_HUB, hubd->h_log_handle,
		    "size != USB_IF_DESCR_SIZE, 0x%x != 0x%x",
		    size, USB_IF_DESCR_SIZE);

		return (USB_FAILURE);
	}

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "interface descriptor:\n\t"
	    "l = 0x%x t = 0x%x alt = 0x%x n_eps = 0x%x c = 0x%x sc = 0x%x\n\t"
	    "ip = 0x%x i = 0x%x",
	    interface_descriptor.bLength,
	    interface_descriptor.bDescriptorType,
	    interface_descriptor.bInterfaceNumber,
	    interface_descriptor.bAlternateSetting,
	    interface_descriptor.bNumEndpoints,
	    interface_descriptor.bInterfaceClass,
	    interface_descriptor.bInterfaceSubClass,
	    interface_descriptor.bInterfaceProtocol,
	    interface_descriptor.iInterface);

	ASSERT(interface_descriptor.bNumEndpoints >= 1);

	/* Parse the endpoint descriptor */
	size = usb_parse_endpoint_descr(ud->usb_config,
			ud->usb_config_length,
			0,		/* interface index */
			0,		/* alt interface index */
			0,		/* ep index */
			endpoint_descriptor,
			USB_EPT_DESCR_SIZE);

	if (size != USB_EPT_DESCR_SIZE) {

		USB_DPRINTF_L1(DPRINT_MASK_HUB, hubd->h_log_handle,
		    "size != USB_EPT_DESCR_SIZE, 0x%x != 0x%x",
		    size, USB_EPT_DESCR_SIZE);

		return (USB_FAILURE);
	}

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "endpoint descriptor:\n\t"
	    "l = 0x%x t = 0x%x add = 0x%x attr = 0x%x mps = %x int = 0x%x",
	    endpoint_descriptor->bLength,
	    endpoint_descriptor->bDescriptorType,
	    endpoint_descriptor->bEndpointAddress,
	    endpoint_descriptor->bmAttributes,
	    endpoint_descriptor->wMaxPacketSize,
	    endpoint_descriptor->bInterval);

	if ((endpoint_descriptor->bmAttributes & USB_EPT_ATTR_MASK)
				!= USB_EPT_ATTR_INTR) {
		USB_DPRINTF_L1(DPRINT_MASK_HUB, hubd->h_log_handle,
		    "ep1 is not an interrupt pipe");

		return (USB_FAILURE);
	}

	/*
	 * initialize pipe policy
	 */
	hubd->h_pipe_policy.pp_version = USB_PIPE_POLICY_V_0;
	hubd->h_pipe_policy.pp_periodic_max_transfer_size =
			endpoint_descriptor->wMaxPacketSize;
	hubd->h_pipe_policy.pp_callback_arg = (void *)hubd;
	hubd->h_pipe_policy.pp_callback = hubd_read_callback;
	hubd->h_pipe_policy.pp_exception_callback = hubd_exception_callback;

	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_open(hubd->h_dip,
			&hubd->h_ep1_descr,
			&hubd->h_pipe_policy,
			USB_FLAGS_OPEN_EXCL,
			&hubd->h_ep1_ph)) != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_HUB, hubd->h_log_handle,
		    "open pipe failed");

		mutex_enter(HUBD_MUTEX(hubd));

		return (rval);
	}

	if (usb_pipe_start_polling(hubd->h_ep1_ph,
		USB_FLAGS_SHORT_XFER_OK) != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HUB, hubd->h_log_handle,
		"start polling failed");

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	mutex_enter(HUBD_MUTEX(hubd));

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "start polling succeeded");

	return (USB_SUCCESS);
}


/*
 * hubd_stop_polling:
 *	close the pipe and wait for the hotplug thread
 *	to exit
 */
static void
hubd_stop_polling(hubd_t *hubd)
{
	int	rval;

	USB_DPRINTF_L4(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "hubd_stop_polling:");

	if (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "drain async intr pipe reset");

		cv_wait(&hubd->h_cv_reset_intrpipe, &hubd->h_mutex);
	}
	ASSERT(hubd->h_intr_pipe_state != HUBD_INTR_PIPE_RESETTING);

	/*
	 * Now that no async operation is outstanding on pipe,
	 * we can change the state to HUBD_INTR_PIPE_CLOSING
	 */
	hubd->h_intr_pipe_state = HUBD_INTR_PIPE_CLOSING;

	/*
	 * wait for the hotplug thread to exit. after
	 * closing the pipe, all callbacks should have
	 * been completed
	 */
	while (hubd->h_hotplug_thread) {
		mutex_exit(HUBD_MUTEX(hubd));
		delay(drv_usectohz(1000));
		mutex_enter(HUBD_MUTEX(hubd));
	}

	if (hubd->h_ep1_ph) {
		/*
		 * We need to let an oustanding reset complete prior
		 * to closing the pipe, else could lead to kernel panic
		 * with BAD TRAP
		 */
		if (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "drain async intr pipe reset");

			cv_wait(&hubd->h_cv_reset_intrpipe, &hubd->h_mutex);
		}
		ASSERT(hubd->h_intr_pipe_state != HUBD_INTR_PIPE_RESETTING);
		mutex_exit(HUBD_MUTEX(hubd));
		rval = usb_pipe_close(&hubd->h_ep1_ph,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);
		mutex_enter(HUBD_MUTEX(hubd));
		hubd->h_ep1_ph = NULL;
	}

	hubd->h_intr_pipe_state &= ~HUBD_INTR_PIPE_CLOSING;
}

/*
 * hubd_intr_pipe_reset_callback
 *	we get intr pipe reset completion notification here
 *	start polling on the intr pipe only if exception was called
 *	for any condition other than device not responding and we
 *	are not in the middle of port reset
 */
/*ARGSUSED*/
static void
hubd_intr_pipe_reset_callback(usb_opaque_t arg1, int rval, uint_t dummy_arg3)
{
	hubd_t	*hubd = (hubd_t *)arg1;

	mutex_enter(HUBD_MUTEX(hubd));
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_HUB, hubd->h_log_handle,
		    "reset intr pipe failed");

	} else if ((hubd->h_intr_pipe_state != HUBD_INTR_PIPE_CLOSING) &&
	    (hubd->h_intr_completion_reason != USB_CC_DEV_NOT_RESP) &&
	    (hubd->h_port_reset_wait == 0)) {

		mutex_exit(HUBD_MUTEX(hubd));
		if (usb_pipe_start_polling(hubd->h_ep1_ph,
			USB_FLAGS_SHORT_XFER_OK) != USB_SUCCESS) {
			USB_DPRINTF_L2(DPRINT_MASK_HUB, hubd->h_log_handle,
			    "restart polling failed");
		}
		mutex_enter(HUBD_MUTEX(hubd));
	}
	if (hubd->h_intr_pipe_state != HUBD_INTR_PIPE_CLOSING) {
		hubd->h_intr_pipe_state = HUBD_INTR_PIPE_OPEN;
	}

	cv_signal(&hubd->h_cv_reset_intrpipe);
	mutex_exit(HUBD_MUTEX(hubd));
}


/*
 * hubd_exception_callback
 *	interrupt ep1 exception callback function
 *	- reset pipe
 *	- restart polling
 */
static int
hubd_exception_callback(usb_pipe_handle_t pipe,
	usb_opaque_t	callback_arg,
	uint_t		completion_reason,
	mblk_t		*data,
	uint_t		flag)
{
	hubd_t		*hubd = (hubd_t *)callback_arg;
	int		rval;

	USB_DPRINTF_L2(DPRINT_MASK_HUB, hubd->h_log_handle,
	    "hubd_exception_callback: cr = 0x%x, data = 0x%x, flag = 0x%x",
	    completion_reason, data, flag);

	if (data) {
		freemsg(data);
	}

	/*
	 * if the device has been disconnected/suspended, do not
	 * start any async request
	 */
	mutex_enter(HUBD_MUTEX(hubd));
	if ((hubd->h_intr_pipe_state == HUBD_INTR_PIPE_OPEN) &&
		((hubd->h_dev_state == USB_DEV_ONLINE) ||
		(hubd->h_dev_state == USB_DEV_POWERED_DOWN))) {

		hubd->h_intr_pipe_state = HUBD_INTR_PIPE_RESETTING;

		mutex_exit(HUBD_MUTEX(hubd));

		USB_DPRINTF_L2(DPRINT_MASK_HUB, hubd->h_log_handle,
		    "hubd_exception_callback: issuing pipe reset");

		rval = usb_pipe_reset(pipe, USB_FLAGS_OPEN_EXCL,
			hubd_intr_pipe_reset_callback, (usb_opaque_t)hubd);
		ASSERT(rval == USB_SUCCESS);
	} else {
		mutex_exit(HUBD_MUTEX(hubd));
	}

	return (USB_SUCCESS);
}


/*
 * hubd_read_callback:
 *	interrupt ep1 callback function
 *
 *	the status indicates just a change on the pipe with no indication
 *	of what the change was
 *
 *	known conditions:
 *		- reset port completion
 *		- connect
 *		- disconnect
 *
 *	for handling the hotplugging, create a new thread that can do
 *	synchronous usba calls
 */
static int
hubd_read_callback(usb_pipe_handle_t pipe,
		usb_opaque_t callback_arg,
		mblk_t *data)
{
	hubd_t		*hubd = (hubd_t *)callback_arg;
	size_t		length;

	/*
	 * At present, we are not handling notification for completion of
	 * asynchronous pipe reset, for which this data ptr could be NULL
	 */
	if (data == NULL) {
		return (USB_SUCCESS);
	}

	mutex_enter(HUBD_MUTEX(hubd));

	if ((hubd->h_dev_state == USB_DEV_CPR_SUSPEND) ||
		(hubd->h_intr_pipe_state == HUBD_INTR_PIPE_CLOSING)) {
		mutex_exit(HUBD_MUTEX(hubd));
		freemsg(data);

		return (USB_SUCCESS);
	}

	ASSERT(hubd->h_ep1_ph == pipe);


	length = data->b_wptr - data->b_rptr;

	/*
	 * Only look at the data and startup the hotplug thread if
	 * there actually is data.
	 */
	if (length != 0) {
		/*
		 * if a port change was already reported and we are waiting for
		 * reset port completion then wake up the hotplug thread which
		 * should be waiting on reset port completion
		 *
		 * if there is disconnect event instead of reset completion, let
		 * the hotplug thread figure this out
		 */

		/* remove the reset wait bits from the status */
		hubd->h_port_change |=
			*data->b_rptr & ~hubd->h_port_reset_wait;

		USB_DPRINTF_L4(DPRINT_MASK_CALLBACK, hubd->h_log_handle,
		    "port change = 0x%x port_reset_wait = 0x%x",
		    hubd->h_port_change, hubd->h_port_reset_wait);


		/* there should be only one reset bit active at the time */
		if (hubd->h_port_reset_wait & *data->b_rptr) {
			hubd->h_port_reset_wait = 0;
			cv_signal(&hubd->h_cv_reset_port);
		}

		freemsg(data);

		/* kick off the thread only if device is ONLINE */
		if ((hubd->h_dev_state == USB_DEV_ONLINE) &&
			(hubd->h_port_change) &&
			(hubd->h_hotplug_thread == NULL)) {
			extern taskq_t	*usba_taskq;

			USB_DPRINTF_L4(DPRINT_MASK_CALLBACK, hubd->h_log_handle,
			    "creating hotplug thread"
			    "dev_state : %d", hubd->h_dev_state);

			hubd->h_hotplug_thread = taskq_dispatch(
				usba_taskq, hubd_hotplug_thread,
				(void *)hubd, KM_NOSLEEP);
		}
	}

	mutex_exit(HUBD_MUTEX(hubd));

	return (USB_SUCCESS);
}


/*
 * hubd_hotplug_thread:
 *	handles resetting of port, and creating children
 *
 *	the ports to check are indicated in h_port_change bit mask
 */
static void
hubd_hotplug_thread(void *arg)
{
	hubd_t		*hubd = (hubd_t *)arg;
	usb_port_t	port;
	uint16_t	nports;
	int		rval;
	uint_t		old_connect_status;
	uint16_t	*status = NULL;
	uint16_t	*change = NULL;
	usb_pipe_state_t	pipe_state;
	int		alloc_size;
	hub_power_t	*hubpm;
	int		connects = 0;

	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_hotplug_thread: started");

	mutex_enter(HUBD_MUTEX(hubd));

	if (usba_is_root_hub(hubd->h_dip)) {

		if (hubd->h_dev_state == USB_DEV_POWERED_DOWN) {

			hubpm = hubd->h_hubpm;

			/* mark the root hub as full power */
			hubpm->hubp_current_power = USB_DEV_OS_FULL_POWER;
			mutex_exit(HUBD_MUTEX(hubd));

			USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "hubd_hotplug_thread:  call pm_power_has_changed");

			rval = pm_power_has_changed(hubd->h_dip, 0,
				USB_DEV_OS_FULL_POWER);
			ASSERT(rval == DDI_SUCCESS);

			mutex_enter(HUBD_MUTEX(hubd));
			hubd->h_dev_state = USB_DEV_ONLINE;
		}

	} else {

		USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "hubd_hotplug_thread: not root hub");
	}

	if (hubd->h_intr_pipe_state != HUBD_INTR_PIPE_OPEN) {

		goto exit;
	}

	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_hotplug_thread: default pipe open");

	nports = hubd->h_hub_descr.bNbrPorts;

	/* interrupt pipe notification	retired */
	mutex_exit(HUBD_MUTEX(hubd));
	rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
		USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);

	/* Allocate status and change arrays to accumulate information */
	alloc_size = sizeof (uint16_t) * (nports + 1);
	status = (uint16_t *)kmem_zalloc(alloc_size, KM_SLEEP);
	change = (uint16_t *)kmem_zalloc(alloc_size, KM_SLEEP);

	mutex_enter(HUBD_MUTEX(hubd));

	while ((hubd->h_dev_state != USB_DEV_DISCONNECTED) &&
		(hubd->h_port_change)) {
		old_connect_status = hubd->h_port_connected;

		/*
		 * For each port that reported a change, ack it, so that
		 * hub doesn't return with the same status again
		 * Also store all the change and status information
		 */
		rval = hubd_open_default_pipe(hubd,
			USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
		ASSERT(rval == USB_SUCCESS);

		for (port = 1; port <= nports; port++) {
			uint_t port_mask = 1 << port;
			if (hubd->h_port_change & port_mask) {
				/* ack changes */
				(void) hubd_determine_port_status(hubd, port,
				    &status[port], &change[port],
				    HUBD_ACK_CHANGES);
			}
		}

		rval = hubd_close_default_pipe(hubd);
		ASSERT(rval == USB_SUCCESS);

		/*
		 * The 0th bit is the hub status change bit.
		 * handle loss of local power here
		 */
		if (hubd->h_port_change & HUB_CHANGE_STATUS) {
			USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "hubd_hotplug_thread: hub status change!");

			/*
			 * This should be handled properly.  For now,
			 * mask off the bit.
			 */
			hubd->h_port_change = hubd->h_port_change &
					~HUB_CHANGE_STATUS;

			/*
			 * check and ack hub status
			 * this causes stall conditions
			 * when local power is removed
			 */
			rval = hubd_open_default_pipe(hubd,
				USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
			ASSERT(rval == USB_SUCCESS);

			(void) hubd_get_hub_status(hubd);

			rval = hubd_close_default_pipe(hubd);
			ASSERT(rval == USB_SUCCESS);
		}

		for (port = 1; port <= nports; port++) {
			uint_t port_mask = 1 << port;
			uint_t was_connected = old_connect_status & port_mask;

			USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "hubd_hotplug_thread: "
			    "port %d mask = 0x%x change = 0x%x "
			    "connected = 0x%x",
			    port, port_mask, hubd->h_port_change,
			    was_connected);

			/*
			 * is this a port connection that changed?
			 */
			if ((hubd->h_port_change & port_mask) == 0) {
				continue;
			}
			hubd->h_port_change &= ~port_mask;

			USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "handle port %d:\n\t"
			    "status = 0x%x change = 0x%x was_conn = 0x%x "
			    "conn = 0x%x mask = 0x%x",
			    port, status[port], change[port], was_connected,
			    hubd->h_port_connected, port_mask);

			/* Recover a disabled port */
			if (change[port] & PORT_CHANGE_PESC) {
				USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG,
					hubd->h_log_handle,
					"port%d Disabled - "
					"status:0x%x, change:0x%x",
					port, status[port], change[port]);

				/*
				 * if the port was connected and is still
				 * connected, recover the port
				 */
				if (was_connected && (status[port] &
					PORT_STATUS_CCS)) {

					hubd_recover_disabled_port(hubd, port);
				}
			}

			/*
			 * Now check what changed on the port
			 */
			if (change[port] & PORT_CHANGE_CSC) {
				if ((status[port] & PORT_STATUS_CCS) &&
				    (!was_connected)) {
					/* new device plugged in */
					hubd_handle_port_connect(hubd, port);
					connects++;

				} else if (was_connected) {
					/* this is a disconnect */
					mutex_exit(HUBD_MUTEX(hubd));
					hubd_post_disconnect_event(hubd, port);
					mutex_enter(HUBD_MUTEX(hubd));

					/* delete the children on this port */
					(void) hubd_delete_child(hubd, port,
					    NDI_DEVI_REMOVE|NDI_DEVI_FORCE);
				}
			}

			/*
			 * Check if any port is coming out of suspend
			 */
			if (change[port] & PORT_CHANGE_PSSC) {

				if (status[port] & PORT_STATUS_PSS) {
					/* port cannot suspend on it own */
					USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG,
					    hubd->h_log_handle,
					    "Port suspended");

				/* a resuming device could have disconnected */
				} else if (was_connected &&
					hubd->h_children_dips[port]) {

					/* device on this port resuming */
					int old_state = hubd->h_dev_state;
					dev_info_t *dip;

					hubd->h_dev_state =
						USB_DEV_HUB_CHILD_PWRLVL;

					dip = hubd->h_children_dips[port];
					mutex_exit(HUBD_MUTEX(hubd));

					/* get the device to full power */
					rval = pm_raise_power(dip, 0,
						USB_DEV_OS_FULL_POWER);
					mutex_enter(HUBD_MUTEX(hubd));

					/*
					 * make sure that we don't accidently
					 * over write the disconnect state
					 */
					if (hubd->h_dev_state ==
					    USB_DEV_HUB_CHILD_PWRLVL) {
						hubd->h_dev_state = old_state;
					}
				}
			}
		}
	}

	/*
	 * If interrupt pipe is not closing, We need to finish the pipe
	 * reset first, before we start polling. This will ensure that pipe
	 * reset doesnot stop polling
	 */
	if (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {
		USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "drain async intr pipe reset");

		cv_wait(&hubd->h_cv_reset_intrpipe, &hubd->h_mutex);
		ASSERT(hubd->h_intr_pipe_state != HUBD_INTR_PIPE_RESETTING);
	}

	/* let things settle down before causing more hotplug events */
	if (hubd_hotplug_delay && connects) {
		int empty;
		/* wait for all drivers to be onlined */
		mutex_exit(HUBD_MUTEX(hubd));
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "waiting for queue empty: lbolt = %d", lbolt);
		empty = i_ndi_devi_hotplug_queue_empty((uint_t)NDI_SLEEP,
							hubd_hotplug_delay);
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "queue empty: lbolt = %d, empty = %d", lbolt, empty);
		mutex_enter(HUBD_MUTEX(hubd));
	}

	/* if intr pipe is closing, there is no use of starting polling */
	if (hubd->h_intr_pipe_state != HUBD_INTR_PIPE_CLOSING) {
		mutex_exit(HUBD_MUTEX(hubd));
		rval = usb_pipe_start_polling(hubd->h_ep1_ph,
			USB_FLAGS_SHORT_XFER_OK);
		ASSERT(rval == USB_SUCCESS);

		rval = usb_pipe_get_state(hubd->h_ep1_ph, &pipe_state,
			USB_FLAGS_SLEEP);
		if (pipe_state != USB_PIPE_STATE_ACTIVE) {
			USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
				"intr pipe state : %d", pipe_state);
		}
		mutex_enter(HUBD_MUTEX(hubd));
	}

exit:
	hubd->h_hotplug_thread = NULL;
	if (status) {
		kmem_free(status, alloc_size);
	}
	if (change) {
		kmem_free(change, alloc_size);
	}

	mutex_exit(HUBD_MUTEX(hubd));

	hubd_device_idle(hubd);
}


/*
 * hubd_handle_port_connect:
 *	Transition a port from Disabled to Enabled.  Ensure that the
 *	port is in the correct state before attempting to
 *	access the device.
 */
static void
hubd_handle_port_connect(hubd_t *hubd, usb_port_t port)
{
	int		rval;
	int		retry;
	usb_port_status_t port_status;
	uint16_t	status;
	uint16_t	change;
	long		time_delay;

	/*
	 * If a device is connected, transition the
	 * port from Disabled to the Enabled state.
	 * The device will receive downstream packets
	 * in the Enabled state.
	 *
	 * reset port and wait for the hub to report
	 * completion
	 */
	ASSERT(hubd->h_port_connected & (1 << port));

	/* Only one device gets the default address at a time */
	usba_enter_enumeration(hubd->h_usb_device);

	/* open the default pipe */
	rval = hubd_open_default_pipe(hubd,
		USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
	ASSERT(rval == USB_SUCCESS);

	status = 0;
	change = 0;

	/* calculate 600 ms delay time */
	time_delay = (6 * drv_usectohz(hubd_device_delay)) / 10;

	for (retry = 0; retry < HUBD_PORT_RETRY; retry++) {
		USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "resettting port%d", port);

		if ((rval = hubd_reset_port(hubd, port)) != USB_SUCCESS) {
			/* ack changes */
			(void) hubd_determine_port_status(hubd,
			    port, &status, &change, HUBD_ACK_CHANGES);
			continue;
		}

		USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "checking status port%d", port);

		/*
		 * When a low speed device is connected to any port of PPX
		 * it has to be explicitly enabled
		 */
		rval = hubd_enable_port(hubd, port);

		if ((rval = hubd_determine_port_status(hubd, port, &status,
			&change, HUBD_ACK_CHANGES)) != USB_SUCCESS) {

			USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "getting status failed (%d)", rval);

			(void) hubd_disable_port(hubd, port);

			/* ack changes */
			(void) hubd_determine_port_status(hubd, port,
			    &status, &change, HUBD_ACK_CHANGES);
			continue;
		}

		if (status & PORT_STATUS_POCI) {
			USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "port %d overcurrent", port);

			(void) hubd_disable_port(hubd, port);

			/* ack changes */
			(void) hubd_determine_port_status(hubd,
			    port, &status, &change, HUBD_ACK_CHANGES);
			continue;
		}

		if (status & PORT_STATUS_PSS) {
			USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "port %d suspended", port);

			(void) hubd_disable_port(hubd, port);

			/* ack changes */
			(void) hubd_determine_port_status(hubd, port,
			    &status, &change, HUBD_ACK_CHANGES);
			continue;
		}

		/* is status really OK? */
		if ((status & PORT_STATUS_MASK) != PORT_STATUS_OK) {
			USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "port %d status (0x%x) not OK", port, status);

			/* check if we still have the connection */
			if (!(status & PORT_STATUS_CCS)) {
				/* lost connection, set exit condition */
				retry = HUBD_PORT_RETRY;
			}
		} else {
			break;
		}

		/* wait a while until it settles? */
		USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "disabling port %d again", port);

		/* avoid delay in the first iteration */
		if (retry) {
			delay(time_delay);
		}
		(void) hubd_disable_port(hubd, port);
		if (retry) {
			delay(time_delay);
		}

		USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "retrying on port %d", port);
	}

	if (retry >= HUBD_PORT_RETRY) {
		USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "ignoring port %d", port);

		/*
		 * the port should be automagically
		 * disabled but just in case, we do
		 * it here
		 */
		(void) hubd_disable_port(hubd, port);

		/* ack changes */
		(void) hubd_determine_port_status(hubd,
		    port, &status, &change, HUBD_ACK_CHANGES);

		rval = hubd_close_default_pipe(hubd);
		ASSERT(rval == USB_SUCCESS);

		usba_exit_enumeration(hubd->h_usb_device);

		return;
	}


	/*
	 * Determine if the device is high or
	 * low speed
	 */
	if (status & PORT_STATUS_LSDA) {
		port_status = USB_LOW_SPEED_DEV;
	} else {
		port_status = USB_HIGH_SPEED_DEV;
	}

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "creating child port%d, status = %x port status=0x%x",
	    port, status, port_status);
	/*
	 * if the child already exists, set addrs and config to the device
	 * post connect event to the child
	 */
	if (hubd->h_children_dips[port]) {

		retry = 0;

		/* set addrs to this device */
		rval = hubd_setdevaddr(hubd, port);

		/*
		 * if set addrs fails, we need another retry
		 * after issuing a reset
		 */
		while ((rval != USB_SUCCESS) &&
			(retry < hubd_retry_enumerate)) {

			rval = hubd_reset_port(hubd, port);
			delay(drv_usectohz(hubd_device_delay/2));
			rval = hubd_setdevaddr(hubd, port);

			retry++;
		}

		/* set the default config for this device */
		hubd_setdevconfig(hubd, port);

		rval = hubd_close_default_pipe(hubd);
		ASSERT(rval == USB_SUCCESS);

		usba_exit_enumeration(hubd->h_usb_device);


		/* indicate to the child that it is online again */
		mutex_exit(HUBD_MUTEX(hubd));
		hubd_post_connect_event(hubd, port);
		mutex_enter(HUBD_MUTEX(hubd));

		return;
	}

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "creating child port%d, status = %x port status=0x%x",
	    port, status, port_status);

	/* create more hub children */
	for (retry = 0; retry < hubd_retry_enumerate; retry++) {


		rval = hubd_reset_port(hubd, port);
		if (rval != USB_SUCCESS) {
			break;
		}

		/*
		 * When a low speed device is connected to any port of PPX
		 * it has to be explicitly enabled
		 */
		rval = hubd_enable_port(hubd, port);

		/* avoid delays in the first iteration */
		if (retry) {
			delay(drv_usectohz(hubd_device_delay));
		}

		rval = hubd_create_child(hubd->h_dip,
					hubd,
					hubd->h_usb_device,
					hubd->h_hubdi_ops,
					port_status, port, retry);
		if (rval == USB_SUCCESS) {
			break;
		}

		ASSERT(hubd->h_children_dips[port] == NULL);

		USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "retrying port %d", port);

		if (retry) {
			delay(drv_usectohz(hubd_device_delay/2));
		}
	}


	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L0(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "connecting device on port %d failed", port);

		/* we should forget that it was connected */
		hubd->h_port_connected &= ~(1 << port);

		(void) hubd_disable_port(hubd, port);
		usba_update_hotplug_stats(hubd->h_dip,
		    USB_TOTAL_HOTPLUG_FAILURE|USB_HOTPLUG_FAILURE);
		hubd->h_total_hotplug_failure++;
	} else {
		usba_update_hotplug_stats(hubd->h_dip,
		    USB_TOTAL_HOTPLUG_SUCCESS|USB_HOTPLUG_SUCCESS);
		hubd->h_total_hotplug_success++;
	}

	rval = hubd_close_default_pipe(hubd);
	ASSERT(rval == USB_SUCCESS);

	usba_exit_enumeration(hubd->h_usb_device);
}


/*
 * hubd_no_powerswitch_check:
 *	This function is only called from hubd_attach.	If a hub
 *	doesn't have power-switching, the intial state of the hub
 *	may be that a device is connected (PORT_STATUS_CCS) and
 *	the PORT_CHANGE_CSC is set, but an event is not generated
 *	on the interrupt pipe.	The ports of this sort of hub
 *	must explicitly be checked for an attached device when the
 *	hub is first accessed.
 */
static void
hubd_no_powerswitch_check(hubd_t *hubd) {
	usb_port_t	port;
	uint16_t	status;
	uint16_t	change;

	/*
	 * Traverse each port to see if a device is attached
	 */
	for (port = 1; port <= hubd->h_hub_descr.bNbrPorts; port++) {
		/* Check the port's status */
		(void) hubd_determine_port_status(hubd,
				port, &status, &change, HUBD_ACK_CHANGES);

		/* See if a device is attached */
		if ((status & (PORT_STATUS_CCS)) &&
				(change & PORT_CHANGE_CSC)) {
			/* A device is attached */
			hubd_handle_port_connect(hubd, port);
		}
	}
}


/*
 * hubd_get_hub_status:
 */
static int
hubd_get_hub_status(hubd_t *hubd)
{
	int	rval;
	uint_t	completion_reason;
	mblk_t	*data = NULL;
	uint16_t status;
	uint16_t change;

	mutex_exit(HUBD_MUTEX(hubd));
	if ((rval = usb_pipe_sync_device_ctrl_receive(
				hubd->h_default_pipe,
				GET_HUB_STATUS,
				USB_REQ_GET_STATUS,
				0,
				0,
				GET_STATUS_LENGTH,
				&data,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

		USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "get hub status failed (%d)", rval);

		if (data) {
			freemsg(data);
			data = NULL;
		}

		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	ASSERT(completion_reason == 0);
	mutex_enter(HUBD_MUTEX(hubd));

	status = (*(data->b_rptr + 1) << 8) | *(data->b_rptr);
	change = (*(data->b_rptr + 3) << 8) | *(data->b_rptr + 2);

	if (status || change) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "hub status = 0x%x change = 0x%x", status, change);
	}

	if (status & HUB_LOCAL_POWER_STATUS) {
		/*
		 * the user must offline this hub in order to recover
		 */
		USB_DPRINTF_L0(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "local power has been lost, please disconnect hub");
	}
	if (status & HUB_OVER_CURRENT) {
		/*
		 * the user must offline this hub in order to recover
		 */
		USB_DPRINTF_L0(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "global over current condition, please disconnect hub");
	}

	mutex_exit(HUBD_MUTEX(hubd));

	freemsg(data);

	if (change & C_HUB_LOCAL_POWER_STATUS) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "clearing feature HUB_LOCAL_POWER_STATUS");

		if ((rval = usb_pipe_sync_device_ctrl_send(
			hubd->h_default_pipe,
			CLEAR_HUB_FEATURE,
			USB_REQ_CLEAR_FEATURE,
			C_HUB_LOCAL_POWER_STATUS,
			0,
			0,
			NULL,
			&completion_reason,
			USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature C_HUB_LOCAL_POWER_STATUS "
				    "failed (%d)", rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
		}
	}

	if (change & C_HUB_OVER_CURRENT) {
		/*
		 * the port power is automatically disabled so we
		 * won't see disconnects. the user has to offline the
		 * hub in order to recover
		 */
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "clearing feature HUB_OVER_CURRENT");
		if ((rval = usb_pipe_sync_device_ctrl_send(
			hubd->h_default_pipe,
			CLEAR_HUB_FEATURE,
			USB_REQ_CLEAR_FEATURE,
			CFS_C_HUB_OVER_CURRENT,
			0,
			0,
			NULL,
			&completion_reason,
			USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature C_HUB_OVER_CURRENT "
				    "failed (%d)", rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
		}
	}

	mutex_enter(HUBD_MUTEX(hubd));

	return (USB_SUCCESS);
}


/*
 * hubd_reset_port:
 */
static int
hubd_reset_port(hubd_t *hubd, usb_port_t port)
{
	int	rval;
	uint_t	completion_reason;
	int	port_mask = 1 << port;
	mblk_t	*data;
	uint16_t status;
	uint16_t change;
	int	i;
	clock_t	current_time;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_reset_port: port %d", port);

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	hubd->h_port_reset_wait |= port_mask;

	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_sync_device_ctrl_send(
				hubd->h_default_pipe,
				SET_PORT_FEATURE,
				USB_REQ_SET_FEATURE,
				CFS_PORT_RESET,
				port,
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "reset port%d failed (%d)", port, rval);

		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	ASSERT(completion_reason == 0);

	mutex_enter(HUBD_MUTEX(hubd));

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "waiting on cv for reset completion");

	/*
	 * wait for port status change event
	 */
	for (i = 0; i < HUBD_PORT_RETRY; i++) {
		/* do not start polling if intr pipe is being reset */
		if (hubd->h_intr_pipe_state == HUBD_INTR_PIPE_RESETTING) {
			current_time = ddi_get_lbolt();
			rval = cv_timedwait(&hubd->h_cv_reset_intrpipe,
				&hubd->h_mutex,
				current_time +
				drv_usectohz(hubd_device_delay));
			if (hubd->h_intr_pipe_state ==
			    HUBD_INTR_PIPE_RESETTING) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "intr pipe not reset yet!");

				return (USB_FAILURE);
			}
		}

		/*
		 * start polling ep1 for receiving notification on
		 * reset completion
		 */
		mutex_exit(HUBD_MUTEX(hubd));
		if (usb_pipe_start_polling(hubd->h_ep1_ph,
			USB_FLAGS_SHORT_XFER_OK) != USB_SUCCESS)
			USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "restart polling failed");

		mutex_enter(HUBD_MUTEX(hubd));

		/*
		 * sleep a max of 100ms for reset completion
		 * notification to be received
		 */
		current_time = ddi_get_lbolt();
		if (hubd->h_port_reset_wait & port_mask) {
			rval = cv_timedwait(&hubd->h_cv_reset_port,
				&hubd->h_mutex,
				current_time +
				drv_usectohz(hubd_device_delay / 10));
			if ((rval <= 0) &&
			    (hubd->h_port_reset_wait & port_mask)) {
				/* we got woken up because of a timeout */
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "timeout : reset port%d failed", port);

				hubd->h_port_reset_wait &=  ~port_mask;
				mutex_exit(HUBD_MUTEX(hubd));
				rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
					USB_FLAGS_SLEEP, NULL, NULL);
				if (rval != USB_SUCCESS) {
					/*
					 * Either async reset may be in
					 * progress, or an exception callback
					 * may be outstanding. Issue a reset
					 * to put pipe in known state
					 */
					rval = usb_pipe_reset(hubd->h_ep1_ph,
						USB_FLAGS_SLEEP, NULL, NULL);
					ASSERT(rval == USB_SUCCESS);

				}
				mutex_enter(HUBD_MUTEX(hubd));

				return (USB_FAILURE);
			}
		}

		USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "reset completion received");

		mutex_exit(HUBD_MUTEX(hubd));
		rval = usb_pipe_stop_polling(hubd->h_ep1_ph,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		data = NULL;

		/* check status to determine whether reset completed */
		if ((rval = usb_pipe_sync_device_ctrl_receive(
					hubd->h_default_pipe,
					GET_PORT_STATUS,
					USB_REQ_GET_STATUS,
					0,
					port,
					GET_STATUS_LENGTH,
					&data,
					&completion_reason,
					USB_FLAGS_ENQUEUE)) !=
						USB_SUCCESS) {

				USB_DPRINTF_L1(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "get status port%d failed (%d)",
				    port, rval);

				if (data) {
					freemsg(data);
					data = NULL;
				}

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
				mutex_enter(HUBD_MUTEX(hubd));

				continue;
		}

		status = (*(data->b_rptr + 1) << 8) | *(data->b_rptr);
		change = (*(data->b_rptr + 3) << 8) | *(data->b_rptr + 2);

		freemsg(data);

		if (status & PORT_STATUS_PRS) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "port%d reset active", port);
			mutex_enter(HUBD_MUTEX(hubd));

			continue;
		} else {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "port%d reset inactive", port);
		}

		if (change & PORT_CHANGE_PRSC) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "clearing feature CFS_C_PORT_RESET");

			if ((rval = usb_pipe_sync_device_ctrl_send(
						hubd->h_default_pipe,
						CLEAR_PORT_FEATURE,
						USB_REQ_CLEAR_FEATURE,
						CFS_C_PORT_RESET,
						port,
						0,
						NULL,
						&completion_reason,
						USB_FLAGS_ENQUEUE)) !=
							USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature CFS_C_PORT_RESET"
				    " port%d failed (%d)",
				    port, rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);

			}
		}

		mutex_enter(HUBD_MUTEX(hubd));
		break;
	}

	if (i >= HUBD_PORT_RETRY) {
		/* port reset has failed */
		rval = USB_FAILURE;
	}

	return (rval);
}


/*
 * hubd_enable_port:
 */
static int
hubd_enable_port(hubd_t *hubd, usb_port_t port)
{
	int	rval, retval;
	uint_t	completion_reason;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_enable_port: port %d", port);

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	mutex_exit(HUBD_MUTEX(hubd));

	retval = usb_pipe_sync_device_ctrl_send(hubd->h_default_pipe,
		SET_PORT_FEATURE,
		USB_REQ_SET_FEATURE,
		CFS_PORT_ENABLE,
		port,
		0,
		NULL,
		&completion_reason,
		USB_FLAGS_ENQUEUE);

	if (retval != USB_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "enable port%d failed (%d)", port, retval);

		/* reset pipe so that we can access the device again */
		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		/* clear endpoint stall */
		if (completion_reason == USB_CC_STALL) {
			rval = usb_pipe_sync_device_ctrl_send(
				hubd->h_default_pipe,
				USB_DEV_REQ_HOST_TO_DEV |
				USB_DEV_REQ_RECIPIENT_ENDPOINT,
				USB_REQ_CLEAR_FEATURE,
				USB_ENDPOINT_HALT, /* Endpt Stall */
				usb_endpoint_num(hubd->h_default_pipe),
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE);

			ASSERT(rval == USB_SUCCESS);
		}

		mutex_enter(HUBD_MUTEX(hubd));

		return (retval);
	}

	ASSERT(completion_reason == 0);

	mutex_enter(HUBD_MUTEX(hubd));

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "enabling port done");

	return (retval);
}


/*
 * hubd_disable_port
 */
static int
hubd_disable_port(hubd_t *hubd, usb_port_t port)
{
	int	rval;
	uint_t	completion_reason;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_disable_port: port %d", port);

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_sync_device_ctrl_send(
				hubd->h_default_pipe,
				CLEAR_PORT_FEATURE,
				USB_REQ_CLEAR_FEATURE,
				CFS_PORT_ENABLE,
				port,
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

		USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "disable port%d failed (%d)", port, rval);

		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	}

	ASSERT(completion_reason == 0);

	USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "clearing feature CFS_C_PORT_ENABLE");

	if ((rval = usb_pipe_sync_device_ctrl_send(
				hubd->h_default_pipe,
				CLEAR_PORT_FEATURE,
				USB_REQ_CLEAR_FEATURE,
				CFS_C_PORT_ENABLE,
				port,
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) !=
					USB_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_PORT,
		    hubd->h_log_handle,
		    "clear feature CFS_C_PORT_ENABLE port%d failed (%d)",
		    port, rval);

		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);

	}

	ASSERT(completion_reason == 0);

	mutex_enter(HUBD_MUTEX(hubd));

	return (USB_SUCCESS);
}


/*
 * hubd_determine_port_status:
 */
static int
hubd_determine_port_status(hubd_t *hubd, usb_port_t port,
		uint16_t *status, uint16_t *change, uint_t ack_flag)
{
	int rval;
	mblk_t	*data = NULL;
	int port_mask = 1 << port;
	uint_t	completion_reason;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_determine_port_status: port %d", port);

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_sync_device_ctrl_receive(
				hubd->h_default_pipe,
				GET_PORT_STATUS,
				USB_REQ_GET_STATUS,
				0,
				port,
				GET_STATUS_LENGTH,
				&data,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

		USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port %d: get status failed (%d)", port, rval);

		if (data) {
			freemsg(data);
			data = NULL;
		}

		rval = usb_pipe_reset(hubd->h_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		*status = *change = 0;
		mutex_enter(HUBD_MUTEX(hubd));

		return (rval);
	}

	mutex_enter(HUBD_MUTEX(hubd));

	ASSERT((data->b_wptr - data->b_rptr) == GET_STATUS_LENGTH);

	*status = (*(data->b_rptr + 1) << 8) | *(data->b_rptr);
	*change = (*(data->b_rptr + 3) << 8) | *(data->b_rptr + 2);

	USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "port%d status = 0x%x, change = 0x%x", port, *status, *change);

	freemsg(data);

	if (*status & PORT_STATUS_CCS) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d connected", port);

		hubd->h_port_connected |= port_mask;
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d disconnected", port);

		hubd->h_port_connected &= ~port_mask;
	}

	if (*status & PORT_STATUS_PES) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d enabled", port);

		hubd->h_port_enabled |= port_mask;
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d disabled", port);

		hubd->h_port_enabled &= ~port_mask;
	}

	if (*status & PORT_STATUS_PSS) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d suspended", port);
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d not suspended", port);
	}

	if (*change & PORT_CHANGE_PRSC) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d reset completed", port);
	}
	if (*status & PORT_STATUS_POCI) {
		USB_DPRINTF_L1(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d over current!", port);
	}
	if (*status & PORT_STATUS_PRS) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d reset active", port);
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d reset inactive", port);
	}
	if (*status & PORT_STATUS_PPS) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d power on", port);
		hubd->h_port_powered |= port_mask;
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d power off", port);
		hubd->h_port_powered &= ~port_mask;
	}
	if (*status & PORT_STATUS_LSDA) {
		USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "port%d low speed", port);
	}

	/*
	 * Acknowledge connection, enable, reset status
	 */
	if (ack_flag) {
		mutex_exit(HUBD_MUTEX(hubd));
		if (*change & PORT_CHANGE_CSC) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "clearing feature CFS_C_PORT_CONNECTION");
			if ((rval = usb_pipe_sync_device_ctrl_send(
						hubd->h_default_pipe,
						CLEAR_PORT_FEATURE,
						USB_REQ_CLEAR_FEATURE,
						CFS_C_PORT_CONNECTION,
						port,
						0,
						NULL,
						&completion_reason,
						USB_FLAGS_ENQUEUE)) !=
							USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature CFS_C_PORT_CONNECTION"
				    " port%d failed (%d)", port, rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}
		}
		if (*change & PORT_CHANGE_PESC) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "clearing feature CFS_C_PORT_ENABLE");
			if ((rval = usb_pipe_sync_device_ctrl_send(
						hubd->h_default_pipe,
						CLEAR_PORT_FEATURE,
						USB_REQ_CLEAR_FEATURE,
						CFS_C_PORT_ENABLE,
						port,
						0,
						NULL,
						&completion_reason,
						USB_FLAGS_ENQUEUE)) !=
							USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature CFS_C_PORT_ENABLE"
				    " port%d failed (%d)", port, rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}
		}
		if (*change & PORT_CHANGE_PSSC) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "clearing feature CFS_C_PORT_SUSPEND");

			if ((rval = usb_pipe_sync_device_ctrl_send(
						hubd->h_default_pipe,
						CLEAR_PORT_FEATURE,
						USB_REQ_CLEAR_FEATURE,
						CFS_C_PORT_SUSPEND,
						port,
						0,
						NULL,
						&completion_reason,
						USB_FLAGS_ENQUEUE)) !=
							USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature CFS_C_PORT_SUSPEND"
				    " port%d failed (%d)", port, rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}
		}
		if (*change & PORT_CHANGE_OCIC) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "clearing feature CFS_C_PORT_OVER_CURRENT");

			if ((rval = usb_pipe_sync_device_ctrl_send(
						hubd->h_default_pipe,
						CLEAR_PORT_FEATURE,
						USB_REQ_CLEAR_FEATURE,
						CFS_C_PORT_OVER_CURRENT,
						port,
						0,
						NULL,
						&completion_reason,
						USB_FLAGS_ENQUEUE)) !=
							USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature CFS_C_PORT_OVER_CURRENT"
				    " port%d failed (%d)", port, rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}
		}
		if (*change & PORT_CHANGE_PRSC) {
			USB_DPRINTF_L3(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "clearing feature CFS_C_PORT_RESET");
			if ((rval = usb_pipe_sync_device_ctrl_send(
						hubd->h_default_pipe,
						CLEAR_PORT_FEATURE,
						USB_REQ_CLEAR_FEATURE,
						CFS_C_PORT_RESET,
						port,
						0,
						NULL,
						&completion_reason,
						USB_FLAGS_ENQUEUE)) !=
							USB_SUCCESS) {
				USB_DPRINTF_L2(DPRINT_MASK_PORT,
				    hubd->h_log_handle,
				    "clear feature CFS_C_PORT_RESET"
				    " port%d failed (%d)", port, rval);

				rval = usb_pipe_reset(hubd->h_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
				ASSERT(rval == USB_SUCCESS);
			}
		}
		mutex_enter(HUBD_MUTEX(hubd));
	}

	return (USB_SUCCESS);
}


/*
 * hubd_recover_disabled_port
 * if the port got disabled because of an error
 * enable it. If hub doesnot suport enable port,
 * reset the port to bring the device to life again
 */
static void
hubd_recover_disabled_port(hubd_t *hubd, usb_port_t port)
{
	int		rval;
	uint16_t	status;
	uint16_t	change;

	/* open the default pipe */
	rval = hubd_open_default_pipe(hubd,
		USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL);
	ASSERT(rval == USB_SUCCESS);

	/* first try enabling the port */
	rval = hubd_enable_port(hubd, port);
	ASSERT(rval == USB_SUCCESS);

	/* read the port status */
	(void) hubd_determine_port_status(hubd, port, &status, &change,
		HUBD_ACK_CHANGES);

	rval = hubd_close_default_pipe(hubd);
	ASSERT(rval == USB_SUCCESS);

	if (status & PORT_STATUS_PES) {
		USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			"Port%d now Enabled", port);
	} else if (status & PORT_STATUS_CCS) {
		/* first post a disconnect event to the child */
		mutex_exit(HUBD_MUTEX(hubd));
		hubd_post_disconnect_event(hubd, port);
		mutex_enter(HUBD_MUTEX(hubd));

		/* then reset the port and recover the device */
		hubd_handle_port_connect(hubd, port);

		USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			"Port%d now Enabled by force", port);
	}
}


/*
 * hubd_enable_all_port_power:
 */
static int
hubd_enable_all_port_power(hubd_t *hubd)
{
	usb_hub_descr_t	*hub_descr = &hubd->h_hub_descr;
	uint_t		completion_reason;
	int		rval = USB_FAILURE;
	usb_port_t	port;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_enable_all_port_power");

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);


	/*
	 * Enable power per port. we ignore gang power and power mask
	 * and always enable all ports one by one.
	 *
	 * If the hub doesn't support power-swtiching, just return
	 * set the right bits in h_port_powered and return
	 * USB_SUCCESS.
	 */
	for (port = 1; port <= hubd->h_hub_descr.bNbrPorts; port++) {
		/*
		 * Transition the port from the Powered Off to the
		 * Disconnected state by supplying power to the port.
		 */
		USB_DPRINTF_L4(DPRINT_MASK_PORT,
		    hubd->h_log_handle,
		    "hubd_enable_all_port_power: power port %d",
		    port);

		mutex_exit(HUBD_MUTEX(hubd));

		if ((rval = usb_pipe_sync_device_ctrl_send(
		    hubd->h_default_pipe,
		    SET_PORT_FEATURE,
		    USB_REQ_SET_FEATURE,
		    CFS_PORT_POWER,
		    port,
		    0,
		    NULL,
		    &completion_reason,
		    USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {
			USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
			    "set port power failed (0x%x)", rval);

			rval = usb_pipe_reset(
			    hubd->h_default_pipe,
			    USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);
			mutex_enter(HUBD_MUTEX(hubd));

		} else {

			ASSERT(completion_reason == USB_CC_NOERROR);
			mutex_enter(HUBD_MUTEX(hubd));
			hubd->h_port_powered |= 1 << port;

		}
	}

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_enable_all_port_power: rval = 0x%x, wait = %d",
	    rval, hubd->h_hub_descr.bPwrOn2PwrGood * 2 * 1000);

	if (hub_descr->wHubCharacteristics & HUB_CHARS_NO_POWER_SWITCHING) {
		delay(drv_usectohz(
			hubd->h_hub_descr.bPwrOn2PwrGood * 2 * 1000));
	}

	return (rval);
}


/*
 * hubd_disable_all_port_power:
 */
static int
hubd_disable_all_port_power(hubd_t *hubd)
{
	usb_port_t port;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_disable_all_port_power");

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	/*
	 * disable power per port, ignore gang power and power mask
	 */
	for (port = 1; port <= hubd->h_hub_descr.bNbrPorts; port++) {
		(void) hubd_disable_port_power(hubd, port);
	}

	return (USB_SUCCESS);
}


/*
 * hubd_disable_port_power:
 *	disable individual port power
 */
static int
hubd_disable_port_power(hubd_t *hubd, usb_port_t port)
{
	int		rval;
	uint_t		completion_reason;

	USB_DPRINTF_L4(DPRINT_MASK_PORT, hubd->h_log_handle,
	    "hubd_disable_port_power: port %d", port);

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));
	ASSERT(hubd->h_default_pipe != 0);

	mutex_exit(HUBD_MUTEX(hubd));

	if ((rval = usb_pipe_sync_device_ctrl_send(
				hubd->h_default_pipe,
				CLEAR_PORT_FEATURE,
				USB_REQ_CLEAR_FEATURE,
				CFS_PORT_POWER,
				port,
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

		USB_DPRINTF_L2(DPRINT_MASK_PORT, hubd->h_log_handle,
		    "clearing port%d power failed (0x%x)", port, rval);

		rval = usb_pipe_reset(hubd->h_default_pipe,
		    USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(HUBD_MUTEX(hubd));

		return (USB_FAILURE);
	} else {

		mutex_enter(HUBD_MUTEX(hubd));
		ASSERT(completion_reason == 0);
		hubd->h_port_powered &= ~(1 << port);
		return (USB_SUCCESS);
	}
}


/*
 * hubd_create_child
 *	- create child dip
 *	- open default pipe
 *	- get device descriptor
 *	- set the address
 *	- get configuration descriptor
 *	- set the configuration
 *	- get interface and endpoint descriptors
 *	- close default pipe
 *	- load appropriate driver(s)
 */
static int
hubd_create_child(dev_info_t *dip,
		hubd_t		*hubd,
		usb_device_t	*hubd_ud,
		usb_hubdi_ops_t *usb_hubdi_ops,
		usb_port_status_t port_status,
		usb_port_t	port,
		int		iteration)
{
	dev_info_t		*child_dip = NULL;
	usb_device_descr_t	usb_dev_descr;
	int			rval;
	usb_device_t		*child_ud = NULL;
	usb_pipe_handle_t	ph = NULL; /* default pipe handle */
	mblk_t			*pdata = NULL;
	uint_t			completion_reason;
	usb_config_descr_t	config_descriptor;
	uint16_t		config_number;
	uchar_t			address = 0;
	uint16_t		length;
	size_t			size;

	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_create_child: port %d", port);

	ASSERT(mutex_owned(HUBD_MUTEX(hubd)));

	child_ud = hubd->h_usb_devices[port];

	mutex_exit(HUBD_MUTEX(hubd));

	/*
	 * create a dip which can be used to open the pipe. we set
	 * the name after getting the descriptors from the device
	 */
	rval = usba_create_child_devi(dip,
			"device",		/* driver name */
			hubd_ud->usb_hcdi_ops, /* usb_hcdi ops */
			hubd_ud->usb_root_hub_dip,
			usb_hubdi_ops,		/* usb_hubdi_ops */
			port_status,		/* low speed device */
			child_ud,
			&child_dip);

	if (rval != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "usb_create_child_devi failed (%d)", rval);

		goto fail_cleanup;
	}

	child_ud = usba_get_usb_device(child_dip);
	ASSERT(child_ud != NULL);

	mutex_enter(&child_ud->usb_mutex);
	address = child_ud->usb_addr;
	child_ud->usb_addr = 0;
	child_ud->usb_dev_descr = kmem_alloc(sizeof (usb_device_descr_t),
		KM_SLEEP);
	bzero(&usb_dev_descr, sizeof (usb_device_descr_t));
	usb_dev_descr.bMaxPacketSize0 = 8;
	bcopy(&usb_dev_descr, child_ud->usb_dev_descr,
			sizeof (usb_device_descr_t));
	child_ud->usb_port = port;
	mutex_exit(&child_ud->usb_mutex);

	/* Open the default pipe */
	rval = usb_pipe_open(child_dip, NULL, NULL, USB_FLAGS_OPEN_EXCL, &ph);

	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			"usb_pipe_open failed (%d)", rval);

		goto fail_cleanup;
	}

	/*
	 * get device descriptor
	 */
	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_create_child: get device descriptor, 8 bytes");

	rval = usb_pipe_sync_device_ctrl_receive(ph,
			USB_DEV_REQ_DEVICE_TO_HOST |
				USB_DEV_REQ_TYPE_STANDARD,
			USB_REQ_GET_DESCRIPTOR,		/* bRequest */
			USB_DESCR_TYPE_SETUP_DEVICE,	/* wValue */
			0,				/* wIndex */
			8,				/* wLength */
			&pdata,
			&completion_reason,
			0);

	if (rval != USB_SUCCESS) {

		USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "getting device descriptor failed (%d)", rval);

		goto fail_cleanup;
	}

	ASSERT(completion_reason == USB_CC_NOERROR);
	ASSERT(pdata != NULL);

	size = usb_parse_device_descr(
			pdata->b_rptr,
			pdata->b_wptr - pdata->b_rptr,
			&usb_dev_descr,
			sizeof (usb_device_descr_t));

	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "parsing device descriptor returned %d", size);

	length = *(pdata->b_rptr);
	freemsg(pdata);
	pdata = NULL;
	if (size < 8) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "get device descriptor returned %d bytes", size);

		goto fail_cleanup;
	}

	rval = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);
	ASSERT(ph == NULL);

	if (length < 8) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "fail enumeration: bLength=%d", length);

		goto fail_cleanup;
	}

	/* save this device descriptor */
	mutex_enter(&child_ud->usb_mutex);
	bcopy(&usb_dev_descr, child_ud->usb_dev_descr,
		sizeof (usb_device_descr_t));
	mutex_exit(&child_ud->usb_mutex);

	/*
	 * reopen the default pipe using the new device descriptor
	 */
	rval = usb_pipe_open(child_dip, NULL, NULL,
					USB_FLAGS_OPEN_EXCL, &ph);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "usb_pipe_open failed (%d)", rval);

		goto fail_cleanup;
	}


	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_create_child: get full device descriptor, %d bytes",
	    length);

	rval = usb_pipe_sync_device_ctrl_receive(ph,
			USB_DEV_REQ_DEVICE_TO_HOST |
				USB_DEV_REQ_TYPE_STANDARD,
			USB_REQ_GET_DESCRIPTOR,		/* bRequest */
			USB_DESCR_TYPE_SETUP_DEVICE,	/* wValue */
			0,				/* wIndex */
			length,				/* wLength */
			&pdata,
			&completion_reason,
			0);

	if (rval != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "getting device descriptor failed (%d)", rval);

		goto fail_cleanup;
	}

	size = usb_parse_device_descr(
			pdata->b_rptr,
			pdata->b_wptr - pdata->b_rptr,
			&usb_dev_descr,
			sizeof (usb_device_descr_t));

	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "parsing device descriptor returned %d", size);



	/*
	 * ??? For now, free the data
	 * eventually, each configuration may need to be looked at
	 */
	freemsg(pdata);
	pdata = NULL;

	if (size != USB_DEVICE_DESCR_SIZE) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "fail enumeration: descriptor size = %d "
		    "expected size = %d", size, USB_DEVICE_DESCR_SIZE);

		goto fail_cleanup;
	}

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "device descriptor:\n\t"
	    "l = 0x%x type = 0x%x USB = 0x%x class = 0x%x subclass = 0x%x\n\t"
	    "protocol = 0x%x maxpktsize = 0x%x "
	    "Vid = 0x%x Pid = 0x%x rel = 0x%x\n\t"
	    "Mfg = 0x%x P = 0x%x sn = 0x%x #config = 0x%x",
	    usb_dev_descr.bLength,
	    usb_dev_descr.bDescriptorType,
	    usb_dev_descr.bcdUSB,
	    usb_dev_descr.bDeviceClass,
	    usb_dev_descr.bDeviceSubClass,
	    usb_dev_descr.bDeviceProtocol,
	    usb_dev_descr.bMaxPacketSize0,
	    usb_dev_descr.idVendor,
	    usb_dev_descr.idProduct,
	    usb_dev_descr.bcdDevice,
	    usb_dev_descr.iManufacturer,
	    usb_dev_descr.iProduct,
	    usb_dev_descr.iSerialNumber,
	    usb_dev_descr.bNumConfigurations);

	/*
	 * save the device descriptor in usb_device since it is needed
	 * later on again
	 */
	mutex_enter(&child_ud->usb_mutex);
	bcopy(&usb_dev_descr, child_ud->usb_dev_descr,
		sizeof (usb_device_descr_t));
	mutex_exit(&child_ud->usb_mutex);

	/* Set the address of the device */
	rval = usb_pipe_sync_device_ctrl_send(ph,
		USB_DEV_REQ_HOST_TO_DEV,
		USB_REQ_SET_ADDRESS,		/* bRequest */
		address,			/* wValue */
		0,				/* wIndex */
		0,				/* wLength */
		NULL,
		&completion_reason,
		0);


	if (rval != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "setting address failed %d", rval);

		goto fail_cleanup;
	}

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "set address 0x%x done", address);

	/* now close the pipe for addr 0 */
	rval = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);

	mutex_enter(&child_ud->usb_mutex);
	child_ud->usb_addr = address;
	mutex_exit(&child_ud->usb_mutex);

	/* re-open the pipe for the device with the new address */
	rval = usb_pipe_open(child_dip, NULL, NULL,
					USB_FLAGS_OPEN_EXCL, &ph);
	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "usb_pipe_open failed (%d)", rval);

		goto fail_cleanup;
	}

	/*
	 * This delay is important for the CATC hub to enumerate
	 * But, avoid delay in the first iteration
	 */
	if (iteration) {
		delay(drv_usectohz(hubd_device_delay/100));
	}

	/* Obtain the configuration descriptor */
	rval = usb_pipe_sync_device_ctrl_receive(ph,
			USB_DEV_REQ_DEVICE_TO_HOST |
				USB_DEV_REQ_TYPE_STANDARD,
			USB_REQ_GET_DESCRIPTOR,
			USB_DESCR_TYPE_SETUP_CONFIGURATION,
			0,
			USB_CONF_DESCR_SIZE,
			&pdata,
			&completion_reason,
			USB_FLAGS_ENQUEUE);

	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "get config/interface/ep descriptors failed (%d)", rval);

		goto fail_cleanup;
	}

	/*
	 * Parse the configuration descriptor to find the size of everything
	 */
	size = usb_parse_configuration_descr(pdata->b_rptr,
		    pdata->b_wptr - pdata->b_rptr,
		    &config_descriptor,
		    USB_CONF_DESCR_SIZE);

	freemsg(pdata);
	pdata = NULL;

	if (size != USB_CONF_DESCR_SIZE)  {

		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "rval != USB_CONF_DESCR_SIZE");

		rval = USB_FAILURE;
		goto fail_cleanup;
	}

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "config descriptor:\n\t"
	    "l = 0x%x type = 0x%x tl = 0x%x #int = 0x%x configv = 0x%x\n\t"
	    "ic = 0x%x att = 0x%x mp = 0x%x",
	    config_descriptor.bLength,
	    config_descriptor.bDescriptorType,
	    config_descriptor.wTotalLength,
	    config_descriptor.bNumInterfaces,
	    config_descriptor.bConfigurationValue,
	    config_descriptor.iConfiguration,
	    config_descriptor.bmAttributes,
	    config_descriptor.MaxPower);

	config_number = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "configuration",
	    config_descriptor.bConfigurationValue);

	if (config_number != 1) {
		USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "config number = %d", config_number);
	}

	/* Set the configuration */
	rval = usb_pipe_sync_device_ctrl_send(ph,
		USB_DEV_REQ_HOST_TO_DEV,
		USB_REQ_SET_CONFIGURATION,	/* bRequest */
		config_number,			/* wValue */
		0,				/* wIndex */
		0,				/* wLength */
		NULL,
		&completion_reason,
		0);

	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "setting configuration failed %d", rval);

		goto fail_cleanup;
	}


	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "set config done");

	/*
	 * get device status for self powered/remote wakeup status
	 */
	rval = usb_pipe_sync_device_ctrl_receive(ph,
			USB_DEV_REQ_DEVICE_TO_HOST |
				USB_DEV_REQ_TYPE_STANDARD,
			USB_REQ_GET_STATUS,		/* bRequest */
			0,				/* wValue */
			0,				/* wIndex */
			2,				/* wLength */
			&pdata,
			&completion_reason,
			0);

	if (rval != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "getting device status failed (%d)", rval);

		goto fail_cleanup;
	}

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "Device status (remote wakeup, self-powered) = 0x%x",
	    *pdata->b_rptr);

	freemsg(pdata);
	pdata = NULL;

	/*
	 * Fetch the interface descriptor & endpoint descriptor.
	 * Length of all of the descriptors is the wTotalLength of the
	 * configuration descriptor.
	 */
	rval = usb_pipe_sync_device_ctrl_receive(ph,
			USB_DEV_REQ_DEVICE_TO_HOST |
				USB_DEV_REQ_TYPE_STANDARD,
			USB_REQ_GET_DESCRIPTOR,
			USB_DESCR_TYPE_SETUP_CONFIGURATION,
			0,
			config_descriptor.wTotalLength,
			&pdata,
			&completion_reason,
			USB_FLAGS_ENQUEUE);

	if (rval != USB_SUCCESS) {

		USB_DPRINTF_L1(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
		    "getting interface and ep descriptors failed (%d)", rval);

		goto fail_cleanup;
	}

	ASSERT((pdata->b_wptr - pdata->b_rptr) != 0);
	ASSERT((pdata->b_wptr - pdata->b_rptr) ==
			config_descriptor.wTotalLength);

	/*
	 * copy config descriptor into usb_device
	 */
	mutex_enter(&child_ud->usb_mutex);
	ASSERT(child_ud->usb_config == NULL);
	child_ud->usb_config = kmem_alloc(
		config_descriptor.wTotalLength, KM_SLEEP);

	bcopy((caddr_t)pdata->b_rptr, (caddr_t)child_ud->usb_config,
		config_descriptor.wTotalLength);
	child_ud->usb_config_length = config_descriptor.wTotalLength;
	mutex_exit(&child_ud->usb_mutex);

	freemsg(pdata);
	pdata = NULL;

	/* close the default pipe */
	rval = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);
	ph = NULL;

	/*
	 * update usb_device
	 */
	mutex_enter(&child_ud->usb_mutex);
	child_ud->usb_port_status = port_status;
	child_ud->usb_n_configs = usb_dev_descr.bNumConfigurations;
	child_ud->usb_n_interfaces = config_descriptor.bNumInterfaces;
	child_ud->usb_configuration_value = config_number;
	mutex_exit(&child_ud->usb_mutex);

	/*
	 * if the attach fails, we leave the devinfo around
	 * on the framework's orphan list
	 */


	/* an existing dip may be returned instead of the one we passed */
	child_dip = usba_bind_driver_to_device(child_dip, USBA_BIND_ATTACH);

	/*
	 * stash away dip and usb_device which we need when offlining
	 * this child
	 */
	mutex_enter(HUBD_MUTEX(hubd));
	ASSERT(hubd->h_children_dips[port] == NULL);
	hubd->h_children_dips[port] = child_dip;

	if (hubd->h_usb_devices[port] == NULL) {
		hubd->h_usb_devices[port] = usba_get_usb_device(child_dip);
	} else {
		ASSERT(hubd->h_usb_devices[port] ==
				usba_get_usb_device(child_dip));
	}

	return (USB_SUCCESS);


fail_cleanup:
	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_create_child: fail_cleanup");

	mutex_enter(HUBD_MUTEX(hubd));
	ASSERT(hubd->h_children_dips[port] == NULL);
	mutex_exit(HUBD_MUTEX(hubd));

	if (pdata) {
		freemsg(pdata);
	}

	if (ph) {
		rval = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);
	}

	if (child_dip) {
		int rval = usba_destroy_child_devi(child_dip,
			NDI_DEVI_REMOVE|NDI_DEVI_FORCE);
		ASSERT(rval == USB_SUCCESS);
	}

	if (child_ud) {
		/* to make sure we free the address */
		mutex_enter(&child_ud->usb_mutex);
		child_ud->usb_addr = address;
		mutex_exit(&child_ud->usb_mutex);

		mutex_enter(HUBD_MUTEX(hubd));
		if (hubd->h_usb_devices[port] == NULL) {
			usba_free_usb_device(child_ud);
		} else {
			mutex_enter(&child_ud->usb_mutex);
			if (child_ud->usb_dev_descr) {
				kmem_free(child_ud->usb_dev_descr,
				    sizeof (usb_device_descr_t));
				child_ud->usb_dev_descr = NULL;
			}
			if (child_ud->usb_config) {
				kmem_free(child_ud->usb_config,
				    child_ud->usb_config_length);
				child_ud->usb_config = NULL;
			}
			mutex_exit(&child_ud->usb_mutex);
			usba_unset_usb_address(child_ud);
		}
		mutex_exit(HUBD_MUTEX(hubd));
	}

exit:
	mutex_enter(HUBD_MUTEX(hubd));

	return (USB_FAILURE);
}


/*
 * hubd_delete_child:
 *	- free usb address
 *	- lookup child dips, there may be multiple on this port
 *	- offline each child devi
 */
static int
hubd_delete_child(hubd_t *hubd, usb_port_t port, uint_t flag)
{
	dev_info_t	*child_dip;
	int rval = USB_SUCCESS;

	USB_DPRINTF_L4(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_delete_child: port %d", port);

	child_dip = hubd->h_children_dips[port];

	mutex_exit(HUBD_MUTEX(hubd));

	if (child_dip) {
		/*
		 * make sure that the child still exists
		 */
		uint_t	circular_count;

		i_ndi_block_device_tree_changes(&circular_count);

		if (usba_child_exists(hubd->h_dip, child_dip) == USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
			    "hubd_delete_child:\n\t"
			    "dip = 0x%p (%s) at port %d",
			    child_dip, ddi_node_name(child_dip), port);

			/*
			 * this will also offline the driver instance,
			 * call the hcdi bus ctl and set parent private
			 * data to NULL
			 */
			rval = usba_destroy_child_devi(child_dip, flag);
		}
		i_ndi_allow_device_tree_changes(circular_count);
	}

	mutex_enter(HUBD_MUTEX(hubd));

	if (rval == USB_SUCCESS) {
		usb_device_t *usb_device = hubd->h_usb_devices[port];
		hubd->h_children_dips[port] = NULL;
		hubd->h_port_connected &= ~(1 << port);
		if (usb_device) {
			usba_unset_usb_address(usb_device);
			mutex_enter(&usb_device->usb_mutex);
			if (usb_device->usb_dev_descr) {
				kmem_free(usb_device->usb_dev_descr,
				    sizeof (usb_device_descr_t));
				usb_device->usb_dev_descr = NULL;
			}
			if (usb_device->usb_config) {
				kmem_free(usb_device->usb_config,
				    usb_device->usb_config_length);
				usb_device->usb_config = NULL;
			}
			if (usb_device->usb_string_descr) {
				kmem_free(usb_device->usb_string_descr,
				    USB_MAXSTRINGLEN);
				usb_device->usb_string_descr = NULL;
			}
			mutex_exit(&usb_device->usb_mutex);
		}
	}

	return (rval);
}

static int
hubd_delete_all_children(hubd_t *hubd)
{
	usb_port_t port;
	int nports = hubd->h_hub_descr.bNbrPorts;
	dev_info_t *child_dip;
	int	rv = USB_SUCCESS;

	for (port = 1; port <= nports; port++) {
		child_dip = hubd->h_children_dips[port];

		if (child_dip) {
			if (usba_child_exists(hubd->h_dip, child_dip) ==
			    USB_SUCCESS) {
				/*
				 * attempt to offline all  children and
				 * continue after failures
				 */
				int rval = hubd_delete_child(hubd, port,
				    NDI_DEVI_FORCE|NDI_DEVI_REMOVE);
				if (rval != USB_SUCCESS) {
					rv = USB_FAILURE;
				} else {
					child_dip = NULL;
				}
			} else {
				child_dip = NULL;
			}
		}

		if ((child_dip == NULL) && hubd->h_usb_devices[port]) {
#ifndef __lock_lint
			ASSERT((hubd->h_usb_devices[port])->
					usb_ref_count == 0);
#endif
			hubd_free_usb_device(hubd,
			    hubd->h_usb_devices[port]);
			hubd->h_usb_devices[port] = NULL;
		}
	}

	/* if all children were offlined, stop polling */
	if ((rv == USB_SUCCESS) && hubd->h_ep1_ph) {
		hubd_stop_polling(hubd);
		hubd->h_port_connected = 0;
		hubd->h_ep1_ph = NULL;
	}

	return (rv);
}


/*
 * hubd_free_usb_device:
 *	free usb device structure unless it is associated with
 *	the root hub which is handled diffently
 */
static void
hubd_free_usb_device(hubd_t *hubd, usb_device_t *usb_device)
{
	if (usb_device && (usb_device->usb_addr != ROOT_HUB_ADDR)) {
		usb_port_t port = usb_device->usb_port;
		dev_info_t *dip = hubd->h_children_dips[port];

		if (dip && (usba_child_exists(hubd->h_dip,
			dip) == USB_SUCCESS)) {
			ASSERT(!DDI_CF1(dip));
		}

		port = usb_device->usb_port;
		hubd->h_usb_devices[port] = NULL;
		hubd->h_port_connected &= ~(1 << port);

		usba_free_usb_device(usb_device);
	}
}


/*
 * event support
 */
static int
hubd_busop_get_eventcookie(dev_info_t *dip,
	dev_info_t *rdip,
	char *eventname,
	ddi_eventcookie_t *cookie,
	ddi_plevel_t *plevelp,
	ddi_iblock_cookie_t *iblock_cookie)
{
	hubd_t	*hubd = (hubd_t *)hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_busop_get_eventcookie: dip=0x%p, rdip=0x%p, "
	    "event=%s", (void *)dip, (void *)rdip, eventname);
	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "(dip=%s%d, rdip=%s%d)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip));

	/* return event cookie, iblock cookie, and level */
	return (ndi_event_retrieve_cookie(hubd->h_ndi_event_hdl,
		rdip, eventname, cookie, plevelp, iblock_cookie,
		NDI_EVENT_NOPASS));
}

static int
hubd_busop_add_eventcall(dev_info_t *dip,
	dev_info_t *rdip,
	ddi_eventcookie_t cookie,
	int (*callback)(dev_info_t *dip,
	    ddi_eventcookie_t cookie, void *arg,
	    void *bus_impldata),
	void *arg)
{
	hubd_t	*hubd = (hubd_t *)hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_busop_add_eventcall: dip=0x%p, rdip=0x%p "
	    "cookie=0x%x, cb=0xp, arg=0x%p",
	    (void *)dip, (void *)rdip, cookie, (void *)callback, arg);
	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "(dip=%s%d, rdip=%s%d, event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ndi_event_cookie_to_name(hubd->h_ndi_event_hdl, cookie));

	/* add callback to our event set */
	return (ndi_event_add_callback(hubd->h_ndi_event_hdl,
		rdip, cookie, callback, arg, NDI_EVENT_NOPASS));
}

static int
hubd_busop_remove_eventcall(dev_info_t *dip,
	dev_info_t *rdip,
	ddi_eventcookie_t cookie)
{
	hubd_t	*hubd = (hubd_t *)hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_busop_remove_eventcall: dip=0x%p, rdip=0x%p "
	    "cookie=0x%x", (void *)dip, (void *)rdip, cookie);
	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "(dip=%s%d, rdip=%s%d, event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ndi_event_cookie_to_name(hubd->h_ndi_event_hdl, cookie));

	/* remove event registration from our event set */
	return (ndi_event_remove_callback(hubd->h_ndi_event_hdl,
		rdip, cookie, NDI_EVENT_NOPASS));
}

static int
hubd_busop_post_event(dev_info_t *dip,
	dev_info_t *rdip,
	ddi_eventcookie_t cookie,
	void *bus_impldata)
{
	hubd_t	*hubd = (hubd_t *)hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_busop_post_event: dip=0x%p, rdip=0x%p "
	    "cookie=0x%x, impl=%xp",
	    (void *)dip, (void *)rdip, cookie, bus_impldata);
	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "(dip=%s%d, rdip=%s%d, event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ndi_event_cookie_to_name(hubd->h_ndi_event_hdl, cookie));

	/* post event to all children registered for this event */
	return (ndi_event_run_callbacks(hubd->h_ndi_event_hdl, rdip,
		cookie, bus_impldata, NDI_EVENT_NOPASS));
}

static void
hubd_post_disconnect_event(hubd_t *hubd, usb_port_t port)
{
	dev_info_t	*dip;
	ddi_eventcookie_t cookie = ndi_event_tag_to_cookie(
		hubd->h_ndi_event_hdl, HUBD_EVENT_TAG_HOT_REMOVAL);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_post_disconnect_event: port=%d", port);

	mutex_enter(HUBD_MUTEX(hubd));
	dip = hubd->h_children_dips[port];
	mutex_exit(HUBD_MUTEX(hubd));

	(void) ndi_event_do_callback(hubd->h_ndi_event_hdl,
		dip, cookie, NULL, NDI_EVENT_NOPASS);
}


static void
hubd_post_connect_event(hubd_t *hubd, usb_port_t port)
{
	dev_info_t	*dip;
	ddi_eventcookie_t cookie = ndi_event_tag_to_cookie(
		hubd->h_ndi_event_hdl, HUBD_EVENT_TAG_HOT_INSERTION);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_post_connect_event: port=%d", port);

	mutex_enter(HUBD_MUTEX(hubd));
	dip = hubd->h_children_dips[port];
	mutex_exit(HUBD_MUTEX(hubd));

	(void) ndi_event_do_callback(hubd->h_ndi_event_hdl,
		dip, cookie, NULL, NDI_EVENT_NOPASS);
}


hubd_connect_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	hubd_t	*hubd = (hubd_t *)hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_connect_event_callback: dip=0x%p, cookie=0x%x, "
	    "arg=0x%p, impl=0x%p",
	    (void *)dip, cookie, arg, bus_impldata);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "(dip=%s%d, event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ndi_event_cookie_to_name(hubd->h_ndi_event_hdl, cookie));

	(void) hubd_restore_device_state(dip, hubd);

	return (DDI_EVENT_CLAIMED);
}


hubd_disconnect_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	hubd_t	*hubd = (hubd_t *)hubd_get_soft_state(dip);

	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "hubd_disconnect_event_callback: dip=0x%p, cookie=0x%x, "
	    "arg=0x%p, impl=0x%p",
	    (void *)dip, cookie, arg, bus_impldata);
	USB_DPRINTF_L3(DPRINT_MASK_HOTPLUG, hubd->h_log_handle,
	    "(dip=%s%d, event=%s)",
	    ddi_driver_name(dip), ddi_get_instance(dip),
	    ndi_event_cookie_to_name(hubd->h_ndi_event_hdl, cookie));

	mutex_enter(HUBD_MUTEX(hubd));
	hubd->h_dev_state = USB_DEV_DISCONNECTED;

	/* stop polling */
	hubd_stop_polling(hubd);
	mutex_exit(HUBD_MUTEX(hubd));

	/* pass all events to the children */
	(void) ndi_event_run_callbacks(hubd->h_ndi_event_hdl, dip,
		cookie, bus_impldata, NDI_EVENT_NOPASS);

	return (DDI_EVENT_CLAIMED);
}


static void
hubd_register_events(hubd_t *hubd)
{
	int rval;
	ddi_plevel_t level;
	ddi_iblock_cookie_t icookie;

	/* get event cookie, discard levl and icookie for now */
	rval = ddi_get_eventcookie(hubd->h_dip, DDI_DEVI_REMOVE_EVENT,
		&hubd->h_remove_cookie, &level, &icookie);

	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(hubd->h_dip,
			hubd->h_remove_cookie, hubd_disconnect_event_callback,
			NULL);

		ASSERT(rval == DDI_SUCCESS);
	}
	rval = ddi_get_eventcookie(hubd->h_dip, DDI_DEVI_INSERT_EVENT,
		&hubd->h_insert_cookie, &level, &icookie);
	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(hubd->h_dip,
			hubd->h_insert_cookie, hubd_connect_event_callback,
			NULL);

		ASSERT(rval == DDI_SUCCESS);
	}
}


static void
hubd_deregister_events(hubd_t *hubd)
{
	int rval;

	if (hubd->h_remove_cookie) {
		rval = ddi_remove_eventcall(hubd->h_dip,
						hubd->h_remove_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}

	if (hubd->h_insert_cookie) {
		rval = ddi_remove_eventcall(hubd->h_dip,
						hubd->h_insert_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * create the pm components required for power management
 */
static void
hubd_create_pm_components(dev_info_t *dip, hubd_t *hubd)
{
	hub_power_t	*hubpm;

	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_create_pm_components : Begin");

	/* Allocate the state structure */
	hubpm = kmem_zalloc(sizeof (hub_power_t), KM_SLEEP);

	hubd->h_hubpm = hubpm;
	hubpm->hubp_hubd = hubd;
	hubpm->hubp_raise_power = B_FALSE;
	hubpm->hubp_pm_capabilities = 0;
	hubpm->hubp_current_power = USB_DEV_OS_FULL_POWER;

	/* alloc memory to save power states of children */
	hubpm->hubp_child_pwrstate = (uint8_t *)
		kmem_zalloc(MAX_PORTS + 1, KM_SLEEP);

	/*
	 * if the enable remote wakeup fails
	 * we still want to enable
	 * parent notification so we can PM the children
	 */
	if (usb_is_pm_enabled(dip) == USB_SUCCESS) {

		usb_enable_parent_notification(dip);

		if (usb_enable_remote_wakeup(dip) == USB_SUCCESS) {
			uint_t		pwr_states;

			USB_DPRINTF_L2(DPRINT_MASK_PM, hubd->h_log_handle,
			    "hubd_create_pm_components: "
			    "Remote Wakeup Enabled");

			if (usb_create_pm_components(dip, &pwr_states) ==
			    USB_SUCCESS) {
				hubpm->hubp_wakeup_enabled = 1;
				hubpm->hubp_pwr_states = (uint8_t)pwr_states;
			}
		}
	}

	USB_DPRINTF_L4(DPRINT_MASK_PM, hubd->h_log_handle,
	    "hubd_create_pm_components: END");
}


/*
 * Device / Hotplug control
 */
/* ARGSUSED */
int
usba_hubdi_open(dev_info_t *dip, dev_t *devp, int flags, int otyp,
	cred_t *credp)
{
	hubd_t *hubd;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	hubd = hubd_get_soft_state(dip);
	if (hubd == NULL) {
		return (ENXIO);
	}

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, hubd->h_log_handle,
	    "hubd_open:");

	mutex_enter(HUBD_MUTEX(hubd));
	if ((flags & FEXCL) && (hubd->h_softstate & HUBD_SS_ISOPEN)) {
		mutex_exit(HUBD_MUTEX(hubd));

		return (EBUSY);
	}

	hubd->h_softstate |= HUBD_SS_ISOPEN;
	mutex_exit(HUBD_MUTEX(hubd));

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, hubd->h_log_handle, "opened");

	return (0);
}


/* ARGSUSED */
int
usba_hubdi_close(dev_info_t *dip, dev_t dev, int flag, int otyp,
	cred_t *credp)
{
	hubd_t *hubd;

	if (otyp != OTYP_CHR) {
		return (EINVAL);
	}

	hubd = hubd_get_soft_state(dip);

	if (hubd == NULL) {
		return (ENXIO);
	}

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, hubd->h_log_handle, "hubd_close:");

	mutex_enter(HUBD_MUTEX(hubd));
	hubd->h_softstate &= ~HUBD_SS_ISOPEN;
	mutex_exit(HUBD_MUTEX(hubd));

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, hubd->h_log_handle, "closed");

	return (0);
}


/*
 * hubd_ioctl: devctl hotplug controls
 */
/* ARGSUSED */
int
usba_hubdi_ioctl(dev_info_t *self, dev_t dev, int cmd, intptr_t arg,
	int mode, cred_t *credp, int *rvalp)
{
	hubd_t *hubd;
	dev_info_t *child_dip = NULL;
	struct devctl_iocdata *dcp;
	int rv = 0;
	int nrv = 0;

	hubd = hubd_get_soft_state(self);
	if (hubd == NULL) {
		return (ENXIO);
	}

	USB_DPRINTF_L4(DPRINT_MASK_CBOPS, hubd->h_log_handle,
	    "cmd=%x, arg=%p, mode=%x, cred=%p, rval=%p",
	    cmd, arg, mode, credp, rvalp);

	/*
	 * read devctl ioctl data
	 */
	if (ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS) {
		return (EFAULT);
	}

	mutex_enter(HUBD_MUTEX(hubd));

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

			/* to avoid deadlocks, do async onlining */
			if (ndi_devi_online(child_dip, 0) != NDI_SUCCESS) {
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

			mutex_exit(HUBD_MUTEX(hubd));
			nrv = ndi_devi_offline(child_dip,
						NDI_DEVI_FORCE);
			mutex_enter(HUBD_MUTEX(hubd));

			if (nrv == NDI_BUSY) {
				rv = EBUSY;
			} else if (nrv == NDI_FAILURE) {
				rv = EIO;
			}
			break;

		case DEVCTL_DEVICE_REMOVE:
		{
			usb_device_t *usb_device;

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
			usb_device = usba_get_usb_device(child_dip);
			if (hubd_delete_child(hubd, usb_device->usb_port,
			    NDI_DEVI_REMOVE|NDI_DEVI_FORCE) != USB_SUCCESS) {
				rv = EIO;
			}
			break;
		}

		case DEVCTL_BUS_CONFIGURE:
			rv = hubd_devctl_bus_configure(hubd);
			break;

		case DEVCTL_BUS_UNCONFIGURE:
			rv = hubd_delete_all_children(hubd);
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

	mutex_exit(HUBD_MUTEX(hubd));

	return (rv);
}


/*
 * reconfigure (ie. reenumerate the bus) after it has been unconfigured
 *
 * by toggling the port power, we should see port change interrupts on
 * hubs with power switching. On hubs without power switching, we poll
 * the ports.
 */
static int
hubd_devctl_bus_configure(hubd_t *hubd)
{
	int rv = 0;

	if (hubd->h_ep1_ph == NULL) {
		if (hubd_open_default_pipe(hubd, USB_FLAGS_SLEEP) !=
		    USB_SUCCESS) {
			rv = EIO;
			goto done;
		}

		if (hubd_disable_all_port_power(hubd) != USB_SUCCESS) {
			rv = EIO;
			goto done;
		}

		if (hubd_enable_all_port_power(hubd) != USB_SUCCESS) {
			rv = EIO;
			goto done;
		}

		hubd->h_port_connected = 0;
		if (hubd_start_polling(hubd) != USB_SUCCESS) {
			rv = EIO;
			goto done;
		}

		if (hubd->h_hub_descr.wHubCharacteristics &
		    HUB_CHARS_NO_POWER_SWITCHING)  {
			hubd_no_powerswitch_check(hubd);
		}
	}

done:
	if (hubd->h_default_pipe && (hubd_close_default_pipe(hubd) !=
	    USB_SUCCESS)) {
		rv = EIO;
	}

	return (rv);
}

#ifdef	DEBUG
/*
 * Utility used to dump all HUB related information. This
 * function is exported to USB framework and gets registered
 * as the dump function.
 */
void
hubd_dump(uint_t flag, usb_opaque_t arg)
{
	char	pathname[MAXNAMELEN];
	hubd_t	*hubd = (hubd_t *)arg;
	uint_t	show = hubd_show_label;

	mutex_enter(&hubd_dump_mutex);
	hubd_show_label = USB_DISALLOW_LABEL;

	/* Root Hub */
	if (usba_is_root_hub(hubd->h_dip)) {
		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "\n******  root hub ****** dip: 0x%p", hubd->h_dip);

		(void) ddi_pathname(hubd->h_dip, pathname);
		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "****** DEVICE: %s", pathname);
		hubd_dump_state(hubd, flag);

		/*
		 * Dump root hub descriptors.
		 */
		if ((flag & USB_DUMP_DESCRIPTORS) && hubd->h_dip) {
			USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING,
			    hubd->h_log_handle,
			    "****** root hub descriptors ******");

			usba_dump_descriptors(hubd->h_dip, USB_DISALLOW_LABEL);
		}
	} else {
		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "****** hubd%d ****** dip: 0x%p",
		    hubd->h_instance, hubd->h_dip);

		/*
		 * Dump other hub descriptors.
		 */
		(void) ddi_pathname(hubd->h_dip, pathname);
		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "****** DEVICE: %s", pathname);
		hubd_dump_state(hubd, flag);
	}

	hubd_show_label = show;
	mutex_exit(&hubd_dump_mutex);
}


/*
 * hubd_dump_state:
 *	Dump hub state information.
 */
static void
hubd_dump_state(hubd_t *hubd, uint_t flag)
{
	int			port;
	dev_info_t		*dip;
	usb_device_t		*usb_device;
	usb_device_descr_t	*usb_dev_descr;
	hub_power_t		*hub_pm;

	_NOTE(NO_COMPETING_THREADS_NOW);
	if (flag & USB_DUMP_STATE) {
		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_instance: 0x%x\t\t\th_init_state: 0x%x",
		    hubd->h_instance, hubd->h_init_state);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_dev_state: 0x%x\t\thub_pm : 0x%p",
		    hubd->h_dev_state, hubd->h_hubpm);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_softstate: 0x%p\t\th_ep1_descr: 0x%p",
		    hubd->h_softstate, hubd->h_ep1_descr);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_pipe_policy: 0x%p\th_hub_descr: 0x%p",
		    hubd->h_pipe_policy, hubd->h_hub_descr);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_hotplug_thread: 0x%x\t\th_children_dips: 0x%p",
		    hubd->h_hotplug_thread, hubd->h_children_dips);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_cd_list_length: 0x%x\t\th_usb_devices: 0x%p",
		    hubd->h_cd_list_length, hubd->h_usb_devices);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_port_change: 0x%x\t\th_port_connected: 0x%x",
		    hubd->h_port_change, hubd->h_port_connected);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_port_powered: 0x%x\t\th_port_enabled: 0x%x",
		    hubd->h_port_powered, hubd->h_port_enabled);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "h_port_reset_wait: 0x%x\t\th_port_reset_wait: 0x%x",
		    hubd->h_port_reset_wait, hubd->h_port_reset_wait);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_hub_descr.bNbrPorts 0x%x\th_default_pipe: 0x%p",
			hubd->h_hub_descr.bNbrPorts, hubd->h_default_pipe);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_dump_ops 0x%p", hubd->h_dump_ops);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_log_handle 0x%p", hubd->h_log_handle);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_ndi_event_hdl 0x%p", hubd->h_ndi_event_hdl);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_remove_cookie 0x%p", hubd->h_remove_cookie);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_insert_cookie 0x%p", hubd->h_insert_cookie);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_total_hotplug_success %d",
			hubd->h_total_hotplug_success);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"h_total_hotplug_failure %d",
			hubd->h_total_hotplug_failure);

		hub_pm = hubd->h_hubpm;

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"\n\nhub power structure at 0x%p", hub_pm);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"hubp_hubd 0x%p", hub_pm->hubp_hubd);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"hubp_wakeup_enabled 0x%x\t\thubp_pwr_states 0x%x",
			hub_pm->hubp_wakeup_enabled, hub_pm->hubp_pwr_states);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"hubp_pm_capabilities 0x%x\t\thubp_current_power 0x%x",
			hub_pm->hubp_pm_capabilities,
			hub_pm->hubp_current_power);

		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
			"hubp_raise_power 0x%x\t\thubp_child_pwrstate 0x%x",
			hub_pm->hubp_raise_power, hub_pm->hubp_child_pwrstate);
	}

	if ((flag & USB_DUMP_USB_DEVICE) && hubd->h_usb_device) {
		USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING, hubd->h_log_handle,
		    "***** USB_DEVICE (h_usb_device) information *****");

		usba_dump_usb_device(hubd->h_usb_device, flag);
	}

	if (hubd->h_default_pipe) {
		if (flag & USB_DUMP_PIPE_POLICY) {
			usb_pipe_handle_impl_t *ph =
				(usb_pipe_handle_impl_t *)hubd->h_default_pipe;

			USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING,
			    hubd->h_log_handle,
			    "***** USB_PIPE_POLICY (h_default_pipe) info ****");

			/* no locking is needed here */
			_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(ph->p_policy))
			usba_dump_usb_pipe_policy(ph->p_policy, flag);
		}
	}

	if (hubd->h_ep1_ph) {

		if (flag & USB_DUMP_PIPE_POLICY) {
			usb_pipe_handle_impl_t *ph =
				(usb_pipe_handle_impl_t *)hubd->h_ep1_ph;

			USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING,
				hubd->h_log_handle,
				"***** USB_PIPE_POLICY (h_ep1_ph) info ****");

			/* no locking is needed here */
			_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(ph->p_policy))
			usba_dump_usb_pipe_policy(ph->p_policy, flag);
		}
	}

	/*
	 * Dump everything about the children
	 */
	for (port = 1; port <= hubd->h_hub_descr.bNbrPorts; port++) {
		dip = hubd->h_children_dips[port];
		usb_device = hubd->h_usb_devices[port];

		if ((flag & USB_DUMP_DESCRIPTORS) && dip) {
			USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING,
			    hubd->h_log_handle,
			    "****** HUB CHILD at PORT %d ****** "
			    " dip: 0x%p", port, dip);

			usba_dump_descriptors(dip, USB_DISALLOW_LABEL);
		}

		/*
		 * If one of the child of the hub in question is
		 * of type HUB, skip displaying its usb_device
		 * information. We will display that info, when
		 * we cover that hub in the next pass.
		 *
		 * This check, thus, will eliminate displaying
		 * the same information twice.
		 *
		 * One may like to know the descriptor information
		 * of a hub, hence this check is not before the previous
		 * if (((flag & USB_DUMP_DESCRIPTORS)) .... block
		 * of code.
		 */
		if (!usb_device) {
			continue;
		}

		usb_dev_descr = usb_device->usb_dev_descr;
		if ((usb_dev_descr) &&
			(usb_dev_descr->bDeviceClass == HUB_CLASS_CODE)) {
			continue;
		}

		if (flag & USB_DUMP_USB_DEVICE) {
			USB_DPRINTF_L3(DPRINT_MASK_HUBDI_DUMPING,
			    hubd->h_log_handle,
			    "\t****** USB_DEVICE of child at port %d"
			    " ******", port);
			usba_dump_usb_device(usb_device, flag);
		}
	} /* end of for */

	_NOTE(COMPETING_THREADS_NOW);
}
#endif	/* DEBUG */
