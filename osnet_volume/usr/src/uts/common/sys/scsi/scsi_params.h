/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_PARAMS_H
#define	_SYS_SCSI_SCSI_PARAMS_H

#pragma ident	"@(#)scsi_params.h	1.15	99/07/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	NUM_SENSE_KEYS		16	/* total number of Sense keys */

#define	NTAGS			256	/* number of tags per lun */

/*
 * General parallel SCSI parameters
 */
#define	NTARGETS		8	/* total # of targets per SCSI bus */
#define	NTARGETS_WIDE		16	/* #targets per wide SCSI bus */
#define	NLUNS_PER_TARGET	8	/* number of luns per target */

/*
 * the following defines are useful for settings max LUNs in nexus/target
 * drivers (the default of 8 is already defined above)
 */
#define	SCSI_16LUNS_PER_TARGET		16
#define	SCSI_32LUNS_PER_TARGET		32
#define	SCSI_1LUN_PER_TARGET		1


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_SCSI_PARAMS_H */
