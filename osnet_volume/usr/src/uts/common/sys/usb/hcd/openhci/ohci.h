/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_OHCI_H
#define	_SYS_USB_OHCI_H

#pragma ident	"@(#)ohci.h	1.6	99/09/24 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This header file describes the registers and data structures shared by
 * the OpenHCI USB controller (HC) and the Host Controller Driver (HCD).
 */

/*
 * For handling RIO specific operations
 */
#define		RIO			1	/* RIO specific operations */

#ifdef	RIO
#define		MAX_RH_PORTS		4	/* Maximum root hub ports */
#else
#define		MAX_RH_PORTS		15	/* Maximum root hub ports */
#endif	/* RIO */

/*
 * USB Host controller DMA scatter gather list defines for
 * Sparc and non-sparc architectures.
 */
#if defined(sparc)
#define	OHCI_DMA_ATTR_MAX_XFER		0xffffffffull
#define	OHCI_DMA_ATTR_COUNT_MAX		0xffffffffull
#define	OHCI_DMA_ATTR_GRANULAR		512
#else
#define	OHCI_DMA_ATTR_MAX_XFER		0x00ffffffull
#define	OHCI_DMA_ATTR_COUNT_MAX		0x00ffffffull
#define	OHCI_DMA_ATTR_GRANULAR		1
#endif

/*
 * OpenHCI Operational Registers
 *
 * The Host Controller (HC) contains a set of on-chip operational registers
 * which are mapped into a noncacheable portion of the system addressable
 * space and these registers are also used by the Host Controller Driver
 * (HCD).
 */
typedef volatile struct hcr_regs {
	uint32_t 	hcr_revision;		/* Specification version */
	uint32_t	hcr_control;		/* Control information */
	uint32_t	hcr_cmd_status;		/* Controller status */
	uint32_t	hcr_intr_status;	/* Interrupt status register */
	uint32_t 	hcr_intr_enable;	/* Interrupt enable */
	uint32_t	hcr_intr_disable;	/* Interrupt disable */
	uint32_t	hcr_HCCA;		/* Pointer to HCCA */
	uint32_t	hcr_periodic_curr;	/* Curr. isoch or int endpt */
	uint32_t	hcr_ctrl_head;		/* Head of contrl list */
	uint32_t	hcr_ctrl_curr;		/* Curr. control endpt */
	uint32_t	hcr_bulk_head;		/* Head of the bulk list */
	uint32_t	hcr_bulk_curr;		/* Curr. bulk endpt */
	uint32_t	hcr_done_head;		/* Head of the done list */
	uint32_t	hcr_frame_interval;	/* Frame interval value */
	uint32_t 	hcr_frame_remaining;    /* Time remaining in frame */
	uint32_t	hcr_frame_number;	/* Frame number */
	uint32_t	hcr_periodic_strt;	/* Time to start per. list */
	uint32_t	hcr_transfer_ls;	/* Low speed threshold */
	uint32_t	hcr_rh_descriptorA;	/* Root hub register A */
	uint32_t	hcr_rh_descriptorB;	/* Root hub register B */
	uint32_t	hcr_rh_status;		/* Root hub status */
	uint32_t 	hcr_rh_portstatus[MAX_RH_PORTS];
						/* Status of root hub ports */
#ifdef	RIO
	uint32_t	hcr_rio_core_ctrl;	/* RIO core control */
	uint32_t	hcr_rio_intr_sts;	/* RIO interrupt status */
	uint32_t	hcr_rio_preftch_ctrl;	/* RIO prefetch control */
	uint32_t	hcr_rio_cache_tag;	/* RIO cache tag */
	uint32_t	hcr_rio_slv_acc_ctrl;	/* RIO slave access control */
#endif	/* RIO */
} hcr_regs_t;

/* hcr_revision bits */
#define	HCR_REVISION_1_0	0x00000010	/* Revision 1.0 */
#define	HCR_REVISION_MASK	0x000000FF	/* Revision mask */

/* hcr_control bits */
#define	HCR_CONTROL_CBSR	0x00000003	/* Control/bulk ratio */
#define	HCR_CONTROL_PLE		0x00000004	/* Periodic list enable */
#define	HCR_CONTROL_IE		0x00000008	/* Isochronous enable */
#define	HCR_CONTROL_CLE		0x00000010	/* Control list enable */
#define	HCR_CONTROL_BLE		0x00000020	/* Bulk list enable */
#define	HCR_CONTROL_HCFS	0x000000C0	/* Controller state */
#define	HCR_CONTROL_IR		0x00000100	/* Interrupt routing */
#define	HCR_CONTROL_RWC		0x00000200	/* Remote wakeup connected */
#define	HCR_CONTROL_RWE		0x00000400	/* Remote wakeup enabled */

/* Values for the Host Controller Functional State bits (HCR_CONTROL_HCFS) */
#define	HCR_CONTROL_RESET	0x00000000	/* USB Reset */
#define	HCR_CONTROL_RESUME	0x00000040	/* USB Resume */
#define	HCR_CONTROL_OPERAT	0x00000080	/* USB Operational */
#define	HCR_CONTROL_SUSPD	0x000000C0	/* USB Suspend */

/* hcr_status bits */
#define	HCR_STATUS_RESET	0x00000001	/* Host controller reset */
#define	HCR_STATUS_CLF		0x00000002	/* Control list filled */
#define	HCR_STATUS_BLF		0x00000004	/* Bulk list filled */
#define	HCR_STATUS_OCR		0x00000008	/* Ownership change */
#define	HCR_STATUS_SOC		0x00030000	/* Error frame count */

/* hcr_intr_status bits and hcr_intr_mask bits */
#define	HCR_INTR_SO		0x00000001	/* Schedule overrun */
#define	HCR_INTR_WDH		0x00000002	/* Writeback done head */
#define	HCR_INTR_SOF		0x00000004	/* Start of frame */
#define	HCR_INTR_RD		0x00000008	/* Resume detected */
#define	HCR_INTR_UE		0x00000010	/* Unrecoverable error */
#define	HCR_INTR_FNO		0x00000020	/* Frame no. overflow */
#define	HCR_INTR_RHSC		0x00000040	/* Root hub status change */
#define	HCR_INTR_OC		0x40000000	/* Change in ownership */
#define	HCR_INTR_MIE		0x80000000	/* Master interrupt enable */

/* hcr_frame_interval bits */
#define	HCR_FRME_INT_FI		0x00003FFF	/* Frame interval */
#define	HCR_FRME_INT_FSMPS	0x7FFF0000	/* Biggest packet */
#define	HCR_FRME_FSMPS_SHFT	16		/* FSMPS */
#define	HCR_FRME_INT_FIT	0x80000000	/* Frame interval toggle */
#define	MAX_OVERHEAD		210		/* Max. bit overhead */

/* hcr_frame_remaining bits */
#define	HCR_FRME_REM_FR		0x00003FFF	/* Frame remaining */
#define	HCR_FRME_REM_FRT	0x80000000	/* Frame remaining toggle */

/* hcr_transfer_ls */
#define	HCR_TRANS_LST		0x000007FF	/* Low Speed threshold */

/* hcr_rh_descriptorA bits */
#define	HCR_RHA_NDP		0x000000FF	/* No. of ports */
#define	HCR_RHA_PSM		0x00000100	/* Power switch mode */
#define	HCR_RHA_NPS		0x00000200	/* No power switching */
#define	HCR_RHA_DT		0x00000400	/* Device type */
#define	HCR_RHA_OCPM		0x00000800	/* Over-current protection */
#define	HCR_RHA_NOCP		0x00001000	/* No over-current protection */
#define	HCR_RHA_PTPGT		0xFF000000	/* Power on to power good */
#define	HCR_RHA_PTPGT_SHIFT	24		/* Shift bits for ptpgt */

/* hcr_rh_descriptorB bits */
#define	HCR_RHB_DR		0x0000FFFF	/* Device removable */
#define	HCR_RHB_PPCM		0xFFFF0000	/* PortPowerControlMask */

/* hcr_rh_status bits */
#define	HCR_RH_STATUS_LPS	0x00000001	/* Local power status */
#define	HCR_RH_STATUS_OCI	0x00000002	/* Over current indicator */
#define	HCR_RH_STATUS_DRWE	0x00008000	/* Device remote wakeup */
#define	HCR_RH_STATUS_LPSC	0x00010000	/* Local power status change */
#define	HCR_RH_STATUS_OCIC	0x00020000	/* Over current indicator */
#define	HCR_RH_STATUS_CRWE	0x80000000	/* Clear remote wakeup enable */
#define	HCR_RH_STATUS_MASK	0x10038003	/* Status mask */

/* hcr_rh_portstatus bits */
#define	HCR_PORT_CCS		0x00000001	/* Current connect status */
#define	HCR_PORT_PES		0x00000002	/* Port enable */
#define	HCR_PORT_PSS		0x00000004	/* Port suspend status */
#define	HCR_PORT_POCI		0x00000008	/* Port over crrnt indicator */
#define	HCR_PORT_PRS		0x00000010	/* Port reset status */
#define	HCR_PORT_PPS		0x00000100	/* Port power status */
#define	HCR_PORT_CPP		0x00000200	/* Clear port power */
#define	HCR_PORT_LSDA		0x00000200	/* Low speed device */
#define	HCR_PORT_CSC		0x00010000	/* Connect status change */
#define	HCR_PORT_PESC		0x00020000	/* Port enable status change */
#define	HCR_PORT_PSSC		0x00040000	/* Port suspend status change */
#define	HCR_PORT_OCIC		0x00080000	/* Port over current change */
#define	HCR_PORT_PRSC		0x00100000	/* Port reset status chnge */
#define	HCR_PORT_MASK		0x001F031F	/* Reserved written as 0 */
#define	HCR_PORT_CHNG_MASK	0x001F0000	/* Mask for change bits */

#ifdef	RIO
/* hcr_rio_preftch_ctrl  Eanble - 0 and Disable - 1 */
#define	HCR_RIO_CACHE		0x00010001	/* RIO Cache enable/disable */
#endif	/* RIO */

#define	DONE_QUEUE_INTR_COUNTER	0x7		/* Done queue intr counter */

/*
 * Host Controller Communications Area
 *
 * The Host Controller Communications Area (HCCA) is a 256-byte structre
 * of system memory that is established by the Host Controller Driver (HCD)
 * and this structre is used for communication between HCD and HC. The HCD
 * maintains a pointer to this structure in the Host Controller (HC). This
 * structure must be aligned to a 256-byte boundary.
 */

#define	NUM_INTR_ED_LISTS	32	/* Number of interrupt lists */
#define	NUM_STATIC_NODES	31	/* Number of static endpoints */

typedef volatile struct hcca {
	uint32_t	HccaIntTble[NUM_INTR_ED_LISTS]; /* 32 intr lists */
							/* Ptrs to hc_ed */
	uint16_t	HccaFrameNo;		/* Current frame number */
	uint16_t 	HccaPad;		/* 0 when HC updates FrameNo */
	uint32_t	HccaDoneHead;		/* Head ptr */
	uint8_t		HccaReserved[118];	/* Reserved area */
} hcca_t;

#define	HCCA_DONE_HEAD_MASK	0xfffffffe	/* Mask off the Lsb */
#define	HCCA_DONE_HEAD_LSB	0x00000001	/* Lsb of the Done Head */


/*
 * Host Controller Endpoint Descriptor
 *
 * An Endpoint Descriptor (ED) is a memory structure that describes the
 * information necessary for the Host Controller (HC) to communicate with
 * a device endpoint.  An ED includes a Transfer Descriptor (TD) pointer.
 * This structure must be aligned to a 16 byte boundary.
 */
typedef volatile struct hc_ed {
	uint32_t	hced_ctrl;	/* See below */
	uint32_t	hced_tailp;	/* (gtd *) End of trans. list */
	uint32_t	hced_headp;	/* (gtd *) Next trans. */
	uint32_t	hced_next;	/* (hc_ed *) Next endpoint */
	uint32_t	hced_prev;	/* (hc_ed *)Virt addr. of prev ept */
	uint32_t	hced_reclaim_next;	/* (hc_ed *) Reclaim list */
	uint32_t	hced_node;	/* The node that its attached */
	uint32_t	hced_flag;	/* Set for static endpoint */
} hc_ed_t;

/*
 * hc_endpoint_descriptor control bits
 */
#define	HC_EPT_FUNC	0x0000007F		/* Address of function */
#define	HC_EPT_EP	0x00000780		/* Address of endpoint */
#define	HC_EPT_DataFlow 0x00001800		/* Direction of data flow */
#define	HC_EPT_DF_IN    0x00001000		/* Data flow in */
#define	HC_EPT_DF_OUT	0x00000800		/* Data flow out */
#define	HC_EPT_Speed   	0x00002000		/* Speed of the endpoint */
#define	HC_EPT_sKip	0x00004000		/* Skip bit */
#define	HC_EPT_Format   0x00008000		/* Type of transfer */
#define	HC_EPT_MPS	0x0EFF0000		/* Max packet size */
#define	HC_EPT_8_MPS	0x00080000		/* 8 byte max packet size */
#define	HC_EPT_64_MPS	0x00400000		/* 64 byte max packet size */
#define	HC_EPT_Halt	0x00000001		/* Halted */
#define	HC_EPT_Carry	0x00000002		/* Toggle carry */

#define	HC_EPT_EP_SHFT	7			/* Bits to shift addr */
#define	HC_EPT_MAXPKTSZ	16			/* Bits to shift maxpktsize */

#define	HC_EPT_TD_TAIL	0xFFFFFFF0		/* TD tail mask */
#define	HC_EPT_TD_HEAD	0xFFFFFFF0		/* TD head mask */
#define	HC_EPT_NEXT	0xFFFFFFF0		/* Next endpoint mask */

#define	HC_EPT_BLANK	0xFFFFFFFF		/* Blank endpoint */
#define	HC_TD_DUMMY	0x10101010		/* Dummy td */

#define	HC_EPT_STATIC	0x00000001		/* static endpoint */


/*
 * Host Controller Transfer Descriptor
 *
 * A Transfer Descriptor (TD) is a memory structure that describes the
 * information necessary for the Host Controller (HC) to transfer a block
 * of data to or from a device endpoint. These TD's will be attached to
 * a Endpoint Descriptor (ED). This structure includes the fields for both
 * General and Isochronous Transfer Descriptors. This structure must be
 * aligned to a 16 byte boundary.
 */
typedef	volatile struct hc_gtd {
	uint32_t	hcgtd_trans_wrapper;	/* Transfer wrapper */
	uint32_t	hcgtd_td_state;		/* TD state */
	uint32_t	hcgtd_next_td;		/* Next td transfer */
	uint8_t		hcgtd_pad[4];		/* padding */

	uint32_t	hcgtd_ctrl;		/* See below */
	uint32_t	hcgtd_cbp;		/* Next buffer addr */
	uint32_t	hcgtd_next;		/* (gtd *)  Next td */
	uint32_t	hcgtd_buf_end;		/* End of buffer */

	uint32_t	hcgtd_offsets[4];	/* Offsets into buf */
						/* Used for isoch */
} gtd;

#define	GTD_WRAPPER		16

/*
 * hc_gtd control bits
 */

#define	HC_GTD_R	0x00040000		/* Buffer rounding */
#define	HC_GTD_PID	0x00180000		/* Pid for the token */
#define	HC_GTD_IN	0x00100000		/* In direction */
#define	HC_GTD_OUT	0x00080000		/* Out direction */
#define	HC_GTD_DI	0x00E00000		/* Delay interrupt */
#define	HC_GTD_1I	0x00200000		/* 1 frame for interrupt */
#define	HC_GTD_4I	0x00400000		/* 4 frame's for interrupt */
#define	HC_GTD_T	0x03000000		/* Data Toggle */
#define	HC_GTD_T_TD_0	0x02000000		/* Toggle from TD 0 */
#define	HC_GTD_T_TD_1	0x03000000		/* Toggle from TD 1 */
#define	HC_GTD_EC	0x0C000000		/* Error Count */
#define	HC_GTD_CC	(uint_t)0xF0000000	/* Condition code */

#define	HC_GTD_CC_NO_E	0x00000000		/* No error */
#define	HC_GTD_CC_CRC	0x10000000		/* CRC error */
#define	HC_GTD_CC_BS	0x20000000		/* Bit stuffing */
#define	HC_GTD_CC_DTM	0x30000000		/* Data Toggle Mismatch */
#define	HC_GTD_CC_STALL	0x40000000		/* Stall */
#define	HC_GTD_CC_DNR	0x50000000		/* Device not responding */
#define	HC_GTD_CC_PCF	0x60000000		/* PID check failure */
#define	HC_GTD_CC_UPID	0x70000000		/* Unexpected PID */
#define	HC_GTD_CC_DO	(uint_t)0x80000000	/* Data overrrun */
#define	HC_GTD_CC_DU	(uint_t)0x90000000	/* Data underrun */
#define	HC_GTD_CC_BO	(uint_t)0xC0000000	/* Buffer overrun */
#define	HC_GTD_CC_BU	(uint_t)0xD0000000	/* Buffer underrun */

#define	HC_GTD_NEXT	(uint_t)0xFFFFFFF0	/* Next TD */
#define	HC_TD_BLANK	(uint_t)0xFFFFFFFF	/* Blank td */

/*
 * hcgtd_td_state
 *
 * TD States
 */
#define	TD_RECLAIM	0x1			/* TD must be reclaimed */
#define	TD_TIMEOUT	0x2			/* TD timeout */

/*
 * hc_itd control bits
 */
#define	HC_ITD_MASK		0x00E00000	/* Mask gtd ctrl info out */
#define	HC_ITD_FRAME		0x0000FFFF	/* Frame number */
#define	HC_ITD_N		0x07000000	/* Frame count */
#define	HC_ITD_CC		0xE0000000	/* Completion code */

#define	HC_ITD_PAGE_MASK	0xFFFFF000

#define	HC_ITD_ODD_OFFSET	0xFFFF0000	/* Odd offset */
#define	HC_ITD_EVEN_OFFSET	0x0000FFFF	/* Even offset */
#define	HC_ITD_OFFSET_SHIFT	16

#define	HC_ITD_SIZE		0x0777


#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_OHCI_H */
