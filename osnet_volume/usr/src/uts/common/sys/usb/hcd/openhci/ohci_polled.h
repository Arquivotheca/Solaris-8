/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_OHCI_POLLED_H
#define	_SYS_USB_OHCI_POLLED_H

#pragma ident	"@(#)ohci_polled.h	1.8	99/09/24 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This header file describes the data structures required for the Host
 * Controller Driver (HCD) to work in POLLED mode which will be  either
 * OBP mode for Sparc architecture or PC PROM mode for X86 architecture
 */

#define	POLLED_RAW_BUF_SIZE	8

/*
 * These two  flags are used to determine if this structure is already
 * in use.
 */
#define	POLLED_INPUT_MODE		0x01

/*
 * These two flags are used to determine if this structure is already in
 * use. We should only save off the controller state information once,
 * restore it once.  These flags are used for the ohci_polled_flags below.
 */
#define	POLLED_INPUT_MODE_INUSE		0x04

/*
 * State structure for the POLLED switch off
 */
typedef struct ohci_polled {

	/*
	 * Pointer to the ohcip structure for the device that is to  be
	 * used as input in polled mode.
	 */
	openhci_state_t *ohci_polled_ohcip;

	/*
	 * Saved copy of the ohci registers of the normal mode & change
	 * required ohci registers values for the polled mode operation.
	 * Before returning from the polled mode to normal mode replace
	 * the required current registers with this saved ohci registers
	 * copy.
	 */
	hcr_regs_t	ohci_polled_save_regs;

	/*
	 * Saved copy of the interrupt table used in normal ohci mode and
	 * replace this table by another interrupt table that used in the
	 * POLLED mode.
	 */
	hc_ed_t		*ohci_polled_save_IntTble[NUM_INTR_ED_LISTS];

	/*
	 * Pipe handle for the pipe that is to be used as input device
	 * in POLLED mode.
	 */
	usb_pipe_handle_impl_t  *ohci_polled_input_pipe_handle;

	/* Dummy endpoint descriptor */
	hc_ed_t		*ohci_polled_dummy_ed;

	/* Interrupt Endpoint descriptor */
	hc_ed_t		*ohci_polled_ed;	/* Interrupt endpoint */

	/*
	 * The buffer that the usb scancodes are copied into.
	 */
	uchar_t		*ohci_polled_buf;

	/*
	 * This flag is used to determine if the state of the controller
	 * has already been saved (enter) or doesn't need to be restored
	 * yet (exit).
	 */
	uint_t		ohci_polled_flags;

	/*
	 * The read or write routines may take TD's of the done head that
	 * are not intended for them.
	 */
	gtd		*ohci_polled_input_done_head;

	/*
	 * This is the tail for the above ohci_polled_input_done_head;
	 */
	gtd		*ohci_polled_input_done_tail;

	/*
	 * ohci_hcdi_polled_input_enter() may be called
	 * multiple times before the ohci_hcdi_polled_input_exit() is called.
	 * For example, the system may:
	 *	- go down to kadb (ohci_hcdi_polled_input_enter())
	 *	- down to the ok prompt, $q (ohci_hcdi_polled_input_enter())
	 *	- back to kadb, "go" (ohci_hcdi_polled_input_exit())
	 *	- back to the OS, $c at kadb (ohci_hcdi_polled_input_exit())
	 *
	 * polled_entry keeps track of how  many times
	 * ohci_polled_input_enter/ohci_polled_input_exit have been
	 * called so that the host controller isn't switched back to OS mode
	 * prematurely.
	 */
	uint_t		ohci_polled_entry;

	/*
	 * Save the pointer usb device structure and the endpoint number
	 * during the polled initilization.
	 */
	usb_device_t	*ohci_polled_usb_dev;	/* USB device */

	uint_t		ohci_polled_ep_index;	/* Endpoint index */
} ohci_polled_t;

_NOTE(SCHEME_PROTECTS_DATA("Only accessed in POLLED mode",
	ohci_polled_t::ohci_polled_flags))
_NOTE(DATA_READABLE_WITHOUT_LOCK(ohci_polled_t::ohci_polled_ohcip))
_NOTE(SCHEME_PROTECTS_DATA("Only accessed in POLLED mode",
	ohci_polled_t::ohci_polled_entry))


#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_OHCI_POLLED_H */
