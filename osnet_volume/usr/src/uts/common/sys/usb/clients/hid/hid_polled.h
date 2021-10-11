/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*
 * HID : This header file defines  project private interfaces between
 * the USB keyboard module (usbkbm) and hid.
 */

#ifndef _SYS_USB_HID_POLLED_H
#define	_SYS_USB_HID_POLLED_H

#pragma ident	"@(#)hid_polled.h	1.1	99/02/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * These are project private interfaces between the USB keyboard
 * module (usbkbm) and hid.
 */

/*
 * These two messages are sent from usbkbm to hid to get and
 * release the hid_polled_input_callback structure.
 */
#define	HID_OPEN_POLLED_INPUT		0x1001
#define	HID_CLOSE_POLLED_INPUT	0x1002

/*
 * The version of this structure.  Increment this value if you change
 * the structure.
 */
#define	HID_POLLED_INPUT_V0		0

/*
 * Opaque handle.
 */
typedef struct hid_polled_handle	*hid_polled_handle_t;

typedef struct hid_polled_input_callback {

	/*
	 * Structure version.
	 */
	unsigned		hid_polled_version;

	/*
	 * This routine is called when we are entering polled mode.
	 */
	int		(*hid_polled_input_enter)(hid_polled_handle_t);

	/*
	 * This is the routine used to read characters in polled mode.
	 */
	int		(*hid_polled_read)(hid_polled_handle_t,
					    uchar_t **);

	/*
	 * This routine is called when we are exiting polled mode.
	 */
	int		(*hid_polled_input_exit)(hid_polled_handle_t);

	/*
	 * Only one hid instance is allowed to be the console input
	 */
	int			hid_polled_instance;

	/*
	 * Opaque handle used by hid.
	 */
	hid_polled_handle_t	hid_polled_input_handle;
} hid_polled_input_callback_t;

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HID_POLLED_H */
