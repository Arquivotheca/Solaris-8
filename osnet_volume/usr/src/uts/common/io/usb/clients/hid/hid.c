/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hid.c	1.9	99/11/05 SMI"

/*
 * Human Interface Device driver (HID)
 *
 * The HID driver is a software driver which acts as a class
 * driver for USB human input devices like keyboard, mouse,
 * joystick etc and provides the class-specific interfaces
 * between these client driver modules and the Universal Serial
 * Bus Driver(USBA).
 */
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/usb/clients/hid/hid.h>
#include <sys/usb/clients/hid/hid_polled.h>
#include <sys/usb/clients/hidparser/hidparser.h>
#include <sys/usb/clients/hid/hidvar.h>
#include <sys/usb/clients/hidparser/hid_parser_driver.h>
#include <sys/stropts.h>

/* Debugging support */
static uint_t	hid_errmask = (uint_t)PRINT_MASK_ALL;
static uint_t	hid_errlevel = USB_LOG_L1;
static uint_t	hid_instance_debug = (uint_t)-1;
static uint_t	hid_show_label = USB_ALLOW_LABEL;

/* STREAMS module entry points */
static int hid_open();
static int hid_close();
static int hid_wput();
static int hid_wsrv();

static int	hid_polled_read(hid_polled_handle_t, uchar_t **);
static int	hid_polled_input_enter(hid_polled_handle_t);
static int	hid_polled_input_exit(hid_polled_handle_t);
static int	hid_polled_input_init(hid_state_t *);
static int	hid_polled_input_fini(hid_state_t *);

/*
 * Warlock is not aware of the automatic locking mechanisms for
 * streams drivers.  The hid streams enter points are protected by
 * a per module perimeter.  If the locking in hid is a bottleneck
 * per queue pair or per queue locking may be used.  Since warlock
 * is not aware of the streams perimeters, these notes have been added.
 *
 * Note that the perimeters do not protect the driver from callbacks
 * happening while a streams entry point is executing.  So, the hid_mutex
 * has been created to protect the data.
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per call", iocblk))
_NOTE(SCHEME_PROTECTS_DATA("unique per call", datab))
_NOTE(SCHEME_PROTECTS_DATA("unique per call", msgb))
_NOTE(SCHEME_PROTECTS_DATA("unique per call", queue))

/*
 * Warlock complains about the protection of the pipe policy
 * The pipe policy is protected at the hid level by the hid_mutex and by
 * mutexes at the usba and ohci levels.
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per call", usb_pipe_policy_t))


/* module information */
static struct module_info hid_mod_info = {
	0x0ffff,			/* module id number */
	"USB HID STREAMS driver",  	/* module name */
	0,				/* min packet size accepted */
	INFPSZ,				/* max packet size accepted */
	512,				/* hi-water mark */
	128				/* lo-water mark */
};


/* read queue information structure */
static struct qinit rinit = {
	NULL,				/* put procedure not needed */
	NULL,				/* service procedure not needed */
	hid_open,			/* called on startup */
	hid_close,			/* called on finish */
	NULL,				/* for future use */
	&hid_mod_info,   		/* module information structure */
	NULL 				/* module statistics structure */
};


/* write queue information structure */
static struct qinit winit = {
	hid_wput,			/* put procedure */
	hid_wsrv,			/* service procedure */
	NULL,				/* open not used on write side */
	NULL,				/* close not used on write side */
	NULL,				/* for future use */
	&hid_mod_info,			/* module information structure */
	NULL				/* module statistics structure */
};


struct streamtab hid_streamtab = {
	&rinit,
	&winit,
	NULL, 			/* not a MUX */
	NULL 			/* not a MUX */
};


struct cb_ops hid_cb_ops = {
	nulldev,		/* open  */
	nulldev,		/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nulldev,		/* dump */
	nulldev,		/* read */
	nulldev,		/* write */
	nulldev,		/* ioctl */
	nulldev,		/* devmap */
	nulldev,		/* mmap */
	nulldev,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	&hid_streamtab,		/* streamtab  */
	D_64BIT | D_MP | D_NEW| D_HOTPLUG | D_MTPERQ
};


/*
 * autoconfiguration data and routines.
 */
static int hid_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
				void *arg, void **result);
static int hid_attach(dev_info_t *dev, ddi_attach_cmd_t cmd);
static int hid_detach(dev_info_t *dev, ddi_detach_cmd_t cmd);
static int hid_restore_device_state(dev_info_t *dip, hid_state_t *hidp);
static int hid_cpr_suspend(hid_state_t *hidp);
static void hid_close_intr_pipe(hid_state_t *hidp);
static void hid_close_default_pipe(hid_state_t *hidp);
static void hid_cancel_timeouts(hid_state_t *hidp);
static void hid_create_pm_components(dev_info_t *dip, hid_state_t *hidp);
static int hid_power(dev_info_t *dip, int comp, int level);
static void hid_power_change_callback(void *arg, int rval);
static void hid_device_idle(hid_state_t *hidp);

static struct dev_ops hid_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	hid_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	hid_attach,		/* attach */
	hid_detach,		/* detach */
	nodev,			/* reset */
	&hid_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	hid_power		/* power */
};


static struct modldrv hidmodldrv =	{
	&mod_driverops,
	"USB HID Client Driver 1.9",
	&hid_ops			/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&hidmodldrv,
	NULL,
};

/* soft state structures */
#define	HID_INITIAL_SOFT_SPACE	4
static void *hid_state;

/*
 * prototypes
 */
static int hid_mctl_send(hid_state_t *, struct iocblk, char *, size_t);
static void hid_timeout(void *);
static int hid_mctl_receive(queue_t *, mblk_t *);
static int hid_ioctl(queue_t *, mblk_t *);
static void hid_detach_cleanup(hid_state_t *);
static int hid_send_async_ctrl_request(hid_state_t *, hid_req_t *,
		uchar_t, int, ushort_t, uint_t, hid_default_pipe_arg_t *);
static int hid_interrupt_pipe_callback(usb_pipe_handle_t,
		usb_opaque_t, mblk_t *);
static int hid_default_pipe_callback(usb_pipe_handle_t,
		usb_opaque_t, mblk_t *);

static void hid_async_pipe_close_callback(usb_opaque_t, int, uint_t);
static int hid_interrupt_pipe_exception_callback(usb_pipe_handle_t,
		usb_opaque_t, uint_t, mblk_t *, uint_t);
static int hid_default_pipe_exception_callback(usb_pipe_handle_t,
		usb_opaque_t, uint_t, mblk_t *, uint_t);
static void hid_interrupt_pipe_reset_callback(usb_opaque_t, int, uint_t);
static void hid_default_pipe_reset_callback(usb_opaque_t, int, uint_t);
static void hid_set_idle(hid_state_t *hidp);
static void hid_set_boot_protocol(hid_state_t *hidp);
static size_t hid_parse_hid_descr(uchar_t *, size_t, usb_hid_descr_t *,
		size_t, usb_interface_descr_t *);
static int hid_parse_hid_descr_failure(hid_state_t *);
static int hid_handle_report_descriptor(hid_state_t *, int, mblk_t **);
static void hid_flush(queue_t *);

static void hid_register_events(hid_state_t *);
static void hid_deregister_events(hid_state_t *);

static int hid_disconnect_event_callback(dev_info_t *,
	ddi_eventcookie_t, void *, void *);
static int hid_connect_event_callback(dev_info_t *,
	ddi_eventcookie_t, void *, void *);


int
_init(void)
{
	int rval;

	if (((rval = ddi_soft_state_init(&hid_state, sizeof (hid_state_t),
	    HID_INITIAL_SOFT_SPACE)) != 0)) {

		return (rval);
	}

	if ((rval = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&hid_state);
	}

	return (rval);
}


int
_fini(void)
{
	int rval;

	if ((rval = mod_remove(&modlinkage)) != 0) {

		return (rval);
	}

	ddi_soft_state_fini(&hid_state);

	return (rval);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * hid_info :
 *	Get minor number, soft state structure etc.
 */
/*ARGSUSED*/
static int
hid_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
			void *arg, void **result)
{
	hid_state_t	*hidp = NULL;
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((hidp = ddi_get_soft_state(hid_state,
		    getminor((dev_t)arg))) != NULL) {
			*result = hidp->hid_dip;
			error = DDI_SUCCESS;
		} else
			*result = NULL;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(uintptr_t)getminor((dev_t)arg);
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}

	return (error);
}


/*
 * hid_attach :
 *	Gets called at the time of attach. Do allocation,
 *	and initialization of the software structure.
 *	Get all the descriptors, setup the
 *	report descriptor tree by calling hidparser
 *	function.
 */
static int
hid_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{

	int			instance = ddi_get_instance(dip);
	hid_state_t		*hidp;
	mblk_t			*data;
	uint_t			config_no;	/* configuration number */
	uint_t			alternate = 0;
	int			interface;
	uchar_t 		*usb_config;	/* buf for config descriptor */
	size_t			usb_config_length; /* config des. length */
	int			parse_hid_descr_error = 0;
	uint32_t		usage_page;
	uint32_t		usage;
	char			minor_name[HID_MINOR_NAME_LEN];
	int			rval;

	switch (cmd) {
		case DDI_ATTACH:
			break;
		case DDI_RESUME:
			hidp = ddi_get_soft_state(hid_state, instance);

			return (hid_restore_device_state(dip, hidp) ==
				USB_SUCCESS ? DDI_SUCCESS : DDI_FAILURE);
		case DDI_PM_RESUME:
		default:
			return (DDI_FAILURE);
	}

	/*
	 * Allocate softc information.
	 */
	if (ddi_soft_state_zalloc(hid_state, instance) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	/*
	 * get soft state space and initialize
	 */
	hidp = (hid_state_t *)ddi_get_soft_state(hid_state, instance);
	if (hidp == NULL) {

		return (DDI_FAILURE);
	}

	hidp->hid_log_handle = usb_alloc_log_handle(dip, NULL, &hid_errlevel,
				&hid_errmask, &hid_instance_debug,
				&hid_show_label, 0);

	hidp->hid_attach_flags  = HID_ATTACH_INIT;

	/* initialize mutex */
	mutex_init(&hidp->hid_mutex, NULL, MUTEX_DRIVER, NULL);

	/* Initialize condition variable for default pipe state maintenance */
	cv_init(&hidp->hid_cv_default_pipe, NULL, CV_DRIVER, NULL);

	/* Initialize cv for interrupt pipe state maintenance */
	cv_init(&hidp->hid_cv_interrupt_pipe, NULL, CV_DRIVER, NULL);

	hidp->hid_instance = instance;
	hidp->hid_dip = dip;

	/* Obtain the raw configuration descriptor */
	usb_config = usb_get_raw_config_data(dip, &usb_config_length);

	/* Parse the configuration descriptor */
	if (usb_parse_configuration_descr(usb_config, usb_config_length,
		&hidp->hid_config_descr, USB_CONF_DESCR_SIZE)
		!= USB_CONF_DESCR_SIZE) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
		    "usb_parse_configuration_descr() failed");

		hid_detach_cleanup(hidp);

		return (DDI_FAILURE);
	}

	config_no = hidp->hid_config_descr.bConfigurationValue;
	hidp->hid_interfaceno = usb_get_interface_number(dip);

	USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
	    "Config = %d, interface = %d", config_no, hidp->hid_interfaceno);

	/*
	 * If interface == -1 then this instance is responsible for the
	 * entire device. use interface 0
	 */
	if (hidp->hid_interfaceno == USB_COMBINED_NODE) {
		interface = 0;
	} else {
		interface = hidp->hid_interfaceno;
	}

	/* Parse the interface descriptor */
	if (usb_parse_interface_descr(usb_config,
			usb_config_length,
			interface,		/* interface index */
			alternate,		/* alt interface index */
			&hidp->hid_interface_descr,
			USB_IF_DESCR_SIZE) != USB_IF_DESCR_SIZE) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
		    "usb_parse_interface_desc() failed");

		hid_detach_cleanup(hidp);

		return (DDI_FAILURE);
	}

	USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
	    "Interface descriptor:\n\t"
	    "l = 0x%x type = 0x%x n = 0x%x alt = 0x%x #ep = 0x%x\n\t"
	    "iclass = 0x%x\n\t"
	    "subclass = 0x%x protocol = 0x%x string index = 0x%x",
		hidp->hid_interface_descr.bLength,
		hidp->hid_interface_descr.bDescriptorType,
		hidp->hid_interface_descr.bInterfaceNumber,
		hidp->hid_interface_descr.bAlternateSetting,
		hidp->hid_interface_descr.bNumEndpoints,
		hidp->hid_interface_descr.bInterfaceClass,
		hidp->hid_interface_descr.bInterfaceSubClass,
		hidp->hid_interface_descr.bInterfaceProtocol,
		hidp->hid_interface_descr.iInterface);


	/*
	 * Attempt to parse the hid descriptor
	 */
	if (hid_parse_hid_descr(usb_config, usb_config_length,
			&hidp->hid_hid_descr, USB_HID_DESCR_SIZE,
			&hidp->hid_interface_descr)
			!= USB_HID_DESCR_SIZE) {

		/*
		 * If parsing of hid descriptor failed and
		 * the device is a keyboard or mouse, use predefined
		 * length and packet size.
		 */
		if (hid_parse_hid_descr_failure(hidp) == USB_FAILURE) {

			hid_detach_cleanup(hidp);

			return (DDI_FAILURE);
		}

		/*
		 * hid descriptor was bad but since
		 * the device is a keyboard or mouse,
		 * we will use the default length
		 * and packet size.
		 */
		parse_hid_descr_error = HID_BAD_DESCR;
	} else {

		/* Parse hid descriptor successful */

		USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
			"Hid descriptor:\n\t"
			"bLength = 0x%x bDescriptorType = 0x%x "
			"bcdHID = 0x%x\n\t"
			"bCountryCode = 0x%x bNumDescriptors = 0x%x\n\t"
			"bReportDescriptorType = 0x%x\n\t"
			"wReportDescriptorLength = 0x%x",
			hidp->hid_hid_descr.bLength,
			hidp->hid_hid_descr.bDescriptorType,
			hidp->hid_hid_descr.bcdHID,
			hidp->hid_hid_descr.bCountryCode,
			hidp->hid_hid_descr.bNumDescriptors,
			hidp->hid_hid_descr.bReportDescriptorType,
			hidp->hid_hid_descr.wReportDescriptorLength);
	}

	/*
	 * Parse the endpoint descriptor
	 */
	if (usb_parse_endpoint_descr(usb_config, usb_config_length,
			interface,	/* interface index */
			alternate,	/* alt interface index */
			0,		/* endpoint 1 */
			&hidp->hid_interrupt_ept_descr,
			USB_EPT_DESCR_SIZE) != USB_EPT_DESCR_SIZE) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
		    "usb_parse_endpoint_descr() failed");

		hid_detach_cleanup(hidp);

		return (DDI_FAILURE);
	}

	USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
	    "Endpoint descriptor:\n\t"
	    "l = 0x%x t = 0x%x add = 0x%x attr = 0x%x mps = %x int = 0x%x",
		hidp->hid_interrupt_ept_descr.bLength,
		hidp->hid_interrupt_ept_descr.bDescriptorType,
		hidp->hid_interrupt_ept_descr.bEndpointAddress,
		hidp->hid_interrupt_ept_descr.bmAttributes,
		hidp->hid_interrupt_ept_descr.wMaxPacketSize,
		hidp->hid_interrupt_ept_descr.bInterval);

	/*
	 * open up default pipe so we can get descriptors
	 */
	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_OPEN;

	if (usb_pipe_open(dip, NULL, NULL,
	    USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL,
	    &hidp->hid_default_pipe) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
			"hid_attach: Default pipe open failed");

		hid_detach_cleanup(hidp);

		return (DDI_FAILURE);
	}

	/*
	 * Don't get the report descriptor if parsing hid descriptor earlier
	 * failed since device probably won't return valid report descriptor
	 * either. Though parsing of hid descriptor failed, we have reached
	 * this point because the device has been identified as a
	 * keyboard or a mouse successfully and the default packet
	 * size and layout(in case of keyboard only) will be used, so it
	 * is ok to go ahead even if parsing of hid descriptor failed and
	 * we will not try to get the report descriptor.
	 */
	if (parse_hid_descr_error != HID_BAD_DESCR) {

		/*
		 * Get and parse the report descriptor.
		 * Set the packet size if parsing is successful.
		 */
		if (hid_handle_report_descriptor(hidp, interface, &data) ==
			USB_FAILURE) {

			hid_detach_cleanup(hidp);

			return (DDI_FAILURE);
		}
	}

	/*
	 * Make a clas specific request to SET_IDLE
	 * In this case send no reports if state has not changed.
	 * See HID 7.2.4.
	 */
	hid_set_idle(hidp);

	/*
	 * set to the boot protocol rather than item.  Boot protocol allows us
	 * to expect a predefined report structure
	 */
	hid_set_boot_protocol(hidp);

	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSE_PENDING;

	rval = usb_pipe_close(&hidp->hid_default_pipe,
		USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);

	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSED;

	/* now create components to power manage this device */
	hid_create_pm_components(dip, hidp);

	/*
	 * Create minor node based on information from the
	 * descriptors
	 */
	if (hidp->hid_interface_descr.bInterfaceProtocol ==
					    KEYBOARD_PROTOCOL) {
			(void) strcpy(minor_name, "keyboard");
	} else if (hidp->hid_interface_descr.bInterfaceProtocol ==
					MOUSE_PROTOCOL) {
			(void) strcpy(minor_name, "mouse");
	} else {
		if (hidparser_get_top_level_collection_usage(
			hidp->hid_report_descr, &usage_page, &usage)
			!= HIDPARSER_FAILURE) {
			(void) sprintf(minor_name, "hid_%x_%x",
					usage_page, usage);
		}
	}

	if ((ddi_create_minor_node(dip, minor_name, S_IFCHR, instance,
					NULL, 0)) != DDI_SUCCESS) {

		USB_DPRINTF_L1(PRINT_MASK_ATTA, hidp->hid_log_handle,
			"hid_attach: Could not create minor node");

		hid_detach_cleanup(hidp);

		return (DDI_FAILURE);
	}

	hidp->hid_attach_flags |= HID_MINOR_NODES;
	hidp->hid_dev_state = USB_DEV_ONLINE;

	/* register for connect and disconnect events */
	hid_register_events(hidp);

	/*
	 * report device
	 */
	ddi_report_dev(dip);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_attach: End");

	return (DDI_SUCCESS);
}


/*
 * hid_detach :
 *	Gets called at the time of detach.
 */
static int
hid_detach(dev_info_t *dip, ddi_detach_cmd_t	cmd)
{
	int instance = ddi_get_instance(dip);
	hid_state_t	*hidp;

	hidp = ddi_get_soft_state(hid_state, instance);

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle, "hid_detach");

	switch (cmd) {
	case DDI_DETACH:
		/*
		 * Undo	what we	did in client_attach, freeing resources
		 * and removing	things we installed.  The system
		 * framework guarantees	we are not active with this devinfo
		 * node	in any other entry points at this time.
		 */
		hid_detach_cleanup(hidp);

		return (DDI_SUCCESS);
	case DDI_SUSPEND:
		return (hid_cpr_suspend(hidp));
	default:
		break;
	}

	return (DDI_FAILURE);
}

/*
 * hid_cpr_suspend
 *	close intr pipe
 *	close control pipe
 *	cancel timeouts
 */
static int
hid_cpr_suspend(hid_state_t *hidp)
{
	int	old_state;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_cpr_suspend");

	mutex_enter(&hidp->hid_mutex);

	/*
	 * set this flag so that we don't kick off
	 * any async ops on open pipes
	 */
	old_state = hidp->hid_dev_state;
	hidp->hid_dev_state = USB_DEV_CPR_SUSPEND;

	/*
	 * if any async request are in progress - just fail the suspend
	 * because we cannot start and finish the thread at this stage
	 */
	if (((hidp->hid_interrupt_pipe_flags == HID_INTERRUPT_PIPE_CLOSED) ||
		(hidp->hid_interrupt_pipe_flags == HID_INTERRUPT_PIPE_OPEN)) &&
		((hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_CLOSED) ||
		(hidp->hid_default_pipe_flags ==  HID_DEFAULT_PIPE_OPEN))) {

		/* first close the interupt pipe */
		hid_close_intr_pipe(hidp);

		/* now close the default pipe */
		hid_close_default_pipe(hidp);

		/* cancel timeouts */
		hid_cancel_timeouts(hidp);

		mutex_exit(&hidp->hid_mutex);

		USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
			"hid_cpr_suspend : SUCCESS");

		return (DDI_SUCCESS);

	} else {

		hidp->hid_dev_state = old_state;
		mutex_exit(&hidp->hid_mutex);

		USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
			"hid_cpr_suspend : FAILURE");

		return (DDI_FAILURE);
	}
}


/*
 * hid_check_same_device()
 *	check if it is the same device and if not, warn user
 */
static int
hid_check_same_device(dev_info_t *dip, hid_state_t *hidp)
{
	char		*ptr;

	if (usb_check_same_device(dip) == USB_FAILURE) {
		if (ptr = usb_get_usbdev_strdescr(dip)) {
			USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
			    "Cannot access device. "
			    "Please reconnect %s", ptr);
		} else {
			USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
			    "Device is not identical to the "
			    "previous one on this port.\n"
			    "Please disconnect and reconnect");
		}

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/* Mark the device busy by setting hid_raise_power flag */
static void
hid_set_device_busy(hid_state_t *hidp)
{
	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_set_device_busy: hid_state : %p", hidp);

	mutex_enter(&hidp->hid_mutex);
	hidp->hid_pm->hid_raise_power = B_TRUE;
	mutex_exit(&hidp->hid_mutex);

	/* reset the timestamp for PM framework */
	hid_device_idle(hidp);
}


/*
 * hid_raise_device_power
 *	raises the power level of the device to the specified power level
 *
 */
static void
hid_raise_device_power(hid_state_t *hidp, int comp, int level)
{
	int rval;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_raise_device_power: hid_state : %p", hidp);

	if (hidp->hid_pm->hid_wakeup_enabled) {

		hid_set_device_busy(hidp);

		rval = pm_raise_power(hidp->hid_dip, comp, level);
		if (rval != DDI_SUCCESS) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"hid_raise_device_power: pm_raise_power "
				"returns : %d", rval);
		}

		mutex_enter(&hidp->hid_mutex);
		hidp->hid_pm->hid_raise_power = B_FALSE;
		mutex_exit(&hidp->hid_mutex);
	}
}


/*
 * hid_restore_device_state
 *	set original configuration of the device
 *	reopen intr pipe
 *	enable wrq - this starts new transactions on the control pipe
 */
static int
hid_restore_device_state(dev_info_t *dip, hid_state_t *hidp)
{
	int		rval;
	queue_t		*rdq, *wrq;
	hid_power_t	*hidpm;
	struct iocblk	mctlmsg;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_restore_device_state: hid_state : %p", hidp);

	mutex_enter(&hidp->hid_mutex);
	ASSERT((hidp->hid_dev_state == USB_DEV_DISCONNECTED) ||
		(hidp->hid_dev_state == USB_DEV_CPR_SUSPEND));

	hidpm = hidp->hid_pm;
	mutex_exit(&hidp->hid_mutex);

	/* First bring the device to full power */
	hid_raise_device_power(hidp, 0, USB_DEV_OS_FULL_POWER);

	/* Check if we are talking to the same device */
	if (hid_check_same_device(dip, hidp) == USB_FAILURE) {

		/* change the device state from suspended to disconnected */
		mutex_enter(&hidp->hid_mutex);
		hidp->hid_dev_state = USB_DEV_DISCONNECTED;
		mutex_exit(&hidp->hid_mutex);

		return (USB_SUCCESS);
	}

	mutex_enter(&hidp->hid_mutex);

	/* Send a connect event up */
	if ((hidp->hid_streams_flags == HID_STREAMS_OPEN) &&
		(hidp->hid_dev_state != USB_DEV_CPR_SUSPEND)) {

		USB_DPRINTF_L2(PRINT_MASK_EVENTS, hidp->hid_log_handle,
			"device is being re-connected");

		mutex_exit(&hidp->hid_mutex);

		/*
		 * Send an MCTL up indicating that
		 * a connect event is going to take place
		 */
		mctlmsg.ioc_cmd = HID_CONNECT_EVENT;
		mctlmsg.ioc_count = 0;

		(void) hid_mctl_send(hidp, mctlmsg, (char *)NULL, 0);

		mutex_enter(&hidp->hid_mutex);
	}


	/* open the default pipe */
	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_OPEN;
	mutex_exit(&hidp->hid_mutex);

	USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_restore_device_state: hid_state_t : %p", hidp);
	USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_restore_device_state: opening default pipe");

	if (usb_pipe_open(dip, NULL, NULL,
		USB_FLAGS_SLEEP | USB_FLAGS_OPEN_EXCL,
		&hidp->hid_default_pipe) != USB_SUCCESS) {

		USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
			"hid_restore_device_state:"
			"opening default pipe failed");

		mutex_enter(&hidp->hid_mutex);
		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSED;
		mutex_exit(&hidp->hid_mutex);

		return (USB_FAILURE);
	}

	hid_set_idle(hidp);

	hid_set_boot_protocol(hidp);

	/* Indicate that close is in progress for default pipe */
	mutex_enter(&hidp->hid_mutex);
	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSE_PENDING;
	mutex_exit(&hidp->hid_mutex);

	rval = usb_pipe_close(&hidp->hid_default_pipe,
		USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);

	/* Indicate that default pipe is in closed state */
	mutex_enter(&hidp->hid_mutex);
	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSED;
	ASSERT(hidp->hid_default_pipe == NULL);

	/* if the device had remote wakeup earlier, enable it again */
	if (hidpm->hid_wakeup_enabled) {
		mutex_exit(&hidp->hid_mutex);

		rval = usb_enable_remote_wakeup(hidp->hid_dip);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(&hidp->hid_mutex);
	}

	/*
	 * reopen the interrupt pipe only if the device
	 * was previously operational
	 */
	if (hidp->hid_streams_flags == HID_STREAMS_OPEN) {

		rdq = hidp->hid_rq_ptr;
		wrq = hidp->hid_wq_ptr;
		hidp->hid_interrupt_pipe_flags = HID_INTERRUPT_PIPE_OPEN;
		hidp->hid_interrupt_pipe = NULL;
		mutex_exit(&hidp->hid_mutex);

		rval = usb_pipe_open(hidp->hid_dip,
			&hidp->hid_interrupt_ept_descr,
			&hidp->hid_intr_pipe_policy, NULL,
			&hidp->hid_interrupt_pipe);

		if (rval != USB_SUCCESS) {
			USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"hid_restore_device_state:"
				"reopen intr pipe failed" " rval = %d ", rval);
			mutex_enter(&hidp->hid_mutex);
			hidp->hid_interrupt_pipe_flags =
				HID_INTERRUPT_PIPE_CLOSED;
			mutex_exit(&hidp->hid_mutex);

			return (USB_FAILURE);
		}

		rval = usb_pipe_start_polling(hidp->hid_interrupt_pipe,
			USB_FLAGS_SHORT_XFER_OK);

		if (rval != USB_SUCCESS) {
			USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"hid_restore_device_state:"
				"start poll intr pipe failed"
				" rval = %d ", rval);

			return (USB_FAILURE);
		}

		/* set the device state ONLINE */
		mutex_enter(&hidp->hid_mutex);
		hidp->hid_dev_state = USB_DEV_ONLINE;

		qenable(rdq);
		qenable(wrq);
	}

	mutex_exit(&hidp->hid_mutex);

	return (DDI_SUCCESS);
}


/*
 * hid_detach_cleanup:
 *	called by attach and detach for cleanup.
 */
static void
hid_detach_cleanup(hid_state_t *hidp)
{
	int	flags = hidp->hid_attach_flags;
	int	rval;
	hid_power_t	*hidpm;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_detach_cleanup: Begin");

	hidpm = hidp->hid_pm;

	USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_detach_cleanup: hidpm : %p", hidpm);

	if (hidpm) {
		kmem_free(hidpm, sizeof (hid_power_t));
		hidp->hid_pm = NULL;
	}

	if (hidp->hid_report_descr != NULL) {
		(void) hidparser_free_report_descriptor_handle(
			hidp->hid_report_descr);
	}

	if (hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_OPEN) {

		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSE_PENDING;

		rval =  usb_pipe_close(&hidp->hid_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL);
		ASSERT(rval == USB_SUCCESS);

		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSED;
	}

	if (flags & HID_MINOR_NODES) {
		ddi_remove_minor_node(hidp->hid_dip, NULL);
	}

	hid_deregister_events(hidp);

	ddi_prop_remove_all(hidp->hid_dip);

	/* Destroy the condition variable for default pipe state maintenance */
	cv_destroy(&hidp->hid_cv_default_pipe);

	/* Destroy the cv for interrupt pipe state maintenance */
	cv_destroy(&hidp->hid_cv_interrupt_pipe);

	mutex_destroy(&hidp->hid_mutex);

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_detach_cleanup: End");

	usb_free_log_handle(hidp->hid_log_handle);

	ddi_soft_state_free(hid_state, hidp->hid_instance);
}


/*
 * event handling
 *
 * hid_connect_event_callback():
 *	the device was disconnected but this instance not detached, probably
 *	because the device was busy
 *
 *	if the same device, continue with restoring state
 */
static int
hid_connect_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	hid_state_t	*hidp = (hid_state_t *)ddi_get_soft_state(hid_state,
				ddi_get_instance(dip));

	ASSERT(hidp != NULL);

	USB_DPRINTF_L3(PRINT_MASK_EVENTS, hidp->hid_log_handle,
	    "hid_connect_event_callback: dip=0x%p, cookie=0x%x, "
	    "arg=0x%p, impl=0x%p",
	    (void *)dip, cookie, arg, bus_impldata);

	(void) hid_restore_device_state(dip, hidp);

	return (DDI_EVENT_CLAIMED);
}


/*
 * hid_disconnect_event_callback()
 *	the device has been disconnected. we either wait for
 *	detach or a reconnect event. Close all pipes and timeouts
 */
static int
hid_disconnect_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	struct iocblk   mctlmsg;
	hid_state_t  *hidp =
		(hid_state_t *)ddi_get_soft_state(hid_state,
						ddi_get_instance(dip));

	ASSERT(hidp != NULL);

	USB_DPRINTF_L3(PRINT_MASK_EVENTS, hidp->hid_log_handle,
	    "hid_disconnect_event_callback: dip=0x%p, cookie=0x%x, "
	    "arg=0x%p, impl=0x%p",
	    (void *)dip, cookie, arg, bus_impldata);

	mutex_enter(&hidp->hid_mutex);

	hidp->hid_dev_state = USB_DEV_DISCONNECTED;

	if (hidp->hid_streams_flags == HID_STREAMS_OPEN) {

		USB_DPRINTF_L2(PRINT_MASK_EVENTS, hidp->hid_log_handle,
			"busy device has been disconnected");

		mutex_exit(&hidp->hid_mutex);

		/*
		 * Send an MCTL up indicating that
		 * an hotplug is going to take place
		 */
		mctlmsg.ioc_cmd = HID_DISCONNECT_EVENT;
		mctlmsg.ioc_count = 0;

		(void) hid_mctl_send(hidp, mctlmsg, (char *)NULL, 0);

		mutex_enter(&hidp->hid_mutex);

		/* first close the interupt pipe */
		hid_close_intr_pipe(hidp);

		/* now close the default pipe */
		hid_close_default_pipe(hidp);

		/* cancel timeouts */
		hid_cancel_timeouts(hidp);

	}

	mutex_exit(&hidp->hid_mutex);

	return (DDI_EVENT_CLAIMED);
}

/*
 * register and deregister for events
 */
static void
hid_register_events(hid_state_t *hidp)
{
	int rval;
	ddi_plevel_t level;
	ddi_iblock_cookie_t icookie;

	/* get event cookie, discard level and icookie for now */
	rval = ddi_get_eventcookie(hidp->hid_dip, DDI_DEVI_REMOVE_EVENT,
		&hidp->hid_remove_cookie, &level, &icookie);

	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(hidp->hid_dip,
			hidp->hid_remove_cookie,
			hid_disconnect_event_callback, NULL);

		ASSERT(rval == DDI_SUCCESS);
	}

	rval = ddi_get_eventcookie(hidp->hid_dip, DDI_DEVI_INSERT_EVENT,
		&hidp->hid_insert_cookie, &level, &icookie);
	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(hidp->hid_dip,
			hidp->hid_insert_cookie,
			hid_connect_event_callback, NULL);

		ASSERT(rval == DDI_SUCCESS);
	}
}


static void
hid_deregister_events(hid_state_t *hidp)
{
	int rval;

	if (hidp->hid_remove_cookie) {
		rval = ddi_remove_eventcall(hidp->hid_dip,
					hidp->hid_remove_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}

	if (hidp->hid_insert_cookie) {
		rval = ddi_remove_eventcall(hidp->hid_dip,
					hidp->hid_insert_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/* mark the device as idle */
static void
hid_device_idle(hid_state_t *hidp)
{
	int		rval;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_device_idle: hid_state : %p", hidp);

	if (hidp->hid_pm->hid_wakeup_enabled) {
		rval = pm_idle_component(hidp->hid_dip, 0);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * hid_power_change_callback
 * async callback function to notify pm_raise_power completion
 * after hid_power entry point is called
 */
static void
hid_power_change_callback(void *arg, int rval)
{
	hid_state_t	*hidp;
	queue_t		*wq;

	hidp = (hid_state_t *)arg;

	USB_DPRINTF_L4(PRINT_MASK_EVENTS, hidp->hid_log_handle,
		"hid_power_change_callback - rval : %d", rval);

	mutex_enter(&hidp->hid_mutex);
	hidp->hid_pm->hid_raise_power = B_FALSE;

	if (hidp->hid_dev_state == USB_DEV_ONLINE) {
		wq = hidp->hid_wq_ptr;
		mutex_exit(&hidp->hid_mutex);

		qenable(wq);

	} else {
		mutex_exit(&hidp->hid_mutex);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * functions to handle power transition for various levels
 * These functions act as place holders to issue USB commands
 * to the devices to change their power levels
 */

static int
hid_pwrlvl0(hid_state_t *hidp)
{
	hid_power_t	*hidpm;
	int		rval;
	struct iocblk   mctlmsg;

	hidpm = hidp->hid_pm;

	switch (hidp->hid_dev_state) {
	case USB_DEV_ONLINE:

		/*
		 * if the device is open, we need to STOP polling
		 * on the intr pipe
		 */
		if ((hidp->hid_streams_flags == HID_STREAMS_OPEN) &&
			(hidp->hid_interrupt_pipe != NULL)) {

			mutex_exit(&hidp->hid_mutex);

			rval = usb_pipe_stop_polling(hidp->hid_interrupt_pipe,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);

			mutex_enter(&hidp->hid_mutex);
		}

		/* Issue USB D3 command to the device here */
		rval = usb_set_device_pwrlvl3(hidp->hid_dip);
		ASSERT(rval == USB_SUCCESS);

		if (hidp->hid_streams_flags == HID_STREAMS_OPEN) {

			mutex_exit(&hidp->hid_mutex);

			/*
			 * Send an MCTL up indicating that
			 * a power off event is going to take place
			 */
			mctlmsg.ioc_cmd = HID_POWER_OFF;
			mctlmsg.ioc_count = 0;

			(void) hid_mctl_send(hidp, mctlmsg, (char *)NULL, 0);

			mutex_enter(&hidp->hid_mutex);
		}


		hidp->hid_dev_state = USB_DEV_POWERED_DOWN;
		hidpm->hid_current_power = USB_DEV_OS_POWER_OFF;

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
hid_pwrlvl1(hid_state_t *hidp)
{
	int		rval;

	/* Issue USB D2 command to the device here */
	rval = usb_set_device_pwrlvl2(hidp->hid_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


/* ARGSUSED */
static int
hid_pwrlvl2(hid_state_t *hidp)
{
	int		rval;

	rval = usb_set_device_pwrlvl1(hidp->hid_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


static int
hid_pwrlvl3(hid_state_t *hidp)
{
	hid_power_t	*hidpm;
	int		rval;
	struct iocblk   mctlmsg;

	hidpm = hidp->hid_pm;

	/* Issue USB D0 command to the device here */
	rval = usb_set_device_pwrlvl0(hidp->hid_dip);
	ASSERT(rval == USB_SUCCESS);

	if (hidp->hid_streams_flags == HID_STREAMS_OPEN) {

		mutex_exit(&hidp->hid_mutex);

		/*
		 * Send an MCTL up indicating that
		 * a full power event going to take place
		 */
		mctlmsg.ioc_cmd = HID_FULL_POWER;
		mctlmsg.ioc_count = 0;

		(void) hid_mctl_send(hidp, mctlmsg, (char *)NULL, 0);

		mutex_enter(&hidp->hid_mutex);
	}

	/* if the device is open, we need to START polling on the intr pipe */
	if ((hidp->hid_streams_flags == HID_STREAMS_OPEN) &&
		(hidp->hid_interrupt_pipe != NULL)) {

		mutex_exit(&hidp->hid_mutex);

		rval = usb_pipe_start_polling(hidp->hid_interrupt_pipe,
			USB_FLAGS_SLEEP | USB_FLAGS_SHORT_XFER_OK);
		ASSERT(rval == USB_SUCCESS);

		mutex_enter(&hidp->hid_mutex);
	}

	hidp->hid_dev_state = USB_DEV_ONLINE;
	hidpm->hid_current_power = USB_DEV_OS_FULL_POWER;

	return (DDI_SUCCESS);
}


/* power entry point */
/* ARGSUSED */
static int
hid_power(dev_info_t *dip, int comp, int level)
{
	int 		instance = ddi_get_instance(dip);
	hid_state_t	*hidp;
	hid_power_t	*hidpm;
	int		retval;

	hidp = ddi_get_soft_state(hid_state, instance);

	USB_DPRINTF_L3(PRINT_MASK_PM, hidp->hid_log_handle, "hid_power:"
		" hid_state : %p comp : %d level : %d", hidp, comp, level);

	/* check if we are transitioning to a legal power level */
	mutex_enter(&hidp->hid_mutex);
	hidpm = hidp->hid_pm;

	if (USB_DEV_PWRSTATE_OK(hidpm->hid_pwr_states, level)) {

		USB_DPRINTF_L2(PRINT_MASK_PM, hidp->hid_log_handle,
			"hid_power: illegal level : %d hid_pwr_states : %d",
			level, hidpm->hid_pwr_states);

		mutex_exit(&hidp->hid_mutex);

		return (DDI_FAILURE);
	}

	/*
	 * If we are about to raise power and we get this call to lower
	 * power, we return failure
	 */
	if ((hidpm->hid_raise_power == B_TRUE) &&
		(level < (int)hidpm->hid_current_power)) {

		mutex_exit(&hidp->hid_mutex);

		return (DDI_FAILURE);
	}

	switch (level) {
	case USB_DEV_OS_POWER_OFF:
		retval = hid_pwrlvl0(hidp);
		break;
	case USB_DEV_OS_POWER_1:
		retval = hid_pwrlvl1(hidp);
		break;
	case USB_DEV_OS_POWER_2:
		retval = hid_pwrlvl2(hidp);
		break;
	case USB_DEV_OS_FULL_POWER:
		retval = hid_pwrlvl3(hidp);
		break;
	}

	mutex_exit(&hidp->hid_mutex);

	return (retval);
}


/* create the pm components required for power management */
static void
hid_create_pm_components(dev_info_t *dip, hid_state_t *hidp)
{
	hid_power_t	*hidpm;

	USB_DPRINTF_L4(PRINT_MASK_PM, hidp->hid_log_handle,
		"hid_create_pm_components : Begin");

	/* Allocate the state structure */
	hidpm = kmem_zalloc(sizeof (hid_power_t), KM_SLEEP);
	hidp->hid_pm = hidpm;
	hidpm->hid_state = hidp;
	hidpm->hid_raise_power = B_FALSE;
	hidpm->hid_pm_capabilities = 0;
	hidpm->hid_current_power = USB_DEV_OS_FULL_POWER; /* full power */

	if ((usb_is_pm_enabled(dip) == USB_SUCCESS) &&
	    (usb_enable_remote_wakeup(dip) == USB_SUCCESS)) {
		uint_t		pwr_states;

		USB_DPRINTF_L2(PRINT_MASK_PM, hidp->hid_log_handle,
		    "hid_create_pm_components: Remote Wakeup Enabled");

		if (usb_create_pm_components(dip, &pwr_states) ==
		    USB_SUCCESS) {
			hidpm->hid_wakeup_enabled = 1;
			hidpm->hid_pwr_states = (uint8_t)pwr_states;
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_PM, hidp->hid_log_handle,
		"hid_create_pm_components : END");
}


/*
 * hid_open :
 *	Open entry point: allocates the pipe policy
 *	structures and initializes. Opens the interrupt
 *	pipe. Sets up queues.
 */
/*ARGSUSED*/
static int
hid_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	usb_pipe_policy_t *policy;
	int no_of_ep = 0;
	int rval;
	hid_state_t *hidp =
		ddi_get_soft_state(hid_state, getminor(*devp));

	if (hidp == NULL) {

		return (ENXIO);
	}

	USB_DPRINTF_L4(PRINT_MASK_OPEN, hidp->hid_log_handle,
		"hid_open: Begin");

	if (sflag) {
		/* clone open NOT supported here */

		return (ENXIO);
	}

	USB_DPRINTF_L3(PRINT_MASK_OPEN, hidp->hid_log_handle,
		"hid_state= %p\n", hidp);

	mutex_enter(&hidp->hid_mutex);

	/* fail open on a disconnected device */
	if (hidp->hid_dev_state == USB_DEV_DISCONNECTED) {
		mutex_exit(&hidp->hid_mutex);

		return (ENODEV);
	}

	/*
	 * Exit if this instance is already open
	 */
	if (q->q_ptr || (hidp->hid_streams_flags == HID_STREAMS_OPEN)) {
		mutex_exit(&hidp->hid_mutex);

		return (0);
	}

	if (hidp->hid_dev_state == USB_DEV_POWERED_DOWN) {
		USB_DPRINTF_L2(PRINT_MASK_PM, hidp->hid_log_handle,
			"get the device full powered");

		mutex_exit(&hidp->hid_mutex);
		hid_raise_device_power(hidp, 0, USB_DEV_OS_FULL_POWER);
		mutex_enter(&hidp->hid_mutex);
	}

	/* Intitialize the queue pointers */
	q->q_ptr = hidp;
	WR(q)->q_ptr = hidp;

	hidp->hid_rq_ptr = q;
	hidp->hid_wq_ptr = WR(q);

	/*
	 * Initialize the pipe policy for the interrupt pipe
	 */
	policy = &hidp->hid_intr_pipe_policy;
	policy->pp_version = USB_PIPE_POLICY_V_0;

	if (flag & FREAD) {
		policy->pp_callback_arg = (usb_opaque_t)hidp;
		policy->pp_callback = hid_interrupt_pipe_callback;
		policy->pp_exception_callback =
			hid_interrupt_pipe_exception_callback;
		policy->pp_periodic_max_transfer_size =
		    hidp->hid_packet_size;
		hidp->hid_interrupt_pipe_flags = HID_INTERRUPT_PIPE_OPEN;

		hidp->hid_interrupt_pipe = NULL;

		no_of_ep = hidp->hid_interface_descr.bNumEndpoints;

		mutex_exit(&hidp->hid_mutex);

		/* Check if interrupt endpoint exists */
		if (no_of_ep > 0) {

			/* Open the interrupt pipe */
			if (USB_SUCCESS != usb_pipe_open(hidp->hid_dip,
						&hidp->hid_interrupt_ept_descr,
						policy, NULL,
						&hidp->hid_interrupt_pipe)) {

				mutex_enter(&hidp->hid_mutex);
				hidp->hid_interrupt_pipe_flags =
					HID_INTERRUPT_PIPE_CLOSED;
				ASSERT(hidp->hid_interrupt_pipe == NULL);
				mutex_exit(&hidp->hid_mutex);

				return (EIO);
			}
		}
	} else {

		/* NOT FREAD */
		mutex_exit(&hidp->hid_mutex);

		return (EIO);
	}

	mutex_enter(&hidp->hid_mutex);

	/*
	 * Intitialize the pipe policy for the default pipe
	 */
	hidp->hid_default_pipe_policy.pp_version = USB_PIPE_POLICY_V_0;

	hidp->hid_default_pipe_policy.pp_callback_arg = NULL;
	hidp->hid_default_pipe_policy.pp_callback = hid_default_pipe_callback;
	hidp->hid_default_pipe_policy.pp_exception_callback =
					hid_default_pipe_exception_callback;
	hidp->hid_default_pipe_policy.pp_periodic_max_transfer_size =
					hidp->hid_packet_size;
	hidp->hid_streams_flags = HID_STREAMS_OPEN;

	mutex_exit(&hidp->hid_mutex);

	qprocson(q);

	/*
	 * strhead is set up now, so interrupt callback can
	 * put messages up. Start polling on the interrupt pipe.
	 */
	mutex_enter(&hidp->hid_mutex);

	/* If interrupt pipe doesn't exist, don't start polling */
	if (!hidp->hid_interrupt_pipe) {

		mutex_exit(&hidp->hid_mutex);

	} else {

		mutex_exit(&hidp->hid_mutex);

		if (usb_pipe_start_polling(hidp->hid_interrupt_pipe,
			USB_FLAGS_SHORT_XFER_OK) != USB_SUCCESS) {
			USB_DPRINTF_L2(PRINT_MASK_OPEN,
			    hidp->hid_log_handle, "Start polling failed");

			mutex_enter(&hidp->hid_mutex);
			hidp->hid_interrupt_pipe_flags =
						HID_INTERRUPT_PIPE_CLOSED;
			hidp->hid_streams_flags = HID_STREAMS_DISMANTLING;
			mutex_exit(&hidp->hid_mutex);

			rval = usb_pipe_close(&hidp->hid_interrupt_pipe,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);

			qprocsoff(q);

			return (EIO);
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_OPEN, hidp->hid_log_handle, "hid_open: End");

	return (0);
}

/*
 * hid_close_intr_pipe:
 *	close the interrupt pipe after drainig all callbacks
 */
static void
hid_close_intr_pipe(hid_state_t *hidp)
{
	clock_t		lb;

	USB_DPRINTF_L4(PRINT_MASK_CLOSE, hidp->hid_log_handle,
		"hid_close_intr_pipe : Begin");

	if (hidp->hid_interrupt_pipe) {
		/*
		 * If a callback is pending, wait for it to finish
		 */
		if ((hidp->hid_interrupt_pipe_flags ==
			HID_INTERRUPT_PIPE_DATA_TRANSFERRING) ||
			(hidp->hid_interrupt_pipe_flags ==
			HID_INTERRUPT_PIPE_RESET_PENDING)) {

			lb = ddi_get_lbolt();
			(void) cv_timedwait(&hidp->hid_cv_interrupt_pipe,
				&hidp->hid_mutex,
				(lb + HID_CLOSE_WAIT_TIMEOUT));
		}

		ASSERT(hidp->hid_interrupt_pipe_flags ==
						HID_INTERRUPT_PIPE_OPEN);

		hidp->hid_interrupt_pipe_flags = HID_INTERRUPT_PIPE_CLOSED;

		mutex_exit(&hidp->hid_mutex);

		/* Close the interrupt pipe */
		while (usb_pipe_close(&hidp->hid_interrupt_pipe,
			USB_FLAGS_SLEEP, NULL, NULL) != USB_SUCCESS) {
			delay(1);
		}

		mutex_enter(&hidp->hid_mutex);

		ASSERT(hidp->hid_interrupt_pipe == NULL);
	}
}

/*
 * hid_close_default_pipe:
 *	close the default pipe after drainig all callbacks
 */
static void
hid_close_default_pipe(hid_state_t *hidp)
{
	clock_t		lb;
	int		ret = 0;

	/*
	 * If the default pipe is closed, then we're done.
	 * else wait until the asynchronous close
	 * callback is called.
	 *
	 * Sometimes devices NAK forever in which case there will
	 * never be a callback.  Set a timedwait and if there is
	 * no response after a certain period of time, close the pipe.
	 */
	lb = ddi_get_lbolt();
	if (hidp->hid_default_pipe_flags != HID_DEFAULT_PIPE_CLOSED) {
		USB_DPRINTF_L4(PRINT_MASK_CLOSE, hidp->hid_log_handle,
			"hid_close_default_pipe: hid_default_pipe_flags = %d ",
			hidp->hid_default_pipe_flags);
		ret = cv_timedwait(&hidp->hid_cv_default_pipe,
				&hidp->hid_mutex,
				(lb + HID_CLOSE_WAIT_TIMEOUT));
	}

	/*
	 * Normally at this point, pipe should be closed. But
	 * if a device continuously NAKs, we need to close the pipe
	 * after a certain period.
	 */
	if (ret == -1) {

		ASSERT(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_OPEN);

		/* Indicate that close is in progress for default pipe */

		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSE_PENDING;

		mutex_exit(&hidp->hid_mutex);

		while (usb_pipe_close(&hidp->hid_default_pipe,
			USB_FLAGS_SLEEP, NULL, NULL) != USB_SUCCESS) {
			delay(1);
		}

		mutex_enter(&hidp->hid_mutex);

		/*
		 * Make sure that nobody changed the state
		 * while we dropped the lock
		 */
		ASSERT(hidp->hid_default_pipe_flags ==
						HID_DEFAULT_PIPE_CLOSE_PENDING);

		/* Indicate that default pipe is in closed state */
		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSED;
		ASSERT(hidp->hid_default_pipe == NULL);
	} else {
		ASSERT(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_CLOSED);
	}
}

/*
 * hid_cancel_timeouts:
 */
static void
hid_cancel_timeouts(hid_state_t *hidp)
{
	timeout_id_t	timeout_id;
	queue_t		*wrq;

	if (hidp->hid_timeout_id) {

		timeout_id = hidp->hid_timeout_id;
		wrq = hidp->hid_wq_ptr;

		/*
		 * We need to release mutex because hid_timeout()
		 * might be executing now and will be waiting for
		 * hid_mutex and quntimeout(called next) does
		 * a cv_wait infinitely if it finds that already
		 * a timeout routine is executing and this causes
		 * a deadlock.
		 */
		mutex_exit(&hidp->hid_mutex);

		/*
		 * By the time we dropped mutex and executing
		 * this quntimeout(), hid_timeout() might have
		 * been called and this call might be redundant; but
		 * there is no side effect.
		 */
		(void) quntimeout(wrq, timeout_id);

		mutex_enter(&hidp->hid_mutex);

		hidp->hid_timeout_id = 0;
	}
}

/*
 * hid_close :
 *	Close entry point.
 */
/*ARGSUSED*/
static int
hid_close(queue_t *q, int flag, cred_t *credp)
{
	hid_state_t	*hidp = q->q_ptr;

	USB_DPRINTF_L4(PRINT_MASK_CLOSE, hidp->hid_log_handle, "hid_close:");

	mutex_enter(&hidp->hid_mutex);

	hidp->hid_streams_flags = HID_STREAMS_DISMANTLING;

	/*
	 * Make sure that pipes are closed only once
	 */
	if (hidp->hid_dev_state != USB_DEV_DISCONNECTED) {

		hid_close_intr_pipe(hidp);

		hid_close_default_pipe(hidp);

		hid_cancel_timeouts(hidp);


	} else {

		while (hidp->hid_default_pipe || hidp->hid_interrupt_pipe ||
			hidp->hid_timeout_id) {

			mutex_exit(&hidp->hid_mutex);
			delay(1);
			mutex_enter(&hidp->hid_mutex);

		}
	}

	mutex_exit(&hidp->hid_mutex);

	qprocsoff(q);

	q->q_ptr = NULL;

	USB_DPRINTF_L4(PRINT_MASK_CLOSE, hidp->hid_log_handle,
		"hid_close: End");

	return (0);

}


/*
 * hid_wput :
 *	write put routine for the hid module
 */
static int
hid_wput(queue_t *q, mblk_t *mp)
{
	int error = HID_SUCCESS;
	int		rval;
	struct iocblk	*iocp;
	hid_state_t 	*hidp = (hid_state_t *)q->q_ptr;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_wput: Begin");

	/* See if the upper module is passing the right thing */
	ASSERT(mp != NULL);
	ASSERT(mp->b_datap != NULL);

	switch (mp->b_datap->db_type) {
	case M_FLUSH:  /* Canonical flush handling */
		if (*mp->b_rptr & FLUSHW) {
			flushq(q, FLUSHDATA);
		}

		/* read queue not used so just send up */
		if (*mp->b_rptr & FLUSHR) {
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
		} else {
			freemsg(mp);
		}
		break;
	case M_IOCTL:
		error = hid_ioctl(q, mp);
		break;
	case M_CTL:
		/*
		 * polled input commands are not dependent on default pipe
		 * state or any other outstanding M_CTL. We process them
		 * right away
		 */
		iocp = (struct iocblk *)mp->b_rptr;
		if ((iocp->ioc_cmd == HID_OPEN_POLLED_INPUT) ||
			(iocp->ioc_cmd == HID_CLOSE_POLLED_INPUT)) {

			error = hid_mctl_receive(q, mp);
			break;
		}

		/*
		 * get the device full powered. We get a callback
		 * which enables the WQ and kicks off IO
		 */
		mutex_enter(&hidp->hid_mutex);
		if (hidp->hid_dev_state == USB_DEV_POWERED_DOWN) {

			hidp->hid_dev_state = USB_DEV_HID_POWER_CHANGE;
			mutex_exit(&hidp->hid_mutex);

			hid_set_device_busy(hidp);

			rval = usb_request_raise_power(hidp->hid_dip,
				0, USB_DEV_OS_FULL_POWER,
				hid_power_change_callback, hidp, 0);
			ASSERT(rval == USB_SUCCESS);

			(void) putq(q, mp);

			break;
		} else if (hidp->hid_dev_state == USB_DEV_HID_POWER_CHANGE) {
			mutex_exit(&hidp->hid_mutex);
			(void) putq(q, mp);

			break;
		}

		mutex_exit(&hidp->hid_mutex);

		/*
		 * If there are messages already on the service
		 * queue, put this message on the queue as well
		 * to preserve message ordering.
		 */
		if (q->q_first) {
			(void) putq(q, mp);
		} else {
			mutex_enter(&hidp->hid_mutex);

			/*
			 * If the default pipe is closed, it's not being
			 * used, the device is still connected and
			 * streams aren't closing down in hid_close(),
			 * send the message directly down the pipe.
			 * If the default pipe is in use, put the
			 * message on the service queue.
			 */
			if ((hidp->hid_dev_state == USB_DEV_ONLINE) &&
				(hidp->hid_default_pipe_flags ==
				HID_DEFAULT_PIPE_CLOSED) &&
				(hidp->hid_streams_flags !=
				HID_STREAMS_DISMANTLING)) {

					mutex_exit(&hidp->hid_mutex);
					/* Send a message down */
					error = hid_mctl_receive(q, mp);
			} else {
				mutex_exit(&hidp->hid_mutex);
				(void) putq(q, mp);
			}
		}
		break;
	default:
		error = HID_FAILURE;
	}

	if (error == HID_FAILURE) {
		if (!canputnext(RD(q))) {
			freemsg(mp);
		} else {
			/*
			 * Pass an error message up.
			 */
			mp->b_datap->db_type = M_ERROR;
			if (mp->b_cont) {
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
			}
			mp->b_rptr = mp->b_datap->db_base;
			mp->b_wptr = mp->b_rptr + sizeof (char);
			*mp->b_rptr = EINVAL;

			qreply(q, mp);
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_wput: End");

	return (HID_SUCCESS);
}

/*
 * hid_wsrv :
 *	Write service routine for hid. When a message arrives through
 *	hid_wput(), it is kept in write queue to be serviced later.
 */
static int
hid_wsrv(queue_t *q)
{
	int		rval;
	mblk_t 		*mp;
	hid_state_t 	*hidp = (hid_state_t *)q->q_ptr;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_wsrv: Begin");

	mutex_enter(&hidp->hid_mutex);

	if (q->q_first == NULL) {
		mutex_exit(&hidp->hid_mutex);

		return (HID_SUCCESS);
	}

	/*
	 * get the device full powered. We get a callback
	 * which enables the WQ and kicks off IO
	 */
	if (hidp->hid_dev_state == USB_DEV_POWERED_DOWN) {

		hidp->hid_dev_state = USB_DEV_HID_POWER_CHANGE;
		mutex_exit(&hidp->hid_mutex);

		hid_set_device_busy(hidp);

		rval = usb_request_raise_power(hidp->hid_dip,
			0,  USB_DEV_OS_FULL_POWER, hid_power_change_callback,
			hidp, 0);
		ASSERT(rval == USB_SUCCESS);

		return (HID_SUCCESS);

	} else if (hidp->hid_dev_state == USB_DEV_HID_POWER_CHANGE) {

		mutex_exit(&hidp->hid_mutex);

		return (HID_SUCCESS);
	}

	/*
	 * continue servicing all the M_CTL's till the queue is empty
	 * or the device gets disconnected or hid_close() not in progress
	 */
	while ((hidp->hid_dev_state == USB_DEV_ONLINE) &&
		(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_CLOSED) &&
		(hidp->hid_streams_flags != HID_STREAMS_DISMANTLING)) {
		if ((mp = getq(q)) != NULL) {

			mutex_exit(&hidp->hid_mutex);

			/* Send a message down */
			if (hid_mctl_receive(q, mp) != HID_SUCCESS) {

				if (!canputnext(RD(q))) {
					freemsg(mp);
				} else {
					mp->b_datap->db_type = M_ERROR;
					if (mp->b_cont) {
						freemsg(mp->b_cont);
						mp->b_cont = NULL;
					}
					mp->b_rptr = mp->b_datap->db_base;
					mp->b_wptr = mp->b_rptr + sizeof (char);
					*mp->b_rptr = EIO;
					putnext(RD(q), mp);
				}
			}

			mutex_enter(&hidp->hid_mutex);
		} else {
			/*
			 * No more message in the queue
			 */
			break;
		}
	}

	mutex_exit(&hidp->hid_mutex);

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_wsrv: End");

	return (HID_SUCCESS);
}


/*
 * hid_mctl_receive:
 * 	Handle M_CTL messages from upper stream.  If
 * 	we don't understand the command, free message.
 */
static int
hid_mctl_receive(register queue_t *q, register mblk_t *mp)
{
	hid_state_t 	*hidd = (hid_state_t *)q->q_ptr;
	struct iocblk	*iocp, mctlmsg;
	hid_req_t	*hid_req_data = NULL;
	uchar_t		request_type;
	int		request_index;
	hid_default_pipe_arg_t	*hid_default_pipe_arg;
	hid_polled_input_callback_t hid_polled_input;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidd->hid_log_handle,
		"hid_mctl_receive");

	iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
		case HID_GET_REPORT:
			/* FALLTHRU */
		case HID_GET_IDLE:
			/* FALLTHRU */
		case HID_GET_PROTOCOL:
			/*
			 * These commands require a hid_req_t.  Make sure
			 * one is present.
			 */
			if (mp->b_cont == NULL) {

				return (HID_FAILURE);
			} else {
				hid_req_data = (hid_req_t *)mp->b_cont->b_rptr;
			}

			/*
			 * Check is version no. is correct. This
			 * is coming from the user
			 */
			if (hid_req_data->hid_req_version_no !=
						HID_VERSION_V_0) {

				return (HID_FAILURE);
			}

			/*
			 * Partially initialize the control request to
			 * be sent to USBA.
			 */
			request_type = USB_DEV_REQ_DEVICE_TO_HOST |
					USB_DEV_REQ_RECIPIENT_INTERFACE |
					USB_DEV_REQ_TYPE_CLASS;

			mutex_enter(&hidd->hid_mutex);
			request_index =
				hidd->hid_interface_descr.bInterfaceNumber;
			mutex_exit(&hidd->hid_mutex);

			/*
			 * Set up the argument to be passed back to hid
			 * when the asynchronous control callback is
			 * executed.
			 */
			hid_default_pipe_arg = kmem_zalloc(
				sizeof (hid_default_pipe_arg_t), KM_SLEEP);

			hid_default_pipe_arg->
				hid_default_pipe_arg_mctlmsg.ioc_cmd =
					iocp->ioc_cmd;

			hid_default_pipe_arg->
				hid_default_pipe_arg_mctlmsg.ioc_count =
					iocp->ioc_count;

			hid_default_pipe_arg->hid_default_pipe_arg_hidp = hidd;
			hid_default_pipe_arg->hid_default_pipe_arg_mblk = mp;

			/*
			 * Send the command down to USBA
			 */
			if (hid_send_async_ctrl_request(hidd,
				hid_req_data,
				request_type,
				iocp->ioc_cmd,
				request_index,
				USB_FLAGS_ENQUEUE,
				hid_default_pipe_arg) != USB_SUCCESS) {

				return (HID_FAILURE);
			}

			return (HID_SUCCESS);
		case HID_SET_REPORT:
			/* FALLTHRU */
		case HID_SET_IDLE:
			/* FALLTHRU */
		case HID_SET_PROTOCOL:
			/*
			 * These commands require a hid_req_t.  Make sure
			 * one is present.
			 */
			if (mp->b_cont == NULL) {

				return (HID_FAILURE);
			} else {
				hid_req_data = (hid_req_t *)mp->b_cont->b_rptr;
			}

			/*
			 * For HID_SET_REPORT command, make sure
			 * that data byte exists. Otherwise it will
			 * panic in ohci.
			 */
			if ((iocp->ioc_cmd == HID_SET_REPORT) &&
				(hid_req_data->hid_req_data == NULL)) {

				return (HID_FAILURE);
			}

			/*
			 * Check is version no. is correct. This
			 * is coming from the user
			 */
			if (hid_req_data->hid_req_version_no !=
						HID_VERSION_V_0) {

				return (HID_FAILURE);
			}

			/*
			 * Partially initialize the control request to
			 * be sent to USBA.
			 */
			request_type = USB_DEV_REQ_HOST_TO_DEV |
					USB_DEV_REQ_RECIPIENT_INTERFACE |
					USB_DEV_REQ_TYPE_CLASS;

			request_index =
				hidd->hid_interface_descr.bInterfaceNumber;

			/*
			 * Set up the argument to be passed back to hid
			 * when the asynchronous control callback is
			 * executed.
			 */
			hid_default_pipe_arg = kmem_zalloc(
				sizeof (hid_default_pipe_arg_t), KM_SLEEP);

			hid_default_pipe_arg->
				hid_default_pipe_arg_mctlmsg.ioc_cmd =
					iocp->ioc_cmd;

			hid_default_pipe_arg->
				hid_default_pipe_arg_mctlmsg.ioc_count = 0;

			hid_default_pipe_arg->hid_default_pipe_arg_hidp = hidd;
			hid_default_pipe_arg->hid_default_pipe_arg_mblk = mp;

			/*
			 * Send the command down to USBA through default
			 * pipe.
			 */
			if (hid_send_async_ctrl_request(hidd,
				hid_req_data,
				request_type,
				iocp->ioc_cmd,
				request_index,
				USB_FLAGS_ENQUEUE,
				hid_default_pipe_arg) != HID_SUCCESS) {

				/*
				 * Some SET M_CTLs will have non-null
				 * hid_req_data, free hid_req_data
				 */
				if (hid_req_data->hid_req_data) {
					freemsg((mblk_t *)hid_req_data->
							hid_req_data);
				}

				return (HID_FAILURE);
			}

			return (HID_SUCCESS);
		case HID_GET_PARSER_HANDLE:
			mctlmsg.ioc_cmd = HID_GET_PARSER_HANDLE;
			mctlmsg.ioc_count =
				sizeof (hidd->hid_report_descr);

			if (hid_mctl_send(hidd, mctlmsg,
				(char *)&hidd->hid_report_descr,
				sizeof (hidd->hid_report_descr)) ==
				HID_SUCCESS) {

				freemsg(mp);

				return (HID_SUCCESS);
			} else {
				return (HID_FAILURE);
			}
		case HID_OPEN_POLLED_INPUT:
			/* Initialize the structure */
			hid_polled_input.hid_polled_version =
					HID_POLLED_INPUT_V0;
			hid_polled_input.hid_polled_read = hid_polled_read;
			hid_polled_input.hid_polled_input_enter =
				hid_polled_input_enter;
			hid_polled_input.hid_polled_input_exit =
				hid_polled_input_exit;
			hid_polled_input.hid_polled_input_handle =
				(hid_polled_handle_t)hidd;

			/* Call down into USBA */
			(void) hid_polled_input_init(hidd);

			mctlmsg.ioc_cmd = HID_OPEN_POLLED_INPUT;
			mctlmsg.ioc_count =
				sizeof (hid_polled_input_callback_t *);

			/* Send respons upstream */
			if (hid_mctl_send(hidd, mctlmsg,
				(char *)&hid_polled_input,
				sizeof (hid_polled_input_callback_t)) ==
				HID_SUCCESS) {

				freemsg(mp);
				return (HID_SUCCESS);
			} else {
				return (HID_FAILURE);
			}
		case HID_CLOSE_POLLED_INPUT:
			/* Call down into USBA */
			(void) hid_polled_input_fini(hidd);

			mctlmsg.ioc_cmd = HID_CLOSE_POLLED_INPUT;
			mctlmsg.ioc_count = 0;

			if (hid_mctl_send(hidd, mctlmsg,
				(char *)NULL, 0) == HID_SUCCESS) {

				freemsg(mp);

				return (HID_SUCCESS);
			} else {
				return (HID_FAILURE);
			}
		default:

			return (HID_FAILURE);
	}
}


/*
 * hid_mctl_send:
 *	This function sends a M_CTL message to the upper stream.
 *	Use the same structure and message format that M_IOCTL uses,
 *	using struct iocblk.  buf is an optional buffer that can be copied
 *	to M_CTL message.
 */
static int
hid_mctl_send(hid_state_t *hidp, struct iocblk mctlmsg, char *buf, size_t len)
{
	mblk_t *tmp_iocblk, *tmp_buf;
	queue_t	*rdq;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle, "hid_mctl_send");

	tmp_iocblk = allocb((int)sizeof (struct iocblk), NULL);

	if (tmp_iocblk == NULL) {

		USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
			"hid_mctl_send: allocb failed");

		/* Caller will use the the mblk and send error up */
		return (HID_FAILURE);

	}

	*((struct iocblk *)tmp_iocblk->b_datap->db_base) = mctlmsg;
	tmp_iocblk->b_datap->db_type = M_CTL;

	if (buf) {
		tmp_buf = allocb(len, NULL);

		if (tmp_buf == NULL) {

			USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
				"hid_mctl_send: Second allocb failed");

			/*
			 * The second allocb() failed and the
			 * first one didn't, so release the memory
			 * allocated in the first allocb() call.
			 */

			freeb(tmp_iocblk);

			/* Caller will use the the mblk and send error up */
			return (HID_FAILURE);
		}

		tmp_iocblk->b_cont = tmp_buf;
		bcopy(buf, tmp_buf->b_datap->db_base, len);
	}

	mutex_enter(&hidp->hid_mutex);

	if (!canputnext(hidp->hid_rq_ptr)) {
		freemsg(tmp_iocblk);
	} else {
		rdq = hidp->hid_rq_ptr;
		mutex_exit(&hidp->hid_mutex);
		(void) putnext(rdq, tmp_iocblk);
		mutex_enter(&hidp->hid_mutex);
	}

	mutex_exit(&hidp->hid_mutex);

	return (HID_SUCCESS);
}


/*
 * hid_timeout :
 *	Re-enable the write side service procedure after the
 *	specified time delay. This is because hid_wsrv() is not
 *	able to process all the messages in the queue since async
 *	transfer is sequential. So kernel automatically disables
 *	the service routine and we need to explicitly enable
 *	it.
 */
static void
hid_timeout(void *addr)
{
	queue_t *q = addr;
	hid_state_t *hidd = (hid_state_t *)q->q_ptr;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidd->hid_log_handle,
	    "hid_timeout: addr = 0x%p", (void *)addr);

	mutex_enter(&hidd->hid_mutex);

	enableok(q);
	qenable(q);

	hidd->hid_timeout_id = 0;

	mutex_exit(&hidd->hid_mutex);
}


/*
 * hid_flush :
 *	Flush data already sent upstreams to client module.
 */
static void
hid_flush(queue_t *q)
{
	/*
	 * Flush pending data already sent upstream
	 */
	if ((q != NULL) && (q->q_next != NULL)) {
		(void) putnextctl1(q, M_FLUSH, FLUSHR);
	}
}


/*
 * hid_ioctl:
 * 	Hid currently doesn't handle any ioctls.  NACK
 *	the ioctl request.
 */
static int
hid_ioctl(register queue_t *q, register mblk_t *mp)
{
	register struct iocblk *iocp;

	iocp = (struct iocblk *)mp->b_rptr;

	iocp->ioc_rval = 0;

	iocp->ioc_error = ENOTTY;

	mp->b_datap->db_type = M_IOCNAK;

	qreply(q, mp);

	return (HID_SUCCESS);
}


/*
 * hid_send_async_ctrl_request:
 *	Send an asynchronous control request to USBA.  Since hid is a STREAMS
 *	driver, it is not allowed to wait in its entry points except for the
 *	open and close entry points.  Therefore, hid must use the asynchronous
 *	USBA calls.
 */
static int
hid_send_async_ctrl_request(hid_state_t *hidd, hid_req_t *hid_request,
			uchar_t request_type, int request_request,
			ushort_t request_index,
			uint_t request_flags,
			hid_default_pipe_arg_t *hid_default_pipe_arg)
{
	int			rval;
	usb_pipe_policy_t	policy;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidd->hid_log_handle,
	    "hid_send_async_ctrl_request: "
	    "rq_type=%d rq_rq=%d index=%d flags=%d",
	    request_type, request_request, request_index,
	    request_flags);

	mutex_enter(&hidd->hid_mutex);

	ASSERT(hidd->hid_default_pipe_flags == HID_DEFAULT_PIPE_CLOSED);

	/*
	 * Open pipe and send control request to the device.
	 */
	hidd->hid_default_pipe_policy.pp_callback_arg =
				(void *)hid_default_pipe_arg;

	bcopy(&hidd->hid_default_pipe_policy, &policy,
				sizeof (usb_pipe_policy_t));

	/* Indicate that default pipe is going to be opened */
	hidd->hid_default_pipe_flags = HID_DEFAULT_PIPE_OPEN;

	mutex_exit(&hidd->hid_mutex);

	if (usb_pipe_open(hidd->hid_dip, NULL,
			&policy, USB_FLAGS_OPEN_EXCL,
			&hidd->hid_default_pipe) !=
			USB_SUCCESS) {

			USB_DPRINTF_L2(PRINT_MASK_ATTA,
				hidd->hid_log_handle,
				"hid_send_async_ctrl_request: "
				"Default pipe open failed");

			kmem_free(hid_default_pipe_arg,
					sizeof (hid_default_pipe_arg_t));

			mutex_enter(&hidd->hid_mutex);
			hidd->hid_default_pipe_flags =
					HID_DEFAULT_PIPE_CLOSED;
			mutex_exit(&hidd->hid_mutex);

			return (HID_FAILURE);
	}

	/*
	 * Send an asynchronous control request to the device
	 */
	if (usb_pipe_device_ctrl_send(hidd->hid_default_pipe,
			    request_type,		/* bmReqeustType */
			    request_request,		/* bRequest */
			    hid_request->hid_req_wValue, /* wValue */
			    request_index,		/* wIndex */
			    hid_request->hid_req_wLength, /* wLength */
			    hid_request->hid_req_data,	/* data */
			    request_flags) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_ALL, hidd->hid_log_handle,
			"usb_pipe_device_ctrl_send() failed");

		/*
		 * usb_pipe_device_ctrl_send() returned error,
		 * probably due to a allocation failure. ohci
		 * won't send control request to the device and
		 * async callback won't be called. Cleanup and
		 * close the pipe.
		 */
		kmem_free(hid_default_pipe_arg,
				sizeof (hid_default_pipe_arg_t));

		mutex_enter(&hidd->hid_mutex);
		/* if device is suspending do not start async close */
		if (hidd->hid_dev_state !=  USB_DEV_CPR_SUSPEND) {

			hidd->hid_default_pipe_flags =
				HID_DEFAULT_PIPE_CLOSE_PENDING;
			mutex_exit(&hidd->hid_mutex);

			rval = usb_pipe_close(&hidd->hid_default_pipe,
				USB_FLAGS_OPEN_EXCL,
				hid_async_pipe_close_callback,
				(usb_opaque_t)hidd);
			ASSERT(rval == USB_SUCCESS);

		} else {
			mutex_exit(&hidd->hid_mutex);
		}
		return (HID_FAILURE);
	}

	return (HID_SUCCESS);
}


/*
 * hid_interrupt_pipe_callback:
 *	Callback function for the hid interrupt pipe.
 *	This function is called by USBA when a buffer has been filled.
 *	Since this driver does not cook the data, it just sends the message up,
 */
/*ARGSUSED*/
static int
hid_interrupt_pipe_callback(usb_pipe_handle_t pipe, usb_opaque_t arg,
			mblk_t *data)
{
	hid_state_t *hidp = (hid_state_t *)arg;
	queue_t	*q;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
	    "hid_interrupt_pipe_callback: ph = 0x%p arg = 0x%p",
	    pipe, arg);

	mutex_enter(&hidp->hid_mutex);

	/*
	 * If hid_close() is in progress, we shouldn't try accessing queue
	 * Otherwise indicate that a putnext is going to happen, so
	 * if close after this, that should wait for the putnext to finish.
	 */
	if (hidp->hid_streams_flags != HID_STREAMS_DISMANTLING) {
		hidp->hid_interrupt_pipe_flags =
			HID_INTERRUPT_PIPE_DATA_TRANSFERRING;
		/*
		 * Check if data can be put to the next queue.
		 */
		if (!canputnext(hidp->hid_rq_ptr)) {

			USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
				"Buffer flushed when overflowed.");

			/* Flush the queue above */
			hid_flush(hidp->hid_rq_ptr);

			/* Destroy own data */
			freemsg(data);
			mutex_exit(&hidp->hid_mutex);

		} else {
			q = hidp->hid_rq_ptr;

			mutex_exit(&hidp->hid_mutex);

			/* Put data upstream */
			(void) putnext(q, data);
		}

		mutex_enter(&hidp->hid_mutex);
		hidp->hid_interrupt_pipe_flags = HID_INTERRUPT_PIPE_OPEN;

		/*
		 * hid_close() might start while we dropped the lock.
		 * In that case, close is waiting for putnext to be
		 * finished. Send a signal.
		 */
		if ((hidp->hid_streams_flags == HID_STREAMS_DISMANTLING) ||
			(hidp->hid_dev_state == USB_DEV_DISCONNECTED) ||
			(hidp->hid_dev_state == USB_DEV_CPR_SUSPEND)) {
			cv_signal(&hidp->hid_cv_interrupt_pipe);
		}
		mutex_exit(&hidp->hid_mutex);
	} else {
		mutex_exit(&hidp->hid_mutex);
	}

	/* mark the driver as idle */
	hid_device_idle(hidp);

	return (USB_SUCCESS);
}


/*
 * hid_default_pipe_callback :
 *	Callback routine for the asynchronous control transfer
 *	Called from hid_send_async_ctrl_request() where we open
 *	the pipe in exclusive mode
 */
/*ARGSUSED*/
static int
hid_default_pipe_callback(usb_pipe_handle_t pipe, usb_opaque_t arg,
	mblk_t *data)

{
	hid_default_pipe_arg_t *hid_default_pipe_arg =
			(hid_default_pipe_arg_t *)arg;
	hid_state_t 	*hidp =
			hid_default_pipe_arg->hid_default_pipe_arg_hidp;
	char		*data_to_send = NULL;
	size_t		wLength = 0;
	queue_t		*q;
	int		rval;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
	    "hid_default_pipe_callback: "
	    "ph = 0x%p, arg = 0x%p, data= 0x%p",
	    pipe, arg, data);

	/*
	 * Send the response up the stream, if this callback resulted
	 * from a normal control transfer (i.e, not from a request
	 * from the exception callback).
	 * If this resulted from a CLEAR FEATURE command, no need
	 * to send anything up since an M_ERROR has already been
	 * sent up in before pipe reset.
	 */
	if (hid_default_pipe_arg->hid_default_pipe_arg_bRequest ==
		USB_REQ_CLEAR_FEATURE) {
		/*
		 * This came from an exception callback.
		 * no need to send anything up, this
		 * has been handled by usb_pipe_reset.
		 * Now all we need to do is closing
		 * the pipe that has been opened in
		 * exclusing mode. Free the mblk_t
		 * before that.
		 */
		freemsg(hid_default_pipe_arg->hid_default_pipe_arg_mblk);

	} else {
		/*
		 * Handle the rest of the commands.
		 * Free the original message that was
		 * sent down.
		 */
		freemsg(hid_default_pipe_arg->hid_default_pipe_arg_mblk);

		/*
		 * If there is no data from below, we shouldn't
		 * try to access rptr
		 */
		if (data != NULL) {
			data_to_send = (char *)data->b_rptr;

			switch (hid_default_pipe_arg->
				hid_default_pipe_arg_mctlmsg.ioc_cmd) {
				case HID_GET_REPORT:
				case HID_GET_IDLE:
				case HID_GET_PROTOCOL:
					wLength = data->b_wptr - data->b_rptr;
					break;
				case HID_SET_REPORT:
				case HID_SET_IDLE:
				case HID_SET_PROTOCOL:
				default:
					wLength = 0;
					break;
			}
		}

		if (hid_mctl_send(hidp,
			hid_default_pipe_arg->hid_default_pipe_arg_mctlmsg,
			data_to_send, wLength) == HID_SUCCESS) {

			if (data != NULL) {
				freemsg(data);
			}
		} else {

			/* Use up the same mblk for sending error message up */
			if (data != NULL) {

				mutex_enter(&hidp->hid_mutex);
				q = hidp->hid_rq_ptr;
				mutex_exit(&hidp->hid_mutex);

				if (!canputnext(q)) {
					freemsg(data);
				} else {
					data->b_datap->db_type = M_ERROR;
					if (data->b_cont) {
						freemsg(data->b_cont);
						data->b_cont = NULL;
					}
					data->b_rptr = data->b_datap->db_base;
					data->b_wptr = data->b_rptr +
								sizeof (char);
					*data->b_rptr = EIO;
					putnext(q, data);
				}
			}
		}
	}

	/* Free the argument for the asynchronous callback */
	kmem_free(hid_default_pipe_arg, sizeof (hid_default_pipe_arg_t));

	/*
	 * Release the pipe so that somebody else
	 * can use it
	 */
	mutex_enter(&hidp->hid_mutex);

	/*
	 * The pipe state must be open. Either this function or
	 * exception callback will be executed.
	 */
	ASSERT(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_OPEN);


	/* do not kick off async pipe close if suspended */
	if (hidp->hid_dev_state != USB_DEV_CPR_SUSPEND) {

		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSE_PENDING;
		mutex_exit(&hidp->hid_mutex);

		/* close pipe asynchronously */
		rval = usb_pipe_close(&hidp->hid_default_pipe,
			USB_FLAGS_OPEN_EXCL, hid_async_pipe_close_callback,
			(usb_opaque_t)hidp);
		ASSERT(rval == USB_SUCCESS);

	} else {

		mutex_exit(&hidp->hid_mutex);
	}

	return (USB_SUCCESS);
}


/*
 * hid_async_pipe_close_callback :
 *	Callback routine for the asynchronous usb_pipe_close()
 *	for the default pipe. Close is complete, reset the pipe
 *	open flag here.
 */
/*ARGSUSED*/
static void
hid_async_pipe_close_callback(usb_opaque_t arg, int dummy_arg2,
	uint_t dummy_arg3)
{
	hid_state_t *hidp = (hid_state_t *)arg;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
	    "hid_async_pipe_close_callback: "
	    "arg = 0x%p arg2 = %d arg3 = %d",
	    arg, dummy_arg2, dummy_arg3);

	mutex_enter(&hidp->hid_mutex);

	/*
	 * Make sure that nobody changed the state
	 * while we dropped the lock after
	 * calling usb_pipe_close().
	 */
	ASSERT(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_CLOSE_PENDING);

	/*
	 * Reset the hid_default_pipe state flag
	 * to indicate that pipe is closed.
	 */
	hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_CLOSED;
	ASSERT(hidp->hid_default_pipe == NULL);

	/*
	 * ensure that hid_close_default_pipe gets this close signal
	 * during cpr_suspend, hid_close and disconnect_event_callback
	 */
	if ((hidp->hid_streams_flags == HID_STREAMS_DISMANTLING) ||
		(hidp->hid_dev_state == USB_DEV_DISCONNECTED) ||
		(hidp->hid_dev_state == USB_DEV_CPR_SUSPEND)) {
		cv_signal(&hidp->hid_cv_default_pipe);
	} else {

		/*
		 * call hid_timeout() for enabling write
		 * side service procedure
		 */
		hidp->hid_timeout_id = qtimeout(hidp->hid_wq_ptr,
			hid_timeout, hidp->hid_wq_ptr, 1);
	}

	mutex_exit(&hidp->hid_mutex);

	/* mark the driver as idle */
	hid_device_idle(hidp);
}


/*
 * hid_interrupt_pipe_exception_callback :
 *	Exception callback routine for interrupt pipe. Resets
 *	the pipe. If there is any data, destroy it. No one
 *	is waiting for the exception callback.
 */
/*ARGSUSED*/
static int
hid_interrupt_pipe_exception_callback(usb_pipe_handle_t pipe,
					usb_opaque_t	arg,
					uint_t		completion_reason,
					mblk_t		*data,
					uint_t		flag)
{
	hid_state_t *hidp = (hid_state_t *)arg;
	int	rval;

	USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_interrupt_pipe_exception_callback: "
		"completion_reason = 0x%x, data = 0x%x, flag = 0x%x",
		completion_reason, data, flag);

	/*
	 * Nobody is waiting for this; free the data.
	 */
	if (data) {
		freemsg(data);
	}

	/*
	 * Pipe must be in open state now.
	 */
	mutex_enter(&hidp->hid_mutex);

	/*
	 * If the streams is being dismantled, or the device is has been
	 * disconnected/suspended, don't reset the pipe.
	 * The pipe will be closed anyways.
	 */
	if ((hidp->hid_streams_flags == HID_STREAMS_DISMANTLING) ||
		(hidp->hid_dev_state == USB_DEV_DISCONNECTED) ||
		(hidp->hid_dev_state == USB_DEV_CPR_SUSPEND)) {

		mutex_exit(&hidp->hid_mutex);

	} else {
		hidp->hid_interrupt_pipe_flags =
			HID_INTERRUPT_PIPE_RESET_PENDING;
		mutex_exit(&hidp->hid_mutex);

		rval = usb_pipe_reset(hidp->hid_interrupt_pipe,
			USB_FLAGS_OPEN_EXCL,
			hid_interrupt_pipe_reset_callback,
			(usb_opaque_t)hidp);
		ASSERT(rval == USB_SUCCESS);
	}

	return (USB_SUCCESS);
}


/*
 * hid_default_pipe_exception_callback :
 *	Exception callback routine for default pipe.
 *	Since the device was opened in exclusive mode,
 *	instead os a pipe reset, do a pipe close. Someone
 *	else might want to use it.
 */
/*ARGSUSED*/
static int
hid_default_pipe_exception_callback(usb_pipe_handle_t	pipe,
					usb_opaque_t	arg,
					uint_t		completion_reason,
					mblk_t		*data,
					uint_t		flag)
{
	hid_default_pipe_arg_t	*hid_default_pipe_arg =
			(hid_default_pipe_arg_t *)arg;
	hid_state_t		*hidp =
			hid_default_pipe_arg->hid_default_pipe_arg_hidp;
	hid_pipe_reset_arg_t	*hid_pipe_reset_arg;
	mblk_t			*mp;
	queue_t			*q;
	int			rval;

	USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
		"hid_default_pipe_exception_callback: "
		"completion_reason = 0x%x, data = 0x%x, flag = 0x%x",
		completion_reason, data, flag);

	/* This is an exception callback, no need to pass data up */
	if (data) {
		freemsg(data);
	}

	/*
	 * CLEAR FEATURE failed : don't retry, close pipe and return.
	 * M_ERROR has been sent up on the first stall itself
	 */
	if (hid_default_pipe_arg->hid_default_pipe_arg_stallcount > 0) {
		/*
		 * Free the original message that was
		 * sent down
		 */
		freemsg(hid_default_pipe_arg->hid_default_pipe_arg_mblk);

		/* Free the argument for callback */
		kmem_free(hid_default_pipe_arg,
				sizeof (hid_default_pipe_arg_t));

		mutex_enter(&hidp->hid_mutex);
		/* if device is suspending do not start async close */
		if (hidp->hid_dev_state !=  USB_DEV_CPR_SUSPEND) {

			hidp->hid_default_pipe_flags =
				HID_DEFAULT_PIPE_CLOSE_PENDING;
			mutex_exit(&hidp->hid_mutex);

			rval = usb_pipe_close(&hidp->hid_default_pipe,
					USB_FLAGS_OPEN_EXCL,
					hid_async_pipe_close_callback,
					(usb_opaque_t)hidp);
			ASSERT(rval == USB_SUCCESS);

		} else {
			mutex_exit(&hidp->hid_mutex);
		}

		return (USB_SUCCESS);
	}

	mutex_enter(&hidp->hid_mutex);
	q = hidp->hid_rq_ptr;
	mutex_exit(&hidp->hid_mutex);

	/*
	 * Pass an error message up.
	 */
	if (canputnext(q)) {
		mp = allocb(sizeof (char), NULL);
		ASSERT(mp != NULL);
		mp->b_datap->db_type = M_ERROR;
		mp->b_rptr = mp->b_datap->db_base;
		mp->b_wptr = mp->b_rptr + sizeof (char);
		*mp->b_rptr = EIO;
		putnext(q, mp);
	}

	hid_pipe_reset_arg = (hid_pipe_reset_arg_t *)kmem_zalloc(
					sizeof (hid_pipe_reset_arg_t),
					KM_SLEEP);

	hid_pipe_reset_arg->hid_pipe_reset_arg_hidp = hidp;
	hid_pipe_reset_arg->hid_pipe_reset_arg_exception_cr =
						completion_reason;
	hid_pipe_reset_arg->hid_pipe_reset_arg_defaultp =
				hid_default_pipe_arg;

	/*
	 * Pipe must be in open state now.
	 */
	mutex_enter(&hidp->hid_mutex);
	ASSERT(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_OPEN);

	/*
	 * if the device is disconnected or suspended, do not start
	 * an async reset
	 */
	if ((hidp->hid_dev_state == USB_DEV_CPR_SUSPEND) ||
		(hidp->hid_dev_state == USB_DEV_DISCONNECTED)) {

		mutex_exit(&hidp->hid_mutex);

	} else {
		hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_RESET_PENDING;
		mutex_exit(&hidp->hid_mutex);

		/* start asynchronous pipe reset */
		rval = usb_pipe_reset(hidp->hid_default_pipe,
			USB_FLAGS_OPEN_EXCL,
			hid_default_pipe_reset_callback,
			(usb_opaque_t)hid_pipe_reset_arg);
		ASSERT(rval == USB_SUCCESS);

	}

	return (USB_SUCCESS);
}


/*
 * hid_default_pipe_reset_callback :
 *	Callback routine upon completion of async usb_pipe_reset
 *	of the default pipe. If the completion reason for the exception
 *	is STALL, set a CLEAR FEATURE to correct the error in the
 *	device and the command CLEAR_FEATURE is saved to be used
 *	in the callback upon completion of the control transfer.
 */
/*ARGSUSED*/
static void
hid_default_pipe_reset_callback(usb_opaque_t arg, int dummy_arg2,
	uint_t dummy_arg3)
{
	hid_pipe_reset_arg_t	*hid_pipe_reset_arg =
					(hid_pipe_reset_arg_t *)arg;
	hid_state_t	*hidp = hid_pipe_reset_arg->hid_pipe_reset_arg_hidp;
	int		rval;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
	    "hid_default_pipe_reset_callback: "
	    "arg = 0x%p, arg2 = %d arg3 = %d",
	    arg, dummy_arg2, dummy_arg3);

	mutex_enter(&hidp->hid_mutex);
	ASSERT(hidp->hid_default_pipe_flags == HID_DEFAULT_PIPE_RESET_PENDING);
	mutex_exit(&hidp->hid_mutex);

	switch (hid_pipe_reset_arg->hid_pipe_reset_arg_exception_cr) {
		case USB_CC_STALL:
			mutex_enter(&hidp->hid_mutex);
			/* Reset the state back to open */
			hidp->hid_default_pipe_flags = HID_DEFAULT_PIPE_OPEN;
			mutex_exit(&hidp->hid_mutex);

			hid_pipe_reset_arg->hid_pipe_reset_arg_defaultp->
				hid_default_pipe_arg_bRequest =
					USB_REQ_CLEAR_FEATURE;

			hid_pipe_reset_arg->hid_pipe_reset_arg_defaultp->
				hid_default_pipe_arg_stallcount++;

			/* Send an async CLEAR FEATURE req to the device */
			if (usb_pipe_device_ctrl_send(hidp->hid_default_pipe,
				USB_DEV_REQ_HOST_TO_DEV |
				USB_DEV_REQ_RECIPIENT_ENDPOINT,
				USB_REQ_CLEAR_FEATURE,	/* bRequest */
				USB_ENDPOINT_HALT,	/* wValue */
				usb_endpoint_num(hidp->hid_default_pipe),
				0,			/* wLength */
				NULL,			/* no data to send */
				USB_FLAGS_ENQUEUE) != USB_SUCCESS) {

				USB_DPRINTF_L2(PRINT_MASK_ALL,
					hidp->hid_log_handle,
					"Clear feature failed");
			}
			break;
		default:
			mutex_enter(&hidp->hid_mutex);
			/* if device is suspending do not start async close */
			if (hidp->hid_attach_flags !=  USB_DEV_CPR_SUSPEND) {
				/* Close the pipe */
				hidp->hid_default_pipe_flags =
					HID_DEFAULT_PIPE_CLOSE_PENDING;
				mutex_exit(&hidp->hid_mutex);

				rval = usb_pipe_close(&hidp->hid_default_pipe,
						USB_FLAGS_OPEN_EXCL,
						hid_async_pipe_close_callback,
						(usb_opaque_t)hidp);
				ASSERT(rval == USB_SUCCESS);
			} else {
				mutex_exit(&hidp->hid_mutex);
			}

			/*
			 * Free the original message that was
			 * sent down
			 */
			freemsg(hid_pipe_reset_arg->
				hid_pipe_reset_arg_defaultp->
				hid_default_pipe_arg_mblk);

			/* Free the argument for callback */

			kmem_free(hid_pipe_reset_arg->
				hid_pipe_reset_arg_defaultp,
				sizeof (hid_default_pipe_arg_t));

			break;
	}

	/* Free the argument for the asynchronous callback */
	kmem_free(hid_pipe_reset_arg, sizeof (hid_pipe_reset_arg_t));
}


/*
 * hid_interrupt_pipe_reset_callback :
 *	Callback routine upon completion of async usb_pipe_reset
 *	of the interrupt pipe. If pipe reset was successful, start
 *	polling the device again. Else, the device is really bad,
 *	give up .
 */
/*ARGSUSED*/
static void
hid_interrupt_pipe_reset_callback(usb_opaque_t arg1, int rval,
	uint_t dummy_arg3)
{
	hid_state_t	*hidp = (hid_state_t *)arg1;

	USB_DPRINTF_L4(PRINT_MASK_ALL, hidp->hid_log_handle,
	    "hid_interrupt_pipe_reset_callback: "
	    "arg1 = 0x%p, rval = %d arg3 = %d",
	    arg1, rval, dummy_arg3);

	mutex_enter(&hidp->hid_mutex);

	/*
	 * If the pipe is being dismantled, then signal the
	 * the hid_close routine which will close the pipe.
	 */
	if ((hidp->hid_streams_flags == HID_STREAMS_DISMANTLING) ||
		(hidp->hid_dev_state == USB_DEV_DISCONNECTED) ||
		(hidp->hid_dev_state == USB_DEV_CPR_SUSPEND)) {

		/*
		 * Set the flag to open state only if the pipe state
		 * is still reset. A usb_pipe_close() might took
		 * place in between and the pipe state may be
		 * Set the flag to open state if the pipe state
		 * closed now.
		 */
		if (hidp->hid_interrupt_pipe_flags ==
			HID_INTERRUPT_PIPE_RESET_PENDING) {

			hidp->hid_interrupt_pipe_flags =
				HID_INTERRUPT_PIPE_OPEN;
		}

		cv_signal(&hidp->hid_cv_interrupt_pipe);
		mutex_exit(&hidp->hid_mutex);

	} else {

		hidp->hid_interrupt_pipe_flags = HID_INTERRUPT_PIPE_OPEN;
		mutex_exit(&hidp->hid_mutex);

		if (rval == USB_SUCCESS) {

			rval = usb_pipe_start_polling((
				(hid_state_t *)hidp)->hid_interrupt_pipe,
				USB_FLAGS_SHORT_XFER_OK);
			ASSERT(rval == USB_SUCCESS);

		} else {
			USB_DPRINTF_L2(PRINT_MASK_ALL, hidp->hid_log_handle,
			    "hid_interrupt_pipe_reset_callback: "
			    "pipe reset failed. rval = 0x%x", rval);
		}
	}
}


/*
 * hid_set_idle:
 *	Make a clas specific request to SET_IDLE.
 *	In this case send no reports if state has not changed.
 *	See HID 7.2.4.
 */
/*ARGSUSED*/
static void
hid_set_idle(hid_state_t	*hidp)
{
	uint_t		completion_reason;
	int		rval;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_set_idle: Begin");

	if (usb_pipe_sync_device_ctrl_send(hidp->hid_default_pipe,
						/* bmRequestType */
				USB_DEV_REQ_HOST_TO_DEV |
				USB_DEV_REQ_TYPE_CLASS |
				USB_DEV_REQ_RECIPIENT_INTERFACE,
				SET_IDLE,	/* bRequest */
				DURATION,	/* wValue no repeat */
						/* wIndex - Interface */
				hidp->hid_interface_descr.bInterfaceNumber,
				0,		/* wLength */
				NULL,		/* no data to send */
				&completion_reason,
				USB_FLAGS_ENQUEUE) != USB_SUCCESS) {


		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"Failed while trying to set idle,"
				"completion reason %d\n", completion_reason);

		/*
		 * Some devices fail to follow the specification
		 * and instead of STALLing, they continously
		 * NAK the SET_IDLE command. We need to reset
		 * the pipe then, so that ohci doesn't panic.
		 */
		if ((completion_reason == USB_CC_STALL) ||
			(completion_reason == USB_CC_TIMEOUT)) {

			rval = usb_pipe_reset(hidp->hid_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);

		}

		/* Send a CLEAR_FEATURE command to clear the STALL */

		if (completion_reason == USB_CC_STALL) {

			if (usb_pipe_sync_device_ctrl_send(
				hidp->hid_default_pipe,
				USB_DEV_REQ_HOST_TO_DEV |
				USB_DEV_REQ_RECIPIENT_ENDPOINT,
				USB_REQ_CLEAR_FEATURE,
				USB_ENDPOINT_HALT,
				usb_endpoint_num(hidp->hid_default_pipe),
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE) != USB_SUCCESS) {

				/* Clear Feature not ok. */
				USB_DPRINTF_L2(PRINT_MASK_ATTA,
					hidp->hid_log_handle,
					"Clear feature failed.");
			}
		}

	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_set_idle: End");
}


/*
 * hid_set_boot_protocol:
 *	Currently, hid automatically sets the device to the boot protocol, if
 *	one exists.  The boot protocol is currently defined for keyboard and
 *	mouse.  See HID 7.2.6 for more details.  In the future,
 *	hid_set_boot_protocol() might need a parameter specifying whether or
 *	not the boot protocol is to be turned on or off.  hid_set_idle() might
 *	also need a parameter since it's possible to set the idle value to
 *	anything.
 */
/*ARGSUSED*/
static void
hid_set_boot_protocol(hid_state_t	*hidp)
{
	uint_t		completion_reason;
	int		rval;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_set_boot_protocol: Begin");

	if (usb_pipe_sync_device_ctrl_send(hidp->hid_default_pipe,
						/* bmRequestType */
				USB_DEV_REQ_HOST_TO_DEV |
				USB_DEV_REQ_TYPE_CLASS |
				USB_DEV_REQ_RECIPIENT_INTERFACE,
				SET_PROTOCOL,	/* bRequest */
				BOOT_PROTOCOL,	/* wValue no repeat */
						/* wIndex - Interface */
				hidp->hid_interface_descr.bInterfaceNumber,
				0,		/* wLength */
				NULL,		/* no data to send */
				&completion_reason,
				USB_FLAGS_ENQUEUE) != USB_SUCCESS) {
		/*
		 * Some devices fail to follow the specification
		 * and instead of STALLing, they continously
		 * NAK the SET_IDLE command. We need to reset
		 * the pipe then, so that ohci doesn't panic.
		 */
		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"Failed while trying to set boot protocol,"
				"completion reason %d\n", completion_reason);

		if ((completion_reason == USB_CC_STALL) ||
			(completion_reason == USB_CC_TIMEOUT)) {
			rval = usb_pipe_reset(hidp->hid_default_pipe,
					USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);
		}

		/* Send a CLEAR_FEATURE command to clear the STALL */
		if (completion_reason == USB_CC_STALL) {

			if (usb_pipe_sync_device_ctrl_send(
				hidp->hid_default_pipe,
				USB_DEV_REQ_HOST_TO_DEV |
				USB_DEV_REQ_RECIPIENT_ENDPOINT,
				USB_REQ_CLEAR_FEATURE,
				USB_ENDPOINT_HALT,
				usb_endpoint_num(hidp->hid_default_pipe),
				0,
				NULL,
				&completion_reason,
				USB_FLAGS_ENQUEUE) != USB_SUCCESS) {

				/* Clear Feature not ok. */

				USB_DPRINTF_L2(PRINT_MASK_ATTA,
				    hidp->hid_log_handle,
				    "Clear feature failed");
			}
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, hidp->hid_log_handle,
		"hid_set_boot_protocol: End");
}


/*
 * hid_parse_hid_descr :
 *	Parse the hid descriptor
 */
static size_t
hid_parse_hid_descr(
	uchar_t			*buf,   /* from GET_DESCRIPTOR(CONFIGURATION) */
	size_t			buflen,
	usb_hid_descr_t		*ret_descr,
	size_t			ret_buf_len,
	usb_interface_descr_t	*interface_descr)
{
	uchar_t *bufend = buf + buflen;
	short found_descr = 0;

	while (buf + 4 <= bufend) {

		if ((buf[1] == USB_DESCR_TYPE_HID) && found_descr) {

			return (usb_parse_CV_descr("ccscccs",
				buf, bufend - buf, (void *)ret_descr,
				(size_t)ret_buf_len));
		}

		if ((buf[1] == USB_DESCR_TYPE_INTERFACE) &&
			(((usb_interface_descr_t *)buf)->bInterfaceNumber ==
			interface_descr->bInterfaceNumber)) {

			found_descr = 1;
		}

		/*
		 * Check for a bad buffer.
		 * If buf[0] is 0, then this will be an infinite loop
		 */
		if (buf[0] == 0) {

			break;
		} else {
			buf += buf[0];
		}
	}

	return (USB_PARSE_ERROR);
}


/*
 * hid_parse_hid_descr_failure :
 *	If parsing of hid descriptor failed and the device is
 *	a keyboard or mouse, use predefined length and packet size.
 */
static int
hid_parse_hid_descr_failure(hid_state_t	*hidp)
{
	/*
	 * Parsing hid descriptor failed, probably because the
	 * device did not return a valid hid descriptor. Check to
	 * see if this is a keyboard or mouse. If so, use the
	 * predefined hid descriptor length and packet size.
	 * Otherwise, detach and return failure.
	 */

	USB_DPRINTF_L1(PRINT_MASK_ATTA, hidp->hid_log_handle,
	    "Parsing of hid descriptor failed");

	if (hidp->hid_interface_descr.bInterfaceProtocol ==
					KEYBOARD_PROTOCOL) {
		/* device is a keyboard */

		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
		    "Set hid descriptor length to predefined "
		    "USB_KB_HID_DESCR_LENGTH for keyboard.");

		hidp->hid_hid_descr.wReportDescriptorLength =
				    USB_KB_HID_DESCR_LENGTH;

		hidp->hid_packet_size = USBKPSZ;

	} else if (hidp->hid_interface_descr.bInterfaceProtocol ==
			    MOUSE_PROTOCOL) {

		/* device is a mouse */

		USB_DPRINTF_L2(PRINT_MASK_ATTA, hidp->hid_log_handle,
		    "Set hid descriptor length to predefined "
		    "USB_MS_HID_DESCR_LENGTH for mouse.");

		hidp->hid_hid_descr.wReportDescriptorLength =
				    USB_MS_HID_DESCR_LENGTH;

		hidp->hid_packet_size = USBMSSZ;

	} else {

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/*
 * hid_handle_report_descriptor :
 *	Get the report descriptor, call hidparser routine to parse
 *	it and query the hidparser tree to get the packet size
 */
static int
hid_handle_report_descriptor(hid_state_t	*hidp,
				int		interface,
				mblk_t		** data)
{
	uint_t			completion_reason;
	uint32_t		packet_size = 0;
	int			i;

	/*
	 * Parsing hid desciptor was successful earlier.
	 * Get Report Descriptor
	 */
	if (usb_pipe_sync_device_ctrl_receive(
		hidp->hid_default_pipe,
		USB_DEV_REQ_DEVICE_TO_HOST |
		USB_DEV_REQ_RECIPIENT_INTERFACE, /* bmRequestType */
		USB_REQ_GET_DESCRIPTOR,		/* bRequest */
		USB_CLASS_DESCR_TYPE_REPORT,	/* wValue */
		interface,			/* Interface. wIndex */
		hidp->hid_hid_descr.wReportDescriptorLength, /* wLength */
		data,				/* data */
		&completion_reason,
		USB_FLAGS_ENQUEUE) != USB_SUCCESS) {

		USB_DPRINTF_L1(PRINT_MASK_ATTA, hidp->hid_log_handle,
			"Failed to receive the Report Descriptor");

		return (USB_FAILURE);

	} else {
#ifdef DEBUG
		int n =  hidp->hid_hid_descr.wReportDescriptorLength;

		/* Print the report descriptor */
		for (i = 0; i < n; i++) {
			USB_DPRINTF_L3(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"Index = %d\tvalue = %x", i,
				(int)(*data)->b_rptr[i]);
		}
#endif
		/* Get Report Descriptor was successful */
		if (hidparser_parse_report_descriptor(
			(*data)->b_rptr,
			hidp->hid_hid_descr.wReportDescriptorLength,
			&hidp->hid_hid_descr,
			&hidp->hid_report_descr) ==
				HIDPARSER_SUCCESS) {

				(void) hidparser_get_packet_size(
					hidp->hid_report_descr,
					0, HIDPARSER_ITEM_INPUT,
					(uint32_t *)&packet_size);

				hidp->hid_packet_size = packet_size/8;
		} else {

			USB_DPRINTF_L1(PRINT_MASK_ATTA, hidp->hid_log_handle,
				"Invalid Report Descriptor");

			freemsg(*data);

			return (USB_FAILURE);
		}

		freemsg(*data);

		return (USB_SUCCESS);
	}
}


/*
 * hid_polled_input_init :
 *	This routine calls down to the lower layers to initialize any state
 *	information.  This routine initializes the lower layers for input.
 */
static int
hid_polled_input_init(hid_state_t *hidp)
{
	/*
	 * Call the lower layers to intialize any state information
	 * that they will need to provide the polled characters.
	 */
	if (usb_console_input_init(hidp->hid_dip, hidp->hid_interrupt_pipe,
		&hidp->hid_polled_raw_buf,
		&hidp->hid_polled_console_info) != USB_SUCCESS) {

		/*
		 * If for some reason the lower layers cannot initialized, then
		 * bail.
		 */

		(void) hid_polled_input_fini(hidp);

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/*
 * hid_polled_input_fini:
 *	This routine is called when we are done using this device as an input
 *	device.
 */
static int
hid_polled_input_fini(hid_state_t *hidp)
{
	/*
	 * Call the lower layers to free any state information
	 */
	if (usb_console_input_fini(hidp->hid_polled_console_info)
		!= USB_SUCCESS) {

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/*
 * hid_polled_input_enter:
 *	This is the routine that is called in polled mode to save the USB
 *	state information before using the USB keyboard as an input device.
 *	This routine, and all of the routines that it calls, are responsible
 *	for saving any state information so that it can be restored when
 *	polling mode is over.
 */
static int
/* ARGSUSED */
hid_polled_input_enter(hid_polled_handle_t hid_polled_inputp)
{
	hid_state_t *hidp = (hid_state_t *)hid_polled_inputp;

	/*
	 * Call the lower layers to tell them to save any state information.
	 */
	(void) usb_console_input_enter(hidp->hid_polled_console_info);

	return (USB_SUCCESS);
}


/*
 * hid_polled_read :
 *	This is the routine that is called in polled mode when it wants to read
 *	a character.  We will call to the lower layers to see if there is any
 *	input data available.  If there is USB scancodes available, we will
 *	give them back.
 */
static int
hid_polled_read(hid_polled_handle_t hid_polled_input, uchar_t **buffer)
{
	hid_state_t *hidp = (hid_state_t *)hid_polled_input;
	uint_t			num_bytes;

	/*
	 * Call the lower layers to get the character from the controller.
	 * The lower layers will return the number of characters that
	 * were put in the raw buffer.  The address of the raw buffer
	 * was passed down to the lower layers during hid_polled_init.
	 */
	if (usb_console_read(hidp->hid_polled_console_info,
		&num_bytes) != USB_SUCCESS) {

		return (0);
	}

	/*LINTED*/
	_NOTE(NO_COMPETING_THREADS_NOW);

	*buffer = hidp->hid_polled_raw_buf;

	/*LINTED*/
	_NOTE(COMPETING_THREADS_NOW);

	/*
	 * Return the number of characters that were copied into the
	 * polled buffer.
	 */
	return (num_bytes);
}


/*
 * hid_polled_input_exit :
 *	This is the routine that is called in polled mode  when it is giving up
 *	control of the USB keyboard.  This routine, and the lower layer routines
 *	that it calls, are responsible for restoring the controller state to the
 *	state it was in before polled mode.
 */
static int
hid_polled_input_exit(hid_polled_handle_t hid_polled_inputp)
{
	hid_state_t *hidp = (hid_state_t *)hid_polled_inputp;

	/*
	 * Call the lower layers to restore any state information.
	 */
	(void) usb_console_input_exit(hidp->hid_polled_console_info);

	return (0);
}
