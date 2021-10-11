/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_NEXUSINTR_IMPL_H
#define	_SYS_NEXUSINTR_IMPL_H

#pragma ident	"@(#)nexusintr_impl.h	1.4	99/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/dditypes.h>

/* This is a sun4u specific interrupt specification structure (ispec) */
typedef struct ddi_ispec {
	uint32_t *is_intr;	/* Interrupt spec at a given bus node */
	int32_t is_intr_sz;	/* Size in bytes of interrupt spec */
	uint32_t is_pil;	/* Hint of the PIL for this interrupt spec */
} ddi_ispec_t;

/* This is a soft interrupt specification */
typedef struct ddi_softispec {
	dev_info_t *sis_rdip;	 /* Interrupt requestors dip */
	uint32_t sis_softint_id; /* Soft interrupt id */
	uint32_t sis_pil;	 /* Hint of the PIL for this interrupt spec */
} ddi_softispec_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NEXUSINTR_IMPL_H */
