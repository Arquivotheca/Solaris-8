/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_CONSOLE_INPUT_H
#define	_SYS_USB_CONSOLE_INPUT_H

#pragma ident	"@(#)genconsole.h	1.3	99/03/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Opaque handle which is used above the usba level.
 */
typedef struct usb_console_info		*usb_console_info_t;

/*
 * Opaque handle which is used above the ohci level.
 */
typedef struct usb_console_info_private	*usb_console_info_private_t;

/*
 * This is the structure definition for the console input handle.
 * This structure is passed down from hid and is used keep track
 * of state information for the USB OBP support.
 */
typedef struct usb_console_info_impl {
	/*
	 * The dip for the device that is going to be used as input.
	 */
	dev_info_t			*uci_dip;

	/*
	 * Private data that ohci uses for state information.
	 */
	usb_console_info_private_t	uci_private;
} usb_console_info_impl_t;

_NOTE(SCHEME_PROTECTS_DATA("Data only written during attach",
	usb_console_info_impl_t::uci_private))
_NOTE(SCHEME_PROTECTS_DATA("Data only written during attach",
        usb_console_info_impl_t::uci_dip))

/*
 * The initialization routine for handling the USB keyboard in OBP mode.
 * This routine saves off state information and calls down to the lower
 * layers to initialize any state information.
 */
int	usb_console_input_init(
	dev_info_t		*dip,
	usb_pipe_handle_t	pipe_handle,
	uchar_t			**obp_buf,
	usb_console_info_t	*console_info_handle
);

/*
 * Free up any resources that we allocated in the above initialization
 * routine.
 */
int	usb_console_input_fini(
	usb_console_info_t console_input_info
);

/*
 * This is the routine that OBP calls to save the USB state information
 * before using the USB keyboard as an input device.  This routine,
 * and all of the routines that it calls, are responsible for saving
 * any state information so that it can be restored when OBP mode is
 * over.
 */
int	usb_console_input_enter(
	usb_console_info_t	console_info_handle
);

/*
 * This is the routine that OBP calls when it wants to read a character.
 * We will call to the lower layers to see if there is any input data
 * available.
 */
int	usb_console_read(
	usb_console_info_t	console_info_handle,
	uint_t			*num_characters
);

/*
 * This is the routine that OBP calls when it is giving up control of the
 * USB keyboard.  This routine, and the lower layer routines that it calls,
 * are responsible for restoring the controller state to the state it was
 * in before OBP took control.
 */
int	usb_console_input_exit(
	usb_console_info_t	console_info_handle
);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_CONSOLE_INPUT_H */
