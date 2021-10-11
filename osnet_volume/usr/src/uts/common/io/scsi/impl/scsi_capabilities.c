/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi_capabilities.c	1.9	96/06/07 SMI"

/*
 *
 * Generic Capabilities Routines
 *
 */

#include <sys/scsi/scsi.h>

#define	A_TO_TRAN(ap)	(ap->a_hba_tran)

int
scsi_ifgetcap(struct scsi_address *ap, char *cap, int whom)
{
	return (*A_TO_TRAN(ap)->tran_getcap)(ap, cap, whom);
}

int
scsi_ifsetcap(struct scsi_address *ap, char *cap, int value, int whom)
{
	return (*A_TO_TRAN(ap)->tran_setcap)(ap, cap, value, whom);
}
