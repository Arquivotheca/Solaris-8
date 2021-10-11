/*
 * Copyright (c) 1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _GHD_DMA_H
#define	_GHD_DMA_H

#pragma ident	"@(#)ghd_dma.h	1.6	99/06/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "ghd.h"

void	ghd_dmafree(gcmd_t *gcmdp);

int	ghd_dmaget(ccc_t *cccp, gcmd_t *gcmdp, struct buf *bp, int flags,
		    int (*callback)(), caddr_t arg, ddi_dma_lim_t *sg_limitp);

int	ghd_dmaget_attr(ccc_t *cccp, gcmd_t *gcmdp, long count, int sg_size,
			uint_t *xfer);

int	ghd_dma_buf_bind_attr(ccc_t *ccp, gcmd_t *gcmdp, struct buf *bp,
		int dma_flags, int (*callback)(), caddr_t arg,
		ddi_dma_attr_t *sg_attrp);

void	ghd_dmafree_attr(gcmd_t *gcmdp);

uint_t	ghd_dmaget_next_attr(ccc_t *cccp, gcmd_t *gcmdp, long max_transfer_cnt,
		int sg_size, ddi_dma_cookie_t cookie);

#ifdef	__cplusplus
}
#endif

#endif /* _GHD_DMA_H */
