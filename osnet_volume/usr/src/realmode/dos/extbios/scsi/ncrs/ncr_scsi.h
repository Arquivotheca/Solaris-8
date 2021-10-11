/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
#pragma ident	"@(#)ncr_scsi.h	1.3	97/07/21 SMI"
 *
 */

/*
 * This file contains parts of various Solaris SCSI-related header files
 * to allow the real mode driver to use as much code as possible from
 * the Solaris driver without modification.
 */

/*
 * Taken from Solaris message.h.
 */
#define	MSG_IDENTIFY		0x80

/*
 * Taken from Solaris generic/commands.h.
 */
#define	CDB_GROUP5	12	/* 12-byte cdb's */

/*
 * Taken from Solaris impl/commands.h.
 */
#define	CDB_SIZE	CDB_GROUP5

union scsi_cdb {		/* scsi command description block */
	struct {
		u_char	cmd;		/* cmd code (byte 0) */
		u_char tag	:5;	/* rest of byte 1 */
		u_char lun	:3;	/* lun (byte 1) */
		union {			/* bytes 2 - 31 */

		u_char	scsi[CDB_SIZE-2];
		/*
		 *	G R O U P   0   F O R M A T (6 bytes)
		 */
#define		scc_cmd		cdb_un.cmd
#define		scc_lun		cdb_un.lun
#define		g0_addr2	cdb_un.tag
#define		g0_addr1	cdb_un.sg.g0.addr1
#define		g0_addr0	cdb_un.sg.g0.addr0
#define		g0_count0	cdb_un.sg.g0.count0
#define		g0_vu_1		cdb_un.sg.g0.vu_57
#define		g0_vu_0		cdb_un.sg.g0.vu_56
#define		g0_flag		cdb_un.sg.g0.flag
#define		g0_link		cdb_un.sg.g0.link
	/*
	 * defines for SCSI tape cdb.
	 */
#define		t_code		cdb_un.tag
#define		high_count	cdb_un.sg.g0.addr1
#define		mid_count	cdb_un.sg.g0.addr0
#define		low_count	cdb_un.sg.g0.count0
		struct scsi_g0 {
			u_char addr1;	    /* middle part of address	*/
			u_char addr0;	    /* low part of address	*/
			u_char count0;	    /* usually block count	*/
			u_char link	:1; /* another command follows 	*/
			u_char flag	:1; /* interrupt when done 	*/
			u_char rsvd	:4; /* reserved 		*/
			u_char vu_56	:1; /* vendor unique(byte5 bit6)*/
			u_char vu_57	:1; /* vendor unique(byte5 bit7)*/
		} g0;


		/*
		 *	G R O U P   1   F O R M A T  (10 byte)
		 */
#define		g1_reladdr	cdb_un.tag
#define		g1_rsvd0	cdb_un.sg.g1.rsvd1
#define		g1_addr3	cdb_un.sg.g1.addr3	/* msb */
#define		g1_addr2	cdb_un.sg.g1.addr2
#define		g1_addr1	cdb_un.sg.g1.addr1
#define		g1_addr0	cdb_un.sg.g1.addr0	/* lsb */
#define		g1_count1	cdb_un.sg.g1.count1	/* msb */
#define		g1_count0	cdb_un.sg.g1.count0	/* lsb */
#define		g1_vu_1		cdb_un.sg.g1.vu_97
#define		g1_vu_0		cdb_un.sg.g1.vu_96
#define		g1_flag		cdb_un.sg.g1.flag
#define		g1_link		cdb_un.sg.g1.link
		struct scsi_g1 {
			u_char addr3;	/* most sig. byte of address 	*/
			u_char	addr2;
			u_char	addr1;
			u_char	addr0;
			u_char rsvd1;	/* reserved (byte 6) 		*/
			u_char count1;  /* transfer length (msb) 	*/
			u_char count0;	/* transfer length (lsb) 	*/
			u_char link	:1; /* another command follows 	*/
			u_char flag	:1; /* interrupt when done 	*/
			u_char rsvd0	:4; /* reserved 		*/
			u_char vu_96	:1; /* vendor unique(byte9 bit6)*/
			u_char vu_97	:1; /* vendor unique(byte9 bit7)*/
		} g1;


		/*
		 *	G R O U P   5   F O R M A T  (12 byte)
		 */
#define		scc5_reladdr	cdb_un.tag
#define		scc5_addr3	cdb_un.sg.g5.addr3	/* msb 		*/
#define		scc5_addr2	cdb_un.sg.g5.addr2
#define		scc5_addr1	cdb_un.sg.g5.addr1
#define		scc5_addr0	cdb_un.sg.g5.addr0	/* lsb 		*/
#define		scc5_count3	cdb_un.sg.g5.count3	/* msb 		*/
#define		scc5_count2	cdb_un.sg.g5.count2
#define		scc5_count1	cdb_un.sg.g5.count1
#define		scc5_count0	cdb_un.sg.g5.count0	/* lsb 		*/
#define		scc5_vu_1	cdb_un.sg.g5.v117
#define		scc5_vu_0	cdb_un.sg.g5.v116
#define		scc5_flag	cdb_un.sg.g5.flag
		struct scsi_g5 {
			u_char addr3;	/* most sig. byte of address 	*/
			u_char addr2;
			u_char addr1;
			u_char addr0;
			u_char count3;	/* most sig. byte of count	*/
			u_char count2;
			u_char count1;
			u_char count0;
			u_char rsvd1;	/* reserved 			*/
			u_char link	:1; /* another command follows 	*/
			u_char flag	:1; /* interrupt when done 	*/
			u_char rsvd0	:4; /* reserved 		*/
			u_char vu_116	:1; /* vendor unique(byte11,bit6)*/
			u_char vu_117	:1; /* vendor unique(byte11,bit7)*/
		} g5;
		}sg;
	} cdb_un;
	u_char cdb_opaque[CDB_SIZE];	/* addressed as an opaque char array */
	u_long cdb_long[CDB_SIZE / sizeof (long)]; /* as a longword array */
};


/*
 * Derived from Solaris SCSI status.h.
 */

/*
 * The definition of of the Status block as a bitfield
 */

struct scsi_status {
        u_char  sts_vu0         : 1,    /* vendor unique                */
                sts_chk         : 1,    /* check condition              */
                sts_cm          : 1,    /* condition met                */
                sts_busy        : 1,    /* device busy or reserved      */
                sts_is          : 1,    /* intermediate status sent     */
                sts_vu6         : 1,    /* vendor unique                */
                sts_vu7         : 1,    /* vendor unique                */
                sts_resvd       : 1;    /* reserved                     */
};
#define sts_scsi2       sts_vu6         /* SCSI-2 modifier bit          */

/*
 * Derived from Solaris scsi_pkt.h.
 * Stripped down for real mode driver.
 */

/*
 * SCSI packet definition.
 *
 *	This structure defines the packet which is allocated by a library
 *	function and handed to a target driver. The target driver fills
 *	in some information, and passes it to the library for transport
 *	to an addressed SCSI target/lun/sublun. The host adapter found by
 *	the library fills in some other information as the command is
 *	processed. When the command completes (or can be taken no further)
 *	the function specified in the packet is called with a pointer to
 *	the packet as it argument. From fields within the packet, the target
 *	driver can determine the success or failure of the command.
 */

struct scsi_pkt {
	long	pkt_resid;		/* data bytes not transferred */
	u_long	pkt_state;		/* state of command */
	u_long	pkt_statistics;		/* statistics */
	u_char  pkt_reason;		/* reason completion called */
};
#define pkt_bytexfer	pkt_resid

/*
 * Definitions for the pkt_flags field.
 */
#define	FLAG_NOINTR	0x0001	/* Run command without interrupts */
#define	FLAG_NODISCON	0x0002	/* Run command without disconnects */
#define	FLAG_SUBLUN	0x0004	/* Use the sublun field in pkt_address */
#define	FLAG_NOPARITY	0x0008	/* Run command without parity checking */
#define	FLAG_HTAG	0x1000	/* Run command as HEAD OF QUEUE tagged cmd */
#define	FLAG_OTAG	0x2000	/* Run command as ORDERED QUEUE tagged cmd */
#define	FLAG_STAG	0x4000	/* Run command as SIMPLE QUEUE tagged cmd */

#define	FLAG_IGNOVRUN	0x0100	/* Ignore overruns */
#define	FLAG_QUEHOLD	0x0200	/* Don't advance HA que until cmd completes */
#define	FLAG_SENSING	0x0400	/* Running request sense for failed pkt */
#define	FLAG_HEAD	0x8000	/* Run command as HEAD OF QUEUE tagged cmd */

#define	FLAG_TAGMASK	(FLAG_HTAG|FLAG_OTAG|FLAG_STAG)

/*
 * Definitions for Uscsi options to the flags field
 */
#define	FLAG_SILENT	0x00010000
#define	FLAG_DIAGNOSE	0x00020000
#define	FLAG_ISOLATE	0x00040000

/*
 * Definitions for the pkt_reason field.
 */

#define	CMD_CMPLT	0	/* no transport errors- normal completion */
#define	CMD_INCOMPLETE	1	/* transport stopped with not normal state */
#define	CMD_DMA_DERR	2	/* dma direction error occurred */
#define	CMD_TRAN_ERR	3	/* unspecified transport error */
#define	CMD_RESET	4	/* SCSI bus reset destroyed command */
#define	CMD_ABORTED	5	/* Command transport aborted on request */
#define	CMD_TIMEOUT	6	/* Command timed out */
#define	CMD_DATA_OVR	7	/* Data Overrun */
#define	CMD_CMD_OVR	8	/* Command Overrun */
#define	CMD_STS_OVR	9	/* Status Overrun */
#define	CMD_BADMSG	10	/* Message not Command Complete */
#define	CMD_NOMSGOUT	11	/* Target refused to go to Message Out phase */
#define	CMD_XID_FAIL	12	/* Extended Identify message rejected */
#define	CMD_IDE_FAIL	13	/* Initiator Detected Error message rejected */
#define	CMD_ABORT_FAIL	14	/* Abort message rejected */
#define	CMD_REJECT_FAIL	15	/* Reject message rejected */
#define	CMD_NOP_FAIL	16	/* No Operation message rejected */
#define	CMD_PER_FAIL	17	/* Message Parity Error message rejected */
#define	CMD_BDR_FAIL	18	/* Bus Device Reset message rejected */
#define	CMD_ID_FAIL	19	/* Identify message rejected */
#define	CMD_UNX_BUS_FREE	20	/* Unexpected Bus Free Phase occurred */
#define	CMD_TAG_REJECT	21	/* Target rejected our tag message */
#define CMD_INCOMP_TIMEOUT	22

/*
 * Definitions for the pkt_state field
 */

#define	STATE_GOT_BUS		0x01	/* SCSI bus arbitration succeeded */
#define	STATE_GOT_TARGET	0x02	/* Target successfully selected */
#define	STATE_SENT_CMD		0x04	/* Command successfully sent */
#define	STATE_XFERRED_DATA	0x08	/* Data transfer took place */
#define	STATE_GOT_STATUS	0x10	/* SCSI status received */
#define	STATE_ARQ_DONE		0x20	/* auto rqsense took place */

/*
 * Definitions for the pkt_statistics field
 */

#define	STAT_DISCON	0x1	/* Command experienced a disconnect */
#define	STAT_SYNC	0x2	/* Command did a synchronous data transfer */
#define	STAT_PERR	0x4	/* Command experienced a SCSI parity error */
#define	STAT_BUS_RESET	0x8	/* Command experienced a bus reset */
#define	STAT_DEV_RESET	0x10	/* Command experienced a device reset */
#define	STAT_ABORTED	0x20	/* Command was aborted */
#define	STAT_TIMEOUT	0x40	/* Command experienced a timeout */

/*
 * Definitions for what pkt_transport returns
 */

#define	TRAN_ACCEPT	1
#define	TRAN_BUSY	0
#define	TRAN_BADPKT	-1

#define	MSG_COMMAND_COMPLETE	0x00
#define	MSG_SAVE_DATA_PTR	0x02
#define	MSG_RESTORE_PTRS	0x03
#define	MSG_DISCONNECT		0x04
#define	MSG_INITIATOR_ERROR	0x05
#define	MSG_ABORT		0x06
#define	MSG_REJECT		0x07
#define	MSG_NOP			0x08
#define	MSG_MSG_PARITY		0x09
#define	MSG_LINK_CMPLT		0x0A
#define	MSG_LINK_CMPLT_FLAG	0x0B
#define	MSG_DEVICE_RESET	0x0C
#define	MSG_ABORT_TAG		0x0D
#define	MSG_CLEAR_QUEUE		0x0E
#define	MSG_INITIATE_RECOVERY	0x0F
#define	MSG_RELEASE_RECOVERY	0x10
#define	MSG_TERMINATE_PROCESS	0x11
#define	MSG_CONTINUE_TASK	0x12
#define	MSG_TARGET_TRAN_DIS	0x13
#define	MSG_CLEAR_ACA		0x16

#define NTARGETS	8
#define NTARGETS_WIDE	16

