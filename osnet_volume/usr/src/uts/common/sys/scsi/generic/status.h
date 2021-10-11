/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_GENERIC_STATUS_H
#define	_SYS_SCSI_GENERIC_STATUS_H

#pragma ident	"@(#)status.h	1.17	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI status completion block
 *
 * The SCSI standard specifies one byte of status.
 */

/*
 * The definition of of the Status block as a bitfield
 */

struct scsi_status {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	sts_vu0		: 1,	/* vendor unique 		*/
		sts_chk		: 1,	/* check condition 		*/
		sts_cm		: 1,	/* condition met 		*/
		sts_busy	: 1,	/* device busy or reserved 	*/
		sts_is		: 1,	/* intermediate status sent 	*/
		sts_vu6		: 1,	/* vendor unique 		*/
		sts_vu7		: 1,	/* vendor unique 		*/
		sts_resvd	: 1;	/* reserved 			*/
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	sts_resvd	: 1,	/* reserved */
		sts_vu7		: 1,	/* vendor unique */
		sts_vu6		: 1,	/* vendor unique */
		sts_is		: 1,	/* intermediate status sent */
		sts_busy	: 1,	/* device busy or reserved */
		sts_cm		: 1,	/* condition met */
		sts_chk		: 1,	/* check condition */
		sts_vu0		: 1;	/* vendor unique */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
};
#define	sts_scsi2	sts_vu6		/* SCSI modifier bit */

/*
 * if auto request sense has been enabled, then use this structure
 */
struct scsi_arq_status {
	struct scsi_status	sts_status;
	struct scsi_status	sts_rqpkt_status;
	uchar_t			sts_rqpkt_reason;	/* reason completion */
	uchar_t			sts_rqpkt_resid;	/* residue */
	uint_t			sts_rqpkt_state;	/* state of command */
	uint_t			sts_rqpkt_statistics;	/* statistics */
	struct scsi_extended_sense sts_sensedata;
};

#define	SECMDS_STATUS_SIZE  (sizeof (struct scsi_arq_status))

/*
 * Bit Mask definitions, for use accessing the status as a byte.
 */

#define	STATUS_MASK			0x3E
#define	STATUS_GOOD			0x00
#define	STATUS_CHECK			0x02
#define	STATUS_MET			0x04
#define	STATUS_BUSY			0x08
#define	STATUS_INTERMEDIATE		0x10
#define	STATUS_SCSI2			0x20
#define	STATUS_INTERMEDIATE_MET		0x14
#define	STATUS_RESERVATION_CONFLICT	0x18
#define	STATUS_TERMINATED		0x22
#define	STATUS_QFULL			0x28
#define	STATUS_ACA_ACTIVE		0x30

#ifdef	__cplusplus
}
#endif

/*
 * Some deviations from the one byte of Status are known. Each
 * implementation will define them specifically
 */

#include <sys/scsi/impl/status.h>

#endif	/* _SYS_SCSI_GENERIC_STATUS_H */
