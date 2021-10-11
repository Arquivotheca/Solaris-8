/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_UHCID_H
#define	_SYS_USB_UHCID_H

#pragma ident	"@(#)uhcid.h	1.18	99/11/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Universal Host Controller Driver (UHCI)
 *
 * The UHCI driver is a software driver which interfaces to the Universal
 * Serial Bus Driver (USBA) and the Host Controller (HC). The interface to
 * the Host Controller is defined by the Universal Host Controller Interface.
 *
 * This file contains the data strucutes for the UHCI driver.
 */

#include		<sys/note.h>
#include		<sys/types.h>
#include		<sys/usb/usba/usba_types.h>
#include		<sys/usb/usba/usba_impl.h>
#include		<sys/usb/hubd/hub.h>

/*
 *   Macro definitions
 */

#define				UHCI_TRUE		1
#define				UHCI_FALSE		0
#define				UHCI_SUCCESS		1
#define				UHCI_FAILURE		0

#define				UHCI_UNDERRUN_OCCURRED	0x1234
#define				UHCI_OVERRUN_OCCURRED	0x5678
#define				UHCI_PROP_MASK		0x01000020
#define				UHCI_RESET_DELAY	15000
#define				UHCI_TIMEWAIT		10000

#define				MAX_RH_PORTS		2
#define				DISCONNECTED		2

#define				POLLED_RAW_BUF_SIZE	8

/* Default time out values for bulk and ctrl commands */
#define				UHCI_CTRL_TIMEOUT	5
#define				UHCI_BULK_TIMEOUT	60

typedef struct root_hub {
	/* Last root hub status */
	uint_t		root_hub_status;

	/* No. of ports on the root */
	uint_t		root_hub_num_ports;

	/* Last status of ports */
	uint_t		root_hub_port_status[MAX_RH_PORTS];

	/* Power on to power good */
	uint_t		root_hub_potpgt;

	/* See below */
	uint_t		root_hub_port_state[MAX_RH_PORTS];

	/* Root hub interrupt pipe handle */
	usb_pipe_handle_impl_t *root_hub_pipe_handle;

	/* Copy of the root hub descriptor */
	usb_hub_descr_t		root_hub_descr;

	/* Root hub interrupt pipe state */
	uint_t				root_hub_pipe_state;

} root_hub_t;

/*
 * UHCI Host Controller per instance data structure
 *
 * The Host Controller Driver (HCD) maintains the state of Host Controller
 * (HC). There is an uhci_state structure per instance  of the UHCI
 * host controller.
 */

typedef struct uhci_state {
	dev_info_t		*uhci_dip;	/* Dip of HC */
	uint_t			uhci_instance;
	usb_hcdi_ops_t		*uhci_hcdi_ops;	/* HCDI structure */

	/* Used for clean up */
	uint_t			uhci_flags;
	uint_t			uhci_dma_addr_bind_flag;

	hc_regs_t		*uhci_regsp;		/* Host ctlr regs */
	ddi_acc_handle_t	uhci_regs_handle;	/* Reg handle */

	ddi_acc_handle_t	uhci_config_handle;	/* Config space hndle */

	/* Frame interval reg */
	uint_t			uhci_frame_interval;
	ddi_dma_attr_t		uhci_dma_attr;		/* DMA attributes */

	ddi_iblock_cookie_t	uhci_iblk_cookie;	/* iblock cookie */
	kmutex_t		uhci_int_mutex;		/* Mutex for struct */

	frame_lst_table_t	*uhci_frame_lst_tablep;	/* Virtual HCCA ptr */
	ddi_dma_cookie_t	uhci_flt_cookie;	/* DMA cookie */
	ddi_dma_handle_t	uhci_flt_dma_handle;	/* DMA handle */
	ddi_acc_handle_t	uhci_flt_mem_handle;	/* Memory handle */

	/*
	 * There are two pools of memory. One pool contains the memory for
	 * the transfer descriptors and other pool contains the memory for
	 * the Queue Head pointers. The advantage of the pools is that it's
	 * easy to go back and forth between the iommu and the cpu addresses.
	 *
	 * The pools are protected by the int_mutex because the memory
	 * in the pools may be accessed by either the host controller or the
	 * host controller driver.
	 */

	/* General transfer descriptor pool */
	gtd			*uhci_td_pool_addr;	/* Start of the pool */
	ddi_dma_cookie_t	uhci_td_pool_cookie;	/* DMA cookie */
	ddi_dma_handle_t	uhci_td_pool_dma_handle; /* DMA hndle */
	ddi_acc_handle_t	uhci_td_pool_mem_handle; /* Mem hndle */

	/* Endpoint descriptor pool */
	queue_head_t		*uhci_qh_pool_addr;	/* Start of the pool */
	ddi_dma_cookie_t	uhci_qh_pool_cookie;	/* DMA cookie */
	ddi_dma_handle_t	uhci_qh_pool_dma_handle; /* DMA handle */
	ddi_acc_handle_t	uhci_qh_pool_mem_handle; /* Mem handle */

	/* Semaphore to serialize opens and closes */
	ksema_t			uhci_ocsem;

	/* Timeout id of the root hub status change pipe handler */
	timeout_id_t		uhci_timeout_id;

	/* Timeout id of the ctrl/bulk xfers timeout */
	timeout_id_t		uhci_ctrl_bulk_timeout_id;

	/*
	 * Bandwidth fields
	 *
	 * The uhci_bandwidth array keeps track of the allocated bandwidth
	 * for this host controller. The uhci_bandwidth_isoch_sum field
	 * represents the sum of the allocated isochronous bandwidth. The
	 * total bandwidth allocated for least allocated list out of the 32
	 * interrupt lists is represented by the uhci_bandwdith_intr_min
	 * field.
	 */
	uint_t	uhci_bandwidth[NUM_FRAME_LST_ENTRIES];
	uint_t	uhci_bandwidth_isoch_sum;
	uint_t	uhci_bandwidth_intr_min;

	struct root_hub		uhci_root_hub;	/* Root hub info */
	gtd			*uhci_oust_tds_head;
	gtd			*uhci_oust_tds_tail;

	queue_head_t		*uhci_ctrl_xfers_q_head;
	queue_head_t		*uhci_ctrl_xfers_q_tail;
	queue_head_t		*uhci_bulk_xfers_q_head;
	queue_head_t		*uhci_bulk_xfers_q_tail;

	kcondvar_t		uhci_cv_SOF;
	uchar_t			uhci_cv_signal;

	/* Polled I/O support */
	uint32_t		uhci_polled_flag;

	/* logging support */
	usb_log_handle_t	uhci_log_hdl;

	/* Dump Support */
	struct usb_dump_ops	*uhci_dump_ops;

	/*
	 * kstat structures
	 */
	kstat_t			*uhci_intrs_stats;
	kstat_t			*uhci_total_stats;
	kstat_t			*uhci_count_stats[USB_N_COUNT_KSTATS];
} uhci_state_t;

typedef struct uhci_intrs_stats {
	struct kstat_named	uhci_intrs_hc_halted;
	struct kstat_named	uhci_intrs_hc_process_err;
	struct kstat_named	uhci_intrs_host_sys_err;
	struct kstat_named	uhci_intrs_resume_detected;
	struct kstat_named	uhci_intrs_usb_err_intr;
	struct kstat_named	uhci_intrs_usb_intr;
	struct kstat_named	uhci_intrs_total;
	struct kstat_named	uhci_intrs_not_claimed;
} uhci_intrs_stats_t;

/*
 * uhci kstat defines
 */
#define	UHCI_INTRS_STATS(uhci)	((uhci)->uhci_intrs_stats)
#define	UHCI_INTRS_STATS_DATA(uhci)	\
	((uhci_intrs_stats_t *)UHCI_INTRS_STATS((uhci))->ks_data)

#define	UHCI_TOTAL_STATS(uhci)	((uhci)->uhci_total_stats)
#define	UHCI_TOTAL_STATS_DATA(uhci)	(KSTAT_IO_PTR((uhci)->uhci_total_stats))
#define	UHCI_CTRL_STATS(uhci)	\
	(KSTAT_IO_PTR((uhci)->uhci_count_stats[USB_EPT_ATTR_CONTROL]))
#define	UHCI_BULK_STATS(uhci)	\
	(KSTAT_IO_PTR((uhci)->uhci_count_stats[USB_EPT_ATTR_BULK]))
#define	UHCI_INTR_STATS(uhci)	\
	(KSTAT_IO_PTR((uhci)->uhci_count_stats[USB_EPT_ATTR_INTR]))
#define	UHCI_ISOC_STATS(uhci)	\
	(KSTAT_IO_PTR((uhci)->uhci_count_stats[USB_EPT_ATTR_ISOCH]))

#define	UHCI_DO_INTRS_STATS(uhci, val) {				\
	if (UHCI_INTRS_STATS(uhci) != NULL) {				\
		UHCI_INTRS_STATS_DATA(uhci)->uhci_intrs_total.value.ui64++; \
		switch (val) {						\
			case USBSTS_REG_HC_HALTED:			\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_hc_halted.value.ui64++;	\
				break;					\
			case USBSTS_REG_HC_PROCESS_ERR:			\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_hc_process_err.value.ui64++; \
				break;					\
			case USBSTS_REG_HOST_SYS_ERR:			\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_host_sys_err.value.ui64++; \
				break;					\
			case USBSTS_REG_RESUME_DETECT:			\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_resume_detected.value.ui64++; \
				break;					\
			case USBSTS_REG_USB_ERR_INTR:			\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_usb_err_intr.value.ui64++; \
				break;					\
			case USBSTS_REG_USB_INTR:			\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_usb_intr.value.ui64++;	\
				break;					\
			default:					\
				UHCI_INTRS_STATS_DATA(uhci)->		\
				    uhci_intrs_not_claimed.value.ui64++; \
				break;					\
		}							\
	}								\
}

#define	UHCI_DO_BYTE_STATS(uhci, len, usb_device, attr, addr) {		\
	uint8_t type = attr & USB_EPT_ATTR_MASK;			\
	uint8_t dir = addr & USB_EPT_DIR_MASK;				\
									\
	if (dir == USB_EPT_DIR_IN) {					\
		UHCI_TOTAL_STATS_DATA(uhci)->reads++;			\
		UHCI_TOTAL_STATS_DATA(uhci)->nread += len;		\
		switch (type) {						\
			case USB_EPT_ATTR_CONTROL:			\
				UHCI_CTRL_STATS(uhci)->reads++;		\
				UHCI_CTRL_STATS(uhci)->nread += len;	\
				break;					\
			case USB_EPT_ATTR_BULK:				\
				UHCI_BULK_STATS(uhci)->reads++;		\
				UHCI_BULK_STATS(uhci)->nread += len;	\
				break;					\
			case USB_EPT_ATTR_INTR:				\
				UHCI_INTR_STATS(uhci)->reads++;		\
				UHCI_INTR_STATS(uhci)->nread += len;	\
				break;					\
			case USB_EPT_ATTR_ISOCH:			\
				UHCI_ISOC_STATS(uhci)->reads++;		\
				UHCI_ISOC_STATS(uhci)->nread += len;	\
				break;					\
		}							\
	} else if (dir == USB_EPT_DIR_OUT) {				\
		UHCI_TOTAL_STATS_DATA(uhci)->writes++;			\
		UHCI_TOTAL_STATS_DATA(uhci)->nwritten += len;		\
		switch (type) {						\
			case USB_EPT_ATTR_CONTROL:			\
				UHCI_CTRL_STATS(uhci)->writes++;	\
				UHCI_CTRL_STATS(uhci)->nwritten += len;	\
				break;					\
			case USB_EPT_ATTR_BULK:				\
				UHCI_BULK_STATS(uhci)->writes++;	\
				UHCI_BULK_STATS(uhci)->nwritten += len;	\
				break;					\
			case USB_EPT_ATTR_INTR:				\
				UHCI_INTR_STATS(uhci)->writes++;	\
				UHCI_INTR_STATS(uhci)->nwritten += len;	\
				break;					\
			case USB_EPT_ATTR_ISOCH:			\
				UHCI_ISOC_STATS(uhci)->writes++;	\
				UHCI_ISOC_STATS(uhci)->nwritten += len;	\
				break;					\
		}							\
	}								\
}

/*
 * Definitions for uhci_polled_flag
 * The flag is set to UHCI_POLLED_FLAG_FALSE by default. The flags is
 * set to UHCI_POLLED_FLAG_TD_CIMPL when shifting from normal mode to
 * polled mode and if the normal TD is completed at that time. And the
 * flag is set to UHCI_POLLED_FLAG_TRUE while exiting from the polled
 * mode. In the timeout handler for root hub status change, this flag
 * is checked. If set to UHCI_POLLED_FLAG_TRUE, the routine
 * uhci_process_submitted_td_queue() to process the completed td.
 */

#define	UHCI_POLLED_FLAG_FALSE		0
#define	UHCI_POLLED_FLAG_TRUE		1
#define	UHCI_POLLED_FLAG_TD_COMPL	2

/*
 * USB device structure
 *
 * This structure represents the USB device and there is a structure per
 * address on the bus.
 */
typedef struct usb_dev {
	usb_device_t		*usb_dev_device_impl;	/* Ptr USBA struct */
	struct uhci_pipe_private *usb_dev_pipe_list;	/* List of pipes */
	kmutex_t		usb_dev_mutex;		/* Mutex for struct */
}usb_dev_t;



/*
 * Pipe private structure
 *
 * There is an instance of this structure per pipe.  This structure holds
 * HCD specific pipe information.  A pointer to this structure is kept in
 * the USBA pipe handle (usb_pipe_handle_impl_t).
 */
typedef struct uhci_pipe_private {
	usb_pipe_handle_impl_t	*pp_pipe_handle; /* Back ptr to handle */
	queue_head_t		*pp_qh;		/* Pipe's ept */
	uint_t			pp_state;	/* See below */
	usb_pipe_policy_t	pp_policy;	/* Copy of the pipe policy */
	uint_t			pp_node;	/* Node in lattice */
	kmutex_t		pp_mutex;	/* Pipe mutex */
	uchar_t			pp_data_toggle;

	/*
	 * Each pipe may have multiple transfer wrappers. Each transfer
	 * wrapper represents a USB transfer on the bus.  A transfer is
	 * made up of one or more transactions.
	 */
	struct uhci_trans_wrapper *pp_tw_head;	/* Head of the list */
	struct uhci_trans_wrapper *pp_tw_tail;	/* Tail of the list */
	struct uhci_pipe_private  *pp_next;	/* Next pipe */
} uhci_pipe_private_t;

/* Pipe states */

#define	PIPE_OPENED	1	/* Pipe has opened */
#define	PIPE_POLLING	2	/* Polling the endpoint */
#define	PIPE_STOPPED	3	/* Polling has stopped */


/* Function prototype */
typedef void (*uhci_handler_function_t) (uhci_state_t *uhcip, gtd  *td);

/*
 * Transfer wrapper
 *
 * The transfer wrapper represents a USB transfer on the bus and there
 * is one instance per USB transfer.  A transfer is made up of one or
 * more transactions.
 *
 * Control and bulk pipes will have one transfer wrapper per transfer
 * and where as Isochronous and Interrupt pipes will only have one
 * transfer wrapper. The transfers wrapper are continually reused for
 * the Interrupt and Isochronous pipes as those pipes are polled.
 */

typedef struct uhci_trans_wrapper {
	struct uhci_trans_wrapper	*tw_next;	/* Next wrapper */
	uhci_pipe_private_t	*tw_pipe_private;
	size_t		tw_length;		/* Txfer length */
	uint_t		tw_tmp;			/* Temp variable */
	ddi_dma_handle_t tw_dmahandle;		/* DMA handle */
	ddi_acc_handle_t tw_accesshandle;	/* Acc hndle */
	char		*tw_buf;		/* Buffer for txfer */
	ddi_dma_cookie_t tw_cookie;		/* DMA cookie */
	int		tw_ctrl_state;		/* See below */
	gtd		*tw_hctd_head;		/* Head TD */
	gtd		*tw_hctd_tail;		/* Tail TD */
	uint_t		tw_direction;		/* Direction of TD */
	uint_t		tw_flags;		/* Flags */

	/*
	 * This is the function to call when this td is done. This way
	 * we don't have to look in the td to figure out what kind it is.
	 */
	uhci_handler_function_t		tw_handle_td;

	/*
	 * This is the callback value used when processing a done td.
	 */
	usb_opaque_t			tw_handle_callback_value;

	uint_t				tw_bytes_xfered;
	uint_t				tw_bytes_pending;
	uint_t				tw_timeout_cnt;

} uhci_trans_wrapper_t;



/*
 * Macros for setting/getting information
 */

#define	Get_OpReg32(addr)	ddi_get32(uhcip->uhci_regs_handle, \
			(uint32_t *)&uhcip->uhci_regsp->addr)
#define	Get_OpReg16(addr)	ddi_get16(uhcip->uhci_regs_handle, \
			(uint16_t *)&uhcip->uhci_regsp->addr)
#define	Get_OpReg8(addr)	ddi_get8(uhcip->uhci_regs_handle, \
			(uchar_t *)&uhcip->uhci_regsp->addr)

#define	Set_OpReg32(addr, val)   ddi_put32(uhcip->uhci_regs_handle, \
			((uint32_t *)&uhcip->uhci_regsp->addr), \
			((int32_t)(val)))

#define	Set_OpReg16(addr, val)   ddi_put16(uhcip->uhci_regs_handle, \
			((uint16_t *)&uhcip->uhci_regsp->addr), \
			((int16_t)(val)))


#define	QH_PADDR(addr) \
			(uint32_t)uhcip->uhci_qh_pool_cookie.dmac_address + \
			(uint32_t)((uint32_t)addr - \
			(uint32_t)uhcip->uhci_qh_pool_addr)


#define	QH_VADDR(addr) \
			((uint32_t)((uint32_t)addr - \
			(uint32_t)uhcip->uhci_qh_pool_cookie.dmac_address) + \
			(uint32_t)uhcip->uhci_qh_pool_addr)

#define	TD_PADDR(addr)  \
			((uint32_t)uhcip->uhci_td_pool_cookie.dmac_address + \
			(uint32_t)((uintptr_t)addr - \
			(uintptr_t)(uhcip->uhci_td_pool_addr)))

#define	BULKTD_PADDR(x, addr)\
			((uint32_t)((uint32_t)addr - \
			(uint32_t)x->bulk_pool_addr) +\
			(uint32_t)x->uhci_bulk_cookie.dmac_address)

#define	TD_VADDR(addr) \
			((uint32_t)((uint32_t)addr - \
			(uint32_t)uhcip->uhci_td_pool_cookie.dmac_address) + \
			(uint32_t)uhcip->uhci_td_pool_addr)
/*
 * If the terminate bit is cleared, there shouldn't be any
 * race condition problems. If the host controller reads the
 * bit before the driver has a chance to set the bit, the bit
 * will be reread on the next frame.
 */

#define		UHCI_SET_TERMINATE_BIT(addr)	addr = addr | HC_END_OF_LIST
#define		UHCI_CLEAR_TERMINATE_BIT(addr)	addr = addr & ~HC_END_OF_LIST

/*
 *   Macros
 */

#define		UHCI_SOFT_STATE_ZALLOC		0x01
#define		UHCI_INTR_HDLR_REGISTER		0x02
#define		UHCI_LOCKS_INIT			0x04
#define		UHCI_REGS_MAPPING		0x08
#define		UHCI_ROOT_HUB_REGISTER		0x10

/*
 * uhci_dma_addr_bind_flag values
 *
 * This flag indicates if the various DMA addresses allocated by the UHCI
 * have been bound to their respective handles. This is needed to recover
 * without errors from uhci_cleanup when it calls ddi_dma_unbind_handle()
 */
#define	UHCI_TD_POOL_BOUND	0x01	/* for TD pools */
#define	UHCI_QH_POOL_BOUND	0x02	/* for QH pools */
#define	UHCI_FLA_POOL_BOUND	0x04	/* for Host Ctrlr Framelist Area */

#define		UHCI_ONE_SECOND			1000000
#define		UHCI_MAX_INSTS			4


/*
 * Severity levels for printing
 */

/*
 * Masks
 */
#define	PRINT_MASK_ATTA			0x00000001	/* Attach time */
#define	PRINT_MASK_LISTS		0x00000002	/* List management */
#define	PRINT_MASK_ROOT_HUB		0x00000004	/* Root hub stuff */
#define	PRINT_MASK_ALLOC		0x00000008	/* Alloc/dealloc */
#define	PRINT_MASK_INTR			0x00000010	/* Interrupt handling */
#define	PRINT_MASK_BW			0x00000020	/* Bandwidth */
#define	PRINT_MASK_CBOPS		0x00000040	/* CB-OPS */
#define	PRINT_MASK_HCDI			0x00000080	/* HCDI entry points */
#define	PRINT_MASK_DUMPING		0x00000100	/* For dumping state */
#define	PRINT_MASK_ALL			0xFFFFFFFF

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_UHCID_H */
