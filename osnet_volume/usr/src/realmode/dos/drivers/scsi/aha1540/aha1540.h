/*
 *  Copyright (c) 1997, by Sun Microsystems, Inc.
 *  All rights reserved.
 */

#ident "@(#)aha1540.h	1.7	97/10/20 SMI"

#ifndef _AHA1540_H_
#define	_AHA1540_H_

/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This header file is provided for use with aha1540.c, the source for
 *	the Adaptec AHA 154x realmode driver AHA1540.BEF.  AHA1540.BEF is used
 *	as both a working driver and a sample SCSI realmode driver for
 *	reference when developing realmode drivers for other SCSI adapters.
 *
 *	This file contains only definitions used by hardware-specific code
 *	in aha1540.c.  There are no constructs that need to be replicated in
 *	header files for other drivers.  Other drivers will normally have a
 *	similar header file for device-dependent definitions but use of a
 *	header file is not required by the driver framework.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

/*
 * Definitions for the Adaptec AHA-1540 AT-bus Intelligent SCSI Host Adapter.
 *
 */

/*
 * Adapter I/O ports.  These are offsets from dcb_ioaddr1.
 */
#define	AHACTL		0	/* Host Adapter Control Port (WRITE) */
#define	AHASTAT		0	/* Status Port (READ) */
#define	AHADATACMD	1	/* Command (WRITE) and Data (READ/WRITE) Port */
#define	AHAINTFLGS	2	/* Interrupt Flags Port (READ) */

/*
 * Bit definitions for AHACTL port:
 */
#define	CTL_HRST	0x80	/* Hard Reset of Host Adapter */
#define	CTL_SRST	0x40	/* Soft Reset (clears commands, no diags) */
#define	CTL_IRST	0x20	/* Interrupt Reset (clears AHAINTFLGS) */
#define	CTL_SCRST	0x10	/* Reset SCSI Bus */

/*
 * Bit definitions for AHASTAT port:
 */
#define	STAT_STST	0x80	/* Self-test in progress */
#define	STAT_DIAGF	0x40	/* Internal Diagnostic Failure */
#define	STAT_INIT	0x20	/* Mailbox Init required */
#define	STAT_IDLE	0x10	/* Adapter is Idle */
#define	STAT_CDF	0x08	/* AHADATACMD (outgoing) is full */
#define	STAT_DF		0x04	/* AHADATACMD (incoming) is full */
#define	STAT_RSRVD	0x02	/* Reserved bit (always 0) */
#define	STAT_INVDCMD	0x01	/* Invalid host adapter command */
#define	STAT_MASK	0xfd	/* mask for valid status bits */

/* AHASTAT bit combinations used for probe testing */
#define	STAT_PROBE_ONBITS	(STAT_INIT | STAT_IDLE)
#define	STAT_PROBE_OFFBITS	(STAT_CDF | STAT_DF)

/*
 * Bit definitions for AHAINTFLGS port:
 */
#define	INT_ANY		0x80	/* Any interrupt (set when bits 0-3 valid) */
#define	INT_SCRD	0x08	/* SCSI reset detected */
#define	INT_HACC	0x04	/* Host Adapter command complete */
#define	INT_MBOE	0x02	/* MailBox (outgoing) Empty */
#define	INT_MBIF	0x01	/* MailBox (incoming) Full */

/*
 * AHA-1540 Host Adapter Command Opcodes:
 * NOTE: for all multi-byte values sent in AHADATACMD, MSB is sent FIRST
 */
#define	CMD_NOP		0x00	/* No operation, sets INT_HACC */

#define	CMD_MBXINIT	0x01	/* Initialize Mailboxes */
				/* ARGS: count, 1-255 valid */
				/*	 mbox addr (3 bytes) */

#define	CMD_DOSCSI	0x02	/* Start SCSI (Scan outgoing mailboxes) */

#define	CMD_ATBIOS	0x03	/* Start AT BIOS command */
				/* ARGS: 10 bytes, see 1540 doc, sect 6.1.3 */

#define	CMD_ADINQ	0x04	/* Adapter Inquiry */
				/* RETURNS: 4 bytes of firmware info */

#define	CMD_MBOE_CTL	0x05	/* Enable/Disable INT_MBOE interrupt */
				/* ARG: 0 - Disable, 1 - Enable */

#define	CMD_SELTO_CTL	0x06	/* Set SCSI Selection Time Out */
				/* ARGS: 0 - TO off, 1 - TO on */
				/*	 Reserved (0) */
				/*	 Time-out value (in ms, 2 bytes) */

#define	CMD_BONTIME	0x07	/* Set Bus-ON Time */
				/* ARG: time in microsec, 2-15 valid */

#define	CMD_BOFFTIME	0x08	/* Set Bus-OFF Time */
				/* ARG: time in microsec, 0-250 valid */

#define	CMD_XFERSPEED	0x09	/* Set AT bus burst rate (MB/sec) */
				/* ARG: 0 - 5MB, 1 - 6.7 MB, */
				/* 2 - 8MB, 3 - 10MB */
				/* 4 - 5.7MB (1542 ONLY!) */

#define	CMD_INSTDEV	0x0a	/* Return Installed Devices */
				/* RETURNS: 8 bytes, one per target ID, */
				/* starting with 0.  Bits set (bitpos=LUN) */
				/* in each indicate Unit Ready */
				/* NOTE: Can only be executed ONCE after */
				/*	 power up or any RESET */

#define	CMD_CONFIG	0x0b	/* Return Configuration Data */
				/* RETURNS: 3 bytes, which are: */
				/* byte 0: DMA Channel, bit encoded as */
#define	CFG_DMA_CH0	0x01	/*		Channel 0 */
#define	CFG_DMA_CH5	0x20	/*		Channel 5 */
#define	CFG_DMA_CH6	0x40	/*		Channel 6 */
#define	CFG_CMD_CH7	0x80	/*		Channel 7 */
#define	CFG_DMA_MASK	0xe1	/*		(mask for above) */
				/* byte 1: Interrupt Channel, bit encoded as */
#define	CFG_INT_CH9	0x01	/*		Channel 9 */
#define	CFG_INT_CH10	0x02	/*		Channel 10 */
#define	CFG_INT_CH11	0x04	/*		Channel 11 */
#define	CFG_INT_CH12	0x08	/*		Channel 12 */
#define	CFG_INT_CH14	0x20	/*		Channel 14 */
#define	CFG_INT_CH15	0x40	/*		Channel 15 */
#define	CFG_INT_MASK	0x6f	/*		(mask for above) */
				/* byte 2: Host Adapter SCSI ID, in binary */
#define	CFG_ID_MASK	0x03	/*		(mask for above) */

#define	CMD_RETSETUP	0x0d	/* Return Setup Data */

#define	CMD_WTFIFO	0x1c	/* Write Adapter FIFO Buffer */
				/* ARG: addr (3 bytes) of 56 bytes to write */

#define	CMD_RDFIFO	0x1d	/* Read Adapter FIFO Buffer */
				/* ARG: addr (3 bytes) of buf for 56 bytes */

#define	CMD_ECHO	0x1f	/* Echo Data */
				/* ARG: one byte of test data */
				/* RETURNS: the same byte (hopefully) */

#define	CMD_GET_EXTBIOS 0x28	/* Return Extended BIOS Information */

#define	CMD_UNLOCK_MBOX 0x29	/* Lock/Unlock Mailbox Interface */


/*
 * The Mail Box Structure.  NOTE: THE AHA-1540 REQUIRES ADDRESSES IN REVERSE
 *			    BYTE ORDER FROM THE 80386.
 */
struct mbox_entry {
	unchar mbx_cmdstat;	/* Command/Status byte (below) */
	unchar mbx_ccb_addr[3];	/* AHA-1540-style CCB address */
};

/*
 * Command codes for mbx_cmdstat:
 */
#define	MBX_FREE	0	/* Available mailbox slot */

#define	MBX_CMD_START	1	/* Start SCSI command described by CCB */
#define	MBX_CMD_ABORT	2	/* Abort SCSI command described by CCB */

#define	MBX_STAT_DONE	1	/* CCB completed without error */
#define	MBX_STAT_ABORT	2	/* CCB was aborted by host */
#define	MBX_STAT_CCBNF	3	/* CCB for ABORT request not found */
#define	MBX_STAT_ERROR	4	/* CCB completed with error */

/*
 * The Adaptec Host Adapter Command Control Block (CCB)
 */
#define	MAX_CDB_LEN	12	/* Max size of SCSI Command Descriptor Block */
#define	MAX_DMA_SEGS	16	/* Max # of Scatter/Gather DMA segments */

/* a Scatter/Gather DMA Segment Descriptor */
struct aha_dma_seg {
	unchar dma_len[3];	/* segment length */
	unchar dma_ptr[3];	/* segment address */
	};

struct aha_ccb {
	unchar ccb_op;		/* CCB Operation Code */
	unchar ccb_targ;	/* Target/LUN byte (defined below) */
	unchar ccb_cdblen;	/* Length of SCSI Command Descriptor Block */
	unchar ccb_reqsense;	/* request sense allocation */
	unchar ccb_dlen[3];	/* Data Length (msb, mid, lsb) */
	unchar ccb_dptr[3];	/* Data (buffer) pointer (msb, mid, lsb) */
	unchar ccb_link[3];	/* Link Pointer (msb, mid, lsb) */
	unchar ccb_linkid;	/* Command Link ID */
	unchar ccb_hastat;	/* Host Adapter status */
	unchar ccb_tarstat;	/* Target Status */
	unchar ccb_reserved2[2];
/*
 * The following allows space for MAX_CDB_LEN bytes of SCSI CDB, followed
 * by 14 bytes of sense data acquired by the AHA-1540 in the event of an
 * error.  The beginning of the sense data will be:
 *		&aha_ccb.ccb_cdb[aha_ccb.ccb_cdblen]
 */
	unchar ccb_cdb[MAX_CDB_LEN+14];
	struct aha_dma_seg ccb_dma[MAX_DMA_SEGS]; /* scatter/gather segs */
	unchar ccb_scratch[64];		/* spare buffer space, if needed */
};

typedef struct aha_ccb AHA_CCB;

/*
 * The following structure defines the ccb_targ field.  It is not included
 * in the structure above due to 'C' alignment problems...
 */
union targ_field
	{
	struct
		{
		uint tf_lun : 3;	/* LUN on target device */
		uint tf_in : 1;		/* 'incoming' xfer, length is checked */
		uint tf_out : 1;	/* 'outgoing' xfer, length is checked */
		uint tf_tid : 3;	/* Target ID */
		} tff;
	unchar tfc;
	};

/*
 * ccb_op values:
 */
#define	COP_COMMAND	0	/* Normal SCSI Command */
#define	COP_RESET	0x81	/* Bus Device Reset (Aborts all outstanding */
				/* commands against the target) */

/*
 * ccb_hastat values:
 */
#define	HS_SELTO	0x11	/* Selection Time Out */

/* defines for the assorted wait intervals required by this controller */
#define	WAIT_RETRIES 		5000
#define	INIT_RETRIES		10000
#define	RESET_SETTLE_TIME	2000

/*
 * Parameters used for the AHA1640 micro-channel adapter
 */
#define	AHA1640_SIGNATURE	0x0F1F	/* characteristic board ID */
#define	BT646_SIGNATURE		0x0708	/* characteristic board ID */

#define SLOT_SELECT     0x96       /* POS/slot select register */
#define SLOT_ENABLE     0x8        /* enable access to a given slot */
#define INTR_DISABLE    0x07       /* disable h/w interrupt channel */
#define MAXSLOTS        0x08       /* max number of slots that we probe */

#define POS0   0x100    /* POS register definitions */
#define POS1   0x101
#define POS2   0x102
#define POS4   0x104


#endif /* _AHA1540_H_ */
