/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USB_USBA_USBA_IMPL_H
#define	_SYS_USB_USBA_USBA_IMPL_H

#pragma ident	"@(#)usba_impl.h	1.9	99/11/18 SMI"

#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Number of types of USB transfers
 */
#define	USB_N_COUNT_KSTATS	4

/*
 * async execution of usb_pipe_* functions which have a
 * completion callback parameter (eg. usb_pipe_close(),
 * usb_pipe_abort(), usb_pipe_reset(), usb_pipe_stop_polling()
 */
typedef struct usb_pipe_async_request {
	usb_pipe_handle_t pipe_handle;
	uint_t		usb_flags;
	void		(*callback)(
				usb_opaque_t callback_arg,
				int error_code,
				uint_t usb_flags);
	usb_opaque_t	callback_arg;
	int		(*sync_func)(usb_pipe_handle_t, uint_t);
} usb_pipe_async_request_t;
_NOTE(SCHEME_PROTECTS_DATA("unique per call", usb_pipe_async_request_t))

/*
 * usb wrapper around pm_request_power_change to allow for
 * non blocking behavior
 */
typedef struct pm_request {
	dev_info_t	*dip;
	int		comp;
	int		old_level;
	int		level;
	void		(*cb)(void *, int);
	void		*arg;
	uint_t		flags;
} pm_request_t;
_NOTE(SCHEME_PROTECTS_DATA("unique per call", pm_request_t))

/*
 * Callback Structure
 *      An instance of this structure is created per callback.  The callback
 *      structures are linked together and handled in FIFO order.
 */
typedef struct usb_cb {

		/* Next callback on the list */
		struct usb_cb		*usb_cb_next;

		/* Pointer to the pipe handle */
		usb_pipe_handle_impl_t  *usb_cb_pipe_handle;

		/* Transfer data, completion reason and flag */
		mblk_t			*usb_cb_data;
		uint_t			usb_cb_completion_reason;
		uint_t			usb_cb_flag;

		/* Time the callback was queued on the list */
		timespec_t		usb_cb_time;
} usb_cb_t;


/*
 * Per HCD Data Structures
 */
typedef  struct usb_hcdi {
	dev_info_t	*hcdi_dip;	/* ptr to devinfo struct */

	ddi_dma_attr_t	*hcdi_dma_attr;

	/*
	 * list of HCD operations
	 */
	usb_hcdi_ops_t	 *hcdi_ops;

	int		 hcdi_flags;	    /* flag options */

	/*
	 * min xfer and min/max burstsizes for DDI_CTLOPS_IOMIN
	 */
	uint_t		 hcdi_min_xfer;
	uchar_t		 hcdi_min_burst_size;
	uchar_t		 hcdi_max_burst_size;

	/*
	 * usb_device ptr for root hub
	 */
	usb_device_t	*hcdi_usb_device;

	/*
	 * usb bus address allocation
	 */
	char		hcdi_usb_address_in_use[USB_ADDRESS_ARRAY_SIZE];

	/*
	 * single thread enumeration
	 */
	ksema_t		hcdi_init_ep_sema;

	/*
	 * hcdi_dump_ops is for dump support
	 */
	struct usb_dump_ops	*hcdi_dump_ops;

	usb_log_handle_t	hcdi_log_handle;

	/*
	 * soft interrupt information
	 */
	kmutex_t	hcdi_mutex;

	/*
	 * List of callbacks for the soft interrupt handler
	 */
	usb_cb_t		*hcdi_cb_list_head;
	usb_cb_t		*hcdi_cb_list_tail;

	ddi_softintr_t		hcdi_soft_int_id;

	uint_t			hcdi_soft_int_state;

	/*
	 * Number of async usba_hcdi_callbacks
	 */
	int			hcdi_usba_async_callbacks;

	/*
	 * Number of times soft interrupt handler exits
	 */
	int			hcdi_soft_intr_exit;

	/*
	 * Number of times soft is already pending in
	 * callback
	 */
	int			hcdi_soft_intr_pending;

	/*
	 * Maximum number of callbacks the soft interrupt
	 * has handled.
	 */
	int			hcdi_max_no_handled;

	/*
	 * Hotplug event statistics since hcdi loaded.
	 */
	ulong_t			hcdi_total_hotplug_success;
	ulong_t			hcdi_total_hotplug_failure;

	/*
	 * Resetable hotplug event statistics.
	 */
	ulong_t			hcdi_hotplug_success;
	ulong_t			hcdi_hotplug_failure;

	/*
	 * Total number of devices currently enumerated.
	 */
	uchar_t			hcdi_device_count;

	/*
	 * kstat structures
	 */
	kstat_t			*hcdi_hotplug_stats;
	kstat_t			*hcdi_error_stats;
} usb_hcdi_t;

/*
 * Hotplug kstats named structure
 */
typedef struct hcdi_hotplug_stats {
	struct kstat_named	hcdi_hotplug_total_success;
	struct kstat_named	hcdi_hotplug_success;
	struct kstat_named	hcdi_hotplug_total_failure;
	struct kstat_named	hcdi_hotplug_failure;
	struct kstat_named	hcdi_device_count;
} hcdi_hotplug_stats_t;

/*
 * USB error kstats named structure
 */
typedef struct hcdi_error_stats {
	/* transport completion codes */
	struct kstat_named	hcdi_usb_cc_crc;
	struct kstat_named	hcdi_usb_cc_bitstuffing;
	struct kstat_named	hcdi_usb_cc_data_toggle_mm;
	struct kstat_named	hcdi_usb_cc_stall;
	struct kstat_named	hcdi_usb_cc_dev_not_resp;
	struct kstat_named	hcdi_usb_cc_pid_checkfailure;
	struct kstat_named	hcdi_usb_cc_unexp_pid;
	struct kstat_named	hcdi_usb_cc_data_overrun;
	struct kstat_named	hcdi_usb_cc_data_underrun;
	struct kstat_named	hcdi_usb_cc_buffer_overrun;
	struct kstat_named	hcdi_usb_cc_buffer_underrun;
	struct kstat_named	hcdi_usb_cc_timeout;
	struct kstat_named	hcdi_usb_cc_unspecified_err;

	/* USBA function return values */
	/*
	struct kstat_named	hcdi_usb_failure;
	struct kstat_named	hcdi_usb_no_resources;
	struct kstat_named	hcdi_usb_no_bandwidth;
	struct kstat_named	hcdi_usb_pipe_reserved;
	struct kstat_named	hcdi_usb_pipe_unshareable;
	struct kstat_named	hcdi_usb_not_supported;
	struct kstat_named	hcdi_usb_pipe_error;
	struct kstat_named	hcdi_usb_pipe_busy;
	*/
} hcdi_error_stats_t;

/*
 * hcdi kstat defines
 */
#define	HCDI_HOTPLUG_STATS(hcdi)	((hcdi)->hcdi_hotplug_stats)
#define	HCDI_HOTPLUG_STATS_DATA(hcdi)	\
	((hcdi_hotplug_stats_t *)HCDI_HOTPLUG_STATS((hcdi))->ks_data)

#define	HCDI_ERROR_STATS(hcdi)		((hcdi)->hcdi_error_stats)
#define	HCDI_ERROR_STATS_DATA(hcdi)	\
	((hcdi_error_stats_t *)HCDI_ERROR_STATS((hcdi))->ks_data)

#define	HCDI_DO_ERROR_STATS(hcdi, completion_reason) {			\
	if (HCDI_ERROR_STATS(hcdi) != NULL) {				\
	switch (completion_reason) {					\
		case USB_CC_NOERROR:					\
			break;						\
		case USB_CC_CRC:					\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_crc.value.ui64++;		\
			break;						\
		case USB_CC_BITSTUFFING:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_bitstuffing.value.ui64++;	\
			break;						\
		case USB_CC_DATA_TOGGLE_MM:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_data_toggle_mm.value.ui64++;	\
			break;						\
		case USB_CC_STALL:					\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_stall.value.ui64++;		\
			break;						\
		case USB_CC_DEV_NOT_RESP:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_dev_not_resp.value.ui64++;	\
			break;						\
		case USB_CC_PID_CHECKFAILURE:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_pid_checkfailure.value.ui64++;	\
			break;						\
		case USB_CC_UNEXP_PID:					\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_unexp_pid.value.ui64++;		\
			break;						\
		case USB_CC_DATA_OVERRUN:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_data_overrun.value.ui64++;	\
			break;						\
		case USB_CC_DATA_UNDERRUN:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_data_underrun.value.ui64++;	\
			break;						\
		case USB_CC_BUFFER_OVERRUN:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_buffer_overrun.value.ui64++;	\
			break;						\
		case USB_CC_BUFFER_UNDERRUN:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_buffer_underrun.value.ui64++;	\
			break;						\
		case USB_CC_TIMEOUT:					\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_timeout.value.ui64++;		\
			break;						\
		case USB_CC_UNSPECIFIED_ERR:				\
			HCDI_ERROR_STATS_DATA(hcdi)->			\
			    hcdi_usb_cc_unspecified_err.value.ui64++;	\
			break;						\
		default:						\
			break;						\
	}								\
	}								\
}

_NOTE(MUTEX_PROTECTS_DATA(usb_hcdi::hcdi_mutex,
				usb_hcdi::hcdi_usb_address_in_use))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_hcdi_t::hcdi_usb_device))

_NOTE(MUTEX_PROTECTS_DATA(usb_hcdi::hcdi_mutex,
				usb_hcdi::hcdi_cb_list_head))
_NOTE(MUTEX_PROTECTS_DATA(usb_hcdi::hcdi_mutex,
				usb_hcdi::hcdi_cb_list_tail))
_NOTE(MUTEX_PROTECTS_DATA(usb_hcdi::hcdi_mutex,
				usb_hcdi::hcdi_soft_int_state))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_hcdi_t::hcdi_soft_int_id))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_hcdi_t::hcdi_soft_intr_exit))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_hcdi_t::hcdi_soft_intr_pending))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_hcdi_t::hcdi_max_no_handled))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_hcdi_t::hcdi_usba_async_callbacks))

_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_cb_t::usb_cb_pipe_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_cb_t::usb_cb_data))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_cb_t::usb_cb_completion_reason))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_cb_t::usb_cb_flag))
_NOTE(DATA_READABLE_WITHOUT_LOCK(usb_cb_t::usb_cb_next))


/*
 * Soft interrupt states
 */
#define	HCDI_SOFT_INT_NOT_PENDING	0
#define	HCDI_SOFT_INT_PENDING		1


/*
 * retrieving the hcdi structure from dip
 */
void usba_hcdi_set_hcdi(dev_info_t *dip, usb_hcdi_t *hcdi);
usb_hcdi_t *usba_hcdi_get_hcdi(dev_info_t *dip);


/*
 * Per Hub Data Structures
 */
typedef  struct usb_hubdi {
	usba_list_entry_t hubdi_list;	 /* linking in hubdi list */

	dev_info_t	*hubdi_dip;	 /* ptr to devinfo struct */

	/*
	 * list of HCD operations
	 */
	usb_hubdi_ops_t   *hubdi_ops;

	int		hubdi_flags;	/* flag options */

} usb_hubdi_t;


/*
 * prototypes
 */
void usba_usba_initialization();
void usba_usba_destroy();

void usba_usbai_initialization();
void usba_usbai_destroy();

void usba_hcdi_initialization();
void usba_hcdi_destroy();

void usba_hubdi_initialization();
void usba_hubdi_destroy();

int usba_is_root_hub(dev_info_t *dip);

int usba_bus_ctl(dev_info_t *dip, dev_info_t *rdip,
		ddi_ctl_enum_t  op, void *arg, void *result);

usb_device_t *usba_alloc_usb_device();
void usba_free_usb_device(usb_device_t *usb_device_t);

void usba_enter_enumeration(usb_device_t *usb_device_t);
void usba_exit_enumeration(usb_device_t *usb_device_t);

int usba_child_exists(dev_info_t *, dev_info_t *);

/* list handling */
void usba_add_list(usba_list_entry_t *head, usba_list_entry_t *element);
void usba_remove_list(usba_list_entry_t *head, usba_list_entry_t *element);
int usba_check_in_list(usba_list_entry_t *head, usba_list_entry_t *element);

/*
 * Get dma attributes from HC.
 */
ddi_dma_attr_t *usb_get_hc_dma_attr(dev_info_t *dip);

/* Get maximum bulk transfer size */
int usb_pipe_bulk_transfer_size(dev_info_t *dip, size_t	*size);

/* flag for serial access, needed for reset and abort */
#define	USBA_FORCE_SERIAL_ACCESS ((uint_t)1)

/* flag for  usba_is_pipe_reserved() */
#define	USBA_RESERVE_PIPE	((uint_t)1)

/*
 * Debug printing
 */
#define	USB_ALLOW_LABEL		0x1
#define	USB_DISALLOW_LABEL	0x0

#ifdef DEBUG
/*
 * USB dump support:
 *
 * The next list of functions and data structures are used to
 * dump all USB related information. The function usba_dump_all()
 * could be invoked from kadb or from within any other module.
 *
 * USBA provides an API for all other USB related modules to
 * register their dump functions with USBA. The name of the API is
 * usba_dump_register(). It can be called from attach time. If a
 * module does not have a specific attach() function it could call
 * this function from its _init() function. It is the responsiblity
 * of the module to deregister its dump function during the "detach"
 * or "_fini" phase.
 *
 * NOTE: Each module defining their own dump functions should provide
 * for a separate "dump mutex". This mutex should be held across the
 * calls to the "dump functions". One should not use the mutex,
 * defined to protect module's data structures in the "dump functions".
 */

#define	USBA_DUMP_OPS_VERSION_0		0	/* Current dump version  */

#define	USB_DUMP_STATE			0x0001	/* Dump soft state info  */
#define	USB_DUMP_USB_DEVICE		0x0002	/* Dump usb device info  */
#define	USB_DUMP_PIPE_HANDLE		0x0004	/* Dump pipe handle info */
#define	USB_DUMP_DESCRIPTORS		0x0008	/* Dump usb descriptors  */
#define	USB_DUMP_PIPE_POLICY		0x0010	/* Dump usb pipe policy  */

typedef struct usb_dump_ops {
	/*
	 * USBA dump implementation version
	 */
	int			usb_dump_ops_version;

	/*
	 * USBA dump function
	 */
	void			(*usb_dump_func)(uint_t, usb_opaque_t);

	/*
	 * Pointer to a per-module call back argument
	 */
	usb_opaque_t		usb_dump_cb_arg;

	/*
	 * Pointer to the next usb_dump function info
	 */
	struct usb_dump_ops	*usb_dump_ops_next;

	/*
	 * usb_dump_order. See below for complete description.
	 */
	uint_t			usb_dump_order;
} usb_dump_ops_t;


/*
 * usb_dump_order:
 *
 * These order definitions are used to enforce an order in which the
 * usb_dump functions are called.
 */
#define	USB_DUMPOPS_HCDI_ORDER		0x01	/* for HCD module */
#define	USB_DUMPOPS_HUB_ORDER		0x02	/* for hub devices */
#define	USB_DUMPOPS_OHCI_ORDER		0x03	/* for host controllers  */
#define	USB_DUMPOPS_USB_MID_ORDER	0x04	/* for usb_mid module */
#define	USB_DUMPOPS_CLIENT_ORDER	0x05	/* for client drivers */
#define	USB_DUMPOPS_OTHER_ORDER		0x06	/* for other USB drivers */


/*
 * Functions to Allocate/Deallocate usb_dump_ops
 */
usb_dump_ops_t	*usba_alloc_dump_ops();
void		usba_free_dump_ops(usb_dump_ops_t *);

/*
 * usba_dump_register() is to be called by a USB module to register
 * its dump function with USBA.
 *
 * NOTE: The USB module must initialize usba_dump_ops before calling
 * this function. It is to be called from attach() or _init().
 */
void	usba_dump_register(usb_dump_ops_t *);

/*
 * usba_dump_deregister() is to be called during detach or _fini()
 */
void	usba_dump_deregister(usb_dump_ops_t *);

/*
 * functions called to dump all the info
 */
void	usba_dump_all(uint_t dump_flags);
void	usba_dump_usb_device(usb_device_t *, uint_t);
void	usba_dump_usb_pipe_handle(usb_pipe_handle_t, uint_t);
void	usba_dump_usb_pipe_policy(usb_pipe_policy_t *, uint_t);
void	usba_dump_descriptors(dev_info_t *, int);

#endif

/*
 * index for getting to usb_pipehandle_list in usb_device or
 * usb_endp_open count
 */
uchar_t usba_get_ep_index(uint8_t ep_addr);

/*
 * retrieve string descriptors for manufacturer, vendor and serial
 * number
 */
int usba_get_mfg_product_sn_strings(dev_info_t *, char *, size_t);

/*
 * Check if we are not in interrupt context and have
 * USB_FLAGS_SLEEP flags set.
 */
#define	USBA_CHECK_CONTEXT()	ASSERT(!(curthread->t_flag & T_INTR_THREAD))
#define	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags) \
		ASSERT(!((curthread->t_flag & T_INTR_THREAD) && \
		    ((usb_flags) & USB_FLAGS_SLEEP)));

/*
 * USBA module Masks
 */
#define	DPRINT_MASK_USBA		0x00000001
#define	DPRINT_MASK_USBAI		0x00000002
#define	DPRINT_MASK_HUBDI		0x00000004
#define	DPRINT_MASK_HCDI		0x00000008
#define	DPRINT_MASK_HCDI_DUMPING	0x00000010
#define	DPRINT_MASK_HUBDI_DUMPING	0x00000020
#define	DPRINT_MASK_ALL 		0xFFFFFFFF

typedef struct {
	dev_info_t	*lh_dip;
	char		*lh_name;
	uint_t		*lh_errlevel;
	uint_t		*lh_mask;
	uint_t		*lh_instance_filter;
	uint_t		*lh_show_label;
	uint_t		lh_flags;
} usb_log_handle_impl_t;


#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_USBA_USBA_IMPL_H */
