/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_OHCID_H
#define	_SYS_USB_OHCID_H

#pragma ident	"@(#)ohcid.h	1.11	99/11/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This header file describes the data structures required for the Host
 * Controller Driver (HCD) for root hub operations, to maintain state of
 * Host Controller (HC), to perform different USB transfers and for the
 * bandwidth allocations.
 */

#include <sys/usb/hubd/hub.h>
#include <sys/id32.h>
#include <sys/kstat.h>

/*
 * Root hub information structure
 *
 * The Root hub is a Universal Serial Bus hub attached directly to the
 * Host Controller (HC) and all the internal registers of the root hub
 * are exposed to the Host Controller Driver (HCD) which is responsible
 * for providing the proper hub-class protocol with the  USB driver and
 * proper control of the root hub. This structure contains information
 * about the root hub and its ports.
 */
typedef struct root_hub {
	uint_t	root_hub_des_A;		/* Descriptor reg A value */
	uint_t	root_hub_des_B;		/* Descriptor reg B value */
	uint_t	root_hub_status;	/* Last root hub status */
	uint_t	root_hub_num_ports;	/* No. of ports on the root */
	uint_t	root_hub_port_status[MAX_RH_PORTS];
					/* Last status for each port */
	uint_t	root_hub_potpgt;	/* Power on to power good */
	uint_t	root_hub_port_state[MAX_RH_PORTS];	/* See below */

	usb_pipe_handle_impl_t *root_hub_ctrl_pipe_handle;
					/* Root hub control pipe handle */
	usb_pipe_handle_impl_t *root_hub_intr_pipe_handle;
					/* Root hub interrupt pipe handle */
	usb_hub_descr_t	root_hub_descr; /* Copy of the root hub descriptor */
	uint_t	root_hub_ctrl_pipe_state;
					/* Root hub control pipe state */
	uint_t	root_hub_intr_pipe_state;
					/* Root hub interrupt pipe state */
} root_hub_t;

/* Port States */
#define	UNINIT		0x00		/* Uninitialized port */
#define	POWERED_OFF	0x01		/* Port has no power */
#define	DISCONNECTED	0x02		/* Port has power, no dev */
#define	DISABLED	0x03		/* Dev connected, no downstream data */
#define	ENABLED		0x04		/* Downstream data is enabled */
#define	SUSPEND		0x05		/* Suspended port */


/*
 * OpenHCI interrupt status information structure
 *
 * The Host Controller Driver (HCD) has to maintain two different sets of
 * Host Controller (HC) state information that includes HC registers, the
 * interrupt tables etc.. for the normal and polled modes.  In  addition,
 * suppose if we switched to polled mode while ohci  interrupt handler is
 * executing in the normal mode then we need to save the interrupt status
 * information that includes interrupts for which ohci interrupt  handler
 * is called and HCCA done head list in the polled mode. This infromation
 * will be used later in normal mode to  service those missed interrupts.
 * This will avoid race conditions like missing of normal mode's ohci SOF
 * and WriteDoneHead interrupts because of this polled switch.
 */
typedef struct ohci_save_intr_status {
	/*
	 * The following field has set of flags & these flags will be set
	 * in the ohci interrupt handler to indicate that currently  ohci
	 * interrupt handler is in execution and also while critical code
	 * execution within the ohci interrupt handler.  These flags will
	 * be verified in polled mode while saving the normal mode's ohci
	 * interrupt status information.
	 */
	uint_t		ohci_intr_flag;		/* Intr handler flags */

	/*
	 * The following fields will be used to save the interrupt status
	 * and the HCCA done head list that the ohci interrupt handler is
	 * currently handling.
	 */
	uint_t		ohci_curr_intr_sts;	/* Current interrupts */
	gtd		*ohci_curr_done_lst;	/* Current done head  */

	/*
	 * The following fields will be used to save the interrupt status
	 * and the HCCA done list currently being handled by the critical
	 * section of the ohci interrupt handler..
	 */
	uint_t		ohci_critical_intr_sts;	/* Critical interrupts */
	gtd		*ohci_critical_done_lst; /* Critical done head */

	/*
	 * The following fields will be used to save the interrupt status
	 * and HCCA done head list by the polled code if an  interrupt is
	 * pending when polled code is entered. These missed interrupts &
	 * done list will be serviced either in current  normal mode ohci
	 * interrupt handler execution or during the next  ohci interrupt
	 * handler execution.
	 */
	uint_t		ohci_missed_intr_sts;	/* Missed interrupts */
	gtd		*ohci_missed_done_lst;	/* Missed done head  */
} ohci_save_intr_status_t;

/*
 * These flags will be set in the the normal mode ohci  interrupt handler
 * to indicate that currently ohci interrupt handler is in  execution and
 * also while critical code  execution within the ohci interrupt handler.
 * These flags will be verified in the polled mode while saving the normal
 * mode's ohci interrupt status infromation.
 */
#define		OHCI_INTR_HANDLING	0x01	/* Handling ohci intrs */
#define		OHCI_INTR_CRITICAL	0x02	/* Critical intr code  */


/*
 * OpenHCI Host Controller state structure
 *
 * The Host Controller Driver (HCD) maintains the state of Host Controller
 * (HC). There is an openhci_state structure per instance  of the OpenHCI
 * host controller.
 */

typedef struct openhci_state {
	dev_info_t		*ohci_dip;		/* Dip of HC */
	uint_t			ohci_instance;
	usb_hcdi_ops_t		*ohci_hcdi_ops;		/* HCDI structure */
	uint_t			ohci_flags;		/* Used for cleanup */
	uint16_t		ohci_vendor_id;		/* chip vendor */
	uint16_t		ohci_device_id;		/* chip version */

	hcr_regs_t		*ohci_regsp;		/* Host ctlr regs */
	ddi_acc_handle_t	ohci_regs_handle;	/* Reg handle */

	ddi_acc_handle_t	ohci_config_handle;	/* Config space hndle */
	uint_t			ohci_frame_interval;	/* Frme inter reg */
	ddi_dma_attr_t		ohci_dma_attr;		/* DMA attributes */

	ddi_iblock_cookie_t	ohci_iblk_cookie;	/* iblock cookie */
	kmutex_t		ohci_int_mutex;		/* Mutex for struct */

	/* HCCA area */
	hcca_t			*ohci_hccap;		/* Virtual HCCA ptr */
	ddi_dma_cookie_t	ohci_hcca_cookie;	/* DMA cookie */
	ddi_dma_handle_t	ohci_hcca_dma_handle;	/* DMA handle */
	ddi_acc_handle_t	ohci_hcca_mem_handle;	/* Memory handle */

	/*
	 * There are two pools of memory. One pool contains the memory for
	 * the transfer descriptors and other pool contains the memory for
	 * the endpoint descriptors. The advantage of the pools is that it's
	 * easy to go back and forth between the iommu and the cpu addresses.
	 *
	 * The pools are protected by the ohci_int_mutex because the memory
	 * in the pools may be accessed by either the host controller or the
	 * host controller driver.
	 */

	/* General transfer descriptor pool */
	gtd			*ohci_td_pool_addr;	/* Start of the pool */
	ddi_dma_cookie_t	ohci_td_pool_cookie;	/* DMA cookie */
	ddi_dma_handle_t	ohci_td_pool_dma_handle;	/* DMA hndle */
	ddi_acc_handle_t	ohci_td_pool_mem_handle;	/* Mem hndle */

	/* Endpoint descriptor pool */
	hc_ed_t			*ohci_ed_pool_addr;	/* Start of the pool */
	ddi_dma_cookie_t	ohci_ed_pool_cookie;	/* DMA cookie */
	ddi_dma_handle_t	ohci_ed_pool_dma_handle;	/* DMA handle */
	ddi_acc_handle_t	ohci_ed_pool_mem_handle;	/* Mem handle */
	uint_t			ohci_dma_addr_bind_flag;	/* DMA flag */

	/* Condition variables */
	kcondvar_t		ohci_SOF_cv;		/* SOF variable */

	/* Condition variable for transfers completion event */
	kcondvar_t		ohci_xfer_cmpl_cv;	/* Xfer completion */

	/* Semaphore to serialize opens and closes */
	ksema_t			ohci_ocsem;

	/*
	 * Bandwidth fields
	 *
	 * The ohci_bandwidth array keeps track of the allocated bandwidth
	 * for this host controller. The ohci_bandwidth_isoch_sum field
	 * represents the sum of the allocated isochronous bandwidth. The
	 * total bandwidth allocated for least allocated list out of the 32
	 * interrupt lists is represented by the ohci_bandwdith_intr_min
	 * field.
	 */
	uint_t	ohci_bandwidth[NUM_INTR_ED_LISTS];
	uint_t	ohci_bandwidth_isoch_sum;
	uint_t	ohci_bandwidth_intr_min;

	uint_t	ohci_open_pipe_count;		/* no. of open pipes */
	/*
	 * Endpoint Reclamation List
	 *
	 * The interrupt or isochronous list processing cannot be stopped
	 * when a periodic endpoint is removed from the list. The endpoints
	 * are detached from the interrupt lattice tree and put on to the
	 * reclaimation list. On next SOF interrupt all those endpoints,
	 * which are on the reclaimation list will be deallocated.
	 */
	hc_ed_t			*ohci_reclaim_list;	/* Reclaimation list */

	struct root_hub		ohci_root_hub;		/* Root hub info */

	/* log handle for debug, console, log messages */
	usb_log_handle_t	ohci_log_hdl;

	/*
	 * ohci_dump_ops is used for dump support
	 */
	struct usb_dump_ops	*ohci_dump_ops;

	/*
	 * ohci_save_intr_stats is used to save the normal mode interrupt
	 * status information while executing interrupt handler & also by
	 * the polled code if an interrupt is pending for the normal mode
	 * when polled code is entered.
	 */
	ohci_save_intr_status_t	ohci_save_intr_status;

	/*
	 * Global transfer timeout handling & this transfer timeout handling
	 * will be per USB Host Controller.
	 */
	struct ohci_trans_wrapper	*ohci_timeout_list;
							/* Timeout List */
	timeout_id_t			ohci_timer_id;	/* Timer id  */

	/*
	 * kstat structures
	 */
	kstat_t				*ohci_intrs_stats;
	kstat_t				*ohci_total_stats;
	kstat_t				*ohci_count_stats[USB_N_COUNT_KSTATS];
} openhci_state_t;

typedef struct ohci_intrs_stats {
	struct kstat_named	ohci_hcr_intr_so;
	struct kstat_named	ohci_hcr_intr_wdh;
	struct kstat_named	ohci_hcr_intr_sof;
	struct kstat_named	ohci_hcr_intr_rd;
	struct kstat_named	ohci_hcr_intr_ue;
	struct kstat_named	ohci_hcr_intr_fno;
	struct kstat_named	ohci_hcr_intr_rhsc;
	struct kstat_named	ohci_hcr_intr_oc;
	struct kstat_named	ohci_hcr_intr_not_claimed;
	struct kstat_named	ohci_hcr_intr_total;
} ohci_intrs_stats_t;

/*
 * ohci kstat defines
 */
#define	OHCI_INTRS_STATS(ohci)	((ohci)->ohci_intrs_stats)
#define	OHCI_INTRS_STATS_DATA(ohci)	\
	((ohci_intrs_stats_t *)OHCI_INTRS_STATS((ohci))->ks_data)

#define	OHCI_TOTAL_STATS(ohci)	((ohci)->ohci_total_stats)
#define	OHCI_TOTAL_STATS_DATA(ohci)	(KSTAT_IO_PTR((ohci)->ohci_total_stats))
#define	OHCI_CTRL_STATS(ohci)	\
	(KSTAT_IO_PTR((ohci)->ohci_count_stats[USB_EPT_ATTR_CONTROL]))
#define	OHCI_BULK_STATS(ohci)	\
	(KSTAT_IO_PTR((ohci)->ohci_count_stats[USB_EPT_ATTR_BULK]))
#define	OHCI_INTR_STATS(ohci)	\
	(KSTAT_IO_PTR((ohci)->ohci_count_stats[USB_EPT_ATTR_INTR]))
#define	OHCI_ISOC_STATS(ohci)	\
	(KSTAT_IO_PTR((ohci)->ohci_count_stats[USB_EPT_ATTR_ISOCH]))

#define	OHCI_DO_INTRS_STATS(ohci, val) {				\
	if (OHCI_INTRS_STATS(ohci) != NULL) {				\
		OHCI_INTRS_STATS_DATA(ohci)->ohci_hcr_intr_total.value.ui64++; \
		switch (val) {						\
			case HCR_INTR_SO:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_so.value.ui64++;	\
				break;					\
			case HCR_INTR_WDH:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_wdh.value.ui64++;	\
				break;					\
			case HCR_INTR_SOF:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_sof.value.ui64++;	\
				break;					\
			case HCR_INTR_RD:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_rd.value.ui64++;	\
				break;					\
			case HCR_INTR_UE:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_ue.value.ui64++;	\
				break;					\
			case HCR_INTR_FNO:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_fno.value.ui64++;	\
				break;					\
			case HCR_INTR_RHSC:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_rhsc.value.ui64++;	\
				break;					\
			case HCR_INTR_OC:				\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_oc.value.ui64++;	\
				break;					\
			default:					\
				OHCI_INTRS_STATS_DATA(ohci)->		\
				    ohci_hcr_intr_not_claimed.value.ui64++; \
				    break;				\
		}							\
	}								\
}

#define	OHCI_DO_BYTE_STATS(ohci, len, usb_device, attr, addr) {		\
	uint8_t type = attr & USB_EPT_ATTR_MASK;			\
	uint8_t dir = addr & USB_EPT_DIR_MASK;				\
									\
	if (dir == USB_EPT_DIR_IN) {					\
		OHCI_TOTAL_STATS_DATA(ohci)->reads++;			\
		OHCI_TOTAL_STATS_DATA(ohci)->nread += len;		\
		switch (type) {						\
			case USB_EPT_ATTR_CONTROL:			\
				OHCI_CTRL_STATS(ohci)->reads++;		\
				OHCI_CTRL_STATS(ohci)->nread += len;	\
				break;					\
			case USB_EPT_ATTR_BULK:				\
				OHCI_BULK_STATS(ohci)->reads++;		\
				OHCI_BULK_STATS(ohci)->nread += len;	\
				break;					\
			case USB_EPT_ATTR_INTR:				\
				OHCI_INTR_STATS(ohci)->reads++;		\
				OHCI_INTR_STATS(ohci)->nread += len;	\
				break;					\
			case USB_EPT_ATTR_ISOCH:			\
				OHCI_ISOC_STATS(ohci)->reads++;		\
				OHCI_ISOC_STATS(ohci)->nread += len;	\
				break;					\
		}							\
	} else if (dir == USB_EPT_DIR_OUT) {				\
		OHCI_TOTAL_STATS_DATA(ohci)->writes++;			\
		OHCI_TOTAL_STATS_DATA(ohci)->nwritten += len;		\
		switch (type) {						\
			case USB_EPT_ATTR_CONTROL:			\
				OHCI_CTRL_STATS(ohci)->writes++;	\
				OHCI_CTRL_STATS(ohci)->nwritten += len;	\
				break;					\
			case USB_EPT_ATTR_BULK:				\
				OHCI_BULK_STATS(ohci)->writes++;	\
				OHCI_BULK_STATS(ohci)->nwritten += len;	\
				break;					\
			case USB_EPT_ATTR_INTR:				\
				OHCI_INTR_STATS(ohci)->writes++;	\
				OHCI_INTR_STATS(ohci)->nwritten += len;	\
				break;					\
			case USB_EPT_ATTR_ISOCH:			\
				OHCI_ISOC_STATS(ohci)->writes++;	\
				OHCI_ISOC_STATS(ohci)->nwritten += len;	\
				break;					\
		}							\
	}								\
}

/* warlock directives, stable data */
_NOTE(MUTEX_PROTECTS_DATA(openhci_state_t::ohci_int_mutex, openhci_state_t))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_iblk_cookie))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_dip))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_regsp))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_instance))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_vendor_id))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_device_id))

/* this may not be stable data in the future */
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_td_pool_addr))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_td_pool_mem_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_ed_pool_addr))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_ed_pool_mem_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_td_pool_cookie))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_ed_pool_cookie))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_hcca_mem_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_hccap))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_dma_addr_bind_flag))
_NOTE(DATA_READABLE_WITHOUT_LOCK(openhci_state_t::ohci_log_hdl))

/*
 * Define all ohci's Vendor-id and Device-id Here
 */
#define	RIO_CHIP	0x108e
#define	RIO_VER1	0x1103
#define	OHCI_RIO_REV1(ohcip)	((ohcip->ohci_vendor_id == RIO_CHIP) &&\
				(ohcip->ohci_device_id == RIO_VER1))
#define	OHCI_IS_RIO(ohcip)	(ohcip->ohci_vendor_id == RIO_CHIP)

/*
 * ohci_dma_addr_bind_flag values
 *
 * This flag indicates if the various DMA addresses allocated by the OHCI
 * have been bound to their respective handles. This is needed to recover
 * without errors from ohci_cleanup when it calls ddi_dma_unbind_handle()
 */
#define	OHCI_TD_POOL_BOUND	0x01	/* For TD pools  */
#define	OHCI_ED_POOL_BOUND	0x02	/* For ED pools  */
#define	OHCI_HCCA_DMA_BOUND	0x04	/* For HCCA area */

/*
 * Maximum SOF wait count
 */
#define	MAX_SOF_WAIT_COUNT	2	/* Wait for maximum SOF frames */


/*
 * USB device structure
 *
 * This structure represents the USB device and there is a structure per
 * address on the bus.
 */
typedef struct usb_dev {
	usb_device_t		*usb_dev_device_impl;	/* Ptr USBA struct */
	struct ohci_pipe_private *usb_dev_pipe_list;	/* List of pipes */
	kmutex_t		usb_dev_mutex;		/* Mutex for struct */
}usb_dev_t;

_NOTE(MUTEX_PROTECTS_DATA(usb_dev_t::usb_dev_mutex, usb_dev_t))


/*
 * Pipe private structure
 *
 * There is an instance of this structure per pipe.  This structure holds
 * HCD specific pipe information.  A pointer to this structure is kept in
 * the USBA pipe handle (usb_pipe_handle_impl_t).
 */
typedef struct ohci_pipe_private {
	usb_pipe_handle_impl_t	*pp_pipe_handle;	/* Back ptr to handle */
	hc_ed_t			*pp_ept;		/* Pipe's ept */

	/* State of the pipe */
	uint_t			pp_state;		/* See below */


	/* Local copy of the pipe policy */
	usb_pipe_policy_t	pp_policy;

	/* For Periodic Pipes Only */
	uint_t			pp_node;		/* Node in lattice */

	kmutex_t		pp_mutex;		/* Pipe mutex */

	/*
	 * Each pipe may have multiple transfer wrappers. Each transfer
	 * wrapper represents a USB transfer on the bus.  A transfer is
	 * made up of one or more transactions.
	 */
	struct ohci_trans_wrapper *pp_tw_head;	/* Head of the list */
	struct ohci_trans_wrapper *pp_tw_tail;	/* Tail of the list */

	struct ohci_pipe_private	*pp_next;	/* Next pipe */

	/* Done td count */
	uint_t			pp_count_done_tds;	/* Done td count */

	/* Flags */
	uint_t			pp_flag;		/* flags */
} ohci_pipe_private_t;

_NOTE(MUTEX_PROTECTS_DATA(ohci_pipe_private_t::pp_mutex, ohci_pipe_private_t))

/*
 * ohci pipe management:
 *
 * The following pipe states are for the both periodic and non-periodic pipes.
 *
 * OPENED:
 *	When a periodic or non-periodic pipe is opened, the pipe's state is
 * 	automatically set to OPENED.
 *
 * HALTED:
 *	When an error like device not responding, stall etc. occurs on either
 *	periodic or non-periodic pipes, the pipe's state is automatically set
 *	to HALTED.
 *
 * The following pipe states are only for the non-periodic pipes.
 *
 * INUSE:
 *	When a non-periodic pipe is in use, the pipe's state is automatically
 *	set to INUSE.
 *
 * The following pipe states are only for the periodic pipes.
 *
 * POLLING:
 *	When a polling or  restart polling is started on either interrupt or
 *	isochronous pipe, the pipe's state is automatically set to POLLING.
 *
 * STOPPED:
 *	When a stop polling is called either for the interrupt or for the
 *	isochronous pipe, the pipe's state is automatically set to STOPPED.
 *
 *
 * State Diagram for Bulk/Control
 *
 *			  --<-------- On Success --------------<--------^
 *                        |                                             |
 *                        v                                             |
 * ohci_hcdi_pipe_open->[OPENED]--ohci_hcdi_device_send/recv--->----->[INUSE]
 *			  ^					        |
 *			  |					        v
 *		           --ohci_hcdi_pipe_reset<-[HALTED]<- On Error -
 *
 *
 * State Diagram for Interrupt/Isochronous
 *
 *			  --<------ohci_hcdi_pipe_stop_polling----------^
 *			  |					        |
 *                        -->--[STOPPED]-----                           |
 *                                           |                          |
 *                                           v                          |
 * ohci_hcdi_pipe_open->[OPENED]--->--ohci_hcdi_pipe_start_polling-->[POLLING]
 *			  ^                                             |
 *			  |                                             v
 *		           --ohci_hcdi_pipe_reset<-[HALTED]<- On Error -
 *
 */

/* For any pipe */
#define		OPENED		1	/* Pipe has opened */
#define		HALTED		2	/* Pipe has halted */

/* For non-periodic pipes only */
#define		INUSE		3	/* Pipe is in use */

/* For periodic pipes only */
#define		POLLING		4	/* Polling the endpoint */
#define		STOPPED		5	/* Polling has stopped */

/* Flags */
#define		OHCI_WAIT_FOR_XFER_CMPL	1	/* Wait xfer completion */

/* Function prototype */
typedef void (*ohci_handler_function_t)(
	openhci_state_t			*ohcip,
	ohci_pipe_private_t		*pp,
	struct ohci_trans_wrapper	*tw,
	gtd				*td,
	void				*ohci_handle_callback_value);


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
typedef struct ohci_trans_wrapper {
	struct ohci_trans_wrapper	*tw_next;	/* Next wrapper */
	ohci_pipe_private_t		*tw_pipe_private;	/* Back ptr */
	size_t				tw_length;	/* Txfer length */
	ddi_dma_handle_t		tw_dmahandle;	/* DMA handle */
	ddi_acc_handle_t		tw_accesshandle;	/* Acc hndle */
	char				*tw_buf;	/* Buffer for txfer */
	ddi_dma_cookie_t		tw_cookie;	/* DMA cookie */
	int				tw_ctrl_state;	/* See below */
	uint_t				tw_num_tds;	/* Number of TDs */
	gtd				*tw_hctd_head;	/* Head TD */
	gtd				*tw_hctd_tail;	/* Tail TD */
	uint_t				tw_direction;	/* Direction of TD */
	uint_t				tw_flags;	/* Flags */
	uint32_t			tw_id;		/* 32bit ID */
	int				tw_timeout_value;
							/* Timeout value */
	struct ohci_trans_wrapper	*tw_timeout_next;
						/* Next TW on Timeout queue */

	/*
	 * This is the function to call when this td is done. This way
	 * we don't have to look in the td to figure out what kind it is.
	 */
	ohci_handler_function_t		tw_handle_td;

	/*
	 * This is the callback value used when processing a done td.
	 */
	usb_opaque_t			tw_handle_callback_value;
} ohci_trans_wrapper_t;

_NOTE(MUTEX_PROTECTS_DATA(openhci_state_t::ohci_int_mutex, ohci_trans_wrapper))

/*
 * Transfer Wrapper states for Control Transfer
 *
 * The tw_ctrl_state field represents the state of a control transaction.
 * A control transaction is made up of a setup, data (optional), and a
 * status phase.
 */
#define	SETUP		1	/* Setup phase */
#define	DATA		2	/* Data phase */
#define	STATUS		3	/* Status phase */

/*
 * Time waits for the different OHCI specific operations.
 * These timeout values are specified in terms of microseconds.
 */
#define	OHCI_RESET_TIMEWAIT	10000	/* HC reset waiting time */
#define	OHCI_RESUME_TIMEWAIT	10000	/* HC resume waiting time */
#define	OHCI_TIMEWAIT		10000	/* HC any other waiting time */

/* These timeout values are specified in seconds */
#define	OHCI_DEFAULT_XFER_TIMEOUT	5 /* Default transfer timeout */
#define	OHCI_MAX_SOF_TIMEWAIT		3 /* Maximum SOF waiting time */
#define	OHCI_XFER_CMPL_TIMEWAIT		3 /* Xfers completion timewait */

/*
 * Maximum allowable data transfer  size per transaction as supported
 * by OHCI is 8k. (See Open Host Controller Interface Spec rev 1.0a)
 * Currently we will use 4k per TD until problems associated with 8k
 * transfer gets resolved.
 */
#define	OHCI_MAX_TD_XFER_SIZE	0x1000	/* Maxmum data per transaction */

/*
 * The maximum allowable bulk data transfer size. It can be different
 * from OHCI_MAX_TD_XFER_SIZE and if it is more then ohci driver will
 * take care of  breaking a bulk data request into  multiples of ohci
 * OHCI_MAX_TD_XFER_SIZE until the request is satisfied.
 */
#define	OHCI_MAX_BULK_XFER_SIZE	0x20000	/* Maximum bulk transfer size */

/*
 * Host Contoller States
 */
#define	OHCI_CTLR_INIT		1	/* full initilization */
#define	OHCI_CTLR_RESTART	2	/* initialization on cpr resume */

/*
 * Timeout flags
 *
 * These flags will be used to stop the timer before timeout handler
 * gets executed.
 */
#define	OHCI_REMOVE_XFER_IFLAST	1	/* Stop the timer if  it is last TD */
#define	OHCI_REMOVE_XFER_ALWAYS	2	/* Stop the timer without condition */


/*
 * Bandwidth allocation
 *
 * The following definitions are  used during  bandwidth calculations
 * for a given endpoint maximum packet size.
 */
#define	MAX_BUS_BANDWIDTH	1500	/* Up to 1500 bytes per frame */
#define	MAX_POLL_INTERVAL	255	/* Maximum polling interval */
#define	MIN_POLL_INTERVAL	1	/* Minimum polling interval */
#define	SOF			6	/* Length in bytes of SOF */
#define	EOF			2	/* Length in bytes of EOF */
#define	TREE_HEIGHT		5	/* Log base 2 of 32 */

/*
 * Minimum polling interval for low speed endpoint
 *
 * According USB Specifications, a full-speed endpoint can specify
 * a desired polling interval 1ms to 255ms and a low speed endpoints
 * are limited to specifying only 10ms to 255ms. But some old keyboards
 * and mice uses polling interval of 8ms. For compatibility purpose,
 * we are using polling interval between 8ms and 255ms for low speed
 * endpoints. But ohci driver will reject any low speed endpoints which
 * request polling interval less than 8ms.
 */
#define	MIN_LOW_SPEED_POLL_INTERVAL	8

/*
 * For non-periodic transfers, reserve atleast for one low-speed device
 * transaction and according to USB Bandwidth Analysis white paper,  it
 * comes around 12% of USB frame time. Then periodic transfers will get
 * 88% of USB frame time.
 */
#define	MAX_PERIODIC_BANDWIDTH	(((MAX_BUS_BANDWIDTH - SOF - EOF)*88)/100)

/*
 * The following are the protocol overheads in terms of Bytes for the
 * different transfer types.  All these protocol overhead  values are
 * derived from the 5.9.3 section of USB Specification  and  with the
 * help of Bandwidth Analysis white paper which is posted on the  USB
 * developer forum.
 */
#define	FS_NON_ISOC_PROTO_OVERHEAD	14
#define	FS_ISOC_INPUT_PROTO_OVERHEAD	11
#define	FS_ISOC_OUTPUT_PROTO_OVERHEAD	10
#define	LOW_SPEED_PROTO_OVERHEAD	97
#define	HUB_LOW_SPEED_PROTO_OVERHEAD	01

/*
 * The Host Controller (HC) delays are the USB host controller specific
 * delays. The value shown below is the host  controller delay for  the
 * Sand core USB host controller.  This value was calculated and  given
 * by the Sun east coast USB hardware people.
 */
#define	HOST_CONTROLLER_DELAY		18

/*
 * The low speed clock below represents that to transmit one low-speed
 * bit takes eight times more than one full speed bit time.
 */
#define	LOW_SPEED_CLOCK			8


/*
 * Macros for setting/getting information
 */
#define	Get_ED(addr)		ddi_get32(ohcip->ohci_ed_pool_mem_handle, \
					(uint32_t *)&addr)

#define	Set_ED(addr, val)	ddi_put32(ohcip->ohci_ed_pool_mem_handle,  \
					((uint32_t *)&addr), \
					((int32_t)(val)))

#define	Get_TD(addr)		ddi_get32(ohcip->ohci_td_pool_mem_handle, \
					(uint32_t *)&addr)

#define	Set_TD(addr, val)	ddi_put32(ohcip->ohci_td_pool_mem_handle, \
					((uint32_t *)&addr), \
					((int32_t)(val)))

#define	Get_HCCA(addr)		ddi_get32(ohcip->ohci_hcca_mem_handle, \
					(uint32_t *)&addr)

#define	Set_HCCA(addr, val)	ddi_put32(ohcip->ohci_hcca_mem_handle, \
					((uint32_t *)&addr), \
					((int32_t)(val)))

#define	Get_OpReg(addr)		ddi_get32(ohcip->ohci_regs_handle, \
					(uint32_t *)&ohcip->ohci_regsp->addr)

#define	Set_OpReg(addr, val)	ddi_put32(ohcip->ohci_regs_handle, \
				((uint32_t *)&ohcip->ohci_regsp->addr), \
					((int32_t)(val)))

/*
 * Macros to speed handling of 32bit IDs
 */
#define	OHCI_GET_ID(x)		id32_alloc((void *)(x), KM_SLEEP)
#define	OHCI_LOOKUP_ID(x)	id32_lookup((x))
#define	OHCI_FREE_ID(x)		id32_free((x))


/*
 * Miscellaneous definitions.
 */

/* sKip bit actions */
#define	CLEAR_sKip	0
#define	SET_sKip	1

typedef uint_t		skip_bit_t;

/*
 * Setup Packet
 */
typedef struct setup_pkt {
	uchar_t	bmRequestType;
	uchar_t	bRequest;
	ushort_t wValue;
	ushort_t wIndex;
	ushort_t wLength;
}setup_pkt_t;

#define	SETUP_SIZE		8	/* Setup packet is always 8 bytes */

#define	REQUEST_TYPE_OFFSET	0
#define	REQUEST_OFFSET		1
#define	VALUE_OFFSET		2
#define	INDEX_OFFSET		4
#define	LENGTH_OFFSET		6

#define	TYPE_DEV_TO_HOST	0x80000000
#define	DEVICE			0x00000001
#define	CONFIGURATION		0x00000002

/*
 * Definitions for different Transfer types
 */
#define	ATTRIBUTES_CONTROL	0x00
#define	ATTRIBUTES_ISOCH	0x01
#define	ATTRIBUTES_BULK		0x02
#define	ATTRIBUTES_INTR		0x03

/*
 * The following are used in attach to   indicate
 * what has been succesfully allocated, so detach
 * can remove them.
 */
#define	OHCI_ATTACH		0x01	/* ohci driver initilization */
#define	OHCI_ZALLOC		0x02	/* Memory for ohci state structure */
#define	OHCI_INTR		0x04	/* Interrupt handler registered */
#define	OHCI_LOCKS		0x08	/* Mutexs and condition variables */
#define	OHCI_USBAREG		0x10	/* USBA registered */
#define	OHCI_RHREG		0x20	/* Root hub driver loaded */
#define	OHCI_CPR_SUSPEND	0x40	/* ohci being CPR suspended */
#define	OHCI_PM_SUSPEND		0x80	/* ohci being PM suspended */

#define	OHCI_UNIT(dev)	(getminor((dev)))

/*
 * Debug printing
 * Masks
 */
#define	PRINT_MASK_ATTA		0x00000001	/* Attach time */
#define	PRINT_MASK_LISTS	0x00000002	/* List management */
#define	PRINT_MASK_ROOT_HUB	0x00000004	/* Root hub stuff */
#define	PRINT_MASK_ALLOC	0x00000008	/* Alloc/dealloc descr */
#define	PRINT_MASK_INTR		0x00000010	/* Interrupt handling */
#define	PRINT_MASK_BW		0x00000020	/* Bandwidth */
#define	PRINT_MASK_CBOPS	0x00000040	/* CB-OPS */
#define	PRINT_MASK_HCDI		0x00000080	/* HCDI entry points */
#define	PRINT_MASK_DUMPING	0x00000100	/* Dump ohci info */
#define	PRINT_MASK_ALL		0xFFFFFFFF


#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_OHCID_H */
