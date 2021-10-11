/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)script.h	1.3	97/07/21 SMI"
 */

/*
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

/*
 * This file was derived from the script.h file in the Solaris
 * driver.
 */


/* SCRATCHA0 flag bits */
#define	NBIT_IS710	0x01

/* Function Codes for the SCRIPTS entry points */

#define	NSS_STARTUP		0	/* select target and start request */
#define	NSS_CONTINUE		1	/* continue after phase mismatch */
#define	NSS_WAIT4RESELECT	2	/* wait for reselect */
#define	NSS_CLEAR_ACK		3	/* continue after both SDTR msgs okay */
#define	NSS_SYNC_OUT		4	/* send out SDTR response to target */
#define	NSS_ERR_MSG		5	/* send Message Reject message */
#define	NSS_BUS_DEV_RESET	6	/* do Bus Device Reset */
#define	NSS_ABORT		7	/* abort an invalid reconnection */
#define	NSS_FUNCS		8	/* number of defined SCRIPT functions */


/*
 * This array holds the physical addresses of the SCRIPTS function
 * entry points.
 */
extern	paddr_t	ncr_scripts[NSS_FUNCS];
extern	paddr_t ncr_do_list;
extern	paddr_t ncr_di_list;

/* SCRIPTS command opcodes for Block Move instructions */

#define	NSOP_MOVE_MASK		0xF8	/* just the opcode bits */
#define	NSOP_MOVE		0x18	/* MOVE FROM ... */
#define	NSOP_CHMOV		0x08	/* CHMOV FROM ... */

#define	NSOP_PHASE		0x0f	/* the expected phase bits */
#define	NSOP_DATAOUT		0x00	/* data out */
#define	NSOP_DATAIN		0x01	/* data in */
#define	NSOP_COMMAND		0x02	/* command */
#define	NSOP_STATUS		0x03	/* status */
#define	NSOP_MSGOUT		0x06	/* message out */
#define	NSOP_MSGIN		0x07	/* message out */
