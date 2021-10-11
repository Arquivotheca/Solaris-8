/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DDI_SUBR_H
#define	_SYS_DDI_SUBR_H

#pragma ident	"@(#)ddi_subrdefs.h	1.3	99/04/21 SMI"

/*
 * Sun DDI platform implementation subroutines definitions
 */

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/nexusintr.h>
#include <sys/nexusintr_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

uint32_t
i_ddi_get_intr_pri(dev_info_t *dip, uint_t inumber);

void
i_ddi_alloc_ispec(dev_info_t *dip, u_int inumber, ddi_intrspec_t *ispecp);

void
i_ddi_free_ispec(ddi_intrspec_t ispecp);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDI_SUBR_H */
