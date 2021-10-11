/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uhcipolled.c	1.8	99/10/18 SMI"

/*
 * This module contains the specific uhci code used in POLLED mode.
 */

#include <sys/usb/hcd/uhci/uhcipolled.h>

/*
 * POLLED entry points
 *
 * These functions are entry points into the POLLED code.
 */

/*
 * uhci_hcdi_polled_input_init:
 *
 * This is the initialization routine for handling the USB keyboard
 * in POLLED mode.  This routine is not called from POLLED mode, so
 * it is OK to acquire mutexes.
 */
int
uhci_hcdi_polled_input_init(usb_pipe_handle_impl_t *ph,
	uchar_t			**polled_buf,
	usb_console_info_impl_t *console_input_info)
{
	uhci_polled_t		*uhci_polledp;
	uhci_state_t		*uhcip;
	int			ret;

	uhcip = uhci_obtain_state(ph->p_usb_device->usb_root_hub_dip);

	/*
	 * Grab the uhci_int_mutex so that things don't change on us
	 * if an interrupt comes in.
	 */
	mutex_enter(&uhcip->uhci_int_mutex);

	ret = uhci_polled_init(ph, uhcip, console_input_info);

	if (ret != USB_SUCCESS) {
		mutex_exit(&uhcip->uhci_int_mutex);
		return (ret);
	}

	uhci_polledp = (uhci_polled_t *)console_input_info->uci_private;

	/*
	 * Mark the structure so that if we are using it, we don't free
	 * the structures if one of them is unplugged.
	 */
	uhci_polledp->uhci_polled_flags |= POLLED_INPUT_MODE;

	/*
	 * This is the buffer we will copy characters into. It will be
	 * copied into at this layer, so we need to keep track of it.
	 */
	uhci_polledp->uhci_polled_buf =
		(uchar_t *)kmem_zalloc(POLLED_RAW_BUF_SIZE, KM_SLEEP);

	*polled_buf = uhci_polledp->uhci_polled_buf;

	mutex_exit(&uhcip->uhci_int_mutex);
	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_polled_input_fini:
 */
int
uhci_hcdi_polled_input_fini(usb_console_info_impl_t *info)
{
	uhci_polled_t		*uhci_polledp;
	uhci_state_t		*uhcip;
	int			ret;

	uhci_polledp = (uhci_polled_t *)info->uci_private;

	uhcip = uhci_polledp->uhci_polled_uhcip;

	mutex_enter(&uhcip->uhci_int_mutex);

	/* Free the buffer that we copied data into */
	kmem_free(uhci_polledp->uhci_polled_buf, POLLED_RAW_BUF_SIZE);

	ret = uhci_polled_fini(uhci_polledp, uhcip);

	info->uci_private = NULL;

	mutex_exit(&uhcip->uhci_int_mutex);

	return (ret);
}


/*
 * uhci_hcdi_polled_input_enter:
 *
 * This is where we enter into POLLED mode.  This routine sets up
 * everything so that calls to  uhci_hcdi_polled_read will return
 * characters.
 */
int
uhci_hcdi_polled_input_enter(usb_console_info_impl_t *info)
{
	uhci_polled_t		*uhci_polledp;

	uhci_polledp = (uhci_polled_t *)info->uci_private;

	uhci_polledp->uhci_polled_entry++;

	/*
	 * If the controller is already switched over, just return
	 */
	if (uhci_polledp->uhci_polled_entry > 1)
		return (USB_SUCCESS);

	uhci_polled_save_state(uhci_polledp);

	uhci_polledp->uhci_polled_flags |= POLLED_INPUT_MODE_INUSE;

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_polled_input_exit:
 *
 * This is where we exit POLLED mode. This routine restores
 * everything that is needed to continue operation.
 */
int
uhci_hcdi_polled_input_exit(usb_console_info_impl_t *info)
{
	uhci_polled_t		*uhci_polledp;

	uhci_polledp = (uhci_polled_t *)info->uci_private;

	uhci_polledp->uhci_polled_entry--;

	/*
	 * If there are still outstanding "enters", just return
	 */
	if (uhci_polledp->uhci_polled_entry > 0)
		return (USB_SUCCESS);

	uhci_polledp->uhci_polled_flags &= ~POLLED_INPUT_MODE_INUSE;

	uhci_polled_restore_state(uhci_polledp);

	return (USB_SUCCESS);
}


/*
 * uhci_hcdi_polled_read:
 *
 * Get a key character
 */
int
uhci_hcdi_polled_read(usb_console_info_impl_t *info, uint_t *num_characters)
{
	uhci_state_t		*uhcip;
	uhci_polled_t		*uhci_polledp;
	gtd			*td;
	uhci_trans_wrapper_t	*tw;
	ushort_t		intr_status;

	uhci_polledp = (uhci_polled_t *)info->uci_private;

	uhcip = uhci_polledp->uhci_polled_uhcip;

	/*
	 * This is a temperary work around for halt problem. The upper
	 * layer code does not call the right sequence of entry points
	 * points for reading a character in a polled mode. Once the
	 * upper layer code is fixed, the following code (two lines)
	 * must be removed.
	 */

	if (uhci_polledp->uhci_polled_entry == 0)
		if (uhci_hcdi_polled_input_enter(info) != DDI_SUCCESS)
			cmn_err(CE_WARN, "Entering Polled Mode failed");

	td = uhci_polledp->uhci_polled_td;

	/*
	 * Check to see if there are any TD's on the done head.
	 */
	if (td->td_dword2.status & TD_ACTIVE)
		*num_characters = 0;
	else {

		/*
		 * If the TD does not complete, retry.
		 */

		if ((td->td_dword2.status) ||
			(td->td_dword2.Actual_len == ZERO_LENGTH)) {
			*num_characters = 0;
			td->td_dword2.Actual_len = 0;
		} else {
			*num_characters = td->td_dword2.Actual_len + 1;

			tw = td->tw;

			/* Copy the data into the message */
			ddi_rep_get8(tw->tw_accesshandle,
				(uint8_t *)uhci_polledp->uhci_polled_buf,
				(uint8_t *)td->tw->tw_buf,
				td->td_dword2.Actual_len + 1,
				DDI_DEV_AUTOINCR);
		}

		/*
		 * Insert the td again in to the lattice.
		 */

		if (td->td_dword3.data_toggle == 0)
			td->td_dword3.data_toggle = 1;
		else
			td->td_dword3.data_toggle = 0;

		td->td_dword2.status = TD_ACTIVE;

		uhci_polledp->uhci_polled_qh->element_ptr =
			TD_PADDR(td);

		/* Clear the interrupt status register */
		intr_status = Get_OpReg16(USBSTS);
		Set_OpReg16(USBSTS, intr_status);
	}

	return (USB_SUCCESS);
}


/*
 * uhci_polled_init:
 *
 * Initialize generic information that is needed to provide USB/POLLED
 * support.
 */


static int
uhci_polled_init(usb_pipe_handle_impl_t	*ph,
	uhci_state_t		*uhcip,
	usb_console_info_impl_t	*console_info)
{
	uhci_polled_t		*uhci_polledp;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/*
	 * If the structure has already been initialized, then we don't
	 * need to redo it.
	 */

	if (console_info->uci_private != NULL)
		return (USB_SUCCESS);

	/* Allocate and intitialize a polled mode state structure */
	uhci_polledp = (uhci_polled_t *)
			kmem_zalloc(sizeof (uhci_polled_t), KM_SLEEP);

	/*
	 * Keep a copy of normal mode state structure and pipe handle.
	 */
	uhci_polledp->uhci_polled_uhcip	= uhcip;
	uhci_polledp->uhci_polled_ph	= ph;

	/*
	 * Allocate a queue head for the device. This queue head wiil be
	 * put in action when we switch to polled mode in _enter point.
	 */
	uhci_polledp->uhci_polled_qh = uhci_alloc_queue_head(uhcip);

	if (uhci_polledp->uhci_polled_qh == NULL) {
		kmem_free(uhci_polledp, sizeof (uhci_polled_t));
		return (USB_NO_RESOURCES);
	}

	/*
	 * Insert a TD onto the queue head.
	 */
	if ((uhci_polled_insert_td_on_qh(uhci_polledp,
		uhci_polledp->uhci_polled_ph)) !=
		USB_SUCCESS) {
		uhci_polledp->uhci_polled_qh->qh_flag = QUEUE_HEAD_FLAG_FREE;
		kmem_free(uhci_polledp, sizeof (uhci_polled_t));
		return (USB_NO_RESOURCES);
	}

	console_info->uci_private = (usb_console_info_private_t)uhci_polledp;

	return (USB_SUCCESS);
}


/*
 * uhci_polled_fini:
 */

static int
uhci_polled_fini(uhci_polled_t *uhci_polledp, uhci_state_t *uhcip)
{
	gtd	*td = uhci_polledp->uhci_polled_td;

	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/*
	 * Free the transfer wrapper
	 */
	uhci_free_tw(uhcip, td->tw);

	/*
	 * Free the queue head and transfer descriptor allocated.
	 */
	uhci_polledp->uhci_polled_qh->qh_flag = QUEUE_HEAD_FLAG_FREE;
	uhci_polledp->uhci_polled_td->flag = TD_FLAG_FREE;

	/*
	 * Deallocate the memory for the polled mode state structure.
	 */
	kmem_free(uhci_polledp, sizeof (uhci_polled_t));

	return (USB_SUCCESS);
}


/*
 * uhci_polled_save_state:
 */
static void
uhci_polled_save_state(uhci_polled_t	*uhci_polledp)
{
	uhci_state_t		*uhcip;
	int			i;
	usb_pipe_handle_impl_t	*ph;
	gtd			*td, *polled_td;


	/*
	 * If either of these two flags are set, then we have already
	 * saved off the state information and setup the controller.
	 */
	if (uhci_polledp->uhci_polled_flags & POLLED_INPUT_MODE_INUSE)
		return;

	uhcip = uhci_polledp->uhci_polled_uhcip;

	/*
	 * Get the normal mode usb pipe handle.
	 */
	ph = (usb_pipe_handle_impl_t *)uhci_polledp->uhci_polled_ph;

	/*
	 * Disable interrupts to prevent the interrupt handler getting
	 * called while we are switing to POLLed mode.
	 */
	Set_OpReg16(USBINTR, DISABLE_ALL_INTRS);

	/*
	 * Stop the HC controller from processing TD's
	 */

	Set_OpReg16(USBCMD, 0);

	/*
	 * Save the current interrupt lattice and  replace this lattice
	 * with an lattice used in POLLED mode. We will restore lattice
	 * back when we exit from the POLLED mode.
	 */
	for (i = 0; i < NUM_FRAME_LST_ENTRIES; i++) {
		uhci_polledp->uhci_polled_save_IntTble[i] =
			uhcip->uhci_frame_lst_tablep[i];
	}

	/*
	 * Zero out the entire interrupt lattice tree.
	 */
	for (i = 0; i < NUM_FRAME_LST_ENTRIES; i++)
		uhcip->uhci_frame_lst_tablep[i] = HC_END_OF_LIST;

	/*
	 * Now, add the endpoint to the lattice that we will  hang  our
	 * TD's off of.  We (assume always) need to poll this device at
	 * every 8 ms.
	 */
	for (i = 0; i < NUM_FRAME_LST_ENTRIES;
			i = i + MIN_LOW_SPEED_POLL_INTERVAL) {
		uhcip->uhci_frame_lst_tablep[i] =
		(QH_PADDR(uhci_polledp->uhci_polled_qh) | HC_QUEUE_HEAD);
	}

	/*
	 * Adjust the data toggle
	 */
	td = uhcip->uhci_oust_tds_head;
	while (td != NULL) {
		if (td->tw->tw_pipe_private->pp_pipe_handle == ph) {
			polled_td = uhci_polledp->uhci_polled_td;
			if (td->td_dword2.status & TD_ACTIVE) {
				polled_td->td_dword3.data_toggle =
					td->td_dword3.data_toggle;
			} else {
				polled_td->td_dword3.data_toggle =
					~td->td_dword3.data_toggle;
				uhcip->uhci_polled_flag =
					UHCI_POLLED_FLAG_TD_COMPL;
			}
			break;
		}
		td = td->oust_td_next;
	}

	/* Set the frame number to zero */
	Set_OpReg16(FRNUM, 0);

	/*
	 * Start the Host controller processing
	 */

	Set_OpReg16(USBCMD, (USBCMD_REG_HC_RUN | USBCMD_REG_MAXPKT_64 |
			USBCMD_REG_CONFIG_FLAG));

}


/*
 * uhci_polled_restore_state:
 */
static void
uhci_polled_restore_state(uhci_polled_t	*uhci_polledp)
{
	uhci_state_t		*uhcip;
	int			i;
	gtd			*td, *polled_td;
	uhci_pipe_private_t	*pp;
	ushort_t		real_data_toggle;

	/*
	 * If this flags is set, then we are still using this structure,
	 * so don't restore any controller state information yet.
	 */
	if (uhci_polledp->uhci_polled_flags & POLLED_INPUT_MODE_INUSE)
		return;

	uhcip = uhci_polledp->uhci_polled_uhcip;

	Set_OpReg16(USBCMD, 0x0);

	/*
	 * Before replacing the lattice, adjust the data togggle
	 * on the on the uhci's interrupt ed
	 */

	/*
	 * Replace the lattice
	 */
	for (i = 0; i < NUM_FRAME_LST_ENTRIES; i++) {
		uhcip->uhci_frame_lst_tablep[i] =
			uhci_polledp->uhci_polled_save_IntTble[i];

	}


	/*
	 * Adjust data toggle
	 */

	pp = (uhci_pipe_private_t *)
		uhci_polledp->uhci_polled_ph->p_hcd_private;

	polled_td = uhci_polledp->uhci_polled_td;

	if (polled_td->td_dword2.status & TD_ACTIVE)
		real_data_toggle = polled_td->td_dword3.data_toggle;
	else
		real_data_toggle = ~polled_td->td_dword3.data_toggle;

	td = uhcip->uhci_oust_tds_head;
	while (td != NULL) {
		if (td->tw->tw_pipe_private->pp_pipe_handle ==
			uhci_polledp->uhci_polled_ph) {
			if (td->td_dword2.status & TD_ACTIVE) {
				td->td_dword3.data_toggle =
					real_data_toggle;
				pp->pp_data_toggle =
					(real_data_toggle == 0) ?
					1 : 0;
			} else {
				pp->pp_data_toggle =
					real_data_toggle;
			}
		}
		td = td->oust_td_next;
	}

	/*
	 * Enable the interrupts.
	 * Start Host controller processing.
	 */
	Set_OpReg16(USBINTR, ENABLE_ALL_INTRS);

	Set_OpReg16(USBCMD, (USBCMD_REG_HC_RUN | USBCMD_REG_MAXPKT_64 |
			USBCMD_REG_CONFIG_FLAG));

	if (uhcip->uhci_polled_flag == UHCI_POLLED_FLAG_TD_COMPL)
		uhcip->uhci_polled_flag = UHCI_POLLED_FLAG_TRUE;
}


/*
 * uhci_polled_insert_td:
 * 	Initializes the transfer descriptor for polling and inserts on the
 *	polled queue head. This will be put in action when entered in to
 *	polled mode.
 */
static int
uhci_polled_insert_td_on_qh(uhci_polled_t *uhci_polledp,
	usb_pipe_handle_impl_t *ph)
{
	uhci_trans_wrapper_t	*tw;
	uhci_state_t		*uhcip = uhci_polledp->uhci_polled_uhcip;
	gtd			*td;

	/* Create the transfer wrapper */
	tw = uhci_polled_create_tw(uhci_polledp->uhci_polled_uhcip);

	if (tw == NULL)
		return (USB_FAILURE);

	/* Use the dummy TD allocated for the queue head */
	td = uhci_polledp->uhci_polled_qh->td_tailp;

	bzero((char *)td, sizeof (gtd));
	td->flag = TD_FLAG_BUSY;

	uhci_polledp->uhci_polled_td = td;
	td->tw = tw;

	td->link_ptr = HC_END_OF_LIST;

	mutex_enter(&ph->p_usb_device->usb_mutex);

	if (ph->p_usb_device->usb_port_status == USB_LOW_SPEED_DEV)
		td->td_dword2.ls = LOW_SPEED_DEVICE;

	td->td_dword2.c_err	= UHCI_MAX_ERR_COUNT;
	td->td_dword3.max_len	= 0x7;

	td->td_dword3.device_addr = ph->p_usb_device->usb_addr;

	td->td_dword3.endpt	=
		ph->p_endpoint->bEndpointAddress & END_POINT_ADDRESS_MASK;
	td->td_dword3.PID	= PID_IN;
	td->buffer_address	= tw->tw_cookie.dmac_address;
	td->td_dword2.ioc	= INTERRUPT_ON_COMPLETION;
	td->td_dword2.status	= 0x80;

	mutex_exit(&ph->p_usb_device->usb_mutex);

	uhci_polledp->uhci_polled_qh->element_ptr = TD_PADDR(td);

	return (USB_SUCCESS);
}


/*
 * uhci_polled_create_wrapper_t:
 * 	Creates the transfer wrapper used in polled mode.
 */

static uhci_trans_wrapper_t *
uhci_polled_create_tw(uhci_state_t *uhcip)
{
	uhci_trans_wrapper_t	*tw;
	uint_t			result, ccount;
	ddi_device_acc_attr_t	dev_attr;
	size_t			real_length;

	/* Allocate space for the transfer wrapper */
	tw = kmem_zalloc(sizeof (uhci_trans_wrapper_t), KM_NOSLEEP);

	if (tw == NULL)
		return (NULL);

	tw->tw_length = POLLED_RAW_BUF_SIZE;

	/* Allocate the DMA handle */
	result = ddi_dma_alloc_handle(uhcip->uhci_dip,
			&uhcip->uhci_dma_attr,
			DDI_DMA_DONTWAIT,
			0,
			&tw->tw_dmahandle);

	if (result != DDI_SUCCESS) {
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));
		return (NULL);
	}

	dev_attr.devacc_attr_version		= DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags	= DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder		= DDI_STRICTORDER_ACC;

	/* Allocate the memory */
	result = ddi_dma_mem_alloc(tw->tw_dmahandle,
			POLLED_RAW_BUF_SIZE,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT,
			NULL,
			(caddr_t *)&tw->tw_buf,
			&real_length,
			&tw->tw_accesshandle);

	if (result != DDI_SUCCESS) {
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));
		return (NULL);
	}

	/* Bind the handle */
	result = ddi_dma_addr_bind_handle(tw->tw_dmahandle,
			NULL,
			(caddr_t)tw->tw_buf,
			real_length,
			DDI_DMA_RDWR|DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT,
			NULL,
			&tw->tw_cookie,
			&ccount);

	/* Process the result */
	if (result != DDI_DMA_MAPPED) {
		ddi_dma_mem_free(&tw->tw_accesshandle);
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));
		return (NULL);
	}

	/* The cookie count should be 1 */
	if (ccount != 1) {
		result = ddi_dma_unbind_handle(tw->tw_dmahandle);
		ASSERT(result == DDI_SUCCESS);

		ddi_dma_mem_free(&tw->tw_accesshandle);
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (uhci_trans_wrapper_t));

		return (NULL);
	}

	return (tw);
}
