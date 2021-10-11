/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_HCDI_H
#define	_SYS_USB_HCDI_H

#pragma ident	"@(#)hcdi.h	1.7	99/09/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/ddi_impldefs.h>

/*
 * usb_pipe_handle
 *	allocated by USBA and used by USBA and HCD but opaque to
 *	client driver
 *
 *	pipes can be shared but pipe_handles are unique
 *
 * p_hcd_private is a pointer to private data for HCD. This space
 * is allocated and maintained by HCD
 */
typedef struct	usb_pipe_handle_impl {
	/* For linking all handles to the same pipe by USBA */
	usba_list_entry_t	p_pipe_handle_list;

	uint_t	p_pipe_flag;	/* Set this flag to call pipe callback */

	size_t			p_pipe_handle_size;

	struct usb_device	*p_usb_device;	/* set on pipe open */

	usb_pipe_state_t	p_state;	/* maintained by USBA */
	usb_pipe_state_t	p_last_state;	/* maintained by USBA */
	usb_pipe_policy_t	*p_policy;	/* maintained by HCD */
	usb_endpoint_descr_t	*p_endpoint;

	/* access control */
	kmutex_t		p_mutex;   /* mutex protecting pipe handle */
	kcondvar_t		p_cv_access;	/* serializing access */
	uint_t			p_busy;		/* access state */

	/* per-pipe private data for HCD */
	usb_opaque_t		p_hcd_private;

	/* per-pipe private data for client */
	usb_opaque_t		p_client_private;

	/* per-pipe private data for USBA */
	usb_opaque_t		p_usba_private;

	/*
	 * for synchronous xfers, only one may be in progress
	 */
	struct {
		uint_t			p_done;
		kcondvar_t		p_cv_sync;
		uint_t			p_rval;
		mblk_t			*p_data;
		uint_t			p_completion_reason;
		uint_t			p_flag;
	} p_sync_result;

	/* bandwidth allocation */
	uint_t			p_hcd_bandwidth;

	/* count for outstanding async requests on this pipehandle */
	uint_t			p_async_requests_count;

	/*
	 * for pipe closing, store the caller pipe's handle pointer so
	 * we can zero its pipehandle on successful completion
	 */
	usb_pipe_handle_t	*p_callers_pipe_handle_p;

	/*
	 * p_n_pending_async_cbs is a count of the number of
	 * pending asynchronous callbacks
	 */
	uint_t			p_n_pending_async_cbs;

	/* maximum time waiting on the callback queue */
	hrtime_t		p_max_time_waiting;

	/* maximum time a callback has taken */
	hrtime_t		p_max_callback_time;

	/* callback that took the longest time */
	caddr_t			p_max_callback;

} usb_pipe_handle_impl_t;


#define	USB_PIPE_CLOSING(ph) \
		(((ph)->p_state == USB_PIPE_STATE_SYNC_CLOSING) || \
		((ph)->p_state == USB_PIPE_STATE_ASYNC_CLOSING))

_NOTE(MUTEX_PROTECTS_DATA(usb_pipe_handle_impl::p_mutex, usb_pipe_handle_impl))

/* these should be really stable data */
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_pipe_handle_impl::p_usb_device))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_pipe_handle_impl::p_hcd_private))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_pipe_handle_impl::p_client_private))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_pipe_handle_impl::p_usba_private))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_pipe_handle_impl::p_endpoint))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_pipe_handle_impl::p_pipe_handle_size)) 

/*
 * usb_addr:
 *	This is	the USB	address	of a device
 */
typedef	uchar_t usb_addr_t;

#define	USB_DEFAULT_ADDR	0

/*
 * The p_pipe_flag will be set to USBA_PIPE_CB_PENDING in the
 * usba_hcdi_callback() to specify that callback for specified
 * pipe is pending. Whenever the usba_hcdi_callback_thread()
 * thread wakes up, it will call pipe specific callback function.
 * Also it resets the p_pipe_flag to USBA_PIPE_NO_CB_PENDING.
 */
#define	USBA_PIPE_NO_CB_PENDING	0
#define	USBA_PIPE_CB_PENDING	1

/*
 * number of endpoint per device, 16 IN and 16 OUT.
 * this define is used for pipehandle list, pipe reserved list
 * and pipe open count array.
 * these lists are indexed by endpoint number * ((address & direction)? 2 : 1)
 *
 * We use a bit mask for exclusive open tracking and therefore
 * USB_N_ENDPOINTS must be equal to the bit size of int.
 *
 */
#define	USB_N_ENDPOINTS		32

/*
 * usb port status
 */
typedef uchar_t usb_port_status_t;

#define	USB_LOW_SPEED_DEV	0x1
#define	USB_HIGH_SPEED_DEV	0x0

/*
 * This	structure uniquely identifies a USB device
 * with all interfaces,	or just one interface of a USB device.
 * usb-device is associated with a devinfo node
 *
 * This	structure is allocated and maintained by USBA and
 * read-only for HCD except usb_hcd_private and usb_hcd_bandwidth
 *
 * There can be	multiple clients per device (multi-class
 * device) in which case this structure is shared.
 */
typedef struct usb_device {
				/* for linking all usb_devices on this bus */
	usba_list_entry_t	usb_device_list;

	/* linked list of all pipe handles on this device per endpoint */
	usba_list_entry_t	usb_pipehandle_list[USB_N_ENDPOINTS];

	kmutex_t		usb_mutex;   /*  protecting usb_device */

	struct usb_hcdi_ops	*usb_hcdi_ops;	/* ptr to HCD ops */

						/* hub ops for parent hub */
	struct usb_hubdi_ops	*usb_parent_hubdi_ops;

	struct usb_hubdi	*usb_hubdi;

	usb_addr_t		usb_addr;	/* usb address */

	dev_info_t		*usb_root_hub_dip;
	struct hubd		*usb_root_hubd;

	usb_device_descr_t	*usb_dev_descr;	/* device descriptor */

	uchar_t			*usb_config;	/* raw config descriptor */
	size_t			usb_config_length; /* length of raw descr */

	char			*usb_string_descr; /* string descriptor */

	usb_port_status_t	usb_port_status; /* usb hub port status */
	uchar_t			usb_port;

	void			*usb_hcd_private; /* per device private data */

				/* open count per e/p, max of 255! */
	uchar_t			usb_endp_open[USB_N_ENDPOINTS];
						/* bit mask excl open e/p's */
	uint_t			usb_endp_excl_open;

	/*
	 * if a usb_pipe_reserved[ep] != NULL, this pipe to the endpoint
	 * has sole access to the endpoint
	 */
	usb_pipe_handle_impl_t	*usb_pipe_reserved[USB_N_ENDPOINTS];
	kcondvar_t		usb_cv_resrvd;
	int			usb_resvrd_callback_id;

	uint_t			usb_configuration_value;

	uint_t			usb_ref_count;	/* for sharing */

	uchar_t			usb_n_configs;
	uchar_t			usb_n_interfaces;

} usb_device_t;

_NOTE(MUTEX_PROTECTS_DATA(usb_device::usb_mutex, usb_device))

/* this should be really stable data */
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_root_hub_dip))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_root_hubd))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_hcd_private))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_addr))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_hcdi_ops))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_n_interfaces))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_port))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_dev_descr)) 
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_config)) 
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_string_descr)) 
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_device::usb_config_length)) 

/*
 * HCD ops structure
 *
 * - this structure defines all entry points into HCD
 *
 * - all client driver USBAI functions that require HCD
 *   involvement go through this ops table
 *
 * - at HCD attach time, the HCD ops are passed to
 *   to the USBA through usb_hcdi_attach()
 *
 * some of these ops implement the semantics of the corresponding
 * USBAI interfaces. Refer to usbai.h for detailed description
 */
#define	HCDI_OPS_VERSION_0 0

typedef struct usb_hcdi_ops {
	int		hcdi_ops_version;	/* implementation version */

	/*
	 * usb_hcdi_client_init	is called at INITCHILD time
	 *
	 * USBA	calls this entry point before loading a	client driver.
	 * The USBA saves the device descriptor in usb_device and then
	 * calls usb_hcdi_client_init().
	 * The HCD allocates resources for the client, and returns
	 * failure if resources	are not	available.
	 */
	int	(*usb_hcdi_client_init)(
		usb_device_t	*usb_device);

	/*
	 * usb_hcdi_client_free	is called at UNINITCHILD time
	 *
	 * the pipes should be all closed so no need to clean up
	 */
	int	(*usb_hcdi_client_free)(
		usb_device_t	*usb_device);

	/*
	 * usb_hcdi_pipe_open:
	 *	implements the semantics of usb_pipe_open()
	 *	USBA allocate the pipe_handle which contains
	 *	pipe_policy and endpoint pointers
	 */
	int	(*usb_hcdi_pipe_open)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uint_t			usb_flags);

	/*
	 * close a pipe
	 */
	int	(*usb_hcdi_pipe_close)(
		usb_pipe_handle_impl_t	*pipe_handle);

	/*
	 * start/stop periodic polling
	 */
	int	(*usb_hcdi_pipe_start_polling)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uint_t			usb_flags);

	int	(*usb_hcdi_pipe_stop_polling)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uint_t			usb_flags);

	/*
	 * get/set pipe policy
	 * implementation note: the HCD should be able to handle
	 * this dynamically without draining/quiescing
	 */
	int	(*usb_hcdi_pipe_get_policy)(
		usb_pipe_handle_impl_t	*pipe_handle,
		usb_pipe_policy_t	*policy);

	int	(*usb_hcdi_pipe_set_policy)(
		usb_pipe_handle_impl_t	*pipe_handle,
		usb_pipe_policy_t	*policy,
		uint_t			usb_flags);

	/*
	 * pipe management
	 */
	int	(*usb_hcdi_pipe_abort)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uint_t			usb_flags);

	int	(*usb_hcdi_pipe_reset)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uint_t			usb_flags);

	/*
	 * device control
	 */
	int	(*usb_hcdi_pipe_device_ctrl_receive)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uchar_t			bmRequestType,
		uchar_t			bRequest,
		uint16_t		wValue,
		uint16_t		wIndex,
		uint16_t		wLength,
		uint_t			usb_flags);

	int	(*usb_hcdi_pipe_device_ctrl_send)(
		usb_pipe_handle_impl_t	*pipe_handle,
		uchar_t			bmRequestType,
		uchar_t			bRequest,
		uint16_t		wValue,
		uint16_t		wIndex,
		uint16_t		wLength,
		mblk_t			*data,
		uint_t			usb_flags);

	/*
	 * data transfer management
	 *
	 * there is no usb_hcdi_pipe_receive_isoc/intr_data since receiving
	 * data on an isoc/intr pipe is automatic
	 */
	int	(*usb_hcdi_bulk_transfer_size)(
		dev_info_t		*dip,
		size_t			*size);

	int	(*usb_hcdi_pipe_receive_bulk_data)(
		usb_pipe_handle_impl_t	*pipe_handle,
		size_t			length,
		uint_t			usb_flags);

	int	(*usb_hcdi_pipe_send_bulk_data)(
		usb_pipe_handle_impl_t	*pipe_handle,
		mblk_t			*data,
		uint_t			usb_flags);

	int	(*usb_hcdi_pipe_send_isoc_data)(
		usb_pipe_handle_impl_t	*pipe_handle,
		mblk_t			*data,
		uint_t			usb_flags);

	/*
	 * Initialize OBP support for input
	 */
	int	(*usb_hcdi_console_input_init)(
		usb_pipe_handle_impl_t		*pipe_handle,
		uchar_t				**obp_buf,
		usb_console_info_impl_t		*console_input_info);

	/*
	 * Free resources allocated by usb_hcdi_console_input_init
	 */
	int	(*usb_hcdi_console_input_fini)(
		usb_console_info_impl_t		*console_input_info);

	/*
	 * Save controller state information
	 */
	int	(*usb_hcdi_console_input_enter)(
		usb_console_info_impl_t		*console_input_info);

	/*
	 * Read character from controller
	 */
	int	(*usb_hcdi_console_read)(
		usb_console_info_impl_t		*console_input_info,
		uint_t				*num_characters);

	/*
	 * Restore controller state information
	 */
	int	(*usb_hcdi_console_input_exit)(
		usb_console_info_impl_t		*console_input_info);

	/*
	 * pointer to private USBA per-hcd info
	 */
	usb_opaque_t	hcdi_usba_private;

	uint8_t		hcdi_pm_enable;

} usb_hcdi_ops_t;


/*
 * callback support:
 *	this function handles all HCD callbacks as follows:
 *	- USB_FLAGS_SLEEP determines whether the client driver made
 *	  a synchronous or asynchronous USBAI call
 *	- for synchronous calls, the args are copied into the pipe handle
 *		and the sync cv of the pipe handle is signalled
 *	- for async calls and completion_reason = 0, the normal callback
 *		is invoked
 *	- for async calls and completion_reason != 0, the exception
 *		callback is invoked
 */
void
usba_hcdi_callback(usb_pipe_handle_impl_t	*ph,
		uint_t		usb_flags,
		mblk_t		*data,
		uint_t		flag,
		uint_t		completion_reason,
		uint_t		rval);  /* return value for the synch call */

/*
 * HCD Nexus driver support:
 */

/*
 * hcd_ops allocator/deallocator
 *	USBA allocates the usb_hcdi_ops so we can easily handle
 *	versioning
 */
usb_hcdi_ops_t	*usba_alloc_hcdi_ops();
void		usba_free_hcdi_ops(usb_hcdi_ops_t *hcdi_ops);

/*
 * Argument structure for usba_hcdi_register
 */
typedef struct usba_hcdi_register_args {

		uint_t			usba_hcdi_register_version;
		dev_info_t		usba_hcdi_register_dip;
		usb_hcdi_ops_t		*usba_hcdi_register_ops;
		ddi_dma_attr_t		*usba_hcdi_register_dma_attr;
		ddi_iblock_cookie_t	usba_hcdi_register_iblock_cookiep;
		usb_log_handle_t	usba_hcdi_register_log_handle;

}usba_hcdi_register_args_t;

#define	HCDI_REGISTER_VERS_0		0


/*
 * make	this instance known to USBA
 *
 * the HCD must initialize the hcdi_ops before calling this function
 */
int	usba_hcdi_register(usba_hcdi_register_args_t *args,
		uint_t			flags);

/*
 * retrieving usb_device from dip
 */
usb_device_t *usba_get_usb_device(dev_info_t *dip);
usb_device_t *usba_polled_get_usb_device(dev_info_t *dip);
void	usba_set_usb_device(dev_info_t *, usb_device_t *usb_device);


/*
 * detach support
 */
int	usba_hcdi_deregister(dev_info_t *dip);


/*
 * High resolution timer support
 */
#define	USB_TIME_LT(a, b) (((a).tv_nsec < (b).tv_nsec && \
		(a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

#define	USB_TIME_DELTA(a, b, delta) { \
	int xxs; \
\
	delta = (a).tv_nsec - (b).tv_nsec; \
	if ((xxs = (a).tv_sec - (b).tv_sec) != 0) { \
		delta += (1000000000 * xxs); \
	} \
} \


#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HCDI_H */
