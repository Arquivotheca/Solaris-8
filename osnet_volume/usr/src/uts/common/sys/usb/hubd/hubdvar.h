/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_HUBDVAR_H
#define	_SYS_USB_HUBDVAR_H

#pragma ident	"@(#)hubdvar.h	1.6	99/09/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * HUB USB device state management :
 *
 *	ONLINE-----1--->SUSPENDED----2---->ONLINE
 *	  |
 *	  +-----3--->DISCONNECTED----4----->RECOVER---11--->ONLINE
 *	  |
 *	  +-----7--->POWERED DOWN----8----->ONLINE
 *	  |
 *	  +-----9--->CHILD PWRLVL----10---->ONLINE
 *			|
 *			+-----3------------>DISCONNECTED
 *
 *	POWERED DOWN----1--->SUSPENDED------2----->POWERED DOWN
 *	  |		      |     ^
 *	  |		      5     |
 *	  |		      |     6
 *	  |		      v     |
 *	  +---------3----->DISCONNECTED---4---->RECOVER---11--->POWERED DOWN
 *
 *	1 = CPR SUSPEND
 *	2 = CPR RESUME (with original device)
 *	3 = Device Unplug
 *	4 = Original Device Plugged in
 *	5 = CPR RESUME (with device disconnected or with a wrong device)
 *	6 = CPR SUSPEND on a disconnected device
 *	7 = Device idles for time T & transitions to low power state
 *	8 = Remote wakeup by device OR Application kicking off IO to device
 *	9 = Hub detect child doing remote wakeup and request the PM
 *	    framework to bring it to full power
 *     10 = PM framework has compeleted call power entry point of the child
 *	    and bus ctls of hub
 *     11 = Restoring states of its children i.e. set addrs & config.
 *
 */

#define	HUBD_INITIAL_SOFT_SPACE	4

typedef struct hub_power_struct {
	void		*hubp_hubd;	/* points back to hubd_t */

	uint8_t		hubp_wakeup_enabled;	/* remote wakeup enabled? */

	/* this is the bit mask of the power states that device has */
	uint8_t		hubp_pwr_states;

	/* wakeup and power transistion capabilites of an interface */
	uint8_t		hubp_pm_capabilities;

	/* flag to indicate if driver is about to raise power level */
	boolean_t	hubp_raise_power;

	uint8_t		hubp_current_power;	/* current power level */

	/* power state of all children are tracked here */
	uint8_t		*hubp_child_pwrstate;

	/* pm-components properties are stored here */
	char		*hubp_pmcomp[5];

	usb_config_pwr_descr_t	hubp_confpwr_descr; /* config pwr descr */
} hub_power_t;

/* warlock directives, stable data */
_NOTE(DATA_READABLE_WITHOUT_LOCK(hub_power_t::hubp_hubd))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hub_power_t::hubp_wakeup_enabled))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hub_power_t::hubp_pwr_states))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hub_power_t::hubp_pm_capabilities))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hub_power_t::hubp_pmcomp))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hub_power_t::hubp_confpwr_descr))


/*
 * soft	state information for this hubd
 */
typedef struct hubd {
	int		h_instance;

	uint_t		h_init_state;

	uint_t		h_dev_state;

	hub_power_t	*h_hubpm;	/* pointer to power struct */

	/*
	 * dev_info_t reference
	 */
	dev_info_t	*h_dip;

	/*
	 * mutex to protect softstate and hw regs
	 */
	kmutex_t	h_mutex;

	/*
	 * save the usb_device pointer
	 */
	usb_device_t	*h_usb_device;

	/*
	 * Transport structure for this	instance of the	hba
	 */
	usb_hubdi_ops_t	*h_hubdi_ops;

	int		h_softstate;

	/*
	 * default pipe handle
	 */
	usb_pipe_handle_t	h_default_pipe;
	kcondvar_t		h_cv_default_pipe;

	/*
	 * pipe handle for ep1
	 */
	usb_pipe_handle_t	h_ep1_ph;
	usb_endpoint_descr_t	h_ep1_descr;
	usb_pipe_policy_t	h_pipe_policy;
	uint_t			h_intr_pipe_state;

	/*
	 * root hub descriptor
	 */
	struct usb_hub_descr 	h_hub_descr;

	/*
	 * hotplug handling
	 */
	uint_t			h_hotplug_thread;

	/*
	 * h_children_dips is a  array for holding
	 * each child dip indexed by port
	 * h_usb_devices is the corresponding usb_device
	 */
	dev_info_t		**h_children_dips;
	size_t			h_cd_list_length;
	usb_device_t		**h_usb_devices;

	/*
	 * the following 5 variables are all bit masks. we restrict
	 * the number of ports to 7 for now
	 */
	uchar_t			h_port_change; /* change reported by hub */
					/* and used by hotplug thread */

	uchar_t			h_port_connected; /* currently connected */
	uchar_t			h_port_powered;	/* port is powered */
	uchar_t			h_port_enabled;	/* port is enabled */

	uchar_t			h_port_reset_wait; /* waiting for reset */
						/* completion callback */
	kcondvar_t		h_cv_reset_port;
	kcondvar_t		h_cv_reset_intrpipe;
	uint_t			h_intr_completion_reason;
	struct usb_dump_ops	*h_dump_ops;	/* for dump support */
	usb_log_handle_t	h_log_handle;	/* for logging msgs */

	ndi_event_hdl_t		h_ndi_event_hdl;
	ddi_eventcookie_t	h_remove_cookie;
	ddi_eventcookie_t	h_insert_cookie;

	/*
	 * Hotplug event statistics since hub was attached
	 */
	ulong_t			h_total_hotplug_success;
	ulong_t			h_total_hotplug_failure;
} hubd_t;

typedef unsigned short	usb_port_t;

_NOTE(MUTEX_PROTECTS_DATA(hubd::h_mutex, hubd))
_NOTE(MUTEX_PROTECTS_DATA(hubd::h_mutex, hub_power_t))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hubd::h_default_pipe
		hubd::h_ndi_event_hdl
		hubd::h_log_handle
		hubd::h_ep1_ph
		hubd::h_instance
		hubd::h_hubpm
		hubd::h_dip))

#define	HUBD_UNIT(dev)		(getminor((dev)))
#define	HUBD_MUTEX(hubd) 	(&((hubd)->h_mutex))
#define	HUBD_SS_ISOPEN		0x0001
#define	HUBD_ACK_CHANGES	0x0001

/* init state */
#define	HUBD_LOCKS_DONE		0x0001
#define	HUBD_ATTACH_DONE 	0x0002
#define	HUBD_MINOR_NODE_CREATED 0x0004
#define	HUBD_EVENTS_REGISTERED	0x0020

/* This dev state is used exclusively by hub to change port suspend/resume */
#define	USB_DEV_HUB_CHILD_PWRLVL	0x80
#define	USB_DEV_HUB_DISCONNECT_RECOVER	0x81

/*
 * hubd interrupt pipe management :
 *
 * Following are the states of the interrupt pipe
 *
 * OPEN :
 *	Set when the pipe is opened by call to hubd_start_polling. This is
 *	typically after a hub has got enumerated and initialised.
 *
 * CLOSING :
 *	Set when the pipe is closed by call to hubd_stop_polling. This is
 *	typically called on hub disconnect via hubd_cleanup.
 *
 * RESETTING :
 *	Set when we need to do reset on the pipe. This is when we get an error
 *	on the pipe in hubd_exception_callback. We reset the pipe only if its
 *	state is OPEN.
 *
 * State diagram for interrupt pipe state :
 *
 *			+---<--hubd_start_polling--<----+
 *			|     				|
 *			|    +--->hubd_cleanup-->--->[CLOSING]
 *			v    |
 * hubd_start_polling[OPEN]--+
 *			^    |
 *			|    +--->hubd_exception_callback-->--->[RESETTING]
 *			|     						|
 *			+---------<--hubd_intr_pipe_reset_callback--<---+
 */
#define	HUBD_INTR_PIPE_OPEN		1
#define	HUBD_INTR_PIPE_RESETTING	2
#define	HUBD_INTR_PIPE_CLOSING		3

/*
 * Debug printing
 * Masks
 */
#define	DPRINT_MASK_ATTA 	0x00000001
#define	DPRINT_MASK_CBOPS 	0x00000002
#define	DPRINT_MASK_CALLBACK 	0x00000004
#define	DPRINT_MASK_PORT 	0x00000008
#define	DPRINT_MASK_HUB 	0x00000010
#define	DPRINT_MASK_HOTPLUG 	0x00000020
#define	DPRINT_MASK_EVENTS	0x00000040
#define	DPRINT_MASK_PM		0x00000080
#define	DPRINT_MASK_ALL 	0xFFFFFFFF


/* status length used in getting hub status */
#define	GET_STATUS_LENGTH	0x04		/* length of get status req */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_USB_HUBDVAR_H */
