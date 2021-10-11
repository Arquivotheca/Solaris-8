/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_UHCI_H
#define	_SYS_USB_UHCI_H

#pragma ident	"@(#)uhci.h	1.12	99/10/07 SMI"

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

#define		LEGACYMODE_REG_OFFSET		0xc0

#define		LEGACYMODE_REG_INIT_VALUE	0xaf00

#define		UHCI_4K_ALIGN			0x1000

/*
 *   The register set of the UCHI controller
 */

#pragma pack(1)
typedef volatile struct hcr_regs {
	uint16_t	USBCMD;
	uint16_t	USBSTS;
	uint16_t	USBINTR;
	uint16_t	FRNUM;
	uint32_t	FRBASEADD;
	uchar_t		SOFMOD;
	uchar_t		rsvd[3];
	uint16_t	PORTSC[2];
} hc_regs_t;
#pragma pack()

/*
 *    #defines for the USB Command register
 */

#define		USBCMD_REG_MAXPKT_64		0x0080
#define		USBCMD_REG_CONFIG_FLAG		0x0040
#define		USBCMD_REG_SW_DEBUG		0x0020
#define		USBCMD_REG_FGBL_RESUME		0x0010
#define		USBCMD_REG_ENER_GBL_SUSPEND	0x0008
#define		USBCMD_REG_GBL_RESET		0x0004
#define		USBCMD_REG_HC_RESET		0x0002
#define		USBCMD_REG_HC_RUN		0x0001


/*
 *    #defines for the USB Status  register
 */

#define		USBSTS_REG_HC_HALTED		0x0020
#define		USBSTS_REG_HC_PROCESS_ERR	0x0010
#define		USBSTS_REG_HOST_SYS_ERR 	0x0008
#define		USBSTS_REG_RESUME_DETECT	0x0004
#define		USBSTS_REG_USB_ERR_INTR		0x0002
#define		USBSTS_REG_USB_INTR		0x0001

/*
 *    #defines for the USB root hub port register
 */

#define		HCR_PORT_CCS			0x1
#define		HCR_PORT_CSC			0x2
#define		HCR_PORT_ENABLE			0x4
#define		HCR_PORT_ENDIS_CHG		0x8
#define		HCR_PORT_LINE_STATSU		0x30
#define		HCR_PORT_RESUME_DETECT		0x40
#define		HCR_PORT_LSDA			0x100
#define		HCR_PORT_RESET			0x200
#define		HCR_PORT_SUSPEND		0x1000

/*
 *   #defines for USB interrupt Enable register
 */

#define		USBINTR_REG_SPINT_EN		0x0008
#define		USBINTR_REG_IOC_EN		0x0004
#define		USBINTR_REG_RESUME_INT_EN	0x0002
#define		USBINTR_REG_TOCRC_INT_EN	0x0001

#define		ENABLE_ALL_INTRS		0x000F
#define		DISABLE_ALL_INTRS		0x0000
#define		UHCI_INTR_MASK			0x1f

/*
 *  Hold the neccessary information related to adding/deleting queue head.
 */

#pragma pack(1)

typedef struct hc_qh {
	uint32_t	link_ptr;	/* Next Queue Head / TD */
	uint32_t	element_ptr;	/* Next queue head / TD	*/
	uint16_t	node;		/* Node	that its attached */
	uint16_t	qh_flag;	/* See	below */

	struct	hc_qh	*prev_qh;	/* Pointer to Prev queue head */
	struct	hc_td	*td_tailp;	/* Pointer to the last TD of QH	*/
	struct	uhci_bulk_xfer_info *bulk_xfer_info;
	uchar_t		rsvd[8];
} queue_head_t;

#define		NUM_STATIC_NODES		63
#define		NUM_INTR_QH_LISTS		64
#define		NUM_FRAME_LST_ENTRIES		1024
#define		TREE_HEIGHT			5
#define		VIRTUAL_TREE_HEIGHT		5
#define		SIZE_OF_FRAME_LST_TABLE		1024 * 4

#define		HC_TD_HEAD			0x0
#define		HC_QUEUE_HEAD			0x2

#define		HC_END_OF_LIST			0x1

#define		QUEUE_HEAD_FLAG_STATIC		0x1
#define		QUEUE_HEAD_FLAG_FREE		0x2
#define		QUEUE_HEAD_FLAG_BUSY		0x3

#define		QH_LINK_PTR_MASK		0xFFFFFFF0
#define		QH_ELEMENT_PTR_MASK		0xFFFFFFF0
#define		FRAME_LST_PTR_MASK		0xFFFFFFF0


/*
 *  Holds the Tansfer descriptor information sending a request
 */

typedef struct hc_td {

	/* Information required by HC for executing the request */
	uint32_t	link_ptr;	/* Pointer to the next TD/QH */
	struct {
		uint32_t
			Actual_len:11,	/* Actual length of data xfer */
			rsvd1:5,
			status:8,	/* Status of the TD */
			ioc:1,		/* =1, interrupt on completion */
			iso:1,		/* =1, for isochronous xfers */
			ls:1,		/* =1, low speed devices */
			c_err:2,	/* Number time HC retries the TD */
			spd:1,		/* =1, interrupt on underrun */
			rsvd:2;
	} td_dword2;
	struct	{
		uint32_t
			PID:8,		/* Token ID */
			device_addr:7,	/* Device address */
			endpt:4,	/* End	point number */
			data_toggle:1,	/* Data toggle sychronize bit */
			rsvd:1,
			max_len:11;	/* Max xfer length */
	} td_dword3;
	uint32_t		buffer_address;	/* Data buffer address   */

	/* Information required by HCD for managing the request */
	struct	hc_td			*qh_td_prev;
	struct	hc_td			*tw_td_next;
	struct	hc_td			*oust_td_next;
	struct	hc_td			*oust_td_prev;
	struct	uhci_trans_wrapper	*tw;
	ushort_t			flag;
	uchar_t				rsvd[10];
} gtd;

typedef struct uhci_bulk_xfer_info {
	uint32_t		bulk_pool_addr;
	ddi_dma_cookie_t	uhci_bulk_cookie;	/* DMA cookie */
	ddi_dma_handle_t	uhci_bulk_dma_handle;	/* DMA handle */
	ddi_acc_handle_t	uhci_bulk_mem_handle;	/* Memory handle */
	ushort_t		num_tds;
} uhci_bulk_xfer_t;

#pragma pack()

#define			TD_FLAG_FREE			0x1
#define			TD_FLAG_BUSY			0x2
#define			TD_FLAG_DUMMY			0x3

#define			UHCI_ERR_STATUS_MASK		0x7F
#define			INTERRUPT_ON_COMPLETION		0x1
#define			END_POINT_ADDRESS_MASK		0xF
#define			UHCI_MAX_ERR_COUNT		3
#define			MAX_NUM_BULK_TDS_PER_XFER	32

#define			UHCI_TD_STALLED			0x40
#define			UHCI_TD_DATA_BUFFER_ERR		0x20
#define			UHCI_TD_BABBLE_ERR		0x10
#define			UHCI_TD_NAK_RECEIVED		0x08
#define			UHCI_TD_CRC_TIMEOUT		0x04
#define			UHCI_TD_BITSTUFF_ERR		0x02

#define			TD_INACTIVE		0x7F
#define			TD_ACTIVE		0x80
#define			TD_STATUS_MASK		0x7E
#define			ZERO_LENGTH		0x7FF

#define			PID_SETUP		0x2D
#define			PID_IN			0x69
#define			PID_OUT			0xe1

#define			SETUP_SIZE		8

#define			SETUP			0x11
#define			DATA			0x12
#define			STATUS			0x13

#define			UHCI_INVALID_PTR	NULL
#define			LOW_SPEED_DEVICE	1

typedef	uint32_t	frame_lst_table_t;

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

/*
 * Minimum polling interval for low speed endpoint
 *
 * According USB Specifications, a full-speed endpoint can specify
 * a desired polling interval 1ms to 255ms and a low speed endpoints
 * are limited to specifying only 10ms to 255ms. But some old keyboards
 * and mice uses polling interval of 8ms. For compatibility purpose,
 * we are using polling interval between 8ms and 255ms for low speed
 * endpoints. But ohci driver will reject any low speed endpoints which
 * request polling interval less than 8ms. So, UHCI also does the same.
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
 * Sand core USB host controller.
 */
#define	HOST_CONTROLLER_DELAY	18

/*
 * The low speed clock below represents that to transmit one low-speed
 * bit takes eight times more than one full speed bit time.
 */
#define	LOW_SPEED_CLOCK		8

#define	 UHCI_QH_ALIGN_SZ	16
#define	 UHCI_TD_ALIGN_SZ	16

#ifdef __cplusplus
}
#endif

#endif /* _SYS_USB_UHCI_H */
