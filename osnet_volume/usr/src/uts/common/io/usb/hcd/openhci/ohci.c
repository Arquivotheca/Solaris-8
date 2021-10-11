/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ohci.c	1.15	99/11/18 SMI"

/*
 * Open Host Controller Driver (OHCI)
 *
 * The OpenHCI driver is a software driver which interfaces to the Universal
 * Serial Bus layer (USBA) and the Host Controller (HC). The interface to
 * the Host Controller is defined by the OpenHCI Host Controller Interface.
 */
#include <sys/note.h>
#include <sys/types.h>
#include <sys/pci.h>
#include <sys/sunddi.h>
#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/usb/hcd/openhci/ohci.h>
#include <sys/usb/hcd/openhci/ohcid.h>
#include <sys/usb/hubd/hubdvar.h>

/* Pointer to the state structure */
static void *ohci_statep;

/* Number of instances */
#define	OHCI_INSTS	2

/* Adjustable variables for the size of the pools */
static int	ed_pool_size = 100;
static int	td_pool_size = 200;

/* Maximum bulk transfer size */
static ohci_bulk_transfer_size = OHCI_MAX_BULK_XFER_SIZE;

/*
 * Initialize the values which are used for setting up head pointers for
 * the 32ms scheduling lists which starts from the HCCA.
 */
static	uchar_t index[NUM_INTR_ED_LISTS / 2] = {0x0, 0x8, 0x4, 0xc,
						0x2, 0xa, 0x6, 0xe,
						0x1, 0x9, 0x5, 0xd,
						0x3, 0xb, 0x7, 0xf};

/* Debugging information */
static uint_t ohci_errmask = (uint_t)PRINT_MASK_ALL;
static uint_t ohci_errlevel = USB_LOG_L1;
static uint_t ohci_instance_debug = (uint_t)-1;
static uint_t ohci_show_label = USB_ALLOW_LABEL;

_NOTE(SCHEME_PROTECTS_DATA("safe sharing", ohci_show_label))

#ifdef	RIO
/* Enable/disable RIO cache */
int ohci_rio_cache = 0;

/* Debugging purpose */
static uint_t ohci_target_abort_debug = 0;
#endif	/* RIO */

/*
 * HCDI entry points
 *
 * The Host Controller Driver Interfaces (HCDI) are the software interfaces
 * between the Universal Serial Bus Driver (USBA) and the Host	Controller
 * Driver (HCD). The HCDI interfaces or entry points are subject to change.
 */
static int	ohci_hcdi_client_init(
				usb_device_t		*usb_device);

static int	ohci_hcdi_client_free(
				usb_device_t		*usb_device);

static int	ohci_hcdi_pipe_open(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t			flags);

static int	ohci_hcdi_pipe_close(
				usb_pipe_handle_impl_t	*pipe_handle);

static int	ohci_hcdi_pipe_reset(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t			usb_flags);

static int	ohci_hcdi_pipe_abort(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t			usb_flags);

static int	ohci_hcdi_pipe_get_policy(
				usb_pipe_handle_impl_t	*pipe_handle,
				usb_pipe_policy_t	*policy);

static int	ohci_hcdi_pipe_set_policy(
				usb_pipe_handle_impl_t	*pipe_handle,
				usb_pipe_policy_t	*policy,
				uint_t			usb_flags);

static int	ohci_hcdi_pipe_device_ctrl_receive(
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t			bmRequestType,
				uchar_t			bRequest,
				uint16_t		wValue,
				uint16_t		wIndex,
				uint16_t		wLength,
				uint_t			usb_flags);

static int	ohci_hcdi_pipe_device_ctrl_send(
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t			bmRequestType,
				uchar_t			bRequest,
				uint16_t		wValue,
				uint16_t		wIndex,
				uint16_t		wLength,
				mblk_t			*data,
				uint_t			usb_flags);

static int	ohci_hcdi_bulk_transfer_size(dev_info_t	*dip,
				size_t			*size);

static int	ohci_hcdi_pipe_receive_bulk_data(
				usb_pipe_handle_impl_t	*pipe_handle,
				size_t			length,
				uint_t			usb_flags);

static int	ohci_hcdi_pipe_send_bulk_data(
				usb_pipe_handle_impl_t	*pipe_handle,
				mblk_t			*data,
				uint_t			usb_flags);

static int	ohci_hcdi_pipe_start_polling(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t			flags);

static int	ohci_hcdi_pipe_stop_polling(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t			flags);

static int	ohci_hcdi_pipe_send_isoc_data(
				usb_pipe_handle_impl_t	*pipe_handle,
				mblk_t			*data,
				uint_t			usb_flags);

/*
 * Internal Function Prototypes
 */

/* Host Controller Driver (HCD) initialization functions */
static void	ohci_set_dma_attributes(openhci_state_t *ohcip);
static int	ohci_allocate_pools(openhci_state_t *ohcip);
static void	ohci_decode_ddi_dma_addr_bind_handle_result(
				openhci_state_t *ohcip,
				int result);
static int	ohci_map_regs(openhci_state_t *ohcip);
static int	ohci_register_intrs_and_init_mutex(openhci_state_t *ohcip);
static int	ohci_init_ctlr(openhci_state_t *ohcip, uchar_t flag);
static int	ohci_init_hcca(openhci_state_t *ohcip);
static void	ohci_build_interrupt_lattice(openhci_state_t *ohci);
static int	ohci_take_control(openhci_state_t *ohcip);
static 		usb_hcdi_ops_t *ohci_alloc_hcdi_ops(openhci_state_t *ohcip);
static void	ohci_init_root_hub(openhci_state_t *ohcip);
static int	ohci_load_root_hub_driver(openhci_state_t *ohcip);

/* Host Controller Driver (HCD) deinitialization functions */
static void	ohci_cleanup(openhci_state_t *ohcip);
static int	ohci_cpr_suspend(openhci_state_t *ohcip);
static int	ohci_cpr_resume(openhci_state_t *ohcip);
static void	ohci_unload_root_hub_driver(openhci_state_t *ohcip);

/* Root hub related functions */
static int	ohci_handle_root_hub_pipe_open(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t usb_flags);
static int	ohci_handle_root_hub_pipe_close(
				usb_pipe_handle_impl_t	*pipe_handle);
static int	ohci_handle_root_hub_pipe_reset(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t usb_flags);
static int	ohci_handle_root_hub_pipe_start_polling(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t usb_flags);
static int	ohci_handle_root_hub_pipe_stop_polling(
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t usb_flags);
static int	ohci_handle_root_hub_request(
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t		bmRequestType,
				uchar_t		bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t *data,
				uint_t usb_flags);
static int	ohci_handle_set_clear_port_feature(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uchar_t 	bRequest,
				uint16_t	wValue,
				uint16_t	port);
static void	ohci_handle_port_power(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port, uint_t on);
static void	ohci_handle_port_enable(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port, uint_t on);
static void	ohci_handle_clrchng_port_enable(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port);
static void	ohci_handle_port_suspend(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port, uint_t on);
static void	ohci_handle_clrchng_port_suspend(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port);
static void	ohci_handle_port_reset(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port);
static int	ohci_handle_complete_port_reset(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port);
static void	ohci_handle_clear_port_connection(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t port);
static int	ohci_handle_get_port_status(openhci_state_t *ohcip,
				uint16_t port,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t wLength);
static int	ohci_handle_get_hub_descriptor(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t wLength);
static int	ohci_handle_get_hub_status(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uint16_t wLength);

/* Bandwidth Allocation functions */
int		ohci_allocate_bandwidth(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
void		ohci_deallocate_bandwidth(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
static uint_t	ohci_compute_total_bandwidth(usb_endpoint_descr_t *endpoint,
				usb_port_status_t port_status);
static int	ohci_bandwidth_adjust(openhci_state_t *ohcip,
				usb_endpoint_descr_t *endpoint,
				usb_port_status_t port_status);
static uint_t	ohci_lattice_height(uint_t bandwidth);
static uint_t	ohci_lattice_parent(uint_t node);
static uint_t	ohci_leftmost_leaf(uint_t node, uint_t height);
static uint_t	ohci_hcca_intr_index(uint_t node);
static uint_t	pow_2(uint_t x);
static uint_t	log_2(uint_t x);

/* Endpoint Descriptor (ED) related functions */
hc_ed_t 	*ohci_alloc_hc_ed(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t	*pipe_handle);
static uint_t	ohci_unpack_endpoint(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
static void	ohci_insert_ed(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
static void	ohci_insert_ctrl_ed(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static void	ohci_insert_bulk_ed(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static void	ohci_insert_periodic_ed(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static int	ohci_modify_sKip_bit(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				skip_bit_t  action);
static void	ohci_remove_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp);
static void	ohci_remove_ctrl_ed(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static void	ohci_remove_bulk_ed(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static void	ohci_remove_periodic_ed(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static void	ohci_insert_ed_on_reclaim_list(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static void	ohci_detach_ed_from_list(openhci_state_t *ohcip,
				hc_ed_t *ept,
				uint_t ept_type);
void		ohci_deallocate_ed(openhci_state_t *ohcip, hc_ed_t *old_ed);

/* Transfer Descriptor (TD) related functions */
static int	ohci_initialize_dummy(openhci_state_t *ohcip,
				hc_ed_t *ept);
static int	ohci_common_ctrl_routine(
				usb_pipe_handle_impl_t *pipe_handle,
				uchar_t	bmRequestType,
				uchar_t	bRequest,
				uint16_t wValue,
				uint16_t wIndex,
				uint16_t wLength,
				mblk_t *data,
				uint_t usb_flags);
static int	ohci_insert_ctrl_td(
				openhci_state_t *ohcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t	bmRequestType,
				uchar_t	bRequest,
				uint16_t wValue,
				uint16_t wIndex,
				uint16_t wLength,
				mblk_t *data,
				uint_t usb_flags);
static int	ohci_create_setup_pkt(
				openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw,
				uchar_t bmRequestType,
				uchar_t bRequest,
				uint16_t wValue,
				uint16_t wIndex,
				uint16_t wLength,
				mblk_t *data, uint_t usb_flags);
static int	ohci_common_bulk_routine(
				usb_pipe_handle_impl_t *pipe_handle,
				size_t length,
				mblk_t *data,
				uint_t usb_flags);
static int	ohci_insert_bulk_td(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				ohci_handler_function_t,
				size_t length,
				mblk_t *data,
				uint_t flags);
int		ohci_insert_intr_td(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uint_t flags,
				ohci_handler_function_t tw_handle_td,
				usb_opaque_t tw_handle_callback_value);
static int 	ohci_insert_isoc_out_tds(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				mblk_t *data,
				uint_t flags);
static int	ohci_insert_hc_td(openhci_state_t *ohcip,
				uint_t hcgtd_ctrl,
				uint32_t hcgtd_iommu_cbp,
				size_t hcgtd_length,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw);
static gtd	*ohci_allocate_td_from_pool(openhci_state_t *ohcip);
static void	ohci_fill_in_td(openhci_state_t *ohcip,
				gtd *td,
				gtd *new_dummy,
				uint_t hcgtd_ctrl,
				uint32_t hcgtd_iommu_cbp,
				size_t hcgtd_length,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw);
static void	ohci_init_itd(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				uint32_t hcgtd_iommu_cbp,
				uint_t hcgtd_ctrl,
				gtd *td);
static void	ohci_insert_td_on_tw(openhci_state_t *ohcip,
				ohci_trans_wrapper_t *tw,
				gtd *td);
void		ohci_traverse_tds(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
static void	ohci_done_list_tds(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
void		ohci_deallocate_gtd(openhci_state_t *ohcip, gtd *old_gtd);
static void	ohci_start_xfer_timer(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw);
static void	ohci_stop_xfer_timer(openhci_state_t *ohcip,
				ohci_trans_wrapper_t *tw,
				uint_t flag);
static void	ohci_xfer_timeout_handler(void *arg);
static void	ohci_remove_tw_from_timeout_list(openhci_state_t *ohcip,
				ohci_trans_wrapper_t *tw);
static void	ohci_start_timer(openhci_state_t *ohcip);

/* Transfer Wrapper (TW) functions */
static ohci_trans_wrapper_t  *ohci_create_transfer_wrapper(
				openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				size_t length, uint_t usb_flags);
void		ohci_deallocate_tw(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw);
static void	ohci_free_dma_resources(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle);
static	void	ohci_free_tw(openhci_state_t *ohcip,
				ohci_trans_wrapper_t *tw);

/* Interrupt Handling functions */
static uint_t	ohci_intr();
static void	ohci_handle_missed_intr(openhci_state_t *ohcip);
static int	ohci_handle_ue(openhci_state_t *ohcip);
static void	ohci_handle_endpoint_reclamation(openhci_state_t *ohcip);
static void	ohci_traverse_done_list(openhci_state_t *ohcip,
				gtd *head_done_list);
static gtd	*ohci_reverse_done_list(openhci_state_t *ohcip,
				gtd *head_done_list);
static void	ohci_handle_normal_td(openhci_state_t *ohcip, gtd *td,
				ohci_trans_wrapper_t *tw);
static void	ohci_handle_ctrl_td(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw,
				gtd *td,
				void *);
static void	ohci_handle_bulk_td(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw, gtd *td,
				void *);
static void	ohci_handle_intr_td(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw, gtd *td,
				void *);
static void	ohci_handle_isoc_td(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw,
				gtd *td,
				void *);
static int	ohci_sendup_td_message(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw,
				gtd *td,
				uint_t error);
static void	ohci_handle_root_hub_status_change(openhci_state_t *ohcip);

/* Miscellaneous functions */
openhci_state_t *ohci_obtain_state(dev_info_t *dip);
static	int	ohci_wait_for_sof(openhci_state_t *ohcip);
static	int	ohci_wait_for_transfers_completion(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
static	int	ohci_check_for_transfers_completion(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp);
uint32_t	EDpool_cpu_to_iommu(openhci_state_t *ohcip, hc_ed_t *addr);
uint32_t	TDpool_cpu_to_iommu(openhci_state_t *ohcip, gtd *addr);
static hc_ed_t	*EDpool_iommu_to_cpu(openhci_state_t *ohcip, uintptr_t addr);
gtd		*TDpool_iommu_to_cpu(openhci_state_t *ohcip, uintptr_t addr);
static void	ohci_handle_error(openhci_state_t *ohcip, gtd *td,
				uint_t error);
static int	ohci_parse_error(openhci_state_t *ohcip, gtd *td);
static void	ohci_print_op_regs(openhci_state_t *ohcip);
static void	ohci_print_ed(openhci_state_t *ohcip, hc_ed_t  *ed);
static void	ohci_print_td(openhci_state_t *ohcip, gtd *gtd);
static void	ohci_create_stats(openhci_state_t *ohcip);
static void	ohci_destroy_stats(openhci_state_t *ohcip);


/*
 * External Function Prototypes
 */
extern int	ohci_hcdi_polled_input_init(usb_pipe_handle_impl_t *,
				uchar_t **, usb_console_info_impl_t *);
extern int	ohci_hcdi_polled_input_enter(usb_console_info_impl_t *);
extern int	ohci_hcdi_polled_read(usb_console_info_impl_t *, uint_t *);
extern int	ohci_hcdi_polled_input_exit(usb_console_info_impl_t *);
extern int	ohci_hcdi_polled_input_fini(usb_console_info_impl_t *);
extern int	usba_hubdi_root_hub_power(dev_info_t *dip, int comp, int level);
extern int	usb_force_enable_pm;

#ifdef	DEBUG
/*
 * Dump support
 */
static void ohci_dump(uint_t, usb_opaque_t);
static void ohci_dump_state(openhci_state_t *, uint_t);
static void ohci_dump_ed_list(openhci_state_t *, char *, hc_ed_t *);
static void ohci_dump_hcca_list(openhci_state_t *);
static void ohci_dump_td_list(openhci_state_t *, gtd *, gtd *);
static kmutex_t ohci_dump_mutex;
#endif	/* DEBUG */

#ifdef	RIO
extern void	prom_printf(const char *fmt, ...);
#endif	/* RIO */

/*
 * Device operations (dev_ops) entries function prototypes.
 *
 * We use the hub cbops since all nexus ioctl operations defined so far will
 * be executed by the root hub. The following are the Host Controller Driver
 * (HCD) entry points.
 *
 * the open/close/ioctl functions call the corresponding usba_hubdi_*
 * calls after looking up the dip thru the dev_t.
 */
static int ohci_open(dev_t *devp, int flags, int otyp, cred_t *credp);
static int ohci_close(dev_t dev, int flag, int otyp, cred_t *credp);
static int ohci_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp);


static int ohci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int ohci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int ohci_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);


static struct cb_ops ohci_cb_ops = {
	ohci_open,			/* Open */
	ohci_close,			/* Close */
	nodev,				/* Strategy */
	nodev,				/* Print */
	nodev,				/* Dump */
	nodev,				/* Read */
	nodev,				/* Write */
	ohci_ioctl,			/* Ioctl */
	nodev,				/* Devmap */
	nodev,				/* Mmap */
	nodev,				/* Segmap */
	nochpoll,			/* Poll */
	ddi_prop_op,			/* cb_prop_op */
	NULL,				/* Streamtab */
	D_NEW | D_MP | D_HOTPLUG	/* Driver compatibility flag */
};

static struct dev_ops ohci_ops = {
	DEVO_REV,		/* Devo_rev */
	0,			/* Refcnt */
	ohci_info,		/* Info */
	nulldev,		/* Identify */
	nulldev,		/* Probe */
	ohci_attach,		/* Attach */
	ohci_detach,		/* Detach */
	nodev,			/* Reset */
	&ohci_cb_ops,		/* Driver operations */
	&usba_hubdi_busops,	/* Bus operations */
	usba_hubdi_root_hub_power	/* Power */
};

/*
 * The USBA library must be loaded for this driver.
 */
static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module. This one is a driver */
	"USB OpenHCI Driver 1.15", /* Name of the module. */
	&ohci_ops,		/* Driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};


int
_init(void)
{
	int error;

	/* Initialize the soft state structures */
	if ((error = ddi_soft_state_init(&ohci_statep, sizeof (openhci_state_t),
			OHCI_INSTS)) != 0) {
		return (error);
	}

	/* Install the loadable module */
	if ((error = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&ohci_statep);
	}

#ifdef	DEBUG
	mutex_init(&ohci_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif	/* DEBUG */

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
#ifdef	DEBUG
		mutex_destroy(&ohci_dump_mutex);
#endif	/* DEBUG */

		/* Release per module resources */
		ddi_soft_state_fini(&ohci_statep);
	}

	return (error);
}


/*
 * Host Controller Driver (HCD) entry points
 */

/*
 * ohci_attach:
 */
static int
ohci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance;		/* Instance number */
	openhci_state_t	*ohcip = NULL;		/* Pointer to soft state */
	usba_hcdi_register_args_t	hcdi_args;

	switch (cmd) {
		case DDI_ATTACH:
			break;
		case DDI_RESUME:
			ohcip = ohci_obtain_state(dip);

			return (ohci_cpr_resume(ohcip));
		default:
			return (DDI_FAILURE);
	}

	/* Get the instance and create soft state */
	instance = ddi_get_instance(dip);

	if (ddi_soft_state_zalloc(ohci_statep, instance) != 0) {

		return (DDI_FAILURE);
	}

	ohcip = ddi_get_soft_state(ohci_statep, instance);
	if (ohcip == NULL) {

		return (DDI_FAILURE);
	}

	ohcip->ohci_flags = OHCI_ATTACH;

	ohcip->ohci_log_hdl = usb_alloc_log_handle(dip, "usb", &ohci_errlevel,
				&ohci_errmask, &ohci_instance_debug,
				&ohci_show_label, 0);

	ohcip->ohci_flags |= OHCI_ZALLOC;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohcip = 0x%p", (void *)ohcip);

	/* Initialize the DMA attributes */
	ohci_set_dma_attributes(ohcip);

	/* Save the dip and instance */
	ohcip->ohci_dip = dip;
	ohcip->ohci_instance = instance;

	/* Initialize the kstat structures */
	ohci_create_stats(ohcip);

	/* Create the td and ed pools */
	if (ohci_allocate_pools(ohcip) != DDI_SUCCESS) {
		ohci_cleanup(ohcip);
		return (DDI_FAILURE);
	}

	/* Map the registers */
	if (ohci_map_regs(ohcip) != DDI_SUCCESS) {
		ohci_cleanup(ohcip);
		return (DDI_FAILURE);
	}

	/* Register interrupts */
	if (ohci_register_intrs_and_init_mutex(ohcip) != DDI_SUCCESS) {
		ohci_cleanup(ohcip);
		return (DDI_FAILURE);
	}
	ohcip->ohci_flags |= (OHCI_INTR | OHCI_LOCKS);

	mutex_enter(&ohcip->ohci_int_mutex);
	/* Initialize the controller */
	if (ohci_init_ctlr(ohcip, OHCI_CTLR_INIT) != DDI_SUCCESS) {
		mutex_exit(&ohcip->ohci_int_mutex);
		ohci_cleanup(ohcip);
		return (DDI_FAILURE);
	}

	/*
	 * At this point, the hardware wiil be okay.
	 * Initialize the usb_hcdi structure
	 */
	ohcip->ohci_hcdi_ops = ohci_alloc_hcdi_ops(ohcip);

	mutex_exit(&ohcip->ohci_int_mutex);

	/*
	 * Make this HCD instance known to USBA
	 * (dma_attr must be passed for USBA busctl's)
	 */
	hcdi_args.usba_hcdi_register_version = HCDI_REGISTER_VERS_0;
	hcdi_args.usba_hcdi_register_dip = dip;
	hcdi_args.usba_hcdi_register_ops = ohcip->ohci_hcdi_ops;
	hcdi_args.usba_hcdi_register_dma_attr = &ohcip->ohci_dma_attr;
	hcdi_args.usba_hcdi_register_iblock_cookiep = ohcip->ohci_iblk_cookie;
	hcdi_args.usba_hcdi_register_log_handle = ohcip->ohci_log_hdl;

	if (usba_hcdi_register(&hcdi_args, 0) != DDI_SUCCESS) {

		ohci_cleanup(ohcip);
		return (DDI_FAILURE);
	}
	ohcip->ohci_flags |= OHCI_USBAREG;

	mutex_enter(&ohcip->ohci_int_mutex);
	ohci_init_root_hub(ohcip);
	mutex_exit(&ohcip->ohci_int_mutex);

	/* Finally load the root hub driver */
	if (ohci_load_root_hub_driver(ohcip) != 0) {

		ohci_cleanup(ohcip);
		return (DDI_FAILURE);
	}
	ohcip->ohci_flags |= OHCI_RHREG;

	/* Display information in the banner */
	ddi_report_dev(dip);

	mutex_enter(&ohcip->ohci_int_mutex);

	/* Reset the ohci initilization flag */
	ohcip->ohci_flags &= ~OHCI_ATTACH;

	/* Print the Host Control's Operational registers */
	ohci_print_op_regs(ohcip);

	/* For RIO we need to call pci_report_pmcap */
	if (OHCI_IS_RIO(ohcip)) {

		(void) pci_report_pmcap(dip, PCI_PM_IDLESPEED, (void *)4096);
	}

	mutex_exit(&ohcip->ohci_int_mutex);

#ifdef	DEBUG
	mutex_enter(&ohci_dump_mutex);

	/* Initialize the dump support */
	ohcip->ohci_dump_ops = usba_alloc_dump_ops();
	ohcip->ohci_dump_ops->usb_dump_ops_version = USBA_DUMP_OPS_VERSION_0;
	ohcip->ohci_dump_ops->usb_dump_func = ohci_dump;
	ohcip->ohci_dump_ops->usb_dump_cb_arg = (usb_opaque_t)ohcip;
	ohcip->ohci_dump_ops->usb_dump_order = USB_DUMPOPS_OHCI_ORDER;
	usba_dump_register(ohcip->ohci_dump_ops);
	mutex_exit(&ohci_dump_mutex);
#endif	/* DEBUG */

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_attach: dip = 0x%p done", (void *)dip);

	return (DDI_SUCCESS);
}


/*
 * ohci_detach:
 */
static int
ohci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	openhci_state_t *ohcip = ohci_obtain_state(dip);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl, "ohci_detach:");

	switch (cmd) {
	case DDI_DETACH:
		ohci_cleanup(ohcip);

		return (DDI_SUCCESS);
	case DDI_SUSPEND:
		return (ohci_cpr_suspend(ohcip));
	default:
		return (DDI_FAILURE);
	}
}


/*
 * ohci_info:
 */
/* ARGSUSED */
static int
ohci_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t dev;
	openhci_state_t  *ohcip;
	int instance, error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		instance = OHCI_UNIT(dev);
		if ((ohcip = ddi_get_soft_state(ohci_statep, instance)) == NULL)

			return (DDI_FAILURE);

		*result = (void *)ohcip->ohci_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		dev = (dev_t)arg;
		instance = OHCI_UNIT(dev);
		*result = (void *)(uintptr_t)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}

	return (error);
}


/*
 * cb_ops entry points
 */
static dev_info_t *
ohci_get_dip(dev_t dev)
{
	minor_t minor = getminor(dev);
	int instance = (int)minor & ~HUBD_IS_ROOT_HUB;
	openhci_state_t *ohcip = ddi_get_soft_state(ohci_statep, instance);

	if (ohcip) {
		return (ohcip->ohci_dip);
	} else {
		return (NULL);
	}
}


static int
ohci_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	dev_info_t *dip = ohci_get_dip(*devp);

	return (usba_hubdi_open(dip, devp, flags, otyp, credp));
}


static int
ohci_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	dev_info_t *dip = ohci_get_dip(dev);

	return (usba_hubdi_close(dip, dev, flag, otyp, credp));
}


static int
ohci_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp)
{
	dev_info_t *dip = ohci_get_dip(dev);

	return (usba_hubdi_ioctl(dip, dev, cmd, arg, mode,
						credp, rvalp));
}


/*
 * Host Controller Driver (HCD) initialization functions
 */

/*
 * ohci_create_stats:
 *
 * Allocate and initialize the ohci kstat structures
 */
void
ohci_create_stats(openhci_state_t *ohcip)
{
	char			kstatname[KSTAT_STRLEN];
	const char		*dname = ddi_driver_name(ohcip->ohci_dip);
	ohci_intrs_stats_t	*isp;
	int			i;
	char			*usbtypes[USB_N_COUNT_KSTATS] =
				    {"ctrl", "isoch", "bulk", "intr"};
	uint_t			instance = ohcip->ohci_instance;

	if (OHCI_INTRS_STATS(ohcip) == NULL) {
		(void) sprintf(kstatname, "%s%d,intrs", dname, instance);
		OHCI_INTRS_STATS(ohcip) = kstat_create("usba", instance,
		    kstatname, "usb_interrupts", KSTAT_TYPE_NAMED,
		    sizeof (ohci_intrs_stats_t) / sizeof (kstat_named_t),
		    KSTAT_FLAG_PERSISTENT);

		if (OHCI_INTRS_STATS(ohcip) != NULL) {
			isp = OHCI_INTRS_STATS_DATA(ohcip);
			kstat_named_init(&isp->ohci_hcr_intr_total,
			"Interrupts Total", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_not_claimed,
			"Not Claimed", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_so,
			"Schedule Overruns", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_wdh,
			"Writeback Done Head", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_sof,
			"Start Of Frame", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_rd,
			"Resume Detected", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_ue,
			"Unrecoverable Error", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_fno,
			"Frame No. Overflow", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_rhsc,
			"Root Hub Status Change", KSTAT_DATA_UINT64);
			kstat_named_init(&isp->ohci_hcr_intr_oc,
			"Change In Ownership", KSTAT_DATA_UINT64);

			OHCI_INTRS_STATS(ohcip)->ks_private = ohcip;
			OHCI_INTRS_STATS(ohcip)->ks_update = nulldev;
			kstat_install(OHCI_INTRS_STATS(ohcip));
		}
	}

	if (OHCI_TOTAL_STATS(ohcip) == NULL) {
		(void) sprintf(kstatname, "%s%d,total", dname, instance);
		OHCI_TOTAL_STATS(ohcip) = kstat_create("usba", instance,
		    kstatname, "usb_byte_count", KSTAT_TYPE_IO, 1,
		    KSTAT_FLAG_PERSISTENT);

		if (OHCI_TOTAL_STATS(ohcip) != NULL) {
			kstat_install(OHCI_TOTAL_STATS(ohcip));
		}
	}

	for (i = 0; i < USB_N_COUNT_KSTATS; i++) {
		if (ohcip->ohci_count_stats[i] == NULL) {
			(void) sprintf(kstatname, "%s%d,%s", dname, instance,
			    usbtypes[i]);
			ohcip->ohci_count_stats[i] = kstat_create("usba",
			    instance, kstatname, "usb_byte_count",
			    KSTAT_TYPE_IO, 1, KSTAT_FLAG_PERSISTENT);

			if (ohcip->ohci_count_stats[i] != NULL) {
				kstat_install(ohcip->ohci_count_stats[i]);
			}
		}
	}
}

/*
 * ohci_set_dma_attributes:
 *
 * Set the limits in the DMA attributes structure. Most of the values used
 * in the  DMA limit structres are the default values as specified by  the
 * Writing PCI device drivers document.
 */
static void
ohci_set_dma_attributes(openhci_state_t *ohcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_set_dma_attributes:");

	/* Initialize the DMA attributes */
	ohcip->ohci_dma_attr.dma_attr_version = DMA_ATTR_V0;
	ohcip->ohci_dma_attr.dma_attr_addr_lo = 0x00000000ull;
	ohcip->ohci_dma_attr.dma_attr_addr_hi = 0xfffffffeull;

	/* 32 bit addressing */
	ohcip->ohci_dma_attr.dma_attr_count_max = OHCI_DMA_ATTR_COUNT_MAX;

	/* Byte alignment */
	ohcip->ohci_dma_attr.dma_attr_align = 0x1;

	/*
	 * Since PCI  specification is byte alignment, the
	 * burstsize field should be set to 1 for PCI devices.
	 */
	ohcip->ohci_dma_attr.dma_attr_burstsizes = 0x1;

	ohcip->ohci_dma_attr.dma_attr_minxfer = 0x1;
	ohcip->ohci_dma_attr.dma_attr_maxxfer = OHCI_DMA_ATTR_MAX_XFER;
	ohcip->ohci_dma_attr.dma_attr_seg = 0xffffffffull;
	ohcip->ohci_dma_attr.dma_attr_sgllen = 1;
	ohcip->ohci_dma_attr.dma_attr_granular = OHCI_DMA_ATTR_GRANULAR;
	ohcip->ohci_dma_attr.dma_attr_flags = 0;
}


/*
 * ohci_allocate_pools:
 *
 * Allocate the system memory for the Endpoint Descriptor (ED) and for	the
 * Transfer Descriptor (TD) pools. Both ED and TD structures must be aligned
 * to a 16 byte boundary.
 */
static int
ohci_allocate_pools(openhci_state_t *ohcip)
{
	ddi_device_acc_attr_t	dev_attr;
	size_t			real_length;
	int			i;
	int			result;
	uint_t			ccount;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_allocate_pools:");

	/* The host controller will be little endian */
	dev_attr.devacc_attr_version	= DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags  = DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder	= DDI_STRICTORDER_ACC;

	/* Allocate the TD pool DMA handle */
	if (ddi_dma_alloc_handle(ohcip->ohci_dip, &ohcip->ohci_dma_attr,
			DDI_DMA_SLEEP, 0,
			&ohcip->ohci_td_pool_dma_handle) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	/* Allocate the memory for the TD pool */
	if (ddi_dma_mem_alloc(ohcip->ohci_td_pool_dma_handle,
			td_pool_size * sizeof (gtd),
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			0,
			(caddr_t *)&ohcip->ohci_td_pool_addr,
			&real_length,
			&ohcip->ohci_td_pool_mem_handle)) {

		return (DDI_FAILURE);
	}

	/* Map the TD pool into the I/O address space */
	result = ddi_dma_addr_bind_handle(
			ohcip->ohci_td_pool_dma_handle,
			NULL,
			(caddr_t)ohcip->ohci_td_pool_addr,
			real_length,
			DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			NULL,
			&ohcip->ohci_td_pool_cookie,
			&ccount);

	bzero((void *)ohcip->ohci_td_pool_addr, td_pool_size * sizeof (gtd));

	/* Process the result */
	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
			    "ohci_allocate_pools: More than 1 cookie");

			return (DDI_FAILURE);
		}
	} else {
		USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_allocate_pools: Result = %d", result);

		ohci_decode_ddi_dma_addr_bind_handle_result(ohcip, result);

		return (DDI_FAILURE);
	}

	/*
	 * DMA addresses for TD pools are bound
	 */
	ohcip->ohci_dma_addr_bind_flag |= OHCI_TD_POOL_BOUND;

	/* Initialize the TD pool */
	for (i = 0; i < td_pool_size; i ++) {
		Set_TD(ohcip->ohci_td_pool_addr[i].hcgtd_ctrl, HC_TD_BLANK);
	}

	/* Allocate the TD pool DMA handle */
	if (ddi_dma_alloc_handle(ohcip->ohci_dip,
			&ohcip->ohci_dma_attr,
			DDI_DMA_SLEEP,
			0,
			&ohcip->ohci_ed_pool_dma_handle) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	/* Allocate the memory for the ED pool */
	if (ddi_dma_mem_alloc(ohcip->ohci_ed_pool_dma_handle,
			ed_pool_size * sizeof (hc_ed_t),
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			0,
			(caddr_t *)&ohcip->ohci_ed_pool_addr,
			&real_length,
			&ohcip->ohci_ed_pool_mem_handle) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	result = ddi_dma_addr_bind_handle(ohcip->ohci_ed_pool_dma_handle,
			NULL,
			(caddr_t)ohcip->ohci_ed_pool_addr,
			real_length,
			DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			DDI_DMA_SLEEP,
			NULL,
			&ohcip->ohci_ed_pool_cookie,
			&ccount);

	bzero((void *)ohcip->ohci_ed_pool_addr,
			ed_pool_size * sizeof (hc_ed_t));

	/* Process the result */
	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
			    "ohci_allocate_pools: More than 1 cookie");

			return (DDI_FAILURE);
		}
	} else {
		ohci_decode_ddi_dma_addr_bind_handle_result(ohcip, result);

		return (DDI_FAILURE);
	}

	/*
	 * DMA addresses for ED pools are bound
	 */
	ohcip->ohci_dma_addr_bind_flag |= OHCI_ED_POOL_BOUND;

	/* Initialize the ED pool */
	for (i = 0; i < ed_pool_size; i ++) {
		Set_ED(ohcip->ohci_ed_pool_addr[i].hced_ctrl, HC_EPT_BLANK);
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_allocate_pools: Completed");

	return (DDI_SUCCESS);
}


/*
 * ohci_decode_ddi_dma_addr_bind_handle_result:
 *
 * Process the return values of ddi_dma_addr_bind_handle()
 */
static void
ohci_decode_ddi_dma_addr_bind_handle_result(openhci_state_t *ohcip,
			int result)
{
	USB_DPRINTF_L2(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_decode_ddi_dma_addr_bind_handle_result:");

	switch (result) {
		case DDI_DMA_PARTIAL_MAP:
			USB_DPRINTF_L2(PRINT_MASK_ALL, ohcip->ohci_log_hdl,
			    "Partial transfers not allowed");
			break;
		case DDI_DMA_INUSE:
			USB_DPRINTF_L2(PRINT_MASK_ALL,	ohcip->ohci_log_hdl,
			    "Handle is in use");
			break;
		case DDI_DMA_NORESOURCES:
			USB_DPRINTF_L2(PRINT_MASK_ALL,	ohcip->ohci_log_hdl,
			    "No resources");
			break;
		case DDI_DMA_NOMAPPING:
			USB_DPRINTF_L2(PRINT_MASK_ALL,	ohcip->ohci_log_hdl,
			    "No mapping");
			break;
		case DDI_DMA_TOOBIG:
			USB_DPRINTF_L2(PRINT_MASK_ALL,	ohcip->ohci_log_hdl,
			    "Object is too big");
			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_ALL,	ohcip->ohci_log_hdl,
			    "Unknown dma error");
	}
}


/*
 * ohci_map_regs:
 *
 * The Host Controller (HC) contains a set of on-chip operational registers
 * and which should be mapped into a non-cacheable portion of the  system
 * addressable space.
 */
static int
ohci_map_regs(openhci_state_t *ohcip)
{
	ddi_device_acc_attr_t attr;
	uint16_t	command_reg;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl, "ohci_map_regs:");

	/* The host controller will be little endian */
	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags  = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/* Map in operational registers */
	if (ddi_regs_map_setup(ohcip->ohci_dip, 1,
			(caddr_t *)&ohcip->ohci_regsp,
			0,
			sizeof (hcr_regs_t),
			&attr,
			&ohcip->ohci_regs_handle) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	if (pci_config_setup(ohcip->ohci_dip,
		&ohcip->ohci_config_handle) != DDI_SUCCESS) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
			    "ohci_map_regs: Config error");

			return (DDI_FAILURE);
	}

#ifdef RIO
	/* For debugging purpose */
	if (ohci_target_abort_debug) {
		uint16_t pci_status;

		pci_status = pci_config_get16(ohcip->ohci_config_handle,
			PCI_CONF_STAT);

		if (pci_status & PCI_STAT_R_TARG_AB) {

			prom_printf("ohci_map_regs ohci%d: Target abort\n",
			    ddi_get_instance(ohcip->ohci_dip));

			debug_enter((char *)NULL);
		}
	}
#endif	/* RIO */

	/* Make sure Memory Access Enable and Master Enable are set */
	command_reg = pci_config_get16(ohcip->ohci_config_handle,
			PCI_CONF_COMM);

	if (!(command_reg & (PCI_COMM_MAE | PCI_COMM_ME))) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_map_regs: no MAE/ME");
	}

	command_reg |= PCI_COMM_MAE | PCI_COMM_ME;

	pci_config_put16(ohcip->ohci_config_handle,
			PCI_CONF_COMM, command_reg);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_map_regs: Completed");

	return (DDI_SUCCESS);
}


/*
 * ohci_register_intrs_and_init_mutex:
 *
 * Register interrupts and initialize each mutex and condition variables
 */
static int
ohci_register_intrs_and_init_mutex(openhci_state_t *ohcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_register_intrs_and_init_mutex:");

	/* Test for high level mutex */
	if (ddi_intr_hilevel(ohcip->ohci_dip, 0) != 0) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_register_intrs_and_init_mutex: "
		    "Hi level int not supported");

		return (DDI_FAILURE);
	}

	/*
	 * First call ddi_get_iblock_cookie() to retrieve the interrupt
	 * block cookie so that the mutexes may be initialized before
	 * adding the interrupt. If the mutexes are initialized after
	 * adding the interrupt, there could be a race condition.
	 */
	if (ddi_get_iblock_cookie(ohcip->ohci_dip, 0,
		&ohcip->ohci_iblk_cookie) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	/* Initialize the mutex */
	mutex_init(&ohcip->ohci_int_mutex, NULL, MUTEX_DRIVER,
				ohcip->ohci_iblk_cookie);

	if (ddi_add_intr(ohcip->ohci_dip, 0,
		&ohcip->ohci_iblk_cookie, NULL, ohci_intr,
				(caddr_t)ohcip) != DDI_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_reg_ints: ddi_add_intr failed");

		mutex_destroy(&ohcip->ohci_int_mutex);
		return (DDI_FAILURE);
	}

	/* Create prototype for SOF condition variable */
	cv_init(&ohcip->ohci_SOF_cv, NULL, CV_DRIVER, NULL);

	/* Create prototype for xfer completion condition variable */
	cv_init(&ohcip->ohci_xfer_cmpl_cv, NULL, CV_DRIVER, NULL);

	/* Semaphore to serialize opens and closes */
	sema_init(&ohcip->ohci_ocsem, 1, NULL, SEMA_DRIVER, NULL);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_register_intrs_and_init_mutex: Completed");

	return (DDI_SUCCESS);
}

/*
 * ohci_init_ctlr:
 *
 * Initialize the Host Controller (HC).
 */
static int
ohci_init_ctlr(openhci_state_t *ohcip, uchar_t flag)
{
	int revision;		/* Revision of Host Controller */
	int frameint;
	int curr_control;
	int max_packet = 0;
	int state;
#ifdef RIO
	hc_ed_t *dummy_ctrl_ept;
	hc_ed_t *dummy_bulk_ept;
#endif	/* RIO */

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl, "ohci_init_ctlr:");

	curr_control = Get_OpReg(hcr_control);
	state = curr_control & HCR_CONTROL_HCFS;

	/*
	 * The host controller should be reset to start off with a
	 * fresh chip state.
	 */
#ifdef RIO
	/*
	 * The RIO host controller can not go from the
	 * Suspend state to the Reset state directly.  So, if the controller
	 * is supended, it goes to resume first and then to reset.
	 */
	/* Reset the host controller if not suspended */
	if (state == HCR_CONTROL_SUSPD) {

		USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_init_ctlr: suspended (0x%x)", curr_control);

		/*
		 * Resume the controller.  Resuming the controller
		 * does not start up list processing nor does it
		 * cause access to the hcca area.
		 */
		curr_control = (curr_control & (~HCR_CONTROL_HCFS)) |
				HCR_CONTROL_RESUME;
		Set_OpReg(hcr_control, curr_control);

		drv_usecwait(OHCI_RESUME_TIMEWAIT);

		ASSERT((Get_OpReg(hcr_control) & HCR_CONTROL_HCFS)
				== HCR_CONTROL_RESUME);
	}
#endif	/* RIO */

	/* Reset the host controller */
	Set_OpReg(hcr_cmd_status, HCR_STATUS_RESET);

	/* Wait 10ms for reset to complete */
	drv_usecwait(OHCI_RESET_TIMEWAIT);

	/*
	 * According to Section 5.1.2.3 of the specification, the
	 * host controller will go into suspend state immediately
	 * after the reset.
	 */

	/* Verify the version number */
	revision = Get_OpReg(hcr_revision);

	if ((revision & HCR_REVISION_MASK) != HCR_REVISION_1_0) {

		return (DDI_FAILURE);
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_ctlr: Revision verified");

	/* hcca area need not be initialized on resume */
	if (flag == OHCI_CTLR_INIT) {

		/* Get the ohci chip vendor and device id */
		ohcip->ohci_vendor_id = pci_config_get16(
		    ohcip->ohci_config_handle, PCI_CONF_VENID);
		ohcip->ohci_device_id = pci_config_get16(
		    ohcip->ohci_config_handle, PCI_CONF_DEVID);

		/* Initialize the hcca area */
		if (ohci_init_hcca(ohcip) != DDI_SUCCESS) {

			return (DDI_FAILURE);
		}

		/* Take control of host controller */
		if (ohci_take_control(ohcip) != DDI_SUCCESS) {

			return (DDI_FAILURE);
		}
	}

	/* Set the HcHCCA to the physical address of the HCCA block */
	Set_OpReg(hcr_HCCA, (uint_t)ohcip->ohci_hcca_cookie.dmac_address);

#ifdef	RIO
	/* Enable RIO cache */
	if (ohci_rio_cache) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_init_ctlr: RIO CACHE IS ENABLED");
	} else {
		/* Disable RIO cache */
		Set_OpReg(hcr_rio_preftch_ctrl,
			(Get_OpReg(hcr_rio_preftch_ctrl) | HCR_RIO_CACHE));

		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_init_ctlr: RIO CACHE IS DISABLED");
	}

	/*
	 * RIO :
	 * Bit 4 of this register is a sim_mode bit, and should be set to 0
	 * (the value should be 7), or else the host controller uses a quicker
	 * than millisecond clock to time certain things (resume signalling is
	 * one of them)
	 */
	Set_OpReg(hcr_rio_core_ctrl, 0x07);

#endif	/* RIO */

	/*
	 * Set HcInterruptEnable to enable all interrupts except Root
	 * Hub Status change and SOF interrupts.
	 */
	Set_OpReg(hcr_intr_enable,
		HCR_INTR_SO | HCR_INTR_WDH | HCR_INTR_RD | HCR_INTR_UE |
		HCR_INTR_MIE);

#ifdef RIO
	/* Do not allocate dummy ed on resume */
	if (flag == OHCI_CTLR_INIT) {
		/*
		 * We need to insert dummy control and bulk endpoints during
		 * the host controller initialization since there is a known
		 * rio bug with the HC control and bulk list updation. Other
		 * wise we will see strange errors like ue, no sof problems.
		 */
		dummy_ctrl_ept = ohci_alloc_hc_ed(ohcip, NULL);

		/* Set the head ptr to the new endpoint */
		Set_OpReg(hcr_ctrl_head,
				EDpool_cpu_to_iommu(ohcip, dummy_ctrl_ept));

		dummy_bulk_ept = ohci_alloc_hc_ed(ohcip, NULL);

		/* Set the head ptr to the new endpoint */
		Set_OpReg(hcr_bulk_head,
				EDpool_cpu_to_iommu(ohcip, dummy_bulk_ept));
	}
#endif /* RIO */

	/* Start up both control and bulk lists processing */
	Set_OpReg(hcr_control, HCR_CONTROL_CLE | HCR_CONTROL_BLE);

	/*
	 * For non-periodic transfers, reserve at least for the one low
	 * speed device transaction and according to the USB Bandwidth
	 * Analysis white paper, it comes around 12% of USB frame time.
	 * Then periodic transfers will get 88% of USB frame time.
	 *
	 * Set HcPeriodicStart to a value that is 88% of the value in
	 * the FrameInterval field.
	 */
	frameint = HCR_FRME_INT_FI & Get_OpReg(hcr_frame_interval);
	Set_OpReg(hcr_periodic_strt, ((frameint * 88)/100));

	/*
	 * The LSThreshold filed of hcr_transfer_ls register  contains a
	 * value which is compared to the FrameRemaining field prior to
	 * initiating a Low Speed transaction and transaction is started
	 * only if FrameRemaining is greater than or equal to this field.
	 * One low-speed device transaction will take upto 12% of the USB
	 * frame time for the maximum of 8 bytes data packet size.
	 */
	Set_OpReg(hcr_transfer_ls, HCR_TRANS_LST & ((frameint * 12)/100));

	/*
	 * Initialize the FSLargestDataPacket value in the frame interval
	 * register. The controller compares the value of MaxPacketSize to
	 * this value to see if the entire packet may be sent out before
	 * the EOF.
	 */
	/* Save the contents of the Frame Interval Registers */
	ohcip->ohci_frame_interval = Get_OpReg(hcr_frame_interval);

	max_packet = (((frameint - MAX_OVERHEAD)*6)/7) << HCR_FRME_FSMPS_SHFT;
	Set_OpReg(hcr_frame_interval,
			(ohcip->ohci_frame_interval | max_packet));

	/* Begin sending SOFs */
	curr_control = Get_OpReg(hcr_control);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_ctlr: curr_control=0x%x", curr_control);

	/* Set the state to operational */
	curr_control = (curr_control &
			(~HCR_CONTROL_HCFS)) | HCR_CONTROL_OPERAT;

	Set_OpReg(hcr_control, curr_control);

	ASSERT((Get_OpReg(hcr_control) & HCR_CONTROL_HCFS)
			== HCR_CONTROL_OPERAT);

	/* Wait for the next SOF */
	if (ohci_wait_for_sof(ohcip) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_init_ctlr: No SOF's have started");

		return (DDI_FAILURE);
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_ctlr: SOF's have started");

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_ctlr: Completed");

	return (DDI_SUCCESS);
}


/*
 * ohci_init_hcca:
 *
 * Allocate the system memory and initialize Host Controller Communication
 * Area (HCCA). The HCCA structure must be aligned to a 256-byte boundary.
 */
static int
ohci_init_hcca(openhci_state_t *ohcip)
{
	ddi_device_acc_attr_t	dev_attr;
	size_t real_length;
	int result;
	uintptr_t addr;
	uint_t mask;
	uint_t ccount;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl, "ohci_init_hcca:");

	/* The host controller will be little endian */
	dev_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags  = DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/* Create space for the HCCA block */
	if (ddi_dma_alloc_handle(ohcip->ohci_dip, &ohcip->ohci_dma_attr,
				DDI_DMA_SLEEP,
				0,
				&ohcip->ohci_hcca_dma_handle)
				!= DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	if (ddi_dma_mem_alloc(ohcip->ohci_hcca_dma_handle,
				2 * sizeof (hcca_t),
				&dev_attr,
				DDI_DMA_CONSISTENT,
				DDI_DMA_SLEEP,
				0,
				(caddr_t *)&ohcip->ohci_hccap,
				&real_length,
				&ohcip->ohci_hcca_mem_handle)) {

		return (DDI_FAILURE);
	}

	bzero((void *)ohcip->ohci_hccap, real_length);

	/*
	 * DMA addresses for HCCA are bound
	 */
	ohcip->ohci_dma_addr_bind_flag |= OHCI_HCCA_DMA_BOUND;

	/* Figure out the alignment requirements */
	Set_OpReg(hcr_HCCA, 0xFFFFFFFF);

	/*
	 * Read the hcr_HCCA register until
	 * contenets are non-zero.
	 */
	mask = Get_OpReg(hcr_HCCA);

	while (mask == 0) {
		drv_usecwait(OHCI_TIMEWAIT);
		mask = Get_OpReg(hcr_HCCA);
	}

	ASSERT(mask != 0);

	addr = (uintptr_t)ohcip->ohci_hccap;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_hcca: addr=0x%p, mask=0x%x", addr, mask);

	while (addr & (~mask)) {
		addr++;
	}

	ohcip->ohci_hccap = (hcca_t *)addr;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_hcca: Real length %d", real_length);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_hcca: virtual hcca 0x%p", (void *)ohcip->ohci_hccap);

	/* Map the whole HCCA into the I/O address space */
	result = ddi_dma_addr_bind_handle(ohcip->ohci_hcca_dma_handle,
				NULL,
				(caddr_t)ohcip->ohci_hccap,
				real_length,
				DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
				DDI_DMA_SLEEP, NULL,
				&ohcip->ohci_hcca_cookie,
				&ccount);

	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
			    "ohci_init_hcca: More than 1 cookie");

			return (DDI_FAILURE);
		}
	} else {
		ohci_decode_ddi_dma_addr_bind_handle_result(ohcip, result);

		return (DDI_FAILURE);
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_hcca: physical 0x%p",
	    (void *)ohcip->ohci_hcca_cookie.dmac_address);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_hcca: size %d", ohcip->ohci_hcca_cookie.dmac_size);

	/* Initialize the interrupt lists */
	ohci_build_interrupt_lattice(ohcip);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_init_hcca: End");

	return (DDI_SUCCESS);
}


/*
 * ohci_build_interrupt_lattice:
 *
 * Construct the interrupt lattice tree using static Endpoint Descriptors
 * (ED). This interrupt lattice tree will have total of 32 interrupt  ED
 * lists and the Host Controller (HC) processes one interrupt ED list in
 * every frame. The lower five bits of the current frame number  indexes
 * into an array of 32 interrupt Endpoint Descriptor lists found  in the
 * HCCA.
 */
static void
ohci_build_interrupt_lattice(openhci_state_t *ohcip)
{
	int i;
	hcca_t *hccap = ohcip->ohci_hccap;
	hc_ed_t *list_array = ohcip->ohci_ed_pool_addr;
	int	half_list = NUM_INTR_ED_LISTS / 2;
	uintptr_t addr;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_build_interrupt_lattice:");

	/*
	 * Reserve the first 31 Endpoint Descriptor (ED) structures
	 * in the pool as static endpoints & these are required for
	 * constructing interrupt lattice tree.
	 */
	for (i = 0; i < NUM_STATIC_NODES; i++) {
		Set_ED(list_array[i].hced_ctrl, HC_EPT_sKip);
		Set_ED(list_array[i].hced_flag, HC_EPT_STATIC);
	}

	/* Build the interrupt lattice tree */
	for (i = 0; i < half_list - 1; i++) {

		/*
		 * The next  pointer in the host controller  endpoint
		 * descriptor must contain an iommu address. Calculate
		 * the offset into the cpu address and add this to the
		 * starting iommu address.
		 */
		addr = EDpool_cpu_to_iommu(ohcip,
						(hc_ed_t *)&list_array[i]);

		Set_ED(list_array[2*i + 1].hced_next, addr);
		Set_ED(list_array[2*i + 2].hced_next, addr);
	}

	/*
	 * Initialize the interrupt list in the HCCA so that it points
	 * to the bottom of the tree.
	 */
	for (i = 0; i < half_list; i++) {
		addr = EDpool_cpu_to_iommu(ohcip,
			(hc_ed_t *)&list_array[half_list - 1 + index[i]]);

		ASSERT(Get_ED(list_array[half_list - 1 + index[i]].hced_ctrl));

		ASSERT(addr != 0);

		Set_HCCA(hccap->HccaIntTble[i], addr);
		Set_HCCA(hccap->HccaIntTble[i + half_list], addr);
	}
}


/*
 * ohci_take_control:
 *
 * Take control of the host controller. OpenHCI allows for optional support
 * of legacy devices through the use of System Management Mode software and
 * system Management interrupt hardware. See section 5.1.1.3 of the OpenHCI
 * spec for more details.
 */
static int
ohci_take_control(openhci_state_t *ohcip)
{
	int ctrl = Get_OpReg(hcr_control);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_take_control: ctrl=%x", ctrl);

	/*
	 * On Sparc, there won't be  special System Management Mode
	 * hardware for legacy devices, while the x86 platforms may
	 * have to deal with  this. This  function may be  platform
	 * specific.
	 */

	/* The interrupt routing bit should not be set */
	if (ctrl & HCR_CONTROL_IR) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_take_control: Routing bit set");

		return (DDI_FAILURE);
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_take_control: End");

	return (DDI_SUCCESS);
}

/*
 * ohci_alloc_hcdi_ops:
 *
 * The HCDI interfaces or entry points are the software interfaces used by
 * the Universal Serial Bus Driver  (USBA) to  access the services of the
 * Host Controller Driver (HCD).  During HCD initialization, inform  USBA
 * about all available HCDI interfaces or entry points.
 */
static usb_hcdi_ops_t *
ohci_alloc_hcdi_ops(openhci_state_t *ohcip)
{
	usb_hcdi_ops_t	*usb_hcdi_ops;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_alloc_hcdi_ops:");

	usb_hcdi_ops = usba_alloc_hcdi_ops();

	usb_hcdi_ops->usb_hcdi_client_init = ohci_hcdi_client_init;
	usb_hcdi_ops->usb_hcdi_client_free = ohci_hcdi_client_free;

	usb_hcdi_ops->usb_hcdi_pipe_open = ohci_hcdi_pipe_open;
	usb_hcdi_ops->usb_hcdi_pipe_close = ohci_hcdi_pipe_close;

	usb_hcdi_ops->usb_hcdi_pipe_reset = ohci_hcdi_pipe_reset;
	usb_hcdi_ops->usb_hcdi_pipe_abort = ohci_hcdi_pipe_abort;

	usb_hcdi_ops->usb_hcdi_pipe_get_policy	=
					ohci_hcdi_pipe_get_policy;
	usb_hcdi_ops->usb_hcdi_pipe_set_policy	=
					ohci_hcdi_pipe_set_policy;

	usb_hcdi_ops->usb_hcdi_pipe_device_ctrl_receive =
					ohci_hcdi_pipe_device_ctrl_receive;
	usb_hcdi_ops->usb_hcdi_pipe_device_ctrl_send =
					ohci_hcdi_pipe_device_ctrl_send;

	usb_hcdi_ops->usb_hcdi_bulk_transfer_size =
					ohci_hcdi_bulk_transfer_size;
	usb_hcdi_ops->usb_hcdi_pipe_receive_bulk_data =
					ohci_hcdi_pipe_receive_bulk_data;
	usb_hcdi_ops->usb_hcdi_pipe_send_bulk_data =
					ohci_hcdi_pipe_send_bulk_data;

	usb_hcdi_ops->usb_hcdi_pipe_start_polling =
					ohci_hcdi_pipe_start_polling;
	usb_hcdi_ops->usb_hcdi_pipe_stop_polling =
					ohci_hcdi_pipe_stop_polling;

	usb_hcdi_ops->usb_hcdi_pipe_send_isoc_data =
					ohci_hcdi_pipe_send_isoc_data;

	usb_hcdi_ops->usb_hcdi_console_input_init =
					ohci_hcdi_polled_input_init;

	usb_hcdi_ops->usb_hcdi_console_input_enter =
					ohci_hcdi_polled_input_enter;

	usb_hcdi_ops->usb_hcdi_console_read = ohci_hcdi_polled_read;

	usb_hcdi_ops->usb_hcdi_console_input_exit =
					ohci_hcdi_polled_input_exit;

	usb_hcdi_ops->usb_hcdi_console_input_fini =
					ohci_hcdi_polled_input_fini;

	usb_hcdi_ops->hcdi_usba_private = NULL;

	if (usb_force_enable_pm) {
		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
			"PM FORCE ENABLED");
		usb_hcdi_ops->hcdi_pm_enable = 1;
	} else {
		/*
		 * First determine on what host controller you are working with
		 * If you have a RIO 1.0 host controller, disable PM
		 * For RIO 2.0 PM is enabled
		 */
		if (OHCI_RIO_REV1(ohcip)) {
			USB_DPRINTF_L1(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
				"RIO Chip : PM DISABLED");
			usb_hcdi_ops->hcdi_pm_enable = 0;
		} else {
			USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
				"PM ENABLED");
			usb_hcdi_ops->hcdi_pm_enable = 1;
		}

	}

	return (usb_hcdi_ops);
}


/*
 * ohci_init_root_hub:
 *
 * Initialize the root hub
 */
static void
ohci_init_root_hub(openhci_state_t *ohcip)
{
	uint_t port_state;
	int i;
	usb_hub_descr_t *root_hub_descr =
			&ohcip->ohci_root_hub.root_hub_descr;
	uint_t	des_A, des_B;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB,
	    ohcip->ohci_log_hdl, "ohci_init_root_hub:");

	/* Read the descriptor registers */
	des_A = ohcip->ohci_root_hub.root_hub_des_A =
					Get_OpReg(hcr_rh_descriptorA);
	des_B = ohcip->ohci_root_hub.root_hub_des_B =
					Get_OpReg(hcr_rh_descriptorB);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB,
	    ohcip->ohci_log_hdl, "root hub descriptor A 0x%x",
	    ohcip->ohci_root_hub.root_hub_des_A);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB,
	    ohcip->ohci_log_hdl, "root hub descriptor B 0x%x",
	    ohcip->ohci_root_hub.root_hub_des_B);

	/* Obtain the root hub status */
	ohcip->ohci_root_hub.root_hub_status = Get_OpReg(hcr_rh_status);

	/* Obtain the number of downstream ports */
	ohcip->ohci_root_hub.root_hub_num_ports =
			ohcip->ohci_root_hub.root_hub_des_A & HCR_RHA_NDP;

	ASSERT(ohcip->ohci_root_hub.root_hub_num_ports <= MAX_RH_PORTS);

	/*
	 * Build the hub descriptor based on HcRhDescriptorA and
	 * HcRhDescriptorB
	 */
	root_hub_descr->bDescLength = ROOT_HUB_DESCRIPTOR_LENGTH;
	root_hub_descr->bDescriptorType = ROOT_HUB_DESCRIPTOR_TYPE;
	root_hub_descr->bNbrPorts = des_A & HCR_RHA_NDP;

	/* Determine the Power Switching Mode */
	if (!(des_A & HCR_RHA_NPS)) {
		/*
		 * The ports are power switched. Check for either individual
		 * or gang power switching.
		 */
		if (des_A & HCR_RHA_PSM) {
			/* each port is powered individually */
			root_hub_descr->wHubCharacteristics =
					HUB_CHARS_INDIVIDUAL_PORT_POWER;

		} else {
			/* the ports are gang powered */
			root_hub_descr->wHubCharacteristics =
					HUB_CHARS_GANGED_POWER;
		}

		/* Each port will start off in the POWERED_OFF mode */
		port_state = POWERED_OFF;

	} else {
		/* The ports are powered when the ctlr is powered */
		root_hub_descr->wHubCharacteristics =
					HUB_CHARS_NO_POWER_SWITCHING;

		port_state = DISCONNECTED;
	}

	/* The root hub should never be a compound device */
	ASSERT((des_A & HCR_RHA_DT) == 0);

	/* Determine the Over-current Protection Mode */
	if (des_A & HCR_RHA_NOCP) {
		/* No over current protection */
		root_hub_descr->wHubCharacteristics |=
				HUB_CHARS_NO_OVER_CURRENT;
	} else {

		/* The OCPM bit should be the same as the PSM bit */
		ASSERT(((des_A & HCR_RHA_OCPM) && (des_A & HCR_RHA_PSM)) ||
			((des_A & HCR_RHA_OCPM) == (des_A & HCR_RHA_PSM)));

		/* See if over current protection is provided */
		if (des_A & HCR_RHA_OCPM) {
			/* reported on a per port basis */
			root_hub_descr->wHubCharacteristics |=
					HUB_CHARS_INDIV_OVER_CURRENT;
		}
	}

	/* Obtain the power on to power good time of the ports */
	ohcip->ohci_root_hub.root_hub_potpgt =
		(uint32_t)((ohcip->ohci_root_hub.root_hub_des_A & HCR_RHA_PTPGT)
				>> HCR_RHA_PTPGT_SHIFT) * 2;

	root_hub_descr->bPwrOn2PwrGood = (uint32_t)((des_A & HCR_RHA_PTPGT) >>
					HCR_RHA_PTPGT_SHIFT);

	USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB,
	    ohcip->ohci_log_hdl, "Power on to power good %d",
	    ohcip->ohci_root_hub.root_hub_potpgt);

	/* Indicate if the device is removable */
	root_hub_descr->DeviceRemovable = (uchar_t)des_B & HCR_RHB_DR;


	/*
	 * Fill in the port power control mask:
	 * Each bit in the  PortPowerControlMask
	 * should be set. Refer to USB 1.1, table 11-8
	 */
	root_hub_descr->PortPwrCtrlMask = (uchar_t)(des_B >> 16);

	/* Set the state of each port and initialize the status */
	for (i = 0; i < ohcip->ohci_root_hub.root_hub_num_ports; i++) {
		ohcip->ohci_root_hub.root_hub_port_state[i] = port_state;

		/* Turn off the power on each port for now */
		Set_OpReg(hcr_rh_portstatus[i],  HCR_PORT_CPP);

		/*
		 * Initialize each of the root hub port	status
		 * equal to zero. This initialization makes sure
		 * that all devices connected to root hub will
		 * enumerates when the first RHSC interrupt occurs
		 * since definitely there will be changes  in
		 * the root hub port status.
		 */
		ohcip->ohci_root_hub.root_hub_port_status[i] = 0;
	}
}


/*
 * ohci_load_root_hub_driver:
 *
 * Attach the root hub
 */
static usb_device_descr_t root_hub_device_descriptor = {
	0x12,	/* Length */
	1,	/* Type */
	1,	/* Bcd */
	9,	/* Class */
	0,	/* Sub class */
	0,	/* Protocol */
	8,	/* Max pkt size */
	0,	/* Vendor */
	0,	/* Product id */
	0,	/* Device release */
	0,	/* Manufacturer */
	0,	/* Product */
	0,	/* Sn */
	1	/* No of configs */
};

static uchar_t root_hub_config_descriptor[] = {
	0x9,	0x2,	0x19,	0x0,	0x1,	0x1,	0x0,	0x60,
	0x0,	0x9,	0x4,	0x0,	0x0,	0x1,	0x9,	0x1,
	0x0,	0x0,	0x7,	0x5,	0x81,	0x3,	0x1,	0x0,
	0x20};


static int
ohci_load_root_hub_driver(openhci_state_t *ohcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_load_root_hub_driver:");

	return (usba_hubdi_bind_root_hub(ohcip->ohci_dip,
		root_hub_config_descriptor,
		sizeof (root_hub_config_descriptor),
		&root_hub_device_descriptor));
}


/*
 * ohci_unload_root_hub_driver:
 */
static void
ohci_unload_root_hub_driver(openhci_state_t *ohcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_unload_root_hub_driver:");

	(void) usba_hubdi_unbind_root_hub(ohcip->ohci_dip);
}


/*
 * Host Controller Driver (HCD) deinitialization functions
 */

/*
 * ohci_cleanup:
 *
 * Cleanup on attach failure or detach
 */
static void
ohci_cleanup(openhci_state_t *ohcip)
{
	ohci_trans_wrapper_t *tw;
	ohci_pipe_private_t *pp;
	int flags = ohcip->ohci_flags;
	int i, ctrl;
	gtd *td;

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl, "ohci_cleanup:");

	if (flags & OHCI_INTR) {

		mutex_enter(&ohcip->ohci_int_mutex);

		/* Disable all HC ED list processing */
		Set_OpReg(hcr_control,
			(Get_OpReg(hcr_control) &
			~(HCR_CONTROL_CLE | HCR_CONTROL_BLE |
				HCR_CONTROL_PLE | HCR_CONTROL_IE)));

		/* Disable all HC interrupts */
		Set_OpReg(hcr_intr_disable, (HCR_INTR_SO |
			HCR_INTR_WDH | HCR_INTR_RD | HCR_INTR_UE |
			HCR_INTR_RHSC));

		/* Wait for the next SOF */
		(void) ohci_wait_for_sof(ohcip);

		/* Disable Master and SOF interrupts */
		Set_OpReg(hcr_intr_disable, (HCR_INTR_MIE | HCR_INTR_SOF));

		/* Set the Host Controller Functional State to Reset */
		Set_OpReg(hcr_control, ((Get_OpReg(hcr_control) &
				(~HCR_CONTROL_HCFS)) | HCR_CONTROL_RESET));

		/* Wait for sometime */
		drv_usecwait(OHCI_TIMEWAIT);

		/* Remove interrupt handler */
		ddi_remove_intr(ohcip->ohci_dip, 0, ohcip->ohci_iblk_cookie);

		mutex_exit(&ohcip->ohci_int_mutex);
	}

	if (flags & OHCI_RHREG) {
		/* Unload the root hub driver */
		ohci_unload_root_hub_driver(ohcip);
	}

	if (flags & OHCI_USBAREG) {
		/* Unregister this HCD instance with USBA */
		(void) usba_hcdi_deregister(ohcip->ohci_dip);
	}

	/* Unmap the OHCI registers */
	if (ohcip->ohci_regs_handle) {
		/* Reset the host controller */
		Set_OpReg(hcr_cmd_status, HCR_STATUS_RESET);

		ddi_regs_map_free(&ohcip->ohci_regs_handle);
	}

	if (ohcip->ohci_config_handle) {
		pci_config_teardown(&ohcip->ohci_config_handle);
	}

	/* Free all the buffers */
	if (ohcip->ohci_td_pool_addr && ohcip->ohci_td_pool_mem_handle) {
		for (i = 0; i < td_pool_size; i ++) {
			td = &ohcip->ohci_td_pool_addr[i];
			ctrl = Get_TD(ohcip->ohci_td_pool_addr[i].hcgtd_ctrl);

			if ((ctrl != HC_TD_BLANK) && (ctrl != HC_TD_DUMMY) &&
				(td->hcgtd_trans_wrapper != NULL)) {

				mutex_enter(&ohcip->ohci_int_mutex);

				tw = (ohci_trans_wrapper_t *)
					OHCI_LOOKUP_ID((uint32_t)
					Get_TD(td->hcgtd_trans_wrapper));

				/* Obtain the pipe private structure */
				pp = tw->tw_pipe_private;

				mutex_enter(&pp->pp_mutex);

				/* Stop the the transfer timer */
				ohci_stop_xfer_timer(ohcip, tw,
						OHCI_REMOVE_XFER_ALWAYS);

				mutex_exit(&pp->pp_mutex);

				ohci_free_tw(ohcip, tw);

				mutex_exit(&ohcip->ohci_int_mutex);
			}
		}

		/*
		 * If OHCI_TD_POOL_BOUND flag is set, then
		 * unbind the handle for TD pools
		 */
		if ((ohcip->ohci_dma_addr_bind_flag & OHCI_TD_POOL_BOUND) ==
			OHCI_TD_POOL_BOUND) {
			int rval;

			rval = ddi_dma_unbind_handle(
					ohcip->ohci_td_pool_dma_handle);
			ASSERT(rval == DDI_SUCCESS);
		}
		ddi_dma_mem_free(&ohcip->ohci_td_pool_mem_handle);
	}

	/* Free the TD pool */
	if (ohcip->ohci_td_pool_mem_handle) {
		ddi_dma_free_handle(&ohcip->ohci_td_pool_mem_handle);
	}

	if (ohcip->ohci_ed_pool_addr && ohcip->ohci_ed_pool_mem_handle) {
		/*
		 * If OHCI_ED_POOL_BOUND flag is set, then
		 * unbind the handle for ED pools
		 */
		if ((ohcip->ohci_dma_addr_bind_flag & OHCI_ED_POOL_BOUND) ==
			OHCI_ED_POOL_BOUND) {
			int rval;

			rval = ddi_dma_unbind_handle(
					ohcip->ohci_ed_pool_dma_handle);
			ASSERT(rval == DDI_SUCCESS);
		}

		ddi_dma_mem_free(&ohcip->ohci_ed_pool_mem_handle);
	}

	/* Free the ED pool */
	if (ohcip->ohci_ed_pool_dma_handle) {
		ddi_dma_free_handle(&ohcip->ohci_ed_pool_dma_handle);
	}

	/* Free the HCCA area */
	if (ohcip->ohci_hccap && ohcip->ohci_hcca_mem_handle) {
		/*
		 * If OHCI_HCCA_DMA_BOUND flag is set, then
		 * unbind the handle for HCCA
		 */
		if ((ohcip->ohci_dma_addr_bind_flag & OHCI_HCCA_DMA_BOUND) ==
			OHCI_HCCA_DMA_BOUND) {
			int rval;

			rval = ddi_dma_unbind_handle(
					ohcip->ohci_hcca_dma_handle);
			ASSERT(rval == DDI_SUCCESS);
		}

		ddi_dma_mem_free(&ohcip->ohci_hcca_mem_handle);
	}
	if (ohcip->ohci_hcca_dma_handle) {
		ddi_dma_free_handle(&ohcip->ohci_hcca_dma_handle);
	}

	if (flags & OHCI_LOCKS) {

		/* Destroy the mutex */
		mutex_destroy(&ohcip->ohci_int_mutex);

		/* Destroy the SOF condition varibale */
		cv_destroy(&ohcip->ohci_SOF_cv);

		/* Destroy the xfer completion condition varibale */
		cv_destroy(&ohcip->ohci_xfer_cmpl_cv);

		/* Destroy the serialize opens and closes semaphore */
		sema_destroy(&ohcip->ohci_ocsem);
	}

	/* clean up kstat structs */
	ohci_destroy_stats(ohcip);

#ifdef	DEBUG
	mutex_enter(&ohci_dump_mutex);
	if (ohcip->ohci_dump_ops) {
		usba_dump_deregister(ohcip->ohci_dump_ops);
		usba_free_dump_ops(ohcip->ohci_dump_ops);
	}
	mutex_exit(&ohci_dump_mutex);
#endif	/* DEBUG */

	if (flags & OHCI_ZALLOC) {

		usb_free_log_handle(ohcip->ohci_log_hdl);

		/* Remove all properties that might have been created */
		ddi_prop_remove_all(ohcip->ohci_dip);

		/* Free the soft state */
		ddi_soft_state_free(ohci_statep,
			ddi_get_instance(ohcip->ohci_dip));
	}
}

/*
 * ohci_destroy_stats:
 *
 * Clean up ohci kstat structures
 */
void
ohci_destroy_stats(openhci_state_t *ohcip)
{
	int i;

	if (OHCI_INTRS_STATS(ohcip)) {
		kstat_delete(OHCI_INTRS_STATS(ohcip));
		OHCI_INTRS_STATS(ohcip) = NULL;
	}

	if (OHCI_TOTAL_STATS(ohcip)) {
		kstat_delete(OHCI_TOTAL_STATS(ohcip));
		OHCI_TOTAL_STATS(ohcip) = NULL;
	}

	for (i = 0; i < USB_N_COUNT_KSTATS; i++) {
		if (ohcip->ohci_count_stats[i]) {
			kstat_delete(ohcip->ohci_count_stats[i]);
			ohcip->ohci_count_stats[i] = NULL;
		}
	}
}

/*
 * ohci_cpr_suspend
 */
static int
ohci_cpr_suspend(openhci_state_t *ohcip)
{
	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_cpr_suspend:");

	/* Only root hub's intr pipe should be open at this time */
	mutex_enter(&ohcip->ohci_int_mutex);

	if (ohcip->ohci_open_pipe_count > 1) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_cpr_suspend: fails as open pipe count = %d",
		    ohcip->ohci_open_pipe_count);

		mutex_exit(&ohcip->ohci_int_mutex);
		return (DDI_FAILURE);
	}

	ohcip->ohci_flags |= OHCI_CPR_SUSPEND;

	mutex_exit(&ohcip->ohci_int_mutex);

	/* Call into the root hub and suspend it */
	if (usba_hubdi_detach(ohcip->ohci_dip, DDI_SUSPEND) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	mutex_enter(&ohcip->ohci_int_mutex);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_cpr_suspend: Disable HC ED list processing");

	/* Disable all HC ED list processing */
	Set_OpReg(hcr_control,
		(Get_OpReg(hcr_control) &
		~(HCR_CONTROL_CLE | HCR_CONTROL_BLE |
			HCR_CONTROL_PLE | HCR_CONTROL_IE)));

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_cpr_suspend: Disable HC interrupts");

	/* Disable all HC interrupts */
	Set_OpReg(hcr_intr_disable, (HCR_INTR_SO |
		HCR_INTR_WDH | HCR_INTR_RD | HCR_INTR_UE | HCR_INTR_RHSC));

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_cpr_suspend: Wait for the next SOF");

	/* Wait for the next SOF */
	if (ohci_wait_for_sof(ohcip) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_cpr_suspend: No SOF's have started");

		mutex_exit(&ohcip->ohci_int_mutex);
		return (DDI_FAILURE);
	}

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_cpr_suspend: Disable Master interrupt");

	/*
	 * Disable Master interrupt so that ohci driver don't
	 * get any ohci interrupts.
	 */
	Set_OpReg(hcr_intr_disable, HCR_INTR_MIE);

	mutex_exit(&ohcip->ohci_int_mutex);
	return (DDI_SUCCESS);
}

/*
 * ohci_cpr_resume
 */
static int
ohci_cpr_resume(openhci_state_t *ohcip)
{
	mutex_enter(&ohcip->ohci_int_mutex);

	USB_DPRINTF_L4(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "ohci_cpr_resume: Restart the controller");

	/* Restart the controller */
	if (ohci_init_ctlr(ohcip, OHCI_CTLR_RESTART) != DDI_SUCCESS) {

		USB_DPRINTF_L1(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
		    "ohci_cpr_resume: Restart the controller - FAILED ");

		mutex_exit(&ohcip->ohci_int_mutex);
		return (DDI_FAILURE);
	}

	mutex_exit(&ohcip->ohci_int_mutex);

	/* Now resume the root hub */
	if (usba_hubdi_attach(ohcip->ohci_dip, DDI_RESUME) != DDI_SUCCESS) {

		return (DDI_FAILURE);
	}

	mutex_enter(&ohcip->ohci_int_mutex);
	ohcip->ohci_flags &= ~OHCI_CPR_SUSPEND;
	mutex_exit(&ohcip->ohci_int_mutex);

	return (DDI_SUCCESS);
}


/*
 * HCDI entry points
 *
 * The Host Controller Driver Interfaces (HCDI) are the software interfaces
 * between the Universal Serial Bus Layer (USBA) and the Host Controller
 * Driver (HCD). The HCDI interfaces or entry points are subject to change.
 */


/*
 * ohci_hcdi_client_init:
 *
 * Member of the HCD ops structure and called at INITCHILD time.  Allocate
 * HCD resources for the device.
 *
 * ohci_hcdi_client_init() is called right before the
 * attach routine of the child driver is called.  The
 * default pipe of the	device has been read and  the
 * device has a  configuration as well as an address.
 * The device fits within the power budget.
 */
static int
ohci_hcdi_client_init(usb_device_t *usb_device)
{
	usb_dev_t	*usb_dev = kmem_zalloc(sizeof (usb_dev_t), KM_SLEEP);
	openhci_state_t *ohcip;

	ohcip = ohci_obtain_state(usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_client_init:");

	mutex_init(&usb_dev->usb_dev_mutex, NULL, MUTEX_DRIVER,
				ohcip->ohci_iblk_cookie);

	mutex_enter(&usb_dev->usb_dev_mutex);
	usb_dev->usb_dev_device_impl = usb_device;
	usb_dev->usb_dev_pipe_list = NULL;
	mutex_exit(&usb_dev->usb_dev_mutex);

	/* Save the HCD's device specific structure */
	mutex_enter(&usb_device->usb_mutex);
	usb_device->usb_hcd_private = usb_dev;
	mutex_exit(&usb_device->usb_mutex);

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_client_free:
 *
 * Member of the HCD ops structure and called at UNINITCHILD time. Deallocate
 * HCD resouces for the device.
 *
 * The Client driver should have called usb_pipe_close()
 * on each pipe it has open when detaching
 * ohci will check for this condition before releasing the client
 * resources & it will return error if corresponding client's
 * pipes are not closed.
 */
static int
ohci_hcdi_client_free(usb_device_t *usb_device)
{

	usb_dev_t *usb_dev = (usb_dev_t *)usb_device->usb_hcd_private;

	if (usb_dev == NULL) {

		return (USB_SUCCESS);
	}

	mutex_enter(&usb_dev->usb_dev_mutex);

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
 * ohci_hcdi_pipe_open:
 *
 * Member of HCD Ops structure and called during client specific pipe open
 * Add the pipe to the data structure representing the device and allocate
 * bandwidth for the pipe if it is a interrupt or isochronous endpoint.
 */
static int
ohci_hcdi_pipe_open(usb_pipe_handle_impl_t *pipe_handle, uint_t flags)
{
	openhci_state_t *ohcip;
	usb_dev_t *usb_dev;
	usb_endpoint_descr_t *endpoint_descr;
	uint_t node = 0;
	ohci_pipe_private_t *pp;
	int kmflag = (flags & USB_FLAGS_SLEEP) ? KM_SLEEP : KM_NOSLEEP;
	int error = USB_SUCCESS;

	ASSERT(pipe_handle);

	ohcip = ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	endpoint_descr = pipe_handle->p_endpoint;
	usb_dev = (usb_dev_t *)pipe_handle->p_usb_device->usb_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_open: addr = 0x%x, ep%d",
	    pipe_handle->p_usb_device->usb_addr,
	    endpoint_descr->bEndpointAddress & USB_EPT_ADDR_MASK);

	sema_p(&ohcip->ohci_ocsem);

	/*
	 * Check and handle root hub pipe open.
	 */
	if (pipe_handle->p_usb_device->usb_addr == ROOT_HUB_ADDR) {

		mutex_enter(&ohcip->ohci_int_mutex);
		error = ohci_handle_root_hub_pipe_open(pipe_handle,
							flags);
		mutex_exit(&ohcip->ohci_int_mutex);
		sema_v(&ohcip->ohci_ocsem);

		return (error);
	}

	/*
	 * Opening of other pipes excluding  root hub pipe are
	 * handled below. Check whether pipe is already opened.
	 */
	if (pipe_handle->p_hcd_private != NULL) {
		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_open: Pipe is already opened");

		sema_v(&ohcip->ohci_ocsem);

		return (USB_FAILURE);
	}

	/*
	 * A portion of the bandwidth is reserved for the nonperdioc
	 * transfers  i.e control and bulk transfers in each  of one
	 * millisecond frame period & usually it will be 10% of frame
	 * period. Hence there is no need to check for the available
	 * bandwidth before adding the control or bulk endpoints.
	 *
	 * There is a need to check for the available bandwidth before
	 * adding the periodic transfers i.e interrupt & isochronous
	 * since all these periodic transfers are guaranteed transfers.
	 * Usually 90% of the total frame time is reserved for periodic
	 * transfers.
	 */
	if (((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_ISOCH) ||
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_INTR)) {

		mutex_enter(&ohcip->ohci_int_mutex);
		mutex_enter(&pipe_handle->p_mutex);

		if ((node = ohci_allocate_bandwidth(ohcip, pipe_handle)) ==
		    USB_FAILURE) {
			USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_hcdi_pipe_open: "
			    "Bandwidth allocation failure");

			mutex_exit(&pipe_handle->p_mutex);
			mutex_exit(&ohcip->ohci_int_mutex);
			sema_v(&ohcip->ohci_ocsem);

			return (USB_NO_BANDWIDTH);
		}

		mutex_exit(&pipe_handle->p_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);
	}

	/* Create the HCD pipe private structure */
	pp = (ohci_pipe_private_t *)
		kmem_zalloc(sizeof (ohci_pipe_private_t), kmflag);

	/*
	 * There will be a mutex lock per pipe. This
	 * will serialize the pipe's transactions.
	 */
	mutex_init(&pp->pp_mutex, NULL, MUTEX_DRIVER,
			ohcip->ohci_iblk_cookie);

	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Store the node in the interrupt lattice */
	pp->pp_node = node;

	/* Set the state of pipe as OPENED */
	pp->pp_state = OPENED;

	/* Store a pointer to the pipe handle */
	pp->pp_pipe_handle = pipe_handle;

	mutex_enter(&pipe_handle->p_mutex);

	/* Store the pointer in the pipe handle */
	pipe_handle->p_hcd_private = (usb_opaque_t)pp;

	/* Store a copy of the pipe policy */
	bcopy(pipe_handle->p_policy, &pp->pp_policy,
			sizeof (usb_pipe_policy_t));

	mutex_exit(&pipe_handle->p_mutex);

	/* Allocate the host controller endpoint descriptor */
	pp->pp_ept = ohci_alloc_hc_ed(ohcip, pipe_handle);

	if (pp->pp_ept == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_open: ED allocation failed");

		mutex_exit(&pp->pp_mutex);

		mutex_enter(&pipe_handle->p_mutex);

		/* Destroy the pipe mutex */
		mutex_destroy(&pp->pp_mutex);

		/*
		 * Deallocate the hcd private portion
		 * of the pipe handle.
		 */
		kmem_free(pipe_handle->p_hcd_private,
				sizeof (ohci_pipe_private_t));

		/*
		 * Set the private structure in the
		 * pipe handle equal to NULL.
		 */
		pipe_handle->p_hcd_private = NULL;
		mutex_exit(&pipe_handle->p_mutex);

		mutex_exit(&ohcip->ohci_int_mutex);
		sema_v(&ohcip->ohci_ocsem);

		return (USB_NO_RESOURCES);
	}

	/*
	 * Insert the endpoint onto the host controller's
	 * appropriate endpoint list. The host controller
	 * will not schedule this endpoint and will not have
	 * any TD's to process.
	 */
	ohci_insert_ed(ohcip, pipe_handle);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_open: ph = 0x%p", (void *)pipe_handle);

	/*
	 * Insert this pipe at the head of the list
	 * of pipes for this device only if its addrs != 0
	 * usb_dev field is initialized in the usb_hcdi_client_init entry
	 * point, which has not been called yet for this device.
	 * The ohci_hcdi_client_init function is only called once
	 * the client has an address and is configured.
	 */
	if (usb_dev) {
		mutex_enter(&usb_dev->usb_dev_mutex);
		pp->pp_next = usb_dev->usb_dev_pipe_list;
		usb_dev->usb_dev_pipe_list = pp;
		mutex_exit(&usb_dev->usb_dev_mutex);
	}

	mutex_exit(&pp->pp_mutex);

	ohcip->ohci_open_pipe_count++;

	mutex_exit(&ohcip->ohci_int_mutex);

	sema_v(&ohcip->ohci_ocsem);

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_pipe_close:
 *
 * Member of HCD Ops structure and called during the client  specific pipe
 * close. Remove the pipe and the data structure representing the device.
 * Deallocate  bandwidth for the pipe if it is a interrupt or isochronous
 * endpoint.
 */
static int
ohci_hcdi_pipe_close(usb_pipe_handle_impl_t  *pipe_handle)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	usb_dev_t *usb_dev = (usb_dev_t *)
			pipe_handle->p_usb_device->usb_hcd_private;
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;
	ohci_pipe_private_t *pipe;
	int	error = USB_SUCCESS;
	uint_t	bit = 0;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_close: addr = 0x%x, ep%d",
	    pipe_handle->p_usb_device->usb_addr,
	    endpoint_descr->bEndpointAddress & USB_EPT_ADDR_MASK);

	sema_p(&ohcip->ohci_ocsem);

	/*
	 * Check and handle root hub pipe close.
	 */
	if (pipe_handle->p_usb_device->usb_addr == ROOT_HUB_ADDR) {

		mutex_enter(&ohcip->ohci_int_mutex);
		error = ohci_handle_root_hub_pipe_close(pipe_handle);
		mutex_exit(&ohcip->ohci_int_mutex);
		sema_v(&ohcip->ohci_ocsem);

		return (error);
	}

	/* All transactions have been stopped */
	mutex_enter(&ohcip->ohci_int_mutex);

	/*
	 * Acquire the pipe mutex so that no other pipe
	 * requests are made on this pipe.
	 */
	mutex_enter(&pp->pp_mutex);

	/*
	 * Before removing the control and bulk endpoints from the
	 * Host Controller's (HC) appropriate endpoint list,  stop
	 * the corresponding endpoint list processing and wait for
	 * the next start of frame interrupt(SOF).
	 *
	 * For the periodic endpoints i.e interrupt and isochronous
	 * endpoints just set the skip bit in the ED  and wait for
	 * the next start of frame  interrupt to ensure that HC is
	 * no longer accessing that endpoint.
	 */
	switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_CONTROL:
			bit = HCR_CONTROL_CLE;
			/* FALLTHROUGH */
		case USB_EPT_ATTR_BULK:
			if (!bit) {
				bit = HCR_CONTROL_BLE;
			}

			Set_OpReg(hcr_control,
				(Get_OpReg(hcr_control) & ~(bit)));

			mutex_exit(&pp->pp_mutex);

			/* Wait for the next SOF */
			if (ohci_wait_for_sof(ohcip) != USB_SUCCESS) {

				USB_DPRINTF_L2(PRINT_MASK_HCDI,
				    ohcip->ohci_log_hdl,
				    "ohci_hcdi_pipe_close:"
				    "No SOF's have started");

				mutex_exit(&ohcip->ohci_int_mutex);
				sema_v(&ohcip->ohci_ocsem);
				return (USB_FAILURE);
			}

			mutex_enter(&pp->pp_mutex);
			break;

		default:
			/*
			 * Set the sKip bit to stop all transactions on
			 * this pipe
			 */
			if (ohci_modify_sKip_bit(ohcip, pp,
					SET_sKip) != USB_SUCCESS) {

				USB_DPRINTF_L2(PRINT_MASK_HCDI,
				    ohcip->ohci_log_hdl,
				    "ohci_hcdi_pipe_close:"
				    "No SOF's have started");

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&ohcip->ohci_int_mutex);
				sema_v(&ohcip->ohci_ocsem);
				return (USB_FAILURE);
			}

			/*
			 * Wait for processing all completed transfers and
			 * to send results to upstream.
			 */
			if (ohci_wait_for_transfers_completion(ohcip,
						pp) != USB_SUCCESS) {

				USB_DPRINTF_L2(PRINT_MASK_HCDI,
				    ohcip->ohci_log_hdl,
				    "ohci_hcdi_pipe_close: Not received"
				    "transfers completion pp = 0x%p", pp);

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&ohcip->ohci_int_mutex);
				sema_v(&ohcip->ohci_ocsem);
				return (USB_FAILURE);
			}
	}

	/*
	 * Traverse the list of TD's on this endpoint and
	 * these TD's have outstanding transfer requests.
	 * Since the list processing is stopped, these tds
	 * can be deallocated.
	 */
	ohci_traverse_tds(ohcip, pipe_handle);

	/*
	 * If all of the endpoint's TD's have been deallocated,
	 * then the DMA mappings can be torn down. If not there
	 * are some TD's on the  done list that have not been
	 * processed. Tag  these TD's  so that they are thrown
	 * away when the done list is processed.
	 */
	ohci_done_list_tds(ohcip, pipe_handle);

	/* Free DMA resources */
	ohci_free_dma_resources(ohcip, pipe_handle);

	ASSERT(endpoint_descr != NULL);

	/*
	 * Remove the endoint descriptor from Host Controller's
	 * appropriate endpoint list.
	*/
	ohci_remove_ed(ohcip, pp);

	/* Deallocate bandwidth */
	if (((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_ISOCH) ||
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_INTR)) {

		mutex_enter(&pipe_handle->p_mutex);
		ohci_deallocate_bandwidth(ohcip, pipe_handle);
		mutex_exit(&pipe_handle->p_mutex);
	}

	/*
	 * Closing of other pipes excluding root hub pipe
	 * are handled below.
	 *
	 * Remove this pipe from the list of pipes for the
	 * device.
	 */
	if (usb_dev != NULL) {
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
		mutex_exit(&usb_dev->usb_dev_mutex);
	}

	/*
	 * Destroy the pipe's mutex.
	 */
	mutex_exit(&pp->pp_mutex);
	mutex_destroy(&pp->pp_mutex);

	mutex_enter(&pipe_handle->p_mutex);

	/*
	 * Deallocate the hcd private portion
	 * of the pipe handle.
	 */
	kmem_free(pipe_handle->p_hcd_private,
			    sizeof (ohci_pipe_private_t));
	pipe_handle->p_hcd_private = NULL;

	mutex_exit(&pipe_handle->p_mutex);


	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_close: ph = 0x%p", (void *)pipe_handle);

	ohcip->ohci_open_pipe_count--;
	mutex_exit(&ohcip->ohci_int_mutex);
	sema_v(&ohcip->ohci_ocsem);

	return (error);
}


/*
 * ohci_hcdi_pipe_reset:
 */
/* ARGSUSED */
static int
ohci_hcdi_pipe_reset(usb_pipe_handle_impl_t *pipe_handle, uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	ohci_pipe_private_t *pp =
		(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;
	int	error = USB_SUCCESS;
	hc_ed_t *ept;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_reset:");

	/*
	 * Check and handle root hub pipe reset.
	 */
	if (pipe_handle->p_usb_device->usb_addr == ROOT_HUB_ADDR) {

		error = ohci_handle_root_hub_pipe_reset(pipe_handle,
							usb_flags);
		return (error);
	}

	/*
	 * Pipe reset for the default and other pipes
	 * are handled below except  for the root hub
	 * pipes.
	 */
	mutex_enter(&ohcip->ohci_int_mutex);

	/*
	 * Acquire the pipe mutex so that no other pipe
	 * requests are made on this pipe.
	 */
	mutex_enter(&pp->pp_mutex);

	ept = pp->pp_ept;

	/*
	 * Set the sKip bit to stop all transactions on
	 * this pipe
	 */
	if (ohci_modify_sKip_bit(ohcip, pp, SET_sKip) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_reset: No SOF's have started");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle, usb_flags,
			0, 0, USB_CC_UNSPECIFIED_ERR, USB_FAILURE);

		return (USB_FAILURE);
	}

	if (((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_ISOCH) ||
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_INTR)) {

		/*
		 * Wait for processing all completed transfers and to send
		 * results to upstream.
		 */
		if (ohci_wait_for_transfers_completion(ohcip,
					pp) != USB_SUCCESS) {

			USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_hcdi_pipe_reset: Not received transfers"
			    "completion pp = 0x%p", pp);

			mutex_exit(&pp->pp_mutex);
			mutex_exit(&ohcip->ohci_int_mutex);

			usba_hcdi_callback(pipe_handle, usb_flags,
				0, 0, USB_CC_UNSPECIFIED_ERR, USB_FAILURE);

			return (USB_FAILURE);
		}
	}

	/*
	 * Traverse the list of TD's on this endpoint and
	 * these TD's have outstanding transfer requests.
	 * Since the list processing is stopped, these tds
	 * can be deallocated.
	 */
	ohci_traverse_tds(ohcip, pipe_handle);

	/*
	 * If all of the endpoint's TD's have been deallocated,
	 * then the DMA mappings can be torn down. If not there
	 * are some TD's on the  done list that have not been
	 * processed. Tag these TD's  so that they are thrown
	 * away when the done list is processed.
	 */
	ohci_done_list_tds(ohcip, pipe_handle);

	/* Free DMA resources */
	ohci_free_dma_resources(ohcip, pipe_handle);

	/* Reset the data toggle bit to 0 */
	Set_ED(ept->hced_headp, (Get_ED(ept->hced_headp) &
						(~(HC_EPT_Carry))));

	/* The endpoint is completely clean.  It only has a dummy td on it */
	ASSERT((Get_ED(ept->hced_headp) != NULL) &&
		    (Get_ED(ept->hced_headp) == Get_ED(ept->hced_tailp)));

	ASSERT((pp->pp_tw_head == NULL) && (pp->pp_tw_tail == NULL));

	/*
	 * Clear the sKip bit to restart all the
	 * transactions on this pipe.
	 */
	(void) ohci_modify_sKip_bit(ohcip, pp, CLEAR_sKip);

	/*
	 * Since the endpoint is stripped of Transfer
	 * Descriptors (TD),  reset the state of the
	 * periodic pipe to OPENED.
	 */
	pp->pp_state = OPENED;

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle,
		usb_flags, 0, 0, USB_CC_NOERROR, USB_SUCCESS);

	return (error);
}


/*
 * ohci_hcdi_pipe_abort:
 *
 * NOTE:
 *	This function is not implemented completely.
 */
/* ARGSUSED */
static int
ohci_hcdi_pipe_abort(usb_pipe_handle_impl_t *pipe_handle, uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_abort:");

	usba_hcdi_callback(pipe_handle,
	    usb_flags, 0, 0, USB_CC_UNSPECIFIED_ERR, USB_SUCCESS);

	return (USB_FAILURE);
}


/*
 * ohci_hcdi_pipe_get_policy:
 */
static int
ohci_hcdi_pipe_get_policy(
		usb_pipe_handle_impl_t	*pipe_handle,
		usb_pipe_policy_t *policy)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	ohci_pipe_private_t *pp =
		(ohci_pipe_private_t *)pipe_handle->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_get_policy:");

	/*
	 * Make sure there are no transactions
	 * on this pipe.
	 */
	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Copy the pipe policy information to clients memory space */
	bcopy(&pp->pp_policy, policy, sizeof (usb_pipe_policy_t));

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_pipe_set_policy:
 */
static int
ohci_hcdi_pipe_set_policy(
		usb_pipe_handle_impl_t	*pipe_handle,
		usb_pipe_policy_t *policy,
		uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	ohci_pipe_private_t *pp =
		(ohci_pipe_private_t *)pipe_handle->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_set_policy: flags = 0x%x", usb_flags);

	/*
	 * Make sure no other part of the driver
	 * is trying to access this  part of the
	 * pipe handle.
	 */
	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/* Copy the new policy into the pipe handle */
	bcopy(policy, &pp->pp_policy, sizeof (usb_pipe_policy_t));

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_pipe_device_ctrl_receive:
 */
static int
ohci_hcdi_pipe_device_ctrl_receive(usb_pipe_handle_impl_t  *pipe_handle,
				uchar_t		bmRequestType,
				uchar_t		bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_device_ctrl_receive: 0x%x 0x%x 0x%x 0x%x 0x%x",
	    bmRequestType, bRequest, wValue, wIndex, wLength);

	return (ohci_common_ctrl_routine(pipe_handle,
				bmRequestType,
				bRequest,
				wValue,
				wIndex,
				wLength,
				NULL, usb_flags));
}


/*
 * ohci_hcdi_pipe_device_ctrl_send:
 */
static int
ohci_hcdi_pipe_device_ctrl_send(usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t		bmRequestType,
				uchar_t		bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t *data,
				uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_device_ctrl_send: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%p",
	    bmRequestType, bRequest, wValue, wIndex, wLength,
	    (void *)data);

	return (ohci_common_ctrl_routine(pipe_handle,
					bmRequestType,
					bRequest,
					wValue,
					wIndex,
					wLength,
					data, usb_flags));
}


/*
 * ohci_hcdi_bulk_transfer_size:
 *
 * Return maximum bulk transfer size
 */

/* ARGSUSED */
static int
ohci_hcdi_bulk_transfer_size(dev_info_t *dip, size_t  *size)
{
	USB_DPRINTF_L4(PRINT_MASK_HCDI, NULL,
	    "ohci_hcdi_bulk_transfer_size:");

	*size = ohci_bulk_transfer_size;

	return (USB_SUCCESS);
}


/*
 * ohci_hcdi_pipe_receive_bulk_data:
 */
static int
ohci_hcdi_pipe_receive_bulk_data(usb_pipe_handle_impl_t	*pipe_handle,
		size_t length,
		uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_receive_bulk_data:");

	return (ohci_common_bulk_routine(pipe_handle,
					length,
					NULL,
					usb_flags));
}


/*
 * ohci_hcdi_pipe_send_bulk_data:
 */
static int
ohci_hcdi_pipe_send_bulk_data(usb_pipe_handle_impl_t  *pipe_handle,
		mblk_t *data,
		uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	size_t	length;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_send_bulk_data:");

	length = data->b_wptr - data->b_rptr;

	ASSERT(length != 0);

	return (ohci_common_bulk_routine(pipe_handle,
					length,
					data,
					usb_flags));
}


/*
 * ohci_hcdi_pipe_start_polling:
 */
static int
ohci_hcdi_pipe_start_polling(usb_pipe_handle_impl_t  *pipe_handle,
					uint_t flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	ohci_pipe_private_t *pp =
		(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;
	int	error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_start_polling: ep%d",
	    pipe_handle->p_endpoint->bEndpointAddress & USB_EPT_ADDR_MASK);

	/*
	 * Check and handle start polling on root hub interrupt pipe.
	 */
	if ((pipe_handle->p_usb_device->usb_addr == ROOT_HUB_ADDR) &&
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
						ATTRIBUTES_INTR)) {

		mutex_enter(&ohcip->ohci_int_mutex);
		error = ohci_handle_root_hub_pipe_start_polling(pipe_handle,
							flags);
		mutex_exit(&ohcip->ohci_int_mutex);

		return (error);
	}

	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	switch (pp->pp_state) {
		case STOPPED:
			/*
			 * This pipe has already been initialized.
			 * Just reset the  skip bit in the Endpoint
			 * Descriptor (ED) to restart the polling.
			 */
			(void) ohci_modify_sKip_bit(ohcip, pp, CLEAR_sKip);

			/*
			 * If head and tail pointers of ED are not equal
			 * then there is a valid interrupt TD on the  ED
			 * list. Otherwise insert interrupt TD into  the
			 * ED list and under this condition STOPPED case
			 * drops down to the OPEN case.
			 */
			if ((Get_ED(pp->pp_ept->hced_tailp) &
				HC_EPT_TD_TAIL) !=
					(Get_ED(pp->pp_ept->hced_headp) &
						HC_EPT_TD_HEAD)) {

				pp->pp_state = POLLING;
				break;
			}

			/* FALLTHRU */
		case OPENED:
			/*
			 * This pipe is uninitialized or if a valid TD is
			 * not found then insert a TD on the  interrupt
			 * ED.
			 */
			error = ohci_insert_intr_td(ohcip, pipe_handle,
				flags, ohci_handle_intr_td, NULL);

			if (error != USB_SUCCESS) {
				USB_DPRINTF_L2(PRINT_MASK_INTR,
				    ohcip->ohci_log_hdl,
				    "ohci_hcdi_pipe_start_polling: "
				    "Start polling failed");

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&ohcip->ohci_int_mutex);

				return (error);
			}

			USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
			    "ohci_hcdi_pipe_start_polling: PP = 0x%p", pp);

			ASSERT((pp->pp_tw_head != NULL) &&
					(pp->pp_tw_tail != NULL));

			/* Enable the interrupt list processing */
			Set_OpReg(hcr_control,
				Get_OpReg(hcr_control) | HCR_CONTROL_PLE);

			pp->pp_state = POLLING;

			break;
		case POLLING:
			USB_DPRINTF_L2(PRINT_MASK_INTR,
			    ohcip->ohci_log_hdl,
			    "ohci_hcdi_pipe_start_polling: "
			    "Polling is already in progress");
			error = USB_SUCCESS;
			break;
		case HALTED:
			USB_DPRINTF_L2(PRINT_MASK_INTR,
			    ohcip->ohci_log_hdl,
			    "ohci_hcdi_pipe_start_polling: "
			    "Pipe is halted and perform reset"
			    "before restart polling");
			error = USB_FAILURE;
			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_INTR,
			    ohcip->ohci_log_hdl,
			    "ohci_hcdi_pipe_start_polling: "
			    "Undefined state");
			error = USB_FAILURE;
			break;
	}

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	return (error);
}


/*
 * ohci_hcdi_pipe_stop_polling:
 */
/* ARGSUSED */
static int
ohci_hcdi_pipe_stop_polling(usb_pipe_handle_impl_t  *pipe_handle,
	uint_t flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;
	int	error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_stop_polling: Flags = 0x%x", flags);

	/*
	 * Check and handle stop polling on root hub interrupt pipe.
	 */
	if ((pipe_handle->p_usb_device->usb_addr == ROOT_HUB_ADDR) &&
		((endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) ==
						ATTRIBUTES_INTR)) {

		error = ohci_handle_root_hub_pipe_stop_polling(pipe_handle,
							flags);
		return (error);
	}

	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	if (pp->pp_state != POLLING) {

		USB_DPRINTF_L2(PRINT_MASK_HCDI,
		    ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_stop_polling: "
		    "Polling is already stopped");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle, flags,
			0, 0, USB_CC_NOERROR, USB_SUCCESS);

		return (USB_SUCCESS);
	}

	/*
	 * Set the skip bit in the host controller endpoint descriptor.
	 * Do not deallocate the bandwidth or tear down the DMA
	 */
	if (ohci_modify_sKip_bit(ohcip, pp, SET_sKip) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_stop_polling: No SOF's have started");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle, flags,
			0, 0, USB_CC_UNSPECIFIED_ERR, USB_FAILURE);

		return (USB_FAILURE);
	}

	/*
	 * Wait for processing all completed transfers and to send
	 * results to upstream.
	 */
	if (ohci_wait_for_transfers_completion(ohcip, pp) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_stop_polling: Not received transfers"
		    "completion pp = 0x%p", pp);

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle, flags,
			0, 0, USB_CC_UNSPECIFIED_ERR, USB_FAILURE);

		return (USB_FAILURE);
	}

	pp->pp_state = STOPPED;

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, flags, 0, 0,
				USB_CC_NOERROR, USB_SUCCESS);

	return (error);
}


/*
 * ohci_hcdi_pipe_send_isoc_data:
 */
static int
ohci_hcdi_pipe_send_isoc_data(
		usb_pipe_handle_impl_t *pipe_handle,
		mblk_t *data,
		uint_t usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	int error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_hcdi_pipe_send_isoc_data:");

	if ((error = ohci_insert_isoc_out_tds(ohcip,
			pipe_handle,
			data,
			usb_flags)) != USB_SUCCESS) {

		return (error);
	}

	return (error);
}


/*
 * Root hub related functions:
 *
 * ohci_handle_root_hub_pipe_open:
 *
 * Handle opening of control and interrupt pipes on root hub.
 */
/* ARGSUSED */
static int
ohci_handle_root_hub_pipe_open(usb_pipe_handle_impl_t  *pipe_handle,
				uint_t		usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_pipe_open: Root hub pipe open");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
		case ATTRIBUTES_CONTROL:
			/*
			 * Return failure if root hub control pipe
			 * is already in use.
			 */
			if (ohcip->ohci_root_hub.root_hub_ctrl_pipe_state
							!= NULL) {
				USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
				    ohcip->ohci_log_hdl,
				    "ohci_handle_root_hub_pipe_open:"
				    "Root hub control pipe open failed");

				return (USB_FAILURE);
			}

			ohcip->ohci_root_hub.root_hub_ctrl_pipe_handle =
							pipe_handle;

			/*
			 * Set the state of the root hub control
			 * pipe as OPENED.
			 */
			ohcip->ohci_root_hub.root_hub_ctrl_pipe_state =
								OPENED;

			USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_open: Root hub control "
			    "pipe open succeeded");

			break;
		case ATTRIBUTES_INTR:
			if ((endpoint_descr->bEndpointAddress &
					USB_EPT_ADDR_MASK) != 1) {
				USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
				    ohcip->ohci_log_hdl,
				    "ohci_handle_root_hub_pipe_open:"
				    "Root hub interrupt pipe open failed");

				return (USB_FAILURE);
			}

			/*
			 * Return failure if root hub interrupt pipe
			 * is already in use.
			 */
			if (ohcip->ohci_root_hub.root_hub_intr_pipe_state
							!= NULL) {
				USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
				    ohcip->ohci_log_hdl,
				    "ohci_handle_root_hub_pipe_open:"
				    "Root hub interrupt pipe open failed");

				return (USB_FAILURE);
			}

			ohcip->ohci_root_hub.root_hub_intr_pipe_handle =
							pipe_handle;

			/*
			 * Set the state of the root hub interrupt
			 * pipe as OPENED.
			 */
			ohcip->ohci_root_hub.root_hub_intr_pipe_state =
								OPENED;

			USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_open: Root hub interrupt"
			    "pipe open succeeded");

			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_open: Root hub pipe open"
			    "failed");

			return (USB_FAILURE);
	}

	ohcip->ohci_open_pipe_count++;

	return (USB_SUCCESS);
}


/*
 * ohci_handle_root_hub_pipe_close:
 *
 * Handle closing of control and interrupt pipes on root hub.
 */
/* ARGSUSED */
static int
ohci_handle_root_hub_pipe_close(usb_pipe_handle_impl_t	*pipe_handle)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_pipe_close: Root hub pipe close");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_CONTROL:
			/*
			 * Set both root hub control pipe handle and pipe
			 * state to NULL.
			 */
			ohcip->ohci_root_hub.root_hub_ctrl_pipe_handle = NULL;
			ohcip->ohci_root_hub.root_hub_ctrl_pipe_state = NULL;

			USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_close: "
			    "Root hub control pipe close succeeded");
			break;
		case USB_EPT_ATTR_INTR:
			ASSERT((endpoint_descr->bEndpointAddress &
					USB_EPT_ADDR_MASK) == 1);

			/*
			 * Set both root hub interrupt pipe handle and pipe
			 * state to NULL.
			 */
			ohcip->ohci_root_hub.root_hub_intr_pipe_handle = NULL;
			ohcip->ohci_root_hub.root_hub_intr_pipe_state = NULL;

			/* Disable root hub status change interrupt */
			Set_OpReg(hcr_intr_disable, HCR_INTR_RHSC);

			USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_close: "
			    "Root hub interrupt pipe close succeeded");

			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_close: "
			    "Root hub pipe close failed");

			return (USB_FAILURE);
	}

	ohcip->ohci_open_pipe_count--;

	return (USB_SUCCESS);
}


/*
 * ohci_handle_root_hub_pipe_reset:
 *
 * Handle resetting of control and interrupt pipes on root hub.
 */
/* ARGSUSED */
static int
ohci_handle_root_hub_pipe_reset(usb_pipe_handle_impl_t	*pipe_handle,
				uint_t		usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_pipe_reset: Root hub pipe reset");

	mutex_enter(&ohcip->ohci_int_mutex);

	switch (endpoint_descr->bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_CONTROL:

			ohcip->ohci_root_hub.root_hub_ctrl_pipe_state
							= OPENED;

			USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_reset: Pipe reset"
			    "for the root hub control pipe successful");

			break;
		case USB_EPT_ATTR_INTR:
			ASSERT((endpoint_descr->bEndpointAddress &
				USB_EPT_ADDR_MASK) == 1);

			if (ohcip->ohci_root_hub.root_hub_intr_pipe_state
								== POLLING) {
				/*
				 * Disable root hub status change
				 * interrupt.
				 */
				Set_OpReg(hcr_intr_disable, HCR_INTR_RHSC);

				ohcip->ohci_root_hub.root_hub_intr_pipe_state
							= OPENED;
			}
			ASSERT(ohcip->ohci_root_hub.root_hub_intr_pipe_state
								== OPENED);

			USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_reset: "
			    "Pipe reset for root hub interrupt "
			    "pipe successful");

			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_pipe_close: "
			    "Root hub pipe reset failed");

			mutex_exit(&ohcip->ohci_int_mutex);

			usba_hcdi_callback(pipe_handle, usb_flags, 0, 0,
				USB_CC_UNSPECIFIED_ERR, USB_FAILURE);

			return (USB_FAILURE);
	}

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, usb_flags,
		0, 0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_root_hub_pipe_start_polling:
 *
 * Handle start polling on root hub interrupt pipe.
 */
/* ARGSUSED */
static int
ohci_handle_root_hub_pipe_start_polling(usb_pipe_handle_impl_t	*pipe_handle,
				uint_t		usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_pipe_start_polling: "
	    "Root hub pipe start polling");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	ASSERT((endpoint_descr->bEndpointAddress &
				USB_EPT_ADDR_MASK) == 1);

	if ((ohcip->ohci_root_hub.root_hub_intr_pipe_state == OPENED) ||
		(ohcip->ohci_root_hub.root_hub_intr_pipe_state == STOPPED)) {

		/* Enable root hub status change interrupt */
		Set_OpReg(hcr_intr_enable, HCR_INTR_RHSC);

		ohcip->ohci_root_hub.root_hub_intr_pipe_state = POLLING;

		USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_handle_root_hub_pipe_start_polling: "
		    "Start polling for root hub successful");
	} else {
		USB_DPRINTF_L2(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_start_polling: "
		    "Polling for root hub is already in progress");
	}

	return (USB_SUCCESS);
}


/*
 * ohci_handle_root_hub_pipe_stop_polling:
 *
 * Handle stop polling on root hub interrupt pipe.
 */
/* ARGSUSED */
static int
ohci_handle_root_hub_pipe_stop_polling(usb_pipe_handle_impl_t  *pipe_handle,
				uint_t		usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	usb_endpoint_descr_t *endpoint_descr = pipe_handle->p_endpoint;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_pipe_stop_polling: "
	    "Root hub pipe stop polling");

	mutex_enter(&ohcip->ohci_int_mutex);

	ASSERT((endpoint_descr->bEndpointAddress &
				USB_EPT_ADDR_MASK) == 1);

	if (ohcip->ohci_root_hub.root_hub_intr_pipe_state == POLLING) {
		/* Disable root hub status change interrupt */
		Set_OpReg(hcr_intr_disable, HCR_INTR_RHSC);

		ohcip->ohci_root_hub.root_hub_intr_pipe_state = STOPPED;

		USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_hcdi_pipe_stop_polling: Stop polling for root"
		    "hub successful");
	} else {
		USB_DPRINTF_L2(PRINT_MASK_INTR,
		    ohcip->ohci_log_hdl, "ohci_hcdi_pipe_stop_polling: "
		    "Polling for root hub is already stopped");
	}

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, usb_flags,
		0, 0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_root_hub_request:
 *
 * Intercept a root hub request.  Handle the  root hub request through the
 * registers
 */
/* ARGSUSED */
static int
ohci_handle_root_hub_request(usb_pipe_handle_impl_t  *pipe_handle,
				uchar_t		bmRequestType,
				uchar_t		bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t		*data,
				uint_t		usb_flags)
{
	uint16_t port = wIndex - 1;  /* Adjust for controller */
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	int	error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_request: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%p",
	    bmRequestType, bRequest, wValue, wIndex, wLength, (void *)data);

	/* Adjust the port number for the controller */
	port = wIndex - 1;

	switch (bmRequestType) {
		case SET_CLEAR_PORT_FEATURE:
			error = ohci_handle_set_clear_port_feature(ohcip,
				pipe_handle, bRequest, wValue, port);
			break;
		case GET_PORT_STATUS:
			error = ohci_handle_get_port_status(ohcip, port,
					pipe_handle, wLength);
			break;
		case GET_HUB_DESCRIPTOR:
		    switch (bRequest) {
			case USB_REQ_GET_STATUS:
				error = ohci_handle_get_hub_status(ohcip,
							pipe_handle, wLength);
				break;
			case USB_REQ_GET_DESCRIPTOR:
				error = ohci_handle_get_hub_descriptor(ohcip,
							pipe_handle, wLength);
				break;
			default:
				USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
					ohcip->ohci_log_hdl,
					"ohci_handle_root_hub_request: "
					"Unsupported request 0x%x",
					bRequest);

				error = USB_FAILURE;

				/* Return failure */
				usba_hcdi_callback(pipe_handle,
					USB_FLAGS_SLEEP, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_FAILURE);
				break;
			}
			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
			    "ohci_handle_root_hub_request: "
			    "Unsupported request 0x%x", bmRequestType);

			error = USB_FAILURE;

			/* Return failure */
			usba_hcdi_callback(pipe_handle,
				USB_FLAGS_SLEEP, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_FAILURE);
			break;
	}

	if (data) {
		freeb(data);
	}

	return (error);
}


/*
 * ohci_handle_set_clear_port_feature:
 */
static int
ohci_handle_set_clear_port_feature(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uchar_t 	bRequest,
			uint16_t	wValue,
			uint16_t	port)
{
	int	error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_set_clear_port_feature: 0x%x 0x%x 0x%x",
	    bRequest, wValue, port);

	switch (bRequest) {
		case USB_REQ_SET_FEATURE:
		    switch (wValue) {
			case CFS_PORT_ENABLE:
				ohci_handle_port_enable(ohcip,
						pipe_handle, port, 1);
				break;
			case CFS_PORT_SUSPEND:
				ohci_handle_port_suspend(ohcip,
						pipe_handle, port, 1);
				break;
			case CFS_PORT_RESET:
				ohci_handle_port_reset(ohcip,
						pipe_handle, port);
				break;
			case CFS_PORT_POWER:
				ohci_handle_port_power(ohcip,
						pipe_handle, port, 1);
				break;
			default:
				USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
				    ohcip->ohci_log_hdl,
				    "ohci_handle_set_clear_port_feature: "
				    "Unsupported request 0x%x 0x%x",
				    bRequest, wValue);

				error = USB_FAILURE;

				/* Return failure */
				usba_hcdi_callback(pipe_handle,
					USB_FLAGS_SLEEP, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_FAILURE);
				break;
		    }
		    break;
		case USB_REQ_CLEAR_FEATURE:
		    switch (wValue) {
			case CFS_PORT_ENABLE:
				ohci_handle_port_enable(ohcip,
						pipe_handle, port, 0);
				break;
			case CFS_C_PORT_ENABLE:
				ohci_handle_clrchng_port_enable(ohcip,
						pipe_handle, port);
				break;
			case CFS_PORT_SUSPEND:
				ohci_handle_port_suspend(ohcip,
						pipe_handle, port, 0);
				break;
			case CFS_C_PORT_SUSPEND:
				ohci_handle_clrchng_port_suspend(ohcip,
						pipe_handle, port);
				break;
			case CFS_C_PORT_RESET:
				error = ohci_handle_complete_port_reset(ohcip,
						pipe_handle, port);
				break;
			case CFS_PORT_POWER:
				ohci_handle_port_power(ohcip,
						pipe_handle, port, 0);
				break;
			case CFS_C_PORT_CONNECTION:
				ohci_handle_clear_port_connection(ohcip,
						pipe_handle, port);
				break;
			default:
				USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
				    ohcip->ohci_log_hdl,
				    "ohci_handle_set_clear_port_feature: "
				    "Unsupported request 0x%x 0x%x",
				    bRequest, wValue);

				error = USB_FAILURE;

				/* Return failure */
				usba_hcdi_callback(pipe_handle,
					USB_FLAGS_SLEEP, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_FAILURE);

				break;
		    }
		    break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_ROOT_HUB,
			    ohcip->ohci_log_hdl,
			    "ohci_handle_set_clear_port_feature: "
			    "Unsupported request 0x%x 0x%x",
			    bRequest, wValue);

			error = USB_FAILURE;

			/* Return failure */
			usba_hcdi_callback(pipe_handle,
				USB_FLAGS_SLEEP, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_FAILURE);
			    break;
	}

	return (error);
}


/*
 * ohci_handle_port_power:
 *
 * Turn on a root hub port.
 */
static void
ohci_handle_port_power(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port, uint_t on)
{
	uint_t	port_status;
	root_hub_t *rh;
	ushort_t wHubCharateristics;
	uint_t p;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = Get_OpReg(hcr_rh_portstatus[port]);
	rh = &ohcip->ohci_root_hub;
	wHubCharateristics =
		ohcip->ohci_root_hub.root_hub_descr.wHubCharacteristics;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_port_power: port = 0x%x status = 0x%x on = %d",
	    port, port_status, on);


	if (on) {
		/*
		 * If the port power is ganged, enable the power through
		 * the status registers, else enable the port power.
		 */
		if ((wHubCharateristics & HUB_CHARS_POWER_SWITCHING_MODE) ==
		    HUB_CHARS_GANGED_POWER) {

			Set_OpReg(hcr_rh_status, HCR_RH_STATUS_LPSC);

			for (p = 0; p < rh->root_hub_num_ports; p++) {
				rh->root_hub_port_status[p] = 0;
				rh->root_hub_port_state[p] = DISCONNECTED;
			}
		} else {
			/* See if the port power is already on */
			if (!(port_status & HCR_PORT_PPS)) {
				/* Turn the port on */
				Set_OpReg(hcr_rh_portstatus[port],
							HCR_PORT_PPS);
			}
			ASSERT(Get_OpReg(hcr_rh_portstatus[port]) &
							HCR_PORT_PPS);

			rh->root_hub_port_status[port] = 0;
			rh->root_hub_port_state[port] = DISCONNECTED;
		}
	} else {
		/*
		 * If the port power is ganged, disable the power through
		 * the status registers, else disable the port power.
		 */
		if ((wHubCharateristics & HUB_CHARS_POWER_SWITCHING_MODE) ==
		    HUB_CHARS_GANGED_POWER) {

			Set_OpReg(hcr_rh_status, HCR_RH_STATUS_LPS);

			for (p = 0; p < rh->root_hub_num_ports; p++) {
				rh->root_hub_port_status[p] = 0;
				rh->root_hub_port_state[p] = POWERED_OFF;
			}
		} else {

			/* See if the port power is already OFF */
			if ((port_status & HCR_PORT_PPS)) {
				/* Turn the port OFF by writing LSSA bit  */
				Set_OpReg(hcr_rh_portstatus[port],
							HCR_PORT_LSDA);
			}
			rh->root_hub_port_status[port] = 0;

			rh->root_hub_port_state[port] = POWERED_OFF;
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_port_power done:"
	    " port = 0x%x status = 0x%x on = %d",
	    port, Get_OpReg(hcr_rh_portstatus[port]), on);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_port_enable:
 *
 * Handle port enable request.
 */
static void
ohci_handle_port_enable(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port, uint_t on)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = Get_OpReg(hcr_rh_portstatus[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_port_enable: port = 0x%x, status = 0x%x",
	    port, port_status);

	if (on) {
		/* See if the port enable is already on */
		if (!(port_status & HCR_PORT_PES)) {
			/* Enable the port */
			Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_PES);
		}
	} else {
		/* See if the port enable is already off */
		if (!(port_status & HCR_PORT_PES)) {
			/* disable the port by writing CCS bit */
			Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_CCS);
		}
	}

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_clrchng_port_enable:
 *
 * Handle clear port enable change bit.
 */
static void
ohci_handle_clrchng_port_enable(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = Get_OpReg(hcr_rh_portstatus[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_port_enable: port = 0x%x, status = 0x%x",
	    port, port_status);

	/* Clear the PortEnableStatusChange Bit */
	Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_PESC);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_port_suspend:
 *
 * Handle port suspend/resume request.
 */
static void
ohci_handle_port_suspend(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port, uint_t on)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = Get_OpReg(hcr_rh_portstatus[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_port_suspend: port = 0x%x, status = 0x%x",
	    port, port_status);

	if (on) {
		/* Suspend the port */
		Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_PSS);
	} else {
		/* To Resume, we write the POCI bit */
		Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_POCI);
	}

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_clrchng_port_suspend:
 *
 * Handle port clear port suspend change bit.
 */
static void
ohci_handle_clrchng_port_suspend(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = Get_OpReg(hcr_rh_portstatus[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_clrchng_port_suspend: port = 0x%x, status = 0x%x",
	    port, port_status);

	Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_PSSC);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_port_reset:
 *
 * Perform a port reset.
 */
static void
ohci_handle_port_reset(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = Get_OpReg(hcr_rh_portstatus[port]);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_port_reset: port = 0x%x status = 0x%x",
	    port, port_status);

	if (!(port_status & HCR_PORT_CCS)) {
		USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
		    "port_status & HCR_PORT_CCS == 0: "
		    "port = 0x%x status = 0x%x", port, port_status);
	}

	Set_OpReg(hcr_rh_portstatus[port],
		    (port_status| HCR_PORT_PRS) & HCR_PORT_MASK);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_complete_port_reset:
 *
 * Perform a port reset change.
 */
static int
ohci_handle_complete_port_reset(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = ohcip->ohci_root_hub.root_hub_port_status[port];

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_complete_port_reset: port = 0x%x status = 0x%x",
	    port, port_status);

	if (!(port_status & HCR_PORT_CCS)) {
		USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
		    "port_status & HCR_PORT_CCS == 0: "
		    "port = 0x%x status = 0x%x", port, port_status);
	}

	Set_OpReg(hcr_rh_portstatus[port], HCR_PORT_PRSC);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, NULL,
				0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_clear_port_connection:
 *
 * Perform a clear port connection.
 */
static void
ohci_handle_clear_port_connection(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t port)
{
	uint_t port_status;

	mutex_enter(&ohcip->ohci_int_mutex);

	port_status = ohcip->ohci_root_hub.root_hub_port_status[port];

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_clear_port_connection: port = 0x%x"
	    "status = 0x%x", port, port_status);

	/*
	 * Actual CSC bit is already cleared during root hub status change
	 * interrupt handling but clear CSC bit in the cached port status.
	 */
	ohcip->ohci_root_hub.root_hub_port_status[port] &= ~HCR_PORT_CSC;

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle,
		USB_FLAGS_SLEEP, NULL, 0,
			USB_CC_NOERROR, USB_SUCCESS);
}


/*
 * ohci_handle_get_port_status:
 *
 * Handle a get port status request.
 */
static int
ohci_handle_get_port_status(openhci_state_t *ohcip,
			uint16_t port,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t wLength)
{
	mblk_t *message;
	uint_t	new_port_status;
	uint_t	old_port_status;
	uint_t	change_status;

	ASSERT(wLength == 4);

	mutex_enter(&ohcip->ohci_int_mutex);

	/*
	 * The old state is the one that got cached during the ohci's
	 * root hub status change interrupt, where we clear all the
	 * change bits, but the driver has not seen them yet
	 * So we return OR of old status and the one currently read
	 * This way, driver gets the correct status even if there is
	 * no polling on the intr pipe
	 */
	old_port_status = ohcip->ohci_root_hub.root_hub_port_status[port];
	new_port_status = Get_OpReg(hcr_rh_portstatus[port]);
	ohcip->ohci_root_hub.root_hub_port_status[port] = new_port_status;
	new_port_status |= old_port_status;

	change_status = (new_port_status & HCR_PORT_CHNG_MASK) >> 16;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_get_port_status: port = %d new status = 0x%x"
	    "change = 0x%x", port, new_port_status, change_status);

	message = allocb(wLength, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_get_port_status: Allocb failed");

		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle,
			USB_FLAGS_SLEEP, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);

		return (USB_NO_RESOURCES);
	}

	*message->b_wptr++ = (uchar_t)new_port_status;
	*message->b_wptr++ = (uchar_t)(new_port_status >> 8);
	*message->b_wptr++ = (uchar_t)change_status;
	*message->b_wptr++ = (uchar_t)(change_status >> 8);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, message,
				0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_get_hub_descriptor:
 */
static int
ohci_handle_get_hub_descriptor(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t wLength)
{
	mblk_t *message;
	usb_hub_descr_t *root_hub_descr;
	static uchar_t	raw_descr[ROOT_HUB_DESCRIPTOR_LENGTH];

	mutex_enter(&ohcip->ohci_int_mutex);

	root_hub_descr = &ohcip->ohci_root_hub.root_hub_descr;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_get_hub_descriptor: wLength = 0x%x",
	    wLength);

	ASSERT(wLength != 0);

	message = allocb(wLength, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_get_hub_descriptor: Allocb failed");

		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle,
			USB_FLAGS_SLEEP, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);

		return (USB_NO_RESOURCES);
	}

	bzero(&raw_descr, ROOT_HUB_DESCRIPTOR_LENGTH);

	raw_descr[0] = root_hub_descr->bDescLength;
	raw_descr[1] = root_hub_descr->bDescriptorType;
	raw_descr[2] = root_hub_descr->bNbrPorts;
	raw_descr[3] = root_hub_descr->wHubCharacteristics & 0x00FF;
	raw_descr[4] = (root_hub_descr->wHubCharacteristics & 0xFF00) >> 8;
	raw_descr[5] = root_hub_descr->bPwrOn2PwrGood;
	raw_descr[6] = root_hub_descr->bHubContrCurrent;
	raw_descr[7] = root_hub_descr->DeviceRemovable;
	raw_descr[8] = root_hub_descr->PortPwrCtrlMask;

	bcopy(raw_descr, message->b_wptr, wLength);
	message->b_wptr += wLength;

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, message,
				0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_get_hub_status:
 *
 * Handle a get hub status request.
 */
static int
ohci_handle_get_hub_status(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle,
			uint16_t wLength)
{
	mblk_t *message;
	uint_t	new_root_hub_status;

	ASSERT(wLength == 4);

	mutex_enter(&ohcip->ohci_int_mutex);

	new_root_hub_status = Get_OpReg(hcr_rh_status);

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_get_hub_status: new root hub status = 0x%x",
	    new_root_hub_status);

	message = allocb(wLength, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_get_hub_status: Allocb failed");

		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(pipe_handle,
			USB_FLAGS_SLEEP, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);

		return (USB_NO_RESOURCES);
	}

	*message->b_wptr++ = (uchar_t)new_root_hub_status;
	*message->b_wptr++ = (uchar_t)(new_root_hub_status >> 8);
	*message->b_wptr++ = (uchar_t)(new_root_hub_status >> 16);
	*message->b_wptr++ = (uchar_t)(new_root_hub_status >> 24);

	mutex_exit(&ohcip->ohci_int_mutex);

	usba_hcdi_callback(pipe_handle, USB_FLAGS_SLEEP, message,
				0, USB_CC_NOERROR, USB_SUCCESS);

	return (USB_SUCCESS);
}


/*
 * Bandwidth Allocation functions
 */


/*
 * ohci_allocate_bandwidth:
 *
 * Figure out whether or not this interval may be supported. Return the index
 * into the  lattice if it can be supported.  Return allocation failure if it
 * can not be supported.
 */
int
ohci_allocate_bandwidth(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle)
{
	int bandwidth;			/* Requested bandwidth */
	uint_t min, min_index;		/* Min total bandwidth for subtree */
	uint_t i;
	uint_t height;			/* Bandwidth's height in the tree */
	uint_t node;
	uint_t leftmost;
	uint_t length;
	usb_endpoint_descr_t *endpoint = pipe_handle->p_endpoint;

	/* This routine is protected by the ohci_int_mutex */
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Calculate the length in bytes of a transaction on this
	 * periodic endpoint.
	 */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);
	length = ohci_compute_total_bandwidth(endpoint,
				pipe_handle->p_usb_device->usb_port_status);
	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/*
	 * If the length in bytes plus the allocated bandwidth exceeds
	 * the maximum, return bandwidth allocation failure.
	 */
	if ((length + ohcip->ohci_bandwidth_intr_min +
	    ohcip->ohci_bandwidth_isoch_sum) > (MAX_PERIODIC_BANDWIDTH)) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_allocate_bandwidth: "
		    "Reached maximum bandwidth value and cannot allocate "
		    "bandwidth for a given Interrupt/Isoch endpoint");

		return (USB_FAILURE);
	}

	/*
	 * If this is an isochronous endpoint, just add the endpoint
	 * to the root of the interrupt lattice and add the required
	 * endpoint's bandiwdth to the isochronous bandwidth.
	 */
	if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK) ==
		ATTRIBUTES_ISOCH) {

		for (i = 0; i < NUM_INTR_ED_LISTS; i ++) {

			if ((length + ohcip->ohci_bandwidth_isoch_sum +
				ohcip->ohci_bandwidth[i]) >
					MAX_PERIODIC_BANDWIDTH) {

				USB_DPRINTF_L2(PRINT_MASK_LISTS,
				    ohcip->ohci_log_hdl,
				    "ohci_allocate_bandwidth: "
				    "Reached maximum bandwidth value & cannot "
				    "allocate bandwidth for Isoch endpoint");

				return (USB_FAILURE);
			}
		}

		ohcip->ohci_bandwidth_isoch_sum =
			ohcip->ohci_bandwidth_isoch_sum + length;

		return (USB_SUCCESS);
	}

	/*
	 * This is an interrupt endpoint
	 */

	/* Adjust bandwidth to be a power of 2 */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);
	bandwidth = ohci_bandwidth_adjust(ohcip, endpoint,
				pipe_handle->p_usb_device->usb_port_status);
	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/*
	 * If this bandwidth can't be supported,
	 * return allocation failure.
	 */
	if (bandwidth == USB_FAILURE) {

		return (USB_FAILURE);

	}

	USB_DPRINTF_L4(PRINT_MASK_BW, ohcip->ohci_log_hdl,
	    "The new bandwidth is %d", bandwidth);

	/* Find the leaf with the smallest allocated bandwidth */
	min_index = 0;
	min = ohcip->ohci_bandwidth[0];

	for (i = 1; i < NUM_INTR_ED_LISTS; i++) {

		if (ohcip->ohci_bandwidth[i] < min) {

			min_index = i;
			min = ohcip->ohci_bandwidth[i];
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_BW, ohcip->ohci_log_hdl,
	    "The leaf with minimal bandwidth %d", min_index);

	USB_DPRINTF_L4(PRINT_MASK_BW, ohcip->ohci_log_hdl,
	    "The smallest bandwidth %d", min);

	/* Adjust min for the lattice */
	min_index = min_index + NUM_INTR_ED_LISTS - 1;

	/*
	 * Find the index into the lattice given the
	 * leaf with the smallest allocated bandwidth.
	 */
	height = ohci_lattice_height(bandwidth);

	USB_DPRINTF_L4(PRINT_MASK_BW, ohcip->ohci_log_hdl,
	    "The height is %d", height);

	node = min_index;

	for (i = 0; i < height; i++) {

		node = ohci_lattice_parent(node);
	}

	USB_DPRINTF_L4(PRINT_MASK_BW, ohcip->ohci_log_hdl,
			"The real node is %d", node);

	/*
	 * Find the leftmost leaf in the subtree
	 * specified by the node.
	 */
	leftmost = ohci_leftmost_leaf(node, height);

	USB_DPRINTF_L4(PRINT_MASK_BW, ohcip->ohci_log_hdl,
	    "Leftmost %d", leftmost);

	for (i = leftmost; i < leftmost + (NUM_INTR_ED_LISTS/bandwidth); i ++) {

		if ((length + ohcip->ohci_bandwidth_isoch_sum +
			ohcip->ohci_bandwidth[i]) > MAX_PERIODIC_BANDWIDTH) {

			USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "ohci_allocate_bandwidth:\n"
			    "Reached maximum bandwidth value and "
			    "cannot allocate bandwidth for intr. endpoint");

			return (USB_FAILURE);
		}
	}

	/*
	 * All the leaves for this node must be updated with the bandwidth.
	 */
	for (i = leftmost; i < leftmost + (NUM_INTR_ED_LISTS/bandwidth); i ++) {

		ohcip->ohci_bandwidth[i] = ohcip->ohci_bandwidth[i] + length;

	}

	/* Find the leaf with the smallest allocated bandwidth */
	min_index = 0;
	min = ohcip->ohci_bandwidth[0];

	for (i = 1; i < NUM_INTR_ED_LISTS; i++) {

		if (ohcip->ohci_bandwidth[i] < min) {

			min_index = i;
			min = ohcip->ohci_bandwidth[i];
		}

	}

	/* Save the minimum for later use */
	ohcip->ohci_bandwidth_intr_min = min;

	/*
	 * Return the index of the
	 * node within the lattice.
	 */
	return (node);
}


/*
 * ohci_deallocate_bandwidth:
 *
 * Deallocate bandwidth for the given node in the lattice and the length of
 * transfer.
 */
void
ohci_deallocate_bandwidth(openhci_state_t *ohcip,
		usb_pipe_handle_impl_t *pipe_handle)
{
	uint_t bandwidth;
	uint_t height;
	uint_t leftmost;
	uint_t i;
	uint_t min;
	usb_endpoint_descr_t *endpoint = pipe_handle->p_endpoint;
	uint_t node, length;
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;

	/* This routine is protected by the ohci_int_mutex */
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Obtain the length */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);
	length = ohci_compute_total_bandwidth(endpoint,
				pipe_handle->p_usb_device->usb_port_status);
	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/*
	 * If this is an isochronous endpoint, just delete endpoint's
	 * bandwidth from the total allocated isochronous bandwidth.
	 */
	if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK)
		== ATTRIBUTES_ISOCH) {
		ohcip->ohci_bandwidth_isoch_sum =
			ohcip->ohci_bandwidth_isoch_sum - length;

		return;
	}

	/* Obtain the node */
	node = pp->pp_node;

	/* Adjust bandwidth to be a power of 2 */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);
	bandwidth = ohci_bandwidth_adjust(ohcip, endpoint,
				pipe_handle->p_usb_device->usb_port_status);
	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/* Find the height in the tree */
	height = ohci_lattice_height(bandwidth);

	/*
	 * Find the leftmost leaf in the subtree specified by the node
	 */
	leftmost = ohci_leftmost_leaf(node, height);

	/* Delete the bandwith from the appropriate lists */
	for (i = leftmost; i < leftmost + (NUM_INTR_ED_LISTS/bandwidth); i ++) {

		ohcip->ohci_bandwidth[i] = ohcip->ohci_bandwidth[i] - length;
	}

	min = ohcip->ohci_bandwidth[0];

	/* Recompute the minimum */
	for (i = 1; i < NUM_INTR_ED_LISTS; i++) {
		if (ohcip->ohci_bandwidth[i] < min) {
			min = ohcip->ohci_bandwidth[i];
		}
	}

	/* Save the minimum for later use */
	ohcip->ohci_bandwidth_intr_min = min;
}


/*
 * ohci_compute_total_bandwidth:
 *
 * Given a periodic endpoint (interrupt or isochronous) determine the total
 * bandwidth for one transaction. The OpenHCI host controller traverses the
 * endpoint descriptor lists on a first-come-first-serve basis. When the HC
 * services an endpoint, only a single transaction attempt is made. The  HC
 * moves to the next Endpoint Descriptor after the first transaction attempt
 * rather than finishing the entire Transfer Descriptor. Therefore, when  a
 * Transfer Descriptor is inserted into the lattice, we will only count the
 * number of bytes for one transaction.
 *
 * The following are the formulas used for  calculating bandwidth in  terms
 * bytes and it is for the single USB full speed and low speed	transaction
 * respectively. The protocol overheads will be different for each of  type
 * of USB transfer and all these formulas & protocol overheads are  derived
 * from the 5.9.3 section of USB Specification & with the help of Bandwidth
 * Analysis white paper which is posted on the USB  developer forum.
 *
 * Full-Speed:
 *		Protocol overhead  + ((MaxPacketSize * 7)/6 )  + Host_Delay
 *
 * Low-Speed:
 *		Protocol overhead  + Hub LS overhead +
 *		  (Low-Speed clock * ((MaxPacketSize * 7)/6 )) + Host_Delay
 */
static uint_t
ohci_compute_total_bandwidth(usb_endpoint_descr_t *endpoint,
				    usb_port_status_t port_status)
{
	ushort_t	MaxPacketSize = endpoint->wMaxPacketSize;
	uint_t		bandwidth = 0;

	/* Add Host Controller specific delay to required bandwidth */
	bandwidth = HOST_CONTROLLER_DELAY;

	/* Add bit-stuffing overhead */
	MaxPacketSize = (ushort_t)((MaxPacketSize * 7) / 6);

	/* Low Speed interrupt transaction */
	if (port_status == USB_LOW_SPEED_DEV) {
		/* Low Speed interrupt transaction */
		bandwidth += (LOW_SPEED_PROTO_OVERHEAD +
				HUB_LOW_SPEED_PROTO_OVERHEAD +
				(LOW_SPEED_CLOCK * MaxPacketSize));
	} else {
		/* Full Speed transaction */
		bandwidth += MaxPacketSize;

		if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK) ==
			ATTRIBUTES_INTR) {
			/* Full Speed interrupt transaction */
			bandwidth += FS_NON_ISOC_PROTO_OVERHEAD;
		} else {
			/* Isochronus and input transaction */
			if ((endpoint->bEndpointAddress &
				USB_EPT_DIR_MASK) == USB_EPT_DIR_IN) {
				bandwidth += FS_ISOC_INPUT_PROTO_OVERHEAD;
			} else {
				/* Isochronus and output transaction */
				bandwidth += FS_ISOC_OUTPUT_PROTO_OVERHEAD;
			}
		}
	}

	return (bandwidth);
}


/*
 * ohci_bandwidth_adjust:
 */
static int
ohci_bandwidth_adjust(openhci_state_t *ohcip,
		usb_endpoint_descr_t *endpoint,
			usb_port_status_t port_status)
{
	uint_t interval;
	int i = 0;

	/*
	 * Get the polling interval from the endpoint descriptor
	 */
	interval = endpoint->bInterval;

	/*
	 * The bInterval value in the endpoint descriptor can range
	 * from 1 to 255ms. The interrupt lattice has 32 leaf nodes,
	 * and the host controller cycles through these nodes every
	 * 32ms. The longest polling  interval that the  controller
	 * supports is 32ms.
	 */

	/*
	 * Return an error if the polling interval is less than 1ms
	 * and greater than 255ms
	 */
	if ((interval < MIN_POLL_INTERVAL) &&
			(interval > MAX_POLL_INTERVAL)) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_bandwidth_adjust: "
		    "Endpoint's poll interval must be between %d and %d ms",
		    MIN_POLL_INTERVAL, MAX_POLL_INTERVAL);

		return (USB_FAILURE);
	}

	/*
	 * According USB Specifications, a  full-speed endpoint can
	 * specify a desired polling interval 1ms to 255ms and a low
	 * speed  endpoints are limited to  specifying only 10ms to
	 * 255ms. But some old keyboards & mice uses polling interval
	 * of 8ms. For compatibility  purpose, we are using polling
	 * interval between 8ms & 255ms for low speed endpoints. But
	 * ohci driver will reject the any low speed endpoints which
	 * request polling interval less than 8ms.
	 */
	if ((port_status == USB_LOW_SPEED_DEV) &&
		(interval < MIN_LOW_SPEED_POLL_INTERVAL)) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_bandwidth_adjust: "
		    "Low speed endpoint's poll interval must be "
		    "less than %d ms", MIN_LOW_SPEED_POLL_INTERVAL);

		return (USB_FAILURE);
	}

	/*
	 * If polling interval is greater than 32ms,
	 * adjust polling interval equal to 32ms.
	 */
	if (interval > NUM_INTR_ED_LISTS) {
		interval = NUM_INTR_ED_LISTS;
	}

	/*
	 * Find the nearest power of 2 that'sless
	 * than interval.
	 */
	while ((pow_2(i)) <= interval) {
		i++;
	}

	return (pow_2((i - 1)));
}


/*
 * ohci_lattice_height:
 *
 * Given the requested bandwidth, find the height in the tree at which the
 * nodes for this bandwidth fall.  The height is measured as the number of
 * nodes from the leaf to the level specified by bandwidth The root of the
 * tree is at height TREE_HEIGHT.
 */
static uint_t
ohci_lattice_height(uint_t bandwidth)
{
	return (TREE_HEIGHT - (log_2(bandwidth)));
}


/*
 * ohci_lattice_parent:
 *
 * Given a node in the lattice, find the index of the parent node
 */
static uint_t
ohci_lattice_parent(uint_t node)
{
	if ((node % 2) == 0) {
		return ((node/2) - 1);
	} else {
		return ((node + 1)/2 - 1);
	}
}


/*
 * ohci_leftmost_leaf:
 *
 * Find the leftmost leaf in the subtree specified by the node. Height refers
 * to number of nodes from the bottom of the tree to the node,	including the
 * node.
 */
static uint_t
ohci_leftmost_leaf(uint_t node, uint_t height)
{
	return (1 + ((pow_2(height)) * node) + ((pow_2(height)) - 1)
				    -  NUM_INTR_ED_LISTS);
}


/*
 * ohci_hcca_intr_index:
 *
 * Given a node in the lattice, find the index for the hcca interrupt table
 */
static uint_t
ohci_hcca_intr_index(uint_t node)
{
	/*
	 * Adjust the node to the array representing
	 * the bottom of the tree.
	 */
	node = node - NUM_STATIC_NODES;

	if ((node % 2) == 0) {
		return (index[node / 2]);
	} else {
		return (index[node / 2] + (NUM_INTR_ED_LISTS / 2));
	}
}


/*
 * pow_2:
 *
 * Compute 2 to the power
 */
static uint_t
pow_2(uint_t x)
{
	if (x == 0) {
		return (1);
	} else {

		return (2 << (x - 1));

	}
}


/*
 * log_2:
 *
 * Compute log base 2 of x
 */
static uint_t
log_2(uint_t x)
{
	int i = 0;

	while (x != 1) {
		x = x >> 1;
		i++;
	}

	return (i);
}


/*
 * Endpoint Descriptor (ED) manipulations functions
 */


/*
 * ohci_alloc_hc_ed:
 *
 * Allocate an endpoint descriptor (ED)
 */
hc_ed_t *
ohci_alloc_hc_ed(openhci_state_t *ohcip,
		usb_pipe_handle_impl_t	*pipe_handle)
{
	int i, ctrl;
	int hced_ctrl = 0;
	hc_ed_t	*hc_ed;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_alloc_hc_ed: ph = 0x%p", (void *)pipe_handle);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * The first 31 endpoints in the Endpoint Descriptor (ED)
	 * buffer pool are reserved for building interrupt lattice
	 * tree. Search for a blank endpoint descriptor in the ED
	 * buffer pool.
	 */
	for (i = NUM_STATIC_NODES; i < ed_pool_size; i ++) {
		ctrl = Get_ED(ohcip->ohci_ed_pool_addr[i].hced_ctrl);

		if (ctrl == HC_EPT_BLANK) {
			break;
		}
	}

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_alloc_hc_ed: Allocated %d", i);

	if (i == ed_pool_size) {
		USB_DPRINTF_L2(PRINT_MASK_ALLOC,  ohcip->ohci_log_hdl,
		    "ohci_alloc_hc_ed: ED exhausted");

		return (NULL);
	} else {

		hc_ed = &ohcip->ohci_ed_pool_addr[i];

		USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
		    "ohci_alloc_hc_ed: Allocated address 0x%p", (void *)hc_ed);

		ohci_print_ed(ohcip, hc_ed);

#ifdef	RIO
		/* Unpack the endpoint descriptor into a control field */
		if (pipe_handle != NULL) {
#endif	/* RIO */
			hced_ctrl = ohci_unpack_endpoint(ohcip, pipe_handle);
#ifdef	RIO
		}
#endif	/* RIO */

		bzero((void *)hc_ed, sizeof (hc_ed_t));

		Set_ED(hc_ed->hced_ctrl, 0);

		Set_ED(hc_ed->hced_ctrl, hced_ctrl);

		if ((ohci_initialize_dummy(ohcip, hc_ed)) == USB_NO_RESOURCES) {
			bzero((void *)hc_ed, sizeof (hc_ed_t));
			Set_ED(hc_ed->hced_ctrl, HC_EPT_BLANK);
			return (NULL);
		}

		Set_ED(hc_ed->hced_prev, NULL);
		Set_ED(hc_ed->hced_next, NULL);

		return (hc_ed);
	}
}


/*
 * ohci_unpack_endpoint:
 *
 * Unpack the information in the pipe handle and create the first byte of the
 * Host Controller's (HC) Endpoint Descriptor (ED).
 */
static uint_t
ohci_unpack_endpoint(openhci_state_t *ohcip,
		usb_pipe_handle_impl_t *pipe_handle)
{
	usb_endpoint_descr_t *endpoint = pipe_handle->p_endpoint;
	uint_t	maxpacketsize;
	uint_t	ctrl = 0;
	uint_t	addr;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_unpack_endpoint:");

	ctrl = pipe_handle->p_usb_device->usb_addr;

	addr = endpoint->bEndpointAddress;

	/* Assign the endpoint's address */
	ctrl = ctrl | ((addr & USB_EPT_ADDR_MASK) << HC_EPT_EP_SHFT);

	/*
	 * Assign the direction. If the endpoint is a control endpoint,
	 * the direction is assigned by the Transfer Descriptor (TD).
	 */
	if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK) !=
		ATTRIBUTES_CONTROL) {
		if (addr & USB_EPT_DIR_MASK) {
			/* The direction is IN */
			ctrl = ctrl | HC_EPT_DF_IN;
		} else {
			/* The direction is OUT */
			ctrl = ctrl | HC_EPT_DF_OUT;
		}
	}

	/* Assign the speed */
	mutex_enter(&pipe_handle->p_usb_device->usb_mutex);
	if (pipe_handle->p_usb_device->usb_port_status == USB_LOW_SPEED_DEV) {
		ctrl = ctrl | HC_EPT_Speed;
	}
	mutex_exit(&pipe_handle->p_usb_device->usb_mutex);

	/* Assign the format */
	if ((endpoint->bmAttributes & USB_EPT_ATTR_MASK) == ATTRIBUTES_ISOCH) {
		ctrl = ctrl | HC_EPT_Format;
	}

	maxpacketsize = endpoint->wMaxPacketSize;
	maxpacketsize = maxpacketsize << HC_EPT_MAXPKTSZ;
	ctrl = ctrl | (maxpacketsize & HC_EPT_MPS);

	return (ctrl);
}


/*
 * ohci_insert_ed:
 *
 * Add the Endpoint Descriptor (ED) into the Host Controller's (HC) appropriate
 * endpoint list.
 */
static void
ohci_insert_ed(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t	*pipe_handle)
{
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_ed:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	switch (pipe_handle->p_endpoint->bmAttributes & USB_EPT_ATTR_MASK) {
		case ATTRIBUTES_CONTROL:
			ohci_insert_ctrl_ed(ohcip, pp);
			break;
		case ATTRIBUTES_BULK:
			ohci_insert_bulk_ed(ohcip, pp);
			break;
		case ATTRIBUTES_ISOCH:
		case ATTRIBUTES_INTR:
			ohci_insert_periodic_ed(ohcip, pp);
			break;
	}
}


/*
 * ohci_insert_ctrl_ed:
 *
 * Insert a control endpoint into the Host Controller's (HC) control endpoint
 * list.
 */
static void
ohci_insert_ctrl_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;
	hc_ed_t *prev_ept;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_ctrl_ed:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Obtain a ptr to the head of the list */
	prev_ept = EDpool_iommu_to_cpu(ohcip, Get_OpReg(hcr_ctrl_head));

	/* Set up the backwards pointer */
	if (prev_ept != NULL) {
		Set_ED(prev_ept->hced_prev, EDpool_cpu_to_iommu(ohcip, ept));
	}

	/* The new endpoint points to the head of the list */
	Set_ED(ept->hced_next, Get_OpReg(hcr_ctrl_head));

	/* Set the head ptr to the new endpoint */
	Set_OpReg(hcr_ctrl_head, EDpool_cpu_to_iommu(ohcip, ept));
}


/*
 * ohci_insert_bulk_ed:
 *
 * Insert a bulk endpoint into the Host Controller's (HC) bulk endpoint list.
 */
static void
ohci_insert_bulk_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;
	hc_ed_t *prev_ept;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_bulk_ed:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Obtain a ptr to the head of the Bulk list */
	prev_ept = EDpool_iommu_to_cpu(ohcip, Get_OpReg(hcr_bulk_head));

	/* Set up the backwards pointer */
	if (prev_ept != NULL) {
		Set_ED(prev_ept->hced_prev, EDpool_cpu_to_iommu(ohcip, ept));
	}

	/* The new endpoint points to the head of the Bulk list */
	Set_ED(ept->hced_next, Get_OpReg(hcr_bulk_head));

	/* Set the Bulk head ptr to the new endpoint */
	Set_OpReg(hcr_bulk_head, EDpool_cpu_to_iommu(ohcip, ept));
}


/*
 * ohci_insert_periodic_ed:
 *
 * Insert a periodic endpoints i.e Interrupt and  Isochronous endpoints into the
 * Host Controller's (HC) interrupt lattice tree.
 */
static void
ohci_insert_periodic_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;
	hc_ed_t *next_lattice_ept, *lattice_ept;

	/*
	 * The appropriate node was found
	 * during the opening of the pipe.
	 */
	uint_t node = pp->pp_node;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_periodic_ed:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	if (node >= NUM_STATIC_NODES) {
		/* Get the hcca interrupt table index */
		node = ohci_hcca_intr_index(node);

		/* Get the first endpoint on the list */
		next_lattice_ept = EDpool_iommu_to_cpu(ohcip,
			Get_HCCA(ohcip->ohci_hccap->HccaIntTble[node]));

		/* Update this endpoint to point to it */
		Set_ED(ept->hced_next,
			EDpool_cpu_to_iommu(ohcip, next_lattice_ept));

		/* Put this endpoint at the head of the list */
		Set_HCCA(ohcip->ohci_hccap->HccaIntTble[node],
				EDpool_cpu_to_iommu(ohcip, ept));

		/* The previous pointer is NULL */
		Set_ED(ept->hced_prev, NULL);

		/* Update the previous pointer of ept->hced_next */
		if (!((Get_ED(next_lattice_ept->hced_flag)) & HC_EPT_STATIC)) {
			Set_ED(next_lattice_ept->hced_prev,
				EDpool_cpu_to_iommu(ohcip, ept));
		}

	} else {
		/* Find the lattice endpoint */
		lattice_ept = &ohcip->ohci_ed_pool_addr[node];

		/* Find the next lattice endpoint */
		next_lattice_ept = EDpool_iommu_to_cpu(ohcip,
					Get_ED(lattice_ept->hced_next));

		/*
		 * Update this endpoint to point to the next one in the
		 * lattice.
		 */
		Set_ED(ept->hced_next, Get_ED(lattice_ept->hced_next));

		/* Insert this endpoint into the lattice */
		Set_ED(lattice_ept->hced_next, EDpool_cpu_to_iommu(ohcip, ept));

		/* Update the previous pointer */
		Set_ED(ept->hced_prev, EDpool_cpu_to_iommu(ohcip, lattice_ept));

		/* Update the previous pointer of ept->hced_next */
		if ((next_lattice_ept != NULL) &&
			(!((Get_ED(next_lattice_ept->hced_flag))
						& HC_EPT_STATIC))) {

			Set_ED(next_lattice_ept->hced_prev,
				EDpool_cpu_to_iommu(ohcip, ept));
		}
	}
}


/*
 * ohci_modify_sKip_bit:
 *
 * Modify the sKip bit on the Host Controller (HC) Endpoint Descriptor (ED).
 */
static int
ohci_modify_sKip_bit(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				skip_bit_t  action)
{
	hc_ed_t *ept = pp->pp_ept;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_modify_sKip_bit: action = 0x%x", action);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	if (action == CLEAR_sKip) {
		/*
		 * If the skip bit is to be cleared, just clear it.
		 * there shouldn't be any race condition problems.
		 * If the host controller reads the bit before the
		 * driver has a chance to set the bit, the bit will
		 * be reread on the next frame.
		 */
		Set_ED(ept->hced_ctrl, (Get_ED(ept->hced_ctrl) & ~HC_EPT_sKip));
	} else {
		/*
		 * The action is to set the skip bit.  In order to
		 * be sure that the HCD has seen the sKip bit, wait
		 * for the next start of frame.
		 */
		/* Set the bit */
		Set_ED(ept->hced_ctrl, (Get_ED(ept->hced_ctrl) | HC_EPT_sKip));

		mutex_exit(&pp->pp_mutex);

		/* Wait for the next SOF */
		if (ohci_wait_for_sof(ohcip) != USB_SUCCESS) {

			USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "ohci_modify_sKip_bit: No SOF's have started");

			mutex_enter(&pp->pp_mutex);
			return (USB_FAILURE);
		}

		mutex_enter(&pp->pp_mutex);

		/*
		 * The endpoint processing has stopped. However, there
		 * may be finished Transfer Descriptor (TD) on the done
		 * list for this endpoint. In other words, DelayInterrupt
		 * bit may be set in the TD's. When the  interrupt does
		 * eventually occur, the callbacks associated with these
		 * TD's must not be generated.
		 */
	}

	return (USB_SUCCESS);
}


/*
 * ohci_remove_ed:
 *
 * Remove the Endpoint Descriptor (ED) from the Host Controller's appropriate
 * endpoint list.
 */
static void
ohci_remove_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	uchar_t attributes;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_remove_ed:");

	attributes = pp->pp_pipe_handle->p_endpoint->bmAttributes &
							USB_EPT_ATTR_MASK;
	switch (attributes) {
		case USB_EPT_ATTR_CONTROL:
			ohci_remove_ctrl_ed(ohcip, pp);
			break;
		case USB_EPT_ATTR_BULK:
			ohci_remove_bulk_ed(ohcip, pp);
			break;
		case USB_EPT_ATTR_INTR:
		case USB_EPT_ATTR_ISOCH:
			ohci_remove_periodic_ed(ohcip, pp);
			break;
	}
}


/*
 * ohci_remove_ctrl_ed:
 *
 * Remove a control Endpoint Descriptor (ED) from the Host Controller's (HC)
 * control endpoint list.
 */
static void
ohci_remove_ctrl_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;	/* Endpoint to be removed */

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_remove_ctrl_ed:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	/*
	 * The control list should already be stopped
	 */

	ASSERT(!(Get_OpReg(hcr_control) & HCR_CONTROL_CLE));

	/* Detach the endpoint from the list that it's on */
	ohci_detach_ed_from_list(ohcip, ept, USB_EPT_ATTR_CONTROL);

	/*
	 * If next endpoint pointed by endpoint to be removed is not NULL
	 * then set current control pointer to the next endpoint pointed by
	 * endpoint to be removed. Otherwise set current control pointer to
	 * the beginning of the control list.
	 */
	if (Get_ED(ept->hced_next) != NULL) {
		Set_OpReg(hcr_ctrl_curr, Get_ED(ept->hced_next));
	} else {
		Set_OpReg(hcr_ctrl_curr, Get_OpReg(hcr_ctrl_head));
	}

	/* Reenable the control list */
	Set_OpReg(hcr_control, (Get_OpReg(hcr_control) | HCR_CONTROL_CLE));

	ohci_insert_ed_on_reclaim_list(ohcip, pp);
}


/*
 * ohci_remove_bulk_ed:
 *
 * Remove free the  bulk Endpoint Descriptor (ED) from the Host Controller's
 * (HC) bulk endpoint list.
 */
static void
ohci_remove_bulk_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;	/* ept to be removed */

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_remove_bulk_ed:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	/*
	 * The bulk list should already be stopped
	 */
	ASSERT(!(Get_OpReg(hcr_control) & HCR_CONTROL_BLE));

	/* Detach the endpoint from the bulk list */
	ohci_detach_ed_from_list(ohcip, ept, USB_EPT_ATTR_BULK);

	/*
	 * If next endpoint pointed by endpoint to be removed is not NULL
	 * then set current bulk pointer to the next endpoint pointed by
	 * endpoint to be removed. Otherwise set current bulk pointer to
	 * the beginning of the bulk list.
	 */
	if (Get_ED(ept->hced_next) != NULL) {
		Set_OpReg(hcr_bulk_curr, Get_ED(ept->hced_next));
	} else {
		Set_OpReg(hcr_bulk_curr, Get_OpReg(hcr_bulk_head));
	}

	/* Re-enable the bulk list */
	Set_OpReg(hcr_control, (Get_OpReg(hcr_control) | HCR_CONTROL_BLE));

	ohci_insert_ed_on_reclaim_list(ohcip, pp);
}


/*
 * ohci_remove_periodic_ed:
 *
 * Set up an periodic endpoint to be removed from the Host Controller's (HC)
 * interrupt lattice tree. The Endpoint Descriptor (ED) will be freed in the
 * interrupt handler.
 */
static void
ohci_remove_periodic_ed(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;		/* ept to be removed */

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_remove_periodic_ed:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	ASSERT((Get_ED(ept->hced_tailp) & HC_EPT_TD_TAIL) ==
		(Get_ED(ept->hced_headp) & HC_EPT_TD_HEAD));

	/* Store the node number */
	Set_ED(ept->hced_node, pp->pp_node);

	/* Remove the endpoint from interrupt lattice tree */
	ohci_detach_ed_from_list(ohcip, ept, USB_EPT_ATTR_INTR);

	ohci_insert_ed_on_reclaim_list(ohcip, pp);
}


/*
 * ohci_insert_ed_on_reclaim_list:
 *
 * Insert Endpoint onto the reclaim list
 */
static void
ohci_insert_ed_on_reclaim_list(openhci_state_t *ohcip, ohci_pipe_private_t *pp)
{
	hc_ed_t *ept = pp->pp_ept;	/* ept to be removed */
	hc_ed_t *head_ept;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	/* Insert the endpoint onto the reclamation list */
	if (ohcip->ohci_reclaim_list == NULL) {
		ohcip->ohci_reclaim_list = ept;
	} else {
		head_ept = ohcip->ohci_reclaim_list;
		Set_ED(ept->hced_reclaim_next,
				EDpool_cpu_to_iommu(ohcip, head_ept));
		ohcip->ohci_reclaim_list = ept;
	}

	/* Enable the SOF interrupt */
	Set_OpReg(hcr_intr_enable, HCR_INTR_SOF);
}


/*
 * ohci_detach_ed_from_list:
 *
 * Remove the Endpoint Descriptor (ED) from the appropriate Host Controller's
 * (HC) endpoint list.
 */
static void
ohci_detach_ed_from_list(openhci_state_t *ohcip, hc_ed_t *ept,
					uint_t ept_type)
{
	hc_ed_t *prev_ept;	/* Previous endpoint */
	hc_ed_t *next_ept;	/* Endpoint after one to be removed */
	uint_t node;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_detach_ed_from_list:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	prev_ept = EDpool_iommu_to_cpu(ohcip, Get_ED(ept->hced_prev));
	next_ept = EDpool_iommu_to_cpu(ohcip, Get_ED(ept->hced_next));

	/*
	 * If there is no previous endpoint, then this
	 * endpoint is at the head of the endpoint list.
	 */
	if (prev_ept == NULL) {
		if (next_ept != NULL) {
			/*
			 * If this endpoint is the first element of the
			 * list and there is more  than one endpoint on
			 * the list then perform specific actions based
			 * on the type of endpoint list.
			 */
			switch (ept_type) {
				case USB_EPT_ATTR_CONTROL:
					/* Set the head of list to next ept */
					Set_OpReg(hcr_ctrl_head,
						Get_ED(ept->hced_next));

					/* Clear prev ptr of  next endpoint */
					Set_ED(next_ept->hced_prev,  NULL);
					break;
				case USB_EPT_ATTR_BULK:
					/* Set the head of list to next ept */
					Set_OpReg(hcr_bulk_head,
						Get_ED(ept->hced_next));

					/* Clear prev ptr of  next endpoint */
					Set_ED(next_ept->hced_prev, NULL);
					break;
				case USB_EPT_ATTR_INTR:
					/*
					 * HCCA area should point
					 * directly to this ept.
					 */

					ASSERT(Get_ED(ept->hced_node)
							>= NUM_STATIC_NODES);

					/* Get the hcca interrupt table index */
					node = ohci_hcca_intr_index(Get_ED(ept->
								hced_node));

					/*
					 * Delete the ept from the
					 * bottom of the tree.
					 */
					Set_HCCA(ohcip->ohci_hccap->
						HccaIntTble[node],
						Get_ED(ept->hced_next));

					/*
					 * Update the previous pointer
					 * of ept->hced_next
					 */
					if (!(Get_ED(next_ept->hced_flag)
							& HC_EPT_STATIC)) {

						Set_ED(next_ept->hced_prev,
									NULL);
					}

					break;
				case USB_EPT_ATTR_ISOCH:
					break;
				default:
					break;
			}
		} else {
			/*
			 * If there was only one element on the list
			 * perform specific actions based on the type
			 * of the list.
			 */
			switch (ept_type) {
				case USB_EPT_ATTR_CONTROL:
					/* Set the head to NULL */
					Set_OpReg(hcr_ctrl_head, NULL);
					break;
				case USB_EPT_ATTR_BULK:
					/* Set the head to NULL */
					Set_OpReg(hcr_bulk_head, NULL);
					break;
				case USB_EPT_ATTR_INTR:
					break;
				case USB_EPT_ATTR_ISOCH:
					break;
				default:
					break;
			}
		}
	} else {
		/* The previous ept points to the next one */
		Set_ED(prev_ept->hced_next, Get_ED(ept->hced_next));

		/*
		 * Set the previous ptr of the next_ept to prev_ept
		 * if this isn't the last endpoint on the list
		 */
		if ((next_ept != NULL) &&
			(!(Get_ED(next_ept->hced_flag) & HC_EPT_STATIC))) {
			/* Set the previous ptr of the next one */
			Set_ED(next_ept->hced_prev, Get_ED(ept->hced_prev));
		}
	}
}


/*
 * ohci_deallocate_ed:
 *
 * Deallocate a Host Controller's (HC) Endpoint Descriptor (ED).
 */
void
ohci_deallocate_ed(openhci_state_t *ohcip, hc_ed_t *old_ed)
{
	gtd *dummy_gtd;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_deallocate_ed:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * The dummy Transfer Descriptor (TD) is deallocated. The
	 * Host Controller (HC) endpoint shouldn't have any gtd's
	 * except for the dummy.
	 */
	ASSERT((Get_ED(old_ed->hced_tailp) & HC_EPT_TD_TAIL) ==
		(Get_ED(old_ed->hced_headp) & HC_EPT_TD_HEAD));

	dummy_gtd =  (TDpool_iommu_to_cpu(ohcip, Get_ED(old_ed->hced_headp)));

	ohci_deallocate_gtd(ohcip, dummy_gtd);

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_deallocate_ed: Deallocated 0x%p", (void *)old_ed);

	bzero((void *)old_ed, sizeof (hc_ed_t));
	Set_ED(old_ed->hced_ctrl, HC_EPT_BLANK);
}


/*
 * Transfer Descriptor manipulations functions
 */


/*
 * ohci_initialize_dummy:
 *
 * An Endpoint Descriptor (ED) has a  dummy Transfer Descriptor (TD) on the
 * end of its TD list. Initially, both the head and tail pointers of the ED
 * point to the dummy TD.
 */
static int
ohci_initialize_dummy(openhci_state_t *ohcip, hc_ed_t *ept)
{
	gtd *dummy;

	/* Obtain a  dummy TD */
	dummy = ohci_allocate_td_from_pool(ohcip);

	if (dummy == NULL) {
		return (USB_NO_RESOURCES);
	}

	/*
	 * Both the head and tail pointers of an
	 * ED point to this new dummy TD.
	 */
	Set_ED(ept->hced_headp,
		(TDpool_cpu_to_iommu(ohcip, dummy)));
	Set_ED(ept->hced_tailp,
		(TDpool_cpu_to_iommu(ohcip, dummy)));

	return (USB_SUCCESS);
}


/*
 * ohci_common_ctrl_routine:
 */
static int
ohci_common_ctrl_routine(usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t		bmRequestType,
				uchar_t		bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t		*data,
				uint_t		usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	int error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_common_ctrl_routine: Flags = %x", usb_flags);

	/*
	 * Check and handle root hub control request.
	 */
	if (pipe_handle->p_usb_device->usb_addr == ROOT_HUB_ADDR) {

		error = ohci_handle_root_hub_request(pipe_handle,
						bmRequestType,
						bRequest,
						wValue,
						wIndex,
						wLength,
						data,
						usb_flags);

		return (error);
	}

	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/*
	 *  Check whether pipe is already in use.
	 */
	if (pp->pp_state != OPENED) {
		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_common_ctrl_routine: Pipe is in use");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		return (USB_FAILURE);
	}

	/* Insert the td's on the endpoint */
	error = ohci_insert_ctrl_td(ohcip, pipe_handle,
				bmRequestType,
				bRequest,
				wValue,
				wIndex,
				wLength,
				data, usb_flags);

	if (error != USB_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
			"ohci_common_ctrl_routine: No resources");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		return (error);
	}

	/* Set pipe state to INUSE */
	pp->pp_state = INUSE;

	/* Free the mblk */
	if (data) {
		freeb(data);
	}

	/* Indicate that the control list is filled */
	Set_OpReg(hcr_cmd_status, (Get_OpReg(hcr_cmd_status) | HCR_STATUS_CLF));

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	return (error);
}


/*
 * ohci_insert_ctrl_td:
 *
 * Create a Transfer Descriptor (TD) and a data buffer for a control endpoint.
 */
static int
ohci_insert_ctrl_td(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t bmRequestType,
				uchar_t bRequest,
				uint16_t wValue,
				uint16_t wIndex,
				uint16_t wLength,
				mblk_t *data, uint_t usb_flags)
{
	ohci_trans_wrapper_t *tw;
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_ctrl_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Allocate a transaction wrapper */
	tw = ohci_create_transfer_wrapper(ohcip, pp,
					wLength + SETUP_SIZE,
					usb_flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_insert_ctrl_td: TW allocation failed");

		return (USB_NO_RESOURCES);
	}

	/*
	 * Initialize the callback and any callback
	 * data for when the td completes.
	 */
	tw->tw_handle_td = ohci_handle_ctrl_td;

	tw->tw_handle_callback_value = NULL;

	if ((ohci_create_setup_pkt(ohcip, pp, tw,
					bmRequestType,
					bRequest,
					wValue,
					wIndex,
					wLength,
					data, usb_flags)) !=
					USB_SUCCESS) {

		/* Free the transfer wrapper */
		ohci_free_tw(ohcip, tw);

		return (USB_NO_RESOURCES);
	}

	tw->tw_ctrl_state = SETUP;

	return (USB_SUCCESS);
}


/*
 * ohci_create_setup_pkt:
 *
 * create a setup packet to initiate a control transfer.
 *
 * We have seen the case where devices fail if there is more than one
 * control transfer to the device within a frame.  At the DWG meetings,
 * we heard of devices failing in this way, and in our lab, we obtained
 * a trace of  a device that failed when the setup and the data phases
 * occurred in the same frame.We believe we've experienced this problem
 * on several different devices, including both low & high speed devices.
 * We at first tried adding  delays within the hub driver.   This only
 * worked some of the time & we still experienced intermittment problems.
 * We are forced to implement the solution where the data td isn't added
 * to the endpoint until the setup phases has completed.  Similarly, the
 * status TD isn't added to the endpoint until the previous phase has been
 * completed.
 */
static int
ohci_create_setup_pkt(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp,
				ohci_trans_wrapper_t *tw,
				uchar_t bmRequestType,
				uchar_t bRequest,
				uint16_t wValue,
				uint16_t wIndex,
				uint16_t wLength,
				mblk_t *data, uint_t usb_flags)
{
	int	sdata;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_create_setup_pkt: 0x%x 0x%x 0x%x 0x%x 0x%x 0%p 0%x",
	    bmRequestType, bRequest, wValue, wIndex, wLength,
	    (void *)data, usb_flags);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(tw != NULL);

	/* Create the first four bytes of the setup packet */
	sdata = (bmRequestType << 24) | (bRequest << 16) |
		(((wValue >> 8) | (wValue << 8)) & 0x0000FFFF);

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_create_setup_pkt: sdata = 0x%x", sdata);

	ddi_put32(tw->tw_accesshandle, (uint_t *)tw->tw_buf, sdata);

	/* Create the second four bytes */
	sdata = (uint32_t)(((((wIndex >> 8) |
		(wIndex << 8)) << 16) & 0xFFFF0000) |
		(((wLength >> 8) | (wLength << 8)) & 0x0000FFFF));

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_create_setup_pkt: sdata = 0x%x", sdata);

	ddi_put32(tw->tw_accesshandle,
		(uint_t *)(tw->tw_buf + sizeof (uint_t)), sdata);

	/*
	 * The TD's are placed on the ED one at a time.
	 * Once this TD is placed on the done list, the
	 * data or status phase TD will be enqueued.
	 */
	if ((ohci_insert_hc_td(ohcip,
		HC_GTD_T_TD_0|HC_GTD_1I,
		tw->tw_cookie.dmac_address,
		SETUP_SIZE,
		pp,
		tw)) != USB_SUCCESS) {

		return (USB_NO_RESOURCES);
	}

	USB_DPRINTF_L3(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "Create_setup: pp 0x%p", (void *)pp);

	/*
	 * If this control transfer has a data phase, record the
	 * direction. If the data phase is an OUT transaction,
	 * copy the data into the buffer of the transfer wrapper.
	 */
	if (wLength != 0) {
		/* There is a data stage.  Find the direction */
		if (bmRequestType & USB_DEV_REQ_DEVICE_TO_HOST) {
			tw->tw_direction = HC_GTD_IN;
		} else {
			tw->tw_direction = HC_GTD_OUT;

			/* Copy the data into the message */
			ddi_rep_put8(tw->tw_accesshandle,
				data->b_rptr,
				(uint8_t *)(tw->tw_buf + SETUP_SIZE),
				wLength,
				DDI_DEV_AUTOINCR);

		}
	}

	/* Start the timer for this control transfer */
	ohci_start_xfer_timer(ohcip, pp, tw);

	return (USB_SUCCESS);
}


/*
 * ohci_common_bulk_routine:
 */
static int
ohci_common_bulk_routine(usb_pipe_handle_impl_t  *pipe_handle,
				size_t		length,
				mblk_t		*data,
				uint_t		usb_flags)
{
	openhci_state_t *ohcip =
		ohci_obtain_state(pipe_handle->p_usb_device->usb_root_hub_dip);
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	int error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
	    "ohci_common_bulk_routine: Flags = %x", usb_flags);

	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	/*
	 *  Check whether pipe is in halted state.
	 */
	if (pp->pp_state == HALTED) {
		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_common_bulk_routine:"
		    "Pipe is in halted state, need pipe reset to continue");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		return (USB_FAILURE);
	}

	/* Add the TD into the Host Controller's bulk list */
	error = ohci_insert_bulk_td(ohcip, pipe_handle,
				ohci_handle_bulk_td,
				length,
				data, usb_flags);

	if (error != USB_SUCCESS) {
		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		USB_DPRINTF_L2(PRINT_MASK_HCDI, ohcip->ohci_log_hdl,
		    "ohci_common_bulk_routine: No resources");

		return (error);
	}

	/* Free the mblk */
	if (data) {
		freeb(data);
	}

	/* Indicate that the bulk list is filled */
	Set_OpReg(hcr_cmd_status, (Get_OpReg(hcr_cmd_status) | HCR_STATUS_BLF));

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	return (error);
}


/*
 * ohci_insert_bulk_td:
 *
 * Create a Transfer Descriptor (TD) and a data buffer for a bulk
 * endpoint.
 */
/* ARGSUSED */
static int
ohci_insert_bulk_td(openhci_state_t *ohcip,
	usb_pipe_handle_impl_t	*pipe_handle,
	ohci_handler_function_t tw_handle_td,
	size_t length,
	mblk_t *data, uint_t flags)
{
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	ohci_trans_wrapper_t *tw;
	int	pipe_dir;		/* pipe direction IN or OUT */
	int	tw_dir;			/* tw direction */
	uint_t	bulk_pkt_size, count;
	size_t	residue = 0, len = 0;
	ulong_t	ctrl = 0;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_bulk_td: length = 0x%x", length);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	/* Check the size of bulk request */
	if (length > ohci_bulk_transfer_size) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_insert_bulk_td: Bulk request size 0x%x is "
		    "more than 0x%x", length, ohci_bulk_transfer_size);

		return (USB_FAILURE);
	}

	/* Get the required bulk packet size */
	bulk_pkt_size = min(length, OHCI_MAX_TD_XFER_SIZE);

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_bulk_td: bulk_pkt_size = %d", bulk_pkt_size);

	/* Allocate a transfer wrapper */
	tw = ohci_create_transfer_wrapper(ohcip, pp, length, flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_insert_bulk_td: TW allocation failed");

		return (USB_NO_RESOURCES);
	}

	pipe_dir = pipe_handle->p_endpoint->
				bEndpointAddress & USB_EPT_DIR_MASK;

	/*
	 * Initialize the callback and any callback
	 * data required when the td completes.
	 */
	tw->tw_handle_td = tw_handle_td;
	tw->tw_handle_callback_value = NULL;
	tw_dir = (pipe_dir == USB_EPT_DIR_OUT) ? HC_GTD_OUT : HC_GTD_IN;
	tw->tw_direction = tw_dir;

	if (tw->tw_direction == HC_GTD_OUT) {

		ASSERT(data != NULL);

		/* Copy the data into the message */
		ddi_rep_put8(tw->tw_accesshandle,
				data->b_rptr,
				(uint8_t *)tw->tw_buf,
				length,
				DDI_DEV_AUTOINCR);
	}

	ctrl = ((~(HC_GTD_T)) & (tw_dir | HC_GTD_1I));

	if (tw->tw_flags & USB_FLAGS_SHORT_XFER_OK) {
		ctrl |= HC_GTD_R;
	}

	/*
	 * Calculate number of TDs to be insert into the bulk endpoint.
	 */
	tw->tw_num_tds = tw->tw_length / bulk_pkt_size;

	/* Calculate the residue data size */
	if (tw->tw_length % bulk_pkt_size) {

		residue = tw->tw_length % bulk_pkt_size;
		tw->tw_num_tds++;
	}

	/* Insert all the bulk TDs */
	for (count = 0; count < tw->tw_num_tds; count++) {

		/* Check for inserting residue data */
		if ((count == (tw->tw_num_tds - 1)) && (residue)) {

			bulk_pkt_size = residue;
		}

		/* Insert the TD onto the endpoint */
		if ((ohci_insert_hc_td(ohcip,
				ctrl,
				tw->tw_cookie.dmac_address + len,
				bulk_pkt_size,
				pp,
				tw)) != USB_SUCCESS) {

			/*
			 * Return failure to client driver if no TDs
			 * are inserted into the bulk endpoint since
			 * there are no TD resources available.
			 */
			if (count == 0) {
				/* Free the transfer wrapper */
				ohci_free_tw(ohcip, tw);

				return (USB_NO_RESOURCES);
			}

			tw->tw_num_tds = count;
			tw->tw_length  = len;
			break;
		}

		len = len + bulk_pkt_size;
	}

	/* Start the timer for this bulk transfer */
	ohci_start_xfer_timer(ohcip, pp, tw);

	return (USB_SUCCESS);
}


/*
 * ohci_insert_intr_td:
 *
 * Create a Transfer Descriptor (TD) and a data buffer for a interrupt
 * endpoint.
 */
int
ohci_insert_intr_td(openhci_state_t *ohcip,
	usb_pipe_handle_impl_t	*pipe_handle, uint_t flags,
	ohci_handler_function_t tw_handle_td,
	usb_opaque_t tw_handle_callback_value)
{
	ohci_trans_wrapper_t *tw;
	size_t	length;
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	uint_t ctrl = 0;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_intr_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Obtain the maximum transfer size */
	length = pp->pp_policy.pp_periodic_max_transfer_size;

	/* Allocate a transaction wrapper */
	tw = ohci_create_transfer_wrapper(ohcip, pp, length, flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_insert_intr_td: TW allocation failed");

		return (USB_NO_RESOURCES);
	}

	/*
	 * Initialize the callback and any callback
	 * data for when the td completes.
	 */
	tw->tw_handle_td = tw_handle_td;

	tw->tw_handle_callback_value = tw_handle_callback_value;

	if (tw->tw_flags & USB_FLAGS_SHORT_XFER_OK) {
		ctrl = ((~(HC_GTD_T)) & (HC_GTD_IN|HC_GTD_1I|HC_GTD_R));
	} else {
		ctrl = ((~(HC_GTD_T)) & (HC_GTD_IN|HC_GTD_1I));
	}
	/* Insert the td onto the endpoint */
	if ((ohci_insert_hc_td(ohcip,
			ctrl,
			tw->tw_cookie.dmac_address,
			length,
			pp,
			tw)) != USB_SUCCESS) {

		/* Free the transfer wrapper */
		ohci_free_tw(ohcip, tw);

		return (USB_NO_RESOURCES);
	}

	return (USB_SUCCESS);
}


/*
 * ohci_insert_isoc_out_tds:
 *
 * Handle isochronous out data.
 */
static int
ohci_insert_isoc_out_tds(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t	*pipe_handle,
			mblk_t *data,
			uint_t flags)
{
	ohci_trans_wrapper_t *tw;
	uint_t length;
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_insert_isoc_out_tds:");

	mutex_enter(&ohcip->ohci_int_mutex);

	/* Enable the periodic list processing */
	Set_OpReg(hcr_control,
		Get_OpReg(hcr_control) | HCR_CONTROL_PLE| HCR_CONTROL_IE);

	/* Wait for the next SOF */
	if (ohci_wait_for_sof(ohcip) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_insert_isoc_out_tds: No SOF's have started");

		mutex_exit(&ohcip->ohci_int_mutex);
		return (USB_FAILURE);
	}

	mutex_enter(&pp->pp_mutex);

	length = (uint32_t)(data->b_wptr - data->b_rptr);

	/* Allocate a transaction wrapper */
	tw = ohci_create_transfer_wrapper(ohcip, pp, length, flags);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_insert_isoc_out_tds: TW allocation failed");

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		return (USB_NO_RESOURCES);
	}

	/*
	 * Initialize the callback and any callback
	 * data for when the td completes.
	 */
	tw->tw_handle_td = ohci_handle_isoc_td;

	tw->tw_handle_callback_value = NULL;

	/*
	 * ohci_create_transfer_wrapper() an I/O buffer.
	 * Copy the data into buffer.
	 */
	ddi_rep_put8(tw->tw_accesshandle,
		data->b_rptr,
		(uint8_t *)(tw->tw_buf),
		length,
		DDI_DEV_AUTOINCR);

	/*
	 * Break the transfer up into parts that can
	 * go onto the ITD. For now let assume	that
	 * 8 * MaxPacketSize can be used for an ITD.
	 */

	/* Insert the td onto the endpoint */
	if ((ohci_insert_hc_td(ohcip,
			0,
			tw->tw_cookie.dmac_address,
			length,
			pp,
			tw)) != USB_SUCCESS) {

		/* Free the transfer wrapper */
		ohci_free_tw(ohcip, tw);

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		return (USB_NO_RESOURCES);
	}

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	return (USB_SUCCESS);
}


/*
 * ohci_insert_hc_td:
 *
 * Insert a Transfer Descriptor (TD) on an Endpoint Descriptor (ED).
 */
int
ohci_insert_hc_td(openhci_state_t *ohcip,
			uint_t hcgtd_ctrl,
			uint32_t hcgtd_iommu_cbp,
			size_t hcgtd_length,
			ohci_pipe_private_t *pp,
			ohci_trans_wrapper_t *tw)
{
	gtd *new_dummy;
	gtd *cpu_current_dummy;
	hc_ed_t *ept = pp->pp_ept;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	new_dummy = ohci_allocate_td_from_pool(ohcip);

	if (new_dummy == NULL) {
		return (USB_NO_RESOURCES);
	}

	/* Fill in the current dummy */
	cpu_current_dummy = (gtd *)
		(TDpool_iommu_to_cpu(ohcip, Get_ED(ept->hced_tailp)));

	/*
	 * Fill in the current dummy td and
	 * add the new dummy to the end.
	 */
	ohci_fill_in_td(ohcip,
			cpu_current_dummy,
			new_dummy,
			hcgtd_ctrl,
			hcgtd_iommu_cbp,
			hcgtd_length,
			pp,
			tw);

	/* Insert this td onto the tw */
	ohci_insert_td_on_tw(ohcip, tw, cpu_current_dummy);

	/*
	 * Add the new dummy to the ED's list.	When this occurs,
	 * the Host Controller will see the newly filled in dummy
	 * TD.
	 */
	Set_ED(ept->hced_tailp, (TDpool_cpu_to_iommu(ohcip, new_dummy)));

	return (USB_SUCCESS);
}


/*
 * ohci_allocate_td_from_pool:
 *
 * Allocate a Transfer Descriptor (TD) from the TD buffer pool.
 */
static gtd *
ohci_allocate_td_from_pool(openhci_state_t *ohcip)
{
	int i, ctrl;
	gtd *td;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Search for a blank Transfer Descriptor (TD)
	 * in the TD buffer pool.
	 */
	for (i = 0; i < td_pool_size; i ++) {
		ctrl = Get_TD(ohcip->ohci_td_pool_addr[i].hcgtd_ctrl);
		if (ctrl == HC_TD_BLANK) {
			break;
		}
	}

	if (i >= td_pool_size) {
		USB_DPRINTF_L2(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
		    "ohci_allocate_td_from_pool: TD exhausted");

		return (NULL);
	}

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_allocate_td_from_pool: Allocated %d", i);

	/* Create a new dummy for the end of the TD list */
	td = &ohcip->ohci_td_pool_addr[i];

	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_allocate_td_from_pool: td 0x%p", (void *)td);

	/* Mark the newly allocated TD as a dummy */
	Set_TD(td->hcgtd_ctrl, HC_TD_DUMMY);

	return (td);
}


/*
 * ohci_fill_in_td:
 *
 * Fill in the fields of a Transfer Descriptor (TD).
 */
static void
ohci_fill_in_td(openhci_state_t *ohcip,
			gtd *td,
			gtd *new_dummy,
			uint_t hcgtd_ctrl,
			uint32_t hcgtd_iommu_cbp,
			size_t hcgtd_length,
			ohci_pipe_private_t *pp,
			ohci_trans_wrapper_t *tw)
{
	/* Assert that the td to be filled in is a dummy */
	ASSERT(Get_TD(td->hcgtd_ctrl) == HC_TD_DUMMY);

	/* Clear the TD */
	bzero((char *)td, sizeof (gtd));

	/*
	 * If this is an isochronous TD, update the special itd
	 * portions. Otherwise, just update the control field.
	 */
	if ((pp->pp_pipe_handle->p_endpoint->bmAttributes &
		USB_EPT_ATTR_MASK) == USB_EPT_ATTR_ISOCH) {
		ohci_init_itd(ohcip, pp, hcgtd_iommu_cbp,
					hcgtd_ctrl, td);
	    } else {
		    /* Update the dummy with control information */
		    Set_TD(td->hcgtd_ctrl, hcgtd_ctrl);

		    /* Update the beginning of the buffer */
		    Set_TD(td->hcgtd_cbp, hcgtd_iommu_cbp);
	    }

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

	/* Print the td */
	ohci_print_td(ohcip, td);

	/* Fill in the wrapper portion of the TD */

	/* Set the transfer wrapper */
	ASSERT(tw != NULL);
	ASSERT(tw->tw_id != NULL);

	Set_TD(td->hcgtd_trans_wrapper, (uint32_t)tw->tw_id);
	Set_TD(td->hcgtd_next_td, NULL);
}


/*
 * ohci_init_itd:
 *
 * Initialize the Isochronous portion of the Transfer Descriptor (TD).
 */
static void
ohci_init_itd(openhci_state_t *ohcip,
			ohci_pipe_private_t *pp,
			uint32_t hcgtd_iommu_cbp,
			uint_t hcgtd_ctrl,
			gtd *td)
{
	uint_t	ctrl = hcgtd_ctrl & HC_ITD_MASK;
	uint_t	frame_no;
	int i;
	uint_t	offset_even, offset_odd;
	uint_t	buf;
	uint_t	wMaxPacketSize = pp->pp_pipe_handle->p_endpoint->wMaxPacketSize;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Set the frame number field */
	frame_no = Get_OpReg(hcr_frame_number) + 100;

	ctrl = ctrl | (frame_no & HC_ITD_FRAME);

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_init_itd:  Frame 0x%x", frame_no);

	ctrl = ctrl | HC_ITD_N | HC_GTD_1I;

	Set_TD(td->hcgtd_ctrl, ctrl);

	/*
	 * For an isochronous transfer, the hcgtd_cbp contains,
	 * the 4k page, and not the actual start of the buffer.
	 */
	Set_TD(td->hcgtd_cbp, ((int)hcgtd_iommu_cbp & HC_ITD_PAGE_MASK));

	/* Find the offset within the page */
	buf = (uint_t)(hcgtd_iommu_cbp -
			((int)hcgtd_iommu_cbp & HC_ITD_PAGE_MASK));

	/* The offsets are actually offsets into the page */
	for (i = 0; i < 4; i++) {
		offset_even = (buf + (wMaxPacketSize * (2*i))) &
						HC_ITD_EVEN_OFFSET;

		offset_odd = (uint32_t)
			(((buf + ((2*i + 1) * wMaxPacketSize))
				<< HC_ITD_OFFSET_SHIFT) & HC_ITD_ODD_OFFSET);

		Set_TD(td->hcgtd_offsets[i],
				(offset_odd | offset_even));

	}
}


/*
 * ohci_insert_td_on_tw:
 *
 * The transfer wrapper keeps a list of all Transfer Descriptors (TD) that
 * are allocated for this transfer. Insert a TD  onto this list. The  list
 * of TD's does not include the dummy TD that is at the end of the list of
 * TD's for the endpoint.
 */
static void
ohci_insert_td_on_tw(openhci_state_t *ohcip, ohci_trans_wrapper_t *tw, gtd *td)
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
 * ohci_traverse_tds:
 *
 * Traverse the list of TD's for an endpoint.  Since the endpoint is marked
 * as sKipped,	the Host Controller (HC) is no longer accessing these TD's.
 * Remove all the TD's that are attached to the endpoint.
 */
void
ohci_traverse_tds(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle)
{
	ohci_trans_wrapper_t *tw;
	hc_ed_t *ept;
	gtd	*tailp, *headp, *next;
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	uint32_t addr;

	ept = pp->pp_ept;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: ph = 0x%p ept = 0x%p",
	    (void *)pipe_handle, (void *)ept);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	addr = Get_ED(ept->hced_headp) & (uint32_t)HC_EPT_TD_HEAD;
	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: addr (head) = 0x%x", addr);

	headp = (gtd *)(TDpool_iommu_to_cpu(ohcip, addr));

	addr = Get_ED(ept->hced_tailp) & (uint32_t)HC_EPT_TD_TAIL;
	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: addr (tail) = 0x%x", addr);

	tailp = (gtd *)(TDpool_iommu_to_cpu(ohcip, addr));

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: cpu head = 0x%p cpu tail = 0x%p",
	    (void *)headp, (void *)tailp);

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: iommu head = 0x%x iommu tail = 0x%x",
	    TDpool_cpu_to_iommu(ohcip, headp),
	    TDpool_cpu_to_iommu(ohcip, tailp));

	/*
	 * Traverse the list of TD's that are currently on the endpoint.
	 * These TD's have not been processed and will not be processed
	 * because the endpoint processing is stopped.
	 */
	while (headp != tailp) {
		next = (gtd *)(TDpool_iommu_to_cpu(ohcip,
				(Get_TD(headp->hcgtd_next) & HC_EPT_TD_TAIL)));

		tw = (ohci_trans_wrapper_t *)
			OHCI_LOOKUP_ID((uint32_t)
				Get_TD(headp->hcgtd_trans_wrapper));

		/* Stop the the transfer timer */
		ohci_stop_xfer_timer(ohcip, tw, OHCI_REMOVE_XFER_ALWAYS);

		ohci_deallocate_gtd(ohcip, headp);
		headp = next;
	}

	/* Both head and tail pointers must be same */
	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: head = 0x%p tail = 0x%p",
	    (void *)headp, (void *)tailp);

	/* Update the pointer in the endpoint descriptor */
	Set_ED(ept->hced_headp, (TDpool_cpu_to_iommu(ohcip, headp)));

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: new head = 0x%x",
	    (TDpool_cpu_to_iommu(ohcip, headp)));

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_tds: tailp = 0x%x headp = 0x%x",
	    (Get_ED(ept->hced_tailp) & HC_EPT_TD_TAIL),
	    (Get_ED(ept->hced_headp) & HC_EPT_TD_HEAD));

	ASSERT((Get_ED(ept->hced_tailp) & HC_EPT_TD_TAIL) ==
		(Get_ED(ept->hced_headp) & HC_EPT_TD_HEAD));
}


/*
 * ohci_done_list_tds:
 *
 * There may be TD's on the done list that have not been processed yet. Walk
 * through these TD's and mark them as RECLAIM. All the mappings for the  TD
 * will be torn down, so the interrupt handle is alerted of this fact through
 * the RECLAIM flag.
 */
static void
ohci_done_list_tds(openhci_state_t *ohcip,
				usb_pipe_handle_impl_t *pipe_handle)
{
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	ohci_trans_wrapper_t *head_tw = pp->pp_tw_head;
	ohci_trans_wrapper_t *next_tw;
	gtd *head_td, *next_td;

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_done_list_tds:");

	/* Process the transfer wrappers for this pipe */
	next_tw = head_tw;
	while (next_tw != NULL) {
		head_td = (gtd *)next_tw->tw_hctd_head;
		next_td = head_td;

		if (head_td != NULL) {
			/*
			 * Walk through each TD for this transfer
			 * wrapper. If a TD still exists, then it
			 * is currently on the done list.
			 */
			while (next_td != NULL) {

				/* To free the TD, set TD state to RECLAIM */
				Set_TD(next_td->hcgtd_td_state, TD_RECLAIM);

				Set_TD(next_td->hcgtd_trans_wrapper, NULL);

				next_td = TDpool_iommu_to_cpu(ohcip,
						Get_TD(next_td->hcgtd_next_td));
			}
		}

		/* Stop the the transfer timer */
		ohci_stop_xfer_timer(ohcip, next_tw, OHCI_REMOVE_XFER_ALWAYS);

		next_tw = next_tw->tw_next;
	}
}


/*
 * ohci_deallocate_gtd:
 *
 * Deallocate a Host Controller's (HC) Transfer Descriptor (TD).
 */
void
ohci_deallocate_gtd(openhci_state_t *ohcip, gtd *old_gtd)
{
	ohci_trans_wrapper_t *tw;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_deallocate_gtd: old_gtd = 0x%p", (void *)old_gtd);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Obtain the transaction wrapper and tw will be
	 * NULL for the dummy and for the reclaim TD's.
	 */
	if ((Get_TD(old_gtd->hcgtd_ctrl) == HC_TD_DUMMY) ||
		(Get_TD(old_gtd->hcgtd_td_state) == TD_RECLAIM)) {
		tw = (ohci_trans_wrapper_t *)((uintptr_t)
		Get_TD(old_gtd->hcgtd_trans_wrapper));
		ASSERT(tw == NULL);
	} else {
		tw = (ohci_trans_wrapper_t *)
		OHCI_LOOKUP_ID((uint32_t)
		Get_TD(old_gtd->hcgtd_trans_wrapper));
		ASSERT(tw != NULL);
	}

	/*
	 * If this TD should be reclaimed, don't try to access its
	 * transfer wrapper.
	 */
	if ((Get_TD(old_gtd->hcgtd_td_state) != TD_RECLAIM) && (tw != NULL)) {
		gtd	*td = (gtd *)tw->tw_hctd_head;
		gtd	*test;

		/*
		 * Take this TD off the transfer wrapper's list since
		 * the pipe is FIFO, this must be the first TD on the
		 * list.
		 */
		ASSERT((gtd *)tw->tw_hctd_head == old_gtd);

		tw->tw_hctd_head = TDpool_iommu_to_cpu(ohcip,
					Get_TD(td->hcgtd_next_td));

		if (tw->tw_hctd_head != NULL) {
			test = (gtd *) tw->tw_hctd_head;
			ASSERT(Get_TD(test->hcgtd_ctrl) != HC_TD_DUMMY);
		}

		/*
		 * If the head becomes NULL, then there are no more
		 * active TD's for this transfer wrapper. Also	set
		 * the tail to NULL.
		 */
		if (tw->tw_hctd_head == NULL) {
			tw->tw_hctd_tail = NULL;
		} else {
			/*
			 * If this is the last td on the list, make
			 * sure it doesn't point to yet another td.
			 */
			if (tw->tw_hctd_head == tw->tw_hctd_tail) {
				td = (gtd *) tw->tw_hctd_head;

				ASSERT(Get_TD(td->hcgtd_next_td) == NULL);
			}
		}
	}

	bzero((char *)old_gtd, sizeof (gtd));

	Set_TD(old_gtd->hcgtd_cbp, 0xa);
	Set_TD(old_gtd->hcgtd_next, 0xa);
	Set_TD(old_gtd->hcgtd_buf_end, 0xa);
	Set_TD(old_gtd->hcgtd_trans_wrapper, NULL);
	Set_TD(old_gtd->hcgtd_td_state, 0xa);
	Set_TD(old_gtd->hcgtd_next_td, 0xa);

	Set_TD(old_gtd->hcgtd_ctrl, HC_TD_BLANK);

	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "Dealloc_td: td 0x%p", (void *)old_gtd);
}


/*
 * ohci_start_xfer_timer:
 *
 * Start the timer for the control or bulk transfers.
 */
/* ARGSUSED */
static void
ohci_start_xfer_timer(openhci_state_t *ohcip,
			ohci_pipe_private_t *pp,
			ohci_trans_wrapper_t *tw)
{
	usb_endpoint_descr_t	*eptd = pp->pp_pipe_handle->p_endpoint;

	USB_DPRINTF_L3(PRINT_MASK_LISTS,  ohcip->ohci_log_hdl,
	    "ohci_start_xfer_timer: tw = 0x%p", tw);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	/*
	 * The timeout handling is done only for control and bulk transfers.
	 */
	if (((eptd->bmAttributes & USB_EPT_ATTR_MASK) == ATTRIBUTES_CONTROL) ||
		((eptd->bmAttributes & USB_EPT_ATTR_MASK) == ATTRIBUTES_BULK)) {

		/*
		 * Get the timeout value which must be in terms of seconds
		 * and save it in the transfer wrapper.
		 */
		tw->tw_timeout_value = pp->pp_policy.pp_timeout_value?
		    pp->pp_policy.pp_timeout_value:OHCI_DEFAULT_XFER_TIMEOUT;

		/*
		 * Add this transfer wrapper into the transfer timeout list.
		 */
		if (ohcip->ohci_timeout_list) {
			tw->tw_timeout_next = ohcip->ohci_timeout_list;
		}

		ohcip->ohci_timeout_list = tw;

		ohci_start_timer(ohcip);
	}
}


/*
 * ohci_stop_xfer_timer:
 *
 * Stop the timer for the control or bulk transfers.
 */
static void
ohci_stop_xfer_timer(openhci_state_t *ohcip,
		ohci_trans_wrapper_t *tw,
		uint_t	flag)
{
	ohci_pipe_private_t *pp = tw->tw_pipe_private;
	usb_endpoint_descr_t	*eptd = pp->pp_pipe_handle->p_endpoint;
	timeout_id_t	timer_id;

	USB_DPRINTF_L3(PRINT_MASK_LISTS,  ohcip->ohci_log_hdl,
	    "ohci_stop_xfer_timer: tw = 0x%p", tw);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	/*
	 * The timeout handling is done only for control and bulk transfers.
	 */
	if (((eptd->bmAttributes & USB_EPT_ATTR_MASK) == ATTRIBUTES_CONTROL) ||
		((eptd->bmAttributes & USB_EPT_ATTR_MASK) == ATTRIBUTES_BULK)) {

		if (ohcip->ohci_timeout_list == NULL) {
			return;
		}

		switch (flag) {
			case OHCI_REMOVE_XFER_IFLAST:
				if (tw->tw_hctd_head != tw->tw_hctd_tail) {
					break;
				}

				if (((eptd->bmAttributes &
				    USB_EPT_ATTR_MASK) == ATTRIBUTES_CONTROL) &&
				    (tw->tw_ctrl_state != STATUS)) {

					break;
				}

				/* FALLTHRU */
			case OHCI_REMOVE_XFER_ALWAYS:
				ohci_remove_tw_from_timeout_list(ohcip, tw);

				if (ohcip->ohci_timeout_list == NULL) {
					timer_id = ohcip->ohci_timer_id;

					/* Reset the the timer id */
					ohcip->ohci_timer_id = 0;

					mutex_exit(&pp->pp_mutex);
					mutex_exit(&ohcip->ohci_int_mutex);

					(void) untimeout(timer_id);

					mutex_enter(&ohcip->ohci_int_mutex);
					mutex_enter(&pp->pp_mutex);
				}

				break;
			default:
				break;
		}
	}
}


/*
 * ohci_xfer_timeout_handler:
 *
 * Control or bulk transfer timeout handler.
 */
static void
ohci_xfer_timeout_handler(void *arg)
{
	openhci_state_t	*ohcip = (openhci_state_t *)arg;
	ohci_trans_wrapper_t *tw, *next;
	ohci_trans_wrapper_t *exp_xfer_list = NULL;
	gtd	*td;

	USB_DPRINTF_L3(PRINT_MASK_LISTS,  ohcip->ohci_log_hdl,
	    "ohci_xfer_timeout_handler: ohcip = 0x%p", ohcip);

	mutex_enter(&ohcip->ohci_int_mutex);

	/* Reset the the timer id */
	ohcip->ohci_timer_id = 0;

	/* Get the transfer timeout list head */
	tw = ohcip->ohci_timeout_list;

	/*
	 * Process ohci timeout list and look whether the timer
	 * is expired for any transfers. Create a temporary list
	 * of expired transfers and process them later.
	 */
	while (tw) {
		/* Get the transfer on the timeout list */
		next = tw->tw_timeout_next;

		if (--tw->tw_timeout_value <= 0) {

			ohci_remove_tw_from_timeout_list(ohcip, tw);

			if (exp_xfer_list) {
				tw->tw_timeout_next = exp_xfer_list;
			}

			exp_xfer_list = tw;
		}

		tw = next;
	}

	/* Get the expired transfer timeout list head */
	tw = exp_xfer_list;

	/*
	 * Process the expired transfers by notifing the corrsponding
	 * client driver through the exception callback.
	 */
	while (tw) {
		/* Get the transfer on the expired transfer timeout list */
		next = tw->tw_timeout_next;

		td = tw->tw_hctd_head;

		while (td != NULL) {
			/* Set TD state to TIMEOUT */
			Set_TD(td->hcgtd_td_state, TD_TIMEOUT);

			/* Get the next TD from the wrapper */
			td = TDpool_iommu_to_cpu(ohcip,
				Get_TD(td->hcgtd_next_td));
		}

		ohci_handle_error(ohcip, tw->tw_hctd_head, USB_CC_TIMEOUT);

		tw = next;
	}

	ohci_start_timer(ohcip);
	mutex_exit(&ohcip->ohci_int_mutex);
}


/*
 * ohci_remove_tw_from_timeout_list:
 *
 * Remove Control or bulk transfer from the timeout list.
 */
static void
ohci_remove_tw_from_timeout_list(openhci_state_t *ohcip,
		ohci_trans_wrapper_t *tw)
{
	ohci_trans_wrapper_t *prev, *next;

	USB_DPRINTF_L3(PRINT_MASK_LISTS,  ohcip->ohci_log_hdl,
	    "ohci_remove_tw_from_timeout_list: tw = 0x%p", tw);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	if (ohcip->ohci_timeout_list == tw) {
		ohcip->ohci_timeout_list = tw->tw_timeout_next;
	} else {
		prev = ohcip->ohci_timeout_list;
		next = prev->tw_timeout_next;

		while (next && (next != tw)) {
			prev = next;
			next = next->tw_timeout_next;
		}

		if (next == tw) {
			prev->tw_timeout_next = next->tw_timeout_next;
		}
	}

	/* Reset the xfer timeout */
	tw->tw_timeout_next = NULL;
}


/*
 * ohci_start_timer:
 *
 * Start the ohci timer
 */
static void
ohci_start_timer(openhci_state_t *ohcip)
{
	USB_DPRINTF_L3(PRINT_MASK_LISTS,  ohcip->ohci_log_hdl,
	    "ohci_start_timer: ohcip = 0x%p", ohcip);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Start the global timer only if currently timer is not
	 * running and if there are any transfers on the timeout
	 * list. This timer will be per USB Host Controller.
	 */
	if ((!ohcip->ohci_timer_id) && (ohcip->ohci_timeout_list)) {
		ohcip->ohci_timer_id = timeout(ohci_xfer_timeout_handler,
			(void *)ohcip, drv_usectohz(1000000));
	}
}


/*
 * Transfer Wrapper functions
 */


/*
 * ohci_create_transfer_wrapper:
 *
 * Create a Transaction Wrapper (TW) and this involves the allocating of DMA
 * resources.
 */
static ohci_trans_wrapper_t *
ohci_create_transfer_wrapper(
			openhci_state_t *ohcip,
			ohci_pipe_private_t *pp,
			size_t length,
			uint_t usb_flags)
{
	ddi_device_acc_attr_t	dev_attr;
	int result;
	size_t	real_length;
	uint_t	ccount;		/* Cookie count */
	ohci_trans_wrapper_t	*tw;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_create_transfer_wrapper: length = 0x%x flags = 0x%x",
	    length, usb_flags);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Allocate space for the transfer wrapper */
	tw = kmem_zalloc(sizeof (ohci_trans_wrapper_t), KM_NOSLEEP);

	if (tw == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS,  ohcip->ohci_log_hdl,
		    "ohci_create_transfer_wrapper: kmem_alloc failed");

		return (NULL);
	}

	tw->tw_length = length;

	/* Allocate the DMA handle */
	result = ddi_dma_alloc_handle(ohcip->ohci_dip,
					&ohcip->ohci_dma_attr,
					DDI_DMA_DONTWAIT,
					0,
					&tw->tw_dmahandle);

	if (result != DDI_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_create_transfer_wrapper: Alloc handle failed");

		kmem_free(tw, sizeof (ohci_trans_wrapper_t));

		return (NULL);
	}

	dev_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;

	/* The host controller will be little endian */
	dev_attr.devacc_attr_endian_flags  = DDI_STRUCTURE_BE_ACC;
	dev_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/* Allocate the memory */
	result = ddi_dma_mem_alloc(tw->tw_dmahandle,
			tw->tw_length,
			&dev_attr,
			DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT,
			NULL,
			(caddr_t *)&tw->tw_buf,
			&real_length,
			&tw->tw_accesshandle);

	if (result != DDI_SUCCESS) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_create_transfer_wrapper: dma_mem_alloc fail");
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (ohci_trans_wrapper_t));

		return (NULL);
	}

	ASSERT(real_length >= length);

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
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_create_transfer_wrapper: Bind handle failed");
		ddi_dma_mem_free(&tw->tw_accesshandle);
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (ohci_trans_wrapper_t));

		return (NULL);
	}

	if (result == DDI_DMA_MAPPED) {
		/* The cookie count should be 1 */
		if (ccount != 1) {
			USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "create_transfer_wrapper: More than 1 cookie");

			result = ddi_dma_unbind_handle(tw->tw_dmahandle);
			ASSERT(result == DDI_SUCCESS);

			ddi_dma_mem_free(&tw->tw_accesshandle);
			ddi_dma_free_handle(&tw->tw_dmahandle);
			kmem_free(tw, sizeof (ohci_trans_wrapper_t));

			return (NULL);
		}
	} else {
		ohci_decode_ddi_dma_addr_bind_handle_result(ohcip, result);

		result = ddi_dma_unbind_handle(tw->tw_dmahandle);
		ASSERT(result == DDI_SUCCESS);

		ddi_dma_mem_free(&tw->tw_accesshandle);
		ddi_dma_free_handle(&tw->tw_dmahandle);
		kmem_free(tw, sizeof (ohci_trans_wrapper_t));

		return (NULL);
	}

	/*
	 * Only allow one wrapper to be added at a time. Insert the
	 * new transaction wrapper into the list for this pipe.
	 */
	if (pp->pp_tw_head == NULL) {
		pp->pp_tw_head = tw;
		pp->pp_tw_tail = tw;
	} else {
		pp->pp_tw_tail->tw_next = tw;
		pp->pp_tw_tail = tw;
	}

	/* Store a back pointer to the pipe private structure */
	tw->tw_pipe_private = pp;

	/* Store the transfer type - synchronous or asynchronous */
	tw->tw_flags = usb_flags;

	/* Get and Store 32bit ID */
	tw->tw_id = OHCI_GET_ID((void *)tw);

	ASSERT(tw->tw_id != NULL);

	return (tw);
}


/*
 * ohci_deallocate_tw:
 *
 * Deallocate of a Transaction Wrapper (TW) and this involves the freeing of
 * of DMA resources.
 */
void
ohci_deallocate_tw(openhci_state_t *ohcip,
		ohci_pipe_private_t *pp, ohci_trans_wrapper_t *tw)
{
	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_deallocate_tw:");

	/*
	 * If the transfer wrapper has no Host Controller (HC)
	 * Transfer Descriptors (TD) associated with it,  then
	 * remove the transfer wrapper. The transfers are done
	 * in FIFO order, so this should be the first transfer
	 * wrapper on the list.
	 */
	if (tw->tw_hctd_head != NULL) {
		ASSERT(tw->tw_hctd_tail != NULL);

		return;
	}

	mutex_enter(&pp->pp_mutex);

	ASSERT(tw->tw_hctd_tail == NULL);
	ASSERT(pp->pp_tw_head == tw);

	/*
	 * If pp->pp_tw_head is NULL, set the tail also to NULL.
	 */
	pp->pp_tw_head = tw->tw_next;
	if (pp->pp_tw_head == NULL) {
		pp->pp_tw_tail = NULL;
	}

	ohci_free_tw(ohcip, tw);

	mutex_exit(&pp->pp_mutex);
}


/*
 * ohci_free_dma_resources:
 *
 * Free dma resources of a Transfer Wrapper (TW) and also free the TW.
 */
static void
ohci_free_dma_resources(openhci_state_t *ohcip,
			usb_pipe_handle_impl_t *pipe_handle)
{
	ohci_pipe_private_t *pp =
			(ohci_pipe_private_t *)pipe_handle->p_hcd_private;
	ohci_trans_wrapper_t *head_tw = pp->pp_tw_head;
	ohci_trans_wrapper_t *next_tw, *tw;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_free_dma_resources: ph = 0x%p", (void *)pipe_handle);

	ASSERT(mutex_owned(&pp->pp_mutex));

	/* Process the Transfer Wrappers */
	next_tw = head_tw;
	while (next_tw != NULL) {
		tw = next_tw;
		next_tw = tw->tw_next;

		ohci_free_tw(ohcip, tw);

		USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_free_dma_resources: Free TW = 0x%p", (void *)tw);
	}

	/* Adjust the head and tail pointers */
	pp->pp_tw_head = NULL;
	pp->pp_tw_tail = NULL;
}


/*
 * ohci_free_tw:
 *
 * Free the Transfer Wrapper (TW).
 */
static void
ohci_free_tw(openhci_state_t *ohcip, ohci_trans_wrapper_t *tw)
{
	int rval;

	USB_DPRINTF_L4(PRINT_MASK_ALLOC, ohcip->ohci_log_hdl,
	    "ohci_free_tw:");

	ASSERT(tw != NULL);
	ASSERT(tw->tw_id != NULL);

	/* Free 32bit ID */
	OHCI_FREE_ID((uint32_t)tw->tw_id);

	rval = ddi_dma_unbind_handle(tw->tw_dmahandle);
	ASSERT(rval == DDI_SUCCESS);

	ddi_dma_mem_free(&tw->tw_accesshandle);
	ddi_dma_free_handle(&tw->tw_dmahandle);
	kmem_free(tw, sizeof (ohci_trans_wrapper_t));
}


/*
 * Interrupt Handling functions
 */


/*
 * ohci_intr:
 *
 * OpenHCI (OHCI) interrupt handling routine.
 */
static uint_t
ohci_intr(caddr_t arg)
{
	openhci_state_t 	*ohcip = (openhci_state_t *)arg;
	uint_t			intr;
	gtd			*done_head;
	ohci_save_intr_status_t	*ohci_intr_sts =
				&ohcip->ohci_save_intr_status;

	USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "Interrupt occurred");

	mutex_enter(&ohcip->ohci_int_mutex);

	/*
	 * Suppose if we switched to the polled mode from the normal
	 * mode when interrupt handler is executing then we  need to
	 * save the interrupt status information in the  polled mode
	 * to  avoid race conditions. The following flag will be set
	 * and reset on entering & exiting of ohci interrupt handler
	 * respectively.  This flag will be used in the  polled mode
	 * to check whether the interrupt handlr was running when we
	 * switched to the polled mode from the normal mode.
	 */
	ohci_intr_sts->ohci_intr_flag = OHCI_INTR_HANDLING;

	/* Temporarily turn off interrupts */
	Set_OpReg(hcr_intr_disable, HCR_INTR_MIE);

	/*
	 * Handle any missed ohci interrupt especially WriteDoneHead
	 * and SOF interrupts because of previous polled mode switch.
	 */
	ohci_handle_missed_intr(ohcip);

	/*
	 * Now process the actual ohci interrupt events  that caused
	 * invocation of this ohci interrupt handler.
	 */

	/*
	 * Updating the WriteDoneHead interrupt:
	 *
	 * (a) Host Controller
	 *
	 *	- First Host controller (HC) checks  whether WDH bit
	 *	  in the interrupt status register is cleared.
	 *
	 *	- If WDH bit is cleared then HC writes new done head
	 *	  list information into the HCCA done head field.
	 *
	 *	- Set WDH bit in the interrupt status register.
	 *
	 * (b) Host Controller Driver (HCD)
	 *
	 *	- First read the interrupt status register. The HCCA
	 *	  done head and WDH bit may be set or may not be set
	 *	  while reading the interrupt status register.
	 *
	 *	- Read the  HCCA done head list. By this time may be
	 *	  HC has updated HCCA done head and  WDH bit in ohci
	 *	  interrupt status register.
	 *
	 *	- If done head is non-null and if WDH bit is not set
	 *	  then Host Controller has updated HCCA  done head &
	 *	  WDH bit in the interrupt stats register in between
	 *	  reading the interrupt status register & HCCA  done
	 *	  head. In that case, definitely WDH bit will be set
	 *	  in the interrupt status register & driver can take
	 *	  it for granted.
	 *
	 * Now read the Interrupt Status & Interrupt enable register
	 * to determine the exact interrupt events.
	 */
	intr = ohci_intr_sts->ohci_curr_intr_sts =
			(Get_OpReg(hcr_intr_status) &
					Get_OpReg(hcr_intr_enable));

	/* Update kstat values */
	OHCI_DO_INTRS_STATS(ohcip, intr);

	/*
	 * Read and Save the HCCA DoneHead value.
	 */
	done_head = ohci_intr_sts->ohci_curr_done_lst = (gtd *)
		(uintptr_t)(Get_HCCA(ohcip->ohci_hccap->HccaDoneHead)
						& HCCA_DONE_HEAD_MASK);

	USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "ohci_intr: Done head! 0x%x", done_head);

	/*
	 * Look at the HccaDoneHead & if it is non-zero, then a done
	 * list update interrupt is indicated.
	 */
	if (done_head != NULL) {

		/*
		 * Check for the  WriteDoneHead interrupt bit in the
		 * interrupt condition and set the WriteDoneHead bit
		 * in the interrupt events if it is not set.
		 */
		if (!(intr & HCR_INTR_WDH)) {
			intr |= HCR_INTR_WDH;
		}
	}

	/*
	 * We could have gotten a spurious interrupts. If so, do not
	 * claim it.  This is quite  possible on some  architectures
	 * where more than one PCI slots share the IRQs.  If so, the
	 * associated driver's interrupt routine may get called even
	 * if the interrupt is not meant for them.
	 *
	 * By unclaiming the interrupt, the other driver gets chance
	 * to service its interrupt.
	 */
	if (!intr) {

		/* Reset the interrupt handler flag */
		ohci_intr_sts->ohci_intr_flag &=
				~OHCI_INTR_HANDLING;

		Set_OpReg(hcr_intr_enable, HCR_INTR_MIE);
		mutex_exit(&ohcip->ohci_int_mutex);
		return (DDI_INTR_UNCLAIMED);
	}

	USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "Interrupt status 0x%x", intr);

	if (intr & HCR_INTR_UE) {
		USB_DPRINTF_L2(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Unrecoverable error");

		if (ohci_handle_ue(ohcip) == USB_SUCCESS) {

			/* Reset the interrupt handler flag */
			ohci_intr_sts->ohci_intr_flag &=
					~OHCI_INTR_HANDLING;

			mutex_exit(&ohcip->ohci_int_mutex);

			return (DDI_INTR_CLAIMED);
		}
	}

	if (intr & HCR_INTR_SOF) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Start of Frame");

		Set_OpReg(hcr_intr_disable, HCR_INTR_SOF);

		if (ohcip->ohci_reclaim_list != NULL) {

			ohci_handle_endpoint_reclamation(ohcip);
		}

		/*
		 * Call cv_broadcast on every SOF interrupt to wakeup
		 * all the threads that are waiting the SOF.  Calling
		 * cv_broadcast on every SOF has no effect even if no
		 * threads are waiting for the SOF.
		 */

		cv_broadcast(&ohcip->ohci_SOF_cv);
	}

	if (intr & HCR_INTR_SO) {
		USB_DPRINTF_L2(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Schedule overrun");
	}

	if (intr & HCR_INTR_WDH) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Done Head");

		/*
		 * Currently if we are processing one  WriteDoneHead
		 * interrupt  and also if we  switched to the polled
		 * mode at least once  during this time,  then there
		 * may be chance that  Host Controller generates one
		 * more Write DoneHead or Start of Frame  interrupts
		 * for the normal since the polled code clears WDH &
		 * SOF interrupt bits before returning to the normal
		 * mode. Under this condition, we must not clear the
		 * HCCA done head field & also we must not clear WDH
		 * interrupt bit in the interrupt  status register.
		 */
		if (done_head == (gtd *)(uintptr_t)
			(Get_HCCA(ohcip->ohci_hccap->HccaDoneHead)
					& HCCA_DONE_HEAD_MASK)) {

			/* Reset the done head to NULL */
			Set_HCCA(ohcip->ohci_hccap->HccaDoneHead, NULL);
		} else {
			intr &= ~HCR_INTR_WDH;
		}

		/* Clear the current done head field */
		ohci_intr_sts->ohci_curr_done_lst = NULL;

		ohci_traverse_done_list(ohcip, done_head);
	}

	if (intr & HCR_INTR_RD) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Resume Detected");
	}

	if (intr & HCR_INTR_FNO) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Frame overflow");
	}

	if (intr & HCR_INTR_RHSC) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Root hub status change");

		ohci_handle_root_hub_status_change(ohcip);
	}

	if (intr & HCR_INTR_OC) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_intr: Change ownership");

	}

	/* Acknowledge the interrupt */
	Set_OpReg(hcr_intr_status, intr);

	/* Clear the current interrupt event field */
	ohci_intr_sts->ohci_curr_intr_sts = 0;

	/*
	 * Reset the following flag indicating exiting the interrupt
	 * handler and this flag will be used in the polled  mode to
	 * do some extra processing.
	 */
	ohci_intr_sts->ohci_intr_flag &= ~OHCI_INTR_HANDLING;

	Set_OpReg(hcr_intr_enable, HCR_INTR_MIE);

	mutex_exit(&ohcip->ohci_int_mutex);

	USB_DPRINTF_L3(PRINT_MASK_ATTA,  ohcip->ohci_log_hdl,
	    "Interrupt handling completed");

	return (DDI_INTR_CLAIMED);
}


/*
 * ohci_handle_missed_intr:
 *
 * Handle any ohci missed interrupts because of polled mode switch.
 */
static void
ohci_handle_missed_intr(
	openhci_state_t	*ohcip)
{
	ohci_save_intr_status_t	*ohci_intr_sts =
				&ohcip->ohci_save_intr_status;
	gtd			*done_head;
	uint_t			intr;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Check whether we have  missed any ohci interrupts because
	 * of the polled mode switch during  previous ohci interrupt
	 * handler execution. Only  Write Done Head & SOF interrupts
	 * saved in the polled mode. First process  these interrupts
	 * before processing actual interrupts that caused invocation
	 * of ohci interrupt handler.
	 */
	if (!ohci_intr_sts->ohci_missed_intr_sts) {

		/*
		 * No interrupts are missed, simply return.
		 */
		return;
	}

	USB_DPRINTF_L4(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "ohci_handle_missed_intr: Handle ohci missed interrupts");

	/*
	 * The functionality and importance of critical code section
	 * in the normal mode ohci  interrupt handler & its usage in
	 * the polled mode is explained below.
	 *
	 * (a) Normal mode:
	 *
	 *	- Set the flag  indicating that  processing critical
	 *	  code in ohci interrupt handler.
	 *
	 *	- Process the missed ohci interrupts by  copying the
	 *	  miised interrupt events and done  head list fields
	 *	  information to the critical interrupt event & done
	 *	  list fields.
	 *
	 *	- Reset the missed ohci interrupt events & done head
	 *	  list fields so that the new missed interrupt event
	 *	  and done head list information can be saved.
	 *
	 *	- All above steps will be executed  with in critical
	 *	  section of the  interrupt handler.Then ohci missed
	 *	  interrupt handler will be called to service missed
	 *	  ohci interrupts.
	 *
	 * (b) Polled mode:
	 *
	 *	- On entering the polled code,it checks for critical
	 *	  section code execution within the normal mode ohci
	 *	  interrupt handler.
	 *
	 *	- If the critical section code is executing in normal
	 *	  mode ohci interrupt handler and if copying of ohci
	 *	  missed interrupt events & done head list fields to
	 *	  the critical fields is finished then save the "any
	 *	  missed interrupt events & done head list"  because
	 *	  of current polled mode switch into "critical missed
	 *	  interrupt events & done list fields" instead actual
	 *	  missed events and done list fields.
	 *
	 *	- Otherwise save "any missed interrupt events & done
	 *	  list" because of this  current polled  mode switch
	 *	  in the actual missed  interrupt events & done head
	 *	  list fields.
	 */

	/*
	 * Set flag indicating that  interrupt handler is processing
	 * critical interrupt code,  so that polled mode code checks
	 * for this condition & will do extra processing as explained
	 * above in order to aviod the race conditions.
	 */
	ohci_intr_sts->ohci_intr_flag |= OHCI_INTR_CRITICAL;
	ohci_intr_sts->ohci_critical_intr_sts |=
			ohci_intr_sts->ohci_missed_intr_sts;

	if (ohci_intr_sts->ohci_missed_done_lst != NULL) {

		ohci_intr_sts->ohci_critical_done_lst =
			ohci_intr_sts->ohci_missed_done_lst;
	}

	ohci_intr_sts->ohci_missed_intr_sts = 0;
	ohci_intr_sts->ohci_missed_done_lst = NULL;
	ohci_intr_sts->ohci_intr_flag &= ~OHCI_INTR_CRITICAL;

	intr = ohci_intr_sts->ohci_critical_intr_sts;
	done_head = ohci_intr_sts->ohci_critical_done_lst;

	if (intr & HCR_INTR_SOF) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_handle_missed_intr: Start of Frame");

		if (ohcip->ohci_reclaim_list != NULL) {

			ohci_handle_endpoint_reclamation(ohcip);
		}

		/*
		 * Call cv_broadcast on every SOF interrupt to wakeup
		 * all the threads that are waiting the SOF.  Calling
		 * cv_broadcast on every SOF has no effect even if no
		 * threads are waiting for the SOF.
		 */
		cv_broadcast(&ohcip->ohci_SOF_cv);
	}

	if (intr & HCR_INTR_WDH) {
		USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_handle_missed_intr: Done Head");

		/* Clear the critical done head field */
		ohci_intr_sts->ohci_critical_done_lst = NULL;

		ohci_traverse_done_list(ohcip, done_head);
	}

	/* Clear the critical interrupt event field */
	ohci_intr_sts->ohci_critical_intr_sts = 0;
}


/*
 * ohci_handle_ue:
 *
 * Handling of Unrecoverable Error interrupt (UE).
 */
static int
ohci_handle_ue(openhci_state_t *ohcip)
{
	static	boolean_t	ue_flag = B_FALSE;
	hcr_regs_t		*ohci_save_regs;

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "ohci_handle_ue: Handling of UE interrupt");

	/*
	 * Perform UE recovery process on first UE error interrupt.
	 * Don't perform any more UE recovery process for subsequent
	 * UE errors. Under this condition perform debug enter with
	 * message saying that it is unrecoverable usb error.
	 */
	if (ue_flag) {
		USB_DPRINTF_L2(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
		    "ohci_handle_ue: No more usb ue error recovery");

		debug_enter((char *)NULL);
	}

	/*
	 * Allocate space for saving current Host Controller
	 * registers. Don't handle UE recovery if allocation
	 * fails.
	 */
	ohci_save_regs = (hcr_regs_t *)
		kmem_zalloc(sizeof (hcr_regs_t), KM_NOSLEEP);

	if (ohci_save_regs == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_INTR,  ohcip->ohci_log_hdl,
		    "ohci_handle_ue: kmem_alloc failed");

		return (USB_FAILURE);
	}

	/*
	 * Save the current registers.
	 */
	bcopy((void *)ohcip->ohci_regsp,
		(void *)ohci_save_regs, sizeof (hcr_regs_t));

	USB_DPRINTF_L4(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "ohci_handle_ue: Save reg = 0x%p", ohci_save_regs);

	Set_OpReg(hcr_cmd_status, HCR_STATUS_RESET);

	USB_DPRINTF_L2(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
	    "ohci_handle_ue: ue recovery in progress");

#ifdef	RIO
	prom_printf("ohci%d: unrecoverable usb error\n",
	    ddi_get_instance(ohcip->ohci_dip));
#endif	/* RIO */

	/* Wait for reset to complete */
	drv_usecwait(OHCI_RESET_TIMEWAIT);

	/*
	 * Restore previous saved HC register value
	 * into the current HC registers.
	 */
	Set_OpReg(hcr_transfer_ls, (uint32_t)
		ohci_save_regs->hcr_transfer_ls);

	Set_OpReg(hcr_periodic_strt, (uint32_t)
		ohci_save_regs->hcr_periodic_strt);

	Set_OpReg(hcr_frame_interval, (uint32_t)
		ohci_save_regs->hcr_frame_interval);

	Set_OpReg(hcr_done_head, (uint32_t)
		ohci_save_regs->hcr_done_head);

	Set_OpReg(hcr_bulk_curr, (uint32_t)
		ohci_save_regs->hcr_bulk_curr);

	Set_OpReg(hcr_bulk_head, (uint32_t)
		ohci_save_regs->hcr_bulk_head);

	Set_OpReg(hcr_ctrl_curr, (uint32_t)
		ohci_save_regs->hcr_ctrl_curr);

	Set_OpReg(hcr_ctrl_head, (uint32_t)
		ohci_save_regs->hcr_ctrl_head);

	Set_OpReg(hcr_periodic_curr, (uint32_t)
		ohci_save_regs->hcr_periodic_curr);

	Set_OpReg(hcr_HCCA, (uint32_t)
		ohci_save_regs->hcr_HCCA);

	Set_OpReg(hcr_intr_enable, (uint32_t)
		ohci_save_regs->hcr_intr_enable);

	Set_OpReg(hcr_cmd_status, (uint32_t)
		ohci_save_regs->hcr_cmd_status);

	Set_OpReg(hcr_control, (uint32_t)
		((ohci_save_regs->hcr_control &
			(~HCR_CONTROL_HCFS)) |
				(Get_OpReg(hcr_control))));

	/*
	 * Deallocate the space that allocated for saving
	 * HC registers.
	 */
	kmem_free((void *) ohci_save_regs, sizeof (hcr_regs_t));

	/*
	 * Set the Host Controller Functional
	 * State to Operational.
	 */
	Set_OpReg(hcr_control,
		((Get_OpReg(hcr_control) &
			(~HCR_CONTROL_HCFS)) |
				HCR_CONTROL_OPERAT));

	/* Wait 10ms for HC to start sending SOF */
	drv_usecwait(OHCI_RESET_TIMEWAIT);

	/*
	 * Set the flag indicating that
	 * UE recovery process is done.
	 */
	ue_flag = B_TRUE;

	Set_OpReg(hcr_intr_enable, HCR_INTR_MIE);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_endpoint_reclamation:
 *
 * Reclamation of Host Controller (HC) Endpoint Descriptors (ED).
 */
static void
ohci_handle_endpoint_reclamation(openhci_state_t *ohcip)
{
	hc_ed_t *reclaim_ed;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_endpoint_reclamation:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Deallocate all Endpoint Descriptors (ED) which are on the
	 * reclaimation list. These ED's are already removed from the
	 * interrupt lattice tree.
	 */

	while (ohcip->ohci_reclaim_list != NULL) {
		reclaim_ed = ohcip->ohci_reclaim_list;

		/* Get the next endpoint from the rec. list */
		ohcip->ohci_reclaim_list =
			EDpool_iommu_to_cpu(ohcip,
				Get_ED(reclaim_ed->hced_reclaim_next));

		/* Deallocate the endpoint */
		ohci_deallocate_ed(ohcip, reclaim_ed);
	}

	ASSERT(ohcip->ohci_reclaim_list == NULL);
}


/*
 * ohci_traverse_done_list:
 */
static void
ohci_traverse_done_list(openhci_state_t *ohcip, gtd *head_done_list)
{
	uint_t			state;		/* TD state */
	gtd			*td, *old_td;	/* TD pointers */
	int			error;		/* Error from TD */
	ohci_trans_wrapper_t	*tw = NULL;	/* Transfer wrapper */

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_traverse_done_list:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(head_done_list != NULL);

	/* Reverse the done list */
	td = ohci_reverse_done_list(ohcip, head_done_list);

	/* Traverse the list of transfer descriptors */
	while (td != NULL) {
		/* Check for TD state */
		state = Get_TD(td->hcgtd_td_state);

		USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_traverse_done_list:\n\t"
		    "td = 0x%p	state = 0x%x", (void *)td, state);

		/*
		 * Obtain the  transfer wrapper only  if the TD is
		 * not marked as RECLAIM.
		 *
		 * A TD that is marked as  RECLAIM has had its DMA
		 * mappings, ED, TD and pipe private structure are
		 * ripped down. Just deallocate this TD.
		 */
		if (state != TD_RECLAIM) {

			tw = (ohci_trans_wrapper_t *)
				OHCI_LOOKUP_ID((uint32_t)
					Get_TD(td->hcgtd_trans_wrapper));

			ASSERT(tw != NULL);

			USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
			    "ohci_traverse_done_list: TW = 0x%p", tw);
		}

		/*
		 * Don't process the TD if its  state is marked as
		 * either RECLAIM or TIMEOUT.
		 *
		 * A TD that is marked as TIMEOUT has already been
		 * processed by TD timeout handler & client driver
		 * has been informed through exeception callback.
		 */
		if ((state != TD_RECLAIM) && (state != TD_TIMEOUT)) {

			/* Look at the error status */
			error = ohci_parse_error(ohcip, td);

			if (error == 0) {
				ohci_handle_normal_td(ohcip, td, tw);
			} else {
				/* handle the error condition */
				ohci_handle_error(ohcip, td, error);
			}
		} else {
			USB_DPRINTF_L3(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
			    "ohci_traverse_done_list: TD State = %d", state);
		}

		/*
		 * Save a pointer to the current transfer descriptor */
		old_td = td;

		td = TDpool_iommu_to_cpu(ohcip,
				Get_TD(td->hcgtd_next));

		/* Deallocate this transfer descriptor */
		ohci_deallocate_gtd(ohcip, old_td);

		/*
		 * Deallocate the transfer wrapper if there are no more
		 * TD's for the transfer wrapper.  ohci_deallocate_tw()
		 * will  not deallocate the tw for a periodic  endpoint
		 * since it will always have a TD attached to it.
		 *
		 * An TD that is marked as reclaim doesn't have a  pipe
		 * or a TW associated with it anymore so don't call this
		 * function.
		 */
		if (state != TD_RECLAIM) {
			ASSERT(tw != NULL);
			ohci_deallocate_tw(ohcip, tw->tw_pipe_private, tw);
		}
	}
}


/*
 * ohci_reverse_done_list:
 *
 * Reverse the order of the Transfer Descriptor (TD) Done List.
 */
static gtd *
ohci_reverse_done_list(openhci_state_t *ohcip, gtd *head_done_list)
{
	gtd *cpu_new_tail, *cpu_new_head, *cpu_save;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_reverse_done_list:");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(head_done_list != NULL);

	/* At first, both the tail and head pointers point to the same elem */
	cpu_new_tail = cpu_new_head =
			TDpool_iommu_to_cpu(ohcip, (uintptr_t)head_done_list);

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
 * ohci_handle_normal_td:
 */
static void
ohci_handle_normal_td(openhci_state_t *ohcip, gtd *td,
			ohci_trans_wrapper_t *tw)
{
	ohci_pipe_private_t	*pp;	/* Pipe private field */

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_normal_td:");

	ASSERT(tw != NULL);
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Obtain the pipe private structure */
	pp = tw->tw_pipe_private;

	mutex_enter(&pp->pp_mutex);

	(*tw->tw_handle_td)(ohcip, pp, tw, td,
		tw->tw_handle_callback_value);

	mutex_exit(&pp->pp_mutex);
}


/*
 * ohci_handle_ctrl_td:
 *
 * Handle a control Transfer Descriptor (TD).
 */
/* ARGSUSED */
static void
ohci_handle_ctrl_td(openhci_state_t *ohcip, ohci_pipe_private_t *pp,
	ohci_trans_wrapper_t *tw, gtd *td,
	void *tw_handle_callback_value)
{
	mblk_t *message;
	uint_t flags = 0;
	int	ctrl = 0;
	usb_pipe_handle_impl_t *usb_pp;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_ctrl_td: pp = 0x%p tw = 0x%p td = 0x%p state = 0x%x",
	    (void *)pp, (void *)tw, (void *)td, tw->tw_ctrl_state);

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	if (tw->tw_flags & USB_FLAGS_SLEEP) {
		flags = USB_FLAGS_SLEEP;
	}

	ASSERT(pp->pp_state == INUSE);

	/*
	 * A control transfer consists of three phases:
	 *
	 * Setup
	 * Data (optional)
	 * Status
	 *
	 * There is a TD per phase. A TD for a given phase isn't
	 * enqueued until the previous phase is finished. OpenHCI
	 * spec allows more than one  control transfer on a pipe
	 * within a frame. However, we've found that some devices
	 * can't handle this.
	 */
	switch (tw->tw_ctrl_state) {
		case SETUP:
			/*
			 * Enqueue either the data or the status
			 * phase depending on the length.
			 */

			/* If the length is 0, move to the status */
			if ((tw->tw_length - SETUP_SIZE) == 0) {

				/*
				 * There is no data stage,  then
				 * initiate status phase from the
				 * host.
				 */
				if ((ohci_insert_hc_td(ohcip,
					HC_GTD_IN|HC_GTD_T_TD_1|HC_GTD_1I,
					NULL, 0, pp,  tw)) != USB_SUCCESS) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
					    ohcip->ohci_log_hdl,
					    "ohci_handle_ctrl_td: No TD");

					usb_pp = pp->pp_pipe_handle;

					mutex_exit(&pp->pp_mutex);
					mutex_exit(&ohcip->ohci_int_mutex);

					usba_hcdi_callback(usb_pp,
						flags, NULL,
						0, USB_CC_UNSPECIFIED_ERR,
						USB_NO_RESOURCES);

					mutex_enter(&ohcip->ohci_int_mutex);
					mutex_enter(&pp->pp_mutex);

					return;
				}

				tw->tw_ctrl_state = STATUS;
			} else {
				if (tw->tw_flags & USB_FLAGS_SHORT_XFER_OK) {
					ctrl = HC_GTD_R;
				}

				/*
				 * There is a data stage.
				 * Find the direction
				 */
				if (tw->tw_direction == HC_GTD_IN) {
					ctrl = ctrl|HC_GTD_R|HC_GTD_IN|
							HC_GTD_T_TD_1|HC_GTD_1I;
				} else {
					ctrl = ctrl|HC_GTD_OUT|
							HC_GTD_T_TD_1|HC_GTD_1I;
				}

				/*
				 * Create the TD.  If this is an OUT
				 * transaction,  the data is already
				 * in the buffer of the TW.
				 */
				if ((ohci_insert_hc_td(ohcip,
					ctrl,
					tw->tw_cookie.dmac_address
					+ SETUP_SIZE,
					tw->tw_length - SETUP_SIZE,
					pp,
					tw)) != USB_SUCCESS) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
					    ohcip->ohci_log_hdl,
					    "ohci_handle_ctrl_td: No TD");

					usb_pp = pp->pp_pipe_handle;

					mutex_exit(&pp->pp_mutex);
					mutex_exit(&ohcip->ohci_int_mutex);

					usba_hcdi_callback(usb_pp,
						flags, NULL,
						0, USB_CC_UNSPECIFIED_ERR,
						USB_NO_RESOURCES);

					mutex_enter(&ohcip->ohci_int_mutex);
					mutex_enter(&pp->pp_mutex);

					return;
				}

				tw->tw_ctrl_state = DATA;

			}

			/* Indicate that the control list is filled */
			Set_OpReg(hcr_cmd_status,
				(Get_OpReg(hcr_cmd_status) | HCR_STATUS_CLF));

			USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "Setup complete: pp 0x%p td 0x%p",
			    (void *)pp, (void *)td);

			break;
		case DATA:
			/*
			 * The direction of the STATUS TD depends  on
			 * the direction of the transfer.
			 */
			if (tw->tw_direction == HC_GTD_IN) {
				ctrl = HC_GTD_OUT|HC_GTD_T_TD_1|HC_GTD_1I;
			} else {
				ctrl = HC_GTD_IN|HC_GTD_T_TD_1|HC_GTD_1I;
			}

			if ((ohci_insert_hc_td(ohcip,
					ctrl,
					NULL, 0, pp,  tw)) != USB_SUCCESS) {

				USB_DPRINTF_L2(PRINT_MASK_LISTS,
				    ohcip->ohci_log_hdl,
				    "ohci_handle_ctrl_td: TD exhausted");

				usb_pp = pp->pp_pipe_handle;

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&ohcip->ohci_int_mutex);

				usba_hcdi_callback(usb_pp,
					flags, NULL,
					0, USB_CC_UNSPECIFIED_ERR,
					USB_NO_RESOURCES);

				mutex_enter(&ohcip->ohci_int_mutex);
				mutex_enter(&pp->pp_mutex);

				return;
			}

			tw->tw_ctrl_state = STATUS;

			/* Indicate that the control list is filled */
			Set_OpReg(hcr_cmd_status,
				(Get_OpReg(hcr_cmd_status) | HCR_STATUS_CLF));

			USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "Data complete: pp 0x%p td 0x%p",
			    (void *)pp, (void *)td);

			break;
		case STATUS:
			/*
			 * Reset pipe state to OPENED state so that
			 * next control transfer can be started.
			 */
			pp->pp_state = OPENED;

			if ((tw->tw_length != 0) &&
			    (tw->tw_direction == HC_GTD_IN)) {
				/*
				 * Call ohci_sendup_td_message to send
				 * message to upstream. The function
				 * ohci_sendup_td_message returns
				 * USB_NO_RESOURCES if allocb fails and
				 * also sends error message to upstream
				 * by calling USBA callback function.
				 * Under error conditions just drop the
				 * current message.
				 */
				if ((ohci_sendup_td_message(ohcip, pp, tw,
				    td, USB_CC_NOERROR)) == USB_NO_RESOURCES) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
					    ohcip->ohci_log_hdl,
					    "ohci_handle_ctrl_td: Drop msg");
				}
			} else {
				usb_pipe_handle_impl_t *ph =
					tw->tw_pipe_private->pp_pipe_handle;

				message = NULL;

				OHCI_DO_BYTE_STATS(ohcip,
				    tw->tw_length - SETUP_SIZE,
				    ph->p_usb_device,
				    ph->p_endpoint->bmAttributes,
				    ph->p_endpoint->bEndpointAddress);

				mutex_exit(&pp->pp_mutex);
				mutex_exit(&ohcip->ohci_int_mutex);

				usba_hcdi_callback(
					ph,
					flags,
					message,
					0,
					USB_CC_NOERROR,
					USB_SUCCESS);

				mutex_enter(&ohcip->ohci_int_mutex);
				mutex_enter(&pp->pp_mutex);
			}

			USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "Status complete: pp 0x%p td 0x%p",
			    (void *)pp, (void *)td);

			break;
		default:
			USB_DPRINTF_L2(PRINT_MASK_INTR, ohcip->ohci_log_hdl,
			    "ohci_handle_ctrl_td: Bad control state");

			usb_pp = pp->pp_pipe_handle;

			mutex_exit(&pp->pp_mutex);
			mutex_exit(&ohcip->ohci_int_mutex);

			usba_hcdi_callback(usb_pp,
				flags, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_FAILURE);

			mutex_enter(&ohcip->ohci_int_mutex);
			mutex_enter(&pp->pp_mutex);
	}
}


/*
 * ohci_handle_bulk_td:
 *
 * Handle a bulk Transfer Descriptor (TD).
 */
/* ARGSUSED */
static void
ohci_handle_bulk_td(openhci_state_t *ohcip, ohci_pipe_private_t *pp,
	ohci_trans_wrapper_t *tw, gtd *td,
	void *tw_handle_callback_value)
{
	usb_endpoint_descr_t *ept_descr = pp->pp_pipe_handle->p_endpoint;
	usb_pipe_handle_impl_t *usb_pp = pp->pp_pipe_handle;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_bulk_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Decrement the TDs counter and check whether all the bulk
	 * data has been send or received. If TDs counter reaches
	 * zero then inform client driver about completion current
	 * bulk request. Other wise wait for completion of other bulk
	 * TDs or transactions on this pipe.
	 */
	if (--tw->tw_num_tds != 0) {

		USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_bulk_td: Number of TDs %d", tw->tw_num_tds);

		return;
	}

	/*
	 * If this is a bulk in pipe, return the data to the client.
	 * For a bulk out pipe, there is no need to do anything.
	 */
	if ((ept_descr->bEndpointAddress & USB_EPT_DIR_MASK) ==
				USB_EPT_DIR_OUT) {
		USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_bulk_td: Bulk out pipe");

		OHCI_DO_BYTE_STATS(ohcip, tw->tw_length, usb_pp->p_usb_device,
		    ept_descr->bmAttributes, ept_descr->bEndpointAddress);

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		/* Do the callback */
		usba_hcdi_callback(usb_pp,
			0,		/* Usb_flags */
			NULL,		/* Mblk */
			0,		/* Flag */
			USB_CC_NOERROR,	/* Completion_reason */
			USB_SUCCESS);	/* Return value, don't care here */

		mutex_enter(&ohcip->ohci_int_mutex);
		mutex_enter(&pp->pp_mutex);

		return;
	}

	/*
	 * Call ohci_sendup_td_message to send message to upstream.
	 * The function ohci_sendup_td_message returns USB_NO_RESOURCES
	 * if allocb fails and also sends error message to upstream by
	 * calling USBA callback function. Under error conditions just
	 * drop the current message.
	 */
	if ((ohci_sendup_td_message(ohcip, pp, tw,
	    td, USB_CC_NOERROR)) == USB_NO_RESOURCES) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_bulk_td: Drop the current message");
	}
}


/*
 * ohci_handle_intr_td:
 *
 * Handle a interrupt Transfer Descriptor (TD).
 */
/* ARGSUSED */
static void
ohci_handle_intr_td(openhci_state_t *ohcip, ohci_pipe_private_t *pp,
	ohci_trans_wrapper_t *tw, gtd *td,
	void *tw_handle_callback_value)
{
	uint_t ctrl;
	usb_pipe_handle_impl_t *usb_pp;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_intr_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * Call ohci_sendup_td_message to send message to upstream.
	 * The function ohci_sendup_td_message returns USB_NO_RESOURCES
	 * if allocb fails and also sends error message to upstream by
	 * calling USBA callback function. Under error conditions just
	 * drop the current message.
	 */
	if ((ohci_sendup_td_message(ohcip, pp, tw,
	    td, USB_CC_NOERROR)) == USB_NO_RESOURCES) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_intr_td: Drop the current message");
	}

	/*
	 * Check whether anybody is waiting for transfers completion event.
	 * If so, send this event and also stop initiating any new transfers
	 * on this pipe.
	 */
	if (ohci_check_for_transfers_completion(ohcip, pp) == USB_SUCCESS) {

		USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_intr_td: No more transfers pp = 0x%p", pp);

		return;
	}

	tw->tw_length = pp->pp_policy.pp_periodic_max_transfer_size;

	if (tw->tw_flags & USB_FLAGS_SHORT_XFER_OK) {
		ctrl = ((~(HC_GTD_T)) & (HC_GTD_IN|HC_GTD_1I|HC_GTD_R));
	} else {
		ctrl = ((~(HC_GTD_T)) & (HC_GTD_IN|HC_GTD_1I));
	}

	/* Insert another interrupt TD */
	if ((ohci_insert_hc_td(ohcip,
				ctrl,
				tw->tw_cookie.dmac_address,
				tw->tw_length,
				pp, tw)) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS,
		    ohcip->ohci_log_hdl, "ohci_handle_intr_td: TD exhausted");

		usb_pp = pp->pp_pipe_handle;

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(usb_pp,
			0, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);

		mutex_enter(&ohcip->ohci_int_mutex);
		mutex_enter(&pp->pp_mutex);
	}
}


/*
 * ohci_handle_isoch_td:
 *
 * Handle an isochronous Transfer Descriptor (TD).
 *
 * When isoc support is complete a call to OHCI_DO_BYTE_STATS is needed for
 * proper accounting of byte count kstats.
 */
/* ARGSUSED */
static void
ohci_handle_isoc_td(openhci_state_t *ohcip, ohci_pipe_private_t *pp,
	ohci_trans_wrapper_t *tw, gtd *td,
	void *tw_handle_callback_value)
{
	usb_endpoint_descr_t *ept_descr = pp->pp_pipe_handle->p_endpoint;
	usb_pipe_handle_impl_t *usb_pp;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_isoc_td:");

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/*
	 * If this is an isochronous in pipe, return the data
	 * to the client. If this is an isochronous out, there
	 * is no need to do anything. There are no  errors in
	 * the isochronous TD..
	 */
	if ((ept_descr->bEndpointAddress & USB_EPT_DIR_MASK) ==
	    USB_EPT_DIR_OUT) {

		USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_isoc_td: Isoc out pipe");

		return;
	}

	/*
	 * Call ohci_sendup_td_message to send message to
	 * upstream. The function ohci_sendup_td_message
	 * returns USB_NO_RESOURCES if allocb fails and
	 * also sends error message to upstream by calling
	 * USBA callback function. Under error conditions
	 * just drop the current message.
	 */
	if ((ohci_sendup_td_message(ohcip, pp, tw,
	    td, USB_CC_NOERROR)) == USB_NO_RESOURCES) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_isoc_td: Drop the current message");
	}

	tw->tw_length = pp->pp_policy.pp_periodic_max_transfer_size;

	/* Insert another isochronous TD */
	if ((ohci_insert_hc_td(ohcip,
			0,
			tw->tw_cookie.dmac_address,
			tw->tw_length,
			pp, tw)) != USB_SUCCESS) {

		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_isoc_td: TD exhausted");

		usb_pp = pp->pp_pipe_handle;

		mutex_exit(&pp->pp_mutex);
		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(usb_pp,
			0, NULL,
			0, USB_CC_UNSPECIFIED_ERR,
			USB_NO_RESOURCES);

		mutex_enter(&ohcip->ohci_int_mutex);
		mutex_enter(&pp->pp_mutex);
	}
}


/*
 * ohci_sendup_td_message:
 *
 * Get a message block and send the received device message to upstream.
 */
static int
ohci_sendup_td_message(openhci_state_t *ohcip, ohci_pipe_private_t *pp,
			ohci_trans_wrapper_t *tw, gtd *td, uint_t error)
{
	usb_endpoint_descr_t	*eptd = pp->pp_pipe_handle->p_endpoint;
	mblk_t	*mp = NULL;
	uint_t	flags = 0;
	size_t	length = 0;
	uchar_t	*buf;
	int rval, result = USB_SUCCESS;
	usb_pipe_handle_impl_t *usb_pp = pp->pp_pipe_handle;

	ASSERT(mutex_owned(&pp->pp_mutex));
	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	ASSERT(tw != NULL);

	length = tw->tw_length;

	if (tw->tw_flags & USB_FLAGS_SLEEP) {
		flags = USB_FLAGS_SLEEP;
	}

	/* Sync the streaming buffer */
	rval = ddi_dma_sync(tw->tw_dmahandle, 0, length, DDI_DMA_SYNC_FORCPU);

	ASSERT(rval == DDI_SUCCESS);

	/*
	 * If "CurrentBufferPointer" of Transfer Descriptor (TD) is
	 * not equal to zero, then we received less data  from the
	 * device than requested by us. In that case, get the actual
	 * received data size.
	 */
	if (Get_TD(td->hcgtd_cbp)) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_sendup_td_message: Less data");

		length = length - (Get_TD(td->hcgtd_buf_end) -
		    Get_TD(td->hcgtd_cbp) + 1);
	}

	switch (eptd->bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_CONTROL:
			/* Get the correct length */
			length = length - SETUP_SIZE;

			/* Copy the data into the mblk_t */
			buf = (uchar_t *)tw->tw_buf + SETUP_SIZE;

			break;
		case USB_EPT_ATTR_BULK:
		case USB_EPT_ATTR_INTR:
		case USB_EPT_ATTR_ISOCH:
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
			OHCI_DO_BYTE_STATS(ohcip, length, usb_pp->p_usb_device,
			    eptd->bmAttributes, USB_EPT_DIR_IN);
		} else {
			OHCI_DO_BYTE_STATS(ohcip, length, usb_pp->p_usb_device,
			    eptd->bmAttributes, eptd->bEndpointAddress);
		}
	}

	if (length != 0) {
		/* Allocate message block of required size */
		mp = allocb(length, BPRI_HI);

		if (mp == NULL) {
			USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "ohci_sendup_td_message: Allocb failed");

			mutex_exit(&pp->pp_mutex);
			mutex_exit(&ohcip->ohci_int_mutex);

			usba_hcdi_callback(usb_pp,
				flags, NULL,
				0, USB_CC_UNSPECIFIED_ERR,
				USB_NO_RESOURCES);

			mutex_enter(&ohcip->ohci_int_mutex);
			mutex_enter(&pp->pp_mutex);

			return (USB_NO_RESOURCES);
		}

		/* Copy the data into the message */
		ddi_rep_get8(tw->tw_accesshandle,
			mp->b_rptr,
			buf,
			length,
			DDI_DEV_AUTOINCR);

		/* Increment the write pointer */
		mp->b_wptr = mp->b_wptr + length;
	} else {
		USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_sendup_td_message: Zero length packet");
	}

	/*
	 * Check the completion reason and
	 * update return value accordingly.
	 */
	if (error != USB_CC_NOERROR) {
		result = USB_FAILURE;
	}

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	/* Do the callback */
	usba_hcdi_callback(usb_pp,
		flags,		/* Usb_flags */
		mp,		/* Mblk */
		0,		/* Flag */
		error,		/* Completion_reason */
		result);	/* Return value, don't care here */

	mutex_enter(&ohcip->ohci_int_mutex);
	mutex_enter(&pp->pp_mutex);

	return (USB_SUCCESS);
}


/*
 * ohci_handle_root_hub_status_change:
 *
 * A root hub status change interrupt will occur any time there is a change
 * in the root hub status register or one of the port status registers.
 */
static void
ohci_handle_root_hub_status_change(openhci_state_t *ohcip)
{
	usb_endpoint_descr_t *endpoint_descr;
	uchar_t	all_ports_status = 0;
	uint_t	new_root_hub_status;
	uint_t	new_port_status;
	uint_t	change_status;
	mblk_t	*message;
	size_t	length;
	int i;

	USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_status_change");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	endpoint_descr =
		ohcip->ohci_root_hub.root_hub_intr_pipe_handle->p_endpoint;

	length = endpoint_descr->wMaxPacketSize;

	ASSERT(length != 0);

	/* Allocate a required size message block */
	message = allocb(length, BPRI_HI);

	if (message == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_handle_root_hub_status_change: Allocb failed");

		return;
	}

	new_root_hub_status = Get_OpReg(hcr_rh_status);

	/* See if the root hub status has changed */
	if ((new_root_hub_status & HCR_RH_STATUS_MASK) !=
			ohcip->ohci_root_hub.root_hub_status) {

		USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
		    "Root hub status has changed!");

		all_ports_status |= 1;
	}

	/* Check each port */
	for (i = 0; i < ohcip->ohci_root_hub.root_hub_num_ports; i++) {
		new_port_status = Get_OpReg(hcr_rh_portstatus[i]);
		change_status = new_port_status & HCR_PORT_CHNG_MASK;

		USB_DPRINTF_L4(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
		    "port %d new status 0x%x change 0x%x", i,
		    new_port_status, change_status);

		/*
		 * If there is change in the port status then set
		 * the bit in the bitmap of changes and inform hub
		 * driver about these changes. Hub driver will take
		 * care of these changes.
		 */
		if (change_status) {

			/* See if a device was attached/detached */
			if (change_status & HCR_PORT_CSC) {
				/*
				 * Update the state depending on whether
				 * the port was attached or detached.
				 */
				if (new_port_status & HCR_PORT_CCS) {
					ohcip->
					ohci_root_hub.root_hub_port_state[i] =
					DISABLED;

					USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB,
					    ohcip->ohci_log_hdl,
					    "Port %d connected", i);
				} else {
					ohcip->
					ohci_root_hub.root_hub_port_state[i] =
					DISCONNECTED;

					USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB,
					    ohcip->ohci_log_hdl,
					    "Port %d disconnected", i);
				}
			}

			/* See if port enable status changed */
			if (change_status & HCR_PORT_PESC) {
				/*
				 * Update the state depending on whether
				 * the port was enabled or disabled.
				 */
				if (new_port_status & HCR_PORT_PES) {
					ohcip->
					ohci_root_hub.root_hub_port_state[i] =
					ENABLED;

					USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB,
					    ohcip->ohci_log_hdl,
					    "Port %d enabled", i);
				} else {
					ohcip->
					ohci_root_hub.root_hub_port_state[i] =
					DISABLED;

					USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB,
					    ohcip->ohci_log_hdl,
					    "Port %d disabled", i);
				}
			}

			all_ports_status |= 1 << (i + 1);
			Set_OpReg(hcr_rh_portstatus[i], change_status);

			/* Update the status */
			ohcip->ohci_root_hub.root_hub_port_status[i] =
							new_port_status;
		}
	}

	USB_DPRINTF_L3(PRINT_MASK_ROOT_HUB, ohcip->ohci_log_hdl,
	    "ohci_handle_root_hub_status_change: all_ports_status = 0x%x",
	    all_ports_status);

	if (ohcip->ohci_root_hub.root_hub_intr_pipe_handle &&
					all_ports_status) {

		usb_pipe_handle_impl_t *tmp_handle =
			ohcip->ohci_root_hub.root_hub_intr_pipe_handle;

		*message->b_wptr++ = all_ports_status;

		mutex_exit(&ohcip->ohci_int_mutex);

		usba_hcdi_callback(
			tmp_handle,
			0,		/* Usb_flags */
			message,	/* Mblk */
			0,		/* Flag */
			USB_CC_NOERROR,	/* Completion_reason */
			USB_SUCCESS);	/* Return value, don't care here */

		mutex_enter(&ohcip->ohci_int_mutex);

	} else {
		/* Free the allocated message block */
		freeb(message);
	}
}


/*
 * Miscellaneous functions
 */

/*
 * ohci_obtain_state:
 */
openhci_state_t *
ohci_obtain_state(dev_info_t *dip)
{
	int instance = ddi_get_instance(dip);

	openhci_state_t *state = ddi_get_soft_state(ohci_statep, instance);

	ASSERT(state != NULL);

	return (state);
}


/*
 * ohci_wait_for_sof:
 *
 * Wait for couple of SOF interrupts
 */
static int
ohci_wait_for_sof(openhci_state_t *ohcip)
{
	int	sof_wait_count;
	clock_t	sof_time_wait;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
	    ohcip->ohci_log_hdl, "ohci_wait_for_sof");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	/* Get the number of clock ticks to wait */
	sof_time_wait = drv_usectohz(OHCI_MAX_SOF_TIMEWAIT * 1000000);

	sof_wait_count = 0;
	while (sof_wait_count < MAX_SOF_WAIT_COUNT) {
		/* Enable the SOF interrupt */
		Set_OpReg(hcr_intr_enable, HCR_INTR_SOF);

		ASSERT(Get_OpReg(hcr_intr_enable) & HCR_INTR_SOF);

		if (cv_timedwait(&ohcip->ohci_SOF_cv,
			&ohcip->ohci_int_mutex,
			ddi_get_lbolt() + sof_time_wait) == -1) {

			USB_DPRINTF_L2(PRINT_MASK_LISTS,
			    ohcip->ohci_log_hdl,
			    "ohci_wait_for_sof: No SOF");

			return (USB_FAILURE);
		}
		sof_wait_count++;
	}

	return (USB_SUCCESS);
}


/*
 * ohci_wait_for_transfers_completion:
 *
 * Wait for processing all completed transfers and to send results
 * to upstream.
 */
static int
ohci_wait_for_transfers_completion(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp)
{
	ohci_trans_wrapper_t *head_tw = pp->pp_tw_head;
	ohci_trans_wrapper_t *next_tw;
	clock_t	xfer_cmpl_time_wait;
	gtd *tailp, *headp, *nextp;
	gtd *head_td, *next_td;
	hc_ed_t *ept;
	int error = USB_SUCCESS;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
	    ohcip->ohci_log_hdl, "ohci_wait_for_transfers_completion");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	ept = pp->pp_ept;
	pp->pp_count_done_tds = 0;

	headp = (gtd *)(TDpool_iommu_to_cpu(ohcip,
		Get_ED(ept->hced_headp) & (uint32_t)HC_EPT_TD_HEAD));

	tailp = (gtd *)(TDpool_iommu_to_cpu(ohcip,
		Get_ED(ept->hced_tailp) & (uint32_t)HC_EPT_TD_TAIL));

	/* Process the transfer wrappers for this pipe */
	next_tw = head_tw;
	while (next_tw != NULL) {
		head_td = (gtd *)next_tw->tw_hctd_head;
		next_td = head_td;

		if (head_td != NULL) {
			/*
			 * Walk through each TD for this transfer
			 * wrapper. If a TD still exists, then it
			 * is currently on the done list.
			 */
			while (next_td != NULL) {

				nextp = headp;

				while (nextp != tailp) {

					/* TD is on the ED */
					if (nextp == next_td) {
						break;
					}

					nextp = (gtd *)(TDpool_iommu_to_cpu
					    (ohcip, (Get_TD(nextp->hcgtd_next) &
					    HC_EPT_TD_TAIL)));
				}

				if (nextp == tailp) {
					pp->pp_count_done_tds++;
				}

				next_td = TDpool_iommu_to_cpu(ohcip,
					Get_TD(next_td->hcgtd_next_td));
			}
		}

		next_tw = next_tw->tw_next;
	}

	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_wait_for_transfers_completion: count_done_tds = 0x%x",
	    pp->pp_count_done_tds);

	if (!pp->pp_count_done_tds) {
		return (error);
	}

	/* Get the number of clock ticks to wait */
	xfer_cmpl_time_wait = drv_usectohz(OHCI_XFER_CMPL_TIMEWAIT * 1000000);

	/* Set the flag saying that waiting transfers completion */
	pp->pp_flag = OHCI_WAIT_FOR_XFER_CMPL;

	while ((pp->pp_flag == OHCI_WAIT_FOR_XFER_CMPL) &&
				(pp->pp_count_done_tds)) {

		mutex_exit(&pp->pp_mutex);

		if (cv_timedwait(&ohcip->ohci_xfer_cmpl_cv,
			&ohcip->ohci_int_mutex,
			ddi_get_lbolt() + xfer_cmpl_time_wait) == -1) {

			mutex_enter(&pp->pp_mutex);

			USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "ohci_wait_for_transfers_completion:"
			    "No transfers completion confirmation received");

			error = USB_FAILURE;
			break;
		}

		mutex_enter(&pp->pp_mutex);
	}

	return (error);
}


/*
 * ohci_check_for_transfers_completion:
 *
 * Check whether anybody is waiting for transfers completion event. If so, send
 * this event and also stop initiating any new transfers on this pipe.
 */
static int
ohci_check_for_transfers_completion(openhci_state_t *ohcip,
				ohci_pipe_private_t *pp)
{
	int error = USB_FAILURE;

	USB_DPRINTF_L4(PRINT_MASK_LISTS,
	    ohcip->ohci_log_hdl, "ohci_check_for_transfers_completion");

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));
	ASSERT(mutex_owned(&pp->pp_mutex));

	if (pp->pp_flag == OHCI_WAIT_FOR_XFER_CMPL) {

		ASSERT(pp->pp_count_done_tds != 0);

		USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_check_for_transfers_completion:"
		    "count_done_tds = 0x%x", pp->pp_count_done_tds);

		/* Decrement the done td count */
		pp->pp_count_done_tds--;

		if (!pp->pp_count_done_tds) {

			/* Send the transfer completion signal */
			cv_broadcast(&ohcip->ohci_xfer_cmpl_cv);

			pp->pp_flag = 0;

			USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
			    "ohci_check_for_transfers_completion:"
			    "Sent transfers completion event pp = 0x%p", pp);

			error = USB_SUCCESS;
		}
	}

	return (error);
}


/*
 * EDpool_cpu_to_iommu:
 *
 * This function converts for the given Endpoint Descriptor (ED) CPU address
 * to IO address.
 */
uint32_t
EDpool_cpu_to_iommu(openhci_state_t *ohcip, hc_ed_t *addr)
{
	uint32_t ed = (uint32_t)ohcip->ohci_ed_pool_cookie.dmac_address +
		(uint32_t)((uintptr_t)addr -
				(uintptr_t)(ohcip->ohci_ed_pool_addr));

	ASSERT(ed >= ohcip->ohci_ed_pool_cookie.dmac_address);
	ASSERT(ed <= ohcip->ohci_ed_pool_cookie.dmac_address +
			sizeof (hc_ed_t) * ed_pool_size);

	return (ed);
}


/*
 * TDpool_cpu_to_iommu:
 *
 * This function converts for the given Transfer Descriptor (TD) CPU address
 * to IO address.
 */
uint32_t
TDpool_cpu_to_iommu(openhci_state_t *ohcip, gtd *addr)
{
	uint32_t td = (uint32_t)ohcip->ohci_td_pool_cookie.dmac_address +
		(uint32_t)((uintptr_t)addr -
				(uintptr_t)(ohcip->ohci_td_pool_addr));

	ASSERT((ohcip->ohci_td_pool_cookie.dmac_address +
	    (uint32_t)(sizeof (gtd) * (addr - ohcip->ohci_td_pool_addr))) ==
	    (ohcip->ohci_td_pool_cookie.dmac_address +
	    (uint32_t)((uintptr_t)addr - (uintptr_t)
					(ohcip->ohci_td_pool_addr))));

	ASSERT(td >= ohcip->ohci_td_pool_cookie.dmac_address);
	ASSERT(td <= ohcip->ohci_td_pool_cookie.dmac_address +
			sizeof (gtd) * td_pool_size);

	/*
	 * The Host Controller (HC) structure
	 * should skip the transfer wrapper.
	 */
	td = td + GTD_WRAPPER;

	return (td);
}


/*
 * EDpool_iommu_to_cpu:
 *
 * This function converts for the given Endpoint Descriptor (ED) IO address
 * to CPU address.
 */
static hc_ed_t *
EDpool_iommu_to_cpu(openhci_state_t *ohcip, uintptr_t addr)
{
	hc_ed_t *ed;

	if (addr == NULL) {

		return (NULL);
	}

	ed = (hc_ed_t *)((uintptr_t)
		(addr - ohcip->ohci_ed_pool_cookie.dmac_address)
		+ (uintptr_t)ohcip->ohci_ed_pool_addr);

	ASSERT(ed >= ohcip->ohci_ed_pool_addr);
	ASSERT((uintptr_t)ed <= (uintptr_t)ohcip->ohci_ed_pool_addr +
			(uintptr_t)(sizeof (hc_ed_t) * ed_pool_size));

	return (ed);
}


/*
 * TDpool_iommu_to_cpu:
 *
 * This function converts for the given Transfer Descriptor (TD) IO address
 * to CPU address.
 */
gtd *
TDpool_iommu_to_cpu(openhci_state_t *ohcip, uintptr_t addr)
{
	gtd *td;

	if (addr == NULL) {

		return (NULL);
	}

	/* Address skips over the wrapper. Realign the pointer */
	td = (gtd *)((uintptr_t)(addr - GTD_WRAPPER -
			ohcip->ohci_td_pool_cookie.dmac_address)
			+ (uintptr_t)ohcip->ohci_td_pool_addr);

	ASSERT(td >= ohcip->ohci_td_pool_addr);
	ASSERT((uintptr_t)td <= (uintptr_t)ohcip->ohci_td_pool_addr +
			(uintptr_t)(sizeof (gtd) * td_pool_size));

	return (td);
}


/*
 * ohci_handle_error:
 *
 * Inform USBA about occured transaction errors by calling the USBA callback
 * routine.
 */
static void
ohci_handle_error(openhci_state_t *ohcip, gtd *td, uint_t error)
{
	ohci_trans_wrapper_t	*tw;
	usb_pipe_handle_impl_t	*ph;
	ohci_pipe_private_t *pp;
	uint_t flags = 0;
	mblk_t	*mp = NULL;
	uchar_t attributes;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_handle_error: error = 0x%x", error);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	ASSERT(td != NULL);

	/* Print the values in the td */
	ohci_print_td(ohcip, td);

	/* Obtain the transfer wrapper from the TD */
	tw = (ohci_trans_wrapper_t *)
		OHCI_LOOKUP_ID((uint32_t)Get_TD(td->hcgtd_trans_wrapper));

	ASSERT(tw != NULL);

	/* Obtain the pipe private structure */
	pp = tw->tw_pipe_private;

	mutex_enter(&pp->pp_mutex);
	pp->pp_state = HALTED;
	ph = tw->tw_pipe_private->pp_pipe_handle;

	if (tw->tw_flags & USB_FLAGS_SLEEP) {
		flags = USB_FLAGS_SLEEP;
	}

	/*
	 * Special error handling
	 */
	if (tw->tw_direction == HC_GTD_IN) {

		attributes = ph->p_endpoint->bmAttributes & USB_EPT_ATTR_MASK;

		switch (attributes) {
			case USB_EPT_ATTR_CONTROL:
				if (((ph->p_endpoint->bmAttributes &
				    USB_EPT_ATTR_MASK) == ATTRIBUTES_CONTROL) &&
				    (tw->tw_ctrl_state == SETUP)) {

					break;
				}
				/* FALLTHROUGH */
			case USB_EPT_ATTR_BULK:
				/*
				 * Call ohci_sendup_td_message to send
				 * message to upstream. The function
				 * ohci_sendup_td_message returns
				 * USB_NO_RESOURCES if allocb fails and
				 * also sends error message to upstream
				 * by calling USBA callback function.
				 * Under error conditions just drop the
				 * current message.
				 */
				if ((ohci_sendup_td_message(ohcip, pp,
				    tw, td, error)) == USB_NO_RESOURCES) {

					USB_DPRINTF_L2(PRINT_MASK_LISTS,
					    ohcip->ohci_log_hdl,
					    "ohci_handle_error: No resources");
				}

				mutex_exit(&pp->pp_mutex);
				return;
			default:
				break;
		}
	}

	mutex_exit(&pp->pp_mutex);
	mutex_exit(&ohcip->ohci_int_mutex);

	/*
	 * Callback the client with the
	 * failure reason.
	 */
	usba_hcdi_callback(
		ph,
		flags,
		mp,
		0,
		error,
		USB_FAILURE);

	mutex_enter(&ohcip->ohci_int_mutex);
}


/*
 * ohci_parse_error:
 *
 * Parse the result for any errors.
 */
static int
ohci_parse_error(openhci_state_t *ohcip, gtd *td)
{
	uint_t ctrl = (uint_t)Get_TD(td->hcgtd_ctrl) & (uint32_t)HC_GTD_CC;
	ohci_trans_wrapper_t	*tw;	/* Transfer wrapper */
	ohci_pipe_private_t *pp;
	int error = 0;
	uint_t flag = OHCI_REMOVE_XFER_ALWAYS;

	USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_parse_error: ctrl = 0x%x", ctrl);

	ASSERT(mutex_owned(&ohcip->ohci_int_mutex));

	ASSERT(td != NULL);

	/* Obtain the transfer wrapper from the TD */
	tw = (ohci_trans_wrapper_t *)
		OHCI_LOOKUP_ID((uint32_t)Get_TD(td->hcgtd_trans_wrapper));

	ASSERT(tw != NULL);

	/* Obtain the pipe private structure */
	pp = tw->tw_pipe_private;

	switch (ctrl) {
	case HC_GTD_CC_CRC:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: CRC error");
		error = USB_CC_CRC;
		break;
	case HC_GTD_CC_BS:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Bit stuffing");
		error = USB_CC_BITSTUFFING;
		break;
	case HC_GTD_CC_DTM:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Data Toggle Mismatch");
		error = USB_CC_DATA_TOGGLE_MM;
		break;
	case HC_GTD_CC_STALL:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Stall");
		error = USB_CC_STALL;
		break;
	case HC_GTD_CC_DNR:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Device not responding");
		error = USB_CC_DEV_NOT_RESP;
		break;
	case HC_GTD_CC_PCF:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: PID check failure");
		error = USB_CC_PID_CHECKFAILURE;
		break;
	case HC_GTD_CC_UPID:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Unexpected PID");
		error = USB_CC_UNEXP_PID;
		break;
	case HC_GTD_CC_DO:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Data overrrun");
		error = USB_CC_DATA_OVERRUN;
		break;
	case HC_GTD_CC_DU:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Data underrun");

		/*
		 * Check whether short packets are acceptable.
		 * If so don't report error to client drivers
		 * and restart the endpoint. Otherwise report
		 * data underrun error to client driver.
		 */
		mutex_enter(&pp->pp_mutex);
		if (tw->tw_flags & USB_FLAGS_SHORT_XFER_OK) {

			/* Clear the halt bit */
			Set_ED(pp->pp_ept->hced_headp,
				(Get_ED(pp->pp_ept->hced_headp) &
						~HC_EPT_Halt));

			mutex_exit(&pp->pp_mutex);
			error = USB_CC_NOERROR;
		} else {
			mutex_exit(&pp->pp_mutex);
			error = USB_CC_DATA_UNDERRUN;
		}

		break;
	case HC_GTD_CC_BO:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Buffer overrun");
		error = USB_CC_BUFFER_OVERRUN;
		break;
	case HC_GTD_CC_BU:
		USB_DPRINTF_L2(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: Buffer underrun");
		error = USB_CC_BUFFER_UNDERRUN;
		break;
	default:
		USB_DPRINTF_L4(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
		    "ohci_parse_error: No Error");
		error = USB_CC_NOERROR;
		flag  = OHCI_REMOVE_XFER_IFLAST;
	}

	mutex_enter(&pp->pp_mutex);

	/* Stop the the transfer timer */
	ohci_stop_xfer_timer(ohcip, tw, flag);

	mutex_exit(&pp->pp_mutex);

	return (error);
}



/*
 * ohci_print_op_regs:
 *
 * Print Host Controller's (HC) Operational registers.
 */
static void
ohci_print_op_regs(openhci_state_t *ohcip)
{
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_revision: 0x%x \t\thcr_control: 0x%x",
	    Get_OpReg(hcr_revision), Get_OpReg(hcr_control));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_cmd_status: 0x%x \t\thcr_intr_enable: 0x%x",
	    Get_OpReg(hcr_cmd_status), Get_OpReg(hcr_intr_enable));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_intr_disable: 0x%x \thcr_HCCA: 0x%x",
	    Get_OpReg(hcr_intr_disable), Get_OpReg(hcr_HCCA));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_periodic_curr: 0x%x \t\thcr_ctrl_head: 0x%x",
	    Get_OpReg(hcr_periodic_curr), Get_OpReg(hcr_ctrl_head));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_ctrl_curr: 0x%x  \thcr_bulk_head: 0x%x",
	    Get_OpReg(hcr_ctrl_curr), Get_OpReg(hcr_bulk_head));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_bulk_curr: 0x%x \t\thcr_done_head: 0x%x",
	    Get_OpReg(hcr_bulk_curr), Get_OpReg(hcr_done_head));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_frame_interval: 0x%x "
	    "\thcr_frame_remaining: 0x%x", Get_OpReg(hcr_frame_interval),
	    Get_OpReg(hcr_frame_remaining));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_frame_number: 0x%x  \thcr_periodic_strt: 0x%x",
	    Get_OpReg(hcr_frame_number), Get_OpReg(hcr_periodic_strt));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_transfer_ls: 0x%x \t\thcr_rh_descriptorA: 0x%x",
	    Get_OpReg(hcr_transfer_ls), Get_OpReg(hcr_rh_descriptorA));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_rh_descriptorB: 0x%x \thcr_rh_status: 0x%x",
	    Get_OpReg(hcr_rh_descriptorB), Get_OpReg(hcr_rh_status));
	USB_DPRINTF_L3(PRINT_MASK_ATTA, ohcip->ohci_log_hdl,
	    "\thcr_rh_portstatus 1: 0x%x "
	    "\thcr_rh_portstatus 2: 0x%x", Get_OpReg(hcr_rh_portstatus[0]),
	    Get_OpReg(hcr_rh_portstatus[1]));
}


/*
 * ohci_print_ed:
 */
static void
ohci_print_ed(openhci_state_t *ohcip, hc_ed_t  *ed)
{
	uint_t ctrl = Get_ED(ed->hced_ctrl);

	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_print_ed: ed = 0x%p", (void *)ed);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "\thced_ctrl: 0x%x %s", ctrl,
	    ((Get_ED(ed->hced_headp) & HC_EPT_Halt) ? "halted": ""));

	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "\ttoggle carry 0x%x", Get_ED(ed->hced_headp) & HC_EPT_Carry);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "\ttailp: 0x%x", Get_ED(ed->hced_tailp));
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "\theadp: 0x%x", Get_ED(ed->hced_headp));
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "\tnext: 0x%x", Get_ED(ed->hced_next));
}


/*
 * ohci_print_td:
 */
static void
ohci_print_td(openhci_state_t *ohcip, gtd *gtd)
{
	uint_t ctrl = Get_TD(gtd->hcgtd_ctrl);

	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "ohci_print_td: td = 0x%p", (void *)gtd);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "tcpu adr 0x%p ", (void *)gtd);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "tiommu addr 0x%x: ", TDpool_cpu_to_iommu(ohcip, gtd));
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "thcgtd_ctrl 0x%x: ", ctrl);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "tPID 0x%x ", ctrl & HC_GTD_PID);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "tDelay Intr 0x%x ", ctrl & HC_GTD_DI);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "tData Toggle 0x%x ", ctrl & HC_GTD_T);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "tError Count 0x%x ", ctrl & HC_GTD_EC);
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "thcgtd_cbp 0x%x: ", Get_TD(gtd->hcgtd_cbp));
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "thcgtd_next 0x%x: ", Get_TD(gtd->hcgtd_next));
	USB_DPRINTF_L3(PRINT_MASK_LISTS, ohcip->ohci_log_hdl,
	    "thcgtd_buf_end 0x%x: ", Get_TD(gtd->hcgtd_buf_end));
}


#ifdef	DEBUG
/*
 * ohci_dump_state:
 *	Dump all OHCI related information. This function is registered
 *	with USBA framework.
 */
static void
ohci_dump(uint_t flag, usb_opaque_t arg)
{
	openhci_state_t	*ohcip;

	mutex_enter(&ohci_dump_mutex);
	ohci_show_label = USB_DISALLOW_LABEL;

	ohcip = (openhci_state_t *)arg;
	ohci_dump_state(ohcip, flag);
	ohci_show_label = USB_ALLOW_LABEL;
	mutex_exit(&ohci_dump_mutex);
}


/*
 * ohci_dump_state:
 *	Dump OHCI state information.
 */
static void
ohci_dump_state(openhci_state_t *ohcip, uint_t flag)
{
	int	i;
	hc_ed_t	*ed;
	char	pathname[MAXNAMELEN];

	_NOTE(NO_COMPETING_THREADS_NOW);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
	    "\n***** OHCI Information *****");

	if (flag & USB_DUMP_STATE) {
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "****** ohci%d ****** dip: 0x%p",
		    ohcip->ohci_instance, ohcip->ohci_dip);

		(void) ddi_pathname(ohcip->ohci_dip, pathname);
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "****** DEVICE: %s", pathname);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_hcdi_ops: 0x%p \tohci_flags: 0x%x",
		    ohcip->ohci_hcdi_ops, ohcip->ohci_flags);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_vendor_id: 0x%x \tohci_device_id: 0x%x",
		    ohcip->ohci_vendor_id, ohcip->ohci_device_id);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_regsp: 0x%p \tohci_regs_handle: 0x%p",
		    ohcip->ohci_regsp, ohcip->ohci_regs_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_config_handle: 0x%p", ohcip->ohci_config_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_frame_interval: 0x%x \tohci_iblk_cookie: 0x%p",
		    ohcip->ohci_frame_interval, ohcip->ohci_iblk_cookie);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_hccap: 0x%p \tohci_hcca_cookie: 0x%p",
		    ohcip->ohci_hccap, ohcip->ohci_hcca_cookie);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_hcca_dma_handle: 0x%p",
		    ohcip->ohci_hcca_dma_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_hcca_mem_handle: 0x%p",
		    ohcip->ohci_hcca_mem_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_td_pool_addr: 0x%p", ohcip->ohci_td_pool_addr);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_td_pool_cookie: 0x%p",
		    ohcip->ohci_td_pool_cookie);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_td_pool_dma_handle: 0x%p ",
		    ohcip->ohci_td_pool_dma_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_td_pool_mem_handle: 0x%p ",
		    ohcip->ohci_td_pool_mem_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_ed_pool_addr: 0x%p", ohcip->ohci_ed_pool_addr);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_ed_pool_cookie: 0x%p",
		    ohcip->ohci_ed_pool_cookie);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_ed_pool_dma_handle: 0x%p ",
		    ohcip->ohci_ed_pool_dma_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_ed_pool_mem_handle: 0x%p ",
		    ohcip->ohci_ed_pool_mem_handle);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_SOF_cv: 0x%p\t\tohci_ocsem: 0x%p",
		    ohcip->ohci_SOF_cv, ohcip->ohci_ocsem);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_bandwidth:");
		for (i = 0; i < NUM_INTR_ED_LISTS; i += 8) {
			USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
			    "\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x"
			    "\t0x%x\t0x%x", ohcip->ohci_bandwidth[i],
			    ohcip->ohci_bandwidth[i + 1],
			    ohcip->ohci_bandwidth[i + 2],
			    ohcip->ohci_bandwidth[i + 3],
			    ohcip->ohci_bandwidth[i + 4],
			    ohcip->ohci_bandwidth[i + 5],
			    ohcip->ohci_bandwidth[i + 6],
			    ohcip->ohci_bandwidth[i + 7]);
		}

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_bandwidth_isoch_sum: 0x%x\t"
		    "ohci_bandwidth_intr_min: 0x%x",
		    ohcip->ohci_bandwidth_isoch_sum,
		    ohcip->ohci_bandwidth_intr_min);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_open_pipe_count: 0x%x",
		    ohcip->ohci_open_pipe_count);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_reclaim_list: 0x%p", ohcip->ohci_reclaim_list);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "ohci_root_hub:");

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub_des_A: 0x%x\troot_hub_des_B: 0x%x",
		    ohcip->ohci_root_hub.root_hub_des_A,
		    ohcip->ohci_root_hub.root_hub_des_B);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub_status: 0x%x\t\troot_hub_num_ports: 0x%x",
		    ohcip->ohci_root_hub.root_hub_status,
		    ohcip->ohci_root_hub.root_hub_num_ports);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub.root_hub_port_status:");
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\t0x%x\t0x%x\t0x%x\t0x%x",
		    ohcip->ohci_root_hub.root_hub_port_status[0],
		    ohcip->ohci_root_hub.root_hub_port_status[1],
		    ohcip->ohci_root_hub.root_hub_port_status[2],
		    ohcip->ohci_root_hub.root_hub_port_status[3]);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub.root_hub_port_state:");
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\t0x%x\t0x%x\t0x%x\t0x%x",
		    ohcip->ohci_root_hub.root_hub_port_state[0],
		    ohcip->ohci_root_hub.root_hub_port_state[1],
		    ohcip->ohci_root_hub.root_hub_port_state[2],
		    ohcip->ohci_root_hub.root_hub_port_state[3]);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub_potpgt: 0x%x",
		    ohcip->ohci_root_hub.root_hub_potpgt);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub_descr");
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tbDescLength: 0x%x\tbDescriptorType: 0x%x",
		    ohcip->ohci_root_hub.root_hub_descr.bDescLength,
		    ohcip->ohci_root_hub.root_hub_descr.bDescriptorType);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tbNbrPorts: 0x%x\t\twHubCharacteristics: 0x%x",
		    ohcip->ohci_root_hub.root_hub_descr.bNbrPorts,
		    ohcip->ohci_root_hub.root_hub_descr.wHubCharacteristics);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tbPwrOn2PwrGood: 0x%x\tbHubContrCurrent: 0x%x",
		    ohcip->ohci_root_hub.root_hub_descr.bPwrOn2PwrGood,
		    ohcip->ohci_root_hub.root_hub_descr.bHubContrCurrent);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tDeviceRemovable: 0x%x\tPortPwrCtrlMask: 0x%x",
		    ohcip->ohci_root_hub.root_hub_descr.DeviceRemovable,
		    ohcip->ohci_root_hub.root_hub_descr.PortPwrCtrlMask);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\troot_hub_ctrl_pipe_state: 0x%x \t"
		    "root_hub_intr_pipe_state: 0x%x",
		    ohcip->ohci_root_hub.root_hub_ctrl_pipe_state,
		    ohcip->ohci_root_hub.root_hub_intr_pipe_state);
	}

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
	    "ohci_dump_ops: 0x%p", ohcip->ohci_dump_ops);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
	    "ohci_log_hdl: 0x%p", ohcip->ohci_log_hdl);

	/* Dump the root hub ctrl pipe handle and pipe policy */
	if (ohcip->ohci_root_hub.root_hub_ctrl_pipe_handle) {
		if (flag & USB_DUMP_PIPE_HANDLE) {
			USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
			    "***** USB_PIPE_HANDLE "
			    "(ohci_root_hub.root_hub_ctrl_pipe_handle) *****");

			usba_dump_usb_pipe_handle((usb_pipe_handle_t)
			    ohcip->ohci_root_hub.root_hub_ctrl_pipe_handle,
			    flag);
		}

		if (flag & USB_DUMP_PIPE_POLICY) {
			usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)
				ohcip->ohci_root_hub.root_hub_ctrl_pipe_handle;

			USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
			    "***** USB_PIPE_POLICY "
			    "(ohci_root_hub.root_hub_ctrl_pipe_handle) *****");

			/* no locking is needed here */
			_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(ph->p_policy))
			usba_dump_usb_pipe_policy(ph->p_policy, flag);
		}
	}

	/* Dump the root hub intr pipe handle and pipe policy */
	if (ohcip->ohci_root_hub.root_hub_intr_pipe_handle) {
		if (flag & USB_DUMP_PIPE_HANDLE) {

			USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
			    "***** USB_PIPE_HANDLE "
			    "(ohci_root_hub.root_hub_intr_pipe_handle) *****");

			usba_dump_usb_pipe_handle((usb_pipe_handle_t)
			    ohcip->ohci_root_hub.root_hub_intr_pipe_handle,
			    flag);
		}

		if (flag & USB_DUMP_PIPE_POLICY) {
			usb_pipe_handle_impl_t	*ph = (usb_pipe_handle_impl_t *)
				ohcip->ohci_root_hub.root_hub_intr_pipe_handle;

			USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
			    "***** USB_PIPE_POLICY "
			    "(ohci_root_hub.root_hub_intr_pipe_handle) *****");

			/* no locking is needed here */
			_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(ph->p_policy))
			usba_dump_usb_pipe_policy(ph->p_policy, flag);
		}
	}

	if (flag & USB_DUMP_STATE) {
		/* Print op registers */
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "****** OHCI: Registers Info");

		ohci_print_op_regs(ohcip);

		/* Create the ed list and print it */
		ed = EDpool_iommu_to_cpu(ohcip, Get_OpReg(hcr_ctrl_head));
		ohci_dump_ed_list(ohcip, "Ctrl", ed);

		/* Create the HCCA list and print it */
		ohci_dump_hcca_list(ohcip);

		/* Create the bulk list and print it */
		ed = EDpool_iommu_to_cpu(ohcip, Get_OpReg(hcr_bulk_head));
		ohci_dump_ed_list(ohcip, "Bulk", ed);
	}

	/* Dump the polled mode information */
	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"***** POLLED MODE ");

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_intr_flag: 0x%x",
			ohcip->ohci_save_intr_status.ohci_intr_flag);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_curr_intr_sts: 0x%x",
			ohcip->ohci_save_intr_status.ohci_curr_intr_sts);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_curr_done_lst head: 0x%p",
			ohcip->ohci_save_intr_status.ohci_curr_done_lst);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_critical_intr_sts: 0x%x",
			ohcip->ohci_save_intr_status.ohci_critical_intr_sts);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_critical_done_list head: 0x%p",
			ohcip->ohci_save_intr_status.ohci_critical_done_lst);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_missed_intr_sts: 0x%x",
			ohcip->ohci_save_intr_status.ohci_missed_intr_sts);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_missed_done_list head: 0x%p",
			ohcip->ohci_save_intr_status.ohci_missed_done_lst);

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		"\tohci_timer_id: 0x%p\tohci_intr_stats: 0x%p",
			ohcip->ohci_timer_id, ohcip->ohci_intrs_stats);
	_NOTE(COMPETING_THREADS_NOW);
}


/*
 * ohci_dump_ed_list:
 *	Dump OHCI ED lists (This could be used to dump ctrl or
 *	bulk ED lists).
 */
static void
ohci_dump_ed_list(openhci_state_t *ohcip, char *ed_type, hc_ed_t *ed)
{
	gtd		*headp, *tailp;
	uint_t		ctrl;
	hc_ed_t		*ept = ed;
	uint32_t	addr;

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
	    "****** OHCI: %s ED's", ed_type);

	while (ept) {
		ctrl = Get_ED(ept->hced_ctrl);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t***** %s ED: 0x%x", ed_type, (void *)ept);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\tnext: 0x%x\thced_ctrl: 0x%x %s",
		    Get_ED(ept->hced_next), ctrl,
		    ((Get_ED(ept->hced_headp) & HC_EPT_Halt) ?
		    "halted": ""));


		addr = Get_ED(ept->hced_headp) & (uint32_t)HC_EPT_TD_HEAD;
		headp = (gtd *)(TDpool_iommu_to_cpu(ohcip, addr));

		addr = Get_ED(ept->hced_tailp) & (uint32_t)HC_EPT_TD_TAIL;
		tailp = (gtd *)(TDpool_iommu_to_cpu(ohcip, addr));

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\theadp: 0x%x\t\ttailp: 0x%x", headp, tailp);
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\ttoggle carry: 0x%x",
		    Get_ED(ept->hced_headp) & HC_EPT_Carry);

		ohci_dump_td_list(ohcip, headp, tailp);
		ept = EDpool_iommu_to_cpu(ohcip, Get_ED(ept->hced_next));
	}
}


/*
 * ohci_dump_hcca_list:
 *	Dump OHCI HCCA interrupt lattice.
 */
static void
ohci_dump_hcca_list(openhci_state_t *ohcip)
{
	int		i;
	gtd		*headp, *tailp;
	uint_t		ctrl;
	hc_ed_t		*ed;
	uint32_t	addr;

	USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
	    "***** OHCI: Interrupt Lattice TD's");

	for (i = 0; i < NUM_INTR_ED_LISTS; i++) {
		ed = EDpool_iommu_to_cpu(ohcip,
			Get_HCCA(ohcip->ohci_hccap->HccaIntTble[i]));

		ctrl = Get_ED(ed->hced_ctrl);

		if (ctrl == HC_EPT_BLANK) {
			continue;
		}

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t****Intr ED[%d]: 0x%p", i, (void *)ed);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\tnext: 0x%p\thced_ctrl: 0x%x %s",
		    Get_ED(ed->hced_next), ctrl, ((Get_ED(ed->hced_headp) &
		    HC_EPT_Halt) ? "halted": ""));

		addr = Get_ED(ed->hced_headp) & (uint32_t)HC_EPT_TD_HEAD;
		headp = (gtd *)(TDpool_iommu_to_cpu(ohcip, addr));

		addr = Get_ED(ed->hced_tailp) & (uint32_t)HC_EPT_TD_TAIL;
		tailp = (gtd *)(TDpool_iommu_to_cpu(ohcip, addr));

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\theadp: 0x%x\t\ttailp: 0x%x", headp, tailp);
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\ttoggle carry: 0x%x",
		    Get_ED(ed->hced_headp) & HC_EPT_Carry);

		ohci_dump_td_list(ohcip, headp, tailp);
	}
}


/*
 * ohci_dump_td_list:
 *	Dump OHCI TD list
 */
static void
ohci_dump_td_list(openhci_state_t *ohcip, gtd *headp, gtd *tailp)
{
	uint_t	ctrl;
	gtd	*gtd;

	for (gtd = headp; gtd != tailp; gtd = TDpool_iommu_to_cpu(ohcip,
				(Get_TD(gtd->hcgtd_next) & HC_EPT_TD_TAIL))) {
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t****TD: 0x%p", (void *)gtd);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\tcpu adr: 0x%p\tiommu addr: 0x%x",
		    (void *)gtd, TDpool_cpu_to_iommu(ohcip, gtd));

		ctrl = Get_TD(gtd->hcgtd_ctrl);
		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\thcgtd_ctrl: 0x%x", ctrl);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tPID: 0x%x", ctrl & HC_GTD_PID);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tDelay Intr: 0x%x", ctrl & HC_GTD_DI);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tData Toggle: 0x%x", ctrl & HC_GTD_T);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\t\tError Count: 0x%x", ctrl & HC_GTD_EC);

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\thcgtd_cbp: 0x%x\thcgtd_buf_end: 0x%x",
		    Get_TD(gtd->hcgtd_cbp), Get_TD(gtd->hcgtd_buf_end));

		USB_DPRINTF_L3(PRINT_MASK_DUMPING, ohcip->ohci_log_hdl,
		    "\thcgtd_next: 0x%x", Get_TD(gtd->hcgtd_next));
	}
}
#endif	/* DEBUG */
