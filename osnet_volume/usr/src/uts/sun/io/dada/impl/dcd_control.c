/*
 * Copyright 9c) 1996, by Sun MicroSystems, Inc.
 * All rights reserved.
*/

#pragma	ident	"@(#)dcd_control.c 1.3     98/02/25 SMI"

/*
 * Generic Abort, Reset and Misc Routines
 */

#include <sys/dada/dada.h>

#define	A_TO_TRAN(ap) (ap->a_hba_tran)


int
dcd_abort(struct dcd_address *ap, struct dcd_pkt *pkt)
{

	return (*A_TO_TRAN(ap)->tran_abort)(ap, pkt);
}

int
dcd_reset(struct dcd_address *ap, int level)
{
	return (*A_TO_TRAN(ap)->tran_reset)(ap, level);
}
