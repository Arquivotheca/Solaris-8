/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _GHD_DMA_H
#define	_GHD_DMA_H

#pragma ident	"@(#)ghd_dma.h	1.5	97/07/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "ghd.h"

void	 ghd_dmafree(gcmd_t *gcmdp);

int	 ghd_dmaget(dev_info_t *dip, gcmd_t *gcmdp, struct buf *bp,
		int flags, int (*callback)(), caddr_t arg,
		ddi_dma_lim_t *sg_limitp, void (*sg_func)());

#ifdef	__cplusplus
}
#endif

#endif /* _GHD_DMA_H */
