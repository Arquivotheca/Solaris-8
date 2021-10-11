/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)intr.h	1.3	97/07/21 SMI"
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
 * This file is derived from the interrupt.h file used for the Solaris NCR
 * driver.  The name has been shortened because it is too long for DOS.
 */


/* Interrupt vectors numbers that script may generate */
#define	NINT_OK		0xff00		/* device accepted the command */
#define	NINT_ILI_PHASE	0xff01		/* Illegal Phase */
#define	NINT_UNS_MSG	0xff02		/* Unsupported message */
#define	NINT_UNS_EXTMSG 0xff03		/* Unsupported extended message */
#define	NINT_MSGIN	0xff04		/* Message in expected */
#define	NINT_MSGREJ	0xff05		/* Message reject */
#define	NINT_RESEL	0xff06		/* C710 chip reselcted */
#define	NINT_SELECTED	0xff07		/* C710 chip selected */
#define	NINT_DISC	0xff09		/* Diconnect message received */
#define	NINT_RESEL_ERR	0xff0a		/* Reselect id error */
#define	NINT_SDP_MSG	0xff0b		/* Save Data Pointer message */
#define	NINT_RP_MSG	0xff0c		/* Restore Pointer message */
#define	NINT_SIGPROC	0xff0e		/* Signal Process */
#define	NINT_TOOMUCHDATA 0xff0f		/* Too much data to/from target */
#define	NINT_SDTR	0xff10		/* SDTR message received */
#define	NINT_SDTR_REJECT 0xff11		/* invalid SDTR exchange */
#define	NINT_REJECT	0xff12		/* failed to send msg reject */
#define	NINT_DEV_RESET	0xff13		/* bus device reset completed */
#define	NINT_WDTR	0xff14		/* WDTR message received */
