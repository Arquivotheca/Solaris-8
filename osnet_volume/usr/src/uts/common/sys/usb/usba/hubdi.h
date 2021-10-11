/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_HUBDI_H
#define	_SYS_USB_HUBDI_H

#pragma ident	"@(#)hubdi.h	1.4	99/06/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* USBA calls these: */
void	usba_hubdi_initialization();
void	usba_hubdi_destruction();

#define	HUBDI_OPS_VERSION_0 	0
#define	HUBD_IS_ROOT_HUB	0x1000

typedef struct usb_hubdi_ops {
	int		hubdi_ops_version;	/* implementation version */

	/*
	 * event handling
	 */
	int	(*usb_hubdi_get_eventcookie)(
		dev_info_t		*dip,
		dev_info_t		*rdip,
		char			*eventname,
		ddi_eventcookie_t	*cookiep,
		ddi_plevel_t		*plevelp,
		ddi_iblock_cookie_t	*iblock_cookiep);

	int	(*usb_hubdi_add_eventcall)(
		dev_info_t		*dip,
		dev_info_t		*rdip,
		ddi_eventcookie_t	eventid,
		int (*event_hdlr)(dev_info_t *dip,
				ddi_eventcookie_t event, void *arg,
				void *bus_impldata),
		void			*arg);

	int	(*usb_hubdi_remove_eventcall)(
		dev_info_t		*dip,
		dev_info_t		*rdip,
		ddi_eventcookie_t	event);

	int	(*usb_hubdi_post_event)(
		dev_info_t		*dip,
		dev_info_t		*rdip,
		ddi_eventcookie_t	event,
		void			*impl_data);


} usb_hubdi_ops_t;

int usba_hubdi_open(dev_info_t *, dev_t *, int, int, cred_t *);
int usba_hubdi_close(dev_info_t *, dev_t, int, int, cred_t *);
int usba_hubdi_ioctl(dev_info_t *, dev_t, int, intptr_t, int,
	cred_t *, int *);

extern struct bus_ops usba_hubdi_busops;

/*
 * autoconfiguration data and routines.
 */
int usba_hubdi_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
				void *arg, void **result);
int usba_hubdi_attach(dev_info_t *dev, ddi_attach_cmd_t cmd);
int usba_hubdi_probe(dev_info_t *dev);
int usba_hubdi_detach(dev_info_t *dev, ddi_detach_cmd_t cmd);

int usba_hubdi_bind_root_hub(dev_info_t *dip,
	uchar_t *config_descriptor, size_t config_length,
	usb_device_descr_t *root_hub_device_descriptor);
int usba_hubdi_unbind_root_hub(dev_info_t *dip);


/*
 * Creating/Destroying children (root hub, and hub children)
 */
int	usba_create_child_devi(
		dev_info_t		*dip,
		char			*node_name,
		usb_hcdi_ops_t		*usb_hcdi_ops,
		dev_info_t		*usb_root_hub_dip,
		usb_hubdi_ops_t		*usb_hubdi_ops,
		usb_port_status_t	port_status,
		usb_device_t		*usb_device,
		dev_info_t		**child_dip);

int usba_destroy_child_devi(dev_info_t *dip, uint_t flag);

/*
 * Driver binding functions
 */
dev_info_t *usba_bind_driver_to_device(dev_info_t *child_dip, uint_t flag);
dev_info_t *usba_bind_driver_to_interface(dev_info_t *dip, uint_t interface,
    uint_t flag);

#define	USBA_BIND_ATTACH	1

/*
 * Allocating a USB address
 */
#define	USB_MAX_ADDRESS		127
#define	USB_ADDRESS_ARRAY_SIZE	((USB_MAX_ADDRESS+8)/8)

int usba_set_usb_address(usb_device_t *usb_device);
void usba_unset_usb_address(usb_device_t *usb_device);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HUBDI_H */
