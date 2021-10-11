/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_SCSA2USB_H
#define	_SYS_USB_SCSA2USB_H

#pragma ident	"@(#)scsa2usb.h	1.1	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSA2USB: This header file contains the internal structures
 * and variable definitions used in usb mass storage disk driver.
 */


#define	SCSA2USB_INITIAL_ALLOC	4	/* initial soft space alloc */

#define	MAX_COMPAT_NAMES   	2	/* max compatible names for children */
#define	SERIAL_NUM_LEN		20	/* for reading string descriptor */
#define	SCSA2USB_SERIAL_LEN	12	/* len of serial no in scsi_inquiry */
#define	GET_MAX_LUN		0xfe	/* get max luns supported */
#define	SCSA2USB_MAX_BULK_XFER	0x2000 	/* maximum bulk xfer size allowed */

/* zip supports 1 lun. this is for other mass storage device support. */
#define	SCSA2USB_MAX_TARGETS	0x10	/* maximum targets supported. */



/*
 * PM support
 */
typedef struct scsa2usb_power  {
	/* this is the bit mask of the power states that device has */
	uint8_t		scsa2usb_pwr_states;

	/* flag to indicate if driver is about to raise power level */
	boolean_t	scsa2usb_raise_power;

	uint8_t		scsa2usb_current_power;
} scsa2usb_power_t;


/*
 * Per bulk device "state" data structure.
 */
typedef struct scsa2usb_state {
	int			scsa2usb_instance;	/* Instance number    */
	dev_info_t		*scsa2usb_dip;		/* Per device. info   */
	int			scsa2usb_dev_state;	/* USB device state */
	scsa2usb_power_t	*scsa2usb_pm;		/* PM state info */
	int			scsa2usb_flags; 	/* Per instance flags */
	int			scsa2usb_intfc_num;	/* Interface number   */

	kmutex_t		scsa2usb_mutex;		/* Per instance lock  */

	struct scsi_hba_tran	*scsa2usb_tran;		/* SCSI transport ptr */
	struct scsi_pkt		*scsa2usb_cur_pkt;	/* SCSI packet ptr    */

	uchar_t			scsa2usb_serial_no[SCSA2USB_SERIAL_LEN];
							/* Serial no. string  */
	dev_info_t		*scsa2usb_target_dip[SCSA2USB_MAX_TARGETS];
					/* store devinfo per target  */

	usb_endpoint_descr_t	scsa2usb_intr_ept;	/* Control ept descr  */
	usb_endpoint_descr_t	scsa2usb_bulkin_ept;	/* Bulk In descriptor */
	usb_endpoint_descr_t	scsa2usb_bulkout_ept;	/* Bulkout descriptor */

	usb_pipe_handle_t	scsa2usb_default_pipe;	/* Default pipe	Hndle */
	usb_pipe_handle_t	scsa2usb_bulkin_pipe;	/* Bulk Inpipe Handle */
	usb_pipe_handle_t	scsa2usb_bulkout_pipe;	/* Bulk Outpipe Hndle */
	usb_pipe_policy_t	scsa2usb_pipe_policy;	/* pipe policy	*/

	usb_interface_descr_t	scsa2usb_intfc_descr;	/* Interface descr   */
	uint_t			scsa2usb_pipe_state;	/* resetting pipes */

	ddi_dma_attr_t		*scsa2usb_dma_attr;	/* HCD dma attribues */
	usb_log_handle_t	scsa2usb_log_handle;	/* log handle	*/

	uint_t			scsa2usb_tag;		/* current tag */
	uint_t			scsa2usb_pkt_state;	/* packet state */

	struct usb_dump_ops	*scsa2usb_dump_ops;	/* dump support */

	size_t			scsa2usb_max_bulk_xfer_size; /* from HCD */

	ddi_eventcookie_t	scsa2usb_remove_cookie;	/* event handling */
	ddi_eventcookie_t	scsa2usb_insert_cookie;	/* event handling */

	size_t			scsa2usb_lbasize;	/* sector size */
	uint_t			scsa2usb_n_luns;	/* number of luns */

	uint_t			scsa2usb_reset_delay;
					/* delay after resetting device */
	struct scsa2usb_cpr	*scsa2usb_cpr_info;	/* for cpr info */
	struct scsa2usb_cpr	*scsa2usb_panic_info;	/* for cpr info */

	size_t			scsa2usb_totalsec;	/* total sectors */
	size_t			scsa2usb_secsz;		/* sector size */
	uint_t			scsa2usb_msg_count;	/* for debug msgs */
} scsa2usb_state_t;


/* for warlock */
_NOTE(MUTEX_PROTECTS_DATA(scsa2usb_state::scsa2usb_mutex, scsa2usb_state))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_log_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_bulkin_pipe))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_bulkout_pipe))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_default_pipe))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_dip))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_target_dip))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_instance))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_intfc_num))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsa2usb_state::scsa2usb_flags))
_NOTE(SCHEME_PROTECTS_DATA("stable data", usb_pipe_policy))


/* scsa2usb_pipe_state values */
#define	SCSA2USB_PIPE_NORMAL		0x00	/* no reset or clearing	*/
#define	SCSA2USB_PIPE_DEFAULT_RESET	0x01	/* reset the default pipe */
#define	SCSA2USB_PIPE_BULK_IN_RESET	0x02	/* reset the bulk in pipe */
#define	SCSA2USB_PIPE_BULK_OUT_RESET	0x04	/* reset the bulk out pipe */
#define	SCSA2USB_PIPE_BULK_IN_CLEAR_STALL 0x10	/* clear bulk in pipe stall */
#define	SCSA2USB_PIPE_BULK_OUT_CLEAR_STALL 0x20	/* clear bulk out pipe stall */
#define	SCSA2USB_PIPE_DEV_RESET		0x40	/* reset the usb zip device */
#define	SCSA2USB_PIPE_CLOSING		0x80	/* closing all pipes */


/* pkt xfer state machine */
#define	SCSA2USB_PKT_NONE		0	/* device is idle */
#define	SCSA2USB_PKT_XFER_SEND_CBW	1	/* device xferred command */
#define	SCSA2USB_PKT_XFER_DATA		2	/* device xferred data */
#define	SCSA2USB_PKT_RECEIVE_CSW_1	3	/* device receiving status 1 */
#define	SCSA2USB_PKT_RECEIVE_CSW_2	4	/* device receiving status 2 */
#define	SCSA2USB_PKT_PROCESS_CSW_1	5	/* device processing status 1 */
#define	SCSA2USB_PKT_PROCESS_CSW_2	6	/* device processing status 1 */
#define	SCSA2USB_PKT_DO_COMP		7	/* device is done xfer */

/* scsa2usb_flags values */
#define	SCSA2USB_FLAGS_PIPES_OPENED	0x001	/* usb pipes are open */
#define	SCSA2USB_FLAGS_HBA_ATTACH_SETUP	0x002	/* scsi hba setup done */
#define	SCSA2USB_FLAGS_QUIESCED		0x004	/* Quiesce the usb/scsi bus */


/* check if it is ok to access the device and send command to it */
#define	SCSA2USB_DEVICE_ACCESS_OK(s) \
	((((s)->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) != 0) && \
	(((s)->scsa2usb_dev_state == USB_DEV_ONLINE)))

/* check if we are in any reset */
#define	SCSA2USB_IN_RESET(s) \
	((((s)->scsa2usb_pipe_state & SCSA2USB_PIPE_DEFAULT_RESET) != 0) || \
	(((s)->scsa2usb_pipe_state & SCSA2USB_PIPE_BULK_IN_RESET) != 0) || \
	(((s)->scsa2usb_pipe_state & SCSA2USB_PIPE_BULK_OUT_RESET) != 0) || \
	(((s)->scsa2usb_pipe_state & SCSA2USB_PIPE_DEV_RESET) != 0))

/* check if the device is busy */
#define	SCSA2USB_BUSY(s) \
	(((s)->scsa2usb_cur_pkt) || \
	((s)->scsa2usb_pipe_state != SCSA2USB_PIPE_NORMAL) || \
	((s)->scsa2usb_pkt_state != SCSA2USB_PKT_NONE))

/* check if we're doing cpr */
#define	SCSA2USB_CHK_CPR(s) \
	(((s)->scsa2usb_dev_state == USB_DEV_CPR_SUSPEND))

/* check if we're either paniced or in cpr state */
#define	SCSA2USB_CHK_PANIC_CPR(s) \
	(ddi_in_panic() || SCSA2USB_CHK_CPR(s))

/* update pkt_state prior to pkt_comp getting called */
#define	SCSA2USB_UPDATE_PKT_STATE(pkt, cmd) \
	if ((pkt)->pkt_reason == CMD_CMPLT) { \
		(pkt)->pkt_state = STATE_GOT_BUS | STATE_GOT_TARGET |\
					STATE_SENT_CMD | STATE_GOT_STATUS; \
		if ((cmd)->cmd_xfercount) { \
			(pkt)->pkt_state |= STATE_XFERRED_DATA; \
		} \
	}

/* reset scsa2usb state after pkt_comp is called */
#define	SCSA2USB_RESET_CUR_PKT(s) \
	(s)->scsa2usb_cur_pkt = NULL; \
	(s)->scsa2usb_pkt_state = SCSA2USB_PKT_NONE;

/* print a panic sync message to the console */
#define	SCSA2USB_PRINT_SYNC_MSG(m, s) \
	if ((m) == B_TRUE) { \
		USB_DPRINTF_L0(DPRINT_MASK_SCSA, (s)->scsa2usb_log_handle, \
		    "syncing not supported"); \
		(m) = B_FALSE; \
	}

/* Cancel callbacks registered during attach time */
#define	SCSA2USB_CANCEL_CB(id) \
	if ((id)) { \
		(void) callb_delete((id)); \
		(id) = 0; \
	}

/* Set SCSA2USB_PKT_DO_COMP state if there is active I/O */
#define	SCSA2USB_SET_PKT_DO_COMP_STATE(s) \
	if ((s)->scsa2usb_cur_pkt) { \
		(s)->scsa2usb_pkt_state = SCSA2USB_PKT_DO_COMP; \
	}

#define	SCSA2USB_FREE_MSG(data) \
	if ((data)) { \
		freemsg((data)); \
	}

/* SCSA related */
#define	ADDR2TRAN(ap)		((ap)->a_hba_tran)
#define	TRAN2SCSA2USB(tran)	((scsa2usb_state_t *)(tran)->tran_hba_private)
#define	ADDR2SCSA2USB(ap)	(TRAN2SCSA2USB(ADDR2TRAN(ap)))


/*
 * The scsa2usb_cpr_info data structure is used for cpr related
 * callbacks. It is used for panic callbacks as well.
 */
typedef struct scsa2usb_cpr {
	callb_cpr_t		cpr;		/* for cpr related info */
	struct scsa2usb_state	*statep;	/* for scsa2usb state info */
	kmutex_t		lockp;		/* mutex used by cpr_info_t */
} scsa2usb_cpr_t;


/*
 * The scsa2usb_cmd data structure is defined here. It gets
 * initialized per command that is sent to the device.
 */
typedef struct scsa2usb_cmd {
	struct scsi_pkt		*cmd_pkt;		/* copy of pkt ptr */
	size_t			cmd_xfercount;		/* current xfer count */
	uchar_t			cmd_dir;		/* direction */
	uchar_t			cmd_cdb[SCSI_CDB_SIZE];	/* CDB */
	uchar_t			cmd_actual_len; 	/* cdb len */
	uchar_t			cmd_cdblen;		/* requested  cdb len */
	int			cmd_scblen;		/* status length */
	int			cmd_tag;		/* tag */
	struct	buf		*cmd_bp;		/* copy of bp ptr */
	int			cmd_timeout;		/* copy of pkt_time */
	uchar_t 		cmd_scb[1]; 		/* status, no arq */

	/* used in multiple up xfers */
	size_t			cmd_total_xfercount;	/* total xfer val */
	size_t			cmd_offset;		/* offset into buf */
	int			cmd_lba;		/* current xfer lba */
	int			cmd_done;		/* command done? */
	struct scsa2usb_cmd	*cmd_cb;
} scsa2usb_cmd_t;

#define	SCSA2USB_BULK_PIPE_TIMEOUT	((clock_t)(2 * USB_PIPE_TIMEOUT))

/* scsa2usb_cdb position of fields in CDB */
#define	SCSA2USB_OPCODE		0		/* Opcode field */
#define	SCSA2USB_LUN		1		/* LUN field */
#define	SCSA2USB_LBA_0		2		/* LBA[0] field */
#define	SCSA2USB_LBA_1		3		/* LBA[1] field */
#define	SCSA2USB_LBA_2		4		/* LBA[2] field */
#define	SCSA2USB_LBA_3		5		/* LBA[3] field */
#define	SCSA2USB_LEN_0		7		/* LEN[0] field */
#define	SCSA2USB_LEN_1		8		/* LEN[1] field */


/* for warlock */
_NOTE(SCHEME_PROTECTS_DATA("unique per packet or safe sharing",
    scsi_cdb scsi_status scsi_pkt buf scsa2usb_cmd))
_NOTE(SCHEME_PROTECTS_DATA("stable data", scsi_device scsi_address))

/* macros to convert a pkt to cmd and vice-versa */
#define	PKT2CMD(pkt)		((scsa2usb_cmd_t *)(pkt)->pkt_ha_private)
#define	CMD2PKT(sp)		((sp)->cmd_pkt

/*
 * this is calculated based on the following formula:
 * total_transfer_count - (data_transferred_so_far - residue reported)
 */
#define	SCSA2USB_RESID(cmd, x)	((cmd)->cmd_total_xfercount - \
					((cmd)->cmd_xfercount - (x)))


/*
 * This structure collects all info about a callback taskq.
 */
typedef struct scsa2usb_cb_info {
	scsa2usb_cmd_t		*c_qf;		/* pointer to the first cmd */
	scsa2usb_cmd_t		*c_qb;		/* pointer to the last cmd */
	kmutex_t		c_mutex;
	uchar_t			c_cb_active;	/* thread is active */
	uchar_t			c_ref_count;
} scsa2usb_cb_info_t;


/*
 * The following data structure is used to save the values returned
 * by the READ_CAPACITY command. lba is the max allowed logical block
 * address and blen is max allowed block size.
 */
typedef struct scsa2usb_read_cap {
	uchar_t	scsa2usb_read_cap_lba3;		/* Max lba supported */
	uchar_t	scsa2usb_read_cap_lba2;
	uchar_t	scsa2usb_read_cap_lba1;
	uchar_t	scsa2usb_read_cap_lba0;
	uchar_t	scsa2usb_read_cap_blen3;	/* Max block size supported */
	uchar_t	scsa2usb_read_cap_blen2;
	uchar_t	scsa2usb_read_cap_blen1;
	uchar_t	scsa2usb_read_cap_blen0;
} scsa2usb_read_cap_t;

#define	SCSA2USB_INT(a, b, c, d) \
		(((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

/* debug and error msg logging */
#define	DPRINT_MASK_SCSA	0x0001		/* for SCSA */
#define	DPRINT_MASK_ATTA	0x0002		/* for ATTA */
#define	DPRINT_MASK_EVENTS	0x0004		/* for event handling */
#define	DPRINT_MASK_CALLBACKS	0x0008		/* for callbacks  */
#define	DPRINT_MASK_TIMEOUT	0x0010		/* for timeouts */
#define	DPRINT_MASK_DUMPING	0x0020		/* for dumping */
#define	DPRINT_MASK_PM		0x0040		/* for pwr mgmt */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_SCSA2USB_H */
