/*
 * Copyright (c) 1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)ghd_dma.c	1.9	99/06/21 SMI"

#include "ghd.h"


/*
 * free dma handle and controller handle allocated in ghd_dmaget()
 */

void
ghd_dmafree(gcmd_t *gcmdp)
{
	GDBG_DMA(("ghd_dmafree: gcmdp 0x%x\n", gcmdp));

	if (gcmdp->cmd_dma_handle != NULL) {
		if (ddi_dma_free(gcmdp->cmd_dma_handle) != DDI_SUCCESS)
			cmn_err(CE_WARN, "ghd dmafree failed");
		GDBG_DMA(("ghd_dmafree: ddi_dma_free 0x%x\n", gcmdp));
		gcmdp->cmd_dma_handle = NULL;
		gcmdp->cmd_dmawin = NULL;
		gcmdp->cmd_totxfer = 0;
	}
}


int
ghd_dmaget(ccc_t		*cccp,
		gcmd_t		*gcmdp,
		struct buf	*bp,
		int		 dma_flags,
		int		(*callback)(),
		caddr_t		 arg,
		ddi_dma_lim_t	*sg_limitp)
{
	ddi_dma_cookie_t cookie;
	int	 sg_size = sg_limitp->dlim_sgllen;
	ulong	 bcount = bp->b_bcount;
	ulong	 xferred = gcmdp->cmd_totxfer;
	int	 single_seg = TRUE;
	int	 num_segs = 0;
	int	 status;
	off_t	 off;
	off_t	 len;

	GDBG_DMA(("ghd_dmaget: start: gcmdp 0x%x lim 0x%x h 0x%x w 0x%x\n",
		gcmdp, sg_limitp, gcmdp->cmd_dma_handle, gcmdp->cmd_dmawin));

	if (gcmdp->cmd_dma_handle == NULL)
		goto new_handle;

	if (gcmdp->cmd_dmawin == NULL)
		goto nextwin;

nextseg:
	do {
		status = ddi_dma_nextseg(gcmdp->cmd_dmawin, gcmdp->cmd_dmaseg,
			    &gcmdp->cmd_dmaseg);
		switch (status) {
		case DDI_SUCCESS:
			break;

		case DDI_DMA_DONE:
			if (num_segs == 0) {
				/* start the next window */
				goto nextwin;
			}
			gcmdp->cmd_totxfer = xferred;
			return (TRUE);

		default:
			return (FALSE);
		}

		if (ddi_dma_segtocookie(gcmdp->cmd_dmaseg, &off, &len,
			&cookie) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "ghd dmaget: "
			    "dma segtocookie failed");
			return (FALSE);
		}

		if (len < bcount) {
			/*
			 * Can't do the transfer in a single segment,
			 * so disable single-segment Scatter/Gather option.
			 */
			single_seg = FALSE;
		}

		/*
		 * call the controller specific S/G function
		 */
		(*cccp->ccc_sg_func)(gcmdp, &cookie, single_seg, num_segs);

		/* take care of the loop-bookkeeping */
		single_seg = FALSE;
		xferred += cookie.dmac_size;
		num_segs++;
	} while (xferred < bcount && num_segs < sg_size);

	gcmdp->cmd_totxfer = xferred;
	return (TRUE);


	/*
	 * First time, need to establish the handle.
	 */

new_handle:
	gcmdp->cmd_dmawin = NULL;


	status = ddi_dma_buf_setup(cccp->ccc_hba_dip, bp, dma_flags,
		    callback, arg, sg_limitp, &gcmdp->cmd_dma_handle);

	GDBG_DMA(("ghd_dmaget: setup: gcmdp 0x%x status %d\n", gcmdp, status));

	switch (status) {
	case DDI_DMA_MAPOK:
	case DDI_DMA_PARTIAL_MAP:
		/* enable first call to ddi_dma_nextwin */
		gcmdp->cmd_dma_flags = dma_flags;
		break;

	case DDI_DMA_NORESOURCES:
		bp->b_error = 0;
		return (FALSE);

	case DDI_DMA_TOOBIG:
		bioerror(bp, EINVAL);
		return (FALSE);

	case DDI_DMA_NOMAPPING:
	default:
		bioerror(bp, EFAULT);
		return (FALSE);
	}


	/*
	 * get the next window
	 */

nextwin:
	gcmdp->cmd_dmaseg = NULL;

	status = ddi_dma_nextwin(gcmdp->cmd_dma_handle, gcmdp->cmd_dmawin,
		    &gcmdp->cmd_dmawin);

	GDBG_DMA(("ghd_dmaget: nextwin: gcmdp 0x%x status %d\n",
		gcmdp, status));

	switch (status) {
	case DDI_SUCCESS:
		break;

	case DDI_DMA_DONE:
		return (FALSE);

	default:
		return (FALSE);
	}
	goto nextseg;

}


void
ghd_dmafree_attr(gcmd_t *gcmdp)
{
	GDBG_DMA(("ghd_dma_attr_free: gcmdp 0x%x\n", gcmdp));

	if (gcmdp->cmd_dma_handle != NULL) {
		if (ddi_dma_unbind_handle(gcmdp->cmd_dma_handle) !=
		    DDI_SUCCESS)
			cmn_err(CE_WARN, "ghd dma free attr: "
			    "unbind handle failed");
		ddi_dma_free_handle(&gcmdp->cmd_dma_handle);
		GDBG_DMA(("ghd_dma_attr_free: ddi_dma_free 0x%x\n", gcmdp));
		gcmdp->cmd_dma_handle = NULL;
		gcmdp->cmd_ccount = 0;
		gcmdp->cmd_totxfer = 0;
	}
}


int
ghd_dma_buf_bind_attr(ccc_t		*cccp,
			gcmd_t		*gcmdp,
			struct buf	*bp,
			int		 dma_flags,
			int		(*callback)(),
			caddr_t		 arg,
			ddi_dma_attr_t	*sg_attrp)
{
	int	 status;

	GDBG_DMA(("ghd_dma_attr_get: start: gcmdp 0x%x sg_attrp 0x%x\n",
		gcmdp, sg_attrp));


	/*
	 * First time, need to establish the handle.
	 */

	ASSERT(gcmdp->cmd_dma_handle == NULL);

	status = ddi_dma_alloc_handle(cccp->ccc_hba_dip, sg_attrp, callback,
		arg, &gcmdp->cmd_dma_handle);

	if (status != DDI_SUCCESS) {
		bp->b_error = 0;
		return (FALSE);
	}

	status = ddi_dma_buf_bind_handle(gcmdp->cmd_dma_handle, bp, dma_flags,
		    callback, arg, &gcmdp->cmd_first_cookie,
		    &gcmdp->cmd_ccount);

	GDBG_DMA(("ghd_dma_attr_get: setup: gcmdp 0x%x status %d h 0x%x "
		"c 0x%x\n", gcmdp, status, gcmdp->cmd_dma_handle,
			gcmdp->cmd_ccount));

	switch (status) {
	case DDI_DMA_MAPPED:
		/* enable first (and only) call to ddi_dma_getwin */
		gcmdp->cmd_wcount = 1;
		break;

	case DDI_DMA_PARTIAL_MAP:
		/* enable first call to ddi_dma_getwin */
		if (ddi_dma_numwin(gcmdp->cmd_dma_handle, &gcmdp->cmd_wcount) !=
								DDI_SUCCESS) {
			bp->b_error = 0;
			ddi_dma_free_handle(&gcmdp->cmd_dma_handle);
			gcmdp->cmd_dma_handle = NULL;
			return (FALSE);
		}
		break;

	case DDI_DMA_NORESOURCES:
		bp->b_error = 0;
		ddi_dma_free_handle(&gcmdp->cmd_dma_handle);
		gcmdp->cmd_dma_handle = NULL;
		return (FALSE);

	case DDI_DMA_TOOBIG:
		bioerror(bp, EINVAL);
		ddi_dma_free_handle(&gcmdp->cmd_dma_handle);
		gcmdp->cmd_dma_handle = NULL;
		return (FALSE);

	case DDI_DMA_NOMAPPING:
	case DDI_DMA_INUSE:
	default:
		bioerror(bp, EFAULT);
		ddi_dma_free_handle(&gcmdp->cmd_dma_handle);
		gcmdp->cmd_dma_handle = NULL;
		return (FALSE);
	}

	/* initialize the loop controls for ghd_dmaget_next_attr() */
	gcmdp->cmd_windex = 0;
	gcmdp->cmd_cindex = 0;
	gcmdp->cmd_totxfer = 0;
	gcmdp->cmd_dma_flags = dma_flags;
	gcmdp->use_first = 1;
	return (TRUE);
}


uint_t
ghd_dmaget_next_attr(ccc_t	*cccp,
			gcmd_t	*gcmdp,
			long	 max_transfer_cnt,
			int	 sg_size,
			ddi_dma_cookie_t cookie)
{
	ulong	toxfer = 0;
	int	num_segs = 0;
	int	single_seg;

	GDBG_DMA(("ghd_dma_attr_get: start: gcmdp 0x%x h 0x%x c 0x%x\n",
			gcmdp, gcmdp->cmd_dma_handle, gcmdp->cmd_ccount));

	/*
	 * Disable single-segment Scatter/Gather option
	 * if can't do this transfer in a single segment,
	 */
	if (gcmdp->cmd_cindex + 1 < gcmdp->cmd_ccount) {
		single_seg = FALSE;
	} else {
		single_seg = TRUE;
	}


	for (;;) {
		/*
		 * call the controller specific S/G function
		 */
		(*cccp->ccc_sg_func)(gcmdp, &cookie, single_seg, num_segs);

		/* take care of the loop-bookkeeping */
		toxfer += cookie.dmac_size;
		num_segs++;
		gcmdp->cmd_cindex++;

		/*
		 * if this was the last cookie in the current window
		 * set the loop controls start the next window and
		 * exit so the HBA can do this partial transfer
		 */
		if (gcmdp->cmd_cindex >= gcmdp->cmd_ccount) {
			gcmdp->cmd_windex++;
			gcmdp->cmd_cindex = 0;
			break;
		}
		ASSERT(single_seg == FALSE);

		if (toxfer >= max_transfer_cnt)
			break;

		if (num_segs >= sg_size)
			break;

		ddi_dma_nextcookie(gcmdp->cmd_dma_handle, &cookie);
	}

	gcmdp->cmd_totxfer += toxfer;

	return (toxfer);
}



int
ghd_dmaget_attr(ccc_t		*cccp,
		gcmd_t		*gcmdp,
		long		count,
		int		sg_size,
		uint_t		*xfer)
{
	int	status;
	ddi_dma_cookie_t cookie;

	*xfer = 0;


	if (gcmdp->use_first == 1) {
		cookie = gcmdp->cmd_first_cookie;
		gcmdp->use_first = 0;
	} else if (gcmdp->cmd_windex >= gcmdp->cmd_wcount) {
		/*
		 * reached the end of buffer. This should not happen.
		 */
		ASSERT(gcmdp->cmd_windex < gcmdp->cmd_wcount);
		return (FALSE);

	} else if (gcmdp->cmd_cindex == 0) {
		off_t	offset;
		size_t	length;

		/*
		 * start the next window, and get its first cookie
		 */
		status = ddi_dma_getwin(gcmdp->cmd_dma_handle,
				gcmdp->cmd_windex, &offset, &length,
				&cookie, &gcmdp->cmd_ccount);
		if (status != DDI_SUCCESS)
			return (FALSE);

	} else {
		/*
		 * get the next cookie in the current window
		 */
		ddi_dma_nextcookie(gcmdp->cmd_dma_handle, &cookie);
	}

	/*
	 * start the Scatter/Gather loop passing in the first
	 * cookie obtained above
	 */
	*xfer = ghd_dmaget_next_attr(cccp, gcmdp, count, sg_size, cookie);
	return (TRUE);
}
