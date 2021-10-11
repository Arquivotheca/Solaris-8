/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_ADDRESS_H
#define	_SYS_SCSI_SCSI_ADDRESS_H

#pragma ident	"@(#)scsi_address.h	1.17	98/01/06 SMI"

#include <sys/scsi/scsi_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI address definition.
 *
 *	A target driver instance controls a target/lun instance.
 *	It sends the command to the device instance it controls.
 * 	In generic case	HBA driver maintains the target/lun information
 *	in the cloned transport structure pointed to by a_hba_tran field.
 *	This is the only way SCSI-3 devices will work.
 *
 *	a_target and a_lun fields are for compatibility with SCSI-2.
 *	They are not defined in SCSI-3 and target driver should use
 *	scsi_get_addr(9F) to get the target/lun information.
 *
 *	a_sublun was never used and was never part of DDI (scsi_address(9S)).
 */
struct scsi_address {
	struct scsi_hba_tran	*a_hba_tran;	/* Transport vectors */
	ushort_t		a_target;	/* Target identifier */
	uchar_t			a_lun;		/* Lun on that Target */
	uchar_t			a_sublun;	/* Sublun on that Lun */
						/* Not used */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_SCSI_ADDRESS_H */
