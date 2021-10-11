/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)ohci_polled.c	1.13	99/10/07 SMI"

/*
 * This module contains the specific ohci code used in POLLED mode.
 * This code is in a separate file since it will never become part
 * of the ohci driver.
 */

#include <sys/note.h>
#include <sys/types.h>
#include <sys/pci.h>
#if defined(sparc)
#include <v9/sys/prom_isa.h>
#endif
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/usb/hcd/openhci/ohci.h>
#include <sys/usb/hcd/openhci/ohcid.h>
#include <sys/usb/hcd/openhci/ohci_polled.h>
#include <sys/sunddi.h>

/*
 * POLLED entry points
 *
 * These functions are entry points into the POLLED code.
 */
int		ohci_hcdi_polled_input_init(usb_pipe_handle_impl_t *,
				uchar_t **, usb_console_info_impl_t *);
int		ohci_hcdi_polled_input_fini(usb_console_info_impl_t *);
int		ohci_hcdi_polled_input_enter(usb_console_info_impl_t *);
int		ohci_hcdi_polled_input_exit(usb_console_info_impl_t *);
int		ohci_hcdi_polled_read(usb_console_info_impl_t *, uint_t *);


/*
 * Internal Function Prototypes
 */

/* Polled initialization routines */
static int	ohci_polled_init(usb_pipe_handle_impl_t *pipe_handle,
				openhci_state_t *ohcip,
			usb_console_info_impl_t *console_input_info);

/* Polled deinitialization routines */
static int	ohci_polled_fini(ohci_polled_t *ohci_polledp,
				openhci_state_t *ohcip);

/* Polled save state routines */
static void	ohci_polled_save_state(ohci_polled_t *ohci_polledp);
static void	ohci_polled_stop_processing(ohci_polled_t *ohci_polledp);

/* Polled restore state routines */
static void	ohci_polled_restore_state(ohci_polled_t *ohci_polledp);
static void	ohci_polled_start_processing(ohci_polled_t *ohci_polledp);

/* Polled read routines */
static int	ohci_polled_check_done_list(openhci_state_t *ohcip,
				ohci_polled_t *ohci_polledp);
static gtd	*ohci_polled_reverse_done_list(openhci_state_t *ohcip,
				gtd *head_done_list);
static void	ohci_polled_create_input_list(openhci_state_t *ohcip,
				ohci_polled_t *ohci_polledp,
				gtd *head_done_list);
static int	ohci_polled_process_input_list(openhci_state_t *ohcip,
				ohci_polled_t *ohci_polledp);
static int	ohci_polled_handle_normal_td(openhci_state_t *ohcip,
				gtd *td, ohci_polled_t *ohci_polledp);
static void	ohci_polled_insert_td(openhci_state_t *ohcip, gtd *td);
static void	ohci_polled_fill_in_td(openhci_state_t *ohcip,
				gtd *td,
				gtd *new_dummy,
				uint_t hcgtd_ctrl,
				uint32_t hcgtd_iommu_cbp,
				size_t hcgtd_length,
				ohci_trans_wrapper_t *tw);
static void	ohci_polled_insert_td_on_tw(openhci_state_t *ohcip,
				ohci_trans_wrapper_t *tw,
				gtd *td);
static void	ohci_polled_finish_interrupt(openhci_state_t *ohcip,
				uint_t intr);


/*
 * External Function Prototypes
 */

/*
 * These routines are only called from the init and fini functions.
 * They are allowed to acquire locks.
 */
extern openhci_state_t	*ohci_obtain_state(dev_info_t *dip);
extern hc_ed_t		*ohci_alloc_hc_ed(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t  *pipe_handle);
extern int		ohci_insert_intr_td(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t  *pipe_handle,
				uint_t flags,
				ohci_handler_function_t tw_handle_td,
				usb_opaque_t tw_handle_callback_value);
extern void		ohci_traverse_tds(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
extern void		ohci_deallocate_gtd(openhci_state_t *ohcip,
				gtd *old_gtd);
extern void		ohci_deallocate_tw(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw);
extern void		ohci_deallocate_ed(openhci_state_t *ohcip,
				hc_ed_t *old_ed);

/*
 * These routines translate from io space to cpu space.  They are
 * called from POLLED mode, and are not allowed to acquire locks.
 */
extern uint32_t   	EDpool_cpu_to_iommu(openhci_state_t *ohcip,
				hc_ed_t *addr);
extern gtd		*TDpool_iommu_to_cpu(openhci_state_t *ohcip,
				uintptr_t addr);
extern uint32_t		TDpool_cpu_to_iommu(openhci_state_t *ohcip,
				gtd *addr);


/*
 * POLLED entry points
 *
 * These functions are entry points into the POLLED code.
 */

/*
 * ohci_hcdi_polled_input_init:
 *
 * This is the initialization routine for handling the USB keyboard
 * in POLLED mode.  This routine is not called from POLLED mode, so
 * it is OK to acquire mutexes.
 */
int
ohci_hcdi_polled_input_init(usb_pipe_handle_impl_t	*pipe_handle,
			uchar_t			**polled_buf,
			usb_console_info_impl_t *console_input_info)
{
	ohci_polled_t		*ohci_polledp;
	openhci_state_t		*ohcip;
	int			ret;

	ohcip = ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	/*
	 * Grab the ohci_int_mutex so that things don't change on us
	 * if an interrupt comes in.
	 */
	mutex_enter(&ohcip->ohci_int_mutex);

	ret = ohci_polled_init(pipe_handle, ohcip, console_input_info);

	if (ret != USB_SUCCESS) {

		/* Allow interrupts to continue */
		mutex_exit(&ohcip->ohci_int_mutex);
		return (ret);
	}

	ohci_polledp = (ohci_polled_t *)console_input_info->uci_private;

	/*
	 * Mark the structure so that if we are using it, we don't free
	 * the structures if one of them is unplugged.
	 */
	ohci_polledp->ohci_polled_flags |= POLLED_INPUT_MODE;

	/*
	 * This is the buffer we will copy characters into. It will be
	 * copied into at this layer, so we need to keep track of it.
	 */
	ohci_polledp->ohci_polled_buf =
		(uchar_t *)kmem_zalloc(POLLED_RAW_BUF_SIZE, KM_SLEEP);

	*polled_buf = ohci_polledp->ohci_polled_buf;

	/* Allow interrupts to continue */
	mutex_exit(&ohcip->ohci_int_mutex);
	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_polled_input_fini:
 */
int
ohci_hcdi_polled_input_fini(usb_console_info_impl_t *info)
{
	ohci_polled_t		*ohci_polledp;
	openhci_state_t		*ohcip;
	int			ret;

	ohci_polledp = (ohci_polled_t *)info->uci_private;

	ohcip = ohci_polledp->ohci_polled_ohcip;

	mutex_enter(&ohcip->ohci_int_mutex);

	/*
	 * Reset the POLLED_INPUT_MODE flag so that we can tell if
	 * this structure is in use in the ohci_polled_fini routine.
	 */
	ohci_polledp->ohci_polled_flags &= ~POLLED_INPUT_MODE;

	/* Free the buffer that we copied data into */
	kmem_free(ohci_polledp->ohci_polled_buf, POLLED_RAW_BUF_SIZE);

	ret = ohci_polled_fini(ohci_polledp, ohcip);

	mutex_exit(&ohcip->ohci_int_mutex);

	return (ret);
}


/*
 * ohci_hcdi_polled_input_enter:
 *
 * This is where we enter into POLLED mode.  This routine sets up
 * everything so that calls to  ohci_hcdi_polled_read will return
 * characters.
 */
int
ohci_hcdi_polled_input_enter(usb_console_info_impl_t *info)
{
	ohci_polled_t		*ohci_polledp;

	ohci_polledp = (ohci_polled_t *)info->uci_private;

	ohci_polledp->ohci_polled_entry++;

	/*
	 * If the controller is already switched over, just return
	 */
	if (ohci_polledp->ohci_polled_entry > 1) {
		return (USB_SUCCESS);
	}

	ohci_polled_save_state(ohci_polledp);

	ohci_polledp->ohci_polled_flags |= POLLED_INPUT_MODE_INUSE;

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_polled_input_exit:
 *
 * This is where we exit POLLED mode. This routine restores
 * everything that is needed to continue operation.
 */
int
ohci_hcdi_polled_input_exit(usb_console_info_impl_t *info)
{
	ohci_polled_t		*ohci_polledp;

	ohci_polledp = (ohci_polled_t *)info->uci_private;

	ohci_polledp->ohci_polled_entry--;

	/*
	 * If there are still outstanding "enters", just return
	 */
	if (ohci_polledp->ohci_polled_entry > 0)
		return (USB_SUCCESS);

	ohci_polledp->ohci_polled_flags &= ~POLLED_INPUT_MODE_INUSE;

	ohci_polled_restore_state(ohci_polledp);

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_polled_read:
 *
 * Get a key character
 */
int
ohci_hcdi_polled_read(usb_console_info_impl_t *info, uint_t *num_characters)
{
	openhci_state_t		*ohcip;
	ohci_polled_t		*ohci_polledp;

	ohci_polledp = (ohci_polled_t *)info->uci_private;

	ohcip = ohci_polledp->ohci_polled_ohcip;

#ifndef lint
	_NOTE(NO_COMPETING_THREADS_NOW);
#endif

	/*
	 * Check to see if there are any TD's on the done head.
	 */
	if (ohci_polled_check_done_list(ohcip, ohci_polledp)
						!= USB_SUCCESS) {

		*num_characters = 0;
	} else {

		/*
		 * Process any TD's on the input done list
		 */
		*num_characters =
			ohci_polled_process_input_list(ohcip,
				ohci_polledp);

		/* Acknowledge the WDH interrupt */
		ohci_polled_finish_interrupt(ohcip, HCR_INTR_WDH);
	}

#ifndef lint
	_NOTE(COMPETING_THREADS_NOW);
#endif

	return (USB_SUCCESS);
}


/*
 * Internal Functions
 */

/*
 * Polled initialization routines
 */


/*
 * ohci_polled_init:
 *
 * Initialize generic information that is needed to provide USB/POLLED
 * support.
 */
static int
ohci_polled_init(usb_pipe_handle_impl_t	*pipe_handle,
		openhci_state_t		*ohcip,
		usb_console_info_impl_t	*console_info)
{
	ohci_polled_t		*ohci_polledp;
	ohci_pipe_private_t	*pp;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * We have already initialized this structure. If the structure
	 * has already been initialized, then we don't need to redo it.
	 */
	if (console_info->uci_private != NULL) {

		return (USB_SUCCESS);
	}

	/* Allocate and intitialize a state structure */
	ohci_polledp = (ohci_polled_t *)
			kmem_zalloc(sizeof (ohci_polled_t), KM_SLEEP);

	/* Return failure if previous allocation failed */
	if (ohci_polledp == NULL) {

		/* This should never happen */
		return (USB_FAILURE);
	}

	/*
	 * Store away the ohcip so that we can get to it when we are in
	 * POLLED mode. We don't want to have to call ohci_obtain_state
	 * every time we want to access this structure.
	 */
	ohci_polledp->ohci_polled_ohcip = ohcip;

	/*
	 * Save usb device and endpoint number information from the usb
	 * pipe handle.
	 */
	mutex_enter(&pipe_handle->p_mutex);

	ohci_polledp->ohci_polled_usb_dev = pipe_handle->p_usb_device;

	ohci_polledp->ohci_polled_ep_index =
	    usba_get_ep_index(pipe_handle->p_endpoint->bEndpointAddress);
	mutex_exit(&pipe_handle->p_mutex);

	/*
	 * Allocate memory to make duplicate of original usb pipe handle.
	 */
	ohci_polledp->ohci_polled_input_pipe_handle =
		kmem_zalloc(sizeof (usb_pipe_handle_impl_t), KM_SLEEP);

	if (ohci_polledp->ohci_polled_input_pipe_handle == NULL) {

		/* This should never happen */
		return (USB_FAILURE);
	}

	/*
	 * Copy the USB handle into the new pipe handle.
	 */
	bcopy((void *)pipe_handle,
		(void *)ohci_polledp->ohci_polled_input_pipe_handle,
			sizeof (usb_pipe_handle_impl_t));

	/*
	 * Create a new ohci pipe private structure
	 */
	pp = (ohci_pipe_private_t *)
		kmem_zalloc(sizeof (ohci_pipe_private_t), KM_SLEEP);

	if (pp == NULL) {

		/* This should never happen */
		return (USB_FAILURE);
	}

	/*
	 * There will be a mutex lock per pipe. This will serialize the
	 * pipe's transactions.
	 */
	mutex_init(&pp->pp_mutex, NULL, MUTEX_DRIVER,
			ohcip->ohci_iblk_cookie);

	/*
	 * Store the pointer in the pipe handle. This structure was also
	 * just allocated.
	 */
	mutex_enter(&ohci_polledp->ohci_polled_input_pipe_handle->p_mutex);

	ohci_polledp->ohci_polled_input_pipe_handle->p_hcd_private =
							(usb_opaque_t)pp;

	mutex_exit(&ohci_polledp->ohci_polled_input_pipe_handle->p_mutex);

	/*
	 * Store a pointer to the pipe handle. This structure was  just
	 * allocated and it is not in use yet.  The locking is there to
	 * satisfy warlock.
	 */
	mutex_enter(&pp->pp_mutex);
	mutex_enter(&pipe_handle->p_mutex);

	bcopy(pipe_handle->p_policy, &pp->pp_policy,
					sizeof (usb_pipe_policy_t));

	mutex_exit(&pipe_handle->p_mutex);

	pp->pp_pipe_handle = ohci_polledp->ohci_polled_input_pipe_handle;

	/*
	 * Allocate a dummy for the interrupt table. This dummy will be
	 * put into the action when we  switch interrupt  tables during
	 * ohci_hcdi_polled_enter. Dummy is placed on the unused lattice
	 * entries. When the ED is allocated we will replace dummy ED by
	 * valid interrupt ED in one or more locations in the interrupt
	 * lattice depending on the requested polling interval. Also we
	 * will hang a dummy TD to the ED & dummy TD is used to indicate
	 * the end of the TD chain.
	 */
	ohci_polledp->ohci_polled_dummy_ed = ohci_alloc_hc_ed(ohcip,
			ohci_polledp->ohci_polled_input_pipe_handle);

	if (ohci_polledp->ohci_polled_dummy_ed == NULL) {

		mutex_exit(&pp->pp_mutex);
		return (USB_NO_RESOURCES);
	}

	/*
	 * Allocate the interrupt endpoint. This ED will be inserted in
	 * to the lattice chain for the  keyboard device. This endpoint
	 * will have the TDs hanging off of it for the processing.
	 */
	ohci_polledp->ohci_polled_ed = ohci_alloc_hc_ed(ohcip,
			ohci_polledp->ohci_polled_input_pipe_handle);

	if (ohci_polledp->ohci_polled_ed == NULL) {

		mutex_exit(&pp->pp_mutex);
		return (USB_NO_RESOURCES);
	}

	/* Insert the endpoint onto the pipe handle */
	pp->pp_ept = ohci_polledp->ohci_polled_ed;

	/*
	 * Insert a TD onto the endpoint.  There will now be two TDs on
	 * the ED, one is the dummy TD that was allocated above in  the
	 * ohci_alloc_hc_ed and this new one.
	 */
	if ((ohci_insert_intr_td(ohcip,
		ohci_polledp->ohci_polled_input_pipe_handle,
		0, NULL, NULL)) != USB_SUCCESS) {

		mutex_exit(&pp->pp_mutex);
		return (USB_NO_RESOURCES);
	}

	/*
	 * We are done with these locks (that we  didn't really have to
	 * be holding).
	 */
	mutex_exit(&pp->pp_mutex);

	console_info->uci_private = (usb_console_info_private_t)ohci_polledp;

	return (USB_SUCCESS);
}


/*
 * Polled deinitialization routines
 */


/*
 * ohci_polled_fini:
 */
static int
ohci_polled_fini(ohci_polled_t *ohci_polledp, openhci_state_t *ohcip)
{
	ohci_pipe_private_t	*pp;		/* Pipe private field */
	gtd *head_td, *next_td, *td;
	ohci_trans_wrapper_t	*head_tw, *next_tw, *tw;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * If the structure is already in use, then don't free it.
	 */
	if (ohci_polledp->ohci_polled_flags & POLLED_INPUT_MODE) {

		return (USB_SUCCESS);
	}

	pp = (ohci_pipe_private_t *)
		ohci_polledp->ohci_polled_input_pipe_handle->p_hcd_private;

	mutex_enter(&pp->pp_mutex);


	/*
	 * Traverse the list of TD's on this endpoint and these TD's
	 * have outstanding transfer requests. Since list processing
	 * is stopped, these TDs can be deallocated.
	 */
	ohci_traverse_tds(ohcip, pp->pp_pipe_handle);

	/*
	 * For each transfer wrapper on this pipe, free the TD and
	 * free the TW.  We don't free the last TD in the chain
	 * because it will be freed by ohci_deallocate_ed.  All TD's
	 * on this TW are also on the end point associated with this
	 * pipe.
	 */
	head_tw = pp->pp_tw_head;
	next_tw = head_tw;

	while (next_tw != NULL) {
		head_td = (gtd *)next_tw->tw_hctd_head;
		next_td = head_td;

		/*
		 * Walk through each TD for this transfer
		 * wrapper and free that TD.
		 */
		while (next_td != NULL) {
			td = next_td;
			next_td = TDpool_iommu_to_cpu(ohcip,
				Get_TD(td->hcgtd_next_td));
			ohci_deallocate_gtd(ohcip, td);
		}

		tw = next_tw;
		next_tw = tw->tw_next;

		mutex_exit(&pp->pp_mutex);

		/* Free the transfer wrapper */
		ohci_deallocate_tw(ohcip, pp, tw);

		mutex_enter(&pp->pp_mutex);
	}

	/*
	 * Deallocate the endpoint descriptors that we allocated
	 * with ohci_alloc_hc_ed.
	 */
	ohci_deallocate_ed(ohcip, ohci_polledp->ohci_polled_ed);

	ohci_deallocate_ed(ohcip, ohci_polledp->ohci_polled_dummy_ed);

	mutex_exit(&pp->pp_mutex);

	mutex_destroy(&pp->pp_mutex);

	/*
	 * Destroy everything about the pipe that we allocated in
	 * ohci_polled_duplicate_pipe_handle
	 */
	kmem_free(pp, sizeof (ohci_pipe_private_t));

	kmem_free(ohci_polledp->ohci_polled_input_pipe_handle,
		sizeof (usb_pipe_handle_impl_t));

	/*
	 * We use this field to determine if a TD is for input or not,
	 * so NULL the pointer so we don't check deallocated data.
	 */
	ohci_polledp->ohci_polled_input_pipe_handle = NULL;

	/*
	 * Finally, free off the structure that we use to keep track
	 * of all this.
	 */
	kmem_free(ohci_polledp, sizeof (ohci_polled_t));

	return (USB_SUCCESS);
}


/*
 * Polled save state routines
 */


/*
 * ohci_polled_save_state:
 */
static void
ohci_polled_save_state(ohci_polled_t	*ohci_polledp)
{
	openhci_state_t		*ohcip;
	int			i;
	uint_t			polled_toggle;
	uint_t			real_toggle;
	ohci_pipe_private_t	*pp;		/* Pipe private field */
	ohci_pipe_private_t	*polled_pp;	/* Pipe private field */
	usb_pipe_handle_impl_t	*ph;
	uint_t			ep_index;
	ohci_save_intr_status_t	*ohci_intr_sts;
	hcr_regs_t		*ohci_polled_regsp;
	gtd			*td, *prev_td;
	gtd			*done_head, **done_list;

#ifndef lint
	_NOTE(NO_COMPETING_THREADS_NOW);
#endif

	/*
	 * If either of these two flags are set, then we have already
	 * saved off the state information and setup the controller.
	 */
	if (ohci_polledp->ohci_polled_flags & POLLED_INPUT_MODE_INUSE) {
#ifndef lint
		_NOTE(COMPETING_THREADS_NOW);
#endif
		return;
	}

	ohcip = ohci_polledp->ohci_polled_ohcip;
	ohci_intr_sts = &ohcip->ohci_save_intr_status;
	ohci_polled_regsp = &ohci_polledp->ohci_polled_save_regs;

	/*
	 * Get the endpoint number.
	 */
	ep_index = ohci_polledp->ohci_polled_ep_index;

	/*
	 * Get the normal mode usb pipe handle.
	 */
	ph = (usb_pipe_handle_impl_t *)
		ohci_polledp->ohci_polled_usb_dev->
				usb_pipehandle_list[ep_index].next;

	/*
	 * Prevent the ohci interrupt handler from handling interrupt.
	 * We will turn off interrupts. This  keeps us from generating
	 * a hardware interrupt.This is the useful for testing because
	 * in POLLED  mode we can't get interrupts anyway. We can test
	 * this code by shutting off hardware interrupt generation and
	 * polling  for the interrupts.
	 */
	Set_OpReg(hcr_intr_disable, HCR_INTR_MIE);

	/*
	 * Save the current normal mode ohci registers  and later this
	 * saved register copy is used to replace some of required ohci
	 * registers before switching from polled mode to normal mode.
	 */
	bzero((void *)ohci_polled_regsp, sizeof (hcr_regs_t));

	bcopy((void *)ohcip->ohci_regsp,
		(void *)ohci_polled_regsp, sizeof (hcr_regs_t));

	/*
	 * The functionality &  importance of critical code section in
	 * the normal mode ohci interrupt handler and its usage in the
	 * polled mode is explained below.
	 *
	 * (a) Normal mode:
	 *
	 *	- Set the flag indicating that processing critical code
	 *	  in ohci interrupt handler.
	 *
	 *	- Process the missed ohci interrupts by copying missed
	 *	  interrupt events & done head list fields information
	 *	  to the critical interrupt events & done list fields.
	 *
	 *	- Reset the missed ohci interrupt events and done head
	 *	  list fields so that the new missed  interrupt events
	 *	  and done head list information can be saved.
	 *
	 *	- All above steps will be executed within the critical
	 *	  section of the  interrupt handler.  Then ohci missed
	 *	  interrupt handler will be called to service the ohci
	 *	  missed interrupts.
	 *
	 * (b) Polled mode:
	 *
	 *	- On entering the polled code, checks for the critical
	 *	  section code execution within normal  mode interrupt
	 *	  handler.
	 *
	 *	- If critical section code is  executing in the normal
	 *	  mode ohci interrupt handler & if copying of the ohci
	 *	  missed interrupt events and done head list fields to
	 *	  the critical fields is finished then, save the  "any
	 *	  missed interrupt events and done head list"  because
	 *	  of current polled mode switch into "critical  missed
	 *	  interrupt events & done list fields" instead  actual
	 *	  missed events and done list fields.
	 *
	 *	- Otherwise save "any missed interrupt events and done
	 *	  list" because of this  current polled mode switch in
	 *	  the actual missed  interrupt events & done head list
	 *	  fields.
	 */

	/*
	 * Check and save the pending SOF interrupt  condition for the
	 * ohci normal mode. This information will be  saved either in
	 * the critical missed event fields or in actual  missed event
	 * fields depending on the whether the critical code section's
	 * execution flag was set or not when switched to  polled mode
	 * from normal mode.
	 */
	if ((ohci_intr_sts->ohci_intr_flag & OHCI_INTR_CRITICAL) &&
			(ohci_intr_sts->ohci_critical_intr_sts != 0)) {

		ohci_intr_sts->ohci_critical_intr_sts |=
			((Get_OpReg(hcr_intr_status) &
				Get_OpReg(hcr_intr_enable)) &
							HCR_INTR_SOF);
	} else {
		ohci_intr_sts->ohci_missed_intr_sts |=
			((Get_OpReg(hcr_intr_status) &
				Get_OpReg(hcr_intr_enable)) &
							HCR_INTR_SOF);
	}

	ohci_polled_stop_processing(ohci_polledp);

	/*
	 * By this time all list processing has been stopped.Now check
	 * and save the information about the pending HCCA done  list,
	 * done head ohci register and WDH bit in the interrupt status
	 * register. This information will be saved either in critical
	 * missed event fields or in actual missed event fields depend
	 * on the whether the  critical code section's  execution flag
	 * was set or not when switched to polled mode from the normal
	 * mode.
	 */

	/*
	 * Read and Save the HCCA DoneHead value.
	 */
	done_head = (gtd *)(uintptr_t)
			(Get_HCCA(ohcip->ohci_hccap->HccaDoneHead)
						& HCCA_DONE_HEAD_MASK);

	if ((done_head != NULL) &&
			(done_head != ohci_intr_sts->ohci_curr_done_lst)) {

		if ((ohci_intr_sts->ohci_intr_flag & OHCI_INTR_CRITICAL) &&
			((ohci_intr_sts->ohci_critical_done_lst != NULL) ||
			(ohci_intr_sts->ohci_missed_done_lst == NULL))) {

			done_list = &ohci_intr_sts->ohci_critical_done_lst;
			ohci_intr_sts->ohci_critical_intr_sts |= HCR_INTR_WDH;
		} else {
			done_list = &ohci_intr_sts->ohci_missed_done_lst;
			ohci_intr_sts->ohci_missed_intr_sts |= HCR_INTR_WDH;
		}

		if (*done_list != NULL) {

			td = (gtd *) TDpool_iommu_to_cpu(ohcip,
						(uintptr_t)done_head);

			while (td != NULL) {
				prev_td = td;
				td = TDpool_iommu_to_cpu(ohcip,
						Get_TD(td->hcgtd_next));
			}

			Set_TD(prev_td->hcgtd_next, *done_list);

			*done_list = done_head;

		} else {
			*done_list = (gtd *)done_head;
		}
	}

	/*
	 * Save the latest hcr_done_head ohci register value,  so that
	 * this value can be replaced  when exit from the POLLED mode.
	 */
	ohci_polled_regsp->hcr_done_head = Get_OpReg(hcr_done_head);

	/*
	 * Reset the HCCA done head and ohci done head register.
	 */
	Set_HCCA(ohcip->ohci_hccap->HccaDoneHead, NULL);
	Set_OpReg(hcr_done_head, (uint32_t)0x0);

	/*
	 * Clear the  WriteDoneHead interrupt bit in the ohci interrupt
	 * status register.
	 */
	Set_OpReg(hcr_intr_status, HCR_INTR_WDH);

	/*
	 * Save the current interrupt lattice and  replace this lattice
	 * with an lattice used in POLLED mode. We will restore lattice
	 * back when we exit from the POLLED mode.
	 */
	for (i = 0; i < NUM_INTR_ED_LISTS; i++) {
		ohci_polledp->ohci_polled_save_IntTble[i] =
			(hc_ed_t *)Get_HCCA(ohcip->ohci_hccap->HccaIntTble[i]);
	}

	/*
	 * Get the normal mode ohci pipe private structure.
	 */
	pp = (ohci_pipe_private_t *)ph->p_hcd_private;

	/*
	 * Get the polled mode ohci pipe private structure.
	 */
	polled_pp = (ohci_pipe_private_t *)
		ohci_polledp->ohci_polled_input_pipe_handle->p_hcd_private;

	/*
	 * Before replacing the lattice, adjust the data togggle on the
	 * on the ohci's interrupt ed
	 */
	polled_toggle = Get_ED(polled_pp->pp_ept->hced_headp) & HC_EPT_Carry;

	real_toggle = Get_ED(pp->pp_ept->hced_headp) & HC_EPT_Carry;

	if (polled_toggle != real_toggle) {
		if (real_toggle == 0) {
			Set_ED(polled_pp->pp_ept->hced_headp,
				Get_ED(polled_pp->pp_ept->hced_headp) &
					~HC_EPT_Carry);
		} else {
			Set_ED(polled_pp->pp_ept->hced_headp,
				Get_ED(polled_pp->pp_ept->hced_headp) |
					HC_EPT_Carry);
		}
	}

	/*
	 * Check whether Halt bit is set in the ED and if so  clear the
	 * halt bit.
	 */
	if (polled_pp->pp_ept->hced_headp & HC_EPT_Halt) {

		/* Clear the halt bit */
		Set_ED(polled_pp->pp_ept->hced_headp,
			(Get_ED(polled_pp->pp_ept->hced_headp) &
							~HC_EPT_Halt));
	}

	/*
	 * Fill in the lattice with dummy EDs. These EDs are used so the
	 * controller can tell that it is at the end of the ED list.
	 */
	for (i = 0; i < NUM_INTR_ED_LISTS; i++) {
		Set_HCCA(ohcip->ohci_hccap->HccaIntTble[i],
			EDpool_cpu_to_iommu(ohcip,
				ohci_polledp->ohci_polled_dummy_ed));
	}

	/*
	 * Now, add the endpoint to the lattice that we will  hang  our
	 * TD's off of.  We need to poll this device at  every 8 ms and
	 * hence add this ED needs 4 entries in interrupt lattice.
	 */
	for (i = 0; i < NUM_INTR_ED_LISTS;
			i = i + MIN_LOW_SPEED_POLL_INTERVAL) {
		Set_HCCA(ohcip->ohci_hccap->HccaIntTble[i],
			EDpool_cpu_to_iommu(ohcip,
				ohci_polledp->ohci_polled_ed));
	}

	/*
	 * Clear the contents of current ohci periodic ED register that
	 * is physical address of current Isochronous or Interrupt ED.
	 */
	Set_OpReg(hcr_periodic_curr, (uint32_t)0x0);

	/*
	 * Make sure WriteDoneHead interrupt is enabled.
	 */
	Set_OpReg(hcr_intr_enable, HCR_INTR_WDH);

	/*
	 * Enable the periodic list. We will now start processing EDs &
	 * TDs again.
	 */
	Set_OpReg(hcr_control, (Get_OpReg(hcr_control) | HCR_CONTROL_PLE));

#ifndef lint
	_NOTE(COMPETING_THREADS_NOW);
#endif
}


/*
 * ohci_polled_stop_processing:
 */
static void
ohci_polled_stop_processing(ohci_polled_t	*ohci_polledp)
{
	openhci_state_t		*ohcip;
	uint_t			count;
	hcr_regs_t		*ohci_polled_regsp;

	ohcip = ohci_polledp->ohci_polled_ohcip;
	ohci_polled_regsp = &ohci_polledp->ohci_polled_save_regs;

	/*
	 * Turn off all list processing. This will take place starting
	 * at the next frame.
	 */
	Set_OpReg(hcr_control,
		(ohci_polled_regsp->hcr_control) &
			~(HCR_CONTROL_CLE|HCR_CONTROL_PLE|
					HCR_CONTROL_BLE|HCR_CONTROL_IE));

	/*
	 * Make sure that the  SOF interrupt bit is cleared in the ohci
	 * interrupt status register.
	 */
	Set_OpReg(hcr_intr_status, HCR_INTR_SOF);

	/* Enable SOF interrupt */
	Set_OpReg(hcr_intr_enable, HCR_INTR_SOF);

	/*
	 * According to  OHCI Specification,  we have to wait for eight
	 * start of frames to make sure that the Host Controller writes
	 * contents of done head register to done head filed of HCCA.
	 */
	for (count = 0; count <= DONE_QUEUE_INTR_COUNTER; count++) {
		while (!((Get_OpReg(hcr_intr_status)) &
					HCR_INTR_SOF)) {
			continue;
		}

		/* Acknowledge the SOF interrupt */
		ohci_polled_finish_interrupt(ohcip, HCR_INTR_SOF);
	}

	Set_OpReg(hcr_intr_disable, HCR_INTR_SOF);
}


/*
 * Polled restore state routines
 */


/*
 * ohci_polled_restore_state:
 */
static void
ohci_polled_restore_state(ohci_polled_t	*ohci_polledp)
{
	openhci_state_t		*ohcip;
	int			i;
	uint_t			polled_toggle;
	uint_t			real_toggle;
	ohci_pipe_private_t	*pp;		/* Pipe private field */
	ohci_pipe_private_t	*polled_pp;	/* Pipe private field */
	gtd			*td;
	gtd			*next_td;	/* TD pointers */
	uint_t			count;
	ohci_save_intr_status_t	*ohci_intr_sts;
	hcr_regs_t		*ohci_polled_regsp;
	uint32_t		mask;
	usb_pipe_handle_impl_t	*ph;
	uint_t			ep_index;

#ifndef lint
	_NOTE(NO_COMPETING_THREADS_NOW);
#endif

	/*
	 * If this flags is set, then we are still using this structure,
	 * so don't restore any controller state information yet.
	 */
	if (ohci_polledp->ohci_polled_flags & POLLED_INPUT_MODE_INUSE) {

#ifndef lint
		_NOTE(COMPETING_THREADS_NOW);
#endif

		return;
	}

	ohcip = ohci_polledp->ohci_polled_ohcip;
	ohci_intr_sts = &ohcip->ohci_save_intr_status;
	ohci_polled_regsp = &ohci_polledp->ohci_polled_save_regs;

	/*
	 * Get the endpoint number.
	 */
	ep_index = ohci_polledp->ohci_polled_ep_index;

	/*
	 * Get the normal mode usb pipe handle.
	 */
	ph = (usb_pipe_handle_impl_t *)
		ohci_polledp->ohci_polled_usb_dev->
				usb_pipehandle_list[ep_index].next;

	/*
	 * Turn off all list processing.  This will take place starting
	 * at the next frame.
	 */
	Set_OpReg(hcr_control, (Get_OpReg(hcr_control) & ~HCR_CONTROL_PLE));

	Set_OpReg(hcr_intr_enable, HCR_INTR_SOF);

	/*
	 * According to  OHCI Specification,  we have to wait for eight
	 * start of frames to make sure that the Host Controller writes
	 * contents of done head register to done head filed of HCCA.
	 */
	for (count = 0; count <= DONE_QUEUE_INTR_COUNTER; count++) {
		while (!((Get_OpReg(hcr_intr_status)) &
					HCR_INTR_SOF)) {
			continue;
		}

		/* Acknowledge the SOF interrupt */
		ohci_polled_finish_interrupt(ohcip, HCR_INTR_SOF);
	}

	/*
	 * Before switching back, we have to process last TD in the POLLED
	 * mode. It may be in the hcr_done_head register or in  done  list
	 * or in the lattice. If it is either on the hcr_done_head register
	 * or in the done list, just re-inserted into the ED's TD list.
	 *
	 * First look up at the TD's that are in the hcr_done_head register
	 * and re-insert them back into the ED's TD list.
	 */
	td = TDpool_iommu_to_cpu(ohcip, (uintptr_t)Get_OpReg(hcr_done_head));

	while (td != NULL) {

		next_td = TDpool_iommu_to_cpu(ohcip,
			Get_TD(td->hcgtd_next));

		/*
		 * Insert valid interrupt TD back into ED's
		 * TD list. No periodic TD's will be processed
		 * since all processing has been stopped.
		 */
		ohci_polled_insert_td(ohcip, td);

		td = next_td;
	}

	/*
	 * Now look up at the TD's that are in the HCCA done head list &
	 * re-insert them back into the ED's TD list.
	 */
	td = TDpool_iommu_to_cpu(ohcip,
		(Get_HCCA(ohcip->ohci_hccap->HccaDoneHead)
					& HCCA_DONE_HEAD_MASK));

	while (td != NULL) {

		next_td = TDpool_iommu_to_cpu(ohcip,
			Get_TD(td->hcgtd_next));

		/*
		 * Insert valid interrupt TD back into ED's
		 * TD list. No periodic TD's will be processed
		 * since all processing has been stopped.
		 */
		ohci_polled_insert_td(ohcip, td);

		td = next_td;
	}

	/*
	 * Reset the HCCA done head list to NULL.
	 */
	Set_HCCA(ohcip->ohci_hccap->HccaDoneHead, NULL);

	/*
	 * Replace the hcr_done_head register field with the saved copy
	 * of current normal mode hcr_done_head register contents.
	 */
	Set_OpReg(hcr_done_head, (uint32_t)
			ohci_polled_regsp->hcr_done_head);

	/*
	 * Clear the WriteDoneHead and SOF interrupt bits in the ohci
	 * interrupt status register.
	 */
	Set_OpReg(hcr_intr_status, (HCR_INTR_WDH | HCR_INTR_SOF));

	/*
	 * Get the normal mode ohci pipe private structure.
	 */
	pp = (ohci_pipe_private_t *)ph->p_hcd_private;

	/*
	 * Get the polled mode ohci pipe private structure.
	 */
	polled_pp = (ohci_pipe_private_t *)
		ohci_polledp->ohci_polled_input_pipe_handle->p_hcd_private;

	/*
	 * Before replacing the lattice, adjust the data togggle
	 * on the on the ohci's interrupt ed
	 */
	polled_toggle = Get_ED(polled_pp->pp_ept->hced_headp) &
							HC_EPT_Carry;

	real_toggle = Get_ED(pp->pp_ept->hced_headp) & HC_EPT_Carry;

	if (polled_toggle != real_toggle) {
		if (polled_toggle == 0) {
			Set_ED(pp->pp_ept->hced_headp,
				Get_ED(pp->pp_ept->hced_headp) &
					~HC_EPT_Carry);
		} else {
			Set_ED(pp->pp_ept->hced_headp,
				Get_ED(pp->pp_ept->hced_headp) |
					HC_EPT_Carry);
		}
	}

	/*
	 * Replace the lattice
	 */
	for (i = 0; i < NUM_INTR_ED_LISTS; i++) {
		Set_HCCA(ohcip->ohci_hccap->HccaIntTble[i],
			ohci_polledp->ohci_polled_save_IntTble[i]);

	}

	/*
	 * Clear the contents of current ohci periodic ED register that
	 * is physical address of current Isochronous or Interrupt ED.
	 */
	Set_OpReg(hcr_periodic_curr, (uint32_t)0x0);

	ohci_polled_start_processing(ohci_polledp);

	/*
	 * Check and enable required ohci  interrupts before  switching
	 * back to normal mode from the POLLED mode.
	 */
	mask = (uint32_t)ohci_polled_regsp->hcr_intr_enable &
					(HCR_INTR_SOF | HCR_INTR_WDH);

	if (ohci_intr_sts->ohci_intr_flag & OHCI_INTR_HANDLING) {
		Set_OpReg(hcr_intr_enable, mask);
	} else {
		Set_OpReg(hcr_intr_enable, mask | HCR_INTR_MIE);
	}

#ifndef lint
	_NOTE(COMPETING_THREADS_NOW);
#endif
}


/*
 * ohci_polled_start_processing:
 */
static void
ohci_polled_start_processing(ohci_polled_t	*ohci_polledp)
{
	openhci_state_t		*ohcip;
	uint32_t		control;
	uint32_t		mask;
	hcr_regs_t		*ohci_polled_regsp;

	ohcip = ohci_polledp->ohci_polled_ohcip;
	ohci_polled_regsp = &ohci_polledp->ohci_polled_save_regs;

	mask = ((uint32_t)ohci_polled_regsp->hcr_control) &
		(HCR_CONTROL_CLE | HCR_CONTROL_PLE |
			HCR_CONTROL_BLE | HCR_CONTROL_IE);

	control = Get_OpReg(hcr_control) &
		~(HCR_CONTROL_CLE | HCR_CONTROL_PLE |
			HCR_CONTROL_BLE | HCR_CONTROL_IE);

	Set_OpReg(hcr_control, (control | mask));
}


/*
 * Polled read routines
 */


/*
 * ohci_polled_check_done_list:
 *
 * Check to see it there are any TD's on the done head.  If there are
 * then reverse the done list and put the TD's on the appropriated list.
 */
static int
ohci_polled_check_done_list(openhci_state_t *ohcip,
			ohci_polled_t	*ohci_polledp)
{
	gtd	*done_head, *done_list;

	/*
	 * Read and Save the HCCA DoneHead value.
	 */
	done_head = (gtd *)(uintptr_t)
		(Get_HCCA(ohcip->ohci_hccap->HccaDoneHead)
					& HCCA_DONE_HEAD_MASK);

	/*
	 * Look at the Done Head and if it is NULL, just return.
	 */
	if (done_head == NULL) {

		return (USB_FAILURE);
	}

	/*
	 * Reverse the done list, put the TD on the appropriate list.
	 */
	done_list = ohci_polled_reverse_done_list(ohcip, done_head);

	/*
	 * Create the input done list.
	 */
	ohci_polled_create_input_list(ohcip, ohci_polledp, done_list);

	/* Reset the done head to NULL */
	Set_HCCA(ohcip->ohci_hccap->HccaDoneHead, NULL);

	return (USB_SUCCESS);
}


/*
 * ohci_polled_reverse_done_list:
 *
 * Reverse the order of the Done Head List
 */
static gtd *
ohci_polled_reverse_done_list(openhci_state_t *ohcip,
				gtd *head_done_list)
{
	gtd *cpu_new_tail, *cpu_new_head, *cpu_save;

	ASSERT(head_done_list != NULL);

	/*
	 * At first, both the tail and head pointers point to the
	 * same element.
	 */
	cpu_new_tail = cpu_new_head =
		TDpool_iommu_to_cpu(ohcip,
				(uintptr_t)head_done_list);

	/* See if the list has only one element */
	if (Get_TD(cpu_new_head->hcgtd_next) == NULL) {

		return (cpu_new_head);
	}

	/* Advance the head pointer */
	cpu_new_head =	TDpool_iommu_to_cpu(ohcip,
				Get_TD(cpu_new_head->hcgtd_next));

	/* The new tail now points to nothing */
	Set_TD(cpu_new_tail->hcgtd_next, NULL);

	cpu_save = (gtd *)TDpool_iommu_to_cpu(ohcip,
				Get_TD(cpu_new_head->hcgtd_next));

	/* Reverse the list and store the pointers as CPU addresses */
	while (cpu_save != NULL) {
		Set_TD(cpu_new_head->hcgtd_next,
			TDpool_cpu_to_iommu(ohcip, cpu_new_tail));
		cpu_new_tail = cpu_new_head;
		cpu_new_head = cpu_save;

		cpu_save = (gtd *)TDpool_iommu_to_cpu(ohcip,
				Get_TD(cpu_new_head->hcgtd_next));
	}

	Set_TD(cpu_new_head->hcgtd_next,
			TDpool_cpu_to_iommu(ohcip, cpu_new_tail));

	return (cpu_new_head);
}


/*
 * ohci_polled_create_input_list:
 *
 * Create the input done list from the actual done head list.
 */
static void
ohci_polled_create_input_list(openhci_state_t *ohcip,
				ohci_polled_t *ohci_polledp,
				gtd *head_done_list)
{
	gtd *cpu_save, *td;
	ohci_trans_wrapper_t	*tw;
	ohci_pipe_private_t	*pp;	/* Pipe private field */

	ASSERT(head_done_list != NULL);

	/* Get the done head list */
	td = (gtd *)head_done_list;

	/*
	 * Traverse the done list and create the input done list.
	 */
	while (td != NULL) {
		/*
		 * Obtain the transfer wrapper from the TD
		 */
		tw = (ohci_trans_wrapper_t *)OHCI_LOOKUP_ID((uint32_t)
			Get_TD(td->hcgtd_trans_wrapper));

		/*
		 * Get the pipe handle for this transfer wrapper.
		 */
		pp = tw->tw_pipe_private;

		/*
		 * Convert the iommu pointer to a cpu pointer. No point
		 * in doing this over and over, might as well do it once.
		 */
		cpu_save = TDpool_iommu_to_cpu(ohcip, Get_TD(td->hcgtd_next));

		/*
		 * Terminate this TD by setting its next pointer to NULL.
		 */
		Set_TD(td->hcgtd_next, NULL);

		/*
		 * Figure out which done list to put this TD on and put it
		 * there.  If the pipe handle of the TD matches the pipe
		 * handle we are using for the input device, then this must
		 * be an input TD.
		 */
		if (pp->pp_pipe_handle ==
			ohci_polledp->ohci_polled_input_pipe_handle) {

			/*
			 * This is an input TD, so put it on the input done
			 * list.
			 */
			if (ohci_polledp->ohci_polled_input_done_head == NULL) {

				/*
				 * There is nothing on the input done list,
				 * so put this TD on the head.
				 */
				ohci_polledp->ohci_polled_input_done_head = td;
			} else {
				Set_TD(ohci_polledp->
					ohci_polled_input_done_tail->hcgtd_next,
						TDpool_cpu_to_iommu(ohcip, td));
			}

			/*
			 * The tail points to the new TD.
			 */
			ohci_polledp->ohci_polled_input_done_tail = td;
		}
		td = cpu_save;
	}
}


/*
 * ohci_polled_process_input_list:
 *
 * This routine takes the TD's off of the input done head and processes
 * them.  It returns the number of characters that have been copied for
 * input.
 */
static int
ohci_polled_process_input_list(openhci_state_t *ohcip,
			ohci_polled_t *ohci_polledp)
{
	gtd		*td;		/* TD pointers */
	gtd		*next_td;	/* TD pointers */
	uint_t		ctrl;
	uint_t		num_characters;
	ohci_trans_wrapper_t	*tw;
	ohci_pipe_private_t	*pp;

	/*
	 * Get the first TD on the input done head.
	 */
	td = ohci_polledp->ohci_polled_input_done_head;

	ohci_polledp->ohci_polled_input_done_head = NULL;

	num_characters = 0;

	/*
	 * Traverse the list of transfer descriptors.  We can't destroy
	 * the hcgtd_next pointers of these TDs because we are using it
	 * to traverse the done list.  Therefore, we can not put these
	 * TD's back on the ED until we are done processing all of them.
	 */
	while (td != NULL) {

		/*
		 * Get the next TD from the input done list.
		 */
		next_td = (gtd *)TDpool_iommu_to_cpu(ohcip,
						Get_TD(td->hcgtd_next));

		/* Look at the status */
		ctrl = (uint_t)Get_TD(td->hcgtd_ctrl) &
			(uint32_t)HC_GTD_CC;

		/*
		 * Check to see if there is an error. If there is error
		 * clear the halt condition in the Endpoint  Descriptor
		 * (ED) associated with this Transfer  Descriptor (TD).
		 */
		if (ctrl != HC_GTD_CC_NO_E) {

			/*
			 * Obtain the transfer wrapper from the TD
			 */
			tw = (ohci_trans_wrapper_t *)
				OHCI_LOOKUP_ID((uint32_t)
					Get_TD(td->hcgtd_trans_wrapper));

			/*
			 * Get the pipe handle for this transfer wrapper.
			 */
			pp = tw->tw_pipe_private;

			/* Clear the halt bit */
			Set_ED(pp->pp_ept->hced_headp,
				(Get_ED(pp->pp_ept->hced_headp) &
							~HC_EPT_Halt));
		} else {
			num_characters +=
				ohci_polled_handle_normal_td(ohcip,
					td, ohci_polledp);
		}

		/*
		 * Insert this interrupt TD back onto the ED's TD list.
		 */
		ohci_polled_insert_td(ohcip, td);

		td = next_td;
	}

	return (num_characters);
}


/*
 * ohci_polled_handle_normal_td:
 */
static int
ohci_polled_handle_normal_td(openhci_state_t *ohcip,
			gtd		*td,
			ohci_polled_t	*ohci_polledp)
{
	uchar_t			*buf;
	ohci_trans_wrapper_t	*tw;
	int			length;

	/* Obtain the transfer wrapper from the TD */
	tw = (ohci_trans_wrapper_t *)OHCI_LOOKUP_ID((uint32_t)
		Get_TD(td->hcgtd_trans_wrapper));

	ASSERT(tw != NULL);

	buf = (uchar_t *)tw->tw_buf;

	length = tw->tw_length;

	/*
	 * If "CurrentBufferPointer" of Transfer Descriptor (TD) is
	 * not equal to zero, then we  received less data  from the
	 * device than requested by us. In that  case, get the actual
	 * received data size.
	 */
	if (Get_TD(td->hcgtd_cbp)) {

		length = length - (Get_TD(td->hcgtd_buf_end) -
					Get_TD(td->hcgtd_cbp) + 1);
	}

	/* Copy the data into the message */
	ddi_rep_get8(tw->tw_accesshandle,
		(uint8_t *)ohci_polledp->ohci_polled_buf,
		(uint8_t *)buf,
		length,
		DDI_DEV_AUTOINCR);

	return (length);
}


/*
 * ohci_polled_insert_td:
 *
 * Insert a Transfer Descriptor (TD) on an Endpoint Descriptor (ED).
 */
static void
ohci_polled_insert_td(openhci_state_t *ohcip, gtd *td)
{
	ohci_pipe_private_t	*pp;		/* Pipe private field */
	hc_ed_t			*ept;
	uint_t			td_control;
	ohci_trans_wrapper_t	*tw;
	gtd 			*cpu_current_dummy;

	/*
	 * Obtain the transfer wrapper from the TD
	 */
	tw = (ohci_trans_wrapper_t *)OHCI_LOOKUP_ID((uint32_t)
		Get_TD(td->hcgtd_trans_wrapper));

	/*
	 * Take this TD off the transfer wrapper's list since
	 * the pipe is FIFO, this must be the first TD on the
	 * list.
	 */
	ASSERT((gtd *)tw->tw_hctd_head == td);

	tw->tw_hctd_head = TDpool_iommu_to_cpu(ohcip,
				Get_TD(td->hcgtd_next_td));

	/*
	 * If the head becomes NULL, then there are no more
	 * active TD's for this transfer wrapper. Also	set
	 * the tail to NULL.
	 */
	if (tw->tw_hctd_head == NULL) {
		tw->tw_hctd_tail = NULL;
	}

	/* Convert current valid TD as new dummy TD */
	bzero((char *)td, sizeof (gtd));
	Set_TD(td->hcgtd_ctrl, HC_TD_DUMMY);

	pp = tw->tw_pipe_private;

	/* Obtain the endpoint */
	ept = pp->pp_ept;

	if (tw->tw_flags & USB_FLAGS_SHORT_XFER_OK) {
		td_control = HC_GTD_IN|HC_GTD_1I|HC_GTD_R;
	} else {
		td_control = HC_GTD_IN|HC_GTD_1I;
	}

	/* Get the current dummy */
	cpu_current_dummy = (gtd *)
		(TDpool_iommu_to_cpu(ohcip, Get_ED(ept->hced_tailp)));

	/*
	 * Fill in the current dummy td and
	 * add the new dummy to the end.
	 */
	ohci_polled_fill_in_td(ohcip,
			cpu_current_dummy,
			td,
			td_control,
			tw->tw_cookie.dmac_address,
			tw->tw_length,
			tw);

	/* Insert this td onto the tw */
	ohci_polled_insert_td_on_tw(ohcip, tw, cpu_current_dummy);

	/*
	 * Add the new dummy to the ED's list.	When this occurs,
	 * the Host Controller will see the newly filled in dummy
	 * TD.
	 */
	Set_ED(ept->hced_tailp, (TDpool_cpu_to_iommu(ohcip, td)));
}


/*
 * ohci_polled_fill_in_td:
 *
 * Fill in the fields of a Transfer Descriptor (TD).
 */
static void
ohci_polled_fill_in_td(openhci_state_t *ohcip,
			gtd *td,
			gtd *new_dummy,
			uint_t hcgtd_ctrl,
			uint32_t hcgtd_iommu_cbp,
			size_t hcgtd_length,
			ohci_trans_wrapper_t *tw)
{
	/* Assert that the td to be filled in is a dummy */
	ASSERT(Get_TD(td->hcgtd_ctrl) == HC_TD_DUMMY);

	/* Clear the TD */
	bzero((char *)td, sizeof (gtd));

	/* Update the dummy with control information */
	Set_TD(td->hcgtd_ctrl, hcgtd_ctrl);

	/* Update the beginning of the buffer */
	Set_TD(td->hcgtd_cbp, hcgtd_iommu_cbp);

	/* The current dummy now points to the new dummy */
	Set_TD(td->hcgtd_next, (TDpool_cpu_to_iommu(ohcip, new_dummy)));

	/* Fill in the end of the buffer */
	if (hcgtd_length == 0) {
		ASSERT(Get_TD(td->hcgtd_cbp) == 0);
		ASSERT(hcgtd_iommu_cbp == 0);
		Set_TD(td->hcgtd_buf_end, 0);
	} else {
		Set_TD(td->hcgtd_buf_end,
		hcgtd_iommu_cbp + hcgtd_length - 1);
	}

	/* Fill in the wrapper portion of the TD */
	Set_TD(td->hcgtd_trans_wrapper, (uint32_t)tw->tw_id);
	Set_TD(td->hcgtd_next_td, NULL);
}


/*
 * ohci_polled_insert_td_on_tw:
 *
 * The transfer wrapper keeps a list of all Transfer Descriptors (TD) that
 * are allocated for this transfer. Insert a TD  onto this list. The  list
 * of TD's does not include the dummy TD that is at the end of the list of
 * TD's for the endpoint.
 */
static void
ohci_polled_insert_td_on_tw(openhci_state_t *ohcip,
			ohci_trans_wrapper_t	*tw,
			gtd			*td)
{

	/*
	 * Set the next pointer to NULL because
	 * this is the last TD on list.
	 */
	Set_TD(td->hcgtd_next_td, NULL);

	if (tw->tw_hctd_head == NULL) {
		ASSERT(tw->tw_hctd_tail == NULL);
		tw->tw_hctd_head = td;
		tw->tw_hctd_tail = td;
	} else {
		gtd *dummy = (gtd *)tw->tw_hctd_tail;

		ASSERT(dummy != NULL);
		ASSERT(dummy != td);
		ASSERT(Get_TD(td->hcgtd_ctrl) != HC_TD_DUMMY);

		/* Add the td to the end of the list */
		Set_TD(dummy->hcgtd_next_td,
					TDpool_cpu_to_iommu(ohcip, td));
		tw->tw_hctd_tail = td;

		ASSERT(Get_TD(td->hcgtd_next_td) == NULL);
	}
}


/*
 * ohci_polled_finish_interrupt:
 */
static void
ohci_polled_finish_interrupt(openhci_state_t	*ohcip,
				uint_t		intr)
{
	/* Acknowledge the interrupt */
	Set_OpReg(hcr_intr_status, intr);
}
