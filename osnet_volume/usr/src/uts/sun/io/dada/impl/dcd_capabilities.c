/*
 * Copyright (c) 1996, by Sun MicroSystem, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dcd_capabilities.c 1.4     98/02/25 SMI"

#if 0

/*
 * Generic capabilities Routines
 */

#include <sys/dada/dada.h>

#define	A_TO_TRAN(ap) (ap->a_hba_tran)


int
dcd_ifgetcap(struct dcd_address *ap, char *cap, int whom)
{

	return (*A_TO_TRAN(ap)->tran_getcap)(ap, cap, whom);
}

int
dcd_ifsetcap(struct dcd_address *ap, char *cap, int value, int whom)
{

	return (*A_TO_TRAN(ap)->tran_setcap)(ap, cap, value, whom);
}
#endif
