/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)uhcihub.c 1.13     99/10/05 SMI"


/*
 * Universal Serial BUS  Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which inetrfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The inetrface to
 * the Host Controller is defined by the Universal Host Controller Interface.
 * THis file contains the code for root hub related functions.
 */

#include		<sys/usb/hcd/uhci/uhcihub.h>

/*
 * uhci_init_root_hub:
 *
 * Initialize the root hub
 */

void
uhci_init_root_hub(uhci_state_t *uhcip)
{
	int		i;
	usb_hub_descr_t	*root_hub_descr =
		&uhcip->uhci_root_hub.root_hub_descr;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_init_root_hub");

	uhcip->uhci_root_hub.root_hub_num_ports = MAX_RH_PORTS;

	/*
	 * Build the hub descriptor
	 */

	root_hub_descr->bDescLength	= ROOT_HUB_DESCRIPTOR_LENGTH;
	root_hub_descr->bDescriptorType = ROOT_HUB_DESCRIPTOR_TYPE;
	root_hub_descr->bNbrPorts	= MAX_RH_PORTS;

	/* Determine the Power Switching Mode */
	root_hub_descr->wHubCharacteristics = HUB_CHARS_NO_POWER_SWITCHING;

	root_hub_descr->wHubCharacteristics |= HUB_CHARS_NO_OVER_CURRENT;

	/* Obtain the power on to power good time of the ports */
	uhcip->uhci_root_hub.root_hub_potpgt = 1;

	root_hub_descr->bPwrOn2PwrGood = 0;

	/* Indicate if the device is removable */
	root_hub_descr->DeviceRemovable = 0x0;

	/* Fill in the port power control mask */
	root_hub_descr->PortPwrCtrlMask = 0xFF;

	for (i = 0; i < uhcip->uhci_root_hub.root_hub_num_ports; i++) {
		uhcip->uhci_root_hub.root_hub_port_state[i]  = DISCONNECTED;
		uhcip->uhci_root_hub.root_hub_port_status[i] = 0;
	}
}


/*
 * uhci_load_root_hub_driver:
 *
 * Attach the root hub
 */

usb_device_descr_t root_hub_device_descriptor = {
	0x12,	/* Length */
	1,	/* Type */
	1,	/* Bcd */
	9,	/* Class */
	0,	/* Sub class */
	0,	/* Protocol */
	8,	/* Max pkt size */
	0,	/* Vendor */
	0,	/* Product id */
	0,	/* Device release */
	0,	/* Manufacturer */
	0,	/* Product */
	0,	/* Sn */
	1	/* No of configs */
};

uchar_t root_hub_config_descriptor[] = {
	0x9,    0x2,    0x19,    0x0,    0x1,    0x1,    0x0,    0x60,
	0x0,    0x9,    0x4,    0x0,    0x0,    0x1,    0x9,    0x1,
	0x0,    0x0,    0x7,    0x5,    0x81,    0x3,    0x1,    0x0,
	0x20};


int
uhci_load_root_hub_driver(uhci_state_t *uhcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
	    "uhci_load_root_hub_driver:");

	return (usba_hubdi_bind_root_hub(uhcip->uhci_dip,
	    root_hub_config_descriptor,
	    sizeof (root_hub_config_descriptor),
	    &root_hub_device_descriptor));
}


/*
 * uhci_handle_root_hub_request:
 *
 * Intercept a root hub request.  Handle the  root hub request through the
 * registers
 */
/* ARGSUSED */
int
uhci_handle_root_hub_request(usb_pipe_handle_impl_t  *pipe_handle,
	uchar_t			bmRequestType,
	uchar_t			bRequest,
	uint16_t		wValue,
	uint16_t		wIndex,
	uint16_t		wLength,
	mblk_t			*data,
	uint_t			usb_flags)
{
	uint16_t	port   = wIndex - 1;
	uhci_state_t	*uhcip;
	int		error = USB_SUCCESS;

	uhcip = (uhci_state_t *)
		uhci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
	"uhci_handle_root_hub_request: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%p",
	bmRequestType, bRequest, wValue, wIndex, wLength, (void *)data);

	switch (bmRequestType) {
		case SET_CLEAR_PORT_FEATURE:
			error = uhci_handle_set_clear_port_feature(uhcip,
				pipe_handle, bRequest, wValue, port);
			break;
		case GET_PORT_STATUS:
			error = uhci_handle_get_port_status(uhcip, port,
				pipe_handle, wLength);
			break;
		case GET_HUB_DESCRIPTOR:
			switch (bRequest) {
			case USB_REQ_GET_DESCRIPTOR:
				error = uhci_handle_get_hub_descriptor(uhcip,
					pipe_handle, wLength);
				break;
			case USB_REQ_GET_STATUS:
				error = uhci_handle_get_hub_status(uhcip,
					pipe_handle, wLength);
				break;
			default:
				USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB,
				uhcip->uhci_log_hdl,
				"uhci_handle_root_hub_request: "
				"Unsupported request 0x%x", bmRequestType);

				error = USB_FAILURE;

				mutex_exit(&uhcip->uhci_int_mutex);
				/* Return failure */
				usba_hcdi_callback(pipe_handle,
					USB_FLAGS_SLEEP, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_FAILURE);
				mutex_enter(&uhcip->uhci_int_mutex);
			}
			break;
		default:
			USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
			"uhci_handle_root_hub_request: "
			"Unsupported request 0x%x", bmRequestType);

			error = USB_FAILURE;

			mutex_exit(&uhcip->uhci_int_mutex);
			/* Return failure */
			usba_hcdi_callback(pipe_handle,
				USB_FLAGS_SLEEP, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_FAILURE);
			mutex_enter(&uhcip->uhci_int_mutex);
	}

	if (data)
		freeb(data);

	return (error);
}


/*
 * uhci_handle_set_clear_port_feature:
 */
static int
uhci_handle_set_clear_port_feature(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t *pipe_handle,
	uchar_t			bRequest,
	uint16_t		wValue,
	uint16_t		port)
{
	int    error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
	"uhci_handle_set_clear_port_feature: 0x%x 0x%x 0x%x",
	    bRequest, wValue, port);

	switch (bRequest) {
		case USB_REQ_SET_FEATURE:
			switch (wValue) {
				case CFS_PORT_POWER:
					uhci_handle_port_power(
					uhcip,
					pipe_handle, port,
					UHCI_ENABLE_PORT_PWR);
					break;
				case CFS_PORT_ENABLE:
					uhci_handle_port_enable_disable(
					uhcip,
					pipe_handle, port, UHCI_ENABLE_PORT);
					break;
				case CFS_PORT_RESET:
					uhci_handle_port_reset(
					uhcip,
					pipe_handle, port);
					break;
				default:
					USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
					uhcip->uhci_log_hdl,
					"uhci_handle_set_clear_port_feature: "
					"Unsupported request 0x%x 0x%x",
					bRequest, wValue);

					error = USB_FAILURE;

					mutex_exit(&uhcip->uhci_int_mutex);
					/* Return failure */
					usba_hcdi_callback(pipe_handle,
						USB_FLAGS_SLEEP, NULL,
						0,
						USB_CC_UNSPECIFIED_ERR,
						USB_FAILURE);
					mutex_enter(&uhcip->uhci_int_mutex);
				break;
			}
			break;

		case USB_REQ_CLEAR_FEATURE:
			switch (wValue) {
				case CFS_PORT_POWER:
					uhci_handle_port_power(uhcip,
						pipe_handle,
						port,
						UHCI_DISABLE_PORT_PWR);
					break;
				case CFS_PORT_ENABLE:
					uhci_handle_port_enable_disable(
						uhcip, pipe_handle,
						port, UHCI_DISABLE_PORT);
					break;
				case CFS_C_PORT_ENABLE:
					uhci_handle_port_enable_disable(
						uhcip, pipe_handle,
						port, UHCI_CLEAR_ENDIS_BIT);
					break;
				case CFS_C_PORT_RESET:
					error = uhci_handle_complete_port_reset(
						uhcip,
						pipe_handle, port);
					break;
				case CFS_C_PORT_CONNECTION:
					uhci_handle_clear_port_connection(
					uhcip, pipe_handle, port);
					break;
				default:
					USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
					uhcip->uhci_log_hdl,
					"uhci_handle_set_clear_port_feature: "
					"Unsupported request 0x%x 0x%x",
					bRequest, wValue);

					error = USB_FAILURE;

					mutex_exit(&uhcip->uhci_int_mutex);
					/* Return failure */
					usba_hcdi_callback(pipe_handle,
						USB_FLAGS_SLEEP, NULL,
						0,
						USB_CC_UNSPECIFIED_ERR,
						USB_FAILURE);

					mutex_enter(&uhcip->uhci_int_mutex);

					break;
			}
			break;

		default:
			USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
				uhcip->uhci_log_hdl,
				"uhci_handle_set_clear_port_feature: "
				"Unsupported request 0x%x 0x%x",
				bRequest, wValue);

			error = USB_FAILURE;

			mutex_exit(&uhcip->uhci_int_mutex);
			/* Return failure */
			usba_hcdi_callback(pipe_handle,
				USB_FLAGS_SLEEP, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_FAILURE);
			mutex_enter(&uhcip->uhci_int_mutex);
	}

	return (error);
}


/*
 * uhci_handle_port_power:
 *
 * Turn on a root hub port.
 */
/* ARGSUSED */
static void
uhci_handle_port_power(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t *pipe_handle,
	uint16_t port, uint_t on)
{
	/*
	 * Driver does not have any control over the power status
	 */
	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
		0, USB_CC_NOERROR, USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);
}


/*
 * uhci_handle_port_enable_disable:
 *
 * Handle port enable request.
 */
static void
uhci_handle_port_enable_disable(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t *pipe_handle,
	uint16_t port, uint_t action)
{
	uint_t		port_status;

	port_status = Get_OpReg16(PORTSC[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_port_enable: port = 0x%x, status = 0x%x",
		port, port_status);

	if (action == UHCI_ENABLE_PORT) {
		/* See if the port enable is already on */
		if (!(port_status & HCR_PORT_ENABLE)) {
			/* Enable the port */
			Set_OpReg16(PORTSC[port],
				(port_status | HCR_PORT_ENABLE));
		}
	} else if (action == UHCI_DISABLE_PORT) {
		/* See if the port enable is already off */
		if ((port_status & HCR_PORT_ENABLE)) {
			/* disable the port by writing CCS bit */
			Set_OpReg16(PORTSC[port], HCR_PORT_CCS);
		}
	} else {
		/* Clear the Enable/Disable change bit */
		Set_OpReg16(PORTSC[port],
			(port_status | HCR_PORT_ENDIS_CHG));

	}

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, NULL,
		0, USB_CC_NOERROR, USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);
}


/*
 * uhci_handle_port_reset:
 *
 * Perform a port reset.
 */
static void
uhci_handle_port_reset(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*pipe_handle,
	uint16_t		port)
{
	uint_t			port_status = Get_OpReg16(PORTSC[port]);
	mblk_t			*message;
	usb_endpoint_descr_t  	*endpoint_descr;
	size_t			length;

	endpoint_descr	=
		uhcip->uhci_root_hub.root_hub_pipe_handle->p_endpoint;
	length		= endpoint_descr->wMaxPacketSize;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_port_reset: port = 0x%x status = 0x%x",
		port, port_status);

#ifdef DEBUG
	if (!(port_status & HCR_PORT_CCS)) {
		USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"port_status & HCR_PORT_CCS == 0: port = 0x%x status = 0x%x",
		port, port_status);
	}
#endif

	Set_OpReg16(PORTSC[port], (port_status| HCR_PORT_RESET));

	drv_usecwait(UHCI_RESET_DELAY);
	Set_OpReg16(PORTSC[port], (port_status & ~HCR_PORT_RESET));

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, NULL,
		0, USB_CC_NOERROR, USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);

	/*
	 * Inform the upper layer that reset has occured on the port
	 * This is required because the upper layer is expecting a
	 * an evernt immidiately after doing reset. In case of OHCI,
	 * the controller gets an interrupt for the change in the
	 * root hub status but in case of UHCI, we dont. So, send a
	 * event to the upper layer as soon as we complete the reset.
	 */

	/* Allocate a required size message block */
	message = allocb(length, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_handle_root_hub_status_change: Allocb failed");
		return;
	}

	*message->b_wptr++ = (1 << (port+1));
	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(uhcip->uhci_root_hub.root_hub_pipe_handle,
		0,		/* Usb_flags */
		message,	/* Mblk */
		0,		/* Flag */
		USB_CC_NOERROR, /* Completion_reason */
		USB_SUCCESS);	/* Return value, don't care here */
	mutex_enter(&uhcip->uhci_int_mutex);
}


/*
 * uhci_handle_complete_port_reset:
 *
 * Perform a port reset change.
 */
static int
uhci_handle_complete_port_reset(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*pipe_handle,
	uint16_t		port)
{
	uint_t port_status = Get_OpReg16(PORTSC[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_complete_port_reset: port = 0x%x status = 0x%x",
		port, port_status);

#ifdef DEBUG
	if (!(port_status & HCR_PORT_CCS)) {
		USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"port_status & HCR_PORT_CCS == 0: port = 0x%x status = 0x%x",
			port, port_status);
	}
#endif

	Set_OpReg16(PORTSC[port], (port_status & (~ HCR_PORT_RESET)));

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, NULL,
		0, USB_CC_NOERROR, USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * uhci_handle_clear_port_connection:
 *
 * Perform a clear port connection.
 */
static void
uhci_handle_clear_port_connection(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*pipe_handle,
	uint16_t		port)
{
	uint_t port_status = Get_OpReg16(PORTSC[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_clear_port_connection: port = 0x%x"
		"status = 0x%x", port, port_status);

	/* Clear CSC bit */
	Set_OpReg16(PORTSC[port], port_status | HCR_PORT_CSC);

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, NULL, 0,
		USB_CC_NOERROR,
		USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);
}


/*
 * uhci_handle_get_port_status:
 *
 * Handle a get port status request.
 */
static int
uhci_handle_get_port_status(uhci_state_t *uhcip,
	uint16_t		port,
	usb_pipe_handle_impl_t	*pipe_handle,
	uint16_t		wLength)
{
	mblk_t			*message;
	uint_t			new_port_status;
	uint_t			old_port_status =
		uhcip->uhci_root_hub.root_hub_port_status[port];
	uint_t			change_status;

	ASSERT(wLength == 4);

	message = allocb(wLength, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_handle_get_port_status: Allocb failed");

		mutex_exit(&uhcip->uhci_int_mutex);
		usba_hcdi_callback(pipe_handle,
			USB_FLAGS_SLEEP, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);
		mutex_enter(&uhcip->uhci_int_mutex);

		return (USB_NO_RESOURCES);
	}


	new_port_status = uhci_get_port_status(uhcip, port);

	change_status   = (old_port_status ^ new_port_status) & 0xff;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_get_port_status:\n\t"
		"port%d: old status = 0x%x  new status = 0x%x change = 0x%x",
		port, old_port_status, new_port_status, change_status);

	*message->b_wptr++ = (uchar_t)new_port_status;
	*message->b_wptr++ = (uchar_t)(new_port_status >> 8);
	*message->b_wptr++ = (uchar_t)change_status;
	*message->b_wptr++ = (uchar_t)(change_status >> 8);

	/* Update the status */
	uhcip->uhci_root_hub.root_hub_port_status[port] = new_port_status;

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, message,
		0, USB_CC_NOERROR,
		USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * uhci_handle_get_hub_descriptor:
 */
static int
uhci_handle_get_hub_descriptor(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*pipe_handle,
	uint16_t		wLength)
{
	mblk_t			*message;
	usb_hub_descr_t		*root_hub_descr =
			&uhcip->uhci_root_hub.root_hub_descr;
	static uchar_t		raw_descr[ROOT_HUB_DESCRIPTOR_LENGTH];


	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_get_hub_descriptor: wLength = 0x%x",
		wLength);

	ASSERT(wLength != 0);

	message = allocb(wLength, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_handle_get_hub_descriptor: Allocb failed");

		mutex_exit(&uhcip->uhci_int_mutex);
		usba_hcdi_callback(pipe_handle,
			USB_FLAGS_SLEEP, NULL,
			0, USB_CC_UNSPECIFIED_ERR, USB_NO_RESOURCES);
		mutex_enter(&uhcip->uhci_int_mutex);

		return (USB_NO_RESOURCES);
	}

	bzero(&raw_descr, ROOT_HUB_DESCRIPTOR_LENGTH);

	raw_descr[0] = root_hub_descr->bDescLength;
	raw_descr[1] = root_hub_descr->bDescriptorType;
	raw_descr[2] = root_hub_descr->bNbrPorts;
	raw_descr[3] = root_hub_descr->wHubCharacteristics & 0x00FF;
	raw_descr[4] = (root_hub_descr->wHubCharacteristics & 0xFF00) >> 8;
	raw_descr[5] = root_hub_descr->bPwrOn2PwrGood;
	raw_descr[6] = root_hub_descr->bHubContrCurrent;
	raw_descr[7] = root_hub_descr->DeviceRemovable;
	raw_descr[8] = root_hub_descr->PortPwrCtrlMask;

	bcopy(raw_descr, message->b_wptr, wLength);
	message->b_wptr += wLength;

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, message,
		0, USB_CC_NOERROR, USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);

	return (USB_SUCCESS);
}

/*
 * uhci_handle_get_hub_status:
 */
static int
uhci_handle_get_hub_status(uhci_state_t *uhcip,
	usb_pipe_handle_impl_t	*pipe_handle,
	uint16_t		wLength)
{
	mblk_t			*message;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_handle_get_hub_status: wLength = 0x%x",
		wLength);

	ASSERT(wLength != 0);

	message = allocb(wLength, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_handle_get_hub_status: Allocb failed");

		mutex_exit(&uhcip->uhci_int_mutex);
		usba_hcdi_callback(pipe_handle,
			USB_FLAGS_SLEEP, NULL,
			0, USB_CC_UNSPECIFIED_ERR, USB_NO_RESOURCES);
		mutex_enter(&uhcip->uhci_int_mutex);

		return (USB_NO_RESOURCES);
	}

	/*
	 * A good status is always sent bcos there is no way that
	 * the driver can get to know about the status change of
	 * the over current or power failure of the root hub from
	 * the Host controller.
	 */

	bzero(message->b_wptr, wLength);
	message->b_wptr += wLength;

	mutex_exit(&uhcip->uhci_int_mutex);
	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, message,
		0, USB_CC_NOERROR, USB_SUCCESS);
	mutex_enter(&uhcip->uhci_int_mutex);

	return (USB_SUCCESS);
}

/*
 * uhci_handle_root_hub_status_change:
 *
 * This function is called every 1 second from the time out handler.
 * It checks for the status change of the root hub and its ports.
 */

void
uhci_handle_root_hub_status_change(void *arg)
{
	uhci_state_t		*uhcip = (uhci_state_t *)arg;
	uint_t			old_port_status;
	uint_t			new_port_status;
	ushort_t		port_status;
	uint_t			change_status;
	uchar_t			all_ports_status = 0;
	int			i;
	mblk_t			*message;
	usb_endpoint_descr_t	*endpoint_descr;
	size_t			length;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
	    "uhci_handle_root_hub_status_change");

	mutex_enter(&uhcip->uhci_int_mutex);

	endpoint_descr = uhcip->uhci_root_hub.root_hub_pipe_handle->p_endpoint;

	length	= endpoint_descr->wMaxPacketSize;

	ASSERT(length != 0);

	/* Check each port */
	for (i = 0; i < uhcip->uhci_root_hub.root_hub_num_ports; i++) {

		new_port_status = uhci_get_port_status(uhcip, i);

		old_port_status = uhcip->uhci_root_hub.root_hub_port_status[i];

		change_status   = (old_port_status ^ new_port_status) & 0xff;


		USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
			"port %d old status 0x%x new status 0x%x change 0x%x",
			i, old_port_status, new_port_status, change_status);

		/* See if a device was attached/detached */
		if (change_status & PORT_STATUS_CCS)
			all_ports_status |= 1 << (i + 1);

		port_status  = Get_OpReg16(PORTSC[i]);
		if (port_status & HCR_PORT_CSC)
			change_status |= 1;

		Set_OpReg16(PORTSC[i], port_status | HCR_PORT_ENDIS_CHG);
	}

	USB_DPRINTF_L3(0, uhcip->uhci_log_hdl,
		"uhci_handle_root_hub_status_change: all_ports_status = 0x%x",
		all_ports_status);

	if (uhcip->uhci_root_hub.root_hub_pipe_handle &&
		all_ports_status &&
		(uhcip->uhci_root_hub.root_hub_pipe_state ==
		PIPE_POLLING)) {

		/* Allocate a required size message block */
		message = allocb(length, BPRI_HI);

		if (message == NULL) {
			USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_handle_root_hub_status_change: Allocb failed");
			mutex_exit(&uhcip->uhci_int_mutex);
			return;
		}

		*message->b_wptr++ = all_ports_status;

		usba_hcdi_callback(uhcip->uhci_root_hub.root_hub_pipe_handle,
			0,		/* Usb_flags */
			message,	/* Mblk */
			0,		/* Flag */
			USB_CC_NOERROR,	/* Completion_reason */
			USB_SUCCESS);	/* Return value, don't care here */
	}

	/* Re-register the timeout handler */
	uhcip->uhci_timeout_id =
			timeout(uhci_handle_root_hub_status_change,
			(void *)uhcip, drv_usectohz(UHCI_ONE_SECOND));

	mutex_exit(&uhcip->uhci_int_mutex);
}

/*
 * uhci_unload_root_hub_driver:
 */

void
uhci_unload_root_hub_driver(uhci_state_t *uhcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, uhcip->uhci_log_hdl,
		"uhci_unload_root_hub_driver:");

	(void) usba_hubdi_unbind_root_hub(uhcip->uhci_dip);
}

uint_t
uhci_get_port_status(uhci_state_t *uhcip, uint_t port)
{
	ushort_t	port_status;
	uint_t		new_port_status;

	port_status = Get_OpReg16(PORTSC[port]);

	new_port_status = PORT_STATUS_PPS;

	if (port_status & HCR_PORT_CCS)
		new_port_status |= PORT_STATUS_CCS;

	if (port_status & HCR_PORT_LSDA)
		new_port_status |= PORT_STATUS_LSDA;

	if (port_status & HCR_PORT_ENABLE)
		new_port_status |= PORT_STATUS_PES;

	if (port_status & HCR_PORT_SUSPEND) {
		new_port_status |= PORT_STATUS_PSS;
	}

	if (port_status & HCR_PORT_RESET)
		new_port_status |= PORT_STATUS_PRS;

	return (new_port_status);

}
