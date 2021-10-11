/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_UHCI_POLLED_H
#define	_SYS_USB_UHCI_POLLED_H

#pragma ident	"@(#)uhcipolled.h	1.1	99/08/05 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This header file describes the data structures required for the Host
 * Controller Driver (HCD) to work in POLLED mode which will be  either
 * OBP mode for Sparc architecture or PC PROM mode for X86 architecture
 */



#include <sys/note.h>
#include <sys/types.h>
#include <sys/pci.h>
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/hcd/uhci/uhci.h>
#include <sys/usb/hcd/uhci/uhcid.h>
#include <sys/sunddi.h>

#define	POLLED_RAW_BUF_SIZE	8

/*
 * These two  flags are used to determine if this structure is already
 * in use.
 */
#define	POLLED_INPUT_MODE		0x01

/*
 * These two flags are used to determine if this structure is already in
 * use. We should only save off the controller state information once,
 * restore it once.  These flags are used for the uhci_polled_flags below.
 */
#define	POLLED_INPUT_MODE_INUSE		0x04

/*
 * State structure for the POLLED switch off
 */
typedef struct uhci_polled {

	/*
	 * Pointer to the uhcip structure for the device that is to  be
	 * used as input in polled mode.
	 */
	uhci_state_t *uhci_polled_uhcip;

	/*
	 * Pipe handle for the pipe that is to be used as input device
	 * in POLLED mode.
	 */
	usb_pipe_handle_impl_t  *uhci_polled_ph;

	/* Interrupt Endpoint descriptor */
	queue_head_t		*uhci_polled_qh;

	/* Transfer descriptor for polling the device */
	gtd			*uhci_polled_td;
	/*
	 * The buffer that the usb scancodes are copied into.
	 */
	uchar_t		*uhci_polled_buf;

	/*
	 * This flag is used to determine if the state of the controller
	 * has already been saved (enter) or doesn't need to be restored
	 * yet (exit).
	 */
	uint_t			uhci_polled_flags;
	frame_lst_table_t	uhci_polled_save_IntTble[1024];

	ushort_t		uhci_polled_entry;

} uhci_polled_t;


/*
 * POLLED entry points
 *
 * These functions are entry points into the POLLED code.
 */
int	uhci_hcdi_polled_input_init(usb_pipe_handle_impl_t *,
	uchar_t **, usb_console_info_impl_t *);
int	uhci_hcdi_polled_input_fini(usb_console_info_impl_t *);
int	uhci_hcdi_polled_input_enter(usb_console_info_impl_t *);
int	uhci_hcdi_polled_input_exit(usb_console_info_impl_t *);
int	uhci_hcdi_polled_read(usb_console_info_impl_t *, uint_t *);


/*
 * Internal Function Prototypes
 */

/* Polled initialization routines */
static int	uhci_polled_init(usb_pipe_handle_impl_t *pipe_handle,
		uhci_state_t *uhcip,
		usb_console_info_impl_t *console_input_info);

/* Polled deinitialization routines */
static int	uhci_polled_fini(uhci_polled_t *uhci_polledp,
		uhci_state_t *uhcip);

/* Polled save state routines */
static void	uhci_polled_save_state(uhci_polled_t *uhci_polledp);

/* Polled restore state routines */
static void	uhci_polled_restore_state(uhci_polled_t *uhci_polledp);

/* Polled read routines */
static int	uhci_polled_insert_td_on_qh(uhci_polled_t *uhcip,
			usb_pipe_handle_impl_t *ph);


static uhci_trans_wrapper_t *
		uhci_polled_create_tw(uhci_state_t *);

/*
 * External Function Prototypes
 */

/*
 * These routines are only called from the init and fini functions.
 * They are allowed to acquire locks.
 */
extern uhci_state_t	*uhci_obtain_state(dev_info_t *dip);
extern queue_head_t	*uhci_alloc_queue_head(uhci_state_t *uhcip);
extern void	uhci_free_tw(uhci_state_t *uhcip, uhci_trans_wrapper_t *tw);
extern gtd *uhci_allocate_td_from_pool(uhci_state_t *uhcip);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_UHCI_POLLED_H */
