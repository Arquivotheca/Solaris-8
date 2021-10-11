/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_UHCIUTIL_H
#define	_SYS_USB_UHCIUTIL_H

#pragma ident	"@(#)uhciutil.h	1.14	99/11/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Universal Host Controller Driver(UHCI)
 *
 * The UHCI driver is a software driver which interfaces to the Universal
 * Serial Bus Driver(USBA) and the Host Controller(HC). The interface to
 * the Host Controller is defined by the UHCI.
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
 *  Extern declarations
 */

extern void	*uhci_statep;

extern uint_t	uhci_intr(caddr_t arg);
extern void	uhci_handle_ctrl_td(uhci_state_t *uhcip, gtd *td);

extern int	uhci_hcdi_client_init(usb_device_t *usb_device);
extern int	uhci_hcdi_client_free(usb_device_t *usb_device);
extern int	uhci_hcdi_pipe_open(usb_pipe_handle_impl_t *pipe_handle,
				uint_t flags);
extern int	uhci_hcdi_pipe_close(usb_pipe_handle_impl_t *pipe_handle);
extern int	uhci_hcdi_pipe_reset(usb_pipe_handle_impl_t *pipe_handle,
				uint_t usb_flags);
extern int	uhci_hcdi_pipe_abort(usb_pipe_handle_impl_t *pipe_handle,
				uint_t usb_flags);
extern int	uhci_hcdi_pipe_get_policy(usb_pipe_handle_impl_t *pipe_handle,
				usb_pipe_policy_t *policy);
extern int	uhci_hcdi_pipe_set_policy(usb_pipe_handle_impl_t *pipe_handle,
				usb_pipe_policy_t *policy, uint_t usb_flags);
extern int	uhci_hcdi_pipe_device_ctrl_receive(
				usb_pipe_handle_impl_t *pipe_handle,
				uchar_t bmRequestType, uchar_t bRequest,
				uint16_t wValue, uint16_t wIndex,
				uint16_t wLength, uint_t usb_flags);
extern int	uhci_hcdi_pipe_device_ctrl_send(
				usb_pipe_handle_impl_t *pipe_handle,
				uchar_t bmRequestType, uchar_t bRequest,
				uint16_t wValue, uint16_t wIndex,
				uint16_t wLength, mblk_t *data,
				uint_t usb_flags);

extern int	uhci_hcdi_bulk_transfer_size(dev_info_t *dip,
				size_t  *size);
extern int	uhci_hcdi_pipe_receive_bulk_data(
				usb_pipe_handle_impl_t *pipe_handle,
				size_t length, uint_t usb_flags);
extern int	uhci_hcdi_pipe_send_bulk_data(
				usb_pipe_handle_impl_t *pipe_handle,
				mblk_t *data, uint_t usb_flags);

extern int	uhci_hcdi_pipe_start_polling(
				usb_pipe_handle_impl_t *pipe_handle,
				uint_t flags);
extern int	uhci_hcdi_pipe_stop_polling(
				usb_pipe_handle_impl_t *pipe_handle,
				uint_t flags);
extern int	uhci_hcdi_pipe_send_isoc_data(
				usb_pipe_handle_impl_t *pipe_handle,
				mblk_t *data, uint_t usb_flags);

extern int	uhci_handle_root_hub_request(
				usb_pipe_handle_impl_t	*pipe_handle,
				uchar_t	bmRequestType, uchar_t bRequest,
				uint16_t wValue, uint16_t wIndex,
				uint16_t wLength, mblk_t *data,
				uint_t	usb_flags);
extern void	uhci_unload_root_hub_driver(uhci_state_t *uhcip);
extern int	uhci_insert_bulk_td(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *ph,
				uhci_handler_function_t tw_handle_td,
				size_t length, mblk_t *data, uint_t flags);
extern int	uhci_sendup_td_message(uhci_state_t *uhcip,
				uhci_pipe_private_t *pp,
				uhci_trans_wrapper_t *tw);
extern uint_t	uhci_parse_td_error(gtd *td, uint_t *NAK_received);
extern void	uhci_process_submitted_td_queue(uhci_state_t *uhcip);

#ifdef DEBUG
extern kmutex_t uhci_dump_mutex;
extern uint_t  uhci_show_label;
extern void usba_dump_deregister(usb_dump_ops_t *dump_ops);
extern void usba_free_dump_ops(usb_dump_ops_t *dump_ops);
void uhci_dump(uint_t, usb_opaque_t);
void uhci_dump_state(uhci_state_t *, uint_t);
void uhci_dump_registers(uhci_state_t *);
void uhci_dump_pending_cmds(uhci_state_t *);
#endif
/*
 *  Function Prototype
 */

static int uhci_build_interrupt_lattice(uhci_state_t *uhcip);
void	uhci_set_dma_attributes(uhci_state_t *uhcip);
void	uhci_initialize_default_pipe(uhci_state_t *uhcip);
void	uhci_deinitialize_default_pipe(uhci_state_t *uhcip);
void	uhci_remove_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp);
void	uhci_insert_qh(uhci_state_t *uhcip,
		usb_pipe_handle_impl_t *pipe_handle);
void	uhci_remove_bulk_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp);
void	uhci_remove_ctrl_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp);
void	uhci_remove_intr_qh(uhci_state_t *uhcip, uhci_pipe_private_t *pp);
void	uhci_deallocate_bandwidth(uhci_state_t *uhcip,
			usb_pipe_handle_impl_t *pipe_handle);
void	uhci_decode_ddi_dma_addr_bind_handle_result(uhci_state_t *uhcip,
			int result);
void	uhci_print_td(uhci_state_t *uhcip, gtd *td);

int	uhci_allocate_pools(dev_info_t *dip, uhci_state_t *uhcip);
int	uhci_register_intrs_and_init_mutex(dev_info_t *dip,
			uhci_state_t *uhcip);
int uhci_init_ctlr(dev_info_t *dip, uhci_state_t *uhcip);
int uhci_map_regs(dev_info_t *dip, uhci_state_t *uhcip);
int uhci_init_frame_lst_table(dev_info_t *dip, uhci_state_t *uhcip);
int	uhci_allocate_bandwidth(uhci_state_t *uhcip,
		usb_pipe_handle_impl_t *pipe_handle);
int	uhci_bandwidth_adjust(uhci_state_t *uhcip,
		usb_endpoint_descr_t *endpoint,
		usb_port_status_t port_status);

void	uhci_remove_tds_tws(uhci_state_t *uhcip, usb_pipe_handle_impl_t *ph);

void	uhci_deallocate_tw(uhci_state_t *uhcip, uhci_pipe_private_t *pp,
			uhci_trans_wrapper_t *tw);
void	uhci_deallocate_gtd(uhci_state_t *uhcip, gtd *old_gtd);
void	uhci_free_tw(uhci_state_t *uhcip, uhci_trans_wrapper_t *tw);
void	uhci_cleanup(uhci_state_t *uhcip, int flags);
void	uhci_remove_all_pending_tds_in_qh(queue_head_t *qh);
void	uhci_insert_qh(uhci_state_t *uhcip,
			usb_pipe_handle_impl_t *pipe_handle);
void	uhci_modify_td_active_bits(uhci_state_t *uhcip, queue_head_t *qh);

gtd *uhci_allocate_td_from_pool(uhci_state_t *uhcip);
static int uhci_create_setup_pkt(uhci_state_t *uhcip,
			uhci_pipe_private_t *pp,
			uhci_trans_wrapper_t *tw,
			uchar_t bmRequestType,
			uchar_t bRequest, uint16_t wValue,
			uint16_t wIndex, uint16_t wLength,
			mblk_t *data, uint_t usb_flags);

static int uhci_insert_ctrl_td(uhci_state_t *uhcip,
				usb_pipe_handle_impl_t *pipe_handle,
				uchar_t		bmRequestType,
				uchar_t		bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t		*data,
				uint_t		usb_flags);
int uhci_common_ctrl_routine(usb_pipe_handle_impl_t *pipe_handle,
				uchar_t	bmRequestType,
				uchar_t	bRequest,
				uint16_t	wValue,
				uint16_t	wIndex,
				uint16_t	wLength,
				mblk_t	*data,
				uint_t	usb_flags);
int uhci_common_bulk_routine(usb_pipe_handle_impl_t  *ph,
				size_t	length,
				mblk_t	*data,
				uint_t	usb_flags);

static void uhci_fill_in_td(uhci_state_t *uhcip,
				gtd		*td,
				gtd		*current_dummy,
				uint32_t	hcgtd_iommu_cbp,
				size_t	length,
				uhci_pipe_private_t	*pp,
				uhci_trans_wrapper_t	*tw,
				uchar_t	PID);

int uhci_insert_hc_td(uhci_state_t *uhcip,
				uint32_t		hcgtd_iommu_cbp,
				size_t		hcgtd_length,
				uhci_pipe_private_t	*pp,
				uhci_trans_wrapper_t	*tw,
				uchar_t					PID);

static uhci_trans_wrapper_t *uhci_create_transfer_wrapper(
		uhci_state_t *uhcip,
		uhci_pipe_private_t *pp,
		size_t	length,
		uint_t	usb_flags);

int uhci_insert_intr_td(uhci_state_t *uhcip,
		usb_pipe_handle_impl_t  *pipe_handle, uint_t flags,
		uhci_handler_function_t tw_handle_td,
		usb_opaque_t tw_handle_callback_value);

static void uhci_insert_intr_qh(uhci_state_t *uhcip,
		uhci_pipe_private_t	*pp);

static void uhci_insert_bulk_qh(uhci_state_t *uhcip,
		uhci_pipe_private_t	*pp);

static void uhci_insert_ctrl_qh(uhci_state_t *uhcip,
		uhci_pipe_private_t	*pp);


uint_t	pow_2(unsigned int x);
uint_t	log_2(unsigned int x);
uint_t	uhci_lattice_height(uint_t bandwidth);
uint_t	uhci_lattice_parent(uint_t node);
uint_t	uhci_leftmost_leaf(uint_t node, uint_t height);
uint_t	uhci_compute_total_bandwidth(usb_endpoint_descr_t *endpoint,
			usb_port_status_t port_status);
void	uhci_print_qh(uhci_state_t *uhcip, queue_head_t *qh);
void	uhci_hanlde_bulk_td_errors(uhci_state_t *uhcip, gtd *td);
void	uhci_handle_bulk_td(uhci_state_t *uhcip, gtd *td);
uint_t	uhci_alloc_mem_bulk_tds(uhci_state_t *uhcip, uint_t num_tds,
			uhci_bulk_xfer_t *info);
void	uhci_fill_in_bulk_td(uhci_state_t *uhcip, gtd *current_td,
			gtd *next_td, uint32_t next_td_paddr,
			usb_pipe_handle_impl_t *ph, uint_t buffer_address,
			uint_t length, uhci_trans_wrapper_t *tw);

usb_hcdi_ops_t  *uhci_alloc_hcdi_ops(uhci_state_t *uhcip);
uhci_state_t    *uhci_obtain_state(dev_info_t *dip);
queue_head_t    *uhci_alloc_queue_head(uhci_state_t *uhcip);
int		uhci_hcdi_polled_input_init(usb_pipe_handle_impl_t *,
				uchar_t **, usb_console_info_impl_t *);
int		uhci_hcdi_polled_input_enter(usb_console_info_impl_t *);
int		uhci_hcdi_polled_read(usb_console_info_impl_t *, uint_t *);
int		uhci_hcdi_polled_input_exit(usb_console_info_impl_t *);
int		uhci_hcdi_polled_input_fini(usb_console_info_impl_t *);
void	uhci_remove_bulk_tds_tws(uhci_state_t *uhcip,
		usb_pipe_handle_impl_t *ph,
		queue_head_t *qh);

uint_t		td_pool_size = 100;
uint_t		qh_pool_size = 130;
ushort_t	tree_bottom_nodes[NUM_FRAME_LST_ENTRIES];


#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_UHCIUTIL_H */
