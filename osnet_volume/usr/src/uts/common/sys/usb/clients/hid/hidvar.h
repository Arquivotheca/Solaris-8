/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_HIDVAR_H
#define	_SYS_USB_HIDVAR_H

#pragma ident	"@(#)hidvar.h	1.6	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * HID : This header file contains the internal structures
 * and variable definitions used in hid driver.
 */

/*
 * HID USB device state management :
 *
 *	ONLINE-----1--->SUSPENDED----2---->ONLINE
 *	  |
 *	  +-----3--->DISCONNECTED----4----->ONLINE
 *	  |
 *	  +-----7--->POWERED DOWN----8----->POWER CHANGE---9--->ONLINE
 *						|
 *						+---3--->DISCONNECTED
 *
 *	POWERED DOWN----1--->SUSPENDED------2----->POWERED DOWN
 *	  |		      |     ^
 *	  |		      5     |
 *	  |		      |     6
 *	  |		      v     |
 *	  +---------3----->DISCONNECTED-------4----->POWERED DOWN
 *
 *	1 = CPR SUSPEND
 *	2 = CPR RESUME (with original device)
 *	3 = Device Unplug
 *	4 = Original Device Plugged in
 *	5 = CPR RESUME (with device disconnected or with a wrong device)
 *	6 = CPR SUSPEND on a disconnected device
 *	7 = Device idles for time T & transitions to low power state
 *	8 = Remote wakeup by device OR Application kicking off IO to device
 *          This results in a Transistion state till PM calls the power
 *	    entry point to raise the power level of the device
 *	9 = Device entry point called to raise power level of the device
 *
 */


/* Boot protocol values for keyboard and mouse */
#define	KEYBOARD_PROTOCOL	0x01		/* legacy keyboard */
#define	MOUSE_PROTOCOL		0x02		/* legacy mouse */


/*
 * If the hid descriptor is not valid, the following values are
 * used.
 */
#define	USBKPSZ 8			/* keyboard packet size */
#define	USBMSSZ 3			/* mouse packet size */
#define	USB_KB_HID_DESCR_LENGTH 0x3f 	/* keyboard Report descr length */
#define	USB_MS_HID_DESCR_LENGTH 0x32 	/* mouse Report descr length */

/*
 * Hid default pipe states. Default pipe
 * can be in only one of these four states -
 * open, close pending, reset_pening, closed. The
 * state diagram is like this :
 *
 *	open----1--->close pending--2----->closed---3-->open
 *	 |
 *	 |-----4---->reset_pending--5-->open
 *	       |
 *	       |--6---->close pending--7---->closed--3-->open
 *
 *	1 = default pipe callback
 *	2 = default pipe close callback
 *	3 = hid_mctl_receive
 *	4 = default pipe exception callback
 *	5 = default pipe reset callback(on STALL)
 *	6 = default pipe reset callback(on other error)
 *	7 = default pipe close callback
 */
#define	HID_DEFAULT_PIPE_CLOSED	0x00 	/* default pipe is closed. */
#define	HID_DEFAULT_PIPE_OPEN	0x01 	/* default pipe is opened. */

/* Close for default pipe is in progress */
#define	HID_DEFAULT_PIPE_CLOSE_PENDING	0x02

/* Reset for default pipe is in progress */
#define	HID_DEFAULT_PIPE_RESET_PENDING	0x03

/* pipe policy */
#define	HID_PP_BYTES_PER_PACKET	25	/* Bytes per packet */

/*
 * Hid interrupt pipe states. Interrupt pipe
 * can be in only one of these states :
 *
 *	open--1-->data_transferring--1-->open
 *	 |
 *	 |----3--->reset_pending---4--->open
 *	 |
 *	 |----2---->closed
 *
 *	1 = interrupt pipe callback
 *	2 = hid_close
 *	3 = interrupt pipe exception callback
 *	4 = interrupt pipe reset callback
 */

#define	HID_INTERRUPT_PIPE_CLOSED		0x00 /* Int. pipe is closed */
#define	HID_INTERRUPT_PIPE_OPEN			0x01 /* Int. pipe is opened */
#define	HID_INTERRUPT_PIPE_RESET_PENDING	0x02 /* Reset has been issued */

/* Data is being sent up */
#define	HID_INTERRUPT_PIPE_DATA_TRANSFERRING	0x03

/* Attach/detach states */
#define	HID_ATTACH_INIT		0x00	/* Initial attach state */
#define	HID_MINOR_NODES		0x02 	/* Set after minor node is created */

/* HID Protocol Requests */
#define	SET_IDLE 0x0a 		/* bRequest value to set idle request */
#define	DURATION 0<<8   	/* no. of repeat reports.  see HID 7.2.4 */
#define	SET_PROTOCOL 0x0b 	/* bRequest value to set to boot protocol */

typedef struct hid_power_struct {

	void			*hid_state;	/* points back to hid_state */

	uint8_t			hid_wakeup_enabled;

	/* this is the bit mask of the power states that device has */
	uint8_t			hid_pwr_states;

	/* wakeup and power transistion capabilites of an interface */
	uint8_t			hid_pm_capabilities;

	/* flag to indicate if driver is about to raise power level */
	boolean_t		hid_raise_power;

	/* current power level the device is in */
	uint8_t			hid_current_power;
} hid_power_t;

_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_power_t::hid_state))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_power_t::hid_wakeup_enabled))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_power_t::hid_pwr_states))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_power_t::hid_pm_capabilities))


typedef struct hid_state_struct {
	dev_info_t		*hid_dip;	/* per-device info handle */
	kmutex_t		hid_mutex;	/* for general locking */
	int			hid_instance;	/* instance number */

	/* Attach/detach flags */
	int			hid_attach_flags;

	/* device state flag */
	int			hid_dev_state;

	/* flags for states of hid_default pipe */
	int			hid_default_pipe_flags;

	/* flags for states of hid_interrupt pipe */
	int			hid_interrupt_pipe_flags;

	queue_t			*hid_rq_ptr;	/* pointer to read queue */
	queue_t			*hid_wq_ptr;	/* pointer to write queue */

	hid_power_t		*hid_pm;	/* ptr to power struct */

	usb_config_descr_t	hid_config_descr;	/* configuration des. */

	/* hid driver is attached to this interface */
	int			hid_interfaceno;

	usb_interface_descr_t	hid_interface_descr;	/* interface descr */
	usb_hid_descr_t		hid_hid_descr;		/* hid descriptor */
	usb_endpoint_descr_t	hid_interrupt_ept_descr; /* intr ept descr */
	hidparser_handle_t	hid_report_descr;	/* report descr */

	usb_pipe_handle_t	hid_default_pipe;	/* default pipe */
	usb_pipe_handle_t	hid_interrupt_pipe;	/* intr pipe handle  */

	int			hid_streams_flags;	/* see below */
	int			hid_packet_size;	/* data packet size */

	/* Pipe policy for the default pipe is saved here */
	usb_pipe_policy_t	hid_default_pipe_policy;

	/* Pipe policy for the interrupt pipe is saved here */
	usb_pipe_policy_t	hid_intr_pipe_policy;

	/*
	 * This field is only used if the device provides polled input
	 * This is state information for the usba layer.
	 */
	usb_console_info_t	hid_polled_console_info;

	/*
	 * This is the buffer that the raw characters are stored in.
	 * for polled mode.
	 */
	uchar_t			*hid_polled_raw_buf;

	/* timeout id for enabling queue after a specified timeout */
	timeout_id_t		hid_timeout_id;

	/* Condition variable for the default pipe states */
	kcondvar_t		hid_cv_default_pipe;

	/* Condition variable for the interrupt pipe states */
	kcondvar_t		hid_cv_interrupt_pipe;

	/* handle for outputting messages */
	usb_log_handle_t	hid_log_handle;

	/* event support */
	ddi_eventcookie_t	hid_remove_cookie;
	ddi_eventcookie_t	hid_insert_cookie;

} hid_state_t;

/* warlock directives, stable data */
_NOTE(MUTEX_PROTECTS_DATA(hid_state_t::hid_mutex, hid_state_t))
_NOTE(MUTEX_PROTECTS_DATA(hid_state_t::hid_mutex, hid_power_t))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_state_t::hid_dip))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_state_t::hid_pm))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_state_t::hid_instance))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_state_t::hid_interrupt_pipe))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_state_t::hid_default_pipe))
_NOTE(DATA_READABLE_WITHOUT_LOCK(hid_state_t::hid_log_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(
	hid_state_struct::hid_interface_descr))

/*
 * The hid_polled_console_info field is a handle from usba.  The
 * handle is used when the kernel is in the single thread mode
 * so the field is tagged with this note.
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per call", 
				hid_state_t::hid_polled_console_info))

/*
 * structure for argument for callback routine for async
 * data transfer through default pipe.
 */
typedef struct hid_default_pipe_argument {

	/* Message to be sent up to the stream */
	struct iocblk	hid_default_pipe_arg_mctlmsg;

	/* Pointer to hid_state structure */
	hid_state_t	*hid_default_pipe_arg_hidp;

	/* Pointer to the original mblk_t received from hid_wput() */
	mblk_t		*hid_default_pipe_arg_mblk;

	/* Request that caused this callback to happen */
	uchar_t		hid_default_pipe_arg_bRequest;

	/* No. of stalls for a particular command */
	int		hid_default_pipe_arg_stallcount;

} hid_default_pipe_arg_t;

/*
 * An instance of this structure is created per command down to the
 * device.  The control callback is not executed until the call is
 * made into usba, so there is no danger of a callback happening when
 * the fields of the structure are being set.
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per call", hid_default_pipe_arg_t))

/*
 * structure for argument for callback routine of async
 * pipe reset.
 */
typedef struct hid_pipe_reset_argument {

	/* Pointer to hid_state structure */
	hid_state_t	*hid_pipe_reset_arg_hidp;

	/*
	 * Pointer to hid_default_pipe_arg_t structure.
	 * Required to modify the bRequest field in
	 * hid_default_pipe_reset_callback before sending
	 * a CLEAR FEATURE command and to keep track
	 * of stallcount.
	 */
	hid_default_pipe_arg_t	*hid_pipe_reset_arg_defaultp;

	/* Completion reason that caused this pipe reset */
	uint_t		hid_pipe_reset_arg_exception_cr;

} hid_pipe_reset_arg_t;

/*
 * An instance of this structure is created per command down to the
 * device.  The callback is not executed until the call is
 * made into usba, so there is no danger of a callback happening when
 * the fields of the structure are being set.
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per call", hid_pipe_reset_arg_t))

/* Value for hid_streams_flags */
#define	HID_STREAMS_OPEN		0x00000001	/* Streams are open */
#define	HID_STREAMS_DISMANTLING		0x00000002	/* In hid_close() */

#define	HID_BAD_DESCR	0x01		/* Bad hid report descriptor */

#define	HID_MINOR_NAME_LEN	20	/* Max length of minor_name string */

/* hid_close will wait 60 secons for callbacks to be over */
#define	HID_CLOSE_WAIT_TIMEOUT	hz*60

/*
 * Debug message Masks
 */
#define	PRINT_MASK_ATTA		0x00000001
#define	PRINT_MASK_OPEN 	0x00000002
#define	PRINT_MASK_CLOSE	0x00000004
#define	PRINT_MASK_EVENTS	0x00000008
#define	PRINT_MASK_PM		0x00000010
#define	PRINT_MASK_ALL		0xFFFFFFFF

/*
 * Define states local to hid driver
*/
#define	USB_DEV_HID_POWER_CHANGE	0x80

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HIDVAR_H */
