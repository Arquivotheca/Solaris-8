/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident  "@(#)genconsole.c 1.3     99/02/17 SMI"

/*
 * USBA: Solaris USB Architecture support
 *
 * ISSUES:
 */
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>

/*
 * Initialize USB OBP support.  This routine calls down to the lower
 * layers to initialize any state information.
 */
int
usb_console_input_init(
dev_info_t		*dip,
usb_pipe_handle_t	pipe_handle,
uchar_t			**obp_buf,
usb_console_info_t	*console_input_info
)
{
	usb_device_t			*usb_device;
	usb_console_info_impl_t		*usb_console_input;
	int				ret;

	usb_console_input = kmem_zalloc(sizeof (struct usb_console_info_impl),
		KM_SLEEP);

	if (usb_console_input == NULL) {

		return (USB_FAILURE);
	}

	/*
	 * Save the dip
	 */
	usb_console_input->uci_dip = dip;

	/*
	 * Translate the dip into a device.
	 */
	usb_device = usba_get_usb_device(dip);

	/*
	 * Call the lower layer to initialize any state information
	 */
	ret = usb_device->usb_hcdi_ops->usb_hcdi_console_input_init(
		(usb_pipe_handle_impl_t *)pipe_handle, obp_buf,
		usb_console_input);

	if (ret == USB_FAILURE) {

		kmem_free(usb_console_input,
			sizeof (struct usb_console_info_impl));
	}

	*console_input_info = (usb_console_info_t)usb_console_input;

	return (ret);
}

/*
 * Free up any resources that we allocated in the above initialization
 * routine.
 */
int
usb_console_input_fini(usb_console_info_t console_input_info)
{
	usb_console_info_impl_t		*usb_console_input;
	usb_device_t			*usb_device;
	int				ret;

	usb_console_input = (usb_console_info_impl_t *)console_input_info;

	/*
	 * Translate the dip into a device.
	 */
	usb_device = usba_get_usb_device(usb_console_input->uci_dip);

	/*
	 * Call the lower layer to free any state information.
	 */
	ret = usb_device->usb_hcdi_ops->usb_hcdi_console_input_fini(
		usb_console_input);

	if (ret == USB_FAILURE) {

		return (ret);
	}

	/*
	 * We won't be needing this information anymore.
	 */
	kmem_free(usb_console_input, sizeof (struct usb_console_info_impl));

	return (USB_SUCCESS);
}

/*
 * This is the routine that OBP calls to save the USB state information
 * before using the USB keyboard as an input device.  This routine,
 * and all of the routines that it calls, are responsible for saving
 * any state information so that it can be restored when OBP mode is
 * over.  At this layer, this code is mainly just a pass through.
 *
 * Warning:  this code runs in polled mode.
 */
int
usb_console_input_enter(usb_console_info_t console_input_info)
{
	usb_device_t				*usb_device;
	usb_console_info_impl_t			*usb_console_input;

	usb_console_input = (usb_console_info_impl_t *)console_input_info;

	/*
	 * Translate the dip into a device.
	 * Do this by directly looking at the dip, do not call
	 * usba_get_usb_device() because this function calls into the DDI.
	 * The ddi then tries to acquire a mutex and the machine hard hangs.
	 */
	usb_device = usba_polled_get_usb_device(usb_console_input->uci_dip);

	/*
	 * Call the lower layer to save state information.
	 */
	usb_device->usb_hcdi_ops->usb_hcdi_console_input_enter(
		usb_console_input);

	return (USB_SUCCESS);
}

/*
 * This is the routine that OBP calls when it wants to read a character.
 * We will call to the lower layers to see if there is any input data
 * available.  At this layer, this code is mainly just a pass through.
 *
 * Warning: This code runs in polled mode.
 */
int
usb_console_read(usb_console_info_t console_input_info,
	uint_t *num_characters)
{
	usb_device_t				*usb_device;
	usb_console_info_impl_t			*usb_console_input;

	usb_console_input = (usb_console_info_impl_t *)console_input_info;

	/*
	 * Translate the dip into a device.
	 * Do this by directly looking at the dip, do not call
	 * usba_get_usb_device() because this function calls into the DDI.
	 * The ddi then tries to acquire a mutex and the machine hard hangs.
	 */
	usb_device = usba_polled_get_usb_device(usb_console_input->uci_dip);

	/*
	 * Call the lower layer to get a a character.  Return the number
	 * of characters read into the buffer.
	 */
	return (usb_device->usb_hcdi_ops->usb_hcdi_console_read(
		usb_console_input, num_characters));
}

/*
 * This is the routine that OBP calls when it is giving up control of the
 * USB keyboard.  This routine, and the lower layer routines that it calls,
 * are responsible for restoring the controller state to the state it was
 * in before OBP took control. At this layer, this code is mainly just a
 * pass through.
 *
 * Warning: This code runs in polled mode.
 */
int
usb_console_input_exit(usb_console_info_t console_input_info)
{
	usb_device_t				*usb_device;
	usb_console_info_impl_t			*usb_console_input;

	usb_console_input = (usb_console_info_impl_t *)console_input_info;

	/*
	 * Translate the dip into a device.
	 * Do this by directly looking at the dip, do not call
	 * usba_get_usb_device() because this function calls into the DDI.
	 * The ddi then tries to acquire a mutex and the machine hard hangs.
	 */
	usb_device = usba_polled_get_usb_device(usb_console_input->uci_dip);

	/*
	 * Restore the state information.
	 */
	usb_device->usb_hcdi_ops->usb_hcdi_console_input_exit(
		usb_console_input);

	return (USB_SUCCESS);
}
