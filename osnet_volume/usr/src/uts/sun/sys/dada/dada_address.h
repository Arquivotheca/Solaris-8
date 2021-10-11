/*
 * Copyright (c) 1996, by Sun Microsystem, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DADA_DADA_ADDRESS_H
#define	_SYS_DADA_DADA_ADDRESS_H

#pragma ident	"@(#)dada_address.h	1.5	98/02/02 SMI"

#include <sys/dada/dada_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * DADA address definition.
 *
 * 	A target driver instance controls a target/lun instance.
 *	It sends the command to the device instance it controls.
 *	In generic case HBA drive maintains the target/lun information
 * 	in the cloned transport structure pointed to by a_hba_tran field.
 *
 */


struct	dcd_address {
	uchar_t			a_lun;		/* Not used. 		*/
	ushort_t		a_target;	/* The target identifier */
	struct dcd_hba_tran	*a_hba_tran; 	/* Transport vectors */
};
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DADA_DADA_ADDRESS_H */
