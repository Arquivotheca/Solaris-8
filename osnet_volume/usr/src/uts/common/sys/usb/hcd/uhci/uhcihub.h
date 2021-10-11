/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_UHCIHUB_H
#define	_SYS_USB_UHCIHUB_H

#pragma ident	"@(#)uhcihub.h	1.11	99/10/05 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Universal Serial BUS  Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which inetrfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The inetrface to
 * the Host Controller is defined by the Universal Host Controller Interface.
 */

#include	<sys/note.h>
#include	<sys/types.h>
#include	<sys/pci.h>
#include	<sys/usb/usba.h>
#include	<sys/usb/usba/usba_types.h>
#include	<sys/usb/hcd/uhci/uhci.h>
#include	<sys/usb/hcd/uhci/uhcid.h>
#include	<sys/usb/hubd/hubdvar.h>


/*
**  Function Prototypes
*/

void uhci_init_root_hub(uhci_state_t *uhcip);
int  uhci_load_root_hub_driver(uhci_state_t *uhcip);
void uhci_handle_root_hub_status_change(void *uhcip);
void uhci_unload_root_hub_driver(uhci_state_t *uhcip);
uint_t uhci_get_port_status(uhci_state_t *uhcip, uint_t port);

int  uhci_handle_root_hub_request(usb_pipe_handle_impl_t  *pipe_handle,
				uchar_t	bmRequestType,
				uchar_t	bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t	*data,
				uint_t	usb_flags);

static	int	uhci_handle_set_clear_port_feature(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t	bRequest,
				uint16_t	wValue,
				uint16_t	port);

static	void	uhci_handle_port_power(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	port,	uint_t	on);

static	void	uhci_handle_port_enable_disable(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	port,	uint_t	on);

static	void	uhci_handle_port_reset(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	port);

static	int	uhci_handle_complete_port_reset(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	port);

static	void	uhci_handle_clear_port_connection(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	port);

static	int	uhci_handle_get_port_status(uhci_state_t	*uhcip,
				uint16_t	port,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	wLength);

static	int	uhci_handle_get_hub_descriptor(uhci_state_t	*uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t	wLength);
static int	uhci_handle_get_hub_status(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint16_t		wLength);


extern	uhci_state_t	*uhci_obtain_state(dev_info_t	*dip);


#define	UHCI_DISABLE_PORT	0
#define	UHCI_ENABLE_PORT	1
#define	UHCI_CLEAR_ENDIS_BIT	2

#define	UHCI_ENABLE_PORT_PWR	1
#define	UHCI_DISABLE_PORT_PWR	0

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_UHCIHUB_H */
