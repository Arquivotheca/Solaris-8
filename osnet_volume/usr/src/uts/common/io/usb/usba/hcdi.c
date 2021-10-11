/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)hcdi.c	1.11	99/11/05 SMI"

/*
 * USBA: Solaris USB Architecture support
 *
 * hcdi.c contains the code for client driver callbacks.  A host controller
 * driver registers/unregisters with usba through usba_hcdi_register/unregister.
 *
 * When the transfer has finished, the host controller driver will call into
 * usba with the result.  The call is usba_hcdi_callback().  This function
 * does different actions based on the transfer type.  If the transfer is
 * synchronous, the arguments are stuffed into the pipe handle, and the
 * waiting thread is signaled.  If the transfer is asynchronous, the
 * callback is put on the callback queue.
 *
 * The callback queue is maintained in FIFO order.  usba_hcdi_callback
 * adds to the queue, and usba_hcdi_softintr takes the callbacks off the queue
 * and executes them.  The soft interrupt handler only handles those callbacks
 * on the queue since its last invocation, so it is not possible for the
 * soft interrupt handler to run forever.
 */

#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/ddi_impldefs.h>
#include <sys/kstat.h>

/*
 * Dump support
 */
#ifdef	DEBUG
static kmutex_t	usba_hcdi_dump_mutex;
static void usba_hcdi_dump(uint_t, usb_opaque_t);
static void usba_hcdi_dump_cb_list(usb_hcdi_t *, uint_t);
#endif	/* DEBUG */

static uint_t usba_hcdi_soft_intr(caddr_t arg);
static void usba_hcdi_create_stats(usb_hcdi_t *, int);
static void usba_hcdi_destroy_stats(usb_hcdi_t *);

void
usba_hcdi_initialization()
{
#ifdef	DEBUG
	mutex_init(&usba_hcdi_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif	/* DEBUG */
}


void
usba_hcdi_destroy()
{
#ifdef	DEBUG
	mutex_destroy(&usba_hcdi_dump_mutex);
#endif	/* DEBUG */
}


/*
 * store hcdi structure in the dip
 */
void
usba_hcdi_set_hcdi(dev_info_t *dip, usb_hcdi_t *hcdi)
{
	DEVI(dip)->devi_driver_data = (caddr_t)hcdi;
}


/*
 * retrieve hcdi structure from the dip
 */
usb_hcdi_t *
usba_hcdi_get_hcdi(dev_info_t *dip)
{
	return ((usb_hcdi_t *)DEVI(dip)->devi_driver_data);
}


/*
 * Called by an	HCD to attach an instance of the driver
 *	make this instance known to USBA
 *	the HCD	should initialize usb_hcdi structure prior
 *	to calling this	interface
 */
int	usba_hcdi_register(usba_hcdi_register_args_t *args,
		uint_t			flags)
{
	usb_hcdi_t *hcdi = kmem_zalloc(sizeof (usb_hcdi_t), KM_SLEEP);

	if (args->usba_hcdi_register_version != HCDI_REGISTER_VERS_0) {
		kmem_free(hcdi, sizeof (usb_hcdi_t));

		return (DDI_FAILURE);
	}

	/*
	 * Use the log handler from the host controller driver
	 */
	hcdi->hcdi_log_handle = args->usba_hcdi_register_log_handle;

	hcdi->hcdi_dip = args->usba_hcdi_register_dip;

	USB_DPRINTF_L4(DPRINT_MASK_HCDI, hcdi->hcdi_log_handle,
	    "usba_hcdi_register: %s", ddi_node_name(hcdi->hcdi_dip));

	/*
	 * Initialize the mutex.  Use the iblock cookie passed in
	 * by the host controller driver.
	 */
	mutex_init(&hcdi->hcdi_mutex, NULL, MUTEX_DRIVER,
			args->usba_hcdi_register_iblock_cookiep);

	/* add soft interrupt */
	if (ddi_add_softintr(hcdi->hcdi_dip,
				DDI_SOFTINT_MED, &hcdi->hcdi_soft_int_id,
				NULL, NULL, usba_hcdi_soft_intr,
				(caddr_t)hcdi) != DDI_SUCCESS) {

		mutex_destroy(&hcdi->hcdi_mutex);

		kmem_free(hcdi, sizeof (usb_hcdi_t));

		return (DDI_FAILURE);
	}

	hcdi->hcdi_dma_attr = args->usba_hcdi_register_dma_attr;
	hcdi->hcdi_flags = flags;
	hcdi->hcdi_ops = args->usba_hcdi_register_ops;
	hcdi->hcdi_soft_int_state = HCDI_SOFT_INT_NOT_PENDING;
	hcdi->hcdi_total_hotplug_success = 0;
	hcdi->hcdi_hotplug_success = 0;
	hcdi->hcdi_total_hotplug_failure = 0;
	hcdi->hcdi_hotplug_failure = 0;
	hcdi->hcdi_device_count = 0;
	usba_hcdi_create_stats(hcdi, ddi_get_instance(hcdi->hcdi_dip));

	sema_init(&hcdi->hcdi_init_ep_sema, 1, NULL, SEMA_DRIVER, NULL);

	hcdi->hcdi_min_xfer =
		hcdi->hcdi_dma_attr->dma_attr_minxfer;
	hcdi->hcdi_min_burst_size =
		(1<<(ddi_ffs(hcdi->hcdi_dma_attr->dma_attr_burstsizes)-1));
	hcdi->hcdi_max_burst_size =
		(1<<(ddi_fls(hcdi->hcdi_dma_attr->dma_attr_burstsizes)-1));

	usba_hcdi_set_hcdi(hcdi->hcdi_dip, hcdi);

#ifdef	DEBUG
	mutex_enter(&usba_hcdi_dump_mutex);
	hcdi->hcdi_dump_ops = usba_alloc_dump_ops();
	hcdi->hcdi_dump_ops->usb_dump_ops_version = USBA_DUMP_OPS_VERSION_0;
	hcdi->hcdi_dump_ops->usb_dump_func = usba_hcdi_dump;
	hcdi->hcdi_dump_ops->usb_dump_cb_arg = (usb_opaque_t)hcdi;
	hcdi->hcdi_dump_ops->usb_dump_order = USB_DUMPOPS_HCDI_ORDER;
	usba_dump_register(hcdi->hcdi_dump_ops);
	mutex_exit(&usba_hcdi_dump_mutex);
#endif	/* DEBUG */

	return (DDI_SUCCESS);
}


/*
 * Called by an	HCD to detach an instance of the driver
 */
/*ARGSUSED*/
int
usba_hcdi_deregister(dev_info_t *dip)
{
	usb_hcdi_t *hcdi = usba_hcdi_get_hcdi(dip);

	USB_DPRINTF_L4(DPRINT_MASK_HCDI, hcdi->hcdi_log_handle,
	    "usb_hcdi_deregister: %s", ddi_node_name(dip));

	if (hcdi == NULL) {

		return (DDI_SUCCESS);
	}

	/* Destroy the soft interrupt */
	ddi_remove_softintr(hcdi->hcdi_soft_int_id);

	usba_hcdi_set_hcdi(dip, NULL);

	mutex_destroy(&hcdi->hcdi_mutex);

	sema_destroy(&hcdi->hcdi_init_ep_sema);

	usba_hcdi_destroy_stats(hcdi);

#ifdef  DEBUG
	mutex_enter(&usba_hcdi_dump_mutex);
	usba_dump_deregister(hcdi->hcdi_dump_ops);
	usba_free_dump_ops(hcdi->hcdi_dump_ops);
	mutex_exit(&usba_hcdi_dump_mutex);
#endif  /* DEBUG */

	kmem_free(hcdi, sizeof (usb_hcdi_t));

	return (DDI_SUCCESS);
}


/*
 * alloc usb_hcdi_ops structure
 * called from the HCD attach routine
 */
usb_hcdi_ops_t *
usba_alloc_hcdi_ops()
{
	usb_hcdi_ops_t	*usb_hcdi_ops;

	usb_hcdi_ops = kmem_zalloc(sizeof (usb_hcdi_ops_t), KM_SLEEP);
	return (usb_hcdi_ops);
}


/*
 * dealloc usb_hcdi_ops structure
 */
void
usba_free_hcdi_ops(usb_hcdi_ops_t *hcdi_ops)
{
	kmem_free(hcdi_ops, sizeof (usb_hcdi_ops_t));
}

/*
 * Allocate the hotplug kstats structure
 */
void
usba_hcdi_create_stats(usb_hcdi_t *hcdi, int instance)
{
	char			kstatname[KSTAT_STRLEN];
	const char		*dname = ddi_driver_name(hcdi->hcdi_dip);
	hcdi_hotplug_stats_t	*hsp;
	hcdi_error_stats_t	*esp;

	if (HCDI_HOTPLUG_STATS(hcdi) == NULL) {
		(void) sprintf(kstatname, "%s%d,hotplug", dname, instance);
		HCDI_HOTPLUG_STATS(hcdi) = kstat_create("usba", instance,
		    kstatname, "usb_hotplug", KSTAT_TYPE_NAMED,
		    sizeof (hcdi_hotplug_stats_t) / sizeof (kstat_named_t),
		    KSTAT_FLAG_PERSISTENT);

		if (HCDI_HOTPLUG_STATS(hcdi) != NULL) {
			hsp = HCDI_HOTPLUG_STATS_DATA(hcdi);
			kstat_named_init(&hsp->hcdi_hotplug_total_success,
			    "Total Hotplug Successes", KSTAT_DATA_UINT64);
			kstat_named_init(&hsp->hcdi_hotplug_success,
			    "Hotplug Successes", KSTAT_DATA_UINT64);
			kstat_named_init(&hsp->hcdi_hotplug_total_failure,
			    "Hotplug Total Failures", KSTAT_DATA_UINT64);
			kstat_named_init(&hsp->hcdi_hotplug_failure,
			    "Hotplug Failures", KSTAT_DATA_UINT64);
			kstat_named_init(&hsp->hcdi_device_count,
			    "Device Count", KSTAT_DATA_UINT64);

			HCDI_HOTPLUG_STATS(hcdi)->ks_private = hcdi;
			HCDI_HOTPLUG_STATS(hcdi)->ks_update = nulldev;
			kstat_install(HCDI_HOTPLUG_STATS(hcdi));
		}
	}

	if (HCDI_ERROR_STATS(hcdi) == NULL) {
		(void) sprintf(kstatname, "%s%d,error", dname, instance);
		HCDI_ERROR_STATS(hcdi) = kstat_create("usba", instance,
		    kstatname, "usb_errors", KSTAT_TYPE_NAMED,
		    sizeof (hcdi_error_stats_t) / sizeof (kstat_named_t),
		    KSTAT_FLAG_PERSISTENT);

		if (HCDI_ERROR_STATS(hcdi) != NULL) {
			esp = HCDI_ERROR_STATS_DATA(hcdi);
			kstat_named_init(&esp->hcdi_usb_cc_crc,
			    "CRC Errors", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_bitstuffing,
			    "Bit Stuffing Violations", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_data_toggle_mm,
			    "Data Toggle PID Errors", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_stall,
			    "Endpoint Stalls", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_dev_not_resp,
			    "Device Not Responding", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_pid_checkfailure,
			    "PID Check Bit Errors", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_unexp_pid,
			    "Invalid PID Errors", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_data_overrun,
			    "Data Overruns", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_data_underrun,
			    "Data Underruns", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_buffer_overrun,
			    "Buffer Overruns", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_buffer_underrun,
			    "Buffer Underruns", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_timeout,
			    "Command Timed Out", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_cc_unspecified_err,
			    "Unspecified Error", KSTAT_DATA_UINT64);
			/*
			kstat_named_init(&esp->hcdi_usb_failure,
			    "USB Failure", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_no_resources,
			    "No Resources", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_no_bandwidth,
			    "No Bandwidth", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_pipe_reserved,
			    "Pipe Reserved", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_pipe_unshareable,
			    "Pipe Unshareable", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_not_supported,
			    "Function Not Supported", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_pipe_error,
			    "Pipe Error", KSTAT_DATA_UINT64);
			kstat_named_init(&esp->hcdi_usb_pipe_busy,
			    "Pipe Busy", KSTAT_DATA_UINT64);
			*/

			HCDI_ERROR_STATS(hcdi)->ks_private = hcdi;
			HCDI_ERROR_STATS(hcdi)->ks_update = nulldev;
			kstat_install(HCDI_ERROR_STATS(hcdi));
		}
	}
}

/*
 * Destroy the hotplug kstats structure
 */
static void
usba_hcdi_destroy_stats(usb_hcdi_t *hcdi)
{
	if (HCDI_HOTPLUG_STATS(hcdi)) {
		kstat_delete(HCDI_HOTPLUG_STATS(hcdi));
		HCDI_HOTPLUG_STATS(hcdi) = NULL;
	}

	if (HCDI_ERROR_STATS(hcdi)) {
		kstat_delete(HCDI_ERROR_STATS(hcdi));
		HCDI_ERROR_STATS(hcdi) = NULL;
	}
}

/*
 * HCD callback handling
 */
void
usba_hcdi_callback(usb_pipe_handle_impl_t *ph,
	uint_t		usb_flags,
	mblk_t		*data,
	uint_t		flag,
	uint_t		completion_reason,
	uint_t		rval)
{

	usb_hcdi_t *hcdi =
	    usba_hcdi_get_hcdi(ph->p_usb_device->usb_root_hub_dip);

	/* Update the hcdi error kstats */
	mutex_enter(&hcdi->hcdi_mutex);
	HCDI_DO_ERROR_STATS(hcdi, completion_reason);
	mutex_exit(&hcdi->hcdi_mutex);

	/*
	 * If completion reason is not equal to USBC_CC_NOERROR
	 * then set pipe state equal to USB_PIPE_STATE_ERROR.
	 */
	if (completion_reason != USB_CC_NOERROR) {
		mutex_enter(&ph->p_mutex);
		if (USB_PIPE_CLOSING(ph)) {
			ph->p_last_state = USB_PIPE_STATE_ERROR;
		} else {
			ph->p_state = USB_PIPE_STATE_ERROR;
		}
		mutex_exit(&ph->p_mutex);
	} else {
		ASSERT(rval == USB_SUCCESS);
	}

	/*
	 * If the transfer is synchronous, stuff the
	 * arguments in the callback handler and signal
	 * the waiting thread.  if the transfer is
	 * asynchronous, add the callback to the callback
	 * queue.
	 */
	if (usb_flags & USB_FLAGS_SLEEP) {
		mutex_enter(&ph->p_mutex);
		ASSERT(ph->p_sync_result.p_done == 0);
		ASSERT(ph->p_sync_result.p_data == NULL);

		/* stuff the results in the sync result structure */
		ph->p_sync_result.p_data = data;
		ph->p_sync_result.p_completion_reason = completion_reason;
		ph->p_sync_result.p_flag = flag;
		ph->p_sync_result.p_rval = rval;
		ph->p_sync_result.p_done = 1;
		cv_signal(&ph->p_sync_result.p_cv_sync);
		mutex_exit(&ph->p_mutex);

	} else {
		/* stuff the results in the async result structure */
		usb_cb_t *cb = kmem_zalloc(sizeof (usb_cb_t), KM_NOSLEEP);

		mutex_enter(&ph->p_mutex);

		/* Increase the number of pending callbacks */
		ph->p_n_pending_async_cbs++;

		mutex_exit(&ph->p_mutex);

		/*
		 * In the rare case that the cb equals NULL, call the
		 * client driver's exception callback immediately.  Note
		 * that the exception callback will be out of order, but
		 * the callback is done in order to prevent the client driver
		 * from hanging.
		 */
		if (cb == NULL) {
			usb_opaque_t cb_arg;
			int (*exc_cb)(usb_pipe_handle_t,
						usb_opaque_t,
						uint_t,
						mblk_t *,
						uint_t);

			USB_DPRINTF_L1(DPRINT_MASK_HCDI, hcdi->hcdi_log_handle,
				"usb_hcdi_callback: out of memory");

			mutex_enter(&ph->p_mutex);
			cb_arg = ph->p_policy->pp_callback_arg;
			exc_cb = ph->p_policy->pp_exception_callback;
			mutex_exit(&ph->p_mutex);

			exc_cb((usb_pipe_handle_t)ph,
				cb_arg,
				completion_reason,
				data,
				flag);

			/*
			 * Decrease the number of pending callbacks
			 * The number was increased above to prevent
			 * the pipe from being destroyed while the
			 * callback is taking place.
			 */
			mutex_enter(&ph->p_mutex);
			ph->p_n_pending_async_cbs--;
			mutex_exit(&ph->p_mutex);

			return;
		}


		mutex_enter(&hcdi->hcdi_mutex);

		cb->usb_cb_pipe_handle = ph;
		cb->usb_cb_data = data;
		cb->usb_cb_completion_reason = completion_reason;
		cb->usb_cb_flag = flag;

#ifdef DEBUG
		/* Record the time */
		gethrestime(&cb->usb_cb_time);

		/* Record the number of times function is called */
		hcdi->hcdi_usba_async_callbacks++;
#endif

		/* Add the callback to the list */
		if (hcdi->hcdi_cb_list_head == NULL)  {
			hcdi->hcdi_cb_list_head = cb;
			hcdi->hcdi_cb_list_tail = cb;
		} else {
			hcdi->hcdi_cb_list_tail->usb_cb_next = cb;
			hcdi->hcdi_cb_list_tail = cb;
		}

		/*
		 * If a soft interrupt isn't pending, then trigger one.
		 */
		if (hcdi->hcdi_soft_int_state == HCDI_SOFT_INT_NOT_PENDING) {
			hcdi->hcdi_soft_int_state = HCDI_SOFT_INT_PENDING;
			mutex_exit(&hcdi->hcdi_mutex);
			ddi_trigger_softintr(hcdi->hcdi_soft_int_id);
		} else {
#ifdef DEBUG
			/*
			 * Record number of times a soft intr is
			 * already pending.
			 */
			hcdi->hcdi_soft_intr_pending++;
#endif
			mutex_exit(&hcdi->hcdi_mutex);
		}
	}
}

/*
 * Soft interrupt handler to perform the callbacks
 */
static uint_t
usba_hcdi_soft_intr(caddr_t arg)
{
	usb_pipe_handle_impl_t	*ph;
	usb_hcdi_t		*hcdi = (usb_hcdi_t *)arg;
	mblk_t			*data;
	uint_t			flag;
	uint_t			completion_reason;
	int 	(*cb)(usb_pipe_handle_t, usb_opaque_t, mblk_t *);
	int 	(*exc_cb)(usb_pipe_handle_t, usb_opaque_t, uint_t, mblk_t *,
								uint_t);
	usb_opaque_t		cb_arg;
	usb_cb_t		*callback;
	usb_cb_t		*old_callback;
#ifdef DEBUG
	timespec_t		hrtime;
	timespec_t		start_callback;
	timespec_t		end_callback;
	hrtime_t		delta;
	int			no_callbacks = 0;
#endif

	USB_DPRINTF_L4(DPRINT_MASK_HCDI, hcdi->hcdi_log_handle,
	    "usba_hcdi_soft_intr: init");

	mutex_enter(&hcdi->hcdi_mutex);

	/* verify there is work to do */
	if ((hcdi->hcdi_cb_list_head == NULL) ||
		(hcdi->hcdi_soft_int_state == HCDI_SOFT_INT_NOT_PENDING)) {

		mutex_exit(&hcdi->hcdi_mutex);
		return (DDI_INTR_UNCLAIMED);

	}

	hcdi->hcdi_soft_int_state = HCDI_SOFT_INT_NOT_PENDING;

	/*
	 * The hcdi_mutex is released in this loop when the
	 * client driver callback is made.  This means that
	 * usba_hcdi_callback() may be called during this time, and
	 * more callbacks may be added to the queue.  If callbacks
	 * are continually added to the queue, the soft interrupt
	 * handler could run forever.
	 *
	 * In order to prevent the interrupt handler from running forever,
	 * only process the callbacks currently on the list.  If
	 * additional callbacks are made when the hcdi_mutex
	 * is dropped, then they will be processed during the next soft
	 * interrupt.
	 *
	 * Start off with a copy of the list, and start with a fresh
	 * callback list.
	 */
	callback = hcdi->hcdi_cb_list_head;

	hcdi->hcdi_cb_list_head = NULL;
	hcdi->hcdi_cb_list_tail = NULL;

	mutex_exit(&hcdi->hcdi_mutex);

	while (callback) {

		ph = callback->usb_cb_pipe_handle;

		mutex_enter(&ph->p_mutex);

		data = callback->usb_cb_data;

		completion_reason = callback->usb_cb_completion_reason;

		cb = ph->p_policy->pp_callback;
		exc_cb = ph->p_policy->pp_exception_callback;
		cb_arg = ph->p_policy->pp_callback_arg;
		flag = callback->usb_cb_flag;
#ifdef DEBUG
		/* Increase counter of the number of no. of callbacks */
		no_callbacks++;

		/*
		 * See if this request has waited longer than
		 * any other for the pipe handle
		 */
		gethrestime(&hrtime);

		/*
		 * Find the difference betweent the time the
		 * request was first put on the queue and the
		 * time the request comes off the queue
		 */
		if (USB_TIME_LT(callback->usb_cb_time, hrtime)) {
			USB_TIME_DELTA(hrtime,
					callback->usb_cb_time,
					delta);
		}

		if (delta > ph->p_max_time_waiting) {
			ph->p_max_time_waiting = delta;
		}

#endif
		mutex_exit(&ph->p_mutex);

#ifdef DEBUG
		/* Record the time of the start of the callback */
		gethrestime(&start_callback);
#endif

		if ((completion_reason == 0) && cb) {
			cb((usb_pipe_handle_t)ph,
			cb_arg, data);
		} else if (exc_cb) {
			exc_cb((usb_pipe_handle_t)ph,
			    cb_arg, completion_reason,
			    data, flag);
		}
#ifdef DEBUG

		/* Record the time the callback finished */
		gethrestime(&end_callback);

		if (USB_TIME_LT(start_callback, end_callback)) {
			USB_TIME_DELTA(end_callback,
					start_callback,
					delta);
		}

		/*
		 * If this callback took longer than any other, then
		 * record the time and address of the callback
		 */
		mutex_enter(&ph->p_mutex);
		if (delta > ph->p_max_callback_time) {
			ph->p_max_callback_time = delta;
			if (cb)
				ph->p_max_callback = (caddr_t)cb;
			else
				ph->p_max_callback = (caddr_t)exc_cb;
		}
		mutex_exit(&ph->p_mutex);

#endif
		/*
		 * Once the callback is finished, decrease
		 * the number of pending callbacks
		 */
		mutex_enter(&ph->p_mutex);
		ph->p_n_pending_async_cbs--;
		mutex_exit(&ph->p_mutex);

		old_callback = callback;
		callback = callback->usb_cb_next;

		kmem_free(old_callback, sizeof (usb_cb_t));
	}

#ifdef DEBUG
	mutex_enter(&hcdi->hcdi_mutex);

	/* Increase the number of times the soft int has exited */
	hcdi->hcdi_soft_intr_exit++;

	/*
	 * If more callbacks were handled than at any other time,
	 * then store the number of callbacks.
	 */
	if (no_callbacks > hcdi->hcdi_max_no_handled) {
		hcdi->hcdi_max_no_handled = no_callbacks;
	}

	mutex_exit(&hcdi->hcdi_mutex);
#endif

	return (DDI_INTR_CLAIMED);
}


#ifdef	DEBUG
/*
 * Utility used to dump all HCDI related information. This
 * function is exported to USB framework and gets registered
 * as the dump function.
 */
void
usba_hcdi_dump(uint_t flag, usb_opaque_t arg)
{
	usb_hcdi_t	*hcdi;
	uint_t		show;
	uint_t		hcdi_show_label = USB_ALLOW_LABEL;

	mutex_enter(&usba_hcdi_dump_mutex);
	show = hcdi_show_label;
	hcdi_show_label = USB_DISALLOW_LABEL;
	hcdi = (usb_hcdi_t *)arg;

	_NOTE(NO_COMPETING_THREADS_NOW);

	USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
	    "\n****** HCDI Information ******");

	if (flag & USB_DUMP_STATE) {
		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi_dip: 0x%p\t\thcdi_dma_attr: 0x%p",
			hcdi->hcdi_dip, hcdi->hcdi_dma_attr);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi_ops: 0x%p\t\thcdi_flags: 0x%x",
			hcdi->hcdi_ops, hcdi->hcdi_flags);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi_min_xfer: 0x%x\t\thcdi_usb_address_in_use: 0x%p",
			hcdi->hcdi_min_xfer, hcdi->hcdi_usb_address_in_use);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi_min_burst_size: 0x%x\thcdi_max_burst_size: 0x%x",
			hcdi->hcdi_min_burst_size, hcdi->hcdi_max_burst_size);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi_dump_ops: 0x%p", hcdi->hcdi_dump_ops);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi no. async callbacks: %d",
			hcdi->hcdi_usba_async_callbacks);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi no. soft exits: %d", hcdi->hcdi_soft_intr_exit);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi no. soft pending: %d",
			hcdi->hcdi_soft_intr_pending);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi max no. handled: %d",
			hcdi->hcdi_max_no_handled);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi hotplug stats:\n\tTotal successes: %d\t"
			"successes: %d", hcdi->hcdi_total_hotplug_success,
			hcdi->hcdi_hotplug_success);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"Total failures: %d\tfailures: %d",
			hcdi->hcdi_total_hotplug_failure,
			hcdi->hcdi_hotplug_failure);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"hcdi device count: %d\thotplug stats 0x%p",
			hcdi->hcdi_device_count, hcdi->hcdi_hotplug_stats);

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			"error stats 0x%p",
			hcdi->hcdi_error_stats);
	}
	_NOTE(COMPETING_THREADS_NOW);

	if (flag & USB_DUMP_STATE) {
		usba_hcdi_dump_cb_list(hcdi, flag);
	}


	hcdi_show_label = show;

	mutex_exit(&usba_hcdi_dump_mutex);
}

/*
 * usba_hcdi_dump_cb_list:
 * This function displays the callback list.
 */
/*ARGSUSED*/
static void
usba_hcdi_dump_cb_list(usb_hcdi_t *hcdi, uint_t flag)
{
	usb_cb_t	*cb;
	usb_pipe_handle_impl_t	*ph;

	_NOTE(NO_COMPETING_THREADS_NOW);
	cb = hcdi->hcdi_cb_list_head;

	USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
		"***** HCDI: call back list *****");

	while (cb != NULL) {
		ph = cb->usb_cb_pipe_handle;

		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
				"pipe handle: 0x%p", ph);
		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
				"\tp_async_result: data 0x%p",
				    cb->usb_cb_data);
		USB_DPRINTF_L3(DPRINT_MASK_HCDI_DUMPING, hcdi->hcdi_log_handle,
			    "\tp_async_result: completeion reason 0x%p",
			    cb->usb_cb_completion_reason);
		cb = cb->usb_cb_next;
	}
	_NOTE(COMPETING_THREADS_NOW);
}
#endif	/* DEBUG */
