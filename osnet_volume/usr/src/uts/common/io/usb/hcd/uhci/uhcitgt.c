/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uhcitgt.c	1.17	99/11/29 SMI"

/*
 * Universal Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which interfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The interface to
 * the Host Controller is defined by the Universal Host Controller Interface.
 * This file contains the code for USBA entry points.
 */

#include	<sys/usb/hcd/uhci/uhcitgt.h>

/* Maximum bulk transfer size */
static uhci_bulk_transfer_size = 0x2000;

extern void uhci_remove_bulk_tds_tws(uhci_state_t *uhcip,
		usb_pipe_handle_impl_t *ph,
		queue_head_t *qh);

/*
 * uhci_hcdi_client_init:
 *
 * Member of the HCD ops structure and called at INITCHILD time.  Allocate
 * HCD resources for the device.
 */

int
uhci_hcdi_client_init(usb_device_t *usb_device)
{
	usb_dev_t	*usb_dev;
	uhci_state_t	*uhcip;

	uhcip = (uhci_state_t *)uhci_obtain_state(
			usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_hcdi_client_init: usb_device = 0x%p done",
		(void *)usb_device);


	usb_dev = (usb_dev_t *)kmem_zalloc(sizeof (usb_dev_t), KM_SLEEP);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_client_init:");

	mutex_init(&usb_dev->usb_dev_mutex, NULL,
		MUTEX_DRIVER, uhcip->uhci_iblk_cookie);

	mutex_enter(&usb_dev->usb_dev_mutex);
	usb_dev->usb_dev_device_impl = usb_device;
	usb_dev->usb_dev_pipe_list   = NULL;
	mutex_exit(&usb_dev->usb_dev_mutex);

	/*
	 * uhci_hcdi_client_init() is called right before the
	 * attach routine of the child driver is called.  The
	 * default pipe of the  device has been read and  the
	 * device has a  configuration as well as an address.
	 * The device fits with in the power budget.
	 */
	/* Save the HCD's device specific structure */
	mutex_enter(&usb_device->usb_mutex);
	usb_device->usb_hcd_private = usb_dev;
	mutex_exit(&usb_device->usb_mutex);

	return (USB_SUCCESS);
}

/*
 * uhci_hcdi_client_free:
 *
 * Member of the HCD ops structure and called at UNINITCHILD time. Deallocate
 * HCD resouces for the device.
 */

int
uhci_hcdi_client_free(usb_device_t *usb_device)
{

	usb_dev_t *usb_dev = (usb_dev_t *)usb_device->usb_hcd_private;

	if (usb_dev == NULL)
		return (USB_SUCCESS);

	mutex_enter(&usb_dev->usb_dev_mutex);

	/*
	 * USBA or Client driver will have to  call abort or close
	 * on each pipe of the pipe it has  opened before  calling
	 * this function for freeing client specific resources. uhci
	 * will check for this condition before releasing the client
	 * resourcs & it will return error if corresponding client's
	 * pipes are not closed.
	*/

	if (usb_dev->usb_dev_pipe_list != NULL) {
		mutex_exit(&usb_dev->usb_dev_mutex);
		return (USB_FAILURE);
	}

	mutex_exit(&usb_dev->usb_dev_mutex);

	mutex_enter(&usb_device->usb_mutex);
	usb_device->usb_hcd_private = NULL;
	mutex_exit(&usb_device->usb_mutex);

	mutex_destroy(&usb_dev->usb_dev_mutex);

	kmem_free(usb_dev, sizeof (usb_dev_t));

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_open:
 *
 * Member of HCD Ops structure and called during client specific pipe open
 * Add the pipe to the data structure representing the device and allocate
 * bandwidth for the pipe if it is a interrupt or isochronous endpoint.
 */

int
uhci_hcdi_pipe_open(usb_pipe_handle_impl_t *ph, uint_t flags)
{
	uhci_state_t		*uhcip;
	usb_dev_t		*usb_dev;
	usb_endpoint_descr_t	*endpoint_descr;
	uint_t			node = 0;
	uhci_pipe_private_t	*pp;
	int kmflag = (flags & USB_FLAGS_SLEEP) ?
				KM_SLEEP:KM_NOSLEEP;

	ASSERT(ph);

	uhcip = (uhci_state_t *)
			uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	endpoint_descr = ph->p_endpoint;
	usb_dev = (usb_dev_t *)ph->p_usb_device->usb_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_open: addr = 0x%x, ep%d",
		ph->p_usb_device->usb_addr,
		endpoint_descr->bEndpointAddress & USB_EPT_ADDR_MASK);

	sema_p(&uhcip->uhci_ocsem);

	/*
	 * Return failure immediately for any other pipe open on the root hub
	 * except control or interrupt pipe.
	 */

	if (ph->p_usb_device->usb_addr == ROOT_HUB_ADDR) {
		switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
			case USB_EPT_ATTR_CONTROL:
				break;
			case USB_EPT_ATTR_INTR:
				if ((endpoint_descr->bEndpointAddress &
					USB_EPT_ADDR_MASK) != 1) {
					USB_DPRINTF_L2(PRINT_MASK_HCDI,
						uhcip->uhci_log_hdl,
						"uhci_hcdi_pipe_open: Root"
						"hub interrupt pipe "
						"open failed");
					sema_v(&uhcip->uhci_ocsem);
					return (USB_FAILURE);
				}

				mutex_enter(&uhcip->uhci_int_mutex);

				/*
				 * Return failure if root hub interrupt pipe
				 * is already in use.
				 */
				if (uhcip->uhci_root_hub.root_hub_pipe_state ==
					PIPE_OPENED) {
					USB_DPRINTF_L2(PRINT_MASK_LISTS,
						uhcip->uhci_log_hdl,
						"uhci_hcdi_pipe_open: Root "
						"hub interrupt "
						"pipe open failed");
					mutex_exit(&uhcip->uhci_int_mutex);
					sema_v(&uhcip->uhci_ocsem);
					return (USB_FAILURE);
				}
				uhcip->uhci_root_hub.root_hub_pipe_handle = ph;

				/*
				 * Set the state of the root hub interrupt
				 * pipe as OPENED.
				 */
				uhcip->uhci_root_hub.root_hub_pipe_state =
						PIPE_OPENED;

				USB_DPRINTF_L4(PRINT_MASK_HCDI,
				    uhcip->uhci_log_hdl,
					"uhci_hcdi_pipe_open: "
					"Root hub interrupt "
					"pipe open succeeded");
				mutex_exit(&uhcip->uhci_int_mutex);
				sema_v(&uhcip->uhci_ocsem);
				return (USB_SUCCESS);
			default:
				USB_DPRINTF_L2(PRINT_MASK_HCDI,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_open: Root hub pipe open"
				"failed");
				sema_v(&uhcip->uhci_ocsem);
				return (USB_FAILURE);
		}
	}


	/*
	 * A portion of the bandwidth is reserved for the nonperdioc
	 * transfers  i.e control and bulk transfers in each  of one
	 * mill second frame period & usually it will be 10% of frame
	 * period. Hence there is no need to check for the available
	 * bandwidth before adding the control or bulk endpoints.
	 *
	 * There is need to check for the available bandwidth before
	 * adding the periodic transfers i.e interrupt & isochronous
	 * since all these periodic transfers are guaranteed transfers.
	 * Usually 90% of the total frame time is reserved for periodic
	 * transfers.
	*/


	if (((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		USB_EPT_ATTR_ISOCH) ||
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		USB_EPT_ATTR_INTR)) {

		mutex_enter(&uhcip->uhci_int_mutex);
		mutex_enter(&ph->p_mutex);

		if ((node = uhci_allocate_bandwidth(uhcip, ph)) ==
			USB_FAILURE) {
			USB_DPRINTF_L2(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
			"uhci_hcdi_pipe_open: Bandwidth allocation failure");
			mutex_exit(&uhcip->uhci_int_mutex);
			mutex_exit(&ph->p_mutex);
			sema_v(&uhcip->uhci_ocsem);
			return (USB_NO_BANDWIDTH);
		}

		mutex_exit(&ph->p_mutex);
		mutex_exit(&uhcip->uhci_int_mutex);
	}

	/* Create the HCD pipe private structure */
	pp = (uhci_pipe_private_t *)
		kmem_zalloc(sizeof (uhci_pipe_private_t), kmflag);

	/*
	 * There will be a mutex lock per pipe. This
	 * will serialize the pipe's transactions.
	 */
	mutex_init(&pp->pp_mutex, NULL, MUTEX_DRIVER, uhcip->uhci_iblk_cookie);

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Store the node in the interrupt lattice */
	pp->pp_node = node;

	/* Set the state of pipe as OPENED */
	pp->pp_state = PIPE_OPENED;

	/* Store a pointer to the pipe handle */
	pp->pp_pipe_handle = ph;

	mutex_enter(&ph->p_mutex);

	/* Store the pointer in the pipe handle */
	ph->p_hcd_private = (usb_opaque_t)pp;

	/* Store a copy of the pipe policy */
	bcopy(ph->p_policy, &pp->pp_policy, sizeof (usb_pipe_policy_t));

	mutex_exit(&ph->p_mutex);


	if (ph->p_usb_device->usb_addr != ROOT_HUB_ADDR) {
		/* Allocate the host controller endpoint descriptor */
		pp->pp_qh = uhci_alloc_queue_head(uhcip);

		if (pp->pp_qh == NULL) {
			USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_hcdi_pipe_open: QH allocation failed");

			/*
			 * Set the private structure in the
			 * pipe handle equal to NULL.
			 */
			mutex_enter(&ph->p_mutex);
			ph->p_hcd_private = NULL;
			mutex_exit(&ph->p_mutex);

			mutex_exit(&pp->pp_mutex);

			/* Destroy the pipe mutex */
			mutex_destroy(&pp->pp_mutex);

			/*
			 * Deallocate the hcd private portion
			 * of the pipe handle.
			 */
			kmem_free(ph->p_hcd_private,
				sizeof (uhci_pipe_private_t));

			mutex_exit(&uhcip->uhci_int_mutex);
			sema_v(&uhcip->uhci_ocsem);
			return (USB_NO_RESOURCES);
		}


		/*
		 * Insert the endpoint onto the host controller's
		 * appropriate endpoint list. The host controller
		 * will not schedule this endpoint will  not have
		 * any TD's to process.
		 */
		uhci_insert_qh(uhcip, ph);
	}
	/*
	 * Insert this pipe at the head of the list
	 * of pipes for this device.
	 */
	if (usb_dev) {
		mutex_enter(&usb_dev->usb_dev_mutex);
		pp->pp_next	= usb_dev->usb_dev_pipe_list;
		usb_dev->usb_dev_pipe_list	= pp;
		mutex_exit(&usb_dev->usb_dev_mutex);
	}

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_open: ph = 0x%p", (void *)ph);

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);
	sema_v(&uhcip->uhci_ocsem);
	return (USB_SUCCESS);
}

/*
 * uhci_hcdi_pipe_close:
 *
 * Member of HCD Ops structure and called during the client  specific pipe
 * close. Remove the pipe to the data structure representing the device and
 * deallocate  bandwidth for the pipe if it is a  interrupt or isochronous
 * endpoint.
 */

int
uhci_hcdi_pipe_close(usb_pipe_handle_impl_t  *ph)
{
	uhci_state_t		*uhcip;
	usb_dev_t		*usb_dev;
	uhci_pipe_private_t	*pp;
	usb_endpoint_descr_t	*endpoint_descr;
	uhci_pipe_private_t	*pipe;
	int			root_hub = UHCI_FALSE;
	gtd			*sof_td;

	uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);
	usb_dev = (usb_dev_t *)ph->p_usb_device->usb_hcd_private;
	pp = (uhci_pipe_private_t *)ph->p_hcd_private;
	endpoint_descr = ph->p_endpoint;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_close: addr = 0x%x, ep%d",
		ph->p_usb_device->usb_addr,
		endpoint_descr->bEndpointAddress & USB_EPT_ADDR_MASK);

	sema_p(&uhcip->uhci_ocsem);

	/*
	 * Check whether the pipe is a root hub
	 */
	if (ph->p_usb_device->usb_addr == ROOT_HUB_ADDR) {

		switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
			case USB_EPT_ATTR_CONTROL:
				mutex_enter(&uhcip->uhci_int_mutex);
				root_hub = UHCI_TRUE;
				mutex_exit(&uhcip->uhci_int_mutex);

				USB_DPRINTF_L4(PRINT_MASK_HCDI,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_close: Root hub control pipe"
				"close will be done below");
				break;

			case USB_EPT_ATTR_INTR:
				ASSERT((endpoint_descr->bEndpointAddress &
					USB_EPT_ADDR_MASK) == 1);

				mutex_enter(&uhcip->uhci_int_mutex);
				uhcip->uhci_root_hub.root_hub_pipe_handle =
					NULL;
				uhcip->uhci_root_hub.root_hub_pipe_state =
					NULL;

				USB_DPRINTF_L4(PRINT_MASK_HCDI,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_close: Root hub interrupt pipe "
				"close succeeded");

				mutex_exit(&uhcip->uhci_int_mutex);
				sema_v(&uhcip->uhci_ocsem);
				return (USB_SUCCESS);
		}
	}

	/* All transactions have been stopped */
	mutex_enter(&uhcip->uhci_int_mutex);

	/*
	 * Acquire the pipe mutex so that no other pipe
	 * requests are made on this pipe.
	 */

	mutex_enter(&pp->pp_mutex);

	/*
	 * Stop all the transactions if it is not the root hub.
	 */
	if (!root_hub) {

		/*
		 * Diasable all the transfers
		 * Wait for the next start of frame.
		 * As UHCi does not provide any intr facility for
		 * start of frame the driver puts a dummy TD in the
		 * lattice for the generaton of intr
		 */

		uhci_modify_td_active_bits(uhcip, pp->pp_qh);

		sof_td = (gtd *)
			TD_VADDR((uhcip->uhci_qh_pool_addr[0].element_ptr
			& QH_ELEMENT_PTR_MASK));

		sof_td->td_dword2.ioc = 1;

		uhcip->uhci_cv_signal = UHCI_TRUE;

		mutex_exit(&pp->pp_mutex);
		cv_wait(&uhcip->uhci_cv_SOF, &uhcip->uhci_int_mutex);
		mutex_enter(&pp->pp_mutex);

		sof_td->td_dword2.ioc = 0;

		UHCI_SET_TERMINATE_BIT(pp->pp_qh->element_ptr);

		if (pp->pp_qh->bulk_xfer_info)
			uhci_remove_bulk_tds_tws(uhcip, ph, pp->pp_qh);
		else
			uhci_remove_tds_tws(uhcip, ph);

		ASSERT(endpoint_descr != NULL);

		/*
		 * Remove the endoint descriptor from Host Controller's
		 * appropriate endpoint list.
		 */
		uhci_remove_qh(uhcip, pp);

		/* Deallocate bandwidth */
		if (((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
			USB_EPT_ATTR_ISOCH) ||
			((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
			USB_EPT_ATTR_INTR)) {

			mutex_enter(&ph->p_mutex);
			uhci_deallocate_bandwidth(uhcip, ph);
			mutex_exit(&ph->p_mutex);
		}
	}

	/*
	 * Remove this pipe from the list of pipes for the device
	 * if this isn't the default pipe that's used before the
	 * device is configured.
	 */
	if (usb_dev) {
		mutex_enter(&usb_dev->usb_dev_mutex);

		pipe = usb_dev->usb_dev_pipe_list;

		if (pipe == pp) {
			usb_dev->usb_dev_pipe_list = pipe->pp_next;
		} else {
		/* Search for the pipe */
			while (pipe->pp_next != pp) {
					pipe = pipe->pp_next;
			}

			/* Remove the pipe */
			pipe->pp_next = pp->pp_next;
		}
	}

	/*
	 * Destroy the pipe's mutex.
	 */
	mutex_exit(&pp->pp_mutex);
	mutex_destroy(&pp->pp_mutex);

	mutex_enter(&ph->p_mutex);

	/*
	 * Deallocate the hcd private portion
	 * of the pipe handle.
	 */

	kmem_free(ph->p_hcd_private, sizeof (uhci_pipe_private_t));
	ph->p_hcd_private = NULL;

	mutex_exit(&ph->p_mutex);

	if (usb_dev)
		mutex_exit(&usb_dev->usb_dev_mutex);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_close: ph = 0x%p", (void *)ph);

	mutex_exit(&uhcip->uhci_int_mutex);
	sema_v(&uhcip->uhci_ocsem);
	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_reset:
 */

int
uhci_hcdi_pipe_reset(usb_pipe_handle_impl_t *ph, uint_t usb_flags)
{
	uhci_state_t		*uhcip;
	uhci_pipe_private_t	*pp;
	usb_endpoint_descr_t	*endpoint_descr;
	gtd			*sof_td;

	uhcip 		= (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);
	pp		= (uhci_pipe_private_t *)ph->p_hcd_private;
	endpoint_descr	= ph->p_endpoint;

	/*
	 * Return failure immediately for any other pipe reset on the root
	 * hub except control or interrupt pipe.
	 */

	if (ph->p_usb_device->usb_addr == ROOT_HUB_ADDR) {
		switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
			case USB_EPT_ATTR_CONTROL:
				USB_DPRINTF_L4(PRINT_MASK_HCDI,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_reset: Pipe reset for root"
				"hub control pipe successful");

				break;
			case USB_EPT_ATTR_INTR:
				ASSERT((endpoint_descr->bEndpointAddress &
					USB_EPT_ADDR_MASK) == 1);

				mutex_enter(&uhcip->uhci_int_mutex);

				if (uhcip->uhci_root_hub.root_hub_pipe_state ==
					PIPE_POLLING)
					uhcip->uhci_root_hub.root_hub_pipe_state
					= PIPE_OPENED;

				ASSERT(uhcip->uhci_root_hub.root_hub_pipe_state
					== PIPE_OPENED);

				USB_DPRINTF_L4(PRINT_MASK_HCDI,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_reset: Pipe reset for root hub"
				"interrupt pipe successful");

				mutex_exit(&uhcip->uhci_int_mutex);
				break;
			default:
				USB_DPRINTF_L2(PRINT_MASK_HCDI,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_close: Root hub pipe reset"
				"failed");

				usba_hcdi_callback(ph, usb_flags, 0, 0,
					USB_CC_UNSPECIFIED_ERR, USB_FAILURE);

				return (USB_FAILURE);
		}

		usba_hcdi_callback(ph, usb_flags,
			0, 0, USB_CC_NOERROR, USB_SUCCESS);

		return (USB_SUCCESS);
	}

	mutex_enter(&uhcip->uhci_int_mutex);

	/*
	 * Acquire the pipe mutex so that no other pipe
	 * requests are made on this pipe.
	 * Set the active bit in the to INACTIVE and for all
	 * the remaining TD's for end point.
	 * Set the active bit for the dummy td. This will
	 * generate an interrupt at the end of the frame.
	 * After receiving the interrupt, it is safe to
	 * to manipulate the lattice.
	 */
	mutex_enter(&pp->pp_mutex);

	uhci_modify_td_active_bits(uhcip, pp->pp_qh);

	sof_td = (gtd *) TD_VADDR((uhcip->uhci_qh_pool_addr[0].element_ptr &
				QH_ELEMENT_PTR_MASK));

	sof_td->td_dword2.ioc = 1;

	uhcip->uhci_cv_signal = UHCI_TRUE;

	/*
	 * Release the mutex here and enter after the completion of CV_wait.
	 * Otherwise, the system hangs if reset is called and a TD for this
	 * pipe got completed.
	 */

	mutex_exit(&pp->pp_mutex);

	cv_wait(&uhcip->uhci_cv_SOF, &uhcip->uhci_int_mutex);

	mutex_enter(&pp->pp_mutex);

	sof_td->td_dword2.ioc = 0;

	UHCI_SET_TERMINATE_BIT(pp->pp_qh->element_ptr);

	/*
	 * Clear the pipe
	 */

	if (pp->pp_qh->bulk_xfer_info)
		uhci_remove_bulk_tds_tws(uhcip, ph, pp->pp_qh);
	else
		uhci_remove_tds_tws(uhcip, ph);

	/*
	 * Initialize the element pointer
	 */

	pp->pp_qh->element_ptr = TD_PADDR(pp->pp_qh->td_tailp);


	/*
	 * Since the endpoint is stripped of Transfer
	 * Descriptors (TD),  reset the state of the
	 * periodic pipe to OPENED.
	 */
	pp->pp_state = PIPE_OPENED;

	/*
	 * Reset the data toggle to zero. It is expected that the
	 * client driver resets the usb device data toggle to zero
	 * so that both the driver and device are in sync after a
	 * error condition.
	 */
	pp->pp_data_toggle = 0;

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	usba_hcdi_callback(ph, usb_flags, 0, 0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_abort:
 *
 * NOTE:
 *		This function is not implemented completely.
 */

int
uhci_hcdi_pipe_abort(usb_pipe_handle_impl_t *ph, uint_t usb_flags)
{
	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_abort:");

	usba_hcdi_callback(ph,
		usb_flags, 0, 0,
			USB_CC_UNSPECIFIED_ERR, USB_SUCCESS);

	return (USB_FAILURE);
}


/*
 * uhci_hcdi_pipe_get_policy:
 */
int
uhci_hcdi_pipe_get_policy(usb_pipe_handle_impl_t  *ph,
	usb_pipe_policy_t *policy)
{
	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	uhci_pipe_private_t *pp =
		(uhci_pipe_private_t *)ph->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_get_policy:");

	/*
	 * Make sure there are no transactions
	 * on this pipe.
	 */
	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Copy the pipe policy information to clients memory space */
	bcopy(&pp->pp_policy, policy, sizeof (usb_pipe_policy_t));

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_set_policy:
 */

int
uhci_hcdi_pipe_set_policy(
	usb_pipe_handle_impl_t	*ph,
	usb_pipe_policy_t	*policy,
	uint_t			usb_flags)
{
	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	uhci_pipe_private_t *pp =
		(uhci_pipe_private_t *)ph->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_set_policy: flags = 0x%x", usb_flags);

	/*
	 * Make sure no other part of the driver
	 * is trying to access this  part of the
	 * pipe handle.
	 */
	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Copy the new policy into the pipe handle */
	bcopy(policy, &pp->pp_policy, sizeof (usb_pipe_policy_t));

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_device_ctrl_receive:
 */

int
uhci_hcdi_pipe_device_ctrl_receive(usb_pipe_handle_impl_t  *ph,
	uchar_t			bmRequestType,
	uchar_t			bRequest,
	uint16_t		wValue,
	uint16_t		wIndex,
	uint16_t		wLength,
	uint_t			usb_flags)
{
	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
	"uhci_hcdi_pipe_device_ctrl_receive: 0x%x 0x%x 0x%x 0x%x 0x%x",
	bmRequestType, bRequest, wValue, wIndex, wLength);

	return (uhci_common_ctrl_routine(ph,
		bmRequestType,
		bRequest,
		wValue,
		wIndex,
		wLength,
		NULL, usb_flags));
}


/*
 * uhci_hcdi_pipe_device_ctrl_send:
 */
int
uhci_hcdi_pipe_device_ctrl_send(usb_pipe_handle_impl_t  *ph,
	uchar_t		bmRequestType,
	uchar_t		bRequest,
	uint16_t	wValue,
	uint16_t	wIndex,
	uint16_t	wLength,
	mblk_t		*data,
	uint_t		usb_flags)
{
	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
	"uhci_hcdi_pipe_device_ctrl_send: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%p",
		bmRequestType, bRequest, wValue, wIndex, wLength,
							(void *)data);

	return (uhci_common_ctrl_routine(ph,
		bmRequestType,
		bRequest,
		wValue,
		wIndex,
		wLength,
		data, usb_flags));
}


/*
 * uhci_hcdi_bulk_transfer_size:
 *
 * Return maximum bulk transfer size
 */

/* ARGSUSED */
int
uhci_hcdi_bulk_transfer_size(dev_info_t *dip, size_t  *size)
{
	USB_DPRINTF_L4(PRINT_MASK_HCDI, NULL,
	    "uhci_hcdi_bulk_transfer_size:");

	*size = uhci_bulk_transfer_size;

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_receive_bulk_data:
 */

int
uhci_hcdi_pipe_receive_bulk_data(usb_pipe_handle_impl_t    *ph,
	size_t length,
	uint_t usb_flags)
{
	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_receive_bulk_data:");

	return (uhci_common_bulk_routine(ph,
		length,
		NULL,
		usb_flags));

}


/*
 * uhci_hcdi_pipe_send_bulk_data:
 */
int
uhci_hcdi_pipe_send_bulk_data(usb_pipe_handle_impl_t  *ph,
	mblk_t *data,
	uint_t usb_flags)
{
	uint_t		length;

	uhci_state_t *uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_send_bulk_data:");

	length = data->b_wptr - data->b_rptr;

	if (length <= 0)
		return (USB_FAILURE);
	else
		return (uhci_common_bulk_routine(ph,
			length,
			data,
			usb_flags));

}


/*
 * uhci_hcdi_pipe_start_polling:
 */

int
uhci_hcdi_pipe_start_polling(usb_pipe_handle_impl_t  *ph, uint_t flags)
{
	uhci_state_t		*uhcip;
	uhci_pipe_private_t	*pp;
	usb_endpoint_descr_t 	*endpoint_descr = ph->p_endpoint;
	int			error = USB_SUCCESS;

	uhcip	= (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);
	pp	= (uhci_pipe_private_t *)ph->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_start_polling: ep%d",
		ph->p_endpoint->bEndpointAddress & USB_EPT_ADDR_MASK);

	/*
	 * Return success immediately if it is a polling request for
	 * root hub.
	 */
	if ((ph->p_usb_device->usb_addr == ROOT_HUB_ADDR) &&
		((endpoint_descr->bmAttributes &
		USB_EPT_ATTR_MASK) == USB_EPT_ATTR_INTR)) {

		ASSERT((endpoint_descr->bEndpointAddress &
				USB_EPT_ADDR_MASK) == 1);

		mutex_enter(&uhcip->uhci_int_mutex);

		if ((uhcip->uhci_root_hub.root_hub_pipe_state == PIPE_OPENED) ||
			(uhcip->uhci_root_hub.root_hub_pipe_state ==
			PIPE_STOPPED)) {

			uhcip->uhci_root_hub.root_hub_pipe_state = PIPE_POLLING;

			USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
			"uhci_hcdi_pipe_start_polling: Start polling for root"
			"hub successful");

			mutex_exit(&uhcip->uhci_int_mutex);
			return (error);
		} else {
			USB_DPRINTF_L2(PRINT_MASK_INTR, uhcip->uhci_log_hdl,
			"uhci_hcdi_pipe_start_polling: "
			"Polling for root hub is already in progress");

			mutex_exit(&uhcip->uhci_int_mutex);
			return (USB_FAILURE);
		}
	}

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	switch (pp->pp_state) {
		case PIPE_OPENED:
			/*
			 * This pipe is unitialized. Insert a TD on
			 * the interrupt ED.
			 */
			error = uhci_insert_intr_td(uhcip, ph,
					flags, uhci_handle_intr_td, NULL);
			if (error != USB_SUCCESS) {
				USB_DPRINTF_L2(PRINT_MASK_INTR,
					uhcip->uhci_log_hdl,
					"uhci_hcdi_pipe_start_polling: "
					"Start polling failed");

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&uhcip->uhci_int_mutex);

				return (error);
			}

			pp->pp_state = PIPE_POLLING;

			break;
	case PIPE_STOPPED:
			/*
			 * This pipe has already been initialized.
			 * Just reset the  skip bit in the Endpoint
			 * Descriptor (ED) to restart the polling.
			 */
			UHCI_CLEAR_TERMINATE_BIT(pp->pp_qh->element_ptr);

			pp->pp_state = PIPE_POLLING;

			break;
		case PIPE_POLLING:
			USB_DPRINTF_L2(PRINT_MASK_INTR,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_start_polling: "
				"Polling is already in progress");
			error = USB_SUCCESS;
			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_INTR,
				uhcip->uhci_log_hdl,
				"uhci_hcdi_pipe_start_polling: "
				"Undefined state");
			error = USB_FAILURE;
			break;
	}

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	return (error);
}


/*
 * uhci_hcdi_pipe_stop_polling:
 */

int
uhci_hcdi_pipe_stop_polling(usb_pipe_handle_impl_t  *ph, uint_t flags)
{
	uhci_state_t		*uhcip;
	uhci_pipe_private_t	*pp;
	usb_endpoint_descr_t	*endpoint_descr = ph->p_endpoint;

	uhcip = (uhci_state_t *)
		uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	pp = (uhci_pipe_private_t *)ph->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
		"uhci_hcdi_pipe_stop_polling: Flags = 0x%x", flags);

	/*
	 * Return success immediately if it is a stop polling request
	 * for root hub.
	 */
	if ((ph->p_usb_device->usb_addr == ROOT_HUB_ADDR) &&
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		USB_EPT_ATTR_INTR)) {

		ASSERT((endpoint_descr->bEndpointAddress &
			USB_EPT_ADDR_MASK) == 1);

		mutex_enter(&uhcip->uhci_int_mutex);

		if (uhcip->uhci_root_hub.root_hub_pipe_state == PIPE_POLLING) {

			uhcip->uhci_root_hub.root_hub_pipe_state = PIPE_STOPPED;

			USB_DPRINTF_L4(PRINT_MASK_HCDI, uhcip->uhci_log_hdl,
			"uhci_hcdi_pipe_stop_polling: Stop polling for root"
			"hub successful");

		} else {
			USB_DPRINTF_L2(PRINT_MASK_INTR,
			uhcip->uhci_log_hdl, "uhci_hcdi_pipe_stop_polling: "
			"Polling for root hub is already stopped");
		}
		mutex_exit(&uhcip->uhci_int_mutex);

		usba_hcdi_callback(ph, flags,
			0, 0, USB_CC_NOERROR, USB_SUCCESS);

		return (USB_SUCCESS);
	}

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	if (pp->pp_state != PIPE_POLLING) {

		USB_DPRINTF_L2(PRINT_MASK_INTR,
			uhcip->uhci_log_hdl,
			"uhci_hcdi_pipe_stop_polling: "
			"Polling is already stopped");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&uhcip->uhci_int_mutex);

		usba_hcdi_callback(ph, flags,
			0, 0, USB_CC_NOERROR, USB_SUCCESS);

		return (USB_SUCCESS);
	}

	/*
	 * Set the teminate bit in the host controller endpoint descriptor.
	 * Do not deallocate the bandwidth or tear down the DMA
	 */
	UHCI_SET_TERMINATE_BIT(pp->pp_qh->element_ptr);
	pp->pp_state = PIPE_STOPPED;

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	usba_hcdi_callback(ph,
		flags, 0, 0,
		USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_pipe_send_isoc_data:
 * NOTE: This function is not implemented.
 */

/* ARGSUSED */
int
uhci_hcdi_pipe_send_isoc_data(
	usb_pipe_handle_impl_t *ph,
	mblk_t *data,
	uint_t usb_flags)
{
	return (USB_FAILURE);
}
