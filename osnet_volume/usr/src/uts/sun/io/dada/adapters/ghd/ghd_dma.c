/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)ghd_dma.c 1.5	97/11/13 SMI"

#include <sys/dada/adapters/ghd/ghd.h>


/*
 * free dma handle and controller handle allocated in ghd_dmaget()
 */

/*ARGSUSED*/
void
ghd_dmafree(gcmd_t *gcmdp)
{
	GDBG_DMA(("ghd_dmafree: gcmdp 0x%x\n", gcmdp));

	if (gcmdp->cmd_dma_handle != NULL) {
		(void) ddi_dma_free(gcmdp->cmd_dma_handle);
		GDBG_DMA(("ghd_dmafree: ddi_dma_free 0x%x\n", gcmdp));
		gcmdp->cmd_dma_handle = NULL;
		gcmdp->cmd_dmawin = NULL;
		gcmdp->cmd_totxfer = 0;
	}
}


int
ghd_dmaget(dev_info_t	*dip,
	gcmd_t		*gcmdp,
	struct buf	*bp,
	int		 dma_flags,
	int		(*callback)(),
	caddr_t		 arg,
	ddi_dma_lim_t	*sg_limitp,
	void		(*sg_func)())
{
#if defined(sparc) || defined(__sparc)
	int	sg_size = 1;
#else
	int	 sg_size = sg_limitp->dlim_sgllen;
#endif
	ulong	 bcount = bp->b_bcount;
	ulong	 xferred = gcmdp->cmd_totxfer;
	int	 status;
	off_t	 off;
	off_t	 len;
	int	 num_segs = 0;
	ddi_dma_cookie_t cookie;
	int	 single_seg = TRUE;

	GDBG_DMA(("ghd_dmaget: start: gcmdp 0x%x lim 0x%x h 0x%x w 0x%x\n",
			gcmdp, sg_limitp, gcmdp->cmd_dma_handle,
			gcmdp->cmd_dmawin));

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
			gcmdp->cmd_resid = bcount - gcmdp->cmd_totxfer;
			return (TRUE);

		default:
			return (FALSE);
		}

		(void) ddi_dma_segtocookie(gcmdp->cmd_dmaseg, &off, &len,
					&cookie);

		if (len < bcount) {
			/*
			 * Can't do the transfer in a single segment,
			 * so disable single-segment Scatter/Gather option.
			 */
			single_seg = FALSE;
		}

		/* call the controller specific S/G function */
		(*sg_func)(gcmdp, &cookie, single_seg, num_segs);

		/* take care of the loop-bookkeeping */
		single_seg = FALSE;
		xferred += cookie.dmac_size;
		num_segs++;
	} while (xferred < bcount && num_segs < sg_size);

	gcmdp->cmd_totxfer = xferred;
	gcmdp->cmd_resid = bcount - gcmdp->cmd_totxfer;
	return (TRUE);


	/*
	 * First time, need to establish the handle.
	 */

new_handle:
	gcmdp->cmd_dmawin = NULL;


	status = ddi_dma_buf_setup(dip, bp, dma_flags, callback, arg, sg_limitp,
					&gcmdp->cmd_dma_handle);

	GDBG_DMA(("ghd_dmaget: setup: gcmdp 0x%x status %d\n", gcmdp, status));

	switch (status) {
	case DDI_DMA_MAPOK:
	case DDI_DMA_PARTIAL_MAP:
		/* enable first call to ddi_dma_nextwin */
		gcmdp->cmd_resid  = 0;
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
