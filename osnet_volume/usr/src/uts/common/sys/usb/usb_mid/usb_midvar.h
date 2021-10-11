/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_USB_MIDVAR_H
#define	_SYS_USB_USB_MIDVAR_H

#pragma ident	"@(#)usb_midvar.h	1.4	99/09/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


typedef struct usb_mid_power_struct {
	void		*mip_usb_mid;	/* points back to usb_mid_t */

	uint8_t		mip_wakeup_enabled;

	/* this is the bit mask of the power states that device has */
	uint8_t		mip_pwr_states;

	/* wakeup and power transistion capabilites of an interface */
	uint8_t		mip_pm_capabilities;

	/* flag to indicate if driver is about to raise power level */
	boolean_t	mip_raise_power;

	uint8_t		mip_current_power;

	/* power state of all children are tracked here */
	uint8_t		*mip_child_pwrstate;
} usb_mid_power_t;

/* warlock directives, stable data */
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_power_t::mip_usb_mid))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_power_t::mip_wakeup_enabled))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_power_t::mip_pwr_states))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid_power_t::mip_pm_capabilities))


/*
 * soft	state information for this usb_mid
 */
typedef struct usb_mid {
	int			mi_instance;

	uint_t			mi_init_state;

	kmutex_t		mi_mutex;

	/*
	 * dev_info_t reference
	 */
	dev_info_t		*mi_dip;

	/* pointer to usb_mid_power_t */
	usb_mid_power_t		*mi_pm;

	/*
	 * save the usb_device pointer
	 */
	usb_device_t		*mi_usb_device;

	int			mi_softstate;

	int			mi_dev_state;

	int			mi_n_interfaces;

	/*
	 * mi_children_dips is a  array for holding
	 * each child dip indexed by interface number
	 */
	dev_info_t		**mi_children_dips;

	size_t			mi_cd_list_length;

	/*
	 * mi_dump_ops is used for dump support
	 */
	struct usb_dump_ops	*mi_dump_ops;

	/* logging of messages */
	usb_log_handle_t	mi_log_handle;

	/* event support */
	ndi_event_hdl_t		mi_ndi_event_hdl;
	ddi_eventcookie_t	mi_remove_cookie;
	ddi_eventcookie_t	mi_insert_cookie;
} usb_mid_t;

_NOTE(MUTEX_PROTECTS_DATA(usb_mid::mi_mutex, usb_mid))
_NOTE(MUTEX_PROTECTS_DATA(usb_mid::mi_mutex, usb_mid_power_t))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_mid::mi_instance
		usb_mid::mi_ndi_event_hdl
		usb_mid::mi_log_handle
		usb_mid::mi_dip
		usb_mid::mi_pm))

#define	USB_MID_UNIT(dev)	(getminor((dev)))
#define	USB_MID_SS_ISOPEN	0x0001

/* init state */
#define	USB_MID_MINOR_NODE_CREATED	0x0001
#define	USB_MID_EVENTS_REGISTERED	0x0002

/*
 * Debug printing
 * Masks
 */
#define	DPRINT_MASK_ATTA 	0x00000001
#define	DPRINT_MASK_CBOPS 	0x00000002
#define	DPRINT_MASK_EVENTS	0x00000004
#define	DPRINT_MASK_DUMPING	0x00000008	/* usb_mid dump mask */
#define	DPRINT_MASK_PM 		0x00000010
#define	DPRINT_MASK_ALL 	0xFFFFFFFF


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_USB_USB_MIDVAR_H */
