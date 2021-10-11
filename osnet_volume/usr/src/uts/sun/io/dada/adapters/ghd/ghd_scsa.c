/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)ghd_scsa.c 1.8	99/06/09 SMI"

#include <sys/dada/adapters/ghd/ghd.h>


/*
 * Local Function Prototypes
 */

static	struct scsi_pkt	*ghd_pktalloc(ccc_t *cccp, struct scsi_address *ap,
				int cmdlen, int statuslen, int tgtlen,
				int (*callback)(), caddr_t arg, int ccblen);

/*
 * Round up all allocations so that we can guarantee
 * long-long alignment.  This is the same alignment
 * provided by kmem_alloc().
 */
#define	ROUNDUP(x)	(((x) + 0x07) & ~0x07)

/*
 * Private wrapper for gcmd_t
 */

/*
 * round up the size so the HBA private area is on a 8 byte boundary
 */
#define	GW_PADDED_LENGTH	ROUNDUP(sizeof (gcmd_t))

typedef struct gcmd_padded_wrapper {
	union {
		gcmd_t	gw_gcmd;
		char	gw_pad[GW_PADDED_LENGTH];

	} gwrap;
} gwrap_t;




/*ARGSUSED*/
void
ghd_tran_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pktp)
{
	gcmd_t *gcmdp = PKTP2GCMDP(pktp);
	int	status;

	if (gcmdp->cmd_dma_handle) {
		status = ddi_dma_sync(gcmdp->cmd_dma_handle, 0, 0,
			(gcmdp->cmd_dma_flags & DDI_DMA_READ) ?
			DDI_DMA_SYNC_FORCPU : DDI_DMA_SYNC_FORDEV);
		if (status != DDI_SUCCESS) {
			cmn_err(CE_WARN, "ghd_tran_sync_pkt() fail\n");
		}
	}
}




struct scsi_pkt *
ghd_tran_init_pkt(ccc_t	*cccp,
		struct scsi_address *ap,
		struct scsi_pkt	*pktp,
		struct buf	*bp,
		int	 cmdlen,
		int	 statuslen,
		int	 tgtlen,
		int	 flags,
		int	(*callback)(),
		caddr_t	 arg,
		int	 ccblen,
		ddi_dma_lim_t	*sg_limitp)
{
	gcmd_t	*gcmdp;
	int	 new_pkt;

	ASSERT(callback == NULL_FUNC || callback == SLEEP_FUNC);

	/*
	 * Allocate a pkt
	 */
	if (pktp == NULL) {
		pktp = ghd_pktalloc(cccp, ap, cmdlen, statuslen, tgtlen,
					callback, arg, ccblen);
		if (pktp == NULL)
			return (NULL);
		new_pkt = TRUE;

	} else {
		new_pkt = FALSE;

	}

	gcmdp = PKTP2GCMDP(pktp);

	GDBG_PKT(("ghd_tran_init_pkt: gcmdp 0x%x dma_handle 0x%x\n",
			gcmdp, gcmdp->cmd_dma_handle));

	/*
	 * free stale DMA window if necessary.
	 */

	if (cmdlen && gcmdp->cmd_dma_handle) {
		/* release the old DMA resources */
		ghd_dmafree(gcmdp);
	}

	/*
	 * Set up dma info if there's any data and
	 * if the device supports DMA.
	 */

	GDBG_PKT(("ghd_tran_init_pkt: gcmdp 0x%x bp 0x%x limp 0x%x\n",
				gcmdp, bp, sg_limitp));

	if (bp && bp->b_bcount && sg_limitp) {
		int	dma_flags;

		/* check direction for data transfer */
		if (bp->b_flags & B_READ)
			dma_flags = DDI_DMA_READ;
		else
			dma_flags = DDI_DMA_WRITE;

		/* check dma option flags */
		if (flags & PKT_CONSISTENT)
			dma_flags |= DDI_DMA_CONSISTENT;
		if (flags & PKT_DMA_PARTIAL)
			dma_flags |= DDI_DMA_PARTIAL;

		/* map the buffer and/or create the scatter/gather list */
		if (ghd_dmaget(cccp->ccc_hba_dip, gcmdp, bp, dma_flags,
				callback, arg, sg_limitp,
				cccp->ccc_sg_func) == NULL) {
			if (new_pkt)
				ghd_pktfree(cccp, ap, pktp);
			return (NULL);
		}
		pktp->pkt_resid = gcmdp->cmd_resid;
	} else {
		pktp->pkt_resid = 0;
	}

	return (pktp);
}

static struct scsi_pkt *
ghd_pktalloc(ccc_t	*cccp,
	struct scsi_address *ap,
	int	cmdlen,
	int	statuslen,
	int	tgtlen,
	int	(*callback)(),
	caddr_t	arg,
	int	ccblen)
{
	gtgt_t		*gtgtp =  ADDR2GTGTP(ap);
	struct scsi_pkt	*pktp;
	gcmd_t		*gcmdp;
	gwrap_t		*gwp;
	int		 gwrap_len;

	gwrap_len = sizeof (gwrap_t) + ROUNDUP(ccblen);

	/* allocate everything from kmem pool */
	pktp = scsi_hba_pkt_alloc(cccp->ccc_hba_dip, ap, cmdlen, statuslen,
				tgtlen, gwrap_len, callback, arg);
	if (pktp == NULL) {
		return (NULL);
	}

	/* get the ptr to the HBA specific buffer */
	gwp = (gwrap_t *)(pktp->pkt_ha_private);

	/* get the ptr to the GHD specific buffer */
	gcmdp = &gwp->gwrap.gw_gcmd;

	ASSERT((caddr_t)gwp == (caddr_t)gcmdp);

	/*
	 * save the ptr to HBA private area and initialize the rest
	 * of the gcmd_t members
	 */
	GHD_GCMD_INIT(gcmdp, (void *)(gwp + 1), gtgtp);

	/*
	 * save the the scsi_pkt ptr in gcmd_t.
	 */
	gcmdp->cmd_pktp = pktp;

	/*
	 * callback to the HBA driver so it can initalize its
	 * buffer and return the ptr to my cmd_t structure which is
	 * probably embedded in its buffer.
	 */

	if (!(*cccp->ccc_ccballoc)(gtgtp, gcmdp, cmdlen, statuslen, tgtlen,
					ccblen)) {
		scsi_hba_pkt_free(ap, pktp);
		return (NULL);
	}

	return (pktp);
}



/*
 * packet free
 */
/*ARGSUSED*/
void
ghd_pktfree(ccc_t		*cccp,
	struct scsi_address	*ap,
	struct scsi_pkt		*pktp)
{
	GDBG_PKT(("ghd_pktfree: cccp 0x%x ap 0x%x pktp 0x%x\n",
			cccp, ap, pktp));

	/* free any extra resources allocated by the HBA */
	(*cccp->ccc_ccbfree)(PKTP2GCMDP(pktp));

	/* free the scsi_pkt and the GHD and HBA private areas */
	scsi_hba_pkt_free(ap, pktp);
}
