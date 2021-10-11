/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_UHCITGT_H
#define	_SYS_USB_UHCITGT_H

#pragma ident	"@(#)uhcitgt.h	1.10	99/10/05 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Universal Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which interfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The interface to
 * the Host Controller is defined by the Universal Host Controller Interface.
 */

#include <sys/note.h>
#include <sys/types.h>
#include <sys/pci.h>
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/hcd/uhci/uhci.h>
#include <sys/usb/hcd/uhci/uhcid.h>
#include <sys/usb/hubd/hubdvar.h>

/*
 *   Function prototypes
 */

int	uhci_hcdi_client_init(usb_device_t *usb_device);
int	uhci_hcdi_client_free(usb_device_t *usb_device);
int	uhci_hcdi_pipe_open(usb_pipe_handle_impl_t *ph, uint_t flags);
int	uhci_hcdi_pipe_close(usb_pipe_handle_impl_t *ph);
int	uhci_hcdi_pipe_reset(usb_pipe_handle_impl_t *ph,
		uint_t usb_flags);
int	uhci_hcdi_pipe_abort(usb_pipe_handle_impl_t	*ph,
		uint_t usb_flags);
int	uhci_hcdi_pipe_get_policy(usb_pipe_handle_impl_t *ph,
		usb_pipe_policy_t *policy);
int	uhci_hcdi_pipe_set_policy(usb_pipe_handle_impl_t *ph,
		usb_pipe_policy_t *policy, uint_t usb_flags);
int	uhci_hcdi_pipe_device_ctrl_receive(usb_pipe_handle_impl_t *ph,
		uchar_t	bmRequestType,	uchar_t	bRequest,	uint16_t wValue,
		uint16_t wIndex, uint16_t wLength, uint_t usb_flags);
int	uhci_hcdi_pipe_device_ctrl_send(usb_pipe_handle_impl_t *ph,
		uchar_t bmRequestType, uchar_t bRequest, uint16_t wValue,
		uint16_t wIndex, uint16_t wLength, mblk_t *data,
		uint_t usb_flags);
int	uhci_hcdi_bulk_transfer_size(dev_info_t *dip, size_t *size);
int	uhci_hcdi_pipe_receive_bulk_data(usb_pipe_handle_impl_t *ph,
		size_t length,	uint_t usb_flags);
int	uhci_hcdi_pipe_send_bulk_data(usb_pipe_handle_impl_t *ph,
		mblk_t *data, uint_t usb_flags);
int	uhci_hcdi_pipe_start_polling(usb_pipe_handle_impl_t *ph,
		uint_t flags);
int	uhci_hcdi_pipe_stop_polling(usb_pipe_handle_impl_t *ph,
		uint_t flags);
int	uhci_hcdi_pipe_send_isoc_data(usb_pipe_handle_impl_t *ph,
		mblk_t *data,	uint_t usb_flags);


/*
**  External function declarations
*/


extern void	uhci_remove_qh(uhci_state_t *uhcip,
					uhci_pipe_private_t *pp);
extern void	uhci_handle_intr_td(uhci_state_t *uhcip, gtd *td);
extern queue_head_t
			*uhci_alloc_queue_head(uhci_state_t *uhcip);
extern uhci_state_t
			*uhci_obtain_state(dev_info_t *dip);
extern void	uhci_insert_qh(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *pipe_handle);
extern void	uhci_modify_td_active_bits(uhci_state_t *uhcip,
				queue_head_t *qh);
extern int	uhci_allocate_bandwidth(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *pipe_handle);
extern void	uhci_deallocate_bandwidth(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *pipe_handle);
extern int	uhci_common_ctrl_routine(usb_pipe_handle_impl_t *pipe_handle,
				uchar_t bmRequestType,
				uchar_t bRequest,
				uint16_t wValue,
				uint16_t wIndex,
				uint16_t wLength,
				mblk_t *data,
				uint_t usb_flags);
extern int	uhci_common_bulk_routine(usb_pipe_handle_impl_t *ph,
				size_t length,
				mblk_t *data,
				uint_t usb_flags);

extern int	uhci_insert_intr_td(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint_t flags,
				uhci_handler_function_t tw_handle_td,
				usb_opaque_t tw_handle_callback_value);

extern void	uhci_remove_tds_tws(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *ph);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_UHCITGT_H */
