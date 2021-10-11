/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)usbai.c	1.12	99/11/18 SMI"

/*
 * USBA: Solaris USB Architecture support
 *
 * all functions exposed to client drivers  have prefix usb_ while all USBA
 * internal functions or functions exposed to HCD or hubd only have prefix
 * usba_
 */
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/taskq.h>
#include <sys/ndi_impldefs.h>

/*
 * Provide a default configuration power descriptor
 */
usb_config_pwr_descr_t	default_config_power = {
	18,	/* bLength */
	USB_DESCR_TYPE_CONFIGURATION_POWER,	/* bDescriptorType */
	0,	/* SelfPowerConsumedD0_l */
	0,	/* SelfPowerConsumedD0_h */
	0,	/* bPowerSummaryId */
	0,	/* bBusPowerSavingD1 */
	0,	/* bSelfPowerSavingD1 */
	0,	/* bBusPowerSavingD2 */
	0,	/* bSelfPowerSavingD2 */
	100,	/* bBusPowerSavingD3 */
	100,	/* bSelfPowerSavingD3 */
	0,	/* TransitionTimeFromD1 */
	0,	/* TransitionTimeFromD2 */
	10,	/* TransitionTimeFromD3 1 Second */
};

/*
 * Provide a default interface power descriptor
 */
usb_interface_pwr_descr_t default_interface_power = {
	15,	/* bLength */
	USB_DESCR_TYPE_INTERFACE_POWER,	/* bDescriptorType */
	8,	/* bmCapabilitiesFlags */
	0,	/* bBusPowerSavingD1 */
	0,	/* bSelfPowerSavingD1 */
	0,	/* bBusPowerSavingD2 */
	0,	/* bSelfPowerSavingD2 */
	100,	/* bBusPowerSavingD3 */
	100,	/* bSelfPowerSavingD3 */
	0,	/* TransitionTimeFromD1 */
	0,	/* TransitionTimeFromD2 */
	10,	/* TransitionTimeFromD3 1 Second */
};

/*
 * print buffer protected by mutex for debug stuff. the mutex also
 * ensures serializing debug messages
 */
static kmutex_t	usba_print_mutex;
static char usba_print_buf[256];

static void usba_pipe_serialize_access(usb_pipe_handle_impl_t *ph, int force);
static int usb_pipe_sync_close(usb_pipe_handle_t pipe_handle, uint_t usb_flags);

/* flag to Force Enable pm on intel/RIO host controller */
int	usb_force_enable_pm = 0;
int	usb_pm_mouse = 0;

/*
 * debug stuff
 */
static usb_log_handle_t usbai_log_handle;
static uint_t		usbai_errlevel = USB_LOG_L2;
static uint_t		usbai_errmask = (uint_t)-1;
static uint_t		usbai_show_label = USB_ALLOW_LABEL;

#define	USB_DEBUG_SIZE_EXTRA_ALLOC	8
#ifdef	DEBUG
#define	USBA_DEBUG_BUF_SIZE		0x10000
#else
#define	USBA_DEBUG_BUF_SIZE		0x2000
#endif	/* DEBUG */

static int usba_suppress_dprintf;		/* Suppress debug printing */
static int usba_buffer_dprintf = 1;		/* Use a debug print buffer */
static int usba_debug_buf_size = USBA_DEBUG_BUF_SIZE;	/* Size of debug buf */
static char *usba_debug_buf = NULL;			/* The debug buf */
static char *usba_buf_sptr, *usba_buf_eptr;

/*
 * taskq:
 * 	A task queue consists of a single queue of tasks,
 *	together with one or more threads to service the queue
 *
 *	The taskq provides to USBA a mechanism for immediate availability
 *	of threads for performing  asynchronous calls or for
 *	hub hotplugging.
 */
taskq_t *usba_taskq;
#define	USBA_TASKQ_N_THREADS	3
static	uint_t usba_taskq_n_threads = USBA_TASKQ_N_THREADS;
extern	pri_t minclsyspri;

#define	USBA_TASKQ_PRI		minclsyspri
#define	USBA_TASKQ_MINALLOC	(4*usba_taskq_n_threads)
#define	USBA_TASKQ_MAXALLOC	(500*usba_taskq_n_threads)


/* USBA framework initializations */
void
usba_usbai_initialization()
{
	usbai_log_handle = usb_alloc_log_handle(NULL, "usbai", &usbai_errlevel,
				&usbai_errmask, NULL, &usbai_show_label, 0);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usba_usbai_initialization");

	mutex_init(&usba_print_mutex, NULL, MUTEX_DRIVER, NULL);

	usba_taskq = taskq_create("USBA taskq", usba_taskq_n_threads,
	    USBA_TASKQ_PRI, USBA_TASKQ_MINALLOC, USBA_TASKQ_MAXALLOC,
	    TASKQ_PREPOPULATE);
}


/* USBA framework destroys */
void
usba_usbai_destroy()
{
	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usba_usbai_destroy");

	mutex_destroy(&usba_print_mutex);
	if (usba_debug_buf) {
		kmem_free(usba_debug_buf,
		    usba_debug_buf_size + USB_DEBUG_SIZE_EXTRA_ALLOC);
	}

	usb_free_log_handle(usbai_log_handle);

	taskq_destroy(usba_taskq);
}


/*
 * default endpoint descriptor and pipe policy
 */
static usb_endpoint_descr_t	usba_default_endpoint_descr =
	{7, 5, 0, USB_EPT_ATTR_CONTROL, 8, 0};

/* set some meaningful defaults */
static usb_pipe_policy_t usba_default_endpoint_pipe_policy =
	{0, 0, 0, 0, 0, 0};

uchar_t
usba_get_ep_index(uint8_t ep_addr)
{
	return ((ep_addr & USB_EPT_ADDR_MASK) +
	    ((ep_addr & USB_EPT_DIR_MASK) ? 16 : 0));
}


/*
 * pipe management
 *	init and destroy a pipehandle
 */
static int
usba_init_pipe_handle(usb_device_t	*usb_device,
		usb_endpoint_descr_t	*endpoint,
		usb_pipe_policy_t	*pipe_policy,
		uint_t			usb_flags,
		usb_pipe_handle_impl_t	**pipe_handle)
{
	int kmflag;
	usb_pipe_handle_impl_t *ph;
	uchar_t	ep_index = usba_get_ep_index(endpoint->bEndpointAddress);
	int attribute	= endpoint->bmAttributes & USB_EPT_ATTR_MASK;
	size_t	size;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usba_init_pipe_handle: "
	    "usb_device = 0x%p ep = 0x%x", usb_device, endpoint);

	kmflag = (usb_flags & USB_FLAGS_SLEEP) ? KM_SLEEP : KM_NOSLEEP;
	size	= sizeof (usb_pipe_handle_impl_t);

	ph = kmem_zalloc(size, kmflag);
	if (ph == NULL) {

		return (USB_NO_RESOURCES);
	}

	mutex_init(&ph->p_mutex, NULL, MUTEX_DRIVER, NULL);

	mutex_enter(&ph->p_mutex);

	ph->p_endpoint = kmem_zalloc(sizeof (usb_endpoint_descr_t), kmflag);
	if (ph->p_endpoint == NULL) {
		kmem_free(ph, size);

		mutex_exit(&ph->p_mutex);
		return (USB_NO_RESOURCES);
	}

	*ph->p_endpoint = *endpoint;

	/* fix up the MaxPacketSize if it is the default endpoint descr */
	if ((endpoint == &usba_default_endpoint_descr) && usb_device) {
		USB_DPRINTF_L3(DPRINT_MASK_USBAI, usbai_log_handle,
		    "adjusting max packet size from %d to %d",
		    ph->p_endpoint->wMaxPacketSize,
		    usb_device->usb_dev_descr->bMaxPacketSize0);

		ph->p_endpoint->wMaxPacketSize =
		    usb_device->usb_dev_descr->bMaxPacketSize0;
	}

	ph->p_usb_device = usb_device;
	ph->p_pipe_handle_size = size;

	/* Keep a copy of the pipe policy */
	ph->p_policy = kmem_zalloc(sizeof (usb_pipe_policy_t), kmflag);
	if (ph->p_policy == NULL) {
		kmem_free(ph->p_endpoint, sizeof (usb_endpoint_descr_t));
		kmem_free(ph, size);

		mutex_exit(&ph->p_mutex);
		return (USB_NO_RESOURCES);
	}

	bcopy(pipe_policy, ph->p_policy, sizeof (usb_pipe_policy_t));

	cv_init(&ph->p_cv_access, NULL, CV_DRIVER, NULL);

	/* Initialize the state according to the type of pipe */
	if ((attribute == USB_EPT_ATTR_CONTROL) ||
		(attribute == USB_EPT_ATTR_BULK)) {
		ph->p_state = USB_PIPE_STATE_ACTIVE;
	} else {
		ph->p_state = USB_PIPE_STATE_IDLE;
	}

	/* Initialize the list mutex */
	mutex_init(&ph->p_pipe_handle_list.list_mutex, NULL,
	    MUTEX_DRIVER, NULL);

	usba_add_list(&usb_device->usb_pipehandle_list[ep_index],
	    &ph->p_pipe_handle_list);

	mutex_exit(&ph->p_mutex);

	*pipe_handle = ph;

	return (USB_SUCCESS);
}


static void
usba_destroy_pipe_handle(usb_pipe_handle_impl_t *ph)
{
	uchar_t	ep_index = usba_get_ep_index(
			ph->p_endpoint->bEndpointAddress);
	usb_pipe_handle_t	*ph_p;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usba_destroy_pipe_handle: ph = 0x%p", ph);

	mutex_enter(&ph->p_mutex);

	ph_p = ph->p_callers_pipe_handle_p;

	/*
	 * before destroying pipe, ensure that no callbacks
	 * pending or asynchronous active threads active
	 * If any, delay until completed
	 */
	if (ph->p_state == USB_PIPE_STATE_ASYNC_CLOSING) {
		/*
		 * if this is an async close, decrement count first
		 * so we don't wait for ourselves
		 */
		ph->p_async_requests_count--;
	}

	while ((ph->p_n_pending_async_cbs != 0) ||
	    (ph->p_async_requests_count != 0)) {

		mutex_exit(&ph->p_mutex);
		delay(drv_usectohz(1000));
		mutex_enter(&ph->p_mutex);
	}

	ASSERT(ph->p_pipe_flag == USBA_PIPE_NO_CB_PENDING);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usba_destroy_pipe_handle: destroying ph = 0x%p", ph);

	usba_remove_list(&ph->p_usb_device->usb_pipehandle_list[ep_index],
		&ph->p_pipe_handle_list);

	mutex_destroy(&ph->p_pipe_handle_list.list_mutex);

	if (ph->p_endpoint) {
		kmem_free(ph->p_endpoint, sizeof (usb_endpoint_descr_t));
	}

	/* free pipe policy */
	if (ph->p_policy) {
		kmem_free(ph->p_policy, sizeof (usb_pipe_policy_t));
	}

	/* release the pipe */
	mutex_enter(&ph->p_usb_device->usb_mutex);
	ph->p_usb_device->usb_pipe_reserved[ep_index] = NULL;
	mutex_exit(&ph->p_usb_device->usb_mutex);

	/* destroy mutex and cv's */
	cv_destroy(&ph->p_sync_result.p_cv_sync);
	cv_destroy(&ph->p_cv_access);

	mutex_exit(&ph->p_mutex);
	mutex_destroy(&ph->p_mutex);

	/* free pipe handle */
	kmem_free(ph, ph->p_pipe_handle_size);

	/* zero caller's ph pointer */
	if (ph_p) {
		*ph_p = NULL;
	}
}


/*
 * usba_pipe_is_owned:
 *	endpoint reservations are maintained in usb_device structure
 *	if this pipehandle owns the reservation on this endpoint, return
 *	1, else 0
 */
static int
usba_pipe_is_owned(usb_pipe_handle_impl_t *ph)
{
	int rval = 0;	/* not owned */
	usb_device_t *usb_device = ph->p_usb_device;
	uchar_t	ep_index = usba_get_ep_index(ph->p_endpoint->bEndpointAddress);

	mutex_enter(&usb_device->usb_mutex);
	if (usb_device->usb_pipe_reserved[ep_index] == ph) {
		rval = 1;
	}
	mutex_exit(&usb_device->usb_mutex);

	return (rval);
}


/*
 * usba_pipe_is_reserved:
 *	check if endpoint is available (ie either we own the endpoint or
 *	wait till it becomes available if usb_flags include
 *	USB_FLAGS_SLEEP
 *	if reserve flag is set, then immediately reserve the pipe
 */
static int
usba_pipe_is_reserved(usb_pipe_handle_impl_t *ph, uint_t usb_flags,
		uint_t reserve)
{
	int rval = 0;	/* not owned */
	usb_device_t *usb_device = ph->p_usb_device;
	uchar_t	ep_index = usba_get_ep_index(ph->p_endpoint->bEndpointAddress);

	mutex_enter(&usb_device->usb_mutex);

	if ((usb_device->usb_pipe_reserved[ep_index] == NULL) ||
	    (usb_device->usb_pipe_reserved[ep_index] == ph)) {
		rval = 1;
	} else if (usb_flags & USB_FLAGS_SLEEP) {
		while (usb_device->usb_pipe_reserved[ep_index]) {
			cv_wait(&usb_device->usb_cv_resrvd,
				&usb_device->usb_mutex);
		}
		rval = 1;
	}

	if (rval && (reserve == USBA_RESERVE_PIPE)) {
		usb_device->usb_pipe_reserved[ep_index] = ph;
	}

	mutex_exit(&usb_device->usb_mutex);

	return (rval);
}


/*
 * serialized access to pipe for synchronous calls
 */
static void
usba_pipe_serialize_access(usb_pipe_handle_impl_t *ph, int force)
{
	ASSERT(mutex_owned(&ph->p_mutex));

	/*
	 * if force is set, then just increment busy flag
	 * (abort and reset pipe need to access regardless of busy flag)
	 *
	 * only one sync cmd can be in progress at any time on this
	 * pipe handle since we cannot share the sync result area
	 */
	if (force != USBA_FORCE_SERIAL_ACCESS) {
		while (ph->p_busy) {
			cv_wait(&ph->p_cv_access, &ph->p_mutex);
		}
	}
	ph->p_busy++;
}


static void
usba_pipe_release_access(usb_pipe_handle_impl_t *ph)
{
	ASSERT(mutex_owned(&ph->p_mutex));

	ph->p_busy--;

	cv_signal(&ph->p_cv_access);
}


/*
 * usba_pipe_sync_init:
 *	init sync transport result structure in the pipe handle
 */

static void
usba_pipe_sync_init(usb_pipe_handle_impl_t *ph, int force)
{
	usba_pipe_serialize_access(ph, force);

	ph->p_sync_result.p_done = 0;
	ph->p_sync_result.p_rval = 0;
	ph->p_sync_result.p_data = NULL;
	ph->p_sync_result.p_completion_reason = 0;
	ph->p_sync_result.p_flag = 0;
}


/*
 * usba_pipe_sync_completion:
 *	handle sync transport completion
 *	if rval is USB_SUCCESS then wait for p_done to be set
 *	otherwise just set completion reason
 */
static void
usba_pipe_sync_completion(usb_pipe_handle_impl_t *ph,
	int	*rval,
	mblk_t	**data,
	uint_t	*completion_reason,
	uint_t	nodata)
{
	mutex_enter(&ph->p_mutex);

	/*
	 * Only wait for the result if the call into the
	 * hcd succeeded.
	 */
	if (*rval == USB_SUCCESS) {
		while (!(ph->p_sync_result.p_done)) {
			if (nodata) {
				ASSERT(ph->p_sync_result.p_data == NULL);
			}

			/* Time out if the device takes too long */
			cv_wait(&ph->p_sync_result.p_cv_sync, &ph->p_mutex);
		}

		if (nodata) {
			ASSERT(ph->p_sync_result.p_data == NULL);
		}

		/* extract return values from pipehandle */
		if (data) {
			*data = ph->p_sync_result.p_data;
		}
		if (completion_reason) {
			*completion_reason =
				ph->p_sync_result.p_completion_reason;
		}
		*rval = ph->p_sync_result.p_rval;
		if (nodata) {
			ASSERT(ph->p_sync_result.p_data == NULL);
		}

	} else {
		/*
		 * An immediate failure won't have accessed hardware
		 * Fill in the unspecified error for the completion
		 * reason.
		 */
		if (completion_reason) {
			*completion_reason = USB_CC_UNSPECIFIED_ERR;
			if (USB_PIPE_CLOSING(ph)) {
				ph->p_last_state = USB_PIPE_STATE_ERROR;
			} else {
				ph->p_state = USB_PIPE_STATE_ERROR;
			}
		}
		if (data) {
			*data = ph->p_sync_result.p_data;
		}
		if (nodata) {
			ASSERT(ph->p_sync_result.p_data == NULL);
		}
	}

	usba_pipe_release_access(ph);

	mutex_exit(&ph->p_mutex);
}

/*
 * usba_drain_callbacks:
 *	Drain the callbacks on the pipe handle
 */
static void
usba_drain_callbacks(usb_pipe_handle_impl_t *ph)
{
	ASSERT(mutex_owned(&ph->p_mutex));

	while (ph->p_n_pending_async_cbs != 0) {

		mutex_exit(&ph->p_mutex);
		delay(drv_usectohz(1000));
		mutex_enter(&ph->p_mutex);
	}
}


/*
 * usba_check_pipe_open:
 *	if the pipe is not open, then it is most likely that
 *	we panic while dereferencing pointers. If the
 *	pipehandle is not in the list, then we will panic anyway.
 */
static void
usba_check_pipe_open(usb_pipe_handle_impl_t *ph)
{
	uchar_t	ep_index = usba_get_ep_index(ph->p_endpoint->bEndpointAddress);
	int rval;

	ASSERT(ph);

	rval = usba_check_in_list(
		&ph->p_usb_device->usb_pipehandle_list[ep_index],
		&ph->p_pipe_handle_list);

	if (rval) {
		USB_DPRINTF_L3(DPRINT_MASK_USBAI, usbai_log_handle,
		    "usba_check_pipe_open: pipe is not open");

		cmn_err(CE_PANIC, "pipe is not open\n");
	}
}

/*
 * usb_pipe_open:
 *	open a pipe to an endpoint which really means allocating and
 *	initializing pipehandle.
 *
 *	Note that there might be multiple pipe handles to the same pipe.
 *
 *	- if no endpoint was specified, then use the default endpoint
 *	  descriptor
 *	- if no pipe policy was specified, then use the default policy
 *	- if another client has this ep reserved, return failure or block
 *	- reserve the endpoint to serialize accesses
 *	- if this is an exclusive open, check whether it is already
 *	  open. if not, set exclusive open bit for this ep.
 *	- allocate a new pipehandle
 *	- request HCD to open the pipe
 *	- mark this pipe as open and release reservation
 */
int
usb_pipe_open(
    dev_info_t			*dip,
    usb_endpoint_descr_t	*endpoint,
    usb_pipe_policy_t		*pipe_policy,
    uint_t			usb_flags,
    usb_pipe_handle_t		*pipe_handle)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	int		rval = USB_FAILURE;
	uchar_t		ep_index, ep_index_mask;
	usb_pipe_handle_impl_t *ph;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_open:\n\t"
	    "dip = 0x%p ep = 0x%x pp = 0x%p uf = 0x%x ph = 0x%p",
	    dip, endpoint, pipe_policy, usb_flags, pipe_handle);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);

	/*
	 * if a null endpoint pointer was passed, use the default
	 * endpoint descriptor
	 */
	if (endpoint == NULL) {
		endpoint = &usba_default_endpoint_descr;
	}
	if (pipe_policy == NULL) {
		pipe_policy = &usba_default_endpoint_pipe_policy;
	}

	/*
	 * if this is an exclusive open, check the open status
	 */
	ep_index = usba_get_ep_index(endpoint->bEndpointAddress);
	ep_index_mask = 1 << ep_index;

	/*
	 * Check for exclusive open. If the endpoint is open and
	 * if SLEEP flag has been set then
	 * periodically check again whether endpoint can be exclusively
	 * opened
	 */
	mutex_enter(&usb_device->usb_mutex);
	if (usb_flags & USB_FLAGS_OPEN_EXCL) {
		while (usb_device->usb_endp_open[ep_index]) {

			USB_DPRINTF_L2(DPRINT_MASK_USBAI, usbai_log_handle,
			    "usb_pipe_open: exclusive open failed (0x%x)",
			    usb_device->usb_endp_open[ep_index]);

			if (usb_flags & USB_FLAGS_SLEEP) {
				mutex_exit(&usb_device->usb_mutex);
				delay(drv_usectohz(1000));
				mutex_enter(&usb_device->usb_mutex);

				continue;
			}

			mutex_exit(&usb_device->usb_mutex);

			return (USB_FAILURE);
		}
		ASSERT((usb_device->usb_endp_excl_open &
						ep_index_mask) == 0);
		usb_device->usb_endp_excl_open |= ep_index_mask;
	}

	/*
	 * Indicate that the endpoint is open
	 */
	usb_device->usb_endp_open[ep_index]++;
	mutex_exit(&usb_device->usb_mutex);

	/*
	 * allocate and initialize the pipe handle
	 */
	rval = usba_init_pipe_handle(usb_device,
		endpoint, pipe_policy, usb_flags, &ph);
	if (rval != USB_SUCCESS) {

		/*
		 * In case of error, indicate that the endpoint
		 * couldn't be opened and if it was an exclusive
		 * open, reset the mask bit
		 */
		mutex_enter(&usb_device->usb_mutex);
		usb_device->usb_endp_open[ep_index]--;

		if (usb_flags & USB_FLAGS_OPEN_EXCL) {
			usb_device->usb_endp_excl_open &= ~ep_index_mask;
		}

		mutex_exit(&usb_device->usb_mutex);

		return (rval);
	}

	/* reserve pipe to avoid simultaneous opens */
	if (!usba_pipe_is_reserved(ph, usb_flags, USBA_RESERVE_PIPE)) {

		/*
		 * this should not block since there should be no
		 * activity on this pipe yet
		 */
		usba_destroy_pipe_handle(ph);

		/*
		 * In case of error, indicate that the endpoint
		 * couldn't be opened and if it was an exclusive
		 * open, reset the mask bit
		 */
		mutex_enter(&usb_device->usb_mutex);
		usb_device->usb_endp_open[ep_index]--;

		if (usb_flags & USB_FLAGS_OPEN_EXCL) {
			usb_device->usb_endp_excl_open &= ~ep_index_mask;
		}

		mutex_exit(&usb_device->usb_mutex);

		return (USB_PIPE_RESERVED);
	}

	/*
	 * if successful, then ask the hcd to open the pipe
	 */
	if (rval == USB_SUCCESS) {
		rval = usb_device->usb_hcdi_ops->usb_hcdi_pipe_open(
		    ph,  usb_flags);
	}

	/*
	 * update the endpoint open mask bits
	 */
	*pipe_handle = (usb_pipe_handle_t)ph;

	if (rval != USB_SUCCESS) {
		(void) usb_pipe_release(*pipe_handle);

		/*
		 * this should not block since there should be no
		 * activity on this pipe yet
		 */
		usba_destroy_pipe_handle(ph);

		/*
		 * In case of error, indicate that the endpoint
		 * couldn't be opened and if it was an exclusive
		 * open, reset the mask bit
		 */
		mutex_enter(&usb_device->usb_mutex);
		usb_device->usb_endp_open[ep_index]--;

		if (usb_flags & USB_FLAGS_OPEN_EXCL) {
			usb_device->usb_endp_excl_open &= ~ep_index_mask;
		}

		mutex_exit(&usb_device->usb_mutex);

		*pipe_handle = NULL;

	} else {

		(void) usb_pipe_release(*pipe_handle);
	}

	return (rval);
}


static void
usb_pipe_do_async_func(void *arg)
{
	usb_pipe_async_request_t *request =  (usb_pipe_async_request_t *)arg;
	usb_pipe_handle_impl_t *ph =
	    (usb_pipe_handle_impl_t *)request->pipe_handle;
	int rval;

	rval = request->sync_func(request->pipe_handle,
			request->usb_flags | USB_FLAGS_SLEEP);

	if (request->callback) {
		request->callback(request->callback_arg, rval, 0);
	}

	if (request->sync_func == usb_pipe_sync_close) {
		/*
		 * update pipe handle if it wasn't closed
		 */
		if (rval != USB_SUCCESS) {
			mutex_enter(&ph->p_mutex);
			ph->p_async_requests_count--;
			if (USB_PIPE_CLOSING(ph)) {
				ph->p_state = ph->p_last_state;
			}
			mutex_exit(&ph->p_mutex);
		}
	} else {
		mutex_enter(&ph->p_mutex);
		ph->p_async_requests_count--;
		mutex_exit(&ph->p_mutex);
	}

	kmem_free(request, sizeof (usb_pipe_async_request_t));
}


static int
usb_pipe_setup_async_request(
    int (*sync_func)(usb_pipe_handle_t, uint_t),
    usb_pipe_handle_t pipe_handle,
    uint_t usb_flags, void (*callback)(usb_opaque_t, int, uint_t),
    usb_opaque_t callback_arg)
{
	usb_pipe_async_request_t *request;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USB_DPRINTF_L3(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_setup_async_request: ph=0x%p, func=0x%p",
	    ph, sync_func);

	request = kmem_zalloc(sizeof (usb_pipe_async_request_t), KM_NOSLEEP);
	if (request == NULL) {

		USB_DPRINTF_L1(DPRINT_MASK_USBAI, usbai_log_handle,
		    "usb_pipe_setup_async_request failed: "
		    "ph=0x%p, func=0x%p", ph, sync_func);

		return (USB_FAILURE);
	}

	request->pipe_handle = pipe_handle;
	request->usb_flags = usb_flags;
	request->sync_func = sync_func;
	request->callback = callback;
	request->callback_arg = callback_arg;

	mutex_enter(&ph->p_mutex);
	ph->p_async_requests_count++;
	mutex_exit(&ph->p_mutex);

	if (!taskq_dispatch(usba_taskq, usb_pipe_do_async_func,
		(void *)request, KM_NOSLEEP)) {

		kmem_free(request, sizeof (usb_pipe_async_request_t));

		USB_DPRINTF_L1(DPRINT_MASK_USBAI, usbai_log_handle,
		    "taskq_dispatch failed: ph=0x%p, func=0x%p",
		    ph, sync_func);

		mutex_enter(&ph->p_mutex);
		ph->p_async_requests_count--;
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}


	return (USB_SUCCESS);
}


/*
 * usb_pipe_close:
 *	pipes can be closed synchronously or asynchronously.
 *	For asynchronous closing, we setup a request and create
 *	a new thread for doing the work.
 *
 * usb_pipe_sync_close:
 *	- check whether pipe is reserved by another pipehandle
 *	- reserve endpoint to avoid simultaneous opens
 *	- drain callbacks and async requests before closing pipe
 *	- stop polling if polling is active
 *	- call HCD to close the pipe
 *	- release the reservation
 */
static int
usb_pipe_sync_close(usb_pipe_handle_t pipe_handle, uint_t usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;
	usb_endpoint_descr_t *endpoint;
	int was_owned, n;
	uchar_t		ep_index, ep_index_mask;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_close: ph = 0x%p", pipe_handle);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	/* reserve pipe to avoid other opens */
	if (!usba_pipe_is_reserved(ph, usb_flags, USBA_RESERVE_PIPE)) {

		USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
		    "usb_pipe_sync_close: pipe reserved");

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);

	ASSERT(USB_PIPE_CLOSING(ph));

	usb_device = ph->p_usb_device;
	endpoint = ph->p_endpoint;
	ep_index = usba_get_ep_index(endpoint->bEndpointAddress);
	ep_index_mask = 1 << ep_index;

	mutex_exit(&ph->p_mutex);

	was_owned = usba_pipe_is_owned(ph);

	mutex_enter(&ph->p_mutex);

	/*
	 * before closing the pipe, ensure that no async threads
	 * are active. no threads can be started at this
	 * point because the closing flag has been set.
	 * Drain all callbacks
	 * We do this before closing the pipe in the HCD to avoid
	 * any unexpected races
	 * If the pipe is asynchronously closed by this thread
	 * then do not wait for this thread to exit (ie. do not deadlock)
	 */
	n = ((ph->p_state == USB_PIPE_STATE_ASYNC_CLOSING) ? 1 : 0);
	while (ph->p_async_requests_count > n) {
		mutex_exit(&ph->p_mutex);
		delay(drv_usectohz(1000));
		mutex_enter(&ph->p_mutex);
	}

	mutex_exit(&ph->p_mutex);

	rval = usb_device->usb_hcdi_ops->usb_hcdi_pipe_close(ph);

	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
		    "usb_pipe_sync_close: hcd close failed %d", rval);

		if (!was_owned) {
			(void) usb_pipe_release(pipe_handle);
		}

		return (rval);
	}

	(void) usb_pipe_release(pipe_handle);
	usba_destroy_pipe_handle(ph);

	mutex_enter(&usb_device->usb_mutex);
	usb_device->usb_endp_excl_open &= ~ep_index_mask;

	ASSERT((int)usb_device->usb_endp_open[ep_index] > 0);
	usb_device->usb_endp_open[ep_index]--;

	mutex_exit(&usb_device->usb_mutex);

	return (rval);
}


int
usb_pipe_close(usb_pipe_handle_t *pipe_handle,
			uint_t usb_flags,
			void	(*callback)(
				    usb_opaque_t callback_arg,
				    int	error_code,
				    uint_t flags),
			usb_opaque_t callback_arg)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)(*pipe_handle);
	int rval;

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
		    "usb_pipe_close: pipe closing already");

		return (USB_FAILURE);
	}

	/*
	 * save current state so we can restore the state to the old
	 * state if the pipe close fails
	 */
	ph->p_last_state = ph->p_state;
	ph->p_callers_pipe_handle_p = pipe_handle;
	if ((usb_flags & USB_FLAGS_SLEEP) == 0) {
		/*
		 * if we are closing a pipe asynchronously, set flag
		 * so we don't wait for ourselves when closing the
		 * pipe
		 */
		ph->p_state = USB_PIPE_STATE_ASYNC_CLOSING;
	} else {
		ph->p_state = USB_PIPE_STATE_SYNC_CLOSING;
	}
	mutex_exit(&ph->p_mutex);

	if (usb_flags & USB_FLAGS_SLEEP) {
		rval = usb_pipe_sync_close(*pipe_handle, usb_flags);
	} else {
		rval = usb_pipe_setup_async_request(
			usb_pipe_sync_close,
			*pipe_handle, usb_flags, callback,
			callback_arg);
		USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
		    "usb_pipe_close: async setup rval %d", rval);
	}

	if (rval != USB_SUCCESS) {
		mutex_enter(&ph->p_mutex);
		if (USB_PIPE_CLOSING(ph)) {
			ph->p_state = ph->p_last_state;
		}
		ph->p_callers_pipe_handle_p = NULL;
		mutex_exit(&ph->p_mutex);
	}

	return (rval);
}


/*
 * usb_pipe_get_state:
 *	Return the state of the pipe
 */
int
usb_pipe_get_state(usb_pipe_handle_t	pipe_handle,
		    usb_pipe_state_t	*pipe_state,
		    uint_t		usb_flags)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_get_state: ph = 0x%p uf=0x%x", ph, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	*pipe_state = ph->p_state;
	mutex_exit(&ph->p_mutex);

	return (USB_SUCCESS);
}


/*
 * usb_pipe_set_private:
 *	set private client date in the pipe handle
 */
int
usb_pipe_set_private(usb_pipe_handle_t	pipe_handle, usb_opaque_t data)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_set_private: ph = 0x%p", ph);

	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	ph->p_client_private = data;
	mutex_exit(&ph->p_mutex);

	return (USB_SUCCESS);
}


/*
 * usb_pipe_get_private:
 *	get private client date from the pipe handle
 */
usb_opaque_t
usb_pipe_get_private(usb_pipe_handle_t	pipe_handle)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_opaque_t	data;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_get_private: ph = 0x%p", ph);

	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (NULL);
	}
	data = ph->p_client_private;
	mutex_exit(&ph->p_mutex);

	return (data);
}


/*
 * usb_pipe_get_policy:
 *	- retrieve the pipe policy from the pipe handle
 */
int
usb_pipe_get_policy(usb_pipe_handle_t	pipe_handle,
		usb_pipe_policy_t	*pipe_policy)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_get_policy: ph = 0x%p pp = 0x%p", ph, pipe_policy);

	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	rval = usb_device->usb_hcdi_ops->usb_hcdi_pipe_get_policy(ph,
			pipe_policy);

	return (rval);
}


/*
 * usb_pipe_set_policy:
 *	- set a new pipe policy
 */
int
usb_pipe_set_policy(usb_pipe_handle_t	pipe_handle,
		usb_pipe_policy_t	*pipe_policy,
		uint_t			usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_set_policy: ph = 0x%p pp = 0x%p uf = 0x%x",
	    ph, pipe_policy, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	rval = usb_device->usb_hcdi_ops->usb_hcdi_pipe_set_policy(ph,
			pipe_policy, usb_flags);

	if (rval == USB_SUCCESS) {
		mutex_enter(&ph->p_mutex);
		bcopy(pipe_policy, ph->p_policy, sizeof (usb_pipe_policy_t));
		mutex_exit(&ph->p_mutex);
	}

	return (rval);
}


/*
 * usb_pipe_reserve:
 *	reserve pipe, block if USB_FLAGS_SLEEP set and ep has been reserved
 */
int
usb_pipe_reserve(usb_pipe_handle_t pipe_handle, uint_t	usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_reserve: ph = 0x%p uf = 0x%x", ph, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (rval);
	}
	mutex_exit(&ph->p_mutex);

	if (usba_pipe_is_reserved(ph, usb_flags, USBA_RESERVE_PIPE)) {
		rval = USB_SUCCESS;
	}

	return (rval);
}


/*
 * usb_pipe_release:
 *	- check if pipe is owned
 */
int
usb_pipe_release(usb_pipe_handle_t	pipe_handle)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;
	uchar_t		ep_index;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_release: ph = 0x%p", ph);

	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (!usba_pipe_is_owned(ph)) {

		return (USB_FAILURE);
	}

	mutex_enter(&ph->p_mutex);

	usb_device = ph->p_usb_device;
	ep_index = usba_get_ep_index(ph->p_endpoint->bEndpointAddress);

	mutex_exit(&ph->p_mutex);

	mutex_enter(&usb_device->usb_mutex);
	ASSERT(usb_device->usb_pipe_reserved[ep_index] == ph);

	usb_device->usb_pipe_reserved[ep_index] = NULL;
	rval = USB_SUCCESS;
	cv_signal(&usb_device->usb_cv_resrvd);

	mutex_exit(&usb_device->usb_mutex);

	return (rval);
}


/*
 * usb_pipe_abort
 *	- check on pipe reservation
 *	- force serialized access during abort operation
 *	- request HCD to abort
 *	- if successful, wait for completion
 */
static int
usb_pipe_sync_abort(usb_pipe_handle_t pipe_handle, uint_t usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_sync_abort: ph = 0x%p uf = 0x%x", ph, usb_flags);

	ASSERT(usb_flags & USB_FLAGS_SLEEP);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);

	if (usb_flags & USB_FLAGS_SLEEP) {
		usba_pipe_sync_init(ph, 0);
	}

	mutex_exit(&ph->p_mutex);

	rval = usb_device->usb_hcdi_ops->usb_hcdi_pipe_abort(ph, usb_flags);

	if ((rval == USB_SUCCESS) && (usb_flags & USB_FLAGS_SLEEP)) {
		usba_pipe_sync_completion(ph, &rval, NULL, NULL, 1);
	}

	mutex_enter(&ph->p_mutex);

	/*
	 * The host controller has stopped polling of the endpoint.
	 * Now, drain the callbacks if there are any on the callback
	 * queue.
	 */
	usba_drain_callbacks(ph);

	mutex_exit(&ph->p_mutex);

	return (rval);
}


int
usb_pipe_abort(usb_pipe_handle_t	pipe_handle,
			uint_t		usb_flags,
			void		(*callback)(
					    usb_opaque_t callback_arg,
					    int	error_code,
					    uint_t usb_flags),
			usb_opaque_t	callback_arg)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (usb_flags & USB_FLAGS_SLEEP) {
		return (usb_pipe_sync_abort(pipe_handle, usb_flags));
	} else {
		return (usb_pipe_setup_async_request(
			usb_pipe_sync_abort,
			pipe_handle, usb_flags, callback,
			callback_arg));
	}
}


/*
 * usb_pipe_reset
 *	- check on pipe reservation
 *	- serialize access during reset operation
 *	- request HCD to reset
 *	- if successful, wait for completion
 */
int
usb_pipe_sync_reset(usb_pipe_handle_t	pipe_handle,
			uint_t		usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;
	int		attribute;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_sync_reset: ph = 0x%p uf = 0x%x", (void *)ph, usb_flags);

	ASSERT(usb_flags & USB_FLAGS_SLEEP);
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	usba_pipe_sync_init(ph, 0);

	usb_device = ph->p_usb_device;

	mutex_exit(&ph->p_mutex);

	rval = usb_device->usb_hcdi_ops->usb_hcdi_pipe_reset(ph, usb_flags);

	if ((rval == USB_SUCCESS) && (usb_flags & USB_FLAGS_SLEEP)) {
		usba_pipe_sync_completion(ph, &rval, NULL, NULL, 1);
	}

	mutex_enter(&ph->p_mutex);

	/*
	 * The host controller has stopped polling of the endpoint.
	 * Now, drain the callbacks if there are any on the callback
	 * queue.
	 */
	usba_drain_callbacks(ph);

	/* Reset the pipe's state */
	attribute = ph->p_endpoint->bmAttributes & USB_EPT_ATTR_MASK;

	if ((attribute == USB_EPT_ATTR_CONTROL) ||
		(attribute == USB_EPT_ATTR_BULK)) {
		ph->p_state = USB_PIPE_STATE_ACTIVE;
	} else {
		ph->p_state = USB_PIPE_STATE_IDLE;
	}

	mutex_exit(&ph->p_mutex);

	return (rval);
}


int
usb_pipe_reset(usb_pipe_handle_t	pipe_handle,
			uint_t		usb_flags,
			void		(*callback)(
					    usb_opaque_t callback_arg,
					    int	error_code,
					    uint_t usb_flags),
			usb_opaque_t	callback_arg)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (usb_flags & USB_FLAGS_SLEEP) {
		return (usb_pipe_sync_reset(pipe_handle, usb_flags));
	} else {
		return (usb_pipe_setup_async_request(
			usb_pipe_sync_reset,
			pipe_handle, usb_flags, callback,
			callback_arg));
	}
}


/*
 * data transfer management
 *
 * usb_pipe_device_ctrl_receive
 *	- check for reserved and stall condition
 *	- request HCD to transport
 */
int
usb_pipe_device_ctrl_receive(usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	ushort_t	wValue,		/* varies according to request	*/
	ushort_t	wIndex,		/* index or offset		*/
	ushort_t	wLength,	/* number of bytes to xfer	*/
	uint_t		usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_device_ctrl_receive:\n\t"
	    "ph = 0x%p, setup = 0x%x 0x%x 0x%x 0x%x 0x%x uf = 0x%x",
	    ph, bmRequestType, bRequest, wValue, wIndex, wLength, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}
	mutex_exit(&ph->p_mutex);

	/* async transport, Check for sleep bit */
	ASSERT(!(usb_flags & USB_FLAGS_SLEEP));

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_device_ctrl_receive(ph,
		bmRequestType, bRequest,
		wValue,	wIndex,
		wLength, usb_flags);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_device_ctrl_receive: rval = %x", rval);

	return (rval);
}


/*
 * usb_pipe_device_ctrl_send
 *	- check for reserved and stall condition
 *	- request HCD to transport
 */
int
usb_pipe_device_ctrl_send(usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	ushort_t	wValue,		/* varies according to request	*/
	ushort_t	wIndex,		/* index or offset		*/
	ushort_t	wLength,	/* number of bytes to xfer	*/
	mblk_t		*data,		/* the data for the data phase	*/
					/* also includes length		*/
	uint_t		usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_device_ctrl_send:\n\t"
	    "ph = 0x%p setup = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x uf = 0x%x",
	    ph, bmRequestType, bRequest, wValue, wIndex, wLength,
	    data, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}
	mutex_exit(&ph->p_mutex);

	/* async transport, Check for sleep bit */
	ASSERT(!(usb_flags & USB_FLAGS_SLEEP));

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_device_ctrl_send(ph,
		bmRequestType, bRequest,
		wValue, wIndex, wLength,
		data, usb_flags);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_device_ctrl_send: rval = %x", rval);

	return (rval);
}


/*
 * usb_pipe_sync_device_ctrl_receive:
 *	- check for reserved and stall condition
 *	- initialize sync result structure in pipehandle
 *	- request transport
 *	- wait for completion and return results
 *
 */
int
usb_pipe_sync_device_ctrl_receive(usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	ushort_t	wValue,		/* varies according to request	*/
	ushort_t	wIndex,		/* index or offset		*/
	ushort_t	wLength,	/* number of bytes to xfer	*/
	mblk_t		**data,		/* the data for the data phase	*/
					/* allocated by the HCD		*/
				/* deallocated by consumer	*/
	uint_t		*completion_reason,
	uint_t		usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_sync_device_ctrl_receive:\n\t"
	    "ph = 0x%p setup = 0x%x 0x%x 0x%x 0x%x 0x%x uf = 0x%x",
	    ph, bmRequestType, bRequest, wValue, wIndex, wLength, usb_flags);

	USBA_CHECK_CONTEXT();
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}
	usb_device = ph->p_usb_device;

	usba_pipe_sync_init(ph, 0);

	mutex_exit(&ph->p_mutex);

	/* sync transport */
	usb_flags |= USB_FLAGS_SLEEP;

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_device_ctrl_receive(ph,
		bmRequestType, bRequest,
		wValue, wIndex, wLength,
		usb_flags);

	usba_pipe_sync_completion(ph, &rval, data, completion_reason, 0);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_sync_device_ctrl_receive: rval=%x reason=%x",
	    rval, *completion_reason);

	return (rval);
}


/*
 * usb_pipe_sync_device_ctrl_send:
 *	- check for reserved and stall condition
 *	- init sync transport
 *	- request HCD to transport
 *	- wait for completion
 */
int
usb_pipe_sync_device_ctrl_send(usb_pipe_handle_t pipe_handle,
	uchar_t		bmRequestType,	/* characteristics of request	*/
	uchar_t		bRequest,	/* specific request		*/
	ushort_t	wValue,		/* varies according to request	*/
	ushort_t	wIndex,		/* index or offset		*/
	ushort_t	wLength,	/* number of bytes to xfer	*/
	mblk_t		*data,		/* the data for the data phase	*/
					/* allocated by the client driver */
					/* deallocated by the HCD	*/
	uint_t		*completion_reason,
	uint_t		usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_sync_device_ctrl_send:\n\t"
	    "ph = 0x%p setup = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x uf = 0x%x",
	    ph, bmRequestType, bRequest, wValue, wIndex, wLength, data,
	    usb_flags);

	USBA_CHECK_CONTEXT();
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}
	usb_device = ph->p_usb_device;

	usba_pipe_sync_init(ph, 0);

	mutex_exit(&ph->p_mutex);

	/* sync transport */
	usb_flags |= USB_FLAGS_SLEEP;

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_device_ctrl_send(ph,
		bmRequestType, bRequest,
		wValue, wIndex, wLength,
		data, usb_flags);

	usba_pipe_sync_completion(ph, &rval, NULL, completion_reason, 1);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_sync_device_ctrl_send: rval=%x reason=%x",
	    rval, *completion_reason);

	return (rval);
}


/*
 * usb_pipe_start_polling:
 *	- check for reserved, stalled, active condition
 *	- request HCD to start polling
 */
int
usb_pipe_start_polling(usb_pipe_handle_t pipe_handle, uint_t usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_start_polling: ph = 0x%p uf = 0x%x", ph, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}

	if (ph->p_state == USB_PIPE_STATE_ACTIVE) {
		mutex_exit(&ph->p_mutex);

		return (USB_SUCCESS);
	}

	usb_device = ph->p_usb_device;

	/*
	 * set state to active so another usb_pipe_start_polling
	 * will be a NOP
	 */
	ph->p_state = USB_PIPE_STATE_ACTIVE;
	mutex_exit(&ph->p_mutex);

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_start_polling(ph, usb_flags);

	if (rval != USB_SUCCESS) {
		mutex_enter(&ph->p_mutex);
		if (!USB_PIPE_CLOSING(ph)) {
			ph->p_state = USB_PIPE_STATE_IDLE;
		}
		mutex_exit(&ph->p_mutex);
	}

	return (rval);
}


/*
 * usb_pipe_stop_polling:
 *	- check for reserved, stall and idle condition
 *	- set up for sync transport, if necessary
 *	- request HCD to stop polling
 *	- wait for completion if sync call
 *	- wait for draining of all callbacks
 */
int
usb_pipe_sync_stop_polling(usb_pipe_handle_t	pipe_handle,
			uint_t		usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_stop_polling: ph = 0x%p uf = 0x%x", ph, usb_flags);

	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}

	if (ph->p_state == USB_PIPE_STATE_IDLE) {
		mutex_exit(&ph->p_mutex);

		return (USB_SUCCESS);
	}

	usb_device = ph->p_usb_device;

	usba_pipe_sync_init(ph, 0);

	mutex_exit(&ph->p_mutex);

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_stop_polling(ph, usb_flags);

	/*
	 * The host controller has stopped polling of the endpoint.
	 * Now, drain the callbacks if there are any on the callback
	 * queue.
	 */
	mutex_enter(&ph->p_mutex);
	usba_drain_callbacks(ph);
	mutex_exit(&ph->p_mutex);

	if ((rval == USB_SUCCESS) && (usb_flags & USB_FLAGS_SLEEP)) {
		usba_pipe_sync_completion(ph, &rval, NULL, NULL, 1);
	}

	mutex_enter(&ph->p_mutex);
	ph->p_state = USB_PIPE_STATE_IDLE;
	mutex_exit(&ph->p_mutex);

	return (rval);
}

int
usb_pipe_stop_polling(usb_pipe_handle_t pipe_handle,
			uint_t		usb_flags,
			void		(*callback)(
					    usb_opaque_t callback_arg,
					    int error_code,
					    uint_t usb_flags),
			usb_opaque_t	callback_arg)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	mutex_exit(&ph->p_mutex);

	if (usb_flags & USB_FLAGS_SLEEP) {
		return (usb_pipe_sync_stop_polling(pipe_handle, usb_flags));
	} else {
		return (usb_pipe_setup_async_request(
			usb_pipe_sync_stop_polling,
			pipe_handle, usb_flags, callback,
			callback_arg));
	}
}


/*
 * usb_pipe_send_isoc_data:
 *	- check for pipe reserved or stalled
 *	- request HCD to transport isoc data asynchronously
 */
int
usb_pipe_send_isoc_data(usb_pipe_handle_t	pipe_handle,
			mblk_t			*data,
			uint_t			usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_send_isoc_data: ph = 0x%p data = 0x%x uf = 0x%x",
	    ph, data, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}

	if (ph->p_state == USB_PIPE_STATE_ACTIVE) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}

	mutex_exit(&ph->p_mutex);

	usb_flags &= ~USB_FLAGS_SLEEP;
	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_send_isoc_data(ph, data, usb_flags);

	return (rval);
}


/*
 * usb_pipe_bulk_transfer_size:
 *	- request HCD to return bulk max transfer data size
 */
int
usb_pipe_bulk_transfer_size(dev_info_t	*dip, size_t	*size)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_bulk_transfer_size: usb_device = 0x%p", usb_device);

	if ((usb_device) &&
		(usb_device->usb_hcdi_ops->usb_hcdi_bulk_transfer_size)) {

		return (usb_device->usb_hcdi_ops->usb_hcdi_bulk_transfer_size(
			dip, size));
	} else {
		*size = 0;
		return (USB_FAILURE);
	}
}


/*
 * usb_pipe_receive_bulk_data:
 *	- check for pipe reserved or stalled
 *	- request HCD to transport data asynchronously
 */
int
usb_pipe_receive_bulk_data(usb_pipe_handle_t	pipe_handle,
			size_t			length,
			uint_t			usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_receive_bulk_data: ph = 0x%p length = 0x%x uf = 0x%x",
	    ph, length, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}
	mutex_exit(&ph->p_mutex);

	usb_flags &= ~USB_FLAGS_SLEEP;

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_receive_bulk_data(ph, length, usb_flags);

	return (rval);
}


/*
 * usb_pipe_send_bulk_data:
 *	- check for pipe reserved or stalled
 *	- request HCD to transport data asynchronously
 */
int
usb_pipe_send_bulk_data(usb_pipe_handle_t	pipe_handle,
			mblk_t			*data,
			uint_t			usb_flags)
{
	int rval = USB_FAILURE;
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;
	usb_device_t	*usb_device = ph->p_usb_device;

	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_pipe_send_bulk_data: ph = 0x%p data = 0x%p uf = 0x%x",
	    ph, data, usb_flags);

	USBA_CHECK_CONTEXT_AND_FLAGS(usb_flags);
	usba_check_pipe_open(ph);

	if (!usba_pipe_is_reserved(ph, usb_flags, 0)) {

		return (USB_PIPE_RESERVED);
	}

	mutex_enter(&ph->p_mutex);
	if (USB_PIPE_CLOSING(ph)) {
		mutex_exit(&ph->p_mutex);

		return (USB_FAILURE);
	}
	if (ph->p_state == USB_PIPE_STATE_ERROR) {
		mutex_exit(&ph->p_mutex);

		return (USB_PIPE_ERROR);
	}
	mutex_exit(&ph->p_mutex);

	usb_flags &= ~USB_FLAGS_SLEEP;

	rval = usb_device->usb_hcdi_ops->
		usb_hcdi_pipe_send_bulk_data(ph, data, usb_flags);

	return (rval);
}


/*
 * utility function to return current usb address, mostly
 * for debugging purposes
 */
int
usb_get_addr(dev_info_t *dip)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	int address;

	mutex_enter(&usb_device->usb_mutex);
	address = usb_device->usb_addr;
	mutex_exit(&usb_device->usb_mutex);

	return (address);
}


/*
 * usb_get_interface returns -1 if the driver is responsible for
 * the entire device. Otherwise it returns the interface number
 */
int
usb_get_interface_number(dev_info_t *dip)
{
	return (ddi_prop_get_int(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "interface", -1));
}


/*
 * utilitie functions to get device and config descriptor from
 * usb_device
 */
usb_device_descr_t *
usb_get_dev_descr(dev_info_t *dip)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	usb_device_descr_t *usb_dev_descr;

	mutex_enter(&usb_device->usb_mutex);
	usb_dev_descr = usb_device->usb_dev_descr;
	mutex_exit(&usb_device->usb_mutex);

	return (usb_dev_descr);
}


uchar_t *
usb_get_raw_config_data(dev_info_t *dip, size_t *length)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);
	uchar_t		*usb_config;

	mutex_enter(&usb_device->usb_mutex);
	usb_config = usb_device->usb_config;
	*length = usb_device->usb_config_length;
	mutex_exit(&usb_device->usb_mutex);

	return (usb_config);
}


/*
 * get a string descriptor in caller-provided buffer. it assumes
 * that the caller has not opened the default pipe exclusively
 */
int
usb_get_string_descriptor(dev_info_t *dip, uint16_t langid, uint8_t index,
	char *buf, size_t buflen)
{
	usb_pipe_handle_t ph;
	mblk_t		*data = NULL;
	uint16_t	length;
	int		rval;
	uint_t		completion_reason;
	size_t		len;

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usbai_log_handle,
	    "usb_get_string_descriptor: %s, langid=0x%x, index=%d",
	    ddi_node_name(dip), langid, index);

	USBA_CHECK_CONTEXT();

	/*
	 * open default pipe
	 */
	if ((rval = usb_pipe_open(dip, NULL, NULL, USB_FLAGS_SLEEP, &ph)) !=
	    USB_SUCCESS) {

		return (rval);
	}

	/*
	 * determine the length of the descriptor
	 */
	rval = usb_pipe_sync_device_ctrl_receive(ph,
		USB_DEV_REQ_DEVICE_TO_HOST,
		USB_REQ_GET_DESCRIPTOR,
		USB_DESCR_TYPE_STRING << 8 | index & 0xff,
		langid,
		4,
		&data,
		&completion_reason,
		USB_FLAGS_SLEEP);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usbai_log_handle,
	    "rval = %d, cr=%d", rval, completion_reason);

	if (rval != USB_SUCCESS || completion_reason != USB_CC_NOERROR) {
		goto done;
	}

	length = *(data->b_rptr);
	freemsg(data);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usbai_log_handle,
	    "length = %d, langid=%x", length, langid);

	rval = usb_pipe_sync_device_ctrl_receive(ph,
		USB_DEV_REQ_DEVICE_TO_HOST,
		USB_REQ_GET_DESCRIPTOR,
		USB_DESCR_TYPE_STRING << 8 | index & 0xff,
		langid,
		length,
		&data,
		&completion_reason,
		USB_FLAGS_SLEEP);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usbai_log_handle,
	    "rval = %d, cr=%d", rval, completion_reason);

	if (rval != USB_SUCCESS || completion_reason != USB_CC_NOERROR) {
		goto done;
	}

	len = usb_ascii_string_descr(data->b_rptr, length, buf, buflen);

	USB_DPRINTF_L4(DPRINT_MASK_USBA, usbai_log_handle, "buf=%s", buf);

	ASSERT(len <= buflen);

	freemsg(data);

done:
	(void) usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);

	return (rval);
}


char *
usb_get_usbdev_strdescr(dev_info_t *dip)
{
	usb_device_t	*usb_device;

	usb_device = usba_get_usb_device(dip);
	return (usb_device->usb_string_descr);
}


int
usb_check_same_device(dev_info_t *dip)
{
	usb_device_descr_t	usb_dev_descr;
	usb_device_t		*usb_device;
	usb_pipe_handle_t	ph = NULL;
	mblk_t			*pdata = NULL;
	uint_t			completion_reason;
	uint16_t		length;
	int			rval;
	char			*buf;

	USBA_CHECK_CONTEXT();
	usb_device = usba_get_usb_device(dip);

	/* open control pipe */
	rval = usb_pipe_open(dip, NULL, NULL, USB_FLAGS_SLEEP, &ph);

	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usbai_log_handle,
		    "usb_check_same_device : usb_pipe_open failed (%d)", rval);

		return (USB_FAILURE);
	}

	length = usb_device->usb_dev_descr->bLength;

	/* get the device descriptor */
	rval = usb_pipe_sync_device_ctrl_receive(ph,
			USB_DEV_REQ_DEVICE_TO_HOST |
				USB_DEV_REQ_TYPE_STANDARD,
			USB_REQ_GET_DESCRIPTOR,		/* bRequest */
			USB_DESCR_TYPE_SETUP_DEVICE,	/* wValue */
			0,				/* wIndex */
			length,				/* wLength */
			&pdata,
			&completion_reason,
			0);

	(void) usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);

	if (rval != USB_SUCCESS) {
		USB_DPRINTF_L3(DPRINT_MASK_USBA, usbai_log_handle,
		    "getting device descriptor failed (%d)", rval);

		return (USB_FAILURE);
	}

	ASSERT(completion_reason == USB_CC_NOERROR);
	ASSERT(pdata != NULL);

	(void) usb_parse_device_descr(pdata->b_rptr,
		pdata->b_wptr - pdata->b_rptr, &usb_dev_descr,
		sizeof (usb_device_descr_t));

	freemsg(pdata);

	/*
	 * At first, we check the device descriptor
	 * If the descriptor obtained is identical to the one in dip
	 * we compare the string descriptor if available.
	 * Return true if both are identical.
	 */
	if (usb_dev_descr.bLength == length) {
		if (bcmp((char *)usb_device->usb_dev_descr,
		    (char *)&usb_dev_descr, length) == 0) {

			/*
			 * if this device has a string descriptor,
			 * check and compare
			 */
			if (usb_device->usb_string_descr) {

				buf = kmem_zalloc(USB_MAXSTRINGLEN, KM_SLEEP);

				if (usba_get_mfg_product_sn_strings(dip, buf,
				    USB_MAXSTRINGLEN) == USB_SUCCESS) {
					length = max(strlen(buf),
							strlen(usb_device->
							usb_string_descr));

					rval = bcmp(buf,
						usb_device->usb_string_descr,
						length);
					kmem_free(buf, USB_MAXSTRINGLEN);
					return (rval ? USB_FAILURE :
						USB_SUCCESS);

				} else {
					kmem_free(buf, USB_MAXSTRINGLEN);
					return (USB_FAILURE);
				}
			} else {
				return (USB_SUCCESS);
			}
		} else {
			return (USB_FAILURE);
		}
	} else {
		return (USB_FAILURE);
	}
}


/*
 * function returning dma attributes of the HCD
 */
ddi_dma_attr_t *
usb_get_hc_dma_attr(dev_info_t *dip)
{
	usb_device_t *usb_device = usba_get_usb_device(dip);
	usb_hcdi_t *hcdi =
		usba_hcdi_get_hcdi(usb_device->usb_root_hub_dip);

	return (hcdi->hcdi_dma_attr);
}


/*
 * function used to dispatch a request to the taskq
 */
int
usb_taskq_request(void (*func)(void *), void *arg, uint_t flag)
{
	USB_DPRINTF_L4(DPRINT_MASK_USBA, usbai_log_handle,
	    "flag = 0x%x", func, arg, flag);

	if (!taskq_dispatch(usba_taskq, func, (void *)arg, flag)) {

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/*
 * usb_endpoint_num:
 *	return endpoint number for a given pipe handle
 */
ushort_t
usb_endpoint_num(usb_pipe_handle_t pipe_handle)
{
	usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)pipe_handle;

	return (ph->p_endpoint->bEndpointAddress & USB_EPT_ADDR_MASK);
}


/*
 * debug, log, and console message handling
 */
usb_log_handle_t
usb_alloc_log_handle(dev_info_t *dip, char *name,
	uint_t *errlevel, uint_t *mask, uint_t *instance_filter,
	uint_t *show_label, uint_t flags)
{
	usb_log_handle_impl_t	*hdl;

	USBA_CHECK_CONTEXT();
	hdl = kmem_zalloc(sizeof (*hdl), KM_SLEEP);

	hdl->lh_dip = dip;
	if (dip && (name == NULL)) {
		hdl->lh_name = (char *)ddi_driver_name(dip);
	} else {
		hdl->lh_name = name;
	}
	hdl->lh_errlevel = errlevel;
	hdl->lh_mask = mask;
	hdl->lh_instance_filter = instance_filter;
	hdl->lh_show_label = show_label;
	hdl->lh_flags = flags;

	return ((usb_log_handle_t)hdl);
}


void
usb_free_log_handle(usb_log_handle_t handle)
{
	kmem_free(handle, sizeof (usb_log_handle_impl_t));
}


/*
 * variable args printf
 */
static void
usba_clear_print_buf()
{
	usba_buf_sptr = usba_debug_buf;
	usba_buf_eptr = usba_debug_buf + usba_debug_buf_size;
	*usba_debug_buf = 0;
}

static void
usb_vprintf(dev_info_t *dip, int level, char *label, char *fmt, va_list ap)
{
	size_t len;
	char driver_name[32];
	char *msg_ptr;

	if (usba_suppress_dprintf) {

		return;
	}

	*driver_name = '\0';
	mutex_enter(&usba_print_mutex);

	/*
	 * Check if we have a valid buf size?
	 * Suppress logging to usb_buffer if so.
	 */
	if (usba_debug_buf_size <= 0) {

		usba_buffer_dprintf = 0;
	}

	/*
	 * if there is label and dip, use <driver name><instance>:
	 * otherwise just use the label
	 */
	if (label == (char *)NULL) {
		(void) sprintf(usba_print_buf, "\t");
	} else {
		if (dip) {
			(void) sprintf(usba_print_buf, "%s%d:\t",
			    label, ddi_get_instance(dip));
			(void) sprintf(driver_name, "(%s%d):",
			    ddi_driver_name(dip), ddi_get_instance(dip));
		} else {
			(void) sprintf(usba_print_buf, "%s:\t", label);
		}
	}

	msg_ptr = usba_print_buf + strlen(usba_print_buf);
	(void) vsprintf(msg_ptr, fmt, ap);

	len = strlen(usba_print_buf);
	usba_print_buf[len++] = '\n';
	usba_print_buf[len] = '\0';

	/*
	 * stuff the message in the debug buf
	 */
	if (usba_buffer_dprintf) {
		if (usba_debug_buf == NULL) {
			usba_debug_buf = (char *)kmem_zalloc(
			    usba_debug_buf_size + USB_DEBUG_SIZE_EXTRA_ALLOC,
			    KM_SLEEP);
			usba_clear_print_buf();
		}

		/*
		 * overwrite >>>> that might be over the end of the
		 * the buffer
		 */
		*(usba_debug_buf + usba_debug_buf_size) = '\0';

		if (usba_buf_sptr + len > usba_buf_eptr) {
			size_t left = usba_buf_eptr - usba_buf_sptr;

			bcopy((caddr_t)usba_print_buf,
				(caddr_t)usba_buf_sptr, left);
			bcopy((caddr_t)usba_print_buf + left,
				(caddr_t)usba_debug_buf,
				len - left);
			usba_buf_sptr = usba_debug_buf + len - left;
		} else {
			bcopy((caddr_t)usba_print_buf,
				usba_buf_sptr, len);
			usba_buf_sptr += len;
		}
		/* add marker */
		(void) sprintf(usba_buf_sptr, ">>>>\0");
	}

	/*
	 * L4-L2 message may go to the log buf if not logged in usba_debug_buf
	 * L1 messages will go to the log buf in non-debug kernels and
	 * to console and log buf in debug kernels
	 * L0 messages are warnings and will go to console and log buf and
	 * include the pathname, if available
	 */

	switch (level) {
	case USB_LOG_L4:
	case USB_LOG_L3:
	case USB_LOG_L2:
		if (!usba_buffer_dprintf) {
			cmn_err(CE_CONT, "^%s", usba_print_buf);
		}
		break;
	case USB_LOG_L1:
#ifdef DEBUG
		cmn_err(CE_CONT, "%s", usba_print_buf);
#else
		if (dip) {
			char *pathname = kmem_alloc(MAXPATHLEN, KM_NOSLEEP);
			if (pathname) {
				cmn_err(CE_CONT, "?%s %s %s",
				    ddi_pathname(dip, pathname),
				    driver_name, msg_ptr);
				kmem_free(pathname, MAXPATHLEN);
			} else {
				cmn_err(CE_CONT, "?%s", usba_print_buf);
			}
		} else {
			cmn_err(CE_CONT, "?%s", usba_print_buf);
		}
#endif
		break;
	case USB_LOG_L0:
		if (dip) {
			char *pathname = kmem_alloc(MAXPATHLEN,
							KM_NOSLEEP);
			if (pathname) {
				cmn_err(CE_WARN, "%s %s %s",
				    ddi_pathname(dip, pathname),
				    driver_name, msg_ptr);
				kmem_free(pathname, MAXPATHLEN);
			} else {
				cmn_err(CE_WARN, usba_print_buf);
			}
		} else {
			cmn_err(CE_WARN, usba_print_buf);
		}
		break;
	}

	mutex_exit(&usba_print_mutex);
}


static int
usb_vlog(usb_log_handle_t handle, uint_t level, uint_t mask,
	char *fmt, va_list ap)
{
	usb_log_handle_impl_t *hdl = (usb_log_handle_impl_t *)handle;
	char *label;
	uint_t hdl_errlevel, hdl_mask, hdl_instance_filter;

	/* if there is no handle, use usba as label */
	if (hdl == NULL) {
		usb_vprintf(NULL, level, "usba", fmt, ap);

		return (USB_SUCCESS);
	}

	/* look up the filters and set defaults */
	if (hdl->lh_errlevel) {
		hdl_errlevel = *(hdl->lh_errlevel);
	} else {
		hdl_errlevel = 0;
	}

	if (hdl->lh_mask) {
		hdl_mask = *(hdl->lh_mask);
	} else {
		hdl_mask = (uint_t)-1;
	}

	if (hdl->lh_instance_filter) {
		hdl_instance_filter = *(hdl->lh_instance_filter);
	} else {
		hdl_instance_filter = (uint_t)-1;
	}

	/* if threshold is lower or mask doesn't match, we are done */
	if ((level > hdl_errlevel) || ((mask & hdl_mask) == 0)) {

		return (USB_FAILURE);
	}

	/*
	 * if we have a dip, and it is not a warning, check
	 * the instance number
	 */
	if (hdl->lh_dip && (level > USB_LOG_L0)) {
		if ((hdl_instance_filter != (uint_t)-1) &&
		    (ddi_get_instance(hdl->lh_dip) != hdl_instance_filter)) {

			return (USB_FAILURE);
		}
	}

	if (hdl->lh_show_label && (*(hdl->lh_show_label) == 0)) {
		label = NULL;
	} else {
		label = hdl->lh_name;
	}

	usb_vprintf(hdl->lh_dip, level, label, fmt, ap);

	return (USB_SUCCESS);
}


void
usb_dprintf4(uint_t mask, usb_log_handle_t handle, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) usb_vlog(handle, USB_LOG_L4, mask, fmt, ap);
	va_end(ap);
}

void
usb_dprintf3(uint_t mask, usb_log_handle_t handle, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) usb_vlog(handle, USB_LOG_L3, mask, fmt, ap);
	va_end(ap);
}

void
usb_dprintf2(uint_t mask, usb_log_handle_t handle, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) usb_vlog(handle, USB_LOG_L2, mask, fmt, ap);
	va_end(ap);
}

void
usb_dprintf1(uint_t mask, usb_log_handle_t handle, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) usb_vlog(handle, USB_LOG_L1, mask, fmt, ap);
	va_end(ap);
}

void
usb_dprintf0(uint_t mask, usb_log_handle_t handle, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) usb_vlog(handle, USB_LOG_L0, mask, fmt, ap);
	va_end(ap);
}

int
usb_log(usb_log_handle_t handle, uint_t level, uint_t mask, char *fmt, ...)
{
	va_list	ap;
	int rval;

	va_start(ap, fmt);
	rval = usb_vlog(handle, level, mask, fmt, ap);
	va_end(ap);

	return (rval);
}


static void
usba_async_request_raise_power(void *arg)
{
	pm_request_t *pmrq = (pm_request_t *)arg;
	int rval;

	/*
	 * To eliminate race condition between the call to power entry
	 * point and our call to raise power level, we first mark the
	 * component busy and later idle
	 */
	(void) pm_busy_component(pmrq->dip, pmrq->comp);
	rval = pm_raise_power(pmrq->dip, pmrq->comp, pmrq->level);
	(void) pm_idle_component(pmrq->dip, pmrq->comp);
	pmrq->cb(pmrq->arg, rval);
}


/* usb function to perform async pm_request_power_change */
int
usb_request_raise_power(dev_info_t *dip, int comp, int level,
	void (*callback)(void *, int), void *arg, uint_t flags)
{
	pm_request_t *pmrq;

	if (flags & USB_FLAGS_SLEEP) {
		return (pm_raise_power(dip, comp, level));
	}

	pmrq = kmem_alloc(sizeof (pm_request_t), KM_NOSLEEP);
	if (pmrq == NULL) {

		return (USB_FAILURE);
	}

	pmrq->dip = dip;
	pmrq->comp = comp;
	pmrq->level = level;
	pmrq->cb = callback;
	pmrq->arg = arg;
	pmrq->flags = flags;

	if (!taskq_dispatch(usba_taskq,
	    usba_async_request_raise_power, (void *)pmrq, KM_NOSLEEP)) {

		kmem_free(pmrq, sizeof (pm_request_t));

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


static void
usba_async_request_lower_power(void *arg)
{
	pm_request_t *pmrq = (pm_request_t *)arg;
	int rval;

	/*
	 * To eliminate race condition between the call to power entry
	 * point and our call to lower power level, we call idle component
	 * to push ahead the PM timestamp
	 */
	(void) pm_idle_component(pmrq->dip, pmrq->comp);
	rval = pm_lower_power(pmrq->dip, pmrq->comp, pmrq->level);
	pmrq->cb(pmrq->arg, rval);
}


/* usb function to perform async pm_request_power_change */
int
usb_request_lower_power(dev_info_t *dip, int comp, int level,
	void (*callback)(void *, int), void *arg, uint_t flags)
{
	pm_request_t *pmrq;

	if (flags & USB_FLAGS_SLEEP) {
		return (pm_lower_power(dip, comp, level));
	}

	pmrq = kmem_alloc(sizeof (pm_request_t), KM_NOSLEEP);
	if (pmrq == NULL) {

		return (USB_FAILURE);
	}

	pmrq->dip = dip;
	pmrq->comp = comp;
	pmrq->level = level;
	pmrq->cb = callback;
	pmrq->arg = arg;
	pmrq->flags = flags;

	if (!taskq_dispatch(usba_taskq,
	    usba_async_request_lower_power, (void *)pmrq, KM_NOSLEEP)) {

		kmem_free(pmrq, sizeof (pm_request_t));

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/* function to see if pm is enabled for this device */
int
usb_is_pm_enabled(dev_info_t *dip)
{
	usb_device_t	*usb_device = usba_get_usb_device(dip);

	if (usb_device->usb_hcdi_ops->hcdi_pm_enable) {
		if (strcmp(ddi_node_name(dip), "mouse") == 0) {

			return (usb_pm_mouse ? USB_SUCCESS : USB_FAILURE);
		}

		return (USB_SUCCESS);
	}

	return (USB_FAILURE);
}


/*
 * usba_enable_device_remote_wakeup:
 *	internal function to enable remote wakeup in the device
 *	or interface
 */
static int
usba_enable_device_remote_wakeup(dev_info_t *dip)
{
	int		rval, retval;
	uint_t		completion_reason;
	usb_pipe_handle_t ph;
	uint8_t 	bmRequest = USB_DEV_REQ_HOST_TO_DEV;
	uint16_t	wIndex = 0;
	int		interface;

	USBA_CHECK_CONTEXT();

	/* do we own the device? */
	if ((interface = usb_get_interface_number(dip)) == -1) {
		bmRequest |= USB_DEV_REQ_RECIPIENT_DEVICE;
	} else {
		bmRequest |= USB_DEV_REQ_RECIPIENT_INTERFACE;
		wIndex = (uint8_t)interface;
	}

	/* open default pipe */
	rval = usb_pipe_open(dip, NULL, NULL,
		USB_FLAGS_OPEN_EXCL | USB_FLAGS_SLEEP, &ph);
	ASSERT(rval == USB_SUCCESS);

	retval = usb_pipe_sync_device_ctrl_send(
		ph,
		bmRequest,
		USB_REQ_SET_FEATURE,	/* bRequest */
		USB_DEVICE_REMOTE_WAKEUP, /* wValue */
		wIndex, 		/* wIndex */
		0,			/* wLength */
		NULL,
		&completion_reason,
		USB_FLAGS_ENQUEUE);

	if (retval != USB_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_USBAI, usbai_log_handle,
			"SetFeature(RemoteWakep) failed: "
			"rval = %d cr = %d", rval, completion_reason);

		switch (completion_reason) {
		case USB_CC_STALL:
			rval = usb_pipe_reset(ph,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);

			/* clear endpoint stall */
			rval = usb_pipe_sync_device_ctrl_send(
			    ph,
			    USB_DEV_REQ_HOST_TO_DEV |
			    USB_DEV_REQ_RECIPIENT_ENDPOINT,
			    USB_REQ_CLEAR_FEATURE,
			    USB_ENDPOINT_HALT, /* Endpt Stall */
			    usb_endpoint_num(ph),
			    0,
			    NULL,
			    &completion_reason,
			    USB_FLAGS_ENQUEUE);
			ASSERT(rval == USB_SUCCESS);
			break;

		default:
			rval = usb_pipe_reset(ph,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);
			break;
		}
	}

	/* Now close the pipe */
	rval = usb_pipe_close(&ph, USB_FLAGS_SLEEP, NULL, NULL);
	ASSERT(rval == USB_SUCCESS);

	return (retval);
}


void
usb_enable_parent_notification(dev_info_t *dip)
{
	int rval;

	USBA_CHECK_CONTEXT();
	rval = ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
			"pm-want-child-notification?", 0, 0);
	ASSERT(rval == DDI_PROP_SUCCESS);
}


/*
 * usb_enable_remote_wakeup:
 *	check if device supports remote wakeup and, if so, enable
 *	remote wake up in the device
 */
int
usb_enable_remote_wakeup(dev_info_t *dip)
{
	usb_config_descr_t	conf_descr;
	uchar_t 		*usb_config;	/* buf for config descriptor */
	size_t			config_length;
	int			rval;

	USBA_CHECK_CONTEXT();

	/* Obtain the raw configuration descriptor */
	usb_config = usb_get_raw_config_data(dip, &config_length);

	/* get configuration descriptor, must succeed */
	rval = usb_parse_configuration_descr(usb_config, config_length,
		&conf_descr, USB_CONF_DESCR_SIZE);
	ASSERT(rval == USB_CONF_DESCR_SIZE);

	/*
	 * If the device supports remote wakeup, and PM is enabled,
	 * we enable remote wakeup in the device
	 */
	if ((usb_is_pm_enabled(dip) == USB_SUCCESS) &&
	    (conf_descr.bmAttributes & USB_CONF_ATTR_REMOTE_WAKEUP)) {

		rval = usba_enable_device_remote_wakeup(dip);
	} else {
		rval = USB_FAILURE;
	}

	return (rval);
}


/*
 * usb_create_pm_components:
 *	map descriptor into  pm properties
 */
int
usb_create_pm_components(dev_info_t *dip, uint_t *pwr_states)
{
	uchar_t 		*usb_config;	/* buf for config descriptor */
	usb_config_descr_t	conf_descr;
	size_t			config_length;
	usb_config_pwr_descr_t	confpwr_descr;
	usb_interface_pwr_descr_t ifpwr_descr;
	uint8_t 		conf_attrib;
	int			i, lvl, rval;
	int			n_prop = 0;
	uint8_t 		*ptr;
	char			*drvname;
	char			str[40];
	char			*pm_comp[USB_PMCOMP_NO];
	int			interface;

	USBA_CHECK_CONTEXT();

	/* Obtain the raw configuration descriptor */
	usb_config = usb_get_raw_config_data(dip, &config_length);

	/* get configuration descriptor, must succceed */
	rval = usb_parse_configuration_descr(usb_config, config_length,
		&conf_descr, USB_CONF_DESCR_SIZE);
	ASSERT(rval == USB_CONF_DESCR_SIZE);

	conf_attrib = conf_descr.bmAttributes;
	*pwr_states = 0;

	/*
	 * Now start creating the pm-components strings
	 */
	drvname = (char *)ddi_driver_name(dip);
	(void) sprintf(str, "NAME= %s%d Power", drvname, ddi_get_instance(dip));

	pm_comp[n_prop] = kmem_zalloc(strlen(str) + 1, KM_SLEEP);
	(void) strcpy(pm_comp[n_prop++], str);

	/*
	 * if the device is bus powered we look at the bBusPowerSavingDx
	 * fields else we look at bSelfPowerSavingDx fields.
	 * OS and USB power states are numerically reversed,
	 *
	 * Here is the mapping :-
	 *	OS State	USB State
	 *	0		D3	(minimal or no power)
	 *	1		D2
	 *	2		D1
	 *	3		D0	(Full power)
	 *
	 * if we own the whole device, we look at the config pwr descr
	 * else at the interface pwr descr.
	 */
	if ((interface = usb_get_interface_number(dip)) == -1) {
		/* Parse the configuration power descriptor */
		rval = usb_parse_config_pwr_descr(usb_config, config_length,
		    &confpwr_descr, USB_CONF_PWR_DESCR_SIZE);

		if (rval != USB_CONF_PWR_DESCR_SIZE) {
			USB_DPRINTF_L2(DPRINT_MASK_USBAI, usbai_log_handle,
			    "usb_create_pm_components: "
			    "usb_parse_config_pwr_descr returns length of %d, "
			    "expecting %d", rval, USB_CONF_PWR_DESCR_SIZE);

			return (USB_FAILURE);
		}

		if (conf_attrib & USB_CONF_ATTR_SELFPWR) {
			ptr = &confpwr_descr.bSelfPowerSavingD3;
		} else {
			ptr = &confpwr_descr.bBusPowerSavingD3;
		}
	} else {
		/* Parse the interface power descriptor */
		rval = usb_parse_interface_pwr_descr(usb_config,
			config_length,
			interface,		/* interface index */
			0,			/* XXXX alt interface index */
			&ifpwr_descr,
			USB_IF_PWR_DESCR_SIZE);

		if (rval != USB_IF_PWR_DESCR_SIZE) {
			USB_DPRINTF_L2(DPRINT_MASK_USBAI, usbai_log_handle,
			    "usb_create_pm_components: "
			    "usb_parse_interface_pwr_descr "
			    "returns length of %d, "
			    "expecting %d", rval, USB_CONF_PWR_DESCR_SIZE);

			return (USB_FAILURE);
		}

		if (conf_attrib & USB_CONF_ATTR_SELFPWR) {
			ptr =  &ifpwr_descr.bSelfPowerSavingD3;
		} else {
			ptr =  &ifpwr_descr.bBusPowerSavingD3;
		}
	}

	/* walk thru levels and create prop level=name strings */
	for (lvl = USB_DEV_OS_POWER_0; lvl <= USB_DEV_OS_POWER_3; lvl++) {
		if (*ptr || (lvl == USB_DEV_OS_POWER_3)) {
			(void) sprintf(str, "%d=USB D%d State",
			    lvl, USB_DEV_OS_PWR2USB_PWR(lvl));
			pm_comp[n_prop] = kmem_zalloc(strlen(str) + 1,
								KM_SLEEP);
			(void) strcpy(pm_comp[n_prop++], str);

			*pwr_states |= USB_DEV_PWRMASK(lvl);
		}

		ptr -= 2; /* skip to the next power state */
	}

	USB_DPRINTF_L3(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_create_pm_components: pwr_states: %x", *pwr_states);

	/* now create the actual components */
	rval = ddi_prop_update_string_array(DDI_DEV_T_NONE, dip,
			"pm-components", pm_comp, n_prop);
	ASSERT(rval == DDI_PROP_SUCCESS);

	/* display & delete properties */
	USB_DPRINTF_L3(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_create_pm_components: The properties are:");
	for (i = 0; i < n_prop; i++) {
		USB_DPRINTF_L3(DPRINT_MASK_USBAI, usbai_log_handle,
		    "\t%s", pm_comp[i]);
		kmem_free(pm_comp[i], strlen(pm_comp[i]) + 1);
	}

	return (USB_SUCCESS);
}


/*
 * Generic Functions to set the power level of any usb device
 *
 * Since OS and USB power states are numerically reverse,
 * Here is the mapping :-
 *	OS State	USB State
 *	0		D3	(minimal or no power)
 *	1		D2
 *	2		D1
 *	3		D0	(Full power)
 */

/* set device power level to 0 (full power) */
/*ARGSUSED*/
int
usb_set_device_pwrlvl0(dev_info_t *dip)
{
	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_set_device_pwrlvl0 : Not Yet Implemented");

	return (USB_SUCCESS);
}


/* set device power level to 1  */
/*ARGSUSED*/
int
usb_set_device_pwrlvl1(dev_info_t *dip)
{
	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_set_device_pwrlvl1 : Not Yet Implemented");

	return (USB_SUCCESS);
}


/* set device power level to 2  */
/*ARGSUSED*/
int
usb_set_device_pwrlvl2(dev_info_t *dip)
{
	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_set_device_pwrlvl2 : Not Yet Implemented");

	return (USB_SUCCESS);
}


/* set device power level to 3  */
/*ARGSUSED*/
int
usb_set_device_pwrlvl3(dev_info_t *dip)
{
	USB_DPRINTF_L4(DPRINT_MASK_USBAI, usbai_log_handle,
	    "usb_set_device_pwrlvl3 : Not Yet Implemented");

	return (USB_SUCCESS);
}
