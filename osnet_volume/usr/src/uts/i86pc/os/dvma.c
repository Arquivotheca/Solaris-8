/*
 * Copyright (c) 1990-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dvma.c 1.5	98/01/09 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

/*ARGSUSED*/
unsigned long
dvma_pagesize(dev_info_t *dip)
{
	return (0);
}

/*ARGSUSED*/
int
dvma_reserve(dev_info_t *dip,  ddi_dma_lim_t *limp, u_int pages,
    ddi_dma_handle_t *handlep)
{
	return (DDI_DMA_NORESOURCES);
}

/*ARGSUSED*/
void
dvma_release(ddi_dma_handle_t h)
{
}

/*ARGSUSED*/
void
dvma_kaddr_load(ddi_dma_handle_t h, caddr_t a, u_int len, u_int index,
	ddi_dma_cookie_t *cp)
{
}

/*ARGSUSED*/
void
dvma_unload(ddi_dma_handle_t h, u_int objindex, u_int type)
{
}

/*ARGSUSED*/
void
dvma_sync(ddi_dma_handle_t h, u_int objindex, u_int type)
{
}
