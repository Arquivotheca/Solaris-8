/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uhci.c	1.18	99/11/18 SMI"

/*
 * Universal Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which interfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The interface to
 * the Host Controller is defined by the Universal Host Controller Interface.
 * This file contains code for auto-configuration entry points.
 */


#include <sys/note.h>
#include <sys/types.h>
#include <sys/pci.h>
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/hcd/uhci/uhci.h>
#include <sys/usb/hcd/uhci/uhcid.h>
#include <sys/usb/hubd/hubdvar.h>


/*
 *  Global Variables
 */

void		*uhci_statep;
static uint_t	uhci_errlevel = USB_LOG_L1;
static uint_t	uhci_errmask = PRINT_MASK_ALL;
uint_t	uhci_show_label = USB_ALLOW_LABEL;
static uint_t	uhci_instance_debug = (uint_t)-1;

/*
 *   Proto Type Declarations
 */

static  int uhci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static  int uhci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static  int uhci_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
void    uhci_handle_ctrl_td(uhci_state_t *uhcip, gtd *td);
int		uhci_sendup_td_message(uhci_state_t *uhcip,
		uhci_pipe_private_t *pp,
		uhci_trans_wrapper_t *tw);
void    uhci_handle_intr_td(uhci_state_t *uhcip, gtd *td);
void    uhci_process_submitted_td_queue(uhci_state_t *uhcip);
uint_t  uhci_intr(caddr_t arg);
void    uhci_handle_intr_td_errors(uhci_state_t *uhcip, gtd *td);
uint_t uhci_parse_td_error(gtd *td, uint_t *NAK_received);

static int uhci_open(dev_t *devp, int flags, int otyp, cred_t *credp);
static int uhci_close(dev_t dev, int flag, int otyp, cred_t *credp);
static int uhci_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp);

#ifdef  DEBUG
/*
 * Dump support
 */
extern void usba_dump_register(usb_dump_ops_t *);
extern void uhci_dump(uint_t, usb_opaque_t);
extern void uhci_dump_state(uhci_state_t *, uint_t);
extern void uhci_dump_reqisters(uhci_state_t *);
extern void uhci_dump_pending_cmds(uhci_state_t *);
kmutex_t uhci_dump_mutex;
#endif
extern void uhci_handle_bulk_td(uhci_state_t *uhcip, gtd *td);

static struct cb_ops uhci_cb_ops = {
	uhci_open,			/* Open */
	uhci_close,			/* Close */
	nodev,				/* Strategy */
	nodev,				/* Print */
	nodev,				/* Dump */
	nodev,				/* Read */
	nodev,				/* Write */
	uhci_ioctl,			/* Ioctl */
	nodev,				/* Devmap */
	nodev,				/* Mmap */
	nodev,				/* Segmap */
	nochpoll,			/* Poll */
	ddi_prop_op,			/* cb_prop_op */
	NULL,				/* Streamtab */
	D_NEW | D_MP | D_HOTPLUG	/* Driver compatibility flag */
};

static struct dev_ops uhci_ops = {
	DEVO_REV,			/* Devo_rev */
	0,				/* Refcnt */
	uhci_info,			/* Info */
	nulldev,			/* Identify */
	nulldev,			/* Probe */
	uhci_attach,			/* Attach */
	uhci_detach,			/* Detach */
	nodev,				/* Reset */
	&uhci_cb_ops,			/* Driver operations */
	&usba_hubdi_busops,		/* Bus operations */
	NULL				/* Power */
};

/*
 * The USBA library must be loaded for this driver.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"USB UHCI Controller Driver 1.18",	/* Name of the module. */
	&uhci_ops,		/* Driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};


/*
 *  External Function Declarations
 */

extern void	uhci_set_dma_attributes(uhci_state_t *uhcip);
extern int	uhci_allocate_pools(dev_info_t *dip,
			uhci_state_t *uhcip);
extern void	uhci_decode_ddi_dma_addr_bind_handle_result(
			uhci_state_t *uhcip, int result);
extern int	uhci_map_regs(dev_info_t *dip, uhci_state_t *uhcip);
extern int	uhci_register_intrs_and_init_mutex(dev_info_t *dip,
			uhci_state_t *uhcip);
extern int	uhci_init_ctlr(dev_info_t *dip, uhci_state_t *uhcip);
extern int	uhci_init_hcca(dev_info_t *dip, uhci_state_t *uhcip);
extern int	uhci_take_control(uhci_state_t *uhcip);
extern void	uhci_initialize_default_pipe(uhci_state_t *uhcip);
extern		usb_hcdi_ops_t *uhci_alloc_hcdi_ops(
			uhci_state_t *uhcip);
extern void	uhci_init_root_hub(uhci_state_t *uhcip);
extern int	uhci_load_root_hub_driver(uhci_state_t *uhcip);

extern void	uhci_cleanup(uhci_state_t *uhcip, int flags);
extern void	uhci_unload_root_hub_driver(uhci_state_t *uhcip);
extern void	uhci_deinitialize_default_pipe(uhci_state_t *uhcip);
extern void	uhci_handle_root_hub_status_change(void *uhcip);

extern uhci_state_t 	*uhci_obtain_state(dev_info_t *dip);
extern void 	uhci_delete_td(uhci_state_t *uhcip, gtd *td);
extern void 	uhci_deallocate_tw(uhci_state_t *uhcip,
			uhci_pipe_private_t *pp,
			uhci_trans_wrapper_t *tw);
extern int 	uhci_insert_hc_td(uhci_state_t *uhcip,
			uint32_t	buffer_address,
			size_t		hcgtd_length,
			uhci_pipe_private_t  *pp,
			uhci_trans_wrapper_t *tw,
			uchar_t PID);
extern void	uhci_ctrl_bulk_timeout_hdlr(void *arg);
extern void	uhci_handle_bulk_td(uhci_state_t *uhcip, gtd *td);

extern void	uhci_create_stats(uhci_state_t *uhcip);
extern void	uhci_destroy_stats(uhci_state_t *uhcip);

int
_init(void)
{
	int error;

	/* Initialize the soft state structures */
	if ((error = ddi_soft_state_init(&uhci_statep,
		sizeof (uhci_state_t),
		UHCI_MAX_INSTS)) != 0) {
		return (error);
	}

	/* Install the loadable module */
	if ((error = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&uhci_statep);
	}

#ifdef  DEBUG
	mutex_init(&uhci_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif  /* DEBUG */

	return (error);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


int
_fini(void)
{
	int error;

	error = mod_remove(&modlinkage);

	if (error == 0) {
#ifdef  DEBUG
		mutex_destroy(&uhci_dump_mutex);
#endif  /* DEBUG */

		/* Release per module resources */
		ddi_soft_state_fini(&uhci_statep);
	}

	return (error);
}


/*
 * Host Controller Driver (HCD) Auto configuration entry points
 */

/*
 * Function Name  :  uhci_attach:
 * Description    :  Attach entry point - called by the Kernel.
 *                   Allocates of per controller data structure.
 *                   Initializes the controller.
 * Output         :  DDI_SUCCESS / DDI_FAILURE
 */

static int
uhci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance;
	int		flags = 0;
	uhci_state_t	*uhcip = NULL;
	usba_hcdi_register_args_t	hcdi_args;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, NULL, "uhci_attach:");

	switch (cmd) {
		case DDI_ATTACH:
			break;
		case DDI_RESUME:
		default:
			return (DDI_FAILURE);
	}

	/* Get the instance and create soft state */
	instance = ddi_get_instance(dip);

	/* Allocate the soft state structure for this instance of the driver */
	if (ddi_soft_state_zalloc(uhci_statep, instance) != 0)
		return (DDI_FAILURE);

	uhcip = ddi_get_soft_state(uhci_statep, instance);
	if (uhcip == NULL) {
		return (DDI_FAILURE);
	}

	uhcip->uhci_log_hdl = usb_alloc_log_handle(dip, "uhci",
			&uhci_errlevel,
			&uhci_errmask,
			&uhci_instance_debug,
			&uhci_show_label, 0);

	flags |= UHCI_SOFT_STATE_ZALLOC;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhcip = 0x%p", (void *)uhcip);

	/* Initialize the DMA attributes */
	uhci_set_dma_attributes(uhcip);

	/* Save the dip and instance */
	uhcip->uhci_dip		= dip;
	uhcip->uhci_instance	= instance;

	/* Initialize the kstat structures */
	uhci_create_stats(uhcip);

	/* Create the td and ed pools */
	if (uhci_allocate_pools(dip, uhcip) != DDI_SUCCESS) {
		uhci_cleanup(uhcip, flags);
		return (DDI_FAILURE);
	}

	/* Map the registers */
	if (uhci_map_regs(dip, uhcip) != DDI_SUCCESS) {
		uhci_cleanup(uhcip, flags);
		return (DDI_FAILURE);
	}

	/*
	 * Disable all the interrrupts. The ddi_add_intr() routine adds a
	 * interrupt handler before adding the original handler. So, if
	 * we have a pending interrupt, the dummy interrupt handler may be
	 * called infinely.
	 */

	Set_OpReg16(USBINTR, DISABLE_ALL_INTRS);


	/* Register interrupts */
	if (uhci_register_intrs_and_init_mutex(dip, uhcip) != DDI_SUCCESS) {
		uhci_cleanup(uhcip, flags);
		return (DDI_FAILURE);
	}

	flags |= (UHCI_INTR_HDLR_REGISTER | UHCI_LOCKS_INIT);


	/* Initialize the controller */
	if (uhci_init_ctlr(dip, uhcip) != DDI_SUCCESS) {
		uhci_cleanup(uhcip, flags);
		return (DDI_FAILURE);
	}

	/*
	 * At this point, the hardware will be okay.
	 * Initialize the usb_hcdi structure
	 */
	uhcip->uhci_hcdi_ops = uhci_alloc_hcdi_ops(uhcip);

	/*
	 * Make this HCD instance known to USBA
	 * (dma_attr must be passed for USBA busctl's)
	 */
	hcdi_args.usba_hcdi_register_version = HCDI_REGISTER_VERS_0;
	hcdi_args.usba_hcdi_register_dip = dip;
	hcdi_args.usba_hcdi_register_ops = uhcip->uhci_hcdi_ops;
	hcdi_args.usba_hcdi_register_dma_attr = &uhcip->uhci_dma_attr;
	hcdi_args.usba_hcdi_register_iblock_cookiep = uhcip->uhci_iblk_cookie;
	hcdi_args.usba_hcdi_register_log_handle = uhcip->uhci_log_hdl;

	if (usba_hcdi_register(&hcdi_args,
		0) != DDI_SUCCESS) {
		uhci_cleanup(uhcip, flags);
		return (DDI_FAILURE);
	}
	flags |= UHCI_REGS_MAPPING;

	/*
	 * On NCR system,  the driver seen  failure of some commands
	 * while booting. This dealy mysteriously solved the problem.
	 */
	drv_usecwait(UHCI_ONE_SECOND);

	mutex_enter(&uhcip->uhci_int_mutex);
	uhci_init_root_hub(uhcip);
	mutex_exit(&uhcip->uhci_int_mutex);

	/* Finally load the root hub driver */
	if (uhci_load_root_hub_driver(uhcip) != 0) {
		uhci_cleanup(uhcip, flags);
		return (DDI_FAILURE);
	}
	flags |= UHCI_ROOT_HUB_REGISTER;

	/* Display information in the banner */
	ddi_report_dev(dip);

	mutex_enter(&uhcip->uhci_int_mutex);

	/* Store the flags in uhcip structure */
	uhcip->uhci_flags = flags;

	uhcip->uhci_oust_tds_head = NULL;
	uhcip->uhci_oust_tds_tail = NULL;

	/*
	 * Create the time out handle. This gets called every second.
	 * Still better way of implentaion is to kick off when
	 * start_polling entry point * is called for the root hub and
	 * turn it off when stop polling is called. But the polling
	 * will definely be requred at all the times, it does not
	 * really matter if we do it here.
	 */
	uhcip->uhci_timeout_id = timeout(
		uhci_handle_root_hub_status_change,
		(void *)uhcip, drv_usectohz(UHCI_ONE_SECOND));

	/*
	 * Create a time out handler. This wil get called every one second
	 * to check whether any control/bulk command failed.
	 */
	uhcip->uhci_ctrl_bulk_timeout_id = timeout(
		uhci_ctrl_bulk_timeout_hdlr,
		(void *)uhcip, drv_usectohz(UHCI_ONE_SECOND));

	mutex_exit(&uhcip->uhci_int_mutex);

#ifdef  DEBUG
	mutex_enter(&uhci_dump_mutex);

	/* Initialize the dump support */
	uhcip->uhci_dump_ops = usba_alloc_dump_ops();
	uhcip->uhci_dump_ops->usb_dump_ops_version = USBA_DUMP_OPS_VERSION_0;
	uhcip->uhci_dump_ops->usb_dump_func = uhci_dump;
	uhcip->uhci_dump_ops->usb_dump_cb_arg = (usb_opaque_t)uhcip;
	uhcip->uhci_dump_ops->usb_dump_order = USB_DUMPOPS_OHCI_ORDER;
	usba_dump_register(uhcip->uhci_dump_ops);
	mutex_exit(&uhci_dump_mutex);
#endif  /* DEBUG */

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
		"uhci_attach: dip = 0x%p done", (void *)dip);

	return (DDI_SUCCESS);
}


/*
 * Function Name  :  uhci_detach:
 * Description    :  Detach entry point - called by the Kernel.
 *                   Deallocates all the memory
 *                   Unregisters the interrupt handle and other resources.
 * Output         :  DDI_SUCCESS / DDI_FAILURE
 */

static int
uhci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	uhci_state_t		*uhcip = (uhci_state_t *)uhci_obtain_state(dip);
	int			flags;
	int			ret_val = DDI_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, NULL, "uhci_detach:");

	switch (cmd) {
	    case DDI_DETACH:
			mutex_enter(&uhcip->uhci_int_mutex);
			flags = uhcip->uhci_flags;
			mutex_exit(&uhcip->uhci_int_mutex);

			uhci_cleanup(uhcip, flags);
			break;

	    case DDI_SUSPEND:
			USB_DPRINTF_L2(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
				"uhci_detach: Suspend not supported");

			ret_val = DDI_FAILURE;
			break;
	    default:
			ret_val = DDI_FAILURE;
			break;
	}

	return (ret_val);
}


/*
 * uhci_info:
 */
/* ARGSUSED */
static int
uhci_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{

	dev_t		dev;
	uhci_state_t	*uhcip;
	int		instance, error;

	switch (infocmd) {
		case DDI_INFO_DEVT2DEVINFO:
			dev = (dev_t)arg;
			instance = getminor(dev);

			if ((uhcip = ddi_get_soft_state(uhci_statep,
				instance)) == NULL)
				return (DDI_FAILURE);

			*result = (void *)uhcip->uhci_dip;
			error = DDI_SUCCESS;
			break;
		case DDI_INFO_DEVT2INSTANCE:
			dev = (dev_t)arg;
			instance = getminor(dev);
			*result = (void *)(uintptr_t)instance;
			error = DDI_SUCCESS;
			break;
	    default:
			error = DDI_FAILURE;
	}

	return (error);
}

/*
 * uhci_intr:
 *
 * uhci interrupt handling routine.
 */


uint_t
uhci_intr(caddr_t arg)
{
	uhci_state_t		*uhcip = (uhci_state_t *)arg;
	ushort_t		intr_status, cmd_reg;

	USB_DPRINTF_L4(PRINT_MASK_INTR, uhcip->uhci_log_hdl,
		"Interrupt occurred");

	mutex_enter(&uhcip->uhci_int_mutex);

	/*
	 * This is used to ensure that the controller is completely
	 * initialized before doing any processing. If our controller
	 * shares an interrupt, there is a possibility of getting
	 * interrupts before we finish init of controller. We may get
	 * some spurious intrs also.
	 */

	if (!(uhcip->uhci_flags & UHCI_ROOT_HUB_REGISTER)) {
		/* Update kstat values */
		UHCI_DO_INTRS_STATS(uhcip, 0);

		mutex_exit(&uhcip->uhci_int_mutex);
		return (DDI_INTR_UNCLAIMED);
	}


	/* Get the status of the interrupts */

	intr_status = Get_OpReg16(USBSTS);

	/* Update kstat values */
	UHCI_DO_INTRS_STATS(uhcip, intr_status);

	/* Acknowledge the interrupt */
	Set_OpReg16(USBSTS, intr_status);


	/*
	 * If the intr is not from our controller, just return a failure
	 */

	if (!(intr_status & UHCI_INTR_MASK)) {
		Set_OpReg16(USBINTR, ENABLE_ALL_INTRS);
		mutex_exit(&uhcip->uhci_int_mutex);
		return (DDI_INTR_UNCLAIMED);
	}

	/*
	 * Wake up all the threads which are waiting for the Start of Frame
	 */

	if (uhcip->uhci_cv_signal == UHCI_TRUE) {
		cv_broadcast(&uhcip->uhci_cv_SOF);
		uhcip->uhci_cv_signal = UHCI_FALSE;
	}


	/*
	 * Check whether any commands got completes. If so, proocess them.
	 */

	uhci_process_submitted_td_queue(uhcip);


	/*
	 * This should not occur. It occurs only if a HC controller
	 * experiances internal problem.
	 */

	if (intr_status & USBSTS_REG_HC_HALTED) {
		USB_DPRINTF_L2(PRINT_MASK_INTR, uhcip->uhci_log_hdl,
			"uhci_intr : Controller halted");
		cmd_reg = Get_OpReg16(USBCMD);
		Set_OpReg16(USBINTR, (cmd_reg | USBCMD_REG_HC_RUN));
	}

#ifdef DEBUG
	/*
	 * These are for just for information purpose. NO need to check
	 * under normal conditions.
	 */

	if (intr_status & USBSTS_REG_RESUME_DETECT)
		USB_DPRINTF_L2(PRINT_MASK_INTR, uhcip->uhci_log_hdl,
		"uhci_intr: Resume Detect");

	if (intr_status & USBSTS_REG_USB_ERR_INTR)
		USB_DPRINTF_L2(PRINT_MASK_INTR, uhcip->uhci_log_hdl,
			"uhci_intr: USB Error Interrupt");
#endif

	mutex_exit(&uhcip->uhci_int_mutex);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, uhcip->uhci_log_hdl,
			"Intr handling completed");

	return (DDI_INTR_CLAIMED);
}


/*
 * uhci_process_submitted_td_queue
 *    Traverse thru the submitted queue and process the completed ones.
 */
void
uhci_process_submitted_td_queue(uhci_state_t *uhcip)
{
	gtd			*head = uhcip->uhci_oust_tds_head;
	gtd			*head_next;
	uhci_trans_wrapper_t	*tw;
	uint_t			flag = UHCI_FALSE;

	while (head != NULL) {

		if (!(head->td_dword2.status & TD_ACTIVE)) {
			/*
			 * If error has occurred for a bulk td,
			 * all the other tds for that pipe are
			 * removed at once. So, start processing
			 * again from the HEAD of list so that we
			 * will not access the deallocated TD.
			 */

			if ((head->td_dword2.status) &&
				(head->tw->tw_handle_td == uhci_handle_bulk_td))
				flag = UHCI_TRUE;

			head_next = head->oust_td_next;

			tw = head->tw;
			/*
			 * Call the corrsponsing handle_td routine
			 */

			(*tw->tw_handle_td)(uhcip, head);

			if (flag)
				head = uhcip->uhci_oust_tds_head;
			else
				head = head_next;
		}
		else
			head = head->oust_td_next;
	}
}

/*
 * uhci_handle_intr_td
 *     hanldes the completed interrupt transfer TD's.
 */

void
uhci_handle_intr_td(uhci_state_t *uhcip, gtd *td)
{
	int			rval;
	uhci_trans_wrapper_t	*tw = td->tw;
	uhci_pipe_private_t	*pp = tw->tw_pipe_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
		"uhci_handle_intr_td:");

	mutex_enter(&pp->pp_mutex);
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	/* Sync the streaming buffer */
	rval = ddi_dma_sync(tw->tw_dmahandle,
		0,
		tw->tw_length,
		DDI_DMA_SYNC_FORCPU);

	ASSERT(rval == DDI_SUCCESS);

	if (td->td_dword2.status & TD_STATUS_MASK) {
		pp->pp_qh->element_ptr = td->link_ptr;
		uhci_handle_intr_td_errors(uhcip, td);
		mutex_exit(&pp->pp_mutex);
		return;
	}

	/*
	 * Get the actual received data size.
	 */

	tw->tw_length = td->td_dword2.Actual_len + 1;

	if (td->td_dword2.Actual_len == ZERO_LENGTH)
		tw->tw_bytes_xfered = 0;
	else
		tw->tw_bytes_xfered = td->td_dword2.Actual_len + 1;

	/*
	 * Call uhci_sendup_td_message to send message to upstream.
	 * The function uhci_sendup_td_message returns USB_NO_RESOURCES
	 * if allocb fails and also sends error message to upstream by
	 * calling USBA callback function. Under error conditions just
	 * drop the current message.
	 * Do not send the message up, if polling is stopped.
	 */

	if ((pp->pp_state == PIPE_POLLING) &&
		(tw->tw_bytes_xfered != 0)) {
		if ((uhci_sendup_td_message(uhcip, pp, tw)) ==
			USB_NO_RESOURCES) {
			USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_handle_intr_td: Drop the current message");
	    }
	}

	tw->tw_length = pp->pp_policy.pp_periodic_max_transfer_size;

	uhci_delete_td(uhcip, td);

	/* Insert another interrupt TD */
	if ((uhci_insert_hc_td(uhcip, tw->tw_cookie.dmac_address,
		tw->tw_length, pp, tw, PID_IN)) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_handle_intr_td: TD exhausted");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&uhcip->uhci_int_mutex);
		usba_hcdi_callback(pp->pp_pipe_handle,
			0, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);
		mutex_enter(&uhcip->uhci_int_mutex);
		mutex_enter(&pp->pp_mutex);
	}
	mutex_exit(&pp->pp_mutex);

}


/*
 * uhci_sendup_td_message:
 *
 * Get a message block and send the received device message to upstream.
 */

int
uhci_sendup_td_message(uhci_state_t *uhcip,
	uhci_pipe_private_t *pp,
	uhci_trans_wrapper_t *tw)
{
	usb_endpoint_descr_t	*eptd = pp->pp_pipe_handle->p_endpoint;
	mblk_t			*mp;
	uint_t			flags = 0;
	size_t			length = 0;
	uchar_t			*buf;
	uint32_t		usb_err = USB_CC_NOERROR;

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	ASSERT(tw != NULL);

	length = tw->tw_bytes_xfered;

	if (length == NULL) {
		USB_DPRINTF_L3(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_sendup_td_message: Zero length packet");
	}

	flags = tw->tw_flags;

	switch (eptd->bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_CONTROL:
			if ((tw->tw_tmp == UHCI_UNDERRUN_OCCURRED) &&
				(!(flags & USB_FLAGS_SHORT_XFER_OK))) {
				usb_err = USB_CC_DATA_UNDERRUN;
			}

			if (tw->tw_tmp == UHCI_OVERRUN_OCCURRED)
				usb_err = USB_CC_DATA_OVERRUN;

			/* Copy the data into the mblk_t */
			buf = (uchar_t *)tw->tw_buf + SETUP_SIZE;

			break;
		case USB_EPT_ATTR_INTR:
		case USB_EPT_ATTR_ISOCH:
			/* Copy the data into the mblk_t */
			buf = (uchar_t *)tw->tw_buf;
			break;
		case USB_EPT_ATTR_BULK:
			if ((tw->tw_tmp == UHCI_UNDERRUN_OCCURRED) &&
				(!(flags & USB_FLAGS_SHORT_XFER_OK))) {
				usb_err = USB_CC_DATA_UNDERRUN;
			}

			/* Copy the data into the mblk_t */
			buf = (uchar_t *)tw->tw_buf;
			break;
	    default:
			break;
	}

	/*
	 * Update kstat byte counts
	 * The control endpoints don't have direction bits so in order for
	 * control stats to be counted correctly an in bit must be faked on
	 * a control read.
	 */
	if (length > 0) {
		if ((eptd->bmAttributes & USB_EPT_ATTR_MASK) ==
		    USB_EPT_ATTR_CONTROL) {
			UHCI_DO_BYTE_STATS(uhcip, length, usb_pp->p_usb_device,
			    eptd->bmAttributes, USB_EPT_DIR_IN);
		} else {
			UHCI_DO_BYTE_STATS(uhcip, length, usb_pp->p_usb_device,
			    eptd->bmAttributes, eptd->bEndpointAddress);
		}
	}

	/* Allocate message block of required size */
	mp = allocb(length, BPRI_HI);

	if (mp == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
			"uhci_sendup_td_message: Allocb failed");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&uhcip->uhci_int_mutex);
		usba_hcdi_callback(pp->pp_pipe_handle,
			flags, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);
		mutex_enter(&uhcip->uhci_int_mutex);
		mutex_enter(&pp->pp_mutex);

		return (USB_NO_RESOURCES);
	}

	ASSERT(mp != NULL);

	/* Copy the data into the message */
	ddi_rep_get8(tw->tw_accesshandle,
		mp->b_rptr,
		buf,
		length,
		DDI_DEV_AUTOINCR);

	/* Increment the write pointer */
	mp->b_wptr = mp->b_wptr + length;

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	/* Do the callback */
	usba_hcdi_callback(pp->pp_pipe_handle,
		flags,
		mp,
		0,
		usb_err,
		USB_SUCCESS);

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	return (USB_SUCCESS);
}

/*
 * uhci_handle_ctrl_td:
 *
 * Handle a control Transfer Descriptor (TD).
 */

void
uhci_handle_ctrl_td(uhci_state_t *uhcip, gtd *td)
{
	mblk_t				*message;
	uint_t				flags = 0;
	int				rval;
	uhci_trans_wrapper_t		*tw = td->tw;
	uhci_pipe_private_t		*pp = tw->tw_pipe_private;
	ushort_t			direction;
	ushort_t			bytes_for_xfer;
	ushort_t			bytes_xfered;
	ushort_t			MaxPacketSize;
	uint_t				NAK_received;
	ushort_t			error = 0;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
	"uhci_handle_ctrl_td: pp = 0x%p tw = 0x%p td = 0x%p state = 0x%x",
		(void *)pp, (void *)tw, (void *)td, tw->tw_ctrl_state);

	mutex_enter(&pp->pp_mutex);
	ASSERT(mutex_owned(&uhcip->uhci_int_mutex));

	flags = tw->tw_flags;

	error = uhci_parse_td_error(td, &NAK_received);

	/*
	 * In case of control transfers, the device can send NAK when he is busy
	 * If a NAK is receved, then send the status TD again.
	 */

	if (error != USB_CC_NOERROR) {
		pp->pp_qh->element_ptr = td->link_ptr;
		uhci_delete_td(uhcip, td);
		if (NAK_received && (tw->tw_ctrl_state == STATUS)) {
			tw->tw_pipe_private->pp_data_toggle = 1;
			if (tw->tw_direction == PID_IN)
				direction = PID_OUT;
			else
				direction = PID_IN;

			if ((uhci_insert_hc_td(uhcip, NULL, 0, pp, tw,
				direction)) != USB_SUCCESS) {
				USB_DPRINTF_L2(PRINT_MASK_LISTS,
					uhcip->uhci_log_hdl,
					"uhci_handle_ctrl_td: TD exhausted");

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&uhcip->uhci_int_mutex);

				usba_hcdi_callback(
					tw->tw_pipe_private->pp_pipe_handle,
					flags, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_NO_RESOURCES);
				mutex_enter(&uhcip->uhci_int_mutex);
				mutex_enter(&pp->pp_mutex);
			}
			else
				tw->tw_ctrl_state = STATUS;
		} else {
			mutex_exit(&pp->pp_mutex);
			mutex_exit(&uhcip->uhci_int_mutex);

			usba_hcdi_callback(pp->pp_pipe_handle,
				flags, NULL, 0, error, USB_FAILURE);

			mutex_enter(&uhcip->uhci_int_mutex);
			mutex_enter(&pp->pp_mutex);

			uhci_deallocate_tw(uhcip, pp, tw);
		}
		mutex_exit(&pp->pp_mutex);
		return;
	}

	/*
	 * A control transfer consists of three phases:
	 *
	 * Setup
	 * Data (optional)
	 * Status
	 *
	 * There is a TD per phase. A TD for a given phase isn't
	 * enqueued until the previous phase is finished.
	 */

	switch (tw->tw_ctrl_state) {
		case SETUP:
			/*
			 * Enqueue either the data or the status
			 * phase depending on the length.
			 */

			pp->pp_data_toggle	= 1;
			uhci_delete_td(uhcip, td);

			/*
			 * If the length is 0, move to the status.
			 * If length is not 0, then we have some data
			 * to move on the bus to device either IN or OUT.
			*/
			if ((tw->tw_length - SETUP_SIZE) == 0) {

				/*
				 * There is no data stage,  then
				 * initiate status phase from the
				 * host.
				 */
				if ((uhci_insert_hc_td(uhcip,
					NULL, 0, pp, tw, PID_IN)) !=
					USB_SUCCESS) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
						uhcip->uhci_log_hdl,
						"uhci_handle_ctrl_td: No TD");

					mutex_exit(&uhcip->uhci_int_mutex);
					mutex_exit(&pp->pp_mutex);
					usba_hcdi_callback(pp->pp_pipe_handle,
						flags, NULL,
						0, USB_CC_UNSPECIFIED_ERR,
						USB_NO_RESOURCES);
					mutex_enter(&uhcip->uhci_int_mutex);
					return;
				}

				tw->tw_ctrl_state = STATUS;
			} else {

				uint_t xx;

				/*
				 * Each USB device can send/receive 8/16/32/64
				 * depending on implementation.
				 * We need to insert 'N = Number of byte/
				 * MaxpktSize" TD's in the lattice to send/
				 * receive the data. Though the USB protocol
				 * allows to insert more than one TD in the same
				 * frame, we are inserting only one TD in one
				 * frame. This is bcos OHCI has seen some
				 * problem when multiple TD's are inserted at
				 * the same time.
				 */

				tw->tw_length -= 8;
				MaxPacketSize =
				pp->pp_pipe_handle->p_endpoint->wMaxPacketSize;

				/*
				 * We dont know the maximum packet size that
				 * the device can handle(MaxPAcketSize=0).
				 * In that case insert a data phase with
				 * eight bytes or less.
				 */
				if (MaxPacketSize == 0) {
					if (tw->tw_length > 8)
						xx = 8;
					else
						xx = tw->tw_length;
				} else {
					if (tw->tw_length > MaxPacketSize)
						xx = MaxPacketSize;
					else
						xx = tw->tw_length;
				}

				tw->tw_tmp = xx;
				/*
				 * Create the TD.  If this is an OUT
				 * transaction,  the data is already
				 * in the buffer of the TW.
				 * Get first 8 bytes of the command only.
				 */
				if ((uhci_insert_hc_td(uhcip,
					tw->tw_cookie.dmac_address + SETUP_SIZE,
					xx, pp, tw,
					tw->tw_direction)) != USB_SUCCESS) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
						uhcip->uhci_log_hdl,
						"uhci_handle_ctrl_td: No TD");

					mutex_exit(&uhcip->uhci_int_mutex);
					mutex_exit(&pp->pp_mutex);

					usba_hcdi_callback(pp->pp_pipe_handle,
						flags, NULL,
						0, USB_CC_UNSPECIFIED_ERR,
						USB_NO_RESOURCES);
					mutex_enter(&uhcip->uhci_int_mutex);
					return;
				}

				tw->tw_ctrl_state = DATA;

			}

			USB_DPRINTF_L3(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
				"Setup complete: pp 0x%p td 0x%p",
				(void *)pp, (void *)td);

			break;
		case DATA:

			uhci_delete_td(uhcip, td);

			MaxPacketSize =
			pp->pp_pipe_handle->p_endpoint->wMaxPacketSize;

			/*
			 * Decreament pending bytes and increment the
			 * total number bytes transfered by the actual
			 * number of bytes transfered in this TD. If
			 * the number of bytes transfered is less than
			 * requested, that means an underrun has occurred.
			 * Set the tw_tmp varible to indicate UNDER run.
			 */

			if (td->td_dword2.Actual_len == ZERO_LENGTH)
				bytes_xfered = 0;
			else
				bytes_xfered = td->td_dword2.Actual_len + 1;

			tw->tw_bytes_pending -= bytes_xfered;
			tw->tw_bytes_xfered += bytes_xfered;

			if (bytes_xfered < tw->tw_tmp) {
				tw->tw_bytes_pending = 0;
				tw->tw_tmp = UHCI_UNDERRUN_OCCURRED;
			}

			if (bytes_xfered > tw->tw_tmp) {
				tw->tw_bytes_pending = 0;
				tw->tw_tmp = UHCI_OVERRUN_OCCURRED;
			}


			/*
			 * If no more bytes are pending, insert status
			 * phase. Otherwise insert data phase.
			 */

			if (tw->tw_bytes_pending) {
				if (tw->tw_bytes_pending > MaxPacketSize)
					bytes_for_xfer = MaxPacketSize;
				else
					bytes_for_xfer = tw->tw_bytes_pending;

				tw->tw_tmp = bytes_for_xfer;

				if ((uhci_insert_hc_td(uhcip,
					tw->tw_cookie.dmac_address +
					SETUP_SIZE + tw->tw_bytes_xfered,
					bytes_for_xfer, pp, tw,
					tw->tw_direction)) != USB_SUCCESS) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
						uhcip->uhci_log_hdl,
						"uhci_handle_ctrl_td: No TD");
					mutex_exit(&pp->pp_mutex);
					mutex_exit(&uhcip->uhci_int_mutex);
					usba_hcdi_callback(pp->pp_pipe_handle,
						flags,
						NULL,
						0,
						USB_CC_UNSPECIFIED_ERR,
						USB_NO_RESOURCES);
					mutex_enter(&uhcip->uhci_int_mutex);
					return;
				}

				tw->tw_ctrl_state = DATA;

				break;
			}

			pp->pp_data_toggle = 1;
			if (tw->tw_direction == PID_IN)
				direction = PID_OUT;
			else
				direction = PID_IN;

			if ((uhci_insert_hc_td(uhcip,
				NULL, 0, pp, tw,
				direction)) != USB_SUCCESS) {
				USB_DPRINTF_L2(PRINT_MASK_LISTS,
					uhcip->uhci_log_hdl,
					"uhci_handle_ctrl_td: TD exhausted");

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&uhcip->uhci_int_mutex);
				usba_hcdi_callback(pp->pp_pipe_handle,
					flags, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_NO_RESOURCES);
				mutex_enter(&uhcip->uhci_int_mutex);
				return;
			}


			tw->tw_ctrl_state = STATUS;

			USB_DPRINTF_L3(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
				"Data complete: pp 0x%p td 0x%p",
				(void *)pp, (void *)td);

			break;
		case STATUS:
			/*
			 * Send the data to the client if it is a DATA IN,
			 * else send just return status for DATA OUT commnads.
			 */
			if ((tw->tw_length != 0) &&
				(tw->tw_direction == PID_IN)) {
				/* Sync the streaming buffer */
				rval = ddi_dma_sync(tw->tw_dmahandle, 0,
					tw->tw_length,
					DDI_DMA_SYNC_FORCPU);

				ASSERT(rval == DDI_SUCCESS);

				/*
				 * Call uhci_sendup_td_message to send
				 * message to upstream. The function
				 * uhci_sendup_td_message returns
				 * USB_NO_RESOURCES if allocb fails and
				 * also sends error message to upstream
				 * by calling USBA callback function.
				 * Under error conditions just drop the
				 * current message.
				 */

				if ((uhci_sendup_td_message(uhcip, pp, tw))
					== USB_NO_RESOURCES) {
					USB_DPRINTF_L2(PRINT_MASK_LISTS,
					uhcip->uhci_log_hdl,
					"uhci_handle_ctrl_td: Drop message");
				}

			} else {
				message = NULL;

				UHCI_DO_BYTE_STATS(uhcip,
				    tw->tw_length,
				    tw->tw_pipe_private->pp_pipe_handle->
					p_usb_device,
				    tw->tw_pipe_private->pp_pipe_handle->
					p_endpoint->bmAttributes,
				    tw->tw_pipe_private->pp_pipe_handle->
					p_endpoint->bEndpointAddress);

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&uhcip->uhci_int_mutex);
				usba_hcdi_callback(
					tw->tw_pipe_private->pp_pipe_handle,
					flags,
					message,
					0,
					USB_CC_NOERROR,
					USB_SUCCESS);
				mutex_enter(&uhcip->uhci_int_mutex);
				mutex_enter(&pp->pp_mutex);
			}

			USB_DPRINTF_L3(PRINT_MASK_LISTS, uhcip->uhci_log_hdl,
				"Status complete: pp 0x%p td 0x%p",
				(void *)pp, (void *)td);

			uhci_delete_td(uhcip, td);
			uhci_deallocate_tw(uhcip, pp, tw);

			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_INTR, uhcip->uhci_log_hdl,
				"uhci_handle_ctrl_td: Bad control state");

			mutex_exit(&pp->pp_mutex);
			mutex_exit(&uhcip->uhci_int_mutex);
			usba_hcdi_callback(pp->pp_pipe_handle,
				flags, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_FAILURE);

			mutex_enter(&uhcip->uhci_int_mutex);
			mutex_enter(&pp->pp_mutex);
	}
	mutex_exit(&pp->pp_mutex);
}

/*
 * uhci_handle_intr_td_errors
 *		Handles the errors encountered for the interrupt transfers.
 */

void
uhci_handle_intr_td_errors(uhci_state_t *uhcip, gtd *td)
{

	uint_t			error;
	uint_t			usb_err = USB_CC_NOERROR;
	uhci_trans_wrapper_t 	*tw;
	usb_pipe_handle_impl_t	*ph;
	uhci_pipe_private_t	*pp;
	uint_t	flags = 0;

	error = td->td_dword2.status & UHCI_ERR_STATUS_MASK;

	switch (error) {
		case UHCI_TD_STALLED:
			usb_err = USB_CC_STALL;
			break;

		case UHCI_TD_DATA_BUFFER_ERR:
			if (td->td_dword3.PID == PID_IN)
				usb_err = USB_CC_DATA_OVERRUN;
			else
				usb_err = USB_CC_DATA_UNDERRUN;
			break;

		case UHCI_TD_BABBLE_ERR:
			usb_err = USB_CC_UNSPECIFIED_ERR;
			break;

		case UHCI_TD_NAK_RECEIVED:
			break;

		case UHCI_TD_CRC_TIMEOUT:
			usb_err = USB_CC_TIMEOUT;
			break;

		case UHCI_TD_BITSTUFF_ERR:
			usb_err = USB_CC_BITSTUFFING;
			break;
		default:
			usb_err = USB_CC_UNSPECIFIED_ERR;
			break;
	}

	if (error & UHCI_TD_STALLED)
		usb_err = USB_CC_STALL;

	tw = td->tw;
	ph = tw->tw_pipe_private->pp_pipe_handle;
	pp = (uhci_pipe_private_t *)ph->p_hcd_private;

	flags = tw->tw_flags;

	uhci_delete_td(uhcip, td);

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&uhcip->uhci_int_mutex);

	usba_hcdi_callback(ph, flags, NULL, 0, usb_err, USB_FAILURE);

	mutex_enter(&uhcip->uhci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	uhci_deallocate_tw(uhcip, tw->tw_pipe_private, tw);
}

/*
 * uhci_parse_td_error
 * 	Parses the Transfer Descriptors error
 */

uint_t
uhci_parse_td_error(gtd *td, uint_t *NAK_received)
{

	uint_t		error;
	uint_t		usb_err = USB_CC_NOERROR;

	*NAK_received = UHCI_FALSE;

	error = td->td_dword2.status & UHCI_ERR_STATUS_MASK;

	if (error & UHCI_TD_STALLED) {
		usb_err = USB_CC_STALL;
	} else if (error & UHCI_TD_DATA_BUFFER_ERR) {
		if (td->td_dword3.PID == PID_IN)
			usb_err = USB_CC_DATA_OVERRUN;
		else
			usb_err = USB_CC_DATA_UNDERRUN;
	} else if (error & UHCI_TD_BABBLE_ERR)
		usb_err = USB_CC_UNSPECIFIED_ERR;

	else if (error & UHCI_TD_NAK_RECEIVED) {
		usb_err = USB_CC_STALL;
		*NAK_received = UHCI_TRUE;
	}

	else if (error & UHCI_TD_CRC_TIMEOUT)
		usb_err = USB_CC_TIMEOUT;

	else if (error & UHCI_TD_BITSTUFF_ERR)
		usb_err = USB_CC_BITSTUFFING;

	if (error & UHCI_TD_NAK_RECEIVED)
		*NAK_received = UHCI_TRUE;

	return (usb_err);

}


/*
 * cb_ops entry points
 */
static dev_info_t *
uhci_get_dip(dev_t dev)
{
	minor_t minor = getminor(dev);
	int instance = (int)minor & ~HUBD_IS_ROOT_HUB;
	uhci_state_t *uhcip = ddi_get_soft_state(uhci_statep, instance);

	if (uhcip) {
		return (uhcip->uhci_dip);
	} else {
		return (NULL);
	}
}


static int
uhci_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	dev_info_t *dip = uhci_get_dip(*devp);

	return (usba_hubdi_open(dip, devp, flags, otyp, credp));
}


static int
uhci_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	dev_info_t *dip = uhci_get_dip(dev);

	return (usba_hubdi_close(dip, dev, flag, otyp, credp));
}


static int
uhci_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp)
{
	dev_info_t *dip = uhci_get_dip(dev);

	return (usba_hubdi_ioctl(dip, dev, cmd, arg, mode,
		credp, rvalp));
}
